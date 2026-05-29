Session - 이렐리아 W 레퍼런스 01-04의 차징 보호막, 금빛 회전 링, 전방 백금색 칼날 파동을 기존 단일 서버 EffectTrigger 시각 경로에 맞춰 반영한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Client/Public/GameObject/Champion/Irelia/Irelia_Tuning.h

기존 코드:

```cpp
        f32_t wLayerLifetime = 0.6f;
        f32_t wLayerSize = 2.0f;
        Vec4 wLayerBladesColor{ 1.00f, 1.00f, 1.00f, 1.00f };
        Vec4 wLayerGlowColor{ 0.60f, 0.90f, 1.60f, 1.00f };
        f32_t fWHoldShieldSize = 3.2f;
        f32_t fWHoldGlowSize = 3.8f;
        Vec4 vWHoldShieldColor{ 0.45f, 0.90f, 1.40f, 0.45f };
        Vec4 vWHoldGlowColor{ 0.35f, 0.85f, 1.40f, 0.30f };
```

아래로 교체:

```cpp
        f32_t wLayerLifetime = 0.42f;
        f32_t wLayerSize = 3.20f;
        Vec4 wLayerBladesColor{ 1.30f, 1.18f, 0.82f, 0.96f };
        Vec4 wLayerGlowColor{ 0.70f, 0.78f, 1.95f, 0.70f };
        f32_t fWHoldShieldSize = 3.40f;
        f32_t fWHoldGlowSize = 4.10f;
        Vec4 vWHoldShieldColor{ 0.56f, 0.88f, 1.60f, 0.42f };
        Vec4 vWHoldGlowColor{ 0.34f, 0.60f, 1.35f, 0.28f };
        f32_t fWHoldInnerSize = 2.55f;
        f32_t fWHoldDarkSize = 2.85f;
        f32_t fWHoldGoldRingSize = 3.35f;
        f32_t fWHoldChargeSize = 3.90f;
        f32_t fWHoldAtlasFps = 16.0f;
        Vec4 vWHoldInnerColor{ 0.62f, 0.78f, 1.70f, 0.74f };
        Vec4 vWHoldDarkColor{ 0.06f, 0.06f, 0.26f, 0.42f };
        Vec4 vWHoldGoldRingColor{ 1.20f, 1.05f, 0.48f, 0.48f };
        Vec4 vWHoldChargeColor{ 0.90f, 1.05f, 1.90f, 0.42f };
        f32_t fWReleaseForwardDist = 2.45f;
        f32_t fWReleaseBladeWidth = 5.50f;
        f32_t fWReleaseBladeHeight = 2.25f;
        f32_t fWReleaseFlashSize = 3.60f;
        f32_t fWReleaseStreakWidth = 0.72f;
        f32_t fWReleaseStreakLength = 6.40f;
        f32_t fWReleaseShardSize = 2.30f;
        Vec4 vWReleaseCoreColor{ 1.35f, 1.24f, 0.88f, 0.98f };
        Vec4 vWReleaseEdgeColor{ 0.58f, 0.64f, 1.90f, 0.55f };
        Vec4 vWReleaseAfterglowColor{ 0.85f, 0.95f, 1.80f, 0.36f };
```

1-2. C:/Users/user/Desktop/Winters/Client/Public/GameObject/Champion/Irelia/IreliaFxPresets.h

기존 코드:

```cpp
	struct IreliaWHoldFxIds
	{
		EntityID spinFxID = NULL_ENTITY;
		EntityID shieldFxID = NULL_ENTITY;
		EntityID glowFxID = NULL_ENTITY;
		EntityID blockFxID = NULL_ENTITY;
	};
```

아래로 교체:

```cpp
	struct IreliaWHoldFxIds
	{
		EntityID spinFxID = NULL_ENTITY;
		EntityID shieldFxID = NULL_ENTITY;
		EntityID glowFxID = NULL_ENTITY;
		EntityID blockFxID = NULL_ENTITY;
		EntityID darkFxID = NULL_ENTITY;
		EntityID goldFxID = NULL_ENTITY;
		EntityID chargeFxID = NULL_ENTITY;
	};
```

1-3. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Irelia/IreliaFxPresets.cpp

기존 코드:

```cpp
IreliaFx::IreliaWHoldFxIds IreliaFx::SpawnWSpinLayers(CWorld& world, EntityID owner, f32_t fLifetime)
{
    const Irelia::IreliaTuning& t = Irelia::GetTuning();

    FxBillboardComponent wfx{};
    wfx.attachTo = owner;
    wfx.vAttachOffset = { 0.f, 1.f, 0.f };
    wfx.texturePath = kPathWSpin;
    wfx.fWidth = 2.f;
    wfx.fHeight = 3.f;
    wfx.fLifetime = fLifetime;
    wfx.fFadeIn = 0.06f;
    wfx.fFadeOut = 0.25f;
    wfx.bBillboard = true;
    wfx.vColor = { 0.7f, 1.0f, 1.5f, 0.85f };
    wfx.blendMode = eBlendPreset::Additive;
    wfx.iAtlasCols = 2;
    wfx.iAtlasRows = 2;
    wfx.iAtlasFrameCount = 4;
    wfx.fAtlasFps = 12.f;
    wfx.bAtlasLoop = true;

    IreliaWHoldFxIds ids{};
    ids.spinFxID = SpawnRuntimeBillboard(world, wfx);

    FxBillboardComponent shield = wfx;
    shield.texturePath = kPathWShieldSoft;
    shield.fWidth = t.fWHoldShieldSize;
    shield.fHeight = t.fWHoldShieldSize;
    shield.fStartDelay = 0.03f;
    shield.vColor = t.vWHoldShieldColor;
    shield.iAtlasCols = 1;
    shield.iAtlasRows = 1;
    shield.iAtlasFrameCount = 1;
    shield.fAtlasFps = 0.f;
    ids.shieldFxID = SpawnRuntimeBillboard(world, shield);

    FxBillboardComponent glow = wfx;
    glow.texturePath = kPathWAmbientGlow;
    glow.fWidth = t.fWHoldGlowSize;
    glow.fHeight = t.fWHoldGlowSize;
    glow.fFadeIn = 0.12f;
    glow.fFadeOut = 0.35f;
    glow.fStartDelay = 0.06f;
    glow.vColor = t.vWHoldGlowColor;
    glow.iAtlasCols = 1;
    glow.iAtlasRows = 1;
    glow.iAtlasFrameCount = 1;
    glow.fAtlasFps = 0.f;
    ids.glowFxID = SpawnRuntimeBillboard(world, glow);

    FxBillboardComponent block = shield;
    block.texturePath = kPathWBlockMult;
    block.fWidth = t.fWHoldShieldSize * 0.85f;
    block.fHeight = t.fWHoldShieldSize * 0.85f;
    block.fStartDelay = 0.09f;
    block.fFadeIn = 0.05f;
    block.fFadeOut = 0.30f;
    block.vColor = { t.vWHoldShieldColor.x, t.vWHoldShieldColor.y, t.vWHoldShieldColor.z,
        t.vWHoldShieldColor.w * 0.75f };
    ids.blockFxID = SpawnRuntimeBillboard(world, block);

    return ids;
}
```

아래로 교체:

```cpp
IreliaFx::IreliaWHoldFxIds IreliaFx::SpawnWSpinLayers(CWorld& world, EntityID owner, f32_t fLifetime)
{
    const Irelia::IreliaTuning& t = Irelia::GetTuning();

    FxBillboardComponent base{};
    base.attachTo = owner;
    base.vAttachOffset = { 0.f, 1.f, 0.f };
    base.fLifetime = fLifetime;
    base.fFadeIn = 0.05f;
    base.fFadeOut = 0.28f;
    base.bBillboard = true;
    base.blendMode = eBlendPreset::Additive;
    base.iAtlasCols = 1;
    base.iAtlasRows = 1;
    base.iAtlasFrameCount = 1;
    base.fAtlasFps = 0.f;
    base.bAtlasLoop = true;

    IreliaWHoldFxIds ids{};

    FxBillboardComponent dark = base;
    dark.texturePath = kPathWSpin;
    dark.fWidth = t.fWHoldDarkSize;
    dark.fHeight = t.fWHoldDarkSize;
    dark.vColor = t.vWHoldDarkColor;
    dark.blendMode = eBlendPreset::AlphaBlend;
    dark.iAtlasCols = 2;
    dark.iAtlasRows = 2;
    dark.iAtlasFrameCount = 4;
    dark.fAtlasFps = t.fWHoldAtlasFps * 0.65f;
    ids.darkFxID = SpawnRuntimeBillboard(world, dark);

    FxBillboardComponent spin = base;
    spin.texturePath = kPathWSpin;
    spin.fWidth = t.fWHoldInnerSize;
    spin.fHeight = t.fWHoldInnerSize;
    spin.vColor = t.vWHoldInnerColor;
    spin.iAtlasCols = 2;
    spin.iAtlasRows = 2;
    spin.iAtlasFrameCount = 4;
    spin.fAtlasFps = t.fWHoldAtlasFps;
    ids.spinFxID = SpawnRuntimeBillboard(world, spin);

    FxBillboardComponent shield = base;
    shield.texturePath = kPathWShieldSoft;
    shield.fWidth = t.fWHoldShieldSize;
    shield.fHeight = t.fWHoldShieldSize;
    shield.fStartDelay = 0.03f;
    shield.vColor = t.vWHoldShieldColor;
    shield.blendMode = eBlendPreset::AlphaBlend;
    ids.shieldFxID = SpawnRuntimeBillboard(world, shield);

    FxBillboardComponent glow = base;
    glow.texturePath = kPathWAmbientGlow;
    glow.fWidth = t.fWHoldGlowSize;
    glow.fHeight = t.fWHoldGlowSize;
    glow.fFadeIn = 0.12f;
    glow.fFadeOut = 0.36f;
    glow.fStartDelay = 0.06f;
    glow.vColor = t.vWHoldGlowColor;
    ids.glowFxID = SpawnRuntimeBillboard(world, glow);

    FxBillboardComponent block = base;
    block.texturePath = kPathWBlockMult;
    block.fWidth = t.fWHoldGoldRingSize;
    block.fHeight = t.fWHoldGoldRingSize;
    block.fStartDelay = 0.07f;
    block.fFadeIn = 0.04f;
    block.fFadeOut = 0.32f;
    block.vColor = t.vWHoldGoldRingColor;
    ids.blockFxID = SpawnRuntimeBillboard(world, block);

    FxBillboardComponent gold = block;
    gold.texturePath = kPathWAmbientGlow;
    gold.fWidth = t.fWHoldGoldRingSize * 1.08f;
    gold.fHeight = t.fWHoldGoldRingSize * 1.08f;
    gold.fStartDelay = 0.12f;
    gold.vColor = { t.vWHoldGoldRingColor.x, t.vWHoldGoldRingColor.y,
        t.vWHoldGoldRingColor.z, t.vWHoldGoldRingColor.w * 0.55f };
    ids.goldFxID = SpawnRuntimeBillboard(world, gold);

    FxBillboardComponent charge = base;
    charge.texturePath = kPathShards;
    charge.fWidth = t.fWHoldChargeSize;
    charge.fHeight = t.fWHoldChargeSize;
    charge.fStartDelay = 0.10f;
    charge.fFadeIn = 0.08f;
    charge.fFadeOut = 0.30f;
    charge.vColor = t.vWHoldChargeColor;
    ids.chargeFxID = SpawnRuntimeBillboard(world, charge);

    return ids;
}
```

기존 코드:

```cpp
void IreliaFx::SpawnWReleaseLayers(CWorld& world, EntityID owner, f32_t fLifetime, f32_t fSize,
    const Vec4& vBladesColor, const Vec4& vGlowColor,
    const Vec3& vAttachOffset)
{
    // ★ DIAG (검증 후 제거 예정) — W2 release 호출 확인
    {
        char dbg[256];
        sprintf_s(dbg,
            "[WRel] Spawn owner=%u life=%.2f size=%.2f off=(%.2f,%.2f,%.2f) blades=(%.2f,%.2f,%.2f,%.2f) glow=(%.2f,%.2f,%.2f,%.2f)\n",
            owner, fLifetime, fSize,
            vAttachOffset.x, vAttachOffset.y, vAttachOffset.z,
            vBladesColor.x, vBladesColor.y, vBladesColor.z, vBladesColor.w,
            vGlowColor.x,   vGlowColor.y,   vGlowColor.z,   vGlowColor.w);
        ::OutputDebugStringA(dbg);
    }

    // Layer 1: swipe_blades (RGBA 칼날 실루엣, AlphaBlend)
    FxBillboardComponent base{};
    base.attachTo      = owner;
    base.vAttachOffset = vAttachOffset;
    base.texturePath   = kPathWSwipeBlades;
    base.fWidth        = fSize;
    base.fHeight       = fSize;
    base.fLifetime     = fLifetime;
    base.bBillboard    = true;
    base.vColor        = vBladesColor;
    base.blendMode     = eBlendPreset::AlphaBlend;
    SpawnBillboardAsset(world, base, "Irelia_W_Release_Blades");

    // Layer 2: mis_glow (luminance, Additive 블루 tint)
    FxBillboardComponent glow = base;
    glow.texturePath = kPathWMisGlow;
    glow.vColor      = vGlowColor;
    glow.blendMode   = eBlendPreset::Additive;
    SpawnBillboardAsset(world, glow, "Irelia_W_Release_Glow");

    FxBillboardComponent after = base;
    after.fStartDelay = 0.05f;
    after.fLifetime = fLifetime * 0.65f;
    after.fWidth = fSize * 1.15f;
    after.fHeight = fSize * 1.15f;
    after.fFadeOut = after.fLifetime * 0.75f;
    after.vColor = { vBladesColor.x, vBladesColor.y, vBladesColor.z, vBladesColor.w * 0.45f };
    SpawnRuntimeBillboard(world, after);

    FxBillboardComponent shard = base;
    shard.texturePath = kPathShards;
    shard.fStartDelay = 0.08f;
    shard.fLifetime = 0.28f;
    shard.fWidth = fSize * 0.7f;
    shard.fHeight = fSize * 0.7f;
    shard.fFadeOut = 0.22f;
    shard.vColor = vGlowColor;
    shard.blendMode = eBlendPreset::Additive;
    SpawnRuntimeBillboard(world, shard);

    // blade_erode 는 SpawnWStage2Slash 가 forward 슬래시로 2장 spawn — 여기선 제외.
}
```

아래로 교체:

```cpp
void IreliaFx::SpawnWReleaseLayers(CWorld& world, EntityID owner, f32_t fLifetime, f32_t fSize,
    const Vec4& vBladesColor, const Vec4& vGlowColor,
    const Vec3& vAttachOffset)
{
    const Irelia::IreliaTuning& t = Irelia::GetTuning();
    const f32_t lifetime = (fLifetime > 0.05f) ? fLifetime : t.wLayerLifetime;
    const f32_t size = (fSize > 0.1f) ? fSize : t.wLayerSize;

    FxBillboardComponent base{};
    base.attachTo = owner;
    base.vAttachOffset = vAttachOffset;
    base.texturePath = kPathWSwipeBlades;
    base.fWidth = t.fWReleaseBladeWidth;
    base.fHeight = t.fWReleaseBladeHeight;
    base.fLifetime = lifetime;
    base.fFadeIn = 0.015f;
    base.fFadeOut = lifetime * 0.72f;
    base.bBillboard = true;
    base.vColor = t.vWReleaseCoreColor;
    base.blendMode = eBlendPreset::AlphaBlend;
    SpawnBillboardAsset(world, base, "Irelia_W_Release_Blades");

    FxBillboardComponent glow = base;
    glow.texturePath = kPathWMisGlow;
    glow.fWidth = t.fWReleaseBladeWidth * 1.18f;
    glow.fHeight = t.fWReleaseBladeHeight * 1.25f;
    glow.vColor = t.vWReleaseAfterglowColor;
    glow.blendMode = eBlendPreset::Additive;
    SpawnBillboardAsset(world, glow, "Irelia_W_Release_Glow");

    FxBillboardComponent edge = base;
    edge.texturePath = kPathWBlade;
    edge.fWidth = t.fWReleaseStreakLength;
    edge.fHeight = t.fWReleaseStreakWidth;
    edge.fStartDelay = 0.04f;
    edge.vColor = t.vWReleaseEdgeColor;
    edge.blendMode = eBlendPreset::Additive;
    SpawnRuntimeBillboard(world, edge);

    FxBillboardComponent flash = base;
    flash.texturePath = kPathWAmbientGlow;
    flash.fWidth = t.fWReleaseFlashSize;
    flash.fHeight = t.fWReleaseFlashSize;
    flash.fStartDelay = 0.05f;
    flash.fLifetime = lifetime * 0.75f;
    flash.fFadeOut = flash.fLifetime * 0.75f;
    flash.vColor = vGlowColor;
    flash.blendMode = eBlendPreset::Additive;
    SpawnRuntimeBillboard(world, flash);

    FxBillboardComponent after = base;
    after.texturePath = kPathWBlockMult;
    after.fStartDelay = 0.07f;
    after.fLifetime = lifetime * 0.85f;
    after.fWidth = size * 1.15f;
    after.fHeight = size * 1.15f;
    after.fFadeOut = after.fLifetime * 0.78f;
    after.vColor = { vBladesColor.x, vBladesColor.y, vBladesColor.z, vBladesColor.w * 0.45f };
    after.blendMode = eBlendPreset::Additive;
    SpawnRuntimeBillboard(world, after);

    FxBillboardComponent shard = base;
    shard.texturePath = kPathShards;
    shard.fStartDelay = 0.09f;
    shard.fLifetime = 0.28f;
    shard.fWidth = t.fWReleaseShardSize;
    shard.fHeight = t.fWReleaseShardSize;
    shard.fFadeOut = 0.22f;
    shard.vColor = vGlowColor;
    shard.blendMode = eBlendPreset::Additive;
    SpawnRuntimeBillboard(world, shard);
}
```

기존 코드:

```cpp
void IreliaFx::SpawnWStage2Slash(CWorld& world, EntityID owner, const Vec3& vForward)
{
    FxBillboardComponent sfx1{};
    sfx1.attachTo      = owner;
    sfx1.vAttachOffset = { vForward.x * 2.0f, 1.0f, vForward.z * 2.0f };
    sfx1.vVelocity     = { 0.f, 0.f, 0.f };
    sfx1.texturePath   = kPathWBlade;
    sfx1.fWidth        = 4.0f;
    sfx1.fHeight       = 2.0f;
    sfx1.fLifetime     = 0.4f;
    sfx1.bBillboard    = true;
    SpawnBillboardAsset(world, sfx1, "Irelia_W_Stage2_Slash_A");

    FxBillboardComponent sfx2 = sfx1;
    sfx2.vAttachOffset.y += 0.2f;
    SpawnBillboardAsset(world, sfx2, "Irelia_W_Stage2_Slash_B");
}
```

아래로 교체:

```cpp
void IreliaFx::SpawnWStage2Slash(CWorld& world, EntityID owner, const Vec3& vForward)
{
    const Irelia::IreliaTuning& t = Irelia::GetTuning();

    FxBillboardComponent slash{};
    slash.attachTo = owner;
    slash.vAttachOffset = {
        vForward.x * (t.fWReleaseForwardDist - 0.35f),
        1.0f,
        vForward.z * (t.fWReleaseForwardDist - 0.35f)
    };
    slash.texturePath = kPathWBlade;
    slash.fWidth = t.fWReleaseBladeWidth;
    slash.fHeight = t.fWReleaseBladeHeight;
    slash.fLifetime = t.wLayerLifetime;
    slash.fFadeIn = 0.01f;
    slash.fFadeOut = t.wLayerLifetime * 0.70f;
    slash.bBillboard = true;
    slash.vColor = t.vWReleaseCoreColor;
    slash.blendMode = eBlendPreset::Additive;
    SpawnBillboardAsset(world, slash, "Irelia_W_Stage2_Slash_A");

    FxBillboardComponent violet = slash;
    violet.fStartDelay = 0.04f;
    violet.fWidth = t.fWReleaseBladeWidth * 1.08f;
    violet.fHeight = t.fWReleaseBladeHeight * 0.70f;
    violet.vAttachOffset.y += 0.16f;
    violet.vColor = t.vWReleaseEdgeColor;
    SpawnBillboardAsset(world, violet, "Irelia_W_Stage2_Slash_B");

    FxBillboardComponent gold = slash;
    gold.texturePath = kPathWAmbientGlow;
    gold.fWidth = t.fWReleaseFlashSize;
    gold.fHeight = t.fWReleaseFlashSize;
    gold.fStartDelay = 0.06f;
    gold.vColor = t.vWReleaseAfterglowColor;
    SpawnRuntimeBillboard(world, gold);
}
```

1-4. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Irelia/Irelia_Skills.cpp

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
    void ClearWHoldFx(CWorld& world)
    {
        MarkBillboardPendingDelete(world, s_state.wHoldFxIds.spinFxID);
        MarkBillboardPendingDelete(world, s_state.wHoldFxIds.shieldFxID);
        MarkBillboardPendingDelete(world, s_state.wHoldFxIds.glowFxID);
        MarkBillboardPendingDelete(world, s_state.wHoldFxIds.blockFxID);
        MarkBillboardPendingDelete(world, s_state.wHoldFxIds.darkFxID);
        MarkBillboardPendingDelete(world, s_state.wHoldFxIds.goldFxID);
        MarkBillboardPendingDelete(world, s_state.wHoldFxIds.chargeFxID);
        s_state.wHoldFxIds = IreliaFx::IreliaWHoldFxIds{};
        s_state.wSpinFxId = NULL_ENTITY;
    }
```

기존 코드:

```cpp
            constexpr f32_t kForwardDist = 2.0f;
            const Vec3 attachOffset{
                forward.x * kForwardDist,
                1.0f,
                forward.z * kForwardDist
            };
```

아래로 교체:

```cpp
            const Vec3 attachOffset{
                forward.x * t.fWReleaseForwardDist,
                1.0f,
                forward.z * t.fWReleaseForwardDist
            };
```

기존 코드:

```cpp
        const f32_t lifetime =
            (ctx.pDef && ctx.pDef->stageWindowSec > 0.f) ? ctx.pDef->stageWindowSec + 0.5f : 4.5f;
```

아래로 교체:

```cpp
        const f32_t lifetime =
            (ctx.pDef && ctx.pDef->stageWindowSec > 0.f) ? ctx.pDef->stageWindowSec + 0.35f : 1.85f;
```

1-5. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Irelia/Irelia_Registration.cpp

기존 코드:

```cpp
                s.lockDurationSec = 5.f;
                s.bOneShot = true;
                s.rotate = eRotateMode::TowardsCursor;
                s.stageCount = 2;
                s.stage2TargetMode = eTargetMode::Direction;
                s.stage2AnimKey = "spell2_2";
                s.stage2LockSec = 0.4f;
                s.stage2Rotate = eRotateMode::TowardsCursor;
                s.stageWindowSec = 4.f;
                s.castFrame = 0.f;
                s.recoveryFrame = 7.f;
                s.stage2CastFrame = 6.f;
                s.stage2RecoveryFrame = 14.f;
                s.animPlaySpeed = 1.f;
                s.stage2PlaySpeed = 1.f;
```

아래로 교체:

```cpp
                s.lockDurationSec = 1.50f;
                s.bOneShot = true;
                s.rotate = eRotateMode::TowardsCursor;
                s.stageCount = 2;
                s.stage2TargetMode = eTargetMode::Direction;
                s.stage2AnimKey = "spell2_2";
                s.stage2LockSec = 0.45f;
                s.stage2Rotate = eRotateMode::TowardsCursor;
                s.stageWindowSec = 1.50f;
                s.castFrame = 0.f;
                s.recoveryFrame = 7.f;
                s.stage2CastFrame = 5.f;
                s.stage2RecoveryFrame = 13.f;
                s.animPlaySpeed = 1.00f;
                s.stage2PlaySpeed = 1.05f;
```

1-6. C:/Users/user/Desktop/Winters/Client/Private/GameObject/SkillTable.cpp

기존 코드:

```cpp
        //W 4초 지속!! 
        5.f, true, eRotateMode::TowardsCursor,
        2, eTargetMode::Direction, "spell2_2", 0.4f, eRotateMode::TowardsCursor, 4.f,
      0.f, 7.f, 6.f, 14.f,
      //디버깅용으로 0.1f로 테스트
      1.f, 1.f,
```

아래로 교체:

```cpp
        //W 원작 차징 시간
        1.50f, true, eRotateMode::TowardsCursor,
        2, eTargetMode::Direction, "spell2_2", 0.45f, eRotateMode::TowardsCursor, 1.50f,
      0.f, 7.f, 5.f, 13.f,
      //animPlaySpeed, stage2PlaySpeed
      1.00f, 1.05f,
```

1-7. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/ChampionRuntimeDefaults.cpp

기존 코드:

```cpp
        { eChampion::IRELIA, 2, 1, 5.00f, 1.00f },
        { eChampion::IRELIA, 2, 2, 0.40f, 1.00f },
```

아래로 교체:

```cpp
        { eChampion::IRELIA, 2, 1, 1.50f, 1.00f },
        { eChampion::IRELIA, 2, 2, 0.45f, 1.05f },
```

기존 코드:

```cpp
        { eChampion::IRELIA, 2, 2, 4.00f },
```

아래로 교체:

```cpp
        { eChampion::IRELIA, 2, 2, 1.50f },
```

1-8. C:/Users/user/Desktop/Winters/Client/Private/UI/EffectTuner.cpp

기존 코드:

```cpp
        ImGui::SliderFloat("W Hold Shield Size", &t.fWHoldShieldSize, 0.5f, 6.0f, "%.2f");
        ImGui::SliderFloat("W Hold Glow Size", &t.fWHoldGlowSize, 0.5f, 7.0f, "%.2f");
        ImGui::ColorEdit4("W Hold Shield", &t.vWHoldShieldColor.x);
        ImGui::ColorEdit4("W Hold Glow", &t.vWHoldGlowColor.x);
```

아래에 추가:

```cpp
        ImGui::SliderFloat("W Hold Inner Size", &t.fWHoldInnerSize, 0.5f, 6.0f, "%.2f");
        ImGui::SliderFloat("W Hold Dark Size", &t.fWHoldDarkSize, 0.5f, 6.0f, "%.2f");
        ImGui::SliderFloat("W Hold Gold Ring Size", &t.fWHoldGoldRingSize, 0.5f, 7.0f, "%.2f");
        ImGui::SliderFloat("W Hold Charge Size", &t.fWHoldChargeSize, 0.5f, 7.0f, "%.2f");
        ImGui::SliderFloat("W Hold Atlas FPS", &t.fWHoldAtlasFps, 1.0f, 30.0f, "%.1f");
        ImGui::ColorEdit4("W Hold Inner", &t.vWHoldInnerColor.x);
        ImGui::ColorEdit4("W Hold Dark", &t.vWHoldDarkColor.x);
        ImGui::ColorEdit4("W Hold Gold Ring", &t.vWHoldGoldRingColor.x);
        ImGui::ColorEdit4("W Hold Charge", &t.vWHoldChargeColor.x);
        ImGui::SliderFloat("W Release Forward Dist", &t.fWReleaseForwardDist, 0.5f, 5.0f, "%.2f");
        ImGui::SliderFloat("W Release Blade Width", &t.fWReleaseBladeWidth, 0.5f, 8.0f, "%.2f");
        ImGui::SliderFloat("W Release Blade Height", &t.fWReleaseBladeHeight, 0.2f, 5.0f, "%.2f");
        ImGui::SliderFloat("W Release Flash Size", &t.fWReleaseFlashSize, 0.5f, 8.0f, "%.2f");
        ImGui::SliderFloat("W Release Streak Width", &t.fWReleaseStreakWidth, 0.05f, 2.0f, "%.2f");
        ImGui::SliderFloat("W Release Streak Length", &t.fWReleaseStreakLength, 0.5f, 10.0f, "%.2f");
        ImGui::SliderFloat("W Release Shard Size", &t.fWReleaseShardSize, 0.2f, 5.0f, "%.2f");
        ImGui::ColorEdit4("W Release Core", &t.vWReleaseCoreColor.x);
        ImGui::ColorEdit4("W Release Edge", &t.vWReleaseEdgeColor.x);
        ImGui::ColorEdit4("W Release Afterglow", &t.vWReleaseAfterglowColor.x);
```

2. 검증

미검증:
- 계획서 작성만 완료, 코드 빌드 미검증.
- 런타임에서 레퍼런스 `01.png`~`04.png`의 W 차징 보호막과 해제 파동 색/타이밍이 맞는지 미검증.
- 서버 권위 경로에서 기존 `CommandExecutor`의 단일 `EffectTrigger`만으로 W visual hook이 재생되고 중복 재생이 없는지 미검증.

검증 명령:

```powershell
git diff --check
MSBuild.exe Winters.sln /m /p:Configuration=Debug /p:Platform=x64
```

수동 확인:
- C:/Users/user/Desktop/Winters/Client/Bin/Resource/Texture/UI/이펙트 이미지/01.png부터 04.png까지를 열어 W 기준 레퍼런스임을 확인한다.
- W 1타를 누르면 몸 주변에 반투명 청백 보호막, 남청 내부 회전, 금빛 링이 함께 유지되는지 확인한다.
- W 2타 해제 시 전방으로 백금색 넓은 칼날 파동이 먼저 터지고, 보라/청색 edge와 잔광이 뒤따르는지 확인한다.
- 같은 순간 로컬 클라이언트와 원격 클라이언트에서 FX가 한 번씩만 재생되는지 `OutputDebugStringA` 로그와 화면을 같이 확인한다.

확인 필요:
- 레퍼런스의 청록색 긴 조준선/화살표가 기존 스킬 타겟팅 프리뷰 시스템에 있는지 확인한다. 없으면 W 이펙트 본체와 별도 계획으로 `Direction` 스킬 프리뷰 렌더링을 추가한다.
- W 원작 최대 차징 시간 1.5초로 줄이면 현재 테스트/스모크 플로우가 너무 짧아지는지 확인한다.
