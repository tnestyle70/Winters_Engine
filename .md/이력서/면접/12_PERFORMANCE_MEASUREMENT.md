# 12. 성능 최적화 / 측정 인프라 — 면접 대비

> 도메인 상태: **working** (측정 인프라는 production, 최적화 Phase 1~7은 설계/로드맵)
> 근거 코드: `Engine/Private/Core/Profiler/`, `Engine/Private/Manager/Profiler/`, `Engine/Private/RHI/DX11/CDX11Device.cpp`
> 근거 문서: `.md/plan/performance/2026-06-12_ENGINE_FULL_OPTIMIZATION_MASTER_PLAN.md`, `.md/plan/performance/2026-06-13_PHASE0_MEASUREMENT_INFRA_PLAN.md`

---

## 0. 한 줄 본질 + 현재 상태

**한 줄 본질**: 이 도메인은 *"엔진의 매 프레임을 거짓말 없이 숫자로 만드는 일"* 이다. 계층형 CPU 프로파일러 + GPU 타임스탬프 + 버전드 JSON 캡처로 "어디서 시간이 사라지는가"를 재현 가능하게 측정하고, 그 측정을 게이트로 삼아 최적화의 성공/실패를 판정한다.

**현재 성숙도(정직하게)**:
- **production**: thread-local scope-stack CPU 프로파일러, DX11 GPU 타임스탬프 쿼리, EMA 안정화 뷰, F4 버전드 JSON 캡처, F11 리미터 토글. 이건 Debug/Release 상시로 매 프레임 돈다.
- **planned (코드 0줄)**: Phase 1~7 최적화 로드맵(맵 컬링 캐시, 잡 병렬화, ECS SoA 재설계, 렌더 스레드 분리, Fiber, GPU 스키닝/인스턴싱). 마스터플랜 문서에만 존재.
- **핵심 차별점**: 측정 인프라를 *먼저* 만들었고, 그 측정이 오히려 내 코드의 한계(`Scheduler::MaxBatchSize=1` — 병렬 배치가 한 번도 발화 안 함)를 드러내 다음 작업을 정의했다. 즉 "측정주도 개발(measurement-driven development)"의 실제 사례다. **300~650 FPS는 마스터플랜의 이론 추정치이지 달성한 수치가 아니다 — 면접에서 절대 "달성했다"고 말하지 않는다.**

---

## 1. 핵심 개념 (본질)

### 1-1. 왜 프로파일러가 필요한가 — 1차 원리

성능 최적화의 1차 원리는 단 하나다: **추측하지 말고 측정하라(Don't guess, measure).** 인간의 직관은 핫스팟을 거의 항상 틀린다. CPU는 분기예측·캐시·OoO 실행 때문에 "이 코드가 무거워 보인다"와 "이 코드가 실제로 시간을 먹는다"가 일치하지 않는다. 따라서 최적화의 첫 단계는 코드를 고치는 게 아니라 **측정 도구를 만드는 것**이다. 측정이 없으면 최적화는 신앙이고, 최적화 후 "빨라졌다"도 증명할 수 없다.

### 1-2. 계층형 스코프 타이밍(hierarchical scope timing)

프레임을 트리로 본다. `Frame > Update > Champion::AnimUpdate`처럼 중첩된 구간(scope)을 push/pop으로 감싸면, 각 구간의 시작·종료 시각과 **depth**를 기록할 수 있다. 이 트리가 있으면 "Update가 3.78ms인데 그 중 1.93ms가 AnimUpdate"처럼 시간을 *귀속(attribution)* 시킬 수 있다.
- 시간 측정은 `QueryPerformanceCounter`(QPC) — Windows의 고해상도 단조 타이머. `QueryPerformanceFrequency`로 tick→ms 환산.
- 왜 `std::chrono`가 아니라 QPC인가? QPC는 RDTSC 기반 하드웨어 카운터를 OS가 보정해 제공하며, frequency가 고정이라 tick 차이를 단순 곱으로 환산할 수 있다. DLL 경계·멀티스레드에서 일관된 단조성을 보장받기 쉽다.

### 1-3. thread_local 스택 — 멀티스레드에서 스코프가 꼬이지 않는 이유

스코프 push/pop은 본질적으로 **콜스택과 같은 LIFO**다. 워커 스레드가 여러 개면 각 스레드가 자기 콜스택을 갖듯, 프로파일러 스택도 스레드별로 분리돼야 한다. 그래서 push 중인 스코프 스택을 `thread_local`로 둔다(`CPUProfiler.cpp:29` `thread_local std::vector<PendingProfilerScope> t_vProfilerStack`). 완성된 이벤트만 mutex로 보호되는 공용 버퍼에 병합한다. 이게 핵심 설계: **수집은 lock-free(thread_local), 병합만 lock.** 락 경합이 스코프마다 일어나면 프로파일러 자체가 관측 대상을 왜곡(probe effect)한다.

### 1-4. GPU 타임스탬프 — CPU 시계로 GPU를 잴 수 없는 이유

CPU와 GPU는 **비동기**다. CPU가 `DrawIndexed`를 호출하는 시각과 GPU가 그 드로우를 실제로 실행하는 시각은 다르다(드라이버가 커맨드를 큐잉). 그래서 CPU 타이머로 렌더 함수를 감싸면 "커맨드 제출 시간"만 재지 "GPU 실행 시간"은 못 잰다. GPU 시간을 재려면 GPU 타임라인 위에 타임스탬프를 박아야 한다.
- **disjoint query**: GPU 클럭은 전력상태(부스트/스로틀)에 따라 주파수가 변한다. disjoint 쿼리는 "이 구간 동안 클럭이 안정적이었나(Disjoint flag)"와 "그 구간의 frequency"를 알려준다. Disjoint이면 측정값을 버려야 한다.
- **timestamp query 2개**(begin/end) 차이를 frequency로 나누면 GPU 경과 시간.
- **non-blocking readback**: GPU 결과는 같은 프레임에 안 나온다(아직 실행 안 끝남). 결과를 즉시 기다리면(GetData blocking) CPU가 GPU를 기다리게 돼 파이프라인이 직렬화된다. 그래서 **N슬롯 링버퍼**로 쿼리를 돌려 수 프레임 지연 후 `DONOTFLUSH`로 폴링한다 — 준비 안 됐으면 그냥 skip.

### 1-5. EMA 스무딩과 안정 행 뷰 — 왜 raw 값을 그대로 보면 안 되나

프레임 타이밍은 본질적으로 노이즈가 심하다(OS 스케줄링, 캐시 미스, GC성 할당). raw ms를 매 프레임 화면에 띄우면 숫자가 떨려서 읽을 수 없다. 그래서 **EMA(지수이동평균)** 로 스무딩한다: `dst += (src - dst) * alpha` (alpha=0.25). 또 스코프가 프레임마다 나타났다 사라지면(조건부 코드) 표가 깜빡인다 — age/hold/prune 로직으로 사라진 행도 일정 프레임 유지하고 서서히 decay시켜 **읽을 수 있는 안정 테이블**을 만든다.

### 1-6. probe effect / 트렁케이션 — 측정 도구의 정직성

측정 도구는 두 가지로 거짓말한다: (1) 측정 자체가 대상을 느리게 만든다(probe effect), (2) 버퍼가 넘쳐서 데이터가 잘렸는데 그걸 "발생 안 함"으로 오독한다. 후자를 막으려고 **캡 도달 시 truncation 플래그를 JSON에 명시**(`truncatedRawEvents` 등)한다. 이건 `.claude/gotchas.md`의 실제 교훈("counter cap=32이면 뒤쪽 counter 누락 → '발생 안 함'으로 오독 금지")에서 나온 설계다.

---

## 2. 왜 이 선택인가 — 기술 스택 선택 + Trade-off

| 선택지 | 장점 | 단점 | 내 결정 이유 |
|---|---|---|---|
| **자체 계층 프로파일러** (vs Tracy 단독) | DLL 경계/게임 로직에 맞춤, F3 HUD 상시, JSON 스키마 직접 통제 | 직접 유지보수, 기능 빈약 | **둘 다 채택.** Tracy는 `WINTERS_PROFILE_SCOPE`가 동시에 호출(`ProfilerAPI.h:47`), 자체 프로파일러는 상시 HUD + 캡처 게이트 용도. Tracy는 깊은 분석, 자체는 항상 켜진 가벼운 계측. |
| **thread_local 수집 + mutex 병합** (vs 전역 lock-per-event) | 핫패스 lock-free, probe effect 최소 | 병합 시점 정렬 필요 | 매 스코프마다 lock 잡으면 멀티스레드에서 프로파일러가 병목이 된다. 수집/병합 분리가 정석. |
| **QueryPerformanceCounter** (vs RDTSC 직접) | OS 보정·코어 이동 안전, frequency 일정 | syscall 약간의 오버헤드 | RDTSC는 코어 간 비동기·주파수 변동 보정을 직접 해야 함. QPC는 그걸 OS가 처리. 신입 1인 범위에서 RDTSC 직접 관리는 과투자. |
| **DX11 timestamp + 4슬롯 링** (vs blocking readback) | CPU-GPU 직렬화 회피 | 4프레임 지연 데이터 | blocking GetData는 CPU가 GPU 끝날 때까지 멈춤 — 측정이 성능을 죽인다. 링버퍼 + DONOTFLUSH가 정답. |
| **버전드 JSON 캡처** (vs 바이너리 트레이스) | 사람이 읽음, git diff·외부 스크립트 분석, 스키마 버전 명시 | 큰 캡처는 용량 큼 | 캡처를 면접·디버깅에서 "열어서 보여줄 수 있어야" 한다. 가독성 > 크기. 캡당 256 raw event라 용량 문제 없음. |
| **측정 먼저 → 최적화 나중** (vs 바로 병렬화) | 핫스팟 오판 방지, before/after 증명 | 당장 빨라지진 않음 | `.claude/gotchas.md`: "새 병렬 경로 만들기 전에 기존 중복 제거 우선." 측정 없이 병렬화하면 `MaxBatchSize=1`처럼 *발화도 안 하는 병렬 경로*를 만들게 된다. |

**근본 Trade-off**: "지금 빨라 보이는 일(병렬화 코드)"을 미루고 "당장은 안 빨라지는 일(측정 인프라)"을 먼저 했다. 신입 1인 프로젝트에서 이게 합리적인 이유는 — 혼자 만든 엔진은 핫스팟에 대한 *검증되지 않은 직관*이 가장 큰 리스크고, 측정 인프라 없이 한 최적화는 "되돌릴 수도, 자랑할 수도 없는" 코드가 되기 때문이다. 실제로 측정이 "병렬 스케줄러가 안 돈다"를 잡아냈다.

---

## 3. 실제 구현 (코드 근거)

### 3-1. 데이터 모델 — `ProfilerTypes.h`
- `ProfilerEvent`(`:9`): `pName, startTicks, endTicks, depth, threadId` — 트리 노드 1개. **threadId**가 있어 워커 스레드 이벤트를 구분(멀티스레드 캡처 근거).
- `ProfilerScopeStat`(`:18`): 같은 이름 스코프의 `totalTicks/maxTicks/callCount` 집계.
- `ProfilerCounter`(`:27`): 이름→누적값(드로우콜 수, GPU us 등).
- 캡 상수(`:4-7`): `PROFILER_MAX_TREE_EVENTS_PER_FRAME=256`, `..._COUNTERS=128`, `..._SCOPE_STATS=128`. 이 캡 도달 여부가 truncation 플래그가 된다.

### 3-2. CPU 프로파일러 핫패스 — `CPUProfiler.cpp`
- `PushScope`(`:62`): QPC 찍고 `t_vProfilerStack`(thread_local)에 depth와 함께 push — **락 없음**.
- `PopScope`(`:73`): QPC 찍고 elapsed 계산 → 여기서만 `m_Mutex` 잡고 (a) raw event 버퍼(캡 256), (b) scope stat 집계에 병합.
- **카운터 중복 버그 수정의 핵심**(`:20-27`, `:122`): `SameProfilerName`이 `a==b` 포인터 비교 *후* `strcmp` 폴백. DLL 경계에서 동일 string literal `"Mesh::DrawCalls"`가 다른 주소로 잡혀 카운터가 두 줄로 중복되던 버그를, 포인터비교→strcmp로 고쳤다. `AddCounter`(`:115`)도 같은 비교 사용.
- `EndFrame`(`:45`): 현재/직전 프레임 버퍼를 `swap`(복사 아님) → 더블버퍼. `WINTERS_PROFILING`이면 확정 카운터를 `TracyPlot`으로도 전송(`:57`).
- DLL export 진입점: `Winters_Profile_Push/Pop/Counter`(`:134-150`)가 `CGameInstance::Get()->Get_CPUProfiler()`로 단일 인스턴스에 기록 → Client/Server 등 모든 모듈이 같은 프로파일러에 쓴다.

### 3-3. API 매크로 — `ProfilerAPI.h`
- `WINTERS_PROFILE_SCOPE(name)`(`:47`): **Tracy ZoneNamedN + 자체 CProfileScope를 동시에** 박는다. `__LINE__` 기반 고유 변수명으로 한 블록 다중 사용 가능.
- `WINTERS_PROFILE_COUNT(name, delta)`(`:24`): 카운터 누적. GPU 타이밍이 이걸 쓴다.
- `WINTERS_PROFILING` 미정의 시 전부 `((void)0)` — 빌드에서 완전 제거.

### 3-4. GPU 타임스탬프 — `CDX11Device.cpp` / `.h`
- 헤더(`.h:143-153`): `kGpuTimingSlots=4`, 슬롯마다 `pDisjoint/pBegin/pEnd` ComPtr + `bPending`, write index, ready 플래그.
- `CreateGpuTimingQueries`(`:905`): disjoint 1 + timestamp 2개를 4슬롯 생성. 실패해도 디바이스 init 막지 않음(`:780` `m_bGpuTimingReady = ...`).
- `BeginFrame`(`:971`): 현재 슬롯이 pending 아니면 `Begin(disjoint)` + `End(begin stamp)`.
- `EndFrame`(`:992`): `End(end stamp)` + `End(disjoint)`, pending=true, write index를 `% kGpuTimingSlots`로 회전. Present 후 `ReadGpuTimingResults`(`:1016`).
- `ReadGpuTimingResults`(`:923`): pending 슬롯을 `DONOTFLUSH`로 폴링 → 준비 안 됐으면 continue. disjoint.Disjoint거나 frequency 0이면 **버린다**(`:945`). 유효하면 `(end-begin)*1e6/freq` → `WINTERS_PROFILE_COUNT("GPU::FrameUs", gpuUs)`(`:949`). 이게 **CPU 바운드 진단의 근거** — CPU `Frame`(ms 단위)과 `GPU::FrameUs`(us 단위)를 대조하면 GPU가 한참 작다.

### 3-5. 안정 행 뷰 — `ProfilerStableView.cpp`
- `Update`(`:42`): 프레임마다 모든 row를 bLive=false로 깔고, 들어온 stat마다 row 찾거나 생성 → EMA 갱신(`SmoothValue` alpha=0.25). 안 들어온 row는 `kStaleDecay=0.94`로 감쇠(`:107`).
- age/visible: `ageFrames <= kHoldFrames(120)`이면 보임. `Prune_ExpiredRows`(`:164`)는 `ageFrames > kPruneFrames(240) && emaTotalMs < 0.001`이면 제거 — 단 `"Frame"`은 절대 안 지움(앵커).
- 정렬(`:141`): `kSortIntervalFrames=12`마다만 정렬 → 표가 매 프레임 흔들리지 않음. visible 우선 → emaTotalMs 내림차순.

### 3-6. 오버레이 + JSON 캡처 — `ProfilerOverlay.cpp`
- `Draw`(`:69`): F3 HUD. 컴팩트 헤더(Frame ms/scope/counter 수) + Details(scope summary, counters, raw event tree).
- `Capture_DisplayFrame`(`:110`): freeze 지원, 샘플 주기(`m_fSampleIntervalSec`)로 표시 갱신 — 화면용은 5~10Hz로 떨림 완화.
- `Save_DisplayFrameToJson`(`:289`): 스키마 `"WintersProfilerCapture.v1"`(`:308`), ticksToMs/frameMs/각 카운트 + **캡/트렁케이션 플래그**(`:318-326`) + stableRows/frameScopes/counters/rawEvents 배열. `WriteJsonString`(`:23`)이 제어문자 escape까지 처리(견고한 JSON). rawEvents는 `baseTicks` 빼서 상대 시각(`:300-304`).

---

## 4. 검증 — 동작을 어떻게 증명했나

이 도메인은 "검증 도구 자체"라서, 검증은 *측정값이 물리적으로 말이 되는가*로 한다.

1. **CPU 바운드 가설 검증**: F4 캡처 JSON에서 CPU `Frame`(9.54ms)과 `GPU::FrameUs`를 대조 → GPU가 1ms 미만이면 "작은 씬임에도 완전 CPU 바운드"가 숫자로 확정된다. 드로우콜 ~94, 인덱스 ~47만은 RTX 4060에서 1ms 미만이라는 게 GPU 카운터로 뒷받침된다.
2. **카운터 중복 버그**: 수정 전 캡처에서 `Mesh::DrawCalls`가 2줄로 찍히던 것을, strcmp 수정 후 1줄로 합쳐지는지로 검증(중복 카운터 0건).
3. **멀티스레드 캡처**: rawEvents의 `threadId` 필드에 메인(예: 11196) 외 워커 스레드 id가 섞여 기록되는지 확인 → Phase 2 병렬화 전에 "워커 스코프가 잡히는가"를 미리 검증(Phase 0 plan S5).
4. **트렁케이션 정직성**: 캡(256/128) 도달 시 `truncatedRawEvents:true`가 JSON에 기록되는지 → 잘린 데이터를 "발생 안 함"으로 오독하지 않게 보증.
5. **재현 가능성**: F4가 `Profiles/profiler_YYYYMMDD_HHMMSS.json` 타임스탬프 아카이브 + 루트 `profiler.json` 둘 다 생성(Phase 0 plan 1-4) → 캡처를 보관·비교 가능. F11 리미터 토글로 "캡 해제 베이스라인"을 별도 확보.

> **정직 포인트**: 자동 골든/회귀 게이트는 *아직 없다.* 검증은 "캡처해서 사람이 숫자가 말이 되는지 본다" 수준이다. Phase 0 plan 본문에도 "빌드 미검증/런타임 미검증" 항목이 명시돼 있다.

---

## 5. 최적화

### 실제로 한 것 (측정 인프라 자체의 최적화 + 측정이 끌어낸 수정)
- **probe effect 최소화**: 핫패스(PushScope)는 thread_local에만 쓰고 lock 안 잡음 → 프로파일러가 관측 대상을 거의 안 흔든다.
- **더블버퍼 swap**(`EndFrame :48`): 프레임 경계에서 vector를 복사 안 하고 `swap` — 할당/복사 0.
- **non-blocking GPU readback**: 4슬롯 링 + DONOTFLUSH로 CPU-GPU 직렬화 회피.
- **측정이 끌어낸 실제 수정 2건**(이게 서사의 핵심):
  1. **카운터 중복 strcmp 수정** — 측정 정확성 버그를 측정으로 발견·수정.
  2. **dt 하드클램프 재설계** — 60fps 경계에서 슬로모션 스터터를 만들던 16.7ms dt 하드클램프를 0.1s 스파이크 상한으로 재설계(`Engine/Private/Core/CTimer.cpp`). 이건 프로파일링 중 발견된 프레임 페이싱 버그.

### 정량 수치 — 정직성 경계
- **"300~650 FPS 달성"은 쓰지 않는다.** 그건 마스터플랜의 이론 추정치(`9.5ms → 1.5~2.0ms`)이고 Phase 1~7 코드는 0줄이다.
- 인용 가능한 실측은 **9.54ms 프레임 / ~94 드로우콜 / GPU sub-ms(CPU 바운드)** — 단, "외워서 말하지 말고 F4로 캡처해 스키마째 보여줄 수 있다"로 방어한다.
- 마스터플랜 단계별 budget(Frame 9.5→6.5→4.5→3.0ms)은 전부 **"측정 예정"** 으로만 말한다.

---

## 6. 구현 예정 (Planned) — 같은 깊이로

마스터플랜(`2026-06-12_..._MASTER_PLAN.md`)은 측정 근거 위에 세운 7단계 로드맵이다. 각 Phase는 **frame budget으로 게이트**되고, budget 미달이면 다음 Phase로 안 넘어간다. 측정값(i5-13500HX, RTX 4060, F5 60fps 캡처)은 §0에 박혀 있다.

### Phase 1 — 메인 스레드 핫스팟 제거 (구조 변경 없음, 목표 9.5→6.5ms)
- **무엇을/왜**: 병렬화 *전에* 중복·재계산부터 죽인다(gotcha 규칙). 가장 큰 건 맵 컬링: `Model.cpp`에서 1080 서브메시를 매 프레임 전수 AABB 테스트(`BuildClipVisibilityMask` 109회/frame, 0.93ms).
- **어떻게**: (a) 탑다운 카메라라 VP 변화가 미세 → VP 임계 변화 시에만 마스크 재계산(캐시), (b) 1080 서브메시를 16x16 균등 그리드 청크로 묶어 청크 AABB 선기각. FOW는 dirty-region + 10~15Hz로 하향 + 프레임 분산.
- **Trade-off 예상**: 캐시 무효화 타이밍을 잘못 잡으면 컬링 결함(보일 게 안 보임). → 새 카운터 `Map::CullChunkRejected/CullMaskReused`로 캐시 적중률 관찰.
- **검증**: budget = `Frame` EMA 6.5ms 이하, `Map::DrawFrustumCulled` 0.3ms 이하, FOW 시각 결함 0.

### Phase 2 — 잡 병렬화 (6.5→4.5ms) — *adversarial 핵심*
- **무엇을/왜**: 측정이 드러낸 진짜 문제. `Scheduler::MaxBatchSize=1` — `kMinParallelBatchSize=2`인데 모든 배치가 1개 시스템이라 **병렬이 한 번도 안 돈다.** "잡을 낼 수 있는 구조"를 "실제로 내는 구조"로.
- **어떻게**: (1) 워커 18개를 P/E 코어로 분리 + affinity/priority, 유휴 스핀을 이벤트 블로킹으로. (2) `Champion::AnimUpdate`(1.93ms, 엔티티별 독립) parallel_for 슬라이스 — *단, `CAnimator::Update`의 공유 상태 유무 확인이 선행 필수*(마스터플랜 미검증 항목). (3) `SystemScheduler.DescribeAccess`의 Write 선언이 과대한지 분해해 무충돌 시스템을 같은 배치로.
- **Trade-off**: 병렬화는 데이터 레이스 리스크. 그래서 Phase 3(ECS 변경 감지)보다 *먼저* 하되 "엔티티별 독립" 확인을 게이트로.
- **검증**: `Scheduler::ParallelBatches>0`, `MaxBatchSize>=3`, 워커 스코프가 rawEvents에 분산 기록, 외부 CPU 부하 중 스터터 재현 테스트 통과.

### Phase 3 — ECS 데이터 지향 재설계 (10배 스케일 대비)
- type_index unordered_map → 정수 ComponentTypeId 배열, Transform position/rotation/world matrix SoA화, version counter 변경 감지(안 움직인 엔티티 스킵), phase 순차 → read/write DAG 스케줄링.
- **검증**: 미니언 600 스폰 스트레스에서 Scheduler+Transform+SpatialHash 합 2.0ms 이하.

### Phase 4 — 렌더 스레드 분리 (프레임 패킷, 4.5→3.0ms)
- Update 말미에 렌더 입력(가시 메시/본 팔레트/UI 쿼드/FOW dirty rect)을 더블버퍼 패킷으로 추출 → 렌더 스레드가 패킷 N 제출하는 동안 메인이 Update N+1. `IRHICommandList`를 DX11 deferred context/자체 커맨드버퍼로.
- 넷코드 55ms 고정 lerp 보간을 서버 타임라인 기반으로 교체(스터터 원인 2)도 여기.

### Phase 5 — Fiber 태스크 그래프 (**조건부**)
- WaitForCounter 스레드 블로킹을 fiber yield로. 잡당 CreateFiber를 128개 풀로.
- **게이트**: Phase 2 후 `Job::WorkerIdleRatio>=30%`일 때만 진행. 아니면 보류(복잡도 대비 이득 없음 — Karpathy 단순성). *현재 Fiber는 dead path(ThreadOnly 기본), 이건 정직하게 인정한다.*

### Phase 6 — GPU 오프로드
- GPU 스키닝(드로우당 32KB CB → 프레임 StructuredBuffer 1회), 미니언 60개 `DrawIndexedInstanced`(94→40 이하 드로우콜), 머티리얼 정렬+텍스처 배열, DX11 한계 내 GPU 컬링(풀 GPU-driven은 DX12 백엔드 후로 보류).

### Phase 7 — 메모리/캐싱
- 프레임 아레나 할당자(per-frame vector 증식 제거), 동일 클립+시간버킷 미니언 포즈 캐시 공유, CB 링버퍼, 스폰 워밍.
- **검증**: 스트레스 씬 1% low가 평균 70% 이상, 프레임 내 힙 할당 0 수렴.

---

## 7. 면접 예상 질문 & 모범 답변

**Q1 (기본). 프로파일러에서 스코프 타이밍이 뭔가요?**
프레임을 중첩 구간 트리로 보고, 각 구간을 push/pop으로 감싸 시작·종료 시각과 depth를 기록합니다. 그러면 "Update 3.78ms 중 AnimUpdate가 1.93ms"처럼 시간을 구간에 귀속시킬 수 있습니다. 시간은 `QueryPerformanceCounter`로 재고, push 중인 스택은 `thread_local`이라 워커 스레드마다 분리됩니다.

**Q2 (기본). 왜 thread_local을 썼나요? 그냥 전역 vector에 lock 걸면 안 되나요?**
스코프마다 lock을 잡으면 멀티스레드에서 프로파일러 자체가 락 경합 병목이 되고, 그게 측정 대상을 왜곡합니다(probe effect). 그래서 수집은 thread_local로 lock-free, 완성된 이벤트 *병합만* mutex로 보호합니다. `PushScope`는 락이 전혀 없고 `PopScope`에서만 한 번 잡습니다.

**Q3 (개념). CPU 타이머로 렌더 시간 재면 되는데 왜 GPU 타임스탬프 쿼리를 따로 만들었나요?**
CPU와 GPU는 비동기라서 `DrawIndexed` 호출 시각과 GPU 실제 실행 시각이 다릅니다. CPU 타이머로 렌더 함수를 감싸면 "커맨드 제출 시간"만 재지 GPU 실행 시간은 못 잽니다. GPU 시간을 재려면 GPU 타임라인에 timestamp를 박아야 하고, disjoint 쿼리로 그 구간 클럭이 안정적이었는지·frequency가 얼마인지 받아 환산합니다. 이걸로 "작은 씬인데 완전 CPU 바운드"를 숫자로 확정했습니다.

**Q4 (설계). GPU 쿼리를 왜 4슬롯 링으로 돌렸나요?**
GPU 결과는 같은 프레임에 안 나옵니다 — 아직 실행이 안 끝났으니까요. 결과를 즉시 기다리면(blocking GetData) CPU가 GPU를 기다리게 돼 파이프라인이 직렬화되고, 그건 측정이 성능을 죽이는 겁니다. 그래서 슬롯을 4개 돌려 수 프레임 지연 후 `DONOTFLUSH`로 폴링하고, 준비 안 됐으면 skip합니다. disjoint flag가 서거나 frequency가 0이면 값을 버립니다.

**Q5 (설계). JSON 캡처에 truncation 플래그를 왜 넣었나요?**
버퍼 캡(raw event 256개)에 도달해서 데이터가 잘렸는데 그걸 "그 이벤트가 발생 안 함"으로 오독하면 잘못된 결론을 내립니다. 실제로 예전에 counter cap이 작아서 뒤쪽 카운터가 누락된 적이 있어요. 그래서 캡 도달 시 `truncatedRawEvents:true`를 JSON에 명시해서, 측정 도구가 정직하게 "여기서부터 못 봤다"를 말하게 했습니다.

**Q6 (디버깅 실전). 카운터 중복 버그는 뭐였고 어떻게 잡았나요?**
`Mesh::DrawCalls` 카운터가 캡처 JSON에 두 줄로 찍혔습니다. 원인은 카운터 키 비교를 포인터 비교로 했는데, DLL 경계에서 같은 string literal이 번역단위마다 다른 주소로 잡혀 같은 이름이 다른 키로 인식된 거였습니다. `SameProfilerName`을 `포인터 동일 → strcmp 폴백`으로 바꿔서 합쳤습니다. 측정 도구의 정확성 버그를, 측정 결과를 보고 발견한 사례입니다.

**Q7 (adversarial). "엔진을 병렬화해서 빨라졌다"고 하셨나요? 실제로 병렬이 도나요?**
아니요, 그렇게 말하지 않습니다. 정직하게 — 병렬 *실행 경로는 구현*돼 있지만 ECS 스케줄러의 `MaxBatchSize=1`이라 병렬 배치가 **한 번도 발화하지 않습니다.** 각 시스템의 Write 접근 선언이 과대해서 모든 배치가 1개 시스템으로 쪼개진 게 원인입니다. 그리고 이건 제 측정 인프라가 잡아낸 겁니다 — `Scheduler::ParallelBatches=0`, `MaxBatchSize=1` 카운터로요. Phase 2에서 DescribeAccess를 정밀화해 무충돌 시스템을 같은 배치로 묶는 게 다음 작업이고, budget은 `ParallelBatches>0, MaxBatchSize>=3`입니다. 저는 "병렬화했다"가 아니라 "병렬 경로를 만들었고 측정이 그게 안 도는 걸 드러내서 다음 작업을 정의했다"고 말합니다.

**Q8 (adversarial). 300 FPS, 650 FPS 같은 수치 — 진짜 달성한 건가요?**
아니요. 그건 마스터플랜의 *이론 추정치*입니다. 현재 9.5ms 크리티컬 패스를 Phase별로 줄였을 때의 계산값이고, Phase 1~7 코드는 0줄입니다. 문서에도 "코드 미반영, 이론 추정"이라고 제가 직접 명시해 뒀습니다. 제가 실측으로 인용할 수 있는 건 9.54ms 프레임, ~94 드로우콜, GPU sub-ms로 CPU 바운드라는 것뿐이고, 이건 F4로 캡처해서 JSON 스키마째 보여드릴 수 있습니다. 최적화는 "측정 인프라를 만들었고 로드맵을 budget으로 게이트했다" 단계입니다.

**Q9 (adversarial). 그럼 이 도메인에서 실제로 "한 일"은 측정 도구뿐인가요? 최적화는 안 했네요?**
측정 도구를 만든 것 자체가 이 도메인의 핵심 산출물이고, 거기서 끝이 아닙니다. 측정이 끌어낸 *실제 코드 수정* 두 건이 있습니다 — 카운터 중복 strcmp 수정, 그리고 60fps 경계에서 슬로모션 스터터를 만들던 16.7ms dt 하드클램프를 0.1s 스파이크 상한으로 재설계한 것. 둘 다 프로파일링 없이는 못 찾았을 버그입니다. 제 주장은 "최적화를 많이 했다"가 아니라 "최적화를 *검증 가능하게* 만드는 인프라를 먼저 깔았고, 그게 이미 두 개의 진짜 버그를 잡았다"입니다. 측정 없는 최적화는 신앙이라고 생각합니다.

**Q10 (심화). Phase 2에서 AnimUpdate를 병렬화한다는데, 그냥 parallel_for 돌리면 되나요?**
안 됩니다. 먼저 `CAnimator::Update`가 엔티티별로 정말 독립인지 — 공유 상태나 정적 버퍼를 건드리지 않는지 확인이 선행돼야 합니다. 마스터플랜에도 이걸 "Phase 2 착수 전 필수 확인" 미검증 항목으로 박아뒀습니다. 독립이 확인되면 엔티티 슬라이스로 잡을 나누고, 1.93ms를 5워커로 ~0.4ms까지 기대합니다. 검증은 결과 비교(병렬/직렬 동일 출력)와 rawEvents의 threadId 분산 기록입니다.

**Q11 (심화). EMA 스무딩을 쓰면 스파이크(1% low)를 놓치지 않나요?**
맞습니다 — EMA는 평균을 위한 거고 스파이크를 가립니다. 그래서 안정 행 뷰는 EMA뿐 아니라 `maxMs`(그 윈도우의 최대)도 같이 들고 있고, raw event 트리는 스무딩 없이 그대로 캡처합니다. 다만 *프레임타임 표준편차/1% low를 카운터로 노출*하는 건 Phase 4 검증 항목으로 잡아둔 미구현 영역입니다. 현재는 max 컬럼 + raw 캡처로 스파이크를 사람이 보는 수준입니다.

**Q12 (심화). 자체 프로파일러가 있는데 Tracy를 왜 같이 쓰나요?**
역할이 다릅니다. 자체 프로파일러는 *항상 켜진* 가벼운 F3 HUD + 통제된 JSON 스키마 캡처용이고, Tracy는 깊은 타임라인 분석·존 단위 추적용입니다. `WINTERS_PROFILE_SCOPE` 매크로가 둘을 동시에 박아서, 평소엔 자체 HUD로 보고 깊게 팔 땐 Tracy on-demand로 붙습니다. Tracy는 Engine DLL 한 곳에만 구현부를 두고 나머지 모듈은 import로 같은 인스턴스에 기록하게 격리했습니다(DLL 경계 관리).

---

## 8. 30초 엘리베이터 피치

"제 엔진 최적화 도메인의 핵심은 화려한 최적화가 아니라 *측정을 먼저 했다*는 겁니다. 계층형 CPU 프로파일러를 thread_local로 깔고, DX11 GPU 타임스탬프 쿼리를 4슬롯 링으로 non-blocking하게 붙이고, 매 프레임을 버전드 JSON으로 캡처하는 인프라를 만들었습니다. 그랬더니 그 측정이 오히려 제 코드의 한계를 드러냈어요 — 제가 만든 병렬 스케줄러가 `MaxBatchSize=1`이라 한 번도 안 돌고, 작은 씬인데 완전 CPU 바운드라는 걸 숫자로 확정했습니다. 그래서 저는 '300 FPS 달성' 같은 말 대신, 이 측정값을 게이트로 삼은 7단계 최적화 로드맵을 가지고 있고, 각 단계는 frame budget으로 합격/불합격을 판정합니다. 저는 추측으로 코드를 고치는 사람이 아니라, 문제를 숫자로 정의하고 measurement-driven으로 검증하는 루프를 보여드리고 싶습니다."
