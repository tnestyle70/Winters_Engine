#pragma once

#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "Shared/GameSim/Components/ActionStateComponent.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "LoLMatchContext.h"
#include "Shared/GameSim/Definitions/SkillAtomData.h"
#include "WintersTypes.h"

class CWorld;
struct DamageRequest;
struct ChampionGameplayDef;
struct SkillGameplayDef;
struct TickContext;

namespace GameplayDefinitionQuery
{
    const ChampionGameplayDef* FindChampion(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion);

    const SkillGameplayDef* FindSkill(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot);

    f32_t ResolveAttackRange(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion);

    f32_t ResolveSkillRange(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot);

    f32_t ResolveSkillCooldown(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot);

    f32_t ResolveSkillManaCost(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot);

    f32_t ResolveSkillEffectParam(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot,
        eSkillEffectParamId param,
        f32_t fallbackValue = 0.f);

    f32_t ResolveSkillEffectParamRanked(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot,
        u8_t rank,
        eSkillEffectParamId param,
        f32_t fallbackValue = 0.f);

    f32_t ResolveSkillFlatDamage(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot,
        u8_t rank,
        f32_t fallbackValue = 0.f);

    f32_t ResolveSkillTargetMissingHpRatio(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot,
        u8_t rank,
        f32_t fallbackValue = 0.f);

    bool_t BuildSkillDamageRequest(
        CWorld& world,
        EntityID source,
        EntityID target,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot,
        u8_t rank,
        eTeam sourceTeam,
        eDamageSourceKind sourceKind,
        DamageRequest& outRequest);

    f32_t ResolveSummonPolicyParam(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot,
        eSummonPolicyParamId param,
        f32_t fallbackValue = 0.f);

    u64_t ResolveSkillActionLockTicks(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot,
        u8_t stage = 1u);

    eSkillActionMovePolicy ResolveSkillActionMovePolicy(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot,
        u8_t stage = 1u);

    bool_t ShouldCreateSkillActionState(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot,
        u8_t stage = 1u);

    bool_t IsSkillTwoStage(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot);

    f32_t ResolveSkillStageWindowSec(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot);

    f32_t ResolveSummonerSpellRange(const TickContext& tc, u16_t legacySpellId);
    f32_t ResolveSummonerSpellCooldown(const TickContext& tc, u16_t legacySpellId);

    f32_t ResolvePassiveDashDistance(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion);

    f32_t ResolvePassiveDashDurationSec(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion);

    f32_t ResolvePassiveDashInputGraceSec(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion);

    u64_t ResolvePassiveDashInputGraceTicks(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion);
}
