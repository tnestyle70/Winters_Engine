# 공통 — 난이도 프리셋

## 목표

동일한 봇 엔진에서 **난이도별로 다르게 행동**. 5단계 프리셋.

## 난이도 테이블

| 난이도 | APM 상한 | 반응 시간 | 실수 주입 | MCTS 깊이 | 활성 Stage | 대표 상대 |
|---|---|---|---|---|---|---|
| **Intro** | 60 | 400ms | 높음 | off | 0~2 | 초보 플레이어 |
| **Beginner** | 90 | 300ms | 중간 | off | 0~3 | 브론즈~실버 |
| **Intermediate** | 150 | 200ms | 낮음 | 3수 | 0~5 | 골드 |
| **Master** | 250 | 100ms | 미세 | 5수 | 0~7 전부 | 플래티넘 |
| **Grandmaster** | 400 | 50ms | 없음 | 7수 | 0~8 (RL 전용) | 다이아+ |

## 파라미터 세부

### APM (Actions Per Minute) 제한

```cpp
struct APMLimiter
{
    f32_t  maxAPM = 150.f;
    f32_t  cooldownPerAction = 60.f / 150.f;  // 0.4초

    bool   CanActNow();
    void   RecordAction();

private:
    f32_t  m_lastActionTime = 0.f;
};
```

과거 N 초간 액션 수 제한 → 로봇처럼 0ms 반응 X.

### 반응 시간 (Reaction Delay)

자극 감지 → 실제 액션까지 지연:

```cpp
struct ReactionDelay
{
    f32_t  baseMs       = 200.f;
    f32_t  varianceMs   = 50.f;    // 가우시안 분산

    f32_t  Sample();
};
```

예시: 적이 Flash 로 붙음 → 봇이 감지 → **0.2 ± 0.05초** 후 Q 회피.

### 실수 주입 (Mistake Injection)

사람답게 가끔 실수:

| 실수 종류 | Intro | Beg | Int | Master |
|---|---|---|---|---|
| 스킬 빗나감 (랜덤 오차 각도) | ±15° | ±10° | ±5° | ±1° |
| 평타 미스 (대상 놓치기) | 10% | 5% | 2% | 0.5% |
| CS 실수 (막타 놓치기) | 30% | 20% | 10% | 3% |
| 잘못된 스킬 순서 | 15% | 8% | 2% | 0% |
| 지나친 밀기 (용감함) | +30% | +15% | 0 | -10% |
| Flash 낭비 | 5% | 2% | 0.5% | 0% |

### Influence Map 활용

| 난이도 | 맵 인식 |
|---|---|
| Intro | off (보이는 시야만) |
| Beginner | Team Influence 만 |
| Intermediate | Team + Threat |
| Master | 전체 레이어 |
| Grandmaster | 전체 + Opportunity 예측 (초 단위) |

### MCTS 설정

| 난이도 | 시간 예산/결정 | 반복 수 |
|---|---|---|
| Intro | 0 | 0 |
| Beginner | 0 | 0 |
| Intermediate | 20ms | 500 |
| Master | 100ms | 5000 |
| Grandmaster | 100ms | 10000 (+ NN prior) |

## 구현

```cpp
// Engine/Public/AI/Core/AIConfig.h
struct AIConfig
{
    f32_t  maxAPM           = 150.f;
    f32_t  reactionBaseMs   = 200.f;
    f32_t  reactionVarMs    = 50.f;

    f32_t  skillAimErrorDeg = 5.f;
    f32_t  basicAttackMiss  = 0.02f;
    f32_t  csSkipChance     = 0.10f;
    f32_t  wrongSkillChance = 0.02f;
    f32_t  overAggression   = 0.f;

    bool_t useInfluenceMap  = true;
    i32_t  influenceLayers  = 2;    // 0=off, 1=team, 2=+threat, 3=+opportunity, 4=+vision

    i32_t  mctsIterations   = 0;
    bool_t useNeuralPolicy  = false;

    static AIConfig Preset_Intro();
    static AIConfig Preset_Beginner();
    static AIConfig Preset_Intermediate();
    static AIConfig Preset_Master();
    static AIConfig Preset_Grandmaster();
};
```

각 봇의 `BotComponent` 에 `AIConfig` 포인터/ID 참조.

## 동적 난이도 조절 (DDA, Dynamic Difficulty Adjustment)

봇전에서 아군 플레이어가 계속 지고 있으면 봇팀 자동 하향:

```cpp
class CDynamicDifficultyManager
{
public:
    void UpdateTeamBalance(CWorld& world, f32_t dt);

private:
    // 골드/킬/오브젝트 차이 기반 조절
    void AdjustBotDifficulty(CBotComponent& bot, f32_t playerTeamStrength);
};
```

- Master 기준에서 ± 2단계까지만 허용
- 급격한 변화 방지 (gradual interpolation)
- 옵션으로 on/off

## 챔피언별 차등

같은 Master 난이도여도 챔피언별 강점/약점 유지:

| 챔피언 | 난이도 특화 |
|---|---|
| Irelia | CS/Trade 과제 쉬움, Roaming 어려움 → CS 실수 낮음, Roaming 타이밍 보정 |
| Yasuo | 기술 의존도 높음 → 스킬 타이밍 정확, 대신 포지셔닝 실수 허용 |
| Sylas | 궁 훔치기 판단 → Utility 점수 특화 (훔칠 적 궁 비교) |
| Viego | 시체 빙의 → 킬 후 즉시 빙의 판단 (반응 시간 중요) |
| Kalista | 발구름 기동 → APM 소모 큼 (상한에 자주 걸림) |

## 난이도 별 성향 (Personality)

같은 점수 경합에서 약간씩 다른 선호:

| 성향 | 공격 vs 안전 | 오브젝트 vs 킬 | 라인전 vs 로밍 |
|---|---|---|---|
| Aggressive | +0.2 공격 | +0.1 킬 | +0.15 로밍 |
| Passive | -0.2 공격 | -0.1 킬 | +0.15 라인 |
| Objective | 중립 | +0.3 오브젝트 | 중립 |
| Killer | +0.15 공격 | +0.25 킬 | +0.1 로밍 |
| Farmer | -0.1 공격 | -0.15 킬 | +0.3 라인 |

난이도 × 성향 조합 = 5 × 5 = 25 가지 개성.

## ImGui 디버깅/조작

연습모드 에디터에서 실시간 변경:

```
[AI Config — Bot #3 Yasuo]
Difficulty: [Master ▼]       ← 드롭다운
Personality: [Aggressive ▼]
APM max:           [250    ] slider
Reaction base:     [100 ms ] slider
Skill aim error:   [1.0°   ] slider
Basic attack miss: [0.5%   ] slider
MCTS iterations:   [5000   ] slider
[Reset to Preset]
```

## 구현 순서

1. `AIConfig` 구조체 + 5 프리셋 함수
2. `APMLimiter` + `ReactionDelay` 기본 기능
3. 실수 주입 (스킬 각도 랜덤화, CS 실수)
4. 난이도별 Stage on/off 스위치
5. DDA 매니저 (옵션)
6. 챔피언별 특화 파라미터
7. 성향 시스템
8. ImGui 에디터 연동
