#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "RHI/RHIDescriptors.h"

struct WINTERS_ENGINE RHICommandListDiagnostics
{
    u64_t emittedResourceBarrierCount = 0;
    u32_t validationErrorCount = 0;
    bool_t validationAvailable = false;
};

class WINTERS_ENGINE IRHICommandList
{
public:
    virtual ~IRHICommandList() = default;

    virtual void Begin() = 0;
    virtual void End() = 0;
    virtual RHICommandListDiagnostics GetDiagnostics() const = 0;

    virtual void BeginRenderPass(RHIRenderPassHandle handle) = 0;
    virtual void EndRenderPass() = 0;

    virtual void SetPipeline(RHIPipelineHandle handle) = 0;
    virtual void SetBindGroup(u32_t slot, RHIBindGroupHandle handle) = 0;

    virtual void SetVertexBuffer(u32_t slot, RHIBufferHandle handle, u32_t stride, u32_t offset) = 0;
    virtual void SetIndexBuffer(RHIBufferHandle handle, u32_t offset, eRHIFormat indexFormat) = 0;

    virtual void Draw(u32_t vertexCount, u32_t instanceCount, u32_t firstVertex, u32_t firstInstance) = 0;
    virtual void DrawIndexed(u32_t indexCount, u32_t instanceCount, u32_t firstIndex, i32_t baseVertex, u32_t firstInstance) = 0;
    virtual void DrawIndexedIndirect(RHIBufferHandle argsHandle, u32_t alignedByteOffset)
    {
        (void)argsHandle;
        (void)alignedByteOffset;
    }
    virtual void Dispatch(u32_t x, u32_t y, u32_t z) = 0;

    virtual void UpdateBuffer(RHIBufferHandle handle, const void* pData, u32_t sizeBytes) = 0;
    virtual bool_t TransitionResource(RHIBufferHandle handle, eRHIResourceState newState) = 0;
    virtual bool_t TransitionResource(RHITextureHandle handle, eRHIResourceState newState) = 0;

    virtual void* GetNativeHandle(eNativeHandleType type) const = 0;
};
