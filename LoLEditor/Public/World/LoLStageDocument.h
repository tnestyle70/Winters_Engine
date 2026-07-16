#pragma once

#include "Shared/GameSim/Definitions/StageData.h"

#include <string>

// CLoLStageDocument — Stage{N}.dat 문서 모델.
// 클라이언트 매니저 4종(Structure/Jungle/Minion/Bush) 없이
// Shared StageData를 직접 소유/편집/직렬화한다.
class CLoLStageDocument
{
public:
	bool_t LoadStage(i32_t stageIndex);
	bool_t SaveStage();
	void NewStage(i32_t stageIndex);

	// 원본 파일 바이트 vs 현재 문서 재직렬화 바이트 비교 (로드 직후 동일해야 함)
	bool_t VerifyRoundtrip(std::wstring& outMessage) const;

	Winters::Map::StageData& Data() { return m_data; }
	const Winters::Map::StageData& Data() const { return m_data; }

	i32_t StageIndex() const { return m_iStageIndex; }
	bool_t IsLoaded() const { return m_bLoaded; }
	bool_t IsDirty() const { return m_bDirty; }
	void MarkDirty() { m_bDirty = true; }

	static bool_t ResolveStagePath(i32_t stageIndex, wchar_t* pOutBuf, u32_t capacity);
	static bool_t ResolveNavGridPath(i32_t stageIndex, wchar_t* pOutBuf, u32_t capacity);

private:
	Winters::Map::StageData m_data;
	i32_t m_iStageIndex = 1;
	bool_t m_bLoaded = false;
	bool_t m_bDirty = false;
};
