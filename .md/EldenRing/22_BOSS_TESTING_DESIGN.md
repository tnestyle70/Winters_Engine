# Boss Pattern Testing Environment 상세 설계

> 작성: 2026-06-23. 대상: Codex 및 EldenRing 팀.
> 시스템 식별자: `BOSS_TESTING`
> 선행 문서: `17_UE5_GRADE_EDITOR_SUITE_MASTER.md`(5대 시스템 매핑·게이트), `12_UE5_REFERENCE_DX12_RHI_EDITOR_BIG_PICTURE.md`(Phase A~J·게이트 G0~G9),
> `06_FX_GRAPH_SEQUENCER_EDITOR.md`(Boss/Hitbox 섹션), `plan/EldenRingEditor/07_BOSS_BLACKBOARD_HFSM_BT_TUNING.md`(Blackboard/HFSM/BT 세션),
> `05_NETWORK_PVP_COOP_RAID_SERVER_AUTH.md`(서버 권위), `13_HKX_ANIMATION_PIPELINE.md`(TAE 이벤트).

---

## 0. 한 줄 목표 + 시스템 경계

**한 줄 목표**: 격리된 결정성 아레나에서 보스 패턴을 시드 고정으로 N회 자동 재생하며, hitbox active window 타이밍·회피 윈도우·텔레그래프 hit/miss를 로깅·시각화하는 **자동 패턴 테스트 루프**를 구축하고, 그 패턴을 편집하는 `BossPatternEditor` + `HitboxTimelineEditor`를 얹는다.

**이 문서가 다루는 시스템 경계**:

| 포함 | 제외(다른 문서 소유) |
|---|---|
| `BossBlackboard`(target/distance/phase/cooldown) | 스킨/메시 렌더 (World Editor / RHI) |
| `BossHFSM` + `BossBT` → **GameCommand 후보만 발행** | 파티클 그래프 시뮬 (FX Niagara 문서) |
| **3D hitbox/hurtbox overlap ECS 신규 구축**(현재 엔진에 없음) | 컷신 카메라/타임라인 (Sequencer 문서) |
| `HitboxTimeline`(active frame window) + TAE 이벤트 매핑 | cell 스트리밍/월드 파티션 (World Partition 문서) |
| 격리 아레나 자동 테스트 루프(시드 고정, N회 재생, hit/miss 로깅) | 멀기트 c2130 skeletal animation baking (HKX 파이프라인) |
| 결정성 debug overlay(blackboard/HFSM/BT/hitbox) | |
| `.wboss`/`.whitbox`(JSON 초기) 포맷 + 런타임 로더 | |

**서버/클라 경계 (절대 위반 금지)**: hitbox 판정·damage·phase 전이는 **Shared/GameSim 서버 권위**. 클라이언트는 anim/telegraph FX/UI/camera shake **재생만**. BossBT/HFSM 결정은 **GameCommand 후보로만** 발행하고 판정하지 않는다.

---

## 1. UE5 실제 아키텍처 (깊이)

### 1.1 Behavior Tree + Blackboard

UE5의 AI 결정은 `UBehaviorTree`(에셋) + `UBlackboardData`(키 스키마)로 분리된다.

- **Blackboard**: `UBlackboardComponent`는 key→value 저장소다. 키는 `FBlackboard::FKey`(uint8 인덱스)로 식별되고, 타입은 `UBlackboardKeyType_Object/Vector/Float/Enum/Bool` 등. BT 노드는 키를 직접 읽고 쓰며 **상태를 노드 바깥에 둔다** — 노드는 stateless하게 유지되고 결정 상태는 blackboard에 산다. 이것이 핵심 철학: *결정 로직(트리)과 결정 상태(blackboard)의 분리*로 디버깅·튜닝·교체가 쉬워진다.
- **Behavior Tree 노드 분류**:
  - **Composite**: `Selector`(첫 성공까지 순회), `Sequence`(첫 실패까지 순회), `SimpleParallel`(main task + background tree).
  - **Task**: leaf. `UBTTaskNode::ExecuteTask` → `EBTNodeResult`(Succeeded/Failed/InProgress/Aborted). 실제 행동(MoveTo, PlayAnim, custom)을 수행.
  - **Decorator**: 조건 가드. `UBTDecorator`는 sub-tree 진입을 gate하고, `FlowAbortMode`(Self/LowerPriority/Both)로 조건 변화 시 실행 중 abort를 트리거. blackboard 키 변화를 observe해서 재평가.
  - **Service**: composite에 부착되어 주기적으로 tick(`TickNode`). 보통 blackboard 갱신(타겟 거리 재계산 등)에 사용.
- **실행 모델**: BT는 매 프레임 전체를 순회하지 않는다. **이벤트 기반 + 활성 노드 추적**. `UBehaviorTreeComponent`가 현재 실행 노드를 들고 있고, decorator의 observer가 abort를 일으킬 때만 상위 재평가. 이 "active node 중심 평가"가 성능과 결정성의 균형점.

**UE5가 이렇게 설계한 이유**: 디자이너가 코드 없이 행동을 조립·관찰하고, blackboard로 상태를 외재화해 AI Debugger가 실시간으로 들여다보게 하려는 것. 트리는 데이터(에셋), 실행은 컴포넌트, 상태는 blackboard — 3분리.

### 1.2 HFSM (계층적 상태 머신)

UE5는 BT를 주로 쓰지만 보스/시퀀스 행동에는 사실상 HFSM 성격이 섞인다(StateTree 플러그인이 이를 명시화). HFSM은 상위 state(Phase) 안에 하위 state(Combo step)를 두고, 전이 조건을 계층으로 관리한다. 보스 패턴에 적합한 이유: **Phase1/Phase2** 같은 큰 모드 전이와 그 안의 **Engage→Combo→Recover** 미세 전이를 분리해서 표현.

### 1.3 EQS (Environment Query System)

`UEnvQuery`는 공간 후보(grid/donut/around actor)를 생성→`UEnvQueryTest`로 스코어링(거리/시야/경로비용)→best item 선택. 보스 패턴에선 "가까운 회피 위치 찾기" 같은 공간 질의에 쓰인다. **본 문서에선 초기 범위 제외**(거리 기반 룰로 대체), 후순위.

### 1.4 AI Debugger (Gameplay Debugger)

`'`(apostrophe) 키로 켜는 `UGameplayDebuggerCategory_AI`. 화면에 blackboard 값, 현재 BT 경로, EQS 결과, perception 시야를 실시간 오버레이. **철학**: AI 결정은 블랙박스가 아니라 *관찰 가능*해야 한다. 결정 사유(왜 이 노드가 선택됐는가)를 시각화.

### 1.5 Gauntlet (자동화 테스트 프레임워크)

`Gauntlet`은 UE5의 자동화 테스트 하네스다. **세션을 스크립트로 부팅→고정 입력/시드로 시나리오 실행→결과(로그/스크린샷/지표) 수집→pass/fail 판정**. CI에서 빌드마다 회귀 검증. 핵심 개념:
- `UnrealSession`: 게임 인스턴스를 controlled하게 기동.
- `TestNode`: 시나리오 단위. setup→tick loop→teardown.
- **결정성/반복성**: 같은 시드·같은 입력이면 같은 결과. 이게 회귀 테스트의 전제.

**보스 테스트 환경의 직접 영감**: Gauntlet의 "시드 고정 + 시나리오 N회 + 지표 수집"을 **보스 패턴 1개**에 적용한다. 패턴을 격리 아레나에서 N회 재생하고, hitbox window가 의도한 프레임에 켜졌는지·더미가 회피 윈도우에 회피했는지를 로깅.

### 1.6 AI Perception

`UAIPerceptionComponent` + `UAISense`(Sight/Hearing/Damage). 자극을 stimuli로 모아 blackboard에 타겟 후보를 제공. **본 문서 초기 범위 제외** — 아레나엔 타겟이 명시적이므로 거리/시야 룰로 충분.

---

## 2. Winters 현재 구조 (실측 근거)

### 2.1 재사용 가능한 실제 코드

| 영역 | 파일:라인 | 재사용 포인트 |
|---|---|---|
| **AI Execute 시그니처** | `Shared/GameSim/Systems/ChampionAI/ChampionAISystem.h:12` | `static void Execute(CWorld&, const TickContext&, std::vector<GameCommand>&)` — BossAISystem이 그대로 차용 |
| **Command 후보 발행** | `Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp:356-368, 476-545` | `MakeAICommand()`로 `GameCommand` 채워 `outCommands.push_back`. Boss도 동일 패턴 |
| **Blackboard/HFSM/tuning/trace 선례** | `Shared/GameSim/Components/ChampionAIComponent.h:96-322` | `ChampionAIDecisionTraceEntry`(ring buffer), `ChampionAITuningParam{default/current/min/max/override}`, `eChampionAIDecisionBlockReason`. Boss용으로 그대로 모사 |
| **GameCommand/TickContext 계약** | `Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h:49-110` | `TickContext{tickIndex, fDt, pRng, ...}`, `GameCommand{kind, issuerEntity, slot, targetEntity, groundPos, direction}` |
| **AI phase 스케줄** | `Server/Private/Game/GameRoomTick.cpp:78, 87-92, 109-111` | `Phase_ServerBotAI` → drain → `Phase_ExecuteCommands`(`m_pExecutor->ExecuteCommand`) → `m_pendingExecCommands.clear()`. `CJungleAISystem::Execute(m_world, tc, m_pendingExecCommands)` 형태로 추가 |
| **ECS API** | `Engine/Public/ECS/World.h:60-177` | `CreateEntity()`, `AddComponent<T>()`, `TryGetComponent<T>()`, `ForEach<T1,T2>(fn)`. 신규 hitbox 시스템이 사용 |
| **Entity 핸들 안전성** | `Engine/Public/ECS/Entity.h:10-71` | `EntityID`(index), `EntityHandle`(generation 포함). 아레나 더미 lifetime엔 핸들 권장 |
| **결정성 RNG** | `Shared/GameSim/Core/Determinism/DeterministicRng.h:1-53` | xorshift64. `MakeSubSeed(tick, entity, skill)`로 패턴별 서브시드 |
| **Transform** | `Engine/Public/ECS/Components/TransformComponent.h:13-45` | `m_LocalPosition/Rotation`. hitbox center 계산에 사용 |
| **2D 거리** | `Engine/Include/WintersMath.h:101-106` | `DistanceSqXZ`. **3D는 없음** — 신규 구축 |
| **이벤트 cue 경로(presentation)** | `Shared/GameSim/Components/ReplicatedEventComponent.h:10-68`, `Shared/GameSim/Systems/ReplicatedEventQueue/ReplicatedEventQueue.h:6` | `EnqueueReplicatedEvent(world, evt)` → `Phase_BroadcastEvents`(`GameRoomReplication.cpp:78`). telegraph FX/anim cue를 클라로 전달 |
| **딜레이+반경 선례** | `Shared/GameSim/Systems/PendingHit/PendingHitSystem.h:18-23` | `Schedule(... fDelay, fRadius, fDamage ...)` — 딜레이드 원형 판정. **OBB/AABB overlap·active-frame window·hurtbox는 없음** |

### 2.2 미구현 (신규 구축 대상)

```text
현재 GameSim에 없는 것 (grep 확인):
- 3D AABB/OBB hitbox <-> hurtbox overlap 판정           (WintersMath는 DistanceSqXZ만)
- active-frame window 개념(타임라인 기반 on/off)         (PendingHit은 단발 delay만)
- BossBlackboard / BossHFSM / BossBT 컴포넌트·시스템     (ChampionAI만 존재)
- 격리 아레나(자족 CWorld) + 자동 N회 테스트 드라이버    (전무)
- 회피 윈도우(dodge window) 판정·로깅                    (전무)
- HitboxTimeline 데이터·TAE 이벤트 매핑                  (전무)
- BossPatternEditor / HitboxTimelineEditor 패널          (전무)
- .wboss / .whitbox 포맷 + 로더                          (전무)
```

`plan/EldenRingEditor/07`은 BossAIComponent/BossAISystem를 `CONFIRM_NEEDED`로만 남겼다 → 본 문서가 그 본문을 확정한다.

---

## 3. Winters 설계

### 3.1 포맷 스키마 (JSON 초기 → `.w*` 승격)

#### 3.1.1 `.whitbox.json` (HitboxTimeline)

active-frame window를 가진 hitbox 시퀀스. presentation이 아니라 **판정 데이터**(서버 로드).

```json
{
  "schema": "winters.eldenring.hitbox_timeline.v1",
  "clip": "c2130_atk_overhead",
  "fps": 30.0,
  "durationFrames": 60,
  "hurtboxRef": "c2130_default_hurtbox",
  "windows": [
    {
      "id": 0,
      "name": "overhead_swing",
      "shape": "OBB",
      "localCenter": [0.0, 1.2, 2.4],
      "halfExtents": [0.8, 1.0, 1.6],
      "attachBone": 0,
      "activeFrameStart": 18,
      "activeFrameEnd": 27,
      "telegraphFrameStart": 6,
      "telegraphFxId": "fx_overhead_warn",
      "damage": 320.0,
      "poiseDamage": 80.0,
      "knockback": 4.0,
      "flags": { "unblockable": false, "grab": false }
    }
  ]
}
```

핵심 필드 의미:
- `activeFrameStart/End`: 이 프레임 구간에만 overlap 판정이 켜진다. `telegraphFrameStart`: 이 프레임에 클라 텔레그래프 FX cue 발행(회피 윈도우 = telegraph~active 사이).
- `shape`: `OBB`(회전 박스) / `AABB`(축정렬) / `Sphere`(반경). 초기 OBB+Sphere만.
- `attachBone`: bone 인덱스(skeletal). animation baking 전엔 0(루트)로 static proxy.

#### 3.1.2 `.wboss.json` (BossPattern)

phase graph + action 목록 + telegraph/hitbox 연결.

```json
{
  "schema": "winters.eldenring.boss.v1",
  "bossId": "c2130_margit",
  "blackboard": {
    "scanRange": 32.0, "leashRange": 42.0,
    "meleeRange": 4.5, "midRange": 12.0,
    "phase2HpRatio": 0.55
  },
  "phases": [
    { "id": 0, "name": "Phase1", "enterHpRatio": 1.00, "actions": [0, 1, 2] },
    { "id": 1, "name": "Phase2", "enterHpRatio": 0.55, "actions": [0, 1, 3, 4] }
  ],
  "actions": [
    {
      "id": 0, "name": "overhead",
      "minRange": 0.0, "maxRange": 4.5,
      "cooldownSec": 1.2, "weight": 1.0,
      "hitboxTimeline": "c2130_atk_overhead",
      "actionStateId": 100, "recoverSec": 0.75
    }
  ],
  "test": {
    "seed": 1337, "repeatCount": 50,
    "dummyKind": "PlayerDodgeBot",
    "dodgeReactionFrames": 9
  }
}
```

#### 3.1.3 `.w*` 승격

JSON 안정화 후 `.whitbox`/`.wboss` 바이너리 승격. 둘 다 **`CAssetStreamingSystem` 핸들로만 로드**(에디터 우회 금지). 17문서 5절 전략: JSON→binary→bundle.

### 3.2 런타임 클래스 계층 (C++ 시그니처)

```text
Engine/Public/Physics3D/                 ← 신규: 엔진 공용 3D overlap 수학(presentation/truth 공용)
  HitVolume.h            HitVolume{ shape, center, halfExtents, rotationYaw }, Overlap helpers
Shared/GameSim/Components/
  BossBlackboardComponent.h   blackboard 키
  BossHFSMComponent.h         HFSM state + cooldown
  HitboxComponent.h           활성 hitbox 인스턴스(서버 판정)
  HurtboxComponent.h          더미/플레이어 피격 볼륨
  HitboxTimelineComponent.h   현재 재생 중 timeline + frame cursor
  BossPatternTuningComponent.h  default+runtime override(ChampionAITuning 모사)
  BossAIDebugComponent.h      replicated debug snapshot
Shared/GameSim/Systems/BossAI/
  BossAISystem.{h,cpp}        blackboard 갱신 + HFSM/BT → GameCommand 후보
  BossBehaviorTree.{h,cpp}    deterministic BT(Selector/Sequence/Task/Decorator)
Shared/GameSim/Systems/Hitbox/
  HitboxTimelineSystem.{h,cpp} frame 진행 → active window on/off → HitboxComponent
  HitboxOverlapSystem.{h,cpp}  hitbox×hurtbox overlap → DamageRequest + 회피/명중 로그
Shared/GameSim/Testing/
  BossArena.{h,cpp}           격리 CWorld 1개 + 더미 스폰
  BossPatternTestDriver.{h,cpp} 시드 고정 N회 재생 + 결과 집계
EldenRingEditor/Panels/
  BossPatternEditorPanel.{h,cpp}
  HitboxTimelineEditorPanel.{h,cpp}
  BossDecisionDebugOverlay.{h,cpp}
```

핵심 시그니처(요약, 전문은 6절):

```cpp
// Engine/Public/Physics3D/HitVolume.h  — presentation/truth 공용, DX 타입 노출 없음
enum class eHitShape : u8_t { AABB, OBB, Sphere };
struct HitVolume { eHitShape shape; Vec3 center; Vec3 halfExtents; f32_t rotationYaw; };
namespace WintersPhysics3D {
    bool Overlap(const HitVolume& a, const HitVolume& b);   // SAT(OBB) / dist(Sphere)
}

// Shared/GameSim/Systems/BossAI/BossAISystem.h  — ChampionAISystem 시그니처와 동일
class CBossAISystem final {
public:
    static void Execute(CWorld& world, const TickContext& tc,
        std::vector<GameCommand>& outCommands);   // 후보만 발행
private: CBossAISystem() = delete;
};

// Shared/GameSim/Systems/Hitbox/HitboxOverlapSystem.h  — 서버 권위 판정
class CHitboxOverlapSystem final {
public:
    static void Execute(CWorld& world, const TickContext& tc,
        BossPatternTestLog* pOptionalLog = nullptr);  // 아레나일 때만 로그
private: CHitboxOverlapSystem() = delete;
};

// Shared/GameSim/Testing/BossPatternTestDriver.h
class CBossPatternTestDriver {
public:
    BossPatternTestResult Run(const BossPatternAsset& boss,
        const HitboxTimelineAsset& timeline, u64_t seed, u32_t repeatCount);
};
```

### 3.3 에디터 패널 (ImGui)

> 게이트 원칙: 패널을 크게 벌리기 전에 런타임 contract(아레나 N회 재생 + 로그)가 먼저 동작해야 한다(17문서 2절).

**BossPatternEditorPanel**
1. Phase 그래프(노드=phase, 엣지=hp ratio 전이). drag로 전이 편집.
2. Action 리스트: `minRange/maxRange/cooldown/weight` 슬라이더.
3. Telegraph FX 드롭다운(`.wfx` id 참조).
4. HitboxTimeline 연결 드롭다운(`.whitbox` 참조).
5. `Run Test` 버튼 → `CBossPatternTestDriver::Run` → 결과 테이블(아래 overlay와 연동).
6. Save/Load `.wboss.json`.

**HitboxTimelineEditorPanel**
1. clip 선택 + frame scrub 슬라이더.
2. window 추가/삭제(OBB/Sphere), `localCenter/halfExtents` 편집.
3. `activeFrameStart/End` 막대 + `telegraphFrameStart` 마커를 타임라인에 표시.
4. hurtbox display 토글, active frame 하이라이트.
5. `damage/poiseDamage/knockback/unblockable/grab` 편집.
6. Save/Load `.whitbox.json`.

**BossDecisionDebugOverlay** (AI Debugger 대응)
- blackboard 값(target/distance/phase/cooldown), 현재 HFSM state, 직전 BT leaf 결과·실패 사유, 최근 결정 ring buffer.
- 아레나 테스트 시: 각 회차 hitbox active 구간, 더미 회피 성공/실패, telegraph→active 회피 윈도우, hit/miss 카운트.

---

## 4. 데이터 흐름 (presentation/truth 경계)

```text
[에디터: 편집]
BossPatternEditor ──► .wboss.json
HitboxTimelineEditor ─► .whitbox.json
        │
        ▼ (에디터 우회 금지)
CAssetStreamingSystem (handle/state, async)
        │
        ├──────────────► [서버 권위: TRUTH] Shared/GameSim
        │                  CBossAISystem::Execute → GameCommand 후보
        │                  CommandExecutor → ActionState 진입
        │                  CHitboxTimelineSystem → active window on/off
        │                  CHitboxOverlapSystem → overlap → DamageRequest (판정)
        │                  CPhaseSystem → hp ratio 전이 (서버만)
        │                       │
        │                       ▼ EnqueueReplicatedEvent
        │                  Phase_BroadcastEvents ─► Snapshot/Event/FX Cue
        │
        └──────────────► [격리 테스트: TRUTH 복제]
                           BossArena(독립 CWorld) + Dummy
                           BossPatternTestDriver(seed 고정, N회)
                           → BossPatternTestLog (hit/miss/timing/dodge)
                                       │
                                       ▼
[클라이언트: PRESENTATION 재생만]
  telegraph FX / anim / UI / camera shake  ← FX Cue 수신
  BossDecisionDebugOverlay (debug snapshot 표시만)
```

**불변식**:
- damage·overlap·phase 전이는 **서버 또는 아레나(서버 룰 복제) 안에서만** 발생. 클라는 절대 판정하지 않는다(05문서, 12문서 Phase I).
- 회피 윈도우 = `telegraphFrameStart` cue 발행 시점 ~ `activeFrameStart`. 더미의 회피 입력이 이 구간 안이면 hit 회피.
- 아레나는 동일 `CWorld`/시스템을 쓰되 네트워크·렌더 없이 독립 실행 → 같은 시드면 같은 결과(결정성).

---

## 5. 구현 순서 (단계별 + 완료기준 + 게이트)

선행: 17문서 4절에 따라 ED-11(Hitbox timeline)·ED-12(Boss tool)는 ladder 후반. 단 **3D overlap 수학과 BossAI 후보 발행은 RHI/렌더에 의존하지 않으므로** static proxy/headless로 G2 이전에도 검증 가능(07 세션 게이트 노트와 동일 전략).

| 단계 | 내용 | 완료 기준 | 게이트 |
|---|---|---|---|
| **S0** | `Engine/Public/Physics3D/HitVolume.h` + `WintersPhysics3D::Overlap`(OBB SAT / Sphere) | AABB/OBB/Sphere overlap 단위테스트 통과(겹침/비겹침/회전 케이스) | 시스템 게이트 HB0 (3D overlap 존재) |
| **S1** | `HitboxComponent`/`HurtboxComponent` + `CHitboxOverlapSystem`: 정적 박스 1쌍 overlap → `DamageRequest` enqueue | 두 더미 박스가 겹칠 때만 damage 1회 발생(headless) | HB1 (overlap→판정) |
| **S2** | `HitboxTimelineComponent` + `CHitboxTimelineSystem`: frame cursor 진행 → `activeFrameStart..End`에서만 hitbox on | active window 밖 프레임은 overlap 0, 안은 1회. JSON `.whitbox` 로드 | HB2 (active-frame window) |
| **S3** | `BossBlackboardComponent`/`BossHFSMComponent`/`BossPatternTuningComponent` + `CBossAISystem::Execute` → GameCommand 후보(`07` 본문 확정) | 더미 보스가 거리/쿨다운/phase에 따라 action GameCommand만 발행(world mutate 0) | 12문서 G9 (서버 권위 후보) |
| **S4** | `BossBehaviorTree`(Selector/Sequence/Task/Decorator) deterministic 평가, leaf 실패 사유 분리 | BT가 cooldown/range/no-target 사유로 leaf 분기, ring buffer trace 기록 | G9 |
| **S5** | `BossArena`(독립 `CWorld`+더미) + `CBossPatternTestDriver::Run(seed, N)`: 패턴 1개 N회 재생, hit/miss/timing 집계 | 같은 seed로 2회 실행 시 byte-identical 결과(결정성), `BossPatternTestLog` 산출 | 시스템 게이트 BT-TEST (Gauntlet식 반복) |
| **S6** | 회피 더미(`PlayerDodgeBot`): telegraph cue~active 사이 회피 입력, 회피 윈도우 hit/miss 로깅 | `dodgeReactionFrames`보다 telegraph 길면 회피 성공, 짧으면 hit. 통계 출력 | BT-TEST |
| **S7** | `BossDecisionDebugOverlay`(ImGui): blackboard/HFSM/BT/hitbox 시각화 + 아레나 결과 테이블 | 결정 사유·active 구간·회피 결과가 화면에 보임 | 12문서 ED-12 |
| **S8** | `HitboxTimelineEditorPanel`: scrub + window 편집 → 저장 → S5 재생 반영 | 에디터에서 window 옮기면 다음 Run에서 타이밍 변화 | ED-11 |
| **S9** | `BossPatternEditorPanel`: phase/action/telegraph/hitbox 연결 + `Run Test` | 편집→저장→런타임 preview(아레나) 왕복 | ED-12 |
| **S10** | 서버 통합: `GameRoomBossAI.cpp`/`Phase_ServerBossAI` 연결, telegraph cue를 `ReplicatedEvent`로 클라 전달 | F5 서버 tick에서 보스가 후보 발행, 클라가 FX cue만 재생 | G9 |

**게이트 막힘 대응**: HB0(overlap 수학)이 막히면 S1 이후 전부 중단하고 SAT 구현부터. S5(결정성)가 깨지면 부동소수/RNG 순서·컨테이너 순회 순서를 먼저 의심(`DeterministicEntityIterator` 사용 강제). c2130 animation baking이 막혀도 S0~S6은 static proxy/frame-only로 진행(07 세션 게이트 노트와 동일).

---

## 6. 코드 스켈레톤 (Winters 타입, 컴파일 가능 형태)

### 6.1 `Engine/Public/Physics3D/HitVolume.h` (신규, presentation/truth 공용)

```cpp
#pragma once
#include "WintersTypes.h"
#include "WintersMath.h"

enum class eHitShape : u8_t { AABB = 0, OBB = 1, Sphere = 2 };

// 저장용 POD. center/halfExtents는 world 기준. OBB는 yaw 회전만(보스 패턴 충분).
struct HitVolume
{
    eHitShape shape = eHitShape::AABB;
    Vec3      center{ 0.f, 0.f, 0.f };
    Vec3      halfExtents{ 0.5f, 0.5f, 0.5f }; // Sphere는 x를 반지름으로 사용
    f32_t     rotationYaw = 0.f;               // radians, OBB 전용
};

namespace WintersPhysics3D
{
    // SAT 기반 3D overlap. 결정성: 모든 연산은 f32_t 고정 순서.
    WINTERS_ENGINE bool Overlap(const HitVolume& a, const HitVolume& b);
    WINTERS_ENGINE bool OverlapAABB(const Vec3& aMin, const Vec3& aMax,
                                    const Vec3& bMin, const Vec3& bMax);
    WINTERS_ENGINE bool OverlapSphere(const Vec3& ca, f32_t ra,
                                      const Vec3& cb, f32_t rb);
    WINTERS_ENGINE bool OverlapOBB(const HitVolume& a, const HitVolume& b); // yaw SAT
}
```

### 6.2 `Shared/GameSim/Components/HitboxComponent.h` / `HurtboxComponent.h` (신규)

```cpp
#pragma once
#include "Engine/Public/Physics3D/HitVolume.h"
#include "WintersTypes.h"
#include "ECS/Entity.h"
#include <type_traits>

// 서버 권위: 현재 active한 hitbox 한 개. 비활성 프레임엔 bActive=false.
struct HitboxComponent
{
    EntityID owner = NULL_ENTITY;     // 보스 entity
    u8_t     team = 0;                // ownerTeam (피아 구분)
    u16_t    windowId = 0;            // .whitbox window id
    HitVolume volume{};               // world 기준(timeline이 매 tick 갱신)
    f32_t    damage = 0.f;
    f32_t    poiseDamage = 0.f;
    f32_t    knockback = 0.f;
    bool_t   bActive = false;         // activeFrame 구간에서만 true
    bool_t   bUnblockable = false;
    bool_t   bGrab = false;
    u32_t    hitMask = 0;             // 이미 맞춘 target 중복 방지(per-window)
};

struct HurtboxComponent
{
    EntityID owner = NULL_ENTITY;
    u8_t     team = 0;
    HitVolume volume{};               // world 기준
    bool_t   bInvulnerable = false;   // 회피(i-frame) 중이면 true
};

static_assert(std::is_trivially_copyable_v<HitboxComponent>, "POD for determinism");
static_assert(std::is_trivially_copyable_v<HurtboxComponent>, "POD for determinism");
```

### 6.3 `Shared/GameSim/Components/HitboxTimelineComponent.h` (신규)

```cpp
#pragma once
#include "Engine/Public/Physics3D/HitVolume.h"
#include "WintersTypes.h"
#include "ECS/Entity.h"

inline constexpr u8_t kMaxHitboxWindows = 8u;

struct HitboxWindowDef
{
    u16_t  windowId = 0;
    HitVolume localVolume{};          // owner local space
    u16_t  attachBone = 0;
    u16_t  activeFrameStart = 0;
    u16_t  activeFrameEnd = 0;
    u16_t  telegraphFrameStart = 0;
    u32_t  telegraphFxId = 0;
    f32_t  damage = 0.f;
    f32_t  poiseDamage = 0.f;
    f32_t  knockback = 0.f;
    bool_t bUnblockable = false;
    bool_t bGrab = false;
};

// 재생 중인 timeline 상태(서버). frame은 결정성 위해 tick 누적으로 계산.
struct HitboxTimelineComponent
{
    f32_t  fps = 30.f;
    u16_t  durationFrames = 0;
    u16_t  currentFrame = 0;
    f32_t  frameAccumSec = 0.f;       // dt 누적 → frame 진행
    u8_t   windowCount = 0;
    bool_t bPlaying = false;
    bool_t bTelegraphSentMask[kMaxHitboxWindows] = {}; // 중복 cue 방지
    HitboxWindowDef windows[kMaxHitboxWindows] = {};
};
```

### 6.4 `Shared/GameSim/Components/BossBlackboardComponent.h` / `BossHFSMComponent.h` (신규, `07` 확정)

```cpp
#pragma once
#include "WintersTypes.h"
#include "WintersMath.h"
#include "ECS/Entity.h"

enum class eBossKind : u8_t { GenericMob = 0, TreeGuard = 1, Margit = 2 };

enum class eBossHFSMState : u8_t
{
    Idle, Patrol, Engage, Combo, Recover, Retreat, Staggered, Dead,
};

enum class eBossBTBlockReason : u8_t
{
    None, NoTarget, OutOfRange, Cooldown, ActionLocked, PhaseLocked, Staggered,
};

// AI Debugger ring buffer 대응(ChampionAIDecisionTraceEntry 모사)
inline constexpr u8_t kBossDecisionTraceCapacity = 16u;
struct BossDecisionTraceEntry
{
    u64_t tick = 0;
    eBossHFSMState state = eBossHFSMState::Idle;
    u16_t selectedActionId = 0xFFFF;
    eBossBTBlockReason blockReason = eBossBTBlockReason::None;
    f32_t distance = 999.f;
    f32_t hpRatio = 1.f;
    u8_t  phase = 0;
};

// Blackboard: 결정 상태를 노드 바깥에 외재화(UE5 철학)
struct BossBlackboardComponent
{
    eBossKind kind = eBossKind::GenericMob;
    EntityID  target = NULL_ENTITY;
    f32_t     distanceToTarget = 999.f;
    f32_t     hpRatio = 1.f;
    u8_t      phase = 0;
    f32_t     staggerValue = 0.f;
    Vec3      arenaAnchor{ 0.f, 0.f, 0.f };
    u16_t     lastActionId = 0xFFFF;
    f32_t     actionCooldownSec[8] = {};   // action별 남은 쿨다운
};

struct BossHFSMComponent
{
    eBossHFSMState state = eBossHFSMState::Idle;
    f32_t          stateTimer = 0.f;
    f32_t          actionLockSec = 0.f;    // 현재 action 회복까지
    u16_t          activeActionId = 0xFFFF;
    eBossBTBlockReason lastBlockReason = eBossBTBlockReason::None;
    BossDecisionTraceEntry trace[kBossDecisionTraceCapacity] = {};
    u8_t           traceHead = 0;
    u8_t           traceCount = 0;
    u32_t          nextCommandSequence = 1;
};
```

### 6.5 `Shared/GameSim/Systems/BossAI/BossAISystem.h` (신규)

```cpp
#pragma once
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include <vector>

class CWorld;

// ChampionAISystem과 동일 계약: gameplay 직접 mutate 금지, 후보만 발행.
class CBossAISystem final
{
public:
    static void Execute(CWorld& world,
        const TickContext& tc,
        std::vector<GameCommand>& outCommands);

private:
    CBossAISystem() = delete;
};
```

### 6.6 `Shared/GameSim/Systems/Hitbox/HitboxTimelineSystem.h` / `HitboxOverlapSystem.h` (신규)

```cpp
// HitboxTimelineSystem.h
#pragma once
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
class CWorld;

// frame cursor 진행 → activeFrame 구간에서 HitboxComponent를 world 좌표로 갱신/활성.
// telegraphFrameStart 도달 시 ReplicatedEvent(FX cue) enqueue (presentation).
class CHitboxTimelineSystem final
{
public:
    static void Execute(CWorld& world, const TickContext& tc);
private: CHitboxTimelineSystem() = delete;
};

// HitboxOverlapSystem.h
#pragma once
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
class CWorld;
struct BossPatternTestLog; // 아레나 전용

// 서버 권위: active hitbox × hurtbox overlap → DamageRequest enqueue.
// pLog가 있으면(아레나) hit/miss/회피윈도우를 기록.
class CHitboxOverlapSystem final
{
public:
    static void Execute(CWorld& world, const TickContext& tc,
        BossPatternTestLog* pLog = nullptr);
private: CHitboxOverlapSystem() = delete;
};
```

### 6.7 `Shared/GameSim/Testing/BossPatternTestDriver.h` (신규, Gauntlet 대응)

```cpp
#pragma once
#include "WintersTypes.h"
#include <vector>

struct BossPatternAsset;      // .wboss 파싱 결과
struct HitboxTimelineAsset;   // .whitbox 파싱 결과

struct BossPatternRunStat
{
    u32_t runIndex = 0;
    u16_t firstActiveFrame = 0;
    u16_t telegraphFrame = 0;
    u16_t dodgeWindowFrames = 0;   // telegraph~active
    bool_t bDodged = false;
    bool_t bHit = false;
    f32_t damageDealt = 0.f;
    u64_t finalRngState = 0;       // 결정성 검증용
};

struct BossPatternTestLog
{
    std::vector<BossPatternRunStat> runs;
    u32_t hitCount = 0;
    u32_t dodgeCount = 0;
    f32_t avgDodgeWindowFrames = 0.f;
};

struct BossPatternTestResult
{
    BossPatternTestLog log;
    bool_t bDeterministic = false; // 동일 seed 재실행 일치 여부
};

// 격리 아레나에서 패턴 N회 재생. 같은 seed면 같은 결과(결정성 보장).
class CBossPatternTestDriver
{
public:
    BossPatternTestResult Run(const BossPatternAsset& boss,
        const HitboxTimelineAsset& timeline,
        u64_t seed, u32_t repeatCount);
};
```

### 6.8 `Shared/GameSim/Testing/BossArena.h` (신규)

```cpp
#pragma once
#include "Shared/GameSim/Core/World/World.h"
#include "Shared/GameSim/Core/Determinism/DeterministicRng.h"
#include "ECS/Entity.h"

enum class eArenaDummyKind : u8_t { StaticTarget = 0, PlayerDodgeBot = 1 };

// 네트워크·렌더 없는 독립 CWorld. 서버 시스템 룰만 복제.
class CBossArena
{
public:
    void Setup(eBossKind bossKind, eArenaDummyKind dummyKind, u64_t seed);
    void Reset(u64_t seed);             // 같은 seed로 재현
    void Step(u32_t maxTicks);          // 패턴 1회 끝까지 tick
    SharedSim::World& World() { return m_world; }
    EntityID Boss()  const { return m_boss; }
    EntityID Dummy() const { return m_dummy; }

private:
    SharedSim::World    m_world;
    DeterministicRng    m_rng{ 1u };
    EntityID            m_boss = NULL_ENTITY;
    EntityID            m_dummy = NULL_ENTITY;
};
```

### 6.9 서버 통합 (기존 파일 anchor)

`Server/Public/Game/GameRoom.h` — 기존 `void Phase_ServerBotAI(TickContext& tc);` 아래 추가:
```cpp
    void Phase_ServerBossAI(TickContext& tc);
```

`Server/Private/Game/GameRoomTick.cpp:78` — 기존 `Phase_ServerBotAI(tc);` 아래 추가(drain 전):
```cpp
    Phase_ServerBossAI(tc);
```

`GameRoomTick.cpp` `Phase_SimulationSystems`의 `CAttackChaseSystem::Execute(...)` 다음에 추가:
```cpp
    CHitboxTimelineSystem::Execute(m_world, tc);
    CHitboxOverlapSystem::Execute(m_world, tc); // 서버: 로그 없이 판정만
```

`Server/Private/Game/GameRoomBossAI.cpp`(신규):
```cpp
#include "Game/GameRoom.h"
#include "Shared/GameSim/Systems/BossAI/BossAISystem.h"

void CGameRoom::Phase_ServerBossAI(TickContext& tc)
{
    CBossAISystem::Execute(m_world, tc, m_pendingExecCommands);
}
```

---

## 7. 검증·리스크

### 7.1 빌드 타겟별 MSBuild

```powershell
# Engine(HitVolume 공용 수학 추가 — public header)
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' Winters.sln /t:Engine /m /p:Configuration=Debug /p:Platform=x64 /v:minimal
# GameSim/Server(BossAI/Hitbox/Testing)
& '...MSBuild.exe' Winters.sln /t:Server /m /p:Configuration=Debug /p:Platform=x64 /v:minimal
# Client(debug overlay, FX cue 재생)
& '...MSBuild.exe' Winters.sln /t:Client /m /p:Configuration=Debug /p:Platform=x64 /v:minimal   # LoL visual smoke 유지
# Editor 패널
& '...MSBuild.exe' Winters.sln /t:EldenRingEditor /m /p:Configuration=Debug-DX12 /p:Platform=x64 /v:minimal
git diff --check
```
Engine public header(`HitVolume.h`) 추가 시 `UpdateLib.bat`/SDK sync 확인(`EngineSDK\inc`에 반영).

### 7.2 게이트 막힐 때 대응

| 막힘 | 대응 |
|---|---|
| HB0 overlap 수학(OBB SAT) | yaw-only OBB로 축소, 안 되면 AABB+Sphere만으로 S1 진행 |
| S5 결정성 깨짐 | `ForEach` 순회 순서 비결정 의심 → `DeterministicEntityIterator` 강제, RNG는 `tc.pRng`만 사용, f32 누적 순서 고정 |
| c2130 anim baking 미완 | `attachBone=0` static proxy frame-only로 S0~S9 진행(07 게이트 노트) |
| 서버 권위 위반 유혹 | 클라에서 overlap 호출 금지 — overlap 시스템은 Server/Arena `CWorld`에서만 Execute |

### 7.3 과설계 방지 (Karpathy 가드레일)

- EQS·AI Perception·Ribbon hitbox는 **초기 범위 제외**. 거리 룰 + OBB/Sphere만.
- OBB는 **yaw 회전만**(full quaternion SAT 금지). 보스 지상 패턴에 충분.
- BT 노드는 Selector/Sequence/Task/Decorator 4종만. Parallel/Service는 후순위.
- 아레나는 **CWorld 1개 + 더미 1~2개**부터. 멀티 더미·다보스는 통계가 필요해질 때만.
- `.wboss/.whitbox` binary 승격은 JSON 안정화 후. 처음부터 binary 금지.

---

## 8. Codex 요구사항 프롬프트 (복붙용)

```text
SYSTEM=BOSS_TESTING

너는 Winters 엔진에 UE5급 Boss Pattern Testing Environment를 구축하는 시니어 엔진/AI 엔지니어다.
목표: 격리 아레나에서 보스 패턴을 시드 고정 N회 자동 재생하며 hitbox active window 타이밍·회피 윈도우·
텔레그래프 hit/miss를 로깅·시각화하는 자동 테스트 루프 + BossPatternEditor + HitboxTimelineEditor를
UE5(Behavior Tree/Blackboard/Gauntlet/AI Debugger)를 reference로만 삼아 Winters .w* contract로 구현한다.

[절대 원칙 — 위반 시 작업 무효]
1. UE5는 reference depot(개념·UX·책임분리)일 뿐. UE 코드 복사/모듈 링크/object model 이식 금지.
2. "에디터 화면 먼저 크게" 금지. runtime contract(아레나 N회 재생+로그)를 먼저 증명 → 에디터가 편집.
   모든 패널 완료기준 = ".whitbox/.wboss JSON seed → 아레나 런타임 preview"가 실제로 보이는 것.
3. hitbox 판정·damage·phase 전이는 서버 권위(Shared/GameSim 또는 아레나 CWorld). BossBT/HFSM 결정은
   GameCommand 후보로만 발행. 클라는 anim/telegraph FX/UI/camera shake 재생만. presentation/truth 분리.
4. 에디터 데이터(.wboss/.whitbox)는 CAssetStreamingSystem 거쳐 런타임 로드. 우회 금지.
5. Engine→Client 의존 역전 금지. Client/Public·Shared에 DX11/DX12 concrete type 노출 금지.
   normal F5 LoL runtime 우회·은폐 금지(LoL DX11 visual smoke 유지).

[환경]
- 저장소: C:/Users/tnest/Desktop/Winters
- ECS: Engine/Public/ECS/World.h (CWorld: AddComponent<T>/TryGetComponent<T>/ForEach<T1,T2>)
       Engine/Public/ECS/Entity.h (EntityID, EntityHandle)
- AI 선례: Shared/GameSim/Systems/ChampionAI/ChampionAISystem.{h,cpp}
           (Execute(world, tc, outCommands) → GameCommand 후보만; MakeAICommand; ring buffer trace)
           Shared/GameSim/Components/ChampionAIComponent.h (blackboard/tuning/blockReason/trace 패턴)
- 계약: Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h (TickContext, GameCommand, eCommandKind)
- 스케줄: Server/Private/Game/GameRoomTick.cpp:78,87-92,109-111 (Phase_ServerBotAI → drain → Execute)
- 결정성 RNG: Shared/GameSim/Core/Determinism/DeterministicRng.h (tc.pRng)
- 이벤트 cue: Shared/GameSim/Systems/ReplicatedEventQueue/ReplicatedEventQueue.h (EnqueueReplicatedEvent)
              Server/Private/Game/GameRoomReplication.cpp:78 (Phase_BroadcastEvents)
- 수학: Engine/Include/WintersMath.h (Vec3, DistanceSqXZ 만 존재 — 3D overlap 없음 → 신규 구축)
- 딜레이+반경 선례: Shared/GameSim/Systems/PendingHit/PendingHitSystem.h (active-frame window 없음)

[먼저 읽을 문서 — 순서대로]
1. .md/EldenRing/18_BOSS_PATTERN_TESTING_ENVIRONMENT.md  ← 이 설계(전체)
2. .md/EldenRing/17_UE5_GRADE_EDITOR_SUITE_MASTER.md     ← 5대 시스템 매핑·순서·게이트
3. .md/EldenRing/12_UE5_REFERENCE_DX12_RHI_EDITOR_BIG_PICTURE.md ← Phase A~J·G0~G9
4. .md/EldenRing/06_FX_GRAPH_SEQUENCER_EDITOR.md (Boss/Hitbox 섹션)
5. .md/plan/EldenRingEditor/07_BOSS_BLACKBOARD_HFSM_BT_TUNING.md (이 문서가 본문 확정)
6. .md/EldenRing/05_NETWORK_PVP_COOP_RAID_SERVER_AUTH.md (서버 권위), 13(TAE 이벤트)
7. CLAUDE.md, .claude/gotchas.md

[작업 범위 — S0~S10 (문서 5절)]
- S0: Engine/Public/Physics3D/HitVolume.h + WintersPhysics3D::Overlap(OBB SAT yaw / Sphere). 단위테스트.
- S1: HitboxComponent/HurtboxComponent + CHitboxOverlapSystem (overlap→DamageRequest).
- S2: HitboxTimelineComponent + CHitboxTimelineSystem (frame cursor → activeFrame window on/off). .whitbox 로드.
- S3: BossBlackboard/BossHFSM/BossPatternTuning + CBossAISystem::Execute → GameCommand 후보(07 본문 확정).
- S4: BossBehaviorTree(Selector/Sequence/Task/Decorator) deterministic, leaf 실패 사유 분리.
- S5: BossArena(독립 CWorld+더미) + CBossPatternTestDriver::Run(seed, N). 결정성(동일 seed 동일 결과).
- S6: PlayerDodgeBot 회피 윈도우(telegraph~active) hit/miss 로깅.
- S7: BossDecisionDebugOverlay(blackboard/HFSM/BT/hitbox + 아레나 결과).
- S8: HitboxTimelineEditorPanel (scrub + window 편집 → 재생 반영).
- S9: BossPatternEditorPanel (phase/action/telegraph/hitbox 연결 + Run Test).
- S10: 서버 통합 — Phase_ServerBossAI/GameRoomBossAI.cpp 연결, telegraph cue → ReplicatedEvent.

[작업 루프]
1. 선행 게이트 확인: 내 시스템은 3D overlap 수학(HB0)과 BossAI 후보 발행이 RHI에 독립 → static proxy로 G2 이전도 진행 가능.
   단 anim baking 의존(attachBone)은 0=루트 proxy로 우회.
2. runtime contract 먼저: HitVolume→overlap→active window→아레나 N회 재생+로그를 최소로 증명.
3. 에디터가 contract 편집: 패널 추가. 완료기준 = "편집→저장→아레나 preview" 왕복.
4. presentation/truth: overlap/damage/phase는 서버·아레나 CWorld에서만. 클라는 cue 재생만.
5. 결정성: tc.pRng만 사용, ForEach 순회 비결정 의심 시 DeterministicEntityIterator, f32 누적 순서 고정.
6. 막히면 사유 분류 보고(서버 권위/의존 역전/게이트/결정성). 나머지는 계속.

[빌드 검증]
- Engine: /t:Engine /p:Configuration=Debug /p:Platform=x64   (HitVolume public header → UpdateLib/SDK sync 확인)
- Server: /t:Server /p:Configuration=Debug /p:Platform=x64
- Client: /t:Client /p:Configuration=Debug /p:Platform=x64    (LoL visual smoke 유지)
- Editor: /t:EldenRingEditor /p:Configuration=Debug-DX12 /p:Platform=x64
- git diff --check

[완료 기준]
- 격리 아레나에서 보스 패턴이 시드 고정 N회 자동 재생된다.
- hitbox active window 타이밍과 더미 회피 윈도우 hit/miss가 로깅된다.
- 같은 seed 재실행 시 byte-identical 결과(결정성).
- hitbox 판정·damage·phase 전이가 서버/아레나 권위로만 발생(클라 판정 0).
- 에디터에서 window/action 편집 → 저장 → 다음 Run에 타이밍 반영.
- BossDecisionDebugOverlay에 blackboard/HFSM/BT 사유와 회피 결과가 보인다.

[금지사항]
- UE5 BT/Blackboard/Gauntlet 코드 복사·이식.
- 클라이언트에서 overlap/damage 판정.
- BossBT/HFSM가 GameCommand 외 gameplay 직접 mutate.
- EQS/AI Perception/full-quaternion OBB/Parallel·Service 노드(초기 범위 외) 선구현.
- .wboss/.whitbox 처음부터 binary화(JSON 먼저).
- Engine public header에 DX11/DX12 concrete type 노출.

[시작]
지금: (1) 위 문서·코드를 읽고, (2) HB0(HitVolume overlap) 선행 충족 여부와 현재 GameSim 상태를 집계해 보고,
(3) S0 runtime contract(overlap 수학)부터 구현하라. 막히면 사유 분류해 보고하고 나머지는 계속 진행하라.
```
