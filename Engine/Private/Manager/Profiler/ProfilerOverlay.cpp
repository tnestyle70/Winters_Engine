#include "Manager/Profiler/ProfilerOverlay.h"
#include "Core/Profiler/CPUProfiler.h"

#include <algorithm>

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

std::unique_ptr<CProfilerOverlay> CProfilerOverlay::Create(CCPUProfiler* pCPU)
{
	auto p = std::unique_ptr<CProfilerOverlay>(new CProfilerOverlay());
	p->m_pCPU = pCPU;
	return p;
}

void CProfilerOverlay::Draw()
{
	if (!m_bShow || !m_pCPU)
		return;

	ImGui::SetNextWindowSize(ImVec2(420.f, 260.f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ImVec2(260.f, 120.f), ImVec2(760.f, 520.f));
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

void CProfilerOverlay::Draw_ScopeSummary()
{
	const auto& stats = m_pCPU->Get_LastFrameScopeStats();
	const f64_t toMs = m_pCPU->Get_TicksToMs();

	m_StableView.Update(stats, toMs);

	const u32_t visibleRows = m_StableView.Get_VisibleRowCount();
	ImGui::Text("Frame %7.3f ms | Scopes %zu | Stable rows %u | Raw samples %zu",
		m_StableView.Get_FrameMs(),
		stats.size(),
		visibleRows,
		m_pCPU->Get_LastFrameEvents().size());

	ImGui::Separator();
	ImGui::Text("%-34s %8s %7s %8s %8s %5s", "Scope", "Total", "Calls", "Avg", "Max", "Age");

	const size_t maxRows = 48u;
	size_t printedRows = 0;
	for (const auto& row : m_StableView.Get_Rows())
	{
		if (!row.bVisible)
			continue;
		if (printedRows >= maxRows)
			break;

		if (row.bLive)
		{
			ImGui::Text("%-34s %8.3f %7u %8.3f %8.3f %5u",
				row.name.c_str(),
				row.emaTotalMs,
				row.callCount,
				row.emaAvgMs,
				row.emaMaxMs,
				row.ageFrames);
		}
		else
		{
			ImGui::TextDisabled("%-34s %8.3f %7u %8.3f %8.3f %5u",
				row.name.c_str(),
				row.emaTotalMs,
				row.callCount,
				row.emaAvgMs,
				row.emaMaxMs,
				row.ageFrames);
		}

		++printedRows;
	}

	if (visibleRows > printedRows)
		ImGui::Text("... %u more stable scopes", visibleRows - static_cast<u32_t>(printedRows));
}

void CProfilerOverlay::Draw_Counters()
{
	ImGui::Separator();
	ImGui::Text("Counters (frame):");
	for (const auto& c : m_pCPU->Get_LastFrameCounters())
	{
		ImGui::Text("  %-28s %llu",
			c.pName ? c.pName : "(null)",
			static_cast<unsigned long long>(c.value));
	}
}

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
