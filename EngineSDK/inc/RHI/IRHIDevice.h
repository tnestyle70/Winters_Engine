#pragma once

#include "WintersAPI.h"
#include "RHI/RHICapabilities.h"
#include "RHI/RHIDescriptors.h"
#include "RHI/IRHICommandList.h"
#include "RHI/IRHIQueue.h"
#include "RHI/IRHISwapChain.h"
#include "RHI/RHISurface.h"
#include "RHI/RHITypes.h"

class WINTERS_ENGINE IRHIDevice
{
public:
    virtual ~IRHIDevice() = default;

    virtual eRHIBackend GetBackend() const = 0;
    virtual void* GetNativeHandle(eNativeHandleType type) const = 0;
    virtual RHICapabilities GetCapabilities() const
    {
        return RHI_MakeDefaultCapabilities(GetBackend());
    }

    virtual void BeginFrame(f32_t r = 0.0f, f32_t g = 0.0f, f32_t b = 0.0f, f32_t a = 1.0f) = 0;
    virtual void EndFrame() = 0;

    virtual IRHISwapChain* CreateSwapChain(const RHIWindowHandle& window) { (void)window; return nullptr; }
    virtual IRHISwapChain* CreateSwapChain(const RHISurfaceDesc& surface)
    {
        return CreateSwapChain(RHI_ToWindowHandle(surface));
    }
    virtual IRHIQueue* GetGraphicsQueue() { return nullptr; }
    virtual IRHICommandList* CreateCommandList() { return nullptr; }
    virtual void DestroyCommandList(IRHICommandList* pCommandList) { (void)pCommandList; }
    virtual IRHICommandList* GetFrameCommandList() { return nullptr; }

    virtual RHIBufferHandle CreateBuffer(const RHIBufferDesc& desc, const void* pInitialData = nullptr)
    {
        (void)desc;
        (void)pInitialData;
        return {};
    }
    virtual void DestroyBuffer(RHIBufferHandle handle) { (void)handle; }
    virtual void* GetBufferNativeHandle(RHIBufferHandle handle, eNativeHandleType type)
    {
        (void)handle;
        (void)type;
        return nullptr;
    }

    virtual RHIShaderHandle CreateShader(eRHIShaderStage stage,
                                         const void* pBytecode,
                                         u32_t sizeBytes,
                                         const char* debugName = nullptr)
    {
        (void)stage;
        (void)pBytecode;
        (void)sizeBytes;
        (void)debugName;
        return {};
    }
    virtual void DestroyShader(RHIShaderHandle handle) { (void)handle; }

    virtual RHITextureHandle CreateTexture(const RHITextureDesc& desc,
                                           const void* pInitialData = nullptr,
                                           u32_t rowPitchBytes = 0)
    {
        (void)desc;
        (void)pInitialData;
        (void)rowPitchBytes;
        return {};
    }
    virtual void DestroyTexture(RHITextureHandle handle) { (void)handle; }
    virtual void* GetTextureNativeHandle(RHITextureHandle handle, eNativeHandleType type)
    {
        (void)handle;
        (void)type;
        return nullptr;
    }

    virtual RHISamplerHandle CreateSampler(const RHISamplerDesc& desc)
    {
        (void)desc;
        return {};
    }
    virtual void DestroySampler(RHISamplerHandle handle) { (void)handle; }

    virtual RHIPipelineHandle CreatePipeline(const RHIPipelineDesc& desc) = 0;
    virtual void DestroyPipeline(RHIPipelineHandle handle) = 0;

    virtual RHIRenderPassHandle CreateRenderPass(const RHIRenderPassDesc& desc) = 0;
    virtual void DestroyRenderPass(RHIRenderPassHandle handle) = 0;

    virtual RHIBindGroupLayoutHandle CreateBindGroupLayout(const RHIBindGroupLayoutDesc& desc) = 0;
    virtual void DestroyBindGroupLayout(RHIBindGroupLayoutHandle handle) = 0;

    virtual RHIBindGroupHandle CreateBindGroup(const RHIBindGroupDesc& desc) = 0;
    virtual void DestroyBindGroup(RHIBindGroupHandle handle) = 0;

    virtual void UpdateBindGroup(RHIBindGroupHandle handle,
                                 const RHIBindGroupResource* resources,
                                 u32_t resourceCount) = 0;
};
