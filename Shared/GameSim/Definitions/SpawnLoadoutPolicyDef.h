#pragma once

#include "WintersTypes.h"
#include "Shared/GameSim/Components/RuneComponent.h"

#include <algorithm>
#include <cmath>

struct SpawnLoadoutPolicyDef
{
    static constexpr u8_t kRespawnLevelCount = 18u;

    u32_t startGold = 0u;
    u8_t startLevel = 0u;
    eRuneId startRune = eRuneId::None;
    u8_t startRuneCount = 0u;
    f32_t respawnDelaySec = 0.f;
    f32_t respawnDelaySecByLevel[kRespawnLevelCount]{};

    f32_t ResolveBaseRespawnDelaySec(u8_t level) const
    {
        const u8_t clampedLevel = level < 1u
            ? 1u
            : (level > kRespawnLevelCount ? kRespawnLevelCount : level);
        const f32_t levelDelay = respawnDelaySecByLevel[clampedLevel - 1u];
        return levelDelay > 0.f ? levelDelay : respawnDelaySec;
    }

    static f32_t ResolveRespawnTimeIncreaseFactor(f32_t gameTimeSec)
    {
        if (!std::isfinite(gameTimeSec) || gameTimeSec <= 900.f)
            return 0.f;
        if (gameTimeSec <= 1800.f)
        {
            const f32_t ticks = std::floor((gameTimeSec - 900.f) / 30.f);
            return (std::min)(0.1275f, ticks * 0.00425f);
        }
        if (gameTimeSec <= 2700.f)
        {
            const f32_t ticks = std::floor((gameTimeSec - 1800.f) / 30.f);
            return (std::min)(0.2175f, 0.1275f + ticks * 0.003f);
        }

        const f32_t ticks = std::floor((gameTimeSec - 2700.f) / 30.f);
        return (std::min)(0.5f, 0.2175f + ticks * 0.0145f);
    }

    f32_t ResolveRespawnDelaySec(
        u8_t level,
        f32_t gameTimeSec,
        bool_t bPracticeModeEnabled = false,
        const f32_t* pPracticeSecondsByLevel = nullptr) const
    {
        const u8_t clampedLevel = level < 1u
            ? 1u
            : (level > kRespawnLevelCount ? kRespawnLevelCount : level);
        const f32_t practiceDelay =
            bPracticeModeEnabled && pPracticeSecondsByLevel
            ? pPracticeSecondsByLevel[clampedLevel - 1u]
            : 0.f;
        const f32_t baseDelay = std::isfinite(practiceDelay) && practiceDelay > 0.f
            ? practiceDelay
            : ResolveBaseRespawnDelaySec(clampedLevel);
        return baseDelay * (1.f + ResolveRespawnTimeIncreaseFactor(gameTimeSec));
    }
};
