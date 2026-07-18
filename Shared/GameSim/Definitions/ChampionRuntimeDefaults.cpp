#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"

#include "Shared/GameSim/Core/Determinism/DeterministicTime.h"
#include "Shared/GameSim/Generated/ChampionGameData.generated.h"

#include <algorithm>
#include <cmath>

namespace
{
    const ChampionGameDataSkill* FindAuthoredSkill(eChampion champion, u8_t slot)
    {
        const ChampionGameData* data = ChampionGameDataGenerated::FindChampion(champion);
        if (!data || slot >= kChampionGameDataSkillSlotCount)
            return nullptr;
        const ChampionGameDataSkill& skill = data->skills[slot];
        return skill.bValid ? &skill : nullptr;
    }

    const ChampionGameDataPassiveDash* FindAuthoredPassiveDash(eChampion champion)
    {
        const ChampionGameData* data = ChampionGameDataGenerated::FindChampion(champion);
        return data && data->passiveDash.bValid ? &data->passiveDash : nullptr;
    }

    StatComponent BuildStatFromDef(const ChampionStatsDef& def, u8_t level)
    {
        StatComponent stat{};
        stat.championId = def.championId;
        stat.level = level > 0u ? level : 1u;
        stat.hpMax = def.baseHp;
        stat.manaMax = def.baseMana;
        stat.baseAd = def.baseAd;
        stat.ad = def.baseAd;
        stat.baseArmor = def.baseArmor;
        stat.armor = def.baseArmor;
        stat.baseMr = def.baseMr;
        stat.mr = def.baseMr;
        stat.baseAttackSpeed = def.baseAttackSpeed;
        stat.attackSpeedRatio = def.attackSpeedRatio;
        stat.attackSpeed = def.baseAttackSpeed;
        stat.attackRange = def.baseAttackRange;
        stat.moveSpeed = def.baseMoveSpeed;
        stat.bDirty = false;
        return stat;
    }

    ChampionSkillTimingDefaults MakeSkillTiming(f32_t lockDurationSec)
    {
        ChampionSkillTimingDefaults timing{};
        if (std::isfinite(lockDurationSec) && lockDurationSec > 0.f)
            timing.lockDurationSec = lockDurationSec;
        return timing;
    }

    u64_t ResolveTicksFromSeconds(f32_t seconds)
    {
        if (!std::isfinite(seconds) || seconds <= 0.f)
            return 0u;
        const f64_t ticks =
            static_cast<f64_t>(seconds) *
            static_cast<f64_t>(DeterministicTime::kTicksPerSecond);
        return static_cast<u64_t>(std::ceil(ticks));
    }
}

u32_t GetDefaultChampionDataBuildHash()
{
    return ChampionGameDataGenerated::GetBuildHash();
}

ChampionStatsDef BuildDefaultChampionStatsDef(eChampion champion)
{
    const ChampionGameData* data = ChampionGameDataGenerated::FindChampion(champion);
    return data ? data->stats : ChampionStatsDef{};
}

StatComponent BuildDefaultChampionStat(eChampion champion, u8_t level)
{
    return BuildStatFromDef(BuildDefaultChampionStatsDef(champion), level);
}

f32_t GetDefaultChampionSkillRange(eChampion champion, u8_t slot)
{
    const ChampionGameDataSkill* skill = FindAuthoredSkill(champion, slot);
    return skill ? skill->rangeMax : 0.f;
}

f32_t GetDefaultChampionSkillCooldown(eChampion champion, u8_t slot)
{
    const ChampionGameDataSkill* skill = FindAuthoredSkill(champion, slot);
    return skill && skill->rankCount > 0u ? skill->cooldownSecByRank[0] : 0.f;
}

f32_t GetDefaultChampionPassiveDashDistance(eChampion champion)
{
    const ChampionGameDataPassiveDash* dash = FindAuthoredPassiveDash(champion);
    return dash ? dash->distance : 0.f;
}

f32_t GetDefaultChampionPassiveDashDurationSec(eChampion champion)
{
    const ChampionGameDataPassiveDash* dash = FindAuthoredPassiveDash(champion);
    return dash ? dash->durationSec : 0.f;
}

f32_t GetDefaultChampionPassiveDashInputGraceSec(eChampion champion)
{
    const ChampionGameDataPassiveDash* dash = FindAuthoredPassiveDash(champion);
    return dash ? dash->inputGraceSec : 0.f;
}

f32_t GetDefaultChampionVisualYawOffset(eChampion champion)
{
    return (champion == eChampion::NONE || champion == eChampion::END)
        ? 0.f
        : WintersMath::kPi;
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
    return GetDefaultChampionSkillTiming(champion, slot, 1u);
}

ChampionSkillTimingDefaults GetDefaultChampionSkillTiming(
    eChampion champion,
    u8_t slot,
    u8_t stage)
{
    const ChampionGameDataSkill* skill = FindAuthoredSkill(champion, slot);
    if (!skill || stage == 0u || stage > skill->stageCount ||
        stage > kChampionGameDataSkillStageMax)
    {
        return {};
    }
    return MakeSkillTiming(skill->stages[stage - 1u].lockDurationSec);
}

ChampionBasicAttackTimingDefaults GetDefaultChampionBasicAttackTiming(eChampion champion)
{
    const ChampionSkillTimingDefaults skillTiming = GetDefaultChampionSkillTiming(
        champion,
        static_cast<u8_t>(eSkillSlot::BasicAttack),
        1u);
    ChampionBasicAttackTimingDefaults timing{};
    timing.fActionDurationSec = skillTiming.lockDurationSec;
    if (timing.fActionDurationSec <= 0.f)
        return timing;
    const ChampionStatsDef stats = BuildDefaultChampionStatsDef(champion);
    const f32_t maxWindup = (std::max)(0.05f, timing.fActionDurationSec - 0.03f);
    if (std::isfinite(stats.basicAttackWindupSec) &&
        stats.basicAttackWindupSec > 0.f)
    {
        timing.fWindupSec = std::clamp(
            stats.basicAttackWindupSec,
            0.05f,
            maxWindup);
    }
    else
    {
        timing.fWindupSec = std::clamp(
            timing.fActionDurationSec * 0.35f,
            0.12f,
            maxWindup);
    }
    return timing;
}

bool_t IsDefaultChampionSkillTwoStage(eChampion champion, u8_t slot)
{
    const ChampionGameDataSkill* skill = FindAuthoredSkill(champion, slot);
    return skill && skill->stageCount >= 2u;
}

f32_t GetDefaultChampionSkillStageWindowSec(eChampion champion, u8_t slot)
{
    const ChampionGameDataSkill* skill = FindAuthoredSkill(champion, slot);
    return skill ? skill->stageWindowSec : 0.f;
}

u16_t EncodeSkillPlaybackRateQ8(f32_t playSpeed)
{
    const f32_t sanitized =
        std::isfinite(playSpeed) && playSpeed > 0.f
        ? std::clamp(playSpeed, 0.05f, 4.f)
        : 1.f;
    const i32_t encoded = static_cast<i32_t>(std::lround(sanitized * 256.f));
    return static_cast<u16_t>(std::clamp(encoded, 1, 1024));
}

u64_t GetDefaultChampionBasicAttackWindupTicks(eChampion champion)
{
    return ResolveTicksFromSeconds(GetDefaultChampionBasicAttackTiming(champion).fWindupSec);
}

u64_t GetDefaultChampionSkillActionLockTicks(eChampion champion, u8_t slot)
{
    return GetDefaultChampionSkillActionLockTicks(champion, slot, 1u);
}

u64_t GetDefaultChampionSkillActionLockTicks(eChampion champion, u8_t slot, u8_t stage)
{
    return ResolveTicksFromSeconds(
        GetDefaultChampionSkillTiming(champion, slot, stage).lockDurationSec);
}
