#include "GameObject/Champion/Zed/Zed_Skills.h"

#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "GameObject/Champion/Zed/ZedFxPresets.h"
#include "GameObject/FX/FxCuePlayer.h"
#include "Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.h"

#include <cmath>

namespace
{
    Vec3 ResolveEntityPosition(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<TransformComponent>(entity))
            return world.GetComponent<TransformComponent>(entity).GetPosition();

        return Vec3{};
    }

    Vec3 ResolveForward(CWorld& world, EntityID caster)
    {
        if (!world.HasComponent<TransformComponent>(caster))
            return Vec3{ 0.f, 0.f, 1.f };

        const f32_t visualYaw = world.GetComponent<TransformComponent>(caster).GetRotation().y;
        return WintersMath::DirectionFromYawXZ(
            visualYaw - ChampionGameDataDB::ResolveVisualYawOffset(eChampion::ZED));
    }

    Vec3 ResolveCommandDirection(const SkillHookContext& ctx)
    {
        if (ctx.pCommand)
        {
            const Vec3 dir = WintersMath::NormalizeXZ(ctx.pCommand->direction, Vec3{}, 0.0001f);
            if (dir.x != 0.f || dir.z != 0.f)
                return dir;
        }

        if (ctx.pWorld)
            return ResolveForward(*ctx.pWorld, ctx.casterEntity);

        return Vec3{ 0.f, 0.f, 1.f };
    }

    Vec3 ResolveCommandPosition(const SkillHookContext& ctx)
    {
        if (ctx.pCommand &&
            (std::fabs(ctx.pCommand->groundPos.x) +
                std::fabs(ctx.pCommand->groundPos.y) +
                std::fabs(ctx.pCommand->groundPos.z)) > 0.001f)
        {
            return ctx.pCommand->groundPos;
        }

        return ctx.pWorld
            ? ResolveEntityPosition(*ctx.pWorld, ctx.casterEntity)
            : Vec3{};
    }

    void PlayCue(
        SkillHookContext& ctx,
        const char* pszCueName,
        const Vec3& position,
        const Vec3& forward,
        EntityID attachTo,
        f32_t lifetimeSec)
    {
        if (!ctx.pWorld)
            return;

        FxCueContext fx{};
        fx.vWorldPos = position;
        fx.vForward = forward;
        fx.attachTo = attachTo;
        if (lifetimeSec > 0.f)
        {
            fx.bOverrideLifetime = true;
            fx.fLifetimeOverride = lifetimeSec;
        }
        CFxCuePlayer::Play(*ctx.pWorld, pszCueName, fx);
    }
}

namespace Zed
{
    void OnCastFrame(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pDef)
            return;

        const u8_t slot = ctx.pDef->slot;
        const Vec3 forward = ResolveCommandDirection(ctx);
        const Vec3 casterPos = ResolveEntityPosition(*ctx.pWorld, ctx.casterEntity);

        if (slot == static_cast<u8_t>(eSkillSlot::Q))
        {
            PlayCue(ctx, "Zed.Q.Cast", { casterPos.x, casterPos.y + 1.15f, casterPos.z },
                forward, ctx.casterEntity, 0.f);
        }
        else if (slot == static_cast<u8_t>(eSkillSlot::W))
        {
            const f32_t shadowDuration = ctx.pDef->stageWindowSec > 0.f
                ? ctx.pDef->stageWindowSec
                : 5.f;
            if (ctx.skillStage >= 2u)
            {
                const Vec3 shadowPos = ResolveCommandPosition(ctx);
                PlayCue(ctx, "Zed.W.ShadowSpawn", shadowPos, forward, NULL_ENTITY, 0.45f);
                if (!ZedFx::MoveShadowCloneModel(*ctx.pWorld, ctx.casterEntity, shadowPos, forward))
                {
                    ZedFx::SpawnShadowCloneModel(
                        *ctx.pWorld,
                        ctx.casterEntity,
                        shadowPos,
                        forward,
                        shadowDuration);
                }
                return;
            }

            const Vec3 shadowPos = ResolveCommandPosition(ctx);
            PlayCue(ctx, "Zed.W.ShadowSpawn", shadowPos, forward, NULL_ENTITY, shadowDuration);
            ZedFx::SpawnShadowCloneModel(*ctx.pWorld, ctx.casterEntity, shadowPos, forward, shadowDuration);
        }
        else if (slot == static_cast<u8_t>(eSkillSlot::E))
        {
            if (ctx.skillStage >= 2u)
            {
                const Vec3 shadowPos = ResolveCommandPosition(ctx);
                PlayCue(ctx, "Zed.E.Slash", shadowPos, forward, NULL_ENTITY, 0.5f);
            }
            else
            {
                PlayCue(ctx, "Zed.E.Slash", { casterPos.x, casterPos.y + 1.0f, casterPos.z },
                    forward, ctx.casterEntity, 0.5f);
            }
        }
        else if (slot == static_cast<u8_t>(eSkillSlot::R))
        {
            const EntityID target = ctx.pCommand ? ctx.pCommand->targetEntityId : NULL_ENTITY;
            if (ctx.skillStage == 3u)
            {
                const Vec3 shadowPos = ResolveCommandPosition(ctx);
                PlayCue(ctx, "Zed.W.ShadowSpawn", shadowPos, forward, NULL_ENTITY, 5.f);
                ZedFx::SpawnShadowCloneModel(*ctx.pWorld, ctx.casterEntity, shadowPos, forward, 5.f);
            }
            else if (ctx.skillStage >= 2u)
            {
                const Vec3 popPos = target != NULL_ENTITY
                    ? ResolveEntityPosition(*ctx.pWorld, target)
                    : ResolveCommandPosition(ctx);
                PlayCue(ctx, "Zed.R.Pop", { popPos.x, popPos.y + 1.25f, popPos.z },
                    forward, target, 0.8f);
            }
            else if (target != NULL_ENTITY)
            {
                const Vec3 targetPos = ResolveEntityPosition(*ctx.pWorld, target);
                PlayCue(ctx, "Zed.R.Mark", { targetPos.x, targetPos.y + 2.25f, targetPos.z },
                    forward, target, 3.f);
            }
        }
    }
}
