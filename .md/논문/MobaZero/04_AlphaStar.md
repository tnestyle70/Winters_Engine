# AlphaStar

원문:
- Nature: https://www.nature.com/articles/s41586-019-1724-z
- DeepMind 소개: https://deepmind.google/en/blog/alphastar-grandmaster-level-in-starcraft-ii-using-multi-agent-reinforcement-learning/

한 줄:
- StarCraft II에서 imitation learning, reinforcement learning, league training을 결합해 Grandmaster 수준의 multi-agent AI를 만든 사례.

## 한국어 핵심

AlphaStar는 OpenAI Five와 함께 “복잡한 실시간 전략 게임에서 AI를 어떻게 훈련하는가”를 보여준다. 특히 중요한 것은 league training이다. 하나의 Agent가 계속 자기 자신과만 싸우면 특정 전략에 과적합될 수 있다. AlphaStar는 다양한 main agent, exploiter, league agent를 두어 전략과 반전략이 같이 발전하게 만들었다.

## 알고리즘 구조

핵심 요소:
- human replay imitation으로 warm start
- reinforcement learning self-play
- league of agents
- exploiter/counter-strategy
- partial observation handling
- long-horizon strategic behavior

## MobaZero 적용 포인트

Winters MOBA:
- 5인 팀 AI는 하나의 최강 정책보다 opponent pool과 league가 중요하다.
- 특정 scripted team만 이기는 봇은 약하다.
- 다양한 메타, 조합, 라인전, 오브젝트 상황을 상대 pool에 보존해야 한다.

NYPC Lab:
- Mushroom에서도 previous best, alpha_beta, minimax, reply_aware, tuned variants를 league pool로 유지한다.
- 새 Agent는 current baseline만 이기는 것이 아니라 frozen champion들을 이겨야 한다.

## 구현 과제

1. NYPC `opponent_pool.json` 또는 manifest를 만든다.
2. 각 Agent 버전을 frozen artifact로 저장한다.
3. 리그 결과를 Elo/score diff/confidence로 정리한다.
4. Winters는 champion composition과 role assignment를 league axis로 둔다.

## 읽기 체크리스트

- league training이 단순 self-play와 다른 점을 설명할 수 있는가?
- exploiter agent가 왜 필요한가?
- imitation warm start가 RL에서 어떤 역할을 하는가?
- Winters에서 “특정 플레이만 이기는 봇”을 어떻게 걸러낼 것인가?

