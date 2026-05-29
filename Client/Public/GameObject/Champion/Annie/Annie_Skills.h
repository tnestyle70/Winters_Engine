#pragma once

#include "GamePlay/SkillHookContext.h"
#include "GamePlay/VisualHookRegistry.h"

namespace Annie
{
    void OnCastFrame_BA(SkillHookContext& ctx);
    void OnCastFrame_Q(SkillHookContext& ctx);
    void OnCastFrame_W(SkillHookContext& ctx);
    void OnCastFrame_E(SkillHookContext& ctx);
    void OnCastFrame_R(SkillHookContext& ctx);

    namespace Visual
    {
        void OnCastFrame_BA_Visual(VisualHookContext& ctx);
        void OnCastFrame_Q_Visual(VisualHookContext& ctx);
        void OnCastFrame_W_Visual(VisualHookContext& ctx);
        void OnCastFrame_E_Visual(VisualHookContext& ctx);
        void OnCastFrame_R_Visual(VisualHookContext& ctx);
    }
}

void Annie_KeepAlive();
