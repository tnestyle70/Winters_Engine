#include "WintersPCH.h"

#include "Renderer/RHIFxMeshResource.h"

#include "AssetFormat/Mesh/WMeshLoader.h"
#include "RHI/RHITextureLoader.h"
#include "WintersPaths.h"

#include <Windows.h>
#include <cstring>
#include <filesystem>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Engine
{
    namespace
    {
        std::string ReplaceExtToWMesh(const std::string& strPath)
        {
            const size_t dot = strPath.find_last_of('.');
            if (dot == std::string::npos)
                return strPath + ".wmesh";
            return strPath.substr(0, dot) + ".wmesh";
        }

        std::wstring ToWidePath(const std::string& strPath)
        {
            return std::wstring(strPath.begin(), strPath.end());
        }

        std::string ToNarrowPath(const std::wstring& strPath)
        {
            std::string strResult;
            strResult.reserve(strPath.size());
            for (wchar_t ch : strPath)
                strResult.push_back(static_cast<char>(ch));
            return strResult;
        }

        std::wstring ResolveContentPathOrInput(const std::string& strPath)
        {
            const std::wstring wPath = ToWidePath(strPath);
            wchar_t fullPath[MAX_PATH] = {};
            if (WintersResolveContentPath(wPath.c_str(), fullPath, MAX_PATH))
                return fullPath;
            return wPath;
        }

        std::wstring MakeMeshKey(
            const std::string& strCookedPath,
            const std::wstring& strDiffuseTexturePath,
            const std::wstring& strErodeTexturePath)
        {
            std::wstring key(strCookedPath.begin(), strCookedPath.end());
            key += L"\nD:";
            key += strDiffuseTexturePath;
            key += L"\nE:";
            key += strErodeTexturePath;
            return key;
        }

        RHITextureHandle CreateDefaultTexture(IRHIDevice* pDevice)
        {
            if (!pDevice)
                return {};

            const u32_t whitePixel = 0xFFFFFFFFu;

            RHITextureDesc desc{};
            desc.width = 1;
            desc.height = 1;
            desc.format = eRHIFormat::R8G8B8A8_UNorm;
            desc.usageFlags = static_cast<u32_t>(eRHITextureUsage::ShaderResource);
            desc.debugName = "RHIFxMeshDefaultTexture";

            return pDevice->CreateTexture(desc, &whitePixel, sizeof(whitePixel));
        }

        void DestroyMeshBuffers(IRHIDevice* pDevice, RHIFxMeshResource& resource)
        {
            if (!pDevice)
                return;

            for (RHIFxMeshPart& part : resource.parts)
            {
                if (part.hVertexBuffer.IsValid())
                    pDevice->DestroyBuffer(part.hVertexBuffer);

                if (part.hIndexBuffer.IsValid())
                    pDevice->DestroyBuffer(part.hIndexBuffer);

                part.hVertexBuffer = {};
                part.hIndexBuffer = {};
            }

            resource.parts.clear();
            resource.hDiffuseTexture = {};
            resource.hErodeTexture = {};
        }

        bool CopySubmeshIndices(
            const Winters::Asset::WMeshLoaded& mesh,
            const Winters::Asset::SubMeshDesc& submesh,
            std::vector<u32_t>& outIndices)
        {
            outIndices.clear();
            outIndices.resize(submesh.index_count);

            const uint8_t* pSrc = mesh.pIndexBlob + submesh.index_offset;
            if (mesh.header.index_stride == 4)
            {
                std::memcpy(outIndices.data(), pSrc, submesh.index_count * sizeof(u32_t));
                return true;
            }

            if (mesh.header.index_stride == 2)
            {
                const uint16_t* pSrc16 = reinterpret_cast<const uint16_t*>(pSrc);
                for (u32_t i = 0; i < submesh.index_count; ++i)
                    outIndices[i] = pSrc16[i];
                return true;
            }

            return false;
        }
    }

    struct CRHIFxMeshResourceCache::Impl
    {
        IRHIDevice* pDevice = nullptr;
        RHITextureHandle hDefaultTexture{};

        std::unordered_map<std::wstring, RHIFxMeshResource> mapMeshes;
        std::unordered_map<std::string, std::wstring> mapFirstKeyByFbx;
        std::unordered_map<std::wstring, RHITextureHandle> mapTextures;

        RHITextureHandle GetOrLoadTexture(const std::wstring& strPath)
        {
            if (!pDevice)
                return {};

            if (strPath.empty())
                return hDefaultTexture;

            auto it = mapTextures.find(strPath);
            if (it != mapTextures.end())
                return it->second;

            RHITextureHandle hTexture = RHI_CreateTextureFromFile(
                pDevice,
                strPath.c_str(),
                "RHIFxMeshTexture");

            if (!hTexture.IsValid())
            {
                ::OutputDebugStringW(
                    (L"[CRHIFxMeshResourceCache] Texture load fail: " + strPath + L"\n").c_str());

                return hDefaultTexture;
            }

            mapTextures.emplace(strPath, hTexture);
            return hTexture;
        }
    };

    CRHIFxMeshResourceCache::CRHIFxMeshResourceCache() = default;

    CRHIFxMeshResourceCache::~CRHIFxMeshResourceCache()
    {
        Shutdown();
        delete m_pImpl;
        m_pImpl = nullptr;
    }

    std::unique_ptr<CRHIFxMeshResourceCache> CRHIFxMeshResourceCache::Create(IRHIDevice* pDevice)
    {
        auto pInstance = std::unique_ptr<CRHIFxMeshResourceCache>(new CRHIFxMeshResourceCache());
        if (!pInstance)
            return nullptr;

        if (!pInstance->Initialize(pDevice))
            return nullptr;

        return pInstance;
    }

    bool_t CRHIFxMeshResourceCache::Initialize(IRHIDevice* pDevice)
    {
        if (!pDevice || pDevice->GetBackend() != eRHIBackend::DX12)
            return false;

        m_pImpl = new Impl();
        if (!m_pImpl)
            return false;

        m_pImpl->pDevice = pDevice;
        m_pImpl->hDefaultTexture = CreateDefaultTexture(pDevice);

        return m_pImpl->hDefaultTexture.IsValid();
    }

    RHIFxMeshResource* CRHIFxMeshResourceCache::LoadOrGet(
        const std::string& strFbxPath,
        const std::wstring& strDiffuseTexturePath,
        const std::wstring& strErodeTexturePath)
    {
        if (!m_pImpl || !m_pImpl->pDevice || strFbxPath.empty())
            return nullptr;

        const std::string strCookedPath = ReplaceExtToWMesh(strFbxPath);
        const std::wstring resolvedWMeshPath = ResolveContentPathOrInput(strCookedPath);
        const std::string strResolvedCookedPath = ToNarrowPath(resolvedWMeshPath);
        const std::wstring strMeshKey = MakeMeshKey(
            strResolvedCookedPath,
            strDiffuseTexturePath,
            strErodeTexturePath);

        auto it = m_pImpl->mapMeshes.find(strMeshKey);
        if (it != m_pImpl->mapMeshes.end())
            return &it->second;

        Winters::Asset::WMeshLoaded loadedMesh;
        if (!std::filesystem::exists(resolvedWMeshPath) ||
            !Winters::Asset::CWMeshLoader::Load(resolvedWMeshPath.c_str(), loadedMesh))
        {
            ::OutputDebugStringA(
                ("[CRHIFxMeshResourceCache] .wmesh load fail: " + strCookedPath + "\n").c_str());
            return nullptr;
        }

        RHIFxMeshResource resource{};
        resource.hDiffuseTexture = m_pImpl->GetOrLoadTexture(strDiffuseTexturePath);
        resource.hErodeTexture = m_pImpl->GetOrLoadTexture(strErodeTexturePath);
        resource.parts.reserve(loadedMesh.subMeshes.size());

        for (const auto& submesh : loadedMesh.subMeshes)
        {
            if (submesh.vertex_count == 0 || submesh.index_count == 0)
                continue;

            const uint8_t* pVertices = loadedMesh.pVertexBlob + submesh.vertex_offset;
            const u32_t vertexBytes = submesh.vertex_count * loadedMesh.header.vertex_stride;

            std::vector<u32_t> indices;
            if (!CopySubmeshIndices(loadedMesh, submesh, indices) || indices.empty())
                continue;

            RHIBufferDesc vbDesc{};
            vbDesc.sizeBytes = vertexBytes;
            vbDesc.usage = eRHIBufferUsage::Vertex;
            vbDesc.memoryUsage = eRHIMemoryUsage::Dynamic;
            vbDesc.dynamic = true;
            vbDesc.debugName = "RHIFxMeshVB";

            RHIBufferDesc ibDesc{};
            ibDesc.sizeBytes = static_cast<u32_t>(indices.size() * sizeof(u32_t));
            ibDesc.usage = eRHIBufferUsage::Index;
            ibDesc.memoryUsage = eRHIMemoryUsage::Dynamic;
            ibDesc.dynamic = true;
            ibDesc.debugName = "RHIFxMeshIB";

            RHIFxMeshPart part{};
            part.hVertexBuffer = m_pImpl->pDevice->CreateBuffer(vbDesc, pVertices);
            part.hIndexBuffer = m_pImpl->pDevice->CreateBuffer(ibDesc, indices.data());
            part.vertexStride = loadedMesh.header.vertex_stride;
            part.indexCount = static_cast<u32_t>(indices.size());

            if (!part.hVertexBuffer.IsValid() || !part.hIndexBuffer.IsValid())
            {
                DestroyMeshBuffers(m_pImpl->pDevice, resource);
                return nullptr;
            }

            resource.parts.push_back(part);
        }

        if (resource.parts.empty())
        {
            DestroyMeshBuffers(m_pImpl->pDevice, resource);
            ::OutputDebugStringA(
                ("[CRHIFxMeshResourceCache] .wmesh has no drawable mesh: "
                    + strCookedPath + "\n").c_str());
            return nullptr;
        }

        auto inserted = m_pImpl->mapMeshes.emplace(strMeshKey, std::move(resource));
        if (m_pImpl->mapFirstKeyByFbx.find(strFbxPath) == m_pImpl->mapFirstKeyByFbx.end())
            m_pImpl->mapFirstKeyByFbx.emplace(strFbxPath, strMeshKey);
        if (m_pImpl->mapFirstKeyByFbx.find(strCookedPath) == m_pImpl->mapFirstKeyByFbx.end())
            m_pImpl->mapFirstKeyByFbx.emplace(strCookedPath, strMeshKey);

        ::OutputDebugStringA(
            ("[CRHIFxMeshResourceCache] .wmesh loaded: " + strCookedPath + "\n").c_str());

        return &inserted.first->second;
    }

    RHIFxMeshResource* CRHIFxMeshResourceCache::Find(const std::string& strFbxPath)
    {
        if (!m_pImpl)
            return nullptr;

        auto keyIt = m_pImpl->mapFirstKeyByFbx.find(strFbxPath);
        if (keyIt == m_pImpl->mapFirstKeyByFbx.end())
        {
            const std::string strCookedPath = ReplaceExtToWMesh(strFbxPath);
            keyIt = m_pImpl->mapFirstKeyByFbx.find(strCookedPath);
        }
        if (keyIt == m_pImpl->mapFirstKeyByFbx.end())
            return nullptr;

        auto it = m_pImpl->mapMeshes.find(keyIt->second);
        if (it == m_pImpl->mapMeshes.end())
            return nullptr;

        return &it->second;
    }

    RHIFxMeshResource* CRHIFxMeshResourceCache::Find(
        const std::string& strFbxPath,
        const std::wstring& strDiffuseTexturePath,
        const std::wstring& strErodeTexturePath)
    {
        if (!m_pImpl)
            return nullptr;

        const std::string strCookedPath = ReplaceExtToWMesh(strFbxPath);
        const std::wstring resolvedWMeshPath = ResolveContentPathOrInput(strCookedPath);
        const std::wstring strMeshKey = MakeMeshKey(
            ToNarrowPath(resolvedWMeshPath),
            strDiffuseTexturePath,
            strErodeTexturePath);

        auto it = m_pImpl->mapMeshes.find(strMeshKey);
        if (it == m_pImpl->mapMeshes.end())
            return nullptr;

        return &it->second;
    }

    RHITextureHandle CRHIFxMeshResourceCache::GetDefaultTexture() const
    {
        if (!m_pImpl)
            return {};

        return m_pImpl->hDefaultTexture;
    }

    void CRHIFxMeshResourceCache::Shutdown()
    {
        if (!m_pImpl)
            return;

        IRHIDevice* pDevice = m_pImpl->pDevice;

        for (auto& pair : m_pImpl->mapMeshes)
            DestroyMeshBuffers(pDevice, pair.second);

        m_pImpl->mapMeshes.clear();
        m_pImpl->mapFirstKeyByFbx.clear();

        if (pDevice)
        {
            for (auto& pair : m_pImpl->mapTextures)
            {
                if (pair.second.IsValid())
                    pDevice->DestroyTexture(pair.second);
            }

            if (m_pImpl->hDefaultTexture.IsValid())
                pDevice->DestroyTexture(m_pImpl->hDefaultTexture);
        }

        m_pImpl->mapTextures.clear();
        m_pImpl->hDefaultTexture = {};
        m_pImpl->pDevice = nullptr;
    }
}
