#pragma once
#include "WintersAPI.h"

struct aiScene;

namespace Winters::Asset
{
    class CWMaterialWriter
    {
    public:
        static bool WriteFromAssimp(const aiScene* pScene,
                                    const wchar_t* pSourceModelPath,
                                    const wchar_t* pOutPath);
    };
}
