#include "Shared/GameSim/Definitions/GameplayDefinitionQuery.h"

#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/ChampionDefinitionComponent.h"
#include "Shared/GameSim/Components/SkillLoadoutComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Core/Determinism/DeterministicTime.h"
#include "Shared/GameSim/Core/World/World.h"
#include "Shared/GameSim/Core/Debug/SimDebugOutput.h"
#include "Shared/GameSim/Definitions/GameplayDefinitionPack.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
    const GameplayDefinitionPack* GetPack(const TickContext& tc)
    {
        return tc.pDefinitions;
    }

    eChampion ResolveChampionFallback(CWorld& world, EntityID entity, eChampion fallbackChampion)
    {
        if (fallbackChampion != eChampion::NONE && fallbackChampion != eChampion::END)
        {
            return fallbackChampion;
        }

        if (entity != NULL_ENTITY && world.HasComponent<StatComponent>(entity))
        {
            return world.GetComponent<StatComponent>(entity).championId;
        }

        if (entity != NULL_ENTITY && world.HasComponent<ChampionComponent>(entity))
        {
            return world.GetComponent<ChampionComponent>(entity).id;
        }

        return fallbackChampion;
    }

    eChampion ResolveEntityChampion(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<ChampionComponent>(entity))
            return world.GetComponent<ChampionComponent>(entity).id;

        if (entity != NULL_ENTITY && world.HasComponent<StatComponent>(entity))
            return world.GetComponent<StatComponent>(entity).championId;

        return eChampion::NONE;
    }

    bool_t IsExplicitChampionOverride(
        CWorld& world,
        EntityID entity,
        eChampion fallbackChampion)
    {
        if (fallbackChampion == eChampion::NONE || fallbackChampion == eChampion::END)
            return false;

        const eChampion entityChampion = ResolveEntityChampion(world, entity);
        return entityChampion != eChampion::NONE &&
            entityChampion != eChampion::END &&
            entityChampion != fallbackChampion;
    }

    // P1 데이터 폴백 가시화: pack miss가 legacy ChampionGameDataDB로 조용히 흘러가면
    // 깨진 데이터 팩이 미묘하게 다른 champion 동작으로만 나타난다 (WINTERS_DATA_ARCHITECTURE.md).
    // 이 카운터를 0으로 만드는 것이 legacy DB 삭제의 게이트다.
    void ReportMissingGameplayDefinition(
        CWorld& world,
        EntityID entity,
        eChampion fallbackChampion,
        const char* pWhat,
        u8_t slot)
    {
        static u32_t s_missingDefinitionLogCount = 0;
        if (s_missingDefinitionLogCount >= 32u)
            return;
        const eChampion champion =
            ResolveChampionFallback(world, entity, fallbackChampion);
        char msg[160]{};
        sprintf_s(msg,
            "[Data] required definition missing: %s champ=%u slot=%u\n",
            pWhat,
            static_cast<u32_t>(champion),
            static_cast<u32_t>(slot));
        WintersOutputAIDebugStringA(msg);
        ++s_missingDefinitionLogCount;
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

    u8_t ResolveSkillRankForScaling(CWorld& world, EntityID entity, u8_t slot)
    {
        if (entity != NULL_ENTITY &&
            slot < SkillRankComponent::kSlotCount &&
            world.HasComponent<SkillRankComponent>(entity))
        {
            const u8_t rank =
                world.GetComponent<SkillRankComponent>(entity).ranks[slot];
            if (rank > 0u)
                return rank;
        }
        return 1u;
    }

    const SkillGameplayDef* FindSkillInChampion(
        const GameplayDefinitionPack& pack,
        const ChampionGameplayDef& champion,
        u8_t slot)
    {
        if (slot >= kChampionSkillSlotCount)
        {
            return nullptr;
        }

        return pack.FindSkill(champion.skillLoadout[slot]);
    }

    const PassiveDashGameplayDef* FindPassiveDash(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion)
    {
        const ChampionGameplayDef* champion =
            GameplayDefinitionQuery::FindChampion(world, entity, tc, fallbackChampion);
        if (!champion || !champion->passiveDash.bValid)
        {
            return nullptr;
        }

        return &champion->passiveDash;
    }
}

namespace GameplayDefinitionQuery
{
    const ChampionGameplayDef* FindChampion(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion)
    {
        const GameplayDefinitionPack* pack = GetPack(tc);
        if (!pack)
        {
            return nullptr;
        }

        const bool_t bExplicitChampionOverride =
            IsExplicitChampionOverride(world, entity, fallbackChampion);

        if (!bExplicitChampionOverride &&
            entity != NULL_ENTITY &&
            world.HasComponent<ChampionDefinitionComponent>(entity))
        {
            if (const ChampionGameplayDef* champion =
                pack->FindChampion(world.GetComponent<ChampionDefinitionComponent>(entity).championDefId))
            {
                return champion;
            }
        }

        const eChampion champion = ResolveChampionFallback(world, entity, fallbackChampion);
        return pack->FindChampion(champion);
    }

    const SkillGameplayDef* FindSkill(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot)
    {
        const GameplayDefinitionPack* pack = GetPack(tc);
        if (!pack || slot >= kChampionSkillSlotCount)
        {
            return nullptr;
        }

        const bool_t bExplicitChampionOverride =
            IsExplicitChampionOverride(world, entity, fallbackChampion);

        if (!bExplicitChampionOverride &&
            entity != NULL_ENTITY &&
            world.HasComponent<SkillLoadoutComponent>(entity))
        {
            if (const SkillGameplayDef* skill =
                pack->FindSkill(world.GetComponent<SkillLoadoutComponent>(entity).skills[slot]))
            {
                return skill;
            }
        }

        if (const ChampionGameplayDef* champion = FindChampion(world, entity, tc, fallbackChampion))
        {
            return FindSkillInChampion(*pack, *champion, slot);
        }

        return nullptr;
    }

    f32_t ResolveAttackRange(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion)
    {
        if (!IsExplicitChampionOverride(world, entity, fallbackChampion) &&
            entity != NULL_ENTITY &&
            world.HasComponent<StatComponent>(entity))
        {
            const f32_t range = world.GetComponent<StatComponent>(entity).attackRange;
            if (range > 0.f)
            {
                return range;
            }
        }

        if (const ChampionGameplayDef* champion = FindChampion(world, entity, tc, fallbackChampion))
        {
            return champion->stats.baseAttackRange;
        }

        ReportMissingGameplayDefinition(
            world, entity, fallbackChampion, "attack-range", 0xFFu);
        return 0.f;
    }

    f32_t ResolveSkillRange(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot)
    {
        if (const SkillGameplayDef* skill = FindSkill(world, entity, tc, fallbackChampion, slot))
        {
            return skill->range.rangeMax;
        }

        if (slot == static_cast<u8_t>(eSkillSlot::BasicAttack))
        {
            return ResolveAttackRange(world, entity, tc, fallbackChampion);
        }

        ReportMissingGameplayDefinition(
            world, entity, fallbackChampion, "skill-range", slot);
        return 0.f;
    }

    f32_t ResolveSkillCooldown(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot)
    {
        if (const SkillGameplayDef* skill = FindSkill(world, entity, tc, fallbackChampion, slot))
        {
            const u8_t rank = ResolveSkillRankForScaling(world, entity, slot);
            const u8_t count = std::clamp<u8_t>(
                skill->cooldown.rankCount, 1u, kSkillRankValueMax);
            const u8_t index = static_cast<u8_t>(std::clamp<u8_t>(rank, 1u, count) - 1u);
            return (std::max)(0.f, skill->cooldown.cooldownSecByRank[index]);
        }

        ReportMissingGameplayDefinition(
            world, entity, fallbackChampion, "skill-cooldown", slot);
        return 0.f;
    }

    f32_t ResolveSkillManaCost(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot)
    {
        if (const SkillGameplayDef* skill = FindSkill(
            world,
            entity,
            tc,
            fallbackChampion,
            slot))
        {
            const u8_t rank = ResolveSkillRankForScaling(world, entity, slot);
            const u8_t count = std::clamp<u8_t>(
                skill->cost.rankCount, 1u, kSkillRankValueMax);
            const u8_t index = static_cast<u8_t>(std::clamp<u8_t>(rank, 1u, count) - 1u);
            return (std::max)(0.f, skill->cost.manaCostByRank[index]);
        }

        return 0.f;
    }

    f32_t ResolveSkillEffectParam(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot,
        eSkillEffectParamId param,
        f32_t fallbackValue)
    {
        if (entity != NULL_ENTITY &&
            world.HasComponent<PracticeSkillEffectOverrideComponent>(entity))
        {
            const auto& overrides =
                world.GetComponent<PracticeSkillEffectOverrideComponent>(entity);
            const u8_t count = (std::min)(
                overrides.count,
                PracticeSkillEffectOverrideComponent::kMaxEntries);
            for (u8_t index = 0u; index < count; ++index)
            {
                const auto& entry = overrides.entries[index];
                if (entry.slot == slot &&
                    entry.paramId == static_cast<u8_t>(param))
                {
                    return entry.value;
                }
            }
        }

        if (const SkillGameplayDef* skill = FindSkill(world, entity, tc, fallbackChampion, slot))
        {
            return ::ResolveSkillEffectParam(skill->effect, param, fallbackValue);
        }

        return fallbackValue;
    }

    bool_t BuildSkillDamageRequest(
        CWorld& world,
        EntityID source,
        EntityID target,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot,
        u8_t rank,
        eTeam sourceTeam,
        eDamageSourceKind sourceKind,
        DamageRequest& outRequest)
    {
        const SkillGameplayDef* skill =
            FindSkill(world, source, tc, fallbackChampion, slot);
        if (!skill || !skill->effect.damage.bValid)
            return false;

        const DamageFormulaDef& formula = skill->effect.damage;
        outRequest = {};
        outRequest.source = source;
        outRequest.target = target;
        outRequest.sourceTeam = sourceTeam;
        outRequest.type = formula.type;
        outRequest.flatAmount = ResolveDamageFormulaRankedValue(
            formula, formula.flatByRank, rank);
        outRequest.adRatioOverride = ResolveDamageFormulaRankedValue(
            formula, formula.totalAdRatioByRank, rank);
        outRequest.bonusAdRatioOverride = ResolveDamageFormulaRankedValue(
            formula, formula.bonusAdRatioByRank, rank);
        outRequest.apRatioOverride = ResolveDamageFormulaRankedValue(
            formula, formula.apRatioByRank, rank);
        outRequest.targetMaxHpRatioOverride = ResolveDamageFormulaRankedValue(
            formula, formula.targetMaxHpRatioByRank, rank);
        outRequest.targetMissingHpRatioOverride = ResolveDamageFormulaRankedValue(
            formula, formula.targetMissingHpRatioByRank, rank);
        outRequest.skillId = static_cast<u16_t>(
            (static_cast<u32_t>(fallbackChampion) << 8u) | slot);
        outRequest.rank = rank > 0u ? rank : 1u;
        outRequest.iSourceSlot = slot;
        outRequest.eSourceKind = sourceKind;
        outRequest.flags = formula.flags;
        return true;
    }

    f32_t ResolveSummonPolicyParam(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot,
        eSummonPolicyParamId param,
        f32_t fallbackValue)
    {
        if (const SkillGameplayDef* skill = FindSkill(world, entity, tc, fallbackChampion, slot))
        {
            return ::ResolveSummonPolicyParam(skill->summonPolicy, param, fallbackValue);
        }

        return fallbackValue;
    }

    u64_t ResolveSkillActionLockTicks(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot,
        u8_t stage)
    {
        u64_t lockTicks = 0u;
        if (const SkillGameplayDef* skill = FindSkill(world, entity, tc, fallbackChampion, slot))
        {
            const u8_t stageNumber = stage > 0u ? stage : 1u;
            const u8_t stageIndex = static_cast<u8_t>((std::min)(
                static_cast<u8_t>(stageNumber - 1u),
                static_cast<u8_t>(kSkillAtomStageMax - 1u)));
            lockTicks = ResolveTicksFromSeconds(skill->stage.commandLockSec[stageIndex]);
        }
        else
        {
            ReportMissingGameplayDefinition(
                world, entity, fallbackChampion, "skill-lock-ticks", slot);
        }

        const eSkillActionMovePolicy policy =
            ResolveSkillActionMovePolicy(
                world, entity, tc, fallbackChampion, slot, stage);
        if (policy == eSkillActionMovePolicy::Allow)
            return 0u;
        if (policy == eSkillActionMovePolicy::QueueUntilUnlock)
        {
            constexpr u64_t kMaximumCastInputLockTicks = 8u;
            return (std::min)(lockTicks, kMaximumCastInputLockTicks);
        }
        return lockTicks;
    }

    eSkillActionMovePolicy ResolveSkillActionMovePolicy(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot,
        u8_t stage)
    {
        if (slot == static_cast<u8_t>(eSkillSlot::BasicAttack))
            return eSkillActionMovePolicy::Allow;

        if (const SkillGameplayDef* skill =
            FindSkill(world, entity, tc, fallbackChampion, slot))
        {
            const u8_t stageNumber = stage > 0u ? stage : 1u;
            const u8_t stageIndex = static_cast<u8_t>((std::min)(
                static_cast<u8_t>(stageNumber - 1u),
                static_cast<u8_t>(kSkillAtomStageMax - 1u)));
            return skill->stage.movePolicy[stageIndex];
        }

        ReportMissingGameplayDefinition(
            world, entity, fallbackChampion, "skill-move-policy", slot);
        return eSkillActionMovePolicy::QueueUntilUnlock;
    }

    bool_t ShouldCreateSkillActionState(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot,
        u8_t stage)
    {
        if (const SkillGameplayDef* skill =
            FindSkill(world, entity, tc, fallbackChampion, slot))
        {
            const u8_t stageNumber = stage > 0u ? stage : 1u;
            const u8_t stageIndex = static_cast<u8_t>((std::min)(
                static_cast<u8_t>(stageNumber - 1u),
                static_cast<u8_t>(kSkillAtomStageMax - 1u)));
            return skill->stage.bCreatesActionState[stageIndex];
        }

        ReportMissingGameplayDefinition(
            world, entity, fallbackChampion, "skill-action-state", slot);
        return true;
    }

    bool_t IsSkillTwoStage(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot)
    {
        if (const SkillGameplayDef* skill = FindSkill(world, entity, tc, fallbackChampion, slot))
        {
            return skill->stage.stageCount >= 2u;
        }

        ReportMissingGameplayDefinition(
            world, entity, fallbackChampion, "skill-two-stage", slot);
        return false;
    }

    f32_t ResolveSkillStageWindowSec(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot)
    {
        if (const SkillGameplayDef* skill = FindSkill(world, entity, tc, fallbackChampion, slot))
        {
            return skill->stage.stageWindowSec;
        }

        ReportMissingGameplayDefinition(
            world, entity, fallbackChampion, "skill-stage-window", slot);
        return 0.f;
    }

    f32_t ResolveSummonerSpellRange(const TickContext& tc, u16_t legacySpellId)
    {
        if (const GameplayDefinitionPack* pack = GetPack(tc))
        {
            if (const SummonerSpellGameplayDef* spell =
                pack->FindSummonerSpellByLegacyId(legacySpellId))
            {
                return spell->rangeMax;
            }
        }

        return 0.f;
    }

    f32_t ResolveSummonerSpellCooldown(const TickContext& tc, u16_t legacySpellId)
    {
        if (const GameplayDefinitionPack* pack = GetPack(tc))
        {
            if (const SummonerSpellGameplayDef* spell =
                pack->FindSummonerSpellByLegacyId(legacySpellId))
            {
                return spell->cooldownSec;
            }
        }

        return 0.f;
    }

    f32_t ResolvePassiveDashDistance(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion)
    {
        if (const PassiveDashGameplayDef* dash = FindPassiveDash(world, entity, tc, fallbackChampion))
        {
            return dash->distance;
        }

        ReportMissingGameplayDefinition(
            world, entity, fallbackChampion, "passive-dash-distance", 0xFFu);
        return 0.f;
    }

    f32_t ResolvePassiveDashDurationSec(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion)
    {
        if (const PassiveDashGameplayDef* dash = FindPassiveDash(world, entity, tc, fallbackChampion))
        {
            return dash->durationSec;
        }

        ReportMissingGameplayDefinition(
            world, entity, fallbackChampion, "passive-dash-duration", 0xFFu);
        return 0.f;
    }

    f32_t ResolvePassiveDashInputGraceSec(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion)
    {
        if (const PassiveDashGameplayDef* dash = FindPassiveDash(world, entity, tc, fallbackChampion))
        {
            return dash->inputGraceSec;
        }

        ReportMissingGameplayDefinition(
            world, entity, fallbackChampion, "passive-dash-grace-sec", 0xFFu);
        return 0.f;
    }

    u64_t ResolvePassiveDashInputGraceTicks(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion)
    {
        if (const PassiveDashGameplayDef* dash = FindPassiveDash(world, entity, tc, fallbackChampion))
        {
            return ResolveTicksFromSeconds(dash->inputGraceSec);
        }

        ReportMissingGameplayDefinition(
            world, entity, fallbackChampion, "passive-dash-grace-ticks", 0xFFu);
        return 0u;
    }
}
