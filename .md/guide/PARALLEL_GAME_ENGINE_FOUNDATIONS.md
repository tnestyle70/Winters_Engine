# Parallel Game Engine Foundations — OS 본질부터 ECS / JobSystem / Fiber 까지

> **작성일**: 2026-05-03
> **목적**: 게임 엔진 병렬화의 진짜 원리를 OS 본질 + C++ 기반 + Winters 코드 베이스로 한 흐름. "왜 OOP 가 느린가" → "왜 ECS 가 빠른가" → "왜 JobSystem 만으론 부족한가" → "왜 Naughty Dog 가 fiber 를 쓰는가" 까지 7-layer 학습 트리.
> **선수 지식**: C++14 이상, basic OS (process/thread), pointer/메모리 기초.
> **연결**:
> - 얕은 fiber 가이드: [FIBER_LEARNING_GUIDE.md](.md/guide/FIBER_LEARNING_GUIDE.md)
> - 신규 박제 코드: [.md/plan/engine/FIBER_JOB_SYSTEM_v2.md](.md/plan/engine/FIBER_JOB_SYSTEM_v2.md)
> - 현 코드: [Engine/Public/Core/JobSystem.h](Engine/Public/Core/JobSystem.h), [Engine/Public/Core/JobCounter.h](Engine/Public/Core/JobCounter.h)

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
| **Kernel Mode (Ring 0)** | 모든 권한 — HW 직접 제어 | **느림 — mode switch ~100ns** | `read()`, `write()`, `mutex lock`, OS API |

**왜 mode switch 가 느린가**:
1. CPU 의 `iret`/`sysret` 명령 실행 (수십 cycle)
2. **TLB flush 위험** — 가상 주소 변환 캐시 비움 (수백 cycle)
3. CPU pipeline flush (분기 예측 무효화)

**핵심**: 게임 엔진은 user mode 에서 최대한 머물러야 빠름. **fiber 가 user mode 만 쓰는 이유**.

### 1-2. Process vs Thread

| | Process | Thread |
|---|---|---|
| 주소 공간 | 독립 (가상 메모리 분리) | 공유 (같은 process 안) |
| 생성 비용 | ~ms | ~μs |
| 컨텍스트 스위치 | ~5μs (TLB flush 포함) | ~1μs |
| 통신 | IPC (파이프/소켓/공유메모리) | 직접 메모리 read/write |
| 보호 | OS 가 강제 | 같은 process 안 — 사람이 책임 |

**Thread 의 정체**:
- OS 의 TCB (Thread Control Block) — 레지스터 / stack pointer / 스케줄링 정보
- Linux 에서 thread = LWP (Light Weight Process) — `clone(CLONE_VM | CLONE_FS | ...)`
- Windows 에서 thread = `CreateThread()` — TCB 자체

### 1-3. Context Switch — 진짜 비용

Worker thread 가 wait → 다른 thread 로 전환 시 OS 가 하는 일:

```
1. 현재 thread 의 모든 레지스터 저장 (stack pointer, instruction pointer, GP regs)
   - x64 에서 16개 GP + 16개 XMM = ~256 bytes
2. 스케줄러 결정 (다음 thread 선택, ~수십 cycle)
3. 새 thread 의 레지스터 복원 (~256 bytes load)
4. ★ TLB partial flush (만약 다른 process 면 full flush)
5. CPU pipeline flush (분기 예측 무효화)
6. ★ Cache 워밍업 — 새 thread 의 데이터가 L1/L2 에 없으면 miss 누적
```

**실제 비용** (Intel Haswell 기준):
- Direct cost: ~1-2μs
- Indirect cost (cache miss 누적): ~5-50μs (workload 따라)

**의미**: thread 100개가 1ms 마다 swap 되면 → CPU 시간의 10-50% 가 swap overhead.

→ **게임 엔진은 thread 수를 코어 수와 같거나 적게** 유지. 그래서 `hardware_concurrency() - 2` 패턴.

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

**1 cache miss = 50-100ns = ~250 명령 실행 가능 시간**.

→ Cache 친화 = 게임 엔진 성능의 50% 이상.

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

`std::atomic<T>` = CPU 의 atomic 명령 (LOCK prefix on x64) wrapper. 단순 `int++` 도 race 발생 — atomic 이 보장.

**6 memory orders** (가장 어려운 부분):

| Order | 보장 | 비용 | 사용 |
|---|---|---|---|
| `relaxed` | atomicity 만 (ordering 없음) | 가장 빠름 | 단순 카운터 |
| `consume` | (deprecated, acquire 와 동일 취급) | — | — |
| `acquire` | 이후 read/write 가 이 시점 이후로 reorder X | 중간 | wait/lock 진입 |
| `release` | 이전 read/write 가 이 시점 이전으로 reorder X | 중간 | publish/unlock |
| `acq_rel` | acquire + release | 중간 | RMW (read-modify-write) |
| `seq_cst` | 전 thread 가 같은 순서 관찰 | 가장 비쌈 | 디버깅 안전 default |

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

**Mutex 의 진실**: user mode lock 시도 → 실패 시 kernel block (`futex_wait`).

```cpp
std::mutex m;
{
    std::lock_guard<std::mutex> lk(m);  // RAII — 자동 lock/unlock
    // critical section
}
```

**비용**:
- Uncontended (경쟁 없음): ~25ns (user mode 만)
- Contended: ~1-10μs (kernel 진입)
- High contention: ~100μs+ (priority inversion)

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
cv.notify_one();  // ★ kernel call
```

**문제**: `cv.wait()` = OS thread 진짜 멈춤. 게임 엔진의 worker 가 멈추면 그 thread 의 코어 유휴.

**Phase 5-A 해결**: cv 제거 → busy-wait + help-stealing (`WaitForCounter`):
```cpp
while (pCounter->Load() > iTarget) {
    if (!TryExecuteOneJob(workerIdx))
        std::this_thread::yield();  // ★ user mode yield (CPU 양보, thread 안 멈춤)
}
```

**Phase 5-B 추가 해결**: fiber yield → thread 가 다른 fiber 픽업.

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

| 측면 | OOP | ECS | 배수 |
|---|---|---|---|
| Cache miss / 1000 entity | ~5000 | ~50 | **100배** |
| SIMD 가능성 | 불가 | 자연 | — |
| 병렬화 가능성 | virtual + heap = 어려움 | system 단위 깔끔 | — |
| 메모리 (1000 entity) | ~500KB (포인터 + heap fragments) | ~100KB (연속) | **5배** |

→ **ECS = 게임 엔진의 표준 (UE5 의 Mass Entity, Unity DOTS, Bevy 모두)**.

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
    alignas(64) std::atomic<int64_t> m_iBottom{ 0 };
    alignas(64) std::atomic<int64_t> m_iTop{ 0 };
    std::array<T, 4096> m_arrBuf;

public:
    bool Push(const T& v) {  // owner only
        int64_t b = m_iBottom.load(std::memory_order_relaxed);
        int64_t t = m_iTop.load(std::memory_order_acquire);
        if (b - t >= kCapacity) return false;
        m_arrBuf[b & MASK] = v;
        std::atomic_thread_fence(std::memory_order_release);  // ★ fence
        m_iBottom.store(b + 1, std::memory_order_relaxed);
        return true;
    }

    bool Pop(T& out) {  // owner only
        int64_t b = m_iBottom.load(std::memory_order_relaxed) - 1;
        m_iBottom.store(b, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_seq_cst);  // ★ fence
        int64_t t = m_iTop.load(std::memory_order_relaxed);
        if (t > b) {  // empty
            m_iBottom.store(t, std::memory_order_relaxed);
            return false;
        }
        out = m_arrBuf[b & MASK];
        if (t != b) return true;  // 일반 case
        // 마지막 1개 — Steal 와 경쟁
        bool ok = m_iTop.compare_exchange_strong(
            t, t + 1,
            std::memory_order_seq_cst,
            std::memory_order_relaxed);
        m_iBottom.store(t + 1, std::memory_order_relaxed);
        return ok;
    }

    bool Steal(T& out) {  // any thread
        int64_t t = m_iTop.load(std::memory_order_acquire);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        int64_t b = m_iBottom.load(std::memory_order_acquire);
        if (t >= b) return false;
        out = m_arrBuf[t & MASK];
        if (!m_iTop.compare_exchange_weak(
            t, t + 1,
            std::memory_order_seq_cst,
            std::memory_order_relaxed))
            return false;
        return true;
    }
};
```

### 4-3. Counter (atomic only — cv 제거됨)

```cpp
// JobCounter.h
class CJobCounter {
    std::atomic<uint32_t> m_iCount{ 0 };
public:
    void Increment(uint32_t n = 1) { m_iCount.fetch_add(n, std::memory_order_relaxed); }
    void Decrement() { m_iCount.fetch_sub(1, std::memory_order_acq_rel); }
    uint32_t Load() const { return m_iCount.load(std::memory_order_acquire); }
};
```

**왜 cv 제거**: cv.wait() = OS block. Worker thread 가 진짜로 멈춤.

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

**Fiber 가 해결하는 것**: C 가 wait 하는 동안 fiber 만 wait list 로 → worker 0 는 즉시 D 같은 다른 fiber 픽업. CPU 100% 활용.

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

→ Phase 5-B fiber 도입 시 5 정책 모두 그대로 작동 (정합성).

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

3 자료구조:

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

    // 4) Fiber Pool — 128 fiber
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
Worker 3: TryExecuteOneJob → ready 큐 우선 검사 → A 픽업
   ↓
Fiber_TryResumeOne:
   1. A.SetReturnFiber(thread_fiber 3)  ← ★ resume worker 의 thread fiber
   2. ctx.iCurrentFiber = A_idx
   3. SwitchToFiber(A)  ← A 의 stack frame 복원
   ↓
Fiber A: WaitForCounter 의 SwitchToFiber 다음 줄에서 재개
   - counter == 0 보장
   - 다음 코드 진행
```

### 6-4. Get_WorkerSlot 함정 (★ 가장 어려운 디버깅)

Fiber A 가 worker 0 에서 시작 → worker 3 에서 resume.

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

### 6-6. Naughty Dog GDC 2015 모델 완성형

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
│  │   Fiber Pool 128             │            │
│  │ F0(Run) F1(Wait) F2(Free) ...│            │
│  └──────────────────────────────┘            │
│                                                │
│  ┌── m_mapWaiters (CJobSystem) ──┐            │
│  │ counter A → [F1, F5]          │            │
│  │ counter B → [F12]             │            │
│  └───────────────────────────────┘            │
│                                                │
│  ┌── m_ReadyFibers ──────────────┐            │
│  │ [F3, F7, F1] queue            │            │
│  └───────────────────────────────┘            │
└────────────────────────────────────────────────┘
```

**3 핵심**:
1. Worker thread = 절대 안 멈춤 (busy 또는 다른 fiber 픽업)
2. Pool = 실행 컨텍스트 (stack + 코드 위치)
3. Wait map + Ready queue = 진행 상태 박제

**Naughty Dog 결과**: 게임 프레임 30 FPS → 60 FPS 안정. 모든 코어 95%+ 활용.

---

## §7. Winters 코드 적용

### 7-1. Phase 5-A 의 흐름 (현재)

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
   shutdown
```

### 7-2. Phase 5-B 의 흐름 (FiberFull 모드)

```
[Main thread] — 변경 0
   기존 그대로

[Worker thread]
   Initialize: ConvertThreadToFiber(NULL)  ← thread → fiber
   while !shutdown:
       1. ★ Ready fiber 우선 검사 (yield 됐던 것 깨움)
          → SwitchToFiber(ready_fiber)
            → fiber 의 stack frame 복원, 작업 진행
            → 작업 끝 또는 다시 yield
          → 복귀 시 Pool Release
       2. 자기 deque pop
       3. global pop
       4. steal
       → ExecuteItem:
            Pool.Acquire() → fiber idx
            fiber.AssignJob(wrap_lambda)
            SwitchToFiber(fiber)  ← fiber 진입
              → 작업 (또는 yield)
            복귀 → Pool Release (Free) 또는 대기 (Waiting)
```

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
   - FiberFull 모드면 wait 시 yield → 다른 fiber 픽업
   - thread 는 절대 멈추지 않음

3. EndFrame
```

---

## §8. 실전 디버깅 + 측정

### 8-1. thread_local + Fiber Resume 함정

```cpp
thread_local std::vector<float> tls_buffer;

void Function_With_Yield() {
    tls_buffer.clear();
    DoWork(tls_buffer);
    WaitForCounter(&someCounter);  // ★ yield 가능
    // ★ 깨어나면 다른 worker 의 tls_buffer 봄!
    UseBuffer(tls_buffer);  // ← 의도한 buffer 가 아님
}
```

**해결**: yield 호출 가능 함수 안에서 thread_local 대신 stack 변수 사용.

### 8-2. Profiler 통합

```cpp
WINTERS_PROFILE_SCOPE("MinionAI::DecisionPass");
// ...
WINTERS_PROFILE_COUNT("Nav::PathNodes", nodeCount);
```

**Worker-Safety v3 적용**:
- Profiler scope stack = thread_local (CPUProfiler.cpp t_vProfilerStack)
- Event merge = mutex + main flush
- Fiber resume 시 thread_local 변경되지만 stack 자체는 보존됨 (RAII)

### 8-3. Stress 시나리오 (FIBER_JOB_SYSTEM_v2.md §5-2)

| # | 시나리오 | 합격 |
|---|---|---|
| S1 | 16 미니언 1 타겟 집중 공격 100회 | 데미지 손실 0 |
| S2 | 16 worker × Submit 의존 그래프 1000회 | wait 정확 깨움, deadlock 0 |
| S3 | Get_WorkerSlot resume 100 fiber × 100 yield | per-slot buffer race 0 |
| S4 | FiberPool 고갈 (128+1 동시 Submit) | inline fallback 정상 |
| S5 | 16 동시 pathfinding | counter contention 1/노드 → 1/함수 |
| S6 | Counter destroy 안전성 1000회 | m_mapWaiters 누수 0 |

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

- Winters 의 5-B 직접 구현 (M1 → M2 → M3 → M4)
- Stress S1-S6 통과
- 자가 평가: "이걸 회사 첫 출근에 30분 안에 설명할 수 있는가?"

---

## §10. 한 줄 요약

**OS 의 user/kernel mode + cache hierarchy + false sharing → C++ atomic + memory ordering + alignas + lock-free → ECS 의 SoA + cache coherency → Job System 의 work stealing + counter → ECS+JobSystem 의 access contract + Decision/Apply 2-pass → Fiber 의 user-mode 양보 + wait list + resume → Naughty Dog 의 worker N + pool 128 + wait map + ready queue. 7 layer 가 게임 엔진 프레임 17ms 안에 모든 의존성 그래프를 굴리는 진짜 원리. Winters 의 Phase 5-B 는 layer 6-7 의 첫 도입 — 5-A (Job System + Worker-Safety v3) 위에 fiber 4 모드 (ThreadOnly/Shell/Pool/Full) 쌓아 점진 적용.**
