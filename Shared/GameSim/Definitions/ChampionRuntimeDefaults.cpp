#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Core/Determinism/DeterministicTime.h"

#include <algorithm>
#include <cmath>

namespace
{
    struct ChampionSkillTimingRow
    {
        eChampion champion = eChampion::NONE;
        u8_t slot = 0;
        u8_t stage = 1;
        f32_t lockDurationSec = 0.6f;
        f32_t animPlaySpeed = 1.f;
    };

    struct ChampionSkillStageRow
    {
        eChampion champion = eChampion::NONE;
        u8_t slot = 0;
        u8_t stageCount = 1;
        f32_t stageWindowSec = 0.f;
    };

    struct ChampionSkillValueRow
    {
        eChampion champion = eChampion::NONE;
        u8_t slot = 0;
        f32_t cooldownSec = 1.f;
        f32_t rangeMax = 0.f;
    };

    // Keep false for normal server-authoritative gameplay. Turning this on
    // intentionally overrides authored cooldowns for fast verification sessions.
    constexpr bool_t kUseFastSkillVerificationCooldown = false;
    constexpr f32_t kVerificationSkillCooldownSec = 0.20f;

    constexpr ChampionSkillTimingRow kChampionSkillTimingTable[] =
    {
        { eChampion::IRELIA, 0, 1, 0.46f, 1.25f },
        { eChampion::IRELIA, 1, 1, 0.36f, 1.20f },
        { eChampion::IRELIA, 2, 1, 5.00f, 1.00f },
        { eChampion::IRELIA, 2, 2, 0.40f, 1.00f },
        { eChampion::IRELIA, 3, 1, 0.90f, 1.05f },
        { eChampion::IRELIA, 3, 2, 0.45f, 1.05f },
        { eChampion::IRELIA, 4, 1, 0.65f, 1.00f },

        { eChampion::YASUO, 0, 1, 0.50f, 0.85f },
        { eChampion::YASUO, 1, 1, 0.50f, 0.85f },
        { eChampion::YASUO, 2, 1, 0.25f, 1.00f },
        { eChampion::YASUO, 3, 1, 0.40f, 1.00f },
        { eChampion::YASUO, 4, 1, 0.60f, 1.00f },

        { eChampion::KALISTA, 0, 1, 0.60f, 1.00f },
        { eChampion::KALISTA, 1, 1, 0.30f, 2.80f },
        { eChampion::KALISTA, 2, 1, 0.50f, 1.00f },
        { eChampion::KALISTA, 3, 1, 0.40f, 1.00f },
        { eChampion::KALISTA, 4, 1, 0.50f, 1.00f },

        { eChampion::GAREN, 0, 1, 1.00f, 1.00f },
        { eChampion::GAREN, 1, 1, 0.60f, 1.00f },
        { eChampion::GAREN, 2, 1, 0.50f, 1.00f },
        { eChampion::GAREN, 3, 1, 3.00f, 1.00f },
        { eChampion::GAREN, 4, 1, 1.50f, 1.00f },

        { eChampion::ZED, 0, 1, 1.00f, 1.00f },
        { eChampion::ZED, 1, 1, 0.70f, 1.00f },
        { eChampion::ZED, 2, 1, 0.50f, 1.00f },
        { eChampion::ZED, 2, 2, 0.25f, 1.00f },
        { eChampion::ZED, 3, 1, 0.60f, 1.00f },
        { eChampion::ZED, 4, 1, 1.50f, 1.00f },

        { eChampion::RIVEN, 0, 1, 1.00f, 1.00f },
        { eChampion::RIVEN, 1, 1, 0.45f, 1.00f },
        { eChampion::RIVEN, 2, 1, 0.60f, 1.00f },
        { eChampion::RIVEN, 3, 1, 0.50f, 1.00f },
        { eChampion::RIVEN, 4, 1, 0.80f, 1.00f },

        { eChampion::EZREAL, 0, 1, 0.65f, 1.00f },
        { eChampion::EZREAL, 1, 1, 0.50f, 1.00f },
        { eChampion::EZREAL, 2, 1, 0.50f, 1.00f },
        { eChampion::EZREAL, 3, 1, 0.60f, 1.00f },
        { eChampion::EZREAL, 4, 1, 1.00f, 1.00f },

        { eChampion::FIORA, 0, 1, 1.00f, 1.00f },
        { eChampion::FIORA, 1, 1, 0.50f, 1.00f },
        { eChampion::FIORA, 2, 1, 1.50f, 1.00f },
        { eChampion::FIORA, 3, 1, 0.40f, 1.00f },
        { eChampion::FIORA, 4, 1, 2.00f, 1.00f },

        { eChampion::JAX, 0, 1, 1.00f, 1.00f },
        { eChampion::JAX, 1, 1, 0.60f, 1.00f },
        { eChampion::JAX, 2, 1, 0.50f, 1.00f },
        { eChampion::JAX, 3, 1, 0.70f, 1.00f },
        { eChampion::JAX, 4, 1, 0.60f, 1.00f },

        { eChampion::LEESIN, 0, 1, 0.60f, 1.00f },
        { eChampion::LEESIN, 1, 1, 0.60f, 1.00f },
        { eChampion::LEESIN, 1, 2, 0.60f, 1.00f },
        { eChampion::LEESIN, 2, 1, 0.60f, 1.00f },
        { eChampion::LEESIN, 2, 2, 0.45f, 1.00f },
        { eChampion::LEESIN, 3, 1, 0.60f, 1.00f },
        { eChampion::LEESIN, 3, 2, 0.45f, 1.00f },
        { eChampion::LEESIN, 4, 1, 0.60f, 1.00f },

        { eChampion::KINDRED, 0, 1, 0.60f, 1.00f },
        { eChampion::KINDRED, 1, 1, 0.60f, 1.00f },
        { eChampion::KINDRED, 2, 1, 0.60f, 1.00f },
        { eChampion::KINDRED, 3, 1, 0.60f, 1.00f },
        { eChampion::KINDRED, 4, 1, 0.60f, 1.00f },

        { eChampion::MASTERYI, 0, 1, 0.60f, 1.00f },
        { eChampion::MASTERYI, 1, 1, 0.60f, 1.00f },
        { eChampion::MASTERYI, 2, 1, 0.60f, 1.00f },
        { eChampion::MASTERYI, 3, 1, 0.60f, 1.00f },
        { eChampion::MASTERYI, 4, 1, 0.60f, 1.00f },

        { eChampion::ANNIE, 0, 1, 0.80f, 1.00f },
        { eChampion::ANNIE, 1, 1, 0.50f, 1.00f },
        { eChampion::ANNIE, 2, 1, 0.60f, 1.00f },
        { eChampion::ANNIE, 3, 1, 0.40f, 1.00f },
        { eChampion::ANNIE, 4, 1, 1.20f, 1.00f },

        { eChampion::ASHE, 0, 1, 0.70f, 1.00f },
        { eChampion::ASHE, 1, 1, 0.50f, 1.00f },
        { eChampion::ASHE, 2, 1, 0.60f, 1.00f },
        { eChampion::ASHE, 3, 1, 0.50f, 1.00f },
        { eChampion::ASHE, 4, 1, 1.00f, 1.00f },

        { eChampion::VIEGO, 0, 1, 0.75f, 1.00f },
        { eChampion::VIEGO, 1, 1, 0.60f, 1.00f },
        { eChampion::VIEGO, 2, 1, 0.70f, 1.00f },
        { eChampion::VIEGO, 2, 2, 0.3f, 1.f},
        { eChampion::VIEGO, 3, 1, 0.75f, 1.00f },
        { eChampion::VIEGO, 4, 1, 0.80f, 1.00f },

        { eChampion::YONE, 0, 1, 0.90f, 0.85f },
        { eChampion::YONE, 1, 1, 0.90f, 0.85f },
        { eChampion::YONE, 2, 1, 0.90f, 0.85f },
        { eChampion::YONE, 3, 1, 0.75f, 1.00f },
        { eChampion::YONE, 4, 1, 1.20f, 1.00f },

        { eChampion::SYLAS, 0, 1, 0.60f, 1.00f },
        { eChampion::SYLAS, 1, 1, 0.55f, 1.00f },
        { eChampion::SYLAS, 2, 1, 0.45f, 1.00f },
        { eChampion::SYLAS, 3, 1, 0.35f, 1.00f },
        { eChampion::SYLAS, 3, 2, 0.50f, 1.00f },
        { eChampion::SYLAS, 4, 1, 0.80f, 1.00f },
    };

    constexpr ChampionSkillStageRow kChampionSkillStageTable[] =
    {
        { eChampion::IRELIA, 2, 2, 4.00f },
        { eChampion::IRELIA, 3, 2, 3.50f },
        { eChampion::VIEGO, 2, 2, 4.f},
        { eChampion::ZED, 2, 2, 5.f },
        { eChampion::LEESIN, 1, 2, 3.00f },
        { eChampion::LEESIN, 2, 2, 3.00f },
        { eChampion::LEESIN, 3, 2, 3.00f },
        { eChampion::SYLAS, 3, 2, 3.00f },
    };

    constexpr ChampionSkillValueRow kChampionSkillValueTable[] =
    {
        { eChampion::IRELIA, 0, 0.60f, 2.10f },
        { eChampion::IRELIA, 1, 0.60f, 6.00f },
        { eChampion::IRELIA, 2, 0.60f, 0.00f },
        { eChampion::IRELIA, 3, 0.60f, 9.00f },
        { eChampion::IRELIA, 4, 0.60f, 12.00f },

        { eChampion::YASUO, 0, 0.60f, 2.50f },
        { eChampion::YASUO, 1, 0.60f, 5.00f },
        { eChampion::YASUO, 2, 0.60f, 4.00f },
        { eChampion::YASUO, 3, 0.60f, 4.75f },
        { eChampion::YASUO, 4, 0.60f, 14.00f },

        { eChampion::KALISTA, 0, 0.50f, 5.50f },
        { eChampion::KALISTA, 1, 0.20f, 11.00f },
        { eChampion::KALISTA, 2, 18.00f, 0.00f },
        { eChampion::KALISTA, 3, 3.00f, 12.00f },
        { eChampion::KALISTA, 4, 120.00f, 0.00f },

        { eChampion::GAREN, 0, 0.60f, 1.50f },
        { eChampion::GAREN, 1, 2.00f, 0.00f },
        { eChampion::GAREN, 2, 2.00f, 0.00f },
        { eChampion::GAREN, 3, 2.00f, 1.65f },
        { eChampion::GAREN, 4, 2.00f, 4.00f },

        { eChampion::ZED, 0, 0.50f, 1.50f },
        { eChampion::ZED, 1, 2.00f, 9.00f },
        { eChampion::ZED, 2, 2.00f, 6.50f },
        { eChampion::ZED, 3, 2.00f, 2.50f },
        { eChampion::ZED, 4, 2.00f, 6.25f },

        { eChampion::RIVEN, 0, 0.60f, 1.50f },
        { eChampion::RIVEN, 1, 1.50f, 4.50f },
        { eChampion::RIVEN, 2, 2.00f, 0.00f },
        { eChampion::RIVEN, 3, 2.00f, 0.00f },
        { eChampion::RIVEN, 4, 2.00f, 0.00f },

        { eChampion::EZREAL, 0, 0.50f, 5.50f },
        { eChampion::EZREAL, 1, 0.20f, 11.00f },
        { eChampion::EZREAL, 2, 0.20f, 10.00f },
        { eChampion::EZREAL, 3, 0.20f, 4.75f },
        { eChampion::EZREAL, 4, 0.20f, 200.00f },

        { eChampion::FIORA, 0, 1.67f, 1.50f },
        { eChampion::FIORA, 1, 5.00f, 4.00f },
        { eChampion::FIORA, 2, 5.00f, 0.00f },
        { eChampion::FIORA, 3, 5.00f, 0.00f },
        { eChampion::FIORA, 4, 5.00f, 5.00f },

        { eChampion::JAX, 0, 1.67f, 1.50f },
        { eChampion::JAX, 1, 5.00f, 7.00f },
        { eChampion::JAX, 2, 5.00f, 0.00f },
        { eChampion::JAX, 3, 5.00f, 0.00f },
        { eChampion::JAX, 4, 5.00f, 0.00f },

        { eChampion::LEESIN, 0, 0.60f, 1.50f },
        { eChampion::LEESIN, 1, 0.60f, 11.00f },
        { eChampion::LEESIN, 2, 0.60f, 7.00f },
        { eChampion::LEESIN, 3, 0.60f, 4.00f },
        { eChampion::LEESIN, 4, 0.60f, 3.00f },

        { eChampion::KINDRED, 0, 0.60f, 5.50f },
        { eChampion::KINDRED, 1, 0.60f, 5.50f },
        { eChampion::KINDRED, 2, 0.60f, 8.00f },
        { eChampion::KINDRED, 3, 0.60f, 5.50f },
        { eChampion::KINDRED, 4, 0.60f, 6.00f },

        { eChampion::MASTERYI, 0, 0.60f, 1.50f },
        { eChampion::MASTERYI, 1, 0.60f, 6.00f },
        { eChampion::MASTERYI, 2, 0.60f, 0.00f },
        { eChampion::MASTERYI, 3, 0.60f, 0.00f },
        { eChampion::MASTERYI, 4, 0.60f, 0.00f },

        { eChampion::ANNIE, 0, 0.60f, 6.25f },
        { eChampion::ANNIE, 1, 4.00f, 6.25f },
        { eChampion::ANNIE, 2, 8.00f, 6.00f },
        { eChampion::ANNIE, 3, 12.00f, 0.00f },
        { eChampion::ANNIE, 4, 100.00f, 6.00f },

        { eChampion::ASHE, 0, 1.72f, 6.00f },
        { eChampion::ASHE, 1, 5.00f, 0.00f },
        { eChampion::ASHE, 2, 5.00f, 9.00f },
        { eChampion::ASHE, 3, 5.00f, 25.00f },
        { eChampion::ASHE, 4, 5.00f, 200.00f },

        { eChampion::VIEGO, 0, 0.60f, 1.50f },
        { eChampion::VIEGO, 1, 4.00f, 5.50f },
        { eChampion::VIEGO, 2, 8.00f, 8.00f },
        { eChampion::VIEGO, 3, 14.00f, 6.00f },
        { eChampion::VIEGO, 4, 100.00f, 6.00f },

        { eChampion::YONE, 0, 0.75f, 1.50f },
        { eChampion::YONE, 1, 4.00f, 4.75f },
        { eChampion::YONE, 2, 16.00f, 6.00f },
        { eChampion::YONE, 3, 22.00f, 4.00f },
        { eChampion::YONE, 4, 120.00f, 10.00f },

        { eChampion::SYLAS, 0, 0.60f, 1.50f },
        { eChampion::SYLAS, 1, 4.00f, 4.00f },
        { eChampion::SYLAS, 2, 5.00f, 5.00f },
        { eChampion::SYLAS, 3, 6.00f, 6.00f },
        { eChampion::SYLAS, 4, 10.00f, 10.00f },
    };

    f32_t SanitizePositive(f32_t value, f32_t fallback)
    {
        return std::isfinite(value) && value > 0.001f ? value : fallback;
    }

    ChampionSkillTimingDefaults ApplyVerificationTiming(
        ChampionSkillTimingDefaults timing)
    {
        return timing;
    }

    const ChampionSkillValueRow* FindSkillValueRow(eChampion champion, u8_t slot)
    {
        for (const ChampionSkillValueRow& row : kChampionSkillValueTable)
        {
            if (row.champion == champion && row.slot == slot)
                return &row;
        }

        return nullptr;
    }
}

ChampionStatsDef BuildDefaultChampionStatsDef(eChampion champion)
{
    ChampionStatsDef def{};
    def.championId = champion;
    def.baseHp = 600.f;
    def.baseMana = 300.f;
    def.baseAd = 55.f;
    def.baseArmor = 30.f;
    def.baseMr = 30.f;
    def.baseAttackSpeed = 0.60f;
    def.attackSpeedRatio = 0.60f;
    def.baseAttackRange = 5.5f;
    def.baseMoveSpeed = 5.f;
    def.navArriveRadius = 0.15f;
    def.spatialRadius = 0.75f;
    def.sightRange = 19.f;

    if (const ChampionSkillValueRow* basicAttack =
        FindSkillValueRow(champion, static_cast<u8_t>(eSkillSlot::BasicAttack)))
    {
        def.baseAttackRange = SanitizePositive(basicAttack->rangeMax, def.baseAttackRange);
    }

    switch (champion)
    {
    case eChampion::ASHE:
        def.baseAd = 50.f;
        def.baseAttackSpeed = 0.58f;
        def.attackSpeedRatio = 0.58f;
        break;
    case eChampion::IRELIA:
        def.baseAd = 65.f;
        def.baseAttackSpeed = 0.90f;
        def.attackSpeedRatio = 0.90f;
        break;
    default:
        break;
    }

    return def;
}

void RegisterDefaultChampionSkillScalingTables()
{
}

StatComponent BuildDefaultChampionStat(eChampion champion, u8_t level)
{
    StatComponent stat{};
    stat.championId = champion;
    stat.level = level > 0 ? level : 1;
    stat.hpMax = 600.f;
    stat.manaMax = 300.f;
    stat.baseAd = 55.f;
    stat.bonusAd = 0.f;
    stat.ad = stat.baseAd;
    stat.baseArmor = 30.f;
    stat.bonusArmor = 0.f;
    stat.armor = stat.baseArmor;
    stat.baseMr = 30.f;
    stat.bonusMr = 0.f;
    stat.mr = stat.baseMr;
    stat.baseAttackSpeed = 0.60f;
    stat.attackSpeedRatio = 0.60f;
    stat.attackSpeedGrowth = 0.f;
    stat.bonusAttackSpeed = 0.f;
    stat.attackSpeed = stat.baseAttackSpeed;
    stat.attackRange = 5.5f;
    stat.moveSpeed = 5.f;
    stat.bDirty = false;

    if (const ChampionSkillValueRow* basicAttack =
        FindSkillValueRow(champion, static_cast<u8_t>(eSkillSlot::BasicAttack)))
    {
        stat.attackRange = SanitizePositive(basicAttack->rangeMax, stat.attackRange);
    }

    switch (champion)
    {
    case eChampion::ASHE:
        stat.baseAd = 50.f;
        stat.ad = stat.baseAd + stat.bonusAd;
        stat.baseAttackSpeed = 0.58f;
        stat.attackSpeedRatio = 0.58f;
        stat.attackSpeed = stat.baseAttackSpeed;
        break;
    case eChampion::IRELIA:
        stat.baseAd = 65.f;
        stat.ad = stat.baseAd + stat.bonusAd;
        stat.baseAttackSpeed = 0.90f;
        stat.attackSpeedRatio = 0.90f;
        stat.attackSpeed = stat.baseAttackSpeed;
        break;
    default:
        break;
    }

    return stat;
}

f32_t GetDefaultChampionSkillRange(eChampion champion, u8_t slot)
{
    if (slot == static_cast<u8_t>(eSkillSlot::BasicAttack))
        return BuildDefaultChampionStat(champion).attackRange;

    if (const ChampionSkillValueRow* row = FindSkillValueRow(champion, slot))
        return row->rangeMax;

    return 0.f;
}

f32_t GetDefaultChampionSkillCooldown(eChampion champion, u8_t slot)
{
    if constexpr (kUseFastSkillVerificationCooldown)
    {
        (void)champion;
        (void)slot;
        return kVerificationSkillCooldownSec;
    }

    if (slot == static_cast<u8_t>(eSkillSlot::BasicAttack))
    {
        if (const ChampionSkillValueRow* row = FindSkillValueRow(champion, slot))
            return row->cooldownSec;

        const StatComponent stat = BuildDefaultChampionStat(champion);
        return (stat.attackSpeed > 0.f) ? (1.f / stat.attackSpeed) : 1.f;
    }

    if (const ChampionSkillValueRow* row = FindSkillValueRow(champion, slot))
        return row->cooldownSec;

    return 1.f;
}

f32_t GetDefaultChampionVisualYawOffset(eChampion champion)
{
    switch (champion)
    {
    case eChampion::NONE:
    case eChampion::END:
        return 0.f;
    case eChampion::IRELIA:
    case eChampion::YASUO:
    case eChampion::VIEGO:
    case eChampion::KALISTA:
    case eChampion::SYLAS:
    case eChampion::ANNIE:
    case eChampion::ASHE:
    case eChampion::FIORA:
    case eChampion::GAREN:
    case eChampion::RIVEN:
    case eChampion::ZED:
    case eChampion::EZREAL:
    case eChampion::YONE:
    case eChampion::JAX:
    case eChampion::MASTERYI:
    case eChampion::KINDRED:
    case eChampion::LEESIN:
        return WintersMath::kPi;

    default:
        return 0.f;
    }
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
    return GetDefaultChampionSkillTiming(champion, slot, 1);
}

ChampionBasicAttackTimingDefaults GetDefaultChampionBasicAttackTiming(eChampion champion)
{
    const ChampionSkillTimingDefaults skillTiming =
        GetDefaultChampionSkillTiming(
            champion,
            static_cast<u8_t>(eSkillSlot::BasicAttack));

    ChampionBasicAttackTimingDefaults timing{};
    timing.fActionDurationSec = SanitizePositive(skillTiming.lockDurationSec, 0.75f);
    timing.fAnimPlaySpeed = SanitizePositive(skillTiming.animPlaySpeed, 1.f);

    const f32_t fRawWindup = timing.fActionDurationSec * 0.35f;
    const f32_t fMaxWindup = (std::max)(0.05f, timing.fActionDurationSec - 0.03f);
    timing.fWindupSec = std::clamp(fRawWindup, 0.12f, fMaxWindup);
    return timing;
}

ChampionSkillTimingDefaults GetDefaultChampionSkillTiming(eChampion champion, u8_t slot, u8_t stage)
{
    for (const ChampionSkillTimingRow& row : kChampionSkillTimingTable)
    {
        if (row.champion == champion && row.slot == slot && row.stage == stage)
        {
            ChampionSkillTimingDefaults timing{};
            timing.lockDurationSec = SanitizePositive(row.lockDurationSec, 0.6f);
            timing.animPlaySpeed = SanitizePositive(row.animPlaySpeed, 1.f);
            return ApplyVerificationTiming(timing);
        }
    }

    if (stage > 1)
        return GetDefaultChampionSkillTiming(champion, slot, 1);

    ChampionSkillTimingDefaults fallback{};
    fallback.lockDurationSec =
        slot == static_cast<u8_t>(eSkillSlot::BasicAttack)
        ? 0.75f
        : 0.6f;
    fallback.animPlaySpeed = 1.f;
    return ApplyVerificationTiming(fallback);
}

bool_t IsDefaultChampionSkillTwoStage(eChampion champion, u8_t slot)
{
    for (const ChampionSkillStageRow& row : kChampionSkillStageTable)
    {
        if (row.champion == champion && row.slot == slot)
            return row.stageCount >= 2;
    }

    return false;
}

f32_t GetDefaultChampionSkillStageWindowSec(eChampion champion, u8_t slot)
{
    for (const ChampionSkillStageRow& row : kChampionSkillStageTable)
    {
        if (row.champion == champion && row.slot == slot)
            return row.stageWindowSec;
    }

    return 0.f;
}

u16_t EncodeSkillPlaybackRateQ8(f32_t playSpeed)
{
    const f32_t sanitized = std::clamp(SanitizePositive(playSpeed, 1.f), 0.05f, 4.f);
    const i32_t encoded = static_cast<i32_t>(std::lround(sanitized * 256.f));
    return static_cast<u16_t>(std::clamp(encoded, 1, 1024));
}

u64_t GetDefaultChampionBasicAttackWindupTicks(eChampion champion)
{
    const ChampionBasicAttackTimingDefaults timing =
        GetDefaultChampionBasicAttackTiming(champion);
    const f64_t ticks =
        static_cast<f64_t>(timing.fWindupSec) *
        static_cast<f64_t>(DeterministicTime::kTicksPerSecond);
    const u64_t roundedUp = static_cast<u64_t>(std::ceil(ticks));
    return roundedUp > 0 ? roundedUp : 1u;
}

u64_t GetDefaultChampionSkillActionLockTicks(eChampion champion, u8_t slot)
{
    return GetDefaultChampionSkillActionLockTicks(champion, slot, 1);
}

u64_t GetDefaultChampionSkillActionLockTicks(eChampion champion, u8_t slot, u8_t stage)
{
    const ChampionSkillTimingDefaults timing = GetDefaultChampionSkillTiming(champion, slot, stage);
    const f64_t ticks =
        static_cast<f64_t>(timing.lockDurationSec) *
        static_cast<f64_t>(DeterministicTime::kTicksPerSecond);
    const u64_t roundedUp = static_cast<u64_t>(std::ceil(ticks));
    return roundedUp > 0 ? roundedUp : 1u;
}
