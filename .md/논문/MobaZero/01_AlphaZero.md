# AlphaZero

원문:
- arXiv: https://arxiv.org/abs/1712.01815
- PDF: https://arxiv.org/pdf/1712.01815

한 줄:
- 게임 규칙만 주고 self-play로 policy/value network를 학습한 뒤, MCTS와 결합해 체스, 쇼기, 바둑에서 초인급 성능을 만든 방법.

## 한국어 핵심

AlphaZero의 핵심은 “강한 수를 사람이 알려주지 않는다”는 점이다. 처음에는 무작위에 가깝게 플레이하지만, 자기 자신과 계속 대국하면서 더 좋은 정책과 가치 평가를 만든다. 매 수마다 MCTS가 현재 neural network를 사용해 탐색하고, 탐색 결과를 다시 학습 데이터로 삼는다.

핵심 루프:

```text
current network f_theta
-> self-play games with MCTS guided by f_theta
-> produce (state, improved policy, game outcome)
-> train f_theta to predict improved policy and value
-> evaluate new network against old network
-> replace if stronger
```

## 알고리즘 구조

입력:
- 현재 보드 상태
- 합법 수 mask

네트워크 출력:
- policy: 각 행동 prior probability
- value: 현재 상태의 승률/기대 결과

MCTS에서 사용하는 값:
- prior probability
- visit count
- action value
- exploration bonus

학습 target:
- policy target: MCTS visit distribution
- value target: 최종 승패

## MobaZero 적용 포인트

NYPC Mushroom:
- search expert가 만든 visit distribution을 policy target으로 쓴다.
- value target은 최종 score diff 또는 승패로 둔다.
- 거대한 NN 대신 작은 feature/value model로 시작한다.

Winters MOBA:
- full game AlphaZero는 너무 크다.
- 먼저 `BattleState` 1v1/2v2 micro combat에서만 AlphaZero식 loop를 흉내낸다.
- 행동 공간은 이동 방향, basic attack, Q/W/E/R, retreat 같은 이산 행동으로 줄인다.

## 구현 과제

1. Mushroom `State -> FeatureVector` 고정.
2. `Move -> ActionIndex` 또는 action feature 표현 결정.
3. alpha-beta/MCTS expert로 `(state, target_policy, target_value)` 저장.
4. PyTorch policy/value model 학습.
5. policy prior + search가 no-prior search보다 빠르거나 강한지 리그로 검증.

## 읽기 체크리스트

- policy target이 왜 사람이 둔 수가 아니라 MCTS visit count인지 설명할 수 있는가?
- value target이 어떤 시점의 값을 예측하는지 설명할 수 있는가?
- exploration이 없으면 self-play가 왜 좁아지는지 설명할 수 있는가?
- Mushroom에서 action space를 어떻게 고정할지 결정했는가?
- Winters MOBA에서 full map이 아니라 micro simulator부터 시작해야 하는 이유를 설명할 수 있는가?

