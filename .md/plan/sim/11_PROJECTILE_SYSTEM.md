# Phase Sim-11 — Projectile Synchronization Plan

**작성일**: 2026-04-30
**상위 문서**: `00_SHAREDGAMESIM_MASTER_PLAN.md` + `10_UDP_LOL_NETSTACK_MASTER_v2.md`
**전제**: Sim-10 v2 M1 (UDP transport) 합격 후 — 또는 04a v2 TCP MVP 환경에서 시작 가능
**범위**: 서버 권위 투사체 (Ezreal Q / Lux Q / Yasuo Q tornado / Kalista Q rend 등) — 생성 / 이동 / 충돌 / hit dispatch 전 사이클
**합격**: client 우클릭 또는 Q → server 가 ProjectileEntity spawn → 매 tick 위치 적분 + 충돌 검사 → snapshot broadcast → client 시각화 + hit FX

**한 줄**: **투사체 = ECS entity (수명 짧은 동적 객체) + server 권위 spawn/move/hit + snapshot 통합 + client visual smoothing. ProjectileComponent + ProjectileSystem 2개 단위 + Snapshot 확장.**

---

## 0. 투사체 model — 3가지 패턴

LoL 챔프 스킬을 분류:

| 패턴 | 예시 | 모델 |
|---|---|---|
| **Skillshot (line)** | Ezreal Q (Mystic Shot), Lux Q (Light Binding), Morgana Q | 직선 투사체. 첫 hit 후 소멸 또는 piercing |
| **Skillshot (area target)** | Lux R, Karthus R, Annie R (AoE) | 위치 지정 후 즉시 / 지연 후 폭발. **투사체 X, 별도 ScheduledHit** (Phase 11+) |
| **Targeted instant** | Annie Q, Garen E (Spin), Ezreal Auto Attack (homing) | target lock + 시간 후 damage. 본 plan 의 **Homing Projectile** 변형 |

**본 plan 범위**: Skillshot (line) + Homing Projectile. AoE 즉시는 별도 plan.

**예시 챔프 매핑**:
- **Ezreal Q (Mystic Shot)**: line skillshot. 첫 적 hit 시 damage + cooldown 절감
- **Ezreal Auto Attack**: homing projectile. target lock + 거리 기반 도달 시간
- **Lux Q (Light Binding)**: line piercing — 2 적까지 stun
- **Yasuo Q tornado (Q3 회오리)**: line skillshot, 첫 hit airborne
- **Kalista Q (Pierce)**: line skillshot, hit + 박힘 stack

---

## 1. ProjectileComponent (★ 신규, ECS POD — Codex P1-4 보정)

`Shared/GameSim/Components/ProjectileComponent.h`:

★ **Codex P1-4 보정**: 기존 `Shared/GameSim/Components/ProjectileKindComponent.h:5` 에 이미 `enum class eProjectileKind : uint16_t { Generic, Wind, Tornado, EQRing, MysticShot, EssenceFlux, GlobalBeam }` 존재 — **재정의 시 컴파일 에러**. 신규 categorical enum 은 별도 이름 `eProjectileMotion` 사용. 두 enum 을 조합 (kind = 시각 자산, motion = 운동/충돌 분류).

```cpp
#pragma once
#include "WintersTypes.h"
#include "WintersMath.h"
#include "ECS/Entity.h"
#include "GameContext.h"

// ★ Codex P1-4 — 기존 eProjectileKind 재사용 (재정의 X)
#include "Shared/GameSim/Components/ProjectileKindComponent.h"

#include <type_traits>

// ─────────────────────────────────────────────────────────────────
//  ProjectileComponent  |  서버 권위 투사체
//
//  생성: server 가 CastSkill 처리 시 spawn (ProjectileSystem)
//  소멸: hit / maxRange / maxLifetimeSec / target out-of-bounds
//  Snapshot: 별도 entity 로 broadcast (ChampionComponent 와 같은 layer)
//
//  ★ Codex P1-4 — 두 enum 분리:
//    - eProjectileKind   (기존 ProjectileKindComponent.h, uint16_t):
//        시각/자산 분류 — Generic, Wind, Tornado, EQRing, MysticShot,
//                        EssenceFlux, GlobalBeam, ...
//    - eProjectileMotion (본 plan 신규, u8_t):
//        운동/충돌 분류 — LineSkillshot, HomingProjectile, ...
//    조합 예: kind=MysticShot + motion=LineSkillshot (Ezreal Q)
//             kind=EssenceFlux + motion=LineSkillshot (Ezreal W)
//             kind=Tornado + motion=TornadoLine (Yasuo Q3)
// ─────────────────────────────────────────────────────────────────

// ★ 신규 — 운동/충돌 분류 (eProjectileKind 와 별도)
enum class eProjectileMotion : u8_t
{
    None = 0,
    LineSkillshot,           // Ezreal Q (Mystic Shot), Lux Q (single hit)
    HomingProjectile,        // Auto Attack — target lock, 일직선 또는 추적
    PiercingLineSkillshot,   // Lux Q (2 적 stun)
    TornadoLine,             // Yasuo Q3 — line + airborne hit
};

enum class eProjectileSource : u8_t
{
    None = 0,
    BasicAttack,
    SkillQ,
    SkillW,
    SkillE,
    SkillR,
};

struct ProjectileComponent
{
    // 발사자 / 소속
    EntityID            ownerEntity     = NULL_ENTITY;
    eTeam               ownerTeam       = eTeam::Blue;
    eProjectileKind     kind            = eProjectileKind::Generic;     // ★ 시각 자산 (기존 enum)
    eProjectileMotion   motion          = eProjectileMotion::None;      // ★ 운동/충돌 (신규 enum)
    eProjectileSource   source          = eProjectileSource::None;
    eChampion           ownerChampion   = eChampion::NONE;
    u8_t                slot            = 0;     // BA=0, Q=1, W=2, E=3, R=4

    // 운동
    Vec3                originPos       = { 0.f, 0.f, 0.f };   // 발사 위치
    Vec3                direction       = { 0.f, 0.f, 1.f };   // unit vector (xz plane)
    f32_t               speed           = 18.f;  // m/s
    f32_t               maxRange        = 22.f;  // m (Ezreal Q 기준)
    f32_t               traveledDist    = 0.f;
    f32_t               radius          = 0.4f;  // 충돌 반경 (m)
    f32_t               maxLifetimeSec  = 3.f;   // 안전 한도

    // 데미지 / 효과
    f32_t               adRatio         = 1.1f;  // 0.0 ~ 2.0
    f32_t               apRatio         = 0.4f;
    f32_t               flatBaseDamage  = 0.f;
    bool_t              bIsCritical     = false;

    // Homing 전용
    EntityID            targetEntity    = NULL_ENTITY;
    f32_t               turnRateRad     = 0.f;   // 0 = 일직선 (homing 아님)

    // Piercing 전용
    u8_t                maxPierceCount  = 1;
    u8_t                currentPierceCount = 0;

    // hit cache (한 entity 두 번 hit 방지)
    static constexpr u32_t kMaxHitCacheSize = 8;
    EntityID            hitCache[kMaxHitCacheSize] = {};
    u8_t                hitCacheCount   = 0;

    // 상태
    f32_t               lifetimeElapsed = 0.f;
    bool_t              bShouldDestroy  = false;
};

static_assert(std::is_trivially_copyable_v<ProjectileComponent>,
    "ProjectileComponent must be trivially_copyable for sim determinism");
```

---

## 2. ProjectileSystem (★ 신규, Phase chain 추가)

`Shared/GameSim/Systems/ProjectileSystem.h`:

```cpp
#pragma once

class CWorld;
struct TickContext;

// ─────────────────────────────────────────────────────────────────
//  CProjectileSystem  |  투사체 운동 + 충돌 검사
//
//  Phase chain 위치 (GameRoom::Phase_SimulationSystems):
//    StatSystem
//    BuffSystem
//    SkillCooldownSystem
//    MoveSystem
//    ★ ProjectileSystem ←
//    DamageQueueSystem
//    DeathSystem
//
//  Per-tick 작업:
//    1. lifetimeElapsed += dt, lifetime 초과 → destroy
//    2. position += direction * speed * dt
//    3. traveledDist += step
//    4. maxRange 초과 → destroy
//    5. Homing 시 turnRate 적용
//    6. 모든 적 entity 와 충돌 검사 (sphere-sphere)
//    7. hit 시:
//       - DamageRequestComponent 큐 push
//       - hitCache 에 추가
//       - Piercing 아니면 destroy
// ─────────────────────────────────────────────────────────────────

class CProjectileSystem
{
public:
    static void Execute(CWorld& world, const TickContext& tc);

private:
    CProjectileSystem() = delete;
};
```

`Shared/GameSim/Systems/ProjectileSystem.cpp` (★ 핵심부):

```cpp
#include "Shared/GameSim/Systems/ProjectileSystem.h"
#include "Shared/GameSim/Systems/ICommandExecutor.h"   // TickContext
#include "Shared/GameSim/Systems/DeterministicEntityIterator.h"
#include "Shared/GameSim/Components/ProjectileComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/DamageRequestComponent.h"

#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"

#include <cmath>
#include <vector>

namespace
{
    bool_t IsAlreadyHit(const ProjectileComponent& proj, EntityID e)
    {
        for (u8_t i = 0; i < proj.hitCacheCount; ++i)
            if (proj.hitCache[i] == e) return true;
        return false;
    }

    void AddHit(ProjectileComponent& proj, EntityID e)
    {
        if (proj.hitCacheCount < ProjectileComponent::kMaxHitCacheSize)
            proj.hitCache[proj.hitCacheCount++] = e;
    }
}

void CProjectileSystem::Execute(CWorld& world, const TickContext& tc)
{
    auto entities = DeterministicEntityIterator<ProjectileComponent>::CollectSorted(world);

    // 충돌 후보 (sorted EntityID — 결정성)
    auto champs = DeterministicEntityIterator<ChampionComponent>::CollectSorted(world);

    std::vector<EntityID> toDestroy;

    for (EntityID e : entities)
    {
        auto& proj = world.GetComponent<ProjectileComponent>(e);
        if (proj.bShouldDestroy) { toDestroy.push_back(e); continue; }

        // 1. lifetime
        proj.lifetimeElapsed += tc.fDt;
        if (proj.lifetimeElapsed >= proj.maxLifetimeSec)
        {
            proj.bShouldDestroy = true;
            toDestroy.push_back(e);
            continue;
        }

        // 2. 위치 적분
        if (!world.HasComponent<TransformComponent>(e)) { toDestroy.push_back(e); continue; }
        auto& tf = world.GetComponent<TransformComponent>(e);
        const Vec3 pos = tf.GetLocalPosition();   // ★ Codex P1-3 일관 (CTransformSystem 미포함)

        // Homing: target 방향으로 turn
        if (proj.motion == eProjectileMotion::HomingProjectile && proj.targetEntity != NULL_ENTITY
            && world.HasComponent<TransformComponent>(proj.targetEntity))
        {
            const auto& tgtTf = world.GetComponent<TransformComponent>(proj.targetEntity);
            const Vec3 tgtPos = tgtTf.GetLocalPosition();
            const Vec3 toTgt{ tgtPos.x - pos.x, 0.f, tgtPos.z - pos.z };
            const f32_t distToTgt = std::sqrtf(toTgt.x * toTgt.x + toTgt.z * toTgt.z);
            if (distToTgt > 0.001f)
            {
                const Vec3 desiredDir{ toTgt.x / distToTgt, 0.f, toTgt.z / distToTgt };
                // 단순 lerp — turnRate 무시 (M2 부터 본격)
                proj.direction = desiredDir;
            }
        }

        const f32_t step = proj.speed * tc.fDt;
        const Vec3 newPos{
            pos.x + proj.direction.x * step,
            pos.y,
            pos.z + proj.direction.z * step
        };
        tf.SetPosition(newPos);
        proj.traveledDist += step;

        // 3. maxRange
        if (proj.traveledDist >= proj.maxRange)
        {
            proj.bShouldDestroy = true;
            toDestroy.push_back(e);
            continue;
        }

        // 4. 충돌 검사 (sphere-sphere, sorted iter — 결정성)
        for (EntityID champE : champs)
        {
            if (champE == proj.ownerEntity) continue;     // 자기 자신 hit X
            if (IsAlreadyHit(proj, champE)) continue;     // 중복 hit X
            if (!world.HasComponent<TransformComponent>(champE)) continue;
            if (!world.HasComponent<HealthComponent>(champE))   continue;

            const auto& champ = world.GetComponent<ChampionComponent>(champE);
            if (champ.team == proj.ownerTeam) continue;   // 같은 팀 hit X

            const auto& hp = world.GetComponent<HealthComponent>(champE);
            if (hp.bIsDead) continue;

            const auto& champTf = world.GetComponent<TransformComponent>(champE);
            const Vec3 cpos = champTf.GetLocalPosition();
            const f32_t dx = cpos.x - newPos.x;
            const f32_t dz = cpos.z - newPos.z;
            const f32_t distSq = dx * dx + dz * dz;
            const f32_t hitRadiusSum = proj.radius + 0.6f;   // 챔피언 hitbox 0.6m 가정
            if (distSq <= hitRadiusSum * hitRadiusSum)
            {
                // hit!
                AddHit(proj, champE);

                // damage request emit
                if (!world.HasComponent<DamageRequestComponent>(champE))
                    world.AddComponent<DamageRequestComponent>(champE, DamageRequestComponent{});
                auto& dmg = world.GetComponent<DamageRequestComponent>(champE);
                // 단순 합산 — DamagePipeline 이 처리
                dmg.amount      += proj.flatBaseDamage;     // M1 = flat 만. M2 부터 ad/ap ratio 본격
                dmg.sourceEntity = proj.ownerEntity;
                dmg.bIsPhysical  = (proj.source == eProjectileSource::BasicAttack
                                   || proj.source == eProjectileSource::SkillQ);
                dmg.bIsCritical  = proj.bIsCritical;

                ++proj.currentPierceCount;
                if (proj.currentPierceCount >= proj.maxPierceCount)
                {
                    proj.bShouldDestroy = true;
                    break;
                }
            }
        }

        if (proj.bShouldDestroy)
            toDestroy.push_back(e);
    }

    // entity 삭제는 후처리 (iter 중 X)
    for (EntityID e : toDestroy)
        world.DestroyEntity(e);
}
```

---

## 3. CommandExecutor 확장 (★ HandleCastSkill)

`Shared/GameSim/Systems/CommandExecutor.cpp` 의 `HandleCastSkill` 에 투사체 spawn 추가:

```cpp
// HandleCastSkill — RH-1 / 04a v2 D-0B 의 cooldown-only accept 에 spawn 추가
//
// ★ Codex P1-5 보정: pDef==nullptr 일 때 early return 하면 비투사체 스킬
//   (Annie Q/W, Garen E, 일부 Yasuo W 등) 의 cooldown set 가 누락되어 회귀.
//   해결: cooldown 은 항상 set (placeholder 또는 SkillDef 기반), projectile spawn 은
//   pDef 가 있을 때만 선택적으로 수행.
void CDefaultCommandExecutor::HandleCastSkill(CWorld& world, const TickContext& tc, const GameCommand& cmd)
{
    (void)tc;

    if (!world.HasComponent<SkillStateComponent>(cmd.issuerEntity)) return;
    if (cmd.slot >= 5) return;

    auto& sk = world.GetComponent<SkillStateComponent>(cmd.issuerEntity);
    if (sk.slots[cmd.slot].cooldownRemaining > 0.f) return;

    // ★ Codex P1-5 — Cooldown 항상 set (RH-1 / 04a v2 D-0B 의 cooldown-only accept 보존).
    //   투사체 정의 (pDef) 가 있으면 그 cooldownSec 사용, 없으면 기본 6.f placeholder.
    //   (Layer 2 에서 SkillDef.cooldownSec lookup 으로 정확한 값 사용)
    f32_t cooldownSec = 6.f;   // RH-1 placeholder

    if (!world.HasComponent<ChampionComponent>(cmd.issuerEntity))
    {
        sk.slots[cmd.slot].cooldownRemaining = cooldownSec;
        return;
    }
    const auto& champ = world.GetComponent<ChampionComponent>(cmd.issuerEntity);

    // 챔프 / slot 별 투사체 정의 lookup
    const ProjectileSpawnDef* pDef = ProjectileSpawnRegistry::Lookup(champ.id, cmd.slot);
    if (pDef)
        cooldownSec = pDef->cooldownSec;   // 투사체 스킬의 정확한 cooldown

    // ★ Cooldown 은 spawn 성공 여부와 무관하게 set (Codex P1-5)
    sk.slots[cmd.slot].cooldownRemaining = cooldownSec;

    // 투사체 정의 없음 → 비투사체 스킬 (cooldown 만 set 하고 종료)
    if (!pDef) return;

    // ── Projectile spawn (pDef 있을 때만) ──
    if (!world.HasComponent<TransformComponent>(cmd.issuerEntity)) return;
    const auto& issuerTf = world.GetComponent<TransformComponent>(cmd.issuerEntity);
    const Vec3 origin = issuerTf.GetLocalPosition();

    // direction = cmd.direction (client 가 보낸 마우스 방향) 또는 origin → groundPos
    Vec3 dir = cmd.direction;
    if (std::abs(dir.x) + std::abs(dir.z) < 0.001f)
    {
        // direction 미명시 시 groundPos 로 추정
        dir = { cmd.groundPos.x - origin.x, 0.f, cmd.groundPos.z - origin.z };
    }
    const f32_t dirLen = std::sqrtf(dir.x * dir.x + dir.z * dir.z);
    if (dirLen < 0.001f) return;   // 방향 추정 실패 — cooldown 은 이미 set 됨
    dir.x /= dirLen; dir.z /= dirLen; dir.y = 0.f;

    EntityID projE = world.CreateEntity();

    TransformComponent projTf{};
    projTf.SetPosition(origin);
    world.AddComponent<TransformComponent>(projE, projTf);

    ProjectileComponent proj{};
    proj.ownerEntity     = cmd.issuerEntity;
    proj.ownerTeam       = champ.team;
    proj.ownerChampion   = champ.id;
    proj.kind            = pDef->kind;       // ★ 시각 자산 (eProjectileKind, 기존 enum)
    proj.motion          = pDef->motion;     // ★ 운동/충돌 (eProjectileMotion, 신규 enum)
    proj.source          = pDef->source;
    proj.slot            = cmd.slot;
    proj.originPos       = origin;
    proj.direction       = dir;
    proj.speed           = pDef->speed;
    proj.maxRange        = pDef->maxRange;
    proj.radius          = pDef->radius;
    proj.maxLifetimeSec  = pDef->maxLifetimeSec;
    proj.flatBaseDamage  = pDef->flatBaseDamage;
    proj.adRatio         = pDef->adRatio;
    proj.apRatio         = pDef->apRatio;
    proj.maxPierceCount  = pDef->maxPierceCount;
    proj.targetEntity    = (cmd.targetEntity != NULL_ENTITY) ? cmd.targetEntity : NULL_ENTITY;
    world.AddComponent<ProjectileComponent>(projE, proj);

    // ★ NetEntityId 발급 — snapshot broadcast 용
    if (tc.pEntityMap)
        tc.pEntityMap->IssueNew(projE);
}
```

---

## 4. ProjectileSpawnRegistry — 챔프/slot → 투사체 정의

`Shared/GameSim/Registries/ProjectileSpawnRegistry.h`:

```cpp
#pragma once
#include "Shared/GameSim/Components/ProjectileComponent.h"
#include "GameContext.h"   // eChampion

struct ProjectileSpawnDef
{
    eProjectileKind   kind;             // ★ 시각 자산 (기존 enum, ProjectileKindComponent.h)
    eProjectileMotion motion;           // ★ 운동/충돌 분류 (신규 enum)
    eProjectileSource source;
    f32_t             speed;            // m/s
    f32_t             maxRange;         // m
    f32_t             radius;           // m
    f32_t             maxLifetimeSec;
    f32_t             flatBaseDamage;
    f32_t             adRatio;
    f32_t             apRatio;
    f32_t             cooldownSec;
    u8_t              maxPierceCount;
};

class ProjectileSpawnRegistry
{
public:
    // 챔프 + slot (BA=0, Q=1, ...) → 투사체 정의. 없으면 nullptr
    static const ProjectileSpawnDef* Lookup(eChampion champ, u8_t slot);
};
```

`Shared/GameSim/Registries/ProjectileSpawnRegistry.cpp`:

```cpp
#include "Shared/GameSim/Registries/ProjectileSpawnRegistry.h"

namespace
{
    // 챔프별 투사체 테이블 (LoL 1:1 매핑)
    struct Entry
    {
        eChampion champ;
        u8_t      slot;
        ProjectileSpawnDef def;
    };

    // ★ Codex P1-4 — kind = 시각 자산 (기존 eProjectileKind), motion = 운동/충돌 (신규 eProjectileMotion)
    constexpr Entry kTable[] = {
        // Ezreal Q (Mystic Shot) — line skillshot
        { eChampion::EZREAL, 1,
            { eProjectileKind::MysticShot, eProjectileMotion::LineSkillshot, eProjectileSource::SkillQ,
              20.f, 22.f, 0.6f, 2.f, 35.f, 1.1f, 0.4f, 6.f, 1 } },
        // Ezreal BA — homing projectile (시각 = Generic 또는 별도 자산 추가)
        { eChampion::EZREAL, 0,
            { eProjectileKind::Generic, eProjectileMotion::HomingProjectile, eProjectileSource::BasicAttack,
              25.f, 25.f, 0.4f, 2.f, 0.f, 1.0f, 0.0f, 0.f, 1 } },

        // Lux Q (Light Binding) — piercing line skillshot, 2 적 (eChampion 에 LUX 추가 후 매핑)
        // { eChampion::LUX, 1,
        //     { eProjectileKind::Generic, eProjectileMotion::PiercingLineSkillshot, eProjectileSource::SkillQ,
        //       16.f, 26.f, 0.5f, 3.f, 50.f, 0.0f, 0.7f, 9.f, 2 } },

        // Yasuo Q3 tornado — line skillshot, airborne hit
        { eChampion::YASUO, 1,
            { eProjectileKind::Tornado, eProjectileMotion::TornadoLine, eProjectileSource::SkillQ,
              22.f, 12.f, 0.7f, 1.5f, 30.f, 1.0f, 0.0f, 4.f, 5 } },

        // Kalista Q (Pierce) — line skillshot
        { eChampion::KALISTA, 1,
            { eProjectileKind::Generic, eProjectileMotion::LineSkillshot, eProjectileSource::SkillQ,
              24.f, 12.f, 0.4f, 1.5f, 40.f, 1.4f, 0.0f, 8.f, 1 } },
    };
}

const ProjectileSpawnDef* ProjectileSpawnRegistry::Lookup(eChampion champ, u8_t slot)
{
    for (const auto& e : kTable)
        if (e.champ == champ && e.slot == slot) return &e.def;
    return nullptr;
}
```

---

## 5. Snapshot 확장 — ProjectileSnapshot

`Shared/Schemas/Snapshot.fbs` 추가:

```fbs
// (기존 EntitySnapshot 옆에 신규 ProjectileSnapshot 추가)
// ★ Codex P1-4 — kind = ushort (eProjectileKind, 기존 uint16_t enum), motion = ubyte (eProjectileMotion)
table ProjectileSnapshot {
    netId:uint;
    ownerNetId:uint;       // 발사자 NetEntityId
    kind:ushort;           // eProjectileKind (시각 자산 — 기존 enum)
    motion:ubyte;          // eProjectileMotion (운동/충돌 — 신규)
    source:ubyte;          // eProjectileSource
    posX:float;
    posY:float;
    posZ:float;
    dirX:float;
    dirY:float;
    dirZ:float;
    speed:float;
    radius:float;
    traveledDist:float;
    maxRange:float;
}

table Snapshot {
    serverTick:ulong;
    serverTimeMs:ulong;
    rngState:ulong;
    entities:[EntitySnapshot];
    projectiles:[ProjectileSnapshot];   // ★ 신규
    lastAckedCommandSeq:uint;
    yourNetId:uint;
    deltaBaseTick:ulong;
}
```

→ flatc 재생성 (`Shared/Schemas/run_codegen.bat`).

`Server/Private/Game/SnapshotBuilder.cpp` 확장:

```cpp
// 기존 EntitySnapshot 직렬화 후, ProjectileSnapshot 추가
auto projEntities = DeterministicEntityIterator<ProjectileComponent>::CollectSorted(world);

std::vector<flatbuffers::Offset<Shared::Schema::ProjectileSnapshot>> projVec;
projVec.reserve(projEntities.size());

for (EntityID pe : projEntities)
{
    NetEntityId pNet = entityMap.ToNet(pe);
    if (pNet == NULL_NET_ENTITY) continue;

    const auto& proj = world.GetComponent<ProjectileComponent>(pe);
    if (!world.HasComponent<TransformComponent>(pe)) continue;
    const auto& tf = world.GetComponent<TransformComponent>(pe);
    const Vec3 pos = tf.GetLocalPosition();

    NetEntityId ownerNet = entityMap.ToNet(proj.ownerEntity);

    projVec.push_back(Shared::Schema::CreateProjectileSnapshot(
        fbb, pNet, ownerNet,
        static_cast<u16_t>(proj.kind),       // ★ kind = ushort (uint16_t enum)
        static_cast<u8_t>(proj.motion),      // ★ motion 신규
        static_cast<u8_t>(proj.source),
        pos.x, pos.y, pos.z,
        proj.direction.x, proj.direction.y, proj.direction.z,
        proj.speed, proj.radius, proj.traveledDist, proj.maxRange));
}

auto projOffset = fbb.CreateVector(projVec);

// CreateSnapshot 호출 시 projOffset 인자 추가
```

---

## 6. Client SnapshotApplier 확장 — Projectile spawn / update

`Client/Private/Network/Client/SnapshotApplier.cpp` 의 `OnSnapshot` 에 추가:

```cpp
// (기존 EntitySnapshot 처리 후)

if (const auto* projs = snapshot->projectiles())
{
    for (const auto* ps : *projs)
    {
        if (!ps || ps->netId() == NULL_NET_ENTITY) continue;

        const u32_t netId = ps->netId();
        EntityID e = entityMap.FromNet(netId);

        // 새 투사체 → spawn
        if (e == NULL_ENTITY)
        {
            if (!m_onNewProjectile) continue;
            e = m_onNewProjectile(
                netId, ps->ownerNetId(),
                static_cast<eProjectileKind>(ps->kind()),       // ★ uint16_t enum
                static_cast<eProjectileMotion>(ps->motion()),   // ★ 신규
                static_cast<eProjectileSource>(ps->source()));
            if (e == NULL_ENTITY) continue;
            entityMap.Bind(netId, e);
        }

        // 기존 투사체 → 위치 갱신
        if (world.HasComponent<TransformComponent>(e))
        {
            auto& tf = world.GetComponent<TransformComponent>(e);
            tf.SetPosition(Vec3{ ps->posX(), ps->posY(), ps->posZ() });
        }
        if (world.HasComponent<ProjectileComponent>(e))
        {
            auto& proj = world.GetComponent<ProjectileComponent>(e);
            proj.direction = Vec3{ ps->dirX(), ps->dirY(), ps->dirZ() };
            proj.speed = ps->speed();
            proj.traveledDist = ps->traveledDist();
        }
    }

    // 서버 snapshot 에 없는 client 측 투사체는 destroy (서버에서 hit 또는 maxRange 도달)
    // → 별도 cleanup 패스 필요 (m_seenProjectileNetIds 와 비교)
}
```

`SnapshotApplier.h` 에 `OnNewProjectileFn` callback 추가:

```cpp
using OnNewProjectileFn = std::function<EntityID(
    u32_t netId, u32_t ownerNetId,
    eProjectileKind kind, eProjectileMotion motion, eProjectileSource source)>;

void SetOnNewProjectileCallback(OnNewProjectileFn fn) { m_onNewProjectile = std::move(fn); }
```

---

## 7. Client Visual — Scene 측 callback + visual smoothing

Scene_InGame `OnEnter()` 에 callback 등록:

```cpp
if (m_pSnapshotApplier)
{
    m_pSnapshotApplier->SetOnNewProjectileCallback(
        [this](u32_t netId, u32_t ownerNetId,
               eProjectileKind kind, eProjectileMotion motion, eProjectileSource source) -> EntityID
        {
            EntityID e = m_World.CreateEntity();

            // Visual: 챔프 / source / kind 별 mesh / FX
            // 예: kind=MysticShot + motion=LineSkillshot (Ezreal Q) = 작은 빛 구슬 (FxBillboardComponent)
            //     kind=Tornado + motion=TornadoLine (Yasuo Q3) = 회오리 mesh + particle

            (void)netId; (void)ownerNetId; (void)motion;
            CreateProjectileVisual(e, kind, source);   // Scene 헬퍼

            return e;
        });
}
```

`CreateProjectileVisual` 은 챔프별 FX preset 호출 (Phase D Effect Tool 활용).

### 7.1 Visual smoothing (★ 30Hz snapshot vs 60+Hz 렌더)

서버 30Hz tick = 매 33ms 위치 갱신. 60Hz 렌더 = 매 16ms. **2 frame 마다 1번만 위치 변화** → 시각적 jitter.

해결: client 측 lerp + extrapolation.

```cpp
// Scene_InGame::OnUpdate 에 추가:
void CScene_InGame::SmoothProjectiles(f32_t dt)
{
    m_World.ForEach<ProjectileComponent>(
        std::function<void(EntityID, ProjectileComponent&)>(
            [&](EntityID e, ProjectileComponent& proj)
            {
                if (!m_World.HasComponent<TransformComponent>(e)) return;
                auto& tf = m_World.GetComponent<TransformComponent>(e);
                const Vec3 cur = tf.GetLocalPosition();

                // direction * speed 로 extrapolate (다음 snapshot 도달 전까지)
                const Vec3 next{
                    cur.x + proj.direction.x * proj.speed * dt,
                    cur.y,
                    cur.z + proj.direction.z * proj.speed * dt
                };
                tf.SetPosition(next);
            }));
}
```

★ 단, 다음 snapshot 도착 시 server 의 위치로 **rewind / snap** — 매 snapshot 의 권위 = server.

---

## 8. Hit FX + Sound (★ Layer 2 — Event 기반, Codex P2-6 보정)

★ **Codex P2-6 보정**: 기존 `Shared/Schemas/Event.fbs` 의 `EventKind` 에 `ProjectileHit = 8` 은 있지만 `EventPacket` table 에는 `projectile:ProjectileSpawnEvent` 만 있고 `projectileHit` 필드가 없음. **즉 client 가 ProjectileHitEvent 를 역직렬화 못함**. table 추가 + EventPacket 필드 추가 + dispatch path 갱신 모두 필요.

### 8.1 `Shared/Schemas/Event.fbs` 갱신 (★ 신규 + 기존 갱신)

**수정 전 (현재 — Codex 검증)**:
```fbs
table EventPacket {
    kind:EventKind;
    serverTick:ulong;
    damage:DamageEvent;
    buffApply:BuffApplyEvent;
    projectile:ProjectileSpawnEvent;     // ← spawn 만
    skillCast:SkillCastEvent;
}
```

**수정 후 (★ Codex P2-6)**:
```fbs
// 신규 table
table ProjectileHitEvent {
    projectileNetId:uint;
    targetNetId:uint;
    hitX:float;
    hitY:float;
    hitZ:float;
    damage:float;
    bIsCritical:bool;
}

// 기존 EventPacket 확장 — projectileHit 필드 추가
table EventPacket {
    kind:EventKind;
    serverTick:ulong;
    damage:DamageEvent;
    buffApply:BuffApplyEvent;
    projectile:ProjectileSpawnEvent;
    projectileHit:ProjectileHitEvent;    // ★ Codex P2-6 신규 필드
    skillCast:SkillCastEvent;
}
```

→ flatc 재생성 (`Shared/Schemas/run_codegen.bat`).

### 8.2 Server emit (CProjectileSystem 의 hit 분기)

`Shared/GameSim/Systems/ProjectileSystem.cpp` 의 hit 처리부에 EventBatch push 추가:

```cpp
// hit 발생 직후 (DamageRequestComponent 추가 후):

if (tc.pEventBatch)   // ★ TickContext 에 pEventBatch 멤버 추가 필요 (Layer 2)
{
    flatbuffers::FlatBufferBuilder& fbb = tc.pEventBatch->GetBuilder();

    NetEntityId projNet = tc.pEntityMap ? tc.pEntityMap->ToNet(e) : NULL_NET_ENTITY;
    NetEntityId tgtNet  = tc.pEntityMap ? tc.pEntityMap->ToNet(champE) : NULL_NET_ENTITY;

    auto hitEvent = Shared::Schema::CreateProjectileHitEvent(
        fbb, projNet, tgtNet,
        cpos.x, cpos.y, cpos.z,
        proj.flatBaseDamage, proj.bIsCritical);

    auto packet = Shared::Schema::CreateEventPacket(
        fbb,
        Shared::Schema::EventKind::ProjectileHit,
        tc.tickIndex,
        /*damage=*/0, /*buffApply=*/0,
        /*projectile=*/0,
        hitEvent,                         // ★ projectileHit 필드 채움
        /*skillCast=*/0);

    tc.pEventBatch->Push(packet);
}
```

### 8.3 Client EventApplier dispatch (★ 신규)

`Client/Public/Network/Client/EventApplier.h` (신규 또는 기존 확장):

```cpp
class CEventApplier final
{
public:
    void OnEventPacket(CWorld& world, EntityIdMap& map,
                       const u8_t* payload, u32_t len);

    using OnProjectileHitFn = std::function<void(
        EntityID projEntity, EntityID targetEntity,
        const Vec3& hitPos, f32_t damage, bool_t bIsCritical)>;

    void SetOnProjectileHitCallback(OnProjectileHitFn fn) { m_onProjectileHit = std::move(fn); }

private:
    OnProjectileHitFn m_onProjectileHit;
};
```

`Client/Private/Network/Client/EventApplier.cpp` 의 dispatch:

```cpp
void CEventApplier::OnEventPacket(CWorld& world, EntityIdMap& map,
    const u8_t* payload, u32_t len)
{
    flatbuffers::Verifier v(payload, len);
    if (!Shared::Schema::VerifyEventPacketBuffer(v)) return;
    const auto* ep = Shared::Schema::GetEventPacket(payload);
    if (!ep) return;

    switch (ep->kind())
    {
        case Shared::Schema::EventKind::ProjectileHit:
        {
            const auto* hit = ep->projectileHit();
            if (!hit) return;
            EntityID projE   = map.FromNet(hit->projectileNetId());
            EntityID targetE = map.FromNet(hit->targetNetId());
            if (m_onProjectileHit)
                m_onProjectileHit(
                    projE, targetE,
                    Vec3{ hit->hitX(), hit->hitY(), hit->hitZ() },
                    hit->damage(), hit->bIsCritical());
            break;
        }
        case Shared::Schema::EventKind::ProjectileSpawn:
            // (기존 spawn dispatch — Snapshot 기반 spawn 과 중복일 가능성 — Layer 2 에서 통합)
            break;
        // ... (기타 EventKind 분기)
        default: break;
    }
}
```

### 8.4 Scene 측 callback (FX/Sound trigger)

```cpp
m_pEventApplier->SetOnProjectileHitCallback(
    [this](EntityID projE, EntityID targetE, const Vec3& hitPos,
           f32_t damage, bool_t bIsCritical)
    {
        (void)projE; (void)damage;

        // 1. FX (FxBillboardComponent — Phase D EffectTool)
        SpawnHitFx(hitPos, bIsCritical);

        // 2. Sound (FMOD spatial)
        // CGameInstance::Get()->Get_Sound()->PlaySpatial("hit_default", hitPos);

        // 3. Damage popup UI
        if (targetE != NULL_ENTITY)
            SpawnDamagePopup(targetE, damage, bIsCritical);
    });
```

### 8.5 합격 추가 (★ Codex P2-6)
- ✅ `Event.fbs` 의 `EventPacket` 에 `projectileHit:ProjectileHitEvent` 필드 추가
- ✅ flatc 재생성 후 `Event_generated.h` 에 `ProjectileHitEvent` + `EventPacket::projectileHit()` accessor 존재
- ✅ Server emit — `CProjectileSystem` 의 hit 분기에서 EventBatch push
- ✅ Client dispatch — `CEventApplier::OnEventPacket` 의 `EventKind::ProjectileHit` case 동작
- ✅ Scene callback — hit FX + sound + damage popup 트리거

본 plan 은 emit + dispatch + Scene callback 까지 포함. EventBatch / EventApplier 본격 박제는 별도 plan (Layer 2 EventStream).

---

## 9. 결정성 (★ 핵심)

투사체는 random 요소가 거의 없지만, **critical chance**, **hit roll** 등은 RNG 사용 가능. 모든 RNG 호출은 `tc.pRng->Next*()` 로:

```cpp
// 예: critical roll
const f32_t critRoll = tc.pRng->NextFloat01();
proj.bIsCritical = (critRoll < champStat.critChance);
```

**전수 검증**: ProjectileSystem 안에서 `std::rand`, `std::mt19937` 등 직접 사용 금지. 회귀 grep:
```bash
rg "std::rand\(|std::mt19937" Shared/GameSim/Systems/ProjectileSystem.cpp
# → 0 hit
```

---

## 10. 합격 게이트

### 10.1 단일 client smoke
- ✅ 우클릭 후 Q 입력 → server log 에 ProjectileEntity spawn (netId 발급)
- ✅ snapshot broadcast 에 projectiles 배열 포함 (size 1+)
- ✅ client 화면에 투사체 visual 등장
- ✅ maxRange 도달 시 visual 사라짐 (server snapshot 에서 제거)

### 10.2 2 client hit
- ✅ Client A Q → 직선 투사체 → Client B 챔프 hit
- ✅ Client B 화면에 hit FX + HP 감소 (snapshot 으로 권위 갱신)
- ✅ Client A 화면에도 동일 hit FX

### 10.3 손실 시나리오
- ✅ 한 snapshot 손실 시 다음 snapshot 으로 위치 정상화 (visual smoothing 이 흡수)
- ✅ 투사체 spawn snapshot 손실 시 client 가 늦게 spawn (1-2 frame 지연 OK)

### 10.4 결정성
- ✅ 동일 server tick + 동일 input → 동일 투사체 spawn / hit / damage
- ✅ Replay 가능 (M5 Client Prediction 의 prerequisite)

### 10.5 성능
- ✅ 30Hz tick 에서 100 projectile 동시 → CPU < 1ms
- ✅ snapshot size — 100 projectile 시 < 4 KB

---

## 11. 위험 / 트레이드오프

| 위험 | 완화 |
|---|---|
| 투사체 N×N 충돌 검사 = O(NM) — 1000 투사체 + 100 적 = 100K 검사 | M3 AOI grid (50m cell) 도입 — 같은 cell 만 검사. 평균 10×10 = 100 |
| Snapshot 에 투사체 매 tick 전송 — 대역폭 폭증 | M3 delta + 정밀도 quantization (위치 16-bit fixed point). 100 projectile × 12B = 1.2KB |
| Client visual smoothing 이 server 권위와 어긋남 | snapshot 도착 시 즉시 snap (lerp X). 1 frame 시각 끊김 < 33ms |
| Homing target 사망 시 투사체 어떻게? | targetEntity == NULL_ENTITY 또는 hp.bIsDead 시 line 직진 fallback |
| Piercing 투사체의 hit cache 8개 한도 초과 | maxPierceCount 5 보장 + 안전 마진 8 → 충분 |
| ProjectileSystem 이 DamageRequestComponent 직접 add — DamageQueue 와 race | Phase chain 순서: ProjectileSystem → DamageQueueSystem → DeathSystem (단일 thread) |

---

## 12. 후속 작업 (본 plan 외)

- **AoE 즉시 폭발** (Karthus R, Annie R) — 별도 plan: ScheduledHitComponent
- **Wall / collision** — 환경 (지형) 과의 충돌. NavGrid 통합
- **Projectile pool** — 빈번한 create/destroy 비용 절감 (M3+)
- **Client prediction** (M5) — 본 발사한 client 가 즉시 시각화 + server snapshot 으로 reconcile

---

## 13. 한 줄

**투사체 = 서버 권위 ECS entity (ProjectileComponent + ProjectileSystem) + ProjectileSpawnRegistry (챔프×slot 정의 테이블) + Snapshot.fbs 확장 (ProjectileSnapshot 배열) + Client SnapshotApplier 확장 (OnNewProjectileFn callback) + visual smoothing (lerp + snap on snapshot). Phase chain 위치 = MoveSystem 다음. RNG 는 tc.pRng 만. 합격 = 2 client Q hit 시각 검증 + 100 projectile <1ms.**
