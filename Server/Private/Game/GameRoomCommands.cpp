#include "Game/GameRoom.h"

#include "Network/Session.h"
#include "Network/Session_Manager.h"

#include <chrono>
#include <vector>

void CGameRoom::Phase_DrainCommands(TickContext& tc)
{
    std::vector<PendingCommand> drained = m_commandIngress.DrainSorted();

    for (const auto& pending : drained)
    {
        const EntityID controlledEntity = m_sessionBinding.ResolveControlledEntity(
            pending.sessionId,
            m_world,
            m_entityMap,
            m_pLobbyAuthority.get());
        if (controlledEntity == NULL_ENTITY)
            continue;

        GameCommand cmd = BuildServerCommand(
            pending.wire, pending.sessionId, controlledEntity, m_entityMap);
        cmd.issuedAtTick = tc.tickIndex;
        cmd.rewindTicks = 0;

        CCommandIngress::TraceCommandTiming(pending, tc.tickIndex);

        m_pendingExecCommands.push_back(cmd);
        m_lastSimCommandSeqBySession[pending.sessionId] = pending.sequenceNum;
    }
}

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

void CGameRoom::EnqueueCommand(u32_t sessionId, const GameCommandWire& wire,
    u64_t acceptedTick, u64_t recvTimeMs, u64_t clientTimestampMs)
{
    m_commandIngress.EnqueueCommand(
        sessionId,
        wire,
        acceptedTick,
        recvTimeMs,
        clientTimestampMs);
}
