Session - Yone Q movement lock must block move input and run animation.

## 1. 반영해야 하는 코드

### 목표

요네 Q 시전 lock 구간에서는 우클릭 이동 입력을 받아도 실제 이동, MoveTarget 생성, 서버 MovingFlag, 클라이언트 Run 애니메이션 예측이 모두 발생하지 않아야 한다.

### 원인

- `Shared/GameSim/Systems/Move/MoveSystem.cpp`는 이미 action lock 중 실제 위치 이동을 막는다.
- 하지만 `Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`의 `HandleMove`는 action lock을 검사하지 않고 `MoveTargetComponent`를 만들 수 있었다.
- `Server/Private/Game/SnapshotBuilder.cpp`는 `MoveTargetComponent.bHasTarget`만 보면 `kSnapshotStateMovingFlag`를 세웠다.
- `Client/Private/Scene/Scene_InGameLocalSkills.cpp`의 네트워크 이동 입력 경로는 `(bNetworkActive || !bActionLocked)` 조건 때문에 네트워크 모드에서 skill lock 중에도 이동 입력과 Run 예측을 허용했다.
- `Client/Private/Scene/Scene_InGameNetwork.cpp`는 서버 action 재생 중에도 이전 위치 이동 grace 또는 MovingFlag가 남으면 기본 Run 애니메이션으로 내려갈 수 있었다.

### 본질 규칙

`SkillQ/W/E/R` action lock 중 이동 입력은 이동 의도가 아니라 무시되어야 하는 입력이다.

- 서버 truth: lock tick 안에서는 새 MoveTarget을 만들지 않는다.
- 스냅샷: lock tick 안에서는 MoveTarget이 남아 있어도 MovingFlag를 내보내지 않는다.
- 클라이언트 presentation: network skill lock 중에는 이동 예측 Run을 재생하지 않는다.
- 예외: BasicAttack move cancel/queue와 Kalista passive dash move queue는 기존 별도 정책을 유지한다.

### 반영 파일

`Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`

- `SkillQ/W/E/R` action lock 확인 helper 추가.
- `HandleMove`에서 Kalista passive dash queue를 먼저 소비한 뒤, skill action lock 중이면 `MoveTarget`을 비우고 반환.
- BasicAttack `CombatActionComponent` move policy는 기존 경로 유지.

`Server/Private/Game/SnapshotBuilder.cpp`

- snapshot build 시 `SkillQ/W/E/R` lock tick 안이면 `MoveTargetComponent.bHasTarget`이 있어도 `kSnapshotStateMovingFlag`를 세우지 않음.

`Client/Public/Scene/Scene_InGame.h`

- 서버 응답 전 입력 누수를 막기 위한 `m_fNetworkMoveInputLockTimer` 추가.
- network move input lock helper 선언.

`Client/Private/Scene/Scene_InGame.cpp`

- frame update에서 `m_fNetworkMoveInputLockTimer` 감소.

`Client/Private/Scene/Scene_InGameLocalSkills.cpp`

- network skill command 발행 직후 skill lock duration만큼 이동 입력 잠금.
- `IssuePlayerMoveTarget`와 `UpdatePlayerControl`에서 network skill lock 중 이동 입력과 Run 예측 차단.
- Kalista passive dash move queue는 예외로 유지.

`Client/Private/Scene/Scene_InGameNetwork.cpp`

- network action animation state가 skill action을 처리 중이면 이동 grace와 MovingFlag를 무시하고 기본 Run 전환을 막음.

## 2. 검증

필수 검증:

1. `python Tools/LoLData/Build-LoLDefinitionPack.py --check`
2. `git diff --check`
3. `MSBuild Server/Include/Server.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal`
4. `MSBuild Client/Include/Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal`

수동 확인:

1. 요네 Q 직후 lock 구간에서 우클릭해도 캐릭터가 제자리 Run으로 전환되지 않는지 확인.
2. Q lock 종료 후 우클릭하면 정상 이동/Run으로 전환되는지 확인.
3. 기본 공격 이동 cancel/queue와 칼리스타 패시브 대시 입력이 같이 막히지 않는지 확인.
