#pragma once
#include "Defines.h"
#include "WintersMath.h"
#include "ECS/Entity.h"

class CWorld;

namespace Engine
{
    class CFxStaticMeshRenderer;
}

namespace YoneFx
{
    void SpawnBasicAttackImpact(CWorld& world, EntityID target, const Vec3& vHitPos, const Vec3& vForward);

    void SpawnMortalSteel(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
        EntityID caster, const Vec3& vOrigin, const Vec3& vForward);

    void SpawnSpiritCleave(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
        EntityID caster, const Vec3& vOrigin, const Vec3& vForward);

    void SpawnSoulOut(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
        EntityID caster, const Vec3& vOrigin, const Vec3& vForward);

    void SpawnSoulReturn(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
        EntityID caster, const Vec3& vOrigin, const Vec3& vForward);

    void SpawnFateSealed(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
        EntityID caster, EntityID target, const Vec3& vOrigin, const Vec3& vForward);
}
