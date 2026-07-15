#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "Core/Profiler/CPUProfiler.h"
#include "GameInstance.h"
#include "ProfilerAPI.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <limits>

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

	uint64_t FindCounterValue(
		const std::vector<ProfilerCounter>& counters,
		const char* pName,
		bool_t& bFound)
	{
		for (const ProfilerCounter& counter : counters)
		{
			if (SameProfilerName(counter.pName, pName))
			{
				bFound = true;
				return counter.value;
			}
		}

		bFound = false;
		return 0;
	}

	f64_t FindFrameMs(
		const std::vector<ProfilerScopeStat>& stats,
		f64_t ticksToMs)
	{
		for (const ProfilerScopeStat& stat : stats)
		{
			if (SameProfilerName(stat.pName, "Frame"))
				return static_cast<f64_t>(stat.totalTicks) * ticksToMs;
		}

		return 0.0;
	}

	void AddProfilerCounterDirect(
		std::vector<ProfilerCounter>& counters,
		const char* pName,
		uint64_t value,
		uint32_t& droppedCounters)
	{
		for (ProfilerCounter& counter : counters)
		{
			if (SameProfilerName(counter.pName, pName))
			{
				counter.value += value;
				return;
			}
		}
		if (counters.size() < PROFILER_MAX_COUNTERS_PER_FRAME)
			counters.push_back({ pName, value });
		else
			++droppedCounters;
	}

	uint32_t ReadProfilerEnvU32(const char* pName, uint32_t fallback)
	{
		char buffer[32]{};
		size_t valueLength = 0;
		if (getenv_s(&valueLength, buffer, sizeof(buffer), pName) != 0 ||
			valueLength == 0)
		{
			return fallback;
		}

		char* pEnd = nullptr;
		const unsigned long parsed = std::strtoul(buffer, &pEnd, 10);
		if (pEnd == buffer)
			return fallback;
		return static_cast<uint32_t>(parsed);
	}

	VOID CALLBACK DestroyProfilerFiberStack(void* pValue)
	{
		delete static_cast<std::vector<PendingProfilerScope>*>(pValue);
	}

	DWORD GetProfilerFiberStackIndex()
	{
		static const DWORD index = FlsAlloc(&DestroyProfilerFiberStack);
		return index;
	}

	void ReportProfilerFlsFallbackOnce()
	{
		static std::atomic<bool> reported{ false };
		bool expected = false;
		if (reported.compare_exchange_strong(
			expected,
			true,
			std::memory_order_relaxed))
		{
			OutputDebugStringA(
				"[CPUProfiler] FLS unavailable; fiber scope nesting may be degraded.\n");
		}
	}

	std::vector<PendingProfilerScope>& GetProfilerFiberStack()
	{
		// FLS follows the active Windows fiber automatically and behaves like TLS
		// on ordinary threads. This keeps nested scope state attached to the parked
		// stack instead of the worker OS thread that happens to resume another fiber.
		static thread_local std::vector<PendingProfilerScope> fallbackStack;
		const DWORD index = GetProfilerFiberStackIndex();
		if (index == FLS_OUT_OF_INDEXES)
		{
			ReportProfilerFlsFallbackOnce();
			return fallbackStack;
		}

		auto* pStack = static_cast<std::vector<PendingProfilerScope>*>(
			FlsGetValue(index));
		if (pStack)
			return *pStack;

		try
		{
			pStack = new std::vector<PendingProfilerScope>();
		}
		catch (...)
		{
			ReportProfilerFlsFallbackOnce();
			return fallbackStack;
		}
		if (!FlsSetValue(index, pStack))
		{
			delete pStack;
			ReportProfilerFlsFallbackOnce();
			return fallbackStack;
		}
		return *pStack;
	}
}

std::unique_ptr<CCPUProfiler> CCPUProfiler::Create()
{
	auto pInst = std::unique_ptr<CCPUProfiler>(new CCPUProfiler());
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);
	pInst->m_fTicksToMs = 1000.0 / static_cast<f64_t>(freq.QuadPart);
#ifdef WINTERS_PROFILING
	ProfilerHistoryConfig config{};
	config.sampleEveryNFrames = ReadProfilerEnvU32(
		"WINTERS_PROFILE_FRAME_STRIDE",
		config.sampleEveryNFrames);
	config.sampleEveryNServerTicks = ReadProfilerEnvU32(
		"WINTERS_PROFILE_TICK_STRIDE",
		config.sampleEveryNServerTicks);
	config.maxFrames = ReadProfilerEnvU32(
		"WINTERS_PROFILE_HISTORY_FRAMES",
		config.maxFrames);
	pInst->SetHistoryConfig(config);
#endif
	return pInst;
}

void CCPUProfiler::BeginFrame()
{
}

void CCPUProfiler::EndFrame()
{
	LARGE_INTEGER mergeStart{};
	QueryPerformanceCounter(&mergeStart);
	std::lock_guard<std::mutex> lk(m_Mutex);
	++m_uRenderFrame;
	m_vLastFrameEvents.swap(m_vCurrentFrame);
	m_vCurrentFrame.clear();
	m_vLastCounters.swap(m_vCurrentCounters);
	m_vCurrentCounters.clear();
	m_vLastScopeStats.swap(m_vCurrentScopeStats);
	m_vCurrentScopeStats.clear();
	m_uLastDroppedScopeStats = m_uCurrentDroppedScopeStats;
	m_uCurrentDroppedScopeStats = 0;
	m_uLastDroppedCounters = m_uCurrentDroppedCounters;
	m_uCurrentDroppedCounters = 0;
	m_uLastDroppedRawEvents = m_uCurrentDroppedRawEvents;
	m_uCurrentDroppedRawEvents = 0;

	std::stable_sort(
		m_vLastFrameEvents.begin(),
		m_vLastFrameEvents.end(),
		[](const ProfilerEvent& lhs, const ProfilerEvent& rhs)
		{
			if (lhs.startTicks != rhs.startTicks)
				return lhs.startTicks < rhs.startTicks;
			if (lhs.threadId != rhs.threadId)
				return lhs.threadId < rhs.threadId;
			return lhs.depth < rhs.depth;
		});

	AddProfilerCounterDirect(
		m_vLastCounters,
		"Profiler::ScopeCalls",
		m_uScopeCalls.exchange(0, std::memory_order_relaxed),
		m_uLastDroppedCounters);
	AddProfilerCounterDirect(
		m_vLastCounters,
		"Profiler::CounterCalls",
		m_uCounterCalls.exchange(0, std::memory_order_relaxed),
		m_uLastDroppedCounters);
	AddProfilerCounterDirect(
		m_vLastCounters,
		"Profiler::GaugeCalls",
		m_uGaugeCalls.exchange(0, std::memory_order_relaxed),
		m_uLastDroppedCounters);
	AddProfilerCounterDirect(
		m_vLastCounters,
		"Profiler::PreviousEndFrameUs",
		m_uPreviousEndFrameUs,
		m_uLastDroppedCounters);
	AddProfilerCounterDirect(
		m_vLastCounters,
		"Profiler::HistoryFrames",
		static_cast<uint64_t>(m_History.size()),
		m_uLastDroppedCounters);

	LARGE_INTEGER mergeEnd{};
	QueryPerformanceCounter(&mergeEnd);
	const uint64_t mergeUs = static_cast<uint64_t>(
		static_cast<f64_t>(mergeEnd.QuadPart - mergeStart.QuadPart) *
		m_fTicksToMs * 1000.0);
	AddProfilerCounterDirect(
		m_vLastCounters,
		"Profiler::PreHistoryMergeUs",
		mergeUs,
		m_uLastDroppedCounters);

	if (m_bHistoryEnabled)
	{
		bool_t bHasServerTick = false;
		const uint64_t serverTick = FindCounterValue(
			m_vLastCounters,
			"Net::LatestServerTick",
			bHasServerTick);
		const uint32_t frameStride = std::max(1u, m_HistoryConfig.sampleEveryNFrames);
		const bool_t bFrameStrideSample = (m_uRenderFrame % frameStride) == 0;
		bool_t bServerTickSample = false;

		if (m_HistoryConfig.sampleEveryNServerTicks > 0 && bHasServerTick)
		{
			const uint32_t tickStride = m_HistoryConfig.sampleEveryNServerTicks;
			bServerTickSample = serverTick != m_uLastCapturedServerTick &&
				(serverTick % tickStride) == 0;
		}

		if (bFrameStrideSample || bServerTickSample)
		{
			ProfilerFrameRecord record{};
			record.renderFrame = m_uRenderFrame;
			record.serverTick = serverTick;
			record.bHasServerTick = bHasServerTick;
			record.bFrameStrideSample = bFrameStrideSample;
			record.bServerTickSample = bServerTickSample;
			record.frameMs = FindFrameMs(m_vLastScopeStats, m_fTicksToMs);
			record.gpuFrameUs = FindCounterValue(
				m_vLastCounters,
				"GPU::FrameUs",
				record.bHasGpuFrameTime);
			record.gpuSourceRhiFrame = FindCounterValue(
				m_vLastCounters,
				"GPU::SourceRHIFrame",
				record.bHasGpuSourceRhiFrame);
			record.droppedScopeStats = m_uLastDroppedScopeStats;
			record.droppedCounters = m_uLastDroppedCounters;
			record.droppedRawEvents = m_uLastDroppedRawEvents;
			record.events = m_vLastFrameEvents;
			record.scopeStats = m_vLastScopeStats;
			record.counters = m_vLastCounters;
			m_History.push_back(std::move(record));
			if (m_History.size() > PROFILER_MAX_RAW_HISTORY_FRAMES)
			{
				ProfilerFrameRecord& expiredRecord =
					m_History[m_History.size() - PROFILER_MAX_RAW_HISTORY_FRAMES - 1u];
				expiredRecord.bRawEventsRetained = false;
				expiredRecord.omittedRawEvents +=
					static_cast<uint32_t>(expiredRecord.events.size());
				std::vector<ProfilerEvent>().swap(expiredRecord.events);
			}

			if (bServerTickSample)
				m_uLastCapturedServerTick = serverTick;

			const size_t maxFrames = static_cast<size_t>(
				std::clamp(
					m_HistoryConfig.maxFrames,
					1u,
					PROFILER_MAX_HISTORY_FRAMES));
			while (m_History.size() > maxFrames)
				m_History.pop_front();
		}
	}

#ifdef WINTERS_PROFILING
	// 프레임 확정 카운터를 Tracy plot 으로 전송 (이름은 string literal 이라 수명 보장).
	for (const auto& c : m_vLastCounters)
		TracyPlot(c.pName, static_cast<int64_t>(c.value));
#endif

	LARGE_INTEGER endFrameEnd{};
	QueryPerformanceCounter(&endFrameEnd);
	m_uPreviousEndFrameUs = static_cast<uint64_t>(
		static_cast<f64_t>(endFrameEnd.QuadPart - mergeStart.QuadPart) *
		m_fTicksToMs * 1000.0);
}

void CCPUProfiler::PushScope(const char* pName)
{
	LARGE_INTEGER t;
	QueryPerformanceCounter(&t);
	auto& profilerStack = GetProfilerFiberStack();
	profilerStack.push_back({
		pName,
		static_cast<uint64_t>(t.QuadPart),
		static_cast<uint32_t>(profilerStack.size())
		});
}

void CCPUProfiler::PopScope()
{
	auto& profilerStack = GetProfilerFiberStack();
	if (profilerStack.empty())
		return;

	LARGE_INTEGER t;
	QueryPerformanceCounter(&t);
	auto top = profilerStack.back();
	profilerStack.pop_back();

	ProfilerEvent event{
		top.pName,
		top.startTicks,
		static_cast<uint64_t>(t.QuadPart),
		top.depth,
		GetCurrentThreadId()
	};

	const uint64_t elapsedTicks = event.endTicks - event.startTicks;
	m_uScopeCalls.fetch_add(1, std::memory_order_relaxed);

	std::lock_guard<std::mutex> lk(m_Mutex);
	if (m_vCurrentFrame.size() < PROFILER_MAX_TREE_EVENTS_PER_FRAME)
		m_vCurrentFrame.push_back(event);
	else
		++m_uCurrentDroppedRawEvents;

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
	else
	{
		++m_uCurrentDroppedScopeStats;
	}
}

void CCPUProfiler::AddCounter(const char* pName, uint64_t delta)
{
	m_uCounterCalls.fetch_add(1, std::memory_order_relaxed);
	std::lock_guard<std::mutex> lk(m_Mutex);
	for (auto& c : m_vCurrentCounters)
	{
		// 포인터 비교는 DLL/번역단위 간 동일 리터럴이 다른 주소를 가질 때
		// 같은 카운터를 중복 행으로 만든다 (scope 통계와 동일하게 strcmp 비교).
		if (SameProfilerName(c.pName, pName))
		{
			c.value += delta;
			return;
		}
	}

	if (m_vCurrentCounters.size() < PROFILER_MAX_COUNTERS_PER_FRAME)
		m_vCurrentCounters.push_back({ pName, delta });
	else
		++m_uCurrentDroppedCounters;
}

void CCPUProfiler::SetCounter(const char* pName, uint64_t value)
{
	m_uGaugeCalls.fetch_add(1, std::memory_order_relaxed);
	std::lock_guard<std::mutex> lk(m_Mutex);
	for (ProfilerCounter& counter : m_vCurrentCounters)
	{
		if (SameProfilerName(counter.pName, pName))
		{
			counter.value = value;
			return;
		}
	}

	if (m_vCurrentCounters.size() < PROFILER_MAX_COUNTERS_PER_FRAME)
		m_vCurrentCounters.push_back({ pName, value });
	else
		++m_uCurrentDroppedCounters;
}

void CCPUProfiler::SetHistoryEnabled(bool_t bEnabled)
{
	std::lock_guard<std::mutex> lk(m_Mutex);
	if (m_bHistoryEnabled != bEnabled)
	{
		m_History.clear();
		m_uLastCapturedServerTick = std::numeric_limits<uint64_t>::max();
	}
	m_bHistoryEnabled = bEnabled;
}

bool_t CCPUProfiler::IsHistoryEnabled() const
{
	std::lock_guard<std::mutex> lk(m_Mutex);
	return m_bHistoryEnabled;
}

void CCPUProfiler::SetHistoryConfig(const ProfilerHistoryConfig& config)
{
	std::lock_guard<std::mutex> lk(m_Mutex);
	ProfilerHistoryConfig next{};
	next.sampleEveryNFrames = std::max(1u, config.sampleEveryNFrames);
	next.sampleEveryNServerTicks = config.sampleEveryNServerTicks;
	next.maxFrames = std::clamp(
		config.maxFrames,
		1u,
		PROFILER_MAX_HISTORY_FRAMES);
	if (next.sampleEveryNFrames != m_HistoryConfig.sampleEveryNFrames ||
		next.sampleEveryNServerTicks != m_HistoryConfig.sampleEveryNServerTicks ||
		next.maxFrames != m_HistoryConfig.maxFrames)
	{
		m_History.clear();
		m_uLastCapturedServerTick = std::numeric_limits<uint64_t>::max();
	}
	m_HistoryConfig = next;
	while (m_History.size() > m_HistoryConfig.maxFrames)
		m_History.pop_front();
}

ProfilerHistoryConfig CCPUProfiler::GetHistoryConfig() const
{
	std::lock_guard<std::mutex> lk(m_Mutex);
	return m_HistoryConfig;
}

void CCPUProfiler::ClearHistory()
{
	std::lock_guard<std::mutex> lk(m_Mutex);
	m_History.clear();
	m_uLastCapturedServerTick = std::numeric_limits<uint64_t>::max();
}

std::vector<ProfilerFrameRecord> CCPUProfiler::TakeHistory()
{
	std::lock_guard<std::mutex> lk(m_Mutex);
	std::vector<ProfilerFrameRecord> frames;
	frames.reserve(m_History.size());
	while (!m_History.empty())
	{
		frames.push_back(std::move(m_History.front()));
		m_History.pop_front();
	}
	return frames;
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

void Winters_Profile_Gauge(const char* pName, uint64_t value)
{
	auto* pProf = CGameInstance::Get()->Get_CPUProfiler();
	if (pProf) pProf->SetCounter(pName, value);
}
#endif
