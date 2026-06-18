# PettingZoo

원문/소스:
- arXiv: https://arxiv.org/abs/2009.14471
- Docs: https://pettingzoo.farama.org/
- GitHub: https://github.com/Farama-Foundation/PettingZoo

한 줄:
- multi-agent reinforcement learning 환경을 위한 표준 Python API.

## 한국어 핵심

PettingZoo는 Gym이 single-agent RL에 제공한 역할을 multi-agent RL에 제공하려는 프로젝트다. 여러 agent가 순서대로 행동하거나 병렬로 행동하는 환경을 표준 API로 다룬다. MOBA 5인 팀 AI나 NYPC 다중 플레이어 게임을 생각할 때 API 설계 참고 가치가 크다.

## 구조 포인트

주요 개념:
- agent list
- observation per agent
- action per agent
- reward per agent
- termination/truncation
- AEC API
- parallel API

## MobaZero 적용 포인트

NYPC:
- 2인 순차 게임은 간단하지만, 동시 입찰/다자 게임이 나오면 multi-agent API가 필요하다.
- agent별 observation과 reward를 분리하는 습관을 들인다.

Winters MOBA:
- 10명의 champion agent가 모두 같은 global state를 보아서는 안 된다.
- 각 agent는 자기 팀 정보, 시야 정보, ping/blackboard, last seen 정보를 가진 observation을 받는다.

## 구현 과제

1. NYPC toy multi-agent environment 작성.
2. sequential mode와 simultaneous mode를 구분.
3. Winters TeamBlackboard를 observation의 일부로 보는 문서 작성.
4. FOW 밖 정보가 observation에 들어가지 않는지 검증.

## 읽기 체크리스트

- multi-agent에서 global state와 per-agent observation의 차이를 설명할 수 있는가?
- AEC 방식과 parallel 방식의 차이를 설명할 수 있는가?
- MOBA에서 reward를 개인/팀으로 어떻게 나눌 것인가?
- TeamBlackboard는 state인가 observation인가?

