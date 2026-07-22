# 2026-07-17 극한 프로파일링 파이프라인 — RESULT

짝 계획서: `2026-07-17_EXTREME_PROFILING_PIPELINE_PLAN.md` · 원장: `performance/PROFILING_LEDGER.md` · 방법론: `.md/guide/PROFILING_NARROWING_PLAYBOOK.md`

## 1. 예측 vs 실측

| 예측 | 실측 | 판정 |
|---|---|---|
| Release 빌드 그린 + F3/게이지 점화 | Engine·Client Release 그린, Debug도 그린. 첫 캡처에서 `GPU::FrameUs`·`Draw::Total`·`GPU::Map` 등 전 게이지 비영 | 적중 |
| `Draw::Total` ~90~150 예상 | **488** — 원시 사이트(UI 플러시·FoW·컨택트섀도우 쿼드·FX)가 기존 부분 카운터가 못 세던 물량의 대부분이었다 | **빗나감 (가장 값진 데이터)** — "~94 드로콜" 서사는 부분 카운터 기준이었음이 실측으로 확정 |
| 리플레이 무인 캡처 완주 | `--replay=... --run-seconds=40 --profile-capture-on-exit` exit 0, 45초, JSON 산출. 로그인·계정 없이 완주 (§3 확인 필요 해소) | 적중 |
| 프로파일러 오버헤드 게이트 1% 이내 | p95 0.016~0.020ms (프레임 1.1~1.7ms의 ~1.5%, 게이트 예산 0.069ms 대비 통과), 드롭 0 | 적중 |
| Release에서 병목 순위 재편 가능성 | **재편 확정**: Debug의 Map::Render 28ms(Codex 캡처)는 Release 상위권에서 소멸. 실제 1위 = `Champion::AnimUpdate` 0.425ms(프레임 25%) | 적중 — Codex §3-1 예측 1과 동시 실증 |
| 패스 Begin/End 쌍 불일치 게이트 없음 | 육안 게이지로 이상 없음(11패스 정상 보고). 게이트 부재는 유지 — 차기 후보 | 예고대로 |

## 2. 판결

**계획 유지 + 1차 최적화 ACCEPT(시각 게이트 보류).** 지연 포즈 평가(`CAnimator` lazy pose eval):

| 지표 | Before | After (런2/런3) | 판정 |
|---|---|---|---|
| Update median | 0.723ms | **0.177 / 0.172ms (-76%)** | 런 간 안정 재현 — 구조적 증거 |
| Champion::AnimUpdate | 0.425ms | 상위 스코프권 밖 소멸 | 구조적 증거 |
| ModelRenderer::RenderSkinned | 0.106ms | 0.300 / 0.243ms | 가시 인스턴스 평가 이동분 (설계 일치) |
| Frame median | 1.686ms | 1.316 / 1.076ms | **-22%~-36%, 런 노이즈 폭 병기** — 3×3 반복으로 확정 전 단일 수치 발화 금지 |
| GPU median | 0.781ms | 0.903 / 0.782ms | 불변 (동일 드로우) |

시각/게임플레이 게이트(챔피언 애니·FX 앵커·스크럽 툴): **사용자 육안 확인 대기**. 리플레이 2회 완주·크래시 0·드로우 카운트 동일(488→485는 FX 프레임 차)이 간접 증거.

## 3. Codex 계획서 검토 결론 (`2026-07-17_EXTREME_PROFILING_OPTIMIZATION_RESUME_NOTION_PIPELINE_PLAN.md`)

Codex 문서는 **측정 계약서로서 우수하고, 실행이 0이었다**("코드 최적화 미착수" 상태로 세션 중단). 본 세션 실측이 그 계약의 Phase 0~4 본질을 하루에 통과했다. 조항별:

| Codex 조항 | 판정 | 근거 |
|---|---|---|
| Debug 수치로 병목 확정 금지 | **채택 + 실증** | Debug Map::Render 28ms가 Release에서 소멸 — Codex의 자체 가설("static map CPU submission")도 Debug 오염이었음이 판결됨 |
| 과거 수치 historical/unverified 격리 | **채택** | 17.8→9ms 등은 측정 방법 서사로만 사용, 헤드라인은 Release 원장 수치로 교체 |
| Profile 별도 구성(OutDir 분리) | **간소화로 대체** | Release+`WINTERS_PROFILING` 2줄이 동일 효과. probe-effect 비교는 define 제거 빌드로 가능. 별도 구성은 배포 요구 생길 때 |
| 3×3 반복·A/B/A | **채택 (차기)** | 런 간 ±18% 변동 실측으로 필요성이 스스로 증명됨. 원장에 정직 주석 |
| WRPL=B등급(진단), 정상 F5 1080p=A등급(최종) | **채택** | 현 수치는 B등급 증거로 기록. A등급 acceptance는 차기 |
| "전체 드로우콜은 외부 도구로" | **부분 대체** | `Draw::Total`이 깔때기 2+원시 5를 통합(Codex가 몰랐던 이번 구현). 잔여 갭=ImGui 백엔드 드로우. RenderDoc 교차 확인은 유효한 차기 항목 |
| 분석기 60Hz/120Hz 게이트 분리 | **채택 (차기)** | 현행 히치 기준 120Hz 고정 확인 |
| Notion Lab 구조(원장·상태 배지) | **채택** | Lab 페이지 `3a0b8c3c75e28155850ce8505c6e5cad` + Performance Experiments DB에 본 실험 행 추가 — 페이로드 `.md/build/profiling/2026-07-17_lazy-pose-eval/NOTION_PAYLOAD.md` |
| 교차 세션 소유권(한 세션만 코드) | **사실로 해소** | Codex 코드 변경 0, 본 세션이 구현 전부 소유. 이후 Codex 세션 재개 시 이 RESULT가 기준 |

## 4. 좁히기 파이프라인 전 단계 검증 매트릭스 (render/update/jobsystem/fiber…)

| 단계 | 증거 (오늘 캡처) | 상태 |
|---|---|---|
| A. 프레임/페이싱 | `Frame::CadenceUs≈WallUs`, `LimiterActive=0` (uncapped 확인) | **검증** |
| B. CPU 귀속 (Update vs Render) | Update 0.723 vs Render 0.929 → 최적화 후 0.17/1.11로 재편 관측 | **검증** |
| C. CPU 세부 (스코프 트리) | `Champion::AnimUpdate` 0.425ms 지목 → 최적화가 예측대로 적중 = 귀속 정확성의 궁극 검증 | **검증** |
| D. Draw/State 규모 | `Draw::Total 488`·`Draw::Indices 2.18M`·`Bind::Texture 279`·`GPU::IAPrimitives 728k` 전부 비영·런 간 일관(488/488/485) | **검증** |
| E. GPU 패스 분해 | `GPU::FrameUs 0.78ms` 중 `GPU::Map 1.26ms(해당 프레임)` 지배 확인, 11패스 보고 | **검증** |
| F. JobSystem/Fiber 레인 | `Scheduler::ParallelBatches=0`·`MaxBatchSize=1`·`SubmittedJobs=0` vs `Transform::SubmittedRootJobs=8` — **스케줄러 병렬이 전혀 안 돌고 Transform만 잡 제출** | **검증 + 차기 병목 지목** |
| G. SIMD/메모리 (샘플러 필요) | 미실행 — VTune/Tracy 샘플링은 외부 도구 게이트. 코드 지형(스칼라 Vec3·SSE2 기본)만 감사 완료 | **유일 미검증 단계** |
| Tracy 타임라인 | 컴파일 확인(ON_DEMAND), 뷰어 접속 미실행 | 사용자 게이트 |

## 5. ⑤ 갱신 — 실측 후 달라진 대가

- Release 계측 상시 포함의 오버헤드는 실측 ~1.5%로 허용 범위. **틀리게 되는 시점**: 외부 배포 빌드 필요 시(define 분리), 또는 per-draw급 고밀도 계측 추가 시(PopScope mutex 벽 — 그 전에 per-thread 버퍼 선행).
- 지연 포즈 평가의 대가: 본 질의(`TryGetBoneGlobalTransform`)가 컬링된 인스턴스에서 발생하면 그 시점 평가 비용 발생(FX 앵커) — 정상 씬에선 이득이 압도. **틀리게 되는 시점**: 오프스크린 본 질의가 프레임당 다수인 콘텐츠(예: 전맵 본 추적 시스템)가 생기면 게이트 재설계.
- 히스토리 창(6000프레임) 꼬리 샘플링의 대가: 런 간 비교는 같은 fps대역이 아니면 다른 리플레이 구간을 본다 — 고정 구간 캡처(시작 트리거) 또는 3×3 중앙값으로 방어.

## 6. 차기 슬라이스 (우선순위)

1. **시각 게이트**: 사용자 육안 — 인게임 챔피언 애니/FX 앵커/'7'키 스크럽 정상 확인 (이력서 승격 전제).
2. **정상 F5 1080p A등급 acceptance** (Codex §2-4 계약 채택): 서버+클라 정상 매치 60초×3.
3. **Scheduler 병렬화**: `MaxBatchSize=1` 원인 제거 — 파이프라인이 지목한 다음 병목.
4. 분석기 60/120Hz 게이트 분리 + RenderDoc 드로우 교차 확인 + Tracy 뷰어 세션.
5. Notion 페이로드 반영 (커넥터 열리는 즉시).
