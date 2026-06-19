#include "Game/CommandIngress.h"

#include "Network/Session.h"
#include "Security/LagCompensation.h"
#include "Shared/Schemas/Generated/cpp/Command_generated.h"

#include <Windows.h>
#include <algorithm>
#include <cstdio>
#include <mutex>

void CCommandIngress::AcceptCommandBatch(
    u32_t sessionId,
    const Shared::Schema::CommandBatch* batch,
    u64_t acceptedTick,
    u64_t recvTimeMs,
    CSession& session)
{
    if (!batch || !batch->commands())
        return;

    const u64_t clientTimestampMs = batch->clientTimestampMs();

    for (const auto* packet : *batch->commands())
    {
        if (!packet)
            continue;

        const u32_t seq = packet->sequenceNum();
        bool bSuspicious = false;
        if (!session.TryAcceptSequence(seq, bSuspicious))
        {
            if (bSuspicious)
                session.FlagSuspicious();
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

        EnqueueCommand(sessionId, wire, acceptedTick, recvTimeMs, clientTimestampMs);
    }
}

void CCommandIngress::EnqueueCommand(
    u32_t sessionId,
    const GameCommandWire& wire,
    u64_t acceptedTick,
    u64_t recvTimeMs,
    u64_t clientTimestampMs)
{
    std::lock_guard lk(m_pendingMutex);

    PendingCommand pending{};
    pending.sessionId = sessionId;
    pending.sequenceNum = wire.sequenceNum;
    pending.wire = wire;
    pending.acceptedTick = acceptedTick;
    pending.recvTimeMs = recvTimeMs;
    pending.clientTimestampMs = clientTimestampMs;

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

std::vector<PendingCommand> CCommandIngress::DrainSorted()
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

    return drained;
}

void CCommandIngress::TraceCommandTiming(const PendingCommand& pending, u64_t execTick)
{
    if (pending.clientTimestampMs == 0)
        return;

    static u32_t s_commandTimingTraceCount = 0;
    if (s_commandTimingTraceCount >= 64u)
        return;

    const long long clockDeltaMs =
        static_cast<long long>(pending.recvTimeMs) -
        static_cast<long long>(pending.clientTimestampMs);
    const u64_t absDeltaMs = clockDeltaMs < 0
        ? static_cast<u64_t>(-clockDeltaMs)
        : static_cast<u64_t>(clockDeltaMs);
    const u64_t observedClampedMs =
        (std::min)(absDeltaMs, CLagCompensation::kMaxRewindMs);

    char msg[256]{};
    sprintf_s(
        msg,
        "[LagComp][CommandTiming] sid=%u seq=%u kind=%u clockDeltaMs=%lld observedClampedMs=%llu acceptedTick=%llu execTick=%llu rewindTicks=0\n",
        pending.sessionId,
        pending.sequenceNum,
        static_cast<u32_t>(pending.wire.kind),
        clockDeltaMs,
        static_cast<unsigned long long>(observedClampedMs),
        static_cast<unsigned long long>(pending.acceptedTick),
        static_cast<unsigned long long>(execTick));
    OutputDebugStringA(msg);
    ++s_commandTimingTraceCount;
}
