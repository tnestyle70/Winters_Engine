#pragma once
#include "Defines.h"
#include "WintersMath.h"
#include "ECS/Entity.h"

#include <vector>

class CWorld;

namespace Engine
{
    class CFxStaticMeshRenderer;
}

namespace YasuoFx
{
    EntityID SpawnQStraight(CWorld& world, const Vec3& vOrigin, const Vec3& vForward,
        f32_t fLifetime);

    EntityID SpawnQBuildUp(CWorld& world, EntityID owner, f32_t fLifetime);

    void SpawnQTornado(CWorld& world,
        const Vec3& vOrigin, const Vec3& vForward,
        f32_t fSpeed, f32_t fLifetime);

    void SpawnWWindWall(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
        const Vec3& vOrigin, const Vec3& vForward,
        f32_t fLifetime, f32_t fWidth, f32_t fHeight,
        f32_t fMeshScale = 0.01f,
        EntityID attachTo = NULL_ENTITY,
        std::vector<EntityID>* pSpawnedEntities = nullptr);

    EntityID SpawnEDashTrail(CWorld& world,
        Engine::CFxStaticMeshRenderer* pRenderer,
        EntityID owner, const Vec3& vForward, f32_t fLifetime);

    EntityID SpawnEDashTargetRing(CWorld& world,
        EntityID target, f32_t fLifetime);

    void SpawnPassiveShield(CWorld& world,
        Engine::CFxStaticMeshRenderer* pRenderer,
        EntityID owner, f32_t fLifetime);

    void SpawnEQRing(CWorld& world, EntityID owner,
        const Vec3& vCenter, f32_t fLifetime, f32_t fRadius);

    void SpawnRLastBreath(CWorld& world,
        const Vec3& vLandPos, EntityID owner, f32_t fLifetime);
}
