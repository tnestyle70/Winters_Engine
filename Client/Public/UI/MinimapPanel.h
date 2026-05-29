#pragma once

#include "WintersTypes.h"

class CWorld;
class CFogOfWarRenderer;

namespace UI
{
    class CMinimapPanel
    {
    public:
        static void Render(CFogOfWarRenderer* pFow, CWorld& world, u8_t localTeam);
    };
}
