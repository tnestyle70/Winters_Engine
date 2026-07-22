#pragma once

#include "WintersTypes.h"

inline constexpr u32_t kSnapshotStateDeadFlag = 1u << 0;
inline constexpr u32_t kSnapshotStateMovingFlag = 1u << 1;
inline constexpr u32_t kSnapshotStateAttackFlag = 1u << 2;
inline constexpr u32_t kSnapshotStateInvisibleFlag = 1u << 3;
inline constexpr u32_t kSnapshotStateViegoSoulFlag = 1u << 4;
inline constexpr u32_t kSnapshotStateKalistaCarriedFlag = 1u << 5;
inline constexpr u32_t kSnapshotStateKalistaOathswornRitualFlag = 1u << 6;
// Bit 31 is reserved for the authoritative RecallComponent snapshot state.
inline constexpr u32_t kSnapshotStateRecallFlag = 1u << 31;

inline constexpr u32_t kObjectiveStateBaronFlag = 1u << 0;
inline constexpr u32_t kObjectiveStateElderFlag = 1u << 1;
inline constexpr u32_t kObjectiveStateBlueFlag = 1u << 2;
inline constexpr u32_t kObjectiveStateRedFlag = 1u << 3;
inline constexpr u32_t kObjectiveStateBaronMinionFlag = 1u << 4;
