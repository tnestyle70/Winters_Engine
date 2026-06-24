# Session - 2026-06-24 Viego Soul/R/E Stealth Fix Report

## 요약

비에고가 사일러스를 처치한 뒤 사일러스가 반복 스폰되는 문제는 `ViegoSoulComponent` 엔티티가 챔피언 외형과 체력 컴포넌트를 가진 상태에서 실제 챔피언 사망처럼 처리될 수 있어서 발생했다.

이번 수정은 소울의 본질을 명확히 분리했다.

> Viego soul은 champion visual/profile을 빌릴 수 있지만, kill credit, respawn, score, kill feed, new soul spawn의 authority target이 아니다.

추가로 Viego R Impact는 검은 바닥감을 줄이고 원형 에너지 리소스를 강화했으며, Viego E 은신 표현은 서버가 복제하는 invisible flag를 클라이언트 presentation에서 소비하도록 반영했다.

## 변경 내용

### 1. 소울 반복 스폰/킬 처리 차단

- `Shared/GameSim/Systems/Damage/DamageQueueSystem.cpp`
  - `TryMarkChampionDeathCredit`에서 `ViegoSoulComponent` 타겟은 kill credit 대상에서 제외했다.
  - 실제 리스폰 가능한 챔피언은 기존 `RespawnComponent::bDeathCredited` one-shot 흐름을 유지한다.

- `Shared/GameSim/Champions/Viego/ViegoGameSim.cpp`
  - `TrySpawnSoulForKill`이 `ViegoSoulComponent` 타겟을 무시하도록 막았다.
  - 소울이 죽으면 lifetime을 기다리지 않고 제거 후보에 넣는다.

- `Server/Private/Game/SnapshotBuilder.cpp`
  - Viego soul snapshot kind를 `Champion`에서 `EffectAnchor`로 바꿨다.
  - champion id/subtype과 soul flag는 유지해서 클라이언트가 시각 표현을 복구할 수 있게 했다.

- `Client/Private/Network/Client/SnapshotApplier.cpp`
  - Viego soul 판정은 snapshot kind가 아니라 `kSnapshotStateViegoSoulFlag` 기준으로 바꿨다.
  - stale cleanup에서 `ViegoSoulComponent` 엔티티도 제거하도록 정리했다.

### 2. Viego R Impact 원형 이펙트 강화

- `Data/LoL/FX/Champions/Viego/r_impact.wfx`
  - `r_fill_smoke` alpha를 낮춰 검은 바닥감을 줄였다.
  - `r_radius_energy_ring`, `r_energybase_circle` additive ground decal을 추가했다.
  - 기존 R MeshParticle scale이 `0`으로 남아 있던 값을 원본 리소스 기준으로 복구했다.

### 3. Viego E 은신 표현

- `Client/Private/Scene/Scene_InGameRender.cpp`
  - Viego가 `kSnapshotStateInvisibleFlag`를 가진 경우 모델 material override를 적용한다.
  - 로컬 플레이어가 Viego invisible 상태이면 fullscreen gray-green overlay를 그린다.

- `Client/Public/Scene/Scene_InGame.h`
  - `RenderViegoMistScreenOverlay` 선언을 추가했다.

## 검증 결과

### 통과

- `git diff --check`
  - 통과. 기존 CRLF 변환 경고만 출력됨.

- `python Tools/LoLData/Build-LoLDefinitionPack.py --check`
  - 통과.
  - Definition pack: `0x42EA0952`
  - Champions: `17`, skills: `85`, summoner spells: `1`

- `Data/LoL/FX/Champions/Viego/r_impact.wfx` JSON parse
  - 통과.
  - MeshParticle scale 확인:
    - `r_cast_swipe_mesh`: `0.0240`
    - `r_impact_swipe_mesh`: `0.0240`
    - `r_shockwave_mesh`: `0.0330`
    - `r_explosion_mesh`: `0.0320`

- `Server/Include/Server.vcxproj` Debug x64 build
  - 통과.
  - 첫 시도는 `WintersEngine.pdb` `LNK1201`로 실패했다.
  - 원인은 실행 중인 `WintersServer.exe`/빌드 PDB 잠금 상태로 판단했다.
  - 서버 프로세스 정리 후 `/m:1 /nr:false`로 재시도하여 통과했다.

- `Client/Include/Client.vcxproj` Debug x64 build
  - 통과.
  - `/m:1 /nr:false`로 PDB 잠금 변수를 줄여 빌드했다.

- `Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1 -Configuration Debug`
  - 통과.
  - SimLab deterministic regression PASS.
  - same-seed replay hash: `67F2A97563B8DB04`
  - seed+1 hash: `5DA19645E291A29B`

## 수동 확인 필요

- 비에고가 사일러스 처치 후 생성된 soul을 다시 처치해도 사일러스 챔피언이 반복 스폰되지 않는지 확인.
- soul 사망이 킬 피드/킬 스코어를 증가시키지 않는지 확인.
- R Impact에서 검은 바닥보다 청록 원형 에너지와 충격파가 먼저 읽히는지 확인.
- E 범위 내부에서 비에고 모델이 은신 톤으로 보이고, 로컬 비에고 화면에 회색/청록 veil이 자연스럽게 보이는지 확인.

## 남은 리스크

- 현재 E 은신 모델 처리는 기존 material override 경로를 사용한다. 엔진의 skinned mesh alpha blending 경로가 완전한 반투명 렌더링을 보장하지 않으면, 다음 단계에서 material/render state 수준의 alpha blend 지원을 추가해야 한다.
- 이번 수정은 gameplay truth를 늘리지 않았다. 서버 authoritative invisible flag와 soul flag를 클라이언트 presentation에서 소비하는 방식으로 제한했다.
