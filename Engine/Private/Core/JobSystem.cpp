#include "WintersPCH.h"
#include "Core/JobSystem.h"
#include "Core/JobCounter.h"
#include "Core/Fiber/FiberPool.h"
#include "ProfilerAPI.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <deque>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace
{
    enum class eFiberRunState : std::uint8_t
    {
        Free,
        Running,
        Waiting,
        Ready,
    };

    constexpr std::uint32_t kFibersPerWorker = 64;
    constexpr SIZE_T kFiberStackCommitBytes = 64u * 1024u;
    constexpr SIZE_T kFiberStackReserveBytes = 256u * 1024u;

    thread_local std::int32_t t_iWorkerIdx = -1;
    thread_local CJobSystem* t_pOwningJobSystem = nullptr;
    thread_local NativeFiberHandle t_hThreadFiber = nullptr;
    thread_local bool t_bConvertedThreadToFiber = false;
    thread_local bool t_bInsideJobFiber = false;
    thread_local void* t_pCurrentFiberRecord = nullptr;
    std::atomic<std::uint64_t> g_uNextJobSystemId{ 1 };

    struct SubmissionTlsNode
    {
        CJobSystem* pSystem = nullptr;
        SubmissionTlsNode* pPrevious = nullptr;
    };

    thread_local SubmissionTlsNode* t_pSubmissionFallbackNode = nullptr;

    DWORD GetSubmissionFlsIndex()
    {
        // Process-lifetime slot. Values point at active call-stack nodes and must
        // follow Windows fiber switches; no destructor callback owns those nodes.
        static const DWORD index = FlsAlloc(nullptr);
        return index;
    }

    SubmissionTlsNode* GetSubmissionNode()
    {
        const DWORD index = GetSubmissionFlsIndex();
        if (index == FLS_OUT_OF_INDEXES)
            return t_pSubmissionFallbackNode;
        return static_cast<SubmissionTlsNode*>(FlsGetValue(index));
    }

    void SetSubmissionNode(SubmissionTlsNode* pNode)
    {
        const DWORD index = GetSubmissionFlsIndex();
        if (index == FLS_OUT_OF_INDEXES)
        {
            t_pSubmissionFallbackNode = pNode;
            return;
        }
        if (!FlsSetValue(index, pNode))
        {
            OutputDebugStringA(
                "[JobSystem] fatal: failed to set fiber-local submission context.\n");
            std::terminate();
        }
    }

#ifdef WINTERS_PROFILING
    struct StableFiberNameRegistry
    {
        std::mutex Mutex;
        std::unordered_map<std::string, std::string> Names;
    };

    const char* GetStableFiberName(
        std::uint64_t uSystemId,
        std::uint32_t iWorkerIdx,
        std::uint32_t iFiberSlot)
    {
        // Tracy queues the pointer and may query it after the native fiber was
        // deleted. Keep this tiny registry for the DLL process lifetime; keys are
        // reused across each JobSystem's reinitialization. The system id prevents
        // two live schedulers from collapsing to one Tracy fiber identity.
        static StableFiberNameRegistry& registry =
            *new StableFiberNameRegistry();
        const std::string name =
            "JobFiber " + std::to_string(uSystemId) + ":" +
            std::to_string(iWorkerIdx) + ":" + std::to_string(iFiberSlot);
        std::lock_guard<std::mutex> nameLock(registry.Mutex);
        const auto [it, bInserted] = registry.Names.try_emplace(
            name,
            name);
        (void)bInserted;
        return it->second.c_str();
    }
#endif

    bool IsCurrentThreadSubmittingTo(CJobSystem* pSystem)
    {
        for (SubmissionTlsNode* pNode = GetSubmissionNode();
             pNode;
             pNode = pNode->pPrevious)
        {
            if (pNode->pSystem == pSystem)
                return true;
        }
        return false;
    }

    void EmitJobFailure(const char* pMessage, std::uint64_t uJobId)
    {
        char szMessage[256]{};
        std::snprintf(
            szMessage,
            sizeof(szMessage),
            "[JobSystem] job %llu failed: %s\n",
            static_cast<unsigned long long>(uJobId),
            pMessage ? pMessage : "unknown exception");
        OutputDebugStringA(szMessage);
    }
}

struct CJobSystem::FiberRecord
{
    CJobSystem* pSystem = nullptr;
    std::uint32_t iOwnerWorker = 0;
    std::uint64_t uFiberId = 0;
    NativeFiberHandle hFiber = nullptr;
    NativeFiberHandle hRootFiber = nullptr;
    WorkItem* pItem = nullptr;
    CJobCounter* pWaitCounter = nullptr;
    std::uint32_t iWaitTarget = 0;
    std::atomic<eFiberRunState> eState{ eFiberRunState::Free };
#ifdef WINTERS_PROFILING
    const char* pProfilerName = nullptr;
#endif
};

struct CJobSystem::FiberSchedulerState
{
    struct WorkerState
    {
        NativeFiberHandle hRootFiber = nullptr;
        CFiberPool Pool{};
        std::vector<std::unique_ptr<FiberRecord>> vecRecords;
        mutable std::mutex ReadyMutex;
        std::array<FiberRecord*, kFibersPerWorker> ReadyFibers{};
        std::uint32_t iReadyHead = 0;
        std::uint32_t iReadyTail = 0;
        std::uint32_t iReadyCount = 0;
    };

    std::vector<std::unique_ptr<WorkerState>> vecWorkerStates;
    std::mutex WaiterMutex;
    std::unordered_map<CJobCounter*, std::vector<FiberRecord*>> Waiters;
    std::atomic<std::uint64_t> uNextFiberId{ 1 };
};

CJobSystem::CJobSystem()
    : m_uSystemId(g_uNextJobSystemId.fetch_add(1, std::memory_order_relaxed))
{
}

CJobSystem::~CJobSystem()
{
    Shutdown();
}

std::int32_t CJobSystem::Get_WorkerIdx()
{
    return t_iWorkerIdx;
}

std::uint32_t CJobSystem::Get_WorkerSlot()
{
    return (t_iWorkerIdx < 0)
        ? 0u
        : static_cast<std::uint32_t>(t_iWorkerIdx + 1);
}

void CJobSystem::Initialize(std::uint32_t iWorkerCount)
{
    if (t_pOwningJobSystem == this || IsCurrentThreadSubmittingTo(this))
    {
        OutputDebugStringA(
            "[JobSystem] Initialize rejected from this system's job/submission.\n");
        return;
    }

    std::lock_guard<std::mutex> lifecycleLock(m_LifecycleMutex);
    if (m_bInitialized.load(std::memory_order_acquire))
        return;

    PauseSubmissionsAndWait();

    if (iWorkerCount == 0)
    {
        const std::uint32_t iHardwareThreads = std::thread::hardware_concurrency();
        iWorkerCount = (iHardwareThreads > 2) ? (iHardwareThreads - 2) : 1u;
    }

    try
    {
        m_vecWorkers.clear();
        m_vecDeques.clear();
        m_vecDeques.reserve(iWorkerCount);
        for (std::uint32_t i = 0; i < iWorkerCount; ++i)
        {
            m_vecDeques.push_back(
                std::make_unique<CWorkStealingDeque<WorkItem*>>());
        }

        if (GetExecutionMode() == eJobExecutionMode::FiberFull)
        {
            if (GetSubmissionFlsIndex() == FLS_OUT_OF_INDEXES)
            {
                throw std::runtime_error(
                    "FiberFull requires a Windows fiber-local storage slot");
            }
            m_pFiberScheduler = std::make_unique<FiberSchedulerState>();
            m_pFiberScheduler->vecWorkerStates.reserve(iWorkerCount);
            for (std::uint32_t i = 0; i < iWorkerCount; ++i)
            {
                m_pFiberScheduler->vecWorkerStates.push_back(
                    std::make_unique<FiberSchedulerState::WorkerState>());
            }
        }
        else
        {
            m_pFiberScheduler.reset();
        }

        m_iRoundRobin.store(0, std::memory_order_relaxed);
        m_bShutdown.store(false, std::memory_order_release);
        m_bInitialized.store(true, std::memory_order_release);

        {
            std::lock_guard<std::mutex> startupLock(m_StartupMutex);
            m_iWorkersReportedStarted = 0;
            m_bWorkerStartupFailed = false;
            m_WorkerStartupException = nullptr;
        }

        m_vecWorkers.reserve(iWorkerCount);
        for (std::uint32_t i = 0; i < iWorkerCount; ++i)
            m_vecWorkers.emplace_back(&CJobSystem::WorkerLoop, this, i);

        std::exception_ptr startupException;
        bool bStartupFailed = false;
        {
            std::unique_lock<std::mutex> startupLock(m_StartupMutex);
            m_StartupCV.wait(startupLock, [this, iWorkerCount]() {
                return m_iWorkersReportedStarted == iWorkerCount;
            });
            bStartupFailed = m_bWorkerStartupFailed;
            startupException = m_WorkerStartupException;
        }
        if (bStartupFailed)
        {
            if (startupException)
                std::rethrow_exception(startupException);
            throw std::runtime_error("one or more JobSystem workers failed startup");
        }
        m_iWorkerCount.store(iWorkerCount, std::memory_order_release);
    }
    catch (...)
    {
        m_bInitialized.store(false, std::memory_order_release);
        m_bShutdown.store(true, std::memory_order_release);
        m_WakeCV.notify_all();
        for (std::thread& worker : m_vecWorkers)
        {
            if (worker.joinable())
                worker.join();
        }
        m_vecWorkers.clear();
        m_iWorkerCount.store(0, std::memory_order_release);
        m_vecDeques.clear();
        m_pFiberScheduler.reset();
        ResumeSubmissions();
        throw;
    }

    ResumeSubmissions();
}

void CJobSystem::Shutdown()
{
    if (t_pOwningJobSystem == this || IsCurrentThreadSubmittingTo(this))
    {
        OutputDebugStringA(
            "[JobSystem] Shutdown rejected from this system's job/submission.\n");
        return;
    }

    std::lock_guard<std::mutex> lifecycleLock(m_LifecycleMutex);
    bool bAlreadyStopped = false;
    {
        std::lock_guard<std::mutex> submitLock(m_SubmitMutex);
        bAlreadyStopped =
            !m_bInitialized.load(std::memory_order_acquire) && m_vecWorkers.empty();
        if (!bAlreadyStopped)
        {
            // Under the same lock as EnqueueJob's decision, work is either published
            // before this boundary or observes stopped state and executes inline.
            m_bInitialized.store(false, std::memory_order_release);
            m_bShutdown.store(true, std::memory_order_release);
        }
    }

    if (bAlreadyStopped)
    {
        // A stopped system still permits synchronous inline Submit. Quiesce calls
        // already in flight so a repeated Shutdown has a real completion boundary.
        PauseSubmissionsAndWait();
        ResumeSubmissions();
        return;
    }

    m_WakeCV.notify_all();
    for (std::thread& worker : m_vecWorkers)
    {
        if (worker.joinable())
            worker.join();
    }
    m_vecWorkers.clear();
    m_iWorkerCount.store(0, std::memory_order_release);

    // Defensive drain. A valid run reaches here with all accepted queues empty.
    for (;;)
    {
        WorkItem* pItem = nullptr;
        {
            std::lock_guard<std::mutex> queueLock(m_GlobalMutex);
            if (m_GlobalQueue.empty())
                break;
            pItem = m_GlobalQueue.front();
            m_GlobalQueue.pop();
        }
        m_uInlineExecutions.fetch_add(1, std::memory_order_relaxed);
        ExecuteItemInline(pItem);
    }

    for (const auto& pDeque : m_vecDeques)
    {
        WorkItem* pItem = nullptr;
        while (pDeque && pDeque->Steal(pItem))
        {
            m_uInlineExecutions.fetch_add(1, std::memory_order_relaxed);
            ExecuteItemInline(pItem);
        }
    }

    // The workers are gone and all pre-boundary queues are drained. Pause new
    // inline submissions and wait for admitted calls before destroying notifier state.
    PauseSubmissionsAndWait();
    m_vecDeques.clear();
    m_pFiberScheduler.reset();
    ResumeSubmissions();
}

void CJobSystem::SetExecutionMode(eJobExecutionMode eMode)
{
    if (t_pOwningJobSystem == this || IsCurrentThreadSubmittingTo(this))
    {
        OutputDebugStringA(
            "[JobSystem] SetExecutionMode rejected from this system's job/submission.\n");
        return;
    }

    std::lock_guard<std::mutex> lifecycleLock(m_LifecycleMutex);
    std::lock_guard<std::mutex> submitLock(m_SubmitMutex);
    if (m_bInitialized.load(std::memory_order_acquire))
    {
        OutputDebugStringA("[JobSystem] SetExecutionMode ignored after Initialize.\n");
        return;
    }
    m_eExecutionMode.store(eMode, std::memory_order_release);
}

eJobExecutionMode CJobSystem::GetExecutionMode() const
{
    return m_eExecutionMode.load(std::memory_order_acquire);
}

CJobSystemStats CJobSystem::GetStats() const
{
    CJobSystemStats stats{};
    stats.uSubmitted = m_uSubmitted.load(std::memory_order_relaxed);
    stats.uExecuted = m_uExecuted.load(std::memory_order_relaxed);
    stats.uFailed = m_uFailed.load(std::memory_order_relaxed);
    stats.uLocalPushes = m_uLocalPushes.load(std::memory_order_relaxed);
    stats.uGlobalPushes = m_uGlobalPushes.load(std::memory_order_relaxed);
    stats.uSteals = m_uSteals.load(std::memory_order_relaxed);
    stats.uInlineExecutions = m_uInlineExecutions.load(std::memory_order_relaxed);
    stats.uFiberSwitches = m_uFiberSwitches.load(std::memory_order_relaxed);
    stats.uFiberWaits = m_uFiberWaits.load(std::memory_order_relaxed);
    stats.uFiberResumes = m_uFiberResumes.load(std::memory_order_relaxed);
    stats.uFiberPoolMisses = m_uFiberPoolMisses.load(std::memory_order_relaxed);
    return stats;
}

void CJobSystem::Submit(std::function<void()> job)
{
    SubmitFunction(std::move(job), nullptr);
}

void CJobSystem::Submit(std::function<void()> job, CJobCounter* pCounter)
{
    SubmitFunction(std::move(job), pCounter);
}

void CJobSystem::Submit(const JobDecl& decl, CJobCounter* pCounter)
{
    const JobFn pFunction = decl.pFn;
    void* const pData = decl.pData;
    SubmitFunction([pFunction, pData]() {
        if (pFunction)
            pFunction(pData);
    }, pCounter);
}

void CJobSystem::SubmitFunction(std::function<void()> job, CJobCounter* pCounter)
{
    auto pItem = std::make_unique<WorkItem>();
    pItem->fn = std::move(job);
    pItem->pCounter = pCounter;
    pItem->uJobId = m_uNextJobId.fetch_add(1, std::memory_order_relaxed);

    const bool bOwnsAdmission = !IsCurrentThreadSubmittingTo(this);
    if (bOwnsAdmission)
        BeginSubmission();

    SubmissionTlsNode submissionNode{ this, GetSubmissionNode() };
    SetSubmissionNode(&submissionNode);
    bool bCounterIncremented = false;

    try
    {
        if (pCounter)
        {
            pCounter->Increment();
            bCounterIncremented = true;
        }
        EnqueueJob(std::move(pItem));
    }
    catch (...)
    {
        if (bCounterIncremented)
        {
            std::uint32_t iRemaining = 0;
            if (pCounter->TryDecrement(iRemaining))
                NotifyCounterChanged(pCounter, iRemaining);
        }
        SetSubmissionNode(submissionNode.pPrevious);
        if (bOwnsAdmission)
            EndSubmission();
        throw;
    }

    SetSubmissionNode(submissionNode.pPrevious);
    if (bOwnsAdmission)
        EndSubmission();
}

void CJobSystem::EnqueueJob(std::unique_ptr<WorkItem> pItem)
{
    if (!pItem)
        return;

    WorkItem* const pRawItem = pItem.get();
    bool bExecuteInline = false;
    {
        std::lock_guard<std::mutex> submitLock(m_SubmitMutex);
        if (!m_bInitialized.load(std::memory_order_acquire) ||
            m_bShutdown.load(std::memory_order_acquire) ||
            m_vecDeques.empty())
        {
            bExecuteInline = true;
        }
        else
        {
            const std::uint32_t iWorkerCount =
                static_cast<std::uint32_t>(m_vecDeques.size());
            if (t_pOwningJobSystem == this &&
                t_iWorkerIdx >= 0 &&
                static_cast<std::uint32_t>(t_iWorkerIdx) < iWorkerCount &&
                m_vecDeques[static_cast<std::uint32_t>(t_iWorkerIdx)]->Push(pRawItem))
            {
                pItem.release();
                m_uSubmitted.fetch_add(1, std::memory_order_relaxed);
                m_uLocalPushes.fetch_add(1, std::memory_order_relaxed);
                m_WakeCV.notify_one();
                return;
            }

            {
                std::lock_guard<std::mutex> queueLock(m_GlobalMutex);
                m_GlobalQueue.push(pRawItem);
            }
            pItem.release();
            m_uSubmitted.fetch_add(1, std::memory_order_relaxed);
            m_uGlobalPushes.fetch_add(1, std::memory_order_relaxed);
        }
    }

    if (bExecuteInline)
    {
        m_uSubmitted.fetch_add(1, std::memory_order_relaxed);
        m_uInlineExecutions.fetch_add(1, std::memory_order_relaxed);
        ExecuteItemInline(pItem.release());
        return;
    }

    m_WakeCV.notify_one();
}

void CJobSystem::BeginSubmission()
{
    std::unique_lock<std::mutex> submitLock(m_SubmitMutex);
    m_SubmitCV.wait(submitLock, [this]() { return !m_bSubmissionsPaused; });
    ++m_iActiveSubmissions;
}

void CJobSystem::EndSubmission()
{
    std::lock_guard<std::mutex> submitLock(m_SubmitMutex);
    if (m_iActiveSubmissions > 0)
        --m_iActiveSubmissions;
    if (m_iActiveSubmissions == 0)
        m_SubmitCV.notify_all();
}

void CJobSystem::PauseSubmissionsAndWait()
{
    std::unique_lock<std::mutex> submitLock(m_SubmitMutex);
    m_bSubmissionsPaused = true;
    m_SubmitCV.wait(
        submitLock,
        [this]() { return m_iActiveSubmissions == 0; });
}

void CJobSystem::ResumeSubmissions()
{
    {
        std::lock_guard<std::mutex> submitLock(m_SubmitMutex);
        m_bSubmissionsPaused = false;
    }
    m_SubmitCV.notify_all();
}

void CJobSystem::ReportWorkerStartup(
    bool bSuccess,
    std::exception_ptr exception)
{
    {
        std::lock_guard<std::mutex> startupLock(m_StartupMutex);
        ++m_iWorkersReportedStarted;
        if (!bSuccess)
        {
            m_bWorkerStartupFailed = true;
            if (!m_WorkerStartupException)
                m_WorkerStartupException = std::move(exception);
        }
    }
    m_StartupCV.notify_all();
}

void CJobSystem::WorkerLoop(std::uint32_t iWorkerIdx)
{
    t_iWorkerIdx = static_cast<std::int32_t>(iWorkerIdx);
    t_pOwningJobSystem = this;

    const eJobExecutionMode eMode = GetExecutionMode();
    auto cleanupWorker = [this, iWorkerIdx, eMode]() {
        if (eMode == eJobExecutionMode::FiberFull)
            ShutdownFiberWorker(iWorkerIdx);

        if (t_bConvertedThreadToFiber && t_hThreadFiber && IsThreadAFiber())
            ConvertFiberToThread();

        t_pCurrentFiberRecord = nullptr;
        t_bInsideJobFiber = false;
        t_bConvertedThreadToFiber = false;
        t_hThreadFiber = nullptr;
        t_pOwningJobSystem = nullptr;
        t_iWorkerIdx = -1;
    };

    bool bStartupSuccess = true;
    std::exception_ptr startupException;
    try
    {

#ifdef WINTERS_PROFILING
        char szThreadName[32]{};
        std::snprintf(szThreadName, sizeof(szThreadName), "JobWorker %u", iWorkerIdx);
        WINTERS_PROFILE_THREAD_NAME(szThreadName);
#endif

        if (eMode != eJobExecutionMode::ThreadOnly)
        {
            if (IsThreadAFiber())
            {
                t_hThreadFiber = GetCurrentFiber();
            }
            else
            {
                t_hThreadFiber = ConvertThreadToFiberEx(
                    nullptr,
                    FIBER_FLAG_FLOAT_SWITCH);
                t_bConvertedThreadToFiber = (t_hThreadFiber != nullptr);
            }
            if (!t_hThreadFiber)
            {
                throw std::runtime_error(
                    "ConvertThreadToFiberEx failed during JobSystem startup");
            }
        }
        if (eMode == eJobExecutionMode::FiberFull &&
            !InitializeFiberWorker(iWorkerIdx))
        {
            throw std::runtime_error(
                "FiberFull worker could not create its complete fiber pool");
        }
    }
    catch (...)
    {
        bStartupSuccess = false;
        startupException = std::current_exception();
    }

    ReportWorkerStartup(bStartupSuccess, startupException);
    if (!bStartupSuccess)
    {
        cleanupWorker();
        return;
    }

    for (;;)
    {
        bool bDidWork = false;
        if (eMode == eJobExecutionMode::FiberFull)
            bDidWork = TryResumeReadyFiber(iWorkerIdx);
        if (!bDidWork)
            bDidWork = TryExecuteOneJob(iWorkerIdx);
        if (bDidWork)
            continue;

        if (m_bShutdown.load(std::memory_order_acquire) &&
            (eMode != eJobExecutionMode::FiberFull || !HasWaitingFiber(iWorkerIdx)))
        {
            break;
        }

        std::unique_lock<std::mutex> wakeLock(m_WakeMutex);
        m_WakeCV.wait_for(wakeLock, std::chrono::milliseconds(1));
    }

    cleanupWorker();
}

std::uint32_t CJobSystem::PickVictim(
    std::uint32_t iSelf,
    std::uint32_t iWorkerCount)
{
    thread_local std::uint32_t iRandomState =
        static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(&iSelf) ^ iSelf ^ 0x9E3779B9u);
    iRandomState ^= iRandomState << 13;
    iRandomState ^= iRandomState >> 17;
    iRandomState ^= iRandomState << 5;
    std::uint32_t iVictim = iRandomState % iWorkerCount;
    if (iVictim == iSelf)
        iVictim = (iVictim + 1) % iWorkerCount;
    return iVictim;
}

bool CJobSystem::TryExecuteOneJob(std::uint32_t iWorkerIdx)
{
    WorkItem* pItem = nullptr;
    if (iWorkerIdx < m_vecDeques.size() && m_vecDeques[iWorkerIdx]->Pop(pItem))
    {
        ExecuteItem(pItem);
        return true;
    }

    {
        std::lock_guard<std::mutex> queueLock(m_GlobalMutex);
        if (!m_GlobalQueue.empty())
        {
            pItem = m_GlobalQueue.front();
            m_GlobalQueue.pop();
        }
    }
    if (pItem)
    {
        ExecuteItem(pItem);
        return true;
    }

    const std::uint32_t iWorkerCount =
        static_cast<std::uint32_t>(m_vecDeques.size());
    if (iWorkerCount > 1)
    {
        const std::uint32_t iVictim = PickVictim(iWorkerIdx, iWorkerCount);
        if (m_vecDeques[iVictim]->Steal(pItem))
        {
            m_uSteals.fetch_add(1, std::memory_order_relaxed);
            ExecuteItem(pItem);
            return true;
        }
    }
    return false;
}

void CJobSystem::ExecuteItem(WorkItem* pItem)
{
    if (!pItem)
        return;

    const eJobExecutionMode eMode = GetExecutionMode();
    if (t_pOwningJobSystem == this && t_iWorkerIdx >= 0)
    {
        if (eMode == eJobExecutionMode::FiberFull &&
            !t_pCurrentFiberRecord &&
            TryExecuteItemOnFiberFull(pItem))
        {
            return;
        }
        if (eMode == eJobExecutionMode::FiberShell &&
            !t_bInsideJobFiber &&
            TryExecuteItemOnFiberShell(pItem))
        {
            return;
        }
    }

    m_uInlineExecutions.fetch_add(1, std::memory_order_relaxed);
    ExecuteItemInline(pItem);
}

void CJobSystem::ExecuteItemInline(WorkItem* pRawItem)
{
    std::unique_ptr<WorkItem> pItem(pRawItem);
    if (!pItem)
        return;

    bool bFailed = false;
    try
    {
        if (pItem->fn)
            pItem->fn();
    }
    catch (const std::exception& exception)
    {
        bFailed = true;
        EmitJobFailure(exception.what(), pItem->uJobId);
    }
    catch (...)
    {
        bFailed = true;
        EmitJobFailure("non-std exception", pItem->uJobId);
    }

    CJobCounter* const pCounter = pItem->pCounter;
    pItem.reset();

    if (bFailed)
        m_uFailed.fetch_add(1, std::memory_order_relaxed);
    m_uExecuted.fetch_add(1, std::memory_order_relaxed);

    if (pCounter)
    {
        std::uint32_t iRemaining = 0;
        if (pCounter->TryDecrement(iRemaining))
        {
            NotifyCounterChanged(pCounter, iRemaining);
        }
        else
        {
            m_uFailed.fetch_add(1, std::memory_order_relaxed);
            OutputDebugStringA("[JobSystem] counter underflow prevented.\n");
        }
    }
}

bool CJobSystem::TryExecuteItemOnFiberShell(WorkItem* pItem)
{
    if (!t_hThreadFiber)
        return false;

    FiberShellCall call{};
    call.pSystem = this;
    call.pItem = pItem;
    call.hReturnFiber = t_hThreadFiber;

    NativeFiberHandle hJobFiber = CreateFiberEx(
        kFiberStackCommitBytes,
        kFiberStackReserveBytes,
        FIBER_FLAG_FLOAT_SWITCH,
        &CJobSystem::FiberShellEntry,
        &call);
    if (!hJobFiber)
        return false;

    m_uFiberSwitches.fetch_add(1, std::memory_order_relaxed);
    SwitchToFiber(hJobFiber);
    DeleteFiber(hJobFiber);
    return true;
}

void WINTERS_FIBER_CALL CJobSystem::FiberShellEntry(void* pParam)
{
    FiberShellCall* const pCall = static_cast<FiberShellCall*>(pParam);
    t_bInsideJobFiber = true;
    if (pCall && pCall->pSystem && pCall->pItem)
        pCall->pSystem->ExecuteItemInline(pCall->pItem);
    t_bInsideJobFiber = false;
    if (pCall && pCall->hReturnFiber)
    {
        if (pCall->pSystem)
        {
            pCall->pSystem->m_uFiberSwitches.fetch_add(
                1,
                std::memory_order_relaxed);
        }
        SwitchToFiber(pCall->hReturnFiber);
    }
}

bool CJobSystem::InitializeFiberWorker(std::uint32_t iWorkerIdx)
{
    if (!m_pFiberScheduler ||
        iWorkerIdx >= m_pFiberScheduler->vecWorkerStates.size() ||
        !t_hThreadFiber)
    {
        return false;
    }

    FiberSchedulerState::WorkerState& worker =
        *m_pFiberScheduler->vecWorkerStates[iWorkerIdx];
    worker.hRootFiber = t_hThreadFiber;
    worker.Pool.Reserve(kFibersPerWorker);
    worker.vecRecords.reserve(kFibersPerWorker);

    for (std::uint32_t i = 0; i < kFibersPerWorker; ++i)
    {
        auto pRecord = std::make_unique<FiberRecord>();
        pRecord->pSystem = this;
        pRecord->iOwnerWorker = iWorkerIdx;
        pRecord->uFiberId =
            m_pFiberScheduler->uNextFiberId.fetch_add(1, std::memory_order_relaxed);
#ifdef WINTERS_PROFILING
        pRecord->pProfilerName = GetStableFiberName(m_uSystemId, iWorkerIdx, i);
#endif
        pRecord->hRootFiber = t_hThreadFiber;
        pRecord->hFiber = CreateFiberEx(
            kFiberStackCommitBytes,
            kFiberStackReserveBytes,
            FIBER_FLAG_FLOAT_SWITCH,
            &CJobSystem::FiberFullEntry,
            pRecord.get());
        if (!pRecord->hFiber)
            break;

        worker.Pool.Add(pRecord->hFiber);
        worker.vecRecords.push_back(std::move(pRecord));
    }
    return worker.Pool.GetCount() == kFibersPerWorker;
}

bool CJobSystem::TryExecuteItemOnFiberFull(WorkItem* pItem)
{
    if (!m_pFiberScheduler || t_iWorkerIdx < 0)
        return false;

    const std::uint32_t iWorkerIdx = static_cast<std::uint32_t>(t_iWorkerIdx);
    if (iWorkerIdx >= m_pFiberScheduler->vecWorkerStates.size())
        return false;

    FiberSchedulerState::WorkerState& worker =
        *m_pFiberScheduler->vecWorkerStates[iWorkerIdx];
    const NativeFiberHandle hFiber = worker.Pool.Acquire();
    if (!hFiber)
    {
        m_uFiberPoolMisses.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    FiberRecord* pRecord = nullptr;
    for (const auto& candidate : worker.vecRecords)
    {
        if (candidate->hFiber == hFiber)
        {
            pRecord = candidate.get();
            break;
        }
    }
    if (!pRecord)
    {
        worker.Pool.Release(hFiber);
        return false;
    }

    pRecord->pItem = pItem;
    pRecord->pWaitCounter = nullptr;
    pRecord->eState.store(eFiberRunState::Running, std::memory_order_release);
    t_pCurrentFiberRecord = pRecord;
    m_uFiberSwitches.fetch_add(1, std::memory_order_relaxed);
    SwitchToFiber(hFiber);
    t_pCurrentFiberRecord = nullptr;

    if (pRecord->eState.load(std::memory_order_acquire) == eFiberRunState::Free)
        worker.Pool.Release(hFiber);
    return true;
}

void WINTERS_FIBER_CALL CJobSystem::FiberFullEntry(void* pParam)
{
    FiberRecord* const pRecord = static_cast<FiberRecord*>(pParam);
    if (!pRecord || !pRecord->pSystem || !pRecord->hRootFiber)
        return;

    for (;;)
    {
#ifdef WINTERS_PROFILING
        TracyFiberEnter(pRecord->pProfilerName);
#endif
        t_pCurrentFiberRecord = pRecord;
        t_bInsideJobFiber = true;
        if (pRecord->pItem)
        {
            WorkItem* const pItem = pRecord->pItem;
            pRecord->pItem = nullptr;
            pRecord->pSystem->ExecuteItemInline(pItem);
        }
        t_bInsideJobFiber = false;
        pRecord->eState.store(eFiberRunState::Free, std::memory_order_release);
#ifdef WINTERS_PROFILING
        TracyFiberLeave;
#endif
        pRecord->pSystem->m_uFiberSwitches.fetch_add(
            1,
            std::memory_order_relaxed);
        SwitchToFiber(pRecord->hRootFiber);
    }
}

bool CJobSystem::TryResumeReadyFiber(std::uint32_t iWorkerIdx)
{
    if (!m_pFiberScheduler ||
        iWorkerIdx >= m_pFiberScheduler->vecWorkerStates.size())
    {
        return false;
    }

    FiberSchedulerState::WorkerState& worker =
        *m_pFiberScheduler->vecWorkerStates[iWorkerIdx];
    FiberRecord* pRecord = nullptr;
    {
        std::lock_guard<std::mutex> readyLock(worker.ReadyMutex);
        if (worker.iReadyCount == 0)
            return false;
        pRecord = worker.ReadyFibers[worker.iReadyHead];
        worker.ReadyFibers[worker.iReadyHead] = nullptr;
        worker.iReadyHead =
            (worker.iReadyHead + 1u) % kFibersPerWorker;
        --worker.iReadyCount;
    }

    eFiberRunState eExpected = eFiberRunState::Ready;
    if (!pRecord || !pRecord->eState.compare_exchange_strong(
        eExpected,
        eFiberRunState::Running,
        std::memory_order_acq_rel,
        std::memory_order_acquire))
    {
        return false;
    }

    t_pCurrentFiberRecord = pRecord;
    m_uFiberResumes.fetch_add(1, std::memory_order_relaxed);
    m_uFiberSwitches.fetch_add(1, std::memory_order_relaxed);
    SwitchToFiber(pRecord->hFiber);
    t_pCurrentFiberRecord = nullptr;

    if (pRecord->eState.load(std::memory_order_acquire) == eFiberRunState::Free)
        worker.Pool.Release(pRecord->hFiber);
    return true;
}

void CJobSystem::SuspendCurrentFiber(
    CJobCounter* pCounter,
    std::uint32_t iTarget)
{
    FiberRecord* const pRecord = static_cast<FiberRecord*>(t_pCurrentFiberRecord);
    if (!pRecord || pRecord->pSystem != this || !m_pFiberScheduler)
        return;

    {
        std::lock_guard<std::mutex> waiterLock(m_pFiberScheduler->WaiterMutex);
        if (pCounter->Load() <= iTarget)
            return;

        // Allocate/register first. If the map/vector throws, the fiber remains
        // Running and the job exception path can complete normally without a lost waiter.
        auto [waiterIt, bInserted] =
            m_pFiberScheduler->Waiters.try_emplace(pCounter);
        try
        {
            waiterIt->second.push_back(pRecord);
        }
        catch (...)
        {
            if (bInserted && waiterIt->second.empty())
                m_pFiberScheduler->Waiters.erase(waiterIt);
            throw;
        }
        pRecord->pWaitCounter = pCounter;
        pRecord->iWaitTarget = iTarget;
        pRecord->eState.store(eFiberRunState::Waiting, std::memory_order_release);
        m_uFiberWaits.fetch_add(1, std::memory_order_relaxed);
    }

    // No scheduler/waiter or other thread-affine lock may cross this switch.
#ifdef WINTERS_PROFILING
    TracyFiberLeave;
#endif
    m_uFiberSwitches.fetch_add(1, std::memory_order_relaxed);
    SwitchToFiber(pRecord->hRootFiber);
#ifdef WINTERS_PROFILING
    TracyFiberEnter(pRecord->pProfilerName);
#endif
    pRecord->pWaitCounter = nullptr;
}

void CJobSystem::NotifyCounterChanged(
    CJobCounter* pCounter,
    std::uint32_t iRemaining) noexcept
{
    if (!pCounter || !m_pFiberScheduler)
        return;

    bool bQueuedReadyFiber = false;
    {
        std::lock_guard<std::mutex> waiterLock(m_pFiberScheduler->WaiterMutex);
        const auto waiterIt = m_pFiberScheduler->Waiters.find(pCounter);
        if (waiterIt == m_pFiberScheduler->Waiters.end())
            return;

        std::vector<FiberRecord*>& waiters = waiterIt->second;
        auto it = waiters.begin();
        while (it != waiters.end())
        {
            FiberRecord* const pRecord = *it;
            if (pRecord && iRemaining <= pRecord->iWaitTarget)
            {
                if (pRecord->iOwnerWorker >=
                    m_pFiberScheduler->vecWorkerStates.size())
                {
                    OutputDebugStringA(
                        "[JobSystem] fatal: waiter has an invalid owner worker.\n");
                    std::terminate();
                }
                FiberSchedulerState::WorkerState& worker =
                    *m_pFiberScheduler->vecWorkerStates[pRecord->iOwnerWorker];
                std::lock_guard<std::mutex> readyLock(worker.ReadyMutex);
                if (worker.iReadyCount >= kFibersPerWorker)
                {
                    OutputDebugStringA(
                        "[JobSystem] fatal: ready fiber ring overflow.\n");
                    std::terminate();
                }
                eFiberRunState eExpected = eFiberRunState::Waiting;
                if (pRecord->eState.compare_exchange_strong(
                    eExpected,
                    eFiberRunState::Ready,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire))
                {
                    worker.ReadyFibers[worker.iReadyTail] = pRecord;
                    worker.iReadyTail =
                        (worker.iReadyTail + 1u) % kFibersPerWorker;
                    ++worker.iReadyCount;
                    bQueuedReadyFiber = true;
                }
                it = waiters.erase(it);
            }
            else
            {
                ++it;
            }
        }
        if (waiters.empty())
            m_pFiberScheduler->Waiters.erase(waiterIt);
    }

    if (bQueuedReadyFiber)
        m_WakeCV.notify_all();
}

bool CJobSystem::HasWaitingFiber(std::uint32_t iWorkerIdx) const
{
    if (!m_pFiberScheduler ||
        iWorkerIdx >= m_pFiberScheduler->vecWorkerStates.size())
    {
        return false;
    }

    const FiberSchedulerState::WorkerState& worker =
        *m_pFiberScheduler->vecWorkerStates[iWorkerIdx];
    for (const auto& pRecord : worker.vecRecords)
    {
        const eFiberRunState eState = pRecord->eState.load(std::memory_order_acquire);
        if (eState == eFiberRunState::Waiting || eState == eFiberRunState::Ready)
            return true;
    }
    return false;
}

void CJobSystem::ShutdownFiberWorker(std::uint32_t iWorkerIdx)
{
    if (!m_pFiberScheduler ||
        iWorkerIdx >= m_pFiberScheduler->vecWorkerStates.size())
    {
        return;
    }

    FiberSchedulerState::WorkerState& worker =
        *m_pFiberScheduler->vecWorkerStates[iWorkerIdx];
    for (const auto& pRecord : worker.vecRecords)
    {
        if (pRecord->hFiber)
            DeleteFiber(pRecord->hFiber);
    }
    worker.Pool.Reset();
    worker.vecRecords.clear();
    {
        std::lock_guard<std::mutex> readyLock(worker.ReadyMutex);
        worker.ReadyFibers.fill(nullptr);
        worker.iReadyHead = 0;
        worker.iReadyTail = 0;
        worker.iReadyCount = 0;
    }
    worker.hRootFiber = nullptr;
}

void CJobSystem::WaitForCounter(
    CJobCounter* pCounter,
    std::uint32_t iTarget)
{
    if (!pCounter)
        return;

    // Worker calls are pinned by Shutdown's join. Inline jobs already own their
    // Submit lease. A standalone external waiter needs its own deque/fiber-state lease.
    const bool bOwnsAdmission =
        t_pOwningJobSystem != this && !IsCurrentThreadSubmittingTo(this);
    if (!bOwnsAdmission)
    {
        WaitForCounterInternal(pCounter, iTarget);
        return;
    }

    BeginSubmission();
    SubmissionTlsNode submissionNode{ this, GetSubmissionNode() };
    SetSubmissionNode(&submissionNode);
    try
    {
        WaitForCounterInternal(pCounter, iTarget);
    }
    catch (...)
    {
        SetSubmissionNode(submissionNode.pPrevious);
        EndSubmission();
        throw;
    }
    SetSubmissionNode(submissionNode.pPrevious);
    EndSubmission();
}

void CJobSystem::WaitForCounterInternal(
    CJobCounter* pCounter,
    std::uint32_t iTarget)
{

    while (pCounter->Load() > iTarget)
    {
        if (GetExecutionMode() == eJobExecutionMode::FiberFull &&
            t_pOwningJobSystem == this &&
            t_pCurrentFiberRecord)
        {
            SuspendCurrentFiber(pCounter, iTarget);
            continue;
        }

        bool bDidWork = false;
        if (t_pOwningJobSystem == this && t_iWorkerIdx >= 0)
        {
            const std::uint32_t iWorkerIdx = static_cast<std::uint32_t>(t_iWorkerIdx);
            if (GetExecutionMode() == eJobExecutionMode::FiberFull)
                bDidWork = TryResumeReadyFiber(iWorkerIdx);
            if (!bDidWork)
                bDidWork = TryExecuteOneJob(iWorkerIdx);
        }
        else
        {
            WorkItem* pItem = nullptr;
            {
                std::lock_guard<std::mutex> queueLock(m_GlobalMutex);
                if (!m_GlobalQueue.empty())
                {
                    pItem = m_GlobalQueue.front();
                    m_GlobalQueue.pop();
                }
            }
            if (pItem)
            {
                ExecuteItem(pItem);
                bDidWork = true;
            }
            else
            {
                const std::uint32_t iWorkerCount =
                    static_cast<std::uint32_t>(m_vecDeques.size());
                if (iWorkerCount > 0)
                {
                    const std::uint32_t iVictim =
                        m_iRoundRobin.fetch_add(1, std::memory_order_relaxed) % iWorkerCount;
                    if (m_vecDeques[iVictim]->Steal(pItem))
                    {
                        m_uSteals.fetch_add(1, std::memory_order_relaxed);
                        ExecuteItem(pItem);
                        bDidWork = true;
                    }
                }
            }
        }

        if (!bDidWork)
            std::this_thread::yield();
    }
}
