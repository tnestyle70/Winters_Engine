#include "EldenRingApp.h"

#include "EldenLimgraveShowcaseScene.h"
#include "EldenRingProbeScene.h"
#include "GameInstance.h"

#include <Windows.h>
#include <cwchar>

namespace
{
    constexpr u32_t kEldenRingProbeSceneID = 0;

    bool IsLimgraveShowcaseRequested()
    {
        const wchar_t* const pCommandLine = ::GetCommandLineW();
        return pCommandLine
            && (wcsstr(pCommandLine, L"--scene=limgrave") || wcsstr(pCommandLine, L"/scene:limgrave"));
    }
}

bool CEldenRingApp::OnInit()
{
#ifdef _DEBUG
    ::OutputDebugStringA("[EldenRingClient] OnInit\n");
#endif

    if (IsLimgraveShowcaseRequested())
    {
        CGameInstance::Get()->Change_Scene(
            kEldenRingProbeSceneID,
            CEldenLimgraveShowcaseScene::Create());
    }
    else
    {
        CGameInstance::Get()->Change_Scene(
            kEldenRingProbeSceneID,
            CEldenRingAssetProbeScene::Create());
    }

    return true;
}

void CEldenRingApp::OnUpdate(f32_t)
{
}

void CEldenRingApp::OnRender()
{
}

void CEldenRingApp::OnShutdown()
{
    CGameInstance::Get()->Change_Scene(kEldenRingProbeSceneID, nullptr);

#ifdef _DEBUG
    ::OutputDebugStringA("[EldenRingClient] OnShutdown\n");
#endif
}
