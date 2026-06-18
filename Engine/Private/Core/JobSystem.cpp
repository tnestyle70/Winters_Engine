#include "WintersPCH.h"
#include "Core/JobSystem.h"
#include "Core/JobCounter.h"
#include "ProfilerAPI.h"

#include <cstdio>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cassert>
#include <random>

// thread_local: 이 스레드가 몇 번 Worker 인지.
//  -1 = 비-Worker (Main 또는 외부 스레드). WaitForCounter 안에서 구분용.
namespace
{
    thread_local std::int32_t t_iWorkerIdx = -1;
    thread_local NativeFiberHandle t_hThreadFiber = nullptr;
    thread_local bool t_bInsideJobFiber = false;
}

CJobSystem::CJobSystem() = default;

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
    if (!m_vecWorkers.empty())
        return; // 이미 Initialize 됨 (중복 호출 방어)

    if (iWorkerCount == 0)
    {
        const std::uint32_t hc = std::thread::hardware_concurrency();
        iWorkerCount = (hc > 2) ? (hc - 2) : 1u;
    }

    // Deque 를 먼저 확보한 뒤 Worker 시작.
    // (Worker 가 즉시 Deque 인덱스 접근하므로 순서 중요)
    // Phase 5-A: vector<T>(N) 대신 unique_ptr 래핑 후 push_back.
    // 이유: CWorkStealingDeque 의 std::atomic 멤버 + alignas(64) 조합이
    //       MSVC construct_at SFINAE 에서 실패하므로 힙 할당 + 포인터 저장.
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

void CJobSystem::Shutdown()
{
    if (m_vecWorkers.empty())
        return;

    m_bShutdown.store(true, std::memory_order_release);
    m_WakeCV.notify_all();
    for (auto& w : m_vecWorkers)
    {
        if (w.joinable())
            w.join();
    }
    m_vecWorkers.clear();
    m_vecDeques.clear();
}

// ─── Submit ─── Enque Job으로 통합!
void CJobSystem::SetExecutionMode(eJobExecutionMode eMode)
{
    m_eExecutionMode.store(eMode, std::memory_order_release);
}

eJobExecutionMode CJobSystem::GetExecutionMode() const
{
    return m_eExecutionMode.load(std::memory_order_acquire);
}

void CJobSystem::Submit(std::function<void()> job)
{
    EnqueueJob(WorkItem{ std::move(job), nullptr });
}

void CJobSystem::Submit(std::function<void()> job, CJobCounter* pCounter)
{
    if (pCounter)
        pCounter->Increment();
    EnqueueJob(WorkItem{ std::move(job), pCounter });
}

void CJobSystem::Submit(const JobDecl& decl, CJobCounter* pCounter)
{
    if (pCounter)
        pCounter->Increment();
    JobFn pFn = decl.pFn;
    void* pData = decl.pData;
    EnqueueJob(WorkItem{ [pFn, pData]() {if (pFn) pFn(pData); }, pCounter });
}

void CJobSystem::PushToSomeDeque(WorkItem&& item)
{
   //wrapper
    EnqueueJob(std::move(item));
}

void CJobSystem::EnqueueJob(WorkItem&& item)
{
    //Shutdown 상태 - 즉시 실행
    if (m_bShutdown.load(std::memory_order_acquire))
    {
        WorkItem local = std::move(item);
        ExecuteItem(local);
        return;
    }

    const std::uint32_t N = static_cast<std::uint32_t>(m_vecDeques.size());

    //fallback - jobsystem 미초기화
    if (N == 0)
    {
        WorkItem local = std::move(item);
        ExecuteItem(local);
        return;
    }
    //Worker 자신 - 자기 dequq push(Chase Lev Owner)
    if (t_iWorkerIdx >= 0 && static_cast<std::uint32_t>(t_iWorkerIdx) < N)
    {
        if (m_vecDeques[t_iWorkerIdx]->Push(item))
        {
            m_WakeCV.notify_one();
            return;
        }
        //overflow
    }
    //main 외부 / overflow - global queue
    {
        std::lock_guard<std::mutex> lk(m_GlobalMutex);
        m_GlobalQueue.push(std::move(item));
    }
    m_WakeCV.notify_one();
}

// ─── Worker 루프 ───────────────────────────────────────────────
void CJobSystem::WorkerLoop(std::uint32_t iWorkerIdx)
{
    t_iWorkerIdx = static_cast<std::int32_t>(iWorkerIdx);

#ifdef WINTERS_PROFILING
    char szThreadName[32];
    std::snprintf(szThreadName, sizeof(szThreadName), "JobWorker %u", iWorkerIdx);
    WINTERS_PROFILE_THREAD_NAME(szThreadName);
#endif
    if (GetExecutionMode() == eJobExecutionMode::FiberShell && !IsThreadAFiber())
    {
        t_hThreadFiber = ConvertThreadToFiber(nullptr);
    }

    while (!m_bShutdown.load(std::memory_order_acquire))
    {
        if (!TryExecuteOneJob(iWorkerIdx))
        {
            // yield 스핀은 외부 CPU 부하와 경쟁하며 코어를 태운다.
            // 짧은 타임아웃 대기로 전환해 잡이 없을 땐 실제로 잠든다.
            // (타임아웃이 있어 steal 전용 깨우기 누락도 1ms 안에 회복)
            std::unique_lock<std::mutex> lk(m_WakeMutex);
            m_WakeCV.wait_for(lk, std::chrono::milliseconds(1));
        }
    }

    if (t_hThreadFiber && IsThreadAFiber())
    {
        ConvertFiberToThread();
        t_hThreadFiber = nullptr;
    }
    t_iWorkerIdx = -1;
}

std::uint32_t CJobSystem::PickVictim(std::uint32_t iSelf, std::uint32_t N)
{
    // 간단 PRNG (xorshift) — 스레드별 시드
    thread_local std::uint32_t s_rng =
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&iSelf) ^ iSelf ^ 0x9E3779B9u);
    s_rng ^= s_rng << 13;
    s_rng ^= s_rng >> 17;
    s_rng ^= s_rng << 5;
    std::uint32_t v = s_rng % N;
    if (v == iSelf)
        v = (v + 1) % N;
    return v;
}

bool CJobSystem::TryExecuteOneJob(std::uint32_t iWorkerIdx)
{
    WorkItem item;
    // 1) 자기 Deque 부터 (LIFO)
    if (m_vecDeques[iWorkerIdx]->Pop(item))   // unique_ptr 디레퍼
    {
        ExecuteItem(item);
        return true;
    }
    //Global Queue
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
    //Steal
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

void CJobSystem::ExecuteItemInline(WorkItem& item)
{
    if (item.fn)
        item.fn();
    if (item.pCounter)
        item.pCounter->Decrement();
}

bool CJobSystem::TryExecuteItemOnFiber(WorkItem& item)
{
    if (!IsThreadAFiber())
        t_hThreadFiber = ConvertThreadToFiber(nullptr);

    if (!t_hThreadFiber)
        return false;

    FiberShellCall call{};
    call.pSystem = this;
    call.pItem = &item;
    call.hReturnFiber = t_hThreadFiber;

    void* hJobFiber = CreateFiber(0, &CJobSystem::FiberShellEntry, &call);
    if (!hJobFiber)
        return false;

    SwitchToFiber(hJobFiber);
    DeleteFiber(hJobFiber);
    return true;
}

void WINTERS_FIBER_CALL CJobSystem::FiberShellEntry(void* pParam)
{
    FiberShellCall* pCall = static_cast<FiberShellCall*>(pParam);
    t_bInsideJobFiber = true;
    if (pCall && pCall->pSystem && pCall->pItem)
        pCall->pSystem->ExecuteItemInline(*pCall->pItem);
    t_bInsideJobFiber = false;
    if (pCall && pCall->hReturnFiber)
        SwitchToFiber(pCall->hReturnFiber);
}

// ─── WaitForCounter (help-stealing) ────────────────────────────
void CJobSystem::WaitForCounter(CJobCounter* pCounter, std::uint32_t iTarget)
{
    if (!pCounter)
        return;

    while (pCounter->Load() > iTarget)
    {
        bool bDidWork = false;

        if (t_iWorkerIdx >= 0)
        {
            // Worker - 본인 Deque + Global + Steam - TryExecuteOneJob 다 처리
            bDidWork = TryExecuteOneJob(static_cast<std::uint32_t>(t_iWorkerIdx));
        }
        else
        {
            //외부 Global Drain First then Steal!
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
                //lock 밖에서 실행
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
        //양보!
        if (!bDidWork)
            std::this_thread::yield();
    }
}
