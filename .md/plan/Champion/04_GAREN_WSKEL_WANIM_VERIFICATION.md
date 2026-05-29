# Phase B-9 — 가렌 풀 사이클 + `.wskel`/`.wanim` Stage 3 구현 (계단식 검증, **v4 — 진짜 최종**)

**작성일**: 2026-04-27
**갱신 이력**:
- v2: Codex 1차 검증 (5건)
- v3: Codex 2차 — POD 크기, tick 단위, CWintersFile 의존 제거, 변환 순서 `skel → mesh --skel → anim`
- **v4**: Codex 3차 — BinaryWriter API 정정 (P0), BuildSkeletonFromStage3 ws+wm 합성 (P0), 5챔프 경로 마이그레이션 (P0), Create() 팩토리, include 보강, uint8 한계 명시, basename 기준

**전제**: Phase B-8 가렌 풀 동작 완료. 이번 사이클 = `.wmesh + .wskel + .wanim` 가렌 1체 검증 → 6 챔프 일괄.
**참조 spec**:
- `.md/plan/WintersFormat/00_WINTERS_FORMAT_INDEX.md`
- `.md/plan/WintersFormat/04_STAGE3_WANIM_WSKEL.md` (POD 크기/tick 단위 정정 필요 — 본 계획서 권위)
- `.md/plan/Champion/03_GAREN_PHASE_B8_PIPELINE.md`

---

## 0. v4 변경 요약 — Codex 3차 검증 반영 (P0 3건 + P1 4건)

| # | Codex 3차 비판 | v4 반영 | 우선 |
|---|---------------|---------|------|
| **1** | `CBinaryWriter::Size()/Data()` 미존재 + `SaveToFile()` 가 헤더 자동 | Writer 사용 = **payload만 Write + `SaveToFile(WF_NONE)` 호출**. `WintersFileHeader` 직접 Write 금지 | **P0** |
| **2** | `BuildSkeletonFromWSkel(ws)` 부족 — `CSkeleton::AddBone` 가 `(name, parent, matOffset, matRestLocal)` **둘 다** 요구 | `BuildSkeletonFromStage3(ws, wm)` 로 **wskel rest + wmesh offset 합성**. v3 의 "wmesh bone index = wskel DFS 순서" 가 이 매칭의 전제 조건 | **P0** |
| **3** | Scene_InGame 5챔프 (Irelia/Yasuo/Sylas/Viego/Kalista) `LOL_Resource` 절대 경로 | M7 직전 **M6.5 신설** — 5챔프 Init 경로 마이그레이션. 미수행 시 fast-path 진입 0 | **P0** |
| 4 | `CSkeleton/CAnimation` private ctor — `make_unique` 불가 | `CSkeleton::Create()` / `CAnimation::Create(name, dDur, dTicksPerSec)` 팩토리만 사용 | P1 |
| 5 | Hash.h `<cstddef>`, WMeshWriter.h `<unordered_map>` 누락 | include 명시 | P1 |
| 6 | `VertexSkinned::indices = 4 × uint8 packed` — bone>256 시 fail | WMeshWriter `--skel` 모드에서 `bone_count >= 256` 즉시 거부 + 로그 | P1 |
| 7 | `.bat` 출력이 `%CHAMP%.wmesh` 면 `irelia_fixed.fbx → irelia_fixed.wmesh` 매칭 실패 | `%~n2` (FBX basename) 기준 출력 | P1 |
| + | `CAnimation::AddChannel(BoneChannel)` 시그니처 — Loader 가 `BoneChannel` 직접 채워 push | v4 LoadAsAnimation 코드 정정 | 정합 |

---

## 1. 현황 — 구현 vs 미구현 (v4 정확화)

| 영역 | 상태 | 위치 |
|---|---|---|
| `WintersFileHeader` 16B + magic `WINT` | ✅ 구현 | `Engine/Public/AssetFormat/Common/WintersFileHeader.h` |
| `CBinaryReader/Writer` (★ 헤더 자동) | ✅ 구현 | `BinaryWriter.h:21 GetPayloadSize`, `.cpp:14-29 SaveToFile 자동 헤더` |
| `CWintersFile/SHA256/LZ4` | ❌ **미구현** | 본 사이클 X |
| `WMeshFormat.h` POD | ✅ 구현 | — |
| `CWMeshWriter::WriteFromAssimp` | ✅ 부분 — `aiMesh::mBones` 순서, parent_index=-1 의도 | M2 에서 `--skel` 보정 |
| `CWMeshLoader` | ✅ 구현 | — |
| `CModel::LoadModel` `.wmesh` hybrid (`bone_count==0`) | ✅ 구현 | `Model.cpp:144-205` |
| `CModel::LoadSkeleton` (Assimp DFS) | ✅ 구현 | `Model.cpp:497` — 권위 |
| `CAnimator::Update` **tick 단위** | ✅ 구현 | `Animator.cpp:27` |
| `CSkeleton::Create()` + `AddBone(name, parent, **matOffset, matRestLocal**)` | ✅ 구현 | `Skeleton.h:14-18` |
| `CAnimation::Create(name, dDur, dTicksPerSec)` + `AddChannel(BoneChannel)` | ✅ 구현 | `Animation.h:35-38` |
| `WSkelFormat.h / WAnimFormat.h` | ❌ **미구현** | M0/M1/M3 신규 |

---

## 1.5 "wbone" — 별도 확장자 아님

| 구역 | 파일 | 책임 | 매칭 |
|---|---|---|---|
| mesh-side bone | `.wmesh::BoneEntry` | `offset_matrix` (Inverse Bind Pose, 정점↔본) | `BoneInfo::matOffset` |
| skel-side bone | `.wskel::BoneNode` | `rest_transform` (Rest Pose 로컬), GlobalInverseRoot, parent_index | `BoneInfo::matRestLocal` + parent |

**v4 핵심**: M2 에서 `.wmesh::BoneEntry` 의 **인덱스 순서를 `.wskel::BoneNode` 와 동일하게** 강제. `BuildSkeletonFromStage3(ws, wm)` 가 `i` 인덱스로 양쪽을 zip:

```cpp
for (uint32_t i = 0; i < ws.header.bone_count; ++i) {
    skel->AddBone(
        ws.bones[i].name,                  // 또는 wm.bones[i].name (동일)
        ws.bones[i].parent_index,
        wm.bones[i].offset_matrix,         // matOffset (메시 측)
        ws.bones[i].rest_transform);       // matRestLocal (스켈 측)
}
```

---

## 2. 6 레이어 매핑

| Layer | 책임 | v4 변경 |
|---|---|---|
| 1. 자원 (RAII) | 기존 `CSkeleton/CAnimation/CAnimator` 재사용 — Loader 가 **`Create()` 팩토리** 호출 | 신규 0 |
| 2. 상태 (POD) | `WSkelFormat.h` + `WAnimFormat.h` (POD 크기 정정) | 신규 2 |
| 3. 정의 (CLI) | `convert_all_assets.bat` — `%~n2` basename 기준 출력 | 수정 1 |
| 4. 로직 (Loader) | `CWSkelLoader/Writer` + `CWAnimLoader/Writer`. WMeshWriter `--skel` 옵션 | 신규 8 + WMeshWriter 수정 |
| 5. 변환 (Tool) | AssetConverter `skel`/`anim`/`info` 서브커맨드 | main.cpp 수정 |
| 6. 통합 (CModel + Scene) | LoadModel + `BuildSkeletonFromStage3(ws, wm)` + **5챔프 경로 마이그레이션** | Model.cpp + Scene_InGame.cpp |

---

## 3. 마일스톤 (M0~M7, M6.5 신설)

```
M0 (포맷 동기화)         — Hash.h 공용화 + POD 크기 정정 + vcxproj
M1 (.wskel)              — DFS 순서 (Model::LoadSkeleton 권위 — pre-order)
M2 (.wmesh --skel)       — wskel index 권위 모드 + bone>=256 거부
M3 (.wanim)              — tick 단위 + skel_hash trailer + Create() 팩토리
M4 (Converter CLI)       — skel/anim/info 추가 + basename 출력
M5 (CModel 통합)         — BuildSkeletonFromStage3(ws, wm) + LoadAsAnimation
M6 (가렌 visual diff)    — 화면 동일 + 애니 이벤트 동일
M6.5 (5챔프 경로 이관)   — ★ v4 신설 — Scene_InGame Irelia/Yasuo/Sylas/Viego/Kalista Init
M7 (6 챔프 일괄 변환)    — bat basename + F5 검증
```

---

## 4. M0 — 포맷 동기화 (Layer 2)

### 4.1 신규 — `Engine/Public/AssetFormat/Common/Hash.h`

```cpp
#pragma once
#include "WintersAPI.h"
#include <cstddef>     // ★ v4 — size_t
#include <cstdint>

namespace Winters::Asset
{
    inline uint64_t FNV1a(const char* str)
    {
        uint64_t h = 0xcbf29ce484222325ull;
        while (str && *str) { h ^= (uint8_t)*str++; h *= 0x100000001b3ull; }
        return h;
    }

    inline uint64_t FNV1a(const void* data, size_t size)
    {
        uint64_t h = 0xcbf29ce484222325ull;
        const uint8_t* p = static_cast<const uint8_t*>(data);
        for (size_t i = 0; i < size; ++i) { h ^= p[i]; h *= 0x100000001b3ull; }
        return h;
    }
}
```

`WMeshWriter.cpp:25-30` 의 static `FNV1a` 제거 후 include `Hash.h`.

### 4.2 vcxproj — `Engine/Include/Engine.vcxproj` (+ .filters)

| 폴더 | 필터 |
|---|---|
| `Public/AssetFormat/Common/` | `01. AssetFormat\Common` |
| `Public/AssetFormat/Anim/` | `01. AssetFormat\Anim` |
| `Private/AssetFormat/Anim/` | `01. AssetFormat\Anim` |

### 4.3 EngineSDK 동기화

post-build event 또는 수동:
- `EngineSDK/inc/AssetFormat/Common/Hash.h`
- `EngineSDK/inc/AssetFormat/Anim/WSkelFormat.h`
- `EngineSDK/inc/AssetFormat/Anim/WAnimFormat.h`

### M0 합격
- Hash.h 추가 후 빌드 통과
- 기존 동작 변경 0

---

## 5. M1 — `.wskel` (Layer 2 + 4)

### 5.1 `Engine/Public/AssetFormat/Anim/WSkelFormat.h` (POD 크기 정정)

```cpp
#pragma once
#include "WintersAPI.h"
#include <cstdint>

namespace Winters::Asset
{
    constexpr char WSKEL_MAGIC[4] = { 'W','S','K','L' };

#pragma pack(push, 1)
    struct SkelMetaHeader
    {
        char     magic[4];           //                    4
        uint32_t bone_count;         //                    4
        uint32_t socket_count;       //                    4
        uint32_t reserved[5];        //                   20
    };
    static_assert(sizeof(SkelMetaHeader) == 32, "SkelMetaHeader == 32");

    struct BoneNode
    {
        uint64_t name_hash;                //              8
        char     name[64];                 //             64
        int32_t  parent_index;             //              4
        float    rest_transform[16];       //             64  (DX row-major 전치)
        uint32_t child_count;              //              4
        uint32_t first_child_index;        //              4
        uint32_t reserved[27];             // ★ v3 정정 108  (총 256)
    };
    static_assert(sizeof(BoneNode) == 256, "BoneNode == 256");

    struct GlobalRootMatrix
    {
        float global_inverse_root[16];     //             64
        uint32_t reserved[16];             //             64  (총 128)
    };
    static_assert(sizeof(GlobalRootMatrix) == 128, "GlobalRootMatrix == 128");

    struct SocketEntry
    {
        char     name[32];             //                 32
        uint64_t name_hash;            //                  8
        int32_t  parent_bone_index;    //                  4
        float    local_offset[16];     //                 64
        uint32_t reserved[5];          // ★ v3 정정       20  (총 128)
    };
    static_assert(sizeof(SocketEntry) == 128, "SocketEntry == 128");
#pragma pack(pop)
}
```

### 5.2 `Engine/Public/AssetFormat/Anim/WSkelWriter.h`

```cpp
#pragma once
#include "AssetFormat/Anim/WSkelFormat.h"
#include <string>
#include <vector>
#include <unordered_map>     // ★ v4

struct aiScene;

namespace Winters::Asset
{
    struct WSkelWriteOptions
    {
        bool bExtractSockets = false;
    };

    struct WSkelWriteResult
    {
        uint64_t skel_hash = 0;
        std::vector<std::string> bone_order_by_index;          // WMeshWriter --skel 에 전달
        std::unordered_map<std::string, uint32_t> name_to_idx;
    };

    class CWSkelWriter
    {
    public:
        // ★ Model.cpp::LoadSkeleton 의 DFS pre-order 와 동일 규칙 (bone index 권위)
        static bool WriteFromAssimp(const aiScene* pScene,
                                     const wchar_t* pOutPath,
                                     WSkelWriteResult& outResult,
                                     const WSkelWriteOptions& opt = {});
    };
}
```

### 5.3 `WSkelWriter.cpp` (★ v4 — payload 만 + SaveToFile 자동 헤더)

```cpp
#include "AssetFormat/Anim/WSkelWriter.h"
#include "AssetFormat/Common/BinaryWriter.h"
#include "AssetFormat/Common/Hash.h"
#include <assimp/scene.h>
#include <DirectXMath.h>
#include <functional>
#include <cstring>

using namespace Winters::Asset;
using namespace DirectX;

static XMMATRIX ConvertAndTranspose(const aiMatrix4x4& m)
{
    XMMATRIX r;
    r.r[0] = XMVectorSet(m.a1, m.b1, m.c1, m.d1);
    r.r[1] = XMVectorSet(m.a2, m.b2, m.c2, m.d2);
    r.r[2] = XMVectorSet(m.a3, m.b3, m.c3, m.d3);
    r.r[3] = XMVectorSet(m.a4, m.b4, m.c4, m.d4);
    return r;
}

bool CWSkelWriter::WriteFromAssimp(const aiScene* pScene, const wchar_t* pOutPath,
                                     WSkelWriteResult& outResult, const WSkelWriteOptions&)
{
    if (!pScene || !pScene->mRootNode) return false;

    std::vector<const aiNode*> nodes;
    std::vector<int32_t> parents;

    // ★ Model.cpp::LoadSkeleton 과 동일 DFS pre-order — 모든 노드 → 본
    std::function<void(const aiNode*, int32_t)> dfs = [&](const aiNode* p, int32_t parent) {
        const uint32_t idx = (uint32_t)nodes.size();
        nodes.push_back(p);
        parents.push_back(parent);
        outResult.name_to_idx[p->mName.C_Str()] = idx;
        outResult.bone_order_by_index.emplace_back(p->mName.C_Str());
        for (uint32_t i = 0; i < p->mNumChildren; ++i)
            dfs(p->mChildren[i], (int32_t)idx);
    };
    dfs(pScene->mRootNode, -1);

    std::vector<BoneNode> bones(nodes.size());
    uint64_t skelHash = 0xcbf29ce484222325ull;
    for (uint32_t i = 0; i < nodes.size(); ++i) {
        BoneNode& b = bones[i];
        const char* nm = nodes[i]->mName.C_Str();
        b.name_hash = FNV1a(nm);
        std::strncpy(b.name, nm, sizeof(b.name) - 1);
        b.parent_index = parents[i];
        XMMATRIX m = ConvertAndTranspose(nodes[i]->mTransformation);
        std::memcpy(b.rest_transform, &m, sizeof(float) * 16);
        skelHash ^= b.name_hash;
        skelHash *= 0x100000001b3ull;
    }

    // child_count / first_child_index
    for (auto& b : bones) { b.child_count = 0; b.first_child_index = 0xFFFFFFFFu; }
    for (uint32_t i = 0; i < bones.size(); ++i) {
        int p = bones[i].parent_index;
        if (p >= 0) {
            if (bones[p].child_count == 0) bones[p].first_child_index = i;
            bones[p].child_count++;
        }
    }

    // GlobalInverseRoot
    XMMATRIX rootGlobal = ConvertAndTranspose(pScene->mRootNode->mTransformation);
    XMMATRIX rootInv = XMMatrixInverse(nullptr, rootGlobal);
    GlobalRootMatrix grm{};
    XMFLOAT4X4 fInv;
    XMStoreFloat4x4(&fInv, rootInv);
    std::memcpy(grm.global_inverse_root, &fInv, sizeof(float) * 16);

    // ★ v4 — payload 만 Write. SaveToFile 가 WintersFileHeader 자동.
    CBinaryWriter w;
    SkelMetaHeader hdr{};
    std::memcpy(hdr.magic, WSKEL_MAGIC, 4);
    hdr.bone_count = (uint32_t)bones.size();
    hdr.socket_count = 0;
    w.Write(hdr);
    for (const auto& b : bones) w.Write(b);
    w.Write(grm);

    outResult.skel_hash = skelHash;
    return w.SaveToFile(pOutPath, WF_NONE);
}
```

### 5.4 `WSkelLoader.h/.cpp`

```cpp
// WSkelLoader.h
#pragma once
#include "AssetFormat/Anim/WSkelFormat.h"
#include <vector>

namespace Winters::Asset
{
    struct WSkelLoaded
    {
        SkelMetaHeader header{};
        std::vector<uint8_t> rawPayload;
        const BoneNode*  bones = nullptr;
        const GlobalRootMatrix* globalRoot = nullptr;
        const SocketEntry* sockets = nullptr;
        uint64_t skelHash = 0;
    };

    class CWSkelLoader
    {
    public:
        static bool Load(const wchar_t* path, WSkelLoaded& out);
    };
}
```

`.cpp` 는 `CBinaryReader` 사용 — 패턴은 `WMeshLoader.cpp` 그대로 차용.

### M1 합격
- 컴파일 통과 (static_assert 4)
- 가렌 wskel 산출 + info 로 `bones=N parent=...` 덤프

---

## 6. M2 — `.wmesh` skinned writer 보정 (★ 핵심, Layer 4)

### 6.1 `WMeshWriter.h` 시그니처 확장

```cpp
#pragma once
#include "AssetFormat/Mesh/WMeshFormat.h"
#include <string>
#include <unordered_map>   // ★ v4

struct aiScene;

namespace Winters::Asset
{
    struct WMeshWriteOptions
    {
        bool  bWriteBounds = true;
        bool  bMirrorX     = false;
        float fScale       = 1.f;

        // ★ v4 — non-null 이면 wskel 권위 모드 (vertex bone indices = wskel order, BoneEntry 정렬)
        const std::unordered_map<std::string, uint32_t>* pSkelNameToIdx = nullptr;
    };

    class CWMeshWriter
    {
    public:
        static bool WriteFromAssimp(const aiScene* pScene,
                                     const wchar_t* pOutPath,
                                     const WMeshWriteOptions& opt = {});
    };
}
```

### 6.2 `WMeshWriter.cpp::CollectBones` — 권위 모드 + uint8 한계 거부

```cpp
static bool CollectBones(const aiScene* pScene,
    const std::unordered_map<std::string, uint32_t>* pSkelNameToIdx,
    std::unordered_map<std::string, uint32_t>& outMap,
    std::vector<BoneEntry>& outBones)
{
    if (pSkelNameToIdx)
    {
        // ★ v4 P1 #6 — uint8 한계 (4 × uint8 packed in indices field)
        if (pSkelNameToIdx->size() >= 256) {
            OutputDebugStringA("[WMeshWriter] FATAL: skel bone_count >= 256 — uint8 vertex index overflow\n");
            return false;
        }

        outBones.resize(pSkelNameToIdx->size());
        for (const auto& kv : *pSkelNameToIdx) {
            const uint32_t idx = kv.second;
            BoneEntry& e = outBones[idx];
            e.name_hash = FNV1a(kv.first.c_str());
            std::strncpy(e.name, kv.first.c_str(), sizeof(e.name) - 1);
            e.parent_index = -1;   // wskel 권위 — 메시 측 미사용
            XMMATRIX I = XMMatrixIdentity();
            std::memcpy(e.offset_matrix, &I, sizeof(float) * 16);
            outMap[kv.first] = idx;
        }
        // 메시에 등장한 본만 진짜 offset_matrix
        for (uint32_t m = 0; m < pScene->mNumMeshes; ++m) {
            const aiMesh* mesh = pScene->mMeshes[m];
            for (uint32_t b = 0; b < mesh->mNumBones; ++b) {
                const aiBone* bone = mesh->mBones[b];
                auto it = pSkelNameToIdx->find(bone->mName.C_Str());
                if (it == pSkelNameToIdx->end()) continue;
                BoneEntry& e = outBones[it->second];
                XMMATRIX off = ConvertAndTranspose(bone->mOffsetMatrix);
                std::memcpy(e.offset_matrix, &off, sizeof(float) * 16);
            }
        }
        return true;
    }

    // 기존 동작 (호환용)
    for (uint32_t m = 0; m < pScene->mNumMeshes; ++m) {
        const aiMesh* mesh = pScene->mMeshes[m];
        for (uint32_t b = 0; b < mesh->mNumBones; ++b) {
            const aiBone* bone = mesh->mBones[b];
            const std::string nm = bone->mName.C_Str();
            if (outMap.count(nm)) continue;
            const uint32_t idx = (uint32_t)outBones.size();
            outMap[nm] = idx;
            BoneEntry e{};
            e.name_hash = FNV1a(nm.c_str());
            std::strncpy(e.name, nm.c_str(), sizeof(e.name) - 1);
            e.parent_index = -1;
            XMMATRIX off = ConvertAndTranspose(bone->mOffsetMatrix);
            std::memcpy(e.offset_matrix, &off, sizeof(float) * 16);
            outBones.push_back(e);
        }
    }
    return true;
}
```

### 6.3 호출처 — `CollectBones` 가 false 반환 시 Writer 도 false

```cpp
bool CWMeshWriter::WriteFromAssimp(...)
{
    ...
    if (!CollectBones(pScene, opt.pSkelNameToIdx, boneMap, bones))
        return false;
    ...
}
```

### M2 합격
- static mesh `.wmesh` regression 0
- 가렌 `--skel` 모드: BoneEntry count = wskel count + name_hash 일치
- bone>=256 시 명시 거부 + 로그

---

## 7. M3 — `.wanim` (tick 단위, Layer 2 + 4)

### 7.1 `WAnimFormat.h` (POD 정정)

```cpp
#pragma once
#include "WintersAPI.h"
#include <cstdint>

namespace Winters::Asset
{
    constexpr char WANIM_MAGIC[4] = { 'W','A','N','M' };

    enum class eAnimEventType : uint16_t
    {
        HitStart = 0, HitEnd = 1, Footstep = 2, SFX = 3,
        VfxSpawn = 4, DamageNumber = 5, SkillCast = 6, SkillRelease = 7,
        Custom = 0xFFFF,
    };

#pragma pack(push, 1)
    struct AnimMetaHeader
    {
        char     magic[4];           //              4
        uint32_t channel_count;      //              4
        float    duration_ticks;     // ★ tick      4
        float    ticks_per_second;   //              4
        uint32_t total_key_count;    //              4
        uint32_t event_count;        //              4
        uint8_t  is_loop;            //              1
        uint8_t  reserved[7];        //              7
    };
    static_assert(sizeof(AnimMetaHeader) == 32, "AnimMetaHeader == 32");

    struct AnimChannel
    {
        uint64_t bone_name_hash;     //              8
        uint32_t pos_key_count;      //              4
        uint32_t pos_offset;         //              4
        uint32_t rot_key_count;      //              4
        uint32_t rot_offset;         //              4
        uint32_t scl_key_count;      //              4
        uint32_t scl_offset;         //              4
        int32_t  bone_index_cached;  //              4
        uint32_t reserved;           // ★ v3       4 (총 40)
    };
    static_assert(sizeof(AnimChannel) == 40, "AnimChannel == 40");

    struct VectorKey { float time_ticks; float x, y, z; };
    static_assert(sizeof(VectorKey) == 16);
    struct QuatKey   { float time_ticks; float x, y, z, w; };
    static_assert(sizeof(QuatKey) == 20);

    struct AnimEvent
    {
        float    time_ticks;          //             4
        uint16_t type;                //             2
        uint16_t reserved0;           //             2
        uint32_t skill_id;            //             4
        uint32_t param_u32;           //             4
        float    param_f32;           //             4
        uint64_t string_hash;         //             8
        uint32_t reserved1;           // ★ v3      4 (총 32)
    };
    static_assert(sizeof(AnimEvent) == 32, "AnimEvent == 32");

    struct WAnimTrailer { uint64_t skel_hash; };
#pragma pack(pop)
}
```

### 7.2 `WAnimWriter.cpp` (★ v4 — payload 만)

```cpp
bool CWAnimWriter::WriteFromAssimp(const aiAnimation* pAnim, const aiScene*,
                                     uint64_t skelHash, const wchar_t* pOutPath)
{
    if (!pAnim) return false;

    const double tps = (pAnim->mTicksPerSecond > 0.0) ? pAnim->mTicksPerSecond : 24.0;

    AnimMetaHeader hdr{};
    std::memcpy(hdr.magic, WANIM_MAGIC, 4);
    hdr.channel_count = pAnim->mNumChannels;
    hdr.duration_ticks = (float)pAnim->mDuration;
    hdr.ticks_per_second = (float)tps;
    hdr.is_loop = IsLoopAnim(pAnim->mName.C_Str()) ? 1 : 0;

    std::vector<AnimChannel> channels;
    std::vector<uint8_t>     keyBlock;
    uint32_t totalKeys = 0;

    for (uint32_t c = 0; c < pAnim->mNumChannels; ++c) {
        const aiNodeAnim* node = pAnim->mChannels[c];
        AnimChannel ch{};
        ch.bone_name_hash = FNV1a(node->mNodeName.C_Str());
        ch.bone_index_cached = -1;

        ch.pos_offset = (uint32_t)keyBlock.size();
        ch.pos_key_count = node->mNumPositionKeys;
        for (uint32_t k = 0; k < node->mNumPositionKeys; ++k) {
            const auto& pk = node->mPositionKeys[k];
            VectorKey vk{ (float)pk.mTime, pk.mValue.x, pk.mValue.y, pk.mValue.z };
            AppendAs(keyBlock, vk);
        }
        ch.rot_offset = (uint32_t)keyBlock.size();
        ch.rot_key_count = node->mNumRotationKeys;
        for (uint32_t k = 0; k < node->mNumRotationKeys; ++k) {
            const auto& rk = node->mRotationKeys[k];
            QuatKey qk{ (float)rk.mTime,
                         rk.mValue.x, rk.mValue.y, rk.mValue.z, rk.mValue.w };
            AppendAs(keyBlock, qk);
        }
        ch.scl_offset = (uint32_t)keyBlock.size();
        ch.scl_key_count = node->mNumScalingKeys;
        for (uint32_t k = 0; k < node->mNumScalingKeys; ++k) {
            const auto& sk = node->mScalingKeys[k];
            VectorKey vk{ (float)sk.mTime, sk.mValue.x, sk.mValue.y, sk.mValue.z };
            AppendAs(keyBlock, vk);
        }
        totalKeys += ch.pos_key_count + ch.rot_key_count + ch.scl_key_count;
        channels.push_back(ch);
    }

    hdr.total_key_count = totalKeys;
    hdr.event_count = 0;

    // ★ v4 — payload 만 (SaveToFile 자동 헤더)
    CBinaryWriter w;
    w.Write(hdr);
    for (const auto& ch : channels) w.Write(ch);
    w.WriteBytes(keyBlock.data(), keyBlock.size());
    WAnimTrailer tr{ skelHash };
    w.Write(tr);
    return w.SaveToFile(pOutPath, WF_NONE);
}
```

### 7.3 `WAnimLoader::LoadAsAnimation` (★ v4 — Create() 팩토리 + AddChannel)

```cpp
#include "Resource/Animation.h"
#include "Resource/Skeleton.h"

std::unique_ptr<Engine::CAnimation> CWAnimLoader::LoadAsAnimation(
    const wchar_t* path, uint64_t expectedSkelHash, const Engine::CSkeleton* pSkel,
    const std::wstring& nameForDebug)
{
    // 1. 파일 읽기 — CBinaryReader 로 Header(WINT) + payload + trailer 검증
    //    (코드 생략 — 패턴은 WMeshLoader 동일)
    AnimMetaHeader hdr;          // 채워짐
    std::vector<AnimChannel> chs;// 채워짐
    std::vector<uint8_t> keyBlk; // 채워짐
    uint64_t trailerHash;        // 채워짐
    if (trailerHash != expectedSkelHash) return nullptr;

    // 2. ★ Create() 팩토리 — std::make_unique 안 됨 (private ctor)
    auto anim = Engine::CAnimation::Create(
        std::string(nameForDebug.begin(), nameForDebug.end()),
        (f64_t)hdr.duration_ticks,
        (f64_t)hdr.ticks_per_second);

    // 3. 채널 → BoneChannel 빌드 → AddChannel
    for (uint32_t c = 0; c < hdr.channel_count; ++c) {
        const AnimChannel& ch = chs[c];
        Engine::BoneChannel bc;
        bc.iBoneIndex = -1;   // ResolveBoneIndices 에서 채움
        // bc.strBoneName 은 hash 만으론 복원 불가 → wanim 에 name 보강 또는 skel 의 name_hash 매칭
        // MVP: skel 에서 hash → name 역참조 (CSkeleton 에 FindNameByHash 추가 권장)

        // pos / rot / scl 키 복원 (tick 단위 그대로)
        const VectorKey* posKeys = reinterpret_cast<const VectorKey*>(keyBlk.data() + ch.pos_offset);
        for (uint32_t k = 0; k < ch.pos_key_count; ++k) {
            Engine::VectorKey vk;
            vk.dTime = (f64_t)posKeys[k].time_ticks;
            vk.vValue = { posKeys[k].x, posKeys[k].y, posKeys[k].z };
            bc.vecPositionKeys.push_back(vk);
        }
        const QuatKey* rotKeys = reinterpret_cast<const QuatKey*>(keyBlk.data() + ch.rot_offset);
        for (uint32_t k = 0; k < ch.rot_key_count; ++k) {
            Engine::QuatKey qk;
            qk.dTime = (f64_t)rotKeys[k].time_ticks;
            qk.vValue = { rotKeys[k].x, rotKeys[k].y, rotKeys[k].z, rotKeys[k].w };
            bc.vecRotationKeys.push_back(qk);
        }
        const VectorKey* sclKeys = reinterpret_cast<const VectorKey*>(keyBlk.data() + ch.scl_offset);
        for (uint32_t k = 0; k < ch.scl_key_count; ++k) {
            Engine::VectorKey vk;
            vk.dTime = (f64_t)sclKeys[k].time_ticks;
            vk.vValue = { sclKeys[k].x, sclKeys[k].y, sclKeys[k].z };
            bc.vecScaleKeys.push_back(vk);
        }
        anim->AddChannel(bc);
    }

    // 4. ResolveBoneIndices — bone_name_hash 로 skel index 찾기
    //    (CAnimation::ResolveBoneIndices 가 이미 존재 — 또는 로더 측에서 직접 매칭)
    if (pSkel) anim->ResolveBoneIndices(pSkel);

    return anim;
}
```

> **개선 권장**: `WAnimChannel` 에 `name` 32B 필드 추가 또는 `CSkeleton::FindNameByHash(hash) → string` 추가. 둘 중 하나.
> MVP 는 `CSkeleton::FindNameByHash` 신규가 가벼움 (skel 측에 hash → name 맵 추가).

### M3 합격
- 가렌 31 wanim 산출, info 로 channel/duration_ticks/skel_hash 덤프

---

## 8. M4 — Converter CLI (Layer 5)

`Engine/Private/Tools/AssetConverter/main.cpp` 4 커맨드:

```cpp
int main(int argc, char** argv) {
    if (argc < 2) return 2;
    const std::string cmd = argv[1];
    if (cmd == "mesh") return CmdMesh(argc - 1, argv + 1);
    if (cmd == "skel") return CmdSkel(argc - 1, argv + 1);
    if (cmd == "anim") return CmdAnim(argc - 1, argv + 1);
    if (cmd == "info") return CmdInfo(argc - 1, argv + 1);
    return 2;
}
```

`CmdMesh` 가 `--skel <wskel>` 받으면 `WSkelLoader::Load` 로 name_to_idx 복원 후 옵션 주입.
`CmdAnim` 은 `aiScene::mNumAnimations` 만큼 반복 → `<output_dir>/<anim_name>.wanim`.

### M4 합격
- 4 커맨드 모두 `--help` + 정상 동작

---

## 9. M5 — CModel 통합 (★ BuildSkeletonFromStage3, Layer 6)

### 9.1 `CModel::LoadModel` 분기

```cpp
bool bUseWMesh = false, bUseWSkel = false;
WMeshLoaded wm; WSkelLoaded ws;

const std::string wmeshPath = ReplaceExtToWMesh(strFilePath);
if (std::filesystem::exists(wmeshPath) && CWMeshLoader::Load(toWide(wmeshPath).c_str(), wm)) {
    bUseWMesh = true;
    if (wm.header.bone_count > 0) {
        const std::string wskelPath = ReplaceExt(strFilePath, ".wskel");
        if (std::filesystem::exists(wskelPath) && CWSkelLoader::Load(toWide(wskelPath).c_str(), ws)) {
            bUseWSkel = true;
            // ★ v4 — bone_count 미스매치 시 fallback
            if (ws.header.bone_count != wm.header.bone_count) {
                OutputDebugStringA("[CModel] wskel/wmesh bone count mismatch — Assimp fallback\n");
                bUseWMesh = false;
                bUseWSkel = false;
            }
        } else {
            OutputDebugStringA("[CModel] wmesh has bones but wskel missing — Assimp fallback\n");
            bUseWMesh = false;
        }
    }
}

const aiScene* pScene = importer.ReadFile(strFilePath, postFlags);
if (!pScene) return E_FAIL;
LoadTextures(pDevice, pScene, dir);

if (bUseWMesh && BuildMeshesFromWMesh(pDevice, wm, m_vecMeshes)) {
    if (bUseWSkel) {
        // ★ v4 — wskel rest + wmesh offset 합성
        m_pSkeleton = BuildSkeletonFromStage3(ws, wm);
        m_bHasBones = true;

        // anims/*.wanim
        std::filesystem::path animDir = std::filesystem::path(strFilePath).parent_path() / "anims";
        if (std::filesystem::exists(animDir)) {
            for (const auto& entry : std::filesystem::directory_iterator(animDir)) {
                if (entry.path().extension() != L".wanim") continue;
                auto anim = CWAnimLoader::LoadAsAnimation(
                    entry.path().c_str(), ws.skelHash, m_pSkeleton.get(),
                    entry.path().stem().wstring());
                if (anim) m_vecAnimations.push_back(std::move(anim));
            }
            m_iAnimCount = (uint32_t)m_vecAnimations.size();
        }
    } else {
        m_bHasBones = false;
        m_iAnimCount = 0;
    }
    return S_OK;
}

// 레거시 Assimp
LoadSkeleton(pScene);
ProcessNode(pDevice, pScene->mRootNode, pScene);
m_iAnimCount = pScene->mNumAnimations;
if (m_iAnimCount > 0 && m_pSkeleton) LoadAnimations(pScene);
return S_OK;
```

### 9.2 `BuildSkeletonFromStage3` 신규 헬퍼 (★ v4 — wskel + wmesh 합성)

```cpp
std::unique_ptr<Engine::CSkeleton> CModel::BuildSkeletonFromStage3(
    const Winters::Asset::WSkelLoaded& ws,
    const Winters::Asset::WMeshLoaded& wm)
{
    using namespace Engine;
    using namespace Winters::Asset;

    auto skel = CSkeleton::Create();   // ★ Create() 팩토리

    const uint32_t N = ws.header.bone_count;
    // 전제: M2 권위 모드로 wmesh::BoneEntry 가 wskel index 와 동일 순서/개수
    // 미스매치는 LoadModel 에서 이미 검증됨

    for (uint32_t i = 0; i < N; ++i) {
        const BoneNode& bn = ws.bones[i];
        const BoneEntry& be = wm.bones[i];

        XMFLOAT4X4 matOffset, matRest;
        std::memcpy(&matOffset, be.offset_matrix, sizeof(float) * 16);
        std::memcpy(&matRest,   bn.rest_transform, sizeof(float) * 16);

        skel->AddBone(std::string(bn.name), bn.parent_index, matOffset, matRest);
    }

    XMFLOAT4X4 grm;
    std::memcpy(&grm, ws.globalRoot->global_inverse_root, sizeof(float) * 16);
    skel->SetGlobalInverseRoot(grm);

    return skel;
}
```

### M5 합격
- 가렌 BanPick → InGame 진입 시:
```
[CModel] .wmesh fast-path: ...garen.wmesh
[CModel] Loaded from .wmesh: meshes=2 textures=2
[CModel] wskel loaded: bones=N hash=0x...
[CModel] Loaded 31 wanim files
```
- `falling back to Assimp` 0

---

## 10. M6 — 가렌 visual diff (Layer 6)

`Champion Tuner` 패널에 `[ ] Use Stage 3 Path` 토글 — 재로딩 후 동일 시각 + 동일 프레임 hook 발동 검증.

### 합격
- BA/Q/W/E/R + idle/run 시각적 미감지
- `[FrameEvent] CAST/RECOVERY` 로그 시간 일치
- 60s 연속 메모리 누수 0

---

## 11. M6.5 — 5챔프 경로 마이그레이션 (★ v4 신설, P0)

### 11.1 [Scene_InGame.cpp:138-189](Client/Private/Scene/Scene_InGame.cpp:138) 일괄 변경

**before** (5 챔프 패턴 동일):
```cpp
m_Irelia.Init("C:/Users/user/Desktop/LOL_Resource/Character/Irelia/irelia_fixed.fbx", ...);
m_Irelia.LoadMeshTexture(0, L"C:/Users/user/Desktop/LOL_Resource/Character/Irelia/irelia_base_blades_tx_cm.png");
// ...
```

**after** (가렌 패턴 그대로):
```cpp
m_Irelia.Init("Client/Bin/Resource/Texture/Character/Irelia/irelia_fixed.fbx", L"Shaders/Mesh3D.hlsl");
m_Irelia.LoadMeshTexture(0, L"Client/Bin/Resource/Texture/Character/Irelia/irelia_base_blades_tx_cm.png");
// ... (5 챔프 모두 SRC base 만 교체)
```

### 11.2 사전 검증

각 챔프 폴더에 FBX/PNG 존재 확인 (이미 `Client/Bin/Resource/Texture/Character/<Champ>/` 로 이동된 상태):
- Irelia: `irelia.fbx` 또는 `irelia_fixed.fbx`?
- Yasuo: `yasuo_fixed.fbx`
- Sylas: `sylas.fbx`
- Viego: `viego_fixed.fbx`
- Kalista: `kalista.fbx`

### M6.5 합격
- F5 시 5 챔프 모두 BanPick → InGame 정상 (이전 = LOL_Resource 로드, 이후 = 새 경로 — 시각 동일)
- LOL_Resource 폴더 분리 가능 상태 (이번 사이클은 폴더 삭제 X — Stage 1.dat 등 미이관)

---

## 12. M7 — 6 챔프 일괄 변환 (Layer 3)

### 12.1 `Tools/convert_all_assets.bat` (★ v4 — basename 기준)

```bat
@echo off
REM ASCII-only (CLAUDE.md gotcha)
setlocal EnableDelayedExpansion
set ROOT=%~dp0
if "%ROOT:~-1%"=="\" set ROOT=%ROOT:~0,-1%

set CONV=%ROOT%\Bin\Debug\WintersAssetConverter.exe
set SRC=%ROOT%\..\Client\Bin\Resource\Texture\Character

set OK=0 & set FAIL=0

REM order: skel -> mesh --skel -> anim --skel
call :convert_champ "Irelia"  "irelia_fixed.fbx"
call :convert_champ "Yasuo"   "yasuo_fixed.fbx"
call :convert_champ "Sylas"   "sylas.fbx"
call :convert_champ "Viego"   "viego_fixed.fbx"
call :convert_champ "Kalista" "kalista.fbx"
call :convert_champ "Garen"   "garen.fbx"

echo OK=!OK! FAIL=!FAIL!
endlocal
exit /b 0

:convert_champ
set CHAMP=%~1
set FBX=%~2
set DIR=%SRC%\%CHAMP%
REM ★ v4 — basename 기준 출력 (irelia_fixed.fbx -> irelia_fixed.wmesh)
set BASE=%~n2

if not exist "%DIR%\%FBX%" ( set /a FAIL+=1 & goto :eof )

"%CONV%" skel "%DIR%\%FBX%" -o "%DIR%\%BASE%.wskel"
if errorlevel 1 ( set /a FAIL+=1 & goto :eof )

"%CONV%" mesh "%DIR%\%FBX%" --skel "%DIR%\%BASE%.wskel" -o "%DIR%\%BASE%.wmesh"
if errorlevel 1 ( set /a FAIL+=1 & goto :eof )

if not exist "%DIR%\anims" mkdir "%DIR%\anims"
"%CONV%" anim "%DIR%\%FBX%" --skel "%DIR%\%BASE%.wskel" -o "%DIR%\anims\"
if errorlevel 1 ( set /a FAIL+=1 & goto :eof )

set /a OK+=1
goto :eof
```

### 12.2 검증 매트릭스

| 챔프 | basename | wskel/wmesh bones | wanim | F5 |
|---|---|---|---|---|
| Garen | garen | 72 | 31 | ☐ |
| Irelia | irelia_fixed | ? | 68 | ☐ |
| Yasuo | yasuo_fixed | ? | 44 | ☐ |
| Sylas | sylas | ? | 80 | ☐ |
| Viego | viego_fixed | ? | 83 | ☐ |
| Kalista | kalista | ? | ? | ☐ |

### M7 합격
- 6 챔프 모두 fast-path + visual 동일
- `falling back` 로그 0
- Profiler: 챔프 평균 30× 단축

---

## 13. Codex 박제 (v4 갱신, 14건)

| # | 항목 | 대응 위치 |
|---|------|----------|
| 1 | wmesh bone index = wskel DFS | M1 (Writer pre-order) + M2 (--skel 권위 모드) |
| 2 | Animator tick 단위 | WAnim duration_ticks + key time_ticks |
| 3 | POD 크기 정정 | M1 / M3 헤더 |
| 4 | **CBinaryWriter payload only + SaveToFile 자동 헤더** | ★ v4 — M1/M3 Writer |
| 5 | **CSkeleton Stage3 합성 (ws + wm)** | ★ v4 — `BuildSkeletonFromStage3` |
| 6 | **5챔프 경로 마이그레이션** | ★ v4 — M6.5 |
| 7 | Create() 팩토리 강제 | LoadAsAnimation / BuildSkeletonFromStage3 |
| 8 | bone>=256 거부 | M2 CollectBones |
| 9 | basename 기준 .bat | M7 `%~n2` |
| 10 | aiAnimation::mTicksPerSecond=0 | 24.0 fallback |
| 11 | bone_count 미스매치 | LoadModel 에서 ws/wm 비교 후 fallback |
| 12 | skel_hash mismatch | WAnimLoader trailer 검증 |
| 13 | 채널 없는 본 = rest_transform | 기존 Animator 가 처리 (matRestLocal 정확히 옮기면 OK) |
| 14 | GlobalInverseRoot | wskel globalRoot → SetGlobalInverseRoot |

---

## 14. 사이클 종료 후 갱신할 파일

1. **CLAUDE.md** L11-18 — `현재 진행`/`다음` 갱신
2. **CLAUDE.md** Phase B-9 Gotchas:
   - **G-9-1**: `CBinaryWriter` payload only — `SaveToFile()` 가 `WintersFileHeader` 자동
   - **G-9-2**: 챔프 wmesh+wskel+anim 셋트 + `BuildSkeletonFromStage3(ws, wm)` 합성 (matOffset=wmesh / matRestLocal=wskel)
   - **G-9-3**: 변환 순서 `skel → mesh --skel → anim` 강제 (그 외 = bone index 파괴)
   - **G-9-4**: wanim 시간 단위 = Assimp tick (sec 아님 — Animator/SkillDef.castFrame 정합)
   - **G-9-5**: bone_count >= 256 시 vertex uint8 overflow (M2 명시 거부)
   - **G-9-6**: `.bat` 출력 = FBX basename (`%~n2`) — `ReplaceExtToWMesh` 와 매칭
3. **MEMORY.md** + 신규 `project_phase_b9_garen_wskel_wanim.md`
4. **`.md/plan/WintersFormat/04_STAGE3_WANIM_WSKEL.md`** — 본 v4 권위 포팅
5. **본 계획서** — 학습 결과 부록

---

## 15. 후속 사이클

- **B-10**: 잔여 챔프 일괄 (Annie/Ashe/Fiora/Riven/Jax/MasterYi/Kindred/Yone/Zed)
- **B-11 (선택)**: AnimEvent → SkillDef.castFrame 박제
- **C-0/C-1/D/E**: .wtex/.wmat/.wmap/Bundle

---

## 16. 예상 소요 (v4 정정)

| 마일스톤 | 작업 | 시간 |
|---|---|---|
| M0 (포맷) | Hash.h + POD + vcxproj | 0.5h |
| M1 (.wskel) | DFS Writer/Loader | 2h |
| M2 (.wmesh --skel) | 권위 모드 + uint8 거부 | 1.5h |
| M3 (.wanim) | tick + Create()/AddChannel + FindNameByHash | 3h |
| M4 (CLI) | 4 커맨드 | 1h |
| M5 (CModel) | Stage3 합성 | 2h |
| M6 (visual diff) | 토글 + 가렌 5 스킬 | 1h |
| M6.5 (5챔프 경로) | Scene_InGame.cpp 일괄 치환 | 0.5h |
| M7 (6 챔프 변환) | bat + F5 6회 | 2.5h |
| 잔여 (gotcha + 메모) | | 1h |
| **합계** | | **15h** (~1.8일) |

---

## 한 줄 요약

**v4: Codex 3차 박제. CBinaryWriter payload only + BuildSkeletonFromStage3 ws+wm 합성 + 5챔프 경로 마이그레이션 (M6.5 신설). M0 → M7. 14건 박제, 15h 목표.**
