#include "UI/AiTraceExport.h"

#include "Shared/GameSim/Components/ChampionAIComponent.h"
#include "Shared/GameSim/Core/World/World.h"

#pragma push_macro("new")
#undef new
#include "Network/Backend/json.hpp"
#pragma pop_macro("new")

#include <ctime>
#include <filesystem>
#include <fstream>

namespace
{
	std::string MakeUtcFileStamp()
	{
		const std::time_t now = std::time(nullptr);
		std::tm utc{};
		gmtime_s(&utc, &now);

		char szBuffer[32]{};
		std::strftime(szBuffer, sizeof(szBuffer), "%Y%m%d_%H%M%S", &utc);
		return szBuffer;
	}
}

namespace Winters
{
	bool_t ExportAiDecisionTraceJsonl(CWorld& world, std::string& outPath)
	{
		std::error_code ec;
		std::filesystem::create_directories("Replay/AITrace", ec);

		const std::string strPath = "Replay/AITrace/AITrace_" + MakeUtcFileStamp() + ".jsonl";
		std::ofstream stream(strPath, std::ios::trunc);
		if (!stream.is_open())
			return false;

		u32_t uRowsWritten = 0u;
		world.ForEach<ChampionAIDebugComponent>(
			[&](EntityID entity, ChampionAIDebugComponent& debug)
			{
				(void)entity;
				const u8_t count = debug.debugDecisionTraceCount <= kChampionAIDebugTraceCapacity
					? debug.debugDecisionTraceCount
					: kChampionAIDebugTraceCapacity;

				for (u8_t i = 0u; i < count; ++i)
				{
					const ChampionAIDecisionTraceEntry& row = debug.debugDecisionTrace[i];

					nlohmann::json rowJson;
					rowJson["netId"] = debug.netId;
					rowJson["tick"] = row.tick;
					rowJson["state"] = static_cast<u32_t>(row.state);
					rowJson["intent"] = static_cast<u32_t>(row.intent);
					rowJson["action"] = static_cast<u32_t>(row.action);
					rowJson["divePhase"] = static_cast<u32_t>(row.divePhase);
					rowJson["blockReason"] = static_cast<u32_t>(row.blockReason);
					rowJson["commandKind"] = static_cast<u32_t>(row.commandKind);
					rowJson["commandSlot"] = static_cast<u32_t>(row.commandSlot);
					rowJson["target"] = row.target;
					rowJson["commandPos"] = { row.commandPos.x, row.commandPos.y, row.commandPos.z };
					rowJson["championScore"] = row.championScore;
					rowJson["farmScore"] = row.farmScore;
					rowJson["structureScore"] = row.structureScore;
					rowJson["retreatScore"] = row.retreatScore;
					rowJson["selfHpRatio"] = row.selfHpRatio;
					rowJson["enemyHpRatio"] = row.enemyHpRatio;
					rowJson["enemyDistance"] = row.enemyDistance;
					rowJson["turretDanger"] = row.turretDanger;
					rowJson["legalMask"] = row.legalCandidateMask;
					rowJson["illegalMask"] = row.illegalCandidateMask;
					rowJson["commandSequence"] = row.commandSequence;
					rowJson["executorState"] = static_cast<u32_t>(row.executorState);
					rowJson["executorReason"] = static_cast<u32_t>(row.executorReason);
					rowJson["shadowStatus"] = static_cast<u32_t>(row.shadowStatus);
					rowJson["shadowActive"] = static_cast<u32_t>(row.shadowActiveCandidateKind);
					rowJson["shadowSelected"] = static_cast<u32_t>(row.shadowSelectedCandidateKind);
					rowJson["shadowDisagreed"] = row.bShadowDisagreed;
					rowJson["shadowPolicyRevision"] = row.shadowPolicyRevision;

					stream << rowJson.dump() << "\n";
					++uRowsWritten;
				}
			});

		if (!stream.good())
			return false;

		outPath = strPath;
		return uRowsWritten > 0u;
	}
}
