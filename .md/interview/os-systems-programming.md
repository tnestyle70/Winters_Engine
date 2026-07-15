# 운영체제 & 시스템 프로그래밍 (Windows 중심) — 기술면접 대비

> 대상: 자체 DX11 엔진(Winters) + IOCP 게임 서버 제작 경험이 있는 게임 클라이언트/서버 프로그래머 지원자.
> 원칙: 모든 답변은 "정의 → 원리 → 트레이드오프 → 내 프로젝트에서 실제로 겪은 일" 순서로 말할 수 있게 준비한다. 인용된 파일 경로는 전부 실존 코드/문서다.

## 출제 경향 개요

게임회사 OS/시스템 면접은 교과서 순서대로 묻지 않는다. 실제로는 세 갈래로 파고든다.

1. **멀티스레딩 실전 능력** — "스레드 몇 개 만들 건가", "이 코드 race 있나", "락 없이 어떻게 하나". 프레임 예산(16.6ms) 안에서 병렬화해 본 사람인지 확인하려는 것. 뮤텍스/스핀락/atomic 선택 기준, 메모리 오더링, false sharing까지 내려간다.
2. **Windows 플랫폼 이해** — 게임 클라이언트는 사실상 Windows 앱이고 서버도 Windows IOCP가 많다. HANDLE, IOCP, SRWLock, Fiber, SEH/minidump, DLL 경계는 "Windows에서 실제로 출시해 본 사람"을 거르는 단골 주제.
3. **메모리 계층 감각** — 가상 메모리/페이지 폴트/워킹셋을 로딩 히치·프레임 스파이크와 연결해 설명할 수 있는지, 스택/힙 비용 차이를 근거로 커스텀 할당자의 필요성을 말할 수 있는지.

나의 무기: Winters의 **CJobSystem(Chase-Lev work-stealing + Fiber shell)**, **CPUProfiler thread_local race를 Decision/Apply 2-pass로 해결한 경험**, **IOCP 서버 × Fiber 통합 설계**, **std::async 수명 함정을 계약으로 박은 CHttpClient**. 개념 질문이 나올 때마다 이 실물 코드로 되받아치는 것이 전략이다.

---

## 핵심 개념 정리

### 1. 프로세스 vs 스레드

**정의**: 프로세스는 자원 소유의 단위(가상 주소 공간, 핸들 테이블, 토큰, 힙), 스레드는 CPU 스케줄링의 단위(레지스터 컨텍스트, 스택, TLS, 커널 스택)다.

**원리**:
- 프로세스마다 독립된 가상 주소 공간을 가진다. x64 유저 공간은 프로세스당 128TB(실질 8TB~128TB, OS 버전에 따라 다름)이고, 페이지 테이블(CR3 레지스터가 가리키는 최상위 테이블)이 프로세스마다 따로 있다.
- 같은 프로세스의 스레드들은 주소 공간·핸들 테이블·힙을 공유하고, 각자 스택(기본 reserve 1MB)과 레지스터 컨텍스트, TEB(Thread Environment Block)만 따로 가진다.
- Windows에서 스레드가 스케줄링 대상이며, 프로세스 자체는 스케줄되지 않는다(프로세스는 우선순위 클래스라는 "기본값"만 제공).

**수치 감각**: 프로세스 생성(CreateProcess)은 ms 단위(이미지 매핑, 주소 공간 구축, 로더 초기화), 스레드 생성(CreateThread)은 수십 μs 단위, 이미 만들어 둔 스레드 풀에서 작업 하나 돌리는 건 μs 미만.

**게임 맥락**: 게임은 "프로세스 하나 + 스레드 여러 개" 모델이 표준이다. 렌더/오디오/IO/잡워커가 같은 주소 공간에서 포인터를 그대로 주고받아야 프레임 예산이 나온다. 프로세스 분리는 크래시 격리가 필요한 곳(런처, 안티치트, 크래시 리포터)에만 쓴다. Winters도 Client/Server 두 프로세스이고, 각 프로세스 내부는 JobSystem 워커 스레드들이 World를 공유한다.

### 2. 컨텍스트 스위칭 비용 — 직접 비용과 간접 비용

**정의**: 실행 중인 스레드의 CPU 상태를 저장하고 다른 스레드의 상태를 복원하는 작업.

**직접 비용 분해** (스레드 간, 같은 프로세스 기준 대략 1~2μs):
1. 인터럽트/시스템 콜 진입 — 유저→커널 모드 전환 (~100–200ns, SYSCALL + 스택 전환)
2. 레지스터 컨텍스트 저장 (범용 + XMM/YMM — AVX 상태만 수백 바이트)
3. 스케줄러 실행 — 다음 스레드 선택, 큐 조작
4. 컨텍스트 복원 + 커널→유저 복귀

프로세스가 다르면 CR3 교체로 **TLB 플러시**가 추가된다(PCID로 완화되지만 여전히 비싸다).

**간접 비용(진짜 비용)**: 새 스레드가 돌기 시작하면 L1/L2 캐시와 TLB가 이전 스레드의 데이터로 오염되어 있어 cold miss가 연쇄된다. 직접 비용 1μs짜리 스위치가 실효 10~30μs로 불어나는 이유. 면접에서 "컨텍스트 스위치 비용?"이라 물으면 **직접/간접을 나눠서** 답하는 것이 차별화 포인트다.

**게임 맥락**: 이것이 "코어 수만큼만 워커를 만들고 스레드를 재우지 말고 잡을 갈아끼운다"는 잡시스템 설계의 근거다. Winters `CJobSystem::Initialize`는 워커 수를 `hardware_concurrency - 2`(메인+렌더 몫을 남김)로 잡는다(`Engine/Private/Core/JobSystem.cpp`). 그리고 스레드 스위치가 비싸니 **더 싼 스위치 = Fiber**(유저 모드, 수십 ns)로 내려가는 것이 다음 단계다(아래 Fiber 절).

### 3. 스케줄링 (Windows 중심)

**정의**: Windows는 32단계 우선순위(0~31) 기반 **선점형(preemptive) 스케줄러**. 16~31은 realtime 범위, 1~15는 dynamic 범위.

**원리**:
- 스레드 우선순위 = 프로세스 우선순위 클래스(NORMAL_PRIORITY_CLASS 등) + 스레드 상대 우선순위(THREAD_PRIORITY_HIGHEST 등)의 조합.
- **퀀텀(quantum)**: 클록 인터럽트(기본 ~15.6ms 간격) 기준으로 클라이언트 SKU는 약 2클록 틱, 서버 SKU는 약 12클록 틱. 같은 우선순위끼리 라운드로빈.
- **동적 부스트**: IO 완료, 포그라운드 윈도우, 대기에서 깨어날 때 우선순위를 일시적으로 올렸다가 퀀텀 소모에 따라 원복. 기아(starvation) 스레드는 밸런스 셋 매니저가 주기적으로 부스트해 준다 — Windows에는 뮤텍스 우선순위 상속(priority inheritance)이 없고 이 안티-스타베이션 부스트로 우선순위 역전을 완화한다는 점이 UNIX 계열과의 차이로 자주 나온다.
- 멀티코어에서는 ideal processor / 캐시 친화(affinity) 힌트를 고려해 배치한다. `SetThreadAffinityMask`, `SetThreadIdealProcessor`로 개입 가능.

**게임 맥락**:
- 메인/렌더 스레드를 THREAD_PRIORITY_HIGHEST로 올리는 것보다, **잡 워커가 유휴일 때 CPU를 실제로 반납하는 것**이 우선이다. Winters WorkerLoop는 yield 스핀 대신 `condition_variable::wait_for(1ms)` 타임아웃 블로킹으로 전환했다(`Engine/Private/Core/JobSystem.cpp` WorkerLoop 주석: "yield 스핀은 외부 CPU 부하와 경쟁하며 코어를 태운다"). yield는 READY 스레드가 없으면 즉시 리턴해서 코어를 100% 태우기 때문.
- 타이머 해상도: `Sleep(1)`이 실제로는 15.6ms까지 늘어질 수 있다. `timeBeginPeriod(1)`이나 고해상도 대기(`CREATE_WAITABLE_TIMER_HIGH_RESOLUTION`)가 프레임 페이싱에서 중요. 오디오처럼 지연에 민감한 스레드는 MMCSS(`AvSetMmThreadCharacteristics`)로 스케줄링 클래스를 올린다.

### 4. 스레드 동기화 프리미티브 전 계열 — 언제 무엇을 쓰나

| 프리미티브 | 본질 | 특성 | 게임에서 쓰는 곳 |
|---|---|---|---|
| **스핀락** | 유저 모드 CAS 루프 | 대기 중 CPU 소모, 컨텍스트 스위치 없음 | 임계구역이 수십 ns급으로 확실히 짧고, 코어가 남을 때만 |
| **CRITICAL_SECTION** | 스핀 카운트 + 커널 이벤트 하이브리드 | 프로세스 내 전용, 재귀 획득 허용, 40바이트 | 레거시/재귀가 필요한 곳. 요즘은 SRWLock에 밀림 |
| **SRWLOCK** | 포인터 1개 크기, futex류 대기 | 비재귀, shared/exclusive(reader-writer), 무초기화(SRWLOCK_INIT) | Windows에서 기본 선택. `std::shared_mutex` 백엔드 |
| **std::mutex** | MSVC에선 SRWLock 기반 | 이식성, RAII(lock_guard/unique_lock) | 크로스플랫폼 코드 기본값. Winters 전역 큐/콜백 큐가 전부 이것 |
| **세마포어** | 카운트 N 허용 | "자원 N개" 모델, 프로세스 간 공유 가능(커널 객체) | 동시 접근 수 제한(로더 동시 IO 수), 생산자-소비자 카운팅 |
| **조건변수 (CV)** | "조건이 될 때까지 재워줘" | 반드시 뮤텍스+술어(predicate)와 함께. spurious wakeup 대응 필수 | 워커 깨우기. Winters `m_WakeCV` |
| **Event (커널)** | manual/auto reset 신호 | HANDLE — WaitForMultipleObjects로 다른 객체와 묶어 대기 가능 | 스레드 종료 신호, IO와 섞어 기다릴 때 |
| **WaitOnAddress** | Win8+ futex 등가물 | 주소 값이 바뀔 때까지 대기 | 커스텀 동기화 프리미티브 제작 |
| **std::atomic** | 락 아님, 하드웨어 원자 명령 | 단일 변수 한정 | 카운터, 플래그, lock-free 자료구조 |

**선택 규칙(면접 요약)**:
1. 기본은 뮤텍스(SRWLock). 잠깐 잠들어도 되는 곳은 전부 이것.
2. 임계구역이 극단적으로 짧고 경합이 낮다는 **측정 근거**가 있을 때만 스핀락. (커널도 스핀락은 인터럽트 컨텍스트처럼 잠들 수 없는 곳에 쓴다.)
3. "개수"의 의미가 있으면 세마포어, "조건"의 의미가 있으면 CV.
4. 변수 하나면 atomic으로 락을 없애는 게 먼저다.

**Winters 실물**: `Engine/Public/Core/JobSystem.h` — 전역 잡 큐는 `std::mutex m_GlobalMutex + std::queue`, 워커 웨이크업은 `std::mutex m_WakeMutex + std::condition_variable m_WakeCV`(1ms 타임아웃), 잡 완료 대기는 락 대신 `CJobCounter`(atomic) + help-stealing. "락 큐 + atomic 카운터 + CV 웨이크업" 3종을 역할별로 나눠 쓴 구성 자체가 답변 소재다.

### 5. 데드락 — 4조건과 예방

**Coffman 4조건** (전부 성립해야 데드락):
1. **상호 배제(Mutual Exclusion)** — 자원을 한 번에 하나만 소유
2. **점유 대기(Hold and Wait)** — 자원을 쥔 채 다른 자원을 대기
3. **비선점(No Preemption)** — 자원을 강제로 뺏을 수 없음
4. **순환 대기(Circular Wait)** — 대기 그래프에 사이클

**예방 전략** (하나만 깨면 됨):
- **락 순서 규약(lock ordering)** — 순환 대기를 깬다. 모든 코드가 락을 전역적으로 같은 순서로 획득. 가장 실용적이고 표준적인 해법.
- `std::scoped_lock(m1, m2)` / `std::lock` — 내부적으로 try-and-back-off로 점유 대기를 깬다.
- try_lock + 타임아웃 + 롤백 — 비선점 조건을 깬다.
- 락 범위 최소화/락 개수 자체를 줄이는 설계(단일 소유자, 메시지 전달) — 애초에 조건이 안 만들어지게.

**Winters 실전 연결 2건**:
1. **락 순서 규약을 설계 단계에서 박은 사례** — 서버 IOCP×Fiber 통합 계획에서 `RoomManager` 도입 시 "lock acquisition order"와 "`m_stateMutex`를 쥔 채 fiber yield 금지"를 명시적 Gotcha로 박았다(메모리: `project_fiber_mastery_session_2026_05_11.md` §8). fiber는 yield하면 같은 스레드가 다른 fiber의 코드를 실행하므로, **락을 쥔 채 yield하면 같은 스레드가 같은 락을 다시 획득하려는 self-deadlock**이 날 수 있다 — 스레드 기반 직관이 깨지는 대표 지점.
2. **"combo deadlock" 수정 경험 (커밋 3847f3f)** — 뮤텍스 데드락이 아니라 **로직 레벨의 무한 대기**였다: 챔피언 봇 콤보 상태머신에서 사거리 안에 들어온 스텝이 발동 gate를 통과하지 못하면 영원히 그 스텝에 머무르는(진행 조건이 절대 참이 될 수 없는) stall. "gate 실패 시 다음 스텝으로 진행"으로 고쳤다. 면접 포인트: 데드락 분석의 본질은 뮤텍스가 아니라 **"대기 그래프에서 빠져나올 간선이 있는가"**이고, 이 프레임은 상태머신·비동기 파이프라인에도 그대로 적용된다고 말하면 이해 깊이를 보여줄 수 있다.
3. (보너스) **데드락은 아니지만 같은 증상이었던 라이브락성 hang** — Phase 5-A에서 `WaitForCounter` 무한 루프(아래 7절). "메인이 멈췄다 = 무조건 데드락"이 아니라 **덤프에서 각 스레드가 무엇을 기다리는지**부터 보는 습관을 이 사고로 얻었다.

### 6. 원자적 연산과 메모리 모델

**정의**: 원자적 연산은 중간 상태가 관측되지 않는 연산. `std::atomic<T>`는 (a) 연산의 원자성, (b) 명시한 memory_order에 따른 **가시성/순서 보장**을 함께 제공한다.

**하드웨어 관점**:
- x86-64에서 aligned load/store는 원래 원자적이다. RMW(fetch_add, CAS)는 `LOCK` prefix 명령으로 캐시 라인을 배타 소유(MESI의 M 상태)한 채 수행한다 — 요즘은 버스 락이 아니라 **cache lock**.
- 캐시 코히런시(MESI)는 "같은 주소는 결국 한 값"을 보장하지만, **서로 다른 주소에 대한 연산 순서**는 보장하지 않는다. 코어마다 store buffer가 있어 내가 쓴 값이 남에게 보이기 전에 내 이후 load가 먼저 실행될 수 있다(StoreLoad 재배열). 이것이 메모리 모델이 필요한 이유.
- x86은 TSO(Total Store Order) — StoreLoad 재배열만 허용. ARM(콘솔/모바일)은 훨씬 약한 모델이라 acquire/release를 안 박으면 x86에서 우연히 돌던 코드가 ARM에서 깨진다.

**memory_order 4종 실전 요약**:
- `relaxed` — 원자성만. 순서/가시성 보장 없음. 통계 카운터, 라운드로빈 인덱스. (Winters: `m_iRoundRobin.fetch_add(1, relaxed)`)
- `acquire` (load) / `release` (store) — 짝으로 쓰면 "release 이전의 모든 쓰기"가 "acquire 이후의 모든 읽기"에 보인다. 생산자-소비자 handoff의 표준. (Winters: `m_bShutdown.store(true, release)` / `load(acquire)`)
- `seq_cst` — 모든 seq_cst 연산에 대한 단일 전역 순서. **서로 다른 두 변수에 대한 내 store와 상대 store의 순서가 서로에게 일관되게 보여야 할 때**만 필요. StoreLoad 재배열까지 막아야 하는 알고리즘(Dekker 패턴, Chase-Lev의 bottom store ↔ top load)이 여기 해당. x86에서 seq_cst store는 `XCHG`/`MFENCE`로 컴파일되어 relaxed store보다 확실히 비싸다.

**Winters 실물 — `Engine/Public/Core/JobSystem/WorkStealingDeque.h` (Chase & Lev 2005)**:
```cpp
// Pop (owner):
std::int64_t b = m_iBottom.load(relaxed) - 1;
m_iBottom.store(b, relaxed);
std::atomic_thread_fence(std::memory_order_seq_cst); // ★ StoreLoad 차단
std::int64_t t = m_iTop.load(relaxed);
...
// 마지막 1개는 Steal과 경합 → CAS(seq_cst)로 판정
```
- Pop의 seq_cst fence가 없으면: 내 `bottom` 감소가 store buffer에 머무는 동안 `top`을 먼저 읽어 버려, thief와 owner가 **같은 마지막 잡을 둘 다 가져가는** 이중 실행이 가능해진다. "seq_cst가 왜 필요하냐"는 질문에 이 코드를 그대로 답으로 쓸 수 있다.
- Push는 버퍼 write 후 release fence → bottom 증가: thief가 bottom 증가를 보면 버퍼 내용도 반드시 보이게 하는 publish 패턴.
- `alignas(64)`로 top/bottom을 다른 캐시 라인에 분리 — **false sharing** 방지. 캐시 라인은 64B이고, 서로 다른 코어가 같은 라인의 다른 변수를 쓰면 라인이 코어 간 핑퐁하며 수십 배 느려진다.

**volatile과의 차이(단골)**: C++ volatile은 컴파일러 최적화 억제일 뿐 원자성도 코어 간 순서도 보장하지 않는다. MSVC 확장(/volatile:ms)이 acquire/release를 주긴 하지만 표준이 아니므로 동기화엔 반드시 atomic.

### 7. Lock-free 실전 — Chase-Lev 규약 위반 사고 (Phase 5-A)

**사고**: `CJobSystem::PushToSomeDeque`에서 **메인 스레드가 워커의 deque에 직접 Push**했다. Chase-Lev는 "Push/Pop은 owner 스레드만, 타인은 Steal만"이 알고리즘의 전제(그래서 owner 연산이 relaxed로 싸게 감)인데 이를 어겼다. 결과: Submit이 동시에 몰리면 memory ordering race → 워커 Pop이 빈 deque로 오인 → 잡이 실행되지 않아 `CJobCounter`가 영원히 0이 안 됨 → `WaitForCounter` 무한 루프 → 메인 hang. 36개 엔티티 루트를 TransformSystem 병렬 경로에 태우자 재현됐다(메모리: `project_phase_5a_complete.md`).

**수정** (`Engine/Private/Core/JobSystem.cpp` EnqueueJob, 현재 코드):
```cpp
// Worker 자신 → 자기 deque Push (Chase-Lev owner 규약 준수)
if (t_iWorkerIdx >= 0 && m_vecDeques[t_iWorkerIdx]->Push(item)) { m_WakeCV.notify_one(); return; }
// Main/외부 스레드/overflow → mutex 보호 Global Queue
{ std::lock_guard<std::mutex> lk(m_GlobalMutex); m_GlobalQueue.push(std::move(item)); }
m_WakeCV.notify_one();
```
워커는 자기 deque(LIFO, 캐시 친화) → Global Queue → 랜덤 victim Steal 순으로 소비(`TryExecuteOneJob`).

**면접 교훈으로 정리**: lock-free 알고리즘은 "코드"가 아니라 **불변식(invariant) 집합**이다. 논문의 스레드 역할 규약(owner-only push)을 하나라도 어기면 memory order 선택 전체가 무효가 된다. 그리고 이런 버그는 재현이 확률적이므로, 나는 이후 "락 있는 단순한 경로(전역 mutex 큐)를 correctness 기준선으로 두고, lock-free는 그 위의 최적화"라는 순서를 지킨다.

### 8. 가상 메모리 — 페이징, 페이지 폴트, 워킹셋

**정의**: 각 프로세스에게 연속된 가상 주소 공간을 주고, MMU가 페이지(기본 4KB) 단위로 물리 메모리에 매핑하는 기법.

**원리**:
- x64는 4단계(또는 5단계) 페이지 테이블. 매 접근마다 테이블을 걷지 않도록 **TLB**가 최근 변환을 캐시한다. TLB miss는 수십~수백 사이클.
- **페이지 폴트**: 매핑 없는/권한 없는 접근 시 CPU 예외 → OS 핸들러.
  - **Soft fault**: 물리 페이지는 있는데(스탠바이 리스트, 다른 프로세스와 공유) 매핑만 없던 경우 — μs 미만.
  - **Hard fault**: 디스크(페이지 파일/매핑된 파일)에서 읽어야 하는 경우 — SSD 수십~수백 μs, HDD ms 단위. **프레임 하나가 통째로 날아가는 비용.**
- **워킹셋(Working Set)**: 프로세스가 최근에 실제로 만진 물리 페이지 집합. 메모리 압박 시 OS가 워킹셋을 트리밍하고, 트리밍된 페이지를 다시 만지면 (운 좋으면 soft) fault가 난다.
- Windows API: `VirtualAlloc(MEM_RESERVE)`는 주소 공간만 예약(물리 X), `MEM_COMMIT`이 커밋 차지(commit charge)에 계상. 이 2단계 분리가 커스텀 할당자(선예약 후 점진 커밋)의 토대다. 대용량 연속 접근엔 large page(2MB)로 TLB 미스를 줄일 수 있다.

**게임 맥락**: 로딩 중 히치의 상당수가 hard fault다. 에셋을 memory-mapped file로 열면 "로딩"이 첫 접근 시점의 페이지 폴트로 흩어진다 — 스트리밍에는 유리하지만 첫 프레임 스파이크의 원인이 되기도 한다. 그래서 프리페치(`PrefetchVirtualMemory`)나 로딩 스레드에서 미리 touch하는 기법을 쓴다. Winters의 `.wmesh` 바이너리 포맷 전환(FBX 파싱 제거)도 본질은 "로딩 경로에서 페이지 폴트+파싱 비용을 줄여 프레임 17.8ms → 9ms" 작업이었다.

### 9. 스택 vs 힙 — 할당 비용과 커스텀 할당자

**스택**: 할당 = SP 레지스터 이동(사실상 0 사이클) + 항상 hot한 캐시. 해제는 스코프 종료와 함께 자동. 스레드당 기본 reserve 1MB(링커 옵션), 실제 커밋은 guard page를 건드릴 때마다 페이지 단위로 확장. 한계: 수명이 스코프에 묶이고 크기가 유한(재귀/큰 로컬 배열로 stack overflow → 이건 SEH로만 잡히는 예외).

**힙**: `malloc/new` → CRT 힙 → `HeapAlloc`(Windows 10부터 Segment Heap/LFH). 비용은 (a) 프리리스트/세그먼트 탐색, (b) **힙 내부 락**(멀티스레드 경합), (c) 부족 시 VirtualAlloc 시스템 콜, (d) 장기적 단편화. uncontended 소형 할당도 수십~수백 ns로 스택의 수십~수백 배, 경합하면 μs.

**게임에서 커스텀 할당자가 필요한 이유**:
1. **프레임 할당자(linear/arena)** — 프레임 시작에 포인터 리셋, 할당은 bump 1회. 프레임 수명 임시 데이터에 최적.
2. **풀 할당자** — 동일 크기 객체(파티클, 발사체, 잡 노드) 고정 슬롯 재사용. 단편화 0, 캐시 지역성 확보.
3. **per-thread 아레나** — 힙 락 경합 제거.

**Winters 실물**: `CWorkStealingDeque`는 4096 슬롯 고정 `std::array` — 런타임 Push/Pop/Steal 경로에 힙 할당이 0이다. 반면 잡 페이로드가 `std::function`이라 캡처가 크면 Submit마다 힙 할당이 발생하는데, 이를 알고 함수포인터+`void*` 기반 `JobDecl` 오버로드를 예비해 뒀다(`Engine/Public/Core/JobSystem.h`의 `Submit(const JobDecl&, ...)`). "현재 비용을 인지하고 개선 경로를 설계에 남겨뒀다"는 서사로 쓸 수 있다. 또한 deque를 `vector<unique_ptr<...>>`로 힙에 두는 이유가 "`std::atomic` 멤버 + `alignas(64)` 조합이 MSVC construct_at에서 실패"라는 실전 컴파일러 이슈였다는 것도 소재(`JobSystem.cpp` Initialize 주석).

### 10. thread_local의 함정 — Winters Profiler 사고와 2-pass 해법

**정의**: `thread_local` 변수는 스레드마다 별도 인스턴스를 가진다. Windows 구현은 TLS 슬롯(`TlsAlloc`) 또는 컴파일러의 `.tls` 섹션 + TEB 오프셋 접근.

**사고 (2026-04-28, 메모리: `project_session_2026_04_28_minion_combat.md`)**: NavigationSystem을 잡 워커로 병렬화하자마자 크래시. 원인은 CPUProfiler의 **단일 공유 scope stack(vector)** — 여러 스레드가 동시에 `push_back` → vector 재할당 중 다른 스레드가 참조 → UB/크래시. "프로파일러는 계측 코드니까 안전하겠지"라는 암묵적 가정이 병렬화 순간 깨진 것.

**해법 (현재 코드, `Engine/Private/Core/Profiler/CPUProfiler.cpp`)**:
```cpp
thread_local std::vector<PendingProfilerScope> t_vProfilerStack; // 스레드별 스택
// PushScope: 락 0회 — t_vProfilerStack에만 push
// PopScope : 자기 스택 pop 후, 완성된 이벤트를 m_Mutex 아래에서만 merge
```
"쓰기 빈도가 높은 로컬 상태는 thread_local, 공유 결과 수집만 mutex" — 락 구간을 merge 1회로 축소.

**이를 일반화한 것이 Decision/Apply 2-pass** (MinionAISystem):
- **DecisionPass(워커)**: World는 read-only, 결정은 per-slot 버퍼에 push만.
- **ApplyPass(메인)**: 싱글 스레드로 버퍼를 reduce하며 모든 write 수행.
- 같은 프레임 안에서 read와 write의 **시간을 분리**하므로 락 없이 race 0.

**슬롯 배정 규칙 — thread_local의 두 번째 함정**: `WaitForCounter`는 help-stealing이라 **메인 스레드도 잡을 실행**한다. 워커 인덱스로만 버퍼를 나누면 메인이 실행한 잡이 슬롯을 침범한다. 그래서 `CJobSystem::Get_WorkerSlot()`을 **main=0, worker=idx+1**로 정의하고 per-slot 버퍼 크기를 `WorkerCount+1`로 잡았다(`JobSystem.cpp:35-40`, thread_local `t_iWorkerIdx`가 -1이면 0 반환). "thread_local로 스레드를 구분했다"에서 끝나지 않고 "**어떤 스레드들이 이 코드를 실행할 수 있는가**를 전수 조사했다"가 핵심.

**추가 함정 목록(꼬리질문 대비)**:
- **Fiber와의 상성**: fiber가 다른 스레드로 이주(migration)하면 thread_local이 다른 인스턴스를 가리킨다. fiber 안전 상태는 FLS(`FlsAlloc`)나 fiber 컨텍스트 구조체에 넣어야 한다. Winters FiberShell이 fiber를 같은 스레드에서 create→switch→delete로 수명을 닫는(`TryExecuteItemOnFiber`) 이유 중 하나.
- DLL마다 thread_local 인스턴스가 분리될 수 있음(모듈 단위 TLS).
- 워커 스레드 풀 재사용 시 이전 작업의 잔류 상태 — 사용 전 초기화 규약 필요.
- 소멸 순서: 스레드 종료 시 thread_local 소멸자 실행 타이밍이 모듈 언로드와 얽히면 크래시.

### 11. Windows HANDLE 모델

**정의**: HANDLE은 커널 객체(프로세스, 스레드, 이벤트, 뮤텍스, 파일, 소켓, IOCP...)에 대한 **프로세스별 핸들 테이블의 불투명 인덱스**. 포인터가 아니다.

**원리**:
- 커널 객체는 커널 공간에 살고 참조 카운트로 수명 관리. `CloseHandle`은 내 테이블 엔트리 제거 + 참조 감소일 뿐, 다른 핸들/커널 참조가 남아 있으면 객체는 산다.
- 프로세스 간 전달: `DuplicateHandle`, 핸들 상속(bInheritHandle).
- 대부분의 커널 객체는 **signaled/non-signaled 상태**를 가져 `WaitForSingleObject/WaitForMultipleObjects`로 통일 대기 가능 — 스레드 핸들은 종료 시 signaled, auto-reset event는 웨이터 하나를 깨우며 리셋. 이 "무엇이든 기다릴 수 있는 통일 인터페이스"가 Windows 동기화의 정체성이다.
- 함정: 핸들 누수(프로세스 종료까지 커널 객체 잔존), `GetCurrentThread()`는 pseudo-handle(-2)이라 다른 스레드에 넘기면 "그 스레드 자신"을 가리킴 — 넘기려면 DuplicateHandle.

**게임 맥락**: 소켓(SOCKET)도 사실상 핸들이라 IOCP에 바인딩 가능하고, 파일 IO·이벤트·타이머를 하나의 대기 지점으로 묶을 수 있다. Winters 서버의 `CreateIoCompletionPort(reinterpret_cast<HANDLE>(socket), m_hIOCP, sessionId, 0)`(`Server/Private/Network/IOCPCore.cpp:200-204`)가 그 실물 — 소켓 핸들을 IOCP에 연결하며 completion key로 sessionId를 심는다.

### 12. IOCP (I/O Completion Port) 내부 동작

**정의**: 비동기(Overlapped) IO의 **완료 통지**를 큐잉하고, 등록된 워커 스레드 풀에 완료를 분배하는 커널 객체. Windows 고성능 서버의 표준.

**동작 원리 (Proactor 패턴)**:
1. `CreateIoCompletionPort(INVALID_HANDLE_VALUE, ...)`로 포트 생성 — 마지막 인자 NumberOfConcurrentThreads가 **동시에 깨어 활동할 스레드 수 상한**(0이면 코어 수).
2. 소켓/파일 핸들을 같은 함수로 포트에 **연관**(completion key 지정).
3. `WSARecv/WSASend/AcceptEx`를 OVERLAPPED 구조체와 함께 발행 — 즉시 리턴(ERROR_IO_PENDING).
4. 커널이 IO를 완료하면 완료 패킷(전송 바이트, key, OVERLAPPED*)을 포트 큐에 push.
5. 워커는 `GetQueuedCompletionStatus`로 dequeue. **스레드 웨이크업은 LIFO** — 가장 최근에 대기한(캐시가 뜨거운) 스레드부터 깨워 스위칭을 줄인다.
6. 활동 스레드가 블로킹되면 커널이 이를 감지해 concurrency 상한 내에서 다른 대기 스레드를 깨운다 — "블로킹 보정"이 IOCP 스레드 풀의 핵심 지능.

**select/epoll과의 구분(단골)**: select/poll/epoll(readiness 모델, Reactor)은 "읽을 수 있게 됨"을 알려주고 read는 유저가 한다. IOCP(completion 모델, Proactor)는 "**읽기가 끝났음**"을 알려준다 — 버퍼는 발행 시점에 커널에 넘겨져 있다. 따라서 OVERLAPPED와 버퍼는 완료까지 수명이 보장되어야 한다(스택에 두면 즉사).

**Winters 실물 (`Server/Private/Network/IOCPCore.cpp`)**:
- `CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, m_workerCount)`로 생성, listen 소켓과 세션 소켓을 바인딩.
- `AcceptEx`(함수 포인터 `m_pfnAcceptEx`, WSAIoctl로 획득)로 accept까지 비동기화, 완료 시 `SO_UPDATE_ACCEPT_CONTEXT` + `TCP_NODELAY` 설정 — Nagle 비활성은 게임 지연 최소화의 기본.
- 워커 루프: `GetQueuedCompletionStatus(..., INFINITE)` → `CONTAINING_RECORD(pOverlapped, IOContext, overlapped)`로 OVERLAPPED를 품은 컨텍스트 복원 → op(Accept/Recv/Send) 분기. `bytes == 0`인 Recv 완료 = 상대 정상 종료 → 세션 정리. 실패 완료도 반드시 dequeue되어 자원 해제 경로를 탄다 — "에러도 완료다"가 IOCP 자원 관리의 철칙.

### 13. Fiber — 유저 모드 스케줄링 (주력 무기)

**정의**: Fiber는 커널이 모르는 **유저 모드 실행 컨텍스트**(자체 스택 + 레지스터 저장 영역). 스레드 위에서 코드가 명시적으로 `SwitchToFiber`를 호출해야만 전환되는 **협조적(cooperative) 스케줄링** 단위다.

**스레드와의 차이**:
| | Thread | Fiber |
|---|---|---|
| 스케줄러 | 커널(선점형) | 내 코드(협조적) |
| 전환 비용 | ~1μs + 캐시 오염 | **수십 ns** (유저 모드 레지스터/SP 교환만, 커널 진입 0) |
| 블로킹 | 커널이 다른 스레드 실행 | fiber가 블로킹하면 **캐리어 스레드 전체가 블로킹** |
| 개수 | 수백 개면 부담 | 수천 개 가능(스택 메모리가 한계) |

**API**: `ConvertThreadToFiber`(스레드를 fiber 문맥으로) → `CreateFiber(stackSize, entry, param)` → `SwitchToFiber(h)` → `DeleteFiber(h)`. fiber 전용 TLS는 `FlsAlloc`(FLS).

**왜 게임 엔진이 fiber인가**: 잡 안에서 "다른 잡 완료를 기다림"이 생겼을 때, 스레드 모델이면 워커가 잠들어 코어가 놀거나 스레드를 초과 생성해야 한다. fiber면 대기 지점에서 fiber를 보류 리스트에 걸고 **같은 워커 스레드가 즉시 다른 잡의 fiber로 전환** — 코어가 항상 일한다. Naughty Dog(GDC 2015 "Parallelizing the Naughty Dog Engine")가 이 모델의 대표.

**Winters 실물 — FiberShell 모드 (`Engine/Private/Core/JobSystem.cpp:251-303`)**:
```cpp
// ExecuteItem: FiberShell 모드 + 워커 스레드 + 중첩 아님 → 잡을 fiber 위에서 실행
// TryExecuteItemOnFiber: ConvertThreadToFiber(1회) → CreateFiber → SwitchToFiber → DeleteFiber
// FiberShellEntry: t_bInsideJobFiber 가드 → ExecuteItemInline → 리턴 fiber로 Switch
```
`eJobExecutionMode::ThreadOnly / FiberShell`(`Engine/Public/Core/Fiber/FiberTypes.h`)을 런타임 전환 가능하게 해서, **public API 불변 상태로 내부만 fiber화**하는 마이그레이션 전략을 취했다. 현 단계는 잡 1개당 fiber 1개(shell)이고, 다음 단계가 fiber 풀 + 대기 중 fiber 보류(suspend) — 학습/계획 문서는 `.md/interview/engine/FIBER_LEARNING_GUIDE.md`, `.md/plan/engine/FIBER_JOB_SYSTEM_v2.md`, `FIBER_JOBSYSTEM_STABILIZATION_PLAN.md`.

**Fiber 함정 카탈로그(면접 고득점 구간)**:
1. **블로킹 콜 금지** — fiber 안에서 Sleep/블로킹 IO/락 대기하면 캐리어 스레드가 통째로 죽는다. 대기는 전부 "fiber 보류 + 스위치"로 변환해야 한다.
2. **락 쥔 채 yield 금지** — 같은 스레드가 다른 fiber에서 같은 락 재획득 시도 → self-deadlock. 재귀 뮤텍스는 더 위험(소유권이 "스레드" 기준이라 다른 fiber가 이미 획득한 걸로 처리됨).
3. **thread_local 오염** — fiber가 다른 스레드에서 재개되면 TLS가 바뀐다. FLS 또는 fiber 컨텍스트에 상태 보관.
4. **스택 크기** — fiber 스택은 고정. 깊은 재귀/큰 로컬이 조용히 넘칠 수 있어 스택 오버플로 감지 설계 필요.
5. **SEH/디버깅** — 콜스택이 fiber 단위로 조각나 크래시 덤프 해석이 어려워진다. fiber-safe 최적화 옵션(/GT)도 체크리스트.

**IOCP × Fiber 통합 설계 (서버, 메모리: `project_fiber_mastery_session_2026_05_11.md`)**: 검토 끝에 **IOCP 워커는 fiber화하지 않는 옵션 A**를 채택했다 — IOCP 완료 → mutex 큐에 push → GameRoom **Tick fiber**가 소비. 이유: (a) GetQueuedCompletionStatus 블로킹과 fiber 협조 스케줄링의 상성이 나쁘고, (b) 시뮬레이션 결정성(같은 입력 → byte-exact 스냅샷) 검증이 최우선인데 네트워크 스레드까지 fiber화하면 검증 면적이 폭발한다. Tick 스레드는 main-like(`t_iWorkerIdx == -1`)로 두고 Phase_SimulationSystems 4단계(Stat∥Cool → Buff → Move → Damage/Death)만 잡 병렬화. "fiber를 어디까지 침투시킬지 경계를 그은 의사결정" 자체가 어필 포인트다.

### 14. SEH와 minidump — 크래시 덤프 분석 흐름

**SEH(Structured Exception Handling)**: Windows의 OS 레벨 예외 메커니즘. access violation(0xC0000005), stack overflow, 0으로 나누기 같은 **하드웨어/OS 예외**를 `__try/__except/__finally`로 다룬다. C++ `try/catch`(테이블 기반, /EHsc)와 별개 계층이며, C++ 예외도 x64에서는 SEH 인프라 위에 구현된다. 어떤 핸들러도 처리하지 않은 예외는 `SetUnhandledExceptionFilter`로 등록한 최종 필터에 도달한다 — 여기가 크래시 리포터의 자리.

**minidump 흐름**:
1. 최종 필터에서 `MiniDumpWriteDump(hProcess, pid, hFile, MiniDumpWithIndirectlyReferencedMemory | ..., &exceptionInfo, ...)` 호출 — 스레드 컨텍스트/스택/모듈 목록(+옵션에 따라 힙 일부)을 .dmp로 저장.
2. 주의: 크래시한 프로세스 안에서 덤프를 쓰는 건 힙이 오염됐을 수 있어 위험 — 원칙은 **별도 watchdog 프로세스**가 덤프를 쓰는 것(프로세스 격리의 실전 사용처).
3. 분석: WinDbg에서 .dmp 열기 → 심볼 경로 설정(빌드 서버가 **빌드마다 PDB 보관**, 바이너리와 GUID 매칭) → `!analyze -v`(자동 분류) → `.ecxr`(예외 컨텍스트 복원) → `k`(콜스택) → `dv`/`dt`(변수/타입).
4. 대량 운영: 크래시를 콜스택 시그니처로 버킷팅해 빈도순 수정(Windows Error Reporting/Sentry/BugSplat 방식).

**Winters 현황(정직하게)**: 자체 코드에는 아직 `SetUnhandledExceptionFilter`/`MiniDumpWriteDump`가 없다(외부 Tracy만 자체 crash filter 보유 — `Engine/External/tracy/client/TracyProfiler.cpp:1524`). 2026-07-10 UE5.7 대비 감사에서 **minidump 부재를 HIGH 항목**으로 확정했고 로드맵 P0("배선만 하면 되는 것")에 올려 둔 상태다. 면접에서는 "분석 흐름은 체득했고, 내 엔진의 다음 P0가 unhandled filter + 별도 프로세스 덤프 라이팅"이라고 현재 상태와 계획을 정확히 말하는 편이 어설픈 과장보다 강하다.

### 15. DLL 로딩과 경계 — CRT 힙, dllexport

**로딩**: `LoadLibrary` → 이미지 매핑 → 의존성 재귀 로드 → 리로케이션/임포트 바인딩 → `DllMain(DLL_PROCESS_ATTACH)`. **loader lock**을 쥔 채 DllMain이 불리므로 그 안에서 스레드 생성·대기, LoadLibrary, COM 초기화는 데드락 지뢰다.

**CRT 힙 경계**: DLL이 정적 CRT(/MT)로 빌드되면 모듈마다 **CRT 힙이 따로** 생긴다. A 모듈에서 `new`한 것을 B 모듈에서 `delete`하면 다른 힙에 free → heap corruption. 동적 CRT(/MD) 통일이 1차 방어이고, 근본 원칙은 **"할당한 모듈이 해제한다"** — 그래서 엔진 API는 Create/Destroy 팩토리 쌍이나 deleter를 함께 넘기는 형태로 설계한다.

**dllexport와 C++ 타입**: STL 타입을 export 클래스 멤버로 노출하면 컴파일러/CRT 버전이 어긋날 때 ABI가 깨진다. Winters 규약: `WINTERS_ENGINE`(dllexport 매크로) 클래스가 `unique_ptr` 멤버를 가지면 **복사 생성/대입을 명시 delete** — `CJobSystem`(`JobSystem.h:33-34`), `CSystemSchedular`가 그 패턴이다(gotchas 2026-04-23 박제). C4251 경고를 침묵시키지 않고 소유권 시그니처로 응답한 사례.

**DLL 경계의 미묘한 실전 예 — 문자열 리터럴 주소**: 같은 `"NavigationSystem"` 리터럴도 모듈/번역단위가 다르면 주소가 다르다. CPUProfiler의 카운터 병합이 포인터 비교였다면 같은 카운터가 중복 행으로 갈라진다 — 그래서 `strcmp` 비교로 고정(`Engine/Private/Core/Profiler/CPUProfiler.cpp` AddCounter 주석). "DLL 경계가 힙만이 아니라 리터럴 identity까지 깨뜨린다"는 디테일은 실경험자만 말할 수 있다.

### 16. std::async / std::future 수명 문제

**함정의 본질**: `std::async(std::launch::async, ...)`가 반환한 future만의 특례 — **소멸자가 태스크 완료까지 블로킹**한다. 반환값을 받지 않으면 임시 future가 그 자리에서 파괴되므로, "비동기 호출"이라고 쓴 코드가 **완전히 동기**가 된다.

```cpp
std::async(std::launch::async, DoRequest); // 임시 future 소멸자가 대기 → 사실상 동기 호출
auto f = std::async(std::launch::async, DoRequest); // f 파괴 시점까지 진짜 비동기
```

**Winters 실전 (gotchas 2026-07-09 + `Client/Public/Network/Backend/CHttpClient.h`)**: CHttpClient의 async HTTP가 정확히 이 함정으로 **호출 시점 블로킹**이었고, 아이러니하게 그 블로킹 때문에 워커 람다의 raw `this` 캡처가 우연히 안전했다(객체보다 태스크가 먼저 끝나니까). 수정은 함정 제거와 수명 문제를 **한 change로**:
1. `vector<future<void>> m_PendingRequests`가 future 수명을 소유 — 호출 시점 비블로킹 확보, `PruneCompletedRequests`로 완료분 정리.
2. 블로킹 지점을 소멸자로 한정하는 계약을 헤더 주석으로 명문화("진행 중인 async 요청이 전부 끝날 때까지 대기 후 파괴된다").
3. raw this 의존 제거 — 워커는 멤버 대신 호출 시점 복사본 `RequestSnapshot`(host/port/authToken)만 읽어 `SetAuthToken`과의 race까지 차단. static `DoRequestWith(snapshot, ...)`로 this 접근 자체를 없앰.

**일반화**: 게임에선 std::async를 로딩 같은 저빈도 경로에만 쓰고, 프레임 경로는 잡시스템으로 통일한다. std::async는 스레드 생성/풀 정책이 구현 정의라 스파이크 제어가 안 되기 때문.

---

## 예상 질문 & 모범답변

### Q. 프로세스와 스레드의 차이를 설명해 보세요.

**답**: 프로세스는 자원 소유의 단위, 스레드는 실행/스케줄링의 단위입니다. 프로세스는 독립된 가상 주소 공간·핸들 테이블·힙을 갖고, 스레드는 그걸 공유하면서 스택·레지스터 컨텍스트·TLS만 따로 갖습니다. Windows에서 스케줄링 대상은 스레드입니다.
**왜 중요한가**: 격리(프로세스)냐 공유 효율(스레드)이냐의 트레이드오프입니다. 프로세스 간 통신은 IPC 비용과 직렬화가 붙지만 크래시가 격리되고, 스레드는 포인터를 그대로 공유하는 대신 race라는 대가를 냅니다.
**내 프로젝트**: Winters는 Client/Server 두 프로세스이고 각 내부는 JobSystem 워커가 World를 공유합니다. 크래시 덤프 라이팅처럼 격리가 이득인 기능만 별도 프로세스로 빼는 게 제 로드맵(P0 minidump watchdog)입니다.
**꼬리질문 대비**: "스레드가 공유 안 하는 건?" — 스택, 레지스터, TLS, TEB, (커널) 커널 스택. "프로세스 전환이 더 비싼 이유?" — CR3 교체로 인한 TLB 플러시 + 캐시 오염.

### Q. 컨텍스트 스위칭 때 실제로 무슨 일이 일어나고, 비용이 얼마나 되나요?

**답**: 커널 진입 → 현재 스레드 레지스터 저장 → 스케줄러가 다음 스레드 선택 → 컨텍스트 복원 → 유저 복귀. 같은 프로세스 스레드 간 직접 비용은 대략 1~2μs입니다. 하지만 진짜 비용은 간접 비용 — 새 스레드가 L1/L2/TLB를 cold 상태로 시작해서 실효 비용이 수십 μs까지 커집니다. 프로세스가 다르면 CR3 교체로 TLB까지 날아갑니다.
**트레이드오프**: 그래서 "스레드 수 = 코어 수, 스위치 최소화"가 게임 엔진 정석이고, 그보다 싼 전환이 필요하면 유저 모드로 내려갑니다.
**내 프로젝트**: 이 수치 감각이 Winters JobSystem 설계의 근거입니다 — 워커 `hardware_concurrency-2`개 고정, 잡 전환은 함수 호출이거나 FiberShell 모드에서 `SwitchToFiber`(수십 ns, 커널 진입 없음)입니다. 스레드 1μs vs fiber 수십 ns 분해는 fiber 학습 문서(`.md/interview/engine/FIBER_LEARNING_GUIDE.md`)에서 측정 계획까지 세워 체득했습니다.
**꼬리질문**: "μs면 싼 거 아닌가요?" — 60fps 프레임 예산이 16.6ms인데, 과도한 스레드로 프레임당 스위치가 수백 회면 ms 단위가 사라집니다. 게다가 캐시 오염은 스위치 횟수에 비례해 전체 코드가 느려집니다.

### Q. 게임 엔진에서 워커 스레드는 몇 개가 적당한가요? 근거는?

**답**: 물리 코어(또는 논리 코어) 수에서 이미 상시 점유하는 스레드(메인, 렌더, 오디오, IO)를 뺀 수입니다. CPU-bound 잡에 코어보다 많은 스레드를 주면 스위칭과 캐시 오염만 늘립니다. IO-bound는 반대로 초과 배정이 정당화되는데, 그래서 IO는 스레드를 늘리는 대신 비동기(IOCP)로 처리해 CPU 스레드 계산에서 빼버리는 게 맞습니다.
**내 프로젝트**: `CJobSystem::Initialize`가 `hw_concurrency > 2 ? hw-2 : 1`로 잡습니다. 그리고 유휴 워커가 yield 스핀으로 코어를 태우지 않게 CV 1ms 타임아웃 대기로 바꿨습니다 — yield는 READY 스레드가 없으면 즉시 리턴해서 사실상 busy-wait이 되기 때문입니다. 타임아웃을 둔 이유는 notify 누락(steal로만 잡이 생기는 경로)이 있어도 1ms 안에 자가 회복하게 하기 위해서입니다(`Engine/Private/Core/JobSystem.cpp` WorkerLoop).
**꼬리질문**: "하이퍼스레딩 논리 코어는 세나요?" — 잡 성격에 따라 다르며, 메모리 대기가 많은 잡은 SMT 이득이 있어 논리 코어 기준도 유효 — 측정으로 결정한다고 답하는 게 정답.

### Q. 뮤텍스와 스핀락, 언제 뭘 쓰나요?

**답**: 뮤텍스는 획득 실패 시 스레드를 재웁니다 — 대기 CPU 0, 대신 sleep/wake 커널 왕복(수 μs)이 붙습니다. 스핀락은 CAS 루프로 계속 시도합니다 — 스위치 비용 0, 대신 대기 내내 코어를 태우고 캐시 라인 핑퐁을 유발합니다. 규칙: 임계구역이 확실히 짧고(수십~수백 ns) 경합이 낮으며 코어 여유가 있을 때만 스핀락, 그 외 전부 뮤텍스.
**중요한 디테일**: 현대 뮤텍스(SRWLock, futex 계열)는 이미 "짧은 스핀 후 잠들기" 하이브리드라, 유저가 순수 스핀락을 직접 쓸 일은 측정으로 증명된 병목에서만 생깁니다. 그리고 스핀락은 우선순위 역전과 최악의 상성입니다 — 락 쥔 낮은 우선순위 스레드가 선점당하면 높은 우선순위 스레드가 스핀으로 코어를 점거해 락 해제를 스스로 막습니다.
**내 프로젝트**: Winters JobSystem 전역 큐는 그냥 `std::mutex`입니다. push/pop이 나노초급이라 스핀락 유혹이 있지만, correctness 기준선을 락으로 두고 lock-free 최적화는 per-worker Chase-Lev deque가 담당하는 역할 분리를 택했습니다. Phase 5-A에서 lock-free 규약 위반으로 hang을 겪은 뒤 세운 원칙입니다.

### Q. CRITICAL_SECTION, std::mutex, SRWLock의 차이는?

**답**: CRITICAL_SECTION은 프로세스 내 전용 하이브리드 락 — 스핀 카운트만큼 유저 모드에서 돌다가 커널 이벤트로 잠듭니다. 재귀 획득이 허용되고 40바이트입니다. SRWLOCK은 포인터 1개 크기, 비재귀, reader/writer(shared/exclusive) 지원, 정적 초기화 가능 — 현대 Windows의 기본 선택이고 MSVC `std::mutex`/`std::shared_mutex`가 이 위에 구현됩니다.
**트레이드오프**: 재귀 허용은 편의처럼 보이지만 락 계층 설계가 무너졌다는 신호라 저는 비재귀를 선호합니다. 재귀 뮤텍스는 fiber와의 상성도 최악입니다 — 소유권이 "스레드" 기준이라, 같은 스레드에서 도는 다른 fiber가 이미 획득한 락을 성공 획득으로 오판합니다.
**꼬리질문**: "SRWLock이 왜 더 빠른가?" — uncontended 경로가 인터락 연산 1회 + 커널 객체 lazy 할당 불필요(WaitOnAddress류 대기). "프로세스 간 락은?" — named mutex(커널 객체) 또는 공유 메모리 + 인터락.

### Q. 조건변수를 쓸 때 반드시 지켜야 하는 것과, 그 이유는?

**답**: 세 가지입니다. (1) 뮤텍스와 함께 쓰고, (2) 술어(predicate)를 루프/람다로 재검사하고, (3) notify 전에 공유 상태 변경을 뮤텍스 아래에서 끝낸다. 이유: **spurious wakeup**(신호 없이 깨어남)이 스펙상 허용되고, **lost wakeup**(대기 등록 전에 notify가 지나감)은 상태 변경과 대기 검사 사이의 race에서 옵니다. `wait(lk, pred)` 형태가 이 둘을 모두 방어합니다.
**내 프로젝트**: Winters 워커 웨이크업은 `m_WakeCV.wait_for(lk, 1ms)`입니다. 정석 predicate 대신 타임아웃을 쓴 건 의도적 설계입니다 — 잡 존재 여부가 3개 소스(자기 deque/전역 큐/steal 대상)에 분산돼 있어 단일 술어로 못 묶고, notify가 누락되는 경로(steal로만 소비 가능한 잡)가 있어도 1ms 안에 자가 회복하는 bounded-wait을 택했습니다. "왜 예외적으로 이렇게 했는지"를 설명할 수 있는 게 원칙 암기보다 강합니다.
**꼬리질문**: "notify_one을 뮤텍스 안에서? 밖에서?" — 밖에서 하면 hurry-up-and-wait을 줄이지만, 술어 변경이 락 안에서 끝났다면 둘 다 정확성엔 문제없다.

### Q. 세마포어는 뮤텍스와 뭐가 다르고 언제 쓰나요?

**답**: 뮤텍스는 소유권 개념이 있는 배타 락(잠근 스레드가 풀어야 함), 세마포어는 소유권 없는 카운터입니다(P/V를 다른 스레드가 해도 됨). 카운트 N이면 동시 N개 진입 허용. "자원이 N개 있다" 또는 "생산된 아이템 개수"라는 의미가 자연스러울 때 세마포어입니다.
**게임 예**: 동시 파일 IO 요청 수를 4로 제한하는 로더, 생산자-소비자 큐에서 아이템 수 카운팅, GPU 업로드 링버퍼 슬롯 관리. 반대로 단순 임계구역 보호에 세마포어(count=1)를 쓰는 건 소유권 검증이 없어 실수 여지만 늘립니다.
**꼬리질문**: "바이너리 세마포어 = 뮤텍스인가?" — 아니다. 소유권/재귀/우선순위 부스트 처리에서 다르고, 뮤텍스는 '해제는 소유자만'이라는 계약 위반을 잡아줄 수 있다.

### Q. 데드락 발생 4조건과, 실무에서 쓰는 예방법을 말해 보세요.

**답**: 상호 배제, 점유 대기, 비선점, 순환 대기 — 4개가 동시에 성립해야 하고 하나만 깨면 예방됩니다. 실무 1순위는 **전역 락 순서 규약**으로 순환 대기를 깨는 것입니다. 락에 계층 번호를 부여하고 항상 낮은 번호부터 획득, 역방향 획득은 코드리뷰/assert로 차단합니다. 두 개를 원자적으로 잡아야 하면 `std::scoped_lock(m1, m2)`(내부적으로 try-and-back-off)로 점유 대기를 깹니다.
**내 프로젝트**: 서버에 RoomManager(1 프로세스 × 100 룸 + 공유 잡 워커)를 설계할 때 lock acquisition order를 계획 문서의 Gotcha 항목으로 먼저 박았고, fiber 통합에서는 "`m_stateMutex`를 쥔 채 fiber yield 금지" 규약을 추가했습니다 — fiber는 락 쥔 채 양보하면 같은 스레드가 같은 락을 재획득하려는 self-deadlock이 나기 때문입니다. 또 봇 콤보 상태머신에서 "gate를 영원히 통과 못 하는 스텝에서 멈추는" 로직 데드락을 수정한 적이 있는데(커밋 3847f3f — gate 실패 시 스텝 전진으로 변경), 데드락 분석의 본질이 뮤텍스가 아니라 "대기 그래프에 탈출 간선이 있는가"라는 걸 체감한 사례입니다.
**꼬리질문**: "데드락 디버깅은 어떻게?" — 덤프를 떠서 모든 스레드 스택 확인(WinDbg `~*k`), 각자 어떤 락을 기다리는지 + 소유자 추적(`!locks`). 라이브면 Attach 후 동일. 예방적으로는 lock 계층 assert, 데드락 감지 스레드(락 획득 타임아웃 로깅).

### Q. 멀티스레드 프로그램이 멈췄습니다(hang). 어떻게 접근하나요?

**답**: 먼저 "데드락인지, 라이브락/무한 루프인지, 그냥 느린 건지"를 구분합니다. 디버거 attach 또는 덤프로 전 스레드 콜스택을 뜨고, (a) 전부 대기 상태에 락 사이클이 보이면 데드락, (b) 특정 스레드가 CPU를 태우며 같은 루프를 돌면 라이브락/조건 미충족 루프입니다. 후자는 "그 루프의 탈출 조건이 왜 영원히 거짓인가"를 데이터로 추적합니다.
**내 프로젝트**: Phase 5-A에서 메인 스레드가 `WaitForCounter` busy-wait 루프에 갇힌 hang을 겪었습니다. 락 사이클이 없어서 데드락이 아니었고, 원인은 Chase-Lev deque에 메인이 owner-push 규약을 어기고 Push해서 생긴 memory ordering race — 워커 Pop이 잡을 못 보고, 실행이 안 되니 atomic 카운터가 영원히 0이 안 되는 구조였습니다. 이후 워커 self-push + 메인은 mutex 전역 큐라는 단일 진입점(`EnqueueJob`)으로 재설계했습니다. 이 사고로 "hang = 데드락"이라는 선입견 대신 **대기 원인을 분류부터** 하는 습관이 생겼습니다.
**꼬리질문**: "재현이 안 되면?" — 저빈도 race는 스트레스 하네스(잡 폭주 제출)와 카운터 계측으로 재현 확률을 올리고, 그래도 안 되면 불변식 assert를 심어 최초 위반 지점에서 잡습니다.

### Q. std::atomic은 무엇을 보장하나요? volatile과는 뭐가 다른가요?

**답**: 두 가지를 보장합니다 — (1) 연산의 원자성(찢어진 read/write 없음, RMW의 lost update 없음), (2) 지정한 memory_order에 따른 스레드 간 가시성/순서. volatile은 컴파일러가 접근을 제거·재배열하지 못하게 할 뿐, CPU 레벨 원자성도 코어 간 순서도 보장하지 않습니다. MMIO용이지 동기화 도구가 아닙니다.
**하드웨어 관점**: x86에서 `fetch_add`는 `LOCK ADD`로 컴파일되어 해당 캐시 라인을 배타 소유한 채 수행됩니다. aligned 단순 load/store는 x86에선 원래 원자적이라, atomic의 진짜 부가 비용은 원자성보다 **순서 보장(fence)**에서 옵니다.
**내 프로젝트**: `CJobCounter`가 atomic 카운터(Increment/Decrement/Load)로 잡 완료를 추적하고, `m_bShutdown`은 release-store/acquire-load 짝, 통계성 라운드로빈은 relaxed로 — 변수마다 필요한 최소 보장만 명시했습니다(`Engine/Private/Core/JobSystem.cpp`).
**꼬리질문**: "atomic이면 무조건 lock-free인가?" — 아니다. `std::atomic<T>`는 T가 크면 내부 락으로 구현될 수 있고 `is_lock_free()`로 확인한다. 128비트 CAS(CMPXCHG16B)까지가 x64 하드웨어 한계.

### Q. memory_order acquire/release를 설명해 보세요.

**답**: release store와 acquire load가 같은 원자 변수에서 짝을 이루면(synchronizes-with), release **이전의 모든 메모리 쓰기**가 acquire **이후의 모든 읽기**에 보인다는 보장입니다. 핵심 용도는 publish 패턴 — 데이터를 다 써놓고 플래그/인덱스를 release로 올리면, 상대가 그 플래그를 acquire로 읽은 순간 데이터 전체가 유효합니다.
**하드웨어 관점**: x86은 TSO라 acquire/release가 사실상 공짜(컴파일러 재배열 금지만)이고, ARM에선 실제 배리어 명령(ldar/stlr)이 나갑니다. 그래서 x86에서만 테스트한 relaxed 남용 코드가 콘솔/모바일에서 터지는 겁니다.
**내 프로젝트**: `CWorkStealingDeque::Push`가 정확히 publish입니다 — 버퍼에 잡을 쓰고 release fence 후 bottom을 올립니다. thief의 Steal은 top을 acquire로 읽어서, bottom 증가가 보이면 버퍼 내용도 보이는 걸 보장합니다(`Engine/Public/Core/JobSystem/WorkStealingDeque.h`).
**꼬리질문**: "acquire/release로 안 되고 seq_cst가 필요한 경우는?" — 다음 질문.

### Q. seq_cst는 언제 진짜로 필요한가요?

**답**: **서로 다른 두 변수에 대한 store와 load의 순서가 모든 스레드에게 하나의 전역 순서로 보여야 할 때**입니다. 대표 패턴이 Dekker식 상호 배제 — "내 플래그를 쓰고(store) 상대 플래그를 읽는(load)" 구조에서, StoreLoad 재배열(내 store가 store buffer에 있는 동안 load가 먼저 실행)을 막아야 둘 다 진입하는 사고가 없습니다. acquire/release는 StoreLoad를 막지 못합니다.
**내 프로젝트 실물**: Chase-Lev deque의 `Pop`이 정확히 이 패턴입니다 — bottom을 내려 쓰고(store) top을 읽는데(load), 그 사이에 `atomic_thread_fence(seq_cst)`가 있습니다(`WorkStealingDeque.h:47`). 이 fence가 없으면 owner와 thief가 마지막 잡 하나를 동시에 가져가 이중 실행됩니다. 마지막 1개 경합은 seq_cst CAS로 승자를 판정합니다. x86에서 이 fence는 MFENCE/LOCK 명령으로 컴파일되어 실측 비용이 있는데, Pop당 1회로 설계된 것이 이 알고리즘의 미덕입니다.
**꼬리질문**: "seq_cst 기본값을 쓰면 안전하니 다 쓰면 되지 않나?" — 정확성은 얻지만 hot path에서 store마다 전체 배리어 비용을 낸다. 다만 저는 "측정 전엔 seq_cst, 병목 확인 후 완화 + 주석으로 근거 명시" 순서를 지지한다 — 완화가 틀리면 디버깅 비용이 수십 배다.

### Q. false sharing이 뭔가요? 어떻게 찾고 고치나요?

**답**: 서로 다른 코어가 **같은 캐시 라인(64B)** 안의 서로 다른 변수를 갱신할 때, 논리적으로 공유가 없는데도 캐시 코히런시가 라인 단위로 동작해 라인이 코어 간 핑퐁(invalidate 폭풍)하는 현상입니다. 증상은 "락도 없고 race도 없는데 코어를 늘릴수록 느려짐".
**해법**: 자주 다른 스레드가 쓰는 변수들을 `alignas(64)`(또는 `std::hardware_destructive_interference_size`)로 다른 라인에 분리, per-thread 데이터는 라인 단위 패딩, 카운터는 per-thread 누적 후 병합.
**내 프로젝트**: `CWorkStealingDeque`의 top과 bottom이 `alignas(64)`로 분리돼 있습니다 — top은 thief들이, bottom은 owner가 계속 갱신하므로 같은 라인이면 Steal이 owner의 Push/Pop을 느리게 만듭니다. Profiler의 per-thread scope stack(thread_local) 분리도 결과적으로 같은 효과를 냅니다.
**꼬리질문**: "어떻게 탐지?" — VTune/uProf의 HITM(캐시 라인 경합) 이벤트, 또는 의심 구조체에 패딩을 넣어보는 A/B 측정.

### Q. lock-free 자료구조를 직접 구현하거나 분석해 본 적 있나요?

**답**: Chase-Lev work-stealing deque(2005 논문)를 엔진 잡시스템에 구현했습니다. 구조는 owner가 bottom에서 Push/Pop(LIFO — 캐시가 뜨거운 최신 잡부터), thief가 top에서 Steal(FIFO — 경합 최소화)하는 링버퍼입니다. 핵심 동기화는 세 곳 — Push의 release publish, Pop의 seq_cst fence(StoreLoad 차단), 마지막 1개 경합의 CAS 판정입니다(`Engine/Public/Core/JobSystem/WorkStealingDeque.h`).
**실패 경험이 더 중요한 답**: 처음에 메인 스레드가 워커 deque에 직접 Push하는 코드(owner-only 규약 위반)를 뒀다가, Submit 폭주 시 워커가 잡을 못 보는 race → 카운터 미완료 → 메인 hang을 겪었습니다. lock-free 알고리즘은 memory order가 **스레드 역할 규약과 한 몸**이라, 규약을 하나 어기면 전체가 무효가 된다는 걸 배웠습니다. 수정 후 아키텍처는 worker self-push + 외부는 mutex 전역 큐 — "락 있는 단순 경로를 correctness 기준선으로, lock-free는 그 위의 최적화"라는 원칙도 이때 세웠습니다.
**꼬리질문**: "왜 owner Pop은 relaxed load로 시작해도 되나?" — bottom은 owner만 쓰므로 자기 스레드 내 순서(program order)로 충분, 타인과의 경합 지점(top)에서만 강한 오더링을 낸다. 이 비대칭이 Chase-Lev 성능의 핵심.

### Q. ABA 문제를 설명해 보세요.

**답**: CAS 기반 알고리즘에서 값이 A→B→A로 돌아왔을 때, CAS는 "값이 A인가"만 보므로 중간 변화를 놓치고 성공해 버리는 문제입니다. 전형적으로 lock-free 스택에서 pop한 노드가 free되고 같은 주소로 재할당되어 돌아오면, 옛 head를 든 스레드의 CAS가 성공하면서 자료구조가 깨집니다.
**해법**: (1) 태그/세대 카운터를 포인터에 결합(x64는 CMPXCHG16B로 포인터+카운터 128비트 CAS, 또는 상위 미사용 비트에 태그), (2) 노드 재사용을 지연시키는 epoch/hazard pointer, (3) 인덱스+세대 기반 핸들.
**내 프로젝트 연결**: Chase-Lev은 top이 단조 증가 카운터(래핑 인덱스는 별도 마스킹)라 ABA에 강한 구조입니다 — 주소가 아니라 세대성 있는 정수를 CAS하기 때문. 엔진 ECS에서 EntityID를 index+generation으로 설계하는 관행도 본질은 ABA(죽은 엔티티 슬롯 재사용 오인) 방지와 같은 아이디어입니다.

### Q. 가상 메모리는 왜 필요한가요? 페이징을 설명해 보세요.

**답**: 목적은 세 가지 — (1) 프로세스 간 격리(서로의 물리 메모리 접근 차단), (2) 연속된 주소 공간이라는 추상화(단편화된 물리 메모리 위에서도), (3) 물리 메모리보다 큰 워킹 공간(디스크 백킹, demand paging). 구현은 페이지(4KB) 단위 매핑 — 가상 주소를 다단계 페이지 테이블로 물리 프레임에 변환하고, MMU+TLB가 이를 하드웨어 가속합니다.
**비용 감각**: TLB 히트면 변환 비용 ~0, 미스면 페이지 워크 수십~수백 사이클. 그래서 데이터 지역성은 캐시뿐 아니라 TLB에도 이득이고, 대용량 연속 버퍼는 large page(2MB)로 TLB 엔트리를 절약할 수 있습니다.
**게임 맥락**: `VirtualAlloc`의 RESERVE/COMMIT 분리를 커스텀 할당자에 씁니다 — 주소 공간만 크게 예약해 두고 사용량에 따라 커밋을 늘리면, 리사이즈 시 포인터 무효화 없는 성장형 아레나를 만들 수 있습니다.

### Q. 페이지 폴트의 종류와 게임 성능에 미치는 영향은?

**답**: soft fault는 물리 페이지가 이미 메모리에 있는데(스탠바이 리스트/공유 페이지) 매핑만 만들면 되는 경우로 μs 미만입니다. hard fault는 디스크에서 읽어야 하는 경우로 SSD 수십~수백 μs, HDD면 ms — 16.6ms 프레임 예산에서 hard fault 몇 번이면 히치가 보입니다.
**대응**: (1) 로딩/스트리밍 스레드에서 미리 touch 또는 `PrefetchVirtualMemory`, (2) 자주 쓰는 에셋은 로드 시점에 페이지인 완료 보장, (3) 게임 중 새 할당 최소화(커밋 시점의 zero-fill 페이지 폴트도 비용), (4) 성능 측정 시 Hard Fault/sec 카운터(PerfMon/ETW)를 프레임 스파이크와 대조.
**내 프로젝트**: Winters에서 프레임 17.8ms→9ms 복구 작업의 큰 축이 로딩 파이프라인 개선(.wmesh 바이너리화)이었는데, 파싱 비용과 함께 파일 접근 패턴이 순차화되면서 폴트 비용이 예측 가능해진 것이 체감 요인이었습니다.
**꼬리질문**: "워킹셋이 뭔가?" — 프로세스가 최근 실제 사용한 물리 페이지 집합. 메모리 압박 시 OS가 트리밍하므로, '내 게임이 얼마나 상주해야 하는가'를 워킹셋 기준으로 측정·관리해야 백그라운드 전환 후 복귀 히치를 설명할 수 있다.

### Q. 스택 할당과 힙 할당의 비용 차이, 그리고 게임에서 커스텀 할당자를 쓰는 이유는?

**답**: 스택은 SP 이동뿐이라 사실상 0 비용 + 항상 hot 캐시입니다. 힙은 프리리스트/세그먼트 탐색 + **힙 내부 락** + 최악엔 시스템 콜 + 장기 단편화로, uncontended여도 수십~수백 ns, 멀티스레드 경합 시 μs입니다. 게임은 프레임당 수천 개의 짧은 수명 할당이 생기므로 범용 힙으로는 (a) 비용 자체, (b) 비용의 **비결정성**(스파이크), (c) 단편화 세 가지가 문제됩니다.
**커스텀 할당자 3종 세트**: 프레임 아레나(bump allocator — 프레임 끝에 통째 리셋), 풀(동일 크기 슬롯 재사용 — 파티클/발사체/네트워크 패킷), per-thread 아레나(힙 락 제거). 핵심은 "수명 패턴별로 할당 전략을 배정"하는 것.
**내 프로젝트**: `CWorkStealingDeque`는 4096 고정 슬롯 배열로 런타임 잡 경로 힙 할당 0입니다. 반면 `std::function` 잡 페이로드는 캡처가 크면 할당이 발생함을 인지하고 있고, 함수포인터+void* 기반 `JobDecl` 경로를 예비해 뒀습니다. "비용을 측정 가능하게 인지하고 개선 지점을 설계에 남긴다"가 제 방식입니다.
**꼬리질문**: "alloca/큰 로컬 배열의 위험은?" — 스택은 스레드당 유한(기본 1MB reserve)하고, 오버플로는 guard page 접근 시 SEH 예외로만 나타나 복구가 사실상 불가. fiber 스택은 더 작게 잡는 경우가 많아 더 민감하다.

### Q. thread_local은 어떻게 동작하고, 어떤 함정이 있나요? (실전 사례 요구)

**답**: 스레드마다 변수 인스턴스가 분리됩니다. 구현은 TEB에서 모듈별 TLS 블록을 찾아 오프셋 접근 — 접근 비용은 몇 명령 수준으로 쌉니다. 함정은 "분리됐으니 안전"이라는 착각이 **경계 조건**에서 깨지는 것입니다.
**실전 사례 (Winters CPUProfiler)**: 병렬화 전 프로파일러가 scope stack을 단일 공유 vector로 갖고 있었는데, NavigationSystem을 잡 워커로 돌리는 순간 동시 push_back → vector 재할당 race로 크래시했습니다. 수정은 (1) scope stack을 `thread_local std::vector<PendingProfilerScope>`로 분리하고(`Engine/Private/Core/Profiler/CPUProfiler.cpp:29`), (2) 완성된 이벤트 merge만 mutex로 보호. 여기서 끝이 아니라 이걸 팀 규약 "Decision/Apply 2-pass"로 일반화했습니다 — 워커 패스는 read-only + per-slot 버퍼 push만, 메인 패스가 싱글스레드로 reduce하며 모든 write. AI 시스템(MinionAI)에 적용해 락 없이 race 0을 만들었습니다.
**두 번째 함정(슬롯 배정)**: help-stealing 때문에 **메인 스레드도 잡을 실행**합니다. 워커 인덱스로만 버퍼를 나누면 메인 실행 잡이 침범하므로, `Get_WorkerSlot()`을 main=0/worker=idx+1로 정의하고 버퍼를 WorkerCount+1개로 잡았습니다(`JobSystem.cpp:35-40`). "thread_local 도입"이 아니라 "**이 코드를 실행할 수 있는 스레드의 전수 목록**"이 진짜 설계 대상입니다.
**꼬리질문**: "fiber에서는?" — fiber가 스레드를 이주하면 thread_local이 바뀐다. fiber 상태는 FLS나 fiber 컨텍스트에 둔다. Winters FiberShell이 fiber 수명을 한 스레드 안에서 닫는 이유 중 하나.

### Q. 병렬 ECS에서 데이터 레이스를 구조적으로 막는 방법은?

**답**: 락을 뿌리는 게 아니라 **접근 패턴을 제약**합니다. Winters에서 확립한 5단계 정책: (1) 작업 버퍼/스코프 스택은 thread_local, (2) 단순 카운터·플래그는 atomic, (3) 결과 수집은 per-slot 버퍼 + 메인 merge(락은 merge에만), (4) 컴포넌트 write는 self-entity만(자기 엔티티 순회 중 자기 것만), (5) cross-entity write는 금지하고 per-worker 버퍼에 "의도"를 쌓아 메인이 일괄 적용. (4)(5)를 합친 게 Decision/Apply 2-pass입니다.
**추가 레이어 — 스케줄러 계약**: 시스템마다 `Get_AccessContract()`(읽는/쓰는 컴포넌트 집합, bExclusive)를 선언하고, 같은 phase에서 계약이 충돌하는 시스템은 직렬화가 기본값입니다. "병렬화는 계약 검증을 통과한 시스템만"이라는 화이트리스트 접근이 사고를 막습니다.
**왜 이 방식인가**: 락 기반 병렬 ECS는 락 순서·경합·데드락이 컴포넌트 수만큼 조합 폭발합니다. read/write의 시간 분리는 검증 면적을 "버퍼 push가 스레드 안전한가" 하나로 줄입니다. 비용은 1프레임 지연(Apply가 세팅한 상태는 다음 프레임에 소비)인데, 게임플레이 AI에서는 허용 가능한 트레이드오프였습니다.

### Q. Windows HANDLE이 뭔가요? 포인터와 뭐가 다른가요?

**답**: 커널 객체에 대한 프로세스별 핸들 테이블 인덱스(불투명 값)입니다. 포인터가 아니므로 다른 프로세스에 값을 복사해 넘겨도 무의미하고, `DuplicateHandle`이나 상속으로 넘겨야 합니다. 커널 객체는 참조 카운트로 살고, CloseHandle은 참조 하나를 놓을 뿐입니다.
**실무 포인트**: (1) 핸들 누수는 커널 메모리 누수 — 게임 서버가 세션마다 이벤트/소켓을 만들면 반드시 소유권 규약(RAII 래퍼)이 필요합니다. (2) 대부분의 핸들이 waitable이라 WaitForMultipleObjects로 "스레드 종료 + IO 이벤트 + 타이머"를 한 지점에서 기다릴 수 있는 게 Windows 설계의 강점입니다. (3) SOCKET도 커널 객체 핸들이라 IOCP에 바인딩됩니다 — 제 서버 코드의 `CreateIoCompletionPort((HANDLE)socket, m_hIOCP, sessionId, 0)`이 그 사용례입니다(`Server/Private/Network/IOCPCore.cpp`).
**꼬리질문**: "CloseHandle을 두 번 하면?" — 에러 또는 (재사용된 인덱스면) **남의 핸들을 닫는** 대형 사고. 디버거에서는 예외 발생. 그래서 핸들 래퍼는 close 후 INVALID_HANDLE_VALUE로 리셋.

### Q. IOCP의 동작 원리를 설명하고, epoll과 비교해 보세요.

**답**: IOCP는 completion(Proactor) 모델입니다. 비동기 IO(WSARecv 등)를 OVERLAPPED와 함께 발행하면 즉시 리턴하고, 커널이 IO를 **끝낸 뒤** 완료 패킷을 포트 큐에 넣어 줍니다. 워커들은 GetQueuedCompletionStatus로 완료를 꺼내 후처리만 합니다. epoll은 readiness(Reactor) 모델 — "이제 읽으면 안 막힌다"를 알려주고 read 자체는 유저가 합니다. IOCP는 버퍼를 발행 시점에 커널에 넘기므로 데이터 복사 타이밍이 다르고, 완료 단위로 일이 분배되므로 스레드 풀과의 결합이 자연스럽습니다.
**내부 동작 디테일(차별화)**: (1) 포트 생성 시 concurrency 값(0=코어 수)이 "동시에 깨어 활동할 스레드 수"를 제한하고, 활동 스레드가 블로킹되면 커널이 감지해 추가 스레드를 깨웁니다. (2) 대기 스레드 웨이크업이 **LIFO**라 캐시가 뜨거운 스레드가 재사용됩니다. (3) 완료 순서는 발행 순서와 무관하므로 프로토콜 레벨 시퀀싱은 유저 몫입니다.
**내 프로젝트**: Winters 서버는 IOCP 워커 + AcceptEx(accept까지 비동기) + CONTAINING_RECORD로 IOContext 복원 + op별(Accept/Recv/Send) 분기 구조입니다. Recv 완료 bytes==0을 정상 종료로 처리하고, 실패 완료도 dequeue 경로에서 세션 정리를 태웁니다 — "에러도 완료 패킷"이라 자원 해제가 한 경로로 모입니다(`Server/Private/Network/IOCPCore.cpp:206-299`).
**꼬리질문**: "워커 스레드 몇 개?" — 후처리가 CPU-bound면 코어 수 내외. 우리 서버는 IO 후처리를 얇게 유지(파싱 후 큐잉)하고 시뮬레이션은 Tick 스레드로 넘기므로 소수로 충분.

### Q. OVERLAPPED 구조체와 버퍼의 수명은 누가 책임지나요?

**답**: 발행자입니다. 비동기 발행 후 완료 패킷이 돌아올 때까지 OVERLAPPED와 IO 버퍼는 이동/해제되면 안 됩니다 — 커널이 그 메모리에 직접 씁니다. 스택에 두면 함수 리턴 후 커널이 죽은 스택에 쓰는 메모리 오염이 됩니다.
**실무 패턴**: OVERLAPPED를 확장한 per-operation 컨텍스트(우리의 IOContext — op 종류, sessionId, 버퍼 내장)를 힙/풀에 두고, 완료 시 `CONTAINING_RECORD(pOverlapped, IOContext, overlapped)`로 복원합니다. 소켓당 outstanding recv 1개 유지, send는 큐잉해 완료 시 다음 send 발행 — 미완료 IO가 있는 채 소켓/컨텍스트를 해제하지 않도록 참조 카운트나 상태 머신으로 방어합니다.
**꼬리질문**: "취소는?" — CancelIoEx 후에도 완료 패킷(실패 상태)은 온다. 즉 '취소했으니 해제'가 아니라 '취소 완료 패킷을 받고 해제'가 맞다. 이 규칙이 IOCP 자원 관리의 90%다.

### Q. Fiber가 뭔가요? 스레드와 뭐가 다르고, 게임 엔진은 왜 쓰나요?

**답**: 커널이 모르는 유저 모드 실행 컨텍스트입니다. 자체 스택과 레지스터 저장 공간을 갖고, `SwitchToFiber` 호출로만 전환되는 협조적 스케줄링 단위입니다. 스레드 전환이 커널 진입+스케줄러+캐시 오염으로 μs급인 반면, fiber 전환은 유저 모드 레지스터/스택 포인터 교환뿐이라 수십 ns급입니다.
**게임 엔진이 쓰는 이유**: 잡 그래프에서 "자식 잡 완료 대기"가 생겼을 때, 스레드 모델은 워커가 잠들어 코어가 놀거나(under-subscription) 스레드를 초과 생성(over-subscription)해야 합니다. fiber 모델은 대기 지점에서 해당 fiber만 보류하고 같은 워커가 즉시 다른 잡 fiber로 전환 — 코어 활용률과 지연을 동시에 잡습니다. Naughty Dog 엔진이 대표 사례입니다.
**내 프로젝트 (주력 분야)**: Winters JobSystem은 `eJobExecutionMode::FiberShell` 모드에서 잡을 fiber 위에서 실행합니다 — 워커가 ConvertThreadToFiber로 fiber 문맥이 되고, 잡마다 CreateFiber→SwitchToFiber→DeleteFiber(`Engine/Private/Core/JobSystem.cpp:272-303`). ThreadOnly와 런타임 전환 가능하게 해서 public API를 바꾸지 않고 내부를 단계 이식하는 전략입니다. 다음 단계는 fiber 풀 + WaitForCounter에서 fiber 보류(진짜 non-blocking wait)로, 계획은 `.md/plan/engine/FIBER_JOB_SYSTEM_v2.md`에 박아 뒀습니다.
**꼬리질문**: "왜 처음부터 full fiber로 안 갔나?" — fiber는 함정(TLS, 락 across yield, 블로킹 콜)이 시스템 전역 규약을 요구한다. shell 모드로 fiber 실행 경로를 먼저 안정화하고 규약을 만든 뒤 대기/보류를 얹는 게 검증 면적을 통제하는 순서라고 판단했다.

### Q. Fiber를 쓸 때 조심해야 할 것들을 아는 대로 말해 보세요.

**답**: (1) **블로킹 콜 금지** — fiber가 Sleep/블로킹 IO/락 대기하면 캐리어 스레드가 통째로 멎어 그 위의 모든 fiber가 굶습니다. 대기는 전부 "보류 + 스위치"로 변환해야 합니다. (2) **락을 쥔 채 yield 금지** — 같은 스레드의 다른 fiber가 그 락을 다시 잡으려 하면 self-deadlock. 특히 재귀 뮤텍스는 스레드 기준 소유권이라 다른 fiber의 획득을 성공으로 오판합니다. (3) **thread_local 오염** — 스레드 이주 시 TLS가 바뀝니다. FLS(FlsAlloc) 또는 fiber 컨텍스트에 상태를 둡니다. (4) **고정 스택 오버플로** — fiber 스택은 작게 잡는 경우가 많아 깊은 재귀가 조용히 넘칩니다. (5) **디버깅/크래시 해석** — 콜스택이 fiber 단위로 조각나므로 fiber inspector 같은 관측 도구를 함께 만들어야 합니다. 컴파일러 fiber-safe TLS 최적화(/GT)도 체크 항목.
**내 프로젝트**: 서버 IOCP×Fiber 통합 설계 때 이 함정들을 결정표로 만들었습니다 — GameRoom `m_stateMutex` 구간은 fiber yield 금지(락 안에서는 main-like 동작), IOCP 워커는 fiber화하지 않고 completion을 큐로 넘겨 Tick fiber가 소비(옵션 A). "fiber를 어디에 침투시키지 않을 것인가"를 먼저 정한 게 실전 교훈입니다.

### Q. IOCP 서버와 Fiber 잡시스템을 통합한다면 어떻게 설계하겠습니까?

**답 (실제로 설계한 내용)**: 결정 기준은 두 개였습니다 — 시뮬레이션 결정성(같은 입력 → byte-exact 스냅샷)과 검증 면적 통제. 결론은 **경계 분리**: (1) IOCP 워커는 fiber화하지 않습니다. GetQueuedCompletionStatus 블로킹 대기와 협조 스케줄링은 상성이 나쁘고, 네트워크 스레드가 fiber가 되면 결정성 검증 범위가 폭발합니다. (2) 완료 패킷은 파싱 후 mutex 큐로 게임 스레드에 전달, (3) GameRoom Tick이 fiber 진입점이 되어 시뮬레이션 6개 시스템 중 병렬 가능한 4단계(Stat∥Cooldown → Buff → Move → Damage/Death)만 잡으로 fan-out, (4) Tick 스레드는 main-like(t_iWorkerIdx=-1)로 두고 help-stealing으로 참여, (5) 스케일아웃은 1 프로세스 × 100 룸이 워커 풀을 공유하는 RoomManager — 이때 룸 간 락 획득 순서 규약을 문서에 선행 박제.
**어필 포인트**: 이 설계의 본질은 "fiber가 좋다"가 아니라 **"권위 서버의 결정성이 최상위 제약이고, 병렬화는 그 제약 아래에서만 허용된다"**는 우선순위 판단입니다. 검증 계획(§6 결정성 검증)을 병렬화 계획보다 중요하게 뒀습니다. (근거: 메모리 `project_fiber_mastery_session_2026_05_11.md`, 서버 코드 `Server/Private/Network/IOCPCore.cpp`)

### Q. SEH가 뭔가요? C++ 예외와 어떻게 다른가요?

**답**: SEH는 Windows OS 레벨의 예외 메커니즘으로, access violation·stack overflow·정수 0 나누기 같은 하드웨어/OS 예외를 `__try/__except(필터)/__finally`로 다룹니다. C++ 예외(try/catch)는 언어 레벨이고 x64에서 테이블 기반 unwinding으로 구현되는데, 내부적으로는 SEH 인프라 위에 얹혀 있습니다. 핵심 차이: C++ catch는 AV를 잡지 못합니다(/EHa로 섞는 건 소멸자 보장이 깨질 수 있어 권장되지 않음). 게임에서는 "AV를 잡아서 계속 실행"이 아니라 **"안 잡히면 최종 필터에서 덤프 뜨고 죽는다"**가 정답 구조입니다.
**게임 실무 구조**: `SetUnhandledExceptionFilter`로 최종 필터 등록 → 필터에서 MiniDumpWriteDump → 업로드 → 종료. 덤프 라이팅은 크래시한 프로세스의 힙이 오염됐을 수 있으므로 별도 watchdog 프로세스가 하는 게 원칙입니다.
**꼬리질문**: "__finally와 C++ 소멸자 차이?" — __finally는 SEH unwinding에서도 실행 보장되는 블록. C++ RAII와 역할이 겹치지만 SEH 경로에서는 C++ 소멸자 실행이 보장되지 않는 조합이 있어 섞어 쓰지 않는 게 규율. "vectored exception handler는?" — 프레임 기반보다 먼저 전역으로 불리는 후킹 지점(Tracy 같은 프로파일러가 crash filter로 사용 — 실제로 우리 엔진의 외부 라이브러리 Tracy가 SetUnhandledExceptionFilter를 씁니다).

### Q. 크래시 덤프(minidump) 분석 흐름을 설명해 보세요.

**답**: (1) 수집 — unhandled exception filter에서 MiniDumpWriteDump로 .dmp 생성(스레드 컨텍스트, 스택, 모듈 목록 + 옵션에 따른 메모리). (2) 심볼 매칭 — 빌드마다 PDB를 보관하고 심볼 서버에 등록, 덤프의 모듈 GUID와 일치해야 스택이 풀립니다. **"모든 릴리즈 빌드의 PDB를 남긴다"가 이 파이프라인의 절반**입니다. (3) 분석 — WinDbg에서 `!analyze -v`로 1차 분류, `.ecxr`로 예외 컨텍스트 복원, `k`로 콜스택, `dv/dt`로 지역변수/타입, 멀티스레드 이슈면 `~*k`로 전 스레드 스택과 `!locks`. (4) 운영 — 콜스택 시그니처로 버킷팅해 빈도순으로 수정.
**내 프로젝트 (정직한 현황 + 계획)**: Winters는 2026-07 UE5.7 대비 감사에서 minidump 부재를 HIGH로 확정했고, 로드맵 P0 "배선만 하면 되는 항목"으로 올려 둔 상태입니다. 자체 코드에 SetUnhandledExceptionFilter가 아직 없다는 걸 감사로 **정확히 알고 있다는 것** 자체가 제 관리 방식입니다 — 크래시 리포팅은 콘텐츠보다 먼저 깔려야 하는 인프라라는 우선순위 판단과 함께 답합니다.
**꼬리질문**: "릴리즈 최적화 빌드에서 스택이 이상하면?" — FPO/인라이닝으로 프레임이 접힌 것. 인라인 심볼 정보(/Zo), 의심 지점 주변 디스어셈블리와 레지스터로 재구성. "재현 안 되는 크래시는?" — 덤프 버킷의 공통 분모(특정 맵/특정 GPU/특정 프레임 단계)를 메타데이터로 함께 수집해 통계로 좁힌다.

### Q. DLL 경계에서 메모리를 넘길 때 뭘 조심해야 하나요?

**답**: 원칙은 **"할당한 모듈이 해제한다"**입니다. 모듈마다 CRT 힙이 다를 수 있어(특히 /MT 정적 CRT), A에서 new한 걸 B에서 delete하면 다른 힙에 반환 → heap corruption입니다. /MD로 CRT를 통일해도 컴파일러 버전이 갈리면 STL ABI가 어긋날 수 있으므로, 안전한 export 표면은 (1) C 스타일 인터페이스 또는 (2) Create/Destroy 팩토리 쌍 + 인터페이스 포인터, (3) deleter를 함께 전달하는 스마트 포인터입니다.
**내 프로젝트**: Winters 엔진 DLL 규약으로 "WINTERS_ENGINE(dllexport) 클래스 + unique_ptr 멤버 = 복사 생성/대입 명시 delete"를 gotchas에 박제했고 `CJobSystem`, `CSystemSchedular`가 그 패턴입니다. 미묘한 사례로, 프로파일러 카운터 이름을 문자열 리터럴 **포인터**로 비교하면 DLL/번역단위마다 리터럴 주소가 달라 같은 카운터가 중복 행으로 갈라집니다 — strcmp 비교로 고정했습니다(`Engine/Private/Core/Profiler/CPUProfiler.cpp` AddCounter). DLL 경계가 힙만이 아니라 "리터럴 identity" 같은 가정까지 깨뜨린다는 실례입니다.
**꼬리질문**: "DllMain에서 하면 안 되는 일은?" — loader lock을 쥔 채 불리므로 스레드 생성·조인, LoadLibrary, 동기화 대기 금지. 초기화는 명시적 Init 함수로 뺀다.

### Q. std::async의 함정을 아는 대로 설명해 보세요.

**답**: 가장 큰 함정은 **future 소멸자 블로킹**입니다. std::async(launch::async)가 반환한 future만의 특례로, 소멸자가 태스크 완료까지 기다립니다. 반환값을 버리면 임시 future가 즉시 파괴되므로 "비동기"라고 쓴 호출이 완전 동기가 됩니다. 부수 함정: launch policy 미지정 시 deferred가 될 수 있어 get() 전까지 실행 자체가 안 될 수 있고, 스레드 생성 정책이 구현 정의라 프레임 경로에서 스파이크 제어가 안 됩니다.
**내 프로젝트 (그대로 겪음)**: CHttpClient의 AsyncGet/AsyncPost가 정확히 이 함정으로 호출 시점 블로킹이었습니다. 더 흥미로운 건 그 블로킹 덕에 람다의 raw this 캡처가 **우연히** 안전했다는 점 — 함정 하나를 고치면 숨어 있던 수명 버그가 드러나는 구조였습니다. 그래서 한 change로 같이 고쳤습니다: `vector<future<void>> m_PendingRequests`가 future를 소유해 진짜 비동기화하고, 블로킹은 소멸자 계약으로 한정(헤더에 명문화), 워커는 this 대신 호출 시점 `RequestSnapshot` 복사본만 읽게 해 SetAuthToken과의 race까지 제거했습니다(`Client/Public/Network/Backend/CHttpClient.h`, gotchas 2026-07-09).
**꼬리질문**: "그럼 게임에서 std::async는 언제 쓰나?" — 빈도 낮고 수명이 명확한 곳(툴, 초기 로딩)만. 프레임 경로는 잡시스템으로 통일. "future 없이 fire-and-forget 하려면?" — detach된 스레드보다 소유권 있는 큐/풀에 제출하고 종료 시 drain하는 구조가 안전.

### Q. 우선순위 역전(priority inversion)이 뭔가요? Windows는 어떻게 다루나요?

**답**: 낮은 우선순위 스레드 L이 락을 쥔 상태에서, 높은 우선순위 H가 그 락을 기다리는데, 중간 우선순위 M이 L을 선점해 L이 락을 못 놓는 상황 — H가 M에게 실질적으로 밀리는 역전입니다. 화성 패스파인더 사고로 유명합니다.
**Windows의 접근**: 뮤텍스 우선순위 상속이 없습니다. 대신 밸런스 셋 매니저가 오래 READY 상태인 기아 스레드의 우선순위를 주기적으로 부스트해 락 소유자가 결국 실행되게 하는 **확률적 완화**입니다. 따라서 실시간성 보장이 필요하면 설계로 풀어야 합니다 — 우선순위 차이가 있는 스레드 간 락 공유 최소화, 특히 높은 우선순위 스레드(오디오)와 일반 스레드가 같은 락을 잡지 않게 lock-free 링버퍼로 통신.
**게임 맥락**: 오디오 콜백 스레드(MMCSS로 높음)가 게임 스레드와 뮤텍스를 공유하면 언더런(소리 끊김)으로 즉시 드러납니다. 오디오-게임 통신을 SPSC lock-free 큐로 하는 것이 업계 표준인 이유입니다. 스핀락+우선순위 차이 조합이 최악(높은 쪽이 스핀으로 코어를 점거해 락 해제를 스스로 방해)이라는 것도 함께 답합니다.

### Q. "락 없이 스레드 간 데이터를 넘기는" 방법을 설계해 보세요.

**답**: 패턴 선택부터: (1) **SPSC 링버퍼** — 생산자 1/소비자 1이면 head/tail 인덱스 각각 단일 작성자라 acquire/release만으로 완성. 가장 싸고 검증 쉬움. (2) **per-thread 버퍼 + 프레임 경계 merge** — N 생산자도 각자 자기 버퍼에만 쓰고, 동기화 지점(프레임 끝)에서 싱글스레드가 수거. 락은 수거 1회. (3) **MPMC가 정말 필요하면** 검증된 구현(Chase-Lev, moodycamel) 채택 — 직접 구현은 논문 규약 준수를 전제로만.
**핵심 원리**: lock-free의 본질은 "락 제거"가 아니라 **작성자 수를 구조적으로 1로 줄이는 것**입니다. 단일 작성자가 되는 순간 필요한 동기화가 publish(release/acquire) 하나로 줄어듭니다.
**내 프로젝트**: (2)가 Winters의 Decision/Apply 2-pass + Get_WorkerSlot per-slot 버퍼이고, (3)의 실패와 재설계가 Phase 5-A Chase-Lev 사고입니다. 프로파일러 merge, AI 결정 수집, 서버 IOCP→Tick 전달(여긴 의도적으로 mutex 큐 — 경합 빈도가 낮아 lock-free가 과설계) 각각에 다른 패턴을 배정했습니다. "전부 lock-free"가 아니라 **경합 프로파일별 배정**이 답이라는 걸 강조합니다.

### Q. 게임 서버에서 스레드 모델을 어떻게 잡겠습니까? (종합 설계 질문)

**답 (Winters 서버 실물 기준)**: 세 층으로 나눕니다. (1) **IO 층** — IOCP 워커 소수(코어 수 이하). 완료 패킷 파싱까지만 하고 게임 상태는 건드리지 않습니다. (2) **시뮬레이션 층** — 룸당 논리적 Tick 흐름(스레드 또는 fiber). 게임 상태의 **단일 작성자**로서 결정성을 보장합니다. 커맨드는 IO층에서 mutex 큐로 넘어와 tick 시작에 일괄 소비. (3) **잡 층** — 시뮬레이션 내부의 데이터 병렬(스탯/쿨다운/이동 등 시스템 단위)을 공유 워커 풀로 fan-out, 완료는 카운터 대기.
**이 구조의 근거**: 게임 상태에 락을 거는 대신 "상태마다 작성자 1명" 원칙으로 락 자체를 없애고, 스케일은 룸 수 × 프로세스로 냅니다. 스레드 수가 룸 수에 비례하지 않도록 워커 풀을 공유하는 게 100룸 설계의 핵심이고, 이때 룸 간 상호작용(매치메이킹 등)만 락 순서 규약 대상이 됩니다.
**꼬리질문**: "tick이 밀리면?" — tick 예산 계측 + 초과 시 degrade 정책(스냅샷 빈도 하향 등)을 먼저 설계. "왜 룸당 스레드가 아닌가?" — 100룸 = 100스레드는 스위칭/메모리 낭비. 룸은 논리 단위, 실행은 풀에서 빌리는 게 맞다.

### Q. QueryPerformanceCounter는 어떻게 동작하나요? 게임 타이밍에서 주의점은?

**답**: QPC는 인바리언트 TSC(constant/invariant TSC — 전력 상태와 무관하게 일정 주파수) 기반의 고해상도 단조 시계입니다. QueryPerformanceFrequency로 초당 틱을 얻어 변환합니다. 해상도는 보통 100ns급 이상이고, 멀티코어 간 동기화는 OS/HAL이 보정합니다(과거 멀티소켓에서 코어 간 불일치 이슈가 있었지만 현대 플랫폼에선 invariant TSC로 해소).
**주의점**: (1) GetTickCount(15.6ms 해상도)나 timeGetTime과 혼용 금지, (2) 프레임 델타는 QPC로 재되 스파이크 클램핑(디버거 브레이크, 페이지 폴트로 델타 폭주) 필요, (3) Sleep 기반 페이싱은 타이머 해상도(timeBeginPeriod) 의존 — 정밀 페이싱은 waitable timer + 짧은 스핀 하이브리드.
**내 프로젝트**: Winters CPUProfiler가 QPC 기반입니다 — Create에서 `QueryPerformanceFrequency` 역수로 ticks→ms 변환 계수를 만들고, Push/PopScope가 QPC 틱을 기록합니다(`Engine/Private/Core/Profiler/CPUProfiler.cpp:32-38`). 프레임 17.8ms→9ms 최적화도 이 프로파일러의 스코프 트리로 병목을 특정하며 진행했습니다.

### Q. 메모리 누수/힙 손상을 Windows에서 어떻게 추적하나요?

**답**: **누수**: (1) CRT 디버그 힙(_CrtSetDbgFlag, _CrtDumpMemoryLeaks)으로 종료 시점 미해제 블록 + 할당 번호 브레이크, (2) 장기 추적은 ETW 기반(Windows Performance Recorder heap 프로파일) 또는 자체 할당자 태깅(카테고리별 카운터를 프레임 오버레이에 노출 — 게임은 이게 제일 실용적), (3) 커널 객체 누수는 작업 관리자/Process Explorer 핸들 카운트.
**힙 손상**: (1) PageHeap/Application Verifier — 할당 뒤에 guard page를 붙여 오버런 순간 AV로 즉사시킴(주소가 곧 범인), (2) ASan(MSVC /fsanitize=address) — use-after-free, 오버런을 섀도 메모리로 검출, (3) 손상이 늦게 터지는 경우(free 리스트 오염) 덤프의 힙 메타데이터 분석.
**핵심 태도**: 손상은 "터진 곳"이 아니라 "쓴 곳"을 찾는 문제라, 증상 지점 디버깅보다 검출 도구로 **오염 순간을 앞당기는** 게 정석입니다. DLL 경계 힙 불일치(다른 모듈 new/delete)도 힙 손상의 단골 원인이라 모듈 소유권 규약과 함께 답합니다.

### Q. 가상 함수 호출, 시스템 콜, 컨텍스트 스위치, fiber 스위치의 비용을 순서대로 감 잡아 보세요.

**답**: 대략의 자릿수 감각으로 — L1 히트 ~1ns, 가상 함수 호출(간접 분기, 예측 성공 시) 수 ns, CAS/LOCK 연산 ~10-40 사이클, uncontended 뮤텍스 획득 ~20ns(인터락 1회), **fiber 스위치 수십 ns**(유저 모드 레지스터 교환), 시스템 콜 왕복 ~100-300ns(SYSCALL + 커널 프롤로그), contended 뮤텍스(잠들기) 수 μs, **스레드 컨텍스트 스위치 직접 1-2μs + 캐시 오염 간접 수십 μs**, hard page fault SSD 수십-수백 μs, HDD ms.
**왜 이 감각이 중요한가**: 설계 선택이 전부 이 표에서 나옵니다 — "잡 하나가 10μs 미만이면 잡 오버헤드가 지배하니 배칭해라", "프레임 경로에서 시스템 콜을 줄여라", "스레드 대신 fiber 스위치로 대기를 처리해라"가 모두 자릿수 비교입니다.
**내 프로젝트**: fiber 학습 계획에 "thread switch 1μs vs fiber 20ns 분해"를 측정 항목으로 박고 RDTSC 기반 ping-pong 벤치를 설계했습니다(`.md/interview/engine/FIBER_LEARNING_GUIDE.md` 트랙). 수치를 외운 게 아니라 **측정할 계획을 세워 본** 경험으로 답합니다.

### Q. 스레드를 안전하게 종료시키는 방법은?

**답**: 원칙은 **협조적 종료**뿐입니다 — atomic 종료 플래그 세팅 + 대기 지점 전부 깨우기 + join. TerminateThread는 절대 금지: 스레드가 락을 쥔 채, 힙 조작 중, CRT 내부 상태 중간에 증발해 프로세스 전체가 오염됩니다.
**실무 디테일**: (1) 플래그는 release로 쓰고 루프 조건에서 acquire로 읽기, (2) CV 대기는 notify_all로, 블로킹 IO는 CancelIoEx/소켓 close로, IOCP는 PostQueuedCompletionStatus로 깨움 패킷 주입, (3) join 전에 해당 스레드가 더 이상 새 작업을 못 받게 입구부터 닫기(순서: 입구 차단 → 플래그 → 깨우기 → join → 자원 해제).
**내 프로젝트**: `CJobSystem::Shutdown`이 그 순서입니다 — `m_bShutdown.store(true, release)` → `m_WakeCV.notify_all()` → 전 워커 join → deque 해제. 워커 루프는 매 사이클 `load(acquire)`로 확인하고, CV 대기가 1ms 타임아웃이라 notify가 유실돼도 종료가 밀리지 않습니다. 종료 후 Submit이 와도 죽지 않게 shutdown 상태면 인라인 실행으로 폴백하는 방어도 있습니다(`EnqueueJob` 첫 분기).
**꼬리질문**: "IOCP 워커 종료는?" — GetQueuedCompletionStatus INFINITE 대기 중이므로 m_bRunning 플래그 + null 패킷(PostQueuedCompletionStatus)이나 포트 close로 깨워서 루프 탈출. 우리 서버 워커 루프도 pOverlapped==nullptr && !m_bRunning 경로로 빠져나옵니다.

### Q. 캐시 코히런시(MESI)를 아는 대로 설명해 보세요.

**답**: 코어별 프라이빗 캐시(L1/L2)가 같은 메모리를 복제해 갖는 시스템에서, 라인 단위(64B)로 Modified/Exclusive/Shared/Invalid 상태를 유지하며 "같은 주소는 하나의 최신 값"을 보장하는 프로토콜입니다. 코어가 쓰려면 라인을 E/M로 배타 소유해야 하고, 다른 코어 사본은 invalidate됩니다.
**성능 함의 3가지**: (1) 쓰기 공유가 잦은 라인은 코어 간 소유권 이동(핑퐁)으로 수십~수백 사이클씩 소모 — 경합 카운터가 느린 이유이자 per-thread 누적 후 병합이 빠른 이유. (2) false sharing — 다른 변수라도 같은 라인이면 같은 비용. (3) 원자 RMW(LOCK)는 이 프로토콜 위에서 라인을 잠깐 독점하는 것이라, 경합 없는 atomic은 싸고 경합하는 atomic은 락만큼 비쌉니다.
**중요 구분**: 코히런시는 "단일 주소의 일관성"이고, **서로 다른 주소 간 순서**는 메모리 컨시스턴시 모델(TSO 등)의 영역입니다 — "MESI가 있는데 왜 memory_order가 필요하냐"는 꼬리질문의 답이 바로 이 구분 + store buffer의 존재입니다.
**내 프로젝트**: WorkStealingDeque의 alignas(64) 분리, 프로파일러 per-thread 스택, per-slot 결정 버퍼 — 전부 "쓰기 소유권을 코어별로 나눠 라인 이동을 없앤다"는 같은 원리의 적용입니다.

### Q. 지금까지 겪은 가장 어려운 동시성 버그와, 그로부터 바꾼 습관은?

**답 (스토리로)**: 두 개를 연결해 말합니다. **첫째, Profiler thread_local race** — NavigationSystem 병렬화를 켜자마자 크래시. 원인은 프로파일러의 공유 vector 스택이었는데, "계측 코드는 데이터 경로가 아니니 안전하다"는 무의식적 가정이 문제였습니다. 병렬화는 해당 시스템만이 아니라 **그 프레임에 실행되는 모든 코드**의 스레드 안전을 요구한다는 걸 배웠고, 수정(thread_local 분리 + merge만 락)을 Decision/Apply 2-pass와 worker-safety 5정책이라는 팀 규약으로 일반화해 gotchas에 박제했습니다. **둘째, Chase-Lev owner 규약 위반 hang** — 크래시도 아니고 로그도 없는 무한 대기. 재현이 엔티티 수(36개)에 의존하는 확률적 버그였고, 원인은 논문 알고리즘의 스레드 역할 규약 위반이었습니다.
**바뀐 습관 3개**: (1) 병렬화 전 "이 프레임에 실행되는 전 코드"의 write 지점 전수 조사, (2) lock-free는 불변식 문서화 없이는 도입 금지 + 락 기반 기준선 유지, (3) hang/race 증상엔 코드 추론 누적 대신 **카운터 계측과 스트레스 재현을 먼저** — 실제로 이 사고 때 협업 AI와 stale 분석을 3회 반복하다 프로파일러 카운터 5분으로 원인을 특정했던 게 결정적 교훈이었습니다.

---

## 내 프로젝트 연결 포인트

면접에서 개념 질문이 나올 때 아래 문장으로 실전 경험을 연결한다.

1. **잡시스템을 직접 만들며 OS 비용을 체득했다** — "Chase-Lev work-stealing deque를 memory_order 수준까지 직접 구현했고(`Engine/Public/Core/JobSystem/WorkStealingDeque.h`), owner-only push 규약을 어겨서 생긴 확률적 hang을 디버깅하며 lock-free는 코드가 아니라 불변식이라는 걸 배웠습니다."
2. **thread_local race를 구조적 규약으로 승화시켰다** — "프로파일러 크래시 하나를 고치는 데서 멈추지 않고, Decision/Apply 2-pass와 main=0/worker=idx+1 슬롯 규칙, worker-safety 5정책으로 일반화해 이후 모든 시스템 병렬화의 기준으로 삼았습니다."
3. **Fiber를 주력 무기로 선언하고 마스터리 트랙을 설계했다** — "스레드 스위치 1μs vs fiber 수십 ns라는 비용 구조에서 출발해, JobSystem에 FiberShell 모드를 넣고(ThreadOnly와 런타임 전환, public API 불변) 서버 IOCP×Fiber 통합까지 결정표로 설계했습니다. fiber를 어디에 침투시키지 **않을지**(IOCP 워커, 락 구간)를 먼저 정한 게 핵심이었습니다."
4. **IOCP 서버를 바닥부터 짰다** — "AcceptEx, CONTAINING_RECORD 컨텍스트 복원, bytes==0 종료 처리, 실패 완료의 자원 해제 경로까지 IOCP의 completion 모델을 직접 다뤘고(`Server/Private/Network/IOCPCore.cpp`), IO층/시뮬레이션층/잡층 3층 스레드 모델로 결정성과 병렬화를 분리했습니다."
5. **수명/경계 버그를 계약으로 봉인하는 습관** — "std::async future 소멸자 블로킹 함정을 CHttpClient에서 직접 겪고, future 소유 + 소멸자 계약 명문화 + this 대신 스냅샷 복사로 한 번에 고쳤습니다. dllexport+unique_ptr 복사 삭제, 문자열 리터럴 주소의 DLL 경계 문제 같은 것들도 전부 gotchas 문서로 박제해 팀 규약화합니다."
6. **부재를 아는 것도 실력** — "minidump 파이프라인이 아직 없다는 걸 UE5.7 대비 감사로 확정하고 P0 로드맵에 올렸습니다. 크래시 리포팅이 콘텐츠보다 먼저라는 우선순위 판단까지가 시스템 프로그래밍 역량이라고 생각합니다."

---

## 마지막 점검 체크리스트

- [ ] 프로세스=자원 소유, 스레드=스케줄링 단위. 스레드 사유물: 스택/레지스터/TLS/TEB.
- [ ] 컨텍스트 스위치: 직접 1~2μs + 간접(캐시/TLB 오염)이 본체. 프로세스 간은 +TLB 플러시.
- [ ] Windows 스케줄러: 0~31 우선순위, 선점형, 퀀텀(클라 ~2틱/서버 ~12틱), 동적 부스트, 우선순위 상속 없음(안티-스타베이션 부스트).
- [ ] 동기화 선택: 기본 SRWLock/std::mutex → 개수면 세마포어 → 조건이면 CV(술어 필수) → 변수 하나면 atomic → 측정된 초단기 구간만 스핀락.
- [ ] 데드락 4조건(상호배제/점유대기/비선점/순환대기) — 실무 해법은 락 순서 규약 + scoped_lock. fiber는 "락 쥔 채 yield 금지" 추가.
- [ ] combo deadlock = 로직 레벨 무한 대기(gate 미통과 stall) → "대기 그래프의 탈출 간선" 프레임으로 설명.
- [ ] memory_order: relaxed=원자성만 / acq-rel=publish 짝 / seq_cst=서로 다른 변수 store↔load 전역 순서(StoreLoad 차단). x86=TSO, ARM=약한 모델.
- [ ] Chase-Lev 3대 지점: Push release publish / Pop seq_cst fence / 마지막 1개 CAS. **owner-only push 규약 위반 → hang 사고** 스토리 준비.
- [ ] false sharing: 64B 라인, alignas(64) 분리 (deque top/bottom 실물).
- [ ] 가상 메모리: RESERVE/COMMIT 분리, soft(μs 미만) vs hard fault(SSD 수십~수백μs), 워킹셋 트리밍, TLB.
- [ ] 스택=SP 이동+hot 캐시 / 힙=탐색+락+시스템 콜. 커스텀 할당자 3종: 프레임 아레나/풀/per-thread.
- [ ] thread_local 사고: 공유 vector 스택 crash → thread_local 분리 + merge 락 → **2-pass + slot(main=0, worker=idx+1, 버퍼 N+1)** 규약화. help-stealing 때문에 메인도 잡 실행!
- [ ] HANDLE=핸들 테이블 인덱스, 커널 객체 refcount, waitable 통일 인터페이스, SOCKET도 IOCP 바인딩 가능.
- [ ] IOCP: Proactor(완료 통지), concurrency 상한+블로킹 보정, LIFO 웨이크업, OVERLAPPED/버퍼 수명은 발행자 책임, 에러도 완료 패킷, CancelIoEx 후에도 완료는 온다.
- [ ] Fiber: 유저 모드 협조 스케줄링, 스위치 수십 ns. 함정 5종(블로킹 콜/락 yield/TLS/고정 스택/디버깅). IOCP 워커는 fiber화 안 함(옵션 A) — 결정성 우선.
- [ ] SEH≠C++ 예외. 최종 필터 → MiniDumpWriteDump(별도 프로세스 권장) → PDB 매칭 → !analyze -v / .ecxr / k. Winters는 P0 로드맵(현재 부재를 정확히 인지).
- [ ] DLL 경계: 할당한 모듈이 해제, /MD 통일, Create/Destroy 쌍, dllexport+unique_ptr=복사 delete, DllMain=loader lock 지뢰, 리터럴 주소 identity 함정(strcmp).
- [ ] std::async: launch::async future 소멸자 블로킹 → 버리면 동기화. CHttpClient 수정 3종(future 소유/소멸자 계약/스냅샷 복사) 스토리.
- [ ] 스레드 종료: 플래그(release/acquire) → 깨우기(notify_all/PostQueuedCompletionStatus) → join. TerminateThread 금지.
- [ ] 비용 자릿수: L1 1ns < 가상호출 수ns < CAS 수십 사이클 < fiber 수십ns < 시스템 콜 수백ns < 스레드 스위치 μs급 < hard fault 수십μs~ms.
