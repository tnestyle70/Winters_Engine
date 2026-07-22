# 프로파일링 실측 원장 (PROFILING LEDGER)

Release(/O2 + WINTERS_PROFILING) 실측만 기록한다. Debug 수치는 이 원장에 올리지 않는다.
캡처 절차·조건 고정 규칙은 `.md/guide/PROFILING_NARROWING_PLAYBOOK.md` §0.
분석기: `python Tools/Profiler/analyze_profiler_capture.py <capture> --target-fps 144`.

하드웨어 기준: i5-13500HX (6P+8E, 14C/20T), RTX 4060 Laptop, Windows 11, 1280x720 창모드.

| 일시 | 구성 | 시나리오 | frames | median | p95 | p99 | 유효FPS | GPU med | Draw::Total | 오버헤드 p95 | 드롭 | 메모 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 2026-07-17 17:54 | Release | replay room1_tick1_1393 uncapped no-vsync 40s | 6000 | 1.686ms | 2.382ms | 2.999ms | 563.7 | 0.781ms | 488 | 0.020ms | 0 | **베이스라인** — Release 계측 점화 첫 캡처. `Profiles/profiler_20260717_175414_439.json` |
| 2026-07-17 18:04 | Release | 동일 (같은 리플레이·같은 조건) | 6000 | **1.316ms** | 1.963ms | 2.460ms | **708.8** | 0.903ms | 488 | 0.019ms | 0 | **지연 포즈 평가 적용 후** (-22.0% median, +25.8% FPS). `Profiles/profiler_20260717_180458_521.json` |
| 2026-07-17 18:13 | Release | room1_tick1_1393.wrpl uncapped no-vsync 40s | 6000 | 1.076ms | 1.687ms | 2.118ms | 887.5 | 0.782ms | 485 | 0.016ms | 0 | pipeline-selftest(lazy-pose) — 런2와 동일 코드 `profiler_20260717_181244_383.json` |
| 2026-07-17 20:56 | Release | room1_tick1_1393.wrpl uncapped no-vsync 40s | 6000 | 1.007ms | 1.619ms | 1.966ms | 943.3 | 0.771ms | 470 | 0.016ms | 0 | sched-baseline-1 `profiler_20260717_205620_335.json` |
| 2026-07-17 20:58 | Release | room1_tick1_1393.wrpl uncapped no-vsync 40s | 6000 | 1.086ms | 2.529ms | 3.538ms | 774.8 | 0.767ms | 471 | 0.017ms | 0 | sched-baseline-2 `profiler_20260717_205731_797.json` |
| 2026-07-17 20:59 | Release | room1_tick1_1393.wrpl uncapped no-vsync 40s | 6000 | 0.989ms | 1.878ms | 2.662ms | 926.3 | 0.77ms | 464 | 0.016ms | 0 | sched-baseline-3 `profiler_20260717_205843_010.json` |
| 2026-07-17 21:07 | Release | room1_tick1_1393.wrpl uncapped no-vsync 40s | 6000 | 0.857ms | 1.311ms | 1.654ms | 1114.3 | 0.785ms | 472 | 0.015ms | 0 | sched-batch-after-1 `profiler_20260717_210708_539.json` |
| 2026-07-17 21:08 | Release | room1_tick1_1393.wrpl uncapped no-vsync 40s | 6000 | 0.808ms | 1.271ms | 1.682ms | 1167.3 | 0.787ms | 472 | 0.016ms | 0 | sched-batch-after-2 `profiler_20260717_210812_050.json` |
| 2026-07-17 21:09 | Release | room1_tick1_1393.wrpl uncapped no-vsync 40s | 6000 | 1.325ms | 1.707ms | 2.006ms | 731.2 | 0.802ms | 470 | 0.02ms | 0 | sched-batch-after-3 `profiler_20260717_210916_359.json` |

## 판독 메모

- 2026-07-17 최적화: `CAnimator` 지연 포즈 평가 — Update는 시간 전진+dirty만, Evaluate+ComputeFinalTransforms는 소비 지점(본 업로드/본 질의) 1회. 증거: `Update` median 0.723→0.177ms(-75.6%), `Champion::AnimUpdate` 0.425ms→상위 스코프권 밖, `ModelRenderer::RenderSkinned` 0.106→0.300ms(가시 인스턴스 평가 이동분). GPU 시간 불변(동일 드로우) — median 0.78→0.90ms는 실행간 변동 범위.
- **런 간 변동성**: 동일 코드 런2 1.316ms vs 런3 1.076ms(±18%) — 히스토리 6000프레임 창이 실행 마지막 구간만 담아 fps에 따라 다른 리플레이 구간을 샘플링 + 노트북 서멀. 프레임 전체 헤드라인 %는 3×3 반복(중앙값 런)으로 확정하기 전까지 "-22%~-36% 범위"로 말한다. **스코프 수준 증거(Update -76%)는 런 간 안정 재현**(0.177/0.172ms).
- 기존 Debug 수치(17.8→9ms, 9.54ms/94드로콜)와 이 표를 절대 섞지 않는다 — 구성·씬이 다르다.
- 2026-07-17 스케줄러 병렬 배치 점화: LocalStatusEffect phase 4→5(Vision과 co-phase) + 두 시스템 정직 DescribeAccess. **구조 카운터가 결정적 증거** — 3런×6000프레임 전 프레임 상수로 `MaxBatchSize 1→2, ParallelBatches 0→1, SequentialBatches 5→3, SubmittedJobs 0→2`. 워커 실행 실증: 보존 rawEvents 64프레임 중 `LocalStatus::Execute` 10~21회·`Vision::Execute` 16~24회가 비메인 threadId(나머지는 WaitForCounter help-execute로 메인 실행 — 설계 정상). **프레임 중앙값 개선은 주장하지 않는다**: baseline 0.989~1.086 vs after 0.808~1.325로 범위가 겹치고(±18% 변동 규칙), baseline/after 빌드 사이에 병행 세션의 Client 파일·데이터 JSON 변경이 끼어 프레임 비교는 오염됨(회귀 없음 판정만 유효). Codex 병행 편집으로 `Minion_Manager.cpp`/`SnapshotApplier.cpp`의 include가 빠진 상태라 두 빌드 모두 `CL=/FI MinionCombatDef.h` 주입으로 컴파일함 — 그쪽 수복 후 클린 빌드에서는 불필요. 상세: `.md/build/2026-07-17_SCHEDULER_PARALLEL_BATCH_IGNITION_REPORT.md`.
