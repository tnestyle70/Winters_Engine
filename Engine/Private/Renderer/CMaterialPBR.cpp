#include "Renderer/CMaterialPBR.h"
#include "RHI/RHITypes.h"
#include "RHI/DX11/DX11ConstantBuffer.h"
#include "Resource/Texture.h"

using namespace Engine;

namespace
{
	ID3D11Device* GetNativeDX11Device(IRHIDevice* pDevice)
	{
		if (!pDevice)
			return nullptr;
		return static_cast<ID3D11Device*>(
			pDevice->GetNativeHandle(eNativeHandleType::DX11Device));
	}
}

CMaterialPBR::CMaterialPBR()
    : m_Constants(MakeDefaultPBRMaterial())
{
}

CMaterialPBR::~CMaterialPBR() = default;

std::unique_ptr<CMaterialPBR> CMaterialPBR::Create(IRHIDevice* pDevice)
{
    ID3D11Device* pNativeDevice = GetNativeDX11Device(pDevice);
    if (!pNativeDevice)
        return nullptr;

    auto material = std::unique_ptr<CMaterialPBR>(new CMaterialPBR());
    material->m_pConstantBuffer = std::make_unique<DX11ConstantBuffer<CBPerMaterial>>();
    if (!material->m_pConstantBuffer->Create(pNativeDevice))
        return nullptr;

    return material;
}

void CMaterialPBR::SetMetallic(f32_t value)
{
    m_Constants.fMetallic = value;
    m_bDirty = true;
}

void CMaterialPBR::SetRoughness(f32_t value)
{
    m_Constants.fRoughness = value;
    m_bDirty = true;
}

void CMaterialPBR::SetAmbientOcclusion(f32_t value)
{
    m_Constants.fAmbientOcclusion = value;
    m_bDirty = true;
}

void CMaterialPBR::SetEmissiveIntensity(f32_t value)
{
    m_Constants.fEmissiveIntensity = value;
    m_bDirty = true;
}

void CMaterialPBR::SetAlbedoTint(f32_t r, f32_t g, f32_t b)
{
    m_Constants.vAlbedoTint[0] = r;
    m_Constants.vAlbedoTint[1] = g;
    m_Constants.vAlbedoTint[2] = b;
    m_bDirty = true;
}

void CMaterialPBR::SetEmissiveTint(f32_t r, f32_t g, f32_t b)
{
    m_Constants.vEmissiveTint[0] = r;
    m_Constants.vEmissiveTint[1] = g;
    m_Constants.vEmissiveTint[2] = b;
    m_bDirty = true;
}

void CMaterialPBR::Bind(IRHIDevice* pDevice)
{
    ID3D11DeviceContext* pContext = static_cast<ID3D11DeviceContext*>(
        pDevice ? pDevice->GetNativeHandle(eNativeHandleType::DX11DeviceContext) : nullptr);
    if (!pContext || !m_pConstantBuffer)
        return;

    if (m_bDirty)
    {
        m_pConstantBuffer->Update(pContext, m_Constants);
        m_bDirty = false;
    }

    m_pConstantBuffer->BindPS(pContext, 3);

    if (m_pAlbedo)
        m_pAlbedo->Bind(pDevice, 0);

    if (m_pNormal)
        m_pNormal->Bind(pDevice, 1);

    if (m_pMetallicRoughness)
        m_pMetallicRoughness->Bind(pDevice, 2);
}
