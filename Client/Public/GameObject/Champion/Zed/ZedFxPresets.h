#pragma once
#include "Defines.h"
#include "WintersMath.h"
#include "ECS/Entity.h"

class CWorld;

namespace Engine
{
    class CFxStaticMeshRenderer;
}

namespace ZedFx
{
    void SpawnQRazor(CWorld& world, EntityID owner, const Vec3& dir, f32_t fLifetime);
    void SpawnWShadow(CWorld& world, EntityID owner, const Vec3& groundPos, f32_t fDuration);
    void SpawnESlash(CWorld& world, EntityID owner, f32_t fLifetime);
    void SpawnRMark(CWorld& world, EntityID target, f32_t fLifetime);
    void SpawnShadowCloneModel(CWorld& world, EntityID owner, const Vec3& groundPos, const Vec3& direction, f32_t fDuration);
    bool_t MoveShadowCloneModel(CWorld& world, EntityID owner, const Vec3& groundPos, const Vec3& direction);
    void TickShadowCloneModels(CWorld& world, f32_t fDeltaTime);
}
