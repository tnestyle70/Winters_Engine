#include "Game/GameRoom.h"

#include "Backend/ReplayUploadQueue.h"
#include "Game/ReplicationEmitter.h"
#include "Game/ReplayRecorder.h"
#include "Game/SnapshotBuilder.h"
#include "Network/ServerSessionHub.h"

#include <Windows.h>

#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

void CGameRoom::FinalizeReplayRecorder()
{
    if (m_bReplayFinalized || !m_pReplayRecorder)
        return;

    if (m_pReplayRecorder->IsEmpty())
    {
        m_bReplayFinalized = true;
        const char* msg = "[Replay] skip save: no records\n";
        OutputDebugStringA(msg);
        std::cout << msg;
        return;
    }

    std::string error;
    const wstring_t path = m_matchID.empty()
        ? m_pReplayRecorder->MakeDefaultPath()
        : m_pReplayRecorder->MakeMatchPendingPath(m_matchID);
    if (m_pReplayRecorder->SaveToFile(path, error))
    {
        m_bReplayFinalized = true;
        std::wstringstream ss;
        ss << L"[Replay] saved " << path
            << L" records=" << m_pReplayRecorder->GetRecordCount()
            << L" snapshots=" << m_pReplayRecorder->GetSnapshotCount()
            << L" events=" << m_pReplayRecorder->GetEventCount()
            << L"\n";

        const wstring_t msg = ss.str();
        OutputDebugStringW(msg.c_str());
        std::wcout << msg;

        bool_t bHasCompleteRoster = !m_authenticatedParticipants.empty();
        ReplayUploadArtifact artifact{};
        artifact.path = path;
        artifact.matchID = m_matchID;
        artifact.formatVersion = Winters::Replay::kReplayVersion;
        artifact.tickRate = m_pReplayRecorder->GetTickRate();
        artifact.recordCount = m_pReplayRecorder->GetRecordCount();
        artifact.snapshotCount = m_pReplayRecorder->GetSnapshotCount();
        artifact.eventCount = m_pReplayRecorder->GetEventCount();
        artifact.commandCount = m_pReplayRecorder->GetCommandCount();
        artifact.firstTick = m_pReplayRecorder->GetFirstTick();
        artifact.lastTick = m_pReplayRecorder->GetLastTick();
        for (const auto& [userID, participant] : m_authenticatedParticipants)
        {
            if (participant.team == 0xFFu || m_winningTeam == 0xFFu)
            {
                bHasCompleteRoster = false;
                break;
            }
            ReplayUploadParticipant uploadParticipant{};
            uploadParticipant.userID = userID;
            uploadParticipant.result = participant.team == m_winningTeam
                ? "win"
                : "loss";
            artifact.participants.push_back(std::move(uploadParticipant));
        }

        std::error_code sizeError;
        artifact.sizeBytes = std::filesystem::file_size(path, sizeError);
        if (!artifact.matchID.empty() && bHasCompleteRoster && !sizeError)
        {
            if (!CReplayUploadQueue::Instance().Enqueue(std::move(artifact)))
            {
                const char* uploadMessage =
                    "[ReplayUpload] artifact retained locally: queue disabled or full\n";
                OutputDebugStringA(uploadMessage);
                std::cout << uploadMessage;
            }
        }
        else if (!artifact.matchID.empty())
        {
            const char* uploadMessage =
                "[ReplayUpload] artifact retained locally: match result or roster incomplete\n";
            OutputDebugStringA(uploadMessage);
            std::cout << uploadMessage;
        }
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
        if (!CServerSessionHub::Instance().IsSessionActive(sid))
            continue;
        if (IsInGamePhase() &&
            !m_sessionBinding.HasBinding(sid))
        {
            continue;
        }

        CServerSessionHub::Instance().SendFrame(
            sid,
            ePacketType::Event,
            sequence,
            payload,
            payloadSize);
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
            static_cast<u32_t>(entity),
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
    if (m_pReplayRecorder && !m_bReplayFinalized && m_pSnapBuilder &&
        CReplayRecorder::ShouldRecordSnapshot(
            tc.tickIndex,
            m_toolRevision,
            m_lastReplaySnapshotTick,
            m_lastReplayToolRevision))
    {
        const auto replaySnapshot = m_pSnapBuilder->Build(
            m_world, m_entityMap, tc.tickIndex,
            ResolveServerGameTimeMs(tc.tickIndex),
            m_rng.GetState(), 0u, NULL_NET_ENTITY,
            m_timelineEpoch,
            m_timelineBranchId,
            m_toolRevision,
            m_bSimPaused,
            m_simSpeedMul.load(std::memory_order_relaxed));

        if (replaySnapshot.size() > 0)
        {
            m_pReplayRecorder->RecordSnapshot(
                tc.tickIndex,
                replaySnapshot.data(),
                static_cast<u32_t>(replaySnapshot.size()));
            m_lastReplaySnapshotTick = tc.tickIndex;
            m_lastReplayToolRevision = m_toolRevision;
        }
    }

    for (u32_t sid : m_sessionIds)
    {
        if (!CServerSessionHub::Instance().IsSessionActive(sid))
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
            m_entityMap.ToNet(controlledEntity),
            m_timelineEpoch,
            m_timelineBranchId,
            m_toolRevision,
            m_bSimPaused,
            m_simSpeedMul.load(std::memory_order_relaxed));

        CServerSessionHub::Instance().SendFrame(
            sid,
            ePacketType::Snapshot,
            static_cast<u32_t>(tc.tickIndex),
            snapshot.data(),
            static_cast<u32_t>(snapshot.size()));
    }
}
