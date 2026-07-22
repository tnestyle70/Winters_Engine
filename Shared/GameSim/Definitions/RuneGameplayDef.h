#pragma once

#include "Shared/GameSim/Components/RuneComponent.h"
#include "Shared/GameSim/Definitions/DefinitionIds.h"
#include "WintersTypes.h"

struct RuneGameplayDef
{
    DefinitionKey key = kInvalidDefinitionKey;
    eRuneId legacyRuneId = eRuneId::None;
    bool_t bEnabled = false;
    u8_t maxStacks = 0u;
};
