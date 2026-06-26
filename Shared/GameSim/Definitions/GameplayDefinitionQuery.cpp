#include "Shared/GameSim/Definitions/GameplayDefinitionQuery.h"

#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Components/ChampionDefinitionComponent.h"
#include "Shared/GameSim/Components/SkillLoadoutComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Core/Determinism/DeterministicTime.h"
#include "Shared/GameSim/Core/World/World.h"
#include "Shared/GameSim/Definitions/GameplayDefinitionPack.h"
#include "Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"

#include <algorithm>
#include <cmath>

namespace
{
    const GameplayDefinitionPack* GetPack(const TickContext& tc)
    {
        return tc.pDefinitions;
    }

    eChampion ResolveChampionFallback(CWorld& world, EntityID entity, eChampion fallbackChampion)
    {
        if (fallbackChampion != eChampion::NONE && fallbackChampion != eChampion::END)
        {
            return fallbackChampion;
        }

        if (entity != NULL_ENTITY && world.HasComponent<StatComponent>(entity))
        {
            return world.GetComponent<StatComponent>(entity).championId;
        }

        if (entity != NULL_ENTITY && world.HasComponent<ChampionComponent>(entity))
        {
            return world.GetComponent<ChampionComponent>(entity).id;
        }

        return fallbackChampion;
    }

    u64_t ResolveTicksFromSeconds(f32_t seconds)
    {
        const f32_t sanitizedSeconds = std::isfinite(seconds) && seconds > 0.f ? seconds : 0.f;
        const f64_t ticks =
            static_cast<f64_t>(sanitizedSeconds) *
            static_cast<f64_t>(DeterministicTime::kTicksPerSecond);
        const u64_t roundedUp = static_cast<u64_t>(std::ceil(ticks));
        return roundedUp > 0u ? roundedUp : 1u;
    }

    const SkillGameplayDef* FindSkillInChampion(
        const GameplayDefinitionPack& pack,
        const ChampionGameplayDef& champion,
        u8_t slot)
    {
        if (slot >= kChampionSkillSlotCount)
        {
            return nullptr;
        }

        return pack.FindSkill(champion.skillLoadout[slot]);
    }

    const PassiveDashGameplayDef* FindPassiveDash(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion)
    {
        const ChampionGameplayDef* champion =
            GameplayDefinitionQuery::FindChampion(world, entity, tc, fallbackChampion);
        if (!champion || !champion->passiveDash.bValid)
        {
            return nullptr;
        }

        return &champion->passiveDash;
    }
}

namespace GameplayDefinitionQuery
{
    const ChampionGameplayDef* FindChampion(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion)
    {
        const GameplayDefinitionPack* pack = GetPack(tc);
        if (!pack)
        {
            return nullptr;
        }

        if (entity != NULL_ENTITY && world.HasComponent<ChampionDefinitionComponent>(entity))
        {
            if (const ChampionGameplayDef* champion =
                pack->FindChampion(world.GetComponent<ChampionDefinitionComponent>(entity).championDefId))
            {
                return champion;
            }
        }

        const eChampion champion = ResolveChampionFallback(world, entity, fallbackChampion);
        return pack->FindChampion(champion);
    }

    const SkillGameplayDef* FindSkill(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot)
    {
        const GameplayDefinitionPack* pack = GetPack(tc);
        if (!pack || slot >= kChampionSkillSlotCount)
        {
            return nullptr;
        }

        if (entity != NULL_ENTITY && world.HasComponent<SkillLoadoutComponent>(entity))
        {
            if (const SkillGameplayDef* skill =
                pack->FindSkill(world.GetComponent<SkillLoadoutComponent>(entity).skills[slot]))
            {
                return skill;
            }
        }

        if (const ChampionGameplayDef* champion = FindChampion(world, entity, tc, fallbackChampion))
        {
            return FindSkillInChampion(*pack, *champion, slot);
        }

        return nullptr;
    }

    f32_t ResolveAttackRange(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion)
    {
        if (entity != NULL_ENTITY && world.HasComponent<StatComponent>(entity))
        {
            const f32_t range = world.GetComponent<StatComponent>(entity).attackRange;
            if (range > 0.f)
            {
                return range;
            }
        }

        if (const ChampionGameplayDef* champion = FindChampion(world, entity, tc, fallbackChampion))
        {
            return champion->stats.baseAttackRange;
        }

        return ChampionGameDataDB::BuildStat(
            ResolveChampionFallback(world, entity, fallbackChampion)).attackRange;
    }

    f32_t ResolveSkillRange(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot)
    {
        if (const SkillGameplayDef* skill = FindSkill(world, entity, tc, fallbackChampion, slot))
        {
            return skill->range.rangeMax;
        }

        if (slot == static_cast<u8_t>(eSkillSlot::BasicAttack))
        {
            return ResolveAttackRange(world, entity, tc, fallbackChampion);
        }

        return ChampionGameDataDB::ResolveSkillRange(
            ResolveChampionFallback(world, entity, fallbackChampion),
            slot);
    }

    f32_t ResolveSkillCooldown(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot)
    {
        if (const SkillGameplayDef* skill = FindSkill(world, entity, tc, fallbackChampion, slot))
        {
            return skill->cooldown.cooldownSec;
        }

        return ChampionGameDataDB::ResolveSkillCooldown(
            ResolveChampionFallback(world, entity, fallbackChampion),
            slot);
    }

    f32_t ResolveSkillEffectParam(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot,
        eSkillEffectParamId param,
        f32_t fallbackValue)
    {
        if (const SkillGameplayDef* skill = FindSkill(world, entity, tc, fallbackChampion, slot))
        {
            return ::ResolveSkillEffectParam(skill->effect, param, fallbackValue);
        }

        return fallbackValue;
    }

    f32_t ResolveSummonPolicyParam(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot,
        eSummonPolicyParamId param,
        f32_t fallbackValue)
    {
        if (const SkillGameplayDef* skill = FindSkill(world, entity, tc, fallbackChampion, slot))
        {
            return ::ResolveSummonPolicyParam(skill->summonPolicy, param, fallbackValue);
        }

        return fallbackValue;
    }

    u64_t ResolveSkillActionLockTicks(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot,
        u8_t stage)
    {
        if (const SkillGameplayDef* skill = FindSkill(world, entity, tc, fallbackChampion, slot))
        {
            const u8_t stageNumber = stage > 0u ? stage : 1u;
            const u8_t stageIndex = static_cast<u8_t>((std::min)(
                static_cast<u8_t>(stageNumber - 1u),
                static_cast<u8_t>(kSkillAtomStageMax - 1u)));
            return ResolveTicksFromSeconds(skill->stage.lockDurationSec[stageIndex]);
        }

        return ChampionGameDataDB::ResolveSkillActionLockTicks(
            ResolveChampionFallback(world, entity, fallbackChampion),
            slot,
            stage);
    }

    bool_t IsSkillTwoStage(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot)
    {
        if (const SkillGameplayDef* skill = FindSkill(world, entity, tc, fallbackChampion, slot))
        {
            return skill->stage.stageCount >= 2u;
        }

        return ChampionGameDataDB::IsSkillTwoStage(
            ResolveChampionFallback(world, entity, fallbackChampion),
            slot);
    }

    f32_t ResolveSkillStageWindowSec(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot)
    {
        if (const SkillGameplayDef* skill = FindSkill(world, entity, tc, fallbackChampion, slot))
        {
            return skill->stage.stageWindowSec;
        }

        return ChampionGameDataDB::ResolveSkillStageWindowSec(
            ResolveChampionFallback(world, entity, fallbackChampion),
            slot);
    }

    f32_t ResolveSummonerSpellRange(const TickContext& tc, u16_t legacySpellId)
    {
        if (const GameplayDefinitionPack* pack = GetPack(tc))
        {
            if (const SummonerSpellGameplayDef* spell =
                pack->FindSummonerSpellByLegacyId(legacySpellId))
            {
                return spell->rangeMax;
            }
        }

        return 0.f;
    }

    f32_t ResolveSummonerSpellCooldown(const TickContext& tc, u16_t legacySpellId)
    {
        if (const GameplayDefinitionPack* pack = GetPack(tc))
        {
            if (const SummonerSpellGameplayDef* spell =
                pack->FindSummonerSpellByLegacyId(legacySpellId))
            {
                return spell->cooldownSec;
            }
        }

        return 0.f;
    }

    f32_t ResolvePassiveDashDistance(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion)
    {
        if (const PassiveDashGameplayDef* dash = FindPassiveDash(world, entity, tc, fallbackChampion))
        {
            return dash->distance;
        }

        return ChampionGameDataDB::ResolvePassiveDashDistance(
            ResolveChampionFallback(world, entity, fallbackChampion));
    }

    f32_t ResolvePassiveDashDurationSec(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion)
    {
        if (const PassiveDashGameplayDef* dash = FindPassiveDash(world, entity, tc, fallbackChampion))
        {
            return dash->durationSec;
        }

        return ChampionGameDataDB::ResolvePassiveDashDurationSec(
            ResolveChampionFallback(world, entity, fallbackChampion));
    }

    f32_t ResolvePassiveDashInputGraceSec(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion)
    {
        if (const PassiveDashGameplayDef* dash = FindPassiveDash(world, entity, tc, fallbackChampion))
        {
            return dash->inputGraceSec;
        }

        return ChampionGameDataDB::ResolvePassiveDashInputGraceSec(
            ResolveChampionFallback(world, entity, fallbackChampion));
    }

    u64_t ResolvePassiveDashInputGraceTicks(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion)
    {
        if (const PassiveDashGameplayDef* dash = FindPassiveDash(world, entity, tc, fallbackChampion))
        {
            return ResolveTicksFromSeconds(dash->inputGraceSec);
        }

        return ChampionGameDataDB::ResolvePassiveDashInputGraceTicks(
            ResolveChampionFallback(world, entity, fallbackChampion));
    }
}
