#include "Shared/GameSim/Champions/Fiora/FioraGameSim.h"

#include "Shared/GameSim/Components/FioraSimComponent.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Core/Ecs/TransformComponent.h"
#include "Shared/GameSim/Core/World/World.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Definitions/GameplayDefinitionQuery.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"
#include "Shared/GameSim/Systems/Move/DashArrival.h"
#include "Shared/GameSim/Systems/ReplicatedEventQueue/ReplicatedEventQueue.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectRequests.h"

#include "Shared/GameSim/Core/Checkpoint/KeyframeComponentRegistry.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <vector>

namespace
{
    f32_t ResolveFioraSkillEffectParam(
        const GameplayHookContext& ctx,
        eSkillSlot slot,
        eSkillEffectParamId param,
        f32_t fallbackValue = 0.f)
    {
        if (!ctx.pWorld || !ctx.pTickCtx)
            return fallbackValue;

        return GameplayDefinitionQuery::ResolveSkillEffectParam(
            *ctx.pWorld,
            ctx.casterEntity,
            *ctx.pTickCtx,
            eChampion::FIORA,
            static_cast<u8_t>(slot),
            param,
            fallbackValue);
    }

    f32_t ResolveFioraSkillEffectParam(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        eSkillSlot slot,
        eSkillEffectParamId param)
    {
        return GameplayDefinitionQuery::ResolveSkillEffectParam(
            world,
            caster,
            tc,
            eChampion::FIORA,
            static_cast<u8_t>(slot),
            param,
            0.f);
    }

    struct FioraDashComponent
    {
        Vec3 start{};
        Vec3 end{};
        f32_t elapsedSec = 0.f;
        f32_t durationSec = 0.f;
    };

    const bool_t s_bFioraDashKeyframeRegistered = []()
    {
        SimCheckpoint::KeyframeComponentRegistry::Get()
            .Register<FioraDashComponent>("FioraDashComponent");
        return true;
    }();

    FioraSimComponent& EnsureFioraState(CWorld& world, EntityID caster)
    {
        if (!world.HasComponent<FioraSimComponent>(caster))
            world.AddComponent<FioraSimComponent>(caster, FioraSimComponent{});

        return world.GetComponent<FioraSimComponent>(caster);
    }

    void ClearMove(CWorld& world, EntityID entity)
    {
        if (world.HasComponent<MoveTargetComponent>(entity))
            world.GetComponent<MoveTargetComponent>(entity).bHasTarget = false;
    }

    u16_t BuildFioraEffectFlags(eSkillSlot slot, u8_t stage, u8_t ordinal)
    {
        return static_cast<u16_t>(
            (static_cast<u16_t>(stage & 0x0fu) << 12) |
            (static_cast<u16_t>(ordinal & 0x0fu) << 8) |
            static_cast<u16_t>(slot));
    }

    Vec3 VitalDirection(u8_t direction)
    {
        switch (direction & 3u)
        {
        case 0: return { 1.f, 0.f, 0.f };
        case 1: return { -1.f, 0.f, 0.f };
        case 2: return { 0.f, 0.f, 1.f };
        default: return { 0.f, 0.f, -1.f };
        }
    }

    Vec3 ResolveEntityPosition(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<TransformComponent>(entity))
            return world.GetComponent<TransformComponent>(entity).GetPosition();
        return {};
    }

    Vec3 ResolveCommandOrFacingDirection(CWorld& world, const GameplayHookContext& ctx)
    {
        if (ctx.pCommand)
        {
            const Vec3 commandDirection = WintersMath::NormalizeXZ(
                ctx.pCommand->direction,
                Vec3{},
                0.0001f);
            if (commandDirection.x != 0.f || commandDirection.z != 0.f)
                return commandDirection;
        }

        if (world.HasComponent<TransformComponent>(ctx.casterEntity))
        {
            return WintersMath::DirectionFromYawXZ(
                world.GetComponent<TransformComponent>(ctx.casterEntity)
                    .GetRotation().y);
        }

        return { 0.f, 0.f, 1.f };
    }

    void EmitFioraEffect(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        EntityID target,
        u16_t variant,
        eSkillSlot slot,
        u8_t stage,
        u8_t ordinal,
        const Vec3& position,
        const Vec3& direction,
        u16_t durationMs,
        f32_t effectLength = 0.f,
        f32_t effectHalfWidth = 0.f)
    {
        // Passive cues are presentation-only.  Keep their semantic skillId as
        // BasicAttack, but use a non-BA event slot so the generic client effect
        // path does not replay a basic-attack action on vital spawn/clear.
        const eSkillSlot eventSlot =
            variant == GameplayHookVariant::Passive_Trigger
                ? eSkillSlot::W
                : slot;
        ReplicatedEventComponent event{};
        event.kind = eReplicatedEventKind::EffectTrigger;
        event.sourceEntity = caster;
        event.targetEntity = target;
        event.effectId = MakeGameplayHookId(eChampion::FIORA, variant);
        event.skillId = static_cast<u16_t>(
            (static_cast<u32_t>(eChampion::FIORA) << 8) |
            static_cast<u8_t>(slot));
        event.slot = static_cast<u8_t>(eventSlot);
        event.rank = 1u;
        event.flags = BuildFioraEffectFlags(eventSlot, stage, ordinal);
        event.sourceChampion = eChampion::FIORA;
        if (world.HasComponent<ChampionComponent>(caster))
        {
            event.sourceTeam = static_cast<u8_t>(
                world.GetComponent<ChampionComponent>(caster).team);
        }
        if (target != NULL_ENTITY && world.HasComponent<ChampionComponent>(target))
        {
            const ChampionComponent& targetChampion =
                world.GetComponent<ChampionComponent>(target);
            event.targetChampion = static_cast<eChampion>(targetChampion.id.value);
            event.targetTeam = static_cast<u8_t>(targetChampion.team);
        }
        event.position = position;
        event.direction = direction;
        event.durationMs = durationMs;
        event.effectLength = effectLength;
        event.effectHalfWidth = effectHalfWidth;
        event.startTick = tc.tickIndex;
        EnqueueReplicatedEvent(world, event);
    }

    void EnqueuePhysicalDamage(
        CWorld& world,
        EntityID source,
        EntityID target,
        eTeam sourceTeam,
        f32_t amount,
        eSkillSlot slot,
        u8_t rank)
    {
        if (target == NULL_ENTITY || !world.IsAlive(target))
            return;

        DamageRequest request{};
        request.source = source;
        request.target = target;
        request.sourceTeam = sourceTeam;
        request.type = eDamageType::Physical;
        request.flatAmount = amount;
        request.skillId = static_cast<u16_t>(
            (static_cast<u32_t>(eChampion::FIORA) << 8) |
            static_cast<u8_t>(slot));
        request.rank = rank;
        request.iSourceSlot = static_cast<u8_t>(slot);
        request.eSourceKind = eDamageSourceKind::Skill;
        request.flags = DamageFlag_OnHit;
        EnqueueDamageRequest(world, request);
    }

    EntityID FindEnemyInCone(
        CWorld& world,
        EntityID caster,
        eTeam casterTeam,
        const Vec3& origin,
        const Vec3& direction,
        f32_t range,
        f32_t radius)
    {
        const Vec3 fwd = WintersMath::NormalizeXZ(direction, Vec3{ 0.f, 0.f, 1.f });
        EntityID best = NULL_ENTITY;
        f32_t bestDistSq = (range + radius) * (range + radius);
        const auto champions =
            DeterministicEntityIterator<ChampionComponent>::CollectSorted(world);

        for (EntityID entity : champions)
        {
            if (entity == caster ||
                !world.IsAlive(entity) ||
                !world.HasComponent<TransformComponent>(entity) ||
                !world.HasComponent<HealthComponent>(entity))
            {
                continue;
            }

            const ChampionComponent& champion = world.GetComponent<ChampionComponent>(entity);
            const HealthComponent& health = world.GetComponent<HealthComponent>(entity);
            if (champion.team == casterTeam || health.bIsDead || health.fCurrent <= 0.f ||
                !GameplayStateQuery::CanBeTargetedBy(world, caster, entity) ||
                !GameplayStateQuery::CanReceiveDamage(world, caster, entity))
            {
                continue;
            }

            const Vec3 enemyPos =
                world.GetComponent<TransformComponent>(entity).GetPosition();
            const f32_t distSq = WintersMath::DistanceSqXZ(origin, enemyPos);
            if (distSq > (range + radius) * (range + radius))
                continue;

            const f32_t dist = std::sqrt(std::max(0.0001f, distSq));
            const f32_t dot =
                fwd.x * ((enemyPos.x - origin.x) / dist) +
                fwd.z * ((enemyPos.z - origin.z) / dist);
            if (dot < 0.5f)
                continue;

            if (distSq < bestDistSq)
            {
                bestDistSq = distSq;
                best = entity;
            }
        }

        return best;
    }

    EntityID FindEnemyInDirectionalStrip(
        CWorld& world,
        EntityID caster,
        eTeam casterTeam,
        const Vec3& origin,
        const Vec3& direction,
        f32_t length,
        f32_t halfWidth)
    {
        const Vec3 forward = WintersMath::NormalizeXZ(
            direction, Vec3{ 0.f, 0.f, 1.f });
        const Vec3 right{ forward.z, 0.f, -forward.x };
        EntityID best = NULL_ENTITY;
        f32_t bestDistanceSq = (std::numeric_limits<f32_t>::max)();
        const auto champions =
            DeterministicEntityIterator<ChampionComponent>::CollectSorted(world);

        for (const EntityID entity : champions)
        {
            if (entity == caster ||
                !world.IsAlive(entity) ||
                !world.HasComponent<TransformComponent>(entity) ||
                !world.HasComponent<HealthComponent>(entity))
            {
                continue;
            }

            const ChampionComponent& champion =
                world.GetComponent<ChampionComponent>(entity);
            const HealthComponent& health =
                world.GetComponent<HealthComponent>(entity);
            if (champion.team == casterTeam ||
                health.bIsDead || health.fCurrent <= 0.f ||
                !GameplayStateQuery::CanBeTargetedBy(world, caster, entity) ||
                !GameplayStateQuery::CanReceiveDamage(world, caster, entity))
            {
                continue;
            }

            const Vec3 enemyPos = ResolveEntityPosition(world, entity);
            const Vec3 delta{
                enemyPos.x - origin.x,
                0.f,
                enemyPos.z - origin.z
            };
            const f32_t forwardDistance =
                delta.x * forward.x + delta.z * forward.z;
            const f32_t lateralDistance = std::fabs(
                delta.x * right.x + delta.z * right.z);
            if (forwardDistance < 0.f || forwardDistance > length ||
                lateralDistance > halfWidth)
            {
                continue;
            }

            const f32_t distanceSq =
                delta.x * delta.x + delta.z * delta.z;
            if (distanceSq < bestDistanceSq)
            {
                bestDistanceSq = distanceSq;
                best = entity;
            }
        }

        return best;
    }

    void ClearPassiveVital(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        FioraSimComponent& state,
        bool_t bStartRespawn)
    {
        if (state.bPassiveVitalActive)
        {
            EmitFioraEffect(
                world,
                tc,
                caster,
                state.passiveVitalTarget,
                GameplayHookVariant::Passive_Trigger,
                eSkillSlot::BasicAttack,
                3u,
                static_cast<u8_t>(state.passiveVitalDirection + 1u),
                ResolveEntityPosition(world, state.passiveVitalTarget),
                VitalDirection(state.passiveVitalDirection),
                0u);
        }

        state.bPassiveVitalActive = false;
        state.passiveVitalTarget = NULL_ENTITY;
        state.passiveVitalTimerSec = 0.f;
        state.passiveRespawnTimerSec = bStartRespawn
            ? ResolveFioraSkillEffectParam(
                world,
                tc,
                caster,
                eSkillSlot::BasicAttack,
                eSkillEffectParamId::RespawnSec)
            : 0.f;
    }

    void SpawnPassiveVital(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        FioraSimComponent& state)
    {
        if (state.bGrandChallengeActive ||
            !world.IsAlive(caster) ||
            !world.HasComponent<ChampionComponent>(caster) ||
            !world.HasComponent<TransformComponent>(caster))
        {
            return;
        }

        const ChampionComponent& casterChampion =
            world.GetComponent<ChampionComponent>(caster);
        const Vec3 casterPosition =
            world.GetComponent<TransformComponent>(caster).GetPosition();
        const f32_t acquireRange = ResolveFioraSkillEffectParam(
            world,
            tc,
            caster,
            eSkillSlot::BasicAttack,
            eSkillEffectParamId::AcquireRange);
        const f32_t acquireRangeSq = acquireRange * acquireRange;
        f32_t bestDistanceSq = std::numeric_limits<f32_t>::max();
        EntityID bestTarget = NULL_ENTITY;
        const auto champions =
            DeterministicEntityIterator<ChampionComponent>::CollectSorted(world);

        for (EntityID candidate : champions)
        {
            if (candidate == caster ||
                !world.IsAlive(candidate) ||
                !world.HasComponent<TransformComponent>(candidate) ||
                !world.HasComponent<HealthComponent>(candidate))
            {
                continue;
            }

            const ChampionComponent& targetChampion =
                world.GetComponent<ChampionComponent>(candidate);
            const HealthComponent& targetHealth =
                world.GetComponent<HealthComponent>(candidate);
            if (targetChampion.team == casterChampion.team ||
                targetHealth.bIsDead ||
                targetHealth.fCurrent <= 0.f ||
                !GameplayStateQuery::CanBeTargetedBy(world, caster, candidate) ||
                !GameplayStateQuery::CanReceiveDamage(world, caster, candidate))
            {
                continue;
            }

            const f32_t distanceSq = WintersMath::DistanceSqXZ(
                casterPosition,
                world.GetComponent<TransformComponent>(candidate).GetPosition());
            if (distanceSq > acquireRangeSq || distanceSq >= bestDistanceSq)
                continue;

            bestDistanceSq = distanceSq;
            bestTarget = candidate;
        }

        if (bestTarget == NULL_ENTITY)
            return;

        const u32_t randomValue = tc.pRng
            ? tc.pRng->NextU32()
            : static_cast<u32_t>(tc.tickIndex ^ caster);
        state.bPassiveVitalActive = true;
        state.passiveVitalTarget = bestTarget;
        state.passiveVitalDirection = static_cast<u8_t>(randomValue & 3u);
        state.passiveVitalTimerSec = ResolveFioraSkillEffectParam(
            world,
            tc,
            caster,
            eSkillSlot::BasicAttack,
            eSkillEffectParamId::LifetimeSec);
        state.passiveRespawnTimerSec = 0.f;

        EmitFioraEffect(
            world,
            tc,
            caster,
            bestTarget,
            GameplayHookVariant::Passive_Trigger,
            eSkillSlot::BasicAttack,
            1u,
            static_cast<u8_t>(state.passiveVitalDirection + 1u),
            ResolveEntityPosition(world, bestTarget),
            VitalDirection(state.passiveVitalDirection),
            8000u);
    }

    void ReleaseRiposte(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        FioraSimComponent& state)
    {
        state.bRiposteActive = false;
        const Vec3 origin = ResolveEntityPosition(world, caster);
        const Vec3 endpoint{
            origin.x + state.riposteDirection.x * state.riposteRange,
            origin.y,
            origin.z + state.riposteDirection.z * state.riposteRange
        };
        EmitFioraEffect(
            world,
            tc,
            caster,
            NULL_ENTITY,
            GameplayHookVariant::W_Recovery,
            eSkillSlot::W,
            1u,
            0u,
            endpoint,
            state.riposteDirection,
            300u,
            state.riposteRange,
            state.riposteRadius);

        if (!world.HasComponent<ChampionComponent>(caster))
        {
            state.bRiposteReleasePending = false;
            state.bRiposteCaughtHardCC = false;
            state.riposteReleaseTarget = NULL_ENTITY;
            return;
        }

        const eTeam casterTeam = world.GetComponent<ChampionComponent>(caster).team;
        const EntityID hitTarget = FindEnemyInDirectionalStrip(
            world,
            caster,
            casterTeam,
            origin,
            state.riposteDirection,
            state.riposteRange,
            state.riposteRadius);
        state.riposteReleaseTarget = hitTarget;
        state.bRiposteReleasePending = hitTarget != NULL_ENTITY;

        if (hitTarget == NULL_ENTITY)
        {
            state.bRiposteCaughtHardCC = false;
            return;
        }

        EnqueuePhysicalDamage(
            world,
            caster,
            hitTarget,
            casterTeam,
            state.riposteDamage,
            eSkillSlot::W,
            state.riposteSkillRank);
    }

    void HealGrandChallengeAllies(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        const FioraSimComponent& state)
    {
        if (!world.HasComponent<ChampionComponent>(caster))
            return;

        const eTeam casterTeam = world.GetComponent<ChampionComponent>(caster).team;
        const f32_t radius = ResolveFioraSkillEffectParam(
            world,
            tc,
            caster,
            eSkillSlot::R,
            eSkillEffectParamId::HealRadius);
        const f32_t radiusSq = radius * radius;
        const f32_t healAmount = ResolveFioraSkillEffectParam(
            world,
            tc,
            caster,
            eSkillSlot::R,
            eSkillEffectParamId::HealAmount);
        const auto champions =
            DeterministicEntityIterator<ChampionComponent>::CollectSorted(world);

        for (EntityID ally : champions)
        {
            if (!world.IsAlive(ally) ||
                !world.HasComponent<HealthComponent>(ally) ||
                !world.HasComponent<TransformComponent>(ally))
            {
                continue;
            }

            ChampionComponent& champion = world.GetComponent<ChampionComponent>(ally);
            HealthComponent& health = world.GetComponent<HealthComponent>(ally);
            if (champion.team != casterTeam || health.bIsDead || health.fCurrent <= 0.f ||
                WintersMath::DistanceSqXZ(
                    state.grandChallengeHealCenter,
                    world.GetComponent<TransformComponent>(ally).GetPosition()) > radiusSq)
            {
                continue;
            }

            health.fCurrent = std::min(
                health.fMaximum,
                health.fCurrent + healAmount);
            champion.hp = health.fCurrent;
            champion.maxHp = health.fMaximum;
        }
    }

    void StartGrandChallengeHeal(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        EntityID target,
        FioraSimComponent& state)
    {
        state.grandChallengeHealCenter = ResolveEntityPosition(world, target);
        state.bGrandChallengeActive = false;
        state.grandChallengeActiveMask = 0u;
        state.grandChallengeTimerSec = 0.f;
        state.grandChallengeTarget = NULL_ENTITY;
        state.bGrandChallengeHealActive = true;
        state.grandChallengeHealTimerSec = ResolveFioraSkillEffectParam(
            world,
            tc,
            caster,
            eSkillSlot::R,
            eSkillEffectParamId::HealDurationSec);
        state.grandChallengeHealTickTimerSec = ResolveFioraSkillEffectParam(
            world,
            tc,
            caster,
            eSkillSlot::R,
            eSkillEffectParamId::HealIntervalSec);

        EmitFioraEffect(
            world,
            tc,
            caster,
            target,
            GameplayHookVariant::R_Recovery,
            eSkillSlot::R,
            3u,
            0u,
            state.grandChallengeHealCenter,
            {},
            5000u);
    }

    bool_t TryConsumeVital(
        CWorld& world,
        const TickContext& tc,
        const DamageRequest& request,
        FioraSimComponent& state)
    {
        if (request.source == NULL_ENTITY ||
            request.target == NULL_ENTITY ||
            !world.HasComponent<TransformComponent>(request.source) ||
            !world.HasComponent<TransformComponent>(request.target))
        {
            return false;
        }

        const Vec3 sourcePosition =
            world.GetComponent<TransformComponent>(request.source).GetPosition();
        const Vec3 targetPosition =
            world.GetComponent<TransformComponent>(request.target).GetPosition();
        const Vec3 targetToSource = WintersMath::DirectionXZ(
            targetPosition,
            sourcePosition,
            Vec3{});

        u8_t consumedDirection = 0u;
        bool_t bConsumedGrandChallenge = false;
        if (state.bGrandChallengeActive &&
            state.grandChallengeTarget == request.target)
        {
            for (u8_t direction = 0u; direction < 4u; ++direction)
            {
                const u8_t directionBit = static_cast<u8_t>(1u << direction);
                if ((state.grandChallengeActiveMask & directionBit) == 0u)
                    continue;

                const Vec3 vitalDirection = VitalDirection(direction);
                const f32_t dot =
                    targetToSource.x * vitalDirection.x +
                    targetToSource.z * vitalDirection.z;
                const f32_t sideDotThreshold = ResolveFioraSkillEffectParam(
                    world,
                    tc,
                    request.source,
                    eSkillSlot::BasicAttack,
                    eSkillEffectParamId::SideDotThreshold);
                if (dot < sideDotThreshold)
                    continue;

                consumedDirection = direction;
                state.grandChallengeActiveMask = static_cast<u8_t>(
                    state.grandChallengeActiveMask & ~directionBit);
                bConsumedGrandChallenge = true;
                EmitFioraEffect(
                    world,
                    tc,
                    request.source,
                    request.target,
                    GameplayHookVariant::R_Recovery,
                    eSkillSlot::R,
                    2u,
                    static_cast<u8_t>(direction + 1u),
                    targetPosition,
                    vitalDirection,
                    450u);
                break;
            }
        }

        bool_t bConsumedPassive = false;
        if (!bConsumedGrandChallenge &&
            state.bPassiveVitalActive &&
            state.passiveVitalTarget == request.target)
        {
            const Vec3 vitalDirection = VitalDirection(state.passiveVitalDirection);
            const f32_t dot =
                targetToSource.x * vitalDirection.x +
                targetToSource.z * vitalDirection.z;
            const f32_t sideDotThreshold = ResolveFioraSkillEffectParam(
                world,
                tc,
                request.source,
                eSkillSlot::BasicAttack,
                eSkillEffectParamId::SideDotThreshold);
            if (dot >= sideDotThreshold)
            {
                consumedDirection = state.passiveVitalDirection;
                bConsumedPassive = true;
                EmitFioraEffect(
                    world,
                    tc,
                    request.source,
                    request.target,
                    GameplayHookVariant::Passive_Trigger,
                    eSkillSlot::BasicAttack,
                    2u,
                    static_cast<u8_t>(consumedDirection + 1u),
                    targetPosition,
                    vitalDirection,
                    450u);
                state.bPassiveVitalActive = false;
                state.passiveVitalTarget = NULL_ENTITY;
                state.passiveVitalTimerSec = 0.f;
                state.passiveRespawnTimerSec = ResolveFioraSkillEffectParam(
                    world,
                    tc,
                    request.source,
                    eSkillSlot::BasicAttack,
                    eSkillEffectParamId::RespawnSec);
            }
        }

        if (!bConsumedGrandChallenge && !bConsumedPassive)
            return false;

        DamageRequest bonusDamage{};
        bonusDamage.source = request.source;
        bonusDamage.target = request.target;
        bonusDamage.sourceTeam = request.sourceTeam;
        bonusDamage.type = eDamageType::True;
        bonusDamage.targetMaxHpRatioOverride = ResolveFioraSkillEffectParam(
            world,
            tc,
            request.source,
            eSkillSlot::BasicAttack,
            eSkillEffectParamId::TargetMaxHpRatio);
        bonusDamage.eSourceKind = eDamageSourceKind::Rune;
        bonusDamage.flags = DamageFlag_None;
        EnqueueDamageRequest(world, bonusDamage);

        if (bConsumedGrandChallenge && state.grandChallengeActiveMask == 0u)
        {
            StartGrandChallengeHeal(
                world,
                tc,
                request.source,
                request.target,
                state);
        }

        return true;
    }

    void OnQ(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld ||
            !ctx.pCommand ||
            !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
        {
            return;
        }

        CWorld& world = *ctx.pWorld;
        auto& transform = world.GetComponent<TransformComponent>(ctx.casterEntity);
        const Vec3 origin = transform.GetPosition();
        const Vec3 direction = WintersMath::NormalizeXZ(ctx.pCommand->direction);
        const f32_t qDashDistance = ResolveFioraSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::DashDistance);
        const f32_t qDashDurationSec = ResolveFioraSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::DashDurationSec);
        const f32_t qRange = ResolveFioraSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::Range);
        const f32_t qRadius = ResolveFioraSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::Radius);
        const f32_t qDamage = ResolveFioraSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::BaseDamage);

        const Vec3 destination{
            origin.x + direction.x * qDashDistance,
            origin.y,
            origin.z + direction.z * qDashDistance
        };

        FioraDashComponent dash{};
        dash.start = origin;
        dash.end = destination;
        dash.durationSec = qDashDurationSec;
        if (world.HasComponent<FioraDashComponent>(ctx.casterEntity))
            world.GetComponent<FioraDashComponent>(ctx.casterEntity) = dash;
        else
            world.AddComponent<FioraDashComponent>(ctx.casterEntity, dash);

        const Vec3 rotation = transform.GetRotation();
        transform.SetRotation(Vec3{
            rotation.x,
            ResolveChampionVisualYawNear(eChampion::FIORA, direction, rotation.y),
            rotation.z
        });

        ClearMove(world, ctx.casterEntity);

        const EntityID hitTarget = FindEnemyInCone(
            world,
            ctx.casterEntity,
            ctx.casterTeam,
            origin,
            direction,
            qRange,
            qRadius);

        EnqueuePhysicalDamage(
            world,
            ctx.casterEntity,
            hitTarget,
            ctx.casterTeam,
            qDamage,
            eSkillSlot::Q,
            ctx.skillRank);
    }

    void OnW(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld ||
            !ctx.pTickCtx ||
            !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
        {
            return;
        }

        CWorld& world = *ctx.pWorld;
        FioraSimComponent& state = EnsureFioraState(world, ctx.casterEntity);
        state.bRiposteActive = true;
        state.bRiposteCaughtHardCC = false;
        state.bRiposteReleasePending = false;
        state.riposteReleaseTarget = NULL_ENTITY;
        state.riposteSkillRank = std::max<u8_t>(1u, ctx.skillRank);
        state.riposteWindowSec = ResolveFioraSkillEffectParam(
            ctx,
            eSkillSlot::W,
            eSkillEffectParamId::EffectDurationSec,
            0.75f);
        state.riposteTimerSec = state.riposteWindowSec;
        state.riposteDirection = ResolveCommandOrFacingDirection(world, ctx);
        state.riposteRange = std::max(0.f, GameplayDefinitionQuery::ResolveSkillRange(
            world,
            ctx.casterEntity,
            *ctx.pTickCtx,
            eChampion::FIORA,
            static_cast<u8_t>(eSkillSlot::W)));
        state.riposteRadius = ResolveFioraSkillEffectParam(
            ctx,
            eSkillSlot::W,
            eSkillEffectParamId::Radius,
            0.8f);
        state.riposteDamage = ResolveFioraSkillEffectParam(
            ctx,
            eSkillSlot::W,
            eSkillEffectParamId::BaseDamage,
            80.f);
        if (state.riposteDamage <= 0.f)
            state.riposteDamage = 80.f;
        state.riposteSlowDurationSec = ResolveFioraSkillEffectParam(
            ctx,
            eSkillSlot::W,
            eSkillEffectParamId::SlowDurationSec,
            1.5f);
        state.riposteSlowMoveSpeedMul = ResolveFioraSkillEffectParam(
            ctx,
            eSkillSlot::W,
            eSkillEffectParamId::MoveSpeedMul,
            0.5f);
        state.riposteStunDurationSec = 1.5f;
        TransformComponent& transform =
            world.GetComponent<TransformComponent>(ctx.casterEntity);
        const Vec3 rotation = transform.GetRotation();
        transform.SetRotation(Vec3{
            rotation.x,
            ResolveChampionVisualYawNear(
                eChampion::FIORA,
                state.riposteDirection,
                rotation.y),
            rotation.z
        });
        ClearMove(world, ctx.casterEntity);

        const Vec3 origin = ResolveEntityPosition(world, ctx.casterEntity);
        const Vec3 endpoint{
            origin.x + state.riposteDirection.x * state.riposteRange,
            origin.y,
            origin.z + state.riposteDirection.z * state.riposteRange
        };

        EmitFioraEffect(
            world,
            *ctx.pTickCtx,
            ctx.casterEntity,
            ctx.casterEntity,
            GameplayHookVariant::W_CastFrame,
            eSkillSlot::W,
            1u,
            0u,
            endpoint,
            state.riposteDirection,
            static_cast<u16_t>(
                std::clamp(state.riposteWindowSec, 0.f, 65.535f) * 1000.f),
            state.riposteRange,
            state.riposteRadius);
    }

    void OnE(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pTickCtx)
            return;

        const f32_t eWindowSec = ResolveFioraSkillEffectParam(
            ctx,
            eSkillSlot::E,
            eSkillEffectParamId::EffectDurationSec,
            5.0f);
        const f32_t eMaxStacks = ResolveFioraSkillEffectParam(
            ctx,
            eSkillSlot::E,
            eSkillEffectParamId::MaxStacks,
            2.f);
        const f32_t eDamageBonus = GameplayDefinitionQuery::ResolveSkillFlatDamage(
            *ctx.pWorld, ctx.casterEntity, *ctx.pTickCtx, eChampion::FIORA,
            static_cast<u8_t>(eSkillSlot::E), ctx.skillRank, 30.f);

        FioraSimComponent& state = EnsureFioraState(*ctx.pWorld, ctx.casterEntity);
        state.bBladeworkActive = true;
        state.bladeworkTimerSec = eWindowSec;
        state.bladeworkHitsRemaining = static_cast<u8_t>(eMaxStacks);
        state.bladeworkPendingHitOrdinal = 0u;
        state.bladeworkDamageBonus = eDamageBonus;

        EmitFioraEffect(
            *ctx.pWorld,
            *ctx.pTickCtx,
            ctx.casterEntity,
            ctx.casterEntity,
            GameplayHookVariant::E_CastFrame,
            eSkillSlot::E,
            1u,
            0u,
            ResolveEntityPosition(*ctx.pWorld, ctx.casterEntity),
            {},
            static_cast<u16_t>(
                std::clamp(eWindowSec, 0.f, 65.535f) * 1000.f));
    }

    void OnR(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand || !ctx.pTickCtx)
            return;

        CWorld& world = *ctx.pWorld;
        const EntityID target = ctx.pCommand->targetEntity;
        if (!FioraGameSim::CanCastGrandChallenge(
                world,
                *ctx.pTickCtx,
                ctx.casterEntity,
                target))
        {
            return;
        }

        FioraSimComponent& state = EnsureFioraState(world, ctx.casterEntity);
        ClearPassiveVital(world, *ctx.pTickCtx, ctx.casterEntity, state, false);
        state.bGrandChallengeActive = true;
        state.grandChallengeTimerSec = ResolveFioraSkillEffectParam(
            world,
            *ctx.pTickCtx,
            ctx.casterEntity,
            eSkillSlot::R,
            eSkillEffectParamId::ChallengeDurationSec);
        state.grandChallengeTarget = target;
        state.grandChallengeActiveMask = 0x0fu;
        state.grandChallengeRank = ctx.skillRank;
        state.bGrandChallengeHealActive = false;
        state.grandChallengeHealTimerSec = 0.f;
        state.grandChallengeHealTickTimerSec = 0.f;

        const Vec3 targetPosition =
            world.GetComponent<TransformComponent>(target).GetPosition();
        EmitFioraEffect(
            world,
            *ctx.pTickCtx,
            ctx.casterEntity,
            target,
            GameplayHookVariant::R_Recovery,
            eSkillSlot::R,
            1u,
            0u,
            targetPosition,
            {},
            8000u);
        for (u8_t direction = 0u; direction < 4u; ++direction)
        {
            EmitFioraEffect(
                world,
                *ctx.pTickCtx,
                ctx.casterEntity,
                target,
                GameplayHookVariant::R_CastFrame,
                eSkillSlot::R,
                2u,
                static_cast<u8_t>(direction + 1u),
                targetPosition,
                VitalDirection(direction),
                8000u);
        }
    }
}

namespace FioraGameSim
{
    bool_t CanCastGrandChallenge(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        EntityID target)
    {
        if (caster == NULL_ENTITY ||
            target == NULL_ENTITY ||
            !world.IsAlive(caster) ||
            !world.IsAlive(target) ||
            !world.HasComponent<ChampionComponent>(caster) ||
            !world.HasComponent<ChampionComponent>(target) ||
            !world.HasComponent<HealthComponent>(target) ||
            !world.HasComponent<TransformComponent>(caster) ||
            !world.HasComponent<TransformComponent>(target) ||
            !GameplayStateQuery::CanBeTargetedBy(world, caster, target) ||
            !GameplayStateQuery::CanReceiveDamage(world, caster, target))
        {
            return false;
        }

        const HealthComponent& targetHealth = world.GetComponent<HealthComponent>(target);
        if (targetHealth.bIsDead || targetHealth.fCurrent <= 0.f)
            return false;

        const ChampionComponent& casterChampion =
            world.GetComponent<ChampionComponent>(caster);
        const ChampionComponent& targetChampion =
            world.GetComponent<ChampionComponent>(target);
        if (casterChampion.team == targetChampion.team)
            return false;

        f32_t range = GameplayDefinitionQuery::ResolveSkillRange(
            world,
            caster,
            tc,
            eChampion::FIORA,
            static_cast<u8_t>(eSkillSlot::R));
        if (range <= 0.f)
            range = 5.f;
        range += GameplayStateQuery::ResolveGameplayRadius(world, caster);
        range += GameplayStateQuery::ResolveGameplayRadius(world, target);

        const Vec3 casterPosition =
            world.GetComponent<TransformComponent>(caster).GetPosition();
        const Vec3 targetPosition =
            world.GetComponent<TransformComponent>(target).GetPosition();
        if (WintersMath::DistanceSqXZ(casterPosition, targetPosition) > range * range)
            return false;

        return !tc.pWalkable ||
            tc.pWalkable->SegmentWalkableXZ(casterPosition, targetPosition, 0.f);
    }

    void CancelRuntime(CWorld& world, EntityID caster)
    {
        if (world.HasComponent<FioraDashComponent>(caster))
            world.RemoveComponent<FioraDashComponent>(caster);
        if (world.HasComponent<FioraSimComponent>(caster))
            world.RemoveComponent<FioraSimComponent>(caster);
    }

    f32_t ConsumeBasicAttackDamage(CWorld& world, EntityID caster, f32_t baseDamage)
    {
        if (!world.HasComponent<FioraSimComponent>(caster))
            return baseDamage;

        FioraSimComponent& state = world.GetComponent<FioraSimComponent>(caster);
        if (!state.bBladeworkActive || state.bladeworkHitsRemaining == 0)
            return baseDamage;

        const u8_t hitOrdinal = static_cast<u8_t>(3u - state.bladeworkHitsRemaining);
        state.bladeworkPendingHitOrdinal = hitOrdinal;
        --state.bladeworkHitsRemaining;
        if (state.bladeworkHitsRemaining == 0)
            state.bBladeworkActive = false;

        return baseDamage + state.bladeworkDamageBonus;
    }

    bool_t PrepareDamageRequest(CWorld& world, DamageRequest& request)
    {
        if (request.source == NULL_ENTITY ||
            request.eSourceKind != eDamageSourceKind::BasicAttack ||
            !world.HasComponent<FioraSimComponent>(request.source) ||
            !world.HasComponent<StatComponent>(request.source))
        {
            return false;
        }

        const FioraSimComponent& state =
            world.GetComponent<FioraSimComponent>(request.source);
        if (state.bladeworkPendingHitOrdinal == 0u)
            return false;

        request.flags |= DamageFlag_ShowCriticalIndicator;
        if (state.bladeworkPendingHitOrdinal != 2u)
            return false;

        const f32_t critDamage = std::max(
            1.f,
            world.GetComponent<StatComponent>(request.source).critDamage);
        request.flatAmount = BuildRawDamage(world, request) * critDamage;
        request.amount = 0.f;
        request.adRatioOverride = 0.f;
        request.bonusAdRatioOverride = 0.f;
        request.apRatioOverride = 0.f;
        request.targetMaxHpRatioOverride = 0.f;
        request.targetMissingHpRatioOverride = 0.f;
        request.flags &= ~DamageFlag_CanCrit;
        return true;
    }

    void OnDamageResolved(
        CWorld& world,
        const TickContext& tc,
        const DamageRequest& request,
        const DamageResult& result)
    {
        if (request.source == NULL_ENTITY ||
            !world.HasComponent<ChampionComponent>(request.source) ||
            world.GetComponent<ChampionComponent>(request.source).id != eChampion::FIORA ||
            !world.HasComponent<FioraSimComponent>(request.source))
        {
            return;
        }

        FioraSimComponent& state =
            world.GetComponent<FioraSimComponent>(request.source);
        const bool_t bBasicAttack =
            request.eSourceKind == eDamageSourceKind::BasicAttack;
        if (bBasicAttack && state.bladeworkPendingHitOrdinal > 0u)
        {
            const u8_t hitOrdinal = state.bladeworkPendingHitOrdinal;
            state.bladeworkPendingHitOrdinal = 0u;
            if (result.finalAmount > 0.f)
            {
                const Vec3 position = ResolveEntityPosition(world, request.target);
                const Vec3 direction = WintersMath::DirectionXZ(
                    ResolveEntityPosition(world, request.source),
                    position,
                    Vec3{});
                EmitFioraEffect(
                    world,
                    tc,
                    request.source,
                    request.target,
                    GameplayHookVariant::E_Recovery,
                    eSkillSlot::E,
                    hitOrdinal >= 2u ? 3u : 2u,
                    hitOrdinal,
                    position,
                    direction,
                    450u);
            }
        }

        const bool_t bRiposteHit =
            request.iSourceSlot == static_cast<u8_t>(eSkillSlot::W) &&
            state.bRiposteReleasePending &&
            state.riposteReleaseTarget == request.target;
        if (bRiposteHit)
        {
            if (result.finalAmount > 0.f)
            {
                if (state.bRiposteCaughtHardCC)
                {
                    GameplayStatus::ApplyStun(
                        world,
                        tc,
                        request.target,
                        request.source,
                        eChampion::FIORA,
                        eSkillSlot::W,
                        state.riposteStunDurationSec);
                }
                else
                {
                    GameplayStatus::ApplySlow(
                        world,
                        tc,
                        request.target,
                        request.source,
                        eChampion::FIORA,
                        eSkillSlot::W,
                        state.riposteSlowDurationSec,
                        state.riposteSlowMoveSpeedMul);
                }
            }

            state.bRiposteReleasePending = false;
            state.bRiposteCaughtHardCC = false;
            state.riposteReleaseTarget = NULL_ENTITY;
        }

        if (result.finalAmount <= 0.f ||
            (request.flags & DamageFlag_OnHit) == 0u)
        {
            return;
        }

        const u8_t sourceSlot = bBasicAttack
            ? static_cast<u8_t>(eSkillSlot::BasicAttack)
            : request.iSourceSlot;
        if (sourceSlot != static_cast<u8_t>(eSkillSlot::BasicAttack) &&
            sourceSlot != static_cast<u8_t>(eSkillSlot::Q) &&
            sourceSlot != static_cast<u8_t>(eSkillSlot::W))
        {
            return;
        }

        (void)TryConsumeVital(world, tc, request, state);
    }

    bool_t TryParryCrowdControl(
        CWorld& world,
        EntityID target,
        const StatusEffectApplyDesc& desc,
        const TickContext* pTickContext)
    {
        constexpr u32_t kHardCrowdControlFlags =
            kGameplayStateStunnedFlag | kGameplayStateAirborneFlag;
        if (target == NULL_ENTITY ||
            (desc.stateFlags & kHardCrowdControlFlags) == 0u ||
            !world.HasComponent<ChampionComponent>(target) ||
            world.GetComponent<ChampionComponent>(target).id != eChampion::FIORA ||
            !world.HasComponent<FioraSimComponent>(target))
        {
            return false;
        }

        FioraSimComponent& state = world.GetComponent<FioraSimComponent>(target);
        if (!state.bRiposteActive)
            return false;

        if (desc.sourceEntity != NULL_ENTITY &&
            world.HasComponent<ChampionComponent>(desc.sourceEntity) &&
            world.GetComponent<ChampionComponent>(desc.sourceEntity).team ==
                world.GetComponent<ChampionComponent>(target).team)
        {
            return false;
        }

        if (!state.bRiposteCaughtHardCC)
        {
            state.bRiposteCaughtHardCC = true;
            if (pTickContext)
            {
                const Vec3 origin = ResolveEntityPosition(world, target);
                const Vec3 endpoint{
                    origin.x + state.riposteDirection.x * state.riposteRange,
                    origin.y,
                    origin.z + state.riposteDirection.z * state.riposteRange
                };
                EmitFioraEffect(
                    world,
                    *pTickContext,
                    target,
                    desc.sourceEntity,
                    GameplayHookVariant::W_CastFrame,
                    eSkillSlot::W,
                    2u,
                    0u,
                    endpoint,
                    state.riposteDirection,
                    static_cast<u16_t>(
                        std::clamp(state.riposteTimerSec, 0.f, 65.535f) * 1000.f),
                    state.riposteRange,
                    state.riposteRadius);
            }
        }

        return true;
    }

    void Tick(CWorld& world, const TickContext& tc)
    {
        const auto champions =
            DeterministicEntityIterator<ChampionComponent>::CollectSorted(world);
        for (EntityID championEntity : champions)
        {
            if (!world.IsAlive(championEntity) ||
                world.GetComponent<ChampionComponent>(championEntity).id != eChampion::FIORA)
            {
                continue;
            }
            if (world.HasComponent<HealthComponent>(championEntity))
            {
                const HealthComponent& health =
                    world.GetComponent<HealthComponent>(championEntity);
                if (health.bIsDead || health.fCurrent <= 0.f)
                    continue;
            }
            EnsureFioraState(world, championEntity);
        }

        std::vector<EntityID> finishedDashes;
        world.ForEach<FioraDashComponent, TransformComponent>(
            std::function<void(EntityID, FioraDashComponent&, TransformComponent&)>(
                [&](EntityID entity, FioraDashComponent& dash, TransformComponent& transform)
                {
                    if (!GameplayStateQuery::CanMove(world, entity) ||
                        world.HasComponent<ForcedMotionComponent>(entity))
                    {
                        finishedDashes.push_back(entity);
                        return;
                    }

                    ClearMove(world, entity);
                    dash.elapsedSec += tc.fDt;
                    f32_t t = dash.durationSec > 0.01f
                        ? dash.elapsedSec / dash.durationSec
                        : 1.f;
                    if (t >= 1.f)
                    {
                        t = 1.f;
                        finishedDashes.push_back(entity);
                    }

                    const Vec3 position{
                        dash.start.x + (dash.end.x - dash.start.x) * t,
                        dash.start.y + (dash.end.y - dash.start.y) * t,
                        dash.start.z + (dash.end.z - dash.start.z) * t
                    };
                    Vec3 guardedPosition = position;
                    bool_t bDashBlocked = false;
                    if (tc.pWalkable)
                    {
                        const Vec3 currentPosition = transform.GetLocalPosition();
                        if (!tc.pWalkable->TryClampMoveSegmentXZ(
                                currentPosition,
                                position,
                                0.5f,
                                guardedPosition))
                        {
                            guardedPosition = currentPosition;
                            bDashBlocked = true;
                        }
                        else if (WintersMath::DistanceSqXZ(
                                     guardedPosition,
                                     position) > 0.0001f)
                        {
                            bDashBlocked = true;
                        }
                    }

                    transform.SetPosition(guardedPosition);
                    if (bDashBlocked && t < 1.f)
                        finishedDashes.push_back(entity);
                }));

        for (EntityID entity : finishedDashes)
        {
            if (world.HasComponent<ForcedMotionComponent>(entity))
            {
                world.RemoveComponent<FioraDashComponent>(entity);
                continue;
            }
            if (world.HasComponent<FioraDashComponent>(entity))
            {
                SnapDashArrivalToWalkable(
                    world,
                    tc,
                    entity,
                    world.GetComponent<FioraDashComponent>(entity).start);
                world.RemoveComponent<FioraDashComponent>(entity);
            }
        }

        const auto fioras =
            DeterministicEntityIterator<FioraSimComponent>::CollectSorted(world);
        for (EntityID caster : fioras)
        {
            if (!world.IsAlive(caster) ||
                !world.HasComponent<FioraSimComponent>(caster))
            {
                continue;
            }

            FioraSimComponent& state = world.GetComponent<FioraSimComponent>(caster);
            if (world.HasComponent<HealthComponent>(caster))
            {
                const HealthComponent& casterHealth =
                    world.GetComponent<HealthComponent>(caster);
                if (casterHealth.bIsDead || casterHealth.fCurrent <= 0.f)
                {
                    ClearPassiveVital(world, tc, caster, state, false);
                    if (state.bGrandChallengeActive ||
                        state.bGrandChallengeHealActive)
                    {
                        const EntityID target = state.grandChallengeTarget;
                        const Vec3 clearPosition = state.bGrandChallengeHealActive
                            ? state.grandChallengeHealCenter
                            : ResolveEntityPosition(world, target);
                        EmitFioraEffect(
                            world,
                            tc,
                            caster,
                            target,
                            GameplayHookVariant::R_Recovery,
                            eSkillSlot::R,
                            4u,
                            0u,
                            clearPosition,
                            {},
                            0u);
                    }
                    state = FioraSimComponent{};
                    if (world.HasComponent<FioraDashComponent>(caster))
                        world.RemoveComponent<FioraDashComponent>(caster);
                    continue;
                }
            }

            if (state.bBladeworkActive)
            {
                state.bladeworkTimerSec = std::max(
                    0.f,
                    state.bladeworkTimerSec - tc.fDt);
                if (state.bladeworkTimerSec <= 0.f)
                {
                    state.bBladeworkActive = false;
                    state.bladeworkHitsRemaining = 0u;
                }
            }

            if (state.bRiposteActive)
            {
                state.riposteTimerSec = std::max(
                    0.f,
                    state.riposteTimerSec - tc.fDt);
                if (state.riposteTimerSec <= 0.f)
                    ReleaseRiposte(world, tc, caster, state);
            }

            if (state.bGrandChallengeActive)
            {
                const EntityID target = state.grandChallengeTarget;
                const bool_t bTargetValid =
                    target != NULL_ENTITY &&
                    world.IsAlive(target) &&
                    world.HasComponent<HealthComponent>(target) &&
                    !world.GetComponent<HealthComponent>(target).bIsDead &&
                    world.GetComponent<HealthComponent>(target).fCurrent > 0.f;
                state.grandChallengeTimerSec = std::max(
                    0.f,
                    state.grandChallengeTimerSec - tc.fDt);
                if (!bTargetValid || state.grandChallengeTimerSec <= 0.f)
                {
                    EmitFioraEffect(
                        world,
                        tc,
                        caster,
                        target,
                        GameplayHookVariant::R_Recovery,
                        eSkillSlot::R,
                        4u,
                        0u,
                        ResolveEntityPosition(world, target),
                        {},
                        0u);
                    state.bGrandChallengeActive = false;
                    state.grandChallengeActiveMask = 0u;
                    state.grandChallengeTarget = NULL_ENTITY;
                }
            }

            if (state.bGrandChallengeHealActive)
            {
                state.grandChallengeHealTimerSec = std::max(
                    0.f,
                    state.grandChallengeHealTimerSec - tc.fDt);
                state.grandChallengeHealTickTimerSec -= tc.fDt;
                while (state.grandChallengeHealTickTimerSec <= 0.f &&
                    state.grandChallengeHealTimerSec > 0.f)
                {
                    HealGrandChallengeAllies(world, tc, caster, state);
                    state.grandChallengeHealTickTimerSec +=
                        ResolveFioraSkillEffectParam(
                            world,
                            tc,
                            caster,
                            eSkillSlot::R,
                            eSkillEffectParamId::HealIntervalSec);
                }
                if (state.grandChallengeHealTimerSec <= 0.f)
                    state.bGrandChallengeHealActive = false;
            }

            if (state.bGrandChallengeActive)
                continue;

            if (state.bPassiveVitalActive)
            {
                const EntityID target = state.passiveVitalTarget;
                const bool_t bTargetValid =
                    target != NULL_ENTITY &&
                    world.IsAlive(target) &&
                    world.HasComponent<TransformComponent>(caster) &&
                    world.HasComponent<TransformComponent>(target) &&
                    world.HasComponent<HealthComponent>(target) &&
                    !world.GetComponent<HealthComponent>(target).bIsDead &&
                    world.GetComponent<HealthComponent>(target).fCurrent > 0.f &&
                    WintersMath::DistanceSqXZ(
                        ResolveEntityPosition(world, caster),
                        ResolveEntityPosition(world, target)) <= [&]()
                        {
                            const f32_t acquireRange = ResolveFioraSkillEffectParam(
                                world,
                                tc,
                                caster,
                                eSkillSlot::BasicAttack,
                                eSkillEffectParamId::AcquireRange);
                            return acquireRange * acquireRange;
                        }();
                state.passiveVitalTimerSec = std::max(
                    0.f,
                    state.passiveVitalTimerSec - tc.fDt);
                if (!bTargetValid || state.passiveVitalTimerSec <= 0.f)
                    ClearPassiveVital(world, tc, caster, state, true);
            }
            else
            {
                state.passiveRespawnTimerSec = std::max(
                    0.f,
                    state.passiveRespawnTimerSec - tc.fDt);
                if (state.passiveRespawnTimerSec <= 0.f)
                    SpawnPassiveVital(world, tc, caster, state);
            }
        }
    }

    void RegisterHooks()
    {
        static bool_t s_bRegistered = false;
        if (s_bRegistered)
            return;

        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::FIORA, GameplayHookVariant::Q_CastFrame),
            &OnQ);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::FIORA, GameplayHookVariant::W_CastFrame),
            &OnW);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::FIORA, GameplayHookVariant::E_CastFrame),
            &OnE);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::FIORA, GameplayHookVariant::R_CastFrame),
            &OnR);

        s_bRegistered = true;
    }
}
