# Session - Viego Soul Eating and Kalista Sentinel Implementation

Date: 2026-06-24
Scope: Viego soul visual/consume/form flow plus Kalista W sentinel cone vision implementation

## 1. 반영해야 하는 코드

### 목표

- Viego kill soul은 서버가 생성하는 `EffectAnchor`이고, 클라이언트는 해당 챔피언 모델을 초록 소울 비주얼로 붙인다.
- Viego가 soul을 우클릭하면 즉시 변신하지 않고 `ViegoConsumeSoul` 액션을 재생한 뒤 변신한다.
- 변신 중 Q/W/E/BA는 대상 챔피언 스킬로 사용하고 R은 Viego R로 사용하면서 변신을 해제한다.
- Kalista W는 직선 왕복하는 서버 권한 sentinel을 만들고, sentinel의 60도 cone 기준으로 Fog of War를 밝힌다.

### 현재 코드 증거

- `Shared/GameSim/Champions/Viego/ViegoGameSim.cpp`
  - `TrySpawnSoulForKill`가 이미 enemy kill soul을 생성한다.
  - `ViegoSoulComponent` snapshot은 `EffectAnchor`로 내려간다.
- `Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`
  - `TryHandleViegoSoulBasicAttack`가 이미 Viego 우클릭 soul consume을 처리한다.
  - 현재는 consume 시점에 바로 `FormOverrideComponent`를 붙인다.
- `Client/Private/Network/Client/SnapshotApplier.cpp`
  - `kSnapshotStateViegoSoulFlag`를 보고 `Viego::Fx::SpawnSoulIdle`을 호출한다.
  - 현재 soul anchor에는 실제 champion renderer가 붙지 않는다.
- `Client/Private/Network/Client/EventApplier.cpp`
  - `ViegoConsumeSoul` 액션은 `passive_attack` 애니메이션을 재생한다.
- `Data/LoL/FX/Champions/Viego/soul_idle.wfx`
  - soul idle mist/glow/cloud ring cue가 이미 존재한다.
- `.md/plan/2026-06-24_KALISTA_W_SENTINEL_CONE_VISION_PLAN.md`
  - Kalista W sentinel의 설계 방향은 이미 문서화했다.

### 1-1. Viego consume flow

`Shared/GameSim/Components/ViegoSimComponent.h`

- pending possession fields를 추가한다.
- 즉시 변신 대신 `bPossessionPending`과 `possessionApplyTimerSec`로 consume animation 종료 시점을 기다린다.

`Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`

- `TryHandleViegoSoulBasicAttack`에서 기존 즉시 `FormOverrideComponent` 생성 코드를 제거한다.
- 대신 ViegoSimComponent에 pending champion/target/delay/duration을 저장한다.
- `StartCommandActionState(... ViegoConsumeSoul ...)`는 유지한다.
- soul entity는 중복 consume 방지를 위해 즉시 제거한다.

`Shared/GameSim/Champions/Viego/ViegoGameSim.cpp`

- pending timer가 0이 되면 `FormOverrideComponent`를 붙인다.
- `skillSlotMask`는 기본값 `0x0F`를 유지해 BA/Q/W/E만 대상 챔피언으로 바꾸고 R은 Viego R로 남긴다.
- `OnR` 시작 시 active/pending possession을 해제한다.

`Client/Private/Network/Client/EventApplier.cpp`

- `ViegoConsumeSoul` 액션은 form override 상태와 무관하게 Viego의 `passive_attack`을 사용한다.

### 1-2. Viego soul visual

`Client/Private/Network/Client/SnapshotApplier.cpp`

- soul snapshot을 처음 받거나 renderer가 없으면 `m_onChampionVisualChanged` 콜백으로 해당 챔피언 visual을 붙인다.
- soul entity에 `ChampionComponent`를 보강해서 render pass 대상이 되게 한다.
- renderer에는 `SetMaterialOverrideColor(Vec4{ 0.20f, 1.05f, 0.72f, 0.80f }, true)`를 적용한다.
- 기존 `Viego::Fx::SpawnSoulIdle`은 유지해 주변 초록 오라/연기 cue를 붙인다.

`Client/Private/Scene/Scene_InGameRender.cpp`

- `ViegoSoulComponent`가 있으면 champion id와 무관하게 soul material override를 유지한다.

### 1-3. Kalista W sentinel

`Shared/GameSim/Components/KalistaSentinelComponent.h`

- 서버/클라가 공유하는 sentinel state component를 추가한다.

`Shared/GameSim/Definitions/EffectAnchorSubtype.h`

- `EffectAnchorSubtype::KalistaWSentinel` 상수를 추가한다.

`Engine/Public/ECS/Components/VisionComponents.h`

- optional `VisionConeComponent`를 추가한다.

`Engine/Private/ECS/Systems/VisionSystem.cpp`

- source가 `VisionConeComponent`를 가지면 radius 후보를 cone dot product로 필터링한다.
- FOW texture 갱신도 같은 cone 필터를 적용한다.

`Shared/GameSim/Champions/Kalista/KalistaGameSim.cpp`

- `W_OnCastAccepted`에서 sentinel anchor를 생성한다.
- `Tick`에서 start/end 구간을 smooth ping-pong으로 왕복 이동시킨다.

`Server/Private/Game/SnapshotBuilder.cpp`

- `KalistaSentinelComponent` entity를 `EffectAnchor` subtype으로 복제한다.

`Client/Private/Network/Client/SnapshotApplier.cpp`

- Kalista sentinel `EffectAnchor`를 받으면 client-side vision source/cone을 보강한다.
- 첫 수신 시 Kalista W avatar/cone visual을 anchor에 붙인다.

`Client/Private/GameObject/Champion/Kalista/KalistaFxPresets.cpp`

- `SpawnWSentinelIdle` helper를 추가해 W 원혼 이미지와 회색 cone decal을 붙인다.

`Client/Private/GameObject/Champion/Kalista/Kalista_Registration.cpp`

- Kalista W를 direction/on-accept hook으로 등록한다.

## 2. 검증

검증 명령:

```text
git diff --check
Build-Server-Debug.bat
Build-Client-Debug.bat
```

인게임 확인:

- Viego가 적 처치 시 해당 챔피언 soul anchor가 한 번만 생성된다.
- soul 주변에 초록 오라가 붙고, soul 본체가 초록/반투명 렌더링된다.
- Viego가 soul을 우클릭하면 `passive_attack` consume 애니메이션이 먼저 재생된다.
- consume 애니메이션 이후 변신이 적용된다.
- 변신 중 BA/Q/W/E는 대상 챔피언 스킬로, R은 Viego R로 사용되며 R 사용 후 변신이 해제된다.
- Kalista W 사용 시 sentinel이 직선 왕복하고 회색 60도 cone visual이 보인다.
- Kalista W cone 내부 fog만 밝혀지고 기존 원형 시야는 변하지 않는다.
