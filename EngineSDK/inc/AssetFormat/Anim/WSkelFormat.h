#pragma once
#include "WintersAPI.h"
#include <cstdint>

namespace Winters::Asset
{
	constexpr char WSKEL_MAGIC[4] = { 'W', 'S', 'K', 'L' };

#pragma pack(push, 1)
	struct SkelMetaHeader
	{
		char magic[4]; //4 Bytes
		uint32_t bone_count; //4 Bytes
		uint32_t socket_count; // 4Bytes
		uint32_t reserved[5]; //20 Bytes
	};
	static_assert(sizeof(SkelMetaHeader) == 32, "SkelMetaHeader == 32");

	struct BoneNode
	{
		uint64_t name_hash; //8 Bytes
		char name[64]; //64bytes
		int32_t parent_index;
		float rest_transform[16];
		uint32_t child_count;
		uint32_t first_child_index;
		uint32_t reserved[27]; //108 Bytes -> Total 256 Bytes
	};
	static_assert(sizeof(BoneNode) == 256, "BoneNode == 256");

	struct GlobalRootMatrix
	{
		float global_inverse_root[16];     //             64
		uint32_t reserved[16];             //             64  (총 128)
	};
	static_assert(sizeof(GlobalRootMatrix) == 128, "GlobalRootMatrix == 128");

	struct SocketEntry
	{
		char     name[32];             //                 32
		uint64_t name_hash;            //                  8
		int32_t  parent_bone_index;    //                  4
		float    local_offset[16];     //                 64
		uint32_t reserved[5];          // ★ v3 정정       20  (총 128)
	};
	static_assert(sizeof(SocketEntry) == 128, "SocketEntry == 128");
#pragma pack(pop)
}