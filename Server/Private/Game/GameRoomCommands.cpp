#include "Game/GameRoom.h"

#include "Network/Session.h"
#include "Network/Session_Manager.h"
#include "Shared/Schemas/Generated/cpp/Command_generated.h"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <vector>

void CGameRoom::Phase_DrainCommands(TickContext& tc)
{
    std::vector<PendingCommand> drained;
    {
        std::lock_guard lk(m_pendingMutex);
        drained.swap(m_pendingCommands);
    }

    std::stable_sort(drained.begin(), drained.end(),
        [](const PendingCommand& lhs, const PendingCommand& rhs)
        {
            if (lhs.acceptedTick != rhs.acceptedTick)
                return lhs.acceptedTick < rhs.acceptedTick;
            if (lhs.sessionId != rhs.sessionId)
                return lhs.sessionId < rhs.sessionId;
            return lhs.sequenceNum < rhs.sequenceNum;
        });

    for (const auto& pending : drained)
    {
        const EntityID controlledEntity = ResolveControlledEntityForSession(pending.sessionId);
        if (controlledEntity == NULL_ENTITY)
            continue;

        GameCommand cmd = BuildServerCommand(
            pending.wire, pending.sessionId, controlledEntity, m_entityMap);
        cmd.issuedAtTick = tc.tickIndex;

        m_pendingExecCommands.push_back(cmd);
        m_lastSimCommandSeqBySession[pending.sessionId] = pending.sequenceNum;
    }
}

EntityID CGameRoom::ResolveControlledEntityForSession(u32_t sessionId)
{
    if (auto it = m_sessionToEntity.find(sessionId);
        it != m_sessionToEntity.end() && it->second != NULL_ENTITY && m_world.IsAlive(it->second))
    {
        return it->second;
    }

    auto resolveFromSlot = [&](LobbySlotState& slot) -> EntityID
        {
            if (!slot.bHuman || slot.sessionId != sessionId || slot.netId == NULL_NET_ENTITY)
                return NULL_ENTITY;

            const EntityID entity = m_entityMap.FromNet(slot.netId);
            if (entity == NULL_ENTITY || !m_world.IsAlive(entity))
                return NULL_ENTITY;

            m_sessionToEntity[sessionId] = entity;
            return entity;
        };

    if (auto slotIt = m_sessionToSlot.find(sessionId); slotIt != m_sessionToSlot.end())
    {
        const u32_t slotIndex = slotIt->second;
        if (slotIndex < kGameRosterSlotCount)
        {
            if (EntityID entity = resolveFromSlot(m_lobbySlots[slotIndex]); entity != NULL_ENTITY)
                return entity;
        }
    }

    for (LobbySlotState& slot : m_lobbySlots)
    {
        if (EntityID entity = resolveFromSlot(slot); entity != NULL_ENTITY)
            return entity;
    }

    return NULL_ENTITY;
}

void CGameRoom::OnCommandBatch(u32_t sessionId, const Shared::Schema::CommandBatch* batch)
{
    if (!batch || !batch->commands())
        return;

    auto pSession = CSession_Manager::Get()->Find(sessionId);
    if (!pSession)
        return;

    const u64_t acceptedTick = GetCurrentTickIndex();
    const u64_t recvMs = static_cast<u64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    for (const auto* packet : *batch->commands())
    {
        if (!packet)
            continue;

        const u32_t seq = packet->sequenceNum();
        bool bSuspicious = false;
        if (!pSession->TryAcceptSequence(seq, bSuspicious))
        {
            if (bSuspicious)
                pSession->FlagSuspicious();
            continue;
        }

        GameCommandWire wire{};
        wire.kind = static_cast<eCommandKind>(packet->kind());
        wire.clientTick = packet->clientTick();
        wire.sequenceNum = seq;
        wire.slot = packet->slot();
        wire.targetNet = packet->targetNet();
        if (const auto* pGround = packet->groundPos())
            wire.groundPos = { pGround->x(), pGround->y(), pGround->z() };
        if (const auto* pDir = packet->direction())
            wire.direction = { pDir->x(), pDir->y(), pDir->z() };
        wire.itemId = packet->itemId();

        EnqueueCommand(sessionId, wire, acceptedTick, recvMs);
    }
}

void CGameRoom::EnqueueCommand(u32_t sessionId, const GameCommandWire& wire,
    u64_t acceptedTick, u64_t recvTimeMs)
{
    std::lock_guard lk(m_pendingMutex);

    PendingCommand pending{};
    pending.sessionId = sessionId;
    pending.sequenceNum = wire.sequenceNum;
    pending.wire = wire;
    pending.acceptedTick = acceptedTick;
    pending.recvTimeMs = recvTimeMs;

    if (wire.kind == eCommandKind::Move)
    {
        for (PendingCommand& oldPending : m_pendingCommands)
        {
            if (oldPending.sessionId == sessionId &&
                oldPending.wire.kind == eCommandKind::Move)
            {
                oldPending = pending;
                return;
            }
        }
    }

    m_pendingCommands.push_back(pending);
}
