#pragma once

#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "WintersTypes.h"

struct ChampionAssistCreditComponent
{
	static constexpr u8_t kMaxCredits = 8;

	struct Credit
	{
		EntityID SourceEntity = NULL_ENTITY;
		u32_t iReservedBeforeTick = 0;
		u64_t iLastDamageTick = 0;
		u8_t iSourceTeam = 0;
		u8_t iReservedAfterTeam[7]{};
	};
	static_assert(sizeof(Credit) == 24u);

	Credit Credits[kMaxCredits]{};
};
static_assert(sizeof(ChampionAssistCreditComponent) == 192u);
