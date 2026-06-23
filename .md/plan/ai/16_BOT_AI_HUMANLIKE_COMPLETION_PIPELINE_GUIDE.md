# Bot AI 완성 파이프라인 가이드 — 사람처럼 판단/선택/교전/라인전/운영하는 봇

작성일: 2026-06-23
대상: Winters LoL 서버 권위 봇 AI
성격: **코드 수정 전** 방향/튜닝·디버깅/검증 파이프라인 가이드 (계획서 아님, 계획서는 본 가이드 승인 후 Phase별로 분해)

---

## 0. 이 문서가 정하는 것 / 정하지 않는 것

정하는 것:
- 현재 코드 위에서 "사람처럼 상황판단 → 선택 → 스킬 → 라인전 → 운영"하는 봇으로 가는 **설계 방향**.
- 그 작업을 굴리는 **튜닝/디버깅 인프라**와 **인게임 튜닝 + 검증 파이프라인**.
- 어디에 무엇을 끼우는가(seam), 무엇을 어기면 안 되는가(북극성 제약), 무엇을 어떤 순서로 검증하는가.

정하지 않는 것(다음 단계):
- 줄 단위 코드 변경 지시(= 계획서). 본 가이드 승인 후 `.md/계획서작성규칙.md` 형식으로 Phase별 분해.

### 0.1 가장 중요한 발견 — "처음부터 만들기"가 아니라 "완성하기"

봇 AI는 이미 **Shared/GameSim에 결정론·command-only로 구현된 Utility/HFSM 하이브리드**다. perception(컨텍스트 수집) → score → 상태머신 → intent(brain) → GameCommand 방출이 동작하며, **런타임 튜닝(14노브) + 결정 트레이스 + 인게임 디버그 패널 + 디버그 명령 왕복**까지 상당 부분 깔려 있다. 따라서 이 가이드는 **빈 캔버스 로드맵이 아니라, 실존 구조의 빈칸(운영 부재, Decision brain 미연결, 난이도 미반영, 전지적 perception, 스킬샷 리드 없음)을 채우는 완성 작업**을 다룬다.

### 0.2 두 개의 트랙을 구분하라

- **희망(aspirational) 트랙**: `.md/plan/ai/`의 Stage 0~8(Aggro/HFSM/BT/GOAP/Utility/InfluenceMap/MCTS/Imitation/RL), `01_ARCHITECTURE.md`의 `Engine/Public/AI` 디렉토리 트리. → **대부분 미구현 설계 스케치.** 디렉토리·타입·이름이 실제 코드와 다르다. 청사진/어휘로만 인용한다.
- **실현(realized) 트랙**: `.md/plan/Champion/`(26/28/29/30), `.md/TODO/05-1x~05-21/`, 그리고 실제 코드 `Shared/GameSim/Systems/ChampionAI/`. → **이 가이드의 북극성.** 신규 작업은 여기에 정렬한다.

### 0.3 Stale 경고 (그대로 인용 금지)

- `.md/plan/ai/codex/BOT_AI_ROSTER_ADD_PIPELINE.md` 등의 심볼 `BotLaneAISystem` / `BotLaneAIPolicy` / `EnsureBotAIStage1SmokeRoster`는 **현 코드에 없다.** 실제는 `CChampionAISystem` / `ChampionAIPolicy`.
- 옛 문서의 경로 `C:/Users/user/...`, `Winters_restored`는 현행 `C:/Users/tnest/Desktop/Winters`와 불일치.
- 이 가이드의 line 번호는 작성 시점 근사치다. 코드 드리프트 시 함수명으로 재확인한다.

---

## 1. 북극성 — 절대 어기지 않는 하드 제약 (작업 전 체크리스트)

1. **봇은 truth를 직접 수정하지 않는다.** Transform/HP/cooldown/SkillState는 손대지 않고 `GameCommand`만 생산한다. 생산은 `Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp`의 `MakeAICommand`/`Emit*Command` → `outCommands`, 적용은 `CDefaultCommandExecutor::ExecuteCommand`(`Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`)가 권위로 한다. 미니언/터렛은 truth를 직접 고치지만 **챔피언 봇은 가짜 플레이어 입력 모델**이다 — 미니언 코드를 복붙해 truth를 건드리면 권위·결정성 파괴.
2. **의사결정 본체는 Shared/GameSim에 둔다.** Shared/GameSim은 Engine 렌더러/런타임/Client/UI/ImGui/DX11을 include하지 않는다. ECS 프리미티브·저수준 타입은 **EngineSDK/inc 공개 헤더로만** 소비한다(`Shared/GameSim/Include/GameSim.vcxproj`의 AdditionalIncludeDirectories가 증거). 맵 단위 정보(시야/네비)는 `IWalkableQuery`(`tc.pWalkable`)처럼 추상화로만 접근한다.
3. **결정론을 깨지 않는다.** 난수는 `tc.pRng`(`DeterministicRng`, 시드 `0xC0FFEE`)만, 시간은 `tickIndex`/`fSimulatedTimeSec`만 사용한다. `std::rand`/wall clock/비결정 부동소수/비정렬 순회 금지. 동점 타이브레이크는 EntityID 같은 안정 키로 명시한다. (`ChampionAIBrain.h` 주석이 이 규약을 명문화한다 → 리플레이/self-play 재현성.)
4. **맵 단위 운영 정보는 Server에서 평탄화해 Context로 주입한다.** Shared가 Engine을 끌어오지 않도록 `Server/Private/Game/ServerAICommandProducer.cpp:Execute`가 경계다.
5. **서버 로그만으로 visual 성공을 판정하지 않는다.** 인게임(F5) 가시 확인 + trace를 함께 본다.
6. **챔피언별 전술은 brain/profile/champion-owned hook으로.** 공용 `ChampionAISystem` if체인 오염 금지. (현재 `TryExecuteYasuo*`/`TryExecuteJaxDive` 하드코딩은 갚아야 할 부채다.)
7. **런타임 튜닝도 client가 직접 set 금지.** 반드시 `eCommandKind::AIDebugControl` command를 거쳐 서버가 적용한다.
8. **새 경로 만들기 전에 기존 경로를 재사용/확장한다.** Engine측 `MCTSPlanner`/`BehaviorTreeSystem`/`RLBridge`는 **비권위(Client 로컬 CWorld 전용)·dead/stub 경로**다. 권위 봇에 끌어들이면 desync. 권위 봇은 오직 `Shared/GameSim/Systems/ChampionAI/*`.

---

## 2. 현재 코드가 실제로 하는 일 (Ground Truth)

### 2.1 실행 경로

```text
CGameRoom::Tick()                                   // 30Hz 고정 dt(1/30), 단일 RNG(0xC0FFEE)
  Phase_DrainCommands(tc)                           // 인간 입력 → m_pendingExecCommands
  Phase_ServerBotAI(tc)                             // 봇 의사결정
    → CServerAICommandProducer::Execute
      → CChampionAISystem::Execute(world, tc, m_pendingExecCommands)   // GameCommand만 push
  Phase_ExecuteCommands(tc)                         // 봇+인간 명령을 동일 executor로 권위 적용
  Phase_SimulationSystems(tc)                       // Stat/Buff/Cooldown/Move/Combat/JungleAI/AttackChase/챔프Sim/미니언/터렛/투사체/Damage/Death
  Phase_BroadcastSnapshot(tc)                       // ChampionAIDebugComponent 포함 스냅샷 송신
```

- 진입점: `Server/Private/Game/GameRoomTick.cpp:56` (Tick), `Server/Private/Game/GameRoomChampionAI.cpp:22` (Phase_ServerBotAI), `Server/Private/Game/ServerAICommandProducer.cpp:24` (Execute 위임).
- 봇 명령과 인간 명령이 **같은 큐·같은 executor**를 통과 → "봇 = 적법성 검증을 받는 가짜 플레이어"가 코드로 강제됨.
- 봇 명령은 생산된 다음 phase에서 적용된다 → **1틱 지연 모델**. 다단 콤보 설계 시 즉시 적용을 가정하면 안 됨.

### 2.2 의사결정 본체 — `Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp` (약 2556줄)

전부 anonymous namespace 자유함수로 4단 분리:
- **Perception**: `BuildChampionAIContext` (`:1132`) — 적 챔피언(락온/leashRange), 저체력 처형 대상, 적 미니언, 아군 웨이브, 적 구조물(오브젝트 랭크 게이팅), 포탑 위험도, Yasuo 전용(공중/E대시 미니언).
- **Score**: `UpdateChampionAIDecisionEvidence` (`:1555`) — `fChampionDecisionScore`/`fFarmDecisionScore`/`fStructureDecisionScore` + selfHp/enemyHp/거리/포탑위험. `CanAttackChampion` 게이트(저체력·포탑위험·leash).
- **Decision**: 상태머신 `Execute` (`:2459`), `ExecuteLaneCombat` (`:2248`), intent는 brain 위임 `SampleLaneCombatIntent` (`:1615`).
- **Execution**: `Emit*Command` (`:466`~`:677`, `:1864`~) — `MakeAICommand`로 issuer/tick/seq 채워 push.

상태/의도/행동(`Shared/GameSim/Components/ChampionAIComponent.h`):

| 구분 | 값 |
|---|---|
| `eChampionAIState` | MoveToOuterTurret, WaitForWave, LaneCombat, Diving, Retreat, Recalling, Dead |
| `eChampionAIIntent` | FarmMinion, AttackChampion, ExecuteDive, SiegeStructure, Retreat, Recall |
| `eChampionAIAction` | MoveToSafeAnchor, FollowWave, AttackMinion, AttackChampion, AttackStructure, UseFlashEscape, Retreat, Recall |

LaneCombat 우선순위(손수 짠 BT 셀렉터, `ExecuteLaneCombat`):
```text
Jax다이브 → 저HP/포탑위험 후퇴 → 진행중 콤보 → 구조물 시즈 → Yasuo전투 → 챔피언공격(harass roll) → 미니언 파밍 → 웨이브 따라가기
```

### 2.3 Brain / Profile 분리 (인간형의 1순위 seam)

- `Shared/GameSim/Systems/ChampionAI/ChampionAIBrain.h` — `IChampionAIBrain::DecideLaneCombatIntent(ai, input)`는 **intent만** 반환. `ChampionAIBrainInput`은 현재 6개 스칼라(매우 얇음). 헤더 주석이 결정론 규약을 명문화.
- `ChampionAIBrain.cpp` — 3종 구현:
  - `RuleBased` (현행 기본, 점수 기반)
  - `PlayerLike` ("사람같은 봇" — `kCommitScale 1.5`로 태세 유지 연장 + HP 우위일 때만 교전 = **인간다움의 코드화된 1차 사례**)
  - `Decision` (외부 판단 모듈/플래너/학습 정책 연동 자리, 현재 `RuleBased`로 위임 — `TODO(bot-v2)`)
- `Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h/.cpp` — 챔피언별 `ChampionAIProfile`(preferredRange/scan/leash/**aggression**/**kiteBias**/retreat·reengageHp/skillRules) + `ChampionAIComboPlan`(최대 8스텝). 18챔프 profile, 콤보는 Jax/Fiora/Ashe/Riven만.

### 2.4 명령 계약 — `Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h`

`eCommandKind`: `None / Move / CastSkill / BasicAttack / LevelSkill / BuyItem / UseItem / Recall / RecallCancel / AIDebugControl / Flash`.
- 봇이 **현재** 방출: `Move / CastSkill / BasicAttack / Flash / Recall`.
- 봇이 **아직 안 쓰는데 executor엔 있는 것**: `LevelSkill`(스킬 레벨업), `BuyItem`(아이템) → **운영/성장 표현에 그대로 활용 가능**. (`UseItem`은 executor 디스패치 미구현.)
- 검증은 전적으로 `CommandExecutor.cpp`의 `Handle*`가 단일 권위. 봇은 `IsSkillReady`/`IsFlashReady`/사거리 사전체크로 낭비 명령만 억제.
- 주의: `CastSkill`의 `itemId=2`는 아이템이 아니라 "2단계 스킬" 신호. `AIDebugControl`은 예약 itemId(`0xFFFx`)로 튜닝/리셋/강제행동을 나른다.

### 2.5 튜닝/디버깅 인프라 (이미 동작)

- **런타임 튜닝 14노브**: `ChampionAITuning` `{default,current,min,max,override}` (`ChampionAIComponent.h:117`). enum `eChampionAITuningId`. 예: championScanRange/minionScanRange/leashRange/retreatHpRatio/reengageHpRatio/turretDangerThreshold/lowHpExecuteThreshold/diveScanRange 등.
- **디버그 제어 모드**: `eChampionAIDebugControlMode{Observe, SingleDecision, ForceAction, TuneRuntime}`.
- **결정 트레이스**: `ChampionAIDecisionTraceEntry` 16엔트리 링버퍼(`debugDecisionTrace`) + `eChampionAIDecisionBlockReason` 13종(왜 명령을 못 냈는가: NoTarget/OutOfRange/Cooldown/FlashNotReady/ActionLocked/TurretDanger…).
- **왕복 경로**(30번 계획이 완성): `AIDebugPanel`(F9) 슬라이더/버튼 → `CommandSerializer::SendAIDebugTune/Control` → `CommandExecutor::HandleAIDebugControl`(`:2510`, `_DEBUG` 한정) → `ChampionAITuning` override → SnapshotBuilder → `SnapshotApplier` → `ChampionAIDebugComponent` → 패널/`DebugDrawSystem`.
- **시각화**: `Client/Private/UI/AIDebugPanel.cpp`(봇 테이블/상세/Runtime Tuning 슬라이더/Decision Trace 테이블), `Client/Private/UI/DebugDrawSystem.cpp`(scan/dive/attack/flash range 원 + state/intent/cmd/block 텍스트).
- **프로파일러**: `WINTERS_PROFILE_SCOPE`/`WINTERS_PROFILE_COUNT`, `ProfilerOverlay`(Scope/Counter, JSON export). 카운터 cap 주의(gotcha: `PROFILER_MAX_COUNTERS_PER_FRAME=32`).
- **로그**: `LogChampionAICommand`(score/HP/dist/range/turretDanger/reason/target/pos 한 줄, **512건 cap**), `OutputServerAITrace`.

### 2.6 재사용 가능한 자산 (다시 만들지 말 것)

- **네비**: Engine `CNavGrid`(512×512, 0.5u/cell) + A*(octile, ReachabilityCache, string-pull). Server `CWalkabilityAuthority`가 권위 단일화, `CGameRoom`이 `IWalkableQuery` 구현 → `tc.pWalkable`로 Shared에 주입. 봇 Move는 `CommandExecutor::TryAssignGridMovePath` → `MoveSystem`(7-fan 회피)로 자동 경로. **봇은 "목표 위치"만 정하면 충돌/지형은 처리됨.**
- **타겟 후보**: `GameRoomMinionAI.cpp:FindClosestEnemyCombatTarget`/`TryResolveServerMinionTargetPriority`(우선순위+거리 타이브레이크) — 막타 후보 탐색에 휴리스틱 재사용(단, 행동은 `EmitBasicAttack`으로).
- **라인 추종**: `ServerMinionFlowField`(셀 방향 룩업, 저비용), 레인 웨이포인트(`ServerMinionWaveRuntime`)는 이미 `ResolveChampionAILaneGoal`/`ResolveChampionAISafeAnchor`가 재사용. (Red팀 Top↔Bot 미러 `ResolveWaypointLane` 준수.)
- **결정론/헤드리스**: `DeterministicRng`(GetState/SetState/MakeSubSeed), `DeterministicTime`(kFixedDt 1/30), **`Tools/SimLab/main.cpp`(결정론 headless 러너)**, `ReplayRecorder`, `GameRoomSmokeRoster`.
- **읽기 API**: `GameplayDefinitionQuery::ResolveAttackRange/ResolveSkillRange/ResolveSkillCooldown/ResolveSkillActionLockTicks/IsSkillTwoStage` — 인간형 스킬샷의 사거리·캐스트락·이단계 정보 정식 질의처.

### 2.7 핵심 공백 (= 이 가이드가 채울 대상)

| 공백 | 현황 | 영향 |
|---|---|---|
| **운영(매크로) 부재** | state/intent가 라인전 6종 한정. 로밍/오브젝트/한타/합류/리콜 타이밍/시야 없음 | "운영" 단계 0% |
| **brainType 미배선** | `GameRoomSpawn.cpp:764`에서 brainType 미지정 → 항상 RuleBased. PlayerLike/Decision 코드 경로 도달 불가 | "사람같은 봇" 실행 안 됨 |
| **난이도 미반영** | `ai.difficulty=slot.botDifficulty` 저장만, 의사결정 미참조 | 난이도별 차등 0 |
| **전지적 perception** | FOW/시야 gate 없이 `CanBeTargetedBy`만으로 월드 전수 스캔 | 인간다움/공정성과 정면 충돌 |
| **스킬샷 리드 없음** | `EmitSkillCommand`는 현재 위치로만 조준. 타겟 속도 예측 없음 | 논타겟 적중/사람다움 미흡 |
| **Decision brain 빈 자리** | 외부 판단/학습 정책 위임만 | 고수준 "상황판단→선택" 미연결 |
| **마나 no-op** | `ManaComponent` 존재하나 manaCost=0, 소모 없음 | "마나 고려됨"으로 착각 금지 |
| **챔프 로직 하드코딩** | Yasuo/Jax가 공용 cpp에 분기 | 로스터 확장 시 오염 |

---

## 3. 사람처럼 보이게 하는 설계 방향

### 3.1 3계층 의사결정을 현 코드에 얹기

| 계층 | 주기 | 현 코드 위치 | 인간형으로 채울 것 |
|---|---|---|---|
| **Operational** (마이크로) | 매 틱 | `Emit*Command` | 스킬샷 리드/조준 오차, 평타·막타 무빙, 궁 캔슬 |
| **Tactical** (미들) | 0.2~0.8s | brain intent + `ExecuteLaneCombat` | 교전/백오프 판단, 콤보, 카이팅, 트레이드 |
| **Strategic** (매크로) | 2~5s | **신규** state/intent | 로밍/오브젝트/리콜/아이템/합류/시야 |

핵심: **저수준 실행(Emit*)·중수준 brain은 이미 분리돼 있다.** 신규 작업의 대부분은 (a) brain 입력(perception)을 두껍게, (b) 새 매크로 state/intent를 추가, (c) Emit 단계에 결정론 노이즈를 주입하는 것이다.

### 3.2 "사람다움"의 5가지 메커니즘 (전부 `tc.pRng`로)

1. **Perception 모델** — 전지적 스캔 제거. 시야/안개 gate + 인지 지연(마지막 목격 위치). `BuildChampionAIContext`에 visibility 질의를 추가(Shared가 참조 가능한 형태로 Server에서 주입).
2. **반응 지연 / 결정 주기** — `decisionInterval`(0.20s), `intentHoldTimer`/`intentHoldDuration`(0.80s), PlayerLike `commitScale`(1.5)가 이미 즉답 억제 노브. 난이도별로 지터.
3. **조준/실행 오차** — `EmitSkillCommand`의 `direction`/`groundPos`에 난이도별 각도 오차, `EmitBasicAttackCommand`에 평타·막타 미스 확률, 스킬샷에 타겟 속도 리드.
4. **비최적 선택** — 항상 최고점만 고르지 않기(ChooseTopN/softmax), 실수 주입(잘못된 교전, 플래시 낭비), hysteresis로 상태 떨림 방지.
5. **일관된 성향/난이도 프리셋** — `aggression`/`kiteBias`(이미 profile 슬롯, 의사결정 미사용) 활성화 + reaction/accuracy/mistake 세트.

### 3.3 라인전(사람처럼)

- **막타(last-hit)**: 미니언 `HealthComponent.fCurrent <= 내 예상 평타딜`일 때만 `EmitBasicAttack`. 후보는 `FindClosestEnemyCombatTarget` 재사용 + HP/AD read. **미니언 직접경로가 아니라 ChampionAISystem 의사결정에 끼운다(북극성 1).**
- **웨이브 관리**: 밀기/얼리기/프리징 intent 신설(현재 없음).
- **트레이드/존征**: harass roll을 "상대 핵심 스킬 쿨다운 인지" 기반으로 정교화.
- **포지셔닝**: 킬존 밖·미니언 뒤·포탑 사거리 밖을 "목표 위치"로. nav clamp/depenetration이 충돌은 막아줌.

### 3.4 교전(사람처럼)

- 스킬샷 예측(리드), 콤보(이미 `ComboPlan`), 카이팅(`kiteBias` 활성화), 이니시/백오프 타이밍.
- (선택·연구) 1v1/2v2 **결정론 BattleState 미니 시뮬**로 "몇 수 앞" — 반드시 권위판, GameSim 룰과 동기화. Engine 비권위 MCTS 재사용 금지.

### 3.5 운영(매크로) — 가장 큰 공백, 가장 큰 임팩트

- **신규 state/intent**: `eChampionAIState`에 Roam/Objective/Teamfight/Group/Back, `eChampionAIIntent`에 로밍/합류/오브젝트/와드/리콜타이밍 추가. 명령은 기존 `Move`/`Recall`/`BuyItem`/`LevelSkill`로 표현 가능.
- **Team Blackboard (GameSim측 신설)**: focus target / current objective / assist 요청 / missing enemy·last-known position. **Engine `Blackboard.h` 재사용 금지**(의존성 방향) — GameSim에 동등 타입 정의 후 `GameRoomTick`에서 시스템화. (`.md/plan/ai/11_TEAM_BLACKBOARD.md`가 어휘 출처.)
- 맵 단위 입력(오브젝트 타이머/시야)은 Server에서 평탄화해 Context 주입.

### 3.6 의사결정 주체 선택 — 권장 우선순위

1. **PlayerLike brain 확장 + perception/profile 확장** (surgical, 현 구조와 정렬) ← **1순위**
2. **Decision brain에 결정론 플래너 주입** (Utility 점수/influence를 `ChampionAIBrainInput` 확장으로 넣고 intent 결정)
3. **(포트폴리오/연구) imitation(인간 로그) → RL warm-start** — ML Lab(`C:/Users/tnest/Desktop/NYPC`) 경계 준수, runtime 직접 수정 금지, 산출물은 **baked artifact/순수 데이터로만** 주입.
- **안티패턴**: `Engine/Public/AI`에 새 프레임워크 추가(레거시와 이중화), 비권위 MCTS/BT/RLBridge를 권위 봇에 끌어쓰기, `AIIntentComponent` 식 추가 추상화로 GameCommand 직접생산 모델을 한 겹 더 감싸기(Karpathy 단순성 위반).

---

## 4. 튜닝 / 디버깅 인프라 (있는 것 + 완성할 것)

### 4.1 원칙 — "튜닝 전에 inspectable debug UI/오버레이/바운디드 트레이스 먼저" (CLAUDE.md 규약)

이 규약은 이미 상당 부분 충족돼 있다. 인간형 작업의 모든 새 행동은 **trace에 남고, 패널에서 보이고, 슬라이더로 만져지는 상태**로 추가한다.

### 4.2 가용 자산 (그대로 사용)

| 자산 | 위치 | 용도 |
|---|---|---|
| AI Debug 패널 (F9) | `Client/Private/UI/AIDebugPanel.cpp` | 봇 선택, state/intent/action/score/blockReason, 강제행동, 14노브 슬라이더, trace 테이블 |
| World 오버레이 | `Client/Private/UI/DebugDrawSystem.cpp` | scan/dive/attack/flash range 원, 상태 텍스트 |
| 결정 트레이스 | `ChampionAIComponent.debugDecisionTrace[16]` | 의도/행동/점수/거부사유 시계열 |
| 거부 사유 | `eChampionAIDecisionBlockReason` 13종 | "왜 안 했나" 설명 |
| 튜닝 왕복 | `AIDebugControl` → `HandleAIDebugControl` | 권위 런타임 튜닝/리셋/강제 |
| 프로파일러 | `WINTERS_PROFILE_SCOPE`, `ProfilerOverlay` | 봇 결정/네비 비용 |

### 4.3 새 인간형 노브를 붙이는 표준 절차 (30번 계획이 템플릿)

```text
1) ChampionAIComponent.h : eChampionAITuningId에 항목 + ChampionAITuning에 파라미터 추가
2) CommandExecutor.cpp   : ResolveChampionAITuningParamById / ApplyChampionAITuningOverride에 매핑
3) Snapshot.fbs          : 디버그 필드 추가 → run_codegen.bat
4) SnapshotBuilder.cpp   : ChampionAIComponent → 스냅샷 채우기
5) SnapshotApplier.cpp   : 스냅샷 → ChampionAIDebugComponent
6) AIDebugPanel.cpp      : RenderTuningSlider 한 줄 추가
```

### 4.4 디버그 가시성 강화 권고 (인간형 회귀 분석용)

- **의도 vs 명령 vs 거부사유**를 한 화면에(이미 trace에 다 있음 — 시각 그래프만 보강).
- **perception 시각화**: 시야/안개 경계, 위협(threat) 히트, 막타 후보 강조 오버레이.
- **score breakdown 패널**: `fChampionDecisionScore` 세부 요인(현재 합산값만 보임).
- **trace 깊이 16 → 확장 + JSONL flush**: 마이크로(<50ms) 추적엔 16칸 부족. cap 제거하고 스트리밍(§5.3).
- **per-bot profiler counter**: 봇별 decision/네비 비용. (카운터 cap 상향 또는 통합 먼저 — gotcha.)

### 4.5 결정론 보존 디버깅 규칙

- `ForceAction`/`SingleDecision`/`TuneRuntime`은 `_DEBUG` 한정 → release 검증은 별도 빌드/경로로.
- 디버그 입력도 client가 직접 mutate 금지, 반드시 command 왕복.
- 트레이스/로그 추가가 결정성·성능을 흔들지 않게(전수 ForEach 추가 시 scope/counter로 측정).

---

## 5. 인게임 튜닝 + 검증 파이프라인

### 5.1 검증 피라미드

```text
4) 메트릭 리그        frozen baseline 대비 challenger 우위 (brainType이 리그 axis)
3) 인게임 스모크 (F5) AIDebugPanel + DebugDraw로 "왜 그 행동/왜 멈춤" 눈으로 확인
2) 시나리오 하니스    SimLab headless + 고정 seed + 배치 상황 → trace/metric assert
1) 결정론 회귀        같은 seed/입력 → 동일 스냅샷 해시 (parity)
```

아래에서 위로 신뢰를 쌓는다. 1·2는 자동(빠른 회귀), 3은 수동(체감/visual 진실), 4는 종합(강함·인간다움 증거).

### 5.2 성공 기준(metric) 어휘

- **강함**: 승률 / Elo / score diff / 오브젝트 참여 / CS@10 / 포탑 골드 / KDA.
- **인간다움**: 반응시간 분포, APM, 스킬 적중률, **decision churn(초당 intent 전환 수)**, **blunder rate(blockReason 기반 비합리 멈춤)**, p95 decision time. 사람 분포와의 거리는 KL/Wasserstein류.
- **안정성**: desync 0, 결정론 parity 100%, tick budget 내(프로파일러).

(어휘 출처: `.md/plan/ai/15_MOBAZERO_RESEARCH_PLATFORM_ROADMAP.md`, `12_DIFFICULTY.md`.)

### 5.3 데이터 export (trace → JSONL)

`PushChampionAIDecisionTrace`(`ChampionAISystem.cpp:257`)와 `LogChampionAICommand`(`:403`)가 **이미 모든 메트릭 입력값**(score/HP/dist/turretDanger/reason)을 모은다. 형식만 구조화 JSONL로 flush하면 즉시 dataset·메트릭 소스가 된다. **단 cap 제거/스트리밍 필요**(현재 trace 16, 로그 512 — 장시간 eval에서 뒤쪽 누락, PROFILER cap gotcha와 동형).

### 5.4 인게임 튜닝 루프 (한 사이클)

```text
1. AIDebugPanel(F9)에서 봇 선택 → trace/score/blockReason 관찰
2. 가설: "X 때문에 비인간적/약함" (예: turretDanger 과민 → 다이브 못함)
3. 슬라이더로 노브 조정 (AIDebugControl, 서버 권위 적용) → 즉시 체감
4. 좋으면 ChampionAIProfile/기본값에 반영 (constexpr; 추후 data-driven cutover)
5. 시나리오 하니스로 회귀 고정 (다시 깨지지 않게)
```

데이터 외부화 사다리(`.md/TODO/05-21/CHAMPION_AI_NEXT_STEPS_ROADMAP.md` 확정): `C++ constexpr → ImGui 런타임 튜너 → JSON/FlatBuffers → 작은 BT asset → Lua(최후)`. **현재 위치 = constexpr + ImGui 튜너.** JSON/Lua는 챔프 8+ 또는 기획자 직접 수정 필요 시까지 미룬다.

### 5.5 "서버 로그만으로 판정 금지"

인게임 visual(스킬 시전/이동/리콜이 실제로 보이는가)과 trace(왜)를 **동시에** 확인한다. 30번 계획의 수동 확인 절차가 모델:
- 봇 선택 시 state/intent/action/divePhase/last command/block reason이 갱신되는가.
- 슬라이더로 range/threshold가 다음 스냅샷에 반영되는가, Reset이 기본값으로 돌리는가.
- DebugDraw에서 scan/dive/attack/flash range가 서로 다른 색 원으로 보이는가.
- 다이브 시 `EngageQ→ArmW→BasicAttack→…→FlashExit→ExitMove` 순서가 trace에 남는가; 안 하면 block reason이 설명 가능한가.

---

## 6. 완성 로드맵 (Karpathy goal-driven, 실현 트랙 정렬)

각 Phase는 **검증 통과 후** 다음으로. 모든 Phase는 §1 북극성 준수.

### Phase A — 활성화 배선 (사전 정지작업)
- **목표**: PlayerLike/Decision brain과 난이도가 실제로 도달/반영되게 한다.
- **산출물**: `GameRoomSpawn.cpp:764`에서 `brainType`/난이도 → brain·tuning 매핑. AIDebugPanel에 brainType 표시.
- **검증**: 봇 스폰 시 PlayerLike가 실행됨(trace로 확인), 난이도별 노브 차등이 보임.

### Phase B — 디버그/검증 토대 완성
- **목표**: 회귀를 자동으로 잡는 토대.
- **산출물**: trace JSONL export(cap 제거), **시나리오 하니스**(SimLab + 고정 seed + 배치 상황), parity 회귀(동일 해시).
- **검증**: 같은 seed → 동일 스냅샷 해시 100%, "1v1 트레이드" 시나리오에서 메트릭 assert 통과.

### Phase C — 인간형 마이크로
- **목표**: 전지적 제거 + 사람급 실행.
- **산출물**: perception FOW/시야 gate, 결정론 reaction/aim 노이즈, 막타, 스킬샷 리드.
- **검증**: 시야 밖 적에 무반응, 막타율/스킬 적중률 메트릭이 목표 구간, 반응시간 분포가 사람 근접.

### Phase D — 인간형 라인전
- **목표**: 라인전 의도 다양화.
- **산출물**: 웨이브 관리(밀기/얼리기/프리징)/트레이드/존征 intent.
- **검증**: 시나리오별 CS@10·체력 교환 효율 메트릭.

### Phase E — 운영(매크로)
- **목표**: 라인 밖 의사결정.
- **산출물**: 신규 state/intent(Roam/Objective/Teamfight/Back) + **Team Blackboard(GameSim측)** + 리콜 타이밍/아이템 구매(BuyItem)/레벨업(LevelSkill)/오브젝트 콜.
- **검증**: 로밍/오브젝트 참여 메트릭, 갱 콜/합류 trace.

### Phase F — (선택/연구) 학습형
- **목표**: Decision brain 실체화.
- **산출물**: imitation(인간 로그 → BC) → RL warm-start. ONNX 추론은 baked artifact로 주입.
- **검증**: frozen baseline 대비 holdout 리그 우위 + 결정론·리플레이 유지.

### 보류 / 안티
- 5v5 전면 RL(연구 수준, 14/15 문서가 이미 보류), `Engine/Public/AI` 새 프레임워크, 비권위 MCTS/BT를 권위 봇에 결선.

---

## 7. 챔피언 봇 추가 파이프라인 (현행 실제 절차)

`BOT_AI_ROSTER_ADD_PIPELINE.md`는 stale. 실제 절차:
1. `ChampionAIPolicy.cpp`의 `GetChampionAIProfile`/`GetChampionAIComboPlan`에 profile·콤보 추가(constexpr).
2. 전용 전술이 필요하면 brain 또는 champion-owned hook으로(공용 `ChampionAISystem` if체인 금지). Yasuo/Jax 하드코딩은 따라 하지 말고 점진 이전 대상으로 둔다.
3. 스폰 시 `brainType` 지정(Phase A 배선 이후).
4. AIDebugPanel/trace로 콤보 순서·복귀(콤보 후 Farm/FollowWave) 검증 → 시나리오로 고정.

---

## 8. 다음 액션 (사용자 확인 필요)

본 가이드 승인 후 **Phase A**부터 `.md/계획서작성규칙.md` 형식 계획서로 분해한다. 그 전에 결정이 필요한 선택지:

- **(a) 인간형 1순위 경로**: PlayerLike brain 확장 vs Decision 모듈(플래너) 먼저.
- **(b) 검증 우선순위**: 시나리오 하니스(자동) 먼저 vs 인게임 스모크(수동/체감) 먼저.
- **(c) 난이도 프리셋 범위**: Intro~Master 몇 단계, 각 단계 노브 세트.
- **(d) 운영(Phase E) 착수 시점**: 마이크로/라인전 완성 후 vs 병행.

---

## 부록 — 핵심 파일 인덱스

| 역할 | 파일 |
|---|---|
| 봇 두뇌(본체) | `Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp` |
| intent brain(인간형 seam) | `Shared/GameSim/Systems/ChampionAI/ChampionAIBrain.{h,cpp}` |
| 챔피언별 데이터 | `Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.{h,cpp}` |
| 봇 상태/튜닝/트레이스 | `Shared/GameSim/Components/ChampionAIComponent.h` |
| 명령 계약/실행 | `Shared/GameSim/Systems/CommandExecutor/{ICommandExecutor.h,CommandExecutor.cpp}` |
| 서버 배선/목표 산출 | `Server/Private/Game/{GameRoomTick.cpp,GameRoomChampionAI.cpp,ServerAICommandProducer.cpp,GameRoomSpawn.cpp}` |
| 재사용 네비/타겟 | `Server/Private/Game/{GameRoomNav.cpp,WalkabilityAuthority.cpp,GameRoomMinionAI.cpp,ServerMinionFlowField.cpp}` |
| 인게임 디버그 UI | `Client/Private/UI/{AIDebugPanel.cpp,DebugDrawSystem.cpp}` |
| 튜닝 왕복 | `Client/Private/Network/Client/CommandSerializer.cpp`, `Client/Private/Network/Client/SnapshotApplier.cpp` |
| 결정론/헤드리스 | `Shared/GameSim/Core/Determinism/*`, `Tools/SimLab/main.cpp`, `Server/Private/Game/ReplayRecorder.cpp` |
| 실현 트랙 문서 | `.md/plan/Champion/{26,28,29,30}_*.md`, `.md/TODO/05-21/CHAMPION_AI_NEXT_STEPS_ROADMAP.md` |
| 환경/학습 설계 | `Plan/S02_OPENAI_STYLE_BOT_AI_ENVIRONMENT_PLAN.md`, `.md/plan/ai/15_MOBAZERO_RESEARCH_PLATFORM_ROADMAP.md` |
