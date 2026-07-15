#pragma once

#include "Defines.h"
#include "WintersMath.h"
#include "ECS/Entity.h"

class CWorld;

namespace Engine
{
    class CFxStaticMeshRenderer;
}

namespace Viego::Fx
{
    void SpawnBAHit(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
        EntityID owner, EntityID target, f32_t fLifetime);
    void SpawnQSlash(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
        EntityID owner, const Vec3& origin, const Vec3& dir, f32_t fLifetime);
    void StopWChargeGlow(CWorld& world, EntityID owner);
    void SpawnWChargeGlow(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
        EntityID owner, const Vec3& origin, const Vec3& dir);
    void SpawnWMissile(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
        EntityID owner, const Vec3& origin, const Vec3& dir, f32_t fLifetime);
    void SpawnEMist(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
        EntityID owner, const Vec3& origin, const Vec3& dir, f32_t fDuration);
    void SpawnRImpact(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
        EntityID owner, const Vec3& origin, const Vec3& dir, f32_t fLifetime);
    void StopSoulIdle(CWorld& world, EntityID owner);
    void StopAllSoulIdle(CWorld& world);
    void SpawnSoulIdle(CWorld& world, EntityID owner, f32_t fLifetime);
}
