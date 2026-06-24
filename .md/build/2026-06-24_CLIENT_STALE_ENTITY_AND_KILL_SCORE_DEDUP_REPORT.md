# Client Stale Entity Assertion And Kill Score Dedup Report

Date: 2026-06-24
Plan: `.md/plan/2026-06-24_CLIENT_STALE_ENTITY_AND_KILL_SCORE_DEDUP_PLAN.md`

## 1. 작업 결과

### Client stale entity guard

반영 파일:
- `Client/Private/Network/Client/EventApplier.cpp`
- `Client/Private/Network/Client/SnapshotApplier.cpp`

결과:
- `ResolveLiveEntity`와 `DestroyEntityIfAlive`를 추가해 network event path에서 stale local entity를 직접 사용하지 않도록 정리했다.
- projectile spawn/hit, effect trigger, damage event, action start 경로의 net entity 조회가 live entity 확인을 통과해야만 component 접근 또는 파괴로 진행한다.
- snapshot stale cleanup과 `EnsureEntity`가 dead local mapping을 발견하면 mapping을 해제하고 재생성 경로로 복구한다.

### Server death credit one-shot

반영 파일:
- `Shared/GameSim/Components/RespawnComponent.h`
- `Shared/GameSim/Systems/Damage/DamageQueueSystem.cpp`
- `Server/Private/Game/GameRoom.cpp`

결과:
- `RespawnComponent.bDeathCredited`를 추가했다.
- `DamageQueueSystem`의 champion kill score/feed/reward 처리를 death episode당 1회로 제한했다.
- 서버 death/respawn phase에서 살아있거나 리스폰 완료된 champion은 다음 사망을 위해 credit flag를 reset한다.

### HUD authoritative score display

반영 파일:
- `Engine/Private/Manager/UI/UI_Manager.cpp`

결과:
- HUD team kill score는 `MatchScoreComponent`가 있으면 그 값을 우선 표시한다.
- local K/D/A는 local champion `ChampionScoreComponent` 값을 우선 표시한다.
- 기존 `m_GameContextHUD` 값은 snapshot score가 아직 없는 초기/fallback 상태에만 사용된다.

## 2. 검증 결과

### 공백 검증

Command:
```powershell
git diff --check
```

Result:
- PASS
- 기존 작업트리의 LF/CRLF 경고만 출력됨.

### Client Debug x64 build

Command:
```powershell
$msbuild = "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
& $msbuild Client\Include\Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m /v:minimal
```

Result:
- 첫 시도는 실행 중인 `Client/Bin/Debug/WintersGame.exe` 잠금으로 `LNK1104` 발생.
- 실행 중이던 `WintersGame.exe`와 `WintersServer.exe` 종료 후 재시도 PASS.
- 산출물: `Client/Bin/Debug/WintersGame.exe`

### Server Debug x64 build

Command:
```powershell
$msbuild = "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
& $msbuild Server\Include\Server.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m /v:minimal
```

Result:
- PASS
- 산출물: `Server/Bin/Debug/WintersServer.exe`

### LoLDataDriven 통합 검증

Command:
```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Tools\LoLData\Verify-LoLDataDrivenPipeline.ps1 -Configuration Debug
```

Result:
- 첫 시도는 Server build 내부 Engine PCH open에서 `Invalid argument`가 1회 발생.
- 실행 프로세스 종료 후 재시도 PASS.
- 통과 항목:
  - Definition pack freshness
  - Legacy ownership audit
  - Client visual timing parity, mismatchCount 0
  - Build `Shared/GameSim/Include/GameSim.vcxproj`
  - Build `Server/Include/Server.vcxproj`
  - Build `Client/Include/Client.vcxproj`
  - Build `Tools/SimLab/SimLab.vcxproj`
  - SimLab deterministic regression
  - Whitespace validation
- SimLab result:
  - same-seed replay OK: `67F2A97563B8DB04`
  - seed sensitivity OK: `5DA19645E291A29B`
  - PASS

## 3. 남은 확인

- 3-client runtime 재현은 이번 자동 검증에는 포함하지 않았다.
- 다음 수동 확인 포인트:
  - red-team Kindred client에서 `Entity.h:106 IsAlive(id)` assertion 재발 여부.
  - Jax bot이 Ashe bot을 처치할 때 kill feed가 사망 1회당 1회만 출력되는지.
  - HUD team score가 kill event 누적이 아니라 server snapshot score와 일치하는지.

