# Winters AI Core 스캐폴딩 구현 계획서

범위: **MCTS/RL을 제외한 나머지 AI 계층**의 뼈대.

기준 루트: `C:\Users\tnest\Desktop\Winters_restored\Winters`

관련 문서:

- `CLAUDE.md`
- `.md/process/PLAN_AUTHORING_PITFALLS.md`
- `.md/architecture/WINTERS_MULTIGAME_ARCHITECTURE.md`
- `.md/plan/ai/00_AI_PLAN_INDEX.md`
- `.md/plan/ai/01_ARCHITECTURE.md`
- `.md/plan/ai/02_STAGE0_AGGRO.md`
- `.md/plan/ai/03_STAGE1_HFSM.md`
- `.md/plan/ai/04_STAGE2_BT.md`
- `.md/plan/ai/05_STAGE3_GOAP.md`
- `.md/plan/ai/06_STAGE4_UTILITY.md`
- `.md/plan/ai/07_STAGE5_INFLUENCE_MAP.md`
- `.md/plan/ai/09_STAGE7_IMITATION.md`
- `.md/plan/ai/11_TEAM_BLACKBOARD.md`
- `.md/plan/ai/12_DIFFICULTY.md`
- `.md/plan/ai/13_DEBUG_EDITOR.md`
- `.md/plan/ai/codex/MCTS_RL_AI_IMPLEMENTATION_PLAN.md`

## 0. 목표

현재 `codex/MCTS_RL_AI_IMPLEMENTATION_PLAN.md`는 MCTS/RL 중심이다. 이 문서는 그 앞단과 주변 계층을
코드로 옮길 수 있게 다음 AI들을 공통 스캐폴딩으로 묶는다.

1. Stage 0: Aggro / Rule AI
2. Stage 1: HFSM
3. Stage 2: Behavior Tree
4. Stage 3: GOAP
5. Stage 4: Utility AI
6. Stage 5: Influence / Threat / Opportunity / Vision Map
7. Common: Team Blackboard / Difficulty / Debug / Replay
8. Stage 7 bridge: Imitation logging and prior interface

적용 제품:

- `WintersLOL`: 챔피언 봇, 미니언, 정글몹, 포탑 어그로.
- `WintersElden`: 엘든링형 보스, 보스 소환체, 액션 전투 적.
- `Class & Servant`: 플레이어 Class 를 보조·대응하는 Servant AI, PvPvE 보스/오브젝트 AI.

용어 정규화:

- 코드명은 `ClassServant`.
- 표시명은 `Class & Servant`.
- 사용자 표기의 `servent`는 기존 통합 아키텍처 문서 기준 `Servant`로 정규화한다.

## 1. 아키텍처 결정

### 1.1 Engine 과 게임별 모듈 경계

Engine AI 는 프레임워크와 런타임만 제공한다.

Engine 공통:

- Perception snapshot
- AI intent
- Aggro scoring
- HFSM runtime
- BT runtime
- Utility scoring
- Influence map grid
- GOAP planner
- Blackboard storage
- Debug snapshot / replay record

게임별:

- LoL 챔피언, 미니언, 정글몹, 포탑 룰.
- Elden 보스 패턴, 페이즈, 락온, 히트박스 타임라인.
- ClassServant 클래스/서번트 스킬, 계약, 소환 소유권, PvPvE 목표.

금지:

```cpp
// Engine/Public/AI 안에서 금지
#include "Scene_InGame.h"
#include "Champion/Irelia/IreliaSkills.h"
#include "Boss/Malenia/MaleniaPattern.h"
extern EntityID Winters_SpawnXxx(...);
```

허용:

```cpp
// Engine/Public/AI 안에서는 데이터 의도만 남긴다.
struct AIIntentComponent
{
    u32_t sequence = 0;
    // ...
};
```

게임별 시스템이 `AIIntentComponent`를 읽어 LoL command, Elden action command, ClassServant servant command 로 변환한다.

### 1.2 실행 Phase

`ISystem::GetPhase()`는 정수다. 같은 phase 시스템은 JobSystem 병렬 실행 가능성이 있으므로 producer/consumer는 분리한다.

| Phase | 시스템 | 쓰기 대상 |
|---|---|---|
| 0 | `CAIPerceptionSystem`, `CBlackboardSystem`, `CInfluenceMapSystem` | 스냅샷/World resource |
| 1 | `CAggroSystem`, `CHFSMSystem`, `CBTSystem`, `CUtilitySystem`, `CGOAPSystem` | `AIIntentComponent`, 각 AI 컴포넌트 |
| 2 | 게임별 `AIIntentApplySystem` | `CommandQueueComponent`, `NavAgentComponent`, 게임별 request |
| 3+ | Navigation / Move / Combat | 기존 게임플레이 컴포넌트 |

MCTS/RL 시스템도 Phase 1에 배치하되, 같은 entity의 intent를 동시에 덮어쓰지 않도록 brain arbitration을 둔다.

### 1.3 Owner Scope

World마다 AI 상태가 달라야 하므로 모든 공간 인덱스와 Blackboard는 `CWorld` resource 소유다.

| 리소스 | Owner | 이유 |
|---|---|---|
| `AIWorldResource` | `CWorld` | 제품별 World, 서버 GameRoom, 던전 샤드 분리 |
| `InfluenceMapResource` | `CWorld` | 맵/아레나/전장별 크기 다름 |
| `TeamBlackboardResource` | `CWorld` | 팀/파티/서번트 계약이 World마다 다름 |
| `AIDebugResource` | `CWorld` | replay/headless에서도 동일 데이터 저장 |

## 2. 권장 디렉터리 구조

### 2.1 Engine 공통

```text
Engine/Public/AI/
  Core/
    AITypes.h
    AIIntentComponent.h
    AIContext.h
    AIPerception.h
    AIPerceptionSystem.h
    AIBrainArbiterSystem.h
  Aggro/
    AggroComponent.h
    AggroScorer.h
    AggroSystem.h
  FSM/
    HFSMTypes.h
    HFSM.h
    FSMComponent.h
    HFSMSystem.h
  BehaviorTree/
    BTTypes.h
    BTNode.h
    BTContext.h
    BTInterpreter.h
    BehaviorTreeComponent.h
    BTSystem.h
  Utility/
    ResponseCurve.h
    UtilityTypes.h
    UtilityDecisionComponent.h
    UtilityDecisionMaker.h
    UtilitySystem.h
  InfluenceMap/
    GridMap.h
    InfluenceMapResource.h
    InfluenceMapSystem.h
  GOAP/
    GOAPTypes.h
    GOAPWorldState.h
    GOAPAction.h
    GOAPGoal.h
    GOAPPlanner.h
    GOAPComponent.h
    GOAPSystem.h
  Blackboard/
    BlackboardTypes.h
    TeamBlackboard.h
    BlackboardSystem.h
  Debug/
    AIDebugSnapshot.h
    AIReplayRecorder.h

Engine/Private/AI/
  Core/
    AIPerceptionSystem.cpp
    AIBrainArbiterSystem.cpp
  Aggro/
    AggroScorer.cpp
    AggroSystem.cpp
  FSM/
    HFSM.cpp
    HFSMSystem.cpp
  BehaviorTree/
    BTInterpreter.cpp
    BTSystem.cpp
  Utility/
    ResponseCurve.cpp
    UtilityDecisionMaker.cpp
    UtilitySystem.cpp
  InfluenceMap/
    GridMap.cpp
    InfluenceMapSystem.cpp
  GOAP/
    GOAPPlanner.cpp
    GOAPSystem.cpp
  Blackboard/
    TeamBlackboard.cpp
    BlackboardSystem.cpp
  Debug/
    AIReplayRecorder.cpp
```

### 2.2 게임별 어댑터

당장 저장소를 크게 흔들지 않는 1차 위치:

```text
Client/Public/AI/Product/
  LOL/
    LOLAIProfiles.h
    LOLAIIntentAdapter.h
  Elden/
    EldenBossAIProfile.h
    EldenAIIntentAdapter.h
  ClassServant/
    ServantAIProfile.h
    ClassServantAIIntentAdapter.h
```

중기 제품 디렉터리 분리 후 위치:

```text
Games/WintersLOL/Client/AI/
Games/WintersElden/Client/AI/
Games/ClassServant/Client/AI/
```

Server 권위 전환 시 같은 어댑터 개념을 `Games/*/Server/AI/`에 둔다.

## 3. Core 스캐폴딩

### 3.1 `AITypes.h`

```cpp
// Engine/Public/AI/Core/AITypes.h
#pragma once

#include "WintersTypes.h"

namespace WintersAI
{
    enum class eAIBrainType : u8_t
    {
        None = 0,
        RuleAggro,
        HFSM,
        BehaviorTree,
        Utility,
        GOAP,
        MCTS,
        NeuralPolicy,
        Hybrid,
    };

    enum class eAIDifficulty : u8_t
    {
        Intro = 0,
        Beginner,
        Intermediate,
        Master,
        Grandmaster,
    };

    enum class eAIProductDomain : u8_t
    {
        Generic = 0,
        LOL,
        Elden,
        ClassServant,
    };

    enum class eAIRole : u8_t
    {
        None = 0,
        Top,
        Jungle,
        Mid,
        ADC,
        Support,
        Boss,
        Minion,
        Monster,
        Servant,
        Companion,
    };
}
```

### 3.2 `AIIntentComponent.h`

AI는 직접 이동/공격 컴포넌트를 만지지 않고 intent만 쓴다.

```cpp
// Engine/Public/AI/Core/AIIntentComponent.h
#pragma once

#include "WintersTypes.h"
#include "WintersMath.h"
#include "ECS/Entity.h"

namespace WintersAI
{
    enum class eAIIntentType : u8_t
    {
        None = 0,
        Hold,
        MoveTo,
        Attack,
        CastSkill,
        Dodge,
        Guard,
        Follow,
        Interact,
        UsePattern,
        RequestReplan,
    };

    struct AIIntentComponent
    {
        eAIIntentType type = eAIIntentType::None;
        EntityID source = NULL_ENTITY;
        EntityID target = NULL_ENTITY;
        Vec3 targetPos{ 0.f, 0.f, 0.f };
        Vec3 direction{ 0.f, 0.f, 1.f };
        u32_t skillSlot = 0;
        u32_t patternId = 0;
        f32_t score = 0.f;
        f32_t validForSec = 0.1f;
        u32_t sequence = 0;
    };
}
```

제품별 변환 예:

- LoL: `CastSkill` → `CommandQueueComponent::Command`.
- Elden: `UsePattern` → `BossPatternRequestComponent`.
- ClassServant: `Guard/Follow/CastSkill` → `ServantCommandComponent`.

### 3.3 `AIContext.h`

모든 AI 계층이 같은 입력을 받게 하는 얇은 스냅샷이다.

```cpp
// Engine/Public/AI/Core/AIContext.h
#pragma once

#include "WintersTypes.h"
#include "WintersMath.h"
#include "ECS/Entity.h"
#include "AI/Core/AITypes.h"

namespace WintersAI
{
    static constexpr u32_t WINTERS_AI_MAX_VISIBLE_UNITS = 32;

    enum class eAITargetKind : u8_t
    {
        Unknown = 0,
        Champion,
        Minion,
        Monster,
        Structure,
        Boss,
        Servant,
        Player,
    };

    struct AITargetSnapshot
    {
        EntityID entity = NULL_ENTITY;
        eAITargetKind kind = eAITargetKind::Unknown;
        u8_t team = 0;
        Vec3 pos{ 0.f, 0.f, 0.f };
        Vec3 velocity{ 0.f, 0.f, 0.f };
        f32_t hpRatio = 1.f;
        f32_t distance = 0.f;
        f32_t threat = 0.f;
        bool_t bVisible = false;
        bool_t bAlive = true;
    };

    struct AIContext
    {
        EntityID self = NULL_ENTITY;
        eAIProductDomain product = eAIProductDomain::Generic;
        eAIRole role = eAIRole::None;
        u8_t team = 0;
        Vec3 selfPos{ 0.f, 0.f, 0.f };
        Vec3 forward{ 0.f, 0.f, 1.f };
        f32_t hpRatio = 1.f;
        f32_t staminaOrManaRatio = 1.f;
        f32_t dt = 0.f;
        u32_t rngState = 0;

        AITargetSnapshot visible[WINTERS_AI_MAX_VISIBLE_UNITS]{};
        u32_t visibleCount = 0;
    };
}
```

`AIContext`는 MCTS의 `BattleState`보다 넓고 가볍다. MCTS/RL은 `BattleState`를 쓰고, 나머지 계층은 `AIContext`를 우선 사용한다.

### 3.4 Brain Arbiter

여러 brain이 같은 entity에 intent를 쓰는 것을 막는다.

```cpp
// Engine/Public/AI/Core/AIBrainArbiterSystem.h
#pragma once

#include "ECS/ISystem.h"
#include "AI/Core/AITypes.h"
#include <memory>

namespace WintersAI
{
    struct AIBrainComponent
    {
        eAIBrainType primary = eAIBrainType::RuleAggro;
        eAIBrainType tactical = eAIBrainType::None;
        eAIBrainType fallback = eAIBrainType::RuleAggro;
        eAIDifficulty difficulty = eAIDifficulty::Beginner;
        eAIProductDomain product = eAIProductDomain::Generic;
        eAIRole role = eAIRole::None;
        f32_t decisionInterval = 0.2f;
        f32_t nextDecisionIn = 0.f;
        bool_t bEnabled = true;
    };

    class CAIBrainArbiterSystem final : public ISystem
    {
    public:
        static std::unique_ptr<CAIBrainArbiterSystem> Create();

        u32_t GetPhase() const override { return 1; }
        const char* GetName() const override { return "AIBrainArbiterSystem"; }
        void Execute(CWorld& world, f32_t dt) override;

    private:
        CAIBrainArbiterSystem() = default;
    };
}
```

초기 버전은 priority만 정한다.

1. `NeuralPolicy`가 유효하면 사용.
2. `MCTS`가 유효하면 전술 intent로 사용.
3. `BT/Utility/HFSM` intent 중 가장 높은 score 사용.
4. 없으면 `RuleAggro` fallback.

## 4. Stage 0 Aggro / Rule AI

### 4.1 공통 컴포넌트

```cpp
// Engine/Public/AI/Aggro/AggroComponent.h
#pragma once

#include "WintersTypes.h"
#include "WintersMath.h"
#include "ECS/Entity.h"

namespace WintersAI
{
    enum class eAggroState : u8_t
    {
        Idle = 0,
        Alerted,
        Attacking,
        Returning,
        Dead,
    };

    struct AggroMemorySlot
    {
        EntityID entity = NULL_ENTITY;
        f32_t damageReceived = 0.f;
        f32_t lastSeenSec = 0.f;
        f32_t score = 0.f;
    };

    static constexpr u32_t WINTERS_AI_AGGRO_MEMORY = 8;

    struct AggroComponent
    {
        eAggroState state = eAggroState::Idle;
        EntityID target = NULL_ENTITY;
        Vec3 spawnPos{ 0.f, 0.f, 0.f };
        f32_t aggroRadius = 8.f;
        f32_t leashRadius = 12.f;
        f32_t attackRange = 1.5f;
        f32_t returnHealPerSec = 0.1f;
        AggroMemorySlot memory[WINTERS_AI_AGGRO_MEMORY]{};
        u32_t memoryCount = 0;
    };
}
```

### 4.2 Scorer

```cpp
// Engine/Public/AI/Aggro/AggroScorer.h
#pragma once

#include "AI/Core/AIContext.h"
#include "AI/Aggro/AggroComponent.h"

namespace WintersAI
{
    struct AggroScoreWeights
    {
        f32_t distance = 0.35f;
        f32_t damage = 0.35f;
        f32_t lastAttacker = 0.20f;
        f32_t lowHp = 0.10f;
        f32_t championBias = 0.15f;
    };

    class CAggroScorer final
    {
    public:
        static EntityID SelectTarget(
            const AIContext& ctx,
            const AggroComponent& aggro,
            const AggroScoreWeights& weights,
            f32_t& outScore);
    };
}
```

### 4.3 제품별 사용

| 제품 | 적용 |
|---|---|
| LoL | 미니언, 정글몹, 포탑 기본 타겟 |
| Elden | 일반 몹, 보스의 플레이어/소환수 타겟 선택 |
| ClassServant | Servant가 보호할 owner 주변 위협 선택 |

Elden 보스는 Aggro를 최종 행동으로 쓰지 않는다. Aggro는 `currentTarget`을 고르는 입력이고, 실제 패턴은 HFSM/BT/Utility가 고른다.

## 5. Stage 1 HFSM

### 5.1 Runtime

```cpp
// Engine/Public/AI/FSM/HFSMTypes.h
#pragma once

#include "WintersTypes.h"

namespace WintersAI
{
    using AIStateID = u32_t;
    static constexpr AIStateID INVALID_AI_STATE = 0xffffffffu;

    enum class eAITransitionResult : u8_t
    {
        Stay = 0,
        Transit,
    };
}
```

```cpp
// Engine/Public/AI/FSM/FSMComponent.h
#pragma once

#include "AI/FSM/HFSMTypes.h"

namespace WintersAI
{
    struct FSMComponent
    {
        AIStateID rootState = INVALID_AI_STATE;
        AIStateID subState = INVALID_AI_STATE;
        AIStateID previousRootState = INVALID_AI_STATE;
        AIStateID previousSubState = INVALID_AI_STATE;
        f32_t stateElapsed = 0.f;
        f32_t transitionCheckAcc = 0.f;
        bool_t bTransitionRequested = false;
        AIStateID requestedRoot = INVALID_AI_STATE;
        AIStateID requestedSub = INVALID_AI_STATE;
    };
}
```

### 5.2 공통 Root 상태

```cpp
namespace WintersAI
{
    enum class eGenericRootState : u32_t
    {
        Idle = 1,
        Combat,
        Reposition,
        Recover,
        Objective,
        Dead,
    };
}
```

LoL / Elden / ClassServant는 이 공통 상태를 그대로 쓰지 않고 제품별 enum을 매핑한다.

### 5.3 LoL HFSM

```cpp
enum class eLOLRootState : u32_t
{
    Laning = 100,
    Farming,
    Ganking,
    TeamFighting,
    Pushing,
    Recalling,
    Defending,
    Objective,
    Dead,
};
```

### 5.4 Elden Boss HFSM

```cpp
enum class eEldenBossRootState : u32_t
{
    Dormant = 200,
    Intro,
    Phase1,
    Phase2,
    Enraged,
    Staggered,
    Ripostable,
    Recovering,
    Dead,
};
```

대표 전이:

- `Dormant -> Intro`: 플레이어 fog gate 진입 또는 arena trigger.
- `Intro -> Phase1`: intro animation event 완료.
- `Phase1 -> Phase2`: HP 60% 이하 또는 scripted phase marker.
- `Phase2 -> Enraged`: HP 25% 이하 + cooldown 완료.
- `Any -> Staggered`: posture break.
- `Staggered -> Ripostable`: stagger animation recovery window.
- `Any -> Dead`: HP 0.

### 5.5 ClassServant Servant HFSM

```cpp
enum class eServantRootState : u32_t
{
    Bound = 300,
    FollowOwner,
    GuardOwner,
    HarassEnemy,
    ControlObjective,
    AssistCombo,
    RescueOwner,
    ReturnToOwner,
    Unsummoned,
};
```

대표 전이:

- `FollowOwner -> GuardOwner`: owner가 피격되거나 threat score 상승.
- `GuardOwner -> AssistCombo`: owner가 CC/궁극기 콤보 시작.
- `HarassEnemy -> ReturnToOwner`: owner와 거리 초과.
- `Any -> Unsummoned`: duration 종료 또는 owner death.

## 6. Stage 2 Behavior Tree

### 6.1 최소 런타임

```cpp
// Engine/Public/AI/BehaviorTree/BTTypes.h
#pragma once

#include "WintersTypes.h"

namespace WintersAI
{
    using BTNodeID = u32_t;
    static constexpr BTNodeID INVALID_BT_NODE = 0xffffffffu;

    enum class eBTStatus : u8_t
    {
        Running = 0,
        Success,
        Failure,
    };

    enum class eBTNodeKind : u8_t
    {
        Selector = 0,
        Sequence,
        Parallel,
        Decorator,
        Condition,
        Action,
        SubTree,
    };
}
```

```cpp
// Engine/Public/AI/BehaviorTree/BehaviorTreeComponent.h
#pragma once

#include "AI/BehaviorTree/BTTypes.h"

namespace WintersAI
{
    static constexpr u32_t WINTERS_BT_STACK_MAX = 32;

    struct BehaviorTreeComponent
    {
        u32_t treeAssetId = 0;
        BTNodeID currentNode = INVALID_BT_NODE;
        BTNodeID runningStack[WINTERS_BT_STACK_MAX]{};
        u32_t runningDepth = 0;
        f32_t tickInterval = 0.1f;
        f32_t nextTickIn = 0.f;
        eBTStatus lastStatus = eBTStatus::Failure;
    };
}
```

### 6.2 BT Action은 intent만 쓴다

```cpp
// Engine/Public/AI/BehaviorTree/BTContext.h
#pragma once

#include "AI/Core/AIContext.h"
#include "AI/Core/AIIntentComponent.h"

namespace WintersAI
{
    struct BTContext
    {
        AIContext ai{};
        AIIntentComponent outIntent{};
        f32_t localTime = 0.f;
    };
}
```

BT leaf가 LoL `CastQ`, Elden `BossSlash01`, ClassServant `GuardOwner`를 직접 호출하지 않는다. leaf는 `AIIntentComponent`를 생성하고, 게임별 adapter가 실행한다.

### 6.3 제품별 트리 예시

LoL Irelia:

```text
LOL_Irelia_Combat
  Selector
    Sequence EmergencyEscape
    Sequence FullCombo
    Sequence PokeAndRetreat
    Action   RepositionSafely
```

Elden Boss:

```text
Elden_Boss_Phase2
  Selector
    Sequence PunishHealing
    Sequence CloseRangeCombo
    Sequence MidRangeGapClose
    Sequence AoEDenial
    Action   RepositionToArenaCenter
```

ClassServant Servant:

```text
ClassServant_Guardian
  Selector
    Sequence RescueOwner
    Sequence InterruptEnemyCast
    Sequence ComboWithOwner
    Sequence HoldObjectiveZone
    Action   FollowOwner
```

## 7. Stage 4 Utility AI

Stage 번호는 문서상 4지만 구현 순서는 BT 직후에 먼저 붙이는 것이 좋다. HFSM 전이, BT subtree 선택, GOAP goal 선택이 모두 Utility 점수에 기대기 때문이다.

### 7.1 Response Curve

```cpp
// Engine/Public/AI/Utility/ResponseCurve.h
#pragma once

#include "WintersTypes.h"

namespace WintersAI
{
    enum class eUtilityCurveType : u8_t
    {
        Linear = 0,
        Quadratic,
        Cubic,
        InverseQuadratic,
        Sigmoid,
        Threshold,
        Bell,
        SmoothStep,
    };

    class CResponseCurve final
    {
    public:
        static f32_t Evaluate(eUtilityCurveType type, f32_t x, f32_t p0 = 0.f, f32_t p1 = 0.f);
    };
}
```

### 7.2 Decision Component

```cpp
// Engine/Public/AI/Utility/UtilityDecisionComponent.h
#pragma once

#include "AI/Core/AIIntentComponent.h"

namespace WintersAI
{
    static constexpr u32_t WINTERS_UTILITY_MAX_CANDIDATES = 16;

    struct UtilityCandidate
    {
        u32_t actionId = 0;
        AIIntentComponent intent{};
        f32_t score = 0.f;
        f32_t lastScore = 0.f;
    };

    struct UtilityDecisionComponent
    {
        UtilityCandidate candidates[WINTERS_UTILITY_MAX_CANDIDATES]{};
        u32_t candidateCount = 0;
        u32_t chosenIndex = 0;
        f32_t hysteresis = 0.1f;
        f32_t randomness = 0.f;
    };
}
```

### 7.3 제품별 Utility 후보

LoL:

- `FarmLane`
- `TradeEnemy`
- `RecallAndBuy`
- `GankLane`
- `TakeObjective`
- `DefendTower`

Elden Boss:

- `UseCloseCombo`
- `UseGapClose`
- `UseArenaAoE`
- `BackstepRecover`
- `SummonAdd`
- `PhaseTransitionAttack`

ClassServant:

- `GuardOwner`
- `FollowOwner`
- `HarassTarget`
- `AssistOwnerCombo`
- `HoldObjective`
- `SacrificeBlock`

## 8. Stage 5 Influence Map

### 8.1 도메인 상수 금지

LoL 맵, Elden 보스 아레나, ClassServant 전장은 크기가 다르다. `static constexpr MAP_GRID_W = 140` 같은 Engine 공용 상수는 쓰지 않는다.

```cpp
// Engine/Public/AI/InfluenceMap/GridMap.h
#pragma once

#include "WintersTypes.h"
#include "WintersMath.h"
#include <vector>

namespace WintersAI
{
    struct GridMapDesc
    {
        Vec3 origin{ 0.f, 0.f, 0.f };
        f32_t cellSize = 1.f;
        i32_t width = 64;
        i32_t height = 64;
    };

    class CGridMap final
    {
    public:
        bool Initialize(const GridMapDesc& desc);
        void Clear(f32_t value = 0.f);
        void Set(i32_t x, i32_t y, f32_t value);
        f32_t Get(i32_t x, i32_t y) const;
        bool WorldToCell(const Vec3& worldPos, i32_t& outX, i32_t& outY) const;

        const GridMapDesc& GetDesc() const { return m_desc; }

    private:
        i32_t ToIndex(i32_t x, i32_t y) const;

    private:
        GridMapDesc m_desc{};
        std::vector<f32_t> m_data;
    };
}
```

`WorldToCell`은 음수 좌표에서 `static_cast<i32_t>(world / cell)`을 쓰면 안 된다. `std::floor` 또는 origin shift 기반으로 계산한다.

### 8.2 Resource

```cpp
// Engine/Public/AI/InfluenceMap/InfluenceMapResource.h
#pragma once

#include "AI/InfluenceMap/GridMap.h"

namespace WintersAI
{
    struct InfluenceMapResource
    {
        CGridMap team;
        CGridMap threat;
        CGridMap opportunity;
        CGridMap vision;
        CGridMap terrain;
        f32_t teamUpdateAcc = 0.f;
        f32_t threatUpdateAcc = 0.f;
        f32_t opportunityUpdateAcc = 0.f;
        f32_t visionUpdateAcc = 0.f;
    };
}
```

### 8.3 제품별 desc

| Product | GridMapDesc |
|---|---|
| LoL | Summoner's Rift 중심, 1~2m cell |
| Elden Boss | 보스 arena 중심, 0.5~1m cell, 원형 arena도 grid로 근사 |
| ClassServant | battlefield/dungeon/objective zone 별 desc |

Elden 보스는 "맵 전체"보다 arena danger map이 중요하다.

레이어 예:

- `threat`: 보스 공격 hitbox 예고 영역.
- `opportunity`: 보스 recovery window에서 접근 가능한 안전 구역.
- `terrain`: 기둥, 낙사, 벽, boss arena boundary.

ClassServant 레이어 예:

- `team`: owner + servant + party influence.
- `threat`: enemy class + boss + hostile servant.
- `opportunity`: contract objective, downed ally, capture point.
- `vision`: PvPvE 전장 시야.

## 9. Stage 3 GOAP

GOAP는 큰 목표의 액션 시퀀스를 만든다. 제품별 액션 라이브러리만 다르고 planner는 Engine 공통이다.

### 9.1 WorldState

```cpp
// Engine/Public/AI/GOAP/GOAPWorldState.h
#pragma once

#include "WintersTypes.h"
#include "WintersMath.h"
#include "ECS/Entity.h"

namespace WintersAI
{
    static constexpr u32_t WINTERS_GOAP_MAX_FACTS = 64;

    enum class eGOAPFactOp : u8_t
    {
        Equals = 0,
        GreaterEqual,
        LessEqual,
    };

    struct GOAPFact
    {
        u32_t key = 0;
        i32_t value = 0;
    };

    struct GOAPWorldState
    {
        GOAPFact facts[WINTERS_GOAP_MAX_FACTS]{};
        u32_t factCount = 0;

        bool GetFact(u32_t key, i32_t& outValue) const;
        void SetFact(u32_t key, i32_t value);
        bool Satisfies(u32_t key, eGOAPFactOp op, i32_t value) const;
    };
}
```

초기 skeleton은 정수 fact 위주로 시작한다. 위치나 EntityID는 fact가 아니라 action context에 둔다.

### 9.2 Action / Goal

```cpp
// Engine/Public/AI/GOAP/GOAPAction.h
#pragma once

#include "AI/GOAP/GOAPWorldState.h"
#include "AI/Core/AIIntentComponent.h"

namespace WintersAI
{
    static constexpr u32_t WINTERS_GOAP_MAX_CONDITIONS = 8;
    static constexpr u32_t WINTERS_GOAP_MAX_EFFECTS = 8;

    struct GOAPCondition
    {
        u32_t key = 0;
        eGOAPFactOp op = eGOAPFactOp::Equals;
        i32_t value = 0;
    };

    struct GOAPAction
    {
        u32_t id = 0;
        f32_t baseCost = 1.f;
        GOAPCondition preconditions[WINTERS_GOAP_MAX_CONDITIONS]{};
        u32_t preconditionCount = 0;
        GOAPFact effects[WINTERS_GOAP_MAX_EFFECTS]{};
        u32_t effectCount = 0;
        AIIntentComponent intentTemplate{};
    };

    struct GOAPGoal
    {
        u32_t id = 0;
        GOAPCondition desired[WINTERS_GOAP_MAX_CONDITIONS]{};
        u32_t desiredCount = 0;
        f32_t priority = 0.f;
    };
}
```

### 9.3 제품별 GOAP 목표

LoL:

- `BuyCoreItem`
- `TakeDragon`
- `DefendInhibitor`
- `SplitPush`
- `GroupForBaron`

Elden Boss:

- `ForcePlayerToMidRange`
- `BreakPlayerGuard`
- `RecoverPosture`
- `SummonAdds`
- `TransitionPhase`

ClassServant:

- `ProtectOwnerUntilCooldown`
- `CaptureObjective`
- `EscortClass`
- `BindBossAsServant`
- `RetreatAndResummon`

Elden Boss GOAP는 MOBA 운영처럼 길게 잡지 않는다. 보스전에서는 2~5초 길이의 pattern plan만 만든다.

## 10. Blackboard

### 10.1 타입 안전 키

문자열 + `std::any`는 디버깅은 쉽지만 서버 권위와 replay에는 불리하다. Skeleton은 enum key + value union으로 시작한다.

```cpp
// Engine/Public/AI/Blackboard/BlackboardTypes.h
#pragma once

#include "WintersTypes.h"
#include "WintersMath.h"
#include "ECS/Entity.h"

namespace WintersAI
{
    enum class eBlackboardKey : u32_t
    {
        CurrentObjective = 1,
        ObjectivePriority,
        GroupLeader,
        GroupingAt,
        FlaggedEnemy,
        SafeRetreatPos,
        OwnerEntity,
        ServantEntity,
        BossPhase,
        BossTarget,
    };

    enum class eBlackboardValueType : u8_t
    {
        None = 0,
        Int,
        Float,
        Bool,
        Entity,
        Vec3,
    };

    struct BlackboardValue
    {
        eBlackboardValueType type = eBlackboardValueType::None;
        i32_t i = 0;
        f32_t f = 0.f;
        bool_t b = false;
        EntityID e = NULL_ENTITY;
        Vec3 v{ 0.f, 0.f, 0.f };
    };
}
```

### 10.2 Team / Party / Pair Blackboard

```cpp
// Engine/Public/AI/Blackboard/TeamBlackboard.h
#pragma once

#include "AI/Blackboard/BlackboardTypes.h"
#include <unordered_map>

namespace WintersAI
{
    class CTeamBlackboard final
    {
    public:
        void Set(eBlackboardKey key, const BlackboardValue& value);
        bool Get(eBlackboardKey key, BlackboardValue& outValue) const;
        void Remove(eBlackboardKey key);
        void Clear();

    private:
        std::unordered_map<u32_t, BlackboardValue> m_values;
    };

    struct TeamBlackboardResource
    {
        CTeamBlackboard teams[2];
        CTeamBlackboard neutralBoss;
    };
}
```

ClassServant는 pair 단위 Blackboard가 필요하다.

```cpp
struct ServantLinkComponent
{
    EntityID owner = NULL_ENTITY;
    EntityID servant = NULL_ENTITY;
    u32_t pairBlackboardId = 0;
    f32_t maxFollowDistance = 12.f;
    bool_t bAllowAutonomousCombat = true;
};
```

이 컴포넌트는 Engine 공통에 둘 수도 있지만, 의미가 ClassServant에 치우치면 게임별 모듈에 둔다.

## 11. Imitation Bridge

`09_STAGE7_IMITATION.md`의 전체 학습 파이프라인은 MCTS/RL 문서와 이어지지만, 룰 기반 AI도 로그를 남겨야 한다.

```cpp
// Engine/Public/AI/Debug/AIDebugSnapshot.h
#pragma once

#include "AI/Core/AITypes.h"
#include "AI/Core/AIIntentComponent.h"
#include "AI/FSM/HFSMTypes.h"
#include "AI/BehaviorTree/BTTypes.h"

namespace WintersAI
{
    struct AIDebugSnapshot
    {
        EntityID entity = NULL_ENTITY;
        eAIProductDomain product = eAIProductDomain::Generic;
        eAIBrainType brain = eAIBrainType::None;
        AIStateID rootState = INVALID_AI_STATE;
        AIStateID subState = INVALID_AI_STATE;
        BTNodeID btNode = INVALID_BT_NODE;
        u32_t goapGoal = 0;
        u32_t utilityAction = 0;
        AIIntentComponent chosenIntent{};
        f32_t decisionMs = 0.f;
        u32_t frame = 0;
    };
}
```

이 snapshot이 있으면 다음이 가능하다.

- LoL 봇이 어떤 state/BT/Utility 때문에 행동했는지 replay.
- Elden 보스가 왜 특정 패턴을 골랐는지 frame 단위 분석.
- ClassServant servant가 owner 보호와 objective 사이에서 어떤 결정을 했는지 비교.
- Stage 7 Imitation 학습 데이터의 label seed로 사용.

## 12. Elden 보스 적용 뼈대

### 12.1 보스 AI 프로필

```cpp
// Client/Public/AI/Product/Elden/EldenBossAIProfile.h
#pragma once

#include "AI/Core/AITypes.h"
#include "AI/FSM/HFSMTypes.h"
#include "ECS/Entity.h"

namespace WintersElden
{
    enum class eBossPatternID : u32_t
    {
        None = 0,
        CloseSlash01,
        CloseSlashCombo,
        GapCloseThrust,
        BackstepRecover,
        ArenaAoE,
        PunishHeal,
        PhaseTransition,
        SummonAdds,
    };

    struct EldenBossAIProfile
    {
        u32_t bossId = 0;
        f32_t perceptionRadius = 40.f;
        f32_t closeRange = 4.f;
        f32_t midRange = 12.f;
        f32_t postureBreakThreshold = 1.f;
        f32_t phase2HpRatio = 0.6f;
        f32_t enrageHpRatio = 0.25f;
    };
}
```

### 12.2 보스 intent adapter

```cpp
// Client/Public/AI/Product/Elden/EldenAIIntentAdapter.h
#pragma once

#include "AI/Core/AIIntentComponent.h"

class CWorld;

namespace WintersElden
{
    class CEldenAIIntentAdapter final
    {
    public:
        static void ApplyBossIntent(CWorld& world, EntityID boss, const WintersAI::AIIntentComponent& intent);
    };
}
```

적용 규칙:

- `UsePattern(patternId)` → `BossPatternRequestComponent`.
- `Dodge(direction)` → boss-specific evasive pattern request.
- `MoveTo(targetPos)` → action-combat movement controller.
- `Attack(target)` → 가까운 기본 패턴으로 변환.

### 12.3 첫 보스 프로토타입

1. `AggroComponent`: player / summon / servant 중 target 선택.
2. `FSMComponent`: `Dormant -> Intro -> Phase1 -> Phase2 -> Dead`.
3. `BehaviorTreeComponent`: phase별 pattern tree 선택.
4. `UtilityDecisionComponent`: `CloseCombo`, `GapClose`, `AoE`, `Recover` 점수화.
5. `InfluenceMapResource`: arena threat/opportunity debug.
6. `GOAPComponent`: phase transition, summon adds, pressure reposition만 계획.
7. `AIDebugSnapshot`: pattern score와 phase 전이 기록.

## 13. Class & Servant 적용 뼈대

### 13.1 Servant Profile

```cpp
// Client/Public/AI/Product/ClassServant/ServantAIProfile.h
#pragma once

#include "AI/Core/AITypes.h"
#include "ECS/Entity.h"

namespace ClassServant
{
    enum class eServantArchetype : u8_t
    {
        Guardian = 0,
        Striker,
        Controller,
        Healer,
        Scout,
    };

    struct ServantAIProfile
    {
        u32_t servantId = 0;
        eServantArchetype archetype = eServantArchetype::Guardian;
        f32_t followDistance = 6.f;
        f32_t guardRadius = 10.f;
        f32_t autonomy = 0.5f;
        f32_t ownerProtectionWeight = 0.7f;
        f32_t objectiveWeight = 0.3f;
    };
}
```

### 13.2 Servant intent adapter

```cpp
// Client/Public/AI/Product/ClassServant/ClassServantAIIntentAdapter.h
#pragma once

#include "AI/Core/AIIntentComponent.h"

class CWorld;

namespace ClassServant
{
    class CClassServantAIIntentAdapter final
    {
    public:
        static void ApplyServantIntent(CWorld& world, EntityID servant, const WintersAI::AIIntentComponent& intent);
    };
}
```

적용 규칙:

- `Follow(owner)` → owner-relative move command.
- `Guard(owner)` → guard point + intercept target.
- `CastSkill` → servant skill request.
- `Interact` → objective/channel/summon interaction.
- `RequestReplan` → GOAP replan flag.

### 13.3 첫 Servant 프로토타입

1. owner + servant entity에 link component 부착.
2. servant HFSM: `FollowOwner / GuardOwner / AssistCombo / ReturnToOwner`.
3. BT: `RescueOwner -> InterruptEnemyCast -> ComboWithOwner -> FollowOwner`.
4. Utility: owner 보호, 적 견제, 오브젝트 유지 점수 비교.
5. Blackboard: owner hp, owner target, party objective, flagged enemy 공유.
6. GOAP: `ProtectOwnerUntilCooldown`, `CaptureObjective`, `RetreatAndResummon`.
7. Debug: owner와 servant decision을 같은 UI 탭에서 나란히 표시.

## 14. LoL 적용 뼈대

LoL은 기존 Stage 문서의 원래 대상이다.

첫 적용 순서:

1. `AggroComponent`를 미니언/정글몹에 부착.
2. `CAggroSystem`이 현재 MinionAI 동작을 보존하는 범위에서 target만 선택.
3. 챔피언 봇에 `AIBrainComponent + FSMComponent + BehaviorTreeComponent + UtilityDecisionComponent`.
4. `LOLAIIntentAdapter`가 `AIIntentComponent`를 공용 `CommandQueueComponent`로 변환.
5. `InfluenceMapResource`는 LoL GameModule이 desc를 주입.
6. `GOAPSystem`은 Utility가 고른 goal에 대해서만 2~5초 간격 replan.

행동 보존 주의:

- MinionAI 최적화와 AI 확장은 분리한다.
- 현재 enemy minion only 동작을 확장하려면 별도 phase에서 챔피언/포탑 어그로 룰을 명시한다.

## 15. 구현 PR 분할

### PR AI-0: Core compile-only

포함:

- `AITypes.h`
- `AIIntentComponent.h`
- `AIContext.h`
- `AIBrainComponent`
- 빈 `CAIBrainArbiterSystem`
- `AIDebugSnapshot.h`
- `Engine.vcxproj`, `Engine.vcxproj.filters`

완료 기준:

- Engine 단독 빌드.
- Client가 SDK 복사된 `AI/Core/...` 헤더를 include 가능.

### PR AI-1: Aggro

포함:

- `AggroComponent.h`
- `AggroScorer.h/.cpp`
- `AggroSystem.h/.cpp`
- LoL 미니언/정글몹 테스트 부착 지점.

완료 기준:

- 미니언/정글몹이 기존 행동을 깨지 않고 target intent를 생성.
- Debug snapshot에 target entity와 score 표시.

### PR AI-2: HFSM + BT skeleton

포함:

- `HFSMTypes.h`, `FSMComponent.h`, `HFSM.h/.cpp`, `HFSMSystem.h/.cpp`
- `BTTypes.h`, `BTContext.h`, `BehaviorTreeComponent.h`, `BTInterpreter.h/.cpp`, `BTSystem.h/.cpp`
- 제품별 sample profile 1개씩: Irelia / EldenBoss / GuardianServant.

완료 기준:

- 각 제품 sample entity가 상태 전이와 BT tick을 로그로 남김.
- 실제 스킬/패턴 실행 없이 intent만 생성.

### PR AI-3: Utility + Blackboard

포함:

- `ResponseCurve.h/.cpp`
- `UtilityDecisionComponent.h`
- `UtilityDecisionMaker.h/.cpp`
- `TeamBlackboard.h/.cpp`
- `BlackboardSystem.h/.cpp`

완료 기준:

- Utility 후보와 점수 breakdown이 Debug UI/로그에 표시.
- 팀/owner blackboard 값 read/write 가능.

### PR AI-4: Influence Map

포함:

- `GridMap.h/.cpp`
- `InfluenceMapResource.h`
- `InfluenceMapSystem.h/.cpp`
- CPU update skeleton.

완료 기준:

- LoL map desc, Elden arena desc, ClassServant battlefield desc를 각각 주입 가능.
- 음수 좌표 cell 변환 테스트 통과.

### PR AI-5: GOAP

포함:

- `GOAPWorldState.h/.cpp`
- `GOAPAction.h`
- `GOAPGoal.h`
- `GOAPPlanner.h/.cpp`
- `GOAPComponent.h`
- `GOAPSystem.h/.cpp`

완료 기준:

- 5~10개 action library로 plan 생성.
- LoL/Elden/ClassServant sample goal 1개씩 plan 생성.

### PR AI-6: Product adapters

포함:

- `LOLAIIntentAdapter`
- `EldenAIIntentAdapter`
- `ClassServantAIIntentAdapter`
- 각 product sample profile.

완료 기준:

- LoL: `AIIntent -> CommandQueueComponent`.
- Elden: `AIIntent -> BossPatternRequestComponent` placeholder.
- ClassServant: `AIIntent -> ServantCommandComponent` placeholder.

### PR AI-7: Debug / Replay / Imitation bridge

포함:

- `AIReplayRecorder`
- ImGui Bot Debugger 탭 skeleton.
- JSONL decision log.

완료 기준:

- HFSM/BT/Utility/GOAP/Aggro 선택 경로가 한 frame snapshot으로 기록.
- 이후 Stage 7 Imitation 학습 데이터로 변환 가능.

## 16. 완료 기준

공통:

- Engine AI framework가 LoL, Elden, ClassServant를 include하지 않는다.
- `AIIntentComponent`를 통해서만 게임별 실행으로 넘어간다.
- 모든 spatial/blackboard resource는 `CWorld` 소유다.
- Scheduler phase는 정수이며 producer/consumer가 분리되어 있다.
- Debug snapshot에서 선택 이유를 볼 수 있다.

LoL:

- 미니언/정글몹 Aggro가 동작한다.
- 챔피언 봇이 HFSM/BT/Utility로 이동/공격 intent를 낸다.

Elden:

- 보스 1체가 `Dormant -> Intro -> Phase1 -> Phase2 -> Dead`를 탄다.
- 보스 패턴 선택이 BT/Utility score로 설명 가능하다.

ClassServant:

- Servant 1체가 owner를 따라다니고 보호 intent를 낸다.
- owner/servant pair blackboard가 동작한다.

## 17. PITFALLS Gate 통과 메모

| Gate | 반영 |
|---|---|
| A 사실 수집 | `CLAUDE.md`, AI Stage 문서, `WINTERS_MULTIGAME_ARCHITECTURE.md`, MCTS/RL 계획서 확인 |
| B TODO 0 | 본 문서는 skeleton 범위를 명시하고 미확정 데이터에 의존하는 수치 박제 없음 |
| C 호출 경로 | 실제 코드 호출 박제가 아니라 신규 scaffold 파일 목록 중심. 제품 실행은 adapter로 분리 |
| D ECS 책임 | Scene 직접 호출 금지, `AIIntentComponent` 기반 |
| E 향후 자료형 | Influence grid는 `GridMapDesc` 주입, bitmask 고정 한도 회피 |
| F Scheduler | Phase 0/1/2 정수 분리 |
| G Owner Scope | Influence/Blackboard/Debug resource는 `CWorld` 소유 |
| H 행동 보존 | LoL MinionAI 행동 확장은 별도 phase로 분리 |

---

**END OF DOCUMENT**
