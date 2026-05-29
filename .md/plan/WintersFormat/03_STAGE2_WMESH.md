# Stage 2 — `.wmesh` 메시 포맷 (가장 중요, Stage 3 상위 순위)

> **목표**: Assimp 런타임 의존 제거 + 챔피언 로드 시간 50~200 ms → **< 1 ms**.
> `.fbx` / `.glb` → **컨버터 타임** `.wmesh` 생성 → 런타임은 파싱 없이 `memcpy` 로 GPU 직행.

> **배경 (CLAUDE.md Gotcha)**:
> - Assimp `aiMatrix4x4` 는 **column-vector** 관례 → DX `row-vector` 로 변환 시 **전치 필수**
> - 스켈레톤 모델에서 **정점 stride 혼재 금지** (VTXANIM 76B 통일)
> - `Final = Offset × GlobalTransform × GlobalInverseRoot` (DX row-major) 공식
>
> → 이 3 gotcha 를 포맷 레벨에서 **박제** 한다. 컨버터가 미리 전치된 행렬을 쓰므로 런타임은 실수 여지 없음.

---

## 1. 목표 / 비목표

**목표**
- FBX/glb → `.wmesh` 단일 바이너리 (Assimp 의존 컨버터 타임만)
- 런타임 로드 `memcpy` + `CreateBuffer` 만 — 파싱 0
- Vertex/Index/Bone/SubMesh/Material 전부 `.wmesh` 내부
- 본 오프셋 행렬은 **이미 DX row-major 전치 상태** 로 저장
- 정점 포맷은 `VertexFormatFlags` 비트로 런타임 감지
- 16/32 bit 인덱스 자동 선택 (정점 수 > 65535 → 32bit)
- SubMesh 별 material index + 해시 (Stage 4 wmat 참조)
- LoD 레벨 (선택, 향후 GPU-Driven 대비)

**비목표 (향후 Stage)**
- 애니메이션 / 본 계층 (Stage 3 — `.wanim` / `.wskel` 분리)
- Meshlet (Phase 3 GPU-Driven 때 추가 섹션)
- 텍스처 경로 — `.wmat` 로 분리
- Bounding Volume (AABB/Sphere) — Stage 2.5 (선택 섹션)

---

## 2. 파일 레이아웃

```
[ WintersFileHeader 16B ]
    magic="WINT", version_major=1, version_minor=0
    flags=LZ4(옵션), content_size=N

[ Payload (N byte) ]
    MeshMetaHeader         (36 B 고정)
    SubMeshDesc[]          (submesh_count × 48 B)
    Vertex Block           (sum of submesh vertex_count × vertex_stride)
    Index Block            (sum of submesh index_count × (2 or 4))
    BoneTable              (bone_count × 128 B = 64 name + 4 parent + 64 offset)
    LoDTable (옵션)        (lod_count × submesh_count × 8 B)
    BoundingVolume (옵션)  (submesh_count × 28 B : AABB 24B + Sphere 4B radius)

[ SHA256 32B ]
```

### 2.1 MeshMetaHeader

```cpp
// Engine/Public/AssetFormat/Mesh/WMeshFormat.h
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include <cstdint>

namespace Winters::Asset
{
    constexpr char WMESH_MAGIC[4] = { 'W','M','S','H' };   // Payload 선두에 한번 더 (안전장치)

    enum VertexFormatFlags : uint32_t
    {
        VF_Position   = 1u << 0,   // float3
        VF_Normal     = 1u << 1,   // float3
        VF_UV0        = 1u << 2,   // float2
        VF_UV1        = 1u << 3,   // float2
        VF_Tangent    = 1u << 4,   // float4 (w=handedness)
        VF_Color0     = 1u << 5,   // rgba8
        VF_BoneWeight = 1u << 6,   // float4 + uint4 (32 B 추가)
        // 32 bit 여유 — 최대 32 종 attribute
    };

    // 정적 메시 (VTXMESH): Position|Normal|UV0|Tangent = 44 B
    constexpr uint32_t VF_STATIC  = VF_Position | VF_Normal | VF_UV0 | VF_Tangent;

    // 스키닝 메시 (VTXANIM): 위 + BoneWeight = 76 B  (CLAUDE.md gotcha 기준 stride)
    constexpr uint32_t VF_SKINNED = VF_STATIC | VF_BoneWeight;

    #pragma pack(push, 1)
    struct MeshMetaHeader
    {
        char     magic[4];          // "WMSH"
        uint32_t submesh_count;
        uint32_t bone_count;         // 0 = static mesh
        uint32_t lod_count;          // 0 = no LoD
        uint32_t vertex_format_flags;// 전역 format (모든 서브메시 동일 stride)
        uint32_t vertex_stride;      // 계산 검증용
        uint32_t total_vertex_count;
        uint32_t total_index_count;
        uint32_t index_stride;       // 2 or 4
        uint8_t  has_bounding;       // 1 = bounding volume 섹션 포함
        uint8_t  reserved[3];
    };
    static_assert(sizeof(MeshMetaHeader) == 36);

    struct SubMeshDesc
    {
        uint32_t vertex_offset;      // Vertex Block 내 시작 byte
        uint32_t vertex_count;
        uint32_t index_offset;       // Index Block 내 시작 byte
        uint32_t index_count;
        uint32_t material_index;     // Stage 4 .wmat 배열 인덱스
        uint64_t material_hash;      // FNV-1a of material name (교차 검증)
        char     name[20];           // 디버그용 (예: "Body", "Blade")
    };
    static_assert(sizeof(SubMeshDesc) == 48);

    // Bone entry (128 B 고정)
    struct BoneEntry
    {
        uint64_t name_hash;           // FNV-1a of bone name
        char     name[64];            // 디버그용
        int32_t  parent_index;        // -1 = root
        float    offset_matrix[16];   // ★ 이미 Assimp→DX 전치 상태
        uint32_t channel_flag;        // 1 = has animation channel (Stage 3 연계)
        uint32_t reserved;
    };
    static_assert(sizeof(BoneEntry) == 128);

    struct SubMeshBounds
    {
        float aabb_min[3];
        float aabb_max[3];
        float sphere_center_radius[4]; // xyz = center, w = radius
    };
    static_assert(sizeof(SubMeshBounds) == 40);   // 28 B 아님 — 정정
    #pragma pack(pop)
}
```

**주의**: `SubMeshBounds` 가 40 B 인 이유 — sphere center(12) + radius(4) + aabb(24) = 40. 계획서 초안의 28 B 표기 정정.

### 2.2 정점 레이아웃 (VF_SKINNED, 76 B)

```cpp
#pragma pack(push, 1)
struct VertexSkinned
{
    float    pos[3];        // 12   VF_Position
    float    nrm[3];        // 12   VF_Normal
    float    uv[2];         //  8   VF_UV0
    float    tan[4];        // 16   VF_Tangent (w=handedness)
    float    weights[4];    // 16   VF_BoneWeight
    uint32_t indices;       //  4   4 bytes packed (4 × uint8 bone index)
    uint32_t reserved;      //  8   padding → stride 76 B (Winters 표준)
    // Wait: 12+12+8+16+16+4 = 68 → 패딩 8 = 76
};
static_assert(sizeof(VertexSkinned) == 76);
#pragma pack(pop)
```

**CLAUDE.md gotcha 박제**: "본 없는 서브메시도 VTXANIM (76 B) 으로 생성. VTXMESH(44B) 와 섞이면 skinned InputLayout 이 44B 정점 잘못 읽어 폭발." → 컨버터가 스켈레톤 유무를 판정해 **모델 전체 stride 강제 통일**.

```cpp
// 컨버터 규칙:
// if (scene->HasBones())    vertex_format = VF_SKINNED;   // 전 서브메시 76B
// else                       vertex_format = VF_STATIC;    // 전 서브메시 44B
```

본 없는 서브메시는 weight[0]=1.0, indices=0x00000000 주입.

---

## 3. 컨버터 — Assimp Scene → `.wmesh`

### 3.1 API

```cpp
// Engine/Public/AssetFormat/Mesh/WMeshWriter.h
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"

struct aiScene;  // Assimp 전방선언

namespace Winters::Asset
{
    struct WMeshWriteOptions
    {
        bool_t  bCompress       = true;   // LZ4
        bool_t  bWriteBounds    = true;
        bool_t  bFlipV          = false;  // glb 는 V 플립
        float   fScale          = 1.f;    // 스케일 리베이크 (맵 Xflip 회피용)
        bool_t  bMirrorX        = false;  // lol2gltf X 플립 워크어라운드 (gotcha)
    };

    class WINTERS_API CWMeshWriter
    {
    public:
        static HRESULT WriteFromAssimp(const aiScene* pScene,
                                        const wchar_t* pOutPath,
                                        const WMeshWriteOptions& opt);
    };
}
```

### 3.2 변환 알고리즘 (핵심)

```cpp
// Engine/Private/AssetFormat/Mesh/WMeshWriter.cpp
HRESULT CWMeshWriter::WriteFromAssimp(const aiScene* pScene,
                                       const wchar_t* pOutPath,
                                       const WMeshWriteOptions& opt)
{
    // 1. 전역 스켈레톤 유무 판정
    const bool_t bSkinned = [&]{
        for (uint32_t i = 0; i < pScene->mNumMeshes; ++i)
            if (pScene->mMeshes[i]->HasBones()) return true;
        return false;
    }();

    const uint32_t vertexFormat = bSkinned ? VF_SKINNED : VF_STATIC;
    const uint32_t vertexStride = bSkinned ? 76 : 44;

    // 2. 본 수집 (전 메시에서 유일한 본 리스트 → 인덱스 할당)
    std::unordered_map<std::string, uint32_t> boneIndexMap;
    std::vector<BoneEntry> bones;
    if (bSkinned) CollectBones(pScene, boneIndexMap, bones);

    // 3. Global Inverse Root — CLAUDE.md gotcha 공식
    XMMATRIX rootGlobal = ConvertAndTranspose(pScene->mRootNode->mTransformation);
    XMMATRIX rootInv    = XMMatrixInverse(nullptr, rootGlobal);

    // 4. SubMesh 별 정점/인덱스 직렬화
    std::vector<VertexSkinned>  vtxBlock;
    std::vector<uint32_t>       idxBlock;
    std::vector<SubMeshDesc>    subs;
    std::vector<SubMeshBounds>  bounds;

    for (uint32_t m = 0; m < pScene->mNumMeshes; ++m) {
        const aiMesh* mesh = pScene->mMeshes[m];
        SubMeshDesc desc{};
        desc.vertex_offset = (uint32_t)(vtxBlock.size() * vertexStride);
        desc.index_offset  = (uint32_t)(idxBlock.size() * sizeof(uint32_t));
        desc.vertex_count  = mesh->mNumVertices;
        desc.index_count   = mesh->mNumFaces * 3;
        desc.material_index = mesh->mMaterialIndex;
        desc.material_hash = ComputeMaterialHash(pScene, mesh->mMaterialIndex);
        std::strncpy(desc.name, mesh->mName.C_Str(), sizeof(desc.name) - 1);

        // 정점 변환
        AppendVertices(mesh, bSkinned, opt, boneIndexMap, vtxBlock);
        AppendIndices(mesh, idxBlock);

        // AABB / Sphere 계산
        if (opt.bWriteBounds) bounds.push_back(ComputeBounds(mesh));

        subs.push_back(desc);
    }

    // 5. 본 offset matrix 설정 — Final = Offset × GlobalTransform × GlobalInverseRoot
    // 주의: 여기서는 Offset 만 직접 저장. GlobalTransform 은 .wskel(Stage 3) 로 분리.
    for (auto& b : bones) {
        // aiBone::mOffsetMatrix 는 column-vector 관례 → 전치
        XMMATRIX off = ConvertAndTranspose(GetAiBoneOffset(pScene, b.name));
        std::memcpy(b.offset_matrix, &off, sizeof(float) * 16);
    }

    // 6. 옵션 스케일 / 미러 리베이크
    if (opt.fScale != 1.f || opt.bMirrorX) RebakeScale(vtxBlock, opt);

    // 7. 인덱스 16/32 bit 결정
    const bool_t bIdx32 = vtxBlock.size() > 65535;
    const uint32_t idxStride = bIdx32 ? 4 : 2;

    // 8. Header / Payload 직렬화
    CBinaryWriter w;
    MeshMetaHeader hdr{};
    std::memcpy(hdr.magic, WMESH_MAGIC, 4);
    hdr.submesh_count        = (uint32_t)subs.size();
    hdr.bone_count           = (uint32_t)bones.size();
    hdr.lod_count            = 0;
    hdr.vertex_format_flags  = vertexFormat;
    hdr.vertex_stride        = vertexStride;
    hdr.total_vertex_count   = (uint32_t)vtxBlock.size();
    hdr.total_index_count    = (uint32_t)idxBlock.size();
    hdr.index_stride         = idxStride;
    hdr.has_bounding         = opt.bWriteBounds ? 1 : 0;
    w.Write(hdr);

    for (const auto& s : subs) w.Write(s);
    w.WriteBytes(vtxBlock.data(), vtxBlock.size() * vertexStride);

    if (bIdx32) {
        w.WriteBytes(idxBlock.data(), idxBlock.size() * 4);
    } else {
        std::vector<uint16_t> idx16(idxBlock.begin(), idxBlock.end());
        w.WriteBytes(idx16.data(), idx16.size() * 2);
    }

    for (const auto& b : bones)  w.Write(b);
    for (const auto& bb : bounds) w.Write(bb);

    uint32_t flags = opt.bCompress ? WINTERS_FLAG_LZ4 : 0;
    return w.SaveToFile(pOutPath, flags);
}
```

### 3.3 `ConvertAndTranspose` — CLAUDE.md gotcha 박제

```cpp
// Engine/Private/AssetFormat/Mesh/AssimpMatrixConvert.h
inline XMMATRIX ConvertAndTranspose(const aiMatrix4x4& m)
{
    // Assimp: column-vector, Translation=4열
    // DirectX row-vector: Translation=4행 → 전치 필수
    XMMATRIX r;
    r.r[0] = XMVectorSet(m.a1, m.b1, m.c1, m.d1);
    r.r[1] = XMVectorSet(m.a2, m.b2, m.c2, m.d2);
    r.r[2] = XMVectorSet(m.a3, m.b3, m.c3, m.d3);
    r.r[3] = XMVectorSet(m.a4, m.b4, m.c4, m.d4);
    return r;
}
```

---

## 4. 런타임 로더

### 4.1 API

```cpp
// Engine/Public/AssetFormat/Mesh/WMeshLoader.h
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include <memory>

namespace Winters { class CMesh; class CDevice; }

namespace Winters::Asset
{
    class WINTERS_API CWMeshLoader
    {
    public:
        // 디스크 경로 → CMesh
        static std::shared_ptr<CMesh> Load(CDevice* pDevice, const std::wstring& path);

        // 번들 내부 mmap 바이트 직접 로드 (zero-copy)
        static std::shared_ptr<CMesh> LoadFromMemory(CDevice* pDevice,
                                                      const void* pData, size_t uSize);
    };
}
```

### 4.2 구현

```cpp
// Engine/Private/AssetFormat/Mesh/WMeshLoader.cpp
std::shared_ptr<CMesh> CWMeshLoader::Load(CDevice* pDevice, const std::wstring& path)
{
    // 1. 파일 로드 + SHA256 검증 (Stage 1 유틸)
    CWintersFile file;
    if (FAILED(CWintersFile::LoadFromDisk(path.c_str(), file)))
        return nullptr;

    return LoadFromMemory(pDevice, file.Decompressed(), file.DecompressedSize());
}

std::shared_ptr<CMesh> CWMeshLoader::LoadFromMemory(CDevice* pDevice,
                                                     const void* pData, size_t uSize)
{
    CBinaryReader r(pData, uSize);

    // 2. Meta 헤더
    auto hdr = r.Read<MeshMetaHeader>();
    if (std::memcmp(hdr.magic, WMESH_MAGIC, 4) != 0) return nullptr;

    // 3. SubMesh 테이블
    std::vector<SubMeshDesc> subs(hdr.submesh_count);
    r.ReadBytes(subs.data(), sizeof(SubMeshDesc) * hdr.submesh_count);

    // 4. Vertex 블록 — mmap 포인터 바로 GPU 업로드 (zero-copy 원칙)
    const uint8_t* pVtx = r.Peek();
    r.Skip(hdr.total_vertex_count * hdr.vertex_stride);

    // 5. Index 블록
    const uint8_t* pIdx = r.Peek();
    r.Skip(hdr.total_index_count * hdr.index_stride);

    // 6. Bone 테이블
    std::vector<BoneEntry> bones(hdr.bone_count);
    if (hdr.bone_count > 0)
        r.ReadBytes(bones.data(), sizeof(BoneEntry) * hdr.bone_count);

    // 7. Bounds 테이블
    std::vector<SubMeshBounds> bounds;
    if (hdr.has_bounding) {
        bounds.resize(hdr.submesh_count);
        r.ReadBytes(bounds.data(), sizeof(SubMeshBounds) * hdr.submesh_count);
    }

    // 8. GPU 리소스 생성 (D3D11 CreateBuffer — pSysMem 에 pVtx/pIdx 직접)
    auto mesh = std::make_shared<CMesh>();
    mesh->CreateVertexBuffer(pDevice, pVtx,
                              hdr.total_vertex_count * hdr.vertex_stride,
                              hdr.vertex_stride, hdr.vertex_format_flags);
    mesh->CreateIndexBuffer(pDevice, pIdx,
                             hdr.total_index_count, hdr.index_stride == 4);

    for (auto& s : subs) mesh->AddSubMesh(s);
    mesh->SetBones(std::move(bones));
    mesh->SetBounds(std::move(bounds));
    return mesh;
}
```

**핵심 최적화**: `r.Peek()` 로 mmap 포인터 그대로 `D3D11_SUBRESOURCE_DATA.pSysMem` 에 전달. memcpy 1회 제거.

---

## 5. CMesh 확장

```cpp
// Engine/Public/Resource/Mesh.h 확장
class CMesh
{
public:
    void CreateVertexBuffer(CDevice* pDev, const void* pData, size_t uSize,
                             uint32_t uStride, uint32_t uFormatFlags);
    void CreateIndexBuffer(CDevice* pDev, const void* pData, uint32_t uCount,
                            bool_t bIs32Bit);
    void AddSubMesh(const Winters::Asset::SubMeshDesc& desc);
    void SetBones(std::vector<Winters::Asset::BoneEntry> bones);
    void SetBounds(std::vector<Winters::Asset::SubMeshBounds> bounds);

    uint32_t GetSubMeshCount() const { return (uint32_t)m_subMeshes.size(); }
    const Winters::Asset::SubMeshDesc& GetSubMesh(uint32_t i) const { return m_subMeshes[i]; }

    // Stage 4 wmat 참조 — material_hash 로 머티리얼 테이블 룩업
    uint64_t GetSubMeshMaterialHash(uint32_t i) const { return m_subMeshes[i].material_hash; }

    const std::vector<Winters::Asset::BoneEntry>& GetBones() const { return m_bones; }

private:
    ComPtr<ID3D11Buffer>                        m_pVB;
    ComPtr<ID3D11Buffer>                        m_pIB;
    uint32_t                                    m_uVertexStride = 0;
    uint32_t                                    m_uVertexFormat = 0;
    bool_t                                      m_bIB32 = false;
    std::vector<Winters::Asset::SubMeshDesc>    m_subMeshes;
    std::vector<Winters::Asset::BoneEntry>      m_bones;
    std::vector<Winters::Asset::SubMeshBounds>  m_bounds;
};
```

---

## 6. 챔피언 5체 마이그레이션

현재 상태:
```
Bin/Resource/Characters/Irelia/*.fbx + *.png  (Assimp 런타임 로드)
Bin/Resource/Characters/Yasuo/*.fbx ...
Bin/Resource/Characters/Sylas/*.fbx ...
Bin/Resource/Characters/Viego/*.fbx ...
Bin/Resource/Characters/Kalista/*.fbx ...
```

타겟:
```
Bin/Resource/Characters/Irelia/body.wmesh  ← (Stage 3 wskel / Stage 4 wanim 분리)
                                skeleton.wskel
                                idle.wanim, attack.wanim, ...
                                body.wmat    ← Stage 5
                                body.wtex    ← Stage 4 BC7
```

### 6.1 일괄 변환 스크립트

```bat
:: Tools/convert_all_champions.bat
:: ⚠ 주석 한글 금지 (CLAUDE.md gotcha) — 영어 주석 only
@echo off
for %%C in (Irelia Yasuo Sylas Viego Kalista) do (
    WintersAssetConverter.exe mesh "Bin\Resource\Characters\%%C\body.fbx" ^
        -o "Bin\Resource\Characters\%%C\body.wmesh" --compress
)
```

### 6.2 Scene_InGame 분기

```cpp
// Client/Private/Scene/Scene_InGame.cpp
HRESULT Scene_InGame::LoadChampionMesh(const wchar_t* name)
{
    // 새 포맷 우선 탐색
    auto wmeshPath = std::wstring(L"Characters/") + name + L"/body.wmesh";
    if (FileExists(wmeshPath)) {
        m_pMesh = CWMeshLoader::Load(m_pDevice, wmeshPath);
        return S_OK;
    }

    // Fallback: 레거시 FBX (개발 빌드만)
#ifdef _DEBUG
    auto fbxPath = std::wstring(L"Characters/") + name + L"/body.fbx";
    m_pMesh = CAssimpLoader::Load(m_pDevice, fbxPath);
    return S_OK;
#else
    return E_FAIL;   // Release 는 .wmesh 강제
#endif
}
```

---

## 7. 성능 검증

**측정 환경**: i7-12700H, NVMe SSD, DX11 Debug.

| 에셋 | FBX (Assimp) | `.wmesh` (현재) | 목표 |
|---|---|---|---|
| Irelia (4 mesh, 68 anim) | 120 ms | ? | < 1 ms (mesh 만 — anim 별도) |
| Sylas (6 mesh, 80 anim) | 180 ms | ? | < 1 ms |
| Viego (6 mesh, 83 anim) | 220 ms | ? | < 1 ms |
| 맵 (21 오브젝트) | 2100 ms | ? | < 5 ms 전체 |

Phase 3-C Profiler 에서 `AssetFormat.Load.Mesh` 카테고리로 자동 수집.

**예상 수치**: `.wmesh` 는 파싱 0, SHA256 + CreateBuffer 만 → 400 KB 메시 기준 **0.5~0.8 ms**.

---

## 8. 테스트 체크리스트

- [ ] Irelia body.fbx → body.wmesh 변환 성공
- [ ] 새 로더로 Irelia 렌더 (같은 화면 / 좌우 반전 없음)
- [ ] 애니메이션 재생 (Stage 3 wanim 완성 전까진 static pose)
- [ ] SHA256 변조 테스트 — vertex 1 바이트 바꾼 파일 로드 거부
- [ ] stride 검증 — VF_SKINNED 모델을 VF_STATIC InputLayout 으로 바인딩 시 에러 반환
- [ ] 인덱스 16/32 bit 자동 선택 (맵처럼 정점 80000 짜리 테스트)
- [ ] LZ4 압축 on/off 크기 비교 (500 KB → 300 KB 예상)
- [ ] GoogleTest: Load roundtrip / Tamper reject / Empty mesh / Bone-less mesh

---

## 9. 보안 고려사항

CLAUDE.md 보안 §9 연계:

| 위협 | 방어 |
|---|---|
| 치트가 body.wmesh vertex 조작 (벽뚫기 확장, hitbox 늘리기) | SHA256 불일치 → 로드 거부 + `CTamperDetector` 신고 |
| 리버서가 .wmesh 복제 후 자기 캐릭터 교체 | Stage 9 Ed25519 서명 — 개발 키 필요 |
| 메모리 오버플로우 공격 (거대 vertex_count 로 OOM) | `hdr.total_vertex_count * hdr.vertex_stride > reasonable_limit` 체크 (< 128 MB) |
| material_index 범위 초과 | 로더에서 `subs[i].material_index < mat_count` 검증 |
| bone parent_index 악의 조작 (-2, 999 등) | 로더에서 `parent < 0 || parent >= bone_count` 거부 |

---

## 10. 보안 검증 루틴

```cpp
// Engine/Private/AssetFormat/Mesh/WMeshValidator.cpp
HRESULT ValidateMeshMeta(const MeshMetaHeader& hdr)
{
    // 상한선 — 악의적 파일 방어
    constexpr uint32_t MAX_VERTICES    = 10'000'000;
    constexpr uint32_t MAX_SUBMESHES   = 256;
    constexpr uint32_t MAX_BONES       = 512;

    if (hdr.submesh_count > MAX_SUBMESHES)      return E_WINTERS_SIZE_OVERFLOW;
    if (hdr.bone_count > MAX_BONES)             return E_WINTERS_SIZE_OVERFLOW;
    if (hdr.total_vertex_count > MAX_VERTICES)  return E_WINTERS_SIZE_OVERFLOW;

    // Stride 재계산 검증
    uint32_t expectedStride = 0;
    if (hdr.vertex_format_flags & VF_Position)   expectedStride += 12;
    if (hdr.vertex_format_flags & VF_Normal)     expectedStride += 12;
    if (hdr.vertex_format_flags & VF_UV0)        expectedStride += 8;
    if (hdr.vertex_format_flags & VF_Tangent)    expectedStride += 16;
    if (hdr.vertex_format_flags & VF_BoneWeight) expectedStride += 32;
    if (expectedStride != hdr.vertex_stride)    return E_WINTERS_SIZE_OVERFLOW;

    return S_OK;
}
```

---

## 11. 완료 기준

- [ ] `WMeshFormat.h` POD + static_assert 전부
- [ ] `WMeshWriter.cpp` Assimp 변환 + 전치 + bone 수집
- [ ] `WMeshLoader.cpp` mmap zero-copy 로드 + Validator
- [ ] 챔피언 5체 변환 스크립트 + 실행 확인
- [ ] Profiler 로 로드 시간 < 1 ms 검증
- [ ] GoogleTest 5개 케이스
- [ ] CLAUDE.md Gotcha 전치 / stride / GlobalInverseRoot 박제 확인

---

## 12. 다음 단계

Stage 3 (`.wanim` + `.wskel`) 로 이동 — 본 계층 + 애니메이션 키프레임 분리 저장. `.wmesh` 의 bone table 은 offset matrix 만, skeleton hierarchy 는 `.wskel` 로.
