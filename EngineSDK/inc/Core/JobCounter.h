#pragma once

#include <atomic>
#include <cstdint>

// Atomic remaining-work counter for a fork/join group.
// Submit increments before publication. Exactly one completion decrements.
// ThreadOnly/FiberShell waits help-execute work; FiberFull registers a waiter,
// yields to the origin worker root, and resumes through that worker's ready inbox.
// The counter must outlive every submitted job and its WaitForCounter call.
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

    bool TryDecrement(std::uint32_t& iRemaining)
    {
        std::uint32_t iCurrent = m_iCount.load(std::memory_order_acquire);
        while (iCurrent != 0)
        {
            if (m_iCount.compare_exchange_weak(
                iCurrent,
                iCurrent - 1,
                std::memory_order_acq_rel,
                std::memory_order_acquire))
            {
                iRemaining = iCurrent - 1;
                return true;
            }
        }

        iRemaining = 0;
        return false;
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
