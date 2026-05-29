# Phase Sim — SharedGameSim Master Plan

**작성일**: 2026-04-29
**v1.1 갱신**: 2026-04-29 — Codex 4 보정 박제 (Hook 2 분리 / NetEntityId / 결정성 강화 / Wrapper 점진 / ID 기반)
**전제**: B-11d v3.1 Ezreal F5 #1 합격 (Registry 3종 + hookId 4분할 + ApplyDamage 공통 + eChampion 명시값)
**목적**: 150 챔프 + IOCP 권위 시뮬 + MCTS/RL AI 가 **단일 시뮬레이션 코어** 위에서 돌도록 SharedGameSim 경계 + Hook 2 분리 + 5 축 공통 + Schema 직렬화 + 결정성 박제

---

## 0. v1.0 → v1.1 변경 요약 (Codex 4 + 1 보정)

| # | 분류 | v1.0 (위험) | v1.1 (정정) |
|---|------|------------|------------|
| **C-1** | 블로커 | "hook 함수 자체는 Client 에 남고..." → 서버가 hook 못 돌림 = 권위 시뮬 불가 | **Hook 2 분리** — `Shared/GameSim/Champions/<Name>_Gameplay.cpp` (위치/투사체/Damage/Buff/쿨다운) + `Client/.../<Name>_Visual.cpp` (anim/FX/sound/camera/UI) |
| **C-2** | 블로커 | EntityID 직접 네트워크 송신 + Command 의 issuerEntity 클라 신뢰 | **NetEntityId vs EntityID 분리** — runtime EntityID (process local) ≠ network NetEntityId (server-issued). Issuer = sessionId → controlledEntity (서버 결정, 클라 spoofing 차단) |
| **C-3** | 블로커 | `unordered_map` 순회 결과로 시뮬 결정 → 플랫폼별 결과 갈라짐 | **결정성 12종 grep 강제** — sorted EntityID iterator / RNG 소비 순서 (tickIndex+sourceId+skillId) / EntityID 생성 순서 / System 실행 DAG / Render·Audio 가 sim state 변경 X |
| **C-4** | 설계 | 헤더 물리 이동 → include 30+ 곳 동시 깨짐 | **Wrapper 점진 이전** — Shared 정본 생성 + 기존 Client 헤더는 1줄 `#include "Shared/..."` wrapper. 사용처 천천히 변경 후 wrapper 폐기 |
| **C-5** | 설계 | `ChampionStatsDef* pBaseDef` 등 pointer 저장 → 직렬화 불가 | **ID 기반 lookup** — Component 안 `eChampion championId` / `u16_t skillId` / `u16_t scalingTableId` 만. 매 tick `Registry::Find` 로 데이터 조회. snapshot/replay/AI 호환 |
| **+1** | 추가 | World Clone 보다 **결정성 박제가 먼저** | Sim-1F 결정성 검증 통과 안 하면 Sim-8 MCTS 진입 X (Codex 답변에 묵시적, 본 v1.1 명시 박제) |

---

## 1. 5 축 공통 시스템 — 절대 hook 안에 넣지 말 것

| # | 축 | 책임 | 잘못된 곳 | 올바른 곳 |
|---|----|------|----------|----------|
| 1 | **DamagePipeline** | 물리/마법/고정 + AD-AP 계수 + 방관 마관 + 치명타 + 온힛 + 보호막 | 챔프 hook | `Shared/GameSim/Systems/DamagePipeline.cpp` |
| 2 | **StatSystem** | 레벨 + 아이템 + 버프 → 최종 스탯 | hook 안 hp += 직접 | `Shared/GameSim/Systems/StatSystem.cpp` |
| 3 | **SkillRankSystem** | 스킬 랭크별 데미지/쿨다운/마나/계수 테이블 | `SkillDef` 단일 값 | `SkillScalingTable[ranks=5]` + `SkillScalingRegistry` |
| 4 | **BuffModifierSystem** | 증감/상태이상/온힛/쉴드 | 챔프별 hard-coded | `BuffComponent` + `BuffSystem` |
| 5 | **SkinRegistry** | 외형/FX/애니 override (게임플레이 X) | `ChampionDef` 분기 | `Shared/GameSim/Definitions/SkinDef.h` |

★ **원칙**: hook 은 `DamageRequest` **만 만들고**, 계산은 `DamagePipeline` 이. 챔프별 코드 = 의도 표현, 공식 = 공통 시스템.

---

## 2. Phase 인덱스 (★ Sim-1 6 단계로 세분화)

| Phase | 내용 | 시간 | 출력 | 진입 전제 |
|-------|------|------|------|----------|
| **Sim-1A** | SharedGameSim 폴더 + 결정성 유틸 (DeterministicTime/Rng/EntityIdMap) | 30분 | `Shared/GameSim/` 골격 | v3.1 F5 #1 합격 |
| **Sim-1B** | SkillDef/ChampionDef/Damage/ProjectileKind **wrapper** 도입 | 30분 | Shared 정본 + Client 1줄 wrapper | Sim-1A |
| **Sim-1C** | `CGameplayHookRegistry` (Shared) 분리 + raw fn ptr 박제 | 30분 | Hook 인프라 | Sim-1B |
| **Sim-1D** | `CVisualHookRegistry` (Client) + Ezreal/Yasuo/Riven hook 2 분리 | 60분 | `_Gameplay.cpp` + `_Visual.cpp` | Sim-1C |
| **Sim-1E** | PendingHitSystem 일반화 — Yasuo 시각 의존 끊기 (B-4 정식) | 30분 | Generic ProjectileSystem | Sim-1D |
| **Sim-1F** | 빌드 + Ezreal/Yasuo/Riven 회귀 + 결정성 grep 12종 | 30분 | gotcha 박제 + 마일스톤 합격 | Sim-1E |
| **Sim-1 합계** | | **210분** | | |
| **Sim-2** | 5 축 공통 (Stat/Damage/SkillRank/Buff/Skin) — ID 기반 (C-5) | 360분 | 5 축 컴포넌트 + 시스템 | Sim-1F |
| **Sim-3** | Schema (Command sessionId 기반 / Snapshot / Event) FlatBuffers | 120분 | `.fbs` + 코드젠 | Sim-2 |
| **Sim-4** | IOCP GameRoom 30Hz tick + sessionId→entity 매핑 (C-2) | 480분 | Server/Game/ | Sim-3 |
| **Sim-5** | Client Prediction + Reconciliation (NetEntityId 추적) | 240분 | input buffer + rollback | Sim-4 |
| **Sim-6** | Backend Skin/Match (Go) + GameSessionConfig | 180분 | Go 서비스 + skin DB | Sim-5 |
| **Sim-7** | Rule/Utility Bot (병렬 가능) | 240분 | `AI/Bot/` | Sim-2 (Sim-3 무관) |
| **Sim-8** | MCTS SimClone + Evaluator (★ 결정성 전제) | 360분 | `AI/MCTS/` | Sim-1F + Sim-7 |
| **Sim-9** | RL Env + Telemetry + 학습 + ONNX inference | 480분 | `AI/RL/` | Sim-7 baseline + Sim-8 |
| **합계** | | **~2670분 (≈45h)** | | |

★ **병렬 가능**: Sim-7 은 Sim-3 무관 (Command 만 알면 됨) → Sim-3~Sim-6 진행 중 별도 트랙 가능.

---

## 3. Phase Sim-1 — SharedGameSim 경계 + Hook 2 분리 + 결정성 (★ v1.1 6단계)

### 3.1 폴더 구조 — Hook 2 분리 박제

```
Shared/                                  ★ 서버/클라/AI 공통 — 결정성 강제
├── GameSim/
│   ├── World.h                          # CWorld alias / forward
│   ├── DeterministicTime.h              ★ Sim-1A
│   ├── DeterministicRng.h               ★ Sim-1A
│   ├── EntityIdMap.h                    ★ Sim-1A — NetEntityId ↔ EntityID
│   ├── Components/
│   │   ├── ChampionComponent.h
│   │   ├── HealthComponent.h
│   │   ├── ManaComponent.h
│   │   ├── StatComponent.h              ★ Sim-2 (championId 만, pointer X)
│   │   ├── BuffComponent.h              ★ Sim-2
│   │   ├── SkillStateComponent.h
│   │   ├── DamageRequestComponent.h     ★ Sim-1B (wrapper)
│   │   ├── PendingHitComponent.h        ★ Sim-1B (wrapper) — B-4 일반화
│   │   ├── ProjectileKindComponent.h    ★ Sim-1B (wrapper)
│   │   └── NetEntityIdComponent.h       ★ Sim-1A — network 식별
│   ├── Champions/                       ★ Hook 2 분리 — Shared 쪽 (월드 변경)
│   │   ├── Ezreal_Gameplay.h/.cpp       ★ Sim-1D — 위치/투사체/Damage/Buff/쿨다운
│   │   ├── Yasuo_Gameplay.h/.cpp        ★ Sim-1D
│   │   ├── Riven_Gameplay.h/.cpp        ★ Sim-1D
│   │   └── ...                          (B-11d-bis 에서 9 챔프 마이그레이션)
│   ├── Systems/
│   │   ├── ICommandExecutor.h           ★ Sim-1A
│   │   ├── GameplayHookRegistry.h/.cpp  ★ Sim-1C — Shared 전용 (월드 변경)
│   │   ├── DamagePipeline.h/.cpp        ★ Sim-2
│   │   ├── StatSystem.h/.cpp            ★ Sim-2
│   │   ├── SkillRankSystem.h/.cpp       ★ Sim-2
│   │   ├── BuffSystem.h/.cpp            ★ Sim-2
│   │   ├── PendingHitSystem.h/.cpp      ★ Sim-1E (Yasuo 의존 X)
│   │   ├── ProjectileSystem.h/.cpp      ★ Sim-1E generic
│   │   └── DeterministicEntityIterator.h ★ Sim-1A — sorted iteration (C-3)
│   ├── Definitions/
│   │   ├── ChampionDef.h                ★ Sim-1B (정본)
│   │   ├── SkillDef.h                   ★ Sim-1B (정본 + skillId/scalingTableId)
│   │   ├── ChampionStatsDef.h           ★ Sim-2 신설
│   │   ├── SkillScalingTable.h          ★ Sim-2 신설
│   │   ├── ItemDef.h                    ★ Sim-2 신설
│   │   └── SkinDef.h                    ★ Sim-2 신설
│   └── Registries/                      ★ ID 기반 lookup (C-5)
│       ├── ChampionStatsRegistry.h/.cpp ★ Sim-2
│       ├── SkillScalingRegistry.h/.cpp  ★ Sim-2
│       └── ItemRegistry.h/.cpp          ★ Sim-2
├── Schemas/                             ★ Sim-3
│   ├── Command.fbs                      (★ sessionId 기반)
│   ├── Snapshot.fbs
│   └── Event.fbs
└── Network/
    ├── PacketDef.h                      (기존)
    └── PacketTypes.h                    ★ Sim-3

Client/Private/GameObject/Champion/      ★ Visual hook 만 잔존
├── Ezreal/
│   ├── Ezreal_Visual.h/.cpp             ★ Sim-1D — anim/FX/sound/camera (Gameplay X)
│   ├── Ezreal_FxPresets.h/.cpp          (기존 — Visual 안에서 호출)
│   ├── Ezreal_Components.h              ★ Visual 전용 컴포넌트만 (gameplay state X)
│   └── Ezreal_Registration.cpp          ★ 양 Registry 등록 (Gameplay + Visual)
└── ...

Client/Public/GamePlay/
├── VisualHookRegistry.h/.cpp            ★ Sim-1D — Client 전용 (std::function OK)
├── SkillHookContext.h                   (Visual ctx 만)
├── SkillRegistry.h                      (UI lookup OK — Sim 결과에 미사용)
└── ChampionRegistry.h                   (UI lookup OK)
```

### 3.2 ICommandExecutor — sessionId 기반 issuer (★ C-2)

```cpp
// Shared/GameSim/Systems/ICommandExecutor.h
#pragma once
#include "Shared/GameSim/World.h"
#include "Shared/GameSim/DeterministicTime.h"
#include "Shared/GameSim/DeterministicRng.h"
#include "Shared/GameSim/EntityIdMap.h"
#include "Shared/GameSim/Definitions/SkillDef.h"

struct TickContext
{
    u64_t tickIndex = 0;
    f32_t fDt = 1.f / 30.f;
    f64_t fSimulatedTimeSec = 0;
    DeterministicRng* pRng = nullptr;
    EntityIdMap* pEntityMap = nullptr;       // ★ NetEntityId ↔ EntityID
    EntityID localPlayer = NULL_ENTITY;       // 클라 only — 서버 NULL
};

enum class eCommandKind : u8_t {
    None=0, Move=1, CastSkill=2, BasicAttack=3,
    LevelSkill=4, BuyItem=5, UseItem=6, Recall=7,
};

// ★ v1.1: 클라가 보내는 wire 패킷 — issuerEntity 송신 X
struct GameCommandWire
{
    eCommandKind kind = eCommandKind::None;
    u64_t        clientTick = 0;
    u32_t        sequenceNum = 0;
    u8_t         slot = 0;
    NetEntityId  targetNet = 0;       // ★ NetEntityId only
    Vec3         groundPos{};
    Vec3         direction{};
    u16_t        itemId = 0;
};

// ★ v1.1: 서버 내부 변환 — issuerEntity 는 sessionId 기반 결정
struct GameCommand
{
    eCommandKind kind = eCommandKind::None;
    EntityID     issuerEntity = NULL_ENTITY;   // 서버 결정 — 클라 신뢰 X
    u64_t        issuedAtTick = 0;
    u32_t        sequenceNum = 0;
    u8_t         slot = 0;
    EntityID     targetEntity = NULL_ENTITY;    // NetEntityId → EntityID 변환됨
    Vec3         groundPos{};
    Vec3         direction{};
    u16_t        itemId = 0;
};

class ICommandExecutor {
public:
    virtual ~ICommandExecutor() = default;
    virtual void ExecuteCommand(CWorld& world, const TickContext& tc,
                                 const GameCommand& cmd) = 0;
};

class CDefaultCommandExecutor final : public ICommandExecutor {
public:
    static std::unique_ptr<CDefaultCommandExecutor> Create();
    ~CDefaultCommandExecutor() = default;
    void ExecuteCommand(CWorld& world, const TickContext& tc,
                       const GameCommand& cmd) override;

private:
    CDefaultCommandExecutor() = default;
    void HandleMove(CWorld&, const TickContext&, const GameCommand&);
    void HandleCastSkill(CWorld&, const TickContext&, const GameCommand&);
    void HandleBasicAttack(CWorld&, const TickContext&, const GameCommand&);
    void HandleLevelSkill(CWorld&, const TickContext&, const GameCommand&);
    void HandleBuyItem(CWorld&, const TickContext&, const GameCommand&);
};

// ★ Server 진입 helper:
//   GameCommandWire (클라 송신) + sessionId → GameCommand (서버 내부)
//   issuerEntity = m_sessionToEntity[sessionId]  ← 클라 신뢰 X
GameCommand BuildServerCommand(const GameCommandWire& wire,
                                u32_t sessionId,
                                EntityID controlledEntity,
                                const EntityIdMap& map);
```

### 3.3 DeterministicTime — std::chrono 추방

```cpp
// Shared/GameSim/DeterministicTime.h
#pragma once
#include "WintersTypes.h"

// ★ 절대 std::chrono / GetTickCount / float clock 금지
//   sim 안에서 시간이 필요하면 TickContext::fSimulatedTimeSec 만 사용
struct DeterministicTime
{
    static constexpr f32_t kFixedDt = 1.f / 30.f;
    static constexpr u64_t kTicksPerSecond = 30;

    static f64_t TickToSec(u64_t tick) { return tick * static_cast<f64_t>(kFixedDt); }
    static u64_t SecToTick(f64_t sec)  { return static_cast<u64_t>(sec * kTicksPerSecond); }
};
```

### 3.4 DeterministicRng — seed 주입 강제

```cpp
// Shared/GameSim/DeterministicRng.h
#pragma once
#include "WintersTypes.h"

class DeterministicRng
{
public:
    explicit DeterministicRng(u64_t seed) : m_state(seed ? seed : 0x9E3779B97F4A7C15ull) {}

    u64_t NextU64() {
        u64_t x = m_state;
        x ^= x << 13; x ^= x >> 7; x ^= x << 17;
        return m_state = x;
    }
    u32_t NextU32() { return static_cast<u32_t>(NextU64() & 0xFFFFFFFFull); }
    f32_t NextF01() { return (NextU32() >> 8) * (1.f / 16777216.f); }
    bool  RollChance(f32_t prob) { return NextF01() < prob; }

    u64_t GetState() const { return m_state; }
    void  SetState(u64_t s)  { m_state = s; }

    // ★ v1.1: 결정성을 위한 sub-stream 분리
    //   같은 tick 안에 entity A 의 RNG 호출이 entity B 결과에 영향 X
    //   → tickIndex + sourceEntityId + skillId 로 sub-seed 생성
    u64_t MakeSubSeed(u64_t tickIndex, u32_t sourceEntityId, u16_t skillId) const {
        u64_t s = m_state;
        s ^= tickIndex * 0xBF58476D1CE4E5B9ull;
        s ^= static_cast<u64_t>(sourceEntityId) * 0x94D049BB133111EBull;
        s ^= static_cast<u64_t>(skillId) * 0x9E3779B97F4A7C15ull;
        return s;
    }

private:
    u64_t m_state;
};
```

### 3.5 World 추상 + DeterministicEntityIterator (★ C-3)

```cpp
// Shared/GameSim/World.h
#pragma once
class CWorld;
namespace SharedSim { using World = ::CWorld; }
```

```cpp
// Shared/GameSim/Systems/DeterministicEntityIterator.h
#pragma once
#include "Shared/GameSim/World.h"
#include "WintersTypes.h"
#include <vector>
#include <algorithm>

// ★ C-3: 시뮬 결과를 만드는 모든 순회는 정렬된 EntityID 사용
//   World 내부의 unordered_map / hash bucket 순서에 의존 금지
template<typename TComponent>
class DeterministicEntityIterator
{
public:
    static std::vector<EntityID> CollectSorted(CWorld& world)
    {
        std::vector<EntityID> entities;
        world.ForEach<TComponent>(
            std::function<void(EntityID, TComponent&)>(
                [&](EntityID e, TComponent&) { entities.push_back(e); }));
        std::sort(entities.begin(), entities.end());   // ★ EntityID 오름차순
        return entities;
    }
};

// 사용 예 (sim 시스템 안):
//   auto sorted = DeterministicEntityIterator<HealthComponent>::CollectSorted(m_world);
//   for (EntityID e : sorted) { /* sim 로직 */ }
```

### 3.6 헤더 이전 — Wrapper 점진 이전 (★ C-4)

**원칙**: 정본은 Shared 에, 기존 Client 헤더는 1줄 wrapper. 사용처 #include 천천히 변경.

```cpp
// 예시 — Sim-1B 패턴
//
// (1) Shared/GameSim/Definitions/SkillDef.h 정본 — 기존 SkillDef 본문 통째 이동
//     + skillId/scalingTableId 필드 추가 (★ Sim-2 대비)
//
// (2) Client/Public/GameObject/SkillDef.h 를 wrapper 로 축소:
#pragma once
#include "Shared/GameSim/Definitions/SkillDef.h"
//
// (3) 빌드 통과 확인 — 사용처 30+ 곳 무수정
//
// (4) 사용처 #include 점진 변경 (한 사이클 5~10 파일):
//     -- "GameObject/SkillDef.h"
//     ++ "Shared/GameSim/Definitions/SkillDef.h"
//
// (5) 모든 사용처 변경 후 wrapper 폐기
```

| 정본 위치 (Sim-1B 신설) | Wrapper (잠시 유지) |
|------------------------|---------------------|
| `Shared/GameSim/Definitions/SkillDef.h` | `Client/Public/GameObject/SkillDef.h` (1줄) |
| `Shared/GameSim/Definitions/ChampionDef.h` | `Client/Public/GameObject/ChampionDef.h` (1줄) |
| `Shared/GameSim/Components/DamageRequestComponent.h` + `Systems/Damage.h` | `Client/Public/GamePlay/Systems/Damage.h` (1줄) |
| `Shared/GameSim/Components/ProjectileKindComponent.h` | `Client/Public/GameObject/Projectile/ProjectileKind.h` (1줄) |
| `Shared/GameSim/Systems/PendingHitSystem.h/.cpp` | `Client/Public/GameObject/Champion/Yasuo/PendingHitSystem.h` (1줄, Sim-1E 후 폐기) |

★ **금지**: 사용처를 한 사이클에 다 바꾸지 말 것. v3.1 안정성 깨짐.

### 3.7 검증 마일스톤 — 결정성 12종 grep (★ C-3 강화)

| 검증 | 합격 |
|------|------|
| 빌드 통과 | Shared/ 신규 + Client wrapper |
| 9 챔프 회귀 + Ezreal F5 #2 | 시각·동작 0 변화 |
| `grep std::chrono Shared/` | 0 hit |
| `grep "::rand\|std::mt19937\|std::random_device" Shared/` | 0 hit |
| `grep Scene_InGame Shared/` | 0 hit |
| `grep CYasuoProjectileSystem Shared/` | 0 hit |
| **★ `grep unordered_map Shared/GameSim/Systems/`** | **0 hit (시뮬 순회)** — Registry lookup-only OK |
| **★ EntityID iteration order** | `DeterministicEntityIterator` 만 |
| **★ RNG 소비 순서** | `MakeSubSeed(tick, sourceId, skillId)` 박제 |
| **★ System 실행 DAG** | Scheduler 가 deterministic order 강제 |
| **★ EntityID 생성 순서** | `World::CreateEntity` 호출 순서 = tick 시작 시 frozen sort |
| **★ Render/Audio/FX → sim state 변경 X** | grep `world.GetComponent` Client/Renderer = `const` only |
| **★ Float mode** | `/fp:strict` 또는 `/fp:precise` (NOT `/fp:fast` — denormal 처리 다름) |

### 3.8 Hook 2 분리 (★ C-1 신규)

**hookId 4 단계 분류 갱신 (v1.1)**:

```
[keySwap]            Visual only — anim key 결정 (Client Registry 등록만)
                       Yasuo Q stack→spell1a/b/c, Ezreal E yaw→spell3_-90/90/180

[onCastAccepted]     양쪽 분기:
  - Gameplay (Shared): 위치 변경 (Ezreal E TP), 스택 갱신 (Riven Q++),
                        상태 전이 (Riven R bUlted=true), 마나 소비
  - Visual (Client):   카메라 자동, 입력 피드백 사운드, UI 반응

[castFrame]          양쪽 분기:
  - Gameplay (Shared): PendingHit 스폰, DamageRequest 발사, Buff 부여,
                        쿨다운 시작, 투사체 entity 생성
  - Visual (Client):   FX 메쉬/sprite, 사운드, 화면 흔들림, 빛/입자

[recovery]           양쪽 분기:
  - Gameplay (Shared): cleanup state (qStackCount reset 등), buff 해제
  - Visual (Client):   end FX, 잔상 해제, 카메라 원복
```

```cpp
// Shared/GameSim/Systems/GameplayHookRegistry.h
#pragma once
#include "Shared/GameSim/World.h"
#include "Shared/GameSim/Systems/ICommandExecutor.h"
#include "GameContext.h"
#include "Shared/GameSim/Definitions/SkillDef.h"

struct GameplayHookContext
{
    CWorld* pWorld = nullptr;
    EntityID casterEntity = NULL_ENTITY;
    eTeam casterTeam = eTeam::Blue;
    eChampion casterChampion = eChampion::NONE;
    u8_t skillRank = 1;
    const SkillDef* pDef = nullptr;
    const GameCommand* pCommand = nullptr;
    const TickContext* pTickCtx = nullptr;
};

class CGameplayHookRegistry
{
public:
    using HookFn = void(*)(GameplayHookContext&);   // ★ raw fn ptr (성능 + 결정성)

    static CGameplayHookRegistry& Instance();
    void Register(u32_t hookId, HookFn fn);
    bool Dispatch(u32_t hookId, GameplayHookContext& ctx) const;
    bool Has(u32_t hookId) const;

private:
    CGameplayHookRegistry() = default;

    // ★ v1.1: 시뮬 순회 결정성 — flat array (champ × variant)
    //   unordered_map 회피
    static constexpr u32_t kMaxChamp = 256;
    static constexpr u32_t kMaxVariant = 256;
    HookFn m_table[kMaxChamp][kMaxVariant] = {};
};
```

```cpp
// Shared/GameSim/Systems/GameplayHookRegistry.cpp
#include "Shared/GameSim/Systems/GameplayHookRegistry.h"

CGameplayHookRegistry& CGameplayHookRegistry::Instance() {
    static CGameplayHookRegistry s; return s;
}

void CGameplayHookRegistry::Register(u32_t hookId, HookFn fn) {
    const u32_t champ = (hookId >> 16) & 0xFF;
    const u32_t variant = hookId & 0xFF;
    m_table[champ][variant] = fn;
}

bool CGameplayHookRegistry::Dispatch(u32_t hookId, GameplayHookContext& ctx) const {
    if (hookId == 0) return false;
    const u32_t champ = (hookId >> 16) & 0xFF;
    const u32_t variant = hookId & 0xFF;
    HookFn fn = m_table[champ][variant];
    if (!fn) return false;
    fn(ctx);
    return true;
}

bool CGameplayHookRegistry::Has(u32_t hookId) const {
    const u32_t champ = (hookId >> 16) & 0xFF;
    const u32_t variant = hookId & 0xFF;
    return m_table[champ][variant] != nullptr;
}
```

```cpp
// Client/Public/GamePlay/VisualHookRegistry.h
#pragma once
#include "GameContext.h"
#include "Shared/GameSim/Definitions/SkillDef.h"
#include <functional>
#include <unordered_map>
#include <string>

class CWorld;

struct VisualHookContext
{
    CWorld* pWorld = nullptr;          // const view (변경 X — assert 으로 강제)
    EntityID casterEntity = NULL_ENTITY;
    const SkillDef* pDef = nullptr;
    const CastSkillCommand* pCommand = nullptr;
    std::string* pKeyOut = nullptr;     // keySwap 채널
};

class CVisualHookRegistry
{
public:
    using HookFn = std::function<void(VisualHookContext&)>;
    static CVisualHookRegistry& Instance();
    void Register(u32_t hookId, HookFn fn);
    bool Dispatch(u32_t hookId, VisualHookContext& ctx) const;
    bool Has(u32_t hookId) const;

private:
    CVisualHookRegistry() = default;
    std::unordered_map<u32_t, HookFn> m_Map;   // Client 전용 — 결정성 무관
};
```

★ **Ezreal_Registration.cpp 갱신 (Sim-1D)**:
```cpp
struct EzrealAutoRegister
{
    EzrealAutoRegister()
    {
        // ChampionDef + SkillDef[5] 등록 (변경 없음)
        // ...

        // ── Shared Gameplay hook (월드 변경) ──
        CGameplayHookRegistry::Instance().Register(kEz_BA_Cast,    &Ezreal::Gameplay::OnCastFrame_BA);
        CGameplayHookRegistry::Instance().Register(kEz_Q_Cast,     &Ezreal::Gameplay::OnCastFrame_Q);
        CGameplayHookRegistry::Instance().Register(kEz_W_Cast,     &Ezreal::Gameplay::OnCastFrame_W);
        CGameplayHookRegistry::Instance().Register(kEz_R_Cast,     &Ezreal::Gameplay::OnCastFrame_R);
        CGameplayHookRegistry::Instance().Register(kEz_E_OnAccept, &Ezreal::Gameplay::OnCastAccepted_E);

        // ── Client Visual hook (FX/anim) ──
        CVisualHookRegistry::Instance().Register(kEz_E_KeySwap,    &Ezreal::Visual::OnKeySwap_E);
        CVisualHookRegistry::Instance().Register(kEz_E_OnAccept,   &Ezreal::Visual::OnCastAccepted_E_Visual);  // FX flash
        CVisualHookRegistry::Instance().Register(kEz_BA_Cast,      &Ezreal::Visual::OnCastFrame_BA_Visual);
        CVisualHookRegistry::Instance().Register(kEz_Q_Cast,       &Ezreal::Visual::OnCastFrame_Q_Visual);
        CVisualHookRegistry::Instance().Register(kEz_W_Cast,       &Ezreal::Visual::OnCastFrame_W_Visual);
        CVisualHookRegistry::Instance().Register(kEz_R_Cast,       &Ezreal::Visual::OnCastFrame_R_Visual);
    }
};
```

★ **Scene_InGame DispatchHook 4 곳 갱신 — 양쪽 Dispatch**:
```cpp
// castFrame (Scene_InGame.cpp:998 영역):
if (bCastHit)
{
    m_bCastFrameFired = true;

    // ── Shared Gameplay (서버도 같은 코드 호출) ──
    GameplayHookContext gctx{};
    gctx.pWorld = &m_World;
    gctx.casterEntity = m_PlayerEntity;
    gctx.casterTeam = m_PlayerTeam;
    gctx.casterChampion = champ;
    gctx.skillRank = GetSkillRank(m_PlayerEntity, slot);
    gctx.pDef = m_pActiveSkillDef;
    gctx.pCommand = &m_GameCommandStorage;
    gctx.pTickCtx = &m_TickCtx;
    bool gameplayHandled = false;
    if (m_pActiveSkillDef->castFrameHookId != 0)
        gameplayHandled = CGameplayHookRegistry::Instance().Dispatch(
            m_pActiveSkillDef->castFrameHookId, gctx);

    // ── Client Visual (FX/sound) ──
    VisualHookContext vctx{};
    vctx.pWorld = &m_World;
    vctx.casterEntity = m_PlayerEntity;
    vctx.pDef = m_pActiveSkillDef;
    vctx.pCommand = &m_ActiveSkillCommandStorage;
    if (m_pActiveSkillDef->castFrameHookId != 0)
        CVisualHookRegistry::Instance().Dispatch(
            m_pActiveSkillDef->castFrameHookId, vctx);

    // ── legacy fallback (B-11d-bis 진행 중인 9 챔프) ──
    if (!gameplayHandled)
    {
        // 기존 if-elif (Garen/Zed/Riven/Kalista) — Sim-1D 마이그레이션 시 모두 _Gameplay.cpp 로 흡수 예정
    }
}
```

### 3.9 NetEntityId vs EntityID 분리 (★ C-2 신규)

```cpp
// Shared/GameSim/EntityIdMap.h
#pragma once
#include "WintersTypes.h"
#include "ECS/Entity.h"
#include <unordered_map>

using NetEntityId = u32_t;
constexpr NetEntityId NULL_NET_ENTITY = 0;

class EntityIdMap
{
public:
    EntityID FromNet(NetEntityId net) const;
    NetEntityId ToNet(EntityID e) const;

    // 서버 only — 새 entity 생성 시
    NetEntityId IssueNew(EntityID e);

    // 클라 only — Snapshot 수신 시
    void Bind(NetEntityId net, EntityID e);

    void Unbind(NetEntityId net);

    // ★ NetEntityId 는 서버 단조 증가 (1, 2, 3, ...) — 결정성 보장
    // ★ EntityID 는 process local — 임의 순서 OK

private:
    std::unordered_map<NetEntityId, EntityID> m_netToLocal;
    std::unordered_map<EntityID, NetEntityId> m_localToNet;
    NetEntityId m_nextNetId = 1;   // 서버 only
};
```

```cpp
// Shared/GameSim/Components/NetEntityIdComponent.h
struct NetEntityIdComponent { NetEntityId netId = NULL_NET_ENTITY; };
```

```
[원칙 박제]
- 네트워크 패킷의 entity 식별 = NetEntityId only
- 클라 입력 = sessionId 기반 issuer 추론 (서버) — 클라가 issuerEntity 직접 보내지 않음
- Snapshot = NetEntityId 기준 직렬화 → 클라가 EntityIdMap 갱신
- AI/Bot/MCTS = EntityID 직접 사용 OK (단일 프로세스)
- Replay = NetEntityId 기준 (서버 로그 재생)
- WildSpawn (server-spawned, e.g. 미니언) = 서버가 IssueNew → Snapshot 으로 전파
```

---

## 4. Phase Sim-2 — 5 축 공통 (★ ID 기반, C-5)

★ 별도 .md 박제: `.md/plan/sim/02_STAT_DAMAGE_SKILLRANK.md` (Sim-2 진입 시 작성)

### 4.1 ChampionStatsDef + Component (★ pointer X)

```cpp
// Shared/GameSim/Definitions/ChampionStatsDef.h
struct ChampionStatsDef
{
    eChampion championId = eChampion::NONE;       // ★ 키
    f32_t baseHp = 600.f;       f32_t hpPerLevel = 100.f;
    f32_t baseMana = 300.f;     f32_t manaPerLevel = 50.f;
    f32_t baseAd = 60.f;        f32_t adPerLevel = 3.5f;
    f32_t baseAp = 0.f;         f32_t apPerLevel = 0.f;
    f32_t baseArmor = 30.f;     f32_t armorPerLevel = 4.0f;
    f32_t baseMr = 30.f;        f32_t mrPerLevel = 1.25f;
    f32_t baseAttackSpeed = 0.65f; f32_t attackSpeedPerLevel = 0.025f;
    f32_t baseAttackRange = 5.5f;  f32_t baseMoveSpeed = 340.f;
};
```

```cpp
// Shared/GameSim/Components/StatComponent.h
struct StatComponent
{
    eChampion championId = eChampion::NONE;       // ★ pointer 대신 ID (C-5)
    u8_t level = 1;
    u32_t buffMaskHash = 0;
    u32_t itemMaskHash = 0;

    // 출력 (StatSystem 갱신, 직렬화 가능)
    f32_t hpMax = 0.f;        f32_t manaMax = 0.f;
    f32_t ad = 0.f;           f32_t ap = 0.f;
    f32_t armor = 0.f;        f32_t mr = 0.f;
    f32_t attackSpeed = 0.f;  f32_t attackRange = 0.f;
    f32_t moveSpeed = 0.f;    f32_t critChance = 0.f;
    f32_t critDamage = 1.75f; f32_t lifesteal = 0.f;
    f32_t spellVamp = 0.f;    f32_t armorPen = 0.f;
    f32_t mrPen = 0.f;        f32_t cdr = 0.f;

    bool bDirty = true;
};
```

```cpp
// Shared/GameSim/Registries/ChampionStatsRegistry.h
class CChampionStatsRegistry {
public:
    static CChampionStatsRegistry& Instance();
    void Add(eChampion id, const ChampionStatsDef& def);
    const ChampionStatsDef* Find(eChampion id) const;
};

// StatSystem.cpp 매 tick (dirty 시):
void CStatSystem::Recompute(StatComponent& stat)
{
    auto* pDef = CChampionStatsRegistry::Instance().Find(stat.championId);
    if (!pDef) return;

    const f32_t fLvl = static_cast<f32_t>(stat.level - 1);
    stat.hpMax       = pDef->baseHp + fLvl * pDef->hpPerLevel;
    stat.manaMax     = pDef->baseMana + fLvl * pDef->manaPerLevel;
    stat.ad          = pDef->baseAd + fLvl * pDef->adPerLevel;
    stat.armor       = pDef->baseArmor + fLvl * pDef->armorPerLevel;
    stat.mr          = pDef->baseMr + fLvl * pDef->mrPerLevel;
    stat.attackSpeed = pDef->baseAttackSpeed * (1.f + fLvl * pDef->attackSpeedPerLevel);
    stat.attackRange = pDef->baseAttackRange;
    stat.moveSpeed   = pDef->baseMoveSpeed;
    // 아이템/버프 modifier 적용 — Sim-2 Buff/Item 시스템에서 chain
}
```

### 4.2 StatSystem 재계산 흐름

```
매 tick 또는 dirty 시:
  1. base + levelGrowth → 1차 stat
  2. flat item adders → 2차
  3. flat buff adders → 3차
  4. % multipliers (item) → 4차
  5. % multipliers (buff) → 5차
  6. 캡 (cdr ≤ 0.4, attackSpeed ≤ 2.5 등)
```

### 4.3 SkillRankSystem (★ ID 기반)

```cpp
// SkillScalingTable.h
struct SkillScalingTable
{
    static constexpr u8_t kMaxRank = 5;
    u16_t scalingTableId = 0;        // ★ 키
    f32_t damage[kMaxRank]      = { 0.f };   // [70, 95, 120, 145, 170]
    f32_t cooldownSec[kMaxRank] = { 0.f };   // [6, 5.5, 5, 4.5, 4]
    f32_t manaCost[kMaxRank]    = { 0.f };
    f32_t adRatio[kMaxRank]     = { 0.f };
    f32_t apRatio[kMaxRank]     = { 0.f };
};

// SkillDef.h 확장 (★ Sim-1B 시점 wrapper 에 추가):
struct SkillDef {
    // ... 기존 ...
    u16_t skillId = 0;           // ★ Sim-2 — global unique
    u16_t scalingTableId = 0;    // ★ Sim-2 — Registry lookup
};

// SkillScalingRegistry.h
class CSkillScalingRegistry {
public:
    static CSkillScalingRegistry& Instance();
    void Add(u16_t scalingTableId, const SkillScalingTable& tbl);
    const SkillScalingTable* Find(u16_t id) const;
};

// SkillRankComponent
struct SkillRankComponent
{
    u8_t ranks[5] = { 0, 0, 0, 0, 0 };     // BA/Q/W/E/R
    u8_t pointsAvailable = 0;
};

// LevelSkillCommand 처리:
void OnLevelSkill(CWorld& world, EntityID entity, u8_t slot)
{
    if (!world.HasComponent<SkillRankComponent>(entity)) return;
    auto& sr = world.GetComponent<SkillRankComponent>(entity);
    if (sr.pointsAvailable == 0) return;
    if (sr.ranks[slot] >= 5) return;
    sr.ranks[slot]++;
    sr.pointsAvailable--;
    // emit SkillRankUpEvent
}
```

### 4.4 DamagePipeline (★ skillId / scalingTableId)

```cpp
// Shared/GameSim/Systems/DamagePipeline.h
enum class eDamageType : u8_t { Physical, Magic, True };

struct DamageRequest
{
    EntityID source = NULL_ENTITY;
    EntityID target = NULL_ENTITY;
    eTeam sourceTeam = eTeam::Neutral;
    eDamageType type = eDamageType::Physical;
    f32_t flatAmount = 0.f;
    u16_t skillId = 0;             // ★ ID 기반 (pointer X)
    u8_t  rank = 1;
    f32_t adRatioOverride = 0.f;   // 0 = scalingTable 의 값 사용
    f32_t apRatioOverride = 0.f;
    u32_t flags = 0;               // CanCrit / CanLifesteal / TrueDamage / Onhit
};

struct DamageResult
{
    f32_t finalAmount = 0.f;
    bool bWasCrit = false;
    bool bWasShielded = false;
    bool bKilled = false;
};

DamageResult ApplyDamageRequest(CWorld& world, const TickContext& tc,
                                const DamageRequest& req);
void EnqueueDamageRequest(CWorld& world, const DamageRequest& req);
```

### 4.5 BuffComponent + BuffSystem

```cpp
struct BuffInstance
{
    u32_t buffDefId;
    EntityID source;
    f32_t fDurationRemaining;
    u8_t  stackCount;
    u32_t paramHash;
};

struct BuffComponent
{
    static constexpr u8_t kMaxBuffs = 16;
    BuffInstance buffs[kMaxBuffs];
    u8_t count;
};

class CBuffSystem {
public:
    static void Execute(CWorld& world, const TickContext& tc);
};
```

### 4.6 SkinDef + SkinRegistry

```cpp
struct SkinDef
{
    u32_t skinId = 0;                 // ★ 키
    eChampion championId = eChampion::NONE;
    const char* fbxPathOverride = nullptr;
    const wchar_t* texturePathOverride[8] = {};
    const char* animPrefixOverride = nullptr;     // nullptr = base
    u32_t fxOverrideHookId = 0;
    const char* displayName = nullptr;
};

class CSkinRegistry {
public:
    static CSkinRegistry& Instance();
    void Add(u32_t skinId, const SkinDef& def);
    const SkinDef* Find(u32_t skinId) const;
    void ForEachByChampion(eChampion, const std::function<void(const SkinDef&)>&) const;
};
```

★ **원칙**: SkinRegistry = **외형 only**. 게임플레이 (스탯/스킬 데미지) 는 ChampionRegistry/SkillRegistry 에 남음. 서버는 skin 무관.

### 4.7 Sim-2 검증 마일스톤

| 검증 | 합격 |
|------|------|
| Stat 재계산 | 레벨 1 / 18 hp/ad/armor 수치 = LoL Wiki ±5% |
| Damage 공식 | 100ad + 1.0 ratio + 30 armor = 100×100/(100+30) = 76.9 |
| SkillRank 점수 | 18 레벨 = 18 SP (Q5 W5 E5 R3) |
| Buff stack | 콘퀘러 5중첩 ad 보너스 적용 |
| Skin 스왑 | 외형만 변경, hp/dmg 수치 동일 |
| Direct 직렬화 | StatComponent / DamageRequest / SkillRankComponent FlatBuffers 통과 |

---

## 5. Phase Sim-3 — Schema (★ sessionId 기반)

★ 별도 .md: `.md/plan/sim/03_SCHEMA_FLATBUFFERS.md`

### 5.1 Command.fbs (v1.1, ★ issuerEntity 제거)

```fbs
namespace Shared.Schema;

enum CommandKind : ubyte {
    None=0, Move=1, CastSkill=2, BasicAttack=3,
    LevelSkill=4, BuyItem=5, UseItem=6, Recall=7
}

table Vec3 { x:float; y:float; z:float; }

table CommandPacket {
    kind:CommandKind;
    // ★ v1.1 — issuerEntity 제거. 서버가 sessionId 기반 결정
    sequenceNum:uint;
    clientTick:ulong;
    slot:ubyte;
    targetNet:uint;          // NetEntityId only
    groundPos:Vec3;
    direction:Vec3;
    itemId:ushort;
}

root_type CommandPacket;
```

### 5.2 Snapshot.fbs (★ NetEntityId 기준)

```fbs
table EntitySnapshot {
    netId:uint;              // ★ NetEntityId
    hp:float;
    mana:float;
    posX:float; posY:float; posZ:float;
    yaw:float;
    animId:ushort;           // ★ v1.1 Codex P1 — string 추방 (30Hz 비용)
    animPhaseFrame:ushort;
    skillCooldowns:[float];
    buffMask:uint;
    level:ubyte;
    skillRanks:[ubyte];
}

table Snapshot {
    serverTick:ulong;
    serverTimeMs:ulong;
    entities:[EntitySnapshot];
    lastAckedCommandSeq:[uint];   // sessionId → 마지막 처리 sequence
    rngState:ulong;          // ★ 결정성 검증 — 클라가 같은 seed 로 rollback
}

root_type Snapshot;
```

### 5.3 Event.fbs

```fbs
enum EventKind : ubyte {
    Damage, Death, BuffApply, BuffExpire, ProjectileSpawn,
    SkillCast, ItemBuy, LevelUp, GameStart, GameEnd
}

table DamageEvent {
    sourceNet:uint; targetNet:uint; amount:float;
    type:ubyte; bWasCrit:bool;
}

table EventPacket {
    kind:EventKind;
    serverTick:ulong;
    damage:DamageEvent;
}
```

### 5.4 Sim-3 검증

| 검증 | 합격 |
|------|------|
| flatc codegen | C++ + Go 양쪽 컴파일 |
| 1 frame snapshot 크기 | < 1 KB (10 entity 기준) |
| 직렬화 round-trip | **semantic equality** (byte-identical 보장 X — FlatBuffers offset alignment 정상) |
| sequenceNum 무결성 | 1 클라가 sequenceNum 갭 시 reject |
| **★ TCP framing (PacketHeader)** | partial / sticky chunk 모두 정상 재조립 — `Shared/Network/PacketEnvelope.h` 박제 |

---

## 6. Phase Sim-4 — IOCP GameRoom (★ sessionId→entity 매핑)

★ 별도 .md: `.md/plan/sim/04_IOCP_GAMEROOM.md`

### 6.1 서버 폴더

```
Server/
├── Network/
│   ├── IOCPCore.h/.cpp
│   ├── Session.h/.cpp            # sessionId 보유
│   ├── Session_Manager.h/.cpp    # sessionId -> CSession* (Manager 류 컨벤션)
│   └── PacketDispatcher.h/.cpp
├── Game/
│   ├── GameRoom.h/.cpp           # 5v5, EntityIdMap 보유, 30Hz
│   ├── GameLogic.h/.cpp          # ICommandExecutor 호출
│   ├── ServerWorld.h/.cpp
│   └── AOI.h/.cpp                # 50m grid
└── Security/
    ├── AntiCheatServer.h/.cpp
    └── LagCompensation.h/.cpp
```

### 6.2 GameRoom 30Hz tick + sessionId→entity (★ C-2)

```cpp
class CGameRoom
{
    // sessionId → controlledEntity (★ 클라 신뢰 X)
    std::unordered_map<u32_t, EntityID> m_sessionToEntity;

    EntityIdMap m_entityMap;
    DeterministicRng m_rng;
    u64_t m_tickIndex = 0;
    std::unique_ptr<ICommandExecutor> m_pExecutor;

    void Tick()
    {
        TickContext tc{ ++m_tickIndex, kFixedDt,
                        DeterministicTime::TickToSec(m_tickIndex),
                        &m_rng, &m_entityMap };

        // 1. 입력 수집 — sessionId 기반 issuer 변환
        for (auto& [sessionId, wirePackets] : m_pendingWire)
        {
            EntityID issuer = m_sessionToEntity[sessionId];
            for (auto& wire : wirePackets)
            {
                GameCommand cmd = BuildServerCommand(wire, sessionId, issuer, m_entityMap);
                m_pExecutor->ExecuteCommand(m_world, tc, cmd);
            }
        }

        // 2. 5 축 공통 시스템 chain (sorted iteration)
        CStatSystem::Execute(m_world, tc);
        CBuffSystem::Execute(m_world, tc);
        CSkillCooldownSystem::Execute(m_world, tc);
        CMoveSystem::Execute(m_world, tc);
        CPendingHitSystem::Execute(m_world, tc);
        CProjectileSystem::Execute(m_world, tc);
        CDamageQueueSystem::Execute(m_world, tc);
        CDeathSystem::Execute(m_world, tc);

        // 3. AOI 컬링 + Snapshot (★ NetEntityId 기준)
        for (auto* pSession : m_sessions)
            pSession->SendSnapshot(BuildSnapshot(m_world, m_entityMap, pSession->visibleSet, tc));
    }
};
```

### 6.3 Sim-4 검증

| 검증 | 합격 |
|------|------|
| 10 클라 동접 1 룸 | tick 30Hz, jitter < 5ms |
| Anticheat speedhack | 클라 fSpeed > stat.moveSpeed × 1.05 → reject |
| Anticheat issuerSpoof | 클라가 다른 NetEntityId 의 Q 시도 → sessionId mismatch reject |
| 결정성 | 같은 seed + input → 5분 후 hp/pos bit-equal 2 룸 비교 |

---

## 7. Phase Sim-5 — Client Prediction + Reconciliation

★ 별도 .md: `.md/plan/sim/05_CLIENT_PREDICTION.md`

### 7.1 입력 큐 + EntityIdMap

```cpp
struct ClientInputBuffer {
    GameCommandWire commands[120];   // 4초 분량
    u32_t head, tail;
    u32_t nextSeq;
};

// 클라 EntityIdMap — Snapshot 의 NetEntityId → 로컬 EntityID
EntityIdMap m_localEntityMap;
```

### 7.2 Snapshot 적용 + Rollback

```cpp
void OnSnapshot(const Snapshot& snap)
{
    // 1. NetEntityId → 로컬 EntityID 매핑 갱신
    for (const auto& es : snap.entities) {
        if (m_localEntityMap.FromNet(es.netId) == NULL_ENTITY) {
            EntityID e = m_predictedWorld.CreateEntity();
            m_localEntityMap.Bind(es.netId, e);
        }
    }

    // 2. 서버 권위 상태로 World rewind
    ApplySnapshot(m_predictedWorld, snap, m_localEntityMap);
    m_localRng.SetState(snap.rngState);   // ★ RNG state 동기화

    // 3. lastAckedSeq 이후 입력 모두 재실행 (예측 재구성)
    u32_t ackedSeq = snap.lastAckedCommandSeq[m_mySession];
    for (u32_t seq = ackedSeq + 1; seq < m_inputBuffer.nextSeq; ++seq)
    {
        TickContext tc{ /* ... */ };
        EntityID issuer = m_localEntityMap.FromNet(/* my netId */);
        GameCommand cmd = BuildClientCommand(m_inputBuffer.commands[seq], issuer, m_localEntityMap);
        m_pExecutor->ExecuteCommand(m_predictedWorld, tc, cmd);
    }

    // 4. 시각 보간
    BeginRenderInterpolation(m_predictedWorld);
}
```

### 7.3 Sim-5 검증

| 검증 | 합격 |
|------|------|
| 100ms RTT | 캐스트 즉시 반응 (예측) + 200ms 후 권위 확인 |
| Mispredict | rollback 자연 보정 (visual jitter < 2 frame) |
| RNG state 동기화 | 클라 예측 / 서버 권위 결과 bit-equal |

---

## 8. Phase Sim-6 — Backend Skin/Match

★ 별도 .md: `.md/plan/sim/06_BACKEND_SKIN_MATCH.md`

### 8.1 GameSessionConfig

```go
type GameSessionConfig struct {
    SessionId uuid.UUID
    Players   []PlayerSlot
}

type PlayerSlot struct {
    UserId       int64
    SessionId    uint32     // ★ runtime sessionId
    ChampionId   uint8      // eChampion 명시값
    SkinId       uint32
    Team         uint8
    RunePresetId uint32
    ItemPresetId uint32
}
```

### 8.2 Skin 소유권 검증

```go
// /matchmaking/lock_in:
// 1. UserSkin DB 에서 skinId 소유 확인
// 2. 미소유면 base skin fallback
// 3. GameServer 에 SessionConfig 전송
```

### 8.3 Sim-6 검증

| 검증 | 합격 |
|------|------|
| 미소유 스킨 시도 | base skin fallback (anti-cheat 1차) |
| 매치 lock-in → 게임 시작 | < 5초 |

---

## 9. Phase Sim-7 — Rule/Utility Bot (병렬)

★ 별도 .md: `.md/plan/sim/07_BOT_AI.md`

### 9.1 IBotPolicy

```cpp
class IBotPolicy {
public:
    virtual GameCommand Decide(const CWorld& world, const TickContext& tc,
                                EntityID self) = 0;
};
```

### 9.2 단계

```
1. CRuleBotPolicy        — HFSM (Idle/Lane/Engage/Retreat/Recall)
2. CUtilityBotPolicy     — 점수 기반 (kill / push / objective / safety)
3. CGoapBotPolicy        — 목표 기반
```

### 9.3 Sim-7 검증

| 검증 | 합격 |
|------|------|
| 1v1 BotMid vs Player | Bot 라인 유지, 5분 cs 차이 < 30 |
| 5v5 Bot vs Player | 게임 진행 가능 |

---

## 10. Phase Sim-8 — MCTS (★ 결정성 전제)

★ 별도 .md: `.md/plan/sim/08_MCTS_PLANNER.md`

### 10.1 World Clone (★ Sim-1F 결정성 통과 후)

```cpp
class CWorldCloner {
public:
    static std::unique_ptr<CWorld> Clone(const CWorld& source);
    // ECS Component 메모리 + EntityID 보존 + RNG state + EntityIdMap 복사
};
```

### 10.2 MCTS 4 단계

```cpp
class CMCTSPlanner {
    GameCommand Plan(const CWorld& root, const TickContext& tc, EntityID self,
                     u32_t numSims, f32_t timeBudgetMs);

    // 1. Selection  — UCB1
    // 2. Expansion  — action mask (legal Command only)
    // 3. Rollout    — RuleBot 또는 random N tick
    // 4. Backprop   — value = winrate or HP-diff
};
```

### 10.3 Evaluator

```cpp
class IEvaluator {
public:
    virtual f32_t Evaluate(const CWorld& world, EntityID self) = 0;
};

class CHpDiffEvaluator final : public IEvaluator { /* hp diff scaled */ };
class CGoldDiffEvaluator final : public IEvaluator { /* gold diff */ };
class CCompositeEvaluator final : public IEvaluator { /* weighted sum */ };
```

### 10.4 Sim-8 검증

| 검증 | 합격 |
|------|------|
| 100ms / 1000 sims | RuleBot 보다 우세 (kill +20%) |
| 결정성 | 같은 root + seed → 같은 plan |
| Sim-1F 결정성 grep | 통과 (결정성 미통과 시 Sim-8 진입 X) |

---

## 11. Phase Sim-9 — RL

★ 별도 .md: `.md/plan/sim/09_RL_ENV.md`

### 11.1 Env interface

```cpp
class CGymEnv {
public:
    Observation Reset(u64_t seed);
    StepResult  Step(const Action& a);
    void        Render();

private:
    std::unique_ptr<CWorld> m_world;
    DeterministicRng m_rng;
    EntityIdMap m_entityMap;
};

struct Observation {
    f32_t features[256];
    u32_t actionMask;
};

struct Action { u8_t kind; u8_t slot; f32_t aimX, aimZ; };

struct StepResult {
    Observation nextObs;
    f32_t reward;
    bool bDone;
};
```

### 11.2 학습 파이프라인

```
1. Self-play loop — Bot vs Bot
2. Experience replay buffer (10M transitions)
3. PPO/SAC/IMPALA 트레이너 (Python)
4. ONNX export → Client/Server inference
5. Eval — vs RuleBot / vs MCTS / vs human
```

### 11.3 Sim-9 검증

| 검증 | 합격 |
|------|------|
| 1M step 학습 후 vs RuleBot | winrate > 60% |
| Inference latency | < 5ms / step (one entity) |
| Production fallback | RL fail 시 RuleBot 자동 swap |

---

## 12. 의존 그래프

```
                    v3.1 Ezreal F5 #1 합격
                            │
                            ▼
                    Sim-1A (폴더 + Determinism)
                            │
                            ▼
                    Sim-1B (Wrapper 도입)
                            │
                            ▼
                    Sim-1C (GameplayHookRegistry)
                            │
                            ▼
                    Sim-1D (Hook 2 분리)
                            │
                            ▼
                    Sim-1E (PendingHit 일반화)
                            │
                            ▼
                    Sim-1F (결정성 grep + 회귀)
                            │
                ┌───────────┼────────────┐
                ▼           ▼            ▼
            Sim-2         Sim-7        Sim-8
        (5축 공통)     (Rule Bot)    (MCTS — Sim-1F 결정성 전제)
                │           │            │
                ▼           │            │
            Sim-3                        │
        (Schema FBS)                     │
                │                        │
                ▼                        │
            Sim-4 (IOCP)                 │
                │                        │
                ▼                        │
            Sim-5 (Prediction)           │
                │                        │
                ▼                        │
            Sim-6 (Backend Skin/Match)   │
                                         │
                                         ▼
                                    Sim-9 (RL)
                                    │
                                    ▼
                          Production = (RL+MCTS hybrid)
                          Fallback = RuleBot
```

---

## 13. 진입 트리거 — Sim-1 6 단계 (★ v1.1)

**전제 처리**:
1. ✅ B-11d v3.1 F5 #1 합격
2. ⚠️ R1 Ezreal_FxPresets 본격 — Sim-1D 의 `Ezreal_Visual.cpp` 안에 흡수
3. ⚠️ R2 PendingHitSystem 일반화 — Sim-1E 에 흡수
4. ⚠️ R3 Ezreal_Registration.h 빈 헤더 — Sim-1D 정리 시 양 Registry 등록 declaration 으로 의미 부여 또는 삭제

### Sim-1A (30분) — SharedGameSim 폴더 + 결정성 유틸

1. `Shared/GameSim/` 폴더 + 서브폴더 생성 (Components/Champions/Systems/Definitions/Registries)
2. `DeterministicTime.h` 박제 (kFixedDt 30Hz)
3. `DeterministicRng.h` 박제 (xorshift64 + MakeSubSeed)
4. `EntityIdMap.h` + `NetEntityIdComponent.h` 박제
5. `World.h` (CWorld alias)
6. `Systems/ICommandExecutor.h` 시그니처 (CDefaultCommandExecutor 구현은 Sim-1D 후)
7. `Systems/DeterministicEntityIterator.h` 박제
8. **빌드 통과 확인** (Shared/ 폴더 컴파일만 — 사용처 0)

### Sim-1B (30분) — Wrapper 도입

1. `Shared/GameSim/Definitions/SkillDef.h` 정본 생성 (기존 본문 + skillId/scalingTableId 필드 추가)
2. `Shared/GameSim/Definitions/ChampionDef.h` 정본 생성
3. `Shared/GameSim/Components/DamageRequestComponent.h` + `Systems/Damage.h` 정본 (기존 흡수)
4. `Shared/GameSim/Components/ProjectileKindComponent.h` 정본
5. **5 개 기존 Client 헤더를 1줄 wrapper 로 축소**:
   - `Client/Public/GameObject/SkillDef.h`
   - `Client/Public/GameObject/ChampionDef.h`
   - `Client/Public/GamePlay/Systems/Damage.h`
   - `Client/Public/GameObject/Projectile/ProjectileKind.h`
   - `Client/Public/GameObject/Champion/Yasuo/PendingHitSystem.h` (Sim-1E 후 폐기)
6. **빌드 통과 + 9 챔프 + Ezreal F5 #2 회귀 0** ← Sim-1B 합격 마일스톤

### Sim-1C (30분) — Shared GameplayHookRegistry

1. `Shared/GameSim/Systems/GameplayHookRegistry.h/.cpp` 박제
   - `HookFn = void(*)(GameplayHookContext&)` raw fn ptr
   - flat array `[256][256]` lookup (unordered_map 회피)
2. `GameplayHookContext` POD 정의 (TickContext 포함)
3. **빌드 통과** (등록은 Sim-1D)

### Sim-1D (60분) — Hook 2 분리 마이그레이션

1. `Client/Public/GamePlay/VisualHookRegistry.h/.cpp` 박제 (`std::function` OK)
2. **Ezreal**: `Ezreal_Skills.cpp` 분해
   - `Shared/GameSim/Champions/Ezreal_Gameplay.cpp` 신설:
     - `OnCastFrame_BA` (DamageRequest), `OnCastFrame_Q/W/R` (PendingHit), `OnCastAccepted_E` (TransformComponent SetPosition)
   - `Client/Private/GameObject/Champion/Ezreal/Ezreal_Visual.cpp` 신설:
     - `OnKeySwap_E`, `OnCastAccepted_E_Visual` (FX flash), `OnCastFrame_*_Visual` (FX 메쉬/sprite)
   - **R1 흡수**: 기존 stub `Ezreal_FxPresets.cpp` 를 본격 구현으로 교체 (Visual hook 안에서 호출)
3. **Yasuo**: 같은 패턴 (Yasuo_Gameplay.cpp + Yasuo_Visual.cpp). Q/W/E/R 분리.
4. **Riven**: 같은 패턴.
5. `Ezreal_Registration.cpp` 갱신 — 양 Registry 등록 (R3 흡수)
6. **Scene_InGame DispatchHook 4 곳 갱신** — 양쪽 Dispatch (Gameplay + Visual)

### Sim-1E (30분) — PendingHitSystem 일반화

1. `Shared/GameSim/Systems/PendingHitSystem.h/.cpp` 정본 생성 (Yasuo/ 의 본문 흡수)
2. **`CYasuoProjectileSystem::Spawn` 의존 끊기** — `eProjectileKind` 별 generic dispatch:
   ```cpp
   switch (pending.kind) {
       case eProjectileKind::Wind:
       case eProjectileKind::Tornado:
       case eProjectileKind::EQRing:
           CYasuoProjectileSystem::Spawn(...); break;
       case eProjectileKind::MysticShot:
       case eProjectileKind::EssenceFlux:
       case eProjectileKind::GlobalBeam:
           CEzrealProjectileSystem::Spawn(...); break;  // ★ 신설 (또는 GenericProjectileSystem)
   }
   ```
3. Yasuo/PendingHitSystem.h wrapper 폐기

### Sim-1F (30분) — 빌드 + 회귀 + 결정성 grep

1. **전체 빌드** + 9 챔프 + Ezreal F5 #2 회귀 검증
2. **결정성 grep 12종** 모두 0 hit 확인 (§3.7)
3. **EntityID 생성 순서 / RNG 소비 / System 실행 DAG** 박제
4. **CLAUDE.md gotcha 추가** (G-Sim1~G-Sim8)
5. **MEMORY 메모** 박제 — `project_phase_sim1_complete.md`

---

## 14. CLAUDE.md / MEMORY 박제 (사이클 종료 시)

추가 gotcha:
- **G-Sim1**: `Shared/GameSim/` 안 `std::chrono / ::rand / Scene_*` 절대 금지. 결정성 = 서버 권위 / MCTS / 리플레이 / RL 학습 4 시스템의 1차 전제
- **G-Sim2**: hook = `DamageRequest` 만 만들고 계산은 `DamagePipeline` 이. 챔프 hook 안 hp -= amount 절대 금지
- **G-Sim3**: ICommandExecutor 단일 entry — Human/Bot/MCTS/RL/Replay/Network 전부
- **G-Sim4 (★ v1.1)**: Hook 2 분리. Gameplay (Shared, 월드 변경) vs Visual (Client, FX/anim/UI). 서버는 GameplayHookRegistry 만, 클라는 둘 다.
- **G-Sim5 (★ v1.1)**: 네트워크에서 EntityID 직접 송신 절대 금지. NetEntityId only. Issuer = sessionId 기반 서버 결정.
- **G-Sim6 (★ v1.1)**: 시뮬 순회에 unordered_map 사용 금지. DeterministicEntityIterator (sorted) 만. Registry lookup-only 면 unordered OK.
- **G-Sim7 (★ v1.1)**: 헤더 이전은 wrapper 점진. 정본 생성 + 1줄 wrapper + 사용처 점진 변경.
- **G-Sim8 (★ v1.1)**: Component 안 pointer 저장 금지. ID 기반 lookup. 직렬화/replay/AI 호환.

WINTERS_GAMEPLAY_ARCHITECTURE.md §11 추가:
- "⑧ SharedGameSim = 서버/클라/AI 단일 코어. hook = 의도, 시스템 = 공식, Schema = 직렬화 경계, Registry = ID lookup. 결정성 12종 grep 미통과 시 MCTS/RL 진입 X."

---

## 한 줄 (v1.1 final)

**v1.1 final = Codex 4 보정 + 1 박제 — Hook 2 분리 (Shared Gameplay + Client Visual) / NetEntityId vs EntityID (sessionId 기반 issuer) / 결정성 12종 (sorted iter + RNG sub-stream + System DAG + float strict) / Wrapper 점진 이전 (v3.1 안정성 보존) / ID 기반 (pointer 추방, 직렬화 호환). Sim-1 = 6 단계 (A-F) 210분. 9 phase + 결정성 박제로 150 챔프 + 30Hz 권위 시뮬 + MCTS/RL/Replay 모두 같은 코어 위. v3.1 = 운영 뼈대 정답 + v1.1 = 그 위에 결정성/직렬화/권위 경계 박제 = LoL 식 운영 1:1 미러.**
