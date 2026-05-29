#pragma once

#include "Defines.h"
#include "WintersMath.h"
#include "ECS/Entity.h"

class CWorld;

namespace Engine
{
    class CFxStaticMeshRenderer;
}

namespace Jax::Fx
{
    void SpawnBAHitFlash(CWorld& world, EntityID target, f32_t fLifetime);
    void SpawnQLeapTrail(CWorld& world, EntityID owner, const Vec3& dir, f32_t fLifetime,
        Engine::CFxStaticMeshRenderer* pRenderer = nullptr);
    void SpawnWEmpowerGlow(CWorld& world, EntityID owner, f32_t fDuration);
    void SpawnECounterSlash(CWorld& world, EntityID owner, f32_t fLifetime,
        Engine::CFxStaticMeshRenderer* pRenderer = nullptr);
    void SpawnRBuffAura(CWorld& world, EntityID owner, f32_t fDuration,
        Engine::CFxStaticMeshRenderer* pRenderer = nullptr);
    void SpawnRThirdAttackAOE(CWorld& world, EntityID owner, f32_t fLifetime,
        Engine::CFxStaticMeshRenderer* pRenderer = nullptr);
}
