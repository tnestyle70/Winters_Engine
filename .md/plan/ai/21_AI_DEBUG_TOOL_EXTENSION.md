Session - F9 AI 디버그 패널을 score breakdown / why-not / 히트맵·고스트 / JSONL 스트리밍 / 반응 inspector로 확장해 Extreme Bot의 관측 장비를 완성한다.

작성일: 2026-07-12 (검증 반영판)
성격: 축 설계 문서 (마스터 = 18). `13_DEBUG_EDITOR.md`의 실현판 확장 — 그린필드가 아니라 기존 F9 인프라 위 증축.
원칙: "튜닝 전에 inspectable debug UI/오버레이/바운디드 트레이스 먼저" (CLAUDE.md 규약). NYPC 원칙 승계: "시각화는 예쁘기 위한 것이 아니다 — 30초 안에 왜 틀린 수를 골랐는지 찾기 위한 검사 장비" (`NYPC/mushroom/docs/plan/02_INDEPENDENT_COMPETITION_ML_LAB.md` §6).

불변식: Bot AI는 GameCommand 생산자다. 디버그 뷰는 서버 권위 스냅샷 경유로만 채워지고, 클라 직접 mutate 금지 — 개입은 `AIDebugControl` 왕복만.

## 현재 자산 (rg 검증 — 그대로 사용)

| 자산 | 위치 |
|---|---|
| F9 AI 패널 (봇 테이블/상세/14노브 슬라이더/trace 테이블/강제행동) | `Client/Private/UI/AIDebugPanel.cpp` |
| 월드 오버레이 (scan/dive/flash range 원 + 상태 텍스트) | `Client/Private/UI/DebugDrawSystem.cpp` `DrawChampionAIDebug` |
| 결정 트레이스 링 16 + 거부사유 12종(+None 센티널 = enum 13값) | `ChampionAIComponent.debugDecisionTrace`, `eChampionAIDecisionBlockReason` |
| 튜닝 왕복 | `AIDebugControl` → `CommandExecutor::HandleAIDebugControl` (`_DEBUG` 한정) |
| 서버→클라 경로 | `SnapshotBuilder.cpp` → `SnapshotApplier` → `ChampionAIDebugComponent` |
| 명령 로그 (512건 cap) | `LogChampionAICommand` (ChampionAISystem.cpp) |

## 현재 갭 (rg 검증)

1. **Valuation 세부항 폐기**: `ChampionFightValue`/`EconomyLead`/`TradeWindow` 항이 계산 후 버려지고 최종 `fight/farm/siege/retreat` 4값만 남는다 — "왜 그 점수인가"를 볼 수 없다.
2. **Why-not 부재**: 선택 안 된 intent의 점수/차단 사유를 나란히 볼 수 없다.
3. **Cap**: trace 링 16 / 로그 512 — 장시간 평가에서 뒤쪽 누락 (16 §5.3).
4. **공간/기억 시각화 부재**: 히트맵(20), last-seen 고스트/쿨다운 추정(19) 그릴 소스와 뷰 모두 없음.
5. **그래프 부재**: 점수 시계열이 표 행으로만 보임.

## 확장 설계 (5건)

### D1 — Score breakdown 패널
`BuildUtilityScores` 입력(ValueInput)과 중간항(FightValue 세부/EconomyLead/TradeWindow)을 디버그 컴포넌트로 노출. 새 노브/필드 추가는 16 §4.3의 6단계 표준 절차(Component→Executor 매핑→Snapshot.fbs→Builder→Applier→패널) 그대로 따른다. 스냅샷 크기 영향 측정 (디버그 필드의 `_DEBUG`/practice 게이트 방식은 현행 규약 확인 후 동일하게 — CONFIRM_NEEDED).

### D2 — Why-not / counterfactual 뷰
trace 엔트리에 "탈락 intent별 점수+차단 사유" 슬롯 추가. NYPC `claude_turn_inspect.py`(턴 부검)의 화면 구성 이식: 한 틱을 고르면 그 틱의 perception 입력 → 후보 점수 → 선택 → 거부 사유가 한 화면.

### D3 — Trace JSONL 스트리밍 ★ M0 항목
`PushChampionAIDecisionTrace`/`LogChampionAICommand`가 이미 모든 메트릭 입력을 모은다 (16 §5.3) — 구조화 JSONL로 flush해 링 cap을 우회하고(컴포넌트 내 링 16은 POD 유지 — 제거 아님), 명령 로그 512 cap은 제거한다.

- 스키마: NYPC Turn-JSONL 표준 승계(`02_INDEPENDENT_COMPETITION_ML_LAB.md` §5): `tick, botNetId, scores{fight,farm,siege,retreat}, chosen_intent, chosen_action, block_reason, decision_ms` + 확장(breakdown/예측).
- **직렬화 단일 소유**: JSONL 레코드 직렬화(스키마+라인 포맷)는 Shared/GameSim의 trace-to-string 헬퍼 **한 곳**에 둔다. Server(GameRoom)와 SimLab은 각자 파일 열기/flush(sink)만 소유 — 방출기가 둘(서버 실행/SimLab 배치)이어도 스키마 드리프트가 없어야 M1 리그·M5 학습의 데이터 계약이 성립한다 (23과 공유).
- 출력 위치: 호스트 프로세스 로컬 파일 (경로 CONFIRM_NEEDED — 기존 리플레이/로그 출력 폴더 규약 확인).

### D4 — 공간/기억 오버레이
- 히트맵: 20의 필드를 다운샘플해 스냅샷 디버그로 → `DebugDrawSystem`에서 셀 색칠 (07의 시각화 아이디어를 클라측에서 실현 — GPU 사용은 여기서만 허용).
- 고스트: 19 last-seen 위치에 반투명 마커 + 미아 경과 시간. 쿨다운 추정: 선택 봇의 상대 스킬 추정 잔여를 패널 표시.

### D5 — Reaction inspector (19 L2 검증 장비)
예측 반응 vs 실제 반응을 이벤트로 쌓아 적중률 누적 표시. 핀 크레딧 회계(관측 확정 후 기장) 원칙 — 예측 시점이 아니라 관측 시점에 채점.

## 구현 순서와 검증

| 슬라이스 | 대응 M | 검증 |
|---|---|---|
| D3 (JSONL) | M0 | 파일 생성/스키마 유효/성능 카운터(스트리밍이 틱 예산 침범 없음), Server·SimLab 두 sink의 출력 라인 포맷 동일, SimLab PASS 유지 |
| D1 (breakdown) | M2 | F9에서 세부항 표시, 슬라이더 변경이 세부항에 반영 |
| D4 (히트맵/고스트) | M2 | F5 눈 확인 + 스냅샷 크기 측정 |
| D2 (why-not) | M2~M3 | 부검 1건 30초 룰 |
| D5 (inspector) | M4 | 적중률 메트릭이 리그 리포트와 일치 |

## Files touched (예정)

`Shared/GameSim/Components/ChampionAIComponent.h`(디버그 필드/trace 확장), `Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp`(trace 생산 + 직렬화 헬퍼), `Server/Private/Game/`(파일 sink 배선), `Shared/Schemas/Snapshot.fbs`+`SnapshotBuilder.cpp`+`SnapshotApplier.cpp`, `Client/Private/UI/{AIDebugPanel.cpp, DebugDrawSystem.cpp}`, `Tools/SimLab/main.cpp`(SimLab sink).

## Rollback scope

뷰별 독립 — 각 D는 스냅샷 필드+패널 코드 단위로 원복 가능. JSONL은 sink 배선 2곳 + 헬퍼 제거.

## Next slice

D3 계획서 (M0 소속): 기존 로그/리플레이 출력 폴더 규약 확인 → JSONL 스키마 확정 → Shared 직렬화 헬퍼 + Server/SimLab sink 배선.
