#include "Shared/GameSim/Champions/Irelia/IreliaGameSim.h"

#include "Shared/GameSim/Components/ActionStateComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/IreliaSimComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Components/SkillChargeStateComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Definitions/GameplayDefinitionQuery.h"
#include "Shared/GameSim/Definitions/SkillGameplayDef.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/ReplicatedEventQueue/ReplicatedEventQueue.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectRequests.h"

#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Core/Ecs/TransformComponent.h"
#include "Shared/GameSim/Systems/Move/DashArrival.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>

namespace
{
    constexpr u8_t kIreliaREffectStageHit = 2u;
    constexpr u8_t kIreliaREffectStageWall = 3u;
    constexpr u8_t kIreliaREffectStageWallMark = 4u;
    constexpr f32_t kIreliaQKillResetCooldownSec = 0.1f;
    constexpr u16_t kIreliaQSkillId =
        static_cast<u16_t>((static_cast<u32_t>(eChampion::IRELIA) << 8) | 1u);

    bool_t TryResolveIreliaQTargetSideLanding(
        const TickContext& tc,
        const Vec3& targetPos,
        const Vec3& rawLanding,
        Vec3& outLanding)
    {
        outLanding = rawLanding;
        if (!tc.pWalkable)
            return true;
        if (tc.pWalkable->IsWalkableXZ(rawLanding))
            return true;

        return tc.pWalkable->TryResolveMoveTarget(
                targetPos, rawLanding, outLanding) &&
            tc.pWalkable->IsWalkableXZ(outLanding);
    }

    f32_t ResolveIreliaSkillEffectParam(
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
            eChampion::IRELIA,
            static_cast<u8_t>(slot),
            param,
            fallbackValue);
    }

    f32_t ResolveIreliaSkillEffectParam(
        const GameplayHookContext& ctx,
        eSkillSlot slot,
        eSkillEffectParamId param,
        f32_t fallbackValue = 0.f)
    {
        if (!ctx.pWorld || !ctx.pTickCtx)
        {
            return fallbackValue;
        }

        return ResolveIreliaSkillEffectParam(
            *ctx.pWorld,
            *ctx.pTickCtx,
            ctx.casterEntity,
            slot,
            param,
            fallbackValue);
    }

    void ClearMove(CWorld& world, EntityID entity)
    {
        if (world.HasComponent<MoveTargetComponent>(entity))
            world.GetComponent<MoveTargetComponent>(entity).bHasTarget = false;
    }

    void EnqueuePhysicalDamage(
        CWorld& world,
        EntityID source,
        EntityID target,
        eTeam team,
        f32_t amount,
        u16_t skillId,
        u8_t rank,
        f32_t skillDamageScale = 1.f)
    {
        if (target == NULL_ENTITY || !world.IsAlive(target))
            return;

        DamageRequest request{};
        request.source = source;
        request.target = target;
        request.sourceTeam = team;
        request.type = eDamageType::Physical;
        request.flatAmount = amount;
        request.skillId = skillId;
        request.rank = rank;
        request.iSourceSlot = static_cast<u8_t>(skillId & 0x00ffu);
        request.eSourceKind = eDamageSourceKind::Skill;
        request.skillDamageScale = skillDamageScale;
        EnqueueDamageRequest(world, request);
    }

    IreliaSimComponent& GetIreliaState(CWorld& world, EntityID caster)
    {
        if (!world.HasComponent<IreliaSimComponent>(caster))
            world.AddComponent<IreliaSimComponent>(caster, IreliaSimComponent{});

        return world.GetComponent<IreliaSimComponent>(caster);
    }

    void LimitIreliaQCooldown(CWorld& world, EntityID caster, f32_t cooldownSec)
    {
        if (!world.HasComponent<SkillStateComponent>(caster))
            return;

        auto& q = world.GetComponent<SkillStateComponent>(caster)
            .slots[static_cast<u8_t>(eSkillSlot::Q)];
        const f32_t limited = std::max(0.f, cooldownSec);
        q.cooldownRemaining = std::min(q.cooldownRemaining, limited);
        q.cooldownDuration = std::min(q.cooldownDuration, limited);
    }

    void ReleaseIreliaQAction(CWorld& world, const TickContext& tc, EntityID caster)
    {
        if (!world.HasComponent<ActionStateComponent>(caster))
            return;

        auto& action = world.GetComponent<ActionStateComponent>(caster);
        if (action.sourceChampion == eChampion::IRELIA &&
            action.sourceSlot == static_cast<u8_t>(eSkillSlot::Q))
        {
            action.lockEndTick = std::min(action.lockEndTick, tc.tickIndex);
            action.movePolicy = eSkillActionMovePolicy::Allow;
        }
    }

    void ApplyIreliaMark(CWorld& world, const TickContext& tc, EntityID source, EntityID target, eSkillSlot slot)
    {
        if (source == NULL_ENTITY || target == NULL_ENTITY || !world.IsAlive(target))
            return;

        const f32_t markDurationSec = ResolveIreliaSkillEffectParam(
            world, tc, source, slot, eSkillEffectParamId::MarkDurationSec);
        if (markDurationSec <= 0.f)
            return;

        IreliaMarkComponent mark{};
        mark.sourceEntity = source;
        mark.fRemainingSec = markDurationSec;

        if (world.HasComponent<IreliaMarkComponent>(target))
            world.GetComponent<IreliaMarkComponent>(target) = mark;
        else
            world.AddComponent<IreliaMarkComponent>(target, mark);
    }

    void ResolveIreliaQImpact(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        EntityID target,
        u8_t rank)
    {
        if (target == NULL_ENTITY || !world.IsAlive(target) ||
            !world.HasComponent<HealthComponent>(target))
        {
            return;
        }

        const auto& health = world.GetComponent<HealthComponent>(target);
        if (health.bIsDead || health.fCurrent <= 0.f)
            return;

        const eTeam casterTeam = world.HasComponent<ChampionComponent>(caster)
            ? world.GetComponent<ChampionComponent>(caster).team
            : eTeam::Neutral;
        EnqueuePhysicalDamage(
            world,
            caster,
            target,
            casterTeam,
            ResolveIreliaSkillEffectParam(
                world, tc, caster, eSkillSlot::Q, eSkillEffectParamId::BaseDamage) +
            ResolveIreliaSkillEffectParam(
                world, tc, caster, eSkillSlot::Q, eSkillEffectParamId::DamagePerRank) *
                static_cast<f32_t>(rank),
            kIreliaQSkillId,
            rank);
    }

    Vec3 ResolveRForward(CWorld& world, EntityID caster, const GameCommand* pCommand)
    {
        if (pCommand)
        {
            const f32_t fLenSq =
                pCommand->direction.x * pCommand->direction.x +
                pCommand->direction.z * pCommand->direction.z;
            if (fLenSq > 0.0001f)
            {
                const f32_t fInvLen = 1.f / std::sqrtf(fLenSq);
                return Vec3{
                    pCommand->direction.x * fInvLen,
                    0.f,
                    pCommand->direction.z * fInvLen
                };
            }
        }

        if (world.HasComponent<TransformComponent>(caster))
        {
            const f32_t fYaw =
                world.GetComponent<TransformComponent>(caster).GetRotation().y -
                GetDefaultChampionVisualYawOffset(eChampion::IRELIA);
            return Vec3{ std::sinf(fYaw), 0.f, std::cosf(fYaw) };
        }

        return Vec3{ 0.f, 0.f, -1.f };
    }

    u16_t EncodeIreliaREffectFlags(u8_t stage, u8_t rank)
    {
        return static_cast<u16_t>(
            (static_cast<u16_t>(stage & 0x0fu) << 12) |
            (static_cast<u16_t>(rank & 0x0fu) << 8) |
            static_cast<u16_t>(eSkillSlot::R));
    }

    void EmitIreliaREffect(CWorld& world,
        EntityID caster,
        EntityID target,
        u8_t rank,
        u8_t stage,
        const Vec3& vPos,
        const Vec3& vForward,
        u16_t durationMs,
        const TickContext& tc)
    {
        ReplicatedEventComponent event{};
        event.kind = eReplicatedEventKind::EffectTrigger;
        event.sourceEntity = caster;
        event.targetEntity = target;
        event.effectId = MakeGameplayHookId(eChampion::IRELIA, GameplayHookVariant::R_OnCastAccepted);
        event.slot = static_cast<u8_t>(eSkillSlot::R);
        event.rank = rank;
        event.flags = EncodeIreliaREffectFlags(stage, rank);
        event.position = vPos;
        event.direction = vForward;
        event.durationMs = durationMs;
        event.startTick = tc.tickIndex;

        EnqueueReplicatedEvent(world, event);
    }

    void ClearTrackedTargets(EntityID* targets, u8_t& count)
    {
        for (u8_t i = 0; i < IreliaSimComponent::kRMaxTrackedTargets; ++i)
            targets[i] = NULL_ENTITY;
        count = 0;
    }

    bool_t IsTrackedTarget(const EntityID* targets, u8_t count, EntityID target)
    {
        for (u8_t i = 0; i < count; ++i)
        {
            if (targets[i] == target)
                return true;
        }
        return false;
    }

    void TrackTarget(EntityID* targets, u8_t& count, EntityID target)
    {
        if (target == NULL_ENTITY || IsTrackedTarget(targets, count, target))
            return;
        if (count >= IreliaSimComponent::kRMaxTrackedTargets)
            return;
        targets[count++] = target;
    }

    void ResetRWave(IreliaSimComponent& state)
    {
        state.rWavePos = {};
        state.rWaveDir = {};
        state.rWaveTravelled = 0.f;
        state.rWallRemainingSec = 0.f;
        state.rRank = 0;
        state.bRWaveActive = false;
        state.bRWallActive = false;
        ClearTrackedTargets(state.rHitTargets, state.rHitTargetCount);
        ClearTrackedTargets(state.rWallTargets, state.rWallTargetCount);
    }

    bool_t IsAliveChampion(CWorld& world, EntityID target)
    {
        if (target == NULL_ENTITY || !world.IsAlive(target))
            return false;
        if (world.HasComponent<HealthComponent>(target))
        {
            const HealthComponent& health = world.GetComponent<HealthComponent>(target);
            if (health.bIsDead || health.fCurrent <= 0.f)
                return false;
        }
        return true;
    }

    bool_t IsInRRectangle(const Vec3& targetPos,
        const Vec3& center,
        const Vec3& forward,
        f32_t length,
        f32_t width)
    {
        const f32_t dx = targetPos.x - center.x;
        const f32_t dz = targetPos.z - center.z;
        const f32_t along = dx * forward.x + dz * forward.z;
        const f32_t perp = dx * (-forward.z) + dz * forward.x;
        return std::fabs(along) <= length * 0.5f &&
            std::fabs(perp) <= width * 0.5f;
    }

    void ApplyDisarm(CWorld& world, EntityID target, EntityID source, f32_t duration)
    {
        if (!IsAliveChampion(world, target))
            return;

        DisarmComponent disarm{};
        disarm.fRemaining = duration;
        disarm.sourceEntity = source;
        if (world.HasComponent<DisarmComponent>(target))
            world.GetComponent<DisarmComponent>(target) = disarm;
        else
            world.AddComponent<DisarmComponent>(target, disarm);

        GameplayStatus::RebuildGameplayState(world, target);
    }

    void StartRWall(CWorld& world,
        const TickContext& tc,
        EntityID caster,
        IreliaSimComponent& state,
        bool_t bEmitWallCue,
        f32_t wallDurationSec)
    {
        state.bRWaveActive = false;
        state.bRWallActive = true;
        state.rWallRemainingSec = wallDurationSec;

        if (bEmitWallCue)
        {
            EmitIreliaREffect(world,
                caster,
                NULL_ENTITY,
                state.rRank,
                kIreliaREffectStageWall,
                state.rWavePos,
                state.rWaveDir,
                static_cast<u16_t>(wallDurationSec * 1000.f),
                tc);
        }
    }

    void TickRWave(CWorld& world,
        const TickContext& tc,
        EntityID caster,
        IreliaSimComponent& state)
    {
        if (!state.bRWaveActive && !state.bRWallActive)
            return;
        if (!world.HasComponent<ChampionComponent>(caster))
            return;

        const eTeam casterTeam = world.GetComponent<ChampionComponent>(caster).team;

        const f32_t rRange = ResolveIreliaSkillEffectParam(
            world, tc, caster, eSkillSlot::R, eSkillEffectParamId::Range);
        const f32_t rSpeed = ResolveIreliaSkillEffectParam(
            world, tc, caster, eSkillSlot::R, eSkillEffectParamId::Speed);
        const f32_t rLength = ResolveIreliaSkillEffectParam(
            world, tc, caster, eSkillSlot::R, eSkillEffectParamId::RectLength);
        const f32_t rWidth = ResolveIreliaSkillEffectParam(
            world, tc, caster, eSkillSlot::R, eSkillEffectParamId::RectWidth);
        const f32_t rDamage = ResolveIreliaSkillEffectParam(
            world, tc, caster, eSkillSlot::R, eSkillEffectParamId::BaseDamage);
        const f32_t rDisarmSec = ResolveIreliaSkillEffectParam(
            world, tc, caster, eSkillSlot::R, eSkillEffectParamId::DisarmDurationSec);
        const f32_t rWallSlowSec = ResolveIreliaSkillEffectParam(
            world, tc, caster, eSkillSlot::R, eSkillEffectParamId::SlowDurationSec);
        const f32_t rWallSlowMul = ResolveIreliaSkillEffectParam(
            world, tc, caster, eSkillSlot::R, eSkillEffectParamId::MoveSpeedMul);
        const f32_t rWallDurationSec = ResolveIreliaSkillEffectParam(
            world, tc, caster, eSkillSlot::R, eSkillEffectParamId::EffectDurationSec);

        if (state.bRWaveActive)
        {
            const f32_t remaining = rRange - state.rWaveTravelled;
            const f32_t step = rSpeed * tc.fDt;
            const f32_t appliedStep = step < remaining ? step : remaining;
            state.rWavePos.x += state.rWaveDir.x * appliedStep;
            state.rWavePos.z += state.rWaveDir.z * appliedStep;
            state.rWaveTravelled += appliedStep;

            EntityID hitTarget = NULL_ENTITY;
            Vec3 hitPos{};

            world.ForEach<ChampionComponent, TransformComponent>(
                std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                    [&](EntityID target, ChampionComponent& champion, TransformComponent& tf)
                    {
                        if (hitTarget != NULL_ENTITY)
                            return;
                        if (target == caster || champion.team == casterTeam)
                            return;
                        if (!IsAliveChampion(world, target))
                            return;
                        if (IsTrackedTarget(state.rHitTargets, state.rHitTargetCount, target))
                            return;

                        const Vec3 targetPos = tf.GetPosition();
                        if (!IsInRRectangle(targetPos, state.rWavePos, state.rWaveDir,
                            rLength, rWidth))
                        {
                            return;
                        }

                        hitTarget = target;
                        hitPos = targetPos;
                    }));

            if (hitTarget != NULL_ENTITY)
            {
                TrackTarget(state.rHitTargets, state.rHitTargetCount, hitTarget);
                TrackTarget(state.rWallTargets, state.rWallTargetCount, hitTarget);
                state.rWavePos = hitPos;

                EnqueuePhysicalDamage(
                    world,
                    caster,
                    hitTarget,
                    casterTeam,
                    rDamage,
                    static_cast<u16_t>((static_cast<u32_t>(eChampion::IRELIA) << 8) |
                        static_cast<u32_t>(eSkillSlot::R)),
                    state.rRank);
                ApplyDisarm(world, hitTarget, caster, rDisarmSec);
                GameplayStatus::ApplySlow(
                    world,
                    tc,
                    hitTarget,
                    caster,
                    eChampion::IRELIA,
                    eSkillSlot::R,
                    rWallSlowSec,
                    rWallSlowMul);

                EmitIreliaREffect(world,
                    caster,
                    hitTarget,
                    state.rRank,
                    kIreliaREffectStageHit,
                    hitPos,
                    state.rWaveDir,
                    static_cast<u16_t>(rWallDurationSec * 1000.f),
                    tc);
                ApplyIreliaMark(world, tc, caster, hitTarget, eSkillSlot::R);
                StartRWall(world, tc, caster, state, false, rWallDurationSec);
                return;
            }

            if (state.rWaveTravelled >= rRange)
            {
                StartRWall(world, tc, caster, state, true, rWallDurationSec);
                return;
            }
        }

        if (!state.bRWallActive)
            return;

        state.rWallRemainingSec -= tc.fDt;
        if (state.rWallRemainingSec <= 0.f)
        {
            ResetRWave(state);
            return;
        }

        world.ForEach<ChampionComponent, TransformComponent>(
            std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                [&](EntityID target, ChampionComponent& champion, TransformComponent& tf)
                {
                    if (target == caster || champion.team == casterTeam)
                        return;
                    if (!IsAliveChampion(world, target))
                        return;
                    if (IsTrackedTarget(state.rWallTargets, state.rWallTargetCount, target))
                        return;

                    const Vec3 targetPos = tf.GetPosition();
                    if (!IsInRRectangle(targetPos, state.rWavePos, state.rWaveDir,
                        rLength, rWidth))
                    {
                        return;
                    }

                    TrackTarget(state.rWallTargets, state.rWallTargetCount, target);
                    GameplayStatus::ApplySlow(
                        world,
                        tc,
                        target,
                        caster,
                        eChampion::IRELIA,
                        eSkillSlot::R,
                        rWallSlowSec,
                        rWallSlowMul);
                    ApplyDisarm(world, target, caster, rDisarmSec);
                    EmitIreliaREffect(world,
                        caster,
                        target,
                        state.rRank,
                        kIreliaREffectStageWallMark,
                        targetPos,
                        state.rWaveDir,
                        static_cast<u16_t>(rDisarmSec * 1000.f),
                        tc);
                    ApplyIreliaMark(world, tc, caster, target, eSkillSlot::R);
                }));
    }

    void OnQ(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand || !ctx.pTickCtx)
            return;

        CWorld& world = *ctx.pWorld;
        const GameCommand& cmd = *ctx.pCommand;

        if (!world.HasComponent<TransformComponent>(ctx.casterEntity) ||
            cmd.targetEntity == NULL_ENTITY ||
            !world.HasComponent<TransformComponent>(cmd.targetEntity))
        {
            return;
        }

        auto& casterTf = world.GetComponent<TransformComponent>(ctx.casterEntity);
        const Vec3 casterPos = casterTf.GetPosition();
        const Vec3 targetPos = world.GetComponent<TransformComponent>(cmd.targetEntity).GetPosition();
        const f32_t dashStopGap = ResolveIreliaSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::Gap,
            IreliaGameSim::kQStopGapFallback);
        const Vec3 endPos = IreliaGameSim::ResolveQDashEndPos(casterPos, targetPos, dashStopGap);

        IreliaSimComponent& state = GetIreliaState(world, ctx.casterEntity);
        state.dashStartPos = casterPos;
        state.dashEndPos = endPos;
        state.dashSpeed = ResolveIreliaSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::Speed,
            IreliaGameSim::kQBaseDashSpeedFallback);
        if (world.HasComponent<StatComponent>(ctx.casterEntity))
            state.dashSpeed += world.GetComponent<StatComponent>(ctx.casterEntity).moveSpeed;
        state.dashSpeed = std::max(0.01f, state.dashSpeed);
        state.dashTarget = cmd.targetEntity;
        state.qRank = ctx.skillRank;
        state.bDashActive = true;

        ClearMove(world, ctx.casterEntity);

        std::cout << "[IreliaSim] Q dash caster=" << ctx.casterEntity
            << " target=" << cmd.targetEntity
            << " start=(" << casterPos.x << "," << casterPos.y << "," << casterPos.z << ")"
            << " end=(" << endPos.x << "," << endPos.y << "," << endPos.z << ")\n";
    }

    void OnW(GameplayHookContext& ctx)
    {
        const bool_t bStage2 = ctx.pCommand != nullptr && ctx.pCommand->itemId == 2u;
        std::cout << "[IreliaSim] W " << (bStage2 ? "release" : "hold")
            << " caster=" << ctx.casterEntity << "\n";

        if (!bStage2 || !ctx.pWorld || !ctx.pCommand || !ctx.pTickCtx ||
            !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
        {
            return;
        }

        CWorld& world = *ctx.pWorld;
        const Vec3 origin =
            world.GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        const Vec3 forward = ResolveRForward(world, ctx.casterEntity, ctx.pCommand);
        const f32_t wBaseDamage = ResolveIreliaSkillEffectParam(
            ctx, eSkillSlot::W, eSkillEffectParamId::BaseDamage);
        const f32_t wDamagePerRank = ResolveIreliaSkillEffectParam(
            ctx, eSkillSlot::W, eSkillEffectParamId::DamagePerRank);
        f32_t wRange = ResolveIreliaSkillEffectParam(
            ctx, eSkillSlot::W, eSkillEffectParamId::Range);
        const f32_t wHalfWidth = ResolveIreliaSkillEffectParam(
            ctx, eSkillSlot::W, eSkillEffectParamId::HalfWidth);
        f32_t damage =
            wBaseDamage + wDamagePerRank * static_cast<f32_t>(ctx.skillRank);
        f32_t skillDamageScale = 1.f;
        if (world.HasComponent<SkillChargeStateComponent>(ctx.casterEntity))
        {
            const f32_t ratio =
                world.GetComponent<SkillChargeStateComponent>(
                    ctx.casterEntity).chargeRatio;
            if (const SkillGameplayDef* skill = GameplayDefinitionQuery::FindSkill(
                world,
                ctx.casterEntity,
                *ctx.pTickCtx,
                eChampion::IRELIA,
                static_cast<u8_t>(eSkillSlot::W)))
            {
                const f32_t rangeScale = ResolveSkillChargeValue(
                    skill->charge.minRangeScale,
                    skill->charge.maxRangeScale,
                    ratio);
                skillDamageScale = ResolveSkillChargeValue(
                    skill->charge.minDamageScale,
                    skill->charge.maxDamageScale,
                    ratio);
                wRange *= rangeScale;
            }
        }

        auto tryHit = [&](EntityID target, eTeam team, const Vec3& targetPos)
        {
            if (target == ctx.casterEntity || team == ctx.casterTeam)
                return;

            const Vec3 delta{ targetPos.x - origin.x, 0.f, targetPos.z - origin.z };
            const f32_t along = delta.x * forward.x + delta.z * forward.z;
            if (along < 0.f || along > wRange)
                return;

            const Vec3 perp{
                delta.x - forward.x * along,
                0.f,
                delta.z - forward.z * along
            };
            if ((perp.x * perp.x + perp.z * perp.z) >
                wHalfWidth * wHalfWidth)
            {
                return;
            }

            EnqueuePhysicalDamage(
                world,
                ctx.casterEntity,
                target,
                ctx.casterTeam,
                damage,
                static_cast<u16_t>((static_cast<u32_t>(eChampion::IRELIA) << 8) | 2u),
                ctx.skillRank,
                skillDamageScale);
        };

        world.ForEach<ChampionComponent, TransformComponent>(
            std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                [&](EntityID entity, ChampionComponent& champion, TransformComponent& tf)
                {
                    tryHit(entity, champion.team, tf.GetPosition());
                }));

        world.ForEach<MinionComponent, TransformComponent>(
            std::function<void(EntityID, MinionComponent&, TransformComponent&)>(
                [&](EntityID entity, MinionComponent& minion, TransformComponent& tf)
                {
                    tryHit(entity, minion.team, tf.GetPosition());
                }));
    }

    void OnE(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand || !ctx.pTickCtx)
            return;

        CWorld& world = *ctx.pWorld;
        const GameCommand& cmd = *ctx.pCommand;
        IreliaSimComponent& state = GetIreliaState(world, ctx.casterEntity);

        const bool_t bStage2 = cmd.itemId == 2u;
        if (!bStage2)
        {
            state.blade1Pos = cmd.groundPos;
            state.blade1Tick = ctx.pTickCtx ? ctx.pTickCtx->tickIndex : 0;
            state.bHasBlade1 = true;

            std::cout << "[IreliaSim] E blade1 pos=("
                << state.blade1Pos.x << "," << state.blade1Pos.y << "," << state.blade1Pos.z << ")\n";
            return;
        }

        state.blade2Pos = cmd.groundPos;
        state.blade2Tick = ctx.pTickCtx ? ctx.pTickCtx->tickIndex : 0;
        state.bHasBlade2 = true;

        if (!state.bHasBlade1)
            return;

        const Vec3 a = state.blade1Pos;
        const Vec3 b = state.blade2Pos;
        const f32_t beamRadius = ResolveIreliaSkillEffectParam(
            ctx, eSkillSlot::E, eSkillEffectParamId::Radius);
        const f32_t stunSec = ResolveIreliaSkillEffectParam(
            ctx, eSkillSlot::E, eSkillEffectParamId::StunDurationSec);
        const f32_t eBaseDamage = ResolveIreliaSkillEffectParam(
            ctx, eSkillSlot::E, eSkillEffectParamId::BaseDamage);
        const f32_t eDamagePerRank = ResolveIreliaSkillEffectParam(
            ctx, eSkillSlot::E, eSkillEffectParamId::DamagePerRank);

        const std::vector<EntityID> targets =
            GameplayStateQuery::CollectEnemyMobileUnitsInSegment(
                world, ctx.casterEntity, a, b, beamRadius);
        for (EntityID target : targets)
        {
            GameplayStatus::ApplyStun(
                world,
                *ctx.pTickCtx,
                target,
                ctx.casterEntity,
                eChampion::IRELIA,
                eSkillSlot::E,
                stunSec);

            EnqueuePhysicalDamage(
                world,
                ctx.casterEntity,
                target,
                ctx.casterTeam,
                eBaseDamage + eDamagePerRank * static_cast<f32_t>(ctx.skillRank),
                static_cast<u16_t>((static_cast<u32_t>(eChampion::IRELIA) << 8) | 3u),
                ctx.skillRank);

            ApplyIreliaMark(world, *ctx.pTickCtx, ctx.casterEntity, target, eSkillSlot::E);
        }

        state.blade1Pos = {};
        state.blade2Pos = {};
        state.blade1Tick = 0;
        state.blade2Tick = 0;
        state.bHasBlade1 = false;
        state.bHasBlade2 = false;

        std::cout << "[IreliaSim] E beam resolved\n";
    }

    void OnR(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand || !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
            return;

        CWorld& world = *ctx.pWorld;
        const Vec3 vOrigin = world.GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        const Vec3 vForward = ResolveRForward(world, ctx.casterEntity, ctx.pCommand);

        IreliaSimComponent& state = GetIreliaState(world, ctx.casterEntity);

        ResetRWave(state);
        state.rWavePos = vOrigin;
        state.rWaveDir = vForward;
        state.rRank = ctx.skillRank;
        state.bRWaveActive = true;

        ClearMove(world, ctx.casterEntity);
    }
}

namespace IreliaGameSim
{
    bool_t CanCastBladesurge(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        EntityID target)
    {
        if (caster == NULL_ENTITY || target == NULL_ENTITY ||
            !world.IsAlive(caster) || !world.IsAlive(target) ||
            !world.HasComponent<TransformComponent>(caster) ||
            !world.HasComponent<TransformComponent>(target) ||
            !world.HasComponent<HealthComponent>(target) ||
            !GameplayStateQuery::CanBeTargetedBy(world, caster, target))
        {
            return false;
        }

        const HealthComponent& health =
            world.GetComponent<HealthComponent>(target);
        if (health.bIsDead || health.fCurrent <= 0.f)
            return false;

        const Vec3 casterPos =
            world.GetComponent<TransformComponent>(caster).GetLocalPosition();
        const Vec3 targetPos =
            world.GetComponent<TransformComponent>(target).GetLocalPosition();
        const f32_t range = GameplayDefinitionQuery::ResolveSkillRange(
            world,
            caster,
            tc,
            eChampion::IRELIA,
            static_cast<u8_t>(eSkillSlot::Q));
        const f32_t effectiveRange = range +
            GameplayStateQuery::ResolveGameplayRadius(world, caster) +
            GameplayStateQuery::ResolveGameplayRadius(world, target);
        if (range <= 0.f ||
            WintersMath::DistanceSqXZ(casterPos, targetPos) >
                effectiveRange * effectiveRange)
        {
            return false;
        }

        const f32_t gap = ResolveIreliaSkillEffectParam(
            world,
            tc,
            caster,
            eSkillSlot::Q,
            eSkillEffectParamId::Gap,
            kQStopGapFallback);
        const Vec3 rawLanding = ResolveQDashEndPos(
            casterPos, targetPos, gap);
        Vec3 landing{};
        if (!TryResolveIreliaQTargetSideLanding(
                tc, targetPos, rawLanding, landing))
        {
            return false;
        }
        const f32_t maxLandingDistance = gap + 1.5f;
        return WintersMath::DistanceSqXZ(landing, targetPos) <=
            maxLandingDistance * maxLandingDistance;
    }

    void Tick(CWorld& world, const TickContext& tc)
    {
        world.ForEach<IreliaSimComponent, TransformComponent>(
            std::function<void(EntityID, IreliaSimComponent&, TransformComponent&)>(
                [&](EntityID entity, IreliaSimComponent& state, TransformComponent& transform)
                {
                    if (state.bDashActive)
                    {
                        const EntityID target = state.dashTarget;
                        const bool_t bCancelled =
                            !GameplayStateQuery::CanMove(world, entity) ||
                            world.HasComponent<ForcedMotionComponent>(entity) ||
                            target == NULL_ENTITY ||
                            !world.IsAlive(target) ||
                            !world.HasComponent<TransformComponent>(target) ||
                            !world.HasComponent<HealthComponent>(target) ||
                            world.GetComponent<HealthComponent>(target).bIsDead ||
                            world.GetComponent<HealthComponent>(target).fCurrent <= 0.f;
                        if (bCancelled)
                        {
                            SnapDashArrivalToWalkable(
                                world, tc, entity, state.dashStartPos);
                            state.bDashActive = false;
                            state.dashSpeed = 0.f;
                            state.dashTarget = NULL_ENTITY;
                            state.qRank = 0u;
                            ReleaseIreliaQAction(world, tc, entity);
                        }
                        else
                        {
                            ClearMove(world, entity);
                            const Vec3 currentPos = transform.GetLocalPosition();
                            const Vec3 targetPos = world.GetComponent<TransformComponent>(target)
                                .GetLocalPosition();
                            const f32_t gap = ResolveIreliaSkillEffectParam(
                                world,
                                tc,
                                entity,
                                eSkillSlot::Q,
                                eSkillEffectParamId::Gap,
                                IreliaGameSim::kQStopGapFallback);
                            const Vec3 rawEnd =
                                ResolveQDashEndPos(currentPos, targetPos, gap);
                            Vec3 desiredEnd{};
                            if (!TryResolveIreliaQTargetSideLanding(
                                    tc, targetPos, rawEnd, desiredEnd))
                            {
                                SnapDashArrivalToWalkable(
                                    world, tc, entity, state.dashStartPos);
                                state.bDashActive = false;
                                state.dashSpeed = 0.f;
                                state.dashTarget = NULL_ENTITY;
                                state.qRank = 0u;
                                ReleaseIreliaQAction(world, tc, entity);
                                return;
                            }
                            state.dashEndPos = desiredEnd;

                            const Vec3 delta{
                                desiredEnd.x - currentPos.x,
                                0.f,
                                desiredEnd.z - currentPos.z };
                            const f32_t remaining = std::sqrtf(
                                delta.x * delta.x + delta.z * delta.z);
                            const f32_t step = state.dashSpeed * tc.fDt;
                            const bool_t bContact =
                                remaining <= step || remaining <= 0.0001f;
                            const Vec3 desiredPos = bContact
                                ? desiredEnd
                                : Vec3{
                                    currentPos.x + delta.x * (step / remaining),
                                    currentPos.y,
                                    currentPos.z + delta.z * (step / remaining) };
                            transform.SetPosition(desiredPos);

                            const Vec3 dir = WintersMath::NormalizeXZ(delta);
                            if (dir.x * dir.x + dir.z * dir.z > 0.0001f)
                            {
                                const Vec3 rot = transform.GetRotation();
                                transform.SetRotation({
                                    rot.x,
                                    ResolveChampionVisualYawNear(
                                        eChampion::IRELIA, dir, rot.y),
                                    rot.z });
                            }

                            if (bContact)
                            {
                                const u8_t impactRank = state.qRank;
                                state.bDashActive = false;
                                state.dashSpeed = 0.f;
                                state.dashTarget = NULL_ENTITY;
                                state.qRank = 0u;
                                const Vec3 finalPos = transform.GetLocalPosition();
                                const f32_t maxImpactDistance = gap + 1.5f;
                                const bool_t bValidArrival =
                                    (!tc.pWalkable ||
                                        tc.pWalkable->IsWalkableXZ(finalPos)) &&
                                    WintersMath::DistanceSqXZ(finalPos, targetPos) <=
                                        maxImpactDistance * maxImpactDistance;
                                ReleaseIreliaQAction(world, tc, entity);
                                if (bValidArrival)
                                {
                                    ResolveIreliaQImpact(
                                        world, tc, entity, target, impactRank);
                                }
                            }
                        }
                    }

                    TickRWave(world, tc, entity, state);
                }));

        std::vector<EntityID> expiredMarks;
        world.ForEach<IreliaMarkComponent>(
            std::function<void(EntityID, IreliaMarkComponent&)>(
                [&](EntityID entity, IreliaMarkComponent& mark)
                {
                    mark.fRemainingSec = std::max(0.f, mark.fRemainingSec - tc.fDt);
                    if (mark.fRemainingSec <= 0.f || !world.IsAlive(mark.sourceEntity))
                        expiredMarks.push_back(entity);
                }));

        for (EntityID entity : expiredMarks)
            world.RemoveComponent<IreliaMarkComponent>(entity);
    }

    void OnDamageResolved(
        CWorld& world,
        const TickContext&,
        const DamageRequest& request,
        const DamageResult& result)
    {
        if (!result.bApplied || request.source == NULL_ENTITY ||
            request.target == NULL_ENTITY ||
            request.eSourceKind != eDamageSourceKind::Skill ||
            request.iSourceSlot != static_cast<u8_t>(eSkillSlot::Q) ||
            request.skillId != kIreliaQSkillId ||
            !world.HasComponent<ChampionComponent>(request.source) ||
            world.GetComponent<ChampionComponent>(request.source).id != eChampion::IRELIA)
        {
            return;
        }

        if (world.HasComponent<IreliaMarkComponent>(request.target))
        {
            const auto& mark = world.GetComponent<IreliaMarkComponent>(request.target);
            if (mark.sourceEntity == request.source && mark.fRemainingSec > 0.f)
            {
                world.RemoveComponent<IreliaMarkComponent>(request.target);
                LimitIreliaQCooldown(world, request.source, 0.f);
            }
        }

        if (result.bKilled &&
            GameplayStateQuery::IsMobileCombatUnit(world, request.target))
        {
            LimitIreliaQCooldown(
                world, request.source, kIreliaQKillResetCooldownSec);
        }
    }

    void RegisterHooks()
    {
        static bool_t s_bRegistered = false;
        if (s_bRegistered)
            return;

        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::IRELIA, GameplayHookVariant::Q_OnCastAccepted), &OnQ);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::IRELIA, GameplayHookVariant::W_OnCastAccepted), &OnW);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::IRELIA, GameplayHookVariant::E_OnCastAccepted), &OnE);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::IRELIA, GameplayHookVariant::R_OnCastAccepted), &OnR);

        s_bRegistered = true;
        std::cout << "[IreliaSim] hooks registered\n";
    }
}
