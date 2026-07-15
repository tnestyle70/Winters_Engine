# Engine Fiber Prerequisites And Fixes For Server

> **상태 동기화 (2026-07-11)**: 선결 1 `CJobSystem` DLL export와 선결 3 Main/external→GlobalQueue owner-race 우회는 현재 코드에 적용됐다. FiberShell은 dormant prototype으로 존재하지만 FiberPool과 wait/yield/resume는 없고 Server는 연결되지 않았다. 아래 “현재 관찰”은 2026-05-07 스냅샷이다. Engine 상세는 [JobSystem 상태 감사](../../../plan/2026-07-11_JOB_SYSTEM_CHASE_LEV_FIBER_STATE_AUDIT.md), Server 적용·pinning·RoomIngress 순서는 [통합 감사](../../../plan/2026-07-11_FULL_UDP_AND_SERVER_FIBER_INTEGRATION_AUDIT.md)를 우선한다.
>
작성일: 2026-05-07  
목표: Server Fiber refactor 전에 Engine `CJobSystem`에서 반드시 정리해야 하는 선결 사항을 확정한다.

---

## 1. 왜 Server 문서에 Engine 선결이 필요한가

Server가 Fiber를 직접 구현하는 것이 목표가 아니다.  
Server는 Engine의 `CJobSystem`을 사용해서 job을 submit한다.

따라서 Server Fiber 적용의 안전성은 다음에 의존한다.

```text
CJobSystem 이 Server EXE 에서 링크 가능한가?
Submit / Counter 계약이 명확한가?
Main/external thread submit 이 Chase-Lev race 를 만들지 않는가?
WaitForCounter 가 fiber context 에서 안전하게 yield 하는가?
Get_WorkerSlot 이 fiber resume 상황에서 오해되지 않는가?
```

---

## 2. 선결 1: CJobSystem DLL export

현재 관찰:

```cpp
// Engine/Public/Core/JobSystem.h
class CJobSystem
{
    ...
};
```

`WINTERS_ENGINE` export 매크로가 없다.

Server에서 아래처럼 직접 생성하면:

```cpp
static CJobSystem s_JobSystem;
```

Server는 `CJobSystem::Initialize`, `Shutdown`, 생성자/소멸자 등 외부 심볼을 링크해야 한다.  
`CJobSystem`이 export되지 않으면 unresolved external 가능성이 높다.

### 수정안

```cpp
#pragma once

#include "WintersAPI.h"
...

class WINTERS_ENGINE CJobSystem
{
    ...
};
```

`CJobCounter`는 현재 header-only라 export가 당장 필요 없다.

### 검증

```powershell
msbuild Engine\Include\Engine.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
msbuild Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

합격:

```text
ServerEntry.cpp 에서 CJobSystem static object 생성 가능
Initialize/Shutdown/Submit/WaitForCounter 링크 성공
```

---

## 3. 선결 2: Submit / Counter ownership 계약

현재 `CJobSystem::Submit(job, pCounter)`는 내부에서 counter를 증가시킨다.

```cpp
void CJobSystem::Submit(std::function<void()> job, CJobCounter* pCounter)
{
    if (pCounter)
        pCounter->Increment();
    EnqueueJob(WorkItem{ std::move(job), pCounter });
}
```

따라서 caller는 submit 전에 `Increment()`하면 안 된다.

### Server 규약

```cpp
CJobCounter counter;
for (...)
{
    pJob->Submit([...]()
    {
        ...
    }, &counter);
}
pJob->WaitForCounter(&counter);
```

### 금지

```cpp
CJobCounter counter;
counter.Increment(N);     // 금지
for (...)
    pJob->Submit(job, &counter);
pJob->WaitForCounter(&counter);
```

### 대안이 필요할 때

bulk increment가 필요하면 API 이름을 다르게 만든다.

```cpp
void SubmitAlreadyCounted(std::function<void()> job, CJobCounter* pCounter);
```

이름이 다르면 caller가 실수할 확률이 줄어든다.

---

## 4. 선결 3: Main / external submit race 확인

현재 원하는 `EnqueueJob` 정책:

```text
worker thread 가 submit:
  자기 deque push

main/external/tick thread 가 submit:
  global queue push

worker deque overflow:
  global queue push
```

이 정책은 Chase-Lev owner 규칙을 지킨다.

확인할 코드:

```cpp
if (t_iWorkerIdx >= 0 && static_cast<std::uint32_t>(t_iWorkerIdx) < N)
{
    if (m_vecDeques[t_iWorkerIdx]->Push(item))
        return;
}

{
    std::lock_guard<std::mutex> lk(m_GlobalMutex);
    m_GlobalQueue.push(std::move(item));
}
```

Server의 `CGameRoom::TickThread`는 worker thread가 아니다.  
따라서 Stage 3의 Submit은 global queue로 들어가야 한다.

### 검증

```powershell
rg -n "void CJobSystem::EnqueueJob|t_iWorkerIdx|m_GlobalQueue|Push\\(" Engine/Private/Core/JobSystem.cpp
```

합격:

```text
external submit 이 worker deque 에 직접 push 하지 않음
```

---

## 5. 선결 4: Fiber context 에서 WaitForCounter 도움실행 금지

현재 FiberShell 코드의 위험:

```text
fiber A 가 WaitForCounter 진입
기존 help-stealing 로직으로 TryExecuteOneJob
TryExecuteOneJob 이 fiber B 실행
fiber B 가 root fiber 로 return
fiber A 의 call stack 이 중간에 끊길 수 있음
```

FiberFull 에서 정답:

```text
if current context is job fiber:
  register current fiber to wait list
  switch to root worker fiber
else:
  old help-stealing path
```

즉 Fiber context 안에서는 help execute가 아니라 yield 해야 한다.

### 필요한 상태

```cpp
thread_local bool t_bInsideJobFiber;
thread_local uint32_t t_iCurrentFiber;
thread_local NativeFiberHandle t_hThreadFiber;
```

단 `thread_local`은 fiber-local이 아니다.  
fiber resume 전후로 context를 명시 세팅해야 한다.

---

## 6. 선결 5: FiberPool

현재 FiberShell은 job마다 `CreateFiber/DeleteFiber`를 호출한다.

학습용으로는 OK지만 서버 실사용에는 비효율이다.

필요 구조:

```text
CFiberPool
  Initialize(count, stackSize)
  Acquire()
  Release(fiberIndex)
  Shutdown()

FiberContext
  handle
  state
  assigned WorkItem
  returnFiber
  owner worker/current slot
```

Pool 규칙:

```text
CreateFiber 는 Initialize 시점에만
DeleteFiber 는 Shutdown 시점에만
job 완료 후 fiber 는 Reset 후 free stack 으로 반환
pool 고갈 시 inline fallback 또는 root execute
```

---

## 7. 선결 6: Ready queue / Waiter map

FiberFull 에 필요한 queue:

```text
ready fiber queue:
  wait 가 풀린 fiber 를 worker 가 다시 resume

counter waiter map:
  CJobCounter* -> waiting fiber list
```

권장 구조:

```cpp
struct CounterWaitState
{
    std::vector<u32_t> waitingFibers;
    u32_t target = 0;
};

std::mutex m_WaiterMutex;
std::unordered_map<CJobCounter*, CounterWaitState> m_mapWaiters;

std::mutex m_ReadyMutex;
std::queue<u32_t> m_ReadyFibers;
```

`CJobCounter` 자체에는 wait list를 넣지 않는다.

이유:

```text
CJobCounter 는 stack lifetime 이 많음
wait list mutex 를 counter 안에 넣으면 lifetime/debug 난이도 증가
JobSystem 이 scheduler 이므로 wait ownership 도 JobSystem 이 맞음
```

---

## 8. 선결 7: Get_WorkerSlot 정책

Fiber는 다른 worker에서 resume될 수 있다.

예:

```text
fiber A starts on worker 0
fiber A waits
counter completed
fiber A resumes on worker 3
```

그러면 `Get_WorkerSlot()` 값은 resume 후 worker 3 기준으로 변한다.

Server 규약:

```text
Server code 에서 Get_WorkerSlot 직접 호출 금지
slot 값 캐시 금지
yield 가능한 함수 전후로 per-worker scratch 보존 금지
```

Engine 쪽 per-worker scratch는 다음 둘 중 하나로 결정해야 한다.

| 옵션 | 의미 | 장점 | 단점 |
|---|---|---|---|
| current thread slot | resume 된 worker 기준 | 구현 단순 | yield 전후 slot 변경 |
| fiber origin slot | fiber 시작 worker 기준 | slot 안정 | current worker cache locality와 불일치 |

현재 권장:

```text
current thread slot 유지
yield 가능한 구간에서 slot 캐시 금지
```

---

## 9. 선결 8: throw 안전

현재 `ExecuteItemInline`은 대략 다음 흐름이다.

```cpp
if (item.fn)
    item.fn();
if (item.pCounter)
    item.pCounter->Decrement();
```

job이 throw하면 counter decrement가 누락될 수 있다.

Server job 람다에서는 try/catch를 직접 둔다.

```cpp
pJob->Submit([&]()
{
    try
    {
        DoWork();
        output.bValid = true;
    }
    catch (...)
    {
        output.bValid = false;
    }
}, &counter);
```

Engine 장기 수정:

```cpp
try
{
    if (item.fn)
        item.fn();
}
catch (...)
{
    // debug log
}
if (item.pCounter)
    item.pCounter->Decrement();
```

Counter decrement는 반드시 finally처럼 실행되어야 한다.

---

## 10. Server 진입 전 Engine acceptance

```text
[ ] CJobSystem WINTERS_ENGINE export
[ ] Engine Debug build 통과
[ ] Server 가 CJobSystem static object 링크 가능
[ ] external submit -> global queue 확인
[ ] Submit(job, counter) counter ownership 문서화
[ ] Fiber context WaitForCounter 도움실행 금지 설계 반영
[ ] FiberPool 설계 또는 shell-only stage 명확화
[ ] ready queue / waiter map 설계 반영
[ ] Get_WorkerSlot policy 문서화
[ ] throw-safe counter decrement 계획 반영
```

---

## 11. Server 단계와 Engine 단계의 연결

| Server Stage | 필요한 Engine 상태 |
|---|---|
| ServerEntry lifetime | CJobSystem export |
| Tick Fiber shell | eJobExecutionMode::FiberShell + Win32 fiber shell |
| Snapshot helper split | Engine 변화 필요 없음 |
| Snapshot encode jobs | Submit external -> global queue race fix |
| nested wait stress | FiberFull WaitForCounter yield |
| MinionAI Decision/Apply jobs | FiberFull + Get_WorkerSlot policy |

---

## 12. 결론

Server 코드를 리팩터링하기 전에 Engine JobSystem의 계약을 손에 쥐어야 한다.

특히 다음 세 문장은 외우는 수준이어야 한다.

```text
Submit(job, &counter)는 counter를 내부에서 증가시킨다.
Fiber context 의 WaitForCounter 는 도움실행이 아니라 yield 해야 한다.
Server Tick thread 의 Submit 은 external submit 이므로 global queue 로 들어가야 한다.
```
