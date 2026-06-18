#include "GameObject/Champion/Kalista/Kalista_Skills.h"
#include "GameObject/Champion/Kalista/KalistaFxPresets.h"
#include "GameObject/Champion/Kalista/KalistaProjectileSystem.h"
#include "GameObject/Champion/Kalista/KalistaRendSystem.h"
#include "GameObject/Champion/Kalista/Kalista_Tuning.h"

#include "ECS/World.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "Renderer/ModelRenderer.h"
#include "Resource/Animation.h"
#include "Resource/Animator.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <Windows.h>

namespace Kalista
{
    namespace
    {
        bool_t s_passiveDashPending = false;
        Vec3 s_passiveDashDirection{};

        bool_t ConsumePassiveDash(Vec3& outDir)
        {
            if (!s_passiveDashPending)
                return false;

            outDir = s_passiveDashDirection;
            s_passiveDashPending = false;
            s_passiveDashDirection = {};
            return true;
        }

        Vec3 ResolveForwardToTarget(CWorld& world, EntityID caster, EntityID target,
            const Vec3& fallback)
        {
            if (caster == NULL_ENTITY || target == NULL_ENTITY)
                return WintersMath::NormalizeXZ(fallback);
            if (!world.HasComponent<TransformComponent>(caster)
                || !world.HasComponent<TransformComponent>(target))
                return WintersMath::NormalizeXZ(fallback);

            const Vec3 origin = world.GetComponent<TransformComponent>(caster).GetPosition();
            const Vec3 targetPos = world.GetComponent<TransformComponent>(target).m_LocalPosition;
            return WintersMath::NormalizeXZ({
                targetPos.x - origin.x,
                0.f,
                targetPos.z - origin.z
            });
        }

        Vec3 GetCasterPosition(CWorld& world, EntityID caster)
        {
            if (!world.HasComponent<TransformComponent>(caster))
                return {};
            return world.GetComponent<TransformComponent>(caster).GetPosition();
        }

        void SpawnSpear(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
            EntityID caster, eTeam casterTeam, const Vec3& origin,
            const Vec3& forward, f32_t speed, f32_t maxDist, f32_t radius,
            f32_t damage, const Vec3& flyScale, f32_t stuckScale,
            bool_t bApplyRendStack)
        {
            const Vec3 dir = WintersMath::NormalizeXZ(forward);
            const f32_t lifetime = (speed > 0.01f) ? (maxDist / speed + 0.1f) : 0.1f;
            const EntityID flyId = KalistaFx::SpawnQSpear(world, pRenderer,
                origin, dir, speed, lifetime, flyScale);

            CKalistaProjectileSystem::Spawn(world,
                origin, dir,
                speed, maxDist, radius, damage,
                caster, casterTeam,
                pRenderer, stuckScale, bApplyRendStack,
                flyId);
        }
    }

    void OnCastFrame_BA(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand)
            return;

        const Vec3 origin = GetCasterPosition(*ctx.pWorld, ctx.casterEntity);
        const Vec3 forward = ResolveForwardToTarget(*ctx.pWorld,
            ctx.casterEntity,
            ctx.pCommand->targetEntityId,
            ctx.pCommand->direction);

        const KalistaTuning& tuning = GetTuning();
        const Vec3 flyScale{
            tuning.baFlySpearScale * tuning.baFlySpearGirthMul,
            tuning.baFlySpearScale * tuning.baFlySpearGirthMul,
            tuning.baFlySpearScale * tuning.baFlySpearLengthMul
        };
        SpawnSpear(*ctx.pWorld, ctx.pFxMeshRenderer,
            ctx.casterEntity, ctx.casterTeam,
            origin, forward,
            tuning.baSpeed, tuning.baMaxDist, tuning.baRadius, tuning.baDamage,
            flyScale, tuning.baStuckSpearScale,
            false);

        if (ctx.pCommand->targetEntityId != NULL_ENTITY)
        {
            CKalistaRendSystem::AddStack(
                *ctx.pWorld,
                ctx.pCommand->targetEntityId,
                ctx.casterEntity,
                ctx.casterTeam,
                ctx.pFxMeshRenderer,
                tuning.baStuckSpearScale);
        }
    }

    void OnCastFrame_Q(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand)
            return;

        const Vec3 origin = GetCasterPosition(*ctx.pWorld, ctx.casterEntity);
        const Vec3 forward = WintersMath::NormalizeXZ(ctx.pCommand->direction);

        const KalistaTuning& tuning = GetTuning();
        const Vec3 flyScale{
            tuning.qFlySpearScale,
            tuning.qFlySpearScale,
            tuning.qFlySpearScale
        };
        SpawnSpear(*ctx.pWorld, ctx.pFxMeshRenderer,
            ctx.casterEntity, ctx.casterTeam,
            origin, forward,
            tuning.qSpeed, tuning.qMaxDist, tuning.qRadius, tuning.qDamage,
            flyScale, tuning.qStuckSpearScale,
            true);
    }

    void OnCastAccepted_E(SkillHookContext& ctx)
    {
        if (!ctx.pWorld)
            return;

        const KalistaTuning& tuning = GetTuning();
        CKalistaRendSystem::TriggerExplode(*ctx.pWorld,
            ctx.casterEntity, tuning.rendBaseDamage, tuning.rendStackDamage);
    }

    void OnRecoveryFrame_PassiveDash(SkillHookContext& ctx)
    {
        if (!ctx.pDef)
            return;
        if (ctx.pDef->slot != 0 && ctx.pDef->slot != 1)
            return;

        Vec3 dashDir{};
        if (!ConsumePassiveDash(dashDir))
            return;

        const i32_t slot = ctx.pDef->slot;
        const char* pDashAnim = "kalista_spell1_dash_0";

        if (ctx.setLocalActionAnimActive)
            ctx.setLocalActionAnimActive(true);

        bool_t bDashAnimPlayed = false;
        if (ctx.bPlayPassiveDashAnimation && ctx.pCasterRenderer)
        {
            if (Engine::CAnimator* pDashAnimator = ctx.pCasterRenderer->GetAnimator())
            {
                const Engine::CAnimation* pCurrent = pDashAnimator->GetCurrentAnimation();
                const bool_t bAlreadyPlayingDash =
                    pCurrent &&
                    pDashAnimator->IsPlaying() &&
                    pCurrent->GetName().find(pDashAnim) != std::string::npos;
                if (!bAlreadyPlayingDash)
                {
                    ctx.pCasterRenderer->PlayAnimationByName(pDashAnim, false);
                    bDashAnimPlayed = true;
                }

                const KalistaTuning& tuning = GetTuning();
                const f32_t rawDashSpeed = ctx.fGlobalAnimSpeed * tuning.passiveDashAnimSpeed;
                const f32_t dashSpeed = (rawDashSpeed < 0.01f) ? 0.01f : rawDashSpeed;
                pDashAnimator->SetPlaySpeed(dashSpeed);

                (void)pDashAnimator->GetCurrentAnimation();
            }
            else
            {
                ctx.pCasterRenderer->PlayAnimationByName(pDashAnim, false);
                bDashAnimPlayed = true;
            }
        }

        const f32_t duration = ctx.getLocalDashDuration
            ? ctx.getLocalDashDuration()
            : GetTuning().passiveDashDuration;

        char dbg[160]{};
        sprintf_s(dbg,
            "[KalistaPassive] dash dir=(%.2f,0,%.2f) slot=%d anim=%s dur=%.2f seq=%u played=%u\n",
            dashDir.x,
            dashDir.z,
            slot,
            pDashAnim,
            duration,
            ctx.actionSeq,
            bDashAnimPlayed ? 1u : 0u);

        if (ctx.startLocalDash)
            ctx.startLocalDash(dashDir);
    }

    void QueuePassiveDash(const Vec3& direction)
    {
        s_passiveDashDirection = WintersMath::NormalizeXZ(direction);
        s_passiveDashPending = true;
    }

    bool_t HasPassiveDashRequest()
    {
        return s_passiveDashPending;
    }

    void ClearPassiveDashRequest()
    {
        s_passiveDashPending = false;
        s_passiveDashDirection = {};
    }
}
