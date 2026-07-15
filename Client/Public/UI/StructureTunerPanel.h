#pragma once

#include "WintersTypes.h"

class CScene_InGame;
class CWorld;

namespace UI
{
	// 구조물(포탑/억제기/넥서스) 체력 + 포탑 데미지 라이브 튜닝 패널 (F4).
	// ApplyStructureStatOverride practice op 으로 서버에 즉시 반영한다 (_DEBUG 서버 + 호스트 전용).
	class CStructureTunerPanel
	{
	public:
		CStructureTunerPanel() = delete;

		static void Render(CWorld& world, CScene_InGame* pScene);
	};
}
