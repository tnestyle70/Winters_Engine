Session - Annie original-like WFX polish and Tibber runtime visual apply.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Annie/q_fireball.wfx

아래로 교체:

```json
{
  "name": "Annie.Q.Fireball",
  "emitters": [
    {
      "name": "q_muzzle_flash",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Annie/particles/annie_base_q_mis_flames.png",
      "lifetime": 0.24,
      "width": 1.50,
      "height": 1.50,
      "color": [1.95, 0.70, 0.18, 0.95],
      "attach_offset": [0.0, 1.18, 0.58],
      "fade_in": 0.01,
      "fade_out": 0.12,
      "billboard": true
    },
    {
      "name": "q_missile_mesh",
      "render_type": "MeshParticle",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "model": "Client/Bin/Resource/Texture/Character/Annie/particles/fbx/annie_base_q_mis_01.fbx",
      "texture": "Client/Bin/Resource/Texture/Character/Annie/particles/annie_base_q_mis_01.png",
      "lifetime": 0.42,
      "segment_t": 0.52,
      "scale": [0.020, 0.020, 0.020],
      "rotation": [0.0, 0.0, 0.0],
      "color": [1.55, 0.48, 0.12, 0.96],
      "attach_offset": [0.0, 1.08, 0.0],
      "fade_in": 0.01,
      "fade_out": 0.18,
      "blockable_by_wind_wall": true
    },
    {
      "name": "q_core_glow",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Annie/particles/annie_base_q_radgrad.png",
      "lifetime": 0.40,
      "width": 1.65,
      "height": 1.65,
      "color": [1.75, 0.45, 0.12, 0.78],
      "segment_t": 0.56,
      "attach_offset": [0.0, 1.08, 0.0],
      "fade_in": 0.01,
      "fade_out": 0.18,
      "billboard": true
    },
    {
      "name": "q_trail_flames",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Annie/particles/annie_base_q_mis_trail.png",
      "lifetime": 0.42,
      "start_delay": 0.02,
      "width": 2.30,
      "height": 1.35,
      "color": [1.70, 0.42, 0.10, 0.88],
      "segment_t": 0.42,
      "attach_offset": [0.0, 1.00, -0.10],
      "fade_in": 0.01,
      "fade_out": 0.24,
      "billboard": true
    },
    {
      "name": "q_smoke_tail",
      "render_type": "Billboard",
      "blend_mode": "AlphaBlend",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Annie/particles/annie_base_q_mis_smoke_trail.png",
      "lifetime": 0.44,
      "start_delay": 0.03,
      "width": 2.50,
      "height": 1.10,
      "color": [0.90, 0.22, 0.06, 0.46],
      "segment_t": 0.32,
      "attach_offset": [0.0, 0.92, -0.16],
      "fade_in": 0.02,
      "fade_out": 0.28,
      "billboard": true
    },
    {
      "name": "q_sparkles",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Annie/particles/annie_base_q_mis_sparkles.png",
      "lifetime": 0.34,
      "start_delay": 0.04,
      "width": 1.10,
      "height": 1.10,
      "color": [2.05, 0.86, 0.26, 0.88],
      "segment_t": 0.66,
      "attach_offset": [0.0, 1.10, 0.0],
      "fade_in": 0.01,
      "fade_out": 0.18,
      "billboard": true
    },
    {
      "name": "q_hit_ash",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Annie/particles/annie_base_q_ash.png",
      "lifetime": 0.34,
      "start_delay": 0.08,
      "width": 2.35,
      "height": 2.35,
      "color": [1.65, 0.58, 0.16, 0.94],
      "segment_t": 1.0,
      "attach_offset": [0.0, 1.0, 0.0],
      "fade_in": 0.01,
      "fade_out": 0.22,
      "billboard": true
    }
  ]
}
```

1-2. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Annie/w_cone.wfx

아래로 교체:

```json
{
  "name": "Annie.W.Cone",
  "emitters": [
    {
      "name": "w_ground_cone",
      "render_type": "GroundDecal",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Annie/particles/annie_base_w_grounddecalfinal.png",
      "lifetime": 0.62,
      "width": 6.40,
      "height": 6.40,
      "color": [1.78, 0.55, 0.12, 0.96],
      "attach_offset": [0.0, 0.035, 2.85],
      "fade_in": 0.02,
      "fade_out": 0.30,
      "billboard": false
    },
    {
      "name": "w_heat_mask",
      "render_type": "GroundDecal",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Annie/particles/annie_base_w_mask.png",
      "lifetime": 0.44,
      "start_delay": 0.04,
      "width": 5.80,
      "height": 5.80,
      "color": [1.85, 0.68, 0.18, 0.70],
      "attach_offset": [0.0, 0.04, 2.55],
      "fade_in": 0.01,
      "fade_out": 0.25,
      "billboard": false
    },
    {
      "name": "w_cone_mesh_body",
      "render_type": "MeshParticle",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "model": "Client/Bin/Resource/Texture/Character/Annie/particles/fbx/annie_base_w_cone_2.fbx",
      "texture": "Client/Bin/Resource/Texture/Character/Annie/particles/render/annie_base_w_cone_2.png",
      "lifetime": 0.34,
      "start_delay": 0.02,
      "scale": [0.018, 0.018, 0.018],
      "rotation": [0.0, -1.57079632679, 0.0],
      "color": [1.65, 0.42, 0.08, 0.78],
      "attach_offset": [0.0, 0.72, 0.36],
      "fade_in": 0.01,
      "fade_out": 0.20
    },
    {
      "name": "w_cone_mesh_edge",
      "render_type": "MeshParticle",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "model": "Client/Bin/Resource/Texture/Character/Annie/particles/fbx/annie_base_w_cone_1_edge.fbx",
      "texture": "Client/Bin/Resource/Texture/Character/Annie/particles/render/annie_base_w_cone_1_edge.png",
      "lifetime": 0.38,
      "start_delay": 0.04,
      "scale": [0.085, 0.085, 0.085],
      "rotation": [0.0, -1.57079632679, 0.0],
      "color": [1.95, 0.72, 0.18, 0.80],
      "attach_offset": [0.0, 0.82, 1.10],
      "fade_in": 0.01,
      "fade_out": 0.22
    },
    {
      "name": "w_center_flare",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Annie/particles/annie_base_glow2.png",
      "lifetime": 0.34,
      "start_delay": 0.02,
      "width": 3.30,
      "height": 2.20,
      "color": [1.86, 0.66, 0.14, 0.92],
      "attach_offset": [0.0, 0.85, 2.20],
      "fade_in": 0.01,
      "fade_out": 0.20,
      "billboard": true
    },
    {
      "name": "w_front_flame",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Annie/particles/annie_fire_01.png",
      "lifetime": 0.40,
      "start_delay": 0.04,
      "width": 3.15,
      "height": 4.05,
      "color": [1.50, 0.42, 0.08, 0.78],
      "attach_offset": [0.0, 1.15, 2.95],
      "fade_in": 0.01,
      "fade_out": 0.24,
      "billboard": true
    },
    {
      "name": "w_smoke_afterburn",
      "render_type": "Billboard",
      "blend_mode": "AlphaBlend",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Annie/particles/annie_base_smokeerode.png",
      "lifetime": 0.58,
      "start_delay": 0.08,
      "width": 4.20,
      "height": 2.40,
      "color": [0.48, 0.18, 0.08, 0.38],
      "attach_offset": [0.0, 0.62, 2.50],
      "fade_in": 0.04,
      "fade_out": 0.32,
      "billboard": true
    }
  ]
}
```

1-3. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Annie/e_shield.wfx

아래로 교체:

```json
{
  "name": "Annie.E.Shield",
  "emitters": [
    {
      "name": "e_ground_ring",
      "render_type": "GroundDecal",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Annie/particles/annie_base_e_circle.png",
      "lifetime": 3.0,
      "width": 2.40,
      "height": 2.40,
      "color": [1.30, 0.52, 0.18, 0.56],
      "attach_offset": [0.0, 0.04, 0.0],
      "fade_in": 0.06,
      "fade_out": 0.70,
      "billboard": false
    },
    {
      "name": "e_outer_ring",
      "render_type": "GroundDecal",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Annie/particles/annie_base_e_circle_2.png",
      "lifetime": 3.0,
      "width": 2.95,
      "height": 2.95,
      "color": [1.10, 0.34, 0.10, 0.42],
      "attach_offset": [0.0, 0.045, 0.0],
      "fade_in": 0.08,
      "fade_out": 0.82,
      "billboard": false
    },
    {
      "name": "e_buf_glow",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Annie/particles/annie_base_e_buf_glow2.png",
      "lifetime": 3.0,
      "width": 1.80,
      "height": 1.80,
      "color": [1.35, 0.70, 0.28, 0.72],
      "attach_offset": [0.0, 1.10, 0.0],
      "fade_in": 0.06,
      "fade_out": 0.80,
      "billboard": true
    },
    {
      "name": "e_deflection_bubble",
      "render_type": "Billboard",
      "blend_mode": "AlphaBlend",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Annie/particles/annie_base_e_deflectionbubble.png",
      "lifetime": 3.0,
      "width": 2.30,
      "height": 2.30,
      "color": [1.0, 0.48, 0.16, 0.48],
      "attach_offset": [0.0, 1.00, 0.0],
      "fade_in": 0.08,
      "fade_out": 0.85,
      "billboard": true
    },
    {
      "name": "e_deflection_ripples",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Annie/particles/annie_base_e_deflectionripples.png",
      "lifetime": 0.80,
      "start_delay": 0.05,
      "width": 2.10,
      "height": 2.10,
      "color": [1.60, 0.62, 0.18, 0.56],
      "attach_offset": [0.0, 1.15, 0.0],
      "fade_in": 0.03,
      "fade_out": 0.36,
      "billboard": true
    },
    {
      "name": "e_small_motes",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Annie/particles/annie_base_e_small_mote.png",
      "lifetime": 1.20,
      "start_delay": 0.10,
      "width": 0.80,
      "height": 0.80,
      "color": [1.80, 0.72, 0.20, 0.72],
      "attach_offset": [0.0, 1.58, 0.0],
      "fade_in": 0.04,
      "fade_out": 0.50,
      "billboard": true
    }
  ]
}
```

1-4. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Annie/r_summon.wfx

아래로 교체:

```json
{
  "name": "Annie.R.Summon",
  "emitters": [
    {
      "name": "r_summon_ring",
      "render_type": "GroundDecal",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Annie/particles/annie_bear_summon_ring.png",
      "lifetime": 1.45,
      "width": 7.20,
      "height": 7.20,
      "color": [1.70, 0.62, 0.14, 1.0],
      "attach_offset": [0.0, 0.04, 0.0],
      "fade_in": 0.03,
      "fade_out": 0.70,
      "billboard": false
    },
    {
      "name": "r_shockwave",
      "render_type": "GroundDecal",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Annie/particles/annie_bear_shockwave.png",
      "lifetime": 0.70,
      "start_delay": 0.05,
      "width": 8.00,
      "height": 8.00,
      "color": [1.85, 0.78, 0.20, 0.92],
      "attach_offset": [0.0, 0.045, 0.0],
      "fade_in": 0.02,
      "fade_out": 0.46,
      "billboard": false
    },
    {
      "name": "r_flame_column",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Annie/particles/annie_base_brazier_flame_temp_01.png",
      "lifetime": 1.00,
      "start_delay": 0.08,
      "width": 3.40,
      "height": 6.40,
      "color": [1.85, 0.66, 0.14, 1.0],
      "attach_offset": [0.0, 1.60, 0.0],
      "fade_in": 0.03,
      "fade_out": 0.55,
      "billboard": true
    },
    {
      "name": "r_fire_ring",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Annie/particles/annie_panda_fire_ring_add.png",
      "lifetime": 0.72,
      "start_delay": 0.06,
      "width": 5.20,
      "height": 5.20,
      "color": [1.90, 0.58, 0.12, 0.76],
      "attach_offset": [0.0, 0.78, 0.0],
      "fade_in": 0.02,
      "fade_out": 0.42,
      "billboard": true
    },
    {
      "name": "r_smoke",
      "render_type": "Billboard",
      "blend_mode": "AlphaBlend",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Annie/particles/annie_panda_smoke_rgb.png",
      "lifetime": 1.05,
      "start_delay": 0.10,
      "width": 5.40,
      "height": 4.10,
      "color": [0.40, 0.16, 0.08, 0.42],
      "attach_offset": [0.0, 1.10, 0.0],
      "fade_in": 0.08,
      "fade_out": 0.62,
      "billboard": true
    },
    {
      "name": "r_ground_glow",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Annie/particles/annie_base_glow.png",
      "lifetime": 0.80,
      "width": 5.80,
      "height": 5.80,
      "color": [1.75, 0.52, 0.10, 0.82],
      "attach_offset": [0.0, 0.35, 0.0],
      "fade_in": 0.02,
      "fade_out": 0.50,
      "billboard": true
    },
    {
      "name": "r_panda_fire_head_hint",
      "render_type": "MeshParticle",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "model": "Client/Bin/Resource/Texture/Character/Annie/particles/fbx/annie_panda_fire_head.fbx",
      "texture": "Client/Bin/Resource/Texture/Character/Annie/particles/annie_panda_fire_head.png",
      "lifetime": 1.20,
      "start_delay": 0.10,
      "scale": [0.075, 0.075, 0.075],
      "rotation": [0.0, 0.0, 0.0],
      "color": [1.45, 0.52, 0.12, 0.96],
      "attach_offset": [0.0, 1.75, 0.0],
      "fade_in": 0.05,
      "fade_out": 0.55
    }
  ]
}
```

1-5. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Annie/ba_projectile.wfx

아래로 교체:

```json
{
  "name": "Annie.BA.Projectile",
  "emitters": [
    {
      "name": "ba_muzzle_flash",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Annie/particles/annie_panda_fire_basic.png",
      "lifetime": 0.18,
      "width": 0.95,
      "height": 0.95,
      "color": [1.85, 0.62, 0.14, 0.95],
      "attach_offset": [0.0, 1.10, 0.46],
      "fade_in": 0.01,
      "fade_out": 0.10,
      "billboard": true
    },
    {
      "name": "ba_projectile_core",
      "render_type": "MeshParticle",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "model": "Client/Bin/Resource/Texture/Character/Annie/particles/fbx/annie_base_q_mis_01.fbx",
      "texture": "Client/Bin/Resource/Texture/Character/Annie/particles/annie_panda_fire_basic.png",
      "lifetime": 0.24,
      "segment_t": 0.56,
      "scale": [0.010, 0.010, 0.010],
      "rotation": [0.0, 0.0, 0.0],
      "color": [1.55, 0.48, 0.10, 0.92],
      "attach_offset": [0.0, 1.02, 0.0],
      "fade_in": 0.01,
      "fade_out": 0.12,
      "blockable_by_wind_wall": true
    },
    {
      "name": "ba_projectile_trail",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Annie/particles/flame_trail_gradient.png",
      "lifetime": 0.24,
      "start_delay": 0.01,
      "width": 1.25,
      "height": 0.55,
      "color": [1.60, 0.42, 0.08, 0.72],
      "segment_t": 0.42,
      "attach_offset": [0.0, 0.98, 0.0],
      "fade_in": 0.01,
      "fade_out": 0.16,
      "billboard": true,
      "blockable_by_wind_wall": true
    },
    {
      "name": "ba_projectile_smoke",
      "render_type": "Billboard",
      "blend_mode": "AlphaBlend",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Annie/particles/annie_base_smoke_01.png",
      "lifetime": 0.26,
      "start_delay": 0.03,
      "width": 1.05,
      "height": 0.68,
      "color": [0.62, 0.20, 0.08, 0.34],
      "segment_t": 0.34,
      "attach_offset": [0.0, 0.94, 0.0],
      "fade_in": 0.02,
      "fade_out": 0.16,
      "billboard": true
    },
    {
      "name": "ba_projectile_spark",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Annie/particles/annie_base_spark.png",
      "lifetime": 0.20,
      "start_delay": 0.02,
      "width": 0.70,
      "height": 0.70,
      "color": [1.95, 0.78, 0.20, 0.90],
      "segment_t": 0.66,
      "attach_offset": [0.0, 1.04, 0.0],
      "fade_in": 0.01,
      "fade_out": 0.12,
      "billboard": true
    }
  ]
}
```

1-6. C:/Users/user/Desktop/Winters/Client/Private/Manager/Minion_Manager.cpp

기존 코드:

```cpp
    static constexpr bool_t kMinionDebugOutput = false;
    static constexpr f32_t kMinionAvoidancePadding = 0.05f;
```

아래에 추가:

```cpp
    static constexpr const char* kTibbersModelPath =
        "Client/Bin/Resource/Texture/Character/Annie/tibber.wmesh";
    static constexpr const wchar_t* kTibbersTexturePath =
        L"Client/Bin/Resource/Texture/Character/Annie/tibber_base.png";
    static constexpr f32_t kTibbersVisualScale = 0.01f;
```

기존 코드:

```cpp
    if (!pRenderer->Init(pPath, L"Shaders/Mesh3D.hlsl"))
    {
        char msg[512]{};
        sprintf_s(msg,
            "[MinionVisual] bind FAIL: ModelRenderer::Init failed entity=%u type=%u team=%u path=%s\n",
            static_cast<u32_t>(entity),
            static_cast<u32_t>(eType),
            static_cast<u32_t>(eTeamParam),
            pPath);
        OutputMinionDebug(msg);
        return false;
    }
```

아래에 추가:

```cpp
    if (eType == eMinionType::Tibbers)
        pRenderer->LoadTextureForAllMeshes(kTibbersTexturePath);
```

기존 코드:

```cpp
    const f32_t fVisualScale = (eType == eMinionType::Tibbers) ? 0.08f : m_fVisualScale;
```

아래로 교체:

```cpp
    const f32_t fVisualScale = (eType == eMinionType::Tibbers) ? kTibbersVisualScale : m_fVisualScale;
```

기존 코드:

```cpp
    if (!pRenderer->Init(pPath, L"Shaders/Mesh3D.hlsl"))
    {
        char m[512];
        sprintf_s(m, "[Minion] Spawn FAIL: ModelRenderer::Init failed for %s\n  path=%s\n",
            typeName, pPath);
        OutputMinionDebug(m);
        return NULL_ENTITY;
    }
```

아래에 추가:

```cpp
    if (eType == eMinionType::Tibbers)
        pRenderer->LoadTextureForAllMeshes(kTibbersTexturePath);
```

기존 코드:

```cpp
    xform.SetScale(m_fVisualScale);
```

아래로 교체:

```cpp
    const f32_t fVisualScale = (eType == eMinionType::Tibbers) ? kTibbersVisualScale : m_fVisualScale;
    xform.SetScale(fVisualScale);
```

기존 코드:

```cpp
          "Client/Bin/Resource/Texture/Character/Annie/particles/fbx/annie_panda_fire_head.wmesh" },
```

아래로 교체:

```cpp
          kTibbersModelPath },
```

위 교체는 Blue/Red Tibbers 슬롯 두 군데 모두 적용한다.

1-7. C:/Users/user/Desktop/Winters/Client/Bin/Resource/Texture/Character/Annie/tibber.wmat

생성 파일:

```text
CONFIRM_NEEDED: binary asset. Generate with WintersAssetConverter material command and verify with `WintersAssetConverter.exe info`.
```

2. 검증

검증 명령:

```text
Tools/Bin/Debug/WintersAssetConverter.exe info Client/Bin/Resource/Texture/Character/Annie/tibber.wmesh
Tools/Bin/Debug/WintersAssetConverter.exe info Client/Bin/Resource/Texture/Character/Annie/tibber.wskel
Tools/Bin/Debug/WintersAssetConverter.exe material Client/Bin/Resource/Texture/Character/Annie/tibber.fbx -o Client/Bin/Resource/Texture/Character/Annie/tibber.wmat
Tools/Bin/Debug/WintersAssetConverter.exe info Client/Bin/Resource/Texture/Character/Annie/tibber.wmat
Get-Content Data/LoL/FX/Champions/Annie/*.wfx | ConvertFrom-Json
git diff --check -- Data/LoL/FX/Champions/Annie Client/Private/Manager/Minion_Manager.cpp .md/plan/Champion/22_ANNIE_ORIGINAL_EFFECT_TIBBER_APPLY_PLAN.md
MSBuild Winters.sln /m /p:Configuration=Debug /p:Platform=x64 /v:minimal
```

확인 필요:

```text
- Tibber는 wmesh/wskel은 존재하지만 전용 .wanim은 현재 제공되지 않았다. ModelRenderer는 정적 bind pose 또는 로드 가능한 애니메이션만 재생한다.
- 실제 스킬 크기, 타이밍, 색감, 카메라 각도별 겹침은 인게임에서 사용자가 튜닝한다.
```
