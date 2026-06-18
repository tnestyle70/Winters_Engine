#pragma once

#include "WintersTypes.h"

#include <type_traits>

enum class eNetAnimId : u16_t
{
    None = 0,
    Idle = 1,
    Run = 2,
    BasicAttack = 10,
    SkillQ = 20,
    SkillW = 21,
    SkillE = 22,
    SkillR = 23,
    Recall = 30,
    Death = 50,
    ViegoConsumeSoul = 60,
};

struct NetAnimationComponent
{
    u16_t animId = static_cast<u16_t>(eNetAnimId::Idle);
    u16_t animPhaseFrame = 0;
    u64_t animStartTick = 0;
    u32_t actionSeq = 0;
    u16_t playbackRateQ8 = 256;
    u16_t flags = 0;
    u8_t priority = 0;
    u8_t _pad = 0;
};

static_assert(std::is_trivially_copyable_v<NetAnimationComponent>,
    "NetAnimationComponent must be trivially_copyable for sim determinism.");
