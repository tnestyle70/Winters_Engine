#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "RHI/IRHIDevice.h"

#include <memory>
#include <string>
#include <vector>

namespace Engine
{
    struct RHIFxMeshPart
    {
        RHIBufferHandle hVertexBuffer{};
        RHIBufferHandle hIndexBuffer{};
        u32_t vertexStride = 0;
        u32_t indexCount = 0;
    };

    struct RHIFxMeshResource
    {
        std::vector<RHIFxMeshPart> parts;
        RHITextureHandle hDiffuseTexture{};
        RHITextureHandle hErodeTexture{};
    };

    class WINTERS_ENGINE CRHIFxMeshResourceCache final
    {
    public:
        ~CRHIFxMeshResourceCache();

        CRHIFxMeshResourceCache(const CRHIFxMeshResourceCache&) = delete;
        CRHIFxMeshResourceCache& operator=(const CRHIFxMeshResourceCache&) = delete;

        static std::unique_ptr<CRHIFxMeshResourceCache> Create(IRHIDevice* pDevice);

        RHIFxMeshResource* LoadOrGet(
            const std::string& strFbxPath,
            const std::wstring& strDiffuseTexturePath,
            const std::wstring& strErodeTexturePath);

        RHIFxMeshResource* Find(const std::string& strFbxPath);
        RHIFxMeshResource* Find(
            const std::string& strFbxPath,
            const std::wstring& strDiffuseTexturePath,
            const std::wstring& strErodeTexturePath);

        RHITextureHandle GetDefaultTexture() const;

        void Shutdown();

    private:
        CRHIFxMeshResourceCache();

        bool_t Initialize(IRHIDevice* pDevice);

        struct Impl;
        Impl* m_pImpl = nullptr;
    };
}
