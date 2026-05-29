#pragma once
#include "AssetFormat/Anim/WSkelFormat.h"
#include <string>
#include <vector>
#include <unordered_map>

struct aiScene;

namespace Winters::Asset
{
    struct WSkelWriteOptions
    {
        bool bExtractSockets = false;
    };

    struct WSkelWriteResult
    {
        uint64_t skel_hash = 0;
        std::vector<std::string> bone_order_by_index;
        std::unordered_map<std::string, uint32_t> name_to_idx;
    };

    class WINTERS_ENGINE CWSkelWriter
    {
    public:
        static bool WriteFromAssimp(const aiScene* pScene,
            const wchar_t* pOutPath,
            WSkelWriteResult& outResult,
            const WSkelWriteOptions& opt = {});
    };
}
