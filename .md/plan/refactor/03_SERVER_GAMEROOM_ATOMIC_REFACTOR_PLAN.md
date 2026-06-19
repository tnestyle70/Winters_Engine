Session - Server GameRoom을 AuthorityExecution shell로 줄이고 LobbyAuthority부터 원자 분리한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Server/Public/Game/GameRoom.h

유지할 public surface:

```cpp
static std::unique_ptr<CGameRoom> Create(u32_t roomId);
~CGameRoom();

void Start();
void Stop();

void OnCommandBatch(u32_t sessionId, const Shared::Schema::CommandBatch* batch);
void EnqueueCommand(u32_t sessionId, const GameCommandWire& wire,
    u64_t acceptedTick, u64_t recvTimeMs, u64_t clientTimestampMs);
void OnLobbyCommand(u32_t sessionId, const Shared::Schema::LobbyCommand* command);

EntityID OnSessionJoin(u32_t sessionId);
void OnSessionLeave(u32_t sessionId);
bool DebugSetHealthByNetId(NetEntityId netId, f32_t value);
```

첫 리팩터에서 유지할 본질:

```text
CGameRoom = RoomLifecycle + AuthorityOrchestration
```

`CGameRoom`에 남길 것:
- room id
- running state
- tick thread
- external server entry API
- atomic owners를 호출하는 orchestration

`CGameRoom`에서 제거할 방향:
- lobby slot rule 직접 소유
- lobby packet build 직접 소유
- session reconnect rule 직접 소유
- pending command queue 직접 소유
- snapshot/event/replay emission 직접 소유
- navgrid/path query 구현 직접 소유
- minion/projectile/death/respawn rule 직접 소유

첫 세션에서는 아래 lobby state와 lobby helper를 새 owner로 옮긴다.

```cpp
enum class eRoomPhase : u8_t
{
    SeatSelect,
    ChampionSelect,
    Loading,
    InGame,
};

struct LobbySlotState
{
    u8_t slotId = kInvalidGameRosterSlot;
    u8_t team = 0;
    bool_t bHuman = false;
    bool_t bBot = false;
    u32_t sessionId = 0;
    NetEntityId netId = NULL_NET_ENTITY;
    eChampion champion = eChampion::END;
    u8_t botDifficulty = 2;
    u8_t botLane = kGameRosterDefaultBotLane;
    bool_t bReady = false;
    bool_t bLocked = false;
    bool_t bDummy = false;
};
```

이후 `CGameRoom`은 lobby 상태를 직접 판단하지 않고 `CLobbyAuthority`에 위임한다.

확인 필요:
- `eRoomPhase`와 `LobbySlotState`가 `GameRoomSmokeRoster`, `GameRoomSpawn`, `GameRoomChampionAI`에서 직접 쓰이므로 첫 세션에서 include 순환 없이 옮길 수 있는 위치를 확정한다.
- `LobbySlotState`를 public header에 유지할지, `LobbyAuthorityTypes.h`로 분리할지 결정한다.

1-2. C:/Users/tnest/Desktop/Winters/Server/Public/Game/LobbyAuthority.h

새 파일:

```cpp
#pragma once

#include "GameContext.h"
#include "Shared/GameSim/Replication/EntityIdMap.h"
#include "WintersTypes.h"

#include <string>
#include <unordered_map>

namespace Shared::Schema
{
    struct LobbyCommand;
}

enum class eRoomPhase : u8_t
{
    SeatSelect,
    ChampionSelect,
    Loading,
    InGame,
};

struct LobbySlotState
{
    u8_t slotId = kInvalidGameRosterSlot;
    u8_t team = 0;
    bool_t bHuman = false;
    bool_t bBot = false;
    u32_t sessionId = 0;
    NetEntityId netId = NULL_NET_ENTITY;
    eChampion champion = eChampion::END;
    u8_t botDifficulty = 2;
    u8_t botLane = kGameRosterDefaultBotLane;
    bool_t bReady = false;
    bool_t bLocked = false;
    bool_t bDummy = false;
};

struct LobbyAuthorityResult
{
    bool_t bAccepted = false;
    bool_t bStateChanged = false;
    bool_t bBroadcastLobbyState = false;
    bool_t bSendHello = false;
    bool_t bBeginLoading = false;
    bool_t bBeginInGame = false;
    bool_t bRequestStopReplay = false;

    u32_t sessionId = 0;
    NetEntityId helloNetId = NULL_NET_ENTITY;
    eChampion helloChampion = eChampion::END;
    u8_t helloTeam = 0;
};

class CLobbyAuthority final
{
public:
    explicit CLobbyAuthority(u32_t roomId);

    void InitializeSlots();

    LobbyAuthorityResult OnLobbyCommand(u32_t sessionId, const Shared::Schema::LobbyCommand* command);
    LobbyAuthorityResult OnSessionJoin(u32_t sessionId);
    LobbyAuthorityResult OnSessionLeave(u32_t sessionId);

    eRoomPhase GetPhase() const;
    u32_t GetRevision() const;
    u32_t GetHostSessionId() const;
    const std::string& GetLastMessage() const;

    LobbySlotState* GetSlots();
    const LobbySlotState* GetSlots() const;
    u32_t GetSlotCount() const;

    bool TryResolveSessionSlot(u32_t sessionId, u8_t& outSlotId) const;
    bool TryAttachDisconnectedHumanSlot(
        u32_t sessionId,
        NetEntityId& outNetId,
        eChampion& outChampion,
        u8_t& outTeam);
    void SetSlotNetId(u8_t slotId, NetEntityId netId);

private:
    u8_t FindFirstEmptySlot(u32_t beginSlot, u32_t endSlot) const;
    void CompactTeamSlots(u32_t beginSlot, u32_t endSlot);
    void OnLobbyJoin(u32_t sessionId);

    bool TryJoinSlot(u32_t sessionId, u8_t slotId);
    bool TryLeaveSlot(u32_t sessionId);
    bool TryPickChampion(u32_t sessionId, eChampion champion);
    bool TrySetBotChampion(u32_t sessionId, u8_t slotId, eChampion champion);
    bool TrySetBotDifficulty(u32_t sessionId, u8_t slotId, u8_t difficulty);
    bool TrySetBotLane(u32_t sessionId, u8_t slotId, u8_t lane);
    bool TryAdvanceToChampionSelect(u32_t sessionId);
    bool TryStartGame(u32_t sessionId);
    bool TrySetReady(u32_t sessionId, bool_t bReady);
    bool TryStopReplay(u32_t sessionId);

    bool AreAllActiveHumanSlotsReady() const;
    bool CanEditBots(u32_t sessionId) const;
    void SetMessage(const std::string& message);
    void SetMessage(const char* message);
    LobbyAuthorityResult MakeStateResult(bool_t bAccepted, bool_t bChanged, u32_t sessionId) const;

    u32_t m_roomId = 0;
    eRoomPhase m_phase = eRoomPhase::SeatSelect;
    u32_t m_hostSessionId = 0;
    u32_t m_revision = 0;
    std::string m_lastMessage;
    bool_t m_bAllPlayersCanEditBots = true;
    LobbySlotState m_slots[kGameRosterSlotCount]{};
    std::unordered_map<u32_t, u8_t> m_sessionToSlot;
};
```

확인 필요:
- `kGameRosterSlotCount`, `kInvalidGameRosterSlot`, `kGameRosterDefaultBotLane`, `eChampion`, `NetEntityId` include 위치가 위 header만으로 컴파일되는지 확인한다.
- `CLobbyAuthority`는 `Network/Session`, `PacketDispatcher`, `flatbuffers`, `CWorld`, `ICommandExecutor`, `CReplayRecorder`를 include하지 않는다.
- `CLobbyAuthority`는 side effect를 실행하지 않고 `LobbyAuthorityResult`만 반환한다.

1-3. C:/Users/tnest/Desktop/Winters/Server/Private/Game/LobbyAuthority.cpp

새 파일:

```cpp
#include "Game/LobbyAuthority.h"

#include "GameRoomSmokeRoster.h"
#include "Shared/GameSim/Definitions/MapSpawnPoints.h"
#include "Shared/Schemas/Generated/cpp/LobbyCommand_generated.h"

#include <cstdio>
#include <string>
#include <vector>
```

이 파일로 옮길 범위:
- `GameRoomLobby.cpp` anonymous namespace의 lobby command 이름/로그/helper
- `CGameRoom::InitializeLobbySlots`
- `CGameRoom::FindFirstEmptyLobbySlot`
- `CGameRoom::CompactLobbyTeamSlotsLocked`
- `CGameRoom::OnLobbyJoin`
- `CGameRoom::TryJoinSlot`
- `CGameRoom::TryLeaveSlot`
- `CGameRoom::TryPickChampion`
- `CGameRoom::TrySetBotChampion`
- `CGameRoom::TrySetBotLane`
- `CGameRoom::TrySetBotDifficulty`
- `CGameRoom::TryAdvanceToChampionSelect`
- `CGameRoom::TryStartGame`
- `CGameRoom::TryStopReplay`
- `CGameRoom::TrySetReady`
- `CGameRoom::AreAllActiveHumanSlotsReady`
- `CGameRoom::CanEditBots`
- `CGameRoom::SetLobbyMessageLocked`

남길 것:
- packet send, hello send, game start broadcast처럼 session transport를 직접 쓰는 함수는 `CGameRoom` adapter에 남긴다.
- `CLobbyAuthority`는 side effect를 직접 실행하지 않고 `LobbyAuthorityResult`로 요청만 반환한다.

CONFIRM_NEEDED:
- `GameRoomLobby.cpp` 전체를 다시 읽고 함수 body 이동 순서를 확정한다.
- `TryStartGame` 안의 `SpawnChampionsFromLobby`, `SpawnServerGameplayObjects`, `SendHelloToSessionLocked` 호출은 `CLobbyAuthority` 밖으로 빼고 `LobbyAuthorityResult::bBeginLoading` 처리에서 실행한다.

1-4. C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoomLobby.cpp

첫 세션 후 남길 책임:

```text
transport adapter
```

남길 함수:
- `CGameRoom::OnLobbyCommand`
- `CGameRoom::OnSessionJoin`
- `CGameRoom::OnSessionLeave`
- `CGameRoom::BroadcastLobbyStateLocked`
- `CGameRoom::BroadcastGameStartLocked`
- `CGameRoom::SendGameStartToSessionLocked`
- `CGameRoom::SendHelloToSessionLocked`

삭제할 방향:
- lobby rule 판단
- slot compaction
- bot edit permission
- ready/all-ready 판단
- champion select/start game permission
- lobby message mutation

CONFIRM_NEEDED:
- `BroadcastLobbyStateLocked`가 `m_lobbySlots`, `m_roomPhase`, `m_lobbyRevision`, `m_hostSessionId`, `m_bAllPlayersCanEditBots`, `m_strLastLobbyMessage`를 직접 읽고 있으므로 `CLobbyAuthority` getter 기반으로 바꾼다.

1-5. C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoom.cpp

기존 코드:

```cpp
CGameRoom::CGameRoom(u32_t roomId)
    : m_roomId(roomId)
{
    InitializeLobbySlots();
}
```

아래 방향으로 교체:

```cpp
CGameRoom::CGameRoom(u32_t roomId)
    : m_roomId(roomId)
{
    InitializeLobbyAuthority();
}
```

아래에 추가할 private helper 방향:

```cpp
void CGameRoom::InitializeLobbyAuthority()
{
    m_pLobbyAuthority = std::make_unique<CLobbyAuthority>(m_roomId);
    m_pLobbyAuthority->InitializeSlots();
}
```

CONFIRM_NEEDED:
- `CGameRoom::OnLobbyCommand`, `OnSessionJoin`, `OnSessionLeave`는 `LobbyAuthorityResult`를 받아 packet send, game bootstrap, replay stop 같은 side effect를 처리한다.
- `CLobbyAuthority`가 `CGameRoom` callback을 저장하지 않는지 확인한다.

1-6. C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoomTick.cpp

유지할 실행 순서:

```cpp
Phase_DrainCommands(tc);
Phase_ServerBotAI(tc);
Phase_ExecuteCommands(tc);
Phase_SimulationSystems(tc);
Phase_BroadcastEvents(tc);
Phase_BroadcastSnapshot(tc);
```

첫 세션에서는 tick pipeline을 바꾸지 않는다.

금지:
- lobby 분리와 동시에 simulation system 순서를 바꾸지 않는다.
- minion/projectile/death/respawn을 같이 옮기지 않는다.
- snapshot/event emission 순서를 같이 바꾸지 않는다.

1-7. C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoomCommands.cpp

다음 세션 분리 원자:

```text
CommandIngress
CommandExecution
SessionBinding
```

첫 세션에서는 수정하지 않는다.

이후 옮길 것:
- `PendingCommand`
- `m_pendingCommands`
- `m_pendingExecCommands`
- `OnCommandBatch`
- `EnqueueCommand`
- `Phase_DrainCommands`
- `ResolveControlledEntityForSession`
- `m_lastSimCommandSeqBySession`

확인 필요:
- command sequence accept는 `Session`과 묶여 있으므로 `CommandIngress`가 session transport를 직접 알지, callback으로 받을지 결정한다.

1-8. C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoomReplication.cpp

다음 세션 분리 원자:

```text
ReplicationEmitter
ReplayEvidence
```

첫 세션에서는 수정하지 않는다.

이후 옮길 것:
- `BroadcastEventPayload`
- `BroadcastReplicatedEvent`
- `Phase_BroadcastEvents`
- `Phase_BroadcastSnapshot`
- `FinalizeReplayRecorder`
- `m_lastBroadcastActionSeq`
- `m_pReplayRecorder`
- `m_bReplayFinalized`

확인 필요:
- replay는 ValidationEvidence 성격이므로 replication transport와 같은 owner로 둘지 `ReplayEvidence`로 분리할지 결정한다.

1-9. C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoomNav.cpp

다음 세션 분리 원자:

```text
WalkabilityAuthority
```

첫 세션에서는 수정하지 않는다.

이후 옮길 것:
- server stage path resolve
- authored navgrid load
- walkable query
- segment clamp
- height sample
- minion waypoint cache
- flow field rebuild trigger

금지:
- `Client/Bin/Debug*/Resource` fallback을 기준 경로로 승격하지 않는다.
- nav 분리와 동시에 gameplay movement truth를 바꾸지 않는다.

1-10. C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoomSpawn.cpp

다음 세션 분리 원자:

```text
WorldBootstrap
```

첫 세션에서는 수정하지 않는다.

이후 옮길 것:
- champion spawn from lobby
- server structures spawn
- jungle spawn
- minion spawn primitive
- stage gameplay object bootstrap

확인 필요:
- champion component defaults가 GameDesignSource/Data로 이동해야 하는 값인지, Shared/GameSim runtime default인지 분류한다.

1-11. C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoomMinionAI.cpp

다음 세션 분리 원자:

```text
ServerAICommandProducer
```

첫 세션에서는 수정하지 않는다.

금지:
- bot/minion AI가 Transform, Health, cooldown 같은 truth component를 command 없이 직접 수정하는 경로를 새로 만들지 않는다.
- minion AI와 navigation 분리를 같은 세션에서 동시에 하지 않는다.

1-12. C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoomProjectiles.cpp

다음 세션 분리 원자:

```text
ServerProjectileAuthority
```

첫 세션에서는 수정하지 않는다.

금지:
- projectile server hit validation과 client visual FX cue를 섞지 않는다.
- event/cue single-source 원칙을 깨지 않는다.

1-13. C:/Users/tnest/Desktop/Winters/Server/Include/Server.vcxproj

계획서에 XML 변경을 쓰지 않는다.

확인 필요:
- `LobbyAuthority.h/.cpp`를 실제로 추가하는 세션에서 project inclusion을 확인한다.
- CMake 경로가 활성 빌드에 쓰이면 `CMakeLists.txt` inclusion도 확인한다.

2. 검증

미검증:
- C++ 구현 미실행.
- `LobbyAuthority.h/.cpp` 프로젝트 포함 미검증.
- Server 빌드 미검증.
- Client/Server runtime smoke 미검증.

검증 명령:
- git diff --check
- msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64

수동 확인:
- lobby join/leave가 기존과 같은 packet을 보낸다.
- champion select, bot edit, ready, loading, in-game 전환이 기존과 같다.
- game start 후 `GameRoomTick.cpp` tick 순서가 바뀌지 않는다.
- snapshot/event/replay emission 순서가 바뀌지 않는다.
- Client visual 성공이 아니라 Server snapshot/event/cue emission으로 authority 변경을 검증한다.

확인 필요:
- `Server/Include/Server.vcxproj`와 `Server/Include/Server.vcxproj.filters` 포함 여부를 확인한다.
- 새 `.h/.cpp`가 빌드 프로젝트에 포함되는지 확인한다.
- `LobbySlotState`를 public type으로 유지할지 internal type으로 감출지 확인한다.
- `GameRoomSmokeRoster`가 lobby type을 직접 참조하는 경계를 유지할지 `LobbyAuthority` helper로 흡수할지 확인한다.
