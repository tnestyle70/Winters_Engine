# Phase B-13 / 02 — 시야 시스템 + Fog of War + 부쉬 (v2)

**버전**: v2 (2026-05-04 — Codex 검토 #2 P-9/P-15 + DLL 경계 + render API 정정)
**가이드**: [`.md/process/PLAN_AUTHORING_PITFALLS.md`](../../process/PLAN_AUTHORING_PITFALLS.md)
**선행**: 04 SPATIAL HASH v2 (CWorld owned `CSpatialIndex`).
**총 LOC**: ~1500.

---

## §0. v1 → v2 정정 매트릭스

| # | v1 결함 | v2 정정 | PITFALLS |
|---|---|---|---|
| 1 | phase = 4 (StatusEffectSystem 과 충돌) | **phase = 5 (단독)** | P-9 |
| 2 | `VisionComponents.h` 의 `WardComponent::ownerTeam = eTeam` — `GameplayComponents.h` 미include | **`#include "ECS/Components/GameplayComponents.h"`** 명시 추가. 또는 `ownerTeam: uint8_t` 로 (eTeam cast). | P-15 |
| 3 | `FogOfWarRenderer.h` 가 `<d3d11.h>` + `Microsoft::WRL::ComPtr<ID3D11Texture2D>` 직접 노출 — RHI 경계 위반 | `IRHIDevice*` + opaque `RHITextureHandle` (이미 `RHI/RHITypes.h` 박제됨) 사용. `<d3d11.h>` 는 .cpp 안. ImGui 용 SRV 노출은 `void* Get_NativeSRV()` (Tier-2). | DLL 경계 |
| 4 | Scene render 패턴: `rc.pRenderer->Render(xf.GetWorldMatrix())` 가정 | 실제 [ModelRenderer](../../../Engine/Public/Renderer/ModelRenderer.h) 는 `UpdateTransform(matWorld)` + `UpdateCamera(vp)` + `Render()` **분리**. 가시성 분기는 `RenderWithVisibility(...)` (B-16 v2 박제 — 여기 의존). main pass + normal pass 둘 다 박제. | P-2 변형 |
| 5 | `CSpatialIndex* spatial = pGI->Get_SpatialIndex()` 호출 — Tier-2 게터 폐기됨 | `CSpatialIndex* spatial = world.Get_SpatialIndex()` (04 v2 §4 CWorld owned) | P-10 |
| 6 | 공개 헤더 flat include | subdir 보존 (`#include "ECS/Components/..."` 등) | P-8 |

**v2 진입 게이트**: 위 6 행 통과 후 §3+ 박제 진입.

---

## 0. 설계 의사결정

### 시야 갱신 주기
- **100ms 틱** (10Hz) — LoL 본체와 동일 (FoW 텍스처 갱신).
- 매 frame X — CPU/GPU 비용 문제. 100ms 면 사용자 체감 충분 빠름.
- **즉시 dirty**: 부쉬 진입/이탈, 챔프 사망, 와드 만료 — 다음 frame 에 강제 rebuild.

### Fog of War 표현
- **CPU 측**: 256×256 R8 grayscale (총 64KB). map 280m × 280m → cell ≈ 1.1m.
- **GPU 측**: 같은 텍스처 → 미니맵 + (옵션) 메인 화면 PostFX.
- 3 stage: `0=Unseen` (검정), `127=PreviouslySeen` (회색), `255=CurrentlyVisible` (선명).

### 부쉬
- 맵 데이터 v5 (현재 v4) 에 `BushVolumeEntry` 추가.
- 정적 — `CBushVolumeIndex` 로 별도 관리 (CSpatialIndex 동적 인덱스와 분리, 04 sub-plan §11 C-7).
- 부쉬 안 엔티티 가시성: **부쉬 안에 같은 팀 unit 이 있어야** 외부에서 보임.

---

## 1. VisionComponent 박제

**신규 파일**: `Engine/Public/ECS/Components/VisionComponents.h`

```cpp
#pragma once
#include "Engine_Defines.h"
#include "WintersTypes.h"
#include "WintersMath.h"
#include "ECS/Entity.h"
#include "ECS/Components/GameplayComponents.h"   // ★ v2: eTeam 정의 보유 (P-15 회피)

// ─────────────────────────────────────────────────────────────
// VisionSourceComponent — 시야 발산 엔티티 (챔프/미니언/구조물/와드)
// ─────────────────────────────────────────────────────────────
struct VisionSourceComponent
{
    f32_t   sightRange = 12.f;       // 시야 반경 (m). LoL: 챔프 19m, 미니언 8m, 구조물 12m, 와드 18m
    bool_t  bTrueSight = false;      // 부쉬 안 적도 보임 (와드 일부)
    bool_t  bFlying    = false;      // 비행 시야 (지형 무시 — 미사용)
    f32_t   sightRangeInBush = 0.f;  // 부쉬 안일 때 별도 범위 (LoL 룰: 부쉬 안에서는 부쉬 자체만 보임)
};

// ─────────────────────────────────────────────────────────────
// VisibilityComponent — 가시성 결과 (CVisionSystem 매 100ms 갱신)
// 적 시각으로부터 자신이 보이는지 표시
// ─────────────────────────────────────────────────────────────
struct VisibilityComponent
{
    // 비트 플래그: bit 0 = Blue 팀에게 보임, bit 1 = Red 팀, bit 2 = Neutral
    uint8_t teamVisibilityMask = 0;
    bool_t  bInBush = false;        // 현재 부쉬 안에 있는지
    EntityID bushId = NULL_ENTITY;  // 들어있는 부쉬 엔티티 ID (BushVolumeComponent 가진)
};

// ─────────────────────────────────────────────────────────────
// BushVolumeComponent — 부쉬 영역 정의 (맵 정적 데이터)
// 원형 또는 다각형. v5 에서는 원형만, 다각형은 v6.
// ─────────────────────────────────────────────────────────────
struct BushVolumeComponent
{
    Vec3   center{};
    f32_t  radius = 4.f;            // LoL 부쉬 평균 4~6m
    uint32_t bushId = 0;            // 동일 부쉬 군집 묶음 (분리된 부쉬 셀 → 같은 시야)
};

// 와드 — VisionSource 의 특수 케이스
struct WardComponent
{
    f32_t  remainingDuration = 90.f;  // 90초 만료
    eTeam  ownerTeam = eTeam::Blue;
    bool_t bControlWard = false;      // 시야와드 vs 핑크와드 (true sight)
};

// LocalPlayerVisionTag — 로컬 플레이어 팀 시야 표시 (FoW 텍스처 업데이트 대상)
struct LocalPlayerVisionTag {};
```

---

## 2. CVisionSystem 본 박제

**신규 파일**: `Engine/Public/ECS/Systems/VisionSystem.h`

```cpp
#pragma once
#include "Engine_Defines.h"
#include "WintersAPI.h"
#include "WintersMath.h"
#include "ECS/ISystem.h"
#include "ECS/Entity.h"
#include <memory>
#include <vector>
#include <cstdint>

class CWorld;
class CSpatialIndex;
class CBushVolumeIndex;

NS_BEGIN(Engine)

class WINTERS_ENGINE CVisionSystem final : public ISystem
{
public:
    static constexpr u32_t FOW_TEX_DIM = 256;
    static constexpr f32_t FOW_TEX_WORLD_SIZE = 280.f; // 맵 280×280m

    ~CVisionSystem() override = default;

    static std::unique_ptr<CVisionSystem> Create(CSpatialIndex* pIndex,
                                                  CBushVolumeIndex* pBushIndex);

    uint32_t    GetPhase() const override { return 5; }   // ★ v2: 정수 phase=5 (StatusEffect=4 후, Turret=6 전)
    const char* GetName()  const override { return "VisionSystem"; }
    void        Execute(CWorld& world, f32_t fTimeDelta) override;

    // 외부 강제 dirty (부쉬 진입/이탈, 챔프 사망 등).
    void ForceRebuildNextFrame() { m_bForceRebuild = true; }

    // FoW 텍스처 데이터 read-only 접근. Fog Renderer 가 ImGui 미니맵에 그릴 때 사용.
    const uint8_t* GetFowTextureData() const { return m_FowTexture.data(); }
    uint32_t       GetFowTextureDim()  const { return FOW_TEX_DIM; }
    bool           IsFowTextureDirty() const { return m_bFowTextureDirty; }
    void           ClearFowTextureDirty()    { m_bFowTextureDirty = false; }

    // 디버그 — 어떤 챔프가 어떤 적에게 보이는지 시각화
    struct VisRecord {
        EntityID source;
        EntityID target;
        f32_t    distance;
    };
    const std::vector<VisRecord>& GetDebugRecords() const { return m_vecDebugRecords; }

private:
    CVisionSystem() = default;

    void TickVisibility(CWorld& world);
    void UpdateBushOccupancy(CWorld& world);
    void UpdateFowTexture(CWorld& world);

    // 부쉬 시야 룰: source 가 부쉬 X 면, target 이 부쉬 안일 때 보이지 않음
    //               (단, source 가 같은 부쉬 안에 있으면 보임 — 부쉬 안 시야).
    bool IsTargetVisible(CWorld& world, EntityID source, EntityID target,
                         f32_t sightRange) const;

    CSpatialIndex*    m_pIndex = nullptr;
    CBushVolumeIndex* m_pBushIndex = nullptr;

    f32_t m_fAccumDt = 0.f;
    static constexpr f32_t TICK_INTERVAL = 0.1f;   // 100ms
    bool  m_bForceRebuild = false;

    // FoW 텍스처 (CPU 측). 0=Unseen, 127=PreviouslySeen, 255=CurrentlyVisible
    std::vector<uint8_t> m_FowTexture;
    bool                 m_bFowTextureDirty = false;

    std::vector<VisRecord> m_vecDebugRecords;
};

NS_END
```

**신규 파일**: `Engine/Private/ECS/Systems/VisionSystem.cpp`

```cpp
#include "WintersPCH.h"
#include "ECS/Systems/VisionSystem.h"
#include "ECS/World.h"
#include "ECS/SpatialIndex.h"
#include "ECS/BushVolumeIndex.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/VisionComponents.h"
#include "ECS/Components/GameplayComponents.h"
#include "ProfilerAPI.h"

#include <cstring>
#include <algorithm>
#include <cmath>

NS_BEGIN(Engine)

std::unique_ptr<CVisionSystem> CVisionSystem::Create(CSpatialIndex* pIndex,
                                                      CBushVolumeIndex* pBushIndex)
{
    auto p = std::unique_ptr<CVisionSystem>(new CVisionSystem());
    p->m_pIndex = pIndex;
    p->m_pBushIndex = pBushIndex;
    p->m_FowTexture.assign(FOW_TEX_DIM * FOW_TEX_DIM, 0);
    return p;
}

void CVisionSystem::Execute(CWorld& world, f32_t dt)
{
    WINTERS_PROFILE_SCOPE("Vision::Execute");
    m_fAccumDt += dt;

    if (!m_bForceRebuild && m_fAccumDt < TICK_INTERVAL)
        return;

    m_fAccumDt = 0.f;
    m_bForceRebuild = false;

    {
        WINTERS_PROFILE_SCOPE("Vision::BushOccupancy");
        UpdateBushOccupancy(world);
    }
    {
        WINTERS_PROFILE_SCOPE("Vision::TickVisibility");
        TickVisibility(world);
    }
    {
        WINTERS_PROFILE_SCOPE("Vision::UpdateFow");
        UpdateFowTexture(world);
    }
}

void CVisionSystem::UpdateBushOccupancy(CWorld& world)
{
    if (!m_pBushIndex) return;

    world.ForEach<TransformComponent, VisibilityComponent, SpatialAgentComponent>(
        function<void(EntityID, TransformComponent&, VisibilityComponent&, SpatialAgentComponent&)>(
            [&](EntityID id, TransformComponent& xf, VisibilityComponent& vis, SpatialAgentComponent&)
            {
                const Vec3 pos = xf.GetPosition();
                EntityID prevBush = vis.bushId;
                EntityID nowBush = m_pBushIndex->QueryBushAt(pos);

                vis.bInBush = (nowBush != NULL_ENTITY);
                vis.bushId  = nowBush;

                if (prevBush != nowBush)
                {
                    // 부쉬 진입/이탈 — 다음 frame 강제 rebuild
                    m_bForceRebuild = true;
                }
            }));
}

void CVisionSystem::TickVisibility(CWorld& world)
{
    m_vecDebugRecords.clear();

    // 모든 unit 의 visibilityMask 초기화
    world.ForEach<VisibilityComponent>(
        function<void(EntityID, VisibilityComponent&)>(
            [&](EntityID id, VisibilityComponent& v)
            {
                v.teamVisibilityMask = 0;
            }));

    // 모든 시야원으로부터 가시 unit 마킹
    world.ForEach<TransformComponent, VisionSourceComponent, SpatialAgentComponent>(
        function<void(EntityID, TransformComponent&, VisionSourceComponent&, SpatialAgentComponent&)>(
            [&](EntityID srcId, TransformComponent& srcXf, VisionSourceComponent& vs, SpatialAgentComponent& srcAgent)
            {
                const Vec3 srcPos = srcXf.GetPosition();
                const uint8_t srcTeam = srcAgent.team;

                std::vector<EntityID> candidates;
                if (m_pIndex)
                {
                    const uint32_t mask = SpatialMask(eSpatialKind::Champion)
                                        | SpatialMask(eSpatialKind::Minion)
                                        | SpatialMask(eSpatialKind::Turret)
                                        | SpatialMask(eSpatialKind::JungleMob)
                                        | SpatialMask(eSpatialKind::Inhibitor)
                                        | SpatialMask(eSpatialKind::Nexus)
                                        | SpatialMask(eSpatialKind::Ward);
                    m_pIndex->QueryRadius(srcPos, vs.sightRange, mask, /*excludeTeamMask*/ 0, candidates);
                }

                for (EntityID tgt : candidates)
                {
                    if (tgt == srcId) continue;
                    if (!world.HasComponent<VisibilityComponent>(tgt)) continue;

                    if (IsTargetVisible(world, srcId, tgt, vs.sightRange))
                    {
                        auto& tgtVis = world.GetComponent<VisibilityComponent>(tgt);
                        tgtVis.teamVisibilityMask |= (1u << srcTeam);
                    }
                }
            }));
}

bool CVisionSystem::IsTargetVisible(CWorld& world, EntityID source, EntityID target,
                                    f32_t sightRange) const
{
    if (!world.HasComponent<TransformComponent>(source) ||
        !world.HasComponent<TransformComponent>(target))
        return false;

    const Vec3 srcPos = world.GetComponent<TransformComponent>(source).GetPosition();
    const Vec3 tgtPos = world.GetComponent<TransformComponent>(target).GetPosition();
    const f32_t dx = tgtPos.x - srcPos.x;
    const f32_t dz = tgtPos.z - srcPos.z;
    const f32_t d2 = dx * dx + dz * dz;
    if (d2 > sightRange * sightRange) return false;

    // 부쉬 룰
    if (world.HasComponent<VisibilityComponent>(target))
    {
        const auto& tgtVis = world.GetComponent<VisibilityComponent>(target);
        if (tgtVis.bInBush)
        {
            // 타겟이 부쉬 안 → source 가 같은 부쉬 안이거나 TrueSight 가져야 보임
            const auto& tgtBushId = tgtVis.bushId;
            if (world.HasComponent<VisibilityComponent>(source))
            {
                const auto& srcVis = world.GetComponent<VisibilityComponent>(source);
                if (srcVis.bInBush && srcVis.bushId == tgtBushId)
                    return true;   // 같은 부쉬 안
            }
            // TrueSight 검사
            if (world.HasComponent<VisionSourceComponent>(source))
            {
                if (world.GetComponent<VisionSourceComponent>(source).bTrueSight)
                    return true;
            }
            return false;   // 부쉬 시야 차단
        }
    }

    return true;
}

void CVisionSystem::UpdateFowTexture(CWorld& world)
{
    // 로컬 플레이어 팀 (보통 Blue) — 그 팀 시각으로 FoW 갱신
    uint8_t localTeam = 0;
    bool bLocalFound = false;
    world.ForEach<LocalPlayerTag, SpatialAgentComponent>(
        function<void(EntityID, LocalPlayerTag&, SpatialAgentComponent&)>(
            [&](EntityID, LocalPlayerTag&, SpatialAgentComponent& a)
            {
                localTeam = a.team;
                bLocalFound = true;
            }));
    if (!bLocalFound) return;

    // 1. 현재 가시 영역 마킹 (255)
    // 2. 이전 가시 → 회색 (127) 으로 감쇠
    for (auto& v : m_FowTexture)
    {
        if (v == 255) v = 127;   // 이번 갱신 전, 직전 visible → fade
    }

    // 모든 시야원에서 cell 단위 채우기
    constexpr f32_t cellWorld = FOW_TEX_WORLD_SIZE / FOW_TEX_DIM;
    constexpr f32_t halfWorld = FOW_TEX_WORLD_SIZE * 0.5f;

    world.ForEach<TransformComponent, VisionSourceComponent, SpatialAgentComponent>(
        function<void(EntityID, TransformComponent&, VisionSourceComponent&, SpatialAgentComponent&)>(
            [&](EntityID, TransformComponent& xf, VisionSourceComponent& vs, SpatialAgentComponent& a)
            {
                if (a.team != localTeam) return;   // 로컬 팀 시야만

                const Vec3 srcPos = xf.GetPosition();
                const f32_t r = vs.sightRange;
                const f32_t r2 = r * r;
                const int32_t cellRadius = static_cast<int32_t>(std::ceil(r / cellWorld));

                const int32_t cx = static_cast<int32_t>((srcPos.x + halfWorld) / cellWorld);
                const int32_t cz = static_cast<int32_t>((srcPos.z + halfWorld) / cellWorld);

                for (int32_t dz = -cellRadius; dz <= cellRadius; ++dz)
                {
                    for (int32_t dx = -cellRadius; dx <= cellRadius; ++dx)
                    {
                        const int32_t qx = cx + dx;
                        const int32_t qz = cz + dz;
                        if (qx < 0 || qx >= static_cast<int32_t>(FOW_TEX_DIM)) continue;
                        if (qz < 0 || qz >= static_cast<int32_t>(FOW_TEX_DIM)) continue;

                        const f32_t wx = (qx + 0.5f) * cellWorld - halfWorld;
                        const f32_t wz = (qz + 0.5f) * cellWorld - halfWorld;
                        const f32_t dx2 = wx - srcPos.x;
                        const f32_t dz2 = wz - srcPos.z;
                        if (dx2 * dx2 + dz2 * dz2 <= r2)
                        {
                            m_FowTexture[qz * FOW_TEX_DIM + qx] = 255;
                        }
                    }
                }
            }));

    m_bFowTextureDirty = true;
}

NS_END
```

---

## 3. CBushVolumeIndex 박제

**신규 파일**: `Engine/Public/ECS/BushVolumeIndex.h`

```cpp
#pragma once
#include "Engine_Defines.h"
#include "WintersAPI.h"
#include "WintersMath.h"
#include "Entity.h"
#include <vector>

class CWorld;

class WINTERS_ENGINE CBushVolumeIndex
{
public:
    CBushVolumeIndex() = default;
    ~CBushVolumeIndex() = default;

    CBushVolumeIndex(const CBushVolumeIndex&) = delete;
    CBushVolumeIndex& operator=(const CBushVolumeIndex&) = delete;

    void Build(CWorld& world);   // 1회 빌드 (정적). 맵 로드 후 호출.
    EntityID QueryBushAt(const Vec3& pos) const;   // pos 가 어느 부쉬 안인지 (없으면 NULL_ENTITY)
    void Clear() { m_vecBushes.clear(); }

private:
    struct BushEntry {
        EntityID id;
        Vec3     center;
        f32_t    radius;
        uint32_t bushId;
    };
    std::vector<BushEntry> m_vecBushes;
};
```

**신규 파일**: `Engine/Private/ECS/BushVolumeIndex.cpp`

```cpp
#include "WintersPCH.h"
#include "ECS/BushVolumeIndex.h"
#include "ECS/World.h"
#include "ECS/Components/VisionComponents.h"

void CBushVolumeIndex::Build(CWorld& world)
{
    m_vecBushes.clear();
    world.ForEach<BushVolumeComponent>(
        function<void(EntityID, BushVolumeComponent&)>(
            [&](EntityID id, BushVolumeComponent& bv)
            {
                BushEntry e{};
                e.id = id;
                e.center = bv.center;
                e.radius = bv.radius;
                e.bushId = bv.bushId;
                m_vecBushes.push_back(e);
            }));
}

EntityID CBushVolumeIndex::QueryBushAt(const Vec3& pos) const
{
    for (const auto& b : m_vecBushes)
    {
        const f32_t dx = pos.x - b.center.x;
        const f32_t dz = pos.z - b.center.z;
        if (dx * dx + dz * dz <= b.radius * b.radius)
            return b.id;
    }
    return NULL_ENTITY;
}
```

---

## 4. MapDataFormats v5 — BushVolumeEntry

**파일**: `Client/Public/Map/MapDataFormats.h`
**작업**: v5 추가.

### 수정 전 (L17-L18)
```cpp
constexpr u32_t STAGE_VERSION            = 4;
constexpr u32_t STAGE_VERSION_MIN_COMPAT = 3;
```

### 수정 후
```cpp
constexpr u32_t STAGE_VERSION            = 5;          // v5: BushVolumeEntry 추가
constexpr u32_t STAGE_VERSION_MIN_COMPAT = 3;
```

### 추가 (L96 직후 — MinionWaypointEntry 뒤)
```cpp
struct BushVolumeEntry
{
    f32_t  cx, cy, cz;       // 중심
    f32_t  radius;           // 반경
    u32_t  bushId;           // 부쉬 군집 묶음 ID (분리 부쉬셀 → 같은 시야)
    u32_t  reserved[2];
};
static_assert(sizeof(BushVolumeEntry) == 4 * sizeof(f32_t) + 3 * sizeof(u32_t),
    "BushVolumeEntry size fixed");
```

---

## 5. CFogOfWarRenderer — 미니맵 + 메인 화면 PostFX

**신규 파일**: `Engine/Public/Renderer/FogOfWarRenderer.h`

**★ v2 정정**: 공개 헤더 (`Engine/Public/Renderer/FogOfWarRenderer.h`) 에서 `<d3d11.h>` 직접 노출 금지. RHI/DX12 확장성 + Tier-2 경계 보존. DX11-specific 코드는 .cpp 안에 PIMPL 캡슐화.

```cpp
#pragma once
#include "Engine_Defines.h"
#include "WintersAPI.h"
#include "RHI/IRHIDevice.h"   // ★ v2: opaque RHI 핸들. <d3d11.h> 노출 X.
#include <memory>
#include <cstdint>

class WINTERS_ENGINE CFogOfWarRenderer
{
public:
    static std::unique_ptr<CFogOfWarRenderer> Create(IRHIDevice* pDevice, uint32_t dim);
    ~CFogOfWarRenderer();

    CFogOfWarRenderer(const CFogOfWarRenderer&) = delete;
    CFogOfWarRenderer& operator=(const CFogOfWarRenderer&) = delete;

    // CPU FoW 텍스처 → GPU 업로드 (D3D11_MAP_WRITE_DISCARD 내부 처리)
    void UpdateTexture(const uint8_t* pData, uint32_t dim);

    // 미니맵 ImGui 용 native SRV (Tier-2 — DX11 한정).
    // void* 반환 → ImGui::Image((ImTextureID) ... ) 캐스팅. CTexture::GetSRV 미공개 컨벤션과 동일.
    void* Get_NativeSRV() const;

private:
    CFogOfWarRenderer() = default;

    // PIMPL — DX11 ComPtr 은 .cpp 안에서만 (Engine/Public/Renderer 의 d3d11.h 노출 회피)
    struct Impl;
    std::unique_ptr<Impl> m_pImpl;
};
```

**v1 폐기 (참고)**: `CDX11Device*` + `<d3d11.h>` + `Microsoft::WRL::ComPtr<ID3D11Texture2D>` 박제는 RHI 경계 위반.

**신규 파일**: `Engine/Private/Renderer/FogOfWarRenderer.cpp`

```cpp
#include "WintersPCH.h"
#include "Renderer/FogOfWarRenderer.h"
#include "RHI/CDX11Device.h"
#include "RHI/RHITypes.h"
#include <wrl/client.h>
#include <d3d11.h>

namespace
{
    ID3D11Device* GetNativeDX11Device(IRHIDevice* pDevice)
    {
        if (!pDevice) return nullptr;
        return static_cast<ID3D11Device*>(pDevice->GetNativeHandle(eNativeHandleType::DX11Device));
    }
    ID3D11DeviceContext* GetNativeDX11Context(IRHIDevice* pDevice)
    {
        if (!pDevice) return nullptr;
        return static_cast<ID3D11DeviceContext*>(pDevice->GetNativeHandle(eNativeHandleType::DX11DeviceContext));
    }
}

struct CFogOfWarRenderer::Impl
{
    IRHIDevice*                                       pDevice = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>           pTex;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  pSRV;
    uint32_t                                          uDim = 0;
};

CFogOfWarRenderer::~CFogOfWarRenderer() = default;

std::unique_ptr<CFogOfWarRenderer> CFogOfWarRenderer::Create(IRHIDevice* pDevice, uint32_t dim)
{
    auto p = std::unique_ptr<CFogOfWarRenderer>(new CFogOfWarRenderer());
    p->m_pImpl = std::make_unique<Impl>();
    p->m_pImpl->pDevice = pDevice;
    p->m_pImpl->uDim = dim;

    ID3D11Device* pNative = GetNativeDX11Device(pDevice);
    if (!pNative) return nullptr;

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = dim;
    desc.Height = dim;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    HRESULT hr = pNative->CreateTexture2D(&desc, nullptr, p->m_pImpl->pTex.GetAddressOf());
    if (FAILED(hr)) return nullptr;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    hr = pNative->CreateShaderResourceView(p->m_pImpl->pTex.Get(), &srvDesc, p->m_pImpl->pSRV.GetAddressOf());
    if (FAILED(hr)) return nullptr;

    return p;
}

void CFogOfWarRenderer::UpdateTexture(const uint8_t* pData, uint32_t dim)
{
    if (!m_pImpl || dim != m_pImpl->uDim) return;
    ID3D11DeviceContext* pCtx = GetNativeDX11Context(m_pImpl->pDevice);
    if (!pCtx) return;

    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT hr = pCtx->Map(m_pImpl->pTex.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr)) return;

    uint8_t* dst = reinterpret_cast<uint8_t*>(mapped.pData);
    for (uint32_t y = 0; y < dim; ++y)
        memcpy(dst + y * mapped.RowPitch, pData + y * dim, dim);
    pCtx->Unmap(m_pImpl->pTex.Get(), 0);
}

void* CFogOfWarRenderer::Get_NativeSRV() const
{
    return m_pImpl ? static_cast<void*>(m_pImpl->pSRV.Get()) : nullptr;
}
}
```

---

## 6. Scene_InGame::OnRender 의 가시성 필터링

**파일**: `Client/Private/Scene/Scene_InGame.cpp`
**작업**: 챔프/미니언 렌더 직전 VisibilityComponent 검사 추가.

### 패턴 (★ v2 정정 — 실제 ModelRenderer API + main pass + normal pass 양쪽)

**v1 결함**: `rc.pRenderer->Render(xf.GetWorldMatrix())` 가정 — 실제 [ModelRenderer](../../../Engine/Public/Renderer/ModelRenderer.h:14) 는 `UpdateTransform(matWorld)` + `UpdateCamera(vp)` + `Render()` 분리. main pass + normal pass 별도 함수.

```cpp
// 기존 (실제 코드)
world.ForEach<RenderComponent, TransformComponent>(
    function<void(EntityID, RenderComponent&, TransformComponent&)>(
        [&](EntityID id, RenderComponent& rc, TransformComponent& xf)
        {
            if (!rc.bVisible || !rc.pRenderer) return;
            rc.pRenderer->UpdateTransform(xf.GetWorldMatrix());
            rc.pRenderer->UpdateCamera(vp);
            rc.pRenderer->Render();
        }));

// 수정 후 — 가시성 필터 추가 (RenderWithVisibility 는 B-16 v2 박제 의존)
world.ForEach<RenderComponent, TransformComponent>(
    function<void(EntityID, RenderComponent&, TransformComponent&)>(
        [&](EntityID id, RenderComponent& rc, TransformComponent& xf)
        {
            if (!rc.bVisible || !rc.pRenderer) return;
            rc.pRenderer->UpdateTransform(xf.GetWorldMatrix());
            rc.pRenderer->UpdateCamera(vp);

            // ★ B-13: 시야 가시성 필터 — Vision 컴포넌트 가진 enemy unit 만 검사
            if (world.HasComponent<VisibilityComponent>(id))
            {
                const auto& vis = world.GetComponent<VisibilityComponent>(id);
                const uint8_t myMask = (1u << localPlayerTeam);
                if (!(vis.teamVisibilityMask & myMask))
                {
                    if (world.HasComponent<SpatialAgentComponent>(id))
                    {
                        const auto& a = world.GetComponent<SpatialAgentComponent>(id);
                        if (a.team != localPlayerTeam)
                            return;   // 적 + 내 팀 시야 밖 → 렌더 X
                    }
                }
            }

            rc.pRenderer->Render();
        }));
```

**Normal pass 도 동일 패턴** ([ModelRenderer.cpp:288](../../../Engine/Private/Renderer/ModelRenderer.cpp:288) `RenderNormalPass`) — Scene_InGame 의 normal pass loop 에서도 같은 가시성 분기 박제. 누락 시 hidden unit 의 G-Buffer normal 잔존 → SSAO 에 ghost (PITFALLS P-3).

**중요**: 위 코드는 챔프/미니언만 필터. 맵/구조물/지형은 항상 보임 (LoL 본체와 동일).

---

## 7. 부쉬 데이터 박제 (소환사의 협곡 12 부쉬)

**파일**: `Client/Bin/Data/Stage1_Bushes.dat` (또는 Stage1.dat 의 v5 섹션)
**작업**: 맵 에디터에서 부쉬 12개 배치 후 저장. 또는 하드코딩 (LoL 본체 좌표 참고).

### 하드코딩 예시 (Scene_InGame::OnEnter)
```cpp
// LoL 소환사의 협곡 부쉬 12 개 (대략 좌표 — 맵 크기 280×280 기준)
struct BushSeed { Vec3 center; f32_t radius; uint32_t bushId; };
static const BushSeed kBushes[] =
{
    // 탑 트라이/리버 부쉬
    { {  -45.f, 0.f,   60.f }, 5.f, 1 },  // 탑 트라이 부쉬 1
    { {  -55.f, 0.f,   45.f }, 4.f, 1 },
    { {  -30.f, 0.f,   90.f }, 5.f, 2 },  // 탑 강 부쉬

    // 미드 부쉬 (양쪽)
    { {  -10.f, 0.f,   10.f }, 4.f, 3 },
    { {   10.f, 0.f,  -10.f }, 4.f, 4 },

    // 봇 트라이/리버 부쉬
    { {   45.f, 0.f,  -60.f }, 5.f, 5 },
    { {   55.f, 0.f,  -45.f }, 4.f, 5 },
    { {   30.f, 0.f,  -90.f }, 5.f, 6 },

    // 정글 부쉬
    { {  -30.f, 0.f,   30.f }, 4.f, 7 },
    { {   30.f, 0.f,  -30.f }, 4.f, 8 },
    { {  -60.f, 0.f,    0.f }, 4.f, 9 },
    { {   60.f, 0.f,    0.f }, 4.f, 10 },
};
for (const auto& b : kBushes)
{
    EntityID e = world.CreateEntity();
    world.AddComponent<BushVolumeComponent>(e) = { b.center, b.radius, b.bushId };
}
m_BushIndex.Build(world);
```

---

## 8. ImGui 미니맵 — FoW 렌더

**신규 파일**: `Client/Public/Editor/MinimapPanel.h` + `.cpp`

```cpp
// .cpp 핵심
void Draw_MinimapPanel(CFogOfWarRenderer* pFow, CWorld& world)
{
    ImGui::Begin("Minimap", nullptr,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse);

    ImVec2 sz = ImGui::GetContentRegionAvail();
    f32_t side = std::min(sz.x, sz.y);

    if (pFow && pFow->GetSRV())
    {
        ImGui::Image((ImTextureID)pFow->GetSRV(), ImVec2(side, side));
    }

    // 챔프/구조물 닷 그리기 (가시성 마스크 반영)
    // 아래는 ImGui DrawList 로 좌표 변환 후 원 그리기
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetItemRectMin();
    constexpr f32_t kWorld = CVisionSystem::FOW_TEX_WORLD_SIZE;

    world.ForEach<TransformComponent, SpatialAgentComponent, VisibilityComponent>(
        function<void(EntityID, TransformComponent&, SpatialAgentComponent&, VisibilityComponent&)>(
            [&](EntityID id, TransformComponent& xf, SpatialAgentComponent& a, VisibilityComponent& v)
            {
                const uint8_t myTeam = 0;   // local
                const bool bMine = (a.team == myTeam);
                const bool bVisToMine = (v.teamVisibilityMask & (1u << myTeam)) != 0;

                if (!bMine && !bVisToMine) return;

                Vec3 p = xf.GetPosition();
                f32_t u = (p.x + kWorld * 0.5f) / kWorld;
                f32_t v2 = (p.z + kWorld * 0.5f) / kWorld;
                ImVec2 c{ origin.x + u * side, origin.y + v2 * side };

                ImU32 color = bMine ? IM_COL32(80, 160, 255, 255)
                                    : IM_COL32(255, 80, 80, 255);
                f32_t radius = (a.kind == eSpatialKind::Champion) ? 4.f : 2.f;
                dl->AddCircleFilled(c, radius, color);
            }));

    ImGui::End();
}
```

---

## 9. vcxproj 등록

**Engine.vcxproj 추가**:
```xml
<ClInclude Include="..\Public\ECS\Components\VisionComponents.h" />
<ClInclude Include="..\Public\ECS\BushVolumeIndex.h" />
<ClInclude Include="..\Public\ECS\Systems\VisionSystem.h" />
<ClInclude Include="..\Public\Renderer\FogOfWarRenderer.h" />

<ClCompile Include="..\Private\ECS\BushVolumeIndex.cpp" />
<ClCompile Include="..\Private\ECS\Systems\VisionSystem.cpp" />
<ClCompile Include="..\Private\Renderer\FogOfWarRenderer.cpp" />
```

**Client.vcxproj 추가**:
```xml
<ClInclude Include="..\Public\Editor\MinimapPanel.h" />
<ClCompile Include="..\Private\Editor\MinimapPanel.cpp" />
```

---

## 10. 검증 시나리오

### V-1: 부쉬 진입/이탈
- [ ] 부쉬 안에 들어간 챔프가 부쉬 밖 적 시야에서 사라짐 (적이 부쉬 밖이면).
- [ ] 부쉬 안에 들어간 적 챔프가 같은 부쉬 안 아군 시야에는 보임.
- [ ] 1 frame 깜빡임 없음 (Codex C-1 의 ForceRebuildNextFrame 작동 확인).

### V-2: 시야 차단
- [ ] 적 미니언이 사거리 밖 (미니언 sightRange 8m 초과) 일 때 미니맵에서 사라짐.
- [ ] 챔프 19m 안에 들어와야 적 챔프 미니맵 표시.

### V-3: FoW
- [ ] 처음 InGame 진입 시 미니맵 전체 검정.
- [ ] 챔프 이동 후 지나간 영역이 회색 (PreviouslySeen) 으로 남음.
- [ ] 현재 시야 영역만 선명 (CurrentlyVisible).

### V-4: 와드
- [ ] 와드 스폰 후 90초 동안 시야 발산.
- [ ] 90초 후 자동 만료 + 시야 사라짐.
- [ ] 핑크와드 (TrueSight) 는 부쉬 안 적도 보임.

### V-5: 성능
- [ ] Profiler `Vision::Execute` 100ms 틱 평균 200us 이하.
- [ ] FoW 텍스처 64KB → GPU 업로드 매 100ms 1회.

---

## 11. Codex 보정 사전 박제

### C-1 (마스터 §6 인용): 부쉬 진입 1 frame 깜빡임
**해결**: `UpdateBushOccupancy` 가 prevBush != nowBush 검출 시 `m_bForceRebuild=true`. 다음 frame 즉시 rebuild.

### C-2: VisibilityComponent 미부착 엔티티
**우려**: 미니언/챔프/구조물 모두 VisibilityComponent 부착 안 하면 "항상 보임" — 의도와 다름.
**해결**: SpatialAgentComponent 부착 시 자동으로 VisibilityComponent 같이 부착 (helper 함수 `AttachSpatialAgent(world, id, kind, team)`).

### C-3: 로컬 플레이어 팀 결정 시점
**우려**: `localTeam` 을 매 frame ForEach<LocalPlayerTag> 로 검색 — 비효율.
**해결**: Scene_InGame OnEnter 에서 1회 캐시 (`m_uLocalTeam`), VisionSystem 에 setter (`SetLocalTeam(uint8_t)`).

### C-4: 부쉬 다각형 vs 원형
**우려**: LoL 부쉬는 다각형 — 원으로 근사하면 가장자리 부정확.
**해결**: v5 = 원형 (간단), v6 에서 다각형 추가. 12 부쉬 × 원 4~5 개 분리해 군집 (`bushId` 같음 → 같은 시야).

### C-5: 와드 만료 처리
**우려**: 와드가 만료해도 VisionSourceComponent 가 남아있으면 시야 계속 발산.
**해결**: `WardComponent::remainingDuration <= 0` 시 `world.DestroyEntity(wardId)` 별도 system (CWardLifetimeSystem, phase=3.5).

### C-6: 같은 frame 안 위치 변경
**우려**: NavSystem(2) 후 위치 변경 → Vision(4) 까지 frame 안에 갱신. SpatialIndex 는 phase=0 이라 stale.
**해결**: 1 frame 지연 허용 (마스터 §6 C-3). 100ms 틱이라 4 frame 후 갱신 — 사용자 체감 무관.

### C-7: GPU FoW 텍스처 용량
**해결**: R8 단일 채널 64KB. 매 100ms 1회 upload.

---

## 12. 다음 진입

02 완료 후 → **03 TOWER ATTACK SYSTEM** (CSpatialIndex + Vision 활용한 LoL 우선순위 규칙).

---

**END OF SUB-PLAN 02**
