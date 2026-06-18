#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "RHI/RHIHandles.h"
#include "RHI/RHITypes.h"

enum class eRHIPrimitiveTopology : u32_t
{
    TriangleList,
    TriangleStrip,
    LineList,
    PointList,
};

enum class eRHIBlendMode : u32_t
{
    Opaque,
    AlphaBlend,
    Additive,
    Premultiplied,
};

enum class eRHIDepthOp : u32_t
{
    Less,
    LessEqual,
    Greater,
    Always,
    Never,
};

enum class eRHICullMode : u32_t
{
    None,
    Front,
    Back,
};

enum class eRHILoadOp : u32_t
{
    Load,
    Clear,
    DontCare,
};

enum class eRHIStoreOp : u32_t
{
    Store,
    DontCare,
};

enum class eRHIBindingType : u32_t
{
    ConstantBuffer,
    ShaderResource,
    UnorderedAccess,
    Sampler,
};

enum class eRHIFilter : u32_t
{
    Point,
    Linear,
    Anisotropic,
};

enum class eRHIAddressMode : u32_t
{
    Wrap,
    Clamp,
    Border,
};

struct WINTERS_ENGINE RHISamplerDesc
{
    eRHIFilter filter = eRHIFilter::Linear;
    eRHIAddressMode addressU = eRHIAddressMode::Wrap;
    eRHIAddressMode addressV = eRHIAddressMode::Wrap;
    eRHIAddressMode addressW = eRHIAddressMode::Wrap;
    u32_t maxAnisotropy = 1;
    const char* debugName = nullptr;
};

enum class eRHIShaderVisibility : u32_t
{
    Vertex = 1u << 0,
    Pixel = 1u << 1,
    Compute = 1u << 2,
    All = 0xFFu,
};

struct WINTERS_ENGINE RHIInputElementDesc
{
    const char* semanticName = nullptr;
    u32_t semanticIndex = 0;
    eRHIFormat format = eRHIFormat::R32G32B32_Float;
    u32_t alignedByteOffset = 0;
    u32_t inputSlot = 0;
};

struct WINTERS_ENGINE RHIPipelineDesc
{
    RHIShaderHandle vsHandle{};
    RHIShaderHandle psHandle{};
    RHIShaderHandle csHandle{};

    RHIBindGroupLayoutHandle bindGroupLayouts[4] = {};
    u32_t bindGroupLayoutCount = 0;

    const RHIInputElementDesc* inputElements = nullptr;
    u32_t inputElementCount = 0;

    eRHIPrimitiveTopology topology = eRHIPrimitiveTopology::TriangleList;
    eRHIBlendMode blendMode = eRHIBlendMode::Opaque;
    eRHIDepthOp depthOp = eRHIDepthOp::Less;
    eRHICullMode cullMode = eRHICullMode::Back;
    bool_t depthWrite = true;

    eRHIFormat rtvFormats[8] = { eRHIFormat::R8G8B8A8_UNorm };
    u32_t rtvCount = 1;
    eRHIFormat dsvFormat = eRHIFormat::D24_UNorm_S8_UInt;

    const char* debugName = nullptr;
};

struct WINTERS_ENGINE RHIAttachmentDesc
{
    RHITextureHandle textureHandle{};
    eRHIFormat format = eRHIFormat::R8G8B8A8_UNorm;
    eRHILoadOp loadOp = eRHILoadOp::Clear;
    eRHIStoreOp storeOp = eRHIStoreOp::Store;
    f32_t clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    f32_t clearDepth = 1.0f;
    u8_t clearStencil = 0;
};

struct WINTERS_ENGINE RHIRenderPassDesc
{
    RHIAttachmentDesc colorAttachments[8];
    u32_t colorCount = 0;
    RHIAttachmentDesc depthAttachment{};
    bool_t hasDepth = false;
    const char* debugName = nullptr;
};

struct WINTERS_ENGINE RHIBindingSlot
{
    u32_t slot = 0;
    eRHIBindingType type = eRHIBindingType::ShaderResource;
    eRHIShaderVisibility visibility = eRHIShaderVisibility::All;
};

struct WINTERS_ENGINE RHIBindGroupLayoutDesc
{
    const RHIBindingSlot* slots = nullptr;
    u32_t slotCount = 0;
    const char* debugName = nullptr;
};

struct WINTERS_ENGINE RHIBindGroupResource
{
    u32_t slot = 0;
    eRHIBindingType type = eRHIBindingType::ShaderResource;
    RHIBufferHandle bufferHandle{};
    RHITextureHandle textureHandle{};
    RHISamplerHandle samplerHandle{};
};

struct WINTERS_ENGINE RHIBindGroupDesc
{
    RHIBindGroupLayoutHandle layoutHandle{};
    const RHIBindGroupResource* resources = nullptr;
    u32_t resourceCount = 0;
    const char* debugName = nullptr;
};

struct WINTERS_ENGINE RHIBufferDesc
{
    u32_t sizeBytes = 0;
    eRHIBufferUsage usage = eRHIBufferUsage::Vertex;
    eRHIMemoryUsage memoryUsage = eRHIMemoryUsage::Default;
    bool_t dynamic = false;
    const char* debugName = nullptr;
};

struct WINTERS_ENGINE RHIIndirectDrawIndexedArgs
{
    u32_t indexCountPerInstance = 0;
    u32_t instanceCount = 0;
    u32_t startIndexLocation = 0;
    i32_t baseVertexLocation = 0;
    u32_t startInstanceLocation = 0;
};

struct WINTERS_ENGINE RHITextureDesc
{
    u32_t width = 0;
    u32_t height = 0;
    u32_t depth = 1;
    u32_t mipLevels = 1;
    eRHIFormat format = eRHIFormat::R8G8B8A8_UNorm;
    u32_t usageFlags = static_cast<u32_t>(eRHITextureUsage::ShaderResource);
    const char* debugName = nullptr;
};
