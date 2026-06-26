#pragma once

#include "ECS/Entity.h"
#include "LoLMatchContext.h"
#include "Shared/GameSim/Definitions/SkillAtomData.h"
#include "WintersTypes.h"

class CWorld;
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

    f32_t ResolveSkillEffectParam(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot,
        eSkillEffectParamId param,
        f32_t fallbackValue = 0.f);

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
