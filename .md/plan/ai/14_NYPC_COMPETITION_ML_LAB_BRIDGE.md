# Stage Bridge — NYPC Competition ML Lab에서 Winters LoL AI로

작성일: 2026-06-06

이 문서는 `C:\Users\tnest\Desktop\NYPC\mushroom`에서 진행하는 독립 Competition ML Lab과 Winters Engine AI 계획 사이의 경계와 통합 순서를 정리한다.

## 1. 경계 원칙

NYPC Lab은 Winters Engine 내부 기능이 아니라 독립 실험실이다.

금지:
- 사용자가 명시적으로 Winters 통합 세션을 요청하기 전에는 `C:\Users\tnest\Desktop\Winters` 내부 런타임/소스 코드를 수정하지 않는다.
- NYPC 실험 코드, Python 학습 스크립트, 대회용 C++ 제출 코드를 Winters Engine에 직접 섞지 않는다.
- 턴제 게임에서 검증되지 않은 ML/RL 구조를 LoL GameSim이나 Client runtime에 먼저 붙이지 않는다.

허용:
- Winters 문서에 경계와 장기 방향을 기록한다.
- Winters AI 설계 문서를 참고해 NYPC Lab의 MCTS/RL/Imitation 구조를 설계한다.
- NYPC Lab에서 검증된 개념을 나중에 별도 계획으로 Winters에 이식한다.

## 2. 왜 NYPC Lab을 먼저 하는가

LoL은 실시간, 다중 유닛, 부분 관측, 긴 지평, 연속 행동, 팀 협동, 서버 권위가 모두 섞인 어려운 환경이다. 바로 LoL 완전 모델 학습으로 들어가면 규칙, 상태, 행동, 보상, 로그, 시각화, 검증이 한꺼번에 무너질 수 있다.

따라서 먼저 턴제 게임에서 아래 축을 독립 검증한다.

```text
Complete Game Model
-> deterministic replay
-> legal action mask
-> search expert
-> self-play league
-> feature dataset
-> imitation / value learning
-> RL environment
-> artifact export
```

버섯 게임은 완전정보/결정론/순차 게임의 기준 모델이다.
Yacht Dice/Auction은 확률/점수판/동시 입찰/CFR 계열 기준 모델이다.
Connexion은 불완전정보/타일 배치/연결 성분/ISMCTS 계열 기준 모델이다.

## 3. Winters로 가져올 때의 목표

NYPC Lab에서 검증된 것은 Winters의 다음 구조로 이식한다.

```text
NYPC Complete Game Model
-> Winters Shared/GameSim deterministic contract
-> Bot state/action feature extractor
-> Search expert / MCTS tactical simulator
-> Imitation / value / policy training
-> ONNX or baked lightweight inference
-> Server-authoritative Bot command generation
-> Client debug visualization
```

통합 원칙:
- gameplay truth는 `Shared/GameSim`과 Server에 둔다.
- Client는 presentation, weak prediction, debug visualization만 담당한다.
- Bot AI는 truth component를 직접 고치지 않고 command/intention을 생산한다.
- 학습은 Python/offline에서 수행하고, runtime은 경량 추론 또는 baked policy를 우선한다.

## 4. 구현 계획: PyTorch 환경 세션

목표:
- 현재 `torch=missing` 상태를 `torch=installed`로 바꾼다.
- NYPC `MLHelloWorldTorch.py`가 실제 학습까지 수행하도록 한다.

산출물:
- Python interpreter 확인 문서
- PyTorch 설치 명령 기록
- `scripts/check_ml_env.py` 결과
- `run_ml_foundations.ps1` 검증 로그

검증:
- `python -c "import torch; print(torch.__version__)"` 통과
- `torch=installed`
- `MLHelloWorldTorch.py`에서 `verification passed`

다음 단계 진입 조건:
- PyTorch가 같은 PowerShell/PyCharm interpreter에서 모두 import된다.

## 5. 구현 계획: Policy / Value Model 입문

목표:
- 단순 선형회귀를 넘어, 게임 상태를 보고 행동 점수 또는 상태 가치를 예측하는 작은 모델을 만든다.

NYPC Lab 산출물:
- toy policy dataset
- toy value dataset
- `TrainToyPolicy.py`
- `TrainToyValue.py`
- validation accuracy / loss report

Winters 관점 학습 포인트:
- `StateVector`: Bot이 보는 세계 요약
- `ActionMask`: 불가능한 행동 제거
- `Policy`: 어떤 행동을 고를지
- `Value`: 현재 상태가 좋은지
- `Inference`: 학습된 모델을 runtime에서 평가하는 것

검증:
- toy task에서 overfit 가능한 작은 dataset을 먼저 100%에 가깝게 맞춘다.
- validation set에서 loss가 내려가는지 확인한다.
- illegal action mask 후 illegal action 선택률이 0인지 확인한다.

다음 단계 진입 조건:
- feature, label, loss, mask, validation의 의미를 설명할 수 있다.

## 6. 구현 계획: 버섯 Complete Game Model

목표:
- 버섯 게임을 ML/RL/MCTS가 사용할 수 있는 완전정보 턴제 환경으로 만든다.

산출물:
- state representation
- legal move generator
- apply move
- score / terminal
- replay runner
- structured turn log
- simple board visualization

검증:
- official testing tool과 점수/종료 판정 일치
- 같은 replay를 두 번 돌리면 같은 state hash
- random board 수천 개 crash 0, illegal 0

다음 단계 진입 조건:
- Agent가 약해도 환경이 틀렸다는 의심 없이 학습 데이터를 만들 수 있다.

## 7. 구현 계획: Search Expert / MCTS

목표:
- ML의 teacher가 될 강한 탐색 Agent를 만든다.

산출물:
- greedy tactical oracle
- alpha-beta / beam search
- MCTS UCB1 prototype
- root child score/visit log
- expert move dataset

검증:
- baseline 대비 통계적 우위
- 같은 seed에서 같은 move
- time budget 내 p95/p99 안정
- expert label을 dataset으로 저장

다음 단계 진입 조건:
- imitation learning에 사용할 `state/action -> expert move` 데이터가 있다.

## 8. 구현 계획: Imitation Learning

목표:
- Search Expert가 고른 수를 policy model이 따라 배우게 한다.

산출물:
- state feature extractor
- action feature extractor
- legal action mask
- expert action label
- PyTorch policy trainer
- validation report

검증:
- top-1 / top-3 expert accuracy 기록
- illegal action mask 후 illegal 0
- policy prior + search가 no-prior search보다 빠르거나 강함

다음 단계 진입 조건:
- policy model이 단순한 장난감이 아니라 league 결과를 개선한다.

## 9. 구현 계획: PPO / RL

목표:
- Complete Game Model 위에 self-play RL 환경을 만든다.

산출물:
- Gymnasium-style environment
- observation vector
- discrete action mapping
- legal action mask
- reward definition
- PPO training script
- opponent pool
- replay / reward hacking report

검증:
- 같은 seed episode 재현
- reward NaN/inf 0
- frozen baseline 대비 holdout league 우위
- 이상한 reward hacking replay를 볼 수 있음

다음 단계 진입 조건:
- RL이 complexity만 늘리는지, 실제 league 성능을 올리는지 숫자로 판단 가능하다.

## 10. Winters 통합 세션 진입 조건

Winters runtime/source code 수정은 아래 조건을 만족하고 사용자가 명시적으로 요청할 때만 시작한다.

- NYPC Lab에서 Complete Game Model, replay, log, league가 안정화됨
- Search Expert 또는 policy/value model이 baseline을 통계적으로 이김
- feature/action/reward 설계가 문서화됨
- runtime 추론 또는 baked artifact 방식이 결정됨
- `Shared/GameSim` 서버 권위 경계를 깨지 않는 통합 계획이 있음

통합 첫 세션의 목표는 LoL 전체 RL이 아니다. 첫 목표는 작은 `BattleState` 또는 `BotDecisionContext`를 만들어 deterministic tactical simulator와 debug snapshot을 연결하는 것이다.
