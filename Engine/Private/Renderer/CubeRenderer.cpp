#include "Renderer/CubeRenderer.h"
#include "WintersMath.h"
#include "Framework/CEngineApp.h"
#include "RHI/IRHIDevice.h"
#include "RHI/RHITypes.h"
#include "RHI/DX11/DX11Shader.h"
#include "RHI/DX11/DX11Buffer.h"
#include "RHI/DX11/DX11Pipeline.h"
#include "RHI/DX11/DX11ConstantBuffer.h"
#include "RHI/Geometry/CubeGeometry.h"

namespace
{
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
}

// ─────────────────────────────────────────────────────────────────
//  CubeRenderer 구현
//
//  CubeGeometry의 정적 데이터를 GPU에 업로드하고,
//  매 프레임 ConstantBuffer로 World/ViewProjection 행렬을 전달.
// ─────────────────────────────────────────────────────────────────

struct CubeRenderer::Impl
{
    DX11Shader                    shader;
    DX11Buffer                    meshBuffer;
    DX11Pipeline                  pipeline;
    DX11ConstantBuffer<CBPerFrame>  cbPerFrame;
    DX11ConstantBuffer<CBPerObject> cbPerObject;
    bool                          bReady = false;
};

CubeRenderer::CubeRenderer()
    : m_pImpl(new Impl{})
{
}

CubeRenderer::~CubeRenderer()
{
    Shutdown();
    delete m_pImpl;
    m_pImpl = nullptr;
}

bool CubeRenderer::Init(const wchar_t* hlslPath)
{
    IRHIDevice& device = CEngineApp::Get().GetDevice();
    ID3D11Device* pDev = GetNativeDX11Device(&device);
    if (!pDev)
        return false;

    // 1. 셰이더
    if (!m_pImpl->shader.Load(pDev, hlslPath, "VS", "PS"))
    {
        OutputDebugStringA("[CubeRenderer] Shader load failed\n");
        return false;
    }

    // 2. VertexBuffer + IndexBuffer
    if (!m_pImpl->meshBuffer.CreateVertex(
            pDev,
            CubeGeometry::Vertices.data(),
            CubeGeometry::VERTEX_STRIDE,
            CubeGeometry::VERTEX_COUNT))
    {
        OutputDebugStringA("[CubeRenderer] VertexBuffer create failed\n");
        return false;
    }

    if (!m_pImpl->meshBuffer.CreateIndex(
            pDev,
            CubeGeometry::Indices.data(),
            CubeGeometry::INDEX_COUNT,
            false))  // uint16
    {
        OutputDebugStringA("[CubeRenderer] IndexBuffer create failed\n");
        return false;
    }

    // 3. Pipeline (3D 레이아웃: POS + NORMAL + COLOR)
    if (!m_pImpl->pipeline.Create3D(pDev, m_pImpl->shader.GetVSBlob()))
    {
        OutputDebugStringA("[CubeRenderer] Pipeline create failed\n");
        return false;
    }

    // 4. Constant Buffers
    if (!m_pImpl->cbPerFrame.Create(pDev) || !m_pImpl->cbPerObject.Create(pDev))
    {
        OutputDebugStringA("[CubeRenderer] ConstantBuffer create failed\n");
        return false;
    }

    m_pImpl->bReady = true;
    return true;
}

void CubeRenderer::UpdateTransform(const Mat4& worldMatrix)
{
    if (!m_pImpl || !m_pImpl->bReady) return;

    ID3D11DeviceContext* pCtx = GetNativeDX11Context(&CEngineApp::Get().GetDevice());
    if (!pCtx)
        return;
    CBPerObject data;
    data.world = worldMatrix.m;
    m_pImpl->cbPerObject.Update(pCtx, data);
}

void CubeRenderer::UpdateCamera(const Mat4& viewProjection)
{
    if (!m_pImpl || !m_pImpl->bReady) return;

    ID3D11DeviceContext* pCtx = GetNativeDX11Context(&CEngineApp::Get().GetDevice());
    if (!pCtx)
        return;
    CBPerFrame data;
    data.viewProjection = viewProjection.m;
    m_pImpl->cbPerFrame.Update(pCtx, data);
}

void CubeRenderer::Render()
{
    if (!m_pImpl || !m_pImpl->bReady) return;

    ID3D11DeviceContext* pCtx = GetNativeDX11Context(&CEngineApp::Get().GetDevice());
    if (!pCtx)
        return;

    // Pipeline + Shader 바인딩
    m_pImpl->pipeline.Bind(pCtx);
    m_pImpl->shader.Bind(pCtx);

    // ConstantBuffer 바인딩
    m_pImpl->cbPerFrame.BindVS(pCtx, 0);   // b0
    m_pImpl->cbPerObject.BindVS(pCtx, 1);  // b1

    // VB + IB 바인딩
    m_pImpl->meshBuffer.BindVertex(pCtx, CubeGeometry::VERTEX_STRIDE);
    m_pImpl->meshBuffer.BindIndex(pCtx);

    // Topology + Draw
    pCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pImpl->meshBuffer.DrawIndexed(pCtx);

    m_pImpl->shader.Unbind(pCtx);
}

void CubeRenderer::Shutdown()
{
    if (!m_pImpl) return;

    m_pImpl->cbPerObject.Release();
    m_pImpl->cbPerFrame.Release();
    m_pImpl->pipeline.Release();
    m_pImpl->meshBuffer.Release();
    m_pImpl->shader.Release();
    m_pImpl->bReady = false;
}
