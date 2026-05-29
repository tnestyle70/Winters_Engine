#pragma once
#include "Defines.h"
#include "GameModule/GameLaunchConfig.h"
#include "IScene.h"
#include <memory>

class IGameModule
{
public:
	virtual ~IGameModule() = default;

	virtual eGameProduct GetProductID() const = 0;
	virtual const char* GetDisplayName() const = 0;
	virtual bool_t IsAvailable() const = 0;

	virtual bool_t InitializeClient(const GameLaunchConfig& config) = 0;
	virtual void ShutdownClient() = 0;

	virtual eSceneID GetInitialSceneID() const = 0;
	virtual std::unique_ptr<IScene> CreateInitialScene() = 0;
};
