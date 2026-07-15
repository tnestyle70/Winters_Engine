#pragma once

#include "Shared/GameSim/Systems/ChampionAI/ChampionAIResearchTypes.h"

#include <cstddef>
#include <cstdio>

namespace Winters::AIResearchTools
{
    // Tools-owned raw capture boundary. Shared/GameSim remains free of file IO.
    // Records must be supplied in deterministic chronological/entity order.
    inline bool WriteAiDecisionTraceCaptureV1(
        const char* outputPath,
        const AiDecisionTraceV1* records,
        std::size_t recordCount) noexcept
    {
        if (outputPath == nullptr || records == nullptr || recordCount == 0u)
            return false;

        FILE* stream = nullptr;
        if (fopen_s(&stream, outputPath, "wb") != 0 || stream == nullptr)
            return false;

        const std::size_t written = std::fwrite(
            records,
            sizeof(AiDecisionTraceV1),
            recordCount,
            stream);
        const int closeResult = std::fclose(stream);
        return written == recordCount && closeResult == 0;
    }
}
