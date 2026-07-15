#include "Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.h"
#include "Shared/GameSim/Core/Debug/SimDebugOutput.h"
#include "Shared/GameSim/Core/Determinism/DeterministicTime.h"
#include "Shared/GameSim/Generated/ChampionGameData.generated.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
    f32_t SanitizePositive(f32_t value, f32_t fallback)
    {
        return std::isfinite(value) && value > 0.001f ? value : fallback;
    }

    ChampionStatsDef BuildFallbackStats(eChampion champion)
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
        return def;
    }

    StatComponent BuildStatFromDef(const ChampionStatsDef& def, u8_t level)
    {
        StatComponent stat{};
        stat.championId = def.championId;
        stat.level = level > 0 ? level : 1;
        stat.hpMax = def.baseHp;
        stat.manaMax = def.baseMana;
        stat.baseAd = def.baseAd;
        stat.bonusAd = 0.f;
        stat.ad = stat.baseAd;
        stat.baseArmor = def.baseArmor;
        stat.bonusArmor = 0.f;
        stat.armor = stat.baseArmor;
        stat.baseMr = def.baseMr;
        stat.bonusMr = 0.f;
        stat.mr = stat.baseMr;
        stat.baseAttackSpeed = def.baseAttackSpeed;
        stat.attackSpeedRatio = def.attackSpeedRatio;
        stat.attackSpeedGrowth = 0.f;
        stat.bonusAttackSpeed = 0.f;
        stat.attackSpeed = stat.baseAttackSpeed;
        stat.attackRange = def.baseAttackRange;
        stat.moveSpeed = def.baseMoveSpeed;
        stat.bDirty = false;
        return stat;
    }

    ChampionSkillTimingDefaults MakeSkillTiming(f32_t lockDurationSec)
    {
        ChampionSkillTimingDefaults timing{};
        timing.lockDurationSec = SanitizePositive(lockDurationSec, 0.6f);
        return timing;
    }

    ChampionSkillTimingDefaults MakeFallbackSkillTiming(u8_t slot)
    {
        ChampionSkillTimingDefaults timing{};
        timing.lockDurationSec =
            slot == static_cast<u8_t>(eSkillSlot::BasicAttack)
            ? 0.75f
            : 0.6f;
        return timing;
    }

    ChampionBasicAttackTimingDefaults BuildBasicAttackTiming(
        const ChampionSkillTimingDefaults& skillTiming)
    {
        ChampionBasicAttackTimingDefaults timing{};
        timing.fActionDurationSec = SanitizePositive(skillTiming.lockDurationSec, 0.75f);

        const f32_t fRawWindup = timing.fActionDurationSec * 0.35f;
        const f32_t fMaxWindup = (std::max)(0.05f, timing.fActionDurationSec - 0.03f);
        timing.fWindupSec = std::clamp(fRawWindup, 0.12f, fMaxWindup);
        return timing;
    }

    u64_t ResolveTicksFromSeconds(f32_t seconds)
    {
        const f32_t sanitizedSeconds = std::isfinite(seconds) && seconds > 0.f ? seconds : 0.f;
        const f64_t ticks =
            static_cast<f64_t>(sanitizedSeconds) *
            static_cast<f64_t>(DeterministicTime::kTicksPerSecond);
        const u64_t roundedUp = static_cast<u64_t>(std::ceil(ticks));
        return roundedUp > 0u ? roundedUp : 1u;
    }

}

namespace ChampionGameDataDB
{
    u32_t GetSchemaVersion()
    {
        return kChampionGameDataSchemaVersion;
    }

    u32_t GetBuildHash()
    {
        return ChampionGameDataGenerated::GetBuildHash();
    }

    const ChampionGameData* FindChampion(eChampion champion)
    {
        return ChampionGameDataGenerated::FindChampion(champion);
    }

    const ChampionGameDataSkill* FindSkill(eChampion champion, u8_t slot)
    {
        const ChampionGameData* pData = FindChampion(champion);
        if (!pData || slot >= kChampionGameDataSkillSlotCount)
        {
            return nullptr;
        }

        const ChampionGameDataSkill& skill = pData->skills[slot];
        return skill.bValid ? &skill : nullptr;
    }

    ChampionStatsDef ResolveStats(eChampion champion)
    {
        if (const ChampionGameData* pData = FindChampion(champion))
        {
            return pData->stats;
        }

        // P1: 생성 테이블에 없는 챔피언이 범용 스탯(600HP/55AD)으로 조용히 살아나면
        // 데이터 팩 오구성이 "미묘하게 이상한 챔피언"으로만 보인다.
        static u32_t s_fallbackStatsLogCount = 0;
        if (s_fallbackStatsLogCount < 16u)
        {
            char msg[128]{};
            sprintf_s(msg,
                "[Data] champion game-data miss champ=%u -> fallback stats\n",
                static_cast<u32_t>(champion));
            WintersOutputAIDebugStringA(msg);
            ++s_fallbackStatsLogCount;
        }
        return BuildFallbackStats(champion);
    }

    StatComponent BuildStat(eChampion champion, u8_t level)
    {
        return BuildStatFromDef(ResolveStats(champion), level);
    }

    f32_t ResolveSkillRange(eChampion champion, u8_t slot)
    {
        if (const ChampionGameDataSkill* pSkill = FindSkill(champion, slot))
        {
            return pSkill->rangeMax;
        }

        if (slot == static_cast<u8_t>(eSkillSlot::BasicAttack))
        {
            return BuildStat(champion, 1).attackRange;
        }

        return 0.f;
    }

    f32_t ResolveSkillCooldown(eChampion champion, u8_t slot)
    {
        if (const ChampionGameDataSkill* pSkill = FindSkill(champion, slot))
        {
            return pSkill->cooldownSec;
        }

        if (slot == static_cast<u8_t>(eSkillSlot::BasicAttack))
        {
            const StatComponent stat = BuildStat(champion, 1);
            return (stat.attackSpeed > 0.f) ? (1.f / stat.attackSpeed) : 1.f;
        }

        return 1.f;
    }

    ChampionSkillTimingDefaults ResolveSkillTiming(eChampion champion, u8_t slot, u8_t stage)
    {
        if (const ChampionGameDataSkill* pSkill = FindSkill(champion, slot))
        {
            u8_t stageNumber = stage > 0 ? stage : 1u;
            if (stageNumber > pSkill->stageCount)
            {
                stageNumber = 1u;
            }

            const u8_t stageIndex = static_cast<u8_t>(stageNumber - 1u);
            if (stageIndex < kChampionGameDataSkillStageMax)
            {
                const ChampionGameDataSkillStage& skillStage = pSkill->stages[stageIndex];
                return MakeSkillTiming(skillStage.lockDurationSec);
            }
        }

        return MakeFallbackSkillTiming(slot);
    }

    ChampionBasicAttackTimingDefaults ResolveBasicAttackTiming(eChampion champion)
    {
        return BuildBasicAttackTiming(ResolveSkillTiming(
            champion,
            static_cast<u8_t>(eSkillSlot::BasicAttack),
            1));
    }

    u64_t ResolveBasicAttackWindupTicks(eChampion champion)
    {
        const ChampionBasicAttackTimingDefaults timing = ResolveBasicAttackTiming(champion);
        return ResolveTicksFromSeconds(timing.fWindupSec);
    }

    u64_t ResolveSkillActionLockTicks(eChampion champion, u8_t slot, u8_t stage)
    {
        const ChampionSkillTimingDefaults timing = ResolveSkillTiming(champion, slot, stage);
        return ResolveTicksFromSeconds(timing.lockDurationSec);
    }

    bool_t IsSkillTwoStage(eChampion champion, u8_t slot)
    {
        if (const ChampionGameDataSkill* pSkill = FindSkill(champion, slot))
        {
            return pSkill->stageCount >= 2;
        }

        return false;
    }

    f32_t ResolveSkillStageWindowSec(eChampion champion, u8_t slot)
    {
        if (const ChampionGameDataSkill* pSkill = FindSkill(champion, slot))
        {
            return pSkill->stageWindowSec;
        }

        return 0.f;
    }

    const ChampionGameDataPassiveDash* FindPassiveDash(eChampion champion)
    {
        const ChampionGameData* pData = FindChampion(champion);
        if (!pData || !pData->passiveDash.bValid)
        {
            return nullptr;
        }

        return &pData->passiveDash;
    }

    f32_t ResolvePassiveDashDistance(eChampion champion)
    {
        if (const ChampionGameDataPassiveDash* pDash = FindPassiveDash(champion))
        {
            return pDash->distance;
        }

        return 0.f;
    }

    f32_t ResolvePassiveDashDurationSec(eChampion champion)
    {
        if (const ChampionGameDataPassiveDash* pDash = FindPassiveDash(champion))
        {
            return pDash->durationSec;
        }

        return 0.f;
    }

    f32_t ResolvePassiveDashInputGraceSec(eChampion champion)
    {
        if (const ChampionGameDataPassiveDash* pDash = FindPassiveDash(champion))
        {
            return pDash->inputGraceSec;
        }

        return 0.f;
    }

    u64_t ResolvePassiveDashInputGraceTicks(eChampion champion)
    {
        if (const ChampionGameDataPassiveDash* pDash = FindPassiveDash(champion))
        {
            return ResolveTicksFromSeconds(pDash->inputGraceSec);
        }

        return 0u;
    }
}
