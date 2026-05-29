#include <Windows.h>

//CRT Leak Check, only for debug modes!!
#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "WintersEngine.h"
#include "GameApp.h"
#include "Defines.h"
#include <cwchar>

extern "C"
{
    __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

namespace
{
    eEngineRHIBackend ParseRequestedRHIBackend()
    {
        const wchar_t* pCommandLine = ::GetCommandLineW();
        if (!pCommandLine)
            return eEngineRHIBackend::DX11;

        if (wcsstr(pCommandLine, L"--rhi=dx11") || wcsstr(pCommandLine, L"/rhi:dx11"))
            return eEngineRHIBackend::DX11;
        if (wcsstr(pCommandLine, L"--rhi=null") || wcsstr(pCommandLine, L"/rhi:null"))
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
    config.vsync = true;
    config.fullscreen = false;
    config.iNumScenes = static_cast<uint32_t>(eSceneID::End);

    return WintersRun(&gameApp, config);
}
