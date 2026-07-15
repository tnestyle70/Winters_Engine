#pragma once

#include "GamePlay/SkillHookContext.h"
#include "GamePlay/VisualHookRegistry.h"

namespace Yasuo
{
    void OnCastAccepted_Q(SkillHookContext& ctx);
    void OnCastAccepted_W(SkillHookContext& ctx);
    void OnCastAccepted_E(SkillHookContext& ctx);
    void OnCastAccepted_R(SkillHookContext& ctx);

    namespace Visual
    {
        void OnKeySwap_Q(VisualHookContext& ctx);
        void OnCastAccepted_Q_Visual(VisualHookContext& ctx);
        void OnCastAccepted_W_Visual(VisualHookContext& ctx);
        void OnCastAccepted_E_Visual(VisualHookContext& ctx);
        void OnCastAccepted_R_Visual(VisualHookContext& ctx);
        void OnPassiveShield_Visual(VisualHookContext& ctx);
    }
}

void Yasuo_KeepAlive();
