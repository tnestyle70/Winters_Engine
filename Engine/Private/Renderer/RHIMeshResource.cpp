#include "WintersPCH.h"

#include "Renderer/RHIMeshResource.h"

#include "RHI/IRHIDevice.h"

CRHIMeshResource::~CRHIMeshResource()
{
    Shutdown();
}

std::unique_ptr<CRHIMeshResource> CRHIMeshResource::CreateIndexed(
    IRHIDevice* pDevice,
    const void* pVertexData,
    u32_t vertexBytes,
    u32_t vertexStride,
    const u32_t* pIndexData,
    u32_t indexCount,
    eRenderVertexLayout vertexLayout,
    const char* debugName)
{
    if (!pDevice ||
        !pVertexData ||
        vertexBytes == 0 ||
        vertexStride == 0 ||
        !pIndexData ||
        indexCount == 0)
    {
        return nullptr;
    }

    auto pResource = std::unique_ptr<CRHIMeshResource>(new CRHIMeshResource());
    if (!pResource)
        return nullptr;

    pResource->m_pDevice = pDevice;

    RHIBufferDesc vertexDesc{};
    vertexDesc.sizeBytes = vertexBytes;
    vertexDesc.usage = eRHIBufferUsage::Vertex;
    vertexDesc.memoryUsage = eRHIMemoryUsage::Default;
    vertexDesc.debugName = debugName ? debugName : "RHIMesh.VertexBuffer";

    RHIBufferDesc indexDesc{};
    indexDesc.sizeBytes = indexCount * static_cast<u32_t>(sizeof(u32_t));
    indexDesc.usage = eRHIBufferUsage::Index;
    indexDesc.memoryUsage = eRHIMemoryUsage::Default;
    indexDesc.debugName = debugName ? debugName : "RHIMesh.IndexBuffer";

    RHIMeshSlice slice{};
    slice.hVertexBuffer = pDevice->CreateBuffer(vertexDesc, pVertexData);
    slice.hIndexBuffer = pDevice->CreateBuffer(indexDesc, pIndexData);
    slice.vertexStride = vertexStride;
    slice.indexCount = indexCount;
    slice.vertexLayout = vertexLayout;

    if (!slice.hVertexBuffer.IsValid() || !slice.hIndexBuffer.IsValid())
    {
        pResource->Shutdown();
        return nullptr;
    }

    pResource->m_Slice = slice;
    pResource->m_bReady = true;
    return pResource;
}

const RHIMeshSlice* CRHIMeshResource::GetSlices() const
{
    return m_bReady ? &m_Slice : nullptr;
}

u32_t CRHIMeshResource::GetSliceCount() const
{
    return m_bReady ? 1u : 0u;
}

bool_t CRHIMeshResource::IsReady() const
{
    return m_pDevice &&
        m_bReady &&
        m_Slice.hVertexBuffer.IsValid() &&
        m_Slice.hIndexBuffer.IsValid();
}

void CRHIMeshResource::Shutdown()
{
    if (!m_pDevice)
    {
        m_Slice = {};
        m_bReady = false;
        return;
    }

    if (m_Slice.hVertexBuffer.IsValid())
        m_pDevice->DestroyBuffer(m_Slice.hVertexBuffer);
    if (m_Slice.hIndexBuffer.IsValid())
        m_pDevice->DestroyBuffer(m_Slice.hIndexBuffer);

    m_Slice = {};
    m_bReady = false;
    m_pDevice = nullptr;
}
