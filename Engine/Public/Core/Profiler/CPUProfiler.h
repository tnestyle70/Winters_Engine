#pragma once
#include "ProfilerTypes.h"
#include <vector>
#include <memory>
#include <mutex>
//계층 스코프 타이머
class CCPUProfiler final
{
public:
	~CCPUProfiler() = default;

	static std::unique_ptr<CCPUProfiler> Create();

	void BeginFrame();
	void EndFrame();
	void PushScope(const char* pName);
	void PopScope();

	//Overlay 읽기
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
	// 현재 프레임 이벤트 수집.
	// Scope stack 은 worker thread 별로 분리하고, event/counter merge 만 mutex 보호.
	std::vector<ProfilerEvent> m_vCurrentFrame{};
	std::vector<ProfilerEvent> m_vLastFrameEvents{};
	std::vector<ProfilerScopeStat> m_vCurrentScopeStats{};
	std::vector<ProfilerScopeStat> m_vLastScopeStats{};

	f64_t m_fTicksToMs = 0.f; //QueryPerformanceFrequency 역수 * 1000


	std::vector<ProfilerCounter> m_vCurrentCounters{};
	std::vector<ProfilerCounter> m_vLastCounters{};
	std::mutex m_Mutex{};
};
