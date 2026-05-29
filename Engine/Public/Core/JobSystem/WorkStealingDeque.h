#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <cstddef>

// ─────────────────────────────────────────────────────────────
// CWorkStealingDeque<T>  (Chase & Lev 2005)
//  - 주인(owner) 이 Push/Pop (bottom)  — LIFO, 캐시 친화적
//  - 타인(thief) 이 Steal    (top)     — FIFO, 캐시 경쟁 최소
//  - 고정 4096 슬롯 (MVP). 초과 시 Push 는 false 반환.
//  - alignas(64) 로 top/bottom false-sharing 방지.
// ─────────────────────────────────────────────────────────────
template <typename T>
class CWorkStealingDeque
{
public:
    static constexpr std::size_t kCapacity = 4096;
    static_assert((kCapacity& (kCapacity - 1)) == 0, "kCapacity must be power of two");

    CWorkStealingDeque() = default;

    CWorkStealingDeque(const CWorkStealingDeque&) = delete;
    CWorkStealingDeque& operator=(const CWorkStealingDeque&) = delete;
    // std::vector<CWorkStealingDeque> 담기 위해 move 만 허용
    CWorkStealingDeque(CWorkStealingDeque&&) noexcept {}
    CWorkStealingDeque& operator=(CWorkStealingDeque&&) noexcept { return *this; }

    // owner 만 호출. 성공 시 true, 풀이면 false.
    bool Push(const T& v)
    {
        std::int64_t b = m_iBottom.load(std::memory_order_relaxed);
        std::int64_t t = m_iTop.load(std::memory_order_acquire);
        if (b - t >= static_cast<std::int64_t>(kCapacity))
            return false;
        m_arrBuf[static_cast<std::size_t>(b) & (kCapacity - 1)] = v;
        std::atomic_thread_fence(std::memory_order_release);
        m_iBottom.store(b + 1, std::memory_order_relaxed);
        return true;
    }

    // owner 만 호출. LIFO.
    bool Pop(T& out)
    {
        std::int64_t b = m_iBottom.load(std::memory_order_relaxed) - 1;
        m_iBottom.store(b, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        std::int64_t t = m_iTop.load(std::memory_order_relaxed);
        if (t > b)
        {
            m_iBottom.store(t, std::memory_order_relaxed);
            return false;
        }
        out = m_arrBuf[static_cast<std::size_t>(b) & (kCapacity - 1)];
        if (t != b)
            return true;
        // 마지막 1개 — Steal 과 경합
        bool ok = m_iTop.compare_exchange_strong(
            t, t + 1,
            std::memory_order_seq_cst,
            std::memory_order_relaxed);
        m_iBottom.store(t + 1, std::memory_order_relaxed);
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
        out = m_arrBuf[static_cast<std::size_t>(t) & (kCapacity - 1)];
        if (!m_iTop.compare_exchange_weak(
            t, t + 1,
            std::memory_order_seq_cst,
            std::memory_order_relaxed))
            return false;
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
    std::array<T, kCapacity> m_arrBuf{};
};