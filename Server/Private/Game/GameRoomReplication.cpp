#include "Game/GameRoom.h"

#include "Game/ReplicationEmitter.h"
#include "Game/ReplayRecorder.h"
#include "Game/SnapshotBuilder.h"
#include "Network/Session.h"
#include "Network/Session_Manager.h"
#include "Shared/Network/PacketEnvelope.h"

#include <Windows.h>

#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

void CGameRoom::FinalizeReplayRecorder()
{
    if (m_bReplayFinalized || !m_pReplayRecorder)
        return;

    m_bReplayFinalized = true;
    if (m_pReplayRecorder->IsEmpty())
    {
        const char* msg = "[Replay] skip save: no records\n";
        OutputDebugStringA(msg);
        std::cout << msg;
        return;
    }

    std::string error;
    const wstring_t path = m_pReplayRecorder->MakeDefaultPath();
    if (m_pReplayRecorder->SaveToFile(path, error))
    {
        std::wstringstream ss;
        ss << L"[Replay] saved " << path
            << L" records=" << m_pReplayRecorder->GetRecordCount()
            << L" snapshots=" << m_pReplayRecorder->GetSnapshotCount()
            << L" events=" << m_pReplayRecorder->GetEventCount()
            << L"\n";

        const wstring_t msg = ss.str();
        OutputDebugStringW(msg.c_str());
        std::wcout << msg;
        return;
    }

    const std::string msg = "[Replay] save failed: " + error + "\n";
    OutputDebugStringA(msg.c_str());
    std::cerr << msg;
}

void CGameRoom::BroadcastEventPayload(const u8_t* payload, u32_t payloadSize, u32_t sequence)
{
    if (!payload || payloadSize == 0)
        return;

    if (m_pReplayRecorder && !m_bReplayFinalized)
        m_pReplayRecorder->RecordEvent(sequence, payload, payloadSize);

    for (u32_t sid : m_sessionIds)
    {
        auto pSession = CSession_Manager::Get()->Find(sid);
        if (!pSession)
            continue;
        if (IsInGamePhase() &&
            !m_sessionBinding.HasBinding(sid))
        {
            continue;
        }

        auto packet = WrapEnvelope(ePacketType::Event, sequence, payload, payloadSize);
        pSession->Send(std::move(packet));
    }
}

void CGameRoom::Phase_BroadcastEvents(TickContext& tc)
{
    auto broadcastSerialized = [&](const SharedSim::SerializedReplicatedEvent& serialized)
    {
        BroadcastEventPayload(
            serialized.payload.data(),
            static_cast<u32_t>(serialized.payload.size()),
            static_cast<u32_t>(tc.tickIndex));

        if (serialized.bUnbindProjectileAfterSend &&
            serialized.projectileNetToUnbind != NULL_NET_ENTITY)
        {
            m_entityMap.Unbind(serialized.projectileNetToUnbind);
        }
    };

    const auto actionEvents = CReplicationEmitter::CollectActionStartEvents(
        m_world,
        m_entityMap,
        tc.tickIndex,
        m_lastBroadcastActionSeq);
    for (const SharedSim::SerializedReplicatedEvent& serialized : actionEvents)
    {
        broadcastSerialized(serialized);
    }

    const auto replicatedEvents =
        CReplicationEmitter::CollectReplicatedEventEntities(m_world);
    for (EntityID entity : replicatedEvents)
    {
        SharedSim::SerializedReplicatedEvent serialized{};
        if (CReplicationEmitter::TryBuildReplicatedEvent(
            m_world,
            m_entityMap,
            entity,
            tc.tickIndex,
            serialized))
        {
            broadcastSerialized(serialized);
        }

        if (m_world.IsAlive(entity))
            m_world.DestroyEntity(entity);
    }
}

void CGameRoom::Phase_BroadcastSnapshot(TickContext& tc)
{
    if (m_pReplayRecorder && !m_bReplayFinalized && m_pSnapBuilder)
    {
        const auto replaySnapshot = m_pSnapBuilder->Build(
            m_world, m_entityMap, tc.tickIndex,
            ResolveServerGameTimeMs(tc.tickIndex),
            m_rng.GetState(), 0u, NULL_NET_ENTITY);

        if (replaySnapshot.size() > 0)
        {
            m_pReplayRecorder->RecordSnapshot(
                tc.tickIndex,
                replaySnapshot.data(),
                static_cast<u32_t>(replaySnapshot.size()));
        }
    }

    for (u32_t sid : m_sessionIds)
    {
        auto pSession = CSession_Manager::Get()->Find(sid);
        if (!pSession)
            continue;

        EntityID controlledEntity = NULL_ENTITY;
        if (!m_sessionBinding.TryGet(sid, controlledEntity) || controlledEntity == NULL_ENTITY)
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
            m_entityMap.ToNet(controlledEntity));

        auto packet = WrapEnvelope(
            ePacketType::Snapshot,
            static_cast<u32_t>(tc.tickIndex),
            snapshot.data(),
            static_cast<u32_t>(snapshot.size()));
        pSession->Send(std::move(packet));
    }
}
