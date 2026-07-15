# Fiber 기반 Job System 구현 계획서 v2.1

> [!IMPORTANT]
> **Historical design.** 아래 본문을 현재 구현 상태로 사용하지 않는다. 최신 기준은 [2026-07-13 canonical implementation plan](../2026-07-13_UDP_JOB_SYSTEM_CHASE_LEV_FIBER_IMPLEMENTATION_PLAN.md)과 [S023 결과 보고서](../../build/2026-07-13_UDP_JOB_SYSTEM_CHASE_LEV_FIBER_RESULT.md)다.
> As-built delta: JobSystem Submit race, Chase-Lev deque, FiberFull 및 stress 구현은 완료되었고, UDP v3 generic vertical slice와 server hub/client facade가 구현되었다. main F5 통합과 최종 build 상태는 S023 결과 보고서를 따른다. 6주 Fiber mastery 프로그램은 미착수이며, 현재 상태는 production UDP cutover가 아니다.
> 과거 UDP v2 수치인 **24 B header / 10 B fragment header / 1 MiB logical payload**는 historical design이다. 실제 v3 상수는 **40 B header / 16 B fragment header / 1,200 B datagram / 64 KiB logical payload**다.

> **상태 동기화 (2026-07-11)**: 이 문서는 2026-05-02의 목표 설계다. M1에 해당하는 per-job FiberShell 골격만 dormant 상태로 들어갔고, M2 FiberPool·M3 wait/yield/resume·M4 AnimUpdate 및 전용 stress는 완료되지 않았다. 현재 runtime은 `ThreadOnly`이며 최신 판정은 [2026-07-11 상태 감사](../2026-07-11_JOB_SYSTEM_CHASE_LEV_FIBER_STATE_AUDIT.md)를 따른다.
>
> **개정일**: 2026-05-02
> **현 버전 메타**: **v2.1 (Codex 검토 6건 반영)** — JobCounter 변경 0 / wait list JobSystem 내부 map 으로 이전 / M0 활성 단계화 4단계 / Get_WorkerSlot 결정 박제 / Phase 5-A 본체 보존 명시 / main thread fiber 화 X 박제
> **이전 버전**: `.md/plan/engine/FIBER_JOB_SYSTEM.md` (v1, 2026-04-08) — Phase 5-A 완료 + WORKER_SAFETY_PACKAGE v3 (2026-04-28) 이전 작성, stale 5건
> **전제 의존**:
> - **Phase 5-A 완료** — Chase-Lev WorkStealingDeque + Global queue hybrid + help-stealing 작동 ([Engine/Public/Core/JobSystem.h:23](Engine/Public/Core/JobSystem.h:23) "5-B 에서 내부가 Fiber 로 교체되어도 public API 불변" 주석)
> - **`WORKER_SAFETY_PACKAGE.md` v3 완료 권장** — Decision/Apply 2-pass / Get_WorkerSlot 규칙 / per-worker buffer / Scheduler access contract / AStar scope guard
> - **CLAUDE.md §1.A Track 3** "1차 목표 = Fiber shell only" + Get_WorkerSlot fiber resume 함정 박제
> **목표**: 기존 `CJobSystem` 내부를 fiber 화 (★ 별도 `CFiberJobSystem` 신설 X) — public API 불변 유지하며 4단계 (M1 shell → M2 pool → M3 yield+wait → M4 AnimUpdate 병렬화) 점진 도입

---

## §0. v1 (2026-04-08) Stale 지점 5건 — v2 가 정정

| # | v1 가정 (2026-04-08) | 실제 (2026-05-02) | v2 정정 |
|---|---|---|---|
| A | "현재 CJobSystem Worker 가 `condition_variable::wait()` 로 OS 스레드 블로킹" | [Engine/Public/Core/JobCounter.h:11](Engine/Public/Core/JobCounter.h:11) — "cv/mutex 제거됨. Wait() API 삭제" 박제 + atomic 만 | §1 동기 부여 갱신 — fiber 도입 이유는 "AnimUpdate 병렬화 + RenderSubmit 의존성 그래프 yield" 로 재정렬 |
| B | 13 신규 파일 (`CFiberJobSystem`/`CFiberPool`/`CFiberCounter`/`CJobQueue`/`CWorkerThread`/`CFiber`) + `Engine/Header`·`Engine/Code` 폴더 | 컨벤션은 `Engine/Public`·`Engine/Private`, 파일명 C 접두사 금지 (CLAUDE.md §네이밍) | §2 — **5 신규 파일** (`Fiber.h/.cpp`, `FiberPool.h/.cpp`, `FiberTypes.h`). `CFiberJobSystem`/`CFiberCounter`/`CJobQueue`/`CWorkerThread` 신설 X |
| C | 별도 `CFiberJobSystem` 클래스 신설 → `CJobSystem` 래퍼화 | [JobSystem.h:23](Engine/Public/Core/JobSystem.h:23) 주석 "Phase 5-B 에서 내부가 Fiber 로 교체되어도 public API 불변" — 의도는 **내부 교체** | §3 — `CJobSystem` 내부 fiber 모드 분기 (`EMode { ThreadOnly, FiberShell, FiberPool, FiberFull }`) |
| D | `CFiberCounter` 신설 (Wait List 별도) — 호출부 `WaitForCounter` 변경 | 호출부 모두 `pJobSystem->WaitForCounter(&c)` 로 이미 이관 (B-10c 본체 완료) | §3 — **★ v2.1 정정**: CJobCounter **변경 0**. Wait list 는 CJobSystem 내부 `unordered_map<CJobCounter*, ...>` 로 관리 (Codex #4) |
| E | WORKER_SAFETY_PACKAGE v3 와의 정합성 0 (작성 시점 차이 20일) | v3 의 결정 5종 — slot 규칙 (main=0/worker=idx+1) / Decision/Apply / per-worker buffer / Scheduler access contract / AStar scope guard | §4 — fiber resume 시 "현재 thread" 의 slot 사용 (CPUProfiler `t_vProfilerStack` thread_local 패턴 동일), Scheduler/MinionAI 와의 상호작용 명시 |

추가 작은 어긋남:
- v1 `JobDecl { pfnEntryPoint, pParam, ePriority }` ↔ 현 [JobDecl.h:8-14](Engine/Public/Core/JobSystem/JobDecl.h:8) `JobFn pFn / void* pData / const char* pszName` — v2 는 현 시그니처 유지 (priority 는 M3+ 옵션)
- v1 `WINTERS_LOG` 사용 — Engine 에 정의 ([Engine/Public/WintersCore.h](Engine/Public/WintersCore.h)) 단 _DEBUG 가드 여부 미검증 → v2 는 `OutputDebugStringA` + `#ifdef _DEBUG` 로 명시 (CLAUDE.md 보안 §5)
- v1 `array<>`/`atomic<>` (std:: 생략) — 공개 헤더는 `std::` 명시 (CLAUDE.md gotcha B-6.6)

---

## §0.1 Changelog v2 → v2.1 (Codex 검토 6건 반영)

| # | Codex 지적 | v2 본문 | v2.1 정정 |
|---|---|---|---|
| 1 | 경로/구조 보정 (`Engine/Header` → `Engine/Public`) | v2 본문에는 v1 잔재 없음 (이미 `Engine/Public`/`Private` 사용) | **OK — 변경 없음**. v1 인용 부분 (§0 표 B) 만 비교용으로 유지 |
| 2 | Phase 5-A 의 main → global / worker → self 분기 보존 | v2 §3-4 수정 어디서도 EnqueueJob 미수정 (보존됨) | §3-4 첫머리에 **"보존 박제"** 1줄 추가 + §0 §A 표에 박제 |
| 3 | Get_WorkerSlot fiber-local 의미 재정의 | v2 §4-1 — "현재 thread 의 slot + push-yield 금지" 로 정리 | §4-1 보강 — **옵션 A (현행 thread 기반 유지) vs 옵션 B (fiber-local origin slot)** 비교 + **결정: 옵션 A** + push-yield gotcha 강제 |
| 4 | CJobCounter 안에 wait list 위험 (stack lifetime + mutex contention) | v2 §3-1 / 3-2 — JobCounter 에 wait list 멤버 5개 추가 + JobCounter.cpp 신규 | **★ 폐기**. JobCounter **변경 0**. Wait list 는 CJobSystem 의 `std::unordered_map<CJobCounter*, CounterWaitState>` 로 이전. Counter destroy 안전성: `WaitForCounter` 정상 return = map entry 이미 erase |
| 5 | Main thread fiber 화 안 함 (당분간 thread+steal 유지) | v2 §3-4 수정 6 — main 분기에 `if (m_eMode == FiberFull && t_iWorkerIdx >= 0)` 가드 (≥0 검사로 main 제외) | **OK — 변경 없음**. §1 M3 설명에 "main thread 는 fiber 화 X" 명시 추가 |
| 6 | Set_JobSystem 단계 활성 (Transform → MinionAI → Navigation 순) | v2 §1 M0 — 한꺼번에 활성 가정 | §1 M0 — **M0a (stress only) → M0b (Transform) → M0c (MinionAI) → M0d (Navigation)** 4단계 박제 |

추가:
- §7 일정: M3 1.5 → 2.0일 (waiter map 분리 작업 +0.5일). 합계 3.5 → **4.0일**
- §6 gotchas: 1건 추가 (counter wait list 직접 박제 금지)
- §5-3 Profiler 카운터: `Fiber::WaitMapInsert` / `Fiber::WaitMapHit` 신규

---

## §1. Phase 5-B 단계화 — M0 ~ M4

### M0. 진입 전제 + Set_JobSystem 단계 활성 (★ Codex #6, Fiber 코드 0줄)

> **2026-07-11 상태**: 아래 M0a~d는 2026-05의 진입 순서 기록이다. 현재 Transform은 항상, local-only Nav/LocalUnitAI는 조건부로 JobSystem이 주입된다. 전용 stress만 여전히 없다.

**M0a** — Phase 5-A stress 검증 only (당시에는 모든 Set_JobSystem 비활성 유지)
- 작업: JobSystem 자체에 `N=16 worker × 1000 Submit dummy job` stress harness 추가
- 위치: `Tests/JobSystemStress.cpp` (신규) 또는 ImGui 디버그 패널의 "Stress" 버튼
- 검증: race 0, deadlock 0, Submit→Counter 정확. 1만회 반복 후 누수 0
- 합격 시 다음

**M0b** — TransformSystem 활성 (낮은 위험 — TransformComponent write-only)
- 작업: [Scene_InGame.cpp:296 부근](Client/Private/Scene/Scene_InGame.cpp:296) `pTrans->Set_JobSystem(pJS);` 활성
- 검증: 챔프 7체 + 미니언 60체 위치 변경 정확. F5 회귀 0
- 합격 시 다음

**M0c** — MinionAISystem 활성 (★ WORKER_SAFETY_PACKAGE v3 §2 Decision/Apply 2-pass 적용 후)
- 전제: v3 §2 의 `MinionDecision` / `Push_Decision` / `ApplyPass` 구현 완료
- 작업: Scene_InGame.cpp `pAI->Set_JobSystem(pJS);` 활성
- 검증: 16 미니언이 1 타겟 집중공격 100회, **데미지 손실 0** (v3 §2.6 M9)

**M0d** — NavigationSystem 활성 (★ 가장 위험 — 마지막)
- 전제: AStar scope guard (v3 §5) 적용 완료
- 작업: Scene_InGame.cpp `pNav->Set_JobSystem(pJS);` 활성
- 검증: 16 worker × 동시 pathfinding 1000회 race 0, counter contention 1/노드 → 1/함수

**M0 합격 기준**: ThreadOnly 모드 (fiber 코드 0줄 도입 X) + 모든 시스템 병렬화 + 1000 frame race 0 + 7 챔프 동작 동일.

**branch**: `feature/phase5b-m0` → 합격 시 tag `phase5a-final` (모든 fiber 단계의 안전 복귀 지점)

### M1. Fiber Shell Only — yield 없음 (검증용 0.5일)

**목표**: `ConvertThreadToFiber` + `CreateFiber` + `SwitchToFiber` + 복귀 사이클이 deadlock 없이 작동함을 확인. wait list / pool / yield 모두 도입 X.

**핵심 흐름**:
```
WorkerLoop 진입 → ConvertThreadToFiber(nullptr) → m_hThreadFiber 보관
└─ TryExecuteOneJob 가 WorkItem 픽업
   └─ ExecuteItem (FiberShell 모드 분기)
      ├─ 매 job 마다 CreateFiber + AssignJob + SwitchToFiber(fiber)
      ├─ Fiber 가 job() + counter->Decrement() + SwitchToFiber(thread_fiber) 로 복귀
      └─ 복귀 후 DeleteFiber (M2 에서 pool 로 교체)
```

**합격 기준**:
- 1000 frame F5 회귀 0 (스킬/이동/dash 모두 동일 동작)
- VS Performance Profiler — Worker thread 의 `SwitchToFiber` 카운트 = Submit 카운트 × 2 (왕복)
- ETW Stack — `ConvertThreadToFiber` Worker 마다 1회만 호출

**의도된 비효율**: 매 job 마다 CreateFiber/DeleteFiber → ~1us 오버헤드. M2 에서 pool 화로 ~20ns 로 축소 예정. M1 에서는 정확성 검증만.

**Main thread 는 fiber 화 X** (★ Codex #5): `t_iWorkerIdx == -1` 조건이 main 을 식별. ConvertThreadToFiber 는 WorkerLoop 안에서만 호출. main 은 기존 `WaitForCounter` 의 help-stealing 그대로.

### M2. Fiber Pool — Acquire/Release (0.5일)

**목표**: `CFiberPool` 도입. 매 job 마다 CreateFiber 회피, free-stack LIFO 로 캐시 친화적 재사용.

**핵심 변경**:
- `CJobSystem::Initialize` 가 `m_pFiberPool->Initialize(this)` 호출 (FIBER_POOL_SIZE=128)
- `ExecuteItem` (FiberPool 모드) — `m_pFiberPool->Acquire()` → `Switch` → 복귀 시 `Release(idx)`
- 풀 고갈 (Acquire == UINT32_MAX) 시 inline fallback (job 직접 실행)

**합격 기준**:
- 1000 Submit 후 fiber 누수 0 (`m_iFreeTop == FIBER_POOL_SIZE` 단언)
- F5 회귀 0
- Profiler — `CreateFiber` 호출 횟수 = `FIBER_POOL_SIZE` (128) 한 번만 (Initialize 시점)

**여전히 yield 없음**: 모든 job 이 SwitchToFiber → job 완료 → 복귀의 1-shot. WaitForCounter 는 기존 help-stealing 그대로.

### M3. Yield + Wait List — Fiber Full Mode (★ v2.1: 1.5 → 2.0일)

**목표**: Naughty Dog GDC 2015 모델 완성. WaitForCounter 가 worker fiber 컨텍스트면 OS thread 블로킹 X, fiber yield → wait list 등록 → counter 가 target 도달 시 다른 worker 가 깨워서 다른 thread 에서 resume.

**Main thread 는 여전히 fiber 화 X** (★ Codex #5): `WaitForCounter` 의 main 분기는 기존 global drain → steal 그대로. fiber yield 분기는 `t_iWorkerIdx >= 0` (worker thread) AND `m_eMode == FiberFull` AND `ctx.iCurrentFiber != UINT32_MAX` (fiber 진행 중) 모두 만족할 때만 진입.

**핵심 변경 (v2.1 Codex #4 정정)**:

★ **CJobCounter 변경 0**. Wait list 는 `CJobSystem` 내부:
```cpp
struct CounterWaitState
{
    std::vector<std::uint32_t> vecWaitFibers;   // wait 중인 fiber idx 들
    std::uint32_t              iTarget = 0;
};

class CJobSystem
{
private:
    std::mutex                                          m_WaiterMutex;
    std::unordered_map<CJobCounter*, CounterWaitState>  m_mapWaiters;
};
```

**Counter destroy 안전성** (Codex #4 의 핵심 우려):
- CJobCounter 는 SystemScheduler / TransformSystem / MinionAISystem / NavigationSystem 등에서 **stack 변수** 로 사용 (`CJobCounter counter; ... WaitForCounter(&counter); ... return;`)
- Caller 가 `WaitForCounter` 정상 return = `Fiber_NotifyCounterComplete` 가 map entry 를 erase 한 상태
- Counter destroy 시점 = scope 종료 = 그 시점에 wait map 에 해당 counter key 없음 보장
- 비정상 패턴 (caller 가 wait 누락 후 destroy) — 기존 5-A 동작도 동일하게 미정의 (caller 책임)
- 추가 안전 가드: `Fiber_TryRegisterWait` 에 register 직전 `pCounter->Load() <= iTarget` 재확인 (race)

**Mutex contention 해소** (Codex #4 의 두 번째 우려):
- v2 v1 안: counter 안 mutex + worker 완료 람다의 mutex = 같은 lock
- v2.1: counter 는 atomic 만, JobSystem 의 `m_WaiterMutex` 가 wait list 전용 — worker 완료 람다는 atomic decrement + map lookup (짧은 lock)

**흐름**:
```
WaitForCounter(&c) [worker fiber 컨텍스트]
  ├─ Fiber_YieldToCounter(&c, target)
  │   ├─ Fiber_TryRegisterWait(&c, target, currentFiber)
  │   │   ├─ lock(m_WaiterMutex)
  │   │   ├─ if (c.Load() <= target) return false   // race 체크
  │   │   ├─ m_mapWaiters[&c].vecWaitFibers.push(currentFiber)
  │   │   └─ return true
  │   ├─ fiber.SetState(Waiting)
  │   └─ SwitchToFiber(thread_fiber)   // worker 가 다음 job 픽업
  │
  └─ (다른 worker 가 ready 큐에서 SwitchTo 한 후) 깨어남, return

ExecuteItem 의 wrap 람다 [job 완료 직후]
  ├─ pCounter->Decrement()
  └─ Fiber_NotifyCounterComplete(pCounter)
      ├─ lock(m_WaiterMutex)
      ├─ if (pCounter->Load() > target) return    // 아직 미도달
      ├─ notify = move(m_mapWaiters[pCounter].vecWaitFibers)
      ├─ m_mapWaiters.erase(pCounter)             // ★ entry 비우고 erase
      ├─ unlock
      └─ ready 큐로 push (m_ReadyMutex)
```

**★ Fiber resume 시 worker 이동 — Get_WorkerSlot 함정** (CLAUDE.md §1.A Track 3 핵심, §4-1 상세):
- Fiber A 가 worker 0 에서 시작 → WaitForCounter yield → wait list
- Counter 도달 → ready 큐 push
- Worker 3 이 ready 큐에서 fiber A 픽업 → `SwitchToFiber(worker3_thread_fiber)` → fiber A 가 worker 3 에서 resume
- **fiber A 의 코드가 `CJobSystem::Get_WorkerSlot()` 호출하면 worker 3 의 slot (= 4) 반환**
- WORKER_SAFETY_PACKAGE v3 의 `Push_Decision` 패턴은 안전 (push 직전 호출 + 그 사이 yield 0) — 단 yield 가능 함수 안에서 slot 캐시 금지

→ §4-1 옵션 비교 + §6 gotcha 박제 의무.

**합격 기준**:
- 1000 frame F5 회귀 0
- 단일 vs FiberFull 결과 deterministic
- Stress: 16 worker × Submit 의존 그래프 (A→B→C 체인 1000회) — 모든 wait 정확 깨움
- Get_WorkerSlot resume 안전성 — yield 전후 slot 다른 fiber 100개 동시 실행, per-slot buffer race 0
- Counter destroy 안전성 — `CJobCounter c; { ... wait...; }` 1000회 반복, m_mapWaiters 누수 0

### M4. AnimUpdate 병렬화 — 1차 실용 케이스 (0.5일)

**목표**: CLAUDE.md "추가 2~3ms 절감" 실현. 미니언 + 챔프 + 정글몹 의 `CAnimator::Update` 묶음 Submit, counter wait.

**핵심 변경**:
- 신규 시스템 `CAnimationSystem` (Phase = TBD, Render 직전) 또는 caller (`CEngineApp::OnUpdate` / `Scene_InGame::OnUpdate`) 에서 묶음 Submit
- `RenderComponent::bAnimated == true` 인 엔티티만 수집 → `vecAnimated` → 100+ 시 병렬, 미만 직렬
- 각 job 은 **자기 entity 의 ModelRenderer/Animator 만 mutate** (Worker-Safety v3 의 self-entity-only 원칙)

**합격 기준**:
- Frame time: 9ms → 7ms (CLAUDE.md "Frame 17.8 → 9ms" 이후 추가 -2ms)
- 미니언 60+ + 챔프 10 라인전에서 애니 동기화 정확 (분기 0)
- Profiler — `Anim::UpdateCalls` counter 가 worker 별 분산

---

## §2. 신규 파일 5개 (h/cpp 전문)

### 2-1. `Engine/Public/Core/Fiber/FiberTypes.h` (신규)

```cpp
#pragma once

// ─────────────────────────────────────────────────────────────
// FiberTypes.h — Fiber Job System 공통 타입
//
// Engine/Public 노출 헤더. Client TU 에서 직접 include 하지 않음
// (CJobSystem 내부 구현 디테일). 단 Engine 내부 TU 가 공유.
//
// 의존성:
//   - <Windows.h> — Win32 Fiber API (LPVOID, CreateFiber 등)
//   - <cstdint>   — std::uint32_t / uint8_t
// 공개 헤더 컨벤션 — std:: 명시 (CLAUDE.md gotcha B-6.6)
// ─────────────────────────────────────────────────────────────

#include <Windows.h>
#include <cstdint>

// Win32 Fiber 핸들 (LPVOID = void*)
using FiberHandle = LPVOID;

enum class EFiberState : std::uint8_t
{
    Free,     // 풀에서 대기 (Job 없음)
    Ready,    // Job 배정 완료, 실행 또는 resume 대기
    Running,  // 현재 어떤 Worker 가 실행 중
    Waiting,  // WaitForCounter yield 로 wait list 등록됨
};

// 상수 — 1 Worker 당 fiber 가 아니라 시스템 전역 풀
constexpr std::uint32_t FIBER_STACK_SIZE              = 64u * 1024u;  // 64 KB / fiber
constexpr std::uint32_t FIBER_POOL_SIZE               = 128u;         // 동시 fiber 수
constexpr std::uint32_t MAX_WAIT_FIBERS_PER_COUNTER   = 32u;          // counter 당 wait list 상한 (런타임 vector 사용 — soft limit)
```

### 2-2. `Engine/Public/Core/Fiber/Fiber.h` (신규)

```cpp
#pragma once

// ─────────────────────────────────────────────────────────────
// CFiber — Win32 Fiber 1개의 RAII 래퍼.
//
// 생성: CFiberPool::Initialize 가 풀 안에서 일괄 Create.
// 실행: ExecuteItem 이 AssignJob → SwitchToFiber(handle).
// 완료: FiberProc 무한 루프가 SwitchToFiber(returnFiber) 로 복귀.
// 재사용: CFiberPool::Release 가 Reset 후 free 스택 push.
//
// 주의: array<CFiber, N> 멤버 보관 가능해야 하므로 default ctor public.
//       단 Create() 호출 전에는 어떤 메서드도 호출 금지.
// ─────────────────────────────────────────────────────────────

#include "Core/Fiber/FiberTypes.h"
#include <functional>

class CJobSystem; // 전방선언

class CFiber
{
public:
    CFiber() = default;
    ~CFiber();

    CFiber(const CFiber&) = delete;
    CFiber& operator=(const CFiber&) = delete;

    // ── 생성/파괴 ────────────────────────────────────────────
    // iIndex: 풀 내 인덱스 (디버깅 + ready queue 식별)
    // pOwner: 완료 통보용 (Decrement/notify 시 owner 의 풀에 반환)
    bool Create(std::uint32_t iIndex, CJobSystem* pOwner);
    void Destroy();

    // ── Job lifecycle ────────────────────────────────────────
    // Free → Ready
    void AssignJob(std::function<void()> job);
    // 완료 후 풀 반환 직전 — Job/state 초기화
    void Reset();

    // ── Worker 컨텍스트 연결 ─────────────────────────────────
    // 현재 fiber 가 yield 또는 완료 시 SwitchToFiber 할 대상.
    // ★ M3 에서 fiber 가 다른 worker 에서 resume 될 수 있으므로
    //   resume 직전 Fiber_TryResumeOne 가 호출되어 갱신됨.
    void        SetReturnFiber(FiberHandle h) { m_hReturnFiber = h; }
    FiberHandle GetReturnFiber() const        { return m_hReturnFiber; }

    // ── Getter / Setter ──────────────────────────────────────
    FiberHandle   GetHandle() const          { return m_hFiber; }
    std::uint32_t GetIndex()  const          { return m_iIndex; }
    EFiberState   GetState()  const          { return m_eState; }
    void          SetState(EFiberState s)    { m_eState = s; }

private:
    // Win32 콜백 — 무한 루프, 절대 return 금지
    // (return 하면 worker thread 자체 종료)
    static void CALLBACK FiberProc(LPVOID lpParam);

private:
    FiberHandle           m_hFiber       = nullptr;
    FiberHandle           m_hReturnFiber = nullptr;
    std::uint32_t         m_iIndex       = UINT32_MAX;
    EFiberState           m_eState       = EFiberState::Free;
    std::function<void()> m_Job;
    CJobSystem*           m_pOwner       = nullptr;
};
```

### 2-3. `Engine/Private/Core/Fiber/Fiber.cpp` (신규)

```cpp
#include "WintersPCH.h"
#include "Core/Fiber/Fiber.h"
#include "Core/JobSystem.h"

CFiber::~CFiber()
{
    Destroy();
}

bool CFiber::Create(std::uint32_t iIndex, CJobSystem* pOwner)
{
    m_iIndex = iIndex;
    m_pOwner = pOwner;
    m_eState = EFiberState::Free;

    // 64KB stack, FiberProc 진입점, this 가 콜백 인자
    m_hFiber = ::CreateFiber(FIBER_STACK_SIZE, &CFiber::FiberProc, this);

#ifdef _DEBUG
    if (!m_hFiber)
    {
        char buf[128];
        ::sprintf_s(buf, "[CFiber] Create FAILED idx=%u err=%lu\n",
                    iIndex, ::GetLastError());
        ::OutputDebugStringA(buf);
    }
#endif

    return m_hFiber != nullptr;
}

void CFiber::Destroy()
{
    if (m_hFiber)
    {
        // ★ 현재 실행 중인 fiber 를 DeleteFiber 하면 안 됨.
        //   Pool::Shutdown 시점에만 호출 (모든 worker 종료 후)
        ::DeleteFiber(m_hFiber);
        m_hFiber = nullptr;
    }
    m_eState = EFiberState::Free;
}

void CFiber::AssignJob(std::function<void()> job)
{
    m_Job    = std::move(job);
    m_eState = EFiberState::Ready;
}

void CFiber::Reset()
{
    m_Job          = nullptr;
    m_hReturnFiber = nullptr;
    m_eState       = EFiberState::Free;
    // ★ m_iIndex / m_pOwner / m_hFiber 는 보존 (재사용 가능)
}

// ─── Win32 콜백 ────────────────────────────────────────────
// 무한 루프. SwitchToFiber 진입 → job 실행 → 복귀.
// 다음 SwitchToFiber 진입 시 SwitchToFiber 다음 줄에서 재개.
void CALLBACK CFiber::FiberProc(LPVOID lpParam)
{
    CFiber* pSelf = static_cast<CFiber*>(lpParam);

    while (true)
    {
        if (pSelf->m_Job)
        {
            pSelf->m_eState = EFiberState::Running;
            pSelf->m_Job();
            pSelf->m_Job = nullptr;
        }

        // job 완료 — Free 로 표시 후 worker 의 thread fiber 로 복귀
        // Pool 반환 / counter Decrement 등은 ExecuteItem 의 wrap 람다 또는
        // ExecuteItem 복귀 후 처리 (CFiber 자체는 통보만)
        pSelf->m_eState = EFiberState::Free;
        ::SwitchToFiber(pSelf->m_hReturnFiber);
        // 다음 SwitchToFiber(this) 진입 시 여기서 재개
        // (AssignJob 으로 새 job 이 배정된 상태)
    }
}
```

### 2-4. `Engine/Public/Core/Fiber/FiberPool.h` (신규)

```cpp
#pragma once

// ─────────────────────────────────────────────────────────────
// CFiberPool — 미리 할당된 CFiber 128개의 풀.
//
// Acquire: free 스택 LIFO pop (캐시 친화 — 최근 반환 fiber 재사용).
// Release: free 스택 push (Reset 호출 후).
// 동기화: spinlock — critical section 이 O(1) 이라 mutex 보다 짧음.
//
// ★ FIBER_POOL_SIZE=128 + 64KB stack = 8MB 메모리 (커밋은 lazy).
// ─────────────────────────────────────────────────────────────

#include "Core/Fiber/Fiber.h"
#include <array>
#include <atomic>

class CJobSystem;

class CFiberPool
{
public:
    CFiberPool() = default;
    ~CFiberPool() = default;

    CFiberPool(const CFiberPool&) = delete;
    CFiberPool& operator=(const CFiberPool&) = delete;

    // 풀 초기화 — FIBER_POOL_SIZE 개 fiber 일괄 Create
    bool Initialize(CJobSystem* pOwner);
    // 모든 fiber Destroy. ★ 모든 worker 종료 후에만 호출
    void Shutdown();

    // free fiber idx 반환. 풀 고갈 시 UINT32_MAX
    std::uint32_t Acquire();
    // 반환 — Reset + free 스택 push
    void          Release(std::uint32_t iIndex);

    // 인덱스 → Fiber 참조 (lock 없음 — caller 가 lifecycle 책임)
    CFiber&       GetFiber(std::uint32_t i)       { return m_arrFibers[i]; }
    const CFiber& GetFiber(std::uint32_t i) const { return m_arrFibers[i]; }

    std::uint32_t GetPoolSize() const { return FIBER_POOL_SIZE; }

private:
    void Lock();
    void Unlock();

private:
    std::array<CFiber, FIBER_POOL_SIZE>        m_arrFibers;
    std::array<std::uint32_t, FIBER_POOL_SIZE> m_arrFreeStack;
    std::uint32_t                              m_iFreeTop = 0;
    std::atomic<bool>                          m_bSpinLock{ false };
};
```

### 2-5. `Engine/Private/Core/Fiber/FiberPool.cpp` (신규)

```cpp
#include "WintersPCH.h"
#include "Core/Fiber/FiberPool.h"
#include <intrin.h>  // _mm_pause

bool CFiberPool::Initialize(CJobSystem* pOwner)
{
    for (std::uint32_t i = 0; i < FIBER_POOL_SIZE; ++i)
    {
        if (!m_arrFibers[i].Create(i, pOwner))
        {
            // 일부 실패 시 전부 Destroy 후 fail
            Shutdown();
            return false;
        }
        m_arrFreeStack[i] = i;
    }
    m_iFreeTop = FIBER_POOL_SIZE;

#ifdef _DEBUG
    char buf[128];
    ::sprintf_s(buf, "[CFiberPool] Initialized %u fibers (stack=%uKB each, total=%uKB)\n",
                FIBER_POOL_SIZE, FIBER_STACK_SIZE / 1024,
                (FIBER_POOL_SIZE * FIBER_STACK_SIZE) / 1024);
    ::OutputDebugStringA(buf);
#endif

    return true;
}

void CFiberPool::Shutdown()
{
    for (auto& fb : m_arrFibers)
        fb.Destroy();
    m_iFreeTop = 0;
}

std::uint32_t CFiberPool::Acquire()
{
    Lock();
    if (m_iFreeTop == 0)
    {
        Unlock();
#ifdef _DEBUG
        ::OutputDebugStringA("[CFiberPool] Acquire FAILED — pool exhausted\n");
#endif
        return UINT32_MAX;
    }
    --m_iFreeTop;
    std::uint32_t idx = m_arrFreeStack[m_iFreeTop];
    Unlock();
    return idx;
}

void CFiberPool::Release(std::uint32_t iIndex)
{
    m_arrFibers[iIndex].Reset();

    Lock();
    m_arrFreeStack[m_iFreeTop] = iIndex;
    ++m_iFreeTop;
    Unlock();
}

void CFiberPool::Lock()
{
    // test-and-test-and-set spinlock + PAUSE 힌트
    while (m_bSpinLock.exchange(true, std::memory_order_acquire))
    {
        do
        {
            _mm_pause();
        }
        while (m_bSpinLock.load(std::memory_order_relaxed));
    }
}

void CFiberPool::Unlock()
{
    m_bSpinLock.store(false, std::memory_order_release);
}
```

---

## §3. 수정 파일 (수정 전/후 cpp 블록) — ★ v2.1: JobCounter 변경 0

### 3-1. `Engine/Public/Core/JobCounter.h` — ★ v2.1 변경 없음 (Codex #4)

**v2 v1 안 폐기**: JobCounter 에 wait list 멤버 5개 + `TryAddWaitingFiber` / `FlushWaitList` 메서드 추가 안.

**폐기 이유** (Codex #4):
- CJobCounter 는 **stack 변수** 로 사용 — SystemScheduler / TransformSystem / MinionAISystem / NavigationSystem 모두 local 변수
- counter destroy 시점 vs waiter 등록 시점 race — caller 가 wait 누락 시 dangling pointer
- counter 안의 mutex 와 worker 완료 람다의 mutex 가 같은 lock — contention

**v2.1 정정**: Wait list 는 [§3-3](#3-3) `CJobSystem::m_mapWaiters` (`std::unordered_map<CJobCounter*, CounterWaitState>`) 로 이전. CJobCounter 는 atomic 카운트만 유지 (현행 그대로).

→ **JobCounter.h 수정 — 없음**.
→ **JobCounter.cpp 신규 — 없음**.

(현 [Engine/Public/Core/JobCounter.h:1-46](Engine/Public/Core/JobCounter.h:1) 그대로 유지)

### 3-2. (폐기 — JobCounter.cpp 신규 안 함)

v2 v1 안의 `JobCounter.cpp` 신규 작성 — Codex #4 검토로 폐기. 본 섹션 비움.

### 3-3. `Engine/Public/Core/JobSystem.h` — Fiber 모드 멤버 + per-worker 컨텍스트 + waiter map

**수정 전** (현재 [Engine/Public/Core/JobSystem.h:1-79](Engine/Public/Core/JobSystem.h:1)):
```cpp
#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <queue>

#include "Core/JobSystem/WorkStealingDeque.h"
#include "Core/JobSystem/JobDecl.h"

class CJobCounter;

class CJobSystem
{
public:
    CJobSystem();
    ~CJobSystem();
    CJobSystem(const CJobSystem&) = delete;
    CJobSystem& operator=(const CJobSystem&) = delete;

    void Initialize(std::uint32_t iWorkerCount = 0);
    void Shutdown();

    void Submit(std::function<void()> job);
    void Submit(std::function<void()> job, CJobCounter* pCounter);
    void Submit(const JobDecl& decl, CJobCounter* pCounter = nullptr);

    void WaitForCounter(CJobCounter* pCounter, std::uint32_t iTarget = 0);

    std::uint32_t GetWorkerCount() const
    {
        return static_cast<std::uint32_t>(m_vecWorkers.size());
    }

    static std::int32_t  Get_WorkerIdx();
    static std::uint32_t Get_WorkerSlot();

private:
    struct WorkItem
    {
        std::function<void()> fn;
        CJobCounter* pCounter = nullptr;
    };

    void EnqueueJob(WorkItem&& item);
    void  WorkerLoop(std::uint32_t iWorkerIdx);
    bool  TryExecuteOneJob(std::uint32_t iWorkerIdx);
    void  ExecuteItem(WorkItem& item);
    void  PushToSomeDeque(WorkItem&& item);
    std::uint32_t PickVictim(std::uint32_t iSelf, std::uint32_t N);

    std::vector<std::thread>   m_vecWorkers;
    std::vector<std::unique_ptr<CWorkStealingDeque<WorkItem>>>  m_vecDeques;
    std::atomic<bool> m_bShutdown{ false };
    std::atomic<std::uint32_t> m_iRoundRobin{ 0 };

    std::mutex m_GlobalMutex;
    std::queue<WorkItem> m_GlobalQueue;
};
```

**수정 후** (Fiber 멤버 + EMode + WorkerContext + Initialize 시그니처 확장 + ★ v2.1: waiter map):
```cpp
#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <queue>
#include <unordered_map>          // ★ v2.1 — m_mapWaiters

#include "Core/JobSystem/WorkStealingDeque.h"
#include "Core/JobSystem/JobDecl.h"
#include "Core/Fiber/FiberTypes.h"   // FiberHandle, EFiberState

class CJobCounter;
class CFiberPool;                    // M2 이후 — 전방선언

// ─────────────────────────────────────────────────────────────
// CJobSystem — Phase 5-A MVP + 5-B Fiber 통합
//  - public API 불변 (Submit/WaitForCounter/Initialize)
//  - 내부 동작은 m_eMode 로 4단계 분기:
//      ThreadOnly  (M0) — 5-A 그대로
//      FiberShell  (M1) — ConvertThreadToFiber + 매 job CreateFiber (검증용)
//      FiberPool   (M2) — Pool Acquire/Release
//      FiberFull   (M3) — + WaitForCounter yield + wait list resume
//  - ★ v2.1 (Codex #4): Wait list 는 JobSystem 내부 unordered_map 으로 관리
//      (CJobCounter 는 stack 변수 — counter 안에 wait list 두면 dangling 위험)
// ─────────────────────────────────────────────────────────────
class CJobSystem
{
public:
    enum class EMode : std::uint8_t
    {
        ThreadOnly = 0,
        FiberShell = 1,
        FiberPool  = 2,
        FiberFull  = 3,
    };

    CJobSystem();
    ~CJobSystem();
    CJobSystem(const CJobSystem&) = delete;
    CJobSystem& operator=(const CJobSystem&) = delete;

    // ★ 시그니처 확장 — eMode 추가 (default = ThreadOnly, 기존 호출부 호환)
    void Initialize(std::uint32_t iWorkerCount = 0,
                    EMode         eMode        = EMode::ThreadOnly);
    void Shutdown();

    EMode GetMode() const { return m_eMode; }

    void Submit(std::function<void()> job);
    void Submit(std::function<void()> job, CJobCounter* pCounter);
    void Submit(const JobDecl& decl, CJobCounter* pCounter = nullptr);

    void WaitForCounter(CJobCounter* pCounter, std::uint32_t iTarget = 0);

    std::uint32_t GetWorkerCount() const
    {
        return static_cast<std::uint32_t>(m_vecWorkers.size());
    }

    static std::int32_t  Get_WorkerIdx();
    static std::uint32_t Get_WorkerSlot();

private:
    struct WorkItem
    {
        std::function<void()> fn;
        CJobCounter*          pCounter = nullptr;
    };

    // ★ M1+ — Worker 별 fiber 컨텍스트 (per-thread, m_vecWorkers 와 1:1)
    struct WorkerContext
    {
        FiberHandle   hThreadFiber  = nullptr;     // ConvertThreadToFiber 결과
        std::uint32_t iCurrentFiber = UINT32_MAX;  // 현재 worker 가 SwitchTo 한 fiber 인덱스
    };

    // ★ v2.1 — Counter 별 wait list (CJobSystem 이 lifetime 책임)
    //   key = CJobCounter* (caller 가 stack/heap 어디 두든 주소만 키로 사용)
    //   value = wait 중인 fiber idx 들 + target 값
    //   ★ Counter destroy 안전성: WaitForCounter 정상 return = entry erase 된 상태
    struct CounterWaitState
    {
        std::vector<std::uint32_t> vecWaitFibers;
        std::uint32_t              iTarget = 0;
    };

    void          EnqueueJob(WorkItem&& item);
    void          WorkerLoop(std::uint32_t iWorkerIdx);
    bool          TryExecuteOneJob(std::uint32_t iWorkerIdx);
    void          ExecuteItem(WorkItem& item);
    void          PushToSomeDeque(WorkItem&& item);
    std::uint32_t PickVictim(std::uint32_t iSelf, std::uint32_t N);

    // ── M3 Fiber 헬퍼 ────────────────────────────────────────
    // WaitForCounter 가 worker fiber 컨텍스트면 호출 — fiber yield 후 thread fiber 로 복귀
    void Fiber_YieldToCounter(CJobCounter* pCounter, std::uint32_t iTarget);
    // ★ v2.1 — JobSystem 의 m_mapWaiters 에 등록 시도 (counter 직접 접근 X)
    bool Fiber_TryRegisterWait(CJobCounter* pCounter, std::uint32_t iTarget,
                               std::uint32_t iFiberIdx);
    // ExecuteItem wrap 람다가 Decrement 직후 호출 — m_mapWaiters 에서 flush → ready 큐
    void Fiber_NotifyCounterComplete(CJobCounter* pCounter);
    // 워커가 ready 큐에서 fiber 픽업해 SwitchTo
    bool Fiber_TryResumeOne(std::uint32_t iWorkerIdx);

private:
    std::vector<std::thread>                                   m_vecWorkers;
    std::vector<std::unique_ptr<CWorkStealingDeque<WorkItem>>> m_vecDeques;
    std::vector<WorkerContext>                                 m_vecWorkerCtx;  // ★ M1+
    std::atomic<bool>                                          m_bShutdown{ false };
    std::atomic<std::uint32_t>                                 m_iRoundRobin{ 0 };

    std::mutex                                                 m_GlobalMutex;
    std::queue<WorkItem>                                       m_GlobalQueue;

    // ── Fiber 모드 (M2+) ────────────────────────────────────
    EMode                                                      m_eMode = EMode::ThreadOnly;
    std::unique_ptr<CFiberPool>                                m_pFiberPool;

    // ── M3 Counter 별 Wait list (★ v2.1 — counter 안 X, JobSystem 안 O) ──
    std::mutex                                                 m_WaiterMutex;
    std::unordered_map<CJobCounter*, CounterWaitState>         m_mapWaiters;

    // ── M3 Ready queue (yield 후 깨어난 fiber 들) ───────────
    std::mutex                                                 m_ReadyMutex;
    std::queue<std::uint32_t>                                  m_ReadyFibers;
};
```

### 3-4. `Engine/Private/Core/JobSystem.cpp` — WorkerLoop / ExecuteItem / WaitForCounter fiber 분기

★ **Phase 5-A 본체 보존 박제 (Codex #2)**: 본 §3-4 의 모든 수정은 [JobSystem.cpp:107-139](Engine/Private/Core/JobSystem.cpp:107) `EnqueueJob` 의 main → global / worker → self deque 분기를 **건드리지 않는다**. Chase-Lev Main-push race 회피는 Phase 5-A 가 이미 완료한 상태로, fiber 도입은 이 위에 얹는다.

**수정 1: Initialize — eMode 인자 + FiberPool 생성**

수정 전 ([JobSystem.cpp:34-62](Engine/Private/Core/JobSystem.cpp:34)):
```cpp
void CJobSystem::Initialize(std::uint32_t iWorkerCount)
{
    if (!m_vecWorkers.empty())
        return;

    if (iWorkerCount == 0)
    {
        const std::uint32_t hc = std::thread::hardware_concurrency();
        iWorkerCount = (hc > 2) ? (hc - 2) : 1u;
    }

    m_vecDeques.clear();
    m_vecDeques.reserve(iWorkerCount);
    for (std::uint32_t i = 0; i < iWorkerCount; ++i)
    {
        m_vecDeques.push_back(std::make_unique<CWorkStealingDeque<WorkItem>>());
    }
    m_bShutdown.store(false, std::memory_order_release);
    m_vecWorkers.reserve(iWorkerCount);
    for (std::uint32_t i = 0; i < iWorkerCount; ++i)
    {
        m_vecWorkers.emplace_back(&CJobSystem::WorkerLoop, this, i);
    }
}
```

수정 후:
```cpp
#include "Core/Fiber/FiberPool.h"  // M2+

void CJobSystem::Initialize(std::uint32_t iWorkerCount, EMode eMode)
{
    if (!m_vecWorkers.empty())
        return;

    m_eMode = eMode;

    if (iWorkerCount == 0)
    {
        const std::uint32_t hc = std::thread::hardware_concurrency();
        iWorkerCount = (hc > 2) ? (hc - 2) : 1u;
    }

    m_vecDeques.clear();
    m_vecDeques.reserve(iWorkerCount);
    for (std::uint32_t i = 0; i < iWorkerCount; ++i)
    {
        m_vecDeques.push_back(std::make_unique<CWorkStealingDeque<WorkItem>>());
    }

    // ★ M1+ — per-worker context
    m_vecWorkerCtx.assign(iWorkerCount, WorkerContext{});

    // ★ M2+ — fiber pool (Worker 시작 전에 모든 fiber Create 완료해야
    //          Worker 가 ConvertThreadToFiber 후 SwitchTo 가능)
    if (m_eMode == EMode::FiberPool || m_eMode == EMode::FiberFull)
    {
        m_pFiberPool = std::make_unique<CFiberPool>();
        if (!m_pFiberPool->Initialize(this))
        {
#ifdef _DEBUG
            ::OutputDebugStringA("[CJobSystem] FiberPool init FAILED — fallback ThreadOnly\n");
#endif
            m_pFiberPool.reset();
            m_eMode = EMode::ThreadOnly;
        }
    }

    m_bShutdown.store(false, std::memory_order_release);
    m_vecWorkers.reserve(iWorkerCount);
    for (std::uint32_t i = 0; i < iWorkerCount; ++i)
    {
        m_vecWorkers.emplace_back(&CJobSystem::WorkerLoop, this, i);
    }
}
```

**수정 2: Shutdown — fiber pool + waiter map 정리 추가**

수정 전 ([JobSystem.cpp:64-77](Engine/Private/Core/JobSystem.cpp:64)):
```cpp
void CJobSystem::Shutdown()
{
    if (m_vecWorkers.empty())
        return;

    m_bShutdown.store(true, std::memory_order_release);
    for (auto& w : m_vecWorkers)
    {
        if (w.joinable())
            w.join();
    }
    m_vecWorkers.clear();
    m_vecDeques.clear();
}
```

수정 후:
```cpp
void CJobSystem::Shutdown()
{
    if (m_vecWorkers.empty())
        return;

    m_bShutdown.store(true, std::memory_order_release);
    for (auto& w : m_vecWorkers)
    {
        if (w.joinable())
            w.join();
    }
    m_vecWorkers.clear();
    m_vecDeques.clear();

    // ★ Worker 모두 종료 후에만 fiber pool 해제
    //   (DeleteFiber 는 현재 실행 중 fiber 에 대해 호출 금지)
    m_vecWorkerCtx.clear();
    if (m_pFiberPool)
    {
        m_pFiberPool->Shutdown();
        m_pFiberPool.reset();
    }

    // ★ v2.1 — waiter map / ready queue 비우기
    {
        std::lock_guard<std::mutex> lk(m_WaiterMutex);
        m_mapWaiters.clear();
    }
    {
        std::lock_guard<std::mutex> lk(m_ReadyMutex);
        std::queue<std::uint32_t> empty;
        std::swap(m_ReadyFibers, empty);
    }

    m_eMode = EMode::ThreadOnly;
}
```

**수정 3: WorkerLoop — ConvertThreadToFiber + ConvertFiberToThread**

수정 전 ([JobSystem.cpp:142-151](Engine/Private/Core/JobSystem.cpp:142)):
```cpp
void CJobSystem::WorkerLoop(std::uint32_t iWorkerIdx)
{
    t_iWorkerIdx = static_cast<std::int32_t>(iWorkerIdx);
    while (!m_bShutdown.load(std::memory_order_acquire))
    {
        if (!TryExecuteOneJob(iWorkerIdx))
            std::this_thread::yield();
    }
    t_iWorkerIdx = -1;
}
```

수정 후:
```cpp
void CJobSystem::WorkerLoop(std::uint32_t iWorkerIdx)
{
    t_iWorkerIdx = static_cast<std::int32_t>(iWorkerIdx);

    // ★ M1+ — thread → fiber 변환. 이후 SwitchToFiber 가능.
    //   m_hThreadFiber 는 이 thread 의 "원래 컨텍스트" — 모든 fiber 가
    //   완료/yield 시 여기로 복귀.
    //   ★ Codex #5: main thread 는 fiber 화 X (이 함수는 worker thread 만 진입)
    if (m_eMode != EMode::ThreadOnly)
    {
        m_vecWorkerCtx[iWorkerIdx].hThreadFiber = ::ConvertThreadToFiber(nullptr);
#ifdef _DEBUG
        if (!m_vecWorkerCtx[iWorkerIdx].hThreadFiber)
        {
            char buf[128];
            ::sprintf_s(buf, "[CJobSystem] ConvertThreadToFiber FAILED w=%u err=%lu\n",
                        iWorkerIdx, ::GetLastError());
            ::OutputDebugStringA(buf);
        }
#endif
    }

    while (!m_bShutdown.load(std::memory_order_acquire))
    {
        if (!TryExecuteOneJob(iWorkerIdx))
            std::this_thread::yield();
    }

    // ★ Worker 종료 — fiber 모드면 thread 로 복귀 (정리)
    if (m_eMode != EMode::ThreadOnly && m_vecWorkerCtx[iWorkerIdx].hThreadFiber)
    {
        ::ConvertFiberToThread();
        m_vecWorkerCtx[iWorkerIdx].hThreadFiber = nullptr;
    }

    t_iWorkerIdx = -1;
}
```

**수정 4: TryExecuteOneJob — ready fiber 우선 (M3)**

수정 전 ([JobSystem.cpp:167-204](Engine/Private/Core/JobSystem.cpp:167)):
```cpp
bool CJobSystem::TryExecuteOneJob(std::uint32_t iWorkerIdx)
{
    WorkItem item;
    if (m_vecDeques[iWorkerIdx]->Pop(item))
    {
        ExecuteItem(item);
        return true;
    }
    bool hasGlobal = false;
    {
        std::lock_guard<std::mutex> lk(m_GlobalMutex);
        if (!m_GlobalQueue.empty())
        {
            item = std::move(m_GlobalQueue.front());
            m_GlobalQueue.pop();
            hasGlobal = true;
        }
    }
    if (hasGlobal)
    {
        ExecuteItem(item);
        return true;
    }
    const std::uint32_t N = static_cast<std::uint32_t>(m_vecDeques.size());
    if (N > 1)
    {
        const std::uint32_t victim = PickVictim(iWorkerIdx, N);
        if (m_vecDeques[victim]->Steal(item))
        {
            ExecuteItem(item);
            return true;
        }
    }
    return false;
}
```

수정 후:
```cpp
bool CJobSystem::TryExecuteOneJob(std::uint32_t iWorkerIdx)
{
    // ★ M3 — Ready fiber 우선 (yield 됐다 깨어난 fiber 가 빨리 진행)
    if (m_eMode == EMode::FiberFull)
    {
        if (Fiber_TryResumeOne(iWorkerIdx))
            return true;
    }

    WorkItem item;

    // 1) 자기 Deque (LIFO) — Phase 5-A Chase-Lev 보존
    if (m_vecDeques[iWorkerIdx]->Pop(item))
    {
        ExecuteItem(item);
        return true;
    }

    // 2) Global queue — Phase 5-A main-push race 회피 보존
    bool hasGlobal = false;
    {
        std::lock_guard<std::mutex> lk(m_GlobalMutex);
        if (!m_GlobalQueue.empty())
        {
            item = std::move(m_GlobalQueue.front());
            m_GlobalQueue.pop();
            hasGlobal = true;
        }
    }
    if (hasGlobal)
    {
        ExecuteItem(item);
        return true;
    }

    // 3) Steal
    const std::uint32_t N = static_cast<std::uint32_t>(m_vecDeques.size());
    if (N > 1)
    {
        const std::uint32_t victim = PickVictim(iWorkerIdx, N);
        if (m_vecDeques[victim]->Steal(item))
        {
            ExecuteItem(item);
            return true;
        }
    }
    return false;
}
```

**수정 5: ExecuteItem — 모드별 분기 + ★ v2.1: 람다 wrap 에서 m_mapWaiters notify**

수정 전 ([JobSystem.cpp:206-212](Engine/Private/Core/JobSystem.cpp:206)):
```cpp
void CJobSystem::ExecuteItem(WorkItem& item)
{
    if (item.fn)
        item.fn();
    if (item.pCounter)
        item.pCounter->Decrement();
}
```

수정 후:
```cpp
void CJobSystem::ExecuteItem(WorkItem& item)
{
    // ── ThreadOnly / FiberShell / 외부 thread (t_iWorkerIdx<0)
    //    — inline 실행 (기존 5-A 동작)
    //    FiberShell 도 검증용으로 inline (Switch 까지 통과는 별도 sanity job 으로)
    const std::int32_t wi = t_iWorkerIdx;
    const bool bUsePool   = (m_eMode == EMode::FiberPool ||
                             m_eMode == EMode::FiberFull) &&
                             (wi >= 0) && m_pFiberPool;

    if (!bUsePool)
    {
        if (item.fn) item.fn();
        if (item.pCounter)
        {
            item.pCounter->Decrement();
            // ★ ThreadOnly/Shell 모드는 wait list 자체 없음 → notify 불필요
        }
        return;
    }

    // ── M2+ FiberPool/FiberFull — pool 에서 fiber 빌려서 실행
    auto& ctx = m_vecWorkerCtx[wi];

    const std::uint32_t iFiberIdx = m_pFiberPool->Acquire();
    if (iFiberIdx == UINT32_MAX)
    {
        // 풀 고갈 — inline fallback
        if (item.fn) item.fn();
        if (item.pCounter)
        {
            item.pCounter->Decrement();
            if (m_eMode == EMode::FiberFull)
                Fiber_NotifyCounterComplete(item.pCounter);
        }
        return;
    }

    auto& fiber = m_pFiberPool->GetFiber(iFiberIdx);

    // ★ Counter Decrement + waiter notify 를 fiber 안에서 수행
    //   (fiber 가 yield 후 깨어나 마지막 줄 도달했을 때 notify 가 일어나야
    //    sibling worker 의 wait 도 해소됨)
    auto      pCounter = item.pCounter;
    auto      fnLocal  = std::move(item.fn);
    CJobSystem* pSelf  = this;
    fiber.AssignJob([pSelf, pCounter, fn = std::move(fnLocal)]() mutable
    {
        if (fn) fn();
        if (pCounter)
        {
            pCounter->Decrement();
            // ★ v2.1 — JobSystem 의 m_mapWaiters 에서 깨우기 (counter 안 X)
            if (pSelf->m_eMode == EMode::FiberFull)
                pSelf->Fiber_NotifyCounterComplete(pCounter);
        }
    });
    fiber.SetReturnFiber(ctx.hThreadFiber);
    ctx.iCurrentFiber = iFiberIdx;

    // SwitchTo — fiber 가 완료 또는 yield 시 여기로 복귀
    ::SwitchToFiber(fiber.GetHandle());

    // 복귀 — Free (완료) 또는 Waiting (yield)
    if (fiber.GetState() == EFiberState::Free)
    {
        m_pFiberPool->Release(iFiberIdx);
    }
    // Waiting 이면 fiber 는 wait list 에 등록된 상태 — 그대로 둠
    // (Fiber_NotifyCounterComplete 가 ready 큐로 이동시킬 때까지)
    ctx.iCurrentFiber = UINT32_MAX;
}
```

**수정 6: WaitForCounter — fiber yield 분기 (Codex #5: main 은 기존 그대로)**

수정 전 ([JobSystem.cpp:215-269](Engine/Private/Core/JobSystem.cpp:215)):
```cpp
void CJobSystem::WaitForCounter(CJobCounter* pCounter, std::uint32_t iTarget)
{
    if (!pCounter)
        return;

    while (pCounter->Load() > iTarget)
    {
        bool bDidWork = false;

        if (t_iWorkerIdx >= 0)
        {
            bDidWork = TryExecuteOneJob(static_cast<std::uint32_t>(t_iWorkerIdx));
        }
        else
        {
            // 외부 — global drain → steal (기존 코드)
            // ...
        }
        if (!bDidWork)
            std::this_thread::yield();
    }
}
```

수정 후:
```cpp
void CJobSystem::WaitForCounter(CJobCounter* pCounter, std::uint32_t iTarget)
{
    if (!pCounter)
        return;

    // ★ M3 — fiber yield 분기 (worker thread + fiber 진행 중일 때만)
    //   조건: FiberFull 모드 + worker thread (t_iWorkerIdx>=0) + 현재 fiber 진행
    //   ★ Codex #5: main thread (t_iWorkerIdx==-1) 는 기존 help-stealing 유지
    if (m_eMode == EMode::FiberFull && t_iWorkerIdx >= 0)
    {
        Fiber_YieldToCounter(pCounter, iTarget);
        return;
    }

    // ── M0/M1/M2 또는 외부 thread (main 포함) — 기존 help-stealing
    while (pCounter->Load() > iTarget)
    {
        bool bDidWork = false;

        if (t_iWorkerIdx >= 0)
        {
            bDidWork = TryExecuteOneJob(static_cast<std::uint32_t>(t_iWorkerIdx));
        }
        else
        {
            // 외부 (main 포함) — global drain → steal. ★ Phase 5-A 보존
            WorkItem item;
            bool hasGlobal = false;
            {
                std::lock_guard<std::mutex> lk(m_GlobalMutex);
                if (!m_GlobalQueue.empty())
                {
                    item = std::move(m_GlobalQueue.front());
                    m_GlobalQueue.pop();
                    hasGlobal = true;
                }
            }
            if (hasGlobal)
            {
                ExecuteItem(item);
                bDidWork = true;
            }
            else
            {
                const std::uint32_t N = static_cast<std::uint32_t>(m_vecDeques.size());
                if (N > 0)
                {
                    const std::uint32_t victim =
                        m_iRoundRobin.fetch_add(1, std::memory_order_relaxed) % N;
                    WorkItem stolen;
                    if (m_vecDeques[victim]->Steal(stolen))
                    {
                        ExecuteItem(stolen);
                        bDidWork = true;
                    }
                }
            }
        }

        if (!bDidWork)
            std::this_thread::yield();
    }
}
```

**수정 7: Fiber 헬퍼 4 메서드 신규 (★ v2.1 — Fiber_TryRegisterWait 신규)**

```cpp
// ── M3 ─────────────────────────────────────────────────────
// 현재 fiber 를 m_mapWaiters 에 등록하고 thread fiber 로 복귀.
// 깨어나면 (다른 worker 가 SwitchTo 함) 이 함수의 SwitchToFiber 다음 줄에서 재개.
void CJobSystem::Fiber_YieldToCounter(CJobCounter* pCounter,
                                      std::uint32_t iTarget)
{
    const std::int32_t wi = t_iWorkerIdx;
    if (wi < 0 || !m_pFiberPool)
    {
        // 안전망 — 외부 thread 호출 시 busy-wait 으로 폴백
        while (pCounter->Load() > iTarget)
            std::this_thread::yield();
        return;
    }

    auto& ctx = m_vecWorkerCtx[wi];
    const std::uint32_t cur = ctx.iCurrentFiber;
    if (cur == UINT32_MAX)
    {
        // fiber 컨텍스트 아님 — busy-wait
        while (pCounter->Load() > iTarget)
            std::this_thread::yield();
        return;
    }

    // ★ v2.1 — JobSystem 내부 wait map 등록 (counter 직접 호출 X)
    if (!Fiber_TryRegisterWait(pCounter, iTarget, cur))
        return;  // 이미 target 도달 또는 등록 실패 — 즉시 진행

    // 등록 성공 — yield
    auto& fb = m_pFiberPool->GetFiber(cur);
    fb.SetState(EFiberState::Waiting);
    // ★ thread fiber 로 복귀 — worker 가 다음 job 픽업
    ::SwitchToFiber(ctx.hThreadFiber);

    // 깨어남 — counter 가 target 도달, 다른 worker 가 ready 큐에서 SwitchTo 함
    // (이 worker 는 같을 수도 다를 수도 있음 — Get_WorkerSlot 함정 §4-1 참고)
    fb.SetState(EFiberState::Running);
}

// ── M3 (★ v2.1 신규) ──────────────────────────────────────
// CJobSystem 내부 m_mapWaiters 에 등록 시도.
//  반환 true  — 등록 완료, 호출자가 yield 해야 함
//  반환 false — 이미 target 도달 또는 wait list full → 즉시 진행
bool CJobSystem::Fiber_TryRegisterWait(CJobCounter* pCounter,
                                       std::uint32_t iTarget,
                                       std::uint32_t iFiberIdx)
{
    if (!pCounter)
        return false;

    std::lock_guard<std::mutex> lk(m_WaiterMutex);

    // ★ register 직전 재확인 (race 방지) — counter 가 막 0 됐을 수 있음
    //   재확인 안 하면: load > target 으로 진입 → wait map 등록 → notify 가
    //   먼저 끝나서 이 fiber 만 영원히 깨우지 못함
    if (pCounter->Load() <= iTarget)
        return false;

    auto& st = m_mapWaiters[pCounter];
    st.iTarget = iTarget;

    // soft limit — vector 라 늘릴 수는 있지만 폭주 방지
    if (st.vecWaitFibers.size() >= MAX_WAIT_FIBERS_PER_COUNTER)
    {
#ifdef _DEBUG
        ::OutputDebugStringA("[CJobSystem] Wait list FULL — fallback busy-wait\n");
#endif
        return false;
    }

    st.vecWaitFibers.push_back(iFiberIdx);
    return true;
}

// ── M3 (★ v2.1 — m_mapWaiters 에서 깨움) ─────────────────
// ExecuteItem 의 wrap 람다가 Decrement 직후 호출.
// counter 가 target 에 도달했으면 wait list 의 fiber 들을 ready 큐로 이동.
void CJobSystem::Fiber_NotifyCounterComplete(CJobCounter* pCounter)
{
    if (!pCounter)
        return;

    std::vector<std::uint32_t> notify;
    {
        std::lock_guard<std::mutex> lk(m_WaiterMutex);
        auto it = m_mapWaiters.find(pCounter);
        if (it == m_mapWaiters.end())
            return;  // wait 등록된 fiber 없음

        // counter 가 target 미도달이면 깨우면 안 됨
        if (pCounter->Load() > it->second.iTarget)
            return;

        notify = std::move(it->second.vecWaitFibers);
        m_mapWaiters.erase(it);  // ★ entry 비우고 erase — counter destroy 안전
    }

    if (notify.empty())
        return;

    {
        std::lock_guard<std::mutex> lk(m_ReadyMutex);
        for (std::uint32_t idx : notify)
        {
            m_pFiberPool->GetFiber(idx).SetState(EFiberState::Ready);
            m_ReadyFibers.push(idx);
        }
    }
}

// ── M3 ─────────────────────────────────────────────────────
// Worker 가 ready 큐에서 fiber 픽업해 SwitchTo. 복귀 후 상태별 처리.
bool CJobSystem::Fiber_TryResumeOne(std::uint32_t iWorkerIdx)
{
    std::uint32_t resumeIdx = UINT32_MAX;
    {
        std::lock_guard<std::mutex> lk(m_ReadyMutex);
        if (!m_ReadyFibers.empty())
        {
            resumeIdx = m_ReadyFibers.front();
            m_ReadyFibers.pop();
        }
    }
    if (resumeIdx == UINT32_MAX)
        return false;

    auto& ctx = m_vecWorkerCtx[iWorkerIdx];
    auto& fb  = m_pFiberPool->GetFiber(resumeIdx);

    // ★ Resume 시 return fiber 갱신 — 다른 worker 에서 yield 됐을 수 있음
    fb.SetReturnFiber(ctx.hThreadFiber);
    ctx.iCurrentFiber = resumeIdx;

    ::SwitchToFiber(fb.GetHandle());

    // 복귀 — Free (완료) 또는 Waiting (재 yield)
    if (fb.GetState() == EFiberState::Free)
        m_pFiberPool->Release(resumeIdx);

    ctx.iCurrentFiber = UINT32_MAX;
    return true;
}
```

### 3-5. `Engine/Include/Engine.vcxproj` + `.filters` — ★ v2.1: JobCounter.cpp 제거

**vcxproj 추가** (`<ItemGroup>` 적절 위치):
```xml
<ClInclude Include="..\Public\Core\Fiber\FiberTypes.h" />
<ClInclude Include="..\Public\Core\Fiber\Fiber.h" />
<ClInclude Include="..\Public\Core\Fiber\FiberPool.h" />
<ClCompile Include="..\Private\Core\Fiber\Fiber.cpp" />
<ClCompile Include="..\Private\Core\Fiber\FiberPool.cpp" />
<!-- ★ v2.1 (Codex #4): JobCounter.cpp 신규 X. JobCounter.h 변경 0 → cpp 도 불필요 -->
```

**vcxproj.filters 추가** (CLAUDE.md Engine 필터 표 §10. JobSystem):
```xml
<ClInclude Include="..\Public\Core\Fiber\FiberTypes.h"> <Filter>10. JobSystem</Filter> </ClInclude>
<ClInclude Include="..\Public\Core\Fiber\Fiber.h">      <Filter>10. JobSystem</Filter> </ClInclude>
<ClInclude Include="..\Public\Core\Fiber\FiberPool.h">  <Filter>10. JobSystem</Filter> </ClInclude>
<ClCompile Include="..\Private\Core\Fiber\Fiber.cpp">     <Filter>10. JobSystem</Filter> </ClCompile>
<ClCompile Include="..\Private\Core\Fiber\FiberPool.cpp"> <Filter>10. JobSystem</Filter> </ClCompile>
```

**AdditionalIncludeDirectories** — `Engine/Public/Core/Fiber` 서브폴더 추가:
```xml
<AdditionalIncludeDirectories>...;..\Public\Core\Fiber;...</AdditionalIncludeDirectories>
```

**UpdateLib.bat 검증**: Engine/Public 의 모든 `.h` 가 EngineSDK/inc 로 flat 복사. CLAUDE.md gotcha (B-6.6) — `FiberTypes.h`/`Fiber.h`/`FiberPool.h` 가 SDK 에 flat 으로 들어가는데, **Client 는 fiber 헤더 직접 include 하지 않음** (Engine 내부 디테일). 단 SDK 복사 자체는 무해.

---

## §4. WORKER_SAFETY_PACKAGE v3 와의 정합 (★ v1 누락 — 5건)

### 4-1. `Get_WorkerSlot()` fiber resume 안전성 — ★ Codex #3 결정 박제

**문제**: M3 에서 fiber A 가 worker 0 에서 시작 → yield → wait list → counter 도달 → worker 3 이 ready 큐에서 픽업 → fiber A 가 worker 3 에서 resume.

이 시점에 fiber A 의 코드가 `CJobSystem::Get_WorkerSlot()` 호출하면 **worker 3 의 slot (= 4) 반환**. yield 전에는 worker 0 의 slot (= 1) 이었음.

**옵션 비교**:

| 옵션 | 동작 | 장점 | 단점 |
|---|---|---|---|
| **A (현행 유지)** | thread idx + 1 반환 (`t_iWorkerIdx`) | 단순, 기존 호출부 변경 0, 다른 thread_local (CPUProfiler) 와 의미 일치 | yield 전후 slot 다름 → caller 가 push 사이 yield 금지 보장 필요 |
| **B (fiber-local)** | Fiber 가 자기 origin worker idx 보관, fiber 컨텍스트 안에서는 origin 반환 | yield 후에도 같은 slot, push 패턴 안전 | Fiber 안 코드와 외부 thread 코드의 의미 달라짐, 디버깅 복잡, CFiber 가 thread_local 흉내, CPUProfiler `t_vProfilerStack` 등 다른 thread_local 자원과 의미 충돌 |

**★ v2.1 결정 — 옵션 A (현행 thread 기반 유지)**:

이유:
- **의미 일관성**: `t_iWorkerIdx` 는 "현재 thread" 의 식별자. fiber 가 다른 worker 에서 resume 됐다면 현재 thread 가 진짜 바뀐 것. CPUProfiler `t_vProfilerStack` 도 같은 의미 (resume 시 새 worker 의 stack 이 보임)
- **CPUProfiler 와의 통합**: 옵션 B 채택 시 Get_WorkerSlot 만 fiber-local 이고 CPUProfiler 는 thread-local 이 되어 의미가 분기됨. 디버깅 시 "이 fiber 의 slot 은 1 인데 profiler stack 은 worker 3 의 것" 같은 모순
- **실제 위험 패턴은 좁음**: WORKER_SAFETY_PACKAGE v3 의 push 함수 (`Push_Decision` 등) 가 yield 가능 호출을 전혀 안 하므로 안전. yield 가 끼는 함수 안에서만 slot 캐시 금지하면 됨

**강제 검증**: WORKER_SAFETY_PACKAGE v3 의 모든 `Get_WorkerSlot()` 호출 사이트 (Push_Decision / Push_DamageEvent / CommandBuffer Defer*) 에서 **호출 ↔ 사용 사이에 WaitForCounter / Submit / 큰 함수 호출 없음** 코드 리뷰. 새 호출 사이트 추가 시 동일 검증.

**위험 패턴 (★ §6 gotcha 박제)**:
```cpp
// ❌ 위험 — yield 전후 slot 캐시
uint32_t slot = CJobSystem::Get_WorkerSlot();  // worker 0 → slot 1
m_pJobSystem->WaitForCounter(&counter);        // ★ yield 가능 — 깨어나면 worker 3
m_vecBuffer[slot].push_back(...);              // worker 0 의 slot 1 에 push
                                               // 단 현재 실제 thread 는 worker 3 → race
```

→ "WaitForCounter 를 호출하는 함수" 안에서는 slot 을 캐시하지 말고 **매번 재호출** 하거나, yield 전에 buffer 를 flush.

→ §6 gotcha 박제 의무.

### 4-2. Scheduler component access contract 와의 상호작용

v3 의 `ISystem::Get_AccessContract` + `Group_NonConflicting` 은 Scheduler 가 같은 phase 의 시스템들을 충돌 없는 그룹으로 묶어 병렬 Submit. fiber 모드에서도 Submit 단위는 동일 — system 들이 fiber 에서 실행돼도 같은 `m_eMode` 컨텍스트 안에 있고 Scheduler 의 그룹화는 영향받지 않음.

**단**: fiber 가 system Execute 안에서 yield 하면 — 해당 phase 의 다른 그룹이 먼저 진행될 수 있음. 현재 Scheduler 는 phase 단위 직렬화 + 그룹 단위 병렬화이므로 yield 가 phase 경계를 넘지 않으려면 system 코드가 **WaitForCounter 직후 즉시 return** (정상 패턴) 이어야 함. 이미 그러함.

### 4-3. MinionAI Decision/Apply 2-pass 가 fiber 에서 그대로 작동하는가

DecisionPass: worker 에서 read-only — fiber 안에서 실행돼도 read 만 하니 무관.
ApplyPass: main thread single-thread — Scene_InGame::OnUpdate 콜체인 안 → 절대 fiber 컨텍스트 아님 (main thread = `t_iWorkerIdx == -1`).

→ **호환 OK**. 단 ApplyPass 안에서 `WaitForCounter(&someJobCounter)` 호출 시 main thread 라 yield 안 함, 기존 help-stealing — OK.

### 4-4. AStar scope guard

Pathfinder.cpp 의 `WINTERS_PROFILE_COUNT` mutex contention. v3 의 scope guard (RAII 로 함수 끝에서 1회 flush). fiber 가 A* 안에서 yield 하면 — A* 함수가 끝나기 전 yield → guard 의 ~CounterFlush() 가 fiber 의 stack 에서 호출됨. fiber resume 시 다시 같은 stack — guard 살아있음 → return 시점에 정상 flush.

**단**: 같은 fiber 가 여러 번 yield 하면 stack 위에 guard 객체 1개만 존재. 정상.

→ **호환 OK**. fiber 안에서도 RAII scope guard 정상 작동.

### 4-5. CommandBuffer per-worker buffer

v3 의 `m_vecCreatesPerWorker[slot].push_back()` 패턴. §4-1 의 yield 위험과 동일 — `Get_WorkerSlot()` 호출 후 push 사이에 yield 만 없으면 안전. CommandBuffer API 는 push 만 + 즉시 return 패턴 → yield 없음 → 안전.

---

## §5. 검증 매트릭스

### 5-1. 단계별 합격 기준

| 단계 | 검증 항목 | 합격 기준 |
|---|---|---|
| **M0a** | JobSystem stress (2026-05 당시 모든 Set_JobSystem 비활성 조건) | 16 worker × 1000 dummy Submit, race 0, 누수 0 |
| **M0b** | TransformSystem 활성 | 챔프 7체 + 미니언 60체 위치 정확, F5 회귀 0 |
| **M0c** | MinionAISystem 활성 (v3 §2 적용) | 16 미니언 1 타겟 데미지 손실 0 |
| **M0d** | NavigationSystem 활성 (v3 §5 적용) | 16 동시 pathfinding race 0 |
| **M1** | Fiber Shell — ConvertThreadToFiber 통과 | Worker 별 ConvertThreadToFiber 1회만, F5 회귀 0 |
| **M2** | Fiber Pool — Acquire/Release | 1000 Submit 후 누수 0, CreateFiber 호출 = FIBER_POOL_SIZE 만, F5 회귀 0 |
| **M3** | Yield + Wait List | A→B→C 의존 체인 1000회, 모든 wait 정확 깨움, deterministic |
| **M4** | AnimUpdate 병렬화 | Frame 9ms → 7ms, 미니언 60+ 챔프 10 라인전 애니 동기화 정확 |

### 5-2. Stress 시나리오

| # | 시나리오 | 합격 |
|---|---|---|
| S1 | 16 미니언 1 타겟 집중 공격 100회 | 데미지 손실 0 (v3 §2.6 M9) |
| S2 | 16 worker × Submit 의존 그래프 1000회 | wait 정확 깨움, 영원 대기 0 |
| S3 | Get_WorkerSlot fiber resume 100 fiber × 100 yield | per-slot buffer race 0 |
| S4 | FiberPool 고갈 시뮬 (FIBER_POOL_SIZE +1 동시 Submit) | inline fallback 정상, crash 0 |
| S5 | 동시 16 pathfinding (A*) | counter contention 1/노드 → 1/함수 (scope guard 검증) |
| S6 | ★ v2.1 — Counter destroy 안전성 | `CJobCounter c; { Submit×N; WaitForCounter(&c); }` 1000회 반복, m_mapWaiters 누수 0 |

### 5-3. Profiler 카운터 신규 (★ v2.1: WaitMap 카운터 2건 추가)

| 카운터 | 정의 위치 | 의미 |
|---|---|---|
| `Fiber::Acquire` | FiberPool.cpp | pool 에서 fiber 빌린 횟수 |
| `Fiber::AcquireFail` | 동 | 풀 고갈 fallback 횟수 |
| `Fiber::Yield` | JobSystem.cpp Fiber_YieldToCounter | yield 호출 횟수 |
| `Fiber::Resume` | 동 Fiber_TryResumeOne | ready 큐에서 픽업 횟수 |
| `Fiber::CrossWorkerResume` | 동 (yield worker != resume worker 시) | 다른 worker 에서 resume 빈도 |
| **`Fiber::WaitMapInsert`** ★ v2.1 | Fiber_TryRegisterWait 성공 | m_mapWaiters 등록 횟수 |
| **`Fiber::WaitMapHit`** ★ v2.1 | Fiber_NotifyCounterComplete (notify 발생 시) | wait list 깨움 발생 횟수 |

---

## §6. CLAUDE.md gotchas 박제 예정 (★ v2.1: 6건)

```markdown
- **Fiber resume 시 worker 이동 — Get_WorkerSlot 캐시 금지 (Phase 5-B M3)**:
  yield 전후로 worker thread 가 바뀔 수 있음. `auto slot = Get_WorkerSlot();
  WaitForCounter(...); m_buf[slot].push(...)` 패턴은 worker 0 에서 시작 →
  worker 3 에서 깨어나면 worker 0 의 slot 에 worker 3 thread 가 push → race.
  해결: yield 가능한 함수 안에서는 Get_WorkerSlot 을 push 직전마다 재호출.
  push 와 호출 사이 yield 없음을 코드 리뷰로 보장. 결정 (v2.1, Codex #3):
  옵션 A (현행 thread 기반 유지) — fiber-local origin slot 옵션 B 폐기
  (CPUProfiler 와의 의미 일관성 + 위험 패턴 좁음).

- **CFiber::FiberProc 는 절대 return 금지**:
  Win32 CreateFiber 가 만든 fiber 가 return 하면 그 fiber 를 실행 중인 worker
  thread 자체가 종료. 무한 루프 안에서 SwitchToFiber(returnFiber) 로 복귀.
  Job 완료 통보는 람다 wrap (counter Decrement + notify) 로 처리, FiberProc
  자체는 통보 X.

- **DeleteFiber 는 모든 worker 종료 후에만 호출**:
  현재 어떤 worker 가 SwitchTo 한 fiber 를 DeleteFiber 하면 즉시 crash.
  CFiberPool::Shutdown 은 CJobSystem::Shutdown 의 worker join 이후에만 호출.

- **ConvertThreadToFiber 는 Worker 마다 1회만**:
  WorkerLoop 진입 직후 1회. ConvertFiberToThread 는 종료 직전 1회.
  중복 호출 시 GetLastError ERROR_ALREADY_FIBER. main thread 는
  ConvertThreadToFiber 호출 X (fiber 모드 적용 X — Codex #5).

- **Fiber 안에서 thread_local 은 "현재 thread" 의 값**:
  fiber 가 다른 worker 에서 resume 되면 thread_local 값도 바뀜.
  CPUProfiler 의 t_vProfilerStack, JobSystem 의 t_iWorkerIdx 모두 안전 —
  resume 시 새 worker 의 stack/idx 가 보임. 단 Pathfinder 의 tls_gScore 등
  "함수 진입~return 사이 일관성 가정" 패턴은 yield 호출 X 인지 확인 필요.

- **★ v2.1 (Codex #4): CJobCounter 에 wait list 박제 금지**:
  CJobCounter 는 stack 변수로 caller 가 lifetime 책임 (SystemScheduler /
  TransformSystem / MinionAISystem / NavigationSystem 모두 local 변수).
  counter 안에 wait list 멤버를 넣으면 — caller 가 WaitForCounter 누락 시
  destroy 시점에 wait map 의 dangling pointer + counter 안 mutex 와 worker
  완료 람다의 mutex 가 같은 lock → contention.
  해결: wait list 는 CJobSystem 내부 std::unordered_map<CJobCounter*, ...>
  로 관리. WaitForCounter 정상 return = wait map entry 이미 비우고 erase.
  counter destroy 시 entry 없음 보장. CJobCounter 자체는 atomic 카운트만.
```

---

## §7. 일정 (★ v2.1: M3 1.5 → 2.0일, 합계 3.5 → 4.0일)

> 이 표는 2026-05-02 구현 추정치이며 실제 완료 기록이 아니다. 별도의 6주 mastery 프로그램과도 다른 일정 축이다.

| 단계 | 작업 | 시간 |
|---|---|---|
| M0 | WORKER_SAFETY_PACKAGE v3 완료 + Set_JobSystem 단계 활성 (a/b/c/d) | (별도 패키지) |
| M1 | FiberTypes.h + Fiber.h/.cpp + WorkerLoop ConvertThreadToFiber + ExecuteItem FiberShell 분기 | 0.5일 |
| M2 | FiberPool.h/.cpp + Initialize/Shutdown 통합 + ExecuteItem FiberPool 분기 | 0.5일 |
| **M3** | ★ v2.1: JobSystem 내 m_mapWaiters + Fiber_TryRegisterWait / NotifyComplete / TryResumeOne + WaitForCounter 분기 + JobCounter 변경 0 + Counter destroy 안전성 stress | **2.0일** |
| M4 | CAnimationSystem 또는 caller 묶음 Submit + 검증 | 0.5일 |
| 검증 | S1~S6 + 7 챔프 회귀 | 0.5일 |
| **합계** | | **4.0일** (M0 제외) |

v1 견적 7-10일 → v2.1 4.0일 (인프라 70% 재사용 + Naughty Dog 모델 4단계 분할 + Codex 검토 안정성 +0.5일).

---

## §8. 의존성 + 롤백 전략

### 8-1. 의존성 그래프

```
[M0 — 진입 전제 + Set_JobSystem 단계 활성]
  ├─ M0a JobSystem stress (2026-05 당시 모든 Set_JobSystem 비활성 조건)
  ├─ M0b TransformSystem 활성
  ├─ M0c MinionAISystem 활성 (★ WORKER_SAFETY_PACKAGE v3 §2 Decision/Apply 적용 후)
  └─ M0d NavigationSystem 활성 (★ AStar scope guard 적용 후)
       ↓
[M1 — Fiber Shell]
  ├─ Engine/Public/Core/Fiber/FiberTypes.h (신규)
  ├─ Engine/Public/Core/Fiber/Fiber.h + .cpp (신규)
  ├─ JobSystem.h: EMode + WorkerContext + Initialize 시그니처 확장
  ├─ JobSystem.cpp: WorkerLoop 에 ConvertThreadToFiber + ConvertFiberToThread
  └─ vcxproj + filters 등록
       ↓
[M2 — Fiber Pool]
  ├─ Engine/Public/Core/Fiber/FiberPool.h + .cpp (신규)
  ├─ JobSystem.h: m_pFiberPool 멤버
  ├─ JobSystem.cpp: Initialize 에 FiberPool 생성, ExecuteItem FiberPool 분기, Shutdown 정리
  └─ Profiler 카운터 Acquire/AcquireFail
       ↓
[M3 — Yield + Wait List ★ v2.1: JobCounter 변경 0]
  ├─ JobCounter.h/.cpp: ★ 변경 없음 (Codex #4)
  ├─ JobSystem.h: m_mapWaiters + CounterWaitState struct + Fiber_TryRegisterWait 시그니처
  ├─ JobSystem.cpp: ExecuteItem 람다 wrap notify + WaitForCounter fiber 분기
  │                 + Fiber_YieldToCounter / TryRegisterWait / NotifyComplete / TryResumeOne
  ├─ TryExecuteOneJob 에 ready 큐 우선 검사
  └─ Profiler 카운터 Yield/Resume/CrossWorkerResume + WaitMapInsert/Hit
       ↓
[M4 — AnimUpdate 병렬화]
  ├─ CAnimationSystem 신규 또는 caller (Scene_InGame, Minion_Manager) 묶음 Submit
  └─ Frame time -2ms 검증
       ↓
[검증 종합]
  ├─ S1~S6 stress (★ v2.1: S6 Counter destroy 안전성 추가)
  └─ 7 챔프 회귀
```

### 8-2. 롤백 branch 전략

| Branch | 용도 | 롤백 조건 |
|---|---|---|
| `phase5a-final` (tag) | M0 완료 시점 — 모든 단계 진입 전 안전 지점 | M1 이후 어떤 단계든 deadlock 발생 시 즉시 복귀 |
| `feature/phase5b-m1` | M1 PR | F5 회귀 발생 시 phase5a-final 로 reset |
| `feature/phase5b-m2` | M2 PR | 풀 누수 발생 시 m1 로 reset |
| `feature/phase5b-m3` | M3 PR | wait/notify race 발생 시 m2 로 reset (ThreadOnly 로 설정 변경만으로도 fallback 가능) |
| `feature/phase5b-m4` | M4 PR | AnimUpdate 분기 발생 시 caller 의 병렬 Submit 만 제거 |

**중요**: `m_eMode = EMode::ThreadOnly` 로 설정하면 fiber 코드가 모두 비활성 → 빠른 fallback 가능. Initialize 시점에 enum 만 바꾸면 됨. 구조적 롤백 부담 최소.

---

## §9. v2.1 한 줄 요약

**v2.1 — Codex 검토 6건 반영. (1) 경로 ✅ 변경 없음 / (2) Phase 5-A 본체 (main → global, worker → self deque) 보존 박제 / (3) Get_WorkerSlot 옵션 A 결정 (현행 thread 기반 유지 — CPUProfiler 와의 의미 일관성) + push-yield gotcha 강제 / (4) ★ JobCounter 변경 0 — wait list 는 CJobSystem 내부 `std::unordered_map<CJobCounter*, CounterWaitState>` 로 이전 (counter destroy 안전성 + mutex contention 해소) / (5) Main thread fiber 화 X 박제 / (6) Set_JobSystem 단계 활성 4단계 (M0a stress → M0b Transform → M0c MinionAI → M0d Navigation). 신규 5 파일 (FiberTypes/Fiber/FiberPool) + 수정 3 파일 (JobSystem.h/.cpp + vcxproj — JobCounter 제외) + 4단계 (M1~M4) + Worker-Safety v3 정합 §4 + Codex 결정 박제 §0.1 + §4-1 옵션 비교. 4.0일 (M3 +0.5일).**
