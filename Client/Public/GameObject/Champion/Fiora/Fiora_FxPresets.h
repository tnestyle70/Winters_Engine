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
    enum class eVitalVisualKind : u8_t
    {
        Passive,
        GrandChallenge,
    };

    void SpawnQSlash(CWorld& world, EntityID owner, const Vec3& dir, f32_t lifetime);
    void SpawnWParryActive(CWorld& world, EntityID owner, const Vec3& dir,
        f32_t length, f32_t halfWidth, f32_t duration);
    void SpawnWParrySuccess(CWorld& world, EntityID owner, const Vec3& dir,
        f32_t length, f32_t halfWidth, f32_t duration);
    void SpawnWRelease(CWorld& world, EntityID owner, const Vec3& dir,
        const Vec3& endWorldPos, f32_t halfWidth, f32_t lifetime);
    void SpawnEBladeworkBuff(CWorld& world, EntityID owner, f32_t duration);
    void StopEBladeworkBuff(CWorld& world, EntityID owner);
    void SpawnEHitSpark(CWorld& world, EntityID target, f32_t lifetime);
    void SpawnVital(CWorld& world, EntityID owner, EntityID target,
        const Vec3& outward, eVitalVisualKind kind, f32_t duration);
    void ConsumeVital(CWorld& world, EntityID owner, EntityID target,
        const Vec3& outward, eVitalVisualKind kind);
    void ClearVitals(CWorld& world, EntityID owner, eVitalVisualKind kind);
    void SpawnRRing(CWorld& world, EntityID owner, EntityID target,
        const Vec3& center, f32_t duration);
    void ClearRPresentation(CWorld& world, EntityID owner);
    void SpawnRHealZone(CWorld& world, const Vec3& center, f32_t duration);
}
