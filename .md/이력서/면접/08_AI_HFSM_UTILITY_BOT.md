# 08. AI (HFSM / Utility / BT / MCTS / 봇) — 면접 대비 세션

> 도메인 성숙도: **working** (라인전 봇은 production 경로에서 매 틱 동작, BT/MCTS/RL은 격리된 비권위 PoC/dead)
> 근거 문서: `.md/이력서/WINTERS_DOMAIN_HONEST_MAP_2026-06-26.md` §8
> 핵심 코드: `Shared/GameSim/Systems/ChampionAI/*`, `Engine/{Public,Private}/AI/*`, `Tools/SimLab/main.cpp`

---

## 0. 한 줄 본질 + 현재 상태

**한 줄 본질**: LoL 봇 AI는 근본적으로 "**서버 권위·결정론 시뮬레이션 안에서, 사람 플레이어와 똑같은 입력(GameCommand)만 생산하는 가짜 플레이어**"다. 봇은 truth(Transform/HP/cooldown)를 직접 못 고치고, perception → 가치 평가(Utility) → 상태머신(HFSM) → 의도(brain) → 명령 방출 파이프라인으로 "지금 무엇을 할지"만 결정한다.

**현재 상태(정직하게)**:
- **production/working**: 라인전 봇 한 종류. `CChampionAISystem`이 30Hz 권위 틱마다 18챔피언 profile + 7상태 HFSM + Utility 스코어링 + brain 위임으로 GameCommand를 만들어 인간 명령과 **같은 큐·같은 executor**를 통과한다. 14노브 런타임 튜닝 + 결정 트레이스 + 인게임 디버그 패널까지 깔려 있다.
- **PoC/dead (절대 "구현했다"라고 하면 즉사)**: `Engine/Private/AI/`의 BehaviorTree / MCTSPlanner / RLBridge. 셋 다 **권위 봇에 연결돼 있지 않다**. MCTS는 UCT 본문은 있으나 출력을 아무도 안 읽고 비결정(`std::random_device`), RLBridge는 `LoadModel`/`Infer`가 무조건 `false`인 stub, BT 계열은 Client 로컬 `CWorld` 전용 비권위 경로다.
- **planned**: 운영(매크로) 의사결정, brainType 스폰 배선, 난이도 차등, FOW 기반 perception, 스킬샷 리드. `.md/plan/ai/16_*`이 완성 로드맵.

이 도메인의 가장 강한 서사는 "**화려한 AI 기법 목록**"이 아니라 "**북극성 제약(봇=가짜 입력)을 코드로 강제해 멀티플레이 결정론을 깨지 않는 구조**"와 "**무엇이 진짜 도는 경로이고 무엇이 dead PoC인지 스스로 그어둔 메타인지**"다.

---

## 1. 핵심 개념 (본질)

### 1.1 왜 "봇 = 가짜 플레이어 입력"인가 (1차 원리)

LoL류 PvP는 두 가지를 동시에 요구한다: (a) **서버 권위** — 클라가 결과(위치/데미지)를 주장하면 핵, 그래서 서버만 truth를 판정한다. (b) **결정론** — 같은 입력이면 모든 머신/리플레이/self-play에서 같은 결과가 나와야 desync가 안 난다.

봇 AI를 만들 때 가장 흔한 실수는 "봇이니까 서버 내부니까 HP를 직접 깎고 위치를 직접 옮기자"이다. 그러면 봇은 인간이 못 하는 짓(쿨다운 무시, 텔레포트)을 할 수 있게 되고, 검증 경로가 인간과 갈라져 권위·결정론 모델이 **두 갈래**가 된다.

그래서 근본 설계는: **봇은 인간과 정확히 같은 좁은 통로(GameCommand)로만 세계에 영향을 준다.** 미니언/터렛은 NPC라 truth를 직접 고치지만, 챔피언 봇만큼은 "사람처럼" 명령을 내고 그 명령은 인간 명령과 동일한 `CommandExecutor`의 적법성 검사(쿨다운/사거리/학습여부/타겟 생존)를 받는다. 이 한 가지 제약이 안티치트·결정론·리플레이·테스트를 전부 단일화한다.

### 1.2 의사결정 기법들의 본질 (면접관에게 가르치듯)

- **FSM / HFSM (Hierarchical Finite State Machine)**: AI를 유한개의 명시적 상태(예: MoveToOuterTurret, WaitForWave, LaneCombat, Diving, Retreat, Recalling, Dead)로 모델링하고, 조건에 따라 전이한다. "계층형"은 상위 상태(LaneCombat) 안에 하위 결정(콤보 단계 등)을 중첩한다는 뜻. 장점: 디버깅·예측이 쉽다(지금 어느 상태인지 한눈에). 단점: 상태/전이 수가 늘면 폭발한다.

- **Utility AI**: 각 선택지에 **점수(utility)**를 매겨 가장 높은 걸 고른다. 본질은 "이질적인 선택지(파밍 vs 교전 vs 시즈)를 **공통 척도**로 환산해 비교 가능하게 만드는 것". Winters에서는 그 공통 척도가 **골드**다 — 킬 가치, 막타 골드, 포탑 골드, 경제 우위를 전부 골드로 환산해 `ChampionFightValue`/`MinionFarmValue`/`StructureValue`로 0~1 스코어를 낸다.

- **Behavior Tree (BT)**: 행동을 트리(Selector=우선순위 OR, Sequence=AND)로 조합. 모듈성·재사용이 강점. Winters의 **권위 봇은 1급 BT 객체를 안 쓰고**, `ExecuteLaneCombat`에 "손으로 짠 Selector"를 if 체인으로 펼친다(우선순위: 다이브 → 후퇴 → 진행 중 콤보 → 시즈 → 챔피언 공격 → 파밍 → 웨이브 추종). Engine에 별도 BT 라이브러리가 있지만 비권위 경로다.

- **MCTS (Monte Carlo Tree Search)**: 현재 상태에서 가능한 행동을 트리로 펼치고, 무작위 rollout으로 미래를 시뮬레이션해 평균 보상이 높은 수를 고른다(UCT = exploit + explore). 교전 "몇 수 앞" 예측에 쓸 수 있으나, **무작위 rollout이 결정론과 충돌**하고 게임 룰 복제 비용이 크다. Winters MCTS는 PoC 단계로 비결정 RNG를 써서 권위 봇에 못 붙인다.

- **RL / Imitation**: 인간 로그로 정책을 학습(BC)하거나 self-play로 강화학습(PPO). 본질은 "규칙을 손으로 안 짜고 데이터/보상에서 정책을 뽑는다". 추론은 ONNX 같은 baked artifact로 주입해야 결정론·런타임 분리가 유지된다. Winters는 `RLBridge` 인터페이스(State 인코딩/Infer/BestAction)만 stub으로 있고 모델 로딩은 미구현.

### 1.3 결정론 계약 (이 도메인의 진짜 어려움)

봇이 결정론을 깨면 멀티플레이 desync, 리플레이 불일치, self-play 재현 실패가 한꺼번에 터진다. 그래서 봇 코드는 **하드 제약**을 지킨다:
- 난수는 `tc.pRng`(`DeterministicRng`, 시드 `0xC0FFEE`)만, `std::rand`/wall clock 금지.
- 시간은 `tickIndex`/고정 dt(1/30)만.
- 비정렬 컨테이너 순회 금지, 동점 타이브레이크는 EntityID 같은 안정 키로 명시.
- brain은 **stateless**, 모든 상태는 `ChampionAIComponent`에 둔다 → 엔티티 간 공유·재진입 안전.

이 계약은 `ChampionAIBrain.h` 주석에 명문화돼 있고, `Tools/SimLab/main.cpp`가 same-seed 동일 해시로 회귀 검증한다.

---

## 2. 왜 이 선택인가 — 기술 스택 선택 + Trade-off

### 2.1 봇을 "가짜 입력"으로 vs "권위 직접 수정"으로

| 선택지 | 장점 | 단점 | 내 결정 |
|---|---|---|---|
| **봇 = GameCommand만 생산** (선택) | 안티치트/결정론/리플레이/테스트가 인간과 단일 경로, 봇이 룰 위반 불가 | 1틱 지연 모델(콤보 설계 복잡), 명령 통로 표현력에 갇힘 | **채택**. 멀티플레이 정합성이 1순위 |
| 봇이 truth 직접 수정 (미니언처럼) | 즉시 반영, 콤보 구현 쉬움 | 권위 모델 2갈래, 봇이 사람 못 할 짓 가능, 검증 경로 분리 | 기각 |

근본 trade-off: **표현력/즉시성을 포기하고 정합성/검증 단일화를 얻었다.** 신입 1인 프로젝트에서 가장 비싼 비용은 "desync 디버깅"이므로, 명령 통로를 좁혀 그 비용 자체를 없앤 것이 합리적이다.

### 2.2 손으로 짠 HFSM+Utility 하이브리드 vs 1급 BT/MCTS 프레임워크

| 선택지 | 장점 | 단점 | 내 결정 |
|---|---|---|---|
| **HFSM + Utility + brain 위임** (선택) | 결정론 쉽게 보존, 디버깅 직관(상태/점수/거부사유 trace), 의존성 가벼움 | 챔프별 분기가 공용 cpp 오염(Yasuo/Jax 하드코딩 부채) | **채택** |
| Engine 1급 BT/MCTS 프레임워크 재사용 | 모듈성/확장성 | Engine은 Client 로컬 비권위 `CWorld` 기반 → 권위 봇에 끌어오면 desync, 추상화 이중화 | 기각(비권위 경로로 격리) |

근본 trade-off: **확장성(BT 모듈성)을 포기하고 결정론·단순성을 얻었다.** Karpathy 가드레일("새 경로 만들기 전에 기존 경로 재사용, 단일성")에 정확히 정렬된다. BT/MCTS는 "엔진에 그런 능력도 있다"는 학습용 PoC로 남기되 **권위 봇과 물리적으로 분리**했다.

### 2.3 brain을 별도 인터페이스로 분리 (IChampionAIBrain)

| 선택지 | 장점 | 단점 | 내 결정 |
|---|---|---|---|
| **brain 인터페이스로 intent만 분리** (선택) | 챔프별·난이도별 전술을 공용 코드 오염 없이 교체(RuleBased/PlayerLike/Decision), 학습 정책 주입 자리 확보 | 입력(ChampionAIBrainInput)이 얇으면 brain이 할 수 있는 게 제한 | **채택** — 인간형 봇의 1순위 seam |
| 상태머신 안에 if로 다 박기 | 코드 한 곳 | 챔프 늘면 폭발, 테스트 불가 | 기각 |

근본 trade-off: brain은 "**무엇을 할까(intent)**"만 결정하고 "**어떻게 실행(Emit*Command)**"은 시스템이 담당하게 분리. 의사결정 정책은 갈아끼우되 실행/결정론 인프라는 고정 — 학습 정책으로 가는 미래 경로까지 같은 seam에서 흡수한다.

### 2.4 Utility 척도를 "골드"로 통일

이질적 선택지(파밍/교전/시즈)를 비교하려면 공통 단위가 필요하다. LoL은 모든 행동이 결국 골드/경험치 우위로 수렴하므로 **골드를 공통 화폐**로 골랐다(`ChampionAIValuation`: 킬 300, 근접 미니언 21, 원거리 14, 포탑 250골드 등 RewardRegistry 실측치 정렬). 대안(승률 직접 추정)은 모델이 필요해 과한 복잡도다.

---

## 3. 실제 구현 (코드 근거)

### 3.1 실행 경로 (권위 틱 안에서)

```text
CGameRoom::Tick()  (30Hz 고정 dt, 단일 RNG 0xC0FFEE)
  Phase_DrainCommands     인간 입력 → pendingExec
  Phase_ServerBotAI       CServerAICommandProducer::Execute
                            → CChampionAISystem::Execute(world, tc, pendingExec)  // GameCommand만 push
  Phase_ExecuteCommands   봇+인간 명령을 동일 CommandExecutor로 권위 적용
  Phase_SimulationSystems Stat/Buff/Cooldown/Move/Combat/Minion/Turret/Projectile/Damage
  Phase_BroadcastSnapshot ChampionAIDebugComponent 포함 스냅샷 송신
```
- 봇 명령은 생산된 **다음 phase**에서 적용 → **1틱 지연 모델**. 다단 콤보는 즉시 적용을 가정하면 안 됨.

### 3.2 의사결정 본체 — `ChampionAISystem.cpp` (약 2,556줄, anonymous namespace 자유함수 4단 분리)

1. **Perception** — `BuildChampionAIContext`. 적 챔피언(락온/leashRange), 저체력 처형 대상, 적 미니언, 아군 웨이브, 적 구조물, 포탑 위험도를 월드 스캔으로 수집해 `ChampionAIContext`로 평탄화.
2. **Score (Utility)** — `UpdateChampionAIDecisionEvidence` + `ChampionAIValuation::*`. `fChampionDecisionScore`/`fFarmDecisionScore`/`fStructureDecisionScore`와 selfHp/enemyHp/거리/포탑위험을 채우고, `CanAttackChampion` 게이트(저체력·포탑위험·leash)를 평가.
   - `ChampionAIValuation.cpp:31` `ChampionFightValue`: base 0.45 + 생존여유·체력우위·사거리·백업웨이브·경제우위 가산, 포탑위험 감산 후 Clamp01.
3. **Decision (HFSM + brain)** — 상태머신 `Execute`(`:2459` 부근)가 7상태를 돌리고, LaneCombat 안에서 `ExecuteLaneCombat`(손으로 짠 Selector). intent는 `SampleLaneCombatIntent`(`:1964`)가 입력을 평탄화해 **brain에 위임**:
   ```cpp
   ai.intent = ResolveChampionAIBrain(ai.brainType)
       .DecideLaneCombatIntent(ai, input);   // ChampionAISystem.cpp:1980
   ```
4. **Execution** — `Emit*Command`(Skill/BasicAttack/Move/Flash/Recall). `MakeAICommand`가 issuer/tick/seq를 채워 `outCommands`에 push. 콤보는 `TryEmitAttackChampionCombo`(`:1641`)가 `ChampionAIComboPlan`의 step을 인덱스로 진행.

### 3.3 brain 3종 — `ChampionAIBrain.cpp`

- **RuleBased**(기본, `:8`): 점수 기반. `bCanAttackChampion`이고 `championScore ≥ farmScore + margin`이면 AttackChampion, 아니면 FarmMinion. `intentHoldTimer`로 태세 유지.
- **PlayerLike**(`:47`): "사람같은 봇" — `kCommitScale=1.5`로 태세 유지를 50% 연장(봇 특유의 즉답 억제) + **HP 우위(`fDecisionSelfHpRatio >= fDecisionEnemyHpRatio`)일 때만 교전**. 인간다움의 코드화된 1차 사례.
- **Decision**(`:91`): 외부 판단/학습 정책 자리. 현재 `RuleBased`로 위임(`TODO(bot-v2)`). 모듈 붙기 전까지 동작 보장.
- `ResolveChampionAIBrain`(`:108`): brainType→static brain 반환. brain은 stateless, 상태는 전부 `ChampionAIComponent`.

### 3.4 챔피언 데이터 — `ChampionAIPolicy.cpp`

- `GetChampionAIProfile`(`:461~`): **18 챔피언** profile(Annie/Ashe/Ezreal/Fiora/Garen/Irelia/Jax/Kalista/Kindred/LeeSin/MasterYi/Riven/Sylas/Viego/Yasuo/Yone/Zed …). preferredRange/scan/leash/**aggression**/**kiteBias**/retreat·reengageHp/skillRules.
- `GetChampionAIComboPlan`(`:500~`): 콤보는 **6명만**(Jax/Fiora/Ashe/Riven/LeeSin/Sylas). 최대 10스텝, 각 step에 slot/range/HP조건/targetMode(TargetEntity/AwayFromTarget/WardBehindTarget/SylasHijack 등).
  > 주의: `16_*` 가이드는 "Jax/Fiora/Ashe/Riven만"이라 적었으나 **현 코드는 LeeSin/Sylas 포함 6명**. 면접 시 코드 기준으로 답한다.

### 3.5 명령 계약 — `ICommandExecutor.h`

`eCommandKind`: None/Move/CastSkill/BasicAttack/LevelSkill/BuyItem/UseItem/Recall/RecallCancel/AIDebugControl/Flash.
- 봇이 **현재 방출**: Move/CastSkill/BasicAttack/Flash/Recall(+콤보의 UseItem 와드).
- 봇이 **아직 안 쓰는데 executor엔 있음**: LevelSkill/BuyItem → 운영/성장 표현에 향후 그대로 활용 가능.
- `CastSkill`의 `itemId=2`는 아이템이 아니라 "2단계 스킬" 신호. `AIDebugControl`은 예약 itemId(`0xFFFx`)로 튜닝/리셋/강제행동을 나름.

### 3.6 튜닝/디버깅 인프라 (`ChampionAIComponent.h`)

- **14노브 런타임 튜닝**: `ChampionAITuning`{default,current,min,max,override} + `eChampionAITuningId`(ChampionScanRange/LeashRange/RetreatHpRatio/TurretDangerThreshold/LowHpExecuteThreshold/DiveScanRange …).
- **결정 트레이스**: `debugDecisionTrace[16]` 링버퍼 + `eChampionAIDecisionBlockReason` 12종(NoTarget/TargetOutOfRange/SkillCooldown/FlashNotReady/ActionLocked/TurretDanger …) — "왜 명령을 못 냈는가".
- **왕복 경로**: `AIDebugPanel`(F9) → `CommandSerializer::SendAIDebugTune/Control` → `CommandExecutor::HandleAIDebugControl`(`_DEBUG` 한정, 서버 권위 적용) → 스냅샷 → `ChampionAIDebugComponent` → 패널/`DebugDrawSystem`. **client가 직접 set 금지, 반드시 command 왕복.**

### 3.7 dead/PoC 경로의 실제 상태 (red flag 근거)

- **MCTS** `MCTSPlanner.cpp:75` `m_rng(std::random_device{}())` — 비결정. `Plan()` 출력을 권위 봇 어디서도 안 읽음. Client 로컬 `CWorld` 전용.
- **RLBridge** `RLBridge.cpp:27` `LoadModel`→`m_bLoaded=false; return false`, `:62` `Infer`→ 항상 false. 모델 추론 미구현 stub.
- **BehaviorTree / AIIntent 큐**: `AIIntentComponent`/intent 큐의 **소비자 grep 0건** — push만 되고 읽히지 않음.

---

## 4. 검증 — 동작을 어떻게 증명했나

검증 피라미드(아래→위로 신뢰 누적):

1. **결정론 회귀 (자동)** — `Tools/SimLab/main.cpp`: 헤드리스 GameSim 러너. 같은 seed + 같은 스크립트 명령 → **per-tick state hash 동일**해야 통과(`main.cpp:420~438` FNV-1a로 pos/hp/dead/mana/level/gold/rng-state 해시), 다르면 exit 1. seed+1이 다른 해시를 내는지(seed 민감도)도 검사(`:649`). 봇 결정이 비결정이면 여기서 즉시 깨진다.
   > 정직한 한계: 해시는 시뮬 상태(위치/HP/골드)를 잡지, AI 내부 state/intent를 직접 잡지 않는다. 봇 결정의 결정성은 **시뮬 결과가 동일해야 한다는 제약을 통해 간접 검증**된다. SimLab은 FlatWalkable(navgrid 없는 평면) GameSim-only 미러라 "서버 시뮬 코어 결정론"으로 한정해 말한다.
2. **시나리오 하니스 (자동)** — 고정 seed + 배치 상황 → trace/metric assert. **현재 부분 구현/계획**(가이드 Phase B 산출물).
3. **인게임 스모크 (수동, F5)** — `AIDebugPanel`로 state/intent/action/divePhase/last command/blockReason 갱신 확인 + `DebugDrawSystem`으로 scan/dive/attack/flash range 원과 상태 텍스트 시각 확인. "서버 로그만으로 visual 성공 판정 금지" 규약.
4. **메트릭 리그 (종합)** — frozen baseline 대비 challenger 우위. **계획 단계**.

판정 기준: "됐다"는 (1) same-seed 해시 100% 일치 + (2) 인게임에서 콤보 순서(`EngageQ→ArmW→BasicAttack→…→FlashExit→ExitMove`)가 trace에 남고 의도대로 보이거나, 안 되면 blockReason이 이유를 설명할 수 있을 때.

---

## 5. 최적화

### 실제로 한 것
- **명령 낭비 억제**: 봇은 `IsSkillReady`/`IsFlashReady`/사거리 사전체크로 거부될 명령을 애초에 안 낸다 → executor 검증 부하·네트워크 명령 수 절감. 거부 시 blockReason 기록.
- **결정 주기 분리**: `decisionInterval`(0.20s)/`intentHoldTimer`(0.80s)로 매 틱 전면 재결정을 피함 → 결정 비용 분산 + 사람다움(즉답 억제) 동시 달성.
- **perception 후보 재사용**: 미니언/타겟 탐색은 미니언 AI의 `FindClosestEnemyCombatTarget` 휴리스틱을 재사용(중복 스캔 회피). 네비는 Engine `CNavGrid` + A* 결과를 `CommandExecutor::TryAssignGridMovePath`로 위임 → 봇은 "목표 위치"만 정함.

### 계획 중 (정량 수치 없음 — 측정 예정)
- **per-bot profiler counter**로 봇별 decision/네비 비용 측정. 단, `PROFILER_MAX_COUNTERS_PER_FRAME=32` cap 때문에 카운터가 누락될 수 있어 cap 상향/통합이 선행(gotcha 등록됨).
- **trace cap 제거 + JSONL 스트리밍**: 현재 trace 16칸/로그 512건 cap이라 장시간 eval에서 뒤쪽이 누락. 마이크로(<50ms) 분석엔 부족.
> 정직성: AI 도메인에 "X배 빨라졌다" 같은 정량 최적화 수치는 **아직 없다.** 있는 건 "낭비 명령 억제·결정 주기 분리"라는 구조적 최적화뿐이며, 측정 인프라(프로파일러)는 있으나 AI에 정조준한 측정은 아직 안 했다.

---

## 6. 구현 예정 (Planned) — 동일한 깊이로

> 사용자는 이걸 **실제로 구현한다.** 가이드 `.md/plan/ai/16_*`의 Phase A~F.

### Phase A — 활성화 배선 (사전 정지작업)
- **무엇/왜**: `GameRoomSpawn.cpp:770`은 `ai.difficulty = slot.botDifficulty`만 저장하고 `brainType`을 **안 정한다** → 라이브는 항상 RuleBased, PlayerLike/Decision 코드 경로에 **도달 불가**. 난이도도 의사결정에서 안 읽힘. "사람같은 봇"이 실행조차 안 되는 상태.
- **어떻게**: 스폰 시 `brainType`/난이도 → brain·tuning 매핑 추가. AIDebugPanel에 brainType 표시.
- **Trade-off**: 난이도를 어디서 읽을지(brain 입력 vs profile 오버라이드). 입력에 넣으면 brain이 무거워지고, profile에 넣으면 챔프×난이도 조합 폭발.
- **검증**: 봇 스폰 시 PlayerLike가 실제 실행됨(trace로 확인), 난이도별 노브 차등이 패널에 보임.

### Phase B — 디버그/검증 토대 완성
- **무엇/왜**: 회귀를 자동으로 잡는 토대. 현재 시나리오 하니스가 미완.
- **어떻게**: trace JSONL export(cap 제거), SimLab + 고정 seed + 배치 상황의 시나리오 하니스, parity 회귀(동일 해시).
- **Trade-off**: 로그 cap 제거가 성능/결정성을 흔들면 안 됨(전수 ForEach 추가 시 scope/counter로 측정).
- **검증**: same-seed 동일 해시 100%, "1v1 트레이드" 시나리오 메트릭 assert 통과.

### Phase C — 인간형 마이크로
- **무엇/왜**: 현재 perception이 **전지적**(FOW/시야 gate 없이 월드 전수 스캔)이고 스킬샷이 **현재 위치로만 조준**(리드 없음) → 사람답지도 공정하지도 않음.
- **어떻게**: (a) `BuildChampionAIContext`에 visibility 질의 추가(Shared가 Engine을 끌어오지 않게 Server에서 평탄화 주입, `IWalkableQuery`처럼 추상화). (b) `EmitSkillCommand`의 direction/groundPos에 **난이도별 결정론 조준 오차**(`tc.pRng`), 타겟 속도 기반 **리드(예측 사격)**. (c) 막타: 미니언 HP ≤ 예상 평타딜일 때만 평타.
- **Trade-off**: 시야 모델을 넣으면 perception 비용↑ + 결정성 보존이 까다로움(시야 질의도 결정론이어야 함). 조준 오차는 난이도별 튜닝 부담.
- **검증**: 시야 밖 적에 무반응, 막타율/스킬 적중률이 목표 구간, 반응시간 분포가 사람 근접(KL/Wasserstein 거리).

### Phase D — 인간형 라인전
- **무엇/왜**: 현재 라인전 의도가 6종(Farm/Attack/Dive/Siege/Retreat/Recall)뿐. 웨이브 관리(밀기/얼리기/프리징) 부재.
- **어떻게**: 새 intent 추가, harass roll을 "상대 핵심 스킬 쿨다운 인지" 기반으로 정교화, 포지셔닝(킬존 밖·미니언 뒤)을 "목표 위치"로(nav가 충돌 처리).
- **검증**: 시나리오별 CS@10, 체력 교환 효율 메트릭.

### Phase E — 운영(매크로) ★ 가장 큰 공백·임팩트
- **무엇/왜**: state/intent가 라인전 한정 → 로밍/오브젝트/한타/합류/리콜타이밍/시야 **0%**.
- **어떻게**: `eChampionAIState`에 Roam/Objective/Teamfight/Group/Back 추가, intent에 로밍/합류/오브젝트/와드 추가(명령은 기존 Move/Recall/**BuyItem**/**LevelSkill**로 표현). **Team Blackboard를 GameSim측에 신설**(Engine `Blackboard.h` 재사용 금지 — 의존성 방향) — focus target/objective/assist 요청/missing enemy last-known position.
- **Trade-off**: 매크로는 팀 단위라 봇 간 공유 상태(Blackboard)가 필요 → 결정론·동시성 주의. Engine BT/Blackboard를 끌어쓰면 desync라 GameSim에 동등 타입을 새로 정의해야 함(중복 비용 vs 의존성 청결).
- **검증**: 로밍/오브젝트 참여 메트릭, 갱 콜/합류 trace.

### Phase F — (선택/연구) 학습형
- **무엇/왜**: `Decision` brain 실체화. 손으로 짠 룰의 한계를 데이터로 넘기.
- **어떻게**: imitation(인간 로그 → BC) → RL warm-start. 추론은 **baked ONNX artifact**로 주입, runtime은 순수 데이터로만 받음(결정론·리플레이 보존). `RLBridge` stub을 실제 추론으로 채움. ML Lab(`C:/Users/tnest/Desktop/NYPC`) 경계 준수.
- **Trade-off**: ONNX 추론이 결정론(부동소수 재현성)을 깰 위험 → 정수 양자화/고정 시드 필요. 권위 봇에 비결정 추론을 직접 넣으면 안 됨.
- **검증**: frozen baseline 대비 holdout 리그 우위 + 결정론·리플레이 유지.

### 안티패턴 (절대 안 함)
- `Engine/Public/AI`에 새 프레임워크 추가(레거시 이중화), 비권위 MCTS/BT/RLBridge를 **권위 봇에 끌어쓰기**(desync), `AIIntentComponent`식 한 겹 더 감싸기(Karpathy 단순성 위반).

---

## 7. 면접 예상 질문 & 모범 답변

### Q1 (기본). HFSM과 BT의 차이가 뭔가요? 왜 HFSM을 골랐나요?
**A.** FSM은 명시적 상태와 전이로 AI를 모델링해 "지금 어느 상태인지" 디버깅이 직관적입니다. BT는 행동을 Selector(OR)/Sequence(AND) 트리로 조합해 모듈성·재사용이 강하죠. 저는 라인전 봇에 HFSM(7상태) + Utility를 골랐는데, **결정론 보존과 디버깅 직관**이 1순위였기 때문입니다. 상태/점수/거부사유를 trace 링버퍼에 남기면 "왜 그 행동을 했나/왜 멈췄나"가 한눈에 보입니다. BT의 모듈성은 LaneCombat 안에서 손으로 짠 Selector(if 우선순위 체인)로 필요한 만큼만 흡수했습니다.

### Q2 (기본). Utility AI에서 이질적 선택지를 어떻게 비교하나요?
**A.** 공통 척도가 필요합니다. LoL은 모든 행동이 결국 골드/경험치 우위로 수렴하므로 **모든 가치를 골드로 환산**했습니다. 킬 300, 근접 미니언 21, 포탑 250골드, 경제 우위는 골드차+레벨차를 1000골드 풀스케일로 정규화. `ChampionFightValue`/`MinionFarmValue`/`StructureValue`가 이걸 0~1 스코어로 내고, brain이 `championScore ≥ farmScore + margin`으로 교전/파밍을 가릅니다.

### Q3 (설계 의도). 봇이 서버 내부인데 왜 HP를 직접 안 깎고 GameCommand만 내나요?
**A.** 그게 이 도메인의 북극성입니다. 봇이 truth를 직접 고치면 (1) 봇이 인간 못 할 짓(쿨다운 무시)을 할 수 있고 (2) 권위·결정론·리플레이·테스트 경로가 인간과 **두 갈래**가 됩니다. 봇을 "인간과 같은 GameCommand만 내는 가짜 플레이어"로 강제하면, 봇 명령도 인간과 **같은 CommandExecutor의 적법성 검사**(쿨다운/사거리/학습/타겟 생존)를 받아 모든 검증이 단일화됩니다. 미니언/터렛은 NPC라 truth를 직접 고치지만 챔피언 봇만큼은 이 모델을 지킵니다. 대신 1틱 지연이 생겨 콤보 설계가 까다로워지는 게 trade-off입니다.

### Q4 (설계 의도). brain을 왜 별도 인터페이스로 뺐나요?
**A.** "무엇을 할까(intent)"와 "어떻게 실행(Emit)"을 분리하기 위해서입니다. `IChampionAIBrain::DecideLaneCombatIntent`는 intent만 반환하고, 실행/결정론 인프라는 시스템이 고정합니다. 그러면 RuleBased/PlayerLike/Decision을 공용 코드 오염 없이 갈아끼울 수 있고, 미래의 학습 정책(Decision brain)도 같은 seam에서 주입됩니다. brain은 stateless고 상태는 전부 `ChampionAIComponent`에 둬서 재진입·공유가 안전합니다.

### Q5 (adversarial). "BT/MCTS/RL도 했다"고 적혀 있던데, 이거 진짜 봇이 쓰나요?
**A.** 아니요, **권위 봇은 안 씁니다.** 정직하게 말하면 `Engine/Private/AI/`의 BehaviorTree/MCTSPlanner/RLBridge는 **격리된 비권위 PoC/dead 경로**입니다. MCTS는 UCT 본문은 있지만 `std::random_device`로 비결정이고 `Plan()` 출력을 권위 봇 어디서도 안 읽습니다. RLBridge는 `LoadModel`/`Infer`가 무조건 false인 stub이고요. BT의 intent 큐는 소비자가 grep 0건, push만 됩니다. 셋 다 Client 로컬 `CWorld` 전용이라 권위 봇에 끌어오면 desync가 납니다. **의도적으로 분리한** 학습용 실험이고, 라이브 봇은 오직 `Shared/GameSim/Systems/ChampionAI/*`의 HFSM+Utility입니다. 정량적으로 어디까지가 production이고 어디부터 PoC인지 제 정직성 지도에 명시해 뒀습니다.

### Q6 (adversarial). "사람같은 봇(PlayerLike)"이 실제로 돌긴 하나요?
**A.** 코드는 있지만 **현재 라이브에서 실행되지 않습니다.** 솔직히 말하면 `GameRoomSpawn.cpp:770`이 난이도만 저장하고 `brainType`을 안 정해서, 스폰되는 봇은 전부 기본값 RuleBased로 떨어집니다 — PlayerLike 코드 경로에 도달을 못 해요. PlayerLike 자체는 `kCommitScale 1.5`로 태세 유지를 연장하고 HP 우위일 때만 교전하는 "사람다움의 1차 코드화"가 구현돼 있습니다. 이걸 활성화하는 게 제 로드맵 Phase A(배선)이고, 그 한 줄(brainType 매핑) + AIDebugPanel 표시면 도달합니다. "코드는 썼지만 스폰 배선이 빠져 도달 불가"라는 걸 제가 코드 감사로 먼저 잡아 둔 상태입니다.

### Q7 (adversarial). 난이도별 봇이 있나요? 봇 perception은 얼마나 정교한가요?
**A.** 둘 다 솔직히 한계가 있습니다. **난이도**는 `ai.difficulty`에 저장만 되고 의사결정에서 안 읽혀서 현재 차등이 0입니다. **perception**은 FOW/시야 gate가 없는 **전지적 스캔**이라 봇이 안개 속 적도 다 봅니다 — 사람다움·공정성과 정면 충돌하죠. 스킬샷도 현재 위치로만 조준하고 리드(예측 사격)가 없습니다. 이건 제가 문서에 스스로 한계로 명시했고, Phase C(시야 gate + 결정론 조준 오차 + 리드)에서 채울 작업입니다. "전지적이라 강한 게 아니라 사람답지 않아서 약점"이라는 진단이 먼저 있었습니다.

### Q8 (심화). 봇 결정의 결정론을 어떻게 보장하고 검증하나요?
**A.** 하드 제약으로 강제합니다: 난수는 `tc.pRng`(DeterministicRng, 시드 0xC0FFEE)만, 시간은 tickIndex/고정 dt(1/30)만, 비정렬 순회 금지, 동점은 EntityID로 타이브레이크, brain은 stateless. 검증은 `Tools/SimLab`의 헤드리스 러너가 same-seed로 두 번 돌려 **per-tick FNV 해시가 동일**한지 봅니다(다르면 exit 1). 다만 정직하게 — 그 해시는 pos/hp/골드 같은 시뮬 상태를 잡지 AI 내부 state를 직접 잡진 않아서, 봇 결정의 결정성은 "시뮬 결과가 동일해야 한다"는 제약을 통해 **간접 검증**됩니다. 그리고 SimLab은 navgrid 없는 평면 미러라 "서버 시뮬 코어 결정론"으로 한정해 말합니다.

### Q9 (심화). 운영(매크로) 의사결정은 어떻게 추가할 건가요? Engine에 BT/Blackboard 있는데 그걸 쓰면 되지 않나요?
**A.** 안 됩니다. Engine `Blackboard.h`/BT는 Client 로컬 비권위 `CWorld` 기반이라 권위 봇에 끌어오면 desync가 납니다. 그래서 **GameSim측에 동등한 Team Blackboard를 새로 정의**합니다 — focus target/objective/missing enemy last-known position을. 중복 타입을 만드는 비용은 있지만 의존성 방향(Shared가 Engine을 안 끌어옴)을 지키는 게 우선입니다. state/intent에 Roam/Objective/Teamfight/Back을 추가하고, 명령은 기존 Move/Recall에 더해 안 쓰던 BuyItem/LevelSkill로 성장·운영을 표현합니다. 맵 단위 정보(오브젝트 타이머/시야)는 Server에서 평탄화해 Context로 주입하고요.

### Q10 (심화). 학습형(Decision brain)으로 가면 결정론이 깨지지 않나요?
**A.** 그게 핵심 위험입니다. ONNX 추론은 부동소수 재현성이 플랫폼/스레드에 따라 흔들릴 수 있어 권위 봇에 **비결정 추론을 직접 넣으면 안 됩니다.** 그래서 학습은 오프라인(imitation→RL), 추론은 **baked artifact + 고정 시드/양자화**로 주입하고, runtime은 순수 데이터로만 정책 결과를 받습니다. `Decision` brain은 이미 그 주입 자리로 비워뒀고(현재 RuleBased 위임), `ChampionAIBrainInput`을 두껍게 해서 Utility 점수/influence를 넣어주면 됩니다. 즉 "학습은 밖에서, 결정론은 안에서" 경계를 긋습니다.

### Q11 (압박/심화). 콤보가 6챔피언만 있고 Yasuo/Jax는 공용 cpp에 하드코딩돼 있던데, 확장성 문제 아닌가요?
**A.** 맞습니다, 인정합니다. `TryExecuteYasuo*`/`TryExecuteJaxDive`가 공용 `ChampionAISystem`에 분기로 박혀 있는 건 **갚아야 할 기술 부채**입니다. 로스터가 늘면 공용 cpp가 오염되죠. 올바른 길은 챔프별 전술을 brain 또는 champion-owned hook으로 빼는 것이고(GAS 도메인의 함수포인터 훅 레지스트리와 같은 패턴), 신규 챔프는 그렇게 추가합니다. Yasuo/Jax는 "따라 하지 말 부채"로 표시해 두고 점진 이전 대상입니다. 콤보 데이터(`ChampionAIComboPlan`) 자체는 이미 constexpr 데이터로 분리돼 있어서, 데이터 외부화 사다리(constexpr→ImGui 튜너→JSON→BT asset)를 따라 올라갈 계획입니다.

---

## 8. 30초 엘리베이터 피치

"제 봇 AI의 핵심은 화려한 기법이 아니라 **한 가지 제약**입니다 — 봇은 truth를 직접 못 고치고, 사람과 똑같은 GameCommand만 내는 가짜 플레이어다. 이걸 코드로 강제하니까 안티치트·결정론·리플레이·테스트가 인간과 단일 경로로 묶입니다. 그 위에 7상태 HFSM과 모든 가치를 골드로 환산하는 Utility 스코어링, 그리고 intent만 결정하는 교체 가능한 brain(RuleBased/PlayerLike/Decision)을 올렸고, 14노브 런타임 튜닝과 결정 트레이스로 인게임에서 '왜 그 행동을 했나'를 눈으로 봅니다. 엔진에 BT/MCTS/RL도 있지만 **그건 비결정·비권위 PoC라 라이브 봇엔 의도적으로 안 붙였다**는 걸 제 정직성 지도에 명시해 뒀습니다. 다음은 전지적 perception을 시야 기반으로 바꾸고 운영(매크로) 의사결정을 채우는 겁니다 — 어디까지가 production이고 어디부터 계획인지 제가 코드로 그어 둔 게 강점입니다."
