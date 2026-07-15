#pragma once
#include "WintersTypes.h"

#include <vector>

constexpr uint32_t PROFILER_MAX_EVENTS_PER_FRAME = 4096;
constexpr uint32_t PROFILER_MAX_COUNTERS_PER_FRAME = 256;
constexpr uint32_t PROFILER_MAX_SCOPE_STATS_PER_FRAME = 256;
constexpr uint32_t PROFILER_MAX_TREE_EVENTS_PER_FRAME = 1024;
constexpr uint32_t PROFILER_MAX_HISTORY_FRAMES = 6000;
constexpr uint32_t PROFILER_MAX_RAW_HISTORY_FRAMES = 64;

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

struct ProfilerHistoryConfig
{
	uint32_t sampleEveryNFrames = 1;
	uint32_t sampleEveryNServerTicks = 0;
	uint32_t maxFrames = 300;
};

struct ProfilerFrameRecord
{
	uint64_t renderFrame = 0;
	uint64_t serverTick = 0;
	uint64_t gpuFrameUs = 0;
	uint64_t gpuSourceRhiFrame = 0;
	f64_t frameMs = 0.0;
	bool_t bHasServerTick = false;
	bool_t bHasGpuFrameTime = false;
	bool_t bHasGpuSourceRhiFrame = false;
	bool_t bFrameStrideSample = false;
	bool_t bServerTickSample = false;
	bool_t bRawEventsRetained = true;
	uint32_t droppedScopeStats = 0;
	uint32_t droppedCounters = 0;
	uint32_t droppedRawEvents = 0;
	uint32_t omittedRawEvents = 0;
	std::vector<ProfilerEvent> events{};
	std::vector<ProfilerScopeStat> scopeStats{};
	std::vector<ProfilerCounter> counters{};
};
