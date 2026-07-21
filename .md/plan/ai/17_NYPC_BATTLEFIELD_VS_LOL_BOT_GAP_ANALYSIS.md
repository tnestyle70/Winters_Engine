Session - NYPC 전장(battlefield) 봇과 Winters LoL 봇의 구조 차이를 확정하고, 이식 가능한 자산과 새로 설계할 부분을 분리한다.

작성일: 2026-07-12
성격: 분석 문서 (코드 패치 계획 아님). Extreme Bot 문서 세트(18~23)의 공통 근거.
읽기 전제: `16_BOT_AI_HUMANLIKE_COMPLETION_PIPELINE_GUIDE.md` (실현 트랙 북극성), `14_NYPC_COMPETITION_ML_LAB_BRIDGE.md` (경계 헌장).

불변식: Bot AI는 GameCommand 생산자다. HP/쿨다운/데미지/이동 truth/사망·부활 상태를 직접 수정하지 않는다.

---

## 1. 두 게임의 Game DNA 비교

NYPC 2026 예선 문제 2 "전장(battlefield)"은 경제 RTS다. "NEXT NATION"은 게임명이 아니라 아레나 상대 핸들이다 (근거: `NYPC/battlefield/CLAUDE.md` — "NEXT NATION 본질 봇" = 그 급 상대를 기준으로 삼은 봇).

| 축 | NYPC 전장 | Winters LoL |
|---|---|---|
| 시간 구조 | 턴제 최대 200일, **동시 명령 제출** | 30Hz 고정 틱 실시간, 명령 1틱 지연 모델 (`GameRoomTick.cpp` Drain→BotAI→Execute) |
| 정보 | 완전정보. 단 상대 MOVE **목적지**는 미공개 → 역추론 필수 | 부분관측이 본질. 현행 봇은 **전지적** (FOW 게이트 없음 — 16 §2.7 갭) |
| 행동 공간 | 이산 3동사 (UPGRADE / MOVE / TRAIN) | 연속+이산 혼합 (Move 좌표, 스킬 방향/지점, BA/Flash/Recall/LevelSkill/BuyItem) |
| 의사결정 주체 | 유닛 다수, 명령권 중앙 1주체 | 챔피언별 독립 브레인 5기 + 미니언/터렛/정글 (별도 truth AI) |
| 시뮬 재현 | 로컬 결정론 심판 (`engine/rules.py`) | 서버 권위 GameSim 결정론 + SimLab 골든 (`Tools/SimLab/main.cpp`) |
| 승리 화폐 | HQ HP diff (중간 화폐 = 골드, 좌석 +13/턴) | 넥서스 (중간 화폐 = 골드 — `ChampionAIValuation` 골드 환산으로 실현 시작) |
| 시간 예산 | 100ms/턴 | decisionInterval 0.20s + 매 틱 긴급 인터럽트, p95 예산은 프로파일러로 |
| 지평 | 200일 고정, 마감 은행(마감 B) 존재 | 비고정, 스노볼/오브젝트 템포 |

핵심 대칭: 두 게임 모두 **단일 화폐 원리**가 성립한다. 전장의 J(`J = Σ[15·filled − 2·army − 10·moves − 120·deaths] + terminal(HQ)`, `docs/claude_EVCORE4_J_DECISION_SPEC_20260708.md`)는 LoL에서 `ChampionAIValuation`의 골드 환산 점수(킬 300 / 근접 21 / 원거리 14 / 포탑 250)로 이미 같은 방향이 시작돼 있다.

## 2. 아키텍처 대응표

전장 봇 4층 (검증 출처: `docs/claude_EVCORE2_perception_20260705.md`):

```text
GameState(사실) → OppTrack(필름/기억) → Perception(귀속) → Offer market
  (KEEP/LABOR/DEFEND/CONTEST/CLAIM/RAID/SIEGE) → pairEv 전역 그리디 청산
  → gold scale (TRAIN/펀딩/저축) → EMIT (fault-safe, dedup)
```

Winters 봇 4단 (검증 출처: 16 §2.2 + rg):

```text
BuildChampionAIContext (ChampionAISystem.cpp:1613)
  → UpdateChampionAIDecisionEvidence + ChampionAIValuation::BuildUtilityScores
  → brain intent (SampleLaneCombatIntent → IChampionAIBrain)
  → ExecuteLaneCombat (하드 안전 → 진행 중 커밋 → 신규 utility)
  → Emit*Command (게이트 후 outCommands push — 단일 실행 초크포인트)
```

| 전장 층 | Winters 대응 | 상태 |
|---|---|---|
| GameState (사실 누적) | ChampionAIContext (단일 틱 스냅샷) | 있음. 단 **히스토리 없음** |
| OppTrack (상대 유닛 필름) | **없음** | net-new → `19_OPPONENT_REACTION_MODEL` |
| Perception (위협 귀속, "연속량만, 임계값 0") | turretDanger 스칼라 1개 (`ComputeTurretDanger`, system.cpp:1505) | 공간 필드 없음 → `20_INFLUENCE_MAP_GAMESIM` |
| Offer market (자원-임무 청산) | 없음 (라인전 한정 intent 6종) | 5v5 매크로(16 Phase E) 때 필요. 이번 세트 범위 밖 |
| gold scale (단일 화폐) | ChampionAIValuation | 실현 시작 (2026-06-24). TradeWindow 미배선 |
| EMIT fault-safe | Emit* 게이트 (CanMove/CanAttack/CanCast 사전 검사) | 실현 완료 |
| FightSim (정확 전방 시뮬) | BattleState 미니 시뮬 (08/15 Phase 6) | 보류 유지 — 권위판 조건부 |

## 3. 이식 가능한 원리 (전장 근거 → LoL 착지점)

1. **"연속량만, 임계값 0"** (`claude_evcore2.cpp:1509` 배너). 위협은 전부 숫자로 존재시키고 분기는 마지막에. → ValueInput 확장 원칙. 새 perception 항목은 bool 게이트가 아니라 연속 점수로 넣는다.
2. **OppTrack 역추론**. 전장은 목적지가 숨겨져 `engineNext(prev,D)==관측` 생존 필터로 역추론했다. LoL은 좌표가 보이는 대신 **다른 것이 숨겨져 있다**: FOW 밖 위치(last-seen), 스킬 쿨다운(시전 관측 시각 + `GameplayDefinitionQuery::ResolveSkillCooldown`으로 추론), 의도(이동 방향 일관성 → 목표 귀속). 역추론의 대상만 바뀌고 방법(관측 누적 → 후보 생존 필터 → 확신도)은 그대로 이식된다.
3. **위협 귀속 5단계** (방향 tier / 역추론 귀속 / 미러-EV 귀속 / 중간선 필터 / 집계). → "이 적은 지금 누구를 노리는가"를 나/아군/구조물/오브젝트에 귀속. 반응 모델(19)의 L1.
4. **pHold/defEV 커밋 오라클** (`pHold = clamp(available + turret − effAtk + 2, 0, 4)`; defEV ≤ 0 ⇒ 포기). → 교전 커밋/백오프 판단. `TradeWindow` 배선의 수학적 원형.
5. **spareArmy / oppNonEarning 회계 항등식** ("파밍하지 않는 적 병력 = 어딘가로 걷는 타격대"). → **MIA 회계**: 라인에서 안 보이는 적 = 로밍 전력으로 계상. 부분관측에서 더 강력해지는 원리.
6. **반사실 핀 크레딧** (`핀크레딧 = J_opp(무위협 시나리오) − J_opp(실제 반응)`, 관측 후에만 기장). → 크로노 브레이크 반사실 실험(22)의 회계 규칙: 예측 가치는 관측으로 확정된 뒤에만 점수화.
7. **검증 생태계 천장** (`docs/claude_RETRO_why_2400_ceiling_20260709.md`: 2400은 봇이 아니라 검증 풀의 천장이었다; 패배 해부 6일 > 승자 해부 시작 7일차). → 리그 상대 풀에 반드시 "나보다 강한 축"을 유지 (과거 챔피언 풀 + 강한 스크립트 봇 + self-play). 봇이 아니라 **측정 생태계를 먼저 설계**한다.
8. **측정 규율** (NYPC `Gotchas.md` §1: offline≠outcome / first-advantage 교란 / holdout 필수 / wall-clock 노이즈 → node-budget DET). → SimLab 고정 seed + 틱 예산 결정론 측정, FIRST/SECOND에 해당하는 Blue/Red 교차 측정.
9. **승자 모델 = Stockfish, not AlphaZero** (`docs/claude_FINALS_blueprint_minii_class_bot_20260709.md`: 민이 = 수천 개 인간 패치 + Fishtest식 게이트 + Situation 1급 객체). → 학습 이전에 **패치 승급 게이트 인프라**(리그/회귀/시나리오)가 강함의 본체.

## 4. 이식 불가 / 왜곡 위험

- **정확 FightSim의 산술**: 전장은 이산·동시턴이라 HP×수×도착 스케줄을 정확 재현 가능. LoL은 연속 공간+스킬샷+CC라 정확 재현 불가 → 근사 BattleState(보류)로 격하하고, 대신 **크로노 브레이크 실측**(22)이 그 자리를 채운다 (시뮬 근사 대신 실제 GameSim을 되감아 실행).
- **마감 은행(마감 B)**: 200일 고정 지평 전용. LoL에 직역 금지.
- **탐색 깊이 우선주의**: mushroom의 "깊이 추가" 노선은 전장에서 명시 금지였다 (`NYPC/AGENTS.md` §4.0.1 — 전장은 국면 가치가 눈으로 읽히는 게임 → visual-first replay 루프). LoL도 같은 클래스다: **눈으로 보는 검증(F5 + 디버그 오버레이)이 1차 센서**, 탐색/학습은 그 다음.
- **완전정보 가정**: 전장 Perception은 전 유닛이 보인다는 전제 위에 있다. LoL 이식 시 모든 귀속·회계는 FOW 게이트(19 R1) **뒤에** 놓아야 한다. 전지적 상태로 이식하면 "인간형"과 정면 충돌 (16 §2.7).

## 5. 결론 — 담당 문서 매핑

| 이식/신설 대상 | 문서 |
|---|---|
| 마스터 통합 + 마일스톤 + 운영(천장 예산) | `18_EXTREME_BOT_MASTER_PLAN.md` |
| OppTrack/귀속/MIA/반응 예측/스킬샷 리드 | `19_OPPONENT_REACTION_MODEL.md` |
| 위협/영향 공간 필드 (turretDanger 일반화) | `20_INFLUENCE_MAP_GAMESIM.md` |
| score breakdown / why-not / 히트맵 / JSONL | `21_AI_DEBUG_TOOL_EXTENSION.md` |
| 반사실 실험 장치 + 골든 시나리오 공장 | `22_CHRONO_BREAK_BOT_TUNING_LOOP.md` |
| 리그/부검/튜너/오라클 .py 이식 | `23_NYPC_PY_TOOLCHAIN_PORT.md` |
| 공개 환전물 | 구현 근거·리그 리포트·재현 가능한 기술 글 |
