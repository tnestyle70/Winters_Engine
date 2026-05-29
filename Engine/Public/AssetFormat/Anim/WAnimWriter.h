#pragma once
#include "AssetFormat/Anim/WAnimFormat.h"

struct aiAnimation;
struct aiScene;

namespace Winters::Asset
{
    class WINTERS_ENGINE CWAnimWriter
    {
    public:
        static bool WriteFromAssimp(const aiAnimation* pAnim,
            const aiScene* pScene,
            uint64_t skelHash,
            const wchar_t* pOutPath);
    };
}
