#pragma once

#include "GamePlay/SkillHookContext.h"

struct VisualHookContext;

namespace Riven
{
    void OnCastAccepted_Q(SkillHookContext& ctx);
    void OnCastFrame(SkillHookContext& ctx);

    namespace Visual
    {
        void OnKeySwap_Q(VisualHookContext& ctx);
        void OnCastAccepted_Q_Visual(VisualHookContext& ctx);
        void OnCastFrame_Visual(VisualHookContext& ctx);
    }
}

void Riven_KeepAlive();
