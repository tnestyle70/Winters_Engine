#pragma once
#include "WintersAPI.h"
#include "AssetFormat/Common/WintersFileHeader.h"
#include <cstdint>
#include <vector>

namespace Winters::Asset
{
	//append-only buffer. SaveToFile에서 WintersFileHeader 16B를 선두에 쓴 뒤 payload flush
	class WINTERS_ENGINE CBinaryWriter
	{
	public:
		template<typename T>
		void Write(const T& value)
		{
			static_assert(std::is_trivially_copyable_v<T>, "POD only");
			WriteBytes(&value, sizeof(T));
		}
		void WriteBytes(const void* pSrc, size_t uBytes);

		size_t GetPayloadSize() const { return m_vBuf.size(); }
		//WintersFileFlag bitmask MVP는 WF_NONE
		bool SaveToFile(const wchar_t* pPath, uint32_t flags = WF_NONE,
			uint16_t verMajor = 1, uint16_t verMinor = 0) const;
	private:
		std::vector<uint8_t> m_vBuf{};
	};
}