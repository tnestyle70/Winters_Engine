#pragma once

#include "Defines.h"
#include "WintersMath.h"
#include "ECS/Entity.h"

class CWorld;

namespace Engine
{
    class CFxStaticMeshRenderer;
}

namespace Fiora::Fx
{
    void SpawnBAHitSpark(CWorld& world, EntityID target, f32_t fLifetime);
    void SpawnQSlash(CWorld& world, EntityID owner, const Vec3& dir, f32_t fLifetime);
    void SpawnWParryActive(CWorld& world, EntityID owner, f32_t fDuration);
    void SpawnWBlockFlash(CWorld& world, EntityID owner, f32_t fLifetime);
    void SpawnEBladeworkBuff(CWorld& world, EntityID owner, f32_t fDuration);
    void SpawnRMark(CWorld& world, EntityID target, f32_t fDuration);
    void SpawnRHealZone(CWorld& world, EntityID owner, f32_t fDuration);
}
