#include "FX/DeterministicRandom.h"

namespace
{
    u64_t RotateLeft(u64_t value, i32_t shift)
    {
        return (value << shift) | (value >> (64 - shift));
    }

    u64_t SplitMix64(u64_t& state)
    {
        u64_t z = (state += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
}

CXoroshiro128::CXoroshiro128()
{
    Seed(1);
}

CXoroshiro128::CXoroshiro128(u64_t seed)
{
    Seed(seed);
}

void CXoroshiro128::Seed(u64_t seed)
{
    if (seed == 0)
        seed = 1;

    u64_t state = seed;
    m_State[0] = SplitMix64(state);
    m_State[1] = SplitMix64(state);
}

u64_t CXoroshiro128::Next()
{
    const u64_t s0 = m_State[0];
    u64_t s1 = m_State[1];
    const u64_t result = s0 + s1;

    s1 ^= s0;
    m_State[0] = RotateLeft(s0, 55) ^ s1 ^ (s1 << 14);
    m_State[1] = RotateLeft(s1, 36);
    return result;
}

f32_t CXoroshiro128::NextFloat()
{
    constexpr f32_t invMax24 = 1.0f / 16777216.0f;
    return static_cast<f32_t>((Next() >> 40) & 0xFFFFFFull) * invMax24;
}

f32_t CXoroshiro128::NextRange(f32_t fMin, f32_t fMax)
{
    return fMin + (fMax - fMin) * NextFloat();
}
