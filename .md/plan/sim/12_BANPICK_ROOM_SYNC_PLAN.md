# BanPick 로비 동기화 계획

> 날짜: 2026-05-06  
> 목표: 리그오브레전드 사용자 설정 게임처럼 BanPick 씬에서 플레이어 진영 선택, 봇 슬롯 설정, 챔피언 선택, 게임 시작 동기화를 끝낸 뒤 InGame으로 진입한다. 현재 발생한 `Hello -> Ezreal 강제 바인딩` 버그도 이 계획 안에서 함께 제거한다.

---

## 0. 현재 코드 기준 사실

### 클라이언트

- `CScene_BanPick`은 현재 로컬 전용 챔피언 선택 씬이다.
  - `OnEnter()`에서 `GameContext.SelectedChampion`을 초기화한다.
  - 챔피언 버튼을 누르면 `SelectedChampion`을 바로 세팅하고 `CScene_MatchLoading`으로 넘어간다.
  - 파일: `Client/Private/Scene/Scene_BanPick.cpp:15-83`
- `GameContext`는 현재 `SelectedChampion` 하나만 들고 있다.
  - 파일: `Engine/Include/GameContext.h:26-29`
- `CScene_InGame`은 자기 안에서 `CClientNetwork`를 새로 만들고 `127.0.0.1:9000`에 자동 접속한다.
  - 파일: `Client/Private/Scene/Scene_InGame.cpp:249-310`
- `Hello`를 받으면 InGame이 `m_PlayerEntity`를 서버 net entity로 바꾸고 `BindPlayerToECSChampion`을 호출한다.
  - 파일: `Client/Private/Scene/Scene_InGame.cpp:278-298`
- 플레이어 스킬/애니메이션 조회는 아직 실제 entity의 `ChampionComponent.id`가 아니라 `GameContext.SelectedChampion`을 읽는 경로가 있다.
  - `DispatchSkillInput`: `Client/Private/Scene/Scene_InGame.cpp:2958`
  - `ApplyLocalPrediction`: `Client/Private/Scene/Scene_InGame.cpp:3162`

### 서버

- 서버는 TCP accept 시점에 바로 게임 플레이 챔피언을 만든다.
  - `CIOCPCore::WorkerLoop`가 `g_pRoom->OnSessionJoin(...)`를 호출한다.
  - 파일: `Server/Private/Network/IOCPCore.cpp:201-213`
- `CGameRoom::OnSessionJoin`은 즉시 `SpawnChampion(sessionId)`를 호출하고 net id를 발급한 뒤 `Hello`를 보낸다.
  - 파일: `Server/Private/Game/GameRoom.cpp:267-317`
- 서버 `Hello`는 챔피언을 항상 `eChampion::EZREAL`로 보낸다.
  - 파일: `Server/Private/Game/GameRoom.cpp:291-298`
- `SpawnChampion`도 `StatComponent.championId`와 `ChampionComponent.id`를 `EZREAL`로 하드코딩한다.
  - 파일: `Server/Private/Game/GameRoom.cpp:355-403`

### 프로토콜

- `PacketEnvelope`는 현재 게임플레이 중심 패킷만 가진다.
  - `CommandBatch`, `Snapshot`, `Event`, `Hello`, `Heartbeat`, `Disconnect`
  - 파일: `Shared/Network/PacketEnvelope.h:13-22`
- `Hello.fbs`는 이미 `championId`와 `team` 필드를 가지고 있지만, 서버가 값을 고정해서 보내고 있다.
  - 파일: `Shared/Schemas/Hello.fbs:3-10`

---

## 1. 목표 플레이 흐름

### 핵심 원칙

- BanPick은 단순 로컬 선택 UI가 아니라 서버 동기화 로비다.
- 서버가 최종 roster를 확정하기 전까지 InGame은 시작하지 않는다.
- 빈 슬롯은 UI상 봇 후보로 보이고, 사람이 들어와 선택하면 해당 슬롯이 Human으로 바뀐다.
- 게임 시작 시점에 남아 있는 빈 슬롯은 봇으로 확정된다.
- TCP/UDP와 무관하게 `sessionId`, `slotId`, `netId`, `team`, `championId`는 서버가 확정한다.

### 클라이언트 3개 예시

초기 로비:

```text
Blue 0: Empty/BotCandidate
Blue 1: Empty/BotCandidate
Blue 2: Empty/BotCandidate
Blue 3: Empty/BotCandidate
Blue 4: Empty/BotCandidate
Red  0: Empty/BotCandidate
Red  1: Empty/BotCandidate
Red  2: Empty/BotCandidate
Red  3: Empty/BotCandidate
Red  4: Empty/BotCandidate
```

클라이언트 1 접속 후 Blue 0 선택:

```text
Blue 0: Human Client1
나머지 9칸: Empty/BotCandidate
```

UI에서는 나머지 9칸을 봇처럼 설정할 수 있다. 단, 이 시점에 서버가 게임 플레이 봇 9명을 즉시 스폰하는 것은 아니다. 로비 상태에서 `봇 후보 슬롯`으로 관리한다.

클라이언트 2 접속 후 Red 0 선택:

```text
Blue 0: Human Client1
Red  0: Human Client2
나머지 8칸: Empty/BotCandidate
```

클라이언트 3 접속 후 Blue 1 선택:

```text
Blue 0: Human Client1
Blue 1: Human Client3
Red  0: Human Client2
나머지 7칸: Empty/BotCandidate
```

Start Game 클릭:

```text
Human 3명 + Bot 7명 = 총 10 슬롯 확정
```

즉 사용자가 말한 “1명이면 9봇, 2명이면 8봇, 3명이면 7봇” 구조가 맞다. 다만 구현상으로는 `접속 즉시 실제 봇 엔티티 생성`이 아니라 `로비 슬롯을 봇 후보로 유지하다가 StartGame에서 확정/스폰`하는 흐름으로 간다.

---

## 2. 최종 TCP/UDP 구조

이번 BanPick 버그 수정은 TCP로 먼저 완성한다. UDP는 별도 트랙으로 진행해도 문제 없다.

### TCP 담당

TCP는 reliable control channel로 유지한다.

```text
TCP
- 서버 접속/세션 생성
- BanPick LobbyState 동기화
- JoinSlot / PickChampion / SetBotChampion / Ready / StartGame
- GameStart
- Hello
- Chat, 설정, 재접속, 로딩 상태
```

BanPick과 MatchLoading은 순서 보장과 신뢰성이 중요하므로 TCP가 맞다.

### UDP 담당

UDP는 InGame 고빈도 gameplay transport로 분리한다.

```text
UDP
- 이동/스킬 Command
- Snapshot / DeltaSnapshot
- Ack / sequence
- jitter buffer
- loss/reorder 대응
- AOI 기반 상태 전송
```

UDP는 BanPick을 완성한 뒤 도입한다. 단, 이번 BanPick에서 확정되는 다음 식별자는 UDP에서도 그대로 재사용할 수 있게 설계한다.

```text
sessionId
slotId
matchId 또는 roomId
matchToken
netId
team
championId
```

### 최종 전환 흐름

```text
Client
  -> TCP Connect
  -> BanPick LobbyState 수신/수정
  -> TCP StartGame
  -> TCP Hello + GameStart 수신
  -> UDP Handshake(matchToken, sessionId, netId)
  -> InGame부터 UDP Command/Snapshot 사용

Server
  -> TCP 세션 생성
  -> LobbyState 관리
  -> StartGame에서 roster lock
  -> ECS entity/netId 생성
  -> TCP Hello/GameStart 전송
  -> UDP endpoint를 session/netId에 매핑
  -> InGame tick부터 UDP snapshot 전송
```

이번 계획의 구현 범위는 TCP BanPick + TCP GameStart/Hello까지다. UDP handshake와 gameplay UDP 전환은 후속 계획으로 분리한다.

---

## 3. 설계 규칙

- 서버가 로비 roster의 단일 진실 공급원이다.
- BanPick에서 동기화가 끝난 뒤 MatchLoading/InGame으로 넘어간다.
- `Hello`는 TCP accept 시점에 보내지 않는다. StartGame 후 확정 roster를 기준으로 보낸다.
- `GameContext.SelectedChampion`은 로컬 fallback/debug 값으로만 남긴다.
- `BanPick -> MatchLoading -> InGame` 동안 네트워크 연결은 유지한다.
- InGame이 다시 TCP connect해서 새 세션을 만들면 안 된다.
- 봇 설정은 먼저 로비 데이터로 관리하고, StartGame 때 ECS/AI 컴포넌트로 materialize한다.
- Map data와 `Stage1.dat`는 이번 계획 범위가 아니다.

---

## 3.1 2026-05-06 구현 반영 로그

- Phase 1/2 완료:
  - `GameContext`에 10슬롯 roster 값 모델 추가.
  - `Scene_BanPick`을 local strict custom room UI로 전환.
  - `Scene_InGame`이 `GameContext.Roster` 기준으로 Pure ECS 챔피언/봇을 생성.
- Phase 3 완료:
  - `Scene_MatchLoading`이 hard-coded 팀 배열 대신 locked roster를 표시.
  - Blue/Red 5슬롯, Human/Bot, 챔피언 이름이 BanPick 선택과 이어짐.
- Phase 4 scaffold 완료:
  - `Shared/Schemas/LobbyTypes.fbs`
  - `Shared/Schemas/LobbyState.fbs`
  - `Shared/Schemas/LobbyCommand.fbs`
  - `Shared/Network/PacketEnvelope.h`에 `LobbyCommand/LobbyState/GameStart` packet type 추가.
  - `run_codegen.bat`에 lobby schema 포함, C++/Go generated 파일 생성 확인.
- Phase 5 TCP lobby MVP 완료:
  - `CGameSessionClient` 신설. BanPick와 InGame 사이에서 TCP 연결을 유지.
  - 서버 `CGameRoom`에 lobby slot state, command 처리, state broadcast, StartGame lock 추가.
  - TCP accept 시 즉시 gameplay spawn하지 않고 lobby join/slot assignment를 수행.
  - `LobbyCommand` packet route를 `CPacketDispatcher`에 추가.
  - StartGame 시 빈 슬롯을 bot으로 확정하고, locked roster 기준 서버 ECS champion/netId를 생성.
  - `LobbySlot.netId`와 `GameRosterSlot.netId`를 추가해 client roster entity와 snapshot entity를 같은 netId로 묶음.
  - `Scene_BanPick`은 서버 연결 성공 시 서버 LobbyState 기반 UI/command 전송을 사용하고, 실패 시 기존 local fallback 유지.
  - `Scene_InGame`은 server roster mode에서 BanPick TCP session을 재사용하고, local fallback에서는 기존 connect 경로 유지.

검증:

- Client `ClCompile` 통과.
- Server `Debug|x64` 빌드 통과.
- Client 전체 링크는 실행 중인 `WintersGame.exe`가 산출물을 잡고 있을 때 `LNK1168`이 발생할 수 있음.
- 2026-05-06 Phase 5 반영 후:
  - Engine `Debug|x64` 빌드 통과 및 EngineSDK 헤더 동기화 확인.
  - Client `Debug|x64` 전체 빌드 통과.
  - Server `Debug|x64` 전체 빌드 통과.

---

## 4. 데이터 모델

### Shared schema

신규 파일 후보:

```text
Shared/Schemas/Lobby.fbs
```

권장 enum:

```cpp
enum LobbyPhase : ubyte {
    None = 0,
    SeatSelect = 1,
    ChampionSelect = 2,
    Locked = 3,
    Starting = 4,
    InGame = 5
}

enum LobbyCommandKind : ubyte {
    None = 0,
    JoinSlot = 1,
    LeaveSlot = 2,
    SwapTeam = 3,
    PickChampion = 4,
    SetBotChampion = 5,
    SetBotDifficulty = 6,
    SetReady = 7,
    StartGame = 8,
    CancelStart = 9,
    SetEditPolicy = 10
}

enum LobbySeatKind : ubyte {
    Empty = 0,
    Human = 1,
    Bot = 2
}
```

권장 table:

```cpp
table LobbySlot {
    slotId:ubyte;          // 0-4 Blue, 5-9 Red
    team:ubyte;            // eTeam
    seatKind:LobbySeatKind;
    sessionId:uint;        // Empty/Bot이면 0
    championId:ubyte;      // eChampion
    botDifficulty:ubyte;   // 0 none, 1 easy, 2 normal, 3 hard
    botProfile:ushort;     // future
    ready:bool;
    locked:bool;
}

table LobbyState {
    roomId:uint;
    revision:uint;
    hostSessionId:uint;
    phase:LobbyPhase;
    allPlayersCanEditBots:bool;
    slots:[LobbySlot];
    startCountdownMs:uint;
}

table LobbyCommand {
    kind:LobbyCommandKind;
    slotId:ubyte;
    team:ubyte;
    championId:ubyte;
    botDifficulty:ubyte;
    value:uint;
}
```

FlatBuffers root type 제약 때문에 `LobbyCommand`와 `LobbyState`를 한 파일에 두기 불편하면 다음처럼 나눈다.

```text
Shared/Schemas/LobbyState.fbs
Shared/Schemas/LobbyCommand.fbs
```

### PacketEnvelope

추가 패킷 타입:

```cpp
LobbyCommand = 20,
LobbyState = 21,
GameStart = 22
```

`GameStart`는 나중에 `Event`로 흡수할 수 있지만, MVP에서는 별도 packet type이 씬 전환을 구현하기 쉽다.

---

## 5. 서버 구현 계획

### M0. Lobby state 추가

파일:

- `Server/Public/Game/GameRoom.h`
- `Server/Private/Game/GameRoom.cpp`

추가 구조체 후보:

```cpp
enum class eRoomPhase : u8_t
{
    Lobby,
    Loading,
    InGame,
};

struct LobbySlotState
{
    u8_t slotId = 0;
    eTeam team = eTeam::Blue;
    bool_t bHuman = false;
    bool_t bBot = false;
    u32_t sessionId = 0;
    eChampion champion = eChampion::END;
    u8_t botDifficulty = 2;
    bool_t bReady = false;
    bool_t bLocked = false;
};
```

서버 room 상태:

```cpp
eRoomPhase m_roomPhase = eRoomPhase::Lobby;
u32_t m_hostSessionId = 0;
u32_t m_lobbyRevision = 0;
bool_t m_bAllPlayersCanEditBots = true; // dev/custom 기본값
LobbySlotState m_lobbySlots[10];
std::unordered_map<u32_t, u8_t> m_sessionToSlot;
```

### M1. TCP accept 시점 스폰 중단

변경:

- `CIOCPCore`는 세션만 만든다.
- `CGameRoom::OnSessionJoin`은 gameplay spawn이 아니라 lobby join을 처리한다.
- 첫 접속자는 host가 된다.
- 플레이어는 첫 빈 슬롯에 자동 배정하거나, 미배정 상태로 두고 UI에서 직접 고르게 한다.
- 서버는 `Hello` 대신 `LobbyState`를 보낸다.
- `CPacketDispatcher::RouteSession`은 lobby packet도 받을 수 있도록 join 시점에 수행한다.

최소 변경:

```cpp
EntityID OnSessionJoin(u32_t sessionId); // Lobby phase에서는 NULL_ENTITY 반환 가능
void OnLobbyJoin(u32_t sessionId);
```

`session->SetControlledEntity(NULL_ENTITY)`는 StartGame 전까지 허용한다.

### M2. Lobby command 처리

수정 파일:

- `Server/Public/Network/PacketDispatcher.h`
- `Server/Private/Network/PacketDispatcher.cpp`
- `Server/Public/Game/GameRoom.h`
- `Server/Private/Game/GameRoom.cpp`

추가:

```cpp
void DispatchLobbyCommand(u32_t sessionId, const ParsedFrameOwned& frame);
void CGameRoom::OnLobbyCommand(u32_t sessionId, const Shared::Schema::LobbyCommand* command);
```

검증 규칙:

- 플레이어는 하나의 human slot만 차지할 수 있다.
- 이미 human이 있는 슬롯은 빼앗을 수 없다.
- 봇 슬롯은 host가 수정할 수 있다.
- dev/custom 옵션에서는 모든 플레이어가 봇을 수정할 수 있다.
- `championId == END`는 StartGame 시 허용하지 않는다.
- MVP에서는 중복 챔피언을 허용한다.

명령이 성공하면:

```cpp
++m_lobbyRevision;
BroadcastLobbyState();
```

### M3. StartGame lock

`StartGame` 명령 처리:

1. human player가 1명 이상인지 확인한다.
2. 아직 비어 있는 슬롯을 bot slot으로 확정한다.
3. 챔피언이 비어 있는 bot slot은 기본 챔피언으로 채운다.
4. 모든 slot을 locked 상태로 바꾼다.
5. room phase를 `Loading`으로 바꾼다.
6. locked roster 기준으로 서버 ECS champion entity를 생성한다.
7. 각 entity에 net id를 발급한다.
8. 각 human client에게 자기 `controlledNetId`를 담은 `Hello`를 보낸다.
9. 모든 client에게 `GameStart` 또는 `LobbyState(phase=Starting/InGame)`를 보낸다.
10. 이후 normal snapshot broadcast를 시작한다.

신규 spawn 함수:

```cpp
EntityID SpawnChampionForLobbySlot(const LobbySlotState& slot);
```

이 함수가 기존 `SpawnChampion(sessionId)`의 Ezreal 하드코딩을 대체한다.

### M4. Bot materialization

MVP:

- Bot slot도 서버 ECS entity로 생성한다.
- `ChampionComponent`, `StatComponent`, `SkillStateComponent`, `NetEntityIdComponent`를 붙인다.
- 가능한 경우 `BotComponent`와 blackboard를 붙인다.
- 챔피언별 AI가 없으면 `BuildStandardChampionBT()`를 사용한다.

Bot은 session이 없다.

```cpp
slot.sessionId = 0;
```

그래도 snapshot에는 일반 champion entity처럼 포함된다.

---

## 6. 클라이언트 구현 계획

### M0. 네트워크 수명 분리

현재 문제:

- BanPick도 네트워크가 필요하다.
- InGame이 새 `CClientNetwork`를 만든다.
- InGame에서 재접속하면 서버가 새 session을 만들고 roster 확정 상태와 어긋난다.

신규 client-side owner:

```text
Client/Public/Network/Client/GameSessionClient.h
Client/Private/Network/Client/GameSessionClient.cpp
```

역할:

- `std::unique_ptr<CClientNetwork>` 소유
- TCP connect 1회
- 최신 `LobbyState` 저장
- `mySessionId`, `myNetId`, `mySlotId` 저장
- `SendLobbyCommand` 제공
- `Pump` 제공
- Scene별 callback 연결

주의:

- `CClientNetwork`를 Engine `GameContext`에 넣지 않는다.
- Engine include boundary에 Client 네트워크 타입을 누설하지 않는다.

### M1. GameContext 확장

Engine 쪽에는 값 타입만 추가한다.

```cpp
struct GameRosterSlot
{
    u8_t slotId = 0;
    eTeam team = eTeam::Blue;
    bool_t bHuman = false;
    bool_t bBot = false;
    u32_t sessionId = 0;
    eChampion champion = eChampion::END;
    u8_t botDifficulty = 0;
};

struct GameContext
{
    eChampion SelectedChampion = eChampion::END; // fallback/debug
    bool_t bUseNetworkRoster = false;
    u32_t MySessionId = 0;
    u32_t MyNetId = 0;
    u8_t MySlotId = 255;
    eTeam MyTeam = eTeam::Blue;
    GameRosterSlot Roster[10] = {};
};
```

### M2. Scene_BanPick을 로비 UI로 변경

파일:

- `Client/Public/Scene/Scene_BanPick.h`
- `Client/Private/Scene/Scene_BanPick.cpp`

`OnEnter`:

- `CGameSessionClient`를 얻거나 만든다.
- `127.0.0.1:9000`에 연결한다.
- lobby state callback을 등록한다.
- 서버가 없으면 기존 local-only champion select fallback을 허용한다.

`OnUpdate`:

- network frame을 pump한다.
- lobby phase가 `Starting` 또는 `InGame`이면 locked roster를 `GameContext`에 복사한다.
- `CScene_MatchLoading`으로 전환한다.

`OnImGui`:

- Blue/Red 2열 UI
- 각 팀 5 슬롯
- 슬롯별 기능:
  - Join / Leave
  - Empty/BotCandidate 표시
  - Champion combo/grid
  - Bot difficulty combo
  - Human ready checkbox
- 하단 champion grid는 `CChampionRegistry::ForEach`로 구성한다.
- Start button:
  - 기본은 host만
  - dev/custom 옵션에서는 모든 플레이어 노출 가능

중요:

- 버튼은 `GameContext`를 직접 바꾸지 않는다.
- 버튼은 `LobbyCommand`를 서버로 보낸다.
- UI는 항상 최신 서버 `LobbyState`를 그린다.

### M3. MatchLoading이 locked roster 표시

현재 `Scene_MatchLoading`은 하드코딩된 팀 이름 배열을 쓴다.

수정:

- `GameContext.Roster`를 읽어서 Blue/Red 슬롯을 표시한다.
- `GetChampionDisplayName` 또는 `CChampionRegistry`를 사용한다.
- Human/Bot 마커를 보여준다.
- progress는 기존 time-based 방식 유지.

파일:

- `Client/Private/Scene/Scene_MatchLoading.cpp`

### M4. InGame이 기존 network session 재사용

파일:

- `Client/Public/Scene/Scene_InGame.h`
- `Client/Private/Scene/Scene_InGame.cpp`

수정:

- `GameContext.bUseNetworkRoster == true`이면 `CGameSessionClient`의 기존 `CClientNetwork`를 사용한다.
- InGame에서 새 TCP connect를 하지 않는다.
- network roster가 잠긴 상태에서는 local `SelectedChampion`으로 플레이어를 다시 고르지 않는다.
- `Hello`는 local player의 `yourNetId` 바인딩에만 사용한다.
- remote/bot entity는 snapshot과 locked roster를 기준으로 만든다.

Fallback:

- 서버 연결이 없거나 `bUseNetworkRoster == false`이면 기존 local-only path를 유지한다.

### M5. `WINTERS_MIN_SCENE` 생성 분기 제거 방향

현재 `Scene_InGame`의 챔피언 생성은 컴파일 타임 매크로에 묶여 있다.

현재 구조:

- `Client/Private/Scene/Scene_InGame.cpp:3`
  - `#define WINTERS_MIN_SCENE 0`
- `CreateECSEntities()`
  - `!WINTERS_MIN_SCENE`: 레거시 멤버 렌더러 5체 + Pure ECS 챔피언 여러 체를 미리 전부 생성한다.
  - `WINTERS_MIN_SCENE`: `GameContext.SelectedChampion` 기준으로 일부 Pure ECS 챔피언만 생성한다.
- 문제:
  - BanPick에서 선택한 roster와 무관하게 씬이 챔피언을 선생성한다.
  - network roster가 잠겨도 `SelectedChampion`/매크로 분기에 끌려갈 수 있다.
  - Legacy champion은 `ModelRenderer m_Irelia`, `m_Yasuo` 같은 Scene 멤버에 묶여 있다.
  - 일부 legacy `ChampionDef`는 `fbxPath`가 비어 있어 `CreateECSChampion`으로 바로 생성할 수 없다.

목표 구조:

```cpp
enum class eInGameSpawnMode : u8_t
{
    LocalSingleChampion, // 서버 없는 local fallback
    NetworkRoster,      // BanPick locked roster
    LegacyShowcase,     // 필요 시 전체 챔피언 디버그 전시
};
```

핵심 변경:

```cpp
void CreateECSEntities();
void CreateRosterChampionsFromGameContext();
EntityID CreateRosterChampion(const GameRosterSlot& slot);
EntityID CreateLocalFallbackChampion(eChampion champion);
void RegisterPlayerEntityFromRoster();
```

생성 원칙:

- `GameContext.bUseNetworkRoster == true`
  - `GameContext.Roster`를 순회한다.
  - `bHuman || bBot`인 슬롯만 생성한다.
  - `slot.champion != END`인 슬롯만 생성한다.
  - `slot.sessionId == GameContext.MySessionId`인 entity를 `m_PlayerEntity`로 지정한다.
- `bUseNetworkRoster == false`
  - 기존 local fallback처럼 `SelectedChampion` 한 체만 생성한다.
- `LegacyShowcase`
  - 필요할 때만 현재 `!WINTERS_MIN_SCENE`의 전시/디버그 전체 생성 모드를 유지한다.

단계별 리팩터:

1. `WINTERS_MIN_SCENE`을 즉시 삭제하지 말고, 먼저 런타임 spawn mode를 추가한다.
2. `CreateECSEntities()` 초입에서 `bUseNetworkRoster`면 `CreateRosterChampionsFromGameContext()`로 빠진다.
3. Pure ECS 챔피언은 기존 `CreateECSChampion(id, team)`을 그대로 사용한다.
4. legacy 멤버 렌더러 기반 챔피언은 당장 강제 이전하지 않는다.
5. legacy 챔피언을 roster에서 선택하려면 `ChampionDef`에 `fbxPath`, texture, spawnScale을 채우거나 adapter를 둔다.
6. 모든 챔피언이 `CreateECSChampion` 경로로 생성되면 `WINTERS_MIN_SCENE`과 레거시 선생성 분기를 제거한다.

Legacy champion 정리 대상:

```text
Irelia / Yasuo / Kalista / Garen / Zed / Sylas / Viego
```

현재 `ChampionTable.cpp`의 일부 legacy def는 animation key만 있고 `fbxPath`가 비어 있다. 그래서 roster-driven spawn을 전 챔피언에 적용하려면 ChampionDef 완성 작업이 선행되어야 한다.

권장 MVP:

```text
1차: Pure ECS/등록 완료 챔피언부터 roster-driven spawn
     Ezreal / Fiora / Jax / Annie / Ashe / Yone / Riven

2차: legacy ChampionDef 채우기
     Irelia / Yasuo / Kalista / Garen / Zed

3차: blueprint/legacy 특수 생성 제거
     Sylas / Viego 포함
```

규모 판단:

- BanPick 로비만 구현하면 중간 규모다.
- BanPick roster가 InGame 생성의 단일 원천이 되게 하면 중간~큰 규모다.
- 모든 legacy champion까지 한 번에 `CreateECSChampion`으로 통합하면 큰 규모다.
- 따라서 이번 버그 수정에서는 `NetworkRoster` 경로를 먼저 추가하고, legacy 제거는 후속 phase로 나누는 것이 안전하다.

---

## 7. Hello Ezreal 버그 수정

이 버그는 BanPick 구현과 함께 제거한다.

### 서버

기존:

```cpp
static_cast<u8_t>(eChampion::EZREAL)
```

변경:

```cpp
static_cast<u8_t>(slot.champion)
```

기존:

```cpp
stat.championId = eChampion::EZREAL;
champion.id = eChampion::EZREAL;
```

변경:

```cpp
stat.championId = slot.champion;
champion.id = slot.champion;
champion.team = slot.team;
```

### 클라이언트

`Scene_InGame`에 helper 추가:

```cpp
eChampion CScene_InGame::GetPlayerChampionId() const
{
    if (m_PlayerEntity != NULL_ENTITY &&
        m_World.HasComponent<ChampionComponent>(m_PlayerEntity))
    {
        return m_World.GetComponent<ChampionComponent>(m_PlayerEntity).id;
    }

    return CGameInstance::Get()->Get_GameContext().SelectedChampion;
}
```

적용 위치:

- `DispatchSkillInput`
- `ApplyLocalPrediction`
- 플레이어 전용 animation/skill lookup 중 `SelectedChampion`을 읽는 경로

목표:

- Fiora/Yone을 선택했는데 Ezreal 스킬/애니메이션을 찾는 상황 제거.
- 서버가 확정한 champion id와 클라이언트 조작/연출 경로를 일치시킨다.

---

## 8. 파일 영향 범위

### 신규

- `Shared/Schemas/Lobby.fbs`
- `Client/Public/Network/Client/GameSessionClient.h`
- `Client/Private/Network/Client/GameSessionClient.cpp`

필요 시 schema split:

- `Shared/Schemas/LobbyCommand.fbs`
- `Shared/Schemas/LobbyState.fbs`

### 수정

- `Shared/Network/PacketEnvelope.h`
- `Shared/Schemas/run_codegen.bat`
- `Engine/Include/GameContext.h`
- `Server/Public/Game/GameRoom.h`
- `Server/Private/Game/GameRoom.cpp`
- `Server/Public/Network/PacketDispatcher.h`
- `Server/Private/Network/PacketDispatcher.cpp`
- `Server/Private/Network/IOCPCore.cpp`
- `Client/Public/Scene/Scene_BanPick.h`
- `Client/Private/Scene/Scene_BanPick.cpp`
- `Client/Private/Scene/Scene_MatchLoading.cpp`
- `Client/Public/Scene/Scene_InGame.h`
- `Client/Private/Scene/Scene_InGame.cpp`
- `Client/Private/GameObject/ChampionTable.cpp`
- `Client/Private/GameObject/Champion/*/*_Registration.cpp`
- Client/Server vcxproj

---

## 9. 구현 단계

### Phase A. Protocol

1. Lobby schema 추가.
2. packet type 추가.
3. FlatBuffers codegen 실행.
4. generated C++ header include 확인.
5. Client/Server parse-only compile.

Gate:

- `Lobby_generated.h`가 Client/Server에서 include된다.
- generated file include path hack이 없다.

### Phase B. Server lobby

1. `LobbySlotState` 추가.
2. 10 슬롯 초기화.
3. `OnSessionJoin`을 lobby join으로 전환.
4. `BroadcastLobbyState` 추가.
5. `OnLobbyCommand` 추가.
6. host leave 시 host migration 추가.

Gate:

- 클라이언트 1개 접속 시 `LobbyState` 수신.
- 클라이언트 3개 접속 시 human session 3개 표시.
- StartGame 전에는 `Hello`가 오지 않는다.

### Phase C. Client BanPick UI

1. `CGameSessionClient` 추가.
2. InGame의 connect/pump 흐름을 session client로 분리.
3. BanPick이 서버 `LobbyState`를 렌더링.
4. JoinSlot / PickChampion / SetBotChampion / SetReady / StartGame command 송신.
5. local-only fallback 유지.

Gate:

- 두 클라이언트가 같은 slot/champion 변경을 본다.
- 한 클라이언트가 바꾼 bot champion이 다른 클라이언트에 보인다.

### Phase D. StartGame/InGame handoff

1. 서버가 roster를 lock한다.
2. 서버가 slot-driven champion entity를 spawn한다.
3. 서버가 human client별 `Hello`를 정확한 net id/champion/team으로 보낸다.
4. 클라이언트가 locked roster를 `GameContext`에 저장한다.
5. MatchLoading이 locked roster를 표시한다.
6. InGame이 기존 TCP session을 재사용한다.

Gate:

- Fiora 선택 시 Fiora로 bind된다.
- Yone 선택 시 Yone으로 bind된다.
- Ezreal을 선택하지 않았는데 Ezreal로 강제 bind되지 않는다.

### Phase E. `WINTERS_MIN_SCENE` 런타임 roster spawn 전환

1. `eInGameSpawnMode` 또는 동등한 런타임 분기 추가.
2. `CreateECSEntities()`에 `NetworkRoster` 경로 추가.
3. `GameContext.Roster`의 Human/Bot 슬롯만 순회해 champion entity 생성.
4. `sessionId == MySessionId`인 슬롯을 `m_PlayerEntity`로 지정.
5. Pure ECS 챔피언은 기존 `CreateECSChampion` 사용.
6. legacy 챔피언은 `ChampionDef` 보강 전까지 fallback/adaptor로 처리.
7. 기존 `WINTERS_MIN_SCENE` 매크로는 최종 제거 전까지 local debug fallback으로만 남긴다.

Gate:

- BanPick에서 선택한 챔피언만 InGame에 생성된다.
- 선택하지 않은 Ezreal/Fiora/Yone이 자동 선생성되지 않는다.
- local fallback에서는 기존처럼 단일 챔피언 테스트가 가능하다.
- `WINTERS_MIN_SCENE` 값 변경 없이 런타임 roster로 생성 대상이 바뀐다.

### Phase F. Skill/animation binding

1. `GetPlayerChampionId` 추가.
2. player skill/anim lookup의 `SelectedChampion` 의존 제거.
3. `SelectedChampion`은 local fallback으로만 사용.

Gate:

- Ezreal/Fiora/Yone의 QWER가 각각 자기 `SkillDef`를 찾는다.
- animation key가 bound player의 `ChampionComponent.id` 기준으로 결정된다.

### Phase G. Bot gameplay

1. Bot slot을 서버 entity로 생성.
2. 가능한 경우 `BotComponent` 부착.
3. snapshot에 bot entity 포함.
4. client `SnapshotApplier::SetOnNewEntityCallback`으로 bot visual entity 생성.

Gate:

- 3 human + 7 bot이 같은 match에 들어간다.
- 모든 client가 같은 roster/net id를 본다.

---

## 10. 검증 체크리스트

### 단일 클라이언트

- 서버 실행.
- 클라이언트 1개 실행.
- Blue 0 선택.
- Fiora 선택.
- bot champion 여러 개 변경.
- StartGame.
- InGame local player가 Fiora인지 확인.
- Fiora Q/W/E/R lookup이 Fiora definition을 쓰는지 확인.

### 클라이언트 3개

- 서버 실행.
- 클라이언트 3개 실행.
- Client1 Blue 0 선택.
- Client2 Red 0 선택.
- Client3 Blue 1 선택.
- 나머지 7칸이 bot candidate로 남는지 확인.
- Client1이 bot champion을 Yone으로 변경.
- Client2가 다른 bot champion을 Annie로 변경.
- Client3이 bot difficulty 변경.
- 모든 클라이언트에서 같은 lobby revision/slot state 확인.
- StartGame.
- 3 human + 7 bot roster로 MatchLoading/InGame 진입.

### 회귀

- 서버가 꺼져 있으면 BanPick local fallback이 동작한다.
- `bUseNetworkRoster == false`이면 기존 local `SelectedChampion` path가 동작한다.
- map data는 변경하지 않는다.

---

## 11. 리스크

- 가장 큰 리스크는 scene 전환 중 network lifetime이다. InGame 재접속은 반드시 막아야 한다.
- FlatBuffers root type 제약으로 schema split이 필요할 수 있다.
- `Scene_InGame`에는 아직 local champion 가정이 많다. 우선 player-control hot path부터 고친다.
- bot AI는 챔피언별 완성도가 다르다. MVP에서는 visual/server entity bot을 먼저 허용한다.
- host 권한과 all-player edit 권한은 명시적으로 나눠야 이후 보안/권한 정책이 꼬이지 않는다.
- UDP 도입은 transport 리스크가 크므로 이번 BanPick 수정과 분리한다.

---

## 12. 이번 계획에서 제외

- Map data / `Stage1.dat` 수정.
- 정식 ban phase.
- skin selection.
- backend matchmaking 연동.
- spectator mode.
- champion duplicate restriction.
- UDP gameplay transport 구현.
- anti-cheat 수준의 lobby command 검증.
