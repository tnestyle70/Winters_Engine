#include "GameObject/Champion/Fiora/Fiora_Skills.h"
#include "GameObject/Champion/Fiora/Fiora_Components.h"
#include "GameObject/Champion/Fiora/Fiora_FxPresets.h"
#include "GamePlay/Systems/Damage.h"

#include "ECS/World.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"

#include <Windows.h>
#include <cmath>
#include <cstdio>

namespace Fiora
{
    namespace
    {
        void ApplyBasicAttack(CWorld& world, EntityID caster, eTeam casterTeam,
                              EntityID target)
        {
            if (target == NULL_ENTITY) return;

            f32_t fDamage = 55.f;
            if (world.HasComponent<FioraStateComponent>(caster))
            {
                auto& fs = world.GetComponent<FioraStateComponent>(caster);
                if (fs.bBladeworkActive && fs.bladeworkHitsRemaining > 0)
                {
                    fDamage += fs.fBladeworkDamageBonus;
                    fs.bladeworkHitsRemaining = static_cast<u8_t>(fs.bladeworkHitsRemaining - 1);
                    if (fs.bladeworkHitsRemaining == 0)
                        fs.bBladeworkActive = false;
                }
            }

            ApplyDamage(world, caster, casterTeam, target, fDamage);

            char dbg[128];
            sprintf_s(dbg, "[Fiora BA] target=%u dmg=%.1f\n",
                static_cast<u32_t>(target), fDamage);
        }

        EntityID FindEnemyInCone(CWorld& world, EntityID caster, eTeam casterTeam,
                                 const Vec3& origin, const Vec3& dir,
                                 f32_t fRange, f32_t fHitRadius)
        {
            const f32_t lenSq = dir.x * dir.x + dir.z * dir.z;
            if (lenSq <= 0.0001f) return NULL_ENTITY;
            const f32_t invLen = 1.f / std::sqrtf(lenSq);
            const Vec3 fwd{ dir.x * invLen, 0.f, dir.z * invLen };

            EntityID best = NULL_ENTITY;
            f32_t fBestDistSq = FLT_MAX;
            world.ForEach<ChampionComponent, TransformComponent>(
                [&](EntityID e, ChampionComponent& cc, TransformComponent& tf)
                {
                    if (e == caster) return;
                    if (cc.team == casterTeam) return;
                    const Vec3 vEnemy = tf.GetPosition();
                    const f32_t dx = vEnemy.x - origin.x;
                    const f32_t dz = vEnemy.z - origin.z;
                    const f32_t fDist = std::sqrtf(dx * dx + dz * dz);
                    if (fDist > fRange + fHitRadius) return;
                    const f32_t fDot = (fDist > 0.001f)
                        ? (fwd.x * dx / fDist + fwd.z * dz / fDist) : 1.f;
                    if (fDot < 0.5f) return;
                    const f32_t fDistSq = dx * dx + dz * dz;
                    if (fDistSq < fBestDistSq)
                    {
                        fBestDistSq = fDistSq;
                        best = e;
                    }
                });
            return best;
        }

        void ApplyLunge(CWorld& world, EntityID caster, eTeam casterTeam,
                        const Vec3& vDir)
        {
            if (!world.HasComponent<TransformComponent>(caster)) return;

            auto& tf = world.GetComponent<TransformComponent>(caster);
            const Vec3 vOrigin = tf.GetPosition();

            const f32_t lenSq = vDir.x * vDir.x + vDir.z * vDir.z;
            if (lenSq > 0.0001f)
            {
                const f32_t invLen = 1.f / std::sqrtf(lenSq);
                const Vec3 fwd{ vDir.x * invLen, 0.f, vDir.z * invLen };
                const Vec3 vDest{
                    vOrigin.x + fwd.x * 3.0f, vOrigin.y, vOrigin.z + fwd.z * 3.0f };
                tf.SetPosition(vDest);
                tf.m_bLocalDirty = true;
                tf.m_bWorldDirty = true;
            }

            const EntityID hitTarget = FindEnemyInCone(world,
                caster, casterTeam, vOrigin, vDir, 4.0f, 1.0f);
            if (hitTarget != NULL_ENTITY)
                ApplyDamage(world, caster, casterTeam, hitTarget, 70.f);

            char dbg[160];
            sprintf_s(dbg, "[Fiora Q] origin=(%.1f,%.1f,%.1f) hit=%u\n",
                vOrigin.x, vOrigin.y, vOrigin.z, static_cast<u32_t>(hitTarget));
        }

        void ArmRiposte(CWorld& world, EntityID caster)
        {
            if (!world.HasComponent<FioraStateComponent>(caster)) return;
            auto& fs = world.GetComponent<FioraStateComponent>(caster);
            fs.bRiposteActive = true;
            fs.fRiposteTimer = fs.fRiposteWindowSec;
        }

        void ActivateBladework(CWorld& world, EntityID caster)
        {
            if (!world.HasComponent<FioraStateComponent>(caster)) return;
            auto& fs = world.GetComponent<FioraStateComponent>(caster);
            fs.bBladeworkActive = true;
            fs.fBladeworkTimer = 5.0f;
            fs.bladeworkHitsRemaining = 2;
        }

        void MarkGrandChallenge(CWorld& world, EntityID caster, eTeam casterTeam,
                                EntityID target)
        {
            if (!world.HasComponent<FioraStateComponent>(caster)) return;
            if (target == NULL_ENTITY) return;

            auto& fs = world.GetComponent<FioraStateComponent>(caster);
            fs.bRActive = true;
            fs.fRTimer = 8.0f;
            fs.rTargetEntity = target;
            ApplyDamage(world, caster, casterTeam, target, 80.f);

            char dbg[128];
            sprintf_s(dbg, "[Fiora R] target=%u marked\n", static_cast<u32_t>(target));
        }
    }

    void OnCastFrame_BA(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand) return;
        ApplyBasicAttack(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam,
            ctx.pCommand->targetEntityId);
    }

    void OnCastFrame_Q(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand) return;
        ApplyLunge(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam,
            ctx.pCommand->direction);
    }

    void OnCastFrame_W(SkillHookContext& ctx)
    {
        if (!ctx.pWorld) return;
        ArmRiposte(*ctx.pWorld, ctx.casterEntity);
    }

    void OnCastFrame_E(SkillHookContext& ctx)
    {
        if (!ctx.pWorld) return;
        ActivateBladework(*ctx.pWorld, ctx.casterEntity);
    }

    void OnCastFrame_R(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand) return;
        MarkGrandChallenge(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam,
            ctx.pCommand->targetEntityId);
    }

    namespace Gameplay
    {
        void OnCastFrame_BA(GameplayHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand) return;
            ApplyBasicAttack(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam,
                ctx.pCommand->targetEntity);
        }

        void OnCastFrame_Q(GameplayHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand) return;
            ApplyLunge(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam,
                ctx.pCommand->direction);
        }

        void OnCastFrame_W(GameplayHookContext& ctx)
        {
            if (!ctx.pWorld) return;
            ArmRiposte(*ctx.pWorld, ctx.casterEntity);
        }

        void OnCastFrame_E(GameplayHookContext& ctx)
        {
            if (!ctx.pWorld) return;
            ActivateBladework(*ctx.pWorld, ctx.casterEntity);
        }

        void OnCastFrame_R(GameplayHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand) return;
            MarkGrandChallenge(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam,
                ctx.pCommand->targetEntity);
        }
    }

    namespace Visual
    {
        void OnCastFrame_Q_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand) return;
            Fx::SpawnQSlash(*ctx.pWorld, ctx.casterEntity,
                ctx.pCommand->direction, 0.4f);
        }

        void OnCastFrame_W_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand) return;
            const f32_t duration = ctx.fEffectLifetimeSec > 0.f
                ? ctx.fEffectLifetimeSec
                : 0.75f;
            if (ctx.skillStage >= 2u)
                Fx::SpawnWParrySuccess(*ctx.pWorld, ctx.casterEntity,
                    ctx.pCommand->direction, duration);
            else
                Fx::SpawnWParryActive(*ctx.pWorld, ctx.casterEntity,
                    ctx.pCommand->direction, duration);
        }

        void OnRecovery_W_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand) return;
            Fx::SpawnWRelease(*ctx.pWorld, ctx.casterEntity,
                ctx.pCommand->direction, 0.30f);
        }

        void OnCastFrame_E_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld) return;
            Fx::SpawnEBladeworkBuff(
                *ctx.pWorld,
                ctx.casterEntity,
                ctx.fEffectLifetimeSec > 0.f ? ctx.fEffectLifetimeSec : 5.0f);
        }

        void OnRecovery_E_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand) return;
            if (ctx.pCommand->targetEntityId != NULL_ENTITY)
                Fx::SpawnEHitSpark(*ctx.pWorld, ctx.pCommand->targetEntityId, 0.40f);
            if (ctx.skillStage >= 3u)
                Fx::StopEBladeworkBuff(*ctx.pWorld, ctx.casterEntity);
        }

        void OnCastFrame_R_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand || ctx.skillStage < 2u) return;
            Fx::SpawnVital(*ctx.pWorld, ctx.casterEntity,
                ctx.pCommand->targetEntityId, ctx.pCommand->direction,
                Fx::eVitalVisualKind::GrandChallenge, ctx.fEffectLifetimeSec);
        }

        void OnRecovery_R_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand) return;
            if (ctx.skillStage == 1u)
                Fx::SpawnRRing(*ctx.pWorld, ctx.casterEntity,
                    ctx.pCommand->targetEntityId, ctx.pCommand->groundPos,
                    ctx.fEffectLifetimeSec);
            else if (ctx.skillStage == 2u)
                Fx::ConsumeVital(*ctx.pWorld, ctx.casterEntity,
                    ctx.pCommand->targetEntityId, ctx.pCommand->direction,
                    Fx::eVitalVisualKind::GrandChallenge);
            else if (ctx.skillStage == 3u)
            {
                Fx::ClearRPresentation(*ctx.pWorld, ctx.casterEntity);
                Fx::SpawnRHealZone(*ctx.pWorld, ctx.pCommand->groundPos,
                    ctx.fEffectLifetimeSec);
            }
            else if (ctx.skillStage >= 4u)
                Fx::ClearRPresentation(*ctx.pWorld, ctx.casterEntity);
        }

        void OnPassiveTrigger_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand) return;
            if (ctx.skillStage == 1u)
                Fx::SpawnVital(*ctx.pWorld, ctx.casterEntity,
                    ctx.pCommand->targetEntityId, ctx.pCommand->direction,
                    Fx::eVitalVisualKind::Passive, ctx.fEffectLifetimeSec);
            else if (ctx.skillStage == 2u)
                Fx::ConsumeVital(*ctx.pWorld, ctx.casterEntity,
                    ctx.pCommand->targetEntityId, ctx.pCommand->direction,
                    Fx::eVitalVisualKind::Passive);
            else
                Fx::ClearVitals(*ctx.pWorld, ctx.casterEntity,
                    Fx::eVitalVisualKind::Passive);
        }
    }
}
