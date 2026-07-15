#pragma once
#include "Defines.h"

#include <string>
#include <vector>

namespace Winters
{
	struct LocalMatchRecord
	{
		std::string strUser{};
		std::string strResult{};   // "victory" | "defeat" | "aborted"
		u64_t uEndTick = 0;
	};

	// Replay/LocalMatchHistory.jsonl 에 UTC 타임스탬프와 함께 한 줄 append.
	// 정상 종료(넥서스 파괴)와 ESC 강제 종료 경로 모두 이 함수를 사용한다.
	bool_t AppendLocalMatchRecord(const LocalMatchRecord& record);

	// MyInfo 씬 표시용 요약 문자열 목록 (최신 기록이 앞).
	std::vector<std::string> LoadLocalMatchRecordSummaries();
}
