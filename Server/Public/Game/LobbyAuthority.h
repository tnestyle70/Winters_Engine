#pragma once

#include "Shared/GameSim/Definitions/LoLMatchContext.h"
#include "Shared/GameSim/Replication/EntityIdMap.h"
#include "WintersTypes.h"

#include <string>
#include <unordered_map>

namespace Shared::Schema
{
    struct LobbyCommand;
}

enum class eRoomPhase : u8_t
{
    SeatSelect,
    ChampionSelect,
    Loading,
    InGame,
};

struct LobbySlotState
{
    u8_t slotId = kInvalidGameRosterSlot;
    u8_t team = 0;
    bool_t bHuman = false;
    bool_t bBot = false;
    u32_t sessionId = 0;
    NetEntityId netId = NULL_NET_ENTITY;
    eChampion champion = eChampion::END;
    u8_t botDifficulty = 2;
    u8_t botLane = kGameRosterDefaultBotLane;
    bool_t bReady = false;
    bool_t bLocked = false;
    bool_t bDummy = false;
};

struct LobbyAuthorityResult
{
    bool_t bBroadcastLobbyState = false;
    bool_t bSendHello = false;
    bool_t bSendGameStart = false;
    bool_t bBeginLoading = false;
    bool_t bBeginInGame = false;
    bool_t bStopReplay = false;

    u32_t sessionId = 0;
    NetEntityId helloNetId = NULL_NET_ENTITY;
    eChampion helloChampion = eChampion::END;
    u8_t helloTeam = 0;
};

class CLobbyAuthority final
{
public:
    explicit CLobbyAuthority(u32_t roomId);

    void InitializeSlots();

    LobbyAuthorityResult OnLobbyCommand(u32_t sessionId, const Shared::Schema::LobbyCommand* command);
    LobbyAuthorityResult OnSessionJoin(u32_t sessionId);
    LobbyAuthorityResult OnSessionLeave(u32_t sessionId);
    LobbyAuthorityResult AttachDisconnectedHumanSlot(u32_t sessionId, u8_t slotId);

    eRoomPhase GetPhase() const { return m_phase; }
    u32_t GetRevision() const { return m_revision; }
    u32_t GetHostSessionId() const { return m_hostSessionId; }
    bool_t CanAllPlayersEditBots() const { return m_bAllPlayersCanEditBots; }
    const std::string& GetLastMessage() const { return m_lastMessage; }

    LobbySlotState* GetSlots() { return m_slots; }
    const LobbySlotState* GetSlots() const { return m_slots; }
    u32_t GetSlotCount() const { return kGameRosterSlotCount; }

    bool TryGetSessionSlot(u32_t sessionId, u8_t& outSlotId) const;
    const LobbySlotState* TryGetSlot(u8_t slotId) const;
    LobbySlotState* TryGetSlot(u8_t slotId);
    bool TryFindDisconnectedHumanSlot(
        u8_t& outSlotId,
        NetEntityId& outNetId,
        eChampion& outChampion,
        u8_t& outTeam) const;
    void SetSlotNetId(u8_t slotId, NetEntityId netId);

private:
    u8_t FindFirstEmptySlot(u32_t beginSlot, u32_t endSlot) const;
    void CompactTeamSlots(u32_t beginSlot, u32_t endSlot);
    void OnLobbyJoin(u32_t sessionId);

    bool TryJoinSlot(u32_t sessionId, u8_t slotId);
    bool TryLeaveSlot(u32_t sessionId);
    bool TryPickChampion(u32_t sessionId, eChampion champion);
    bool TrySetBotChampion(u32_t sessionId, u8_t slotId, eChampion champion);
    bool TrySetBotDifficulty(u32_t sessionId, u8_t slotId, u8_t difficulty);
    bool TrySetBotLane(u32_t sessionId, u8_t lane, u8_t slotId);
    bool TryAdvanceToChampionSelect(u32_t sessionId);
    bool TryStartGame(u32_t sessionId);
    bool TrySetReady(u32_t sessionId, bool_t bReady, LobbyAuthorityResult& result);
    bool TryStopReplay(u32_t sessionId, LobbyAuthorityResult& result);

    bool AreAllActiveHumanSlotsReady() const;
    bool CanEditBots(u32_t sessionId) const;
    void SetMessage(const std::string& message);
    void SetMessage(const char* message);
    void IncrementRevision(LobbyAuthorityResult& result);

    u32_t m_roomId = 0;
    eRoomPhase m_phase = eRoomPhase::SeatSelect;
    u32_t m_hostSessionId = 0;
    u32_t m_revision = 0;
    std::string m_lastMessage;
    bool_t m_bAllPlayersCanEditBots = true;
    LobbySlotState m_slots[kGameRosterSlotCount]{};
    std::unordered_map<u32_t, u8_t> m_sessionToSlot;
};
