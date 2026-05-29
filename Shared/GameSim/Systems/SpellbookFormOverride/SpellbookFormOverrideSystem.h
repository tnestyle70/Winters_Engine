#pragma once

#include "ECS/Entity.h"
#include "GameContext.h"
#include "WintersTypes.h"

class CWorld;
struct TickContext;

struct SkillOverrideResolveResult
{
	eChampion baseChampion = eChampion::NONE;
	eChampion hookChampion = eChampion::NONE;
	eChampion cooldownChampion = eChampion::NONE;
	u8_t localSlot = 0;
	u8_t hookSlot = 0;
	u8_t cooldownSlot = 0;
	bool_t bOverridden = false;
	bool_t bConsumeSpellbookOnAccept = false;
};

class CSpellbookFormOverrideSystem final
{
public:
	static void Execute(CWorld& world, const TickContext& tc);

	static SkillOverrideResolveResult ResolveSkill(
		CWorld& world,
		EntityID caster,
		eChampion baseChampion,
		u8_t localSlot);

	static void ConsumeSpellbookOverride(
		CWorld& world,
		EntityID caster,
		u8_t localSlot);
};
