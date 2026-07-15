# 동시성 · 멀티스레딩 · 비동기

## ① 한 줄 본질

데이터 레이스(data race)는 "동기화 없이 두 스레드가 같은 메모리를 접근하고 그중 하나가 쓰기"인 순간 미정의 동작(undefined behavior)이며, 동시성 설계란 mutex·atomic·happens-before로 그 조건을 깨거나 — 더 좋게는 — 공유 가변 상태 자체를 설계 단계에서 제거하는 일이다. 나는 Winters 엔진에서 Chase-Lev work-stealing JobSystem, thread_local 프로파일러, IOCP 서버를 직접 구현하며 이 원칙을 코드로 확인했다.

## ② 기본 개념

### 스레드와 데이터 레이스

- C++ 표준의 데이터 레이스 정의: **서로 다른 스레드의 두 접근이 (1) 같은 메모리 위치, (2) 최소 하나는 쓰기, (3) 둘 다 atomic이 아니고, (4) 둘 사이에 happens-before 관계가 없으면** 데이터 레이스이고, 프로그램 전체가 UB다.
- "레이스가 나면 값이 좀 이상해지는 정도"가 아니다. UB이므로 컴파일러는 레이스가 없다는 전제로 최적화(레지스터 캐싱, 재배치, 분기 제거)를 하고, 그 결과 "절대 일어날 수 없는" 제어 흐름도 관측될 수 있다.
- `std::thread`는 OS 커널 스레드 래퍼. **joinable 상태로 소멸자(destructor)가 불리면 `std::terminate`** — 그래서 스레드를 소유하는 객체는 소멸자에서 반드시 join 프로토콜을 밟아야 한다(아래 Winters JobSystem/IOCP 참고).

### mutex / lock_guard / unique_lock

| 도구 | 특징 | 용도 |
|---|---|---|
| `std::mutex` | lock/unlock 원시 연산. 비재귀 | 직접 쓰지 말고 RAII 래퍼로 |
| `std::lock_guard` | 생성 시 lock, 소멸 시 unlock. 그게 전부 | 대부분의 임계구역 (가장 가벼움) |
| `std::unique_lock` | 지연 잠금, 중도 unlock, 이동 가능 | `condition_variable::wait`가 요구 |

- 핵심 규율은 **락 범위 최소화**: 락 안에서는 자료구조 조작만, 무거운 작업·사용자 콜백·I/O는 락 밖에서. 락 보유 중 콜백을 부르면 콜백이 같은 락을 다시 잡아 데드락(비재귀 mutex)이 나거나, 락 보유 시간이 통제 불능이 된다.
- `mutable std::mutex`: const 멤버 함수(논리적 const)에서도 물리적으로는 락을 잡아야 하므로 mutex는 관례적으로 mutable로 선언한다.

### condition_variable

- `wait(lock, pred)`는 (1) pred 검사 → (2) 거짓이면 atomic하게 "unlock + 수면" → (3) notify로 깨어나면 다시 lock 잡고 pred 재검사. **spurious wakeup(가짜 기상)이 표준상 허용**되므로 predicate 루프가 필수다.
- **lost wakeup**: 조건 변경과 notify가 대기자의 "pred 검사 ~ 수면 진입" 사이 틈에 끼면 신호가 유실된다. 표준 해법은 조건 변경을 반드시 같은 mutex 안에서 하는 것. (Winters는 의도적으로 다른 절충을 택했다 — ④ 참고.)

### std::atomic과 memory ordering

- `std::atomic<T>`는 trivially copyable한 T를 감쌀 수 있다(정수, 포인터, enum class, 작은 POD). 원자성(찢긴 읽기/쓰기 없음)과 순서 보장(ordering)을 제공한다.
- **memory_order 3단계 요약**:
  - `seq_cst`(기본값): 모든 seq_cst 연산이 **전역 단일 순서(single total order)** 를 가진다. 가장 강하고 가장 비싸다(x86에서도 store에 mfence급 비용).
  - `acquire/release`: **짝(pairing)** 으로 동작. release store 이전의 모든 쓰기는, 그 값을 acquire load로 읽은 스레드에게 보인다(happens-before 성립). "데이터 준비 → 플래그 발행 → 플래그 확인 → 데이터 소비" 패턴의 표준 도구. RMW(read-modify-write) 연산에는 `acq_rel`.
  - `relaxed`: 원자성만 보장, 순서 보장 없음. **그 값 자체만 필요하고 다른 메모리의 가시성을 동반하지 않을 때만** — 통계 카운터, ID 발급기, 단일 값 발행.
- `compare_exchange_weak` vs `strong`: weak은 값이 같아도 실패할 수 있는 **spurious failure** 허용(LL/SC 아키텍처에서 더 쌈) — 재시도 루프에 적합. strong은 spurious failure 없음 — 실패를 재시도로 흡수할 수 없는 자리(한 번 지면 작업이 유실되는 자리)에 필수.

### thread_local

- 세 번째 저장 수명(storage duration): static도 automatic도 아닌, **스레드마다 별도 인스턴스**. 스레드 시작 시 초기화, 종료 시 파괴.
- 실전 용도 두 가지: (1) **스레드 신원**(내가 몇 번 워커인가) → 락 없는 경로 분기와 per-thread 버퍼 인덱싱, (2) **스크래치 버퍼** → 여러 스레드가 같은 알고리즘을 동시에 돌려도 공유 상태가 없어 재진입 안전.

### lock-free와 Chase-Lev work-stealing deque

- lock-free = "어느 스레드가 멈춰도(선점당해도) 시스템 전체는 전진한다". mutex 기반은 락 보유자가 선점되면 전원이 멈추므로 lock-free가 아니다.
- **Chase & Lev(2005) deque**: 워커별 작업 큐의 표준 답.
  - **owner는 bottom에서 Push/Pop (LIFO)** — 방금 push한 job의 데이터가 캐시에 뜨겁다.
  - **thief는 top에서 Steal (FIFO)** — 가장 오래된(캐시에서 식은) job을 가져가 owner와의 캐시 라인 경쟁을 최소화.
  - 경합이 **"마지막 1개 요소"에서 owner Pop과 thief Steal이 만나는 CAS 한 지점**으로 좁혀지는 것이 이 알고리즘의 핵심이다.

### std::async / future의 함정

- `std::async(launch::async, ...)`가 반환한 future를 받지 않고 버리면, **임시 future의 소멸자가 작업 완료까지 블로킹**한다(async 출신 shared state의 특례). "비동기로 던졌다"고 생각한 코드가 사실상 동기 순차 실행이 된다.
- 해법: future를 컨테이너가 소유하고, 완료 여부는 `wait_for(0s) == future_status::ready`로 논블로킹 폴링, 블로킹은 소유 객체의 소멸자 한 지점으로 계약을 좁힌다.

### fiber vs thread

- **thread**: OS가 선점 스케줄링(preemptive). 컨텍스트 스위치에 커널 진입 비용. 아무 때나 끊길 수 있으므로 모든 공유 접근에 동기화 필요.
- **fiber**: 유저 모드 협조적(cooperative) 스케줄링. `SwitchToFiber`를 **명시적으로 호출한 지점에서만** 제어가 넘어간다. 커널 진입 없이 스위칭이 싸고, 전환 지점이 코드에 보이므로 "이 사이에는 끊기지 않는다"는 추론이 가능. 대가: 하나가 양보하지 않으면 그 스레드 위의 전원이 멈춘다. 게임 잡 시스템에서 "job이 하위 job을 기다릴 때 스레드를 반납"하는 용도로 쓰인다(Naughty Dog GDC 강연 계열).

### IOCP (I/O Completion Port)

- Windows의 **proactor** 모델: "읽을 수 있으면 알려줘"(reactor, select/epoll)가 아니라 **"읽기를 미리 걸어두면(WSARecv + OVERLAPPED), 완료됐을 때 완료 큐로 알려줄게"**.
- 워커 스레드 풀이 `GetQueuedCompletionStatus`로 완료 패킷을 꺼내 처리. 커널이 동시 실행 스레드 수를 조절해 컨텍스트 스위치를 최소화한다.
- 핵심 계약: **비동기 요청에 넘긴 버퍼와 OVERLAPPED는 완료 통지가 올 때까지 살아 있어야 한다** — 소켓 래퍼의 수명 관리가 어려운 이유.

## ③ 심화 (꼬리질문 대비)

### acquire/release로 부족하고 seq_cst가 꼭 필요한 곳이 있나?

있다. **StoreLoad 재배치**를 막아야 하는 자리다. acquire는 "load 뒤의 접근이 앞으로 못 옴", release는 "store 앞의 접근이 뒤로 못 감"만 보장하고, **"내 store가 남에게 보인 뒤에야 남의 store를 load한다"** 는 보장(Dekker/Peterson 알고리즘의 전제)은 못 한다. x86조차 StoreLoad만은 하드웨어가 재배치하므로 이 한 지점에는 seq_cst fence(mfence)가 필요하다. Chase-Lev Pop의 "bottom 내려쓰기 → top 읽기"가 정확히 이 사례다(④에서 실코드).

### refcount형 카운터의 memory_order 비대칭 (shared_ptr과 동일 논리)

- **증가는 relaxed**: 카운터를 올리는 시점엔 아무도 이 값에 근거한 동기화 결정을 하지 않는다(내가 참조를 이미 갖고 있다는 사실이 선행 보장).
- **감소는 acq_rel**: release로 "내 작업의 쓰기가 감소보다 먼저 완료됨"을 발행하고, acquire로 다른 감소자들의 쓰기를 획득 — 0을 만든 마지막 스레드(또는 0을 관측한 대기자)가 전원의 결과를 안전하게 봐야 하므로.
- **0 관측 load는 acquire**: release 감소와 짝을 맞춰 happens-before를 완성.

### false sharing

서로 다른 스레드가 **서로 다른 변수**를 써도, 두 변수가 같은 64바이트 캐시 라인에 있으면 캐시 일관성 프로토콜(MESI)이 라인 전체를 무효화 핑퐁시켜 성능이 무너진다. 해법은 `alignas(64)`(또는 C++17 `std::hardware_destructive_interference_size`)로 경합 변수들을 다른 라인에 격리.

### lock-free 컨테이너에 아무 타입이나 담아도 되나? (speculative read UB)

안 된다. Chase-Lev Steal은 "슬롯을 먼저 복사 → CAS로 소유권 검증 → 실패면 복사본 폐기" 구조인데, 복사가 **검증 전에** 일어난다. 버려질 값이라도 그 복사 자체가 non-atomic 읽기이므로, 그 순간 owner가 ring을 한 바퀴 돌아 같은 슬롯에 Push(쓰기)하면 표준 정의상 데이터 레이스 = UB다. `std::function`처럼 copy ctor가 힙 포인터를 역참조하는 타입이면 실제로 터질 수 있다. 그래서 원 논문 계열 C11/C++11 구현(Lê et al. 2013)은 배열 원소를 atomic으로 두거나 T를 trivially copyable로 제한한다. Winters의 현재 구현은 WorkItem에 `std::function`이 들어 있어 이 제약을 위반한 **알려진 한계**다 — 이걸 스스로 지적할 수 있으면 면접에서 최고급 신호다.

### job이 job을 기다리면 deadlock 아닌가? → work-helping

워커가 job 안에서 하위 job의 counter를 블로킹 대기하면, 워커 수만큼의 대기가 겹치는 순간 실행할 스레드가 없어 전체 풀이 교착한다. 정답은 **대기자도 일하는 것**: WaitForCounter 루프 안에서 자기 deque → global queue → steal 순으로 job을 직접 실행한다. (더 나아간 답이 fiber — 대기 중인 job을 fiber째 보류하고 스레드를 완전히 반납.)

### magic statics (C++11)

함수 지역 static 변수 초기화는 C++11부터 **최초 진입 스레드만 초기화하고 나머지는 대기**함을 표준이 보장한다(컴파일러가 이중 검사 잠금을 삽입). 그래서 Meyers 싱글턴은 별도 락이 필요 없다. 단, 보장되는 것은 **초기화 1회뿐** — 이후의 사용이 thread-safe해지는 게 아니고, 프로그램 종료 시 static 소멸 순서 문제는 여전히 남는다.

## ④ Winters에서의 적용

### 1) Chase-Lev deque 직접 구현 — memory ordering의 교과서

`Engine/Public/Core/JobSystem/WorkStealingDeque.h`

- **Push(:30)**: 슬롯 기록 → `atomic_thread_fence(release)` → bottom을 relaxed store. "데이터 먼저, 인덱스 발행 나중".
- **Pop(:43)**: bottom을 relaxed로 내려 쓴 뒤 **`atomic_thread_fence(seq_cst)`(:47)** 를 치고 top을 읽는다 — 위 심화의 StoreLoad 차단 사례. 이 fence가 없으면 owner와 thief가 마지막 job을 둘 다 가져간다.
- **마지막 1개 경합(:58)**: `compare_exchange_strong` — 여기서 spurious failure가 나면 실제로는 이겼는데 진 것으로 처리돼 job이 유실되므로 strong 필수. 반면 **Steal(:75)** 은 `compare_exchange_weak` — 실패하면 다른 victim을 고르면 그만.
- **false sharing 차단(:91-92)**: `alignas(64) std::atomic<std::int64_t> m_iBottom / m_iTop` — owner가 갱신하는 bottom과 thief들이 CAS하는 top을 다른 캐시 라인에 격리.
- **:19** `static_assert((kCapacity& (kCapacity - 1)) == 0)`으로 2의 거듭제곱을 컴파일 타임 강제 → `%` 대신 `& (kCapacity-1)` 마스크 인덱싱.
- **자기 비판 포인트(:26)**: `CWorkStealingDeque(CWorkStealingDeque&&) noexcept {}` — vector에 담으려고 손으로 쓴 "아무것도 안 옮기는" 가짜 move ctor가 남아 있다. 실제 해법은 `Engine/Public/Core/JobSystem.h:87`의 `std::vector<std::unique_ptr<CWorkStealingDeque<WorkItem>>>` 박싱(주석 `Engine/Private/Core/JobSystem.cpp:55-57`: atomic + alignas(64) 조합이 MSVC construct_at SFINAE에서 실패). 부수 효과로 도둑들이 참조하는 deque 주소가 vector 재할당에도 불변이 됐다.

### 2) CJobCounter — 연산별 memory_order 논증

`Engine/Public/Core/JobCounter.h:24-42`

```cpp
void Increment(std::uint32_t n = 1) { m_iCount.fetch_add(n, std::memory_order_relaxed); }
void Decrement()                    { m_iCount.fetch_sub(1, std::memory_order_acq_rel); }
bool IsComplete() const             { return m_iCount.load(std::memory_order_acquire) == 0; }
```

위 심화의 refcount 비대칭 그대로다. 헤더 주석(:12)에 "cv/mutex 제거, 기존 Wait() API 삭제, busy-wait + help-stealing으로 대체"라는 설계 변경 이력을 박제했고, atomic 멤버 때문에 복사 불가이므로 copy ctor/assign을 명시 delete(:21-22)했다.

### 3) 사용 규약 위반 사고 → 하이브리드 큐 재설계

`Engine/Private/Core/JobSystem.cpp:126-161` (EnqueueJob)

초기 버전은 Main 스레드가 round-robin으로 아무 워커 deque에나 Push했다. Chase-Lev의 bottom은 **owner 단일 작성자** 전제라 이건 자료구조 버그가 아니라 **사용 계약 위반 race**였다(2026-04-23 발견). 수정: thread_local 신원(`t_iWorkerIdx`, :18, -1 = 비워커 sentinel)으로 경로를 분기 — 워커 자신(:146)만 자기 deque에 lock-free Push, Main/외부 스레드와 overflow는 mutex 보호 global `std::queue`(:156-159)로 우회. "lock-free 도입 시 진짜 어려운 건 알고리즘이 아니라 사용 계약을 코드 구조로 강제하는 것"이라는 교훈.

### 4) WaitForCounter = help-stealing, 소멸 프로토콜, CV 절충

`Engine/Private/Core/JobSystem.cpp`

- **WaitForCounter(:306-360)**: 대기 중 재우지 않고 일을 시킨다. 워커면 `TryExecuteOneJob`(자기 deque → global → steal), 비워커면 global drain 후 `m_iRoundRobin.fetch_add(relaxed)`로 victim 순회 steal. 일을 못 찾았을 때만 `yield()`.
- **소멸 프로토콜(:72-86)**: `m_bShutdown.store(true, release)` → `notify_all` → joinable 검사 후 전원 `join` → 컨테이너 clear. `~CJobSystem()`(:25-28)이 Shutdown을 호출해 joinable 스레드 소멸 = terminate를 차단. shutdown 이후 도착한 job은 버리지 않고 호출 스레드에서 inline 실행(:129-134).
- **CV 절충(:178-187)**: 유휴 워커는 `m_WakeCV.wait_for(lk, 1ms)`. producer의 notify는 lock 밖에서 불리고 predicate도 없어 고전적 lost wakeup이 가능하지만, **타임아웃이 상한을 정해 1ms 안에 자가 회복**한다 — push 핫패스를 lock-free로 유지하는 대신 지연 상한으로 정확성을 산 의식적 트레이드오프(주석에 명시).
- **victim 선택(:198-210)**: `thread_local` xorshift PRNG(시드 = 스택 주소 ^ 인덱스 ^ 황금비 상수). 내부 공유 상태가 있는 `rand()` 배제, 동기화 비용 0.
- **Fiber shell(:280-291)**: `FiberShellCall`을 스택에 만들어 `CreateFiber`에 넘기고 `SwitchToFiber` — job fiber가 hReturnFiber로 되돌아올 때까지 호출자 프레임이 살아 있다는 협조적 전환 규약이 스택 주소 전달의 안전 근거. `thread_local t_bInsideJobFiber`(:20)로 fiber 안 fiber 재진입 차단. 실행 모드는 `std::atomic<eJobExecutionMode>`(JobSystem.h:90)로 무락 토글 — atomic이 enum class도 감쌀 수 있음을 활용.

### 5) 2-pass Decision/Apply — "lock을 잘 거는 것"보다 "lock이 필요 없게"

`Client/Private/GamePlay/System/LocalUnitAISystem.cpp`

미니언 AI를 (1) **DecisionPass**(:79-98): 후보가 kParallelThreshold 미만이면 순차(병렬화 오버헤드 회피), 이상이면 JobSystem으로 병렬 — world는 읽기만 하고, 결정 POD는 `Push_Decision`(:354-360)이 `Get_WorkerSlot()`(main=0, worker=idx+1) 슬롯 버퍼에 push하므로 mutex가 0개. (2) **ApplyPass**(:100-103): WaitForCounter 후 메인 스레드 단독으로 전 슬롯을 순회하며 컴포넌트 mutation. **"병렬 구간에 공유 쓰기 없음, 쓰기 구간에 병렬 없음"** — 2026-04-28 Profiler thread_local race 사고 이후 정착한 설계 원칙.

### 6) 접근 선언 기반 시스템 병렬화 — race를 스케줄링 단계에서 차단

`Engine/Public/ECS/SystemAccess.h:77` — `SystemAccessConflicts`는 한쪽이라도 `bUnknown`이거나 `bWritesWorldStructure`면 무조건 충돌, 같은 `type_index`에 한쪽이라도 Write면 충돌. DescribeAccess를 오버라이드하지 않은 시스템은 "전부 쓴다"로 간주되는 **보수적 기본값(fail-safe default)** 이라 절대 병렬화되지 않는다. `Engine/Private/ECS/SystemScheduler.cpp:113-123`은 충돌 없는 batch만 fork-join으로 제출하는데, 지역 `CJobCounter counter`(:113)와 `[sys, &world, fTimeDelta]` 참조 캡처(:117)가 안전한 유일한 근거는 같은 스코프의 `WaitForCounter(&counter)`(:123)가 완료를 보장하는 **structured concurrency 불변식**이다 — 그 한 줄을 지우면 즉시 dangling reference.

### 7) thread_local 3연타 — 프로파일러, A* 스크래치, 커맨드 버퍼

- `Engine/Private/Core/Profiler/CPUProfiler.cpp:29` — `thread_local std::vector<PendingProfilerScope> t_vProfilerStack`. 매 프레임 수천 번 불리는 PushScope/PopScope 핫패스는 무동기화, 완성 이벤트를 공유 프레임 버퍼로 merge하는 PopScope 말단(:93)만 `lock_guard`. EndFrame(:47-53)은 copy 대신 **swap**으로 O(1) 더블 버퍼링.
- `Engine/Private/Manager/Navigation/Pathfinder.cpp:463-467` — A*의 gScore/parent/closed/generation 배열 전부 thread_local → JobSystem 워커들이 동시에 길찾기해도 공유 상태 0. 배열 clear는 O(N) fill 대신 **generation 스탬프**(:469-481): 세대 번호를 올리고 "스탬프 == 현재 세대"만 유효로 판정(Touch 람다), uint32 wrap 시에만 전체 fill.
- `Engine/Private/ECS/CommandBuffer.cpp:35-73` — ForEach 순회 중 엔티티 생성/삭제(이터레이터 무효화)를 지연 명령으로 기록, Flush는 **lock 안에서는 swap만(:43-50), 실행은 lock 밖**(:52-72) — 명령 실행이 DeferXXX를 재진입해도 데드락 없음.

### 8) std::async 소멸자 블로킹 사고 → future 소유 재설계

`Client/Private/Network/Backend/CHttpClient.cpp`

- 사고(:90-92 주석에 박제): async 반환 future를 버려서 임시 future 소멸자가 join → "비동기" HTTP가 사실상 동기 호출이었다(gotcha 2026-07-09). 부작용: 그 우연한 블로킹 덕분에 raw `this` 캡처가 안전했다 — **async 수명 수정과 this 캡처 수명 보장은 반드시 같은 변경에서** 가야 하는 커플링.
- 수정: `m_PendingRequests(vector<future<void>>)`가 future 소유(:148-156), 소멸자(:103-117)가 swap-out 후 전량 `wait()` — 블로킹을 파괴 시점 1회로 계약 축소(헤더 :23-24 주석). `PruneCompletedRequests`(:129-141)는 `wait_for(chrono::seconds(0)) == future_status::ready` 논블로킹 폴링으로 완료 future 소각(역방향 순회로 erase 인덱스 무효화 회피).
- **race 원천 차단(`Client/Public/Network/Backend/CHttpClient.h:43-57`)**: 워커에서 도는 `DoRequestWith`를 **static으로 선언**해 멤버 접근을 컴파일러가 물리적으로 차단, 호출 시점에 `MakeRequestSnapshot()`으로 host/port/authToken을 값 복사한 불변 스냅샷만 전달 — 메인 스레드의 `SetAuthToken`과 경합이 불가능. "공유 대신 복사".
- **콜백 마샬링(:159-171)**: 워커는 결과 콜백을 큐에 push만, `ProcessCallbacks`(메인 스레드)가 lock 안에서 swap 후 lock 밖에서 실행 — UI/게임 상태 단일 스레드 규약 유지.

### 9) Server IOCP — 프로액터 모델의 수명 관리

- **워커 풀 종료(`Server/Private/Network/IOCPCore.cpp:136-164`)**: `m_bRunning.exchange(false)`(이전 값으로 1회성 보장) → 워커 수만큼 `PostQueuedCompletionStatus(m_hIOCP, 0, 0, nullptr)` — INFINITE 블록 중인 워커를 **null OVERLAPPED poison pill**로 깨움(:218-223에서 종료 판정) → listen 소켓 close → 전원 join → 완료 포트 핸들 close.
- **완료 라우팅(:225)**: `IOContext* ctx = CONTAINING_RECORD(pOverlapped, IOContext, overlapped)` — OVERLAPPED를 IOContext 멤버로 임베드하고 offsetof 역산으로 바깥 컨텍스트(op 종류/sessionId/버퍼) 복원.
- **지연 파괴(`Server/Public/Network/Session.h:38-52`)**: `AddPendingIo/CompletePendingIo`가 atomic 카운터, `CanDestroy() = m_bClosing && m_pendingIoCount == 0`. 소켓이 끊겨도 커널이 OVERLAPPED/버퍼를 붙들고 있는 in-flight IO가 남았으면 즉시 delete = use-after-free이므로, 카운터가 0이 될 때까지 파괴를 미룬다.
- **단일 in-flight 송신(`Server/Private/Network/Session.cpp:91-132`)**: 소켓당 WSASend는 한 번에 하나. 이미 전송 중이면 deque에 push만(:100-103), `OnSendComplete`(:134-147)가 front를 pop하고 다음 것을 체이닝. 동시 다발 WSASend는 바이트 순서가 섞일 수 있고, deque가 완료까지 front 버퍼를 소유해 수명을 보장.
- **틱 스레드 → 네트워크 스레드 발행(`Server/Private/Game/GameRoomTick.cpp:89-90`)**: `m_visibleTickIndex.store(m_tickIndex, relaxed)` — 틱 값 하나만 발행하고 다른 버퍼 가시성을 동반하지 않으므로 relaxed로 충분. 동반했다면 release/acquire가 필요했을 자리라고 구분해 말할 수 있어야 한다.
- **드리프트 없는 고정 틱(:68-80)**: `steady_clock` + `next += period; sleep_until(next)`(30Hz, 33333us) — 절대 데드라인 누적이라 개별 틱 지터가 장기 드리프트로 쌓이지 않는다(sleep_for였다면 누적됨). system_clock이 아닌 단조 시계라 시스템 시간 조정에도 무관.

### 10) Meyers 싱글턴

`Engine/Public/Engine_Macro.h:57` — `DECLARE_SINGLETON`이 `static CLASSNAME instance;` 지역 static(magic statics로 초기화 thread-safe)을 반환하고, `NO_COPY`(:52)가 복사 생성/대입을 delete. 한계(소멸 순서, "초기화만" 안전)는 ③ 참고. 매크로 마지막이 `private:`으로 끝나 이후 멤버 선언의 접근제어를 바꿔놓는 것도 매크로 코드 생성의 함정.

## ⑤ 면접 Q&A

**Q1. 데이터 레이스가 정확히 뭐고, 왜 "값이 이상해지는 정도"가 아닌가?**
- 답: 표준 정의 4조건(같은 위치/하나는 쓰기/non-atomic/happens-before 부재) 성립 시 UB. 컴파일러가 레이스 부재를 전제로 최적화하므로 불가능해 보이는 실행도 관측 가능.
- Winters 연결: Chase-Lev Steal의 speculative read(WorkStealingDeque.h:74)가 "버려질 값의 복사"조차 정의상 레이스인 알려진 한계임을 스스로 지적.

**Q2. relaxed / acquire-release / seq_cst를 각각 실제로 어디에 썼나?**
- 답: relaxed = 값만 필요(카운터 증가, 틱 인덱스 발행, ID 발급). acquire/release = 데이터 발행-소비 짝. seq_cst = StoreLoad 차단이 필요한 유일 지점.
- Winters 연결: JobCounter.h:24-42의 증가-relaxed/감소-acq_rel 비대칭(= shared_ptr refcount 논리), WorkStealingDeque.h:47의 Dekker 스타일 seq_cst fence, GameRoomTick.cpp:90의 relaxed 단일 값 발행.

**Q3. compare_exchange_weak과 strong의 차이는? 각각 언제 쓰나?**
- 답: weak은 spurious failure 허용(LL/SC 아키텍처에서 저렴) — 실패를 재시도로 흡수 가능한 루프용. strong은 실패 = 진짜 값 불일치 보장 — 한 번 지면 복구 불가능한 자리용.
- Winters 연결: 같은 파일 안에서 Pop 마지막 1개 경합은 strong(WorkStealingDeque.h:58, 지면 job 유실), Steal은 weak(:75, 실패하면 다른 victim 고르면 그만).

**Q4. job system에서 job이 job을 기다리면 deadlock 나지 않나?**
- 답: 블로킹 대기면 워커 수만큼 대기가 겹치는 순간 교착. 해법은 work-helping(대기자가 직접 job 실행), 근본 해법은 fiber로 대기 job을 보류하고 스레드 반납.
- Winters 연결: WaitForCounter(JobSystem.cpp:306-360)가 자기 deque → global → steal 순 help-stealing. Phase 5-B로 FiberShell 모드(atomic enum 무락 토글, JobSystem.h:90) 준비.

**Q5. condition_variable의 lost wakeup은 왜 생기고 어떻게 막나?**
- 답: 조건 변경~notify가 대기자의 검사~수면 틈에 끼면 유실. 표준 해법은 조건 변경을 같은 mutex 안에서 + predicate wait.
- Winters 연결: JobSystem.cpp:178-187은 반대로 push 경로를 lock-free로 유지하려고 lost wakeup을 허용하되 `wait_for(1ms)` 타임아웃으로 회복 상한을 정했다 — 표준 답과 실무 절충을 둘 다 말하면 깊이가 산다.

**Q6. std::async의 반환 future를 안 받으면 어떻게 되나?**
- 답: async 출신 shared state의 특례로 임시 future 소멸자가 완료까지 블로킹 → 사실상 동기 호출. future를 소유 컨테이너에 보관하고 `wait_for(0)`으로 폴링, 블로킹은 소멸자 1회로 축소.
- Winters 연결: CHttpClient 실사고(cpp:90-92 주석 박제) — 덤으로 "우연한 블로킹이 raw this 캡처를 안전하게 만들고 있었다"는 수명 커플링까지 설명하면 시니어 신호.

**Q7. 락 없이 thread-safety를 보장하는 방법들을 아는 대로 말해보라.**
- 답: (1) thread_local 분리 + 좁은 merge 지점, (2) 불변 스냅샷 값 복사, (3) phase 분리(병렬 읽기 → 단독 쓰기), (4) 접근 선언 기반 conflict-free 스케줄링, (5) 스레드 어피니티 계약 + 디버그 assert.
- Winters 연결: CPUProfiler(1), CHttpClient RequestSnapshot + static 강제(2), LocalUnitAISystem 2-pass(3), SystemAccess conflict batching(4).

**Q8. IOCP에서 어떤 IO가 끝났는지 어떻게 알고, 세션은 언제 지워도 되나?**
- 답: OVERLAPPED를 컨텍스트 구조체에 임베드하고 CONTAINING_RECORD로 복원. 파괴는 in-flight IO 카운트가 0이 될 때까지 지연 — 커널이 버퍼/OVERLAPPED를 아직 참조하므로.
- Winters 연결: IOCPCore.cpp:225의 CONTAINING_RECORD, Session.h:38-52의 pending-IO refcount + closing flag, 종료는 null OVERLAPPED poison pill + join(IOCPCore.cpp:136-164).

## ⑥ 흔한 오답/함정

1. **"volatile로 스레드 동기화한다"** — volatile은 컴파일러의 접근 생략/병합만 막을 뿐, 원자성도 순서 보장(하드웨어 재배치, 캐시 가시성)도 없다. MMIO용이지 동시성 도구가 아니다. 답은 `std::atomic`.
2. **"x86은 강한 메모리 모델이라 fence가 필요 없다"** — x86도 StoreLoad는 재배치한다. Chase-Lev Pop의 bottom-store → top-load가 정확히 그 반례이고, 잘못된 ordering으로 짠 코드가 x86에서 우연히 도는 것과 표준상 올바른 것은 별개다(ARM에서 터진다).
3. **"lock-free 자료구조를 가져다 쓰면 race는 끝"** — 자료구조가 옳아도 **사용 계약**(Chase-Lev: bottom은 owner 단독 작성) 위반이 race를 만든다. Winters의 2026-04-23 사고가 정확히 이 케이스. 계약을 코드 구조(thread_local 신원 분기)로 강제해야 한다.
4. **"static 지역 변수는 thread-safe하다"** — 보장은 **초기화 1회**뿐이다. 이후의 읽기-수정-쓰기(예: `if (count < 8) ++count;` 같은 로그 상한 가드)는 여전히 비원자적. "magic statics = 초기화 안전 ≠ 사용 안전"을 구분 못 하면 감점.
5. **"detach하면 join 안 해도 된다"** — detach는 terminate만 피할 뿐, 스레드가 참조하는 객체의 수명 보장이 사라지고 프로세스 종료 시 static 파괴와 경합한다. 스레드 소유 객체는 flag(release store) → wake → join 소멸 프로토콜(JobSystem.cpp:72-86, IOCPCore.cpp:136-164)이 정석.

## 다른 챕터와의 연결

- [03_memory_lifetime_raii.md](03_memory_lifetime_raii.md) — 스택 캡처의 수명 논증(fork-join 스코프), async raw this 캡처, IOCP in-flight 버퍼 수명은 결국 수명 문제다.
- [04_pointers_smart_pointers.md](04_pointers_smart_pointers.md) — 소멸자 join 프로토콜, future 소유권, `vector<unique_ptr>` 박싱, ResourceCache의 unique/shared 소유권 구분.
- [08_stl_containers_cache.md](08_stl_containers_cache.md) — false sharing과 alignas(64), owner-LIFO/thief-FIFO의 캐시 논리, generation 스탬프 O(1) 리셋, 순회 중 삭제와 이터레이터 무효화.
- [05_class_design_value_semantics.md](05_class_design_value_semantics.md) — atomic 멤버 클래스의 복사/이동 불가와 =delete 의도 표현, sink parameter(`Send(std::vector<u8_t>)` 값 전달 + move).
