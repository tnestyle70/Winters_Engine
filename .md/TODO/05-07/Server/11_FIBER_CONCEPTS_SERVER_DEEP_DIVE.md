# Fiber Concepts For Server Deep Dive

> **상태 동기화 (2026-07-11)**: 개념 자료로 보존한다. 현재 Server에는 별도 AcceptThread가 없고, Engine은 `CreateFiber(0)`의 per-job FiberShell만 가지며 pool/waiter/ready/yield-resume는 없다. TickThread의 단순 변환은 mainline 성능 단계가 아니며 최신 적용 경계는 [2026-07-11 UDP/Fiber 통합 감사](../../../plan/2026-07-11_FULL_UDP_AND_SERVER_FIBER_INTEGRATION_AUDIT.md)를 따른다.

작성일: 2026-05-07  
목표: Server Fiber 적용 전에 반드시 이해해야 하는 개념을 Winters 코드 기준으로 잡는다.

---

## 1. Thread, Fiber, Job 의 차이

### Thread

OS가 스케줄링하는 실행 단위다.

```text
Thread = OS가 언제 실행할지 결정하는 stack + register set
```

Server에서 thread는 이미 여러 종류가 있다.

```text
main thread
GameRoom tick thread
IOCP accept thread
IOCP worker threads
CJobSystem worker threads
```

Thread는 OS block이 가능하다. 예를 들어 `GetQueuedCompletionStatus(INFINITE)`는 OS thread를 실제로 잠재운다.

### Fiber

Fiber는 OS thread 위에서 user-mode로 바꿔 타는 실행 문맥이다.

```text
Fiber = OS thread 안에서 엔진이 직접 스케줄링하는 stack + register set
```

Fiber는 OS가 모른다. 그래서 `SwitchToFiber`는 빠르지만, 자동 선점이 없다.  
명시적으로 `SwitchToFiber` 해야만 다른 fiber가 돈다.

### Job

Job은 "실행할 일"이다.

```cpp
std::function<void()> job;
```

Job은 thread일 수도 있고 fiber일 수도 없다. Job은 그냥 함수다.  
Job을 어떤 thread/fiber 위에서 실행할지는 `CJobSystem`이 결정한다.

---

## 2. Fiber 를 왜 쓰는가

ThreadOnly 모델에서도 병렬 실행은 된다.

문제는 job 안에서 다른 job을 기다릴 때다.

```cpp
void ParentJob()
{
    CJobCounter childCounter;
    Submit(ChildA, &childCounter);
    Submit(ChildB, &childCounter);
    WaitForCounter(&childCounter);
    ContinueParent();
}
```

ThreadOnly 에서는 `ParentJob`을 실행하던 worker thread가 wait 동안 묶인다.  
현재 Winters 5-A는 help-stealing으로 다른 job을 돕지만, 이것도 한계가 있다.

FiberFull 에서는 wait 중인 것은 thread가 아니라 fiber다.

```text
Parent fiber:
  WaitForCounter 진입
  wait list 등록
  root worker fiber 로 yield

Worker thread:
  멈추지 않고 즉시 다른 ready fiber/job 실행

Counter 완료:
  Parent fiber 를 ready queue 로 이동
  아무 worker 가 다시 resume
```

핵심:

```text
Fiber 의 목적 = 기다리는 코드를 멈추되, worker thread 는 멈추지 않게 만드는 것
```

---

## 3. Win32 Fiber API

Server/Engine에서 직접 만나는 Win32 API는 거의 5개다.

```cpp
LPVOID ConvertThreadToFiber(LPVOID lpParameter);
BOOL   ConvertFiberToThread();
LPVOID CreateFiber(SIZE_T stackSize, LPFIBER_START_ROUTINE start, LPVOID param);
void   SwitchToFiber(LPVOID fiber);
void   DeleteFiber(LPVOID fiber);
```

### ConvertThreadToFiber

현재 OS thread를 fiber를 실행할 수 있는 root fiber로 바꾼다.

```cpp
LPVOID hRoot = ConvertThreadToFiber(nullptr);
```

규칙:

```text
한 thread 에 한 번만
thread 종료 전 ConvertFiberToThread 권장
이미 fiber 인 thread 에 중복 호출 금지
```

### CreateFiber

별도 stack을 가진 fiber를 만든다.

```cpp
LPVOID hJobFiber = CreateFiber(64 * 1024, FiberProc, param);
```

Server에서 fiber stack size는 보통 64KB부터 시작한다.  
FlatBuffer, 큰 vector, 재귀, pathfinding은 stack 사용량을 따로 봐야 한다.

### SwitchToFiber

현재 fiber를 멈추고 대상 fiber로 이동한다.

```cpp
SwitchToFiber(hJobFiber);
```

`SwitchToFiber` 이후 줄은 대상 fiber가 나중에 다시 이 fiber로 switch 해줘야 실행된다.

### DeleteFiber

실행 중인 fiber는 삭제하면 안 된다.  
반드시 다른 fiber로 돌아온 뒤 삭제한다.

---

## 4. FiberProc 의 핵심 패턴

나쁜 패턴:

```cpp
void CALLBACK FiberProc(void* p)
{
    RunOneJob();
    return; // 위험
}
```

좋은 패턴:

```cpp
void CALLBACK FiberProc(void* p)
{
    while (true)
    {
        WaitUntilAssigned();
        RunAssignedJob();
        MarkComplete();
        SwitchToFiber(returnFiber);
    }
}
```

Win32 fiber proc가 return하면 해당 fiber 실행 문맥이 끝난다.  
pool 재사용 모델에서는 return하지 않고 무한 loop 안에서 job을 교체한다.

---

## 5. Winters 현재 상태와 Fiber 단계

현재 `CJobSystem`은 다음 모드를 갖고 있다.

```cpp
enum class eJobExecutionMode : u8_t
{
    ThreadOnly = 0,
    FiberShell,
};
```

현재 의미:

```text
ThreadOnly:
  worker thread 가 job 을 직접 실행

FiberShell:
  worker thread 를 ConvertThreadToFiber
  job 마다 CreateFiber / SwitchToFiber / DeleteFiber
  WaitForCounter yield 는 아직 없음
```

완전 Fiber server를 위해 필요한 의미:

```text
FiberShell:
  API 연동 / lifecycle 검증용

FiberPool:
  CreateFiber/DeleteFiber 반복 제거

FiberFull:
  WaitForCounter 가 fiber yield
  ready/wait queue 를 JobSystem 내부에서 관리
```

---

## 6. Server 에서 Fiber 를 어디에 적용하나

적용 대상:

```text
CJobSystem worker threads
GameRoom tick 이 submit 하는 simulation jobs
read-only snapshot build jobs
Decision phase jobs
```

비적용 대상:

```text
IOCP WorkerLoop
Accept thread
blocking socket API
main console input loop
```

IOCP는 OS completion queue와 thread wait가 맞물려 있다.  
Fiber는 user-mode cooperative scheduling이다.  
둘을 같은 층으로 섞지 않는다.

---

## 7. WaitForCounter 의 두 세계

### ThreadOnly WaitForCounter

```text
while counter > target:
  if worker thread:
    TryExecuteOneJob()
  else:
    global drain / steal
  if no work:
    std::this_thread::yield()
```

장점:

```text
단순함
OS thread 만 알면 됨
```

단점:

```text
wait 중인 call stack 이 worker thread 를 점유
nested dependency 에 취약
```

### FiberFull WaitForCounter

```text
if current context is worker job fiber:
  register current fiber to counter wait list
  SwitchToFiber(root worker fiber)
else:
  old help-stealing path
```

장점:

```text
worker thread 가 block 되지 않음
deep job dependency 에 강함
```

단점:

```text
더 어려움
fiber-local / thread-local 혼동 위험
debugger stack 이 낯설어짐
```

---

## 8. Thread-local 과 Fiber-local

현재 `CJobSystem`은 thread-local을 쓴다.

```cpp
thread_local std::int32_t t_iWorkerIdx = -1;
thread_local NativeFiberHandle t_hThreadFiber = nullptr;
thread_local bool t_bInsideJobFiber = false;
```

thread-local은 OS thread마다 하나다.  
같은 OS thread 위의 root fiber와 job fiber는 같은 thread-local 값을 본다.

문제:

```text
worker 0 에서 fiber A 시작
fiber A yield
worker 3 에서 fiber A resume
CJobSystem::Get_WorkerSlot() 값이 달라질 수 있음
```

결정:

```text
Server code 는 Get_WorkerSlot / Get_WorkerIdx 를 직접 쓰지 않는다.
per-worker scratch 는 yield 없는 구간에서만 사용한다.
yield 전후로 slot 값을 캐시하지 않는다.
```

---

## 9. Fiber-safe 코드란

Fiber-safe는 thread-safe와 다르다.

### thread-safe

동시에 여러 OS thread가 접근해도 깨지지 않는다.

### fiber-safe

중간에 yield 되어 다른 fiber가 실행되어도 상태가 깨지지 않는다.

나쁜 예:

```cpp
std::lock_guard lk(m_stateMutex);
SubmitJobs();
WaitForCounter(&counter); // fiber yield 가능
```

이 자체가 항상 금지는 아니다.  
하지만 job이 같은 mutex를 잡거나, mutex가 필요한 경로로 들어오면 deadlock이다.

좋은 예:

```cpp
CollectReadOnlyInputs();   // lock 구간 짧게
SubmitEncodeJobs();        // lock 없는 데이터만 사용
WaitForCounter(&counter);
ApplyOutputsSerial();
```

---

## 10. Server Fiber 사고 패턴

### 사고 1. Counter 이중 증가

```cpp
counter.Increment(N);
Submit(job, &counter); // 내부 Increment
```

결과:

```text
counter = 2N
job 완료 후 counter = N
WaitForCounter 영원 대기
```

### 사고 2. Fiber 안 help-execute 가 다른 fiber 를 직접 resume

Fiber A가 `WaitForCounter` 중인데 기존 help-stealing처럼 job B를 실행하고, B가 fiber로 root에 return하면 A의 call stack이 버려질 수 있다.

해결:

```text
Fiber context 에서 WaitForCounter:
  도움실행 금지
  wait list 등록
  root fiber 로 yield
```

### 사고 3. IOCP worker fiber 화

IOCP completion을 소비하는 thread를 user-mode fiber scheduler처럼 쓰면, OS completion queue 소비 타이밍을 추적하기 어려워진다.

해결:

```text
IOCP는 thread model 유지
simulation만 JobSystem/Fiber 대상
```

### 사고 4. CWorld read-only 라고 믿고 바로 병렬 ForEach

`CWorld::GetComponent`는 non-const이고 내부 store 경로가 const로 보장되지 않는다.  
병렬 read는 검증 전에는 가정하지 않는다.

해결:

```text
Stage 1: 직렬 DTO collect
Stage 2: DTO encode 병렬
Stage 3: CWorld const read API 도입 후 world read 병렬
```

---

## 11. 직접 설명할 수 있어야 하는 문장

다음 질문에 답할 수 있으면 개념은 잡힌 것이다.

```text
Q. Fiber와 thread의 차이는?
Q. Fiber를 쓰면 왜 worker thread가 block되지 않는가?
Q. WaitForCounter가 fiber context와 main/thread context에서 달라야 하는 이유는?
Q. IOCP worker를 fiber화하지 않는 이유는?
Q. counter를 왜 Submit 전에 Increment하면 안 되는가?
Q. Get_WorkerSlot 값이 yield 전후에 달라질 수 있는 이유는?
Q. read-only 병렬화와 write-heavy 병렬화의 차이는?
```
