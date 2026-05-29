#include "GameObject/Champion/Kalista/Kalista_Tuning.h"

namespace Kalista
{
    namespace
    {
        KalistaTuning s_tuning{};
    }

    KalistaTuning& GetTuning()
    {
        return s_tuning;
    }

    void ResetTuning()
    {
        s_tuning = KalistaTuning{};
    }
}
