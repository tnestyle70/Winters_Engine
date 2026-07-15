# Parallel Game Engine Foundations — OS 본질부터 ECS / JobSystem / Fiber 까지

> **현행 정본 (2026-07-13)**: Winters는 heap `WorkItem*` publication, Submit/Shutdown admission lease, 고정 4096-slot Chase-Lev deque, `ThreadOnly`/`FiberShell`/`FiberFull`, worker별 64-fiber pool과 origin-pinned wait/resume를 구현했고 전용 stress를 통과했다. 기본 모드는 `ThreadOnly`다. 서버 제품 배선과 세 mode startup probe는 완료됐지만 실제 GameRoom workload의 병렬화와 성능 수치는 별도 단계이므로 scheduler 구현 완료와 제품 speedup을 동일시하지 않는다. Fiber 6주 mastery 프로그램은 미착수다. 표의 고정 ns/μs 수치는 교육용 범위 예시일 뿐 대상 머신 계측 없이 Winters 성능 주장에 사용하지 않는다.
>
> **작성일**: 2026-05-03
> **목적**: 게임 엔진 병렬화의 진짜 원리를 OS 본질 + C++ 기반 + Winters 코드 베이스로 한 흐름. "왜 OOP 가 느린가" → "왜 ECS 가 빠른가" → "왜 JobSystem 만으론 부족한가" → "왜 Naughty Dog 가 fiber 를 쓰는가" 까지 7-layer 학습 트리.
> **선수 지식**: C++14 이상, basic OS (process/thread), pointer/메모리 기초.

### 현재 구현을 관통하는 네 가지 불변식

1. **publish-before-consume**: `WorkItem`은 heap에서 완성되고 counter가 먼저 증가한 뒤, atomic pointer token만 queue에 publish된다. 실패하면 counter를 rollback한다.
2. **one owner, many thieves**: worker만 자기 deque bottom을 push/pop하고, 외부 submitter와 overflow는 global MPMC queue로 간다. thief는 top CAS에 성공한 경우에만 token을 실행·회수한다.
3. **lifecycle is a memory-lifetime boundary**: admission lease가 활성 Submit과 standalone external Wait를 pin한다. Shutdown은 그 경계를 통과한 뒤 worker join, queue drain, deque/fiber state 파괴 순서로 간다.
4. **fiber is continuation, not parallelism**: 병렬 실행의 운반체는 OS worker thread다. FiberFull은 wait 중 stack을 보존해 동일 origin worker가 다른 job을 실행하게 하며, IOCP OS wait나 임의 socket callback을 fiber 안으로 끌어들이지 않는다.

현행 FiberFull은 worker마다 64개를 `CreateFiberEx`로 미리 만든다(64 KiB commit, 256 KiB reserve, `FIBER_FLAG_FLOAT_SWITCH`). counter별 waiter map에서 target 도달을 재확인하고 owner worker의 64-slot ready ring에만 enqueue한다. profiler scope stack과 nested Submit call-chain처럼 **fiber를 따라야 하는 상태**는 FLS를 사용한다.
> **연결**:
> - Fiber 집중 가이드: `FIBER_LEARNING_GUIDE.md`
> - 역사적 설계안: `.md/plan/engine/FIBER_JOB_SYSTEM_v2.md`
> - 현행 코드는 문서 끝 canonical code pointers를 따른다.

---

## §0. 큰 그림 — 7 Layer 학습 트리

```
Layer 7  Naughty Dog 모델 — Fiber + Wait List + Resume
Layer 6  Fiber — User-mode 양보, OS 가 모르는 스케줄링
Layer 5  ECS + Job System 통합 — Schedule / Access Contract / Race
Layer 4  Job System — Work Queue, Work Stealing, Counter
Layer 3  ECS — Data-Oriented Design, SoA, Cache Coherency
Layer 2  C++ Concurrency — atomic, memory ordering, alignas, lock-free
Layer 1  OS 본질 — User/Kernel mode, Context Switch, Cache, NUMA
```

**올라가는 순서**:
- L1-2 = 기초 도구 (도구함)
- L3-4 = 데이터 + 작업 모델 (재료)
- L5 = 통합 시스템 (조립)
- L6-7 = 한계 돌파 (튜닝)

거꾸로 내려가면서 "왜?" 자문하면 본질 이해.

---

## §1. OS 기초 — 모든 것의 base

### 1-1. User Mode vs Kernel Mode

CPU 는 두 모드로 명령 실행:

| 모드 | 권한 | 비용 | 예시 |
|---|---|---|---|
| **User Mode (Ring 3)** | 제한 — 다른 프로세스 메모리 접근 X | 빠름 | 일반 함수 호출, 산술 연산 |
| **Kernel Mode (Ring 0)** | 모든 권한 — HW 직접 제어 | syscall/scheduler/cache 효과를 포함해 workload별 상이 | 파일·소켓 I/O, page fault, contended wait |

**왜 mode switch 가 느린가**:
1. CPU 의 `iret`/`sysret` 명령 실행 (수십 cycle)
2. address-space 전환이면 TLB/PCID 영향이 생길 수 있음; 같은 process thread 전환을 매번 full flush로 단정할 수 없음
3. scheduler와 cache working-set 교체가 간접 비용을 만들 수 있음

**핵심**: 게임 엔진은 user mode 에서 최대한 머물러야 빠름. **fiber 가 user mode 만 쓰는 이유**.

### 1-2. Process vs Thread

| | Process | Thread |
|---|---|---|
| 주소 공간 | 독립 (가상 메모리 분리) | 공유 (같은 process 안) |
| 생성 비용 | 주소 공간·핸들·초기화 때문에 보통 더 큼 | stack/TCB·scheduler 등록 비용; 환경별 측정 |
| 컨텍스트 스위치 | 주소 공간 변경 영향 가능 | 같은 주소 공간을 공유하지만 cache/scheduler 비용은 존재 |
| 통신 | IPC (파이프/소켓/공유메모리) | 직접 메모리 read/write |
| 보호 | OS 가 강제 | 같은 process 안 — 사람이 책임 |

**Thread 의 정체**:
- OS 의 TCB (Thread Control Block) — 레지스터 / stack pointer / 스케줄링 정보
- Linux 에서 thread = LWP (Light Weight Process) — `clone(CLONE_VM | CLONE_FS | ...)`
- Windows 에서 thread = `CreateThread()` — TCB 자체

### 1-3. Context Switch — 진짜 비용

Worker thread 가 wait → 다른 thread 로 전환 시 OS 가 하는 일:

```
1. ABI/OS가 요구하는 thread context(스택·명령 위치·필요 레지스터)를 저장
2. scheduler가 runnable thread와 processor placement를 결정
3. 다음 thread context를 복원
4. 주소 공간이 바뀌면 page-translation cache 영향이 커질 수 있음
5. 새 실행 흐름으로 frontend/branch/cache locality가 흔들릴 수 있음
6. ★ 새 thread의 working set이 L1/L2에 없으면 cache miss 누적
```

직접 switch 비용보다 새 working set을 다시 데우는 간접 비용이 더 클 수 있다. 고정 μs나 손실 비율은 CPU·Windows build·affinity·부하에 따라 달라지므로 ETW/WPA 또는 profiler capture로 측정한다.

→ CPU-bound worker는 대개 logical processor 수를 넘겨 oversubscribe하지 않도록 시작하고, main/render/IO thread와 workload를 측정해 조정한다. Winters JobSystem의 `workerCount=0` 기본값은 `hardware_concurrency()-2`지만 서버는 IOCP/tick 예약을 고려한 명시값이 더 적절하다.

### 1-4. CPU Cache Hierarchy

```
CPU Core
  ├─ L1 Data Cache    32KB    ~4 cycle  (1ns)
  ├─ L1 Inst Cache    32KB    ~4 cycle
  ├─ L2 Cache        256KB   ~12 cycle  (3-4ns)
  └─ (shared) L3   16-64MB   ~40 cycle  (10-20ns)
       │
       ▼
     Main RAM       16-64GB ~200 cycle  (50-100ns)
```

위 숫자는 특정 세대의 전형적 규모를 설명하는 예시다. cache 크기·inclusive 정책·latency는 CPU마다 다르고 memory-level parallelism/prefetch 때문에 miss 하나를 고정 시간으로 환산할 수 없다. 본질은 계산량뿐 아니라 working-set과 접근 순서가 frame time을 지배할 수 있다는 점이다.

### 1-5. Cache Line + False Sharing (★ 매우 중요)

CPU 는 메모리를 **64 byte 단위 (cache line)** 로 가져옴.

```cpp
struct BadCounter {
    std::atomic<int> a;  // worker 0 가 자주 수정
    std::atomic<int> b;  // worker 1 이 자주 수정
};
// 두 변수 = 한 cache line (8 byte) → false sharing!
// worker 0 의 write 가 worker 1 의 cache 무효화 → coherence traffic 폭증
```

**해결**:
```cpp
struct GoodCounter {
    alignas(64) std::atomic<int> a;  // 자체 cache line
    alignas(64) std::atomic<int> b;  // 자체 cache line
};
```

**Winters 적용**:
```cpp
// WorkStealingDeque.h:91-92
alignas(64) std::atomic<std::int64_t> m_iBottom{ 0 };  // owner 만 자주 수정
alignas(64) std::atomic<std::int64_t> m_iTop{ 0 };     // 타인 가끔 수정
```

→ owner 가 bottom 자주 push/pop 하는 동안 타인의 top 접근이 cache 충돌 안 일으킴.

### 1-6. NUMA (Non-Uniform Memory Access)

큰 서버 (16+ 코어) 는 메모리가 socket 별 분리:
- Socket 0 의 thread → Socket 0 메모리 (~50ns)
- Socket 0 의 thread → Socket 1 메모리 (~100-150ns)

게임은 보통 1 socket 이지만, 서버는 NUMA 영향 큼. 백엔드 service 의 thread affinity 박제 필요.

---

## §2. C++ Concurrency Primitives — 도구함

### 2-1. std::atomic + Memory Ordering

`std::atomic<T>`는 data race 없는 원자 접근과 선택한 memory ordering을 표현한다. x64에서도 단순 load/store가 항상 `LOCK` prefix를 쓰는 것은 아니며, read-modify-write/CAS와 ordering에 따라 코드 생성이 달라진다. MSVC가 C++ memory model을 target ISA에 맞게 구현한다.

**6 memory orders** (가장 어려운 부분):

| Order | 보장 | 비용 | 사용 |
|---|---|---|---|
| `relaxed` | atomicity 만 (ordering 없음) | 가장 빠름 | 단순 카운터 |
| `consume` | (deprecated, acquire 와 동일 취급) | — | — |
| `acquire` | 이후 read/write 가 이 시점 이후로 reorder X | 중간 | wait/lock 진입 |
| `release` | 이전 read/write 가 이 시점 이전으로 reorder X | 중간 | publish/unlock |
| `acq_rel` | acquire + release | 중간 | RMW (read-modify-write) |
| `seq_cst` | 모든 seq_cst 연산이 하나의 total order를 이룸 | ISA/연산별 상이 | 가장 강한 단순 모델이 필요할 때 |

**비유**:
- `relaxed` = 우편 도착 순서 불정 (각자 받음)
- `acquire` = 편지 받았으면 그 전에 친구가 보낸 것 다 받음
- `release` = 편지 보내기 전에 내 모든 일 정리
- `seq_cst` = 모든 사람이 같은 순서로 편지 받음 (= 글로벌 동기화)

**Winters 예시** (JobCounter.h):
```cpp
void Decrement() {
    m_iCount.fetch_sub(1, std::memory_order_acq_rel);
    // acq_rel = job 완료 전 모든 write (job 결과) 가 visible
    // + 다음 작업이 이 사이즈 이후로 시작
}

uint32_t Load() const {
    return m_iCount.load(std::memory_order_acquire);
    // acquire = 이 값 본 후 read 가 이전 release 이후 데이터 봄
}
```

### 2-2. Lock-Free 패턴 — CAS

CAS = Compare-And-Swap. atomic 명령으로 "지금 X 면 Y 로 바꿔" 한 번에:

```cpp
std::atomic<int> m_iTop{ 0 };

bool Steal(T& out) {
    int t = m_iTop.load(std::memory_order_acquire);
    // 다른 worker 도 같은 t 봤을 수 있음
    out = m_arrBuf[t & MASK];
    // CAS: m_iTop 이 아직 t 면 t+1 로 바꿈, 아니면 false
    return m_iTop.compare_exchange_weak(
        t, t + 1,
        std::memory_order_seq_cst,
        std::memory_order_relaxed
    );
}
```

**ABA 문제**:
- Thread A: t=5 read
- Thread B: t=5→6→5 (다른 작업 후 reset)
- Thread A: CAS(5, 6) 성공 → 단 실제로는 다른 데이터
- 해결: 버전 카운터 (tagged pointer) 또는 hazard pointer

Winters Chase-Lev 은 ABA 가능 케이스 회피 설계. 단순 CAS.

### 2-3. std::mutex / lock_guard

**Mutex의 진실**: uncontended fast path는 user mode에서 끝날 수 있지만 contention이 지속되면 OS wait/wake와 scheduler가 개입한다. `futex`는 Linux 용어다. Windows의 MSVC `std::mutex` 내부 구현은 버전별로 달라질 수 있으므로 특정 primitive로 ABI 계약처럼 가정하지 않는다.

```cpp
std::mutex m;
{
    std::lock_guard<std::mutex> lk(m);  // RAII — 자동 lock/unlock
    // critical section
}
```

비용은 critical-section 길이, 경쟁자 수, preemption과 Windows 구현에 따라 달라진다. uncontended/contended를 별도 benchmark하고 lock hold time을 profiler에서 본다.

**원칙**: critical section 짧게. lock 안에서 system call 금지.

### 2-4. condition_variable — 왜 fiber 도입 시 제거했나

`cv` = thread 가 진짜로 sleep (kernel block) 하는 메커니즘:

```cpp
std::mutex m;
std::condition_variable cv;
bool ready = false;

// Thread A — 기다림
{
    std::unique_lock<std::mutex> lk(m);
    cv.wait(lk, [&]{ return ready; });  // ★ kernel block
}

// Thread B — 깨움
{
    std::lock_guard<std::mutex> lk(m);
    ready = true;
}
cv.notify_one();  // waiter를 깨우는 신호; 실제 kernel transition 여부는 구현/상태별 상이
```

**문제**: `cv.wait()` = OS thread 진짜 멈춤. 게임 엔진의 worker 가 멈추면 그 thread 의 코어 유휴.

**Phase 5-A 해결**: cv 제거 → busy-wait + help-stealing (`WaitForCounter`):
```cpp
while (pCounter->Load() > iTarget) {
    if (!TryExecuteOneJob(workerIdx))
        std::this_thread::yield();  // OS scheduler에 실행 기회를 양보하는 힌트. Fiber yield가 아님
}
```

**현행 FiberFull 해결**: current fiber를 waiter map에 등록하고 origin worker의 root scheduler fiber로 전환한다. 같은 worker thread는 다른 ready fiber/job을 픽업하고, counter가 target에 도달하면 그 fiber는 origin worker ready ring을 통해 재개된다.

### 2-5. RAII + Scope Guard

C++ 의 핵심 idiom — 객체 lifetime 으로 resource 관리:

```cpp
struct AutoCounter {
    int& count;
    AutoCounter(int& c) : count(c) { ++count; }
    ~AutoCounter() { --count; }  // ★ scope 종료 시 자동 호출
};

void DoWork() {
    AutoCounter ac(g_running);  // ++
    // ... 작업 ...
}  // ← 여기서 -- 자동
```

**Worker-Safety v3 의 AStar scope guard**:
```cpp
struct CounterFlush {
    const uint32_t& count;
    ~CounterFlush() {
        WINTERS_PROFILE_COUNT("AStar::NodesVisited", (i32_t)count);
    }
} guard{ nodesVisited };

// 어느 return 경로든 ~CounterFlush 자동 호출 → 누락 0
```

---

## §3. ECS — Data-Oriented Design

### 3-1. OOP 의 Cache 비효율

전통 OOP:
```cpp
class GameObject {
    Transform* m_pTransform;     // heap 어딘가
    RenderComp* m_pRender;        // heap 다른 곳
    PhysicsComp* m_pPhysics;      // heap 또 다른 곳
    AnimComp* m_pAnim;            // heap 또 다른 곳
    virtual void Update() = 0;    // 가상 함수 — vtable 조회
};

std::vector<GameObject*> objects;  // ★ pointer vector

for (auto* obj : objects) {
    obj->Update();  // ★ pointer 따라 jump → cache miss
    //  ★ vtable lookup → cache miss
    //  ★ 4개 component 각각 다른 heap → cache miss × 4
}
```

**문제**: 1개 GameObject Update 에 cache miss 5+ → 250+ cycle wasted.

**1000 GameObject** = 25만 cycle wasted = 0.1ms wasted (3.0GHz CPU).

### 3-2. SoA vs AoS

**AoS (Array of Structures, OOP)**:
```cpp
struct Particle { float x, y, z; float vx, vy, vz; float life; };
std::vector<Particle> particles;  // x,y,z,vx,vy,vz,life | x,y,z,...
```

**SoA (Structure of Arrays, ECS)**:
```cpp
struct ParticleSystem {
    std::vector<float> xs, ys, zs;     // x,x,x,x,...
    std::vector<float> vxs, vys, vzs;
    std::vector<float> lives;
};
```

**왜 SoA 가 빠른가**:
- "x 만 업데이트하는 시스템" → SoA 면 64 byte cache line 에 16 float (= 16 particle) 한 번에 load
- AoS 면 1 cache line 에 ~2.3 particle (28 byte 단위) → load 횟수 7배

**SIMD (Single Instruction Multiple Data)**:
- SoA + SSE/AVX = 한 명령으로 4-8 float 동시 처리
- Vectorization 자동 가능

### 3-3. Entity = ID + Generation

```cpp
struct EntityID {
    uint32_t index;      // ComponentStore 의 slot
    uint32_t generation; // 재사용 시 +1 (dangling 방지)
};

bool IsAlive(EntityID e) {
    return store[e.index].generation == e.generation;
}
```

**Generation 의 역할**:
- Entity destroy → slot 재사용
- 옛 ID 들고 있던 곳에서 access 시 generation mismatch → false 반환
- C++ raw pointer 의 dangling 위험 회피

### 3-4. Component = POD struct

```cpp
struct TransformComponent {
    DirectX::XMFLOAT3 vPosition;
    DirectX::XMFLOAT3 vRotation;
    DirectX::XMFLOAT3 vScale;
    bool bDirty;
    EntityID parentId;
};

// POD = Plain Old Data
// - 가상 함수 X
// - 생성자/소멸자 trivial
// - memcpy 가능
// - SIMD-friendly
```

### 3-5. System = Pure Function

```cpp
class CTransformSystem : public ISystem {
public:
    void Execute(CWorld& world, float dt) override {
        // Transform 만 read/write (다른 component 안 건드림)
        world.ForEach<TransformComponent>([dt](EntityID id, TransformComponent& t) {
            // 부모 dirty 면 자기 worldMatrix 재계산
            if (t.bDirty) {
                t.UpdateWorldMatrix();
                t.bDirty = false;
            }
        });
    }
};
```

**System 의 특징**:
- 입력: World (component store) + dt
- 출력: World 의 component 변경 (in-place)
- 사이드 이펙트 = 명시적 (Component 만)

### 3-6. ECS 의 진짜 가치

| 측면 | pointer-heavy object graph | data-oriented ECS |
|---|---|---|
| Cache locality | 객체/컴포넌트 배치에 따라 pointer chasing 증가 | 같은 component의 dense iteration을 설계하기 쉬움 |
| SIMD | 가상 호출·산재 데이터가 자동 vectorization을 방해할 수 있음 | SoA/contiguous layout이면 vectorization 후보가 명확 |
| 병렬화 | side effect와 access set이 객체 메서드 안에 숨기 쉬움 | system read/write contract를 명시하기 쉬움 |
| 메모리 | allocation metadata/fragmentation/포인터 비용 가능 | dense store는 overhead를 줄일 수 있으나 sparse/indirection 비용도 존재 |

ECS가 자동으로 빠른 것은 아니다. archetype 이동, sparse lookup, branch, 실제 access pattern을 함께 측정해야 한다. 가치는 "데이터 배치와 access contract를 명시적으로 설계할 수 있다"는 데 있다. UE Mass, Unity DOTS, Bevy는 이 방향의 서로 다른 구현이다.

---

## §4. Job System — Task Parallelism

### 4-1. Work Queue 패턴

```
┌─────────────────────────────────────┐
│ Main Thread                         │
│  Submit(job1) ─┐                    │
│  Submit(job2) ─┤ → Queue            │
│  Submit(job3) ─┘                    │
└─────────────────────────────────────┘
            │
            ▼
┌─────────────────────────────────────┐
│ Queue: [job1] [job2] [job3]         │
└─────────────────────────────────────┘
   │     │     │
   ▼     ▼     ▼
Worker 0  Worker 1  Worker 2

각 Worker:
  while (true) {
      job = Queue.Pop();
      job.Run();
  }
```

**문제 1 — 단일 큐 contention**: 모든 worker 가 같은 queue 에 lock 시도 → contention 폭증.

**해결 — Per-worker queue + Work Stealing**.

### 4-2. Work Stealing (Chase-Lev 2005)

각 worker 자체 deque (double-ended queue):

```
Worker 0 deque:                 Worker 1 deque:
┌───┬───┬───┬───┬───┐          ┌───┬───┬───┐
│j1 │j2 │j3 │j4 │j5 │          │j6 │j7 │j8 │
└───┴───┴───┴───┴───┘          └───┴───┴───┘
 ▲                 ▲            ▲           ▲
 top              bottom        top         bottom
 (steal)         (push/pop)
```

**3 연산**:

| 연산 | 위치 | 호출자 | 동기화 |
|---|---|---|---|
| Push | bottom | owner 만 | atomic store (lock-free) |
| Pop | bottom | owner 만 | 일반 case lock-free, 마지막 1개만 CAS |
| Steal | top | 타인 | CAS |

**왜 owner 가 bottom?**:
- LIFO = 가장 최근 push 한 job 먼저 pop = **cache hit 좋음**
- 같은 작업 흐름 → 같은 데이터 → 같은 cache line

**왜 타인이 top?**:
- 가장 오래된 job = owner 가 안 쓸 가능성 높음
- top/bottom 분리 = owner ↔ thief 의 cache line 분리 (false sharing 방지)

**Chase-Lev 알고리즘** (Winters Engine [WorkStealingDeque.h](Engine/Public/Core/JobSystem/WorkStealingDeque.h)):
```cpp
template <typename T>
class CWorkStealingDeque {
    static_assert(std::is_trivially_copyable_v<T>);
    alignas(64) std::atomic<int64_t> m_iBottom{ 0 };
    alignas(64) std::atomic<int64_t> m_iTop{ 0 };
    std::array<std::atomic<T>, 4096> m_arrBuf{};

public:
    bool Push(T v) {  // owner only
        int64_t b = m_iBottom.load(std::memory_order_relaxed);
        int64_t t = m_iTop.load(std::memory_order_acquire);
        if (b - t >= kCapacity) return false;
        m_arrBuf[b & MASK].store(v, std::memory_order_relaxed);
        m_iBottom.store(b + 1, std::memory_order_release);
        return true;
    }

    bool Pop(T& out) {  // owner only
        int64_t b = m_iBottom.load(std::memory_order_relaxed) - 1;
        m_iBottom.store(b, std::memory_order_release);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        int64_t t = m_iTop.load(std::memory_order_relaxed);
        if (t > b) {  // empty
            m_iBottom.store(t, std::memory_order_release);
            return false;
        }
        const T value = m_arrBuf[b & MASK].load(std::memory_order_acquire);
        if (t != b) { out = value; return true; }
        // 마지막 1개 — Steal 와 경쟁
        const int64_t lastIndex = t;
        bool ok = m_iTop.compare_exchange_strong(
            t, t + 1,
            std::memory_order_seq_cst,
            std::memory_order_relaxed);
        // CAS 실패 시 expected `t`는 thief가 쓴 새 top으로 바뀐다.
        // 반드시 CAS 전 index로 bottom generation을 복구한다.
        m_iBottom.store(lastIndex + 1, std::memory_order_release);
        if (ok) out = value;
        return ok;
    }

    bool Steal(T& out) {  // any thread
        int64_t t = m_iTop.load(std::memory_order_acquire);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        int64_t b = m_iBottom.load(std::memory_order_acquire);
        if (t >= b) return false;
        const T value = m_arrBuf[t & MASK].load(std::memory_order_acquire);
        if (!m_iTop.compare_exchange_weak(
            t, t + 1,
            std::memory_order_seq_cst,
            std::memory_order_relaxed))
            return false;
        out = value;
        return true;
    }
};
```

Winters의 `T`는 `WorkItem*`다. 함수 객체 자체를 ring slot에 두지 않는 이유는 publication과 object lifetime을 분리하기 위해서다. 4096칸을 넘으면 owner push가 `false`를 반환하고 JobSystem이 global queue로 넘긴다. 마지막 한 칸은 owner Pop과 thief Steal 중 **CAS 승자 하나만** 가져가며, stress는 이 경합을 10만 회 반복해 중복/유실과 ring wrap generation을 검사한다.

### 4-3. Counter (atomic only — cv 제거됨)

```cpp
// JobCounter.h
class CJobCounter {
    std::atomic<uint32_t> m_iCount{ 0 };
public:
    void Increment(uint32_t n = 1) { m_iCount.fetch_add(n, std::memory_order_relaxed); }
    bool TryDecrement(uint32_t& remaining); // CAS로 underflow 차단
    uint32_t Load() const { return m_iCount.load(std::memory_order_acquire); }
};
```

Submit은 counter를 **queue publish 전에** 증가시킨다. allocation/publish가 예외로 실패하면 같은 counter를 rollback하고, 실행 경로는 예외가 나도 정확히 한 번 `TryDecrement`한다. 그래서 waiter가 `0`을 너무 일찍 보고 return하거나 실패 job 때문에 영원히 남는 두 race를 함께 막는다.

**왜 counter 자체에 cv를 두지 않는가**: worker가 counter cv에서 잠들면 그 OS worker는 다른 job을 수행할 수 없다. ThreadOnly/FiberShell은 help-execute하고, FiberFull job fiber는 waiter map에 자신을 등록한 뒤 root로 양보한다. JobSystem의 idle worker를 깨우는 1ms wake CV는 별개 목적이다.

→ **WaitForCounter 가 busy-wait + help-stealing 으로 대체** (5-A) → fiber yield 로 대체 예정 (5-B).

### 4-4. WaitForCounter 의 한계 (★ Fiber 도입 동기)

```cpp
void CJobSystem::WaitForCounter(CJobCounter* c) {
    while (c->Load() > 0) {
        if (t_iWorkerIdx >= 0) {
            TryExecuteOneJob(t_iWorkerIdx);  // help-stealing
        } else {
            std::this_thread::yield();
        }
    }
}
```

**시나리오 — 의존성 그래프**:
```
   A ──┐
   B ──┼──> C
   D ──┘
```

C 는 A, B, D 끝나야 시작. C 가 worker 0 에서 실행 중일 때:
- `WaitForCounter(&counter)` (counter = A+B+D 의 합산)
- counter > 0 인 동안 worker 0 가 다른 일 처리
- 단 다른 일이 없으면 worker 0 = 단순 spin/yield = **CPU 낭비**

**더 큰 문제 — depth 깊은 그래프**:
```
A → B → C → D → E → F
```

각 단계가 wait. 만약 thread 4개고 depth 6 인데 각 단계 1개 job 만 있으면 → 3 worker 는 항상 idle.

**Fiber가 해결하는 것**: C의 call stack만 waiter로 빼고 worker 0가 ready D를 실행할 기회를 만든다. ready work가 없거나 memory/lock 병목이면 utilization이 자동으로 100%가 되는 것은 아니다.

### 4-5. 의존성 그래프 (DAG)

게임 프레임의 작업은 자연스럽게 DAG:

```
                     Animation
                         │
   Physics ──────────────┤
                         ▼
                     Skinning
                         │
   AI Decision ──────────┤
                         ▼
                     CullPass
                         │
                         ▼
                     RenderSubmit
                         │
                         ▼
                     GPU Kick
```

이 그래프를 17ms 안에 끝내려면 — 모든 worker 가 절대 안 멈추는 게 핵심.

---

## §5. ECS + Job System 통합

### 5-1. System Schedule (Phase 별)

```cpp
class CSystemScheduler {
    std::map<uint32_t, std::vector<unique_ptr<ISystem>>> m_mapPhases;
    // phase 0: Transform
    // phase 1: AI / MinionAI
    // phase 2: Navigation
    // phase 3: Status / Buff

public:
    void Execute(CWorld& world, float dt) {
        for (auto& [phase, systems] : m_mapPhases) {
            // 같은 phase 안 system 들 — 병렬 가능 (단 access contract 검증)
            CJobCounter counter;
            for (auto& sys : systems) {
                m_pJobSystem->Submit([&]{ sys->Execute(world, dt); }, &counter);
            }
            m_pJobSystem->WaitForCounter(&counter);  // ★ phase 직렬
        }
    }
};
```

**Phase 직렬 + Phase 안 병렬**:
- Phase 0 (Transform) 끝나야 Phase 1 (AI) 시작 — 일반적 의존성
- Phase 안 system 들 동시 실행 — 단 같은 component write 시 race

### 5-2. Component Access Contract

WORKER_SAFETY_PACKAGE v3 의 핵심:

```cpp
struct SystemAccess {
    bool bExclusive = true;  // ★ 기본 = 단독 실행 (보수적)
    std::vector<std::type_index> reads;   // read-only
    std::vector<std::type_index> writes;  // write
};

class ISystem {
public:
    virtual void Get_AccessContract(SystemAccess& out) const {
        out.bExclusive = true;  // ★ 미박제 시 자동 직렬화 (안전)
    }
};

void CTransformSystem::Get_AccessContract(SystemAccess& out) const override {
    out.bExclusive = false;
    out.writes.push_back(std::type_index(typeid(TransformComponent)));
    // 다른 시스템과 병렬 가능 — TransformComponent 만 write
}
```

Scheduler 가 contract 분석:
- 같은 phase 의 시스템들 그룹화 (read/write 충돌 없는 그룹)
- 그룹 단위 병렬 Submit
- 충돌 시 그룹 분리 (직렬화)

### 5-3. Component-Level Race

**위험 시나리오**: MinionAI 가 다른 entity 의 HealthComponent.fCurrent write
```cpp
// 미니언 5명이 같은 적 미니언 T 공격
auto& tgtHp = world.GetComponent<HealthComponent>(ms.attackTargetId);
tgtHp.fCurrent -= ms.attackDamage;  // ★ race! 5 worker 동시 read-modify-write
```

worker 0~4 모두 read 100 → write 90 → 결과 90 (50 빠져야 했는데 10만)

### 5-4. Decision/Apply 2-Pass (Worker-Safety v3 §2)

해결: **Read 와 Write 시간상 분리**.

```
Pass 1 (Worker, 병렬):
  - 모든 미니언이 read-only 로 결정 박제
  - MinionDecision 구조체 → per-worker buffer 에 push
  - World 의 어떤 component 도 write X

Pass 2 (Main, 단일):
  - 모든 buffer 의 Decision 일괄 적용
  - HealthComponent / MinionStateComponent / NavAgent 모두 main 단일 thread 만 write
```

**race 0** — 한 프레임 안에서 read phase 와 write phase 가 시간상 분리.

```cpp
void CMinionAISystem::DecisionPass(CWorld& world, EntityID id, float dt) {
    // ★ const 참조 — write 0
    const auto& ms = world.GetComponent<MinionStateComponent>(id);
    // 결정 박제
    MinionDecision dec{};
    // ...
    Push_Decision(dec);  // ★ per-worker buffer
}

void CMinionAISystem::ApplyPass(CWorld& world, float dt) {
    // ★ Main thread — 모든 buffer 순차 적용
    for (auto& buf : m_vecDecisionsPerSlot) {
        for (const auto& dec : buf) {
            auto& ms = world.GetComponent<MinionStateComponent>(dec.self);
            ms.attackTargetId = dec.attackTarget;
            // ...
        }
    }
}
```

### 5-5. Worker-Safety v3 의 5 정책

| # | 정책 | 사용 |
|---|---|---|
| 1 | **thread_local** | 작업 버퍼 (CPUProfiler stack, AStar tls_gScore) |
| 2 | **atomic** | 단순 카운터 / flag |
| 3 | **lock + buffer + main merge** | 결과 수집 (CPUProfiler events) |
| 4 | **self-entity only** | ECS Component write (자기 entity 만) |
| 5 | **per-worker buffer + main flush** | cross-entity write (MinionAI Decision/Apply) |

→ FiberFull에서는 atomic/lock/main-merge 원칙은 유지되지만 `thread_local`은 재분류가 필요하다. yield가 없는 leaf scratch는 TLS로 둘 수 있고, wait를 가로지르는 continuation 상태는 stack/FLS/job-owned state로 옮긴다. worker별 buffer는 origin pinning으로 slot이 유지되더라도 sibling fiber interleave에 안전한 사용 규약이 있어야 한다.

---

## §6. Fiber — Job System 의 한계 돌파

### 6-1. WaitForCounter 가 Thread 를 점유하는 진짜 문제

5-A 의 WaitForCounter 는 thread 안 멈추지만 (busy-wait), thread 가 wait 코드 안에서 빠져나오질 못함:

```
Worker 0 thread:
  ExecuteItem(Job_C)  ← C 시작
    C 코드 안에서 WaitForCounter(&dependencyCounter)
      while (counter > 0) {
        TryExecuteOneJob()  ← 다른 일감 처리 (help-stealing)
        ...
      }
    counter == 0 → WaitForCounter return
  ExecuteItem return
  ← Worker 0 다시 자유

문제: Worker 0 가 C 의 stack frame 안에 있어
"다른 일감 = Job_X" 처리 중일 때 Job_X 도 wait 하면
스택이 X 안에 또 들어감. Recursive 호출 깊이 ↑.

→ 의존성 깊은 그래프에서 stack overflow 위험.
→ 또는 worker 자체가 다 같은 깊이로 stuck.
```

**Fiber 해결**: C 의 stack 자체를 보존 + worker thread 는 다른 fiber 의 stack 으로 SwitchTo.

### 6-2. Fiber 의 User-Mode 양보

```cpp
// Worker 0: fiber A 실행 중
void FiberA_Code() {
    // 작업 1
    WaitForCounter(&counter);  // ★ counter > 0
    // ...
}

// WaitForCounter 가 fiber 모드 감지:
void WaitForCounter(CJobCounter* c) {
    if (m_eMode == FiberFull && t_iWorkerIdx >= 0) {
        // Fiber A 의 stack 보존 + thread fiber 로 SwitchTo
        Fiber_YieldToCounter(c);
        // ★ 여기서 멈춤. 누군가 SwitchToFiber(A) 진입 시 다음 줄
        return;
    }
    // 기존 busy-wait
    while (c->Load() > 0) ...
}
```

**핵심**: thread 의 stack pointer 만 thread fiber 로 swap. Fiber A 의 모든 stack frame 그대로 보존.

### 6-3. Wait List + Resume 메커니즘

> **현행과의 차이**: 아래 global `m_ReadyFibers`/pool 128 코드는 2026-05 역사적 설계다. 실제 구현은 `FiberSchedulerState::WorkerState`마다 64-fiber pool과 64-slot ready ring을 소유한다. `FiberRecord::iOwnerWorker`는 생성 시 고정되고 notifier는 그 worker의 ring에만 넣는다. waiter 등록은 counter를 lock 안에서 다시 읽은 뒤 `Running → Waiting`으로 전환해 completion과의 lost-wakeup을 막는다.

역사적 3 자료구조 스케치:

```cpp
class CJobSystem {
private:
    // 1) per-worker context — 현재 어느 fiber 실행 중
    struct WorkerContext {
        FiberHandle hThreadFiber;     // ConvertThreadToFiber 결과
        uint32_t iCurrentFiber;       // 현재 fiber idx (UINT32_MAX = 없음)
    };
    std::vector<WorkerContext> m_vecWorkerCtx;

    // 2) Counter 별 wait list — JobSystem 안 (Counter 안 X, Codex #4)
    struct CounterWaitState {
        std::vector<uint32_t> vecWaitFibers;
        uint32_t iTarget;
    };
    std::mutex m_WaiterMutex;
    std::unordered_map<CJobCounter*, CounterWaitState> m_mapWaiters;

    // 3) Ready queue — yield 후 깨어난 fiber 들
    std::mutex m_ReadyMutex;
    std::queue<uint32_t> m_ReadyFibers;

    // 4) Fiber Pool — 128 fiber (역사적 global-pool 초안)
    std::unique_ptr<CFiberPool> m_pFiberPool;
};
```

**흐름**:

```
Worker 0: fiber A 실행 → WaitForCounter(&c) 진입
   ↓
Fiber_YieldToCounter:
   1. lock(m_WaiterMutex)
   2. if (c.Load() <= 0) return  ← race 재확인 (counter 가 막 0 됐을 수)
   3. m_mapWaiters[&c].vecWaitFibers.push(A)
   4. unlock
   5. A.SetState(Waiting)
   6. SwitchToFiber(thread_fiber 0)  ← thread 는 worker loop 로 복귀
   ↓
Worker 0: TryExecuteOneJob 다시 → 다른 fiber B 픽업 → 실행
   ↓
(어느 worker 가 c 에 영향 주는 마지막 job 완료)
   ↓
ExecuteItem 의 wrap 람다:
   1. job() 실행
   2. c.Decrement() → c == 0
   3. Fiber_NotifyCounterComplete(&c):
      a. lock(m_WaiterMutex)
      b. notify = move(m_mapWaiters[&c].vecWaitFibers)
      c. m_mapWaiters.erase(&c)  ← ★ counter destroy 안전
      d. unlock
      e. lock(m_ReadyMutex)
      f. for idx in notify: m_ReadyFibers.push(idx); GetFiber(idx).SetState(Ready)
      g. unlock
   ↓
Worker 0: 자기 ready ring 우선 검사 → A 픽업
   ↓
Fiber_TryResumeOne:
   1. A의 hRootFiber는 생성 당시 worker 0 root로 고정
   2. worker 0가 Ready → Running CAS
   3. SwitchToFiber(A)  ← A 의 stack frame 복원
   ↓
Fiber A: WaitForCounter 의 SwitchToFiber 다음 줄에서 재개
   - counter == 0 보장
   - 다음 코드 진행
```

### 6-4. TLS와 FLS 함정 (★ 가장 어려운 디버깅)

현행 Fiber A는 worker 0에서 시작해 worker 0에서 재개되므로 `Get_WorkerSlot()`은 유지된다. 아래 cross-worker 예시는 채택되지 않은 역사적 설계가 왜 위험했는지 보여준다.

```cpp
void Some_System_Function() {
    uint32_t slot = CJobSystem::Get_WorkerSlot();  // worker 0 → slot 1
    m_pJobSystem->WaitForCounter(&counter);        // ★ yield 가능
    // ★ 여기서 깨어나면 worker 3 → slot 4
    m_buf[slot].push_back(...);  // worker 0 의 slot 1 에 push
                                  // 하지만 실제 thread 는 worker 3 → race
}
```

**해결**: yield 가능 함수 안 slot 캐시 금지. push 직전 매번 호출.

```cpp
void Some_System_Function() {
    m_pJobSystem->WaitForCounter(&counter);
    uint32_t slot = CJobSystem::Get_WorkerSlot();  // ★ 여기서 호출
    m_buf[slot].push_back(...);  // ★ push 직전, yield 0
}
```

현행의 실제 함정은 **같은 worker TLS를 sibling fibers가 공유**한다는 점이다. A가 TLS linked-stack에 node를 push한 뒤 wait하고 B가 같은 worker에서 그 TLS를 덮으면, A 재개 시 B의 이미 파괴된 stack node를 따라갈 수 있다. Winters는 nested Submit admission chain을 FLS로, CPU profiler scope stack도 FLS로 옮겼다. worker index처럼 thread에 귀속된 값은 TLS에 남긴다.

### 6-5. Counter Destroy 안전성 (Codex #4)

**v2 v1 안 (폐기)**:
```cpp
class CJobCounter {
    std::atomic<uint32_t> m_iCount;
    std::vector<uint32_t> m_arrWaitFibers;  // ★ Counter 안 wait list
};

// Caller scope:
{
    CJobCounter counter;  // stack 변수
    Submit(jobs..., &counter);
    // ★ WaitForCounter 누락
}  // ← counter destroy. Wait list 의 fiber 들이 dangling pointer
```

**v2.1 정정**:
```cpp
class CJobCounter {
    std::atomic<uint32_t> m_iCount;  // 변경 0
};

class CJobSystem {
    std::unordered_map<CJobCounter*, CounterWaitState> m_mapWaiters;
    // ★ key = Counter 의 주소, value = wait list
};

// 정상 흐름:
{
    CJobCounter counter;
    Submit(jobs..., &counter);
    WaitForCounter(&counter);  // ★ 정상 return = m_mapWaiters[&counter] erase 됨
}  // ← counter destroy. m_mapWaiters 에 entry 없음 → 안전
```

정확한 public contract는 더 강하다. counter는 모든 submitted job과 waiter보다 오래 살아야 하고 caller는 `WaitForCounter` 완료 전에 scope를 끝내면 안 된다. wait map이 counter 안에 있지 않다는 사실은 정상 완료의 정리를 단순하게 할 뿐, dangling counter 사용을 자동으로 합법화하지 않는다.

### 6-6. Naughty Dog 모델과 Winters의 구체화

```
┌────────────────────────────────────────────────┐
│  CJobSystem (FiberFull mode)                   │
│                                                │
│  Worker 0  Worker 1  ...  Worker N-1           │ ← OS thread (블로킹 X)
│   thread    thread          thread             │
│     │         │                │               │
│     ▼         ▼                ▼               │
│  SwitchToFiber  SwitchToFiber  SwitchToFiber   │
│     │         │                │               │
│  ┌──┴─────────┴────────────────┴─┐            │
│  │   Fiber Pool 64 / worker     │            │
│  │ F0(Run) F1(Wait) F2(Free) ...│            │
│  └──────────────────────────────┘            │
│                                                │
│  ┌── m_mapWaiters (CJobSystem) ──┐            │
│  │ counter A → [F1, F5]          │            │
│  │ counter B → [F12]             │            │
│  └───────────────────────────────┘            │
│                                                │
│  ┌── origin worker ready ring ────┐            │
│  │ fixed 64 slots / worker        │            │
│  └───────────────────────────────┘            │
└────────────────────────────────────────────────┘
```

**3 핵심**:
1. Worker thread = 실제 병렬 실행 주체; idle 시 wake CV로 대기하고 FiberFull wait에서는 다른 work를 픽업
2. worker-local pool = 실행 continuation(stack + 코드 위치), 64 KiB commit/256 KiB reserve
3. global counter waiter map + origin-worker ready ring = 진행 상태와 native fiber thread-affinity를 함께 보존

**주의**: 외부 발표의 성능 수치는 해당 게임·하드웨어·워크로드 결과다. Winters의 합격 수치로 복사하지 않고 자체 before/after와 core utilization을 측정한다.

---

## §7. Winters 코드 적용

### 7-1. 기본 경로 — ThreadOnly

```
[Main thread]
   Submit(job1, &counter)
   Submit(job2, &counter)
   WaitForCounter(&counter)
       ↓
   while counter > 0:
       global drain → steal (다른 worker 의 deque)
       std::this_thread::yield()
       ↓
   counter == 0 → return

[Worker thread]
   while !shutdown:
       1. 자기 deque pop (LIFO)
       2. global queue pop
       3. steal (다른 worker)
       → ExecuteItem (job 실행 + counter Decrement)
       → 일이 없으면 condition_variable wait_for(1ms)
   shutdown
```

### 7-2. opt-in 경로 — FiberFull (구현·stress 완료)

```
[Main thread] — 변경 0
   기존 그대로

[Worker thread]
   Initialize: ConvertThreadToFiberEx(FLOAT_SWITCH)
               + worker-local fiber 64개 완전 생성, 아니면 startup 실패
   while !shutdown:
       1. ★ Ready fiber 우선 검사 (yield 됐던 것 깨움)
          → origin worker ready ring에서 pop/CAS
          → SwitchToFiber(ready_fiber)
            → fiber 의 stack frame 복원, 작업 진행
            → 작업 끝 또는 다시 yield
          → 복귀 시 Pool Release
       2. 자기 deque pop
       3. global pop
       4. steal
       → ExecuteItem:
            worker.Pool.Acquire() → native fiber
            fiber.AssignJob(wrap_lambda)
            SwitchToFiber(fiber)  ← fiber 진입
              → 작업 (또는 yield)
            복귀 → Pool Release (Free) 또는 waiter map 대기 (Waiting)
```

`FiberShell`은 중간 검증 모드다. worker thread를 fiber로 바꾸고 job마다 64 KiB/256 KiB stack의 native fiber를 생성·삭제하지만 counter wait는 ThreadOnly처럼 help-execute한다. 따라서 FiberFull의 성능 모드가 아니라 Win32 전환/예외/완료 경로를 분리 검증하는 비교군이다.

### 7-3. 통합 사이클

```
1. Main: BeginFrame
   → InputSystem 처리
   → ECS Tick (SystemScheduler.Execute)
       → Phase 0 (Transform): Submit jobs → WaitForCounter
       → Phase 1 (AI):
           ├ MinionAI DecisionPass: Submit per-minion jobs (병렬)
           │   각 job: read-only on World → MinionDecision push
           │   (★ FiberFull 이면 worker 가 wait 안 멈춤)
           ├ WaitForCounter
           └ MinionAI ApplyPass: Main thread 단일 (모든 write)
       → Phase 2 (Navigation): Submit jobs → WaitForCounter
       → Phase 3 (Status / Buff)
   → RenderSystem
       → CullPass (Submit + Wait)
       → SubmitPass (RHI bind)
   → GPU Kick

2. Worker thread (모든 단계):
   - FiberFull job fiber가 counter wait 시 origin root로 yield → 다른 fiber/job 픽업
   - 외부/main Wait는 submission lease 아래 global drain/steal로 help-execute
   - 일감이 전혀 없으면 wake CV에서 짧게 대기하며 busy-spin을 피함

3. EndFrame
```

---

## §8. 실전 디버깅 + 측정

### 8-1. TLS 공유 + Fiber Interleave 함정

```cpp
thread_local std::vector<float> tls_buffer;

void Function_With_Yield() {
    tls_buffer.clear();
    DoWork(tls_buffer);
    WaitForCounter(&someCounter);  // ★ yield 가능
    // 같은 origin worker라도 그 사이 sibling fiber가 같은 TLS를 사용 가능
    UseBuffer(tls_buffer);  // ← sibling이 clear/overwrite했을 수 있음
}
```

**해결**: worker identity/scheduler root처럼 thread에 귀속된 값만 TLS에 둔다. yield를 건너 continuation별로 유지해야 하는 값은 stack, job-owned state, FLS 중 하나를 사용한다. Winters의 nested Submit admission chain과 profiler scope stack은 FLS다.

### 8-2. Profiler 통합

```cpp
WINTERS_PROFILE_SCOPE("MinionAI::DecisionPass");
// ...
WINTERS_PROFILE_COUNT("Nav::PathNodes", nodeCount);
```

**현행 profiler 적용**:
- profiler scope stack = `FlsAlloc` 기반 fiber-local stack (`CPUProfiler.cpp`)
- event merge = mutex + main flush
- Tracy에는 안정된 fiber name과 `TracyFiberEnter/Leave`를 연결
- FLS 할당 실패 fallback은 진단 대상이며 FiberFull submission context는 fail-closed startup 조건

### 8-3. 현행 executable stress gate

| 범주 | 실제 하네스 | 합격 조건 |
|---|---|---|
| deque | 마지막 1개 Pop/Steal 경합 100,000회 | 매 iteration 승자 정확히 1, 값/size/wrap 이상 0 |
| 세 모드 | fan-out, pure-worker, nested wait, overflow, 예외, shutdown drain | submitted/executed/counter exact, deadlock/유실/중복 0 |
| lifecycle | multi-producer Submit/Shutdown, stopped recursive Submit, external Wait/Shutdown, reinitialize, worker lifecycle guard | admission 경계 전 publish와 경계 후 inline 실행 모두 완료 |
| FLS | 1 worker에서 sibling parent/child Submit interleave | child/follow-up exact, wait/resume parity |
| pool | 1 worker에 waiting parent 80개 | 64 parked continuation 이후 inline fallback, pool miss > 0, 전체 완료 |

명령은 `Tools/Harness/RunJobSystemStress.ps1 -Mode all`이다. 이 PASS는 scheduler correctness 증거이지 GameRoom 병렬화의 FPS/TPS 향상 증거는 아니다. 성능은 제품 workload의 before/after capture로 별도 증명한다.

---

## §9. 학습 트리 — 어디서 무엇을 학습

### 기초 (1-2주)

| 주제 | 자료 |
|---|---|
| OS user/kernel mode | "Operating Systems: Three Easy Pieces" Ch 4-6 (무료 PDF) |
| Cache hierarchy | Ulrich Drepper "What Every Programmer Should Know About Memory" |
| Process/Thread | OSTEP Ch 26-28 |

### C++ Concurrency (1-2주)

| 주제 | 자료 |
|---|---|
| std::atomic memory ordering | Anthony Williams "C++ Concurrency in Action" 2nd Ed Ch 5 |
| Lock-free patterns | "The Art of Multiprocessor Programming" (Herlihy) |
| Cache + false sharing | Mike Acton "Data-Oriented Design and C++" GDC 2014 |

### ECS (1주)

| 주제 | 자료 |
|---|---|
| ECS basics | EnTT 라이브러리 docs (https://entt.docsforge.com/) |
| Mass Entity (UE5) | UE5 Mass Entity 공식 docs |
| Bevy ECS | https://bevy-cheatbook.github.io/programming/ecs-intro.html |

### Job System (1주)

| 주제 | 자료 |
|---|---|
| Work Stealing | Chase, Lev 2005 "Dynamic Circular Work-Stealing Deque" 논문 |
| Job System 설계 | Stefan Reinalter blog (https://blog.molecular-matters.com/) "Job System 2.0" 시리즈 |

### Fiber (1주)

| 주제 | 자료 |
|---|---|
| Win32 Fiber API | Microsoft Learn "Fibers" |
| Naughty Dog 모델 | Christian Gyrling GDC 2015 "Parallelizing the Naughty Dog Engine Using Fibers" (YouTube 무료) |
| 본 가이드 §6 + Winters 코드 | [FIBER_JOB_SYSTEM_v2.md](.md/plan/engine/FIBER_JOB_SYSTEM_v2.md) |

### 통합 (실전, 2-4주)

- Winters scheduler 구현은 완료됐으므로 actual Server/Client workload의 immutable input/output 경계와 DAG를 설계한다.
- `ThreadOnly` parity를 유지한 채 `FiberFull` opt-in capture에서 wall time, worker utilization, fiber waits/resumes/pool misses를 비교한다.
- 자가 평가: "이걸 회사 첫 출근에 30분 안에 설명할 수 있는가?"

이 학습 트리는 권장 순서이며 완료 기록이 아니다. 별도로 계획된 6주 mastery 프로그램은 2026-07-13 기준 미착수다.

---

## §10. 한 줄 요약

**OS scheduling/cache → C++ atomic과 memory ordering → ECS data/access contract → immutable job publication과 Chase-Lev → stackful Fiber continuation이 한 층씩 연결된다. Winters scheduler는 세 모드와 worker-local FiberFull, lifecycle admission, FLS를 구현하고 correctness stress를 통과했다. 기본값은 ThreadOnly이고, 실제 제품 speedup·서버 workload fan-out·6주 mastery는 별도의 적용/계측 과제다.**

## Canonical code pointers

- Job lifecycle and scheduler: `Engine/Public/Core/JobSystem.h`, `Engine/Private/Core/JobSystem.cpp`
- Chase-Lev deque: `Engine/Public/Core/JobSystem/WorkStealingDeque.h`
- Counter: `Engine/Public/Core/JobCounter.h`
- Fiber pool/types: `Engine/Public/Core/Fiber/FiberPool.h`, `Engine/Public/Core/Fiber/FiberTypes.h`
- FLS profiler stack: `Engine/Private/Core/Profiler/CPUProfiler.cpp`
- Executable gate: `Tools/Harness/JobSystemStress.cpp`, `Tools/Harness/RunJobSystemStress.ps1`
