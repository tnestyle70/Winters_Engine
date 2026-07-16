#pragma once
#include "Defines.h"

#include <string>
#include <vector>

namespace Winters
{
	struct LocalMatchRecord
	{
		std::string strUserID{};
		std::string strDisplayName{};
		std::string strMatchID{};
		std::string strResult{};   // "victory" | "defeat" | "aborted"
		u64_t uEndTick = 0;
	};

	// %LOCALAPPDATA%/Winters/Accounts/<user-id>/MatchHistory.jsonl에 append.
	// 정상 종료(넥서스 파괴)와 ESC 강제 종료 경로 모두 이 함수를 사용한다.
	bool_t AppendLocalMatchRecord(const LocalMatchRecord& record);

	// 지정 계정의 MyInfo 씬 표시용 요약 문자열 목록 (최신 기록이 앞).
	std::vector<std::string> LoadLocalMatchRecordSummaries(
		const std::string& strUserID);
}
