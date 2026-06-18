#include "Manager/Profiler/ProfilerOverlay.h"
#include "Core/Profiler/CPUProfiler.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <ostream>
#include <string>

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

namespace
{
	const char* SafeProfilerName(const char* pName)
	{
		return pName ? pName : "(null)";
	}

	void WriteJsonString(std::ostream& out, const char* pText)
	{
		out << '"';
		const unsigned char* p = reinterpret_cast<const unsigned char*>(SafeProfilerName(pText));
		for (; *p; ++p)
		{
			switch (*p)
			{
			case '\\': out << "\\\\"; break;
			case '"':  out << "\\\""; break;
			case '\b': out << "\\b"; break;
			case '\f': out << "\\f"; break;
			case '\n': out << "\\n"; break;
			case '\r': out << "\\r"; break;
			case '\t': out << "\\t"; break;
			default:
				if (*p < 0x20)
				{
					out << "\\u"
						<< std::hex << std::uppercase << std::setw(4)
						<< std::setfill('0') << static_cast<int>(*p)
						<< std::dec << std::nouppercase << std::setfill(' ');
				}
				else
				{
					out << static_cast<char>(*p);
				}
				break;
			}
		}
		out << '"';
	}

	void WriteJsonString(std::ostream& out, const std::string& text)
	{
		WriteJsonString(out, text.c_str());
	}
}

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

	if (m_bDetailsOpen)
	{
		ImGui::SetNextWindowSize(ImVec2(420.f, 260.f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSizeConstraints(ImVec2(260.f, 120.f), ImVec2(760.f, 520.f));
	}
	else
	{
		ImGui::SetNextWindowSize(ImVec2(360.f, 72.f), ImGuiCond_Always);
	}

	const ImGuiWindowFlags flags = m_bDetailsOpen
		? 0
		: ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;

	if (!ImGui::Begin("Profiler", &m_bShow, flags))
	{
		ImGui::End();
		return;
	}

	Capture_DisplayFrame(false);
	DrawCompactHeader();

	if (m_bDetailsOpen)
	{
		Draw_ControlBar();
		Draw_ScopeSummary();
		Draw_Counters();

		if (ImGui::CollapsingHeader("Raw events"))
			Draw_FrameTree();
	}

	ImGui::End();
}

void CProfilerOverlay::Capture_DisplayFrame(bool_t bForce)
{
	if (!m_pCPU)
		return;
	if (m_bFreeze && !bForce)
		return;

	const f64_t now = ImGui::GetTime();
	if (!bForce && !m_vDisplayStats.empty() && now < m_fNextSampleTime)
		return;

	m_vDisplayStats = m_pCPU->Get_LastFrameScopeStats();
	m_vDisplayCounters = m_pCPU->Get_LastFrameCounters();
	m_vDisplayEvents = m_pCPU->Get_LastFrameEvents();
	m_StableView.Update(m_vDisplayStats, m_pCPU->Get_TicksToMs());
	m_fNextSampleTime = now + std::max(0.05f, m_fSampleIntervalSec);
}

bool_t CProfilerOverlay::CaptureToJson(const char* pPath, bool_t bForce)
{
	if (!m_pCPU || !pPath || pPath[0] == '\0')
		return false;

	if (bForce || m_vDisplayStats.empty())
		Capture_DisplayFrame(true);

	return Save_DisplayFrameToJson(pPath);
}

void CProfilerOverlay::DrawCompactHeader()
{
	ImGui::Text("Frame %7.3f ms | Scopes %zu | Counters %zu",
		m_StableView.Get_FrameMs(),
		m_vDisplayStats.size(),
		m_vDisplayCounters.size());

	ImGui::SameLine();
	if (ImGui::SmallButton(m_bDetailsOpen ? "Hide" : "Details"))
		m_bDetailsOpen = !m_bDetailsOpen;

	ImGui::SameLine();
	if (ImGui::SmallButton("Save"))
	{
		if (!m_bFreeze)
			Capture_DisplayFrame(true);
		const bool_t bSaved = Save_DisplayFrameToJson("profiler.json");
		m_strSaveStatus = bSaved ? "Saved profiler.json" : "Save failed: profiler.json";
	}

	if (!m_strSaveStatus.empty())
	{
		ImGui::SameLine();
		ImGui::TextDisabled("%s", m_strSaveStatus.c_str());
	}
}

void CProfilerOverlay::Draw_ControlBar()
{
	if (ImGui::Button(m_bFreeze ? "Live" : "Freeze"))
	{
		m_bFreeze = !m_bFreeze;
		if (!m_bFreeze)
			m_fNextSampleTime = 0.0;
	}

	ImGui::SameLine();
	if (ImGui::Button("Reset"))
	{
		m_StableView.Reset();
		m_vDisplayStats.clear();
		m_vDisplayCounters.clear();
		m_vDisplayEvents.clear();
		m_fNextSampleTime = 0.0;
		Capture_DisplayFrame(true);
	}

	ImGui::SameLine();
	ImGui::SetNextItemWidth(120.f);
	if (ImGui::SliderFloat("Sample", &m_fSampleIntervalSec, 0.05f, 1.00f, "%.2fs"))
	{
		m_fSampleIntervalSec = std::clamp(m_fSampleIntervalSec, 0.05f, 1.00f);
		m_fNextSampleTime = 0.0;
	}

	ImGui::SameLine();
	if (ImGui::Button("Save"))
	{
		if (!m_bFreeze)
			Capture_DisplayFrame(true);
		const bool_t bSaved = Save_DisplayFrameToJson("profiler.json");
		m_strSaveStatus = bSaved ? "Saved profiler.json" : "Save failed: profiler.json";
	}

	if (!m_strSaveStatus.empty())
		ImGui::TextDisabled("%s", m_strSaveStatus.c_str());
}

void CProfilerOverlay::Draw_ScopeSummary()
{
	const u32_t visibleRows = m_StableView.Get_VisibleRowCount();
	ImGui::Text("Frame %7.3f ms | Scopes %zu | Stable rows %u | Raw samples %zu%s",
		m_StableView.Get_FrameMs(),
		m_vDisplayStats.size(),
		visibleRows,
		m_vDisplayEvents.size(),
		m_bFreeze ? " | Frozen" : "");

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
	for (const auto& c : m_vDisplayCounters)
	{
		ImGui::Text("  %-28s %llu",
			c.pName ? c.pName : "(null)",
			static_cast<unsigned long long>(c.value));
	}
}

void CProfilerOverlay::Draw_FrameTree()
{
	const f64_t toMs = m_pCPU->Get_TicksToMs();

	const size_t maxRawEvents = 128u;
	size_t printedEvents = 0;
	for (const auto& ev : m_vDisplayEvents)
	{
		if (printedEvents >= maxRawEvents)
			break;

		const f64_t ms = static_cast<f64_t>(ev.endTicks - ev.startTicks) * toMs;
		ImGui::Text("%*s%-30s %7.3f ms", ev.depth * 2, "", ev.pName, ms);
		++printedEvents;
	}

	if (m_vDisplayEvents.size() > printedEvents)
		ImGui::Text("... %zu more raw events", m_vDisplayEvents.size() - printedEvents);
}

bool_t CProfilerOverlay::Save_DisplayFrameToJson(const char* pPath)
{
	if (!m_pCPU || !pPath || pPath[0] == '\0')
		return false;

	std::ofstream out(pPath, std::ios::binary | std::ios::trunc);
	if (!out.is_open())
		return false;

	const f64_t toMs = m_pCPU->Get_TicksToMs();
	uint64_t baseTicks = 0;
	for (const ProfilerEvent& ev : m_vDisplayEvents)
	{
		if (baseTicks == 0 || ev.startTicks < baseTicks)
			baseTicks = ev.startTicks;
	}

	out << std::fixed << std::setprecision(6);
	out << "{\n";
	out << "  \"schema\": \"WintersProfilerCapture.v1\",\n";
	out << "  \"source\": \"ProfilerOverlay\",\n";
	out << "  \"frozen\": " << (m_bFreeze ? "true" : "false") << ",\n";
	out << "  \"sampleIntervalSec\": " << m_fSampleIntervalSec << ",\n";
	out << "  \"ticksToMs\": " << toMs << ",\n";
	out << "  \"frameMs\": " << m_StableView.Get_FrameMs() << ",\n";
	out << "  \"scopeCount\": " << m_vDisplayStats.size() << ",\n";
	out << "  \"stableRowCount\": " << m_StableView.Get_Rows().size() << ",\n";
	out << "  \"counterCount\": " << m_vDisplayCounters.size() << ",\n";
	out << "  \"rawEventCount\": " << m_vDisplayEvents.size() << ",\n";
	out << "  \"scopeStatCap\": " << PROFILER_MAX_SCOPE_STATS_PER_FRAME << ",\n";
	out << "  \"counterCap\": " << PROFILER_MAX_COUNTERS_PER_FRAME << ",\n";
	out << "  \"rawEventCap\": " << PROFILER_MAX_TREE_EVENTS_PER_FRAME << ",\n";
	out << "  \"truncatedScopes\": "
		<< (m_vDisplayStats.size() >= PROFILER_MAX_SCOPE_STATS_PER_FRAME ? "true" : "false") << ",\n";
	out << "  \"truncatedCounters\": "
		<< (m_vDisplayCounters.size() >= PROFILER_MAX_COUNTERS_PER_FRAME ? "true" : "false") << ",\n";
	out << "  \"truncatedRawEvents\": "
		<< (m_vDisplayEvents.size() >= PROFILER_MAX_TREE_EVENTS_PER_FRAME ? "true" : "false") << ",\n";

	out << "  \"stableRows\": [\n";
	const auto& rows = m_StableView.Get_Rows();
	for (size_t i = 0; i < rows.size(); ++i)
	{
		const auto& row = rows[i];
		out << "    {";
		out << "\"name\": ";
		WriteJsonString(out, row.name);
		out << ", \"totalMs\": " << row.totalMs;
		out << ", \"avgMs\": " << row.avgMs;
		out << ", \"maxMs\": " << row.maxMs;
		out << ", \"emaTotalMs\": " << row.emaTotalMs;
		out << ", \"emaAvgMs\": " << row.emaAvgMs;
		out << ", \"emaMaxMs\": " << row.emaMaxMs;
		out << ", \"calls\": " << row.callCount;
		out << ", \"age\": " << row.ageFrames;
		out << ", \"samples\": " << row.sampleCount;
		out << ", \"live\": " << (row.bLive ? "true" : "false");
		out << ", \"visible\": " << (row.bVisible ? "true" : "false");
		out << "}";
		if (i + 1 < rows.size())
			out << ",";
		out << "\n";
	}
	out << "  ],\n";

	out << "  \"frameScopes\": [\n";
	for (size_t i = 0; i < m_vDisplayStats.size(); ++i)
	{
		const ProfilerScopeStat& stat = m_vDisplayStats[i];
		const f64_t totalMs = static_cast<f64_t>(stat.totalTicks) * toMs;
		const f64_t avgMs = stat.callCount > 0
			? totalMs / static_cast<f64_t>(stat.callCount)
			: 0.0;
		const f64_t maxMs = static_cast<f64_t>(stat.maxTicks) * toMs;

		out << "    {";
		out << "\"name\": ";
		WriteJsonString(out, stat.pName);
		out << ", \"totalMs\": " << totalMs;
		out << ", \"avgMs\": " << avgMs;
		out << ", \"maxMs\": " << maxMs;
		out << ", \"calls\": " << stat.callCount;
		out << "}";
		if (i + 1 < m_vDisplayStats.size())
			out << ",";
		out << "\n";
	}
	out << "  ],\n";

	out << "  \"counters\": [\n";
	for (size_t i = 0; i < m_vDisplayCounters.size(); ++i)
	{
		const ProfilerCounter& counter = m_vDisplayCounters[i];
		out << "    {";
		out << "\"name\": ";
		WriteJsonString(out, counter.pName);
		out << ", \"value\": " << counter.value;
		out << "}";
		if (i + 1 < m_vDisplayCounters.size())
			out << ",";
		out << "\n";
	}
	out << "  ],\n";

	out << "  \"rawEvents\": [\n";
	for (size_t i = 0; i < m_vDisplayEvents.size(); ++i)
	{
		const ProfilerEvent& ev = m_vDisplayEvents[i];
		const f64_t startMs = baseTicks > 0 && ev.startTicks >= baseTicks
			? static_cast<f64_t>(ev.startTicks - baseTicks) * toMs
			: 0.0;
		const f64_t endMs = baseTicks > 0 && ev.endTicks >= baseTicks
			? static_cast<f64_t>(ev.endTicks - baseTicks) * toMs
			: 0.0;
		const f64_t durationMs = ev.endTicks >= ev.startTicks
			? static_cast<f64_t>(ev.endTicks - ev.startTicks) * toMs
			: 0.0;

		out << "    {";
		out << "\"name\": ";
		WriteJsonString(out, ev.pName);
		out << ", \"startMs\": " << startMs;
		out << ", \"endMs\": " << endMs;
		out << ", \"durationMs\": " << durationMs;
		out << ", \"depth\": " << ev.depth;
		out << ", \"threadId\": " << ev.threadId;
		out << "}";
		if (i + 1 < m_vDisplayEvents.size())
			out << ",";
		out << "\n";
	}
	out << "  ]\n";
	out << "}\n";

	return out.good();
}
