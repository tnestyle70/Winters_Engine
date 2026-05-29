# Stage 1 — HFSM (Hierarchical Finite State Machine)

## 목표

봇의 상태를 **2단 계층**으로 관리. Root 상태 + Sub 상태.

## 왜 HFSM 인가

단일 FSM 으론 LoL 봇의 복잡도 처리 불가 (상태 폭발). HFSM 으로 분리 시 `Laning` 상태 하나에
`Farming_Safe / Farming_Aggressive / Trading / Zoning / Freezing` 5개 서브 상태 집어넣을 수 있음.

## 루트 상태 (9개)

```cpp
enum class BotRootState : u8_t {
    Laning,           // 라인 유지
    Farming,          // CS 수급
    Ganking,          // 갱킹 이동 중
    TeamFighting,     // 한타
    Pushing,          // 라인 밀기
    Recalling,        // 귀환 중 (채널링)
    Defending,        // 수비 (포탑/억제기 방어)
    Objective,        // 오브젝트 (Dragon/Baron)
    Dead              // 사망 (리스폰 대기)
};
```

## 서브 상태 (루트별)

### Laning
- `Farming_Safe` — 안전거리 유지 CS
- `Farming_Aggressive` — 챔피언 견제 병행 CS
- `Trading` — 교환 딜 진행
- `Zoning` — 상대 CS 차단
- `Freezing` — 미니언 웨이브 프리즈 (수비 유리 포지션)

### Ganking
- `Approaching` — 갱 포지션으로 이동 (시야 회피 경로)
- `WaitingAmbush` — 부쉬에서 대기
- `Engaging` — 점프/CC 사용 진입
- `Cleanup` — 적 사망 후 탑/바텀 컨트롤
- `Escaping` — 실패 시 귀환

### TeamFighting
- `Positioning` — 한타 전 포지션
- `Frontline` — 탱커/브루저 전진 (CC 진입)
- `Backline` — 원거리 딜러 안전지대
- `FlashIn` — 플래시 진입 (암살자/이니시)
- `Peeling` — 원거리 보호 (CC/갱백)
- `Chasing` — 퇴각 적 추적
- `Retreating` — 한타 패배 시 후퇴

### Recalling
- `ReturningToLane` — 귀환 후 라인 복귀 (텔포/이동 경로 선택)
- `Channeling` — 귀환 채널링 중 (방해 감지)

### Defending
- `UnderTower` — 포탑 아래 대기
- `CounterPush` — 미니언 웨이브 맞받아치기
- `Sieging` — 공성 전 진형

### Objective
- `SetupVision` — 시야 확보
- `Sieging` — 드래곤/바론 앞 라인 정리
- `Executing` — 오브젝트 처치
- `Teamfight_OnObjective` — 오브젝트 부근 교전

### Pushing
- `WaveClear` — 미니언 웨이브 처리
- `TowerDive` — 포탑 다이브
- `Splitpush` — 스플릿 푸시 (백도어 압박)

### Dead
- `Respawning` — 리스폰 카운트다운
- `ItemShopping` — 부활 직전 아이템 구매 계획

## 상태 전이 조건

전이는 **매 프레임** 검사하지 않고 **서브 상태 내부 이벤트** 또는 **정기 검사** (0.5초 간격) 로.

### Laning → Ganking 전이 예시

```cpp
bool BotFSM::ShouldTransition_LaningToGanking(const BotContext& ctx)
{
    if (m_role != Role::Jungle) return false;                  // 정글만
    if (ctx.manaPercent < 0.3f) return false;                  // 마나 부족
    if (ctx.hpPercent < 0.5f) return false;                    // 체력 부족
    
    const LaneOpportunity& opp = ctx.blackboard.bestGankLane;
    if (opp.score < GANK_THRESHOLD) return false;              // 기회 없음
    if (opp.travelTime > 15.f) return false;                   // 너무 멂
    
    return true;
}
```

## 구현 스케치

```cpp
// Engine/Public/AI/FSM/HFSM.h
class CHFSM
{
public:
    using StateHandler = std::function<void(CWorld&, EntityID, f32_t)>;

    struct StateNode
    {
        u32_t       id;
        u32_t       parentID;            // 루트는 INVALID
        StateHandler onEnter;
        StateHandler onUpdate;
        StateHandler onExit;
    };

    void RegisterState(u32_t id, u32_t parentID, StateHandler onEnter,
                       StateHandler onUpdate, StateHandler onExit);

    void TransitionTo(u32_t newStateID);

    void Update(CWorld& world, EntityID self, f32_t dt);

    u32_t GetCurrentRoot() const;
    u32_t GetCurrentSub() const;

private:
    std::unordered_map<u32_t, StateNode> m_states;
    u32_t m_currentRoot = 0;
    u32_t m_currentSub  = 0;
    f32_t m_stateElapsed = 0.f;
};
```

전이 시 **계층 상위까지 OnExit 호출, 계층 하위로 OnEnter 호출**:
```
Laning.Farming_Safe → Ganking.Approaching 전이:
  Farming_Safe.OnExit()
  Laning.OnExit()
  Ganking.OnEnter()
  Approaching.OnEnter()
```

## 데이터 드리븐 (선택)

`.fsm` 파일로 상태 정의 분리 가능 (JSON/FlatBuffers). 수업 일정상 초기엔 코드 하드코딩,
후반에 에디터 UI 로 상태 편집 가능하게 확장.

## 챔피언별 차등

각 챔피언 봇 프로필이 HFSM 상태 조합을 다르게 선택:

| 챔피언 | Role | 주요 상태 커스텀 |
|---|---|---|
| Irelia | Top/Mid 딜탱 | `Trading` 공격적, `Engaging` Q 대쉬 사거리 최적화 |
| Yasuo | Mid | `Zoning` 불가능 (근접), `TeamFighting.FlashIn` = Q3 + R 콤보 |
| Sylas | Mid | `TeamFighting` 항상 `Engaging` (E-Q-R 콤보) |
| Viego | Jungle | `Ganking` + 갱 후 `Cleanup.Possession` 시체 빙의 |
| Kalista | ADC | `Backline` 강화, `Frontline` 금지. `Peeling` 궁 카운터 |

## ImGui 디버거

```
[Bot Inspector — Yasuo #3]
Root:    TeamFighting
Sub:     Backline
Elapsed: 2.34s
Prev:    Laning.Farming_Aggressive (12.1s ago)
---
Transitions Available:
  → Ganking.Approaching       (score: 0.12, blocked: manaLow)
  → Recalling.Channeling      (score: 0.34, blocked: inCombat)
  → TeamFighting.FlashIn      (score: 0.89) [GO!]
```

## 구현 순서

1. `CHFSM` 클래스 + 상태 등록/전이 엔진
2. 9 Root × 대표 Sub 하나씩 스텁 구현
3. 전이 조건 함수 (Utility Stage 4 점수 활용)
4. ImGui 디버거 패널
5. 챔피언별 상태 커스텀 (BotProfile_* 에서)
6. 이벤트 기반 전이 (예: `OnAllyDeath`, `OnObjectiveAvailable`)
