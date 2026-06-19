#pragma once

#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "WintersTypes.h"

#include <mutex>
#include <vector>

class CSession;
namespace Shared::Schema { struct CommandBatch; }

struct PendingCommand
{
    u32_t sessionId = 0;
    u32_t sequenceNum = 0;
    GameCommandWire wire{};
    u64_t acceptedTick = 0;
    u64_t recvTimeMs = 0;
    u64_t clientTimestampMs = 0;
};

class CCommandIngress final
{
public:
    void AcceptCommandBatch(
        u32_t sessionId,
        const Shared::Schema::CommandBatch* batch,
        u64_t acceptedTick,
        u64_t recvTimeMs,
        CSession& session);

    void EnqueueCommand(
        u32_t sessionId,
        const GameCommandWire& wire,
        u64_t acceptedTick,
        u64_t recvTimeMs,
        u64_t clientTimestampMs);

    std::vector<PendingCommand> DrainSorted();

    static void TraceCommandTiming(const PendingCommand& pending, u64_t execTick);

private:
    std::mutex m_pendingMutex;
    std::vector<PendingCommand> m_pendingCommands;
};
