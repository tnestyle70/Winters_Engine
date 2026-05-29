# Stage 4 — Utility AI

## 목표

같은 상황에서 **여러 행동 후보**의 점수를 계산해 최고점 선택. Dave Mark 의
"Behavioral Mathematics for Game AI" 방법론.

## 왜 Utility 인가

- FSM/BT 는 if-else 분기 → 조건 추가할수록 관리 비용 폭발
- GOAP 는 여러 액션의 순서를 짜지만 "어떤 **Goal** 을 집을지" 는 별도 판단 필요
- Utility 는 연속적 입력 → 연속적 점수 → **부드러운 의사결정** (동점 방지)
- The Sims / XCOM / RimWorld 방식

## 기본 공식

```
Score(action) = Σ weight_i × responseCurve_i(input_i)
```

예:
```
Score(GankBot) = 
    0.3 × Linear(1 / distance_to_bot_lane, max=20m)
  + 0.3 × Sigmoid(enemyADC_hp_missing, threshold=0.4)
  + 0.2 × Quadratic(my_mana_percent, power=2)
  + 0.1 × Linear(time_since_last_gank_bot, max=180s)
  + 0.1 × Bool(enemy_flash_cooldown > 60s)
  - 0.2 × Linear(my_hp_missing)
```

각 입력을 0~1 정규화 후 응답 곡선 적용, 가중 합.

## 응답 곡선 (Response Curves)

```cpp
enum class CurveType : u8_t {
    Linear,         // y = x
    Quadratic,      // y = x²
    Cubic,          // y = x³
    InverseQuad,    // y = 1 - (1-x)²
    Sigmoid,        // y = 1 / (1 + e^(-k(x-0.5)))
    Logistic,       // S-curve
    Threshold,      // y = (x > t) ? 1 : 0
    Bell,           // y = e^(-k(x-center)²)
    SmoothStep      // y = 3x² - 2x³
};

class CResponseCurve
{
public:
    static f32_t Evaluate(CurveType type, f32_t x, f32_t param1 = 0, f32_t param2 = 0);
};
```

## Goal 우선순위 평가 (GOAP 연동)

```cpp
// Engine/Public/AI/Utility/DecisionMaker.h
class CDecisionMaker
{
public:
    struct ScoredAction {
        u32_t actionID;
        f32_t score;
        std::string debug;   // ImGui 디버거용 점수 브레이크다운
    };

    ScoredAction ChooseBest(const std::vector<ScoredAction>& candidates);

    // 상위 N 개 중 랜덤 (예측 방지)
    ScoredAction ChooseTopN(const std::vector<ScoredAction>& candidates, u32_t N, f32_t randomness);
};
```

## Goal 점수 계산 예시 (봇 Top 라이너)

```cpp
f32_t EvaluateGoal_PushTopLane(const BotContext& ctx)
{
    f32_t score = 0.f;

    // 탑 라인 미니언 밀도 (가까우면 좋음)
    score += 0.25f * CResponseCurve::Evaluate(
        CurveType::Linear, ctx.waveNearTopTower_dist_normalized);

    // 상대 탑솔러 죽어 있으면 매우 좋음
    score += 0.30f * (ctx.enemyTop_isAlive ? 0.f : 1.f);

    // 내 HP/마나 충분
    score += 0.15f * CResponseCurve::Evaluate(
        CurveType::Sigmoid, ctx.my_hp_percent, /*k=*/8, /*threshold=*/0.5);

    // 우리 정글 오브젝트 밀리는 중이면 양보
    score -= 0.20f * (ctx.baron_contested ? 1.f : 0.f);

    // 상대 탑솔 Flash 없으면 다이브 가능
    score += 0.10f * (ctx.enemyTop_flashCooldown > 60.f ? 1.f : 0.f);

    return std::clamp(score, 0.f, 1.f);
}
```

## 중요한 용례들

| 의사결정 | Utility 사용? |
|---|---|
| 어느 라인 갱갈까? | ✅ 5 라인 점수 비교 |
| 어느 오브젝트 먹을까? | ✅ 드래곤/바론/전령 비교 |
| 어떤 스킬 쏠까? | 보통 BT 로 충분 (트리거 명확) |
| 어떤 적을 targeting 할까? | ✅ 10 적 점수 비교 |
| 어떤 아이템 살까? | ✅ 빌드 가짓수 비교 |
| 언제 귀환할까? | ✅ HP/마나/골드 임계 |
| FSM 루트 전이 | ✅ 새 상태 점수 > 현재 상태 점수 + hysteresis |

## Hysteresis (히스테리시스)

점수 근처에서 상태가 왔다갔다 치는 것 방지. **현재 상태 유지 가산점** 추가:

```cpp
f32_t score_currentState = baseScore + 0.15f;  // 현재 유지하는 게 더 매력적
```

## ImGui Utility 디버거

```
[Yasuo #3 — Utility Inspector]
Selected: FarmMidLane (score: 0.68)
---
Candidates:
  FarmMidLane         0.68  │ wave=0.8  hp=0.7  safe=0.6
  GankTopLane         0.45  │ dist=0.4  ally=0.6  mana=0.5
  RecallAndBuy        0.32  │ gold=1.0  hp=0.5  time=0.2
  TakeScuttle         0.28  │ dist=0.2  contest=0.5  hp=0.7
  DefendBotTower      0.15  │ enemy=0.1  dist=0.3
```

## 학습 기반 가중치 튜닝 (선택)

- 각 Goal 의 weight_i 는 처음엔 수동 설정
- Stage 7 모방 학습 데이터로 회귀 분석 → 플레이어 행동 빈도 매칭하도록 weight 자동 조정
- 난이도별 가중치 다르게 (Intro 는 공격 가중치 낮게)

## 구현 순서

1. `CResponseCurve` 8종 곡선 구현
2. `ScoreCalculator` — weight 배열 + input 배열 → 가중합
3. 10 개 핵심 Goal 점수 함수 (위 예시 수준)
4. `CDecisionMaker::ChooseBest` + `ChooseTopN`
5. GOAP 의 Goal 선정에 Utility 연동
6. HFSM 루트 전이에 Utility 연동
7. ImGui 디버거 (점수 breakdown)
8. 챔피언별 weight 프리셋
