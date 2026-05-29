#pragma once

#include "ECS/Entity.h"
#include "Shared/GameSim/Replication/EntityIdMap.h"
#include "WintersTypes.h"

#include <flatbuffers/flatbuffers.h>
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
        NetEntityId yourNetId);

private:
    CSnapshotBuilder() = default;
};
