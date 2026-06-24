Session - Viego W를 charge glow 3단계와 W2 Soul dash로 분리하고 dash 거리/속도를 절반으로 낮춘다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Data/Gameplay/ChampionGameData/champions.json

`champion == "VIEGO"`의 `slot: 2` W 원천 데이터에서 아래 기존 코드를:

기존 코드:

```json
"stageCount": 2,
"stageWindowSec": 4.0,
"cooldownSec": 1.0,
"rangeMax": 8.0,
"manaCost": 0.0,
```

아래로 교체:

```json
"stageCount": 2,
"stageWindowSec": 4.0,
"cooldownSec": 1.0,
"rangeMax": 4.0,
"manaCost": 0.0,
```

튜닝 기준:
- `stageWindowSec`: W charge 전체 가능 시간. 현재 4.0초라 glow 1/3 지점은 1.33초, 2/3 지점은 2.67초다.
- `rangeMax`: 서버 권위 W2 dash 거리. 8.0에서 4.0으로 줄인다.
- `dashDurationSec`: 기존 0.26초 유지. 거리가 절반이고 시간이 같으므로 실제 dash 속도도 절반이 된다.

1-2. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Viego/Viego_Skills.cpp

`OnCastFrame_W`와 `OnCastFrame_W_Visual`에서 아래 기존 코드를:

기존 코드:

```cpp
Fx::SpawnWMissile(*ctx.pWorld, ctx.pFxMeshRenderer, ctx.casterEntity, origin, dir, 0.45f);
```

아래로 교체:

```cpp
if (ctx.skillStage >= 2u)
    Fx::SpawnWMissile(*ctx.pWorld, ctx.pFxMeshRenderer, ctx.casterEntity, origin, dir, 0.45f);
else
    Fx::SpawnWChargeGlow(*ctx.pWorld, ctx.pFxMeshRenderer, ctx.casterEntity, origin, dir);
```

튜닝 기준:
- W1 charge stage는 glow만 재생한다.
- W2 dash stage에서만 Soul/missile 계열 FX를 재생한다.
- stage 판정은 서버 이벤트에서 내려온 `skillStage`만 사용한다.

1-3. C:/Users/user/Desktop/Winters/Client/Public/GameObject/Champion/Viego/Viego_FxPresets.h

아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
void SpawnQSlash(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
    EntityID owner, const Vec3& origin, const Vec3& dir, f32_t fLifetime);
```

아래에 추가:

```cpp
void StopWChargeGlow(CWorld& world, EntityID owner);
void SpawnWChargeGlow(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
    EntityID owner, const Vec3& origin, const Vec3& dir);
```

1-4. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Viego/Viego_FxPresets.cpp

상단 include 영역에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
#include "ECS/World.h"
```

아래에 추가:

```cpp
#include <unordered_map>
#include <vector>
```

익명 namespace의 cue 상수 영역에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
constexpr const char* kCueQSlash = "Viego.Q.Slash";
```

아래에 추가:

```cpp
constexpr const char* kCueWChargeGlow = "Viego.W.ChargeGlow";
```

익명 namespace의 cue 상수 영역 끝에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
constexpr const char* kCueSoulIdle = "Viego.Soul.Idle";
```

아래에 추가:

```cpp
std::unordered_map<EntityID, std::vector<EntityID>> g_WChargeGlowEntities;
```

`MakeAttachedCue` 함수 바로 아래에 추가:

기존 코드:

```cpp
FxCueContext MakeAttachedCue(EntityID owner, const Vec3& worldPos,
    Engine::CFxStaticMeshRenderer* pRenderer, f32_t fLifetime)
{
    FxCueContext cue{};
    cue.attachTo = owner;
    cue.vWorldPos = worldPos;
    cue.pFxMeshRenderer = pRenderer;
    cue.bOverrideLifetime = true;
    cue.fLifetimeOverride = fLifetime;
    return cue;
}
```

아래에 추가:

```cpp
void DestroyLiveEntities(CWorld& world, const std::vector<EntityID>& entities)
{
    for (const EntityID entity : entities)
    {
        if (entity != NULL_ENTITY && world.IsAlive(entity))
            world.DestroyEntity(entity);
    }
}
```

`SpawnWMissile` 시작부에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
void SpawnWMissile(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
    EntityID owner, const Vec3& origin, const Vec3& dir, f32_t fLifetime)
{
```

아래에 추가:

```cpp
StopWChargeGlow(world, owner);
```

`SpawnWMissile` 내부 이동 거리/속도 값을 아래로 교체:

기존 코드:

```cpp
castCue.vEndWorldPos = OffsetForward(start, forward, 4.6f);
missileCue.vVelocity = { forward.x * 7.2f, 0.f, forward.z * 7.2f };
missileCue.vEndWorldPos = OffsetForward(start, forward, 4.8f, 0.85f);
```

아래로 교체:

```cpp
castCue.vEndWorldPos = OffsetForward(start, forward, 2.3f);
missileCue.vVelocity = { forward.x * 3.6f, 0.f, forward.z * 3.6f };
missileCue.vEndWorldPos = OffsetForward(start, forward, 2.4f, 0.85f);
```

`SpawnWMissile` 함수 바로 아래에 추가:

기존 코드:

```cpp
CFxCuePlayer::Play(world, kCueWMissile, missileCue);
}
```

아래에 추가:

```cpp
void StopWChargeGlow(CWorld& world, EntityID owner)
{
    if (owner == NULL_ENTITY)
        return;

    const auto it = g_WChargeGlowEntities.find(owner);
    if (it == g_WChargeGlowEntities.end())
        return;

    DestroyLiveEntities(world, it->second);
    g_WChargeGlowEntities.erase(it);
}
```

`SpawnWChargeGlow` 새 함수:

```cpp
void SpawnWChargeGlow(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
    EntityID owner, const Vec3& origin, const Vec3& dir)
{
    StopWChargeGlow(world, owner);

    const Vec3 start = owner != NULL_ENTITY ? ResolvePosition(world, owner) : origin;
    const Vec3 forward = NormalizeOrForward(dir);

    FxCueContext cue{};
    cue.vWorldPos = start;
    cue.vForward = forward;
    cue.pFxMeshRenderer = pRenderer;

    std::vector<EntityID> spawned;
    CFxCuePlayer::PlayAll(world, kCueWChargeGlow, cue, &spawned);

    if (owner != NULL_ENTITY && !spawned.empty())
        g_WChargeGlowEntities[owner] = spawned;
}
```

튜닝 기준:
- `StopWChargeGlow`: W2가 빨리 발동될 때 아직 start delay 대기 중인 2/3, 3/3 glow를 제거한다.
- `SpawnWMissile`: Soul/missile 계열 W2 FX만 담당한다.
- `SpawnWChargeGlow`: W1 charge glow cue 전체를 생성하고 owner별 entity를 추적한다.

1-5. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Viego/w_charge_glow.wfx

새 파일:

```json
{
  "schema": "WintersWfx",
  "version": 1,
  "name": "Viego.W.ChargeGlow",
  "emitters": [
    {
      "name": "w_charge_glow_01",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Viego/particles/viego_base_e_glow.png",
      "lifetime": 4.0,
      "fade_in": 0.12,
      "fade_out": 0.35,
      "width": 1.05,
      "height": 1.05,
      "color": [0.42, 1.45, 0.92, 0.64],
      "attach_offset": [0.0, 1.05, 1.05],
      "billboard": true
    },
    {
      "name": "w_charge_glow_02",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Viego/particles/viego_base_e_glow.png",
      "lifetime": 2.67,
      "start_delay": 1.33,
      "fade_in": 0.12,
      "fade_out": 0.35,
      "width": 1.12,
      "height": 1.12,
      "color": [0.50, 1.62, 1.02, 0.70],
      "attach_offset": [-0.44, 1.12, 1.42],
      "billboard": true
    },
    {
      "name": "w_charge_glow_03",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Viego/particles/viego_base_e_glow.png",
      "lifetime": 1.33,
      "start_delay": 2.67,
      "fade_in": 0.12,
      "fade_out": 0.35,
      "width": 1.18,
      "height": 1.18,
      "color": [0.62, 1.80, 1.15, 0.78],
      "attach_offset": [0.44, 1.18, 1.42],
      "billboard": true
    }
  ]
}
```

튜닝 기준:
- `start_delay`: 0.0, 1.33, 2.67로 charge window 4.0초를 3등분한다.
- `lifetime`: 각 glow가 4.0초 charge 끝까지 유지되도록 4.0, 2.67, 1.33을 사용한다.
- `attach_offset`: `x`는 좌우 분산, `y`는 높이, `z`는 비에고 전방 거리다.
- `color`, `width`, `height`, `fade_in`, `fade_out`이 디자이너/FX 튜닝 지점이다.

1-6. C:/Users/user/Desktop/Winters/Data/LoL/ServerPrivate/Gameplay/SkillGameplayDefs.json

직접 편집하지 않는다. `Data/Gameplay/ChampionGameData/champions.json`에서 `Build-LoLDefinitionPack.py`로 생성한다.

생성 후 `skill.viego.w`의 서버 권위 값은 아래 형태가 되어야 한다:

```json
"key": "skill.viego.w",
"range": {
  "maximum": 4.0
},
"stage": {
  "count": 2,
  "windowSeconds": 4.0
}
```

1-7. C:/Users/user/Desktop/Winters/Server/Private/Data/Generated/LoLGameplayDefinitions.generated.cpp

직접 편집하지 않는다. `Build-LoLDefinitionPack.py` 생성 결과에서 `MakeSkill_VIEGO_W()`는 아래 값을 가져야 한다:

```cpp
def.range.rangeMax = 4.f;
def.stage.stageWindowSec = 4.f;
```

2. 검증

검증 명령:
- `git diff --check`
- `python Tools/LoLData/Build-LoLDefinitionPack.py --check`
- `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1 -Configuration Debug`

확인 필요:
- 인게임에서 W1 charge 시작 직후 비에고 전방 glow 1개가 보이는지 확인.
- charge 1.33초 이후 glow 2개, 2.67초 이후 glow 3개가 되는지 확인.
- W2 dash가 발동되는 순간 남은 charge glow가 제거되고 Soul/missile이 앞으로 나가는지 확인.
- W1에는 Soul/missile이 나오지 않는지 확인.
- W2 dash 거리가 기존의 절반 수준인지 확인.
