#pragma once

#include "Shared/GameSim/Definitions/GameplayDefinitionPack.h"
#include "Shared/GameSim/Definitions/SpawnObjectDefinitionPack.h"

namespace ServerData
{
    const GameplayDefinitionPack& GetLoLGameplayDefinitionPack();
    const SpawnObjectDefinitionPack& GetLoLSpawnObjectDefinitionPack();
}
