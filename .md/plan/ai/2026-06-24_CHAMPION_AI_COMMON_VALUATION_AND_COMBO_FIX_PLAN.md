Session - 모든 봇 공통 판단 뼈대(ChampionAIValuation, 골드 단위 가치 평가)를 도입하고, 콤보 교착 버그와 LeeSin 콤보 R 누락 버그를 수정한다.

배경: 사일러스/리신 콤보·패시브 BA·ward·이펙트(wfx/png) 반영은 검토에서 대부분 정상 확인됨. 두 가지 실제 동작 버그(콤보 교착, LeeSin stepCount off-by-one)와 공통 평가 뼈대 부재만 본 세션에서 처리한다. 이펙트/wfx/png는 수정 대상 아님.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.cpp

LeeSin 콤보 배열은 9개 step(Q, Q2, BA, E, BA, E2, Ward, W-hop, R)인데 stepCount가 8로 잘못 지정되어 마지막 R(index 8)이 `comboStep % stepCount`에서 영원히 선택되지 않는다.

`static constexpr ChampionAIComboPlan s_LeeSin` 정의의 마지막 부분에서 기존 코드:

cpp

            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::R), 0, 0.f, 3.0f, 0.35f, 0.70f },
        },
        8
    };

아래로 교체:

cpp

            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::R), 0, 0.f, 3.0f, 0.35f, 0.70f },
        },
        9
    };

1-2. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp (콤보 교착 수정)

`TryEmitAttackChampionCombo`에서 학습된 step이 조건 불충족(쿨다운/스테이지/강탈 불가/override 미활성)일 때, 적이 사거리 안에 있으면 comboStep을 advance하지 않고 그 인덱스에 정지해 콤보가 교착된다. 사거리 안이면 다음 step으로 진행하도록 바꾼다.

기존 코드:

cpp

                if (maxRange <= 0.f || ctx.enemyDistance <= maxRange)
                    return bWasActive;

아래로 교체:

cpp

                if (maxRange <= 0.f || ctx.enemyDistance <= maxRange)
                {
                    // 사거리 안인데 조건 불충족(쿨다운/스테이지/강탈/override 등):
                    // 정지 대신 다음 step으로 진행해 콤보가 교착되지 않게 한다.
                    const u8_t nextComboStep =
                        static_cast<u8_t>((index + 1u) % stepCount);
                    ai.comboStep = nextComboStep;
                    if (nextComboStep == 0u)
                        CompleteChampionAICombo(ai, ctx);
                    return bWasActive;
                }

1-3. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIValuation.h

새 파일 (모든 봇 공통 판단 뼈대):

cpp

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

1-4. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIValuation.cpp

새 파일:

cpp

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
        score -= in.turretDanger * 0.50f;                                // 포탑 위험
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
}

1-5. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp (공통 뼈대 통합)

1-5-1. include 추가. 기존 코드:

cpp

#include "Shared/GameSim/Components/ChampionScore.h"

아래에 추가:

cpp

#include "Shared/GameSim/Components/GoldComponent.h"

1-5-2. include 추가. 기존 코드:

cpp

#include "Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h"

아래에 추가:

cpp

#include "Shared/GameSim/Systems/ChampionAI/ChampionAIValuation.h"

1-5-3. `struct ChampionAIContext`에 골드/레벨 필드를 추가한다. 기존 코드:

cpp

        f32_t selfHpRatio = 1.f;
        f32_t enemyHpRatio = 1.f;
        f32_t enemyDistance = 999.f;

아래로 교체:

cpp

        f32_t selfHpRatio = 1.f;
        f32_t enemyHpRatio = 1.f;
        f32_t selfGold = 0.f;
        f32_t enemyGold = 0.f;
        u8_t  selfLevel = 1;
        u8_t  enemyLevel = 1;
        f32_t enemyDistance = 999.f;

1-5-4. `BuildChampionAIContext`에서 골드/레벨을 채운다. enemyChampion 해석 직후 기존 코드:

cpp

        ctx.enemyChampion = targetChampion;
        if (targetChampion != NULL_ENTITY)
        {
            const f32_t distSq = WintersMath::DistanceSqXZ(selfPos, targetChampionPos);
            ctx.enemyDistance = std::sqrt(std::max(0.f, distSq));
            ctx.enemyHpRatio = HealthRatio(world, targetChampion);
        }

아래에 추가:

cpp

        if (world.HasComponent<GoldComponent>(self))
            ctx.selfGold = static_cast<f32_t>(world.GetComponent<GoldComponent>(self).amount);
        if (world.HasComponent<StatComponent>(self))
            ctx.selfLevel = world.GetComponent<StatComponent>(self).level;
        if (targetChampion != NULL_ENTITY)
        {
            if (world.HasComponent<GoldComponent>(targetChampion))
                ctx.enemyGold =
                    static_cast<f32_t>(world.GetComponent<GoldComponent>(targetChampion).amount);
            if (world.HasComponent<StatComponent>(targetChampion))
                ctx.enemyLevel = world.GetComponent<StatComponent>(targetChampion).level;
        }

1-5-5. `UpdateChampionAIDecisionEvidence`의 점수 계산을 공통 뼈대 호출로 교체한다. 기존 코드:

cpp

        ai.fFarmDecisionScore = ctx.enemyMinion != NULL_ENTITY ? 0.55f : 0.15f;
        if (ctx.selfHpRatio <= ai.retreatHpRatio + 0.10f)
            ai.fFarmDecisionScore += 0.15f;

        ai.fStructureDecisionScore =
            (ctx.enemyChampion == NULL_ENTITY &&
                ctx.enemyStructure != NULL_ENTITY &&
                ctx.alliedWave != NULL_ENTITY &&
                ctx.bStructureWaveTanking)
            ? 0.70f
            : 0.f;

        if (!ai.bCanAttackChampion)
        {
            ai.fChampionDecisionScore = 0.f;
            return;
        }

        f32_t championScore = 0.45f;
        championScore += (ctx.selfHpRatio - ai.retreatHpRatio) * 0.30f;
        championScore += (ctx.selfHpRatio - ctx.enemyHpRatio) * 0.35f;
        championScore += (ctx.enemyDistance <= ctx.attackRange) ? 0.20f : -0.10f;
        championScore += ctx.bAlliedWaveNearby ? 0.10f : 0.f;
        championScore -= ctx.turretDanger * 0.50f;

        if (ai.fPostComboBATimer > 0.f)
        {
            ai.bPostComboBAAllowed = ShouldContinueBasicAttackAfterCombo(ai, ctx);
            if (ai.bPostComboBAAllowed)
                championScore = 1.f;
        }

        ai.fChampionDecisionScore = WintersMath::Clamp01(championScore);
        ai.fFarmDecisionScore = WintersMath::Clamp01(ai.fFarmDecisionScore);
        ai.fStructureDecisionScore = WintersMath::Clamp01(ai.fStructureDecisionScore);

아래로 교체:

cpp

        ChampionAIValuation::ValueInput vin{};
        vin.selfHpRatio = ctx.selfHpRatio;
        vin.enemyHpRatio = ctx.enemyHpRatio;
        vin.selfGold = ctx.selfGold;
        vin.enemyGold = ctx.enemyGold;
        vin.selfLevel = ctx.selfLevel;
        vin.enemyLevel = ctx.enemyLevel;
        vin.enemyDistance = ctx.enemyDistance;
        vin.attackRange = ctx.attackRange;
        vin.turretDanger = ctx.turretDanger;
        vin.retreatHpRatio = ai.retreatHpRatio;
        vin.bAlliedWaveNearby = ctx.bAlliedWaveNearby;
        vin.bEnemyMinionInRange = (ctx.enemyMinion != NULL_ENTITY);
        vin.bStructureExposed =
            (ctx.enemyChampion == NULL_ENTITY &&
                ctx.enemyStructure != NULL_ENTITY &&
                ctx.alliedWave != NULL_ENTITY &&
                ctx.bStructureWaveTanking);

        ai.fFarmDecisionScore = ChampionAIValuation::MinionFarmValue(vin);
        ai.fStructureDecisionScore = ChampionAIValuation::StructureValue(vin);

        if (!ai.bCanAttackChampion)
        {
            ai.fChampionDecisionScore = 0.f;
            return;
        }

        f32_t championScore = ChampionAIValuation::ChampionFightValue(vin);

        if (ai.fPostComboBATimer > 0.f)
        {
            ai.bPostComboBAAllowed = ShouldContinueBasicAttackAfterCombo(ai, ctx);
            if (ai.bPostComboBAAllowed)
                championScore = 1.f;
        }

        ai.fChampionDecisionScore = WintersMath::Clamp01(championScore);

1-6. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Include/GameSim.vcxproj

새 ChampionAIValuation.cpp를 GameSim 라이브러리 프로젝트에 등록한다(Server/Client가 이 라이브러리를 참조하므로 별도 등록 불필요).

기존 코드:

cpp

    <ClCompile Include="..\Systems\ChampionAI\ChampionAIBrain.cpp" />

아래에 추가:

cpp

    <ClCompile Include="..\Systems\ChampionAI\ChampionAIValuation.cpp" />

2. 검증

미검증:
- 런타임에서 봇이 콤보 전체(특히 LeeSin R, Sylas R 강탈/사용)를 끝까지 진행하는지 미검증
- 경제 우위 항 추가 후 봇 교전 공격성이 의도대로 바뀌는지 미검증

검증 명령:
- git diff --check
- MSBuild Server.vcxproj /p:Configuration=Debug /p:Platform=x64
- MSBuild Client.vcxproj /p:Configuration=Debug /p:Platform=x64

확인 필요:
- ChampionAIValuation.cpp가 GameSim.vcxproj에 포함되어 GameSim 라이브러리로 빌드되는지 확인.

설계 메모:
- ChampionFightValue/MinionFarmValue/StructureValue는 기존 점수식과 동일한 값을 내도록 보존했고, 유일한 동작 변화는 ChampionFightValue에 EconomyLead × 0.10 항이 추가된 것이다. 기존 봇 동작을 크게 바꾸지 않으면서 골드/레벨 차원을 공통 뼈대로 노출한다.
- TradeWindow는 본 세션에서 점수에 직접 연결하지 않고 공통 뼈대 API로만 제공한다(향후 brain/콤보 진입 판단에서 사용).
