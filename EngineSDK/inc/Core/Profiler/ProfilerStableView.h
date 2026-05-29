#pragma once

#include "ProfilerTypes.h"
#include "WintersTypes.h"

#include <string>
#include <vector>

class CProfilerStableView final
{
public:
    struct Row
    {
        std::string name{};
        f64_t totalMs = 0.0;
        f64_t avgMs = 0.0;
        f64_t maxMs = 0.0;
        f64_t emaTotalMs = 0.0;
        f64_t emaAvgMs = 0.0;
        f64_t emaMaxMs = 0.0;
        u32_t callCount = 0;
        u32_t firstSeenFrame = 0;
        u32_t lastSeenFrame = 0;
        u32_t ageFrames = 0;
        u32_t sampleCount = 0;
        bool_t bLive = false;
        bool_t bVisible = false;
    };

    void Reset();
    void Update(const std::vector<ProfilerScopeStat>& stats, f64_t ticksToMs);

    const std::vector<Row>& Get_Rows() const { return m_vRows; }
    f64_t Get_FrameMs() const { return m_fFrameMs; }
    u32_t Get_VisibleRowCount() const;

private:
    Row* Find_Row(const char* pName);
    void Sort_IfNeeded(bool_t bForceSort);
    void Prune_ExpiredRows();

    std::vector<Row> m_vRows{};
    u32_t m_iFrameIndex = 0;
    u32_t m_iFramesUntilSort = 0;
    f64_t m_fFrameMs = 0.0;
};
