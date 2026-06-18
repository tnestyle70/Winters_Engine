# RLCard

원문/소스:
- arXiv: https://arxiv.org/abs/1910.04376
- GitHub: https://github.com/datamllab/rlcard

한 줄:
- Blackjack, Leduc Hold'em, Texas Hold'em, Dou Dizhu, Mahjong, UNO 등 카드 게임 RL 실험을 위한 플랫폼.

## 한국어 핵심

RLCard는 불완전정보, 확률, legal action mask가 있는 게임을 RL로 실험하는 구조를 보여준다. NYPC에서 Yacht Auction, poker류, hidden bag류 게임을 준비할 때 좋은 참고가 된다. 특히 game engine, agent, training, evaluation을 분리하는 방식이 중요하다.

## 구조 포인트

핵심 요소:
- environment
- state extractor
- legal actions
- agent interface
- trajectory
- evaluation
- baseline algorithms

## MobaZero 적용 포인트

NYPC:
- Yacht, Connexion, 카드류 문제는 RLCard식 environment와 legal action mask가 중요하다.
- random/heuristic/CFR/DQN/NFSP 같은 baseline을 같은 환경에서 비교할 수 있어야 한다.

Winters:
- 직접 RLCard를 쓰지는 않는다.
- 하지만 champion action mask, skill cooldown mask, target validity mask 설계에 참고한다.

## 구현 과제

1. Mushroom legal action mask를 RLCard식으로 정리.
2. Yacht score/bid action mask 설계.
3. hidden-info toy card game을 NYPC Lab에 추가.
4. evaluation tournament와 exploitability/regret 지표 공부.

## 읽기 체크리스트

- 카드 게임에서 legal action mask가 왜 중요한가?
- trajectory를 어떻게 supervised/RL dataset으로 바꿀 수 있는가?
- imperfect information game에서 observation이 state보다 작은 이유를 설명할 수 있는가?
- Yacht/Connexion에 어떤 구조를 빌려올 것인가?

