# 04. JobSystem·병렬처리 — 박사 연구 심화

> **상태 동기화 (2026-07-11 — RESEARCH ROADMAP)**: Client JobSystem은 active ThreadOnly이고 과거 Main-push owner race 경로는 수정됐다. FiberShell은 dormant prototype이며 FiberFull·Server 결정론 병렬 runtime·전용 stress는 아직 연구/구현 목표다. 아래 미래 시스템을 작동하는 결과로 인용하지 말고 [2026-07-11 상태 감사](../plan/2026-07-11_JOB_SYSTEM_CHASE_LEV_FIBER_STATE_AUDIT.md)를 현재 기준으로 삼는다.
>
> 전제 문서: [`00_PHD_Paper_Guide.md`](00_PHD_Paper_Guide.md). 본 문서는 가이드 §1(구현 vs 기여), §3(thesis statement), §4(구조), §7(평가: 약/강 scaling, core 수 대비 throughput, 결정론 재현률, 메모리)을 **그대로 전제**한다. 모든 세부주제마다 "이건 구현 항목인가, 기여 후보인가?"를 가이드 §1로 되돌아가 묻는다.
>
> 독자: LoL 스타일 MOBA + 오픈월드 엔진 'Winters'를 만든 숙련 C++ 엔진 개발자. 현재 Naughty Dog 스타일 fiber 기반 **서버** 잡 시스템을 설계 중이며, 서버는 lockstep/replay 결정론 권위를 가진다.
>
> Top venue: **PPoPP**, SOSP, OSDI, EuroSys, ASPLOS (저널 TOPC/TOCS). 산업 참고(비학술): CppCon, GDC.

---

## 0. 이 분야를 박사로 본다는 것 (구현 vs 기여 + top venue 표)

병렬처리는 "엔진 박사"가 가장 흔히 **구현 자랑으로 미끄러지는** 분야다. "나는 work-stealing 스케줄러와 fiber job system을 만들었다"는 **석사 프로젝트**다. Chase-Lev deque, fiber 컨텍스트 스위치, ECS, SoA 변환은 전부 **이미 존재하는 기법**이며, 코드로 작동시키는 것은 엔지니어링이다. 박사는 이 위에서 **"기존 설계로는 불가능했던 무엇을, 정량적으로 얼마나 더, 어떤 제약 하에서"**를 증명한다.

이 분야에서 박사 기여가 살아나는 지점은 가이드 §5의 기여 유형 중 **2(새 자료구조: 더 나은 동시성/메모리), 3(새 시스템/아키텍처: 불가능하던 조합), 4(새 이론/증명: 메모리 모델 정합성·결정론 보존 증명)**이며, 게임 서버 맥락에서는 이 셋이 하나의 긴장으로 수렴한다.

> **이 문서를 관통하는 연구 긴장(research tension):**
> 멀티코어 work-stealing은 **본질적으로 비결정적 실행 순서(non-deterministic schedule)**를 낳는다. 어떤 스레드가 어떤 job을 언제 훔치는지는 OS 스케줄링·캐시·경합에 의존한다. 그런데 게임 서버의 lockstep/replay는 **비트 단위 결정론(bit-exact determinism)**을 요구한다. 같은 입력 → 같은 상태 해시 → cross-platform 재현. **"병렬 + 결정론 동시 달성"**은 단순 구현이 아니라 깊은 명제다. 이것이 본 문서의 핵심 박사 각도다.

### 0.1 구현 vs 기여 — 4개 세부주제 대조

| 세부주제 | 구현 (석사/산업) | 연구 기여 (박사) |
|---|---|---|
| Work-Stealing / Chase-Lev | "lock-free deque를 만들어 worker가 훔치게 했다" | "약한 메모리 모델 위에서 정합성을 **기계검증(model checking)** 으로 증명하면서, NUMA toplogy-aware victim 선택으로 96-core에서 throughput을 X% 개선" |
| Fiber Job System | "stackful fiber로 wait 시 thread가 안 막히게 했다" | "fiber 스케줄을 **재현 가능(replayable)** 하게 만드는 record/replay 레이어가, 결정론 시뮬을 멀티코어에서 비트단위 재현하면서 오버헤드 Y% 이내" |
| ECS | "archetype/sparse-set 저장으로 컴포넌트를 배치했다" | "structural change(엔티티 생성/컴포넌트 추가)를 **동시에 안전**하게 적용하면서 쿼리 반복 순서를 결정론적으로 유지하는 deferred-commit 모델 + 그 직렬화 가능성(serializability) 증명" |
| DOD | "AoS를 SoA로 바꿔 캐시 미스를 줄였다" | "데이터 레이아웃을 워크로드 접근 패턴에서 **자동 도출**하는 컴파일러 협력 변환과, 그 변환이 hot loop의 bandwidth-bound 구간에서 z 배 개선함을 증명" |

### 0.2 Top Venue 표 (병렬/시스템)

| 구분 | Venue | 성격 | 이 분야 대표 채택 주제 |
|---|---|---|---|
| 병렬 알고리즘/스케줄링 | **PPoPP** (Principles and Practice of Parallel Programming) | work-stealing, lock-free 자료구조, 메모리 모델 | Chase-Lev 후속, 정합성 증명, scheduling |
| 시스템 아키텍처 | **SOSP / OSDI** | OS·런타임·결정론적 실행 | deterministic multithreading (DMP, Kendo) |
| 유럽 시스템 | **EuroSys** | 런타임·동시성·재현성 | record/replay, deterministic execution |
| 아키텍처 협력 | **ASPLOS** | HW/SW 경계, 메모리 레이아웃, 데이터 지향 | data layout, prefetch, cache 협력 |
| 저널 | **ACM TOPC, TOCS** | 확장판/증명 | — |
| 산업(비학술, 인용용) | CppCon, GDC | 사례·영향력 근거 | Naughty Dog fiber(Gyrling 2015), Acton DOD(2014) |

> **주의(가이드 §9):** Gyrling(GDC 2015)·Acton(CppCon 2014)은 **권위 있으나 동료심사 학술 출판이 아니다.** 박사 기여의 1차 무대는 PPoPP/OSDI이고, GDC/CppCon은 "산업에서 이 문제가 실재함"의 근거(motivation)와 사례로 인용한다.

### 0.3 Heilmeier 체크 (이 분야 적용)

가이드 §6의 Heilmeier Catechism을 본 분야에 박으면 좋은 질문이 걸러진다:
- **무엇을?** "멀티코어에서 빠르면서도 매 실행이 비트단위로 똑같은 게임 서버 시뮬"
- **지금 한계?** 빠른 것(work-stealing)은 비결정적, 결정적인 것(single-thread lockstep)은 안 빠르다. 둘을 동시에 가진 시스템이 없다.
- **새로움?** 스케줄 비결정성을 **결과 결정성과 분리**하는 레이어 (실행 순서는 자유, 관찰 가능 상태는 고정).
- **누가 신경?** MOBA/RTS/격투 게임 서버, replay·anti-cheat·e스포츠 검증, 분산 시뮬.
- **어떻게 측정?** 결정론 재현률(N회 실행 상태 해시 일치 비율), core 수 대비 strong/weak scaling, 결정론 레이어의 오버헤드(%).

---

## 1. Work-Stealing 스케줄러와 Chase-Lev Deque

### 1.1 핵심 원리

**문제:** P개의 worker가 동적으로 생성되는 task를 어떻게 부하 균형(load balancing)하면서, 중앙 큐 경합 없이 나눠 가질 것인가.

**Work-stealing의 답:** 각 worker가 **자기 deque**를 갖는다. 자기 deque의 **bottom**에서 LIFO로 push/pop(캐시 친화적 — 방금 만든 task가 캐시에 뜨겁다). 일이 떨어진 worker는 다른 worker deque의 **top**에서 FIFO로 steal(훔친 task는 보통 큰 subtree의 뿌리 → 한 번 훔치면 오래 일함).

이 LIFO-local / FIFO-steal 구조가 work-stealing의 핵심 통찰이다:
- **owner의 push/pop은 거의 항상 비경합(uncontended)** → atomic 1~2개로 충분, 빠르다.
- **steal만 경합** → 경합이 deque의 반대쪽 끝(top)에서 일어나 owner의 hot path와 캐시 라인이 분리된다.
- 이론적으로 Blumofe & Leiserson은 fully-strict 계산에서 **기대 실행 시간 T_P ≤ T_1/P + O(T_∞)**, steal 횟수 기대값 **O(P·T_∞)** 를 증명했다(T_1 = 총 일, T_∞ = critical path, span). 즉 **work는 선형 분배, steal은 span에 비례**해서만 발생한다.

```text
worker 0 deque:  [top] ... A B C D [bottom]
                   ↑                  ↑
            thief steal          owner push/pop
            (FIFO, 경합)        (LIFO, 비경합)
```

### 1.2 대표 기존 연구

- **Blumofe & Leiserson (1999), "Scheduling Multithreaded Computations by Work Stealing"** (JACM). Cilk의 이론적 토대. randomized work stealing의 시간/공간 bound 증명. **이 분야의 출발점.**
- **Frigo, Leiserson & Randall (1998), "The Implementation of the Cilk-5 Multithreaded Language"** (PLDI). work-first 원칙, THE protocol(deque의 owner-thief 동기화 프로토콜의 원형).
- **Chase & Lev (2005), "Dynamic Circular Work-Stealing Deque"** (SPAA). **고정 크기 한계를 푼 핵심 논문.** circular array + 동적 확장(grow)으로 무한 용량처럼 동작하는 lock-free deque. 오늘날 거의 모든 엔진 job system(Winters 포함)의 deque가 이 논문 변형이다.
- **Lê, Pop, Cohen & Zappa Nardelli (2013), "Correct and Efficient Work-Stealing for Weak Memory Models"** (PPoPP). **본 절에서 가장 중요한 박사급 논문.** Chase-Lev 알고리즘을 C11/ARM/POWER 같은 **약한 메모리 모델**에서 정확히 동작하도록 메모리 순서(memory ordering)를 **최소한으로** 배치하고, 그 정합성을 형식적으로 논증. seq_cst fence가 **어디에 왜 꼭 필요한지**를 규명.
- **Morrison & Afek (2014), "Fence-Free Work Stealing on Bounded TSO Processors"** (ASPLOS). x86 TSO에서 비싼 full fence(`mfence`)를 제거하는 기법. 전력/지연 관점.

### 1.3 자료구조/알고리즘 (의사코드)

핵심은 **owner(Push/Pop)와 thief(Steal)가 마지막 한 개의 원소를 두고 경합**하는 지점이다. 여기서 메모리 순서가 틀리면 (a) 같은 task를 둘이 실행하거나 (b) task를 잃어버린다.

Chase-Lev의 Lê et al.(2013) 약한 메모리 모델 버전 (C11 atomics):

```text
// 공유 상태: top(thief가 CAS로 전진), bottom(owner만 store), buf[](circular array)
// 핵심 불변식: top <= bottom. 원소 개수 = bottom - top.

PushBottom(v):                          // owner 전용
    b = bottom.load(relaxed)
    t = top.load(acquire)               // 풀 검사용 — thief의 진행을 본다
    if b - t >= capacity: grow()        // Chase-Lev: 동적 확장
    buf[b % cap] = v
    atomic_thread_fence(release)        // (1) buf write가 bottom 증가보다 먼저 보이게
    bottom.store(b + 1, relaxed)        //     thief가 bottom 증가를 보면 buf[b]는 이미 valid

Steal():                                // thief 전용
    t = top.load(acquire)
    atomic_thread_fence(seq_cst)        // (2) top load와 bottom load 사이 전역 순서 강제
    b = bottom.load(acquire)
    if t >= b: return EMPTY             // 비어있음
    v = buf[t % cap]                    // load는 CAS '전에' (Lê et al. 핵심)
    if !top.CAS(t, t+1, seq_cst, relaxed): return ABORT  // 다른 thief/owner가 먼저 가져감
    return v

PopBottom():                            // owner 전용
    b = bottom.load(relaxed) - 1
    bottom.store(b, relaxed)
    atomic_thread_fence(seq_cst)        // (3) bottom 감소와 top load 사이 — 가장 미묘한 fence
    t = top.load(relaxed)
    if t > b:                           // 비어있었음
        bottom.store(t, relaxed)        // bottom 복원
        return EMPTY
    v = buf[b % cap]
    if t != b: return v                 // 둘 이상 — thief와 충돌 불가, 그냥 반환
    // 마지막 1개 — Steal과 경합. CAS로 소유권 다툼.
    if !top.CAS(t, t+1, seq_cst, relaxed): v = EMPTY   // thief가 이김
    bottom.store(t + 1, relaxed)
    return v
```

**왜 각 메모리 순서인가 (대학원 수준 설명):**
- **(1) release fence (Push):** owner가 `buf[b]`에 값을 쓴 뒤 `bottom`을 증가시킨다. 이 둘이 재배열되면, thief가 증가한 bottom을 보고 `buf[b]`를 읽었을 때 **아직 안 쓰인 쓰레기**를 읽는다. release fence가 "buf write happens-before bottom 증가"를 보장한다. thief의 `bottom.load(acquire)`와 짝을 이룬다.
- **(2)(3) seq_cst fence:** Pop과 Steal이 **둘 다 마지막 원소**를 노릴 때, "owner가 bottom을 줄이는 것"과 "thief가 top을 올리는 것"이 **전역적으로 단일 순서(single total order)** 로 합의되어야 한다. acquire/release만으로는 두 atomic 변수(top, bottom)에 걸친 순서를 보장 못 한다 — **이것이 work-stealing deque가 seq_cst를 못 버리는 근본 이유**다. Lê et al.(2013)의 핵심 결과: Pop과 Steal 각각의 경합 경로에 **딱 하나의 seq_cst fence**면 충분하고 필요하다.
- **ABA 회피:** top은 **단조 증가(monotonic)** 한다(절대 감소 안 함). 그래서 CAS의 고전적 ABA 문제(값이 A→B→A로 돌아와 CAS가 잘못 성공)가 **구조적으로 발생하지 않는다.** index를 재사용하지 않는 설계가 ABA를 푼다 — 이게 Chase-Lev의 우아함이다.

### 1.4 박사급 novel 각도 (open problems)

1. **약한 메모리 모델 정합성의 기계검증(machine-checked proof).** Lê et al.은 손논증(hand proof) 중심. 기여: CDSChecker/GenMC/Coq로 Chase-Lev 변형(특히 grow가 있는 동적 버전)의 정합성을 **자동 검증**하고, 엔진들이 실제로 쓰는 "최적화된" 변형들(Winters처럼 fence를 줄인 버전)에 **숨은 버그가 있는지** 체계적으로 탐사. → PPoPP/CAV.
2. **NUMA / 대규모 코어(96~256 core) 확장성.** 단순 random victim 선택은 NUMA에서 cross-socket steal로 지연이 폭증. 기여: **topology-aware victim 선택 + steal 지역성(socket-local 먼저)** 정책의 설계와, 그것이 strong scaling 곡선을 펴는 정도를 정량화. open: steal 지역성 ↔ 부하 균형의 trade-off 최적점.
3. **전력 효율(energy-aware work stealing).** busy-wait steal은 전력을 태운다. 기여: "일이 없을 때 얼마나 오래 훔치다 잠들 것인가"의 정책을 **에너지-지연 Pareto front**로 정식화 (Winters의 `WaitForCounter` busy-wait이 정확히 이 문제에 있음).
4. **결정론적 스케줄(deterministic work stealing).** 본 문서의 중심 긴장. steal은 본질적으로 비결정적인데, 게임 서버는 결정론을 원한다. 기여: 실행 **순서**는 비결정적으로 두되 **결과**(관찰 가능 상태)는 결정적이게 만드는 스케줄러 계약(§2.4에서 fiber와 결합).

### 1.5 Thesis statement 예시

> "Chase-Lev work-stealing deque의 victim 선택을 **NUMA 토폴로지와 steal-지역성**에 맞춰 재설계하고 그 메모리 모델 정합성을 기계검증하면, randomized work stealing 대비 **96-core에서 throughput을 X% 향상**시키면서 cross-socket steal을 Y% 감소시키고, **정합성 위반 0**을 형식적으로 보장한다."

(가이드 §3 형식: 새 구조 X + 제약 C[약한 메모리 모델, NUMA] + 목표 Y[scaling] + 정량 개선 Z + falsifiable.)

### 1.6 평가 방법

- **Throughput / scaling:** task-parallel 벤치(BOTS, Cilk 스타일 fib/nqueens/cholesky) + 실제 게임 틱 워크로드. **strong scaling**(고정 문제, core ↑)과 **weak scaling**(core당 일 고정, core·문제 동반 ↑) 곡선 둘 다. baseline: TBB, Cilk Plus/OpenCilk, Intel/MS PPL, naive 중앙 큐.
- **Steal 통계:** steal 시도/성공률, cross-socket steal 비율, deque 점유율. ablation으로 victim 정책 각 요소 분리.
- **정합성:** model checker(GenMC/CDSChecker)로 메모리 모델별(C11/ARMv8/POWER) **위반 0** 증명. 이게 "구현"과 "기여"를 가른다.
- **메모리:** deque grow 빈도, peak 메모리, false-sharing(perf c2c).
- **통계 엄밀성(가이드 §7):** 여러 시드·머신, 신뢰구간. 평균만 제시 금지.

### 1.7 Winters 연결점

Winters는 **이미 실제 Chase-Lev deque를 구현**해 두었다 — 이게 testbed로서 강력한 출발점이다.

- [`Engine/Public/Core/JobSystem/WorkStealingDeque.h`](../../Engine/Public/Core/JobSystem/WorkStealingDeque.h) — `CWorkStealingDeque<T>`, 헤더 주석이 명시적으로 "(Chase & Lev 2005)"를 표방. 고정 4096 슬롯(grow 없는 MVP), `alignas(64)`로 top/bottom **false-sharing 방지**(L91-92).
- **§1.3 의사코드와 1:1 대응이 실재한다:** `Push`의 release fence(L37), `Pop`의 마지막-원소 `compare_exchange_strong(seq_cst)`(L58-61), `Steal`의 seq_cst fence + `compare_exchange_weak(seq_cst)`(L70-80). **즉 §1.3에서 설명한 (1)(2)(3) fence가 그대로 코드에 박혀 있다.** Lê et al.(2013) 분석을 이 파일에 직접 적용해 "이 fence가 정말 필요/충분한가"를 model checker로 검증하는 것이 곧 §1.4-(1) 기여의 testbed.
- [`Engine/Public/Core/JobSystem.h`](../../Engine/Public/Core/JobSystem.h) / `JobSystem.cpp` — `CJobSystem`. 현재 Submit은 **worker→자기 deque, main/외부/overflow→mutex global queue** 하이브리드이고 `WaitForCounter`는 help-stealing이다. `PickVictim`은 현재 victim 선택 지점, `m_GlobalQueue`는 중앙 경합 측정 대상이다.
- **역사적 race(연구 소재):** 2026-04-23에는 Main이 non-owner worker deque의 bottom에 Push해 Chase-Lev single-owner 불변식을 깼다. 현재는 `EnqueueJob` 경로 분리로 해당 접근을 차단했다. 연구 과제는 “미해결 race 수정”이 아니라 이 과거 사고를 최소 재현하고, fixed-ring/non-trivial payload를 포함한 현재 변형 전체를 stress/model checking으로 검증하는 것이다.

---

## 2. Fiber 기반 Job System (stackful coroutine)

### 2.1 핵심 원리

**문제:** job 안에서 다른 job들을 spawn하고 **그 완료를 기다려야(join)** 할 때, 기다리는 동안 worker thread가 묶이면(blocked) 그 코어가 논다.

**두 해법:**
- **Stackless continuation (CPS):** "기다린 뒤 할 일"을 콜백/coroutine frame으로 떼어 둔다. C++20 코루틴, async/await. 장점: 메모리 작음. 단점: 코드를 continuation 단위로 쪼개야 하고, 기존 동기 코드를 그대로 못 쓴다.
- **Stackful fiber:** worker thread 위에 **별도 스택을 가진 사용자 수준 실행 문맥(fiber)** 을 올린다. job이 `WaitForCounter`에 도달하면 **현재 fiber 전체를 통째로 yield**(스택 보존)하고, worker thread는 즉시 다른 ready fiber/job을 집어 돈다. counter가 0이 되면 대기 fiber를 ready 큐로 옮겨 **아무 worker나 resume**. 장점: **동기 코드처럼 짜도 비차단**. 단점: fiber당 스택 메모리(수십 KB), 컨텍스트 스위치 비용, 디버깅 난이도.

**Naughty Dog가 stackful fiber를 택한 이유(Gyrling 2015):** 기존 게임 코드의 거대한 동기 호출 그래프를 **재작성 없이** 비차단으로 만들 수 있다. fiber switch는 OS thread context switch보다 훨씬 싸다(커널 진입 없음, 레지스터+스택 포인터 교체만).

```text
ThreadOnly 모델 (Winters 현재):
  ParentJob 실행 중 worker0
  └─ WaitForCounter → worker0이 busy-wait + help-steal (thread가 묶임)

FiberFull 모델 (목표):
  Parent fiber 실행 중 worker0
  └─ WaitForCounter → counter wait list 등록, root worker fiber로 yield
       worker0 → 즉시 다른 ready fiber 실행 (안 묶임)
       counter==0 → Parent fiber를 ready 큐로 → 아무 worker가 resume
```

### 2.2 대표 기존 연구

- **Gyrling (2015), "Parallelizing the Naughty Dog Engine Using Fibers"** (GDC). **산업 레퍼런스의 원전.** 카운터 기반 의존성, fiber pool, atomic counter로 job 의존성 표현. 학술 출판 아님 → motivation/사례로 인용(가이드 §9).
- **Kukanov & Voss (2007), "The Foundations for Scalable Multi-core Software in Intel TBB"**. task 기반 스케줄링·continuation passing의 산업 표준.
- **C++ coroutines (P0057, Gor Nishanov)** — stackless 대안. fiber와의 trade-off 비교 대상.
- **결정론적 멀티스레딩(이 절의 박사 핵심):**
  - **Devietti, Lucia, Ceze & Oskin (2009), "DMP: Deterministic Shared Memory Multiprocessing"** (ASPLOS). 공유메모리 멀티프로세싱을 결정론적으로 만드는 HW/SW 기법.
  - **Olszewski, Ansel & Amarasinghe (2009), "Kendo: Efficient Deterministic Multithreading in Software"** (ASPLOS). **순수 SW**로 lock 기반 프로그램의 결정론적 인터리빙을 강제. 게임 서버에 직접 영감.
  - **Bergan et al. (2010), "CoreDet: A Compiler and Runtime System for Deterministic Multithreaded Execution"** (ASPLOS).
  - **Record/replay 계열:** Veeraraghavan et al. "DoublePlay"(ASPLOS 2011), "Respec"(ASPLOS 2010) — 실행을 기록해 재현. **fiber 스케줄 기록/재현**의 직접 선행 연구.

### 2.3 자료구조/알고리즘 (의사코드)

**카운터 기반 의존성 + fiber yield:**

```text
struct Counter { atomic<u32> remaining; WaitList waiters; }  // fiber들이 대기

Submit(jobs[N], counter):
    counter.remaining.fetch_add(N, relaxed)   // ★ Submit 안에서 증가 (호출부 수동 증가 금지)
    for j in jobs: ReadyQueue.push({j, counter})

WorkerLoop():                                  // 각 worker thread = root fiber 1개
    loop:
        if pick ready fiber F: SwitchToFiber(F)        // 중단됐던 fiber 재개
        elif pick ready job J:
            f = FiberPool.acquire()                    // ★ pool에서 재사용 (Create/Delete 반복 금지)
            f.bind(J); SwitchToFiber(f)
        else: backoff / steal

// job 실행 중 fiber 안에서:
WaitForCounter(counter, target=0):
    if running_in_job_fiber:                   // ★ 컨텍스트 분기 (Winters 데드락 사고 #2)
        if counter.remaining.load(acquire) <= target: return   // 이미 완료
        counter.waiters.register(current_fiber)
        SwitchToFiber(root_worker_fiber)       // yield — worker는 안 막힘
        // ... 나중에 resume되면 여기서 이어짐 (스택 그대로)
    else:                                       // main/thread 컨텍스트
        busy_wait + help_steal until remaining <= target   // (Winters 현재 ThreadOnly path)

OnJobComplete(counter):                         // job 끝낸 fiber가
    if counter.remaining.fetch_sub(1, acq_rel) == 1:   // 마지막이면
        for w in counter.waiters: ReadyQueue.push(w)    // 대기 fiber 깨움
    FiberPool.release(self) after switching out         // ★ 실행 중 fiber 삭제 금지
```

**fiber 시스템의 함정 4종(메모리 순서·소유권):**
1. **Counter 이중 증가:** 호출부가 `Increment(N)` 하고 `Submit`도 내부에서 증가 → counter=2N → 영원 대기. **Submit이 유일한 증가 지점**이어야 한다.
2. **실행 중 fiber 삭제:** `DeleteFiber`를 자기 자신에게 호출 = 즉사. 반드시 다른 fiber로 switch한 뒤 삭제(또는 pool 반환).
3. **WaitForCounter 컨텍스트 혼동:** fiber 컨텍스트에서 "옛 help-stealing"을 그대로 돌리면, 도운 job이 root로 return하면서 대기 fiber의 콜스택이 버려진다. fiber면 **yield**, thread면 **help-steal**로 분기.
4. **fiber-local vs thread-local 혼동(가장 미묘):** §2.4 참조.

### 2.4 박사급 novel 각도 (open problems) — ★ 본 문서의 심장

1. **결정론적 fiber 스케줄(deterministic replay of fiber schedule).** 본 문서의 중심 명제. work-stealing + fiber는 "어떤 fiber가 어떤 worker에서 언제 resume되는가"가 매 실행 다르다. 게임 서버 lockstep은 비트단위 재현을 요구. **기여 후보:** 스케줄 비결정성과 결과 결정성을 **분리하는 계약**:
   - 모든 job이 **자기 출력 슬롯에만 write**(no shared mutable state) → 실행 순서 무관하게 결과 동일(Winters jobification playbook의 "output per-index write"가 정확히 이 원리).
   - reduction/apply 단계는 **결정론적 정렬 순서**(EntityID ascending 등)로 직렬 처리 → 부동소수점 합산 순서까지 고정.
   - 이를 **형식적으로**: job 그래프가 데이터-경합 없음(race-free)이고 commutative하지 않은 연산은 모두 정렬된 직렬 단계로 보내면, 임의 스케줄에 대해 **결과가 유일(confluent)** 함을 증명.
   - → SOSP/OSDI/PPoPP. "deterministic-by-construction job system."
2. **fiber 스케줄의 record/replay 디버깅.** fiber 시스템은 디버거 콜스택이 낯설고(스택이 worker thread와 분리), 버그 재현이 어렵다. 기여: 스케줄 결정점(steal·resume 순서)만 **경량 기록**해 비트단위 재현하는 디버깅 인프라 + 오버헤드 분석. (record/replay 계열을 fiber에 특화.)
3. **우선순위·기한(priority/deadline-aware fiber scheduling).** 게임 틱은 16.6ms 예산. 기여: critical-path job 우선 + soft-deadline 보장 스케줄링이 프레임 타임 jitter를 줄이는 정도. open: work-stealing의 부하 균형 ↔ 우선순위의 충돌.
4. **fiber-local 상태의 안전성 모델.** 가장 미묘한 함정: `thread_local`은 **OS thread당 하나**인데, fiber가 worker를 갈아타면(worker0에서 시작 → yield → worker3에서 resume) `thread_local` 값이 바뀐다. 기여: "fiber-safe" 코드의 형식적 정의(yield 경계를 넘는 thread-local 캐싱 금지)와 그 위반을 정적으로 검출하는 분석. ("thread-safe ≠ fiber-safe" 를 정리화.)

### 2.5 Thesis statement 예시

> "Work-stealing fiber job system에 **출력 격리 + 결정론적 reduction 계약**을 부과하면, 실행 스케줄이 비결정적임에도 lockstep 게임 시뮬레이션을 **멀티코어에서 N회 실행 비트단위 재현(상태 해시 일치율 100%)** 하면서, single-thread 결정론 대비 **strong scaling X배**를 달성하고 결정론 레이어 오버헤드는 Y% 이내다."

(가이드 §3: 새 계약 X + 제약 C[비결정적 work-stealing] + 목표 Y[비트단위 재현 + scaling] + 정량 Z + falsifiable: "해시 불일치 1건이라도 나오면 반증".)

### 2.6 평가 방법

- **결정론 재현률(이 분야 고유 metric, 가이드 §7 "결정론 재현률"):** 동일 입력으로 게임 세션을 N회(예: 1000회) × M tick 실행 → 매 tick 상태 해시(FNV/xxhash over 정렬된 컴포넌트 바이트) 캡처 → **전 실행 해시 행렬 완전 일치 비율.** cross-platform(x86 ↔ ARM)이면 더 강함. Winters 문서가 이미 "byte-identical 검증", "1000 tick state hash 동일"을 합격 기준으로 씀.
- **Scaling:** strong(고정 시뮬, core ↑) + weak. baseline: single-thread lockstep(결정론 100%지만 안 빠름), naive 멀티스레드(빠르지만 결정론 깨짐). **이 둘 사이를 메우는 게 기여의 증명.**
- **결정론 레이어 오버헤드:** 결정론 보장 끈 버전 대비 frame time/throughput 차이(%). ablation: 출력 격리만 / 정렬 reduction만 / 둘 다.
- **Jitter:** tick 시간 분포(p50/p99/max). Winters가 이미 `maxJitter` 로깅.
- **Threats to validity:** FP 비결합성(부동소수점 합산 순서), `std::unordered_map` 반복 순서, RNG 상태 공유 — 모두 비결정성 누출원으로 적시.

### 2.7 Winters 연결점

Winters는 **active ThreadOnly 위에 dormant FiberShell 골격까지만 구현**했고, 이후 단계는 설계 문서로 남아 있다. 따라서 현재는 완성된 사례가 아니라 단계별 검증이 가능한 testbed다.

- **단계적 fiber 도입:** 실제 enum은 `ThreadOnly/FiberShell` 둘뿐이다. FiberShell은 per-job Create/Switch/Delete 골격이지만 default OFF이고 enable caller가 없다. `FiberPool`과 `FiberFull(WaitForCounter yield)`은 목표 설계다.
- **§2.3 함정은 설계 위험 목록:** counter 이중 증가, lock을 든 yield, TLS/FLS, IOCP 비적용, CWorld access 계약은 구현 완료 사고가 아니라 이후 단계의 검증 체크리스트다.
- **현재 API 표면:** `Submit`, help-stealing `WaitForCounter`, dormant `FiberShellEntry/TryExecuteItemOnFiber`까지 존재한다. `CJobCounter`는 atomic count만 가지며 waiter map/ready queue는 없다.
- **결정론 계약은 설계됨:** Server playbook의 Decision-parallel/Apply-serial, sorted reduction, byte-identical 기준은 유효한 목표다. 그러나 현재 Server runtime은 JobSystem/Fiber에 연결되지 않았으므로 “작동하는 서버 병렬 사례”가 아니라 future acceptance contract로 분류한다.
- **권위 충돌이 실재(핵심 긴장의 testbed):** 서버는 lockstep/replay 결정론 권위를 가지는데(Server/Replay 문서군) fiber 병렬을 도입 중 → "병렬 + 결정론"의 충돌이 **추상 명제가 아니라 자기 코드의 미해결 과제**. 박사 명제(§2.5)를 자기 엔진에서 직접 falsify/검증 가능.
- **IOCP 비적용 경계:** deep-dive §6이 IOCP WorkerLoop/accept thread/blocking socket을 **fiber 비적용**으로 명시 — "OS completion queue와 user-mode cooperative scheduling을 같은 층에 섞지 않는다"는 시스템 설계 판단. 평가의 threats(어디까지가 fiber 영역인가)를 명확히 함.

---

## 3. Entity Component System (ECS)

### 3.1 핵심 원리

ECS는 **데이터(Component)와 동작(System)을 엔티티(Entity, 단순 ID)에서 분리**하는 아키텍처다. 상속 계층 대신 **합성(composition)**: 엔티티 = 컴포넌트 집합. System은 "특정 컴포넌트 조합을 가진 엔티티들"을 쿼리해 일괄 처리.

병렬/DOD 관점에서 ECS의 진짜 가치는 **저장 레이아웃**에 있다. 두 갈래:

- **Archetype (table) 저장:** 같은 컴포넌트 조합(archetype)을 가진 엔티티들을 **하나의 연속 테이블**에 모은다. 컴포넌트별 열(column)이 SoA로 빽빽. 쿼리 = 매칭 archetype들의 열을 선형 순회 → **캐시 지역성 극대화, 자동 SoA**. 단점: 컴포넌트 추가/제거(structural change)가 엔티티를 **다른 테이블로 통째 이동(memcpy)** → 비쌈. Unity DOTS, Unreal Mass, flecs.
- **Sparse-set 저장:** 컴포넌트 타입마다 (sparse 배열: entity→dense index) + (dense 배열: 빽빽한 컴포넌트). 쿼리 = 가장 작은 set 기준 교집합. structural change가 **싸다**(set에 push/swap-pop). 단점: 다중 컴포넌트 쿼리 시 dense 배열들이 서로 다른 순서 → 약간의 간접 참조. EnTT(기본).

```text
Archetype:  [(Pos,Vel,HP)] 테이블 → Pos열|Vel열|HP열 (각 SoA, 빽빽이 순회)
            [(Pos,Vel)]    테이블 → Pos열|Vel열
            컴포넌트 추가 → 엔티티를 새 테이블로 이동(memcpy)

Sparse-set: Pos: sparse[entity]→i, dense[i]=Pos값
            Vel: sparse[entity]→j, dense[j]=Vel값
            쿼리(Pos&Vel) = 작은 set 순회하며 다른 set 멤버십 확인
```

### 3.2 대표 기존 연구

ECS는 학술 논문보다 **산업/시스템 구현**이 앞서간 분야다(박사 기여의 빈 땅이 많다는 뜻):
- **Unity DOTS / ECS** — archetype-chunk(16KB chunk) 모델. job system과 결합한 데이터 병렬. 산업 레퍼런스.
- **EnTT (Caini)** — sparse-set ECS의 사실상 표준 오픈소스. group/view, 그룹화로 지역성 확보.
- **Bitsquid/Stingray (Niklas Frykholm 블로그 시리즈)** — 데이터 지향 엔진 설계의 산업 원전.
- **flecs (Sander Mertens)** — archetype + relationship(엔티티 간 관계) ECS. "Where are the ECS papers?" 같은 글로 학술 공백을 지적.
- **학술 인접:** database 분야의 **column store**(MonetDB, C-Store; Stonebraker 2005) — archetype-SoA는 본질적으로 컬럼 스토어. 쿼리 최적화·벡터화 실행 이론을 ECS로 이식하는 게 미개척 기여 영역.

### 3.3 자료구조/알고리즘 (의사코드)

**Sparse-set ECS (EnTT 스타일):**
```text
struct SparseSet<T>:
    vector<u32>   sparse   // entity_id → index into dense (또는 INVALID)
    vector<Entity> dense   // 빽빽한 entity 목록 (쿼리 순회 대상)
    vector<T>      comps    // dense[i]의 컴포넌트 (dense와 평행)

    add(e, v):  sparse[e] = dense.size(); dense.push(e); comps.push(v)
    remove(e):  i = sparse[e]; last = dense.size()-1
                swap(dense[i], dense[last]); swap(comps[i], comps[last])
                sparse[dense[i]] = i; dense.pop(); comps.pop()   // swap-and-pop: O(1)
    has(e):     sparse[e] valid && dense[sparse[e]] == e

Query(A, B):    // 작은 쪽 기준 교집합
    base = smaller(setA, setB)
    for e in base.dense:
        if other.has(e): yield (e, setA.get(e), setB.get(e))
```

**Deferred structural change (동시성 안전의 핵심):**
```text
// 문제: System들이 병렬로 도는 동안 add/remove/create가 일어나면
//       다른 system의 쿼리 반복이 무효화(iterator invalidation) + race.
// 해법: structural change를 command buffer에 기록만 하고, barrier 후 직렬 적용.

CommandBuffer (worker별 1개, race 없음):
    record: CreateEntity / Destroy(e) / Add<T>(e,v) / Remove<T>(e)

ParallelSystemPhase:
    for system in phase (서로 다른 컴포넌트 집합):  // 병렬
        system.run(world_readonly, cmdbuf[worker])  // 구조변경은 기록만

Barrier  // 모든 system join

PlaybackSerial:                                   // 직렬 + 결정론적 순서
    sort commands by (deterministic key)          // 예: entity id, 기록 순서
    for cmd in commands: world.apply(cmd)          // 실제 구조 변경
```

### 3.4 박사급 novel 각도 (open problems)

1. **동시성 안전 structural change의 직렬화 가능성(serializability) 증명.** deferred-commit이 "마치 어떤 직렬 순서로 적용한 것과 동일"함을 형식적으로 보장하고, 그 순서가 **결정론적**임을 증명. open: command 간 의존(같은 엔티티에 add+remove)의 정의된 semantics.
2. **쿼리 스케줄링(query scheduling) = 의존성 자동 추론.** 각 system이 읽고/쓰는 컴포넌트 집합(read/write set)에서 **충돌 그래프**를 만들어, 충돌 없는 system들을 자동 병렬 배치(R-R 동시 OK, W-W/R-W 직렬). 기여: 이 스케줄링을 work-stealing job graph로 컴파일하고 최적 병렬도를 분석. (Unity가 수동으로 하는 것을 자동·증명 가능하게.) → §2.4-(1) 결정론과 결합하면 강력.
3. **결정론적 반복 순서(deterministic iteration order).** sparse-set의 dense 배열은 add/remove(swap-pop) 이력에 따라 순서가 달라진다 → 같은 엔티티 집합도 반복 순서가 실행마다 다를 수 있음 → 결정론 누출. 기여: O(1) 구조 변경을 유지하면서 반복 순서를 결정론적으로 고정하는 자료구조(또는 정렬 비용의 trade-off 분석).
4. **archetype vs sparse-set의 워크로드 적응형 하이브리드.** structural change 빈도 × 쿼리 빈도에 따라 저장 방식을 **런타임/컴파일타임에 자동 선택**. 기여: 게임 워크로드(MOBA: 미니언 대량 생성/소멸 = structural-heavy; 정적 지형 = query-heavy)에서 적응형이 양 극단 대비 얼마나 이득인지 정량화.

### 3.5 Thesis statement 예시

> "ECS의 structural change를 **read/write set 기반 충돌 그래프**로 스케줄링하고 deferred-commit로 적용하면, 수동 동기화 없이 system들을 자동 병렬화하면서 **직렬 실행과 동일한(serializable) 결정론적 결과**를 보장하고, 단일 스레드 ECS 대비 **엔티티 N개에서 throughput X배**를 달성한다."

### 3.6 평가 방법

- **Scaling:** 엔티티 수(10^4~10^7) 대비 system update throughput. structural-heavy(대량 spawn/destroy) vs query-heavy 워크로드 분리.
- **캐시:** L1/L2 miss(perf/PMU), 쿼리당 cache line touch. archetype vs sparse-set 직접 대조.
- **Structural change 비용:** add/remove/create의 amortized cost, command buffer playback 시간.
- **결정론:** §2.6과 동일 — 반복 순서·commit 순서를 바꿔도 상태 해시 동일.
- **Baseline:** EnTT(sparse), Unity DOTS/flecs(archetype), naive OOP(가상함수 + AoS).

### 3.7 Winters 연결점

Winters는 ECS가 **이미 가동 중**이고, MOBA 워크로드(미니언 대량 생성/소멸 = structural-heavy)가 §3.4-(4)의 적응형 연구에 딱 맞는다.

- **실재 ECS 시스템들:** [`Engine/Private/ECS/Systems/`](../../Engine/Private/ECS/Systems/) — `MinionAISystem`, `MinionSeparationSystem`, `TurretAISystem`, `TurretProjectileSystem`, `NavigationSystem`, `SpatialHashSystem`, `StatusEffectSystem`, `VisionSystem`, `GameplayCollisionSystem`, `BehaviorTreeSystem`, `MCTSSystem`, `TransformSystem` 등. Client측 [`YoneSoulSpawnSystem`](../../Client/Private/ECS/Systems/YoneSoulSpawnSystem.cpp). **충돌 그래프(§3.4-(2))를 그릴 실제 read/write set이 여기 있다.**
- **컴포넌트 = 네트워크 친화 POD:** [`GameplayComponents.h`](../../Engine/Public/ECS/Components/GameplayComponents.h) — `ChampionComponent`, `MinionComponent`, `TurretComponent` 등이 "값 멤버만, Shared/Schemas/*.fbs와 1:1 매핑"으로 설계(주석 L9-16). **POD·SoA 친화 = §3.1 archetype-SoA 적용 대상이자 §2.4-(1) 결정론(바이트 단위 해시)의 기반.**
- **structural-heavy 실증:** Server 문서가 `Phase_ServerProjectiles`/`Phase_ServerMinionAI`에서 `m_world.DestroyEntity`, `AddComponent`, `EntityIdMap.IssueNew`를 **write-heavy로 분류해 병렬화 금지**([`03_SERVER_PARALLEL_PHASES.md`](../TODO/05-07/Server/03_SERVER_PARALLEL_PHASES.md) §7, playbook §2 Type C). **이게 정확히 §3.4-(1) "동시성 안전 structural change"의 미해결 지점** — Winters가 지금은 "직렬 회피"로 막아둔 것을 deferred-commit + serializability 증명으로 푸는 게 기여.
- **deferred-commit 단초:** playbook의 "Decision parallel / Apply serial"(§5)이 사실상 command buffer 패턴의 수동 버전 → §3.3 deferred structural change로 일반화·증명할 자리.
- **CWorld read 동시성 미검증:** deep-dive 사고#4가 "`CWorld::GetComponent`는 non-const, 병렬 read 검증 전 가정 금지"라고 적시 → **§3.4-(2) read/write set 추론의 출발 문제**(어떤 접근이 정말 read-only인가)가 자기 코드에 실재.

---

## 4. Data-Oriented Design (DOD)

### 4.1 핵심 원리

DOD의 출발점은 하드웨어 사실 하나다: **메모리 지연(수백 cycle)이 연산(1 cycle)보다 압도적으로 비싸다.** 따라서 성능은 "연산을 줄이는 것"이 아니라 **"메모리를 어떻게 흐르게 하는가"**로 결정된다(memory-bound). DOD는 코드를 **변환(transform): 입력 데이터 → 출력 데이터** 로 보고, 그 데이터의 **실제 접근 패턴**에 맞춰 레이아웃을 설계한다.

핵심 원칙(Mike Acton):
- **"Where there is one, there are many."** 단수를 위한 추상화(객체 1개)는 거의 항상 거짓 — 실제로는 항상 배열로 처리된다. 따라서 **복수(배열) 단위로 설계**하라.
- **SoA vs AoS:** AoS(Array of Structs)는 `struct{pos,vel,hp,name,...}` 배열 → 한 필드만 순회해도 **불필요한 필드까지 캐시 라인에 끌려옴**. SoA(Struct of Arrays)는 필드별 배열 → hot 필드만 빽빽이 순회 → **캐시 라인 100% 유효 데이터**, 자동 벡터화(SIMD) 친화.
- **캐시 라인(64B) 단위 사고 + 프리페치(prefetch).** 선형 접근은 HW prefetcher가 미리 끌어온다 → 포인터 추적(linked list/가상함수)은 prefetch 무력화.

```text
AoS:  [P0 V0 HP0 Name0][P1 V1 HP1 Name1]...  // HP만 순회 → 캐시에 P,V,Name 쓰레기
SoA:  [P0 P1 P2 ...][V0 V1 ...][HP0 HP1 ...]  // HP 순회 → 캐시 라인 100% HP, SIMD 가능
```

### 4.2 대표 기존 연구

- **Acton (2014), "Data-Oriented Design and C++"** (CppCon). **DOD 산업 선언문.** "the data is the problem space", OOP 추상화 비판, 캐시·"one vs many". 학술 아님 → motivation/사례로 인용.
- **Fabian (2018), "Data-Oriented Design"** (책). DOD 원칙의 체계적 정리.
- **Llopis (2009), "Data-Oriented Design (or Why You Might Be Shooting Yourself in the Foot With OOP)"**. 초기 정식화.
- **학술 토대:**
  - **Drepper (2007), "What Every Programmer Should Know About Memory"** — 캐시 계층·prefetch·NUMA의 기초 레퍼런스.
  - **Database 컬럼 스토어:** Boncz et al. "MonetDB/X100" (CIDR 2005), Stonebraker et al. "C-Store" (VLDB 2005) — SoA·벡터화 실행의 학술 원조. **DOD를 학술 언어로 말하는 다리.**
  - **컴파일러 자동 변환:** Hagog & Tice "Cache Aware Data Layout Reorganization" (GCC), Hundt et al. structure splitting/field reordering (CGO 2006), structure-of-array 자동 변환 연구.

### 4.3 자료구조/알고리즘 (의사코드)

**Hot/Cold splitting + SoA:**
```text
// 나쁨 (AoS, hot/cold 혼재):
struct Entity { vec3 pos; vec3 vel; float hp;        // hot (매 틱)
                char name[32]; Texture* icon; ... }  // cold (드묾)
vector<Entity> entities;
for e in entities: e.pos += e.vel * dt;   // 캐시 라인에 name/icon 쓰레기

// 좋음 (SoA + hot/cold split):
struct Hot  { vector<vec3> pos; vector<vec3> vel; vector<float> hp; }   // 빽빽
struct Cold { vector<Name> name; vector<Texture*> icon; }              // 분리
for i in range(N): hot.pos[i] += hot.vel[i] * dt;   // 캐시 라인 100% 유효, SIMD

// 변환을 명시적으로 (Acton: 코드 = 데이터 변환):
Integrate(span<vec3> pos, span<const vec3> vel, float dt):  // in/out 명시
    for i: pos[i] = fma(vel[i], dt, pos[i])   // 컴파일러가 AVX로 벡터화
```

**자동 SoA 변환(연구 방향, 의사):**
```text
// 컴파일러/메타프로그래밍이 AoS 선언에서 SoA 저장을 자동 생성:
SOA_STRUCT(Particle, (vec3,pos)(vec3,vel)(float,life))
// → 내부적으로 vector<vec3> pos; vector<vec3> vel; vector<float> life; 생성
// → particle[i] 접근 시 proxy로 묶어 AoS처럼 보이되 SoA로 저장
// 접근 패턴 프로파일 → hot 필드 자동 군집(field reordering)
```

### 4.4 박사급 novel 각도 (open problems)

1. **자동 SoA 변환(automatic AoS→SoA / data layout synthesis).** 개발자가 AoS로 짜되, **접근 패턴 프로파일에서 최적 레이아웃을 자동 도출**(SoA/AoSoA/hot-cold split)하는 변환. open: 정확성 보존, 포인터 별칭(aliasing), 변환의 자동 검증.
2. **워크로드 기반 데이터 레이아웃 최적화.** 어떤 필드들이 **함께 접근(co-access)**되는지 동적 프로파일 → 같은 캐시 라인/배열로 군집(field clustering). database physical design(어떤 컬럼을 함께 둘지)의 게임 엔진 이식. 기여: 게임 hot loop에서 자동 군집이 수동 튜닝에 근접/초과함을 증명.
3. **컴파일러 협력(compiler cooperation).** `restrict`/별칭 정보·접근 패턴을 컴파일러에 전달해 벡터화·prefetch를 강제. 기여: 언어 확장 또는 attribute로 "이 루프는 SoA·race-free"를 선언하면 컴파일러가 자동 SIMD+병렬화, 그 안전성을 타입 시스템으로 보장.
4. **AoSoA(tiled) 자동 타일 크기 선택.** SoA는 random access가 약하고 AoS는 순회가 약하다. AoSoA(예: 8개씩 묶은 SoA 타일)가 절충인데 타일 크기가 ISA(SIMD 폭)·캐시에 의존. 기여: 타일 크기를 자동 도출하고 cross-ISA(x86 AVX ↔ ARM NEON/SVE) 이식성과 함께 분석.

> **§1 가이드 환기:** "SoA로 바꿨다"는 **구현**. "접근 패턴에서 레이아웃을 자동 도출하고, 그것이 수동 튜닝 대비 z배·N개 워크로드에서 일반화됨을 증명"은 **기여**. DOD는 단독 박사 챕터보다, ECS·job system의 **성능 근거(why SoA helps here)** 와 자동화 기여로 엮일 때 강하다.

### 4.5 Thesis statement 예시

> "게임 hot loop의 동적 co-access 프로파일에서 **데이터 레이아웃(SoA/AoSoA/필드 군집)을 자동 합성**하는 변환은, 개발자 AoS 선언을 보존하면서 bandwidth-bound 구간에서 **수동 SoA 튜닝의 z% 이내 성능**을 자동 달성하고, cross-ISA에서 일관된 이득을 보인다."

### 4.6 평가 방법

- **성능:** hot loop throughput(M elem/s), bandwidth 활용률(achieved/peak), IPC, SIMD 활용도. roofline 모델로 memory-bound vs compute-bound 위치.
- **캐시:** L1/L2/L3 miss rate, prefetch 적중률, cache line 유효 데이터 비율(perf c2c, PMU).
- **Ablation:** AoS / SoA / AoSoA / hot-cold split을 동일 워크로드에서 분리 측정 → "효과의 원천" 증명.
- **이식성:** x86(AVX2/512) vs ARM(NEON/SVE) 동일 코드 이득.
- **Baseline:** 수동 SoA(상한), naive AoS(하한), 컴파일러 -O3 자동벡터화 단독.

### 4.7 Winters 연결점

- **FX/파티클이 DOD의 정석 적용처:** Winters FX 시스템([`Client/Private/GameObject/FX/`](../../Client/Private/GameObject/FX/) — `FxSystem`, `FxMeshSystem`, `FxBeamSystem`, `FxRibbonComponent`)과 EffectTool 설계가 **"runtime SoA + ECS systems"** 를 명시([`04_EFX2_RUNTIME_SOA_AND_ECS_SYSTEMS.md`](../TODO/05-07/EffectTool/04_EFX2_RUNTIME_SOA_AND_ECS_SYSTEMS.md)). 파티클은 "one vs many"의 교과서 — §4.4 자동 SoA·AoSoA 변환의 직접 testbed.
- **GPU compute 경로:** EffectTool가 GPU compute data interface(`07_EFX5_GPU_COMPUTE_DATA_INTERFACE`)를 두어, SoA 레이아웃이 CPU→GPU 전송·compute shader 효율에 직결 → §4.4-(3) 컴파일러/ISA 협력을 GPU로 확장.
- **ECS POD = SoA 준비됨:** §3.7의 네트워크 친화 POD 컴포넌트가 그대로 SoA 변환·SIMD 적분 대상. 미니언 대량(수백 유닛) 이동/분리(`MinionSeparationSystem`, `MinionPerformanceSystem`)가 bandwidth-bound hot loop 후보.
- **측정 인프라(가이드 §7):** CLAUDE.md의 "inspectable debug UI/overlay + bounded trace" 문화 + Server의 `maxJitter` 로깅이 곧 DOD 평가의 PMU·throughput 측정대.

---

## 종합. 통합 학위논문 구조 예시 (three-papers → 하나의 명제)

가이드 §4.1 "Three Papers Make a Thesis" 모델로, **이 분야의 인접 3문제**를 하나의 학위논문으로 묶는다. 묶는 명제는 본 문서의 중심 긴장이다.

> **통합 Thesis statement:**
> "스케줄 비결정성과 결과 결정성을 **분리하는 fiber work-stealing 런타임**은, lockstep 게임 서버 시뮬레이션을 멀티코어에서 **비트단위 재현 가능**하게 유지하면서 single-thread 결정론 대비 강한 scaling을 달성한다."

```text
제목(가제): Deterministic-by-Construction Parallelism for Lockstep Game Simulation
             on Fiber-Based Work-Stealing Runtimes

Ch 1. 서론
   1.3 Thesis: 위 통합 명제
   1.4 기여:
       (C1) 약한 메모리 모델에서 기계검증된 NUMA-aware Chase-Lev work-stealing 스케줄러
       (C2) 출력 격리 + 결정론적 reduction 계약을 강제하는 fiber job 런타임 (deterministic-by-construction)
       (C3) read/write set 기반 자동 병렬 ECS 스케줄링 + serializable deferred structural change
       (보조) SoA 자동 변환을 통한 hot-loop bandwidth 최적화 (C1~C3의 성능 근거)

Ch 2. 배경/관련 연구
   - work stealing (Blumofe&Leiserson 1999, Chase&Lev 2005, Lê et al. 2013)
   - fiber/continuation (Gyrling 2015, Kukanov&Voss 2007)
   - deterministic execution (DMP, Kendo, CoreDet; record/replay)
   - ECS/DOD (DOTS, EnTT; Acton 2014; column store)
   - gap: "병렬은 비결정, 결정론은 비병렬" — 둘을 동시 만족하는 게임 서버 런타임 부재

Ch 3 (논문1, PPoPP): C1 — Correct & Scalable Work-Stealing
   - Winters CWorkStealingDeque의 메모리 순서를 model checker로 검증, Main-push race 수정
   - NUMA-aware victim 선택 → 96-core strong/weak scaling
   - 평가: throughput, steal 통계, 정합성 위반 0

Ch 4 (논문2, OSDI/SOSP): C2 — Deterministic-by-Construction Fiber Runtime
   - 출력 격리 + 정렬 reduction 계약, confluence(결과 유일성) 증명
   - fiber 스케줄 record/replay 디버깅
   - 평가: 1000회×M tick 상태 해시 일치율 100%, scaling, 결정론 오버헤드 %, cross-platform

Ch 5 (논문3, EuroSys/PPoPP): C3 — Auto-Parallel Serializable ECS
   - read/write set 충돌 그래프 → job graph 컴파일, deferred-commit serializability 증명
   - 결정론적 반복 순서
   - 평가: 엔티티 N개 scaling, structural-heavy vs query-heavy, 직렬과 동일 결과

Ch 6. 종합 평가 — Winters MOBA 서버 워크로드(미니언/투사체/AI)에서 C1+C2+C3 통합 측정
Ch 7. 논의 — threats(FP 비결합성, unordered_map 순서, RNG), 일반화(RTS/격투/분산 시뮬)
Ch 8. 결론 — 통합 명제 증명 정리, future work(GPU job graph, 분산 결정론)
```

핵심: 3편이 각각 **독립 venue 논문**(PPoPP/OSDI/EuroSys)이면서, 서론·결론이 "병렬+결정론"이라는 **단일 명제**로 묶는다(가이드 §4.1). Winters는 C1(실제 deque), C2(fiber 단계 박제 + 결정론 계약), C3(가동 중 ECS + write-heavy 회피 지점) **모두에 작동하는 testbed**를 이미 보유 — 가이드 §2의 "엔진은 검증 플랫폼" 그 자체.

---

## 참고문헌

**Work-Stealing / 메모리 모델**
- Blumofe, R. D., & Leiserson, C. E. (1999). Scheduling Multithreaded Computations by Work Stealing. *Journal of the ACM*, 46(5).
- Frigo, M., Leiserson, C. E., & Randall, K. H. (1998). The Implementation of the Cilk-5 Multithreaded Language. *PLDI*.
- Chase, D., & Lev, Y. (2005). Dynamic Circular Work-Stealing Deque. *SPAA*.
- Lê, N. M., Pop, A., Cohen, A., & Zappa Nardelli, F. (2013). Correct and Efficient Work-Stealing for Weak Memory Models. *PPoPP*.
- Morrison, A., & Afek, Y. (2014). Fence-Free Work Stealing on Bounded TSO Processors. *ASPLOS*.

**Fiber / 결정론적 실행**
- Gyrling, C. (2015). Parallelizing the Naughty Dog Engine Using Fibers. *GDC* (산업, 비학술).
- Kukanov, A., & Voss, M. J. (2007). The Foundations for Scalable Multi-core Software in Intel Threading Building Blocks. *Intel Technology Journal*.
- Devietti, J., Lucia, B., Ceze, L., & Oskin, M. (2009). DMP: Deterministic Shared Memory Multiprocessing. *ASPLOS*.
- Olszewski, M., Ansel, J., & Amarasinghe, S. (2009). Kendo: Efficient Deterministic Multithreading in Software. *ASPLOS*.
- Bergan, T., et al. (2010). CoreDet: A Compiler and Runtime System for Deterministic Multithreaded Execution. *ASPLOS*.
- Veeraraghavan, K., et al. (2011). DoublePlay: Parallelizing Sequential Logging and Replay. *ASPLOS*.

**ECS / DOD / 메모리**
- Acton, M. (2014). Data-Oriented Design and C++. *CppCon* (산업, 비학술).
- Fabian, R. (2018). *Data-Oriented Design*.
- Drepper, U. (2007). What Every Programmer Should Know About Memory. *Red Hat Technical Report*.
- Boncz, P., Zukowski, M., & Nes, N. (2005). MonetDB/X100: Hyper-Pipelining Query Execution. *CIDR*.
- Stonebraker, M., et al. (2005). C-Store: A Column-Oriented DBMS. *VLDB*.
- (ECS: Unity DOTS, EnTT[S. Caini], flecs[S. Mertens] — 산업 구현, 동료심사 학술 출판 아님; 사례로 인용.)

**Winters testbed (1차 출처)**
- [`Engine/Public/Core/JobSystem/WorkStealingDeque.h`](../../Engine/Public/Core/JobSystem/WorkStealingDeque.h) — Chase-Lev deque 구현
- [`Engine/Public/Core/JobSystem.h`](../../Engine/Public/Core/JobSystem.h), [`Engine/Public/Core/JobCounter.h`](../../Engine/Public/Core/JobCounter.h), [`Engine/Public/Core/Fiber/FiberTypes.h`](../../Engine/Public/Core/Fiber/FiberTypes.h)
- [`Engine/Public/ECS/Components/GameplayComponents.h`](../../Engine/Public/ECS/Components/GameplayComponents.h), [`Engine/Private/ECS/Systems/`](../../Engine/Private/ECS/Systems/)
- [`.md/TODO/05-07/Server/`](../TODO/05-07/Server/) — `01_SERVER_FIBER_SKELETON`, `02_SERVER_TICK_FIBER_AWARE`, `03_SERVER_PARALLEL_PHASES`, `11_FIBER_CONCEPTS_SERVER_DEEP_DIVE`, `15_SERVER_PHASE_JOBIFICATION_PLAYBOOK`
- [`.md/TODO/05-07/EffectTool/04_EFX2_RUNTIME_SOA_AND_ECS_SYSTEMS.md`](../TODO/05-07/EffectTool/04_EFX2_RUNTIME_SOA_AND_ECS_SYSTEMS.md)
