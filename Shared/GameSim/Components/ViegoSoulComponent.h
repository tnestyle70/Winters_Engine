#pragma once

#include "ECS/Entity.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "../Definitions/LoLMatchContext.h"
#include "WintersTypes.h"

struct ViegoSoulComponent
{
	EntityID deadChampion = NULL_ENTITY;
	eChampion champion = eChampion::END;
	eTeam eligibleTeam = eTeam::TEAM_END;
	f32_t fRemainingSec = 5.f;
};
