#include "Core/Profiler/ProfilerStableView.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace
{
    constexpr f64_t kEmaAlpha = 0.25;
    constexpr f64_t kStaleDecay = 0.94;
    constexpr u32_t kHoldFrames = 120;
    constexpr u32_t kPruneFrames = 240;
    constexpr u32_t kSortIntervalFrames = 12;

    const char* SafeName(const char* pName)
    {
        return pName ? pName : "(null)";
    }

    bool_t SameName(const std::string& lhs, const char* rhs)
    {
        return std::strcmp(lhs.c_str(), SafeName(rhs)) == 0;
    }

    void SmoothValue(f64_t& dst, f64_t src, bool_t bFirstSample)
    {
        if (bFirstSample)
            dst = src;
        else
            dst += (src - dst) * kEmaAlpha;
    }
}

void CProfilerStableView::Reset()
{
    m_vRows.clear();
    m_iFrameIndex = 0;
    m_iFramesUntilSort = 0;
    m_fFrameMs = 0.0;
}

void CProfilerStableView::Update(const std::vector<ProfilerScopeStat>& stats, f64_t ticksToMs)
{
    ++m_iFrameIndex;

    for (Row& row : m_vRows)
    {
        row.bLive = false;
        row.ageFrames = m_iFrameIndex - row.lastSeenFrame;
        row.bVisible = row.ageFrames <= kHoldFrames;
    }

    bool_t bAddedRow = false;
    f64_t currentFrameMs = 0.0;
    bool_t bSawFrame = false;

    for (const ProfilerScopeStat& stat : stats)
    {
        Row* pRow = Find_Row(stat.pName);
        if (!pRow)
        {
            Row row{};
            row.name = SafeName(stat.pName);
            row.firstSeenFrame = m_iFrameIndex;
            row.lastSeenFrame = m_iFrameIndex;
            m_vRows.push_back(std::move(row));
            pRow = &m_vRows.back();
            bAddedRow = true;
        }

        const f64_t totalMs = static_cast<f64_t>(stat.totalTicks) * ticksToMs;
        const f64_t avgMs = (stat.callCount > 0)
            ? totalMs / static_cast<f64_t>(stat.callCount)
            : 0.0;
        const f64_t maxMs = static_cast<f64_t>(stat.maxTicks) * ticksToMs;
        const bool_t bFirstSample = (pRow->sampleCount == 0);

        pRow->totalMs = totalMs;
        pRow->avgMs = avgMs;
        pRow->maxMs = maxMs;
        pRow->callCount = stat.callCount;
        pRow->lastSeenFrame = m_iFrameIndex;
        pRow->ageFrames = 0;
        pRow->bLive = true;
        pRow->bVisible = true;
        ++pRow->sampleCount;

        SmoothValue(pRow->emaTotalMs, totalMs, bFirstSample);
        SmoothValue(pRow->emaAvgMs, avgMs, bFirstSample);
        SmoothValue(pRow->emaMaxMs, maxMs, bFirstSample);

        if (SameName(pRow->name, "Frame"))
        {
            currentFrameMs = totalMs;
            bSawFrame = true;
        }
    }

    for (Row& row : m_vRows)
    {
        if (row.bLive)
            continue;

        row.ageFrames = m_iFrameIndex - row.lastSeenFrame;
        row.bVisible = row.ageFrames <= kHoldFrames;
        row.emaTotalMs *= kStaleDecay;
        row.emaAvgMs *= kStaleDecay;
        row.emaMaxMs *= kStaleDecay;
    }

    if (bSawFrame)
        m_fFrameMs = currentFrameMs;
    else if (Row* pFrame = Find_Row("Frame"))
        m_fFrameMs = pFrame->emaTotalMs;

    Prune_ExpiredRows();
    Sort_IfNeeded(bAddedRow);
}

u32_t CProfilerStableView::Get_VisibleRowCount() const
{
    u32_t count = 0;
    for (const Row& row : m_vRows)
    {
        if (row.bVisible)
            ++count;
    }
    return count;
}

CProfilerStableView::Row* CProfilerStableView::Find_Row(const char* pName)
{
    for (Row& row : m_vRows)
    {
        if (SameName(row.name, pName))
            return &row;
    }
    return nullptr;
}

void CProfilerStableView::Sort_IfNeeded(bool_t bForceSort)
{
    if (!bForceSort && m_iFramesUntilSort > 0)
    {
        --m_iFramesUntilSort;
        return;
    }

    std::stable_sort(m_vRows.begin(), m_vRows.end(),
        [](const Row& lhs, const Row& rhs)
        {
            if (lhs.bVisible != rhs.bVisible)
                return lhs.bVisible && !rhs.bVisible;
            if (lhs.emaTotalMs != rhs.emaTotalMs)
                return lhs.emaTotalMs > rhs.emaTotalMs;
            if (lhs.firstSeenFrame != rhs.firstSeenFrame)
                return lhs.firstSeenFrame < rhs.firstSeenFrame;
            return lhs.name < rhs.name;
        });

    m_iFramesUntilSort = kSortIntervalFrames;
}

void CProfilerStableView::Prune_ExpiredRows()
{
    m_vRows.erase(
        std::remove_if(m_vRows.begin(), m_vRows.end(),
            [](const Row& row)
            {
                if (row.name == "Frame")
                    return false;
                return row.ageFrames > kPruneFrames && row.emaTotalMs < 0.001;
            }),
        m_vRows.end());
}
