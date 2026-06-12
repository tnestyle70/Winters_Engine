#include "Game/GameRoom.h"

#include "GameRoomSmokeRoster.h"
#include "Network/PacketDispatcher.h"
#include "Network/Session.h"
#include "Network/Session_Manager.h"
#include "Shared/GameSim/Definitions/MapSpawnPoints.h"
#include "Shared/Network/PacketEnvelope.h"
#include "Shared/Schemas/Generated/cpp/Hello_generated.h"
#include "Shared/Schemas/Generated/cpp/LobbyCommand_generated.h"
#include "Shared/Schemas/Generated/cpp/LobbyState_generated.h"

#include <cstdio>
#include <flatbuffers/flatbuffers.h>
#include <mutex>
#include <string>
#include <vector>

namespace
{
    void OutputServerAITrace(const char* pText)
    {
        if (!pText)
            return;

        WintersOutputAIDebugStringA(pText);
    }

    const char* GetLobbyCommandKindName(Shared::Schema::LobbyCommandKind kind)
    {
        switch (kind)
        {
        case Shared::Schema::LobbyCommandKind::JoinSlot:
            return "JoinSlot";
        case Shared::Schema::LobbyCommandKind::LeaveSlot:
            return "LeaveSlot";
        case Shared::Schema::LobbyCommandKind::PickChampion:
            return "PickChampion";
        case Shared::Schema::LobbyCommandKind::SetBotChampion:
            return "SetBotChampion";
        case Shared::Schema::LobbyCommandKind::SetBotDifficulty:
            return "SetBotDifficulty";
        case Shared::Schema::LobbyCommandKind::SetBotLane:
            return "SetBotLane";
        case Shared::Schema::LobbyCommandKind::SetReady:
            return "SetReady";
        case Shared::Schema::LobbyCommandKind::StopReplay:
            return "StopReplay";
        case Shared::Schema::LobbyCommandKind::StartGame:
            return "StartGame";
        case Shared::Schema::LobbyCommandKind::CancelStart:
            return "CancelStart";
        case Shared::Schema::LobbyCommandKind::SetEditPolicy:
            return "SetEditPolicy";
        default:
            return "None";
        }
    }

    std::string FormatLobbyCommandLog(
        const char* result,
        u32_t sessionId,
        Shared::Schema::LobbyCommandKind kind,
        u8_t slotId,
        eChampion champion,
        const char* reason)
    {
        char text[320]{};
        sprintf_s(
            text,
            "%s sid=%u cmd=%s slot=%u champ=%u reason=%s",
            result,
            sessionId,
            GetLobbyCommandKindName(kind),
            static_cast<u32_t>(slotId),
            static_cast<u32_t>(champion),
            reason ? reason : "-");
        return std::string(text);
    }

    bool_t IsValidLobbyBotLane(u8_t lane)
    {
        return lane == kGameSimLaneTop ||
            lane == kGameSimLaneMid ||
            lane == kGameSimLaneBot;
    }

    u8_t GetDefaultLobbyBotLane(u8_t slotId)
    {
        switch (slotId % 5u)
        {
        case 1:
            return kGameSimLaneTop;
        case 2:
            return kGameSimLaneMid;
        case 3:
        case 4:
            return kGameSimLaneBot;
        default:
            return kGameSimLaneMid;
        }
    }
}

void CGameRoom::OnLobbyCommand(u32_t sessionId, const Shared::Schema::LobbyCommand* command)
{
    if (!command)
        return;

    std::lock_guard stateLock(m_stateMutex);

    const bool_t bLobbyEditablePhase =
        m_roomPhase == eRoomPhase::SeatSelect ||
        m_roomPhase == eRoomPhase::ChampionSelect;

    if (!bLobbyEditablePhase)
    {
        if (m_roomPhase == eRoomPhase::Loading &&
            command->kind() == Shared::Schema::LobbyCommandKind::SetReady)
        {
            if (TrySetReady(sessionId, command->value() != 0))
                return;
            ++m_lobbyRevision;
            BroadcastLobbyStateLocked();
            return;
        }
        if (command->kind() == Shared::Schema::LobbyCommandKind::StopReplay)
        {
            if (TryStopReplay(sessionId))
                return;

            ++m_lobbyRevision;
            BroadcastLobbyStateLocked();
            return;
        }
        SetLobbyMessageLocked(FormatLobbyCommandLog(
            "reject",
            sessionId,
            command->kind(),
            command->slotId(),
            static_cast<eChampion>(command->championId()),
            "room is not in lobby phase"));
        ++m_lobbyRevision;
        BroadcastLobbyStateLocked();
        return;
    }

    bool bChanged = false;
    m_strLastLobbyMessage.clear();

    switch (command->kind())
    {
    case Shared::Schema::LobbyCommandKind::JoinSlot:
        if (m_roomPhase != eRoomPhase::SeatSelect)
        {
            SetLobbyMessageLocked(FormatLobbyCommandLog(
                "reject",
                sessionId,
                command->kind(),
                command->slotId(),
                static_cast<eChampion>(command->championId()),
                "slot changes are only allowed in seat select"));
            break;
        }
        bChanged = TryJoinSlot(sessionId, command->slotId());
        break;
    case Shared::Schema::LobbyCommandKind::LeaveSlot:
        if (m_roomPhase != eRoomPhase::SeatSelect)
        {
            SetLobbyMessageLocked(FormatLobbyCommandLog(
                "reject",
                sessionId,
                command->kind(),
                command->slotId(),
                static_cast<eChampion>(command->championId()),
                "slot changes are only allowed in seat select"));
            break;
        }
        bChanged = TryLeaveSlot(sessionId);
        break;
    case Shared::Schema::LobbyCommandKind::PickChampion:
        if (m_roomPhase != eRoomPhase::ChampionSelect)
        {
            SetLobbyMessageLocked(FormatLobbyCommandLog(
                "reject",
                sessionId,
                command->kind(),
                command->slotId(),
                static_cast<eChampion>(command->championId()),
                "player champion pick is only allowed in champion select"));
            break;
        }
        bChanged = TryPickChampion(sessionId, static_cast<eChampion>(command->championId()));
        break;
    case Shared::Schema::LobbyCommandKind::SetBotChampion:
        bChanged = TrySetBotChampion(
            sessionId,
            command->slotId(),
            static_cast<eChampion>(command->championId()));
        break;
    case Shared::Schema::LobbyCommandKind::SetReady:
        if (TrySetReady(sessionId, command->value() != 0))
            return;
        break;
    case Shared::Schema::LobbyCommandKind::SetBotDifficulty:
        bChanged = TrySetBotDifficulty(sessionId, command->slotId(), command->botDifficulty());
        break;
    case Shared::Schema::LobbyCommandKind::SetBotLane:
        bChanged = TrySetBotLane(sessionId, command->slotId(), static_cast<u8_t>(command->value()));
        break;
    case Shared::Schema::LobbyCommandKind::StartGame:
        if (m_roomPhase == eRoomPhase::SeatSelect)
        {
            if (TryAdvanceToChampionSelect(sessionId))
                return;
        }
        else if (m_roomPhase == eRoomPhase::ChampionSelect)
        {
            if (TryStartGame(sessionId))
                return;
        }
        else
        {
            SetLobbyMessageLocked(FormatLobbyCommandLog(
                "reject",
                sessionId,
                command->kind(),
                command->slotId(),
                static_cast<eChampion>(command->championId()),
                "room is not ready to advance"));
        }
        break;
    default:
        SetLobbyMessageLocked(FormatLobbyCommandLog(
            "reject",
            sessionId,
            command->kind(),
            command->slotId(),
            static_cast<eChampion>(command->championId()),
            "unsupported lobby command"));
        break;
    }

    if (bChanged)
    {
        if (m_strLastLobbyMessage.empty())
        {
            SetLobbyMessageLocked(FormatLobbyCommandLog(
                "accept",
                sessionId,
                command->kind(),
                command->slotId(),
                static_cast<eChampion>(command->championId()),
                "state changed"));
        }
        ++m_lobbyRevision;
        BroadcastLobbyStateLocked();
    }
    else
    {
        if (m_strLastLobbyMessage.empty())
        {
            SetLobbyMessageLocked(FormatLobbyCommandLog(
                "reject",
                sessionId,
                command->kind(),
                command->slotId(),
                static_cast<eChampion>(command->championId()),
                "no state change"));
        }
        ++m_lobbyRevision;
        BroadcastLobbyStateLocked();
    }
}

EntityID CGameRoom::OnSessionJoin(u32_t sessionId)
{
    std::lock_guard stateLock(m_stateMutex);

    if (auto it = m_sessionToEntity.find(sessionId);
        it != m_sessionToEntity.end() && it->second != NULL_ENTITY)
        return it->second;

    if (std::find(m_sessionIds.begin(), m_sessionIds.end(), sessionId) == m_sessionIds.end())
        m_sessionIds.push_back(sessionId);
    std::sort(m_sessionIds.begin(), m_sessionIds.end());

    CPacketDispatcher::Instance().RouteSession(sessionId, m_roomId);

    if (m_roomPhase == eRoomPhase::SeatSelect ||
        m_roomPhase == eRoomPhase::ChampionSelect)
    {
        OnLobbyJoin(sessionId);
        ++m_lobbyRevision;
        SendHelloToSessionLocked(sessionId, NULL_NET_ENTITY, eChampion::END, 0);
        BroadcastLobbyStateLocked();

        char msg[128]{};
        sprintf_s(msg, "[GameRoom] Lobby join sid=%u\n", sessionId);
        OutputServerAITrace(msg);
        return NULL_ENTITY;
    }

    EntityID lateJoinEntity = NULL_ENTITY;
    if (TryAttachSessionToDisconnectedHumanSlot(sessionId, lateJoinEntity))
    {
        const LobbySlotState& slot = m_lobbySlots[m_sessionToSlot[sessionId]];
        ++m_lobbyRevision;
        SendHelloToSessionLocked(sessionId, slot.netId, slot.champion, slot.team);
        BroadcastLobbyStateLocked();
        if (m_roomPhase == eRoomPhase::InGame)
            SendGameStartToSessionLocked(sessionId);

        char msg[192]{};
        sprintf_s(msg,
            "[GameRoom] Late session attach sid=%u slot=%u net=%u entity=%u phase=%u\n",
            sessionId,
            static_cast<u32_t>(slot.slotId),
            slot.netId,
            static_cast<u32_t>(lateJoinEntity),
            static_cast<u32_t>(m_roomPhase));
        OutputServerAITrace(msg);
        return lateJoinEntity;
    }

    auto slotIt = m_sessionToSlot.find(sessionId);
    if (slotIt != m_sessionToSlot.end())
    {
        const LobbySlotState& slot = m_lobbySlots[slotIt->second];
        SendHelloToSessionLocked(sessionId, slot.netId, slot.champion, slot.team);
    }
    else
    {
        SendHelloToSessionLocked(sessionId, NULL_NET_ENTITY, eChampion::END, 0);
        BroadcastLobbyStateLocked();

        char msg[160]{};
        sprintf_s(msg,
            "[GameRoom] Session sid=%u connected without available slot phase=%u\n",
            sessionId,
            static_cast<u32_t>(m_roomPhase));
        OutputServerAITrace(msg);
    }

    return NULL_ENTITY;
}

void CGameRoom::OnSessionLeave(u32_t sessionId)
{
    std::lock_guard stateLock(m_stateMutex);

    m_sessionToEntity.erase(sessionId);
    m_sessionIds.erase(
        std::remove(m_sessionIds.begin(), m_sessionIds.end(), sessionId),
        m_sessionIds.end());

    if (auto slotIt = m_sessionToSlot.find(sessionId); slotIt != m_sessionToSlot.end())
    {
        LobbySlotState& slot = m_lobbySlots[slotIt->second];
        const u32_t beginSlot = slot.team == 0 ? 0u : 5u;
        const u32_t endSlot = slot.team == 0 ? 5u : 10u;
        if (slot.bHuman && slot.sessionId == sessionId)
        {
            if (m_roomPhase == eRoomPhase::SeatSelect ||
                m_roomPhase == eRoomPhase::ChampionSelect)
            {
                slot.bHuman = false;
                slot.sessionId = 0;
                slot.netId = NULL_NET_ENTITY;
                slot.champion = eChampion::END;
                slot.botDifficulty = 2;
                slot.botLane = GetDefaultLobbyBotLane(slot.slotId);
                slot.bReady = false;
                slot.bLocked = false;
            }
            else
            {
                slot.sessionId = 0;
                slot.bReady = false;
            }
        }
        m_sessionToSlot.erase(slotIt);
        if (m_roomPhase == eRoomPhase::SeatSelect ||
            m_roomPhase == eRoomPhase::ChampionSelect)
        {
            CompactLobbyTeamSlotsLocked(beginSlot, endSlot);
        }
        SetLobbyMessageLocked(FormatLobbyCommandLog(
            "accept",
            sessionId,
            Shared::Schema::LobbyCommandKind::LeaveSlot,
            slot.slotId,
            eChampion::END,
            (m_roomPhase == eRoomPhase::SeatSelect ||
                m_roomPhase == eRoomPhase::ChampionSelect)
                ? "disconnected; slot freed"
                : "disconnected; slot reserved for reconnect"));
        ++m_lobbyRevision;
        BroadcastLobbyStateLocked();
    }

    char msg[128]{};
    sprintf_s(msg, "[GameRoom] OnSessionLeave sid=%u\n", sessionId);
    OutputServerAITrace(msg);
}

void CGameRoom::InitializeLobbySlots()
{
    for (u32_t i = 0; i < kGameRosterSlotCount; ++i)
    {
        LobbySlotState& slot = m_lobbySlots[i];
        slot = LobbySlotState{};
        slot.slotId = static_cast<u8_t>(i);
        slot.team = static_cast<u8_t>(i < 5 ? 0 : 1);
        slot.botDifficulty = 2;
        slot.botLane = GetDefaultLobbyBotLane(slot.slotId);
    }
}

u8_t CGameRoom::FindFirstEmptyLobbySlot(u32_t beginSlot, u32_t endSlot) const
{
    for (u32_t i = beginSlot; i < endSlot && i < kGameRosterSlotCount; ++i)
    {
        const LobbySlotState& slot = m_lobbySlots[i];
        if (!slot.bHuman && !slot.bBot)
            return static_cast<u8_t>(i);
    }

    return kInvalidGameRosterSlot;
}

void CGameRoom::CompactLobbyTeamSlotsLocked(u32_t beginSlot, u32_t endSlot)
{
    std::vector<LobbySlotState> occupied;
    for (u32_t i = beginSlot; i < endSlot && i < kGameRosterSlotCount; ++i)
    {
        if (m_lobbySlots[i].bHuman || m_lobbySlots[i].bBot)
            occupied.push_back(m_lobbySlots[i]);
    }

    for (u32_t i = beginSlot; i < endSlot && i < kGameRosterSlotCount; ++i)
    {
        LobbySlotState reset{};
        reset.slotId = static_cast<u8_t>(i);
        reset.team = static_cast<u8_t>(i < 5 ? 0 : 1);
        reset.botDifficulty = 2;
        reset.botLane = GetDefaultLobbyBotLane(reset.slotId);
        m_lobbySlots[i] = reset;
    }

    u32_t occupiedIndex = 0;
    for (u32_t i = beginSlot; i < endSlot && i < kGameRosterSlotCount && occupiedIndex < occupied.size(); ++i)
    {
        m_lobbySlots[i] = occupied[occupiedIndex++];
        m_lobbySlots[i].slotId = static_cast<u8_t>(i);
        m_lobbySlots[i].team = static_cast<u8_t>(i < 5 ? 0 : 1);
        if (m_lobbySlots[i].bHuman)
            m_sessionToSlot[m_lobbySlots[i].sessionId] = static_cast<u8_t>(i);
    }
}

void CGameRoom::OnLobbyJoin(u32_t sessionId)
{
    if (m_hostSessionId == 0)
    {
        m_hostSessionId = sessionId;
        TryJoinSlot(sessionId, 0);
        return;
    }

    const u8_t blueSlot = FindFirstEmptyLobbySlot(0, 5);
    if (blueSlot < kGameRosterSlotCount)
    {
        TryJoinSlot(sessionId, blueSlot);
        return;
    }

    const u8_t redSlot = FindFirstEmptyLobbySlot(5, 10);
    if (redSlot < kGameRosterSlotCount)
    {
        TryJoinSlot(sessionId, redSlot);
        return;
    }

    SetLobbyMessageLocked(FormatLobbyCommandLog(
        "accept",
        sessionId,
        Shared::Schema::LobbyCommandKind::None,
        kInvalidGameRosterSlot,
        eChampion::END,
        "connected; choose a slot"));
}

bool CGameRoom::TryJoinSlot(u32_t sessionId, u8_t slotId)
{
    if (slotId >= kGameRosterSlotCount)
    {
        SetLobbyMessageLocked(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::JoinSlot,
            slotId,
            eChampion::END,
            "invalid slot"));
        return false;
    }

    LobbySlotState& target = m_lobbySlots[slotId];
    if (target.bHuman && target.sessionId != sessionId)
    {
        SetLobbyMessageLocked(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::JoinSlot,
            slotId,
            target.champion,
            "slot is occupied by another player"));
        return false;
    }
    if (target.bBot)
    {
        SetLobbyMessageLocked(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::JoinSlot,
            slotId,
            target.champion,
            "slot is occupied by a bot"));
        return false;
    }
    if (target.bLocked)
    {
        SetLobbyMessageLocked(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::JoinSlot,
            slotId,
            target.champion,
            "slot is locked"));
        return false;
    }

    eChampion previousChampion = eChampion::END;
    bool_t bHadPreviousSlot = false;
    u32_t previousBeginSlot = 0;
    u32_t previousEndSlot = 0;
    if (auto prevIt = m_sessionToSlot.find(sessionId); prevIt != m_sessionToSlot.end())
    {
        LobbySlotState& oldSlot = m_lobbySlots[prevIt->second];
        previousChampion = oldSlot.champion;
        bHadPreviousSlot = true;
        previousBeginSlot = oldSlot.team == 0 ? 0u : 5u;
        previousEndSlot = oldSlot.team == 0 ? 5u : 10u;
        oldSlot.bHuman = false;
        oldSlot.sessionId = 0;
        oldSlot.netId = NULL_NET_ENTITY;
        oldSlot.champion = eChampion::END;
        oldSlot.botDifficulty = 2;
        oldSlot.botLane = GetDefaultLobbyBotLane(oldSlot.slotId);
        oldSlot.bReady = false;
        oldSlot.bLocked = false;
        m_sessionToSlot.erase(prevIt);
        if (bHadPreviousSlot && previousBeginSlot != (slotId < 5 ? 0u : 5u))
            CompactLobbyTeamSlotsLocked(previousBeginSlot, previousEndSlot);
    }

    LobbySlotState& joinTarget = m_lobbySlots[slotId];
    joinTarget.bHuman = true;
    joinTarget.bBot = false;
    joinTarget.sessionId = sessionId;
    joinTarget.netId = NULL_NET_ENTITY;
    joinTarget.champion = (previousChampion != eChampion::END && previousChampion != eChampion::NONE)
        ? previousChampion
        : joinTarget.champion;
    joinTarget.bReady = true;
    joinTarget.bLocked = false;
    m_sessionToSlot[sessionId] = slotId;
    SetLobbyMessageLocked(FormatLobbyCommandLog(
        "accept",
        sessionId,
        Shared::Schema::LobbyCommandKind::JoinSlot,
        slotId,
        joinTarget.champion,
        "joined slot"));
    return true;
}

bool CGameRoom::TryLeaveSlot(u32_t sessionId)
{
    auto it = m_sessionToSlot.find(sessionId);
    if (it == m_sessionToSlot.end())
    {
        SetLobbyMessageLocked(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::LeaveSlot,
            kInvalidGameRosterSlot,
            eChampion::END,
            "session has no slot"));
        return false;
    }

    LobbySlotState& slot = m_lobbySlots[it->second];
    const u8_t slotId = slot.slotId;
    const u32_t beginSlot = slot.team == 0 ? 0u : 5u;
    const u32_t endSlot = slot.team == 0 ? 5u : 10u;
    slot.bHuman = false;
    slot.sessionId = 0;
    slot.netId = NULL_NET_ENTITY;
    slot.champion = eChampion::END;
    slot.botDifficulty = 2;
    slot.botLane = GetDefaultLobbyBotLane(slot.slotId);
    slot.bReady = false;
    slot.bLocked = false;
    m_sessionToSlot.erase(it);
    CompactLobbyTeamSlotsLocked(beginSlot, endSlot);
    SetLobbyMessageLocked(FormatLobbyCommandLog(
        "accept",
        sessionId,
        Shared::Schema::LobbyCommandKind::LeaveSlot,
        slotId,
        eChampion::END,
        "left slot"));
    return true;
}

bool CGameRoom::TryPickChampion(u32_t sessionId, eChampion champion)
{
    if (champion == eChampion::END || champion == eChampion::NONE)
    {
        SetLobbyMessageLocked(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::PickChampion,
            kInvalidGameRosterSlot,
            champion,
            "invalid champion"));
        return false;
    }

    auto it = m_sessionToSlot.find(sessionId);
    if (it == m_sessionToSlot.end())
    {
        SetLobbyMessageLocked(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::PickChampion,
            kInvalidGameRosterSlot,
            champion,
            "session has no slot"));
        return false;
    }

    m_lobbySlots[it->second].champion = champion;
    SetLobbyMessageLocked(FormatLobbyCommandLog(
        "accept",
        sessionId,
        Shared::Schema::LobbyCommandKind::PickChampion,
        m_lobbySlots[it->second].slotId,
        champion,
        "picked champion"));
    return true;
}

bool CGameRoom::TrySetBotChampion(u32_t sessionId, u8_t slotId, eChampion champion)
{
    if (!CanEditBots(sessionId) || slotId >= kGameRosterSlotCount)
    {
        SetLobbyMessageLocked(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::SetBotChampion,
            slotId,
            champion,
            !CanEditBots(sessionId) ? "no bot edit permission" : "invalid slot"));
        return false;
    }

    LobbySlotState& slot = m_lobbySlots[slotId];
    if (slot.bHuman)
    {
        SetLobbyMessageLocked(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::SetBotChampion,
            slotId,
            champion,
            "slot is occupied by a player"));
        return false;
    }

    if (m_roomPhase == eRoomPhase::ChampionSelect)
    {
        if (!slot.bBot)
        {
            SetLobbyMessageLocked(FormatLobbyCommandLog(
                "reject",
                sessionId,
                Shared::Schema::LobbyCommandKind::SetBotChampion,
                slotId,
                champion,
                "only existing bot champion can be changed in champion select"));
            return false;
        }

        if (champion == eChampion::END || champion == eChampion::NONE)
        {
            SetLobbyMessageLocked(FormatLobbyCommandLog(
                "reject",
                sessionId,
                Shared::Schema::LobbyCommandKind::SetBotChampion,
                slotId,
                champion,
                "bot cannot be removed in champion select"));
            return false;
        }
    }

    if (champion == eChampion::END || champion == eChampion::NONE)
    {
        const u32_t beginSlot = slot.team == 0 ? 0u : 5u;
        const u32_t endSlot = slot.team == 0 ? 5u : 10u;
        slot.bBot = false;
        slot.champion = eChampion::END;
        slot.botDifficulty = 2;
        slot.botLane = GetDefaultLobbyBotLane(slot.slotId);
        CompactLobbyTeamSlotsLocked(beginSlot, endSlot);
        SetLobbyMessageLocked(FormatLobbyCommandLog(
            "accept",
            sessionId,
            Shared::Schema::LobbyCommandKind::SetBotChampion,
            slotId,
            champion,
            "removed bot"));
        return true;
    }

    slot.bBot = true;
    slot.sessionId = 0;
    slot.netId = NULL_NET_ENTITY;
    slot.champion = champion;
    if (slot.botDifficulty == 0)
        slot.botDifficulty = 2;
    if (!IsValidLobbyBotLane(slot.botLane))
        slot.botLane = GetDefaultLobbyBotLane(slot.slotId);
    SetLobbyMessageLocked(FormatLobbyCommandLog(
        "accept",
        sessionId,
        Shared::Schema::LobbyCommandKind::SetBotChampion,
        slotId,
        champion,
        "set bot champion"));
    return true;
}

bool CGameRoom::TrySetBotLane(u32_t sessionId, u8_t slotId, u8_t lane)
{
    if (!CanEditBots(sessionId) || slotId >= kGameRosterSlotCount || !IsValidLobbyBotLane(lane))
    {
        SetLobbyMessageLocked(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::SetBotLane,
            slotId,
            eChampion::END,
            !CanEditBots(sessionId) ? "no bot edit permission" : "invalid bot lane"));
        return false;
    }

    LobbySlotState& slot = m_lobbySlots[slotId];
    if (!slot.bBot)
    {
        SetLobbyMessageLocked(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::SetBotLane,
            slotId,
            slot.champion,
            "slot is not a bot"));
        return false;
    }

    slot.botLane = lane;
    SetLobbyMessageLocked(FormatLobbyCommandLog(
        "accept",
        sessionId,
        Shared::Schema::LobbyCommandKind::SetBotLane,
        slotId,
        slot.champion,
        "set bot lane"));
    return true;
}

bool CGameRoom::TrySetBotDifficulty(u32_t sessionId, u8_t slotId, u8_t difficulty)
{
    if (!CanEditBots(sessionId) || slotId >= kGameRosterSlotCount)
    {
        SetLobbyMessageLocked(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::SetBotDifficulty,
            slotId,
            eChampion::END,
            !CanEditBots(sessionId) ? "no bot edit permission" : "invalid slot"));
        return false;
    }

    LobbySlotState& slot = m_lobbySlots[slotId];
    if (!slot.bBot)
    {
        SetLobbyMessageLocked(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::SetBotDifficulty,
            slotId,
            slot.champion,
            "slot is not a bot"));
        return false;
    }

    slot.botDifficulty = difficulty ? difficulty : 2;
    SetLobbyMessageLocked(FormatLobbyCommandLog(
        "accept",
        sessionId,
        Shared::Schema::LobbyCommandKind::SetBotDifficulty,
        slotId,
        slot.champion,
        "set bot difficulty"));
    return true;
}

bool CGameRoom::TryAdvanceToChampionSelect(u32_t sessionId)
{
    if (sessionId != m_hostSessionId && !m_bAllPlayersCanEditBots)
    {
        SetLobbyMessageLocked(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::StartGame,
            kInvalidGameRosterSlot,
            eChampion::END,
            "only host can advance to champion select"));
        return false;
    }

    bool_t bHasHuman = false;
    for (const LobbySlotState& slot : m_lobbySlots)
    {
        if (slot.bHuman)
        {
            bHasHuman = true;
            break;
        }
    }

    if (!bHasHuman)
    {
        SetLobbyMessageLocked(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::StartGame,
            kInvalidGameRosterSlot,
            eChampion::END,
            "no human player"));
        return false;
    }

    m_roomPhase = eRoomPhase::ChampionSelect;
    ++m_lobbyRevision;

    SetLobbyMessageLocked(FormatLobbyCommandLog(
        "accept",
        sessionId,
        Shared::Schema::LobbyCommandKind::StartGame,
        kInvalidGameRosterSlot,
        eChampion::END,
        "champion select started"));

    BroadcastLobbyStateLocked();
    return true;
}

bool CGameRoom::TryStartGame(u32_t sessionId)
{
    if (sessionId != m_hostSessionId && !m_bAllPlayersCanEditBots)
    {
        SetLobbyMessageLocked(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::StartGame,
            kInvalidGameRosterSlot,
            eChampion::END,
            "only host can start"));
        return false;
    }

    bool_t bHasHuman = false;
    for (const LobbySlotState& slot : m_lobbySlots)
    {
        if (slot.bHuman)
        {
            bHasHuman = true;
            break;
        }
    }
    if (!bHasHuman)
    {
        SetLobbyMessageLocked(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::StartGame,
            kInvalidGameRosterSlot,
            eChampion::END,
            "no human player"));
        return false;
    }

    if (ShouldUseRedSylasSmokeRoster())
        EnsureRedSylasSmokeRoster(m_lobbySlots, kGameRosterSlotCount);

    for (u32_t i = 0; i < kGameRosterSlotCount; ++i)
    {
        LobbySlotState& slot = m_lobbySlots[i];
        if (!slot.bHuman && !slot.bBot)
        {
            slot.bLocked = false;
            slot.bReady = false;
            slot.champion = eChampion::END;
            continue;
        }

        if (slot.champion == eChampion::END || slot.champion == eChampion::NONE)
        {
            SetLobbyMessageLocked(FormatLobbyCommandLog(
                "reject",
                sessionId,
                Shared::Schema::LobbyCommandKind::StartGame,
                slot.slotId,
                slot.champion,
                slot.bHuman ? "human slot has no champion" : "bot slot has no champion"));
            return false;
        }

        slot.bLocked = true;

        char msg[192]{};
        sprintf_s(msg,
            "[GameRoom] LockSlot slot=%u team=%u human=%u bot=%u champ=%u\n",
            static_cast<u32_t>(slot.slotId),
            static_cast<u32_t>(slot.team),
            slot.bHuman ? 1u : 0u,
            slot.bBot ? 1u : 0u,
            static_cast<u32_t>(slot.champion));
        OutputServerAITrace(msg);
    }

    m_roomPhase = eRoomPhase::Loading;
    ++m_lobbyRevision;

    for (LobbySlotState& slot : m_lobbySlots)
    {
        if (slot.bHuman)
            slot.bReady = false;
    }

    SpawnChampionsFromLobby();
    SpawnServerGameplayObjects();
    for (const LobbySlotState& slot : m_lobbySlots)
    {
        if (slot.bHuman && slot.sessionId != 0)
            SendHelloToSessionLocked(slot.sessionId, slot.netId, slot.champion, slot.team);
    }

    SetLobbyMessageLocked(FormatLobbyCommandLog(
        "accept",
        sessionId,
        Shared::Schema::LobbyCommandKind::StartGame,
        kInvalidGameRosterSlot,
        eChampion::END,
        "loading started"));

    BroadcastLobbyStateLocked();

    char msg[128]{};
    sprintf_s(msg, "[GameRoom] StartGame loading revision=%u\n", m_lobbyRevision);
    OutputServerAITrace(msg);
    return true;
}

bool CGameRoom::TryStopReplay(u32_t sessionId)
{
    if (sessionId != m_hostSessionId && !m_bAllPlayersCanEditBots)
    {
        SetLobbyMessageLocked(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::StopReplay,
            kInvalidGameRosterSlot,
            eChampion::END,
            "only host can stop replay"));
        return false;
    }

    if (m_roomPhase != eRoomPhase::Loading &&
        m_roomPhase != eRoomPhase::InGame)
    {
        SetLobbyMessageLocked(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::StopReplay,
            kInvalidGameRosterSlot,
            eChampion::END,
            "game is not running"));
        return false;
    }
    FinalizeReplayRecorder();
    ++m_lobbyRevision;

    SetLobbyMessageLocked(FormatLobbyCommandLog(
        "accept",
        sessionId,
        Shared::Schema::LobbyCommandKind::StopReplay,
        kInvalidGameRosterSlot,
        eChampion::END,
        "replay stopped"));

    BroadcastLobbyStateLocked();
    return true;
}

bool CGameRoom::TrySetReady(u32_t sessionId, bool_t bReady)
{
    auto it = m_sessionToSlot.find(sessionId);

    if (it == m_sessionToSlot.end())
        return false;

    LobbySlotState& slot = m_lobbySlots[it->second];

    if (!slot.bHuman || slot.sessionId != sessionId)
        return false;

    slot.bReady = bReady;
    ++m_lobbyRevision;

    if (m_roomPhase == eRoomPhase::Loading && AreAllActiveHumanSlotsReady())
    {
        BeginInGameLocked(sessionId);
        return true;
    }

    BroadcastLobbyStateLocked();
    return true;
}

bool CGameRoom::AreAllActiveHumanSlotsReady() const
{
    for (const LobbySlotState& slot : m_lobbySlots)
    {
        if (slot.bHuman && slot.sessionId != 0 && !slot.bReady)
            return false;
    }

    return true;
}

void CGameRoom::BeginInGameLocked(u32_t sessionId)
{
    m_roomPhase = eRoomPhase::InGame;
    ++m_lobbyRevision;

    if (m_bGameplayObjectsSpawned)
    {
        m_serverMinionWaves.ScheduleFirstWave(
            m_tickIndex,
            [](const char* pText)
            {
                OutputServerAITrace(pText);
            });
    }

    SetLobbyMessageLocked(FormatLobbyCommandLog(
        "accept",
        sessionId,
        Shared::Schema::LobbyCommandKind::SetReady,
        kInvalidGameRosterSlot,
        eChampion::END,
        "all clients loaded; game started"));

    BroadcastLobbyStateLocked();
    BroadcastGameStartLocked();

    char msg[128]{};
    sprintf_s(msg, "[GameRoom] BeginInGame all clients ready revision=%u\n", m_lobbyRevision);
    OutputServerAITrace(msg);
}

bool CGameRoom::CanEditBots(u32_t sessionId) const
{
    return m_bAllPlayersCanEditBots || sessionId == m_hostSessionId;
}

void CGameRoom::SetLobbyMessageLocked(const std::string& message)
{
    m_strLastLobbyMessage = message;
    if (!m_strLastLobbyMessage.empty())
    {
        char msg[512]{};
        sprintf_s(msg, "[Lobby] %s\n", m_strLastLobbyMessage.c_str());
        OutputServerAITrace(msg);
    }
}

void CGameRoom::SetLobbyMessageLocked(const char* message)
{
    SetLobbyMessageLocked(message ? std::string(message) : std::string());
}

void CGameRoom::BroadcastLobbyStateLocked()
{
    flatbuffers::FlatBufferBuilder fbb(1024);
    std::vector<flatbuffers::Offset<Shared::Schema::LobbySlot>> slots;
    slots.reserve(kGameRosterSlotCount);

    for (const LobbySlotState& slot : m_lobbySlots)
    {
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
    const auto debugMessage = fbb.CreateString(m_strLastLobbyMessage);
    Shared::Schema::LobbyPhase phase = Shared::Schema::LobbyPhase::None;
    switch (m_roomPhase)
    {
    case eRoomPhase::SeatSelect:
        phase = Shared::Schema::LobbyPhase::SeatSelect;
        break;
    case eRoomPhase::ChampionSelect:
        phase = Shared::Schema::LobbyPhase::ChampionSelect;
        break;
    case eRoomPhase::Loading:
        phase = Shared::Schema::LobbyPhase::Starting;
        break;
    case eRoomPhase::InGame:
        phase = Shared::Schema::LobbyPhase::InGame;
        break;
    }

    const auto state = Shared::Schema::CreateLobbyState(
        fbb,
        m_roomId,
        m_lobbyRevision,
        m_hostSessionId,
        phase,
        m_bAllPlayersCanEditBots,
        slotVec,
        0,
        debugMessage);
    fbb.Finish(state);
    auto buffer = fbb.Release();

    const auto packet = WrapEnvelope(
        ePacketType::LobbyState,
        m_lobbyRevision,
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
    const auto packet = WrapEnvelope(ePacketType::GameStart, m_lobbyRevision, nullptr, 0);
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

    pSession->Send(WrapEnvelope(ePacketType::GameStart, m_lobbyRevision, nullptr, 0));
}

void CGameRoom::SendHelloToSessionLocked(u32_t sessionId, NetEntityId netId, eChampion champion, u8_t team)
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

    auto packet = WrapEnvelope(
        ePacketType::Hello,
        m_lobbyRevision,
        helloBuffer.data(),
        static_cast<u32_t>(helloBuffer.size()));
    pSession->Send(std::move(packet));
}

bool CGameRoom::TryAttachSessionToDisconnectedHumanSlot(u32_t sessionId, EntityID& outEntity)
{
    outEntity = NULL_ENTITY;

    for (LobbySlotState& slot : m_lobbySlots)
    {
        if (!slot.bHuman ||
            slot.sessionId != 0 ||
            slot.netId == NULL_NET_ENTITY ||
            slot.champion == eChampion::END ||
            slot.champion == eChampion::NONE)
        {
            continue;
        }

        const EntityID entity = m_entityMap.FromNet(slot.netId);
        if (entity == NULL_ENTITY || !m_world.IsAlive(entity))
            continue;

        slot.sessionId = sessionId;
        slot.bReady = (m_roomPhase == eRoomPhase::InGame);
        m_sessionToSlot[sessionId] = slot.slotId;
        m_sessionToEntity[sessionId] = entity;
        outEntity = entity;
        return true;
    }

    return false;
}
