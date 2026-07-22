#pragma once

#include "WintersTypes.h"

class CScene_InGame;

namespace UI
{
	enum class eBalanceTunerCategory : u8_t
	{
		Champions = 0u,
		Skills,
		Minions,
		Towers,
		Objectives,
	};

	class CChampionTuner
	{
	public:
		static void Open(
			eBalanceTunerCategory category = eBalanceTunerCategory::Champions);
		static void Render(CScene_InGame* pScene);
	};
}
