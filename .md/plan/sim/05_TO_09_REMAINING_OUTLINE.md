# Phase Sim-5 ~ Sim-9 — Remaining Outline

**작성일**: 2026-04-29
**전제**: Sim-4 합격 후 본격 사이클별 박제 (각 사이클 진입 시점에 v1.1 스타일 1000+ 줄 .md 로 확장)
**목적**: 본 outline 은 5 phase 의 **인터페이스 + 검증 + 단계 마일스톤** 만 박제. 각 사이클의 본격 코드 전문은 진입 시점에 별도 .md 작성.

| Phase | 본격 .md (진입 시 작성) | 시간 | 의존 |
|-------|----------------------|------|------|
| Sim-5 | `.md/plan/sim/05_CLIENT_PREDICTION.md` | 240분 | Sim-4 |
| Sim-6 | `.md/plan/sim/06_BACKEND_SKIN_MATCH.md` | 180분 | Sim-5 |
| Sim-7 | `.md/plan/sim/07_BOT_AI.md` | 240분 | Sim-2 (병렬 가능) |
| Sim-8 | `.md/plan/sim/08_MCTS_PLANNER.md` | 360분 | Sim-1F + Sim-7 |
| Sim-9 | `.md/plan/sim/09_RL_ENV.md` | 480분 | Sim-7 + Sim-8 |

---

## ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

# Phase Sim-5 — Client Prediction + Reconciliation

**전제**: Sim-4 IOCP 합격 (서버 권위 시뮬 30Hz)
**목적**: 클라가 입력 즉시 반응하면서 서버 권위 결과로 자연스럽게 보정. 100ms RTT 환경에서 사용자 perceived latency = 0.

## 5.1 신규 인프라

| # | 인프라 | 헤더 | 구현 |
|---|--------|------|------|
| I-1 | `CClientInputBuffer` | `Client/Public/Network/ClientInputBuffer.h` | `Client/Private/Network/ClientInputBuffer.cpp` (Push/Drop/ForEachAfter) |
| I-2 | `CPredictionWorld` | `Client/Public/Network/PredictionWorld.h` | `Client/Private/Network/PredictionWorld.cpp` |
| I-3 | `CSnapshotApplier` | `Client/Public/Network/SnapshotApplier.h` (Sim-3 박제) | `Client/Private/Network/SnapshotApplier.cpp` (Sim-3 placeholder → 본격 entity 적용) |
| I-4 | `CRollbackEngine` | `Client/Public/Network/RollbackEngine.h` | `Client/Private/Network/RollbackEngine.cpp` |
| I-5 | `CRenderInterpolator` | `Client/Public/Network/RenderInterpolator.h` | `Client/Private/Network/RenderInterpolator.cpp` |

★ **의존성 includes 매트릭스**:
```cpp
// 공통 — 모든 Sim-5 헤더 (forward declare 우선)
#include "WintersTypes.h"
#include "ECS/Entity.h"
class CWorld;
class EntityIdMap;
class DeterministicRng;

// ClientInputBuffer.h — POD struct 위주
#include "Shared/GameSim/Systems/ICommandExecutor.h"  // GameCommandWire
#include <functional>

// SnapshotApplier.cpp — Sim-3 박제 본격화
#include "Shared/Schemas/Generated/cpp/Snapshot_generated.h"  // Snapshot, EntitySnapshot
#include "Shared/GameSim/EntityIdMap.h"
#include "Shared/GameSim/DeterministicRng.h"
#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/NetEntityIdComponent.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"

// RollbackEngine.cpp
#include "Shared/GameSim/Systems/ICommandExecutor.h"      // CDefaultCommandExecutor, BuildClientCommand
#include "Network/ClientInputBuffer.h"
#include "Network/SnapshotApplier.h"

// RenderInterpolator.cpp
#include "ECS/Components/TransformComponent.h"
```

## 5.2 ClientInputBuffer

```cpp
struct ClientInputBuffer {
    static constexpr u32_t kCapacity = 120;   // 4초 @ 30Hz
    GameCommandWire commands[kCapacity];
    u64_t commandTicks[kCapacity];
    u32_t head = 0;
    u32_t tail = 0;
    u32_t nextSeq = 1;

    u32_t Push(const GameCommandWire& cmd, u64_t clientTick);
    void Drop(u32_t ackedSeq);
    bool ForEachAfter(u32_t startSeq, std::function<void(u32_t, const GameCommandWire&, u64_t)> fn) const;
};
```

## 5.3 OnSnapshot 흐름 (★ 핵심)

```cpp
void CClientNetwork::OnSnapshot(const u8_t* bytes, u32_t len)
{
    auto* snap = Shared::Schema::GetSnapshot(bytes);

    // 1. NetEntityId → 로컬 EntityID 매핑
    for (const auto* es : *snap->entities()) {
        if (m_entityMap.FromNet(es->netId()) == NULL_ENTITY) {
            EntityID e = m_predictedWorld.CreateEntity();
            m_entityMap.Bind(es->netId(), e);
        }
    }

    // 2. 서버 권위 상태 적용
    m_pSnapshotApplier->Apply(snap, m_predictedWorld, m_entityMap, m_localRng);
    m_localRng.SetState(snap->rngState());   // ★ 결정성 동기화

    // 3. lastAckedSeq 이후 입력 모두 재실행 (★ Rollback)
    const u32_t ackedSeq = snap->lastAckedCommandSeq();
    EntityID self = m_entityMap.FromNet(snap->yourNetId());

    m_inputBuffer.ForEachAfter(ackedSeq,
        [&](u32_t seq, const GameCommandWire& wire, u64_t clientTick) {
            TickContext tc{ clientTick, kFixedDt, /* ... */ };
            GameCommand cmd = BuildClientCommand(wire, self, m_entityMap);
            m_pExecutor->ExecuteCommand(m_predictedWorld, tc, cmd);
        });

    // 4. 시각 보간 (Rollback 결과 → 부드럽게 lerp)
    m_pInterpolator->BeginInterpolation(m_predictedWorld);
}
```

## 5.4 RollbackEngine

```cpp
class CRollbackEngine
{
public:
    // Snapshot 적용 후 클라 예측 input 재실행 — 결정성 보장
    void Rebuild(u32_t lastAckedSeq, CWorld& world, const ClientInputBuffer& inputs,
                  EntityIdMap& map, DeterministicRng& rng);

    // 시각 jitter 측정 — pre/post hp diff
    f32_t GetMispredictAmount(EntityID self) const;
};
```

## 5.5 RenderInterpolator (시각 jitter 마스킹)

```cpp
class CRenderInterpolator
{
public:
    // World snap-to 대신 100ms lerp
    void BeginInterpolation(const CWorld& truth);
    void Tick(CWorld& visualWorld, f32_t dt);
};
```

## 5.6 Sim-5 6 단계

| Phase | 내용 | 시간 |
|-------|------|------|
| 5A | ClientInputBuffer + Push/Drop/ForEachAfter | 30분 |
| 5B | SnapshotApplier 본격 + EntityIdMap 갱신 | 45분 |
| 5C | RollbackEngine + 입력 재실행 | 45분 |
| 5D | RenderInterpolator + 시각 lerp | 30분 |
| 5E | 통합 OnSnapshot 흐름 + 회귀 | 60분 |
| 5F | 100ms / 250ms RTT 시뮬 + 검증 | 30분 |
| 합계 | | **240분** |

## 5.7 검증 마일스톤

| 검증 | 합격 |
|------|------|
| 100ms RTT | 캐스트 즉시 반응 (예측) + 200ms 후 권위 확인. visual jitter < 2 frame |
| 250ms RTT | rollback 자연 보정 (snap 안 보이게) |
| RNG state 동기화 | 클라 예측 / 서버 권위 결과 bit-equal |
| Mispredict 시각 | snap-to 가 아니라 lerp |
| 입력 큐 오버플로우 | 4초 (120 frame) 초과 시 reject + 재연결 |

---

## ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

# Phase Sim-6 — Backend Skin/Match

**전제**: Sim-5 합격
**목적**: 매치메이킹 → SkinId 소유권 검증 → IOCP 룸 lock-in → GameSessionConfig 전달. Go 백엔드 + IOCP 게임서버 통신.

## 6.1 신규 서비스

```
Services/
├── cmd/gamesession/main.go              ★ 신규 (Go entrypoint)
├── internal/gamesession/
│   ├── handler.go                       # HTTP REST + sessionId 발급
│   ├── service.go                       # LockIn / Skin 소유권 검증
│   └── lockin.go                        # 매치 lock-in + GameServer 전송
└── internal/gamesession/repo/           # DB layer
    ├── userskin_repo.go                 # UserSkin 소유권 조회
    └── session_repo.go                  # GameSessionConfig 영속화

migrations/                              # SQL 스키마
├── 008_user_skin.up.sql                 # ★ 신규 (skinId / unlockedAt)
└── 009_game_session.up.sql              # ★ 신규
```

★ **Go 모듈 의존성** (`Services/go.mod`):
```go
require (
    github.com/google/uuid v1.6.0           // SessionId
    github.com/jackc/pgx/v5 v5.5.0          // PostgreSQL
    github.com/redis/go-redis/v9 v9.4.0     // 매치메이킹 큐 / Skin cache
    github.com/segmentio/kafka-go v0.4.47   // MatchCompleted 이벤트
    github.com/google/flatbuffers v25.12.19 // ★ Sim-3 codegen 산출물 import
    google.golang.org/grpc v1.62.0          // (선택) GameServer 통신 — 1차는 HTTP
)

// internal/gamesession/repo import
import (
    schema "winters/services/shared/schemas/generated/go/Shared/Schema"  // ★ Sim-3
    "winters/services/pkg/database"
    "winters/services/pkg/cache"
)
```

★ **C++ 측 통신 의존**: `Server/Public/Network/BackendClient.h/.cpp` 신규 — WinHTTP 또는 libcurl 로 Go 백엔드 lock-in 결과 수신. Sim-6 진입 시 박제.

## 6.2 GameSessionConfig

```go
type GameSessionConfig struct {
    SessionId    uuid.UUID
    GameRoomId   uint32
    StartedAtMs  int64
    Players      []PlayerSlot
    InitialSeed  uint64    // ★ 결정성 — 양 서버/클라 동기 시드
}

type PlayerSlot struct {
    UserId       int64
    SessionId    uint32
    ChampionId   uint8
    SkinId       uint32     // ★ 소유 검증된 값
    Team         uint8
    RunePresetId uint32
    ItemPresetId uint32
}
```

## 6.3 Skin 소유권 검증

```go
func (s *Service) ValidateLockIn(req LockInRequest) (LockInResponse, error) {
    // 1. UserSkin DB 조회
    owns, err := s.repo.UserOwnsSkin(req.UserID, req.SkinID)
    if err != nil { return LockInResponse{}, err }

    // 2. 미소유 시 base skin fallback
    if !owns {
        base, _ := s.repo.GetBaseSkin(req.ChampionID)
        req.SkinID = base.SkinID
    }

    // 3. GameRoom 에 SessionConfig 전송 (gRPC 또는 직접 IOCP)
    cfg := s.buildConfig(req)
    err = s.gameServerClient.AssignRoom(cfg)
    return LockInResponse{ ServerHost: ..., ServerPort: ..., Token: ... }, err
}
```

## 6.4 IOCP 게임서버 ↔ Go 백엔드

옵션:
- (a) gRPC — Go ↔ C++ (grpcpp)
- (b) HTTP REST — Go → C++ (winhttp 서버 측)
- (c) Kafka — Match 이벤트 비동기

**1차 채택**: (b) — HTTP REST. Go 백엔드의 `/internal/gameserver/assign` POST 호출.

## 6.5 Sim-6 5 단계

| Phase | 내용 | 시간 |
|-------|------|------|
| 6A | GameSessionConfig DB 스키마 + migrations | 30분 |
| 6B | UserSkin 소유권 repo | 30분 |
| 6C | LockIn 흐름 + base skin fallback | 45분 |
| 6D | Go ↔ C++ 통신 (HTTP) | 45분 |
| 6E | 회귀 + 검증 | 30분 |
| 합계 | | **180분** |

## 6.6 검증 마일스톤

| 검증 | 합격 |
|------|------|
| 미소유 스킨 시도 | base skin fallback (anti-cheat 1차) |
| 매치 lock-in → 게임 시작 | < 5초 |
| InitialSeed 전달 | 클라/서버 양쪽 같은 RNG seed 시작 |
| 50 동접 매치 | 부하 테스트 통과 |

---

## ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

# Phase Sim-7 — Rule/Utility Bot (★ 병렬 가능)

**전제**: Sim-2 합격 (Sim-3 와 무관 — Command 만 알면 됨)
**목적**: AI 가 같은 ICommandExecutor entry 로 World 변경. RuleBot baseline → MCTS/RL 비교 기준.

## 7.1 신규 인프라

```
Client/Public/AI/Bot/                       Client/Private/AI/Bot/
├── IBotPolicy.h                            ├── (인터페이스 — cpp 없음)
├── BotState.h                              ├── (POD — cpp 없음)
├── BotPerception.h                         ├── BotPerception.cpp        # Build(world, self) 본문
├── RuleBotPolicy.h                         ├── RuleBotPolicy.cpp        # HFSM
├── UtilityBotPolicy.h                      ├── UtilityBotPolicy.cpp     # 점수 기반
├── GoapBotPolicy.h                         ├── GoapBotPolicy.cpp        # 목표 기반 (Sim-7 후반부)
└── BotManager.h                            └── BotManager.cpp           # 룸당 봇 슬롯 관리

Shared/GameSim/Bot/
└── BotCommandIssuer.h                      └── BotCommandIssuer.cpp    # Bot GameCommand 발행 (서버/AI Env 공통)
```

★ **의존성 includes 매트릭스**:
```cpp
// IBotPolicy.h — 인터페이스
#include "Shared/GameSim/Systems/ICommandExecutor.h"     // GameCommand, TickContext
#include "ECS/Entity.h"
class CWorld;
struct BotPerception;

// BotPerception.h — POD
#include "WintersTypes.h"
#include "WintersMath.h"                                 // Vec3
#include "ECS/Entity.h"
#include <vector>

// RuleBotPolicy.cpp — HFSM 본문
#include "AI/Bot/RuleBotPolicy.h"
#include "AI/Bot/BotPerception.h"
#include "ECS/World.h"
#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "ECS/Components/TransformComponent.h"

// UtilityBotPolicy.cpp — 점수 시스템
#include "AI/Bot/UtilityBotPolicy.h"
#include "AI/Bot/BotPerception.h"
#include "Shared/GameSim/Definitions/SkillDef.h"
#include "GamePlay/SkillRegistry.h"

// BotManager.cpp — 룸당 봇 슬롯
#include "AI/Bot/BotManager.h"
#include "AI/Bot/IBotPolicy.h"
#include "Shared/GameSim/Systems/ICommandExecutor.h"
#include <unordered_map>
#include <memory>
```

## 7.2 IBotPolicy

```cpp
class IBotPolicy {
public:
    virtual ~IBotPolicy() = default;
    virtual GameCommand Decide(const CWorld& world, const TickContext& tc,
                                 EntityID self, const BotPerception& perception) = 0;
};

struct BotPerception {
    struct EnemyView { EntityID e; f32_t hp; f32_t threat; f32_t distance; };
    struct AllyView  { EntityID e; f32_t hp; };
    struct ObjectiveView { EntityID e; f32_t priority; };

    std::vector<EnemyView> enemies;
    std::vector<AllyView>  allies;
    std::vector<ObjectiveView> objectives;   // 미니언/타워/넥서스
    Vec3 selfPos;
    f32_t selfHp, selfMana;
};
```

## 7.3 RuleBotPolicy (HFSM)

```cpp
enum class eBotState : u8_t {
    Idle, Lane, Engage, Retreat, Recall, Dead
};

class CRuleBotPolicy final : public IBotPolicy
{
public:
    GameCommand Decide(...) override;

private:
    eBotState m_state = eBotState::Idle;
    f32_t m_stateTime = 0.f;

    GameCommand DecideLane(const BotPerception&);
    GameCommand DecideEngage(const BotPerception&);
    GameCommand DecideRetreat(const BotPerception&);
    eBotState ChooseNextState(const BotPerception&);
};
```

## 7.4 UtilityBotPolicy

```cpp
class CUtilityBotPolicy final : public IBotPolicy
{
public:
    GameCommand Decide(...) override;

private:
    struct ActionScore {
        GameCommand cmd;
        f32_t score;
    };

    f32_t ScoreKill(const BotPerception&, EntityID target);
    f32_t ScorePush(const BotPerception&);
    f32_t ScoreObjective(const BotPerception&);
    f32_t ScoreSafety(const BotPerception&);

    std::vector<ActionScore> EnumerateActions(const BotPerception&);
};
```

## 7.5 BotManager (★ Server-side / AI Env 양쪽 공통)

```cpp
class CBotManager
{
public:
    static std::unique_ptr<CBotManager> Create();

    void AddBot(EntityID e, std::unique_ptr<IBotPolicy> policy);
    void RemoveBot(EntityID e);

    // Tick 안에서 호출 — Bot 의 Command 를 ICommandExecutor 로 송출
    void Decide(CWorld& world, const TickContext& tc,
                  std::vector<GameCommand>& outCommands);

private:
    std::unordered_map<EntityID, std::unique_ptr<IBotPolicy>> m_bots;
};
```

## 7.6 Sim-7 6 단계

| Phase | 내용 | 시간 |
|-------|------|------|
| 7A | IBotPolicy + BotPerception + BotManager | 30분 |
| 7B | RuleBotPolicy HFSM (Idle/Lane/Engage/Retreat) | 60분 |
| 7C | UtilityBotPolicy 점수 시스템 | 60분 |
| 7D | BotCommandIssuer + 서버 통합 | 30분 |
| 7E | 1v1 시뮬 (BotMid vs Player) | 30분 |
| 7F | 5v5 BotTeam vs PlayerTeam + 회귀 | 30분 |
| 합계 | | **240분** |

## 7.7 검증 마일스톤

| 검증 | 합격 |
|------|------|
| 1v1 BotMid vs Player | Bot 라인 유지, 5분 cs 차이 < 30 |
| 5v5 Bot vs Player | 게임 진행 가능, 30분 안에 결판 |
| Bot Command 결정성 | 같은 World + seed → 같은 Command 시퀀스 |
| Bot 가 World 직접 변경 X | grep `world.GetComponent<...>(.*).hp =` Bot/ = 0 hit |
| Tick 부하 | 5 봇 1 룸 CPU 추가 < 5% |

---

## ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

# Phase Sim-8 — MCTS Planner (★ 결정성 전제)

**전제**: Sim-1F 결정성 grep 12종 통과 + Sim-7 RuleBot baseline
**목적**: Tactical planner — 1 tick 안에 N rollout 으로 미래 평가. RuleBot 보다 명백히 우세 입증.

## 8.1 신규 인프라

```
Client/Public/AI/MCTS/                      Client/Private/AI/MCTS/
├── WorldCloner.h                           ├── WorldCloner.cpp          # ECS Component memcpy + EntityIdMap + RNG
├── MCTSNode.h                              ├── (POD — cpp 없음)
├── MCTSPlanner.h                           ├── MCTSPlanner.cpp           # Selection/Expansion/Rollout/Backprop
├── ActionEnumerator.h                      ├── ActionEnumerator.cpp     # 합법 Command 후보
├── Rollout.h                               ├── Rollout.cpp              # RuleBot 또는 random
├── Evaluator.h                             └── Evaluators/
└── Evaluators/HpDiffEvaluator.h                ├── HpDiffEvaluator.cpp
   GoldDiffEvaluator.h                          ├── GoldDiffEvaluator.cpp
   CompositeEvaluator.h                         └── CompositeEvaluator.cpp
```

★ **의존성 includes 매트릭스**:
```cpp
// WorldCloner.h
#include "WintersTypes.h"
#include <memory>
class CWorld;
class EntityIdMap;
class DeterministicRng;

// WorldCloner.cpp
#include "AI/MCTS/WorldCloner.h"
#include "ECS/World.h"
#include "Shared/GameSim/EntityIdMap.h"
#include "Shared/GameSim/DeterministicRng.h"
#include "Shared/GameSim/Components/StatComponent.h"      // ★ trivially_copyable static_assert 보장
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/BuffComponent.h"
#include "Shared/GameSim/Components/PendingHitComponent.h"
#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/NetEntityIdComponent.h"
#include "ECS/Components/TransformComponent.h"

// MCTSPlanner.cpp
#include "AI/MCTS/MCTSPlanner.h"
#include "AI/MCTS/WorldCloner.h"
#include "AI/MCTS/MCTSNode.h"
#include "AI/MCTS/ActionEnumerator.h"
#include "AI/MCTS/Rollout.h"
#include "AI/MCTS/Evaluator.h"
#include "AI/Bot/RuleBotPolicy.h"            // ★ Rollout policy
#include "Shared/GameSim/Systems/ICommandExecutor.h"
#include "Shared/GameSim/DeterministicRng.h"
#include <chrono>                              // ★ time budget 만 외부 시간 OK (sim 안 X)

// Evaluators
#include "AI/MCTS/Evaluator.h"
#include "Shared/GameSim/Components/HealthComponent.h"   // HpDiff
#include "Shared/GameSim/Components/ChampionComponent.h" // gold (★ Sim-2 ChampionComponent 확장 필요)
```

## 8.2 WorldCloner (★ 결정성 1차 통합 시점)

```cpp
class CWorldCloner
{
public:
    static std::unique_ptr<CWorld> Clone(const CWorld& source,
                                          const EntityIdMap& sourceMap,
                                          EntityIdMap& outClonedMap);

    // 보존:
    //   - 모든 ECS Component (memcpy 가능 — Sim-2 trivially_copyable static_assert)
    //   - EntityID 자체 (process local OK)
    //   - EntityIdMap (NetEntityId ↔ EntityID)
    //   - RNG state
    //   - System 실행 DAG (별도 — 시스템 인스턴스 자체는 stateless)
};
```

## 8.3 MCTSNode

```cpp
struct MCTSNode
{
    GameCommand action;
    u32_t visitCount = 0;
    f32_t totalValue = 0.f;
    std::vector<std::unique_ptr<MCTSNode>> children;
    MCTSNode* parent = nullptr;

    f32_t UCB1(f32_t parentVisits, f32_t explorationConstant) const;
};
```

## 8.4 MCTSPlanner 4 단계

```cpp
class CMCTSPlanner
{
public:
    GameCommand Plan(const CWorld& root, const EntityIdMap& rootMap,
                       const TickContext& tc, EntityID self,
                       u32_t numSimulations, f32_t timeBudgetMs);

private:
    // 1. Selection — 루트에서 leaf 까지 UCB1
    MCTSNode* Select(MCTSNode* root);

    // 2. Expansion — leaf 의 합법 action 1개 추가
    MCTSNode* Expand(MCTSNode* leaf, const CWorld& state, EntityID self);

    // 3. Rollout — leaf 부터 N tick RuleBot 또는 random
    f32_t Rollout(const CWorld& state, const EntityIdMap& map,
                    EntityID self, u32_t depth);

    // 4. Backpropagation — value 를 root 까지 전파
    void Backprop(MCTSNode* node, f32_t value);

    std::unique_ptr<IEvaluator> m_pEvaluator;
    std::unique_ptr<IBotPolicy> m_pRolloutPolicy;   // Rollout 시 RuleBot 사용
};
```

## 8.5 Evaluator

```cpp
class IEvaluator {
public:
    virtual ~IEvaluator() = default;
    virtual f32_t Evaluate(const CWorld& world, EntityID self) = 0;
};

class CHpDiffEvaluator final : public IEvaluator {
    f32_t Evaluate(...) override;   // sum(ally.hp%) - sum(enemy.hp%)
};

class CGoldDiffEvaluator final : public IEvaluator {
    f32_t Evaluate(...) override;
};

class CCompositeEvaluator final : public IEvaluator {
    f32_t Evaluate(...) override;   // weighted: 0.4 hp + 0.3 gold + 0.2 obj + 0.1 pos
};
```

## 8.6 Sim-8 7 단계

| Phase | 내용 | 시간 |
|-------|------|------|
| 8A | WorldCloner + 결정성 검증 (clone 후 같은 input → 같은 결과) | 60분 |
| 8B | MCTSNode + UCB1 | 30분 |
| 8C | ActionEnumerator (합법 Command 후보) | 60분 |
| 8D | Rollout (RuleBot policy + N tick) | 45분 |
| 8E | MCTSPlanner 4 단계 통합 | 60분 |
| 8F | Evaluator 3종 + Composite | 45분 |
| 8G | 1v1 vs RuleBot 비교 + 회귀 | 60분 |
| 합계 | | **360분** |

## 8.7 검증 마일스톤

| 검증 | 합격 |
|------|------|
| 100ms / 1000 sims | RuleBot 보다 우세 (kill +20%) |
| 결정성 | 같은 root + seed → 같은 plan (10회 반복 bit-equal) |
| Sim-1F 결정성 grep 통과 | 통과 (미통과 시 진입 X) |
| WorldCloner 메모리 | clone 1회 < 5ms (10 entity 기준) |
| Rollout 깊이 | 30 tick (1초 sim) 권장 |

---

## ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

# Phase Sim-9 — RL Env + 학습 + Inference

**전제**: Sim-7 RuleBot baseline + Sim-8 MCTS advanced policy
**목적**: OpenAI Gym 미러 환경 박제 → PPO/SAC 학습 → ONNX inference 통합. Production fallback = RuleBot.

## 9.1 신규 인프라

```
Client/Public/AI/RL/                        Client/Private/AI/RL/
├── GymEnv.h                                ├── GymEnv.cpp               # Reset/Step/Render 본문
├── Observation.h                           ├── ObservationExtractor.cpp # 256 features 추출
├── ActionMask.h                            ├── ActionMask.cpp           # legal action 비트마스크
├── Reward.h                                ├── RewardCalculator.cpp     # 보상 함수
├── ReplayBuffer.h                          ├── ReplayBuffer.cpp         # 10M transition
├── ONNXRuntime.h                           ├── ONNXRuntime.cpp          # ONNX inference
└── RLPolicy.h                              └── RLPolicy.cpp             # IBotPolicy 구현 + fallback

Tools/RL/                                   # Python 트레이너 (C++ binding)
├── train_ppo.py
├── train_sac.py
├── eval.py
├── export_onnx.py
└── winters_gym_env.cpp                     # ★ pybind11 binding
```

★ **의존성 includes 매트릭스**:
```cpp
// GymEnv.h
#include "WintersTypes.h"
#include "ECS/Entity.h"
#include "AI/RL/Observation.h"
#include "AI/RL/Reward.h"
#include <memory>
class CWorld;
class EntityIdMap;
class DeterministicRng;

// GymEnv.cpp
#include "AI/RL/GymEnv.h"
#include "AI/MCTS/WorldCloner.h"               // ★ Reset 시 World 초기 상태 복제
#include "ECS/World.h"
#include "Shared/GameSim/EntityIdMap.h"
#include "Shared/GameSim/DeterministicRng.h"
#include "Shared/GameSim/Systems/ICommandExecutor.h"
#include "AI/Bot/BotPerception.h"

// ObservationExtractor.cpp
#include "AI/RL/Observation.h"
#include "ECS/World.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/BuffComponent.h"
#include "ECS/Components/TransformComponent.h"

// RewardCalculator.cpp
#include "AI/RL/Reward.h"
#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"

// ONNXRuntime.cpp — 외부 의존
#include "AI/RL/ONNXRuntime.h"
#include <onnxruntime_cxx_api.h>             // ★ ThirdPartyLib/ONNX/

// RLPolicy.cpp
#include "AI/RL/RLPolicy.h"
#include "AI/RL/ONNXRuntime.h"
#include "AI/RL/Observation.h"
#include "AI/Bot/RuleBotPolicy.h"            // ★ Production fallback
```

★ **외부 의존**: `Engine/ThirdPartyLib/ONNX/` 신규 + pybind11 (Python binding) 추가. Sim-9 진입 시 ThirdParty 박제.

## 9.2 GymEnv interface

```cpp
struct Observation {
    static constexpr u32_t kFeatureDim = 256;
    f32_t features[kFeatureDim];
    u32_t actionMask;          // 32 bit — 합법 action only
};

struct Action {
    u8_t kind;       // eCommandKind
    u8_t slot;       // 0..4
    f32_t aimX, aimZ;
    u8_t targetIdx;  // -1 if direction
};

struct StepResult {
    Observation nextObs;
    f32_t reward;
    bool bDone;
    bool bTruncated;   // time limit
};

class CGymEnv
{
public:
    Observation Reset(u64_t seed);
    StepResult Step(const Action& a);
    void Render();   // 옵션

    u32_t ActionSpaceSize() const;
    u32_t ObservationSpaceSize() const;

private:
    std::unique_ptr<CWorld> m_world;
    DeterministicRng m_rng;
    EntityIdMap m_entityMap;
    EntityID m_agentEntity;
    u64_t m_currentTick;
};
```

## 9.3 Observation feature extraction

```cpp
class CObservationExtractor
{
public:
    Observation Extract(const CWorld& world, EntityID self);

    // 256 features:
    //   - self stats (16): hp%, mana%, level, ad, ap, armor, mr, ...
    //   - skill states (10): cd[5] + rank[5]
    //   - 3x ally features (30): hp%, distance, threat
    //   - 4x enemy features (40): hp%, distance, threat, last_seen_ms
    //   - 5x objective (25): gold value, distance, hp%
    //   - map context (50): own/enemy tower hp, dragon timer, baron timer, ...
    //   - 시간 (5): game time, respawn time, ...
    //   - 패딩 (80)
};
```

## 9.4 Reward 함수

```cpp
class CRewardCalculator
{
public:
    f32_t Calculate(const CWorld& prev, const CWorld& curr, EntityID self) const;

    // 합산:
    //   + kill: +1.0
    //   + assist: +0.5
    //   + minion last hit: +0.05
    //   + tower destroy (참여): +1.0
    //   + objective (drake/baron): +2.0
    //   - death: -1.5
    //   - per tick (게임 시간 압박): -0.001
};
```

## 9.5 ONNX Runtime inference

```cpp
class CRLPolicy final : public IBotPolicy
{
public:
    static std::unique_ptr<CRLPolicy> CreateFromOnnx(const wchar_t* modelPath);

    GameCommand Decide(const CWorld& world, const TickContext& tc,
                        EntityID self, const BotPerception& perception) override;

private:
    Ort::Session m_session;
    std::unique_ptr<CObservationExtractor> m_pExtractor;

    // ★ Production fallback
    std::unique_ptr<CRuleBotPolicy> m_pFallback;
    bool m_bInferenceFailed = false;
};
```

## 9.6 학습 파이프라인 (Python)

```python
# Tools/RL/train_ppo.py
import gym
from stable_baselines3 import PPO
from winters_gym_env import WintersGymEnv   # C++ binding

env = WintersGymEnv()
model = PPO("MlpPolicy", env, verbose=1, tensorboard_log="./logs/")
model.learn(total_timesteps=1_000_000)
model.save("ppo_winters_v1")

# 평가
from stable_baselines3.common.evaluation import evaluate_policy
mean_reward, std_reward = evaluate_policy(model, env, n_eval_episodes=100)
print(f"mean={mean_reward}, std={std_reward}")

# ONNX export
import torch
torch.onnx.export(model.policy.mlp_extractor.policy_net, ...)
```

## 9.7 Sim-9 9 단계

| Phase | 내용 | 시간 |
|-------|------|------|
| 9A | GymEnv interface + Reset/Step | 60분 |
| 9B | ObservationExtractor (256 features) | 60분 |
| 9C | ActionMask + Reward | 45분 |
| 9D | ReplayBuffer + 10M transition | 30분 |
| 9E | C++ ↔ Python binding (pybind11) | 60분 |
| 9F | Python PPO 트레이너 (Tools/RL/train_ppo.py) | 60분 |
| 9G | ONNX export + Runtime inference | 60분 |
| 9H | RLPolicy + Production fallback | 45분 |
| 9I | 학습 + 평가 + 회귀 | 60분 |
| 합계 | | **480분 (8h, 학습 시간 별도)** |

## 9.8 검증 마일스톤

| 검증 | 합격 |
|------|------|
| 1M step 학습 후 vs RuleBot | winrate > 60% |
| Inference latency | < 5ms / step (one entity) |
| Production fallback | RL inference fail 시 RuleBot 자동 swap (로그 + 메트릭) |
| 결정성 (학습 환경) | 같은 seed → 같은 trajectory 재현 |
| ONNX 호환 | Windows + Linux 동일 모델 |
| 메모리 | 모델 < 50MB |

---

## ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

# 종합 — Phase Sim 전체 의존 그래프

```
v3.1 Ezreal F5 #1 합격
        │
        ▼
   Sim-1A (폴더 + Determinism)
   Sim-1B (Wrapper)
   Sim-1C (GameplayHookRegistry)
   Sim-1D (Hook 2 분리)
   Sim-1E (PendingHit 일반화)
   Sim-1F (결정성 grep 12종 ★ MCTS 전제)
        │
        ▼
   Sim-2 (5 축 공통 — ID 기반 + trivially_copyable)
        │
   ┌────┼─────────┐
   ▼    ▼         ▼
Sim-3 Sim-7     ↓
(Schema) (Bot)
   │    │       
   ▼    │       
Sim-4   │       
(IOCP)  │       
   │    │       
   ▼    │       
Sim-5   │       
(Pred)  │       
   │    │       
   ▼    │       
Sim-6   │       
(Backend)│      
        │       
   ┌────┘       
   ▼            
Sim-8           
(MCTS)          
   │            
   ▼            
Sim-9           
(RL)            
   │            
   ▼            
Production = RL+MCTS hybrid + RuleBot fallback
```

## 본격 .md 박제 약속

각 사이클 진입 시점에 본 outline 의 해당 섹션을 v1.1 마스터 플랜 스타일로 1000+ 줄 박제:
- Sim-5 진입 → `.md/plan/sim/05_CLIENT_PREDICTION.md`
- Sim-6 진입 → `.md/plan/sim/06_BACKEND_SKIN_MATCH.md`
- Sim-7 진입 → `.md/plan/sim/07_BOT_AI.md`
- Sim-8 진입 → `.md/plan/sim/08_MCTS_PLANNER.md`
- Sim-9 진입 → `.md/plan/sim/09_RL_ENV.md`

---

## 한 줄

**Sim-5~9 = 클라 예측 / 백엔드 스킨 / Bot baseline / MCTS planner / RL Env 단계적 박제. 본 outline = 인터페이스 + 검증 + 단계 마일스톤 박제. 본격 코드 전문은 진입 시점에 v1.1 스타일 1000+ 줄 .md 작성. 합계 1500 분 (≈25h, RL 학습 시간 별도). Sim-1F 결정성 grep 통과가 Sim-8/9 진입 1차 전제 — 결정성 미통과 시 MCTS rollout 평가 무의미.**
