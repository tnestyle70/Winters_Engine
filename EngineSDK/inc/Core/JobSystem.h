#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "WintersAPI.h"
#include "Core/Fiber/FiberTypes.h"
#include "Core/JobSystem/JobDecl.h"
#include "Core/JobSystem/WorkStealingDeque.h"

class CJobCounter;

struct CJobSystemStats
{
    std::uint64_t uSubmitted = 0;
    std::uint64_t uExecuted = 0;
    std::uint64_t uFailed = 0;
    std::uint64_t uLocalPushes = 0;
    std::uint64_t uGlobalPushes = 0;
    std::uint64_t uSteals = 0;
    // WorkItem executed without a job fiber; this includes normal ThreadOnly execution.
    std::uint64_t uInlineExecutions = 0;
    std::uint64_t uFiberSwitches = 0;
    std::uint64_t uFiberWaits = 0;
    std::uint64_t uFiberResumes = 0;
    std::uint64_t uFiberPoolMisses = 0;
};

// CJobSystem - ThreadOnly + opt-in FiberShell/FiberFull runtime.
// - A worker alone pushes/pops the bottom of its Chase-Lev deque.
// - Main/external submitters and local overflow use the global MPMC queue.
// - ThreadOnly waits help-execute work on the calling stack.
// - FiberFull parks a waiting stack and resumes it through its origin worker inbox.
class WINTERS_ENGINE CJobSystem
{
public:
    CJobSystem();
    ~CJobSystem();

    CJobSystem(const CJobSystem&) = delete;
    CJobSystem& operator=(const CJobSystem&) = delete;

    // Call from an external owner thread. A zero worker count selects
    // hw_concurrency - 2. Lifecycle calls made by this system's jobs are rejected.
    void Initialize(std::uint32_t iWorkerCount = 0);
    void Shutdown();

    // Execution mode is immutable after Initialize and defaults to ThreadOnly.
    void SetExecutionMode(eJobExecutionMode eMode);
    eJobExecutionMode GetExecutionMode() const;
    CJobSystemStats GetStats() const;

    void Submit(std::function<void()> job);
    void Submit(std::function<void()> job, CJobCounter* pCounter);
    void Submit(const JobDecl& decl, CJobCounter* pCounter = nullptr);

    // ThreadOnly/FiberShell help-execute on this stack. FiberFull yields the
    // current job fiber and resumes it on the same worker. The counter must
    // outlive all jobs/waiters. Never cross a FiberFull wait while holding a
    // thread-affine lock or relying on mutable thread_local state.
    void WaitForCounter(CJobCounter* pCounter, std::uint32_t iTarget = 0);

    std::uint32_t GetWorkerCount() const
    {
        return m_iWorkerCount.load(std::memory_order_acquire);
    }

    static std::int32_t Get_WorkerIdx();
    static std::uint32_t Get_WorkerSlot();

private:
    struct WorkItem
    {
        std::function<void()> fn;
        CJobCounter* pCounter = nullptr;
        std::uint64_t uJobId = 0;
    };

    struct FiberRecord;
    struct FiberSchedulerState;

    struct FiberShellCall
    {
        CJobSystem* pSystem = nullptr;
        WorkItem* pItem = nullptr;
        NativeFiberHandle hReturnFiber = nullptr;
    };

    void SubmitFunction(std::function<void()> job, CJobCounter* pCounter);
    void EnqueueJob(std::unique_ptr<WorkItem> pItem);
    void WaitForCounterInternal(CJobCounter* pCounter, std::uint32_t iTarget);
    void BeginSubmission();
    void EndSubmission();
    void PauseSubmissionsAndWait();
    void ResumeSubmissions();
    void ReportWorkerStartup(bool bSuccess, std::exception_ptr exception);

    void WorkerLoop(std::uint32_t iWorkerIdx);
    bool TryExecuteOneJob(std::uint32_t iWorkerIdx);
    bool TryResumeReadyFiber(std::uint32_t iWorkerIdx);
    void ExecuteItem(WorkItem* pItem);
    void ExecuteItemInline(WorkItem* pItem);
    bool TryExecuteItemOnFiberShell(WorkItem* pItem);
    bool TryExecuteItemOnFiberFull(WorkItem* pItem);

    bool InitializeFiberWorker(std::uint32_t iWorkerIdx);
    void ShutdownFiberWorker(std::uint32_t iWorkerIdx);
    void SuspendCurrentFiber(CJobCounter* pCounter, std::uint32_t iTarget);
    void NotifyCounterChanged(
        CJobCounter* pCounter,
        std::uint32_t iRemaining) noexcept;
    bool HasWaitingFiber(std::uint32_t iWorkerIdx) const;

    static void WINTERS_FIBER_CALL FiberShellEntry(void* pParam);
    static void WINTERS_FIBER_CALL FiberFullEntry(void* pParam);
    std::uint32_t PickVictim(std::uint32_t iSelf, std::uint32_t iWorkerCount);

    std::vector<std::thread> m_vecWorkers;
    std::vector<std::unique_ptr<CWorkStealingDeque<WorkItem*>>> m_vecDeques;
    std::unique_ptr<FiberSchedulerState> m_pFiberScheduler;

    const std::uint64_t m_uSystemId;
    std::atomic<bool> m_bInitialized{ false };
    std::atomic<bool> m_bShutdown{ true };
    std::atomic<std::uint32_t> m_iWorkerCount{ 0 };
    std::atomic<std::uint32_t> m_iRoundRobin{ 0 };
    std::atomic<eJobExecutionMode> m_eExecutionMode{ eJobExecutionMode::ThreadOnly };
    std::atomic<std::uint64_t> m_uNextJobId{ 1 };

    // Lifecycle operations are single-owner. Submission admission separately
    // pins deque/fiber state until each Submit has published or run inline.
    std::mutex m_LifecycleMutex;
    std::mutex m_SubmitMutex;
    std::condition_variable m_SubmitCV;
    bool m_bSubmissionsPaused = false;
    std::uint32_t m_iActiveSubmissions = 0;
    std::mutex m_GlobalMutex;
    std::queue<WorkItem*> m_GlobalQueue;

    std::mutex m_WakeMutex;
    std::condition_variable m_WakeCV;

    std::mutex m_StartupMutex;
    std::condition_variable m_StartupCV;
    std::uint32_t m_iWorkersReportedStarted = 0;
    bool m_bWorkerStartupFailed = false;
    std::exception_ptr m_WorkerStartupException;

    std::atomic<std::uint64_t> m_uSubmitted{ 0 };
    std::atomic<std::uint64_t> m_uExecuted{ 0 };
    std::atomic<std::uint64_t> m_uFailed{ 0 };
    std::atomic<std::uint64_t> m_uLocalPushes{ 0 };
    std::atomic<std::uint64_t> m_uGlobalPushes{ 0 };
    std::atomic<std::uint64_t> m_uSteals{ 0 };
    std::atomic<std::uint64_t> m_uInlineExecutions{ 0 };
    std::atomic<std::uint64_t> m_uFiberSwitches{ 0 };
    std::atomic<std::uint64_t> m_uFiberWaits{ 0 };
    std::atomic<std::uint64_t> m_uFiberResumes{ 0 };
    std::atomic<std::uint64_t> m_uFiberPoolMisses{ 0 };
};
