#include "AssetFormat/Anim/WSkelWriter.h"
#include "AssetFormat/Common/BinaryWriter.h"
#include "AssetFormat/Common/Hash.h"
#include <assimp/scene.h>
#include <DirectXMath.h>
#include <functional>
#include <cstring>

using namespace Winters::Asset;
using namespace DirectX;

static XMMATRIX ConvertAndTranspose(const aiMatrix4x4& m)
{
    XMMATRIX r;
    r.r[0] = XMVectorSet(m.a1, m.b1, m.c1, m.d1);
    r.r[1] = XMVectorSet(m.a2, m.b2, m.c2, m.d2);
    r.r[2] = XMVectorSet(m.a3, m.b3, m.c3, m.d3);
    r.r[3] = XMVectorSet(m.a4, m.b4, m.c4, m.d4);
    return r;
}

bool CWSkelWriter::WriteFromAssimp(const aiScene* pScene, const wchar_t* pOutPath,
    WSkelWriteResult& outResult, const WSkelWriteOptions&)
{
    if (!pScene || !pScene->mRootNode) return false;

    std::vector<const aiNode*> nodes;
    std::vector<int32_t> parents;

    // ★ Model.cpp::LoadSkeleton 과 동일 DFS pre-order — 모든 노드 → 본
    std::function<void(const aiNode*, int32_t)> dfs = [&](const aiNode* p, int32_t parent) {
        const uint32_t idx = (uint32_t)nodes.size();
        nodes.push_back(p);
        parents.push_back(parent);
        outResult.name_to_idx[p->mName.C_Str()] = idx;
        outResult.bone_order_by_index.emplace_back(p->mName.C_Str());
        for (uint32_t i = 0; i < p->mNumChildren; ++i)
            dfs(p->mChildren[i], (int32_t)idx);
        };
    dfs(pScene->mRootNode, -1);

    std::vector<BoneNode> bones(nodes.size());
    uint64_t skelHash = 0xcbf29ce484222325ull;
    for (uint32_t i = 0; i < nodes.size(); ++i) {
        BoneNode& b = bones[i];
        const char* nm = nodes[i]->mName.C_Str();
        b.name_hash = FNV1a(nm);
        strncpy_s(b.name, sizeof(b.name), nm, _TRUNCATE);
        b.parent_index = parents[i];
        XMMATRIX m = ConvertAndTranspose(nodes[i]->mTransformation);
        std::memcpy(b.rest_transform, &m, sizeof(float) * 16);
        skelHash ^= b.name_hash;
        skelHash *= 0x100000001b3ull;
    }

    // child_count / first_child_index
    for (auto& b : bones) { b.child_count = 0; b.first_child_index = 0xFFFFFFFFu; }
    for (uint32_t i = 0; i < bones.size(); ++i) {
        int p = bones[i].parent_index;
        if (p >= 0) {
            if (bones[p].child_count == 0) bones[p].first_child_index = i;
            bones[p].child_count++;
        }
    }

    // GlobalInverseRoot
    XMMATRIX rootGlobal = ConvertAndTranspose(pScene->mRootNode->mTransformation);
    XMMATRIX rootInv = XMMatrixInverse(nullptr, rootGlobal);
    GlobalRootMatrix grm{};
    XMFLOAT4X4 fInv;
    XMStoreFloat4x4(&fInv, rootInv);
    std::memcpy(grm.global_inverse_root, &fInv, sizeof(float) * 16);

    // v4 — payload 만 Write. SaveToFile 가 WintersFileHeader 자동.
    CBinaryWriter w;
    SkelMetaHeader hdr{};
    std::memcpy(hdr.magic, WSKEL_MAGIC, 4);
    hdr.bone_count = (uint32_t)bones.size();
    hdr.socket_count = 0;
    w.Write(hdr);
    for (const auto& b : bones) w.Write(b);
    w.Write(grm);

    outResult.skel_hash = skelHash;
    return w.SaveToFile(pOutPath, WF_NONE);
}
