#pragma once

#include "Defines.h"

struct ProjectileVisualDesc
{
    const char* pszSpawnCue = nullptr;
    const char* pszHitCue = nullptr;
    const char* pszAttachedCue = nullptr;
    const wchar_t* pszFallbackSpawnTexture = nullptr;
    const wchar_t* pszFallbackHitTexture = nullptr;
    f32_t fFallbackSpawnWidth = 0.8f;
    f32_t fFallbackSpawnHeight = 0.8f;
    f32_t fFallbackHitWidth = 1.4f;
    f32_t fFallbackHitHeight = 1.4f;
    bool_t bUseGenericSpawnFallback = true;
    bool_t bUseGenericHitFallback = true;
};

namespace ProjectileVisualCatalog
{
    const ProjectileVisualDesc& Resolve(u16_t kind);
    bool_t IsTurretProjectileKind(u16_t kind);
}
