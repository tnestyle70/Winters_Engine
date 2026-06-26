#pragma once

#include "WintersTypes.h"
#include "WintersMath.h"
#include "ECS/Entity.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Components/ProjectileKindComponent.h"

struct PendingHitComponent
{
    EntityID ownerEntity = NULL_ENTITY;
    Vec3 vDirectionAtCast{};
    f32_t fDelay = 0.f;
    eProjectileKind kind = eProjectileKind::Wind;
    f32_t fSpeed = 25.f;
    f32_t fMaxDist = 12.f;
    f32_t fRadius = 1.f;
    f32_t fDamage = 60.f;
    f32_t fStunSec = 0.f;
    eTeam ownerTeam = eTeam::Neutral;
};
