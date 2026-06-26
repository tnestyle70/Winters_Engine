#pragma once

#include <cstddef>

#include "Shared/GameSim/Definitions/ChampionGameplayDef.h"
#include "Shared/GameSim/Definitions/DataPackManifest.h"
#include "Shared/GameSim/Definitions/SkillGameplayDef.h"
#include "Shared/GameSim/Definitions/SummonerSpellGameplayDef.h"

struct GameplayDefinitionPack
{
    DataPackManifest manifest{};
    const ChampionGameplayDef* champions = nullptr;
    std::size_t championCount = 0u;
    const SkillGameplayDef* skills = nullptr;
    std::size_t skillCount = 0u;
    const SummonerSpellGameplayDef* summonerSpellDefs = nullptr;
    std::size_t summonerSpellCount = 0u;

    const ChampionGameplayDef* FindChampion(ChampionDefId id) const;
    const ChampionGameplayDef* FindChampion(DefinitionKey key) const;
    const ChampionGameplayDef* FindChampion(eChampion legacyChampion) const;
    const SkillGameplayDef* FindSkill(SkillDefId id) const;
    const SkillGameplayDef* FindSkill(DefinitionKey key) const;
    const SummonerSpellGameplayDef* FindSummonerSpell(SummonerSpellDefId id) const;
    const SummonerSpellGameplayDef* FindSummonerSpellByLegacyId(u16_t legacySpellId) const;
};
