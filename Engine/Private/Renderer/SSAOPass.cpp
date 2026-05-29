#include "Renderer/SSAOPass.h"

#include "RHI/IRHIDevice.h"
#include "RHI/RHITypes.h"
#include "WintersPaths.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

using namespace Engine;

namespace
{
    struct CBGTAOData
    {
        DirectX::XMFLOAT4X4 viewProj;
        DirectX::XMFLOAT4X4 viewProjInv;
        DirectX::XMFLOAT2 screenSize;
        f32_t radius;
        f32_t intensity;
        f32_t thicknessHeuristic;
        f32_t pad[3];
    };
    static_assert(sizeof(CBGTAOData) % 16 == 0);

    struct CBBlurData
    {
        DirectX::XMFLOAT2 screenSize;
        DirectX::XMFLOAT2 pad;
    };
    static_assert(sizeof(CBBlurData) % 16 == 0);

    ID3D11Device* GetNativeDX11Device(IRHIDevice* pDevice)
    {
        if (!pDevice)
            return nullptr;
        return static_cast<ID3D11Device*>(
            pDevice->GetNativeHandle(eNativeHandleType::DX11Device));
    }

    ID3D11DeviceContext* GetNativeDX11Context(IRHIDevice* pDevice)
    {
        if (!pDevice)
            return nullptr;
        return static_cast<ID3D11DeviceContext*>(
            pDevice->GetNativeHandle(eNativeHandleType::DX11DeviceContext));
    }

    ID3DBlob* CompileComputeShaderBlob(const wchar_t* pPath, const char* pEntry)
    {
        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

        ID3DBlob* pCode = nullptr;
        ID3DBlob* pError = nullptr;
        HRESULT hr = D3DCompileFromFile(
            pPath,
            nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            pEntry,
            "cs_5_0",
            flags,
            0,
            &pCode,
            &pError);

        if (FAILED(hr))
        {
            if (pError)
            {
                OutputDebugStringA("[SSAOPass] Compute shader compile failed:\n");
                OutputDebugStringA(static_cast<const char*>(pError->GetBufferPointer()));
                pError->Release();
            }
            if (pCode)
                pCode->Release();
            return nullptr;
        }

        if (pError)
            pError->Release();
        return pCode;
    }
}

struct CSSAOPass::Impl
{
    u32_t width = 0;
    u32_t height = 0;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> pAOTextureRaw;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> pAOTextureRawSRV;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> pAOTextureRawUAV;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> pAOTextureFiltered;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> pAOTextureFilteredSRV;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> pAOTextureFilteredUAV;

    Microsoft::WRL::ComPtr<ID3D11ComputeShader> pGTAOCS;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> pBlurCS;
    Microsoft::WRL::ComPtr<ID3D11Buffer> pGTAOCB;
    Microsoft::WRL::ComPtr<ID3D11Buffer> pBlurCB;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> pPointSampler;
};

CSSAOPass::CSSAOPass()
    : m_pImpl(std::make_unique<Impl>())
{
}

CSSAOPass::~CSSAOPass() = default;

std::unique_ptr<CSSAOPass> CSSAOPass::Create(IRHIDevice* pDevice, u32_t width, u32_t height)
{
    auto pPass = std::unique_ptr<CSSAOPass>(new CSSAOPass());
    if (!pPass->Initialize(pDevice, width, height))
        return nullptr;
    return pPass;
}

bool_t CSSAOPass::Initialize(IRHIDevice* pDevice, u32_t width, u32_t height)
{
    ID3D11Device* pNativeDevice = GetNativeDX11Device(pDevice);
    if (!pNativeDevice || width == 0 || height == 0)
        return false;

    m_pImpl->width = width;
    m_pImpl->height = height;

    D3D11_TEXTURE2D_DESC aoDesc = {};
    aoDesc.Width = width;
    aoDesc.Height = height;
    aoDesc.MipLevels = 1;
    aoDesc.ArraySize = 1;
    aoDesc.Format = DXGI_FORMAT_R16_FLOAT;
    aoDesc.SampleDesc.Count = 1;
    aoDesc.Usage = D3D11_USAGE_DEFAULT;
    aoDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

    if (FAILED(pNativeDevice->CreateTexture2D(&aoDesc, nullptr, m_pImpl->pAOTextureRaw.GetAddressOf())))
        return false;
    if (FAILED(pNativeDevice->CreateShaderResourceView(
        m_pImpl->pAOTextureRaw.Get(), nullptr, m_pImpl->pAOTextureRawSRV.GetAddressOf())))
        return false;
    if (FAILED(pNativeDevice->CreateUnorderedAccessView(
        m_pImpl->pAOTextureRaw.Get(), nullptr, m_pImpl->pAOTextureRawUAV.GetAddressOf())))
        return false;

    if (FAILED(pNativeDevice->CreateTexture2D(&aoDesc, nullptr, m_pImpl->pAOTextureFiltered.GetAddressOf())))
        return false;
    if (FAILED(pNativeDevice->CreateShaderResourceView(
        m_pImpl->pAOTextureFiltered.Get(), nullptr, m_pImpl->pAOTextureFilteredSRV.GetAddressOf())))
        return false;
    if (FAILED(pNativeDevice->CreateUnorderedAccessView(
        m_pImpl->pAOTextureFiltered.Get(), nullptr, m_pImpl->pAOTextureFilteredUAV.GetAddressOf())))
        return false;

    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cbDesc.ByteWidth = sizeof(CBGTAOData);
    if (FAILED(pNativeDevice->CreateBuffer(&cbDesc, nullptr, m_pImpl->pGTAOCB.GetAddressOf())))
        return false;
    cbDesc.ByteWidth = sizeof(CBBlurData);
    if (FAILED(pNativeDevice->CreateBuffer(&cbDesc, nullptr, m_pImpl->pBlurCB.GetAddressOf())))
        return false;

    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MinLOD = 0.f;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(pNativeDevice->CreateSamplerState(&samplerDesc, m_pImpl->pPointSampler.GetAddressOf())))
        return false;

    wchar_t gtaoShaderPath[MAX_PATH] = {};
    if (!WintersResolveContentPath(L"Shaders/SSAO/GTAO_CS.hlsl", gtaoShaderPath, MAX_PATH))
        return false;
    ID3DBlob* pGTAOBlob = CompileComputeShaderBlob(gtaoShaderPath, "CS_Main");
    if (!pGTAOBlob)
        return false;
    HRESULT hr = pNativeDevice->CreateComputeShader(
        pGTAOBlob->GetBufferPointer(),
        pGTAOBlob->GetBufferSize(),
        nullptr,
        m_pImpl->pGTAOCS.GetAddressOf());
    pGTAOBlob->Release();
    if (FAILED(hr))
        return false;

    wchar_t blurShaderPath[MAX_PATH] = {};
    if (!WintersResolveContentPath(L"Shaders/SSAO/GTAO_Blur_CS.hlsl", blurShaderPath, MAX_PATH))
        return false;
    ID3DBlob* pBlurBlob = CompileComputeShaderBlob(blurShaderPath, "CS_Main");
    if (!pBlurBlob)
        return false;
    hr = pNativeDevice->CreateComputeShader(
        pBlurBlob->GetBufferPointer(),
        pBlurBlob->GetBufferSize(),
        nullptr,
        m_pImpl->pBlurCS.GetAddressOf());
    pBlurBlob->Release();
    if (FAILED(hr))
        return false;

    return true;
}

void CSSAOPass::Execute(IRHIDevice* pDevice,
    void* pDepthSRVNative,
    void* pNormalSRVNative,
    const Mat4& matViewProj)
{
    ID3D11DeviceContext* pContext = GetNativeDX11Context(pDevice);
    if (!m_bEnabled || !pContext || !pDepthSRVNative || !pNormalSRVNative)
        return;

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(pContext->Map(m_pImpl->pGTAOCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        CBGTAOData data = {};
        data.viewProj = matViewProj.m;
        const DirectX::XMMATRIX invViewProj =
            DirectX::XMMatrixInverse(nullptr, matViewProj.ToXMMATRIX());
        DirectX::XMStoreFloat4x4(&data.viewProjInv, invViewProj);
        data.screenSize = {
            static_cast<f32_t>(m_pImpl->width),
            static_cast<f32_t>(m_pImpl->height)
        };
        data.radius = m_fRadius;
        data.intensity = m_fIntensity;
        data.thicknessHeuristic = m_fThicknessHeuristic;
        memcpy(mapped.pData, &data, sizeof(data));
        pContext->Unmap(m_pImpl->pGTAOCB.Get(), 0);
    }

    ID3D11ShaderResourceView* pDepthSRV = static_cast<ID3D11ShaderResourceView*>(pDepthSRVNative);
    ID3D11ShaderResourceView* pNormalSRV = static_cast<ID3D11ShaderResourceView*>(pNormalSRVNative);
    ID3D11ShaderResourceView* pGTAOSRVs[2] = { pDepthSRV, pNormalSRV };
    ID3D11UnorderedAccessView* pRawUAV = m_pImpl->pAOTextureRawUAV.Get();
    ID3D11SamplerState* pPointSampler = m_pImpl->pPointSampler.Get();
    ID3D11Buffer* pGTAOCB = m_pImpl->pGTAOCB.Get();

    pContext->CSSetShader(m_pImpl->pGTAOCS.Get(), nullptr, 0);
    pContext->CSSetConstantBuffers(0, 1, &pGTAOCB);
    pContext->CSSetSamplers(0, 1, &pPointSampler);
    pContext->CSSetShaderResources(0, 2, pGTAOSRVs);
    pContext->CSSetUnorderedAccessViews(0, 1, &pRawUAV, nullptr);
    pContext->Dispatch((m_pImpl->width + 7) / 8, (m_pImpl->height + 7) / 8, 1);

    ID3D11ShaderResourceView* pNullSRVs[2] = { nullptr, nullptr };
    ID3D11UnorderedAccessView* pNullUAV = nullptr;
    pContext->CSSetShaderResources(0, 2, pNullSRVs);
    pContext->CSSetUnorderedAccessViews(0, 1, &pNullUAV, nullptr);
    pContext->CSSetShader(nullptr, nullptr, 0);

    if (SUCCEEDED(pContext->Map(m_pImpl->pBlurCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        const CBBlurData data = {
            { static_cast<f32_t>(m_pImpl->width), static_cast<f32_t>(m_pImpl->height) },
            { 0.f, 0.f }
        };
        memcpy(mapped.pData, &data, sizeof(data));
        pContext->Unmap(m_pImpl->pBlurCB.Get(), 0);
    }

    ID3D11ShaderResourceView* pBlurSRVs[2] = {
        m_pImpl->pAOTextureRawSRV.Get(),
        pDepthSRV
    };
    ID3D11UnorderedAccessView* pFilteredUAV = m_pImpl->pAOTextureFilteredUAV.Get();
    ID3D11Buffer* pBlurCB = m_pImpl->pBlurCB.Get();

    pContext->CSSetShader(m_pImpl->pBlurCS.Get(), nullptr, 0);
    pContext->CSSetConstantBuffers(0, 1, &pBlurCB);
    pContext->CSSetShaderResources(0, 2, pBlurSRVs);
    pContext->CSSetUnorderedAccessViews(0, 1, &pFilteredUAV, nullptr);
    pContext->Dispatch((m_pImpl->width + 7) / 8, (m_pImpl->height + 7) / 8, 1);

    pContext->CSSetShaderResources(0, 2, pNullSRVs);
    pContext->CSSetUnorderedAccessViews(0, 1, &pNullUAV, nullptr);
    pContext->CSSetShader(nullptr, nullptr, 0);
}

void* CSSAOPass::GetOutputSRVNative() const
{
    return m_pImpl ? m_pImpl->pAOTextureFilteredSRV.Get() : nullptr;
}
