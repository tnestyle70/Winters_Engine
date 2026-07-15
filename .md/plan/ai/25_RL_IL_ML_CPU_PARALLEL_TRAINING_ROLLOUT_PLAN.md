Session - CPU 병렬 시뮬레이션 기반 PyTorch 학습(IL→RL) 인프라를 P0~P4 슬라이스로 완성한다.

작성일: 2026-07-15
성격: 마스터 로드맵 (각 슬라이스 착수 시 `.md/계획서작성규칙.md` 형식의 구현 계획서로 분해한다 — 이 문서는 코드 diff 지시서가 아니다).
근거: [24_RL_IL_ML_INFRA_AUDIT_20260715.md](24_RL_IL_ML_INFRA_AUDIT_20260715.md) (전수 감사, 앵커 전부 rg 재검증됨).
참조 의무: `Tools/AIResearch/README.md`(현행 정본), `16_BOT_AI_HUMANLIKE_COMPLETION_PIPELINE_GUIDE.md` §1 북극성, `18_EXTREME_BOT_MASTER_PLAN.md`(M0~M5와의 관계는 §Why this order).

## Current sequence

```text
P0_자산보전_게이트수복 (0.5~1일, 즉시)
-> P1_Release_병렬_데이터_공장 (3~5일 — IL corpus 10,000+ 달성)
-> P2_평가축_리그_승급게이트 (3~5일 — 학습이 "강한지" 측정 가능해짐)
-> P3_학습심화_정책실배선 (1~2주 — DAgger 도구 + LearnedControl + 모델 표현력)
-> P4_RL_진입 (수주 — trajectory 계약 + reward + 벡터화(선택) + PPO 1v1)
```

## Goal

감사가 확정한 사실 — "IL은 64레코드 bootstrap까지 실증, RL은 계약 부재, 병렬 팜은 학습 데이터 수율 0" — 에서 출발해, **CPU 병렬 시뮬레이션이 학습 데이터를 대량 생산하고, PyTorch가 오프라인 학습하고, 학습 정책이 측정 가능한 승급 게이트를 거쳐 봇 행동에 반영되는 폐루프**를 닫는다. RL(PPO)은 이 폐루프가 닫힌 뒤에만 진입한다.

## Non-goals

- GPU/멀티스레드 학습 (CPU float64 결정론 계약 유지 — 재설계는 P4 이후 별도 결정)
- 네트워크 분산 rollout (UDP production gate 미완 — 단일 호스트 프로세스 병렬로 충분)
- `Engine/Public/AI` stub(MCTSPlanner/RLBridge/BehaviorTree) 부활 금지 (16 §1.8 — 비권위 dead 경로)
- ONNX/libtorch 네이티브 통합 (P3의 `.wbc` V2(MLP)로 충분한지 먼저 판정)
- 마이크로 액션(이동 좌표·스킬 조준) 학습 (매크로 4후보 폐루프 완성이 선행)
- 09/10 문서(Stage7/8)의 심볼 인용 금지 — CBotLogCollector/CONNXRuntime/IBotEnv는 미존재 (감사 §6-6)

## Why this order

1. **P0이 전부의 전제**: 755파일 미커밋 상태에서는 어떤 실측도 스냅샷으로 고정되지 않고, Live 스모크 회귀가 공식 corpus 재생성을 막고 있다. 자산 보전 없이 P1을 시작하면 감사가 경고한 소실 리스크(S025 evidence, NYPC 원료) 위에 쌓는 셈이다.
2. **P1(데이터)이 P2(평가)보다 앞**: corpus 64레코드로는 리그를 돌려도 의미 없다. 이미 실측된 최고 처리량 경로(8액터 6,002 tick/s)에 드레인만 부착하면 되는, 부착점이 확정된 작업이다.
3. **P2(평가)가 P3(배선)보다 앞**: 승급 게이트 없이 LearnedControl을 배선하면 "강해졌는지" 판정 불가 — S025가 세운 EVALUATION_ONLY 경계를 승급 판정 없이 넘지 않는다.
4. **P4(RL)가 마지막**: PPO는 trajectory 계약+reward+대량 rollout이 전부 선행돼야 한다. 감사 결과 이 셋이 전부 부재이므로 IL 폐루프의 부산물로 준비한다.
5. **18(M0~M5)과의 관계**: 본 계획은 M5(학습형 연장)와 M1(BotLab 툴체인)의 실행판이다. M2(Perception 2.0)·M4(반응 모델)는 P3의 관측 확장과 접점이 생길 때 재개한다. 레인 정본은 Codex 레인의 `Tools/AIResearch`로 확정한다 (BotLab 디렉토리 신설 금지 — 23 문서의 레인 충돌은 이 선언으로 중재).

## 북극성 불변식 (모든 슬라이스 공통)

1. **Bot AI는 GameCommand 생산자다. 게임플레이 truth(HP/쿨다운/데미지/이동/사망)를 직접 변경하지 않는다.** 학습 정책의 개입도 반드시 command 산출→`CDefaultCommandExecutor` 단일 초크포인트를 거친다.
2. 학습 계측은 권위 상태를 오염시키지 않는다 — `ChampionAIResearchDebugComponent`는 transient(키프레임 제외) 계약 유지, 기계검사 존치.
3. 결정론 우선: 모든 corpus/정책 아티팩트는 seed+rules_hash+SHA-256 provenance를 가진다. fail-closed가 기본값.
4. 성능/승급 주장 금지 라벨(CONTRACT_ONLY / SHADOW_ONLY / EVALUATION_ONLY)은 해당 게이트를 실제로 통과하기 전까지 제거하지 않는다.
5. `hard safety -> active commitment -> new utility` 결정 순서 준수 (gotchas 2026-07-12).

---

## P0 — 자산 보전 & 게이트 수복 (0.5~1일)

### 작업
1. **AI 트랙 커밋**: `Tools/AIResearch/**`(untracked 전체), `.md/plan/ai/17~25`, `Shared/GameSim/Systems/ChampionAI/*` 수정분, `Tools/SimLab/main.cpp`, `Tools/Harness/*`를 분리 커밋. (`__pycache__`는 .gitignore 추가)
2. **증거 사본 고정**: `%TEMP%\WintersS025ActiveFinal-*` → `.md/build/evidence/s025_active_canary/` 복사. NYPC untracked 원료 2종(`belief_fact_ledger.py`, `claude_turn_inspect.py`)은 NYPC 레포 커밋 또는 Winters측 사본+SHA 고정 중 택1.
3. **Live 스모크 회귀 수복**: illegal Farm 선택(selected=3, legal=0x3, illegal=0xC, exec=Accepted) 원인 규명. 디버깅 파이프라인 규칙 적용 — 결정 시점 utility/legality trace를 먼저 계측하고 코드 추론은 그 뒤. legality 산정과 executor 수락 검증의 불일치이므로 ChampionAIValuation/ChampionAISystem의 legal mask 산출과 후보 선택 경로를 대조. CONFIRM_NEEDED: 원인이 07-15 미커밋 변경(142파일)인지 커밋 3847f3f인지.
4. **Release 재빌드**: SimLab Release + `RunGameRoomBotMatchSoak.ps1 -Configuration Release` + Server Release — 현 트리 기준 병렬 팜 재현 가능 상태 복구.
5. **환경 고정**: `Tools/AIResearch/requirements.txt`(python 3.14.4 / torch 2.13.0 / numpy 2.4.4 기록) 추가.

### 검증 게이트
- `RunValidation.ps1 -BuildCpp` 전 단계 green (live smoke 240틱 promotable 트레이스 복구 포함)
- SimLab 골든 1800/42 = 85A270CA375932B7 유지 (회귀 수복이 골든을 바꾸면 의도 갱신 여부를 명시 판단)
- git log에 AI 트랙 커밋 존재, evidence 디렉토리에 s025 추가 확인

## P1 — Release 병렬 데이터 공장 (3~5일)

### 작업
1. **소크 하네스 트레이스 드레인 부착**: `GameRoomBotMatchSoak.cpp`의 샘플링 분기 근처에서 `CGameRoomIntegrationProbeAccess`(이미 `CGameRoom` friend, `GameRoom.h:95`)로 `ForEach<ChampionAIResearchDebugComponent>` 순회 → `WriteAiDecisionTraceCaptureV1`(헤더온리·비게이트, include 경로 기부착 — vcxproj 수정 불요) 호출. 드레인 주기는 **시뮬 시간 3초 미만**(16칸 링 = 5Hz 봇당 3.2초 분량). **fail-closed 계수기 필수**: 드레인된 레코드 수 vs commandSequence 증가량 대조, 불일치 시 FAIL (링 유실은 무증상이므로).
2. **5v5 episode metadata 계약 결정** (CONFIRM_NEEDED — P1 첫 결정 포인트):
   - 안 A: 결정 경계 상태 해시의 저비용 구현 (기존 `ComputeAuthoritativeResearchStateHash` 서브셋 — `SaveWorldKeyframe` 사용 금지, 처리량 붕괴 위험)
   - 안 B: trace-only corpus를 받는 완화된 exporter 계약 (`ExportAiEpisodeV1.py`에 metadata-lite 모드 추가)
   - 판정 기준: 안 A의 결정당 해시 비용 실측이 tick 예산의 5%를 넘으면 안 B.
3. **seed allocator + 병렬 수집 오케스트레이터**: S029 §9 제안(manifest ordinal) 구현. N프로세스 × 서로 다른 seed → per-actor 출력 디렉토리 → corpus 병합+dedup(identity 검사는 SimLab 패턴 이식). 학습(python)과 수집의 lane 분리 (현 preflight는 python 프로세스 존재 시 throw — 수집 lane은 이 게이트를 완화한 별도 러너로).
4. **시나리오 확장**: 3/4-way legal mask conflict 시나리오를 measured registry에 추가 (현 RETREAT_VS_ONE_MACRO_BOOTSTRAP 2-way 한정 해소). Recall/DefendMid의 V1 계약 편입 여부는 CONFIRM_NEEDED (스키마 V2 논의로 미루면 P4와 병합).
5. **rules_hash 계약 확장**: Release 수집 시 rules_hash = 해당 Release exe SHA-256 (README의 Debug 관행을 Release lane으로 복제 — Debug/Release corpus 혼합 금지).

### 검증 게이트
- 드레인 부착 후 재측정 tick/s 공표 (6,002 수치 인용 금지 — 드레인 비용 포함 신규 실측)
- corpus **≥ 10,000 Accepted decisions + 100+ frozen groups + holdout 분리** (README 자체 기준)
- 서로 다른 seed N액터 병렬 실행에서 per-actor 결정론(각자 2회 실행 해시 일치) 유지
- 드레인 계수기 0 유실 증명

## P2 — 평가 축: 리그/승급 게이트 (3~5일)

### 작업
1. **`RunOfflineLeague.py` 신설**: NYPC `lab_selfplay.py`의 스케줄링/집계 골격(~60줄: 전쌍 라운드로빈×양진영, W/D/L+승점+헤드투헤드)만 이식. 실행층은 "1매치=1 SimLab/소크 프로세스"로 교체(ThreadPool referee·battlefield subprocess는 매니페스트 금지 의존). 통계층(신뢰구간)과 영속층(JSONL)은 **신규 작성** — 원본에 없음(감사 확인). `league.py`를 원료로 쓰려면 bridge_manifest 등재 선행. seed·policy hash를 결과 행에 기록.
2. **frozen opponent + `WintersPolicyLeagueReportV1`(MEASURED_LEAGUE) 생산** → `PolicyPromotionGate.py`가 실데이터로 판정하도록 배선 (현재는 report 형식 검증만).
3. **크로노 반사실 러너(선택)**: `run_s338` → `RunChronoCounterfactual.py` 개명 이식 (원 파일명 복사는 DoNotCopy 스캔에 걸림). SETSTATE 대체는 GameSim keyframe+저널 — keyframe blob export/SimLab 로더가 선행 필요하므로 P2에서는 스코프 판단만 (CONFIRM_NEEDED).

### 검증 게이트
- 학습 정책 vs RuleBased 베이스라인 리그 결과 (승률+신뢰구간, seed/policy hash 완비)가 JSONL로 영속
- PolicyPromotionGate가 MEASURED_LEAGUE evidence로 PASS/FAIL을 실판정

## P3 — 학습 심화 + 정책 실배선 (1~2주)

### 작업
1. **DAgger 교정 저작 도구**: F9 trace 리뷰에서 `AiDecisionCorrectionSidecarV1` JSON을 생성하고 episode 파일 SHA+canonical record SHA를 자동 계산하는 CLI/패널. (클라 `AiTraceExport`는 학습 원료 불가 확정 — 서버/SimLab episode 기준으로 저작.) `--corrected-only` fail-closed 규율 유지.
2. **LearnedControl 배선**: `CDecisionChampionBrain` TODO 해소 — `.wbc` 정책이 매크로 후보를 선택하고 이후 계층(HFSM/콤보/안전)은 룰 유지. **Release용 정식 policy-actuation 경로 신설** (AIDebugControl 재사용 금지 — executor 레벨 `_DEBUG` 컴파일 아웃이며 Release에선 silent no-op). 승급 게이트(P2) PASS 정책만 배선 가능. 안전 우선순위(`hard safety -> active commitment -> new utility`)가 learned 제안을 덮는 구조 유지 + safety_override 계수.
3. **모델 표현력 V2** (CONFIRM_NEEDED — P3 결정 포인트): `.wbc` V2(소형 MLP) 설계 시 포맷+C++ 디코더(`ChampionAIPolicy.cpp`)+트레이너 3곳 동시 개정. 게이트: Python↔C++ parity delta + ns/eval 벤치가 서버 틱 예산 내.
4. **관측 확장**: influence map 9x9를 typed observation에 연결 (feature order SHA는 V2 스키마로 명시 개정 — 조용한 드리프트는 fail-closed가 잡지만 계획된 개정으로).

### 검증 게이트
- Debug SimLab에서 LearnedControl A/B 결정론 유지 + 리그 재판정 통과 후에만 서버 노출
- shadow→active 전환은 룸 단위 opt-in 플래그로 격리, 기본값 off

## P4 — RL 진입 (수주)

### 작업
1. **AiEpisodeV2(PPO trajectory 계약)**: dense per-tick observation + explicit next observation + discount + recurrent state 자리. V1(BC)과 병존 (V1 corpus 무효화 금지).
2. **reward 채널 설계**: HP델타 단일 → 승패 터미널/골드/오브젝트 이벤트 채널 구성. potential-based shaping + reward hacking 감사 절차.
3. **인프로세스 벡터화 (선택 — 학습 처리량이 프로세스 병렬로 부족할 때만)**: 감사 확정 견적 2~3일 — ①GameSim 진단 카운터 20곳 `std::atomic`화(삭제 금지 — dead diagnostics gotcha) ②훅/레지스트리 워커 스폰 전 선등록 계약(assert/once_flag 강제) ③per-thread RunMatch + keyframe restore-as-reset ④**동일 seed 1-thread vs N-thread 월드 해시 완전일치 게이트를 첫 산출물로**. GameRoom(미니언/포탑 포함) 벡터화는 Server 카운터 ~24곳+Engine 재감사가 추가되는 별도 트랙.
4. **PPO 1v1 (recurrent)**: 위 계약 위에서 최소 구성 — league(P2)가 평가 축.

### 검증 게이트
- trajectory 계약 round-trip 테스트 (C++ 캡처→Python 파싱→학습 입력 텐서)
- PPO 정책이 리그에서 RuleBased+BC 베이스라인 대비 유의미 승률 (신뢰구간 포함)

---

## 2. 검증

미검증:
- 본 문서는 로드맵이며 코드 미변경 — P0~P4 각 슬라이스는 착수 시 `.md/계획서작성규칙.md` 형식 구현 계획서로 분해 후 적용
- P1 드레인 비용, P3 MLP ns/eval, P4 벡터화 견적(2~3일)은 실측 전 추정치

검증 명령 (각 슬라이스 공통 회귀):
- `Tools/AIResearch/tests/RunValidation.ps1` (P0 이후 -BuildCpp green이 기본선)
- `Tools/Bin/Debug/SimLab.exe 1800 42` → 골든 해시 일치
- `python -B -m unittest discover Tools/AIResearch/tests` → 80+ OK

확인 필요:
- P0-3 회귀 원인 (미커밋 142파일 vs 커밋 3847f3f)
- P1-2 metadata 계약 안 A/B 판정
- P1-4 Recall/DefendMid 계약 편입 시점
- P2-3 크로노 반사실 러너 스코프
- P3-3 `.wbc` V2 착수 여부 (선형 성능 천장 확인 후)

## Next slice

P0 착수 계획서 작성 (`.md/계획서작성규칙.md` 형식): ①커밋 분리 목록 확정 ②live 스모크 회귀의 utility/legality 계측 코드 지점 ③evidence 복사 명령 전문. P0-3(회귀 수복)은 착수 전 `rg`로 legal mask 산출 경로 재검증 필수.
