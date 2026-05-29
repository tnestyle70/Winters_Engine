#pragma once
#include "Defines.h"
#include "WintersMath.h"
#include "ECS/Entity.h"

class CWorld;

namespace Engine
{
    class CFxStaticMeshRenderer;
}

namespace YasuoFx
{
    EntityID SpawnQStraight(CWorld& world, const Vec3& vOrigin, const Vec3& vForward,
        f32_t fSpeed, f32_t fLifetime);

    EntityID SpawnQBuildUp(CWorld& world, EntityID owner, f32_t fLifetime);

    void SpawnQTornado(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
        const Vec3& vOrigin, const Vec3& vForward,
        f32_t fSpeed, f32_t fLifetime, f32_t fScale,
        const Vec4& vColor = Vec4{ 1.f, 1.4f, 2.2f, 1.f });

    void SpawnWWindWall(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
        const Vec3& vOrigin, const Vec3& vForward,
        f32_t fLifetime, f32_t fWidth, f32_t fHeight,
        f32_t fMeshScale = 0.01f);

    EntityID SpawnEDashTrail(CWorld& world, EntityID owner, f32_t fLifetime);

    void SpawnEQRing(CWorld& world, EntityID owner,
        const Vec3& vCenter, f32_t fLifetime, f32_t fRadius);

    void SpawnRLastBreath(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
        const Vec3& vLandPos, EntityID owner, f32_t fLifetime);
}
