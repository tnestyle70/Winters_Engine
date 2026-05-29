#pragma once

struct VisualHookContext;

namespace LeeSin
{
    namespace Visual
    {
        void OnQCastFrame(VisualHookContext& ctx);
        void OnWCastFrame(VisualHookContext& ctx);
        void OnECastFrame(VisualHookContext& ctx);
        void OnRCastFrame(VisualHookContext& ctx);
    }
}

void LeeSin_KeepAlive();
