Session - 1차: CPU Profiler hot path를 per-thread 수집으로 바꾸고 Frame scope 종료 순서를 바로잡는다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Engine/Public/Core/Profiler/ProfilerTypes.h

기존 코드:

```cpp
#pragma once
#include "WintersTypes.h"

constexpr uint32_t PROFILER_MAX_EVENTS_PER_FRAME = 4096;
constexpr uint32_t PROFILER_MAX_COUNTERS_PER_FRAME = 32;
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
```

아래로 교체:

```cpp
#pragma once
#include "WintersTypes.h"

constexpr u32_t PROFILER_MAX_EVENTS_PER_FRAME = 4096;
constexpr u32_t PROFILER_MAX_COUNTERS_PER_FRAME = 128;
constexpr u32_t PROFILER_MAX_SCOPE_STATS_PER_FRAME = 256;
constexpr u32_t PROFILER_MAX_TREE_EVENTS_PER_FRAME = 2048;

constexpr u32_t PROFILER_MAX_THREADS = 32;
constexpr u32_t PROFILER_MAX_SCOPE_STACK_DEPTH = 64;
constexpr u32_t PROFILER_MAX_THREAD_TREE_EVENTS_PER_FRAME = 256;
constexpr u32_t PROFILER_MAX_THREAD_SCOPE_STATS_PER_FRAME = 128;
constexpr u32_t PROFILER_MAX_THREAD_COUNTERS_PER_FRAME = 64;

enum class eProfilerCaptureMode : u8_t
{
	Off = 0,
	StatsOnly,
	StatsAndTree,
};

struct ProfilerEvent
{
	const char* pName = nullptr;
	u64_t startTicks = 0; // QueryPerformanceCounter
	u64_t endTicks = 0;
	u32_t depth = 0;
	u32_t threadId = 0;
	u32_t workerSlot = 0;
	u32_t fiberDepth = 0;
};

struct ProfilerScopeStat
{
	const char* pName = nullptr;
	u64_t totalTicks = 0;
	u64_t maxTicks = 0;
	u32_t callCount = 0;
};

struct ProfilerCounter
{
	const char* pName = nullptr;
	u64_t value = 0;
};

struct ProfilerRuntimeStats
{
	u64_t droppedEvents = 0;
	u64_t droppedScopeStats = 0;
	u64_t droppedCounters = 0;
	u64_t droppedThreadRegistrations = 0;
	u32_t registeredThreads = 0;
	u32_t captureMode = 0;
};
```

1-2. C:/Users/user/Desktop/Winters/Engine/Public/Core/Profiler/CPUProfiler.h

기존 코드:

```cpp
#pragma once
#include "ProfilerTypes.h"
#include <vector>
#include <memory>
#include <mutex>
//怨꾩링 ?ㅼ퐫????대㉧
class CCPUProfiler final
{
public:
	~CCPUProfiler() = default;

	static std::unique_ptr<CCPUProfiler> Create();

	void BeginFrame();
	void EndFrame();
	void PushScope(const char* pName);
	void PopScope();

	//Overlay ?쎄린
	const std::vector<ProfilerEvent>& Get_LastFrameEvents() const
	{
		return m_vLastFrameEvents;
	}

	f64_t Get_TicksToMs() const { return m_fTicksToMs; }
	void AddCounter(const char* pName, uint64_t delta);
	const std::vector<ProfilerCounter>& Get_LastFrameCounters() const { return m_vLastCounters; }
	const std::vector<ProfilerScopeStat>& Get_LastFrameScopeStats() const { return m_vLastScopeStats; }

private:
	CCPUProfiler() = default;
	// ?꾩옱 ?꾨젅???대깽???섏쭛.
	// Scope stack ? worker thread 蹂꾨줈 遺꾨━?섍퀬, event/counter merge 留?mutex 蹂댄샇.
	std::vector<ProfilerEvent> m_vCurrentFrame{};
	std::vector<ProfilerEvent> m_vLastFrameEvents{};
	std::vector<ProfilerScopeStat> m_vCurrentScopeStats{};
	std::vector<ProfilerScopeStat> m_vLastScopeStats{};

	f64_t m_fTicksToMs = 0.f; //QueryPerformanceFrequency ??닔 * 1000


	std::vector<ProfilerCounter> m_vCurrentCounters{};
	std::vector<ProfilerCounter> m_vLastCounters{};
	std::mutex m_Mutex{};
};
```

아래로 교체:

```cpp
#pragma once
#include "ProfilerTypes.h"

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

struct ProfilerPendingScope
{
	const char* pName = nullptr;
	u64_t startTicks = 0;
	u32_t depth = 0;
	u32_t workerSlot = 0;
	u32_t fiberDepth = 0;
};

struct alignas(64) ProfilerThreadState
{
	bool_t bRegistered = false;
	u32_t threadId = 0;
	u32_t workerSlot = 0;
	u32_t fiberDepth = 0;

	std::array<ProfilerPendingScope, PROFILER_MAX_SCOPE_STACK_DEPTH> scopeStack{};
	u32_t scopeDepth = 0;

	std::array<ProfilerEvent, PROFILER_MAX_THREAD_TREE_EVENTS_PER_FRAME> events{};
	u32_t eventCount = 0;

	std::array<ProfilerScopeStat, PROFILER_MAX_THREAD_SCOPE_STATS_PER_FRAME> scopeStats{};
	u32_t scopeStatCount = 0;

	std::array<ProfilerCounter, PROFILER_MAX_THREAD_COUNTERS_PER_FRAME> counters{};
	u32_t counterCount = 0;

	u64_t droppedEvents = 0;
	u64_t droppedScopeStats = 0;
	u64_t droppedCounters = 0;
};

class CCPUProfiler final
{
public:
	~CCPUProfiler() = default;

	static std::unique_ptr<CCPUProfiler> Create();

	void BeginFrame();
	void EndFrame();
	bool_t PushScope(const char* pName);
	void PopScope();
	void AddCounter(const char* pName, u64_t delta);
	void SetThreadContextForCurrentThread(u32_t workerSlot, u32_t fiberDepth);

	void SetCaptureMode(eProfilerCaptureMode eMode);
	eProfilerCaptureMode GetCaptureMode() const;
	bool_t IsEnabled() const;

	const std::vector<ProfilerEvent>& Get_LastFrameEvents() const
	{
		return m_vLastFrameEvents;
	}

	f64_t Get_TicksToMs() const { return m_fTicksToMs; }
	const std::vector<ProfilerCounter>& Get_LastFrameCounters() const { return m_vLastCounters; }
	const std::vector<ProfilerScopeStat>& Get_LastFrameScopeStats() const { return m_vLastScopeStats; }
	ProfilerRuntimeStats Get_RuntimeStats() const { return m_RuntimeStats; }

private:
	CCPUProfiler() = default;

	ProfilerThreadState* AcquireThreadState();
	static void ResetFrameStorage(ProfilerThreadState& state);

	std::array<ProfilerThreadState, PROFILER_MAX_THREADS> m_vThreadStates{};
	std::atomic<u32_t> m_iThreadStateCount{ 0 };
	std::mutex m_ThreadRegistryMutex{};

	std::vector<ProfilerEvent> m_vLastFrameEvents{};
	std::vector<ProfilerScopeStat> m_vLastScopeStats{};
	std::vector<ProfilerCounter> m_vLastCounters{};

	std::atomic<eProfilerCaptureMode> m_eCaptureMode{ eProfilerCaptureMode::StatsOnly };
	ProfilerRuntimeStats m_RuntimeStats{};
	f64_t m_fTicksToMs = 0.0; // QueryPerformanceFrequency inverse * 1000
};
```

1-3. C:/Users/user/Desktop/Winters/Engine/Private/Core/Profiler/CPUProfiler.cpp

기존 코드:

```cpp
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "Core/Profiler/CPUProfiler.h"
#include "GameInstance.h"
#include "ProfilerAPI.h"

#include <algorithm>
#include <cstring>

namespace
{
	struct PendingProfilerScope
	{
		const char* pName = nullptr;
		uint64_t startTicks = 0;
		uint32_t depth = 0;
	};

	bool SameProfilerName(const char* a, const char* b)
	{
		if (a == b)
			return true;
		if (a == nullptr || b == nullptr)
			return false;
		return std::strcmp(a, b) == 0;
	}

	thread_local std::vector<PendingProfilerScope> t_vProfilerStack;
}

std::unique_ptr<CCPUProfiler> CCPUProfiler::Create()
{
	auto pInst = std::unique_ptr<CCPUProfiler>(new CCPUProfiler());
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);
	pInst->m_fTicksToMs = 1000.0 / static_cast<f64_t>(freq.QuadPart);
	return pInst;
}

void CCPUProfiler::BeginFrame()
{
}

void CCPUProfiler::EndFrame()
{
	std::lock_guard<std::mutex> lk(m_Mutex);
	m_vLastFrameEvents.swap(m_vCurrentFrame);
	m_vCurrentFrame.clear();
	m_vLastCounters.swap(m_vCurrentCounters);
	m_vCurrentCounters.clear();
	m_vLastScopeStats.swap(m_vCurrentScopeStats);
	m_vCurrentScopeStats.clear();
}

void CCPUProfiler::PushScope(const char* pName)
{
	LARGE_INTEGER t;
	QueryPerformanceCounter(&t);
	t_vProfilerStack.push_back({
		pName,
		static_cast<uint64_t>(t.QuadPart),
		static_cast<uint32_t>(t_vProfilerStack.size())
		});
}

void CCPUProfiler::PopScope()
{
	if (t_vProfilerStack.empty())
		return;

	LARGE_INTEGER t;
	QueryPerformanceCounter(&t);
	auto top = t_vProfilerStack.back();
	t_vProfilerStack.pop_back();

	ProfilerEvent event{
		top.pName,
		top.startTicks,
		static_cast<uint64_t>(t.QuadPart),
		top.depth,
		GetCurrentThreadId()
	};

	const uint64_t elapsedTicks = event.endTicks - event.startTicks;

	std::lock_guard<std::mutex> lk(m_Mutex);
	if (m_vCurrentFrame.size() < PROFILER_MAX_TREE_EVENTS_PER_FRAME)
		m_vCurrentFrame.push_back(event);

	for (auto& stat : m_vCurrentScopeStats)
	{
		if (SameProfilerName(stat.pName, top.pName))
		{
			stat.totalTicks += elapsedTicks;
			stat.maxTicks = std::max(stat.maxTicks, elapsedTicks);
			++stat.callCount;
			return;
		}
	}

	if (m_vCurrentScopeStats.size() < PROFILER_MAX_SCOPE_STATS_PER_FRAME)
	{
		m_vCurrentScopeStats.push_back(
			ProfilerScopeStat{ top.pName, elapsedTicks, elapsedTicks, 1u });
	}
}

void CCPUProfiler::AddCounter(const char* pName, uint64_t delta)
{
	std::lock_guard<std::mutex> lk(m_Mutex);
	for (auto& c : m_vCurrentCounters)
	{
		if (c.pName == pName)
		{
			c.value += delta;
			return;
		}
	}

	if (m_vCurrentCounters.size() < PROFILER_MAX_COUNTERS_PER_FRAME)
		m_vCurrentCounters.push_back({ pName, delta });
}

#ifdef WINTERS_PROFILING
void Winters_Profile_Push(const char* pName)
{
	auto* pProf = CGameInstance::Get()->Get_CPUProfiler();
	if (pProf) pProf->PushScope(pName);
}

void Winters_Profile_Pop()
{
	auto* pProf = CGameInstance::Get()->Get_CPUProfiler();
	if (pProf) pProf->PopScope();
}

void Winters_Profile_Counter(const char* pName, uint64_t delta)
{
	auto* pProf = CGameInstance::Get()->Get_CPUProfiler();
	if (pProf) pProf->AddCounter(pName, delta);
}
#endif
```

아래로 교체:

```cpp
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "Core/Profiler/CPUProfiler.h"
#include "ProfilerAPI.h"

#include <algorithm>
#include <atomic>
#include <cstring>

namespace
{
	std::atomic<CCPUProfiler*> g_pActiveProfiler{ nullptr };
	thread_local CCPUProfiler* t_pProfilerOwner = nullptr;
	thread_local ProfilerThreadState* t_pProfilerState = nullptr;

	u64_t ReadProfilerTicks()
	{
		LARGE_INTEGER t;
		QueryPerformanceCounter(&t);
		return static_cast<u64_t>(t.QuadPart);
	}

	bool_t SameProfilerName(const char* a, const char* b)
	{
		if (a == b)
			return true;
		if (a == nullptr || b == nullptr)
			return false;
		return std::strcmp(a, b) == 0;
	}

	void MergeScopeStat(std::vector<ProfilerScopeStat>& dst, const ProfilerScopeStat& src, u64_t& dropped)
	{
		for (ProfilerScopeStat& stat : dst)
		{
			if (SameProfilerName(stat.pName, src.pName))
			{
				stat.totalTicks += src.totalTicks;
				stat.maxTicks = std::max(stat.maxTicks, src.maxTicks);
				stat.callCount += src.callCount;
				return;
			}
		}

		if (dst.size() < PROFILER_MAX_SCOPE_STATS_PER_FRAME)
		{
			dst.push_back(src);
			return;
		}

		++dropped;
	}

	void MergeCounter(std::vector<ProfilerCounter>& dst, const ProfilerCounter& src, u64_t& dropped)
	{
		for (ProfilerCounter& counter : dst)
		{
			if (SameProfilerName(counter.pName, src.pName))
			{
				counter.value += src.value;
				return;
			}
		}

		if (dst.size() < PROFILER_MAX_COUNTERS_PER_FRAME)
		{
			dst.push_back(src);
			return;
		}

		++dropped;
	}
}

std::unique_ptr<CCPUProfiler> CCPUProfiler::Create()
{
	auto pInst = std::unique_ptr<CCPUProfiler>(new CCPUProfiler());

	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);
	pInst->m_fTicksToMs = 1000.0 / static_cast<f64_t>(freq.QuadPart);

	pInst->m_vLastFrameEvents.reserve(PROFILER_MAX_TREE_EVENTS_PER_FRAME);
	pInst->m_vLastScopeStats.reserve(PROFILER_MAX_SCOPE_STATS_PER_FRAME);
	pInst->m_vLastCounters.reserve(PROFILER_MAX_COUNTERS_PER_FRAME);
	return pInst;
}

void CCPUProfiler::BeginFrame()
{
	m_RuntimeStats = {};
	m_RuntimeStats.captureMode = static_cast<u32_t>(GetCaptureMode());

	std::lock_guard<std::mutex> lk(m_ThreadRegistryMutex);
	const u32_t count = m_iThreadStateCount.load(std::memory_order_acquire);
	m_RuntimeStats.registeredThreads = count;
	for (u32_t i = 0; i < count; ++i)
	{
		ResetFrameStorage(m_vThreadStates[i]);
	}
}

void CCPUProfiler::EndFrame()
{
	m_vLastFrameEvents.clear();
	m_vLastScopeStats.clear();
	m_vLastCounters.clear();

	u64_t droppedEvents = 0;
	u64_t droppedScopeStats = 0;
	u64_t droppedCounters = 0;

	std::lock_guard<std::mutex> lk(m_ThreadRegistryMutex);
	const u32_t count = m_iThreadStateCount.load(std::memory_order_acquire);
	m_RuntimeStats.registeredThreads = count;

	for (u32_t i = 0; i < count; ++i)
	{
		ProfilerThreadState& state = m_vThreadStates[i];
		droppedEvents += state.droppedEvents;
		droppedScopeStats += state.droppedScopeStats;
		droppedCounters += state.droppedCounters;

		for (u32_t eventIndex = 0; eventIndex < state.eventCount; ++eventIndex)
		{
			if (m_vLastFrameEvents.size() < PROFILER_MAX_TREE_EVENTS_PER_FRAME)
			{
				m_vLastFrameEvents.push_back(state.events[eventIndex]);
			}
			else
			{
				++droppedEvents;
			}
		}

		for (u32_t statIndex = 0; statIndex < state.scopeStatCount; ++statIndex)
		{
			MergeScopeStat(m_vLastScopeStats, state.scopeStats[statIndex], droppedScopeStats);
		}

		for (u32_t counterIndex = 0; counterIndex < state.counterCount; ++counterIndex)
		{
			MergeCounter(m_vLastCounters, state.counters[counterIndex], droppedCounters);
		}
	}

	m_RuntimeStats.droppedEvents += droppedEvents;
	m_RuntimeStats.droppedScopeStats += droppedScopeStats;
	m_RuntimeStats.droppedCounters += droppedCounters;
}

bool_t CCPUProfiler::PushScope(const char* pName)
{
	if (!IsEnabled())
		return false;

	ProfilerThreadState* pState = AcquireThreadState();
	if (!pState)
		return false;

	const u32_t depth = pState->scopeDepth;
	if (depth >= PROFILER_MAX_SCOPE_STACK_DEPTH)
	{
		++pState->droppedScopeStats;
		return false;
	}

	pState->scopeStack[depth] = ProfilerPendingScope{
		pName,
		ReadProfilerTicks(),
		depth,
		pState->workerSlot,
		pState->fiberDepth
	};
	pState->scopeDepth = depth + 1u;
	return true;
}

void CCPUProfiler::PopScope()
{
	ProfilerThreadState* pState = (t_pProfilerOwner == this) ? t_pProfilerState : AcquireThreadState();
	if (!pState)
		return;

	if (pState->scopeDepth == 0)
	{
		++pState->droppedScopeStats;
		return;
	}

	const u64_t endTicks = ReadProfilerTicks();
	const ProfilerPendingScope top = pState->scopeStack[pState->scopeDepth - 1u];
	--pState->scopeDepth;

	const eProfilerCaptureMode eMode = GetCaptureMode();
	if (eMode == eProfilerCaptureMode::Off)
		return;

	const u64_t elapsedTicks = endTicks - top.startTicks;

	if (eMode == eProfilerCaptureMode::StatsAndTree)
	{
		if (pState->eventCount < PROFILER_MAX_THREAD_TREE_EVENTS_PER_FRAME)
		{
			pState->events[pState->eventCount++] = ProfilerEvent{
				top.pName,
				top.startTicks,
				endTicks,
				top.depth,
				pState->threadId,
				top.workerSlot,
				top.fiberDepth
			};
		}
		else
		{
			++pState->droppedEvents;
		}
	}

	for (u32_t i = 0; i < pState->scopeStatCount; ++i)
	{
		ProfilerScopeStat& stat = pState->scopeStats[i];
		if (SameProfilerName(stat.pName, top.pName))
		{
			stat.totalTicks += elapsedTicks;
			stat.maxTicks = std::max(stat.maxTicks, elapsedTicks);
			++stat.callCount;
			return;
		}
	}

	if (pState->scopeStatCount < PROFILER_MAX_THREAD_SCOPE_STATS_PER_FRAME)
	{
		pState->scopeStats[pState->scopeStatCount++] =
			ProfilerScopeStat{ top.pName, elapsedTicks, elapsedTicks, 1u };
	}
	else
	{
		++pState->droppedScopeStats;
	}
}

void CCPUProfiler::AddCounter(const char* pName, u64_t delta)
{
	if (!IsEnabled())
		return;

	ProfilerThreadState* pState = AcquireThreadState();
	if (!pState)
		return;

	for (u32_t i = 0; i < pState->counterCount; ++i)
	{
		ProfilerCounter& counter = pState->counters[i];
		if (SameProfilerName(counter.pName, pName))
		{
			counter.value += delta;
			return;
		}
	}

	if (pState->counterCount < PROFILER_MAX_THREAD_COUNTERS_PER_FRAME)
	{
		pState->counters[pState->counterCount++] = ProfilerCounter{ pName, delta };
	}
	else
	{
		++pState->droppedCounters;
	}
}

void CCPUProfiler::SetThreadContextForCurrentThread(u32_t workerSlot, u32_t fiberDepth)
{
	ProfilerThreadState* pState = AcquireThreadState();
	if (!pState)
		return;

	pState->workerSlot = workerSlot;
	pState->fiberDepth = fiberDepth;
}

void CCPUProfiler::SetCaptureMode(eProfilerCaptureMode eMode)
{
	m_eCaptureMode.store(eMode, std::memory_order_release);
}

eProfilerCaptureMode CCPUProfiler::GetCaptureMode() const
{
	return m_eCaptureMode.load(std::memory_order_acquire);
}

bool_t CCPUProfiler::IsEnabled() const
{
	return GetCaptureMode() != eProfilerCaptureMode::Off;
}

ProfilerThreadState* CCPUProfiler::AcquireThreadState()
{
	if (t_pProfilerOwner == this && t_pProfilerState)
		return t_pProfilerState;

	const u32_t threadId = GetCurrentThreadId();
	std::lock_guard<std::mutex> lk(m_ThreadRegistryMutex);

	const u32_t count = m_iThreadStateCount.load(std::memory_order_acquire);
	for (u32_t i = 0; i < count; ++i)
	{
		ProfilerThreadState& state = m_vThreadStates[i];
		if (state.bRegistered && state.threadId == threadId)
		{
			t_pProfilerOwner = this;
			t_pProfilerState = &state;
			return &state;
		}
	}

	if (count >= PROFILER_MAX_THREADS)
	{
		++m_RuntimeStats.droppedThreadRegistrations;
		return nullptr;
	}

	ProfilerThreadState& state = m_vThreadStates[count];
	state.bRegistered = true;
	state.threadId = threadId;
	state.workerSlot = 0;
	state.fiberDepth = 0;
	ResetFrameStorage(state);

	m_iThreadStateCount.store(count + 1u, std::memory_order_release);
	t_pProfilerOwner = this;
	t_pProfilerState = &state;
	return &state;
}

void CCPUProfiler::ResetFrameStorage(ProfilerThreadState& state)
{
	state.scopeDepth = 0;
	state.eventCount = 0;
	state.scopeStatCount = 0;
	state.counterCount = 0;
	state.droppedEvents = 0;
	state.droppedScopeStats = 0;
	state.droppedCounters = 0;
}

#ifdef WINTERS_PROFILING
void Winters_Profile_SetCPUProfiler(CCPUProfiler* pProfiler)
{
	g_pActiveProfiler.store(pProfiler, std::memory_order_release);
	if (!pProfiler)
	{
		t_pProfilerOwner = nullptr;
		t_pProfilerState = nullptr;
	}
}

bool_t Winters_Profile_Push(const char* pName)
{
	CCPUProfiler* pProf = g_pActiveProfiler.load(std::memory_order_acquire);
	return pProf ? pProf->PushScope(pName) : false;
}

void Winters_Profile_Pop()
{
	CCPUProfiler* pProf = g_pActiveProfiler.load(std::memory_order_acquire);
	if (pProf)
		pProf->PopScope();
}

void Winters_Profile_Counter(const char* pName, u64_t delta)
{
	CCPUProfiler* pProf = g_pActiveProfiler.load(std::memory_order_acquire);
	if (pProf)
		pProf->AddCounter(pName, delta);
}

void Winters_Profile_SetThreadContext(u32_t workerSlot, u32_t fiberDepth)
{
	CCPUProfiler* pProf = g_pActiveProfiler.load(std::memory_order_acquire);
	if (pProf)
		pProf->SetThreadContextForCurrentThread(workerSlot, fiberDepth);
}
#endif
```

1-4. C:/Users/user/Desktop/Winters/Engine/Include/ProfilerAPI.h

기존 코드:

```cpp
// Engine/Include/ProfilerAPI.h 援먯껜
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"

#ifdef WINTERS_PROFILING
WINTERS_ENGINE void Winters_Profile_Counter(const char* pName, uint64_t delta);
#define WINTERS_PROFILE_COUNT(name, delta) Winters_Profile_Counter(name, delta)
#else
#define WINTERS_PROFILE_COUNT(name, delta) ((void)0)
#endif

#ifdef WINTERS_PROFILING
WINTERS_ENGINE void Winters_Profile_Push(const char* pName);
WINTERS_ENGINE void Winters_Profile_Pop();

class WINTERS_ENGINE CProfileScope
{
public:
    explicit CProfileScope(const char* pName) { Winters_Profile_Push(pName); }
    ~CProfileScope() { Winters_Profile_Pop(); }
};

// 2?④퀎 CONCAT ?쇰줈 __LINE__ 吏???뺤옣
#define WINTERS_PROFILE_CAT_INNER(a, b) a##b
#define WINTERS_PROFILE_CAT(a, b)       WINTERS_PROFILE_CAT_INNER(a, b)
#define WINTERS_PROFILE_SCOPE(name) \
    ::CProfileScope WINTERS_PROFILE_CAT(_winProfScope_, __LINE__)(name)

#else
#define WINTERS_PROFILE_SCOPE(name) ((void)0)
#endif
```

아래로 교체:

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"

#ifdef WINTERS_PROFILING
class CCPUProfiler;

WINTERS_ENGINE void Winters_Profile_SetCPUProfiler(CCPUProfiler* pProfiler);
WINTERS_ENGINE bool_t Winters_Profile_Push(const char* pName);
WINTERS_ENGINE void Winters_Profile_Pop();
WINTERS_ENGINE void Winters_Profile_Counter(const char* pName, u64_t delta);
WINTERS_ENGINE void Winters_Profile_SetThreadContext(u32_t workerSlot, u32_t fiberDepth);

#define WINTERS_PROFILE_SET_CPU_PROFILER(pProfiler) Winters_Profile_SetCPUProfiler(pProfiler)
#define WINTERS_PROFILE_COUNT(name, delta) Winters_Profile_Counter(name, static_cast<u64_t>(delta))
#define WINTERS_PROFILE_THREAD_CONTEXT(workerSlot, fiberDepth) \
	Winters_Profile_SetThreadContext(static_cast<u32_t>(workerSlot), static_cast<u32_t>(fiberDepth))

class WINTERS_ENGINE CProfileScope
{
public:
	explicit CProfileScope(const char* pName)
		: m_bActive(Winters_Profile_Push(pName))
	{
	}

	~CProfileScope()
	{
		if (m_bActive)
			Winters_Profile_Pop();
	}

	CProfileScope(const CProfileScope&) = delete;
	CProfileScope& operator=(const CProfileScope&) = delete;

private:
	bool_t m_bActive = false;
};

#define WINTERS_PROFILE_CAT_INNER(a, b) a##b
#define WINTERS_PROFILE_CAT(a, b)       WINTERS_PROFILE_CAT_INNER(a, b)
#define WINTERS_PROFILE_SCOPE(name) \
	::CProfileScope WINTERS_PROFILE_CAT(_winProfScope_, __LINE__)(name)

#else
#define WINTERS_PROFILE_SET_CPU_PROFILER(pProfiler) ((void)0)
#define WINTERS_PROFILE_COUNT(name, delta) ((void)0)
#define WINTERS_PROFILE_THREAD_CONTEXT(workerSlot, fiberDepth) ((void)0)
#define WINTERS_PROFILE_SCOPE(name) ((void)0)
#endif
```

1-5. C:/Users/user/Desktop/Winters/Engine/Private/GameInstance.cpp

기존 코드:

```cpp
#include "Manager/Profiler/ProfilerOverlay.h"
#include "ECS/Systems/EntityBlueprintRegistry.h"
```

아래에 추가:

```cpp
#include "ProfilerAPI.h"
```

기존 코드:

```cpp
	m_pProfiler = CCPUProfiler::Create();
	if (!m_pProfiler) return E_FAIL;
	m_pProfilerOverlay = CProfilerOverlay::Create(m_pProfiler.get());
```

아래로 교체:

```cpp
	m_pProfiler = CCPUProfiler::Create();
	if (!m_pProfiler) return E_FAIL;
	WINTERS_PROFILE_SET_CPU_PROFILER(m_pProfiler.get());
	m_pProfilerOverlay = CProfilerOverlay::Create(m_pProfiler.get());
```

기존 코드:

```cpp
	m_pProfilerOverlay.reset();
	m_pProfiler.reset();
```

아래로 교체:

```cpp
	WINTERS_PROFILE_SET_CPU_PROFILER(nullptr);
	m_pProfilerOverlay.reset();
	m_pProfiler.reset();
```

1-6. C:/Users/user/Desktop/Winters/Engine/Private/Framework/CEngineApp.cpp

`CEngineApp::Run` 안에서 아래 기존 코드 전체를 교체:

기존 코드:

```cpp
    while (m_bRunning)
    {
        CGameInstance::Get()->Profiler_Begin();
        WINTERS_PROFILE_SCOPE("Frame");

        if (CInput::Get().IsKeyPressed(VK_F3))
            CGameInstance::Get()->Profiler_Toggle();

        if (!m_Window.PumpMessages())
        {
            m_bRunning = false;
            break;
        }

        CGameInstance::Get()->Update_TimeDelta(L"Timer_Default");
        float32 deltaTime = CGameInstance::Get()->Get_TimeDelta(L"Timer_Default");

        CGameInstance::Get()->Tick_Engine();

        {
            WINTERS_PROFILE_SCOPE("Update");
            Update(deltaTime);
        }
        {
            WINTERS_PROFILE_SCOPE("Render");
            Render();
        }

        CGameInstance::Get()->Profiler_End();

        CInput::Get().EndFrame();
    }
```

아래로 교체:

```cpp
    while (m_bRunning)
    {
        CGameInstance::Get()->Profiler_Begin();

        {
            WINTERS_PROFILE_SCOPE("Frame");

            if (CInput::Get().IsKeyPressed(VK_F3))
                CGameInstance::Get()->Profiler_Toggle();

            if (!m_Window.PumpMessages())
            {
                m_bRunning = false;
            }
            else
            {
                CGameInstance::Get()->Update_TimeDelta(L"Timer_Default");
                float32 deltaTime = CGameInstance::Get()->Get_TimeDelta(L"Timer_Default");

                CGameInstance::Get()->Tick_Engine();

                {
                    WINTERS_PROFILE_SCOPE("Update");
                    Update(deltaTime);
                }
                {
                    WINTERS_PROFILE_SCOPE("Render");
                    Render();
                }
            }
        }

        CGameInstance::Get()->Profiler_End();

        if (!m_bRunning)
            break;

        CInput::Get().EndFrame();
    }
```

1-7. C:/Users/user/Desktop/Winters/Engine/Public/Manager/Profiler/ProfilerOverlay.h

기존 코드:

```cpp
#pragma once
#include "Core/Profiler/ProfilerStableView.h"

#include <memory>

class CProfilerOverlay final
{
public:
	static std::unique_ptr<CProfilerOverlay> Create(class CCPUProfiler* pCPU);

	void Toggle() { m_bShow = !m_bShow; }
	void Draw();

private:
	CProfilerOverlay() = default;
	void Draw_ScopeSummary();
	void Draw_Counters();
	void Draw_FrameTree();

	CCPUProfiler* m_pCPU = nullptr;
	CProfilerStableView m_StableView{};
	bool m_bShow = true;
};
```

아래로 교체:

```cpp
#pragma once
#include "Core/Profiler/ProfilerStableView.h"

#include <memory>

class CProfilerOverlay final
{
public:
	static std::unique_ptr<CProfilerOverlay> Create(class CCPUProfiler* pCPU);

	void Toggle() { m_bShow = !m_bShow; }
	void Draw();

private:
	CProfilerOverlay() = default;
	void Draw_CaptureControls();
	void Draw_ScopeSummary();
	void Draw_Counters();
	void Draw_FrameTree();

	CCPUProfiler* m_pCPU = nullptr;
	CProfilerStableView m_StableView{};
	bool m_bShow = true;
};
```

1-8. C:/Users/user/Desktop/Winters/Engine/Private/Manager/Profiler/ProfilerOverlay.cpp

기존 코드:

```cpp
void CProfilerOverlay::Draw()
{
	if (!m_bShow || !m_pCPU)
		return;

	if (!ImGui::Begin("Profiler", &m_bShow))
	{
		ImGui::End();
		return;
	}

	Draw_ScopeSummary();
	Draw_Counters();

	if (ImGui::CollapsingHeader("Raw events"))
		Draw_FrameTree();

	ImGui::End();
}
```

아래로 교체:

```cpp
void CProfilerOverlay::Draw()
{
	if (!m_bShow || !m_pCPU)
		return;

	if (!ImGui::Begin("Profiler", &m_bShow))
	{
		ImGui::End();
		return;
	}

	Draw_CaptureControls();
	Draw_ScopeSummary();
	Draw_Counters();

	if (ImGui::CollapsingHeader("Raw events"))
		Draw_FrameTree();

	ImGui::End();
}

void CProfilerOverlay::Draw_CaptureControls()
{
	const char* modes[] = { "Off", "Stats", "Stats + Tree" };
	int selected = static_cast<int>(m_pCPU->GetCaptureMode());
	if (ImGui::Combo("Capture", &selected, modes, IM_ARRAYSIZE(modes)))
	{
		m_pCPU->SetCaptureMode(static_cast<eProfilerCaptureMode>(selected));
		if (static_cast<eProfilerCaptureMode>(selected) == eProfilerCaptureMode::Off)
			m_StableView.Reset();
	}

	const ProfilerRuntimeStats runtime = m_pCPU->Get_RuntimeStats();
	ImGui::Text("Threads %u | Drops E/S/C/T %llu/%llu/%llu/%llu",
		runtime.registeredThreads,
		static_cast<unsigned long long>(runtime.droppedEvents),
		static_cast<unsigned long long>(runtime.droppedScopeStats),
		static_cast<unsigned long long>(runtime.droppedCounters),
		static_cast<unsigned long long>(runtime.droppedThreadRegistrations));
}
```

기존 코드:

```cpp
void CProfilerOverlay::Draw_FrameTree()
{
	const auto& events = m_pCPU->Get_LastFrameEvents();
	const f64_t toMs = m_pCPU->Get_TicksToMs();

	for (const auto& ev : events)
	{
		const f64_t ms = static_cast<f64_t>(ev.endTicks - ev.startTicks) * toMs;
		ImGui::Text("%*s%-30s %7.3f ms", ev.depth * 2, "", ev.pName, ms);
	}
}
```

아래로 교체:

```cpp
void CProfilerOverlay::Draw_FrameTree()
{
	const auto& events = m_pCPU->Get_LastFrameEvents();
	const f64_t toMs = m_pCPU->Get_TicksToMs();

	for (const auto& ev : events)
	{
		const f64_t ms = static_cast<f64_t>(ev.endTicks - ev.startTicks) * toMs;
		ImGui::Text("%*s%-30s %7.3f ms  T%u W%u F%u",
			ev.depth * 2,
			"",
			ev.pName ? ev.pName : "(null)",
			ms,
			ev.threadId,
			ev.workerSlot,
			ev.fiberDepth);
	}
}
```

2. 검증

미검증:
- 빌드 미검증
- F3 Profiler overlay에서 `Frame` row가 현재 프레임으로 집계되는지 미검증
- `StatsOnly` 기본 모드에서 raw tree event가 쌓이지 않고 scope summary/counter만 갱신되는지 미검증
- `Stats + Tree` 모드에서 thread/worker/fiber 컬럼이 표시되는지 미검증

검증 명령:
- git diff --check
- msbuild Winters.sln /p:Configuration=Debug-DX12 /p:Platform=x64

확인 필요:
- 모든 `WINTERS_PROFILE_SCOPE` call site가 `CProfileScope` 반환값 기반 push/pop과 컴파일되는지 확인.
- `ProfilerRuntimeStats`의 Drops E/S/C/T가 정상 플레이 중 0인지 확인.
- `Profiler_Begin` 이후부터 `Profiler_End` 이전까지 JobSystem 작업이 모두 wait되는지 확인.

후속 동기화:
- Engine public header 변경 후 `UpdateLib.bat` 실행 필요.

Session - 2차: JobSystem FiberShell 실행을 Profiler thread context와 counters로 검증한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Engine/Private/Core/JobSystem.cpp

기존 코드:

```cpp
#include "WintersPCH.h"
#include "Core/JobSystem.h"
#include "Core/JobCounter.h"
```

아래에 추가:

```cpp
#include "ProfilerAPI.h"
```

기존 코드:

```cpp
void CJobSystem::WorkerLoop(std::uint32_t iWorkerIdx)
{
    t_iWorkerIdx = static_cast<std::int32_t>(iWorkerIdx);
    if (GetExecutionMode() == eJobExecutionMode::FiberShell && !IsThreadAFiber())
    {
        t_hThreadFiber = ConvertThreadToFiber(nullptr);
    }

    while (!m_bShutdown.load(std::memory_order_acquire))
    {
        if (!TryExecuteOneJob(iWorkerIdx))
            std::this_thread::yield();
    }

    if (t_hThreadFiber && IsThreadAFiber())
    {
        ConvertFiberToThread();
        t_hThreadFiber = nullptr;
    }
    t_iWorkerIdx = -1;
}
```

아래로 교체:

```cpp
void CJobSystem::WorkerLoop(std::uint32_t iWorkerIdx)
{
    t_iWorkerIdx = static_cast<std::int32_t>(iWorkerIdx);
    WINTERS_PROFILE_THREAD_CONTEXT(iWorkerIdx + 1u, 0u);

    if (GetExecutionMode() == eJobExecutionMode::FiberShell && !IsThreadAFiber())
    {
        WINTERS_PROFILE_COUNT("Fiber::ConvertThread", 1);
        t_hThreadFiber = ConvertThreadToFiber(nullptr);
        if (!t_hThreadFiber)
            WINTERS_PROFILE_COUNT("Fiber::ConvertThreadFail", 1);
    }

    while (!m_bShutdown.load(std::memory_order_acquire))
    {
        if (!TryExecuteOneJob(iWorkerIdx))
            std::this_thread::yield();
    }

    if (t_hThreadFiber && IsThreadAFiber())
    {
        ConvertFiberToThread();
        t_hThreadFiber = nullptr;
    }

    WINTERS_PROFILE_THREAD_CONTEXT(0u, 0u);
    t_iWorkerIdx = -1;
}
```

기존 코드:

```cpp
void CJobSystem::ExecuteItemInline(WorkItem& item)
{
    if (item.fn)
        item.fn();
    if (item.pCounter)
        item.pCounter->Decrement();
}
```

아래로 교체:

```cpp
void CJobSystem::ExecuteItemInline(WorkItem& item)
{
    if (item.fn)
    {
        item.fn();
        WINTERS_PROFILE_COUNT("JobSystem::Executed", 1);
    }

    if (item.pCounter)
        item.pCounter->Decrement();
}
```

기존 코드:

```cpp
bool CJobSystem::TryExecuteItemOnFiber(WorkItem& item)
{
    if (!IsThreadAFiber())
        t_hThreadFiber = ConvertThreadToFiber(nullptr);

    if (!t_hThreadFiber)
        return false;

    FiberShellCall call{};
    call.pSystem = this;
    call.pItem = &item;
    call.hReturnFiber = t_hThreadFiber;

    void* hJobFiber = CreateFiber(0, &CJobSystem::FiberShellEntry, &call);
    if (!hJobFiber)
        return false;

    SwitchToFiber(hJobFiber);
    DeleteFiber(hJobFiber);
    return true;
}
```

아래로 교체:

```cpp
bool CJobSystem::TryExecuteItemOnFiber(WorkItem& item)
{
    WINTERS_PROFILE_COUNT("Fiber::ShellAttempt", 1);

    if (!IsThreadAFiber())
    {
        WINTERS_PROFILE_COUNT("Fiber::ConvertThreadLazy", 1);
        t_hThreadFiber = ConvertThreadToFiber(nullptr);
    }

    if (!t_hThreadFiber)
    {
        WINTERS_PROFILE_COUNT("Fiber::NoThreadFiber", 1);
        return false;
    }

    FiberShellCall call{};
    call.pSystem = this;
    call.pItem = &item;
    call.hReturnFiber = t_hThreadFiber;

    void* hJobFiber = CreateFiber(0, &CJobSystem::FiberShellEntry, &call);
    if (!hJobFiber)
    {
        WINTERS_PROFILE_COUNT("Fiber::CreateFail", 1);
        return false;
    }

    WINTERS_PROFILE_COUNT("Fiber::ShellSwitch", 1);
    SwitchToFiber(hJobFiber);
    DeleteFiber(hJobFiber);
    WINTERS_PROFILE_COUNT("Fiber::ShellComplete", 1);
    return true;
}
```

기존 코드:

```cpp
void WINTERS_FIBER_CALL CJobSystem::FiberShellEntry(void* pParam)
{
    FiberShellCall* pCall = static_cast<FiberShellCall*>(pParam);
    t_bInsideJobFiber = true;
    if (pCall && pCall->pSystem && pCall->pItem)
        pCall->pSystem->ExecuteItemInline(*pCall->pItem);
    t_bInsideJobFiber = false;
    if (pCall && pCall->hReturnFiber)
        SwitchToFiber(pCall->hReturnFiber);
}
```

아래로 교체:

```cpp
void WINTERS_FIBER_CALL CJobSystem::FiberShellEntry(void* pParam)
{
    FiberShellCall* pCall = static_cast<FiberShellCall*>(pParam);
    const u32_t workerSlot = CJobSystem::Get_WorkerSlot();

    WINTERS_PROFILE_THREAD_CONTEXT(workerSlot, 1u);
    WINTERS_PROFILE_COUNT("Fiber::ShellRun", 1);

    t_bInsideJobFiber = true;
    if (pCall && pCall->pSystem && pCall->pItem)
        pCall->pSystem->ExecuteItemInline(*pCall->pItem);
    t_bInsideJobFiber = false;

    WINTERS_PROFILE_THREAD_CONTEXT(workerSlot, 0u);

    if (pCall && pCall->hReturnFiber)
        SwitchToFiber(pCall->hReturnFiber);
}
```

1-2. C:/Users/user/Desktop/Winters/Server/Private/Game/ServerEntry.cpp

기존 코드:

```cpp
#include "Game/ServerEntry.h"

#include <iostream>

std::atomic<bool_t> CServerEntry::s_bInitialized{ false };
CJobSystem CServerEntry::s_JobSystem{};
eJobExecutionMode CServerEntry::s_eExecutionMode{ eJobExecutionMode::ThreadOnly };

bool_t CServerEntry::Initialize(u32_t uWorkerCount, bool_t bEnableFiberShell)
{
	bool_t expected = false;
	//?닿쾶 ?대뼡 ?⑥닔瑜??몄텧??嫄곌퀬, memory_order_acq_rel? 臾댁뒯 ?섎?瑜?媛吏?
	if (!s_bInitialized.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
	{
		std::cerr << "[ServerEntry] Initialize called twice.\n";
		return false;
	}

	s_JobSystem.Initialize(uWorkerCount);

	//s_eExecutionMode = bEnableFiberShell

	return bool_t();
}

void CServerEntry::Shutdown()
{}

CJobSystem* CServerEntry::Get_JobSystem()
{
	return nullptr;
}

eJobExecutionMode CServerEntry::Get_ExecutionMode()
{
	return eJobExecutionMode();
}

bool_t CServerEntry::IsInitialized()
{
	return bool_t();
}
```

아래로 교체:

```cpp
#include "Game/ServerEntry.h"

#include <iostream>

std::atomic<bool_t> CServerEntry::s_bInitialized{ false };
CJobSystem CServerEntry::s_JobSystem{};
eJobExecutionMode CServerEntry::s_eExecutionMode{ eJobExecutionMode::ThreadOnly };

bool_t CServerEntry::Initialize(u32_t uWorkerCount, bool_t bEnableFiberShell)
{
	bool_t expected = false;
	if (!s_bInitialized.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
	{
		std::cerr << "[ServerEntry] Initialize called twice.\n";
		return false;
	}

	s_eExecutionMode = bEnableFiberShell
		? eJobExecutionMode::FiberShell
		: eJobExecutionMode::ThreadOnly;

	s_JobSystem.Initialize(uWorkerCount);
	s_JobSystem.SetExecutionMode(s_eExecutionMode);
	return true;
}

void CServerEntry::Shutdown()
{
	bool_t expected = true;
	if (!s_bInitialized.compare_exchange_strong(expected, false, std::memory_order_acq_rel))
		return;

	s_JobSystem.Shutdown();
	s_eExecutionMode = eJobExecutionMode::ThreadOnly;
}

CJobSystem* CServerEntry::Get_JobSystem()
{
	return s_bInitialized.load(std::memory_order_acquire) ? &s_JobSystem : nullptr;
}

eJobExecutionMode CServerEntry::Get_ExecutionMode()
{
	return s_eExecutionMode;
}

bool_t CServerEntry::IsInitialized()
{
	return s_bInitialized.load(std::memory_order_acquire);
}
```

2. 검증

미검증:
- 빌드 미검증
- `FiberShell` 모드에서 `Fiber::ShellAttempt`, `Fiber::ShellRun`, `Fiber::ShellComplete` 값이 같은지 미검증
- `Fiber::ConvertThreadFail`, `Fiber::CreateFail`, `Fiber::NoThreadFiber`가 정상 실행에서 0인지 미검증
- ServerEntry가 `bEnableFiberShell=true`일 때 실제로 `CJobSystem::SetExecutionMode(eJobExecutionMode::FiberShell)`까지 연결되는지 미검증

검증 명령:
- git diff --check
- msbuild Winters.sln /p:Configuration=Debug-DX12 /p:Platform=x64
- msbuild Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64

확인 필요:
- F3 Profiler overlay에서 JobSystem 작업이 실행될 때 `JobSystem::Executed` counter가 증가하는지 확인.
- `Stats + Tree` 모드에서 worker job scope가 `W1+`, FiberShell scope가 `F1`로 표시되는지 확인.
- 현재 코드는 full Fiber yield/resume 풀 모델이 아니라 per-job FiberShell이다. `WaitForCounter` yield, FiberPool reuse, cross-worker resume 검증은 `FiberPool/Waiter` 구현 세션에서 별도 계획서로 확정 필요.

후속 동기화:
- Engine public header 변경 후 `UpdateLib.bat` 실행 필요.
