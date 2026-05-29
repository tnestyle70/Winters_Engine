#pragma once
#include "Engine_Defines.h"

NS_BEGIN(Engine)

// ─────────────────────────────────────────────────────────────
//  고정 채널 슬롯. 한 채널에는 한 사운드만 재생되며,
//  같은 채널에 다시 PlaySound 호출 시 기존 사운드를 stop 후 교체.
//  효과음처럼 겹쳐 재생하려면 PlayEffect 를 사용 (자동 채널 할당).
// ─────────────────────────────────────────────────────────────
enum class eSoundChannel : u8_t
{
    BGM = 0,
    PlayerAction,   // 평타 · 스킬 보이스
    PlayerVoice,    // 대사 · 피격음
    UI,
    Ambient,
    Effect0,
    Effect1,
    Effect2,
    Effect3,
    MAX_CHANNEL
};

constexpr u32_t SOUND_CHANNEL_COUNT = static_cast<u32_t>(eSoundChannel::MAX_CHANNEL);

NS_END
