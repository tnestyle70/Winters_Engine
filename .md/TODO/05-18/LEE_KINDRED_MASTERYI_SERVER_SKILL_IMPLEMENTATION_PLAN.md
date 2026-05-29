Session - LeeSin, Kindred, MasterYi server GameSim files are wired into the existing server-authoritative skill, ward, snapshot, and FX cue pipeline.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/LeeSinSimComponent.h

아래로 교체:

```cpp
#pragma once

#include "ECS/Entity.h"
#include "WintersMath.h"
#include "WintersTypes.h"

struct LeeSinQMarkComponent
{
    EntityID sourceEntity = NULL_ENTITY;
    f32_t fRemainingSec = 3.f;
};

struct LeeSinWardOwnerComponent
{
    EntityID owner = NULL_ENTITY;
};

struct LeeSinDashComponent
{
    Vec3 vStart{};
    Vec3 vEnd{};
    f32_t fElapsedSec = 0.f;
    f32_t fDurationSec = 0.18f;
};

struct LeeSinKnockbackComponent
{
    EntityID sourceEntity = NULL_ENTITY;
    Vec3 vStart{};
    Vec3 vEnd{};
    f32_t fBaseY = 0.f;
    f32_t fElapsedSec = 0.f;
    f32_t fDurationSec = 0.35f;
    f32_t fLift = 1.8f;
};
```

1-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/KindredSimComponent.h

아래로 교체:

```cpp
#pragma once

#include "ECS/Entity.h"
#include "WintersMath.h"
#include "WintersTypes.h"

struct KindredSimComponent
{
    f32_t fWRemainingSec = 0.f;
    Vec3 vWCenter{};
};

struct KindredEMarkComponent
{
    EntityID sourceEntity = NULL_ENTITY;
    u8_t hitCount = 0;
    f32_t fRemainingSec = 4.f;
};

struct KindredHealthFloorComponent
{
    EntityID sourceEntity = NULL_ENTITY;
    Vec3 vCenter{};
    f32_t fRadius = 5.f;
    f32_t fRemainingSec = 4.f;
    f32_t fMinHealth = 1.f;
};

struct KindredDashComponent
{
    Vec3 vStart{};
    Vec3 vEnd{};
    f32_t fElapsedSec = 0.f;
    f32_t fDurationSec = 0.18f;
};
```

1-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/MasterYiSimComponent.h

아래로 교체:

```cpp
#pragma once

#include "ECS/Entity.h"
#include "WintersMath.h"
#include "WintersTypes.h"

struct MasterYiSimComponent
{
    EntityID qTarget = NULL_ENTITY;
    Vec3 vQReturnPosition{};
    f32_t fAlphaStrikeRemainingSec = 0.f;
    f32_t fMeditateRemainingSec = 0.f;
    f32_t fMeditateHealTickSec = 0.f;
    f32_t fWujuStyleRemainingSec = 0.f;
    f32_t fHighlanderRemainingSec = 0.f;
    f32_t fHighlanderMoveSpeedBonus = 0.f;
    f32_t fHighlanderAttackSpeedBonus = 0.f;
};
```

1-4. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/LeeSinGameSim.h

기존 코드:

```cpp
#pragma once

namespace LeeSinGameSim
{
	void RegisterHooks();
}
```

아래로 교체:

```cpp
#pragma once

#include "Shared/GameSim/Systems/ICommandExecutor.h"

namespace LeeSinGameSim
{
    void RegisterHooks();
    void Tick(CWorld& world, const TickContext& tc);
}
```

1-5. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/KindredGameSim.h

기존 코드:

```cpp
namespace KindredGameSim
{
	void RegisterHooks();
}
```

아래로 교체:

```cpp
#pragma once

#include "Shared/GameSim/Systems/ICommandExecutor.h"

namespace KindredGameSim
{
    void RegisterHooks();
    void Tick(CWorld& world, const TickContext& tc);
}
```

1-6. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/MasterYiGameSim.h

기존 코드:

```cpp
namespace MasterYiGameSim
{
	void RegisterHooks();
}
```

아래로 교체:

```cpp
#pragma once

#include "Shared/GameSim/Systems/ICommandExecutor.h"

namespace MasterYiGameSim
{
    void RegisterHooks();
    void Tick(CWorld& world, const TickContext& tc);
}
```

1-7. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/ProjectileKindComponent.h

기존 코드:

```cpp
    MysticShot = 10,
    EssenceFlux = 11,
    GlobalBeam = 12,
    PROJECTILE_END
```

아래로 교체:

```cpp
    MysticShot = 10,
    EssenceFlux = 11,
    GlobalBeam = 12,
    LeeSinSonicWave = 20,
    KindredArrow = 21,
    PROJECTILE_END
```

1-8. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/ChampionRuntimeDefaults.cpp

`kChampionSkillTimingTable`의 Yone row 뒤에 아래를 추가:

```cpp
        { eChampion::LEESIN, 0, 1, 0.60f, 1.00f },
        { eChampion::LEESIN, 1, 1, 0.45f, 1.00f },
        { eChampion::LEESIN, 1, 2, 0.35f, 1.00f },
        { eChampion::LEESIN, 2, 1, 0.35f, 1.00f },
        { eChampion::LEESIN, 3, 1, 0.40f, 1.00f },
        { eChampion::LEESIN, 4, 1, 0.70f, 1.00f },

        { eChampion::KINDRED, 0, 1, 0.70f, 1.00f },
        { eChampion::KINDRED, 1, 1, 0.25f, 1.20f },
        { eChampion::KINDRED, 2, 1, 0.35f, 1.00f },
        { eChampion::KINDRED, 3, 1, 0.45f, 1.00f },
        { eChampion::KINDRED, 4, 1, 0.70f, 1.00f },

        { eChampion::MASTERYI, 0, 1, 0.60f, 1.00f },
        { eChampion::MASTERYI, 1, 1, 0.50f, 1.00f },
        { eChampion::MASTERYI, 2, 1, 4.00f, 1.00f },
        { eChampion::MASTERYI, 3, 1, 0.25f, 1.00f },
        { eChampion::MASTERYI, 4, 1, 0.40f, 1.00f },
```

`kChampionSkillStageTable`에 아래를 추가:

```cpp
        { eChampion::LEESIN, 1, 2, 3.00f },
```

`kChampionSkillValueTable`의 Yone row 뒤에 아래를 추가:

```cpp
        { eChampion::LEESIN, 0, 0.60f, 1.50f },
        { eChampion::LEESIN, 1, 8.00f, 11.00f },
        { eChampion::LEESIN, 2, 12.00f, 7.00f },
        { eChampion::LEESIN, 3, 10.00f, 4.25f },
        { eChampion::LEESIN, 4, 90.00f, 3.75f },

        { eChampion::KINDRED, 0, 0.70f, 5.00f },
        { eChampion::KINDRED, 1, 8.00f, 3.40f },
        { eChampion::KINDRED, 2, 18.00f, 8.00f },
        { eChampion::KINDRED, 3, 14.00f, 5.00f },
        { eChampion::KINDRED, 4, 120.00f, 5.00f },

        { eChampion::MASTERYI, 0, 0.60f, 1.50f },
        { eChampion::MASTERYI, 1, 18.00f, 6.00f },
        { eChampion::MASTERYI, 2, 28.00f, 0.00f },
        { eChampion::MASTERYI, 3, 18.00f, 0.00f },
        { eChampion::MASTERYI, 4, 90.00f, 0.00f },
```

1-9. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/LeeSinGameSim.cpp

기존 주석-only 파일 전체를 교체한다. 핵심 구현은 아래 구조로 작성한다.

```cpp
#include "Shared/GameSim/Champions/LeeSinGameSim.h"

#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/LeeSinSimComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Systems/DamagePipeline.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/ReplicatedEventQueue.h"
#include "Shared/GameSim/Systems/StatusEffectSystem.h"

#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/VisionComponents.h"
#include "Shared/GameSim/World.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

namespace
{
    constexpr f32_t kPi = 3.14159265358979323846f;
    constexpr f32_t kQDamage = 70.f;
    constexpr f32_t kQ2Damage = 110.f;
    constexpr f32_t kQSpeed = 24.f;
    constexpr f32_t kQRadius = 0.55f;
    constexpr f32_t kWDashDuration = 0.18f;
    constexpr f32_t kWShieldAmount = 80.f;
    constexpr f32_t kEDamage = 80.f;
    constexpr f32_t kESlowDuration = 2.0f;
    constexpr f32_t kRDamage = 180.f;
    constexpr f32_t kRKnockbackDistance = 6.0f;

    u16_t SkillId(u8_t slot)
    {
        return static_cast<u16_t>((static_cast<u32_t>(eChampion::LEESIN) << 8) | slot);
    }

    Vec3 ResolveDirection(const GameplayHookContext& ctx, const Vec3& origin)
    {
        if (ctx.pCommand)
        {
            Vec3 dir = WintersMath::NormalizeXZ(ctx.pCommand->direction);
            if (dir.x != 0.f || dir.z != 0.f)
                return dir;

            dir = WintersMath::NormalizeXZ(Vec3{
                ctx.pCommand->groundPos.x - origin.x,
                0.f,
                ctx.pCommand->groundPos.z - origin.z
            });
            if (dir.x != 0.f || dir.z != 0.f)
                return dir;
        }
        return Vec3{ 0.f, 0.f, 1.f };
    }

    void EnqueueTargetDamage(CWorld& world, EntityID source, EntityID target,
        eTeam sourceTeam, f32_t damage, u8_t slot, u8_t rank)
    {
        DamageRequest req{};
        req.source = source;
        req.target = target;
        req.sourceTeam = sourceTeam;
        req.type = eDamageType::Physical;
        req.flatAmount = damage;
        req.skillId = SkillId(slot);
        req.rank = rank;
        req.flags = DamageFlag_OnHit;
        EnqueueDamageRequest(world, req);
    }

    bool_t IsAlliedSafeguardTarget(CWorld& world, EntityID caster, EntityID target, eTeam casterTeam)
    {
        if (target == NULL_ENTITY || !world.IsAlive(target) || !world.HasComponent<TargetableTag>(target))
            return false;
        if (target == caster)
            return true;
        if (world.HasComponent<ChampionComponent>(target))
            return world.GetComponent<ChampionComponent>(target).team == casterTeam;
        if (world.HasComponent<MinionComponent>(target))
            return world.GetComponent<MinionComponent>(target).team == casterTeam;
        if (world.HasComponent<WardComponent>(target))
            return world.GetComponent<WardComponent>(target).ownerTeam == casterTeam;
        return false;
    }

    EntityID FindMarkedTarget(CWorld& world, EntityID caster)
    {
        EntityID best = NULL_ENTITY;
        world.ForEach<LeeSinQMarkComponent>(
            std::function<void(EntityID, LeeSinQMarkComponent&)>(
                [&](EntityID entity, LeeSinQMarkComponent& mark)
                {
                    if (best == NULL_ENTITY && mark.sourceEntity == caster)
                        best = entity;
                }));
        return best;
    }

    void StartDash(CWorld& world, EntityID entity, const Vec3& end, f32_t durationSec)
    {
        if (!world.HasComponent<TransformComponent>(entity))
            return;
        LeeSinDashComponent dash{};
        dash.vStart = world.GetComponent<TransformComponent>(entity).GetPosition();
        dash.vEnd = end;
        dash.fDurationSec = durationSec;
        if (world.HasComponent<LeeSinDashComponent>(entity))
            world.GetComponent<LeeSinDashComponent>(entity) = dash;
        else
            world.AddComponent<LeeSinDashComponent>(entity, dash);
        if (world.HasComponent<MoveTargetComponent>(entity))
            world.GetComponent<MoveTargetComponent>(entity).bHasTarget = false;
    }

    void EmitEffect(CWorld& world, EntityID source, EntityID target,
        u8_t slot, u8_t rank, const TickContext* tc, const Vec3& pos, const Vec3& dir)
    {
        if (!tc)
            return;
        ReplicatedEventComponent ev{};
        ev.kind = eReplicatedEventKind::EffectTrigger;
        ev.sourceEntity = source;
        ev.targetEntity = target;
        ev.effectId = MakeGameplayHookId(eChampion::LEESIN, GameplayHookVariant::Q_CastFrame + (slot - 1u));
        ev.slot = slot;
        ev.rank = rank;
        ev.flags = static_cast<u16_t>((rank << 8) | slot);
        ev.position = pos;
        ev.direction = dir;
        ev.durationMs = 700;
        ev.startTick = tc->tickIndex;
        EnqueueReplicatedEvent(world, ev);
    }

    void OnQ(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
            return;
        CWorld& world = *ctx.pWorld;
        const Vec3 origin = world.GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        const bool_t bStage2 = ctx.pCommand && ctx.pCommand->itemId == 2u;

        if (bStage2)
        {
            const EntityID target = FindMarkedTarget(world, ctx.casterEntity);
            if (target == NULL_ENTITY || !world.HasComponent<TransformComponent>(target))
                return;
            const Vec3 targetPos = world.GetComponent<TransformComponent>(target).GetPosition();
            StartDash(world, ctx.casterEntity, targetPos, 0.22f);
            EnqueueTargetDamage(world, ctx.casterEntity, target, ctx.casterTeam, kQ2Damage, 1, ctx.skillRank);
            world.RemoveComponent<LeeSinQMarkComponent>(target);
            EmitEffect(world, ctx.casterEntity, target, 1, ctx.skillRank, ctx.pTickCtx, targetPos, Vec3{});
            return;
        }

        SkillProjectileComponent projectile{};
        projectile.sourceEntity = ctx.casterEntity;
        projectile.sourceTeam = ctx.casterTeam;
        projectile.kind = eProjectileKind::LeeSinSonicWave;
        projectile.skillID = SkillId(1);
        projectile.rank = ctx.skillRank;
        projectile.currentPos = Vec3{ origin.x, origin.y + 1.0f, origin.z };
        projectile.direction = ResolveDirection(ctx, origin);
        projectile.speed = kQSpeed;
        projectile.maxDistance = GetDefaultChampionSkillRange(eChampion::LEESIN, 1);
        projectile.hitRadius = kQRadius;
        projectile.damage = kQDamage;

        const EntityID projectileEntity = world.CreateEntity();
        world.AddComponent<SkillProjectileComponent>(projectileEntity, projectile);
        TransformComponent transform{};
        transform.SetPosition(projectile.currentPos);
        world.AddComponent<TransformComponent>(projectileEntity, transform);
    }

    void OnW(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand)
            return;
        CWorld& world = *ctx.pWorld;
        const EntityID target = ctx.pCommand->targetEntity;
        if (!IsAlliedSafeguardTarget(world, ctx.casterEntity, target, ctx.casterTeam))
            return;
        if (!world.HasComponent<TransformComponent>(target))
            return;

        StartDash(world, ctx.casterEntity, world.GetComponent<TransformComponent>(target).GetPosition(), kWDashDuration);
        if (world.HasComponent<ChampionComponent>(target))
        {
            auto& champion = world.GetComponent<ChampionComponent>(target);
            champion.fShieldRemaining = kWShieldAmount;
            champion.fShieldTimer = 2.0f;
        }
    }

    void OnE(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
            return;
        CWorld& world = *ctx.pWorld;
        const Vec3 origin = world.GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        const f32_t radiusSq = GetDefaultChampionSkillRange(eChampion::LEESIN, 3) *
            GetDefaultChampionSkillRange(eChampion::LEESIN, 3);

        world.ForEach<ChampionComponent, TransformComponent>(
            std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                [&](EntityID target, ChampionComponent& champ, TransformComponent& xf)
                {
                    if (target == ctx.casterEntity || champ.team == ctx.casterTeam)
                        return;
                    if (WintersMath::DistanceSqXZ(origin, xf.GetPosition()) > radiusSq)
                        return;
                    EnqueueTargetDamage(world, ctx.casterEntity, target, ctx.casterTeam, kEDamage, 3, ctx.skillRank);
                    StatusEffectApplyDesc slow{};
                    slow.effectId = eStatusEffectId::GenericSlow;
                    slow.sourceEntity = ctx.casterEntity;
                    slow.stateFlags = kGameplayStateSlowedFlag;
                    slow.fDurationSec = kESlowDuration;
                    slow.fMoveSpeedMul = 0.60f;
                    GameplayStatus::ApplyStatusEffect(world, target, slow);
                }));
    }

    void OnR(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand)
            return;
        CWorld& world = *ctx.pWorld;
        const EntityID target = ctx.pCommand->targetEntity;
        if (target == NULL_ENTITY || !world.HasComponent<TransformComponent>(ctx.casterEntity) ||
            !world.HasComponent<TransformComponent>(target))
        {
            return;
        }
        const Vec3 casterPos = world.GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        const Vec3 targetPos = world.GetComponent<TransformComponent>(target).GetPosition();
        const Vec3 dir = WintersMath::NormalizeXZ(Vec3{ targetPos.x - casterPos.x, 0.f, targetPos.z - casterPos.z });

        EnqueueTargetDamage(world, ctx.casterEntity, target, ctx.casterTeam, kRDamage, 4, ctx.skillRank);
        LeeSinKnockbackComponent knock{};
        knock.sourceEntity = ctx.casterEntity;
        knock.vStart = targetPos;
        knock.vEnd = Vec3{ targetPos.x + dir.x * kRKnockbackDistance, targetPos.y, targetPos.z + dir.z * kRKnockbackDistance };
        knock.fBaseY = targetPos.y;
        if (world.HasComponent<LeeSinKnockbackComponent>(target))
            world.GetComponent<LeeSinKnockbackComponent>(target) = knock;
        else
            world.AddComponent<LeeSinKnockbackComponent>(target, knock);

        StunComponent stun{};
        stun.fRemaining = knock.fDurationSec;
        stun.sourceEntity = ctx.casterEntity;
        if (world.HasComponent<StunComponent>(target))
            world.GetComponent<StunComponent>(target) = stun;
        else
            world.AddComponent<StunComponent>(target, stun);
    }
}

namespace LeeSinGameSim
{
    void RegisterHooks()
    {
        static bool_t s_bRegistered = false;
        if (s_bRegistered)
            return;
        CGameplayHookRegistry::Instance().Register(MakeGameplayHookId(eChampion::LEESIN, GameplayHookVariant::Q_CastFrame), &OnQ);
        CGameplayHookRegistry::Instance().Register(MakeGameplayHookId(eChampion::LEESIN, GameplayHookVariant::W_CastFrame), &OnW);
        CGameplayHookRegistry::Instance().Register(MakeGameplayHookId(eChampion::LEESIN, GameplayHookVariant::E_CastFrame), &OnE);
        CGameplayHookRegistry::Instance().Register(MakeGameplayHookId(eChampion::LEESIN, GameplayHookVariant::R_CastFrame), &OnR);
        s_bRegistered = true;
    }

    void Tick(CWorld& world, const TickContext& tc)
    {
        std::vector<EntityID> finishedDashes;
        world.ForEach<LeeSinDashComponent, TransformComponent>(
            std::function<void(EntityID, LeeSinDashComponent&, TransformComponent&)>(
                [&](EntityID entity, LeeSinDashComponent& dash, TransformComponent& transform)
                {
                    dash.fElapsedSec += tc.fDt;
                    f32_t t = dash.fDurationSec > 0.01f ? dash.fElapsedSec / dash.fDurationSec : 1.f;
                    if (t >= 1.f)
                    {
                        t = 1.f;
                        finishedDashes.push_back(entity);
                    }
                    Vec3 pos{
                        dash.vStart.x + (dash.vEnd.x - dash.vStart.x) * t,
                        dash.vStart.y + (dash.vEnd.y - dash.vStart.y) * t,
                        dash.vStart.z + (dash.vEnd.z - dash.vStart.z) * t
                    };
                    if (tc.pWalkable)
                    {
                        Vec3 guarded = pos;
                        if (tc.pWalkable->TryClampMoveSegmentXZ(transform.GetPosition(), pos, 0.5f, guarded))
                            pos = guarded;
                    }
                    transform.SetPosition(pos);
                }));
        for (EntityID entity : finishedDashes)
            world.RemoveComponent<LeeSinDashComponent>(entity);

        std::vector<EntityID> expiredMarks;
        world.ForEach<LeeSinQMarkComponent>(
            std::function<void(EntityID, LeeSinQMarkComponent&)>(
                [&](EntityID entity, LeeSinQMarkComponent& mark)
                {
                    mark.fRemainingSec = std::max(0.f, mark.fRemainingSec - tc.fDt);
                    if (mark.fRemainingSec <= 0.f)
                        expiredMarks.push_back(entity);
                }));
        for (EntityID entity : expiredMarks)
            world.RemoveComponent<LeeSinQMarkComponent>(entity);
    }
}
```

1-10. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp

`Phase_ServerProjectiles`에서 projectile hit damage를 넣기 직전, 기존 Tornado 처리 주변에 아래를 추가:

```cpp
            if (projectile.kind == eProjectileKind::LeeSinSonicWave)
            {
                LeeSinQMarkComponent mark{};
                mark.sourceEntity = projectile.sourceEntity;
                mark.fRemainingSec = 3.f;
                if (m_world.HasComponent<LeeSinQMarkComponent>(target))
                    m_world.GetComponent<LeeSinQMarkComponent>(target) = mark;
                else
                    m_world.AddComponent<LeeSinQMarkComponent>(target, mark);
            }
```

파일 상단 include에 아래를 추가:

```cpp
#include "Shared/GameSim/Champions/LeeSinGameSim.h"
#include "Shared/GameSim/Champions/KindredGameSim.h"
#include "Shared/GameSim/Champions/MasterYiGameSim.h"
#include "Shared/GameSim/Components/LeeSinSimComponent.h"
```

`Phase_SimulationSystems`의 기존 champion tick 목록 아래에 추가:

```cpp
    LeeSinGameSim::Tick(m_world, tc);
    KindredGameSim::Tick(m_world, tc);
    MasterYiGameSim::Tick(m_world, tc);
```

`InitializeServerSimSystems`의 기존 champion hook 등록 아래에 추가:

```cpp
    LeeSinGameSim::RegisterHooks();
    KindredGameSim::RegisterHooks();
    MasterYiGameSim::RegisterHooks();
```

1-11. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/KindredGameSim.cpp

빈 파일을 아래 구조로 교체한다.

```cpp
#include "Shared/GameSim/Champions/KindredGameSim.h"

#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/KindredSimComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Systems/DamagePipeline.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/StatusEffectSystem.h"

#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "Shared/GameSim/World.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

namespace
{
    constexpr f32_t kQDamage = 65.f;
    constexpr f32_t kQDashDistance = 3.4f;
    constexpr f32_t kQWallProbeExtra = 1.4f;
    constexpr f32_t kWDuration = 8.f;
    constexpr f32_t kEDamage = 80.f;
    constexpr f32_t kRDuration = 4.f;

    u16_t SkillId(u8_t slot)
    {
        return static_cast<u16_t>((static_cast<u32_t>(eChampion::KINDRED) << 8) | slot);
    }

    Vec3 ResolveDirection(const GameplayHookContext& ctx, const Vec3& origin)
    {
        if (ctx.pCommand)
        {
            Vec3 dir = WintersMath::NormalizeXZ(ctx.pCommand->direction);
            if (dir.x != 0.f || dir.z != 0.f)
                return dir;
            return WintersMath::NormalizeXZ(Vec3{
                ctx.pCommand->groundPos.x - origin.x,
                0.f,
                ctx.pCommand->groundPos.z - origin.z
            });
        }
        return Vec3{ 0.f, 0.f, 1.f };
    }

    Vec3 ResolveDashEnd(const GameplayHookContext& ctx, const Vec3& origin, const Vec3& dir)
    {
        Vec3 desired{ origin.x + dir.x * kQDashDistance, origin.y, origin.z + dir.z * kQDashDistance };
        if (!ctx.pTickCtx || !ctx.pTickCtx->pWalkable)
            return desired;

        Vec3 guarded{};
        if (ctx.pTickCtx->pWalkable->TryClampMoveSegmentXZ(origin, desired, 0.5f, guarded))
            return guarded;

        Vec3 wallHop{ origin.x + dir.x * (kQDashDistance + kQWallProbeExtra), origin.y,
            origin.z + dir.z * (kQDashDistance + kQWallProbeExtra) };
        if (ctx.pTickCtx->pWalkable->IsWalkableXZ(wallHop))
            return wallHop;
        return origin;
    }

    void StartDash(CWorld& world, EntityID caster, const Vec3& end)
    {
        if (!world.HasComponent<TransformComponent>(caster))
            return;
        KindredDashComponent dash{};
        dash.vStart = world.GetComponent<TransformComponent>(caster).GetPosition();
        dash.vEnd = end;
        if (world.HasComponent<KindredDashComponent>(caster))
            world.GetComponent<KindredDashComponent>(caster) = dash;
        else
            world.AddComponent<KindredDashComponent>(caster, dash);
        if (world.HasComponent<MoveTargetComponent>(caster))
            world.GetComponent<MoveTargetComponent>(caster).bHasTarget = false;
    }

    void EnqueueTargetDamage(CWorld& world, EntityID source, EntityID target,
        eTeam sourceTeam, f32_t damage, u8_t slot, u8_t rank)
    {
        DamageRequest req{};
        req.source = source;
        req.target = target;
        req.sourceTeam = sourceTeam;
        req.type = eDamageType::Physical;
        req.flatAmount = damage;
        req.skillId = SkillId(slot);
        req.rank = rank;
        req.flags = DamageFlag_OnHit;
        EnqueueDamageRequest(world, req);
    }

    void OnQ(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
            return;
        CWorld& world = *ctx.pWorld;
        const Vec3 origin = world.GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        const Vec3 dir = ResolveDirection(ctx, origin);
        StartDash(world, ctx.casterEntity, ResolveDashEnd(ctx, origin, dir));

        EntityID best = NULL_ENTITY;
        f32_t bestDistSq = 25.f;
        world.ForEach<ChampionComponent, TransformComponent>(
            std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                [&](EntityID target, ChampionComponent& champ, TransformComponent& xf)
                {
                    if (target == ctx.casterEntity || champ.team == ctx.casterTeam)
                        return;
                    const f32_t distSq = WintersMath::DistanceSqXZ(origin, xf.GetPosition());
                    if (distSq < bestDistSq)
                    {
                        bestDistSq = distSq;
                        best = target;
                    }
                }));
        if (best != NULL_ENTITY)
            EnqueueTargetDamage(world, ctx.casterEntity, best, ctx.casterTeam, kQDamage, 1, ctx.skillRank);
    }

    void OnW(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
            return;
        CWorld& world = *ctx.pWorld;
        KindredSimComponent state{};
        if (world.HasComponent<KindredSimComponent>(ctx.casterEntity))
            state = world.GetComponent<KindredSimComponent>(ctx.casterEntity);
        state.fWRemainingSec = kWDuration;
        state.vWCenter = world.GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        if (world.HasComponent<KindredSimComponent>(ctx.casterEntity))
            world.GetComponent<KindredSimComponent>(ctx.casterEntity) = state;
        else
            world.AddComponent<KindredSimComponent>(ctx.casterEntity, state);
    }

    void OnE(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand)
            return;
        CWorld& world = *ctx.pWorld;
        const EntityID target = ctx.pCommand->targetEntity;
        if (target == NULL_ENTITY)
            return;
        KindredEMarkComponent mark{};
        mark.sourceEntity = ctx.casterEntity;
        mark.fRemainingSec = 4.f;
        if (world.HasComponent<KindredEMarkComponent>(target))
            world.GetComponent<KindredEMarkComponent>(target) = mark;
        else
            world.AddComponent<KindredEMarkComponent>(target, mark);
        EnqueueTargetDamage(world, ctx.casterEntity, target, ctx.casterTeam, kEDamage, 3, ctx.skillRank);
    }

    void OnR(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand)
            return;
        CWorld& world = *ctx.pWorld;
        const Vec3 center = ctx.pCommand->groundPos;
        const f32_t radius = GetDefaultChampionSkillRange(eChampion::KINDRED, 4);
        world.ForEach<HealthComponent, TransformComponent>(
            std::function<void(EntityID, HealthComponent&, TransformComponent&)>(
                [&](EntityID target, HealthComponent&, TransformComponent& xf)
                {
                    if (WintersMath::DistanceSqXZ(center, xf.GetPosition()) > radius * radius)
                        return;
                    KindredHealthFloorComponent floor{};
                    floor.sourceEntity = ctx.casterEntity;
                    floor.vCenter = center;
                    floor.fRadius = radius;
                    floor.fRemainingSec = kRDuration;
                    floor.fMinHealth = 1.f;
                    if (world.HasComponent<KindredHealthFloorComponent>(target))
                        world.GetComponent<KindredHealthFloorComponent>(target) = floor;
                    else
                        world.AddComponent<KindredHealthFloorComponent>(target, floor);
                }));
    }
}

namespace KindredGameSim
{
    void RegisterHooks()
    {
        static bool_t s_bRegistered = false;
        if (s_bRegistered)
            return;
        CGameplayHookRegistry::Instance().Register(MakeGameplayHookId(eChampion::KINDRED, GameplayHookVariant::Q_CastFrame), &OnQ);
        CGameplayHookRegistry::Instance().Register(MakeGameplayHookId(eChampion::KINDRED, GameplayHookVariant::W_CastFrame), &OnW);
        CGameplayHookRegistry::Instance().Register(MakeGameplayHookId(eChampion::KINDRED, GameplayHookVariant::E_CastFrame), &OnE);
        CGameplayHookRegistry::Instance().Register(MakeGameplayHookId(eChampion::KINDRED, GameplayHookVariant::R_CastFrame), &OnR);
        s_bRegistered = true;
    }

    void Tick(CWorld& world, const TickContext& tc)
    {
        // KindredDashComponent 보간, KindredSimComponent W timer, KindredEMarkComponent timer,
        // KindredHealthFloorComponent timer/area health floor clamp를 이 함수에 실제 구현한다.
    }
}
```

`Tick`은 구현 시 위 주석을 남기지 말고 Yone/Yasuo Tick 패턴처럼 `std::vector<EntityID> finished`를 모아 remove한다.

1-12. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/MasterYiGameSim.cpp

빈 파일을 아래 구조로 교체한다.

```cpp
#include "Shared/GameSim/Champions/MasterYiGameSim.h"

#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/MasterYiSimComponent.h"
#include "Shared/GameSim/Systems/DamagePipeline.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "Shared/GameSim/World.h"

#include <algorithm>
#include <functional>
#include <vector>

namespace
{
    constexpr f32_t kQDuration = 0.75f;
    constexpr f32_t kQDamage = 90.f;
    constexpr f32_t kWDuration = 4.f;
    constexpr f32_t kWHealPerTick = 35.f;
    constexpr f32_t kWHealTickSec = 0.5f;
    constexpr f32_t kEDuration = 5.f;
    constexpr f32_t kRDuration = 7.f;
    constexpr f32_t kRMoveSpeedBonus = 2.0f;
    constexpr f32_t kRAttackSpeedBonus = 0.35f;

    u16_t SkillId(u8_t slot)
    {
        return static_cast<u16_t>((static_cast<u32_t>(eChampion::MASTERYI) << 8) | slot);
    }

    MasterYiSimComponent& EnsureState(CWorld& world, EntityID caster)
    {
        if (!world.HasComponent<MasterYiSimComponent>(caster))
            world.AddComponent<MasterYiSimComponent>(caster, MasterYiSimComponent{});
        return world.GetComponent<MasterYiSimComponent>(caster);
    }

    void AddGameplayState(CWorld& world, EntityID entity, u32_t flags)
    {
        auto& state = world.HasComponent<GameplayStateComponent>(entity)
            ? world.GetComponent<GameplayStateComponent>(entity)
            : world.AddComponent<GameplayStateComponent>(entity, GameplayStateComponent{});
        state.stateFlags |= flags;
    }

    void RemoveGameplayState(CWorld& world, EntityID entity, u32_t flags)
    {
        if (world.HasComponent<GameplayStateComponent>(entity))
            world.GetComponent<GameplayStateComponent>(entity).stateFlags &= ~flags;
    }

    void EnqueueTargetDamage(CWorld& world, EntityID source, EntityID target,
        eTeam sourceTeam, f32_t damage, u8_t slot, u8_t rank, eDamageType type = eDamageType::Physical)
    {
        DamageRequest req{};
        req.source = source;
        req.target = target;
        req.sourceTeam = sourceTeam;
        req.type = type;
        req.flatAmount = damage;
        req.skillId = SkillId(slot);
        req.rank = rank;
        req.flags = DamageFlag_OnHit;
        EnqueueDamageRequest(world, req);
    }

    void OnQ(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand)
            return;
        CWorld& world = *ctx.pWorld;
        const EntityID target = ctx.pCommand->targetEntity;
        MasterYiSimComponent& state = EnsureState(world, ctx.casterEntity);
        state.qTarget = target;
        state.fAlphaStrikeRemainingSec = kQDuration;
        if (world.HasComponent<TransformComponent>(ctx.casterEntity))
            state.vQReturnPosition = world.GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();

        AddGameplayState(world, ctx.casterEntity,
            kGameplayStateInvisibleFlag |
            kGameplayStateUntargetableFlag |
            kGameplayStateCannotMoveFlag |
            kGameplayStateCannotAttackFlag |
            kGameplayStateCannotCastFlag);

        if (target != NULL_ENTITY)
            EnqueueTargetDamage(world, ctx.casterEntity, target, ctx.casterTeam, kQDamage, 1, ctx.skillRank);
    }

    void OnW(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld)
            return;
        MasterYiSimComponent& state = EnsureState(*ctx.pWorld, ctx.casterEntity);
        state.fMeditateRemainingSec = kWDuration;
        state.fMeditateHealTickSec = 0.f;
        AddGameplayState(*ctx.pWorld, ctx.casterEntity,
            kGameplayStateCannotMoveFlag |
            kGameplayStateCannotAttackFlag |
            kGameplayStateCannotCastFlag);
    }

    void OnE(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld)
            return;
        MasterYiSimComponent& state = EnsureState(*ctx.pWorld, ctx.casterEntity);
        state.fWujuStyleRemainingSec = kEDuration;
    }

    void OnR(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pWorld->HasComponent<StatComponent>(ctx.casterEntity))
            return;
        CWorld& world = *ctx.pWorld;
        auto& yi = EnsureState(world, ctx.casterEntity);
        auto& stat = world.GetComponent<StatComponent>(ctx.casterEntity);
        if (yi.fHighlanderRemainingSec <= 0.f)
        {
            stat.moveSpeed += kRMoveSpeedBonus;
            stat.bonusAttackSpeed += kRAttackSpeedBonus;
            yi.fHighlanderMoveSpeedBonus = kRMoveSpeedBonus;
            yi.fHighlanderAttackSpeedBonus = kRAttackSpeedBonus;
        }
        yi.fHighlanderRemainingSec = kRDuration;
        stat.bDirty = true;
    }
}

namespace MasterYiGameSim
{
    void RegisterHooks()
    {
        static bool_t s_bRegistered = false;
        if (s_bRegistered)
            return;
        CGameplayHookRegistry::Instance().Register(MakeGameplayHookId(eChampion::MASTERYI, GameplayHookVariant::Q_CastFrame), &OnQ);
        CGameplayHookRegistry::Instance().Register(MakeGameplayHookId(eChampion::MASTERYI, GameplayHookVariant::W_CastFrame), &OnW);
        CGameplayHookRegistry::Instance().Register(MakeGameplayHookId(eChampion::MASTERYI, GameplayHookVariant::E_CastFrame), &OnE);
        CGameplayHookRegistry::Instance().Register(MakeGameplayHookId(eChampion::MASTERYI, GameplayHookVariant::R_CastFrame), &OnR);
        s_bRegistered = true;
    }

    void Tick(CWorld& world, const TickContext& tc)
    {
        // 구현 시: Q timer 종료 때 GameplayState flags 제거,
        // W heal tick마다 HealthComponent fCurrent 회복,
        // E timer 종료,
        // R timer 종료 때 StatComponent moveSpeed / bonusAttackSpeed 원복.
    }
}
```

`Tick`은 구현 시 주석으로 남기지 말고 실제 timer 감소/상태 복구 코드로 채운다.

1-13. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor.cpp

파일 상단 include에 아래를 추가:

```cpp
#include "ECS/Components/VisionComponents.h"
#include "Shared/GameSim/Components/LeeSinSimComponent.h"
#include "Shared/GameSim/Components/MasterYiSimComponent.h"
#include "Shared/GameSim/Components/NetEntityIdComponent.h"
```

`ExecuteCommand` switch의 `BuyItem` 아래에 추가:

```cpp
    case eCommandKind::UseItem:
        HandleUseItem(world, tc, cmd);
        break;
```

`HandleBuyItem` 아래에 아래 함수를 추가:

```cpp
void CDefaultCommandExecutor::HandleUseItem(CWorld& world, const TickContext& tc,
    const GameCommand& cmd)
{
    if (cmd.issuerEntity == NULL_ENTITY || cmd.slot != 4)
        return;
    if (!tc.pEntityMap || !world.HasComponent<TransformComponent>(cmd.issuerEntity))
        return;

    const eTeam ownerTeam = ResolveTeam(world, cmd.issuerEntity);
    const Vec3 ownerPos = world.GetComponent<TransformComponent>(cmd.issuerEntity).GetPosition();
    Vec3 pos = cmd.groundPos;
    if (WintersMath::DistanceSqXZ(ownerPos, pos) > 6.0f * 6.0f)
        return;
    if (tc.pWalkable)
    {
        Vec3 resolved = pos;
        if (!tc.pWalkable->TryClampMoveSegmentXZ(ownerPos, pos, 0.2f, resolved))
            return;
        pos = resolved;
    }

    const EntityID wardEntity = world.CreateEntity();
    TransformComponent transform{};
    transform.SetPosition(pos);
    world.AddComponent<TransformComponent>(wardEntity, transform);

    WardComponent ward{};
    ward.ownerTeam = ownerTeam;
    ward.remainingDuration = 90.f;
    world.AddComponent<WardComponent>(wardEntity, ward);

    LeeSinWardOwnerComponent owner{};
    owner.owner = cmd.issuerEntity;
    world.AddComponent<LeeSinWardOwnerComponent>(wardEntity, owner);

    VisionSourceComponent vision{};
    vision.sightRange = 9.f;
    world.AddComponent<VisionSourceComponent>(wardEntity, vision);

    VisibilityComponent visibility{};
    visibility.teamVisibilityMask = 0x03;
    world.AddComponent<VisibilityComponent>(wardEntity, visibility);

    SpatialAgentComponent spatial{};
    spatial.kind = eSpatialKind::Ward;
    spatial.team = static_cast<u8_t>(ownerTeam);
    spatial.radius = 0.35f;
    world.AddComponent<SpatialAgentComponent>(wardEntity, spatial);
    world.AddComponent<TargetableTag>(wardEntity);

    const NetEntityId netId = tc.pEntityMap->IssueNew(wardEntity);
    NetEntityIdComponent net{};
    net.netId = netId;
    world.AddComponent<NetEntityIdComponent>(wardEntity, net);
}
```

`HandleBasicAttack`에서 기본 damage request enqueue 직후 아래를 추가:

```cpp
    if (champion == eChampion::MASTERYI &&
        world.HasComponent<MasterYiSimComponent>(cmd.issuerEntity) &&
        world.GetComponent<MasterYiSimComponent>(cmd.issuerEntity).fWujuStyleRemainingSec > 0.f)
    {
        DamageRequest trueReq{};
        trueReq.source = cmd.issuerEntity;
        trueReq.target = cmd.targetEntity;
        trueReq.sourceTeam = sourceTeam;
        trueReq.type = eDamageType::True;
        trueReq.flatAmount = 30.f;
        trueReq.skillId = static_cast<u16_t>((static_cast<u32_t>(eChampion::MASTERYI) << 8) | 3u);
        trueReq.rank = ResolveSkillRank(world, cmd.issuerEntity, static_cast<u8_t>(eSkillSlot::E));
        EnqueueDamageRequest(world, trueReq);
    }
```

1-14. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ICommandExecutor.h

private 선언부의 `HandleBuyItem` 아래에 추가:

```cpp
    void HandleUseItem(CWorld&, const TickContext&, const GameCommand&);
```

1-15. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/DamagePipeline.cpp

include에 아래를 추가:

```cpp
#include "Shared/GameSim/Components/KindredSimComponent.h"
```

`hp.fCurrent` 차감 직전 기존 코드:

```cpp
    result.finalAmount = amount;

    auto& hp = world.GetComponent<HealthComponent>(req.target);
    hp.fCurrent = (hp.fCurrent > amount) ? (hp.fCurrent - amount) : 0.f;
```

아래로 교체:

```cpp
    auto& hp = world.GetComponent<HealthComponent>(req.target);
    if (world.HasComponent<KindredHealthFloorComponent>(req.target))
    {
        const auto& floor = world.GetComponent<KindredHealthFloorComponent>(req.target);
        if (floor.fRemainingSec > 0.f && hp.fCurrent - amount < floor.fMinHealth)
            amount = std::max(0.f, hp.fCurrent - floor.fMinHealth);
    }

    result.finalAmount = amount;
    hp.fCurrent = (hp.fCurrent > amount) ? (hp.fCurrent - amount) : 0.f;
```

1-16. C:/Users/user/Desktop/Winters/Server/Private/Game/SnapshotBuilder.cpp

include에 아래를 추가:

```cpp
#include "ECS/Components/VisionComponents.h"
```

`SkillProjectileComponent` entity kind 처리 아래에 추가:

```cpp
        if (world.HasComponent<WardComponent>(entity))
        {
            const auto& ward = world.GetComponent<WardComponent>(entity);
            entityKind = Shared::Schema::EntityKind::Ward;
            team = static_cast<u8_t>(ward.ownerTeam);
            subtype = ward.bControlWard ? 1u : 0u;
        }
```

1-17. C:/Users/user/Desktop/Winters/Client/Public/Network/Client/CommandSerializer.h

`SendBuyItem` 아래에 추가:

```cpp
    void SendUseItem(CClientNetwork& net, u8_t slot, const Vec3& groundPos);
```

1-18. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/CommandSerializer.cpp

`SendBuyItem` 아래에 추가:

```cpp
void CCommandSerializer::SendUseItem(CClientNetwork& net, u8_t slot, const Vec3& groundPos)
{
    GameCommandWire wire{};
    wire.kind = eCommandKind::UseItem;
    wire.clientTick = m_clientTick++;
    wire.sequenceNum = m_nextSequenceNum++;
    wire.slot = slot;
    wire.groundPos = groundPos;
    SendSingle(net, wire);
}
```

1-19. C:/Users/user/Desktop/Winters/Client/Private/Scene/InGameCombatInputBridge.cpp

`B` recall 처리 아래, `Q` 처리 위에 추가:

```cpp
        if (in.IsKeyPressed('4') && scene.IsNetworkAuthoritativeGameplay())
        {
            CCommandSerializer* pCommandSerializer = scene.GetCommandSerializer();
            CClientNetwork* pNetworkView = scene.GetNetworkView();
            if (pCommandSerializer && pNetworkView && pNetworkView->IsConnected())
            {
                ClearNetworkAttackIntent();
                pCommandSerializer->SendUseItem(*pNetworkView, 4, scene.ResolveMouseMapSurfacePos());
            }
        }
```

1-20. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp

`EnsureEntity`의 minion 처리 아래에 추가:

```cpp
    if (kind == Shared::Schema::EntityKind::Ward)
    {
        if (!world.HasComponent<WardComponent>(e))
        {
            WardComponent ward{};
            ward.ownerTeam = static_cast<eTeam>(team);
            ward.bControlWard = subtype != 0u;
            world.AddComponent<WardComponent>(e, ward);
        }
        if (!world.HasComponent<VisionSourceComponent>(e))
        {
            VisionSourceComponent vision{};
            vision.sightRange = 9.f;
            world.AddComponent<VisionSourceComponent>(e, vision);
        }
        if (!world.HasComponent<SpatialAgentComponent>(e))
        {
            SpatialAgentComponent spatial{};
            spatial.kind = eSpatialKind::Ward;
            spatial.team = team;
            spatial.radius = 0.35f;
            world.AddComponent<SpatialAgentComponent>(e, spatial);
        }
        if (!world.HasComponent<TargetableTag>(e))
            world.AddComponent<TargetableTag>(e);
    }
```

1-21. C:/Users/user/Desktop/Winters/Server/Include/Server.vcxproj

사용자가 프로젝트 파일 수정까지 진행할 때만 아래 XML 항목을 기존 Shared/GameSim Champions/Components 그룹에 추가:

```xml
    <ClCompile Include="..\..\Shared\GameSim\Champions\LeeSinGameSim.cpp" />
    <ClCompile Include="..\..\Shared\GameSim\Champions\KindredGameSim.cpp" />
    <ClCompile Include="..\..\Shared\GameSim\Champions\MasterYiGameSim.cpp" />
    <ClInclude Include="..\..\Shared\GameSim\Champions\LeeSinGameSim.h" />
    <ClInclude Include="..\..\Shared\GameSim\Champions\KindredGameSim.h" />
    <ClInclude Include="..\..\Shared\GameSim\Champions\MasterYiGameSim.h" />
    <ClInclude Include="..\..\Shared\GameSim\Components\LeeSinSimComponent.h" />
    <ClInclude Include="..\..\Shared\GameSim\Components\KindredSimComponent.h" />
    <ClInclude Include="..\..\Shared\GameSim\Components\MasterYiSimComponent.h" />
```

1-22. C:/Users/user/Desktop/Winters/Server/Include/Server.vcxproj.filters

사용자가 프로젝트 파일 수정까지 진행할 때만 아래 XML 항목을 기존 `04. Shared\GameSim\Champions` / `04. Shared\GameSim\Components` filter에 추가:

```xml
    <ClCompile Include="..\..\Shared\GameSim\Champions\LeeSinGameSim.cpp">
      <Filter>04. Shared\GameSim\Champions</Filter>
    </ClCompile>
    <ClCompile Include="..\..\Shared\GameSim\Champions\KindredGameSim.cpp">
      <Filter>04. Shared\GameSim\Champions</Filter>
    </ClCompile>
    <ClCompile Include="..\..\Shared\GameSim\Champions\MasterYiGameSim.cpp">
      <Filter>04. Shared\GameSim\Champions</Filter>
    </ClCompile>
    <ClInclude Include="..\..\Shared\GameSim\Components\LeeSinSimComponent.h">
      <Filter>04. Shared\GameSim\Components</Filter>
    </ClInclude>
    <ClInclude Include="..\..\Shared\GameSim\Components\KindredSimComponent.h">
      <Filter>04. Shared\GameSim\Components</Filter>
    </ClInclude>
    <ClInclude Include="..\..\Shared\GameSim\Components\MasterYiSimComponent.h">
      <Filter>04. Shared\GameSim\Components</Filter>
    </ClInclude>
```

2. 검증

미검증:
- KindredGameSim.cpp / MasterYiGameSim.cpp는 현재 0바이트라 프로젝트에 추가하면 구현 전에는 빌드 차단 가능.
- LeeSinGameSim.cpp는 현재 주석-only라 프로젝트에 추가해도 hook symbol이 없으면 링크/동작 검증 실패.
- Ward visual은 snapshot entity 생성까지 확인한 뒤 별도 모델/marker 표시를 F5에서 확인 필요.

검증 명령:
- `git diff --check`
- `"C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`
- `"C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`

수동 확인:
- LeeSin Q1 projectile hit 후 mark가 생기고 Q2가 marked target으로 이동한다.
- LeeSin W가 아군 champion, minion, ward에는 성공하고 적 champion/minion/ward에는 실패한다.
- 4 키 ward가 서버 entity로 생성되고 snapshot에 `EntityKind::Ward`로 내려온다.
- Ward에 `VisionSourceComponent`가 붙어 fog/visibility가 열린다.
- Kindred Q가 짧은 벽을 넘되 비정상 장거리 순간이동은 하지 않는다.
- Kindred R 영역에서 HP가 1 아래로 내려가지 않고, 영역/시간 종료 뒤 damage가 정상 적용된다.
- MasterYi Q 중 target 불가/보이지 않음 상태가 적용되고 timer 종료 뒤 원복된다.
- MasterYi W가 channel 동안 회복하고 movement/cast/attack 제한이 timer 종료 뒤 원복된다.
- MasterYi E 동안 기본 공격에 true damage가 추가되고, R 동안 move speed / attack speed가 증가 후 원복된다.
