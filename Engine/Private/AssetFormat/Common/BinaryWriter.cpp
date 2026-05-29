#include "AssetFormat/Common/BinaryWriter.h"
#include <fstream>
#include <cstring>

namespace Winters::Asset
{
    void CBinaryWriter::WriteBytes(const void* pSrc, size_t uBytes)
    {
        const auto oldSize = m_vBuf.size();
        m_vBuf.resize(oldSize + uBytes);
        std::memcpy(m_vBuf.data() + oldSize, pSrc, uBytes);
    }

    bool CBinaryWriter::SaveToFile(const wchar_t* pPath, uint32_t flags,
        uint16_t verMajor, uint16_t verMinor) const
    {
        WintersFileHeader hdr{};
        std::memcpy(hdr.magic, WINTERS_MAGIC, 4);
        hdr.version_major = verMajor;
        hdr.version_minor = verMinor;
        hdr.flags = flags;
        hdr.content_size = static_cast<uint32_t>(m_vBuf.size());

        std::ofstream ofs(pPath, std::ios::binary | std::ios::trunc);
        if (!ofs) return false;
        ofs.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
        ofs.write(reinterpret_cast<const char*>(m_vBuf.data()), m_vBuf.size());
        return ofs.good();
    }
}