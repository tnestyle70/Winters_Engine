# Codex AI Implementation Index

기준 폴더: `.md/plan/ai/codex/`

이 폴더는 Phase F AI 문서를 실제 코드 작업으로 옮길 때 필요한 handoff와 구현 순서를 정리한다.

---

## Core Documents

| 문서 | 범위 | 목적 |
|---|---|---|
| [AI_CORE_HFSM_BT_GOAP_UTILITY_INFLUENCE_SCAFFOLD_PLAN.md](AI_CORE_HFSM_BT_GOAP_UTILITY_INFLUENCE_SCAFFOLD_PLAN.md) | Aggro / HFSM / BT / Utility / Influence Map / GOAP / Blackboard / Imitation bridge | MCTS/RL 이전 AI 프레임워크와 제품별 어댑터 구조 |
| [MCTS_RL_AI_IMPLEMENTATION_PLAN.md](MCTS_RL_AI_IMPLEMENTATION_PLAN.md) | MCTS / RL / FeatureExtractor / TacticalSimulator / ONNX 추론 | 전술 탐색과 학습 확장 구조 |

---

## 2026-05-12 Server Authority AI Entry

S10 BotAIStage1은 1차 runtime smoke를 통과했다. 아래 문서를 최신 기준선으로 본다.

```text
.md/TODO/05-11/AI_STAGE1_SERVER_COMMAND_HANDOFF.md
.md/TODO/05-11/STAGE1_SERVER_AI_SMOKE_SUCCESS.md
```

이번 AI 진입의 범위는 고급 HFSM/BT/GOAP/Utility가 아니라 **server-side command generator**다.

```text
Bot reads CWorld
-> Bot emits GameCommand
-> CDefaultCommandExecutor validates/applies it
-> Server Snapshot/Event
-> Client animation/fx only
```

첫 구현은 기존 `Shared/GameSim/Systems/BotLaneAISystem.cpp`를 안정화한다.

- AI는 `Transform`, `Health`, `SkillState`, `MoveTarget` gameplay 결과를 직접 수정하지 않는다.
- AI가 직접 갱신해도 되는 값은 `BotLaneAIComponent`의 판단 상태/debug 값뿐이다.
- champion별 profile은 switch 기반으로 시작한다.
- smoke 기준선: Jax/Fiora lane fight, minion farm, tower aggro-safe behavior.
- 미니언 lane 회귀 방지: combat target scan은 range reject 후 priority/distance 비교.
- 다음 구현 1순위: Death / TargetInvalid / Respawn 공통 규칙.
- Stage1 통과 후 HFSM/BT/Utility 문서로 확장한다.

---

## Product Priority

| Product | AI 적용 우선순위 |
|---|---|
| `WintersLOL` | Server command bot -> lane/farm/fight HFSM -> champion BT/Utility -> Influence/GOAP -> MCTS/RL |
| `WintersElden` | Boss aggro/perception -> BossPhase HFSM -> Pattern BT/Utility -> Arena Influence Map -> boss GOAP |
| `ClassServant` | Servant follow/guard HFSM -> ability BT -> utility role selection -> party blackboard -> PvPvE GOAP |

---

## Principles

- Engine AI는 프레임워크만 제공한다.
- 챔피언, 보스, 클래스/서번트 로직은 게임별 모듈이 가진다.
- AI 판단은 `CWorld`를 읽고 의도를 만들며, 최종 결과는 `GameCommand`나 공용 command request로 적용한다.
- `Scene_InGame` 직접 호출, `extern` bridge, 제품별 include를 Engine AI에 넣지 않는다.
