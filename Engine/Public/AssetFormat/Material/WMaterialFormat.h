#pragma once
#include "WintersAPI.h"
#include <cstdint>

namespace Winters::Asset
{
    constexpr char WMAT_MAGIC[4] = { 'W', 'M', 'A', 'T' };
    constexpr uint32_t WMAT_MAX_PATH = 260;
    constexpr uint32_t WMAT_NAME_BYTES = 64;

#pragma pack(push, 1)
    struct MaterialMetaHeader
    {
        char     magic[4];
        uint32_t material_count;
    };
    static_assert(sizeof(MaterialMetaHeader) == 8, "MaterialMetaHeader must be 8 bytes");

    struct MaterialEntry
    {
        uint32_t material_index;
        uint64_t material_hash;
        char     name[WMAT_NAME_BYTES];
        wchar_t  diffuse_path[WMAT_MAX_PATH];
    };
    static_assert(sizeof(MaterialEntry) == 596, "MaterialEntry must be 596 bytes");
#pragma pack(pop)
}
