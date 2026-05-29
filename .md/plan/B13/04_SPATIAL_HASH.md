# Phase B-13 / 04 — 미니언/챔프/구조물 Spatial Hash 인프라 (v2)

**버전**: v2 (2026-05-04 — Codex 검토 #2 P-9/P-10/P-11/P-12/P-14 정정)
**가이드**: [`.md/process/PLAN_AUTHORING_PITFALLS.md`](../../process/PLAN_AUTHORING_PITFALLS.md)
**선행**: 없음 (인프라). 02/03/05 모두 본 sub-plan 의 `CSpatialIndex::QueryRadius` API 에 의존.
**총 LOC**: ~900.
**효과**: 현재 `MinionAISystem::FindClosestEnemy` 의 O(N²) → O(K) (K = cell 안 엔티티 수, 보통 5~20). 미니언 30체 + 챔프 10체 + 정글 + 구조물 ≈ 60+ 엔티티 환경에서 measurable. **행동 보존** — 입력/출력 도메인은 기존 알고리즘과 정확히 동일 (P-14).

---

## §0. v1 → v2 정정 매트릭스

| # | v1 결함 | v2 정정 | PITFALLS |
|---|---|---|---|
| 1 | phase = 0.5 (Transform 과 충돌, fractional 무효) | **phase = 1 (정수, 단독)** | P-9 |
| 2 | `CEngineApp::m_SpatialIndex` 전역 + `CGameInstance::Get_SpatialIndex()` Tier-2 게터 | **`CWorld::Get_SpatialIndex()` (CWorld owned)** — 서버 GameRoom / 멀티 World 시 World 마다 1개 | P-10 |
| 3 | `CELL_SIZE = 8.f`, `GRID_HALF_EXTENT = 32` Engine 공용 박힘 | **`SpatialGridDesc` 주입** — `Initialize(const SpatialGridDesc&)`. Scene/GameRoom 이 LoL 용 desc 전달 | P-11 |
| 4 | `CellX = static_cast<int32_t>(world / cell)` — 음수 0-방향 절삭 | **`std::floor((world - origin) / cellSize)`** | P-12 |
| 5 | MinionAI 교체 mask = Minion+Champion+Turret+JungleMob+... 6종 (라인전 행동 변경) | **mask = `SpatialMask(eSpatialKind::Minion)` only** (행동 보존) | P-14 |
| 6 | 공개 헤더 `#include "Entity.h"` flat | **`#include "ECS/Entity.h"` (subdir 보존)** | P-8 |

---

## 0. 설계 의사결정

### Spatial Hash vs Quadtree vs KD-Tree

| 기법 | 빌드 비용 | 쿼리 비용 | LoL 적합 | 메모리 | 동적 갱신 |
|---|---|---|---|---|---|
| **Spatial Hash (Uniform Grid)** | O(N) | O(K) avg | ✅ 최적 | mid | ✅ 즉시 |
| Quadtree | O(N log N) | O(log N + K) | ⚠️ 챔프 밀집 시 split 비용 | low | ⚠️ rebalance 비용 |
| KD-Tree | O(N log N) | O(log N) | ❌ 동적 갱신 비용 큼 | low | ❌ rebuild 빈번 |
| BVH (Phase D Physics) | O(N log N) | O(log N) | ❌ overkill | high | ❌ |

**선택: Spatial Hash (Uniform Grid)**.

**이유**:
1. LoL 맵은 **영역 한정** (평면 약 200×200 m) — uniform grid 의 약점인 광활한 빈 공간 문제 없음.
2. 모든 엔티티가 평면 (y ≈ 0) → 2D grid 면 충분.
3. **frame 마다 rebuild** 가능 — 미니언/챔프 30~60체 규모는 CPU 50us 수준.
4. cell size 8m (LoL 평타 사거리 평균 5.5m, 시야 19m) → 대부분 쿼리는 **9 cell** 만 검사.

### Cell Size

LoL 맵 약 280m × 280m. cell size 8m → 35 × 35 = 1225 셀. 셀당 평균 0.05 엔티티.
**8m 결정 근거**:
- 평타 사거리 (Caitlyn 6.5m, Ashe 6.0m) ≤ cell 1개
- 챔프 시야 19m ≈ 3×3 cell (9 cell 검사)
- 미니언 시야 8m ≈ 1 cell

---

## 1. SpatialAgentComponent 박제

**신규 파일**: `Engine/Public/ECS/Components/SpatialAgentComponent.h`

```cpp
#pragma once
#include "Engine_Defines.h"
#include "WintersMath.h"
#include "WintersTypes.h"
#include "ECS/Entity.h"

// ─────────────────────────────────────────────────────────────
// SpatialAgentComponent — Spatial Hash 인덱싱 대상 엔티티 마커
//
// CSpatialHashSystem 매 프레임 빌드:
//   1. Transform 갱신 (Phase 0) 후
//   2. 본 컴포넌트 가진 엔티티만 grid 에 삽입
//
// kind 는 쿼리 필터링 용. QueryRadius(pos, r, mask) 로 비트마스크 필터.
// ─────────────────────────────────────────────────────────────

enum class eSpatialKind : uint32_t
{
    None       = 0,
    Champion   = 1u << 0,
    Minion     = 1u << 1,
    Turret     = 1u << 2,
    JungleMob  = 1u << 3,
    Inhibitor  = 1u << 4,
    Nexus      = 1u << 5,
    Ward       = 1u << 6,
    Projectile = 1u << 7,
    Bush       = 1u << 8,   // 부쉬 영역 (시야 차단 — 02 sub-plan 에서 사용)
    All        = 0xFFFFFFFFu
};

struct SpatialAgentComponent
{
    eSpatialKind kind = eSpatialKind::None;
    uint8_t      team = 0;     // eTeam cast — 0=Blue, 1=Red, 2=Neutral
    f32_t        radius = 0.5f; // collision/selection 반경 (m)

    // 내부 캐시 (CSpatialHashSystem 만 갱신)
    int32_t      cachedCellX = INT32_MIN;
    int32_t      cachedCellZ = INT32_MIN;
};

// 비트마스크 헬퍼
inline constexpr uint32_t SpatialMask(eSpatialKind k)
{
    return static_cast<uint32_t>(k);
}
inline constexpr uint32_t operator|(eSpatialKind a, eSpatialKind b)
{
    return static_cast<uint32_t>(a) | static_cast<uint32_t>(b);
}
```

---

## 2. CSpatialIndex 인터페이스 + 구현

**신규 파일**: `Engine/Public/ECS/SpatialIndex.h`

```cpp
#pragma once
#include "Engine_Defines.h"
#include "WintersAPI.h"
#include "WintersMath.h"
#include "WintersTypes.h"
#include "Entity.h"
#include <vector>
#include <unordered_map>
#include <cstdint>

// Forward
class CWorld;
struct SpatialAgentComponent;

// ─────────────────────────────────────────────────────────────
// CSpatialIndex — CWorld owned (P-10 회피).
// CGameInstance Tier-2 게터 박제 폐기. 사용 예:
//   CSpatialIndex* spatial = world.Get_SpatialIndex();
//   std::vector<EntityID> hits;
//   spatial->QueryRadius(myPos, 12.f, SpatialMask(eSpatialKind::Minion), 0, hits);
//
// 도메인 상수 (cell size, grid extent) 는 SpatialGridDesc 주입 (P-11 회피).
// LoL Scene 은 LoL desc 주입, 엘든링 World 는 자체 desc.
// ─────────────────────────────────────────────────────────────

// Engine 공용 — 호출자가 채워서 Initialize 에 전달
struct SpatialGridDesc
{
    Vec3    worldOrigin = { 0.f, 0.f, 0.f };  // 그리드 (0,0) 셀이 표현할 월드 좌표 origin
    f32_t   cellSize    = 8.f;                // 셀 크기 (m)
    int32_t halfExtentX = 32;                 // ±halfExtentX 범위만 인덱싱 (벗어나면 skip)
    int32_t halfExtentZ = 32;
};

// LoL 기본 desc (Scene_InGame 에서 사용)
inline SpatialGridDesc LoLSpatialGridDesc()
{
    return SpatialGridDesc{
        /*origin*/   { 0.f, 0.f, 0.f },
        /*cellSize*/ 8.f,
        /*halfX*/    32,
        /*halfZ*/    32
    };
}

class WINTERS_ENGINE CSpatialIndex
{
public:
    CSpatialIndex() = default;
    ~CSpatialIndex() = default;

    CSpatialIndex(const CSpatialIndex&) = delete;
    CSpatialIndex& operator=(const CSpatialIndex&) = delete;

    // ★ v2: GridDesc 주입. CWorld::Initialize_Spatial(desc) 에서 호출.
    void Initialize(const SpatialGridDesc& desc) { m_desc = desc; }
    const SpatialGridDesc& GetDesc() const { return m_desc; }

    // 매 frame Phase 1 (CSpatialHashSystem) 에서 호출. Transform/SpatialAgent 가진 모든 엔티티 재인덱싱.
    void Rebuild(CWorld& world);

    // 반경 r 쿼리. mask=0xFFFFFFFF 이면 전체 종류, excludeTeamMask 비트셋 (예: 1<<0 = Blue 제외).
    void QueryRadius(const Vec3& center, f32_t radius,
                     uint32_t kindMask, uint32_t excludeTeamMask,
                     std::vector<EntityID>& out) const;

    EntityID QueryClosest(const Vec3& center, f32_t maxRadius,
                          uint32_t kindMask, uint8_t myTeam) const;

    struct DebugStats {
        uint32_t totalEntities = 0;
        uint32_t occupiedCells = 0;
        uint32_t maxEntitiesInCell = 0;
    };
    DebugStats GetDebugStats() const;
    void GetOccupiedCellCenters(std::vector<Vec3>& out) const;

private:
    // ★ v2: floor 기반 (P-12 회피). worldOrigin 빼고 / cellSize → floor.
    //   negative 좌표 truncation 회피. -0.5/8 → -1 (int cast 시 0 = wrong cell).
    inline int32_t CellX(f32_t worldX) const
    {
        return static_cast<int32_t>(std::floor((worldX - m_desc.worldOrigin.x) / m_desc.cellSize));
    }
    inline int32_t CellZ(f32_t worldZ) const
    {
        return static_cast<int32_t>(std::floor((worldZ - m_desc.worldOrigin.z) / m_desc.cellSize));
    }
    inline int64_t MakeKey(int32_t cx, int32_t cz) const
    {
        // sign extension 회피 — uint32_t 캐스팅으로 음수 cz 의 상위 비트 제거 후 OR
        return (static_cast<int64_t>(cx) << 32) | static_cast<uint32_t>(cz);
    }

    struct CellEntry {
        EntityID     id;
        Vec3         pos;
        eSpatialKind kind;
        uint8_t      team;
        f32_t        radius;
    };

    SpatialGridDesc m_desc{};   // Initialize() 에서 채워짐
    std::unordered_map<int64_t, std::vector<CellEntry>> m_mapCells;
    uint32_t m_uTotalEntities = 0;
};
```

**신규 파일**: `Engine/Private/ECS/SpatialIndex.cpp`

```cpp
#include "WintersPCH.h"
#include "ECS/SpatialIndex.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ProfilerAPI.h"

#include <cmath>
#include <algorithm>

void CSpatialIndex::Rebuild(CWorld& world)
{
    WINTERS_PROFILE_SCOPE("SpatialIndex::Rebuild");

    m_mapCells.clear();
    m_uTotalEntities = 0;

    world.ForEach<TransformComponent, SpatialAgentComponent>(
        function<void(EntityID, TransformComponent&, SpatialAgentComponent&)>(
            [&](EntityID id, TransformComponent& xf, SpatialAgentComponent& agent)
            {
                if (agent.kind == eSpatialKind::None) return;

                const Vec3 pos = xf.GetPosition();
                const int32_t cx = CellX(pos.x);
                const int32_t cz = CellZ(pos.z);

                if (std::abs(cx) > m_desc.halfExtentX ||
                    std::abs(cz) > m_desc.halfExtentZ)
                    return;   // 맵 밖 — 스킵

                agent.cachedCellX = cx;
                agent.cachedCellZ = cz;

                CellEntry e{};
                e.id = id;
                e.pos = pos;
                e.kind = agent.kind;
                e.team = agent.team;
                e.radius = agent.radius;

                m_mapCells[MakeKey(cx, cz)].push_back(e);
                ++m_uTotalEntities;
            }));

    WINTERS_PROFILE_COUNT("SpatialIndex::TotalEntities",
        static_cast<i32_t>(m_uTotalEntities));
    WINTERS_PROFILE_COUNT("SpatialIndex::OccupiedCells",
        static_cast<i32_t>(m_mapCells.size()));
}

void CSpatialIndex::QueryRadius(const Vec3& center, f32_t radius,
                                uint32_t kindMask, uint32_t excludeTeamMask,
                                std::vector<EntityID>& out) const
{
    WINTERS_PROFILE_SCOPE("SpatialIndex::QueryRadius");

    const int32_t cx = CellX(center.x);
    const int32_t cz = CellZ(center.z);
    const int32_t cellRadius = static_cast<int32_t>(std::ceil(radius / m_desc.cellSize)) + 1;
    const f32_t   r2 = radius * radius;

    for (int32_t dz = -cellRadius; dz <= cellRadius; ++dz)
    {
        for (int32_t dx = -cellRadius; dx <= cellRadius; ++dx)
        {
            const int32_t qx = cx + dx;
            const int32_t qz = cz + dz;
            if (std::abs(qx) > m_desc.halfExtentX || std::abs(qz) > m_desc.halfExtentZ)
                continue;

            auto it = m_mapCells.find(MakeKey(qx, qz));
            if (it == m_mapCells.end()) continue;

            for (const auto& e : it->second)
            {
                if ((static_cast<uint32_t>(e.kind) & kindMask) == 0) continue;
                if ((1u << e.team) & excludeTeamMask) continue;
                const f32_t dx2 = e.pos.x - center.x;
                const f32_t dz2 = e.pos.z - center.z;
                if (dx2 * dx2 + dz2 * dz2 <= r2)
                    out.push_back(e.id);
            }
        }
    }
}

EntityID CSpatialIndex::QueryClosest(const Vec3& center, f32_t maxRadius,
                                     uint32_t kindMask, uint8_t myTeam) const
{
    WINTERS_PROFILE_SCOPE("SpatialIndex::QueryClosest");

    const int32_t cx = CellX(center.x);
    const int32_t cz = CellZ(center.z);
    const int32_t cellRadius = static_cast<int32_t>(std::ceil(maxRadius / m_desc.cellSize)) + 1;
    const f32_t   r2max = maxRadius * maxRadius;

    EntityID best = NULL_ENTITY;
    f32_t    bestDist2 = r2max;

    for (int32_t dz = -cellRadius; dz <= cellRadius; ++dz)
    {
        for (int32_t dx = -cellRadius; dx <= cellRadius; ++dx)
        {
            const int32_t qx = cx + dx;
            const int32_t qz = cz + dz;
            if (std::abs(qx) > m_desc.halfExtentX || std::abs(qz) > m_desc.halfExtentZ)
                continue;

            auto it = m_mapCells.find(MakeKey(qx, qz));
            if (it == m_mapCells.end()) continue;

            for (const auto& e : it->second)
            {
                if ((static_cast<uint32_t>(e.kind) & kindMask) == 0) continue;
                if (e.team == myTeam) continue;   // 같은 팀 제외

                const f32_t dx2 = e.pos.x - center.x;
                const f32_t dz2 = e.pos.z - center.z;
                const f32_t d2  = dx2 * dx2 + dz2 * dz2;
                if (d2 < bestDist2)
                {
                    bestDist2 = d2;
                    best = e.id;
                }
            }
        }
    }
    return best;
}

CSpatialIndex::DebugStats CSpatialIndex::GetDebugStats() const
{
    DebugStats s{};
    s.totalEntities = m_uTotalEntities;
    s.occupiedCells = static_cast<uint32_t>(m_mapCells.size());
    for (const auto& kv : m_mapCells)
        s.maxEntitiesInCell = std::max(s.maxEntitiesInCell,
            static_cast<uint32_t>(kv.second.size()));
    return s;
}

void CSpatialIndex::GetOccupiedCellCenters(std::vector<Vec3>& out) const
{
    out.clear();
    out.reserve(m_mapCells.size());
    for (const auto& kv : m_mapCells)
    {
        const int32_t cx = static_cast<int32_t>(kv.first >> 32);
        const int32_t cz = static_cast<int32_t>(kv.first & 0xFFFFFFFFu);
        out.push_back({ (cx + 0.5f) * m_desc.cellSize + m_desc.worldOrigin.x,
                         0.f,
                         (cz + 0.5f) * m_desc.cellSize + m_desc.worldOrigin.z });
    }
}
```

---

## 3. CSpatialHashSystem — Phase 1 등록 (정수)

**신규 파일**: `Engine/Public/ECS/Systems/SpatialHashSystem.h`

```cpp
#pragma once
#include "Engine_Defines.h"
#include "WintersAPI.h"
#include "ECS/ISystem.h"
#include "ECS/SpatialIndex.h"
#include <memory>

NS_BEGIN(Engine)

class CWorld;

class WINTERS_ENGINE CSpatialHashSystem final : public ISystem
{
public:
    ~CSpatialHashSystem() override = default;

    // ★ v2: CWorld owned. Create 시 World 의 SpatialIndex 핸들 받음.
    static std::unique_ptr<CSpatialHashSystem> Create()
    {
        return std::unique_ptr<CSpatialHashSystem>(new CSpatialHashSystem());
    }

    // ★ v2 정수 phase. Transform(0) 직후 단독 phase=1.
    // MinionAI(2)/Nav(3)/Status(4)/Vision(5)/Turret(6)/BT(7)/MCTS(8) 모두 본 시스템 후 실행.
    // 같은 phase 시스템 0개 — race 0.
    uint32_t    GetPhase() const override { return 1; }
    const char* GetName()  const override { return "SpatialHashSystem"; }
    void        Execute(CWorld& world, f32_t fTimeDelta) override;

private:
    CSpatialHashSystem() = default;
};

NS_END
```

**신규 파일**: `Engine/Private/ECS/Systems/SpatialHashSystem.cpp`

```cpp
#include "WintersPCH.h"
#include "ECS/Systems/SpatialHashSystem.h"
#include "ECS/World.h"
#include "ECS/SpatialIndex.h"
#include "ProfilerAPI.h"

NS_BEGIN(Engine)

void CSpatialHashSystem::Execute(CWorld& world, f32_t /*dt*/)
{
    WINTERS_PROFILE_SCOPE("SpatialHashSystem::Execute");
    CSpatialIndex* pIndex = world.Get_SpatialIndex();
    if (!pIndex) return;
    pIndex->Rebuild(world);
}

NS_END
```

**phase=1 단독 검증**: Transform(0) → SpatialHash(1) 단독 → MinionAI(2). 다른 시스템 등록 시 phase=1 충돌 금지.

---

## 4. CWorld 가 SpatialIndex 소유 (★ v2 정정)

**v1 폐기**: `CGameInstance::Get_SpatialIndex()` Tier-2 게터 + `CEngineApp::m_SpatialIndex` 멤버 박제.
**v2 정답**: CWorld 가 멤버로 소유. World 마다 1개 — 서버 멀티 GameRoom / 엘든링 멀티 World 모두 자연스럽게 1:1 (PITFALLS P-10).

**파일**: `Engine/Public/ECS/World.h`
**작업**: `CWorld` 에 `unique_ptr<CSpatialIndex>` 멤버 + 게터 + Initialize.

### 수정 전 (CWorld 의 멤버 영역)
```cpp
class WINTERS_ENGINE CWorld
{
    // ... 기존 ECS 멤버 ...
};
```

### 수정 후
```cpp
#include "ECS/SpatialIndex.h"   // 헤더 — full type 필요 (멤버에서 unique_ptr 사용)

class WINTERS_ENGINE CWorld
{
public:
    // ★ v2: SpatialIndex CWorld owned (P-10 회피).
    // Scene_InGame::OnEnter 또는 GameRoom::Initialize 에서 1회 호출.
    void Initialize_Spatial(const SpatialGridDesc& desc);
    CSpatialIndex* Get_SpatialIndex() const { return m_pSpatialIndex.get(); }

private:
    // ... 기존 멤버 ...
    std::unique_ptr<CSpatialIndex> m_pSpatialIndex;   // ★ v2
};
```

**구현** (`Engine/Private/ECS/World.cpp`):
```cpp
void CWorld::Initialize_Spatial(const SpatialGridDesc& desc)
{
    if (!m_pSpatialIndex)
        m_pSpatialIndex = std::unique_ptr<CSpatialIndex>(new CSpatialIndex());
    m_pSpatialIndex->Initialize(desc);
}
```

**Scene_InGame::OnEnter 호출**:
```cpp
m_World.Initialize_Spatial(LoLSpatialGridDesc());   // LoL 280×280m, cell=8m
```

**CGameInstance Tier-2 게터 박제 폐기** — Engine/Include/GameInstance.h 변경 없음.

### 4-구버전 (참고용) — CGameInstance Tier-2 게터 (★ v1 폐기)

```cpp
class CDX11Device;
class DX11Shader;
class DX11Pipeline;
class CBlendStateCache;

class WINTERS_ENGINE CGameInstance : ...
{
public:
    // Tier-2 RHI getters
    CDX11Device*     Get_RHIDevice();
    DX11Shader*      Get_MeshShader();
    DX11Pipeline*    Get_MeshPipeline();
    CBlendStateCache* Get_BlendStateCache();
    ...
};
```

### ⚠️ 수정 후 (★ v1 폐기 — 본 박제는 적용 금지. CWorld owned 가 v2 정답)

```cpp
// ★ 이 박제는 v1 잔재. 적용 금지. CWorld::Get_SpatialIndex() 사용 (§4 v2 박제).
// v2: Engine/Include/GameInstance.h 변경 없음.
```

---

## 5. MinionAISystem 의 FindClosestEnemy 교체 (★ v2 — Minion only mask, 행동 보존)

**파일**: `Engine/Private/ECS/Systems/MinionAISystem.cpp`
**현재**: O(N²) loop (CLAUDE.md 보고서 기준 — `vecMinions` 전체 순회).

### 수정 전 (개념)
```cpp
EntityID CMinionAISystem::FindClosestEnemy(CWorld& world, EntityID self,
    const Vec3& myPos, uint8_t myTeamRaw, f32_t searchRange)
{
    EntityID best = NULL_ENTITY;
    f32_t    bestDist2 = searchRange * searchRange;

    world.ForEach<MinionStateComponent, TransformComponent>(
        function<void(EntityID, MinionStateComponent&, TransformComponent&)>(
            [&](EntityID id, MinionStateComponent& ms, TransformComponent& xf)
            {
                if (id == self) return;
                if (ms.team == myTeamRaw) return;
                ...
                const f32_t d2 = /* dist^2 */;
                if (d2 < bestDist2)
                {
                    bestDist2 = d2;
                    best = id;
                }
            }));
    return best;
}
```

### 수정 후 — SpatialIndex.QueryClosest 호출 (★ v2 정정)

**중요 (P-14)**: 현재 [`CMinionAISystem::FindClosestEnemy`](../../../Engine/Public/ECS/Systems/MinionAISystem.h:61) 는 `MinionStateComponent` 가진 엔티티만 ForEach 순회 = **enemy minion only 타겟팅**. 본 교체는 **자료구조만 바꾸고 도메인 동일 유지** — mask = `eSpatialKind::Minion` only. 챔프/타워/정글몹 어그로는 별도 PR (B-13.5+).

```cpp
EntityID CMinionAISystem::FindClosestEnemy(CWorld& world, EntityID self,
    const Vec3& myPos, uint8_t myTeamRaw, f32_t searchRange)
{
    CSpatialIndex* spatial = world.Get_SpatialIndex();
    if (!spatial)
    {
        // 폴백 — 기존 O(N²) 코드 (개발 중 World::Initialize_Spatial 미호출 시)
        return FindClosestEnemy_Legacy(world, self, myPos, myTeamRaw, searchRange);
    }

    // ★ v2: Minion only — 행동 보존. 챔프/타워 어그로 확장은 별도 PR.
    const uint32_t mask = SpatialMask(eSpatialKind::Minion);

    EntityID candidate = spatial->QueryClosest(myPos, searchRange, mask, myTeamRaw);
    if (candidate == self) return NULL_ENTITY;
    if (!world.IsAlive(candidate)) return NULL_ENTITY;

    // 기존 검증 유지 — MinionState/Health 검사
    if (!world.HasComponent<MinionStateComponent>(candidate)) return NULL_ENTITY;
    if (world.HasComponent<HealthComponent>(candidate))
    {
        const auto& hp = world.GetComponent<HealthComponent>(candidate);
        if (hp.bIsDead || hp.fCurrent <= 0.f) return NULL_ENTITY;
    }
    return candidate;
}
```

**v1 의 6종 mask 박제는 폐기**. 아래는 참고용 v1 본문 — 적용 금지:

```text
★ v1 박제 (전체 폐기 — 적용 금지):
   const uint32_t mask = SpatialMask(eSpatialKind::Minion)
                       | SpatialMask(eSpatialKind::Champion)
                       | SpatialMask(eSpatialKind::Turret)
                       | SpatialMask(eSpatialKind::JungleMob)
                       | SpatialMask(eSpatialKind::Inhibitor)
                       | SpatialMask(eSpatialKind::Nexus);
v1 의 6종 mask 는 LoL 라인전 동작 (미니언이 적 미니언만 평타) 을 변경시킴.
v2 는 mask = SpatialMask(eSpatialKind::Minion) only.
```

**CWorld SpatialIndex 멤버는 §4 (위) 에서 박제 완료** — `unique_ptr<CSpatialIndex> m_pSpatialIndex` + `Initialize_Spatial(desc)` + `Get_SpatialIndex()`. 별도 setter (`SetSpatialIndex`) 박제 폐기.

Scene_InGame::OnEnter 호출 (1 줄):
```cpp
m_World.Initialize_Spatial(LoLSpatialGridDesc());
```

---

## 6. SpatialAgentComponent 부착 위치

**파일**: 미니언/챔프/구조물 스폰 지점 — 각각 1 줄 추가.

| 엔티티 | 스폰 함수 | 부착 코드 |
|---|---|---|
| 미니언 | `CMinion_Manager::SpawnMinion` | `world.AddComponent<SpatialAgentComponent>(id) = { eSpatialKind::Minion, team, 0.5f };` |
| 챔프 | `CreateChampionEntity` (Scene_InGame) | `... = { eSpatialKind::Champion, team, 0.6f };` |
| 타워 | `CStructure_Manager::SpawnStructure` | tier 에 따라 `eSpatialKind::Turret`, `eSpatialKind::Inhibitor`, `eSpatialKind::Nexus` 분기 |
| 정글몹 | `CJungle_Manager::SpawnJungleMob` | `... = { eSpatialKind::JungleMob, /*Neutral=*/ 2, 0.8f };` |

---

## 7. 디버그 ImGui 패널

**신규 파일**: `Client/Public/Editor/SpatialDebugPanel.h` + `.cpp`

```cpp
// .h
#pragma once
class CSpatialIndex;
class CGraphicDev;

namespace Editor
{
    void Draw_SpatialDebugPanel(CSpatialIndex* spatial);
    void Draw_SpatialDebugOverlay3D(CSpatialIndex* spatial); // 3D 셀 와이어프레임
}
```

```cpp
// .cpp 핵심
#include "Editor/SpatialDebugPanel.h"
#include "ECS/SpatialIndex.h"
#include <imgui.h>

namespace Editor
{
    void Draw_SpatialDebugPanel(CSpatialIndex* spatial)
    {
        if (!spatial) return;
        ImGui::Begin("Spatial Hash Debug");
        auto stats = spatial->GetDebugStats();
        ImGui::Text("Total Entities: %u", stats.totalEntities);
        ImGui::Text("Occupied Cells: %u", stats.occupiedCells);
        ImGui::Text("Max in Cell: %u", stats.maxEntitiesInCell);
        ImGui::Separator();
        ImGui::Text("Cell Size: %.1f m", CSpatialIndex::CELL_SIZE);
        ImGui::Text("Grid: %d x %d (%d cells)",
            CSpatialIndex::GRID_HALF_EXTENT * 2,
            CSpatialIndex::GRID_HALF_EXTENT * 2,
            (CSpatialIndex::GRID_HALF_EXTENT * 2) *
            (CSpatialIndex::GRID_HALF_EXTENT * 2));
        ImGui::End();
    }
}
```

**호출 위치**: `Scene_InGame::OnImGui()` 끝에 1 줄 추가:
```cpp
Editor::Draw_SpatialDebugPanel(pGI->Get_SpatialIndex());
```

---

## 8. vcxproj 등록

**파일**: `Engine/Include/Engine.vcxproj`
**작업**: 다음 6 항목 추가.

```xml
<ClInclude Include="..\Public\ECS\Components\SpatialAgentComponent.h" />
<ClInclude Include="..\Public\ECS\SpatialIndex.h" />
<ClInclude Include="..\Public\ECS\Systems\SpatialHashSystem.h" />

<ClCompile Include="..\Private\ECS\SpatialIndex.cpp" />
<ClCompile Include="..\Private\ECS\Systems\SpatialHashSystem.cpp" />
```

`.filters` 에 `05. ECS\Components` (SpatialAgent), `05. ECS` (SpatialIndex), `05. ECS\Systems` (SpatialHashSystem) 분류.

UpdateLib.bat 자동 동기화 (`xcopy /S` 가 `EngineSDK/inc` 로 복사).

---

## 9. 검증 시나리오

### V-1: 빌드 통과
- [ ] `MSBuild Engine.vcxproj /p:Configuration=Debug` 0 error
- [ ] `MSBuild Client.vcxproj /p:Configuration=Debug` 0 error

### V-2: 스폰 검증
- [ ] InGame 진입 후 ImGui SpatialDebug 창에 `Total Entities` 가 미니언 30 + 챔프 1 + 타워 11 + 정글몹 ≈ 50 표시.
- [ ] `Occupied Cells` 가 ~30 (미니언/챔프/타워 흩어진 셀 수).
- [ ] `Max in Cell` 이 ~5 (미니언 5 체 한 웨이브 = 같은 cell).

### V-3: 쿼리 정확성
- [ ] MinionAISystem 의 적 검색이 기존과 **동일한 결과** 반환 (양 알고리즘 비교 로그 — Profiler 카운터 `MinionAI::TargetChanged` 가 0).
- [ ] 적 미니언이 시야 (sightRange=15) 안에 있을 때 `FindClosestEnemy` 가 nullptr 반환 X.

### V-4: 성능
- [ ] Profiler `SpatialIndex::Rebuild` 가 50us 이하 (60 엔티티 기준).
- [ ] `MinionAI::DecisionPass` 가 기존 대비 30% 이상 단축.

### V-5: 회귀 테스트
- [ ] grep `FindClosestEnemy_Legacy` — 폴백 경로가 호출되지 않는지 (`SetSpatialIndex` 누락 시에만 호출).
- [ ] grep `world.ForEach.*MinionStateComponent.*Transform` — 미니언 AI 의 O(N²) loop 제거 확인.

---

## 10. 회귀 grep 4 종

```bash
# 1. SpatialAgentComponent 미부착 엔티티 검출
grep -r "AddComponent<MinionComponent>" Client/Private/ Engine/Private/
# → 같은 줄 또는 직후에 SpatialAgentComponent 부착 필수

# 2. O(N²) 패턴 검출 (미니언 AI 외 다른 시스템)
grep -rn "ForEach.*Component.*ForEach" Engine/Private/

# 3. Tier-2 게터 일관성
grep -n "Get_SpatialIndex" Engine/Include/GameInstance.h Engine/Private/GameInstance.cpp

# 4. CWorld::SetSpatialIndex 호출 누락
grep -rn "World.SetSpatialIndex\|m_World.SetSpatialIndex" Client/
# → Scene_InGame::OnEnter 에서 1회 필수
```

---

## 11. Codex 보정 7 건

### Codex C-1: phase=0 등록 순서 race
**우려**: TransformSystem 과 SpatialHashSystem 모두 phase=0 — 등록 순서 잘못되면 stale Transform 으로 빌드.
**해결**: Scene_InGame::OnEnter 에서 `RegisterSystem(TransformSystem)` 먼저, `RegisterSystem(SpatialHashSystem)` 다음. 코멘트로 박제.

### Codex C-2: 큰 cell 일정 분포 가정
**우려**: 베이스 (Nexus 영역) 에 미니언/챔프 10+ 모이면 Max in Cell 폭발.
**해결**: cell size 8m 유지 (LoL 베이스 좁아도 ≈ 30m → 4 cell). 셀당 5~8 엔티티는 vector linear scan 이 cache hit 빨라 OK.

### Codex C-3: cell 경계 stale entity (CLAUDE.md L1187 마스터 §6 C-3)
**해결**: SpatialHash 가 Phase 0 에 빌드, NavSystem(2) 후 위치 갱신은 다음 frame Phase 0 에 반영. 1 frame stale 허용 — 시야/타워 100ms 틱이라 무관.

### Codex C-4: int64 cell key 충돌 가능성
**우려**: cx>>32 | cz 인코딩 — cz 가 음수면 sign extension 으로 충돌.
**해결**: `(int64_t)cx << 32 | (uint32_t)cz` — uint32_t 캐스팅으로 sign 제거.

### Codex C-5: Rebuild 매 frame 시 atomic
**우려**: JobSystem 멀티스레드 빌드 시 `m_mapCells[key].push_back` race.
**해결**: 본 phase 0 은 단일 스레드 (`SystemScheduler::Execute` 의 phase 내 직렬 분기). 병렬화 미적용 — 60 엔티티 50us 면 충분.

### Codex C-6: 그림자 분신 (Zed W) — 본체와 같은 cell
**우려**: Zed 그림자 분신은 별도 EntityID 지만 SpatialAgent 부착 시 본체와 같은 위치 → cell 안 2개.
**해결**: 그림자 분신은 `eSpatialKind::Champion` 부착 + team 동일 → MinionAI 의 `excludeTeamMask` 가 자동 제외. 별 처리 X.

### Codex C-7: 부쉬 (eSpatialKind::Bush) 정적 + 동적 혼재
**우려**: 부쉬는 정적 (맵 데이터에서 1회 로드 후 안 움직임) — 매 frame Rebuild 비효율.
**해결**: Bush 는 별도 정적 인덱스 `CBushVolumeIndex` (02 sub-plan) 로 분리. SpatialIndex 는 동적 엔티티 전용.

---

## 12. 다음 진입

04 완료 후 → **02 VISION FoW** (CSpatialIndex 사용해 시야 후보 빠르게 검색) → 03 → 05.

---

**END OF SUB-PLAN 04**
