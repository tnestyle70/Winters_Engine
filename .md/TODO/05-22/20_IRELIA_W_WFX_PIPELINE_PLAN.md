Session - 이렐리아 W를 01-04 레퍼런스 이미지와 E WFX 파이프라인 기준으로 Hold/Aim/Release cue로 전환한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Client/Public/GameObject/FX/FxCuePlayer.h

기존 코드:

```cpp
#include "WintersMath.h"

class CWorld;
```

아래로 교체:

```cpp
#include "WintersMath.h"

#include <vector>

class CWorld;
```

기존 코드:

```cpp
	static EntityID Play(CWorld& world, const char* pszCueName, const FxCueContext& ctx);
```

아래에 추가:

```cpp
	static EntityID PlayAll(CWorld& world, const char* pszCueName, const FxCueContext& ctx,
		std::vector<EntityID>* pOutSpawned);
```

1-2. C:/Users/user/Desktop/Winters/Client/Private/GameObject/FX/FxCuePlayer.cpp

기존 코드:

```cpp
EntityID CFxCuePlayer::Play(CWorld& world, const char* pszCueName, const FxCueContext& ctx)
{
    const FxAssetHandle handle = FindCue(pszCueName);
    const FxAsset* pAsset = CFxSystem::GetAssetRegistry().Find(handle);
    if (!pAsset)
    {
        LogMissingCue(pszCueName);
        return NULL_ENTITY;
    }

    EntityID firstEntity = NULL_ENTITY;
    for (u32_t i = 0; i < pAsset->emitters.size(); ++i)
    {
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
            {
                entity = CFxMeshSystem::Spawn(
                    world,
                    ctx.pFxMeshRenderer,
                    BuildCueMesh(emitter, ctx, pAsset->handle, i));
            }
            else
            {
                LogSkippedCueEmitter(pszCueName, emitter);
            }
        }
        else
        {
            LogSkippedCueEmitter(pszCueName, emitter);
        }

        if (firstEntity == NULL_ENTITY)
            firstEntity = entity;
    }

    return firstEntity;
}
```

아래로 교체:

```cpp
EntityID CFxCuePlayer::Play(CWorld& world, const char* pszCueName, const FxCueContext& ctx)
{
    return PlayAll(world, pszCueName, ctx, nullptr);
}

EntityID CFxCuePlayer::PlayAll(
    CWorld& world,
    const char* pszCueName,
    const FxCueContext& ctx,
    std::vector<EntityID>* pOutSpawned)
{
    const FxAssetHandle handle = FindCue(pszCueName);
    const FxAsset* pAsset = CFxSystem::GetAssetRegistry().Find(handle);
    if (!pAsset)
    {
        LogMissingCue(pszCueName);
        return NULL_ENTITY;
    }

    EntityID firstEntity = NULL_ENTITY;
    for (u32_t i = 0; i < pAsset->emitters.size(); ++i)
    {
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
            {
                entity = CFxMeshSystem::Spawn(
                    world,
                    ctx.pFxMeshRenderer,
                    BuildCueMesh(emitter, ctx, pAsset->handle, i));
            }
            else
            {
                LogSkippedCueEmitter(pszCueName, emitter);
            }
        }
        else
        {
            LogSkippedCueEmitter(pszCueName, emitter);
        }

        if (entity != NULL_ENTITY && pOutSpawned)
            pOutSpawned->push_back(entity);
        if (firstEntity == NULL_ENTITY)
            firstEntity = entity;
    }

    return firstEntity;
}
```

1-3. C:/Users/user/Desktop/Winters/Client/Public/GameObject/Champion/Irelia/Irelia_Tuning.h

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
```

1-4. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Irelia/Irelia_Skills.cpp

기존 코드:

```cpp
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxCuePlayer.h"
```

아래에 추가:

```cpp
#include "GameObject/FX/FxBeamComponent.h"
```

기존 코드:

```cpp
#include <unordered_map>
```

아래에 추가:

```cpp
#include <vector>
```

기존 코드:

```cpp
        IreliaFx::IreliaWHoldFxIds wHoldFxIds{};
        f32_t beamDelay = 0.f;
```

아래로 교체:

```cpp
        IreliaFx::IreliaWHoldFxIds wHoldFxIds{};
        std::vector<EntityID> wHoldCueIds{};
        std::vector<EntityID> wAimCueIds{};
        bool_t bWHoldCueActive = false;
        f32_t beamDelay = 0.f;
```

기존 코드:

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

아래로 교체:

```cpp
    void MarkBeamPendingDelete(CWorld& world, EntityID fxId)
    {
        if (fxId != NULL_ENTITY && world.HasComponent<FxBeamComponent>(fxId))
            world.GetComponent<FxBeamComponent>(fxId).bPendingDelete = true;
    }

    void ClearFxIdList(CWorld& world, std::vector<EntityID>& ids)
    {
        for (EntityID fxId : ids)
        {
            MarkBillboardPendingDelete(world, fxId);
            MarkBeamPendingDelete(world, fxId);
        }
        ids.clear();
    }

    void ClearWHoldFx(CWorld& world, IreliaLocalState& state)
    {
        ClearFxIdList(world, state.wHoldCueIds);
        ClearFxIdList(world, state.wAimCueIds);
        MarkBillboardPendingDelete(world, state.wHoldFxIds.spinFxID);
        MarkBillboardPendingDelete(world, state.wHoldFxIds.shieldFxID);
        MarkBillboardPendingDelete(world, state.wHoldFxIds.glowFxID);
        MarkBillboardPendingDelete(world, state.wHoldFxIds.blockFxID);
        state.wHoldFxIds = IreliaFx::IreliaWHoldFxIds{};
        state.wSpinFxId = NULL_ENTITY;
        state.bWHoldCueActive = false;
    }
```

`ClearWHoldFx` 아래에 추가:

```cpp
    Vec3 ResolveWAimEnd(const Vec3& origin, const Vec3& cursorGround, f32_t range)
    {
        Vec3 dir = WintersMath::DirectionXZ(origin, cursorGround, Vec3{ 0.f, 0.f, 1.f });
        return {
            origin.x + dir.x * range,
            origin.y,
            origin.z + dir.z * range
        };
    }

    bool_t SpawnWHoldCueOrLegacy(CWorld& world, EntityID caster, IreliaLocalState& state,
        f32_t lifetime, const Vec3& forward)
    {
        FxCueContext hold{};
        hold.attachTo = caster;
        hold.vWorldPos = ResolveOrigin(world, caster);
        hold.vForward = forward;
        hold.bOverrideLifetime = true;
        hold.fLifetimeOverride = lifetime;

        if (CFxCuePlayer::PlayAll(world, "Irelia.W.Spin", hold, &state.wHoldCueIds) != NULL_ENTITY)
        {
            const IreliaTuning& t = GetTuning();
            FxCueContext aim{};
            const Vec3 origin = ResolveOrigin(world, caster);
            Vec3 start = origin;
            start.y += t.fWAimYOffset;
            aim.vWorldPos = start;
            aim.vEndWorldPos = {
                start.x + forward.x * t.fWAimRange,
                start.y,
                start.z + forward.z * t.fWAimRange
            };
            aim.vForward = forward;
            aim.bOverrideEndWorldPos = true;
            aim.bOverrideLifetime = true;
            aim.fLifetimeOverride = lifetime;
            CFxCuePlayer::PlayAll(world, "Irelia.W.Aim", aim, &state.wAimCueIds);
            state.bWHoldCueActive = true;
            return true;
        }

        state.wHoldFxIds = IreliaFx::SpawnWSpinLayers(world, caster, lifetime);
        state.wSpinFxId = state.wHoldFxIds.spinFxID;
        return false;
    }

    bool_t SpawnWReleaseCueOrLegacy(CWorld& world, EntityID caster, const Vec3& forward)
    {
        const IreliaTuning& t = GetTuning();
        Vec3 start = ResolveOrigin(world, caster);
        start.y += t.fWReleaseYOffset;

        FxCueContext release{};
        release.vWorldPos = start;
        release.vEndWorldPos = {
            start.x + forward.x * t.fWReleaseRange,
            start.y,
            start.z + forward.z * t.fWReleaseRange
        };
        release.vForward = forward;
        release.bOverrideEndWorldPos = true;
        release.bOverrideLifetime = true;
        release.fLifetimeOverride = t.wLayerLifetime;

        if (CFxCuePlayer::Play(world, "Irelia.W.Stage2Slash", release) != NULL_ENTITY)
            return true;

        constexpr f32_t kForwardDist = 2.0f;
        const Vec3 attachOffset{
            forward.x * kForwardDist,
            1.0f,
            forward.z * kForwardDist
        };
        IreliaFx::SpawnWStage2Slash(world, caster, forward);
        IreliaFx::SpawnWReleaseLayers(world, caster,
            t.wLayerLifetime, t.wLayerSize,
            t.wLayerBladesColor, t.wLayerGlowColor,
            attachOffset);
        return false;
    }

    void UpdateWAimCue(CWorld& world, IreliaLocalState& state,
        EntityID caster, const Vec3& cursorGround)
    {
        if (!state.bWHoldCueActive ||
            caster == NULL_ENTITY ||
            !world.HasComponent<TransformComponent>(caster))
        {
            return;
        }

        const IreliaTuning& t = GetTuning();
        Vec3 start = world.GetComponent<TransformComponent>(caster).GetPosition();
        start.y += t.fWAimYOffset;
        const Vec3 end = ResolveWAimEnd(start, cursorGround, t.fWAimRange);

        for (EntityID fxId : state.wAimCueIds)
        {
            if (!world.HasComponent<FxBeamComponent>(fxId))
                continue;

            FxBeamComponent& beam = world.GetComponent<FxBeamComponent>(fxId);
            const Vec3 dir = WintersMath::DirectionXZ(start, end, Vec3{ 0.f, 0.f, 1.f });
            const Vec3 right{ dir.z, 0.f, -dir.x };
            const Vec3 sideOffset{
                right.x * beam.vStartOffset.x,
                beam.vStartOffset.y,
                right.z * beam.vStartOffset.x
            };
            beam.vStartWorldPos = {
                start.x + sideOffset.x,
                start.y + sideOffset.y,
                start.z + sideOffset.z
            };
            beam.vEndWorldPos = {
                end.x + sideOffset.x,
                end.y + sideOffset.y,
                end.z + sideOffset.z
            };
            beam.vVelocity = {};
        }
    }
```

기존 코드:

```cpp
    void OnCastAccepted_W(SkillHookContext& ctx)
    {
        if (!ctx.pWorld)
            return;

        IreliaLocalState& state = GetState(ctx.casterEntity);
        if (ctx.skillStage >= 2)
        {
            ClearWHoldFx(*ctx.pWorld, state);

            const IreliaTuning& t = GetTuning();
            const Vec3 forward = ResolveForward(*ctx.pWorld, ctx.casterEntity, ctx.pCommand);
            IreliaFx::SpawnWStage2Slash(*ctx.pWorld, ctx.casterEntity, forward);

            constexpr f32_t kForwardDist = 2.0f;
            const Vec3 attachOffset{
                forward.x * kForwardDist,
                1.0f,
                forward.z * kForwardDist
            };
            IreliaFx::SpawnWReleaseLayers(*ctx.pWorld, ctx.casterEntity,
                t.wLayerLifetime, t.wLayerSize,
                t.wLayerBladesColor, t.wLayerGlowColor,
                attachOffset);
            return;
        }

        const f32_t lifetime =
            (ctx.pDef && ctx.pDef->stageWindowSec > 0.f) ? ctx.pDef->stageWindowSec + 0.5f : 4.5f;
        state.wHoldFxIds = IreliaFx::SpawnWSpinLayers(*ctx.pWorld, ctx.casterEntity, lifetime);
        state.wSpinFxId = state.wHoldFxIds.spinFxID;
    }
```

아래로 교체:

```cpp
    void OnCastAccepted_W(SkillHookContext& ctx)
    {
        if (!ctx.pWorld)
            return;

        IreliaLocalState& state = GetState(ctx.casterEntity);
        const Vec3 forward = ResolveForward(*ctx.pWorld, ctx.casterEntity, ctx.pCommand);
        if (ctx.skillStage >= 2)
        {
            ClearWHoldFx(*ctx.pWorld, state);
            SpawnWReleaseCueOrLegacy(*ctx.pWorld, ctx.casterEntity, forward);
            return;
        }

        ClearWHoldFx(*ctx.pWorld, state);
        const f32_t lifetime =
            (ctx.pDef && ctx.pDef->stageWindowSec > 0.f) ? ctx.pDef->stageWindowSec + 0.5f : 4.5f;
        SpawnWHoldCueOrLegacy(*ctx.pWorld, ctx.casterEntity, state, lifetime, forward);
    }
```

`UpdateLocalBladeState` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
        if (state.sword2Id != NULL_ENTITY && !world.IsAlive(state.sword2Id))
            state.sword2Id = NULL_ENTITY;
```

아래에 추가:

```cpp
        UpdateWAimCue(world, state, casterEntity, vCursorGround);
```

1-5. C:/Users/user/Desktop/Winters/Client/Private/GameObject/FX/FxLegacyManifest.cpp

기존 코드:

```cpp
            {
                "Irelia.W.Spin", "Irelia", "IreliaFx::SpawnWSpin",
                "Client/Private/GameObject/Champion/Irelia/IreliaFxPresets.cpp",
                "Billboard",
                L"Client/Bin/Resource/FX/LoL/Irelia/Irelia_W_Spin.wfx",
                L"Client/Bin/Resource/FX/LoL/Irelia/MI_Irelia_W_Spin.wmi",
                "LegacyOnly"
            },
            {
                "Irelia.W.Stage2Slash", "Irelia", "IreliaFx::SpawnWStage2Slash",
                "Client/Private/GameObject/Champion/Irelia/IreliaFxPresets.cpp",
                "Billboard",
                L"Client/Bin/Resource/FX/LoL/Irelia/Irelia_W_Stage2Slash.wfx",
                L"Client/Bin/Resource/FX/LoL/Irelia/MI_Irelia_W_Stage2Slash.wmi",
                "LegacyOnly"
            },
```

아래로 교체:

```cpp
            {
                "Irelia.W.Spin", "Irelia", "CFxCuePlayer::PlayAll",
                "Client/Private/GameObject/Champion/Irelia/Irelia_Skills.cpp",
                "Billboard,ShockwaveRing",
                L"Data/LoL/FX/Champions/Irelia/w_hold.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Irelia.W.Aim", "Irelia", "CFxCuePlayer::PlayAll",
                "Client/Private/GameObject/Champion/Irelia/Irelia_Skills.cpp",
                "Beam",
                L"Data/LoL/FX/Champions/Irelia/w_aim.wfx",
                L"",
                "WfxPilot"
            },
            {
                "Irelia.W.Stage2Slash", "Irelia", "CFxCuePlayer::Play",
                "Client/Private/GameObject/Champion/Irelia/Irelia_Skills.cpp",
                "Beam,Billboard",
                L"Data/LoL/FX/Champions/Irelia/w_release.wfx",
                L"",
                "WfxPilot"
            },
```

1-6. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Irelia/w_hold.wfx

새 파일:

```json
{
  "name": "Irelia.W.Spin",
  "emitters": [
    {
      "name": "w_hold_outer_shield",
      "render_type": "Billboard",
      "blend_mode": "AlphaBlend",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_w_shield_soft_tex.png",
      "lifetime": 4.5,
      "width": 3.45,
      "height": 3.45,
      "color": [0.54, 0.90, 1.65, 0.42],
      "attach_offset": [0.0, 1.00, 0.0],
      "fade_in": 0.06,
      "fade_out": 0.30,
      "billboard": true
    },
    {
      "name": "w_hold_dark_spin",
      "render_type": "Billboard",
      "blend_mode": "AlphaBlend",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_w_bladeimages_spin_02.png",
      "lifetime": 4.5,
      "width": 2.85,
      "height": 2.85,
      "color": [0.06, 0.06, 0.26, 0.42],
      "attach_offset": [0.0, 1.02, 0.0],
      "fade_in": 0.05,
      "fade_out": 0.25,
      "atlas_cols": 2,
      "atlas_rows": 2,
      "atlas_frame_count": 4,
      "atlas_fps": 10.0,
      "atlas_loop": true,
      "billboard": true
    },
    {
      "name": "w_hold_inner_spin",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_w_bladeimages_spin_02.png",
      "lifetime": 4.5,
      "start_delay": 0.03,
      "width": 2.55,
      "height": 2.55,
      "color": [0.62, 0.78, 1.70, 0.74],
      "attach_offset": [0.0, 1.04, 0.0],
      "fade_in": 0.05,
      "fade_out": 0.28,
      "atlas_cols": 2,
      "atlas_rows": 2,
      "atlas_frame_count": 4,
      "atlas_fps": 16.0,
      "atlas_loop": true,
      "billboard": true
    },
    {
      "name": "w_hold_gold_rim",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_temp_w_solar_circle.png",
      "lifetime": 4.5,
      "start_delay": 0.08,
      "width": 3.35,
      "height": 3.35,
      "color": [1.20, 1.05, 0.48, 0.48],
      "attach_offset": [0.0, 1.06, 0.0],
      "fade_in": 0.08,
      "fade_out": 0.34,
      "billboard": true
    },
    {
      "name": "w_hold_ambient_glow",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_temp_w_ambientglow.png",
      "lifetime": 4.5,
      "start_delay": 0.12,
      "width": 4.15,
      "height": 4.15,
      "color": [0.32, 0.62, 1.45, 0.30],
      "attach_offset": [0.0, 1.02, 0.0],
      "fade_in": 0.12,
      "fade_out": 0.36,
      "billboard": true
    }
  ]
}
```

1-7. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Irelia/w_aim.wfx

새 파일:

```json
{
  "name": "Irelia.W.Aim",
  "emitters": [
    {
      "name": "w_aim_core_line",
      "render_type": "Beam",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_temp_w_recall_beam_mult.png",
      "lifetime": 4.5,
      "width": 0.10,
      "color": [0.34, 1.75, 1.55, 0.70],
      "attach_offset": [0.0, 0.0, 0.0],
      "fade_in": 0.05,
      "fade_out": 0.20,
      "uv_scroll": [0.0, -0.45]
    },
    {
      "name": "w_aim_side_a",
      "render_type": "Beam",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_temp_w_recall_beam_mult.png",
      "lifetime": 4.5,
      "width": 0.045,
      "color": [0.34, 1.35, 1.55, 0.36],
      "attach_offset": [0.34, 0.0, 0.0],
      "fade_in": 0.06,
      "fade_out": 0.18,
      "uv_scroll": [0.0, -0.30]
    },
    {
      "name": "w_aim_side_b",
      "render_type": "Beam",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_temp_w_recall_beam_mult.png",
      "lifetime": 4.5,
      "width": 0.045,
      "color": [0.34, 1.35, 1.55, 0.36],
      "attach_offset": [-0.34, 0.0, 0.0],
      "fade_in": 0.06,
      "fade_out": 0.18,
      "uv_scroll": [0.0, -0.30]
    }
  ]
}
```

1-8. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Irelia/w_release.wfx

새 파일:

```json
{
  "name": "Irelia.W.Stage2Slash",
  "emitters": [
    {
      "name": "w_release_blue_rail",
      "render_type": "Beam",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_w_mis_glow.png",
      "lifetime": 0.48,
      "width": 1.20,
      "color": [0.45, 0.72, 2.10, 0.42],
      "attach_offset": [0.0, 0.0, 0.0],
      "fade_in": 0.02,
      "fade_out": 0.34,
      "uv_scroll": [0.0, -0.48]
    },
    {
      "name": "w_release_gold_core",
      "render_type": "Beam",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_w_projectile.png",
      "lifetime": 0.30,
      "start_delay": 0.03,
      "width": 0.55,
      "color": [1.45, 1.30, 0.82, 0.88],
      "attach_offset": [0.0, 0.03, 0.0],
      "fade_in": 0.01,
      "fade_out": 0.22,
      "uv_scroll": [0.0, -0.72]
    },
    {
      "name": "w_release_blade_swipe",
      "render_type": "Billboard",
      "blend_mode": "AlphaBlend",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_w_swipe_blades.png",
      "lifetime": 0.36,
      "start_delay": 0.02,
      "segment_t": 0.58,
      "width": 5.80,
      "height": 2.45,
      "color": [1.25, 1.18, 0.90, 0.92],
      "attach_offset": [0.0, 0.12, 0.0],
      "fade_in": 0.01,
      "fade_out": 0.28,
      "billboard": true
    },
    {
      "name": "w_release_white_flash",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_w_hit_star.png",
      "lifetime": 0.22,
      "start_delay": 0.06,
      "segment_t": 0.64,
      "width": 3.40,
      "height": 2.10,
      "color": [1.15, 1.22, 1.95, 0.95],
      "attach_offset": [0.0, 0.18, 0.0],
      "fade_in": 0.01,
      "fade_out": 0.16,
      "billboard": true
    },
    {
      "name": "w_release_shards",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_shards.png",
      "lifetime": 0.26,
      "start_delay": 0.10,
      "segment_t": 0.50,
      "width": 2.30,
      "height": 2.30,
      "color": [0.78, 0.96, 2.10, 0.58],
      "attach_offset": [0.0, 0.16, 0.0],
      "fade_in": 0.01,
      "fade_out": 0.20,
      "billboard": true
    }
  ]
}
```

2. 검증

미검증:
- 이 계획서는 아직 코드에 반영하지 않았다.
- 01.png/02.png 기준 Hold 방어막, 회전막, 목표선의 크기와 알파는 런타임 캡처로 재튜닝해야 한다.
- 03.png/04.png 기준 Release 슬래시가 캐릭터 앞 방향 6m 범위를 정확히 덮는지 런타임 확인이 필요하다.

검증 명령:
- `git diff --check -- Client/Public/GameObject/FX/FxCuePlayer.h Client/Private/GameObject/FX/FxCuePlayer.cpp Client/Public/GameObject/Champion/Irelia/Irelia_Tuning.h Client/Private/GameObject/Champion/Irelia/Irelia_Skills.cpp Client/Private/GameObject/FX/FxLegacyManifest.cpp Data/LoL/FX/Champions/Irelia/w_hold.wfx Data/LoL/FX/Champions/Irelia/w_aim.wfx Data/LoL/FX/Champions/Irelia/w_release.wfx`
- `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' Winters.sln /m /p:Configuration=Debug /p:Platform=x64`

확인 필요:
- WFX 파일은 `Data/LoL/FX`에서 런타임 로드되므로 `.vcxproj` XML 등록은 하지 않는다.
- `WintersServer.exe`가 실행 중이면 Server link가 잠길 수 있으므로 빌드 전 프로세스 확인이 필요하다.
