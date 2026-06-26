#include "GamePlay/LoLMatchContextRuntime.h"

namespace Client
{
    CLoLMatchContextRuntime& CLoLMatchContextRuntime::Instance()
    {
        static CLoLMatchContextRuntime s_Instance;
        return s_Instance;
    }
}
