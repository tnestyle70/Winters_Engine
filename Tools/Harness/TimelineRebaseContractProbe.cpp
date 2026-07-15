#include "Network/Client/ClientInputBuffer.h"
#include "Network/Client/SnapshotApplier.h"
#include "Shared/Schemas/Generated/cpp/Snapshot_generated.h"

#include <cstdio>

namespace
{
    bool CheckDefaultSnapshotMetadata()
    {
        flatbuffers::FlatBufferBuilder builder(128u);
        const auto root = Shared::Schema::CreateSnapshot(builder);
        Shared::Schema::FinishSnapshotBuffer(builder, root);

        flatbuffers::Verifier verifier(builder.GetBufferPointer(), builder.GetSize());
        if (!Shared::Schema::VerifySnapshotBuffer(verifier))
            return false;

        const auto* snapshot = Shared::Schema::GetSnapshot(
            builder.GetBufferPointer());
        return snapshot &&
            snapshot->timelineEpoch() == 0u &&
            snapshot->branchId() == 0u &&
            snapshot->toolRevision() == 0u &&
            !snapshot->simPaused() &&
            snapshot->simSpeedMul() == 1.f;
    }

    bool CheckExplicitSnapshotMetadata()
    {
        flatbuffers::FlatBufferBuilder builder(128u);
        Shared::Schema::SnapshotBuilder snapshotBuilder(builder);
        snapshotBuilder.add_timelineEpoch(3u);
        snapshotBuilder.add_branchId(7u);
        snapshotBuilder.add_toolRevision(11u);
        snapshotBuilder.add_simPaused(true);
        snapshotBuilder.add_simSpeedMul(0.5f);
        const auto root = snapshotBuilder.Finish();
        Shared::Schema::FinishSnapshotBuffer(builder, root);

        const auto* snapshot = Shared::Schema::GetSnapshot(
            builder.GetBufferPointer());
        return snapshot &&
            snapshot->timelineEpoch() == 3u &&
            snapshot->branchId() == 7u &&
            snapshot->toolRevision() == 11u &&
            snapshot->simPaused() &&
            snapshot->simSpeedMul() == 0.5f;
    }

    bool CheckTimelineTransitionPolicy()
    {
        SnapshotTimelineState base{};
        base.timelineEpoch = 1u;
        base.branchId = 1u;
        base.toolRevision = 4u;

        SnapshotTimelineState sameIdentity = base;
        sameIdentity.toolRevision = 5u;
        sameIdentity.simPaused = true;
        sameIdentity.simSpeedMul = 2.f;

        SnapshotTimelineState nextEpoch = sameIdentity;
        nextEpoch.timelineEpoch = 2u;
        SnapshotTimelineState nextBranch = sameIdentity;
        nextBranch.branchId = 2u;

        return
            !CSnapshotApplier::ShouldRebaseTimeline(false, {}, base) &&
            !CSnapshotApplier::ShouldRebaseTimeline(
                true, base, sameIdentity) &&
            CSnapshotApplier::ShouldRebaseTimeline(
                true, base, nextEpoch) &&
            CSnapshotApplier::ShouldRebaseTimeline(
                true, base, nextBranch);
    }

    bool CheckInputBufferRebase()
    {
        CClientInputBuffer inputBuffer;
        GameCommandWire command{};
        command.sequenceNum = 9u;
        inputBuffer.Push(command);

        u32_t before = 0u;
        inputBuffer.ForEachAfter(0u, [&](const GameCommandWire&) { ++before; });
        inputBuffer.Clear();
        u32_t after = 0u;
        inputBuffer.ForEachAfter(0u, [&](const GameCommandWire&) { ++after; });
        return before == 1u && after == 0u;
    }
}

int main()
{
    const bool pass =
        CheckDefaultSnapshotMetadata() &&
        CheckExplicitSnapshotMetadata() &&
        CheckTimelineTransitionPolicy() &&
        CheckInputBufferRebase();

    std::printf(
        "[TimelineRebaseContract] %s: schema defaults/explicit metadata, "
        "first-snapshot guard, epoch/branch rebase, input-buffer clear\n",
        pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
