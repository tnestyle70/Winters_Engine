# 24 — RL/IL/ML 학습 인프라 전수 감사 보고서 (2026-07-15)

작성일: 2026-07-15
성격: 조사 보고서 (코드 변경 없음). 반영 계획은 [25_RL_IL_ML_CPU_PARALLEL_TRAINING_ROLLOUT_PLAN.md](25_RL_IL_ML_CPU_PARALLEL_TRAINING_ROLLOUT_PLAN.md).
조사 방법: 문서 4영역 + 코드 7영역 병렬 전수 + 적대적 검증 4건 + 완전성 비평 + 추가 조사 5건 (멀티에이전트 워크플로, 총 18 에이전트). 핵심 주장은 **2026-07-15 워킹트리에서 실제 재실행**으로 확증.
질문: **"PyTorch 학습(IL/RL/ML)을 CPU 병렬 시뮬레이션 기반으로 지금 돌릴 수 있는가?"**

---

## §1 한 줄 판정

> **IL(행동복제)은 조건부 YES — 지금도 돌아간다. RL(PPO)은 NO — 계약 자체가 없다. CPU 병렬은 프로세스 격리만 GO(8액터 실측), 인프로세스 멀티월드는 NO-GO(단, 수리 견적 2~3일로 좁혀짐).**

| 축 | 판정 | 실증 범위 | 최대 병목 |
|---|---|---|---|
| IL (BC) | **가능 (실행 검증됨)** | SimLab→AiEpisodeV1→PyTorch masked-BC→`.wbc`→C++ shadow/canary 전 구간 배선·오늘 재실행 PASS | corpus 64레코드 (목표 10,000+의 0.6%), 2-way mask bootstrap |
| DAgger | **계약만 완성** | corrected-only fail-closed 파이프 실존 | 교정 sidecar 저작 도구 전무 (수작업 JSON+수작업 SHA) |
| RL (PPO) | **불가** | 없음 | PPO trajectory 계약·gym step/reset API·reward 체계·league 전부 부재 |
| CPU 병렬 시뮬 | **프로세스 격리 GO** | 1/2/4/8 액터 실측, 8액터 6,002 world-tick/s (효율 76.7%, 20논리코어) | 병렬 팜이 학습 데이터를 **한 건도 생산하지 않음** (드레인 미부착) |
| 모델 표현력 | **선형 한정** | 67-feature bias-free 선형 `.wbc` (WBCPOL1) | C++ NN 추론 런타임 없음 (ONNX는 스텁, `CRLBridge::LoadModel`=항상 false) |
| 정책→봇 반영 | **SHADOW_ONLY** | 서버 `--ai-shadow-policy` 로드+재채점만. active 개입은 Debug SimLab 1v1 canary 한정 | `CDecisionChampionBrain`=TODO 폴백, promotion gate=report-only |

## §2 적대적 검증 결과 (4건)

| # | 주장 | 판정 | 요지 |
|---|---|---|---|
| 1 | SimLab 헤드리스 비트정확 결정론 달성 | **CONFIRMED** | f32 비트 해시(`main.cpp:194-199`), 같은 시드 A/B + seed+1 민감도 게이트, `/fp:precise` 전 프로젝트 일치. 단 결정론 범위=동일 머신·동일 빌드·GameSim 서브셋(서버 전용 페이즈 제외) |
| 2 | S022 PyTorch BC는 실제 torch 학습이고 지금 재실행 가능 | **CONFIRMED** | `torch.autograd.grad` 실학습(`TrainImitationRankingBaseline.py:709-715`). 보존 corpus로 재학습해 `.wbc` SHA·CE 수치를 **바이트/자릿수 단위 재현**. torch 2.13.0+cu126/Python 3.14.4 설치 확인 |
| 3 | 파이썬 정책이 C++ 봇 의사결정에 실제 개입 | **PARTIAL** | 개입 경로 실존하나 (a) 포맷은 JSON이 아니라 `.wbc` 바이너리 (b) 실증 범위는 Debug SimLab 1v1 canary + 수제(비학습) 정책. 학습 정책은 shadow(비개입). 라이브 서버 개입 없음 |
| 4 | CPU 병렬 실행 검증 완료, N배 확장 차단 요소 없음 | **PARTIAL** | 병렬 실행 검증은 진짜(수치 자릿수까지 evidence 일치). 그러나 ①same-seed 복제만 검증 ②N≤8 ③persistent pool 부재 ④학습·수집 동시 실행이 하네스 게이트로 차단 ⑤Debug/단기 horizon 측정 — 5개 차단·미검증 요소 실재 |

## §3 실존 인프라 지도 (2026-07-15 코드 기준)

```text
[생산]  Shared/GameSim ChampionAISystem (5Hz 결정)
        └─ AiDecisionTraceV1 (528B POD) → 봇당 16칸 링 (Release에서도 기록 활성, 빌드 게이트 없음)
[수출]  Tools/SimLab CLI 3종 (--export-ai-research-smoke / --export-ai-research-episode / --run-ai-active-macro-episode)
        └─ decision_trace_v1.bin + episode_metadata.json (reward=HP우위 델타, next_state_hash, terminal/truncated)
[변환]  Tools/AIResearch: ExportAiEpisodeV1.py → AiEpisodeV1 JSONL → ValidateAiEpisode.py → MaterializeImitationDataset.py
[학습]  TrainImitationRankingBaseline.py — 기본 NumPy pairwise / opt-in --backend pytorch-masked-bc
        └─ CPU float64 단일스레드 full-batch masked CE, 결정론 강제 (use_deterministic_algorithms + set_num_threads(1))
[산출]  WBCPOL1 .wbc (67-feature bias-free 선형) + policy JSON (provenance)
[환류]  Server --ai-shadow-policy(-sha256) → SHA fail-closed 로드 → EvaluateChampionAIShadowPolicyV1 재채점 (SHADOW_ONLY)
        SimLab Debug canary: 키프레임 save→preflight→restore→AIDebugControl one-shot 주입 (2-pass)
[병렬]  RunGameRoomActorScalingPreflight.ps1 / RunActiveAiActorScalingPreflight.ps1 — 프로세스 격리 1~32 (실측 8)
[검증]  RunValidation.ps1 (매니페스트→Python 80테스트→물질화→BC 스모크→C++ 프로브→SimLab 골든)
```

핵심 앵커 (rg 재검증 완료):
- 봇 두뇌 4층: HFSM 8상태 + 골드환산 utility + Brain 3종(RuleBased/PlayerLike/Decision) + 챔피언 프로필. `CDecisionChampionBrain`은 TODO 폴백 (`ChampionAIBrain.cpp:128-131`), 런타임 `brainType=Decision` 할당 코드 없음
- shadow 정책 계약: `ChampionAIPolicy.h:68-115` (67피처, 4후보, fail-closed 디코드), 서버 로드 `Server/Private/main.cpp:34-35,516-589`
- active 주입은 executor 레벨 `#if defined(_DEBUG)` (`CommandExecutor.cpp:3573`) — SimLab CLI 게이트(`main.cpp:8289`)보다 한 층 깊음. Release에서는 **no-op 수용(silent)**
- 병렬 팜 최고 처리량 경로(`GameRoomBotMatchSoak.cpp`)에 트레이스 드레인 0건 — 링이 덮어써지며 소멸, **학습 데이터 수율 0**
- 브리지는 전면 파일 기반 배치. 소켓/IPC/pybind 실시간 경로 0건. 정책 리로드 없음(기동 시 1회, const 멤버)

## §4 오늘(07-15) 워킹트리 재실행 결과

전부 이번 감사에서 실제 실행한 결과다 (07-12~14 스냅샷 기록 아님):

| 검증 항목 | 결과 |
|---|---|
| AIResearch Python 테스트 | **80/80 OK** (구 기록 "61건"은 stale — 현재 80건) |
| pytorch-masked-bc 학습 2회 | **PASS** — 해시 완전일치 (report=B4A2D94A…, binary=36E14DBF…) |
| 학습→`.wbc`→C++ shadow parity 1사이클 | **PASS** — 오늘 갓 뽑은 trace에 `--verify-ai-shadow-policy` exit 0 |
| SimLab 골든 (1800틱 seed42) | **PASS** — 85A270CA375932B7, seed+1 상이 |
| active learned-policy canary 프로브 | **PASS** — 2-run SHA 일치, 실제 intervention 발생, 잘린 `.wbc` fail-closed |
| 측정 에피소드 수출 4패밀리 | 개별 **PASS** (retreat/fight/farm/siege) |
| **RunValidation.ps1 -BuildCpp (공식 게이트)** | **FAIL** — 아래 확정 회귀 |
| bridge manifest (NYPC SHA 10 entries) | **PASS** — 단 원료 2종이 NYPC 워크트리 untracked (소실 위험) |

**확정 회귀 1건**: Live AiEpisodeV1 스모크에서 240틱 강제 최종 결정이 illegal Farm 후보를 선택 (`selected=3(Farm)`, `legal=0x3(Retreat|Fight)`, `illegal=0xC`, `exec=2(Accepted)`) → promotable 트레이스가 tick 207에서 끊김(expected 240). 이 스모크가 `-BuildMeasuredCorpus`보다 무조건 먼저 실행되므로 **공식 corpus 재생성 경로도 막혀 있음**. ChampionAI가 illegal 후보를 선택하고 executor가 Accepted 처리한 것 자체가 valuation/legality 회귀일 수 있다. 원인 후보: 07-15 미커밋 GameSim 변경(142파일) 또는 커밋 3847f3f "ChampionAI common valuation skeleton". README의 "07-13 -FullBuild PASS" 기록은 현 트리에서 재현되지 않는다 (문서-현실 괴리 확정).

바이너리 현황: `Tools/Bin/Release/SimLab.exe` 부재(Debug만 존재, vcxproj Release 구성은 정의됨). 병렬 팜용 `GameRoomBotMatchSoak_Release.exe`(%TEMP%)와 `Server/Bin/Release/WintersServer.exe`는 07-14 04:26 산출물로 07-15 01:51 소스 변경 미반영 — 현 트리 재현에는 Release 재빌드 필요.

## §5 격차 전수 (RL/IL/ML 관점 통합)

### 5-1. 데이터 공장 (최우선 병목)
- 병렬 팜(6,002 tick/s)이 트레이스를 영속화하지 않음 — 드레인 부착점은 확정됨: `CGameRoomIntegrationProbeAccess`가 `CGameRoom` friend(`GameRoom.h:95`), `WriteAiDecisionTraceCaptureV1`은 헤더온리·비게이트, include 경로 기부착 → **vcxproj 수정 없이 include 한 줄+드레인 루프로 연결 가능**
- 5v5 episode metadata 생산 수단 부재 — per-decision `next_state_hash`/`reward`/`terminal`은 SimLab 1v1 경로에만 존재. 결정 경계 상태 해시를 `SaveWorldKeyframe`으로 구현하면 처리량 붕괴 위험
- seed 다양화 미구현 — 실측은 전부 SAME_SEED_REPLICA. manifest ordinal seed allocator는 제안만 존재(S029 §9). measured corpus 빌더는 순차 루프
- 시나리오 협소 — Ashe vs Jax 1v1 4계열 하드코딩, 2-way mask, FlatWalkable 평면(실제 navgrid 미사용). 3/4-way conflict 데이터 0건
- 링 유실 무증상 — 16칸 링(5Hz 기준 봇당 ~3.2초 분량), 드레인 주기 놓쳐도 조용히 덮어씀. fail-closed 계수기 필요

### 5-2. RL 계약 (전무)
- AiEpisodeV1은 decision-event BC/ranking 계약 — dense per-tick obs/next obs/discount/recurrent state 없음 (문서 자인: `AI_EPISODE_V1.md`, README:219)
- gym식 step/reset API·Python↔C++ 온라인 브리지 0건 (pybind/소켓/IPC 없음)
- 보상: SimLab 스모크의 HP우위 델타 스칼라뿐. 승패/골드/오브젝트 보상 채널·credit assignment 없음
- self-play/league/frozen opponent/curriculum 없음. `bridge_manifest.json` 9 entry 중 구현 2개뿐 (RunOfflineLeague.py 등 7개 타깃 미존재)

### 5-3. 행동 공간/관측
- 액션 = 매크로 4후보(Retreat/Fight/Farm/Siege)뿐. 이동 좌표·스킬 조준·콤보 스텝은 룰 소유. Recall/DefendMid는 V1 계약 밖이라 trace에서 제외 (커버리지 공백)
- influence map(9x9 4레이어) 구현됐으나 정책 입력·에피소드 미연결. terrain LOS·network FOW 미완
- 봇이 LevelSkill 명령을 방출하지 않음 — 스킬 레벨업이 학습 후보에서 누락

### 5-4. 모델/승격
- `.wbc`=67 선형 가중치 전용. MLP/RNN을 실을 방법 없음 (포맷·C++ 디코더·트레이너 3곳 동시 개정 필요). libtorch/onnxruntime 미통합, `CRLBridge`=스텁
- promotion 루프 미완결: PolicyPromotionGate.py는 report 형식 검증만. 학습→평가→승급→재학습 폐루프 안 닫힘
- GPU/멀티스레드 학습 금지 계약 (결정론) — 대규모 corpus 도달 시 학습 처리량 경로 미설계

### 5-5. 인간 데이터/DAgger
- F9 교정 저작 도구 전무 — sidecar는 수작업 JSON+수작업 SHA 계산 (실사용 불가 수준)
- 클라이언트 `AiTraceExport`는 학습 원료 불가 **확정** — 봇당 최신 1행만 복제(SnapshotBuilder), 관측 필드·후보 evidence·메타데이터 전부 결여, 포맷도 불일치
- WRPL→학습 데이터 변환기 없음 (명령 저널 정확 재시뮬 미구현) — 사람 플레이 데이터가 학습에 못 들어감

### 5-6. 병렬화
- 인프로세스 멀티월드 blocker 실목록 (전수 감사 결과, S029 요약보다 훨씬 좁음):
  - GameSim mutable static = **정확히 3부류**: ①진단 로그 rate-limit 카운터 20곳(게임플레이 무관) ②17챔피언 `s_bRegistered` 등록 가드(비원자 check-then-set) ③`CGameplayHookRegistry` 함수포인터 테이블(유일한 진짜 전역)
  - 레지스트리들은 등록 후 진짜 read-only 확인. 월드 상태는 이미 world-local (TickContext 주입, SimLab이 한 프로세스 수십 월드 순차 실행 중)
  - **수리 견적 2~3일**: P0=카운터 20곳 원자화+훅 선등록 계약(0.5~1일), P1=per-thread RunMatch+keyframe reset+seed allocator+1-thread/N-thread 해시 일치 게이트(1~2일)
- GameRoom 인프로세스 병렬은 Server측 카운터 ~24곳 추가 + Engine측(SpatialHash/TurretAI) 미감사 — 별도 트랙
- 포트 9000 하드코딩(Server) — 헤드리스 팜이 무사한 이유는 소켓을 아예 안 열기 때문

## §6 리스크 & 즉시 조치 필요

1. **[자산 소실] Tools/AIResearch 전체 + 문서 17~23 + ChampionAI 연구 코드가 git 미커밋** (755파일 dirty). 디스크 사고 시 파이프라인 전체 유실 — **커밋이 최우선 조치**
2. **[증거 소실] S025 evidence가 %TEMP%에만 존재** (`.md/build/evidence`에 S022/s024/s029만 있음). %TEMP% 정리 시 재현 근거 소실. NYPC 원료 2종(belief_fact_ledger.py, claude_turn_inspect.py)도 untracked — NYPC 정리 시 SHA 재현 불가
3. **[확정 회귀] Live 스모크 illegal Farm 선택** — 공식 게이트+corpus 재생성 차단 중 (§4)
4. **[환경 미고정] requirements/lock 없음** — pip 업그레이드 한 번에 golden fixture SHA 재현 깨질 수 있음. rules_hash가 SimLab.exe 바이너리 SHA에 묶여 코드 변경마다 corpus provenance 무효화 (의도된 보수성이나 스케일 수집과 충돌)
5. **[오독 방지] top1=1.0/holdout CE 0.003은 4클래스 분리 bootstrap fixture의 계약 증거** — 성능 주장 아님 (보고서들 스스로 CONTRACT_ONLY_NOT_MEASURED 라벨 반복). 포트폴리오 서사에서 "IL 완성"으로 과장 금지
6. **[문서 양방향 시차] 09/10(Stage7/8)의 심볼(CBotLogCollector, CONNXRuntime, IBotEnv 등)은 전부 미존재** — 인용 시 hallucination. 반대로 16의 공백 표(brainType 미배선 등)는 이미 해소된 과거 주장. 최신 정본 = `Tools/AIResearch/README.md` + `.md/build/S0xx` 보고서 + 본 감사

## §7 실측 수치 부록

| 항목 | 수치 | 조건 |
|---|---|---|
| GameRoom 병렬 팜 | 1/2/4/8액터 = 979/1,909/3,434/6,002 world-tick/s (효율 0.975/0.955/0.877/0.767) | Release, 1800틱, 10봇, same-seed, 20논리코어 |
| SimLab active canary 팜 | 8액터 540 transitions/s (효율 0.938), peak private 1,487 MiB | **Debug** 2-pass canary — production 수치 아님 |
| 54,000틱 soak | 5v5 봇전 A/B 해시 완전일치, p99 3.4~3.6ms, wall ~107초 | Release, 시뮬시간 30분 가속 |
| S022 학습 | 64 records, train CE 0.0029 / holdout CE 0.00265, Python↔C++ logit parity 1.89e-7 | CPU float64, 400 epoch, 오늘 비트 재현 |
| shadow 추론 | ns/eval 벤치 10,000회 존재 (SimLab 내) | 선형 67피처 |

증거 경로: `.md/build/evidence/s024_bot_soak/`, `.md/build/evidence/s029_ai_actor_scaling/20260714_042900_final/`, `.md/build/evidence/S022/`. 원 보고서: `.md/build/2026-07-1{2,3,4}_S0{17,21,22,24,25,29}_*.md`.

주의: 1,800틱 처리량을 54,000틱으로 외삽 금지(S029 자체 경고). Debug canary 수치와 Release 팜 수치 혼용 금지. bot-step/s(60,023)는 스케줄 스텝이지 decision record 수가 아님.
