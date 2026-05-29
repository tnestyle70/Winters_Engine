# Ch9. AI (BehaviorTree / Blackboard / EQS / Perception / NavMesh / Crowd)

> Winters 현재: `Shared/GameSim/Systems/BotLaneAISystem.cpp` (S10 Stage1 smoke pass), `Engine/Public/AI/` 스켈레톤.
> CLAUDE.md 2026-05-12 박제: **Bot AI = server-side GameCommand producer**. Transform/Health/SkillState 직접 수정 금지.
> 레퍼런스: `UnrealEngine/Engine/Source/Runtime/AIModule/Classes/BehaviorTree/`, `Runtime/NavigationSystem/Public/`.

---

## 1. 기초 원리 — AI의 4가지 일

1. **상태 결정** (BehaviorTree / FSM / Goal Oriented / Utility)
2. **공간 인식** (NavMesh / EQS / Perception)
3. **타겟 선택** (priority / threat / role)
4. **행동 실행** (GameCommand 발행 — Winters 불변식)

이 4가지를 따로 풀고 합치는 게 모던 게임 AI. 모두 한 시스템에 박으면 BotLaneAI 1개가 5000줄짜리 if-else가 된다 (Winters S10 stage 1 한계점에 도달).

---

## 2. 핵심 — UE5 AI 5대 구성

### 2.1 BehaviorTree

```text
Selector (자식 중 첫 성공)
├── Sequence (모든 자식 성공)
│   ├── Decorator [Health < 30%]
│   ├── Task: Find Healer
│   └── Task: Move To Healer
├── Sequence
│   ├── Decorator [HasTarget]
│   └── Task: Attack Target
└── Task: Patrol (fallback)
```

각 node는:
- **Composite** (Selector / Sequence / Parallel / RandomSelector)
- **Decorator** (조건 검사. 실패 시 해당 분기 skip)
- **Service** (백그라운드 갱신. 매 N 초 blackboard 업데이트)
- **Task** (실제 행동 — 보통 비동기, Latent)

UE5 `Source/Runtime/AIModule/Classes/BehaviorTree/BehaviorTreeComponent.h:25~60`:

```cpp
struct FBTNodeExecutionInfo
{
    /** index of first task allowed to be executed */
    FBTNodeIndex SearchStart;
    FBTNodeIndex SearchEnd;
    TObjectPtr<const UBTCompositeNode> ExecuteNode;
    uint16 ExecuteInstanceIdx;
    TEnumAsByte<EBTNodeResult::Type> ContinueWithResult;
    uint8 bTryNextChild : 1;
    uint8 bIsRestart : 1;
};
```

**핵심 동작**: tree를 매 tick 위→아래로 traverse가 아니라 **event-driven re-search**. Decorator condition이 바뀌면 affected subtree만 재평가. 1000 NPC도 cheap.

### 2.2 Blackboard

NPC 1개당 1개. AI의 working memory.

```text
Blackboard for NPC_42:
  TargetActor    : Actor*    (현재 적)
  HomeLocation   : Vector    (귀환 지점)
  LastKnownEnemyPos : Vector
  CurrentHealth  : float
  IsAlerted      : bool
```

BT의 task/decorator/service는 모두 blackboard만 읽고 씀. 직접 actor 만지면 thread-unsafe / race / 결합도 폭주.

### 2.3 EQS (Environment Query System)

"가장 좋은 위치를 찾아라" 문제.

```text
Query: BestSniperSpot
  Generator: PointsInDonut(min=10m, max=30m around player)
  Tests:
    - Trace LOS to player (binary)
    - Distance to player (scoring, lower=worse)
    - Height advantage (scoring, higher=better)
    - Not visible by other allies (scoring)
  → 가중치 sum → 최고점 위치 반환
```

`Source/Runtime/AIModule/Classes/EnvironmentQuery/`. 보스 텔레포트 위치, 엄폐, 회피 지점, 정찰 경로에 사용.

### 2.4 Perception

시각/청각/촉각/팀 정보 입력 처리.

```cpp
class UAIPerceptionComponent
{
    void RegisterSenseClass(UAISense* Sense);  // Sight / Hearing / Damage / Touch / Team

    // sense별 stimulus 등록
    void OnStimulusReceived(AActor* SourceActor, FAIStimulus Stimulus);

    // 시야각 / 시야 거리 / 측면 시야
    struct FSightConfig { float SightRadius; float LoseSightRadius; float PeripheralVisionAngleDegrees; };
};
```

GTA6 NPC가 "총 소리 듣고 돌아본다" = hearing stimulus.
엘든링 적이 "벽 뒤로 도망갔는데 잠깐 후에 다시 찾는다" = sight + last known location memory.

### 2.5 NavMesh + Crowd

**NavMesh** (Recast):
1. 레벨 mesh를 voxelize → walkable surface 추출
2. region 분할 → contour
3. triangle mesh (NavMesh) 생성
4. Detour로 A* 경로 탐색

`UnrealEngine/Engine/Source/Runtime/NavigationSystem/Public/`:
```text
NavigationData.h           NavMesh 추상
NavCollision.h             actor의 collision → NavMesh build input
NavAreas/NavArea*.h        영역 (점프 가능, 수영, 사다리, blocked)
NavFilters/NavigationQueryFilter.h  쿼리 시 영역 가중치
CrowdManagerBase.h         ORCA/RVO crowd avoidance
```

**Crowd avoidance** (ORCA):
- 100명이 동시에 길 찾기 → 서로 안 부딪히게 path 보정
- 각 agent가 "내가 이 속도로 가면 N초 안에 충돌"을 미리 계산하고 회피
- 군중씬, RTS, MMO 길드원 이동에 필수

---

## 3. 심화

### 3.1 GOAP (Goal Oriented Action Planning)

"적 죽이기" 목표 → action 시퀀스를 plan: `(이동 → 무기뽑기 → 조준 → 발사)`. F.E.A.R. (2005)가 시조. BT보다 동적이지만 디버그 어려움.

### 3.2 Utility AI

각 action에 utility score 함수. 매 tick 최고 점수 action 실행.

```text
Action: Heal
  Utility = (1 - HP/MaxHP) × 0.8 + (HasHealItem ? 0.2 : 0)

Action: Attack
  Utility = HasTarget × InRange × (1 - Cooldown/Total)
```

The Sims, Far Cry 시리즈가 이 패턴. BT보다 emergent behavior가 자연스러움.

### 3.3 ML / RL AI

OpenAI Five (Dota2), AlphaStar (SC2), AI Town (LLM NPC). 프로덕션 게임에 일부 도입 중이지만 일반화 어려움. 트렌드.

Winters memory `project_session_2026_04_28_minion_stuck.md` 박제:
> MCTS/RL 도입 시 silent fail 환경 모델 = sim2real 격차.

→ RL 도입 전에 deterministic / observable 환경부터 확보.

### 3.4 Crowd LOD

먼 NPC는 정밀 시뮬 안 함:
- LOD0: 풀 BT + perception + animation
- LOD1: 단순 FSM + 거리만
- LOD2: 위치 보간만 + crowd 애니
- LOD3: 안 그리고 안 시뮬

엘든링 전쟁터, GTA6 시내 군중에 필수.

### 3.5 서버 권위 AI

AAA 멀티에서는 AI도 서버에서 돌고 클라엔 snapshot만 전송 (그냥 다른 플레이어처럼). 클라 prediction 안 함.

Winters는 이 원칙 박제됨 (S10 Stage 1 통과).

---

## 4. Winters 매핑

### 4.1 현재 상태

```cpp
// Shared/GameSim/Systems/BotLaneAISystem.cpp
// S10 stage 1: lane 이동 / 적 미니언 평타 / 챔프 대상 / 타워 회피
// 입력: 게임 상태, 출력: m_pendingExecCommands (GameCommand 시퀀스)
```

CLAUDE.md 2026-05-12 박제:
> Bot AI는 `Transform`, `Health`, `SkillState`, `MoveTarget` gameplay 결과를 직접 수정하지 않는다.
> 직접 갱신 가능한 값은 `BotLaneAIComponent` 판단/debug 상태뿐이다.

이게 Ch9 전체에 걸쳐 유지되어야 할 불변식.

### 4.2 Ch9 추가/확장 헤더 (제안)

```cpp
// Engine/Public/AI/BehaviorTree.h
class WINTERS_ENGINE CBehaviorTree
{
public:
    void Tick(BTContext& ctx);            // event-driven re-search
    void Abort();
private:
    BTNodeIndex m_activeIndex;
};

enum class eBTResult : u8_t { Success, Failure, InProgress, Aborted };

class IBTNode
{
public:
    virtual ~IBTNode() = default;
    virtual eBTResult Execute(BTContext& ctx) = 0;
    virtual void Abort(BTContext& ctx) {}
};

// Engine/Public/AI/Blackboard.h
class WINTERS_ENGINE CBlackboard
{
public:
    template<typename T> T    Get(BBKey key) const;
    template<typename T> void Set(BBKey key, const T& v);

    void RegisterChangeNotify(BBKey key, std::function<void()> cb);
};

// Engine/Public/AI/EQS.h
struct EQSQueryDesc
{
    enum class eGenerator { PointsOnGrid, PointsInDonut, PointsAroundActor };
    enum class eTest      { LOS, Distance, Visibility, NavReachable, Custom };
    eGenerator generator;
    std::vector<EQSTest> tests;
};
Vec3 EQS_RunQuery(const EQSQueryDesc& desc, const EQSContext& ctx);

// Engine/Public/AI/Perception.h
struct AIStimulus
{
    enum class eSense { Sight, Hearing, Damage, Touch };
    eSense       sense;
    EntityID     source;
    Vec3         location;
    f32_t        strength;
    f32_t        receivedTime;
};
class WINTERS_ENGINE CPerceptionComponent
{
public:
    void OnStimulus(const AIStimulus& s);
    std::vector<AIStimulus> GetActiveStimuli() const;
};

// Engine/Public/Navigation/NavMesh.h
class WINTERS_ENGINE CNavMesh
{
public:
    bool Build(const NavMeshBuildDesc& desc);
    bool FindPath(const Vec3& from, const Vec3& to, std::vector<Vec3>& outWaypoints) const;
    bool ProjectPoint(const Vec3& p, Vec3& outOnNav) const;
};

// Engine/Public/Navigation/CrowdManager.h
// ORCA / RVO
class WINTERS_ENGINE CCrowdManager
{
public:
    void RegisterAgent(EntityID id, const CrowdAgentDesc& desc);
    void Update(f32_t dt);
    Vec3 ResolveVelocity(EntityID id, const Vec3& preferred) const;
};
```

### 4.3 Bot AI 불변식 (재확인)

```text
[잘못된 BT Task]
class BTTask_Attack : IBTNode
    Execute:
        target.Health -= myDamage          ← 금지

[올바른 BT Task]
class BTTask_Attack : IBTNode
    Execute:
        ctx.PushCommand(GameCommand::CastAbility("BasicAttack", target));
        return InProgress;  // pendingExecCommands가 처리될 때까지 wait
```

Winters의 모든 BT Task가 이 패턴이어야 한다. `m_pendingExecCommands` → `CDefaultCommandExecutor`가 진실의 게이트.

### 4.4 S10 BotLaneAI → BehaviorTree 전환

현재:
```cpp
// BotLaneAISystem.cpp (의사)
void Execute(World& w) {
    for each bot:
        if (dead) ...
        else if (low hp) Retreat();
        else if (has enemy in range) Attack();
        else MoveLane();
}
```

Ch9 후:
```text
BotLaneBT.bt (data asset)
  Selector
    Sequence [Decorator: bDead]
      Task: WaitForRespawn
    Sequence [Decorator: HP < 30%]
      Task: FindHealLocation (EQS)
      Task: MoveTo(blackboard.HealLoc)
    Sequence [Decorator: HasTarget]
      Task: AttackTarget(blackboard.Target)
    Task: PatrolLane

Services:
  - UpdateNearestEnemy (every 0.2s)
  - UpdateHealth (every 0.1s)
```

Task는 모두 GameCommand 생성. 직접 mutate 0건.

### 4.5 단계별

```text
Ch9-Stage1  BT + Blackboard 기본 (composite + decorator + task)
Ch9-Stage2  BT event-driven re-search (decorator condition change → subtree restart)
Ch9-Stage3  BTService (백그라운드 blackboard 갱신)
Ch9-Stage4  S10 BotLaneAI를 BT로 마이그레이션
Ch9-Stage5  Perception (sight/hearing)
Ch9-Stage6  EQS (위치 탐색)
Ch9-Stage7  NavMesh (Recast/Detour) 통합 (Ch5 Physics 의존)
Ch9-Stage8  CrowdManager (ORCA)
Ch9-Stage9  LOD (먼 AI 단순화)
Ch9-Stage10 Utility AI / GOAP (게임 종류에 따라)
```

### 4.6 게임별 적용

| 게임 | 필요 Stage |
|------|-----------|
| LoL (현재 + S10 안정화) | Stage 1~4 |
| 로아 PvE 보스 | Stage 1~6 |
| 엘든링 | Stage 1~9 + 보스 패턴 BT 다층 |
| GTA6 시내 | Stage 1~10 + crowd LOD 강화 |

---

## 5. 검증 명령

```powershell
.\Server\Bin\Debug\WintersServer.exe --bot-debug --bt-trace

# 기대 로그
# [AI] entity#42 (Jax) BT loaded: BotLaneBT.bt (12 nodes)
# [AI] entity#42 BT tick: enter Sequence "ChaseEnemy", target=entity#88
# [AI] entity#42 BT task "AttackTarget" → GameCommand::CastAbility("BasicAttack", 88)
# [AI] entity#42 BT result Success, next decorator check
```

---

## 6. 다음 챕터로

Ch9 Stage 4까지 가야 S10 stage 1의 5000줄 BotLaneAI가 200줄짜리 BT data + 30개 Task로 압축. Ch11 Sequencer 보스 패턴은 BT subtree로 정의.
