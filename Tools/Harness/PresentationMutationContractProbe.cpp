#include "Network/Client/PresentationMutation.h"

#include <cstdio>

namespace
{
    constexpr PresentationMutationStamp MakeStamp(
        std::uint64_t tick,
        ePresentationMutationPhase phase,
        std::uint32_t ordinal)
    {
        return PresentationMutationStamp{ tick, ordinal, phase, true };
    }

    constexpr PresentationMutationStamp kInvalid{};
    constexpr PresentationMutationStamp kSpawn10 = MakeStamp(
        10u, ePresentationMutationPhase::SpawnOrMark, 1u);
    constexpr PresentationMutationStamp kSpawn10Later = MakeStamp(
        10u, ePresentationMutationPhase::SpawnOrMark, 2u);
    constexpr PresentationMutationStamp kContact10 = MakeStamp(
        10u, ePresentationMutationPhase::ContactOrClear, 1u);
    constexpr PresentationMutationStamp kSnapshot10 = MakeStamp(
        10u, ePresentationMutationPhase::SnapshotTruth, 0xFFFFFFFFu);
    constexpr PresentationMutationStamp kSpawn11 = MakeStamp(
        11u, ePresentationMutationPhase::SpawnOrMark, 0u);

    static_assert(IsNewerPresentationMutation(kSpawn10, kInvalid));
    static_assert(!IsNewerPresentationMutation(kInvalid, kSpawn10));
    static_assert(IsNewerPresentationMutation(kSpawn10Later, kSpawn10));
    static_assert(!IsNewerPresentationMutation(kSpawn10, kSpawn10Later));
    static_assert(IsNewerPresentationMutation(kContact10, kSpawn10Later));
    static_assert(!IsNewerPresentationMutation(kSpawn10Later, kContact10));
    static_assert(IsNewerPresentationMutation(kSnapshot10, kContact10));
    static_assert(!IsNewerPresentationMutation(kContact10, kSnapshot10));
    static_assert(IsNewerPresentationMutation(kSpawn11, kSnapshot10));
}

int main()
{
    PresentationMutationStamp structureState{};
    structureState = kContact10;
    const bool reversedStructureSpawnRejected =
        !IsNewerPresentationMutation(kSpawn10Later, structureState);

    PresentationMutationStamp fluxState{};
    fluxState = kContact10;
    const bool reversedFluxMarkRejected =
        !IsNewerPresentationMutation(kSpawn10Later, fluxState);

    PresentationMutationStamp snapshotState{};
    snapshotState = kSnapshot10;
    const bool sameTickEventRejected =
        !IsNewerPresentationMutation(kContact10, snapshotState);
    const bool nextTickEventAccepted =
        IsNewerPresentationMutation(kSpawn11, snapshotState);

    const bool pass =
        reversedStructureSpawnRejected &&
        reversedFluxMarkRejected &&
        sameTickEventRejected &&
        nextTickEventAccepted;
    std::printf(
        "[PresentationMutationContract] %s: reversed spawn/contact, mark/clear, snapshot truth, next tick\n",
        pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
