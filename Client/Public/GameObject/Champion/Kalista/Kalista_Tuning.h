#pragma once

#include "Defines.h"

namespace Kalista
{
    struct KalistaTuning
    {
        f32_t baSpeed = 15.f;
        f32_t baMaxDist = 8.f;
        f32_t baRadius = 0.6f;
        f32_t baDamage = 70.f;
        f32_t baFlySpearScale = 0.02f;
        f32_t baFlySpearLengthMul = 0.6f;
        f32_t baFlySpearGirthMul = 0.5f;
        f32_t baStuckSpearScale = 0.01f;

        f32_t qSpeed = 9.f;
        f32_t qMaxDist = 14.f;
        f32_t qRadius = 0.6f;
        f32_t qDamage = 70.f;
        f32_t qFlySpearScale = 0.015f;
        f32_t qStuckSpearScale = 0.008f;

        f32_t passiveDashDist = 2.f;
        f32_t passiveDashDuration = 0.2f;
        f32_t passiveDashAnimSpeed = 1.25f;
        f32_t passiveDashInputGraceSec = 0.2f;

        f32_t rendBaseDamage = 20.f;
        f32_t rendStackDamage = 30.f;
        f32_t eRendWispSize = 2.5f;
        f32_t eRendWispLifetime = 0.45f;
        f32_t eRendWispAtlasFps = 24.f;
    };

    KalistaTuning& GetTuning();
    void ResetTuning();
}
