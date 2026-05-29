#pragma once

#include "Shared/GameSim/Components/PendingHitComponent.h"

#include <memory>

class CWorld;

class CPendingHitSystem final
{
public:
    ~CPendingHitSystem() = default;

    static std::unique_ptr<CPendingHitSystem> Create();

    void Execute(CWorld& world, f32_t dt);

    static EntityID Schedule(CWorld& world,
        EntityID owner, eTeam ownerTeam,
        const Vec3& vDirection, f32_t fDelay,
        eProjectileKind kind,
        f32_t fSpeed, f32_t fMaxDist, f32_t fRadius,
        f32_t fDamage, f32_t fStunSec);

private:
    CPendingHitSystem() = default;
};
