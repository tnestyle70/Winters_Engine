# Session - 2026-06-24 Viego Soul/R/E Stealth Fix Plan

## 목표

비에고가 사일러스를 처치한 뒤 사일러스가 반복 스폰되는 현상을 서버 권한 기준으로 차단하고, 비에고 R Impact와 E 은신 표현을 클라이언트 시각 레이어에서 튜닝한다.

본질 기준은 하나다.

> 소울은 챔피언 외형과 위치를 빌리는 시각/상호작용 엔티티일 수 있지만, 킬 점수/킬 피드/리스폰 권한을 가진 실제 챔피언은 아니다.

## 원인 판단

### 1. 사일러스 반복 스폰

- `ViegoSoulComponent` 엔티티가 `ChampionComponent`, `HealthComponent`, `TargetableTag`를 함께 가진다.
- 기존 사망 처리 흐름은 `ChampionComponent`가 붙은 사망 타겟을 실제 챔피언 사망처럼 점수, 킬 피드, 보상, 비에고 소울 생성 후보로 볼 수 있었다.
- 그래서 비에고가 만든 사일러스 소울이 다시 죽으면, 그 소울을 또 사일러스 챔피언 사망처럼 해석할 수 있다.
- 잭스 킬 메시지 반복과 같은 계열의 문제지만, 이번 케이스의 핵심은 `bool` 하나의 누락보다 `실제 챔피언`과 `챔피언 외형을 빌린 소울`의 카테고리 분리가 부족했던 것이다.

### 2. Viego R Impact의 검은 바닥감

- `r_impact.wfx`에 이미 비에고 R 리소스가 연결되어 있었지만, 어두운 fill/smoke 레이어가 강하고 실제 비에고 R 이미지 리소스의 원형 에너지 레이어가 부족했다.
- 이전 튜닝 과정에서 WFX 필드가 확장되며 일부 MeshParticle `scale` 값이 `0`으로 남아 충격파/검격 메시 레이어가 약해질 수 있는 상태도 확인했다.

### 3. Viego E 은신 표현

- 서버 GameSim은 이미 E 미스트 범위 안의 비에고에게 `Invisible` gameplay state를 부여하고, snapshot에는 `kSnapshotStateInvisibleFlag`로 복제한다.
- 따라서 새 gameplay truth를 만들 필요는 없다.
- 클라이언트는 복제된 invisible flag를 보고 비에고 모델 표현과 화면 오버레이만 바꾸면 된다.

## 1. 반영해야 하는 코드

### 서버/GameSim - 킬 권한 분리

- `Shared/GameSim/Systems/Damage/DamageQueueSystem.cpp`
  - `TryMarkChampionDeathCredit`에서 `ViegoSoulComponent` 타겟은 킬 점수/킬 피드/보상 대상에서 제외한다.
  - 기존 `RespawnComponent::bDeathCredited` one-shot 흐름은 실제 리스폰 가능한 챔피언에게만 의미를 갖게 유지한다.

- `Shared/GameSim/Champions/Viego/ViegoGameSim.cpp`
  - `TrySpawnSoulForKill`은 `ViegoSoulComponent`가 붙은 타겟을 소울 생성 후보로 보지 않는다.
  - 소울 자신의 `HealthComponent`가 죽은 상태가 되면 lifetime 만료를 기다리지 않고 제거 후보에 넣는다.

### 서버 Snapshot - 소울을 실제 챔피언으로 내보내지 않기

- `Server/Private/Game/SnapshotBuilder.cpp`
  - `ViegoSoulComponent` 엔티티는 `EntityKind::Champion`이 아니라 `EntityKind::EffectAnchor`로 전송한다.
  - 소울 champion id/subtype과 `kSnapshotStateViegoSoulFlag`는 유지해서 클라이언트가 외형 연출만 복구할 수 있게 한다.

### 클라이언트 Snapshot - 소울 시각 엔티티 수명 정리

- `Client/Private/Network/Client/SnapshotApplier.cpp`
  - Viego soul 판정은 `EntityKind::Champion`이 아니라 `kSnapshotStateViegoSoulFlag`를 기준으로 한다.
  - stale cleanup에서 minion뿐 아니라 `ViegoSoulComponent`도 서버 snapshot에서 사라지면 제거한다.
  - 죽은 entity가 net id에 남아 있으면 unbind 후 새 snapshot을 안전하게 받을 수 있게 한다.

### 클라이언트 Render - E 은신 표현

- `Client/Private/Scene/Scene_InGameRender.cpp`
  - 로컬/동일 팀 렌더링 경로에서 비에고가 `kSnapshotStateInvisibleFlag`를 가진 경우 모델 material override를 적용한다.
  - 로컬 플레이어가 비에고이고 invisible flag를 가진 경우 fullscreen gray-green raw image overlay를 그린다.
  - 이 처리는 서버 판정이 아니라 복제 상태 기반의 presentation이다.

- `Client/Public/Scene/Scene_InGame.h`
  - `RenderViegoMistScreenOverlay` 선언을 추가한다.

### FX Data - Viego R Impact

- `Data/LoL/FX/Champions/Viego/r_impact.wfx`
  - 어두운 fill smoke alpha를 낮춘다.
  - `viego_base_r_decal_radiusenergies.png`, `viego_base_r_energybase.png` 기반의 원형 additive ground decal을 추가한다.
  - 기존 R MeshParticle scale을 원본 리소스 기준으로 복구한다.

## 2. 검증

### 자동 검증

- `git diff --check`
- `Data/LoL/FX/Champions/Viego/r_impact.wfx` JSON parse
- `Server/Include/Server.vcxproj` Debug x64 build
- `Client/Include/Client.vcxproj` Debug x64 build
- `Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1 -Configuration Debug`

### 수동 인게임 확인 기준

- 비에고가 사일러스 처치 후 생성된 소울을 다시 처치해도 사일러스 챔피언이 반복 스폰되지 않는다.
- 소울 사망은 킬 피드/킬 스코어를 증가시키지 않는다.
- Viego R Impact는 검은 바닥보다 원형 청록 에너지 문양과 충격파가 먼저 읽힌다.
- 비에고가 E 범위 내부에서 invisible flag를 받을 때 모델이 은신 톤으로 바뀌고, 로컬 비에고 화면에는 반투명 회색/청록 veil이 깔린다.

## 리스크와 후속 판단

- 현재 모델 표현은 기존 `ModelRenderer::SetMaterialOverrideColor` 경로를 사용한다. 실제 skinned mesh의 완전한 0.5 alpha blending은 엔진 material/render state 지원 범위에 따라 추가 작업이 필요할 수 있다.
- 이번 수정의 본질은 gameplay truth를 늘리지 않고, 서버가 이미 복제하는 invisible flag를 presentation에서 소비하는 것이다.
