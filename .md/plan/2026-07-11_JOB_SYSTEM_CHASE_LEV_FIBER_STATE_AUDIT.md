Session - 2026-07-11 JobSystem · Chase-Lev Deque · Fiber 실제 상태 동기화와 CS/Windows 구현 해설

> [!IMPORTANT]
> **Historical baseline / as of 2026-07-11.** 아래 본문을 현재 구현 상태로 사용하지 않는다. 최신 기준은 [2026-07-13 canonical implementation plan](2026-07-13_UDP_JOB_SYSTEM_CHASE_LEV_FIBER_IMPLEMENTATION_PLAN.md)과 [S023 결과 보고서](../build/2026-07-13_UDP_JOB_SYSTEM_CHASE_LEV_FIBER_RESULT.md)다.
> As-built delta: JobSystem Submit race, Chase-Lev deque, FiberFull 및 stress 구현은 완료되었고, UDP v3 generic vertical slice와 server hub/client facade가 구현되었다. main F5 통합과 최종 build 상태는 S023 결과 보고서를 따른다. 6주 Fiber mastery 프로그램은 미착수이며, 현재 상태는 production UDP cutover가 아니다.
> 과거 UDP v2 수치인 **24 B header / 10 B fragment header / 1 MiB logical payload**는 historical design이다. 실제 v3 상수는 **40 B header / 16 B fragment header / 1,200 B datagram / 64 KiB logical payload**다.

검증 기준: 2026-07-11 현재 workspace. 이 문서는 4~5월의 구현 계획과 회고를 지우지 않고, 현재 코드가 어디까지 들어와 있는지를 별도로 판정한다. 코드가 바뀌면 과거 계획서의 줄 번호가 아니라 아래의 심볼과 검증 명령으로 다시 확인한다.

## 0. 결론

| 항목 | 현재 판정 | 근거 |
|---|---|---|
| Client/Engine JobSystem | **활성** | `CGameInstance::Initialize_Engine`가 항상 생성/초기화하고 Loader, ECS scheduler, Transform, local-only Navigation/UnitAI가 사용한다. |
| 2026-04-23 Main-push Submit race | **정식 우회 구현됨** | worker만 자기 deque의 bottom에 Push하고 main/외부/overflow는 mutex global queue로 간다. |
| Chase-Lev 검증 수준 | **MVP 구현, 전용 stress/formal proof 없음** | 고정 4096 슬롯과 C++ atomic/fence 구현은 있으나 1만 fan-out/nested-wait 전용 실행 프로젝트는 없다. |
| FiberShell | **컴파일된 dormant prototype** | Win32 변환/생성/전환/삭제 코드는 있으나 기본값은 `ThreadOnly`, `SetExecutionMode(FiberShell)` 호출자는 0이다. |
| FiberPool / FiberFull | **미구현** | pool acquire/release, waiter map, ready queue, counter wait yield/resume, cross-worker resume가 없다. |
| Server Job/Fiber 통합 | **stub / 런타임 미연결** | `CServerEntry`는 프로젝트에는 들어가지만 mode 설정·반환·Shutdown이 stub이고 `main` 호출자가 없다. |
| Fiber 6주 mastery 프로그램 | **미착수** | 6주는 학습·실험·외부 산출물 프로그램이다. 이미 존재하는 FiberShell M0와는 다른 축이다. |

따라서 현재 상태를 한 문장으로 부르면 다음이 정확하다.

> **Winters Client는 race 우회가 적용된 ThreadOnly JobSystem을 실제 사용한다. Win32 FiberShell M0는 소스에 있으나 런타임에서 켜지지 않으며, wait를 양보하는 FiberFull과 Server 통합, 전용 stress/계측은 아직 시작 전이다.**

## 1. 세 개를 분리해야 본질이 보인다

### 1.1 JobSystem은 “일의 표현과 배분”이다

Job은 실행해야 할 작은 work packet이다. 보통 함수/데이터, 완료 counter, 디버그 이름을 가진다. JobSystem은 다음 네 책임을 가진다.

1. work를 제출한다.
2. ready work를 worker들에 배분한다.
3. idle worker가 다른 worker의 work를 가져오게 한다.
4. 여러 job의 완료와 의존성을 표현한다.

본질은 **함수 호출 중심 control flow를 work와 dependency 중심 dataflow로 바꾸는 것**이다. `for` 루프 하나를 무조건 잘게 쪼개는 장치가 아니다. 작업이 충분히 크고, 서로 독립이며, 완료 barrier가 명확할 때만 병렬 이득이 난다.

Windows의 **Job Object**는 여러 process를 한 단위로 제한/종료/회계하는 커널 객체다. 게임 엔진의 JobSystem과 이름만 같고 다른 개념이다.

### 1.2 Chase-Lev deque는 “ready work의 한 저장 방식”이다

각 worker가 deque 하나를 소유한다.

```text
thief들                                      owner 하나
   Steal(top)  <── 오래된/굵은 work ... 최신 work ──>  Push/Pop(bottom)
       FIFO                                                    LIFO
```

- owner의 `Push/Pop(bottom)`은 단일 writer 가정 덕분에 대부분 relaxed access로 빠르게 동작한다.
- thief의 `Steal(top)`은 다른 thief 및 owner와 경쟁하므로 CAS가 선형화 지점이 된다.
- owner가 최신 work를 LIFO로 소비하면 방금 만진 데이터가 cache에 남을 가능성이 크다.
- thief가 오래된 work를 가져가면 보통 더 큰 subtree를 훔쳐 병렬 slack을 확보하기 쉽다.

중요한 점은 **single-owner가 최적화 팁이 아니라 정확성 전제**라는 것이다. Main이 임의 worker의 bottom에 Push하면 “조금 느려지는” 것이 아니라 알고리즘이 증명하던 프로그램 자체가 아니게 된다.

### 1.3 Fiber는 “병렬 실행 장치”가 아니라 “대기 continuation”이다

Fiber는 별도 stack과 중단 지점을 가진 stackful user-mode execution context다. OS가 worker thread를 더 만들어 주는 것이 아니다.

```text
N개의 OS worker thread        : 실제 병렬성의 상한
M개의 fiber (보통 M > N)      : 실행/대기 중인 continuation 수
N:M 스케줄링                  : ready fiber를 N개 thread 위에 올림
```

순수 CPU job이 아무것도 기다리지 않고 끝난다면 fiber는 일을 더 빠르게 만들지 않는다. 오히려 switch와 stack 관리 비용이 추가된다. Fiber의 가치는 parent job이 child counter를 기다릴 때 나타난다.

```text
ThreadOnly help-stealing
parent call stack가 worker를 계속 소유
WaitForCounter → 그 worker가 다른 job을 대신 실행 → parent로 복귀

FiberFull
parent call stack 전체를 fiber stack에 보존
WaitForCounter → waiter 등록 → root scheduler fiber로 yield
worker는 다른 ready fiber/job 실행
counter 도달 → parent fiber를 ready queue로 이동 → 나중에 resume
```

즉 Fiber의 본질은 **대기 중인 call stack과 물리 worker thread의 수명을 분리하는 것**이다.

## 2. 동시성의 CS 본질

### 2.1 Concurrency, parallelism, asynchrony

- concurrency: 여러 작업의 수명이 겹치고 진행 순서를 조정해야 하는 성질.
- parallelism: 실제로 여러 core에서 같은 시각에 실행되는 성질.
- asynchrony: 결과를 지금 즉시 받지 않고 나중에 완료 통지를 받는 인터페이스 성질.

JobSystem은 세 성질을 함께 쓸 수 있지만 동일어는 아니다. Fiber 하나를 추가해도 worker thread가 하나면 CPU 병렬성은 하나다.

### 2.2 Fork-join과 counter

Winters의 `CJobCounter`는 남은 job 수를 나타낸다.

```text
Submit(job, &counter)  → counter + 1
job 완료               → counter - 1
WaitForCounter(c, 0)   → 0 이하가 될 때 join 완료
```

Counter는 단순 숫자이면서 동시에 **의존성이 충족됐다는 증명서**다. Stack-local counter를 raw pointer로 WorkItem에 넣으므로 caller는 모든 job이 끝날 때까지 counter 수명을 보장해야 한다. 예외, 취소, 조기 return이 decrement/wait 계약을 깨면 use-after-free 또는 영원한 wait가 될 수 있다.

### 2.3 Memory ordering

원자성은 “찢어지지 않는다”만 보장하는 것이 아니다. 다른 메모리의 publish 순서를 함께 설계해야 한다.

Winters deque의 의미는 다음과 같다.

- Push는 buffer slot에 work를 먼저 쓰고, release fence 뒤에 `bottom` 증가를 공개한다.
- Steal은 `top/bottom`을 읽고 slot을 복사한 뒤 `top` CAS에 성공한 thief만 work 소유권을 얻는다.
- Pop과 Steal이 마지막 한 개를 놓고 경쟁할 때 `top` CAS 승자만 실행한다.
- `alignas(64)`는 `top`과 `bottom`을 다른 cache line에 두어 false sharing을 줄인다.

`memory_order_seq_cst`를 많이 썼다고 자동으로 올바른 것이 아니며, relaxed를 썼다고 자동으로 틀린 것도 아니다. **소유권 전제, publish 순서, 선형화 지점, slot 재사용 수명**을 하나의 증명으로 봐야 한다.

또한 Winters 전체 JobSystem은 “완전 lock-free”가 아니다. Per-worker deque 경로가 lock-free 성격을 가지지만 external submission/global drain에는 mutex가 있고 idle worker에는 condition variable이 있다. 이 혼합은 의도된 correctness baseline이다.

### 2.4 Race, deadlock, livelock, starvation

- data race: 동기화되지 않은 동일 메모리 접근 중 하나 이상이 write. C++에서는 undefined behavior다.
- deadlock: 서로가 가진 자원을 기다리는 cycle 때문에 누구도 진행 못 한다.
- livelock: thread는 계속 움직이지만 유용한 상태 전이가 없다.
- starvation: 전체 시스템은 진행하지만 특정 job/fiber가 계속 선택되지 않는다.

과거 Submit 사고의 표면 증상은 Main의 `WaitForCounter` 무한 대기였지만, 핵심 원인은 mutex cycle이 아니라 owner 규약을 깬 deque 접근과 work 유실 가능성이었다. “hang = deadlock”으로 단정하면 진단이 빗나간다.

## 3. Winters의 실제 ThreadOnly 경로

```text
CGameInstance::Initialize_Engine
  └ CJobSystem 생성 → Initialize(hw_concurrency - 2, 최소 1)
       ├ worker thread N
       ├ worker별 CWorkStealingDeque<WorkItem>
       └ GlobalQueue + wake condition_variable

Client submitter
  ├ Loader CPU load
  ├ CSystemSchedular의 conflict-free system batch
  ├ TransformSystem root range
  ├ local-only NavigationSystem agent
  └ local-only LocalUnitAISystem decision

EnqueueJob
  ├ caller가 worker owner → self deque Push
  └ main/external/overflow → GlobalQueue lock + push

Worker consume
  1. self Pop
  2. GlobalQueue pop
  3. random victim 한 명 Steal
  4. 없으면 wake CV에서 최대 1ms 대기

WaitForCounter
  ├ worker caller → 자기/전역/steal help-execute
  └ main/external → global drain 후 round-robin steal
```

### 현재 활성의 직접 증거

- `Engine/Private/GameInstance.cpp`: JobSystem을 무조건 생성하고 `Initialize(0)` 한다.
- `Client/Private/Scene/Scene_InGameLifecycle.cpp`: scheduler와 Transform에 주입하고, 비-network-authoritative local 경로에서는 Navigation/UnitAI에도 주입한다.
- `Client/Private/Scene/Loader.cpp`: CPU load를 JobSystem에 submit한다.
- `Profiles/profiler_20260711_210619.json`: `Transform::RootCount=90`, `Transform::SubmittedRootJobs=12`가 기록됐고 raw Transform events가 12개 worker thread id에 분산돼 실제 parallel submit 경로가 실행됐다. 같은 capture의 scheduler batch 자체는 `ParallelBatches=0`이므로 “스케줄러 전체가 병렬이었다”로 과장하면 안 된다.

## 4. 과거 Submit race와 현재 수정

과거 경로는 Main이 round-robin으로 worker deque의 bottom에 직접 Push했다. 그러나 bottom은 worker owner만 조작해야 한다. Worker Pop과 Main Push가 같은 bottom을 건드리면서 Chase-Lev의 전제가 깨졌고, work가 소비되지 않으면 counter가 0이 되지 않아 Main이 영원히 기다릴 수 있었다.

현재 `CJobSystem::EnqueueJob`은 호출 경로 자체를 분리한다.

```cpp
if (caller_is_worker_owner)
{
    if (selfDeque.Push(item))
        return;
}

lock(globalQueue);
globalQueue.push(item); // main, external, overflow
```

이것은 ordering 숫자를 임의로 강화한 수정이 아니라 **비-owner가 bottom에 도달할 수 없게 만든 소유권 수정**이다. 따라서 “Submit race로 현재 임시 비활성”은 stale 문구다.

다만 “그 race의 원인이 제거됐다”와 “deque 구현 전체가 모든 interleaving에서 증명됐다”는 다른 주장이다. 현재 전용 stress executable, model checking, sanitizer 기반 검증은 없다. 특히 fixed circular slot에 non-trivial `WorkItem(std::function)`을 by-value로 저장·복사하는 구현은 wrap-around와 지연된 thief의 slot 재사용까지 별도 검토할 가치가 있다. 이번 감사에서는 실제 실패로 재현하지 않았으므로 **확정 버그가 아니라 검증 부채**로 분류한다.

## 5. Windows Fiber 언어로 번역

### 5.1 API 흐름

Win32에서 fiber는 application이 수동 스케줄한다.

```cpp
root = ConvertThreadToFiber(nullptr);  // 현재 worker thread를 root fiber로
job  = CreateFiber(stackSize, Entry, context);
SwitchToFiber(job);                    // root 중단, job stack/register 복원
// job이 SwitchToFiber(root)하면 이 줄 다음에서 재개
DeleteFiber(job);                      // 실행 중이 아닌 fiber만 삭제
ConvertFiberToThread();                // worker 종료 전 원복
```

`SwitchToFiber`는 OS scheduler에게 새 thread를 요청하는 호출이 아니다. 현재 fiber의 stack pointer와 보존 대상 register/context를 저장하고 대상 fiber context를 복원한다. Windows 문서상 다른 thread에서 만든 fiber도 적절한 동기화 아래 다른 thread가 schedule할 수 있다. 이것이 FiberFull의 cross-worker resume 기반이다.

Fiber entry가 return하면 그 fiber를 실행 중인 thread가 종료되므로 정상 경로는 scheduler/root fiber로 명시적으로 switch해야 한다. 실행 중 fiber를 삭제해도 안 된다.

### 5.2 TLS와 FLS

`thread_local`/TLS는 fiber-local이 아니다. Fiber가 worker 0에서 worker 3으로 옮겨 resume하면 그 코드가 보는 `thread_local`은 worker 3의 값이다. Fiber별 지속 context가 필요하면 다음 중 하나가 필요하다.

- scheduler가 current fiber context pointer를 switch 전후 명시적으로 세팅
- Win32 FLS (`FlsAlloc/FlsSetValue/FlsGetValue`) 사용
- fiber object 안에 state를 두고 모든 hot path가 명시적으로 전달

현재 Winters는 cross-worker resume 자체가 없으므로 `t_iWorkerIdx`가 ThreadOnly/FiberShell에서 worker identity로 동작한다. FiberFull을 추가할 때는 `Get_WorkerSlot`을 origin identity로 오해하면 per-slot buffer race가 열린다.

### 5.3 현재 x64와 floating-point context

현재 vcxproj는 x64만 제공한다. Windows의 `ConvertThreadToFiberEx/CreateFiberEx`는 `FIBER_FLAG_FLOAT_SWITCH`를 제공하며 Microsoft 문서는 flag 0의 floating-point state 손상 주의를 x86에 명시한다. 현재 x64-only 빌드에 곧바로 실패 판정을 내릴 근거는 아니지만, Win32 target을 추가하거나 context 보존 정책을 일반화할 때 기본 API와 Ex API 선택을 검증해야 한다.

## 6. 현재 FiberShell이 하는 일과 하지 않는 일

현재 mode enum은 둘뿐이다.

```cpp
enum class eJobExecutionMode : u8_t
{
    ThreadOnly = 0,
    FiberShell,
};
```

FiberShell을 명시적으로 켰다고 가정하면 worker는 root fiber가 되고, 각 WorkItem마다 `CreateFiber(0)` → `SwitchToFiber` → job 실행/완료 → root 복귀 → `DeleteFiber`를 수행한다.

`CreateFiber(0)`은 executable의 기본 stack reserve를 사용하므로 현재 shell은 job마다 fiber object와 stack address-space lifecycle을 반복한다. 이것은 context switch만 재는 구조가 아니며, pool 없이 production 성능 경로로 쓸 이유가 없다.

하는 일:

- Win32 fiber 진입/복귀 call chain을 시험할 수 있다.
- public `Submit/WaitForCounter` API를 유지한 채 execution backend 분기 골격을 제공한다.
- fiber 내부 재진입은 `t_bInsideJobFiber`로 inline 처리한다.

하지 않는 일:

- fiber 재사용/pool
- `WaitForCounter`에서 현재 fiber suspend
- counter waiter 등록
- ready fiber queue
- 다른 worker에서 resume
- fiber별 context/FLS
- nested dependency에서 worker를 call stack으로부터 해방

따라서 FiberShell은 **Fiber 기반 JobSystem의 성능/대기 모델이 아니라 API와 context-switch를 확인하는 M0 shell**이다. 게다가 현재 `SetExecutionMode(FiberShell)` 호출자가 없어 일반 Client/Server runtime은 이 shell도 실행하지 않는다.

## 7. Visual Studio와 DLL 연결

현재 build spine은 `Winters.sln` + MSBuild다.

- Engine: `DynamicLibrary`, v143 toolset, Windows 10 SDK, C++20, x64, `/MDd` 또는 `/MD`.
- `CJobSystem`은 `WINTERS_ENGINE`으로 export된다. Engine build에서는 `WINTERS_ENGINE_EXPORTS`가 `__declspec(dllexport)`, 소비자는 `__declspec(dllimport)`가 된다.
- 현재는 전체 클래스를 export해 `std::vector<std::thread>`, mutex, queue, condition variable 등 STL/CRT 구현 멤버가 public DLL ABI에 노출되고 MSVC C4251 경고가 난다. 동일 v143 + DLL CRT 조합에서는 빌드되지만 장기 SDK 경계는 opaque/PImpl 검토 대상이다.
- `Engine/Private/Core/JobSystem.cpp`와 Fiber/Job public headers는 `Engine.vcxproj` 및 filters에 들어 있다.
- post-build `UpdateLib.bat`가 public headers/import library/DLL을 `EngineSDK`와 runtime output으로 동기화한다. `EngineSDK/inc`는 수동 편집 대상이 아니다.
- Client는 EngineSDK header와 `WintersEngine.lib` import library를 사용하고 solution dependency로 Engine을 먼저 빌드한다.
- Server는 Engine project reference를 갖지만 `CServerEntry` 호출 경로가 없어 JobSystem을 runtime simulation에 연결하지 않는다.
- Win32 fiber 함수는 `Kernel32.dll` API다. Windows desktop C++ 링크의 기본 system library 경로를 통해 import된다.

Visual Studio debugger에서 OS thread만 보면 fiber wait의 논리 call stack이 끊겨 보일 수 있다. FiberFull을 만들 때 profiler event에는 최소 `worker id`, `fiber id`, `job id`, `counter id`, `yield/resume worker`, `ready/wait time`이 필요하다.

## 8. 현재 남은 위험과 완료 게이트

### A. ThreadOnly baseline

1. 1만 fan-out Submit/Wait, 정확히 1만 complete.
2. parent job이 child job을 submit하고 wait하는 nested-wait stress.
3. deque overflow/global fallback stress.
4. worker count 1, 2, hardware default 반복.
5. shutdown과 outstanding job의 명시적 계약.
6. fixed-ring/non-trivial slot reuse를 model-check 또는 더 단순한 atomic task pointer representation으로 검토.
7. 현재 `Shutdown`은 flag를 세우면 worker가 즉시 loop를 빠져나오고 deque를 파괴하며 GlobalQueue를 drain/clear하지 않는다. “shutdown 전 outstanding job 0”을 assert하거나 drain semantics를 구현해야 한다.
8. TLS에는 worker index만 있고 owning `CJobSystem*`가 없다. JobSystem A의 worker가 B에 submit하면 B가 자기 owner로 오인할 수 있으므로 단일 인스턴스 계약을 강제하거나 TLS owner pointer를 함께 둔다.
9. `Submit`이 counter를 먼저 증가시킨 뒤 allocation/copy가 throw하거나 job body가 throw하면 decrement/root 복귀가 보장되지 않는다. “job은 noexcept, 모든 counter는 join” 계약과 실패 검증이 필요하다.

### B. FiberShell lab

1. runtime 기본값은 계속 ThreadOnly로 두고 별도 test executable에서만 opt-in.
2. Create/Switch/Delete 성공·실패 counter와 GetLastError 진단.
3. job 예외/조기 종료가 worker thread를 죽이지 않는 계약.
4. x86 지원 시 `*Ex + FIBER_FLAG_FLOAT_SWITCH` 검증.

### C. FiberPool/FiberFull

1. preallocated pool과 명확한 Free/Running/Waiting/Ready state machine.
2. stack-local `CJobCounter*` lifetime을 포함한 waiter 등록/해제 선형화.
3. lock/yield 금지 구간. Mutex를 잡은 채 yield하면 같은 OS thread의 다른 fiber가 같은 mutex를 기다리는 self-deadlock이 가능하다.
4. cross-worker resume 동기화 및 TLS/FLS/context-slot 규약.
5. deterministic reduce/apply barrier. 실행 순서는 비결정적이어도 관찰 가능한 gameplay 결과는 결정적이어야 한다.
6. thread-only와 fiber-full의 byte/state parity, profiler로 실제 wait 감소 증명.

### D. Server

1. `CServerEntry`의 stub을 별도 구현 세션에서 완성하고 `main` lifetime에 연결.
2. IOCP worker는 OS completion wait 역할로 유지하고, simulation tick의 독립 phase만 JobSystem 후보로 분류.
3. 동일 input/seed에서 snapshot/event byte parity를 우선 게이트로 둔다.

## 9. 이번 검증 결과

- 정적 대조: Job/Fiber public/private source, Client 호출부, Server stub, vcxproj/filters, SDK mirror를 확인했다.
- runtime artifact 대조: 2026-07-11 profiler capture에서 Transform 12 jobs submit을 확인했다.
- Debug x64 Engine build 성공:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' `
  Engine\Include\Engine.vcxproj /m /t:Build `
  /p:Configuration=Debug /p:Platform=x64 /nologo /v:minimal
```

- Debug x64 Server build 성공:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' `
  Server\Include\Server.vcxproj /m /t:Build `
  /p:Configuration=Debug /p:Platform=x64 /nologo /v:minimal
```

Server 성공은 `ServerEntry.cpp`가 컴파일·링크된다는 뜻일 뿐 stub이 기능적으로 연결됐다는 뜻이 아니다. 두 build에서 exported STL 멤버에 대한 C4251 경고가 확인됐다.
- 문서/공개 주석 동기화 뒤 표준 solution build도 실행했다. Engine, GameSim, SimLab, Elden client, Server는 산출물 생성까지 통과했지만 전체 결과는 Client의 기존 dirty change `Client/Private/Manager/Minion_Manager.cpp:1009` (`animCount` 미선언)에서 실패했다. 따라서 **관련 Engine/Server target은 통과, 전체 solution은 비-green**으로 기록한다. 이 무관 Client 코드는 수정하지 않았다.
- `dumpbin /imports`로 `ConvertThreadToFiber`, `ConvertFiberToThread`, `CreateFiber`, `SwitchToFiber`, `DeleteFiber`, `IsThreadAFiber` 6개 Kernel32 import를 확인했다.
- `dumpbin /exports`로 `CJobSystem` 생성/소멸, Initialize/Shutdown, mode API, Submit overload, WaitForCounter, FiberShell 관련 symbol export를 확인했다.
- 누락: 전용 `WintersJobSystemStress` project/file은 존재하지 않는다.
- 누락: FiberShell opt-in runtime stress, Fiber counter/profile, FiberFull test는 없다.

## 10. 문서 신뢰 순서

1. 현재 코드와 vcxproj
2. 이 날짜의 상태 감사
3. `FIBER_JOBSYSTEM_STABILIZATION_PLAN.md`의 설계 위험 목록
4. `FIBER_JOB_SYSTEM_v2.md`의 목표 설계
5. v1 및 4~5월 Server TODO/과거 회고

과거 문서의 “임시 비활성”, “race 미해결”, “Fiber 본체 미구현”은 그 날짜의 역사로는 의미가 있다. 현재 사실로 재사용하면 안 된다.

## 11. 6주 mastery와 엔진 구현의 관계

6주 프로그램은 다음처럼 구현 단계와 별개로 관리해야 한다.

```text
엔진 현재 상태       : active ThreadOnly + dormant FiberShell M0
엔진 다음 구현       : stress → shell lab → pool → wait/resume → server integration
6주 mastery 산출물   : 개념 설명, 최소 재현, 계측, 디버깅 기록, 결정성 비교, 포트폴리오 환전물
```

따라서 “6주 미착수”는 맞지만 “Fiber 코드가 한 줄도 없다”는 뜻은 아니다. 반대로 FiberShell 코드가 존재한다고 6주 학습이나 FiberFull 구현을 완료했다고 말할 수도 없다.

## 참고 원전

- Microsoft Learn, Fibers: https://learn.microsoft.com/en-us/windows/win32/procthread/fibers
- Microsoft Learn, SwitchToFiber: https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-switchtofiber
- Microsoft Learn, ConvertThreadToFiberEx: https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-convertthreadtofiberex
- Chase and Lev, Dynamic Circular Work-Stealing Deque (SPAA 2005): https://doi.org/10.1145/1073970.1073974
- Lê et al., Correct and Efficient Work-Stealing for Weak Memory Models (PPoPP 2013): https://doi.org/10.1145/2442516.2442524
