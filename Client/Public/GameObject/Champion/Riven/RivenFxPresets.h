#pragma once
#include "Defines.h"
#include "WintersMath.h"
#include "ECS/Entity.h"

class CWorld;

namespace Engine
{
    class CFxStaticMeshRenderer;
}

namespace RivenFx
{
    void SpawnQSlash(CWorld& world, EntityID owner, uint8_t stackIndex, f32_t fLifetime,
        Engine::CFxStaticMeshRenderer* pRenderer = nullptr);
    void SpawnWNova(CWorld& world, EntityID owner, f32_t fLifetime,
        Engine::CFxStaticMeshRenderer* pRenderer = nullptr);
    void SpawnEShield(CWorld& world, EntityID owner, f32_t fDuration,
        Engine::CFxStaticMeshRenderer* pRenderer = nullptr);
    void SpawnRActivate(CWorld& world, EntityID owner, f32_t fLifetime,
        Engine::CFxStaticMeshRenderer* pRenderer = nullptr);
    void SpawnRBlade(CWorld& world, EntityID owner, f32_t fDuration,
        Engine::CFxStaticMeshRenderer* pRenderer = nullptr);
    void SpawnRWindSlash(CWorld& world, EntityID owner, f32_t fLifetime,
        Engine::CFxStaticMeshRenderer* pRenderer = nullptr);
}
