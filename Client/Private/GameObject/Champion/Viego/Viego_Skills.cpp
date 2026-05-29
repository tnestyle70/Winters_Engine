#include "GameObject/Champion/Viego/Viego_Skills.h"

#include "GameObject/Champion/Viego/Viego_FxPresets.h"

#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"

#include <Windows.h>
#include <cmath>

namespace Viego
{
    namespace
    {
        Vec3 ResolveOrigin(CWorld* world, EntityID caster)
        {
            if (!world || !world->HasComponent<TransformComponent>(caster))
                return Vec3{};

            return world->GetComponent<TransformComponent>(caster).GetPosition();
        }

        Vec3 ResolveDirection(CWorld* world, EntityID caster, const CastSkillCommand* command)
        {
            if (command)
            {
                Vec3 dir = WintersMath::NormalizeXZ(command->direction);
                if (dir.x != 0.f || dir.z != 0.f)
                    return dir;

                if (world && world->HasComponent<TransformComponent>(caster))
                {
                    const Vec3 origin = world->GetComponent<TransformComponent>(caster).GetPosition();
                    dir = WintersMath::NormalizeXZ(Vec3{
                        command->groundPos.x - origin.x,
                        0.f,
                        command->groundPos.z - origin.z
                    });
                    if (dir.x != 0.f || dir.z != 0.f)
                        return dir;
                }
            }

            return Vec3{ 0.f, 0.f, 1.f };
        }
    }

    void OnCastFrame_BA(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand)
            return;

        Fx::SpawnBAHit(
            *ctx.pWorld,
            ctx.pFxMeshRenderer,
            ctx.casterEntity,
            ctx.pCommand->targetEntityId,
            0.35f);
    }

    void OnCastFrame_Q(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand)
            return;

        const Vec3 origin = ResolveOrigin(ctx.pWorld, ctx.casterEntity);
        const Vec3 dir = ResolveDirection(ctx.pWorld, ctx.casterEntity, ctx.pCommand);
        Fx::SpawnQSlash(*ctx.pWorld, ctx.pFxMeshRenderer, ctx.casterEntity, origin, dir, 0.45f);
    }

    void OnCastFrame_W(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand)
            return;

        const Vec3 origin = ResolveOrigin(ctx.pWorld, ctx.casterEntity);
        const Vec3 dir = ResolveDirection(ctx.pWorld, ctx.casterEntity, ctx.pCommand);
        Fx::SpawnWMissile(*ctx.pWorld, ctx.pFxMeshRenderer, ctx.casterEntity, origin, dir, 0.45f);
    }

    void OnCastFrame_E(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand)
            return;

        const Vec3 origin = ResolveOrigin(ctx.pWorld, ctx.casterEntity);
        const Vec3 dir = ResolveDirection(ctx.pWorld, ctx.casterEntity, ctx.pCommand);
        Fx::SpawnEMist(*ctx.pWorld, ctx.pFxMeshRenderer, ctx.casterEntity, origin, dir, 4.0f);
    }

    void OnCastFrame_R(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand)
            return;

        const Vec3 origin = ResolveOrigin(ctx.pWorld, ctx.casterEntity);
        const Vec3 dir = ResolveDirection(ctx.pWorld, ctx.casterEntity, ctx.pCommand);
        Fx::SpawnRImpact(*ctx.pWorld, ctx.pFxMeshRenderer, ctx.casterEntity, origin, dir, 0.8f);
    }

    namespace Gameplay
    {
        void OnCastFrame_BA(GameplayHookContext& ctx) { (void)ctx; }
        void OnCastFrame_Q(GameplayHookContext& ctx) { (void)ctx; }
        void OnCastFrame_W(GameplayHookContext& ctx) { (void)ctx; }
        void OnCastFrame_E(GameplayHookContext& ctx) { (void)ctx; }
        void OnCastFrame_R(GameplayHookContext& ctx) { (void)ctx; }
    }

    namespace Visual
    {
        void OnCastFrame_BA_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand)
                return;

            Fx::SpawnBAHit(
                *ctx.pWorld,
                ctx.pFxMeshRenderer,
                ctx.casterEntity,
                ctx.pCommand->targetEntityId,
                0.35f);
        }

        void OnCastFrame_Q_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand)
                return;

            const Vec3 origin = ResolveOrigin(ctx.pWorld, ctx.casterEntity);
            const Vec3 dir = ResolveDirection(ctx.pWorld, ctx.casterEntity, ctx.pCommand);
            Fx::SpawnQSlash(*ctx.pWorld, ctx.pFxMeshRenderer, ctx.casterEntity, origin, dir, 0.45f);
        }

        void OnCastFrame_W_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand)
                return;

            const Vec3 origin = ResolveOrigin(ctx.pWorld, ctx.casterEntity);
            const Vec3 dir = ResolveDirection(ctx.pWorld, ctx.casterEntity, ctx.pCommand);
            Fx::SpawnWMissile(*ctx.pWorld, ctx.pFxMeshRenderer, ctx.casterEntity, origin, dir, 0.45f);
        }

        void OnCastFrame_E_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand)
                return;

            const Vec3 origin = ResolveOrigin(ctx.pWorld, ctx.casterEntity);
            const Vec3 dir = ResolveDirection(ctx.pWorld, ctx.casterEntity, ctx.pCommand);
            Fx::SpawnEMist(*ctx.pWorld, ctx.pFxMeshRenderer, ctx.casterEntity, origin, dir, 4.0f);
        }

        void OnCastFrame_R_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand)
                return;

            const Vec3 origin = ResolveOrigin(ctx.pWorld, ctx.casterEntity);
            const Vec3 dir = ResolveDirection(ctx.pWorld, ctx.casterEntity, ctx.pCommand);
            Fx::SpawnRImpact(*ctx.pWorld, ctx.pFxMeshRenderer, ctx.casterEntity, origin, dir, 0.8f);
        }
    }
}
