#pragma once

#include "Defines.h"
#include "WintersMath.h"
#include "ECS/Entity.h"

class CWorld;

namespace Annie::Fx
{
    void SpawnBAFireFlash(CWorld& world, EntityID target, f32_t fLifetime);
    void SpawnQFireball(CWorld& world, EntityID owner, EntityID target, f32_t fLifetime);
    void SpawnWConeFire(CWorld& world, EntityID owner, const Vec3& dir, f32_t fLifetime);
    void SpawnEShield(CWorld& world, EntityID owner, f32_t fDuration);
    void SpawnRTibbersSummon(CWorld& world, EntityID owner, const Vec3& groundPos, f32_t fLifetime);
    void SpawnStunCharge(CWorld& world, EntityID owner, f32_t fLifetime);
}
