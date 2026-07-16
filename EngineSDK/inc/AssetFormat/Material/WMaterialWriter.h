#pragma once
#include "WintersAPI.h"

#include <string>
#include <unordered_map>

struct aiScene;

namespace Winters::Asset
{
    struct WMaterialWriteOptions
    {
        const std::unordered_map<std::string, std::wstring>* pDiffusePathOverrides = nullptr;
    };

    class CWMaterialWriter
    {
    public:
        static bool WriteFromAssimp(const aiScene* pScene,
                                    const wchar_t* pSourceModelPath,
                                    const wchar_t* pOutPath,
                                    const WMaterialWriteOptions& opt = {});
    };
}
