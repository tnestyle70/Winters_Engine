#pragma once
#include "WintersTypes.h"

class CWorld;
class CScene_InGame;

namespace UI
{
	//Scene_InGame의 CombatDebug를 분리
	class CCombatDebugPanel
	{
	public:
		static void Render(CWorld& world, CScene_InGame* pScene);
	};
}