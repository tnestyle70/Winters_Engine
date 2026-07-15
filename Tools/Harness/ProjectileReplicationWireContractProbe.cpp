#include "Shared/Schemas/Generated/cpp/Event_generated.h"
#include "Shared/Schemas/Generated/cpp/Snapshot_generated.h"

#include <cmath>
#include <cstdio>

static_assert(
    Shared::Schema::ProjectileSpawnEvent::VT_MAXDIST == 24 &&
    Shared::Schema::ProjectileSpawnEvent::VT_TARGETNET == 26,
    "ProjectileSpawnEvent fields must remain append-only");
static_assert(
    Shared::Schema::ProjectileHitEvent::VT_BDESTROYED == 18 &&
    Shared::Schema::ProjectileHitEvent::VT_CONTACTREASON == 20 &&
    Shared::Schema::ProjectileHitEvent::VT_CONTACTORDINAL == 22,
    "ProjectileHitEvent fields must remain append-only");
static_assert(
    Shared::Schema::EventPacket::VT_KILLFEED == 22 &&
    Shared::Schema::EventPacket::VT_EVENTORDINAL == 24,
    "EventPacket eventOrdinal must stay after killFeed");
static_assert(
    Shared::Schema::EntitySnapshot::VT_FORCEDMOTIONREMAININGSEC == 230 &&
    Shared::Schema::EntitySnapshot::VT_PROJECTILEDIRX == 232 &&
    Shared::Schema::EntitySnapshot::VT_PROJECTILETRAVELEDDIST == 238,
    "EntitySnapshot projectile motion fields must remain append-only");
static_assert(
    Shared::Schema::Snapshot::VT_SIMSPEEDMUL == 42 &&
    Shared::Schema::Snapshot::VT_GAMEPLAYSTATES == 44,
    "Snapshot gameplayStates must stay after simSpeedMul");

namespace
{
    bool NearlyEqual(float lhs, float rhs)
    {
        return std::fabs(lhs - rhs) <= 0.0001f;
    }

    bool CheckOldProjectileSpawnDefaults()
    {
        flatbuffers::FlatBufferBuilder builder(256u);
        Shared::Schema::ProjectileSpawnEventBuilder spawnBuilder(builder);
        spawnBuilder.add_netId(101u);
        spawnBuilder.add_ownerNet(7u);
        spawnBuilder.add_kind(44u);
        spawnBuilder.add_startX(1.f);
        spawnBuilder.add_startY(2.f);
        spawnBuilder.add_startZ(3.f);
        spawnBuilder.add_dirX(0.5f);
        spawnBuilder.add_dirZ(0.75f);
        spawnBuilder.add_speed(20.f);
        spawnBuilder.add_maxDist(12.f);
        const auto spawn = spawnBuilder.Finish();

        Shared::Schema::EventPacketBuilder packetBuilder(builder);
        packetBuilder.add_kind(Shared::Schema::EventKind::ProjectileSpawn);
        packetBuilder.add_serverTick(900u);
        packetBuilder.add_projectile(spawn);
        const auto packetOffset = packetBuilder.Finish();
        Shared::Schema::FinishEventPacketBuffer(builder, packetOffset);

        flatbuffers::Verifier verifier(
            builder.GetBufferPointer(), builder.GetSize());
        if (!Shared::Schema::VerifyEventPacketBuffer(verifier))
            return false;

        const auto* packet = Shared::Schema::GetEventPacket(
            builder.GetBufferPointer());
        const auto* event = packet ? packet->projectile() : nullptr;
        return packet && event &&
            packet->serverTick() == 900u &&
            packet->eventOrdinal() == 0u &&
            event->netId() == 101u &&
            event->ownerNet() == 7u &&
            event->kind() == 44u &&
            event->targetNet() == 0u &&
            NearlyEqual(event->maxDist(), 12.f);
    }

    bool CheckOldProjectileHitDefaults()
    {
        flatbuffers::FlatBufferBuilder builder(256u);
        Shared::Schema::ProjectileHitEventBuilder hitBuilder(builder);
        hitBuilder.add_netId(101u);
        hitBuilder.add_ownerNet(7u);
        hitBuilder.add_targetNet(55u);
        hitBuilder.add_kind(44u);
        hitBuilder.add_posX(4.f);
        hitBuilder.add_posY(5.f);
        hitBuilder.add_posZ(6.f);
        hitBuilder.add_bDestroyed(true);
        const auto hit = hitBuilder.Finish();

        Shared::Schema::EventPacketBuilder packetBuilder(builder);
        packetBuilder.add_kind(Shared::Schema::EventKind::ProjectileHit);
        packetBuilder.add_serverTick(901u);
        packetBuilder.add_projectileHit(hit);
        const auto packetOffset = packetBuilder.Finish();
        Shared::Schema::FinishEventPacketBuffer(builder, packetOffset);

        flatbuffers::Verifier verifier(
            builder.GetBufferPointer(), builder.GetSize());
        if (!Shared::Schema::VerifyEventPacketBuffer(verifier))
            return false;

        const auto* packet = Shared::Schema::GetEventPacket(
            builder.GetBufferPointer());
        const auto* event = packet ? packet->projectileHit() : nullptr;
        return packet && event &&
            packet->eventOrdinal() == 0u &&
            event->netId() == 101u &&
            event->ownerNet() == 7u &&
            event->targetNet() == 55u &&
            event->bDestroyed() &&
            event->contactReason() ==
                Shared::Schema::ProjectileContactReason::None &&
            event->contactOrdinal() == 0u;
    }

    bool CheckExplicitProjectileContact()
    {
        flatbuffers::FlatBufferBuilder builder(256u);
        const auto hit = Shared::Schema::CreateProjectileHitEvent(
            builder,
            202u,
            8u,
            66u,
            45u,
            7.f,
            8.f,
            9.f,
            false,
            Shared::Schema::ProjectileContactReason::Barrier,
            3u);
        const auto packet = Shared::Schema::CreateEventPacket(
            builder,
            Shared::Schema::EventKind::ProjectileHit,
            902u,
            0,
            0,
            0,
            hit,
            0,
            0,
            0,
            0,
            77u);
        Shared::Schema::FinishEventPacketBuffer(builder, packet);

        const auto* decoded = Shared::Schema::GetEventPacket(
            builder.GetBufferPointer());
        const auto* event = decoded ? decoded->projectileHit() : nullptr;
        return decoded && event &&
            decoded->eventOrdinal() == 77u &&
            event->contactReason() ==
                Shared::Schema::ProjectileContactReason::Barrier &&
            event->contactOrdinal() == 3u &&
            !event->bDestroyed();
    }

    bool CheckOldSnapshotDefaults()
    {
        flatbuffers::FlatBufferBuilder builder(512u);
        Shared::Schema::EntitySnapshotBuilder entityBuilder(builder);
        entityBuilder.add_netId(303u);
        entityBuilder.add_entityKind(Shared::Schema::EntityKind::Projectile);
        entityBuilder.add_projectileKind(44u);
        entityBuilder.add_projectileOwnerNet(7u);
        entityBuilder.add_projectileTargetNet(55u);
        entityBuilder.add_projectileSpeed(20.f);
        entityBuilder.add_projectileRadius(0.6f);
        entityBuilder.add_projectileMaxDist(12.f);
        const auto entity = entityBuilder.Finish();
        const auto entities = builder.CreateVector(&entity, 1u);

        Shared::Schema::SnapshotBuilder snapshotBuilder(builder);
        snapshotBuilder.add_serverTick(903u);
        snapshotBuilder.add_entities(entities);
        snapshotBuilder.add_lastAckedCommandSeq(19u);
        snapshotBuilder.add_timelineEpoch(2u);
        snapshotBuilder.add_branchId(3u);
        snapshotBuilder.add_toolRevision(4u);
        snapshotBuilder.add_simPaused(true);
        snapshotBuilder.add_simSpeedMul(0.5f);
        const auto snapshotOffset = snapshotBuilder.Finish();
        Shared::Schema::FinishSnapshotBuffer(builder, snapshotOffset);

        flatbuffers::Verifier verifier(
            builder.GetBufferPointer(), builder.GetSize());
        if (!Shared::Schema::VerifySnapshotBuffer(verifier))
            return false;

        const auto* snapshot = Shared::Schema::GetSnapshot(
            builder.GetBufferPointer());
        const auto* decodedEntities = snapshot ? snapshot->entities() : nullptr;
        const auto* decodedEntity = decodedEntities && !decodedEntities->empty()
            ? decodedEntities->Get(0u)
            : nullptr;
        return snapshot && decodedEntity &&
            snapshot->lastAckedCommandSeq() == 19u &&
            snapshot->timelineEpoch() == 2u &&
            snapshot->branchId() == 3u &&
            snapshot->toolRevision() == 4u &&
            snapshot->simPaused() &&
            NearlyEqual(snapshot->simSpeedMul(), 0.5f) &&
            snapshot->gameplayStates() == nullptr &&
            decodedEntity->projectileOwnerNet() == 7u &&
            decodedEntity->projectileTargetNet() == 55u &&
            NearlyEqual(decodedEntity->projectileMaxDist(), 12.f) &&
            NearlyEqual(decodedEntity->projectileDirX(), 0.f) &&
            NearlyEqual(decodedEntity->projectileDirY(), 0.f) &&
            NearlyEqual(decodedEntity->projectileDirZ(), 0.f) &&
            NearlyEqual(decodedEntity->projectileTraveledDist(), 0.f);
    }

    bool CheckExplicitGameplayState()
    {
        flatbuffers::FlatBufferBuilder builder(256u);
        Shared::Schema::GameplayStateSnapshotBuilder stateBuilder(builder);
        stateBuilder.add_kind(
            Shared::Schema::GameplayStateKind::EzrealRisingSpellForce);
        stateBuilder.add_sourceNet(7u);
        stateBuilder.add_startTick(100u);
        stateBuilder.add_expireTick(280u);
        stateBuilder.add_stackCount(5u);
        const auto state = stateBuilder.Finish();
        const auto states = builder.CreateVector(&state, 1u);

        Shared::Schema::SnapshotBuilder snapshotBuilder(builder);
        snapshotBuilder.add_serverTick(904u);
        snapshotBuilder.add_gameplayStates(states);
        const auto snapshotOffset = snapshotBuilder.Finish();
        Shared::Schema::FinishSnapshotBuffer(builder, snapshotOffset);

        const auto* snapshot = Shared::Schema::GetSnapshot(
            builder.GetBufferPointer());
        const auto* statesOut = snapshot ? snapshot->gameplayStates() : nullptr;
        const auto* stateOut = statesOut && !statesOut->empty()
            ? statesOut->Get(0u)
            : nullptr;
        return stateOut &&
            stateOut->kind() ==
                Shared::Schema::GameplayStateKind::EzrealRisingSpellForce &&
            stateOut->sourceNet() == 7u &&
            stateOut->startTick() == 100u &&
            stateOut->expireTick() == 280u &&
            stateOut->stackCount() == 5u;
    }
}

int main()
{
    const bool pass =
        CheckOldProjectileSpawnDefaults() &&
        CheckOldProjectileHitDefaults() &&
        CheckExplicitProjectileContact() &&
        CheckOldSnapshotDefaults() &&
        CheckExplicitGameplayState();

    std::printf(
        "[ProjectileReplicationWireContract] %s: append-only field ids, old-wire defaults, typed contacts, gameplay state\n",
        pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
