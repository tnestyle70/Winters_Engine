# Stage 3 — `.wanim` 애니메이션 + `.wskel` 스켈레톤 (분리 저장)

> **목표**: Assimp 의 `aiAnimation` / `aiNodeAnim` / `aiNode` 를 런타임 의존 없이 직접 로드.
> 스켈레톤 계층은 한 번 로드 후 여러 애니가 공유 → `.wskel` 단일 + `.wanim` N 개 참조 구조.

> **배경 (CLAUDE.md Gotcha)**:
> - 채널 없는 본은 Identity 가 아닌 Rest Pose (aiNode::mTransformation) 로 초기화 필수
> - GlobalInverseRoot 빠지면 루트 노드 트랜스폼만큼 전체 메시 틀어짐
> - Final = Offset(`.wmesh`) × GlobalTransform(`.wskel` 조합) × GlobalInverseRoot(`.wskel`)

---

## 1. 왜 분리

| 이유 | 이점 |
|---|---|
| **스킨 공유** — 같은 챔피언의 모든 스킨이 공통 본 구조 | `.wskel` 1 개를 스킨 N 개가 참조 |
| **애니 공유** — 이동/공격은 스킨 무관 | `.wanim` 도 스킨 간 공유 |
| **빌드 시간** — 메시만 바뀌면 `.wmesh` 만 재빌드 | 애니 재변환 불필요 |
| **번들 효율** — 중복 제거 용이 | `.winters` 번들 TOC 에서 같은 wskel 해시면 1 저장소 |

---

## 2. `.wskel` 포맷

### 2.1 레이아웃

```
[ WintersFileHeader 16B ]  flags=LZ4(옵션)
[ Payload ]
    SkelMetaHeader               (32 B)
    BoneNode[]                   (bone_count × 256 B)
        - name[64]
        - name_hash
        - parent_index
        - rest_transform[16]     (DX row-major 전치 상태)
        - global_inverse_root[16] (전역 공유, 중복 저장 생략 → 대신 별도 블록)
    GlobalInverseRootMatrix (64 B)  ★ 한 번만
    SocketTable[] (옵션)           (socket_count × 128 B) — Phase C-3 연계
[ SHA256 32B ]
```

### 2.2 POD

```cpp
// Engine/Public/AssetFormat/Anim/WSkelFormat.h
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include <cstdint>

namespace Winters::Asset
{
    constexpr char WSKEL_MAGIC[4] = { 'W','S','K','L' };

    #pragma pack(push, 1)
    struct SkelMetaHeader
    {
        char     magic[4];           // "WSKL"
        uint32_t bone_count;
        uint32_t socket_count;       // 0 = no socket
        uint32_t reserved[5];
    };
    static_assert(sizeof(SkelMetaHeader) == 32);

    struct BoneNode
    {
        uint64_t name_hash;                // FNV-1a
        char     name[64];                 // 디버그용
        int32_t  parent_index;             // -1 = root
        float    rest_transform[16];       // aiNode::mTransformation × transpose
        uint32_t child_count;              // Forward 참조 (iteration 최적화)
        uint32_t first_child_index;        // bone 배열 내 첫 자식 인덱스
        uint32_t reserved[20];             // 추후 IK/제약조건
    };
    static_assert(sizeof(BoneNode) == 256);

    struct GlobalRootMatrix
    {
        float global_inverse_root[16];
        uint32_t reserved[16];             // 64 B 맞춤
    };
    static_assert(sizeof(GlobalRootMatrix) == 128);

    // Phase C-3 Socket — 무기/이펙트 부착 지점
    struct SocketEntry
    {
        char     name[32];             // "Weapon_R", "Spine_FX"
        uint64_t name_hash;
        int32_t  parent_bone_index;
        float    local_offset[16];     // bone-local 에서 socket 위치
        uint32_t reserved[3];
    };
    static_assert(sizeof(SocketEntry) == 128);
    #pragma pack(pop)
}
```

### 2.3 컨버터

```cpp
// Engine/Private/AssetFormat/Anim/WSkelWriter.cpp
HRESULT CWSkelWriter::WriteFromAssimp(const aiScene* scene, const wchar_t* outPath)
{
    // 1. 모든 aiNode 순회 — 본으로 참조되는 노드만 수집
    std::vector<BoneNode> bones;
    std::unordered_map<std::string, uint32_t> nameToIdx;
    CollectBoneNodesRecursive(scene->mRootNode, -1, nameToIdx, bones);

    // 2. parent_index / child_index 설정
    FillHierarchy(bones);

    // 3. rest_transform 은 aiNode::mTransformation 전치
    for (uint32_t i = 0; i < bones.size(); ++i) {
        auto aiTx = GetAiNodeByName(scene, bones[i].name)->mTransformation;
        XMMATRIX m = ConvertAndTranspose(aiTx);
        std::memcpy(bones[i].rest_transform, &m, sizeof(float) * 16);
    }

    // 4. GlobalInverseRoot
    XMMATRIX rootGlobal = ConvertAndTranspose(scene->mRootNode->mTransformation);
    XMMATRIX rootInv    = XMMatrixInverse(nullptr, rootGlobal);
    GlobalRootMatrix grm{};
    std::memcpy(grm.global_inverse_root, &rootInv, sizeof(float) * 16);

    // 5. Socket (선택) — aiNode 이름이 "Socket_*" / "Attach_*" 인 자식 수집
    auto sockets = ExtractSockets(scene, nameToIdx);

    // 6. 직렬화
    CBinaryWriter w;
    SkelMetaHeader hdr{};
    std::memcpy(hdr.magic, WSKEL_MAGIC, 4);
    hdr.bone_count   = (uint32_t)bones.size();
    hdr.socket_count = (uint32_t)sockets.size();
    w.Write(hdr);
    for (const auto& b : bones)   w.Write(b);
    w.Write(grm);
    for (const auto& s : sockets) w.Write(s);

    return w.SaveToFile(outPath, WINTERS_FLAG_LZ4);
}
```

---

## 3. `.wanim` 포맷

### 3.1 레이아웃

```
[ WintersFileHeader 16B ]
[ Payload ]
    AnimMetaHeader                  (40 B)
    uint64_t skel_hash              ★ 연결된 .wskel FNV-1a (호환성 체크)
    AnimChannel[]                   (channel_count × 32 B)
        - bone_name_hash
        - bone_index                (skel 내 인덱스 — 런타임 캐시)
        - pos_key_count, pos_offset
        - rot_key_count, rot_offset
        - scl_key_count, scl_offset
    KeyframeData                    (position / rotation / scale 블록)
    AnimationEventTable (옵션)      (event_count × 32 B)
[ SHA256 32B ]
```

### 3.2 POD

```cpp
// Engine/Public/AssetFormat/Anim/WAnimFormat.h
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include <cstdint>

namespace Winters::Asset
{
    constexpr char WANIM_MAGIC[4] = { 'W','A','N','M' };

    // 애니메이션 이벤트 타입 (CLAUDE.md Socket + Hitbox 연계)
    enum class eAnimEventType : uint16_t
    {
        HitStart       = 0,   // Hitbox active = true
        HitEnd         = 1,   // Hitbox active = false
        Footstep       = 2,
        SFX            = 3,
        VfxSpawn       = 4,   // FxSpawnSystem 트리거 (Phase G)
        DamageNumber   = 5,
        SkillCast      = 6,
        SkillRelease   = 7,
        Custom         = 0xFFFF,
    };

    #pragma pack(push, 1)
    struct AnimMetaHeader
    {
        char     magic[4];           // "WANM"
        uint32_t channel_count;
        float    duration_sec;       // 전체 길이
        float    ticks_per_second;   // 프레임 속도 (Assimp mTicksPerSecond)
        uint32_t total_key_count;
        uint32_t event_count;        // 0 = no event
        uint8_t  is_loop;            // 1 = loop (idle/run), 0 = one-shot (attack)
        uint8_t  reserved[7];
    };
    static_assert(sizeof(AnimMetaHeader) == 32);

    struct AnimChannel
    {
        uint64_t bone_name_hash;     // FNV-1a
        uint32_t pos_key_count;
        uint32_t pos_offset;         // KeyframeData 내 byte offset
        uint32_t rot_key_count;
        uint32_t rot_offset;
        uint32_t scl_key_count;
        uint32_t scl_offset;
        int32_t  bone_index_cached;  // -1 = 런타임 resolve 필요
    };
    static_assert(sizeof(AnimChannel) == 40);

    struct VectorKey
    {
        float time;      // 초 단위
        float x, y, z;
    };
    static_assert(sizeof(VectorKey) == 16);

    struct QuatKey
    {
        float time;
        float x, y, z, w;
    };
    static_assert(sizeof(QuatKey) == 20);

    struct AnimEvent
    {
        float    time;                // 초
        uint16_t type;                // eAnimEventType
        uint16_t reserved0;
        uint32_t skill_id;            // eAnimEventType::HitStart 등에서 사용
        uint32_t param_u32;           // payload 1
        float    param_f32;           // payload 2
        uint64_t string_hash;         // sound key / fx name FNV-1a
    };
    static_assert(sizeof(AnimEvent) == 32);

    struct WAnimTrailer
    {
        uint64_t skel_hash;           // .wskel 파일 매칭 검증
    };
    #pragma pack(pop)
}
```

### 3.3 런타임 애니메이션 계산

```cpp
// Engine/Private/Resource/Animator.cpp
void CAnimator::EvaluatePose(const CSkeleton* pSkel, const CAnimation* pAnim,
                              float fTime, std::vector<XMMATRIX>& outBoneFinal)
{
    const auto& bones = pSkel->GetBones();
    std::vector<XMMATRIX> localTx(bones.size());

    // 1. 채널 있는 본은 애니 샘플
    for (uint32_t c = 0; c < pAnim->GetChannelCount(); ++c) {
        const auto& ch = pAnim->GetChannel(c);
        int idx = ch.bone_index_cached;
        if (idx < 0) continue;

        XMVECTOR pos = SamplePosition(pAnim, ch, fTime);
        XMVECTOR rot = SampleRotation(pAnim, ch, fTime);   // Slerp
        XMVECTOR scl = SampleScale(pAnim, ch, fTime);

        localTx[idx] = XMMatrixScalingFromVector(scl) *
                       XMMatrixRotationQuaternion(rot) *
                       XMMatrixTranslationFromVector(pos);
    }

    // 2. 채널 없는 본은 Rest Pose — CLAUDE.md gotcha
    for (uint32_t i = 0; i < bones.size(); ++i) {
        if (!HasChannel(pAnim, i)) {
            std::memcpy(&localTx[i], bones[i].rest_transform, sizeof(XMMATRIX));
        }
    }

    // 3. 누적 — GlobalTransform[i] = LocalTx[i] * GlobalTransform[parent]
    std::vector<XMMATRIX> globalTx(bones.size());
    for (uint32_t i = 0; i < bones.size(); ++i) {
        int p = bones[i].parent_index;
        globalTx[i] = (p < 0) ? localTx[i]
                              : localTx[i] * globalTx[p];
    }

    // 4. Final = Offset(.wmesh) × Global × GlobalInverseRoot(.wskel)
    //    CLAUDE.md gotcha 공식 박제
    XMMATRIX rootInv;
    std::memcpy(&rootInv, pSkel->GetGlobalInverseRoot(), sizeof(XMMATRIX));

    for (uint32_t i = 0; i < bones.size(); ++i) {
        XMMATRIX offset;
        std::memcpy(&offset, bones[i].offset_matrix_from_mesh, sizeof(XMMATRIX));
        outBoneFinal[i] = offset * globalTx[i] * rootInv;
    }
}
```

**주의**: `offset_matrix_from_mesh` 는 **`.wmesh` BoneEntry 에서** 가져와야 함. `.wskel` 은 rest_transform (애니 없을 때 기본 포즈) 만 가짐.

### 3.4 스켈/메시 본 인덱스 매칭

```cpp
// 로드 시 1회만 — wanim 의 bone_index_cached 결정
void CAnimation::ResolveBoneIndices(const CSkeleton* pSkel)
{
    for (auto& ch : m_channels) {
        ch.bone_index_cached = pSkel->FindBoneIndexByHash(ch.bone_name_hash);
        // 못 찾으면 -1 유지 → EvaluatePose 에서 skip
    }
}
```

---

## 4. AnimationEvent — Phase C-3 Socket 연계

CLAUDE.md "Socket + Hitbox/Hurtbox + AnimationEvent" 설계를 `.wanim` 에 박제:

### 4.1 Q 스킬 — 이렐리아 예시

```
Irelia/Q_Attack.wanim
  duration: 0.6s
  events:
    [0.15s] SkillCast       skill_id=Q, string_hash=FNV("Irelia/Q_swoosh.wav")
    [0.25s] HitStart        skill_id=Q
    [0.35s] HitEnd          skill_id=Q
    [0.40s] VfxSpawn        skill_id=Q, string_hash=FNV("FX/Irelia/Q_Hit.fxg")
    [0.50s] DamageNumber
```

### 4.2 런타임 dispatch

```cpp
// Engine/Private/Resource/Animator.cpp
void CAnimator::DispatchEvents(float fPrevTime, float fCurTime)
{
    for (const auto& ev : m_pAnim->GetEvents()) {
        if (ev.time >= fPrevTime && ev.time < fCurTime) {
            switch ((eAnimEventType)ev.type) {
                case eAnimEventType::HitStart:
                    m_pHookManager->OnSkillHitStart(ev.skill_id);
                    break;
                case eAnimEventType::SFX:
                    CGameInstance::Get()->PlayEffect(ResolveSoundKey(ev.string_hash));
                    break;
                case eAnimEventType::VfxSpawn:
                    CGameInstance::Get()->SpawnFx(ResolveFxKey(ev.string_hash),
                                                   GetWorldPos(), GetForward());
                    break;
                // ...
            }
        }
    }
}
```

---

## 5. 컨버터 - `.wanim` 생성

```cpp
// Engine/Private/AssetFormat/Anim/WAnimWriter.cpp
HRESULT CWAnimWriter::WriteFromAssimp(const aiAnimation* pAnim, const aiScene* pScene,
                                       uint64_t skelHash, const wchar_t* pOutPath)
{
    AnimMetaHeader hdr{};
    std::memcpy(hdr.magic, WANIM_MAGIC, 4);
    hdr.channel_count    = pAnim->mNumChannels;
    hdr.duration_sec     = (float)(pAnim->mDuration / pAnim->mTicksPerSecond);
    hdr.ticks_per_second = (float)pAnim->mTicksPerSecond;
    hdr.is_loop          = IsLoopAnim(pAnim->mName.C_Str()) ? 1 : 0;

    std::vector<AnimChannel> channels;
    std::vector<uint8_t>     keyBlock;

    for (uint32_t c = 0; c < pAnim->mNumChannels; ++c) {
        const aiNodeAnim* node = pAnim->mChannels[c];
        AnimChannel ch{};
        ch.bone_name_hash    = FNV1a(node->mNodeName.C_Str());
        ch.bone_index_cached = -1;   // 런타임에서 resolve

        ch.pos_offset = (uint32_t)keyBlock.size();
        ch.pos_key_count = node->mNumPositionKeys;
        for (uint32_t k = 0; k < node->mNumPositionKeys; ++k) {
            const auto& pk = node->mPositionKeys[k];
            VectorKey vk{ (float)(pk.mTime / pAnim->mTicksPerSecond),
                          pk.mValue.x, pk.mValue.y, pk.mValue.z };
            AppendAs(keyBlock, vk);
        }

        ch.rot_offset = (uint32_t)keyBlock.size();
        ch.rot_key_count = node->mNumRotationKeys;
        for (uint32_t k = 0; k < node->mNumRotationKeys; ++k) {
            const auto& rk = node->mRotationKeys[k];
            QuatKey qk{ (float)(rk.mTime / pAnim->mTicksPerSecond),
                        rk.mValue.x, rk.mValue.y, rk.mValue.z, rk.mValue.w };
            AppendAs(keyBlock, qk);
        }

        ch.scl_offset = (uint32_t)keyBlock.size();
        ch.scl_key_count = node->mNumScalingKeys;
        for (uint32_t k = 0; k < node->mNumScalingKeys; ++k) {
            const auto& sk = node->mScalingKeys[k];
            VectorKey vk{ (float)(sk.mTime / pAnim->mTicksPerSecond),
                          sk.mValue.x, sk.mValue.y, sk.mValue.z };
            AppendAs(keyBlock, vk);
        }

        channels.push_back(ch);
    }

    // total_key_count 재계산
    hdr.total_key_count = (uint32_t)(keyBlock.size() / sizeof(VectorKey));  // 근사

    // 이벤트 — JSON 사이드카 또는 Assimp aiAnimation::mMetaData 에서 읽기
    auto events = LoadEventsSidecar(pAnim->mName.C_Str());
    hdr.event_count = (uint32_t)events.size();

    // 직렬화
    CBinaryWriter w;
    w.Write(hdr);
    for (const auto& ch : channels) w.Write(ch);
    w.WriteBytes(keyBlock.data(), keyBlock.size());
    for (const auto& ev : events) w.Write(ev);

    // skel_hash trailer (파일 끝에 한 번 — 헤더 뒤 trailer)
    WAnimTrailer tr{ skelHash };
    w.Write(tr);

    return w.SaveToFile(pOutPath, WINTERS_FLAG_LZ4);
}
```

---

## 6. 런타임 로더

```cpp
// Engine/Public/AssetFormat/Anim/WAnimLoader.h
namespace Winters::Asset
{
    class WINTERS_API CWAnimLoader
    {
    public:
        static std::shared_ptr<CAnimation> Load(const std::wstring& path,
                                                  uint64_t expectedSkelHash);
    };

    class WINTERS_API CWSkelLoader
    {
    public:
        static std::shared_ptr<CSkeleton> Load(const std::wstring& path);
    };
}
```

**핵심**: `.wanim` 로드 시 `expectedSkelHash` 검증 → 다른 챔피언 스켈에 재생하는 실수 방지.

---

## 7. ResourceCache 확장

```cpp
// Engine/Public/Resource/ResourceCache.h
class CResourceCache
{
    std::unordered_map<std::wstring, std::weak_ptr<CSkeleton>>   m_skeletons;
    std::unordered_map<std::wstring, std::weak_ptr<CAnimation>>  m_animations;

public:
    std::shared_ptr<CSkeleton>  LoadSkeleton(const std::wstring& path);

    // 스켈과 묶어서 로드 — 해시 불일치 시 E_WINTERS_HASH_MISMATCH
    std::shared_ptr<CAnimation> LoadAnimation(const std::wstring& path,
                                                const CSkeleton* pSkel);
};
```

---

## 8. 챔피언 에셋 구조 (최종)

```
Bin/Resource/Characters/Irelia/
├── body.wmesh            ← Stage 2 (오프셋 포함, 본 rest_transform 없음)
├── skeleton.wskel        ← Stage 3 (rest_transform, GlobalInverseRoot)
├── anims/
│   ├── idle.wanim
│   ├── run.wanim
│   ├── attack1.wanim    (+ events.json sidecar)
│   ├── attack2.wanim
│   ├── q_cast.wanim
│   ├── w_cast.wanim
│   ├── e_cast.wanim
│   ├── r_cast.wanim
│   └── death.wanim
├── body.wmat             ← Stage 5
└── textures/
    ├── body_diffuse.wtex ← Stage 4
    └── body_normal.wtex
```

---

## 9. 이벤트 사이드카 JSON (옵션)

컨버터가 Assimp 메타데이터에서 이벤트 못 찾으면 JSON 에서 보완:

```json
// Bin/Resource/Characters/Irelia/anims/attack1.events.json
{
  "events": [
    { "time": 0.15, "type": "SkillCast",  "sound": "Irelia/Q_swoosh.wav" },
    { "time": 0.25, "type": "HitStart",   "skill_id": 1 },
    { "time": 0.35, "type": "HitEnd",     "skill_id": 1 },
    { "time": 0.40, "type": "VfxSpawn",   "fx_path": "FX/Irelia/Q_Hit.fxg" },
    { "time": 0.50, "type": "DamageNumber" }
  ]
}
```

**이유**: FBX 에 이벤트 트랙 박아 넣기 어려움. JSON 사이드카가 아티스트/프로그래머 협업 접점.

컨버터는 `input.fbx` 옆 `input.events.json` 있으면 자동 병합.

---

## 10. 성능 검증

| 에셋 | Assimp | `.wanim` | 목표 |
|---|---|---|---|
| Irelia skeleton (50 bones) | 30 ms | ? | < 0.3 ms |
| Irelia idle.wanim (90 keyframe) | 10 ms | ? | < 0.2 ms |
| Irelia 68 anim 전부 | 1200 ms | ? | < 15 ms 전체 |
| EvaluatePose (50 bones, fTime=2.5) | — | — | < 0.1 ms / call |

---

## 11. 보안 고려사항

| 위협 | 방어 |
|---|---|
| 치트가 `.wanim` 이벤트 time 조작 (HitStart 를 애니 시작으로 당김 → 선딜 없음) | SHA256 검증 + 서버가 권위 (hitbox 판정은 서버 시간 기준) |
| 본 parent_index 악의 조작 | 로더에서 `parent < bone_count` 검증 |
| bone_name_hash 충돌 공격 (다른 본에 같은 해시 할당) | name 문자열도 저장 → 충돌 시 로그 + 거부 |
| 키프레임 count 거대값 (OOM) | `total_key_count > 100'000` 거부 |

---

## 12. 테스트 체크리스트

- [ ] Irelia skeleton.wskel 생성 + 본 수 / parent 검증
- [ ] idle.wanim 생성 + duration / channel 확인
- [ ] 런타임 로드 후 EvaluatePose → 본 행렬 기댓값 일치
- [ ] 기존 Assimp 파이프라인과 렌더 결과 동일 (visual diff)
- [ ] 이벤트 dispatch 동작 (HitStart/End, SFX, VfxSpawn)
- [ ] Skel/Anim 해시 mismatch 시 로드 거부
- [ ] 채널 없는 본 Rest Pose 적용 확인
- [ ] GlobalInverseRoot 적용 확인 (없으면 메시 틀어짐)

---

## 13. 완료 기준

- [ ] `WSkelFormat.h` / `WAnimFormat.h` POD + static_assert
- [ ] `WSkelWriter.cpp` / `WSkelLoader.cpp`
- [ ] `WAnimWriter.cpp` / `WAnimLoader.cpp`
- [ ] `CAnimator::EvaluatePose` CLAUDE.md 공식 박제
- [ ] 이벤트 sidecar JSON 파서
- [ ] 챔피언 5체 전체 애니 변환 (총 ~300 개)
- [ ] Profiler 로 로드 시간 검증

---

## 14. 다음 단계

Stage 4 (`.wtex`) 로 이동 — DirectXTex BC7 인코딩 + 밉맵 생성 + GPU 업로드.
