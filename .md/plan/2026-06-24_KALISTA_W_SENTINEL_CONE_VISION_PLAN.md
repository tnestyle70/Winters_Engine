# Session - Kalista W Sentinel Cone Vision

Date: 2026-06-24
Scope: Kalista W sentinel patrol, 60-degree cone boundary, and cone-based fog reveal

## 1. 반영해야 하는 코드

### 목표

칼리스타 W의 본질은 데미지 스킬이 아니라 "움직이는 시야 소스"다.

- 서버는 원혼의 생성, 위치, 수명, 팀, 부채꼴 시야 범위를 소유한다.
- 클라이언트는 서버가 복제한 원혼 상태를 보고 원혼 이미지와 회색 부채꼴 경계를 표현한다.
- 시야 판정은 원형 VisionSource를 우회하지 않고, 기존 `CVisionSystem`에 부채꼴 필터를 최소 확장한다.
- 부채꼴 각도는 전체 60도 기준이다. 내부 계산은 half angle 30도, `cos(30deg) = 0.8660254`를 사용한다.

### 현재 코드 증거

- `Client/Private/GameObject/Champion/Kalista/Kalista_Registration.cpp:102`
  - Kalista W는 `slot = 2`로 등록되어 있지만 현재 `Self`, `rangeMax = 0`, 훅 없음 상태다.
- `Shared/GameSim/Champions/Kalista/KalistaGameSim.cpp:127`
  - 서버 Gameplay hook은 현재 E만 등록한다.
- `Shared/GameSim/Champions/Kalista/KalistaGameSim.cpp:140`
  - `KalistaGameSim::Tick`은 현재 비어 있다.
- `Engine/Public/ECS/Components/VisionComponents.h:9`
  - `VisionSourceComponent`는 현재 원형 `sightRange`만 보유한다.
- `Engine/Private/ECS/Systems/VisionSystem.cpp:162`
  - 타겟 가시성 계산은 `VisionSourceComponent`와 `QueryRadius` 기반이다.
- `Engine/Private/ECS/Systems/VisionSystem.cpp:302`
  - Fog of War 텍스처 갱신도 현재 원형 반경으로만 밝힌다.
- `Shared/Schemas/Snapshot.fbs:15`
  - `EntityKind::EffectAnchor`가 이미 있다. 원혼은 데미지 없는 투사체가 아니라 이 타입의 복제 앵커로 다루는 것이 맞다.
- `Client/Bin/Resource/Texture/Character/Kalista/particles/kalista_base_w_avatar.png`
  - W 원혼 이미지가 이미 있다.
- `Client/Bin/Resource/Texture/Character/Kalista/particles/kalista_base_w_viewcone.png`
  - W 부채꼴 시야 표현용 텍스처가 이미 있다.
- `Client/Bin/Resource/Texture/Character/Kalista/particles/fbx/kalista_base_w_radialdisc.fbx`
  - 필요하면 ground cone mesh 표현에 사용할 수 있는 리소스가 이미 있다.

### 본질 구조

```text
Client Input W(direction)
-> GameCommand(slot W, direction)
-> Server KalistaGameSim::OnW
-> server creates KalistaWSentinel entity
-> KalistaGameSim::Tick moves sentinel by ping-pong line motion
-> VisionSystem reveals cone area from sentinel
-> Snapshot replicates EffectAnchor transform/team/subtype
-> Client SnapshotApplier tags it as Kalista W sentinel
-> Client visual system draws avatar + 60-degree gray cone boundary
```

### 1-1. 데이터

`Data/LoL/ServerPrivate/Gameplay/SkillGameplayDefs.json`

`skill.kalista.w`의 `effect.params`에 아래 의미의 값을 추가한다. 새 enum을 먼저 늘리지 않고 기존 파라미터를 재사용한다.

```json
"params": [
  { "id": "Range", "value": 12.0 },
  { "id": "Speed", "value": 3.5 },
  { "id": "EffectDurationSec", "value": 12.0 },
  { "id": "SummonSightRange", "value": 10.0 },
  { "id": "SummonRadius", "value": 0.45 },
  { "id": "HalfAngleCos", "value": 0.8660254 }
]
```

`Data/Gameplay/ChampionGameData/champions.json`

- Kalista W `slot: 2`는 방향이 필요하므로 `targetMode`를 `Direction` 계열로 정리한다.
- 현재 이펙트 튜닝 중인 상태를 유지하기 위해 cooldown은 `1.0`을 유지한다.
- `rangeMax`는 원혼의 이동 구간 길이와 같은 값으로 맞춘다.

`Data/LoL/ClientPublic/Visual/ChampionVisualDefs.json`

- W animation playback은 우선 기존 `1.0`을 유지한다.
- W 원혼 시각 효과는 스킬 애니메이션에 직접 붙이지 않고, 복제된 `EffectAnchor` 생명주기에 붙인다.

### 1-2. 서버 게임플레이

새 파일: `Shared/GameSim/Components/KalistaSentinelComponent.h`

역할은 원혼의 시뮬레이션 상태만 담는 것이다.

```cpp
#pragma once

#include "ECS/Entity.h"
#include "GameContext.h"
#include "WintersMath.h"
#include "WintersTypes.h"

#include <type_traits>

struct KalistaSentinelComponent
{
    EntityID owner = NULL_ENTITY;
    eTeam team = eTeam::Blue;
    Vec3 startPos{};
    Vec3 endPos{};
    Vec3 forward{ 0.f, 0.f, 1.f };
    f32_t speed = 3.5f;
    f32_t sightRange = 10.f;
    f32_t halfAngleCos = 0.8660254f;
    f32_t radius = 0.45f;
    f32_t ageSec = 0.f;
    f32_t durationSec = 12.f;
};

static_assert(std::is_trivially_copyable_v<KalistaSentinelComponent>,
    "KalistaSentinelComponent must be trivially copyable for GameSim determinism.");
```

`Shared/GameSim/Champions/Kalista/KalistaGameSim.cpp`

- `GameplayHookVariant::W_OnCastAccepted`를 등록한다.
- `OnW`에서 caster 위치와 command direction을 기준으로 원혼 앵커를 생성한다.
- direction이 0이면 caster yaw/facing을 fallback으로 사용한다.
- 생성 엔티티에는 다음 컴포넌트를 붙인다.
  - `KalistaSentinelComponent`
  - `TransformComponent`
  - `SpatialAgentComponent` with `kind = eSpatialKind::Ward`, caster team, small radius
  - `VisionSourceComponent` with `sightRange = SummonSightRange`
  - `VisionConeComponent` with `forward`, `halfAngleCos`
  - `VisibilityComponent`
  - `NetEntityIdComponent` if `tc.pEntityMap` exists
- `Tick`에서 원혼을 직선 왕복 운동시킨다.
  - `segmentLength = DistanceXZ(startPos, endPos)`
  - `phase = fmod(ageSec * speed / segmentLength, 2.0)`
  - `rawT = phase <= 1.0 ? phase : 2.0 - phase`
  - 자연스러운 왕복감을 위해 위치 보간에는 `smoothstep(rawT)`를 적용한다.
  - 돌아오는 구간에서는 `forward = -baseForward`로 갱신해 부채꼴 시야가 진행 방향을 바라보게 한다.
- `ageSec >= durationSec`이면 서버에서 원혼 엔티티를 제거한다.

주의:

- 이 스킬은 데미지/충돌 투사체가 아니므로 `SkillProjectileComponent`를 사용하지 않는다.
- 타겟 가능/파괴 가능 원혼은 이번 요구사항의 본질이 아니므로 `TargetableTag`는 1차 구현에서 제외한다.

### 1-3. 부채꼴 시야 확장

`Engine/Public/ECS/Components/VisionComponents.h`

원형 시야를 깨지 않기 위해 `VisionSourceComponent`에 모양 필드를 섞기보다 선택 컴포넌트를 추가한다.

```cpp
struct VisionConeComponent
{
    Vec3 forward{ 0.f, 0.f, 1.f };
    f32_t halfAngleCos = 0.8660254f;
};
```

`Engine/Public/ECS/Systems/VisionSystem.h`

- `IsTargetVisibleFast`에 optional cone 인자를 추가하거나, 내부 helper `IsInsideVisionConeXZ`를 추가한다.

`Engine/Private/ECS/Systems/VisionSystem.cpp`

- `TickVisibility`에서 source가 `VisionConeComponent`를 가지면 radius 후보를 먼저 구한 뒤 dot product로 부채꼴 내부만 통과시킨다.
- `UpdateFowTexture`에서도 source가 `VisionConeComponent`를 가지면 원형 텍셀 후보 중 부채꼴 내부 텍셀만 `VisibleValue`로 올린다.
- 기존 champion/minion/turret/ward 원형 시야는 `VisionConeComponent`가 없으므로 동작이 변하지 않는다.
- bush/true sight 규칙은 기존 `VisibilityComponent` 로직을 유지한다. 이 작업은 "안 보이던 지형/FOW를 밝히는 것"이지 은신/부쉬 규칙을 무시하는 true sight가 아니다.

### 1-4. 스냅샷 복제

`Server/Private/Game/SnapshotBuilder.cpp`

- `KalistaSentinelComponent`를 가진 엔티티는 `EntityKind::EffectAnchor`로 복제한다.
- `championId = KALISTA`, `team = sentinel.team`, `ownerNet = owner`, `subtype = kEffectAnchorSubtypeKalistaWSentinel`로 보낸다.
- 별도 schema 필드 추가는 1차 구현에서 피한다. 위치와 yaw는 기존 snapshot 필드를 사용한다.

새 파일: `Shared/GameSim/Definitions/EffectAnchorSubtype.h`

```cpp
#pragma once

#include "WintersTypes.h"

namespace EffectAnchorSubtype
{
    inline constexpr u16_t KalistaWSentinel = 0x0802;
}
```

`Client/Private/Network/Client/SnapshotApplier.cpp`

- `EntityKind::EffectAnchor`, `championId == KALISTA`, `subtype == EffectAnchorSubtype::KalistaWSentinel`이면 클라이언트 엔티티에 원혼 런타임 태그를 붙인다.
- 클라이언트 FOW 갱신을 위해 `SpatialAgentComponent`, `VisibilityComponent`, `VisionSourceComponent`, `VisionConeComponent`도 동일하게 보강한다.
- 시야 수치와 half-angle은 클라이언트도 같은 데이터 정의를 읽거나, 우선 동일 fallback 상수를 둔다. 장기적으로는 generated gameplay definition에서 같은 값을 읽는 쪽이 맞다.

CONFIRM_NEEDED:

- 현재 Snapshot schema에는 원혼 전용 sightRange/halfAngle/speed 필드가 없다. 1차는 `subtype + shared data fallback`으로 충분하지만, 런타임 튜닝 값을 클라 FOW에 100% 즉시 반영해야 하면 `EntitySnapshot` 확장이 필요하다.

### 1-5. 클라이언트 비주얼

새 파일 후보:

- `Client/Public/GameObject/Champion/Kalista/KalistaSentinelSystem.h`
- `Client/Private/GameObject/Champion/Kalista/KalistaSentinelSystem.cpp`

역할:

- 복제된 Kalista W sentinel anchor를 찾는다.
- 원혼 avatar billboard를 anchor 위에 붙인다.
- 지면에는 회색 60도 부채꼴 경계를 그린다.
- snapshot transform을 따르므로 별도 클라 이동 예측은 하지 않는다.

리소스:

- avatar: `Client/Bin/Resource/Texture/Character/Kalista/particles/kalista_base_w_avatar.png`
- cone texture: `Client/Bin/Resource/Texture/Character/Kalista/particles/kalista_base_w_viewcone.png`
- cone mesh fallback: `Client/Bin/Resource/Texture/Character/Kalista/particles/fbx/kalista_base_w_radialdisc.fbx`

표현 원칙:

- 회색 경계는 `Vec4{ 0.55f, 0.58f, 0.62f, 0.38f }` 수준의 AlphaBlend로 시작한다.
- 원혼 avatar는 수직 billboard로 두고, cone boundary는 지면 decal/mesh로 둔다.
- cone 방향은 `VisionConeComponent.forward`와 동일해야 한다. 시각 경계와 실제 시야 판정이 어긋나면 안 된다.
- `.wfx` 에셋으로 자연스럽게 표현 가능한지 확인한 뒤, 어렵다면 전용 system에서 mesh/billboard를 직접 관리한다.

`Client/Private/Scene/Scene_InGameLifecycle.cpp`

- `m_pKalistaProjectileSystem`, `m_pKalistaRendSystem` 생성 위치 근처에 `m_pKalistaSentinelSystem`을 추가한다.
- FX mesh preload 목록에는 W radialdisc mesh/texture를 추가한다.

`Client/Private/Scene/Scene_InGame*.cpp`

- 기존 skill FX system execute 흐름에 `KalistaSentinelSystem::Execute`를 추가한다.

### 1-6. 칼리스타 스킬 등록

`Client/Private/GameObject/Champion/Kalista/Kalista_Registration.cpp`

- `kKal_W_OnAccept = MakeHookId(eChampion::KALISTA, HookVariant::W_OnCastAccepted)` 추가.
- W `SkillDef`를 방향형으로 정리한다.
  - `targetMode = eTargetMode::Direction`
  - `rangeMax = 12.f`
  - `rotate = eRotateMode::TowardsCursor`
  - `onCastAcceptedHookId = kKal_W_OnAccept`
- client visual hook은 원혼 자체를 직접 생성하지 않는다. 서버 snapshot anchor가 비주얼의 생명주기 주인이기 때문이다.
- 단, W cast 순간 손/몸 주변 cast flash가 필요하면 별도 짧은 cue만 visual hook에서 재생한다.

`Client/Private/GameObject/Champion/Kalista/Kalista_Skills.cpp`

- 1차 구현에서는 W visual handler를 비워두거나 cast flash만 담당한다.
- 원혼 avatar와 cone boundary는 `KalistaSentinelSystem`이 담당한다.

### 1-7. FX 데이터 위치

현재 Kalista 전용 `.wfx` 폴더는 확인되지 않았다. 새로 추가한다면 아래 경로가 맞다.

- `Data/LoL/FX/Champions/Kalista/w_sentinel_avatar.wfx`
- `Data/LoL/FX/Champions/Kalista/w_sentinel_cone.wfx`

하지만 이 작업의 본질은 moving anchor에 붙는 persistent visual이므로, `.wfx`는 "스폰 순간/짧은 보조 효과"에만 쓰고 지속 원혼/부채꼴은 system에서 anchor를 따라가게 하는 편이 안전하다.

## 2. 검증

### 빌드 검증

1. `Build-Client-Debug.bat`
2. `Build-Server-Debug.bat`
3. `Build-Tools-Debug.bat` if `.wfx` 또는 generated data tool을 수정한 경우
4. generated gameplay definition을 갱신했다면 생성 파일 diff 확인

### 단위/로그 검증

- W cast accepted 시 서버 로그:
  - caster entity
  - sentinel entity/netId
  - start/end position
  - team
  - sightRange
  - halfAngleCos
- Debug overlay 또는 `OutputDebugStringA`로 클라이언트:
  - snapshot EffectAnchor 수신 여부
  - Kalista W sentinel subtype 매칭 여부
  - avatar/cone visual entity 생성 여부

### 인게임 검증

1. 칼리스타로 W 사용.
2. 원혼 이미지가 칼리스타 앞 방향으로 생성된다.
3. 원혼이 start/end 구간을 자연스럽게 직선 왕복한다.
4. 왕복 반환 구간에서 부채꼴 방향이 이동 방향으로 뒤집힌다.
5. 회색 60도 부채꼴 경계와 실제 밝아지는 FOW 범위가 일치한다.
6. cone 바깥의 fog는 밝아지지 않는다.
7. 기존 챔피언/미니언/타워 원형 시야가 변하지 않는다.
8. 적 팀 클라이언트에서 아군 원혼 시야가 잘못 공유되지 않는다.
9. 원혼 수명 만료 후 서버 엔티티, snapshot entity, client visual이 모두 사라진다.

### 회귀 확인

- 칼리스타 Q/E 기존 spear/rend FX가 그대로 동작한다.
- `VisionSourceComponent`만 가진 기존 엔티티는 원형 시야를 유지한다.
- `EntityKind::EffectAnchor`를 사용하는 Viego soul 표시가 깨지지 않는다.

## 결과 보고서에 남길 항목

- 실제 사용한 W 리소스 경로.
- 최종 튜닝 값: patrol range, speed, duration, sightRange, halfAngleCos.
- 스냅샷 subtype 값과 클라 매칭 경로.
- 시각 cone과 FOW cone이 일치하는지 인게임 확인 결과.
- `CONFIRM_NEEDED`였던 snapshot schema 확장 필요 여부.
