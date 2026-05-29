# Winters MCTS + RL AI 구현 계획서

기준 루트: `C:\Users\user\Desktop\Winters`

참고 문서:

- `.md/plan/ai/01_ARCHITECTURE.md`
- `.md/plan/ai/08_STAGE6_MCTS.md`
- `.md/plan/ai/09_STAGE7_IMITATION.md`
- `.md/plan/ai/10_STAGE8_RL.md`
- `.md/plan/ai/11_TEAM_BLACKBOARD.md`
- `.md/plan/ai/13_DEBUG_EDITOR.md`

현재 코드 기준 핵심 관찰:

- 프로젝트는 `Engine`, `Client`, `Server`, `Tools` 4개 Visual Studio 프로젝트로 나뉜다.
- 런타임 게임 오브젝트는 `CWorld` + ECS 컴포넌트 구조다.
- AI와 직접 연결 가능한 기존 축은 다음이다.
  - `Engine/Public/ECS/World.h`: `CWorld`, `ForEach`, `GetComponent`, `HasComponent`
  - `Engine/Public/ECS/Components/CoreComponents.h`: `VelocityComponent`, `HealthComponent`, `AIStateComponent`
  - `Engine/Public/ECS/Components/GameplayComponents.h`: `ChampionComponent`, `MinionComponent`, `StructureComponent`, `SkillStateComponent`, `MinionStateComponent`, `CommandQueueComponent`, 상태 이상 컴포넌트
  - `Engine/Public/ECS/Components/NavAgentComponent.h`: 이동 목표와 경로 추적
  - `Engine/Public/ECS/Systems/MinionAISystem.h`, `Engine/Private/ECS/Systems/MinionAISystem.cpp`: 현재 가장 실질적인 AI 시스템
  - `Engine/Public/ECS/Systems/NavigationSystem.h`, `Engine/Private/ECS/Systems/NavigationSystem.cpp`: `NavAgentComponent`를 `VelocityComponent`로 변환
  - `Client/Public/GameObject/SkillDef.h`, `Client/Private/GameObject/SkillTable.cpp`: 챔피언별 스킬 메타데이터
- 현재 `Client/Public/GamePlay/CommandQueueComponent.h`와 `Engine/Public/ECS/Components/GameplayComponents.h`에 서로 다른 `CommandQueueComponent` 정의가 존재한다. 장기적으로 AI는 서버/엔진 권위 계층에서 돌아야 하므로 `Engine/Public/ECS/Components/GameplayComponents.h` 쪽 명령 모델을 기준으로 통일하거나, 클라이언트 전용 입력 큐와 엔진 공용 명령 큐의 이름을 분리해야 한다.
- `MinionAISystem`은 이미 병렬 판단 패스와 적용 패스를 분리한다. MCTS/RL도 이 패턴을 따라야 한다. 즉, 병렬 구간에서는 월드 읽기와 후보 계산만 하고, 실제 ECS 변경은 메인 적용 패스에서 한다.

## 1. 목표

이 계획의 목표는 Winters 프로젝트에 다음 두 계층을 단계적으로 붙이는 것이다.

1. MCTS 기반 전술 AI
   - 1v1, 2v2, 소규모 교전에서 다음 행동을 선택한다.
   - 초기에 챔피언 봇 대상, 이후 미니언/정글/구조물까지 확장한다.
   - 게임 프레임을 막지 않도록 시간 예산 기반 anytime 알고리즘으로 동작한다.

2. RL 기반 정책 AI
   - 초기에는 C++ 런타임에서 추론만 수행한다.
   - 학습은 별도 `Tools/AI` Python 환경에서 진행한다.
   - 모델은 ONNX로 export한 뒤 C++에서 로드한다.
   - MCTS는 RL 모델의 fallback, 검증기, self-play 데이터 생성기로도 사용한다.

핵심 원칙:

- 실제 ECS 월드를 rollout마다 복사하지 않는다.
- MCTS/RL이 직접 `TransformComponent`, `HealthComponent`를 수정하지 않는다.
- `BattleState`라는 순수 C++ 스냅샷을 만들어 빠르게 시뮬레이션한다.
- 최종 선택만 `CommandQueueComponent`, `NavAgentComponent`, `VelocityComponent` 또는 향후 `BotIntentComponent`로 변환한다.
- 모든 AI 판단은 결정론적 seed를 받을 수 있어야 한다.
- 디버깅 가능성이 성능만큼 중요하다. 선택된 action, score, visits, reward를 ImGui/로그에서 확인 가능해야 한다.

## 2. 권장 디렉터리 구조

기존 `.md/plan/ai/01_ARCHITECTURE.md`는 `Engine/Public/AI` 구조를 제안한다. 현재 프로젝트의 실제 빌드 방식은 `Engine/Include/Engine.vcxproj`에 파일을 명시하는 방식이므로, 다음 파일을 추가하면 `.vcxproj`와 `.filters`도 같이 갱신해야 한다.

```text
Engine/Public/AI/
  Core/
    BotTypes.h
    BotAction.h
    BotComponent.h
    BotDecisionContext.h
    BotFeatureExtractor.h
  MCTS/
    BattleState.h
    BattleStateBuilder.h
    TacticalSimulator.h
    MCTSNode.h
    MCTSTree.h
    MCTSConfig.h
    MCTSSystem.h
  RL/
    RLTypes.h
    IBotEnv.h
    RLPolicy.h
    NeuralPolicyComponent.h
    NeuralPolicySystem.h
    ModelRegistry.h
  Debug/
    AIDebugSnapshot.h
    AIDebugSystem.h

Engine/Private/AI/
  Core/
    BotFeatureExtractor.cpp
  MCTS/
    BattleStateBuilder.cpp
    TacticalSimulator.cpp
    MCTSTree.cpp
    MCTSSystem.cpp
  RL/
    RLPolicy.cpp
    NeuralPolicySystem.cpp
    ModelRegistry.cpp
  Debug/
    AIDebugSystem.cpp

Tools/AI/
  train_ppo.py
  winters_env.py
  export_onnx.py
  evaluate_policy.py
  configs/
    ppo_1v1.yaml
    ppo_2v2.yaml
  datasets/
  models/
```

중요: `EngineSDK/inc`는 현재 `Engine/Public`과 같은 헤더들이 복사되어 있는 형태다. 새 public 헤더를 추가하면 빌드/배포 파이프라인에 맞춰 `EngineSDK/inc/AI/...` 동기화 정책도 정해야 한다.

## 3. 단계별 구현 로드맵

### Phase 0. 정리와 기반 점검

목표:

- AI가 얹힐 ECS 명령 경로를 먼저 안정화한다.
- `CommandQueueComponent` 중복 정의 문제를 정리한다.
- 챔피언/미니언/구조물/상태 이상 스냅샷을 안전하게 읽을 수 있게 한다.

작업:

1. `CommandQueueComponent` 기준 확정
   - 엔진 공용: `Engine/Public/ECS/Components/GameplayComponents.h`의 `Command`, `CommandQueueComponent`
   - 클라이언트 입력 전용: `Client/Public/GamePlay/CommandQueueComponent.h`는 `ClientInputQueueComponent`처럼 이름 변경 검토
   - AI 출력은 엔진 공용 `CommandQueueComponent` 또는 새 `BotIntentComponent`로 보낸다.

2. `BotComponent` 추가

```cpp
// Engine/Public/AI/Core/BotComponent.h
#pragma once
#include "WintersTypes.h"
#include "GameContext.h"

enum class EBotDifficulty : uint8_t
{
    Intro,
    Beginner,
    Intermediate,
    Master,
    Grandmaster
};

enum class EBotBrainType : uint8_t
{
    RuleBased,
    MCTS,
    NeuralPolicy,
    Hybrid
};

struct BotComponent
{
    uint32_t botId = 0;
    EBotDifficulty difficulty = EBotDifficulty::Beginner;
    EBotBrainType brainType = EBotBrainType::RuleBased;
    eChampion champion = eChampion::END;
    uint8_t role = 0;
    bool_t bEnabled = true;
    float decisionCooldown = 0.f;
    float nextDecisionIn = 0.f;
};
```

3. `BotAction` 정의

```cpp
// Engine/Public/AI/Core/BotAction.h
#pragma once
#include "WintersMath.h"
#include "ECS/Entity.h"
#include "WintersTypes.h"

enum class EBotActionType : uint8_t
{
    None,
    MoveTo,
    AttackTarget,
    CastSkill,
    Hold,
    Retreat
};

struct BotAction
{
    EBotActionType type = EBotActionType::None;
    EntityID target = NULL_ENTITY;
    Vec3 targetPos{ 0.f, 0.f, 0.f };
    Vec3 direction{ 0.f, 0.f, 1.f };
    uint8_t skillSlot = 0;
    float score = 0.f;
};
```

4. 시스템 실행 순서 확인
   - 현재 `MinionAISystem::GetPhase() == 2`
   - `NavigationSystem`도 `GetPhase()`를 가진다.
   - `MCTSSystem`은 의사결정 시스템이므로 이동/물리보다 앞, 실제 데미지/헬스 처리보다 앞에 위치해야 한다.
   - 권장:
     - Phase 0: 입력/봇 의도 생성
     - Phase 1: MCTS/RL 의사결정
     - Phase 2: 명령 적용/네비게이션
     - Phase 3: 이동
     - Phase 4: 전투/헬스/상태 이상

### Phase 1. BattleState 스냅샷

MCTS와 RL의 공통 기반은 `BattleState`다. 이 구조체는 ECS에서 읽어 온 최소 전투 정보를 담는다.

설계 원칙:

- POD에 가깝게 만든다.
- `std::vector` 남발을 피하고, 초기 버전은 `std::array`와 count를 쓴다.
- EntityID는 디버깅/최종 action mapping에만 사용하고 rollout 계산은 인덱스 기반으로 한다.
- 월드 좌표는 XZ 평면 중심으로 계산한다.

```cpp
// Engine/Public/AI/MCTS/BattleState.h
#pragma once
#include "WintersTypes.h"
#include "WintersMath.h"
#include "ECS/Entity.h"
#include "GameContext.h"

static constexpr uint32_t WINTERS_AI_MAX_UNITS = 16;
static constexpr uint32_t WINTERS_AI_MAX_SKILLS = 5; // BA/Q/W/E/R

enum class EBattleUnitKind : uint8_t
{
    Champion,
    Minion,
    Structure,
    Jungle
};

struct BattleSkillRuntime
{
    float cooldownRemaining = 0.f;
    float cooldownMax = 0.f;
    float range = 0.f;
    float manaCost = 0.f;
};

struct BattleUnit
{
    EntityID entity = NULL_ENTITY;
    EBattleUnitKind kind = EBattleUnitKind::Champion;
    eChampion champion = eChampion::END;
    uint8_t team = 0;

    Vec3 pos{ 0.f, 0.f, 0.f };
    Vec3 velocity{ 0.f, 0.f, 0.f };

    float hp = 0.f;
    float maxHp = 0.f;
    float mana = 0.f;
    float maxMana = 0.f;
    float moveSpeed = 0.f;
    float attackDamage = 0.f;
    float attackRange = 0.f;
    float attackCooldown = 0.f;
    float attackCooldownMax = 1.f;

    uint32_t statusMask = 0; // Stun/Slow/Disarm 등 bit
    BattleSkillRuntime skills[WINTERS_AI_MAX_SKILLS]{};
    bool_t bDead = false;
};

struct BattleState
{
    BattleUnit units[WINTERS_AI_MAX_UNITS]{};
    uint32_t unitCount = 0;

    uint32_t selfIndex = UINT32_MAX;
    uint8_t selfTeam = 0;
    float elapsed = 0.f;
    uint32_t rngState = 0;
};
```

`BattleStateBuilder`는 ECS 월드를 읽어 스냅샷을 만든다.

```cpp
// Engine/Public/AI/MCTS/BattleStateBuilder.h
#pragma once
#include "AI/MCTS/BattleState.h"

class CWorld;

namespace WintersAI
{
    class CBattleStateBuilder final
    {
    public:
        static bool BuildForEntity(
            CWorld& world,
            EntityID self,
            float radius,
            BattleState& outState);

    private:
        static bool AppendChampion(CWorld& world, EntityID id, BattleState& outState);
        static bool AppendMinion(CWorld& world, EntityID id, BattleState& outState);
        static bool AppendStructure(CWorld& world, EntityID id, BattleState& outState);
    };
}
```

구현 원리:

- `self`의 `TransformComponent`와 team을 먼저 찾는다.
- 반경 `radius` 안에 있는 챔피언, 미니언, 구조물을 순회한다.
- `HealthComponent`가 있으면 생존 판정은 `HealthComponent`를 우선한다.
- `ChampionComponent`의 `cooldowns[4]`와 `SkillStateComponent::slots[5]`가 동시에 있을 수 있으므로 하나의 정책을 정한다.
  - 추천: 런타임 스킬 상태는 `SkillStateComponent`를 우선
  - `ChampionComponent::cooldowns`는 네트워크 snapshot 또는 단순 fallback으로 취급
- `StunComponent`, `SlowComponent`, `DisarmComponent`는 `statusMask`로 압축한다.

주의:

- `CWorld::ForEach`는 현재 내부 store를 직접 순회한다. 병렬 작업에서 동시에 world를 수정하면 위험하다.
- 따라서 `BattleStateBuilder`는 메인 스레드 의사결정 수집 단계에서만 호출하거나, MCTS 시스템이 먼저 필요한 스냅샷을 모두 만든 뒤 병렬 MCTS를 돌린다.

### Phase 2. TacticalSimulator

MCTS rollout은 실제 게임 전체 로직을 호출하면 너무 느리고 부작용이 생긴다. 대신 `BattleState` 전용 근사 시뮬레이터를 만든다.

목표:

- 이동, 평타, 단순 스킬, 쿨다운, 사망 판정만 빠르게 근사한다.
- 1 tick은 0.05초 또는 0.1초로 고정한다.
- 결과값은 실제 게임과 완전히 같을 필요는 없지만, 상대적 선택 순위가 맞아야 한다.

```cpp
// Engine/Public/AI/MCTS/TacticalSimulator.h
#pragma once
#include "AI/Core/BotAction.h"
#include "AI/MCTS/BattleState.h"

namespace WintersAI
{
    class CTacticalSimulator final
    {
    public:
        static void ApplyAction(BattleState& state, uint32_t actorIndex, const BotAction& action);
        static void Step(BattleState& state, float dt);
        static bool IsTerminal(const BattleState& state);
        static float Evaluate(const BattleState& state, uint8_t perspectiveTeam);

        static void GenerateActions(
            const BattleState& state,
            uint32_t actorIndex,
            BotAction* outActions,
            uint32_t maxActions,
            uint32_t& outCount);

    private:
        static void ApplyMove(BattleUnit& unit, const Vec3& targetPos, float dt);
        static void ApplyAttack(BattleState& state, uint32_t actorIndex, uint32_t targetIndex);
        static void ApplySkill(BattleState& state, uint32_t actorIndex, const BotAction& action);
        static uint32_t FindUnitIndexByEntity(const BattleState& state, EntityID entity);
    };
}
```

행동 후보 생성:

- 기본 이동: 적 방향 전진, 후퇴, 좌측, 우측, 현재 위치 유지
- 기본 공격: 사거리 안의 적 1~3명
- 스킬: Q/W/E/R 중 쿨다운 0, 마나 충분, 사거리 조건 충족
- 후퇴: 아군 진영 또는 가장 가까운 안전 지점 방향

초기 action 수 제한:

```cpp
static constexpr uint32_t kMaxActionsPerNode = 16;
```

전투 평가 함수:

```cpp
float CTacticalSimulator::Evaluate(const BattleState& state, uint8_t perspectiveTeam)
{
    float allyHp = 0.f;
    float enemyHp = 0.f;
    float allyMax = 0.f;
    float enemyMax = 0.f;
    int allyDead = 0;
    int enemyDead = 0;

    for (uint32_t i = 0; i < state.unitCount; ++i)
    {
        const BattleUnit& u = state.units[i];
        const bool ally = (u.team == perspectiveTeam);
        if (ally)
        {
            allyHp += u.hp;
            allyMax += u.maxHp;
            allyDead += u.bDead ? 1 : 0;
        }
        else
        {
            enemyHp += u.hp;
            enemyMax += u.maxHp;
            enemyDead += u.bDead ? 1 : 0;
        }
    }

    const float allyRatio = allyMax > 0.f ? allyHp / allyMax : 0.f;
    const float enemyRatio = enemyMax > 0.f ? enemyHp / enemyMax : 0.f;
    float value = (allyRatio - enemyRatio);
    value += static_cast<float>(enemyDead - allyDead) * 0.5f;

    if (value > 1.f) value = 1.f;
    if (value < -1.f) value = -1.f;
    return value;
}
```

평가 함수 확장:

- 적 처치: 큰 보상
- 아군 사망: 큰 패널티
- 스킬 명중 가능성: 보상
- 너무 깊게 들어감: 패널티
- 포탑 사거리 진입: 강한 패널티
- 체력 낮을 때 생존 거리 확보: 보상

### Phase 3. MCTS Tree 구현

MCTS는 `Selection -> Expansion -> Simulation/Rollout -> Backpropagation`으로 구성한다.

```cpp
// Engine/Public/AI/MCTS/MCTSNode.h
#pragma once
#include "AI/Core/BotAction.h"
#include "AI/MCTS/BattleState.h"
#include "WintersTypes.h"

namespace WintersAI
{
    static constexpr uint32_t INVALID_MCTS_NODE = UINT32_MAX;

    struct MCTSNode
    {
        uint32_t parent = INVALID_MCTS_NODE;
        uint32_t firstChild = INVALID_MCTS_NODE;
        uint32_t nextSibling = INVALID_MCTS_NODE;
        uint32_t childCount = 0;

        BotAction action{};
        BattleState state{};

        uint32_t visits = 0;
        float totalValue = 0.f;

        uint32_t untriedActionCursor = 0;
        BotAction untriedActions[16]{};
        uint32_t untriedActionCount = 0;

        bool_t bTerminal = false;

        float AverageValue() const
        {
            return visits > 0 ? totalValue / static_cast<float>(visits) : 0.f;
        }
    };
}
```

`std::vector<uint32_t> children`는 편하지만 노드마다 heap allocation이 생긴다. MCTS는 수천~수만 노드를 매 프레임 만들 수 있으므로, 초기부터 pool + sibling linked list를 추천한다.

```cpp
// Engine/Public/AI/MCTS/MCTSTree.h
#pragma once
#include "AI/MCTS/MCTSNode.h"
#include "AI/MCTS/MCTSConfig.h"
#include <vector>

namespace WintersAI
{
    class CMCTSTree final
    {
    public:
        void Reset(const BattleState& root, const MCTSConfig& config);
        void RunForIterations(uint32_t iterations);
        void RunForTime(float milliseconds);
        BotAction GetBestAction() const;

        uint32_t GetNodeCount() const { return static_cast<uint32_t>(m_nodes.size()); }
        const MCTSNode* GetRoot() const;

    private:
        uint32_t Select(uint32_t nodeIndex) const;
        uint32_t Expand(uint32_t nodeIndex);
        float Rollout(BattleState state);
        void Backpropagate(uint32_t nodeIndex, float value);
        float UCB1(uint32_t parentIndex, uint32_t childIndex) const;

        uint32_t AllocateNode();
        void LinkChild(uint32_t parentIndex, uint32_t childIndex);

    private:
        std::vector<MCTSNode> m_nodes;
        MCTSConfig m_config{};
        uint32_t m_root = INVALID_MCTS_NODE;
        uint8_t m_perspectiveTeam = 0;
    };
}
```

```cpp
// Engine/Public/AI/MCTS/MCTSConfig.h
#pragma once
#include "WintersTypes.h"

namespace WintersAI
{
    struct MCTSConfig
    {
        uint32_t maxNodes = 4096;
        uint32_t maxIterations = 512;
        uint32_t rolloutDepth = 40;
        float rolloutDt = 0.05f;
        float explorationC = 1.41421356f;
        float timeBudgetMs = 2.f;
        bool_t bUseHeavyPlayout = true;
    };
}
```

Selection 원리:

```cpp
float CMCTSTree::UCB1(uint32_t parentIndex, uint32_t childIndex) const
{
    const MCTSNode& parent = m_nodes[parentIndex];
    const MCTSNode& child = m_nodes[childIndex];
    if (child.visits == 0)
        return FLT_MAX;

    const float exploit = child.AverageValue();
    const float explore = m_config.explorationC *
        sqrtf(logf(static_cast<float>(parent.visits + 1)) /
              static_cast<float>(child.visits));
    return exploit + explore;
}
```

Expansion 원리:

- 노드 생성 시 `CTacticalSimulator::GenerateActions`로 `untriedActions`를 채운다.
- 아직 시도하지 않은 action이 있으면 하나 꺼낸다.
- 부모 state를 복사하고 action을 적용한 뒤 `Step`을 1회 진행한다.
- 새 state를 자식 노드로 추가한다.

Rollout 원리:

- 완전 random보다 heavy playout이 좋다.
- 예:
  - 체력이 낮으면 후퇴 확률 증가
  - 사거리 안이면 평타/스킬 우선
  - 적이 멀면 접근
  - 군중제어 상태면 hold
- rollout depth는 40 tick, `dt=0.05` 기준 2초부터 시작한다.

Backpropagation 원리:

```cpp
void CMCTSTree::Backpropagate(uint32_t nodeIndex, float value)
{
    while (nodeIndex != INVALID_MCTS_NODE)
    {
        MCTSNode& node = m_nodes[nodeIndex];
        node.visits += 1;
        node.totalValue += value;
        nodeIndex = node.parent;
    }
}
```

상대 행동 처리:

- 초기 버전: rollout 안에서 모든 적은 simple heuristic으로 행동한다.
- 다음 버전: 적도 MCTS action을 샘플링하되 비용이 커지므로 제한한다.
- RL 연동 이후: 적 action prior는 RL policy로 샘플링 가능하다.

### Phase 4. MCTSSystem과 ECS 연결

`MCTSSystem`은 실제 월드를 바꾸지 않고 판단만 수행한 뒤, 최종 action을 명령으로 변환한다.

```cpp
// Engine/Public/AI/MCTS/MCTSSystem.h
#pragma once
#include "ECS/ISystem.h"
#include "AI/MCTS/MCTSTree.h"
#include <unordered_map>

class CJobSystem;

namespace WintersAI
{
    class CMCTSSystem final : public ISystem
    {
    public:
        static std::unique_ptr<CMCTSSystem> Create();

        uint32_t GetPhase() const override { return 1; }
        const char* GetName() const override { return "MCTSSystem"; }
        void Execute(CWorld& world, float dt) override;

        void SetJobSystem(CJobSystem* jobSystem) { m_jobSystem = jobSystem; }
        void SetConfig(const MCTSConfig& config) { m_config = config; }

    private:
        struct PendingDecision
        {
            EntityID self = NULL_ENTITY;
            BotAction action{};
        };

        void CollectCandidates(CWorld& world, float dt);
        void RunDecision(const BattleState& state, PendingDecision& outDecision);
        void ApplyDecisions(CWorld& world);
        void ApplyActionToCommand(CWorld& world, EntityID self, const BotAction& action);

    private:
        CJobSystem* m_jobSystem = nullptr;
        MCTSConfig m_config{};
        std::vector<BattleState> m_states;
        std::vector<PendingDecision> m_decisions;
    };
}
```

연결 방식:

1. `CollectCandidates`
   - `BotComponent + ChampionComponent + TransformComponent + HealthComponent`를 가진 엔티티 순회
   - `BotComponent::brainType == MCTS` 또는 `Hybrid`만 대상
   - `nextDecisionIn` 감소
   - 판단 주기가 됐으면 `BattleStateBuilder::BuildForEntity` 호출

2. `RunDecision`
   - 각 `BattleState`마다 `CMCTSTree` 생성 또는 thread local 재사용
   - 시간 예산만큼 실행
   - `GetBestAction` 저장

3. `ApplyDecisions`
   - 메인 스레드에서 ECS 변경
   - `CommandQueueComponent`가 있으면 command push
   - 없으면 `NavAgentComponent`, `VelocityComponent` fallback

`BotAction`을 기존 명령으로 변환:

```cpp
void CMCTSSystem::ApplyActionToCommand(CWorld& world, EntityID self, const BotAction& action)
{
    if (!world.HasComponent<CommandQueueComponent>(self))
    {
        if (action.type == EBotActionType::MoveTo &&
            world.HasComponent<NavAgentComponent>(self))
        {
            auto& nav = world.GetComponent<NavAgentComponent>(self);
            nav.vTarget = action.targetPos;
            nav.bHasGoal = true;
            nav.bPathDirty = true;
        }
        return;
    }

    auto& queue = world.GetComponent<CommandQueueComponent>(self);
    Command cmd{};

    switch (action.type)
    {
    case EBotActionType::MoveTo:
        cmd.type = eCommandType::Move;
        cmd.vTargetPos = action.targetPos;
        break;
    case EBotActionType::AttackTarget:
        cmd.type = eCommandType::Attack;
        cmd.targetEntity = action.target;
        break;
    case EBotActionType::CastSkill:
        cmd.type = eCommandType::Cast;
        cmd.iSkillSlot = action.skillSlot;
        cmd.targetEntity = action.target;
        cmd.vTargetPos = action.targetPos;
        break;
    case EBotActionType::Hold:
        cmd.type = eCommandType::Hold;
        break;
    default:
        return;
    }

    queue.queue.push_back(cmd);
}
```

주의:

- 위 코드는 `GameplayComponents.h`의 `CommandQueueComponent` 기준이다.
- 만약 클라이언트 쪽 `PulseCommand` 모델을 계속 쓸 경우 `BotAction -> PulseCommand` 변환기를 별도로 둬야 한다.

### Phase 5. RL 공통 FeatureExtractor

RL은 state vector를 먹고 action distribution을 낸다. MCTS와 동일한 `BattleState`에서 feature를 뽑으면 일관성이 유지된다.

```cpp
// Engine/Public/AI/Core/BotFeatureExtractor.h
#pragma once
#include "AI/MCTS/BattleState.h"
#include <vector>

namespace WintersAI
{
    struct FeatureLayout
    {
        uint32_t unitStride = 32;
        uint32_t maxUnits = WINTERS_AI_MAX_UNITS;
        uint32_t globalOffset = 0;
        uint32_t unitOffset = 16;
        uint32_t totalDim = 16 + WINTERS_AI_MAX_UNITS * 32;
    };

    class CBotFeatureExtractor final
    {
    public:
        static FeatureLayout GetDefaultLayout();
        static void Extract(
            const BattleState& state,
            const FeatureLayout& layout,
            std::vector<float>& outFeatures);
    };
}
```

Feature 설계:

- Global features
  - self hp ratio
  - self mana ratio
  - self team
  - elapsed normalized
  - ally count
  - enemy count
  - nearest enemy distance
  - under crowd control 여부
- Unit features, 고정 16개
  - exists mask
  - is self
  - is ally
  - kind one-hot
  - champion id normalized
  - relative x/z
  - distance
  - hp ratio
  - mana ratio
  - move speed
  - attack range
  - cooldown Q/W/E/R
  - status mask bits

정규화:

- 거리: `distance / 50.0f` 후 clamp
- 체력/마나: ratio
- cooldown: `remaining / max(0.1f, maxCooldown)`
- enum: one-hot 또는 `id / END`

### Phase 6. RL 추론 계층

처음부터 C++에서 PPO 학습을 구현하지 않는다. C++은 추론만 담당하고 학습은 Python이 담당한다.

```cpp
// Engine/Public/AI/RL/NeuralPolicyComponent.h
#pragma once
#include "WintersTypes.h"

struct NeuralPolicyComponent
{
    uint32_t modelId = 0;
    float inferenceInterval = 0.1f;
    float nextInferenceIn = 0.f;
    float lastValue = 0.f;
    uint32_t lastActionIndex = 0;
    bool_t bFallbackToMCTS = true;
};
```

```cpp
// Engine/Public/AI/RL/RLPolicy.h
#pragma once
#include "AI/Core/BotAction.h"
#include "AI/MCTS/BattleState.h"
#include <vector>

namespace WintersAI
{
    class IRLPolicy
    {
    public:
        virtual ~IRLPolicy() = default;
        virtual bool Load(const wchar_t* path) = 0;
        virtual bool Infer(
            const std::vector<float>& features,
            std::vector<float>& outActionLogits,
            float& outValue) = 0;
    };

    class CRLActionMapper final
    {
    public:
        static BotAction DecodeDiscreteAction(
            const BattleState& state,
            uint32_t actionIndex);

        static uint32_t EncodeAction(const BotAction& action);
        static uint32_t GetDiscreteActionCount();
    };
}
```

초기 discrete action space:

```text
0  Hold
1  MoveToEnemy
2  Retreat
3  StrafeLeft
4  StrafeRight
5  AttackNearest
6  CastQNearest
7  CastWNearest
8  CastENearest
9  CastRBest
```

나중에 action head 분리:

- action type categorical
- target unit categorical
- ground offset continuous
- skill slot categorical

이 구조가 PPO에는 더 자연스럽지만, C++ 추론/디버깅 초기는 discrete가 훨씬 단순하다.

### Phase 7. NeuralPolicySystem

```cpp
// Engine/Public/AI/RL/NeuralPolicySystem.h
#pragma once
#include "ECS/ISystem.h"
#include "AI/Core/BotFeatureExtractor.h"
#include "AI/RL/RLPolicy.h"
#include <memory>
#include <vector>

namespace WintersAI
{
    class CNeuralPolicySystem final : public ISystem
    {
    public:
        static std::unique_ptr<CNeuralPolicySystem> Create();

        uint32_t GetPhase() const override { return 1; }
        const char* GetName() const override { return "NeuralPolicySystem"; }
        void Execute(CWorld& world, float dt) override;

    private:
        void ApplyPolicyAction(CWorld& world, EntityID self, const BotAction& action);

    private:
        FeatureLayout m_layout{};
        std::vector<float> m_features;
        std::vector<float> m_logits;
    };
}
```

작동:

- `BotComponent::brainType == NeuralPolicy` 또는 `Hybrid`인 엔티티 대상
- `BattleStateBuilder`로 스냅샷 생성
- `CBotFeatureExtractor::Extract`
- `IRLPolicy::Infer`
- logits에서 action index 선택
  - 평가 모드: argmax
  - self-play 수집 모드: temperature sampling
- `CRLActionMapper::DecodeDiscreteAction`
- `CommandQueueComponent`로 변환

Hybrid 정책:

- RL confidence가 낮거나 불법 action이면 MCTS fallback
- MCTS의 방문 수 분포를 policy target으로 저장하면 imitation/RL warm-start에 사용할 수 있다.
- Grandmaster 난이도는 `policy prior + MCTS search` 구조까지 확장 가능하다.

### Phase 8. IBotEnv와 Python 학습

RL 학습은 게임 엔진을 환경으로 보고 `Reset`, `Step`을 호출한다.

```cpp
// Engine/Public/AI/RL/IBotEnv.h
#pragma once
#include <vector>
#include <string>

namespace WintersAI
{
    struct BotEnvStepResult
    {
        std::vector<float> nextState;
        float reward = 0.f;
        bool done = false;
        std::string info;
    };

    class IBotEnv
    {
    public:
        virtual ~IBotEnv() = default;

        virtual std::vector<float> Reset(uint32_t seed) = 0;
        virtual BotEnvStepResult Step(uint32_t discreteAction) = 0;

        virtual int GetStateDim() const = 0;
        virtual int GetActionDim() const = 0;
    };
}
```

현실적인 구현 순서:

1. C++ headless mode를 바로 만들기 전에 `TacticalSimulator` 기반 lightweight env를 만든다.
   - 빠르다.
   - 결정론적이다.
   - MCTS와 같은 state/action/reward를 사용한다.

2. 이후 실제 `CWorld` 기반 env를 만든다.
   - `Scene_InGame` 의존을 분리해야 한다.
   - 서버 권위 구조가 완성되면 Server 쪽에서 돌리는 것이 맞다.

Python 학습 스크립트 초안:

```python
# Tools/AI/train_ppo.py
from stable_baselines3 import PPO
from winters_env import WintersTacticalEnv

env = WintersTacticalEnv(config_path="Tools/AI/configs/ppo_1v1.yaml")

model = PPO(
    "MlpPolicy",
    env,
    learning_rate=1e-4,
    n_steps=2048,
    batch_size=256,
    n_epochs=10,
    gamma=0.99,
    gae_lambda=0.95,
    clip_range=0.2,
    ent_coef=0.01,
    verbose=1,
)

model.learn(total_timesteps=10_000_000)
model.save("Tools/AI/models/winters_ppo_1v1")
```

ONNX export:

```python
# Tools/AI/export_onnx.py
import torch

def export_policy(model, output_path: str, state_dim: int):
    dummy = torch.randn(1, state_dim)
    torch.onnx.export(
        model.policy,
        dummy,
        output_path,
        input_names=["state"],
        output_names=["action_logits", "value"],
        opset_version=17,
    )
```

주의:

- Stable-Baselines3 policy를 그대로 ONNX export할 때 출력 signature가 예상과 다를 수 있다.
- 실제 운영용으로는 작은 PyTorch module wrapper를 만들어 `forward(state) -> logits, value`만 export하는 편이 안정적이다.

### Phase 9. Reward 설계

초기 reward는 단순해야 한다. 너무 많은 shaping은 exploit을 만든다.

1v1 전투 reward:

```text
reward =
  + 1.0 * enemy_hp_delta_ratio
  - 1.0 * self_hp_delta_ratio
  + 2.0 * enemy_killed
  - 2.0 * self_dead
  + 0.1 * valid_skill_cast
  - 0.1 * invalid_action
  - 0.001 per step
```

2v2 reward:

```text
reward =
  team_reward * 0.7 + individual_reward * 0.3
```

MOBA 확장 reward:

- 라인 유지
- CS 획득
- 오브젝트 기여
- 포탑 사거리 위험 회피
- 아군과 거리 유지
- 과도한 후퇴 방지

금지:

- "이동할 때마다 보상"처럼 목적과 무관한 dense reward
- 스킬을 쓰기만 하면 큰 보상
- 체력 변화보다 위치 reward가 지나치게 큰 구조

### Phase 10. MCTS와 RL의 결합

권장 순서:

1. RuleBased
   - 현재 `MinionAISystem`과 유사한 단순 의사결정

2. MCTS
   - 소규모 교전에서 rule보다 강한 전술 판단

3. Imitation
   - MCTS가 고른 action을 dataset으로 저장
   - `state -> MCTS best action` supervised learning

4. RL
   - imitation 모델을 PPO warm-start
   - self-play로 개선

5. Hybrid
   - RL policy가 prior를 제공
   - MCTS는 policy prior를 이용해 탐색 폭을 줄임

PUCT 형태:

```text
score = Q(s,a) + c_puct * P(s,a) * sqrt(N(s)) / (1 + N(s,a))
```

여기서:

- `Q(s,a)`: MCTS 평균 가치
- `P(s,a)`: neural policy prior
- `N(s)`: 부모 방문 수
- `N(s,a)`: 자식 방문 수

이를 위해 `MCTSNode`에 prior를 추가한다.

```cpp
float prior = 0.f;
```

### Phase 11. 데이터 로깅

MCTS/RL 품질을 올리려면 재현 가능한 로그가 필요하다.

```cpp
// Engine/Public/AI/Debug/AIDebugSnapshot.h
#pragma once
#include "AI/Core/BotAction.h"
#include "WintersTypes.h"

struct AIDebugActionStat
{
    BotAction action{};
    uint32_t visits = 0;
    float averageValue = 0.f;
    float prior = 0.f;
};

struct AIDebugSnapshot
{
    EntityID bot = NULL_ENTITY;
    uint32_t frame = 0;
    float decisionMs = 0.f;
    uint32_t nodeCount = 0;
    BotAction chosen{};
    AIDebugActionStat rootActions[16]{};
    uint32_t rootActionCount = 0;
};
```

저장 포맷:

- 개발 초기: JSONL
- 대량 학습: binary 또는 parquet 변환

예:

```json
{"frame":1204,"bot":33,"state":[...],"action":6,"reward":0.12,"done":false}
```

로그 종류:

- `mcts_decision.jsonl`: root action visits/value
- `rl_episode.jsonl`: state/action/reward/done
- `eval_match.jsonl`: seed, policy version, win/loss, damage, deaths

### Phase 12. 디버그 UI

기존 문서 `13_DEBUG_EDITOR.md` 방향과 맞춰 ImGui 패널을 추가한다.

권장 패널:

- selected bot id
- brain type
- difficulty
- last decision ms
- MCTS node count
- root action table
  - action
  - visits
  - average value
  - prior
- selected action
- RL logits top-k
- feature vector min/max/NaN 검사
- fallback 발생 횟수

중요:

- AI 디버그 UI는 Client 전용이어도 되지만, 원본 데이터는 Engine의 `AIDebugSnapshot`으로 유지한다.
- 그래야 Server/headless 학습에서도 같은 로그를 저장할 수 있다.

### Phase 13. 성능 예산

초기 목표:

```text
Beginner      RuleBased only
Intermediate 128~256 MCTS iterations, 1~2ms
Master       512~1024 MCTS iterations, 2~4ms
Grandmaster  RL prior + 1024+ MCTS iterations, 4~8ms
```

최적화 순서:

1. `BattleState` 복사 비용 줄이기
2. MCTS node pool reserve
3. action 수 제한
4. rollout depth 제한
5. heavy playout 단순화
6. per-bot decision interval 적용
7. 여러 봇이 같은 frame에 판단하지 않도록 stagger
8. JobSystem으로 bot 단위 병렬화

주의:

- `CWorld` 접근은 병렬로 하지 않는 편이 안전하다.
- 병렬화 대상은 이미 만들어진 `BattleState`에 대한 `CMCTSTree` 실행이다.
- `MinionAISystem`처럼 decision buffer와 apply pass를 분리한다.

### Phase 14. 테스트 계획

단위 테스트가 아직 별도 프로젝트로 보이지 않으므로, 우선 `Tools` 또는 개발용 console test를 추가하는 방식이 현실적이다.

테스트 항목:

1. `BattleStateBuilder`
   - self entity가 없으면 false
   - 죽은 entity는 dead로 snapshot
   - 반경 밖 entity 제외
   - team 분류 정확성

2. `TacticalSimulator`
   - 이동 action 후 위치 변화
   - 사거리 안 평타 후 HP 감소
   - 쿨다운 중 스킬 불가
   - terminal 판정
   - reward clamp

3. `MCTSTree`
   - 빈 action이면 Hold 반환
   - 방문 수 증가
   - child average value 계산
   - 같은 seed에서 같은 action
   - 시간 예산 초과 방지

4. `RL FeatureExtractor`
   - 출력 dimension 고정
   - NaN 없음
   - 정규화 범위 검사
   - unit ordering 결정론 유지

5. 통합
   - MCTS 봇 vs idle 봇: 90% 이상 승리
   - MCTS 봇 vs random 봇: 70% 이상 승리
   - RL policy가 invalid action을 내면 fallback 정상 동작

### Phase 15. 실제 구현 순서 체크리스트

1. `BotComponent`, `BotAction` 추가
2. `BattleState`, `BattleStateBuilder` 추가
3. `TacticalSimulator` 추가
4. `MCTSNode`, `MCTSConfig`, `MCTSTree` 추가
5. 개발용 local test main 또는 간단한 debug command 추가
6. `MCTSSystem` 추가
7. `Engine.vcxproj`, `Engine.vcxproj.filters` 갱신
8. `Scene_InGame` 또는 entity 생성부에서 테스트 봇에 `BotComponent` 부착
9. `CommandQueueComponent` 출력 경로 정리
10. ImGui debug snapshot 출력
11. `BotFeatureExtractor` 추가
12. `IBotEnv`와 lightweight simulator env 작성
13. `Tools/AI/train_ppo.py` 작성
14. ONNX export와 C++ model loader 추가
15. `NeuralPolicySystem` 추가
16. MCTS fallback과 hybrid policy 연결

## 4. 위험 요소와 선결 과제

### CommandQueue 중복

현재 엔진 공용 `CommandQueueComponent`와 클라이언트 `PulseCommand` 기반 큐가 동시에 존재한다. AI는 서버/엔진 로직이어야 하므로, 최종적으로 다음 중 하나를 택해야 한다.

추천:

- `Engine/Public/ECS/Components/GameplayComponents.h`의 `CommandQueueComponent`를 공용 명령 큐로 유지
- `Client/Public/GamePlay/CommandQueueComponent.h`는 클라이언트 입력 전용으로 rename
- `Scene_InGame`의 직접 입력/스킬 처리 로직을 점진적으로 공용 command 처리 시스템으로 이동

### Skill 실행 경로

현재 스킬은 `Scene_InGame` 쪽 로컬 예측/연출 코드에 많이 묶여 있다. AI가 `Cast` command를 내도 실제 스킬 발동까지 이어지는 공용 시스템이 부족할 수 있다.

단계적 대응:

- MCTS 초기 버전은 평타/이동 중심으로 검증
- 스킬은 simulator 안에서만 평가
- 실제 런타임 cast는 `SkillDispatchSystem` 또는 새 `ChampionCombatSystem`을 살린 뒤 연결

### ECS 복사 문제

`CWorld` 전체 복사는 불가능하거나 비싸다. MCTS rollout은 반드시 `BattleState`에서만 수행한다.

### 결정론

RL 학습과 MCTS 비교에는 seed가 필요하다.

필수:

- MCTS config에 seed 추가
- BattleState에 rngState 저장
- rollout random은 `std::random_device` 금지
- 간단한 xorshift 또는 PCG 사용

### 네트워크 권위

장기적으로 AI는 Server에서 돌아야 한다. Client에서만 돌아가면 디버그와 오프라인 봇전은 편하지만, 실제 멀티플레이 권위 구조와 충돌한다.

권장:

- Phase 1~4: Client local scene에서 빠른 검증
- Phase 5 이후: Server 또는 shared simulation layer로 이전

## 5. 완료 기준

MCTS 1차 완료:

- 봇 챔피언이 `BotComponent + MCTS`로 생성된다.
- 1v1에서 idle 또는 random 상대를 안정적으로 이긴다.
- MCTS root action visits/value를 UI 또는 로그로 볼 수 있다.
- 프레임당 AI 판단 시간이 설정한 예산을 넘지 않는다.
- 같은 seed에서 같은 결과가 나온다.

RL 1차 완료:

- `BattleState -> feature vector`가 고정 dimension으로 추출된다.
- Python PPO가 lightweight env에서 학습된다.
- ONNX 모델이 생성된다.
- C++ `NeuralPolicySystem`이 모델을 로드해 action을 낸다.
- invalid action 시 MCTS 또는 rule fallback이 작동한다.

Hybrid 완료:

- RL prior를 MCTS expansion/selection에 반영한다.
- pure MCTS보다 같은 시간 예산에서 더 높은 승률을 보인다.
- policy version별 evaluation log가 남는다.

## 6. 권장 첫 PR 범위

첫 구현 PR은 작게 끊는 것이 좋다.

포함:

- `BotComponent.h`
- `BotAction.h`
- `BattleState.h`
- `BattleStateBuilder.h/.cpp`
- `TacticalSimulator.h/.cpp`
- `MCTSConfig.h`
- `MCTSNode.h`
- `MCTSTree.h/.cpp`
- `Engine.vcxproj`, `Engine.vcxproj.filters`

제외:

- ONNX
- Python 학습
- UI
- 서버 연동
- 복잡한 스킬 발동

첫 PR의 목적은 "MCTS core가 순수 C++ BattleState에서 제대로 돌아간다"를 증명하는 것이다. ECS 연결은 두 번째 PR에서 붙이는 것이 안정적이다.
