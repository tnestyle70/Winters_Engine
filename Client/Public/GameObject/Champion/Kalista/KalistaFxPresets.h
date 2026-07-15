#pragma once
#include "Defines.h"
#include "WintersMath.h"
#include "ECS/Entity.h"

class CWorld;
class CFxSystem;

namespace Engine
{
    class CFxStaticMeshRenderer;
}

namespace KalistaFx
{
    bool_t PreloadWSentinelTextures(CFxSystem& fxSystem);

    EntityID SpawnQSpear(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
        const Vec3& vOrigin, const Vec3& vForward,
        f32_t fSpeed, f32_t fLifetime, f32_t fScale);

    EntityID SpawnQSpear(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
        const Vec3& vOrigin, const Vec3& vForward,
        f32_t fSpeed, f32_t fLifetime, const Vec3& vScale);

    EntityID SpawnESpearStuck(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
        EntityID target, f32_t fScale);

    void SpawnEExplode(CWorld& world, EntityID target, f32_t fLifetime);

    void SpawnWSentinelIdle(CWorld& world, EntityID sentinel,
        const Vec3& vForward, f32_t fLifetime, f32_t fSightRange,
        u8_t iOwnerTeam,
        EntityID* pOutAvatarFx = nullptr,
        EntityID* pOutConeFx = nullptr);
}
