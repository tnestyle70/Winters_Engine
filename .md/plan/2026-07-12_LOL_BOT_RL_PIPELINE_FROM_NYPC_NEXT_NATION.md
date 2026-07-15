# LoL Bot RL Pipeline From NYPC / NEXT NATION

Session - NYPC 버섯게임과 NEXT NATION 전장 봇에서 얻은 결정론 환경·utility·self-play·평가 자산을 Winters LoL Bot의 `환경 -> 모방 -> self-play -> 평가` 학습 파이프라인으로 환전한다.

작성일: 2026-07-12  
성격: 분석·설계 박제 문서  
적용 범위: NYPC mushroom, NYPC battlefield/NEXT NATION, Winters `Shared/GameSim`, Server Champion AI, Python/PyTorch 학습 파이프라인, Bot 포트폴리오 서사

## 0. 결론

이 문서의 본체는 "봇을 잘 만든다"가 아니다.

본체는 아래 파이프라인이다.

```text
Environment
-> Imitation
-> Self-Play
-> Evaluation
-> Promotion
-> Runtime Deployment
```

Influence map / utility 봇은 최종 목적이 아니라 다음 역할을 한다.

```text
1. baseline
2. teacher
3. sparring opponent
4. label generator
5. debug oracle
6. learned policy의 safety guardrail
```

PyTorch 정책 하나가 이 파이프라인 위에서 학습되고, holdout 평가를 통과하고, Winters Server에서 `GameCommand`를 내는 순간 채용 공고에서 요구하는 다음 역량이 모두 실증된다.

```text
게임 환경 모델링
상태/행동/보상 설계
모방학습
강화학습
self-play
평가/승급 게이트
Python/PyTorch
C++ 서버 권위 GameSim 연동
AI Debug / 플레이 가이드
```

단, 엄밀하게 말하면 현재까지의 NEXT NATION / battlefield에는 production RL policy가 있었다고 말하면 안 된다. `claude_rl_tune.py`라는 이름은 있지만 실제 내용은 policy gradient나 Q-learning이 아니라 deterministic self-play 기반 black-box parameter tuning이다.

정확한 표현:

```text
NEXT NATION / battlefield:
  deterministic environment + utility policy + self-play league + trace/autopsy + black-box tuning.
  RL-ready pipeline의 전단계는 강하게 존재했다.
  strict RL/production PyTorch policy는 없었다.

Mushroom:
  deterministic game model + fixed-depth search + oracle regret + evolutionary tuning + self-play factory + distillation 방향.
  일부 imitation/value distillation 개념과 작은 ML 실험은 있었지만, LoL급 PPO/RL 완성품은 아니었다.

Winters LoL Bot:
  여기서 처음으로 Environment -> Imitation -> Self-Play -> Evaluation -> PolicyArtifact를 server-authoritative GameSim 위에 연결한다.
```

## 1. 용어 판정표

이번 문서에서 용어를 엄격하게 나눈다.

| 이름 | 정의 | NYPC/NEXT NATION 상태 | LoL Bot 적용 |
|---|---|---|---|
| Utility Bot | 사람이 설계한 feature와 weight로 가장 좋은 행동을 고르는 봇 | 존재 | baseline, teacher, guardrail |
| Influence Map | 공간 위험/기회/지원/도착시간을 field로 만든 것 | 개념과 일부 analog 존재 | Perception feature, debug, policy input |
| Self-Play | 봇끼리 반복 대전해 데이터와 평가를 만드는 것 | 존재 | league, data collection, PPO workers |
| Black-box Tuning | weight/knob를 대전 결과로 최적화 | 존재 | rule baseline 튜닝, 대조군 |
| Evolution / GA / ES | population 또는 mutation으로 weight를 고르는 최적화 | mushroom/battlefield에 존재 | reward 설계 전 baseline optimizer |
| Imitation Learning | teacher 행동을 supervised learning으로 따라 배우는 것 | 일부 개념/도구 존재 | BC, pairwise ranking, DAgger |
| Reinforcement Learning | trajectory reward로 policy/value를 업데이트하는 것 | production 수준은 없음 | PPO/recurrent PPO 수직 슬라이스 |
| Evaluation Gate | holdout, side swap, previous best, fault 0으로 승급 여부 결정 | 강하게 존재 | policy promotion gate |

가장 중요한 판정:

```text
map size별 최적 base 수를 찾는 sweep/tuning은 RL이 아니다.
그것은 empirical parameter search 또는 black-box optimization이다.
```

RL이라고 부르려면 최소한 다음이 있어야 한다.

```text
state/observation
action
reward
episode trajectory
policy/value update
exploration 또는 policy improvement
held-out evaluation
```

NEXT NATION의 `GameState -> Perception -> Behavior -> highest gold action` 구조는 ML/RL 이전의 강한 symbolic/utility decision system이다. 그것 자체는 RL이 아니지만, RL이 학습할 환경·feature·baseline·teacher를 제공한다.

## 2. 현재 코드 증거

Winters의 현재 봇 경로는 이미 server-authoritative AI 학습 파이프라인의 좋은 착지점이다.

실제 파일:

```text
Server/Private/Game/GameRoomChampionAI.cpp
Server/Private/Game/ServerAICommandProducer.cpp
Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp
Shared/GameSim/Systems/ChampionAI/ChampionAIPerception.h
Shared/GameSim/Systems/ChampionAI/ChampionAIValuation.h
Shared/GameSim/Systems/ChampionAI/ChampionAIValuation.cpp
Shared/GameSim/Systems/ChampionAI/ChampionAIBrain.h
Shared/GameSim/Systems/ChampionAI/ChampionAIBrain.cpp
Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp
Shared/Schemas/Command.fbs
Shared/Schemas/Snapshot.fbs
Shared/Schemas/Event.fbs
Client/Private/UI/AIDebugPanel.cpp
```

현재 권위 흐름:

```text
CGameRoom::Phase_ServerBotAI
-> CServerAICommandProducer::Execute
-> CChampionAISystem::Execute
-> GameCommand outCommands
-> CDefaultCommandExecutor::ExecuteCommand
-> GameSim state mutation
-> Snapshot/Event
-> Client Visual
```

현재 perception:

```text
ChampionAIPerception
  lowHpEnemyChampion
  diveTarget
  enemyStructure
  abilityTarget
  mobilityTarget
  selfHpRatio
  enemyHpRatio
  selfGold
  enemyGold
  enemyDistance
  turretDanger
  bCanMove
  bCanAttack
  bCanCast
```

현재 valuation:

```text
ChampionAIValuation::ValueInput
ChampionFightValue
MinionFarmValue
StructureValue
TradeWindow
RetreatValue
BuildUtilityScores
```

현재 brain:

```text
RuleBased
PlayerLike
Decision
```

`Decision` brain은 아직 learned/external policy가 붙기 전까지 RuleBased fallback이다. 이 자리가 PyTorch policy artifact를 연결할 자연스러운 포트다.

현재 AI 루프의 핵심 성질:

```text
bot은 Transform/HP/cooldown을 직접 바꾸지 않는다.
bot은 GameCommand를 만든다.
결과는 CommandExecutor와 GameSim이 만든다.
```

이 구조가 반드시 유지되어야 한다.

## 3. NYPC 버섯게임에서 가져올 본질

버섯게임에서 가져올 것은 "버섯 규칙"이 아니다.

가져올 본질은 다음이다.

```text
deterministic rules engine
legal action mask
fixed-depth / oracle / regret
self-play league
failure board 저장
genome / parameter evolution
imitation / value distillation 방향
manifest / hash / reproducibility
promotion gate
```

버섯게임의 중요한 교훈:

```text
환경이 결정론적으로 재현되지 않으면 학습도 평가도 믿을 수 없다.
```

버섯에서 wall-clock 노이즈를 제거하기 위해 `FIXED_DEPTH`를 둔 것처럼, LoL Bot도 다음을 가져야 한다.

```text
same seed
same GameSim version
same data definition hash
same policy artifact hash
same input journal
same output episode hash
```

버섯에서 발전 루프:

```text
current bot
-> self-play / league
-> failure boards
-> oracle regret
-> genome/feature update
-> bake-off
-> promotion
```

LoL Bot으로 번역:

```text
current policy
-> SimLab / headless GameSim episodes
-> failure scenarios
-> utility/search/teacher relabel
-> BC/DAgger/PPO update
-> holdout league
-> PolicyArtifact promotion
```

버섯의 "강화학습"은 엄밀히 말하면 많은 부분이 RL이 아니라 evolutionary self-improvement였다. 하지만 RL-ready infrastructure는 맞다.

즉:

```text
버섯의 자산 = RL 그 자체보다 RL을 가능하게 하는 실험 공장.
```

## 4. NEXT NATION / battlefield에서 가져올 본질

NEXT NATION은 게임명이 아니라 battlefield에서 기준으로 삼은 강한 상대/전략 축이다. 로컬 문서의 핵심 선언은 다음이다.

```text
목표 = 특정 상대 카운터의 합이 아니라 게임 본질에 가장 가까운 전략.

민이형 뱅킹의 본질 =
확장으로 net income 우위
-> 중앙에 spare army 예치
-> 상대가 어디를 비우든 한 번에 절단
-> base/net 붕괴
-> HQ 마감
```

이걸 LoL로 번역하면:

```text
lane/economy 우위
-> 안전한 중심/강한 위치에 option 저장
-> 상대가 wave/objective/vision 중 하나를 비우는 순간 punish
-> tempo/economy/objective 붕괴
-> nexus win condition
```

NEXT NATION의 본질은 "중앙으로 무조건 간다"가 아니다.

본질은:

```text
option value를 보존하고,
상대의 commit을 기다렸다가,
상대가 비운 자원을 절단하는 것.
```

전장의 구조:

```text
GameState
-> OppTrack
-> Perception
-> Offer market
-> Behavior
-> gold-scale utility
-> command emit
-> replay/trace/evaluation
```

사용자 표현대로 다시 쓰면:

```text
GameState = Fact
Perception = Fact의 가공
Behavior = CLAIM / WAIT / LABOR / RUSH / DEFEND / CONTEST
Decision = 수치, 수식, 변수, 가중치를 통해 가장 높은 gold/value 행동 선택
```

이것은 현재 Winters AI 구조와 바로 대응된다.

```text
GameState/Fact
  -> Server authoritative CWorld

Perception
  -> ChampionAIPerception + future Observation/Belief

Behavior
  -> Champion AI intent / tactical candidate

Utility
  -> ChampionAIValuation + candidate score breakdown

Emit
  -> GameCommand

Outcome
  -> CommandExecutor result + Snapshot/Event
```

## 5. NEXT NATION에 RL/ML이 있었나?

정확한 답:

```text
NEXT NATION / battlefield에는 strict RL policy가 있었다고 보기 어렵다.
```

근거:

```text
1. 행동은 사람이 설계한 Behavior와 utility function에서 나왔다.
2. tuning은 WAR_* env knob를 바꿔 self-play 결과를 평가하는 방식이었다.
3. claude_rl_tune.py는 이름과 달리 policy gradient, Q-learning, PPO, value network 학습이 아니다.
4. 해당 스크립트는 config 후보를 mutation하고 deterministic games의 lexicographic fitness로 비교한다.
5. 즉 black-box optimization / ES-like tuning / parameter sweep에 가깝다.
```

그러나 다음은 분명히 존재했다.

```text
RL에 필요한 환경 사고
self-play
holdout
baseline 격리
side swap
fault 0 gate
trace/autopsy
opponent pool
parameter promotion
failure replay
```

그래서 서사는 이렇게 말해야 한다.

```text
NEXT NATION에는 production RL이 있었다기보다,
RL을 하기 위한 환경·평가·baseline·self-play 문화가 있었다.
그 위에 PyTorch policy/value learner를 얹는 것이 Winters LoL Bot의 다음 단계다.
```

## 6. map size별 최적 base 탐색은 RL인가?

엄밀히는 아니다.

map size별 최적 base 수, upgrade timing, train reserve, spare army threshold를 찾은 과정은 다음 중 하나다.

```text
parameter sweep
black-box optimization
evolutionary tuning
empirical policy search
ablation study
```

RL과의 차이:

```text
RL:
  매 episode trajectory에서 reward를 받고 policy/value가 업데이트된다.

Parameter tuning:
  정해진 policy 구조의 knob를 바꿔 여러 게임 결과를 비교한다.
```

하지만 이것을 낮게 볼 필요는 없다.

게임 AI 실무에서는 다음이 매우 강하다.

```text
hand-designed policy
+ deterministic evaluation
+ black-box tuning
+ holdout gate
+ replay autopsy
```

그리고 이것은 RL의 baseline으로 반드시 필요하다. RL은 baseline 없이는 자신이 개선됐는지 모른다.

정확한 포트폴리오 표현:

```text
NYPC에서 self-play와 black-box optimization으로 utility bot의 전략 파라미터를 조정했고,
그 경험을 LoL Bot의 imitation/RL baseline, opponent pool, promotion gate로 확장했다.
```

## 7. LoL 30tick GameSim에서 1 turn 사고를 어떻게 바꾸나

NEXT NATION은 turn/day 단위로 생각한다.

LoL은 30Hz GameSim이다.

단순 변환은 이렇게 하면 안 된다.

```text
1 turn = 1 tick
```

이렇게 하면 고수준 의사결정을 33ms마다 다시 해서 bot이 흔들린다.

올바른 변환:

```text
1 turn 사고 = decision epoch 또는 option boundary
30 tick = authoritative simulation clock
```

추천 cadence:

| 계층 | 주기 | 역할 |
|---|---:|---|
| Hard safety | 30Hz, 매 tick | 사망, CC, action lock, target invalid, skill impact, emergency retreat |
| Micro decision | 5~10Hz, 3~6 tick | trade, kite, last hit, skill cast, dodge, chase |
| Tactical decision | 2~5Hz, 6~15 tick | wave state, lane pressure, gank risk, fight/retreat |
| Macro decision | 1~2Hz, 15~30 tick | recall, buy, rotate, objective, group, defend |
| Event interrupt | event-driven | damage spike, enemy reveal, objective spawn, ally death |

즉 LoL에서 "한 턴"은 보통 다음 중 하나다.

```text
0.2초 micro decision step
0.5초 tactical decision step
1.0초 macro decision step
skill/action commitment이 끝나는 option step
```

RL 환경도 마찬가지다.

```text
env.step(action)
  -> GameCommand 또는 intent를 낸다.
  -> GameSim을 N tick 진행한다.
  -> next observation, reward, done, info를 반환한다.
```

N은 고정 6tick일 수도 있고, option이 끝날 때까지일 수도 있다.

초기 수직 슬라이스는 6tick이 좋다.

```text
30Hz GameSim
6 tick per RL step
=> 5 decisions/sec
```

이 정도면 LoL micro에 충분히 반응하면서도 학습 step이 과하게 길지 않다.

## 8. LoL Bot의 행동 계층

NEXT NATION 행동을 LoL로 번역한다.

| NEXT NATION | 본질 | LoL Bot intent |
|---|---|---|
| CLAIM | 미래 경제 거점 확보 | lane farm, wave crash, jungle camp, vision/objective setup |
| WAIT | 옵션 보존, 상대 commit 대기 | hold position, freeze, zone, bait, safe last hit |
| LABOR | 안정 수입 생산 | CS, XP soak, recall/buy timing, resource collection |
| RUSH | 상대 약점에 빠른 force | all-in, dive, fast rotate, turret push |
| DEFEND | 가치 있는 자산 보호 | defend turret, peel ally, hold wave, anti-dive |
| CONTEST | 양측이 노리는 중립 가치 경합 | river/objective fight, wave contest, vision contest |

중요:

```text
행동 enum을 바로 GameCommand로 만들지 않는다.
```

LoL에서는 계층이 필요하다.

```text
Strategic Objective
  FarmLane / RecallBuy / Rotate / Defend / Contest / Siege

Tactical Intent
  Trade / AllIn / Peel / Retreat / Zone / Hold

Mechanical Plan
  Kite / Chase / Combo / LastHit / SkillshotLead

Atomic Command
  Move / BasicAttack / CastSkill / Flash / Recall / BuyItem
```

PyTorch policy가 처음부터 좌표와 QWER를 직접 뱉으면 위험하다.

처음에는 다음을 학습시킨다.

```text
legal candidate list 중 하나를 선택한다.
```

예:

```text
Candidate 0: FarmMinion(targetNet=...)
Candidate 1: Retreat(anchorCell=...)
Candidate 2: TradeBasicAttack(enemyNet=...)
Candidate 3: CastQ(enemyNet=..., leadCell=...)
Candidate 4: HoldWave(cell=...)
```

정책은 candidate index를 고른다. 최종 `GameCommand`는 기존 validator/executor가 만든다.

## 9. Influence Map은 어떻게 깔 것인가

Influence Map은 하나의 예쁜 히트맵이 아니다.

LoL Bot에서 Influence Map은 `Perception`을 공간 함수로 만든 것이다.

레이어를 분리한다.

```text
ThreatNow
  지금 보이는 적/포탑/투사체의 즉시 위험

ThreatBelief
  안 보이는 적의 last-seen 기반 위험 확률

AllySupportETA
  아군이 특정 cell에 도착하는 시간

EnemyArrivalETA
  적이 특정 cell에 도착할 수 있는 시간

VisionConfidence
  본 지 얼마나 됐는가

EscapeCost
  안전 지점까지 도달 비용

WaveValue
  CS/XP/라인 압박 가치

ObjectiveValue
  turret/dragon/baron/nexus pressure 가치
```

현재 Winters 문서 `20_INFLUENCE_MAP_GAMESIM.md`의 방향과 같이, influence map은 `Shared/GameSim` 결정론 CPU 경로로 계산한다.

금지:

```text
GPU texture를 AI truth로 쓰지 않는다.
Client debug heatmap을 server decision truth로 쓰지 않는다.
시야 밖 적의 현재 위치를 그대로 넣지 않는다.
```

학습 feature로는 전체 map을 다 넣기보다 local crop과 summary를 쓴다.

```text
local crop around self: 16x16 or 32x32
global summary: objective/lane/team ETA vector
sampled path values: current, retreat path, engage path, objective path
```

## 10. 상대 행동과 패턴 분석

사용자가 말한 핵심은 이것이다.

```text
상대의 행동과 패턴을 분석하고,
그에 대응책을 모색하고,
실질적으로 수행하게 하는 능력.
```

LoL Bot에서는 이걸 네 단계로 만든다.

### 10.1 Observation

현재 보이는 사실:

```text
visible enemy position
visible HP/mana/level
visible animation/action
visible projectile/skill cast
visible minion wave
visible turret/objective state
```

### 10.2 Belief

안 보이는 것을 추정:

```text
lastSeenTick
lastSeenPosition
possiblePathSet
cooldownEstimate
recallProbability
gankProbability
objectiveCommitProbability
confidence
```

### 10.3 Opponent Pattern

반복 행동을 요약:

```text
aggression frequency
retreat threshold
skill usage timing
flash usage tendency
lane push tendency
roam timing
objective response delay
favorite combo opener
```

### 10.4 Counter-Policy

대응 후보를 만든다.

```text
if opponent overextends:
  bait -> all-in -> punish

if opponent roams:
  ping/retreat -> push wave -> take plate/objective

if opponent holds skill:
  bait cooldown -> re-engage

if opponent always contests:
  setup vision -> collapse with ally ETA

if opponent freezes:
  call jungle/support or reset wave
```

이 구조는 NEXT NATION의 `OppTrack -> Perception -> Offer market`과 같다.

LoL에서는 `OppTrack`이 더 중요하다. 부분 관측 때문이다.

## 11. Python / PyTorch는 어디에 붙는가

Python은 GameSim truth를 소유하지 않는다.

정확한 경계:

```text
C++ Winters GameSim
  authoritative transition
  observation export
  action mask
  candidate generation
  reward source
  episode hash

Python / PyTorch
  dataset loading
  model training
  evaluation orchestration
  artifact export

C++ Server Runtime
  artifact load
  inference or baked policy score
  final legal mask
  GameCommand emission
```

절대 금지:

```text
Python GameState를 LoL runtime truth로 사용
Python subprocess를 Server tick 안에서 호출
Python에서 계산한 damage/HP/cooldown을 authoritative result로 사용
```

가능한 구조:

```text
Tools/AIResearch
  ExportEpisodes.py
  TrainBehaviorCloning.py
  TrainDagger.py
  TrainPpo.py
  RunOfflineLeague.py
  ValidatePolicyArtifact.py
```

훈련 산출물:

```text
PolicyArtifact/
  metadata.json
  model.pt or model.onnx
  observation_schema.json
  action_schema.json
  feature_stats.json
  policy_hash.txt
  training_report.md
  holdout_report.md
```

## 12. Observation / Action / Reward 설계

### 12.1 Observation

초기 observation은 너무 크게 만들지 않는다.

```text
self:
  hpRatio, manaRatio, level, gold, cooldowns, actionState, position, velocity

visible enemies:
  topK relative position, hpRatio, level, distance, status, lastAction

visible allies:
  topK relative position, hpRatio, role/intent summary

wave/objective:
  minion count, wave position, turret hp, objective timer

belief:
  enemy last seen, confidence, ETA envelope, cooldown estimate

influence:
  local crop or sampled path values

commitment:
  current intent, remaining lock, active combo phase

legal mask:
  canMove, canAttack, canCastQWER, canFlash, canRecall, candidate valid flags
```

### 12.2 Action

초기 action은 raw 좌표가 아니다.

```text
action = candidate index
```

candidate는 C++에서 만든다.

```text
Retreat(anchor)
FarmMinion(target)
TradeBasicAttack(enemy)
CastSkill(slot, target/cell)
HoldPosition(cell)
FollowWave(cell)
ContestObjective(objective)
Recall()
```

이유:

```text
1. legal action mask가 쉽다.
2. invalid command를 줄인다.
3. rule baseline과 learned policy를 같은 후보 위에서 비교할 수 있다.
4. AI Debug에서 왜 선택했는지 설명 가능하다.
```

### 12.3 Reward

reward는 terminal win/loss가 북극성이다.

초기 reward:

```text
R =
  terminal win/loss
  + delta team gold/xp potential
  + delta objective/structure potential
  + bounded combat trade result
  - death risk/result
  - invalid command
  - intent churn
```

주의:

```text
CS만 크게 주면 싸움을 피하는 farming bot이 된다.
damage만 크게 주면 죽어도 딜 넣는 bot이 된다.
kill만 크게 주면 objective를 버린다.
```

따라서 reward는 항상 replay autopsy와 holdout으로 검증한다.

## 13. 학습 단계

처음부터 5v5 PPO를 하지 않는다.

순서는 다음이다.

```text
Stage 0. Rule / Utility Baseline
  current ChampionAIValuation + PlayerLike brain

Stage 1. Episode Export
  observation, candidates, selected action, executor result, reward, next state

Stage 2. Imitation Learning
  teacher = utility bot, search/chrono oracle, human replay if available
  model = candidate ranking / behavior cloning

Stage 3. DAgger
  learned policy가 방문한 state를 teacher가 다시 label

Stage 4. Self-Play League
  learned policy vs utility baselines vs past selves

Stage 5. PPO / Recurrent PPO
  1v1 lane micro부터

Stage 6. Distillation / Deployment
  smaller runtime model or baked table

Stage 7. 2v2 / 5v5 / macro
  team blackboard, objective, rotation
```

첫 증명 수직 슬라이스:

```text
1 champion
1 lane
1 opponent baseline
6 tick decision step
candidate action selection
BC -> DAgger -> small PPO
holdout seed win-rate/regret improvement
invalid command 0
```

## 14. Imitation Learning의 역할

IL은 "사람처럼 한다"보다 먼저 "baseline이 아는 것을 신경망이 안정적으로 따라 한다"를 증명한다.

Teacher:

```text
current utility bot
rule-based PlayerLike brain
Chrono counterfactual best candidate
search/minisim if available
human replay later
```

Label:

```text
selected candidate
candidate rank
teacher score
reject reason
regret
```

Loss:

```text
cross entropy for selected action
pairwise ranking loss for candidate order
value regression for expected outcome
invalid action mask loss = hard zero tolerance
```

IL 성공 기준:

```text
top-1 teacher match
top-3 teacher match
regret vs teacher
illegal action 0
holdout scenario non-regression
```

## 15. RL의 역할

RL은 마지막에 붙인다.

RL이 해결할 문제:

```text
teacher보다 나은 timing 찾기
상대 policy에 적응하기
long-term reward 보기
미세한 trade/retreat threshold 개선
상대 패턴을 exploit하되 holdout에서 무너지지 않기
```

초기 알고리즘은 PPO가 적절하다.

```text
on-policy
action mask와 함께 쓰기 쉬움
continuous real-time보다 candidate discrete action에 적합
stable baseline으로 설명하기 쉬움
```

하지만 네트워크가 직접 게임 규칙을 깨면 안 된다.

```text
policy proposes candidate
legal mask filters
server validates
executor accepts/rejects
episode records result
```

PPO loop:

```text
for each update:
  run N headless GameSim workers
  collect trajectory: obs, action, logprob, reward, value, done, mask
  compute GAE advantage
  update policy/value with clipped PPO loss
  evaluate against frozen baselines
  promote only if holdout gate passes
```

## 16. self-play와 평가

NYPC에서 가장 강한 자산은 "리그/게이트/부검 문화"다.

LoL Bot에 그대로 가져온다.

League pool:

```text
RuleBased baseline
PlayerLike baseline
Utility tuned baseline
Frozen learned policy N-1
Exploit bot
Defensive bot
Aggressive bot
Random legal weak bot
Scripted scenario bot
```

평가 지표:

```text
win rate / Elo
CS@10
gold/xp diff
kill/death
damage taken/dealt
turret/objective conversion
invalid command count
executor reject count
intent churn
reaction time
decision p50/p95/p99
state hash determinism
```

승급 조건:

```text
train improvement
validation improvement
holdout non-regression
side swap non-regression
invalid command 0
same seed deterministic
previous best regression 0
reward hacking replay 없음
```

이게 없으면 RL은 "잘 된 것 같은 느낌"으로 끝난다.

## 17. NEXT NATION의 1 turn을 LoL 학습 step으로 번역한 예

NEXT NATION:

```text
turn 시작
-> GameState 읽기
-> Perception 계산
-> CLAIM/WAIT/LABOR/RUSH/DEFEND/CONTEST 후보 생성
-> gold/value 점수
-> 최고 행동 emit
-> 다음 turn 결과 관측
```

LoL Bot:

```text
decision epoch 시작, 예: tick 900
-> Server CWorld fact 읽기
-> Team visibility filter
-> Observation/Belief 생성
-> Influence layer sample
-> 후보 생성
    FarmMinion
    Retreat
    Trade
    CastQ
    Hold
    Contest
-> legal mask
-> utility / policy score
-> selected candidate
-> GameCommand emit
-> GameSim 6 tick 진행
-> executor result + Snapshot/Event + reward
-> next observation
```

즉:

```text
NEXT NATION 1 turn = LoL decision epoch
LoL 30tick = 그 decision들이 들어가는 simulation clock
```

## 18. Command 계산과 30tick 관계

30Hz에서 command를 계산한다고 해서 모든 bot이 매 tick 새로운 macro 행동을 뽑는다는 뜻이 아니다.

실행 흐름:

```text
tick 0:
  hard safety check
  micro policy decision
  command emit

tick 1~5:
  GameSim executes movement/action
  emergency check
  no new high-level decision unless interrupt

tick 6:
  next micro decision

tick 30:
  macro decision can refresh
```

예:

```text
tick 900:
  policy chooses TradeBasicAttack
  emits BasicAttack command

tick 901~906:
  action lock / projectile / damage timing proceeds

tick 906:
  reward observes trade result
  policy may choose kite/continue/retreat
```

중요:

```text
active commitment을 무시하고 매 tick 새 행동을 고르면 bot은 사람답지 않고 combo가 깨진다.
```

그래서 현재 gotcha의 `hard safety -> active commitment -> new utility` 순서를 학습 policy에도 적용한다.

## 19. 학습 데이터 스키마

초기 episode record:

```json
{
  "episodeId": "uuid",
  "tick": 900,
  "schemaVersion": 1,
  "rulesHash": "...",
  "definitionHash": "...",
  "policyRevision": "...",
  "seed": 1234,
  "selfNetId": 17,
  "observation": {},
  "belief": {},
  "influenceSamples": {},
  "candidates": [
    {
      "candidateId": 0,
      "kind": "FarmMinion",
      "legal": true,
      "features": {},
      "utilityScore": 0.64
    }
  ],
  "teacherAction": 0,
  "policyAction": 0,
  "executorAccepted": true,
  "reward": 0.12,
  "nextStateHash": "..."
}
```

이 record가 있어야 한다.

없으면 PyTorch 학습은 불가능하거나, 불가능한 것을 억지로 근사하게 된다.

## 20. Runtime 배포

학습된 정책은 runtime에서 다음처럼 사용한다.

```text
Server CChampionAISystem
-> Build observation
-> Build candidates
-> Build legal mask
-> PolicyArtifact inference
-> Select candidate
-> Emit GameCommand
-> CommandExecutor validates again
```

Runtime policy는 절대 다음을 하지 않는다.

```text
HP 직접 변경
cooldown 직접 변경
damage 직접 적용
Transform 직접 순간이동
Client visual 직접 호출
```

PolicyArtifact에는 다음이 필요하다.

```text
observation schema version
action schema version
model hash
training dataset hash
rules/definition hash
normalization stats
legal mask semantics
evaluation report
rollback policy
```

## 21. 기존 NYPC 툴 자산의 적용

| NYPC 자산 | 본질 | LoL Bot 적용 |
|---|---|---|
| deterministic rules/replay | 같은 입력은 같은 결과 | SimLab/GameSim episode hash |
| fixed-depth / oracle | 더 좋은 label 생성 | chrono/search teacher |
| self-play league | 감이 아니라 통계 | policy promotion league |
| side swap / holdout | 과적합 방지 | blue/red, lane side, seed split |
| trace/autopsy | 왜 졌는지 추적 | AI Debug candidate ledger |
| black-box tuner | utility baseline 최적화 | rule baseline tuning, PPO 대조군 |
| manifest/hash | artifact 재현성 | PolicyArtifact manifest |
| failure board 저장 | 패배를 dataset으로 전환 | failure scenario replay |
| pairwise/ranking IL | best action 따라 배우기 | candidate rank model |
| counterfactual branch | 같은 상태에서 다른 선택 비교 | Chrono A/B policy evaluation |

가져오지 말 것:

```text
NYPC Python GameState
NYPC referee protocol
버섯/전장 전용 보드 파서
대회 제출용 hard-coded bot policy
601개 Python 파일 일괄 복사
```

가져올 것은 코드 덩어리가 아니라 실험 규율이다.

## 22. 포트폴리오 서사

잘못된 서사:

```text
RL 봇을 만들었습니다.
```

강한 서사:

```text
NYPC에서 결정론 환경, self-play league, trace autopsy, black-box tuning, oracle regret를 구축했고,
이를 Winters 서버 권위 MOBA GameSim 위에 observation/action/reward/episode/policy artifact 파이프라인으로 재구성했습니다.
Rule/utility bot을 baseline과 teacher로 두고,
PyTorch BC/DAgger/PPO policy를 학습시켜 holdout league로 승급시키는 구조를 설계했습니다.
```

더 짧은 면접 답변:

```text
저는 봇을 손으로 잘 튜닝한 것에서 끝내지 않고,
그 튜닝 봇을 teacher와 sparring partner로 바꿔
모방학습, self-play, 평가 게이트가 돌아가는 학습 환경으로 확장했습니다.
```

## 23. 단계별 구현 로드맵

### P0. 현재 utility baseline 봉인

목표:

```text
현재 ChampionAIValuation / PlayerLike brain을 baseline으로 고정한다.
```

산출물:

```text
baseline policy revision
decision trace
SimLab scenario set
invalid command count
```

### P1. Episode Export

목표:

```text
GameSim episode를 Python이 학습할 수 있는 데이터로 내보낸다.
```

산출물:

```text
AiEpisodeV1 JSONL
observation schema
candidate/action schema
reward schema
state hash
```

### P2. Imitation v1

목표:

```text
utility bot의 선택을 PyTorch candidate-ranking model이 따라 한다.
```

산출물:

```text
TrainBehaviorCloning.py
top-1/top-3/regret report
holdout report
PolicyArtifact v1
```

### P3. DAgger v1

목표:

```text
learned policy가 방문한 실패 state를 teacher가 다시 label한다.
```

산출물:

```text
failure scenario queue
teacher relabel report
new policy revision
```

### P4. PPO v1

목표:

```text
1v1 lane micro에서 PPO가 utility baseline 대비 특정 metric을 개선한다.
```

산출물:

```text
TrainPpo.py
training curve
reward hacking report
holdout league report
```

### P5. Runtime ShadowCoach

목표:

```text
learned policy가 명령을 내지 않고 top-k 행동과 이유를 보여준다.
```

산출물:

```text
F9 AI Debug top-k
candidate contribution
human-readable recommendation
```

### P6. Runtime Control

목표:

```text
promotion gate를 통과한 policy만 실제 GameCommand를 낸다.
```

산출물:

```text
runtime policy loader
inference parity test
invalid command 0
rollback path
```

## 24. 검증 기준

필수 검증:

```text
same seed episode hash identical
observation has no privileged hidden enemy current fact
legal mask prevents invalid action
executor reject count tracked
teacher top-k and policy top-k logged
train/validation/holdout split separated
side swap measured
previous best regression gate
reward hacking replay inspected
Python/C++ inference parity
runtime final command still passes CommandExecutor
```

성능 검증:

```text
win rate / Elo with confidence
CS@10
gold/xp diff
death rate
turret/objective conversion
trade value
reaction time
intent churn
AI p95/p99 decision cost
```

## 25. Ownership Boundaries

```text
Shared/GameSim
  authoritative world, components, systems, command validation, deterministic observation primitives

Server
  bot policy orchestration, episode export, artifact load, final GameCommand emission

Tools/AIResearch
  Python/PyTorch training, league, manifest, reports

Client
  AI Debug, ShadowCoach UI, visualization only

Services/Docker
  later: experiment tracking, artifact registry, distributed training workers
```

금지:

```text
Shared/GameSim -> Python 의존
Shared/GameSim -> Engine/Client/ImGui/DX 의존
Server -> Client visual 의존
Client -> authoritative gameplay truth 생성
Python -> runtime gameplay transition truth 소유
```

## 26. 30% 환전 예산

이 트랙은 깊게 파기 쉬운 바닥 작업이다.

따라서 매주 최소 30%는 다음으로 환전한다.

```text
1. 1페이지 architecture figure
2. F9 AI Debug 짧은 영상
3. BC/PPO 학습 곡선 스크린샷
4. holdout league 표
5. 면접용 90초 설명 스크립트
6. README / 기술 블로그 초안
7. 실제 지원서 문장 업데이트
```

학습 파이프라인은 포트폴리오로 보이지 않으면 없는 것과 같다.

## 27. 최종 문장

이 프로젝트의 본질은 다음이다.

```text
NYPC에서 만든 것은 봇 하나가 아니라,
환경을 결정론적으로 재현하고,
상태를 가공해 행동 후보를 만들고,
self-play와 trace로 실패를 모으고,
평가 게이트로 승급시키는 실험 문화였다.

Winters LoL Bot은 그 문화를 서버 권위 30Hz GameSim 위에 올리고,
utility/influence bot을 baseline·teacher·sparring partner로 삼아,
PyTorch 모방학습과 강화학습 정책을 실제 GameCommand 생산자로 승급시키는 프로젝트다.
```

## 28. Verification / Handoff

이 문서는 코드 변경 없이 분석과 설계를 박제한 문서다.

확인한 로컬 근거:

```text
C:\Users\user\Desktop\NYPC\battlefield\CLAUDE.md
C:\Users\user\Desktop\NYPC\STRATEGY_GROUNDTRUTH_DIRECTION.md
C:\Users\user\Desktop\NYPC\battlefield\tools\claude_rl_tune.py
C:\Users\user\Desktop\NYPC\mushroom\docs\winters_ai_learning_ml\session_03_agent_league.md
C:\Users\user\Desktop\NYPC\mushroom\docs\winters_ai_learning_ml\session_04_mcts_rl_learning_loop.md
C:\Users\user\Desktop\NYPC\mushroom\docs\mushroom_zero_self_improvement_loop_20260608.md
C:\Users\user\Desktop\Winters\.md\architecture\WINTERS_NYPC_HUMANLIKE_AI_RESEARCH_ARCHITECTURE.md
C:\Users\user\Desktop\Winters\.md\plan\ai\14_NYPC_COMPETITION_ML_LAB_BRIDGE.md
C:\Users\user\Desktop\Winters\Shared\GameSim\Systems\ChampionAI\ChampionAIPerception.h
C:\Users\user\Desktop\Winters\Shared\GameSim\Systems\ChampionAI\ChampionAIValuation.h
C:\Users\user\Desktop\Winters\Shared\GameSim\Systems\ChampionAI\ChampionAIBrain.cpp
C:\Users\user\Desktop\Winters\Shared\GameSim\Systems\ChampionAI\ChampionAISystem.cpp
C:\Users\user\Desktop\Winters\Server\Private\Game\ServerAICommandProducer.cpp
C:\Users\user\Desktop\Winters\Server\Private\Game\GameRoomChampionAI.cpp
```

다음 세션 추천:

```text
Session 1:
  AiEpisodeV1 스키마 설계.

Session 2:
  current utility bot을 teacher로 삼는 BC dataset export 설계.

Session 3:
  PyTorch candidate ranking trainer 최소 수직 슬라이스.

Session 4:
  holdout league와 PolicyArtifact promotion gate.
```
