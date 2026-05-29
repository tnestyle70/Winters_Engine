#pragma once

#include "ECS/Entity.h"
#include "FX/FxAsset.h"
#include "FX/ParticlePool.h"
#include "WintersMath.h"
#include "WintersTypes.h"

#include <array>

struct FxInstanceComponent
{
    FxAssetHandle hAsset{};
    EntityHandle hAttachTo = NULL_ENTITY_HANDLE;
    Vec3 vAttachOffset = { 0.f, 0.f, 0.f };
    f32_t fAge = 0.f;
    f32_t fLifetime = 3.f;
    bool_t bLoop = false;
    std::array<u32_t, 4> aPoolIndices = {
        FX_INVALID_PARTICLE,
        FX_INVALID_PARTICLE,
        FX_INVALID_PARTICLE,
        FX_INVALID_PARTICLE
    };
};
