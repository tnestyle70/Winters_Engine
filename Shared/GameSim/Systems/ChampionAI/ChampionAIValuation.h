#pragma once

#include "Shared/GameSim/Registries/Reward/RewardRegistry.h"
#include "WintersTypes.h"

// ─────────────────────────────────────────────────────────────
// ChampionAI Valuation — 모든 봇의 공통 판단 뼈대
//
// 모든 가치를 "골드"로 환산해 한 곳에서 평가한다.
//  - 챔피언 가치  = 킬 골드 × 체력 비율 (아직 안 뺏긴 킬 골드)
//  - 미니언 가치  = 막타 골드
//  - 포탑 가치    = 시즈 골드
//  - 경제 우위    = 골드 차 + 레벨 차의 골드 등가
//  - 딜교환 타이밍 = 사거리 안에서 체력/경제 우위와 포탑 안전의 결합
//
// brain(RuleBased/PlayerLike/Decision)이 공유하는 단일 평가 소스다.
// 결정적(deterministic): 입력은 평탄화된 수치뿐, 난수·시간 없음.
// ─────────────────────────────────────────────────────────────

namespace ChampionAIValuation
{
    // RewardRegistry 실측치를 직접 조회하는 골드 기준값 (미등록 시 레거시 상수 폴백).
    inline f32_t GetChampionKillGoldValue()
    {
        const RewardDef* pReward =
            CRewardRegistry::Instance().FindReward(eRewardSourceKind::Champion);
        return pReward ? pReward->gold.killerGold : 300.f;
    }

    inline f32_t GetMeleeMinionGoldValue()
    {
        const RewardDef* pReward = CRewardRegistry::Instance().FindReward(
            eRewardSourceKind::Minion, static_cast<u8_t>(eMinionRewardKind::Melee));
        return pReward ? pReward->gold.killerGold : 21.f;
    }

    inline f32_t GetRangedMinionGoldValue()
    {
        const RewardDef* pReward = CRewardRegistry::Instance().FindReward(
            eRewardSourceKind::Minion, static_cast<u8_t>(eMinionRewardKind::Ranged));
        return pReward ? pReward->gold.killerGold : 14.f;
    }

    inline f32_t GetTurretGoldValue()
    {
        const RewardDef* pReward =
            CRewardRegistry::Instance().FindReward(eRewardSourceKind::Turret);
        return pReward ? pReward->gold.killerGold : 250.f;
    }

    constexpr f32_t kGoldLeadFullScale = 1000.f;  // 경제차 1000골드 = 우위 만점
    constexpr f32_t kLevelGoldValue = 120.f;      // 레벨 1 ≈ 120골드 가치 근사

    // 시스템이 ctx에서 평탄화해 전달하는 공통 판단 입력
    struct ValueInput
    {
        f32_t selfHpRatio = 1.f;
        f32_t enemyHpRatio = 1.f;
        f32_t selfGold = 0.f;
        f32_t enemyGold = 0.f;
        u8_t  selfLevel = 1;
        u8_t  enemyLevel = 1;
        f32_t enemyDistance = 999.f;
        f32_t attackRange = 1.5f;
        f32_t engageRange = 1.5f;
        f32_t turretDanger = 0.f;
        f32_t incomingComboDamageRatio = 0.f;
        f32_t retreatHpRatio = 0.10f;
        f32_t reengageHpRatio = 0.25f;
        f32_t fightUtilityWeight = 1.f;
        f32_t farmUtilityWeight = 1.f;
        f32_t siegeUtilityWeight = 1.f;
        f32_t turretRiskWeight = 1.f;
        bool_t bEnemyChampionTargetable = false;
        bool_t bAlliedWaveNearby = false;
        bool_t bEnemyMinionInRange = false;
        bool_t bStructureExposed = false;
    };

    // 1. 경제(골드+레벨) 우위 → [-1, 1]
    f32_t EconomyLead(const ValueInput& in);
    // 2. 체력(딜교환) 우위 → [-1, 1]
    f32_t HealthLead(const ValueInput& in);
    // 3. 교전(챔피언 공격) 가치 → [0, 1]
    f32_t ChampionFightValue(const ValueInput& in);
    // 4. 파밍(미니언) 가치 → [0, 1]
    f32_t MinionFarmValue(const ValueInput& in);
    // 5. 시즈(포탑) 가치 → [0, 1]
    f32_t StructureValue(const ValueInput& in);
    // 6. 딜교환 타이밍(지금 들어갈 때인가) → [0, 1]
    f32_t TradeWindow(const ValueInput& in);

    struct UtilityScores
    {
        f32_t retreat = 0.f;
        f32_t fight = 0.f;
        f32_t farm = 0.f;
        f32_t siege = 0.f;
    };

    struct UtilityTerm
    {
        f32_t rawValue = 0.f;
        f32_t weight = 0.f;
        f32_t contribution = 0.f;
    };

    struct CandidateBreakdown
    {
        f32_t score = 0.f;
        UtilityTerm terms[4]{};
        u8_t termCount = 0u;
    };

    struct UtilityBreakdown
    {
        CandidateBreakdown retreat{};
        CandidateBreakdown fight{};
        CandidateBreakdown farm{};
        CandidateBreakdown siege{};
    };

    f32_t RetreatValue(const ValueInput& in);
    UtilityBreakdown BuildUtilityBreakdown(const ValueInput& in);
    UtilityScores BuildUtilityScores(const ValueInput& in);
}
