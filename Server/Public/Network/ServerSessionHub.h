#pragma once

#include "Shared/Network/PacketType.h"
#include "WintersTypes.h"

#include <memory>
#include <string>

class CGameRoom;
class CUdpIocpCore;

enum class eServerSessionTransport : u8_t
{
    Tcp = 0,
    Udp,
};

enum class eServerFrameSendResult : u8_t
{
    Queued = 0,
    GateClosed,
    UnknownSession,
    SessionClosed,
    InvalidFrame,
    TransportBackpressure,
    TransportFailure,
};

struct ServerSessionHubMetrics
{
    u64_t activeTcpSessions = 0;
    u64_t activeUdpSessions = 0;
    u64_t queuedIngressEvents = 0;
    u64_t queuedIngressBytes = 0;
    u64_t droppedStaleFrames = 0;
    u64_t ingressOverflowDisconnects = 0;
    u64_t rejectedOutboundFrames = 0;
    u64_t inFlightOutboundCalls = 0;
};

struct ServerSessionIdentity
{
    std::string matchID;
    std::string userID;
    std::string gameSessionID;
    u64_t expiresAtUnix = 0;
    bool_t bAuthenticated = false;
};

// Owns transport-neutral logical session identity and the UDP callback/tick
// handoff. TCP keeps its existing IOCP receive path, while both transports use
// SendFrame and the same command-sequence/suspicion state.
class CServerSessionHub final
{
public:
    static CServerSessionHub& Instance();

    // Attach must happen before the selected transport starts accepting peers.
    // Passing nullptr keeps the legacy TCP-only runtime.
    bool Attach(CGameRoom& room, CUdpIocpCore* pUdpCore = nullptr);

    // Closes ingress/outbound admission and waits for already-admitted sends.
    // It never stops a socket core and is safe to call before producer threads
    // are joined.
    void BeginShutdown();

    // The caller must call BeginShutdown and stop the attached UDP core before
    // Detach. Detach clears callbacks and all process-local logical-session
    // state.
    bool Detach();

    // TCP sessions use the same monotonic logical-id allocator as UDP peers.
    // Returns zero after the lifecycle gate has closed.
    u32_t RegisterTcpSession();
    void OnTcpDisconnected(u32_t sessionId);

    // applicationSequence is the legacy TCP envelope sequence. UDP owns a
    // per-lane transport message sequence; application values that affect
    // authority must therefore remain in the schema payload.
    eServerFrameSendResult SendFrame(
        u32_t sessionId,
        ePacketType type,
        u32_t applicationSequence,
        const u8_t* payload,
        u32_t payloadSize);

    bool TryAcceptCommandSequence(
        u32_t sessionId,
        u32_t sequence,
        bool& bSuspicious);
    void FlagSuspicious(u32_t sessionId);
    bool IsSuspicious(u32_t sessionId) const;
    bool IsSessionActive(u32_t sessionId) const;
    bool TryGetAuthenticatedIdentity(
        u32_t sessionId,
        ServerSessionIdentity& outIdentity) const;
    bool IsIngressOpen() const;

    // Called by CGameRoom::Tick before m_stateMutex is acquired. UDP callbacks
    // only append to the bounded queue; all room mutations happen here.
    void DrainIngress(CGameRoom& room, u32_t maxEvents = 512u);

    ServerSessionHubMetrics GetMetrics() const;

private:
    CServerSessionHub();
    ~CServerSessionHub();

    CServerSessionHub(const CServerSessionHub&) = delete;
    CServerSessionHub& operator=(const CServerSessionHub&) = delete;

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
