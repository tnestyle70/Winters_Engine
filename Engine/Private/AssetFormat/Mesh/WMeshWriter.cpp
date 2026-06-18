#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <DirectXMath.h>

#include "AssetFormat/Mesh/WMeshWriter.h"
#include "AssetFormat/Common/BinaryWriter.h"
#include "AssetFormat/Common/Hash.h"

#include <assimp/scene.h>
#include <assimp/matrix4x4.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <vector>

using namespace DirectX;

namespace Winters::Asset
{
    static XMMATRIX ConvertAndTranspose(const aiMatrix4x4& m)
    {
        XMMATRIX r;
        r.r[0] = XMVectorSet(m.a1, m.b1, m.c1, m.d1);
        r.r[1] = XMVectorSet(m.a2, m.b2, m.c2, m.d2);
        r.r[2] = XMVectorSet(m.a3, m.b3, m.c3, m.d3);
        r.r[3] = XMVectorSet(m.a4, m.b4, m.c4, m.d4);
        return r;
    }

    static bool IsLayerOverlayNode(const aiNode* pNode)
    {
        if (!pNode)
            return false;

        const char* pName = pNode->mName.C_Str();
        return pName && std::strncmp(pName, "Layer", 5) == 0;
    }

    static uint32_t CountNodeMeshesRecursive(const aiNode* pNode)
    {
        if (!pNode)
            return 0;

        uint32_t count = pNode->mNumMeshes;
        for (uint32_t i = 0; i < pNode->mNumChildren; ++i)
            count += CountNodeMeshesRecursive(pNode->mChildren[i]);
        return count;
    }

    static void CollectMeshIndicesFromNode(const aiNode* pNode,
        const aiScene* pScene,
        std::vector<uint32_t>& outMeshIndices,
        uint32_t& outSkippedNodeCount,
        uint32_t& outSkippedMeshCount,
        bool bIncludeLayerOverlays)
    {
        if (!pNode)
            return;

        if (!bIncludeLayerOverlays && IsLayerOverlayNode(pNode))
        {
            const uint32_t skippedMeshes = CountNodeMeshesRecursive(pNode);
            outSkippedMeshCount += skippedMeshes;
            ++outSkippedNodeCount;

            char msg[256]{};
            sprintf_s(msg,
                "[WMeshWriter] SKIP overlay node: %s meshes=%u\n",
                pNode->mName.C_Str(),
                skippedMeshes);
            OutputDebugStringA(msg);
            return;
        }

        for (uint32_t i = 0; i < pNode->mNumMeshes; ++i)
        {
            const uint32_t meshIndex = pNode->mMeshes[i];
            if (meshIndex < pScene->mNumMeshes)
                outMeshIndices.push_back(meshIndex);
        }

        for (uint32_t i = 0; i < pNode->mNumChildren; ++i)
            CollectMeshIndicesFromNode(pNode->mChildren[i],
                pScene,
                outMeshIndices,
                outSkippedNodeCount,
                outSkippedMeshCount,
                bIncludeLayerOverlays);
    }

    static std::vector<uint32_t> CollectMeshIndicesForWrite(const aiScene* pScene,
        bool bIncludeLayerOverlays)
    {
        std::vector<uint32_t> meshIndices;
        if (!pScene || !pScene->mRootNode)
            return meshIndices;

        uint32_t skippedNodeCount = 0;
        uint32_t skippedMeshCount = 0;
        CollectMeshIndicesFromNode(pScene->mRootNode,
            pScene,
            meshIndices,
            skippedNodeCount,
            skippedMeshCount,
            bIncludeLayerOverlays);

        if (meshIndices.empty())
        {
            meshIndices.reserve(pScene->mNumMeshes);
            for (uint32_t i = 0; i < pScene->mNumMeshes; ++i)
                meshIndices.push_back(i);
        }

        if (skippedNodeCount > 0 || skippedMeshCount > 0)
        {
            char msg[256]{};
            sprintf_s(msg,
                "[WMeshWriter] Layer overlay filter kept=%zu skippedNodes=%u skippedMeshes=%u\n",
                meshIndices.size(),
                skippedNodeCount,
                skippedMeshCount);
            OutputDebugStringA(msg);
        }

        return meshIndices;
    }

    static bool CollectBones(const aiScene* pScene,
        const std::vector<uint32_t>& meshIndices,
        const std::unordered_map<std::string, uint32_t>* pSkelNameToIdx,
        std::unordered_map<std::string, uint32_t>& outMap,
        std::vector<BoneEntry>& outBones)
    {
        if (pSkelNameToIdx)
        {
            if (pSkelNameToIdx->size() > 1024)
            {
                OutputDebugStringA("[WMeshWriter] FATAL: skel bone_count > 1024 - bone palette limit\n");
                return false;
            }

            outBones.resize(pSkelNameToIdx->size());
            for (const auto& kv : *pSkelNameToIdx)
            {
                const uint32_t idx = kv.second;
                if (idx >= outBones.size())
                    return false;

                BoneEntry& entry = outBones[idx];
                entry.name_hash = FNV1a(kv.first.c_str());
                strncpy_s(entry.name, sizeof(entry.name), kv.first.c_str(), _TRUNCATE);
                entry.parent_index = -1;

                const XMMATRIX identity = XMMatrixIdentity();
                std::memcpy(entry.offset_matrix, &identity, sizeof(float) * 16);
                outMap[kv.first] = idx;
            }

            for (uint32_t meshIndex : meshIndices)
            {
                const aiMesh* mesh = pScene->mMeshes[meshIndex];
                for (uint32_t b = 0; b < mesh->mNumBones; ++b)
                {
                    const aiBone* bone = mesh->mBones[b];
                    const auto it = pSkelNameToIdx->find(bone->mName.C_Str());
                    if (it == pSkelNameToIdx->end())
                        continue;

                    BoneEntry& entry = outBones[it->second];
                    const XMMATRIX off = ConvertAndTranspose(bone->mOffsetMatrix);
                    std::memcpy(entry.offset_matrix, &off, sizeof(float) * 16);
                }
            }
            return true;
        }

        for (uint32_t meshIndex : meshIndices)
        {
            const aiMesh* mesh = pScene->mMeshes[meshIndex];
            for (uint32_t b = 0; b < mesh->mNumBones; ++b)
            {
                const aiBone* bone = mesh->mBones[b];
                const std::string name = bone->mName.C_Str();
                if (outMap.count(name))
                    continue;

                const uint32_t idx = static_cast<uint32_t>(outBones.size());
                outMap[name] = idx;

                BoneEntry entry{};
                entry.name_hash = FNV1a(name.c_str());
                strncpy_s(entry.name, sizeof(entry.name), name.c_str(), _TRUNCATE);
                entry.parent_index = -1;

                const XMMATRIX off = ConvertAndTranspose(bone->mOffsetMatrix);
                std::memcpy(entry.offset_matrix, &off, sizeof(float) * 16);
                outBones.push_back(entry);
            }
        }
        return true;
    }

    static SubMeshBounds ComputeBounds(const aiMesh* mesh)
    {
        SubMeshBounds b{};
        if (mesh->mNumVertices == 0)
            return b;

        XMFLOAT3 mn = { mesh->mVertices[0].x, mesh->mVertices[0].y, mesh->mVertices[0].z };
        XMFLOAT3 mx = mn;
        for (uint32_t i = 1; i < mesh->mNumVertices; ++i)
        {
            const auto& v = mesh->mVertices[i];
            mn.x = std::min(mn.x, v.x); mn.y = std::min(mn.y, v.y); mn.z = std::min(mn.z, v.z);
            mx.x = std::max(mx.x, v.x); mx.y = std::max(mx.y, v.y); mx.z = std::max(mx.z, v.z);
        }

        b.aabb_min[0] = mn.x; b.aabb_min[1] = mn.y; b.aabb_min[2] = mn.z;
        b.aabb_max[0] = mx.x; b.aabb_max[1] = mx.y; b.aabb_max[2] = mx.z;

        const XMFLOAT3 c = { (mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f, (mn.z + mx.z) * 0.5f };
        float rSq = 0.f;
        for (uint32_t i = 0; i < mesh->mNumVertices; ++i)
        {
            const auto& v = mesh->mVertices[i];
            const float dx = v.x - c.x;
            const float dy = v.y - c.y;
            const float dz = v.z - c.z;
            rSq = std::max(rSq, dx * dx + dy * dy + dz * dz);
        }

        b.sphere_center_radius[0] = c.x;
        b.sphere_center_radius[1] = c.y;
        b.sphere_center_radius[2] = c.z;
        b.sphere_center_radius[3] = std::sqrt(rSq);
        return b;
    }

    static void AppendVertices(const aiMesh* mesh, bool bSkinned,
        const WMeshWriteOptions& opt,
        const std::unordered_map<std::string, uint32_t>& boneMap,
        std::vector<uint8_t>& outBlob)
    {
        const uint32_t stride = bSkinned ? STRIDE_SKINNED : STRIDE_STATIC;
        const uint32_t startByte = static_cast<uint32_t>(outBlob.size());
        outBlob.resize(startByte + stride * mesh->mNumVertices);
        uint8_t* pDst = outBlob.data() + startByte;

        std::vector<float> weights(mesh->mNumVertices * 4, 0.f);
        std::vector<uint32_t> indices(mesh->mNumVertices * 4, 0u);
        std::vector<uint8_t> slotUsed(mesh->mNumVertices, 0u);

        if (bSkinned)
        {
            for (uint32_t b = 0; b < mesh->mNumBones; ++b)
            {
                const aiBone* bone = mesh->mBones[b];
                const auto it = boneMap.find(bone->mName.C_Str());
                if (it == boneMap.end())
                    continue;

                const uint32_t boneIdx = it->second;
                for (uint32_t w = 0; w < bone->mNumWeights; ++w)
                {
                    const uint32_t vid = bone->mWeights[w].mVertexId;
                    if (vid >= mesh->mNumVertices)
                        continue;

                    uint8_t slot = slotUsed[vid];
                    if (slot >= 4)
                        continue;

                    weights[vid * 4 + slot] = bone->mWeights[w].mWeight;
                    indices[vid * 4 + slot] = boneIdx;
                    slotUsed[vid] = slot + 1;
                }
            }
        }

        for (uint32_t v = 0; v < mesh->mNumVertices; ++v)
        {
            uint8_t* p = pDst + v * stride;

            float px = mesh->mVertices[v].x * opt.fScale;
            float py = mesh->mVertices[v].y * opt.fScale;
            float pz = mesh->mVertices[v].z * opt.fScale;
            if (opt.bMirrorX) px = -px;
            std::memcpy(p + 0, &px, 4);
            std::memcpy(p + 4, &py, 4);
            std::memcpy(p + 8, &pz, 4);

            float nx = mesh->mNormals ? mesh->mNormals[v].x : 0.f;
            float ny = mesh->mNormals ? mesh->mNormals[v].y : 1.f;
            float nz = mesh->mNormals ? mesh->mNormals[v].z : 0.f;
            if (opt.bMirrorX) nx = -nx;
            std::memcpy(p + 12, &nx, 4);
            std::memcpy(p + 16, &ny, 4);
            std::memcpy(p + 20, &nz, 4);

            const float u = mesh->mTextureCoords[0] ? mesh->mTextureCoords[0][v].x : 0.f;
            const float tv = mesh->mTextureCoords[0] ? mesh->mTextureCoords[0][v].y : 0.f;
            std::memcpy(p + 24, &u, 4);
            std::memcpy(p + 28, &tv, 4);

            float tx = mesh->mTangents ? mesh->mTangents[v].x : 1.f;
            float ty = mesh->mTangents ? mesh->mTangents[v].y : 0.f;
            float tz = mesh->mTangents ? mesh->mTangents[v].z : 0.f;
            if (opt.bMirrorX) tx = -tx;
            std::memcpy(p + 32, &tx, 4);
            std::memcpy(p + 36, &ty, 4);
            std::memcpy(p + 40, &tz, 4);

            if (bSkinned)
            {
                if (slotUsed[v] == 0)
                {
                    weights[v * 4 + 0] = 1.f;
                    indices[v * 4 + 0] = 0u;
                }

                std::memcpy(p + 44, &indices[v * 4], sizeof(uint32_t) * 4);
                std::memcpy(p + 60, &weights[v * 4], sizeof(float) * 4);
            }
            else
            {
                const float tw = 1.f;
                std::memcpy(p + 44, &tw, 4);
            }
        }
    }

    static void AppendIndices(const aiMesh* mesh, std::vector<uint32_t>& out)
    {
        for (uint32_t f = 0; f < mesh->mNumFaces; ++f)
        {
            const aiFace& face = mesh->mFaces[f];
            if (face.mNumIndices != 3)
                continue;
            out.push_back(face.mIndices[0]);
            out.push_back(face.mIndices[1]);
            out.push_back(face.mIndices[2]);
        }
    }

    bool CWMeshWriter::WriteFromAssimp(const aiScene* pScene,
        const wchar_t* pOutPath,
        const WMeshWriteOptions& opt)
    {
        if (!pScene || pScene->mNumMeshes == 0)
            return false;

        const std::vector<uint32_t> meshIndices = CollectMeshIndicesForWrite(
            pScene,
            opt.bIncludeLayerOverlays);
        if (meshIndices.empty())
            return false;

        bool bSkinned = false;
        for (uint32_t meshIndex : meshIndices)
        {
            if (pScene->mMeshes[meshIndex]->HasBones())
            {
                bSkinned = true;
                break;
            }
        }

        std::unordered_map<std::string, uint32_t> boneMap;
        std::vector<BoneEntry> bones;
        if (bSkinned && !CollectBones(pScene, meshIndices, opt.pSkelNameToIdx, boneMap, bones))
            return false;

        std::vector<uint8_t> vtxBlob;
        std::vector<uint32_t> idxBlob;
        std::vector<SubMeshDesc> subs;
        std::vector<SubMeshBounds> bounds;

        const uint32_t stride = bSkinned ? STRIDE_SKINNED : STRIDE_STATIC;
        uint32_t vtxRunningBytes = 0;

        for (uint32_t meshIndex : meshIndices)
        {
            const aiMesh* mesh = pScene->mMeshes[meshIndex];

            SubMeshDesc desc{};
            desc.vertex_offset = vtxRunningBytes;
            desc.vertex_count = mesh->mNumVertices;
            desc.index_offset = 0;
            desc.index_count = mesh->mNumFaces * 3;
            desc.material_index = mesh->mMaterialIndex;

            const aiMaterial* mat = pScene->mMaterials[mesh->mMaterialIndex];
            aiString matName;
            mat->Get(AI_MATKEY_NAME, matName);
            desc.material_hash = FNV1a(matName.C_Str());
            strncpy_s(desc.name, sizeof(desc.name), mesh->mName.C_Str(), _TRUNCATE);

            AppendVertices(mesh, bSkinned, opt, boneMap, vtxBlob);
            AppendIndices(mesh, idxBlob);

            if (opt.bWriteBounds)
                bounds.push_back(ComputeBounds(mesh));
            subs.push_back(desc);

            vtxRunningBytes += mesh->mNumVertices * stride;
        }

        const bool bIdx32 = (vtxBlob.size() / stride) > 65535;
        const uint32_t idxStride = bIdx32 ? 4u : 2u;
        uint32_t runningByte = 0;
        for (auto& s : subs)
        {
            s.index_offset = runningByte;
            runningByte += s.index_count * idxStride;
        }

        CBinaryWriter w;

        MeshMetaHeader hdr{};
        std::memcpy(hdr.magic, WMESH_MAGIC, 4);
        hdr.submesh_count = static_cast<uint32_t>(subs.size());
        hdr.bone_count = static_cast<uint32_t>(bones.size());
        hdr.vertex_format_flags = bSkinned ? VF_SKINNED : VF_STATIC;
        hdr.vertex_stride = stride;
        hdr.total_vertex_count = static_cast<uint32_t>(vtxBlob.size() / stride);
        hdr.total_index_count = static_cast<uint32_t>(idxBlob.size());
        hdr.index_stride = idxStride;
        hdr.has_bounding = opt.bWriteBounds ? 1 : 0;
        w.Write(hdr);

        for (const auto& s : subs)
            w.Write(s);
        if (!vtxBlob.empty())
            w.WriteBytes(vtxBlob.data(), vtxBlob.size());

        if (bIdx32)
        {
            w.WriteBytes(idxBlob.data(), idxBlob.size() * sizeof(uint32_t));
        }
        else
        {
            std::vector<uint16_t> idx16(idxBlob.begin(), idxBlob.end());
            w.WriteBytes(idx16.data(), idx16.size() * sizeof(uint16_t));
        }

        for (const auto& b : bones)
            w.Write(b);
        for (const auto& bb : bounds)
            w.Write(bb);

        return w.SaveToFile(pOutPath, WF_NONE);
    }
}
