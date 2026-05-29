#include "Network/Backend/SnapshotApplier.h"

#include "Shared/GameSim/Core/Determinism/DeterministicRng.h"
#include "Shared/GameSim/Replication/EntityIdMap.h"
#include "Shared/Schemas/Generated/cpp/Snapshot_generated.h"

#include "ECS/World.h"

bool CSnapshotApplier::ApplyBytes(const u8_t* bytes, u32_t len,
    CWorld& world, EntityIdMap& map, DeterministicRng& rng)
{
    if (bytes == nullptr || len == 0)
        return false;

    flatbuffers::Verifier verifier(bytes, len);
    if (!Shared::Schema::VerifySnapshotBuffer(verifier))
        return false;

    Apply(Shared::Schema::GetSnapshot(bytes), world, map, rng);
    return true;
}

void CSnapshotApplier::Apply(const Shared::Schema::Snapshot* snap,
    CWorld& world, EntityIdMap& map, DeterministicRng& rng)
{
    (void)world;
    (void)map;

    if (snap == nullptr)
        return;

    rng.SetState(snap->rngState());
}
