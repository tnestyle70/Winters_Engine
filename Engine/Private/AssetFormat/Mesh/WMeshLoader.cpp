#include "AssetFormat/Mesh/WMeshLoader.h"
#include "AssetFormat/Common/BinaryReader.h"
#include "AssetFormat/Common/WintersFileHeader.h"
#include <cstring>

namespace Winters::Asset
{
    // 상한선 — 악의적 파일 방어
    static constexpr uint32_t MAX_VERTICES = 10'000'000;
    static constexpr uint32_t MAX_SUBMESHES = 2048;
    static constexpr uint32_t MAX_BONES = 512;

    static bool ValidateMeta(const MeshMetaHeader& h)
    {
        if (std::memcmp(h.magic, WMESH_MAGIC, 4) != 0)            return false;
        if (h.submesh_count > MAX_SUBMESHES)                      return false;
        if (h.bone_count > MAX_BONES)                          return false;
        if (h.total_vertex_count > MAX_VERTICES)                  return false;
        if (h.index_stride != 2 && h.index_stride != 4)           return false;

        // stride 는 STRIDE_STATIC(48) 또는 STRIDE_SKINNED(76) 만 허용.
        // tangent float4(16B) 포함이라 static 48. skinned 는 weights(16)+indices(4)+reserved(8)=28 추가 → 76.
        if (h.vertex_stride != STRIDE_STATIC && h.vertex_stride != STRIDE_SKINNED)
            return false;

        // format flag 와 stride 정합성 확인
        const bool bHasBone = (h.vertex_format_flags & VF_BoneWeight) != 0;
        if (bHasBone  && h.vertex_stride != STRIDE_SKINNED) return false;
        if (!bHasBone && h.vertex_stride != STRIDE_STATIC)  return false;

        return true;
    }

    bool CWMeshLoader::Load(const wchar_t* pPath, WMeshLoaded& out)
    {
        if (!CBinaryReader::LoadFileToMemory(pPath, out.m_vRawFile)) return false;
        // m_vRawFile 수명 유지 → blob 포인터가 가리킴.
        return LoadFromMemory(out.m_vRawFile.data(), out.m_vRawFile.size(), out);
    }

    bool CWMeshLoader::LoadFromMemory(const void* pData, size_t uSize, WMeshLoaded& out)
    {
        try
        {
            CBinaryReader r(pData, uSize);

            // 1. WintersFileHeader
            const auto fh = r.Read<WintersFileHeader>();
            if (std::memcmp(fh.magic, WINTERS_MAGIC, 4) != 0)       return false;
            if (fh.version_major != 1)                                return false;
            // MVP: flags 는 WF_NONE 만 지원
            if (fh.flags != WF_NONE)                                  return false;
            if (fh.content_size > r.Remaining())                      return false;

            // 2. MeshMetaHeader
            out.header = r.Read<MeshMetaHeader>();
            if (!ValidateMeta(out.header))                            return false;

            // 3. SubMesh 테이블
            out.subMeshes.resize(out.header.submesh_count);
            if (out.header.submesh_count > 0)
                r.ReadBytes(out.subMeshes.data(),
                    sizeof(SubMeshDesc) * out.header.submesh_count);

            // 4. Vertex Block (zero-copy)
            out.uVertexBlobBytes = static_cast<size_t>(out.header.total_vertex_count) *
                out.header.vertex_stride;
            if (out.uVertexBlobBytes > r.Remaining())                 return false;
            out.pVertexBlob = r.Peek();
            r.Skip(out.uVertexBlobBytes);

            // 5. Index Block (zero-copy)
            out.uIndexBlobBytes = static_cast<size_t>(out.header.total_index_count) *
                out.header.index_stride;
            if (out.uIndexBlobBytes > r.Remaining())                  return false;
            out.pIndexBlob = r.Peek();
            r.Skip(out.uIndexBlobBytes);

            // 6. Bone 테이블
            if (out.header.bone_count > 0)
            {
                out.bones.resize(out.header.bone_count);
                r.ReadBytes(out.bones.data(), sizeof(BoneEntry) * out.header.bone_count);

                // parent_index 검증
                for (const auto& b : out.bones)
                    if (b.parent_index >= static_cast<int32_t>(out.header.bone_count))
                        return false;
            }

            // 7. Bounds (옵션)
            if (out.header.has_bounding)
            {
                out.bounds.resize(out.header.submesh_count);
                r.ReadBytes(out.bounds.data(),
                    sizeof(SubMeshBounds) * out.header.submesh_count);
            }

            return true;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }
}
