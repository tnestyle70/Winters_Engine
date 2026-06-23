#pragma once

#include "WintersTypes.h"

using DefinitionKey = u32_t;

inline constexpr DefinitionKey kInvalidDefinitionKey = 0u;

struct ChampionDefId
{
    u16_t value = 0u;

    bool IsValid() const { return value != 0u; }
};

struct SkillDefId
{
    u16_t value = 0u;

    bool IsValid() const { return value != 0u; }
};

struct SummonerSpellDefId
{
    u16_t value = 0u;

    bool IsValid() const { return value != 0u; }
};

using GameplayPolicyId = u32_t;
using ReplicatedCueId = u32_t;
