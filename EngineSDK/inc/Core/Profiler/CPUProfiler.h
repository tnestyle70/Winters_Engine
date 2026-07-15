#pragma once
#include "ProfilerTypes.h"
#include <atomic>
#include <deque>
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
	void SetCounter(const char* pName, uint64_t value);
	void SetHistoryEnabled(bool_t bEnabled);
	bool_t IsHistoryEnabled() const;
	void SetHistoryConfig(const ProfilerHistoryConfig& config);
	ProfilerHistoryConfig GetHistoryConfig() const;
	void ClearHistory();
	std::vector<ProfilerFrameRecord> TakeHistory();

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
	std::deque<ProfilerFrameRecord> m_History{};
	ProfilerHistoryConfig m_HistoryConfig{};
	uint64_t m_uRenderFrame = 0;
	uint64_t m_uLastCapturedServerTick = ~uint64_t{ 0 };
	uint32_t m_uCurrentDroppedScopeStats = 0;
	uint32_t m_uLastDroppedScopeStats = 0;
	uint32_t m_uCurrentDroppedCounters = 0;
	uint32_t m_uLastDroppedCounters = 0;
	uint32_t m_uCurrentDroppedRawEvents = 0;
	uint32_t m_uLastDroppedRawEvents = 0;
	std::atomic<uint64_t> m_uScopeCalls{ 0 };
	std::atomic<uint64_t> m_uCounterCalls{ 0 };
	std::atomic<uint64_t> m_uGaugeCalls{ 0 };
	uint64_t m_uPreviousEndFrameUs = 0;
#ifdef WINTERS_PROFILING
	bool_t m_bHistoryEnabled = true;
#else
	bool_t m_bHistoryEnabled = false;
#endif
	mutable std::mutex m_Mutex{};
};
