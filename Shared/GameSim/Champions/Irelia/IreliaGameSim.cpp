#include "Shared/GameSim/Champions/Irelia/IreliaGameSim.h"

#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/IreliaSimComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Definitions/GameplayDefinitionQuery.h"
#include "Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/ReplicatedEventQueue/ReplicatedEventQueue.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectRequests.h"

#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"

#include <cmath>
#include <functional>
#include <iostream>

namespace
{
    constexpr f32_t kIreliaRLength = 5.f;
    constexpr f32_t kIreliaRRange = 8.f;
    constexpr f32_t kIreliaRWidth = 7.5f;
    constexpr f32_t kIreliaRSpeed = 15.f;
    constexpr f32_t kIreliaRDamage = 250.f;
    constexpr f32_t kIreliaRDisarmSec = 1.5f;
    constexpr f32_t kIreliaRWallSlowSec = 0.5f;
    constexpr f32_t kIreliaRWallSlowMul = 0.5f;
    constexpr f32_t kIreliaRWallDurationSec = 2.5f;
    constexpr u8_t kIreliaREffectStageHit = 2u;
    constexpr u8_t kIreliaREffectStageWall = 3u;
    constexpr u8_t kIreliaREffectStageWallMark = 4u;
    constexpr f32_t kIreliaQDashDurationSec = 0.25f;
    constexpr f32_t kIreliaQBaseDamage = 45.f;
    constexpr f32_t kIreliaQDamagePerRank = 25.f;
    constexpr f32_t kIreliaWRange = 6.0f;
    constexpr f32_t kIreliaWHalfWidth = 2.2f;
    constexpr f32_t kIreliaWBaseDamage = 30.f;
    constexpr f32_t kIreliaWDamagePerRank = 40.f;
    constexpr f32_t kIreliaEBeamRadius = 1.5f;
    constexpr f32_t kIreliaEStunSec = 0.75f;
    constexpr f32_t kIreliaEBaseDamage = 70.f;
    constexpr f32_t kIreliaEDamagePerRank = 30.f;

    f32_t ResolveIreliaSkillEffectParam(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        eSkillSlot slot,
        eSkillEffectParamId param,
        f32_t fallbackValue)
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
        f32_t fallbackValue)
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

    void EnqueuePhysicalDamage(CWorld& world, EntityID source, EntityID target, eTeam team, f32_t amount, u16_t skillId, u8_t rank)
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
        EnqueueDamageRequest(world, request);
    }

    IreliaSimComponent& GetIreliaState(CWorld& world, EntityID caster)
    {
        if (!world.HasComponent<IreliaSimComponent>(caster))
            world.AddComponent<IreliaSimComponent>(caster, IreliaSimComponent{});

        return world.GetComponent<IreliaSimComponent>(caster);
    }

    Vec3 Lerp(const Vec3& a, const Vec3& b, f32_t t)
    {
        return Vec3{
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t
        };
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
                ChampionGameDataDB::ResolveVisualYawOffset(eChampion::IRELIA);
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
            world, tc, caster, eSkillSlot::R, eSkillEffectParamId::Range, kIreliaRRange);
        const f32_t rSpeed = ResolveIreliaSkillEffectParam(
            world, tc, caster, eSkillSlot::R, eSkillEffectParamId::Speed, kIreliaRSpeed);
        const f32_t rLength = ResolveIreliaSkillEffectParam(
            world, tc, caster, eSkillSlot::R, eSkillEffectParamId::RectLength, kIreliaRLength);
        const f32_t rWidth = ResolveIreliaSkillEffectParam(
            world, tc, caster, eSkillSlot::R, eSkillEffectParamId::RectWidth, kIreliaRWidth);
        const f32_t rDamage = ResolveIreliaSkillEffectParam(
            world, tc, caster, eSkillSlot::R, eSkillEffectParamId::BaseDamage, kIreliaRDamage);
        const f32_t rDisarmSec = ResolveIreliaSkillEffectParam(
            world, tc, caster, eSkillSlot::R, eSkillEffectParamId::DisarmDurationSec, kIreliaRDisarmSec);
        const f32_t rWallSlowSec = ResolveIreliaSkillEffectParam(
            world, tc, caster, eSkillSlot::R, eSkillEffectParamId::SlowDurationSec, kIreliaRWallSlowSec);
        const f32_t rWallSlowMul = ResolveIreliaSkillEffectParam(
            world, tc, caster, eSkillSlot::R, eSkillEffectParamId::MoveSpeedMul, kIreliaRWallSlowMul);
        const f32_t rWallDurationSec = ResolveIreliaSkillEffectParam(
            world, tc, caster, eSkillSlot::R, eSkillEffectParamId::EffectDurationSec, kIreliaRWallDurationSec);

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
                }));
    }

    void OnQ(GameplayHookContext& ctx)
    {
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
        const Vec3 endPos = IreliaGameSim::ResolveQDashEndPos(casterPos, targetPos);

        IreliaSimComponent& state = GetIreliaState(world, ctx.casterEntity);
        state.dashStartPos = casterPos;
        state.dashEndPos = endPos;
        state.dashElapsedSec = 0.f;
        state.dashDurationSec = ResolveIreliaSkillEffectParam(
            ctx, eSkillSlot::Q, eSkillEffectParamId::DashDurationSec, kIreliaQDashDurationSec);
        state.dashTarget = cmd.targetEntity;
        state.bDashActive = true;

        ClearMove(world, ctx.casterEntity);

        const f32_t qBaseDamage = ResolveIreliaSkillEffectParam(
            ctx, eSkillSlot::Q, eSkillEffectParamId::BaseDamage, kIreliaQBaseDamage);
        const f32_t qDamagePerRank = ResolveIreliaSkillEffectParam(
            ctx, eSkillSlot::Q, eSkillEffectParamId::DamagePerRank, kIreliaQDamagePerRank);

        EnqueuePhysicalDamage(
            world,
            ctx.casterEntity,
            cmd.targetEntity,
            ctx.casterTeam,
            qBaseDamage + qDamagePerRank * static_cast<f32_t>(ctx.skillRank),
            static_cast<u16_t>((static_cast<u32_t>(eChampion::IRELIA) << 8) | 1u),
            ctx.skillRank);

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

        if (!bStage2 || !ctx.pWorld || !ctx.pCommand ||
            !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
        {
            return;
        }

        CWorld& world = *ctx.pWorld;
        const Vec3 origin =
            world.GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        const Vec3 forward = ResolveRForward(world, ctx.casterEntity, ctx.pCommand);
        const f32_t wBaseDamage = ResolveIreliaSkillEffectParam(
            ctx, eSkillSlot::W, eSkillEffectParamId::BaseDamage, kIreliaWBaseDamage);
        const f32_t wDamagePerRank = ResolveIreliaSkillEffectParam(
            ctx, eSkillSlot::W, eSkillEffectParamId::DamagePerRank, kIreliaWDamagePerRank);
        const f32_t wRange = ResolveIreliaSkillEffectParam(
            ctx, eSkillSlot::W, eSkillEffectParamId::Range, kIreliaWRange);
        const f32_t wHalfWidth = ResolveIreliaSkillEffectParam(
            ctx, eSkillSlot::W, eSkillEffectParamId::HalfWidth, kIreliaWHalfWidth);
        const f32_t damage =
            wBaseDamage + wDamagePerRank * static_cast<f32_t>(ctx.skillRank);

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
                ctx.skillRank);
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
        const f32_t dx = b.x - a.x;
        const f32_t dz = b.z - a.z;
        const f32_t segLenSq = dx * dx + dz * dz + 0.000001f;
        const f32_t beamRadius = ResolveIreliaSkillEffectParam(
            ctx, eSkillSlot::E, eSkillEffectParamId::Radius, kIreliaEBeamRadius);
        const f32_t stunSec = ResolveIreliaSkillEffectParam(
            ctx, eSkillSlot::E, eSkillEffectParamId::StunDurationSec, kIreliaEStunSec);
        const f32_t eBaseDamage = ResolveIreliaSkillEffectParam(
            ctx, eSkillSlot::E, eSkillEffectParamId::BaseDamage, kIreliaEBaseDamage);
        const f32_t eDamagePerRank = ResolveIreliaSkillEffectParam(
            ctx, eSkillSlot::E, eSkillEffectParamId::DamagePerRank, kIreliaEDamagePerRank);

        world.ForEach<ChampionComponent, TransformComponent>(
            std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                [&](EntityID target, ChampionComponent& champion, TransformComponent& tf)
                {
                    if (champion.team == ctx.casterTeam)
                        return;

                    const Vec3 pos = tf.GetPosition();
                    f32_t u = ((pos.x - a.x) * dx + (pos.z - a.z) * dz) / segLenSq;
                    if (u < 0.f) u = 0.f;
                    if (u > 1.f) u = 1.f;

                    const Vec3 closest{ a.x + dx * u, pos.y, a.z + dz * u };
                    if (WintersMath::DistanceSqXZ(pos, closest) > beamRadius * beamRadius)
                        return;

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
                }));

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
    void Tick(CWorld& world, const TickContext& tc)
    {
        world.ForEach<IreliaSimComponent, TransformComponent>(
            std::function<void(EntityID, IreliaSimComponent&, TransformComponent&)>(
                [&](EntityID entity, IreliaSimComponent& state, TransformComponent& transform)
                {
                    if (state.bDashActive)
                    {
                        ClearMove(world, entity);

                        const f32_t duration =
                            state.dashDurationSec > 0.01f ? state.dashDurationSec : kIreliaQDashDurationSec;
                        state.dashElapsedSec += tc.fDt;

                        f32_t t = state.dashElapsedSec / duration;
                        if (t < 0.f) t = 0.f;
                        if (t > 1.f) t = 1.f;

                        const Vec3 desiredPos = Lerp(state.dashStartPos, state.dashEndPos, t);
                        Vec3 guardedPos = desiredPos;
                        bool_t bDashBlocked = false;
                        if (tc.pWalkable)
                        {
                            const Vec3 currentPos = transform.GetLocalPosition();
                            if (!tc.pWalkable->TryClampMoveSegmentXZ(currentPos, desiredPos, 0.5f, guardedPos))
                            {
                                guardedPos = currentPos;
                                bDashBlocked = true;
                            }
                            else if (WintersMath::DistanceSqXZ(guardedPos, desiredPos) > 0.0001f)
                            {
                                bDashBlocked = true;
                            }
                        }

                        transform.SetPosition(guardedPos);

                        const Vec3 dir = WintersMath::NormalizeXZ(Vec3{
                            state.dashEndPos.x - state.dashStartPos.x,
                            0.f,
                            state.dashEndPos.z - state.dashStartPos.z
                        });
                        if (dir.x * dir.x + dir.z * dir.z > 0.0001f)
                        {
                            Vec3 rot = transform.GetRotation();
                            transform.SetRotation({
                                rot.x,
                                ResolveChampionVisualYawNear(eChampion::IRELIA, dir, rot.y),
                                rot.z });
                        }

                        if (t >= 1.f || bDashBlocked)
                        {
                            state.bDashActive = false;
                            state.dashElapsedSec = 0.f;
                            state.dashDurationSec = 0.f;
                            state.dashTarget = NULL_ENTITY;
                        }
                    }

                    TickRWave(world, tc, entity, state);
                }));
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
