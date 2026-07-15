#pragma once

#include "Defines.h"

struct ReplicatedProjectilePresentationTag
{
};

struct ProjectileVisualDesc
{
    const char* pszSpawnCue = nullptr;
    const char* pszHitCue = nullptr;
    const char* pszAttachedCue = nullptr;
    const char* pszBarrierCue = nullptr;
    const char* pszTerrainCue = nullptr;
    const char* pszExpireCue = nullptr;
    f32_t fYawOffset = 0.f;
    // 부착 큐(박힌 창 등)를 히트마다 랜덤 각도/오프셋으로 분산시킨다 (누적 시각화).
    bool_t bAttachedCueRandomJitter = false;
};

namespace ProjectileVisualCatalog
{
    const ProjectileVisualDesc& Resolve(u16_t kind);
    bool_t IsStructureProjectileKind(u16_t kind);
}
