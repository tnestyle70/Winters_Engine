#pragma once
#include "GameModule/GameProduct.h"

#include <string>

struct GameLaunchConfig
{
	eGameProduct eProduct = eGameProduct::None;
	std::string strGameModeID{};
	wstring_t strContentRoot{};
	wstring_t strServiceNamespace{};
	wstring_t strServerEndpoint{};
	bool_t bUseOnlineServices = false;
	bool_t bUseEditorTools = true;
};
