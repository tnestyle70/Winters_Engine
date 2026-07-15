Session - 상대 챔피언의 행동을 추적(OppTrack)·추론(귀속/쿨다운/MIA)·예측(반응 통계/리드)하는 결정론 상대 모델을 Shared/GameSim에 설계한다.

작성일: 2026-07-12 (검증 반영판)
성격: 축 설계 문서 (마스터 = 18). 슬라이스 R1~R4는 착수 시 `.md/계획서작성규칙.md` 형식으로 분해.
원형: NYPC 전장 OppTrack/Perception (17 §3-2, §3-3, §3-5 — `NYPC/battlefield/bots/cpp/claude_evcore2.cpp`의 `struct OppTrack`, `struct UnitHist`, `struct Perception`).

불변식: Bot AI는 GameCommand 생산자다. 상대 모델은 **읽기 전용 추론**이며 gameplay truth를 만들지도 수정하지도 않는다.

## Goal

봇이 "지금 보이는 것"만이 아니라 **"관측해 온 것"으로 판단**하게 만든다: 상대가 어디로 가는가, 무슨 스킬이 빠졌는가, 안 보이는 적은 어디 있는가, 내가 들어가면 상대가 어떻게 반응하는가.

## Non-goals

- 인간 플레이어 로그 기반 모델링 (봇/self-play 데이터가 1차; 인간 로그는 M5 이후).
- 팀 단위 공유 기억 (Team Blackboard — 16 Phase E). 이 문서는 봇 개체의 상대 모델. (단, 시야 **사실**의 팀 공유는 LoL 규칙 자체다 — 20의 필드 소유권 절 참조.)
- 실시간 탐색(MCTS/BattleState) — 보류 유지.

## 현재 코드 근거 (rg 검증, 2026-07-12)

- 상대 인식은 **단일 틱 스냅샷**: `BuildChampionAIContext` (`Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp:1613` 근처)가 매 결정마다 `FindEnemyChampion` 등 선형 스캔으로 즉석 재구축. 히스토리 버퍼 없음.
- `Shared/GameSim/Systems/ChampionAI/ChampionAIPerception.h`의 `struct ChampionAIPerception`에는 visibility/last-seen/반응 지연 필드가 없다.
- **FOW 게이트 없음**: 타겟 획득은 `GameplayStateQuery::CanBeTargetedBy`만 통과하면 시야 무관 전수 스캔 (16 §2.7 "전지적 perception").
- brain 선택은 이미 배선됨: `GameRoomSpawn.cpp:800` 근처 `difficulty >= 2 ? PlayerLike : RuleBased` (18 supersede 절). 난이도→노브 프리셋은 M0 소유.
- `CDecisionChampionBrain` (`Shared/GameSim/Systems/ChampionAI/ChampionAIBrain.cpp` `TODO(bot-v2)`)은 RuleBased 위임 stub — 외부 판단 모듈의 예약석.
- `ILagCompensationQuery` (`ICommandExecutor.h`)는 판정용이며 봇 두뇌는 소비하지 않는다.
- 스킬 쿨다운 정의 질의처: `GameplayDefinitionQuery::ResolveSkillCooldown/ResolveSkillRange` (16 §2.6).

## 설계 — 3층

### L0 관측 기억 (OppTrack 이식)

`ChampionAIOpponentTrack` — 적 챔피언별 고정 크기 링 버퍼:

```text
{ tick, posXZ, hpRatio, observedCast(slot, stage), moveDirXZ, bVisible }
```

- **FOW 게이트가 입구다**: 보이는 틱만 기록. 안 보이면 last-seen(위치/틱)과 미아 경과 시간만 유지.
- 저장 위치: `ChampionAIComponent` 내 고정 배열(적 5기 × 링 N) 또는 별도 컴포넌트. is_trivially_copyable 유지 → **키프레임 레지스트리 등록 대상** (크로노 되감기 후 기억도 함께 복원 — 22와 정합).
- **링 N 예산 (R1 계획서에서 정량)**: 트랙은 키프레임 blob(현 ~81KB)×링 90에 곱해지고, N 변경은 22 U3 골든 시나리오 blob을 전부 무효화한다(버전 정책 i). N은 바이트/적 5기/봇 수로 예산 계산 후 여유를 두고 한 번에 확정.
- 결정론: 고정 크기, EntityID 정렬 순회, `tickIndex` 기반.
- CONFIRM_NEEDED: 서버 권위 시야 판정의 현재 소스 — `VisionSensorComponent`가 챔피언 상호 가시성을 이미 제공하는지, 부쉬/FOW 규칙이 서버에 있는지 코드 검사 후 R1 계획서에서 확정. 없으면 Server 평탄화 주입(16 §1.4)으로 시작.

### L1 상태 추론

1. **쿨다운 추론**: `observedCast` 관측 시각 + `ResolveSkillCooldown` ⇒ "상대 Q는 앞으로 N틱 없음". 전장의 destCand 생존 필터와 동형 — 관측이 후보(사용 가능 스킬 집합)를 좁힌다. 산출: `enemySkillReadyMask`(추정) + 확신도.
2. **위협 귀속** (전장 5단계의 축소판): 이동 방향 일관성(연속 틱 방향 내적) + 거리 감소 대상 ⇒ 이 적이 노리는 자산(나/아군/포탑/웨이브) 귀속. 산출: 자산별 `{attackers, minEta}` — "연속량만, 임계값 0" 원칙.
3. **MIA 회계** (spareArmy/oppNonEarning 항등식 이식): 라인에서 관측되지 않는 적 = 로밍 전력으로 계상. last-seen 위치+경과 시간으로 도달 가능 반경 추정 → 위험 계수. 산출: `miaThreat` 연속량 → ValueInput의 retreat 가중.

### L2 반응 예측과 활용

1. **반응 통계 테이블**: `(내 행동 클래스 × 상대 상태 버킷) → 상대 반응 분포` (반응 클래스: 후퇴/맞교환/스킬 응수/무시). 데이터 공장 = 크로노 반사실 실험(22) + BotLab 리그(23). 통계는 오프라인 산출 → **constexpr/데이터 테이블로 소성(baked)** 주입 (16 §3.6 — 런타임 학습 금지).
2. **교전 커밋 오라클**: 전장 pHold/defEV 이식. `TradeWindow`(`ChampionAIValuation.h` — API 존재, 미배선)를 반응 예측으로 완성: 내 콤보 기대딜 vs 예측 반응(응수 스킬 가능 여부 = L1 쿨다운 추론) ⇒ 커밋/포기. 회계 규칙 = 핀 크레딧 원리: 예측 가치는 관측으로 확정된 후에만 메트릭에 기장 (21 inspector가 적중률 추적).
3. **스킬샷 리드**: `EmitSkillCommand` 조준을 현재 위치가 아니라 `관측 속도 외삽 + 예측 반응(도주 방향)`으로. 난이도 연동은 **리드 정확도/반응 지연 축의 노브 배선**만 이 슬라이스 소유 — 난이도→프리셋 체계 자체는 M0 소유, brain 선택 배선은 기존 코드 (이중 소유 금지).

### Brain 착지

`ChampionAIBrainInput` 확장(현재 8필드 — f32 스칼라 5 + bool 게이트 3, `ChampionAIBrain.h:23`)에 L1/L2 산출(귀속 위협, miaThreat, enemySkillReadyMask 요약, 커밋 오라클 점수)을 추가. 1차 소비자는 RuleBased/PlayerLike 임계 규칙, 최종적으로 `CDecisionChampionBrain` 예약석에 플래너/베이크드 정책이 앉는다.

## 구현 슬라이스

| 슬라이스 | 내용 | 검증 |
|---|---|---|
| R1 | FOW 게이트 + last-seen/미아 시간 (L0) + 링 N 예산 확정 | SimLab "시야 밖 적 무반응" 프로브 + 부쉬 진입 시 타겟 드랍 인게임 확인 |
| R2 | 쿨다운 추론 (L1-1) | 프로브: 상대 Q 시전 관측 후 N틱간 readyMask=false; F9에 추정 표시(21) |
| R3 | 귀속 + MIA 회계 (L1-2/3) | 프로브: 접근하는 적의 귀속 대상 안정성; MIA 시 retreat 점수 상승 |
| R4 | 반응 통계 + 커밋 오라클 + 리드 (L2) | 예측 적중률 메트릭 + 리그 승률 우위 (18 M4 게이트) |

## Files touched (예정)

`Shared/GameSim/Systems/ChampionAI/{ChampionAIPerception.h, ChampionAISystem.cpp, ChampionAIBrain.h/.cpp, ChampionAIValuation.h/.cpp}`, `Shared/GameSim/Components/ChampionAIComponent.h`(트랙), `Shared/GameSim/Core/Checkpoint/WorldKeyframe.cpp`(레지스트리 등록), `Server/Private/Game/GameRoomChampionAI.cpp`(시야 평탄화 — R1 CONFIRM 결과에 따라), Snapshot.fbs+Builder/Applier(디버그 노출), `Tools/SimLab/main.cpp`(프로브).

## Rollback scope

각 슬라이스는 독립 컴포넌트/필드 추가 — 제거 시 소비처(ValueInput 확장 항, BrainInput 항)만 원복. 키프레임 레지스트리 등록은 컴포넌트와 함께 제거 (완전성 기계검사가 누락을 즉시 FAIL로 알림).

## Next slice

R1 계획서: 시야 권위 소스 코드 검사(CONFIRM_NEEDED 해소) → FOW 게이트 + last-seen 필드 + 링 N 예산 + 프로브.
