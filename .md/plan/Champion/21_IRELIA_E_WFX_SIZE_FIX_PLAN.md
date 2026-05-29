Session - 이렐리아 E 이펙트가 과대하게 보이는 문제를 WFX 단일 place 레이어와 축소된 emitter 크기로 수정한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Client/Public/GameObject/Champion/Irelia/IreliaBladeSystem.h

기존 코드:

```cpp
	EntityID groundGlowFxID = NULL_ENTITY;
	EntityID groundCoreFxID = NULL_ENTITY;
```

아래에 추가:

```cpp
	EntityID placeCueFxID = NULL_ENTITY;
```

1-2. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Irelia/IreliaBladeSystem.cpp

기존 코드:

```cpp
                    if (blade.groundCoreFxID != NULL_ENTITY &&
                        world.HasComponent<FxBillboardComponent>(blade.groundCoreFxID))
                        world.GetComponent<FxBillboardComponent>(blade.groundCoreFxID).bPendingDelete = true;
                    vecDelete.push_back(entity);
                    return;
```

아래로 교체:

```cpp
                    if (blade.groundCoreFxID != NULL_ENTITY &&
                        world.HasComponent<FxBillboardComponent>(blade.groundCoreFxID))
                        world.GetComponent<FxBillboardComponent>(blade.groundCoreFxID).bPendingDelete = true;
                    if (blade.placeCueFxID != NULL_ENTITY &&
                        world.HasComponent<FxBillboardComponent>(blade.placeCueFxID))
                        world.GetComponent<FxBillboardComponent>(blade.placeCueFxID).bPendingDelete = true;
                    vecDelete.push_back(entity);
                    return;
```

기존 코드:

```cpp
    IreliaFx::IreliaEPlacedFxIds eFx = IreliaFx::SpawnEPlacedLayers(world, vRaisedGround,
        placedBlade.fLifetime);

    placedBlade.groundGlowFxID = eFx.groundGlowFxID;
    placedBlade.groundCoreFxID = eFx.groundCoreFxID;

    FxCueContext fx{};
    fx.vWorldPos = vRaisedGround;
    fx.vForward = { std::sinf(vRotation.y), 0.f, std::cosf(vRotation.y) };
    fx.pFxMeshRenderer = pRenderer;
    CFxCuePlayer::Play(world, "Irelia.E.Place", fx);
```

아래로 교체:

```cpp
    FxCueContext fx{};
    fx.vWorldPos = vRaisedGround;
    fx.vForward = { std::sinf(vRotation.y), 0.f, std::cosf(vRotation.y) };
    fx.pFxMeshRenderer = pRenderer;
    placedBlade.placeCueFxID = CFxCuePlayer::Play(world, "Irelia.E.Place", fx);
```

1-3. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Irelia/e_place.wfx

기존 파일 전체를 아래로 교체:

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
      "lifetime": 3.0,
      "width": 2.20,
      "height": 2.20,
      "color": [0.42, 0.68, 1.55, 0.24],
      "attach_offset": [0.0, -2.95, 0.0],
      "fade_in": 0.05,
      "fade_out": 0.28,
      "billboard": false
    },
    {
      "name": "e_place_blade_glow",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_p_blade_glow.png",
      "lifetime": 0.34,
      "width": 1.18,
      "height": 1.18,
      "color": [0.78, 0.92, 1.95, 0.62],
      "attach_offset": [0.0, 0.12, 0.0],
      "fade_in": 0.02,
      "fade_out": 0.21,
      "billboard": true
    },
    {
      "name": "e_place_violet_edge",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_p_gradient.png",
      "lifetime": 0.42,
      "start_delay": 0.04,
      "width": 1.48,
      "height": 1.48,
      "color": [0.66, 0.36, 1.60, 0.32],
      "attach_offset": [0.0, 0.26, 0.0],
      "fade_in": 0.02,
      "fade_out": 0.25,
      "billboard": true
    },
    {
      "name": "e_place_spark",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_e_warnig_spark.png",
      "lifetime": 0.28,
      "start_delay": 0.02,
      "width": 0.92,
      "height": 0.92,
      "color": [0.88, 1.02, 2.05, 0.86],
      "attach_offset": [0.0, 0.44, 0.0],
      "fade_in": 0.01,
      "fade_out": 0.18,
      "billboard": true
    }
  ]
}
```

1-4. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Irelia/e_connect.wfx

기존 파일 전체를 아래로 교체:

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
      "lifetime": 0.66,
      "width": 0.76,
      "height": 6.0,
      "color": [0.02, 0.06, 0.34, 0.46],
      "attach_offset": [0.0, -2.90, 0.0],
      "fade_in": 0.02,
      "fade_out": 0.34,
      "uv_scroll": [0.0, -0.30]
    },
    {
      "name": "e_connect_core_rail",
      "render_type": "Beam",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_e_stun_beam.png",
      "lifetime": 0.34,
      "start_delay": 0.05,
      "width": 0.24,
      "height": 6.0,
      "color": [0.56, 0.70, 1.85, 0.70],
      "attach_offset": [0.0, -2.88, 0.0],
      "fade_in": 0.02,
      "fade_out": 0.22,
      "uv_scroll": [0.0, -0.65]
    },
    {
      "name": "e_connect_afterglow",
      "render_type": "Beam",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_e_stun_beam_dark.png",
      "lifetime": 0.76,
      "start_delay": 0.12,
      "width": 1.08,
      "height": 6.0,
      "color": [0.32, 0.50, 1.45, 0.22],
      "attach_offset": [0.0, -2.87, 0.0],
      "fade_in": 0.05,
      "fade_out": 0.40,
      "uv_scroll": [0.0, -0.16]
    },
    {
      "name": "e_connect_mid_flash",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_e_warnig_spark.png",
      "lifetime": 0.20,
      "start_delay": 0.18,
      "width": 1.05,
      "height": 1.05,
      "color": [0.95, 1.08, 2.10, 0.78],
      "segment_t": 0.50,
      "attach_offset": [0.0, -2.20, 0.0],
      "fade_in": 0.01,
      "fade_out": 0.14,
      "billboard": true
    },
    {
      "name": "e_connect_blade_mesh",
      "render_type": "MeshParticle",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "model": "Client/Bin/Resource/Texture/FX/Irelia/fbx/irelia_base_e_beam.fbx",
      "texture": "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_e_beam_mult.png",
      "lifetime": 0.34,
      "start_delay": 0.04,
      "segment_t": 0.50,
      "scale_z_to_segment": true,
      "scale": [0.020, 0.020, 0.020],
      "rotation": [0.0, 0.0, 0.0],
      "color": [0.56, 0.70, 1.85, 0.68],
      "attach_offset": [0.0, -2.88, 0.0],
      "fade_in": 0.02,
      "fade_out": 0.22
    }
  ]
}
```

1-5. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Irelia/e_connect_pop.wfx

기존 파일 전체를 아래로 교체:

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
      "lifetime": 0.28,
      "width": 0.82,
      "height": 0.82,
      "color": [0.88, 1.02, 2.05, 0.84],
      "attach_offset": [0.0, 0.52, 0.0],
      "fade_in": 0.01,
      "fade_out": 0.18,
      "billboard": true
    },
    {
      "name": "e_connect_pop_ring",
      "render_type": "ShockwaveRing",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/FX/Irelia/irelia_base_temp_e_ring_indicator_v2.png",
      "lifetime": 0.42,
      "start_delay": 0.03,
      "start_radius": 0.22,
      "end_radius": 0.92,
      "thickness": 0.08,
      "width": 1.0,
      "height": 1.0,
      "color": [0.50, 0.62, 1.60, 0.28],
      "attach_offset": [0.0, -2.92, 0.0],
      "fade_in": 0.02,
      "fade_out": 0.28,
      "billboard": false
    }
  ]
}
```

2. 검증

미검증:
- 아직 코드에 반영하지 않은 수정 계획이다.
- 실제 화면에서 E place가 너무 작아지면 `e_place_ground_ring width/height`만 2.20에서 2.45까지 올려 2차 튜닝한다.
- E connect가 너무 얇으면 `e_connect_dark_rail width`만 0.76에서 0.90까지 올리고 core/mesh는 유지한다.

검증 명령:
- `git diff --check -- Client/Public/GameObject/Champion/Irelia/IreliaBladeSystem.h Client/Private/GameObject/Champion/Irelia/IreliaBladeSystem.cpp Data/LoL/FX/Champions/Irelia/e_place.wfx Data/LoL/FX/Champions/Irelia/e_connect.wfx Data/LoL/FX/Champions/Irelia/e_connect_pop.wfx`
- `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' Winters.sln /m /p:Configuration=Debug /p:Platform=x64`

확인 필요:
- 새 파일 추가가 없으므로 `.vcxproj`/`.filters` 수정은 하지 않는다.
- `WintersServer.exe`가 실행 중이면 전체 솔루션 링크가 잠길 수 있으므로 빌드 전 프로세스 확인이 필요하다.
