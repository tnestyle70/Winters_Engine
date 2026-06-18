# 05. 프로파일러·성능 측정 — 박사 연구 심화

> 이 문서는 `00_PHD_Paper_Guide.md`의 개념 틀(특히 §1 "구현 vs 기여", §3 thesis statement, §7 평가 전제)을 전제로 한다.
> 프로파일러는 다른 도메인 문서와 결이 다르다. 렌더링·물리·AI는 "측정 대상"이지만, **프로파일러는 측정 인프라 그 자체이자 동시에 연구 대상**이다.
> 즉 "더 빠른 GI"를 증명하려면 프로파일러가 필요하고, 그 프로파일러의 정확도·오버헤드가 곧 모든 다른 주장의 신뢰도 하한이 된다. 측정 도구의 박사는 "측정 도구를 만들었다"가 아니라 **"측정이라는 행위 자체의 근본 한계를 한 칸 밀었다"**여야 한다.

---

## 0. 이 분야를 박사로 본다는 것 (구현 vs 기여 + top venue)

### 0.1 "프로파일러를 만들었다"는 박사가 아니다

가이드 §1로 즉시 돌아가자.

> ❌ "Tracy처럼 zone 마커 + lock-free 버퍼 + 타임라인 뷰 + GPU timestamp query를 다 구현했다" → 이것은 **엔지니어링 결과물**이다.

Tracy(Bartosz Taudul)는 이미 존재하고, 오픈소스이며, 산업 표준에 가깝다. 그것을 재현하는 것은 석사 프로젝트로는 훌륭하지만 인류 지식에 새 것을 더하지 않는다. 프로파일러 박사의 기여는 다음 중 하나여야 한다.

- **새 알고리즘**: 같은 정확도를 더 낮은 오버헤드로, 또는 같은 오버헤드로 더 높은 해상도로 얻는 수집/샘플링 기법 (가이드 §5-1).
- **새 자료구조**: 고빈도 이벤트를 경합 없이 수집·압축하는 동시성 자료구조 (§5-2).
- **새 시스템/아키텍처**: 기존 도구로는 불가능하던 조합 — 예: fiber가 cross-thread로 마이그레이션하면서도 인과적으로 정렬된 분산 트레이스 (§5-3).
- **새 경험적 발견**: 측정으로 드러난 일반화 가능한 법칙 — 예: "관측 오버헤드 X%가 측정 대상의 스케줄링 결정을 Y% 왜곡한다"는 정량 모델 (§5-5, observer effect).
- **새 평가 방법·벤치마크**: 프로파일러 정확도 자체를 측정하는 ground-truth 벤치마크. **과소평가되지만 강력하다** (가이드 §5-6).

### 0.2 이 분야의 심장: Observer Effect (관측자 효과)

프로파일러 연구를 다른 모든 시스템 연구와 구별짓는 단 하나의 긴장점:

> **측정하는 행위 자체가 측정 대상을 교란한다 (the observer effect / probe effect).**

물리의 Heisenberg와 이름만 겹치는 비유가 아니라, 시스템 측정에서는 **인과적으로 실재하는 효과**다. Gait(1986)가 "The Probe Effect in Concurrent Programs"(*Software: Practice and Experience*)에서 처음 엄밀히 정식화했다: 계측 코드를 삽입하면 타이밍이 바뀌고, 타이밍이 바뀌면 동시성 프로그램의 **스케줄링 결정 자체가 달라져** 원래 재현하려던 버그(race, deadlock)가 사라지거나 새로 생긴다. 즉 측정은 수동적 관찰이 아니라 **시스템 상태에 개입하는 능동적 행위**다.

이 긴장은 모든 세부 주제를 관통한다.
- §1: 계측은 정확하지만 침습적, 샘플링은 저오버헤드지만 통계적 불확실성. → 트레이드오프의 본질.
- §2: 고빈도 zone을 다 기록하면 캐시·대역폭을 잡아먹어 측정 대상을 느리게 만든다.
- §3: GPU timestamp query는 파이프라인을 직렬화(serialize)시켜 GPU 자체를 교란할 수 있다.
- §4: 프로덕션에서 항상 켜두려면(continuous profiling) 오버헤드가 1% 미만이어야 하므로 정확도를 희생해야 한다.

박사 명제는 거의 항상 **"이 트레이드오프 곡선을 새 기법으로 한 칸 옮긴다"**의 형태다.

### 0.3 Top Venue

가이드 §9에서 프로파일러는 "병렬/시스템" 계열에 속하되, 측정·도구 자체의 색이 강하다.

| 성격 | Top Venue |
|------|-----------|
| 시스템·OS·런타임 측정 | **SOSP, OSDI, EuroSys, ASPLOS, USENIX ATC** |
| 성능 측정 방법론·워크로드 특성화 | **ISPASS** (IEEE Int'l Symp. on Performance Analysis of Systems and Software) — 이 분야의 본진 |
| 병렬 프로그래밍·동시성 자료구조 | **PPoPP**, SPAA |
| 프로덕션·대규모 시스템 운영 측정 | OSDI/ATC, 그리고 SoCC(클라우드) |
| 시각화(타임라인·flame graph) | IEEE VIS, EuroVis (보조 venue) |
| 산업(비학술, 인용용) | GDC(Naughty Dog/Frostbite/Insomniac 발표), CppCon |

> **주의(가이드 §9):** Brendan Gregg의 flame graph, Tracy, Optick, PIX, RenderDoc, Nsight, Unreal Insights는 강력한 산업 도구지만 동료심사 학술 출판이 아니다. 영향력·사례 근거로 인용하되, 1차 기여 주장은 학술 venue에 건다.

---

## 1. 샘플링 vs 계측(Instrumentation) 프로파일링

### 1.1 핵심 원리

두 패러다임은 "성능 데이터를 어떻게 시간 축에 귀속시키는가"가 근본적으로 다르다.

**계측(Instrumentation, 침습적)** — 코드에 명시적 마커를 심는다.

```text
BeginScope("Render")        // t0 = now()
   ... 측정 대상 코드 ...
EndScope()                  // t1 = now(); record(t1 - t0)
```

- 측정 단위가 **의미 있는 구간(scope/zone)**이다. "Render는 4.2ms"처럼 인과적으로 해석 가능.
- 정확도(해상도)는 타이머 분해능에 직접 묶인다(`QueryPerformanceCounter`, `rdtsc`).
- 비용은 **호출 빈도에 비례**한다. 매 scope마다 타임스탬프 2회 + 저장 1회. 핫 루프(초당 수백만 회) 안에 넣으면 측정 대상보다 측정 코드가 더 비싸진다.

**샘플링(Sampling, 통계적)** — 일정 주기로 실행을 "멈춰 세워" 그 순간 무엇을 하고 있었는지 본다.

```text
매 N μs (또는 매 M개 하드웨어 이벤트):
   인터럽트 발생 → 현재 PC(program counter) + call stack 캡처
나중에: 각 함수가 샘플에 등장한 비율 = 그 함수가 쓴 시간의 추정치
```

- 측정 단위가 **확률적**이다. "RenderMesh가 전체 샘플의 23%" → "RenderMesh가 CPU 시간의 약 23%".
- 비용은 **샘플링 주파수에만 비례**하고 코드 구조와 무관. 1kHz 샘플링이면 1ms마다 한 번이므로 오버헤드가 본질적으로 낮고 **예측 가능**.
- 단점: **통계적 오차**. 드물게 호출되지만 짧은 함수는 샘플에 안 잡힐 수 있고(blind spot), 정확도는 샘플 수 N의 √N 법칙을 따른다 — 함수의 진짜 비중 p에 대해 추정 표준편차 ≈ √(p(1−p)/N). 즉 **측정 시간을 4배 늘려야 오차가 절반**이 된다.

### 1.2 트레이드오프의 정량화 (박사가 반드시 수치로 말해야 하는 것)

> "계측은 정확하고 샘플링은 싸다"는 학부 수준이다. 박사는 **곡선의 좌표를 댄다.**

- **타임스탬프 비용**: `rdtsc`/`rdtscp`는 현대 x86에서 약 **20~40 cycle**, `QueryPerformanceCounter`는 OS·HW에 따라 **수십~수백 ns**(보통 내부적으로 rdtsc 또는 HPET). Winters의 `CCPUProfiler`는 `QueryPerformanceCounter`를 PushScope/PopScope마다 호출한다(아래 §1.7). scope당 최소 두 번이므로, scope를 초당 100만 번 진입하면 QPC만으로도 수 ms가 사라질 수 있다 — 이것이 "계측은 핫패스에서 위험하다"의 정량적 의미.
- **샘플링 오버헤드 모델**: 오버헤드 ≈ (인터럽트 처리 + 스택 언와인드 비용) × 주파수. 스택 언와인드가 비싸므로(프레임 포인터 없으면 .eh_frame/DWARF 파싱 또는 `CaptureStackBackTrace`), 1kHz에서도 깊은 스택이면 오버헤드가 1%를 넘을 수 있다. Linux `perf`의 frame-pointer vs DWARF vs LBR(Last Branch Record) 언와인드 선택이 정확히 이 비용을 가른다.
- **해상도 한계**: 1kHz 샘플링은 1ms보다 짧은 현상을 직접 관측 못 한다. 프레임 예산이 16.67ms인 게임에서 0.3ms짜리 spike는 샘플 1개 미만으로 묻힌다 → **게임 엔진은 프레임 내부 구조를 봐야 하므로 순수 샘플링만으로는 부족**하다는 것이 핵심 동기.

### 1.3 대표 기존 연구/도구

- **gprof** (Graham, Kessler, McKusick, 1982, *PLDI*): 최초의 널리 쓰인 호출 그래프 프로파일러. 계측(컴파일러 삽입 카운트) + 샘플링(주기적 PC) 혼합. 호출 그래프 비용 귀속(call graph attribution)을 정식화한 고전.
- **Linux `perf`** (Molnar 등, perf_events 서브시스템): 하드웨어 PMU(Performance Monitoring Unit) 기반 샘플링. PEBS(Precise Event-Based Sampling)/IBS로 skid(인터럽트가 실제 명령보다 늦게 잡히는 오차)를 줄인다.
- **Intel VTune / AMD uProf**: PMU 기반, top-down microarchitecture analysis(Yasin, 2014, *ISPASS* — "A Top-Down Method for Performance Analysis and Counters Architecture")로 "front-end bound / back-end bound / retiring / bad speculation" 분해.
- **ETW (Event Tracing for Windows)**: OS 커널 수준 통합 트레이싱. 컨텍스트 스위치, 디스크 IO, GPU 스케줄러(DXGKrnl)까지 같은 타임라인에 — Windows 게임 프로파일링의 토대(WPA, GPUView, PIX의 backend).
- **eBPF** (Linux): 커널에 안전한 샌드박스 프로그램을 동적 삽입해 임의 지점을 계측. **저오버헤드 동적 계측**의 현재 프론티어. Gregg의 BPF 도구군(*BPF Performance Tools*, 2019)이 사실상 레퍼런스.
- **Magpie / 인과 추적 계열**: 분산 시스템에서 요청을 가로질러 인과를 따라가는 계열(→ §4와 연결).

### 1.4 자료구조/알고리즘(의사코드)

**적응적 샘플링(Adaptive Sampling)** — open problem "저오버헤드 고해상도"의 한 접근. 평상시엔 저주파로 돌다가, 관심 신호(프레임 spike, 특정 카운터 임계 초과)가 감지되면 순간적으로 고주파/계측으로 전환.

```text
state: rate = BASE_HZ           // 예: 250Hz
loop every sampling tick:
    capture_pc_and_stack()
    budget_spent += unwind_cost

    # 신호 기반 부스트
    if frame_time > p99_threshold or hw_counter(LLC_MISS) > burst_level:
        rate = BURST_HZ          # 예: 4000Hz, 다음 K프레임 동안만
        enable_instrumentation(hot_subsystem)   # 핫 서브시스템만 zone 마커 켬
    else if cooldown_elapsed:
        rate = BASE_HZ
        disable_instrumentation(hot_subsystem)

    # 오버헤드 상한 자동 제어 (closed-loop)
    if budget_spent / wall_time > OVERHEAD_CAP:   # 예: 1%
        rate = max(MIN_HZ, rate * 0.5)            # 스스로 후퇴
```

핵심 설계 결정 두 가지가 곧 기여 포인트다.
1. **무엇을 신호로 쓰는가** (frame time? PMU 카운터? 큐 깊이?) — 신호 선택이 blind spot을 좌우.
2. **부스트 비용을 어떻게 상한 두는가** — 위 closed-loop가 없으면 부스트가 관측자 효과를 폭발시킨다.

**계측 게이팅(call-site gating)** — 계측을 끄는 비용조차 0이 아니다. Winters는 `eProfilerCaptureMode{ Off, StatsOnly, StatsAndTree }`로 모드 분기한다(§1.7). 박사 각도: 분기 자체를 분기 예측(branch prediction)에 친화적으로, 또는 컴파일 타임 특수화로 제거.

### 1.5 박사급 novel 각도 (open problems)

1. **적응적·신호 구동 하이브리드의 정량 모델**: "어떤 신호 정책이 주어진 오버헤드 예산에서 spike 포착률(detection recall)을 최대화하는가"를 **증명 가능한 최적화 문제**로 정식화. baseline = 고정 주파수 perf.
2. **하드웨어 보조 저-skid 계측**: Intel PT(Processor Trace)/LBR을 써서, 계측 마커 없이도 zone 경계를 사후 복원. "계측의 해상도 + 샘플링의 저오버헤드"를 동시에.
3. **관측자 효과의 보정(de-perturbation)**: 측정된 타이밍에서 프로브 비용을 통계적으로 빼내 "측정 안 했을 때의 타이밍"을 추정하는 모델. Malony 등의 perturbation analysis 계열을 게임 프레임 루프에 일반화.
4. **fiber/협력 스케줄링에서의 샘플링 의미 재정의**: PC 샘플링은 OS thread 단위인데, Winters처럼 한 thread 위에서 fiber가 갈아탈 때 "이 샘플이 어느 논리적 job에 속하는가"가 모호해진다 → §3/§4와 교차하는 미해결.

### 1.6 Thesis statement 예시

```text
"프레임 타임 분포와 LLC-miss 버스트를 신호로 쓰는 closed-loop 적응적 샘플러는,
 1% 오버헤드 상한 하에서 0.5ms 이하 프레임 spike의 포착률을
 고정 1kHz perf 대비 3배 높이면서, 정상 구간 오버헤드를 동등하게 유지한다."
```

falsifiable: "spike 포착률", "오버헤드 상한", "고정 perf baseline"이 모두 측정 가능.

### 1.7 Winters 연결점

Winters에는 이미 계측 기반 프로파일러가 살아 있다 — `Engine/Private/Core/Profiler/CPUProfiler.cpp`, `Engine/Public/Core/Profiler/ProfilerTypes.h`. 이것이 **트레이드오프의 살아있는 교과서**다.

- **현 구조 = 순수 계측**: `PushScope/PopScope`가 `QueryPerformanceCounter`를 부르고, thread-local 스택에 쌓는다. 즉 §1.1의 침습적 패러다임 그 자체.
- **이미 드러난 관측자 효과**: 초기 구현(`CPUProfiler.cpp`)은 `PopScope`마다 `std::lock_guard<std::mutex> lk(m_Mutex)`로 전역 벡터에 push한다. **모든 워커 스레드가 같은 mutex를 두고 경합** → 측정이 측정 대상의 동시성을 직렬화하는 전형적 probe effect. 세션 계획서 `.md/plan/engine/2026-05-21_PROFILER_CPU_FIBER_EXTREME_SESSION_PLAN.md`는 이를 **per-thread `ProfilerThreadState`(cache-line `alignas(64)`) + 프레임 끝 merge**로 바꾸는 리팩터를 담고 있다 → 이것이 §2의 lock-free 수집으로 가는 다리.
- **이미 존재하는 적응 훅**: `eProfilerCaptureMode{ Off, StatsOnly, StatsAndTree }`(계획서 ProfilerTypes.h)는 §1.4의 "계측 게이팅"의 맹아다. `StatsOnly`는 zone 트리를 버리고 집계만(저오버헤드), `StatsAndTree`는 전체 타임라인(고해상도). **박사 testbed: 이 모드 전환을 신호 구동·자동화**하면 §1.5-1의 적응적 샘플러 실험이 그대로 굴러간다.
- **baseline 확보 용이**: `WINTERS_PROFILING` 매크로로 계측 전체를 컴파일 타임에 `((void)0)`로 날릴 수 있으므로(`ProfilerAPI.h`), "프로파일러 OFF vs StatsOnly vs StatsAndTree"의 3점 오버헤드 곡선을 같은 빌드 인프라에서 깨끗이 뽑을 수 있다(가이드 §7 ablation).

---

## 2. 프레임 타임라인·구간(zone) 프로파일링과 시각화

### 2.1 핵심 원리

게임 엔진 프로파일링이 일반 서버 프로파일링과 갈라지는 지점: **프레임이라는 강한 주기 구조**와 **하드 실시간 예산(16.67ms @ 60fps, 6.94ms @ 144fps)**. 평균이 아니라 **꼬리(tail)**가 중요하다 — 평균 8ms여도 매 100프레임마다 20ms spike가 한 번 나면 체감 끊김(hitch)이다. 따라서 측정은 **모든 프레임의 모든 zone을 시간 축에 정렬해 보존**해야 하고, 이는 §1의 통계적 샘플링으로는 불가능하다(그래서 게임은 계측 기반 타임라인이 지배적).

타임라인 프로파일링의 세 요구:
1. **고빈도 이벤트 수집을 경합 없이** — 워커가 12개면 12개 producer가 동시에 쓴다.
2. **계층(중첩 zone) 보존** — `Render > ShadowPass > DrawCall`의 트리 구조.
3. **사후 시각화** — flame graph(누적), 또는 per-thread 타임라인(실시간 순서).

### 2.2 대표 기존 연구/도구

- **Tracy Profiler** (Bartosz Taudul, 오픈소스): 현 시점 게임 zone 프로파일링의 사실상 표준. 나노초 해상도, lock-free per-thread 이벤트 큐, 실시간 원격 스트리밍(클라이언트가 서버 게임에 붙어 라이브로 봄), zone당 수 ns 오버헤드를 목표. **string interning**(이름을 포인터/ID로)으로 이벤트당 바이트를 줄이는 설계가 핵심. (학술 논문은 아니나 사례·설계 근거로 인용.)
- **Flame Graph** (Brendan Gregg, 2011~; "The Flame Graph", *Communications of the ACM*, 2016): 스택 샘플을 폭=시간비중, 높이=스택깊이로 누적 시각화. **샘플 집계의 표준 시각화**. icicle graph(위→아래), differential flame graph(두 프로파일 차이를 색으로)도 같은 계열.
- **Chrome Tracing / Perfetto** (Google): `chrome://tracing`의 JSON trace event 포맷이 사실상 교환 표준. Perfetto는 그 후속으로 프로토버프 기반 대용량 트레이스 + SQL 질의(TraceProcessor).
- **Unreal Insights / Optick / Microprofile / Remotery**: 각각 per-thread 버퍼 + 원격 뷰 + 프레임 경계 모델. 설계는 대동소이(가이드 §5 "동일 접근법").
- **Magic Trace** (Jane Street): Intel PT 기반으로 **계측 없이** 함수 진입/이탈 타임라인을 사후 복원 — §1.5-2의 산업적 실증.

### 2.3 자료구조/알고리즘(의사코드)

**Per-thread SPSC lock-free ring buffer** — 다수 producer 경합을 원천 제거하는 표준. Winters 계획서(`PROFILER_SYSTEM_PLAN.md` §3)와 Tracy/Optick/Insights가 모두 같은 선택을 한 데는 이유가 있다: **글로벌 MPSC 큐는 12+ 워커의 CAS 충돌로 캐시라인 바운싱**을 일으켜 측정이 측정 대상을 느리게 만든다(probe effect).

```text
# Producer: 소유 스레드만 호출, 락 불필요 (Single-Producer)
Push(event):
    w = writeIndex.load(relaxed)
    data[w] = event                     # 덮어쓰기 허용(가장 오래된 것 손실은 프로파일링에서 OK)
    writeIndex.store((w+1) % N, release) # release: 소비자가 데이터를 본 뒤 인덱스를 보게

# Consumer: 프레임 끝에서 1개 수집 스레드만 (Single-Consumer)
DrainAll(fn):
    r = readIndex                       # 소비자 전용, atomic 불필요
    w = writeIndex.load(acquire)        # acquire: 인덱스를 본 뒤 데이터를 보게
    while r != w:
        fn(data[r]); r = (r+1) % N
    readIndex = r
```

이 release/acquire 쌍이 **유일한 동기화**다. Push 경로에 락도 CAS도 없다 → producer 오버헤드 ≈ (타임스탬프 + 배열 store) 수준.

**계층 복원** — Begin/End를 평면 스트림으로 기록하고(`depth` 필드 동봉), 사후에 스택으로 트리 재구성. Winters의 `ProfilerEvent{ startTicks, endTicks, depth, threadId, workerSlot, fiberDepth }`가 정확히 이 모델.

```text
# 평면 이벤트 → 프레임 트리/flame
reconstruct(events sorted by startTicks):
    stack = []
    for e in events:
        while stack and stack.top.endTicks <= e.startTicks: stack.pop()
        e.parent = stack.top if stack else ROOT
        stack.push(e)
    # flame graph: 같은 (parent-chain, name) 경로의 시간 합산
```

**고빈도 압축·원격 스트리밍** (open problem). 초당 수백만 이벤트를 원격으로 보내려면 대역폭이 병목.
```text
- string interning: name → 32bit id (최초 1회만 문자열 전송)
- delta + varint: timestamp를 직전 이벤트 기준 차분 후 가변길이 부호화
- per-thread block 단위 LZ4/zstd: 수집은 lock-free, 압축은 별도 소비 스레드
```

### 2.4 박사급 novel 각도 (open problems)

1. **무손실 고빈도 수집의 한계 곡선**: "이벤트율 R, 코어 수 C일 때, 관측자 효과 ε% 이하로 무손실 수집 가능한 R의 상한은?" — 자료구조(ring/hazard pointer/RCU) 별로 측정·증명. PPoPP/SPAA 색.
2. **적응적 해상도 압축**: 정상 프레임은 집계만(StatsOnly), 예산 초과 프레임만 풀 트리를 무손실로 — §1.4 적응 정책과 §2.3 압축의 결합. "프레임당 평균 바이트 vs spike 정보 보존"의 Pareto.
3. **원격 분산 스트리밍의 인과 정렬**: 다수 클라이언트 + 서버의 zone 스트림을 단일 타임라인에 인과적으로 정렬(→ §4와 핵심 교차).
4. **시각화의 perceptual 평가**: flame graph vs per-thread 타임라인 중 어느 것이 특정 병목 유형을 더 빨리 찾게 하는가 — user study(가이드 §7 정성). VIS 색.

### 2.5 Thesis statement 예시

```text
"프레임 예산 초과를 신호로 무손실 풀-트리 캡처를, 정상 프레임엔 집계-only를 적용하는
 적응적 zone 수집기는, 초당 5M 이벤트 워크로드에서 관측자 효과를 0.8% 이하로 유지하면서
 spike 프레임의 인과 트리를 100% 보존한다(고정 풀-캡처 대비 평균 대역폭 90%↓)."
```

### 2.6 평가 방법 (가이드 §7)

- **Metric**: producer당 이벤트 비용(ns), 관측자 효과(프로파일러 ON vs OFF의 프레임타임 분포 차이 — 평균이 아니라 p99/p99.9), 손실 이벤트율(`droppedEvents` — Winters에 이미 카운터 존재), 원격 대역폭(KB/frame).
- **Baseline**: 프로파일러 OFF, 글로벌 mutex 수집(Winters의 옛 구현 그 자체), Tracy.
- **Ablation**: string interning ON/OFF, 압축 ON/OFF, per-thread vs 글로벌.
- **재현성**: 워크로드(씬, 엔티티 수, 워커 수)·HW 명시. **결정론적 리플레이가 있으면**(Winters에 있음, §아래) 같은 입력으로 ON/OFF를 반복해 관측자 효과를 분산까지 측정 가능 — 강력한 무기.

### 2.7 Winters 연결점

- **이미 lock-free로 가는 중**: `PROFILER_SYSTEM_PLAN.md`의 `CRingBuffer<T,N>`(SPSC, release/acquire) + per-thread `ThreadProfilerState`, 그리고 EXTREME 세션 계획서의 `alignas(64) ProfilerThreadState`가 §2.3 의 정확한 구현. **즉 박사 testbed가 코드로 절반 깔려 있다.**
- **손실 계측 내장**: `ProfilerRuntimeStats{ droppedEvents, droppedScopeStats, droppedCounters }`(EXTREME 계획서)는 §2.6의 "손실 이벤트율" metric을 **공짜로 제공**. 적응적 수집 실험에서 핵심 종속변수.
- **오버레이 = 즉석 시각화·관측 문화**: `Manager/Profiler/ProfilerOverlay.cpp`가 F3 토글로 scope summary / counters / raw frame tree를 그린다. CLAUDE.md가 의무화한 "inspectable debug UI/overlay"가 곧 §2.4-4 시각화 연구의 기성 plumbing.
- **Chrome Tracing 내보내기 계획**(`PROFILER_SYSTEM_PLAN.md`의 `CProfilerExporter`)이 있으므로 Perfetto·외부 도구와의 교차 검증(ground truth 대조)이 가능 → §2.6 정확도 평가의 발판.

---

## 3. GPU 프로파일링과 CPU-GPU 타임라인 상관

### 3.1 핵심 원리

GPU는 CPU와 **비동기**로 돈다. CPU가 draw call을 "제출"하는 시점과 GPU가 그것을 "실행"하는 시점은 보통 1~3프레임 어긋난다(triple buffering). 따라서:

1. **GPU 시간은 GPU의 시계로만 잴 수 있다** — `QueryPerformanceCounter`(CPU 시계)로 draw call 호출을 감싸면 "제출에 걸린 CPU 시간"을 잴 뿐, GPU 실행 시간이 아니다.
2. **timestamp query**: GPU 커맨드 스트림에 "지금 GPU 시각을 기록하라"는 쿼리를 끼워 넣는다. Begin/End 두 timestamp의 차 = 그 구간의 GPU 실행 시간.
3. **disjoint**: GPU 클럭은 전력 상태(DVFS)에 따라 주파수가 바뀐다. D3D11의 `TIMESTAMP_DISJOINT` 쿼리는 "이 구간 동안 주파수가 흔들렸는가(disjoint)"와 "주파수 값"을 같이 준다. disjoint면 그 프레임 측정은 버려야 한다.
4. **읽기 지연(readback latency)**: 방금 제출한 쿼리 결과를 즉시 읽으면 CPU가 GPU 완료를 기다리며 **스톨**한다(= GPU를 직렬화하는 probe effect). 그래서 N-2 프레임 전 결과를 읽는다(triple buffering으로 스톨 회피).

**CPU-GPU 타임라인 상관(correlation)** — 둘은 서로 다른 시계(QPC vs GPU tick)를 쓰므로, 같은 화면에 그리려면 **공통 기준점에서 두 시계를 정렬**하고, 시간이 갈수록 벌어지는 **드리프트(clock drift)를 보정**해야 한다. D3D12 `ID3D12CommandQueue::GetClockCalibration`(CPU·GPU timestamp 쌍을 동시에 반환)이 정확히 이 calibration용 API다.

### 3.2 대표 기존 연구/도구

- **PIX on Windows** (Microsoft): D3D12 캡처·타이밍의 사실상 표준. ETW 기반으로 CPU 큐 제출과 GPU 실행을 한 타임라인에. GPU occupancy·warp 점유까지(IHV 협조 범위에서).
- **RenderDoc** (Baldur Karlsson, 오픈소스): 프레임 캡처·draw call 단위 디버깅·timestamp. 학술 인용 가능한 오픈 도구.
- **NVIDIA Nsight (Graphics/Systems)**, **AMD Radeon GPU Profiler(RGP)**, **Intel GPA**: IHV별 심층. RGP는 **비동기 컴퓨트 큐와 그래픽스 큐의 중첩**, wavefront occupancy를 보여줌 — §3.4의 멀티 큐 상관의 산업적 최전선.
- **GPUView** (ETW 기반): Windows에서 GPU 스케줄러 큐(하드웨어 큐별 패킷)를 시각화 — CPU 제출 ↔ GPU 실행의 원조 상관 도구.
- 학술: GPU 성능 모델·roofline의 GPU 적용(§4.1), warp/occupancy 분석은 ISPASS·IISWC 계열에 다수.

### 3.3 자료구조/알고리즘(의사코드)

**Triple-buffered timestamp query 수집** (Winters `CGPUProfiler` 설계, `PROFILER_SYSTEM_PLAN.md` §5):

```text
frames[3] = { disjointQuery, queryPairs[256], submitted }

BeginFrame(f):
    ctx.Begin(frames[f].disjoint)
BeginGPUEvent(name): ctx.End(beginQuery[i])   # D3D11은 timestamp를 End()로 '찍는다'
EndGPUEvent():       ctx.End(endQuery[i])
EndFrame(f):
    ctx.End(frames[f].disjoint); f = (f+1) % 3   # 다음 프레임으로

CollectResults():                 # N-2 프레임을 읽어 스톨 회피
    fr = oldest_submitted_frame
    GetData(fr.disjoint, &dj, DONOTFLUSH)
    if not ready: return false    # 아직이면 포기(스톨 안 함)
    if dj.Disjoint: return false  # 주파수 흔들림 → 이 프레임 버림
    for pair in fr.queries:
        durMs = (endTick - beginTick) / dj.Frequency * 1000
```

**CPU↔GPU clock calibration + 드리프트 보정**:

```text
# 주기적으로(예: 매 N프레임) 동시 샘플
(cpu0, gpu0) = GetClockCalibration()        # D3D12
... 시간 경과 ...
(cpu1, gpu1) = GetClockCalibration()

# 선형 모델: cpu = a * gpu + b  (a = 드리프트 보정 스케일)
a = (cpu1 - cpu0) / (gpu1 - gpu0)
b = cpu0 - a * gpu0
cpu_of(gpu_tick) = a * gpu_tick + b          # GPU 이벤트를 CPU 타임라인으로 투영

# 비동기 큐가 여러 개면 큐마다 다른 disjoint/calibration이 필요할 수 있음 → §3.4
```

### 3.4 박사급 novel 각도 (open problems)

1. **비동기·멀티 큐 인과 상관**: 그래픽스 큐 + 컴퓨트 큐 + 카피 큐가 동시에 도는데, "이 컴퓨트 디스패치가 어느 그래픽스 패스를 위한 것인가"의 인과 의존(fence/barrier 그래프)을 자동 복원. RGP가 보여주지만 **자동 인과 추론은 미해결**.
2. **GPU occupancy 추론(비침습적)**: timestamp만으로(IHV 카운터 없이) "이 패스가 occupancy-bound인가 latency-bound인가"를 추론하는 모델 — 벤더 종속성을 줄이는 이식 가능한 진단.
3. **드리프트 보정의 정확도 한계**: calibration 주기 vs 정렬 오차의 트레이드오프를 정량화. "얼마나 자주 calibrate해야 CPU-GPU 정렬 오차가 X μs 이하인가" — 측정 자체가 기여.
4. **저오버헤드 GPU 연속 프로파일링**: query는 파이프라인을 부분 직렬화한다. 프로덕션에서 항상 켜도 되는 GPU 측정의 오버헤드/정확도 곡선(→ §4 continuous와 결합).

### 3.5 Thesis statement 예시

```text
"fence 의존 그래프와 timestamp만으로 비동기 컴퓨트/그래픽스 큐의 인과 정렬을 복원하는 기법은,
 IHV 전용 카운터 없이도 멀티 큐 중첩의 병목 패스를 RGP와 90% 일치하게 식별하며,
 드리프트 보정 오차를 50μs 이하로 유지한다."
```

### 3.6 평가 방법

- **Metric**: GPU 측정 오버헤드(query ON/OFF의 GPU frame time 차), CPU 스톨 시간(readback로 인한), CPU-GPU 정렬 오차(μs, calibration 정밀도), disjoint 발생률.
- **Ground truth**: PIX/RGP 캡처를 기준으로 자작 측정의 일치율. **상용 도구를 ground truth로 쓰는 것**이 GPU 프로파일링 평가의 정석.
- **Baseline**: 단일 calibration(드리프트 무시) vs 주기적 recalibration; 즉시 readback(스톨) vs triple-buffered.
- **Threats to validity**: 드라이버 버전·GPU 모델 의존성, DVFS 상태 → 여러 HW에서 반복.

### 3.7 Winters 연결점

- **GPU 프로파일러 설계가 이미 정석**: `PROFILER_SYSTEM_PLAN.md` §5의 `CGPUProfiler`는 `TIMESTAMP_DISJOINT` + triple-buffer(`PROFILER_GPU_FRAME_LATENCY=3`) + `DONOTFLUSH`로 §3.3 의 스톨 회피·disjoint 처리를 정확히 구현. **여기에 §3.3의 calibration 레이어와 §3.4의 큐 상관을 얹는 것이 박사 챕터.**
- **DX12 경로 존재**: 빌드 구성에 `Debug-DX12`(EXTREME 계획서의 빌드 명령)가 있으므로 `GetClockCalibration`(DX12 전용)을 쓸 수 있다 → §3.3 드리프트 보정 실험 가능.
- **CPU 타임라인과 합칠 틀이 이미 있음**: `FrameStats{ cpuFrameMs, gpuFrameMs }`, `FrameTimeline{ cpuEvents, gpuEvents }`(`CFrameInspector`)가 CPU·GPU를 한 프레임 객체로 묶는다 → 상관 시각화의 기반.
- **MOBA + 오픈월드의 이질적 GPU 부하**: Winters는 좁은 MOBA 씬과 넓은 오픈월드 스트리밍을 둘 다 가진다 → 비동기 컴퓨트(컬링·파티클)와 그래픽스의 중첩이 자연스러운 멀티 큐 워크로드 testbed.

---

## 4. 자동 병목 탐지·연속(continuous) 프로파일링 (연구 프론티어)

### 4.1 핵심 원리

지금까지(§1~3)는 "데이터를 어떻게 싸게·정확히 모으는가"였다. §4는 **"모은 데이터에서 자동으로 통찰을 뽑는가"**, 그리고 **"개발 환경이 아니라 프로덕션에서 상시 측정하는가"**다.

**Roofline 모델** (Williams, Waterman, Patterson, 2009, *CACM* — "Roofline: An Insightful Visual Performance Model for Multicore Architectures"): 한 커널의 성능 상한을 단 두 직선으로. x축 = **arithmetic intensity**(FLOP/byte), y축 = 달성 GFLOP/s. 수평선 = 연산 정점(compute roof), 빗변 = 메모리 대역폭 정점(memory roof). 커널이 빗변에 붙어 있으면 **memory-bound**(연산을 늘려도 소용없음, 데이터 이동을 줄여라), 수평선에 붙어 있으면 **compute-bound**. 즉 "최적화 방향"을 자동으로 가리킨다. 이것이 "자동 병목 탐지"의 가장 고전적·원리적 형태.

**Top-down microarchitecture analysis** (Yasin, 2014, *ISPASS*): PMU 카운터로 매 사이클을 Retiring / Bad-Speculation / Front-End-Bound / Back-End-Bound로 분해해 병목 계층을 자동 지목. VTune의 핵심 엔진.

**연속(continuous) 프로파일링**: 프로파일링을 "가끔 켜는 도구"가 아니라 **fleet 전체에서 상시 낮은 오버헤드로 돌리는 인프라**로 본다. 핵심 제약은 단 하나: **오버헤드 < 1%** (안 그러면 프로덕션에 못 켠다). 그래서 거의 항상 §1의 저주파 샘플링 기반.

### 4.2 대표 기존 연구/도구

- **Google-Wide Profiling (GWP)** (Ren, Tene, Moseley, Hundt, 2010, *IEEE Micro* — "Google-Wide Profiling: A Continuous Profiling Infrastructure for Data Centers"): **이 분야의 정전(canonical).** 데이터센터 전체에서 **머신·시간을 가로질러 무작위로 극소수만 샘플링**(per-machine 오버헤드 무시 가능 수준)해, 통계적으로 fleet 전체의 성능 프로파일을 항상 보유. "어떤 함수가 전사적으로 가장 많은 CPU를 쓰는가"를 상시 답함. 핵심 통찰: **개별 머신엔 거의 부하 0, 집계하면 통계적으로 완전**.
- **Continuous Profiling (디지털 시대의 원조)**: Anderson 등, "Continuous Profiling: Where Have All the Cycles Gone?" (1997, *SOSP* / *TOCS*) — DCPI(Digital Continuous Profiling Infrastructure). 저오버헤드 상시 샘플링의 학술적 시조.
- **산업 연속 프로파일러**: Google Cloud Profiler, Parca / Polar Signals(eBPF 기반 fleet-wide), Pyroscope/Grafana, Datadog Continuous Profiler. 모두 GWP+flame graph 계보.
- **회귀(regression) 자동 검출 / 성능 이상탐지**: 커밋 간 성능 비교·변화점 탐지(change-point detection). Daly 등, "The Use of Change Point Detection to Identify Software Performance Regressions in a Continuous Integration System" (MongoDB, 2020, *ICPE*) — CI에서 성능 회귀를 통계적으로 자동 검출한 실증.

### 4.3 자료구조/알고리즘(의사코드)

**연속 프로파일링 집계 (GWP식)**:

```text
# 각 노드: 극저주파 샘플 (오버헤드 << 1%)
on_node(rate = very_low):           # 예: 99Hz, 또는 무작위 짧은 윈도우
    if random() < node_sampling_prob:   # 노드 자체도 일부만
        s = sample_stack()
        ship(compress(s))           # 중앙으로

# 중앙: 시간·머신·빌드버전 차원으로 집계
aggregate(samples):
    weight = 1 / (node_sampling_prob * rate)   # 역가중으로 모집단 추정
    profile[func] += weight * count(func in samples)
    # 빌드 버전별로 쪼개면 → 회귀 검출의 입력
```

**성능 회귀 자동 검출 (change-point detection)**:

```text
series = perf_metric_per_commit[]    # 예: 벤치마크 frame time, 시계열
# E-Divisive 등 비모수 변화점 탐지
changepoints = detect_changepoints(series, significance=0.05)
for cp in changepoints:
    if mean(after cp) > mean(before cp) * (1 + threshold):
        flag_regression(commit_at(cp), magnitude, confidence)
```

**자동 병목 원인 규명 (open problem)** — 단순 핫스팟이 아니라 **인과**를 묻는다.

```text
# "느린 프레임"을 라벨로, 어떤 zone/카운터가 그것을 '설명'하는가
for spike_frame in frames where frametime > p99:
    align spike to: CPU zones, GPU passes, GC/alloc bursts, network stalls, lock waits
    # 상관이 아니라 인과로: 같은 워크로드 결정론 리플레이에서 해당 요인만 제거해보고 spike가 사라지는가
    counterfactual_test(remove=suspect_factor) → 인과 기여도
```

### 4.4 박사급 novel 각도 (open problems)

1. **자동 병목 "원인" 규명**: 핫스팟 나열을 넘어, spike의 인과 요인을 결정론적 리플레이 + counterfactual로 규명. "이 lock 경합이 이 hitch의 원인임을 60% 신뢰도로"처럼 **인과·신뢰도까지**. (가이드 §5-5 새 발견 + §5-1 새 알고리즘.)
2. **분산(클라이언트+서버) 프로파일링 상관**: MOBA는 N개 클라이언트 + 권위 서버. "서버 tick의 spike가 어느 클라이언트의 어떤 입력 폭주와 인과적으로 연결되는가"를 **저오버헤드로** 정렬. → 핵심 연구 긴장점.
3. **fiber 서버의 연속 프로파일링**: fiber가 cross-thread로 마이그레이션하므로 thread 단위 샘플링의 의미가 깨진다(§1.5-4). "논리적 job 단위"로 샘플을 귀속시키는 fiber-aware continuous profiler — Naughty Dog fiber 모델(Gyrling, GDC 2015 "Parallelizing the Naughty Dog Engine Using Fibers")의 측정 대응물.
4. **ML 이상탐지의 함정**: 성능 시계열은 비정상(non-stationary)·다봉(multi-modal)이라 단순 ML이 false positive를 쏟는다. "재현 가능·해석 가능한 회귀 검출"이 미해결.

### 4.5 Thesis statement 예시

```text
"결정론적 네트워크 리플레이와 counterfactual zone 제거를 결합한 분산 프로파일러는,
 1% 오버헤드 하에서 서버 tick spike의 원인(클라이언트 입력 폭주 / 락 경합 / GPU 스톨)을
 단순 상관 기반 baseline 대비 2배 높은 정밀도로, 인과 신뢰도와 함께 자동 규명한다."
```

### 4.6 평가 방법

- **Metric**: 연속 모드 오버헤드(%, 1% 이하가 합격선), 회귀 검출의 precision/recall(주입한 known 회귀 대비), 원인 규명 정확도(주입한 known 병목을 맞히는가), 분산 정렬 오차.
- **Baseline**: 평균 임계 알람(naive), 단순 핫스팟 순위, 상관-only 원인 추정.
- **Ablation**: counterfactual 제거 ON/OFF, 결정론 리플레이 ON/OFF.
- **재현성·통계**: 주입 결함(fault injection)으로 ground truth를 만들고 여러 시드로 신뢰구간. 가이드 §7의 핵심.
- **Threats to validity**: 합성 워크로드가 실제 플레이를 대표하는가, 회귀 주입이 현실적인가.

### 4.7 Winters 연결점 — 가장 강한 testbed

> 핵심 연구 긴장점("다수 클라이언트 + fiber 서버에서 인과적으로 정렬된 분산 트레이스를 저오버헤드로")이 Winters에서 거의 그대로 구현 가능한 형태로 존재한다.

- **결정론적 네트워크 리플레이 = counterfactual의 전제조건**: 서버에 `Server/Private/Game/ReplayRecorder.cpp`가 있고, 서버 fiber 검증 문서(`.md/TODO/05-07/Server/16_VERIFICATION_STRESS_DEBUGGING.md`)는 **"직렬 snapshot과 병렬 snapshot의 packet bytes가 byte-identical"**해야 한다는 결정론 게이트를 못박는다. 결정론 리플레이가 있으면 §4.3 의 "같은 입력에서 한 요인만 제거하고 spike가 사라지는지" counterfactual 실험이 **가능**해진다 — 이것이 인과 규명의 토대.
- **fiber-aware 측정 훅이 이미 설계됨**: `ProfilerEvent`에 `workerSlot`·`fiberDepth` 필드가 있고(`ProfilerTypes.h` EXTREME 계획서), `WINTERS_PROFILE_THREAD_CONTEXT(workerSlot, fiberDepth)`로 fiber 진입/이탈 시 컨텍스트를 갱신한다(EXTREME 계획서 JobSystem 패치). 이것이 §4.4-3 "논리적 job 단위 귀속"의 시작점. fiber 개념 문서(`.md/TODO/05-07/Server/11_FIBER_CONCEPTS_SERVER_DEEP_DIVE.md` §8)는 **"Get_WorkerSlot 값이 yield 전후로 달라질 수 있다"**고 경고하는데, 이것이 바로 §1.5-4/§4.4-3의 "fiber 마이그레이션 시 샘플 귀속 모호성"의 실증.
- **fiber 이벤트 카운터가 이미 깔림**: EXTREME 계획서가 `Fiber::ShellAttempt/ShellRun/ShellComplete`, `JobSystem::Executed` 등 카운터를 심는다. 검증 문서 §8은 `Fiber::YieldWait/ResumeReady/WaitMapInsert/PoolAcquire` 등 **fiber 스케줄링 카운터 집합**을 요구한다 → §4의 "무엇을 신호·요인으로 쓸 것인가"의 후보가 코드 레벨로 준비됨.
- **서버 측 성능 metric도 명시됨**: 검증 문서 §11이 `Tick maxJitter / Phase_BroadcastSnapshot ms / WaitForCounter wait time / worker utilization`을 측정 항목으로 못박고, ThreadOnly serial → ThreadOnly jobified → FiberShell → FiberFull의 **4점 비교**(가이드 §7 ablation 그 자체)를 요구한다. spike(tick jitter)의 원인 규명이 자연스러운 연구 질문.
- **관측 문화가 의무**: CLAUDE.md "Progressive Sections"는 디버깅 전 **inspectable overlay + bounded `OutputDebugStringA/W` trace + 시각적 캡처**를 의무화한다(`CMemoryProfiler::ReportLeaks`가 실제로 `OutputDebugStringA`로 누수를 토한다). 즉 측정 인프라가 팀 규범이라 박사 평가의 절반(가이드 §7 "측정 인프라")이 이미 문화로 존재.

---

## 종합. 통합 학위논문 구조 예시

가이드 §4의 "Three Papers Make a Thesis" 모델로, **하나의 분야(저오버헤드·인과적 분산 프로파일링) 안의 인접한 세 문제**를 묶는다. 관통 명제:

> **학위논문 thesis statement:**
> "관측자 효과를 1% 이하로 통제하면서, 적응적 수집·CPU/GPU 정렬·결정론 리플레이 기반 counterfactual을 결합하면,
> fiber 기반 분산 게임 엔진에서 프레임 spike의 인과를 자동으로, 신뢰도와 함께 규명할 수 있다."

```text
Ch 1. 서론
   - 동기: 실시간 게임의 tail latency(hitch)는 평균이 아니라 인과 규명이 핵심.
   - 문제: 측정이 측정을 교란하고(probe effect), fiber·분산이 귀속을 깨뜨림.
   - Thesis statement(위) + 기여 4개 bullet.

Ch 2. 배경 및 관련 연구
   - 샘플링 vs 계측, Tracy/perf/ETW/eBPF, GWP, roofline, flame graph.
   - gap: 저오버헤드·고해상도·인과·분산을 동시에 만족하는 것이 미해결.

Ch 3. [논문 1] 적응적·관측자효과-통제 zone 수집      (→ §1+§2)
   - 신호 구동 적응 캡처 + lock-free per-thread + 무손실 압축.
   - 평가: 관측자 효과(p99), 손실율, 대역폭. baseline=글로벌 mutex/Tracy.
   - venue: PPoPP / ISPASS.

Ch 4. [논문 2] CPU-GPU 인과 정렬과 멀티 큐 상관        (→ §3)
   - calibration 드리프트 보정 + fence 그래프 기반 큐 인과 복원.
   - 평가: 정렬 오차(μs), PIX/RGP ground truth 일치율.
   - venue: ISPASS / I3D(그래픽스 색이면).

Ch 5. [논문 3] 결정론 리플레이 기반 분산·fiber 인과 규명 (→ §4)
   - counterfactual zone 제거 + fiber-aware 귀속 + 회귀 자동 검출.
   - 평가: 원인 규명 정밀도(주입 결함 ground truth), 1% 오버헤드.
   - venue: OSDI / EuroSys / ATC.

Ch 6. 종합 평가
   - Winters를 testbed로: MOBA 좁은 씬 + 오픈월드 스트리밍 + N클라이언트 부하.
   - 세 기여를 하나의 파이프라인으로 통합한 end-to-end 사례 연구.

Ch 7. 논의 / Ch 8. 결론 및 향후 연구
   - threats to validity(HW 의존, 합성 워크로드 대표성), 일반화 가능성.
```

자가진단(가이드 §12): **"내 thesis는 무엇이고, 무엇으로 측정하며, 무엇과 비교했는가?"** — 위 구조는 셋 다 즉답된다(관측자 효과·정렬 오차·인과 정밀도 / 글로벌 mutex·PIX·상관-only baseline).

---

## 참고문헌

> 표기: 확실히 검증 가능한 1차 문헌·도구만. 학술 논문은 저자·연도·venue, 도구는 제작자·성격 명시(가이드 §10 인용 규범).

**계측·샘플링·프로파일링 기초**
- Graham, S. L., Kessler, P. B., McKusick, M. K. "gprof: A Call Graph Execution Profiler." *PLDI*, 1982.
- Gait, J. "A Probe Effect in Concurrent Programs." *Software: Practice and Experience*, 16(3), 1986. (관측자 효과의 정식화)
- Anderson, J. M., et al. "Continuous Profiling: Where Have All the Cycles Gone?" *SOSP* 1997 / *ACM TOCS* 1997. (DCPI)

**시각화·타임라인**
- Gregg, B. "The Flame Graph." *Communications of the ACM*, 59(6), 2016. (및 brendangregg.com flame graph 원전, 2011~)
- Taudul, B. *Tracy Profiler* (오픈소스, 도구·매뉴얼). — 게임 zone 프로파일링 설계 사례.
- Google. *Perfetto / Chrome Tracing trace event format* (도구·포맷 명세).
- Karlsson, B. *RenderDoc* (오픈소스 GPU 프레임 디버거).

**GPU·CPU-GPU 상관**
- Microsoft. *PIX on Windows*, *GPUView* (ETW 기반 도구).
- AMD. *Radeon GPU Profiler (RGP)*; NVIDIA *Nsight Graphics/Systems* (IHV 도구). — 멀티 큐·occupancy 사례.
- Microsoft. Direct3D 12 Programming Guide — `ID3D12CommandQueue::GetClockCalibration`, timestamp/disjoint query (API 문서).

**성능 모델·자동 분석·연속 프로파일링**
- Williams, S., Waterman, A., Patterson, D. "Roofline: An Insightful Visual Performance Model for Multicore Architectures." *Communications of the ACM*, 52(4), 2009.
- Yasin, A. "A Top-Down Method for Performance Analysis and Counters Architecture." *ISPASS*, 2014.
- Ren, G., Tene, G., Moseley, T., Hundt, R. "Google-Wide Profiling: A Continuous Profiling Infrastructure for Data Centers." *IEEE Micro*, 30(4), 2010.
- Daly, D., et al. "The Use of Change Point Detection to Identify Software Performance Regressions in a Continuous Integration System." *ICPE*, 2020.
- Gregg, B. *BPF Performance Tools.* Addison-Wesley, 2019. (eBPF 동적 계측 레퍼런스)

**fiber·병렬 (테스트베드 맥락)**
- Gyrling, C. "Parallelizing the Naughty Dog Engine Using Fibers." *GDC*, 2015. (학술 아님; fiber 측정 동기로 인용)

**Winters 내부 1차 자료 (testbed)**
- `Engine/Private/Core/Profiler/CPUProfiler.cpp`, `Engine/Public/Core/Profiler/ProfilerTypes.h` — 현행 계측 프로파일러.
- `.md/plan/engine/PROFILER_SYSTEM_PLAN.md` — lock-free 링버퍼·GPU timestamp·Chrome export 설계.
- `.md/plan/engine/2026-05-21_PROFILER_CPU_FIBER_EXTREME_SESSION_PLAN.md` — per-thread 수집·`workerSlot/fiberDepth`·capture mode.
- `.md/TODO/05-07/Server/11_FIBER_CONCEPTS_SERVER_DEEP_DIVE.md`, `.../16_VERIFICATION_STRESS_DEBUGGING.md` — fiber 귀속·결정론·서버 metric 게이트.
- `Server/Private/Game/ReplayRecorder.cpp` — 결정론 리플레이(counterfactual 전제).
