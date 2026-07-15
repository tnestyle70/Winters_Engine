#include "Shared/GameSim/Systems/ReplicatedEventSerializer/ReplicatedEventSerializer.h"

#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include "Shared/Schemas/Generated/cpp/Event_generated.h"
#pragma pop_macro("max")
#pragma pop_macro("min")

namespace SharedSim
{
    namespace
    {
        void Reset(SerializedReplicatedEvent& out)
        {
            out.payload = flatbuffers::DetachedBuffer{};
            out.projectileNetToUnbind = NULL_NET_ENTITY;
            out.bValid = false;
            out.bUnbindProjectileAfterSend = false;
        }

        bool_t Finish(
            flatbuffers::FlatBufferBuilder& fbb,
            flatbuffers::Offset<Shared::Schema::EventPacket> packet,
            SerializedReplicatedEvent& out)
        {
            fbb.Finish(packet);
            out.payload = fbb.Release();
            out.bValid = out.payload.data() != nullptr && out.payload.size() > 0;
            return out.bValid;
        }
    }

    bool_t CReplicatedEventSerializer::Build(
        CWorld& world,
        EntityIdMap& entityMap,
        const ReplicatedEventComponent& event,
        u64_t serverTick,
        u32_t uEventOrdinal,
        SerializedReplicatedEvent& out)
    {
        Reset(out);

        switch (event.kind)
        {
        case eReplicatedEventKind::Damage:
        {
            const NetEntityId targetNet = entityMap.ToNet(event.targetEntity);
            if (targetNet == NULL_NET_ENTITY)
                return false;

            flatbuffers::FlatBufferBuilder fbb(128);
            const auto damage = Shared::Schema::CreateDamageEvent(
                fbb,
                entityMap.ToNet(event.sourceEntity),
                targetNet,
                event.amount,
                static_cast<u8_t>(event.damageType),
                event.bWasCrit,
                event.bKilled,
                event.skillId);

            return Finish(fbb, Shared::Schema::CreateEventPacket(
                fbb,
                Shared::Schema::EventKind::Damage,
                serverTick,
                damage,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                uEventOrdinal), out);
        }
        case eReplicatedEventKind::SkillCast:
        {
            const NetEntityId casterNet = entityMap.ToNet(event.sourceEntity);
            if (casterNet == NULL_NET_ENTITY)
                return false;

            flatbuffers::FlatBufferBuilder fbb(128);
            const auto skillCast = Shared::Schema::CreateSkillCastEvent(
                fbb,
                casterNet,
                event.slot,
                event.rank,
                entityMap.ToNet(event.targetEntity));

            return Finish(fbb, Shared::Schema::CreateEventPacket(
                fbb,
                Shared::Schema::EventKind::SkillCast,
                serverTick,
                0,
                0,
                0,
                0,
                skillCast,
                0,
                0,
                0,
                uEventOrdinal), out);
        }
        case eReplicatedEventKind::EffectTrigger:
        {
            const NetEntityId sourceNet = event.sourceNetOverride != NULL_NET_ENTITY
                ? event.sourceNetOverride
                : entityMap.ToNet(event.sourceEntity);
            const NetEntityId targetNet = event.targetNetOverride != NULL_NET_ENTITY
                ? event.targetNetOverride
                : entityMap.ToNet(event.targetEntity);
            if (sourceNet == NULL_NET_ENTITY && targetNet == NULL_NET_ENTITY)
                return false;

            flatbuffers::FlatBufferBuilder fbb(192);
            const auto effect = Shared::Schema::CreateEffectTriggerEvent(
                fbb,
                event.effectId,
                sourceNet,
                targetNet,
                event.attachBone,
                event.position.x,
                event.position.y,
                event.position.z,
                event.direction.x,
                event.direction.y,
                event.direction.z,
                event.startTick,
                event.durationMs,
                event.flags);

            return Finish(fbb, Shared::Schema::CreateEventPacket(
                fbb,
                Shared::Schema::EventKind::EffectTrigger,
                serverTick,
                0,
                0,
                0,
                0,
                0,
                0,
                effect,
                0,
                uEventOrdinal), out);
        }
        case eReplicatedEventKind::ProjectileSpawn:
        {
            NetEntityId projectileNet = event.projectileNetOverride;
            if (projectileNet == NULL_NET_ENTITY &&
                event.projectileEntity != NULL_ENTITY &&
                world.IsAlive(event.projectileEntity))
            {
                projectileNet = entityMap.IssueNew(event.projectileEntity);
            }

            flatbuffers::FlatBufferBuilder fbb(192);
            const auto projectile = Shared::Schema::CreateProjectileSpawnEvent(
                fbb,
                projectileNet,
                event.sourceNetOverride != NULL_NET_ENTITY
                    ? event.sourceNetOverride
                    : entityMap.ToNet(event.sourceEntity),
                event.projectileKind,
                event.position.x,
                event.position.y,
                event.position.z,
                event.direction.x,
                event.direction.y,
                event.direction.z,
                event.speed,
                event.maxDistance,
                event.targetNetOverride != NULL_NET_ENTITY
                    ? event.targetNetOverride
                    : entityMap.ToNet(event.targetEntity));

            return Finish(fbb, Shared::Schema::CreateEventPacket(
                fbb,
                Shared::Schema::EventKind::ProjectileSpawn,
                serverTick,
                0,
                0,
                projectile,
                0,
                0,
                0,
                0,
                0,
                uEventOrdinal), out);
        }
        case eReplicatedEventKind::KillFeed:
        {
            const NetEntityId sourceNet = entityMap.ToNet(event.sourceEntity);
            if (sourceNet == NULL_NET_ENTITY)
                return false;

            flatbuffers::FlatBufferBuilder fbb(128);
            const auto killFeed = Shared::Schema::CreateKillFeedEvent(
                fbb, sourceNet, entityMap.ToNet(event.targetEntity),
                static_cast<u8_t>(event.sourceChampion),
                static_cast<u8_t>(event.targetChampion),
                event.sourceTeam,
                event.targetTeam,
                static_cast<Shared::Schema::KillFeedObjectKind>(
                    static_cast<u8_t>(event.killFeedObjectKind)));

            return Finish(fbb, Shared::Schema::CreateEventPacket(
                fbb,
                Shared::Schema::EventKind::KillFeed,
                serverTick,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                killFeed,
                uEventOrdinal), out);
        }
        case eReplicatedEventKind::ProjectileHit:
        {
            const NetEntityId projectileNet =
                event.projectileNetOverride != NULL_NET_ENTITY
                    ? event.projectileNetOverride
                    : entityMap.ToNet(event.projectileEntity);

            flatbuffers::FlatBufferBuilder fbb(160);
            const auto projectileHit = Shared::Schema::CreateProjectileHitEvent(
                fbb,
                projectileNet,
                event.sourceNetOverride != NULL_NET_ENTITY
                    ? event.sourceNetOverride
                    : entityMap.ToNet(event.sourceEntity),
                event.targetNetOverride != NULL_NET_ENTITY
                    ? event.targetNetOverride
                    : entityMap.ToNet(event.targetEntity),
                event.projectileKind,
                event.position.x,
                event.position.y,
                event.position.z,
                event.bDestroyed,
                static_cast<Shared::Schema::ProjectileContactReason>(
                    static_cast<u8_t>(event.eContactReason)),
                event.uContactOrdinal);

            const bool_t ok = Finish(fbb, Shared::Schema::CreateEventPacket(
                fbb,
                Shared::Schema::EventKind::ProjectileHit,
                serverTick,
                0,
                0,
                0,
                projectileHit,
                0,
                0,
                0,
                0,
                uEventOrdinal), out);

            if (ok && event.bDestroyed && projectileNet != NULL_NET_ENTITY)
            {
                out.projectileNetToUnbind = projectileNet;
                out.bUnbindProjectileAfterSend = true;
            }

            return ok;
        }
        default:
            return false;
        }
    }

    bool_t CReplicatedEventSerializer::BuildActionStart(
        NetEntityId netId,
        const ReplicatedActionComponent& action,
        u64_t serverTick,
        SerializedReplicatedEvent& out)
    {
        Reset(out);

        if (netId == NULL_NET_ENTITY || action.sequence == 0)
            return false;

        flatbuffers::FlatBufferBuilder fbb(128);
        const auto actionStart = Shared::Schema::CreateActionStartEvent(
            fbb,
            netId,
            action.actionId,
            action.stage,
            action.sequence,
            action.startTick,
            static_cast<u8_t>(action.sourceChampion),
            action.sourceSlot,
            static_cast<u8_t>(action.movePolicy),
            action.lockEndTick,
            action.commandSequence);

        return Finish(fbb, Shared::Schema::CreateEventPacket(
            fbb,
            Shared::Schema::EventKind::ActionStart,
            serverTick,
            0,
            0,
            0,
            0,
            0,
            actionStart,
            0), out);
    }
}
