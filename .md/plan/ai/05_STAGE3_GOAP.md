# Stage 3 — GOAP (Goal-Oriented Action Planning)

## 목표

**장기 계획**을 자동 생성. A* 알고리즘으로 목표 상태까지 최소 비용 액션 시퀀스 탐색.

## 왜 GOAP 인가

- HFSM/BT 는 "지금 뭐 할지"만 처리. 여러 단계 필요한 목표는 하드코딩 필요
- GOAP 는 액션 + 전후조건 선언 → 플래너가 순서 자동 계획
- 챔피언별 커스텀 트리 없이도 새 아이템/스킬 추가 시 재계획 자동 적용
- Jeff Orkin, F.E.A.R. 게임 (2005) 로 유명

## 월드 상태 (WorldState)

```cpp
// 봇이 인식하는 월드의 key-value 상태 벡터
struct WorldState
{
    // 자원
    f32_t   gold        = 0.f;
    f32_t   hp          = 1.f;       // 비율
    f32_t   mana        = 1.f;
    i32_t   level       = 1;
    i32_t   itemSlots[6] = {0};
    
    // 위치
    Vec3    position;
    bool_t  isAtBase    = false;
    bool_t  isAtShop    = false;
    bool_t  isAtLane    = false;
    
    // 쿨다운
    bool_t  qReady = true, wReady = true, eReady = true, rReady = true;
    bool_t  flashReady = true, ignitReady = true;
    
    // 팀 상황
    i32_t   alliesNearby = 0;
    i32_t   enemiesNearby = 0;
    i32_t   towersAlive_Enemy = 11;
    bool_t  inhibitorBroken_Top = false;
    bool_t  baronUp = false;
    bool_t  dragonUp = false;
    
    // 목표 표식
    bool_t  wantsToKill_Target = false;
    EntityID killTarget = INVALID_ENTITY;
};
```

실제 구현 시 bitset + 16비트 hash 로 축약.

## 액션 (Action)

```cpp
struct Action
{
    u32_t id;
    f32_t cost;                          // 기본 비용 (동적 가중치 가능)

    // 전후조건
    std::function<bool(const WorldState&)> preconditions;
    std::function<void(WorldState&)>       effects;

    // 실행 (BT 가 받음)
    std::function<BTStatus(BTContext&)>    executor;
};
```

## 액션 라이브러리 (약 30개)

| 액션 | 전제조건 | 효과 | 기본 비용 |
|---|---|---|---|
| `MoveToLane` | `!isAtLane` | `isAtLane = true` | 거리 기반 |
| `MoveToBase` | — | `isAtBase = true` | 거리 |
| `MoveToShop` | `isAtBase` | `isAtShop = true` | 1 |
| `Recall` | `!inCombat` | `isAtBase = true` | 8 (8초 채널링) |
| `BuyItem(X)` | `isAtShop && gold >= X.cost` | `gold -= X.cost, itemSlots += X` | 1 |
| `FarmMinionWave` | `isAtLane` | `gold += 150, hp -= 0.1` | 30 |
| `ClearJungleCamp` | `nearCamp` | `gold += 200, xp += 80, hp -= 0.2` | 40 |
| `KillChampion` | `enemiesNearby >= 1 && hp > 0.6` | `gold += 300, wantsToKill_Target = false` | 50 |
| `PushTower` | `isAtLane && alliesNearby >= 1` | `towersAlive_Enemy -= 1` | 60 |
| `TakeDragon` | `dragonUp && alliesNearby >= 3` | `dragonUp = false, teamBuff+` | 80 |
| `TakeBaron` | `baronUp && alliesNearby >= 4` | `baronUp = false, teamBuff++` | 100 |
| `WardArea(pos)` | `hasWard` | `visionAt(pos) = true` | 3 |
| `FlashForKill` | `flashReady && killTarget near` | `flashReady = false, wantsToKill = true` | 5 |
| `Backdoor` | `level >= 11` | `pushing inhibitor` | 30 |
| `DefendTower` | `enemySiegeOnOurTower` | `protect tower` | 50 |

## 목표 (Goal)

```cpp
struct Goal
{
    u32_t id;

    // 이 조건이 참이 되도록 플래너가 계획
    std::function<bool(const WorldState&)> isSatisfied;

    // 이 목표의 중요도 (Utility Stage 4 에서 계산)
    f32_t priority;
};
```

예시:
- `GetItem_Ionian_Boots` — `isSatisfied = hasItem(IonianBoots)`
- `KillEnemyADC` — `isSatisfied = !entityAlive(enemyADC)`
- `TakeDragon` — `isSatisfied = dragonTaken == true`
- `DefendInhibitor_Top` — `isSatisfied = inhibitor_top_HP > 0.5`
- `ReachLevel_6` — `isSatisfied = level >= 6`

## 플래너 (A*)

```cpp
// Engine/Public/AI/GOAP/Planner.h
class CGOAPPlanner
{
public:
    // 현재 상태에서 목표까지 최소 비용 액션 시퀀스
    // 실패 시 빈 벡터 반환
    std::vector<u32_t> Plan(const WorldState& current, const Goal& goal,
                            const std::vector<Action>& actionLibrary,
                            i32_t maxDepth = 12);

private:
    struct Node {
        WorldState      state;
        f32_t           gCost;         // 시작부터 비용
        f32_t           hCost;         // 휴리스틱 (목표까지 추정)
        f32_t           fCost() const { return gCost + hCost; }
        i32_t           parentIdx;
        u32_t           actionTaken;
    };

    f32_t Heuristic(const WorldState& state, const Goal& goal);

    std::vector<Node> m_openList;
    std::vector<Node> m_closedList;
};
```

### 동작

1. Start = current WorldState, f = 0
2. OpenList 에 Start push
3. While OpenList 비지 않음:
   - f 최소인 노드 pop
   - Goal.isSatisfied(node.state) 면 역추적 → plan 리턴
   - 모든 Action 에 대해:
     - action.preconditions(node.state) 만족?
     - neighbor = action.effects 적용
     - gCost = node.gCost + action.cost
     - 이미 CloseList 에 더 싸면 skip
     - OpenList push
4. maxDepth 초과 시 실패

### 휴리스틱

단순: 아직 달성 안 된 조건 수.
정교: 목표별 수치 차이 (현재 gold ↔ 필요 gold, 현재 level ↔ 필요 level).

## 동적 재계획

월드가 변경되면 플랜 무효화:
- 팀원 사망 → `allies_nearby` 감소 → TakeBaron 불가 → 재계획
- 적 갱킹 감지 → `enemiesNearby` 증가 → 수비/후퇴 재계획
- 스킬 쿨다운 → 일부 액션 차단

```cpp
// Engine/Public/AI/Systems/GOAPSystem.h
class CGOAPSystem : public ISystem
{
public:
    void Execute(CWorld& world, f32_t dt) override
    {
        world.ForEach<GOAPComponent, BotComponent>(
            [&](EntityID e, GOAPComponent& g, BotComponent& b)
        {
            g.lastReplanTime += dt;

            // 2초마다 월드 체크 + 필요시 재계획
            if (g.lastReplanTime >= REPLAN_INTERVAL || IsWorldStateInvalidated(world, e, g))
            {
                Goal goal = SelectHighestPriorityGoal(world, e);   // Utility Stage 4 호출
                WorldState current = BuildWorldState(world, e);
                g.plan = m_planner.Plan(current, goal, m_actions);
                g.planStep = 0;
                g.lastReplanTime = 0.f;
            }
        });
    }

private:
    CGOAPPlanner m_planner;
    std::vector<Action> m_actions;
};
```

## Utility 와의 관계

**Stage 4 Utility AI 가 Goal 우선순위 점수를 매기고, Stage 3 GOAP 가 선택된 Goal 로 플랜 생성**.

```
Utility Layer  (Stage 4)   ← "어떤 Goal 이 지금 중요한가?"
     ↓ (선택된 Goal)
GOAP Layer     (Stage 3)   ← "그 Goal 을 어떻게 달성할까?"
     ↓ (액션 시퀀스)
BT Layer       (Stage 2)   ← "각 액션을 어떻게 실행할까?"
```

## 캐싱 / 성능

- A* 플래닝은 비용 높음 (수십 개 액션 × 12 깊이)
- 봇 5명이 매 프레임 재계획하면 CPU 과부하
- **트리거 기반 재계획**: 특정 이벤트 (사망/오브젝트/아이템 완성) 에서만
- 병렬화: JobSystem 으로 봇별 플래너 작업 분산 (Phase 1b)

## 구현 순서

1. `WorldState` 구조체 + bitset 압축
2. `Action` 인터페이스 + 기본 10개 액션
3. `CGOAPPlanner::Plan` A* 구현
4. `Goal` 인터페이스 + 기본 5개 목표
5. `CGOAPSystem` ECS 통합
6. 재계획 트리거 이벤트
7. ImGui 에서 현재 플랜 시각화 (액션 체인)
8. 액션/목표 라이브러리 확장 (30/10개)
