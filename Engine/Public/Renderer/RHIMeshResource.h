#pragma once

#include "WintersAPI.h"
#include "Renderer/RenderWorldSnapshot.h"

#include <memory>

class IRHIDevice;

class WINTERS_ENGINE CRHIMeshResource final
{
public:
    ~CRHIMeshResource();

    CRHIMeshResource(const CRHIMeshResource&) = delete;
    CRHIMeshResource& operator=(const CRHIMeshResource&) = delete;

    static std::unique_ptr<CRHIMeshResource> CreateIndexed(
        IRHIDevice* pDevice,
        const void* pVertexData,
        u32_t vertexBytes,
        u32_t vertexStride,
        const u32_t* pIndexData,
        u32_t indexCount,
        eRenderVertexLayout vertexLayout = eRenderVertexLayout::PositionColorUv,
        const char* debugName = nullptr);

    const RHIMeshSlice* GetSlices() const;
    u32_t GetSliceCount() const;
    bool_t IsReady() const;

    void Shutdown();

private:
    CRHIMeshResource() = default;

private:
    IRHIDevice* m_pDevice = nullptr;
    RHIMeshSlice m_Slice{};
    bool_t m_bReady = false;
};
