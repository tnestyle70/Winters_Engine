# OpenSpiel

원문/소스:
- GitHub: https://github.com/google-deepmind/open_spiel
- 논문/프로젝트명: OpenSpiel: A Framework for Reinforcement Learning in Games

한 줄:
- 다양한 게임, search, RL, game theory 알고리즘을 같은 `Game`/`State` 추상화 아래 실험할 수 있게 만든 프레임워크.

## 한국어 핵심

OpenSpiel은 우리가 만들고 싶은 NYPC Competition ML Lab의 좋은 참고 구조다. 핵심은 게임별 로직과 알고리즘을 분리하는 것이다. 알고리즘은 특정 게임을 몰라도 `State`, `LegalActions`, `ApplyAction`, `IsTerminal`, `Returns` 같은 공통 API를 호출한다.

## 구조 포인트

중요한 추상화:
- Game
- State
- legal actions
- chance node
- information state
- observation
- returns
- algorithms

## MobaZero 적용 포인트

NYPC Lab:
- Mushroom, Yacht, Connexion을 같은 interface에 꽂는다.
- match/league/report는 게임별 코드를 몰라야 한다.
- hidden information game을 위해 observation/information_state를 처음부터 고려한다.

Winters:
- `BotDecisionContext`를 OpenSpiel의 `State`처럼 설계할 수 있다.
- 단, Winters runtime은 서버 권위 GameSim과 연결되므로 Engine 코드에 실험용 구조를 바로 섞지 않는다.

## 구현 과제

1. NYPC Lab에 최소 `GameSpec`/`State` interface 문서화.
2. Mushroom adapter 작성.
3. toy stochastic game과 hidden-info toy game 추가.
4. search algorithms가 game-specific import 없이 돌게 한다.

## 읽기 체크리스트

- Game과 State를 분리하는 이유를 설명할 수 있는가?
- legal action mask가 algorithm interface에 왜 필요한가?
- hidden information game에서 observation과 state가 왜 다른가?
- NYPC Lab의 현재 구조가 OpenSpiel식으로 어디까지 맞는가?

