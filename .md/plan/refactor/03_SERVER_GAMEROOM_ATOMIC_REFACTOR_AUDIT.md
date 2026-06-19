# Server GameRoom Atomic Refactor Audit

결론: 아직 본질만 남긴 상태가 아니다.

현재 `CGameRoom`은 `AuthorityExecution`의 얇은 shell이 아니라 여러 원자가 붙은 서버 축소판이다. `GameRoom*.cpp` 파일이 이미 나뉘어 있어도, 대부분이 여전히 `CGameRoom::` 멤버 함수와 `CGameRoom` 멤버 상태를 공유한다. 이것은 파일 분할이지 원자 분리는 아니다.

## 검증 기준

Server의 북극성:

```text
Client Input
-> GameCommand
-> Server AuthorityExecution
-> Shared/GameSim GameplayTruth
-> Snapshot/Event/Cue
-> Client Presentation
```

`CGameRoom`에 남길 수 있는 본질:

```text
RoomLifecycle
AuthorityOrchestration
```

`CGameRoom`에서 의심해야 하는 것:
- gameplay rule 직접 구현
- lobby rule 직접 구현
- transport packet build 직접 구현
- replay evidence 직접 저장
- nav/path file discovery 직접 구현
- bot/minion/projectile rule 직접 구현
- debug mutation 직접 구현

## 기존 코드 검증 결과

### GameRoom.h

위반:
- `PendingCommand`, `eRoomPhase`, `LobbySlotState`가 public header에 노출된다.
- `CWorld`, `EntityIdMap`, `DeterministicRng`, command executor, snapshot builder, lag compensation, replay recorder, navgrid, spatial system, turret AI, lobby state, command queue, session map이 한 class에 모여 있다.
- private 함수 목록이 tick, lobby, command, spawn, nav, minion, projectile, replication, replay, debug를 모두 포함한다.

판정:

```text
CGameRoom != 더 나눌 수 없는 원자
```

### GameRoomLobby.cpp

위반:
- lobby rule과 session transport가 같은 파일에 있다.
- `BroadcastLobbyStateLocked`, `SendHelloToSessionLocked`, `BroadcastGameStartLocked`가 flatbuffers, packet envelope, `CSession_Manager`를 직접 다룬다.
- `TryStartGame`이 lobby permission 판단과 `SpawnChampionsFromLobby`, `SpawnServerGameplayObjects`, hello send를 함께 수행한다.
- `TryStopReplay`가 lobby command와 replay side effect를 함께 다룬다.

판정:

```text
LobbyAuthority + TransportAdapter + WorldBootstrap trigger + ReplayControl 이 섞여 있다.
```

### GameRoomCommands.cpp

위반:
- command queue와 session sequence accept는 `CommandIngress`다.
- `ResolveControlledEntityForSession`은 lobby slot과 session entity binding을 함께 읽는다.
- pending command drain이 lobby state, session binding, entity map을 직접 의존한다.

판정:

```text
CommandIngress + SessionBinding 이 섞여 있다.
```

### GameRoomTick.cpp

위반:
- tick thread, tick clock, command phase, GameSim system order, champion-specific tick, server local phases가 한 파일에 있다.
- `Phase_SimulationSystems`는 Shared/GameSim system pipeline과 Server-only phases를 한 함수에 묶는다.

판정:

```text
RoomClock + GameSimPipeline + ServerLocalSystems 가 섞여 있다.
```

### GameRoom.cpp

위반:
- lifecycle, system initialization, champion hook registration, death/respawn gameplay rule, debug HP mutation이 함께 있다.
- `Phase_ServerDeathAndRespawn`은 `HealthComponent`, `RespawnComponent`, `ChampionAIComponent`, `MoveTargetComponent`, `SkillStateComponent`, `TransformComponent`를 직접 변경한다.
- `DebugSetHealthByNetId`는 명시적 debug gate 없이 gameplay truth를 직접 바꾼다.

판정:

```text
RoomLifecycle + GameSimRegistration + RespawnRule + DebugAuthorityGate 가 섞여 있다.
```

### GameRoomReplication.cpp

위반:
- event serialization, network send, snapshot build, replay record가 함께 있다.
- replay는 검증 증거인데 replication transport와 같은 함수 흐름에 붙어 있다.

판정:

```text
ReplicationEmitter + ReplayEvidence 가 섞여 있다.
```

### GameRoomNav.cpp

위반:
- path discovery, authored navgrid load, wmesh fallback, walkable query, movement clamp, minion waypoint cache가 함께 있다.
- `Client/Bin/Debug*/Resource` fallback 후보가 남아 있어 runtime resource 기준과 충돌할 수 있다.

판정:

```text
ServerResourceResolve + WalkabilityAuthority + MinionPathSupport 가 섞여 있다.
```

### GameRoomSpawn.cpp

위반:
- stage load 결과 해석, champion bootstrap, structure spawn, jungle spawn, minion spawn primitive, hardcoded HP/stat/radius/default가 함께 있다.
- 일부 수치가 `GameDesignSource`인지 `Shared/GameSim` runtime default인지 불명확하다.

판정:

```text
WorldBootstrap + GameDesignDefault + RuntimeEntityFactory 가 섞여 있다.
```

### GameRoomMinionAI.cpp

위반:
- minion AI가 command producer인지 world mutation system인지 경계가 흐리다.
- transform, health, projectile spawn, animation component를 직접 다룬다.

판정:

```text
ServerAICommandProducer 라고 부르기에는 아직 truth mutation이 많다.
```

### GameRoomProjectiles.cpp

위반:
- turret projectile, skill projectile, hit resolve, entity destroy, net entity issue, replicated event trigger가 함께 있다.

판정:

```text
ProjectileAuthority + ReplicationCue + NetEntityBinding 이 섞여 있다.
```

## 계획서 자체 재검토

첫 계획의 `CLobbyAuthorityCallbacks`는 본질이 아니었다.

문제:
- `BroadcastLobbyState`
- `BroadcastGameStart`
- `SendGameStart`
- `SendHello`
- `SpawnGame`
- `BeginInGame`
- `StopReplay`
- `Trace`

이 callback들을 `CLobbyAuthority`가 저장하면 `CLobbyAuthority`가 다시 작은 `CGameRoom`이 된다.

수정 방향:

```text
CLobbyAuthority = lobby state transition only
CGameRoom = side effect adapter
```

`CLobbyAuthority`가 해야 하는 일:
- lobby phase 판단
- slot state 변경
- ready/all-ready 판단
- champion select/start permission 판단
- 결과를 `LobbyAuthorityResult`로 반환

`CLobbyAuthority`가 하지 않는 일:
- packet send
- flatbuffers build
- session manager lookup
- world spawn
- replay stop
- tick start
- command execution
- snapshot/event emission

## 더 본질적인 첫 칼질

첫 구현은 `LobbyAuthority` 전체 추출이 아니라 아래 순서여야 한다.

```text
1. LobbySlotState/eRoomPhase를 lobby type으로 격리한다.
2. lobby rule을 side effect 없는 state transition으로 만든다.
3. CGameRoomLobby.cpp는 LobbyAuthorityResult를 transport/world side effect로 번역한다.
4. tick, command, replication, nav, spawn, minion, projectile는 건드리지 않는다.
```

첫 세션 완료 기준:
- `CLobbyAuthority`가 `Network/Session`, `PacketDispatcher`, `flatbuffers`, `CWorld`, `ICommandExecutor`, `CReplayRecorder`를 include하지 않는다.
- `CLobbyAuthority`는 packet을 보내지 않는다.
- `CLobbyAuthority`는 world를 spawn하지 않는다.
- `CLobbyAuthority`는 replay를 저장하거나 중단하지 않는다.
- `CGameRoom` public API는 유지된다.
- lobby behavior는 기존과 같아야 한다.

## 의심을 유지할 질문

새 class를 만들 때마다 묻는다.

```text
이 class가 side effect를 실행하는가?
이 class가 두 개 이상의 원자 state를 가진가?
이 class가 Network와 World를 동시에 아는가?
이 class가 GameSim truth와 transport를 동시에 아는가?
이 class가 Data/Design 기본값을 코드에 새로 박는가?
이 class가 build 편의를 이유로 owner를 흐리는가?
```

하나라도 `예`면 아직 덜 나눈 것이다.
