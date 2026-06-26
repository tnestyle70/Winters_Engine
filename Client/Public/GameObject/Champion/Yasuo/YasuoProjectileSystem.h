#pragma once

#include "WintersTypes.h"
#include "WintersMath.h"
#include "ECS/Entity.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "GameObject/Projectile/ProjectileKind.h"

#include <memory>
#include <unordered_set>

class CWorld;

using eYasuoProjectileKind = eProjectileKind;

struct YasuoProjectileComponent
{
    Vec3 vWorldPos{};
    Vec3 vDirection{};
    f32_t fSpeed = 25.f;
    f32_t fMaxDist = 12.f;
    f32_t fTravelled = 0.f;
    f32_t fRadius = 1.f;
    f32_t fDamage = 60.f;
    f32_t fStunSec = 0.f;
    eYasuoProjectileKind kind = eYasuoProjectileKind::Wind;
    EntityID ownerEntity = NULL_ENTITY;
    eTeam ownerTeam = eTeam::Neutral;
    std::unordered_set<EntityID> hitSet;
};

class CYasuoProjectileSystem final
{
public:
    ~CYasuoProjectileSystem() = default;

    static std::unique_ptr<CYasuoProjectileSystem> Create();

    void Execute(CWorld& world, f32_t dt);

    static EntityID Spawn(CWorld& world,
        const Vec3& vOrigin, const Vec3& vDirection,
        f32_t fSpeed, f32_t fMaxDist, f32_t fRadius,
        f32_t fDamage, f32_t fStunSec,
        eYasuoProjectileKind kind,
        EntityID owner, eTeam ownerTeam);

private:
    CYasuoProjectileSystem() = default;
};
