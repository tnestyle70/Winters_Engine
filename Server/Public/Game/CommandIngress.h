#pragma once

#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "WintersTypes.h"

#include <mutex>
#include <vector>

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
        u64_t recvTimeMs);

    void EnqueueCommand(
        u32_t sessionId,
        const GameCommandWire& wire,
        u64_t acceptedTick,
        u64_t recvTimeMs,
        u64_t clientTimestampMs);

    std::vector<PendingCommand> DrainSorted();

    // Chrono Break: 되감기 시 명령 백로그 폐기 (과거로 간 세계에 미래 입력을 흘리지 않는다).
    void Clear()
    {
        std::lock_guard lock(m_pendingMutex);
        m_pendingCommands.clear();
    }

    static void TraceCommandTiming(const PendingCommand& pending, u64_t execTick);

private:
    std::mutex m_pendingMutex;
    std::vector<PendingCommand> m_pendingCommands;
};
