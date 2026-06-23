#pragma once

#include "Shared/GameSim/Definitions/DefinitionIds.h"
#include "WintersTypes.h"

struct SummonerSpellGameplayDef
{
    DefinitionKey key = kInvalidDefinitionKey;
    SummonerSpellDefId id{};
    u16_t legacySpellId = 0u;
    f32_t rangeMax = 0.f;
    f32_t cooldownSec = 0.f;
    GameplayPolicyId gameplayPolicyId = 0u;
    ReplicatedCueId replicatedCueId = 0u;
};
