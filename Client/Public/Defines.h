#pragma once

#include "Engine_Defines.h"

NS_BEGIN(Client)

// ── Window ──
static const uint32_t g_iWinSizeX = { 1280 };
static const uint32_t g_iWinSizeY = { 720 };

// ── Client Version ──
#define CLIENT_VERSION      L"0.1.0"
#define CLIENT_WINDOW_TITLE L"Winters Game"

// ── Scene ID ──
enum class eSceneID : int
{
	GameSelect,
	Login,
	MainMenu,
	CustomMode,
	BanPick,
	Shop,
	MatchLoading,
	InGame,
	Editor,
	Result,
	SceneLoading,
	End
};

NS_END

using namespace Client;
