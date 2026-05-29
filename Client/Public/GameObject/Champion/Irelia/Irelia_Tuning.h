#pragma once

#include "Defines.h"
#include "WintersMath.h"

namespace Irelia
{
    struct IreliaTuning
    {
        f32_t bladeTravelSpeed = 22.f;
        f32_t bladeStunSec = 1.25f;
        f32_t bladeScale = 0.0301f;
        f32_t bladePitch = 0.f;
        f32_t bladeYaw = 0.f;
        f32_t bladeRoll = 0.f;
        f32_t bladeSpinSpeed = 6.283f;

        f32_t beamScaleAxis = 0.92f;
        f32_t fPlacedBladeScaleMul = 0.385f;
        f32_t fOrbitBladeScaleMul = 0.72f;
        f32_t fOrbitRadius = 0.58f;
        f32_t fOrbitAngularSpeed = 3.4f;
        f32_t fOrbitSpinSpeedMul = 1.2f;

        //렌더링 퀄리티용 매개변수
        Vec4 eBladeColor{ 0.54f, 0.82f, 1.02f, 1.f };
        Vec4 eGroundGlowColor{ 0.42f, 0.64f, 1.75f, 0.30f };
        Vec4 eGroundCoreColor{ 0.72f, 0.92f, 2.10f, 0.20f };
        Vec4 eCloseSparkColor{ 0.95f, 1.12f, 2.55f, 1.f };
        Vec4 eCloseBeamColor{ 0.70f, 0.84f, 2.20f, 0.90f };

        f32_t eGroundYOffset = -0.05f;
        f32_t eGroundGlowSize = 2.85f;
        f32_t eGroundCoreSize = 1.25f;
        f32_t eGroundSpinSpeed = 0.9f;
        f32_t eCloseSparkSize = 2.15f;
        f32_t eCloseBeamWidth = 0.34f;
        f32_t fEConnectLifetime = 0.82f;
        f32_t fECloseCoreWidthMul = 0.35f;
        f32_t fECloseDarkWidthMul = 4.9f;
        f32_t fECloseAfterglowWidthMul = 6.4f;
        f32_t fECloseStreakWidth = 0.18f;
        f32_t fECloseStreakLifetime = 0.16f;
        Vec4 vECloseAfterglowColor{ 0.30f, 0.50f, 1.60f, 0.26f };
        Vec4 vECloseFlashColor{ 1.05f, 1.20f, 2.75f, 1.00f };

        f32_t qTrailYOffset = 0.8f;
        f32_t qTrailWidth = 1.35f;
        f32_t qTrailHeight = 0.45f;
        f32_t qTrailLifetimeMax = 0.35f;
        f32_t qTrailAtlasFps = 18.f;
        Vec4 qTrailColor{ 0.55f, 0.95f, 1.20f, 0.80f };

        f32_t beamGirth = 0.14f;
        f32_t beamMeshBaseScale = 0.01f;
        f32_t beamYawOffset = 0.f;

        f32_t waveLength = 5.f;
        f32_t waveWidth = 7.5f;
        f32_t waveSpeed = 15.f;
        f32_t waveMaxDist = 8.f;
        f32_t waveDamage = 250.f;

        f32_t rFxWidth = 4.2f;
        f32_t rFxHeight = 3.0f;
        f32_t rFxYOffset = 0.91f;
        f32_t rFxFwdOffset = 0.0f;
        f32_t rFxYawOffset = 1.571f;
        i32_t iRWallBladeCount = 24;
        f32_t fRWallBladeSpreadRad = 4.18879f;
        f32_t fRWallBladePlaceDist = 3.0f;
        f32_t fRWallBladeLifetime = 2.5f;
        f32_t fRWallBladeScaleMul = 0.385f;

        f32_t wLayerLifetime = 0.48f;
        f32_t wLayerSize = 3.35f;
        Vec4 wLayerBladesColor{ 1.35f, 1.24f, 0.86f, 0.96f };
        Vec4 wLayerGlowColor{ 0.72f, 0.86f, 2.05f, 0.70f };
        f32_t fWHoldShieldSize = 3.45f;
        f32_t fWHoldGlowSize = 4.15f;
        Vec4 vWHoldShieldColor{ 0.54f, 0.90f, 1.65f, 0.42f };
        Vec4 vWHoldGlowColor{ 0.32f, 0.62f, 1.45f, 0.30f };
        f32_t fWAimRange = 6.0f;
        f32_t fWAimYOffset = 0.16f;
        f32_t fWReleaseRange = 6.0f;
        f32_t fWReleaseYOffset = 1.05f;

        bool_t bRTriangleMode = true;
        f32_t rTipBoost = 1.5f;
        f32_t rSideShrink = 0.f;
        f32_t fRTrailWidthMul = 1.35f;
        f32_t fRTrailHeightMul = 1.8f;
        Vec4 vRTrailColor{ 0.35f, 0.85f, 1.60f, 0.45f };
        Vec4 vRLeadColor{ 0.80f, 1.10f, 2.00f, 0.95f };
    };

    IreliaTuning& GetTuning();
    void ResetTuning();
}
