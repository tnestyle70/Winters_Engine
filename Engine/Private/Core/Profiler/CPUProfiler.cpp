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

#ifdef WINTERS_PROFILING
	// 프레임 확정 카운터를 Tracy plot 으로 전송 (이름은 string literal 이라 수명 보장).
	for (const auto& c : m_vLastCounters)
		TracyPlot(c.pName, static_cast<int64_t>(c.value));
#endif
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
