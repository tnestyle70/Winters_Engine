#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.h"

#include <algorithm>
#include <cmath>

namespace
{
    f32_t SanitizePositive(f32_t value, f32_t fallback)
    {
        return std::isfinite(value) && value > 0.001f ? value : fallback;
    }
}

ChampionStatsDef BuildDefaultChampionStatsDef(eChampion champion)
{
    return ChampionGameDataDB::ResolveStats(champion);
}

void RegisterDefaultChampionSkillScalingTables()
{
}

StatComponent BuildDefaultChampionStat(eChampion champion, u8_t level)
{
    return ChampionGameDataDB::BuildStat(champion, level);
}

f32_t GetDefaultChampionSkillRange(eChampion champion, u8_t slot)
{
    return ChampionGameDataDB::ResolveSkillRange(champion, slot);
}

f32_t GetDefaultChampionSkillCooldown(eChampion champion, u8_t slot)
{
    return ChampionGameDataDB::ResolveSkillCooldown(champion, slot);
}

f32_t GetDefaultChampionVisualYawOffset(eChampion champion)
{
    return ChampionGameDataDB::ResolveVisualYawOffset(champion);
}

f32_t NormalizeChampionVisualYaw(f32_t yaw)
{
    return WintersMath::NormalizeRadians(yaw);
}

f32_t MakeChampionVisualYawNear(f32_t yaw, f32_t referenceYaw)
{
    return WintersMath::NearestEquivalentRadians(yaw, referenceYaw);
}

f32_t ResolveChampionVisualYawFromDirection(eChampion champion, const Vec3& direction)
{
    return WintersMath::NormalizeRadians(WintersMath::YawFromDirectionXZ(
        direction,
        GetDefaultChampionVisualYawOffset(champion)));
}

f32_t ResolveChampionVisualYawNear(eChampion champion, const Vec3& direction, f32_t referenceYaw)
{
    return MakeChampionVisualYawNear(
        ResolveChampionVisualYawFromDirection(champion, direction),
        referenceYaw);
}

ChampionSkillTimingDefaults GetDefaultChampionSkillTiming(eChampion champion, u8_t slot)
{
    return ChampionGameDataDB::ResolveSkillTiming(champion, slot);
}

ChampionBasicAttackTimingDefaults GetDefaultChampionBasicAttackTiming(eChampion champion)
{
    return ChampionGameDataDB::ResolveBasicAttackTiming(champion);
}

ChampionSkillTimingDefaults GetDefaultChampionSkillTiming(eChampion champion, u8_t slot, u8_t stage)
{
    return ChampionGameDataDB::ResolveSkillTiming(champion, slot, stage);
}

bool_t IsDefaultChampionSkillTwoStage(eChampion champion, u8_t slot)
{
    return ChampionGameDataDB::IsSkillTwoStage(champion, slot);
}

f32_t GetDefaultChampionSkillStageWindowSec(eChampion champion, u8_t slot)
{
    return ChampionGameDataDB::ResolveSkillStageWindowSec(champion, slot);
}

u16_t EncodeSkillPlaybackRateQ8(f32_t playSpeed)
{
    const f32_t sanitized = std::clamp(SanitizePositive(playSpeed, 1.f), 0.05f, 4.f);
    const i32_t encoded = static_cast<i32_t>(std::lround(sanitized * 256.f));
    return static_cast<u16_t>(std::clamp(encoded, 1, 1024));
}

u64_t GetDefaultChampionBasicAttackWindupTicks(eChampion champion)
{
    return ChampionGameDataDB::ResolveBasicAttackWindupTicks(champion);
}

u64_t GetDefaultChampionSkillActionLockTicks(eChampion champion, u8_t slot)
{
    return ChampionGameDataDB::ResolveSkillActionLockTicks(champion, slot);
}

u64_t GetDefaultChampionSkillActionLockTicks(eChampion champion, u8_t slot, u8_t stage)
{
    return ChampionGameDataDB::ResolveSkillActionLockTicks(champion, slot, stage);
}
