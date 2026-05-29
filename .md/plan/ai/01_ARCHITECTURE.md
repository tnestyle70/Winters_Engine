# AI 아키텍처 — ECS 통합 + 디렉토리

## ECS 컴포넌트 설계

봇은 ECS Entity + 여러 컴포넌트 조합.

```cpp
// 모든 봇 엔티티가 가짐
struct BotComponent {
    u32_t      botID;
    f32_t      difficulty;       // 0.0 (Intro) ~ 1.0 (Grandmaster)
    u32_t      role;             // Top/Jungle/Mid/ADC/Support
    u32_t      blackboardID;     // 팀 Blackboard 참조
    u32_t      championID;       // Irelia/Yasuo/Sylas/Viego/Kalista
    bool_t     bEnabled;         // 비활성화 가능 (연습모드에서 토글)
};

// Stage 1 HFSM
struct FSMComponent {
    u32_t      rootState;        // Laning/Ganking/... enum
    u32_t      subState;
    f32_t      stateElapsed;     // 현재 상태 경과 시간
};

// Stage 2 BT
struct BehaviorTreeComponent {
    u32_t      treeAssetID;      // 로드된 .bt 에셋 ID
    u32_t      currentNodeIdx;   // 현재 실행 중인 노드
    std::vector<u32_t> stackFrames;  // Parallel/Decorator 스택
};

// Stage 3 GOAP
struct GOAPComponent {
    u32_t      currentGoal;
    std::vector<u32_t> plan;     // 현재 액션 시퀀스
    i32_t      planStep;         // 현재 실행 중인 액션 인덱스
    f32_t      lastReplanTime;
};

// Stage 4 Utility
struct DecisionComponent {
    struct Candidate { u32_t actionID; f32_t score; };
    std::vector<Candidate> candidates;
    u32_t chosenAction;
};

// Stage 5 맵 인식
struct MapAwarenessComponent {
    f32_t      threatAtPosition;
    f32_t      opportunityAtPosition;
    Vec2       safestRetreatCell;
    Vec2       bestAttackCell;
};

// Stage 6 MCTS (교전 시)
struct MCTSComponent {
    bool_t     bActive;
    u32_t      rootNodeIdx;      // 트리 풀 내 인덱스
    u32_t      simulationsPerFrame;
};

// Stage 7/8 ML
struct NeuralPolicyComponent {
    u32_t      modelID;          // ONNX 모델 ID
    f32_t      lastInferenceTime;
    std::vector<f32_t> lastAction; // 확률 벡터
};
```

## 시스템 (ISystem 파생)

실행 순서 (SystemScheduler Phase):

```cpp
// PreUpdate Phase — 맵/팀 상태 갱신
1. InfluenceMapSystem        (Stage 5, 100ms 간격)
2. BlackboardSystem          (팀 공유 메모리 갱신)
3. ThreatAssessmentSystem    (Stage 5)

// Update Phase — 봇 의사결정
4. StrategicDecisionSystem   (Stage 3/4, 2~5초 간격 per bot)
5. TacticalDecisionSystem    (Stage 1/2, 0.2~0.5초 간격 per bot)
6. OperationalSystem         (매 프레임 — 스킬샷/평타)

// PostUpdate Phase — 명령 실행
7. BotActionSystem           (의사결정 결과를 입력 이벤트로 변환)
8. PathfindingSystem         (이동 경로 갱신)
```

시스템별 난이도에 따라 on/off — Intro 봇은 InfluenceMapSystem/MCTS 비활성.

## 디렉토리 구조

```
Engine/Public/AI/
├── Core/
│   ├── IBotAI.h              모든 봇 의사결정 인터페이스
│   ├── BotTypes.h            enum (Role/Difficulty/Goal/Action)
│   └── AIConfig.h            난이도 프리셋 설정
├── FSM/
│   ├── HFSM.h                Hierarchical FSM 엔진
│   ├── FSMState.h            State 베이스 클래스
│   └── States/               Laning/Ganking/... 구체 상태
├── BehaviorTree/
│   ├── BTNode.h              Selector/Sequence/Parallel/Decorator
│   ├── BTInterpreter.h       런타임 실행기
│   ├── BTLoader.h            .bt 파일 파서
│   └── Nodes/                Common nodes (IsInRange, HasMana, CastQ 등)
├── GOAP/
│   ├── WorldState.h          key-value 상태 벡터
│   ├── Action.h              precondition/effect/cost
│   ├── Goal.h                목표 상태
│   └── Planner.h             A* 기반 플래너
├── Utility/
│   ├── ScoreCalculator.h     가중 점수 합산
│   ├── ResponseCurve.h       Linear/Quadratic/Sigmoid/Logistic
│   └── DecisionMaker.h       최고점 선택
├── Blackboard/
│   ├── Blackboard.h          team-shared key-value
│   └── BlackboardSystem.h
├── InfluenceMap/
│   ├── GridMap.h             2D 그리드 베이스
│   ├── TeamInfluence.h       아군/적군 영향력
│   ├── ThreatMap.h           적 위협도
│   ├── OpportunityMap.h      기회 (오브젝트/갱)
│   ├── VisionMap.h           와드/안개
│   └── MapFusion.h           레이어 합성 → 최종 점수
├── Pathfinding/
│   ├── AStar.h               격자/네비메시 A*
│   ├── JumpPointSearch.h     JPS 최적화
│   └── PathSmoothing.h       경로 smoothing (funnel)
├── MCTS/
│   ├── MCTSNode.h            UCB1, 방문수, 평균값
│   ├── MCTSTree.h            노드 풀
│   ├── Rollout.h             간이 시뮬레이터
│   └── Simulator.h           교전 상태 전이 근사
├── Imitation/
│   ├── LogCollector.h        실제 플레이어 로그 저장
│   ├── FeatureExtractor.h    상태 → 벡터
│   ├── ONNXRuntime.h         모델 추론 래퍼
│   └── BehaviorCloner.h
├── RL/
│   ├── IBotEnv.h             state/step/reset 인터페이스
│   ├── PPOAgent.h            추론만 (학습은 파이썬)
│   └── SelfPlay.h            여러 봇 병렬 매칭 관리
├── Bots/                      챔피언별 봇 프리셋
│   ├── BotProfile_Irelia.h
│   ├── BotProfile_Yasuo.h
│   ├── BotProfile_Sylas.h
│   ├── BotProfile_Viego.h
│   └── BotProfile_Kalista.h
└── Systems/
    ├── BotSystem.h
    ├── FSMSystem.h
    ├── BTSystem.h
    ├── GOAPSystem.h
    ├── UtilitySystem.h
    ├── InfluenceMapSystem.h
    ├── MCTSSystem.h
    └── NeuralPolicySystem.h
```

## Client 사이드

```
Client/Public/AI/
├── BotProfile.h              챔피언별 봇 생성 API
└── BotDebugger.h             ImGui 디버거

Client/Private/AI/
└── (동일)
```

## 챔피언별 봇 프리셋 패턴

```cpp
// Engine/Public/AI/Bots/BotProfile_Irelia.h
class CBotProfile_Irelia
{
public:
    // 이렐리아 봇 생성 — 모든 Stage 컴포넌트 셋업
    static EntityID CreateBot(CWorld& world, f32_t difficulty, u32_t role);

    // 이렐리아 전용 BT 에셋 경로
    static constexpr const char* BT_PATH = "AI/BT/Irelia.bt";

    // 이렐리아 전용 Utility 함수들 (Q 대상 선정, R 타이밍 등)
    static f32_t ScoreQTarget(EntityID self, EntityID target);
    static bool_t ShouldUseUltimate(EntityID self, const BattleContext& ctx);
};
```

동일 패턴을 Yasuo/Sylas/Viego/Kalista 에 적용.

## 외부 의존

- **ONNX Runtime** (Stage 7/8 선택) — Microsoft/ONNX Runtime, C++ API
- **Lua 5.4** (Stage 2 BT 조건/액션 노드 스크립팅 고려) — 이미 CLAUDE.md 기술 스택에 포함
- 그 외 외부 AI 라이브러리 미사용

## 서버/클라이언트 경계

- **봇 AI 는 서버에서 실행** — 부정행위 방지 + 서버 권위
- **오프라인 봇전 모드**: Client 내 "Mini Server" 쓰레드가 AI 실행
- Client 는 봇 상태를 시각화만 함 (디버거 제외)
