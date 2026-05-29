#pragma once

#include "GamePlay/SkillHookContext.h"

class CWorld;
struct VisualHookContext;

namespace Engine
{
    class CFxStaticMeshRenderer;
}

namespace Irelia
{
    void OnCastAccepted_Q(SkillHookContext& ctx);
    void OnCastAccepted_W(SkillHookContext& ctx);
    void OnCastAccepted_E(SkillHookContext& ctx);

    void UpdateLocalBladeState(
        CWorld& world,
        Engine::CFxStaticMeshRenderer* pFxMeshRenderer,
        EntityID casterEntity,
        eTeam casterTeam,
        f32_t fDeltaTime,
        const Vec3& vCursorGround,
        bool_t bApplyLocalGameplay);

    void ResetLocalState();

    namespace Visual
    {
        void OnCastAccepted_Q_Visual(VisualHookContext& ctx);
        void OnCastAccepted_W_Visual(VisualHookContext& ctx);
        void OnCastAccepted_E_Visual(VisualHookContext& ctx);
        void OnCastAccepted_R_Visual(VisualHookContext& ctx);
    }
}

void Irelia_KeepAlive();
