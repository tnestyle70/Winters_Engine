#include "GameObject/Champion/Yasuo/Yasuo_Tuning.h"

namespace Yasuo
{
    namespace
    {
        YasuoTuning s_tuning{};
    }

    YasuoTuning& GetTuning()
    {
        return s_tuning;
    }

    void ResetTuning()
    {
        s_tuning = YasuoTuning{};
    }
}
