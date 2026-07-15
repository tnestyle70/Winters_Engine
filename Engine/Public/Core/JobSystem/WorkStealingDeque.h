#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <type_traits>

// ─────────────────────────────────────────────────────────────
// CWorkStealingDeque<T>  (Chase & Lev 2005)
//  - 주인(owner) 이 Push/Pop (bottom)  — LIFO, 캐시 친화적
//  - 타인(thief) 이 Steal    (top)     — FIFO, 캐시 경쟁 최소
//  - 고정 4096 atomic 슬롯. 초과 시 Push 는 false 반환.
//  - T 는 pointer/integer handle 같은 trivially-copyable token 이어야 한다.
//    CAS 성공으로 소유권을 얻은 호출자만 token 을 dereference/reclaim 한다.
//  - alignas(64) 로 top/bottom false-sharing 방지.
// ─────────────────────────────────────────────────────────────
template <typename T>
class CWorkStealingDeque
{
public:
    static constexpr std::size_t kCapacity = 4096;
    static_assert((kCapacity& (kCapacity - 1)) == 0, "kCapacity must be power of two");
    static_assert(std::is_trivially_copyable_v<T>,
        "Chase-Lev slots must contain an atomic trivially-copyable token");

    CWorkStealingDeque() = default;

    CWorkStealingDeque(const CWorkStealingDeque&) = delete;
    CWorkStealingDeque& operator=(const CWorkStealingDeque&) = delete;
    CWorkStealingDeque(CWorkStealingDeque&&) = delete;
    CWorkStealingDeque& operator=(CWorkStealingDeque&&) = delete;

    // owner 만 호출. 성공 시 true, 풀이면 false.
    bool Push(T v)
    {
        std::int64_t b = m_iBottom.load(std::memory_order_relaxed);
        std::int64_t t = m_iTop.load(std::memory_order_acquire);
        if (b - t >= static_cast<std::int64_t>(kCapacity))
            return false;
        m_arrBuf[static_cast<std::size_t>(b) & (kCapacity - 1)].store(
            v, std::memory_order_relaxed);
        m_iBottom.store(b + 1, std::memory_order_release);
        return true;
    }

    // owner 만 호출. LIFO.
    bool Pop(T& out)
    {
        std::int64_t b = m_iBottom.load(std::memory_order_relaxed) - 1;
        // A thief may observe this rewound value instead of the Push release.
        // Keep it release so C++20 does not rely on the removed same-thread
        // continuation rule for release sequences.
        m_iBottom.store(b, std::memory_order_release);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        std::int64_t t = m_iTop.load(std::memory_order_relaxed);
        if (t > b)
        {
            m_iBottom.store(t, std::memory_order_release);
            return false;
        }
        const T value = m_arrBuf[static_cast<std::size_t>(b) & (kCapacity - 1)].load(
            std::memory_order_acquire);
        if (t != b)
        {
            out = value;
            return true;
        }
        // 마지막 1개 — Steal 과 경합
        const std::int64_t iLastIndex = t;
        bool ok = m_iTop.compare_exchange_strong(
            t, t + 1,
            std::memory_order_seq_cst,
            std::memory_order_relaxed);
        // compare_exchange updates `t` on failure. Restore bottom from the
        // pre-CAS last-item index, not from the thief-updated expected value.
        m_iBottom.store(iLastIndex + 1, std::memory_order_release);
        if (ok)
            out = value;
        return ok;
    }

    // 타인이 호출. FIFO.
    bool Steal(T& out)
    {
        std::int64_t t = m_iTop.load(std::memory_order_acquire);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        std::int64_t b = m_iBottom.load(std::memory_order_acquire);
        if (t >= b)
            return false;
        const T value = m_arrBuf[static_cast<std::size_t>(t) & (kCapacity - 1)].load(
            std::memory_order_acquire);
        if (!m_iTop.compare_exchange_weak(
            t, t + 1,
            std::memory_order_seq_cst,
            std::memory_order_relaxed))
            return false;
        out = value;
        return true;
    }

    std::int64_t SizeApprox() const
    {
        std::int64_t b = m_iBottom.load(std::memory_order_relaxed);
        std::int64_t t = m_iTop.load(std::memory_order_relaxed);
        return (b >= t) ? (b - t) : 0;
    }

private:
    alignas(64) std::atomic<std::int64_t> m_iBottom{ 0 };
    alignas(64) std::atomic<std::int64_t> m_iTop{ 0 };
    std::array<std::atomic<T>, kCapacity> m_arrBuf{};
};
