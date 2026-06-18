# PPO

원문:
- arXiv: https://arxiv.org/abs/1707.06347
- PDF: https://arxiv.org/pdf/1707.06347
- OpenAI 소개: https://openai.com/index/openai-baselines-ppo/

한 줄:
- policy를 너무 크게 바꾸지 않도록 clipping하면서 안정적으로 강화학습하는 정책경사 알고리즘.

## 한국어 핵심

PPO는 “정책을 업데이트하되 한 번에 너무 멀리 가지 말자”는 알고리즘이다. 이전 정책과 새 정책의 action probability 비율을 보고, 그 비율이 일정 범위를 넘으면 objective를 잘라낸다. 구현이 비교적 간단하고 안정적이라 OpenAI Five 같은 대규모 RL에서도 쓰였다.

## 알고리즘 구조

핵심 값:

```text
r_t(theta) = pi_theta(a_t | s_t) / pi_old(a_t | s_t)
advantage = A_t
clipped objective = min(r_t * A_t, clip(r_t, 1-eps, 1+eps) * A_t)
```

필요 구성:
- policy network
- value network
- rollout buffer
- advantage estimation
- entropy bonus
- clipped policy loss
- value loss

## MobaZero 적용 포인트

NYPC Lab:
- Mushroom에서는 PPO보다 imitation/value supervised learning이 먼저다.
- PPO는 environment API와 reward 검증이 끝난 뒤 실험한다.

Winters MOBA:
- OpenAI Five를 이해하려면 PPO가 필요하다.
- 하지만 PPO는 reward hacking에 취약하므로 replay viewer와 debug trace가 필수다.

## 구현 과제

1. toy environment에서 PPO 수식 직접 구현 또는 Stable-Baselines3 사용.
2. legal action mask를 PPO policy에 어떻게 반영할지 결정.
3. reward가 NaN/inf 없이 안정적인지 검사.
4. trained policy를 frozen baseline과 holdout league에서 비교.

## 읽기 체크리스트

- PPO clipping이 왜 필요한지 설명할 수 있는가?
- advantage가 무엇인지 설명할 수 있는가?
- value function이 policy 학습에 왜 필요한가?
- legal action mask가 없는 PPO가 게임에서 왜 위험한가?

