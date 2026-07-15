Session - UDP socket 없이 TCP 동작을 보존하며 Server outbound와 command admission을 transport-neutral 경계로 절단한다.

> [!IMPORTANT]
> **Historical implementation plan / as of 2026-07-12.** 아래 본문을 현재 구현 상태로 사용하지 않는다. 최신 기준은 [2026-07-13 canonical implementation plan](2026-07-13_UDP_JOB_SYSTEM_CHASE_LEV_FIBER_IMPLEMENTATION_PLAN.md)과 [S023 결과 보고서](../build/2026-07-13_UDP_JOB_SYSTEM_CHASE_LEV_FIBER_RESULT.md)다.
> As-built delta: JobSystem Submit race, Chase-Lev deque, FiberFull 및 stress 구현은 완료되었고, UDP v3 generic vertical slice와 server hub/client facade가 구현되었다. main F5 통합과 최종 build 상태는 S023 결과 보고서를 따른다. 6주 Fiber mastery 프로그램은 미착수이며, 현재 상태는 production UDP cutover가 아니다.
> 과거 UDP v2 수치인 **24 B header / 10 B fragment header / 1 MiB logical payload**는 historical design이다. 실제 v3 상수는 **40 B header / 16 B fragment header / 1,200 B datagram / 64 KiB logical payload**다.

## 1. 반영해야 하는 코드

### 1-1. C:/Users/user/Desktop/Winters/Shared/Network/PacketType.h

새 파일:

```cpp
#pragma once

#include <cstdint>

enum class ePacketType : std::uint16_t
{
    None = 0,
    CommandBatch = 1,
    Snapshot = 2,
    Event = 3,
    Hello = 10,
    Heartbeat = 11,
    Disconnect = 12,
    LobbyCommand = 20,
    LobbyState = 21,
    GameStart = 22,
};
```

### 1-2. C:/Users/user/Desktop/Winters/Shared/Network/PacketEnvelope.h

아래 기존 코드:

```cpp
#include "WintersTypes.h"

#include <cstdint>
#include <cstring>
#include <vector>

constexpr uint16_t kPacketMagic = 0x5742;       // 'WB' Winters Binary
constexpr uint16_t kPacketVersion = 1;
constexpr uint32_t kMaxPacketPayloadSize = 64u * 1024u;

enum class ePacketType : uint16_t
{
    None = 0,
    CommandBatch = 1,
    Snapshot = 2,
    Event = 3,
    Hello = 10,
    Heartbeat = 11,
    Disconnect = 12,
    LobbyCommand = 20,
    LobbyState = 21,
    GameStart = 22,
};
```

아래로 교체:

```cpp
#include "Shared/Network/PacketType.h"
#include "WintersTypes.h"

#include <cstdint>
#include <cstring>
#include <vector>

constexpr uint16_t kPacketMagic = 0x5742;       // 'WB' Winters Binary
constexpr uint16_t kPacketVersion = 1;
constexpr uint32_t kMaxPacketPayloadSize = 64u * 1024u;
```

### 1-3. C:/Users/user/Desktop/Winters/Server/Public/Network/IServerOutbound.h

새 파일:

```cpp
#pragma once

#include "Shared/Network/PacketType.h"
#include "WintersTypes.h"

class IServerOutbound
{
public:
    virtual ~IServerOutbound() = default;

    // 구현체는 호출이 반환되기 전에 payload를 복사하거나 수명을 소유해야 한다.
    virtual bool SendToRecipient(
        u32_t recipientId,
        ePacketType type,
        u32_t sequence,
        const u8_t* payload,
        u32_t payloadSize) = 0;
};
```

### 1-4. C:/Users/user/Desktop/Winters/Server/Public/Game/CommandSequenceGate.h

새 파일:

```cpp
#pragma once

#include "WintersTypes.h"

class ICommandSequenceGate
{
public:
    virtual ~ICommandSequenceGate() = default;

    virtual bool TryAcceptSequence(u32_t sequence, bool& bSuspicious) = 0;
    virtual void FlagSuspicious() = 0;
};
```

### 1-5. C:/Users/user/Desktop/Winters/Server/Public/Network/Session.h

아래 기존 코드:

```cpp
#include "ECS/Entity.h"
#include "Network/FrameParser.h"
#include "Network/IOCPCore.h"
#include "WintersTypes.h"
```

아래로 교체:

```cpp
#include "ECS/Entity.h"
#include "Game/CommandSequenceGate.h"
#include "Network/FrameParser.h"
#include "Network/IOCPCore.h"
#include "WintersTypes.h"
```

아래 기존 코드:

```cpp
class CSession : public std::enable_shared_from_this<CSession>
```

아래로 교체:

```cpp
class CSession : public std::enable_shared_from_this<CSession>,
    public ICommandSequenceGate
```

`class CSession`의 public 영역에서 아래 기존 코드:

```cpp
    static std::shared_ptr<CSession> Create(SOCKET socket, u32_t sessionId);
    ~CSession();
```

아래로 교체:

```cpp
    static std::shared_ptr<CSession> Create(SOCKET socket, u32_t sessionId);
    ~CSession() override;
```

아래 기존 코드:

```cpp
    bool TryAcceptSequence(u32_t seq, bool& bSuspicious);

    void FlagSuspicious() { ++m_suspicionCount; }
    bool IsSuspicious() const { return m_suspicionCount > 5; }
```

아래로 교체:

```cpp
    bool TryAcceptSequence(u32_t seq, bool& bSuspicious) override;

    void FlagSuspicious() override { ++m_suspicionCount; }
    bool IsSuspicious() const { return m_suspicionCount > 5; }
```

### 1-6. C:/Users/user/Desktop/Winters/Server/Public/Network/Session_Manager.h

아래 기존 코드:

```cpp
#include "Network/Session.h"
```

아래로 교체:

```cpp
#include "Network/IServerOutbound.h"
#include "Network/Session.h"
```

아래 기존 코드:

```cpp
class CSession_Manager
```

아래로 교체:

```cpp
class CSession_Manager : public IServerOutbound
```

`class CSession_Manager`의 public 영역에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    void OnRecvComplete(u32_t sessionId, const u8_t* bytes, u32_t len);
    void OnSendComplete(u32_t sessionId, u32_t bytes);
```

아래에 추가:

```cpp
    bool SendToRecipient(
        u32_t recipientId,
        ePacketType type,
        u32_t sequence,
        const u8_t* payload,
        u32_t payloadSize) override;
```

### 1-7. C:/Users/user/Desktop/Winters/Server/Private/Network/Session_Manager.cpp

아래 기존 코드:

```cpp
#include "Game/GameRoom.h"
#include "Network/PacketDispatcher.h"
```

아래로 교체:

```cpp
#include "Game/GameRoom.h"
#include "Network/PacketDispatcher.h"
#include "Shared/Network/PacketEnvelope.h"
```

`CSession_Manager::OnSendComplete` 바로 아래에 추가:

기존 코드:

```cpp
void CSession_Manager::OnSendComplete(u32_t sessionId, u32_t bytes)
{
    auto pSession = Find(sessionId);
    if (pSession)
        pSession->OnSendComplete(bytes);
    ReapClosingSessions();
}
```

아래에 추가:

```cpp
bool CSession_Manager::SendToRecipient(
    u32_t recipientId,
    ePacketType type,
    u32_t sequence,
    const u8_t* payload,
    u32_t payloadSize)
{
    auto pSession = Find(recipientId);
    if (!pSession)
        return false;

    return pSession->Send(
        WrapEnvelope(type, sequence, payload, payloadSize));
}
```

### 1-8. C:/Users/user/Desktop/Winters/Server/Public/Game/CommandIngress.h

아래 기존 코드:

```cpp
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "WintersTypes.h"

#include <mutex>
#include <vector>

class CSession;
namespace Shared::Schema { struct CommandBatch; }
```

아래로 교체:

```cpp
#include "Game/CommandSequenceGate.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "WintersTypes.h"

#include <mutex>
#include <vector>

namespace Shared::Schema { struct CommandBatch; }
```

`CCommandIngress::AcceptCommandBatch`의 아래 기존 코드:

```cpp
    void AcceptCommandBatch(
        u32_t sessionId,
        const Shared::Schema::CommandBatch* batch,
        u64_t acceptedTick,
        u64_t recvTimeMs,
        CSession& session);
```

아래로 교체:

```cpp
    void AcceptCommandBatch(
        u32_t sessionId,
        const Shared::Schema::CommandBatch* batch,
        u64_t acceptedTick,
        u64_t recvTimeMs,
        ICommandSequenceGate& sequenceGate);
```

### 1-9. C:/Users/user/Desktop/Winters/Server/Private/Game/CommandIngress.cpp

삭제할 코드:

```cpp
#include "Network/Session.h"
```

`CCommandIngress::AcceptCommandBatch`의 아래 기존 코드:

```cpp
void CCommandIngress::AcceptCommandBatch(
    u32_t sessionId,
    const Shared::Schema::CommandBatch* batch,
    u64_t acceptedTick,
    u64_t recvTimeMs,
    CSession& session)
```

아래로 교체:

```cpp
void CCommandIngress::AcceptCommandBatch(
    u32_t sessionId,
    const Shared::Schema::CommandBatch* batch,
    u64_t acceptedTick,
    u64_t recvTimeMs,
    ICommandSequenceGate& sequenceGate)
```

같은 함수 안의 아래 기존 코드:

```cpp
        const u32_t seq = packet->sequenceNum();
        bool bSuspicious = false;
        if (!session.TryAcceptSequence(seq, bSuspicious))
        {
            if (bSuspicious)
                session.FlagSuspicious();
            continue;
        }
```

아래로 교체:

```cpp
        const u32_t seq = packet->sequenceNum();
        bool bSuspicious = false;
        if (!sequenceGate.TryAcceptSequence(seq, bSuspicious))
        {
            if (bSuspicious)
                sequenceGate.FlagSuspicious();
            continue;
        }
```

### 1-10. C:/Users/user/Desktop/Winters/Server/Public/Game/GameRoom.h

아래 기존 코드:

```cpp
class CSnapshotBuilder;
class CLagCompensation;
//Replay
class CReplayRecorder;
```

아래로 교체:

```cpp
class CSnapshotBuilder;
class CLagCompensation;
class IServerOutbound;
//Replay
class CReplayRecorder;
```

아래 기존 코드:

```cpp
    static std::unique_ptr<CGameRoom> Create(u32_t roomId);
```

아래로 교체:

```cpp
    static std::unique_ptr<CGameRoom> Create(
        u32_t roomId,
        IServerOutbound& outbound);
```

아래 기존 코드:

```cpp
    void OnCommandBatch(u32_t sessionId, const Shared::Schema::CommandBatch* batch);
```

아래로 교체:

```cpp
    void OnCommandBatch(
        u32_t sessionId,
        const Shared::Schema::CommandBatch* batch,
        ICommandSequenceGate& sequenceGate);
```

private 영역의 아래 기존 코드:

```cpp
    CGameRoom(u32_t roomId);
```

아래로 교체:

```cpp
    CGameRoom(u32_t roomId, IServerOutbound& outbound);
```

아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    u32_t m_roomId = 0;
```

아래에 추가:

```cpp
    IServerOutbound& m_outbound;
```

### 1-11. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp

삭제할 코드:

```cpp
#include "Network/Session.h"
#include "Network/Session_Manager.h"
```

삭제할 코드:

```cpp
#include "Shared/Network/PacketEnvelope.h"
```

아래 기존 코드:

```cpp
std::unique_ptr<CGameRoom> CGameRoom::Create(u32_t roomId)
{
    auto room = std::unique_ptr<CGameRoom>(new CGameRoom(roomId));
    room->m_pExecutor = CDefaultCommandExecutor::Create();
    room->m_pSnapBuilder = CSnapshotBuilder::Create();
    room->m_pLagCompensation = std::make_unique<CLagCompensation>();
    room->m_pReplayRecorder = CReplayRecorder::Create(roomId, 30);
    room->InitializeServerSimSystems();
    return room;
}

CGameRoom::CGameRoom(u32_t roomId)
    : m_roomId(roomId)
{
    InitializeLobbyAuthority();
}
```

아래로 교체:

```cpp
std::unique_ptr<CGameRoom> CGameRoom::Create(
    u32_t roomId,
    IServerOutbound& outbound)
{
    auto room = std::unique_ptr<CGameRoom>(
        new CGameRoom(roomId, outbound));
    room->m_pExecutor = CDefaultCommandExecutor::Create();
    room->m_pSnapBuilder = CSnapshotBuilder::Create();
    room->m_pLagCompensation = std::make_unique<CLagCompensation>();
    room->m_pReplayRecorder = CReplayRecorder::Create(roomId, 30);
    room->InitializeServerSimSystems();
    return room;
}

CGameRoom::CGameRoom(u32_t roomId, IServerOutbound& outbound)
    : m_roomId(roomId),
      m_outbound(outbound)
{
    InitializeLobbyAuthority();
}
```

### 1-12. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomCommands.cpp

삭제할 코드:

```cpp
#include "Network/Session.h"
#include "Network/Session_Manager.h"
```

아래 기존 코드:

```cpp
void CGameRoom::OnCommandBatch(u32_t sessionId, const Shared::Schema::CommandBatch* batch)
{
    if (!batch)
        return;

    auto pSession = CSession_Manager::Get()->Find(sessionId);
    if (!pSession)
        return;

    const u64_t acceptedTick = GetCurrentTickIndex();
    const u64_t recvMs = static_cast<u64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    m_commandIngress.AcceptCommandBatch(
        sessionId,
        batch,
        acceptedTick,
        recvMs,
        *pSession);
}
```

아래로 교체:

```cpp
void CGameRoom::OnCommandBatch(
    u32_t sessionId,
    const Shared::Schema::CommandBatch* batch,
    ICommandSequenceGate& sequenceGate)
{
    if (!batch)
        return;

    const u64_t acceptedTick = GetCurrentTickIndex();
    const u64_t recvMs = static_cast<u64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    m_commandIngress.AcceptCommandBatch(
        sessionId,
        batch,
        acceptedTick,
        recvMs,
        sequenceGate);
}
```

### 1-13. C:/Users/user/Desktop/Winters/Server/Private/Network/PacketDispatcher.cpp

아래 기존 코드:

```cpp
void CPacketDispatcher::DispatchCommandBatch(u32_t sessionId, const ParsedFrameOwned& frame)
{
    flatbuffers::Verifier verifier(frame.payload.data(), frame.payload.size());
    if (!Shared::Schema::VerifyCommandBatchBuffer(verifier))
    {
        if (auto pSession = CSession_Manager::Get()->Find(sessionId))
            pSession->FlagSuspicious();
        return;
    }

    const auto* batch = Shared::Schema::GetCommandBatch(frame.payload.data());

    CGameRoom* pRoom = nullptr;
    {
        std::lock_guard lk(m_mutex);
        auto routeIt = m_sessionToRoom.find(sessionId);
        if (routeIt == m_sessionToRoom.end())
            return;

        auto roomIt = m_rooms.find(routeIt->second);
        if (roomIt == m_rooms.end())
            return;

        pRoom = roomIt->second;
    }

    if (pRoom)
        pRoom->OnCommandBatch(sessionId, batch);
}
```

아래로 교체:

```cpp
void CPacketDispatcher::DispatchCommandBatch(u32_t sessionId, const ParsedFrameOwned& frame)
{
    flatbuffers::Verifier verifier(frame.payload.data(), frame.payload.size());
    if (!Shared::Schema::VerifyCommandBatchBuffer(verifier))
    {
        if (auto pSession = CSession_Manager::Get()->Find(sessionId))
            pSession->FlagSuspicious();
        return;
    }

    const auto* batch = Shared::Schema::GetCommandBatch(frame.payload.data());

    auto pSession = CSession_Manager::Get()->Find(sessionId);
    if (!pSession)
        return;

    CGameRoom* pRoom = nullptr;
    {
        std::lock_guard lk(m_mutex);
        auto routeIt = m_sessionToRoom.find(sessionId);
        if (routeIt == m_sessionToRoom.end())
            return;

        auto roomIt = m_rooms.find(routeIt->second);
        if (roomIt == m_rooms.end())
            return;

        pRoom = roomIt->second;
    }

    if (pRoom)
        pRoom->OnCommandBatch(sessionId, batch, *pSession);
}
```

### 1-14. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomLobby.cpp

아래 기존 코드:

```cpp
#include "GameRoomInternal.h"
#include "Network/PacketDispatcher.h"
#include "Network/Session.h"
#include "Network/Session_Manager.h"
#include "Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.h"
#include "Shared/Network/PacketEnvelope.h"
```

아래로 교체:

```cpp
#include "GameRoomInternal.h"
#include "Network/IServerOutbound.h"
#include "Network/PacketDispatcher.h"
#include "Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.h"
```

`CGameRoom::BroadcastLobbyStateLocked` 안의 아래 기존 코드:

```cpp
    const auto packet = WrapEnvelope(
        ePacketType::LobbyState,
        m_pLobbyAuthority->GetRevision(),
        buffer.data(),
        static_cast<u32_t>(buffer.size()));

    for (u32_t sid : m_sessionIds)
    {
        if (auto pSession = CSession_Manager::Get()->Find(sid))
            pSession->Send(std::vector<u8_t>(packet.begin(), packet.end()));
    }
```

아래로 교체:

```cpp
    const u32_t revision = m_pLobbyAuthority->GetRevision();
    for (u32_t sid : m_sessionIds)
    {
        m_outbound.SendToRecipient(
            sid,
            ePacketType::LobbyState,
            revision,
            buffer.data(),
            static_cast<u32_t>(buffer.size()));
    }
```

아래 기존 코드:

```cpp
void CGameRoom::BroadcastGameStartLocked()
{
    const u32_t revision = m_pLobbyAuthority ? m_pLobbyAuthority->GetRevision() : 0u;
    const auto packet = WrapEnvelope(ePacketType::GameStart, revision, nullptr, 0);
    for (u32_t sid : m_sessionIds)
    {
        if (auto pSession = CSession_Manager::Get()->Find(sid))
            pSession->Send(std::vector<u8_t>(packet.begin(), packet.end()));
    }
}
```

아래로 교체:

```cpp
void CGameRoom::BroadcastGameStartLocked()
{
    const u32_t revision = m_pLobbyAuthority ? m_pLobbyAuthority->GetRevision() : 0u;
    for (u32_t sid : m_sessionIds)
    {
        m_outbound.SendToRecipient(
            sid,
            ePacketType::GameStart,
            revision,
            nullptr,
            0);
    }
}
```

아래 기존 코드:

```cpp
void CGameRoom::SendGameStartToSessionLocked(u32_t sessionId)
{
    auto pSession = CSession_Manager::Get()->Find(sessionId);
    if (!pSession)
        return;

    const u32_t revision = m_pLobbyAuthority ? m_pLobbyAuthority->GetRevision() : 0u;
    pSession->Send(WrapEnvelope(ePacketType::GameStart, revision, nullptr, 0));
}
```

아래로 교체:

```cpp
void CGameRoom::SendGameStartToSessionLocked(u32_t sessionId)
{
    const u32_t revision = m_pLobbyAuthority ? m_pLobbyAuthority->GetRevision() : 0u;
    m_outbound.SendToRecipient(
        sessionId,
        ePacketType::GameStart,
        revision,
        nullptr,
        0);
}
```

`CGameRoom::SendHelloToSessionLocked` 안의 아래 기존 코드 삭제:

```cpp
    auto pSession = CSession_Manager::Get()->Find(sessionId);
    if (!pSession)
        return;
```

같은 함수 끝의 아래 기존 코드:

```cpp
    const u32_t revision = m_pLobbyAuthority ? m_pLobbyAuthority->GetRevision() : 0u;
    auto packet = WrapEnvelope(
        ePacketType::Hello,
        revision,
        helloBuffer.data(),
        static_cast<u32_t>(helloBuffer.size()));
    pSession->Send(std::move(packet));
```

아래로 교체:

```cpp
    const u32_t revision = m_pLobbyAuthority ? m_pLobbyAuthority->GetRevision() : 0u;
    m_outbound.SendToRecipient(
        sessionId,
        ePacketType::Hello,
        revision,
        helloBuffer.data(),
        static_cast<u32_t>(helloBuffer.size()));
```

### 1-15. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomReplication.cpp

아래 기존 코드:

```cpp
#include "Game/ReplicationEmitter.h"
#include "Game/ReplayRecorder.h"
#include "Game/SnapshotBuilder.h"
#include "Network/Session.h"
#include "Network/Session_Manager.h"
#include "Shared/Network/PacketEnvelope.h"
```

아래로 교체:

```cpp
#include "Game/ReplicationEmitter.h"
#include "Game/ReplayRecorder.h"
#include "Game/SnapshotBuilder.h"
#include "Network/IServerOutbound.h"
```

아래 기존 코드:

```cpp
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
```

아래로 교체:

```cpp
#include <iostream>
#include <sstream>
#include <string>
```

`CGameRoom::BroadcastEventPayload` 안의 아래 기존 코드:

```cpp
    for (u32_t sid : m_sessionIds)
    {
        auto pSession = CSession_Manager::Get()->Find(sid);
        if (!pSession)
            continue;
        if (IsInGamePhase() &&
            !m_sessionBinding.HasBinding(sid))
        {
            continue;
        }

        auto packet = WrapEnvelope(ePacketType::Event, sequence, payload, payloadSize);
        pSession->Send(std::move(packet));
    }
```

아래로 교체:

```cpp
    for (u32_t sid : m_sessionIds)
    {
        if (IsInGamePhase() &&
            !m_sessionBinding.HasBinding(sid))
        {
            continue;
        }

        m_outbound.SendToRecipient(
            sid,
            ePacketType::Event,
            sequence,
            payload,
            payloadSize);
    }
```

`CGameRoom::Phase_BroadcastSnapshot`의 session loop 안에서 삭제할 코드:

```cpp
        auto pSession = CSession_Manager::Get()->Find(sid);
        if (!pSession)
            continue;
```

같은 loop 끝의 아래 기존 코드:

```cpp
        auto packet = WrapEnvelope(
            ePacketType::Snapshot,
            static_cast<u32_t>(tc.tickIndex),
            snapshot.data(),
            static_cast<u32_t>(snapshot.size()));
        pSession->Send(std::move(packet));
```

아래로 교체:

```cpp
        m_outbound.SendToRecipient(
            sid,
            ePacketType::Snapshot,
            static_cast<u32_t>(tc.tickIndex),
            snapshot.data(),
            static_cast<u32_t>(snapshot.size()));
```

### 1-16. C:/Users/user/Desktop/Winters/Server/Private/main.cpp

아래 기존 코드:

```cpp
#include "Game/GameRoom.h"
#include "Network/IOCPCore.h"
#include "Network/PacketDispatcher.h"
```

아래로 교체:

```cpp
#include "Game/GameRoom.h"
#include "Network/IOCPCore.h"
#include "Network/PacketDispatcher.h"
#include "Network/Session_Manager.h"
```

아래 기존 코드:

```cpp
    auto room = CGameRoom::Create(1);
```

아래로 교체:

```cpp
    auto room = CGameRoom::Create(
        1,
        *CSession_Manager::Get());
```

## 2. 검증

미검증:

- 현재 계획은 코드와 anchor만 확인했으며 구현·빌드는 수행하지 않음.
- TCP packet byte parity와 실제 BanPick/InGame 왕복은 미검증.
- disconnect와 Snapshot build가 겹칠 때 inactive recipient에 한 번 더 Snapshot을 build한 뒤 gateway send가 실패할 수 있는 성능 차이는 확인 필요.

검증 명령:

- `git diff --check`
- `rg -n 'WrapEnvelope|CSession_Manager::Get|#include "Network/(Session|Session_Manager)\.h"' Server/Private/Game Server/Public/Game`
- `rg -n '#include "Network/Session.h"|CSession\b' Server/Private/Game/CommandIngress.cpp Server/Public/Game/CommandIngress.h`
- `rg -n 'SOCK_DGRAM|IPPROTO_UDP|WSARecvFrom|WSASendTo|recvfrom|sendto' Server Client Shared`
- `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Server\Include\Server.vcxproj /m /t:Build /p:Configuration=Debug /p:Platform=x64 /nologo /v:minimal`
- `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Client\Include\Client.vcxproj /m /t:Build /p:Configuration=Debug /p:Platform=x64 /nologo /v:minimal`

수동 확인:

- 기존 TCP로 접속해 LobbyState, GameStart, Hello, CommandBatch, Event, Snapshot이 이전과 동일한 type·sequence·FlatBuffers payload로 왕복하는지 확인.
- `PacketHeader`의 16바이트 `static_assert`와 기존 `WrapEnvelope` 결과가 유지되는지 확인.
- command sequence rejection, suspicion count, pending Move coalescing, `lastAckedCommandSeq`가 이전과 동일한지 확인.
- TCP 1-client smoke의 Snapshot/Event output record 수와 별도 authoritative state 결과가 변경 전과 동일한지 확인.
- `GameRoomLobby.cpp`, `GameRoomReplication.cpp`, `GameRoomCommands.cpp`, `CommandIngress.cpp`에서 concrete `CSession`/`CSession_Manager` include와 lookup이 0인지 확인.
- `CSession_Manager`만 TCP `WrapEnvelope`와 per-session `Send`를 연결하는 outbound 구현체인지 확인.

확인 필요:

- 새 header 3개가 Visual Studio browsing project와 `.vcxproj.filters`에 표시되는지 확인.
- 현재 `.wrpl`은 command/input과 state hash를 기록하지 않으므로 단독 parity oracle로 쓰지 않는다. canonical command trace/state hash harness는 UDP 전환 전 후속 Stage 0 계측으로 추가한다.
- 이번 slice에서는 join/leave/lobby mailbox와 IOCP worker의 `m_stateMutex` 접근을 변경하지 않는다.
- full `RoomIngress`, Client transport interface, UDP socket, reliability, fragmentation, delivery policy, Snapshot DTO/계측은 후속 session으로 남긴다.
