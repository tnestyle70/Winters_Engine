Session - Stage 0 TCP baseline과 bounded queue/shutdown gate 통과 뒤, TCP 동작을 보존한 채 GameRoom outbound와 command ingress에서 concrete TCP Session 의존을 제거한다.

> [!IMPORTANT]
> **Historical implementation plan / as of 2026-07-12.** 아래 본문을 현재 구현 상태로 사용하지 않는다. 최신 기준은 [2026-07-13 canonical implementation plan](2026-07-13_UDP_JOB_SYSTEM_CHASE_LEV_FIBER_IMPLEMENTATION_PLAN.md)과 [S023 결과 보고서](../build/2026-07-13_UDP_JOB_SYSTEM_CHASE_LEV_FIBER_RESULT.md)다.
> As-built delta: JobSystem Submit race, Chase-Lev deque, FiberFull 및 stress 구현은 완료되었고, UDP v3 generic vertical slice와 server hub/client facade가 구현되었다. main F5 통합과 최종 build 상태는 S023 결과 보고서를 따른다. 6주 Fiber mastery 프로그램은 미착수이며, 현재 상태는 production UDP cutover가 아니다.
> 과거 UDP v2 수치인 **24 B header / 10 B fragment header / 1 MiB logical payload**는 historical design이다. 실제 v3 상수는 **40 B header / 16 B fragment header / 1,200 B datagram / 64 KiB logical payload**다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shared/Network/PacketType.h

새 파일:

```cpp
#pragma once

#include <cstdint>

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

1-2. C:/Users/user/Desktop/Winters/Shared/Network/PacketEnvelope.h

기존 코드:

```cpp
#include "WintersTypes.h"
```

아래로 교체:

```cpp
#include "Shared/Network/PacketType.h"
#include "WintersTypes.h"
```

삭제할 코드:

```cpp
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

1-3. C:/Users/user/Desktop/Winters/Shared/Network/PacketSemantics.h

새 파일:

```cpp
#pragma once

#include "Shared/Network/PacketType.h"

#include <cstdint>

enum class PacketDelivery : uint8_t
{
    ReliableOrdered = 0,
    UnreliableSequenced,
};

enum class PacketLane : uint8_t
{
    Invalid = 0,
    Control,
    Heartbeat,
    Command,
    Event,
    Snapshot,
};

struct PacketSemantics
{
    PacketDelivery delivery = PacketDelivery::ReliableOrdered;
    PacketLane lane = PacketLane::Invalid;
};

constexpr PacketSemantics ResolvePacketSemantics(ePacketType type)
{
    switch (type)
    {
    case ePacketType::CommandBatch:
        return {
            PacketDelivery::ReliableOrdered,
            PacketLane::Command,
        };
    case ePacketType::Event:
        return {
            PacketDelivery::ReliableOrdered,
            PacketLane::Event,
        };
    case ePacketType::Snapshot:
        return {
            PacketDelivery::UnreliableSequenced,
            PacketLane::Snapshot,
        };
    case ePacketType::Heartbeat:
        return {
            PacketDelivery::UnreliableSequenced,
            PacketLane::Heartbeat,
        };
    case ePacketType::Hello:
    case ePacketType::Disconnect:
    case ePacketType::LobbyCommand:
    case ePacketType::LobbyState:
    case ePacketType::GameStart:
        return {
            PacketDelivery::ReliableOrdered,
            PacketLane::Control,
        };
    case ePacketType::None:
    default:
        return {};
    }
}
```

1-4. C:/Users/user/Desktop/Winters/Server/Public/Network/IGamePacketTransport.h

새 파일:

```cpp
#pragma once

#include "Shared/Network/PacketType.h"
#include "WintersTypes.h"

enum class GamePacketSendResult : u8_t
{
    Queued = 0,
    NoAssociation,
    QueueFull,
    Closing,
    TransportError,
};

class IGamePacketTransport
{
public:
    virtual ~IGamePacketTransport() = default;

    // 구현체는 반환 전에 payload를 소유하고, 실패 진단과 connection 정책을 적용한다.
    virtual GamePacketSendResult SendToSession(
        u32_t sessionId,
        ePacketType type,
        u32_t applicationSequence,
        const u8_t* payload,
        u32_t payloadSize) = 0;
};
```

1-5. C:/Users/user/Desktop/Winters/Server/Public/Network/TcpGamePacketTransport.h

새 파일:

```cpp
#pragma once

#include "Network/IGamePacketTransport.h"

class CTcpGamePacketTransport final : public IGamePacketTransport
{
public:
    GamePacketSendResult SendToSession(
        u32_t sessionId,
        ePacketType type,
        u32_t applicationSequence,
        const u8_t* payload,
        u32_t payloadSize) override;
};
```

1-6. C:/Users/user/Desktop/Winters/Server/Private/Network/TcpGamePacketTransport.cpp

새 파일:

```cpp
#include "Network/TcpGamePacketTransport.h"

#include "Network/Session.h"
#include "Network/Session_Manager.h"
#include "Shared/Network/PacketEnvelope.h"

#include <utility>

GamePacketSendResult CTcpGamePacketTransport::SendToSession(
    u32_t sessionId,
    ePacketType type,
    u32_t applicationSequence,
    const u8_t* payload,
    u32_t payloadSize)
{
    auto pSession = CSession_Manager::Get()->Find(sessionId);
    if (!pSession)
        return GamePacketSendResult::NoAssociation;

    auto packet = WrapEnvelope(
        type,
        applicationSequence,
        payload,
        payloadSize);
    const SessionSendResult sendResult = pSession->Send(std::move(packet));
    switch (sendResult.status)
    {
    case SessionSendStatus::Queued:
        return GamePacketSendResult::Queued;
    case SessionSendStatus::QueueFull:
        return GamePacketSendResult::QueueFull;
    case SessionSendStatus::Closing:
        return GamePacketSendResult::Closing;
    case SessionSendStatus::InvalidPacket:
    case SessionSendStatus::TransportError:
    default:
        return GamePacketSendResult::TransportError;
    }
}
```

1-7. C:/Users/user/Desktop/Winters/Server/Public/Game/GameRoom.h

아래 기존 include 바로 아래에 추가:

기존 코드:

```cpp
#include "Game/SessionBinding.h"
```

아래에 추가:

```cpp
#include "Shared/Network/PacketType.h"
```

아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
class CSnapshotBuilder;
class CLagCompensation;
//Replay
class CReplayRecorder;
```

아래에 추가:

```cpp
class IGamePacketTransport;
```

아래 기존 코드를:

```cpp
    static std::unique_ptr<CGameRoom> Create(u32_t roomId);
```

아래로 교체:

```cpp
    static std::unique_ptr<CGameRoom> Create(
        u32_t roomId,
        IGamePacketTransport& gamePacketTransport);
```

private 영역의 아래 기존 코드를:

```cpp
    CGameRoom(u32_t roomId);
    static u64_t ResolveServerGameTimeMs(u64_t iServerTick);
```

아래로 교체:

```cpp
    CGameRoom(u32_t roomId, IGamePacketTransport& gamePacketTransport);
    static u64_t ResolveServerGameTimeMs(u64_t iServerTick);

    void SendPacketToSession(
        u32_t sessionId,
        ePacketType type,
        u32_t applicationSequence,
        const u8_t* payload,
        u32_t payloadSize);
```

아래 기존 코드를:

```cpp
    u32_t m_roomId = 0;
    std::atomic<bool> m_bRunning{ false };
```

아래로 교체:

```cpp
    u32_t m_roomId = 0;
    IGamePacketTransport* m_pGamePacketTransport = nullptr;
    std::atomic<bool> m_bRunning{ false };
```

1-8. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp

삭제할 코드:

```cpp
#include "Shared/Network/PacketEnvelope.h"
```

아래 기존 include를:

```cpp
#include "Network/PacketDispatcher.h"
#include "Network/Session.h"
#include "Network/Session_Manager.h"
```

아래로 교체:

```cpp
#include "Network/IGamePacketTransport.h"
```

아래 기존 코드를:

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
    IGamePacketTransport& gamePacketTransport)
{
    auto room = std::unique_ptr<CGameRoom>(
        new CGameRoom(roomId, gamePacketTransport));
    room->m_pExecutor = CDefaultCommandExecutor::Create();
    room->m_pSnapBuilder = CSnapshotBuilder::Create();
    room->m_pLagCompensation = std::make_unique<CLagCompensation>();
    room->m_pReplayRecorder = CReplayRecorder::Create(roomId, 30);
    room->InitializeServerSimSystems();
    return room;
}

CGameRoom::CGameRoom(
    u32_t roomId,
    IGamePacketTransport& gamePacketTransport)
    : m_roomId(roomId),
      m_pGamePacketTransport(&gamePacketTransport)
{
    InitializeLobbyAuthority();
}

void CGameRoom::SendPacketToSession(
    u32_t sessionId,
    ePacketType type,
    u32_t applicationSequence,
    const u8_t* payload,
    u32_t payloadSize)
{
    if (!m_pGamePacketTransport)
    {
        static std::atomic<u32_t> s_missingTransportCount{ 0u };
        if (s_missingTransportCount.fetch_add(1u, std::memory_order_relaxed) < 8u)
        {
            std::cerr << "[GameRoom] missing packet transport sid="
                      << sessionId << " type=" << static_cast<u32_t>(type) << '\n';
        }
        return;
    }

    const GamePacketSendResult result = m_pGamePacketTransport->SendToSession(
        sessionId,
        type,
        applicationSequence,
        payload,
        payloadSize);
    if (result == GamePacketSendResult::Queued)
        return;

    static std::atomic<u32_t> s_sendFailureCount{ 0u };
    if (s_sendFailureCount.fetch_add(1u, std::memory_order_relaxed) < 8u)
    {
        std::cerr << "[GameRoom] packet send failed sid=" << sessionId
                  << " type=" << static_cast<u32_t>(type)
                  << " result=" << static_cast<u32_t>(result) << '\n';
    }
}
```

1-9. C:/Users/user/Desktop/Winters/Server/Private/main.cpp

아래 기존 include 바로 아래에 추가:

기존 코드:

```cpp
#include "Network/IOCPCore.h"
#include "Network/PacketDispatcher.h"
```

아래에 추가:

```cpp
#include "Network/TcpGamePacketTransport.h"
```

아래 기존 코드를:

```cpp
    auto room = CGameRoom::Create(1);
```

아래로 교체:

```cpp
    CTcpGamePacketTransport gamePacketTransport;
    auto room = CGameRoom::Create(1, gamePacketTransport);
```

1-10. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomReplication.cpp

삭제할 코드:

```cpp
#include "Network/Session.h"
#include "Network/Session_Manager.h"
```

삭제할 코드:

```cpp
#include "Shared/Network/PacketEnvelope.h"
```

삭제할 코드:

```cpp
#include <utility>
```

`CGameRoom::BroadcastEventPayload`의 아래 기존 코드를:

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

        SendPacketToSession(
            sid,
            ePacketType::Event,
            sequence,
            payload,
            payloadSize);
    }
```

`CGameRoom::Phase_BroadcastSnapshot`의 아래 기존 코드를:

```cpp
    for (u32_t sid : m_sessionIds)
    {
        auto pSession = CSession_Manager::Get()->Find(sid);
        if (!pSession)
            continue;

        EntityID controlledEntity = NULL_ENTITY;
        if (!m_sessionBinding.TryGet(sid, controlledEntity) || controlledEntity == NULL_ENTITY)
            continue;

        const auto ackIt = m_lastSimCommandSeqBySession.find(sid);
        const u32_t lastSimCommandSeq =
            (ackIt != m_lastSimCommandSeqBySession.end()) ? ackIt->second : 0u;

        auto snapshot = m_pSnapBuilder->Build(
            m_world,
            m_entityMap,
            tc.tickIndex,
            ResolveServerGameTimeMs(tc.tickIndex),
            m_rng.GetState(),
            lastSimCommandSeq,
            m_entityMap.ToNet(controlledEntity));

        auto packet = WrapEnvelope(
            ePacketType::Snapshot,
            static_cast<u32_t>(tc.tickIndex),
            snapshot.data(),
            static_cast<u32_t>(snapshot.size()));
        pSession->Send(std::move(packet));
    }
```

아래로 교체:

```cpp
    for (u32_t sid : m_sessionIds)
    {
        EntityID controlledEntity = NULL_ENTITY;
        if (!m_sessionBinding.TryGet(sid, controlledEntity) || controlledEntity == NULL_ENTITY)
            continue;

        const auto ackIt = m_lastSimCommandSeqBySession.find(sid);
        const u32_t lastSimCommandSeq =
            (ackIt != m_lastSimCommandSeqBySession.end()) ? ackIt->second : 0u;

        auto snapshot = m_pSnapBuilder->Build(
            m_world,
            m_entityMap,
            tc.tickIndex,
            ResolveServerGameTimeMs(tc.tickIndex),
            m_rng.GetState(),
            lastSimCommandSeq,
            m_entityMap.ToNet(controlledEntity));

        SendPacketToSession(
            sid,
            ePacketType::Snapshot,
            static_cast<u32_t>(tc.tickIndex),
            snapshot.data(),
            static_cast<u32_t>(snapshot.size()));
    }
```

1-11. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomLobby.cpp

삭제할 코드:

```cpp
#include "Network/Session.h"
#include "Network/Session_Manager.h"
```

삭제할 코드:

```cpp
#include "Shared/Network/PacketEnvelope.h"
```

`CGameRoom::BroadcastLobbyStateLocked`의 아래 기존 코드를:

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
    for (u32_t sid : m_sessionIds)
    {
        SendPacketToSession(
            sid,
            ePacketType::LobbyState,
            m_pLobbyAuthority->GetRevision(),
            buffer.data(),
            static_cast<u32_t>(buffer.size()));
    }
```

`CGameRoom::BroadcastGameStartLocked` 전체를:

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
        SendPacketToSession(
            sid,
            ePacketType::GameStart,
            revision,
            nullptr,
            0u);
    }
}
```

`CGameRoom::SendGameStartToSessionLocked` 전체를:

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
    SendPacketToSession(
        sessionId,
        ePacketType::GameStart,
        revision,
        nullptr,
        0u);
}
```

`CGameRoom::SendHelloToSessionLocked`에서 아래 기존 코드를:

```cpp
    auto pSession = CSession_Manager::Get()->Find(sessionId);
    if (!pSession)
        return;

    flatbuffers::FlatBufferBuilder fbb(128);
```

아래로 교체:

```cpp
    flatbuffers::FlatBufferBuilder fbb(128);
```

같은 함수의 아래 기존 코드를:

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
    SendPacketToSession(
        sessionId,
        ePacketType::Hello,
        revision,
        helloBuffer.data(),
        static_cast<u32_t>(helloBuffer.size()));
```

`CGameRoom::OnSessionJoin`에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    std::lock_guard stateLock(m_stateMutex);
```

아래에 추가:

```cpp
    m_commandIngress.RegisterSession(sessionId);
```

`CGameRoom::OnSessionLeave`에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    m_sessionBinding.Unbind(sessionId);
```

아래에 추가:

```cpp
    m_commandIngress.RemoveSession(sessionId);
    m_lastSimCommandSeqBySession.erase(sessionId);
```

1-12. C:/Users/user/Desktop/Winters/Server/Public/Game/CommandIngress.h

삭제할 코드:

```cpp
class CSession;
```

아래 include 바로 아래에 추가:

기존 코드:

```cpp
#include <mutex>
#include <vector>
```

아래에 추가:

```cpp
#include <unordered_map>
```

아래 기존 코드를:

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
        u64_t recvTimeMs);

    void RegisterSession(u32_t sessionId);
    void RemoveSession(u32_t sessionId);
```

아래 기존 private 영역을:

```cpp
private:
    std::mutex m_pendingMutex;
    std::vector<PendingCommand> m_pendingCommands;
};
```

아래로 교체:

```cpp
private:
    std::mutex m_mutex;
    std::unordered_map<u32_t, u32_t> m_lastProcessedSeqBySession;
    std::vector<PendingCommand> m_pendingCommands;
};
```

1-13. C:/Users/user/Desktop/Winters/Server/Private/Game/CommandIngress.cpp

삭제할 코드:

```cpp
#include "Network/Session.h"
```

아래 기존 함수 시작부를:

```cpp
void CCommandIngress::AcceptCommandBatch(
    u32_t sessionId,
    const Shared::Schema::CommandBatch* batch,
    u64_t acceptedTick,
    u64_t recvTimeMs,
    CSession& session)
{
```

아래로 교체:

```cpp
void CCommandIngress::AcceptCommandBatch(
    u32_t sessionId,
    const Shared::Schema::CommandBatch* batch,
    u64_t acceptedTick,
    u64_t recvTimeMs)
{
```

같은 함수의 아래 기존 코드를:

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
```

`CCommandIngress::AcceptCommandBatch` 바로 위에 추가:

```cpp
void CCommandIngress::RegisterSession(u32_t sessionId)
{
    std::lock_guard ingressLock(m_mutex);
    m_lastProcessedSeqBySession.try_emplace(sessionId, 0u);
}

void CCommandIngress::RemoveSession(u32_t sessionId)
{
    std::lock_guard ingressLock(m_mutex);
    m_lastProcessedSeqBySession.erase(sessionId);
    m_pendingCommands.erase(
        std::remove_if(
            m_pendingCommands.begin(),
            m_pendingCommands.end(),
            [sessionId](const PendingCommand& pending)
            {
                return pending.sessionId == sessionId;
            }),
        m_pendingCommands.end());
}
```

`CCommandIngress::EnqueueCommand`의 아래 기존 코드를:

```cpp
    std::lock_guard lk(m_pendingMutex);

    PendingCommand pending{};
```

아래로 교체:

```cpp
    std::lock_guard ingressLock(m_mutex);
    auto sequenceIt = m_lastProcessedSeqBySession.find(sessionId);
    if (sequenceIt == m_lastProcessedSeqBySession.end())
        return;

    u32_t& lastProcessedSeq = sequenceIt->second;
    if (wire.sequenceNum <= lastProcessedSeq ||
        wire.sequenceNum > lastProcessedSeq + 60u)
    {
        return;
    }

    lastProcessedSeq = wire.sequenceNum;

    PendingCommand pending{};
```

`CCommandIngress::DrainSorted`의 아래 기존 코드를:

```cpp
    {
        std::lock_guard lk(m_pendingMutex);
        drained.swap(m_pendingCommands);
    }
```

아래로 교체:

```cpp
    {
        std::lock_guard ingressLock(m_mutex);
        drained.swap(m_pendingCommands);
    }
```

1-14. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomCommands.cpp

삭제할 코드:

```cpp
#include "Network/Session.h"
#include "Network/Session_Manager.h"
```

`CGameRoom::OnCommandBatch`의 아래 기존 코드를:

```cpp
    auto pSession = CSession_Manager::Get()->Find(sessionId);
    if (!pSession)
        return;

    const u64_t acceptedTick = GetCurrentTickIndex();
```

아래로 교체:

```cpp
    const u64_t acceptedTick = GetCurrentTickIndex();
```

같은 함수의 아래 기존 코드를:

```cpp
    m_commandIngress.AcceptCommandBatch(
        sessionId,
        batch,
        acceptedTick,
        recvMs,
        *pSession);
```

아래로 교체:

```cpp
    m_commandIngress.AcceptCommandBatch(
        sessionId,
        batch,
        acceptedTick,
        recvMs);
```

1-15. C:/Users/user/Desktop/Winters/Server/Public/Network/Session.h

삭제할 코드:

```cpp
    bool TryAcceptSequence(u32_t seq, bool& bSuspicious);
```

삭제할 코드:

```cpp
    mutable std::mutex m_seqMutex;
    u32_t m_lastProcessedSeq = 0;
```

1-16. C:/Users/user/Desktop/Winters/Server/Private/Network/Session.cpp

삭제할 코드:

```cpp
bool CSession::TryAcceptSequence(u32_t seq, bool& bSuspicious)
{
    std::lock_guard lk(m_seqMutex);
    bSuspicious = false;

    if (seq <= m_lastProcessedSeq)
        return false;

    if (seq > m_lastProcessedSeq + 60)
    {
        bSuspicious = true;
        return false;
    }

    m_lastProcessedSeq = seq;
    return true;
}
```

2. 검증

미검증:
- 위 코드는 계획만 작성했으며 아직 source에 반영하지 않았다.
- 이 문서는 Stage 0 이후 목표 초안이다. `2026-07-12_TCP_BASELINE_STAGE_0_PLAN.md`의 `CONFIRM_NEEDED`를 complete code로 닫고 실제 source에 적용한 뒤 모든 anchor를 재감사하기 전에는 적용하지 않는다.
- `SessionSendStatus/SessionSendResult` mapping과 CommandIngress private block은 Stage 0 최종 API에 맞춰 재생성해야 한다.
- Server/Client build 미검증.
- TCP packet byte parity와 5-client runtime 미검증.
- 이 Stage 1A는 outbound와 command sequence 소유권만 분리한다. IOCP worker의 Join/Leave/Lobby direct call을 owned `RoomIngress`로 바꾸는 Stage 1B는 아직 미반영이다.
- Client `CCommandSerializer -> CClientNetwork` 결합을 transport interface로 바꾸는 Stage 1B는 아직 미반영이다.
- UDP socket, reliability, fragmentation, ticket, Snapshot delta/AOI는 이 단계에서 구현하지 않는다.

검증 명령:
- `git diff --check`
- `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Server\Include\Server.vcxproj /m /t:Build /p:Configuration=Debug /p:Platform=x64 /nologo /v:minimal`
- `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Client\Include\Client.vcxproj /m /t:Build /p:Configuration=Debug /p:Platform=x64 /nologo /v:minimal`
- `rg -n 'CSession_Manager|CSession' Server/Private/Game/GameRoomReplication.cpp Server/Private/Game/GameRoomCommands.cpp Server/Public/Game/CommandIngress.h Server/Private/Game/CommandIngress.cpp`
- `rg -n 'WrapEnvelope' Server/Private/Game/GameRoomReplication.cpp Server/Private/Game/GameRoomLobby.cpp`

수동 확인:
- 기존 TCP LobbyCommand/LobbyState/Hello/GameStart 왕복이 동일하게 동작하는지 확인.
- 기존 TCP CommandBatch가 같은 command sequence로 GameSim에 한 번만 적용되는지 확인.
- Event/Snapshot payload와 outer application sequence가 변경 전과 byte-for-byte 같은지 capture로 확인.
- disconnect 후 같은 process의 새 session이 command sequence 1부터 정상 수락되는지 확인.
- `ResolvePacketSemantics`가 lane/delivery만 정의하고, 이 Stage의 TCP adapter는 runtime route 결정 없이 모든 packet을 legacy TCP로 보내 byte parity를 유지하는지 확인.
- Snapshot마다 application payload 전체 복사/할당이 기존 경로보다 늘지 않고, `Server.Tick.Total` p95/p99가 Stage 0 baseline을 악화하지 않는지 확인.
- session leave 뒤 `m_lastProcessedSeqBySession`, pending command, `m_lastSimCommandSeqBySession`에 해당 session state가 남지 않는지 확인.

확인 필요:
- 이 계획 적용 전에 설계 문서 Stage 0의 TCP 1/5-client baseline, partial-send reproducer, bounded ingress/send/receive queue, pre-game command reject, shutdown drain, queue/tick 측정값을 모두 통과. 하나라도 미완료면 Stage 1A를 시작하지 않음.
- Stage 0 적용 diff를 기준으로 1-6, 1-12, 1-13과 `GameRoomLobby.cpp` leave cleanup anchor를 재작성하고 `CONFIRM_NEEDED`가 0건인지 확인.
- 새 `.h/.cpp` 파일이 `Server/Include/Server.vcxproj`와 `.vcxproj.filters`에 포함되는지 확인.
- `Shared/Network/PacketType.h`와 `Shared/Network/PacketSemantics.h`가 solution/workspace map에서 검색 가능한지 확인.
- Stage 1A parity 통과 후에만 Stage 1B `RoomIngress`와 Client transport interface의 완전한 코드 계획을 작성.
- Stage 1B에서 sequence key를 현재 TCP-lifetime `sessionId`에서 `(roomMemberId, commandStreamGeneration)`으로 바꾸고, reconnect generation 증가와 old-generation drop을 UDP association보다 먼저 검증.
- Stage 2 Snapshot payload 측정 뒤 UDP header, auth library, fragment/reassembly hard cap의 정확한 상수를 확정.
