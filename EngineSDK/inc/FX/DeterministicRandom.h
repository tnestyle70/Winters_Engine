#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"

class CXoroshiro128 final
{
public:
    WINTERS_ENGINE CXoroshiro128();
    WINTERS_ENGINE explicit CXoroshiro128(u64_t seed);
    ~CXoroshiro128() = default;

    WINTERS_ENGINE void Seed(u64_t seed);
    WINTERS_ENGINE u64_t Next();
    WINTERS_ENGINE f32_t NextFloat();
    WINTERS_ENGINE f32_t NextRange(f32_t fMin, f32_t fMax);

private:
    u64_t m_State[2] = {};
};
