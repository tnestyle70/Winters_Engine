#pragma once
#include "Defines.h"

#include <string>

class CWorld;

namespace Winters
{
	// 월드의 모든 ChampionAIDebugComponent decision trace 링을
	// Replay/AITrace/AITrace_<UTC>.jsonl 로 덤프한다.
	// 트리거 3종 공용: 정상 종료(넥서스 파괴) / AI Debug 수동 저장 / ESC·씬 이탈.
	// 표현 데이터 덤프이므로 게임플레이 truth 비변이.
	bool_t ExportAiDecisionTraceJsonl(CWorld& world, std::string& outPath);
}
