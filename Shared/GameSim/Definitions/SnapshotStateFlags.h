#pragma once

#include "WintersTypes.h"

inline constexpr u32_t kSnapshotStateDeadFlag = 1u << 0;
inline constexpr u32_t kSnapshotStateMovingFlag = 1u << 1;
inline constexpr u32_t kSnapshotStateAttackFlag = 1u << 2;
inline constexpr u32_t kSnapshotStateInvisibleFlag = 1u << 3;
inline constexpr u32_t kSnapshotStateViegoSoulFlag = 1u << 4;