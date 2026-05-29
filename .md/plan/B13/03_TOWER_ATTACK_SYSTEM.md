# Phase B-13 / 03 — 타워 공격 AI 시스템 (LoL 우선순위 규칙) (v2)

**버전**: v2 (2026-05-04 — Codex 검토 #2 P-9/P-13 정정)
**가이드**: [`.md/process/PLAN_AUTHORING_PITFALLS.md`](../../process/PLAN_AUTHORING_PITFALLS.md)
**선행**: 04 SPATIAL HASH v2 (CWorld owned), 02 VISION v2.
**총 LOC**: ~700.

---

## §0. v1 → v2 정정 매트릭스

| # | v1 결함 | v2 정정 | PITFALLS |
|---|---|---|---|
| 1 | `CTurretAISystem::GetPhase() = 1` (MinionAI 와 동일 phase = 의도 불명확) | **phase = 6 (정수 단독 또는 TurretProjectileSystem 과 같은 6 — write set 다름 race-safe)**. MinionAI(2) / Vision(5) 후. | P-9 |
| 2 | `CTurretProjectileSystem::GetPhase() = 3` — Vision(5) 보다 먼저 | **phase = 6 (TurretAI 와 같은 phase, write set 다름)**. | P-9 |
| 3 | `m_SystemScheduler.GetSystem<CTurretAISystem>()` Cache — **`CSystemSchedular` 에 GetSystem API 없음** | **ECS event 패턴**: 챔프 평타 시 `TowerAggroNotifyComponent` (1-frame request) 부착 → `CTurretAISystem::Execute` 시작에 ForEach 로 소비 + 제거. Scene 측 cache 불필요. | P-13 |
| 4 | 공개 헤더 flat include | subdir 보존 | P-8 |
| 5 | `CSpatialIndex* m_pIndex` 멤버 + `Create(pIndex)` ctor 인자 | `CWorld::Get_SpatialIndex()` 매 Execute 호출 (CWorld owned, 04 v2) | P-10 |

**TowerAggroNotifyComponent 신규** (`Engine/Public/ECS/Components/GameplayComponents.h` 또는 별도 헤더):
```cpp
// 1-frame request — CTurretAISystem 이 소비 후 즉시 제거.
struct TowerAggroNotifyComponent
{
    EntityID attacker = NULL_ENTITY;   // 평타한 적 챔프
    EntityID victim   = NULL_ENTITY;   // 평타 받은 아군 챔프
    bool_t   bConsumed = false;
};
```

**Champion BA 처리부 (`Scene_InGame::ApplyChampionAttackHit` 또는 BA 적용 위치)**:
```cpp
// 챔프가 챔프 평타 명중 시 — 본 attacker 가 victim 의 팀 타워에 어그로 락 트리거
if (world.HasComponent<ChampionComponent>(attacker)
 && world.HasComponent<ChampionComponent>(victim))
{
    if (!world.HasComponent<TowerAggroNotifyComponent>(attacker))
        world.AddComponent<TowerAggroNotifyComponent>(attacker);
    auto& notify = world.GetComponent<TowerAggroNotifyComponent>(attacker);
    notify.attacker = attacker;
    notify.victim   = victim;
    notify.bConsumed = false;
}
// ★ Scene 이 m_SystemScheduler.GetSystem<...>() 호출하지 않음 (P-13 회피)
```

**`CTurretAISystem::Execute` 시작에 소비**:
```cpp
void CTurretAISystem::Execute(CWorld& world, f32_t dt)
{
    WINTERS_PROFILE_SCOPE("TurretAI::Execute");

    // ★ v2: ECS event 소비 — 1-frame TowerAggroNotifyComponent 부착된 attacker 들 처리.
    std::vector<EntityID> notifyToRemove;
    world.ForEach<TowerAggroNotifyComponent>(
        function<void(EntityID, TowerAggroNotifyComponent&)>(
            [&](EntityID id, TowerAggroNotifyComponent& n)
            {
                if (n.bConsumed) { notifyToRemove.push_back(id); return; }
                ApplyAggro(world, n.attacker, n.victim);   // 기존 NotifyChampionAttackedAlly 본체
                n.bConsumed = true;
                notifyToRemove.push_back(id);
            }));
    for (EntityID id : notifyToRemove)
        if (world.HasComponent<TowerAggroNotifyComponent>(id))
            world.RemoveComponent<TowerAggroNotifyComponent>(id);

    // 기존 타워 ForEach (TurretAIComponent / TransformComponent / SpatialAgentComponent)
    // ... (v1 본문 유지) ...
}
```

`ApplyAggro` 는 v1 의 `NotifyChampionAttackedAlly` 본문 그대로 (인자만 `(world, attacker, victim)` 동일).

---

## 0. LoL 타워 우선순위 규칙 (정식)

타워는 매 100ms 다음 5 단계로 타깃 결정:

1. **현재 타깃 유효성 검사** — 현재 타깃이 사거리 + 시야 내 + 살아있으면 유지.
2. **챔프 → 같은 팀 챔프 평타 공격 검출** — 어떤 적 챔프가 내 팀 챔프를 평타로 때렸으면, **2초 동안 그 적 챔프로 강제 타깃**. (LoL 의 "타워 어그로" 룰).
3. **사거리 내 + 시야 내 미니언/몬스터 검색** — 우선순위 (사거리 7.75m): Super 미니언 > Siege 미니언 > Melee 미니언 > Caster 미니언 > 정글몹.
4. **3 에서 후보 0 일 때만 챔프 타깃** — 사거리 내 적 챔프 중 가장 가까운.
5. **3/4 모두 0 → 타깃 없음 (Idle)**.

**스택 데미지**: 같은 타깃 연속 공격 시 +50%, +100%, +200%, +300% (4타째부터 +300% 고정).

---

## 1. TurretAIComponent — 기존 TurretComponent 확장

**파일**: `Engine/Public/ECS/Components/GameplayComponents.h`
**작업**: L97 (`TurretComponent` 끝) 직후 신규 컴포넌트 추가.

### 추가 코드
```cpp
// ─────────────────────────────────────────────────────────────
// TurretAIComponent — 타워 공격 AI 상태 (B-13)
// ─────────────────────────────────────────────────────────────
struct TurretAIComponent
{
    // 타깃 + 어그로
    EntityID currentTarget = NULL_ENTITY;
    f32_t    aggroLockTimer = 0.f;     // > 0: aggroTarget 유지 (LoL 어그로 락 2초)
    EntityID aggroTarget = NULL_ENTITY; // 챔프 평타로 어그로 받은 적 챔프

    // 공격 타이밍
    f32_t    attackCooldown = 0.f;
    f32_t    attackCooldownMax = 1.2f;  // 0.83 attacks/sec
    f32_t    attackRange = 7.75f;       // LoL 700u → 7.75m
    f32_t    attackDamage = 152.f;      // lvl 1 기본
    f32_t    attackProjectileSpeed = 30.f;

    // 스택
    EntityID lastDamagedTarget = NULL_ENTITY;
    uint8_t  stackCount = 0;            // 0~4 (1.0x, 1.5x, 2.0x, 3.0x, 4.0x)

    // 시야 (TurretComponent 와 별도 — VisionSourceComponent 와 시너지)
    f32_t    sightRange = 12.f;         // LoL 1100u → ≈12m

    // 비활성 — Outer 살아있을 때 Inner/Inhib/Nexus 는 false
    bool_t   bActive = true;

    // 시각화 — 현재 발사 중인 발사체 ID (TurretProjectileComponent 가진)
    EntityID activeProjectile = NULL_ENTITY;
};

// ─────────────────────────────────────────────────────────────
// TurretProjectileComponent — 타워가 발사한 투사체 (호밍)
// ─────────────────────────────────────────────────────────────
struct TurretProjectileComponent
{
    EntityID source = NULL_ENTITY;
    EntityID target = NULL_ENTITY;
    Vec3     spawnPos{};
    f32_t    speed = 30.f;
    f32_t    damage = 150.f;
    f32_t    elapsedTime = 0.f;
    f32_t    maxLifetime = 2.f;
};
```

---

## 2. CTurretAISystem 본 박제

**신규 파일**: `Engine/Public/ECS/Systems/TurretAISystem.h`

```cpp
#pragma once
#include "Engine_Defines.h"
#include "WintersAPI.h"
#include "WintersMath.h"
#include "ECS/ISystem.h"
#include "ECS/Entity.h"
#include <memory>
#include <vector>

class CWorld;
class CSpatialIndex;

NS_BEGIN(Engine)

class WINTERS_ENGINE CTurretAISystem final : public ISystem
{
public:
    ~CTurretAISystem() override = default;

    static std::unique_ptr<CTurretAISystem> Create(CSpatialIndex* pIndex)
    {
        auto p = std::unique_ptr<CTurretAISystem>(new CTurretAISystem());
        p->m_pIndex = pIndex;
        return p;
    }

    uint32_t    GetPhase() const override { return 6; }   // ★ v2: phase=6 (MinionAI=2/Vision=5 후, 단독 또는 TurretProjectile 과 같은 6 — write set 다름 race-safe)
    const char* GetName()  const override { return "TurretAISystem"; }
    void        Execute(CWorld& world, f32_t fTimeDelta) override;

    // 외부에서 챔프 평타 검출 시 호출 — 적 챔프에게 어그로 락
    void NotifyChampionAttackedAlly(CWorld& world,
                                     EntityID attacker, EntityID victim);

    // Outer/Inner/Inhib/Nexus 활성화 갱신 (외부에서 100ms 주기 호출)
    void UpdateActivation(CWorld& world);

private:
    CTurretAISystem() = default;

    EntityID SelectTarget(CWorld& world, EntityID turretId,
                          const Vec3& turretPos, uint8_t turretTeam,
                          const TurretAIComponent& ai) const;

    void FireProjectile(CWorld& world, EntityID turretId, EntityID targetId,
                        TurretAIComponent& ai, Vec3 turretPos);

    int32_t GetMinionPriority(CWorld& world, EntityID id) const;

    CSpatialIndex* m_pIndex = nullptr;
};

NS_END
```

**신규 파일**: `Engine/Private/ECS/Systems/TurretAISystem.cpp`

```cpp
#include "WintersPCH.h"
#include "ECS/Systems/TurretAISystem.h"
#include "ECS/World.h"
#include "ECS/SpatialIndex.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/VisionComponents.h"
#include "ProfilerAPI.h"

#include <algorithm>
#include <cmath>

NS_BEGIN(Engine)

void CTurretAISystem::Execute(CWorld& world, f32_t dt)
{
    WINTERS_PROFILE_SCOPE("TurretAI::Execute");

    world.ForEach<TurretAIComponent, TransformComponent, SpatialAgentComponent>(
        function<void(EntityID, TurretAIComponent&, TransformComponent&, SpatialAgentComponent&)>(
            [&](EntityID id, TurretAIComponent& ai, TransformComponent& xf, SpatialAgentComponent& a)
            {
                if (!ai.bActive) return;

                // HP 체크 — 죽은 타워는 발사 X
                if (world.HasComponent<HealthComponent>(id))
                {
                    const auto& hp = world.GetComponent<HealthComponent>(id);
                    if (hp.bIsDead || hp.fCurrent <= 0.f) return;
                }

                // 어그로 타이머 감쇠
                if (ai.aggroLockTimer > 0.f)
                {
                    ai.aggroLockTimer = std::max(0.f, ai.aggroLockTimer - dt);
                    if (ai.aggroLockTimer == 0.f)
                        ai.aggroTarget = NULL_ENTITY;
                }

                // 공격 쿨 감쇠
                ai.attackCooldown = std::max(0.f, ai.attackCooldown - dt);

                // 타깃 선정
                const Vec3 turretPos = xf.GetPosition();
                EntityID newTarget = SelectTarget(world, id, turretPos, a.team, ai);

                // 타깃 변경 시 스택 리셋
                if (newTarget != ai.lastDamagedTarget)
                {
                    ai.stackCount = 0;
                    ai.lastDamagedTarget = newTarget;
                }
                ai.currentTarget = newTarget;

                // 발사
                if (newTarget != NULL_ENTITY && ai.attackCooldown <= 0.f)
                {
                    FireProjectile(world, id, newTarget, ai, turretPos);
                    ai.attackCooldown = ai.attackCooldownMax;
                    ai.stackCount = std::min<uint8_t>(4, ai.stackCount + 1);
                }
            }));
}

EntityID CTurretAISystem::SelectTarget(CWorld& world, EntityID turretId,
                                        const Vec3& turretPos, uint8_t turretTeam,
                                        const TurretAIComponent& ai) const
{
    // 1. 어그로 락 — aggroTarget 유효 + 사거리 내면 강제 유지
    if (ai.aggroTarget != NULL_ENTITY && ai.aggroLockTimer > 0.f)
    {
        if (world.IsAlive(ai.aggroTarget)
            && world.HasComponent<TransformComponent>(ai.aggroTarget))
        {
            const Vec3 tp = world.GetComponent<TransformComponent>(ai.aggroTarget).GetPosition();
            const f32_t dx = tp.x - turretPos.x;
            const f32_t dz = tp.z - turretPos.z;
            if (dx * dx + dz * dz <= ai.attackRange * ai.attackRange)
                return ai.aggroTarget;
        }
    }

    if (!m_pIndex) return NULL_ENTITY;

    // 2. 사거리 내 모든 적 후보 — 미니언/챔프/정글몹
    std::vector<EntityID> candidates;
    const uint32_t mask = SpatialMask(eSpatialKind::Minion)
                        | SpatialMask(eSpatialKind::Champion)
                        | SpatialMask(eSpatialKind::JungleMob);
    m_pIndex->QueryRadius(turretPos, ai.attackRange, mask, /*excludeTeamMask*/ (1u << turretTeam),
                          candidates);

    // 시야 내 검사 — 부쉬 안 적은 타워가 못 봄 (Vision 은 챔프 시각)
    // 단 타워는 자체 VisionSourceComponent 보유 → 자기 시야 안에 있어야 공격
    auto bVisibleToTurret = [&](EntityID e) -> bool {
        if (!world.HasComponent<VisibilityComponent>(e)) return true;
        const auto& v = world.GetComponent<VisibilityComponent>(e);
        return (v.teamVisibilityMask & (1u << turretTeam)) != 0;
    };

    EntityID bestMinion = NULL_ENTITY;
    EntityID bestChamp = NULL_ENTITY;
    int32_t  bestPrio = INT32_MIN;
    f32_t    bestMinionDist2 = std::numeric_limits<f32_t>::max();
    f32_t    bestChampDist2 = std::numeric_limits<f32_t>::max();

    for (EntityID id : candidates)
    {
        if (id == turretId) continue;
        if (!world.IsAlive(id)) continue;
        if (!bVisibleToTurret(id)) continue;

        // HP
        if (world.HasComponent<HealthComponent>(id))
        {
            const auto& h = world.GetComponent<HealthComponent>(id);
            if (h.bIsDead || h.fCurrent <= 0.f) continue;
        }

        const Vec3 tp = world.GetComponent<TransformComponent>(id).GetPosition();
        const f32_t dx = tp.x - turretPos.x;
        const f32_t dz = tp.z - turretPos.z;
        const f32_t d2 = dx * dx + dz * dz;

        if (world.HasComponent<MinionStateComponent>(id) ||
            world.HasComponent<JungleComponent>(id))
        {
            const int32_t prio = GetMinionPriority(world, id);
            if (prio > bestPrio || (prio == bestPrio && d2 < bestMinionDist2))
            {
                bestPrio = prio;
                bestMinionDist2 = d2;
                bestMinion = id;
            }
        }
        else if (world.HasComponent<ChampionComponent>(id))
        {
            if (d2 < bestChampDist2)
            {
                bestChampDist2 = d2;
                bestChamp = id;
            }
        }
    }

    // 미니언/정글몹이 우선. 없으면 챔프.
    return (bestMinion != NULL_ENTITY) ? bestMinion : bestChamp;
}

int32_t CTurretAISystem::GetMinionPriority(CWorld& world, EntityID id) const
{
    if (world.HasComponent<MinionStateComponent>(id))
    {
        const auto& ms = world.GetComponent<MinionStateComponent>(id);
        // type: 0=Melee, 1=Ranged, 2=Siege, 3=Super
        switch (ms.type)
        {
        case 3: return 100;   // Super (랭크 4)
        case 2: return 80;    // Siege (랭크 3)
        case 0: return 60;    // Melee (랭크 2)
        case 1: return 40;    // Caster/Ranged (랭크 1)
        default: return 20;
        }
    }
    if (world.HasComponent<JungleComponent>(id))
        return 30;   // 정글몹 = caster 보다 살짝 낮음
    return 0;
}

void CTurretAISystem::FireProjectile(CWorld& world, EntityID turretId, EntityID targetId,
                                      TurretAIComponent& ai, Vec3 turretPos)
{
    // 데미지 계산 — 스택 적용
    const f32_t kStackMul[5] = { 1.f, 1.5f, 2.f, 3.f, 4.f };
    const f32_t finalDamage = ai.attackDamage * kStackMul[ai.stackCount];

    // 발사체 엔티티 생성
    EntityID proj = world.CreateEntity();
    auto& tp = world.AddComponent<TransformComponent>(proj);
    tp.SetPosition({ turretPos.x, turretPos.y + 5.f, turretPos.z });   // 타워 꼭대기

    auto& pc = world.AddComponent<TurretProjectileComponent>(proj);
    pc.source = turretId;
    pc.target = targetId;
    pc.spawnPos = turretPos;
    pc.speed = ai.attackProjectileSpeed;
    pc.damage = finalDamage;
    pc.elapsedTime = 0.f;
    pc.maxLifetime = 2.f;

    ai.activeProjectile = proj;

    // SFX/VFX 훅 — Phase B-14 에서 박제 (TurretFxPresets::SpawnFireball)
    // SFX/VFX hook stub
}

void CTurretAISystem::NotifyChampionAttackedAlly(CWorld& world,
                                                  EntityID attacker, EntityID victim)
{
    WINTERS_PROFILE_SCOPE("TurretAI::NotifyChampAttack");

    if (!world.HasComponent<TransformComponent>(victim)) return;
    if (!world.HasComponent<SpatialAgentComponent>(victim)) return;

    const Vec3 vPos = world.GetComponent<TransformComponent>(victim).GetPosition();
    const uint8_t victimTeam = world.GetComponent<SpatialAgentComponent>(victim).team;

    // victim 의 팀의 모든 타워 중 사거리 내인 타워 → aggro 락
    world.ForEach<TurretAIComponent, TransformComponent, SpatialAgentComponent>(
        function<void(EntityID, TurretAIComponent&, TransformComponent&, SpatialAgentComponent&)>(
            [&](EntityID id, TurretAIComponent& ai, TransformComponent& xf, SpatialAgentComponent& a)
            {
                if (a.team != victimTeam) return;   // 같은 팀 타워만 어그로
                if (!ai.bActive) return;

                const Vec3 tp = xf.GetPosition();
                const f32_t dx = tp.x - vPos.x;
                const f32_t dz = tp.z - vPos.z;
                if (dx * dx + dz * dz > ai.attackRange * ai.attackRange) return;

                ai.aggroTarget = attacker;
                ai.aggroLockTimer = 2.f;   // LoL 2초 어그로 락
            }));
}

void CTurretAISystem::UpdateActivation(CWorld& world)
{
    WINTERS_PROFILE_SCOPE("TurretAI::UpdateActivation");

    // LoL 룰: lane 안에서 Outer 살아있으면 Inner/Inhib/Nexus 는 비활성 (TargetableTag 제거).
    // tier: 0=Outer, 1=Inner, 2=Inhib, 3=Nexus.
    // lane: 0=Top, 1=Mid, 2=Bot, 3=Base.

    // step 1: lane × team 별 살아있는 최저 tier 추적
    // 8 entries: Blue×{Top,Mid,Bot,Base} + Red×{Top,Mid,Bot,Base}
    int32_t lowestAliveTier[2][4] = {{4,4,4,4},{4,4,4,4}};

    world.ForEach<TurretAIComponent, StructureComponent, HealthComponent>(
        function<void(EntityID, TurretAIComponent&, StructureComponent&, HealthComponent&)>(
            [&](EntityID id, TurretAIComponent&, StructureComponent& s, HealthComponent& h)
            {
                if (h.bIsDead) return;
                if (s.team >= 2 || s.lane >= 4) return;
                int32_t tier = static_cast<int32_t>(s.tier);
                if (tier < lowestAliveTier[s.team][s.lane])
                    lowestAliveTier[s.team][s.lane] = tier;
            }));

    // step 2: 자기 tier > lowestAliveTier 면 비활성
    world.ForEach<TurretAIComponent, StructureComponent>(
        function<void(EntityID, TurretAIComponent&, StructureComponent&)>(
            [&](EntityID id, TurretAIComponent& ai, StructureComponent& s)
            {
                if (s.team >= 2 || s.lane >= 4) { ai.bActive = true; return; }
                const int32_t myTier = static_cast<int32_t>(s.tier);
                const int32_t lowest = lowestAliveTier[s.team][s.lane];
                ai.bActive = (myTier <= lowest);
                // ★ Codex C-2: TargetableTag 도 동기화 (적 미니언이 타워 무시하도록)
                if (ai.bActive)
                {
                    if (!world.HasComponent<TargetableTag>(id))
                        world.AddComponent<TargetableTag>(id);
                }
                else
                {
                    if (world.HasComponent<TargetableTag>(id))
                        world.RemoveComponent<TargetableTag>(id);
                }
            }));
}

NS_END
```

---

## 3. CTurretProjectileSystem — 발사체 호밍 + 명중 처리

**신규 파일**: `Engine/Public/ECS/Systems/TurretProjectileSystem.h`

```cpp
#pragma once
#include "Engine_Defines.h"
#include "WintersAPI.h"
#include "ECS/ISystem.h"
#include <memory>

class CWorld;

NS_BEGIN(Engine)

class WINTERS_ENGINE CTurretProjectileSystem final : public ISystem
{
public:
    ~CTurretProjectileSystem() override = default;
    static std::unique_ptr<CTurretProjectileSystem> Create()
    {
        return std::unique_ptr<CTurretProjectileSystem>(new CTurretProjectileSystem());
    }
    uint32_t    GetPhase() const override { return 6; }   // ★ v2: phase=6 (TurretAI 와 같은 phase, write set 다름)
    const char* GetName()  const override { return "TurretProjectileSystem"; }
    void        Execute(CWorld& world, f32_t fTimeDelta) override;
private:
    CTurretProjectileSystem() = default;
};

NS_END
```

**신규 파일**: `Engine/Private/ECS/Systems/TurretProjectileSystem.cpp`

```cpp
#include "WintersPCH.h"
#include "ECS/Systems/TurretProjectileSystem.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/GameplayComponents.h"
#include "ProfilerAPI.h"

#include <cmath>

NS_BEGIN(Engine)

void CTurretProjectileSystem::Execute(CWorld& world, f32_t dt)
{
    WINTERS_PROFILE_SCOPE("TurretProjectile::Execute");

    std::vector<EntityID> toDestroy;

    world.ForEach<TurretProjectileComponent, TransformComponent>(
        function<void(EntityID, TurretProjectileComponent&, TransformComponent&)>(
            [&](EntityID id, TurretProjectileComponent& pc, TransformComponent& xf)
            {
                pc.elapsedTime += dt;

                // 타깃 유효 검사
                if (!world.IsAlive(pc.target) ||
                    !world.HasComponent<TransformComponent>(pc.target))
                {
                    toDestroy.push_back(id);
                    return;
                }
                if (pc.elapsedTime > pc.maxLifetime)
                {
                    toDestroy.push_back(id);
                    return;
                }

                const Vec3 cur = xf.GetPosition();
                const Vec3 tgt = world.GetComponent<TransformComponent>(pc.target).GetPosition();
                Vec3 dir = { tgt.x - cur.x, tgt.y + 1.f - cur.y, tgt.z - cur.z };
                const f32_t dist = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);

                if (dist <= pc.speed * dt + 0.5f)
                {
                    // 명중
                    if (world.HasComponent<HealthComponent>(pc.target))
                    {
                        auto& h = world.GetComponent<HealthComponent>(pc.target);
                        h.fCurrent -= pc.damage;
                        if (h.fCurrent <= 0.f)
                        {
                            h.fCurrent = 0.f;
                            h.bIsDead = true;
                        }
                    }
                    toDestroy.push_back(id);
                    return;
                }

                // 호밍 이동
                const f32_t inv = 1.f / dist;
                Vec3 step = { dir.x * inv * pc.speed * dt,
                              dir.y * inv * pc.speed * dt,
                              dir.z * inv * pc.speed * dt };
                xf.SetPosition({ cur.x + step.x, cur.y + step.y, cur.z + step.z });
            }));

    for (EntityID id : toDestroy)
        world.DestroyEntity(id);
}

NS_END
```

---

## 4. 챔프 평타 발생 시 타워 어그로 알림

**파일**: `Client/Private/Scene/Scene_InGame.cpp`
**작업**: BA 시전 + 명중 시점 (recoveryFrame hook 안) 에 1줄 추가.

### 패턴 (이렐리아 ApplyHit 예시)
```cpp
// 기존: 데미지 적용
if (world.HasComponent<HealthComponent>(targetId))
{
    auto& h = world.GetComponent<HealthComponent>(targetId);
    h.fCurrent -= damage;
}

// 수정 후: 챔프가 챔프를 평타 공격 시 타워 어그로 알림
if (world.HasComponent<ChampionComponent>(targetId)
    && world.HasComponent<ChampionComponent>(playerEntity))
{
    // ★ v2: ECS event 패턴 (P-13 회피). pTurretAI cache 불필요.
    if (!world.HasComponent<TowerAggroNotifyComponent>(playerEntity))
        world.AddComponent<TowerAggroNotifyComponent>(playerEntity);
    auto& notify = world.GetComponent<TowerAggroNotifyComponent>(playerEntity);
    notify.attacker = playerEntity;
    notify.victim   = targetId;
    notify.bConsumed = false;
}
```

**v1 폐기**: `pTurretAI = m_SystemScheduler.GetSystem<CTurretAISystem>()` Cache 박제 — `CSystemSchedular` 에 `GetSystem` API 없음 (P-13). v2 는 ECS event 패턴 (위 코드 참조). 본 §0 정정 매트릭스 + `CTurretAISystem::Execute` 시작의 ForEach 소비 박제 참조.

---

## 5. 타워 비활성 갱신 — 100ms 틱

**파일**: `Client/Private/Scene/Scene_InGame.cpp`
**작업**: OnUpdate 에서 100ms 틱 누적.

```cpp
m_fTowerActivationTimer += dt;
if (m_fTowerActivationTimer >= 0.1f)
{
    m_fTowerActivationTimer = 0.f;
    // ★ v2: UpdateActivation 도 외부 호출 대신 CTurretAISystem 자체에서 100ms 틱.
    // CTurretAISystem 멤버에 m_fActivationAccum 추가 + Execute 끝에 dt 누적 + 100ms 도달 시 자체 호출.
    // Scene 측 cache 불필요 — 같은 효과.
    // (이 블록 자체 폐기 권장 — Scene_InGame 변경 X)
}
```

---

## 6. 타워 시야 — VisionSourceComponent 부착

**파일**: 타워 스폰 함수 (`CStructure_Manager::SpawnStructure`)
**작업**: 타워 스폰 시 VisionSourceComponent 추가.

```cpp
// 기존
world.AddComponent<TurretComponent>(id);
world.AddComponent<HealthComponent>(id) = { 3000.f, 3000.f, false };

// 추가
world.AddComponent<TurretAIComponent>(id);
world.AddComponent<VisionSourceComponent>(id) = { /*sightRange*/ 12.f, /*bTrueSight*/ true, false, 0.f };
world.AddComponent<VisibilityComponent>(id);
world.AddComponent<SpatialAgentComponent>(id) = {
    eSpatialKind::Turret, /*team*/ static_cast<uint8_t>(team), /*radius*/ 1.5f
};
```

**왜 bTrueSight=true?** — 타워는 LoL 본체에서 부쉬 안 적도 시야 안에 있으면 공격 (단 챔프는 부쉬 들어가면 해제).

---

## 7. 디버그 ImGui 패널

**신규 파일**: `Client/Public/Editor/TurretDebugPanel.h` + `.cpp`

```cpp
// .cpp 핵심
void Draw_TurretDebugPanel(CWorld& world)
{
    ImGui::Begin("Turret AI Debug");

    int idx = 0;
    world.ForEach<TurretAIComponent, StructureComponent>(
        function<void(EntityID, TurretAIComponent&, StructureComponent&)>(
            [&](EntityID id, TurretAIComponent& ai, StructureComponent& s)
            {
                ImGui::PushID(idx++);
                const char* tierStr[] = { "Outer", "Inner", "Inhib", "Nexus", "?" };
                const char* laneStr[] = { "Top", "Mid", "Bot", "Base", "?" };
                ImGui::Text("[%s] [%s] team=%u  active=%d  cd=%.2f  stack=%u",
                    laneStr[std::min<uint32_t>(s.lane, 4)],
                    tierStr[std::min<uint32_t>(s.tier, 4)],
                    s.team, ai.bActive ? 1 : 0,
                    ai.attackCooldown, ai.stackCount);
                if (ai.currentTarget != NULL_ENTITY)
                    ImGui::Text("  → target = %u", ai.currentTarget);
                if (ai.aggroLockTimer > 0.f)
                    ImGui::Text("  AGGRO LOCK: %u (%.1fs)", ai.aggroTarget, ai.aggroLockTimer);
                ImGui::PopID();
            }));

    ImGui::End();
}
```

---

## 8. vcxproj 등록

```xml
<ClInclude Include="..\Public\ECS\Systems\TurretAISystem.h" />
<ClInclude Include="..\Public\ECS\Systems\TurretProjectileSystem.h" />

<ClCompile Include="..\Private\ECS\Systems\TurretAISystem.cpp" />
<ClCompile Include="..\Private\ECS\Systems\TurretProjectileSystem.cpp" />
```

---

## 9. 검증 시나리오

### V-1: 사거리 진입 시 발포
- [ ] Blue 미니언이 Red Outer 타워 사거리 (7.75m) 진입 → 1.2초 마다 발포.
- [ ] 발사체가 미니언 호밍 → 명중 → 데미지 152 (1타).
- [ ] 같은 미니언 2타째 → 데미지 152*1.5 = 228.

### V-2: 우선순위
- [ ] 미니언 + 챔프 모두 사거리 안 → 미니언 우선.
- [ ] Super 미니언 + Melee 미니언 → Super 우선.

### V-3: 타워 어그로
- [ ] Blue 챔프가 Red 챔프를 평타 → Red Outer 타워 사거리 안이면 2초 동안 Blue 챔프 강제 타깃.
- [ ] 2초 후 자동 해제 → 다시 미니언 우선.

### V-4: 활성화 룰
- [ ] Top Outer 살아있을 때 Top Inner 발포 X (적 미니언 사거리 안에 와도).
- [ ] Top Outer 파괴 → Top Inner 즉시 활성화.
- [ ] Inhib 파괴 → Nexus Top 도 활성화.

### V-5: 시야
- [ ] 적 챔프가 부쉬 진입 (사거리 안) → 타워가 더 이상 안 봄 (BUT bTrueSight=true 라 봄). 대신 타워 어그로 락 안 트리거.

---

## 10. Codex 보정

### C-1 마스터 §6 (재인용): Outer 살아있을 때 Inner 공격 가능 버그
**해결**: `UpdateActivation` 100ms 틱. TargetableTag 동기화 (§2 본 구현에 포함).

### C-2: 타워가 자기 자신 발사
**우려**: SpatialIndex.QueryRadius 가 자기 자신 반환 → 자기 자신 공격.
**해결**: `if (id == turretId) continue;` (`SelectTarget` 본 구현 L83 처럼 박제됨).

### C-3: 발사체 명중 전 타깃 사망
**해결**: `TurretProjectileSystem` 의 `world.IsAlive(pc.target)` 검사 → 사망 시 발사체 destroy.

### C-4: 타워 위치가 cell 경계에 있을 때 stale
**해결**: 타워는 정적 — frame 마다 같은 cell. NavSystem 안 거침. 무관.

### C-5: `aggroTarget` 챔프가 너무 멀어졌을 때
**해결**: `SelectTarget` 의 어그로 분기에서 사거리 검증 (§2 L66 본 구현). 사거리 밖이면 미니언 룰로 폴백.

### C-6: 발사체 Visual 미박제
**해결**: Phase B-14 의 TurretFxPresets 에서 박제. 본 Phase 는 데미지 적용까지만.

### C-7: TargetableTag 의존 시스템
**우려**: TargetableTag 제거 시 다른 시스템 (예: BT 의 ConditionNode) 이 영향받을 수 있음.
**해결**: TargetableTag 는 "공격 가능" 유의미 (CLAUDE.md L242 정의). MinionAI/BT 모두 TargetableTag 검사하므로 제거 시 자연스럽게 무시됨.

---

## 11. 다음 진입

03 완료 후 → **05 AI BT/MCTS/RL** (BT 의 Action 노드가 "타워 사거리 회피" 같은 행동에 본 시스템 의존).

---

**END OF SUB-PLAN 03**
