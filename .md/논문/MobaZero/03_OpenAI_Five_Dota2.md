# OpenAI Five / Dota 2

원문:
- arXiv: https://arxiv.org/abs/1912.06680
- PDF: https://arxiv.org/pdf/1912.06680
- OpenAI 자료: https://openai.com/index/dota-2-with-large-scale-deep-reinforcement-learning/

한 줄:
- Dota 2라는 MOBA 환경에서 대규모 self-play PPO로 팀 단위 초인급 플레이를 만든 사례.

## 한국어 핵심

OpenAI Five는 MOBA AI를 이해할 때 피할 수 없는 기준점이다. 실시간, 부분 관측, 긴 지평, 많은 행동, 팀 협동, sparse reward가 모두 섞여 있다. 핵심은 사람이 모든 규칙을 hand-code하지 않고, 엄청난 self-play와 PPO로 정책을 강화했다는 점이다.

하지만 이 사례는 단순히 “RL을 돌리면 된다”가 아니다. 계산 자원, 환경 안정성, action abstraction, reward shaping, opponent pool, evaluation이 모두 있어야 한다.

## 알고리즘 구조

핵심 요소:
- PPO 기반 policy optimization
- self-play
- LSTM memory
- team reward
- action space 제한과 abstraction
- 대규모 분산 rollout
- 인간 프로팀 상대 평가

## MobaZero 적용 포인트

Winters MOBA:
- 최종 북극성에 가장 가깝다.
- 하지만 바로 5v5 full game PPO로 가면 실패한다.
- 먼저 `1v1 lane`, `2v2 skirmish`, `objective fight`, `5v5 scripted macro`로 curriculum을 나눈다.

NYPC Lab:
- 대규모 RL이 아니라, self-play league와 policy/value dataset의 운영 방식을 배운다.
- “Agent가 강해졌다는 증거”는 단일 승리가 아니라 holdout league와 regression report다.

## 구현 과제

1. Winters에서 headless deterministic simulator가 필요하다.
2. observation/action/reward schema를 고정해야 한다.
3. TeamBlackboard와 learned policy의 역할을 나눠야 한다.
4. 사람이 볼 수 있는 AI Debug/Replay가 있어야 reward hacking을 잡을 수 있다.

## 읽기 체크리스트

- 왜 OpenAI Five가 PPO를 썼는지 설명할 수 있는가?
- Dota action space를 그대로 쓰지 않고 제한한 이유를 설명할 수 있는가?
- team reward가 credit assignment 문제를 어떻게 어렵게 만드는가?
- Winters에서 full RL 전에 어떤 curriculum을 먼저 해야 하는가?

