#pragma once
#include <atomic>
#include <cstdint>

// ─────────────────────────────────────────────────────────────
// CJobCounter — Job 묶음의 남은 개수 원자 카운터
//  - Submit(job, &counter)  호출 시 Increment()
//  - Job 실행 완료 시 CJobSystem::ExecuteItem 이 Decrement()
//  - 대기는 CJobSystem::WaitForCounter(pCounter, 0) 로 수행
//    (busy-wait + help-stealing. Phase 5-B 에서 Fiber yield 로 교체)
//
// 주의: cv/mutex 제거됨. 기존 JobCounter::Wait() API 삭제.
//       호출부는 반드시 pJobSystem->WaitForCounter(&c) 로 대체.
// ─────────────────────────────────────────────────────────────
class CJobCounter
{
public:
    CJobCounter() = default;
    ~CJobCounter() = default;

    CJobCounter(const CJobCounter&) = delete;
    CJobCounter& operator=(const CJobCounter&) = delete;

    void Increment(std::uint32_t n = 1)
    {
        m_iCount.fetch_add(n, std::memory_order_relaxed);
    }

    void Decrement()
    {
        m_iCount.fetch_sub(1, std::memory_order_acq_rel);
    }

    bool IsComplete() const
    {
        return m_iCount.load(std::memory_order_acquire) == 0;
    }

    std::uint32_t Load() const
    {
        return m_iCount.load(std::memory_order_acquire);
    }

private:
    std::atomic<std::uint32_t> m_iCount{ 0 };
};