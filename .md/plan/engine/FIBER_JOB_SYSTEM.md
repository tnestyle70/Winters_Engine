# Fiber 기반 Job System 구현 계획서

> 작성일: 2026-04-08
> 구현 순서: **Fiber Job System** → Render Graph → GPU Driven Pipeline

---

## 1. 원리 (Principles)

### Fiber vs Thread

**Thread (OS 스레드):** OS가 스케줄링하는 실행 단위. 컨텍스트 스위칭 비용이 크고 (커널 모드 전환, TLB 플러시 등), 수백~수천 개 생성 시 성능 저하가 심하다. 현재 CJobSystem은 Worker Thread에서 `condition_variable::wait()`로 **OS 스레드 자체가 블로킹**된다.

**Fiber (협력적 코루틴):** 유저 모드에서 실행되는 경량 실행 컨텍스트. 스택 + 레지스터 상태만 보존하면 되므로 전환 비용이 극히 낮다 (~20ns vs 스레드 전환 ~1us). 핵심은 **블로킹 없이 양보(yield)** 할 수 있다는 것이다.

### 왜 게임 엔진에 Fiber인가

게임 프레임은 의존성 그래프를 형성한다:
```
Physics ──┐
Animation─┤──→ RenderSubmit ──→ GPU Kick
AI ───────┘
```

Thread Pool 모델의 문제: RenderSubmit이 Physics 완료를 기다릴 때 `counter.Wait()`가 **OS 스레드를 블로킹**한다. 블로킹된 스레드는 CPU를 낭비하고, 12코어 중 일부가 유휴 상태에 빠진다.

Fiber 모델의 해결: RenderSubmit Fiber가 Physics Counter를 기다릴 때, **Fiber만 중단**하고 Worker Thread는 즉시 다른 Fiber를 픽업한다. Worker Thread는 **절대 블로킹되지 않는다.**

### Naughty Dog GDC 2015 모델

핵심 구조:
1. **Worker Thread** (코어 수 - 2개): OS 스레드. 절대 블로킹하지 않음
2. **Fiber Pool** (128~256개): 미리 할당된 Fiber. Job 실행의 실제 컨텍스트
3. **Job Queue**: Worker가 Fiber를 꺼내 실행하는 작업 큐
4. **Counter**: atomic 카운터. Job이 완료되면 감소, 0이 되면 대기 중인 Fiber를 깨움
5. **Wait List**: Counter에 걸려 대기 중인 Fiber 목록. Counter가 0이 되면 Ready Queue로 이동

---

## 2. 핵심 기술 (Core Tech)

### Win32 Fiber API

PCH에 이미 `<Windows.h>`가 포함되어 있으므로 추가 include 불필요:
- `ConvertThreadToFiber(lpParameter)` -- Worker Thread를 Fiber로 변환 (Fiber API 사용을 위한 전제)
- `CreateFiber(dwStackSize, lpStartAddress, lpParameter)` -- 새 Fiber 생성 (스택 크기 지정 가능)
- `SwitchToFiber(lpFiber)` -- 현재 Fiber에서 대상 Fiber로 즉시 전환
- `DeleteFiber(lpFiber)` -- Fiber 삭제
- `GetCurrentFiber()` -- 현재 실행 중인 Fiber 핸들 반환

### Work Stealing (작업 도둑질)

per-worker 큐 + global queue 하이브리드:
- 각 Worker에 자체 로컬 큐 (deque, push/pop은 자기만)
- 자기 큐가 비면 다른 Worker의 큐 뒤쪽에서 steal
- 고우선순위 작업은 global queue에 push
- spinlock 기반 (mutex보다 짧은 critical section에 적합)

### Counter 기반 의존성

```cpp
JobCounter* counter = AllocCounter();
counter->Reset(3);  // 3개 Job이 이 카운터에 연결

Submit(JobA, counter);
Submit(JobB, counter);
Submit(JobC, counter);

WaitForCounter(counter, 0);  // <-- Fiber가 yield, Worker는 다른 Fiber 실행
// 여기부터 JobA/B/C 모두 완료 보장
```

### Fiber Yielding 흐름

```
Worker Thread #0 실행 중: Fiber #5 (RenderSubmit Job)
  ↓
RenderSubmit이 WaitForCounter(physicsCounter, 0) 호출
  ↓
physicsCounter가 아직 0이 아님
  ↓
Fiber #5를 physicsCounter의 Wait List에 등록
  ↓
Fiber #5의 상태를 WAITING으로 변경
  ↓
Worker Thread #0는 Ready Queue에서 다음 Fiber를 꺼내 SwitchToFiber()
  ↓
(나중에 physicsCounter가 0에 도달)
  ↓
Wait List에서 Fiber #5를 꺼내 Ready Queue에 push
  ↓
어떤 Worker Thread가 Fiber #5를 픽업 → SwitchToFiber → RenderSubmit 재개
```

---

## 3. 아키텍처 다이어그램

```
┌──────────────────────────────────────────────────────────────────────┐
│                        CFiberJobSystem                               │
│                                                                      │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐                 │
│  │ Worker #0   │  │ Worker #1   │  │ Worker #N   │  (OS Threads)   │
│  │  ┌───────┐  │  │  ┌───────┐  │  │  ┌───────┐  │                 │
│  │  │LocalQ │  │  │  │LocalQ │  │  │  │LocalQ │  │                 │
│  │  └───┬───┘  │  │  └───┬───┘  │  │  └───┬───┘  │                 │
│  │      │steal │  │      │steal │  │      │steal │                  │
│  │      ▼      │  │      ▼      │  │      ▼      │                  │
│  │ SwitchToFiber│  │ SwitchToFiber│ │ SwitchToFiber│                │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘                 │
│         │                │                │                          │
│  ═══════╪════════════════╪════════════════╪═══════════               │
│         ▼                ▼                ▼                          │
│  ┌──────────────── Global Ready Queue ─────────────────┐            │
│  │  [Fiber#3:Job] [Fiber#7:Job] [Fiber#12:Job] ...     │            │
│  └─────────────────────┬───────────────────────────────┘            │
│                        │                                             │
│  ┌─────────────── Fiber Pool (128개) ──────────────────┐            │
│  │  Fiber#0(FREE)  Fiber#1(RUNNING)  Fiber#2(WAITING)  │            │
│  │  Fiber#3(READY) Fiber#4(FREE)     ...               │            │
│  │  각 Fiber: 64KB 스택 + Job 포인터 + 상태            │            │
│  └─────────────────────────────────────────────────────┘            │
│                                                                      │
│  ┌─────────────── Wait Lists ──────────────────────────┐            │
│  │  Counter#0 (val=2) → [Fiber#2, Fiber#9]             │            │
│  │  Counter#1 (val=0) → [] (완료, 대기 Fiber 없음)     │            │
│  │  Counter#2 (val=1) → [Fiber#11]                     │            │
│  └─────────────────────────────────────────────────────┘            │
│                                                                      │
│  ┌─────────────── Counter Pool (64개) ─────────────────┐            │
│  │  pre-allocated atomic counters + wait list each      │            │
│  └─────────────────────────────────────────────────────┘            │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 4. 파일 목록

### 신규 생성 파일 (13개)

| 파일 경로 | 용도 | 핵심 타입/함수 |
|-----------|------|---------------|
| `Engine/Header/Core/Fiber/FiberTypes.h` | 공통 타입 정의 | `JobEntryPoint`, `FiberHandle`, `EFiberState`, `JobPriority`, `JobDecl` |
| `Engine/Header/Core/Fiber/CFiber.h` | Fiber 래퍼 | `CFiber` (Win32 Fiber 래핑, 상태관리) |
| `Engine/Code/Core/Fiber/CFiber.cpp` | CFiber 구현 | `Create()`, `SwitchTo()`, `Reset()` |
| `Engine/Header/Core/Fiber/CFiberPool.h` | Fiber 풀 | `CFiberPool` (미리 할당, 대여/반납) |
| `Engine/Code/Core/Fiber/CFiberPool.cpp` | CFiberPool 구현 | `Acquire()`, `Release()` |
| `Engine/Header/Core/Fiber/CFiberCounter.h` | Fiber 인식 카운터 | `CFiberCounter` (대기 시 yield, 완료 시 깨움) |
| `Engine/Code/Core/Fiber/CFiberCounter.cpp` | CFiberCounter 구현 | `Decrement()`, `WaitForValue()` |
| `Engine/Header/Core/Fiber/CJobQueue.h` | 작업 큐 | `CJobQueue` (spinlock deque, steal 지원) |
| `Engine/Code/Core/Fiber/CJobQueue.cpp` | CJobQueue 구현 | `Push()`, `Pop()`, `Steal()` |
| `Engine/Header/Core/Fiber/CWorkerThread.h` | Worker 스레드 | `CWorkerThread` (Fiber 전환 루프) |
| `Engine/Code/Core/Fiber/CWorkerThread.cpp` | CWorkerThread 구현 | `ThreadMain()`, `ScheduleNextFiber()` |
| `Engine/Header/Core/Fiber/CFiberJobSystem.h` | 메인 시스템 | `CFiberJobSystem` (전체 조율) |
| `Engine/Code/Core/Fiber/CFiberJobSystem.cpp` | CFiberJobSystem 구현 | `Initialize()`, `Submit()`, `WaitForCounter()`, `Shutdown()` |

### 수정 파일 (8개)

| 파일 경로 | 변경 내용 |
|-----------|----------|
| `Engine/Header/Core/JobSystem.h` | CFiberJobSystem으로 위임하는 래퍼로 전환 |
| `Engine/Header/Core/JobCounter.h` | Fiber 컨텍스트 Wait() 금지 주석 추가 |
| `Engine/Code/Core/JobSystem.cpp` | 빈 번역 단위로 축소 (헤더에서 인라인 래퍼) |
| `Engine/Code/ECS/SystemScheduler.cpp` | CFiberCounter + WaitForCounter 패턴으로 변경 |
| `Engine/Header/Framework/CEngineApp.h` | `CJobSystem m_JobSystem` 멤버 추가 |
| `Engine/Code/Framework/CEngineApp.cpp` | Initialize/Shutdown에 JobSystem 통합 |
| `Engine/Include/Engine.vcxproj` | 신규 .h/.cpp 파일 등록 |
| `Engine/Include/Engine.vcxproj.filters` | `10. JobSystem` 필터에 신규 파일 배치 |

---

## 5. 각 파일 상세 코드

---

### 5-1. `Engine/Header/Core/Fiber/FiberTypes.h` — 공통 타입 정의

```cpp
#pragma once

// ─────────────────────────────────────────────────────────────────
//  FiberTypes.h  |  Fiber Job System 공통 타입
//
//  모든 Fiber 관련 파일이 이것만 include하면 기본 타입을 사용 가능.
//  WintersPCH.h를 통해 Windows.h, atomic 등은 이미 가용.
// ─────────────────────────────────────────────────────────────────

// Fiber 핸들 (Win32 LPVOID)
using FiberHandle = LPVOID;

// Job 진입점 함수 시그니처
// param: 유저가 넘긴 데이터 포인터
using JobEntryPoint = void(*)(void* param);

// Fiber 상태
enum class EFiberState : uint8_t
{
    Free,       // 풀에서 대기 중 (Job 없음)
    Ready,      // Job이 배정되어 실행 대기 중
    Running,    // 현재 Worker가 실행 중
    Waiting,    // Counter 대기로 yield됨
};

// Job 우선순위
enum class EJobPriority : uint8_t
{
    High,       // 프레임 크리티컬 (렌더 서브밋, 입력 처리)
    Normal,     // 일반 게임 로직 (Physics, AI)
    Low,        // 백그라운드 (에셋 스트리밍, 로그)
    Count
};

// Job 선언 구조체
struct JobDecl
{
    JobEntryPoint   pfnEntryPoint = nullptr;    // Job 함수
    void*           pParam        = nullptr;    // Job 파라미터
    EJobPriority    ePriority     = EJobPriority::Normal;
};

// Worker Thread 로컬 컨텍스트 (TLS로 접근)
struct WorkerThreadContext
{
    uint32_t    iWorkerIndex    = 0;        // 이 Worker의 인덱스
    FiberHandle hThreadFiber    = nullptr;  // ConvertThreadToFiber()로 변환한 원래 스레드 Fiber
    uint32_t    iCurrentFiber   = UINT32_MAX;  // 현재 실행 중인 Fiber Pool 인덱스
};

// 상수
constexpr uint32_t FIBER_STACK_SIZE     = 64 * 1024;   // 64KB per Fiber
constexpr uint32_t FIBER_POOL_SIZE      = 128;          // 동시 Fiber 수
constexpr uint32_t COUNTER_POOL_SIZE    = 64;           // 동시 Counter 수
constexpr uint32_t MAX_WAIT_LIST_SIZE   = 16;           // Counter당 최대 대기 Fiber 수
```

---

### 5-2. `Engine/Header/Core/Fiber/CFiber.h` — Fiber 래퍼

```cpp
#pragma once
#include "FiberTypes.h"

// ─────────────────────────────────────────────────────────────────
//  CFiber  |  Win32 Fiber를 래핑하는 RAII 객체
//
//  각 Fiber는 고유 스택(64KB)을 가지며, Job이 배정되면 실행,
//  완료되면 Free 상태로 돌아가 재사용된다.
//  SwitchTo()를 통해 다른 Fiber로 전환할 수 있다.
// ─────────────────────────────────────────────────────────────────

class CFiberJobSystem;  // 전방선언

class CFiber
{
public:
    CFiber() = default;
    ~CFiber();

    // Fiber 생성 (스택 할당, 아직 실행하지 않음)
    // iFiberIndex: FiberPool 내 인덱스 (디버깅 + TLS 역추적용)
    bool Create(uint32_t iFiberIndex);

    // Fiber 파괴 (스택 해제)
    void Destroy();

    // 현재 실행 중인 Fiber에서 이 Fiber로 전환
    void SwitchTo();

    // Job 배정 (Free → Ready)
    void AssignJob(const JobDecl& job);

    // Job 완료 후 Fiber 재사용 (→ Free)
    void Reset();

    // Getter
    uint32_t        GetIndex()  const { return m_iIndex; }
    EFiberState     GetState()  const { return m_eState; }
    FiberHandle     GetHandle() const { return m_hFiber; }
    const JobDecl&  GetJob()    const { return m_Job; }

    // 상태 변경 (JobSystem이 직접 조작)
    void SetState(EFiberState eState) { m_eState = eState; }

    // 이 Fiber가 yield될 때 돌아갈 Worker의 Thread Fiber
    void            SetReturnFiber(FiberHandle h) { m_hReturnFiber = h; }
    FiberHandle     GetReturnFiber() const { return m_hReturnFiber; }

private:
    // Win32 Fiber 진입점 (static, __stdcall 콜백)
    static void CALLBACK FiberProc(LPVOID lpParam);

    FiberHandle     m_hFiber        = nullptr;
    FiberHandle     m_hReturnFiber  = nullptr;      // yield 시 복귀할 Fiber
    uint32_t        m_iIndex        = UINT32_MAX;
    EFiberState     m_eState        = EFiberState::Free;
    JobDecl         m_Job           = {};
};
```

---

### 5-3. `Engine/Code/Core/Fiber/CFiber.cpp` — CFiber 구현

```cpp
#include "WintersPCH.h"
#include "Core/Fiber/CFiber.h"
#include "Core/Fiber/CFiberJobSystem.h"
#include "WintersCore.h"

CFiber::~CFiber()
{
    Destroy();
}

bool CFiber::Create(uint32_t iFiberIndex)
{
    m_iIndex = iFiberIndex;
    m_eState = EFiberState::Free;

    // Win32 Fiber 생성: 스택 64KB, FiberProc이 진입점, this가 파라미터
    m_hFiber = ::CreateFiber(FIBER_STACK_SIZE, &CFiber::FiberProc, this);
    if (!m_hFiber)
    {
        WINTERS_LOG("CFiber::Create failed for index %u (GetLastError=%lu)",
                     iFiberIndex, ::GetLastError());
        return false;
    }

    WINTERS_LOG("CFiber[%u] created (stack=%uKB)", m_iIndex, FIBER_STACK_SIZE / 1024);
    return true;
}

void CFiber::Destroy()
{
    if (m_hFiber)
    {
        // 주의: 현재 실행 중인 Fiber를 DeleteFiber하면 안 됨
        // Pool에서 Shutdown 시에만 호출
        ::DeleteFiber(m_hFiber);
        m_hFiber = nullptr;
    }
    m_eState = EFiberState::Free;
}

void CFiber::SwitchTo()
{
    // 현재 실행 컨텍스트에서 이 Fiber로 전환
    // 호출한 쪽(Worker의 Thread Fiber 또는 다른 Fiber)은 여기서 멈춤
    ::SwitchToFiber(m_hFiber);
}

void CFiber::AssignJob(const JobDecl& job)
{
    m_Job = job;
    m_eState = EFiberState::Ready;
}

void CFiber::Reset()
{
    m_Job = {};
    m_hReturnFiber = nullptr;
    m_eState = EFiberState::Free;
}

// ── Fiber 진입점 (Win32 콜백) ──────────────────────────────────
// Fiber가 최초 SwitchToFiber될 때 여기서 시작한다.
// Job을 실행 → 완료 → Worker Thread Fiber로 복귀 → (무한 반복)
//
// CreateFiber로 만들어진 Fiber는 한 번 return하면 스레드가 종료되므로
// 절대 return하지 않고 무한 루프로 Job을 수행한다.
void CALLBACK CFiber::FiberProc(LPVOID lpParam)
{
    CFiber* pSelf = static_cast<CFiber*>(lpParam);

    while (true)
    {
        // Job 실행
        if (pSelf->m_Job.pfnEntryPoint)
        {
            pSelf->m_eState = EFiberState::Running;
            pSelf->m_Job.pfnEntryPoint(pSelf->m_Job.pParam);
        }

        // Job 완료 → JobSystem에 완료 통보
        // CFiberJobSystem::OnFiberJobComplete()가 카운터 감소 + Fiber Free 처리
        CFiberJobSystem::Get()->OnFiberJobComplete(pSelf->m_iIndex);

        // Worker Thread Fiber로 복귀 (다음 Job을 스케줄링하도록)
        // 이 시점에서 pSelf는 Free 상태이며 Pool로 반환된 상태
        ::SwitchToFiber(pSelf->m_hReturnFiber);

        // ↑ 다음에 이 Fiber가 다시 SwitchToFiber로 진입하면 여기서 재개
        // (AssignJob으로 새 Job이 배정된 상태)
    }
}
```

---

### 5-4. `Engine/Header/Core/Fiber/CFiberPool.h` — Fiber 풀

```cpp
#pragma once
#include "FiberTypes.h"

class CFiber;

// ─────────────────────────────────────────────────────────────────
//  CFiberPool  |  미리 할당된 Fiber 객체 풀
//
//  Initialize 시 FIBER_POOL_SIZE(128)개의 Fiber를 한 번에 생성.
//  Acquire()로 Free 상태 Fiber를 가져오고,
//  Release()로 반납한다.
//  spinlock으로 보호 (acquire/release는 매우 짧은 critical section).
// ─────────────────────────────────────────────────────────────────

class CFiberPool
{
public:
    CFiberPool() = default;
    ~CFiberPool() = default;

    // 풀 초기화: FIBER_POOL_SIZE개 Fiber 생성
    bool Initialize();

    // 풀 정리: 모든 Fiber 파괴
    void Shutdown();

    // Free 상태 Fiber 하나 획득 (없으면 UINT32_MAX 반환)
    // 호출 후 Fiber 상태는 여전히 Free — 호출자가 AssignJob()으로 Ready 전환
    uint32_t Acquire();

    // Fiber를 풀에 반납 (Free 상태로 전환)
    void Release(uint32_t iFiberIndex);

    // 인덱스로 Fiber 접근
    CFiber& GetFiber(uint32_t iIndex) { return m_arrFibers[iIndex]; }
    const CFiber& GetFiber(uint32_t iIndex) const { return m_arrFibers[iIndex]; }

    uint32_t GetPoolSize() const { return FIBER_POOL_SIZE; }

private:
    // 고정 크기 배열 (힙 할당 없음, 캐시 프렌들리)
    array<CFiber, FIBER_POOL_SIZE>  m_arrFibers;

    // Free Fiber 인덱스 스택 (LIFO: 최근 사용된 Fiber 우선 재사용 → 캐시 히트)
    array<uint32_t, FIBER_POOL_SIZE> m_arrFreeStack;
    uint32_t m_iFreeTop = 0;

    // spinlock (mutex 대신: acquire/release가 O(1)이므로)
    atomic<bool> m_bLock{ false };

    void Lock();
    void Unlock();
};
```

---

### 5-5. `Engine/Code/Core/Fiber/CFiberPool.cpp` — CFiberPool 구현

```cpp
#include "WintersPCH.h"
#include "Core/Fiber/CFiberPool.h"
#include "Core/Fiber/CFiber.h"
#include "WintersCore.h"

bool CFiberPool::Initialize()
{
    // 모든 Fiber 생성
    for (uint32_t i = 0; i < FIBER_POOL_SIZE; ++i)
    {
        if (!m_arrFibers[i].Create(i))
        {
            WINTERS_LOG("CFiberPool: Failed to create Fiber[%u]", i);
            Shutdown();
            return false;
        }
        // Free 스택에 push
        m_arrFreeStack[i] = i;
    }
    m_iFreeTop = FIBER_POOL_SIZE;

    WINTERS_LOG("CFiberPool initialized: %u fibers (stack=%uKB each, total=%uKB)",
                FIBER_POOL_SIZE, FIBER_STACK_SIZE / 1024,
                (FIBER_POOL_SIZE * FIBER_STACK_SIZE) / 1024);
    return true;
}

void CFiberPool::Shutdown()
{
    for (uint32_t i = 0; i < FIBER_POOL_SIZE; ++i)
    {
        m_arrFibers[i].Destroy();
    }
    m_iFreeTop = 0;
}

uint32_t CFiberPool::Acquire()
{
    Lock();
    if (m_iFreeTop == 0)
    {
        Unlock();
        WINTERS_LOG("CFiberPool::Acquire FAILED — pool exhausted!");
        return UINT32_MAX;
    }
    --m_iFreeTop;
    uint32_t idx = m_arrFreeStack[m_iFreeTop];
    Unlock();
    return idx;
}

void CFiberPool::Release(uint32_t iFiberIndex)
{
    m_arrFibers[iFiberIndex].Reset();

    Lock();
    m_arrFreeStack[m_iFreeTop] = iFiberIndex;
    ++m_iFreeTop;
    Unlock();
}

void CFiberPool::Lock()
{
    // spinlock: test-and-set
    while (m_bLock.exchange(true, memory_order_acquire))
    {
        // spin하면서 CPU 양보 힌트
        _mm_pause();
    }
}

void CFiberPool::Unlock()
{
    m_bLock.store(false, memory_order_release);
}
```

---

### 5-6. `Engine/Header/Core/Fiber/CFiberCounter.h` — Fiber 인식 카운터

```cpp
#pragma once
#include "FiberTypes.h"

// ─────────────────────────────────────────────────────────────────
//  CFiberCounter  |  Fiber-aware atomic counter
//
//  기존 JobCounter와 달리, Wait 시 OS 스레드를 블로킹하지 않고
//  현재 Fiber를 yield시킨다.
//
//  구조:
//    atomic<uint32_t> 값 + Wait List (대기 중인 Fiber 인덱스 배열)
//    Decrement로 값이 목표치에 도달하면 Wait List의 모든 Fiber를
//    Ready Queue로 이동시킨다.
// ─────────────────────────────────────────────────────────────────

class CFiberCounter
{
public:
    CFiberCounter() = default;
    ~CFiberCounter() = default;

    // 카운터 초기값 설정 (Submit 전 호출)
    void Reset(uint32_t iInitialValue);

    // 카운터 +1 (Job submit 시)
    void Increment();

    // 카운터 -1 (Job 완료 시)
    // 목표치 도달 시 Wait List의 Fiber들을 Ready Queue로 이동
    // 반환값: true면 목표치 도달 (waking fibers)
    bool Decrement();

    // 대기 Fiber 등록 (WaitForCounter 호출 시)
    // 이미 목표치면 false 반환 (등록 불필요, yield 안 함)
    // 등록 성공하면 true 반환 (호출자가 yield해야 함)
    bool AddWaitingFiber(uint32_t iFiberIndex, uint32_t iTargetValue);

    // 현재 값 조회 (non-blocking)
    uint32_t GetValue() const { return m_iValue.load(memory_order_acquire); }

    // 사용 중 여부
    bool IsInUse() const { return m_bInUse.load(memory_order_acquire); }
    void SetInUse(bool b) { m_bInUse.store(b, memory_order_release); }

    // Wait List에서 깨워야 할 Fiber 목록 꺼내기 (Decrement 내부에서 사용)
    // 반환: 깨워야 할 Fiber 인덱스 배열의 개수
    uint32_t FlushWaitList(uint32_t* pOutFiberIndices, uint32_t iMaxOut);

private:
    atomic<uint32_t>    m_iValue{ 0 };
    atomic<bool>        m_bInUse{ false };
    uint32_t            m_iTargetValue = 0;  // 이 값에 도달하면 Wait List 깨움

    // Wait List: 이 카운터를 기다리는 Fiber 인덱스들
    array<uint32_t, MAX_WAIT_LIST_SIZE> m_arrWaitList;
    uint32_t            m_iWaitCount = 0;

    // spinlock
    atomic<bool>        m_bLock{ false };
    void Lock();
    void Unlock();
};
```

---

### 5-7. `Engine/Code/Core/Fiber/CFiberCounter.cpp` — CFiberCounter 구현

```cpp
#include "WintersPCH.h"
#include "Core/Fiber/CFiberCounter.h"
#include "WintersCore.h"

void CFiberCounter::Reset(uint32_t iInitialValue)
{
    m_iValue.store(iInitialValue, memory_order_release);
    m_iWaitCount = 0;
    m_iTargetValue = 0;
}

void CFiberCounter::Increment()
{
    m_iValue.fetch_add(1, memory_order_relaxed);
}

bool CFiberCounter::Decrement()
{
    uint32_t prev = m_iValue.fetch_sub(1, memory_order_acq_rel);
    // prev는 감소 전 값. prev - 1이 현재 값.
    uint32_t newVal = prev - 1;

    if (newVal == m_iTargetValue && m_iWaitCount > 0)
    {
        // 목표치 도달 + 대기 Fiber 존재 → 깨움
        return true;
    }
    return false;
}

bool CFiberCounter::AddWaitingFiber(uint32_t iFiberIndex, uint32_t iTargetValue)
{
    Lock();

    // 이미 목표치에 도달했으면 등록 불필요
    if (m_iValue.load(memory_order_acquire) == iTargetValue)
    {
        Unlock();
        return false;   // yield 안 해도 됨
    }

    m_iTargetValue = iTargetValue;

    if (m_iWaitCount >= MAX_WAIT_LIST_SIZE)
    {
        Unlock();
        WINTERS_LOG("CFiberCounter: Wait list full! Fiber[%u] cannot wait.", iFiberIndex);
        // fallback: busy-wait (바람직하지 않지만 안전)
        return false;
    }

    m_arrWaitList[m_iWaitCount] = iFiberIndex;
    ++m_iWaitCount;

    Unlock();
    return true;    // 호출자는 yield해야 함
}

uint32_t CFiberCounter::FlushWaitList(uint32_t* pOutFiberIndices, uint32_t iMaxOut)
{
    Lock();
    uint32_t count = (m_iWaitCount < iMaxOut) ? m_iWaitCount : iMaxOut;
    for (uint32_t i = 0; i < count; ++i)
    {
        pOutFiberIndices[i] = m_arrWaitList[i];
    }
    // 남은 것들을 앞으로 이동 (count == m_iWaitCount면 전부 비움)
    uint32_t remaining = m_iWaitCount - count;
    for (uint32_t i = 0; i < remaining; ++i)
    {
        m_arrWaitList[i] = m_arrWaitList[count + i];
    }
    m_iWaitCount = remaining;
    Unlock();
    return count;
}

void CFiberCounter::Lock()
{
    while (m_bLock.exchange(true, memory_order_acquire))
    {
        _mm_pause();
    }
}

void CFiberCounter::Unlock()
{
    m_bLock.store(false, memory_order_release);
}
```

---

### 5-8. `Engine/Header/Core/Fiber/CJobQueue.h` — 작업 큐

```cpp
#pragma once
#include "FiberTypes.h"

// ─────────────────────────────────────────────────────────────────
//  CJobQueue  |  Spinlock 기반 Job Deque (Work Stealing 지원)
//
//  각 Worker Thread가 하나씩 소유한다.
//  Push/Pop은 소유자 Worker만 호출 (front).
//  Steal은 다른 Worker가 호출 (back) — 반대 끝에서 꺼냄.
//  Global Queue도 동일 타입 사용 (단, 모든 Worker가 Pop 가능).
// ─────────────────────────────────────────────────────────────────

// Fiber 인덱스 + 연결된 Counter 인덱스를 함께 저장
struct ReadyFiberEntry
{
    uint32_t iFiberIndex    = UINT32_MAX;
    uint32_t iCounterIndex  = UINT32_MAX;   // UINT32_MAX면 카운터 없음
};

class CJobQueue
{
public:
    CJobQueue() = default;
    ~CJobQueue() = default;

    // Queue 초기화
    void Initialize(uint32_t iCapacity = 1024);

    // 소유자가 Job을 넣는다 (front push)
    bool Push(const ReadyFiberEntry& entry);

    // 소유자가 Job을 꺼낸다 (front pop)
    bool Pop(ReadyFiberEntry& outEntry);

    // 다른 Worker가 훔친다 (back pop)
    bool Steal(ReadyFiberEntry& outEntry);

    // 큐가 비어있는지
    bool IsEmpty() const;

    // 현재 크기
    uint32_t Size() const;

private:
    // 링 버퍼 기반 deque
    vector<ReadyFiberEntry> m_arrBuffer;
    uint32_t m_iHead     = 0;   // Push/Pop 위치 (소유자)
    uint32_t m_iTail     = 0;   // Steal 위치 (도둑)
    uint32_t m_iCapacity = 0;
    uint32_t m_iSize     = 0;

    // spinlock
    atomic<bool> m_bLock{ false };
    void Lock();
    void Unlock();
};
```

---

### 5-9. `Engine/Code/Core/Fiber/CJobQueue.cpp` — CJobQueue 구현

```cpp
#include "WintersPCH.h"
#include "Core/Fiber/CJobQueue.h"

void CJobQueue::Initialize(uint32_t iCapacity)
{
    m_iCapacity = iCapacity;
    m_arrBuffer.resize(iCapacity);
    m_iHead = 0;
    m_iTail = 0;
    m_iSize = 0;
}

bool CJobQueue::Push(const ReadyFiberEntry& entry)
{
    Lock();
    if (m_iSize >= m_iCapacity)
    {
        Unlock();
        return false;   // 큐 가득 참
    }
    // Head 앞에 삽입 (front push)
    m_iHead = (m_iHead == 0) ? (m_iCapacity - 1) : (m_iHead - 1);
    m_arrBuffer[m_iHead] = entry;
    ++m_iSize;
    Unlock();
    return true;
}

bool CJobQueue::Pop(ReadyFiberEntry& outEntry)
{
    Lock();
    if (m_iSize == 0)
    {
        Unlock();
        return false;
    }
    // Front pop (LIFO — 최신 Job 우선)
    outEntry = m_arrBuffer[m_iHead];
    m_iHead = (m_iHead + 1) % m_iCapacity;
    --m_iSize;
    Unlock();
    return true;
}

bool CJobQueue::Steal(ReadyFiberEntry& outEntry)
{
    Lock();
    if (m_iSize == 0)
    {
        Unlock();
        return false;
    }
    // Back pop (반대 끝에서 꺼냄 — FIFO 효과)
    uint32_t tailIdx = (m_iHead + m_iSize - 1) % m_iCapacity;
    outEntry = m_arrBuffer[tailIdx];
    --m_iSize;
    Unlock();
    return true;
}

bool CJobQueue::IsEmpty() const
{
    return m_iSize == 0;
}

uint32_t CJobQueue::Size() const
{
    return m_iSize;
}

void CJobQueue::Lock()
{
    while (m_bLock.exchange(true, memory_order_acquire))
    {
        _mm_pause();
    }
}

void CJobQueue::Unlock()
{
    m_bLock.store(false, memory_order_release);
}
```

---

### 5-10. `Engine/Header/Core/Fiber/CWorkerThread.h` — Worker 스레드

```cpp
#pragma once
#include "FiberTypes.h"

class CFiberJobSystem;
class CJobQueue;

// ─────────────────────────────────────────────────────────────────
//  CWorkerThread  |  OS 스레드 하나를 관리
//
//  각 Worker Thread는:
//    1. ConvertThreadToFiber()로 자신을 Fiber로 변환
//    2. 무한 루프:
//       a. Ready Queue에서 Fiber를 꺼냄 (자기 큐 → 글로벌 → 다른 Worker steal)
//       b. SwitchToFiber()로 해당 Fiber 실행
//       c. Fiber가 완료/yield되면 여기로 복귀
//       d. 반복
//    3. Shutdown 시 종료
// ─────────────────────────────────────────────────────────────────

class CWorkerThread
{
public:
    CWorkerThread() = default;
    ~CWorkerThread() = default;

    // Worker 생성 및 시작
    // iIndex: Worker 인덱스
    // pSystem: 소유 JobSystem (Fiber Pool, 글로벌 큐 접근용)
    void Start(uint32_t iIndex, CFiberJobSystem* pSystem);

    // 종료 요청 후 join
    void Stop();

    // 이 Worker의 로컬 큐
    CJobQueue& GetLocalQueue() { return *m_pLocalQueue; }

    uint32_t GetIndex() const { return m_iIndex; }

private:
    // 스레드 진입점
    void ThreadMain();

    // 다음 실행할 Fiber를 찾는다 (자기 큐 → 글로벌 → steal)
    // 찾으면 true + outEntry에 결과
    bool FindNextFiber(ReadyFiberEntry& outEntry);

    uint32_t            m_iIndex    = 0;
    CFiberJobSystem*    m_pSystem   = nullptr;
    thread              m_Thread;
    CJobQueue*          m_pLocalQueue = nullptr;    // 소유하지 않음 (System이 할당)
    atomic<bool>        m_bRunning{ false };
};

// TLS로 현재 Worker 컨텍스트 접근
// (thread_local 변수는 cpp에서 정의)
WorkerThreadContext& GetCurrentWorkerContext();
```

---

### 5-11. `Engine/Code/Core/Fiber/CWorkerThread.cpp` — CWorkerThread 구현

```cpp
#include "WintersPCH.h"
#include "Core/Fiber/CWorkerThread.h"
#include "Core/Fiber/CFiberJobSystem.h"
#include "Core/Fiber/CFiber.h"
#include "Core/Fiber/CFiberPool.h"
#include "Core/Fiber/CJobQueue.h"
#include "WintersCore.h"

// ── TLS: 각 Worker Thread가 자신의 컨텍스트를 보유 ──
static thread_local WorkerThreadContext tls_WorkerContext;

WorkerThreadContext& GetCurrentWorkerContext()
{
    return tls_WorkerContext;
}

void CWorkerThread::Start(uint32_t iIndex, CFiberJobSystem* pSystem)
{
    m_iIndex  = iIndex;
    m_pSystem = pSystem;
    m_pLocalQueue = &pSystem->GetWorkerQueue(iIndex);
    m_bRunning.store(true, memory_order_release);
    m_Thread = thread(&CWorkerThread::ThreadMain, this);
}

void CWorkerThread::Stop()
{
    m_bRunning.store(false, memory_order_release);
    if (m_Thread.joinable())
        m_Thread.join();
}

void CWorkerThread::ThreadMain()
{
    // ── Step 1. 이 OS 스레드를 Fiber로 변환 ──
    // Win32 Fiber API는 ConvertThreadToFiber를 먼저 호출해야
    // SwitchToFiber를 사용할 수 있다.
    FiberHandle hThreadFiber = ::ConvertThreadToFiber(nullptr);
    if (!hThreadFiber)
    {
        WINTERS_LOG("Worker[%u] ConvertThreadToFiber failed (err=%lu)",
                     m_iIndex, ::GetLastError());
        return;
    }

    // ── Step 2. TLS 초기화 ──
    tls_WorkerContext.iWorkerIndex   = m_iIndex;
    tls_WorkerContext.hThreadFiber   = hThreadFiber;
    tls_WorkerContext.iCurrentFiber  = UINT32_MAX;

    WINTERS_LOG("Worker[%u] started (threadId=%u)", m_iIndex, ::GetCurrentThreadId());

    // ── Step 3. 메인 루프 ──
    // Fiber를 찾아서 실행하고, 완료/yield 시 여기로 복귀
    while (m_bRunning.load(memory_order_acquire))
    {
        ReadyFiberEntry entry;
        if (FindNextFiber(entry))
        {
            // Fiber를 실행한다
            CFiber& fiber = m_pSystem->GetFiberPool().GetFiber(entry.iFiberIndex);

            // Fiber에게 복귀할 Fiber(이 Worker의 Thread Fiber)를 알려줌
            fiber.SetReturnFiber(hThreadFiber);
            fiber.SetState(EFiberState::Running);
            tls_WorkerContext.iCurrentFiber = entry.iFiberIndex;

            // ── SwitchToFiber: 이 시점에서 Worker Thread는 멈추고
            //    Fiber가 실행된다. Fiber가 완료되거나 yield하면 여기로 복귀 ──
            fiber.SwitchTo();

            // ── 여기로 복귀 = Fiber가 완료되었거나 yield됨 ──
            tls_WorkerContext.iCurrentFiber = UINT32_MAX;
        }
        else
        {
            // 할 일 없음 — 짧게 spin 후 yield
            // 32번 spin → OS yield (thread 수 > core 수일 때 대비)
            for (int spin = 0; spin < 32; ++spin)
                _mm_pause();
            ::SwitchToThread();  // Win32: 같은 프로세서의 다른 스레드에 양보
        }
    }

    // ── Step 4. 정리 ──
    // ConvertFiberToThread로 원래 스레드로 복원
    ::ConvertFiberToThread();
    WINTERS_LOG("Worker[%u] stopped", m_iIndex);
}

bool CWorkerThread::FindNextFiber(ReadyFiberEntry& outEntry)
{
    // 1순위: 자기 로컬 큐
    if (m_pLocalQueue->Pop(outEntry))
        return true;

    // 2순위: 글로벌 큐 (High priority)
    if (m_pSystem->GetGlobalQueue(EJobPriority::High).Pop(outEntry))
        return true;

    // 3순위: 글로벌 큐 (Normal priority)
    if (m_pSystem->GetGlobalQueue(EJobPriority::Normal).Pop(outEntry))
        return true;

    // 4순위: 다른 Worker에서 steal
    uint32_t workerCount = m_pSystem->GetWorkerCount();
    // 자기 인덱스 다음부터 순회 (편향 방지)
    for (uint32_t i = 1; i < workerCount; ++i)
    {
        uint32_t target = (m_iIndex + i) % workerCount;
        if (m_pSystem->GetWorkerQueue(target).Steal(outEntry))
            return true;
    }

    // 5순위: 글로벌 큐 (Low priority)
    if (m_pSystem->GetGlobalQueue(EJobPriority::Low).Pop(outEntry))
        return true;

    return false;   // 아무것도 없음
}
```

---

### 5-12. `Engine/Header/Core/Fiber/CFiberJobSystem.h` — 메인 시스템

```cpp
#pragma once
#include "FiberTypes.h"

class CFiber;
class CFiberPool;
class CFiberCounter;
class CJobQueue;
class CWorkerThread;

// ─────────────────────────────────────────────────────────────────
//  CFiberJobSystem  |  Fiber 기반 Job System 메인 클래스
//
//  Naughty Dog GDC 2015 모델을 Win32 Fiber API로 구현.
//  모든 Worker Thread는 절대 블로킹하지 않으며,
//  Fiber의 yield/resume으로 의존성을 처리한다.
//
//  사용법:
//    CFiberJobSystem::Get()->Initialize();
//
//    JobDecl jobs[3] = { ... };
//    CFiberCounter* pCounter = nullptr;
//    CFiberJobSystem::Get()->Submit(jobs, 3, &pCounter);
//    CFiberJobSystem::Get()->WaitForCounter(pCounter, 0);
//    // 여기서 3개 Job 모두 완료 보장
//
//    CFiberJobSystem::Get()->Shutdown();
// ─────────────────────────────────────────────────────────────────

class CFiberJobSystem
{
public:
    CFiberJobSystem() = default;
    ~CFiberJobSystem();

    // 싱글톤 접근 (CEngineApp이 소유)
    static CFiberJobSystem* Get() { return s_pInstance; }

    // ── 생명주기 ──
    // iWorkerCount: 0이면 hardware_concurrency - 2
    bool Initialize(uint32_t iWorkerCount = 0);
    void Shutdown();

    // ── Job 제출 ──
    // pJobs: Job 배열, iCount: Job 수
    // ppOutCounter: null이 아니면 새 Counter를 할당하여 반환 (WaitForCounter에 사용)
    void Submit(const JobDecl* pJobs, uint32_t iCount, CFiberCounter** ppOutCounter = nullptr);

    // 단일 Job 제출 (편의 함수)
    void Submit(const JobDecl& job, CFiberCounter** ppOutCounter = nullptr);

    // ── std::function 호환 제출 (기존 API 호환) ──
    // 내부적으로 JobDecl로 변환
    void Submit(function<void()> fnJob);
    void Submit(function<void()> fnJob, CFiberCounter** ppOutCounter);

    // ── Counter 대기 ──
    // Fiber 컨텍스트에서 호출: 현재 Fiber를 yield하고 다른 Fiber 실행
    // 메인 스레드에서 호출: busy-wait (Fiber가 아니므로)
    void WaitForCounter(CFiberCounter* pCounter, uint32_t iTargetValue);

    // ── Counter 반납 ──
    void FreeCounter(CFiberCounter* pCounter);

    // ── 모든 Job 완료 대기 (프레임 동기화용) ──
    void WaitAll();

    // ── 내부 접근 (Worker/Fiber가 사용) ──
    CFiberPool&     GetFiberPool()      { return *m_pFiberPool; }
    CJobQueue&      GetGlobalQueue(EJobPriority ePriority);
    CJobQueue&      GetWorkerQueue(uint32_t iWorkerIndex);
    uint32_t        GetWorkerCount() const { return m_iWorkerCount; }

    // Fiber가 Job 완료 시 호출 (CFiber::FiberProc에서)
    void OnFiberJobComplete(uint32_t iFiberIndex);

private:
    // Counter 풀에서 할당/반환
    CFiberCounter* AllocCounter();

    // 대기 중인 Fiber들을 Ready Queue로 이동
    void WakeWaitingFibers(CFiberCounter* pCounter);

    // Ready Fiber를 적절한 큐에 push
    void EnqueueReadyFiber(const ReadyFiberEntry& entry, EJobPriority ePriority);

    // ── 멤버 ──
    static CFiberJobSystem* s_pInstance;

    unique_ptr<CFiberPool>      m_pFiberPool;
    unique_ptr<CWorkerThread[]> m_pWorkers;
    uint32_t                    m_iWorkerCount = 0;

    // 글로벌 큐: 우선순위별 3개
    unique_ptr<CJobQueue[]>     m_pGlobalQueues;    // [High, Normal, Low]

    // 워커별 로컬 큐
    unique_ptr<CJobQueue[]>     m_pWorkerQueues;

    // Counter 풀
    unique_ptr<CFiberCounter[]> m_pCounterPool;
    array<bool, COUNTER_POOL_SIZE> m_arrCounterUsed = {};
    atomic<bool>                m_bCounterLock{ false };

    // Fiber-to-Counter 매핑 (Fiber가 어떤 Counter에 연결되어 있는지)
    array<uint32_t, FIBER_POOL_SIZE> m_arrFiberCounterMap;

    // 전체 진행 중 Job 수 (WaitAll용)
    atomic<uint32_t> m_iPendingJobs{ 0 };
    // WaitAll용 동기화 (메인 스레드가 Fiber가 아닌 경우)
    mutex               m_mtxWaitAll;
    condition_variable  m_cvWaitAll;

    atomic<bool> m_bShutdown{ false };

    // std::function Job을 위한 래퍼 저장소 (수명 관리)
    // spinlock으로 보호
    struct FuncJobWrapper
    {
        function<void()> fn;
        bool bInUse = false;
    };
    static constexpr uint32_t FUNC_WRAPPER_POOL_SIZE = 512;
    array<FuncJobWrapper, FUNC_WRAPPER_POOL_SIZE> m_arrFuncWrappers;
    atomic<bool> m_bFuncLock{ false };
};
```

---

### 5-13. `Engine/Code/Core/Fiber/CFiberJobSystem.cpp` — CFiberJobSystem 구현

```cpp
#include "WintersPCH.h"
#include "Core/Fiber/CFiberJobSystem.h"
#include "Core/Fiber/CFiber.h"
#include "Core/Fiber/CFiberPool.h"
#include "Core/Fiber/CFiberCounter.h"
#include "Core/Fiber/CJobQueue.h"
#include "Core/Fiber/CWorkerThread.h"
#include "WintersCore.h"

CFiberJobSystem* CFiberJobSystem::s_pInstance = nullptr;

CFiberJobSystem::~CFiberJobSystem()
{
    Shutdown();
}

bool CFiberJobSystem::Initialize(uint32_t iWorkerCount)
{
    if (s_pInstance)
    {
        WINTERS_LOG("CFiberJobSystem already initialized!");
        return false;
    }
    s_pInstance = this;

    // Worker 수 결정
    if (iWorkerCount == 0)
    {
        iWorkerCount = max(1u, thread::hardware_concurrency() - 2);
    }
    m_iWorkerCount = iWorkerCount;

    WINTERS_LOG("CFiberJobSystem::Initialize — workers=%u, fibers=%u, counters=%u",
                m_iWorkerCount, FIBER_POOL_SIZE, COUNTER_POOL_SIZE);

    // ── Fiber Pool 초기화 ──
    m_pFiberPool = make_unique<CFiberPool>();
    if (!m_pFiberPool->Initialize())
    {
        WINTERS_LOG("CFiberJobSystem: FiberPool init failed");
        return false;
    }

    // ── Counter Pool 초기화 ──
    m_pCounterPool = make_unique<CFiberCounter[]>(COUNTER_POOL_SIZE);
    m_arrCounterUsed.fill(false);

    // ── Fiber-Counter 매핑 초기화 ──
    m_arrFiberCounterMap.fill(UINT32_MAX);

    // ── 글로벌 큐 초기화 (3개: High/Normal/Low) ──
    m_pGlobalQueues = make_unique<CJobQueue[]>(static_cast<uint32_t>(EJobPriority::Count));
    for (uint32_t i = 0; i < static_cast<uint32_t>(EJobPriority::Count); ++i)
    {
        m_pGlobalQueues[i].Initialize(1024);
    }

    // ── 워커별 로컬 큐 초기화 ──
    m_pWorkerQueues = make_unique<CJobQueue[]>(m_iWorkerCount);
    for (uint32_t i = 0; i < m_iWorkerCount; ++i)
    {
        m_pWorkerQueues[i].Initialize(256);
    }

    // ── std::function 래퍼 풀 초기화 ──
    for (auto& w : m_arrFuncWrappers)
        w.bInUse = false;

    // ── Worker Thread 시작 ──
    m_pWorkers = make_unique<CWorkerThread[]>(m_iWorkerCount);
    for (uint32_t i = 0; i < m_iWorkerCount; ++i)
    {
        m_pWorkers[i].Start(i, this);
    }

    WINTERS_LOG("CFiberJobSystem initialized successfully");
    return true;
}

void CFiberJobSystem::Shutdown()
{
    if (!s_pInstance)
        return;

    m_bShutdown.store(true, memory_order_release);

    // Worker 종료 대기
    for (uint32_t i = 0; i < m_iWorkerCount; ++i)
    {
        m_pWorkers[i].Stop();
    }

    // Fiber Pool 정리
    if (m_pFiberPool)
        m_pFiberPool->Shutdown();

    m_pFiberPool.reset();
    m_pWorkers.reset();
    m_pGlobalQueues.reset();
    m_pWorkerQueues.reset();
    m_pCounterPool.reset();

    s_pInstance = nullptr;
    WINTERS_LOG("CFiberJobSystem shutdown complete");
}

// ── Job 제출 (배열) ──────────────────────────────────────────────
void CFiberJobSystem::Submit(const JobDecl* pJobs, uint32_t iCount,
                              CFiberCounter** ppOutCounter)
{
    CFiberCounter* pCounter = nullptr;
    uint32_t counterIdx = UINT32_MAX;

    if (ppOutCounter)
    {
        pCounter = AllocCounter();
        pCounter->Reset(iCount);
        *ppOutCounter = pCounter;

        // Counter 인덱스 찾기 (풀 내 오프셋)
        counterIdx = static_cast<uint32_t>(pCounter - m_pCounterPool.get());
    }

    for (uint32_t i = 0; i < iCount; ++i)
    {
        // Fiber Pool에서 Free Fiber 획득
        uint32_t fiberIdx = m_pFiberPool->Acquire();
        if (fiberIdx == UINT32_MAX)
        {
            WINTERS_LOG("Submit: Fiber pool exhausted! Job[%u] dropped.", i);
            if (pCounter)
                pCounter->Decrement();  // 실행 안 된 Job은 카운터에서 빼야 함
            continue;
        }

        // Fiber에 Job 배정
        CFiber& fiber = m_pFiberPool->GetFiber(fiberIdx);
        fiber.AssignJob(pJobs[i]);

        // Fiber-Counter 매핑
        m_arrFiberCounterMap[fiberIdx] = counterIdx;

        // Ready Queue에 push
        m_iPendingJobs.fetch_add(1, memory_order_relaxed);
        ReadyFiberEntry entry;
        entry.iFiberIndex   = fiberIdx;
        entry.iCounterIndex = counterIdx;
        EnqueueReadyFiber(entry, pJobs[i].ePriority);
    }
}

// ── 단일 Job 제출 ──
void CFiberJobSystem::Submit(const JobDecl& job, CFiberCounter** ppOutCounter)
{
    Submit(&job, 1, ppOutCounter);
}

// ── std::function 호환 (기존 CJobSystem API 호환) ──────────────
void CFiberJobSystem::Submit(function<void()> fnJob)
{
    Submit(move(fnJob), nullptr);
}

void CFiberJobSystem::Submit(function<void()> fnJob, CFiberCounter** ppOutCounter)
{
    // function<void()>를 래퍼 풀에 저장하고 static 함수로 변환
    // spinlock
    while (m_bFuncLock.exchange(true, memory_order_acquire))
        _mm_pause();

    uint32_t wrapperIdx = UINT32_MAX;
    for (uint32_t i = 0; i < FUNC_WRAPPER_POOL_SIZE; ++i)
    {
        if (!m_arrFuncWrappers[i].bInUse)
        {
            m_arrFuncWrappers[i].fn = move(fnJob);
            m_arrFuncWrappers[i].bInUse = true;
            wrapperIdx = i;
            break;
        }
    }
    m_bFuncLock.store(false, memory_order_release);

    if (wrapperIdx == UINT32_MAX)
    {
        WINTERS_LOG("Submit(function): wrapper pool exhausted!");
        return;
    }

    // JobDecl로 변환: 진입점은 static 래퍼, param은 인덱스
    JobDecl job;
    job.pfnEntryPoint = [](void* param)
    {
        uint32_t idx = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(param));
        auto* pSystem = CFiberJobSystem::Get();
        pSystem->m_arrFuncWrappers[idx].fn();
        pSystem->m_arrFuncWrappers[idx].fn = nullptr;
        pSystem->m_arrFuncWrappers[idx].bInUse = false;
    };
    job.pParam = reinterpret_cast<void*>(static_cast<uintptr_t>(wrapperIdx));
    job.ePriority = EJobPriority::Normal;

    Submit(job, ppOutCounter);
}

// ── Counter 대기 ────────────────────────────────────────────────
void CFiberJobSystem::WaitForCounter(CFiberCounter* pCounter, uint32_t iTargetValue)
{
    if (!pCounter)
        return;

    // 이미 목표치면 즉시 반환
    if (pCounter->GetValue() == iTargetValue)
        return;

    // 현재 Fiber 컨텍스트에서 호출되었는지 확인
    WorkerThreadContext& ctx = GetCurrentWorkerContext();

    if (ctx.iCurrentFiber != UINT32_MAX)
    {
        // ── Fiber 컨텍스트: yield ──
        // 현재 Fiber를 Wait List에 등록
        bool needYield = pCounter->AddWaitingFiber(ctx.iCurrentFiber, iTargetValue);
        if (needYield)
        {
            // Fiber를 WAITING 상태로 전환
            CFiber& currentFiber = m_pFiberPool->GetFiber(ctx.iCurrentFiber);
            currentFiber.SetState(EFiberState::Waiting);

            // Worker의 Thread Fiber로 복귀 (Worker가 다른 Fiber를 스케줄)
            ::SwitchToFiber(ctx.hThreadFiber);

            // ── 여기로 복귀 = Counter가 목표치에 도달하여 이 Fiber가 깨어남 ──
        }
        // needYield == false: 등록하는 사이에 이미 완료됨, 계속 진행
    }
    else
    {
        // ── 메인 스레드 (비-Fiber): busy-wait ──
        // 메인 스레드는 Fiber가 아니므로 yield할 수 없다.
        // spin-wait + OS yield로 대기
        while (pCounter->GetValue() != iTargetValue)
        {
            for (int i = 0; i < 32; ++i)
                _mm_pause();
            ::SwitchToThread();
        }
    }
}

// ── Counter 반납 ────────────────────────────────────────────────
void CFiberJobSystem::FreeCounter(CFiberCounter* pCounter)
{
    if (!pCounter)
        return;

    uint32_t idx = static_cast<uint32_t>(pCounter - m_pCounterPool.get());
    if (idx >= COUNTER_POOL_SIZE)
        return;

    pCounter->SetInUse(false);

    while (m_bCounterLock.exchange(true, memory_order_acquire))
        _mm_pause();
    m_arrCounterUsed[idx] = false;
    m_bCounterLock.store(false, memory_order_release);
}

// ── WaitAll (프레임 동기화) ─────────────────────────────────────
void CFiberJobSystem::WaitAll()
{
    // 메인 스레드 전용: 모든 pending job이 0이 될 때까지 대기
    while (m_iPendingJobs.load(memory_order_acquire) != 0)
    {
        for (int i = 0; i < 32; ++i)
            _mm_pause();
        ::SwitchToThread();
    }
}

// ── Fiber Job 완료 콜백 ────────────────────────────────────────
void CFiberJobSystem::OnFiberJobComplete(uint32_t iFiberIndex)
{
    // Counter 감소
    uint32_t counterIdx = m_arrFiberCounterMap[iFiberIndex];
    if (counterIdx != UINT32_MAX)
    {
        CFiberCounter& counter = m_pCounterPool[counterIdx];
        bool shouldWake = counter.Decrement();
        if (shouldWake)
        {
            WakeWaitingFibers(&counter);
        }
        m_arrFiberCounterMap[iFiberIndex] = UINT32_MAX;
    }

    // Fiber 반납
    m_pFiberPool->Release(iFiberIndex);

    // Pending 감소
    if (m_iPendingJobs.fetch_sub(1, memory_order_acq_rel) == 1)
    {
        m_cvWaitAll.notify_all();
    }
}

// ── Counter 할당 ────────────────────────────────────────────────
CFiberCounter* CFiberJobSystem::AllocCounter()
{
    while (m_bCounterLock.exchange(true, memory_order_acquire))
        _mm_pause();

    for (uint32_t i = 0; i < COUNTER_POOL_SIZE; ++i)
    {
        if (!m_arrCounterUsed[i])
        {
            m_arrCounterUsed[i] = true;
            m_bCounterLock.store(false, memory_order_release);
            m_pCounterPool[i].SetInUse(true);
            return &m_pCounterPool[i];
        }
    }

    m_bCounterLock.store(false, memory_order_release);
    WINTERS_LOG("AllocCounter: Counter pool exhausted!");
    return nullptr;
}

// ── 대기 Fiber 깨우기 ──────────────────────────────────────────
void CFiberJobSystem::WakeWaitingFibers(CFiberCounter* pCounter)
{
    uint32_t fiberIndices[MAX_WAIT_LIST_SIZE];
    uint32_t count = pCounter->FlushWaitList(fiberIndices, MAX_WAIT_LIST_SIZE);

    for (uint32_t i = 0; i < count; ++i)
    {
        CFiber& fiber = m_pFiberPool->GetFiber(fiberIndices[i]);
        fiber.SetState(EFiberState::Ready);

        ReadyFiberEntry entry;
        entry.iFiberIndex   = fiberIndices[i];
        entry.iCounterIndex = UINT32_MAX;   // 이미 완료된 카운터이므로

        // High priority로 깨어난 Fiber를 넣어 빠르게 재개
        EnqueueReadyFiber(entry, EJobPriority::High);
    }
}

// ── Ready Fiber를 적절한 큐에 push ─────────────────────────────
void CFiberJobSystem::EnqueueReadyFiber(const ReadyFiberEntry& entry, EJobPriority ePriority)
{
    // Worker Fiber 컨텍스트에서 호출되면 로컬 큐에 push
    WorkerThreadContext& ctx = GetCurrentWorkerContext();
    if (ctx.iCurrentFiber != UINT32_MAX)
    {
        // 현재 Worker의 로컬 큐에 push 시도
        if (m_pWorkerQueues[ctx.iWorkerIndex].Push(entry))
            return;
    }

    // 글로벌 큐에 push
    m_pGlobalQueues[static_cast<uint32_t>(ePriority)].Push(entry);
}

// ── 내부 접근 함수 ──────────────────────────────────────────────
CJobQueue& CFiberJobSystem::GetGlobalQueue(EJobPriority ePriority)
{
    return m_pGlobalQueues[static_cast<uint32_t>(ePriority)];
}

CJobQueue& CFiberJobSystem::GetWorkerQueue(uint32_t iWorkerIndex)
{
    return m_pWorkerQueues[iWorkerIndex];
}
```

---

## 6. 기존 코드 수정

### 6-1. `Engine/Header/Core/JobSystem.h` — 하위 호환 래퍼

**수정 전 (L1-L31, 전체):**
```cpp
#pragma once

class JobCounter;

class CJobSystem
{
public:
	CJobSystem();
	~CJobSystem();

	void Initialize(uint32_t threadCount = 0);
	void Shutdown();
	void Submit(function<void()> job);
	void Submit(function<void()> job, JobCounter* counter);
	void WaitAll();
	uint32_t GetThreadCount() const { return static_cast<uint32_t>(m_vecWorkers.size()); }

private:
	void WorkerThread();
	vector<thread> m_vecWorkers;
	queue<function<void()>> m_queJobs;
	mutex m_mtxQueue;
	condition_variable m_cvWork;
	condition_variable m_cvFinished;
	atomic<uint32_t> m_iPendingJobs{ 0 };
	atomic<bool> m_bShutdown{ false };
};
```

**수정 후:**
```cpp
#pragma once
#include "Core/Fiber/CFiberJobSystem.h"
#include "Core/Fiber/CFiberCounter.h"

// ─────────────────────────────────────────────────────────────────
//  CJobSystem  |  기존 API 호환 래퍼
//
//  내부적으로 CFiberJobSystem에 위임한다.
//  기존 코드(SystemScheduler 등)가 수정 없이 동작하도록 함.
//  새 코드는 CFiberJobSystem을 직접 사용할 것을 권장.
// ─────────────────────────────────────────────────────────────────

class JobCounter;  // 하위 호환용 전방선언

class CJobSystem
{
public:
	CJobSystem() = default;
	~CJobSystem() { Shutdown(); }

	void Initialize(uint32_t threadCount = 0)
	{
		m_FiberSystem.Initialize(threadCount);
	}

	void Shutdown()
	{
		m_FiberSystem.Shutdown();
	}

	void Submit(function<void()> job)
	{
		m_FiberSystem.Submit(move(job));
	}

	void Submit(function<void()> job, JobCounter* counter)
	{
		counter->Increment();
		m_FiberSystem.Submit([job = move(job), counter]()
		{
			job();
			counter->Decrement();
		});
	}

	void WaitAll()
	{
		m_FiberSystem.WaitAll();
	}

	uint32_t GetThreadCount() const { return m_FiberSystem.GetWorkerCount(); }

	// 새 API로의 직접 접근
	CFiberJobSystem& GetFiberSystem() { return m_FiberSystem; }

private:
	CFiberJobSystem m_FiberSystem;
};
```

### 6-2. `Engine/Header/Core/JobCounter.h` — 하위 호환

**수정:** 주석만 추가 (Fiber 컨텍스트에서 Wait() 금지 경고)

```cpp
#pragma once

// ─────────────────────────────────────────────────────────────────
//  JobCounter  |  기존 API 호환 래퍼
//
//  내부 동작은 기존과 동일 (OS 스레드 블로킹 Wait).
//  새 코드는 CFiberCounter + WaitForCounter() 패턴을 사용할 것.
//
//  주의: 이 클래스의 Wait()는 OS 스레드를 블로킹한다.
//  Fiber 컨텍스트에서는 CFiberJobSystem::WaitForCounter()를 사용해야 한다.
// ─────────────────────────────────────────────────────────────────

class JobCounter
{
public:
	void Increment()
	{
		m_iCount.fetch_add(1, memory_order_relaxed);
	}

	void Decrement()
	{
		if (m_iCount.fetch_sub(1, memory_order_acq_rel) == 1)
		{
			lock_guard<mutex> lock(m_mutex);
			m_cv.notify_all();
		}
	}

	// 주의: OS 스레드 블로킹. Fiber 컨텍스트에서는 사용 금지.
	void Wait()
	{
		unique_lock<mutex> lock(m_mutex);
		m_cv.wait(lock, [this]
			{
				return m_iCount.load(memory_order_acquire) == 0;
			});
	}

	bool IsComplete() const
	{
		return m_iCount.load(memory_order_acquire) == 0;
	}
private:
	atomic<uint32_t> m_iCount{ 0 };
	mutex m_mutex;
	condition_variable m_cv;
};
```

### 6-3. `Engine/Code/Core/JobSystem.cpp` — 빈 번역 단위

```cpp
#include "WintersPCH.h"
#include "Core/JobSystem.h"

// CJobSystem은 이제 헤더에서 CFiberJobSystem에 위임하는 인라인 래퍼.
// 이 파일은 번역 단위 유지를 위해 남겨둠 (vcxproj에서 제거하지 않도록).
```

### 6-4. `Engine/Code/ECS/SystemScheduler.cpp` — Fiber 인식 대기

**수정 전 (L1-L5, include):**
```cpp
#include "WintersPCH.h"
#include "ECS/SystemScheduler.h"
#include "Core/JobSystem.h"
#include "Core/JobCounter.h"
#include "ECS/World.h"
```

**수정 후 (L1-L8, include):**
```cpp
#include "WintersPCH.h"
#include "ECS/SystemScheduler.h"
#include "Core/JobSystem.h"
#include "Core/JobCounter.h"
#include "Core/Fiber/CFiberJobSystem.h"
#include "Core/Fiber/CFiberCounter.h"
#include "Core/Fiber/FiberTypes.h"
#include "ECS/World.h"
```

**수정 전 (L18-L42, Execute):**
```cpp
void CSystemSchedular::Execute(CWorld & world, float fTimeDelta)
{
	for (auto& [phase, systems] : m_mapPhases)
	{
		if (systems.size() == 1)
		{
			systems[0]->Execute(world, fTimeDelta);
		}
		else
		{
			JobCounter counter;
			for (auto& sys : systems)
			{
				m_pJobSystem->Submit(
					[&sys, &world, fTimeDelta]()
					{
						sys->Execute(world, fTimeDelta);
					}, &counter);
			}
			counter.Wait();
		}
	}
}
```

**수정 후:**
```cpp
void CSystemSchedular::Execute(CWorld & world, float fTimeDelta)
{
	for (auto& [phase, systems] : m_mapPhases)
	{
		if (systems.size() == 1)
		{
			systems[0]->Execute(world, fTimeDelta);
		}
		else
		{
			// Fiber-aware 카운터 사용
			auto* pFiberSystem = CFiberJobSystem::Get();
			if (pFiberSystem)
			{
				vector<JobDecl> jobs;
				jobs.reserve(systems.size());

				struct SystemExecParam
				{
					ISystem* pSystem;
					CWorld* pWorld;
					float fDelta;
				};
				vector<SystemExecParam> params(systems.size());

				for (size_t i = 0; i < systems.size(); ++i)
				{
					params[i] = { systems[i].get(), &world, fTimeDelta };
					JobDecl decl;
					decl.pfnEntryPoint = [](void* p)
					{
						auto* param = static_cast<SystemExecParam*>(p);
						param->pSystem->Execute(*param->pWorld, param->fDelta);
					};
					decl.pParam = &params[i];
					decl.ePriority = EJobPriority::Normal;
					jobs.push_back(decl);
				}

				CFiberCounter* pCounter = nullptr;
				pFiberSystem->Submit(jobs.data(),
					static_cast<uint32_t>(jobs.size()), &pCounter);
				pFiberSystem->WaitForCounter(pCounter, 0);
				pFiberSystem->FreeCounter(pCounter);
			}
			else
			{
				// Fallback: 기존 JobCounter 방식 (CFiberJobSystem 미초기화 시)
				JobCounter counter;
				for (auto& sys : systems)
				{
					m_pJobSystem->Submit(
						[&sys, &world, fTimeDelta]()
						{
							sys->Execute(world, fTimeDelta);
						}, &counter);
				}
				counter.Wait();
			}
		}
	}
}
```

### 6-5. `Engine/Header/Framework/CEngineApp.h`

include에 `#include "Core/JobSystem.h"` 추가, 멤버에 `CJobSystem m_JobSystem;` 추가.

### 6-6. `Engine/Code/Framework/CEngineApp.cpp`

Initialize 끝에 `m_JobSystem.Initialize()` 추가, Shutdown에 `m_JobSystem.Shutdown()` 추가.

---

## 7. vcxproj 변경

### `Engine/Include/Engine.vcxproj`에 추가

```xml
<!-- 소스 파일 ItemGroup -->
<ClCompile Include="..\Code\Core\Fiber\CFiber.cpp" />
<ClCompile Include="..\Code\Core\Fiber\CFiberPool.cpp" />
<ClCompile Include="..\Code\Core\Fiber\CFiberCounter.cpp" />
<ClCompile Include="..\Code\Core\Fiber\CJobQueue.cpp" />
<ClCompile Include="..\Code\Core\Fiber\CWorkerThread.cpp" />
<ClCompile Include="..\Code\Core\Fiber\CFiberJobSystem.cpp" />

<!-- 헤더 파일 ItemGroup -->
<ClInclude Include="..\Header\Core\Fiber\FiberTypes.h" />
<ClInclude Include="..\Header\Core\Fiber\CFiber.h" />
<ClInclude Include="..\Header\Core\Fiber\CFiberPool.h" />
<ClInclude Include="..\Header\Core\Fiber\CFiberCounter.h" />
<ClInclude Include="..\Header\Core\Fiber\CJobQueue.h" />
<ClInclude Include="..\Header\Core\Fiber\CWorkerThread.h" />
<ClInclude Include="..\Header\Core\Fiber\CFiberJobSystem.h" />
```

### `Engine/Include/Engine.vcxproj.filters`에 추가

```xml
<!-- 필터 정의 -->
<Filter Include="10. JobSystem\00. Core">
  <UniqueIdentifier>{F1A00001-0000-0000-0000-000000000001}</UniqueIdentifier>
</Filter>
<Filter Include="10. JobSystem\01. Fiber">
  <UniqueIdentifier>{F1A00002-0000-0000-0000-000000000001}</UniqueIdentifier>
</Filter>
<Filter Include="10. JobSystem\02. Counter">
  <UniqueIdentifier>{F1A00003-0000-0000-0000-000000000001}</UniqueIdentifier>
</Filter>
<Filter Include="10. JobSystem\03. Queue">
  <UniqueIdentifier>{F1A00004-0000-0000-0000-000000000001}</UniqueIdentifier>
</Filter>

<!-- 헤더 파일 배치 -->
<ClInclude Include="..\Header\Core\Fiber\FiberTypes.h">
  <Filter>10. JobSystem\00. Core</Filter>
</ClInclude>
<ClInclude Include="..\Header\Core\Fiber\CFiber.h">
  <Filter>10. JobSystem\01. Fiber</Filter>
</ClInclude>
<ClInclude Include="..\Header\Core\Fiber\CFiberPool.h">
  <Filter>10. JobSystem\01. Fiber</Filter>
</ClInclude>
<ClInclude Include="..\Header\Core\Fiber\CFiberCounter.h">
  <Filter>10. JobSystem\02. Counter</Filter>
</ClInclude>
<ClInclude Include="..\Header\Core\Fiber\CJobQueue.h">
  <Filter>10. JobSystem\03. Queue</Filter>
</ClInclude>
<ClInclude Include="..\Header\Core\Fiber\CWorkerThread.h">
  <Filter>10. JobSystem\01. Fiber</Filter>
</ClInclude>
<ClInclude Include="..\Header\Core\Fiber\CFiberJobSystem.h">
  <Filter>10. JobSystem\00. Core</Filter>
</ClInclude>

<!-- 소스 파일 배치 -->
<ClCompile Include="..\Code\Core\Fiber\CFiber.cpp">
  <Filter>10. JobSystem\01. Fiber</Filter>
</ClCompile>
<ClCompile Include="..\Code\Core\Fiber\CFiberPool.cpp">
  <Filter>10. JobSystem\01. Fiber</Filter>
</ClCompile>
<ClCompile Include="..\Code\Core\Fiber\CFiberCounter.cpp">
  <Filter>10. JobSystem\02. Counter</Filter>
</ClCompile>
<ClCompile Include="..\Code\Core\Fiber\CJobQueue.cpp">
  <Filter>10. JobSystem\03. Queue</Filter>
</ClCompile>
<ClCompile Include="..\Code\Core\Fiber\CWorkerThread.cpp">
  <Filter>10. JobSystem\01. Fiber</Filter>
</ClCompile>
<ClCompile Include="..\Code\Core\Fiber\CFiberJobSystem.cpp">
  <Filter>10. JobSystem\00. Core</Filter>
</ClCompile>
```

---

## 8. 의존성 순서

### Include 의존성 그래프 (빌드 순서)

```
FiberTypes.h          (의존: Windows.h, stdint — PCH에서 제공)
    ↑
CFiber.h              (의존: FiberTypes.h)
CFiberPool.h          (의존: FiberTypes.h, CFiber 전방선언)
CFiberCounter.h       (의존: FiberTypes.h)
CJobQueue.h           (의존: FiberTypes.h)
    ↑
CWorkerThread.h       (의존: FiberTypes.h, CFiberJobSystem 전방선언)
    ↑
CFiberJobSystem.h     (의존: FiberTypes.h, 나머지 전방선언)
    ↑
JobSystem.h (래퍼)    (의존: CFiberJobSystem.h)
    ↑
CEngineApp.h          (의존: JobSystem.h)
SystemScheduler.cpp   (의존: CFiberJobSystem.h, CFiberCounter.h)
```

### 구현 순서 (단계별)

1. **Step 1**: `FiberTypes.h` 작성 (기반 타입)
2. **Step 2**: `CFiber.h/.cpp` 작성 (Win32 Fiber 래핑)
3. **Step 3**: `CFiberPool.h/.cpp` 작성 (Fiber 풀)
4. **Step 4**: `CFiberCounter.h/.cpp` 작성 (Fiber 인식 카운터)
5. **Step 5**: `CJobQueue.h/.cpp` 작성 (Job 큐)
6. **Step 6**: `CWorkerThread.h/.cpp` 작성 (Worker 스레드)
7. **Step 7**: `CFiberJobSystem.h/.cpp` 작성 (전체 조율)
8. **Step 8**: `JobSystem.h`, `JobCounter.h`, `JobSystem.cpp` 수정 (하위 호환 래퍼)
9. **Step 9**: `CEngineApp.h/.cpp` 수정 (초기화/종료 통합)
10. **Step 10**: `SystemScheduler.cpp` 수정 (Fiber 인식 대기)
11. **Step 11**: `Engine.vcxproj`, `Engine.vcxproj.filters` 수정 (빌드 등록)

---

## 9. 테스트 계획

### 9-1. 단위 테스트 (우선순위순)

1. **Fiber 생성/삭제 테스트**: CreateFiber/DeleteFiber가 정상 동작하는지, 128개 동시 생성 확인
2. **Fiber Pool Acquire/Release 테스트**: 모두 꺼내고, 반납하고, 다시 꺼내기. Exhaustion 시 UINT32_MAX 반환 확인
3. **CJobQueue Push/Pop/Steal 테스트**: 1000개 push, pop으로 전부 소진, steal로 전부 소진, 멀티스레드 race condition 없는지
4. **CFiberCounter Decrement + WakeList 테스트**: 3개 Fiber 등록 후 카운터 0 도달 시 3개 모두 깨어나는지
5. **CFiberJobSystem Submit + WaitAll 테스트**: 100개 Job submit, WaitAll, 모든 Job이 실행되었는지

### 9-2. 통합 테스트

1. **SystemScheduler 병렬 실행**: 4개 Phase에 8개 System, 모두 정상 실행 + 순서 보장
2. **Counter 의존성 체인**: JobA(physics) → WaitForCounter → JobB(render)가 정확한 순서로 실행
3. **Fiber Yield/Resume**: Job 내부에서 WaitForCounter 호출, 다른 Job이 그 사이에 실행되는지 확인
4. **Stress 테스트**: 10000개 Job, 다양한 의존성, 12코어 전부 활용되는지 프로파일

### 9-3. 검증 방법

- **디버그 로그**: WINTERS_LOG로 Fiber 생성/전환/완료 추적
- **CPU 프로파일**: Task Manager 또는 Intel VTune으로 코어 활용률 확인 (목표: 80%+)
- **동시성 검증**: ThreadSanitizer 또는 수동 race condition 코드 리뷰
- **데드락 검증**: Fiber가 yield 없이 mutex를 잡고 있지 않은지 확인

### 9-4. 주의사항

- **Fiber-unsafe 패턴 금지**: Fiber 내에서 `std::mutex::lock()` 후 yield하면 데드락. Fiber 컨텍스트에서는 spinlock 또는 lock-free만 사용
- **thread_local 주의**: Fiber는 OS 스레드가 아니므로, Fiber 전환 시 thread_local 값이 바뀌지 않는다. Worker Thread 컨텍스트는 TLS에, Fiber 컨텍스트는 Fiber 객체에 저장
- **스택 오버플로우**: 64KB 스택이 부족한 Job이 있으면 FIBER_STACK_SIZE를 128KB로 조정하거나, 별도 large-stack Fiber 풀 운영
