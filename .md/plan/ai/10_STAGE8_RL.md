# Stage 8 — 강화학습 (RL, Reinforcement Learning) — 선택

## 목표

Self-Play 환경에서 봇이 **스스로 강화**. OpenAI Five / AlphaStar 방식.

⚠️ **연구 수준 도전 과제**. Phase F 의 다른 Stage 가 먼저 안정화된 뒤에 시도.

## 왜 RL 인가

- Imitation (Stage 7) 은 데이터 상한 = 인간 상한
- RL 은 Self-Play 로 **인간 이상 성능** 가능
- AlphaGo 가 Go, AlphaStar 가 SC2, OpenAI Five 가 Dota 에서 입증

## 왜 "선택" 인가

- 연산 자원 부담 큼 (GPU 며칠/수주)
- 환경 시뮬레이터 완성도 요구 (Stage 6 MCTS 시뮬레이터의 고도화판)
- 게임 밸런스 변경될 때마다 재훈련 필요

## 알고리즘: PPO (Proximal Policy Optimization)

OpenAI Five 에서 사용. 구현 간단, 안정적.

```
1. 현재 정책 π_θ 로 N 게임 플레이 → 경험 수집 (state, action, reward)
2. Advantage 추정 (GAE)
3. Loss = min(r × A, clip(r, 1-ε, 1+ε) × A)
   where r = π_new(a|s) / π_old(a|s)
4. θ 업데이트 (Adam)
5. Iterate
```

## 환경 인터페이스

```cpp
// Engine/Public/AI/RL/IBotEnv.h
class IBotEnv
{
public:
    virtual ~IBotEnv() = default;

    // 환경 리셋 → 초기 상태 반환
    virtual std::vector<f32_t> Reset() = 0;

    // 한 스텝 진행
    struct StepResult {
        std::vector<f32_t>  nextState;
        f32_t               reward;
        bool_t              done;
        std::string         info;
    };
    virtual StepResult Step(const std::vector<f32_t>& action) = 0;

    virtual i32_t GetStateDim() const  = 0;
    virtual i32_t GetActionDim() const = 0;
};
```

## 보상 설계 (Reward Shaping)

```
reward = 
    + 100    × enemy_champion_killed
    + 10     × enemy_minion_killed
    + 200    × enemy_tower_destroyed
    + 300    × enemy_inhibitor_destroyed
    + 5000   × enemy_nexus_destroyed       ← 승리
    - 50     × ally_death
    - 200    × ally_tower_lost
    - 5000   × ally_nexus_destroyed        ← 패배
    + 1      × gold_gained_this_tick
    + 0.5    × xp_gained_this_tick
    + 0.1    × hp_gained_this_tick
    - 2      × hp_lost_this_tick
    + team_reward / 5                      ← 팀 보상 공유
```

실제 RL 에선 reward hacking 주의 — 과한 shaping 은 학습 방해.

## 분산 Self-Play 아키텍처

```
        ┌─────────────────┐
        │   Learner       │   PyTorch, GPU
        │   (PPO 학습)    │
        └────────┬────────┘
                 │ (weights broadcast)
      ┌──────────┼──────────┐
      ↓          ↓          ↓
  ┌────────┐ ┌────────┐ ┌────────┐
  │Worker 1│ │Worker 2│ │Worker N│  ← Docker 컨테이너 (CPU)
  │(게임 시뮬)│ │...  │ │   │
  └────┬────┘ └────────┘ └────────┘
       │ (experience)
       └──────────────────────────→ Replay Buffer → Learner
```

- Workers = 여러 봇전 매치 병렬 실행
- Learner = 정기적으로 Worker 에서 경험 수집 → 정책 업데이트
- OpenAI Five 는 수천 개 CPU + 256 GPU 사용. Winters 는 훨씬 작은 스케일.

## Python 학습 코드 스켈레톤

```python
# Tools/AI/train_rl.py
from stable_baselines3 import PPO
from winters_env import WintersBotEnv   # C++ 환경을 Python 에 bind

env = WintersBotEnv(port=8090)   # C++ 엔진의 RL 서버에 연결

model = PPO(
    "MlpPolicy", env,
    learning_rate=1e-4,
    n_steps=2048,
    batch_size=256,
    n_epochs=10,
    gamma=0.99,
    gae_lambda=0.95,
    clip_range=0.2,
    ent_coef=0.01,
)

# Behavior Cloning warm-start (Stage 7 에서 저장한 모델)
model.policy.load_state_dict(torch.load("bot_policy_bc.pt"))

model.learn(total_timesteps=100_000_000)
model.save("bot_policy_rl.zip")

# ONNX export
import torch.onnx
dummy = torch.randn(1, 400)
torch.onnx.export(model.policy, dummy, "bot_policy_rl.onnx")
```

## 환경 서버 (C++)

```cpp
// Engine/Public/AI/RL/RLServer.h
// 파이썬 학습 프로세스와 gRPC/IPC 통신
class CRLServer
{
public:
    static unique_ptr<CRLServer> Create(u32_t port);

    void Run();   // 블로킹 — 게임 시뮬레이션 실행 + 요청 처리

    // Python 이 호출:
    //   Reset() → initial state
    //   Step(action) → (nextState, reward, done)
};
```

각 Worker 프로세스는 Client 엔진을 **Headless 모드**로 실행하며 RL 서버 포트로 통신.

## Curriculum Learning

점진적으로 어려운 환경:

1. **Phase 1**: 1v1 미드 라인전만 (정글몹/포탑 없음)
2. **Phase 2**: 1v1 + 정글몹
3. **Phase 3**: 2v2 (미드 + 정글)
4. **Phase 4**: 5v5 전체
5. **Phase 5**: 상대 실력 점진 상승

## Counter-Play 학습 (League of Legends Specific)

OpenAI Five 가 놓쳤던 것: **적의 픽에 따른 대응**. 
- 챔피언 조합 → 메타 학습
- 아이템 빌드 상황 대응
- Nash Equilibrium 탐색 (AlphaStar 의 League Training 참고)

## 온라인 학습 (선택)

릴리스 후 실제 플레이어 데이터로 지속 학습 — 메타 변화 대응. 단, 서버 인프라 부담 큼.

## 구현 순서 (매우 장기)

1. Stage 7 Imitation 안정화 (warm-start 데이터)
2. C++ 환경 서버 (`IBotEnv` 구현)
3. Python 측 환경 바인딩 (gRPC 또는 ZMQ)
4. 1v1 PPO 학습 스크립트
5. GPU 환경에서 며칠~주 학습
6. ONNX export → 엔진에서 추론
7. 점진적 Curriculum 확장 (2v2 → 5v5)
8. Counter-Play / League Training (AlphaStar 스타일)

## 주의사항

- **Reproducibility**: 훈련 재현 가능한 시드/환경 버전 관리
- **Evaluation**: 새 정책이 기존보다 진짜 나은지 통계 검증 (Elo 레이팅)
- **Safety**: RL 봇이 exploit 찾아 이상한 플레이 할 수 있음 → 룰 기반 가드레일 병행
- **정치**: 릴리스 시 "AI 봇이 너무 셈" 불만 가능 → 난이도 옵션 필수

## 연구 자료

- OpenAI Five (2019): https://arxiv.org/abs/1912.06680
- AlphaStar (2019): Nature 논문
- DeepMind For The Win (2018): Quake III Capture the Flag
- PPO 원논문: Schulman et al. 2017
- Stable-Baselines3: https://stable-baselines3.readthedocs.io/
