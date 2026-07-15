#pragma once

#include "Defines.h"

namespace Yasuo
{
    struct YasuoTuning
    {
        f32_t qSpeed = 25.f;
        f32_t qLifetime = 0.5f;
        f32_t qTornadoSpeed = 18.f;
        f32_t qTornadoLifetime = 1.5f;
        f32_t wLifetime = 4.0f;
        f32_t wWidth = 3.2f;
        f32_t wHeight = 0.5f;
        f32_t eDashDuration = 0.25f;
        f32_t eTargetReuseDuration = 10.0f;
        f32_t rSearchRadius = 8.f;
        f32_t rSequenceDuration = 1.0f;

        f32_t qDamage = 60.f;
        f32_t qTornadoDamage = 100.f;
        f32_t qTornadoStunSec = 1.0f;
        f32_t eDamage = 80.f;
        f32_t rPerHitDamage = 40.f;
        f32_t rHitInterval = 0.2f;
        f32_t wMeshScale = 0.01f;

        f32_t qHitDelay = 0.25f;
        f32_t eqDelay = 0.20f;
        f32_t eqRadius = 2.5f;
        f32_t eqDamage = 70.f;
    };

    YasuoTuning& GetTuning();
    void ResetTuning();
}
