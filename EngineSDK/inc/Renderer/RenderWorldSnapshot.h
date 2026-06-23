#pragma once

#include "WintersCore.h"
#include "RHI/RHIDescriptors.h"

#include <vector>

enum class eRenderVertexLayout : u8_t
{
    PositionColorUv = 0,
    PositionNormalUv,
};

struct RenderViewDesc
{
    Mat4 matView = Mat4::Identity();
    Mat4 matProjection = Mat4::Identity();
    Mat4 matViewProjection = Mat4::Identity();
    Vec3 vCameraWorld{};
    u32_t iWidth = 0;
    u32_t iHeight = 0;
};

struct RHIMeshSlice
{
    RHIBufferHandle hVertexBuffer{};
    RHIBufferHandle hIndexBuffer{};
    u32_t vertexStride = 0;
    u32_t indexCount = 0;
    u32_t firstIndex = 0;
    i32_t baseVertex = 0;
    eRenderVertexLayout vertexLayout = eRenderVertexLayout::PositionColorUv;
};

struct RenderMeshItem
{
    Mat4 matWorld = Mat4::Identity();
    RHIMeshSlice mesh{};
    RHITextureHandle hAlbedoTexture{};
    RHISamplerHandle hSampler{};
    Vec4 vTint{ 1.f, 1.f, 1.f, 1.f };
    bool_t bDepthWrite = true;
};

struct RenderFxItem
{
    Mat4 matWorld = Mat4::Identity();
    RHITextureHandle hTexture{};
    RHISamplerHandle hSampler{};
    Vec4 vTint{ 1.f, 1.f, 1.f, 1.f };
};

struct RenderDebugItem
{
    Mat4 matWorld = Mat4::Identity();
    Vec4 vColor{ 1.f, 1.f, 1.f, 1.f };
};

struct RenderWorldSnapshot
{
    RenderViewDesc view{};
    std::vector<RenderMeshItem> meshes{};
    std::vector<RenderFxItem> fx{};
    std::vector<RenderDebugItem> debug{};

    void Clear()
    {
        meshes.clear();
        fx.clear();
        debug.clear();
    }
};
