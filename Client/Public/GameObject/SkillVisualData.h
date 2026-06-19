#pragma once

#include "GameObject/VisualEventData.h"
#include "WintersTypes.h"

inline constexpr u8_t kSkillVisualStageMax = 2;
inline constexpr u8_t kSkillVisualEventMax = 4;

struct SkillVisualStageData
{
    u8_t stage = 1;
    const char* animationKey = nullptr;
    f32_t playbackSpeed = 1.f;
    bool_t bLoop = false;
    u8_t eventCount = 0;
    VisualEventData events[kSkillVisualEventMax] = {};
};

struct SkillVisualData
{
    bool_t bValid = false;
    u8_t slot = 0;
    u8_t stageCount = 1;
    const char* vfxKey = nullptr;
    const char* sfxKey = nullptr;
    SkillVisualStageData stages[kSkillVisualStageMax] = {};
    const char* endTransitionIdleAnim = nullptr;
    const char* endTransitionRunAnim = nullptr;
    f32_t endTransitionDuration = 0.1f;
};
