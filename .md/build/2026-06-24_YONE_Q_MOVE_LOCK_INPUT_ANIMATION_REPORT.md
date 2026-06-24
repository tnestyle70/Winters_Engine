# Yone Q Move Lock Input Animation Report

Date: 2026-06-24
Plan: `.md/plan/2026-06-24_YONE_Q_MOVE_LOCK_INPUT_ANIMATION_PLAN.md`

## 1. 작업 결과

### 원인 판정

요네 Q 시전 중 실제 이동이 막히는 것은 기존 서버 `MoveSystem` 기준으로 맞았다. 문제는 이동 금지의 원자가 `실제 위치 이동`에만 걸려 있고, `이동 입력 수락`, `MoveTarget 생성`, `Snapshot MovingFlag`, `클라이언트 Run 예측`에는 같은 규칙이 완전히 닫혀 있지 않았다는 점이다.

검증한 누수 지점:

- `Shared/GameSim/Systems/Move/MoveSystem.cpp`
  - 이미 `SkillQ/W/E/R` action lock tick 동안 실제 위치 이동을 막고 있었다.
- `Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`
  - 기존 `HandleMove`는 skill action lock을 검사하지 않아 lock 중에도 `MoveTargetComponent`가 만들어질 수 있었다.
- `Server/Private/Game/SnapshotBuilder.cpp`
  - 기존 snapshot은 `MoveTargetComponent.bHasTarget`만 보고 `kSnapshotStateMovingFlag`를 세울 수 있었다.
- `Client/Private/Scene/Scene_InGameLocalSkills.cpp`
  - 네트워크 모드에서는 `(bNetworkActive || !bActionLocked)` 조건 때문에 lock 중 우클릭이 `IssuePlayerMoveTarget`까지 들어갈 수 있었다.
- `Client/Private/Scene/Scene_InGameNetwork.cpp`
  - 서버 action 재생 중에도 이전 이동 grace나 MovingFlag가 남으면 기본 Run 애니메이션으로 전환될 수 있었다.

### 본질 규칙

Winters의 북극성은 `Client Input -> GameCommand -> Server GameSim -> Snapshot/Event -> Client Visual`이다. 이번 버그의 나눌 수 없는 원자 단위는 `Skill action lock 중 이동 입력은 이동 의도가 아니라 무시해야 하는 입력`이다.

그래서 동일 규칙을 네 층에 맞췄다.

- 서버 명령: lock tick 안에서는 새 `MoveTarget`을 만들지 않는다.
- 서버 스냅샷: lock tick 안에서는 `MoveTarget`이 남아 있어도 `MovingFlag`를 내보내지 않는다.
- 클라이언트 입력: 네트워크 skill command 직후부터 lock 시간 동안 우클릭 이동 입력을 받지 않는다.
- 클라이언트 시각화: skill action 재생 중에는 이동 grace와 MovingFlag가 Run 전환을 일으키지 못한다.

예외는 기존 정책을 유지했다.

- BasicAttack은 move cancel/queue 정책이 별도로 있으므로 skill lock 입력 잠금에서 제외했다.
- Kalista passive dash move queue는 기존 입력 예외를 유지했다.

### 코드 반영

반영 파일:

- `Client/Public/Scene/Scene_InGame.h`
  - `m_fNetworkMoveInputLockTimer`
  - `IsPlayerNetworkMoveInputLocked`
  - `ArmNetworkMoveInputLock`
- `Client/Private/Scene/Scene_InGame.cpp`
  - 프레임 업데이트에서 network move input lock timer 감소.
- `Client/Private/Scene/Scene_InGameLocalSkills.cpp`
  - 네트워크 skill command 발행 직후 lock duration만큼 이동 입력 잠금.
  - `IssuePlayerMoveTarget`에서 lock 중 이동 명령 발행 차단.
  - `UpdatePlayerControl`에서 lock 중 Run 예측 진입 차단.
- `Client/Private/Scene/Scene_InGameNetwork.cpp`
  - replicated skill action 처리 중 이동 grace와 `MovingFlag`를 무시해 기본 Run 전환 차단.
- `Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`
  - `HandleMove`에서 `SkillQ/W/E/R` lock tick 안이면 `MoveTarget`을 비우고 반환.
- `Server/Private/Game/SnapshotBuilder.cpp`
  - `SkillQ/W/E/R` lock tick 안이면 snapshot `kSnapshotStateMovingFlag`를 세우지 않음.
- `Data/LoL/FX/Champions/Yone/q_mortal_steel.wfx`
  - 이전 요청의 요네 Q emitter yaw `1.5708` 적용 상태 유지 및 확인.

## 2. 검증 결과

### Definition Pack

Command:

```powershell
python Tools/LoLData/Build-LoLDefinitionPack.py --check
```

Result:

- PASS
- Definition pack: `0x07D4F0AA`
- Champions: 17, skills: 85, summoner spells: 1

### Whitespace

Command:

```powershell
git diff --check
```

Result:

- PASS
- whitespace error 없음.
- LF/CRLF 변환 경고만 출력됨.

### Server Debug x64 Build

Command:

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Server/Include/Server.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
```

Result:

- PASS
- Output: `Server/Bin/Debug/WintersServer.exe`
- 첫 빌드에서 `SnapshotBuilder.cpp`의 직접 `ChampionGameDataDB` 참조가 include 경계 밖이라 실패했고, 기존 `ChampionRuntimeDefaults.h` 래퍼 `GetDefaultChampionSkillActionLockTicks`로 교체한 뒤 통과했다.
- 기존 DLL interface warning 및 일부 legacy encoding warning은 남아 있으나 이번 변경 파일은 컴파일/링크를 통과했다.

### Client Debug x64 Build

Command:

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Client/Include/Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
```

Result:

- PASS
- Output: `Client/Bin/Debug/WintersGame.exe`
- 기존 DLL interface warning 및 legacy encoding warning은 남아 있으나 이번 변경 파일은 컴파일/링크를 통과했다.

## 3. 수동 확인 항목

1. 네트워크 게임에서 요네 Q 직후 lock 구간 동안 우클릭해도 제자리 Run으로 전환되지 않는지 확인한다.
2. Q lock 종료 후 우클릭하면 정상적으로 이동과 Run 애니메이션이 시작되는지 확인한다.
3. BasicAttack 입력 중 이동 cancel/queue가 기존처럼 동작하는지 확인한다.
4. Kalista passive dash move queue가 이번 skill lock 입력 차단에 막히지 않는지 확인한다.
