#pragma once

#include "WintersTypes.h"

#include <array>

inline constexpr u32_t kMeshVisibilityMaskWords = 32;
inline constexpr u32_t kMeshVisibilityMaxSubmeshes = kMeshVisibilityMaskWords * 64u;

using VisibilityMask = std::array<u64_t, kMeshVisibilityMaskWords>;

inline VisibilityMask MakeAllVisibleMask()
{
    VisibilityMask mask{};
    for (u64_t& word : mask)
        word = ~0ull;
    return mask;
}

inline bool_t IsSubmeshVisible(const VisibilityMask& mask, u32_t iSubmesh)
{
    if (iSubmesh >= kMeshVisibilityMaxSubmeshes)
        return false;

    const u32_t iWord = iSubmesh / 64u;
    const u32_t iBit = iSubmesh % 64u;
    return (mask[iWord] & (1ull << iBit)) != 0ull;
}

inline bool_t IsAllVisibleMask(const VisibilityMask& mask)
{
    for (const u64_t word : mask)
    {
        if (word != ~0ull)
            return false;
    }
    return true;
}

inline void SetSubmeshVisible(VisibilityMask& mask, u32_t iSubmesh, bool_t bVisible)
{
    if (iSubmesh >= kMeshVisibilityMaxSubmeshes)
        return;

    const u32_t iWord = iSubmesh / 64u;
    const u32_t iBit = iSubmesh % 64u;
    const u64_t bit = 1ull << iBit;

    if (bVisible)
        mask[iWord] |= bit;
    else
        mask[iWord] &= ~bit;
}

struct MeshGroupVisibilityComponent
{
    VisibilityMask mask = MakeAllVisibleMask();
    bool_t bEnabled = true;
};
