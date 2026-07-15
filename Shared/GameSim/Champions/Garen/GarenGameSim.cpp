#include "Shared/GameSim/Champions/Garen/GarenGameSim.h"

#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Definitions/GameplayDefinitionQuery.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"

#include "Shared/GameSim/Core/Ecs/TransformComponent.h"
#include "Shared/GameSim/Core/World/World.h"

namespace
{
    constexpr u8_t kGarenRSlot = static_cast<u8_t>(eSkillSlot::R);

    bool_t IsAliveChampion(CWorld& world, EntityID entity)
    {
        if (entity == NULL_ENTITY ||
            !world.IsAlive(entity) ||
            !world.HasComponent<ChampionComponent>(entity) ||
            !world.HasComponent<HealthComponent>(entity))
        {
            return false;
        }

        const auto& health = world.GetComponent<HealthComponent>(entity);
        return !health.bIsDead && health.fCurrent > 0.f;
    }

    void OnR(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pTickCtx || !ctx.pCommand)
            return;

        CWorld& world = *ctx.pWorld;
        const EntityID target = ctx.pCommand->targetEntity;
        if (!GarenGameSim::CanCastDemacianJustice(
            world,
            *ctx.pTickCtx,
            ctx.casterEntity,
            target))
        {
            return;
        }

        const u8_t rank = ctx.skillRank > 0u ? ctx.skillRank : 1u;
        const f32_t rankIndex = static_cast<f32_t>(rank - 1u);
        const f32_t baseDamage = GameplayDefinitionQuery::ResolveSkillEffectParam(
            world,
            ctx.casterEntity,
            *ctx.pTickCtx,
            eChampion::GAREN,
            kGarenRSlot,
            eSkillEffectParamId::BaseDamage,
            150.f);
        const f32_t damagePerRank = GameplayDefinitionQuery::ResolveSkillEffectParam(
            world,
            ctx.casterEntity,
            *ctx.pTickCtx,
            eChampion::GAREN,
            kGarenRSlot,
            eSkillEffectParamId::DamagePerRank,
            150.f);
        const f32_t missingHealthRatio = GameplayDefinitionQuery::ResolveSkillEffectParam(
            world,
            ctx.casterEntity,
            *ctx.pTickCtx,
            eChampion::GAREN,
            kGarenRSlot,
            eSkillEffectParamId::MissingHealthDamageRatio,
            0.25f);

        DamageRequest request{};
        request.source = ctx.casterEntity;
        request.target = target;
        request.sourceTeam = ctx.casterTeam;
        request.type = eDamageType::True;
        request.flatAmount = baseDamage + damagePerRank * rankIndex;
        request.targetMissingHpRatioOverride = missingHealthRatio;
        request.skillId = static_cast<u16_t>(
            (static_cast<u32_t>(eChampion::GAREN) << 8) | kGarenRSlot);
        request.rank = rank;
        request.iSourceSlot = kGarenRSlot;
        request.eSourceKind = eDamageSourceKind::Skill;
        EnqueueDamageRequest(world, request);
    }
}

namespace GarenGameSim
{
    void RegisterHooks()
    {
        static bool_t s_bRegistered = false;
        if (s_bRegistered)
            return;

        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::GAREN, GameplayHookVariant::R_CastFrame),
            &OnR);
        s_bRegistered = true;
    }

    bool_t CanCastDemacianJustice(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        EntityID target)
    {
        if (!IsAliveChampion(world, caster) ||
            !IsAliveChampion(world, target) ||
            !world.HasComponent<TransformComponent>(caster) ||
            !world.HasComponent<TransformComponent>(target) ||
            !GameplayStateQuery::CanReceiveDamage(world, caster, target))
        {
            return false;
        }

        const auto& casterChampion = world.GetComponent<ChampionComponent>(caster);
        const auto& targetChampion = world.GetComponent<ChampionComponent>(target);
        if (casterChampion.team == targetChampion.team)
            return false;

        f32_t range = GameplayDefinitionQuery::ResolveSkillRange(
            world,
            caster,
            tc,
            eChampion::GAREN,
            kGarenRSlot);
        if (range <= 0.f)
            range = 4.f;
        range += GameplayStateQuery::ResolveGameplayRadius(world, caster);
        range += GameplayStateQuery::ResolveGameplayRadius(world, target);

        const Vec3 casterPosition =
            world.GetComponent<TransformComponent>(caster).GetPosition();
        const Vec3 targetPosition =
            world.GetComponent<TransformComponent>(target).GetPosition();
        if (WintersMath::DistanceSqXZ(casterPosition, targetPosition) >
            range * range)
        {
            return false;
        }

        return !tc.pWalkable ||
            tc.pWalkable->SegmentWalkableXZ(casterPosition, targetPosition, 0.f);
    }
}
