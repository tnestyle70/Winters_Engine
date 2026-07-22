#pragma once

#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/ChampionAI/ChampionAIResearchTypes.h"

#include <vector>

class CWorld;
enum class eTeam : uint8_t;
struct ChampionAIComponent;
struct ChampionAIPerception;
struct ChampionAIShadowPolicyArtifactV1;

class CChampionAISystem final
{
public:
	static Vec3 ResolveSafeAnchor(
		CWorld& world,
		eTeam team,
		u8_t lane,
		const Vec3& fallback);

	static void Execute(CWorld& world,
		const TickContext& tc,
		std::vector<GameCommand>& outCommands,
		const ChampionAIShadowPolicyArtifactV1* pShadowPolicy = nullptr);

	static AiDecisionTraceV1 BuildResearchDecisionTrace(
		const TickContext& tc,
		EntityID self,
		const ChampionAIComponent& ai,
		const ChampionAIPerception& perception);

	static f32_t EstimateObservedEnemyComboDamageRatio(
		CWorld& world,
		const TickContext& tc,
		EntityID self,
		EntityID enemy,
		f32_t enemyDistance);

private:
	CChampionAISystem() = delete;
};
