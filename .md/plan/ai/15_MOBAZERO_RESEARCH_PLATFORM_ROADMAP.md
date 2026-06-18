Session - NYPC Competition ML Lab을 Winters LoL AI 연구 플랫폼과 포트폴리오 로드맵으로 연결한다.

# Stage 15 — MobaZero Research Platform Roadmap

작성일: 2026-06-06

이 문서는 코드 패치 계획이 아니라 연구/포트폴리오 로드맵이다. Winters 런타임 코드는 이 문서만으로 수정하지 않는다. NYPC Lab에서 검증한 규칙 모델링, 탐색, 리그, 로그, ML/RL 실험을 장기적으로 Winters의 서버 권위 MOBA AI 설계로 이식하는 방향을 정리한다.

## 1. 현재 판단

지금 깨달은 핵심은 맞다.

에셋을 가져오고 롤을 모작하는 것은 결과물의 외형을 만든다. 하지만 “답이 없는 주제를 끝까지 파는 능력”은 규칙, 상태, 행동, 가치, 탐색, 학습, 검증, 로그, 재현성을 한 시스템으로 닫는 데서 나온다.

따라서 북극성은 다음이다.

```text
MobaZero:
NYPC형 턴제 게임에서 Complete Game Model과 Agent Lab을 만들고,
그 원리를 Winters 서버 권위 GameSim 기반 MOBA 5인 팀 AI로 확장한다.
```

대외 표현은 다음처럼 잡는다.

```text
MobaZero: A Hierarchical Multi-Agent Research Platform for MOBA Decision Making
```

또는 Winters 포트폴리오 이름으로:

```text
WintersZero: Search, Self-Play, and Hierarchical Team AI for MOBA Games
```

## 2. 현재 폴더 상황

### 2.1 NYPC Lab

루트:

```text
C:\Users\tnest\Desktop\NYPC
```

현재 구현 워크스페이스:

```text
C:\Users\tnest\Desktop\NYPC\mushroom
```

현재 구조:

```text
mushroom/
  bots/
    cpp/main.cpp
    baseline_py/main.py
    pass_py/main.py
    greedy_delta_py/main.py
    rules_greedy_py/main.py
    reply_aware_py/main.py
    minimax_py/main.py
    alpha_beta_py/main.py
  lab/
    games/mushroom/
      MushroomRules.py
      GreedyAgent.py
      ReplyAwareGreedyAgent.py
      MinimaxAgent.py
      AlphaBetaMinimaxAgent.py
      FeatureExtractor.py
      ExportReplayFeatures.py
      ReplayConsistencyCheck.py
      AnalyzeFeatures.py
    ml_hello_world/
    policy_value_hello_world/
  scripts/
    build_cpp.ps1
    build_cpp_wsl.ps1
    run_batch.py
    run_dual_match.ps1
    run_cpp_vs_agents_league.ps1
    run_feature_analysis.ps1
    analyze_log.py
    parse_log.py
    quick_sweep.py
  data/
    boards/
    logs/
    reports/
    experiments/
  docs/
    plan/
    winters_ai_learning_ml/
    ml_notes/
```

현재 의미:

- 단일 제출 후보는 `bots/cpp/main.cpp`다.
- Python bots는 baseline과 실험용 비교 상대다.
- `lab/games/mushroom`에는 Complete Game Model, Agent, feature/export/replay 검증 조각이 있다.
- `data/reports/latest.summary.json`과 `latest.turns.jsonl`은 로그 분석이 이미 구조화되고 있음을 보여준다.
- 최근 `cpp_eval_patch_5_summary.csv` 기준 C++ 후보는 `rules_greedy`에는 크게 이기지만, `alpha_beta`에는 열세이고 `minimax`/`reply_aware`와는 비슷하다. 즉 다음 병목은 더 강한 search expert, feature 기반 blunder mining, evaluator tuning이다.

### 2.2 Winters AI

주요 문서:

```text
C:\Users\tnest\Desktop\Winters\.md\plan\ai\00_AI_PLAN_INDEX.md
C:\Users\tnest\Desktop\Winters\.md\plan\ai\08_STAGE6_MCTS.md
C:\Users\tnest\Desktop\Winters\.md\plan\ai\09_STAGE7_IMITATION.md
C:\Users\tnest\Desktop\Winters\.md\plan\ai\10_STAGE8_RL.md
C:\Users\tnest\Desktop\Winters\.md\plan\ai\11_TEAM_BLACKBOARD.md
C:\Users\tnest\Desktop\Winters\.md\plan\ai\13_DEBUG_EDITOR.md
C:\Users\tnest\Desktop\Winters\.md\plan\ai\14_NYPC_COMPETITION_ML_LAB_BRIDGE.md
```

주요 코드 축:

```text
Engine/Public/AI/BehaviorTree.h
Engine/Public/AI/Blackboard.h
Engine/Public/AI/MCTSPlanner.h
Engine/Public/AI/RLBridge.h
Engine/Public/ECS/Systems/VisionSystem.h
Shared/GameSim/Systems/ChampionAI/ChampionAISystem.h
Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h
Server/Private/Game/GameRoomChampionAI.cpp
Client/Private/UI/AIDebugPanel.cpp
```

현재 의미:

- Winters는 이미 HFSM/BT/MCTS/RL/Blackboard 방향의 뼈대가 있다.
- 서버 권위 흐름은 `Client Input -> GameCommand -> Server GameSim -> Snapshot/Event -> Client Visual`이다.
- `ChampionAISystem`은 `GameCommand`를 생산하는 구조라 장기 AI 통합 방향과 맞다.
- `AIDebugPanel`은 현재 상태, 의도, 행동, block reason, decision trace를 UI로 보여준다. 연구 플랫폼의 디버그 철학과 맞다.
- `VisionSystem`과 FOW texture는 전장의 안개/부분 관측 연구의 시작점이다.

## 3. 연구 질문

큰 질문:

```text
MOBA에서 한 팀 5명의 의사결정을 상태 머신, 탐색, self-play, imitation/RL, 팀 blackboard로 결합하면,
규칙 기반 봇과 사람 초급~중급 팀을 압도하는 재현 가능한 Agent를 만들 수 있는가?
```

작게 쪼갠 질문:

- 전장의 안개에서 봇은 무엇을 “알아도 되는가?”
- 미니언 하나, 웨이브 하나, 포탑 체력, 챔피언 체력, 스펠 쿨타임의 가치는 어떻게 수치화하는가?
- 챔피언 행동의 가치는 즉시 피해량인가, 위치 우위인가, 다음 3초의 생존 확률인가?
- 1v1 micro와 5v5 macro 사이를 어떤 abstraction으로 연결할 것인가?
- 팀의 목표는 한 명이 정하는가, blackboard 투표로 정하는가, learned policy가 정하는가?
- 탐색은 실시간에서 어디까지 가능하고, 오프라인 학습은 무엇을 baked artifact로 남길 수 있는가?
- 강해졌다는 증거는 무엇인가: 승률, Elo, score diff, blunder rate, regret, p95 decision time 중 무엇을 기준으로 삼을 것인가?

## 4. 개념 계층

### 4.1 Game DNA

모든 문제를 받으면 먼저 분류한다.

```text
players:
zero_sum / score_sum:
perfect_info / hidden_info:
deterministic / stochastic:
sequential / simultaneous:
finite_horizon:
branching_factor:
legal_action_mask:
scoring:
terminal:
official_tool:
submission_artifacts:
best_first_algorithm:
```

버섯 게임은 완전정보, 결정론, 순차, finite horizon, cheap evaluator다.

MOBA는 부분 관측, 실시간, 연속/이산 혼합 행동, 다중 에이전트, 긴 지평, 팀 보상, 비정상 상대 정책이다.

### 4.2 Complete Game Model

NYPC에서 먼저 완성할 능력:

```text
State
Action
LegalMask
Apply
Score
Terminal
Replay
Hash
FeatureVector
StructuredLog
LeagueReport
```

Winters로 가져올 때 대응:

```text
NYPC State              -> GameSim snapshot / BotDecisionContext
NYPC LegalMask          -> 가능한 GameCommand / skill cast validation
NYPC Apply              -> Shared/GameSim deterministic simulator
NYPC FeatureVector      -> Bot state/action features
NYPC Replay             -> Server replay + AI decision trace
NYPC LeagueReport       -> bot benchmark dashboard
```

## 5. 단계별 로드맵

### Phase 0. 기준 고정

목표:
- “롤 모작”이 아니라 “MOBA AI 연구 플랫폼”을 포트폴리오 핵심 주제로 잡는다.
- Winters와 NYPC의 역할을 분리한다.

산출물:
- 이 문서
- NYPC `docs/plan/02_INDEPENDENT_COMPETITION_ML_LAB.md`
- Winters `14_NYPC_COMPETITION_ML_LAB_BRIDGE.md`

검증:
- 새 작업자가 문서만 읽고 “NYPC는 독립 Lab, Winters는 장기 통합 목적지”라고 이해한다.

다음 단계:
- NYPC에서 replay/log/league가 안정될 때까지 Winters 런타임 통합을 보류한다.

### Phase 1. NYPC Mushroom Complete Model

목표:
- 버섯 게임을 완전정보 턴제 기준 모델로 닫는다.

산출물:
- `MushroomRules.py`
- legal rectangle generator
- apply/score/pass/terminal 검증
- replay consistency checker
- structured turn JSONL

검증:
- random board 수천 개 crash 0, illegal 0
- official tool과 score/terminal 일치
- 같은 replay 두 번 실행 시 state hash 일치

다음 단계:
- Agent가 틀렸을 때 엔진을 의심하지 않아도 되는 상태.

### Phase 2. Search Expert

목표:
- ML teacher가 될 강한 탐색 Agent를 만든다.

산출물:
- greedy tactical oracle
- alpha-beta / beam search
- transposition table
- endgame solver
- top-k candidate log

검증:
- current C++ 후보가 alpha_beta에 밀리는 구간을 찾아 reverse-engineering한다.
- `cpp_eval_patch_5_summary.csv` 같은 리그 리포트에서 previous best 대비 통계 우위를 만든다.
- seed, board, first/second 교차 테스트가 재현된다.

다음 단계:
- expert action dataset을 만들 수 있다.

### Phase 3. Feature / Blunder Mining

목표:
- 패배를 “아쉬웠다”가 아니라 feature와 regret으로 분해한다.

산출물:
- state feature extractor
- action feature extractor
- blunder classifier
- critical turn report
- replay viewer 또는 HTML board viewer

검증:
- 각 턴에서 chosen move, expert best move, reply risk, missed swing을 볼 수 있다.
- 패배 로그 1개를 30초 안에 설명할 수 있다.

다음 단계:
- evaluator weight tuning과 imitation dataset으로 이어진다.

### Phase 4. Offline Tuning

목표:
- 평가 함수와 탐색 파라미터를 손감각이 아니라 리그로 튜닝한다.

산출물:
- GA/Optuna/CMA-ES 중 하나의 최소 튜너
- genome manifest
- train/holdout board split
- generation별 league report

검증:
- train board만 이기고 holdout에서 무너지는 Agent를 걸러낸다.
- previous best와 cross-seat 대전에서 안정적으로 앞선다.

다음 단계:
- tuned weight를 C++ constants 또는 `data.bin`으로 export한다.

### Phase 5. Imitation / Policy-Value

목표:
- search expert의 판단을 policy/value model이 따라 배우게 한다.

산출물:
- `(state_features, action_features, legal_mask, expert_action, value)` dataset
- PyTorch policy/value trainer
- validation report
- top-1/top-3 accuracy
- regret metric

검증:
- legal mask 이후 illegal action 선택률 0
- holdout board에서 regret 감소
- policy prior + search가 no-prior search보다 같은 시간 예산에서 강함

다음 단계:
- 제출 가능한 경량 artifact로 압축한다.

### Phase 6. MOBA Micro Simulator

목표:
- Winters 전체 게임이 아니라 1v1/2v2 교전의 작은 simulator부터 만든다.

산출물:
- `BattleState`
- champion HP/resource/cooldown/status abstraction
- discrete action set
- deterministic step
- simple reward
- MCTS rollout

검증:
- 랜덤 봇 대비 MCTS 봇 승률 90% 이상
- 같은 seed에서 같은 결과
- decision p95 time budget 기록

다음 단계:
- `Shared/GameSim` truth와 동기화 가능한 `BotDecisionContext` 설계.

### Phase 7. MOBA Value Model

목표:
- 전장의 안개, 미니언, 챔피언, 오브젝트, 행동 가치를 수치화한다.

상태 가치:
- 내/상대 HP, 마나, 쿨타임
- 거리, 각도, 충돌/지형
- 미니언 wave health와 위치
- 포탑 사거리와 danger
- FOW last seen / uncertainty
- objective timer와 team gold/xp

행동 가치:
- kill probability
- death risk
- damage expected value
- wave control value
- objective tempo
- vision denial / information gain
- retreat safety

검증:
- AI Debug panel에서 decision score와 block reason을 추적한다.
- 사람이 봐도 이상한 행동을 critical turn report로 분류한다.

다음 단계:
- Utility AI, MCTS, learned value가 같은 feature 정의를 공유한다.

### Phase 8. Team Blackboard / Macro

목표:
- 5명이 각자 강한 것이 아니라 팀으로 강하게 만든다.

산출물:
- team blackboard schema
- role assignment
- objective proposal/vote
- last known enemy observation
- group/retreat/engage commands

검증:
- 봇 5명이 같은 objective를 향해 움직이는지 확인한다.
- 사람이 낸 ping이 bot team decision에 제한적으로 반영된다.
- FOW 밖 정보는 쓰지 않는다.

다음 단계:
- 5v5 scripted team benchmark.

### Phase 9. Self-Play League

목표:
- 강한 Agent를 감으로 고르지 않고 리그로 고른다.

산출물:
- opponent pool
- frozen baselines
- current challenger
- Elo/report
- regression board set
- replay archive

검증:
- 새 정책이 old champion, greedy, rule-based, scripted team을 통계적으로 이긴다.
- exploit 한 가지에만 과적합한 정책을 걸러낸다.

다음 단계:
- 포트폴리오에서 “강해지는 과정”을 숫자와 영상으로 보여준다.

### Phase 10. Portfolio Packaging

목표:
- 연구가 코드 더미가 아니라 읽히는 결과물이 되게 한다.

산출물:
- GitHub README
- architecture diagram
- paper reading notes
- benchmark table
- replay viewer demo
- AI Debug panel screenshot/video
- 실패 분석과 개선 사례
- short technical report

검증:
- 처음 보는 사람이 5분 안에 문제, 구조, 실험, 결과를 이해한다.
- 30분 안에 핵심 코드를 따라갈 수 있다.
- 대회 결과와 별개로 “이 사람은 어려운 주제를 끝까지 판다”가 보인다.

## 6. 참고 논문과 소스

### 6.1 먼저 읽을 것

1. AlphaZero
   - `Mastering Chess and Shogi by Self-Play with a General Reinforcement Learning Algorithm`
   - https://arxiv.org/abs/1712.01815
   - 핵심: rules + self-play + policy/value + MCTS.

2. MuZero
   - `Mastering Atari, Go, Chess and Shogi by Planning with a Learned Model`
   - https://arxiv.org/abs/1911.08265
   - 핵심: 명시적 rules 없이 learned dynamics로 planning.

3. OpenAI Five
   - `Dota 2 with Large Scale Deep Reinforcement Learning`
   - https://arxiv.org/abs/1912.06680
   - 핵심: MOBA형 long horizon, partial information, continuous action, team self-play.

4. AlphaStar
   - `Grandmaster level in StarCraft II using multi-agent reinforcement learning`
   - https://www.nature.com/articles/s41586-019-1724-z
   - 핵심: league training, diverse agents, counter-strategy.

5. PPO
   - `Proximal Policy Optimization Algorithms`
   - https://arxiv.org/abs/1707.06347
   - 핵심: RL baseline algorithm.

6. MCTS Survey
   - `A Survey of Monte Carlo Tree Search Methods`
   - https://ieeexplore.ieee.org/document/6145622/
   - 핵심: UCT, rollout, tree policy, variants.

7. DeepStack / Libratus / CFR
   - DeepStack: https://arxiv.org/abs/1701.01724
   - CFR: https://papers.nips.cc/paper/3306-regret-minimization-in-games-with-incomplete-information
   - Libratus: https://pubmed.ncbi.nlm.nih.gov/29249696/
   - 핵심: 불완전정보, safe search, regret minimization.

### 6.2 참고할 오픈소스

1. OpenSpiel
   - https://github.com/google-deepmind/open_spiel
   - 게임 RL/search/planning framework 구조 참고.

2. PettingZoo
   - https://pettingzoo.farama.org/
   - multi-agent environment API 참고.

3. RLCard
   - https://github.com/datamllab/rlcard
   - 카드/불완전정보 게임 구조 참고.

4. Stable-Baselines3
   - https://stable-baselines3.readthedocs.io/
   - PPO 등 baseline trainer 사용법 참고.

5. OpenAI Five 자료
   - https://openai.com/five/
   - https://openai.com/index/dota-2-with-large-scale-deep-reinforcement-learning/
   - MOBA scale, self-play 운영, 한계와 교훈 참고.

## 7. 다음 실천 순서

1. NYPC Mushroom에서 `alpha_beta`를 이기는 C++ 제출 후보를 만든다.
2. `latest.turns.jsonl`을 HTML replay viewer로 본다.
3. 버섯 feature/blunder report를 완성한다.
4. evaluator weight tuner를 만든다.
5. policy/value hello world를 Mushroom expert dataset으로 확장한다.
6. Winters 쪽은 아직 런타임 통합하지 않는다.
7. 통합이 필요해지는 순간 `BotDecisionContext`와 `BattleState`부터 계획한다.

## 8. 완료 정의

이 주제가 포트폴리오로 성립한 상태:

- NYPC Lab에서 게임 규칙, 탐색, 학습, 리그, 로그, replay가 닫힌다.
- 버섯/야추/Connexion류 문제에 같은 Game DNA 분석을 적용한다.
- Winters에는 서버 권위 MOBA AI로 가져올 수 있는 feature/action/value 추상화가 있다.
- 1v1 micro, 2v2 tactical, 5v5 macro를 단계별 benchmark로 보여준다.
- 결과는 “내가 만든 봇이 세다”가 아니라 “강해지는 실험 시스템을 만들었다”로 설명된다.
