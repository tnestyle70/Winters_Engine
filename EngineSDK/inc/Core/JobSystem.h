#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>       // std::unique_ptr — Deque 를 힙에 놓고 포인터로 벡터 보관
#include <mutex>
#include <thread>
#include <vector>
#include <queue>

#include "WintersAPI.h"
#include "Core/JobSystem/WorkStealingDeque.h"
#include "Core/JobSystem/JobDecl.h"
#include "Core/Fiber/FiberTypes.h"

class CJobCounter;

// ─────────────────────────────────────────────────────────────
// CJobSystem — Phase 5-A MVP
//  - Worker N 개 (hw_concurrency - 2, 최소 1)
//  - 각 Worker 가 CWorkStealingDeque 1개 소유
//  - Submit: round-robin 으로 Deque 에 Push
//  - WaitForCounter: busy-wait + help-stealing (Main 도 Steal)
//  - Phase 5-B 에서 내부가 Fiber 로 교체되어도 public API 불변
// ─────────────────────────────────────────────────────────────
class WINTERS_ENGINE CJobSystem
{
public:
    CJobSystem();
    ~CJobSystem();

    CJobSystem(const CJobSystem&) = delete;
    CJobSystem& operator=(const CJobSystem&) = delete;

    // 엔진 시작 시 1회 호출. iWorkerCount==0 이면 자동 (hw_concurrency-2)
    void Initialize(std::uint32_t iWorkerCount = 0);
    void Shutdown();
    void SetExecutionMode(eJobExecutionMode eMode);
    eJobExecutionMode GetExecutionMode() const;

    // 레거시 호환 시그니처 — 기존 SystemScheduler/Loader 등에서 사용
    void Submit(std::function<void()> job);
    void Submit(std::function<void()> job, CJobCounter* pCounter);

    // (선택) 함수포인터 버전 — 5-B 용 예비
    void Submit(const JobDecl& decl, CJobCounter* pCounter = nullptr);

    // Counter 가 iTarget 이하가 될 때까지 help-stealing. 블로킹 아님.
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

    struct FiberShellCall
    {
        CJobSystem* pSystem = nullptr;
        WorkItem* pItem = nullptr;
        NativeFiberHandle hReturnFiber = nullptr;
    };

    //이거 맞음?
    void EnqueueJob(WorkItem&& item);

    void  WorkerLoop(std::uint32_t iWorkerIdx);
    bool  TryExecuteOneJob(std::uint32_t iWorkerIdx);
    void  ExecuteItem(WorkItem& item);
    void  ExecuteItemInline(WorkItem& item);
    bool  TryExecuteItemOnFiber(WorkItem& item);
    static void WINTERS_FIBER_CALL FiberShellEntry(void* pParam);
    void  PushToSomeDeque(WorkItem&& item);
    std::uint32_t PickVictim(std::uint32_t iSelf, std::uint32_t N);

    std::vector<std::thread>   m_vecWorkers;
    std::vector<std::unique_ptr<CWorkStealingDeque<WorkItem>>>  m_vecDeques;
    std::atomic<bool> m_bShutdown{ false };
    std::atomic<std::uint32_t> m_iRoundRobin{ 0 };
    std::atomic<eJobExecutionMode> m_eExecutionMode{ eJobExecutionMode::ThreadOnly };

    //Global MPMC Queue + per-worker local deque hybrid
    std::mutex m_GlobalMutex;
    std::queue<WorkItem> m_GlobalQueue;
};
