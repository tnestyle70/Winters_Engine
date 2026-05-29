#include "GameObject/Champion/Yasuo/Yasuo_Skills.h"

#include "ECS/World.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "GameObject/Champion/Yasuo/PendingHitSystem.h"
#include "GameObject/Champion/Yasuo/Yasuo_Tuning.h"
#include "GameObject/Champion/Yasuo/YasuoFxPresets.h"
#include "GameObject/FX/WindWallSystem.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"

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
            const Vec3 dir = WintersMath::NormalizeXZ(pCommand->direction);
            if (dir.x != 0.f || dir.z != -1.f)
                return dir;
        }

        if (world.HasComponent<TransformComponent>(caster))
        {
            const f32_t yaw = world.GetComponent<TransformComponent>(caster).GetRotation().y -
                GetDefaultChampionVisualYawOffset(eChampion::YASUO);
            return WintersMath::NormalizeXZ(Vec3{ std::sinf(yaw), 0.f, std::cosf(yaw) });
        }

        return Vec3{ 0.f, 0.f, -1.f };
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
            YasuoFx::SpawnQTornado(world, ctx.pFxMeshRenderer,
                origin, forward,
                tuning.qTornadoSpeed, tuning.qTornadoLifetime, tuning.qTornadoScale,
                tuning.qTornadoColor);
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
                forward, tuning.qSpeed, tuning.qLifetime);
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

        if (ctx.startTargetDash)
            ctx.startTargetDash(target);

        YasuoFx::SpawnEDashTrail(*ctx.pWorld, ctx.casterEntity, GetTuning().eDashDuration);
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
            ::OutputDebugStringA("[Yasuo R] No airborne (Stun) target - skip\n");
            return;
        }

        if (ctx.startUltimateDash)
            ctx.startUltimateDash(airborne);

        const Vec3 vLandPos = world.GetComponent<TransformComponent>(airborne).m_LocalPosition;
        YasuoFx::SpawnRLastBreath(world, ctx.pFxMeshRenderer,
            vLandPos, ctx.casterEntity, tuning.rSequenceDuration);
    }
}

namespace Yasuo::Visual
{
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
                ctx.pFxMeshRenderer,
                origin,
                forward,
                tuning.qTornadoSpeed,
                tuning.qTornadoLifetime,
                tuning.qTornadoScale,
                tuning.qTornadoColor);
        }
        else
        {
            YasuoFx::SpawnQStraight(world, origin, forward, tuning.qSpeed, tuning.qLifetime);
            YasuoFx::SpawnQBuildUp(world, ctx.casterEntity, 0.3f);
        }
    }

    void OnCastAccepted_W_Visual(VisualHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand)
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
        if (!ctx.pWorld)
            return;

        YasuoFx::SpawnEDashTrail(*ctx.pWorld, ctx.casterEntity, GetTuning().eDashDuration);
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
            ctx.pFxMeshRenderer,
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
        ::OutputDebugStringA(dbg);
    }
}
