# Fiber JobSystem 안정화 계획서 (Phase 5-B-pre)

> [!IMPORTANT]
> **Historical stabilization plan.** 아래 본문을 현재 구현 상태로 사용하지 않는다. 최신 기준은 [2026-07-13 canonical implementation plan](../2026-07-13_UDP_JOB_SYSTEM_CHASE_LEV_FIBER_IMPLEMENTATION_PLAN.md)과 [S023 결과 보고서](../../build/2026-07-13_UDP_JOB_SYSTEM_CHASE_LEV_FIBER_RESULT.md)다.
> As-built delta: JobSystem Submit race, Chase-Lev deque, FiberFull 및 stress 구현은 완료되었고, UDP v3 generic vertical slice와 server hub/client facade가 구현되었다. main F5 통합과 최종 build 상태는 S023 결과 보고서를 따른다. 6주 Fiber mastery 프로그램은 미착수이며, 현재 상태는 production UDP cutover가 아니다.
> 과거 UDP v2 수치인 **24 B header / 10 B fragment header / 1 MiB logical payload**는 historical design이다. 실제 v3 상수는 **40 B header / 16 B fragment header / 1,200 B datagram / 64 KiB logical payload**다.

> **상태 동기화 (2026-07-11)**: 이 문서는 2026-05-06 당시 workspace를 박제한 역사 계획이다. 현재는 Main-push owner race 우회와 Client `Set_JobSystem` 주입이 적용됐고 `CJobSystem`도 DLL export된다. 반면 이 문서가 제안한 stress project, FiberPool, waiter/ready queue, yield/resume는 아직 없다. 옛 줄 번호·worktree·미주입 주장을 직접 적용하지 말고 [2026-07-11 상태 감사](../2026-07-11_JOB_SYSTEM_CHASE_LEV_FIBER_STATE_AUDIT.md)에서 다시 진입한다.

작성일: 2026-05-06 (codex 4차 검토 반영 — 사용자 12 + 4 P1 + 구현성 2건 추가)

계기:
- 5/5 Fiber 헤더 3 + JobSystem.cpp FiberShell 본체 박제 후 codex 2차 검토 → 4 잔여 사고
- 5/6 codex 3차 검토 → 사용자 12 지적 추가 (F-8~F-15)
- 5/6 codex 4차 검토 → 사용자 4 P1 추가 (F-16~F-19) + 구현성 2건 추가 (F-20~F-21) + 라인 변동 (#5) + worktree 정합성 (#6) — 본 v4
- CLAUDE.md §1.A Track 3 의 "박제 0%" 표현이 부정확
- 실제는 FiberShell M0 박제 완료 + CFiberPool / WaitForCounter Yield / Get_WorkerSlot / race / 생명주기 / 소유권 / 컨텍스트 전환 규칙 / **fiber-안 도움실행 금지 / stress 프로젝트 정적 빌드 / leak 검증 순서 / WorkerLoop 슬롯 초기화** 미박제

관계 문서:
- 마스터 계획서: `.md/plan/engine/FIBER_JOB_SYSTEM_v2.md` (Codex 6건 보정, Naughty Dog GDC 2015 모델)
- 학습 가이드: `.md/guide/FIBER_LEARNING_GUIDE.md`
- 박제 함정: `.md/process/PLAN_AUTHORING_PITFALLS.md`

합격 기준:
- Stabilization-0 통과 후 진입
- 1만 job counter stress test 통과 (단순 fan-out)
- Nested wait stress test 통과 (job 안에서 child submit + WaitForCounter)
- fiber 핸들 leak 0 — **검증 순서**: shutdown 전 `acquired == released`, shutdown 후 `created == deleted` (★ Finding 3 / F-18)
- `Scene_InGame.cpp` NavSystem `Set_JobSystem` 주석 복구 정합성 (현재 라인 434~445, **라인 고정 X — `CNavigationSystem::Create` 패턴 검색 우선** ★ Finding 5)

codex 4차 검토 반영 핵심 (사용자 4 P1 + 2 부가 + 구현성 2건 → 본 계획서 매핑):

| # | 사용자 4차 지적 | 등급 | 본 계획서 반영 위치 |
|---|---|---|---|
| Finding 1 | `WaitForCounter` 가 fiber 안에서 도움실행 → nested stress 가 yield/pending 경로 안 밟음. 더 위험한 건 fiber A 가 `TryResumeFromPending` 으로 fiber B 를 resume → B 의 hReturnFiber 는 root → A 가 중간에 버려짐 | **P1** | §4 M2-1 (fiber 컨텍스트면 즉시 Waiting + root yield) + §4 M2-2 (TryResumeFromPending 진입 `assert(!t_bInsideJobFiber)`) |
| Finding 2 | `Tools/WintersJobSystemStress` 가 `CJobSystem js;` 직접 사용 → `CJobSystem` 은 `WINTERS_ENGINE` export 없음 → "Engine project ref + EngineSDK include" 만으로는 링크 실패 | **P1** | §2 M0-1 + §8 8-1 (AssetConverter 패턴 — `WINTERS_STATIC_BUILD` + JobSystem.cpp/FiberPool.cpp 직접 ClCompile) |
| Finding 3 | M0-1 이 `js.Shutdown()` 전에 `created == deleted` 검사 = false failure. main 이 `WaitForCounter` 안에서 inline 실행 가능 → `acquired == jobCount` 보장 X | **P1** | §2 M0-1 (검증 순서 분리: shutdown 전 `acquired == released` + `acquired <= jobCount`, shutdown 후 `created == deleted`) |
| Finding 4 | `Get_ContextSlot()` 이 `t_iContextSlot` 읽지만 WorkerLoop 진입 시 root worker slot 초기화 미박제 → fiber pool 고갈 / ThreadOnly fallback 시 worker root 가 slot 0 | **P1** | §3 M1-4 수정 7 신규 (WorkerLoop 시작 `t_iContextSlot = iWorkerIdx + 1`, 종료 0 reset) |
| Finding 5 | `Scene_InGame` 라인 또 변동 (381 → 434~445), 라인 고정 박제 취약 | 부가 | §0-3 + §2 M0-2 + §9 (라인 + `CNavigationSystem::Create` 패턴 검색 보조) |
| Finding 6 | 메인 레포 untracked / `nifty-leavitt` worktree 미존재 → PR 흐름이 worktree 기준이면 본 계획서 PR 안 들어감 | 부가 | §11 신규 (worktree 정합성 옵션 A/B/C 박제 — 사용자 결정 대기) |
| Finding 7 | stress vcxproj 가 `Engine/Private/Core/JobCounter.cpp` 를 직접 ClCompile 로 넣지만 실제 파일 없음. `CJobCounter` 는 header-only | **P1** | §2 M0-1 / §8 8-1 (`JobCounter.cpp` 제거, `Core/JobCounter.h` include 만 유지) |
| Finding 8 | `TryExecuteOnFiber(std::function&&, ...)` 는 pool 고갈/ConvertThreadToFiber 실패 시 이미 `item.fn` 이 move 되어 inline fallback 이 빈 함수 | **P1** | §3 M1-3 / §3 M1-4 (`TryExecuteOnFiber(WorkItem&)`, acquire 성공 후에만 ctx 로 move) |

(이전 12 지적 v3 매핑은 §10 변경 이력 v3 참조. 본 머리말은 v4 신규만 명시.)

---

## §0. 현재 박제 상태 (실측, 5/6)

### 0-1. 헤더 3 박제 완료

`Engine/Public/Core/Fiber/FiberTypes.h` 전문:

```cpp
#pragma once

#include "WintersTypes.h"

enum class eJobExecutionMode : u8_t
{
    ThreadOnly = 0,
    FiberShell,
};

using NativeFiberHandle = void*;

#if defined(_MSC_VER)
#define WINTERS_FIBER_CALL __stdcall
#else
#define WINTERS_FIBER_CALL
#endif
```

`Engine/Public/Core/Fiber/Fiber.h` 전문:

```cpp
#pragma once

#include "Core/Fiber/FiberTypes.h"

class CFiber
{
public:
    NativeFiberHandle GetNativeHandle() const { return m_hFiber; }
    void SetNativeHandle(NativeFiberHandle hFiber) { m_hFiber = hFiber; }

private:
    NativeFiberHandle m_hFiber = nullptr;
};
```

`Engine/Public/Core/Fiber/FiberPool.h` 전문 (23줄, **본체 미박제**):

```cpp
#pragma once

#include <vector>

#include "Core/Fiber/Fiber.h"

class CFiberPool
{
public:
    void Reset() { m_vecFibers.clear(); }
    uint32_t GetCount() const { return static_cast<uint32_t>(m_vecFibers.size()); }

private:
    std::vector<CFiber> m_vecFibers;
};
```

### 0-2. JobSystem.cpp FiberShell 동작 본체 박제

파일: `Engine/Private/Core/JobSystem.cpp`

thread_local 3개 (L13-18):

```cpp
namespace
{
    thread_local std::int32_t t_iWorkerIdx = -1;
    thread_local NativeFiberHandle t_hThreadFiber = nullptr;
    thread_local bool t_bInsideJobFiber = false;
}
```

> ⚠️ **fiber-local 아님**: `thread_local` 은 OS thread 마다 하나. 같은 worker thread 위에서 root fiber 와 job fiber 가 번갈아 실행되면 두 fiber 가 같은 TLS 를 본다 → root↔fiber 경계마다 `Set_FiberContext / Clear_FiberContext` 명시 호출 필수 (사용자 지적 #5 / F-10).

`CJobSystem` 클래스 선언 ([JobSystem.h:25](Engine/Public/Core/JobSystem.h:25)):

```cpp
class CJobSystem        // ★ WINTERS_ENGINE dllexport 매크로 없음 (사용자 4차 #2 / F-17)
{
public:
    CJobSystem();
    ~CJobSystem();
    // ...
};
```

> **DLL 경계 함의**: `CJobSystem` 은 export 되지 않음. Tools 측에서 `CJobSystem js;` 직접 사용 시 unresolved external symbol 링크 실패. 해결 = AssetConverter 처럼 `WINTERS_STATIC_BUILD` + `Engine/Private/Core/JobSystem.cpp` 직접 컴파일 (§2 M0-1).

m_vecDeques unique_ptr 우회 (L50-60):

```cpp
// Phase 5-A: vector<T>(N) 대신 unique_ptr 래핑 후 push_back.
// 이유: CWorkStealingDeque 의 std::atomic 멤버 + alignas(64) 조합이
//       MSVC construct_at SFINAE 에서 실패하므로 힙 할당 + 포인터 저장.
m_vecDeques.clear();
m_vecDeques.reserve(iWorkerCount);
for (std::uint32_t i = 0; i < iWorkerCount; ++i)
{
    m_vecDeques.push_back(std::make_unique<CWorkStealingDeque<WorkItem>>());
}
```

> 직접 인용 (P-8 회피): 위 L52-54 의 코멘트 "atomic 멤버 + alignas(64) 조합이 MSVC construct_at SFINAE 에서 실패" 가 본 계획서 §3 M1-1 의 `vector<vector<unique_ptr<FiberContext>>>` 우회 근거.

WorkerLoop ([JobSystem.cpp:157-177](Engine/Private/Core/JobSystem.cpp:157)):

```cpp
void CJobSystem::WorkerLoop(std::uint32_t iWorkerIdx)
{
    t_iWorkerIdx = static_cast<std::int32_t>(iWorkerIdx);
    if (GetExecutionMode() == eJobExecutionMode::FiberShell && !IsThreadAFiber())
    {
        t_hThreadFiber = ConvertThreadToFiber(nullptr);
    }
    // ★ Finding 4 / F-19: 여기서 t_iContextSlot 초기화 미박제 →
    //   FiberShell + 풀 고갈 / ThreadOnly fallback 시 worker root 의 t_iContextSlot 이 0 으로 보임
    //   = MinionAI per-slot buffer 가 worker root 작업도 main(slot 0) 으로 라우팅
    //   → §3 M1-4 수정 7 에서 박제

    while (!m_bShutdown.load(std::memory_order_acquire))
    {
        if (!TryExecuteOneJob(iWorkerIdx))
            std::this_thread::yield();
    }

    if (t_hThreadFiber && IsThreadAFiber())
    {
        ConvertFiberToThread();
        t_hThreadFiber = nullptr;
    }
    t_iWorkerIdx = -1;
    // ★ Finding 4: 종료 시 t_iContextSlot = 0 reset 도 미박제
}
```

TryExecuteItemOnFiber ([JobSystem.cpp:253-273](Engine/Private/Core/JobSystem.cpp:253)):

```cpp
bool CJobSystem::TryExecuteItemOnFiber(WorkItem& item)
{
    if (!IsThreadAFiber())
        t_hThreadFiber = ConvertThreadToFiber(nullptr);

    if (!t_hThreadFiber)
        return false;

    FiberShellCall call{};
    call.pSystem = this;
    call.pItem = &item;                          // ⚠️ caller stack 의 WorkItem& 참조 (F-8)
    call.hReturnFiber = t_hThreadFiber;

    void* hJobFiber = CreateFiber(0, &CJobSystem::FiberShellEntry, &call);  // 매 job 생성
    if (!hJobFiber)
        return false;

    SwitchToFiber(hJobFiber);
    DeleteFiber(hJobFiber);  // 매 job 삭제
    return true;
}
```

WaitForCounter ([JobSystem.cpp:287-340](Engine/Private/Core/JobSystem.cpp:287)) — 핵심 결함:

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
            // ⚠️ Finding 1 / F-16: fiber 컨텍스트에서도 도움실행
            //    → fiber A 가 TryExecuteOneJob 으로 fiber B 를 fire 하면
            //      B 의 hReturnFiber = root 라서 A 가 중간에 버려짐
            //    → 즉시 Waiting + root yield 로 변경 (§4 M2-1)
            bDidWork = TryExecuteOneJob(static_cast<std::uint32_t>(t_iWorkerIdx));
        }
        else
        {
            // Main / 외부 - Global Drain First then Steal (생략)
        }
        if (!bDidWork)
            std::this_thread::yield();   // Fiber yield 아님
    }
}
```

### 0-3. Scene_InGame 라인 변동 (★ Finding 5)

> ★ **라인 정정 (사용자 4차 지적 #5)**: v3 의 L381~393 표기는 5/6 시점 정확했으나 **이미 또 변동**. 현재 (5/6 후반) 실측 = L434~445. 본 박제는 라인 + **`CNavigationSystem::Create` 블록 패턴 검색** 보조로 변경.

실측 ([Scene_InGame.cpp:434-445](Client/Private/Scene/Scene_InGame.cpp:434)):

```cpp
// Phase 5-A: Scheduler
{
    auto pNav = CNavigationSystem::Create();
    pNav->Set_Grid(m_pNavGrid.get());
    //pNav->Set_JobSystem(pJS);   // 여전히 주석 (race 1차 수정 후 미검증)
    m_pScheduler->RegisterSystem(std::move(pNav));   // Phase 3
}

{
    auto pAI = CMinionAISystem::Create();
    pAI->Set_JobSystem(pJS);   // MinionAI 만 활성
    m_pScheduler->RegisterSystem(std::move(pAI));   // Phase 2
}
```

> **박제 진입 시 검증 (P-8 회피 + Finding 5)**:
> 1. `Client/Private/Scene/Scene_InGame.cpp` 에서 `CNavigationSystem::Create` 첫 occurrence 찾기 (grep)
> 2. 해당 블록 안의 `//pNav->Set_JobSystem(pJS);` 주석 줄 확인
> 3. M0-2 박제는 그 줄 (라인 무관) 만 변경
> 4. CLAUDE.md §1.A 의 라인 표기 (564 → 381 → 434) 도 함께 갱신

### 0-4. SystemScheduler nested-wait 경로 (사용자 3차 #10)

파일: [SystemScheduler.cpp:40-89](Engine/Private/ECS/SystemScheduler.cpp:40)

```cpp
void CSystemSchedular::Execute(CWorld& world, float fTimeDelta)
{
    for (auto& [phase, systems] : m_mapPhases)
    {
        // ... batch 구성 ...
        auto flushBatch = [&]()
        {
            if (batch.size() == 1 || m_pJobSystem == nullptr)
            {
                for (ISystem* sys : batch)
                    sys->Execute(world, fTimeDelta);
            }
            else
            {
                CJobCounter counter;
                for (ISystem* sys : batch)
                {
                    m_pJobSystem->Submit(
                        [sys, &world, fTimeDelta]()
                        {
                            sys->Execute(world, fTimeDelta);   // ← system job 안에서
                        },
                        &counter);
                }
                m_pJobSystem->WaitForCounter(&counter);
            }
        };
        // ...
    }
}
```

**핵심**: Phase 가 같은 system 2개 이상이면 `Submit` + `WaitForCounter` 로 batch 실행. 각 system 내부에서 `Execute` 가 다시 `m_pJobSystem->Submit` + `WaitForCounter` 호출하면 nested wait. M0-3 stress 가 본 경로 검증.

---

## §1. 잔여 사고 (codex 4차 검토 → F-21 까지 확장)

```txt
F-1.  CFiberPool 미사용 - 매 job CreateFiber/DeleteFiber                       (P1, M1)
F-2.  WaitForCounter Yield 없음 - std::this_thread::yield 만                    (P1, M2)
F-3.  Get_WorkerSlot Fiber-unsafe - thread-local 인덱스만                       (P2, M3)
F-4.  Scene_InGame NavSystem race 미검증                                        (P1, M0)
F-5.  M1 thread_local handoff = Win32 fiber affinity 위반 위험                  (P1, M1 - per-worker pool)
F-6.  M1 SwitchToFiber 반환 = job 완료 가정 위험                                (P1, M1 - state machine)
F-7.  M3 MinionAI Ensure_SlotBuffers 변경 누락                                  (P2, M3)
F-8.  FiberContext::pItem = &item = caller stack ref, yield 시 stale            (P1, M1) [3차 #2]
F-9.  vector<vector<FiberContext>> + atomic = MSVC SFINAE                       (P1, M1) [3차 #3]
F-10. thread_local 은 fiber-local 아님, root↔fiber 경계 helper 필요             (P1, M1/M2) [3차 #5]
F-11. pending fiber list 전역 단일 = per-worker pool 원칙 충돌                  (P1, M2) [3차 #6]
F-12. t_iFiberPoolIdx worker-local 충돌 = 여러 worker fiber 0 슬롯 공유         (P2, M3) [3차 #7]
F-13. Engine = DLL → Engine/Tests/main() 등록 불가                              (P2, M0) [3차 #9]
F-14. 10K 단순 fan-out 만으로 핵심 버그 못 잡음                                  (P1, M0) [3차 #10]
F-15. MinionAI QueueDamage 는 slot 0 고정 (Get_WorkerSlot 미사용)               (P2, M3) [3차 #8]
F-16. ★ WaitForCounter 가 fiber 안에서 도움실행 = fiber-to-fiber resume 폭주    (P1, M2) [4차 #1]
F-17. ★ CJobSystem WINTERS_ENGINE export 없음 = stress 프로젝트 링크 실패       (P1, M0) [4차 #2]
F-18. ★ M0-1 leak 검증 순서 오류 = false failure                                (P1, M0) [4차 #3]
F-19. ★ WorkerLoop 시작 시 t_iContextSlot 초기화 미박제                         (P1, M1/M3) [4차 #4]
F-20. ★ stress vcxproj 가 없는 JobCounter.cpp 직접 컴파일                        (P1, M0) [4차 #7]
F-21. ★ TryExecuteOnFiber(std::function&&) 이 fallback 전에 fn 을 move            (P1, M1) [4차 #8]
```

영향 요약 (4차 신규만):

- **F-16**: fiber A 가 `WaitForCounter` 안에서 `TryExecuteOneJob` 으로 fiber B 를 fire 하면, B 의 진입 시점에 `hReturnFiber = t_hThreadFiber (root)` 박제. B 완료 시 root 로 복귀 → A 의 fiber stack 은 깨어나지 못한 채 root 가 다음 job 으로 진행. A 가 영구 stall. 더 약한 변형: `TryResumeFromPending` 으로 B 를 resume 시 같은 사고. **회피**: WaitForCounter 가 fiber 컨텍스트면 즉시 Waiting + root yield, 도움실행은 root 만.
- **F-17**: [JobSystem.h:25](Engine/Public/Core/JobSystem.h:25) `class CJobSystem` 에 `WINTERS_ENGINE` 매크로 없음 → DLL export 안 됨. `Tools/WintersJobSystemStress` 가 `CJobSystem js;` 직접 사용 시 LNK2019 (link error). EngineSDK include path 만으로는 헤더는 보이나 심볼은 못 찾음.
- **F-18**: 풀 fiber 는 `Initialize` 에서 `CreateFiber` 로 `created++`, `Shutdown` 에서 `DeleteFiber` 로 `deleted++`. 정상 동작에서도 `Shutdown` 전에는 `created > deleted` (deleted = 0). 또 main thread 가 `WaitForCounter` 안에서 inline 실행 시 fiber acquire 안 함 → `acquired < jobCount` 정상.
- **F-19**: `t_iContextSlot` 갱신 = `Set_FiberContext / Clear_FiberContext` 만 담당. WorkerLoop 시작 시 worker root 컨텍스트는 두 helper 어느 쪽도 안 거치므로 `t_iContextSlot = 0` (default) 유지. FiberShell 모드라도 풀 고갈 (perWorker 4 < 동시 job 수) 시 inline fallback → worker root 가 `Get_ContextSlot()` 호출하면 0 반환 → MinionAI 가 main 슬롯에 push → race.
- **F-20**: 실제 `Engine/Private/Core/` 에 `JobCounter.cpp` 없음. `CJobCounter` 는 [JobCounter.h](Engine/Public/Core/JobCounter.h) 에 구현까지 있는 header-only 타입. stress vcxproj 에 `JobCounter.cpp` 를 넣으면 MSBuild 가 즉시 "file not found" 로 실패.
- **F-21**: `ExecuteItem` 에서 `TryExecuteOnFiber(std::move(item.fn), item.pCounter)` 를 호출하면 함수 인자 생성 시점에 `item.fn` 이 비워진다. 이후 `TryExecuteOnFiber` 가 풀 고갈 / `ConvertThreadToFiber` 실패로 `false` 를 반환하면 `ExecuteItemInline(item)` 은 빈 `fn` 을 보고 아무 일도 하지 않고 counter 만 decrement 하거나, 구현에 따라 job 자체를 유실한다. 회피: `TryExecuteOnFiber(WorkItem& item)` 가 precondition + acquire 성공 후에만 `ctx->fn = std::move(item.fn)` 수행.

---

## §2. M0 - 사전 검증

Stabilization-0 통과 후 즉시 진입.

### M0-1. 1만 job counter stress test (FiberShell mode) — 재설계 (★ Finding 2/3)

박제 위치: 별도 콘솔 프로젝트 `Tools/WintersJobSystemStress/main.cpp` (사용자 3차 #9 — Engine 은 DLL).

**★ vcxproj 패턴 (Finding 2 / F-17, Finding 7 / F-20)**: AssetConverter 패턴 그대로 — `WINTERS_STATIC_BUILD` preprocessor + `Engine/Private/Core/JobSystem.cpp` + `Engine/Private/Core/Fiber/FiberPool.cpp` 직접 ClCompile. `CJobCounter` 는 header-only 이므로 `JobCounter.cpp` 는 없음. Engine project reference 사용 X (DLL 링크 실패 회피).

#### M0-1-A. WintersJobSystemStress.vcxproj 신설

파일 신설: `Tools/WintersJobSystemStress/WintersJobSystemStress.vcxproj`

전문 (AssetConverter 패턴 차용 + JobSystem 직접 컴파일):

```xml
<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="17.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations">
    <ProjectConfiguration Include="Debug|x64">
      <Configuration>Debug</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="Release|x64">
      <Configuration>Release</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
  </ItemGroup>
  <PropertyGroup Label="Globals">
    <VCProjectVersion>17.0</VCProjectVersion>
    <ProjectGuid>{F5555555-5555-5555-5555-555555555555}</ProjectGuid>
    <RootNamespace>WintersJobSystemStress</RootNamespace>
    <WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>
    <Keyword>Win32Proj</Keyword>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" />
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'" Label="Configuration">
    <ConfigurationType>Application</ConfigurationType>
    <UseDebugLibraries>true</UseDebugLibraries>
    <PlatformToolset>v143</PlatformToolset>
    <CharacterSet>Unicode</CharacterSet>
  </PropertyGroup>
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'" Label="Configuration">
    <ConfigurationType>Application</ConfigurationType>
    <UseDebugLibraries>false</UseDebugLibraries>
    <PlatformToolset>v143</PlatformToolset>
    <WholeProgramOptimization>true</WholeProgramOptimization>
    <CharacterSet>Unicode</CharacterSet>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" />

  <PropertyGroup>
    <OutDir>$(MSBuildThisFileDirectory)..\Bin\$(Configuration)\</OutDir>
    <IntDir>$(MSBuildThisFileDirectory)..\Intermediate\$(Configuration)\$(ProjectName)\</IntDir>
    <TargetName>WintersJobSystemStress</TargetName>
  </PropertyGroup>

  <ItemDefinitionGroup>
    <ClCompile>
      <LanguageStandard>stdcpp20</LanguageStandard>
      <WarningLevel>Level3</WarningLevel>
      <SDLCheck>true</SDLCheck>
      <ConformanceMode>true</ConformanceMode>
      <AdditionalOptions>/utf-8 %(AdditionalOptions)</AdditionalOptions>
      <!-- WINTERS_STATIC_BUILD: 이 EXE 는 DLL 이 아니므로 dllexport/dllimport 비활성 (F-17 회피) -->
      <PreprocessorDefinitions>_CRT_SECURE_NO_WARNINGS;NOMINMAX;WINTERS_STATIC_BUILD;%(PreprocessorDefinitions)</PreprocessorDefinitions>
      <AdditionalIncludeDirectories>
        $(MSBuildThisFileDirectory)..\..\Engine\Public;
        $(MSBuildThisFileDirectory)..\..\Engine\Include;
        $(SolutionDir)Engine\Public;
        $(SolutionDir)Engine\Include;
        %(AdditionalIncludeDirectories)
      </AdditionalIncludeDirectories>
    </ClCompile>
    <Link>
      <SubSystem>Console</SubSystem>
    </Link>
  </ItemDefinitionGroup>

  <ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">
    <ClCompile>
      <Optimization>Disabled</Optimization>
    </ClCompile>
  </ItemDefinitionGroup>
  <ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'">
    <ClCompile>
      <Optimization>MaxSpeed</Optimization>
    </ClCompile>
  </ItemDefinitionGroup>

  <!-- ★ Engine/Private 의 .cpp 직접 컴파일 (Finding 2 / F-17, Finding 7 / F-20) -->
  <!-- CJobCounter 는 Core/JobCounter.h header-only 이므로 JobCounter.cpp 등록 금지 -->
  <ItemGroup>
    <ClCompile Include="..\..\Engine\Private\Core\JobSystem.cpp" />
    <ClCompile Include="..\..\Engine\Private\Core\Fiber\FiberPool.cpp" />
    <ClCompile Include="main.cpp" />
  </ItemGroup>

  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />
</Project>
```

> **WintersPCH 의존성 주의**: 위 2 .cpp 가 `#include "WintersPCH.h"` 시작이면 stress 프로젝트도 PCH 처리 또는 `WintersPCH.h` 가 stand-alone 가능한지 확인 필요. AssetConverter 가 PCH 쓰지 않고 직접 컴파일하는지는 같은 vcxproj 의 ClCompile 항목에서 확인 (현 AssetConverter 는 PCH 항목 없음 → stand-alone 가능). **불가하면**: stress vcxproj 에 `<PrecompiledHeader>NotUsing</PrecompiledHeader>` + 각 .cpp 의 PCH include 가 자체 #ifdef 가드 보유한지 확인.

#### M0-1-B. main.cpp 박제 (검증 순서 수정 — Finding 3)

파일 신설: `Tools/WintersJobSystemStress/main.cpp`

전문:

```cpp
// Tools/WintersJobSystemStress/main.cpp
// Phase 5-B-pre M0: Fiber JobSystem stress test
//   - M0-1: 단순 fan-out (10K)
//   - M0-3: nested wait (1K parent x 4 child)

#include "Core/JobSystem.h"
#include "Core/JobCounter.h"
#include <atomic>
#include <iostream>

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

namespace
{
    bool g_bAnyFail = false;
    void Fail(const char* msg)
    {
        std::cerr << "FAIL: " << msg << "\n";
        g_bAnyFail = true;
    }
}

// ─── M0-1: 단순 fan-out (10K) ─────────────────────────────────────────
void StressTest_FiberShell_FanOut_10K()
{
    CJobSystem js;
    js.Initialize(0);
    js.SetExecutionMode(eJobExecutionMode::FiberShell);

    CJobCounter counter;
    std::atomic<std::uint64_t> sum{ 0 };

    constexpr std::uint32_t kJobCount = 10'000;
    for (std::uint32_t i = 0; i < kJobCount; ++i)
    {
        js.Submit([i, &sum]() {
            sum.fetch_add(static_cast<std::uint64_t>(i), std::memory_order_relaxed);
        }, &counter);
    }

    js.WaitForCounter(&counter, 0);

    constexpr std::uint64_t kExpected = static_cast<std::uint64_t>(kJobCount) * (kJobCount - 1) / 2;
    if (sum.load() != kExpected)
        Fail("M0-1 fan-out sum mismatch");
    else
        std::cout << "PASS: M0-1 sum check (" << sum.load() << ")\n";

    // ★ Finding 3 / F-18: shutdown 전 검증 = acquired/released 균형 + acquired 상한
    //   - acquired == released: 풀 acquire/release 짝 맞음 (Waiting fiber 잔존 없음)
    //   - acquired <= jobCount: main thread inline 실행 (= acquire 안 함) 분만큼 빠질 수 있음
    //                           (acquired < jobCount 정상, acquired == jobCount 도 정상)
    {
        const auto stat = js.GetFiberPoolStat();
        std::cout << "  [pre-shutdown] created=" << stat.created
                  << " deleted=" << stat.deleted
                  << " acquired=" << stat.acquired
                  << " released=" << stat.released << "\n";

        if (stat.acquired != stat.released)
            Fail("M0-1 acquire/release imbalance (Waiting fiber 잔존)");
        if (stat.acquired > kJobCount)
            Fail("M0-1 acquired > jobCount (풀 재사용 회로 손상)");
        // ★ stat.created == stat.deleted 검사 금지 (정상 = created > deleted = 0 까지)
    }

    js.Shutdown();

    // ★ shutdown 후: 모든 풀 fiber 가 DeleteFiber 됐는지 검증
    {
        const auto stat = js.GetFiberPoolStat();
        std::cout << "  [post-shutdown] created=" << stat.created
                  << " deleted=" << stat.deleted << "\n";

        if (stat.created != stat.deleted)
            Fail("M0-1 fiber leak (created != deleted after shutdown)");
        else
            std::cout << "PASS: M0-1 fiber leak check\n";
    }
}

// ─── M0-3: nested wait (1K parent x 4 child) ──────────────────────────
void StressTest_FiberShell_NestedWait_1K()
{
    CJobSystem js;
    js.Initialize(0);
    js.SetExecutionMode(eJobExecutionMode::FiberShell);

    CJobCounter parentCounter;
    std::atomic<std::uint32_t> totalChildExecuted{ 0 };

    constexpr std::uint32_t kParentCount = 1'000;
    constexpr std::uint32_t kChildPerParent = 4;

    for (std::uint32_t p = 0; p < kParentCount; ++p)
    {
        js.Submit([&js, &totalChildExecuted, kChildPerParent]() {
            CJobCounter childCounter;
            for (std::uint32_t c = 0; c < kChildPerParent; ++c)
            {
                js.Submit([&totalChildExecuted]() {
                    totalChildExecuted.fetch_add(1, std::memory_order_relaxed);
                }, &childCounter);
            }
            // ★ parent 가 fiber 컨텍스트 안에서 WaitForCounter 호출 → M2 yield 경로
            //   (Finding 1 / F-16 회피 = 즉시 Waiting + root yield 동작 검증)
            js.WaitForCounter(&childCounter, 0);
        }, &parentCounter);
    }

    js.WaitForCounter(&parentCounter, 0);

    constexpr std::uint32_t kExpected = kParentCount * kChildPerParent;
    if (totalChildExecuted.load() != kExpected)
        Fail("M0-3 nested wait child count mismatch");
    else
        std::cout << "PASS: M0-3 nested wait (" << totalChildExecuted.load() << " children)\n";

    {
        const auto stat = js.GetFiberPoolStat();
        if (stat.acquired != stat.released)
            Fail("M0-3 acquire/release imbalance (pending fiber leak)");
    }

    js.Shutdown();

    {
        const auto stat = js.GetFiberPoolStat();
        if (stat.created != stat.deleted)
            Fail("M0-3 fiber leak (created != deleted after shutdown)");
    }
}

int main()
{
#ifdef _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    StressTest_FiberShell_FanOut_10K();
    StressTest_FiberShell_NestedWait_1K();

    std::cout << "All stress tests done. " << (g_bAnyFail ? "[FAIL]" : "[PASS]") << "\n";
    return g_bAnyFail ? 1 : 0;
}
```

`CJobSystem` 에 leak 검증용 카운터 노출 (M1 박제 시 같이):

```cpp
// JobSystem.h public 추가 (M1 박제 시):
struct FiberPoolStat
{
    std::uint64_t created;
    std::uint64_t deleted;
    std::uint64_t acquired;
    std::uint64_t released;
};
FiberPoolStat GetFiberPoolStat() const;
```

#### M0-1 합격 기준 (재정의 — Finding 3)

- **shutdown 전**:
  - `sum == 49,995,000` (M0-1) / `totalChildExecuted == 4,000` (M0-3)
  - `stat.acquired == stat.released` (Waiting fiber 잔존 0)
  - `stat.acquired <= kJobCount` (acquire 폭주 없음, **acquired < kJobCount 도 정상** — main inline)
- **shutdown 후**:
  - `stat.created == stat.deleted` (fiber leak 0)
- 검증 도구 (사용자 3차 #11): CRT `_CrtDumpMemoryLeaks()` 0 leak / Application Verifier (Basics + Heap + Locks + Leak) 위반 0 / **GDI/USER 카운트 폐기**

### M0-2. Scene_InGame NavSystem 주석 복구 (★ Finding 5 — 패턴 검색)

파일: `Client/Private/Scene/Scene_InGame.cpp`

> ★ **라인 고정 X — 패턴 검색**: `CNavigationSystem::Create` 첫 occurrence 가 박제 대상. v3 의 라인 (381) 에서 v4 시점 (434) 이미 변동. 박제 시작 시 grep 으로 현재 라인 확인.

수정 전 (현재 5/6 후반 실측 L434-440):

```cpp
// Phase 5-A: Scheduler
{
    auto pNav = CNavigationSystem::Create();
    pNav->Set_Grid(m_pNavGrid.get());
    //pNav->Set_JobSystem(pJS);
    m_pScheduler->RegisterSystem(std::move(pNav));   // Phase 3
}
```

수정 후:

```cpp
// Phase 5-B-pre: Scheduler (race 1차 수정 + Fiber M0 통과 후 NavSystem 활성)
{
    auto pNav = CNavigationSystem::Create();
    pNav->Set_Grid(m_pNavGrid.get());
    pNav->Set_JobSystem(pJS);   // M0 통과 후 복구 (5/6+)
    m_pScheduler->RegisterSystem(std::move(pNav));   // Phase 3
}
```

박제 절차:

```bash
# 1. 패턴 검색으로 현재 라인 확인
grep -n "CNavigationSystem::Create" Client/Private/Scene/Scene_InGame.cpp

# 2. 그 블록 안의 //pNav->Set_JobSystem(pJS); 줄 찾기
grep -n "//pNav->Set_JobSystem" Client/Private/Scene/Scene_InGame.cpp

# 3. 둘이 같은 블록인지 +5/-5 라인 컨텍스트 보고 확인 후 박제
```

합격 기준:
- 5분 in-game 매치 동안 NavSystem 가 race 없이 동작
- 이전 race 증상 (Pathfinder empty path 직전 frame stale velocity) 재현 안 됨
- Profiler 에서 NavSystem 병렬 실행 확인 (Phase 3 worker tag)

### M0-3. Nested wait stress (사용자 3차 #10)

위 M0-1-B 의 `StressTest_FiberShell_NestedWait_1K` 가 본체. M2 박제 후 재실행. 합격 기준은 M0-1 검증 순서와 동일.

검증 의의:
- [SystemScheduler.cpp:60-70](Engine/Private/ECS/SystemScheduler.cpp:60) 의 system job 안에서 Submit/WaitForCounter 호출하는 실제 위험 경로 시뮬레이션
- **Finding 1 / F-16 회피 검증**: parent fiber 가 WaitForCounter 안에서 즉시 root yield → root 가 child fire → parent fiber 영구 stall 0 hit

---

## §3. M1 - CFiberPool 본체 박제 (★ Finding 4 통합)

### M1 설계 원칙

1. Per-worker pool. 각 worker 가 자신의 fiber free list 만 acquire/release.
2. **FiberContext 가 fn/pCounter 직접 소유** (사용자 3차 #2 / F-8). caller stack 의 `WorkItem& item` 참조 금지.
3. State machine (`Idle/Running/Waiting/Finished`).
4. **vector<vector<unique_ptr<FiberContext>>> 우회** (사용자 3차 #3 / F-9).
5. **thread_local 경계 helper** (사용자 3차 #5 / F-10) — `Set_FiberContext / Clear_FiberContext`.
6. Same-thread acquire/release 강제.
7. **iGlobalFiberIndex / iContextSlot 미리 박제** (사용자 3차 #7 / F-12).
8. **★ WorkerLoop 시작/종료 시 t_iContextSlot 명시 갱신** (Finding 4 / F-19).

### M1-1. FiberPool.h 박제

(v3 박제 그대로 — FiberContext 의 `iContextSlot = 1u + workerCount + iGlobalFiberIndex` 미리 박제)

파일: `Engine/Public/Core/Fiber/FiberPool.h`

수정 후 (전문):

```cpp
#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

#include "Core/Fiber/Fiber.h"
#include "WintersTypes.h"

class CJobSystem;
class CJobCounter;

enum class eFiberState : u8_t
{
    Idle = 0,
    Running,
    Waiting,
    Finished,
};

struct FiberContext
{
    std::function<void()> fn;
    CJobCounter*          pCounter = nullptr;
    CJobSystem*           pSystem  = nullptr;

    NativeFiberHandle     hSelfFiber   = nullptr;
    NativeFiberHandle     hReturnFiber = nullptr;

    std::atomic<eFiberState> state{ eFiberState::Idle };

    CJobCounter*          pWaitingOn = nullptr;
    std::uint32_t         iWaitTarget = 0;

    std::int32_t          iOwnerWorker      = -1;
    std::uint32_t         iLocalFiberIndex  = 0;
    std::uint32_t         iGlobalFiberIndex = 0;
    std::uint32_t         iContextSlot      = 0;

    FiberContext() = default;
    FiberContext(const FiberContext&) = delete;
    FiberContext& operator=(const FiberContext&) = delete;
    FiberContext(FiberContext&&) = delete;
    FiberContext& operator=(FiberContext&&) = delete;
};

class CFiberPool
{
public:
    using FiberEntry = void(__stdcall*)(void*);

    void Initialize(std::uint32_t workerCount, std::uint32_t perWorkerCount, FiberEntry pEntry);
    void Shutdown();

    FiberContext* Acquire(std::uint32_t workerIdx);
    void          Release(FiberContext* pCtx);

    std::uint32_t GetWorkerCount() const     { return m_WorkerCount; }
    std::uint32_t GetPerWorkerCount() const  { return m_PerWorkerCount; }
    std::uint32_t GetTotalCount() const      { return m_WorkerCount * m_PerWorkerCount; }

    std::uint64_t GetCreatedCount()   const { return m_iCreated.load(std::memory_order_acquire); }
    std::uint64_t GetDeletedCount()   const { return m_iDeleted.load(std::memory_order_acquire); }
    std::uint64_t GetAcquiredCount()  const { return m_iAcquired.load(std::memory_order_acquire); }
    std::uint64_t GetReleasedCount()  const { return m_iReleased.load(std::memory_order_acquire); }

private:
    std::vector<std::vector<std::unique_ptr<FiberContext>>> m_PerWorker;
    std::vector<std::vector<FiberContext*>>                 m_FreeList;

    std::uint32_t m_WorkerCount    = 0;
    std::uint32_t m_PerWorkerCount = 0;
    FiberEntry    m_pEntry         = nullptr;

    std::atomic<std::uint64_t> m_iCreated  { 0 };
    std::atomic<std::uint64_t> m_iDeleted  { 0 };
    std::atomic<std::uint64_t> m_iAcquired { 0 };
    std::atomic<std::uint64_t> m_iReleased { 0 };
};
```

### M1-2. FiberPool.cpp 신설

(v3 박제 그대로 — Initialize 시점에 iGlobalFiberIndex / iContextSlot 미리 박제)

파일 신설: `Engine/Private/Core/Fiber/FiberPool.cpp`

전문:

```cpp
#include "WintersPCH.h"
#include "Core/Fiber/FiberPool.h"
#include "Core/JobSystem.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cassert>

void CFiberPool::Initialize(std::uint32_t workerCount, std::uint32_t perWorkerCount, FiberEntry pEntry)
{
    if (!m_PerWorker.empty())
        return;

    m_WorkerCount    = workerCount;
    m_PerWorkerCount = perWorkerCount;
    m_pEntry         = pEntry;

    m_PerWorker.resize(workerCount);
    m_FreeList.resize(workerCount);

    for (std::uint32_t w = 0; w < workerCount; ++w)
    {
        m_PerWorker[w].reserve(perWorkerCount);
        m_FreeList[w].reserve(perWorkerCount);

        for (std::uint32_t i = 0; i < perWorkerCount; ++i)
        {
            auto ctx = std::make_unique<FiberContext>();
            ctx->iOwnerWorker      = static_cast<std::int32_t>(w);
            ctx->iLocalFiberIndex  = i;
            ctx->iGlobalFiberIndex = w * perWorkerCount + i;
            ctx->iContextSlot      = 1u + workerCount + ctx->iGlobalFiberIndex;
            ctx->state.store(eFiberState::Idle, std::memory_order_relaxed);

            ctx->hSelfFiber = CreateFiber(0, m_pEntry, ctx.get());
            if (ctx->hSelfFiber)
            {
                m_iCreated.fetch_add(1, std::memory_order_relaxed);
                FiberContext* pRaw = ctx.get();
                m_PerWorker[w].push_back(std::move(ctx));
                m_FreeList[w].push_back(pRaw);
            }
        }
    }
}

void CFiberPool::Shutdown()
{
    for (auto& vecCtx : m_PerWorker)
    {
        for (auto& ctx : vecCtx)
        {
            if (ctx && ctx->hSelfFiber)
            {
                DeleteFiber(ctx->hSelfFiber);
                ctx->hSelfFiber = nullptr;
                m_iDeleted.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }
    m_PerWorker.clear();
    m_FreeList.clear();
}

FiberContext* CFiberPool::Acquire(std::uint32_t workerIdx)
{
    assert(workerIdx < m_WorkerCount && "Acquire: workerIdx out of range");
    assert(static_cast<std::int32_t>(workerIdx) == CJobSystem::Get_WorkerIdx() &&
           "Acquire: thread != owner worker (per-worker pool 룰 위반 / F-5)");

    auto& freeList = m_FreeList[workerIdx];
    if (freeList.empty())
        return nullptr;
    FiberContext* pCtx = freeList.back();
    freeList.pop_back();
    m_iAcquired.fetch_add(1, std::memory_order_relaxed);
    return pCtx;
}

void CFiberPool::Release(FiberContext* pCtx)
{
    if (!pCtx) return;
    assert(pCtx->state.load(std::memory_order_acquire) == eFiberState::Finished &&
           "Release: state != Finished (Waiting 은 pending list 로 / F-6)");
    assert(pCtx->iOwnerWorker == CJobSystem::Get_WorkerIdx() &&
           "Release: thread != owner worker");

    pCtx->fn          = nullptr;
    pCtx->pCounter    = nullptr;
    pCtx->pSystem     = nullptr;
    pCtx->hReturnFiber = nullptr;
    pCtx->pWaitingOn  = nullptr;
    pCtx->iWaitTarget = 0;
    pCtx->state.store(eFiberState::Idle, std::memory_order_release);

    m_FreeList[static_cast<std::uint32_t>(pCtx->iOwnerWorker)].push_back(pCtx);
    m_iReleased.fetch_add(1, std::memory_order_relaxed);
}
```

### M1-3. JobSystem.h - 멤버 + 경계 helper

(v3 박제 그대로 — Get_ContextSlot / GetFiberPoolCount / GetFiberPoolStat / Set_FiberContext / Clear_FiberContext / TryResumeFromPending / TryExecuteOnFiber 신설, m_FiberPool / m_vecPendingFibers / m_vecPendingMutex 멤버)

파일: `Engine/Public/Core/JobSystem.h`

수정 1 — include 추가:

```cpp
#include "Core/JobSystem/WorkStealingDeque.h"
#include "Core/JobSystem/JobDecl.h"
#include "Core/Fiber/FiberTypes.h"
#include "Core/Fiber/FiberPool.h"   // Phase 5-B-pre M1
```

수정 2 — public 메서드 추가 (`Get_WorkerSlot` 옆):

```cpp
public:
    static std::int32_t  Get_WorkerIdx();
    static std::uint32_t Get_WorkerSlot();
    static std::uint32_t Get_ContextSlot();   // M3

    std::uint32_t GetFiberPoolCount() const
    {
        return m_FiberPool.GetTotalCount();
    }

    struct FiberPoolStat
    {
        std::uint64_t created;
        std::uint64_t deleted;
        std::uint64_t acquired;
        std::uint64_t released;
    };
    FiberPoolStat GetFiberPoolStat() const
    {
        return { m_FiberPool.GetCreatedCount(),
                 m_FiberPool.GetDeletedCount(),
                 m_FiberPool.GetAcquiredCount(),
                 m_FiberPool.GetReleasedCount() };
    }
```

수정 3 — private 멤버 + helper:

```cpp
private:
    CFiberPool m_FiberPool;

    std::vector<std::vector<FiberContext*>>  m_vecPendingFibers;
    std::vector<std::unique_ptr<std::mutex>> m_vecPendingMutex;

    static void Set_FiberContext(FiberContext* pCtx);
    static void Clear_FiberContext();

    bool TryResumeFromPending(std::uint32_t workerIdx);

    // ★ Finding 8 / F-21: WorkItem& 는 저장하지 않고, acquire 성공 후 ctx 로 fn/pCounter 만 move/copy.
    //   std::function&& 시그니처는 실패 fallback 전에 item.fn 을 비워 job 유실 가능.
    bool TryExecuteOnFiber(WorkItem& item);

    static void WINTERS_FIBER_CALL FiberShellEntry(void* pParam);
};
```

### M1-4. JobSystem.cpp - 통합 (★ Finding 4 추가)

#### 수정 1 — thread_local 확장

수정 전 (L13-18):

```cpp
namespace
{
    thread_local std::int32_t t_iWorkerIdx = -1;
    thread_local NativeFiberHandle t_hThreadFiber = nullptr;
    thread_local bool t_bInsideJobFiber = false;
}
```

수정 후:

```cpp
namespace
{
    thread_local std::int32_t      t_iWorkerIdx        = -1;
    thread_local NativeFiberHandle t_hThreadFiber      = nullptr;
    thread_local bool              t_bInsideJobFiber   = false;

    // Phase 5-B-pre M1 (사용자 3차 #5 / F-10): root↔fiber 경계
    thread_local FiberContext*     t_pCurrentFiber     = nullptr;
    thread_local std::uint32_t     t_iContextSlot      = 0;     // ★ Finding 4 / F-19: WorkerLoop 시작 시 갱신 필수
}
```

#### 수정 2 — Set/Clear_FiberContext + Get_ContextSlot helper 신설

```cpp
void CJobSystem::Set_FiberContext(FiberContext* pCtx)
{
    assert(pCtx && "Set_FiberContext: nullptr");
    t_pCurrentFiber   = pCtx;
    t_iContextSlot    = pCtx->iContextSlot;
    t_bInsideJobFiber = true;
}

void CJobSystem::Clear_FiberContext()
{
    t_pCurrentFiber   = nullptr;
    t_iContextSlot    = (t_iWorkerIdx < 0)
                            ? 0u
                            : static_cast<std::uint32_t>(t_iWorkerIdx + 1);
    t_bInsideJobFiber = false;
}

std::uint32_t CJobSystem::Get_ContextSlot()
{
    return t_iContextSlot;
}
```

#### 수정 3 — Initialize 에 풀 + per-worker pending list 박제

수정 전:

```cpp
m_vecWorkers.reserve(iWorkerCount);
for (std::uint32_t i = 0; i < iWorkerCount; ++i)
{
    m_vecWorkers.emplace_back(&CJobSystem::WorkerLoop, this, i);
}
```

수정 후:

```cpp
m_vecWorkers.reserve(iWorkerCount);

m_FiberPool.Initialize(iWorkerCount, /*perWorker*/ 4, &CJobSystem::FiberShellEntry);

m_vecPendingFibers.assign(iWorkerCount, {});
m_vecPendingMutex.clear();
m_vecPendingMutex.reserve(iWorkerCount);
for (std::uint32_t i = 0; i < iWorkerCount; ++i)
    m_vecPendingMutex.push_back(std::make_unique<std::mutex>());

for (std::uint32_t i = 0; i < iWorkerCount; ++i)
{
    m_vecWorkers.emplace_back(&CJobSystem::WorkerLoop, this, i);
}
```

#### 수정 4 — Shutdown 에 풀 + pending 정리

수정 후:

```cpp
m_vecWorkers.clear();
m_vecDeques.clear();

m_FiberPool.Shutdown();

for (const auto& list : m_vecPendingFibers)
{
    assert(list.empty() && "Shutdown: pending fibers 잔존 (counter 미만족)");
}
m_vecPendingFibers.clear();
m_vecPendingMutex.clear();
```

#### 수정 5 — ExecuteItem 새 시그니처 호출 (★ Finding 8 / F-21)

수정 전 (L232-243):

```cpp
void CJobSystem::ExecuteItem(WorkItem& item)
{
    if (GetExecutionMode() == eJobExecutionMode::FiberShell &&
        t_iWorkerIdx >= 0 &&
        !t_bInsideJobFiber &&
        TryExecuteItemOnFiber(item))
    {
        return;
    }
    ExecuteItemInline(item);
}
```

수정 후:

```cpp
void CJobSystem::ExecuteItem(WorkItem& item)
{
    if (GetExecutionMode() == eJobExecutionMode::FiberShell &&
        t_iWorkerIdx >= 0 &&
        !t_bInsideJobFiber)
    {
        // ★ Finding 8 / F-21: TryExecuteOnFiber 가 실패할 때 inline fallback 이 원본 fn 을
        //   그대로 실행할 수 있어야 하므로 여기서 std::move(item.fn) 금지.
        //   TryExecuteOnFiber 내부가 acquire 성공 후에만 ctx 로 move 한다.
        if (TryExecuteOnFiber(item))
            return;
        // 풀 고갈 / ConvertThreadToFiber 실패 → item.fn 보존, inline fallback 가능.
    }
    ExecuteItemInline(item);
}
```

#### 수정 6 — TryExecuteOnFiber 신설 (TryExecuteItemOnFiber 폐기, ★ Finding 8 / F-21)

수정 후:

```cpp
bool CJobSystem::TryExecuteOnFiber(WorkItem& item)
{
    if (t_iWorkerIdx < 0)
        return false;
    if (!item.fn)
        return false;

    if (!IsThreadAFiber())
        t_hThreadFiber = ConvertThreadToFiber(nullptr);
    if (!t_hThreadFiber)
        return false;

    FiberContext* pCtx = m_FiberPool.Acquire(static_cast<std::uint32_t>(t_iWorkerIdx));
    if (!pCtx)
        return false;

    // ★ Finding 8 / F-21: 여기까지 성공한 뒤에만 item.fn 을 ctx 로 move.
    //   이 줄 이후 본 함수는 true 반환 경로로만 진행해야 한다.
    pCtx->fn           = std::move(item.fn);
    pCtx->pCounter     = item.pCounter;
    pCtx->pSystem      = this;
    pCtx->hReturnFiber = t_hThreadFiber;
    pCtx->state.store(eFiberState::Running, std::memory_order_release);

    Set_FiberContext(pCtx);
    SwitchToFiber(pCtx->hSelfFiber);
    Clear_FiberContext();

    const eFiberState st = pCtx->state.load(std::memory_order_acquire);
    if (st == eFiberState::Finished)
    {
        m_FiberPool.Release(pCtx);
    }
    else if (st == eFiberState::Waiting)
    {
        assert(pCtx->iOwnerWorker == t_iWorkerIdx && "Waiting fiber owner mismatch");
        const std::uint32_t w = static_cast<std::uint32_t>(t_iWorkerIdx);
        std::lock_guard<std::mutex> lk(*m_vecPendingMutex[w]);
        m_vecPendingFibers[w].push_back(pCtx);
    }
    else
    {
        assert(false && "fiber returned in invalid state (Idle/Running)");
    }
    return true;
}
```

#### 수정 7 — ★ WorkerLoop 시작/종료 시 t_iContextSlot 갱신 (Finding 4 / F-19)

수정 전 (L157-177):

```cpp
void CJobSystem::WorkerLoop(std::uint32_t iWorkerIdx)
{
    t_iWorkerIdx = static_cast<std::int32_t>(iWorkerIdx);
    if (GetExecutionMode() == eJobExecutionMode::FiberShell && !IsThreadAFiber())
    {
        t_hThreadFiber = ConvertThreadToFiber(nullptr);
    }

    while (!m_bShutdown.load(std::memory_order_acquire))
    {
        if (!TryExecuteOneJob(iWorkerIdx))
            std::this_thread::yield();
    }

    if (t_hThreadFiber && IsThreadAFiber())
    {
        ConvertFiberToThread();
        t_hThreadFiber = nullptr;
    }
    t_iWorkerIdx = -1;
}
```

수정 후:

```cpp
void CJobSystem::WorkerLoop(std::uint32_t iWorkerIdx)
{
    t_iWorkerIdx   = static_cast<std::int32_t>(iWorkerIdx);
    // ★ Finding 4 / F-19: 풀 고갈 / ThreadOnly fallback 시에도 worker root 가
    //   slot 0 (main) 으로 보이는 사고 회피. Set/Clear_FiberContext 가 닿지 않는 경계.
    t_iContextSlot = static_cast<std::uint32_t>(iWorkerIdx + 1);

    if (GetExecutionMode() == eJobExecutionMode::FiberShell && !IsThreadAFiber())
    {
        t_hThreadFiber = ConvertThreadToFiber(nullptr);
    }

    while (!m_bShutdown.load(std::memory_order_acquire))
    {
        // Phase 5-B-pre M2: pending fiber 우선 (자기 list 만, root 컨텍스트에서만 호출)
        if (TryResumeFromPending(iWorkerIdx))
            continue;
        if (!TryExecuteOneJob(iWorkerIdx))
            std::this_thread::yield();
    }

    if (t_hThreadFiber && IsThreadAFiber())
    {
        ConvertFiberToThread();
        t_hThreadFiber = nullptr;
    }

    t_iWorkerIdx   = -1;
    t_iContextSlot = 0;   // ★ Finding 4: 종료 reset (다른 thread 가 이 OS thread 재사용 시 stale 회피)
}
```

#### 수정 8 — FiberShellEntry 가 ctx 직접 소비

수정 후:

```cpp
void WINTERS_FIBER_CALL CJobSystem::FiberShellEntry(void* pParam)
{
    FiberContext* pCtx = static_cast<FiberContext*>(pParam);
    if (!pCtx)
        return;

    while (true)
    {
        if (pCtx->fn)
        {
            pCtx->fn();
        }
        if (pCtx->pCounter)
            pCtx->pCounter->Decrement();

        pCtx->state.store(eFiberState::Finished, std::memory_order_release);
        SwitchToFiber(pCtx->hReturnFiber);
        // 다음 acquire 시 fn 갱신 + state = Running 박제 후 재진입
    }
}
```

### M1 합격 검증

- M0-1 stress 재실행 → `created == deleted` (shutdown 후) / `acquired == released` (shutdown 전)
- assert 0 hit (per-worker `Acquire` thread affinity, `Release` state == Finished, `Set_FiberContext` nullptr 가드)
- `m_vecPendingFibers` 모두 비어있음 (M1 단계에서는 yield 경로 미사용)
- 빌드 검증: `vector<vector<unique_ptr<FiberContext>>>` MSVC C++20 SFINAE 통과
- ★ **Finding 4 검증**: ThreadOnly mode 로 stress 재실행 → `Get_ContextSlot()` 이 worker root 에서 `iWorkerIdx + 1` 반환 (0 아님) — assert 또는 출력 검사

---

## §4. M2 - WaitForCounter Yield-aware (★ Finding 1 본격 반영)

### M2 설계 — fiber 컨텍스트는 즉시 yield, 도움실행은 root 만

이전 v3 박제 (`if (t_iWorkerIdx >= 0) { TryResumeFromPending; TryExecuteOneJob; } if (!bDidWork && t_bInsideJobFiber) yield;`) 는 **fiber A 가 도움실행으로 fiber B 를 fire 하면 B 의 hReturnFiber = root → A 가 영구 stall** (Finding 1 / F-16).

**v4 설계**: `WaitForCounter` 진입 직후 `t_bInsideJobFiber` 검사:
- **fiber 컨텍스트 (`t_bInsideJobFiber == true`)**: 도움실행 0 회. 즉시 `state = Waiting` 박제 + `SwitchToFiber(t_hThreadFiber)` 로 root yield. resume 후 다음 iteration 에서 counter 재검사.
- **root 컨텍스트 (`t_bInsideJobFiber == false`)**: 기존 도움실행 (TryResumeFromPending / TryExecuteOneJob / Global drain / Steal).

`TryResumeFromPending` 자체에도 진입 가드 (`assert(!t_bInsideJobFiber)`) 추가.

### M2-1. WaitForCounter (★ Finding 1 — fiber-안 도움실행 금지)

파일: `Engine/Private/Core/JobSystem.cpp`

수정 위치: `WaitForCounter` (L287-340).

수정 전:

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
            // Global Drain First then Steal (Main / 외부)
        }
        if (!bDidWork)
            std::this_thread::yield();
    }
}
```

수정 후 (★ v4 본체):

```cpp
void CJobSystem::WaitForCounter(CJobCounter* pCounter, std::uint32_t iTarget)
{
    if (!pCounter)
        return;

    while (pCounter->Load() > iTarget)
    {
        // ★ Finding 1 / F-16: fiber 컨텍스트면 도움실행 0 회 — 즉시 Waiting + root yield
        //   이유: fiber A 가 TryExecuteOneJob/TryResumeFromPending 으로 fiber B 를 fire 하면
        //         B 의 hReturnFiber = root → B 완료 시 root 로 복귀 → A 가 영구 stall
        //   회피: A 자체를 Waiting 으로 박제 후 root 양보. root 가 자기 일정으로 다른 job/pending 을
        //         fire 하므로 fiber-to-fiber 직접 점프 없음.
        if (t_bInsideJobFiber && t_pCurrentFiber && t_hThreadFiber)
        {
            FiberContext* pCtx = t_pCurrentFiber;
            const std::int32_t iSelfIdx = t_iWorkerIdx;

            pCtx->pWaitingOn  = pCounter;
            pCtx->iWaitTarget = iTarget;
            pCtx->state.store(eFiberState::Waiting, std::memory_order_release);

            Clear_FiberContext();
            NativeFiberHandle hReturn = pCtx->hReturnFiber;
            SwitchToFiber(hReturn);

            // ─── resume 지점 ───
            // root 가 TryResumeFromPending → SwitchToFiber(hSelfFiber) 로 깨움
            // resume 시 caller (root 의 TryResumeFromPending) 가 Set_FiberContext 호출 → t_pCurrentFiber 복원
            assert(t_iWorkerIdx == iSelfIdx &&
                   "fiber resumed on different worker (per-worker pending 룰 위반)");
            assert(t_bInsideJobFiber && "fiber resumed without Set_FiberContext");
            // 다음 while iteration 으로 counter 재검사
            continue;
        }

        // ★ root 컨텍스트만 도움실행 (TryResumeFromPending 도 root 전용)
        bool bDidWork = false;

        if (t_iWorkerIdx >= 0)
        {
            // Worker root: 자기 pending 우선 → 자기 deque/global/steal
            if (TryResumeFromPending(static_cast<std::uint32_t>(t_iWorkerIdx)))
                bDidWork = true;
            else
                bDidWork = TryExecuteOneJob(static_cast<std::uint32_t>(t_iWorkerIdx));
        }
        else
        {
            // Main / 외부: Global drain → Steal (TryResumeFromPending 호출 X — owner worker 만 자기 pending 다룸)
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

### M2-2. TryResumeFromPending — root 전용 (★ Finding 1)

파일: `Engine/Private/Core/JobSystem.cpp`

새 메서드:

```cpp
// Phase 5-B-pre M2: 자기 pending list 만 검사
//   ★ Finding 1 / F-16: 진입 시 t_bInsideJobFiber == false 강제 (fiber 안에서 호출 금지)
//      fiber 안에서 호출 시 fiber-to-fiber 직접 점프 = stall. WaitForCounter 가 fiber 컨텍스트면
//      이미 위 분기에서 root 로 yield 했으므로 본 함수 진입 시점에는 항상 root 컨텍스트.
bool CJobSystem::TryResumeFromPending(std::uint32_t workerIdx)
{
    assert(!t_bInsideJobFiber &&
           "TryResumeFromPending: fiber 컨텍스트에서 호출 금지 (Finding 1 / F-16)");
    assert(static_cast<std::int32_t>(workerIdx) == t_iWorkerIdx &&
           "TryResumeFromPending: 다른 worker 의 pending 검사 금지");

    FiberContext* pCtx = nullptr;
    {
        std::lock_guard<std::mutex> lk(*m_vecPendingMutex[workerIdx]);
        auto& list = m_vecPendingFibers[workerIdx];
        for (auto it = list.begin(); it != list.end(); ++it)
        {
            FiberContext* p = *it;
            if (p->pWaitingOn && p->pWaitingOn->Load() <= p->iWaitTarget)
            {
                pCtx = p;
                list.erase(it);
                break;
            }
        }
    }

    if (!pCtx)
        return false;

    pCtx->state.store(eFiberState::Running, std::memory_order_release);
    pCtx->pWaitingOn  = nullptr;
    pCtx->iWaitTarget = 0;

    // ★ hReturnFiber 갱신 — 현재 root 의 t_hThreadFiber 로 (resume 후 root 로 복귀해야 함)
    pCtx->hReturnFiber = t_hThreadFiber;

    Set_FiberContext(pCtx);
    SwitchToFiber(pCtx->hSelfFiber);
    Clear_FiberContext();

    const eFiberState st = pCtx->state.load(std::memory_order_acquire);
    if (st == eFiberState::Finished)
    {
        m_FiberPool.Release(pCtx);
    }
    else if (st == eFiberState::Waiting)
    {
        std::lock_guard<std::mutex> lk(*m_vecPendingMutex[workerIdx]);
        m_vecPendingFibers[workerIdx].push_back(pCtx);
    }
    else
    {
        assert(false && "resumed fiber returned in invalid state");
    }
    return true;
}
```

### M2-3. WorkerLoop pending 우선 검사 (M1-4 수정 7 에 이미 박제)

§3 M1-4 수정 7 에서 함께 박제 완료. 본 절은 의의만 명시:
- WorkerLoop 의 main loop 가 매 iteration 마다 `TryResumeFromPending(iWorkerIdx)` 우선 검사
- pending 이 비어있을 때만 `TryExecuteOneJob` 진행
- 결과: counter 만족 fiber 가 바로 resume → wait latency 감소

### M2-4. 정상 종료 시 모든 fiber Release

- `CFiberPool::Shutdown` 가 처리 (M1-2)
- worker root fiber 는 `WorkerLoop` 의 `ConvertFiberToThread`
- Shutdown 의 pending list 비어있음 assert (M1-4 수정 4)

### M2 합격 검증

- M0-3 nested wait stress: parent 1K x child 4 = 4K 전 child 실행, deadlock 없음
- ★ **Finding 1 회피 검증**: parent fiber 가 `WaitForCounter` 안에서 절대 도움실행 안 함 — `TryExecuteOneJob` / `TryResumeFromPending` 호출 카운터 0 (별도 stat 추가 가능)
- worker affinity assert 0 hit
- pending list size <= worker count x perWorker 항상 만족

---

## §5. M3 - Get_WorkerSlot Fiber-safe (변경 없음 + Finding 4 와 자연 정합)

### M3-1. 현재 한계

```cpp
std::uint32_t CJobSystem::Get_WorkerSlot()
{
    return (t_iWorkerIdx < 0) ? 0u : static_cast<std::uint32_t>(t_iWorkerIdx + 1);
}
```

Fiber 가 yield → resume 시 stable slot 보장 안 됨. MinionAI per-slot buffer 가 race.

### M3-2. 대안 - 미리 박제된 iContextSlot

§3 M1-1 의 `FiberContext::iContextSlot = 1u + workerCount + iGlobalFiberIndex` 박제 + §3 M1-4 수정 2 의 `Get_ContextSlot()` + 수정 7 의 `WorkerLoop` 시작/종료 갱신 (★ Finding 4) 으로 자연 정합.

slot 레이아웃:

```txt
slot 0       = Main / 외부 thread (또는 OS thread 가 worker 가 아닌 경우)
slot 1~N     = Worker 0~N-1 root context (= worker idx + 1, ★ Finding 4 로 보장)
slot N+1~    = Fiber pool ctx (= 1 + N + iGlobalFiberIndex)
              worker 0 fiber 0 → N+1
              worker 0 fiber 1 → N+2
              ...
              worker (W-1) fiber (P-1) → N + W*P
```

총 slot = `1 + N + W*P` (default = `1 + N + 4N` = `1 + 5N`).

### M3-3. MinionAISystem 마이그 (사용자 3차 #8 / F-15 그대로)

파일: `Engine/Private/ECS/Systems/MinionAISystem.cpp`

#### 수정 1 — Ensure_SlotBuffers 가 fiber pool 까지 resize

수정 전 ([MinionAISystem.cpp 의 Ensure_SlotBuffers 주변]):

```cpp
void CMinionAISystem::Ensure_SlotBuffers()
{
    const uint32_t need = (m_pJobSystem ? m_pJobSystem->GetWorkerCount() : 0u) + 1u;
    if (m_vecDecisionsPerSlot.size() == need && m_vecDamagesPerSlot.size() == need)
        return;

    m_vecDecisionsPerSlot.resize(need);
    m_vecDamagesPerSlot.resize(need);
}
```

수정 후:

```cpp
void CMinionAISystem::Ensure_SlotBuffers()
{
    // Phase 5-B-pre M3: slot 레이아웃 = [Main(0) | Worker root 0~N-1 (1~N) | Fiber pool (N+1 ~ N+W*P)]
    const uint32_t workerCount    = m_pJobSystem ? m_pJobSystem->GetWorkerCount()    : 0u;
    const uint32_t fiberPoolCount = m_pJobSystem ? m_pJobSystem->GetFiberPoolCount() : 0u;
    const uint32_t need = 1u + workerCount + fiberPoolCount;
    if (m_vecDecisionsPerSlot.size() == need && m_vecDamagesPerSlot.size() == need)
        return;

    m_vecDecisionsPerSlot.resize(need);
    m_vecDamagesPerSlot.resize(need);
}
```

#### 수정 2 — Push_Decision 마이그 ([MinionAISystem.cpp:305-311](Engine/Private/ECS/Systems/MinionAISystem.cpp:305))

수정 전:

```cpp
void CMinionAISystem::Push_Decision(const MinionDecision& dec)
{
    const uint32_t slot = CJobSystem::Get_WorkerSlot();
    if (slot >= m_vecDecisionsPerSlot.size())
        return;
    m_vecDecisionsPerSlot[slot].push_back(dec);
}
```

수정 후:

```cpp
void CMinionAISystem::Push_Decision(const MinionDecision& dec)
{
    const uint32_t slot = CJobSystem::Get_ContextSlot();   // M3: fiber-safe
    if (slot >= m_vecDecisionsPerSlot.size())
    {
        assert(false && "Push_Decision: slot >= buffer size — Ensure_SlotBuffers 누락 또는 fiber slot 미반영");
        return;
    }
    m_vecDecisionsPerSlot[slot].push_back(dec);
}
```

#### 수정 3 — QueueDamage 마이그 (사용자 3차 #8 / F-15)

수정 전 ([MinionAISystem.cpp:313-318](Engine/Private/ECS/Systems/MinionAISystem.cpp:313) — **slot 0 고정**):

```cpp
void CMinionAISystem::QueueDamage(EntityID source, EntityID target, f32_t amount, bool_t bKill)
{
    if (m_vecDamagesPerSlot.empty())
        return;
    m_vecDamagesPerSlot[0].push_back(DamageEvent{ source, target, amount, bKill });
}
```

수정 후:

```cpp
void CMinionAISystem::QueueDamage(EntityID source, EntityID target, f32_t amount, bool_t bKill)
{
    // Phase 5-B-pre M3: 이전에는 slot 0 고정 (Main thread 가정)
    //   → fiber 컨텍스트에서 호출 가능해지면서 context slot 사용
    const uint32_t slot = CJobSystem::Get_ContextSlot();
    if (slot >= m_vecDamagesPerSlot.size())
    {
        assert(false && "QueueDamage: slot >= buffer size — Ensure_SlotBuffers 누락 또는 fiber slot 미반영");
        return;
    }
    m_vecDamagesPerSlot[slot].push_back(DamageEvent{ source, target, amount, bKill });
}
```

> ⚠️ **행동 보존 검증** (CLAUDE.md §5.7 P-14): 자료 구조 라우팅만 변경. damage 적용 순서 / 합산 / Apply_Damage 본체 불변. 5분 매치 minion 행동 시퀀스 동일성 검증 필수.

### M3 합격 검증

- 1만 minion AI tick stress (FiberShell mode) → decision/damage drop 0 hit
- assert 0 hit
- ★ **Finding 4 와 자연 정합 검증**: ThreadOnly mode 에서도 worker root 가 `Get_ContextSlot()` 로 `iWorkerIdx + 1` 반환 (slot 0 아님)
- 검증 도구 (사용자 3차 #12): ASan / Application Verifier / Concurrency Visualizer / 커스텀 stress

---

## §6. 진입 순서 + 합격 단계

```txt
0. Stabilization-0 통과 (CLAUDE.md §1.B Option Stabilization-0)

1. M0 사전 검증
   - Tools/WintersJobSystemStress 신규 콘솔 프로젝트 박제
     ★ Finding 2 / F-17 + Finding 7 / F-20:
       WINTERS_STATIC_BUILD + JobSystem.cpp/FiberPool.cpp 직접 ClCompile
       (CJobCounter 는 header-only, JobCounter.cpp 없음)
   - M0-1: 1만 fan-out
     ★ Finding 3 / F-18: shutdown 전 acquired==released, shutdown 후 created==deleted
   - M0-2: Scene_InGame NavSystem 주석 복구
     ★ Finding 5: 라인 고정 X → CNavigationSystem::Create 패턴 검색
   - M0-3: 1K x 4 nested wait
     ★ Finding 1 / F-16 회피 검증

2. M1 CFiberPool 박제
   - FiberPool.h / .cpp 본체 박제
   - JobSystem.h: Get_ContextSlot / GetFiberPoolCount / GetFiberPoolStat
                  Set_FiberContext / Clear_FiberContext / TryExecuteOnFiber / TryResumeFromPending
   - JobSystem.cpp:
     - thread_local 5개 (★ t_iContextSlot 추가)
     - Initialize / Shutdown / ExecuteItem / TryExecuteOnFiber / FiberShellEntry
     - ★ WorkerLoop 시작 t_iContextSlot = iWorkerIdx + 1, 종료 0 reset (Finding 4)
   - M0-1 재실행 → fiber leak 0
   - 빌드 검증

3. M2 WaitForCounter Yield
   - ★ WaitForCounter 진입 직후 t_bInsideJobFiber 검사 → fiber 컨텍스트면 즉시 Waiting + root yield (Finding 1)
   - ★ root 컨텍스트만 TryResumeFromPending / TryExecuteOneJob 호출 (Finding 1)
   - TryResumeFromPending 진입 assert(!t_bInsideJobFiber)
   - M0-3 재실행 → deadlock 0 + 도움실행 카운터 root 만 증가
   - Worker affinity assert 통과

4. M3 Get_WorkerSlot Fiber-safe
   - Get_ContextSlot 노출 (M1-3 시점)
   - MinionAISystem 마이그 (Push_Decision / QueueDamage / Ensure_SlotBuffers)
   - ASan / Application Verifier / Concurrency Visualizer race 0 hit
   - ★ Finding 4 정합: ThreadOnly mode 에서도 worker root slot != 0
   - 5분 매치 행동 보존 검증
```

---

## §7. 박제 함정 자체 점검

`.md/process/PLAN_AUTHORING_PITFALLS.md` 의 8 단계 관문:

```txt
A. §1 사전 결정 미박제 0      OK (사고 ID F-1~F-21 명시)
B. PIMPL 본체 read           OK (JobSystem.h L1-60 + JobSystem.cpp L1-340 +
                                 MinionAISystem.cpp L33-318 + SystemScheduler.cpp L40-89 +
                                 Scene_InGame.cpp L425-455 + WintersAssetConverter.vcxproj 전수 read)
C. Render path 전수          N/A (render 변경 없음)
D. 인용 의미 검증             OK (모든 L## 인용에 직접 인용 블록 동반,
                                 특히 L25 CJobSystem export 부재 + L50-54 atomic SFINAE +
                                 SystemScheduler L60-70 nested wait + AssetConverter L51 / L95-106)
E. ECS Scheduler 동시성       ★ 직접 변경 없으나 SystemScheduler:60-70 nested wait 의 주 검증 대상 (M0-3 stress)
F. Owner Scope 매트릭스       OK (CFiberPool / m_vecPendingFibers / m_vecPendingMutex 모두 CJobSystem 멤버)
G. API 실재 검증              OK (Win32 Fibers API 표준 + JobSystem/MinionAISystem 본체 grep +
                                 AssetConverter vcxproj 패턴 검증)
H. 도메인 상수 분리           OK (FiberPool default = workerCount * 4 동적 계산)
```

---

## §8. 외부 영향 분석

### 8-1. 영향 받는 파일

```txt
Engine/Public/Core/Fiber/FiberPool.h               본체 박제 (FiberContext + Acquire/Release + leak 카운터)
Engine/Private/Core/Fiber/FiberPool.cpp            신설
Engine/Public/Core/JobSystem.h                     m_FiberPool / m_vecPendingFibers / Set_FiberContext /
                                                     Clear_FiberContext / Get_ContextSlot / GetFiberPoolCount /
                                                     GetFiberPoolStat / TryExecuteOnFiber / TryResumeFromPending
Engine/Private/Core/JobSystem.cpp                  thread_local 5개 (★ t_iContextSlot 추가) /
                                                     Initialize / Shutdown / ExecuteItem / TryExecuteOnFiber /
                                                     FiberShellEntry / WaitForCounter (★ Finding 1) /
                                                     TryResumeFromPending (★ Finding 1 assert) /
                                                     WorkerLoop (★ Finding 4 슬롯 갱신) /
                                                     Get_ContextSlot / Set_FiberContext / Clear_FiberContext

★ Tools/WintersJobSystemStress/main.cpp            신설 (M0-1 + M0-3 stress, ★ Finding 3 검증 순서)
★ Tools/WintersJobSystemStress/                    신설 콘솔 프로젝트
   WintersJobSystemStress.vcxproj                    (★ Finding 2: WINTERS_STATIC_BUILD +
                                                      Engine/Private/Core/JobSystem.cpp /
                                                      Fiber/FiberPool.cpp 직접 ClCompile,
                                                      CJobCounter 는 header-only)

Client/Private/Scene/Scene_InGame.cpp              NavSystem Set_JobSystem 주석 복구
                                                     (★ Finding 5: 라인 고정 X — CNavigationSystem::Create 패턴 검색)
Engine/Private/ECS/Systems/MinionAISystem.cpp      Ensure_SlotBuffers / Push_Decision / QueueDamage
Engine/Include/Engine.vcxproj + .filters           FiberPool.cpp 등록
Tools 솔루션 (Winters.sln 또는 Tools.sln)         WintersJobSystemStress.vcxproj 추가 (Engine project ref 없음 — 직접 컴파일)
```

### 8-2. 외부 호출 호환성

- `CJobSystem::Submit` / `WaitForCounter` / `CJobCounter` public API 불변
- `Get_WorkerSlot` 기존 호출자 그대로 동작 (`Get_ContextSlot` 은 신규)
- `eJobExecutionMode { ThreadOnly, FiberShell }` 기존 enum 유지
- `CJobSystem::TryExecuteItemOnFiber(WorkItem&)` 폐기 → `TryExecuteOnFiber(WorkItem&)` 신설 (private, 외부 호출자 없음)
  - ★ Finding 8 / F-21: `WorkItem&` 는 저장하지 않고 acquire 성공 후 `ctx->fn = std::move(item.fn)` 만 수행. 실패 시 inline fallback 을 위해 `item.fn` 보존.
- ★ `WaitForCounter` 의 fiber-안 호출 동작 변경: v3 까지 도움실행 → v4 부터 즉시 root yield (Finding 1)

### 8-3. 데스크탑 진입 시 주의

- VS (devenv.exe) 종료 후 빌드 (vc143.pdb lock 회피)
- `git checkout -b feature/fiber-stabilization-m1`
- M0 → M1 → M2 → M3 순차 진입
- M1 단계에서 `Engine.vcxproj` `.filters` 갱신 필수 (FiberPool.cpp 등록)
- ★ **M0 단계 중요** (Finding 2): `Tools/WintersJobSystemStress.vcxproj` 신규 등록 시
  - Tools 솔루션 존재 여부 확인 (없으면 사용자 결정 필요 — Tools.sln 신설 vs Winters.sln 에 추가)
  - WintersPCH 의존성 검증 (직접 컴파일 .cpp 가 PCH 의존하는지) — 현 AssetConverter 패턴 보면 PCH 안 씀, 그대로 차용
  - Engine project reference **사용 X** (DLL 링크 실패 회피 — F-17)
- ★ **Finding 5**: M0-2 박제 직전 `grep -n "CNavigationSystem::Create" Client/Private/Scene/Scene_InGame.cpp` 로 현재 라인 확인
- Application Verifier 셋업: `appverif.exe` → WintersJobSystemStress.exe → Basics / ThreadPool / Heap / Locks / Leak

---

## §9. 진입 직전 체크리스트

```txt
- Stabilization-0 5 항목 완료
- 박제 함정 8 관문 통과 확인 (§7)
- devenv 종료 + branch 생성

- 코드 현 상태 1회 더 read (라인 변동 가능성):
  - Engine/Public/Core/Fiber/{FiberPool.h, Fiber.h, FiberTypes.h}
  - Engine/Public/Core/JobSystem.h L25 (CJobSystem export 매크로 여전히 없는지) ★ Finding 2
  - Engine/Private/Core/JobSystem.cpp (5/6 이후 변경)
  - Engine/Private/ECS/Systems/MinionAISystem.cpp L313 QueueDamage (slot 0 고정 변경 없는지)
  - Engine/Private/ECS/SystemScheduler.cpp L40-89 (nested wait 경로 변경 없는지)
  - Client/Private/Scene/Scene_InGame.cpp ★ Finding 5: grep "CNavigationSystem::Create" 로 라인 재확인
  - Tools/WintersAssetConverter/WintersAssetConverter.vcxproj L42-107 (vcxproj 패턴 갱신 가능성)

- Tools 솔루션 존재 여부 확인 (없으면 신설 결정 필요)
- ASan / Application Verifier / Concurrency Visualizer 셋업 완료
- ★ worktree 정합성 결정 (§11)
```

---

## §10. 변경 이력

```txt
2026-05-05 v1: 4 잔여 사고 (F-1~F-4) 박제
2026-05-05 v2: codex 2차 → F-5~F-7 추가 + M1 per-worker pool + state machine 재설계
2026-05-06 v3: codex 3차 (사용자 12 지적) → F-8~F-15 추가
              - Scene_InGame 라인 564 → 381 정정
              - FiberContext fn/pCounter 직접 소유
              - vector<vector<unique_ptr<FiberContext>>> 우회
              - M2 SwitchToFiber + state machine 본격 연결
              - Set_FiberContext / Clear_FiberContext 경계 helper
              - per-worker pending list
              - iGlobalFiberIndex / iContextSlot 미리 박제
              - QueueDamage 마이그 정확화
              - Tools/WintersJobSystemStress 콘솔 프로젝트 분리
              - Nested wait stress 신규
              - GDI/USER 카운트 폐기
              - TSAN → ASan 등으로 변경

2026-05-06 v4: codex 4차 (사용자 4 P1 + 2 부가 + 구현성 2건) → F-16~F-21 추가
              ★ Finding 1 / F-16: WaitForCounter 가 fiber 안에서도 도움실행 = fiber-to-fiber stall
                                  → 즉시 Waiting + root yield + TryResumeFromPending root 전용 assert
              ★ Finding 2 / F-17: CJobSystem WINTERS_ENGINE export 없음 → stress 프로젝트 링크 실패
                                  → AssetConverter 패턴 (WINTERS_STATIC_BUILD + 직접 ClCompile)
              ★ Finding 3 / F-18: M0-1 leak 검증 순서 오류 (false failure)
                                  → shutdown 전 acquired==released + acquired<=jobCount,
                                    shutdown 후 created==deleted
              ★ Finding 4 / F-19: WorkerLoop 시작 시 t_iContextSlot 초기화 미박제
                                  → 시작 = iWorkerIdx + 1, 종료 = 0 reset
              ★ Finding 7 / F-20: stress vcxproj 가 실제 없는 JobCounter.cpp 직접 컴파일
                                  → JobCounter.cpp 제거, CJobCounter 는 header-only 로 사용
              ★ Finding 8 / F-21: TryExecuteOnFiber(std::function&&) 가 fallback 전에 fn move
                                  → TryExecuteOnFiber(WorkItem&) 로 변경, acquire 성공 후에만 ctx 로 move
              ★ Finding 5: Scene_InGame 라인 또 변동 (381 → 434~445)
                          → 라인 + CNavigationSystem::Create 패턴 검색 보조 박제
              ★ Finding 6: worktree 정합성 (메인 레포 untracked / nifty-leavitt 미존재)
                          → §11 신규 (옵션 A/B/C, 사용자 결정 대기)
```

---

## §11. ★ Worktree 정합성 (Finding 6 — 사용자 결정 대기)

### 현 상태

- 본 계획서는 **메인 레포** (`C:\Users\user\Desktop\Winters\.md\plan\engine\FIBER_JOBSYSTEM_STABILIZATION_PLAN.md`) 에 박제됨
- 메인 레포 git 상태: 본 계획서가 **untracked** 또는 **modified**
- 워크트리 (`C:\Users\user\Desktop\Winters\.claude\worktrees\nifty-leavitt-6d37dd\`, 브랜치 `claude/nifty-leavitt-6d37dd`) 에는 **본 계획서 없음** (별도 working tree)
- PR 흐름이 워크트리 기준이면 본 계획서 변경이 PR 안에 안 들어감

### 옵션 (사용자 결정 필요)

**옵션 A**: 메인 레포 그대로 두기
- 사용자가 메인 레포에서 직접 검토 + commit
- 워크트리 브랜치에는 본 계획서 안 들어감
- 사용 케이스: 본 계획서가 워크트리 PR 본체와 분리된 별도 산출물일 때

**옵션 B**: 워크트리로 옮기기
- 같은 내용을 워크트리에도 Write
- 메인 레포 변경 되돌림 (`git -C C:\Users\user\Desktop\Winters checkout -- .md/plan/engine/FIBER_JOBSYSTEM_STABILIZATION_PLAN.md` 또는 사용자가 직접 수동 폐기)
- 워크트리 브랜치에서 commit / PR
- 사용 케이스: 본 계획서가 PR 본체에 포함되어야 할 때

**옵션 C**: 양쪽 다
- 메인 레포는 사용자가 commit
- 워크트리에도 같은 내용 Write 후 별도 commit
- 중복 작업이지만 둘 다 살림
- 사용 케이스: 메인 레포 작업 흐름 + 워크트리 PR 양쪽 다 필요할 때

### 권장 (사용자 메모리 `feedback_worktree_vs_main.md` 와 충돌)

사용자 메모리: "**읽기/검증** 은 메인 레포 기준" — Write 의도까지 메인인지 워크트리인지 모호. 박제 시점에 사용자 결정 필요.

> 박제 진입 결정 트리거: 본 §11 의 옵션 선택 후 §1~§10 박제 진입.
