# Notion 반영 페이로드 — 2026-07-17 lazy-pose-eval

대상: Lab 페이지 `Winters Engine — Extreme Profiling & Optimization Lab`
(<https://app.notion.com/p/3a0b8c3c75e28155850ce8505c6e5cad>)
DB: `Performance Experiments` (`collection://56df5a93-a57b-49fb-bfdb-45b605a2f282`)

이 세션(Claude)에서는 Notion 커넥터가 안 보여 직접 반영 불가 — 커넥터가 열린 세션에서 아래를 그대로 입력한다.

## DB 행 1 — RELEASE-REPLAY-BASELINE

| 필드 | 값 |
|---|---|
| Name | RELEASE-REPLAY-BASELINE — Release 계측 점화 첫 실측 |
| Status | **Measured** |
| Scenario | WRPL replay `room1_tick1_1393.wrpl`, uncapped, no-vsync, dx11, 40s, 1280x720 (B등급 — 진단용. A등급=정상 F5 1080p는 차기) |
| Commit | 미커밋 작업 트리 (2026-07-17, branch codex/2026-07-16-replay-backend-worktree) |
| Before p95 | — (베이스라인 자체) |
| After p95 | Frame median 1.686ms / p95 2.382ms / p99 2.999ms |
| CPU/GPU p95 | CPU 2.38ms / GPU median 0.781ms — **CPU 바운드** |
| Actual draws | Draw::Total 488 (통합 카운터: 깔때기 2+원시 5. ImGui 백엔드 제외) |
| Evidence | `Profiles/profiler_20260717_175414_439.json` + `.md/plan/performance/PROFILING_LEDGER.md` |
| Result | 유효 563.7 FPS. 병목 1위 = Champion::AnimUpdate 0.425ms (프레임 25%) |
| Run date | 2026-07-17 17:54 |

## DB 행 2 — LAZY-POSE-EVAL

| 필드 | 값 |
|---|---|
| Name | LAZY-POSE-EVAL — 애니메이션 지연 포즈 평가 |
| Status | **Measured** (Visual Verified 대기 — 육안 게이트 통과 시 Resume Ready 승격) |
| Hypothesis | 포즈 평가(Evaluate+ComputeFinalTransforms)가 가시성 무관 전 인스턴스에서 돈다 → 소비 지점 지연 평가로 컬링 인스턴스 비용 0화 |
| Change | `CAnimator::Update` 시간 전진+dirty만, `EnsurePoseEvaluated()`를 본 업로드/본 질의에 배치 (`Engine/Public/Resource/Animator.h`, `Engine/Private/Resource/Animator.cpp`) |
| Before | Frame median 1.686ms / Update 0.723ms / Champion::AnimUpdate 0.425ms |
| After | Frame median 1.316/1.076ms (런2/런3) / **Update 0.177/0.172ms (-76%, 런 간 안정)** / AnimUpdate 상위권 소멸 |
| Delta | Update 페이즈 **-76% (구조적, 재현 확인)** · Frame 전체 -22%~-36% (런 노이즈 ±18% 병기, 3×3 프로토콜로 확정 예정) |
| CPU/GPU | GPU 불변 (0.78→0.90/0.78ms, 동일 드로우) |
| Actual draws | 488 → 488/485 (동일 씬 확인) |
| Evidence | before `profiler_20260717_175414_439.json` / after `profiler_20260717_180458_521.json`, `profiler_20260717_181244_383.json` / RESULT `.md/plan/2026-07-17_EXTREME_PROFILING_PIPELINE_RESULT.md` |
| Result | **ACCEPT (시각 게이트 보류)** |
| Run date | 2026-07-17 18:04 / 18:13 |

## 페이지 본문 추가 블록 (섹션 "Current Truth / 측정 계약" 아래)

```text
2026-07-17 갱신 (Claude 세션):
- Release에 WINTERS_PROFILING 점화 — 최적화 빌드 실측 개통 (기존 전 수치는 Debug였음)
- 계측 신설: 통합 Draw::Total/Bind::* (relaxed atomic), GPU 패스 타임스탬프 11개(D3D11 disjoint 링 확장),
  D3D11 파이프라인 통계, ID3DUserDefinedAnnotation 마커, --replay/--run-seconds/--profile-capture-on-exit 무인 캡처
- 원커맨드 파이프라인: Tools/Profiler/run_profile_session.ps1 (빌드→캡처→분석→원장 append)
- Debug 오염 실증: Debug의 Map::Render 28ms가 Release에서 상위권 소멸 — 병목 판정은 최적화 빌드에서만
- 다음 병목 (파이프라인 지목): Scheduler::ParallelBatches=0 / MaxBatchSize=1 — 시스템 병렬화 미가동
```

## 이력서 문장 (Codex §2-9-4 템플릿 준수 — Visual Verified 후 승격)

```text
자체 프로파일러가 지목한 애니메이션 포즈 평가 병목(프레임의 25%)을 지연 평가(lazy evaluation)로
전환해 동일 리플레이 재현 조건(Release /O2 실측)에서 Update 페이즈 p50을 0.72ms → 0.17ms(-76%)로
줄였다. GPU 시간·드로우콜 불변으로 회귀 없음을 계측으로 확인했다.
```
