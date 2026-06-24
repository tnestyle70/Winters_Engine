# Session - Yone BA/Q Animation Continuity

Date: 2026-06-24
Scope: Yone basic attack and Q animation continuity in network-authoritative LoL client

## 1. 반영해야 하는 코드

### 목표

1. 요네 BA/Q가 끊겨 보이는 원인을 애니메이션 추출 데이터와 런타임 재생 로직으로 분리해 검증한다.
2. 서버 권위 전투 판정, 쿨타임, action lock은 건드리지 않는다.
3. 클라이언트 시각화만 실제 애니메이션 클립 길이와 생성 데이터 재생 속도에 맞춰 회복되게 한다.
4. 레거시/오프라인 경로의 요네 BA/Q 등록값도 생성 데이터와 맞춰 협업자가 같은 결과를 보게 한다.

### 원인 분석

증거:

- `Client/Bin/Resource/Texture/Character/Yone/Yone.wskel`
  - bones: 172
  - hash: `0xda105f3000f5b6cc`
- `Client/Bin/Resource/Texture/Character/Yone/anims/skinned_mesh_yone_attack1.wanim`
  - channels: 169
  - duration: `8.000 / 24.000 = 0.333s`
  - keys: 4563
  - skel_hash: `0xda105f3000f5b6cc`
- `Client/Bin/Resource/Texture/Character/Yone/anims/skinned_mesh_yone_spell1_a1.wanim`
  - channels: 169
  - duration: `5.600 / 24.000 = 0.233s`
  - keys: 3042
  - skel_hash: `0xda105f3000f5b6cc`
- `Shared/GameSim/Generated/ChampionGameData.generated.cpp`
  - Yone BA/Q lockDurationSec: `0.9f`
  - Yone BA/Q animPlaySpeed: `0.85f`

판단:

- `.wanim`은 비어 있지 않고, key 수가 존재하며, 스켈레톤 해시도 요네 `.wskel`과 일치한다.
- 따라서 1차 원인은 Anim Mesh Binary Data 추출 누락이 아니라, 짧은 액션 클립을 긴 서버 action lock 시간 동안 클라이언트 상태 머신이 붙잡는 구조다.
- 기존 `EventApplier`는 서버 액션 애니메이션 재생 속도를 `CSkillRegistry`의 하드코딩 값에서 가져와 요네 BA/Q를 1.0배로 재생했다. 생성 데이터의 0.85배가 반영되지 않았다.
- 기존 `UpdateNetworkChampionLocomotion`은 클라이언트 시각화 타이머를 실제 클립 길이가 아니라 `ChampionGameDataDB::ResolveSkillTiming(...).lockDurationSec`로 잡았다. 요네 Q는 약 0.27초 재생 후 0.9초까지 마지막 프레임에 멈출 수 있었다.

### 수정 원리

- 서버 권위 lock은 전투 판정용이다.
- 클라이언트 액션 시각화 타이머는 presentation recovery용이다.
- 따라서 네트워크 액션 시각화는 `min(serverLockSec, animationDurationSec / playSpeed)`로 액션 클립을 붙잡는다.
- 루프 액션인 Jax E는 기존처럼 lock 기준을 유지한다.
- 재생 속도는 생성 데이터 `ChampionGameDataDB`를 우선 사용하고, 생성 데이터가 없을 때만 `SkillDef` 하드코딩 값으로 fallback한다.

### 반영 파일

`Client/Private/Network/Client/EventApplier.cpp`

- `ChampionGameDataDB` include 추가.
- `ResolveReplicatedActionPlaySpeed` 추가.
- `PlayReplicatedActionVisual`의 BA/Q/W/E/R 재생 속도 계산을 생성 데이터 우선으로 변경.

`Client/Private/Scene/Scene_InGameNetwork.cpp`

- `ResolveNetworkActionPlaySpeed` 추가.
- 기존 `ResolveNetworkActionDurationSec`를 lock duration 계산과 visual duration 계산으로 분리.
- 새 액션 sequence 수신 시 실제 액션 애니메이션 이름을 계산한 뒤, renderer의 `GetAnimationDurationSecondsByName`으로 클립 길이를 얻어 visual action timer에 반영.
- Jax E loop action은 기존 lock 기반 동작 유지.

`Client/Private/GameObject/Champion/Yone/Yone_Registration.cpp`

- Yone BA/Q legacy registration을 generated timing과 맞춤.
  - BA/Q `lockDurationSec = 0.9f`
  - BA/Q `animPlaySpeed = 0.85f`
- 이미 존재하는 요네 전환 클립을 짧게 연결.
  - BA idle: `attack1_toidle1`, run: `attack1_towalk1`, duration `0.18f`
  - Q run: `spell1a_towalk1`, duration `0.16f`

## 2. 검증

필수 검증:

1. `Tools/Bin/Debug/WintersAssetConverter.exe info Client/Bin/Resource/Texture/Character/Yone/Yone.wskel`
2. `Tools/Bin/Debug/WintersAssetConverter.exe info Client/Bin/Resource/Texture/Character/Yone/anims/skinned_mesh_yone_attack1.wanim`
3. `Tools/Bin/Debug/WintersAssetConverter.exe info Client/Bin/Resource/Texture/Character/Yone/anims/skinned_mesh_yone_spell1_a1.wanim`
4. `git diff --check`
5. `MSBuild Client/Include/Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal`

수동 확인:

1. 요네 BA가 입력 후 0프레임으로 다시 튀거나 마지막 프레임에 길게 멈추지 않는지 확인한다.
2. 요네 Q가 약 0.9초 lock 동안 멈춰 보이지 않고, Q 클립 후 run/idle로 자연 복귀하는지 확인한다.
3. 서버 판정, 쿨타임, Q/BA damage, FX cue 발생 횟수가 기존과 동일한지 확인한다.
