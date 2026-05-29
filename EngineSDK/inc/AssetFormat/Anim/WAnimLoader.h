#pragma once
#include "AssetFormat/Anim/WAnimFormat.h"
#include <memory>
#include <string>

namespace Engine
{
    class CAnimation;
    class CSkeleton;
}

namespace Winters::Asset
{
    class WINTERS_ENGINE CWAnimLoader
    {
    public:
        static std::unique_ptr<Engine::CAnimation> LoadAsAnimation(
            const wchar_t* path,
            uint64_t expectedSkelHash,
            const Engine::CSkeleton* pSkeleton,
            const std::wstring& nameForDebug);
    };
}
