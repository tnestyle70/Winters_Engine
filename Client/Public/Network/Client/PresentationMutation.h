#pragma once

#include <cstdint>

enum class ePresentationMutationPhase : std::uint8_t
{
    SpawnOrMark = 0,
    ContactOrClear = 1,
    SnapshotTruth = 2,
};

struct PresentationMutationStamp
{
    std::uint64_t uServerTick = 0u;
    std::uint32_t uEventOrdinal = 0u;
    ePresentationMutationPhase ePhase =
        ePresentationMutationPhase::SpawnOrMark;
    bool bValid = false;
};

constexpr bool IsNewerPresentationMutation(
    const PresentationMutationStamp& candidate,
    const PresentationMutationStamp& current)
{
    if (!candidate.bValid)
        return false;
    if (!current.bValid)
        return true;
    if (candidate.uServerTick != current.uServerTick)
        return candidate.uServerTick > current.uServerTick;
    if (candidate.ePhase != current.ePhase)
    {
        return static_cast<std::uint8_t>(candidate.ePhase) >
            static_cast<std::uint8_t>(current.ePhase);
    }
    return candidate.uEventOrdinal > current.uEventOrdinal;
}
