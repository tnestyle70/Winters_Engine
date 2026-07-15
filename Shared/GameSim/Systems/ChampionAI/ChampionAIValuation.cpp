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
        score += (in.enemyDistance <= in.attackRange) ? 0.20f : -0.10f;  // 사거리
        score += in.bAlliedWaveNearby ? 0.10f : 0.f;                     // 백업 웨이브
        score += EconomyLead(in) * 0.10f;                                // 경제 우위(신규)
        score -= in.turretDanger * 0.50f * in.turretRiskWeight;          // 포탑 위험
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
            in.turretDanger * 0.45f * in.turretRiskWeight;
        if (in.selfHpRatio <= in.retreatHpRatio)
            score = 1.f;
        return WintersMath::Clamp01(score);
    }

    UtilityScores BuildUtilityScores(const ValueInput& in)
    {
        UtilityScores scores{};
        scores.retreat = RetreatValue(in);
        scores.fight = in.bEnemyChampionTargetable
            ? WintersMath::Clamp01(
                ChampionFightValue(in) * in.fightUtilityWeight)
            : 0.f;
        scores.farm = WintersMath::Clamp01(
            MinionFarmValue(in) * in.farmUtilityWeight);
        scores.siege = WintersMath::Clamp01(
            StructureValue(in) * in.siegeUtilityWeight);
        return scores;
    }
}
