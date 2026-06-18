#pragma once

#include "WintersTypes.h"

struct ChampionScoreComponent
{
	static constexpr u8_t kSummonerSpellSlotCount = 2;
	static constexpr u16_t kSummonerSpellFlash = 4;
	static constexpr u16_t kSummonerSpellIgnite = 14;

	u16_t iKills = 0;
	u16_t iDeaths = 0;
	u16_t iAssists = 0;
	u16_t iSummonerSpellIds[kSummonerSpellSlotCount] =
	{
		kSummonerSpellFlash,
		kSummonerSpellIgnite
	};
};

struct SummonerSpellStateComponent
{
	static constexpr u8_t kSlotCount = ChampionScoreComponent::kSummonerSpellSlotCount;

	f32_t cooldownRemaining[kSlotCount] = {};
	f32_t cooldownDuration[kSlotCount] = {};
};
