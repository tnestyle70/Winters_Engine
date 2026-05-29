#pragma once

#include "WintersTypes.h"

class CWorld;
class CScene_InGame;

namespace UI
{
	class CAIDebugPanel final
	{
	public:
		static void Render(CWorld& world, CScene_InGame* pScene);
	};
}
