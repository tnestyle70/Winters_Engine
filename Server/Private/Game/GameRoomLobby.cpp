#include "Game/GameRoom.h"

#include "GameRoomInternal.h"
#include "Network/PacketDispatcher.h"
#include "Network/Session.h"
#include "Network/Session_Manager.h"
#include "Shared/Network/PacketEnvelope.h"
#include "Shared/Schemas/Generated/cpp/Hello_generated.h"
#include "Shared/Schemas/Generated/cpp/LobbyCommand_generated.h"
#include "Shared/Schemas/Generated/cpp/LobbyState_generated.h"

#include <algorithm>
#include <cstdio>
#include <flatbuffers/flatbuffers.h>
#include <mutex>
#include <string>
#include <vector>

namespace
{
    Shared::Schema::LobbyPhase ToSchemaLobbyPhase(eRoomPhase phase)
    {
        switch (phase)
        {
        case eRoomPhase::SeatSelect:
            return Shared::Schema::LobbyPhase::SeatSelect;
        case eRoomPhase::ChampionSelect:
            return Shared::Schema::LobbyPhase::ChampionSelect;
        case eRoomPhase::Loading:
            return Shared::Schema::LobbyPhase::Starting;
        case eRoomPhase::InGame:
            return Shared::Schema::LobbyPhase::InGame;
        default:
            return Shared::Schema::LobbyPhase::None;
        }
    }
}

void CGameRoom::InitializeLobbyAuthority()
{
    m_pLobbyAuthority = std::make_unique<CLobbyAuthority>(m_roomId);
    m_pLobbyAuthority->InitializeSlots();
}

void CGameRoom::OnLobbyCommand(
    u32_t sessionId,
    const Shared::Schema::LobbyCommand* command)
{
    if (!command || !m_pLobbyAuthority)
        return;

    std::lock_guard stateLock(m_stateMutex);
    const LobbyAuthorityResult result =
        m_pLobbyAuthority->OnLobbyCommand(sessionId, command);
    ApplyLobbyAuthorityResult(result);
}

EntityID CGameRoom::OnSessionJoin(u32_t sessionId)
{
    std::lock_guard stateLock(m_stateMutex);

    EntityID boundEntity = NULL_ENTITY;
    if (m_sessionBinding.TryGet(sessionId, boundEntity) && boundEntity != NULL_ENTITY)
    {
        return boundEntity;
    }

    if (std::find(m_sessionIds.begin(), m_sessionIds.end(), sessionId) == m_sessionIds.end())
        m_sessionIds.push_back(sessionId);
    std::sort(m_sessionIds.begin(), m_sessionIds.end());

    CPacketDispatcher::Instance().RouteSession(sessionId, m_roomId);

    if (m_pLobbyAuthority &&
        m_pLobbyAuthority->GetPhase() != eRoomPhase::SeatSelect &&
        m_pLobbyAuthority->GetPhase() != eRoomPhase::ChampionSelect)
    {
        EntityID lateJoinEntity = NULL_ENTITY;
        LobbyAuthorityResult attachResult{};
        if (TryAttachSessionToDisconnectedHumanSlot(sessionId, lateJoinEntity, attachResult))
        {
            ApplyLobbyAuthorityResult(attachResult);

            char msg[192]{};
            sprintf_s(msg,
                "[GameRoom] Late session attach sid=%u net=%u entity=%u phase=%u\n",
                sessionId,
                attachResult.helloNetId,
                static_cast<u32_t>(lateJoinEntity),
                static_cast<u32_t>(m_pLobbyAuthority->GetPhase()));
            OutputServerAITrace(msg);
            return lateJoinEntity;
        }
    }

    bool_t bHadKnownSlot = false;
    if (m_pLobbyAuthority)
    {
        u8_t slotId = kInvalidGameRosterSlot;
        bHadKnownSlot = m_pLobbyAuthority->TryGetSessionSlot(sessionId, slotId);
    }

    const LobbyAuthorityResult result = m_pLobbyAuthority
        ? m_pLobbyAuthority->OnSessionJoin(sessionId)
        : LobbyAuthorityResult{};
    ApplyLobbyAuthorityResult(result);

    if (m_pLobbyAuthority &&
        (m_pLobbyAuthority->GetPhase() == eRoomPhase::SeatSelect ||
            m_pLobbyAuthority->GetPhase() == eRoomPhase::ChampionSelect))
    {
        char msg[128]{};
        sprintf_s(msg, "[GameRoom] Lobby join sid=%u\n", sessionId);
        OutputServerAITrace(msg);
        return NULL_ENTITY;
    }

    if (!bHadKnownSlot)
    {
        char msg[160]{};
        sprintf_s(msg,
            "[GameRoom] Session sid=%u connected without available slot phase=%u\n",
            sessionId,
            m_pLobbyAuthority ? static_cast<u32_t>(m_pLobbyAuthority->GetPhase()) : 0u);
        OutputServerAITrace(msg);
    }

    return NULL_ENTITY;
}

void CGameRoom::OnSessionLeave(u32_t sessionId)
{
    std::lock_guard stateLock(m_stateMutex);

    m_sessionBinding.Unbind(sessionId);
    m_sessionIds.erase(
        std::remove(m_sessionIds.begin(), m_sessionIds.end(), sessionId),
        m_sessionIds.end());

    if (m_pLobbyAuthority)
    {
        const LobbyAuthorityResult result =
            m_pLobbyAuthority->OnSessionLeave(sessionId);
        ApplyLobbyAuthorityResult(result);
    }

    char msg[128]{};
    sprintf_s(msg, "[GameRoom] OnSessionLeave sid=%u\n", sessionId);
    OutputServerAITrace(msg);
}

void CGameRoom::ApplyLobbyAuthorityResult(const LobbyAuthorityResult& result)
{
    if (!m_pLobbyAuthority)
        return;

    if (result.bStopReplay)
        FinalizeReplayRecorder();

    if (result.bBeginLoading)
    {
        SpawnChampionsFromLobby();
        SpawnServerGameplayObjects();

        const LobbySlotState* pSlots = GetLobbySlots();
        const u32_t slotCount = GetLobbySlotCount();
        for (u32_t i = 0; i < slotCount; ++i)
        {
            const LobbySlotState& slot = pSlots[i];
            if (slot.bHuman && slot.sessionId != 0)
                SendHelloToSessionLocked(slot.sessionId, slot.netId, slot.champion, slot.team);
        }

        char msg[128]{};
        sprintf_s(msg,
            "[GameRoom] StartGame loading revision=%u\n",
            m_pLobbyAuthority->GetRevision());
        OutputServerAITrace(msg);
    }

    if (result.bBeginInGame)
    {
        if (m_bGameplayObjectsSpawned)
        {
            m_serverMinionWaves.ScheduleFirstWave(
                m_tickIndex,
                [](const char* pText)
                {
                    OutputServerAITrace(pText);
                });
        }

        char msg[128]{};
        sprintf_s(msg,
            "[GameRoom] BeginInGame all clients ready revision=%u\n",
            m_pLobbyAuthority->GetRevision());
        OutputServerAITrace(msg);
    }

    TraceLobbyMessageLocked();

    if (result.bSendHello)
    {
        SendHelloToSessionLocked(
            result.sessionId,
            result.helloNetId,
            result.helloChampion,
            result.helloTeam);
    }

    if (result.bBroadcastLobbyState)
        BroadcastLobbyStateLocked();

    if (result.bSendGameStart || result.bBeginInGame)
        BroadcastGameStartLocked();
}

void CGameRoom::TraceLobbyMessageLocked() const
{
    if (!m_pLobbyAuthority)
        return;

    const std::string& message = m_pLobbyAuthority->GetLastMessage();
    if (message.empty())
        return;

    char msg[512]{};
    sprintf_s(msg, "[Lobby] %s\n", message.c_str());
    OutputServerAITrace(msg);
}

void CGameRoom::BroadcastLobbyStateLocked()
{
    if (!m_pLobbyAuthority)
        return;

    flatbuffers::FlatBufferBuilder fbb(1024);
    std::vector<flatbuffers::Offset<Shared::Schema::LobbySlot>> slots;
    slots.reserve(kGameRosterSlotCount);

    const LobbySlotState* pSlots = m_pLobbyAuthority->GetSlots();
    for (u32_t i = 0; i < m_pLobbyAuthority->GetSlotCount(); ++i)
    {
        const LobbySlotState& slot = pSlots[i];
        const auto seatKind = slot.bHuman
            ? Shared::Schema::LobbySeatKind::Human
            : (slot.bBot ? Shared::Schema::LobbySeatKind::Bot : Shared::Schema::LobbySeatKind::Empty);

        slots.push_back(Shared::Schema::CreateLobbySlot(
            fbb,
            slot.slotId,
            slot.team,
            seatKind,
            slot.sessionId,
            slot.netId,
            static_cast<u8_t>(slot.champion),
            slot.botDifficulty,
            slot.botLane,
            0,
            slot.bReady,
            slot.bLocked));
    }

    const auto slotVec = fbb.CreateVector(slots);
    const auto debugMessage = fbb.CreateString(m_pLobbyAuthority->GetLastMessage());
    const auto state = Shared::Schema::CreateLobbyState(
        fbb,
        m_roomId,
        m_pLobbyAuthority->GetRevision(),
        m_pLobbyAuthority->GetHostSessionId(),
        ToSchemaLobbyPhase(m_pLobbyAuthority->GetPhase()),
        m_pLobbyAuthority->CanAllPlayersEditBots(),
        slotVec,
        0,
        debugMessage);
    fbb.Finish(state);
    auto buffer = fbb.Release();

    const auto packet = WrapEnvelope(
        ePacketType::LobbyState,
        m_pLobbyAuthority->GetRevision(),
        buffer.data(),
        static_cast<u32_t>(buffer.size()));

    for (u32_t sid : m_sessionIds)
    {
        if (auto pSession = CSession_Manager::Get()->Find(sid))
            pSession->Send(std::vector<u8_t>(packet.begin(), packet.end()));
    }
}

void CGameRoom::BroadcastGameStartLocked()
{
    const u32_t revision = m_pLobbyAuthority ? m_pLobbyAuthority->GetRevision() : 0u;
    const auto packet = WrapEnvelope(ePacketType::GameStart, revision, nullptr, 0);
    for (u32_t sid : m_sessionIds)
    {
        if (auto pSession = CSession_Manager::Get()->Find(sid))
            pSession->Send(std::vector<u8_t>(packet.begin(), packet.end()));
    }
}

void CGameRoom::SendGameStartToSessionLocked(u32_t sessionId)
{
    auto pSession = CSession_Manager::Get()->Find(sessionId);
    if (!pSession)
        return;

    const u32_t revision = m_pLobbyAuthority ? m_pLobbyAuthority->GetRevision() : 0u;
    pSession->Send(WrapEnvelope(ePacketType::GameStart, revision, nullptr, 0));
}

void CGameRoom::SendHelloToSessionLocked(
    u32_t sessionId,
    NetEntityId netId,
    eChampion champion,
    u8_t team)
{
    auto pSession = CSession_Manager::Get()->Find(sessionId);
    if (!pSession)
        return;

    flatbuffers::FlatBufferBuilder fbb(128);
    const auto hello = Shared::Schema::CreateHello(
        fbb,
        sessionId,
        netId,
        m_tickIndex,
        ResolveServerGameTimeMs(m_tickIndex),
        static_cast<u8_t>(champion),
        team);
    fbb.Finish(hello);
    auto helloBuffer = fbb.Release();

    const u32_t revision = m_pLobbyAuthority ? m_pLobbyAuthority->GetRevision() : 0u;
    auto packet = WrapEnvelope(
        ePacketType::Hello,
        revision,
        helloBuffer.data(),
        static_cast<u32_t>(helloBuffer.size()));
    pSession->Send(std::move(packet));
}

bool CGameRoom::TryAttachSessionToDisconnectedHumanSlot(
    u32_t sessionId,
    EntityID& outEntity,
    LobbyAuthorityResult& outResult)
{
    outEntity = NULL_ENTITY;
    outResult = LobbyAuthorityResult{};

    if (!m_pLobbyAuthority)
        return false;

    u8_t slotId = kInvalidGameRosterSlot;
    NetEntityId netId = NULL_NET_ENTITY;
    eChampion champion = eChampion::END;
    u8_t team = 0;
    if (!m_pLobbyAuthority->TryFindDisconnectedHumanSlot(slotId, netId, champion, team))
        return false;

    const EntityID entity = m_entityMap.FromNet(netId);
    if (entity == NULL_ENTITY || !m_world.IsAlive(entity))
        return false;

    outResult = m_pLobbyAuthority->AttachDisconnectedHumanSlot(sessionId, slotId);
    if (!outResult.bSendHello)
        return false;

    m_sessionBinding.Bind(sessionId, entity);
    outEntity = entity;
    return true;
}

LobbySlotState* CGameRoom::GetLobbySlots()
{
    return m_pLobbyAuthority ? m_pLobbyAuthority->GetSlots() : nullptr;
}

const LobbySlotState* CGameRoom::GetLobbySlots() const
{
    return m_pLobbyAuthority ? m_pLobbyAuthority->GetSlots() : nullptr;
}

u32_t CGameRoom::GetLobbySlotCount() const
{
    return m_pLobbyAuthority ? m_pLobbyAuthority->GetSlotCount() : 0u;
}
