#pragma once

#include "WintersTypes.h"

inline bool SeqGreater(u32_t lhs, u32_t rhs)
{
	return static_cast<i32_t>(lhs - rhs) > 0;
}

inline u32_t SeqDistance(u32_t newerSeq, u32_t olderSeq)
{
	return newerSeq - olderSeq;
}