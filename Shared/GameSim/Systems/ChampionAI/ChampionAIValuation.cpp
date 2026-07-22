#include "Shared/GameSim/Systems/ChampionAI/ChampionAIValuation.h"

#include "WintersMath.h"

#include <algorithm>

namespace ChampionAIValuation
{
    namespace
    {
        f32_t ClampSigned(f32_t v)
        {
            return std::clamp(v, -1.f, 1.f);
        }
    }

    f32_t EconomyLead(const ValueInput& in)
    {
        const f32_t goldDiff = in.selfGold - in.enemyGold;
        const f32_t levelDiff =
            (static_cast<f32_t>(in.selfLevel) - static_cast<f32_t>(in.enemyLevel))
            * kLevelGoldValue;
        return ClampSigned((goldDiff + levelDiff) / kGoldLeadFullScale);
    }

    f32_t HealthLead(const ValueInput& in)
    {
        return ClampSigned(in.selfHpRatio - in.enemyHpRatio);
    }

    f32_t ChampionFightValue(const ValueInput& in)
    {
        // 기존 점수식을 보존하고 경제 우위 항만 명시적으로 추가한다.
        f32_t score = 0.45f;
        score += (in.selfHpRatio - in.retreatHpRatio) * 0.30f;            // 자기 생존 여유
        score += HealthLead(in) * 0.35f;                                 // 체력 우위(딜교환)
        score += (in.enemyDistance <= in.engageRange) ? 0.20f : -0.10f;  // 진입 사거리
        score += in.bAlliedWaveNearby ? 0.10f : 0.f;                     // 백업 웨이브
        score += EconomyLead(in) * 0.10f;                                // 경제 우위(신규)
        score -= in.turretDanger * 0.50f * in.turretRiskWeight;          // 포탑 위험
        score -= WintersMath::Clamp01(in.incomingComboDamageRatio) * 0.45f;
        return WintersMath::Clamp01(score);
    }

    f32_t MinionFarmValue(const ValueInput& in)
    {
        f32_t score = in.bEnemyMinionInRange ? 0.55f : 0.15f;
        if (in.selfHpRatio <= in.retreatHpRatio + 0.10f)
            score += 0.15f;   // 저체력일수록 안전한 파밍을 선호
        return WintersMath::Clamp01(score);
    }

    f32_t StructureValue(const ValueInput& in)
    {
        return in.bStructureExposed ? 0.70f : 0.f;
    }

    f32_t TradeWindow(const ValueInput& in)
    {
        if (in.enemyDistance > in.attackRange)
            return 0.f;
        f32_t window = 0.5f;
        window += HealthLead(in) * 0.3f;
        window += EconomyLead(in) * 0.2f;
        window -= in.turretDanger * 0.5f;
        return WintersMath::Clamp01(window);
    }

    f32_t RetreatValue(const ValueInput& in)
    {
        const f32_t reengage = (std::max)(in.reengageHpRatio, 0.01f);
        const f32_t healthPressure = WintersMath::Clamp01(
            (reengage - in.selfHpRatio) / reengage);
        f32_t score = healthPressure * 0.75f +
            in.turretDanger * 0.75f * in.turretRiskWeight +
            WintersMath::Clamp01(in.incomingComboDamageRatio) * 0.65f;
        if (in.selfHpRatio <= in.retreatHpRatio)
            score = 1.f;
        return WintersMath::Clamp01(score);
    }

    UtilityScores BuildUtilityScores(const ValueInput& in)
    {
        const UtilityBreakdown breakdown = BuildUtilityBreakdown(in);
        UtilityScores scores{};
        scores.retreat = breakdown.retreat.score;
        scores.fight = breakdown.fight.score;
        scores.farm = breakdown.farm.score;
        scores.siege = breakdown.siege.score;
        return scores;
    }

    UtilityBreakdown BuildUtilityBreakdown(const ValueInput& in)
    {
        UtilityBreakdown out{};

        auto setTerm = [](CandidateBreakdown& candidate,
            u8_t index, f32_t rawValue, f32_t weight)
        {
            candidate.terms[index].rawValue = rawValue;
            candidate.terms[index].weight = weight;
            candidate.terms[index].contribution = rawValue * weight;
            candidate.termCount = (std::max)(
                candidate.termCount,
                static_cast<u8_t>(index + 1u));
        };
        auto closeCandidate = [&](CandidateBreakdown& candidate,
            u8_t adjustmentIndex, f32_t score)
        {
            f32_t sum = 0.f;
            for (u8_t i = 0u; i < adjustmentIndex; ++i)
                sum += candidate.terms[i].contribution;
            setTerm(candidate, adjustmentIndex, score - sum, 1.f);
            candidate.score = score;
        };

        const f32_t fightOpportunity =
            0.45f +
            (in.selfHpRatio - in.retreatHpRatio) * 0.30f +
            HealthLead(in) * 0.35f +
            ((in.enemyDistance <= in.engageRange) ? 0.20f : -0.10f) +
            (in.bAlliedWaveNearby ? 0.10f : 0.f) +
            EconomyLead(in) * 0.10f;
        setTerm(out.fight, 0u, fightOpportunity, in.fightUtilityWeight);
        setTerm(out.fight, 1u, in.turretDanger,
            -0.50f * in.turretRiskWeight * in.fightUtilityWeight);
        setTerm(out.fight, 2u,
            WintersMath::Clamp01(in.incomingComboDamageRatio),
            -0.45f * in.fightUtilityWeight);
        const f32_t fightScore = in.bEnemyChampionTargetable
            ? WintersMath::Clamp01(
                ChampionFightValue(in) * in.fightUtilityWeight)
            : 0.f;
        closeCandidate(out.fight, 3u, fightScore);

        const f32_t reengage = (std::max)(in.reengageHpRatio, 0.01f);
        const f32_t healthPressure = WintersMath::Clamp01(
            (reengage - in.selfHpRatio) / reengage);
        setTerm(out.retreat, 0u, healthPressure, 0.75f);
        setTerm(out.retreat, 1u, in.turretDanger,
            0.75f * in.turretRiskWeight);
        setTerm(out.retreat, 2u,
            WintersMath::Clamp01(in.incomingComboDamageRatio), 0.65f);
        closeCandidate(out.retreat, 3u, RetreatValue(in));

        const f32_t farmOpportunity = MinionFarmValue(in);
        setTerm(out.farm, 0u, farmOpportunity, in.farmUtilityWeight);
        closeCandidate(out.farm, 1u, WintersMath::Clamp01(
            farmOpportunity * in.farmUtilityWeight));

        const f32_t structureExposure = StructureValue(in);
        setTerm(out.siege, 0u, structureExposure, in.siegeUtilityWeight);
        closeCandidate(out.siege, 1u, WintersMath::Clamp01(
            structureExposure * in.siegeUtilityWeight));

        return out;
    }
}
