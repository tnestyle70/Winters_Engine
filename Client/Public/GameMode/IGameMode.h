#pragma once

#include "Defines.h"
#include "ECS/Entity.h"
#include "GameMode/GameModeDef.h"

class CWorld;

class IGameMode
{
public:
	virtual ~IGameMode() = default;

	virtual const GameModeDef& GetDef() const = 0;
	virtual bool_t OnLoadContent() = 0;
	virtual bool_t OnCreateWorld(CWorld& world) = 0;
	virtual void OnMatchStart() = 0;
	virtual void Tick(f32_t dt) = 0;
	virtual void OnEntityKilled(EntityID victim, EntityID killer) = 0;
	virtual bool_t IsGameOver() const = 0;
};
