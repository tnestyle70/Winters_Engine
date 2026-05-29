#pragma once
#include "WintersAPI.h"
#include <cstdint>

namespace Winters::Asset
{
    constexpr char WMESH_MAGIC[4] = { 'W', 'M', 'S', 'H' };

    enum VertexFormatFlags : uint32_t
    {
        VF_Position = 1u << 0,
        VF_Normal = 1u << 1,
        VF_UV0 = 1u << 2,
        VF_Tangent = 1u << 3,
        VF_BoneWeight = 1u << 4,
    };

    constexpr uint32_t VF_STATIC = VF_Position | VF_Normal | VF_UV0 | VF_Tangent;
    constexpr uint32_t VF_SKINNED = VF_STATIC | VF_BoneWeight;

    // Static keeps a padded 48B stride. Skinned matches legacy VTXANIM/InputLayout:
    // pos12 + nrm12 + uv8 + tangent12 + uint4 indices16 + weights16 = 76.
    constexpr uint32_t STRIDE_STATIC = 48;
    constexpr uint32_t STRIDE_SKINNED = 76;

#pragma pack(push, 1)
    struct MeshMetaHeader
    {
        char     magic[4];
        uint32_t submesh_count;
        uint32_t bone_count;
        uint32_t vertex_format_flags;
        uint32_t vertex_stride;
        uint32_t total_vertex_count;
        uint32_t total_index_count;
        uint32_t index_stride;
        uint8_t  has_bounding;
        uint8_t  reserved[3];
    };
    static_assert(sizeof(MeshMetaHeader) == 36, "MeshMetaHeader must be 36 bytes");

    struct SubMeshDesc
    {
        uint32_t vertex_offset;
        uint32_t vertex_count;
        uint32_t index_offset;
        uint32_t index_count;
        uint32_t material_index;
        uint64_t material_hash;
        char     name[20];
    };
    static_assert(sizeof(SubMeshDesc) == 48, "SubMeshDesc must be 48 bytes");

    struct BoneEntry
    {
        uint64_t name_hash;
        char     name[32];
        int32_t  parent_index;
        float    offset_matrix[16];
        uint32_t channel_flag;
        uint8_t  reserved[16];
    };
    static_assert(sizeof(BoneEntry) == 128, "BoneEntry must be 128 bytes");

    struct SubMeshBounds
    {
        float aabb_min[3];
        float aabb_max[3];
        float sphere_center_radius[4];
    };
    static_assert(sizeof(SubMeshBounds) == 40, "SubMeshBounds must be 40 bytes");

    struct VertexSkinned
    {
        float    pos[3];
        float    nrm[3];
        float    uv[2];
        float    tan[3];
        uint32_t indices[4];
        float    weights[4];
    };
    static_assert(sizeof(VertexSkinned) == 76, "VertexSkinned must be 76 bytes");

    struct VertexStatic
    {
        float pos[3];
        float nrm[3];
        float uv[2];
        float tan[4];
    };
    static_assert(sizeof(VertexStatic) == 48, "VertexStatic must be 48 bytes");
#pragma pack(pop)
}
