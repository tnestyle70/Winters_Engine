#include "GameObject/Champion/Irelia/Irelia_Skills.h"

#include "ECS/World.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "GameObject/Champion/Irelia/IreliaBladeSystem.h"
#include "GameObject/Champion/Irelia/IreliaFxPresets.h"
#include "GameObject/Champion/Irelia/Irelia_Tuning.h"
#include "GameObject/FX/FxBeamComponent.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxCuePlayer.h"
#include "GameObject/FX/FxMeshComponent.h"
#include "GameObject/FX/FxRibbonComponent.h"
#include "GamePlay/VisualHookRegistry.h"
#include "Shared/GameSim/Champions/Irelia/IreliaGameSim.h"
#include "Client/Private/Data/LoLVisualDefinitionPack.h"

#include <Windows.h>
#include <cmath>
#include <cstdio>
#include <functional>
#include <unordered_map>
#include <vector>

namespace
{
    struct IreliaLocalState
    {
        EntityID sword1Id = NULL_ENTITY;
        EntityID sword2Id = NULL_ENTITY;
        std::vector<EntityID> wHoldCueIds{};
        std::vector<EntityID> wAimCueIds{};
        bool_t bWHoldCueActive = false;
        f32_t beamDelay = 0.f;
        bool_t bBeamSpawned = false;
        f32_t eSword1Elapsed = 0.f;
        bool_t bEAutoSecondPending = false;
        std::vector<EntityID> rPulseCueIds{};
    };

    std::unordered_map<EntityID, IreliaLocalState> s_stateByCaster;

    IreliaLocalState& GetState(EntityID caster)
    {
        return s_stateByCaster[caster];
    }

    Vec3 ResolveForward(CWorld& world, EntityID casterEntity, const CastSkillCommand* pCommand)
    {
        if (pCommand)
        {
            const f32_t lenSq =
                pCommand->direction.x * pCommand->direction.x +
                pCommand->direction.z * pCommand->direction.z;
            if (lenSq > 1e-4f)
            {
                const f32_t invLen = 1.f / std::sqrtf(lenSq);
                return {
                    pCommand->direction.x * invLen,
                    0.f,
                    pCommand->direction.z * invLen
                };
            }
        }

        if (world.HasComponent<TransformComponent>(casterEntity))
        {
            const f32_t yaw =
                world.GetComponent<TransformComponent>(casterEntity).GetRotation().y -
                ClientData::ResolveChampionModelYawOffset(eChampion::IRELIA);
            return { std::sinf(yaw), 0.f, std::cosf(yaw) };
        }

        return { 0.f, 0.f, -1.f };
    }

    Vec3 ResolveOrigin(CWorld& world, EntityID casterEntity)
    {
        if (world.HasComponent<TransformComponent>(casterEntity))
            return world.GetComponent<TransformComponent>(casterEntity).GetPosition();

        return {};
    }

    Vec3 ResolveQForward(CWorld& world, EntityID casterEntity,
        const CastSkillCommand* pCommand)
    {
        if (pCommand &&
            pCommand->targetEntityId != NULL_ENTITY &&
            world.HasComponent<TransformComponent>(casterEntity) &&
            world.HasComponent<TransformComponent>(pCommand->targetEntityId))
        {
            const Vec3 casterPos =
                world.GetComponent<TransformComponent>(casterEntity).GetPosition();
            const Vec3 targetPos =
                world.GetComponent<TransformComponent>(pCommand->targetEntityId).GetPosition();

            const Vec3 dir = WintersMath::NormalizeXZOrZero(Vec3{
                targetPos.x - casterPos.x,
                0.f,
                targetPos.z - casterPos.z
            });
            if (dir.x != 0.f || dir.z != 0.f)
                return dir;
        }
        return ResolveForward(world, casterEntity, pCommand);
    }

    void PlayQLeadingEdgeCue(CWorld& world, EntityID casterEntity, const CastSkillCommand* pCommand)
    {
        FxCueContext fx{};
        fx.attachTo = casterEntity;
        fx.vWorldPos = ResolveOrigin(world, casterEntity);
        fx.vForward = ResolveQForward(world, casterEntity, pCommand);

        CFxCuePlayer::Play(world, "Irelia.Q.LeadingEdge", fx);
    }
}

namespace Irelia
{
    //E Blade ??젣 ?ы띁
    constexpr const f32_t kESecondAutoDelay = 2.f;
    constexpr const f32_t kEBladeExpireDelay = 0.5f;

    void ExpireCurrentEBlades(CWorld& world, IreliaLocalState& state)
    {
        CIreliaBladeSystem::ExpireAfter(world, state.sword1Id, kEBladeExpireDelay);
        CIreliaBladeSystem::ExpireAfter(world, state.sword2Id, kEBladeExpireDelay);
    }

    void MarkBillboardPendingDelete(CWorld& world, EntityID fxId)
    {
        if (fxId != NULL_ENTITY && world.HasComponent<FxBillboardComponent>(fxId))
            world.GetComponent<FxBillboardComponent>(fxId).bPendingDelete = true;
    }

    void MarkBeamPendingDelete(CWorld& world, EntityID fxId)
    {
        if (fxId != NULL_ENTITY && world.HasComponent<FxBeamComponent>(fxId))
            world.GetComponent<FxBeamComponent>(fxId).bPendingDelete = true;
    }

    void MarkMeshPendingDelete(CWorld& world, EntityID fxId)
    {
        if (fxId != NULL_ENTITY && world.HasComponent<FxMeshComponent>(fxId))
            world.GetComponent<FxMeshComponent>(fxId).bPendingDelete = true;
    }

    void MarkRibbonPendingDelete(CWorld& world, EntityID fxId)
    {
        if (fxId != NULL_ENTITY && world.HasComponent<FxRibbonComponent>(fxId))
            world.GetComponent<FxRibbonComponent>(fxId).bPendingDelete = true;
    }

    void ClearFxIdList(CWorld& world, std::vector<EntityID>& ids)
    {
        for (EntityID fxId : ids)
        {
            MarkBillboardPendingDelete(world, fxId);
            MarkBeamPendingDelete(world, fxId);
            MarkMeshPendingDelete(world, fxId);
            MarkRibbonPendingDelete(world, fxId);
        }
        ids.clear();
    }

    void ClearWHoldFx(CWorld& world, IreliaLocalState& state)
    {
        ClearFxIdList(world, state.wHoldCueIds);
        ClearFxIdList(world, state.wAimCueIds);
        state.bWHoldCueActive = false;
    }

    void ClearRPulseFx(CWorld& world, IreliaLocalState& state)
    {
        ClearFxIdList(world, state.rPulseCueIds);
    }

    Vec3 ResolveWAimEnd(const Vec3& origin, const Vec3& cursorGround, f32_t range)
    {
        Vec3 dir = WintersMath::DirectionXZ(origin, cursorGround, Vec3{ 0.f, 0.f, 1.f });
        return {
            origin.x + dir.x * range,
            origin.y,
            origin.z + dir.z * range
        };
    }

    bool_t PlayWHoldCue(CWorld& world, EntityID caster, IreliaLocalState& state,
        f32_t lifetime, const Vec3& forward)
    {
        FxCueContext hold{};
        hold.attachTo = caster;
        hold.vWorldPos = ResolveOrigin(world, caster);
        hold.vForward = forward;
        hold.bOverrideLifetime = true;
        hold.fLifetimeOverride = lifetime;

        const bool_t bPlayed =
            CFxCuePlayer::PlayAll(world, "Irelia.W.Spin", hold, &state.wHoldCueIds) != NULL_ENTITY;
        state.bWHoldCueActive = bPlayed;
        return bPlayed;
    }

    bool_t PlayWReleaseCue(CWorld& world, EntityID caster, const Vec3& forward)
    {
        const IreliaTuning& t = GetTuning();
        Vec3 start = ResolveOrigin(world, caster);
        start.y += t.fWReleaseYOffset;

        FxCueContext release{};
        release.vWorldPos = start;
        release.vEndWorldPos = {
            start.x + forward.x * t.fWReleaseRange,
            start.y,
            start.z + forward.z * t.fWReleaseRange
        };
        release.vForward = forward;
        release.bOverrideEndWorldPos = true;
        release.bOverrideLifetime = true;
        release.fLifetimeOverride = t.wLayerLifetime;

        return CFxCuePlayer::Play(world, "Irelia.W.Stage2Slash", release) != NULL_ENTITY;
    }

    bool_t PlayTargetMarkCue(CWorld& world, EntityID target, f32_t lifetime)
    {
        if (target == NULL_ENTITY || !world.HasComponent<TransformComponent>(target))
            return false;

        FxCueContext mark{};
        mark.attachTo = target;
        mark.vWorldPos = world.GetComponent<TransformComponent>(target).GetPosition();
        mark.bOverrideLifetime = true;
        mark.fLifetimeOverride = lifetime;

        return CFxCuePlayer::PlayAll(world, "Irelia.Target.Mark", mark, nullptr) != NULL_ENTITY;
    }

    void ApplyEConnectHitTargets(CWorld& world,
        EntityID casterEntity,
        eTeam casterTeam,
        const Vec3& p1,
        const Vec3& p2,
        f32_t hitRadius,
        f32_t stunSeconds,
        bool_t bApplyLocalGameplay)
    {
        const f32_t dx = p2.x - p1.x;
        const f32_t dz = p2.z - p1.z;
        const f32_t segLenSq = dx * dx + dz * dz + 1e-6f;
        const f32_t hitRadiusSq = hitRadius * hitRadius;

        world.ForEach<ChampionComponent, TransformComponent>(
            std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                [&](EntityID target, ChampionComponent& cc, TransformComponent& ttf)
                {
                    if (target == casterEntity || cc.team == casterTeam)
                        return;

                    const f32_t tdx = ttf.m_LocalPosition.x - p1.x;
                    const f32_t tdz = ttf.m_LocalPosition.z - p1.z;
                    f32_t u = (tdx * dx + tdz * dz) / segLenSq;
                    if (u < 0.f)
                        u = 0.f;
                    else if (u > 1.f)
                        u = 1.f;

                    const f32_t px = p1.x + dx * u;
                    const f32_t pz = p1.z + dz * u;
                    const f32_t ex = ttf.m_LocalPosition.x - px;
                    const f32_t ez = ttf.m_LocalPosition.z - pz;
                    if (ex * ex + ez * ez >= hitRadiusSq)
                        return;

                    if (bApplyLocalGameplay)
                    {
                        StunComponent stun{};
                        stun.fRemaining = stunSeconds;
                        stun.sourceEntity = casterEntity;
                        if (world.HasComponent<StunComponent>(target))
                            world.GetComponent<StunComponent>(target) = stun;
                        else
                            world.AddComponent<StunComponent>(target, stun);
                    }

                    PlayTargetMarkCue(world, target, stunSeconds);
                }));
    }

    bool_t PlayRPulseCue(CWorld& world, const Vec3& origin, const Vec3& forward,
        f32_t speed, f32_t lifetime,
        f32_t width, f32_t height,
        f32_t yOffset, f32_t fwdOffset, f32_t yawOffset,
        std::vector<EntityID>* pOutSpawned)
    {
        (void)lifetime;
        (void)width;
        (void)height;
        (void)yOffset;
        (void)fwdOffset;
        (void)yawOffset;

        FxCueContext fx{};
        fx.vWorldPos = origin;
        fx.vForward = forward;
        fx.bOverrideVelocity = true;
        fx.vVelocity = { forward.x * speed, 0.f, forward.z * speed };

        return CFxCuePlayer::PlayAll(world, "Irelia.R.Pulse", fx, pOutSpawned) != NULL_ENTITY;
    }

    bool_t PlayRHitCue(CWorld& world, const Vec3& hitPos,
        const Vec3& forward, f32_t lifetime)
    {
        (void)lifetime;

        FxCueContext fx{};
        fx.vWorldPos = hitPos;
        fx.vForward = forward;

        return CFxCuePlayer::Play(world, "Irelia.R.Hit", fx) != NULL_ENTITY;
    }

    bool_t SpawnRWallCue(CWorld& world, const Vec3& wallPos, const Vec3& forward)
    {
        FxCueContext fx{};
        fx.vWorldPos = wallPos;
        fx.vWorldPos.y += 0.02f;
        fx.vForward = forward;

        return CFxCuePlayer::Play(world, "Irelia.R.Wall", fx) != NULL_ENTITY;
    }

    void UpdateWAimCue(CWorld& world, IreliaLocalState& state,
        EntityID caster, const Vec3& cursorGround)
    {
        if (!state.bWHoldCueActive ||
            caster == NULL_ENTITY ||
            !world.HasComponent<TransformComponent>(caster))
        {
            return;
        }

        const IreliaTuning& t = GetTuning();
        Vec3 start = world.GetComponent<TransformComponent>(caster).GetPosition();
        start.y += t.fWAimYOffset;
        const Vec3 end = ResolveWAimEnd(start, cursorGround, t.fWAimRange);

        for (EntityID fxId : state.wAimCueIds)
        {
            if (!world.HasComponent<FxBeamComponent>(fxId))
                continue;

            FxBeamComponent& beam = world.GetComponent<FxBeamComponent>(fxId);
            const Vec3 dir = WintersMath::DirectionXZ(start, end, Vec3{ 0.f, 0.f, 1.f });
            const Vec3 right{ dir.z, 0.f, -dir.x };
            const Vec3 cueOffset{
                right.x * beam.vStartOffset.x + dir.x * beam.vStartOffset.z,
                beam.vStartOffset.y,
                right.z * beam.vStartOffset.x + dir.z * beam.vStartOffset.z
            };
            beam.vStartWorldPos = {
                start.x + cueOffset.x,
                start.y + cueOffset.y,
                start.z + cueOffset.z
            };
            beam.vEndWorldPos = {
                end.x + cueOffset.x,
                end.y + cueOffset.y,
                end.z + cueOffset.z
            };
            beam.vVelocity = {};
        }
    }

    void ResetLocalState()
    {
        s_stateByCaster.clear();
    }

    bool_t PlayEConnectCue(
        CWorld& world,
        Engine::CFxStaticMeshRenderer* pFxMeshRenderer,
        const Vec3& p1,
        const Vec3& p2,
        const IreliaTuning& t)
    {
        const Vec3 forward = WintersMath::NormalizeXZOrZero(Vec3{
            p2.x - p1.x, 0.f, p2.z - p1.z });

        FxCueContext fx{};
        fx.vWorldPos = p1;
        fx.vEndWorldPos = p2;
        fx.vForward = forward;
        fx.pFxMeshRenderer = pFxMeshRenderer;
        fx.bOverrideEndWorldPos = true;
        fx.bOverrideLifetime = true;
        fx.fLifetimeOverride = t.fEConnectLifetime;

        if (CFxCuePlayer::Play(world, "Irelia.E.Connect", fx) == NULL_ENTITY)
            return false;

        FxCueContext pop = fx;
        pop.bOverrideEndWorldPos = false;
        pop.bOverrideLifetime = false;
        pop.vWorldPos = p1;
        CFxCuePlayer::Play(world, "Irelia.E.ConnectPop", pop);
        pop.vWorldPos = p2;
        CFxCuePlayer::Play(world, "Irelia.E.ConnectPop", pop);
        return true;
    }

    void OnCastAccepted_Q(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand)
            return;

        if (!ctx.startPointDash)
        {
            const f32_t duration = ctx.pDef ? ctx.pDef->lockDurationSec : 0.3f;
            IreliaFx::SpawnQTrail(*ctx.pWorld, ctx.casterEntity, duration);
            PlayQLeadingEdgeCue(*ctx.pWorld, ctx.casterEntity, ctx.pCommand);
            return;
        }

        const EntityID target = ctx.pCommand->targetEntityId;
        if (target == NULL_ENTITY)
            return;
        if (!ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity) ||
            !ctx.pWorld->HasComponent<TransformComponent>(target))
        {
            return;
        }

        const Vec3 pStart =
            ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        const Vec3 pTarget =
            ctx.pWorld->GetComponent<TransformComponent>(target).GetPosition();
        constexpr f32_t kQDashStopGap = 1.35f;
        const Vec3 pEnd = IreliaGameSim::ResolveQDashEndPos(pStart, pTarget, kQDashStopGap);

        const f32_t duration = ctx.getLocalDashDuration ? ctx.getLocalDashDuration() : 0.3f;
        ctx.startPointDash(pStart, pEnd, duration, target);
        IreliaFx::SpawnQTrail(*ctx.pWorld, ctx.casterEntity, duration);
        PlayQLeadingEdgeCue(*ctx.pWorld, ctx.casterEntity, ctx.pCommand);

        char dbg[192]{};
        sprintf_s(dbg,
            "[Irelia Q Dash] start (%.1f,%.1f,%.1f) -> (%.1f,%.1f,%.1f) dur=%.2fs target=%u\n",
            pStart.x, pStart.y, pStart.z,
            pEnd.x, pEnd.y, pEnd.z,
            duration, target);
    }

    void OnCastAccepted_W(SkillHookContext& ctx)
    {
        if (!ctx.pWorld)
            return;

        IreliaLocalState& state = GetState(ctx.casterEntity);
        const Vec3 forward = ResolveForward(*ctx.pWorld, ctx.casterEntity, ctx.pCommand);
        if (ctx.skillStage >= 2)
        {
            ClearWHoldFx(*ctx.pWorld, state);
            PlayWReleaseCue(*ctx.pWorld, ctx.casterEntity, forward);
            return;
        }

        ClearWHoldFx(*ctx.pWorld, state);
        const f32_t lifetime =
            (ctx.pDef && ctx.pDef->stageWindowSec > 0.f) ? ctx.pDef->stageWindowSec + 0.5f : 4.5f;
        PlayWHoldCue(*ctx.pWorld, ctx.casterEntity, state, lifetime, forward);
    }

    void OnCastAccepted_E(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand || !ctx.pFxMeshRenderer)
            return;

        const IreliaTuning& t = GetTuning();
        IreliaLocalState& state = GetState(ctx.casterEntity);
        const Vec3 rotation{ t.bladePitch, t.bladeYaw, t.bladeRoll };
        const EntityID bladeId = CIreliaBladeSystem::SpawnPlaced(
            *ctx.pWorld,
            ctx.pFxMeshRenderer,
            ctx.pCommand->groundPos,
            ctx.casterEntity,
            t.bladeScale,
            rotation,
            t.bladeSpinSpeed);
        //stage2 ??寃쎌슦 ??젣 ?덉빟
        if (ctx.skillStage >= 2)
        {
            state.sword2Id = bladeId;
            state.bEAutoSecondPending = false;
            state.eSword1Elapsed = 0.f;
            if (state.sword1Id != NULL_ENTITY &&
                state.sword2Id != NULL_ENTITY &&
                !state.bBeamSpawned &&
                ctx.pWorld->HasComponent<IreliaBladeComponent>(state.sword1Id) &&
                ctx.pWorld->HasComponent<IreliaBladeComponent>(state.sword2Id))
            {
                const Vec3 p1 =
                    ctx.pWorld->GetComponent<IreliaBladeComponent>(state.sword1Id).vWorldPos;
                const Vec3 p2 =
                    ctx.pWorld->GetComponent<IreliaBladeComponent>(state.sword2Id).vWorldPos;
                PlayEConnectCue(*ctx.pWorld, ctx.pFxMeshRenderer, p1, p2, t);
                ApplyEConnectHitTargets(*ctx.pWorld,
                    ctx.casterEntity,
                    ctx.casterTeam,
                    p1,
                    p2,
                    1.5f,
                    t.bladeStunSec,
                    ctx.applyTargetDamage ? true : false);
                state.bBeamSpawned = true;
            }
            ExpireCurrentEBlades(*ctx.pWorld, state);
        }
        else
        {
            state.sword1Id = bladeId;
            state.sword2Id = NULL_ENTITY;
            state.beamDelay = 0.f;
            state.bBeamSpawned = false;
            state.eSword1Elapsed = 0.f;
            state.bEAutoSecondPending = (bladeId != NULL_ENTITY);
        }
    }

    namespace Visual
    {
        SkillHookContext BuildSkillContextFromVisual(VisualHookContext& ctx)
        {
            SkillHookContext skillCtx{};
            skillCtx.pWorld = ctx.pWorld;
            skillCtx.casterEntity = ctx.casterEntity;
            if (ctx.pWorld
                && ctx.casterEntity != NULL_ENTITY
                && ctx.pWorld->HasComponent<ChampionComponent>(ctx.casterEntity))
            {
                skillCtx.casterTeam =
                    ctx.pWorld->GetComponent<ChampionComponent>(ctx.casterEntity).team;
            }
            skillCtx.pDef = ctx.pDef;
            skillCtx.pCommand = ctx.pCommand;
            skillCtx.skillStage = ctx.skillStage;
            skillCtx.pFxMeshRenderer = ctx.pFxMeshRenderer;

            return skillCtx;
        }

        void OnCastAccepted_Q_Visual(VisualHookContext& ctx)
        {
            SkillHookContext skillCtx = BuildSkillContextFromVisual(ctx);
            OnCastAccepted_Q(skillCtx);
        }

        void OnCastAccepted_W_Visual(VisualHookContext& ctx)
        {
            SkillHookContext skillCtx = BuildSkillContextFromVisual(ctx);
            OnCastAccepted_W(skillCtx);
        }

        void OnCastAccepted_E_Visual(VisualHookContext& ctx)
        {
            SkillHookContext skillCtx = BuildSkillContextFromVisual(ctx);
            OnCastAccepted_E(skillCtx);
        }

        void OnCastAccepted_R_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld)
                return;

            const IreliaTuning& t = GetTuning();
            const Vec3 forward = ResolveForward(*ctx.pWorld, ctx.casterEntity, ctx.pCommand);
            const Vec3 eventPos = ctx.pCommand
                ? ctx.pCommand->groundPos
                : ResolveOrigin(*ctx.pWorld, ctx.casterEntity);
            IreliaLocalState& state = GetState(ctx.casterEntity);

            if (ctx.skillStage <= 1)
            {
                const Vec3 origin = ResolveOrigin(*ctx.pWorld, ctx.casterEntity);
                const f32_t rSpeed = (t.waveSpeed > 1.f) ? t.waveSpeed : 25.f;
                const f32_t lifetime = t.waveMaxDist / rSpeed + 0.1f;

                ClearRPulseFx(*ctx.pWorld, state);
                PlayRPulseCue(*ctx.pWorld,
                    origin,
                    forward,
                    rSpeed,
                    lifetime,
                    t.rFxWidth,
                    t.rFxHeight,
                    t.rFxYOffset,
                    t.rFxFwdOffset,
                    t.rFxYawOffset,
                    &state.rPulseCueIds);
                return;
            }

            if (ctx.skillStage == 2)
            {
                ClearRPulseFx(*ctx.pWorld, state);
                PlayTargetMarkCue(*ctx.pWorld,
                    ctx.pCommand ? ctx.pCommand->targetEntityId : NULL_ENTITY,
                    1.5f);
                PlayRHitCue(*ctx.pWorld, eventPos, forward, 0.45f);
                SpawnRWallCue(*ctx.pWorld, eventPos, forward);

                const Vec3 bladeRot{ t.bladePitch, t.bladeYaw, t.bladeRoll };
                IreliaFx::SpawnRBladeFan(*ctx.pWorld,
                    ctx.pFxMeshRenderer,
                    eventPos,
                    forward,
                    t.iRWallBladeCount,
                    t.fRWallBladeSpreadRad,
                    t.fRWallBladePlaceDist,
                    t.fRWallBladeLifetime,
                    t.bladeScale * t.fRWallBladeScaleMul,
                    bladeRot,
                    t.bRTriangleMode,
                    t.rTipBoost,
                    t.rSideShrink);
                return;
            }

            if (ctx.skillStage == 3)
            {
                ClearRPulseFx(*ctx.pWorld, state);
                SpawnRWallCue(*ctx.pWorld, eventPos, forward);
                return;
            }

            if (ctx.skillStage == 4)
            {
                PlayTargetMarkCue(*ctx.pWorld,
                    ctx.pCommand ? ctx.pCommand->targetEntityId : NULL_ENTITY,
                    1.5f);
            }
        }
    }

    void UpdateLocalBladeState(
        CWorld& world,
        Engine::CFxStaticMeshRenderer* pFxMeshRenderer,
        EntityID casterEntity,
        eTeam casterTeam,
        f32_t fDeltaTime,
        const Vec3& vCursorGround,
        bool_t bApplyLocalGameplay)
    {
        IreliaLocalState& state = GetState(casterEntity);
        if (state.sword1Id != NULL_ENTITY && !world.IsAlive(state.sword1Id))
            state.sword1Id = NULL_ENTITY;
        if (state.sword2Id != NULL_ENTITY && !world.IsAlive(state.sword2Id))
            state.sword2Id = NULL_ENTITY;

        UpdateWAimCue(world, state, casterEntity, vCursorGround);

        //stale sword ?뺣━ 吏곹썑, beam ?앹꽦 濡쒖쭅 ?꾩뿉 異붽?
        if (state.sword1Id == NULL_ENTITY)
        {
            state.bEAutoSecondPending = false;
            state.eSword1Elapsed = 0.f;
        }

        if (state.bEAutoSecondPending
            && state.sword1Id != NULL_ENTITY
            && state.sword2Id == NULL_ENTITY
            && pFxMeshRenderer)
        {
            state.eSword1Elapsed += fDeltaTime;
            if (state.eSword1Elapsed >= kESecondAutoDelay)
            {
                const IreliaTuning& t = GetTuning();
                const Vec3 rotation{ t.bladePitch, t.bladeYaw, t.bladeRoll };

                state.sword2Id = CIreliaBladeSystem::SpawnPlaced(
                    world,
                    pFxMeshRenderer,
                    vCursorGround,
                    casterEntity,
                    t.bladeScale,
                    rotation,
                    t.bladeSpinSpeed);

                state.bEAutoSecondPending = false;
                state.eSword1Elapsed = 0.f;
                ExpireCurrentEBlades(world, state);
            }
        }

        if (state.sword1Id != NULL_ENTITY
            && state.sword2Id != NULL_ENTITY
            && !state.bBeamSpawned)
        {
            state.beamDelay += fDeltaTime;
            if (state.beamDelay >= 0.1f)
            {
                if (world.HasComponent<IreliaBladeComponent>(state.sword1Id)
                    && world.HasComponent<IreliaBladeComponent>(state.sword2Id))
                {
                    const IreliaTuning& t = GetTuning();
                    const Vec3 p1 = world.GetComponent<IreliaBladeComponent>(state.sword1Id).vWorldPos;
                    const Vec3 p2 = world.GetComponent<IreliaBladeComponent>(state.sword2Id).vWorldPos;
                    PlayEConnectCue(world, pFxMeshRenderer, p1, p2, t);
                    ApplyEConnectHitTargets(world,
                        casterEntity,
                        casterTeam,
                        p1,
                        p2,
                        1.5f,
                        t.bladeStunSec,
                        bApplyLocalGameplay);
                }
                state.bBeamSpawned = true;
            }
        }

        if (state.sword1Id == NULL_ENTITY && state.sword2Id == NULL_ENTITY)
        {
            state.beamDelay = 0.f;
            state.bBeamSpawned = false;
        }
    }
}
