# Winters Atomic Decomposition Playbook — GameRoom 현재 아키텍처 기준

작성일: 2026-06-23 (현재 GameRoom 상태 기준 재작성)
북극성: `.md/plan/refactor/00_ESSENCE_BOUNDARY_REFACTOR.md` (한 파일 = 하나의 본질)
보존할 흐름: `Client Input -> GameCommand -> Server GameSim -> Snapshot/Event/FxCue -> Client Visual`
검증 레퍼런스: Server GameRoom — Stage 1 split(commit `1b2b214`‥`2c14a15`) + Stage 2 atom owner(commit `305ce1e` + 미커밋 WIP)

## 이 문서의 목적

북극성(`00_ESSENCE_BOUNDARY_REFACTOR.md`)은 "무엇을 어떤 owner로 나눌지"를 정한다. 이 문서는 그 분리를 **회귀 0으로 안전하게 수행하는 방법**을, 가장 멀리 간 적용처인 **GameRoom의 실제 현재 구조**를 레퍼런스로 고정한다.

GameRoom은 더 이상 거대 단일 파일이 아니다. 파일 단위로 쪼개졌고(Stage 1), 이제 상태/규칙이 원자 owner 클래스로 빠져나가는 중이다(Stage 2). 이 문서는 그 도달점을 정본 패턴으로 박아, 남은 God-file에 같은 규율로 적용하기 위한 것이다.

이 문서는 코드 변경 지시서가 아니다. 각 대상의 실제 적용 계획은 작업 직전 `.md/계획서작성규칙.md` 형식으로 따로 쓴다(`03_SERVER_GAMEROOM_ATOMIC_REFACTOR_PLAN.md`가 GameRoom Stage 2의 원본 계획). 여기서는 **방법론, 현재 owner 지도, 경계 가드레일, 검증 게이트, 남은 작업**을 고정한다.

## 검증된 2단계 분해 (GameRoom이 증명함)

분해는 두 단계로 나눈다. 한 번에 둘 다 하지 않는다.

### Stage 1 — 기계적 본질 분리 (verbatim move, 동작 불변) — GameRoom 완료

God-file을 책임별 `.cpp`로 쪼갠다. 함수 본문은 라인 단위 그대로 이동한다. public `.h` 선언은 건드리지 않고, 시그니처·동작·호출 순서를 바꾸지 않는다. 두 파일 이상이 공유하는 anonymous namespace 헬퍼는 `XxxInternal.h`로 승격한다. 이동과 로직 수정을 같은 커밋에 섞지 않는다.

GameRoom: `GameRoom.cpp` 6,077줄 → `GameRoomLobby`/`Nav`/`Spawn`/`MinionAI`/`Projectiles` + `GameRoomInternal.h/.cpp` + `GameRoomSmokeRoster.h/.cpp`로 분리(commit `1b2b214`‥`2c14a15`). 그 전부터 `GameRoomTick`/`Commands`/`Replication`/`ChampionAI`는 이미 분리돼 있었고, 이 작업이 그 패턴을 같은 규율로 연장했다.

### Stage 2 — 원자 owner 추출 (상태 소유권 이동) — GameRoom 진행 중

God-class를 orchestration shell로 줄이고, 상태와 규칙을 원자 owner 클래스로 추출한다. 추출하면서 **두 가지 owner 형태**가 나왔다. 둘 다 정본 패턴이다.

- **stateful authority**: 자기 slice의 상태를 소유하고, side effect를 직접 실행하지 않고 `XxxResult`를 반환한다. shell이 결과를 받아 packet send / bootstrap / replay stop을 실행한다.
  예: `CLobbyAuthority`(lobby 상태 + `LobbyAuthorityResult`), `CCommandIngress`(pending 큐), `CSessionBinding`(session→entity).
- **static authority**: 상태가 없는 순수 함수 집합. `CWorld`/navgrid 같은 입력을 인자로 받아 `XxxResult` 구조체를 반환한다. ECS 반복 glue는 호출하는 shell(GameRoom 멤버 파일)에 남는다.
  예: `CWalkabilityAuthority`, `CWorldBootstrap`, `CServerAICommandProducer`, `CServerProjectileAuthority`, `CReplicationEmitter`.

GameRoom 자신은 **orchestration shell + ECS 반복 glue + transport/리소스 소유자**로 남는다. navgrid 3종, replay recorder, world, session 목록은 GameRoom이 보유하고, 계산/규칙은 authority로 위임한다.

### 왜 단계를 나누나

Stage 1과 Stage 2를 한 번에 하면 회귀가 났을 때 원인이 "파일을 잘못 옮겼나"인지 "소유권을 잘못 옮겼나"인지 분리할 수 없다. Stage 1을 먼저 끝내면 diff가 순수 이동이라 회귀 원인이 좁혀지고, Stage 2의 owner 경계도 코드로 보인다. GameRoom이 이 순서로 회귀 없이 진행됐다.

## 현재 GameRoom 원자 owner 지도 (실측 2026-06-23)

`CGameRoom`(`GameRoom.h` 229줄)은 이제 owner들을 멤버로 들고 위임한다: `m_pLobbyAuthority`, `m_commandIngress`, `m_sessionBinding` + navgrid/world/replay 리소스. shell 본체 `GameRoom.cpp`는 403줄(Create/ctor/Start/Stop/`InitializeServerSimSystems`/`Phase_ServerDeathAndRespawn`/`DebugSetHealthByNetId`/tick 조립).

### 추출 완료 — committed (`305ce1e` "Refactor runtime atom ownership")

| owner | 파일(.h/.cpp) | 형태 | 소유/책임 | GameRoom 위임 지점 |
|---|---|---|---|---|
| `CLobbyAuthority` | `LobbyAuthority` 116/1,111 | stateful | lobby slot/phase/revision/host, slot rule, ready/start 판정 → `LobbyAuthorityResult` | `OnLobbyCommand`/`OnSessionJoin`/`OnSessionLeave` → `ApplyLobbyAuthorityResult`로 side effect |
| `CCommandIngress` | `CommandIngress` 46/142 | stateful | `PendingCommand` 큐 + mutex, `AcceptCommandBatch`/`EnqueueCommand`/`DrainSorted` | `Phase_DrainCommands`가 `DrainSorted` 소비 |
| `CSessionBinding` | `SessionBinding` 29/106 | stateful | session→entity 맵, `ResolveControlledEntity` | snapshot/event broadcast의 session 필터 |

`GameRoomLobby.cpp`는 이 추출로 1,139 → 388줄로 줄어 **transport adapter**(packet send / hello / game start broadcast)만 남았다. `03` 계획이 의도한 도달점.

### 추출 진행 중 — 미커밋 WIP (`git status ??`)

| owner | 파일(.h/.cpp) | 형태 | 소유/책임 |
|---|---|---|---|
| `CWalkabilityAuthority` | `WalkabilityAuthority` 146/631 | static | navgrid 경로/walkable 계산: `ResolveMoveTarget`/`BuildMovePath`/`IsWalkableXZ`/`TryClampMoveSegmentXZ` → rich Result 구조체. GameRoom의 `IWalkableQuery` impl이 navgrid 보유 + 위임 |
| `CWorldBootstrap` | `WorldBootstrap` 72/104 | static | `SpawnObjectDefinitionPack`에서 structure/jungle spawn **request** 빌드(데이터 주도), `BuildFallbackStructures` |
| `CServerAICommandProducer` | `ServerAICommandProducer` 26/56 | static | bot AI가 `GameCommand` 생산(서버 권위 보존), `ResolveInitialBotLane` |
| `CServerProjectileAuthority` | `ServerProjectileAuthority` 60/181 | static | projectile hit 판정 + spawn/hit event + turret/skill damage request 빌드 |
| `CReplicationEmitter` | `ReplicationEmitter` 33/107 | static | action-start/replicated event 수집·직렬화. GameRoom은 transport(session send/replay record)만 |
| `AttachChampionSimComponents` | `Factory/ChampionSimComponentTable` 8/54 | table | 챔피언별 sim component `if` 체인을 부착 테이블로 추출 |

`GameRoomReplication.cpp`는 이미 `CReplicationEmitter`에 위임하고 transport만 보유, `GameRoomNav.cpp`는 `CWalkabilityAuthority`에 계산을 위임하는 것을 코드로 확인했다(부분 적용).

### 아직 GameRoom 멤버 파일에 남은 glue (다음 thinning 대상)

authority 위임 후에도 ECS 반복 loop와 navgrid build가 멤버 파일에 남아 있다.

- `GameRoomMinionAI.cpp` 1,586줄 — 가장 큰 잔여. 미니언 이동/AI loop가 아직 멤버 메서드.
- `GameRoomNav.cpp` 913줄 — navgrid **build**(StageGameplayBounds 등) + `IWalkableQuery` impl(계산은 `CWalkabilityAuthority` 위임).
- `GameRoomSpawn.cpp` 823줄 — spawn glue(`CWorldBootstrap` request + `AttachChampionSimComponents` 위임, ECS Add는 GameRoom).
- `GameRoomProjectiles.cpp` 475줄 — projectile/turret phase loop(`CServerProjectileAuthority` 위임).

## 교차하는 두 북극성 트랙 (GameRoom에 이미 착지)

원자 분해와 같은 방향으로 두 트랙이 GameRoom에 들어와 있다. owner가 깨끗할수록 이 경계가 선명해진다.

1. **데이터 주도** (collab-pipeline `00`/refactor `09`): `GameplayDefinitionPack`이 `TickContext::pDefinitions`로 흐른다. `CStatSystem::Execute(world, definitions)`, respawn delay·structure·jungle 수치가 `SpawnObjectDefinitionPack`(=`SpawnLoadoutPolicyDef`/`StructureGameDef`/`JungleCampGameDef`)에서 온다. 소스: `Server/Private/Data/LoLGameplayDefinitionPack.h`, `ServerData::GetLoLGameplayDefinitionPack()`. 하드코딩 리터럴이 data pack으로 빠지는 중.
2. **Pose/Action 복제** (refactor `04`): `NetAnimationComponent`의 혼합이 `ReplicatedPose`/`ReplicatedAction`으로 갈렸다. `StartReplicatedAction`/`SetReplicatedPose` + `ePoseStateId`/`eActionStateId`가 서버 권위 경로에 반영됨.

이 둘은 별도 lane이지만 원자 owner 추출과 충돌하지 않는다. owner가 data pack을 읽고 pose/action을 쓰는 형태로 자연히 합류한다.

## 본질 판정 (북극성 00의 질문을 함수 묶음에 적용)

파일 안에서 함수 목록을 table-of-contents처럼 읽고, 각 묶음에 묻는다.

1. 서버가 판정하는 gameplay truth인가? → Server / Shared (authority 또는 GameSim system)
2. 이미 정해진 결과를 보여주는 presentation 해석인가? → Client
3. transport / adapter(packet send, session bind)인가? → shell
4. 사람이 조정하는 의도/수치인가? → Data pack (`GameplayDefinitionPack`)
5. authoring / editor 인가? debug / lab / tuner 인가?

답이 다르면 다른 owner로 쪼갠다. owner가 틀린 코드는 삭제가 아니라 이동한다. "한 코드가 두 가지 이유로 바뀐다면 아직 덜 쪼갠 것이다."

## GameRoom Stage 2 남은 작업

1. WIP owner 6종(`Walkability`/`WorldBootstrap`/`ServerAICommandProducer`/`ServerProjectileAuthority`/`ReplicationEmitter`/`ChampionSimComponentTable`) + `Server/Private/Data/`를 `Server.vcxproj`/`.filters`에 등록하고 게이트(빌드+스모크) 통과시켜 커밋.
2. `GameRoomMinionAI.cpp`(1,586) → 미니언 이동/타겟팅 loop를 `ServerMinionAuthority`(static) 형태로 추가 추출. 가장 큰 잔여이므로 우선.
3. `GameRoomNav.cpp`의 navgrid **build**(StageGameplayBounds 등)를 `CWalkabilityAuthority`(또는 별도 `NavGridBakery`)로 마저 이동, `IWalkableQuery` impl은 위임만 남기기.
4. data pack으로 더 빠질 하드코딩 잔량(스폰 collider/scale, AI 튜닝)을 `09`/collab-pipeline 트랙으로 넘김.

## 분해 대상 지도 — GameRoom 밖 God-file (동일 방법론)

GameRoom에서 검증된 2단계를 그대로 적용한다. 실측(ReadAllLines 2026-06-23):

| 파일 | 줄수 | domain | 1차 ROLE |
|---|---|---|---|
| `Client/Private/Scene/Scene_InGame.cpp` | ~~6,588~~ → 760 (Stage 1 완료) | Client | `CLIENT_SCENE` — Stage 1 끝(8 TU 분리), Stage 2 owner 추출은 `17`/`18` |
| `Engine/Private/Manager/UI/UI_Manager.cpp` | 4,546 | Engine | S7 lane (제품의미 분리, 아래) |
| `Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp` | 2,586 | Shared | `SHARED_SYSTEMS` |
| `Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp` | 2,555 | Shared | `SHARED_SYSTEMS` |
| `Engine/Private/RHI/DX12/DX12Device.cpp` | 2,006 | Engine | `ENGINE_RHI` |
| `Client/Private/Manager/Minion_Manager.cpp` | 1,670 | Client | `CLIENT_MANAGERS` |
| `Client/Private/Network/Client/SnapshotApplier.cpp` | 1,461 | Client | `CLIENT_SCENE`(2차) |
| `Engine/Private/RHI/DX11/CDX11Device.cpp` | 1,291 | Engine | `ENGINE_RHI` |
| `Client/Private/Network/Client/EventApplier.cpp` | 1,193 | Client | `CLIENT_SCENE`(2차) |

분리 대상 아님: `Client/Private/GameObject/ChampionTable.cpp`(149, 단일 책임), `ChampionSpawnService.cpp`(319, 데이터 주도 Factory 트랙). editor/tooling(`WfxEffectToolPanel.cpp`, `ChampionHUDPanel.cpp`)은 1차 wave 밖.

Codex 핸드오프 프롬프트와 ROLE 정의는 `.md/plan/refactor/14_CODEX_PROMPT_ATOMIC_DECOMPOSITION.md`. (GameRoom `SERVER_ATOMIZE`의 첫 슬라이스는 이제 "LobbyAuthority 신규"가 아니라 "WIP owner 커밋 + glue thinning"이다 — 현재 상태에 맞게 프롬프트도 갱신 필요.)

## 경계 가드레일 (북극성 의존 규칙 — 절대 위반 금지)

분리는 의존 방향을 새로 만들지 않는다.

- `Shared/GameSim`은 Engine/Client/Renderer/UI/ImGui/DX type을 include하지 않는다.
- `Server`는 Client visual에 의존하지 않는다. bot AI는 truth component를 직접 고치지 않고 **command를 생산**한다(`CServerAICommandProducer`가 이 경계를 지킨다).
- `Client`는 authoritative gameplay truth를 새로 만들지 않는다. local-only prediction/lab path는 이름으로 격리하고 snapshot-apply와 섞지 않는다.
- `Client/Public`/`Shared` 헤더에 `ID3D11*`/`IDXGI*` 노출을 넓히지 않는다. `IRHIDevice` 추상 유지.
- static authority는 mutable global singleton이 아니다. 입력을 인자로 받고 Result를 반환한다. data는 `GameplayDefinitionPack` read-only로 흐른다.
- 새 `.h/.cpp`는 프로젝트 전역 `/utf-8`에 의존(한글 주석 CP949 오판 방지).
- `.vcxproj`는 명시적 `ClCompile` 목록(glob 아님). 새 split/owner 파일 등록은 분해의 필수 단계(미등록 시 링크 단계에서 함수 증발). 그 외 XML 구조는 건드리지 않는다.
- Engine public header 변경 시 `EngineSDK/inc` sync(`UpdateLib.bat`). `EngineSDK/inc` 직접 수정 금지.

## Engine 제품의미 분리는 별도 lane (mechanical split 아님)

`UI_Manager.h`/`GameplayComponents.h`/`MinionAISystem.h`/`TurretAISystem.h`/`TurretProjectileSystem.h`/`BTNodes_Champion.h`/`FxMaterialDesc.h`/`GameInstance.h`의 LoL 제품의미(`eChampion`/Champion/Minion/Turret/HUD/shop/`LOL*`/`Elden*` enum)는 Engine public API로 leak되어 있다(컴퍼스 `00` "Engine 현재 분리 후보" 8종).

정리는 verbatim 분리가 아니라 **owner 이동(Engine → Shared/GameSim/Client)이며 의존 방향이 바뀐다**. 북극성 S2(Engine De-Productization)/S7(UI Ownership Split), `04` 계획의 dual-write 회귀방지로 접근. 리스크가 커서 별도 세션·`ENGINE_DEPRODUCT` ROLE로 다룬다.

## 검증 게이트 (모든 분해 공통)

- **G1 빌드**: 해당 target Debug x64 통과. (Engine 미커밋 변경으로 전체 리빌드가 깨질 때 `/p:BuildProjectReferences=false`로 해당 프로젝트만.)
- **G2 순수 이동/위임**: `git diff`가 이동·위임·등록만. 이동과 로직 수정 분리 커밋.
- **G3 경계**: forbidden-dependency 스캔 0.
- **G4 스모크 (F5, 서버 로그만으로 판정 금지)**: roster, map, minion, champion, snapshot, UI, FX 7개 시스템 미은닉.
- **G5 본질**: 분리 후 각 파일/owner가 한 가지 이유로만 바뀌는가.

## 권장 순서

```text
1. GameRoom Stage 2 마무리              WIP owner 6종 커밋 + MinionAI/Nav glue thinning (거의 완료)
2. Client Scene_InGame Stage 1 [완료]   8 TU 분리. Stage 2 owner 추출은 `17`/`18` (AMBIENT→…→PREDICTION)
3. Client managers Stage 1              Minion 우선, Structure/Jungle 후순위
4. Shared systems Stage 1               CommandExecutor / ChampionAISystem
5. Engine RHI Stage 1                   DX12Device / CDX11Device resource factory
6. Engine 제품의미 분리 (S2/S7)         별도 lane, 마지막
```

## 검증 명령

```powershell
git diff --stat
git diff --check

# Server 단독 (Engine 리빌드 회피)
& "C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" Server/Include/Server.vcxproj /m /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false

# Client 단독
& "C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" Client/Include/Client.vcxproj /m /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false

# 서버 스모크 (기동/틱/종료)
.\Server\Bin\Debug\WintersServer.exe --smoke-seconds=10

# forbidden dependency
rg -n "#include .*(Client|Renderer|UI|ImGui|d3d|DX)" Shared/GameSim Server
rg -n "#include .*Server" Client Shared/GameSim
```
