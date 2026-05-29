#pragma once
#include "WintersAPI.h"
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>

namespace Winters::Asset
{
	//Memory Buffer를 순차적으로 parsing하는 read-only cursor
	//zero copy - 필요하면 Peek()로 내부 포인터 반환 후 skip
	class WINTERS_ENGINE CBinaryReader
	{
	public:
		CBinaryReader(const void* pData, size_t uSize);

		template<typename T>
		T Read()
		{
			static_assert(std::is_trivially_copyable_v<T>, "POD only");
			T value{};
			ReadBytes(&value, sizeof(T));
			return value;
		}

		void ReadBytes(void* pDst, size_t uBytes);
		//zero-copy pointer return 
		const uint8_t* Peek() const { return m_pCursor; }
		void Skip(size_t uBytes);

		size_t Remaining() const { return m_pEnd - m_pCursor; }
		bool IsEOF() const { return m_pCursor >= m_pEnd; }
		//디스크에서 버퍼 적재(Decompression hook 대비 static helper)
		static bool LoadFileToMemory(const wchar_t* pPath, std::vector<uint8_t>& outBuf);

	private:
		const uint8_t* m_pBegin = nullptr;
		const uint8_t* m_pCursor = nullptr;
		const uint8_t* m_pEnd = nullptr;
	};
}