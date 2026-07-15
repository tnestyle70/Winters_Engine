#pragma once

#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "Shared/GameSim/Components/RuneComponent.h"
#include "WintersTypes.h"

class CWorld;
struct TickContext;
struct DamageRequest;
struct DamageResult;

class CRuneSystem final
{
public:
    static bool_t HasRune(CWorld& world, EntityID entity, eRuneId runeId);
    static void Execute(CWorld& world, const TickContext& tc);
    static void OnBasicAttackHitChampion(CWorld& world, const TickContext& tc,
        EntityID source, EntityID target);
    static void OnChampionDamageDealt(CWorld& world, const TickContext& tc,
        const DamageRequest& request, const DamageResult& result);

private:
    CRuneSystem() = delete;
};
