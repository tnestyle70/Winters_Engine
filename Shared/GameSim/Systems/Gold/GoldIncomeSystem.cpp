#include "Shared/GameSim/Systems/Gold/GoldIncomeSystem.h"

#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/GoldComponent.h"
#include "Shared/GameSim/Core/World/World.h"
#include "Shared/GameSim/Definitions/GameplayDefinitionPack.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"

void CGoldIncomeSystem::Execute(CWorld& world, const TickContext& tc)
{
	u64_t startTick = kPassiveGoldStartTick;
	u64_t intervalTicks = kPassiveGoldGrantIntervalTicks;
	u32_t goldPerGrant = kPassiveGoldPerGrant;
	if (const EconomyGameplayDef* pEconomy =
		tc.pDefinitions ? tc.pDefinitions->FindEconomy() : nullptr)
	{
		startTick = pEconomy->passiveGoldStartTick;
		intervalTicks = pEconomy->passiveGoldIntervalTicks;
		goldPerGrant = pEconomy->passiveGoldPerGrant;
	}

	if (tc.tickIndex < startTick)
		return;
	if (intervalTicks == 0ull || (tc.tickIndex % intervalTicks) != 0ull)
		return;

	const auto entities =
		DeterministicEntityIterator<GoldComponent>::CollectSorted(world);
	for (EntityID entity : entities)
	{
		if (!world.HasComponent<ChampionComponent>(entity))
			continue;

		world.GetComponent<GoldComponent>(entity).amount += goldPerGrant;
	}
}
