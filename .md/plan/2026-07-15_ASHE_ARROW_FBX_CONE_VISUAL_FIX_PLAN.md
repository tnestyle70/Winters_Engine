Session - Ashe W의 서버 권위 8발·45도 cone과 BA FBX MeshParticle(BA/W 0.021)은 유지하고, snapshot yaw 보정으로 FBX의 local X축을 진행 방향에 맞춘다. E는 서버 EffectTrigger가 전달한 방향으로만 3x BA FBX를 0.70초간 이동시키며, R의 현재 3x BA FBX(0.063)는 보존·검증한다. Gameplay projectile, hit, damage, vision/reveal 권한은 추가하지 않는다.

## 1. 반영해야 하는 코드

### 1-1. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Projectile/ProjectileVisualCatalog.cpp

기존 코드:

```cpp
    constexpr u16_t kStructureProjectileKind = 100;

    constexpr ProjectileVisualDesc kNoProjectileVisual{};
```

아래에 추가:

```cpp
    constexpr f32_t kAsheArrowMeshYawOffset = -1.57079632679f;
```

기존 코드:

```cpp
    constexpr ProjectileVisualDesc kAsheBasicAttackVisual{
        "Ashe.BA.Arrow", nullptr, "Ashe.BA.Hit" 
    };

    constexpr ProjectileVisualDesc kAsheVolleyArrowVisual{
        "Ashe.W.Arrow", "Ashe.W.Hit", nullptr
    };

    constexpr ProjectileVisualDesc kAsheCrystalArrowVisual{
        "Ashe.R.Arrow", "Ashe.R.Hit", nullptr
    };
```

아래로 교체:

```cpp
    constexpr ProjectileVisualDesc kAsheBasicAttackVisual{
        "Ashe.BA.Arrow", nullptr, "Ashe.BA.Hit", nullptr, nullptr, nullptr,
        kAsheArrowMeshYawOffset
    };

    constexpr ProjectileVisualDesc kAsheVolleyArrowVisual{
        "Ashe.W.Arrow", "Ashe.W.Hit", nullptr, nullptr, nullptr, nullptr,
        kAsheArrowMeshYawOffset
    };

    constexpr ProjectileVisualDesc kAsheCrystalArrowVisual{
        "Ashe.R.Arrow", "Ashe.R.Hit", nullptr, nullptr, nullptr, nullptr,
        kAsheArrowMeshYawOffset
    };
```

`EnsureProjectilePresentation`은 매 snapshot에서 `mesh.vRotation.y = yaw`를 설정한다. 이 catalog offset은 WFX의 `rotation.y = -1.57079632679`와 같은 값을 그 `yaw`에 포함하므로 BA/W/R mesh의 authored 축 보정이 매 snapshot 뒤에도 유지된다. W/R trail은 camera-facing Billboard이므로 이 값으로 회전하지 않는다.

### 1-2. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/EventApplier.cpp

`EntityID CEventApplier::EnsureProjectilePresentation`의 `if (visual.pszSpawnCue)` 블록에서 아래 기존 코드 바로 아래에 bounded Debug trace를 추가한다. 이 trace는 W 한 번에 8개 projectile마다 mesh와 billboard가 각각 실제로 생성됐는지 확인하는 검증 전용이며 Release에는 포함되지 않는다.

기존 코드:

```cpp
            CFxCuePlayer::PlayAll(
                world,
                visual.pszSpawnCue,
                fx,
                &visualIt->second);
```

아래에 추가:

```cpp
#if defined(_DEBUG)
            const eProjectileKind projectileKind =
                static_cast<eProjectileKind>(uProjectileKind);
            if (projectileKind == eProjectileKind::AsheBasicAttack ||
                projectileKind == eProjectileKind::AsheVolleyArrow ||
                projectileKind == eProjectileKind::AsheCrystalArrow)
            {
                static u32_t s_asheProjectileVisualTraceCount = 0u;
                if (s_asheProjectileVisualTraceCount < 64u)
                {
                    u32_t meshCount = 0u;
                    u32_t billboardCount = 0u;
                    for (const EntityID visualEntity : visualIt->second)
                    {
                        if (world.HasComponent<FxMeshComponent>(visualEntity))
                            ++meshCount;
                        if (world.HasComponent<FxBillboardComponent>(visualEntity))
                            ++billboardCount;
                    }

                    char msg[256]{};
                    sprintf_s(
                        msg,
                        "[AsheProjectileVisual] net=%u kind=%u mesh=%u billboard=%u yaw=%.3f\\n",
                        static_cast<u32_t>(uProjectileNet),
                        static_cast<u32_t>(uProjectileKind),
                        meshCount,
                        billboardCount,
                        yaw);
                    OutputDebugStringA(msg);
                    ++s_asheProjectileVisualTraceCount;
                }
            }
#endif
```

### 1-3. C:/Users/user/Desktop/Winters/Client/Public/GameObject/Champion/Ashe/AsheVisualCueCatalog.h

기존 코드:

```cpp
namespace Ashe::VisualCue
{
    constexpr const char* kBAArrow = "Ashe.BA.Arrow";
    constexpr const char* kBAHit = "Ashe.BA.Hit";
    constexpr const char* kWCast = "Ashe.W.Cast";
    constexpr const char* kRCharge = "Ashe.R.Cast";

    constexpr f32_t kBAArrowSpeed = 18.f;
    constexpr f32_t kBAArrowLifetime = 0.4f;
    constexpr f32_t kWCastLifetime = 0.45f;
    constexpr f32_t kRChargeLifetime = 0.45f;
}
```

아래로 교체:

```cpp
namespace Ashe::VisualCue
{
    constexpr const char* kBAArrow = "Ashe.BA.Arrow";
    constexpr const char* kBAHit = "Ashe.BA.Hit";
    constexpr const char* kWCast = "Ashe.W.Cast";
    constexpr const char* kEHawkshot = "Ashe.E.Hawkshot";
    constexpr const char* kRCharge = "Ashe.R.Cast";

    constexpr f32_t kBAArrowSpeed = 18.f;
    constexpr f32_t kBAArrowLifetime = 0.4f;
    constexpr f32_t kWCastLifetime = 0.45f;
    constexpr f32_t kEHawkshotTravelSec = 0.70f;
    constexpr f32_t kRChargeLifetime = 0.45f;
}
```

### 1-4. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Ashe/Ashe_Skills.cpp

기존 코드:

```cpp
        void OnCastFrame_E_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand) return;
            if (!ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity)) return;

            const Vec3 origin =
                ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
            const Vec3 dir = ctx.pCommand->direction;
            const Vec3 dest = { origin.x + dir.x * 25.f, origin.y, origin.z + dir.z * 25.f };
            Fx::SpawnEHawkshot(*ctx.pWorld, origin, dest, 5.0f);
        }
```

아래로 교체:

```cpp
        void OnCastFrame_E_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand || !ctx.bAuthoritativeEvent) return;
            if (!ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity)) return;

            const Vec3 forward =
                WintersMath::NormalizeXZOrZero(ctx.pCommand->direction);
            if (forward.x == 0.f && forward.z == 0.f) return;

            const f32_t range = (ctx.pDef && ctx.pDef->rangeMax > 0.f)
                ? ctx.pDef->rangeMax
                : 25.f;
            const f32_t speed = range / VisualCue::kEHawkshotTravelSec;
            const Vec3 casterPosition =
                ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();

            FxCueContext fx{};
            fx.vWorldPos = {
                casterPosition.x + forward.x * 0.8f,
                casterPosition.y + 1.f,
                casterPosition.z + forward.z * 0.8f };
            fx.vForward = forward;
            fx.vVelocity = { forward.x * speed, 0.f, forward.z * speed };
            fx.attachTo = NULL_ENTITY;
            fx.pFxMeshRenderer = ctx.pFxMeshRenderer;
            fx.bOverrideVelocity = true;
            fx.bOverrideLifetime = true;
            fx.fLifetimeOverride = VisualCue::kEHawkshotTravelSec;
            CFxCuePlayer::PlayAll(
                *ctx.pWorld,
                VisualCue::kEHawkshot,
                fx,
                nullptr);
        }
```

이 함수는 local cast-frame/레거시 경로를 재생하지 않고 server EffectTrigger의 `ctx.bAuthoritativeEvent`에서 한 번만 실행한다. 새 cue에는 `anchor`를 쓰지 않고 `attachTo = NULL_ENTITY`로 두어 `CFxMeshSystem`이 velocity를 적분하게 한다.

### 1-5. C:/Users/user/Desktop/Winters/Client/Public/GameObject/Champion/Ashe/Ashe_FxPresets.h

삭제할 코드:

```cpp
    void SpawnEHawkshot(CWorld& world, const Vec3& start, const Vec3& dest, f32_t fLifetime);
```

### 1-6. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Ashe/Ashe_FxPresets.cpp

삭제할 코드:

```cpp
    constexpr const wchar_t* kPathEHawkTex =
        L"Texture/Character/Ashe/particles/ashe_base_e_textureowl.png";
```

삭제할 코드:

```cpp
    void SpawnEHawkshot(CWorld& world, const Vec3& start, const Vec3&, f32_t fLifetime)
    {
        FxBillboardComponent fx{};
        fx.attachTo = NULL_ENTITY;
        fx.vWorldPos = { start.x, start.y + 3.0f, start.z };
        fx.vAttachOffset = { 0.f, 0.f, 0.f };
        fx.texturePath = kPathEHawkTex;
        fx.fWidth = 1.4f;
        fx.fHeight = 1.0f;
        fx.bBillboard = true;
        fx.fLifetime = fLifetime;
        fx.vColor = { 0.8f, 1.1f, 1.3f, 1.f };
        fx.blendMode = eBlendPreset::Additive;
        fx.fFadeOut = fLifetime * 0.4f;
        CFxSystem::Spawn(world, fx);
    }
```

### 1-7. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Ashe/e_hawkshot.wfx

새 파일:

```json
{
  "name": "Ashe.E.Hawkshot",
  "emitters": [
    {
      "name": "e_hawkshot_arrow_mesh",
      "render_type": "MeshParticle",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "model": "Client/Bin/Resource/Texture/Character/Ashe/particles/fbx/ashe_base_aa_arrow.fbx",
      "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_aa_arrowtext.png",
      "lifetime": 0.70,
      "scale": [0.063, 0.063, 0.063],
      "rotation": [0.0, -1.57079632679, 0.0],
      "color": [0.78, 1.18, 1.52, 0.98],
      "attach_offset": [0.0, 0.0, 0.0],
      "fade_in": 0.01,
      "fade_out": 0.18
    }
  ]
}
```

E는 첫 검증 단계에서 Billboard/Ribbon emitter를 넣지 않는다. 따라서 mesh renderer 미주입 또는 mesh preload 실패 시 “trail만 남아서 FBX처럼 보이지 않는” 상태를 숨기지 않는다.

### 1-8. C:/Users/user/Desktop/Winters/Data/LoL/ClientPublic/Visual/ObjectVisualDefs.json

기존 코드:

```json
  "fxMeshPreloads": [
    {
      "key": "fx.irelia.e_beam",
      "mesh": "Texture/FX/Irelia/fbx/irelia_base_e_beam.fbx",
      "texture": "Texture/FX/Irelia/irelia_base_e_beam_mult.png"
    },
```

아래로 교체:

```json
  "fxMeshPreloads": [
    {
      "key": "fx.ashe.base_arrow",
      "mesh": "Texture/Character/Ashe/particles/fbx/ashe_base_aa_arrow.fbx",
      "texture": "Texture/Character/Ashe/particles/ashe_base_aa_arrowtext.png"
    },
    {
      "key": "fx.irelia.e_beam",
      "mesh": "Texture/FX/Irelia/fbx/irelia_base_e_beam.fbx",
      "texture": "Texture/FX/Irelia/irelia_base_e_beam_mult.png"
    },
```

이 single preload는 BA/W/E/R이 공유하는 cooked `ashe_base_aa_arrow.wmesh`를 게임 시작 시 한 번 준비한다. `Client/Private/Data/Generated/LoLVisualDefinitions.generated.cpp`는 직접 수정하지 않고 아래 검증 명령으로 재생성한다.

## 2. 검증

미검증:

- 이번 세션은 조사·계획서 작성만 수행한다. 사용자 클라이언트/서버가 실행 중이므로 코드, WFX, generated definition, 빌드는 수정·실행하지 않는다.

검증 명령:

```powershell
python Tools/LoLData/Build-LoLDefinitionPack.py
python Tools/LoLData/Build-LoLDefinitionPack.py --check
git diff --check
MSBuild.exe Winters.sln /m /p:Configuration=Debug /p:Platform=x64
```

수동 확인:

- normal F5 서버 권위 세션에서 BA/W/R을 각각 전방·후방·좌·우·대각선으로 발사한다. Debug Output의 `[AsheProjectileVisual]`에서 BA/W/R마다 `mesh=1`을 확인하고, W 한 번은 서로 다른 net ID 8개가 `mesh=1`, `billboard=1`로 기록되는지 확인한다.
- W의 8개 snapshot direction이 중앙을 기준으로 `-22.5°`부터 `+22.5°`까지 분포하고, 각 BA FBX long axis가 자신의 direction과 일치하는지 영상 캡처로 확인한다. 이때 W cast muzzle은 별도 cue이며 cone projectile을 다시 만들지 않는다.
- E를 좌/우 서로 다른 cursor 방향으로 두 번 시전한다. replicated EffectTrigger의 direction이 두 방향과 일치하고, `Ashe.E.Hawkshot`이 25 units를 `0.70s` 동안 이동하는지 확인한다. 새 E는 `MeshParticle` 하나만 생성하며, 서버에 projectile, damage, status, vision/reveal entity가 새로 생기지 않아야 한다.
- R은 현재 `r_arrow.wfx`의 BA FBX scale `0.063`을 유지한다. BA/W `0.021` 대비 E/R 모두 정확히 3x이고, R의 ice/smoke billboard trail은 보조 효과로만 남으며 3D arrow mesh의 방향을 가리지 않는지 확인한다.
- `FxCuePlayer`의 skipped-mesh 경고 또는 model preload 실패가 나오면 catalog yaw를 재조정하지 말고 먼저 renderer 주입(`Scene_InGameLifecycle`)과 `fx.ashe.base_arrow` preload 결과를 고친다. `FxMesh::Drawn` profiler counter도 동일 구간에서 증가해야 한다.

확인 필요:

- `Data/Gameplay/ChampionGameData/champions.json`의 Ashe E source targetMode는 현재 `Conditional`이지만, manual client registration은 `Direction`이다. normal F5 capture에서 E EffectTrigger direction이 nonzero이면 이번 visual 변경만 적용한다.
- E direction이 0으로 재현되는 bot/remote 경로가 있으면, generated `Data/LoL/ServerPrivate/Gameplay/SkillGameplayDefs.json`을 직접 고치지 않는다. 사용자 승인 후 source `champions.json`의 slot 3 `"targetMode": "Conditional"`을 `"targetMode": "Direction"`으로 바꾸고 `Build-LoLDefinitionPack.py`를 다시 실행해 서버/client definition contract를 함께 정렬한다. 이 변경은 target semantics를 바꾸므로 별도 authority 검증이 필요하다.
