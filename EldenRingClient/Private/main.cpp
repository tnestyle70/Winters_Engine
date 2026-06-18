#include <Windows.h>

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "EldenRingApp.h"
#include "WintersEngine.h"

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
        const wchar_t* const pCommandLine = ::GetCommandLineW();
        if (!pCommandLine)
            return eEngineRHIBackend::DX12;

        if (wcsstr(pCommandLine, L"--rhi=auto") || wcsstr(pCommandLine, L"/rhi:auto"))
            return eEngineRHIBackend::Auto;
        if (wcsstr(pCommandLine, L"--rhi=dx12") || wcsstr(pCommandLine, L"/rhi:dx12"))
            return eEngineRHIBackend::DX12;
        if (wcsstr(pCommandLine, L"--rhi=dx11") || wcsstr(pCommandLine, L"/rhi:dx11"))
            return eEngineRHIBackend::DX11;
        if (wcsstr(pCommandLine, L"--rhi=null") || wcsstr(pCommandLine, L"/rhi:null"))
            return eEngineRHIBackend::Null;

        return eEngineRHIBackend::DX12;
    }
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
#ifdef _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    CEldenRingApp app;

    EngineConfig config{};
    config.windowTitle = L"Elden Ring - Winters";
    config.windowWidth = 1280;
    config.windowHeight = 720;
    config.fullscreen = false;
    config.vsync = true;
    config.iNumScenes = 1;
    config.rhiBackend = ParseRequestedRHIBackend();
    config.allowRHIFallback = true;

    return WintersRun(&app, config);
}
