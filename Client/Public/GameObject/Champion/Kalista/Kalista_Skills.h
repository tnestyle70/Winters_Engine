#pragma once

#include "GamePlay/SkillHookContext.h"

namespace Kalista
{
    void OnCastFrame_BA(SkillHookContext& ctx);
    void OnCastFrame_Q(SkillHookContext& ctx);
    void OnCastAccepted_E(SkillHookContext& ctx);
    void OnRecoveryFrame_PassiveDash(SkillHookContext& ctx);
    void QueuePassiveDash(const Vec3& direction);
    bool_t HasPassiveDashRequest();
    void ClearPassiveDashRequest();
}

void Kalista_KeepAlive();
