#pragma once

#include "WintersMath.h"
#include "WintersTypes.h"
#include "ECS/Entity.h"
#include "ECS/Components/GameplayComponents.h"
#include "Shared/GameSim/Components/ProjectileKindComponent.h"

struct SkillProjectileComponent
{
    EntityID sourceEntity = NULL_ENTITY;
    EntityID targetEntity = NULL_ENTITY;
    eTeam sourceTeam = eTeam::Neutral;
    eProjectileKind kind = eProjectileKind::Generic;
    u16_t skillId = 0;
    u8_t rank = 1;
    bool_t bSpawned = false;

    Vec3 currentPos{};
    Vec3 direction{ 0.f, 0.f, 1.f };
    f32_t speed = 0.f;
    f32_t maxDistance = 0.f;
    f32_t traveledDistance = 0.f;
    f32_t hitRadius = 0.5f;
    f32_t damage = 0.f;

    bool_t bApplyOnHitStatus = false;
    StatusEffectApplyDesc onHitStatus{};
};
