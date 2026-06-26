#include "Shared/GameSim/Champions/Jax/JaxGameSim.h"

#include "Shared/GameSim/Components/CombatActionComponent.h"
#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/JaxSimComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/PoseActionStateHelpers.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Definitions/GameplayDefinitionQuery.h"
#include "Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/ReplicatedEventQueue/ReplicatedEventQueue.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectRequests.h"

#include "Shared/GameSim/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "Shared/GameSim/Core/World/World.h"
#include "Shared/GameSim/Systems/Move/DashArrival.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <vector>

namespace
{
    f32_t ResolveJaxSkillEffectParam(
        const GameplayHookContext& ctx,
        eSkillSlot slot,
        eSkillEffectParamId param,
        f32_t fallbackValue = 0.f)
    {
        if (!ctx.pWorld || !ctx.pTickCtx)
        {
            return fallbackValue;
        }

        return GameplayDefinitionQuery::ResolveSkillEffectParam(
            *ctx.pWorld,
            ctx.casterEntity,
            *ctx.pTickCtx,
            eChampion::JAX,
            static_cast<u8_t>(slot),
            param,
            fallbackValue);
    }

    f32_t ResolveJaxSkillEffectParam(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        eSkillSlot slot,
        eSkillEffectParamId param,
        f32_t fallbackValue = 0.f)
    {
        return GameplayDefinitionQuery::ResolveSkillEffectParam(
            world,
            caster,
            tc,
            eChampion::JAX,
            static_cast<u8_t>(slot),
            param,
            fallbackValue);
    }

    struct JaxDashComponent
    {
        Vec3 start{};
        Vec3 end{};
        f32_t elapsedSec = 0.f;
        f32_t durationSec = 0.f;
    };

    JaxSimComponent& EnsureJaxState(CWorld& world, EntityID caster)
    {
        if (!world.HasComponent<JaxSimComponent>(caster))
            world.AddComponent<JaxSimComponent>(caster, JaxSimComponent{});

        return world.GetComponent<JaxSimComponent>(caster);
    }

    void ClearMove(CWorld& world, EntityID entity)
    {
        if (world.HasComponent<MoveTargetComponent>(entity))
            world.GetComponent<MoveTargetComponent>(entity).bHasTarget = false;
    }

    void RotateToward(CWorld& world, EntityID entity, const Vec3& direction)
    {
        if (!world.HasComponent<TransformComponent>(entity))
            return;

        const Vec3 dir = WintersMath::NormalizeXZ(direction);
        auto& transform = world.GetComponent<TransformComponent>(entity);
        const Vec3 rot = transform.GetRotation();
        transform.SetRotation(Vec3{
            rot.x,
            ResolveChampionVisualYawNear(eChampion::JAX, dir, rot.y),
            rot.z });
    }

    void EnqueuePhysicalDamage(
        CWorld& world,
        EntityID source,
        EntityID target,
        eTeam sourceTeam,
        f32_t amount,
        u8_t slot,
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
        request.skillId = static_cast<u16_t>((static_cast<u32_t>(eChampion::JAX) << 8) | slot);
        request.rank = rank;
        request.flags = DamageFlag_OnHit;
        EnqueueDamageRequest(world, request);
    }

    void EnqueueCircleDamage(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        eTeam sourceTeam,
        const Vec3& origin,
        f32_t radius,
        f32_t amount,
        f32_t stunDurationSec,
        u8_t slot)
    {
        const f32_t radiusSq = radius * radius;
        std::vector<EntityID> targets;
        targets.reserve(8);

        world.ForEach<ChampionComponent, TransformComponent>(
            std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                [&](EntityID entity, ChampionComponent& champion, TransformComponent& transform)
                {
                    if (entity == source || champion.team == sourceTeam)
                        return;

                    const Vec3 pos = transform.GetPosition();
                    const f32_t dx = pos.x - origin.x;
                    const f32_t dz = pos.z - origin.z;
                    if (dx * dx + dz * dz <= radiusSq)
                        targets.push_back(entity);
                }));

        for (EntityID target : targets)
        {
            EnqueuePhysicalDamage(world, source, target, sourceTeam, amount, slot, 1);
            if (slot == static_cast<u8_t>(eSkillSlot::E))
            {
                GameplayStatus::ApplyStun(
                    world,
                    tc,
                    target,
                    source,
                    eChampion::JAX,
                    eSkillSlot::E,
                    stunDurationSec,
                    eStatusEffectId::JaxCounterStrike);
            }
        }
    }

    u16_t BuildJaxEStage2Flags(u8_t rank)
    {
        return static_cast<u16_t>(
            (static_cast<u16_t>(2u) << 12) |
            (static_cast<u16_t>(rank & 0x0fu) << 8) |
            static_cast<u16_t>(eSkillSlot::E));
    }

    void StartJaxEReleaseAction(CWorld& world, EntityID caster, const TickContext& tc)
    {
        StartActionState(world, caster, eActionStateId::SkillE, tc.tickIndex, 2u);
    }

    void ClearJaxEStageWindow(CWorld& world, EntityID caster)
    {
        if (!world.HasComponent<SkillStateComponent>(caster))
            return;

        auto& slot = world.GetComponent<SkillStateComponent>(caster)
            .slots[static_cast<u8_t>(eSkillSlot::E)];
        slot.currentStage = 0;
        slot.stageWindow = 0.f;
    }

    void EnqueueJaxEReleaseVisual(CWorld& world, EntityID caster, const TickContext& tc, u8_t rank)
    {
        ReplicatedEventComponent effectEvent{};
        effectEvent.kind = eReplicatedEventKind::EffectTrigger;
        effectEvent.sourceEntity = caster;
        effectEvent.effectId = MakeGameplayHookId(eChampion::JAX, GameplayHookVariant::E_CastFrame);
        effectEvent.slot = static_cast<u8_t>(eSkillSlot::E);
        effectEvent.rank = rank;
        effectEvent.flags = BuildJaxEStage2Flags(rank);
        effectEvent.startTick = tc.tickIndex;
        effectEvent.durationMs = 700;

        if (world.HasComponent<TransformComponent>(caster))
            effectEvent.position = world.GetComponent<TransformComponent>(caster).GetPosition();

        EnqueueReplicatedEvent(world, effectEvent);
    }

    void ReleaseJaxCounterStrike(
        CWorld& world,
        EntityID caster,
        const TickContext& tc,
        JaxSimComponent& state,
        bool_t bEmitVisual)
    {
        if (!state.bCounterStrikeActive)
            return;

        state.bCounterStrikeActive = false;
        state.counterTimerSec = 0.f;
        ClearJaxEStageWindow(world, caster);

        if (bEmitVisual)
        {
            StartJaxEReleaseAction(world, caster, tc);
            EnqueueJaxEReleaseVisual(world, caster, tc, state.counterRank);
        }

        if (world.HasComponent<TransformComponent>(caster) &&
            world.HasComponent<ChampionComponent>(caster))
        {
            const auto& champion = world.GetComponent<ChampionComponent>(caster);
            const Vec3 origin = world.GetComponent<TransformComponent>(caster).GetPosition();
            const f32_t stunDurationSec = ResolveJaxSkillEffectParam(
                world,
                tc,
                caster,
                eSkillSlot::E,
                eSkillEffectParamId::StunDurationSec);
            EnqueueCircleDamage(
                world,
                tc,
                caster,
                champion.team,
                origin,
                state.counterRadius,
                state.counterDamage,
                stunDurationSec,
                static_cast<u8_t>(eSkillSlot::E));
        }
    }

    void StartTargetDash(
        CWorld& world,
        EntityID caster,
        EntityID target,
        f32_t gap,
        f32_t durationSec)
    {
        if (!world.HasComponent<TransformComponent>(caster) ||
            target == NULL_ENTITY ||
            !world.HasComponent<TransformComponent>(target))
        {
            return;
        }

        const Vec3 start = world.GetComponent<TransformComponent>(caster).GetPosition();
        const Vec3 targetPos = world.GetComponent<TransformComponent>(target).GetPosition();
        const Vec3 dir = WintersMath::NormalizeXZ(Vec3{
            targetPos.x - start.x,
            0.f,
            targetPos.z - start.z
        });
        const f32_t dx = targetPos.x - start.x;
        const f32_t dz = targetPos.z - start.z;
        const f32_t dist = std::sqrt(dx * dx + dz * dz);
        const f32_t moveDist = std::max(0.f, dist - gap);

        JaxDashComponent dash{};
        dash.start = start;
        dash.end = Vec3{
            start.x + dir.x * moveDist,
            start.y,
            start.z + dir.z * moveDist
        };
        dash.durationSec = durationSec;

        if (world.HasComponent<JaxDashComponent>(caster))
            world.GetComponent<JaxDashComponent>(caster) = dash;
        else
            world.AddComponent<JaxDashComponent>(caster, dash);

        RotateToward(world, caster, dir);
        ClearMove(world, caster);
    }

    void OnQ(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand)
            return;

        const f32_t qGap = ResolveJaxSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::Gap);
        const f32_t qDashDurationSec = ResolveJaxSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::DashDurationSec);
        const f32_t qDamage = ResolveJaxSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::BaseDamage);

        StartTargetDash(
            *ctx.pWorld,
            ctx.casterEntity,
            ctx.pCommand->targetEntity,
            qGap,
            qDashDurationSec);
        EnqueuePhysicalDamage(
            *ctx.pWorld,
            ctx.casterEntity,
            ctx.pCommand->targetEntity,
            ctx.casterTeam,
            qDamage,
            static_cast<u8_t>(eSkillSlot::Q),
            ctx.skillRank);

        std::cout << "[JaxSim] Q leap caster=" << ctx.casterEntity
            << " target=" << ctx.pCommand->targetEntity << "\n";
    }

    void OnW(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld)
            return;

        JaxSimComponent& state = EnsureJaxState(*ctx.pWorld, ctx.casterEntity);
        state.bEmpowerActive = true;
        state.empowerTimerSec = state.empowerWindowSec;
        std::cout << "[JaxSim] W empower caster=" << ctx.casterEntity << "\n";
    }

    void OnE(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld)
            return;

        JaxSimComponent& state = EnsureJaxState(*ctx.pWorld, ctx.casterEntity);
        if (ctx.pCommand && ctx.pCommand->itemId == 2u)
        {
            TickContext fallbackTick{};
            const TickContext& tickCtx = ctx.pTickCtx ? *ctx.pTickCtx : fallbackTick;
            ReleaseJaxCounterStrike(*ctx.pWorld, ctx.casterEntity, tickCtx, state, false);
            std::cout << "[JaxSim] E counter release caster=" << ctx.casterEntity << "\n";
            return;
        }

        state.bCounterStrikeActive = true;
        state.counterTimerSec = state.counterDurationSec;
        state.counterRank = ctx.skillRank;
        ClearMove(*ctx.pWorld, ctx.casterEntity);
        std::cout << "[JaxSim] E counter start caster=" << ctx.casterEntity << "\n";
    }

    void OnR(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld)
            return;

        JaxSimComponent& state = EnsureJaxState(*ctx.pWorld, ctx.casterEntity);
        state.bUltActive = true;
        state.ultTimerSec = state.ultDurationSec;
        state.ultAttackCounter = 0;
        std::cout << "[JaxSim] R active caster=" << ctx.casterEntity << "\n";
    }
}

namespace JaxGameSim
{
    bool_t TryConsumeEmpowerForBasicAttack(CWorld& world, EntityID caster)
    {
        if (!world.HasComponent<JaxSimComponent>(caster))
            return false;

        JaxSimComponent& state = world.GetComponent<JaxSimComponent>(caster);
        if (!state.bEmpowerActive)
            return false;

        state.bEmpowerActive = false;
        state.empowerTimerSec = 0.f;
        return true;
    }

    f32_t ConsumeBasicAttackDamage(
        CWorld& world,
        EntityID caster,
        EntityID /*target*/,
        eTeam /*casterTeam*/,
        u16_t actionFlags,
        f32_t baseDamage)
    {
        if (!world.HasComponent<JaxSimComponent>(caster))
            return baseDamage;

        JaxSimComponent& state = world.GetComponent<JaxSimComponent>(caster);
        f32_t damage = baseDamage;

        if ((actionFlags & CombatActionFlags::JaxEmpower) != 0u)
            damage += state.empowerBonusDamage;

        if (state.bUltActive)
        {
            ++state.ultAttackCounter;
            if (state.ultAttackCounter >= 3)
            {
                state.ultAttackCounter = 0;
                damage += state.ultThirdHitDamage;
            }
        }

        return damage;
    }

    void Tick(CWorld& world, const TickContext& tc)
    {
        std::vector<EntityID> finishedDashes;
        world.ForEach<JaxDashComponent, TransformComponent>(
            std::function<void(EntityID, JaxDashComponent&, TransformComponent&)>(
                [&](EntityID entity, JaxDashComponent& dash, TransformComponent& transform)
                {
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
                        const Vec3 currentPos = transform.GetLocalPosition();
                        if (!tc.pWalkable->TryClampMoveSegmentXZ(currentPos, position, 0.5f, guardedPosition))
                        {
                            guardedPosition = currentPos;
                            bDashBlocked = true;
                        }
                        else if (WintersMath::DistanceSqXZ(guardedPosition, position) > 0.0001f)
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
            if (world.HasComponent<JaxDashComponent>(entity))
                SnapDashArrivalToWalkable(world, tc, entity,
                    world.GetComponent<JaxDashComponent>(entity).start);
            world.RemoveComponent<JaxDashComponent>(entity);
        }

        world.ForEach<JaxSimComponent>(
            std::function<void(EntityID, JaxSimComponent&)>(
                [&](EntityID entity, JaxSimComponent& state)
                {
                    if (state.bEmpowerActive)
                    {
                        state.empowerTimerSec = std::max(0.f, state.empowerTimerSec - tc.fDt);
                        if (state.empowerTimerSec <= 0.f)
                            state.bEmpowerActive = false;
                    }

                    if (state.bCounterStrikeActive)
                    {
                        state.counterTimerSec = std::max(0.f, state.counterTimerSec - tc.fDt);
                        if (state.counterTimerSec <= 0.f)
                            ReleaseJaxCounterStrike(world, entity, tc, state, true);
                    }

                    if (state.bUltActive)
                    {
                        state.ultTimerSec = std::max(0.f, state.ultTimerSec - tc.fDt);
                        if (state.ultTimerSec <= 0.f)
                        {
                            state.bUltActive = false;
                            state.ultAttackCounter = 0;
                        }
                    }
                }));
    }

    void RegisterHooks()
    {
        static bool_t s_bRegistered = false;
        if (s_bRegistered)
            return;

        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::JAX, GameplayHookVariant::Q_CastFrame), &OnQ);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::JAX, GameplayHookVariant::W_CastFrame), &OnW);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::JAX, GameplayHookVariant::E_CastFrame), &OnE);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::JAX, GameplayHookVariant::R_CastFrame), &OnR);

        s_bRegistered = true;
        std::cout << "[JaxSim] hooks registered\n";
    }
}
