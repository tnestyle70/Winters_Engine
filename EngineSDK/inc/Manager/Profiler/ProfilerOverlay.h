#pragma once
#include "Core/Profiler/ProfilerStableView.h"

#include <memory>
#include <string>
#include <vector>

class CProfilerOverlay final
{
public:
	static std::unique_ptr<CProfilerOverlay> Create(class CCPUProfiler* pCPU);

	void Toggle() { m_bShow = !m_bShow; }
	bool_t IsVisible() const { return m_bShow; }
	void Draw();
	bool_t CaptureToJson(const char* pPath, bool_t bForce = true);

private:
	CProfilerOverlay() = default;
	void Capture_DisplayFrame(bool_t bForce);
	void DrawCompactHeader();
	void Draw_ControlBar();
	void Draw_ScopeSummary();
	void Draw_Counters();
	void Draw_FrameTree();
	bool_t Save_DisplayFrameToJson(const char* pPath);

	CCPUProfiler* m_pCPU = nullptr;
	CProfilerStableView m_StableView{};
	std::vector<ProfilerScopeStat> m_vDisplayStats{};
	std::vector<ProfilerCounter> m_vDisplayCounters{};
	std::vector<ProfilerEvent> m_vDisplayEvents{};
	std::string m_strSaveStatus{};
	f64_t m_fNextSampleTime = 0.0;
	f32_t m_fSampleIntervalSec = 1.00f;
	bool_t m_bFreeze = false;
	bool_t m_bDetailsOpen = false;
	bool m_bShow = false;
};
