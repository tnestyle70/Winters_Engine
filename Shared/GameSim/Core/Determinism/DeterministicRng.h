#pragma once

#include "WintersTypes.h"

#include <cstdint>

class DeterministicRng
{
public:
    explicit DeterministicRng(uint64_t seed)
        : m_state(seed ? seed : 0x9E3779B97F4A7C15ull)
    {
    }

    uint64_t NextU64()
    {
        uint64_t x = m_state;
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        return m_state = x;
    }

    uint32_t NextU32()
    {
        return static_cast<uint32_t>(NextU64() & 0xFFFFFFFFull);
    }

    f32_t NextF01()
    {
        return (NextU32() >> 8) * (1.f / 16777216.f);
    }

    bool RollChance(f32_t probability)
    {
        return NextF01() < probability;
    }

    uint64_t GetState() const { return m_state; }
    void SetState(uint64_t state) { m_state = state; }

    uint64_t MakeSubSeed(uint64_t tickIndex, uint32_t sourceEntityId, uint16_t skillId) const
    {
        uint64_t seed = m_state;
        seed ^= tickIndex * 0xBF58476D1CE4E5B9ull;
        seed ^= static_cast<uint64_t>(sourceEntityId) * 0x94D049BB133111EBull;
        seed ^= static_cast<uint64_t>(skillId) * 0x9E3779B97F4A7C15ull;
        return seed;
    }

private:
    uint64_t m_state = 0;
};
