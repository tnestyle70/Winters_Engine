#pragma once

#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "WintersTypes.h"

struct ChampionAssistCreditComponent
{
	static constexpr u8_t kMaxCredits = 8;

	struct Credit
	{
		EntityID SourceEntity = NULL_ENTITY;
		u64_t iLastDamageTick = 0;
		u8_t iSourceTeam = 0;
	};

	Credit Credits[kMaxCredits]{};
};