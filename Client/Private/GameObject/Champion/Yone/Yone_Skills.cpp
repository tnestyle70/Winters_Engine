#include "GameObject/Champion/Yone/Yone_Skills.h"
#include "GameObject/Champion/Yone/Yone_Components.h"
#include "GameObject/Champion/Yone/Yone_FxPresets.h"
#include "GameObject/Champion/Yone/Yone_MeshGroups.h"
#include "GamePlay/Systems/Damage.h"
#include "GameObject/FX/FxCuePlayer.h"

#include "ECS/World.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "ECS/Components/MeshGroupVisibilityComponent.h"
#include "ECS/Components/TransformComponent.h"

#include <Windows.h>
#include <cstdio>
#include <unordered_map>
#include <vector>

namespace Yone
{
    namespace
    {
        std::unordered_map<EntityID, std::vector<EntityHandle>>
            s_soulMarkFxByTarget;

        void ClearSoulMarkFx(CWorld& world, EntityID target)
        {
            const auto it = s_soulMarkFxByTarget.find(target);
            if (it == s_soulMarkFxByTarget.end())
                return;

            for (const EntityHandle handle : it->second)
            {
                if (world.IsAlive(handle))
                    world.DestroyEntity(handle);
            }
            s_soulMarkFxByTarget.erase(it);
        }

        void ShowSoulMarkFx(VisualHookContext& ctx, EntityID target)
        {
            if (!ctx.pWorld || target == NULL_ENTITY ||
                !ctx.pWorld->IsAlive(target))
            {
                return;
            }

            ClearSoulMarkFx(*ctx.pWorld, target);
            FxCueContext cue{};
            cue.attachTo = target;
            cue.vWorldPos = ctx.pWorld->HasComponent<TransformComponent>(target)
                ? ctx.pWorld->GetComponent<TransformComponent>(target).GetPosition()
                : Vec3{};
            cue.vForward = { 0.f, 0.f, 1.f };
            cue.pFxMeshRenderer = ctx.pFxMeshRenderer;
            cue.bOverrideLifetime = true;
            cue.fLifetimeOverride = ctx.fEffectLifetimeSec > 0.f
                ? ctx.fEffectLifetimeSec
                : 5.f;

            std::vector<EntityID> spawned;
            CFxCuePlayer::PlayAll(
                *ctx.pWorld,
                "Yone.E.SoulMark",
                cue,
                &spawned);
            std::vector<EntityHandle>& handles = s_soulMarkFxByTarget[target];
            handles.reserve(spawned.size());
            for (EntityID entity : spawned)
            {
                if (entity != NULL_ENTITY && ctx.pWorld->IsAlive(entity))
                    handles.push_back(ctx.pWorld->GetEntityHandle(entity));
            }
            if (handles.empty())
                s_soulMarkFxByTarget.erase(target);
        }

        void AddOrReplaceSoulRequest(CWorld& world, EntityID entity, const YoneSoulRequestComponent& req)
        {
            if (world.HasComponent<YoneSoulRequestComponent>(entity))
                world.GetComponent<YoneSoulRequestComponent>(entity) = req;
            else
                world.AddComponent<YoneSoulRequestComponent>(entity, req);
        }

        MeshGroupVisibilityComponent& EnsureVisibility(CWorld& world, EntityID entity)
        {
            if (!world.HasComponent<MeshGroupVisibilityComponent>(entity))
                return world.AddComponent<MeshGroupVisibilityComponent>(entity);
            return world.GetComponent<MeshGroupVisibilityComponent>(entity);
        }

        Vec3 GetCasterPosition(CWorld& world, EntityID entity)
        {
            if (!world.HasComponent<TransformComponent>(entity))
                return {};
            return world.GetComponent<TransformComponent>(entity).GetPosition();
        }

        void ApplyBasicAttack(CWorld& world, EntityID caster, eTeam casterTeam,
                              EntityID target)
        {
            if (target == NULL_ENTITY)
                return;

            ApplyDamage(world, caster, casterTeam, target, 55.f);
        }

        void ApplyMortalSteel(CWorld& world, EntityID caster, eTeam casterTeam,
                              EntityID target)
        {
            if (target != NULL_ENTITY)
                ApplyDamage(world, caster, casterTeam, target, 75.f);

        }

        void ApplySpiritCleave()
        {
        }

        void ApplySoulUnbound(CWorld& world, EntityID caster, const Vec3& direction)
        {
            if (!world.HasComponent<YoneStateComponent>(caster))
                world.AddComponent<YoneStateComponent>(caster);

            auto& state = world.GetComponent<YoneStateComponent>(caster);

            if (!state.bEActive)
            {
                const Vec3 origin = GetCasterPosition(world, caster);
                const Vec3 dir = WintersMath::NormalizeXZ(direction);

                state.bEActive = true;
                state.fEDurationSec = 5.f;
                state.fETimer = state.fEDurationSec;
                state.vOriginalPosition = origin;

                auto& visibility = EnsureVisibility(world, caster);
                visibility.mask = MeshGroups::MaskBaseDefault();
                visibility.bEnabled = true;

                YoneSoulRequestComponent req{};
                req.action = eYoneSoulRequestAction::Spawn;
                req.fDurationSec = state.fEDurationSec;
                req.vSpawnPosition = {
                    origin.x + dir.x * 4.f,
                    origin.y,
                    origin.z + dir.z * 4.f
                };
                AddOrReplaceSoulRequest(world, caster, req);

            }
            else
            {
                YoneSoulRequestComponent req{};
                req.action = eYoneSoulRequestAction::Despawn;
                req.fDurationSec = 0.f;
                AddOrReplaceSoulRequest(world, caster, req);

            }
        }

        void ApplyFateSealed(CWorld& world, EntityID caster, eTeam casterTeam,
                             EntityID target)
        {
            if (target != NULL_ENTITY)
                ApplyDamage(world, caster, casterTeam, target, 150.f);

        }

        Vec3 ResolveVisualPosition(CWorld& world, EntityID entity)
        {
            if (entity != NULL_ENTITY && world.HasComponent<TransformComponent>(entity))
                return world.GetComponent<TransformComponent>(entity).GetPosition();
            return {};
        }

        EntityID ResolveVisualTarget(const VisualHookContext& ctx)
        {
            if (ctx.pCommand && ctx.pCommand->targetEntityId != NULL_ENTITY)
                return ctx.pCommand->targetEntityId;
            return ctx.casterEntity;
        }

        Vec3 ResolveVisualDirection(const VisualHookContext& ctx)
        {
            if (ctx.pCommand)
            {
                Vec3 dir = WintersMath::NormalizeXZ(ctx.pCommand->direction);
                if (dir.x != 0.f || dir.z != 0.f)
                    return dir;

                if (ctx.pWorld && ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
                {
                    const Vec3 origin =
                        ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
                    dir = WintersMath::NormalizeXZ(Vec3{
                        ctx.pCommand->groundPos.x - origin.x,
                        0.f,
                        ctx.pCommand->groundPos.z - origin.z
                    });
                    if (dir.x != 0.f || dir.z != 0.f)
                        return dir;
                }
            }

            return { 0.f, 0.f, 1.f };
        }
    }

    void OnCastFrame_BA(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand)
            return;

        ApplyBasicAttack(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam,
            ctx.pCommand->targetEntityId);
    }

    void OnCastFrame_Q(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand)
            return;

        ApplyMortalSteel(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam,
            ctx.pCommand->targetEntityId);
    }

    void OnCastFrame_W(SkillHookContext& ctx)
    {
        if (!ctx.pWorld)
            return;

        ApplySpiritCleave();
    }

    void OnCastFrame_E(SkillHookContext& ctx)
    {
        if (!ctx.pWorld)
            return;

        const Vec3 dir = ctx.pCommand ? ctx.pCommand->direction : Vec3{ 0.f, 0.f, 1.f };
        ApplySoulUnbound(*ctx.pWorld, ctx.casterEntity, dir);
    }

    void OnCastFrame_R(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand)
            return;

        ApplyFateSealed(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam,
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
            ApplyMortalSteel(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam,
                ctx.pCommand->targetEntity);
        }

        void OnCastFrame_W(GameplayHookContext& ctx)
        {
            if (!ctx.pWorld) return;
            ApplySpiritCleave();
        }

        void OnCastFrame_E(GameplayHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand) return;
            ApplySoulUnbound(*ctx.pWorld, ctx.casterEntity,
                ctx.pCommand->direction);
        }

        void OnCastFrame_R(GameplayHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand) return;
            ApplyFateSealed(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam,
                ctx.pCommand->targetEntity);
        }
    }

    namespace Visual
    {
        void ClearSoulMarkFxRuntime(CWorld& world)
        {
            for (const auto& [target, handles] : s_soulMarkFxByTarget)
            {
                (void)target;
                for (const EntityHandle handle : handles)
                {
                    if (world.IsAlive(handle))
                        world.DestroyEntity(handle);
                }
            }
            s_soulMarkFxByTarget.clear();
        }

        void OnCastFrame_BA_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld)
                return;

            const EntityID target = ResolveVisualTarget(ctx);
            YoneFx::SpawnBasicAttackImpact(*ctx.pWorld, target,
                ResolveVisualPosition(*ctx.pWorld, target), ResolveVisualDirection(ctx));
        }

        void OnCastFrame_Q_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld)
                return;

            const Vec3 origin = ResolveVisualPosition(*ctx.pWorld, ctx.casterEntity);
            const Vec3 dir = ResolveVisualDirection(ctx);
            YoneFx::SpawnMortalSteel(*ctx.pWorld, ctx.pFxMeshRenderer,
                ctx.casterEntity, origin, dir);
        }

        void OnCastFrame_W_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld)
                return;

            YoneFx::SpawnSpiritCleave(*ctx.pWorld, ctx.pFxMeshRenderer,
                ctx.casterEntity, ResolveVisualPosition(*ctx.pWorld, ctx.casterEntity),
                ResolveVisualDirection(ctx));
        }

        void OnCastFrame_E_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld)
                return;

            const EntityID target = ResolveVisualTarget(ctx);
            if (ctx.skillStage == 3u)
            {
                if (target != ctx.casterEntity)
                    ShowSoulMarkFx(ctx, target);
                return;
            }
            if (ctx.skillStage == 4u)
            {
                ClearSoulMarkFx(*ctx.pWorld, target);
                return;
            }

            if (ctx.skillStage >= 2u)
            {
                YoneSoulRequestComponent req{};
                req.action = eYoneSoulRequestAction::Despawn;
                req.fDurationSec = 0.f;
                AddOrReplaceSoulRequest(*ctx.pWorld, ctx.casterEntity, req);

                YoneFx::SpawnSoulReturn(*ctx.pWorld, ctx.pFxMeshRenderer,
                    ctx.casterEntity, ResolveVisualPosition(*ctx.pWorld, ctx.casterEntity),
                    ResolveVisualDirection(ctx));
                return;
            }

            if (!ctx.pWorld->HasComponent<YoneStateComponent>(ctx.casterEntity))
                ctx.pWorld->AddComponent<YoneStateComponent>(ctx.casterEntity);

            auto& state = ctx.pWorld->GetComponent<YoneStateComponent>(ctx.casterEntity);
            state.bEActive = true;
            state.fEDurationSec = 5.f;
            state.fETimer = state.fEDurationSec;
            state.vOriginalPosition = ResolveVisualPosition(*ctx.pWorld, ctx.casterEntity);

            YoneSoulRequestComponent req{};
            req.action = eYoneSoulRequestAction::Spawn;
            req.fDurationSec = state.fEDurationSec;
            req.vSpawnPosition = state.vOriginalPosition;
            AddOrReplaceSoulRequest(*ctx.pWorld, ctx.casterEntity, req);

            YoneFx::SpawnSoulOut(*ctx.pWorld, ctx.pFxMeshRenderer,
                ctx.casterEntity, state.vOriginalPosition, ResolveVisualDirection(ctx));
        }

        void OnCastFrame_R_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld)
                return;

            const EntityID target = ResolveVisualTarget(ctx);
            const Vec3 origin = ResolveVisualPosition(*ctx.pWorld, ctx.casterEntity);
            const Vec3 dir = ResolveVisualDirection(ctx);
            YoneFx::SpawnFateSealed(*ctx.pWorld, ctx.pFxMeshRenderer,
                ctx.casterEntity, target, origin, dir);
        }
    }
}
