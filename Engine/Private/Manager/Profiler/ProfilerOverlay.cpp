#include "Manager/Profiler/ProfilerOverlay.h"
#include "Core/Profiler/CPUProfiler.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
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

	uint64_t FindBaseTicks(const std::vector<ProfilerEvent>& events)
	{
		uint64_t baseTicks = 0;
		for (const ProfilerEvent& event : events)
		{
			if (baseTicks == 0 || event.startTicks < baseTicks)
				baseTicks = event.startTicks;
		}
		return baseTicks;
	}
}

std::unique_ptr<CProfilerOverlay> CProfilerOverlay::Create(CCPUProfiler* pCPU)
{
	auto p = std::unique_ptr<CProfilerOverlay>(new CProfilerOverlay());
	p->m_pCPU = pCPU;
	if (p->m_pCPU)
	{
		const ProfilerHistoryConfig config = p->m_pCPU->GetHistoryConfig();
		p->m_iHistoryEveryNFrames = static_cast<int>(config.sampleEveryNFrames);
		p->m_iHistoryEveryNServerTicks = static_cast<int>(config.sampleEveryNServerTicks);
		p->m_iHistoryMaxFrames = static_cast<int>(config.maxFrames);
		p->m_bHistoryEnabled = p->m_pCPU->IsHistoryEnabled();
	}
	return p;
}

CProfilerOverlay::~CProfilerOverlay()
{
	if (!m_SaveFuture.valid())
		return;

	try
	{
		m_SaveFuture.get();
	}
	catch (...)
	{
	}
}

void CProfilerOverlay::Draw()
{
	PollSaveResult();
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

bool_t CProfilerOverlay::CaptureToJson(
	const char* pPath,
	bool_t bForce,
	const char* pAliasPath)
{
	if (!m_pCPU || !pPath || pPath[0] == '\0')
		return false;
	PollSaveResult();
	if (IsSaveInProgress())
		return false;

	if (bForce || m_vDisplayStats.empty())
		Capture_DisplayFrame(true);

	std::vector<ProfilerFrameRecord> frames;
	if (m_pCPU->IsHistoryEnabled())
		frames = m_pCPU->TakeHistory();

	const bool_t bTimeline = !frames.empty();
	DisplayFrameSaveData displayFrame{};
	if (!bTimeline)
	{
		displayFrame.stats = m_vDisplayStats;
		displayFrame.counters = m_vDisplayCounters;
		displayFrame.events = m_vDisplayEvents;
		displayFrame.stableRows = m_StableView.Get_Rows();
		displayFrame.ticksToMs = m_pCPU->Get_TicksToMs();
		displayFrame.frameMs = m_StableView.Get_FrameMs();
		displayFrame.sampleIntervalSec = m_fSampleIntervalSec;
		displayFrame.bFrozen = m_bFreeze;
	}

	const f64_t ticksToMs = m_pCPU->Get_TicksToMs();
	const ProfilerHistoryConfig config = m_pCPU->GetHistoryConfig();
	const std::string primaryPath = pPath;
	const std::string aliasPath = pAliasPath ? pAliasPath : "";
	m_strSaveStatus = bTimeline ? "Saving timeline..." : "Saving frame...";

	try
	{
		m_SaveFuture = std::async(
			std::launch::async,
			[primaryPath,
			 aliasPath,
			 bTimeline,
			 frames = std::move(frames),
			 displayFrame = std::move(displayFrame),
			 ticksToMs,
			 config]()
			{
				const std::string tempPath = primaryPath + ".tmp";
				DeleteFileA(tempPath.c_str());
				const bool_t bWritten = bTimeline
					? CProfilerOverlay::Save_TimelineToJson(
						tempPath.c_str(), frames, ticksToMs, config)
					: CProfilerOverlay::Save_DisplayFrameToJson(
						tempPath.c_str(), displayFrame);
				if (!bWritten)
				{
					DeleteFileA(tempPath.c_str());
					return false;
				}

				if (!MoveFileExA(
					tempPath.c_str(),
					primaryPath.c_str(),
					MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
				{
					DeleteFileA(tempPath.c_str());
					return false;
				}

				if (!aliasPath.empty() &&
					_stricmp(aliasPath.c_str(), primaryPath.c_str()) != 0)
				{
					const std::string aliasTempPath = aliasPath + ".tmp";
					DeleteFileA(aliasTempPath.c_str());
					if (!CopyFileA(
						primaryPath.c_str(), aliasTempPath.c_str(), FALSE) ||
						!MoveFileExA(
							aliasTempPath.c_str(),
							aliasPath.c_str(),
							MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
					{
						DeleteFileA(aliasTempPath.c_str());
						return false;
					}
				}
				return true;
			});
	}
	catch (...)
	{
		m_strSaveStatus = "Save worker launch failed";
		return false;
	}

	return true;
}

bool_t CProfilerOverlay::IsSaveInProgress() const
{
	return m_SaveFuture.valid() &&
		m_SaveFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready;
}

void CProfilerOverlay::PollSaveResult()
{
	if (!m_SaveFuture.valid() ||
		m_SaveFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
	{
		return;
	}

	bool_t bSaved = false;
	try
	{
		bSaved = m_SaveFuture.get();
	}
	catch (...)
	{
		bSaved = false;
	}
	m_strSaveStatus = bSaved ? "Timeline saved" : "Timeline save failed";
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
	if (IsSaveInProgress())
	{
		ImGui::TextDisabled("Saving...");
	}
	else if (ImGui::SmallButton("Save Timeline"))
	{
		if (!CaptureToJson("profiler.json", true))
			m_strSaveStatus = "Save request failed";
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

	if (ImGui::Checkbox("Timeline", &m_bHistoryEnabled))
		ApplyHistoryConfig();

	ImGui::SameLine();
	ImGui::SetNextItemWidth(72.f);
	if (ImGui::InputInt("Frame stride", &m_iHistoryEveryNFrames))
		ApplyHistoryConfig();

	ImGui::SameLine();
	ImGui::SetNextItemWidth(72.f);
	if (ImGui::InputInt("Server tick stride", &m_iHistoryEveryNServerTicks))
		ApplyHistoryConfig();

	ImGui::SetNextItemWidth(96.f);
	if (ImGui::InputInt("History frames", &m_iHistoryMaxFrames))
		ApplyHistoryConfig();

	ImGui::SameLine();
	if (ImGui::Button("Clear Timeline"))
		m_pCPU->ClearHistory();

}

void CProfilerOverlay::ApplyHistoryConfig()
{
	if (!m_pCPU)
		return;

	m_iHistoryEveryNFrames = std::clamp(m_iHistoryEveryNFrames, 1, 10000);
	m_iHistoryEveryNServerTicks = std::clamp(m_iHistoryEveryNServerTicks, 0, 10000);
	m_iHistoryMaxFrames = std::clamp(
		m_iHistoryMaxFrames,
		1,
		static_cast<int>(PROFILER_MAX_HISTORY_FRAMES));

	ProfilerHistoryConfig config{};
	config.sampleEveryNFrames = static_cast<uint32_t>(m_iHistoryEveryNFrames);
	config.sampleEveryNServerTicks = static_cast<uint32_t>(m_iHistoryEveryNServerTicks);
	config.maxFrames = static_cast<uint32_t>(m_iHistoryMaxFrames);
	m_pCPU->SetHistoryConfig(config);
	m_pCPU->SetHistoryEnabled(m_bHistoryEnabled);
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

bool_t CProfilerOverlay::Save_DisplayFrameToJson(
	const char* pPath,
	const DisplayFrameSaveData& frame)
{
	if (!pPath || pPath[0] == '\0')
		return false;

	std::ofstream out(pPath, std::ios::binary | std::ios::trunc);
	if (!out.is_open())
		return false;

	const f64_t toMs = frame.ticksToMs;
	uint64_t baseTicks = 0;
	for (const ProfilerEvent& ev : frame.events)
	{
		if (baseTicks == 0 || ev.startTicks < baseTicks)
			baseTicks = ev.startTicks;
	}

	out << std::fixed << std::setprecision(6);
	out << "{\n";
	out << "  \"schema\": \"WintersProfilerCapture.v1\",\n";
	out << "  \"source\": \"ProfilerOverlay\",\n";
	out << "  \"frozen\": " << (frame.bFrozen ? "true" : "false") << ",\n";
	out << "  \"sampleIntervalSec\": " << frame.sampleIntervalSec << ",\n";
	out << "  \"ticksToMs\": " << toMs << ",\n";
	out << "  \"frameMs\": " << frame.frameMs << ",\n";
	out << "  \"scopeCount\": " << frame.stats.size() << ",\n";
	out << "  \"stableRowCount\": " << frame.stableRows.size() << ",\n";
	out << "  \"counterCount\": " << frame.counters.size() << ",\n";
	out << "  \"rawEventCount\": " << frame.events.size() << ",\n";
	out << "  \"scopeStatCap\": " << PROFILER_MAX_SCOPE_STATS_PER_FRAME << ",\n";
	out << "  \"counterCap\": " << PROFILER_MAX_COUNTERS_PER_FRAME << ",\n";
	out << "  \"rawEventCap\": " << PROFILER_MAX_TREE_EVENTS_PER_FRAME << ",\n";
	out << "  \"truncatedScopes\": "
		<< (frame.stats.size() >= PROFILER_MAX_SCOPE_STATS_PER_FRAME ? "true" : "false") << ",\n";
	out << "  \"truncatedCounters\": "
		<< (frame.counters.size() >= PROFILER_MAX_COUNTERS_PER_FRAME ? "true" : "false") << ",\n";
	out << "  \"truncatedRawEvents\": "
		<< (frame.events.size() >= PROFILER_MAX_TREE_EVENTS_PER_FRAME ? "true" : "false") << ",\n";

	out << "  \"stableRows\": [\n";
	const auto& rows = frame.stableRows;
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
	for (size_t i = 0; i < frame.stats.size(); ++i)
	{
		const ProfilerScopeStat& stat = frame.stats[i];
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
		if (i + 1 < frame.stats.size())
			out << ",";
		out << "\n";
	}
	out << "  ],\n";

	out << "  \"counters\": [\n";
	for (size_t i = 0; i < frame.counters.size(); ++i)
	{
		const ProfilerCounter& counter = frame.counters[i];
		out << "    {";
		out << "\"name\": ";
		WriteJsonString(out, counter.pName);
		out << ", \"value\": " << counter.value;
		out << "}";
		if (i + 1 < frame.counters.size())
			out << ",";
		out << "\n";
	}
	out << "  ],\n";

	out << "  \"rawEvents\": [\n";
	for (size_t i = 0; i < frame.events.size(); ++i)
	{
		const ProfilerEvent& ev = frame.events[i];
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
		if (i + 1 < frame.events.size())
			out << ",";
		out << "\n";
	}
	out << "  ]\n";
	out << "}\n";

	out.flush();
	const bool_t bFlushed = out.good();
	out.close();
	return bFlushed && !out.fail();
}

bool_t CProfilerOverlay::Save_TimelineToJson(
	const char* pPath,
	const std::vector<ProfilerFrameRecord>& frames,
	f64_t toMs,
	const ProfilerHistoryConfig& config)
{
	if (!pPath || pPath[0] == '\0' || frames.empty())
		return false;

	std::ofstream out(pPath, std::ios::binary | std::ios::trunc);
	if (!out.is_open())
		return false;

	out << std::fixed << std::setprecision(6);
	out << "{\n";
	out << "  \"schema\": \"WintersProfilerTimeline.v3\",\n";
	out << "  \"source\": \"CPUProfilerHistory\",\n";
	out << "  \"frameCount\": " << frames.size() << ",\n";
	out << "  \"sampleEveryNFrames\": " << config.sampleEveryNFrames << ",\n";
	out << "  \"sampleEveryNServerTicks\": " << config.sampleEveryNServerTicks << ",\n";
	out << "  \"historyCapacityFrames\": " << config.maxFrames << ",\n";
	out << "  \"rawEventRetentionFrames\": "
		<< PROFILER_MAX_RAW_HISTORY_FRAMES << ",\n";
	out << "  \"ticksToMs\": " << toMs << ",\n";
	out << "  \"frames\": [\n";

	for (size_t frameIndex = 0; frameIndex < frames.size(); ++frameIndex)
	{
		const ProfilerFrameRecord& frame = frames[frameIndex];
		const uint64_t baseTicks = FindBaseTicks(frame.events);
		out << "    {\n";
		out << "      \"renderFrame\": " << frame.renderFrame << ",\n";
		out << "      \"frameStrideSample\": "
			<< (frame.bFrameStrideSample ? "true" : "false") << ",\n";
		out << "      \"serverTickSample\": "
			<< (frame.bServerTickSample ? "true" : "false") << ",\n";
		out << "      \"serverTick\": ";
		if (frame.bHasServerTick)
			out << frame.serverTick;
		else
			out << "null";
		out << ",\n";
		out << "      \"frameMs\": " << frame.frameMs << ",\n";
		out << "      \"gpuFrameUs\": ";
		if (frame.bHasGpuFrameTime)
			out << frame.gpuFrameUs;
		else
			out << "null";
		out << ",\n";
		out << "      \"gpuSourceRhiFrame\": ";
		if (frame.bHasGpuSourceRhiFrame)
			out << frame.gpuSourceRhiFrame;
		else
			out << "null";
		out << ",\n";
		out << "      \"droppedScopeStats\": " << frame.droppedScopeStats << ",\n";
		out << "      \"droppedCounters\": " << frame.droppedCounters << ",\n";
		out << "      \"droppedRawEvents\": " << frame.droppedRawEvents << ",\n";
		out << "      \"rawEventsRetained\": "
			<< (frame.bRawEventsRetained ? "true" : "false") << ",\n";
		out << "      \"omittedRawEvents\": " << frame.omittedRawEvents << ",\n";
		out << "      \"rawEventCount\": " << frame.events.size() << ",\n";

		out << "      \"scopes\": [\n";
		for (size_t i = 0; i < frame.scopeStats.size(); ++i)
		{
			const ProfilerScopeStat& stat = frame.scopeStats[i];
			const f64_t totalMs = static_cast<f64_t>(stat.totalTicks) * toMs;
			const f64_t averageMs = stat.callCount > 0
				? totalMs / static_cast<f64_t>(stat.callCount)
				: 0.0;
			out << "        {\"name\": ";
			WriteJsonString(out, stat.pName);
			out << ", \"totalMs\": " << totalMs;
			out << ", \"avgMs\": " << averageMs;
			out << ", \"maxMs\": " << static_cast<f64_t>(stat.maxTicks) * toMs;
			out << ", \"calls\": " << stat.callCount << "}";
			if (i + 1 < frame.scopeStats.size())
				out << ',';
			out << '\n';
		}
		out << "      ],\n";

		out << "      \"counters\": [\n";
		for (size_t i = 0; i < frame.counters.size(); ++i)
		{
			const ProfilerCounter& counter = frame.counters[i];
			out << "        {\"name\": ";
			WriteJsonString(out, counter.pName);
			out << ", \"value\": " << counter.value << "}";
			if (i + 1 < frame.counters.size())
				out << ',';
			out << '\n';
		}
		out << "      ],\n";

		out << "      \"rawEvents\": [\n";
		for (size_t i = 0; i < frame.events.size(); ++i)
		{
			const ProfilerEvent& event = frame.events[i];
			const f64_t startMs = baseTicks > 0 && event.startTicks >= baseTicks
				? static_cast<f64_t>(event.startTicks - baseTicks) * toMs
				: 0.0;
			const f64_t endMs = baseTicks > 0 && event.endTicks >= baseTicks
				? static_cast<f64_t>(event.endTicks - baseTicks) * toMs
				: 0.0;
			out << "        {\"name\": ";
			WriteJsonString(out, event.pName);
			out << ", \"startMs\": " << startMs;
			out << ", \"endMs\": " << endMs;
			out << ", \"durationMs\": "
				<< static_cast<f64_t>(event.endTicks - event.startTicks) * toMs;
			out << ", \"depth\": " << event.depth;
			out << ", \"threadId\": " << event.threadId << "}";
			if (i + 1 < frame.events.size())
				out << ',';
			out << '\n';
		}
		out << "      ]\n";
		out << "    }";
		if (frameIndex + 1 < frames.size())
			out << ',';
		out << '\n';
	}

	out << "  ]\n";
	out << "}\n";
	out.flush();
	const bool_t bFlushed = out.good();
	out.close();
	return bFlushed && !out.fail();
}
