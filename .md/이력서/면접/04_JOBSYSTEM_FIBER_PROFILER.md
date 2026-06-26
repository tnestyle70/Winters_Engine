# 면접 대비 04 — JobSystem / Fiber / Profiler (병렬화)

> 도메인 정직성 등급: **working** (정직성 지도 §4)
> 근거 코드: `Engine/Public/Core/JobSystem.h`, `Engine/Private/Core/JobSystem.cpp`, `Engine/Public/Core/JobSystem/WorkStealingDeque.h`, `Engine/Public/Core/JobCounter.h`, `Engine/Public/Core/Fiber/*`, `Engine/Private/Core/Profiler/CPUProfiler.cpp`, `Engine/Private/RHI/DX11/CDX11Device.cpp`, `Engine/Private/ECS/SystemScheduler.cpp`, `Engine/Private/ECS/Systems/{TransformSystem,NavigationSystem}.cpp`
> 설계 문서: `.md/plan/engine/FIBER_JOB_SYSTEM.md`, `.md/plan/engine/FIBER_JOBSYSTEM_STABILIZATION_PLAN.md`, `.md/plan/performance/2026-06-12_ENGINE_FULL_OPTIMIZATION_MASTER_PLAN.md`, `.md/plan/engine/PROFILER_SYSTEM_PLAN.md`

---

## 0. 한 줄 본질 + 현재 상태

**한 줄 본질**: 멀티코어 CPU에서 프레임 작업을 잘게 쪼개 N개의 워커 스레드에 부하 분산하는 **work-stealing JobSystem**과, 그 병렬화가 "실제로 도움이 되는지"를 매 프레임 ground truth로 재는 **계층형 CPU/GPU 프로파일러**의 묶음.

**현재 성숙도 (정직하게)**:
- **production**: CPU 계층 스코프 프로파일러 + DX11 GPU timestamp 4슬롯 링버퍼. 매 프레임(Debug/Release 상시) 돈다.
- **working**: Chase-Lev work-stealing deque + 글로벌 MPMC 큐 하이브리드 JobSystem, atomic JobCounter, help-stealing WaitForCounter. TransformSystem / NavigationSystem 두 곳에서 임계값 기반 fan-out으로 실제 병렬 실행.
- **prototype / dead-path**: `eJobExecutionMode::FiberShell`(잡마다 `CreateFiber`/`DeleteFiber`하는 셸). 기본 모드는 `ThreadOnly`이고 FiberShell은 라이브에서 켜지 않음. `CFiberPool`은 사실상 빈 클래스.
- **planned-only**: Fiber 풀 + WaitForCounter yield화, 1만 job 스트레스/leak 테스트, Scheduler 배치 병렬 활성화.

> **이 도메인의 핵심 서사**: "병렬 인프라를 만들었더니, 그 인프라가 매 프레임 내보내는 프로파일러 카운터(`Scheduler::MaxBatchSize=1`)가 **병렬 배치가 실게임에서 한 번도 발화하지 않는다**는 사실을 스스로 폭로했다." 측정이 자기 코드의 한계를 드러낸 정직한 사례 — 이게 이 도메인의 무기다.

---

## 1. 핵심 개념 (본질)

### 1-1. 왜 JobSystem이 존재하는가 — first principles

CPU는 더 이상 단일 코어 클럭으로 빨라지지 않는다(Dennard scaling 종료). 성능을 더 얻는 유일한 길은 코어를 늘리는 것이고, 그러면 **소프트웨어가 일을 병렬로 쪼개야** 그 코어를 쓴다. 게임 프레임은 16.6ms(60fps) 안에 Physics/Animation/AI/Culling/RenderSubmit을 전부 끝내야 한다. 이걸 한 스레드에서 순차로 돌리면 코어 1개만 쓰고 나머지는 논다.

JobSystem의 본질은 **"작업(Job)을 데이터로 만들어 큐에 넣고, 놀고 있는 워커가 알아서 집어가게 하는 것"**이다. 핵심 설계 질문 3가지:
1. **부하 분산**: 어떤 워커가 어떤 잡을 처리할지 누가 정하나? → push 기반(스케줄러가 배정) vs pull 기반(워커가 가져감).
2. **의존성**: "A,B,C가 끝나야 D를 시작"을 어떻게 표현하나? → Counter(atomic 카운터).
3. **대기**: D를 들고 있는 스레드가 A,B,C를 기다릴 때 뭘 하나? → 블로킹(스레드 잠듦) vs help-stealing(기다리는 동안 다른 잡 처리) vs fiber yield(스레드는 안 멈추고 컨텍스트만 양보).

### 1-2. Work-stealing deque (Chase-Lev)

각 워커가 **자기 전용 deque**를 하나 가진다. 핵심은 deque의 양 끝을 서로 다른 주체가, 서로 다른 순서로 만지게 해서 경합을 최소화하는 것이다.

- **owner(주인)**: `bottom` 끝에서 `Push`/`Pop` → **LIFO**. 방금 만든 잡을 바로 꺼내므로 캐시가 따뜻하다(데이터가 L1에 살아있음).
- **thief(도둑)**: `top` 끝에서 `Steal` → **FIFO**. 가장 오래된(= owner가 당장 안 쓸) 잡을 가져간다. owner와 반대 끝이라 대부분의 경우 atomic 충돌이 없다.

왜 두 끝을 분리하나? **single-producer / multi-consumer**가 아니라 **owner는 양 끝 빠른 경로, 도둑만 느린 atomic 경로**로 만들기 위해서다. owner의 Push/Pop은 대부분 `relaxed` 로드/스토어로 끝나고, 도둑과 부딪히는 "deque에 1개 남은 순간"에만 `compare_exchange`로 승부를 본다. 이 한 군데 race를 정확히 처리하는 게 Chase-Lev의 전부다.

### 1-3. 메모리 오더링 — 왜 fence가 필요한가

멀티코어에서 컴파일러와 CPU는 명령을 재배열한다. atomic 변수에 순서 보장을 주지 않으면 "버퍼에 데이터를 쓰기 전에 bottom을 증가시킨" 것처럼 보여 도둑이 쓰레기를 읽는다. 그래서:
- `Push`: 버퍼에 값 쓰기 → `release fence` → `bottom` 증가. (도둑이 새 bottom을 보면 값도 본다)
- `Pop`/`Steal`의 마지막 1개 경합: `seq_cst`로 owner의 bottom 감소와 도둑의 top 증가가 전역 순서를 갖게 해, 둘 다 같은 잡을 가져가는 double-pop을 막는다.

### 1-4. False sharing과 `alignas(64)`

`top`과 `bottom`이 같은 64바이트 캐시라인에 있으면, owner가 `bottom`만 써도 도둑의 캐시에서 `top`이 든 라인이 무효화된다(cache-line ping-pong). 둘을 각각 `alignas(64)`로 다른 라인에 배치하면 이 불필요한 무효화가 사라진다.

### 1-5. Fiber — 왜 그리고 언제

Fiber는 **유저 모드 협력적 코루틴**이다. 스레드는 OS가 선점(preempt)하지만, fiber는 스스로 `SwitchToFiber`로 양보할 때만 전환된다. 컨텍스트 전환이 커널 진입 없이 스택+레지스터 교체로 끝나 ~수십ns 수준이다.

게임 엔진이 fiber를 쓰는 단 하나의 이유: **`WaitForCounter`에서 스레드를 블로킹하지 않기 위해서**. 잡 A가 잡 B,C를 기다릴 때, 스레드를 잠재우면 그 코어가 논다. fiber면 A를 대기 리스트에 걸어두고 같은 스레드가 즉시 다른 잡을 집는다 — **워커 스레드는 절대 멈추지 않는다**(Naughty Dog GDC 2015 모델). 단, fiber는 affinity 함정(fiber가 다른 스레드에서 재개되면 TLS가 깨짐), leak, 락 보유 중 yield 데드락 같은 까다로운 문제를 동반한다. 그래서 "잡 안에서 잡을 기다리는 중첩 패턴이 많아진 뒤에만" 도입 가치가 있다.

### 1-6. 프로파일러 — 측정 없는 최적화는 추측

- **CPU 계층 스코프**: `QueryPerformanceCounter`(QPC)로 스코프 진입/이탈 틱을 찍고, `thread_local` 스택으로 중첩 깊이(depth)를 기록한다. QPC는 고해상도 단조 카운터라 프레임 내 미세 구간 측정에 적합하다.
- **GPU timestamp**: GPU는 비동기라 CPU가 "지금 끝났니?"라고 물으면 파이프라인이 멈춘다(stall). 그래서 **N프레임 전에 던진 쿼리를 non-blocking으로 회수**하는 링버퍼가 필요하다. `disjoint` 쿼리는 그 구간에 GPU 클럭이 흔들렸는지(주파수 변동/리셋)를 알려줘, 신뢰할 수 없는 측정을 버리게 한다.

---

## 2. 왜 이 선택인가 — 기술 스택 선택 이유 + Trade-off

| 선택지 | 장점 | 단점 | 내 결정 이유 |
|---|---|---|---|
| **Chase-Lev work-stealing deque** | owner 빠른 경로, 도둑만 atomic, 자동 부하 분산 | 구현 난이도(메모리 오더링), 고정 capacity | 락 없는 부하 분산이 게임 프레임의 불균등 잡 분포에 맞음. 워커가 알아서 노는 코어를 채움 |
| 단일 글로벌 mutex 큐 | 구현 단순 | 모든 워커가 한 락에 경합 → 코어 늘수록 악화 | deque의 **overflow/외부 스레드 fallback**으로만 채택(하이브리드) |
| **JobCounter (atomic)** | 의존성 표현 단순, lock-free | counter→fiber wake 리스트는 별도 필요 | fork-join 한 패턴만 필요 → cv/mutex 없는 순수 atomic 카운터로 충분 |
| **help-stealing WaitForCounter** | fiber 없이도 워커가 안 놀게 함, 구현 단순 | 잡 안 잡 대기 시 스택 깊이 증가, 진짜 yield 아님 | Fiber 도입 전 **현실적 절충**. 워커는 기다리는 동안 다른 잡을 처리 |
| Win32 Fiber 풀 (전면) | 워커 점유율 극대 | affinity/leak/데드락, TLS≠fiber-local 함정 | **지금은 보류**. 중첩 wait 패턴이 적어 복잡도 대비 이득 불명확 → 측정으로 게이트 |
| **유휴 워커 1ms CV 타임아웃 블로킹** | 외부 프로세스와 코어 경쟁 안 함, 스터터 완화 | 최대 1ms 깨우기 지연 | 순수 yield 스핀이 코어를 태워 스터터를 유발 → 측정 후 블로킹으로 전환 |
| **QPC 계층 스코프 프로파일러(자체)** | 의존성 0, DLL 경계 통제, JSON 캡처 자유 | Tracy만큼 화려하지 않음 | 자체 엔진이라 외부 프로파일러 통합 비용 회피. Tracy는 `WINTERS_PROFILING`일 때 plot으로 병행 |

### 근본 Trade-off — "왜 신입 1인 프로젝트에서 합리적인가"

핵심은 **"가장 비싼 부품(Fiber 전면 도입)을 먼저 사지 않은 것"**이다. Naughty Dog식 fiber 잡 시스템은 멋지지만, 그 가치는 "잡 안에서 잡을 기다리는 중첩 그래프"가 많을 때 나온다. 내 프레임에는 아직 그런 그래프가 거의 없다(스케줄러 배치도 발화 안 함). 그래서:
1. 먼저 **work-stealing + help-stealing**이라는 단순하고 검증 쉬운 모델로 병렬 경로를 깔고,
2. **프로파일러로 워커 유휴율을 재서**,
3. 그 수치가 임계(마스터플랜의 `Job::WorkerIdleRatio` 30%)를 넘을 때만 Fiber를 켠다.

이건 Karpathy 가드레일의 "Simplicity First / 측정 없는 추상화 금지"를 그대로 따른 결정이다. 면접에서 "왜 Fiber를 안 켰냐"는 약점이 아니라, **복잡도를 측정으로 게이트한 의사결정**으로 설명한다.

---

## 3. 실제 구현 (코드 근거)

### 3-1. WorkStealingDeque (Chase-Lev) — `WorkStealingDeque.h`

- 고정 `kCapacity = 4096`, power-of-two로 강제(`static_assert`)해 인덱싱을 `& (kCapacity-1)` 비트마스크로 처리(:18-19, :36).
- `alignas(64)` 로 `m_iBottom`/`m_iTop`을 다른 캐시라인에 배치(:91-92) → false sharing 방지.
- `Push`(:30-40): capacity 체크 → 버퍼 쓰기 → `release fence`(:37) → bottom 증가. owner 전용.
- `Pop`(:43-64): bottom 선감소 → `seq_cst fence`(:47) → top 로드. 마지막 1개일 때만 `compare_exchange_strong`(:58)로 도둑과 경합 해소.
- `Steal`(:67-81): top 로드 → `seq_cst fence` → bottom 로드 → `compare_exchange_weak`(:75)로 top 전진. 실패하면 false(다른 도둑/owner에 졌음).

### 3-2. CJobSystem — `JobSystem.h` / `JobSystem.cpp`

**자료구조** (`JobSystem.h:86-99`):
- `m_vecWorkers`: 워커 스레드 N개 (`hw_concurrency - 2`, 최소 1 — `JobSystem.cpp:49-50`).
- `m_vecDeques`: 워커당 deque 1개. `std::atomic + alignas(64)` 조합이 MSVC `construct_at` SFINAE에서 막혀 `unique_ptr` 래핑 후 `push_back`(`JobSystem.cpp:55-63`, 헤더 주석에도 명시).
- `m_GlobalQueue` + `m_GlobalMutex`: overflow / 비-워커 스레드 제출용 MPMC 글로벌 큐.
- `m_WakeCV` + `m_WakeMutex`: 유휴 워커 블로킹 대기용.
- `t_iWorkerIdx`(thread_local): 현재 스레드의 워커 인덱스, -1이면 비워커(Main/외부) — `JobSystem.cpp:18`.

**제출 경로** (`EnqueueJob`, `JobSystem.cpp:126-161`):
1. Shutdown 중이거나 N==0이면 즉시 인라인 실행(fallback).
2. 호출자가 워커 자신이면 자기 deque에 `Push`(Chase-Lev owner 경로) → `notify_one`.
3. 외부 스레드이거나 deque overflow면 글로벌 큐에 push → `notify_one`.

**워커 루프** (`WorkerLoop`, `JobSystem.cpp:164-196`):
- `t_iWorkerIdx` 세팅 → (FiberShell 모드면 `ConvertThreadToFiber`) → `TryExecuteOneJob` 반복. 잡 없으면 `m_WakeCV.wait_for(1ms)`로 블로킹(:185-186).

**잡 획득 우선순위** (`TryExecuteOneJob`, `JobSystem.cpp:212-249`):
1. 자기 deque `Pop`(LIFO) → 2. 글로벌 큐 → 3. 랜덤 victim에서 `Steal`. victim 선택은 thread_local xorshift PRNG(`PickVictim`, :198-210)로 self 회피.

**의존성/대기** (`WaitForCounter`, `JobSystem.cpp:306-360`):
- `pCounter->Load() > iTarget`인 동안 루프. **워커면** `TryExecuteOneJob`(자기 일 도움실행), **외부 스레드면** 글로벌 드레인 → round-robin steal. 둘 다 못 하면 `std::this_thread::yield()`(:357-358). **이게 help-stealing이고, fiber yield가 아니다** — 이 한계가 정직성 지도의 핵심.

**JobCounter** (`JobCounter.h`): `atomic<uint32_t>` 하나. `Increment`(submit 시), `Decrement`(잡 완료 시 `ExecuteItemInline`이 호출 — `JobSystem.cpp:264-270`). cv/mutex 의도적으로 제거하고 대기는 전부 `WaitForCounter`로 위임(헤더 주석 :12-13).

### 3-3. FiberShell — PoC(dead path)임을 코드로 인정

`TryExecuteItemOnFiber`(`JobSystem.cpp:272-292`)는 잡마다 `CreateFiber` → `SwitchToFiber` → `DeleteFiber`한다(풀 없음). `FiberShellEntry`(:294-303)가 잡 본문 실행 후 `hReturnFiber`로 복귀. 하지만:
- 기본 모드는 `ThreadOnly`(`FiberTypes.h:7`, `JobSystem.h:90` 멤버 초기값).
- `ExecuteItem`이 FiberShell 분기를 타려면 `GetExecutionMode()==FiberShell`이어야 하는데(`JobSystem.cpp:253-259`), 라이브에서 `SetExecutionMode(FiberShell)`를 호출하는 곳이 없다.
- `CFiberPool`(`FiberPool.h`)은 `vector<CFiber>` + `Reset`/`GetCount`만 있는 **빈 클래스**, `CFiber`(`Fiber.h`)는 핸들 getter/setter뿐.

→ 면접에서 **"Fiber 기반 잡 시스템"이라고 말하지 않는다.** "FiberShell 셸까지 박제했고 풀과 yield는 다음 단계"라고 정확히 긋는다.

### 3-4. SystemScheduler — access-conflict 병렬 배치 (그러나 미발화)

`RebuildExecutionPlan`(`SystemScheduler.cpp:45-83`): phase별로 시스템을 순회하며 각 시스템의 `DescribeAccess`(Read/Write 집합)를 보고, 현재 배치와 충돌하면 배치를 flush하고 새 배치 시작. **무충돌 시스템만 한 배치**로 묶는다.

`Execute`(:85-132): 배치 크기 ≥ `kMinParallelBatchSize`(=2)면 워커에 `Submit` + `WaitForCounter`로 병렬, 아니면 순차. 그리고 **매 프레임 `Scheduler::MaxBatchSize` 카운터를 내보낸다**(:131). 이 카운터가 실측 1 → 모든 배치가 시스템 1개라 병렬 분기를 한 번도 안 탄다.

### 3-5. 실제로 병렬이 도는 두 곳 — Transform / Navigation fan-out

- `TransformSystem::Execute`(`TransformSystem.cpp:19-67`): 루트 수가 `kParallelThreshold`(=16) 이상이면 8개씩(`kTransformRootsPerJob`) 슬라이스해 잡으로 분할(:49-61), `WaitForCounter`로 join. 미만이면 단일 스레드 fallback(:34-39). JobSystem 미주입 시에도 fallback.
- `NavigationSystem::Execute`(`NavigationSystem.cpp:77-96`): agent 수가 임계 이상이면 동일 패턴으로 fan-out.

→ "엔진 전체 병렬화"가 아니라 **임계값 기반 fan-out 두 곳 + fallback**이 정확한 표현.

### 3-6. CPU 프로파일러 — `CPUProfiler.cpp`

- `PushScope`/`PopScope`(:62-113): `thread_local t_vProfilerStack`에 QPC 틱+depth push, pop 시 elapsed 계산.
- **DLL 경계 버그 수정**: 같은 string literal이 DLL/번역단위마다 다른 주소를 가질 수 있어, 포인터 비교로 카운터를 합치면 같은 이름이 중복 행이 된다. `SameProfilerName`(:20-27)이 `a==b` 빠른 경로 후 `strcmp`로 보정.
- merge만 `m_Mutex`로 보호(스코프 스택은 thread_local이라 락 불필요). `EndFrame`(:45-60)에서 current↔last swap으로 더블버퍼.
- `WINTERS_PROFILING`이면 카운터를 `TracyPlot`으로 전송(:55-59).
- cap 상수: `PROFILER_MAX_COUNTERS_PER_FRAME=128` 등(`ProfilerTypes.h:4-7`). (gotcha: cap이 작으면 뒤쪽 카운터가 JSON에서 누락 — 32→128로 상향한 이력이 gotchas.md에 있음.)

### 3-7. GPU timestamp — `CDX11Device.cpp` (production)

- `CreateGpuTimingQueries`(:905-921): 슬롯마다 `D3D11_QUERY_TIMESTAMP_DISJOINT` 1개 + `TIMESTAMP` 2개(begin/end) 생성. `m_GpuTimingSlots`는 `kGpuTimingSlots`개 링버퍼.
- `BeginFrame`(:971-979): 쓰기 슬롯에 disjoint Begin + begin timestamp End.
- `EndFrame`(:992-1002): end timestamp + disjoint End, `bPending=true`, write index 링 전진.
- `ReadGpuTimingResults`(:923-951): `D3D11_ASYNC_GETDATA_DONOTFLUSH`로 **non-blocking** 회수. `disjoint.Disjoint`이거나 freq=0이거나 end≤begin이면 버림(:945). 통과하면 `(end-begin)*1e6/freq`를 마이크로초로 `GPU::FrameUs` 카운터로 내보냄(:948-949).

---

## 4. 검증 — 동작을 어떻게 증명했나

1. **매 프레임 프로파일러 카운터 = 자기 검증**: `Scheduler::ParallelBatches`, `Scheduler::SubmittedJobs`, `Scheduler::MaxBatchSize`(`SystemScheduler.cpp:128-131`), `Transform::SubmittedRootJobs`(`TransformSystem.cpp:65`)를 매 프레임 내보낸다. "병렬이 도는가"를 코드 주장이 아니라 **카운터 수치**로 확인한다. → 이 카운터가 `MaxBatchSize=1`을 보여줘 **스케줄러 배치 병렬이 미발화**임을 발견(마스터플랜 :22, :32에 기록).
2. **GPU vs CPU 대조**: `GPU::FrameUs` 카운터와 CPU 프레임 시간을 대조해 **"작은 씬임에도 완전 CPU 바운드"**를 확정(드로우콜 ~94, 마스터플랜 :22).
3. **JSON 캡처(F4) + 리미터 토글(F11)**: 버전드 JSON 스키마로 프레임 측정을 아카이브해 재현 가능. "수치를 외워 말하지 않고 F4로 캡처해서 보여줄 수 있다"가 방어선.
4. **DLL 경계 카운터 중복 버그**: 포인터 비교 → strcmp 수정 후 카운터가 단일 행으로 합쳐지는 것으로 검증(`CPUProfiler.cpp:20-27`).

**아직 없는 검증(정직)**: 1만 job 스트레스/fiber leak 테스트는 `FIBER_JOBSYSTEM_STABILIZATION_PLAN.md`의 M0(`Tools/WintersJobSystemStress`)에 **설계만 있고 코드는 없다**. 골든/자동 게이트가 아니라 인게임 카운터 + 수동 캡처 수준이다.

---

## 5. 최적화

### 실제로 한 것 (근거 있음)
1. **유휴 워커 yield 스핀 → 1ms CV 타임아웃 블로킹**(`JobSystem.cpp:182-187`): 순수 `std::this_thread::yield()` 스핀이 외부 프로세스와 코어를 경쟁해 스터터를 유발. 잡 없을 때 실제로 잠들게 바꿈. 타임아웃 1ms라 steal 전용 깨우기 누락도 1ms 안에 회복.
2. **Chase-Lev 빠른 경로 + `alignas(64)`**: owner Push/Pop을 대부분 relaxed로 처리, top/bottom false sharing 제거.
3. **프로파일러 DLL 경계 카운터 수정**: 중복 행 제거(strcmp 보정).
4. **임계값 기반 fan-out**: `kParallelThreshold`(Transform 16, Nav), `kTransformRootsPerJob=8`로 잡 분할 오버헤드가 병렬 이득을 넘지 않게 임계 설정.

### 정량 수치 (정직성 지도 한정)
- Scheduler 0.63ms(EMA 1.67ms), `ParallelBatches=0`, `MaxBatchSize=1`(마스터플랜 :22). 프레임 9.5ms / 드로우콜 ~94 / 완전 CPU 바운드.
- **300~650 FPS는 이론 추정치, Phase 1~7 코드 미반영**(마스터플랜 :45-48이 스스로 명시). "달성했다"고 말하지 않는다.

### 계획 중 (측정 예정)
- Scheduler `MaxBatchSize≥3`으로 끌어올리기: `DescribeAccess`의 Write 선언이 과대해 무충돌 시스템이 같은 배치로 안 묶이는지 점검(마스터플랜 Phase 2, :102-103).
- 워커 affinity/priority(P/E 코어 분리), AnimUpdate/Vision 잡화(:98-101).

---

## 6. 구현 예정 (Planned) — 동일한 깊이로

### 6-1. Fiber 풀 + WaitForCounter yield화 (Phase 5)

**무엇을**: 잡마다 `CreateFiber`/`DeleteFiber`하는 FiberShell을 **사전 할당 fiber 풀**(per-worker)로 교체하고, `WaitForCounter`를 **진짜 fiber yield**로 바꾼다.

**왜**: 현재 help-stealing은 "기다리는 동안 다른 잡을 처리"하지만, 잡 안에서 잡을 기다리는 중첩이 깊어지면 콜스택이 쌓이고 워커가 막힌다. fiber면 대기 잡을 리스트에 걸고 워커가 즉시 다음 잡으로 넘어가 **워커 점유율을 극대화**한다.

**어떻게** (`FIBER_JOBSYSTEM_STABILIZATION_PLAN.md` M1 설계):
- **자료구조**: `FiberContext`(fn/pCounter/state/hReturnFiber/owner를 **직접 소유** — caller stack의 `WorkItem&` 참조 금지, 그게 F-8 stale 버그), `eFiberState{Idle,Running,Waiting,Finished}` 상태머신.
- **per-worker pool**: 각 워커가 자기 free list만 acquire/release(Win32 fiber affinity 위반 회피, F-5). `vector<vector<unique_ptr<FiberContext>>>`로 atomic+alignas SFINAE 우회(F-9).
- **경계 helper**: `thread_local`은 fiber-local이 아니므로 root↔fiber 전환마다 `Set_FiberContext`/`Clear_FiberContext`로 슬롯을 명시 갱신(F-10, F-19).
- **단계**: M0(스트레스/leak 하니스) → M1(풀 본체) → M2(yield) → M3(per-slot 버퍼).

**예상 Trade-off**:
- 위험: fiber A가 `WaitForCounter` 안에서 fiber B를 직접 fire하면 B의 `hReturnFiber`가 root라 A가 중간에 버려져 영구 stall(F-16). → 회피: fiber 컨텍스트면 즉시 Waiting + root yield, 도움실행은 root만.
- 위험: leak / 락 보유 중 yield 데드락. → 회피: acquire/release 짝 + same-thread 강제 assert.

**어떻게 검증**: `Tools/WintersJobSystemStress`로 (a) 1만 job fan-out 합 검증(`sum==49,995,000`), (b) nested wait(1K parent × 4 child) 검증, (c) **leak 검증 순서 분리** — shutdown 전 `acquired==released`, shutdown 후 `created==deleted`(F-18). CRT `_CrtDumpMemoryLeaks` + Application Verifier 0 위반.

**판단 게이트(중요)**: Phase 2 완료 후 `Job::WorkerIdleRatio`가 **30% 이상일 때만** 진행(마스터플랜 :149). 아니면 보류 — 복잡도 대비 이득이 없으면 안 한다.

### 6-2. Scheduler 배치 병렬 활성화 (Phase 2)

**무엇을**: `MaxBatchSize=1`의 원인을 분해해 무충돌 시스템을 같은 배치로 묶어 `MaxBatchSize≥3`, `ParallelBatches>0`으로 만든다.

**왜**: 병렬 인프라는 다 있는데 배치가 전부 크기 1이라 한 번도 발화하지 않는다. 원인은 (a) 각 시스템 `DescribeAccess`의 Write 선언이 과대(예: Transform이 컴포넌트 전체를 Write로 선언)해 거의 모든 시스템이 서로 충돌로 판정되거나, (b) phase 분리가 과해 한 phase에 시스템이 1개씩만 있는 것.

**어떻게**: Read/Write 선언을 컴포넌트 단위로 정밀화 → access-conflict 판정이 풀려 무충돌 시스템(Status/Vision/스킬 Execute류)이 한 배치로 묶임. **검증**: `Scheduler::ParallelBatches>0`, `MaxBatchSize≥3` 카운터로 확인.

**Trade-off**: 선언을 너무 좁히면 실제 충돌을 놓쳐 데이터 레이스. → 결정론 SimLab 골든 해시로 회귀 확인.

### 6-3. 워커 affinity / 대기 전략 고도화 (Phase 2)

P-코어/E-코어 분리 + `SetThreadAffinityMask`/`SetThreadPriority`, 유휴 워커를 1ms 타임아웃 대신 **잡 제출 시 정확히 wake**하는 이벤트 기반으로. **검증**: 스터터 캡처(F4)에서 프레임 타임 분산 감소.

---

## 7. 면접 예상 질문 & 모범 답변

**Q1 (기본). Work-stealing deque가 뭐고 왜 일반 큐 대신 쓰나요?**
A. 각 워커가 자기 전용 deque를 가지고, owner는 한쪽 끝(bottom)에서 LIFO로 push/pop, 도둑은 반대 끝(top)에서 FIFO로 steal합니다. owner의 Push/Pop은 대부분 relaxed atomic으로 끝나 빠르고, 도둑과 부딪히는 건 deque에 1개 남은 순간뿐이라 경합이 최소화됩니다. 글로벌 단일 큐는 모든 워커가 한 락에 몰려 코어가 늘수록 나빠지죠. 저는 deque를 메인으로 쓰고, overflow와 외부 스레드 제출만 글로벌 mutex 큐로 받는 하이브리드입니다(`JobSystem.cpp:146-160`).

**Q2 (기본). JobCounter로 의존성을 어떻게 표현하나요?**
A. atomic 카운터 하나입니다. `Submit(job, &counter)`마다 `Increment`, 잡 완료 시 `Decrement`(`ExecuteItemInline`), `WaitForCounter(&counter, 0)`로 0이 될 때까지 대기합니다. fork-join 한 패턴만 필요해서 cv/mutex를 의도적으로 뺐고, 대기 동안 노는 걸 막으려고 카운터 대신 WaitForCounter가 help-stealing을 합니다.

**Q3 (설계). `alignas(64)`와 메모리 fence가 왜 필요한가요?**
A. `alignas(64)`는 top/bottom을 다른 캐시라인에 둬서, owner가 bottom만 써도 도둑 캐시의 top 라인이 무효화되는 false sharing을 막습니다. fence는 재배열 방지입니다 — Push에서 버퍼에 값을 쓰기 *전에* bottom 증가가 보이면 도둑이 쓰레기를 읽으니 release fence로 순서를 강제하고(`WorkStealingDeque.h:37`), 마지막 1개 경합은 seq_cst로 owner의 bottom 감소와 도둑의 top 증가에 전역 순서를 줘 double-pop을 막습니다(:47, :58, :75).

**Q4 (설계). WaitForCounter에서 스레드를 그냥 재우면 안 되나요?**
A. 재우면 그 코어가 놉니다. 그래서 두 단계로 갑니다. 워커 루프에서 잡이 *아예* 없을 땐 1ms CV 타임아웃으로 블로킹해 외부 프로세스와 코어 경쟁을 안 합니다. 하지만 **잡을 기다리는 중(WaitForCounter)**엔 안 잡니다 — 기다리는 동안 `TryExecuteOneJob`으로 다른 잡을 처리하는 help-stealing을 합니다(`JobSystem.cpp:315-318`). 못 하면 그제야 yield하고요.

**Q5 (압박/adversarial). "Fiber 기반 잡 시스템"이라고 하셨는데, 실제로 Fiber로 도나요?**
A. 아니요, 정확히 긋겠습니다. 기본 모드는 `ThreadOnly`이고, 라이브에서 FiberShell을 켜는 호출이 없습니다. FiberShell은 잡마다 `CreateFiber`/`DeleteFiber`하는 **셸까지만 박제**했고(`JobSystem.cpp:272-303`), `CFiberPool`은 사실상 빈 클래스입니다. 그래서 저는 "Fiber 기반"이라 말하지 않고 "**실가동은 스레드 풀 + work-stealing, Fiber는 PoC/설계**"라고 합니다. 풀과 yield화는 `FIBER_JOBSYSTEM_STABILIZATION_PLAN.md`에 자료구조·안전규칙·검증까지 설계해뒀고, **Phase 2 후 워커 유휴율 30% 이상일 때만** 켜는 게이트를 둔 이유는 중첩 wait 패턴이 적어 지금은 복잡도 대비 이득이 불명확하기 때문입니다.

**Q6 (압박/adversarial). "엔진을 병렬화했다"는데, 병렬이 실제로 도나요?**
A. 부분적입니다. 그리고 그걸 제가 측정으로 발견했습니다. SystemScheduler에 access-conflict 기반 배치 병렬을 만들었는데, 매 프레임 내보내는 `Scheduler::MaxBatchSize` 카운터가 **1**입니다 — 모든 배치가 시스템 1개라 병렬 분기를 한 번도 안 탑니다(`SystemScheduler.cpp:101-131`). 실제로 병렬이 도는 건 TransformSystem과 NavigationSystem의 임계값 기반 fan-out 두 곳뿐입니다(`TransformSystem.cpp:49-63`). 원인은 시스템들의 Write 접근 선언이 과대해 거의 다 충돌로 판정되는 거라 보고, Read/Write 선언 정밀화로 `MaxBatchSize≥3`을 만드는 게 다음 작업입니다.

**Q7 (압박/adversarial). 1만 job 스트레스 테스트나 fiber leak 테스트는 돌려보셨나요?**
A. 코드로는 아직 없습니다. `Tools/WintersJobSystemStress`로 1만 job fan-out 합 검증, nested wait, leak 검증(shutdown 전 `acquired==released`, 후 `created==deleted`)을 **설계**했지만 박제는 안 했습니다. 지금 검증은 인게임 프로파일러 카운터 + F4 JSON 캡처 수준입니다. Fiber 풀을 켤 때 이 하니스가 선행 게이트(M0)라 먼저 만들 계획이고, leak 검증 순서를 분리한 이유도 — main이 WaitForCounter 안에서 inline 실행하면 `acquired<jobCount`가 정상이고, shutdown 전엔 `created>deleted`가 정상이라 — false failure를 피하기 위해서입니다.

**Q8 (심화). GPU 시간을 어떻게 stall 없이 재나요?**
A. GPU는 비동기라 결과를 바로 물으면 파이프라인이 멈춥니다. 그래서 4슬롯 링버퍼로 **N프레임 전 쿼리를 non-blocking 회수**합니다. BeginFrame에서 disjoint Begin + timestamp, EndFrame에서 timestamp + disjoint End 후 `bPending`을 켜고 write index를 링 전진합니다(`CDX11Device.cpp:971-1002`). 회수는 `D3D11_ASYNC_GETDATA_DONOTFLUSH`로 안 막고 시도하다가, disjoint 쿼리가 그 구간 GPU 클럭이 흔들렸다고 하면(`disjoint.Disjoint`) 그 측정을 버립니다(:945). 통과한 것만 `(end-begin)*1e6/freq`로 마이크로초 환산합니다.

**Q9 (심화). 프로파일러 카운터가 중복으로 보이던 버그를 어떻게 잡았나요?**
A. DLL 경계에서 같은 string literal이 번역단위/DLL마다 다른 주소를 가질 수 있습니다. 카운터를 이름 포인터 비교로 합쳤더니 같은 이름이 두 행으로 갈라졌습니다. `SameProfilerName`에서 `a==b` 빠른 경로 후 `strcmp` 폴백으로 보정했습니다(`CPUProfiler.cpp:20-27`). 추가로 `PROFILER_MAX_COUNTERS_PER_FRAME` cap이 작으면 뒤쪽 카운터가 JSON에서 누락돼 "발생 안 함"으로 오판할 수 있어 cap을 32→128로 올린 이력도 있습니다.

**Q10 (심화). Chase-Lev Pop의 "마지막 1개" 경합을 코드로 설명해보세요.**
A. `Pop`은 먼저 bottom을 선감소하고 seq_cst fence 후 top을 읽습니다(`WorkStealingDeque.h:45-48`). `t>b`면 비었으니 bottom 복구하고 false. `t<b`면 도둑과 안 겹치니 그냥 가져갑니다. `t==b`가 마지막 1개라 도둑과 정확히 겹치는데, 여기서 `compare_exchange_strong(t, t+1)`로 top을 선점 시도합니다(:58). 성공하면 내가 가져간 거고, 실패하면 도둑이 가져간 거라 false. 어느 쪽이든 bottom을 t+1로 정규화해 빈 상태로 만듭니다. Steal도 같은 top을 `compare_exchange_weak`로 노려서, 둘 중 하나만 이깁니다.

**Q11 (심화). 왜 deque를 `unique_ptr`로 래핑했나요?**
A. `CWorkStealingDeque`가 `std::atomic` 멤버에 `alignas(64)`를 걸었는데, 이 조합이 MSVC의 `std::construct_at` SFINAE에서 막혀 `vector<Deque>(N)` 직접 생성이 안 됐습니다. 그래서 힙에 `make_unique`로 만들고 `vector<unique_ptr<Deque>>`에 push_back합니다(`JobSystem.cpp:55-63`). 컴파일러 제약을 우회한 거고 헤더 주석에도 이유를 남겨놨습니다.

**Q12 (확장). 코어를 32개로 늘리면 이 시스템이 그만큼 빨라지나요?**
A. 지금은 아닙니다. 현재 워크로드가 완전 CPU 바운드인데 병렬이 fan-out 두 곳뿐이라, 코어를 늘려도 노는 워커만 늘어납니다. 그래서 순서가 (1) Scheduler 배치 병렬 활성화로 시스템 단위 병렬 폭을 넓히고, (2) AnimUpdate/Vision을 잡화하고, (3) 그래도 워커 유휴율이 높으면 Fiber로 점유율을 끌어올리는 겁니다. 코어 확장의 이득은 "병렬 폭"이 먼저 넓어진 뒤에 나옵니다 — 그 폭을 측정으로 게이트하는 게 제 마스터플랜 Phase 2~5입니다.

---

## 8. 30초 엘리베이터 피치

제 JobSystem은 Chase-Lev work-stealing deque에 글로벌 큐를 섞은 하이브리드입니다. 워커마다 전용 deque를 줘서 owner는 락 없이 빠르게 push/pop하고, 노는 워커가 남의 일을 훔쳐 알아서 부하를 분산합니다. 의존성은 atomic 카운터로, 대기는 스레드를 재우는 대신 help-stealing으로 처리하고요. 그런데 제가 자랑하고 싶은 건 기능이 아니라 **측정 루프**입니다. 매 프레임 프로파일러로 병렬이 실제로 도는지를 재는데, 그 카운터가 "스케줄러 배치 병렬이 한 번도 발화 안 한다(MaxBatchSize=1)"고 제 코드의 한계를 스스로 폭로했습니다. 그래서 저는 Fiber 같은 비싼 부품을 먼저 사지 않고, 워커 유휴율이 임계를 넘을 때만 켜는 게이트를 뒀습니다. 측정으로 문제를 정의하고, 복잡도를 데이터로 정당화하는 — 그게 이 도메인입니다.
