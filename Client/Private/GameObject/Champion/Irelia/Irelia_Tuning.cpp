#include "GameObject/Champion/Irelia/Irelia_Tuning.h"

namespace Irelia
{
    namespace
    {
        IreliaTuning s_tuning{};
    }

    IreliaTuning& GetTuning()
    {
        return s_tuning;
    }

    void ResetTuning()
    {
        s_tuning = IreliaTuning{};
    }
}
