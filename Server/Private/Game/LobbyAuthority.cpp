#include "Game/LobbyAuthority.h"

#include "GameRoomSmokeRoster.h"
#include "Server/Private/Data/RuntimeGameplayDefinitionOverlay.h"
#include "Shared/GameSim/Definitions/MapSpawnPoints.h"
#include "Shared/Schemas/Generated/cpp/LobbyCommand_generated.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
    eChampion ResolveCommandChampion(const Shared::Schema::LobbyCommand& command)
    {
        const ChampionGameplayDef* definition =
            ServerData::GetActiveLoLGameplayDefinitionPack().FindChampion(
                command.championDefinitionKey());
        return definition ? definition->legacyChampion : eChampion::END;
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

CLobbyAuthority::CLobbyAuthority(u32_t roomId)
    : m_roomId(roomId)
{
}

void CLobbyAuthority::InitializeSlots()
{
    for (u32_t i = 0; i < kGameRosterSlotCount; ++i)
    {
        LobbySlotState& slot = m_slots[i];
        slot = LobbySlotState{};
        slot.slotId = static_cast<u8_t>(i);
        slot.team = static_cast<u8_t>(i < 5 ? 0 : 1);
        slot.botDifficulty = 2;
        slot.botLane = GetDefaultLobbyBotLane(slot.slotId);
    }
}

LobbyAuthorityResult CLobbyAuthority::OnLobbyCommand(
    u32_t sessionId,
    const Shared::Schema::LobbyCommand* command)
{
    LobbyAuthorityResult result{};
    result.sessionId = sessionId;

    if (!command)
        return result;

    const bool_t bLobbyEditablePhase =
        m_phase == eRoomPhase::SeatSelect ||
        m_phase == eRoomPhase::ChampionSelect;

    if (!bLobbyEditablePhase)
    {
        if (m_phase == eRoomPhase::Loading &&
            command->kind() == Shared::Schema::LobbyCommandKind::SetReady)
        {
            if (TrySetReady(sessionId, command->value() != 0, result))
                return result;
            IncrementRevision(result);
            return result;
        }

        if (command->kind() == Shared::Schema::LobbyCommandKind::StopReplay)
        {
            if (TryStopReplay(sessionId, result))
                return result;
            IncrementRevision(result);
            return result;
        }

        SetMessage(FormatLobbyCommandLog(
            "reject",
            sessionId,
            command->kind(),
            command->slotId(),
            ResolveCommandChampion(*command),
            "room is not in lobby phase"));
        IncrementRevision(result);
        return result;
    }

    bool bChanged = false;
    m_lastMessage.clear();

    switch (command->kind())
    {
    case Shared::Schema::LobbyCommandKind::JoinSlot:
        if (m_phase != eRoomPhase::SeatSelect)
        {
            SetMessage(FormatLobbyCommandLog(
                "reject",
                sessionId,
                command->kind(),
                command->slotId(),
                ResolveCommandChampion(*command),
                "slot changes are only allowed in seat select"));
            break;
        }
        bChanged = TryJoinSlot(sessionId, command->slotId());
        break;
    case Shared::Schema::LobbyCommandKind::LeaveSlot:
        if (m_phase != eRoomPhase::SeatSelect)
        {
            SetMessage(FormatLobbyCommandLog(
                "reject",
                sessionId,
                command->kind(),
                command->slotId(),
                ResolveCommandChampion(*command),
                "slot changes are only allowed in seat select"));
            break;
        }
        bChanged = TryLeaveSlot(sessionId);
        break;
    case Shared::Schema::LobbyCommandKind::PickChampion:
        if (m_phase != eRoomPhase::ChampionSelect)
        {
            SetMessage(FormatLobbyCommandLog(
                "reject",
                sessionId,
                command->kind(),
                command->slotId(),
                ResolveCommandChampion(*command),
                "player champion pick is only allowed in champion select"));
            break;
        }
        bChanged = TryPickChampion(sessionId, ResolveCommandChampion(*command));
        break;
    case Shared::Schema::LobbyCommandKind::SetBotChampion:
        bChanged = TrySetBotChampion(
            sessionId,
            command->slotId(),
            ResolveCommandChampion(*command));
        break;
    case Shared::Schema::LobbyCommandKind::SetReady:
        if (TrySetReady(sessionId, command->value() != 0, result))
            return result;
        break;
    case Shared::Schema::LobbyCommandKind::SetBotDifficulty:
        bChanged = TrySetBotDifficulty(sessionId, command->slotId(), command->botDifficulty());
        break;
    case Shared::Schema::LobbyCommandKind::SetBotLane:
        bChanged = TrySetBotLane(sessionId, static_cast<u8_t>(command->value()), command->slotId());
        break;
    case Shared::Schema::LobbyCommandKind::StartGame:
        if (m_phase == eRoomPhase::SeatSelect)
        {
            if (TryAdvanceToChampionSelect(sessionId))
            {
                IncrementRevision(result);
                return result;
            }
        }
        else if (m_phase == eRoomPhase::ChampionSelect)
        {
            if (TryStartGame(sessionId))
            {
                result.bBeginLoading = true;
                IncrementRevision(result);
                return result;
            }
        }
        else
        {
            SetMessage(FormatLobbyCommandLog(
                "reject",
                sessionId,
                command->kind(),
                command->slotId(),
                ResolveCommandChampion(*command),
                "room is not ready to advance"));
        }
        break;
    default:
        SetMessage(FormatLobbyCommandLog(
            "reject",
            sessionId,
            command->kind(),
            command->slotId(),
            ResolveCommandChampion(*command),
            "unsupported lobby command"));
        break;
    }

    if (bChanged)
    {
        if (m_lastMessage.empty())
        {
            SetMessage(FormatLobbyCommandLog(
                "accept",
                sessionId,
                command->kind(),
                command->slotId(),
                ResolveCommandChampion(*command),
                "state changed"));
        }
    }
    else
    {
        if (m_lastMessage.empty())
        {
            SetMessage(FormatLobbyCommandLog(
                "reject",
                sessionId,
                command->kind(),
                command->slotId(),
                ResolveCommandChampion(*command),
                "no state change"));
        }
    }

    IncrementRevision(result);
    return result;
}

LobbyAuthorityResult CLobbyAuthority::OnSessionJoin(u32_t sessionId)
{
    LobbyAuthorityResult result{};
    result.sessionId = sessionId;

    if (m_phase == eRoomPhase::SeatSelect ||
        m_phase == eRoomPhase::ChampionSelect)
    {
        OnLobbyJoin(sessionId);
        result.bSendHello = true;
        IncrementRevision(result);
        return result;
    }

    u8_t slotId = kInvalidGameRosterSlot;
    if (TryGetSessionSlot(sessionId, slotId))
    {
        const LobbySlotState* pSlot = TryGetSlot(slotId);
        if (pSlot)
        {
            result.bSendHello = true;
            result.helloNetId = pSlot->netId;
            result.helloChampion = pSlot->champion;
            result.helloTeam = pSlot->team;
            return result;
        }
    }

    result.bSendHello = true;
    result.bBroadcastLobbyState = true;
    return result;
}

LobbyAuthorityResult CLobbyAuthority::OnSessionLeave(u32_t sessionId)
{
    LobbyAuthorityResult result{};
    result.sessionId = sessionId;

    auto slotIt = m_sessionToSlot.find(sessionId);
    if (slotIt == m_sessionToSlot.end())
        return result;

    LobbySlotState& slot = m_slots[slotIt->second];
    const u8_t slotId = slot.slotId;
    const u32_t beginSlot = slot.team == 0 ? 0u : 5u;
    const u32_t endSlot = slot.team == 0 ? 5u : 10u;
    const bool_t bEditablePhase =
        m_phase == eRoomPhase::SeatSelect ||
        m_phase == eRoomPhase::ChampionSelect;

    if (slot.bHuman && slot.sessionId == sessionId)
    {
        if (bEditablePhase)
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
    if (bEditablePhase)
        CompactTeamSlots(beginSlot, endSlot);

    SetMessage(FormatLobbyCommandLog(
        "accept",
        sessionId,
        Shared::Schema::LobbyCommandKind::LeaveSlot,
        slotId,
        eChampion::END,
        bEditablePhase
            ? "disconnected; slot freed"
            : "disconnected; slot reserved for reconnect"));
    IncrementRevision(result);
    return result;
}

LobbyAuthorityResult CLobbyAuthority::AttachDisconnectedHumanSlot(u32_t sessionId, u8_t slotId)
{
    LobbyAuthorityResult result{};
    result.sessionId = sessionId;

    LobbySlotState* pSlot = TryGetSlot(slotId);
    if (!pSlot ||
        !pSlot->bHuman ||
        pSlot->sessionId != 0 ||
        pSlot->netId == NULL_NET_ENTITY ||
        pSlot->champion == eChampion::END ||
        pSlot->champion == eChampion::NONE)
    {
        return result;
    }

    pSlot->sessionId = sessionId;
    pSlot->bReady = (m_phase == eRoomPhase::InGame);
    m_sessionToSlot[sessionId] = pSlot->slotId;

    result.bSendHello = true;
    result.helloNetId = pSlot->netId;
    result.helloChampion = pSlot->champion;
    result.helloTeam = pSlot->team;
    result.bSendGameStart = (m_phase == eRoomPhase::InGame);
    IncrementRevision(result);
    return result;
}

bool CLobbyAuthority::TryGetSessionSlot(u32_t sessionId, u8_t& outSlotId) const
{
    auto it = m_sessionToSlot.find(sessionId);
    if (it == m_sessionToSlot.end())
        return false;

    outSlotId = it->second;
    return outSlotId < kGameRosterSlotCount;
}

const LobbySlotState* CLobbyAuthority::TryGetSlot(u8_t slotId) const
{
    if (slotId >= kGameRosterSlotCount)
        return nullptr;
    return &m_slots[slotId];
}

LobbySlotState* CLobbyAuthority::TryGetSlot(u8_t slotId)
{
    if (slotId >= kGameRosterSlotCount)
        return nullptr;
    return &m_slots[slotId];
}

bool CLobbyAuthority::TryFindDisconnectedHumanSlot(
    u8_t& outSlotId,
    NetEntityId& outNetId,
    eChampion& outChampion,
    u8_t& outTeam) const
{
    outSlotId = kInvalidGameRosterSlot;
    outNetId = NULL_NET_ENTITY;
    outChampion = eChampion::END;
    outTeam = 0;

    for (const LobbySlotState& slot : m_slots)
    {
        if (!slot.bHuman ||
            slot.sessionId != 0 ||
            slot.netId == NULL_NET_ENTITY ||
            slot.champion == eChampion::END ||
            slot.champion == eChampion::NONE)
        {
            continue;
        }

        outSlotId = slot.slotId;
        outNetId = slot.netId;
        outChampion = slot.champion;
        outTeam = slot.team;
        return true;
    }

    return false;
}

void CLobbyAuthority::SetSlotNetId(u8_t slotId, NetEntityId netId)
{
    if (LobbySlotState* pSlot = TryGetSlot(slotId))
        pSlot->netId = netId;
}

LobbyAuthorityResult CLobbyAuthority::TransferInGameHumanControl(
    u32_t sessionId,
    NetEntityId targetNetId)
{
    LobbyAuthorityResult result{};
    result.sessionId = sessionId;
    if (m_phase != eRoomPhase::InGame || targetNetId == NULL_NET_ENTITY)
        return result;

    const auto sourceIt = m_sessionToSlot.find(sessionId);
    if (sourceIt == m_sessionToSlot.end())
        return result;

    LobbySlotState& source = m_slots[sourceIt->second];
    LobbySlotState* pTarget = nullptr;
    for (LobbySlotState& slot : m_slots)
    {
        if (slot.netId == targetNetId)
        {
            pTarget = &slot;
            break;
        }
    }

    if (!pTarget ||
        pTarget == &source ||
        !source.bHuman ||
        source.bBot ||
        source.sessionId != sessionId ||
        !pTarget->bBot ||
        pTarget->bHuman ||
        pTarget->bDummy)
    {
        return result;
    }

    source.bHuman = false;
    source.bBot = true;
    source.sessionId = 0u;
    source.bReady = true;
    if (source.botDifficulty == 0u)
        source.botDifficulty = 2u;
    if (!IsValidLobbyBotLane(source.botLane))
        source.botLane = GetDefaultLobbyBotLane(source.slotId);

    pTarget->bHuman = true;
    pTarget->bBot = false;
    pTarget->sessionId = sessionId;
    pTarget->bReady = true;
    m_sessionToSlot[sessionId] = pTarget->slotId;

    result.bSendHello = true;
    result.helloNetId = pTarget->netId;
    result.helloChampion = pTarget->champion;
    result.helloTeam = pTarget->team;
    SetMessage("practice control transferred to roster champion");
    IncrementRevision(result);
    return result;
}

LobbyAuthorityResult CLobbyAuthority::ReplaceInGameControlledChampion(
    u32_t sessionId,
    eChampion champion,
    NetEntityId newNetId)
{
    LobbyAuthorityResult result{};
    result.sessionId = sessionId;
    if (m_phase != eRoomPhase::InGame ||
        newNetId == NULL_NET_ENTITY ||
        champion == eChampion::NONE ||
        champion == eChampion::END)
    {
        return result;
    }

    const auto sourceIt = m_sessionToSlot.find(sessionId);
    if (sourceIt == m_sessionToSlot.end())
        return result;

    LobbySlotState& slot = m_slots[sourceIt->second];
    if (!slot.bHuman || slot.bBot || slot.sessionId != sessionId)
        return result;

    slot.champion = champion;
    slot.netId = newNetId;
    slot.bReady = true;

    result.bSendHello = true;
    result.helloNetId = slot.netId;
    result.helloChampion = slot.champion;
    result.helloTeam = slot.team;
    SetMessage("practice controlled champion replaced");
    IncrementRevision(result);
    return result;
}

u8_t CLobbyAuthority::FindFirstEmptySlot(u32_t beginSlot, u32_t endSlot) const
{
    for (u32_t i = beginSlot; i < endSlot && i < kGameRosterSlotCount; ++i)
    {
        const LobbySlotState& slot = m_slots[i];
        if (!slot.bHuman && !slot.bBot)
            return static_cast<u8_t>(i);
    }

    return kInvalidGameRosterSlot;
}

void CLobbyAuthority::CompactTeamSlots(u32_t beginSlot, u32_t endSlot)
{
    std::vector<LobbySlotState> occupied;
    for (u32_t i = beginSlot; i < endSlot && i < kGameRosterSlotCount; ++i)
    {
        if (m_slots[i].bHuman || m_slots[i].bBot)
            occupied.push_back(m_slots[i]);
    }

    for (u32_t i = beginSlot; i < endSlot && i < kGameRosterSlotCount; ++i)
    {
        LobbySlotState reset{};
        reset.slotId = static_cast<u8_t>(i);
        reset.team = static_cast<u8_t>(i < 5 ? 0 : 1);
        reset.botDifficulty = 2;
        reset.botLane = GetDefaultLobbyBotLane(reset.slotId);
        m_slots[i] = reset;
    }

    u32_t occupiedIndex = 0;
    for (u32_t i = beginSlot;
        i < endSlot && i < kGameRosterSlotCount && occupiedIndex < occupied.size();
        ++i)
    {
        m_slots[i] = occupied[occupiedIndex++];
        m_slots[i].slotId = static_cast<u8_t>(i);
        m_slots[i].team = static_cast<u8_t>(i < 5 ? 0 : 1);
        if (m_slots[i].bHuman)
            m_sessionToSlot[m_slots[i].sessionId] = static_cast<u8_t>(i);
    }
}

void CLobbyAuthority::OnLobbyJoin(u32_t sessionId)
{
    if (m_hostSessionId == 0)
    {
        m_hostSessionId = sessionId;
        TryJoinSlot(sessionId, 0);
        return;
    }

    const u8_t blueSlot = FindFirstEmptySlot(0, 5);
    if (blueSlot < kGameRosterSlotCount)
    {
        TryJoinSlot(sessionId, blueSlot);
        return;
    }

    const u8_t redSlot = FindFirstEmptySlot(5, 10);
    if (redSlot < kGameRosterSlotCount)
    {
        TryJoinSlot(sessionId, redSlot);
        return;
    }

    SetMessage(FormatLobbyCommandLog(
        "accept",
        sessionId,
        Shared::Schema::LobbyCommandKind::None,
        kInvalidGameRosterSlot,
        eChampion::END,
        "connected; choose a slot"));
}

bool CLobbyAuthority::TryJoinSlot(u32_t sessionId, u8_t slotId)
{
    if (slotId >= kGameRosterSlotCount)
    {
        SetMessage(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::JoinSlot,
            slotId,
            eChampion::END,
            "invalid slot"));
        return false;
    }

    LobbySlotState& target = m_slots[slotId];
    if (target.bHuman && target.sessionId != sessionId)
    {
        SetMessage(FormatLobbyCommandLog(
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
        SetMessage(FormatLobbyCommandLog(
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
        SetMessage(FormatLobbyCommandLog(
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
        LobbySlotState& oldSlot = m_slots[prevIt->second];
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
            CompactTeamSlots(previousBeginSlot, previousEndSlot);
    }

    LobbySlotState& joinTarget = m_slots[slotId];
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
    SetMessage(FormatLobbyCommandLog(
        "accept",
        sessionId,
        Shared::Schema::LobbyCommandKind::JoinSlot,
        slotId,
        joinTarget.champion,
        "joined slot"));
    return true;
}

bool CLobbyAuthority::TryLeaveSlot(u32_t sessionId)
{
    auto it = m_sessionToSlot.find(sessionId);
    if (it == m_sessionToSlot.end())
    {
        SetMessage(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::LeaveSlot,
            kInvalidGameRosterSlot,
            eChampion::END,
            "session has no slot"));
        return false;
    }

    LobbySlotState& slot = m_slots[it->second];
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
    CompactTeamSlots(beginSlot, endSlot);
    SetMessage(FormatLobbyCommandLog(
        "accept",
        sessionId,
        Shared::Schema::LobbyCommandKind::LeaveSlot,
        slotId,
        eChampion::END,
        "left slot"));
    return true;
}

bool CLobbyAuthority::TryPickChampion(u32_t sessionId, eChampion champion)
{
    if (champion == eChampion::END || champion == eChampion::NONE)
    {
        SetMessage(FormatLobbyCommandLog(
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
        SetMessage(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::PickChampion,
            kInvalidGameRosterSlot,
            champion,
            "session has no slot"));
        return false;
    }

    m_slots[it->second].champion = champion;
    SetMessage(FormatLobbyCommandLog(
        "accept",
        sessionId,
        Shared::Schema::LobbyCommandKind::PickChampion,
        m_slots[it->second].slotId,
        champion,
        "picked champion"));
    return true;
}

bool CLobbyAuthority::TrySetBotChampion(u32_t sessionId, u8_t slotId, eChampion champion)
{
    if (!CanEditBots(sessionId) || slotId >= kGameRosterSlotCount)
    {
        SetMessage(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::SetBotChampion,
            slotId,
            champion,
            !CanEditBots(sessionId) ? "no bot edit permission" : "invalid slot"));
        return false;
    }

    LobbySlotState& slot = m_slots[slotId];
    if (slot.bHuman)
    {
        SetMessage(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::SetBotChampion,
            slotId,
            champion,
            "slot is occupied by a player"));
        return false;
    }

    if (m_phase == eRoomPhase::ChampionSelect)
    {
        if (!slot.bBot)
        {
            SetMessage(FormatLobbyCommandLog(
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
            SetMessage(FormatLobbyCommandLog(
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
        CompactTeamSlots(beginSlot, endSlot);
        SetMessage(FormatLobbyCommandLog(
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
    SetMessage(FormatLobbyCommandLog(
        "accept",
        sessionId,
        Shared::Schema::LobbyCommandKind::SetBotChampion,
        slotId,
        champion,
        "set bot champion"));
    return true;
}

bool CLobbyAuthority::TrySetBotLane(u32_t sessionId, u8_t lane, u8_t slotId)
{
    if (!CanEditBots(sessionId) || slotId >= kGameRosterSlotCount || !IsValidLobbyBotLane(lane))
    {
        SetMessage(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::SetBotLane,
            slotId,
            eChampion::END,
            !CanEditBots(sessionId) ? "no bot edit permission" : "invalid bot lane"));
        return false;
    }

    LobbySlotState& slot = m_slots[slotId];
    if (!slot.bBot)
    {
        SetMessage(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::SetBotLane,
            slotId,
            slot.champion,
            "slot is not a bot"));
        return false;
    }

    slot.botLane = lane;
    SetMessage(FormatLobbyCommandLog(
        "accept",
        sessionId,
        Shared::Schema::LobbyCommandKind::SetBotLane,
        slotId,
        slot.champion,
        "set bot lane"));
    return true;
}

bool CLobbyAuthority::TrySetBotDifficulty(u32_t sessionId, u8_t slotId, u8_t difficulty)
{
    if (!CanEditBots(sessionId) || slotId >= kGameRosterSlotCount)
    {
        SetMessage(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::SetBotDifficulty,
            slotId,
            eChampion::END,
            !CanEditBots(sessionId) ? "no bot edit permission" : "invalid slot"));
        return false;
    }

    LobbySlotState& slot = m_slots[slotId];
    if (!slot.bBot)
    {
        SetMessage(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::SetBotDifficulty,
            slotId,
            slot.champion,
            "slot is not a bot"));
        return false;
    }

    slot.botDifficulty = difficulty ? difficulty : 2;
    SetMessage(FormatLobbyCommandLog(
        "accept",
        sessionId,
        Shared::Schema::LobbyCommandKind::SetBotDifficulty,
        slotId,
        slot.champion,
        "set bot difficulty"));
    return true;
}

bool CLobbyAuthority::TryAdvanceToChampionSelect(u32_t sessionId)
{
    if (sessionId != m_hostSessionId && !m_bAllPlayersCanEditBots)
    {
        SetMessage(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::StartGame,
            kInvalidGameRosterSlot,
            eChampion::END,
            "only host can advance to champion select"));
        return false;
    }

    bool_t bHasHuman = false;
    for (const LobbySlotState& slot : m_slots)
    {
        if (slot.bHuman)
        {
            bHasHuman = true;
            break;
        }
    }

    if (!bHasHuman)
    {
        SetMessage(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::StartGame,
            kInvalidGameRosterSlot,
            eChampion::END,
            "no human player"));
        return false;
    }

    m_phase = eRoomPhase::ChampionSelect;

    SetMessage(FormatLobbyCommandLog(
        "accept",
        sessionId,
        Shared::Schema::LobbyCommandKind::StartGame,
        kInvalidGameRosterSlot,
        eChampion::END,
        "champion select started"));
    return true;
}

bool CLobbyAuthority::TryStartGame(u32_t sessionId)
{
    if (sessionId != m_hostSessionId && !m_bAllPlayersCanEditBots)
    {
        SetMessage(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::StartGame,
            kInvalidGameRosterSlot,
            eChampion::END,
            "only host can start"));
        return false;
    }

    u32_t humanCount = 0u;
    for (const LobbySlotState& slot : m_slots)
    {
        if (slot.bHuman)
            ++humanCount;
    }
    if (humanCount == 0u)
    {
        SetMessage(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::StartGame,
            kInvalidGameRosterSlot,
            eChampion::END,
            "no human player"));
        return false;
    }

    if (ShouldUseAttackSpeedLabRoster())
    {
        if (humanCount != 1u)
        {
            SetMessage(FormatLobbyCommandLog(
                "reject",
                sessionId,
                Shared::Schema::LobbyCommandKind::StartGame,
                kInvalidGameRosterSlot,
                eChampion::END,
                "attack speed lab requires exactly one human"));
            return false;
        }
        if (!EnsureAttackSpeedLabRoster(m_slots, kGameRosterSlotCount))
        {
            SetMessage(FormatLobbyCommandLog(
                "reject",
                sessionId,
                Shared::Schema::LobbyCommandKind::StartGame,
                kInvalidGameRosterSlot,
                eChampion::END,
                "attack speed lab roster bootstrap failed"));
            return false;
        }
    }
    else if (ShouldUseRedSylasSmokeRoster())
        EnsureRedSylasSmokeRoster(m_slots, kGameRosterSlotCount);

    for (u32_t i = 0; i < kGameRosterSlotCount; ++i)
    {
        LobbySlotState& slot = m_slots[i];
        if (!slot.bHuman && !slot.bBot)
        {
            slot.bLocked = false;
            slot.bReady = false;
            slot.champion = eChampion::END;
            continue;
        }

        if (slot.champion == eChampion::END || slot.champion == eChampion::NONE)
        {
            SetMessage(FormatLobbyCommandLog(
                "reject",
                sessionId,
                Shared::Schema::LobbyCommandKind::StartGame,
                slot.slotId,
                slot.champion,
                slot.bHuman ? "human slot has no champion" : "bot slot has no champion"));
            return false;
        }

        slot.bLocked = true;
    }

    m_phase = eRoomPhase::Loading;

    for (LobbySlotState& slot : m_slots)
    {
        if (slot.bHuman)
            slot.bReady = false;
    }

    SetMessage(FormatLobbyCommandLog(
        "accept",
        sessionId,
        Shared::Schema::LobbyCommandKind::StartGame,
        kInvalidGameRosterSlot,
        eChampion::END,
        "loading started"));
    return true;
}

bool CLobbyAuthority::TrySetReady(
    u32_t sessionId,
    bool_t bReady,
    LobbyAuthorityResult& result)
{
    auto it = m_sessionToSlot.find(sessionId);

    if (it == m_sessionToSlot.end())
        return false;

    LobbySlotState& slot = m_slots[it->second];

    if (!slot.bHuman || slot.sessionId != sessionId)
        return false;

    slot.bReady = bReady;

    if (m_phase == eRoomPhase::Loading && AreAllActiveHumanSlotsReady())
    {
        m_phase = eRoomPhase::InGame;
        SetMessage(FormatLobbyCommandLog(
            "accept",
            sessionId,
            Shared::Schema::LobbyCommandKind::SetReady,
            kInvalidGameRosterSlot,
            eChampion::END,
            "all clients loaded; game started"));
        result.bBeginInGame = true;
    }

    IncrementRevision(result);
    return true;
}

bool CLobbyAuthority::TryStopReplay(u32_t sessionId, LobbyAuthorityResult& result)
{
    if (sessionId != m_hostSessionId && !m_bAllPlayersCanEditBots)
    {
        SetMessage(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::StopReplay,
            kInvalidGameRosterSlot,
            eChampion::END,
            "only host can stop replay"));
        return false;
    }

    if (m_phase != eRoomPhase::Loading &&
        m_phase != eRoomPhase::InGame)
    {
        SetMessage(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::StopReplay,
            kInvalidGameRosterSlot,
            eChampion::END,
            "game is not running"));
        return false;
    }

    result.bStopReplay = true;
    SetMessage(FormatLobbyCommandLog(
        "accept",
        sessionId,
        Shared::Schema::LobbyCommandKind::StopReplay,
        kInvalidGameRosterSlot,
        eChampion::END,
        "replay stopped"));
    IncrementRevision(result);
    return true;
}

bool CLobbyAuthority::AreAllActiveHumanSlotsReady() const
{
    for (const LobbySlotState& slot : m_slots)
    {
        if (slot.bHuman && slot.sessionId != 0 && !slot.bReady)
            return false;
    }

    return true;
}

bool CLobbyAuthority::CanEditBots(u32_t sessionId) const
{
    return m_bAllPlayersCanEditBots || sessionId == m_hostSessionId;
}

void CLobbyAuthority::SetMessage(const std::string& message)
{
    m_lastMessage = message;
}

void CLobbyAuthority::SetMessage(const char* message)
{
    SetMessage(message ? std::string(message) : std::string());
}

void CLobbyAuthority::IncrementRevision(LobbyAuthorityResult& result)
{
    ++m_revision;
    result.bBroadcastLobbyState = true;
}
