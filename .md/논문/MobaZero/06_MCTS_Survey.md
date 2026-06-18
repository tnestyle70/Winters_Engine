# MCTS Survey

원문:
- IEEE: https://ieeexplore.ieee.org/document/6145622/
- 제목: A Survey of Monte Carlo Tree Search Methods

한 줄:
- MCTS의 기본 단계, UCT, rollout, 변형 알고리즘을 정리한 기준 survey.

## 한국어 핵심

MCTS는 가능한 모든 수를 완전 탐색하지 않고, 유망한 가지를 통계적으로 더 많이 탐색한다. 기본 루프는 selection, expansion, simulation, backpropagation이다. AlphaZero와 MuZero는 이 MCTS를 neural policy/value와 결합했고, NYPC Mushroom에서는 alpha-beta와 비교할 search expert 후보가 된다.

## 알고리즘 구조

```text
1. Selection
   root에서 UCB/UCT 점수가 높은 child를 따라 내려간다.

2. Expansion
   아직 시도하지 않은 action을 child node로 추가한다.

3. Simulation / Rollout
   leaf state부터 terminal 또는 depth limit까지 시뮬레이션한다.

4. Backpropagation
   rollout 결과를 parent 방향으로 누적한다.
```

UCT:

```text
score = average_value + C * sqrt(log(parent_visits) / child_visits)
```

## MobaZero 적용 포인트

NYPC Mushroom:
- branching factor가 중간 이상이면 alpha-beta와 MCTS를 비교한다.
- rollout은 random보다 greedy/heavy playout이 유리할 가능성이 크다.
- action generator가 정확해야 한다.

Winters MOBA:
- full 5v5 MCTS는 폭발한다.
- 1v1/2v2 `BattleState`에서만 짧게 적용한다.
- root child visits를 AI Debug panel에 보여주면 좋다.

## 구현 과제

1. Mushroom MCTS prototype.
2. greedy rollout과 random rollout 비교.
3. time budget별 visits/p95 decision time 기록.
4. alpha-beta 대비 이기는 board type 분석.
5. Winters `CMCTSPlanner`를 나중에 `BattleState` 기반으로 정리.

## 읽기 체크리스트

- UCT의 exploration term을 설명할 수 있는가?
- rollout policy가 성능에 왜 큰 영향을 주는가?
- MCTS가 alpha-beta보다 유리한 게임 조건은 무엇인가?
- MOBA에서 MCTS 적용 범위를 왜 줄여야 하는가?

