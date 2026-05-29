#pragma once

#include "WintersTypes.h"

namespace Winters::Map
{
    constexpr u32_t STAGE_MAGIC = 0x47545357; // 'W','S','T','G'
    constexpr u32_t STAGE_VERSION = 4;
    constexpr u32_t STAGE_VERSION_MIN_COMPAT = 3;

    enum class eObjectKind : u32_t
    {
        Structure_Nexus = 0,
        Structure_Inhibitor = 1,
        Structure_Turret = 2,
        Jungle_Epic = 10,
        Jungle_Normal = 11,
    };

    enum class eTeam : u32_t
    {
        Blue = 0,
        Red = 1,
        Neutral = 2,
    };

    enum class eTurretTier : u32_t
    {
        Outer = 0,
        Inner = 1,
        Inhibitor = 2,
        Nexus = 3,
        None = 255,
    };

    enum class eLane : u32_t
    {
        Top = 0,
        Mid = 1,
        Bot = 2,
        Base = 3,
        None = 255,
    };

    struct StageHeader
    {
        u32_t magic = 0;
        u32_t version = 0;
        u32_t reserved[6]{};
    };

    struct StructureEntry
    {
        char name[64]{};
        u32_t subKind = 0;
        u32_t team = 0;
        u32_t tier = 0;
        u32_t lane = 0;
        f32_t px = 0.f;
        f32_t py = 0.f;
        f32_t pz = 0.f;
        f32_t rx = 0.f;
        f32_t ry = 0.f;
        f32_t rz = 0.f;
        f32_t scale = 1.f;
        u32_t bVisible = 1;
    };

    struct JungleEntry
    {
        char name[64]{};
        u32_t subKind = 0;
        u32_t campId = 0;
        f32_t px = 0.f;
        f32_t py = 0.f;
        f32_t pz = 0.f;
        f32_t rx = 0.f;
        f32_t ry = 0.f;
        f32_t rz = 0.f;
        f32_t scale = 1.f;
        u32_t bVisible = 1;
    };

    struct MinionWaypointEntry
    {
        u32_t team = 0;
        u32_t lane = 0;
        u32_t order = 0;
        f32_t px = 0.f;
        f32_t py = 0.f;
        f32_t pz = 0.f;
        u32_t reserved = 0;
    };

    static_assert(sizeof(StageHeader) == 8 * sizeof(u32_t), "StageHeader size fixed");
    static_assert(sizeof(StructureEntry) == 64 + 12 * sizeof(u32_t), "StructureEntry size fixed");
    static_assert(sizeof(JungleEntry) == 64 + 10 * sizeof(u32_t), "JungleEntry size fixed");
    static_assert(sizeof(MinionWaypointEntry) == 4 * sizeof(u32_t) + 3 * sizeof(f32_t),
        "MinionWaypointEntry size fixed");
}
