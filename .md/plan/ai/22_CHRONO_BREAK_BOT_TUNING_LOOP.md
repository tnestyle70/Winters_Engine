Session - 크로노 브레이크(비트 정확 되감기)를 봇 개발의 반사실 실험 장치와 골든 시나리오 공장으로 승격한다.

작성일: 2026-07-12
성격: 축 설계 문서 (마스터 = 18). S015 반영분(전 빌드 PASS, 인게임 게이트 대기) 위에 봇 개발 용법을 정의한다.
참조: `Plan/S015_CHRONO_BREAK_KEYFRAME_REWIND_RESULT_20260712.md` (§2 UE/Unity 비교, §3 인게임 게이트, §5 P2 경계), `Plan/S014_DESIGNER_SIM_CONSOLE_AND_CHRONO_BREAK_FOUNDATION_SESSION_20260712.md`.

불변식: Bot AI는 GameCommand 생산자다. 되감기/스폰/오버라이드 개입은 전부 `eCommandKind::PracticeControl` 명령 왕복(`_DEBUG` 한정)이며, 봇 명령은 저널하지 않는다 — **복원된 상태+RNG에서 재결정** (S015 설계 의도).

## Goal

"멈춤 → 분해 → 조작 → 같은 상황 재실행 → 관찰" 루프(S015에서 성립)를 봇 개발의 세 가지 정식 용법으로 확장한다: (1) 튜닝 루프, (2) 반사실 반응 실험, (3) 골든 시나리오 공장.

## Non-goals

- 리플레이 타임라인 포크 / spatial 셀 직렬화 / 더블 버퍼 복원 (S015 §5 P2 — 별도 슬라이스, 이 문서는 의존성만 명시).
- 인간 플레이 리플레이 시스템 (WRPL 저널은 명령 기록이지 시나리오 자산이 아님).

## 현재 자산 (rg 검증)

- 저장/복원: `Shared/GameSim/Core/Checkpoint/{KeyframeComponentRegistry.h, WorldKeyframe.h/.cpp}` — 컴포넌트 ~90종 + EntityManager + RNG + EntityIdMap, 완전성 기계검사 내장 (`SaveWorldKeyframe`의 unregistered store 즉시 FAIL).
- 링: `CGameRoom` 30틱 간격 × 90개 = 90초 창 (`GameRoomCommands.cpp` `CaptureKeyframeIfDue`, `kKeyframeIntervalTicks=30`/`kKeyframeCapacity=90`).
- 되감기: op 15 `RewindSimulationSeconds` → `m_pendingRewindToTick` → Tick 최상단 `PerformPendingRewind` → **paused 착지** + ingress/pending/lagComp 클리어 + 미래 키프레임 폐기 + spatial Rebuild.
- 개입 수단: F9 봇 튜닝 슬라이더(`SendAIDebugTune`, targetNet 지정), F10 스킬 오버라이드 `Target NetId`(op 10 — 적 봇 수치 튜닝), `SpawnChampion`(op 16 — 17챔피언/Dummy·AI), Pause/Step/TimeScale(op 12~14), 강제행동(`AIDebugControl` ForceAction).
- 기계 증명: `Tools/SimLab/main.cpp` `RunKeyframeRestoreDeterminismProbe` — save@300 → 복원 → 301~600 재실행 해시 완전 일치 + save→restore→save 바이트 동일. 상시 게이트.

## 세 가지 용법

### U1 — 튜닝 루프 (이미 성립, 게이트만 남음)

되감기 → F9 노브 변경 → Resume → 같은 상황에서 달라진 봇 행동 관찰. S015 §3 게이트 A-4가 본질 검증 절차다. 이 문서의 추가분: 관찰을 21의 breakdown/why-not 뷰와 결합해 "노브 X가 점수 항 Y를 바꿔 행동 Z가 됐다"까지 30초 안에 설명 가능하게.

### U2 — 반사실 반응 실험 (신규 — 19의 데이터 공장)

전장의 핀 크레딧(`J_opp(무위협) − J_opp(실제 반응)`)은 반사실 비교였지만 상대를 되돌릴 수 없어 관측 후 기장만 가능했다. **크로노 브레이크는 반사실을 실제로 실행한다**:

```text
키프레임 T에서 되감기 (paused)
→ 개입 A: 내 봇에 ForceAction(예: 다이브 진입) / 개입 B: 무개입(대조군)
→ Resume N틱 → 상대 봇의 반응(후퇴/응수/무시)과 결과(HP/골드 델타) 기록
→ 같은 T에서 반복, 개입만 변경 → (상황, 내 행동) → 상대 반응 분포 축적
```

- 결정론이 대조군을 보증한다: 같은 키프레임+같은 개입 = 같은 결과 (실험 재현성 프로브로 고정).
- 산출 = 19 L2 반응 통계 테이블의 집계 데이터 (JSONL, 21 D3 경유).
- 자동화 착지: 인게임 수동(F10)으로 시작 → SimLab이 키프레임 blob을 로드해 개입 스크립트를 배치 실행(U3 로더 재사용)하는 오프라인 공장으로 승격.
- **P2 의존성**: spatial index가 복원 후 재유도라 되감기 직후 1틱의 공간 쿼리 순서가 무중단 실행과 다를 수 있음 (S015 §5). 실험 비교는 **복원+1틱 이후 구간**으로 한정해 회피하고, `CSpatialIndex::SaveState/RestoreState` 봉인 후 제한 해제.

### U3 — 골든 시나리오 공장 (신규 — 시나리오 하네스의 재료 조달)

16 Phase B의 시나리오 하네스는 "1v1 트레이드" 같은 상황을 손으로 배치해야 했다. 크로노 브레이크로 **실전에서 채집**한다:

```text
인게임에서 흥미로운 상황 발견 → Pause → 키프레임 blob을 파일로 export
→ SimLab `--keyframe <path>` 로더가 그 상황을 초기 상태로 복원
→ 시나리오 프로브가 trace/metric assert (회귀 자산화)
```

- 슬라이스: (a) export — F10 버튼 또는 신규 op `SaveKeyframeToFile` (CONFIRM_NEEDED: op 추가 vs 서버 콘솔 명령 — PracticeControl op 테이블 오염 최소화 관점에서 결정), (b) SimLab 로더 — `RestoreWorldKeyframe`를 파일 blob으로 호출 + 프로브 훅.
- **blob 수명 정책 (CONFIRM_NEEDED)**: 컴포넌트 추가/변경 시 WKF1 blob은 재생 불가가 된다 (완전성 검사가 FAIL). 시나리오 자산의 수명 관리 — (i) 버전 불일치 시 시나리오 폐기+재채집(단순, 초기 권장), (ii) 마이그레이션 지원(비용 큼). 초기엔 (i) + 시나리오 메타(채집일/버전/설명) 기록.

## 검증

- 미검증(현재): S015 §3 인게임 게이트 전체 (U1의 전제 — 최우선 잔여).
- U2 프로브: 같은 키프레임+같은 개입 스크립트 = 같은 결과 해시 (SimLab).
- U3 프로브: export→로드→N틱 재실행이 인게임 무중단 실행과 해시 일치 (RunKeyframeRestoreDeterminismProbe의 파일 왕복판).
- U3 프로브의 전제: 파생 캐시(영향맵 등 레지스트리 미등록 필드)는 20의 갱신 위상 불변식(절대 틱 위상, 주기가 키프레임 간격 30의 약수)을 지켜야 복원 후 재계산 값이 무중단 실행과 일치한다 — 어기면 이 프로브가 영구 FAIL.
- 회귀: 키프레임 캡처 비용(30틱당 1회, ~81KB) 프레임 예산 유지, 기존 프로브 전부 PASS.

## Files touched (예정)

`Server/Private/Game/GameRoomCommands.cpp`(export 배선), `Client/Private/UI/ChampionTuner.cpp`(F10 버튼), `Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h`+`Shared/Schemas/Command.fbs`(op 추가 시), `Tools/SimLab/main.cpp`(로더+프로브), `Tools/BotLab/`(실험 스크립트 러너 — 23).

## Rollback scope

U2/U3는 export/로더/프로브 추가만 — 기존 되감기 경로 무변경. 제거 시 파일 단위 원복.

## Next slice

S015 §3 인게임 게이트 통과 (사용자 수동) → U3-(b) SimLab 키프레임 로더 계획서 (U2 자동화의 기반이므로 U2보다 먼저).
