# Phase F — 자체 AI 봇 구현 계획 (인덱스)

## 비전

실제 LoL Intermediate 봇 수준을 **넘어서는** 고도화된 MOBA AI. 최종 목표 프리셋 "Master" 는
인간 골드~플래티넘 체감. 외부 AI 라이브러리 의존 없이 C++20 로 직접 구현하며, 최신 MOBA AI
연구 흐름 (OpenAI Five, AlphaStar) 의 축소판을 게임 엔진에 통합한다.

## 왜 자체 구현인가

- **봇전이 메인 콘텐츠** — Phase 4 네트워크 성숙 전에도 봇전만으로 게임이 성립
- 선생님 수업엔 AI/FSM/BT 기본만 있음. LoL 모작 수준까지 확장하려면 직접 설계 필요
- 포트폴리오 관점에서 Game AI Pro / Behavioral Mathematics / OpenAI Five 논문 지식 실전 적용
- 연습모드와 통합 — 에디터 UI 로 봇 상태 실시간 조작 = Winters 의 "연습모드=에디터" 철학 구현

## 3-Layer 의사결정 계층

```
Strategic (매크로, 2~5초)    팀 단위 목표 선택 (드래곤/갱/바론/백도어)
      ↓
Tactical (미들, 0.2~0.5초)   개별 봇의 경로·포지션·교전 판단
      ↓
Operational (마이크로, 매 프레임)   스킬샷 예측, 평타 무빙, 궁 캔슬링
```

## Stage 로드맵 (0~8)

| Stage | 내용 | 난이도 | 문서 |
|---|---|---|---|
| 0 | 단순 Aggro (정글몹/미니언) | Intro+ | [02_STAGE0_AGGRO.md](02_STAGE0_AGGRO.md) |
| 1 | HFSM (Hierarchical FSM) | Beginner+ | [03_STAGE1_HFSM.md](03_STAGE1_HFSM.md) |
| 2 | Behavior Tree (BT) | Beginner+ | [04_STAGE2_BT.md](04_STAGE2_BT.md) |
| 3 | GOAP (A* Action Planning) | Intermediate+ | [05_STAGE3_GOAP.md](05_STAGE3_GOAP.md) |
| 4 | Utility AI (점수 기반) | Intermediate+ | [06_STAGE4_UTILITY.md](06_STAGE4_UTILITY.md) |
| 5 | Influence / Threat / Opportunity / Vision Map | Master | [07_STAGE5_INFLUENCE_MAP.md](07_STAGE5_INFLUENCE_MAP.md) |
| 6 | MCTS (교전 시뮬) | Master | [08_STAGE6_MCTS.md](08_STAGE6_MCTS.md) |
| 7 | 모방 학습 (Behavior Cloning, ONNX) | Master | [09_STAGE7_IMITATION.md](09_STAGE7_IMITATION.md) |
| 8 | 강화학습 (PPO Self-Play) — 선택 | Grandmaster | [10_STAGE8_RL.md](10_STAGE8_RL.md) |

## 공통 시스템

| 주제 | 문서 |
|---|---|
| 아키텍처 + ECS 통합 + 디렉토리 | [01_ARCHITECTURE.md](01_ARCHITECTURE.md) |
| 팀 협동 Blackboard | [11_TEAM_BLACKBOARD.md](11_TEAM_BLACKBOARD.md) |
| 난이도 프리셋 (Intro~Grandmaster) | [12_DIFFICULTY.md](12_DIFFICULTY.md) |
| 디버깅 / ImGui 에디터 통합 / Replay | [13_DEBUG_EDITOR.md](13_DEBUG_EDITOR.md) |
| NYPC Competition ML Lab → Winters LoL AI 통합 경계 | [14_NYPC_COMPETITION_ML_LAB_BRIDGE.md](14_NYPC_COMPETITION_ML_LAB_BRIDGE.md) |
| NYPC → MobaZero 연구 플랫폼/포트폴리오 로드맵 | [15_MOBAZERO_RESEARCH_PLATFORM_ROADMAP.md](15_MOBAZERO_RESEARCH_PLATFORM_ROADMAP.md) |

## Codex 구현 계획서

| 주제 | 문서 |
|---|---|
| Aggro / HFSM / BT / GOAP / Utility / Influence Map / Blackboard / Imitation bridge 스캐폴딩 | [codex/AI_CORE_HFSM_BT_GOAP_UTILITY_INFLUENCE_SCAFFOLD_PLAN.md](codex/AI_CORE_HFSM_BT_GOAP_UTILITY_INFLUENCE_SCAFFOLD_PLAN.md) |
| MCTS / RL / TacticalSimulator / FeatureExtractor / ONNX 추론 | [codex/MCTS_RL_AI_IMPLEMENTATION_PLAN.md](codex/MCTS_RL_AI_IMPLEMENTATION_PLAN.md) |
| Codex AI 구현 계획 인덱스 | [codex/00_CODEX_AI_IMPLEMENTATION_INDEX.md](codex/00_CODEX_AI_IMPLEMENTATION_INDEX.md) |

## 구현 순서 (권장)

```
[Phase C 완료]           ← Collider, Navigation, Socket 필요
     ↓
Stage 0: Aggro          ← 정글몹 동작 가능 (봇전 베이스)
     ↓
Stage 1: HFSM            ← Laning/Ganking/Recall 루트 상태
     ↓
Stage 2: BT              ← 교전 의사결정 트리
     ↓
Stage 4: Utility         ← "지금 뭐 할까" 점수 판정
     ↓
Stage 5: Influence Map   ← 맵 인식 (갱킹/포지셔닝)
     ↓
Stage 3: GOAP            ← 장기 계획 (빌드/오브젝트)
     ↓
Stage 6: MCTS            ← 교전 미래 예측
     ↓
Stage 7: Imitation       ← 인간다움 학습
     ↓
Stage 8: RL (선택)       ← Self-Play 고도화
```

**주의 — Stage 3 을 Stage 4~5 뒤로 미룬 이유**: GOAP 의 행동 비용 함수는 Utility 점수와
Influence Map 수치에 의존한다. 먼저 맵 인식 + Utility 가 있어야 GOAP 의 계획이 의미 있음.

## 참고 문헌

- **Game AI Pro 1/2/3** — Steve Rabin (FSM/BT/Utility/Influence Map 실무 아티클)
- **Programming Game AI by Example** — Mat Buckland (FSM/Flocking/Path Planning 입문)
- **Behavioral Mathematics for Game AI** — Dave Mark (Utility AI 이론)
- **Game AI Pro 3 — "Goal Oriented Action Planning: Ten Years Old and No Fear!"** — Jeff Orkin
- **OpenAI Five** — Dota 2 PPO (2018)
- **AlphaStar** — StarCraft II (Vinyals et al. 2019)
- **DeepMind FTW** — Quake III CTF (Jaderberg et al. 2019)

## 의존성

| 필요 | 상태 | Phase |
|---|---|---|
| Navigation / Cell | ⏭️ | Phase C-4 |
| Pathfinding (A* + JPS) | ⏭️ | Phase C-4 (Navigation 내) |
| Collider + Bounding | ⏭️ | Phase C-4 |
| Socket + Hitbox/Hurtbox | ⏭️ | Phase C-3 |
| AnimationEvent | ⏭️ | Phase C-3 |
| DebugDraw | ⏭️ | Phase C-2 |
| ImGui 에디터 통합 | 🔄 | Phase B-5 (현재) |
| ECS 기반 | ✅ | Phase 1a 완료 |
| Networking (서버 권위) | ⏭️ | Phase 4 |

**Phase F 시작 조건**: Phase C-4 완료 (Navigation + Collider 필요). Phase 4 네트워크 없어도
오프라인 봇전은 가능.

## 예상 소요 (추정, 실제 진행에 따라 조정)

- Stage 0~2: 기본 봇 1주일 (놀기용)
- Stage 3~5: Intermediate 2~3주 (라인전 가능)
- Stage 6: Master 1~2주 (교전 예측)
- Stage 7: 로그 수집 + 학습 인프라 1개월
- Stage 8: RL 환경 + 분산 Self-Play 수개월 (연구 수준)
