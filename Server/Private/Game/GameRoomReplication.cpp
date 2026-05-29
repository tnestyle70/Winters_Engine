#include "Game/GameRoom.h"

#include "Game/ReplayRecorder.h"
#include "Game/SnapshotBuilder.h"
#include "Network/Session.h"
#include "Network/Session_Manager.h"
#include "Shared/GameSim/Components/NetAnimationComponent.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/ReplicatedEventSerializer/ReplicatedEventSerializer.h"
#include "Shared/Network/PacketEnvelope.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

void CGameRoom::FinalizeReplayRecorder()
{
    if (m_bReplayFinalized || !m_pReplayRecorder)
        return;

    m_bReplayFinalized = true;
    if (m_pReplayRecorder->IsEmpty())
        return;

    std::string error;
    const wstring_t path = m_pReplayRecorder->MakeDefaultPath();
    (void)m_pReplayRecorder->SaveToFile(path, error);
}

void CGameRoom::BroadcastEventPayload(const u8_t* payload, u32_t payloadSize, u32_t sequence)
{
    if (!payload || payloadSize == 0)
        return;

    for (u32_t sid : m_sessionIds)
    {
        auto pSession = CSession_Manager::Get()->Find(sid);
        if (!pSession)
            continue;
        if (m_roomPhase == eRoomPhase::InGame &&
            m_sessionToEntity.find(sid) == m_sessionToEntity.end())
        {
            continue;
        }

        auto packet = WrapEnvelope(ePacketType::Event, sequence, payload, payloadSize);
        pSession->Send(std::move(packet));
    }
}

void CGameRoom::BroadcastReplicatedEvent(const ReplicatedEventComponent& event, TickContext& tc)
{
    SharedSim::SerializedReplicatedEvent serialized{};
    if (!SharedSim::CReplicatedEventSerializer::Build(
        m_world,
        m_entityMap,
        event,
        tc.tickIndex,
        serialized))
    {
        return;
    }

    BroadcastEventPayload(
        serialized.payload.data(),
        static_cast<u32_t>(serialized.payload.size()),
        static_cast<u32_t>(tc.tickIndex));

    if (serialized.bUnbindProjectileAfterSend &&
        serialized.projectileNetToUnbind != NULL_NET_ENTITY)
    {
        m_entityMap.Unbind(serialized.projectileNetToUnbind);
    }
}

void CGameRoom::Phase_BroadcastEvents(TickContext& tc)
{
    struct AnimEvent
    {
        NetEntityId netId = NULL_NET_ENTITY;
        NetAnimationComponent anim{};
    };

    std::vector<AnimEvent> events;
    m_world.ForEach<NetAnimationComponent>(
        std::function<void(EntityID, NetAnimationComponent&)>(
            [&](EntityID entity, NetAnimationComponent& anim)
            {
                if (anim.actionSeq == 0)
                    return;

                const NetEntityId netId = m_entityMap.ToNet(entity);
                if (netId == NULL_NET_ENTITY)
                    return;

                u32_t& lastSeq = m_lastBroadcastActionSeq[entity];
                if (lastSeq == anim.actionSeq)
                    return;

                lastSeq = anim.actionSeq;
                events.push_back(AnimEvent{ netId, anim });
            }));

    for (const AnimEvent& ev : events)
    {
        SharedSim::SerializedReplicatedEvent serialized{};
        if (!SharedSim::CReplicatedEventSerializer::BuildAnimationStart(
            ev.netId,
            ev.anim,
            tc.tickIndex,
            serialized))
        {
            continue;
        }

        BroadcastEventPayload(
            serialized.payload.data(),
            static_cast<u32_t>(serialized.payload.size()),
            static_cast<u32_t>(tc.tickIndex));
    }

    const auto replicatedEvents =
        DeterministicEntityIterator<ReplicatedEventComponent>::CollectSorted(m_world);

    for (EntityID entity : replicatedEvents)
    {
        if (!m_world.IsAlive(entity) ||
            !m_world.HasComponent<ReplicatedEventComponent>(entity))
        {
            continue;
        }

        const ReplicatedEventComponent event =
            m_world.GetComponent<ReplicatedEventComponent>(entity);
        BroadcastReplicatedEvent(event, tc);
        m_world.DestroyEntity(entity);
    }
}

void CGameRoom::Phase_BroadcastSnapshot(TickContext& tc)
{
    for (u32_t sid : m_sessionIds)
    {
        auto pSession = CSession_Manager::Get()->Find(sid);
        if (!pSession)
            continue;

        auto entityIt = m_sessionToEntity.find(sid);
        if (entityIt == m_sessionToEntity.end() || entityIt->second == NULL_ENTITY)
            continue;

        const auto ackIt = m_lastSimCommandSeqBySession.find(sid);
        const u32_t lastSimCommandSeq =
            (ackIt != m_lastSimCommandSeqBySession.end()) ? ackIt->second : 0u;

        auto snapshot = m_pSnapBuilder->Build(
            m_world,
            m_entityMap,
            tc.tickIndex,
            ResolveServerGameTimeMs(tc.tickIndex),
            m_rng.GetState(),
            lastSimCommandSeq,
            m_entityMap.ToNet(entityIt->second));

        auto packet = WrapEnvelope(
            ePacketType::Snapshot,
            static_cast<u32_t>(tc.tickIndex),
            snapshot.data(),
            static_cast<u32_t>(snapshot.size()));
        pSession->Send(std::move(packet));
    }
}
