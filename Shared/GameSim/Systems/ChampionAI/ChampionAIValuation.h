#pragma once

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
    // RewardRegistry 실측치와 정렬한 골드 기준값
    constexpr f32_t kChampionKillGold = 300.f;
    constexpr f32_t kMeleeMinionGold = 21.f;
    constexpr f32_t kRangedMinionGold = 14.f;
    constexpr f32_t kTurretGold = 250.f;
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
        f32_t turretDanger = 0.f;
        f32_t retreatHpRatio = 0.10f;
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
}
