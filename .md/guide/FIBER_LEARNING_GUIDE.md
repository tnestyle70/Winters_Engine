# Fiber Job System 학습 가이드 — 기초 원리부터 Winters 코드 베이스 적용까지

> **작성일**: 2026-05-03
> **목적**: 사용자가 Phase 5-B Fiber 작업을 병렬 진행 중 참조용. Win32 Fiber API + Naughty Dog GDC 2015 모델 + 현 Winters JobSystem 코드 + FIBER_JOB_SYSTEM_v2.md 의 박제된 신규 코드까지 한 흐름으로 이해.
> **참조**:
> - 현 코드: [Engine/Public/Core/JobSystem.h](Engine/Public/Core/JobSystem.h), [Engine/Private/Core/JobSystem.cpp](Engine/Private/Core/JobSystem.cpp), [Engine/Public/Core/JobSystem/WorkStealingDeque.h](Engine/Public/Core/JobSystem/WorkStealingDeque.h), [Engine/Public/Core/JobCounter.h](Engine/Public/Core/JobCounter.h)
> - 신규 박제: [.md/plan/engine/FIBER_JOB_SYSTEM_v2.md](.md/plan/engine/FIBER_JOB_SYSTEM_v2.md) (v2.1, Codex 검토 6건 반영)
> - 외부 자료: Naughty Dog GDC 2015 "Parallelizing the Naughty Dog Engine Using Fibers" (Christian Gyrling)

---

## §0. 한 단락 — 무엇을 만들고 왜

**한 줄**: OS thread 가 절대 블로킹되지 않게 만들어 게임 프레임 17ms 안에 모든 의존성 그래프 작업을 완료시키는 시스템.

**비유**: 음식점 주방.
- **Thread 모델 (현 5-A)**: 셰프 8명 (= worker thread). "수프 끓을 때까지 기다리세요" 명령 받으면 셰프가 멈춰 서서 진짜 기다림 → 다른 일 못 함
- **Fiber 모델 (5-B 도입)**: 셰프 8명 + 작업 카드 128장 (= fiber). 수프 카드 받은 셰프가 "끓을 때까지 wait" 만나면 카드를 wait 통에 넣고 다른 카드 픽업 → 셰프가 절대 멈추지 않음

게임 프레임 = 60 FPS = 16.67ms. 8 코어 활용 못 하면 1 코어가 병목 → frame drop. Fiber 가 핵심.

---

## §1. 기초 원리 — Thread vs Fiber

### 1-1. OS Thread (현 Phase 5-A)

| 특징 | 비용 |
|---|---|
| OS kernel 이 스케줄링 | 컨텍스트 스위치 ~1μs (kernel mode 진입 + TLB flush + 레지스터 저장) |
| `condition_variable::wait()` | 진짜 OS thread 블로킹 (CPU 사이클 0 사용) |
| 100개 생성 가능 | 단 100개 동시 active 면 cache 폭발 + 스위치 비용 폭증 |
| 외부 시그널 / I/O 가능 | epoll / IOCP 등 OS 의 이벤트 통합 |

**한계**: Worker thread 가 `WaitForCounter` 호출 시 진짜로 block 되거나 (cv 사용 시) busy-wait (현 5-A help-stealing). 둘 다 코어를 효율적으로 쓰지 못함.

### 1-2. Fiber (Phase 5-B 도입)

| 특징 | 비용 |
|---|---|
| **User-mode** 에서 협력적 스케줄링 | 스위치 ~20ns (레지스터 + stack pointer 저장만) |
| `SwitchToFiber()` = 명시적 양보 (yield) | OS 모름 — 100% user-mode |
| 1000개 생성 가능 (각 64KB stack) | Stack 만 메모리 차지 |
| OS 시그널 / I/O 통합 X | Worker thread 가 OS 와 통신 — fiber 는 user-mode 만 |

**핵심 능력**: Fiber 는 **자기 진행 중인 코드를 멈추고 다른 fiber 로 양보 가능**. 이 양보는 **OS thread 를 멈추지 않음** — thread 는 즉시 다른 fiber 픽업.

### 1-3. 비교 표

| 측면 | Thread | Fiber |
|---|---|---|
| 스케줄러 | OS kernel | 게임 엔진 (user code) |
| 컨텍스트 스위치 | ~1μs | ~20ns (50배 빠름) |
| 블로킹 | OS 이벤트 (cv/mutex/IO) | 명시적 yield (SwitchToFiber) |
| stack | 1MB+ | 64KB (배치 가능) |
| 동시 N | ~100 | ~1000 |
| 생성 비용 | ~10μs | ~1μs |
| 디버깅 | OS 도구 (PerfView/ETW) | 추가 도구 필요 |

---

## §2. Win32 Fiber API — 5 함수만 알면 됨

```cpp
#include <Windows.h>

// 1. Thread → Fiber 변환 (한 번만, worker 시작 시)
LPVOID hThreadFiber = ConvertThreadToFiber(nullptr);

// 2. 새 fiber 생성 (스택 할당 + entry point 박제)
LPVOID hNewFiber = CreateFiber(
    64 * 1024,           // stack size (64KB)
    &MyFiberProc,        // entry point (callback)
    pUserData            // arg
);

// 3. 다른 fiber 로 전환 (현재 fiber 멈춤)
SwitchToFiber(hNewFiber);
// ↑ 여기서 멈춤. 누군가 SwitchToFiber(thisFiber) 호출 시 다음 줄에서 재개

// 4. fiber 해제 (현재 실행 중 fiber 는 절대 X)
DeleteFiber(hOldFiber);

// 5. 현재 fiber 핸들
LPVOID hCurrent = GetCurrentFiber();
```

### 2-1. FiberProc 의 무한 루프 (★ 가장 중요한 패턴)

```cpp
void CALLBACK MyFiberProc(LPVOID lpParam)
{
    while (true)            // ★ 절대 return 금지
    {
        DoJob();            // 실제 작업
        SwitchToFiber(hReturnFiber);  // 작업 완료 → 호출자에게 복귀
        // 다음에 누군가 SwitchToFiber(thisFiber) 진입 시 여기서 재개
        // (새 job 이 박제된 상태)
    }
}
```

★ **return 하면 worker thread 자체 종료** (Win32 사양). 무한 루프 안에서 SwitchToFiber 로 양보하는 게 정석.

---

## §3. 현 Winters JobSystem (Phase 5-A) 코드 분석

### 3-1. 클래스 구조 (`Engine/Public/Core/JobSystem.h`)

```cpp
class CJobSystem
{
public:
    void Initialize(uint32_t iWorkerCount = 0);
    void Shutdown();

    void Submit(std::function<void()> job);
    void Submit(std::function<void()> job, CJobCounter* pCounter);
    void WaitForCounter(CJobCounter* pCounter, uint32_t iTarget = 0);

    static int32_t Get_WorkerIdx();    // main = -1, worker = [0, N)
    static uint32_t Get_WorkerSlot();  // main = 0, worker = idx+1

private:
    void WorkerLoop(uint32_t iWorkerIdx);
    bool TryExecuteOneJob(uint32_t iWorkerIdx);
    void ExecuteItem(WorkItem& item);
    void EnqueueJob(WorkItem&& item);

    std::vector<std::thread>   m_vecWorkers;
    std::vector<std::unique_ptr<CWorkStealingDeque<WorkItem>>>  m_vecDeques;
    std::queue<WorkItem>       m_GlobalQueue;
    std::mutex                 m_GlobalMutex;
};
```

**3 핵심 자료구조**:
1. `m_vecWorkers` — N개 OS thread (hardware_concurrency - 2)
2. `m_vecDeques` — 각 worker 의 LIFO deque (자기 push/pop) + 다른 worker 가 steal 가능
3. `m_GlobalQueue` — main thread 가 push 하는 fallback queue (Phase 5-A Codex #2 보존)

### 3-2. Submit 흐름 — `EnqueueJob`

```cpp
void CJobSystem::EnqueueJob(WorkItem&& item)
{
    if (m_bShutdown.load()) {
        ExecuteItem(item);  // shutdown 중 → 즉시 실행
        return;
    }

    const uint32_t N = m_vecDeques.size();
    if (N == 0) {
        ExecuteItem(item);  // jobsystem 미초기화
        return;
    }

    // ★ Worker 자신이 호출 → 자기 deque push (LIFO, 캐시 친화)
    if (t_iWorkerIdx >= 0 && t_iWorkerIdx < N) {
        if (m_vecDeques[t_iWorkerIdx]->Push(item))
            return;
    }

    // ★ Main 또는 외부 thread → global queue (Chase-Lev race 회피)
    {
        std::lock_guard lk(m_GlobalMutex);
        m_GlobalQueue.push(std::move(item));
    }
}
```

**Phase 5-A Codex #2 핵심**: main thread 가 worker 의 deque 에 push 하면 race. main 은 항상 global queue. worker 만 자기 deque push.

### 3-3. Worker 루프 — `WorkerLoop`

```cpp
void CJobSystem::WorkerLoop(uint32_t iWorkerIdx)
{
    t_iWorkerIdx = (int32_t)iWorkerIdx;  // thread_local 박제

    while (!m_bShutdown.load()) {
        if (!TryExecuteOneJob(iWorkerIdx))
            std::this_thread::yield();  // 일감 없음 → CPU 양보
    }

    t_iWorkerIdx = -1;
}

bool CJobSystem::TryExecuteOneJob(uint32_t iWorkerIdx)
{
    WorkItem item;

    // 1) 자기 Deque (LIFO, 가장 최근 push 한 일감)
    if (m_vecDeques[iWorkerIdx]->Pop(item)) {
        ExecuteItem(item);
        return true;
    }

    // 2) Global queue (main 이 push 한 것)
    bool hasGlobal = false;
    {
        std::lock_guard lk(m_GlobalMutex);
        if (!m_GlobalQueue.empty()) {
            item = std::move(m_GlobalQueue.front());
            m_GlobalQueue.pop();
            hasGlobal = true;
        }
    }
    if (hasGlobal) {
        ExecuteItem(item);
        return true;
    }

    // 3) Steal — 다른 worker 의 deque 뒤쪽에서 훔침
    const uint32_t N = m_vecDeques.size();
    if (N > 1) {
        const uint32_t victim = PickVictim(iWorkerIdx, N);
        if (m_vecDeques[victim]->Steal(item)) {
            ExecuteItem(item);
            return true;
        }
    }

    return false;
}
```

**3 단계 fallback**: 자기 deque → global → steal. 모두 실패 시 yield (다른 thread 에 CPU 양보).

### 3-4. 한계 — `WaitForCounter`

```cpp
void CJobSystem::WaitForCounter(CJobCounter* pCounter, uint32_t iTarget)
{
    while (pCounter->Load() > iTarget)  // ★ busy-wait + help-stealing
    {
        if (t_iWorkerIdx >= 0)
            TryExecuteOneJob(t_iWorkerIdx);  // worker 면 다른 일감 처리하며 대기
        else
            // main 은 global drain → steal
        ...

        if (!bDidWork)
            std::this_thread::yield();
    }
}
```

**문제**:
- Worker 가 `WaitForCounter` 호출 시 → "다른 일감 있는 동안" busy-wait
- 단 만약 의존성 그래프가 깊어서 다른 일감 없는데 wait 만 길면 → CPU 낭비
- 더 큰 문제: **A → B → C 의존성**에서 B 를 위해 wait 중인 A 가 thread 점유 → C 를 처리할 thread 부족

→ **Fiber 도입 = wait 중인 fiber 를 yield → thread 가 즉시 다른 fiber 픽업**.

### 3-5. Chase-Lev WorkStealingDeque

```cpp
template <typename T>
class CWorkStealingDeque
{
public:
    bool Push(const T& v);   // owner 만 (LIFO bottom)
    bool Pop(T& out);        // owner 만 (LIFO bottom)
    bool Steal(T& out);      // 타인 (FIFO top)

private:
    alignas(64) std::atomic<int64_t> m_iBottom{ 0 };
    alignas(64) std::atomic<int64_t> m_iTop{ 0 };
    std::array<T, 4096> m_arrBuf;
};
```

**Chase-Lev 2005 알고리즘**:
- Owner Push/Pop = bottom 쪽 (자주 접근, cache hit)
- Steal = top 쪽 (다른 worker 가 가끔 접근, cache 경쟁 최소)
- `alignas(64)` = false-sharing 방지 (CPU cache line 분리)

---

## §4. Phase 5-B 의 Fiber 추가 — 4 모드

FIBER_JOB_SYSTEM_v2.md 의 핵심 결정: **public API 불변** + **`m_eMode` 로 4 단계 분기**.

```cpp
enum class EMode : uint8_t {
    ThreadOnly = 0,  // Phase 5-A 그대로 (fiber 코드 비활성)
    FiberShell = 1,  // M1: ConvertThreadToFiber 만, 매 job CreateFiber (검증용)
    FiberPool  = 2,  // M2: FiberPool 도입
    FiberFull  = 3,  // M3: + WaitForCounter yield + wait list resume
};

void Initialize(uint32_t iWorkerCount = 0,
                EMode eMode = EMode::ThreadOnly);  // ★ 기본 ThreadOnly = 호환
```

★ **빠른 fallback**: 문제 발생 시 `m_eMode = ThreadOnly` 로 설정만 변경 → fiber 코드 전부 비활성. 구조적 롤백 부담 0.

### 4-1. M1 — Fiber Shell Only (0.5일)

목표: ConvertThreadToFiber + CreateFiber + SwitchToFiber 사이클이 deadlock 없이 작동하는지 검증.

```cpp
void CJobSystem::WorkerLoop(uint32_t iWorkerIdx)
{
    t_iWorkerIdx = (int32_t)iWorkerIdx;

    // ★ M1+: thread → fiber 변환
    if (m_eMode != EMode::ThreadOnly)
        m_vecWorkerCtx[iWorkerIdx].hThreadFiber = ::ConvertThreadToFiber(nullptr);

    while (!m_bShutdown.load()) {
        if (!TryExecuteOneJob(iWorkerIdx))
            std::this_thread::yield();
    }

    if (m_eMode != EMode::ThreadOnly && m_vecWorkerCtx[iWorkerIdx].hThreadFiber) {
        ::ConvertFiberToThread();
    }

    t_iWorkerIdx = -1;
}
```

매 job 마다 CreateFiber/DeleteFiber → ~1μs 오버헤드. M2 에서 pool 화로 ~20ns.

### 4-2. M2 — Fiber Pool (0.5일)

`CFiberPool` 도입. 128 fiber 미리 생성 (8MB), free-stack LIFO 로 재사용.

```cpp
class CFiberPool {
    std::array<CFiber, FIBER_POOL_SIZE> m_arrFibers;       // 128개
    std::array<uint32_t, FIBER_POOL_SIZE> m_arrFreeStack;  // free idx
    uint32_t m_iFreeTop = 0;
    std::atomic<bool> m_bSpinLock{ false };

    uint32_t Acquire();   // free idx pop (LIFO 캐시 친화)
    void Release(uint32_t iIndex);  // free idx push
};
```

`ExecuteItem` 의 fiber 모드 분기:
```cpp
if (bUsePool) {
    uint32_t iFiberIdx = m_pFiberPool->Acquire();
    auto& fiber = m_pFiberPool->GetFiber(iFiberIdx);

    fiber.AssignJob([pSelf, pCounter, fn](){ ... });
    fiber.SetReturnFiber(ctx.hThreadFiber);
    ctx.iCurrentFiber = iFiberIdx;

    ::SwitchToFiber(fiber.GetHandle());  // ★ fiber 로 진입

    // 복귀 — Free (완료) 또는 Waiting (yield)
    if (fiber.GetState() == EFiberState::Free)
        m_pFiberPool->Release(iFiberIdx);
}
```

### 4-3. M3 — Yield + Wait List (★ 가장 중요, 2일)

**Naughty Dog GDC 2015 모델 완성**.

#### 핵심 흐름:

```
Worker 0: fiber A 실행 중
   ↓
fiber A: WaitForCounter(&counter) 호출 (counter 아직 > 0)
   ↓
JobSystem::Fiber_YieldToCounter:
   - Fiber_TryRegisterWait(&counter, 0, A)
   - m_mapWaiters[&counter].push_back(A)
   - fiber A 의 state = Waiting
   - SwitchToFiber(thread_fiber 0)  ← worker 0 의 thread 로 복귀
   ↓
Worker 0: TryExecuteOneJob 다시 → 다른 fiber B 픽업 → 실행
...
(나중에)
Worker 5: 어떤 job 완료 → counter Decrement → 0 도달
   ↓
Fiber_NotifyCounterComplete(&counter):
   - m_mapWaiters[&counter] 의 A 추출 → m_ReadyFibers.push(A)
   ↓
Worker 3: TryExecuteOneJob → ready 큐 우선 검사 → A 픽업
   ↓
SwitchToFiber(A) → fiber A 가 worker 3 에서 resume
   ↓
fiber A: WaitForCounter return → 다음 줄부터 진행
```

★ **Worker 0 에서 시작한 fiber A 가 worker 3 에서 깨어남**. OS thread 입장에서 A 는 wait 동안 그냥 사라진 것 — 다른 fiber 처리 가능.

#### 코드 (FIBER_JOB_SYSTEM_v2.md §3-4 수정 7):

```cpp
// 1) Fiber 가 yield
void CJobSystem::Fiber_YieldToCounter(CJobCounter* pCounter, uint32_t iTarget)
{
    auto& ctx = m_vecWorkerCtx[t_iWorkerIdx];
    uint32_t cur = ctx.iCurrentFiber;

    if (!Fiber_TryRegisterWait(pCounter, iTarget, cur))
        return;  // 이미 target 도달

    auto& fb = m_pFiberPool->GetFiber(cur);
    fb.SetState(EFiberState::Waiting);
    ::SwitchToFiber(ctx.hThreadFiber);  // ★ thread fiber 로 복귀
    fb.SetState(EFiberState::Running);  // 깨어남
}

// 2) JobSystem 내부 wait map (Codex #4 — Counter 안에 wait list X)
bool CJobSystem::Fiber_TryRegisterWait(CJobCounter* pC, uint32_t iTarget, uint32_t idx)
{
    std::lock_guard lk(m_WaiterMutex);
    if (pC->Load() <= iTarget) return false;  // race 재확인
    m_mapWaiters[pC].vecWaitFibers.push_back(idx);
    return true;
}

// 3) Counter 도달 → wait list 깨움
void CJobSystem::Fiber_NotifyCounterComplete(CJobCounter* pCounter)
{
    std::vector<uint32_t> notify;
    {
        std::lock_guard lk(m_WaiterMutex);
        auto it = m_mapWaiters.find(pCounter);
        if (it == m_mapWaiters.end() || pCounter->Load() > it->second.iTarget)
            return;
        notify = std::move(it->second.vecWaitFibers);
        m_mapWaiters.erase(it);  // ★ entry erase — counter destroy 안전
    }

    std::lock_guard lk(m_ReadyMutex);
    for (uint32_t idx : notify) {
        m_pFiberPool->GetFiber(idx).SetState(EFiberState::Ready);
        m_ReadyFibers.push(idx);
    }
}

// 4) Worker 가 ready 큐에서 fiber 픽업 (resume)
bool CJobSystem::Fiber_TryResumeOne(uint32_t iWorkerIdx)
{
    uint32_t resumeIdx;
    {
        std::lock_guard lk(m_ReadyMutex);
        if (m_ReadyFibers.empty()) return false;
        resumeIdx = m_ReadyFibers.front();
        m_ReadyFibers.pop();
    }

    auto& ctx = m_vecWorkerCtx[iWorkerIdx];
    auto& fb = m_pFiberPool->GetFiber(resumeIdx);

    fb.SetReturnFiber(ctx.hThreadFiber);  // ★ resume worker 의 thread fiber
    ctx.iCurrentFiber = resumeIdx;

    ::SwitchToFiber(fb.GetHandle());  // ★ fiber 깨움 — fiber A 의 yield 다음 줄로

    if (fb.GetState() == EFiberState::Free)
        m_pFiberPool->Release(resumeIdx);
    return true;
}
```

### 4-4. M4 — AnimUpdate 병렬화 (0.5일)

CLAUDE.md "추가 2~3ms 절감" 실현. 미니언 60+ + 챔프 10 의 `CAnimator::Update` 묶음 Submit, counter wait.

---

## §5. Naughty Dog GDC 2015 모델 정리

```
┌────────────────────────────────────────────────┐
│            CJobSystem (FiberFull)              │
│                                                │
│  Worker 0    Worker 1    ...    Worker N-1     │ ← OS threads (블로킹 X)
│   (thread)    (thread)            (thread)     │
│      │           │                   │         │
│      ▼           ▼                   ▼         │
│  SwitchToFiber  SwitchToFiber  SwitchToFiber   │
│      │           │                   │         │
│      ▼           ▼                   ▼         │
│  ┌──────────── Fiber Pool ──────────────┐     │
│  │ F0(Run) F1(Wait) F2(Free) ... F127   │     │
│  │ 각 64KB stack + job + state          │     │
│  └──────────────────────────────────────┘     │
│                                                │
│  ┌────── Wait List (m_mapWaiters) ─────┐      │
│  │ counter A → [F1, F5]                │      │
│  │ counter B → [F12]                   │      │
│  └─────────────────────────────────────┘      │
│                                                │
│  ┌────── Ready Queue ──────────────────┐      │
│  │ [F3, F7, F1] (yield 후 깨어난 fiber) │     │
│  └─────────────────────────────────────┘      │
└────────────────────────────────────────────────┘
```

**3 자료구조**:
1. **Worker thread N개** — 절대 블로킹 X
2. **Fiber Pool 128** — 실제 코드 실행 컨텍스트
3. **Wait List + Ready Queue** — fiber 의 진행 상태 박제

---

## §6. 핵심 코드 흐름 4가지 (반복 필독)

### A. ExecuteItem 의 fiber 분기

```
WorkItem 픽업 (Pop / Steal / Global drain)
   ↓
ExecuteItem(item)
   ├─ ThreadOnly 모드 → inline 실행
   └─ FiberPool/FiberFull 모드:
       1) m_pFiberPool->Acquire() → fiber idx
       2) fiber.AssignJob(wrap_lambda)
          (wrap_lambda = job() + counter Decrement + notify)
       3) ctx.iCurrentFiber = idx
       4) SwitchToFiber(fiber)  ← 진입
       5) (fiber 안에서 실행 또는 yield)
       6) 복귀 시 fiber 상태 검사:
          - Free  → Pool 반환
          - Waiting → Pool 안 반환 (notify 후 깨어날 때까지 대기)
```

### B. WaitForCounter 의 yield (M3)

```
WaitForCounter(&counter)
   ├─ Counter == 0 → 즉시 return
   └─ Counter > 0:
       FiberFull 모드 + worker thread + fiber 컨텍스트:
         Fiber_YieldToCounter:
           1) m_mapWaiters[&counter].push(currentFiber)
           2) currentFiber.SetState(Waiting)
           3) SwitchToFiber(thread_fiber)  ← worker 가 다음 일감으로
       (다른 worker 가 깨움)
       4) SwitchToFiber(currentFiber) 진입 → 여기서 재개
       5) WaitForCounter return → 호출자의 다음 줄
```

### C. Fiber resume 시 worker 이동

**시나리오**:
- Worker 0: fiber A 시작 → SwitchToFiber(A)
- A: WaitForCounter → SwitchToFiber(thread_fiber 0)
- Worker 0: 다른 fiber B 처리...
- (나중) 다른 worker 가 counter 0 도달 → A 를 ready queue 로
- Worker 3: ready queue 에서 A 픽업 → `A.SetReturnFiber(thread_fiber 3)` → SwitchToFiber(A)
- A: 깨어남, 코드 진행
- A 완료 → SwitchToFiber(thread_fiber 3) → worker 3 이 다음 일감

**Get_WorkerSlot 함정 (★ 중요)**:
- A 가 yield 전 `auto slot = Get_WorkerSlot();` (= 1, worker 0)
- yield 후 깨어남 → 현재 worker 3 → `Get_WorkerSlot()` 호출하면 4 반환
- A 가 옛 slot (1) 캐시 들고 push → race

→ **yield 가능한 함수 안에서 slot 캐시 금지** (CLAUDE.md gotcha 박제 예정).

### D. JobCounter 와 wait list 분리 (Codex #4)

**v2 v1 안 (폐기됨)**:
```cpp
class CJobCounter {
    std::atomic<uint32_t> m_iCount;
    std::vector<uint32_t> m_arrWaitFibers;  // ★ counter 안에 wait list
    std::mutex m_WaitMutex;
};
```

**문제**: CJobCounter 는 stack 변수 (`CJobCounter counter;`). caller scope 종료 시 destroy. wait list 안에 fiber idx 들어 있으면:
- caller 가 WaitForCounter 누락 → counter destroy 시점에 dangling
- counter 안 mutex + worker 람다 mutex = lock contention

**v2.1 정정**:
```cpp
// CJobCounter 변경 0
class CJobCounter {
    std::atomic<uint32_t> m_iCount;
};

// CJobSystem 안 wait map
class CJobSystem {
    std::mutex m_WaiterMutex;
    std::unordered_map<CJobCounter*, CounterWaitState> m_mapWaiters;
};
```

**Counter destroy 안전성**: WaitForCounter 정상 return = `Fiber_NotifyCounterComplete` 가 entry erase 한 상태. caller scope 종료 시 wait map 에 해당 counter key 없음 → 안전.

---

## §7. Worker-Safety v3 와의 정합성

### 7-1. MinionAI Decision/Apply 2-pass

- DecisionPass: worker 에서 read-only — fiber 안에서 실행돼도 read 만 → 무관
- ApplyPass: main thread single-thread — `t_iWorkerIdx == -1` → fiber 컨텍스트 절대 X

→ **호환 OK**. 단 ApplyPass 안 `WaitForCounter` 호출 시 main 이라 yield 안 함, 기존 help-stealing.

### 7-2. CommandBuffer per-worker buffer

```cpp
void CCommandBuffer::DeferDestroy(EntityID e)
{
    uint32_t slot = CJobSystem::Get_WorkerSlot();  // ★ push 직전 호출
    m_vecDestroysPerWorker[slot].push_back(e);     // ★ 즉시 push (yield 없음)
}
```

push 와 호출 사이 yield 없음 → 안전. 단 push 후 다른 작업 + WaitForCounter 호출 + 다시 push = slot 다를 수 있음 (위양성 없음 — 다른 buffer 에 push 도 정상).

### 7-3. Pathfinder thread_local

```cpp
thread_local std::vector<float> tls_gScore;  // A* 작업 버퍼

std::vector<Cell> Find_Path(...)
{
    tls_gScore.clear();   // 함수 시작
    // A* 실행 (yield 호출 X — 순수 계산)
    return path;
}
```

A* 안에서 yield 호출 안 하니 `tls_gScore` 의 worker 이동 없음 → 안전.

★ **단 yield 가 들어있는 함수 안에서 thread_local 의존 시 위험** — 검토 필수.

---

## §8. 학습 추천 순서 (사용자가 fiber 작업 진입 시)

### Step 1 — 기초 (1시간)
- 본 가이드 §1-§2 정독
- Microsoft Learn — "Fibers" 페이지 검색 + 5 API 함수 docs
- Naughty Dog GDC 2015 video 시청 (60분, YouTube 무료)

### Step 2 — 현 코드 (1시간)
- 본 가이드 §3 + 실제 코드 read:
  - `Engine/Public/Core/JobSystem.h`
  - `Engine/Private/Core/JobSystem.cpp`
  - `Engine/Public/Core/JobSystem/WorkStealingDeque.h`
  - `Engine/Public/Core/JobCounter.h`
- 사고 실험: "WaitForCounter 안에서 thread 가 진짜로 멈춘다면?" (= cv 사용 시 — 5-A 제거됨)

### Step 3 — Phase 5-B 신규 코드 (2시간)
- `.md/plan/engine/FIBER_JOB_SYSTEM_v2.md` 정독 (전 11 섹션)
- 본 가이드 §4-§6 으로 흐름 재확인
- 핵심 트랩: §4-3 의 M3 핵심 흐름 + §6-C resume worker 이동

### Step 4 — 구현 (단계별)
- M0: WORKER_SAFETY_PACKAGE v3 + Set_JobSystem 단계 활성 (이미 별도 패키지)
- M1: ConvertThreadToFiber 만 도입 (0.5일)
- M2: FiberPool (0.5일)
- M3: Wait list + yield (1.5-2일) — ★ 가장 어려운 단계
- M4: AnimUpdate 병렬화 (0.5일)

### Step 5 — 검증
- v2.1 §5 의 6 stress 시나리오 (S1-S6)
- 특히 S3 (Get_WorkerSlot resume) + S6 (Counter destroy 안전성)

---

## §9. 자주 막히는 5 지점

### 1. "FiberProc 가 return 했는데 thread 가 죽었어요"
→ §2-1 — FiberProc 는 무한 루프 안 SwitchToFiber 로 복귀. return 절대 X.

### 2. "DeleteFiber 후 crash"
→ DeleteFiber 는 현재 실행 중 fiber 에 호출 금지. Pool::Shutdown 은 모든 worker 종료 후만.

### 3. "yield 후 깨어났는데 thread_local 값이 다름"
→ §6-C — Fiber 가 다른 worker 에서 resume. thread_local 은 새 worker 의 값. yield 호출 가능 함수 안에서 thread_local 캐시 금지.

### 4. "Counter destroy 후 ghost notify"
→ §6-D — JobSystem 의 m_mapWaiters 가 wait list. WaitForCounter 정상 return = entry erase. caller 가 wait 누락 시 dangling — 단 이건 caller 버그.

### 5. "FiberPool 고갈"
→ §4-2 — Acquire 가 UINT32_MAX 반환. inline fallback 으로 처리 (job 직접 실행).

---

## §10. 한 줄 요약

**Thread (5-A) = OS 가 멈추면 진짜 멈춤. Fiber (5-B) = user-mode 양보. 핵심 = WaitForCounter 가 fiber 면 yield → thread 가 즉시 다른 fiber. Win32 5 API (Convert/Create/Switch/Delete/Get) + Naughty Dog 모델 (Worker N + Pool 128 + Wait List + Ready Queue). v2.1 (Codex 6건) = JobCounter 변경 0 + JobSystem 내부 wait map (counter destroy 안전) + main thread fiber 화 X + Get_WorkerSlot 옵션 A (현행 유지) + 4 모드 (ThreadOnly/Shell/Pool/Full) fallback. M0 (전제) → M1 (Shell, 0.5일) → M2 (Pool, 0.5일) → M3 (yield+wait, 2일) → M4 (Anim 병렬, 0.5일) = 3.5-4일.**
