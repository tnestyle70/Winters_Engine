#pragma once

#include <cstddef>

#include "Shared/GameSim/Definitions/ChampionGameplayDef.h"
#include "Shared/GameSim/Definitions/DataPackManifest.h"
#include "Shared/GameSim/Definitions/EconomyGameplayDef.h"
#include "Shared/GameSim/Definitions/ItemDef.h"
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
    // aggregate init 호환을 위해 항상 맨 뒤에 추가한다.
    const EconomyGameplayDef* economy = nullptr;
    const ItemDef* items = nullptr;
    std::size_t itemCount = 0u;

    const ChampionGameplayDef* FindChampion(ChampionDefId id) const;
    const ChampionGameplayDef* FindChampion(DefinitionKey key) const;
    const ChampionGameplayDef* FindChampion(eChampion legacyChampion) const;
    const SkillGameplayDef* FindSkill(SkillDefId id) const;
    const SkillGameplayDef* FindSkill(DefinitionKey key) const;
    const SummonerSpellGameplayDef* FindSummonerSpell(SummonerSpellDefId id) const;
    const SummonerSpellGameplayDef* FindSummonerSpellByLegacyId(u16_t legacySpellId) const;
    // 장착되고(bValid) 유효한 경제 정의만 반환. 없으면 nullptr = 레거시 상수 폴백.
    const EconomyGameplayDef* FindEconomy() const;
    // 장착된 아이템 정의 배열만 반환 (outCount 채움). 없으면 nullptr = 컴파일된 기본 표 폴백.
    const ItemDef* FindItems(std::size_t& outCount) const;
};
