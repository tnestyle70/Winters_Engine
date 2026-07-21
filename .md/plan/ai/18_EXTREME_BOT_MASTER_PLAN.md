Session - 상대 행동·반응 기반 Extreme Bot 설계를 실현 트랙 위에 5축으로 통합하고, 마일스톤·검증·환전물·천장 예산을 고정한다.

작성일: 2026-07-12 (검증 반영판 — brainType 배선은 이미 코드 존재 확인, M0 재스코프)
성격: 마스터 로드맵 (구현 슬라이스는 각 축 문서 + 착수 시 `.md/계획서작성규칙.md` 형식 계획서로 분해).
참조 의무: `16_BOT_AI_HUMANLIKE_COMPLETION_PIPELINE_GUIDE.md` §1 북극성, `.md/TODO/05-09/ServerAICompletion.md`, `17_NYPC_BATTLEFIELD_VS_LOL_BOT_GAP_ANALYSIS.md`.

## Current sequence

```text
M0_계측과_난이도_프리셋 (16 Phase A 잔여 + B 승계)
-> M1_BotLab_툴체인_v1 (23)
-> M2_Perception_2.0 (19 R1-R2 + 20 v1 + 21 히트맵)
-> M3_크로노_반사실_루프 (22)
-> M4_반응_모델_v1 (19 R3-R4)
-> M5_학습형_연장 (PyTorch BC — 채용 서사 직결)
```

## Goal

"기능 구현"이 아니라 **답이 없는 문제 — 상대의 행동과 그에 대한 대응을 어떻게 수치화·예측·활용하는가 — 를 측정 가능한 시스템으로 닫는 것.** NYPC에서 검증한 방법론(단일 화폐, 위협 귀속, 역추론, 반사실 회계, 측정 규율, 승자 해부)을 Winters 서버 권위 LoL 봇에 이식한다. 최종 산출 = 강해지는 봇 + **강해지는 과정을 증명하는 실험 시스템** (15 §8 완료 정의 승계).

## Non-goals

- 5v5 전면 RL (14/15/16이 이미 보류 — M5도 반응 예측 BC까지만).
- Engine/Public/AI 부활 (MCTSPlanner/RLBridge/BehaviorTree stub은 비권위 — 16 §1.8 금지 유지).
- 팀 Offer market / Team Blackboard (16 Phase E 영역 — 이 세트는 1v1 라인전 반경에서 반응 모델을 완성한 뒤 확장).
- 챔피언 로스터 확장, FX/렌더 품질.

## 16 로드맵과의 관계 (supersede 선언)

- 16 Phase A의 brainType 배선은 **이미 코드에 반영됨** (`Server/Private/Game/GameRoomSpawn.cpp:800` 근처 — `difficulty >= 2 ? PlayerLike : RuleBased`, 로비 기본 난이도 2 → PlayerLike가 기본 도달; 16 §2.7의 "미배선"은 stale). 잔여(난이도→노브 프리셋)만 M0이 승계.
- 16 Phase B → M0(JSONL)/M1(하네스). Phase C 중 FOW perception → 19 R1(M2), 스킬샷 리드 → 19 R4(M4). Phase F(학습형) → M5.
- Phase C 잔여(막타 정밀화/조준 오차)와 Phase D(웨이브 관리/트레이드 intent)는 이 세트 범위 밖 보류 — 반응 모델 완성 후 재개.

## 북극성 불변식 (모든 슬라이스 공통)

1. **Bot AI는 GameCommand 생산자다. HP/쿨다운/데미지/이동 truth/사망·부활 상태를 직접 수정하지 않는다.** 생산 = `MakeAICommand` → `outCommands`, 적용 = `CDefaultCommandExecutor::ExecuteCommand` 단일 초크포인트.
2. 의사결정 본체는 `Shared/GameSim` (Engine/Client/ImGui/DX include 금지, 맵 정보는 `IWalkableQuery`류 추상화 또는 Server 평탄화 주입).
3. 결정론: `tc.pRng`/`tickIndex`만, EntityID 안정 타이브레이크, 고정 순회. `hard safety -> active commitment -> new utility` 순서 준수 (gotchas 2026-07-12).
4. 런타임 튜닝은 `eCommandKind::AIDebugControl` 왕복만.
5. 튜닝 전에 관측 장비 먼저 (CLAUDE.md 디버깅 파이프라인 규약).

## Why this order

- M0이 없으면 아무것도 측정 안 된다: trace cap(링 16/로그 512)이 장시간 평가를 막고, 난이도가 brain 선택 외 의사결정에 미참조(`ChampionAIComponent.h:195` 필드 저장뿐)라 난이도 축 실험이 불가능하다.
- M1(리그/부검 툴)이 M2~M4의 **판정자**다. NYPC 교훈: 봇보다 측정 생태계 먼저 (17 §3-7).
- M2(FOW+필드)가 M4(반응 모델)의 전제다: 전지적 봇에게 "상대 반응 예측"은 무의미하다 — 무엇이 보이고 안 보이는지부터.
- M3(크로노 루프)는 M4의 **데이터 공장**이다: 같은 상황 되감기 → 내 행동만 변경 → 상대 봇 반응 차이 관측 = 반사실 반응 데이터의 결정론 생성기.
- M5는 M1~M4의 산출(JSONL trace + 반응 데이터셋) 위에서만 성립한다.

## 마일스톤

각 M은 검증 통과 후 다음으로. 착수 시 `.md/계획서작성규칙.md` 형식 계획서로 분해한다.

### M0 — 계측과 난이도 프리셋
- 산출물: (a) trace JSONL 스트리밍(링 cap 우회) + 명령 로그 512 cap 제거 (21 D3 — 직렬화는 Shared 헬퍼 단일 소유), (b) 난이도→튜닝 노브 프리셋 매핑 (brain 선택은 기존 코드 유지), (c) SimLab 시나리오 프로브 템플릿 1종.
- 검증: JSONL 파일 생성·스키마 유효, 난이도별 노브 차등이 trace로 확인, SimLab 기존 프로브 + same-seed/seed+1 계약 전부 PASS 유지.
- 환전물 게이트: **지원작전 §4의 1차 지원 제출 완료** (기술 환전물 없는 유일한 M — 제출이 게이트를 대신한다).

### M1 — BotLab 툴체인 v1 (23)
- 산출물: `Tools/BotLab/` 리그 러너(league.py 이식) + tick 부검(turn_inspect 이식) + 결과 CSV/리포트. 선행 계약: 헤드리스 승패/종료 지표 + 사이드별 봇 구성 CLI + frozen baseline 정책 (23 P1 스펙 — SimLab측 확장 슬라이스 포함).
- 검증: 같은 seed 두 번 = 같은 CSV (결정론 리그), 부검 1건 30초 룰 (15 Phase 3 기준).
- 환전물: 리그 리포트 1편 (RuleBased vs PlayerLike baseline 표).

### M2 — Perception 2.0
- 산출물: FOW 게이트 + last-seen (19 R1), 쿨다운 추론 (19 R2), ThreatField v1 (20), 히트맵/고스트 오버레이 (21).
- 검증: "시야 밖 적에 무반응" SimLab 프로브, ThreatField 결정론 해시 프로브, 프레임 예산 카운터.
- 환전물: **히트맵+고스트 데모 영상** (봇이 무엇을 보고 무엇을 기억하는지 시각 증명).

### M3 — 크로노 반사실 루프 (22)
- 산출물: 되감기→개입→재결정 관찰 루프 정식화, 골든 시나리오 export/로더 (키프레임 blob → SimLab), 반사실 실험 재현성 프로브.
- 검증: S015 §3 인게임 게이트 통과(선행 잔여), 같은 키프레임+같은 개입=같은 해시.
- 환전물: **크로노 튜닝 루프 영상** — "되감고, 상대 수치를 바꾸고, 같은 상황을 다시 산다" (UE/Unity 부재 능력, S015 RESULT §2 표 인용).

### M4 — 반응 모델 v1 (19 R3-R4)
- 산출물: 위협 귀속/MIA 회계, 반응 통계 테이블(M3 공장 + 리그 데이터), 교전 커밋 오라클(TradeWindow 배선), 스킬샷 리드.
- 검증: 반응 예측 적중률 메트릭(21 inspector), 리그에서 frozen baseline 대비 승률 우위 (Blue/Red 교차, holdout seed).
- 환전물: **기술 글 1편** — "상대의 반응을 수치화하는 봇" (NYPC 귀속→LoL 이식 서사).

### M5 — 학습형 연장 (선택·연구, 채용 서사 직결)
- 산출물: 반응 예측 모델 BC (PyTorch, 입력=JSONL trace/반사실 데이터셋, 출력=상대 반응 분포) + 평가 리포트(top-1/적중률/리그 효과). 런타임 주입은 baked artifact/순수 데이터만 (16 §3.6).
- 검증: illegal 예측 0, holdout에서 통계 테이블 대비 개선, 결정론·리플레이 유지.
- 환전물: **학습 리포트 + 공고 요건 직격 증거** ("게임 플레이 로그 분석·행동 모델링" — 지원작전 문서 §3).

## 운영 규칙 (GOAL_OPERATING_DOCTRINE 적용)

- **천장 예산 30%**: 주간 시간의 30%는 제출·공개 작업(지원서, 영상, 글, 리그 리포트 공개)에 고정. 각 M의 환전물은 선택이 아니라 게이트다 — 환전물 없이 다음 M 진입 금지 (이해→환전).
- **순서 역전**: "봇 완성 후 공개"가 아니라 M0부터 구현 근거·리그 리포트·재현 절차를 공개 환전물로 병행한다. 외부 지원 일정은 저장소 밖 지원 자산에서 관리한다.
- **주간 역산**: 매주 "목표 지표(제출 수/공개물 수/리그 우위)를 직접 올린 날"을 센다.
- 이 교리를 이유로 검증 기준(SimLab 게이트, 결정론 계약, 인게임 게이트)을 낮추지 않는다.

## 검증 피라미드 (16 §5.1 채택 + NYPC 규율)

```text
4) 메트릭 리그        frozen baseline 대비 우위 (BotLab, Blue/Red 교차, holdout seed)
3) 인게임 스모크 (F5) 히트맵/고스트/inspector로 "왜 그 행동" 눈 확인 + 크로노 재실행
2) 시나리오 하네스    SimLab + 골든 키프레임 시나리오 → trace/metric assert
1) 결정론 회귀        same-seed 해시 parity + keyframe restore 프로브 (상시 게이트)
```

메트릭 어휘: 강함(승률/CS@10/오브젝트) + 인간다움(반응시간 분포/decision churn/blunder rate) + **반응 모델(예측 적중률/커밋 후회 regret)** + 안정성(desync 0, parity 100%).

## Files touched (세트 전체 조감 — 상세는 각 축 문서)

- `Shared/GameSim/Systems/ChampionAI/*` (perception/track/valuation 확장), `Shared/GameSim/Components/ChampionAIComponent.h` (트랙/필드/디버그), `Server/Private/Game/GameRoom*.cpp` (평탄화 주입/시나리오 export), `Shared/Schemas/Snapshot.fbs`+Builder/Applier (디버그 필드), `Client/Private/UI/{AIDebugPanel,DebugDrawSystem,ChampionTuner}.cpp` (뷰), `Tools/SimLab/main.cpp` (프로브/로더/리그 계약), `Tools/BotLab/` (신설 py).

## Next slice

M0 착수 계획서: trace JSONL 스트리밍 + 난이도→노브 프리셋. 착수 전 rg 재검증 필수 — 특히 `GameRoomSpawn.cpp`의 brainType 배선은 **이미 존재**하므로 no-op 계획을 쓰지 않는다 (gotchas 2026-05-18); 16의 line 번호도 드리프트 있음, 함수명 앵커 사용.
