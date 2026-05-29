#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include <DirectXMath.h>

namespace Engine
{
    inline constexpr u32_t kMaxForwardPointLights = 4;

    struct PointLightData
    {
        DirectX::XMFLOAT3 positionWorld;
        f32_t             radius;
        DirectX::XMFLOAT3 color;
        f32_t             intensity;
    };
    static_assert(sizeof(PointLightData) == 32, "PointLightData must be 32 bytes");
}
