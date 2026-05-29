#include "Shared/GameSim/Definitions/MapSpawnPoints.h"

namespace
{
    constexpr f32_t kBlueSpawnX = 27.f;
    constexpr f32_t kRedSpawnX = -27.f;
    constexpr f32_t kSpawnSlotSpacing = 3.f;
    constexpr f32_t kBlueSpawnZ = -6.f;
    constexpr f32_t kRedSpawnZ = 6.f;
    constexpr f32_t kMidLaneBotSpawnX = 127.f;
    constexpr f32_t kMidLaneBotSpawnZ = 2.f;
    constexpr f32_t kMidLaneBotSpawnSpacing = 3.f;
    constexpr f32_t kLaneGatherX = 5.f;
    constexpr f32_t kLaneTopZ = 18.f;
    constexpr f32_t kLaneMidZ = 0.f;
    constexpr f32_t kLaneBotZ = -18.f;

    bool_t IsRedTeam(u8_t team)
    {
        return team == 1;
    }

    f32_t GetLaneZ(u8_t lane)
    {
        switch (lane)
        {
        case kGameSimLaneTop:
            return kLaneTopZ;
        case kGameSimLaneBot:
            return kLaneBotZ;
        case kGameSimLaneMid:
        default:
            return kLaneMidZ;
        }
    }
}

Vec3 GetGameSimRosterSpawnPosition(u8_t slotId, u8_t team)
{
    const u8_t laneSlot = static_cast<u8_t>(slotId % 5);
    const bool_t bRed = IsRedTeam(team);

    return Vec3{
        bRed
            ? (kRedSpawnX - static_cast<f32_t>(laneSlot) * kSpawnSlotSpacing)
            : (kBlueSpawnX + static_cast<f32_t>(laneSlot) * kSpawnSlotSpacing),
        1.f,
        bRed ? kRedSpawnZ : kBlueSpawnZ
    };
}

Vec3 GetGameSimRosterSpawnPosition(u8_t slotId, u8_t team, bool_t bBot)
{
    (void)bBot;
    return GetGameSimRosterSpawnPosition(slotId, team);
}

Vec3 GetGameSimMidLaneBotSpawnPosition(u8_t slotId, u8_t team)
{
    const bool_t bRed = IsRedTeam(team);
    const u8_t teamSlot = static_cast<u8_t>(bRed && slotId >= 5
        ? slotId - 5
        : slotId % 5);
    const f32_t zOffset = kMidLaneBotSpawnZ
        + static_cast<f32_t>(teamSlot) * kMidLaneBotSpawnSpacing;

    return Vec3{
        kMidLaneBotSpawnX,
        1.f,
        bRed ? zOffset : -zOffset
    };
}

u8_t GetGameSimRosterLane(u8_t slotId)
{
    switch (slotId)
    {
    case 1:
    case 6:
        return kGameSimLaneTop;
    case 2:
    case 7:
        return kGameSimLaneMid;
    default:
        break;
    }

    return kGameSimLaneMid;
}

Vec3 GetGameSimLaneGatherPosition(u8_t lane, u8_t team)
{
    const bool_t bRed = IsRedTeam(team);
    return Vec3{
        bRed ? -kLaneGatherX : kLaneGatherX,
        1.f,
        GetLaneZ(lane)
    };
}
