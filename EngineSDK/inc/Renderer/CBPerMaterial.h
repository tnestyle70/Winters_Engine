#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"

namespace Engine
{
    // b3 material constants for Mesh3D_PBR.hlsl.
    struct WINTERS_ENGINE CBPerMaterial
    {
        f32_t vAlbedoTint[3];
        f32_t fMetallic;
        f32_t fRoughness;
        f32_t fAmbientOcclusion;
        f32_t fEmissiveIntensity;
        f32_t fMaterialPad0;
        f32_t vEmissiveTint[3];
        f32_t fMaterialPad1;
        f32_t fReserved[4];
    };

    static_assert(sizeof(CBPerMaterial) == 64, "CBPerMaterial must be 64 bytes");

    inline CBPerMaterial MakeDefaultPBRMaterial()
    {
        CBPerMaterial material{};
        material.vAlbedoTint[0] = 1.0f;
        material.vAlbedoTint[1] = 1.0f;
        material.vAlbedoTint[2] = 1.0f;
        material.fMetallic = 0.0f;
        material.fRoughness = 0.5f;
        material.fAmbientOcclusion = 1.0f;
        material.fEmissiveIntensity = 0.0f;
        material.vEmissiveTint[0] = 0.0f;
        material.vEmissiveTint[1] = 0.0f;
        material.vEmissiveTint[2] = 0.0f;
        return material;
    }
}
