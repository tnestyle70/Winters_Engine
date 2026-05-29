#pragma once

#include "WintersTypes.h"
#include "ECS/Entity.h"

class CWorld;
struct TickContext;

namespace FioraGameSim
{
	void RegisterHooks();
	void Tick(CWorld& world, const TickContext& tc);
	f32_t ConsumeBasicAttackDamage(CWorld& world, EntityID caster, f32_t baseDamage);
}