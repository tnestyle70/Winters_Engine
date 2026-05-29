#pragma once
#include "WintersAPI.h"
#include "AssetFormat/Material/WMaterialFormat.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Winters::Asset
{
    struct WMaterialLoaded
    {
        MaterialMetaHeader header{};
        std::vector<MaterialEntry> entries;
    };

    class WINTERS_ENGINE CWMaterialLoader
    {
    public:
        static bool Load(const wchar_t* pPath, WMaterialLoaded& out);
        static bool LoadFromMemory(const void* pData, size_t uSize, WMaterialLoaded& out);
    };
}
