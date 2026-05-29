// ─────────────────────────────────────────────────────────────────
//  WintersEngine.cpp  |  DLL 진입점 + 공개 API 구현
//
//  Client 입장에서 보이는 것:
//    #include "WintersEngine.h"  →  WintersRun() 호출  →  끝.
//
//  실제 내부 동작:
//    WintersRun() → CEngineApp 생성 → Initialize → Run → Shutdown → 반환
// ─────────────────────────────────────────────────────────────────

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "WintersEngine.h"
#include "GameInstance.h"
#include "ProfilerAPI.h"
#include "Framework/CEngineApp.h"

// ── DLL 진입점 ──────────────────────────────────────────────────
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule); // 스레드 Attach/Detach 알림 비활성화 (성능)
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

// ── 공개 API ────────────────────────────────────────────────────
WINTERS_ENGINE int32 WintersRun(IWintersApp* app, const EngineConfig& config)
{
    if (!app)
        return -1;

    // Step 1. CGameInstance 초기화 (CEngineApp 보다 먼저)
    //         Timer_Manager 등 내부 매니저들이 여기서 생성된다.
    if (FAILED(CGameInstance::Get()->Initialize_Engine(config.iNumScenes)))
    {
        OutputDebugStringA("[WintersRun] CGameInstance::Initialize_Engine() failed\n");
        return -1;
    }

    // Step 2. 엔진 앱 초기화 (내부에서 Add_Timer(L"Timer_Default") 호출)
    CEngineApp engineApp;
    if (!engineApp.Initialize(app, config))
        return -1;

    // Step 3. 메인 루프
    return engineApp.Run();
}

WINTERS_ENGINE IRHIDevice* WintersGetRHIDevice()
{
    return &CEngineApp::Get().GetDevice();
}
