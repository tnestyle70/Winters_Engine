#include <Windows.h>

//CRT Leak Check, only for debug modes!!
#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "WintersEngine.h"
#include "GameApp.h"
#include "Defines.h"
#include <cstdlib>
#include <cwchar>

extern "C"
{
    __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

namespace
{
    constexpr uint32_t kDefaultTargetFPS = 60u;

    const wchar_t* GetCommandLineText()
    {
        const wchar_t* pCommandLine = ::GetCommandLineW();
        return pCommandLine ? pCommandLine : L"";
    }

    bool_t HasCommandLineFlag(const wchar_t* pLongFlag, const wchar_t* pShortFlag = nullptr)
    {
        const wchar_t* pCommandLine = GetCommandLineText();
        return (pLongFlag && wcsstr(pCommandLine, pLongFlag)) ||
            (pShortFlag && wcsstr(pCommandLine, pShortFlag));
    }

    const wchar_t* FindCommandLineValue(const wchar_t* pPrefix)
    {
        if (!pPrefix)
            return nullptr;

        const wchar_t* pFound = wcsstr(GetCommandLineText(), pPrefix);
        return pFound ? pFound + wcslen(pPrefix) : nullptr;
    }

    uint32_t ParseRequestedTargetFPS()
    {
        if (HasCommandLineFlag(L"--fps=0", L"/fps:0") ||
            HasCommandLineFlag(L"--uncapped", L"/uncapped"))
        {
            return 0u;
        }

        const wchar_t* pValue = FindCommandLineValue(L"--fps=");
        if (!pValue)
            pValue = FindCommandLineValue(L"/fps:");
        if (!pValue)
            return kDefaultTargetFPS;

        wchar_t* pEnd = nullptr;
        const unsigned long parsed = std::wcstoul(pValue, &pEnd, 10);
        if (pEnd == pValue)
            return kDefaultTargetFPS;

        return static_cast<uint32_t>(parsed);
    }

    bool_t ParseRequestedVSync()
    {
        if (HasCommandLineFlag(L"--vsync", L"/vsync"))
            return true;
        if (HasCommandLineFlag(L"--no-vsync", L"/no-vsync"))
            return false;
        return false;
    }

    eEngineRHIBackend ParseRequestedRHIBackend()
    {
        if (HasCommandLineFlag(L"--rhi=dx11", L"/rhi:dx11"))
            return eEngineRHIBackend::DX11;
        if (HasCommandLineFlag(L"--rhi=dx12", L"/rhi:dx12"))
            return eEngineRHIBackend::DX12;
        if (HasCommandLineFlag(L"--rhi=null", L"/rhi:null"))
            return eEngineRHIBackend::Null;

        return eEngineRHIBackend::DX11;
    }
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
#ifdef _DEBUG
    //종료 직전 미해제 힙 블록을 Output 창에 덤프
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    CGameApp gameApp;

    EngineConfig config;
    config.windowTitle = L"LOL";
    config.rhiBackend = ParseRequestedRHIBackend();
    config.allowRHIFallback = true;
    config.windowWidth = 1280;
    config.windowHeight = 720;
    config.vsync = ParseRequestedVSync();
    config.targetFPS = ParseRequestedTargetFPS();
    config.fullscreen = false;
    config.iNumScenes = static_cast<uint32_t>(eSceneID::End);

    return WintersRun(&gameApp, config);
}
