#include "AssetFormat/Common/BinaryReader.h"
#include <fstream>
#include <stdexcept>

namespace Winters::Asset
{
	CBinaryReader::CBinaryReader(const void* pData, size_t uSize)
		: m_pBegin(static_cast<const uint8_t*>(pData))
		, m_pCursor(static_cast<const uint8_t*>(pData))
		, m_pEnd(static_cast<const uint8_t*>(pData) + uSize)
	{}

    void CBinaryReader::ReadBytes(void* pDst, size_t uBytes)
    {
        if (m_pCursor + uBytes > m_pEnd)
            throw std::runtime_error("CBinaryReader: read past EOF");
        std::memcpy(pDst, m_pCursor, uBytes);
        m_pCursor += uBytes;
    }

    void CBinaryReader::Skip(size_t uBytes)
    {
        if (m_pCursor + uBytes > m_pEnd)
            throw std::runtime_error("CBinaryReader: skip past EOF");
        m_pCursor += uBytes;
    }

    bool CBinaryReader::LoadFileToMemory(const wchar_t* pPath, std::vector<uint8_t>& outBuf)
    {
        std::ifstream ifs(pPath, std::ios::binary | std::ios::ate);
        if (!ifs) return false;
        const std::streamsize size = ifs.tellg();
        ifs.seekg(0, std::ios::beg);
        outBuf.resize(static_cast<size_t>(size));
        if (!ifs.read(reinterpret_cast<char*>(outBuf.data()), size)) return false;
        return true;
    }
}