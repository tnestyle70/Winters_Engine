#include "GameObject/Champion/Ashe/Ashe_Skills.h"
#include "GameObject/Champion/Ashe/Ashe_Components.h"
#include "GameObject/Champion/Ashe/Ashe_FxPresets.h"
#include "GameObject/Champion/Ashe/AsheVisualCueCatalog.h"
#include "GameObject/FX/FxCuePlayer.h"
#include "GameObject/Champion/Yasuo/PendingHitSystem.h"
#include "GameObject/Projectile/ProjectileKind.h"
#include "GamePlay/Systems/Damage.h"

#include "ECS/World.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"

#include <Windows.h>
#include <cmath>
#include <cstdio>

namespace Ashe
{
    namespace
    {
        Vec3 GetMuzzlePos(CWorld& world, EntityID entity)
        {
            Vec3 pos{};
            if (world.HasComponent<TransformComponent>(entity))
            {
                auto& tf = world.GetComponent<TransformComponent>(entity);
                pos = tf.GetWorldPosition();
                pos.y += 1.0f;
            }
            return pos;
        }

        void ApplyFrostSlowStub(CWorld&, EntityID target, f32_t, f32_t)
        {
            char dbg[96];
            sprintf_s(dbg, "[Ashe Frost] target=%u slow stub\n", static_cast<u32_t>(target));
        }

        Vec3 ResolveVisualPosition(CWorld& world, EntityID entity)
        {
            if (entity != NULL_ENTITY && world.HasComponent<TransformComponent>(entity))
                return world.GetComponent<TransformComponent>(entity).GetPosition();
            return Vec3{};
        }

        Vec3 ResolveVisualForward(CWorld& world, EntityID caster, const CastSkillCommand* pCommand)
        {
            if (pCommand)
            {
                const Vec3 commandDir = WintersMath::NormalizeXZOrZero(pCommand->direction);
                if (std::fabs(commandDir.x) > 0.001f || std::fabs(commandDir.z) > 0.001f)
                    return commandDir;

                if (pCommand->targetEntityId != NULL_ENTITY &&
                    world.HasComponent<TransformComponent>(caster) &&
                    world.HasComponent<TransformComponent>(pCommand->targetEntityId))
                {
                    const Vec3 casterPos = world.GetComponent<TransformComponent>(caster).GetPosition();
                    const Vec3 targetPos = world.GetComponent<TransformComponent>(pCommand->targetEntityId).GetPosition();
                    const Vec3 targetDir{
                        targetPos.x - casterPos.x,
                        0.f,
                        targetPos.z - casterPos.z
                    };
                    const Vec3 n = WintersMath::NormalizeXZOrZero(targetDir);
                    if (std::fabs(n.x) > 0.001f || std::fabs(n.z) > 0.001f)
                        return n;
                }
            }

            return { 0.f, 0.f, 1.f };
        }

        void PlayMovingCue(
            CWorld& world,
            Engine::CFxStaticMeshRenderer* pFxMeshRenderer,
            const char* pszCueName,
            const Vec3& origin,
            const Vec3& forward,
            f32_t speed,
            f32_t lifetime)
        {
            if (!pszCueName)
                return;

            FxCueContext fx{};
            fx.vWorldPos = origin;
            fx.vForward = forward;
            fx.vVelocity = { forward.x * speed, forward.y * speed, forward.z * speed };
            fx.bOverrideVelocity = true;
            fx.pFxMeshRenderer = pFxMeshRenderer;
            fx.fLifetimeOverride = lifetime;
            fx.bOverrideLifetime = true;
            CFxCuePlayer::Play(world, pszCueName, fx);
        }

        void PlayAttachedCue(
            CWorld& world,
            Engine::CFxStaticMeshRenderer* pFxMeshRenderer,
            const char* pszCueName,
            EntityID attachTo,
            const Vec3& worldPos,
            const Vec3& forward,
            f32_t lifetime)
        {
            if (!pszCueName)
                return;

            FxCueContext fx{};
            fx.vWorldPos = worldPos;
            fx.vForward = forward;
            fx.attachTo = attachTo;
            fx.pFxMeshRenderer = pFxMeshRenderer;
            if (lifetime > 0.f)
            {
                fx.fLifetimeOverride = lifetime;
                fx.bOverrideLifetime = true;
            }
            CFxCuePlayer::Play(world, pszCueName, fx);
        }
    }

    void OnCastFrame_BA(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand) return;

        const EntityID target = ctx.pCommand->targetEntityId;
        if (target == NULL_ENTITY) return;

        f32_t fDamage = 50.f;
        if (ctx.pWorld->HasComponent<AsheStateComponent>(ctx.casterEntity))
        {
            auto& as = ctx.pWorld->GetComponent<AsheStateComponent>(ctx.casterEntity);
            if (as.bQActive)
                fDamage += as.fQDamageBonus;

            if (!as.bQActive)
            {
                as.focusStacks = static_cast<u8_t>(as.focusStacks + 1);
                if (as.focusStacks == as.focusThreshold)
                    Fx::SpawnQReadySparks(*ctx.pWorld, ctx.casterEntity, 0.6f);
            }
        }

        ApplyDamage(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam, target, fDamage);

        if (ctx.pWorld->HasComponent<AsheStateComponent>(ctx.casterEntity))
        {
            const auto& as = ctx.pWorld->GetComponent<AsheStateComponent>(ctx.casterEntity);
            ApplyFrostSlowStub(*ctx.pWorld, target, as.fFrostSlowPercent, as.fFrostSlowDuration);
        }

        char dbg[128];
        sprintf_s(dbg, "[Ashe BA] target=%u dmg=%.1f\n",
            static_cast<u32_t>(target), fDamage);
    }

    void OnCastFrame_Q(SkillHookContext& ctx)
    {
        if (!ctx.pWorld) return;
        if (!ctx.pWorld->HasComponent<AsheStateComponent>(ctx.casterEntity)) return;

        auto& as = ctx.pWorld->GetComponent<AsheStateComponent>(ctx.casterEntity);
        if (as.focusStacks < as.focusThreshold)
        {
            char dbg[96];
            sprintf_s(dbg, "[Ashe Q] not ready (stacks=%u/%u)\n",
                as.focusStacks, as.focusThreshold);
            return;
        }

        as.bQActive = true;
        as.fQTimer = as.fQDurationSec;
        as.focusStacks = 0;

    }

    void OnCastFrame_W(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand) return;
        if (!ctx.pWorld->HasComponent<AsheStateComponent>(ctx.casterEntity)) return;

        const auto& as = ctx.pWorld->GetComponent<AsheStateComponent>(ctx.casterEntity);
        const Vec3 dir = WintersMath::NormalizeXZ(ctx.pCommand->direction);
        const Vec3 origin = GetMuzzlePos(*ctx.pWorld, ctx.casterEntity);
        const u32_t arrowCount = as.volleyArrowCount > 0 ? as.volleyArrowCount : 1;
        const f32_t halfConeRad = DirectX::XMConvertToRadians(as.fVolleyConeAngleDeg * 0.5f);

        for (u32_t i = 0; i < arrowCount; ++i)
        {
            const f32_t t = (arrowCount > 1)
                ? static_cast<f32_t>(i) / static_cast<f32_t>(arrowCount - 1)
                : 0.5f;
            const f32_t angle = -halfConeRad + (halfConeRad * 2.f * t);
            const Vec3 arrowDir = WintersMath::RotateXZ(dir, angle);
            Fx::SpawnWVolleyArrow(*ctx.pWorld, ctx.casterEntity, origin, arrowDir, 0.35f);
            CPendingHitSystem::Schedule(*ctx.pWorld,
                ctx.casterEntity, ctx.casterTeam, arrowDir,
                0.f, eProjectileKind::AsheVolleyArrow,
                35.f, as.fVolleyRange, 0.35f,
                40.f, 0.f);
        }

        char dbg[128];
        sprintf_s(dbg, "[Ashe W] %u arrows fired\n", arrowCount);
    }

    void OnCastFrame_E(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand) return;
        if (!ctx.pWorld->HasComponent<AsheStateComponent>(ctx.casterEntity)) return;
        if (!ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity)) return;

        const auto& as = ctx.pWorld->GetComponent<AsheStateComponent>(ctx.casterEntity);
        const Vec3 origin =
            ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        const Vec3 dir = WintersMath::NormalizeXZ(ctx.pCommand->direction);
        const Vec3 dest = {
            origin.x + dir.x * as.fHawkshotRange,
            origin.y,
            origin.z + dir.z * as.fHawkshotRange
        };

        char dbg[160];
        sprintf_s(dbg, "[Ashe E] Hawkshot dest=(%.1f,%.1f,%.1f) duration=%.1fs\n",
            dest.x, dest.y, dest.z, as.fHawkshotVisionDurationSec);
    }

    void OnCastFrame_R(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand) return;
        if (!ctx.pWorld->HasComponent<AsheStateComponent>(ctx.casterEntity)) return;

        const auto& as = ctx.pWorld->GetComponent<AsheStateComponent>(ctx.casterEntity);
        const Vec3 dir = WintersMath::NormalizeXZ(ctx.pCommand->direction);

        CPendingHitSystem::Schedule(*ctx.pWorld,
            ctx.casterEntity, ctx.casterTeam, dir,
            0.f, eProjectileKind::AsheCrystalArrow,
            as.fCrystalArrowSpeed, as.fCrystalArrowMaxDist,
            1.0f, 250.f, as.fCrystalArrowStunMin);

        char dbg[128];
        sprintf_s(dbg, "[Ashe R] Crystal Arrow fired dir=(%.2f,%.2f,%.2f)\n",
            dir.x, dir.y, dir.z);
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
            if (!ctx.pWorld || !ctx.pCommand) return;
            const EntityID target = ctx.pCommand->targetEntityId;
            const Vec3 forward = ResolveVisualForward(*ctx.pWorld, ctx.casterEntity, ctx.pCommand);
            const Vec3 origin = GetMuzzlePos(*ctx.pWorld, ctx.casterEntity);

            PlayMovingCue(*ctx.pWorld, ctx.pFxMeshRenderer,
                VisualCue::kBAArrow, origin, forward,
                VisualCue::kBAArrowSpeed, VisualCue::kBAArrowLifetime);
            if (target != NULL_ENTITY)
            {
                PlayAttachedCue(*ctx.pWorld, ctx.pFxMeshRenderer,
                    VisualCue::kBAHit, target,
                    ResolveVisualPosition(*ctx.pWorld, target),
                    forward, VisualCue::kBAArrowLifetime);
            }
        }

        void OnCastFrame_Q_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld) return;
            Fx::SpawnQBuffActive(*ctx.pWorld, ctx.casterEntity, 4.0f);
            Fx::SpawnQBuffMesh(*ctx.pWorld, ctx.pFxMeshRenderer, ctx.casterEntity, 4.0f);
        }

        void OnCastFrame_W_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld) return;
            const Vec3 forward = ResolveVisualForward(*ctx.pWorld, ctx.casterEntity, ctx.pCommand);
            PlayAttachedCue(*ctx.pWorld, ctx.pFxMeshRenderer,
                VisualCue::kWCast, ctx.casterEntity,
                ResolveVisualPosition(*ctx.pWorld, ctx.casterEntity),
                forward, VisualCue::kWCastLifetime);
        }

        void OnCastFrame_E_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand) return;
            if (!ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity)) return;

            const Vec3 origin =
                ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
            const Vec3 dir = ctx.pCommand->direction;
            const Vec3 dest = { origin.x + dir.x * 25.f, origin.y, origin.z + dir.z * 25.f };
            Fx::SpawnEHawkshot(*ctx.pWorld, origin, dest, 5.0f);
        }

        void OnCastFrame_R_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand) return;
            const Vec3 forward = ResolveVisualForward(*ctx.pWorld, ctx.casterEntity, ctx.pCommand);
            PlayAttachedCue(*ctx.pWorld, ctx.pFxMeshRenderer,
                VisualCue::kRCharge, ctx.casterEntity,
                ResolveVisualPosition(*ctx.pWorld, ctx.casterEntity),
                forward, VisualCue::kRChargeLifetime);
        }
    }
}
