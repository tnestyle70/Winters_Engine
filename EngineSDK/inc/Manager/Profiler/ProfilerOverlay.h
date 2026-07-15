#pragma once
#include "Core/Profiler/ProfilerStableView.h"

#include <memory>
#include <future>
#include <string>
#include <vector>

class CProfilerOverlay final
{
public:
	static std::unique_ptr<CProfilerOverlay> Create(class CCPUProfiler* pCPU);
	~CProfilerOverlay();

	void Toggle() { m_bShow = !m_bShow; }
	bool_t IsVisible() const { return m_bShow; }
	void Draw();
	bool_t CaptureToJson(
		const char* pPath,
		bool_t bForce = true,
		const char* pAliasPath = nullptr);

private:
	struct DisplayFrameSaveData
	{
		std::vector<ProfilerScopeStat> stats{};
		std::vector<ProfilerCounter> counters{};
		std::vector<ProfilerEvent> events{};
		std::vector<CProfilerStableView::Row> stableRows{};
		f64_t ticksToMs = 0.0;
		f64_t frameMs = 0.0;
		f32_t sampleIntervalSec = 0.f;
		bool_t bFrozen = false;
	};

	CProfilerOverlay() = default;
	void Capture_DisplayFrame(bool_t bForce);
	void DrawCompactHeader();
	void Draw_ControlBar();
	void Draw_ScopeSummary();
	void Draw_Counters();
	void Draw_FrameTree();
	void ApplyHistoryConfig();
	void PollSaveResult();
	bool_t IsSaveInProgress() const;
	static bool_t Save_DisplayFrameToJson(
		const char* pPath,
		const DisplayFrameSaveData& frame);
	static bool_t Save_TimelineToJson(
		const char* pPath,
		const std::vector<ProfilerFrameRecord>& frames,
		f64_t ticksToMs,
		const ProfilerHistoryConfig& config);

	CCPUProfiler* m_pCPU = nullptr;
	CProfilerStableView m_StableView{};
	std::vector<ProfilerScopeStat> m_vDisplayStats{};
	std::vector<ProfilerCounter> m_vDisplayCounters{};
	std::vector<ProfilerEvent> m_vDisplayEvents{};
	std::string m_strSaveStatus{};
	std::future<bool_t> m_SaveFuture{};
	f64_t m_fNextSampleTime = 0.0;
	f32_t m_fSampleIntervalSec = 1.00f;
	int m_iHistoryEveryNFrames = 1;
	int m_iHistoryEveryNServerTicks = 0;
	int m_iHistoryMaxFrames = 300;
	bool_t m_bHistoryEnabled = true;
	bool_t m_bFreeze = false;
	bool_t m_bDetailsOpen = false;
	bool m_bShow = false;
};
