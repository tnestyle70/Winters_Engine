# Stage 7 — 모방 학습 (Imitation Learning / Behavior Cloning)

## 목표

실제 플레이어의 게임 로그에서 **행동 분포**를 학습. 룰 기반 AI 에 인간다움 주입.

## 왜 Imitation 인가

- 룰 기반 (Stage 0~6) 만으론 "자연스러움" 한계 — 너무 최적화돼서 로봇처럼 보임
- 실제 플레이어는 실수/머뭇거림/습관 있음 → 봇이 흉내내면 진짜 사람 같음
- RL (Stage 8) 의 **Warm-Start** 로 활용 — 0부터 학습하는 것보다 빠름
- OpenAI Five 도 초기 Imitation 으로 시작

## 파이프라인

```
[플레이어 게임 로그 수집]
         ↓
[특징 추출 (State Vector)]
         ↓
[액션 벡터 라벨링]
         ↓
[ML 학습 (Python)]
         ↓
[ONNX 모델 저장]
         ↓
[Winters Engine 에서 로드 → 추론]
```

## 로그 수집

### 서버 측

```cpp
// Server/Private/AI/BotLogCollector.cpp
class CBotLogCollector
{
public:
    // 매 게임 틱 호출 (50ms)
    void RecordTick(const ServerWorld& world, u32_t matchID, u32_t tickID);

    void FlushToDisk(const std::string& path);  // 게임 종료 시

private:
    struct Frame {
        u32_t   tick;
        std::array<PlayerState, 10> players;
        std::array<PlayerInput, 10> inputs;   // 각 플레이어가 이 tick 에 뭘 했는지
        ObjectiveState objectives;
    };
    std::vector<Frame> m_frames;
};
```

수집 대상:
- 플레이어 입력 (WASD 이동, Q/W/E/R, 평타, 핑)
- 월드 상태 (모든 엔티티 위치/체력/마나/쿨다운)
- 메타 이벤트 (킬/데스/오브젝트 처치)

FlatBuffers 스키마로 직렬화 → `.replay` 파일.

### 클라이언트 측

- 개발 빌드만: 로컬 플레이 로그 저장 (연습모드에서 테스트)
- 릴리스: 서버만 수집 (용량 문제)

## 특징 추출 (Feature Engineering)

### 상태 벡터 (State) — 약 200~500 차원

각 플레이어 시점에서:

| 카테고리 | 차원 | 내용 |
|---|---|---|
| 자기 자신 | 40 | HP, MP, 레벨, 골드, 아이템 6슬롯, 쿨다운 6개, 위치(x,y) |
| 팀원 4명 | 40 × 4 | 각자 위 상태 (팀원 시점) |
| 적 5명 | 40 × 5 | 보이는 적만 (fog 고려) |
| 오브젝트 | 30 | 포탑 18개 HP, 드래곤/바론 상태, 미니언 웨이브 위치 |
| 시간/경제 | 10 | 게임 시간, 팀 골드 차이, 킬 스코어 |
| 맵 컨텍스트 | 100 | 주변 그리드 Influence Map 덤프 (10x10) |

### 액션 벡터 (Action) — 약 15 차원

```
action = [
    move_dir_x, move_dir_y,          // 이동 방향 (-1~1)
    cast_q, cast_w, cast_e, cast_r,  // 스킬 사용 (0/1)
    skill_target_x, skill_target_y,   // 스킬 조준 좌표
    basic_attack_target_id,           // 평타 대상 엔티티 ID
    use_summoner_d, use_summoner_f,   // D/F 소환사 주문
    ping_x, ping_y, ping_type        // 핑
]
```

Continuous + Discrete 혼합 → 디스크리트 부분은 softmax, continuous 는 gaussian.

## 학습 (Python — 별도 프로세스)

```python
# Tools/AI/train_imitation.py
import torch
import torch.nn as nn

class BotPolicy(nn.Module):
    def __init__(self, state_dim=400, action_dim=15):
        super().__init__()
        self.fc1 = nn.Linear(state_dim, 512)
        self.fc2 = nn.Linear(512, 256)
        self.fc3 = nn.Linear(256, action_dim)
    
    def forward(self, state):
        x = torch.relu(self.fc1(state))
        x = torch.relu(self.fc2(x))
        return self.fc3(x)

# 학습 루프 (Behavior Cloning)
def train(model, dataloader, epochs=50):
    opt = torch.optim.Adam(model.parameters(), lr=1e-4)
    for epoch in range(epochs):
        for state, action_expert in dataloader:
            pred = model(state)
            loss = ((pred - action_expert)**2).mean()
            opt.zero_grad()
            loss.backward()
            opt.step()

# ONNX 로 내보내기
dummy_input = torch.randn(1, 400)
torch.onnx.export(model, dummy_input, "bot_policy.onnx")
```

## 엔진 통합 (C++ 추론)

### ONNX Runtime 사용

```cpp
// Engine/Public/AI/Imitation/ONNXRuntime.h
#include <onnxruntime_cxx_api.h>

class CONNXRuntime
{
public:
    static unique_ptr<CONNXRuntime> Create(const std::string& modelPath);

    // 입력: 400-dim state → 출력: 15-dim action
    std::vector<f32_t> Infer(const std::vector<f32_t>& state);

private:
    CONNXRuntime() = default;

    std::unique_ptr<Ort::Env> m_env;
    std::unique_ptr<Ort::Session> m_session;
    Ort::MemoryInfo m_memInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
};
```

### 봇 ECS 컴포넌트

```cpp
struct NeuralPolicyComponent
{
    u32_t                 modelID;
    std::vector<f32_t>    lastAction;
    f32_t                 lastInferenceTime = 0.f;
    f32_t                 inferenceInterval = 0.1f;   // 100ms 간격 (10 Hz)
};

class CNeuralPolicySystem : public ISystem
{
public:
    void Execute(CWorld& world, f32_t dt) override
    {
        world.ForEach<NeuralPolicyComponent, BotComponent>(
            [&](EntityID e, NeuralPolicyComponent& n, BotComponent& b)
        {
            n.lastInferenceTime += dt;
            if (n.lastInferenceTime < n.inferenceInterval) return;
            n.lastInferenceTime = 0.f;

            std::vector<f32_t> state = ExtractStateVector(world, e);
            n.lastAction = m_runtime.Infer(state);

            // 액션을 기존 시스템 (BT/FSM) 의 힌트로 사용
            ApplyNeuralActionAsPrior(world, e, n.lastAction);
        });
    }

private:
    CONNXRuntime m_runtime;
};
```

## 학습된 정책의 역할

**단독 결정 X, 다른 Stage 의 Prior 로 사용**:

1. **MCTS 의 Selection Prior**: `UCB1 + α × NN(action | state)`
2. **Utility 의 가중치 튜닝**: 특정 상황 출현 빈도 매핑
3. **BT 의 조건 판정**: "지금 이 상황에서 평균적 플레이어가 공격할까 후퇴할까?" 비교

## 데이터 수집 전략

### 초기 (더미 데이터)

- Stage 0~6 구현된 봇끼리 Self-Play → 로그 생성
- "로봇같지만 규칙 충실" 한 데이터로 Baseline 학습

### 중기 (개발팀 플레이)

- 개발 과정에서 봇전 직접 플레이한 로그 수집
- 수백 게임 → 기본 훈련

### 후기 (베타 테스터)

- 실제 플레이어 로그 → 다양성 확보
- 개인정보 익명화

## 난이도 생성

- **Intro/Beginner**: 학습 데이터 중 하위 티어 (브론즈/실버) 만 사용
- **Intermediate**: 골드/플래티넘 로그
- **Master**: 다이아+ 로그
- **Grandmaster**: 챌린저 로그 + Self-Play 강화

## 구현 순서

1. FlatBuffers 로그 스키마 정의 (`.replay` 포맷)
2. Server 측 `CBotLogCollector` 구현
3. Python 데이터 로더 + 간단 BC 학습
4. ONNX 모델 export
5. C++ 측 `CONNXRuntime` 래퍼
6. `CNeuralPolicySystem` 추론 통합
7. Stage 0~6 봇 결과와 혼합 (Prior 방식)
8. 실제 플레이어 로그 수집 파이프라인 (Phase 4 네트워크 이후)
