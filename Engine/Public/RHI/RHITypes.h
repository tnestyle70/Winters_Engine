#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"

enum class eRHIBackend : u32_t
{
    DX11 = 0,
    DX12,
    Vulkan,
    Metal,
    Xbox,
    PS5,
};

enum class eNativeHandleType : u32_t
{
    Unknown = 0,
    DX11Device,
    DX11DeviceContext,
    DX11SwapChain,
    DX12Device,
    DX12CommandQueue,
    DX12SwapChain,
    DX12CommandList,
    DX12Resource,
    DX11Resource,
};

using eRHINativeType = eNativeHandleType;

enum class eRHIShaderStage : u32_t
{
    Vertex = 0,
    Pixel,
    Compute,
};

enum class eRHIFormat : u32_t
{
    Unknown = 0,
    R8G8B8A8_UNorm,
    R16_Float,
    R16G16B16A16_Float,
    R24G8_Typeless,
    D24_UNorm_S8_UInt,
    R24_UNorm_X8_Typeless,
    R32_Float,
    R32G32_Float,
    R32G32B32_Float,
    R32G32B32A32_Float,
    R32_UInt,
    R32G32B32A32_UInt,
};

enum class eRHIBufferUsage : u32_t
{
    Vertex = 0,
    Index,
    Constant,
    Structured,
    Raw,
    IndirectArgs,
};

enum class eRHIMemoryUsage : u32_t
{
    Default = 0,
    Immutable,
    Dynamic,
    Staging,
};

enum class eRHITextureUsage : u32_t
{
    ShaderResource = 1u << 0,
    RenderTarget = 1u << 1,
    DepthStencil = 1u << 2,
    UnorderedAccess = 1u << 3,
};

enum class eRHIResourceState : u32_t
{
    Common = 0,
    VertexConstant,
    IndexBuffer,
    RenderTarget,
    DepthRead,
    DepthWrite,
    ShaderResource,
    UnorderedAccess,
    CopyDest,
    CopySource,
    Present,
};

struct WINTERS_ENGINE RHIWindowHandle
{
    void* nativeWindow = nullptr;
    u32_t width = 0;
    u32_t height = 0;
    bool_t vsync = true;
    bool_t fullscreen = false;
};
