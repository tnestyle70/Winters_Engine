#pragma once

#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/ChampionAI/ChampionAIResearchTypes.h"

#include <vector>

class CWorld;
struct ChampionAIComponent;
struct ChampionAIPerception;
struct ChampionAIShadowPolicyArtifactV1;

class CChampionAISystem final
{
public:
	static void Execute(CWorld& world,
		const TickContext& tc,
		std::vector<GameCommand>& outCommands,
		const ChampionAIShadowPolicyArtifactV1* pShadowPolicy = nullptr);

	static AiDecisionTraceV1 BuildResearchDecisionTrace(
		const TickContext& tc,
		EntityID self,
		const ChampionAIComponent& ai,
		const ChampionAIPerception& perception);

private:
	CChampionAISystem() = delete;
};
