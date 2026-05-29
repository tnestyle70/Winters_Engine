#pragma once

#include "ECS/Entity.h"
#include "ECS/Components/GameplayComponents.h"
#include "GameContext.h"
#include "WintersTypes.h"

struct ViegoSoulComponent
{
	EntityID deadChampion = NULL_ENTITY;
	eChampion champion = eChampion::END;
	eTeam eligibleTeam = eTeam::TEAM_END;
	f32_t fRemainingSec = 5.f;
};
