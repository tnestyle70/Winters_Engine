# 10. 동시성 구조 · JobSystem — Chase-Lev Work-Stealing과 "끄는 용기"

> **상태 동기화 (2026-07-11)**: “껐던 날”은 2026-04-23의 역사적 대응이다. 현재 Client JobSystem은 active ThreadOnly이고 Main-push race 경로는 수정됐다. FiberShell은 컴파일됐지만 live enable 호출이 없으며 FiberFull/Server 통합/전용 stress는 미완료다. 상세 근거는 [2026-07-11 상태 감사](../../plan/2026-07-11_JOB_SYSTEM_CHASE_LEV_FIBER_STATE_AUDIT.md)를 따른다.
>
> 면접 대본 + 지식 베이스. 코드 문법(atomic/memory_order/thread_local 자체)은 `.md/interview/cpp/09_concurrency.md` 담당. 여기서는 **구조, 의사결정, 사고와 해결**만 다룬다.

---

## ① 한 줄 정의

"Winters의 동시성은 3층입니다. 맨 아래에 Chase-Lev work-stealing deque 기반 JobSystem, 그 위에 read/write 접근권 선언으로 시스템을 자동 배칭하는 ECS 스케줄러, 맨 위에 게임 로직의 race를 구조적으로 배제하는 Decision/Apply 2-pass 패턴 — 락을 잘 쓰는 게 아니라 **락이 필요한 지점 자체를 줄이는 구조**를 목표로 했습니다."

이 한 문장 뒤에 바로 붙일 수 있는 두 번째 문장: "그리고 이 시스템에서 제일 자랑하고 싶은 건 병렬화를 켠 날이 아니라, race를 발견하고 **병렬화를 껐던 날**의 의사결정입니다."

---

## ② 구조와 데이터 흐름

### 2.1 계층 지도

```text
[게임 로직 계층]   Decision/Apply 2-pass (LocalUnitAISystem)
                   - DecisionPass: worker 가 World read-only + per-slot buffer push
                   - ApplyPass:    main 이 단일 스레드로 모든 write
                          │
[스케줄링 계층]    CSystemSchedular
                   - Phase(uint32) 오름차순 std::map = 프레임 내 실행 순서 계약
                   - 같은 Phase 안에서 DescribeAccess(read/write 선언) 충돌 검사
                     → 충돌 없는 시스템끼리 SystemBatch 로 그리디 배칭
                   - batch >= 2 && JobSystem 있음 → 병렬, 아니면 순차
                          │
[Job 계층]         CJobSystem (worker N = hw_concurrency - 2, 최소 1)
                   - worker 마다 Chase-Lev deque 1개 (고정 4096 슬롯)
                   - + mutex 보호 GlobalQueue (하이브리드)
                   - CJobCounter (원자 카운터만, cv 없음) + help-stealing 대기
```

### 2.2 잡 하나의 생애 (Submit → 소비)

```text
Submit(job, &counter)
  └ counter.Increment()  → EnqueueJob 단일 진입점
       ├ Shutdown / 미초기화(N==0)      → 그 자리에서 인라인 즉시 실행 (fallback)
       ├ 호출자가 worker 본인            → 자기 deque 에 Push (Chase-Lev owner 규약)
       └ Main·외부 스레드 / deque 넘침   → GlobalQueue (mutex) 로 push
     → m_WakeCV.notify_one()

Worker 소비 우선순위 (TryExecuteOneJob):
  1) 자기 deque Pop   — LIFO, 캐시 친화
  2) GlobalQueue      — Main 이 넣은 잡 수거
  3) 랜덤 victim Steal — FIFO, xorshift PRNG 로 victim 선택 (자기 자신 제외)
  잡 없음 → m_WakeCV.wait_for(1ms)  (yield 스핀 아님)

WaitForCounter(&counter):
  counter > target 인 동안 — 대기 스레드도 잡을 훔쳐 실행 (help-stealing)
  - worker 라면: TryExecuteOneJob (자기+전역+steal 전부)
  - Main 이라면: GlobalQueue drain → round-robin steal → 없으면 yield
```

근거 코드 (전부 실측 확인):
- `Engine/Public/Core/JobSystem.h` — worker/deque/GlobalQueue/WakeCV 멤버 구조, "Phase 5-B 에서 내부가 Fiber 로 교체되어도 public API 불변" 주석.
- `Engine/Private/Core/JobSystem.cpp` — `thread_local t_iWorkerIdx = -1`(비-worker 식별), `EnqueueJob` 4분기, `WorkerLoop`의 `wait_for(1ms)`, `WaitForCounter` help-stealing.
- `Engine/Public/Core/JobSystem/WorkStealingDeque.h` — Chase & Lev 2005, `kCapacity = 4096`(2의 거듭제곱 + `&` 마스크 인덱싱), `alignas(64)`로 top/bottom 캐시라인 분리.
- `Engine/Public/Core/JobCounter.h` — "cv/mutex 제거됨. 대기는 WaitForCounter 로" 주석, Increment는 relaxed / Decrement는 acq_rel.
- `Engine/Private/ECS/SystemScheduler.cpp` — `kMinParallelBatchSize = 2`, 접근권 충돌 시 flushBatch 하는 그리디 배칭, `Scheduler::ParallelBatches` 등 profiler counter 4종.

### 2.3 스레드 식별 규약: main=0 / worker=idx+1

`CJobSystem::Get_WorkerSlot()`은 `t_iWorkerIdx < 0`(Main·외부)이면 0, worker면 idx+1을 반환한다 (`Engine/Private/Core/JobSystem.cpp`). per-worker 버퍼를 쓰는 코드는 버퍼 크기를 `GetWorkerCount() + 1`로 잡는다 — `Client/Private/GamePlay/System/LocalUnitAISystem.cpp`의 `Ensure_SlotBuffers()`가 정확히 `need = WorkerCount + 1u`로 구현. 이 +1이 중요한 이유: **WaitForCounter 안에서 Main도 help-stealing으로 잡을 실행**하므로, Main에게 slot 0을 전용으로 주지 않으면 Main이 실행한 잡과 worker 잡이 같은 버퍼에 겹친다.

---

## ③ 핵심 설계 결정과 트레이드오프

### 결정 1 — Chase-Lev per-worker deque + 전역 큐 하이브리드

- **왜**: 프레임마다 수십~수백 개의 짧은 잡(Transform 루트 갱신, 미니언 AI 판단)을 뿌리는데, 단일 공유 큐는 모든 worker가 한 락을 두드려 확장이 안 된다.
- **대안**: (a) mutex 단일 큐, (b) moodycamel 같은 외부 MPMC lock-free 큐, (c) per-worker Chase-Lev deque.
- **선택 이유**: Chase-Lev는 owner가 bottom에서 락 없이 Push/Pop(LIFO, 방금 만든 잡이 캐시에 뜨거울 때 소비)하고 thief만 top에서 CAS 경쟁(FIFO)하는 구조라, "자기 잡은 공짜, 훔칠 때만 비용"이라는 게임 프레임 워크로드에 맞는다. 외부 라이브러리 대신 직접 구현한 건 memory ordering을 체득하는 게 이 프로젝트의 목적 중 하나였기 때문이다.
- **감수한 비용**: (1) 순수 Chase-Lev는 **owner만 push 가능** — Main이 잡을 넣을 통로가 없어서 mutex 보호 GlobalQueue를 하이브리드로 붙였다. Main 제출 경로만 락 비용을 낸다. (2) 고정 4096 슬롯이라 넘치면 Push가 false를 반환하고 GlobalQueue로 우회 — 동적 확장(circular array growth)은 미구현 상태로 남긴 MVP 트레이드오프.

### 결정 2 — WaitForCounter: 조건변수 블로킹 대신 help-stealing

- **왜**: fork-join(잡 뿌리고 완료 대기)에서 대기 스레드가 자면 코어 하나가 통째로 논다. 특히 Main이 프레임 중간에 WaitForCounter를 부르는데 Main이 자버리면 프레임이 그만큼 늘어난다.
- **대안**: (a) CJobCounter에 cv/mutex를 넣고 Wait() — 초기 버전이 실제 이랬다. (b) 대기 중에도 잡을 직접 실행하는 help-stealing.
- **선택 이유**: (b). `Engine/Public/Core/JobCounter.h` 주석에 "cv/mutex 제거됨. 기존 JobCounter::Wait() API 삭제"라고 결정을 박제했다. 대기 스레드가 자기 몫을 실행하니 지연이 줄고, cv 깨우기 누락 같은 lost-wakeup 클래스가 카운터 경로에서 사라진다.
- **감수한 비용**: 대기 중 실행한 잡이 길면 counter가 0이 된 뒤에도 그 잡을 끝낼 때까지 리턴이 늦는다(latency 상한이 잡 하나 길이만큼 흔들림). 또 스핀 성분이 있어 잡이 정말 없을 땐 yield로 양보만 한다.

### 결정 3 — 병렬 안전은 스케줄러가, 기본값은 "직렬"

- **왜**: 시스템 작성자가 매번 락을 고민하게 하면 반드시 하나는 빠뜨린다.
- **대안**: (a) 컴파일 타임 타입 태그(Unity ECS 스타일), (b) 런타임 read/write 선언, (c) 전부 수동.
- **선택 이유**: (b). `ISystem::DescribeAccess`로 read/write 컴포넌트를 선언하면 스케줄러가 충돌 없는 시스템만 같은 배치로 묶는다. 핵심은 기본 구현이 `builder.UnknownWritesAll()`이라는 것 (`Engine/Public/ECS/ISystem.h`) — **선언하지 않은 시스템은 무조건 충돌 판정 = 절대 병렬 배치에 못 들어간다.** 병렬화 이득은 옵트인, race 위험은 옵트아웃 불가.
- **감수한 비용**: 미선언 시스템이 많으면 배치가 잘게 쪼개져 병렬 이득이 0에 수렴한다. 성능이 아니라 정확성이 기본값이라는 걸 의도적으로 받아들였다.

### 결정 4 — 병렬화 대상 선정: "크고, 독립적이고, 측정으로 증명된 것만"

선정 기준 세 가지를 세웠다:

1. **작업량이 임계를 넘는가** — `Engine/Private/ECS/Systems/TransformSystem.cpp`의 `kParallelThreshold = 16`, `kTransformRootsPerJob = 8`. 주석 원문: "Submit/steal/Counter 오버헤드가 병렬 이득보다 커지는 임계. 프로파일링으로 조정". 루트 16개 미만이면 단일 스레드 fallback이 그냥 빠르다.
2. **엔티티 간 독립적인가** — Transform은 루트 서브트리끼리 독립(루트 단위 분할), 미니언 AI는 판단 단계만 독립(2-pass로 분리).
3. **profiler counter로 손익이 증명되는가** — 스케줄러가 `Scheduler::ParallelBatches/SubmittedJobs`, Transform이 `Transform::RootCount/SubmittedRootJobs`를 매 프레임 계측한다. "병렬화했더니 빨라진 것 같다"는 금지, 숫자로만 말한다.

- **감수한 비용**: 임계값이 하드코딩 상수라 머신마다 최적점이 다르다. 대신 상수를 코드 최상단에 노출하고 "프로파일링으로 조정"을 주석으로 계약해 뒀다.

### 결정 5 — Submit race 발견 시 병렬화를 끄고 진행한 것 (Phase 5-A, 2026-04-23)

이 챕터에서 가장 중요한 의사결정이라 ④에서 사고 자체를 다루고, 여기서는 판단 논리만:

- **왜 껐나**: race의 근본 원인(Chase-Lev owner 불변식 위반)은 파악했지만 수정 방향이 3가지 옵션으로 갈렸고, 그날 안에 검증까지 끝낼 수 없었다. hang은 크래시보다 나쁜 증상이다 — 덤프도 안 남고 재현 조건(루트 36개 이상)이 콘텐츠 진행에 따라 확률적으로 바뀐다.
- **왜 끄는 게 가능했나**: 구조 덕분이다. TransformSystem은 `m_pJobSystem == nullptr`이면 단일 스레드 경로로 fallback하도록 처음부터 설계했으므로, `Set_JobSystem` 호출부만 주석 처리하면 **기능 회귀 0으로** 빌드·플레이가 가능했다.
- **트레이드오프**: 병렬 성능을 며칠 포기하는 대신, 미니언 AI 등 다른 개발 트랙이 hang 없이 계속 굴러갔다. "동시성 기능은 끌 수 있는 스위치와 동작하는 fallback을 가진 채로 추가한다"가 이후 모든 병렬화의 전제 조건이 됐다.

---

## ④ 어려웠던 점과 해결 — war story 3건

### 4.1 Submit race: Main이 Chase-Lev의 owner 불변식을 깼다

**증상**: 구조물 30개가 로드되면서 Transform 루트가 36개 → `kParallelThreshold(16)` 초과 → 병렬 경로 최초 진입 → Main 스레드 hang. 크래시가 아니라 무한 대기.

**원인**: 당시 `PushToSomeDeque`가 round-robin으로 **Main 스레드에서 worker의 deque에 직접 Push**했다. Chase-Lev는 owner 스레드만 bottom을 만진다는 불변식 위에 memory ordering이 설계돼 있는데(owner의 Push/Pop은 relaxed 위주, thief의 Steal만 CAS), 제3의 스레드가 bottom에 쓰면 이 가정이 전부 무너진다. worker의 Pop이 빈 deque로 오인 → 잡이 실행되지 않음 → counter가 영원히 0으로 안 내려감 → `WaitForCounter` 무한 루프.

**해결 경로**: 당일은 위 결정 5대로 병렬화 OFF. 정식 수정은 세 옵션(Main 전용 submission queue / deque N+1개로 Main도 owner화 / MPMC 큐 교체) 중 첫째 계열로 수렴했고, 현재 코드가 그 결과다 — `Engine/Private/Core/JobSystem.cpp`의 `EnqueueJob`:

```cpp
// Worker 자신 - 자기 deque push (Chase-Lev Owner)
if (t_iWorkerIdx >= 0 && static_cast<std::uint32_t>(t_iWorkerIdx) < N)
{
    if (m_vecDeques[t_iWorkerIdx]->Push(item)) { m_WakeCV.notify_one(); return; }
    // overflow
}
// main / 외부 / overflow - global queue
{
    std::lock_guard<std::mutex> lk(m_GlobalMutex);
    m_GlobalQueue.push(std::move(item));
}
```

`thread_local t_iWorkerIdx`(비-worker는 -1)가 "너는 owner인가"를 판정하고, owner가 아니면 절대 deque를 만지지 못하게 **경로 자체를 막았다**. 모든 제출을 `EnqueueJob` 단일 진입점으로 통합해 우회로도 없앴다 (`PushToSomeDeque`는 wrapper로 격하).

**면접 포인트**: "lock-free 자료구조의 버그는 코드 한 줄이 아니라 **불변식 위반**입니다. Chase-Lev 논문의 전제(single owner)를 제 호출부가 깼고, 수정도 ordering 튜닝이 아니라 불변식을 코드 구조로 강제하는 방향이었습니다."

### 4.2 Profiler 자체가 race였다: 계측 도구의 역설

**증상** (2026-04-28): NavigationSystem 병렬 활성 직후 크래시. 게임 로직이 아니라 **CPUProfiler 내부**에서 죽었다.

**원인**: scope stack이 단일 `vector` 멤버였고, 여러 worker가 동시에 `PushScope`/`PopScope` → `push_back` 중 vector 재할당이 겹치며 크래시. "병렬화를 계측하려던 도구가 병렬에서 안전하지 않으면, 병렬화 디버깅 자체가 불가능하다"는 역설을 맞았다.

**해결**: 역할을 스레드 소유와 공유로 쪼갰다 — `Engine/Private/Core/Profiler/CPUProfiler.cpp`:
- **스택은 thread_local** — `thread_local std::vector<PendingProfilerScope> t_vProfilerStack`. Push/Pop의 뜨거운 경로는 락 0.
- **merge만 mutex** — PopScope가 완성된 이벤트를 통계에 합칠 때만 `m_Mutex`.
- **프레임 더블버퍼** — EndFrame에서 current↔last swap으로 수집과 오버레이 읽기를 분리.
- 덤으로 발견한 함정: scope/counter 이름을 포인터 비교하면 DLL/번역단위 경계에서 같은 문자열 리터럴이 다른 주소를 가져 중복 행이 생긴다 → `strcmp` 비교(`SameProfilerName`)로 교체. 주석으로 박제돼 있다.

이 사고가 **Worker-Safety 5정책**의 성문화 계기다: (1) thread_local = 작업 버퍼/scope stack, (2) atomic = 단순 카운터, (3) lock+buffer+main merge = 결과 수집, (4) ECS write는 self-entity만, (5) cross-entity write는 per-worker buffer + main flush.

### 4.3 게임 로직의 race를 없앤 Decision/Apply 2-pass

미니언 AI를 병렬화하면 "A 미니언의 worker가 B 미니언의 HP를 깎는" cross-entity write가 필연이다. 락으로 막는 대신 **read와 write의 시간을 분리**했다 — `Client/Private/GamePlay/System/LocalUnitAISystem.cpp`:

- **DecisionPass (worker, 병렬)**: World는 read-only. 결정(타겟, 데미지)은 `Get_WorkerSlot()`으로 자기 slot 버퍼에만 push. 후보 16개 미만이면 병렬 자체를 건너뛴다(`kParallelThreshold`).
- **ApplyPass (main, 단일)**: WaitForCounter 후 main이 slot 버퍼를 reduce하며 모든 write 수행.

같은 프레임 안에서 "모두가 읽는 시간"과 "혼자 쓰는 시간"이 절대 겹치지 않으므로 race가 0이다. 락 경합 없고, 순회 중 삭제 문제도 ApplyPass로 밀리며 자연 해소된다. 비용은 결정 버퍼 메모리와, write가 다음 pass로 밀리는 1단계 지연 — MOBA 30Hz 시뮬에서 관측 불가능한 수준.

---

## ⑤ 향후 개선 방향

1. **FiberShell → 진짜 fiber 협조 스케줄링**. 현재 `eJobExecutionMode::FiberShell`은 잡을 fiber 위에서 실행하고 즉시 복귀하는 셸까지만 구현돼 있다(`Engine/Private/Core/JobSystem.cpp`의 `TryExecuteItemOnFiber`/`FiberShellEntry`, `t_bInsideJobFiber` 재진입 방지). 다음 단계는 `WaitForCounter`를 fiber yield로 바꿔 대기 중인 잡이 스레드를 점유하지 않게 하는 것. public API(Submit/WaitForCounter)는 처음부터 내부 교체를 견디도록 고정해 뒀다 — 헤더 주석 "Phase 5-B 에서 내부가 Fiber 로 교체되어도 public API 불변".
2. **Fiber×IOCP 서버 통합**. 현 서버는 IOCP worker 4 + Tick 스레드 1 + JobSystem 미사용 구조다(`.md/TODO/05-07/Server/12_CURRENT_SERVER_CONCURRENCY_AUDIT.md`의 스레드 지도). 채택한 방향은 "IOCP worker는 fiber화하지 않는다" — completion은 큐로 넘기고 Tick fiber가 소비, Tick 스레드는 main-like(`t_iWorkerIdx == -1`)로 남긴다. IOCP는 OS blocking wait가 본체라 fiber화 이득이 없고, 시뮬레이션 쪽만 fiber로 병렬화하는 게 **결정론(같은 입력 → byte-exact snapshot)**을 지키기 쉽다. 결정성 검증이 이 통합의 최우선 게이트다.
3. **deque 동적 확장**: 고정 4096이 넘치면 GlobalQueue로 우회하는 현 구조는 동작은 하지만 overflow 시 Main 경로와 같은 락을 탄다. Chase-Lev circular array growth가 정석.
4. **steal 전략**: 현재 실패 시 victim 1명만 랜덤 시도 후 1ms 대기. 부하가 몰릴 때 victim 여러 명 순회 후 대기로 바꾸면 tail latency가 줄 여지가 있다 — 단, 이것도 counter 계측으로 증명한 뒤에.
5. **접근권 선언 커버리지 확대**: `UnknownWritesAll` 기본값에 남아 있는 시스템이 많을수록 배치가 쪼개진다. 시스템별 DescribeAccess를 채우는 것 자체가 저비용 최적화다.

---

## ⑥ 면접 Q&A

### Q1. "Work-stealing을 직접 구현했다고요? 왜 std::async나 스레드풀이 아니라?"

**답변 골격**: 프레임 기반 게임은 "매 프레임 짧은 잡 수백 개 + 프레임 끝 join"이라는 특수한 워크로드다. std::async는 잡당 스레드/future 비용이 크고, 단일 큐 스레드풀은 모든 worker가 한 락에 몰린다. Chase-Lev는 자기 잡 소비가 락 프리 + LIFO(캐시 친화)이고 훔칠 때만 CAS 경쟁이라 이 워크로드에 맞다. 학습 목적도 있었다 — memory ordering을 논문 구현으로 체득.
**꼬리질문 대비**: "std::async의 다른 함정은?" → future 폐기 시 소멸자가 join해 동기가 되는 함정을 CHttpClient에서 실제로 겪었다(비동기 챕터/에러 처리 챕터로 연결).

### Q2. "lock-free 자료구조에서 겪은 가장 어려운 버그는?"

**답변 골격**: §4.1 그대로. 핵심 문장 — "Chase-Lev의 single-owner 불변식을 제 호출부(Main push)가 깼습니다. 증상은 데드락도 크래시도 아닌 hang이었고, 재현 조건이 '루트 엔티티 16개 이상'이라는 콘텐츠 상태였습니다. 수정은 ordering 튜닝이 아니라 thread_local worker 식별 + 비-owner는 전역 큐로 우회라는 **경로 차단**이었습니다."
**꼬리질문 대비**: "왜 hang이 됐는지 메커니즘은?" → worker Pop이 빈 deque로 오인 → counter 미감소 → help-stealing WaitForCounter가 영원히 스핀. "지금 그 함수는?" → `PushToSomeDeque`는 `EnqueueJob` wrapper로 격하, 제출 진입점 단일화.

### Q3. "race를 발견했는데 왜 고치지 않고 껐습니까? 후퇴 아닌가요?"

**답변 골격**: 세 가지 근거. (1) hang은 확률적 재현이라 반쯤 고친 채 두면 다른 트랙 개발 전체가 인질이 된다. (2) 수정 옵션이 3개였고 어느 것이 옳은지 검증 없이 당일 결정하면 그게 진짜 도박이다. (3) 끄는 비용이 0이었다 — JobSystem nullptr fallback을 처음부터 설계해 뒀기 때문. "동시성 기능은 끌 수 있어야 추가할 자격이 있다"가 이후 원칙이 됐다.
**꼬리질문 대비**: "언제 다시 켰나?" → 원인 수정(EnqueueJob 분기) 후 TransformSystem/MinionAI부터 단계적 재활성, NavigationSystem은 별도 검증 후.

### Q4. "Pop과 Steal이 마지막 1개를 두고 경합하면 어떻게 됩니까?"

**답변 골격**: `WorkStealingDeque.h`의 Pop — bottom을 먼저 내리고 seq_cst fence 후 top을 읽는다. `t == b`(마지막 1개)면 owner도 top에 `compare_exchange_strong`으로 참전해서, Steal의 CAS와 정확히 한쪽만 이긴다. 진 쪽은 빈손. fence가 seq_cst인 이유는 owner의 bottom 감소와 thief의 top 증가가 서로에게 보이는 순서를 전역 합의해야 하기 때문 — 이게 acquire/release만으로 안 되는 지점이다.
**꼬리질문 대비**: "왜 Steal은 compare_exchange_weak?" → Steal은 실패 시 호출부가 그냥 다른 일을 하러 가는 루프라 spurious failure 허용이 싸다. Pop의 마지막 1개는 재시도 루프가 없는 1회 판정이라 strong.

### Q5. "fork-join 대기는 어떻게 구현했고, 조건변수 대비 장단은?"

**답변 골격**: WaitForCounter는 블로킹이 아니라 help-stealing — 대기자가 counter > 0인 동안 잡을 직접 실행한다. worker면 자기 deque+전역+steal 전부, Main이면 전역 drain 후 round-robin steal. 장점: 대기 코어가 놀지 않고, lost-wakeup 클래스가 카운터 경로에서 사라짐. 단점: 대기 중 잡은 긴 잡의 꼬리 latency, 스핀 성분. 유휴 worker 쪽은 반대로 순수 스핀을 버리고 `wait_for(1ms)`로 바꿨다 — 잡 대기와 완료 대기의 성격이 달라서 정책도 다르게 갔다.
**꼬리질문 대비**: "wait_for 1ms의 근거는?" → notify 누락(steal용 깨우기 없음)이 있어도 1ms 내 자동 회복하는 안전망을 겸한 실용 타협. 순수 cv 설계로 가면 깨우기 규약이 복잡해진다.

### Q6. "멀티스레드 게임 로직에서 락 없이 race를 없앴다는 게 무슨 뜻입니까?"

**답변 골격**: Decision/Apply 2-pass. worker는 읽기만 + 자기 slot 버퍼에 결정만 쌓고, main이 한 번에 쓴다. read 구간과 write 구간이 시간적으로 분리되므로 임계 영역 자체가 없다. slot 규칙(main=0/worker=idx+1, 버퍼 크기 WorkerCount+1)이 help-stealing 중인 main과 worker의 버퍼 충돌까지 막는다. 이 위에 Worker-Safety 5정책으로 케이스별 표준 답을 정해 두어, 새 시스템을 병렬화할 때 매번 고민하지 않는다.
**꼬리질문 대비**: "1프레임 지연은 문제 없나?" → 30Hz 시뮬에서 결정→적용이 같은 프레임 안(WaitForCounter 뒤 ApplyPass)이라 지연은 pass 간이지 프레임 간이 아니다.

### Q7. "프로파일러가 크래시났다는 이야기를 해보세요."

**답변 골격**: §4.2. 핵심 문장 — "병렬화를 검증할 도구가 병렬 안전하지 않으면 그 뒤의 모든 디버깅이 무너집니다. 그래서 계측 도구를 가장 먼저 worker-safe로 만들었습니다. 설계는 '뜨거운 경로는 thread_local, 공유는 merge 시점에만 mutex, UI 읽기는 더블버퍼'라는 3단 분리입니다."
**꼬리질문 대비**: "그 외 프로파일러 함정?" → DLL 경계에서 문자열 리터럴 주소가 달라 포인터 비교가 카운터를 중복 행으로 쪼갬 → strcmp 비교로 교체 (DLL 챕터와 연결).

### Q8. "어떤 작업을 병렬화 대상으로 고르나요?"

**답변 골격**: 3조건 — 임계량 이상(kParallelThreshold=16, "Submit/steal/Counter 오버헤드가 이득을 넘는 지점"이 주석으로 계약됨), 엔티티 간 독립(Transform 루트 서브트리 / AI 판단 단계), counter로 증명 가능. 반대로 스케줄러 레벨에서는 기본값이 UnknownWritesAll = 직렬이라, 병렬은 항상 옵트인이다. "일단 병렬화하고 버그 잡자"의 반대 방향.
**꼬리질문 대비**: "실제로 병렬화를 포기한 사례?" → 루트 16개 미만 Transform은 측정상 단일 스레드가 더 빨라 fallback 유지. 클라이언트 NavigationSystem은 서버 권위 이동 도입 후 replicated 챔피언에 대해 아예 실행하지 않는 쪽으로 정리(네트워크 챕터와 연결).

### Q9. "동시성 버그를 잡는 본인만의 방법론이 있습니까?"

**답변 골격**: 5단계로 답한다.
1. **불변식부터 쓴다** — "이 자료구조는 누가 어느 끝을 만질 수 있나"를 문장으로. Submit race는 코드가 아니라 이 문장("owner만 bottom")과의 대조에서 잡혔다.
2. **추측 2회 빗나가면 계측으로 전환** — 미니언 stuck 때 1시간 코드 추론보다 profiler counter 5분이 정답이었다. 동시성에서는 특히 로그 타이밍이 race를 숨기므로(하이젠버그), 락 없는 thread_local 계측을 쓴다.
3. **재현 조건을 데이터로 고정** — "루트 36개에서 hang"처럼 콘텐츠 상태를 수치로 박아야 회귀 테스트가 된다.
4. **끌 수 있게 만들어 이분법** — fallback 스위치로 병렬 on/off를 바꿔 증상이 따라오는지 확인. 따라오면 동시성 버그, 아니면 로직 버그.
5. **수정은 ordering 튜닝보다 경로 차단** — memory_order를 만지는 건 최후. 먼저 "그 스레드가 그 데이터에 도달하는 경로"를 구조로 없앤다 (EnqueueJob 단일 진입점, 2-pass).
**꼬리질문 대비**: "도구는?" → 자체 profiler counter/scope + Tracy 이중 계측. TSan은 MSVC 환경 제약으로 미사용 — 대신 위 4번의 스위치 이분법과 스트레스 재현으로 보완했다고 솔직하게.

### Q10. "Fiber는 왜 도입하려고 하나요? 서버 IOCP와는 어떻게 통합합니까?"

**답변 골격**: 스레드 context switch는 커널 진입 포함 ~1μs, fiber 전환은 유저 모드 레지스터 교환 ~수십 ns 수준이라, "잡이 잡을 기다리는" fork-join 중첩이 깊어질수록 fiber yield가 이긴다. 통합 원칙은 '전부 fiber화'가 아니라 경계 선정: IOCP worker는 OS blocking wait가 본체라 fiber화 대상이 아니고, completion을 큐로 넘겨 Tick fiber가 소비한다. Tick 스레드는 main-like로 남겨 JobSystem 규약(t_iWorkerIdx==-1 → 전역 큐)을 그대로 탄다. 최우선 검증 게이트는 결정론 — 같은 입력에서 byte-exact snapshot이 나와야 병렬화가 합격이다.
**꼬리질문 대비**: "fiber의 함정?" → fiber yield 중 mutex를 들고 있으면 같은 스레드의 다음 fiber가 같은 락을 기다리는 self-deadlock — lock 구간에서는 yield 금지 규약이 필요하다. 현 FiberShell 단계는 yield가 없어 이 함정이 아직 봉인돼 있다는 것까지 아는 게 포인트.

---

## ⑦ 다른 챕터와의 연결

- **`.md/interview/cpp/09_concurrency.md`** — 이 챕터가 "왜/어떻게 결정했나"라면, 그쪽은 atomic/memory_order/thread_local/condition_variable의 문법과 표준 의미론. Q4(seq_cst fence)의 언어 레벨 근거는 그쪽에서 보강.
- **`.md/interview/cpp/02_compile_link_dll.md`** — dllexport 클래스 + unique_ptr 멤버의 copy 명시 delete(CSystemSchedular 헤더 주석이 표준 사례), DLL 경계에서 문자열 리터럴 주소가 갈라져 profiler가 strcmp 비교로 간 이유.
- **`.md/interview/cpp/11_architecture_ecs.md`** — Phase 번호 = 실행 순서 계약, DescribeAccess 배칭의 ECS 쪽 시점. 미니언 stuck의 Phase swap(순서 = 데이터 의존성) 사고도 그쪽 축.
- **엔진 문서 세트 내 연결** — 프로파일러/최적화 챕터(bAnimated 17.8ms→9ms 복구가 "측정으로 증명" 원칙의 실전례), 서버·네트워크 챕터(Fiber×IOCP 통합의 결정론 검증, 30Hz Tick 스레드 구조), ECS 챕터(스케줄러 2층 모델: 순서=Phase, 병렬=접근권).
- **에러 처리 철학 챕터** — "끌 수 있는 스위치 + 동작하는 fallback"은 P2(실패 격리)·P4(디버깅 수월한 구조 우선)의 동시성 버전이다. `.md/architecture/WINTERS_DESIGN_PHILOSOPHY.md` 참조.
