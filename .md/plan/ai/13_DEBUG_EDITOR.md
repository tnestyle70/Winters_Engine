# 공통 — 디버깅 / 에디터 통합 / Replay

## 목표

봇 AI 를 **블랙박스로 두지 말 것**. ImGui 로 모든 의사결정 단계 실시간 시각화 + DebugDraw 로
공간 정보 표현 + Replay 로 회귀 분석.

## Winters 철학 연동

CLAUDE.md ImGui 정책: **"빌드 1번으로 모든 값 튜닝"**. 봇 난이도/성향/상태를 실시간 조작하는
에디터가 **연습모드**. 연습모드=에디터 철학의 핵심 실현.

## ImGui 봇 인스펙터

메인 창 구조:

```
┌─ Bot Debugger ────────────────────────────────────────────┐
│ [Bot Selector: #3 Yasuo Mid ▼]  [Pause AI: □] [Step: F10] │
├───────────────────────────────────────────────────────────┤
│ Tabs: [Overview] [HFSM] [BT] [GOAP] [Utility] [Map]       │
│       [MCTS] [Neural] [Blackboard] [Config]               │
├───────────────────────────────────────────────────────────┤
│ [선택한 탭 내용]                                          │
└───────────────────────────────────────────────────────────┘
```

### Overview 탭

```
Champion      : Yasuo
Role          : Mid
Difficulty    : Master (50ms)
Personality   : Aggressive
HP/MP         : 85% / 60%  Level 8  Gold: 1850
Current Goal  : FarmMidLane (GOAP)
Current State : Laning.Farming_Aggressive (HFSM)
Target        : #7 Enemy Sylas
Last Action   : CastQ at (35.2, 0, 40.1) @ 0.12s ago
Status        : [●] AI Active  [ ] Paused  [ ] Manual Override
```

### HFSM 탭

현재 상태 경로 시각화 + 전이 후보:
```
Root: Laning ▸ Farming_Aggressive
Elapsed in Sub: 2.34s
Elapsed in Root: 45.1s
Transition History (last 5):
  12.1s ago  Recalling.Channeling → Laning.Farming_Safe
  8.4s ago   Farming_Safe → Trading (enemy within Q range)
  5.2s ago   Trading → Farming_Safe (enemy retreated)
  2.3s ago   Farming_Safe → Farming_Aggressive (CS opportunity)

Available Transitions:
  → Ganking.Approaching    blocked (manaLow: 60%<65%)
  → TeamFighting.FlashIn   score 0.12
  → Recalling.Channeling   score 0.34
  → Trading                score 0.42 ★ will trigger in 0.1s
```

### BT 탭

현재 트리 순회 경로 (노드 하이라이트):
```
Yasuo_Lane.bt
└─ Selector (Running)
   ├─ Sequence "EmergencyEscape" (Failure)   [HP 85% > 25%]
   ├─ Sequence "FullCombo" (Failure)         [Q stacks 1/3]
   ├─ Sequence "PokeAndRetreat" (Running) ★
   │  ├─ Condition "InRange(QDash)" (Success)
   │  ├─ Action "CastQOnMinionNearTarget" (Running) ★
   │  └─ Action "AutoAttackOnce" (not yet)
   └─ Action "RepositionSafely" (not yet)
```

### GOAP 탭

```
Selected Goal: FarmMidLane (priority 0.68)
Plan:
  ① [CURRENT] MoveToLane          elapsed 2.1s / ~2.5s
  ②           FarmMinionWave      ~30s
  ③           Recall              ~8s (when gold >= 1600)
  ④           BuyItem_Sheen
  ⑤           MoveToLane
  ⑥           FarmMinionWave      until level 11
Total Cost Estimate: 78.5
Alternative Plans Considered:
  - GankTopLane: cost 92, blocked (ally_cooldowns)
  - TakeScuttle: cost 65, lower priority goal (0.28)
```

### Utility 탭

각 Goal 점수 브레이크다운:
```
Goal Candidates:
 Goal                    Score   Breakdown
 FarmMidLane             0.68  │ wave=0.8×0.3 hp=0.7×0.2 safe=0.6×0.15 ... 
 GankTopLane             0.45  │ dist=0.4×0.3 ally=0.6×0.3 mana=0.5×0.15 ...
 RecallAndBuy            0.32  │ gold=1.0×0.4 hp=0.5×0.2 time=0.2×0.1 ...
 TakeScuttle             0.28  │ ...
 DefendBotTower          0.15  │ ...
 
[Selected: FarmMidLane]
```

### Map 탭

Influence/Threat/Opportunity/Vision 히트맵:
```
Layer: [Team Influence ▼]
[280×280 히트맵 이미지]
Min: -8.2  Max: +12.4
Current Bot Cell: (42, 55)  value: -1.2 (contested)
Best Move Target: (58, 52)  value: +4.8 (safe + opportunity)
```

### MCTS 탭

탐색 트리 (루트 + 자식 몇 개) 시각화:
```
Root (visits: 4823)
├─ CastQ [←]          visits 1820  avg +0.45  UCB 1.23
├─ CastQ [→]          visits 1204  avg +0.32  UCB 1.18
├─ BasicAttack (Sylas) visits  984  avg +0.28  UCB 1.12
├─ MoveBackward        visits  612  avg +0.15  UCB 1.05
├─ UseFlash → CastR    visits  103  avg +0.88  UCB 1.41 ★ PROMISING
└─ Idle                visits  100  avg +0.10  UCB 1.01

Recommendation: CastQ [←] (max visits)
Exploration candidate: UseFlash → CastR (highest UCB)
```

### Neural 탭

ONNX 정책 추론 결과:
```
Last inference: 12ms ago (interval 100ms)
Input state size: 400
Output action (15 dims):
  move_dir      (-0.82, +0.34)
  cast_q        0.92  ← strong signal
  cast_w        0.11
  cast_e        0.38
  cast_r        0.04
  skill_target  (38.1, 0, 44.2)
  basic_attack  ent#7 (Sylas)  0.67
  ping_type     none
```

### Blackboard 탭

팀 공유 메모리 덤프 (Team Blackboard 문서 참조).

### Config 탭

난이도/성향 실시간 조작 (Difficulty 문서 참조).

## DebugDraw 통합

3D 월드에 오버레이:

| 표시 | 색상 | 의미 |
|---|---|---|
| 봇 현재 목적지 | 노랑 선 | 이동 경로 |
| 인식 중인 적 | 빨강 원 | 공격 의도 |
| 시야 있는 영역 | 초록 반투명 | 시야 체크 |
| Influence Map 셀 | 그라데이션 | 현재 선택 레이어 히트맵 |
| Hitbox/Hurtbox | 파랑/초록 와이어 | 충돌 디버그 |
| 플래시/점멸 경로 | 노란 점선 | 예상 이동 |
| MCTS 탐색 액션 후보 | 자주 | 상위 5개 후보 위치 |

```cpp
class CBotDebugDrawSystem : public ISystem
{
public:
    void Execute(CWorld& world, f32_t dt) override
    {
        if (!m_showOverlay) return;

        world.ForEach<BotComponent>([&](EntityID e, BotComponent& b) {
            if (e != m_selectedBot && !m_showAllBots) return;
            DrawBotOverlay(world, e);
        });
    }

    void SetSelectedBot(EntityID e) { m_selectedBot = e; }

private:
    EntityID m_selectedBot = INVALID_ENTITY;
    bool_t   m_showAllBots = false;
    bool_t   m_showOverlay = true;
};
```

## Replay 시스템

봇 의사결정 로그를 파일로 저장 → 나중에 재생하며 동일한 버그 재현.

### 녹화

```cpp
// Engine/Public/AI/Debug/DecisionRecorder.h
class CDecisionRecorder
{
public:
    void Start(const std::string& path);
    void Stop();

    void RecordDecision(EntityID bot, const DecisionSnapshot& snap);

private:
    struct DecisionSnapshot {
        f32_t      time;
        u32_t      hfsmRoot, hfsmSub;
        u32_t      btNodeIdx;
        u32_t      goapGoal;
        u32_t      chosenAction;
        WorldStateSnapshot worldState;
    };
    std::ofstream m_file;
};
```

### 재생

```cpp
class CDecisionReplayer
{
public:
    bool_t Load(const std::string& path);

    // 시간 슬라이더로 원하는 지점 이동
    void Seek(f32_t timeSec);

    const DecisionSnapshot& GetCurrentFrame() const;

    // ImGui 디버거가 이 스냅샷 표시
};
```

ImGui 컨트롤:
```
Replay: [▷][⏸][⏭][⟲]  time: [────●─────] 45.3s / 120.0s
Bot Filter: [All ▼]
```

## 연습모드와의 통합

연습모드 씬은 **디버거 UI 를 Always-On** 으로 띄움. 라이브 빌드에선 WINTERS_EDITOR 매크로로
제거 (릴리스에서 디버거 무게 제로).

```cpp
// Engine/Public/AI/Debug/BotDebugger.h
#ifdef WINTERS_EDITOR
class CBotDebugger
{
public:
    void Render();
    void SetSelectedBot(EntityID e);
};
#else
class CBotDebugger
{
public:
    void Render() {}                     // no-op
    void SetSelectedBot(EntityID) {}
};
#endif
```

## 단축키

| 키 | 기능 |
|---|---|
| F1 | 디버거 토글 |
| F2 | 모든 봇 Pause |
| F3 | 선택된 봇 Step (한 틱 진행) |
| F4 | DebugDraw 오버레이 토글 |
| F5 | Replay 녹화 시작/중지 |
| F6 | 현재 Influence Map 레이어 순환 |
| Ctrl+Click (월드) | 그 위치의 봇 선택 |

## 구현 순서

1. 빈 ImGui 창 + 봇 선택 드롭다운
2. Overview 탭 (간단한 상태 표시)
3. HFSM 탭 (상태 경로 + 전이 후보)
4. BT 탭 (트리 시각화)
5. DebugDraw 오버레이 (경로/대상)
6. Utility/GOAP 탭
7. Map 히트맵 (텍스처 업로드)
8. MCTS 트리 시각화
9. Replay 녹화 + 재생
10. Config 실시간 조작
