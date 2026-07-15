#pragma once

#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "WintersTypes.h"

class CWorld;
struct TickContext;

class CShieldSystem final
{
public:
    static bool_t Grant(
        CWorld& world,
        const TickContext& tc,
        EntityID target,
        f32_t amount,
        f32_t durationSec);

    static f32_t Absorb(
        CWorld& world,
        const TickContext& tc,
        EntityID target,
        f32_t incomingDamage,
        bool_t& outShielded);

    static void Execute(CWorld& world, const TickContext& tc);
    static void Clear(CWorld& world, EntityID target);

    CShieldSystem() = delete;
};
