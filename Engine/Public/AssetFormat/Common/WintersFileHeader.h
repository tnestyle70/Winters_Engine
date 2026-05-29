#pragma once
#include "WintersAPI.h"
#include <cstdint>

namespace Winters::Asset
{
	//모든 .xxxx 포맷의 선두 16Bytes
	// Stage 1 SHA256 + Stage 9 Ed25519 서명은 MVP 이후 별도 커밋.
	//이게 뭔 소리임??
	constexpr char WINTERS_MAGIC[4] = { 'W', 'I', 'N', 'T' };

	enum WintersFileFlag : uint32_t
	{
		//이게 뭔 소리??
		WF_NONE = 0,
		WF_LZ4 = 1u << 0, //payload LZ4 compressed(MVP 미지원)
		WF_HAS_SHA256 = 1u << 1,
	};
#pragma pack(push, 1)
	struct WintersFileHeader
	{
		char magic[4]; //"WINT"
		uint16_t version_major;
		uint16_t version_minor;
		uint32_t flags;
		uint32_t content_size;
	};
	static_assert(sizeof(WintersFileHeader) == 16, "WintersFileHeader must be 16 bytes");
#pragma pack(pop)
}