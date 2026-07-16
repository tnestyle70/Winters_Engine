#pragma once

#include "Shared/GameSim/Definitions/MapDataFormats.h"

#include <cstdio>
#include <vector>

namespace Winters::Map
{
    struct StageData
    {
        StageHeader header{};
        std::vector<StructureEntry> structures;
        std::vector<JungleEntry> jungles;
        std::vector<MinionWaypointEntry> minionWaypoints;
        std::vector<BushEntry> bushes;

        void Clear()
        {
            header = {};
            structures.clear();
            jungles.clear();
            minionWaypoints.clear();
            bushes.clear();
        }
    };

    inline bool_t ReadStageBlockCount(FILE* pFile, u32_t& outCount)
    {
        return pFile && std::fread(&outCount, sizeof(u32_t), 1, pFile) == 1;
    }

    template <typename T>
    inline bool_t ReadStageEntries(FILE* pFile, u32_t count, std::vector<T>& outEntries)
    {
        outEntries.clear();
        outEntries.reserve(count);

        for (u32_t i = 0; i < count; ++i)
        {
            T entry{};
            if (std::fread(&entry, sizeof(T), 1, pFile) != 1)
                return false;
            outEntries.push_back(entry);
        }

        return true;
    }

    inline bool_t LoadStageDataFromFile(const wchar_t* pAbsPath, StageData& outData)
    {
        outData.Clear();
        if (!pAbsPath)
            return false;

        FILE* pFile = nullptr;
        if (_wfopen_s(&pFile, pAbsPath, L"rb") != 0 || !pFile)
            return false;

        StageHeader header{};
        if (std::fread(&header, sizeof(StageHeader), 1, pFile) != 1)
        {
            std::fclose(pFile);
            return false;
        }

        if (header.magic != STAGE_MAGIC ||
            header.version < STAGE_VERSION_MIN_COMPAT ||
            header.version > STAGE_VERSION)
        {
            std::fclose(pFile);
            return false;
        }

        u32_t structureCount = 0;
        if (!ReadStageBlockCount(pFile, structureCount) ||
            !ReadStageEntries(pFile, structureCount, outData.structures))
        {
            std::fclose(pFile);
            outData.Clear();
            return false;
        }

        u32_t jungleCount = 0;
        if (!ReadStageBlockCount(pFile, jungleCount) ||
            !ReadStageEntries(pFile, jungleCount, outData.jungles))
        {
            std::fclose(pFile);
            outData.Clear();
            return false;
        }

        if (header.version >= 4)
        {
            u32_t waypointCount = 0;
            if (!ReadStageBlockCount(pFile, waypointCount) ||
                !ReadStageEntries(pFile, waypointCount, outData.minionWaypoints))
            {
                std::fclose(pFile);
                outData.Clear();
                return false;
            }
        }

        if (header.version >= 5)
        {
            u32_t bushCount = 0;
            if (!ReadStageBlockCount(pFile, bushCount) ||
                !ReadStageEntries(pFile, bushCount, outData.bushes))
            {
                std::fclose(pFile);
                outData.Clear();
                return false;
            }
        }

        outData.header = header;
        std::fclose(pFile);
        return true;
    }

    inline bool_t WriteStageBlockCount(FILE* pFile, u32_t count)
    {
        return pFile && std::fwrite(&count, sizeof(u32_t), 1, pFile) == 1;
    }

    template <typename T>
    inline bool_t WriteStageEntries(FILE* pFile, const std::vector<T>& entries)
    {
        for (const T& entry : entries)
        {
            if (std::fwrite(&entry, sizeof(T), 1, pFile) != 1)
                return false;
        }

        return true;
    }

    // 로더와 대칭인 라이터. header를 보존해 로드→세이브 왕복 시 바이트 동일을 보장한다.
    // (블록 존재 여부는 로더와 동일하게 header.version 기준으로 결정)
    inline bool_t SaveStageDataToFile(const wchar_t* pAbsPath, const StageData& data)
    {
        if (!pAbsPath)
            return false;

        StageHeader header = data.header;
        if (header.magic != STAGE_MAGIC ||
            header.version < STAGE_VERSION_MIN_COMPAT ||
            header.version > STAGE_VERSION)
        {
            header = {};
            header.magic = STAGE_MAGIC;
            header.version = STAGE_VERSION;
        }

        FILE* pFile = nullptr;
        if (_wfopen_s(&pFile, pAbsPath, L"wb") != 0 || !pFile)
            return false;

        bool_t bOk = std::fwrite(&header, sizeof(StageHeader), 1, pFile) == 1;

        bOk = bOk && WriteStageBlockCount(pFile, static_cast<u32_t>(data.structures.size()));
        bOk = bOk && WriteStageEntries(pFile, data.structures);

        bOk = bOk && WriteStageBlockCount(pFile, static_cast<u32_t>(data.jungles.size()));
        bOk = bOk && WriteStageEntries(pFile, data.jungles);

        if (header.version >= 4)
        {
            bOk = bOk && WriteStageBlockCount(pFile, static_cast<u32_t>(data.minionWaypoints.size()));
            bOk = bOk && WriteStageEntries(pFile, data.minionWaypoints);
        }

        if (header.version >= 5)
        {
            bOk = bOk && WriteStageBlockCount(pFile, static_cast<u32_t>(data.bushes.size()));
            bOk = bOk && WriteStageEntries(pFile, data.bushes);
        }

        std::fclose(pFile);
        return bOk;
    }
}
