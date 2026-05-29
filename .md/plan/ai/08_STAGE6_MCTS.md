# Stage 6 — MCTS (Monte Carlo Tree Search)

## 목표

**단기 교전 시뮬레이션** — 1:1, 2:2, 소규모 전투에서 "몇 수 앞" 을 시뮬해 최선의 액션 선정.

## 왜 MCTS 인가

- 체스/바둑/MOBA AI 의 핵심 알고리즘 (AlphaGo, AlphaStar)
- 가능한 액션이 너무 많을 때 (평타/Q/W/E/R/이동방향 × 거리) Brute Force 대신 통계적 탐색
- 휴리스틱 없이도 수만 회 시뮬레이션으로 좋은 수 찾음
- 짧은 시간 제한 (예: 100ms) 에서 점진적 개선 가능 (Anytime Algorithm)

## Phase E 와의 관계

**몬테카를로 적분 (Phase E Stage 3) 과 수학적 공유**:
- 확률 분포 샘플링
- 분산 감소 (UCB1 은 탐색-활용 균형)
- 난수 발생기 (Stage 3 LDS 재활용 가능)

## 알고리즘

```
while (time_budget > 0):
    1. Selection    : 루트부터 UCB1 최대 노드 선택하여 리프까지 내려감
    2. Expansion    : 리프에서 가능한 액션 하나 추가 (새 자식 노드)
    3. Rollout      : 새 노드에서 무작위 플레이아웃 → 결과 (승/패)
    4. Backpropagation: 리프부터 루트까지 결과 업데이트

return root.bestChild().action
```

## 노드 구조

```cpp
struct MCTSNode
{
    u32_t       parentIdx     = INVALID;
    std::vector<u32_t> childrenIdx;

    u32_t       action        = 0;       // 이 노드로 오게 한 액션
    BotAction   actionDetail;            // 구체적 정보 (누구에게, 어디로)

    u32_t       visits        = 0;
    f32_t       totalValue    = 0.f;     // 누적 보상

    bool_t      isFullyExpanded = false;
    bool_t      isTerminal    = false;
    f32_t       terminalValue = 0.f;

    f32_t AverageValue() const { return visits > 0 ? totalValue / visits : 0.f; }
};

class CMCTSTree
{
public:
    void Reset(const BattleState& rootState);

    // 한 번의 MCTS 반복
    void Iterate(u32_t iterations);

    // 가장 많이 방문된 자식의 액션 반환
    BotAction GetBestAction() const;

private:
    std::vector<MCTSNode> m_pool;
    u32_t                 m_rootIdx = 0;
    BattleState           m_rootState;

    u32_t Select();
    u32_t Expand(u32_t nodeIdx);
    f32_t Rollout(const BattleState& state);
    void  Backpropagate(u32_t nodeIdx, f32_t value);
};
```

## UCB1 (Selection 공식)

```
UCB1(child) = avgValue(child) + C × √(ln(parent.visits) / child.visits)
```

C = 탐색 상수. 작으면 활용(exploit), 크면 탐색(explore). 일반적으로 √2 ≈ 1.41.

## 교전 상태 추상화 (BattleState)

```cpp
struct BattleState
{
    struct Unit {
        u32_t       championID;
        f32_t       hp, maxHP;
        f32_t       mana;
        Vec3        position;
        f32_t       cooldowns[6];         // Q/W/E/R/Flash/Ignite
        u32_t       statusEffects;        // bitset (Stun/Silence/Root/Slow)
        f32_t       attackDamage, abilityPower, armor, magicResist;
    };

    std::array<Unit, 4> myTeam;            // 최대 2v2 까지 MCTS
    std::array<Unit, 4> enemyTeam;
    u32_t   myCount, enemyCount;

    f32_t   elapsedTime = 0.f;
};
```

5v5 한타 전체 MCTS 는 상태 공간 폭발 — 초기엔 1v1/2v2 만 지원.

## 가능한 액션 (Action Space)

```cpp
enum class BotActionType : u8_t {
    MoveForward, MoveBackward, MoveLeft, MoveRight,   // 4방향 이동
    CastQ, CastW, CastE, CastR,                       // 스킬 4개
    BasicAttack,                                      // 평타
    UseFlash, UseIgnite,                              // 소환사 주문
    Idle                                              // 아무것도 안 함
};
```

이동은 8방향 또는 연속적 각도 (상태 폭발 방지를 위해 이산화).

## Rollout (간이 시뮬레이터)

실제 게임 로직 전체가 아닌 **간이 근사**:

```cpp
f32_t CMCTSTree::Rollout(const BattleState& state)
{
    BattleState sim = state;
    i32_t steps = 0;
    constexpr i32_t MAX_STEPS = 100;    // ~5초 시뮬

    while (steps++ < MAX_STEPS) {
        if (IsTerminal(sim)) break;

        // 양 팀 랜덤 액션 (또는 간단한 휴리스틱)
        for (auto& unit : sim.myTeam)    ApplyRandomAction(sim, unit);
        for (auto& unit : sim.enemyTeam) ApplyRandomAction(sim, unit);

        SimulateStep(sim, 0.05f);       // 50ms tick
    }

    return EvaluateOutcome(sim);        // HP 총합 차이 등
}
```

## 결과 평가 (Reward)

```
value = 
    Σ my_hp_remaining - Σ enemy_hp_remaining
    + 100 × (kills - deaths)
    + time_alive_bonus
    normalized to [-1, 1]
```

## 시간 예산

- 교전 시작 첫 프레임: 100ms 투자 (5000 반복 가능)
- 교전 지속 중: 매 틱 10ms 시뮬 (500 반복)
- 대기 상태: MCTS off

## 휴리스틱 개선

### Progressive Widening

자식 노드 수를 방문 수에 따라 제한 → 가지 폭발 방지:
```
numChildren = ⌈C × visits^0.5⌉
```

### RAVE (Rapid Action Value Estimation)

같은 액션이 다른 깊이에서 쓰였을 때 값 공유. 수렴 속도 개선.

### Heavy Playout (스마트 Rollout)

순수 랜덤 대신 간단한 휴리스틱 (가까우면 평타, 멀면 이동) 적용.
단순 랜덤 대비 5~10배 수렴 빠름.

## 난이도별 사용

| 난이도 | MCTS 사용 | 반복 수 |
|---|---|---|
| Intro | ❌ | 0 |
| Beginner | ❌ | 0 |
| Intermediate | ✅ | 500/tick |
| Master | ✅✅ | 5000/tick |
| Grandmaster | ✅✅ + NN 정책 prior | 10000/tick |

## 구현 순서

1. `BattleState` 구조체 + 복사 가능 (Rollout 마다 복사됨)
2. `MCTSNode` + 노드 풀 (벡터 재사용)
3. Selection (UCB1)
4. Expansion
5. 간이 시뮬레이터 (`SimulateStep`)
6. Rollout (순수 랜덤 먼저)
7. Backpropagation
8. 시간 예산 기반 Iterate
9. Heavy Playout 휴리스틱
10. ImGui 에서 MCTS 트리 시각화 (루트 + 자식들의 방문 수/평균값)

## 검증

- 봇 vs 덤벼드는 봇 1v1: Master MCTS 봇이 랜덤 봇 90%+ 승률
- 사용자 vs Master 봇 1v1: 인간이 처음엔 2~3번 연패, 적응 후 비등
- 교전 시뮬 1000회 평균 20ms 내 완료
