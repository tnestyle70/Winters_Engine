# Fiber Job System 학습 가이드 — 기초 원리부터 Winters 코드 베이스 적용까지

> **현행 정본 (2026-07-13)**: `CJobSystem`은 `ThreadOnly`, `FiberShell`, `FiberFull` 세 실행 모드를 구현했고 전용 stress가 세 모드, Submit/Shutdown 경합, Chase-Lev 마지막 원소 경합, nested wait, 예외/overflow/drain, FiberFull FLS interleave와 pool saturation을 통과했다. 기본 모드는 여전히 `ThreadOnly`이며 `FiberFull`은 명시적 opt-in이다. 서버는 세 job mode를 CLI로 연결했고 FiberFull startup probe에서 실제 parent-child wait/resume `1/1`을 확인했으며 TCP·UDP·dual F5 smoke를 통과했다. 다만 GameRoom 실 workload jobification과 speedup은 완료 주장이 아니고, 별도의 **Fiber 6주 mastery 프로그램은 미착수**다.
>
> **작성일**: 2026-05-03
> **목적**: Win32 Fiber API, Chase-Lev publication, Job lifecycle, stackful continuation과 Winters의 현행 구현을 한 흐름으로 이해한다. 과거 `FIBER_JOB_SYSTEM_v2.md`는 설계 진화 비교용이다.
> **참조**:
> - 현 코드: [Engine/Public/Core/JobSystem.h](Engine/Public/Core/JobSystem.h), [Engine/Private/Core/JobSystem.cpp](Engine/Private/Core/JobSystem.cpp), [Engine/Public/Core/JobSystem/WorkStealingDeque.h](Engine/Public/Core/JobSystem/WorkStealingDeque.h), [Engine/Public/Core/JobCounter.h](Engine/Public/Core/JobCounter.h)
> - 신규 박제: [.md/plan/engine/FIBER_JOB_SYSTEM_v2.md](.md/plan/engine/FIBER_JOB_SYSTEM_v2.md) (v2.1, Codex 검토 6건 반영)
> - 외부 자료: Naughty Dog GDC 2015 "Parallelizing the Naughty Dog Engine Using Fibers" (Christian Gyrling)

### 2026-07-13 as-built 계약

| 축 | 실제 구현 | 본질 |
|---|---|---|
| 제출 객체 | heap `WorkItem`을 만든 뒤 deque/global queue에는 immutable `WorkItem*` token만 publish | 함수 객체를 복사·이동 중인 슬롯을 thief가 읽지 못하게 하고, CAS로 소유권을 얻은 단 한 실행자만 reclaim |
| Submit/Shutdown | `BeginSubmission`/`EndSubmission` admission lease + lifecycle mutex/CV | Shutdown이 deque/fiber 상태를 파괴하기 전에 이미 들어온 Submit/외부 Wait의 publish 또는 inline 실행 완료를 기다림 |
| Chase-Lev | worker당 고정 4096개의 `std::atomic<T>` 슬롯, owner bottom push/pop, thief top steal | 빠른 owner 경로와 희소한 steal 경로를 분리; full이면 global queue로 fallback |
| 마지막 원소 | Pop이 CAS 전 `top`을 `iLastIndex`로 저장하고 CAS 뒤 bottom을 `iLastIndex + 1`로 복구 | 실패한 CAS가 expected 값을 thief의 새 top으로 덮어쓰는 C++ API 성질 때문에, 변형된 expected로 bottom을 복구하면 generation을 건너뜀 |
| 모드 | `ThreadOnly`, `FiberShell`, `FiberFull` | baseline, per-job fiber 경로, pooled stackful continuation을 분리해 fallback과 비교 검증 가능 |
| FiberFull pool | worker마다 정확히 64 fibers; `CreateFiberEx(64 KiB commit, 256 KiB reserve, FIBER_FLAG_FLOAT_SWITCH)` | commit은 초기 실제 메모리 부담, reserve는 stack 주소 공간 상한; pool 생성이 하나라도 실패하면 startup fail-closed |
| wait/resume | counter별 waiter map + **origin worker 전용** 64-slot ready ring | Win32 fiber를 다른 thread에서 재개하지 않고, 시작한 worker의 root fiber로만 돌아가 thread-affinity를 보존 |
| local state | worker identity는 TLS, fiber를 따라가야 하는 submission/profiler context는 FLS | 같은 worker에서 여러 parked fiber가 interleave하므로 TLS만으로는 fiber별 call-chain을 구분할 수 없음 |
| 검증 | `Tools/Harness/RunJobSystemStress.ps1 -Mode all` PASS | 실행 수·counter exactness·last-item uniqueness·wait/resume parity·pool miss fallback을 회귀 게이트로 고정 |

`FiberFull`이 해결하는 것은 "스레드를 더 많이 만든다"가 아니다. 대기 중인 **call stack의 continuation**을 fiber에 보존하고 root scheduler로 돌아와 같은 OS worker가 다른 ready work를 수행하게 하는 것이다. 반대로 IOCP의 `GetQueuedCompletionStatus`처럼 OS completion을 기다리는 전용 thread를 fiber로 바꾸는 것은 이 모델의 목표가 아니다.

> **역사 자료 읽기 규칙**: 아래 §3~§6의 2026-05/07-11 코드 조각 중 `128개 global pool`, `cross-worker resume`, `FiberFull 미구현` 표기는 당시 설계 진화 기록이다. 현행 구현 판단에는 이 as-built 표와 문서 끝 canonical code pointers만 사용한다.

---

## §0. 한 단락 — 무엇을 만들고 왜

**한 줄**: Job의 대기 중 call stack을 worker thread에서 분리해, worker가 다른 ready work를 수행할 수 있게 만드는 시스템.

**비유**: 음식점 주방.
- **ThreadOnly 모델 (현재)**: 셰프 8명 (= worker thread). 기다리는 parent의 call stack을 유지한 채 다른 일을 help-execute할 수는 있지만, 그 parent와 worker의 수명은 계속 묶여 있다.
- **FiberFull 모델 (현행 opt-in)**: 셰프 8명 + worker별 작업 카드 64장 (= fiber). wait를 만난 카드의 stack을 보존해 wait list에 놓고, 그 카드를 시작한 같은 셰프가 ready inbox에서 다시 집는다.

게임 프레임 = 60 FPS = 16.67ms. 8 코어 활용 못 하면 1 코어가 병목 → frame drop. Fiber 가 핵심.

---

## §1. 기초 원리 — Thread vs Fiber

### 1-1. OS Thread (현재 ThreadOnly runtime)

| 특징 | 비용 |
|---|---|
| OS kernel 이 스케줄링 | 전환 비용은 CPU·부하·동일 process 여부에 따라 달라지며 고정 수치나 매번 TLB flush로 단정할 수 없음 |
| `condition_variable::wait()` | 진짜 OS thread 블로킹 (CPU 사이클 0 사용) |
| thread마다 stack/TLS/커널 스케줄 상태 | 수가 늘면 stack·cache·scheduler 비용 증가 |
| 외부 시그널 / I/O 가능 | Windows에서는 IOCP, event, condition variable 등 OS 이벤트와 통합 |

**한계**: Worker thread 가 `WaitForCounter` 호출 시 진짜로 block 되거나 (cv 사용 시) busy-wait (현 5-A help-stealing). 둘 다 코어를 효율적으로 쓰지 못함.

### 1-2. Fiber (현행 opt-in FiberFull)

| 특징 | 비용 |
|---|---|
| **User-mode** 에서 협력적 스케줄링 | kernel thread switch 없이 fiber context를 저장/복원. 실제 비용은 반드시 대상 머신에서 측정 |
| `SwitchToFiber()` = 명시적 양보 (yield) | OS 모름 — 100% user-mode |
| fiber마다 독립 stack/context | stack reserve/commit 크기와 pool 수만큼 가상/물리 메모리 비용 |
| OS 시그널 / I/O 통합 X | Worker thread 가 OS 와 통신 — fiber 는 user-mode 만 |

**핵심 능력**: Fiber 는 **자기 진행 중인 코드를 멈추고 다른 fiber 로 양보 가능**. 이 양보는 **OS thread 를 멈추지 않음** — thread 는 즉시 다른 fiber 픽업.

### 1-3. 비교 표

| 측면 | Thread | Fiber |
|---|---|---|
| 스케줄러 | OS kernel | 게임 엔진 (user code) |
| 컨텍스트 스위치 | OS scheduler와 thread context | user-mode fiber context; 배수는 측정값으로만 주장 |
| 블로킹 | OS 이벤트 (cv/mutex/IO) | 명시적 yield (SwitchToFiber) |
| stack | OS thread stack | fiber별 stack; 크기는 API/설정에 따름 |
| 동시 N | core 병렬성의 실제 운반체 | N개 thread 위에 M개 continuation을 multiplex |
| 생성 비용 | thread/stack/kernel bookkeeping | fiber object/stack 생성; 그래서 pool 재사용이 필요 |
| 디버깅 | OS 도구 (PerfView/ETW) | 추가 도구 필요 |

---

## §2. Win32 Fiber API — 5 함수만 알면 됨

```cpp
#include <Windows.h>

// 1. Thread → Fiber 변환 (한 번만, worker 시작 시)
LPVOID hThreadFiber = ConvertThreadToFiberEx(nullptr, FIBER_FLAG_FLOAT_SWITCH);

// 2. 새 fiber 생성 (스택 할당 + entry point 박제)
LPVOID hNewFiber = CreateFiberEx(
    64 * 1024,           // initial commit (64 KiB)
    256 * 1024,          // reserve (256 KiB)
    FIBER_FLAG_FLOAT_SWITCH,
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

## §3. 역사적 구현 스냅샷 — 2026-07-11 이전 (현행 구현 판단에 사용 금지)

이 절은 Submit race를 임시 우회하던 시기의 구조를 보존한다. 현재는 `WorkItem*` publication, admission lease, 세 모드와 FiberFull scheduler가 구현되어 있으므로 정확한 API/필드는 문서 앞 as-built 표와 canonical code를 따른다.

### 3-1. 클래스 구조 (`Engine/Public/Core/JobSystem.h`)

```cpp
class CJobSystem
{
public:
    void Initialize(uint32_t iWorkerCount = 0);
    void Shutdown();
    void SetExecutionMode(eJobExecutionMode mode);

    void Submit(std::function<void()> job);
    void Submit(std::function<void()> job, CJobCounter* pCounter);
    void WaitForCounter(CJobCounter* pCounter, uint32_t iTarget = 0);

    static int32_t Get_WorkerIdx();    // main = -1, worker = [0, N)
    static uint32_t Get_WorkerSlot();  // 모든 non-worker/external = 0, worker = idx+1

private:
    void WorkerLoop(uint32_t iWorkerIdx);
    bool TryExecuteOneJob(uint32_t iWorkerIdx);
    void ExecuteItem(WorkItem& item);
    void EnqueueJob(WorkItem&& item);

    std::vector<std::thread>   m_vecWorkers;
    std::vector<std::unique_ptr<CWorkStealingDeque<WorkItem>>>  m_vecDeques;
    std::queue<WorkItem>       m_GlobalQueue;
    std::mutex                 m_GlobalMutex;
    std::condition_variable    m_WakeCV;
};
```

**3 핵심 자료구조**:
1. `m_vecWorkers` — N개 OS thread (hardware_concurrency - 2)
2. `m_vecDeques` — 각 worker 의 LIFO deque (자기 push/pop) + 다른 worker 가 steal 가능
3. `m_GlobalQueue` — main/외부 thread와 local deque overflow의 fallback queue

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

    if (GetExecutionMode() == eJobExecutionMode::FiberShell && !IsThreadAFiber())
        t_hThreadFiber = ConvertThreadToFiber(nullptr);

    while (!m_bShutdown.load()) {
        if (!TryExecuteOneJob(iWorkerIdx)) {
            std::unique_lock lk(m_WakeMutex);
            m_WakeCV.wait_for(lk, std::chrono::milliseconds(1));
        }
    }

    if (t_hThreadFiber && IsThreadAFiber())
        ConvertFiberToThread();
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

## §4. 역사적 M1→M4 설계 진화 기록 (현행 아님)

아래 `128 fiber`와 global ready queue/cross-worker resume 코드는 채택 전 설계다. 실제 코드는 worker별 64 fiber와 origin-pinned ready ring을 사용한다.

2026-07-11 감사 당시 코드의 실행 mode는 둘뿐이었다.

```cpp
enum class eJobExecutionMode : uint8_t {
    ThreadOnly = 0,
    FiberShell,
};
```

`FiberPool`과 `FiberFull`은 당시 `FIBER_JOB_SYSTEM_v2.md`의 목표 단계 이름이었고 enum/구현에 없었다. 이는 역사적 판정이다. 2026-07-13 현재 enum과 runtime에는 `FiberFull`이 구현됐고 Server CLI로 명시적 opt-in할 수 있으며, 안전한 기본값은 계속 `ThreadOnly`다.

### 4-1. M1 — Fiber Shell Only (골격 구현, runtime dormant, stress 미검증)

당시 구현: ConvertThreadToFiber + per-job CreateFiber/SwitchToFiber/DeleteFiber 골격. 빌드와 DLL import는 확인됐지만 실제 opt-in stress는 없었다.

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

매 job 마다 CreateFiber/DeleteFiber와 stack lifecycle 비용을 낸다. 구체 비용은 아직 계측되지 않았으며 M2 pool의 목적이 이 반복 생성/삭제를 없애는 것이다.

### 4-2. M2 — Fiber Pool (미구현 목표)

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

## §5. 역사적 개념 스케치 — 외부 모델을 Winters에 투영한 초안

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

## §6. 역사적 설계 흐름 4가지 (현행은 origin-pinned)

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

### C. Fiber resume의 현행 계약 — origin worker 고정

**실제 시나리오**:
- Worker 0이 fiber A를 자기 pool에서 acquire하고 실행한다.
- A가 `WaitForCounter`에서 Waiting이 되어 worker 0의 root fiber로 돌아간다.
- 어느 worker든 counter를 target까지 낮출 수 있지만, notifier는 A를 **owner worker 0의** 고정 ready ring에 넣는다.
- Worker 0이 `TryResumeReadyFiber(0)`에서 A를 꺼내 동일 native thread에서 재개한다.
- A가 끝나면 worker 0의 pool로 반환된다.

따라서 현행에서는 yield 전후 `Get_WorkerSlot()`이 바뀌지 않는다. 그래도 thread-affine lock, OS wait, 소켓 호출의 in-flight 상태를 FiberFull wait 너머로 들고 가는 것은 금지다. 또한 "같은 thread"와 "같은 fiber-local call chain"은 다르다. 같은 worker에서 sibling fibers가 교차 실행되므로 submission/profiler처럼 continuation에 귀속된 context는 FLS로 보존한다.

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

push와 호출 사이 yield가 없어 안전하다. 현행 FiberFull은 origin worker로 재개되므로 worker slot도 유지된다. 단 per-fiber 의미의 상태를 per-worker buffer 하나에 숨기면 sibling fiber interleave로 논리 충돌할 수 있으므로, continuation 소유 상태는 stack/FLS/명시적 job context 중 하나에 둔다.

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

A* 안에서 yield 호출이 없고 job 하나가 해당 worker를 연속 점유하므로 `tls_gScore` 재진입이 없어 안전하다.

★ origin pinning은 thread 이동만 막는다. 같은 worker의 다른 fiber가 중간에 실행될 수 있으므로, yield 구간을 가로질러 mutable TLS scratch를 독점한다고 가정하면 안 된다. fiber별 의미가 필요한 값은 FLS 또는 fiber/job 소유 상태여야 한다.

---

## §8. 학습 추천 순서 (사용자가 fiber 작업 진입 시)

### Step 1 — 기초 (1시간)
- 본 가이드 §1-§2 정독
- Microsoft Learn — "Fibers" 페이지 검색 + 5 API 함수 docs
- Naughty Dog GDC 2015 video 시청 (60분, YouTube 무료)

### Step 2 — 현 코드 (1시간)
- 문서 앞 as-built 계약 + 실제 코드 read:
  - `Engine/Public/Core/JobSystem.h`
  - `Engine/Private/Core/JobSystem.cpp`
  - `Engine/Public/Core/JobSystem/WorkStealingDeque.h`
  - `Engine/Public/Core/JobCounter.h`
- 사고 실험: "WaitForCounter 안에서 thread 가 진짜로 멈춘다면?" (= cv 사용 시 — 5-A 제거됨)

### Step 3 — 현행 FiberFull 코드 추적 (2시간)
- `InitializeFiberWorker` → `TryExecuteItemOnFiberFull` → `SuspendCurrentFiber` → `NotifyCounterChanged` → `TryResumeReadyFiber` 순서로 실제 코드를 읽는다.
- 역사적 설계 문서와 비교해 global pool/cross-worker resume가 왜 worker-local pool/origin pinning으로 바뀌었는지 설명한다.
- 핵심 트랩: last-item CAS expected mutation, lost-wakeup 재검사, FLS submission chain, shutdown admission boundary.

### Step 4 — 구현 상태와 남은 적용
- Engine runtime: `ThreadOnly`/`FiberShell`/`FiberFull`, pool/waiter/ready ring/FLS와 lifecycle race fix 구현 완료.
- Stress: 세 모드, nested/overflow/exception/drain, Submit/Shutdown, external Wait/Shutdown, last-item 10만 경합, FLS interleave, 80-parent pool saturation PASS.
- 제품 적용: 기본값은 `ThreadOnly`; server startup self-test와 실제 server workload fan-out은 별도 통합/측정 대상이다.
- **Fiber 6주 mastery**: 개념→Windows API lab→scheduler 재구현→프로파일링→문서/포트폴리오 프로그램은 미착수다. 코드 기능 완료와 사람의 숙련 완료를 혼동하지 않는다.

### Step 5 — 검증
- `Tools/Harness/RunJobSystemStress.ps1 -Mode all`
- `uSubmitted == uExecuted`, failure/exception counter exactness, `uFiberWaits == uFiberResumes`, saturation 시 `uFiberPoolMisses > 0`을 함께 본다.
- 제품 빌드는 stress와 별개다. Engine/Server/Client 빌드와 server binary의 opt-in startup smoke를 각각 증거로 남긴다.

---

## §9. 자주 막히는 5 지점

### 1. "FiberProc 가 return 했는데 thread 가 죽었어요"
→ §2-1 — FiberProc 는 무한 루프 안 SwitchToFiber 로 복귀. return 절대 X.

### 2. "DeleteFiber 후 crash"
→ DeleteFiber 는 현재 실행 중 fiber 에 호출 금지. Pool::Shutdown 은 모든 worker 종료 후만.

### 3. "같은 worker인데 submission/profiler context가 섞여요"
→ §6-C — 현행 fiber는 origin worker에 고정되지만 sibling fiber가 같은 TLS를 번갈아 본다. continuation별 context는 FLS/stack/job-owned state로 둔다.

### 4. "Counter destroy 후 ghost notify"
→ §6-D — JobSystem 의 m_mapWaiters 가 wait list. WaitForCounter 정상 return = entry erase. caller 가 wait 누락 시 dangling — 단 이건 caller 버그.

### 5. "FiberPool 고갈"
→ worker별 64개 pool이 고갈되면 `Acquire()`가 null을 반환하고 해당 job은 allocation-free inline fallback으로 실행된다. 이 경로에서는 nested wait가 fiber suspend가 아니라 help-execute이므로 계측의 `fiber_pool_misses`를 반드시 본다.

---

## §10. 한 줄 요약

**현재 Winters는 immutable `WorkItem*` publication과 lifecycle admission 위에 Chase-Lev, `ThreadOnly`/`FiberShell`/`FiberFull`, worker-local 64-fiber pool, origin-pinned wait/resume, FLS context를 구현했고 전용 stress를 통과했다. Fiber의 본질은 전환 속도 자랑이 아니라 기다리는 call stack을 보존한 채 OS worker를 다른 ready work에 재사용하는 것이다. 기본 모드는 계속 `ThreadOnly`이며 서버 제품 배선·실 workload speedup과 별도의 6주 mastery는 독립적으로 검증해야 한다.**

## Canonical code pointers

- Job lifecycle/public contract: `Engine/Public/Core/JobSystem.h`, `Engine/Private/Core/JobSystem.cpp`
- Chase-Lev token deque: `Engine/Public/Core/JobSystem/WorkStealingDeque.h`
- Counter contract: `Engine/Public/Core/JobCounter.h`
- Fiber types/pool: `Engine/Public/Core/Fiber/FiberTypes.h`, `Engine/Public/Core/Fiber/FiberPool.h`
- Executable regression proof: `Tools/Harness/JobSystemStress.cpp`, `Tools/Harness/RunJobSystemStress.ps1`
