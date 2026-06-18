#pragma once
#include "WintersTypes.h"

constexpr uint32_t PROFILER_MAX_EVENTS_PER_FRAME = 4096;
constexpr uint32_t PROFILER_MAX_COUNTERS_PER_FRAME = 128;
constexpr uint32_t PROFILER_MAX_SCOPE_STATS_PER_FRAME = 128;
constexpr uint32_t PROFILER_MAX_TREE_EVENTS_PER_FRAME = 256;

struct ProfilerEvent
{
	const char* pName;
	uint64_t startTicks; //QueryPerformanceCounter
	uint64_t endTicks;
	uint32_t depth;
	uint32_t threadId;
};

struct ProfilerScopeStat
{
	const char* pName;
	uint64_t totalTicks;
	uint64_t maxTicks;
	uint32_t callCount;
};

//Profiler
struct ProfilerCounter
{
	const char* pName;
	uint64_t value;
};
