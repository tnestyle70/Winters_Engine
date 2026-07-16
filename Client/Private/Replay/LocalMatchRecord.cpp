#include "Replay/LocalMatchRecord.h"
#include "Replay/ReplayLibrary.h"

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
		if (!CReplayLibrary::IsSafeAccountKey(record.strUserID))
			return false;

		const std::filesystem::path accountDirectory(
			CReplayLibrary::GetAccountDataDirectory(record.strUserID));
		if (accountDirectory.empty())
			return false;

		std::error_code ec;
		std::filesystem::create_directories(accountDirectory, ec);
		if (ec)
			return false;

		std::ofstream stream(accountDirectory / L"MatchHistory.jsonl", std::ios::app);
		if (!stream.is_open())
			return false;

		nlohmann::json recordJson;
		recordJson["utc"] = MakeUtcTimestamp();
		recordJson["user_id"] = record.strUserID;
		recordJson["display_name"] = record.strDisplayName;
		recordJson["match_id"] = record.strMatchID;
		recordJson["result"] = record.strResult;
		recordJson["end_tick"] = record.uEndTick;

		stream << recordJson.dump() << "\n";
		return stream.good();
	}

	std::vector<std::string> LoadLocalMatchRecordSummaries(
		const std::string& strUserID)
	{
		std::vector<std::string> summaries;
		if (!CReplayLibrary::IsSafeAccountKey(strUserID))
			return summaries;

		const std::filesystem::path accountDirectory(
			CReplayLibrary::GetAccountDataDirectory(strUserID));
		if (accountDirectory.empty())
			return summaries;

		std::ifstream stream(accountDirectory / L"MatchHistory.jsonl");
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
				if (recordJson.value("user_id", std::string{}) != strUserID)
					continue;

				char szBuffer[256]{};
				sprintf_s(szBuffer, sizeof(szBuffer), "[%s] %s - %s (match %s, tick %llu)",
					recordJson.value("utc", std::string{}).c_str(),
					recordJson.value("display_name", std::string{}).c_str(),
					recordJson.value("result", std::string{}).c_str(),
					recordJson.value("match_id", std::string{"local"}).c_str(),
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
