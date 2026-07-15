#include "GameObject/Champion/Yasuo/Yasuo_Skills.h"

#include "ECS/World.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "GameObject/Champion/Yasuo/PendingHitSystem.h"
#include "GameObject/Champion/Yasuo/Yasuo_Tuning.h"
#include "GameObject/Champion/Yasuo/YasuoFxPresets.h"
#include "GameObject/FX/WindWallSystem.h"
#include "Client/Private/Data/LoLVisualDefinitionPack.h"

#include <cmath>
#include <cstdio>
#include <Windows.h>

namespace
{
    Vec3 ResolveCasterPosition(CWorld& world, EntityID caster)
    {
        if (world.HasComponent<TransformComponent>(caster))
            return world.GetComponent<TransformComponent>(caster).GetPosition();

        return Vec3{ 0.f, 0.f, 0.f };
    }

    Vec3 ResolveCasterForward(CWorld& world, EntityID caster, const CastSkillCommand* pCommand)
    {
        if (pCommand)
        {
            const Vec3 dir = WintersMath::NormalizeXZOrZero(pCommand->direction);
            if (dir.x != 0.f || dir.z != 0.f)
                return dir;
        }

        if (world.HasComponent<TransformComponent>(caster))
        {
            const f32_t yaw = world.GetComponent<TransformComponent>(caster).GetRotation().y -
                ClientData::ResolveChampionModelYawOffset(eChampion::YASUO);
            return WintersMath::NormalizeXZ(Vec3{ std::sinf(yaw), 0.f, std::cosf(yaw) });
        }

        return Vec3{ 0.f, 0.f, -1.f };
    }

    Vec3 ResolveEDashForward(CWorld& world, EntityID caster,
        const CastSkillCommand* pCommand)
    {
        if (pCommand)
        {
            const Vec3 commandDirection =
                WintersMath::NormalizeXZOrZero(pCommand->direction);
            if (commandDirection.x != 0.f || commandDirection.z != 0.f)
                return commandDirection;
        }

        const Vec3 fallback = ResolveCasterForward(world, caster, pCommand);
        if (!pCommand ||
            pCommand->targetEntityId == NULL_ENTITY ||
            !world.HasComponent<TransformComponent>(caster) ||
            !world.HasComponent<TransformComponent>(pCommand->targetEntityId))
        {
            return fallback;
        }

        const Vec3 casterPos =
            world.GetComponent<TransformComponent>(caster).GetPosition();
        const Vec3 targetPos = world.GetComponent<TransformComponent>(
            pCommand->targetEntityId).GetPosition();
        return WintersMath::DirectionXZ(casterPos, targetPos, fallback);
    }
}

namespace Yasuo
{
    void OnCastAccepted_Q(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand)
            return;
        if (!ctx.pWorld->HasComponent<YasuoStateComponent>(ctx.casterEntity))
            return;

        CWorld& world = *ctx.pWorld;
        auto& ys = world.GetComponent<YasuoStateComponent>(ctx.casterEntity);
        const YasuoTuning& tuning = GetTuning();
        const Vec3 origin = ResolveCasterPosition(world, ctx.casterEntity);
        const Vec3 forward = ResolveCasterForward(world, ctx.casterEntity, ctx.pCommand);

        if (ys.bEActive)
        {
            YasuoFx::SpawnEQRing(world, ctx.casterEntity, origin,
                0.6f, tuning.eqRadius);
            CPendingHitSystem::Schedule(world,
                ctx.casterEntity, ctx.casterTeam,
                forward, tuning.eqDelay,
                eProjectileKind::EQRing,
                0.f, 0.f, tuning.eqRadius,
                tuning.eqDamage, 0.f);
        }
        else if (ys.qStackCount >= 2)
        {
            YasuoFx::SpawnQTornado(world,
                origin, forward,
                tuning.qTornadoSpeed, tuning.qTornadoLifetime);
            CPendingHitSystem::Schedule(world,
                ctx.casterEntity, ctx.casterTeam,
                forward,
                tuning.qHitDelay,
                eProjectileKind::Tornado,
                tuning.qTornadoSpeed,
                tuning.qTornadoSpeed * tuning.qTornadoLifetime,
                1.5f,
                tuning.qTornadoDamage, tuning.qTornadoStunSec);
            ys.qStackCount = 0;
            ys.qStackTimer = 0.f;
        }
        else
        {
            YasuoFx::SpawnQStraight(world, origin,
                forward, tuning.qLifetime);
            YasuoFx::SpawnQBuildUp(world, ctx.casterEntity, 0.3f);
            CPendingHitSystem::Schedule(world,
                ctx.casterEntity, ctx.casterTeam,
                forward,
                tuning.qHitDelay,
                eProjectileKind::Wind,
                tuning.qSpeed,
                tuning.qSpeed * tuning.qLifetime,
                0.8f,
                tuning.qDamage, 0.f);
            ys.qStackCount += 1;
            ys.qStackTimer = 6.f;
        }
    }

    void OnCastAccepted_W(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand)
            return;

        CWorld& world = *ctx.pWorld;
        const YasuoTuning& tuning = GetTuning();
        const Vec3 origin = ResolveCasterPosition(world, ctx.casterEntity);
        const Vec3 forward = ResolveCasterForward(world, ctx.casterEntity, ctx.pCommand);

        YasuoFx::SpawnWWindWall(world, ctx.pFxMeshRenderer,
            origin, forward, tuning.wLifetime, tuning.wWidth, tuning.wHeight,
            tuning.wMeshScale);
        CWindWallSystem::Spawn(world,
            origin, forward, tuning.wLifetime, tuning.wWidth, tuning.wHeight,
            ctx.casterEntity, eTeam::Neutral);
    }

    void OnCastAccepted_E(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand)
            return;
        if (!ctx.pWorld->HasComponent<YasuoStateComponent>(ctx.casterEntity))
            return;

        const EntityID target = ctx.pCommand->targetEntityId;
        if (target == NULL_ENTITY || !ctx.pWorld->HasComponent<TransformComponent>(target))
            return;

        auto& ys = ctx.pWorld->GetComponent<YasuoStateComponent>(ctx.casterEntity);
        ys.bEActive = true;
        ys.eActiveTimer = 0.5f;

        const Vec3 dashForward = ResolveEDashForward(
            *ctx.pWorld, ctx.casterEntity, ctx.pCommand);

        if (ctx.startTargetDash)
            ctx.startTargetDash(target);

        YasuoFx::SpawnEDashTrail(
            *ctx.pWorld,
            ctx.pFxMeshRenderer,
            ctx.casterEntity,
            dashForward,
            GetTuning().eDashDuration);
        YasuoFx::SpawnEDashTargetRing(
            *ctx.pWorld,
            target,
            GetTuning().eTargetReuseDuration);
    }

    void OnCastAccepted_R(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.findAirborneTarget)
            return;

        CWorld& world = *ctx.pWorld;
        const YasuoTuning& tuning = GetTuning();
        const Vec3 origin = ResolveCasterPosition(world, ctx.casterEntity);
        const EntityID airborne = ctx.findAirborneTarget(origin, tuning.rSearchRadius);
        if (airborne == NULL_ENTITY || !world.HasComponent<TransformComponent>(airborne))
        {
            return;
        }

        if (ctx.startUltimateDash)
            ctx.startUltimateDash(airborne);

        const Vec3 vLandPos = world.GetComponent<TransformComponent>(airborne).m_LocalPosition;
        YasuoFx::SpawnRLastBreath(world,
            vLandPos, ctx.casterEntity, tuning.rSequenceDuration);
    }
}

namespace Yasuo::Visual
{
    void OnPassiveShield_Visual(VisualHookContext& ctx)
    {
        if (!ctx.pWorld)
            return;

        YasuoFx::SpawnPassiveShield(
            *ctx.pWorld,
            ctx.pFxMeshRenderer,
            ctx.casterEntity,
            3.0f);
    }

    void OnCastAccepted_Q_Visual(VisualHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand)
            return;

        CWorld& world = *ctx.pWorld;
        const YasuoTuning& tuning = GetTuning();
        const Vec3 origin = ResolveCasterPosition(world, ctx.casterEntity);
        const Vec3 forward = ResolveCasterForward(world, ctx.casterEntity, ctx.pCommand);

        const u8_t stage = ctx.skillStage;
        if (stage == 4)
        {
            YasuoFx::SpawnEQRing(world, ctx.casterEntity, origin, 0.6f, tuning.eqRadius);
        }
        else if (stage == 3)
        {
            YasuoFx::SpawnQTornado(
                world,
                origin,
                forward,
                tuning.qTornadoSpeed,
                tuning.qTornadoLifetime);
        }
        else
        {
            YasuoFx::SpawnQStraight(world, origin, forward, tuning.qLifetime);
            YasuoFx::SpawnQBuildUp(world, ctx.casterEntity, 0.3f);
        }
    }

    void OnCastAccepted_W_Visual(VisualHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand)
            return;
        if (ctx.bAuthoritativeEvent)
            return;

        CWorld& world = *ctx.pWorld;
        const YasuoTuning& tuning = GetTuning();
        const Vec3 origin = ResolveCasterPosition(world, ctx.casterEntity);
        const Vec3 forward = ResolveCasterForward(world, ctx.casterEntity, ctx.pCommand);

        YasuoFx::SpawnWWindWall(
            world,
            ctx.pFxMeshRenderer,
            origin,
            forward,
            tuning.wLifetime,
            tuning.wWidth,
            tuning.wHeight,
            tuning.wMeshScale);
    }

    void OnCastAccepted_E_Visual(VisualHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand)
            return;

        const EntityID target = ctx.pCommand->targetEntityId;
        const Vec3 dashForward = ResolveEDashForward(
            *ctx.pWorld, ctx.casterEntity, ctx.pCommand);
        YasuoFx::SpawnEDashTrail(
            *ctx.pWorld,
            ctx.pFxMeshRenderer,
            ctx.casterEntity,
            dashForward,
            GetTuning().eDashDuration);
        YasuoFx::SpawnEDashTargetRing(
            *ctx.pWorld,
            target,
            GetTuning().eTargetReuseDuration);
    }

    void OnCastAccepted_R_Visual(VisualHookContext& ctx)
    {
        if (!ctx.pWorld)
            return;

        CWorld& world = *ctx.pWorld;
        const YasuoTuning& tuning = GetTuning();
        Vec3 landPos = ResolveCasterPosition(world, ctx.casterEntity);
        if (ctx.pCommand &&
            ctx.pCommand->targetEntityId != NULL_ENTITY &&
            world.HasComponent<TransformComponent>(ctx.pCommand->targetEntityId))
        {
            landPos = world.GetComponent<TransformComponent>(ctx.pCommand->targetEntityId).GetPosition();
        }

        YasuoFx::SpawnRLastBreath(
            world,
            landPos,
            ctx.casterEntity,
            tuning.rSequenceDuration);
    }

    void OnKeySwap_Q(VisualHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pKeyOut)
            return;
        if (!ctx.pWorld->HasComponent<YasuoStateComponent>(ctx.casterEntity))
            return;

        const auto& ys = ctx.pWorld->GetComponent<YasuoStateComponent>(ctx.casterEntity);
        if (ys.bEActive)
            *ctx.pKeyOut = "spell1c";
        else if (ys.qStackCount >= 2)
            *ctx.pKeyOut = "spell1_wind";
        else if (ys.qStackCount == 1)
            *ctx.pKeyOut = "spell1b";
        else
            *ctx.pKeyOut = "spell1a";

        char dbg[96]{};
        sprintf_s(dbg, "[Yasuo Q Anim] stack=%u key=%s\n",
            static_cast<u32_t>(ys.qStackCount), ctx.pKeyOut->c_str());
    }
}
