Session - NYPC NEXT NATION의 결정론·Perception·회고 루프를 Winters ChronoBreak 기반 인간형 MOBA AI 연구 플랫폼으로 전환한다.

# Winters NYPC Humanlike AI Research Architecture

작성일: 2026-07-12  
적용 범위: NYPC battlefield/mushroom 연구 자산, Winters LoL `Shared/GameSim`, Server Champion AI, SimLab, Replay/ChronoBreak, Client AI Debug, Python 학습·평가 도구, 게임 AI 포트폴리오

## 0. 결론

이 방향은 채용 공고와 강하게 맞는다. 다만 목표는 “규칙 봇을 오래 깎은 프로젝트”가 아니라 아래 네 능력을 하나의 재현 가능한 시스템으로 증명하는 것이다.

```text
결정론적 게임 환경
-> 부분 관측 Perception과 상대 모델
-> 설명 가능한 후보 선택과 GameCommand
-> imitation / RL / self-play 학습
-> ChronoBreak 반사실 실험
-> holdout league와 promotion gate
-> 인게임 AI Debug와 플레이 가이드
```

현재 자산은 이 목표의 좋은 출발점이지만 완성된 RL 플랫폼은 아니다.

- NEXT NATION/battlefield의 강점은 `GameState -> Perception -> Offer -> EV -> 검증 명령`, 상대 행동 기억, replay, clone/fidelity, 회고, 승급 게이트다.
- Mushroom의 강점은 결정론 규칙, 탐색 teacher, pairwise imitation, manifest/hash, 반사실 강제 분기다.
- Winters의 강점은 서버 권위 30 Hz GameSim, 실제 `GameCommand` 집행, F9 AI Debug, SimLab, S015 checkpoint rewind다.
- 실제 production PPO/DQN/Gymnasium 학습 코드는 아직 없다. PyTorch는 toy 예제 두 개뿐이고 `claude_rl_tune.py`는 RL이 아니라 coordinate ascent + ES + GA다.
- 따라서 “RL 경험 완성”을 주장하기 전에 작은 실전 BC/DAgger/PPO 수직 슬라이스를 반드시 닫는다.

601개 Python 파일을 전부 Winters로 복사하지 않는다. 최소 351개가 session/tmp/scratch/patch 이력이고, 게임별 Python `GameState`를 런타임에 들이면 `Shared/GameSim`과 경쟁하는 두 번째 gameplay truth가 생긴다. 가져올 대상은 파일 묶음이 아니라 검증된 알고리즘·계약·실험 규율이다.

## 1. 채용 요구와 증명 산출물

| 채용 요구 | 이 프로젝트가 보여줘야 할 증거 | 현재 상태 |
|---|---|---|
| 강화학습·모방학습 게임 AI | 실제 BC/DAgger/PPO 학습 곡선, legal mask, holdout 평가, 배포 policy | typed episode와 NumPy pairwise supervised baseline까지 구현, PyTorch BC/DAgger/PPO는 미구현 |
| 플레이 가이드·행동 최적화 | 같은 policy를 `Control`과 `ShadowCoach` 모드로 실행하고 top-k 행동·근거·regret 출력 | 후보 점수와 block reason 일부만 존재 |
| 자율 행동·전략 의사결정 | 부분 관측 memory, micro/tactical/macro 계층, opponent response, GameCommand | lane utility + DefendMid 수직 슬라이스 존재 |
| 데이터 분석·평가 체계 | episode schema, replay, divergence, league, CI, promotion gate | Winters typed export/validator와 report gate 구현, 실제 league/divergence runner는 미구현 |
| Python·PyTorch | 재현 가능한 trainer/evaluator/artifact exporter | NumPy contract trainer만 구현, 실전 PyTorch trainer는 미구현 |
| 게임 프로젝트 | 서버 권위 C++ 엔진, 네트워크, nav, champion/minion/skill sim | 강한 증거 |
| 주도적 문제 해결 | 실패 가설, fixture, 반사실 분기, 회귀 게이트로 개선한 한 사례 | NYPC 회고는 강함, Winters 영상·리포트 환전 필요 |
| 코딩 AI 활용 | work packet, 소유권, 검증, 에이전트 비판을 포함한 사람 주도 개발 과정 | 협업 문서가 있으나 결과물 서사로 정리 필요 |

지원은 “5v5 RL 완성 뒤”로 미루지 않는다. 2026-07-15를 1차 외부 마감으로 두고 현재 결정론/NYPC/Winters 증거로 지원한 뒤, 아래 수직 슬라이스를 후속 포트폴리오 업데이트로 공개한다. 매주 시간의 최소 30%는 코드 바닥 작업이 아니라 영상, README, 실험표, 경력기술서, 실제 지원에 고정한다.

## 2. 현재 Winters 코드의 진실

### 2-1. 실제 권위 경로

```text
CGameRoom::Tick
-> Phase_ServerBotAI
-> CServerAICommandProducer::Execute
-> CChampionAISystem::Execute
-> m_pendingExecCommands
-> CDefaultCommandExecutor::ExecuteCommand
-> Simulation Systems
-> Snapshot/Event
```

봇과 플레이어가 같은 `GameCommand` 집행기를 통과하는 방향은 옳다. 학습 정책도 이 경계를 우회하지 않는다.

### 2-2. 현재 AI와 S017 research slice

- `ChampionAIPerception.h`는 tick, 관측 target, HP/level, 거리, 포탑 위험, capability를 담는다. V1의 `self_gold`/`enemy_gold` wire field에는 wallet gold가 아니라 관측 가능한 inventory purchase value가 들어간다.
- `ChampionAIResearchTypes.h`에는 POD observation, action mask, 네 candidate, command, executor result와 provenance를 가진 typed `AiDecisionTraceV1`가 있다.
- `ChampionAIValuation`은 retreat/fight/farm/siege 점수를 결정론적으로 만든다.
- `IChampionAIBrain`은 `RuleBased`, `PlayerLike`, `Decision` 세 유형을 가진다.
- `Decision` brain은 아직 `RuleBased`로 위임한다.
- 난이도 2 이상은 `GameRoomSpawn.cpp`에서 `PlayerLike`로 연결된다.
- 기존 F9 trace는 최종 점수와 명령 일부를 16행 ring에 남긴다. 별도의 transient `ChampionAIResearchDebugComponent`는 typed research trace와 influence instrumentation을 보유하며 checkpoint truth에서는 제외된다.
- 9x9 `ThreatNow`, `ThreatBelief`, `SupportEta`, `EscapeCost` map이 GameSim에 구현돼 native/live probe에서 계산된다. 아직 Brain/Valuation의 입력이 아니고 `AiEpisodeV1`이나 Client F9 overlay에도 직렬화되지 않는다.
- `AiEpisodeV1` native capture, canonical JSONL exporter/validator, promotion validator와 NumPy pairwise supervised imitation baseline이 구현돼 있다. 이 baseline은 PyTorch 또는 RL이 아니며 runtime policy를 바꾸지 않는다.

### 2-3. 부분 관측의 현재 수준과 결손

S017 champion AI observation은 allied `VisionSource`의 반경·원뿔 방향과
invisible/true-sight/concealment 조건으로 현재 적을 필터링하고, 관측이
끊긴 적은 5초 bounded last-seen memory로만 `ThreatBelief`에 반영한다.
`AiEpisodeV1`에서는 숨은 적의 current NetEntityId, level, HP ratio,
inventory value, distance를 canonical zero로 만든다. current wallet gold를
관측 feature로 사용하지 않는다.

하지만 이것은 완전한 FOW 보안 경계가 아니다.

- terrain/nav obstacle을 가리는 wall line-of-sight가 없다.
- team별 network snapshot confidentiality가 없어서 Client replication은 별도 해결이 필요하다.
- visibility를 모든 AI/targeting/skill query가 공유하는 team cache로 통일하지 않았다.
- last-seen belief의 route/cooldown 확률 모델과 confidence calibration ledger는 없다.

따라서 현재 수준은 “champion AI research observation filter + 5초 memory”이며
“게임 전체 권위 FOW”로 주장하지 않는다.

### 2-4. 현재 ChronoBreak와 WRPL의 정확한 수준

S015는 중요한 checkpoint foundation이다.

- 등록된 GameSim component store, Entity allocator, RNG, EntityIdMap을 저장·복원한다.
- server room은 minion wave state, turret accumulator, practice enabled를 추가 보존한다.
- F10에서 과거 1초 간격 keyframe으로 돌아가 일시정지 착지한다.
- SimLab fixture는 save/restore 후 동일 tick hash를 증명한다.
- snapshot에는 `timelineEpoch`, `branchId`, `toolRevision`, pause/speed metadata가 append-only로 실리고, 성공한 rewind 뒤 epoch/branch가 증가한다.
- Client는 epoch/branch 변경을 감지해 prediction/interpolation/action/locomotion/hover/event dedupe/FX 상태를 rebase한다.
- WRPL v2에는 command payload domain과 tool revision journal foundation이 있으며 v1 reader 호환 경계가 있다.

그러나 아직 완전한 반사실 branch orchestrator는 아니다.

- target tick까지 external journal을 재실행하지 않고 가장 가까운 keyframe으로만 복원한다.
- checkpoint부터 target tick까지 external command journal을 exact re-simulate하지 않는다.
- faithful fixed-input branch와 reactive policy A/B orchestrator, 최초 divergence report가 없다.
- replay loader는 branch-aware non-monotonic scheduling 대신 비단조 record를 거절한다.
- spatial index는 복원 후 재구축되고 server-only phase 전체 골든 하네스가 없다.
- 일부 room-level state와 network/presentation cache의 branch 정책이 미완료다.
- 현재 단일 mutable RNG는 정책 A/B가 다른 수의 난수를 소비하면 비교군 공통 난수를 깨뜨릴 수 있다.

따라서 포트폴리오에서 현재 상태를 “전체 서버 월드 완전 ChronoBranch”로 과장하지 않고 “GameSim checkpoint rewind + timeline identity/client rebase + WRPL v2 foundation, exact journal branch 실험 진행 중”으로 설명한다.

## 3. NEXT NATION과 LoL 봇의 본질 차이

| 축 | NEXT NATION / battlefield | Winters LoL | 설계 결과 |
|---|---|---|---|
| 정보 | 완전정보 | 부분 관측·안개·낡아지는 기억 | Fact와 Belief를 분리 |
| 시간 | turn/day 단위 동시수 | 30 Hz 실시간 + 이벤트 인터럽트 | 실행·전술·매크로 cadence 분리 |
| 공간 | 작은 그래프·거점 | 연속 XZ/navmesh·사거리·투사체 | ETA field와 local geometry 필요 |
| 행동 | WAIT/CLAIM/LABOR/RUSH/DEFEND 등 | Move/BA/QWER/Flash/Recall + combo | 계층적 action head와 legal mask 필요 |
| 커밋 | 이동 중 재명령 불가 | 이동 취소 가능, skill phase/자원은 커밋 | option recovery time으로 재정의 |
| 상대 추론 | 지난 turn 행동 완전 관측 | last seen·cooldown belief·경로 확률 | PerceptionMemory와 confidence 필요 |
| 탐색 | exact rollout이 비교적 저렴 | 긴 지평·큰 branching·150 champion | 제한 후보 + learned value + 짧은 rollout |
| 제어 | 한 agent가 전체 군대 배정 | 5개의 actor와 team objective | decentralized command + team blackboard |
| 보상 | HQ HP/파괴가 명확 | Nexus sparse terminal + 많은 중간 가치 | terminal 중심, potential shaping 감사 |
| 사람다움 | 승리 최적화 중심 | 반응 지연·APM·실수·의도 유지도 품질 | strength와 human-likeness를 별도 평가 |
| 런타임 | 단일 제출 C++ | Server/Client/Network/Presentation | policy도 server authority와 replication 준수 |

## 4. NYPC에서 가져올 본질

### 4-1. `commit`

NEXT NATION의 commit은 행동 enum이 아니라 “다른 자산에 다시 도달 가능한 시간 `tau`가 늦어져 옵션 가치가 사라지는 현상”이었다. LoL에서는 다음을 같은 단위로 가격화한다.

```text
CommitCost
= cast windup / recovery
+ cooldown and mana spent
+ summoner spell spent
+ dash landing displacement
+ channel interrupt exposure
+ lane/vision position abandoned
+ safe-state recovery ETA
```

이 값은 `intentHoldTimer` 하나로 끝나지 않는다. 각 후보의 `commitUntilTick`, `interruptPolicy`, `recoveryETA`, `opportunityCost`를 trace에 남긴다.

### 4-2. `gold0`

NYPC의 “죽는 골드 0”은 지갑을 무조건 비우는 규칙이 아니라 잉여를 병력·레벨·전술 옵션으로 바꾸되 도달 불가능한 저축에 묶지 않는 원리다. LoL에서는 다음 함수로 바뀐다.

```text
SpendOrHoldValue
= immediate combat power
+ item completion spike
+ objective timing value
- recall travel and channel time
- missed wave XP/gold
- death/interruption risk
```

미사용 골드는 귀환 전 소비 불가능하므로 `cash == waste`를 하드코딩하지 않는다.

### 4-3. `convergence`

NYPC의 수렴은 ETA 안에 충분한 질량이 모이는지 계산하고, 이길 수 없는 수비에 한 기씩 헌납하지 않는 `NOFEED` 산수다. LoL에서는 다음을 계산한다.

- ally/enemy arrival ETA distribution
- 유효 체력과 예상 burst/DPS
- CC/ultimate/summoner availability
- retreat route와 reinforcements
- wave/structure/objective deadline

새 `Converge` 행동을 중복 추가하기보다 기존 DEFEND/CONTEST/ENGAGE 후보가 같은 도착·전투 가치 함수를 공유한다.

### 4-4. `안녕하세요민이입니다`

민이 봇의 핵심은 중앙 좌표가 아니라 “중앙에 응답 옵션을 저장한 뒤 상대 커밋과 경제 붕괴 때 현금화”하는 함수였다. 보이는 BANK/ROLL/FINISH 단계를 복제한 후보는 전체 상대 풀에서 회귀했다. LoL에서도 상위 플레이의 표면 행동을 외우지 않고 다음을 학습한다.

```text
관측된 행동
-> 그 행동이 보존한 옵션
-> 상대가 잃은 옵션
-> 발동 조건과 신뢰도
-> 결과 가치
```

### 4-5. NYPC를 넘어서는 지점

NYPC playback clone은 상대 명령을 고정하므로 수정 후 상대의 재판단을 검증하지 못했다. Winters는 ChronoBreak로 두 모드를 분리한다.

1. Faithful reproduction: 외부 입력과 정책 revision을 고정해 버그 순간을 재현한다.
2. Counterfactual response: 같은 checkpoint에서 선택 policy 또는 양측 bot policy를 바꾸고 다시 판단시킨다.

두 branch의 최초 divergence를 Fact부터 결과까지 한 열로 비교한다.

## 5. 목표 AI 아키텍처

```text
Authoritative World Fact
-> Team Observation Filter
-> Perception Memory / Belief
-> Derived Situation Features
-> Influence Layers
-> Hierarchical Candidate Generation
-> Hard Feasibility + Action Mask
-> Utility / Policy / Value / Risk
-> Commitment Controller
-> One Intent / Mechanical Plan
-> Atomic GameCommand
-> Authoritative Executor Result
-> Decision Ledger / Episode / Replay
```

### 5-1. 세 개의 판단 주기

| 계층 | 기본 주기 | 역할 |
|---|---:|---|
| Execution safety | 30 Hz | capability, CC, impact, target validity, command acceptance |
| Micro/Tactical | 5~10 Hz | BA/QWER/Flash, kite, trade, retreat, combo |
| Macro | 1~2 Hz + event interrupt | wave, recall, buy, rotate, objective, group |

모든 후보를 30 Hz마다 처음부터 재계산하지 않는다. damage, death, hard CC, target reveal, objective transition 같은 event만 즉시 재평가를 깨운다.

### 5-2. Fact, Observation, Belief

```text
Fact
  server truth. bot에게 직접 전부 노출하지 않음.

Observation
  team vision으로 지금 확인 가능한 사실.

Belief
  lastSeenTick/position/velocity, route hypotheses,
  cooldown and recall probability, confidence, ETA envelope.

Perception
  Observation + Belief에서 결정용으로 파생한 값.
```

belief는 `Observed`, `StrongInference`, `WeakInference`, `Unknown` 같은 신뢰도 타입을 가진다. 약한 추론 하나가 높은 비용의 engage를 단독 발동하지 못하도록 candidate별 evidence threshold를 둔다.

### 5-3. 행동 계층

```text
Strategic Objective
  FarmLane / RecallBuy / Rotate / Defend / Contest / Siege

Tactical Intent
  Trade / AllIn / Peel / Retreat / Zone / Hold

Mechanical Plan
  Combo1 / Kite / Chase / LastHit / SkillshotLead

Atomic Command
  Move / BasicAttack / Q / W / E / R / Flash / Recall / BuyItem
```

ATTACK, DEFEND, Q, R, Combo1을 한 enum에서 같은 후보로 비교하지 않는다. 상위 선택이 하위 후보 집합을 만들고, 최종 결과만 `GameCommand`가 된다.

### 5-4. Policy mode

동일 observation/candidate 계약 위에 네 모드를 둔다.

- `RuleBased`: 현재 안전 baseline.
- `PlayerLike`: 반응·commitment·실행 노이즈를 가진 규칙 policy.
- `LearnedControl`: 검증된 artifact가 실제 candidate를 선택.
- `ShadowCoach`: 실제 명령은 내지 않고 top-k 행동, 근거, 예상 가치, confidence를 플레이 가이드로 출력.

`ShadowCoach`는 채용 공고의 플레이 가이드 생성과 행동 최적화를 같은 연구 자산으로 증명한다.

## 6. Influence Map의 실제 설계

단일 색 히트맵 하나를 만들지 않는다. 의미가 다른 값을 합치면 원인을 설명하거나 학습 feature를 감사할 수 없다.

현재 구현된 v1은 bot-local 9x9 `ThreatNow`, `ThreatBelief`, `SupportEta`,
`EscapeCost` 네 layer의 transient instrumentation이다. source ordering,
capacity cap, team-filter provenance와 all-walkable nav query를 probe로
검증하지만, 아래 표 전체를 구현한 상태는 아니다. 특히 현재
`ThreatNow`의 source coverage와 `SupportEta` 산식은 연구용 수직 슬라이스이며
실제 policy feature나 Client UI가 아니다.

| Layer | 의미 | 주요 source | cadence |
|---|---|---|---:|
| ThreatNow | 지금 즉시 받을 수 있는 피해/CC | visible units, turret, projectile | 5~10 Hz/event |
| ThreatBelief | 숨은 적이 도달할 확률 질량 | last seen, routes, movement speed | 2~5 Hz |
| AllySupportETA | 아군 지원 도착 시간 | ally path/travel time | 2~5 Hz |
| EnemyArrivalETA | 적 증원 도착 envelope | observed/belief path | 2~5 Hz |
| VisionControl | 보이는 곳, 재확인된 곳, 미확인 시간 | team vision/ward | event/2 Hz |
| EscapeCost | 안전 지점까지 path cost와 차단 위험 | nav + threat | 5 Hz |
| WaveValue | XP/gold/pressure와 도착 deadline | minion wave | 1~2 Hz |
| ObjectiveValue | turret/jungle/nexus/recall timing | objective state | 1 Hz/event |

핵심 규칙:

- gameplay layer는 Server/Shared 결정론 경로에서 CPU로 계산한다. GPU compute map은 visualization 전용이며 AI truth가 될 수 없다.
- Euclidean Gaussian보다 nav travel time을 우선한다.
- team-shared coarse global field와 bot-local high-resolution crop을 분리한다.
- 512x512 전 레이어를 bot마다 30 Hz로 재계산하지 않는다.
- layer는 quantized stable representation과 revision을 갖고 episode에 저장한다.
- Server는 Engine navgrid 타입을 Shared에 include하지 않고 `IChampionAIEnvironmentQuery` 같은 read-only adapter로 평탄화한다.
- Client는 subscribe한 bot/layer/ROI debug data만 받아 그린다.

대표식은 다음처럼 해석 가능한 형태를 유지한다.

```text
SourceInfluence(cell)
= sourcePower
 * Reachability(cell)
 * ExpDecay(TravelETA(source, cell), tau)
 * ObservationConfidence
```

## 7. Candidate, 가치, trace

### 7-1. hard gate가 먼저다

다음은 점수의 큰 음수가 아니라 후보 제거 조건이다.

- capability/CC/사망
- legal target/action mask
- nav/path 불가
- cooldown/mana/item 불가
- stale action generation
- deterministic ownership conflict

그 뒤 효용을 비교한다.

```text
CandidateValue
= win-probability proxy
+ survival
+ expected damage/kill
+ XP/gold
+ objective/tempo
+ information/position option
- death risk
- missed farm/travel opportunity
- cooldown/mana/summoner cost
- uncertainty penalty
- commitment recovery cost
```

`내 HP - 적 HP`는 전투 feature 하나이지 전체 목적 함수가 아니다.

### 7-2. Decision Ledger

S017의 현재 typed V1은 observation/provenance, action mask, Retreat/Fight/
Farm/Siege 후보, 선택 후보, emitted command와 executor result를 bounded
transient ring과 episode exporter로 연결했다. 숨은 적 current fact는 zero로
정규화한다. Belief 세부 evidence, influence samples, weight contribution,
commit cost, next observation을 아직 담지 않으므로 아래는 V2 목표다.

```text
episodeId / scenarioId
tick / timelineEpoch / branchId
rulesHash / definitionHash / policyRevision / tuningRevision
rngSeed / namedRandomKey
observationHash
Fact references visible to debugger only
Observation fields
Belief fields and confidence
Influence layer samples/revisions
candidates[]
  action and target
  actionMask / hardGate / rejectReason
  rawFeatures[]
  weights[]
  contributions[]
  risk / opportunity / commitCost
  finalScore / rank
selectedCandidate
commitmentBefore / commitmentAfter
emittedCommand
executorAccepted / executorReason
nextStateHash / reward / terminal
```

선택한 bot만 상세 trace를 subscribe하고 Server는 장기 ring 또는 streaming sink를 소유한다. Snapshot마다 모든 bot의 장기 trace를 반복 전송하지 않는다.

## 8. ChronoBreak AI Laboratory

### 8-1. 세 실험 모드

1. Read-only re-score
   - 같은 Perception/candidate를 다른 weight로 즉시 재평가한다.
   - world를 돌리지 않아 수식 영향 확인이 빠르다.

2. Fixed-input faithful branch
   - player/external journal과 policy/data revision을 고정한다.
   - 버그의 최초 divergence와 재현성을 증명한다.

3. Reactive counterfactual branch
   - 같은 checkpoint에서 policy A/B를 적용하고 선택 bot 또는 양측 bot을 재결정한다.
   - 상대의 반응 변화까지 평가한다.

### 8-2. 비교 공정성

단일 mutable RNG를 그대로 두면 policy B가 난수 한 번을 더 소비한 것만으로 전투 RNG까지 달라질 수 있다. AI 실험은 named/counter-based random key를 사용한다.

```text
RandomKey = Hash(matchSeed, subsystem, entityNetId, tick, decisionOrdinal, purposeTag)
```

combat RNG와 AI sampling RNG를 분리하고 branch manifest에 seed/substream revision을 저장한다.

### 8-3. A/B 결과

- first divergence tick
- observation/belief/influence difference
- candidate rank difference
- command and executor result
- delta HP/gold/XP/CS/death/objective
- intent churn/reaction time/APM
- terminal result와 confidence interval

## 9. 학습 환경과 모델

### 9-1. 환경 원칙

Python은 Winters gameplay transition을 다시 구현하지 않는다.

```text
Winters Shared/GameSim headless episode
-> versioned JSONL/binary dataset
-> Python training/evaluation
-> versioned PolicyArtifact
-> Server/Shared Decision brain
-> legal mask + GameCommand executor
```

Python proxy simulator는 빠른 연구 근사로 사용할 수 있지만 promotion truth는 반드시 GameSim episode다.

현재 구현된 `AiEpisodeV1`은 Accepted/Rejected decision event를 보존하는
BC/ranking 계약이다. live smoke는 정확히 빌드된 `SimLab.exe`의 SHA-256을
`rules_hash`로 사용하고, 240 tick time limit의 마지막 Accepted 결정을
강제로 만든 뒤 `truncated=true`로 닫는다. dense per-tick observation,
explicit next observation, elapsed-step discount와 recurrent hidden state가
없으므로 PPO trajectory로 사용하지 않는다.

현재 trainer는 promotion-valid Accepted record만 소비하는 결정론적 NumPy
pairwise logistic ranker다. `(scenario_id, rules_hash, definition_hash)`를
frozen split group으로 사용하며 authored candidate score/reward를 feature로
먹지 않는다. 이것은 supervised contract baseline이고 PyTorch BC, DAgger,
PPO, self-play, runtime artifact promotion은 아직 없다.

### 9-2. curriculum

1. legal action/capability scenario
2. 1v1 basic attack + 단일 skill
3. last hit/trade/retreat
4. full 1v1 lane matchup
5. 2v2 reinforce/peel
6. wave/recall/buy/rotate
7. 5v5 scripted macro
8. learned multi-agent self-play

### 9-3. 학습 순서

1. Pairwise imitation baseline
   - NYPC의 group split/regret/top-1 방식을 새 schema로 이식.
2. Behavior Cloning
   - scripted/search/human teacher의 legal candidate 분포 학습.
3. DAgger
   - learned policy가 방문한 실패 state를 teacher가 다시 label.
4. Recurrent PPO
   - action masking, GAE, clipped objective, value/entropy loss, truncated sequence.
5. League self-play
   - frozen baselines, past selves, exploiters, champion archetype pool.
6. Distillation
   - 작은 runtime policy로 압축하고 Python/C++ inference parity 검증.

5v5 전면 RL부터 시작하지 않는다. 첫 실제 증명은 한 챔피언의 1v1 lane micro다.

### 9-4. observation와 action

초기 observation:

- self stats/resource/cooldown/status/action phase
- visible top-K entities + relative geometry
- last-seen belief summary
- local influence crop
- wave/turret/objective summary
- active intent/commitment

초기 action은 후보 선택형으로 제한한다.

- candidate ID
- target entity 또는 quantized move cell
- optional skill slot/stage

정책이 임의 world 좌표와 gameplay result를 직접 출력하지 않는다.

### 9-5. reward

terminal Nexus 결과가 북극성이다. dense reward는 potential difference로 감사한다.

```text
Reward
= terminal win/loss
+ Delta(team economy and XP potential)
+ Delta(structure/objective potential)
+ bounded combat outcome
- death and invalid command
- optional human-likeness regularizer
```

reward hacking replay를 자동 분류하고, 새 reward는 frozen scenario에서 이전 정책을 역평가한다.

## 10. NYPC Python selective bridge

### 10-1. Extract/Adapt

| 원본 | 추출할 본질 | Winters 대상 |
|---|---|---|
| `mushroom/scripts/position_loop_manifest.py` | SHA-256, atomic artifact manifest | Python ArtifactManifest |
| `mushroom/scripts/promotion_gate_codex.py` | mirrored side, repeat determinism, holdout promotion | PolicyPromotionGate |
| `mushroom/scripts/train_codex_action_model.py` | group split, pairwise rank, regret/top-1 | ImitationRankingBaseline |
| `battlefield/tools/belief_fact_ledger.py` | belief와 미래 fact의 FP/MISS 교정 | PerceptionCalibrationLedger |
| `mushroom/scripts/run_s338_forced_branch_counterfactual.py` | 동일 state 강제 분기 비교 | ChronoCounterfactualRunner |
| `battlefield/tools/trace_match.py` | per-tick JSONL/flush/state-action-event 연결 | EpisodeExporter schema |
| `battlefield/tools/claude_turn_inspect.py` | Fact/Perception/EV/결과 UX | F9 AI Lab view model |
| `battlefield/tools/lab_selfplay.py` | mirrored league, frozen opponent, fault rank | Offline League runner |
| `battlefield/tools/claude_rl_tune.py` | ES/GA black-box baseline | RL이 아닌 비교군 |

각 항목은 새 모듈로 추출하고 원본 game semantics, regex parser, absolute path, subprocess protocol을 제거한다.

### 10-2. Reference only

- `battlefield/engine/{rules,features,tuned_policy,mcts}.py`
- battlefield C++ `main_codex.cpp`의 `GameState/Perception/Offer/pairEv/ledger`
- `mushroom/lab/games/mushroom/*`
- game-specific oracle, bot, replay parser

구조와 실패 교훈만 참고하고 LoL 코드로 복사하지 않는다.

### 10-3. Do not copy

- mushroom session/probe/patch 318개
- battlefield root tmp 24개와 scratch 9개
- submission/bot/archive 사본
- official/third-party tool
- 특정 board/opponent/log 경로를 하드코딩한 runner

### 10-4. Bridge manifest

```text
sourceRepoRevision
sourcePath
sourceSha256
provenanceAndLicense
disposition: Extract | Adapt | Reference | DoNotCopy
reusableSymbol
targetOwner
targetContract
forbiddenDependencies
observationSchemaVersion
actionSchemaVersion
policyRevision
deterministicFixture
goldenOutputHash
promotionGate
rollbackOrDeletePath
```

## 11. 평가 체계

### 11-1. correctness

- invalid/illegal command 0
- same seed/input/policy hash parity
- replay final state hash parity
- Python/C++ inference parity
- FOW privileged feature leakage 0

### 11-2. strength

- win rate/Elo with confidence interval
- CS@10, gold/XP diff, kill/death, turret/objective conversion
- frozen baseline/past-self/exploiter pool
- seen/holdout champion, side, seed, scenario split

### 11-3. human-likeness

- reaction time distribution
- APM/action interval
- intent churn
- skillshot accuracy and miss pattern
- path/spacing distribution
- recall/buy timing
- human trace와의 Wasserstein/KL 계열 거리

강함과 사람다움을 하나의 점수로 숨기지 않는다. 두 축과 safety를 별도 표로 공개한다.

### 11-4. engineering budget

- 30 Hz server tick 33.33 ms budget을 침범하지 않음
- micro inference/decision p50/p95/p99
- ten-bot aggregate AI cost
- influence layer update cost와 bytes
- episode exporter backpressure/drop count

AI instrumentation만 추가하고 최적화로 계산하지 않는다.

## 12. 구현 순서

### 12-0. 2026-07-13 코드 반영 상태

| 항목 | 상태 | 정확한 경계 |
|---|---|---|
| selective bridge manifest/validator | 구현 | source SHA/provenance/DoNotCopy 검증 |
| typed decision/executor/AiEpisodeV1 | 구현 | 네 candidate의 decision-event BC/ranking 계약 |
| promotion report gate | 구현 | report 검증만 수행, policy/league state 변경 없음 |
| NumPy pairwise imitation baseline | 구현 | supervised offline baseline, PyTorch/RL 아님 |
| champion AI observation FOW/memory | 부분 구현 | radial/cone/concealment + 5초 memory; terrain LOS/network FOW 없음 |
| Influence v1 | instrumentation 구현 | 9x9 네 layer; policy/episode/UI 미연결 |
| timeline identity/client rebase/WRPL v2 | foundation 구현 | exact journal re-sim/reactive A/B 미구현 |
| BC/DAgger/PPO/self-play/league | 미구현 | measured corpus와 PyTorch pipeline부터 필요 |
| ShadowCoach | 미구현 | learned policy/artifact 계약 이후 |

이 표는 아래의 원래 gate 계획 및 `CONFIRM_NEEDED` code-preview보다 최신인
authoritative 상태다. 표의 “구현”만으로 빌드·테스트 성공을 추론하지 않고,
바로 아래의 날짜·명령이 있는 validation 기록으로 범위를 한정한다.

2026-07-13에는 full validation(`SimTicks=1800`, `Seed=42`)과 Server/Client
Debug x64 빌드가 통과했다. live episode 두 run의 canonical JSONL SHA-256은
`BECC9A151ABD8C24E45D79C787EA37FDB7BAFFA513896723128F912C98B0130E`로
일치했고, replay FlatBuffer fail-closed 변경 뒤 Client도 다시 빌드됐다.
이는 foundation 계약/빌드 증거이지 AI strength나 RL 완료 증거가 아니다.

### Gate 0. S014/S015 정확성 봉인

- AIDebug host/capability와 issuer life 분리
- single-param override가 whole profile을 덮는 회귀 수정
- timelineEpoch/branchId/full snapshot rebase
- replay journal domain과 non-monotonic branch 처리
- server-only participant/state hash 확대
- named AI random substream

### Gate 1. Observation/Decision Ledger vertical slice

- 한 bot의 versioned observation/action mask/candidate trace
- selected-bot subscription
- authoritative JSONL export
- read-only re-score
- Perception calibration ledger

### Gate 2. 부분 관측과 Influence v1

- server team visibility truth
- PerceptionMemory/last seen/confidence
- ThreatNow/ThreatBelief/SupportETA/EscapeCost 네 layer
- F9 overlay와 trace source drill-down

### Gate 3. Chrono A/B

- faithful vs reactive branch
- fixed external journal + policy revision manifest
- first divergence table와 metric diff

### Gate 4. 실제 IL

- 한 챔피언/한 lane scenario corpus
- pairwise baseline + BC + DAgger
- PolicyArtifact v1과 C++ parity
- ShadowCoach top-k guide

### Gate 5. 실제 RL

- recurrent PPO 1v1
- curriculum/self-play opponent pool
- holdout promotion
- reward hacking report

### Gate 6. macro/team

- team blackboard
- arrival/convergence model
- recall/buy/rotate/objective
- 2v2 후 5v5 확장

## 13. 첫 포트폴리오 장면

3~5분 영상 한 편이 다음 루프를 보여줘야 한다.

```text
1. 봇이 결정을 내린 tick에서 Pause
2. Fact / Observation / Belief / Influence layer 표시
3. 모든 후보의 mask, 점수 항, 차선 후보 표시
4. weight 또는 policy A/B 선택
5. Read-only re-score로 즉시 순위 변화 확인
6. ChronoBreak로 같은 checkpoint 복원
7. reactive counterfactual branch 실행
8. 상대의 새 반응과 first divergence/결과 metric 비교
9. 같은 policy를 ShadowCoach로 바꿔 플레이 가이드 출력
```

포트폴리오 문장은 “강한 봇을 만들었다”보다 다음이 정확하다.

```text
서버 권위 MOBA GameSim 위에 부분 관측 AI의 결정 근거를 추적하고,
동일 checkpoint에서 정책을 반사실 비교하며,
imitation/RL 정책을 holdout league로 승급하는 연구·디버깅 플랫폼을 설계·구현했다.
```

## 14. 30% ceiling과 외부 마감

### 2026-07-15까지

- 현재 NYPC/Winters 내용을 정직하게 반영한 경력기술서 제출
- 1페이지 architecture figure
- 기존 F9/S015 checkpoint 영상 60~90초
- “현재 규칙/IL 기반, PPO vertical slice 진행 중”을 명시

### 매주 배분

```text
70% foundation
  observation, FOW, trace, training, evaluation, deployment

30% ceiling
  실제 지원, README, 영상, 실험표, 기술 글, 면접 설명
```

한 트랙의 바닥 작업이 세 세션 연속 이어지면 다음 질문을 강제로 건다.

```text
이번 주에 채용 담당자가 직접 볼 수 있게 환전된 산출물은 무엇인가?
```

## 15. 금지

- Python transition model을 Winters runtime truth로 사용하지 않는다.
- 601개 `.py`를 일괄 복사하지 않는다.
- `claude_rl_tune.py`를 RL 경험으로 주장하지 않는다.
- 서버에 arbitrary Python/subprocess/deepcopy rollout을 넣지 않는다.
- GPU influence texture를 gameplay truth로 사용하지 않는다.
- 숨은 적의 실제 위치/골드/cooldown을 observation에 유출하지 않는다.
- 모든 판단을 `self HP - enemy HP` 하나로 압축하지 않는다.
- strength와 human-likeness를 한 metric으로 숨기지 않는다.
- fixed playback 승리를 adaptive 상대 반응 검증으로 오인하지 않는다.
- 5v5 PPO부터 시작하지 않는다.
- 완성할 때까지 지원을 미루지 않는다.

## 16. 완료 정의

- GameSim이 observation/action mask/state hash의 단일 truth다.
- 같은 seed/input/policy revision이 같은 episode hash를 만든다.
- bot은 team vision 밖의 current fact를 사용하지 않는다.
- candidate별 gate, feature, weight, contribution, commit, executor result가 추적된다.
- Influence layer가 의미별로 분리되고 source/ETA/confidence를 설명한다.
- Chrono faithful branch는 bit/state parity를, reactive branch는 first divergence를 보여준다.
- 실제 PyTorch BC/DAgger/PPO 중 최소 하나가 toy가 아닌 Winters scenario에서 학습된다.
- learned policy는 frozen holdout league를 통과하고 invalid command가 0이다.
- runtime policy도 최종 `GameCommand` validator를 통과한다.
- ShadowCoach가 같은 policy의 top-k 행동과 근거를 출력한다.
- 포트폴리오 영상, README, 실험표, 경력기술서가 실제 지원에 사용된다.
