# MuZero

원문:
- arXiv: https://arxiv.org/abs/1911.08265
- PDF: https://arxiv.org/pdf/1911.08265

한 줄:
- 환경의 실제 규칙이나 전이식을 직접 알지 않고, 학습된 model 안에서 planning을 수행해 Atari, Go, Chess, Shogi를 다룬 방법.

## 한국어 핵심

AlphaZero는 게임 규칙을 알고 MCTS를 한다. MuZero는 한 걸음 더 나아가, 환경의 dynamics를 직접 알지 않아도 된다. 대신 observation을 latent state로 바꾸고, 그 latent state 위에서 action을 적용했을 때 다음 latent state, reward, policy, value를 예측한다.

MuZero는 “정확한 세계 모델”을 배우려 하지 않는다. 의사결정에 필요한 reward, policy, value를 잘 예측하는 내부 모델을 배운다.

## 알고리즘 구조

주요 함수:

```text
representation: observation -> hidden state
dynamics: hidden state + action -> next hidden state + reward
prediction: hidden state -> policy + value
```

Planning:
- 실제 게임 엔진을 복사해 시뮬레이션하지 않는다.
- learned dynamics 안에서 tree search를 한다.

## MobaZero 적용 포인트

NYPC Mushroom:
- 지금 단계에서는 rules를 정확히 알고 있으므로 MuZero가 우선순위는 아니다.
- 다만 “복잡한 평가 함수를 learned latent로 대체한다”는 장기 아이디어는 유용하다.

Winters MOBA:
- 전체 LoL GameSim을 tree search에 복사하기 어렵다면, learned dynamics가 대안이 될 수 있다.
- 예: `BattleState + action -> 0.2초 뒤 HP/position/value`를 예측하는 작은 model.
- 단, 초반에는 learned model보다 deterministic micro simulator가 먼저다.

## 구현 과제

1. Mushroom에서는 MuZero를 바로 구현하지 않는다.
2. `State -> value` supervised model부터 만든다.
3. 이후 `State + Move -> next feature delta` 예측 toy model을 만든다.
4. Winters에서는 champion micro combat transition model을 장기 과제로 둔다.

## 읽기 체크리스트

- MuZero가 실제 next observation 전체를 예측하지 않아도 되는 이유를 설명할 수 있는가?
- learned dynamics와 real simulator의 차이를 설명할 수 있는가?
- Mushroom에서 MuZero가 과한 이유를 말할 수 있는가?
- Winters에서 learned micro simulator가 필요한 순간은 언제인가?

