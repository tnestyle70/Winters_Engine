#include "Network/ServerSessionHub.h"

#include "Game/GameRoom.h"
#include "Network/PacketDispatcher.h"
#include "Network/Session.h"
#include "Network/Session_Manager.h"
#include "Network/UdpIocpCore.h"
#include "Shared/Network/PacketEnvelope.h"
#include "Shared/Network/PacketSemantics.h"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
    constexpr size_t kMaxIngressEvents = 4096u;
    constexpr size_t kIngressControlReserve = 128u;
    constexpr size_t kMaxIngressFrameEvents =
        kMaxIngressEvents - kIngressControlReserve;
    constexpr size_t kMaxIngressBytes = 8u * 1024u * 1024u;
    constexpr size_t kMaxAssociationTombstones = 1024u;
    constexpr size_t kMaxTrackedLogicalSessions = 2048u;
    constexpr u32_t kSuspicionDisconnectThreshold = 5u;

    enum class eSessionLifecycle : u8_t
    {
        Active = 0,
        Tombstone,
    };

    enum class eIngressKind : u8_t
    {
        Connect = 0,
        Frame,
        Disconnect,
    };

    struct AssociationKey
    {
        u64_t connectionId = 0;
        u32_t generation = 0;

        bool operator==(const AssociationKey& rhs) const
        {
            return connectionId == rhs.connectionId &&
                generation == rhs.generation;
        }
    };

    struct AssociationKeyHash
    {
        size_t operator()(const AssociationKey& key) const
        {
            const size_t low = std::hash<u64_t>{}(key.connectionId);
            const size_t high = std::hash<u32_t>{}(key.generation);
            return low ^ (high + static_cast<size_t>(0x9e3779b9u) +
                (low << 6u) + (low >> 2u));
        }
    };

    struct LogicalSession
    {
        eServerSessionTransport transport = eServerSessionTransport::Tcp;
        eSessionLifecycle lifecycle = eSessionLifecycle::Active;
        u64_t connectionId = 0;
        u32_t generation = 0;
        u32_t lastCommandSequence = 0;
        u32_t suspicionCount = 0;
        bool bJoinPublished = false;
    };

    struct AssociationState
    {
        u32_t sessionId = 0;
        eSessionLifecycle lifecycle = eSessionLifecycle::Active;
        bool bDisconnectQueued = false;
    };

    struct IngressEvent
    {
        eIngressKind kind = eIngressKind::Frame;
        u32_t sessionId = 0;
        AssociationKey association{};
        ParsedFrameOwned frame{};
    };

    struct OutboundTarget
    {
        eServerSessionTransport transport = eServerSessionTransport::Tcp;
        u64_t connectionId = 0;
        u32_t generation = 0;
        CUdpIocpCore* pUdpCore = nullptr;
    };
}

struct CServerSessionHub::Impl
{
    class OutboundCallLease final
    {
    public:
        explicit OutboundCallLease(Impl& impl)
            : m_impl(impl)
        {
        }

        ~OutboundCallLease()
        {
            m_impl.CompleteOutboundCall();
        }

    private:
        Impl& m_impl;
    };

    mutable std::mutex mutex;
    std::condition_variable outboundCv;
    CGameRoom* pRoom = nullptr;
    CUdpIocpCore* pUdpCore = nullptr;
    bool bAttached = false;
    bool bIngressOpen = true;
    bool bOutboundOpen = true;
    bool bShutdownBegun = false;
    u32_t nextLogicalSessionId = 1u;
    u64_t inFlightOutboundCalls = 0u;

    std::unordered_map<u32_t, LogicalSession> sessions;
    std::unordered_map<AssociationKey, AssociationState, AssociationKeyHash>
        associations;
    std::deque<AssociationKey> tombstoneOrder;
    std::deque<IngressEvent> ingress;
    size_t ingressFrameEvents = 0u;
    size_t ingressBytes = 0u;

    u64_t droppedStaleFrames = 0u;
    u64_t ingressOverflowDisconnects = 0u;
    u64_t rejectedOutboundFrames = 0u;

    u32_t AllocateSessionIdLocked()
    {
        for (u64_t attempt = 0u; attempt < 0xffffffffull; ++attempt)
        {
            u32_t candidate = nextLogicalSessionId++;
            if (nextLogicalSessionId == 0u)
                nextLogicalSessionId = 1u;
            if (candidate != 0u && sessions.find(candidate) == sessions.end())
                return candidate;
        }
        return 0u;
    }

    void PruneTombstonesLocked()
    {
        while (tombstoneOrder.size() > kMaxAssociationTombstones)
        {
            const AssociationKey key = tombstoneOrder.front();
            tombstoneOrder.pop_front();
            const auto iterator = associations.find(key);
            if (iterator != associations.end() &&
                iterator->second.lifecycle == eSessionLifecycle::Tombstone &&
                sessions.find(iterator->second.sessionId) == sessions.end())
            {
                associations.erase(iterator);
            }
        }
    }

    void MarkTombstoneLocked(
        const AssociationKey& key,
        AssociationState& association)
    {
        if (association.lifecycle == eSessionLifecycle::Tombstone)
            return;

        association.lifecycle = eSessionLifecycle::Tombstone;
        const auto sessionIterator = sessions.find(association.sessionId);
        if (sessionIterator != sessions.end())
            sessionIterator->second.lifecycle = eSessionLifecycle::Tombstone;
        tombstoneOrder.push_back(key);
        PruneTombstonesLocked();
    }

    void QueueDisconnectLocked(
        const AssociationKey& key,
        AssociationState& association)
    {
        if (association.bDisconnectQueued ||
            ingress.size() >= kMaxIngressEvents)
        {
            return;
        }

        association.bDisconnectQueued = true;
        IngressEvent event{};
        event.kind = eIngressKind::Disconnect;
        event.sessionId = association.sessionId;
        event.association = key;
        ingress.push_back(std::move(event));
    }

    void FailAssociationForOverflowLocked(
        const AssociationKey& key,
        AssociationState& association)
    {
        if (association.lifecycle == eSessionLifecycle::Tombstone)
            return;

        MarkTombstoneLocked(key, association);
        ++ingressOverflowDisconnects;

        for (auto iterator = ingress.begin(); iterator != ingress.end();)
        {
            if (iterator->sessionId == association.sessionId &&
                iterator->kind == eIngressKind::Frame)
            {
                ingressBytes -= iterator->frame.payload.size();
                --ingressFrameEvents;
                iterator = ingress.erase(iterator);
            }
            else
            {
                ++iterator;
            }
        }
        QueueDisconnectLocked(key, association);
    }

    void OnUdpConnection(u64_t connectionId, u32_t generation, bool bConnected)
    {
        const AssociationKey key{ connectionId, generation };
        CUdpIocpCore* rejectedCore = nullptr;
        std::unique_lock lock(mutex);

        if (bConnected)
        {
            if (!bAttached || !bIngressOpen)
            {
                rejectedCore = pUdpCore;
            }
            else
            {
                const auto existing = associations.find(key);
                if (existing != associations.end())
                    return;
                if (sessions.size() >= kMaxTrackedLogicalSessions)
                {
                    ++ingressOverflowDisconnects;
                    rejectedCore = pUdpCore;
                }
                else
                {
                    const u32_t sessionId = AllocateSessionIdLocked();
                    if (sessionId == 0u || ingress.size() >= kMaxIngressEvents)
                    {
                        ++ingressOverflowDisconnects;
                        rejectedCore = pUdpCore;
                    }
                    else
                    {
                        LogicalSession session{};
                        session.transport = eServerSessionTransport::Udp;
                        session.connectionId = connectionId;
                        session.generation = generation;
                        sessions.emplace(sessionId, session);

                        AssociationState association{};
                        association.sessionId = sessionId;
                        associations.emplace(key, association);

                        IngressEvent event{};
                        event.kind = eIngressKind::Connect;
                        event.sessionId = sessionId;
                        event.association = key;
                        ingress.push_back(std::move(event));
                        return;
                    }
                }
            }

            lock.unlock();
            if (rejectedCore)
                rejectedCore->DisconnectConnection(connectionId);
            return;
        }

        const auto iterator = associations.find(key);
        if (iterator == associations.end() ||
            iterator->second.lifecycle == eSessionLifecycle::Tombstone)
        {
            return;
        }

        MarkTombstoneLocked(key, iterator->second);
        if (bIngressOpen)
            QueueDisconnectLocked(key, iterator->second);
    }

    void OnUdpFrame(UdpServerInboundFrame incoming)
    {
        const AssociationKey key{
            incoming.connectionId,
            incoming.generation,
        };
        std::lock_guard lock(mutex);

        const auto iterator = associations.find(key);
        if (!bIngressOpen || iterator == associations.end() ||
            iterator->second.lifecycle != eSessionLifecycle::Active)
        {
            ++droppedStaleFrames;
            return;
        }

        if (!IsKnownPacketType(incoming.type) ||
            IsTransportHandshakePacketType(incoming.type) ||
            incoming.payload.size() > kMaxPacketPayloadSize)
        {
            auto sessionIterator = sessions.find(iterator->second.sessionId);
            if (sessionIterator != sessions.end())
                ++sessionIterator->second.suspicionCount;
            return;
        }

        if (ingress.size() >= kMaxIngressEvents ||
            ingressFrameEvents >= kMaxIngressFrameEvents ||
            incoming.payload.size() > kMaxIngressBytes - ingressBytes)
        {
            FailAssociationForOverflowLocked(key, iterator->second);
            return;
        }

        IngressEvent event{};
        event.kind = eIngressKind::Frame;
        event.sessionId = iterator->second.sessionId;
        event.association = key;
        event.frame.type = incoming.type;
        event.frame.sequence = incoming.messageSequence;
        event.frame.payload = std::move(incoming.payload);
        ingressBytes += event.frame.payload.size();
        ++ingressFrameEvents;
        ingress.push_back(std::move(event));
    }

    void CompleteOutboundCall()
    {
        std::lock_guard lock(mutex);
        if (inFlightOutboundCalls > 0u)
            --inFlightOutboundCalls;
        if (inFlightOutboundCalls == 0u)
            outboundCv.notify_all();
    }

    bool IsIngressEventActiveLocked(const IngressEvent& event) const
    {
        if (!bIngressOpen)
            return false;

        const auto associationIterator =
            associations.find(event.association);
        if (associationIterator == associations.end() ||
            associationIterator->second.sessionId != event.sessionId ||
            associationIterator->second.lifecycle !=
                eSessionLifecycle::Active)
        {
            return false;
        }

        const auto sessionIterator = sessions.find(event.sessionId);
        return sessionIterator != sessions.end() &&
            sessionIterator->second.transport ==
                eServerSessionTransport::Udp &&
            sessionIterator->second.lifecycle == eSessionLifecycle::Active &&
            sessionIterator->second.connectionId ==
                event.association.connectionId &&
            sessionIterator->second.generation ==
                event.association.generation;
    }
};

CServerSessionHub& CServerSessionHub::Instance()
{
    static CServerSessionHub instance;
    return instance;
}

CServerSessionHub::CServerSessionHub()
    : m_impl(std::make_unique<Impl>())
{
}

CServerSessionHub::~CServerSessionHub() = default;

bool CServerSessionHub::Attach(CGameRoom& room, CUdpIocpCore* pUdpCore)
{
    {
        std::lock_guard lock(m_impl->mutex);
        if (m_impl->bAttached || m_impl->bShutdownBegun)
            return false;
        m_impl->pRoom = &room;
        m_impl->pUdpCore = pUdpCore;
        m_impl->bAttached = true;
        m_impl->bIngressOpen = true;
        m_impl->bOutboundOpen = true;
    }

    CPacketDispatcher::Instance().RegisterRoom(room.GetRoomId(), &room);

    if (pUdpCore)
    {
        pUdpCore->SetConnectionCallback(
            [this](u64_t connectionId, u32_t generation, bool bConnected)
            {
                m_impl->OnUdpConnection(
                    connectionId,
                    generation,
                    bConnected);
            });
        pUdpCore->SetFrameCallback(
            [this](UdpServerInboundFrame frame)
            {
                m_impl->OnUdpFrame(std::move(frame));
            });
    }
    return true;
}

void CServerSessionHub::BeginShutdown()
{
    std::unique_lock lock(m_impl->mutex);
    m_impl->bShutdownBegun = true;
    m_impl->bIngressOpen = false;
    m_impl->bOutboundOpen = false;
    m_impl->outboundCv.wait(
        lock,
        [this]
        {
            return m_impl->inFlightOutboundCalls == 0u;
        });
}

bool CServerSessionHub::Detach()
{
    CUdpIocpCore* pUdpCore = nullptr;
    CGameRoom* pRoom = nullptr;
    {
        std::lock_guard lock(m_impl->mutex);
        if (!m_impl->bAttached)
            return true;
        if (!m_impl->bShutdownBegun)
            return false;
        if (m_impl->pUdpCore && m_impl->pUdpCore->IsRunning())
            return false;
        pUdpCore = m_impl->pUdpCore;
        pRoom = m_impl->pRoom;
    }

    if (pUdpCore)
    {
        pUdpCore->SetFrameCallback({});
        pUdpCore->SetConnectionCallback({});
    }
    if (pRoom)
    {
        CPacketDispatcher::Instance().UnregisterRoom(
            pRoom->GetRoomId(),
            pRoom);
    }

    std::lock_guard lock(m_impl->mutex);
    m_impl->pRoom = nullptr;
    m_impl->pUdpCore = nullptr;
    m_impl->bAttached = false;
    m_impl->sessions.clear();
    m_impl->associations.clear();
    m_impl->tombstoneOrder.clear();
    m_impl->ingress.clear();
    m_impl->ingressFrameEvents = 0u;
    m_impl->ingressBytes = 0u;
    m_impl->bShutdownBegun = false;
    m_impl->bIngressOpen = false;
    m_impl->bOutboundOpen = false;
    return true;
}

u32_t CServerSessionHub::RegisterTcpSession()
{
    std::lock_guard lock(m_impl->mutex);
    if (!m_impl->bIngressOpen || !m_impl->bOutboundOpen)
        return 0u;
    if (m_impl->sessions.size() >= kMaxTrackedLogicalSessions)
        return 0u;

    const u32_t sessionId = m_impl->AllocateSessionIdLocked();
    if (sessionId == 0u)
        return 0u;

    LogicalSession session{};
    session.transport = eServerSessionTransport::Tcp;
    m_impl->sessions.emplace(sessionId, session);
    return sessionId;
}

void CServerSessionHub::OnTcpDisconnected(u32_t sessionId)
{
    std::lock_guard lock(m_impl->mutex);
    const auto iterator = m_impl->sessions.find(sessionId);
    if (iterator != m_impl->sessions.end() &&
        iterator->second.transport == eServerSessionTransport::Tcp)
    {
        m_impl->sessions.erase(iterator);
    }
}

eServerFrameSendResult CServerSessionHub::SendFrame(
    u32_t sessionId,
    ePacketType type,
    u32_t applicationSequence,
    const u8_t* payload,
    u32_t payloadSize)
{
    if (!IsKnownPacketType(type) ||
        IsTransportHandshakePacketType(type) ||
        (payloadSize != 0u && !payload) ||
        payloadSize > kMaxPacketPayloadSize)
    {
        return eServerFrameSendResult::InvalidFrame;
    }

    OutboundTarget target{};
    {
        std::lock_guard lock(m_impl->mutex);
        if (!m_impl->bOutboundOpen)
        {
            ++m_impl->rejectedOutboundFrames;
            return eServerFrameSendResult::GateClosed;
        }

        const auto iterator = m_impl->sessions.find(sessionId);
        if (iterator == m_impl->sessions.end())
        {
            ++m_impl->rejectedOutboundFrames;
            return eServerFrameSendResult::UnknownSession;
        }
        if (iterator->second.lifecycle != eSessionLifecycle::Active)
        {
            ++m_impl->rejectedOutboundFrames;
            return eServerFrameSendResult::SessionClosed;
        }

        target.transport = iterator->second.transport;
        target.connectionId = iterator->second.connectionId;
        target.generation = iterator->second.generation;
        target.pUdpCore = m_impl->pUdpCore;
        ++m_impl->inFlightOutboundCalls;
    }
    Impl::OutboundCallLease lease(*m_impl);
    const bool bReliable = IsReliableDelivery(
        ResolvePacketSemantics(type).delivery);
    const auto recordRejectedOutbound = [this]()
    {
        std::lock_guard lock(m_impl->mutex);
        ++m_impl->rejectedOutboundFrames;
    };

    try
    {
        if (target.transport == eServerSessionTransport::Tcp)
        {
            auto pSession = CSession_Manager::Get()->Find(sessionId);
            if (!pSession)
            {
                recordRejectedOutbound();
                return eServerFrameSendResult::SessionClosed;
            }
            const bool bQueued = pSession->Send(WrapEnvelope(
                type,
                applicationSequence,
                payload,
                payloadSize));
            if (bQueued)
                return eServerFrameSendResult::Queued;

            recordRejectedOutbound();
            // SendFrame is commonly called while GameRoom owns m_stateMutex.
            // Close the socket here so its outstanding IOCP receive completion
            // performs the manager-level teardown off the room thread; calling
            // Session_Manager::OnDisconnect synchronously would re-enter
            // GameRoom::OnSessionLeave and deadlock that mutex.
            pSession->OnDisconnect();
            return eServerFrameSendResult::TransportFailure;
        }

        if (!target.pUdpCore)
        {
            recordRejectedOutbound();
            return eServerFrameSendResult::TransportFailure;
        }

        const eUdpSendResult result = target.pUdpCore->SendToConnection(
            target.connectionId,
            type,
            payload,
            payloadSize);
        switch (result)
        {
        case eUdpSendResult::Queued:
            return eServerFrameSendResult::Queued;
        case eUdpSendResult::ReliableQueueFull:
        case eUdpSendResult::OutboundQueueFull:
            recordRejectedOutbound();
            if (bReliable)
                target.pUdpCore->DisconnectConnection(target.connectionId);
            return eServerFrameSendResult::TransportBackpressure;
        case eUdpSendResult::NoAssociation:
        case eUdpSendResult::NotConfirmed:
            recordRejectedOutbound();
            return eServerFrameSendResult::SessionClosed;
        case eUdpSendResult::InvalidPacket:
            recordRejectedOutbound();
            return eServerFrameSendResult::InvalidFrame;
        case eUdpSendResult::NotRunning:
        case eUdpSendResult::SocketError:
        default:
            recordRejectedOutbound();
            if (bReliable)
                target.pUdpCore->DisconnectConnection(target.connectionId);
            return eServerFrameSendResult::TransportFailure;
        }
    }
    catch (...)
    {
        recordRejectedOutbound();
        if (target.transport == eServerSessionTransport::Udp &&
            target.pUdpCore && bReliable)
        {
            target.pUdpCore->DisconnectConnection(target.connectionId);
        }
        return eServerFrameSendResult::TransportFailure;
    }
}

bool CServerSessionHub::TryAcceptCommandSequence(
    u32_t sessionId,
    u32_t sequence,
    bool& bSuspicious)
{
    std::lock_guard lock(m_impl->mutex);
    bSuspicious = false;

    const auto iterator = m_impl->sessions.find(sessionId);
    if (iterator == m_impl->sessions.end() ||
        iterator->second.lifecycle != eSessionLifecycle::Active)
        return false;

    LogicalSession& session = iterator->second;
    if (sequence <= session.lastCommandSequence)
        return false;

    if (sequence > session.lastCommandSequence + 60u)
    {
        bSuspicious = true;
        return false;
    }

    session.lastCommandSequence = sequence;
    return true;
}

void CServerSessionHub::FlagSuspicious(u32_t sessionId)
{
    std::lock_guard lock(m_impl->mutex);
    const auto iterator = m_impl->sessions.find(sessionId);
    if (iterator != m_impl->sessions.end())
        ++iterator->second.suspicionCount;
}

bool CServerSessionHub::IsSuspicious(u32_t sessionId) const
{
    std::lock_guard lock(m_impl->mutex);
    const auto iterator = m_impl->sessions.find(sessionId);
    return iterator != m_impl->sessions.end() &&
        iterator->second.suspicionCount > kSuspicionDisconnectThreshold;
}

bool CServerSessionHub::IsSessionActive(u32_t sessionId) const
{
    std::lock_guard lock(m_impl->mutex);
    const auto iterator = m_impl->sessions.find(sessionId);
    return iterator != m_impl->sessions.end() &&
        iterator->second.lifecycle == eSessionLifecycle::Active;
}

bool CServerSessionHub::IsIngressOpen() const
{
    std::lock_guard lock(m_impl->mutex);
    return m_impl->bIngressOpen;
}

void CServerSessionHub::DrainIngress(CGameRoom& room, u32_t maxEvents)
{
    if (maxEvents == 0u)
        return;

    std::vector<IngressEvent> drained;
    CUdpIocpCore* pUdpCore = nullptr;
    {
        std::lock_guard lock(m_impl->mutex);
        if (!m_impl->bAttached || m_impl->pRoom != &room)
            return;

        const size_t eventBudget = static_cast<size_t>(maxEvents);
        drained.reserve(eventBudget);

        // A disconnect control event normally consumes the reserved queue
        // capacity. If adversarial churn filled even that reserve, synthesize
        // the already-recorded tombstone here so a logical session cannot stay
        // joined forever merely because the bounded queue was full.
        for (auto& [associationKey, association] : m_impl->associations)
        {
            if (drained.size() >= eventBudget)
                break;
            if (association.lifecycle != eSessionLifecycle::Tombstone ||
                association.bDisconnectQueued ||
                m_impl->sessions.find(association.sessionId) ==
                    m_impl->sessions.end())
            {
                continue;
            }

            association.bDisconnectQueued = true;
            IngressEvent event{};
            event.kind = eIngressKind::Disconnect;
            event.sessionId = association.sessionId;
            event.association = associationKey;
            drained.push_back(std::move(event));
        }

        const size_t remainingBudget = eventBudget - drained.size();
        const size_t count = (std::min)(
            remainingBudget,
            m_impl->ingress.size());
        for (size_t index = 0u; index < count; ++index)
        {
            IngressEvent event = std::move(m_impl->ingress.front());
            m_impl->ingress.pop_front();
            if (event.kind == eIngressKind::Frame)
            {
                m_impl->ingressBytes -= event.frame.payload.size();
                --m_impl->ingressFrameEvents;
            }
            drained.push_back(std::move(event));
        }
        pUdpCore = m_impl->pUdpCore;
    }

    for (IngressEvent& event : drained)
    {
        switch (event.kind)
        {
        case eIngressKind::Connect:
        {
            bool bActive = false;
            {
                std::lock_guard lock(m_impl->mutex);
                bActive = m_impl->IsIngressEventActiveLocked(event);
            }
            if (bActive)
            {
                room.OnSessionJoin(event.sessionId);
                std::lock_guard lock(m_impl->mutex);
                const auto sessionIterator =
                    m_impl->sessions.find(event.sessionId);
                if (sessionIterator != m_impl->sessions.end())
                    sessionIterator->second.bJoinPublished = true;
            }
            break;
        }
        case eIngressKind::Frame:
        {
            bool bActive = false;
            {
                std::lock_guard lock(m_impl->mutex);
                bActive = m_impl->IsIngressEventActiveLocked(event);
                if (!bActive)
                    ++m_impl->droppedStaleFrames;
            }
            if (bActive)
            {
                CPacketDispatcher::Instance().DispatchFrame(
                    event.sessionId,
                    event.frame);
            }
            break;
        }
        case eIngressKind::Disconnect:
        {
            bool bJoinPublished = false;
            {
                std::lock_guard lock(m_impl->mutex);
                const auto sessionIterator =
                    m_impl->sessions.find(event.sessionId);
                if (sessionIterator != m_impl->sessions.end())
                {
                    bJoinPublished = sessionIterator->second.bJoinPublished;
                    sessionIterator->second.bJoinPublished = false;
                }
            }
            if (bJoinPublished)
            {
                room.OnSessionLeave(event.sessionId);
                CPacketDispatcher::Instance().UnrouteSession(event.sessionId);
            }
            if (pUdpCore)
                pUdpCore->DisconnectConnection(event.association.connectionId);
            {
                std::lock_guard lock(m_impl->mutex);
                const auto sessionIterator =
                    m_impl->sessions.find(event.sessionId);
                if (sessionIterator != m_impl->sessions.end() &&
                    sessionIterator->second.transport ==
                        eServerSessionTransport::Udp)
                {
                    m_impl->sessions.erase(sessionIterator);
                }
                m_impl->PruneTombstonesLocked();
            }
            break;
        }
        default:
            break;
        }
    }
}

ServerSessionHubMetrics CServerSessionHub::GetMetrics() const
{
    std::lock_guard lock(m_impl->mutex);
    ServerSessionHubMetrics result{};
    for (const auto& [sessionId, session] : m_impl->sessions)
    {
        (void)sessionId;
        if (session.lifecycle != eSessionLifecycle::Active)
            continue;
        if (session.transport == eServerSessionTransport::Tcp)
            ++result.activeTcpSessions;
        else
            ++result.activeUdpSessions;
    }
    result.queuedIngressEvents = m_impl->ingress.size();
    result.queuedIngressBytes = m_impl->ingressBytes;
    result.droppedStaleFrames = m_impl->droppedStaleFrames;
    result.ingressOverflowDisconnects =
        m_impl->ingressOverflowDisconnects;
    result.rejectedOutboundFrames = m_impl->rejectedOutboundFrames;
    result.inFlightOutboundCalls = m_impl->inFlightOutboundCalls;
    return result;
}
