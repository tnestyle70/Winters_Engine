#include "GameObject/Champion/Jax/Jax_Skills.h"
#include "GameObject/Champion/Jax/Jax_Components.h"
#include "GameObject/Champion/Jax/Jax_FxPresets.h"
#include "GamePlay/Systems/Damage.h"

#include "ECS/World.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"

#include <Windows.h>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <vector>

namespace Jax
{
    namespace
    {
        u32_t ApplyAOEDamageInRadius(CWorld& world, EntityID caster, eTeam casterTeam,
            const Vec3& origin, f32_t fRadius, f32_t fDamage);

        void ApplyBasicAttack(CWorld& world, EntityID caster, eTeam casterTeam,
            EntityID target, bool_t bSpawnVisual)
        {
            if (target == NULL_ENTITY) return;

            f32_t fDamage = 50.f;

            if (world.HasComponent<JaxStateComponent>(caster))
            {
                auto& js = world.GetComponent<JaxStateComponent>(caster);
                if (js.bEmpowerActive)
                {
                    fDamage += js.fEmpowerDamageBonus;
                    js.bEmpowerActive = false;
                    js.fEmpowerTimer = 0.f;
                }

                if (js.bUltActive)
                {
                    js.ultAttackCounter = static_cast<u8_t>(js.ultAttackCounter + 1);
                    if (js.ultAttackCounter >= 3)
                    {
                        js.ultAttackCounter = 0;
                        if (world.HasComponent<TransformComponent>(caster))
                        {
                            const Vec3 vOrigin = world
                                .GetComponent<TransformComponent>(caster).GetPosition();
                            const u32_t hits = ApplyAOEDamageInRadius(world,
                                caster, casterTeam, vOrigin,
                                js.fUltAOERadius, js.fUltAOEDamage);
                            if (bSpawnVisual)
                                Fx::SpawnRThirdAttackAOE(world, caster, 0.5f);

                            char dbg[128];
                            sprintf_s(dbg, "[Jax R AOE] hits=%u dmg=%.1f\n",
                                hits, js.fUltAOEDamage);
                            OutputDebugStringA(dbg);
                        }
                    }
                }
            }

            ApplyDamage(world, caster, casterTeam, target, fDamage);

            char dbg[128];
            sprintf_s(dbg, "[Jax BA] target=%u dmg=%.1f\n",
                static_cast<u32_t>(target), fDamage);
            OutputDebugStringA(dbg);
        }

        u32_t ApplyAOEDamageInRadius(CWorld& world, EntityID caster, eTeam casterTeam,
            const Vec3& origin, f32_t fRadius, f32_t fDamage)
        {
            u32_t hits = 0;
            std::vector<EntityID> targets;
            targets.reserve(8);

            world.ForEach<ChampionComponent, TransformComponent>(
                [&](EntityID e, ChampionComponent& cc, TransformComponent& tf)
                {
                    if (e == caster) return;
                    if (cc.team == casterTeam) return;

                    const Vec3 v = tf.GetPosition();
                    const f32_t dx = v.x - origin.x;
                    const f32_t dz = v.z - origin.z;
                    if (dx * dx + dz * dz <= fRadius * fRadius)
                        targets.push_back(e);
                });

            for (EntityID e : targets)
            {
                ApplyDamage(world, caster, casterTeam, e, fDamage);
                ++hits;
            }

            return hits;
        }

        void ApplyLeapStrike(CWorld& world, EntityID caster, eTeam casterTeam,
            EntityID target)
        {
            if (!world.HasComponent<TransformComponent>(caster)) return;

            auto& tf = world.GetComponent<TransformComponent>(caster);
            const Vec3 vOrigin = tf.GetPosition();
            Vec3 vDest = vOrigin;

            if (target != NULL_ENTITY
                && world.HasComponent<TransformComponent>(target))
            {
                const Vec3 vTarget = world
                    .GetComponent<TransformComponent>(target).GetPosition();
                const f32_t dx = vTarget.x - vOrigin.x;
                const f32_t dz = vTarget.z - vOrigin.z;
                const f32_t fLen = std::sqrtf(dx * dx + dz * dz);
                if (fLen > 0.001f)
                {
                    const f32_t fGap = 1.0f;
                    const f32_t fMove = (fLen > fGap) ? (fLen - fGap) : 0.f;
                    vDest = {
                        vOrigin.x + (dx / fLen) * fMove,
                        vOrigin.y,
                        vOrigin.z + (dz / fLen) * fMove
                    };
                    tf.SetPosition(vDest);
                    tf.m_bLocalDirty = true;
                    tf.m_bWorldDirty = true;
                }

                ApplyDamage(world, caster, casterTeam, target, 70.f);
            }

            char dbg[160];
            sprintf_s(dbg, "[Jax Q] origin=(%.1f,%.1f,%.1f) dest=(%.1f,%.1f,%.1f) target=%u\n",
                vOrigin.x, vOrigin.y, vOrigin.z, vDest.x, vDest.y, vDest.z,
                static_cast<u32_t>(target));
            OutputDebugStringA(dbg);
        }

        void ArmEmpower(CWorld& world, EntityID caster)
        {
            if (!world.HasComponent<JaxStateComponent>(caster)) return;

            auto& js = world.GetComponent<JaxStateComponent>(caster);
            js.bEmpowerActive = true;
            js.fEmpowerTimer = js.fEmpowerWindowSec;

            OutputDebugStringA("[Jax W] Empower armed (next BA enhanced)\n");
        }

        void ActivateCounterStrike(CWorld& world, EntityID caster, eTeam casterTeam)
        {
            if (!world.HasComponent<JaxStateComponent>(caster)) return;
            if (!world.HasComponent<TransformComponent>(caster)) return;

            auto& js = world.GetComponent<JaxStateComponent>(caster);
            const Vec3 vOrigin = world
                .GetComponent<TransformComponent>(caster).GetPosition();

            const u32_t hits = ApplyAOEDamageInRadius(world,
                caster, casterTeam, vOrigin, 1.8f, 60.f);

            js.bCounterActive = true;
            js.fCounterTimer = js.fCounterWindowSec;

            char dbg[128];
            sprintf_s(dbg, "[Jax E] hits=%u counter armed\n", hits);
            OutputDebugStringA(dbg);
        }

        void ActivateGrandmastersMight(CWorld& world, EntityID caster)
        {
            if (!world.HasComponent<JaxStateComponent>(caster)) return;

            auto& js = world.GetComponent<JaxStateComponent>(caster);
            js.bUltActive = true;
            js.fUltTimer = js.fUltDurationSec;
            js.ultAttackCounter = 0;

            OutputDebugStringA("[Jax R] Grandmaster's Might activated (8s)\n");
        }
    }

    void OnCastFrame_BA(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand) return;
        ApplyBasicAttack(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam,
            ctx.pCommand->targetEntityId, true);
    }

    void OnCastFrame_Q(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand) return;
        ApplyLeapStrike(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam,
            ctx.pCommand->targetEntityId);
    }

    void OnCastFrame_W(SkillHookContext& ctx)
    {
        if (!ctx.pWorld) return;
        ArmEmpower(*ctx.pWorld, ctx.casterEntity);
    }

    void OnCastFrame_E(SkillHookContext& ctx)
    {
        if (!ctx.pWorld) return;
        ActivateCounterStrike(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam);
    }

    void OnCastFrame_R(SkillHookContext& ctx)
    {
        if (!ctx.pWorld) return;
        ActivateGrandmastersMight(*ctx.pWorld, ctx.casterEntity);
    }

    namespace Gameplay
    {
        void OnCastFrame_BA(GameplayHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand) return;
            ApplyBasicAttack(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam,
                ctx.pCommand->targetEntity, false);
        }

        void OnCastFrame_Q(GameplayHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand) return;
            ApplyLeapStrike(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam,
                ctx.pCommand->targetEntity);
        }

        void OnCastFrame_W(GameplayHookContext& ctx)
        {
            if (!ctx.pWorld) return;
            ArmEmpower(*ctx.pWorld, ctx.casterEntity);
        }

        void OnCastFrame_E(GameplayHookContext& ctx)
        {
            if (!ctx.pWorld) return;
            ActivateCounterStrike(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam);
        }

        void OnCastFrame_R(GameplayHookContext& ctx)
        {
            if (!ctx.pWorld) return;
            ActivateGrandmastersMight(*ctx.pWorld, ctx.casterEntity);
        }
    }

    namespace Visual
    {
        void OnCastFrame_BA_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand) return;

            const EntityID target = ctx.pCommand->targetEntityId;
            if (target == NULL_ENTITY) return;
            Fx::SpawnBAHitFlash(*ctx.pWorld, target, 0.4f);
        }

        void OnCastFrame_Q_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand) return;
            Fx::SpawnQLeapTrail(*ctx.pWorld, ctx.casterEntity,
                ctx.pCommand->direction, 0.4f, ctx.pFxMeshRenderer);
            Fx::SpawnBAHitFlash(*ctx.pWorld, ctx.pCommand->targetEntityId, 0.28f);
        }

        void OnCastFrame_W_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld) return;
            Fx::SpawnWEmpowerGlow(*ctx.pWorld, ctx.casterEntity, 4.0f);
        }

        void OnCastFrame_E_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld) return;
            Fx::SpawnECounterSlash(*ctx.pWorld, ctx.casterEntity, 2.0f,
                ctx.pFxMeshRenderer);
        }

        void OnCastFrame_R_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld) return;
            Fx::SpawnRBuffAura(*ctx.pWorld, ctx.casterEntity, 8.0f,
                ctx.pFxMeshRenderer);
        }
    }
}
