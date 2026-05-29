#pragma once

#include "Defines.h"

#include <string>
#include <vector>

enum class eFriendPresence : u32_t
{
	Offline,
	Online,
	InGame,
	Away,
};

enum class eLobbyQueueState : u32_t
{
	Idle,
	Searching,
	MatchFound,
};

struct ShellStoreItem
{
	std::string strItemID{};
	std::string strName{};
	std::string strDescription{};
	std::string strItemType{};
	i32_t iPriceRP = 0;
	bool_t bOwned = false;
};

struct ShellProfileSummary
{
	std::string strUserID{};
	std::string strDisplayName{};
	i32_t iLevel = 1;
	i32_t iWins = 0;
	i32_t iLosses = 0;
	i32_t iMMR = 0;
	i32_t iRP = 0;
};

struct ShellFriendEntry
{
	std::string strUserID{};
	std::string strDisplayName{};
	eFriendPresence ePresence = eFriendPresence::Offline;
	std::string strStatus{};
};

struct ShellLobbyState
{
	std::string strSelectedGameModeID{ "summoners_rift" };
	std::string strQueueName{ "Normal" };
	std::string strMatchID{};
	eLobbyQueueState eQueueState = eLobbyQueueState::Idle;
	bool_t bMatchReady = false;
};

inline const char* GetFriendPresenceName(eFriendPresence presence)
{
	switch (presence)
	{
	case eFriendPresence::Online:
		return "Online";
	case eFriendPresence::InGame:
		return "In Game";
	case eFriendPresence::Away:
		return "Away";
	default:
		return "Offline";
	}
}

inline const char* GetLobbyQueueStateName(eLobbyQueueState state)
{
	switch (state)
	{
	case eLobbyQueueState::Searching:
		return "Searching";
	case eLobbyQueueState::MatchFound:
		return "Match Found";
	default:
		return "Idle";
	}
}
