#pragma once

#include "Shared/GameSim/Definitions/ChampionGameplayDef.h"

struct SkillLoadoutComponent
{
    SkillDefId skills[kChampionSkillSlotCount] = {};
};
