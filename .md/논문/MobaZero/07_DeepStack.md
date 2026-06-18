# DeepStack

원문:
- arXiv: https://arxiv.org/abs/1701.01724
- PDF: https://arxiv.org/pdf/1701.01724

한 줄:
- Heads-up no-limit poker에서 depth-limited solving과 neural value function을 결합해 전문가 수준을 달성한 시스템.

## 한국어 핵심

DeepStack은 불완전정보 게임에서 전체 게임 트리를 한 번에 풀지 않는다. 현재 상황에서 제한된 깊이까지만 search하고, 그 이후의 가치는 neural network가 평가한다. poker는 상대의 private card를 모르므로 belief와 counterfactual value가 중요하다.

## 알고리즘 구조

핵심 요소:
- continual re-solving
- depth-limited search
- neural counterfactual value estimator
- imperfect information abstraction
- 실시간 의사결정

## MobaZero 적용 포인트

NYPC:
- Connexion, Yacht Auction, poker류 불완전정보 문제에서 중요하다.
- hidden state 전체를 정확히 알 수 없을 때 belief sampling이나 abstraction이 필요하다.

Winters MOBA:
- 전장의 안개는 일종의 불완전정보다.
- 적 위치와 쿨타임을 완전히 아는 봇은 치트다.
- last seen, belief, uncertainty를 value model에 넣어야 한다.

## 구현 과제

1. FOW에서 `LastKnownEnemyState`와 uncertainty decay 설계.
2. hidden information game toy example 작성.
3. depth-limited search 뒤 value estimator를 붙이는 구조 실험.
4. Mushroom보다는 Connexion/Yacht 준비 때 우선 적용.

## 읽기 체크리스트

- perfect information search와 imperfect information search의 차이를 설명할 수 있는가?
- continual re-solving이 왜 필요한가?
- FOW에서 적 위치를 어떻게 belief로 표현할 것인가?
- Winters 봇이 알면 안 되는 정보 목록을 만들었는가?

