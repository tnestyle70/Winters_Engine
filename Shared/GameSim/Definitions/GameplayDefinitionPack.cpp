#include "Shared/GameSim/Definitions/GameplayDefinitionPack.h"

namespace
{
    template <typename TDef, typename TId>
    const TDef* FindDense(const TDef* definitions, std::size_t count, TId id)
    {
        if (!definitions || !id.IsValid())
        {
            return nullptr;
        }

        const std::size_t index = static_cast<std::size_t>(id.value - 1u);
        if (index >= count || definitions[index].id.value != id.value)
        {
            return nullptr;
        }

        return &definitions[index];
    }

    template <typename TDef>
    const TDef* FindByKey(const TDef* definitions, std::size_t count, DefinitionKey key)
    {
        if (!definitions || key == kInvalidDefinitionKey)
        {
            return nullptr;
        }

        for (std::size_t index = 0u; index < count; ++index)
        {
            if (definitions[index].key == key)
            {
                return &definitions[index];
            }
        }

        return nullptr;
    }
}

const ChampionGameplayDef* GameplayDefinitionPack::FindChampion(ChampionDefId id) const
{
    return FindDense(champions, championCount, id);
}

const ChampionGameplayDef* GameplayDefinitionPack::FindChampion(DefinitionKey key) const
{
    return FindByKey(champions, championCount, key);
}

const ChampionGameplayDef* GameplayDefinitionPack::FindChampion(eChampion legacyChampion) const
{
    for (std::size_t index = 0u; index < championCount; ++index)
    {
        if (champions[index].legacyChampion == legacyChampion)
        {
            return &champions[index];
        }
    }

    return nullptr;
}

const SkillGameplayDef* GameplayDefinitionPack::FindSkill(SkillDefId id) const
{
    return FindDense(skills, skillCount, id);
}

const SkillGameplayDef* GameplayDefinitionPack::FindSkill(DefinitionKey key) const
{
    return FindByKey(skills, skillCount, key);
}

const SummonerSpellGameplayDef* GameplayDefinitionPack::FindSummonerSpell(SummonerSpellDefId id) const
{
    return FindDense(summonerSpellDefs, summonerSpellCount, id);
}

const SummonerSpellGameplayDef* GameplayDefinitionPack::FindSummonerSpellByLegacyId(u16_t legacySpellId) const
{
    for (std::size_t index = 0u; index < summonerSpellCount; ++index)
    {
        if (summonerSpellDefs[index].legacySpellId == legacySpellId)
        {
            return &summonerSpellDefs[index];
        }
    }

    return nullptr;
}
