Session - 이렐리아 원작 E 레퍼런스의 청백/보라 색감, 양끝 칼날 수렴 애니메이션, 서버 단일 FX cue 흐름을 기존 Irelia FX에 반영한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Client/Public/GameObject/Champion/Irelia/Irelia_Tuning.h

기존 코드:

```cpp
        //?뚮뜑留??꾨━?곗슜 留ㅺ컻蹂??        Vec4 eBladeColor{ 0.43f, 0.76f, 0.78f, 1.f };
        Vec4 eGroundGlowColor{ 0.50f, 0.90f, 1.00f, 0.24f };
        Vec4 eGroundCoreColor{ 0.70f, 0.95f, 1.05f, 0.14f };
        Vec4 eCloseSparkColor{ 0.85f, 1.1f, 2.f, 1.f };
        Vec4 eCloseBeamColor{ 0.55f, 0.95f, 1.8f, 0.85f };

        f32_t eGroundYOffset = -0.05f;
        f32_t eGroundGlowSize = 2.6f;
        f32_t eGroundCoreSize = 1.4f;
        f32_t eGroundSpinSpeed = 1.2f;
        f32_t eCloseSparkSize = 2.0f;
        f32_t eCloseBeamWidth = 1.1f;
        f32_t fECloseCoreWidthMul = 0.55f;
        f32_t fECloseDarkWidthMul = 1.65f;
        f32_t fECloseAfterglowWidthMul = 2.2f;
        f32_t fECloseStreakWidth = 0.55f;
        f32_t fECloseStreakLifetime = 0.18f;
        Vec4 vECloseAfterglowColor{ 0.35f, 0.95f, 1.50f, 0.28f };
        Vec4 vECloseFlashColor{ 1.00f, 1.25f, 2.00f, 0.95f };
```

아래로 교체:

```cpp
        Vec4 vEBladeColor{ 0.50f, 0.82f, 1.15f, 1.00f };
        Vec4 vEGroundGlowColor{ 0.45f, 0.72f, 1.65f, 0.34f };
        Vec4 vEGroundCoreColor{ 0.78f, 0.92f, 1.80f, 0.24f };
        Vec4 vEPlacedRingColor{ 0.55f, 0.68f, 1.85f, 0.40f };
        Vec4 vEPlacedBladeGlowColor{ 0.85f, 0.96f, 2.10f, 0.74f };
        Vec4 vEPlacedVioletColor{ 0.75f, 0.42f, 1.85f, 0.42f };
        Vec4 vECloseSparkColor{ 0.92f, 1.06f, 2.35f, 1.00f };
        Vec4 vECloseBeamColor{ 0.62f, 0.74f, 2.05f, 0.78f };
        Vec4 vECloseDarkRailColor{ 0.02f, 0.06f, 0.36f, 0.58f };
        Vec4 vECloseArrowColor{ 0.86f, 0.96f, 2.40f, 0.88f };
        Vec4 vECloseAfterglowColor{ 0.38f, 0.58f, 1.70f, 0.30f };
        Vec4 vECloseFlashColor{ 1.00f, 1.18f, 2.45f, 0.98f };
        Vec4 vECloseVioletEdgeColor{ 0.72f, 0.38f, 1.95f, 0.50f };

        f32_t fEGroundYOffset = -0.05f;
        f32_t fEGroundGlowSize = 3.15f;
        f32_t fEGroundCoreSize = 1.55f;
        f32_t fEGroundSpinSpeed = 1.45f;
        f32_t fEPlacedBladeGlowSize = 1.85f;
        f32_t fEPlacedVioletSize = 2.35f;
        f32_t fEConnectLifetime = 0.72f;
        f32_t fEConnectDarkWidth = 1.38f;
        f32_t fEConnectCoreWidth = 0.42f;
        f32_t fEConnectAfterglowWidth = 2.15f;
        f32_t fECloseSparkSize = 2.10f;
        f32_t fECloseEdgeSparkSize = 1.35f;
        f32_t fECloseArrowWidth = 1.25f;
        f32_t fECloseArrowLength = 1.85f;
        f32_t fECloseArrowTravelTime = 0.28f;
        f32_t fECloseStreakWidth = 0.52f;
        f32_t fECloseStreakLifetime = 0.18f;
```

1-2. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Irelia/IreliaBladeSystem.cpp

기존 코드:

```cpp
                    world.GetComponent<FxBillboardComponent>(blade.groundGlowFxID).fYaw +=
                        t.eGroundSpinSpeed * fTimeDelta;
```

아래로 교체:

```cpp
                    world.GetComponent<FxBillboardComponent>(blade.groundGlowFxID).fYaw +=
                        t.fEGroundSpinSpeed * fTimeDelta;
```

기존 코드:

```cpp
                    world.GetComponent<FxBillboardComponent>(blade.groundCoreFxID).fYaw -=
                        t.eGroundSpinSpeed * 0.6f * fTimeDelta;
```

아래로 교체:

```cpp
                    world.GetComponent<FxBillboardComponent>(blade.groundCoreFxID).fYaw -=
                        t.fEGroundSpinSpeed * 0.6f * fTimeDelta;
```

기존 코드:

```cpp
    // eBladeColor??EffectTuner??Irelia Live Tuning?먯꽌 議곗젙?쒕떎.
    fxmesh.vColor = t.eBladeColor;
```

아래로 교체:

```cpp
    fxmesh.vColor = t.vEBladeColor;
```

1-3. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Irelia/IreliaFxPresets.cpp

기존 코드:

```cpp
    constexpr const wchar_t* kPathEStunTrail =
        L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_e_blade_stun_trail.png";
    constexpr const wchar_t* kPathLensflareStreak =
        L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_lensflare_streak.png";
```

아래에 추가:

```cpp
    constexpr const wchar_t* kPathEBladeGlow =
        L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_p_blade_glow.png";
    constexpr const wchar_t* kPathEBladeSwirl =
        L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_e_blade_swirl.png";
    constexpr const wchar_t* kPathEBladesErode =
        L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_e_blades_erode.png";
    constexpr const wchar_t* kPathEGlowSpecks =
        L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_glowspecks001.png";
    constexpr const wchar_t* kPathEGradient =
        L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_p_gradient.png";
```

기존 코드:

```cpp
    EntityID SpawnRuntimeBillboard(CWorld& world, const FxBillboardComponent& fx)
    {
        return CFxSystem::Spawn(world, fx);
    }
```

아래에 추가:

```cpp
    Vec3 LerpVec3(const Vec3& a, const Vec3& b, f32_t t)
    {
        return {
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t
        };
    }

    FxBillboardComponent MakeGroundLayer(
        const Vec3& pos,
        const wchar_t* texturePath,
        f32_t width,
        f32_t height,
        f32_t yaw,
        f32_t lifetime,
        const Vec4& color,
        eBlendPreset blendMode)
    {
        FxBillboardComponent fx{};
        fx.vWorldPos = pos;
        fx.texturePath = texturePath;
        fx.fWidth = width;
        fx.fHeight = height;
        fx.fYaw = yaw;
        fx.fLifetime = lifetime;
        fx.fFadeIn = 0.025f;
        fx.fFadeOut = lifetime * 0.55f;
        fx.bBillboard = false;
        fx.vColor = color;
        fx.blendMode = blendMode;
        fx.fAlphaClip = 0.01f;
        return fx;
    }

    FxBillboardComponent MakeWorldBillboard(
        const Vec3& pos,
        const wchar_t* texturePath,
        f32_t size,
        f32_t lifetime,
        const Vec4& color,
        eBlendPreset blendMode)
    {
        FxBillboardComponent fx{};
        fx.vWorldPos = pos;
        fx.texturePath = texturePath;
        fx.fWidth = size;
        fx.fHeight = size;
        fx.fLifetime = lifetime;
        fx.fFadeIn = 0.015f;
        fx.fFadeOut = lifetime * 0.60f;
        fx.bBillboard = true;
        fx.vColor = color;
        fx.blendMode = blendMode;
        fx.fAlphaClip = 0.01f;
        return fx;
    }
```

기존 코드:

```cpp
IreliaFx::IreliaEPlacedFxIds IreliaFx::SpawnEPlacedLayers(CWorld& world, const Vec3& vBladePos, f32_t fLifetime)
{
    const Irelia::IreliaTuning& t = Irelia::GetTuning();
    const Vec3 ground{ vBladePos.x, vBladePos.y - 2.95f + t.eGroundYOffset, vBladePos.z };

    FxBillboardComponent glow{};
    glow.vWorldPos = ground;
    glow.texturePath = kPathEGroundTyphoon;
    glow.fWidth = t.eGroundGlowSize;
    glow.fHeight = t.eGroundGlowSize;
    glow.fLifetime = fLifetime;
    glow.fFadeIn = 0.08f;
    glow.fFadeOut = 0.25f;
    glow.bBillboard = false;
    glow.vColor = t.eGroundGlowColor;
    glow.blendMode = eBlendPreset::AlphaBlend;
    glow.fAlphaClip = 0.01f;

    IreliaEPlacedFxIds ids{};
    ids.groundGlowFxID = SpawnBillboardAsset(world, glow, "Irelia_E_Ground_Typhoon");

    FxBillboardComponent core = glow;
    core.fWidth = t.eGroundCoreSize;
    core.fHeight = t.eGroundCoreSize;
    core.vColor = t.eGroundCoreColor;
    core.blendMode = eBlendPreset::AlphaBlend;
    core.fAlphaClip = 0.01f;
    ids.groundCoreFxID = SpawnBillboardAsset(world, core, "Irelia_E_Ground_Core");

    return ids;
}
```

아래로 교체:

```cpp
IreliaFx::IreliaEPlacedFxIds IreliaFx::SpawnEPlacedLayers(CWorld& world, const Vec3& vBladePos, f32_t fLifetime)
{
    const Irelia::IreliaTuning& t = Irelia::GetTuning();
    const Vec3 ground{ vBladePos.x, vBladePos.y - 2.95f + t.fEGroundYOffset, vBladePos.z };

    FxBillboardComponent ring = MakeGroundLayer(
        ground,
        kPathEGroundTyphoon,
        t.fEGroundGlowSize,
        t.fEGroundGlowSize,
        0.f,
        fLifetime,
        t.vEPlacedRingColor,
        eBlendPreset::AlphaBlend);
    ring.fFadeIn = 0.06f;
    ring.fFadeOut = 0.32f;

    IreliaEPlacedFxIds ids{};
    ids.groundGlowFxID = SpawnBillboardAsset(world, ring, "Irelia_E_Ground_Typhoon");

    FxBillboardComponent core = MakeGroundLayer(
        { ground.x, ground.y + 0.015f, ground.z },
        kPathEBladeGlow,
        t.fEGroundCoreSize,
        t.fEGroundCoreSize,
        0.f,
        fLifetime,
        t.vEGroundCoreColor,
        eBlendPreset::Additive);
    core.fFadeIn = 0.03f;
    core.fFadeOut = 0.30f;
    ids.groundCoreFxID = SpawnBillboardAsset(world, core, "Irelia_E_Ground_Core");

    FxBillboardComponent bladeGlow = MakeWorldBillboard(
        { vBladePos.x, vBladePos.y + 0.15f, vBladePos.z },
        kPathEBladeGlow,
        t.fEPlacedBladeGlowSize,
        0.42f,
        t.vEPlacedBladeGlowColor,
        eBlendPreset::Additive);
    SpawnRuntimeBillboard(world, bladeGlow);

    FxBillboardComponent violet = MakeWorldBillboard(
        { vBladePos.x, vBladePos.y + 0.32f, vBladePos.z },
        kPathEGradient,
        t.fEPlacedVioletSize,
        0.52f,
        t.vEPlacedVioletColor,
        eBlendPreset::Additive);
    violet.fStartDelay = 0.04f;
    SpawnRuntimeBillboard(world, violet);

    FxBillboardComponent spark = MakeWorldBillboard(
        { vBladePos.x, vBladePos.y + 0.55f, vBladePos.z },
        kPathEWarningSpark,
        t.fECloseEdgeSparkSize,
        0.36f,
        t.vECloseSparkColor,
        eBlendPreset::Additive);
    spark.fStartDelay = 0.02f;
    SpawnBillboardAsset(world, spark, "Irelia_E_Placed_Spark");

    FxBillboardComponent specks = MakeWorldBillboard(
        { vBladePos.x, vBladePos.y + 0.45f, vBladePos.z },
        kPathEGlowSpecks,
        t.fEPlacedVioletSize * 0.9f,
        0.65f,
        t.vECloseVioletEdgeColor,
        eBlendPreset::Additive);
    specks.fStartDelay = 0.07f;
    SpawnRuntimeBillboard(world, specks);

    return ids;
}
```

기존 코드:

```cpp
void IreliaFx::SpawnECloseLayers(CWorld& world, const Vec3& vStart, const Vec3& vEnd, f32_t fLifetime)
{
    const Irelia::IreliaTuning& t = Irelia::GetTuning();
    const f32_t dx = vEnd.x - vStart.x;
    const f32_t dz = vEnd.z - vStart.z;
    const f32_t dist = std::sqrtf(dx * dx + dz * dz);
    const f32_t len = (dist > 0.1f) ? dist : 0.1f;
    const f32_t yaw = std::atan2f(dx, dz);
    const Vec3 mid{
        (vStart.x + vEnd.x) * 0.5f,
        (vStart.y + vEnd.y) * 0.5f,
        (vStart.z + vEnd.z) * 0.5f
    };
    const f32_t groundY = mid.y - 2.95f + t.eGroundYOffset;

    FxBillboardComponent dark{};
    dark.vWorldPos = { mid.x, groundY, mid.z };
    dark.texturePath = kPathEStunBeamDark;
    dark.fWidth = t.eCloseBeamWidth * t.fECloseDarkWidthMul;
    dark.fHeight = len;
    dark.fYaw = yaw;
    dark.fLifetime = fLifetime;
    dark.fFadeIn = 0.03f;
    dark.fFadeOut = 0.25f;
    dark.bBillboard = false;
    dark.vColor = { t.eCloseBeamColor.x, t.eCloseBeamColor.y, t.eCloseBeamColor.z,
        t.eCloseBeamColor.w * 0.55f };
    dark.blendMode = eBlendPreset::Additive;
    SpawnBillboardAsset(world, dark, "Irelia_E_Close_Beam_Dark");

    FxBillboardComponent beam = dark;
    beam.texturePath = kPathEStunBeam;
    beam.fWidth = t.eCloseBeamWidth;
    beam.vColor = t.eCloseBeamColor;
    SpawnBillboardAsset(world, beam, "Irelia_E_Close_Beam");

    FxBillboardComponent core = beam;
    core.fWidth = t.eCloseBeamWidth * t.fECloseCoreWidthMul;
    core.fLifetime = 0.32f;
    core.fFadeIn = 0.02f;
    core.fFadeOut = 0.22f;
    core.fStartDelay = 0.02f;
    core.vColor = { t.eCloseBeamColor.x * 1.25f, t.eCloseBeamColor.y * 1.25f,
        t.eCloseBeamColor.z * 1.25f, t.eCloseBeamColor.w };
    SpawnRuntimeBillboard(world, core);

    FxBillboardComponent spark{};
    spark.vWorldPos = mid;
    spark.texturePath = kPathEWarningSpark;
    spark.fWidth = t.eCloseSparkSize;
    spark.fHeight = t.eCloseSparkSize;
    spark.fLifetime = fLifetime;
    spark.fFadeIn = 0.02f;
    spark.fFadeOut = 0.28f;
    spark.bBillboard = true;
    spark.vColor = t.eCloseSparkColor;
    spark.blendMode = eBlendPreset::Additive;
    SpawnBillboardAsset(world, spark, "Irelia_E_Close_Spark_Mid");

    FxBillboardComponent edgeSpark = spark;
    edgeSpark.fWidth = t.eCloseSparkSize * 0.7f;
    edgeSpark.fHeight = t.eCloseSparkSize * 0.7f;
    edgeSpark.vWorldPos = vStart;
    SpawnBillboardAsset(world, edgeSpark, "Irelia_E_Close_Spark_A");
    edgeSpark.vWorldPos = vEnd;
    SpawnBillboardAsset(world, edgeSpark, "Irelia_E_Close_Spark_B");

    FxBillboardComponent flash = spark;
    flash.fWidth = t.eCloseSparkSize * 1.45f;
    flash.fHeight = t.eCloseSparkSize * 1.45f;
    flash.fLifetime = 0.18f;
    flash.fFadeIn = 0.01f;
    flash.fFadeOut = 0.16f;
    flash.fStartDelay = 0.08f;
    flash.vColor = t.vECloseFlashColor;
    SpawnRuntimeBillboard(world, flash);

    FxBillboardComponent afterglow = dark;
    afterglow.fWidth = t.eCloseBeamWidth * t.fECloseAfterglowWidthMul;
    afterglow.fLifetime = fLifetime + 0.2f;
    afterglow.fFadeIn = 0.08f;
    afterglow.fFadeOut = 0.45f;
    afterglow.fStartDelay = 0.12f;
    afterglow.vColor = t.vECloseAfterglowColor;
    SpawnRuntimeBillboard(world, afterglow);

    f32_t fStreakLifetime = t.fECloseStreakLifetime;
    if (fStreakLifetime < 0.05f)
        fStreakLifetime = 0.05f;

    const Vec3 dir{ dx / len, 0.f, dz / len };
    FxBillboardComponent streak{};
    streak.vWorldPos = { vStart.x, groundY + 0.03f, vStart.z };
    streak.vVelocity = { dir.x * len / fStreakLifetime, 0.f, dir.z * len / fStreakLifetime };
    streak.texturePath = kPathLensflareStreak;
    streak.fWidth = t.fECloseStreakWidth;
    streak.fHeight = len * 0.35f;
    streak.fYaw = yaw;
    streak.fLifetime = fStreakLifetime;
    streak.fFadeIn = 0.02f;
    streak.fFadeOut = 0.12f;
    streak.bBillboard = false;
    streak.vColor = t.vECloseFlashColor;
    streak.blendMode = eBlendPreset::Additive;
    SpawnRuntimeBillboard(world, streak);

    FxBillboardComponent reverseStreak = streak;
    reverseStreak.vWorldPos = { vEnd.x, groundY + 0.04f, vEnd.z };
    reverseStreak.vVelocity = { -dir.x * len / fStreakLifetime, 0.f, -dir.z * len / fStreakLifetime };
    reverseStreak.fStartDelay = 0.06f;
    reverseStreak.vColor = { t.vECloseFlashColor.x, t.vECloseFlashColor.y,
        t.vECloseFlashColor.z, t.vECloseFlashColor.w * 0.7f };
    SpawnRuntimeBillboard(world, reverseStreak);
}
```

아래로 교체:

```cpp
void IreliaFx::SpawnECloseLayers(CWorld& world, const Vec3& vStart, const Vec3& vEnd, f32_t fLifetime)
{
    const Irelia::IreliaTuning& t = Irelia::GetTuning();
    const f32_t dx = vEnd.x - vStart.x;
    const f32_t dz = vEnd.z - vStart.z;
    const f32_t dist = std::sqrtf(dx * dx + dz * dz);
    const f32_t len = (dist > 0.1f) ? dist : 0.1f;
    const f32_t yaw = std::atan2f(dx, dz);
    const Vec3 dir{ dx / len, 0.f, dz / len };
    const Vec3 mid{
        (vStart.x + vEnd.x) * 0.5f,
        (vStart.y + vEnd.y) * 0.5f,
        (vStart.z + vEnd.z) * 0.5f
    };
    const f32_t groundY = mid.y - 2.95f + t.fEGroundYOffset;
    const f32_t lifetime = (fLifetime > 0.1f) ? fLifetime : t.fEConnectLifetime;

    FxBillboardComponent dark = MakeGroundLayer(
        { mid.x, groundY + 0.010f, mid.z },
        kPathEStunBeamDark,
        t.fEConnectDarkWidth,
        len,
        yaw,
        lifetime,
        t.vECloseDarkRailColor,
        eBlendPreset::AlphaBlend);
    dark.fFadeIn = 0.02f;
    dark.fFadeOut = 0.38f;
    SpawnBillboardAsset(world, dark, "Irelia_E_Close_Dark_Rail");

    FxBillboardComponent railGlow = dark;
    railGlow.texturePath = kPathEStunBeam;
    railGlow.fWidth = t.fEConnectCoreWidth;
    railGlow.fLifetime = 0.42f;
    railGlow.fStartDelay = 0.05f;
    railGlow.fFadeIn = 0.02f;
    railGlow.fFadeOut = 0.28f;
    railGlow.vColor = t.vECloseBeamColor;
    railGlow.blendMode = eBlendPreset::Additive;
    SpawnBillboardAsset(world, railGlow, "Irelia_E_Close_Core_Rail");

    FxBillboardComponent afterglow = dark;
    afterglow.texturePath = kPathEStunBeamDark;
    afterglow.fWidth = t.fEConnectAfterglowWidth;
    afterglow.fLifetime = lifetime + 0.18f;
    afterglow.fStartDelay = 0.12f;
    afterglow.fFadeIn = 0.05f;
    afterglow.fFadeOut = 0.48f;
    afterglow.vColor = t.vECloseAfterglowColor;
    afterglow.blendMode = eBlendPreset::Additive;
    SpawnRuntimeBillboard(world, afterglow);

    FxBillboardComponent startRing = MakeGroundLayer(
        { vStart.x, groundY + 0.025f, vStart.z },
        kPathEGroundTyphoon,
        t.fEGroundGlowSize * 0.72f,
        t.fEGroundGlowSize * 0.72f,
        yaw,
        0.55f,
        t.vEGroundGlowColor,
        eBlendPreset::Additive);
    SpawnBillboardAsset(world, startRing, "Irelia_E_Close_Start_Ring");

    FxBillboardComponent endRing = startRing;
    endRing.vWorldPos = { vEnd.x, groundY + 0.030f, vEnd.z };
    endRing.vColor = t.vEPlacedVioletColor;
    SpawnBillboardAsset(world, endRing, "Irelia_E_Close_End_Ring");

    const f32_t travelTime = (t.fECloseArrowTravelTime > 0.05f) ? t.fECloseArrowTravelTime : 0.28f;
    for (int32_t i = 0; i < 3; ++i)
    {
        const f32_t fraction = 0.18f + static_cast<f32_t>(i) * 0.13f;
        const f32_t delay = static_cast<f32_t>(i) * 0.045f;

        FxBillboardComponent arrowA = MakeGroundLayer(
            { vStart.x + dir.x * len * fraction, groundY + 0.060f, vStart.z + dir.z * len * fraction },
            kPathEStunTrail,
            t.fECloseArrowWidth,
            t.fECloseArrowLength,
            yaw,
            travelTime,
            t.vECloseArrowColor,
            eBlendPreset::Additive);
        arrowA.fStartDelay = delay;
        arrowA.fFadeOut = 0.16f;
        arrowA.vVelocity = { dir.x * len * 0.95f / travelTime, 0.f, dir.z * len * 0.95f / travelTime };
        SpawnRuntimeBillboard(world, arrowA);

        FxBillboardComponent arrowB = arrowA;
        arrowB.vWorldPos = {
            vEnd.x - dir.x * len * fraction,
            groundY + 0.070f,
            vEnd.z - dir.z * len * fraction
        };
        arrowB.vVelocity = { -arrowA.vVelocity.x, 0.f, -arrowA.vVelocity.z };
        arrowB.fYaw = yaw + 3.14159265f;
        arrowB.vColor = { t.vECloseArrowColor.x, t.vECloseArrowColor.y,
            t.vECloseArrowColor.z, t.vECloseArrowColor.w * 0.82f };
        SpawnRuntimeBillboard(world, arrowB);
    }

    FxBillboardComponent edgeSpark = MakeWorldBillboard(
        { vStart.x, vStart.y + 0.65f, vStart.z },
        kPathEWarningSpark,
        t.fECloseEdgeSparkSize,
        0.34f,
        t.vECloseSparkColor,
        eBlendPreset::Additive);
    SpawnBillboardAsset(world, edgeSpark, "Irelia_E_Close_Spark_A");
    edgeSpark.vWorldPos = { vEnd.x, vEnd.y + 0.65f, vEnd.z };
    edgeSpark.vColor = t.vECloseVioletEdgeColor;
    SpawnBillboardAsset(world, edgeSpark, "Irelia_E_Close_Spark_B");

    FxBillboardComponent midFlash = MakeWorldBillboard(
        { mid.x, mid.y + 0.75f, mid.z },
        kPathEWarningSpark,
        t.fECloseSparkSize,
        0.24f,
        t.vECloseFlashColor,
        eBlendPreset::Additive);
    midFlash.fStartDelay = 0.18f;
    midFlash.fFadeOut = 0.18f;
    SpawnBillboardAsset(world, midFlash, "Irelia_E_Close_Spark_Mid");

    f32_t fStreakLifetime = t.fECloseStreakLifetime;
    if (fStreakLifetime < 0.05f)
        fStreakLifetime = 0.05f;

    FxBillboardComponent streak = MakeGroundLayer(
        { vStart.x, groundY + 0.090f, vStart.z },
        kPathLensflareStreak,
        t.fECloseStreakWidth,
        len * 0.42f,
        yaw,
        fStreakLifetime,
        t.vECloseFlashColor,
        eBlendPreset::Additive);
    streak.vVelocity = { dir.x * len / fStreakLifetime, 0.f, dir.z * len / fStreakLifetime };
    SpawnRuntimeBillboard(world, streak);

    FxBillboardComponent reverseStreak = streak;
    reverseStreak.vWorldPos = { vEnd.x, groundY + 0.100f, vEnd.z };
    reverseStreak.vVelocity = { -dir.x * len / fStreakLifetime, 0.f, -dir.z * len / fStreakLifetime };
    reverseStreak.fYaw = yaw + 3.14159265f;
    reverseStreak.fStartDelay = 0.06f;
    reverseStreak.vColor = { t.vECloseFlashColor.x, t.vECloseFlashColor.y,
        t.vECloseFlashColor.z, t.vECloseFlashColor.w * 0.72f };
    SpawnRuntimeBillboard(world, reverseStreak);
}
```

1-4. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Irelia/Irelia_Skills.cpp

기존 코드:

```cpp
        IreliaFx::SpawnECloseLayers(world, p1, p2, 0.5f);
```

아래로 교체:

```cpp
        IreliaFx::SpawnECloseLayers(world, p1, p2, t.fEConnectLifetime);
```

기존 코드:

```cpp
    IreliaLocalState s_state{};
```

아래로 교체:

```cpp
    std::unordered_map<EntityID, IreliaLocalState> s_stateByCaster;

    IreliaLocalState& GetState(EntityID caster)
    {
        return s_stateByCaster[caster];
    }
```

기존 코드:

```cpp
#include <functional>
```

아래에 추가:

```cpp
#include <unordered_map>
```

기존 코드:

```cpp
    void ExpireCurrentEBlades(CWorld& world)
    {
        CIreliaBladeSystem::ExpireAfter(world, s_state.sword1Id, kEBladeExpireDelay);
        CIreliaBladeSystem::ExpireAfter(world, s_state.sword2Id, kEBladeExpireDelay);
    }
```

아래로 교체:

```cpp
    void ExpireCurrentEBlades(CWorld& world, IreliaLocalState& state)
    {
        CIreliaBladeSystem::ExpireAfter(world, state.sword1Id, kEBladeExpireDelay);
        CIreliaBladeSystem::ExpireAfter(world, state.sword2Id, kEBladeExpireDelay);
    }
```

기존 코드:

```cpp
    void ClearWHoldFx(CWorld& world)
    {
        MarkBillboardPendingDelete(world, s_state.wHoldFxIds.spinFxID);
        MarkBillboardPendingDelete(world, s_state.wHoldFxIds.shieldFxID);
        MarkBillboardPendingDelete(world, s_state.wHoldFxIds.glowFxID);
        MarkBillboardPendingDelete(world, s_state.wHoldFxIds.blockFxID);
        s_state.wHoldFxIds = IreliaFx::IreliaWHoldFxIds{};
        s_state.wSpinFxId = NULL_ENTITY;
    }
```

아래로 교체:

```cpp
    void ClearWHoldFx(CWorld& world, IreliaLocalState& state)
    {
        MarkBillboardPendingDelete(world, state.wHoldFxIds.spinFxID);
        MarkBillboardPendingDelete(world, state.wHoldFxIds.shieldFxID);
        MarkBillboardPendingDelete(world, state.wHoldFxIds.glowFxID);
        MarkBillboardPendingDelete(world, state.wHoldFxIds.blockFxID);
        state.wHoldFxIds = IreliaFx::IreliaWHoldFxIds{};
        state.wSpinFxId = NULL_ENTITY;
    }
```

기존 코드:

```cpp
    void ResetLocalState()
    {
        s_state = IreliaLocalState{};
    }
```

아래로 교체:

```cpp
    void ResetLocalState()
    {
        s_stateByCaster.clear();
    }
```

`void OnCastAccepted_W(SkillHookContext& ctx)` 안에서 아래 기존 코드 바로 위에 추가:

기존 코드:

```cpp
        if (ctx.skillStage >= 2)
```

아래에 추가:

```cpp
        IreliaLocalState& state = GetState(ctx.casterEntity);
```

기존 코드:

```cpp
            ClearWHoldFx(*ctx.pWorld);
```

아래로 교체:

```cpp
            ClearWHoldFx(*ctx.pWorld, state);
```

기존 코드:

```cpp
        s_state.wHoldFxIds = IreliaFx::SpawnWSpinLayers(*ctx.pWorld, ctx.casterEntity, lifetime);
        s_state.wSpinFxId = s_state.wHoldFxIds.spinFxID;
```

아래로 교체:

```cpp
        state.wHoldFxIds = IreliaFx::SpawnWSpinLayers(*ctx.pWorld, ctx.casterEntity, lifetime);
        state.wSpinFxId = state.wHoldFxIds.spinFxID;
```

`void OnCastAccepted_E(SkillHookContext& ctx)` 안에서 아래 기존 코드 바로 위에 추가:

기존 코드:

```cpp
        const IreliaTuning& t = GetTuning();
```

아래에 추가:

```cpp
        IreliaLocalState& state = GetState(ctx.casterEntity);
```

기존 코드:

```cpp
            s_state.sword2Id = bladeId;
            s_state.bEAutoSecondPending = false;
            s_state.eSword1Elapsed = 0.f;
            ExpireCurrentEBlades(*ctx.pWorld);
```

아래로 교체:

```cpp
            state.sword2Id = bladeId;
            state.bEAutoSecondPending = false;
            state.eSword1Elapsed = 0.f;
            ExpireCurrentEBlades(*ctx.pWorld, state);
```

기존 코드:

```cpp
            s_state.sword1Id = bladeId;
            s_state.sword2Id = NULL_ENTITY;
            s_state.beamDelay = 0.f;
            s_state.bBeamSpawned = false;
            s_state.eSword1Elapsed = 0.f;
            s_state.bEAutoSecondPending = (bladeId != NULL_ENTITY);
```

아래로 교체:

```cpp
            state.sword1Id = bladeId;
            state.sword2Id = NULL_ENTITY;
            state.beamDelay = 0.f;
            state.bBeamSpawned = false;
            state.eSword1Elapsed = 0.f;
            state.bEAutoSecondPending = (bladeId != NULL_ENTITY);
```

`void UpdateLocalBladeState(...)` 안에서 아래 기존 코드 바로 위에 추가:

기존 코드:

```cpp
        if (s_state.sword1Id != NULL_ENTITY && !world.IsAlive(s_state.sword1Id))
```

아래에 추가:

```cpp
        IreliaLocalState& state = GetState(casterEntity);
```

`void UpdateLocalBladeState(...)` 안의 나머지 `s_state.` 참조는 모두 `state.`로 교체한다.

기존 코드:

```cpp
                ExpireCurrentEBlades(world);
```

아래로 교체:

```cpp
                ExpireCurrentEBlades(world, state);
```

1-5. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Irelia/Irelia_Registration.cpp

기존 코드:

```cpp
                s.stage2AnimKey = "spell3";
                s.stage2LockSec = 0.4f;
                s.stage2Rotate = eRotateMode::TowardsCursor;
                s.stageWindowSec = 2.f;
                s.castFrame = 10.f;
                s.recoveryFrame = 20.f;
                s.stage2CastFrame = 6.f;
                s.stage2RecoveryFrame = 14.f;
```

아래로 교체:

```cpp
                s.stage2AnimKey = "spell3_b";
                s.stage2LockSec = 0.45f;
                s.stage2Rotate = eRotateMode::TowardsCursor;
                s.stageWindowSec = 3.5f;
                s.castFrame = 8.f;
                s.recoveryFrame = 18.f;
                s.stage2CastFrame = 5.f;
                s.stage2RecoveryFrame = 13.f;
```

확인 필요:
- stage2 종료 전환이 실제 재생 중 튀면 `spell3_b_to_idle`과 `spell3_b_run`을 stage별 전환으로 분리하는 `SkillDef` 필드가 별도로 필요한지 확인한다.

1-6. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/ChampionRuntimeDefaults.cpp

기존 코드:

```cpp
        { eChampion::IRELIA, 3, 1, 1.00f, 1.00f },
        { eChampion::IRELIA, 3, 2, 0.40f, 1.00f },
```

아래로 교체:

```cpp
        { eChampion::IRELIA, 3, 1, 0.90f, 1.05f },
        { eChampion::IRELIA, 3, 2, 0.45f, 1.05f },
```

기존 코드:

```cpp
        { eChampion::IRELIA, 3, 2, 2.00f },
```

아래로 교체:

```cpp
        { eChampion::IRELIA, 3, 2, 3.50f },
```

1-7. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Irelia/IreliaGameSim.cpp

기존 코드:

```cpp
    void EmitIreliaRHitVisual(CWorld& world, const GameplayHookContext& ctx,
        EntityID target, const Vec3& vHitPos, const Vec3& vForward)
```

아래에 추가:

```cpp
    u16_t MakeIreliaEffectFlags(u8_t slot, u8_t rank, u8_t stage)
    {
        return static_cast<u16_t>(
            (static_cast<u16_t>(stage & 0x0fu) << 12) |
            (static_cast<u16_t>(rank & 0x0fu) << 8) |
            static_cast<u16_t>(slot));
    }

    void EmitIreliaEVisual(CWorld& world, const GameplayHookContext& ctx,
        const Vec3& vBladePos, u8_t stage)
    {
        ReplicatedEventComponent event{};
        event.kind = eReplicatedEventKind::EffectTrigger;
        event.sourceEntity = ctx.casterEntity;
        event.targetEntity = NULL_ENTITY;
        event.effectId = MakeGameplayHookId(eChampion::IRELIA, GameplayHookVariant::E_OnCastAccepted);
        event.slot = static_cast<u8_t>(eSkillSlot::E);
        event.rank = ctx.skillRank;
        event.flags = MakeIreliaEffectFlags(
            static_cast<u8_t>(eSkillSlot::E),
            ctx.skillRank,
            stage);
        event.position = vBladePos;
        event.direction = ctx.pCommand ? ctx.pCommand->direction : Vec3{ 0.f, 0.f, 1.f };
        event.durationMs = stage >= 2u ? 720u : 3500u;
        event.startTick = ctx.pTickCtx ? ctx.pTickCtx->tickIndex : 0;

        EnqueueReplicatedEvent(world, event);
    }
```

`void OnE(GameplayHookContext& ctx)` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
            state.bHasBlade1 = true;
```

아래에 추가:

```cpp
            EmitIreliaEVisual(world, ctx, state.blade1Pos, 1u);
```

`void OnE(GameplayHookContext& ctx)` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
        state.bHasBlade2 = true;
```

아래에 추가:

```cpp
        EmitIreliaEVisual(world, ctx, state.blade2Pos, 2u);
```

기존 코드:

```cpp
        event.flags = static_cast<u16_t>(
            (2u << 12) |
            (static_cast<u16_t>(ctx.skillRank & 0x0fu) << 8) |
            static_cast<u16_t>(eSkillSlot::R));
```

아래로 교체:

```cpp
        event.flags = MakeIreliaEffectFlags(
            static_cast<u8_t>(eSkillSlot::R),
            ctx.skillRank,
            2u);
```

1-8. C:/Users/user/Desktop/Winters/Engine/Public/FX/FxAsset.h

기존 코드:

```cpp
    f32_t fLifetime = 3.f;
    f32_t fFadeIn = 0.f;
    f32_t fFadeOut = 0.f;
    u32_t iRibbonPointCount = 2;
```

아래로 교체:

```cpp
    f32_t fLifetime = 3.f;
    f32_t fFadeIn = 0.f;
    f32_t fStartDelay = 0.f;
    f32_t fFadeOut = 0.f;
    u32_t iRibbonPointCount = 2;
```

1-9. C:/Users/user/Desktop/Winters/Engine/Private/FX/FxAsset.cpp

기존 코드:

```cpp
            ExtractNumber(block, "fade_in", emitter.fFadeIn);
            ExtractNumber(block, "fade_out", emitter.fFadeOut);
```

아래로 교체:

```cpp
            ExtractNumber(block, "fade_in", emitter.fFadeIn);
            ExtractNumber(block, "start_delay", emitter.fStartDelay);
            if (emitter.fStartDelay == 0.f)
                ExtractNumber(block, "startDelay", emitter.fStartDelay);
            ExtractNumber(block, "fade_out", emitter.fFadeOut);
```

1-10. C:/Users/user/Desktop/Winters/Client/Private/GameObject/FX/FxSystem.cpp

기존 코드:

```cpp
        fx.fLifetime = emitter.fLifetime;
        fx.bBillboard = emitter.bBillboard && emitter.renderType == eFxRenderType::Billboard;
```

아래로 교체:

```cpp
        fx.fLifetime = emitter.fLifetime;
        fx.fStartDelay = emitter.fStartDelay;
        fx.bBillboard = emitter.bBillboard && emitter.renderType == eFxRenderType::Billboard;
```

1-11. C:/Users/user/Desktop/Winters/Client/Public/GameObject/FX/FxBeamComponent.h

기존 코드:

```cpp
    f32_t fLifetime = 1.f;
    f32_t fElapsed = 0.f;
    f32_t fFadeIn = 0.f;
```

아래로 교체:

```cpp
    f32_t fLifetime = 1.f;
    f32_t fElapsed = 0.f;
    f32_t fStartDelay = 0.f;
    f32_t fFadeIn = 0.f;
```

1-12. C:/Users/user/Desktop/Winters/Client/Public/GameObject/FX/FxRibbonComponent.h

기존 코드:

```cpp
    f32_t fLifetime = 1.f;
    f32_t fElapsed = 0.f;
    f32_t fFadeIn = 0.f;
```

아래로 교체:

```cpp
    f32_t fLifetime = 1.f;
    f32_t fElapsed = 0.f;
    f32_t fStartDelay = 0.f;
    f32_t fFadeIn = 0.f;
```

1-13. C:/Users/user/Desktop/Winters/Client/Private/GameObject/FX/FxBeamSystem.cpp

기존 코드:

```cpp
        beam.fLifetime = emitter.fLifetime;
        beam.fFadeIn = emitter.fFadeIn;
        beam.fFadeOut = emitter.fFadeOut;
```

아래로 교체:

```cpp
        beam.fLifetime = emitter.fLifetime;
        beam.fStartDelay = emitter.fStartDelay;
        beam.fFadeIn = emitter.fFadeIn;
        beam.fFadeOut = emitter.fFadeOut;
```

기존 코드:

```cpp
        ribbon.fLifetime = emitter.fLifetime;
        ribbon.fFadeIn = emitter.fFadeIn;
        ribbon.fFadeOut = emitter.fFadeOut;
```

아래로 교체:

```cpp
        ribbon.fLifetime = emitter.fLifetime;
        ribbon.fStartDelay = emitter.fStartDelay;
        ribbon.fFadeIn = emitter.fFadeIn;
        ribbon.fFadeOut = emitter.fFadeOut;
```

기존 코드:

```cpp
                if (beam.bPendingDelete || beam.fElapsed >= beam.fLifetime)
                    vecDelete.push_back(e);
```

아래로 교체:

```cpp
                if (beam.bPendingDelete || beam.fElapsed >= beam.fStartDelay + beam.fLifetime)
                    vecDelete.push_back(e);
```

기존 코드:

```cpp
                if (ribbon.bPendingDelete || ribbon.fElapsed >= ribbon.fLifetime)
                    vecDelete.push_back(e);
```

아래로 교체:

```cpp
                if (ribbon.bPendingDelete || ribbon.fElapsed >= ribbon.fStartDelay + ribbon.fLifetime)
                    vecDelete.push_back(e);
```

기존 코드:

```cpp
                if (beam.bPendingDelete || beam.fLifetime <= 0.f || !beam.texturePath)
                    return;

                if (!beam.bMaterialReady)
                    beam.RefreshMaterialFromLegacyFields();

                const f32_t alpha = ComputeFadeAlpha(
                    beam.fElapsed, beam.fLifetime, beam.fFadeIn, beam.fFadeOut);
```

아래로 교체:

```cpp
                if (beam.bPendingDelete || beam.fLifetime <= 0.f || !beam.texturePath)
                    return;
                if (beam.fElapsed < beam.fStartDelay)
                    return;

                if (!beam.bMaterialReady)
                    beam.RefreshMaterialFromLegacyFields();

                const f32_t fAge = beam.fElapsed - beam.fStartDelay;
                const f32_t alpha = ComputeFadeAlpha(
                    fAge, beam.fLifetime, beam.fFadeIn, beam.fFadeOut);
```

기존 코드:

```cpp
                const Vec2 uvScroll = { beam.fUvScrollU * beam.fElapsed,
                                        (beam.fUvScrollV + beam.fUvScrollSpeed) * beam.fElapsed };
```

아래로 교체:

```cpp
                const Vec2 uvScroll = { beam.fUvScrollU * fAge,
                                        (beam.fUvScrollV + beam.fUvScrollSpeed) * fAge };
```

기존 코드:

```cpp
                const f32_t fNormalizedAge =
                    (beam.fLifetime > 0.f) ? std::clamp(beam.fElapsed / beam.fLifetime, 0.f, 1.f) : 0.f;
```

아래로 교체:

```cpp
                const f32_t fNormalizedAge =
                    (beam.fLifetime > 0.f) ? std::clamp(fAge / beam.fLifetime, 0.f, 1.f) : 0.f;
```

기존 코드:

```cpp
                    beam.fElapsed,
                    fNormalizedAge);
```

아래로 교체:

```cpp
                    fAge,
                    fNormalizedAge);
```

기존 코드:

```cpp
                if (ribbon.bPendingDelete || ribbon.fLifetime <= 0.f ||
                    !ribbon.texturePath || ribbon.iPointCount < 2)
                    return;

                if (!ribbon.bMaterialReady)
                    ribbon.RefreshMaterialFromLegacyFields();

                const f32_t alpha = ComputeFadeAlpha(
                    ribbon.fElapsed, ribbon.fLifetime, ribbon.fFadeIn, ribbon.fFadeOut);
```

아래로 교체:

```cpp
                if (ribbon.bPendingDelete || ribbon.fLifetime <= 0.f ||
                    !ribbon.texturePath || ribbon.iPointCount < 2)
                    return;
                if (ribbon.fElapsed < ribbon.fStartDelay)
                    return;

                if (!ribbon.bMaterialReady)
                    ribbon.RefreshMaterialFromLegacyFields();

                const f32_t fAge = ribbon.fElapsed - ribbon.fStartDelay;
                const f32_t alpha = ComputeFadeAlpha(
                    fAge, ribbon.fLifetime, ribbon.fFadeIn, ribbon.fFadeOut);
```

기존 코드:

```cpp
                const Vec2 uvScroll = { ribbon.fUvScrollU * ribbon.fElapsed,
                                        ribbon.fUvScrollV * ribbon.fElapsed };
```

아래로 교체:

```cpp
                const Vec2 uvScroll = { ribbon.fUvScrollU * fAge,
                                        ribbon.fUvScrollV * fAge };
```

기존 코드:

```cpp
                const f32_t fNormalizedAge =
                    (ribbon.fLifetime > 0.f) ? std::clamp(ribbon.fElapsed / ribbon.fLifetime, 0.f, 1.f) : 0.f;
```

아래로 교체:

```cpp
                const f32_t fNormalizedAge =
                    (ribbon.fLifetime > 0.f) ? std::clamp(fAge / ribbon.fLifetime, 0.f, 1.f) : 0.f;
```

기존 코드:

```cpp
                    ribbon.fElapsed,
                    fNormalizedAge);
```

아래로 교체:

```cpp
                    fAge,
                    fNormalizedAge);
```

1-14. C:/Users/user/Desktop/Winters/Client/Public/GameObject/FX/FxCuePlayer.h

기존 코드:

```cpp
class CWorld;
```

아래에 추가:

```cpp
namespace Engine
{
    class CFxStaticMeshRenderer;
}
```

기존 코드:

```cpp
    Vec3 vVelocity{};
    EntityID attachTo = NULL_ENTITY;
    bool_t bOverrideVelocity = false;
    bool_t bOverrideLifetime = false;
    f32_t fLifetimeOverride = 0.f;
```

아래로 교체:

```cpp
    Vec3 vVelocity{};
    Vec3 vEndWorldPos{};
    EntityID attachTo = NULL_ENTITY;
    Engine::CFxStaticMeshRenderer* pFxMeshRenderer = nullptr;
    bool_t bOverrideVelocity = false;
    bool_t bOverrideLifetime = false;
    bool_t bOverrideEndWorldPos = false;
    bool_t bOverrideSize = false;
    f32_t fLifetimeOverride = 0.f;
    f32_t fWidthOverride = 1.f;
    f32_t fHeightOverride = 1.f;
```

1-15. C:/Users/user/Desktop/Winters/Client/Private/GameObject/FX/FxCuePlayer.cpp

기존 코드:

```cpp
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxSystem.h"
```

아래로 교체:

```cpp
#include "GameObject/FX/FxBeamComponent.h"
#include "GameObject/FX/FxBeamSystem.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxMeshComponent.h"
#include "GameObject/FX/FxMeshSystem.h"
#include "GameObject/FX/FxRibbonComponent.h"
#include "GameObject/FX/FxSystem.h"
```

기존 코드:

```cpp
#include <Windows.h>
#include <cstdio>
```

아래로 교체:

```cpp
#include <Windows.h>
#include <algorithm>
#include <cstdio>
```

기존 코드:

```cpp
        fx.fWidth = emitter.fWidth;
        fx.fHeight = emitter.fHeight;
```

아래로 교체:

```cpp
        fx.fWidth = ctx.bOverrideSize ? ctx.fWidthOverride : emitter.fWidth;
        fx.fHeight = ctx.bOverrideSize ? ctx.fHeightOverride : emitter.fHeight;
```

기존 코드:

```cpp
        fx.fFadeIn = emitter.fFadeIn;
        fx.fFadeOut = emitter.fFadeOut;
```

아래로 교체:

```cpp
        fx.fFadeIn = emitter.fFadeIn;
        fx.fStartDelay = emitter.fStartDelay;
        fx.fFadeOut = emitter.fFadeOut;
```

`FxBillboardComponent BuildCueBillboard(...)` 함수 바로 아래에 추가:

```cpp
    FxBeamComponent BuildCueBeam(
        const FxAsset& asset,
        const FxEmitterDesc& emitter,
        u32_t emitterIndex,
        const FxCueContext& ctx)
    {
        const Vec3 vForward = WintersMath::NormalizeXZOrZero(ctx.vForward);
        const Vec3 start = ApplyCueOffset(ctx.vWorldPos, emitter.vAttachOffset, vForward);
        const f32_t fallbackLength = ctx.bOverrideSize
            ? ctx.fHeightOverride
            : ((emitter.fHeight > 0.f) ? emitter.fHeight : 1.f);
        const Vec3 end = ctx.bOverrideEndWorldPos
            ? ctx.vEndWorldPos
            : ApplyCueOffset(start, Vec3{ 0.f, 0.f, fallbackLength }, vForward);

        FxBeamComponent beam{};
        beam.hAsset = asset.handle;
        beam.iEmitterIndex = emitterIndex;
        beam.hStart = ctx.attachTo;
        beam.vStartWorldPos = start;
        beam.vEndWorldPos = end;
        beam.vStartOffset = emitter.vAttachOffset;
        beam.vEndOffset = ctx.bOverrideEndWorldPos
            ? Vec3{ end.x - ctx.vWorldPos.x, end.y - ctx.vWorldPos.y, end.z - ctx.vWorldPos.z }
            : emitter.vEndOffset;
        beam.vVelocity = ctx.bOverrideVelocity ? ctx.vVelocity : emitter.vVelocity;
        beam.SetTexturePath(emitter.strTexturePath);
        beam.fWidth = ctx.bOverrideSize ? ctx.fWidthOverride : emitter.fWidth;
        beam.fLifetime = ctx.bOverrideLifetime ? ctx.fLifetimeOverride : emitter.fLifetime;
        beam.fStartDelay = emitter.fStartDelay;
        beam.fFadeIn = emitter.fFadeIn;
        beam.fFadeOut = emitter.fFadeOut;
        beam.fUvScrollSpeed = emitter.fUvScrollV;
        beam.fUvScrollU = emitter.fUvScrollU;
        beam.fUvScrollV = emitter.fUvScrollV;
        beam.vColor = emitter.vColor;
        beam.blendMode = emitter.blendMode;
        beam.bBlockableByWindWall = emitter.bBlockableByWindWall;
        beam.fAlphaClip = emitter.fAlphaClip;
        beam.fErodeThreshold = emitter.fErodeThreshold;
        beam.SetMaterialFromDesc(emitter.material, emitter.depthMode);
        return beam;
    }

    FxMeshComponent BuildCueMesh(
        const FxEmitterDesc& emitter,
        const FxCueContext& ctx,
        FxAssetHandle handle,
        u32_t emitterIndex)
    {
        const Vec3 vForward = WintersMath::NormalizeXZOrZero(ctx.vForward);

        FxMeshComponent mesh{};
        mesh.hAsset = handle;
        mesh.iEmitterIndex = emitterIndex;
        mesh.vWorldPos = ctx.attachTo != NULL_ENTITY
            ? ctx.vWorldPos
            : ApplyCueOffset(ctx.vWorldPos, emitter.vAttachOffset, vForward);
        mesh.attachTo = ctx.attachTo;
        mesh.vAttachOffset = emitter.vAttachOffset;
        mesh.vVelocity = ctx.bOverrideVelocity ? ctx.vVelocity : emitter.vVelocity;
        mesh.vScale = emitter.vScale;
        mesh.vRotation = emitter.vRotation;
        if (vForward.x != 0.f || vForward.z != 0.f)
            mesh.vRotation.y += WintersMath::YawFromDirectionXZ(vForward);
        mesh.SetModelPath(emitter.strModelPath);
        mesh.SetTexturePath(emitter.strTexturePath);
        mesh.SetErodeTexturePath(emitter.strErodeTexturePath);
        mesh.fLifetime = ctx.bOverrideLifetime ? ctx.fLifetimeOverride : emitter.fLifetime;
        mesh.fStartDelay = emitter.fStartDelay;
        mesh.blendMode = emitter.blendMode;
        mesh.fFadeIn = emitter.fFadeIn;
        mesh.fFadeOut = emitter.fFadeOut;
        mesh.SetMaterialFromDesc(emitter.material, emitter.depthMode);
        mesh.bBlockableByWindWall = emitter.bBlockableByWindWall;
        return mesh;
    }

    FxRibbonComponent BuildCueRibbon(
        const FxAsset& asset,
        const FxEmitterDesc& emitter,
        u32_t emitterIndex,
        const FxCueContext& ctx)
    {
        const Vec3 vForward = WintersMath::NormalizeXZOrZero(ctx.vForward);
        const Vec3 start = ApplyCueOffset(ctx.vWorldPos, emitter.vAttachOffset, vForward);
        const f32_t fallbackLength = ctx.bOverrideSize
            ? ctx.fHeightOverride
            : ((emitter.fHeight > 0.f) ? emitter.fHeight : 1.f);
        const Vec3 end = ctx.bOverrideEndWorldPos
            ? ctx.vEndWorldPos
            : ApplyCueOffset(start, Vec3{ 0.f, 0.f, fallbackLength }, vForward);

        FxRibbonComponent ribbon{};
        ribbon.hAsset = asset.handle;
        ribbon.iEmitterIndex = emitterIndex;
        ribbon.attachTo = ctx.attachTo;
        ribbon.vStartOffset = emitter.vAttachOffset;
        ribbon.vEndOffset = ctx.bOverrideEndWorldPos
            ? Vec3{ end.x - ctx.vWorldPos.x, end.y - ctx.vWorldPos.y, end.z - ctx.vWorldPos.z }
            : emitter.vEndOffset;
        ribbon.vVelocity = ctx.bOverrideVelocity ? ctx.vVelocity : emitter.vVelocity;
        ribbon.SetTexturePath(emitter.strTexturePath);
        ribbon.fWidth = ctx.bOverrideSize ? ctx.fWidthOverride : emitter.fWidth;
        ribbon.fLifetime = ctx.bOverrideLifetime ? ctx.fLifetimeOverride : emitter.fLifetime;
        ribbon.fStartDelay = emitter.fStartDelay;
        ribbon.fFadeIn = emitter.fFadeIn;
        ribbon.fFadeOut = emitter.fFadeOut;
        ribbon.fUvScrollU = emitter.fUvScrollU;
        ribbon.fUvScrollV = emitter.fUvScrollV;
        ribbon.vColor = emitter.vColor;
        ribbon.blendMode = emitter.blendMode;
        ribbon.bBlockableByWindWall = emitter.bBlockableByWindWall;
        ribbon.fAlphaClip = emitter.fAlphaClip;
        ribbon.fErodeThreshold = emitter.fErodeThreshold;
        ribbon.SetMaterialFromDesc(emitter.material, emitter.depthMode);

        const u32_t pointCount = std::clamp(emitter.iRibbonPointCount, 2u, FX_RIBBON_MAX_POINTS);
        for (u32_t pointIndex = 0; pointIndex < pointCount; ++pointIndex)
        {
            const f32_t t = (pointCount > 1u)
                ? static_cast<f32_t>(pointIndex) / static_cast<f32_t>(pointCount - 1u)
                : 0.f;
            ribbon.SetPoint(pointIndex, Vec3{
                start.x + (end.x - start.x) * t,
                start.y + (end.y - start.y) * t,
                start.z + (end.z - start.z) * t
            });
        }

        return ribbon;
    }

    void LogSkippedCueEmitter(const char* pszCueName, const FxEmitterDesc& emitter)
    {
        char szBuffer[256]{};
        sprintf_s(szBuffer, "[FxCuePlayer] Skipped cue emitter cue=%s emitter=%s type=%u\n",
            pszCueName ? pszCueName : "(null)",
            emitter.strName.empty() ? "(unnamed)" : emitter.strName.c_str(),
            static_cast<u32_t>(emitter.renderType));
        OutputDebugStringA(szBuffer);
    }
```

기존 코드:

```cpp
        const FxEmitterDesc& emitter = pAsset->emitters[i];
        if (!IsCueBillboardType(emitter.renderType))
            continue;

        const EntityID entity = CFxSystem::Spawn(
            world,
            BuildCueBillboard(*pAsset, emitter, i, ctx));
        if (firstEntity == NULL_ENTITY)
            firstEntity = entity;
```

아래로 교체:

```cpp
        const FxEmitterDesc& emitter = pAsset->emitters[i];
        EntityID entity = NULL_ENTITY;
        if (IsCueBillboardType(emitter.renderType))
        {
            entity = CFxSystem::Spawn(
                world,
                BuildCueBillboard(*pAsset, emitter, i, ctx));
        }
        else if (emitter.renderType == eFxRenderType::Beam)
        {
            entity = CFxBeamSystem::Spawn(
                world,
                BuildCueBeam(*pAsset, emitter, i, ctx));
        }
        else if (emitter.renderType == eFxRenderType::Ribbon)
        {
            entity = CFxBeamSystem::Spawn(
                world,
                BuildCueRibbon(*pAsset, emitter, i, ctx));
        }
        else if (emitter.renderType == eFxRenderType::MeshParticle)
        {
            if (ctx.pFxMeshRenderer)
                entity = CFxMeshSystem::Spawn(
                    world,
                    ctx.pFxMeshRenderer,
                    BuildCueMesh(emitter, ctx, pAsset->handle, i));
            else
                LogSkippedCueEmitter(pszCueName, emitter);
        }
        else
        {
            LogSkippedCueEmitter(pszCueName, emitter);
        }

        if (firstEntity == NULL_ENTITY)
            firstEntity = entity;
```

1-16. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/EventApplier.cpp

기존 코드:

```cpp
        FxCueContext fx{};
        fx.vWorldPos = pos;
        fx.vForward = WintersMath::Normalize3D(Vec3{ ev->dirX(), ev->dirY(), ev->dirZ() });
        fx.attachTo = bKeepEventPosition ? NULL_ENTITY : attachTo;
        fx.bOverrideLifetime = true;
        fx.fLifetimeOverride = lifetime;
```

아래로 교체:

```cpp
        FxCueContext fx{};
        fx.vWorldPos = pos;
        fx.vForward = WintersMath::Normalize3D(Vec3{ ev->dirX(), ev->dirY(), ev->dirZ() });
        fx.attachTo = bKeepEventPosition ? NULL_ENTITY : attachTo;
        fx.pFxMeshRenderer = m_pFxMeshRenderer;
        fx.bOverrideLifetime = true;
        fx.fLifetimeOverride = lifetime;
```

1-17. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Irelia/Irelia_Skills.cpp

기존 코드:

```cpp
        IreliaFx::SpawnECloseLayers(world, p1, p2, t.fEConnectLifetime);
```

아래로 교체:

```cpp
        FxCueContext fx{};
        fx.vWorldPos = p1;
        fx.vEndWorldPos = p2;
        fx.vForward = WintersMath::NormalizeXZOrZero(
            Vec3{ p2.x - p1.x, 0.f, p2.z - p1.z });
        fx.pFxMeshRenderer = pFxMeshRenderer;
        fx.bOverrideEndWorldPos = true;
        fx.bOverrideLifetime = true;
        fx.fLifetimeOverride = t.fEConnectLifetime;
        CFxCuePlayer::Play(world, "Irelia.E.Connect", fx);
```

기존 코드:

```cpp
#include "GameObject/FX/FxMeshComponent.h"
#include "GameObject/FX/FxMeshSystem.h"
```

아래에 추가:

```cpp
#include "GameObject/FX/FxCuePlayer.h"
```

확인 필요:
- 이 변경은 기존 `IreliaFx::SpawnECloseLayers`를 바로 삭제하지 않고, WFX cue가 정상 재생되는 동안 fallback으로 남길지 결정해야 한다.
- 첫 적용은 `CFxCuePlayer::Play(...)` 실패 시 기존 `SpawnECloseLayers(...)`를 호출하는 fallback 형태가 가장 안전하다.

1-18. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Irelia/IreliaBladeSystem.cpp

기존 코드:

```cpp
    IreliaFx::IreliaEPlacedFxIds eFx = IreliaFx::SpawnEPlacedLayers(world, vRaisedGround,
```

아래에 추가:

```cpp
    FxCueContext fx{};
    fx.vWorldPos = vRaisedGround;
    fx.vForward = forward;
    fx.pFxMeshRenderer = pRenderer;
    CFxCuePlayer::Play(world, "Irelia.E.Place", fx);
```

기존 코드:

```cpp
#include "GameObject/FX/FxMeshSystem.h"
#include "GameObject/FX/FxMeshComponent.h"
```

아래에 추가:

```cpp
#include "GameObject/FX/FxCuePlayer.h"
```

확인 필요:
- 위 추가 위치의 `forward` 변수명이 실제 함수 스코프에 없으면 기존 칼날 yaw 계산에 쓰는 방향 변수명으로 맞춘다.
- WFX place cue가 정상 확인되면 `IreliaFx::SpawnEPlacedLayers`는 fallback 또는 제거 대상으로 분리한다.

1-19. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Irelia/e_place.wfx

새 파일:

```json
{
  "name": "Irelia.E.Place",
  "emitters": [
    {
      "name": "e_place_ground_ring",
      "render_type": "GroundDecal",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_temp_e_ring_indicator_v2.png",
      "lifetime": 3.5,
      "width": 3.15,
      "height": 3.15,
      "color": [0.45, 0.72, 1.65, 0.34],
      "attach_offset": [0.0, -2.95, 0.0],
      "fade_in": 0.06,
      "fade_out": 0.32,
      "billboard": false
    },
    {
      "name": "e_place_blade_glow",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_p_blade_glow.png",
      "lifetime": 0.42,
      "width": 1.85,
      "height": 1.85,
      "color": [0.85, 0.96, 2.10, 0.74],
      "attach_offset": [0.0, 0.15, 0.0],
      "fade_in": 0.03,
      "fade_out": 0.25,
      "billboard": true
    },
    {
      "name": "e_place_violet_edge",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_p_gradient.png",
      "lifetime": 0.52,
      "start_delay": 0.04,
      "width": 2.35,
      "height": 2.35,
      "color": [0.75, 0.42, 1.85, 0.42],
      "attach_offset": [0.0, 0.32, 0.0],
      "fade_in": 0.02,
      "fade_out": 0.31,
      "billboard": true
    },
    {
      "name": "e_place_spark",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_e_warnig_spark.png",
      "lifetime": 0.36,
      "start_delay": 0.02,
      "width": 1.35,
      "height": 1.35,
      "color": [0.92, 1.06, 2.35, 1.0],
      "attach_offset": [0.0, 0.55, 0.0],
      "fade_in": 0.01,
      "fade_out": 0.22,
      "billboard": true
    }
  ]
}
```

1-20. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Irelia/e_connect.wfx

새 파일:

```json
{
  "name": "Irelia.E.Connect",
  "emitters": [
    {
      "name": "e_connect_dark_rail",
      "render_type": "Beam",
      "blend_mode": "AlphaBlend",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_e_stun_beam_dark.png",
      "lifetime": 0.72,
      "width": 1.38,
      "height": 6.0,
      "color": [0.02, 0.06, 0.36, 0.58],
      "attach_offset": [0.0, -2.90, 0.0],
      "fade_in": 0.02,
      "fade_out": 0.38,
      "uv_scroll": [0.0, -0.30]
    },
    {
      "name": "e_connect_core_rail",
      "render_type": "Beam",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_e_stun_beam.png",
      "lifetime": 0.42,
      "start_delay": 0.05,
      "width": 0.42,
      "height": 6.0,
      "color": [0.62, 0.74, 2.05, 0.78],
      "attach_offset": [0.0, -2.88, 0.0],
      "fade_in": 0.02,
      "fade_out": 0.28,
      "uv_scroll": [0.0, -0.65]
    },
    {
      "name": "e_connect_afterglow",
      "render_type": "Beam",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_e_stun_beam_dark.png",
      "lifetime": 0.90,
      "start_delay": 0.12,
      "width": 2.15,
      "height": 6.0,
      "color": [0.38, 0.58, 1.70, 0.30],
      "attach_offset": [0.0, -2.87, 0.0],
      "fade_in": 0.05,
      "fade_out": 0.48,
      "uv_scroll": [0.0, -0.16]
    },
    {
      "name": "e_connect_mid_flash",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_e_warnig_spark.png",
      "lifetime": 0.24,
      "start_delay": 0.18,
      "width": 2.10,
      "height": 2.10,
      "color": [1.00, 1.18, 2.45, 0.98],
      "attach_offset": [0.0, 0.75, 0.0],
      "fade_in": 0.01,
      "fade_out": 0.18,
      "billboard": true
    },
    {
      "name": "e_connect_blade_mesh",
      "render_type": "MeshParticle",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "model": "Client/Bin/Resource/Texture/FX/Irelia/fbx/irelia_base_e_beam.fbx",
      "texture": "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_e_beam_mult.png",
      "lifetime": 0.44,
      "start_delay": 0.04,
      "scale": [0.04, 0.04, 0.04],
      "rotation": [0.0, 0.0, 0.0],
      "color": [0.62, 0.74, 2.05, 0.78],
      "attach_offset": [0.0, 0.12, 0.0],
      "fade_in": 0.02,
      "fade_out": 0.28
    }
  ]
}
```

1-21. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Irelia/e_connect_pop.wfx

새 파일:

```json
{
  "name": "Irelia.E.ConnectPop",
  "emitters": [
    {
      "name": "e_connect_pop_spark_a",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_e_warnig_spark.png",
      "lifetime": 0.34,
      "width": 1.35,
      "height": 1.35,
      "color": [0.92, 1.06, 2.35, 1.0],
      "attach_offset": [0.0, 0.65, 0.0],
      "fade_in": 0.01,
      "fade_out": 0.22,
      "billboard": true
    },
    {
      "name": "e_connect_pop_ring",
      "render_type": "ShockwaveRing",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_temp_e_ring_indicator_v2.png",
      "lifetime": 0.55,
      "start_delay": 0.03,
      "start_radius": 0.35,
      "end_radius": 1.55,
      "thickness": 0.12,
      "width": 1.0,
      "height": 1.0,
      "color": [0.55, 0.68, 1.85, 0.40],
      "attach_offset": [0.0, -2.92, 0.0],
      "fade_in": 0.02,
      "fade_out": 0.34,
      "billboard": false
    }
  ]
}
```

1-22. C:/Users/user/Desktop/Winters/Client/Private/GameObject/FX/FxLegacyManifest.cpp

기존 코드:

```cpp
        { "Irelia.E.Blade", "Irelia", "IreliaFx::SpawnEBlade", "Client/Private/GameObject/Champion/Irelia/IreliaFxPresets.cpp", "MeshParticle + GroundDecal", "Client/Bin/Resource/Texture/FX/Irelia/fbx/irelia_base_e_blade.fbx", "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_e_blades_erode.png", "LegacyOnly" },
        { "Irelia.E.Beam", "Irelia", "IreliaFx::SpawnECloseLayers", "Client/Private/GameObject/Champion/Irelia/IreliaFxPresets.cpp", "Beam + Billboard", "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_e_stun_beam.png", "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_e_stun_beam_dark.png", "LegacyOnly" },
```

아래로 교체:

```cpp
        { "Irelia.E.Place", "Irelia", "CFxCuePlayer::Play", "Data/LoL/FX/Champions/Irelia/e_place.wfx", "GroundDecal + Billboard", "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_temp_e_ring_indicator_v2.png", "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_p_blade_glow.png", "WfxPilot" },
        { "Irelia.E.Connect", "Irelia", "CFxCuePlayer::Play", "Data/LoL/FX/Champions/Irelia/e_connect.wfx", "Beam + MeshParticle + Billboard", "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_e_stun_beam.png", "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_e_beam_mult.png", "WfxPilot" },
```

확인 필요:
- `FxLegacyManifest.cpp`의 실제 배열 항목 문자열이 위와 다르면 기존 Irelia E 항목 anchor를 실제 문자열 기준으로 맞춘다.
- 리신처럼 얇은 `VisualHookContext -> cue name -> WFX` 패턴을 기준으로 삼고, 제드처럼 모델 상태가 필요한 부분만 C++ 커스텀을 남긴다.
- 구현 기준 에셋 매핑:
  - 레퍼런스 1~3의 양끝 칼날 배치와 보라/청백 글로우는 기존 `CIreliaBladeSystem::SpawnPlaced`의 `irelia_base_e_blade.fbx` + `irelia_base_blades_passive_4_texture.png`를 유지하고, `Irelia.E.Place` WFX의 `irelia_base_temp_e_ring_indicator_v2.png`, `irelia_base_p_blade_glow.png`, `irelia_base_p_gradient.png`, `irelia_base_e_warnig_spark.png` 레이어를 추가한다.
  - 레퍼런스 2~4의 짙은 남청 연결 레일과 청백 중심선은 `Irelia.E.Connect` WFX의 `irelia_base_e_stun_beam_dark.png`, `irelia_base_e_stun_beam.png` Beam emitter가 담당한다.
  - 레퍼런스 4~6의 중앙 섬광과 수렴 감속은 `Irelia.E.Connect`/`Irelia.E.ConnectPop`의 `irelia_base_e_warnig_spark.png`, `irelia_base_temp_e_ring_indicator_v2.png`, `start_delay` 레이어가 담당한다.
  - 레퍼런스 5~6의 넓은 칼날형 중앙 판은 `irelia_base_e_beam.fbx` + `irelia_base_e_beam_mult.png` MeshParticle로 먼저 구현한다.
  - `irelia_base_e_blade_01.fbx`, `irelia_base_e_blade_02.fbx`, `irelia_base_e_blade_03.fbx`, `irelia_base_e_cast.fbx`, `irelia_base_e_mis_mesh.fbx`, `irelia_base_temp_e_tar_blade_indicator_typhoon.fbx`는 현재 계획의 1차 구현 필수 경로가 아니다. 원작 1:1 추가 폴리시 때 비교 후보로 남기고, 1차 구현은 기존 배치 칼날 FBX + WFX 레이어로 검증한다.

2. 검증

미검증:
- 계획서 작성만 완료, 코드 빌드 미검증.
- 업로드된 레퍼런스 원본 파일은 현재 로컬 파일시스템에서 확인되지 않아 `01.png` 같은 순번 저장 미검증.
- E stage1/stage2가 서버 `EffectTrigger`로 한 번씩만 도착하고 클라이언트에서 중복 재생되지 않는지 미검증.
- `CFxCuePlayer`가 `Billboard/GroundDecal/ShockwaveRing` 외에 `Beam/Ribbon/MeshParticle` WFX emitter를 실제로 재생하는지 미검증.
- `start_delay`가 `FxBillboardComponent`, `FxBeamComponent`, `FxRibbonComponent`, `FxMeshComponent`에서 같은 의미로 적용되는지 미검증.
- `Data/LoL/FX/Champions/Irelia/e_place.wfx`, `e_connect.wfx`, `e_connect_pop.wfx` 신규 WFX 로드 미검증.

검증 명령:

```powershell
git diff --check
rg -n "Irelia.E.Place|Irelia.E.Connect|start_delay|MeshParticle|Beam|Ribbon" Data/LoL/FX/Champions/Irelia Client/Private/GameObject/FX Engine/Private/FX Engine/Public/FX
MSBuild.exe Winters.sln /m /p:Configuration=Debug /p:Platform=x64
```

수동 확인:
- F5 서버 권위 플레이에서 이렐리아 E 1타를 찍으면 양끝 배치 링/칼날 글로우가 3.5초 유지되는지 확인한다.
- E 2타를 찍으면 어두운 남청 레일이 먼저 깔리고, 양끝 청백 칼날 잔상이 중앙으로 수렴한 뒤 중앙 플래시가 터지는지 확인한다.
- 같은 순간 로컬 클라이언트와 원격 클라이언트에서 FX가 한 번씩만 재생되는지 `OutputDebugStringA` 로그와 화면을 같이 확인한다.
- `FxCuePlayer` 로그에 `Skipped cue emitter`가 뜨지 않는지 확인한다. 뜬다면 해당 WFX emitter type이 아직 공용 cue path에 연결되지 않은 것이다.
- 리신 Q/E와 제드 Q/W/E/R WFX가 회귀 없이 그대로 재생되는지 확인한다.
- `spell3_b.wanim`, `spell3_b_to_idle.wanim`, `spell3_b_run.wanim`이 실제 애니메이션 키 `spell3_b`, `spell3_b_to_idle`, `spell3_b_run`으로 로드되는지 확인한다.

확인 필요:
- `Irelia_Registration.cpp`의 E stage2 종료 전환을 stage별로 나누려면 `SkillDef`에 stage2 전용 transition 필드가 필요한지 확인한다.
- WFX 데이터 파일은 빌드 컴파일 항목이 아니므로 `.vcxproj` 등록은 필요 없을 가능성이 높지만, 패키징/복사 파이프라인이 `Data/LoL/FX/Champions/Irelia/*.wfx`를 런타임 위치에서 읽는지 확인한다.
- Engine public header `FxAsset.h` 변경 후 EngineSDK 동기화가 필요한지 확인한다.
