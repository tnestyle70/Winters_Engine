# Phase Sim-10 v2 — M3 Sub-plan: Snapshot Delta + AOI + Vision (시야 동기화)

**작성일**: 2026-04-30
**상위 문서**: `10_UDP_LOL_NETSTACK_MASTER_v2.md` §5 + §8 M3
**전제**: Sim-10 v2 M2 (Reliability) 합격 후
**범위**: Snapshot Delta encoding + AOI grid filtering + **Vision (시야 / fog-of-war)** 시스템 박제. 5v5 룸 outbound 4KB/s/client 목표.
**합격**: 5v5 평균 snapshot < 200B, baseline 미일치 시 자동 full resync, **시야 외 적군 entity 0건 broadcast**, idle client 5초 후 baseline ack 정상

**한 줄**: **Snapshot 대역폭 80% 절감 = Delta encoding + AOI 50m grid + Vision 컴포넌트 (per-team fog-of-war). LoL 의 핵심 — "시야 안 보이는 적군 정보 절대 client 에 전송 X" (anti-cheat 의 1차 방어선).**

---

## 0. M3 사이클 위치

```
M1 UDP Transport    (완료)
M2 Reliability      (완료)
        ↓
   ┌────────────── M3 (본 sub-plan) ──────────────┐
   │  Snapshot Delta + AOI + Vision               │
   │  - 대역폭 80% 절감                            │
   │  - 시야 안 보이는 적군 정보 0 leak (anti-cheat) │
   └───────────────────┬───────────────────────────┘
                       ↓
                M4 Lag Compensation
                       ↓
                M5 Client Prediction
```

M3 = **양적 (대역폭) + 질적 (시야) 두 측면 동시 강화**. LoL 같은 5v5 PvP 게임에서 가장 중요한 시스템 — fog-of-war 누수 시 maphack 가능.

---

## 1. 3가지 핵심 — Delta + AOI + Vision

### 1.1 Snapshot Delta (★ 대역폭)

**현재 (M1/M2)**: 매 snapshot 에 모든 entity 의 모든 필드 전송. 5v5 = 10 entity × 60B = 600B/snapshot × 30Hz = 18 KB/s/client.

**M3 후 (Delta)**: 직전 acked snapshot 과의 차분만 전송. 정적 entity (idle 챔프) 는 0B. 평균 200B/snapshot × 30Hz = 6 KB/s/client.

### 1.2 AOI Grid (★ 결정성 + 성능)

**현재**: 모든 entity 가 모든 client 에 broadcast. server CPU O(N×M) — 100 동접 × 50 entity = 5000.

**M3 후**: 50m grid + 3×3 cell 기준. 각 client 는 본인 챔프 cell + 인접 8 cell 의 entity 만 받음. 평균 visible entity = 10 → broadcast 1/5 절감.

### 1.3 Vision (★ Anti-cheat 1차 방어)

**LoL 의 핵심 메커닉**:
- 본인 팀 (Blue) 시야: 챔프 sightRange (1100 unit ≈ 11m 게임 단위) + ward + turret + minion sight
- 시야 안 보이는 **적군 챔프 위치 / HP / 스킬 cooldown 모두 client 에 전송 X**
- snapshot filter 가 server 측에서 강제 → client 가 어떤 trick 도 불가능 (maphack 불가)

**M3 의 vision 작업**:
- `VisionComponent` 신규 (sightRange, bIsRevealed, lastSeenTick)
- `VisionSystem` — 매 tick 각 entity 의 visible 상태 갱신
- `SnapshotBuilder` 가 vision filter 적용 — 본 client 팀 시야 안의 entity 만 직렬화

---

## 2. VisionComponent (★ 신규 POD)

`Shared/GameSim/Components/VisionComponent.h`:

```cpp
#pragma once
#include "WintersTypes.h"
#include "GameContext.h"   // eTeam (※ ECS/Components/GameplayComponents.h 의 eTeam 사용)

#include <type_traits>

// ─────────────────────────────────────────────────────────────────
//  VisionComponent  |  시야 emitter / receiver
//
//  Emitter (시야 제공): 챔프, ward, turret, minion. sightRange > 0
//  Receiver (시야 받음): 적군 챔프 — bIsRevealed 가 매 tick 갱신
//
//  Snapshot filter 흐름:
//    1. VisionSystem 이 매 tick 모든 entity 의 bIsRevealed 갱신 (per team)
//    2. SnapshotBuilder 가 client 팀 기준으로 bIsRevealed=true 인 적군만 직렬화
// ─────────────────────────────────────────────────────────────────

enum class eVisionFlag : u32_t
{
    None              = 0,
    EmitsSight        = 1 << 0,    // 본인 시야 emitter (챔프, ward 등)
    AlwaysVisible     = 1 << 1,    // 항상 보임 (turret, structure)
    Stealthed         = 1 << 2,    // 잠수 — sightRange 안에서도 안 보임 (E.g. 사일러스 일부 스킬)
    True_Sight        = 1 << 3,    // True sight (turret 잠수 무시)
};

struct VisionComponent
{
    eTeam   team               = eTeam::Neutral;
    f32_t   sightRange         = 11.f;      // m (LoL 1100 unit ≈ 11m)
    f32_t   trueSightRange     = 0.f;       // turret / control ward 만
    u32_t   flags              = 0;         // eVisionFlag bitmask

    // Per-team visible state — Blue / Red / Neutral 팀 시야에서 보이는지
    //   bit 0 = Blue, bit 1 = Red, bit 2 = Neutral
    u8_t    revealedMask       = 0;

    // Last seen tick — 시야 끊긴 후 fade-out 효과 (LoL 의 "안개에서 사라짐" 시간차)
    u64_t   lastSeenByTeam[3]  = { 0, 0, 0 };

    // Stealth state
    bool_t  bIsStealthed       = false;
};

static_assert(std::is_trivially_copyable_v<VisionComponent>,
    "VisionComponent must be trivially_copyable");

inline bool_t IsRevealedFor(const VisionComponent& vc, eTeam observerTeam)
{
    const u8_t bit = static_cast<u8_t>(observerTeam);
    if (bit > 2) return false;
    return (vc.revealedMask & (1u << bit)) != 0;
}
```

---

## 3. VisionSystem (★ Phase chain 추가)

`Shared/GameSim/Systems/VisionSystem.h`:

```cpp
#pragma once

class CWorld;
struct TickContext;

// ─────────────────────────────────────────────────────────────────
//  CVisionSystem  |  매 tick 시야 emitter ↔ receiver 매트릭스 갱신
//
//  Phase chain 위치 (GameRoom::Phase_SimulationSystems):
//    StatSystem
//    BuffSystem
//    SkillCooldownSystem
//    MoveSystem
//    ProjectileSystem
//    ★ VisionSystem ←  (snapshot 직전)
//    DamageQueueSystem
//    DeathSystem
//
//  Per-tick 작업:
//    1. 모든 entity 의 revealedMask = 0 으로 reset
//    2. 모든 emitter (챔프, ward, turret) 순회
//    3. emitter 의 team 시야 = sightRange 안의 entity 의 revealedMask 에 bit set
//    4. AlwaysVisible flag 가진 entity 는 모든 팀에 set
//    5. Stealthed flag 가진 entity 는 trueSight 만 reveal
//    6. revealedMask 변경 시 lastSeenByTeam[t] 업데이트
// ─────────────────────────────────────────────────────────────────

class CVisionSystem
{
public:
    static void Execute(CWorld& world, const TickContext& tc);

private:
    CVisionSystem() = delete;
};
```

`Shared/GameSim/Systems/VisionSystem.cpp` (★ 핵심부):

```cpp
#include "Shared/GameSim/Systems/VisionSystem.h"
#include "Shared/GameSim/Systems/ICommandExecutor.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator.h"
#include "Shared/GameSim/Components/VisionComponent.h"

#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"

#include <cmath>

namespace
{
    f32_t DistSqXZ(const Vec3& a, const Vec3& b)
    {
        const f32_t dx = a.x - b.x;
        const f32_t dz = a.z - b.z;
        return dx * dx + dz * dz;
    }
}

void CVisionSystem::Execute(CWorld& world, const TickContext& tc)
{
    // ★ 결정성 — sorted EntityID 순회
    auto entities = DeterministicEntityIterator<VisionComponent>::CollectSorted(world);

    // 1. Reset revealedMask
    for (EntityID e : entities)
    {
        auto& vc = world.GetComponent<VisionComponent>(e);

        // AlwaysVisible 은 항상 모든 팀에 보임
        if ((vc.flags & static_cast<u32_t>(eVisionFlag::AlwaysVisible)) != 0)
        {
            vc.revealedMask = 0b111;   // Blue + Red + Neutral
        }
        else
        {
            vc.revealedMask = 0;       // 시야에 의해 set 됨
        }
    }

    // 2. emitter 순회 — 시야 매트릭스 계산
    for (EntityID emitterE : entities)
    {
        const auto& emitter = world.GetComponent<VisionComponent>(emitterE);

        // emitter 가 EmitsSight 아니면 skip
        if ((emitter.flags & static_cast<u32_t>(eVisionFlag::EmitsSight)) == 0) continue;

        if (!world.HasComponent<TransformComponent>(emitterE)) continue;
        const auto& emitterTf = world.GetComponent<TransformComponent>(emitterE);
        const Vec3 emitterPos = emitterTf.GetLocalPosition();

        const f32_t sightSq      = emitter.sightRange     * emitter.sightRange;
        const f32_t trueSightSq  = emitter.trueSightRange * emitter.trueSightRange;
        const u8_t  emitterBit   = static_cast<u8_t>(emitter.team);

        // 3. 모든 receiver 와 거리 검사
        for (EntityID receiverE : entities)
        {
            if (receiverE == emitterE) continue;

            auto& receiver = world.GetComponent<VisionComponent>(receiverE);

            // 같은 팀은 항상 보임 (LoL 룰)
            if (receiver.team == emitter.team)
            {
                receiver.revealedMask |= (1u << emitterBit);
                receiver.lastSeenByTeam[emitterBit] = tc.tickIndex;
                continue;
            }

            if (!world.HasComponent<TransformComponent>(receiverE)) continue;
            const auto& receiverTf = world.GetComponent<TransformComponent>(receiverE);
            const f32_t distSq = DistSqXZ(emitterPos, receiverTf.GetLocalPosition());

            // sightRange 검사
            if (distSq > sightSq) continue;

            // Stealthed 처리
            const bool_t bReceiverStealthed =
                receiver.bIsStealthed
                || (receiver.flags & static_cast<u32_t>(eVisionFlag::Stealthed)) != 0;

            if (bReceiverStealthed)
            {
                // True sight 안에 있어야만 reveal
                if (distSq > trueSightSq) continue;
            }

            // reveal!
            if (emitterBit < 3)
            {
                receiver.revealedMask |= (1u << emitterBit);
                receiver.lastSeenByTeam[emitterBit] = tc.tickIndex;
            }
        }
    }
}
```

---

## 4. AOI Grid (★ 시야 + 대역폭 동시 최적화)

`Shared/GameSim/Spatial/AoiGrid.h`:

```cpp
#pragma once
#include "WintersTypes.h"
#include "WintersMath.h"
#include "ECS/Entity.h"

#include <vector>
#include <unordered_map>

// ─────────────────────────────────────────────────────────────────
//  CAoiGrid  |  50m × 50m cell 기반 entity spatial index
//
//  사용:
//    grid.Rebuild(world);   // 매 tick (또는 5 tick 마다)
//    auto cells = grid.GetCellsAround(observerPos, 1);   // 3×3
//    for (auto& cell : cells)
//        for (EntityID e : cell.entities) { ... }
//
//  ★ 결정성: cell 안 entity 는 sorted (EntityID 순)
// ─────────────────────────────────────────────────────────────────

class CWorld;

struct AoiCell
{
    std::vector<EntityID> entities;   // sorted
};

class CAoiGrid
{
public:
    static constexpr f32_t kCellSize = 50.f;   // m

    void Rebuild(CWorld& world);

    // observerPos 주변 (2*radius+1)^2 cell 의 entity 합집합 — sorted
    std::vector<EntityID> GetEntitiesAround(const Vec3& observerPos, i32_t cellRadius = 1) const;

    // observerPos 가 속한 cell 좌표
    static std::pair<i32_t, i32_t> WorldToCell(const Vec3& pos);

private:
    // (cellX, cellZ) → cell
    std::unordered_map<u64_t, AoiCell> m_cells;

    static u64_t PackCellKey(i32_t cx, i32_t cz);
};
```

`Shared/GameSim/Spatial/AoiGrid.cpp` (★ 핵심부):

```cpp
#include "Shared/GameSim/Spatial/AoiGrid.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator.h"
#include "Shared/GameSim/Components/VisionComponent.h"

#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"

#include <algorithm>
#include <cmath>

std::pair<i32_t, i32_t> CAoiGrid::WorldToCell(const Vec3& pos)
{
    return {
        static_cast<i32_t>(std::floor(pos.x / kCellSize)),
        static_cast<i32_t>(std::floor(pos.z / kCellSize))
    };
}

u64_t CAoiGrid::PackCellKey(i32_t cx, i32_t cz)
{
    return (static_cast<u64_t>(static_cast<u32_t>(cx)) << 32) |
            static_cast<u64_t>(static_cast<u32_t>(cz));
}

void CAoiGrid::Rebuild(CWorld& world)
{
    m_cells.clear();

    // ★ 모든 entity (Vision 가진 것만 — chamipons / wards / turrets / minions / projectiles)
    auto entities = DeterministicEntityIterator<TransformComponent>::CollectSorted(world);
    for (EntityID e : entities)
    {
        const auto& tf = world.GetComponent<TransformComponent>(e);
        const Vec3 pos = tf.GetLocalPosition();
        auto [cx, cz] = WorldToCell(pos);
        m_cells[PackCellKey(cx, cz)].entities.push_back(e);
    }

    // 각 cell 의 entity 정렬 (결정성)
    for (auto& [_, cell] : m_cells)
        std::sort(cell.entities.begin(), cell.entities.end());
}

std::vector<EntityID> CAoiGrid::GetEntitiesAround(const Vec3& observerPos, i32_t cellRadius) const
{
    std::vector<EntityID> result;

    auto [cx, cz] = WorldToCell(observerPos);
    for (i32_t dz = -cellRadius; dz <= cellRadius; ++dz)
    {
        for (i32_t dx = -cellRadius; dx <= cellRadius; ++dx)
        {
            const u64_t key = PackCellKey(cx + dx, cz + dz);
            auto it = m_cells.find(key);
            if (it == m_cells.end()) continue;

            for (EntityID e : it->second.entities)
                result.push_back(e);
        }
    }

    // 합집합 sorted
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}
```

---

## 5. SnapshotBuilder 에 Vision Filter 통합 (★ Anti-cheat 핵심)

`Server/Public/Game/SnapshotBuilder.h` 수정 — 시그니처에 observer 정보 추가:

```cpp
// 기존 Build (RH-1 / 04a v2 D-1H)
flatbuffers::DetachedBuffer Build(
    CWorld& world,
    const EntityIdMap& entityMap,
    u64_t serverTick,
    u64_t rngState,
    u32_t lastAckedSeq,
    NetEntityId yourNetId);

// ★ M3 신규 (vision filter)
flatbuffers::DetachedBuffer BuildFiltered(
    CWorld& world,
    const EntityIdMap& entityMap,
    const CAoiGrid& aoiGrid,
    u64_t serverTick,
    u64_t rngState,
    u32_t lastAckedSeq,
    NetEntityId yourNetId,
    EntityID observerEntity,        // ★ 본 client 의 챔프
    eTeam observerTeam,             // ★ 본 client 의 팀
    u64_t baselineTick = 0);        // ★ delta encoding base
```

`Server/Private/Game/SnapshotBuilder.cpp` 의 BuildFiltered 핵심:

```cpp
flatbuffers::DetachedBuffer CSnapshotBuilder::BuildFiltered(
    CWorld& world,
    const EntityIdMap& entityMap,
    const CAoiGrid& aoiGrid,
    u64_t serverTick, u64_t rngState, u32_t lastAckedSeq,
    NetEntityId yourNetId,
    EntityID observerEntity, eTeam observerTeam,
    u64_t baselineTick)
{
    flatbuffers::FlatBufferBuilder fbb(2048);

    // 1. Observer 위치 확보
    Vec3 observerPos = { 0.f, 0.f, 0.f };
    if (observerEntity != NULL_ENTITY && world.HasComponent<TransformComponent>(observerEntity))
        observerPos = world.GetComponent<TransformComponent>(observerEntity).GetLocalPosition();

    // 2. AOI 후보 entity (3×3 cell)
    auto candidates = aoiGrid.GetEntitiesAround(observerPos, 1);

    // 3. Vision filter — observerTeam 시야 안의 것만
    std::vector<EntityID> visible;
    visible.reserve(candidates.size());
    for (EntityID e : candidates)
    {
        // 본인 (observerEntity) 은 항상 포함
        if (e == observerEntity) { visible.push_back(e); continue; }

        // VisionComponent 없는 entity (예: structure) 는 항상 보임
        if (!world.HasComponent<VisionComponent>(e))
        {
            visible.push_back(e);
            continue;
        }

        const auto& vc = world.GetComponent<VisionComponent>(e);

        // 같은 팀은 항상 보임
        if (vc.team == observerTeam)
        {
            visible.push_back(e);
            continue;
        }

        // 다른 팀 — revealedMask 검사
        if (IsRevealedFor(vc, observerTeam))
            visible.push_back(e);
    }

    // 4. NetEntityId 정렬 (snapshot 결정성)
    struct Tmp { NetEntityId net; EntityID e; };
    std::vector<Tmp> sorted;
    sorted.reserve(visible.size());
    for (EntityID e : visible)
    {
        NetEntityId net = entityMap.ToNet(e);
        if (net == NULL_NET_ENTITY) continue;
        sorted.push_back({ net, e });
    }
    std::sort(sorted.begin(), sorted.end(),
        [](const Tmp& a, const Tmp& b) { return a.net < b.net; });

    // 5. EntitySnapshot 직렬화 (기존 Build 와 동일)
    // ... (기존 코드 — Layer 1 entity 직렬화 + delta 적용)

    // ★ Delta 처리: baselineTick 이 0 이면 full snapshot. 아니면 직전 baseline 과 차분만.
    //   (실 구현은 §6 참조)

    // 6. CreateSnapshot 호출 (kind = SnapshotKind::Filtered or Delta)
    // ...
    return fbb.Release();
}
```

**보안 영향**:
- client 가 packet inspector 로 envelope 까보더라도 **시야 밖 entity 정보 자체가 없음** → maphack 불가능
- 적군 위치 / HP / cooldown 모두 server side filter 통과한 것만 도달

---

## 6. Snapshot Delta Encoding (★ 대역폭)

### 6.1 DeltaBuilder + Baseline ack

`Shared/Schemas/Snapshot.fbs` 갱신:

```fbs
enum SnapshotKind : ubyte {
    Full = 0,
    Delta = 1,
}

table SnapshotEnvelope {
    kind:SnapshotKind;
    serverTick:ulong;
    baselineTick:ulong;     // Delta 면 base. Full 이면 0
    payload:[ubyte];        // Full: Snapshot, Delta: SnapshotDelta
}

table SnapshotDelta {
    addedEntities:[EntitySnapshot];     // 신규 entity (full 직렬화)
    updatedEntities:[EntitySnapshotDelta];  // 변경 entity (필드별 partial)
    removedEntities:[uint];             // 사라진 NetEntityId
    rngState:ulong;
    lastAckedCommandSeq:uint;
}

table EntitySnapshotDelta {
    netId:uint;
    fieldMask:uint;     // 어떤 필드가 변경됐는지 bitmask
    posX:float;
    posY:float;
    posZ:float;
    hp:float;
    // ... (mask 가 set 된 필드만 의미)
}

root_type SnapshotEnvelope;
```

### 6.1.5 Client SnapshotApplier migration (★ Codex P1-2 — root_type 변경 영향)

★ **Codex P1-2 핵심**: `Snapshot.fbs` 의 `root_type` 이 `Snapshot` → `SnapshotEnvelope` 로 바뀌면, 기존 `CSnapshotApplier::OnSnapshot` 의 `VerifySnapshotBuffer` / `GetSnapshot` 호출이 **즉시 깨짐** (root mismatch). 다음 client migration 필수:

#### 6.1.5.1 `Client/Private/Network/Client/SnapshotApplier.cpp` 변경

**수정 전 (M2 까지)**:
```cpp
void CSnapshotApplier::OnSnapshot(CWorld& world, EntityIdMap& entityMap,
    const u8_t* payload, u32_t len)
{
    flatbuffers::Verifier verifier(payload, len);
    if (!Shared::Schema::VerifySnapshotBuffer(verifier)) return;
    const auto* snapshot = Shared::Schema::GetSnapshot(payload);
    // ... (entity loop)
}
```

**수정 후 (M3, ★ Codex P1-2)**:
```cpp
void CSnapshotApplier::OnSnapshot(CWorld& world, EntityIdMap& entityMap,
    const u8_t* payload, u32_t len)
{
    // ★ 1. SnapshotEnvelope verify (M3 신규 root_type)
    flatbuffers::Verifier envVerifier(payload, len);
    if (!Shared::Schema::VerifySnapshotEnvelopeBuffer(envVerifier))
    {
        OutputDebugStringA("[SnapshotApplier] invalid SnapshotEnvelope\n");
        return;
    }

    const auto* env = Shared::Schema::GetSnapshotEnvelope(payload);
    if (!env || !env->payload()) return;

    const u64_t serverTick   = env->serverTick();
    const u64_t baselineTick = env->baselineTick();
    const auto* innerPayload = env->payload();
    const u32_t innerLen     = static_cast<u32_t>(innerPayload->size());
    const u8_t* innerBytes   = innerPayload->data();

    // ★ 2. SnapshotKind 분기
    switch (env->kind())
    {
        case Shared::Schema::SnapshotKind::Full:
            ApplyFullSnapshot(world, entityMap, innerBytes, innerLen);
            m_LastBaselineTick = serverTick;   // 다음 ack 용
            break;

        case Shared::Schema::SnapshotKind::Delta:
            // ★ Delta 가 baselineTick 와 일치해야 적용 가능
            if (baselineTick != m_LastBaselineTick)
            {
                // Out-of-sync — drop. Server 가 다음 tick 에 full resync 예정.
                OutputDebugStringA("[SnapshotApplier] delta baseline mismatch — drop\n");
                return;
            }
            ApplyDeltaSnapshot(world, entityMap, innerBytes, innerLen);
            m_LastBaselineTick = serverTick;
            break;

        default:
            return;
    }

    m_lastServerTick           = serverTick;
    m_pendingAckSnapshotTick   = serverTick;   // ★ 다음 CommandBatch piggyback
}

// ★ 신규 helper — full path
void CSnapshotApplier::ApplyFullSnapshot(CWorld& world, EntityIdMap& entityMap,
    const u8_t* bytes, u32_t len)
{
    flatbuffers::Verifier v(bytes, len);
    if (!Shared::Schema::VerifySnapshotBuffer(v)) return;
    const auto* snap = Shared::Schema::GetSnapshot(bytes);
    if (!snap) return;

    // (기존 OnSnapshot 의 entity loop 본문 그대로)
    // ... if entity not in entityMap → EnsureEntity → spawn
    // ... TransformComponent / HealthComponent / ChampionComponent / StatComponent 갱신
}

// ★ 신규 helper — delta path
void CSnapshotApplier::ApplyDeltaSnapshot(CWorld& world, EntityIdMap& entityMap,
    const u8_t* bytes, u32_t len)
{
    flatbuffers::Verifier v(bytes, len);
    if (!Shared::Schema::VerifySnapshotDeltaBuffer(v)) return;
    const auto* delta = Shared::Schema::GetSnapshotDelta(bytes);
    if (!delta) return;

    // 1. addedEntities — full EntitySnapshot 으로 spawn (ApplyFullSnapshot 의 spawn path 재사용)
    if (const auto* added = delta->addedEntities())
    {
        for (const auto* es : *added) ApplyEntitySnapshot(world, entityMap, es);
    }

    // 2. updatedEntities — fieldMask 기반 partial update
    if (const auto* updated = delta->updatedEntities())
    {
        for (const auto* esd : *updated)
        {
            EntityID e = entityMap.FromNet(esd->netId());
            if (e == NULL_ENTITY) continue;

            const u32_t mask = esd->fieldMask();
            if (mask & 0x01)   // Position bit
            {
                if (world.HasComponent<TransformComponent>(e))
                {
                    auto& tf = world.GetComponent<TransformComponent>(e);
                    tf.SetPosition(Vec3{ esd->posX(), esd->posY(), esd->posZ() });
                }
            }
            if (mask & 0x02)   // HP bit
            {
                if (world.HasComponent<HealthComponent>(e))
                {
                    auto& hp = world.GetComponent<HealthComponent>(e);
                    hp.fCurrent = esd->hp();
                    hp.bIsDead  = (hp.fCurrent <= 0.f);
                }
            }
            // ... (mask 비트 별 분기)
        }
    }

    // 3. removedEntities — entityMap.Unbind + world.DestroyEntity
    if (const auto* removed = delta->removedEntities())
    {
        for (u32_t i = 0; i < removed->size(); ++i)
        {
            const u32_t netId = removed->Get(i);
            EntityID e = entityMap.FromNet(netId);
            if (e != NULL_ENTITY)
            {
                entityMap.Unbind(netId);
                world.DestroyEntity(e);
            }
        }
    }
}
```

#### 6.1.5.2 `CSnapshotApplier.h` 신규 멤버

```cpp
class CSnapshotApplier final
{
public:
    // ★ M3 신규 — CommandBatch piggyback 용
    u64_t  GetPendingAckSnapshotTick() const { return m_pendingAckSnapshotTick; }
    void   ConsumeAckSnapshotTick()          { m_pendingAckSnapshotTick = 0; }

private:
    void ApplyFullSnapshot(CWorld&, EntityIdMap&, const u8_t*, u32_t);
    void ApplyDeltaSnapshot(CWorld&, EntityIdMap&, const u8_t*, u32_t);
    void ApplyEntitySnapshot(CWorld&, EntityIdMap&, const Shared::Schema::EntitySnapshot* es);

    // ... (기존 멤버)
    u64_t m_LastBaselineTick      = 0;   // 마지막 적용 성공한 tick
    u64_t m_pendingAckSnapshotTick = 0;  // 다음 CommandBatch 에 piggyback
};
```

#### 6.1.5.3 `CommandSerializer::SendMove` 변경

```cpp
void CCommandSerializer::SendMove(CClientNetwork& net, const Vec3& groundPos,
    u64_t lastAckedSnapshotTick)   // ★ 신규 매개변수
{
    // ... (기존 wire 생성)

    auto commandsOffset = fbb.CreateVector(commands);
    auto batch = Shared::Schema::CreateCommandBatch(
        fbb, commandsOffset,
        timestampMs,
        lastAckedSnapshotTick);    // ★ M3 신규 piggyback
    fbb.Finish(batch);
    // ... (기존 envelope wrap + send)
}
```

`Scene_InGame.cpp` 의 caller 변경:
```cpp
m_pCommandSerializer->SendMove(*m_pNetwork, ground,
    m_pSnapshotApplier->GetPendingAckSnapshotTick());
m_pSnapshotApplier->ConsumeAckSnapshotTick();
```

#### 6.1.5.4 합격 추가 (★ Codex P1-2)
- ✅ Schema 변경 후 client 가 깨지지 않음 — `VerifySnapshotEnvelopeBuffer` 통과
- ✅ Full path 동작 — 기존 entity 로직 100% 보존
- ✅ Delta path 동작 — fieldMask 기반 partial 적용
- ✅ baselineTick mismatch 시 drop + 다음 server tick 의 full resync 자동 적용
- ✅ CommandBatch 에 piggyback `lastAckedSnapshotTick` 도착 검증

---

### 6.2 Server: Per-client baseline 추적

`Server/Public/Game/PerClientBaseline.h`:

```cpp
struct EntityBaseline
{
    NetEntityId netId;
    Vec3        pos;
    f32_t       hp;
    f32_t       moveSpeed;
    u8_t        championId;
    u8_t        team;
    // ... (Snapshot 의 모든 필드)
};

class CPerClientBaseline
{
public:
    void OnSnapshotAcked(u64_t baselineTick);
    bool_t HasBaseline(u64_t tick) const;
    const std::vector<EntityBaseline>& GetBaseline(u64_t tick) const;
    void StoreSnapshot(u64_t tick, std::vector<EntityBaseline> entities);

    void EvictOlderThan(u64_t threshold);   // 메모리 한도

private:
    // tickIndex → baseline snapshot
    std::unordered_map<u64_t, std::vector<EntityBaseline>> m_baselines;
    u64_t m_lastAckedTick = 0;
};
```

### 6.3 Client: lastAckedSnapshotTick piggyback

CommandBatch envelope 에 `lastAckedSnapshotTick` 필드 추가 — 매 client → server 전송 시 piggyback. server 가 받은 후 PerClientBaseline 의 ack 처리.

```fbs
// Shared/Schemas/Command.fbs 확장
table CommandBatch {
    commands:[CommandPacket];
    clientTimestampMs:ulong;
    lastAckedSnapshotTick:ulong;   // ★ M3 신규
}
```

### 6.4 Idle client AckOnlyHeartbeat

idle client (5초 무조작) 는 CommandBatch 안 보냄 → server 가 baseline ack 못 받음 → delta base 누적 → 메모리 폭증.

→ **AckOnlyHeartbeat 패킷** (5초마다 자동 송신):

```fbs
table AckOnlyHeartbeat {
    lastAckedSnapshotTick:ulong;
}
```

Client 측: idle 5초 마다 heartbeat 송신. Server 측: heartbeat 수신 시 PerClientBaseline 의 ack 만 갱신.

### 6.5 Full-resync fallback

Server 가 baseline 미일치 (client 가 ack 한 baseline 이 메모리에 없음) → 자동 full snapshot 송신. SnapshotKind::Full + baselineTick=0.

---

## 7. LOD — 정밀도 quantization

먼 거리 entity 의 위치 정밀도 낮춰 대역폭 절감:

| 거리 (observer 기준) | 위치 정밀도 | 비트 |
|---|---|---|
| 0~10m | float32 (raw) | 32 bit × 3 = 12B |
| 10~30m | int16 (1/100 m precision) | 16 bit × 3 = 6B |
| 30m+ | int8 (1/10 m precision) | 8 bit × 3 = 3B |

→ 5v5 평균 entity 거리 20m → 6B / entity. 10 entity = 60B. delta 전 = 200B/snapshot 에 도달.

**구현**: `EntitySnapshotLOD` 별도 table 추가 → SnapshotBuilder 가 거리 기반 분기.

---

## 7.5 GameRoom 연결 (★ Codex P1-3 — BuildFiltered broadcast 경로 wiring)

★ **Codex P1-3 핵심**: `BuildFiltered` 시그니처만 정의하면 broadcast 경로에서 호출이 안 됨. `GameRoom` 의 broadcast loop 가 여전히 기존 `Build` 호출. 또 `m_AoiGrid`, `m_PerClientBaselines` 멤버가 GameRoom 에 없음. M3 진입 시 다음 변경 필수:

### 7.5.1 `Server/Public/Game/GameRoom.h` 멤버 추가

```cpp
#include "Shared/GameSim/Spatial/AoiGrid.h"
#include "Server/Public/Game/PerClientBaseline.h"

class CGameRoom
{
    // ... (기존 멤버)

    // ★ M3 신규
    CAoiGrid                                              m_AoiGrid;
    std::unordered_map<u32_t, CPerClientBaseline>         m_PerClientBaselines;   // sessionId → baseline
    // (m_sessionToEntity 는 RH-1 / 04a v2 D-1 에서 이미 존재)
};
```

### 7.5.2 `GameRoom::Phase_BroadcastSnapshot` 본격 변경

**수정 전 (M2 까지)**:
```cpp
void CGameRoom::Phase_BroadcastSnapshot(TickContext& tc)
{
    for (u32_t sid : m_sessionIds)
    {
        // ...
        auto buf = m_pSnapBuilder->Build(
            m_world, m_entityMap,
            tc.tickIndex, m_rng.GetState(),
            /*lastAckedSeq=*/0,
            m_entityMap.ToNet(observerEntity));
        // ... wrap + send
    }
}
```

**수정 후 (M3)**:
```cpp
void CGameRoom::Phase_BroadcastSnapshot(TickContext& tc)
{
    // ★ 1. AOI grid rebuild — 매 tick (또는 5 tick 마다 — 성능 trade-off)
    m_AoiGrid.Rebuild(m_world);

    for (u32_t sid : m_sessionIds)
    {
        auto pSession = CSession_Manager::Get()->Find(sid);   // (M1 부터는 CUdpSession_Manager)
        if (!pSession) continue;

        // ★ 2. Per-session observer 정보 확보
        auto it = m_sessionToEntity.find(sid);
        if (it == m_sessionToEntity.end() || it->second == NULL_ENTITY) continue;
        const EntityID observerEntity = it->second;

        eTeam observerTeam = eTeam::Neutral;
        if (m_world.HasComponent<ChampionComponent>(observerEntity))
            observerTeam = m_world.GetComponent<ChampionComponent>(observerEntity).team;

        // ★ 3. Per-client baseline (delta 용)
        auto& baseline = m_PerClientBaselines[sid];
        const u64_t baselineTick = baseline.GetLastAckedTick();   // 0 이면 full snapshot

        // ★ 4. BuildFiltered 호출 (★ Codex P1-3)
        auto buf = m_pSnapBuilder->BuildFiltered(
            m_world, m_entityMap, m_AoiGrid,
            tc.tickIndex, m_rng.GetState(),
            /*lastAckedSeq=*/0,
            m_entityMap.ToNet(observerEntity),
            observerEntity, observerTeam,
            baselineTick);

        // ★ 5. Baseline 갱신 (이번 tick 의 entity state 저장)
        baseline.StoreSnapshot(tc.tickIndex,
            CollectBaselineEntities(m_world, m_entityMap, m_AoiGrid,
                                     observerEntity, observerTeam));

        // ★ 6. Old baseline evict — 메모리 한도 (60 tick = 2초)
        baseline.EvictOlderThan(tc.tickIndex - 60);

        // 7. Wrap + send
        auto wrapped = WrapEnvelope(
            ePacketType::Snapshot, static_cast<u32_t>(tc.tickIndex),
            buf.data(), static_cast<u32_t>(buf.size()));
        pSession->Send(std::move(wrapped));
        // (M1 부터는 CUdpSession_Manager::Get()->SendTo)
    }
}
```

### 7.5.3 `OnCommandBatch` 의 `lastAckedSnapshotTick` piggyback 처리

CommandBatch.fbs 의 `lastAckedSnapshotTick` 필드 (§6.3) 가 도착하면 baseline 갱신:

```cpp
void CGameRoom::OnCommandBatch(u32_t sessionId, const Shared::Schema::CommandBatch* batch)
{
    // ... (기존 sequence guard / wire push 로직)

    // ★ M3 신규 — baseline ack 처리
    const u64_t ackedTick = batch->lastAckedSnapshotTick();
    if (ackedTick > 0)
    {
        auto it = m_PerClientBaselines.find(sessionId);
        if (it != m_PerClientBaselines.end())
            it->second.OnSnapshotAcked(ackedTick);
    }
}
```

### 7.5.4 `AckOnlyHeartbeat` 처리 (idle client)

`UdpPacketDispatcher` (M1) / `PacketDispatcher` (TCP) 에 `AckOnlyHeartbeat` 디스패치 추가:

```cpp
case ePacketType::AckOnlyHeartbeat:
    DispatchAckOnlyHeartbeat(sessionId, frame.payload.data(), frame.payload.size());
    break;
```

`DispatchAckOnlyHeartbeat` 가 GameRoom 에 ack 만 전달:
```cpp
void CPacketDispatcher::DispatchAckOnlyHeartbeat(u32_t sessionId, const u8_t* payload, u32_t len)
{
    flatbuffers::Verifier v(payload, len);
    if (!Shared::Schema::VerifyAckOnlyHeartbeatBuffer(v)) return;
    const auto* h = Shared::Schema::GetAckOnlyHeartbeat(payload);
    if (auto* room = LookupRoomBySession(sessionId))
        room->OnBaselineAck(sessionId, h->lastAckedSnapshotTick());
}
```

### 7.5.5 합격 추가 (★ Codex P1-3 반영)
- ✅ `m_AoiGrid` / `m_PerClientBaselines` 멤버 추가 + 빌드 통과
- ✅ Phase_BroadcastSnapshot 가 `BuildFiltered` 호출 (Build 의 직접 호출 0 hit)
- ✅ baseline ack piggyback 처리 (CommandBatch + AckOnlyHeartbeat 양쪽)
- ✅ AOI grid Rebuild 매 tick — 5v5 100 entity 시 < 0.1ms

---

## 8. 챔프별 sightRange 초기화 (★ Spawn 시)

`Server/Private/Game/GameRoom.cpp` 의 `SpawnChampion` 에 추가:

```cpp
// (기존 spawn 코드 후)

VisionComponent vision{};
vision.team       = champ.team;
vision.sightRange = 11.f;       // LoL 기본 1100 unit
vision.flags      = static_cast<u32_t>(eVisionFlag::EmitsSight);
m_world.AddComponent<VisionComponent>(entity, vision);
```

다른 entity 별:
- **Turret**: `sightRange = 11.f, trueSightRange = 0.f, flags = EmitsSight | AlwaysVisible` (적 turret 도 항상 미니맵에 보임)
- **Minion**: `sightRange = 9.f`
- **Ward (Stealth)**: `sightRange = 9.f, flags = EmitsSight | Stealthed`
- **Ward (Control)**: `sightRange = 9.f, trueSightRange = 9.f, flags = EmitsSight`
- **Projectile**: `sightRange = 5.f, flags = EmitsSight` (LoL 룰 — 투사체도 시야 제공)

---

## 9. 합격 게이트

### 9.1 대역폭 합격 (Sim-10 v2 §8 M3)
- ✅ 5v5 룸 평균 snapshot < 200B
- ✅ 100 동접 10 룸 부하 시 server outbound < 5 Mbps
- ✅ idle client 5초 후 baseline ack 정상 (heartbeat 동작)
- ✅ baseline 미일치 시 자동 full resync

### 9.2 Vision 합격 (★ Anti-cheat)
- ✅ Client A (Blue) → 시야 안 보이는 Red 챔프 위치 packet inspector 로 보면 0건 (envelope 안에 없음)
- ✅ Stealth ward 가 적 ward 시야 안에 있어도 reveal X (true sight 만 reveal)
- ✅ 시야 emitter 사망 시 다음 tick 부터 receiver 시야 끊김 (lastSeenByTeam 업데이트)
- ✅ AlwaysVisible (turret) 은 항상 broadcast

### 9.3 결정성
- ✅ AOI grid 의 cell 안 entity 정렬 검증 — `std::is_sorted` assert
- ✅ VisionSystem 의 receiver 순회 정렬 검증
- ✅ DeterministicRng 만 사용 (random reveal X)

### 9.4 성능
- ✅ AOI Rebuild — 5v5 100 entity = < 0.1ms
- ✅ VisionSystem 매 tick — N×N (N=100) = < 0.5ms
- ✅ SnapshotBuilder per client — < 0.3ms × 10 client = 3ms < 33ms (30Hz tick)

---

## 10. 위험 / 트레이드오프

| 위험 | 완화 |
|---|---|
| VisionSystem N×N — 1000 entity 시 1M 검사 | AOI grid 통합 — 같은 cell 안만 검사. O(N×k) k=평균 cell 안 entity |
| Delta 의 PerClientBaseline 메모리 폭증 (10 client × 100 entity × 60 baseline = 60K records) | EvictOlderThan(currentTick - 60) — 2초 한도 |
| 시야 luminence flicker — 1 frame on/off 반복 시 시각 깜빡임 | lastSeenByTeam 기반 fade-out (3 tick = 100ms 보존) |
| LOD quantization 시 client 보간 jitter | client 측 lerp + 30Hz baseline 동기 — visual smoothing 흡수 |
| Full-resync 가 자주 발생 시 대역폭 절감 의미 X | 정상 RTT < 100ms 환경에선 < 1% 발생 — 모니터링 필수 |
| Stealth 챔프 (사일러스 일부, Akali, Twitch 등) 의 reveal 룰 복잡 | LoL 1:1 매핑 별도 sub-plan (Stealth.md) |
| Anti-cheat: 클라가 fake CommandBatch 로 lastAckedSnapshotTick spoof | server 측 sequence guard + suspicion ++ |

---

## 11. 후속 작업

- **Wards 시스템** — control / stealth ward placement (LoL Trinket / Sweeper). Item 시스템 통합 후
- **Bush** — 수풀 안 챔프는 시야 emitter 가 같은 수풀 안에 있어야만 reveal. 별도 BushComponent
- **Brush vision rules** — LoL 의 복잡한 수풀 시야 룰 (vision oracle / sweeper / control ward)
- **Replay 시스템** — Delta + baseline 으로 server tick 단위 replay. M5 Client Prediction 의 prerequisite

---

## 12. 한 줄

**M3 = Snapshot Delta encoding (대역폭 80% 절감) + AOI Grid (50m × 50m, 3×3 cell) + Vision (per-team fog-of-war, anti-cheat 1차 방어선). 3개 ECS unit (VisionComponent / VisionSystem / CAoiGrid) + SnapshotBuilder::BuildFiltered + PerClientBaseline + AckOnlyHeartbeat + Full-resync fallback + LOD quantization. LoL 의 핵심 — "시야 안 보이는 적 정보 client 에 0 leak". 합격 = 5v5 < 200B/snapshot + 시야 외 entity 0 broadcast 검증 (packet inspector) + 100 동접 < 5 Mbps.**
