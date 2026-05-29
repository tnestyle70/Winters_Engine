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
