#pragma once

#include "Client/Private/Data/LoLVisualDefinitionPack.h"

#include <string>

namespace ClientData
{
    const ChampionVisualDefinition* FindRuntimeChampionVisualDefinition(eChampion champion);
    const ChampionVisualDefinition* FindRuntimeChampionVisualDefinition(DefinitionKey key);
    const ChampionModelVisualPack* GetRuntimeChampionModelVisualPack();
    const ChampionModelVisualDefinition* FindRuntimeChampionModelVisualDefinition(eChampion champion);
    const ChampionUiVisualDefinition* FindRuntimeChampionUiVisualDefinition(eChampion champion);

    bool_t TryReloadRuntimeVisualDefinitions(std::string& outError);
    void ClearRuntimeVisualDefinitions();
    u32_t GetRuntimeVisualDefinitionRevision();
}
