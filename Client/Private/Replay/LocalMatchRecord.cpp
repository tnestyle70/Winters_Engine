#include "Replay/LocalMatchRecord.h"

#pragma push_macro("new")
#undef new
#include "Network/Backend/json.hpp"
#pragma pop_macro("new")

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>

namespace
{
	constexpr const char* kLocalMatchHistoryPath = "Replay/LocalMatchHistory.jsonl";

	std::string MakeUtcTimestamp()
	{
		const std::time_t now = std::time(nullptr);
		std::tm utc{};
		gmtime_s(&utc, &now);

		char szBuffer[32]{};
		std::strftime(szBuffer, sizeof(szBuffer), "%Y-%m-%d %H:%M:%S", &utc);
		return szBuffer;
	}
}

namespace Winters
{
	bool_t AppendLocalMatchRecord(const LocalMatchRecord& record)
	{
		std::error_code ec;
		std::filesystem::create_directories("Replay", ec);

		std::ofstream stream(kLocalMatchHistoryPath, std::ios::app);
		if (!stream.is_open())
			return false;

		nlohmann::json recordJson;
		recordJson["utc"] = MakeUtcTimestamp();
		recordJson["user"] = record.strUser;
		recordJson["result"] = record.strResult;
		recordJson["end_tick"] = record.uEndTick;

		stream << recordJson.dump() << "\n";
		return stream.good();
	}

	std::vector<std::string> LoadLocalMatchRecordSummaries()
	{
		std::vector<std::string> summaries;

		std::ifstream stream(kLocalMatchHistoryPath);
		if (!stream.is_open())
			return summaries;

		std::string strLine;
		while (std::getline(stream, strLine))
		{
			if (strLine.empty())
				continue;

			try
			{
				const nlohmann::json recordJson = nlohmann::json::parse(strLine);
				char szBuffer[192]{};
				sprintf_s(szBuffer, sizeof(szBuffer), "[%s] %s - %s (tick %llu)",
					recordJson.value("utc", std::string{}).c_str(),
					recordJson.value("user", std::string{}).c_str(),
					recordJson.value("result", std::string{}).c_str(),
					static_cast<unsigned long long>(recordJson.value("end_tick", 0ull)));
				summaries.emplace_back(szBuffer);
			}
			catch (const nlohmann::json::exception&)
			{
				// 손상 라인은 건너뛴다 — 기록 파일 전체를 무효화하지 않는다.
			}
		}

		std::reverse(summaries.begin(), summaries.end());
		return summaries;
	}
}
