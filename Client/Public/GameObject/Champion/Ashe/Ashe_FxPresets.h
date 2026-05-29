#pragma once

#include "Defines.h"
#include "WintersMath.h"
#include "ECS/Entity.h"

class CWorld;

namespace Engine
{
    class CFxStaticMeshRenderer;
}

namespace Ashe::Fx
{
    void SpawnBAArrow(CWorld& world, EntityID owner, const Vec3& origin, const Vec3& dir, f32_t fLifetime);
    void SpawnBAArrowMesh(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
        const Vec3& origin, const Vec3& dir, f32_t fLifetime);
    void SpawnFrostHit(CWorld& world, EntityID target, f32_t fLifetime);
    void SpawnQBuffActive(CWorld& world, EntityID owner, f32_t fDuration);
    void SpawnQBuffMesh(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
        EntityID owner, f32_t fDuration);
    void SpawnQReadySparks(CWorld& world, EntityID owner, f32_t fLifetime);
    void SpawnWVolleyArrow(CWorld& world, EntityID owner, const Vec3& origin, const Vec3& dir, f32_t fLifetime);
    void SpawnWVolleyMuzzle(CWorld& world, EntityID owner, f32_t fLifetime);
    void SpawnWVolleyMesh(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
        const Vec3& origin, const Vec3& dir, f32_t fLifetime);
    void SpawnEHawkshot(CWorld& world, const Vec3& start, const Vec3& dest, f32_t fLifetime);
    void SpawnRCrystalCharge(CWorld& world, EntityID owner, f32_t fLifetime);
    void SpawnRCrystalArrow(CWorld& world, EntityID owner, const Vec3& origin, const Vec3& dir, f32_t fLifetime);
    void SpawnRCrystalArrowMesh(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
        const Vec3& origin, const Vec3& dir, f32_t fLifetime);
    void SpawnRStunFrost(CWorld& world, EntityID target, f32_t fLifetime);
}
