#pragma once
#include "WintersAPI.h"
#include <cstddef>
#include <cstdint>

namespace Winters::Asset
{
	inline uint64_t FNV1a(const char* str)
	{
		uint64_t h = 0xcbf29ce484222325ull;
		while (str && *str)
		{
			h ^= (uint8_t)*str++;
			h *= 0x100000001b3ull;
		}
		return h;
	}
	inline uint64_t FNV1a(const void* data, size_t size)
	{
		uint64_t h = 0xcbf29ce484222325ull;
		const uint8_t* p = static_cast<const uint8_t*>(data);
		for (size_t i = 0; i < size; ++i) { h ^= p[i]; h *= 0x100000001b3ull; }
		return h;
	}
}