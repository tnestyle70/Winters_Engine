#pragma once

#include "WintersMath.h"
#include "WintersTypes.h"
#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "Shared/GameSim/Components/GameplayComponents.h"

struct ProjectileBarrierComponent
{
    EntityID sourceEntity = NULL_ENTITY;
    eTeam sourceTeam = eTeam::Neutral;
    Vec3 origin{};
    Vec3 previousCenter{};
    Vec3 center{};
    Vec3 direction{ 0.f, 0.f, 1.f };
    f32_t halfLength = 1.6f;
    f32_t halfThickness = 0.5f;
    u64_t spawnTick = 0u;
    u64_t formationEndTick = 0u;
    u64_t expireTick = 0u;
};
