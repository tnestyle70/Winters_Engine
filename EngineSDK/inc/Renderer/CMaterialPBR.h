#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "CBPerMaterial.h"
#include "RHI/IRHIDevice.h"
#include <memory>

template<typename T> class DX11ConstantBuffer;

namespace Engine
{
    class CTexture;

    class WINTERS_ENGINE CMaterialPBR
    {
    public:
        ~CMaterialPBR();
        CMaterialPBR(const CMaterialPBR&) = delete;
        CMaterialPBR& operator=(const CMaterialPBR&) = delete;
        CMaterialPBR(CMaterialPBR&&) noexcept = default;
        CMaterialPBR& operator=(CMaterialPBR&&) noexcept = default;

        static std::unique_ptr<CMaterialPBR> Create(IRHIDevice* pDevice);

        void SetAlbedoMap(CTexture* pTexture)            { m_pAlbedo = pTexture; }
        void SetNormalMap(CTexture* pTexture)            { m_pNormal = pTexture; }
        void SetMetallicRoughnessMap(CTexture* pTexture) { m_pMetallicRoughness = pTexture; }

        void SetMetallic(f32_t value);
        void SetRoughness(f32_t value);
        void SetAmbientOcclusion(f32_t value);
        void SetEmissiveIntensity(f32_t value);
        void SetAlbedoTint(f32_t r, f32_t g, f32_t b);
        void SetEmissiveTint(f32_t r, f32_t g, f32_t b);

        void Bind(IRHIDevice* pDevice);

        const CBPerMaterial& GetConstants() const { return m_Constants; }

    private:
        CMaterialPBR();

        bool_t m_bDirty = true;
        CBPerMaterial m_Constants{};
        CTexture* m_pAlbedo = nullptr;
        CTexture* m_pNormal = nullptr;
        CTexture* m_pMetallicRoughness = nullptr;
        std::unique_ptr<DX11ConstantBuffer<CBPerMaterial>> m_pConstantBuffer;
    };
}
