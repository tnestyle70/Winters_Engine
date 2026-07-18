#pragma once

#include "ECS/Entity.h"
#include "Shared/GameSim/Replication/EntityIdMap.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "WintersTypes.h"

#include <flatbuffers/flatbuffers.h>
#include <array>
#include <memory>

class CWorld;

class CSnapshotBuilder final
{
public:
    static std::unique_ptr<CSnapshotBuilder> Create();

    flatbuffers::DetachedBuffer Build(
        CWorld& world,
        const EntityIdMap& entityMap,
        u64_t serverTick,
        u64_t serverTimeMs,
        u64_t rngState,
        u32_t lastAckedSeq,
        const std::array<SkillCommandFeedback, 5u>& commandFeedback,
        NetEntityId yourNetId,
        u64_t timelineEpoch,
        u64_t branchId,
        u64_t toolRevision,
        bool_t simPaused,
        f32_t simSpeedMul);

private:
    CSnapshotBuilder() = default;
};
