#pragma once
#include "WintersAPI.h"
#include <cstdint>

namespace Winters::Asset
{
    constexpr char WANIM_MAGIC[4] = { 'W','A','N','M' };

    enum class eAnimEventType : uint16_t
    {
        HitStart = 0,
        HitEnd = 1,
        Footstep = 2,
        SFX = 3,
        VfxSpawn = 4,
        DamageNumber = 5,
        SkillCast = 6,
        SkillRelease = 7,
        Custom = 0xFFFF,
    };

#pragma pack(push, 1)
    struct AnimMetaHeader
    {
        char     magic[4];
        uint32_t channel_count;
        float    duration_ticks;
        float    ticks_per_second;
        uint32_t total_key_count;
        uint32_t event_count;
        uint8_t  is_loop;
        uint8_t  reserved[7];
    };
    static_assert(sizeof(AnimMetaHeader) == 32, "AnimMetaHeader must be 32 bytes");

    struct AnimChannel
    {
        uint64_t bone_name_hash;
        uint32_t pos_key_count;
        uint32_t pos_offset;
        uint32_t rot_key_count;
        uint32_t rot_offset;
        uint32_t scl_key_count;
        uint32_t scl_offset;
        int32_t  bone_index_cached;
        uint32_t reserved;
    };
    static_assert(sizeof(AnimChannel) == 40, "AnimChannel must be 40 bytes");

    struct VectorKey
    {
        float time_ticks;
        float x, y, z;
    };
    static_assert(sizeof(VectorKey) == 16, "VectorKey must be 16 bytes");

    struct QuatKey
    {
        float time_ticks;
        float x, y, z, w;
    };
    static_assert(sizeof(QuatKey) == 20, "QuatKey must be 20 bytes");

    struct AnimEvent
    {
        float    time_ticks;
        uint16_t type;
        uint16_t reserved0;
        uint32_t skill_id;
        uint32_t param_u32;
        float    param_f32;
        uint64_t string_hash;
        uint32_t reserved1;
    };
    static_assert(sizeof(AnimEvent) == 32, "AnimEvent must be 32 bytes");

    struct WAnimTrailer
    {
        uint64_t skel_hash;
    };
#pragma pack(pop)
}
