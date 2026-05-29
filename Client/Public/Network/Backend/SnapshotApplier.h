#pragma once

#include "WintersTypes.h"

class CWorld;
class DeterministicRng;
class EntityIdMap;

namespace Shared::Schema
{
    struct Snapshot;
}

class CSnapshotApplier
{
public:
    bool ApplyBytes(const u8_t* bytes, u32_t len,
        CWorld& world, EntityIdMap& map, DeterministicRng& rng);

    void Apply(const Shared::Schema::Snapshot* snap,
        CWorld& world, EntityIdMap& map, DeterministicRng& rng);
};
