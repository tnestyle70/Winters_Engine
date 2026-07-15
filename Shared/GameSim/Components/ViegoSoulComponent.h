#pragma once

#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "../Definitions/LoLMatchContext.h"
#include "WintersTypes.h"

struct ViegoSoulComponent
{
	EntityID deadChampion = NULL_ENTITY;
	EntityID eligibleViego = NULL_ENTITY;
	eChampion champion = eChampion::END;
	eTeam eligibleTeam = eTeam::TEAM_END;
	u8_t skillRanks[SkillRankComponent::kSlotCount] = {};
	f32_t fRemainingSec = 5.f;
	bool_t bHasSkillRanks = false;
};
