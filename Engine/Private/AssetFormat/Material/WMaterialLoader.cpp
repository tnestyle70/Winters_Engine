#include "AssetFormat/Material/WMaterialLoader.h"
#include "AssetFormat/Common/BinaryReader.h"
#include "AssetFormat/Common/WintersFileHeader.h"

#include <cstring>
#include <exception>
#include <vector>

namespace Winters::Asset
{
    static constexpr uint32_t MAX_MATERIALS = 4096;

    static bool ValidateHeader(const MaterialMetaHeader& h)
    {
        if (std::memcmp(h.magic, WMAT_MAGIC, 4) != 0)
            return false;
        if (h.material_count > MAX_MATERIALS)
            return false;
        return true;
    }

    bool CWMaterialLoader::Load(const wchar_t* pPath, WMaterialLoaded& out)
    {
        std::vector<uint8_t> raw;
        if (!CBinaryReader::LoadFileToMemory(pPath, raw))
            return false;
        return LoadFromMemory(raw.data(), raw.size(), out);
    }

    bool CWMaterialLoader::LoadFromMemory(const void* pData, size_t uSize, WMaterialLoaded& out)
    {
        try
        {
            CBinaryReader r(pData, uSize);

            const auto fh = r.Read<WintersFileHeader>();
            if (std::memcmp(fh.magic, WINTERS_MAGIC, 4) != 0)
                return false;
            if (fh.version_major != 1)
                return false;
            if (fh.flags != WF_NONE)
                return false;
            if (fh.content_size > r.Remaining())
                return false;

            out.header = r.Read<MaterialMetaHeader>();
            if (!ValidateHeader(out.header))
                return false;

            out.entries.resize(out.header.material_count);
            if (out.header.material_count > 0)
            {
                const size_t bytes = sizeof(MaterialEntry) * out.header.material_count;
                if (bytes > r.Remaining())
                    return false;
                r.ReadBytes(out.entries.data(), bytes);
            }

            return true;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }
}
