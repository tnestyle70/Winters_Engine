Session - CEngineApp frame loop를 backend-neutral로 열어 LoL과 Elden이 같은 SceneManager render entry를 탄다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Engine/Private/Framework/CEngineApp.cpp

`void CEngineApp::Update(f32_t deltaTime)` 전체를 아래로 교체:

기존 코드:

```cpp
void CEngineApp::Update(f32_t deltaTime)
{
    if (!m_bDX11RuntimeEnabled)
    {
        if (m_pGameApp)
            m_pGameApp->OnUpdate(deltaTime);
        return;
    }

    auto* pSceneManager = CGameInstance::Get()->Get_SceneManager();
    pSceneManager->Update(deltaTime);
    pSceneManager->LateUpdate(deltaTime);
}
```

아래로 교체:

```cpp
void CEngineApp::Update(f32_t deltaTime)
{
    auto* pSceneManager = CGameInstance::Get()->Get_SceneManager();
    if (pSceneManager)
    {
        pSceneManager->Update(deltaTime);
        pSceneManager->LateUpdate(deltaTime);
    }

    if (m_pGameApp)
        m_pGameApp->OnUpdate(deltaTime);
}
```

`void CEngineApp::Render()` 전체를 아래로 교체:

기존 코드:

```cpp
void CEngineApp::Render()
{
    m_pDevice->BeginFrame(0.08f, 0.08f, 0.12f, 1.f);

    if (m_bDX11RuntimeEnabled)
    {
        m_ImGui.BeginFrame();

        auto* pSceneManager = CGameInstance::Get()->Get_SceneManager();
        pSceneManager->Render();

        pSceneManager->ImGui();

        if (m_pGameApp)
            m_pGameApp->OnImGui();

        DebugRender();

        //Profiler Overlay(F3 Toggle)
        CGameInstance::Get()->Profiler_DrawOverlay();

        m_ImGui.EndFrame();

        CGameInstance::Get()->UI_Render_Cursor();
    }
    else if (m_pGameApp)
    {
        m_pGameApp->OnRender();
    }
    m_pDevice->EndFrame();
}
```

아래로 교체:

```cpp
void CEngineApp::Render()
{
    m_pDevice->BeginFrame(0.08f, 0.08f, 0.12f, 1.f);

    auto* pSceneManager = CGameInstance::Get()->Get_SceneManager();

    if (m_bDX11RuntimeEnabled)
    {
        m_ImGui.BeginFrame();

        if (pSceneManager)
            pSceneManager->Render();

        if (pSceneManager)
            pSceneManager->ImGui();

        if (m_pGameApp)
            m_pGameApp->OnImGui();

        DebugRender();

        //Profiler Overlay(F3 Toggle)
        CGameInstance::Get()->Profiler_DrawOverlay();

        m_ImGui.EndFrame();

        CGameInstance::Get()->UI_Render_Cursor();
    }
    else
    {
        if (pSceneManager)
            pSceneManager->Render();

        if (m_pGameApp)
            m_pGameApp->OnRender();
    }

    m_pDevice->EndFrame();
}
```

2. 검증

검증 명령:

```powershell
git diff --check
rg -n "if \(!m_bDX11RuntimeEnabled\)" Engine/Private/Framework/CEngineApp.cpp
rg -n "DX12\.exe|DX12\.vcxproj|Smoke\.vcxproj|DX12SmokeHost|Tools/Smoke" .md Engine Client Tools Winters.sln
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' Winters.sln /t:Client /m /p:Configuration=Debug /p:Platform=x64 /v:minimal
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' Winters.sln /t:Client /m /p:Configuration=Debug-DX12 /p:Platform=x64 /v:minimal
```

런타임 확인:

```powershell
Client/Bin/Debug/WintersGame.exe --rhi=dx11
Client/Bin/Debug-DX12/WintersGame.exe --rhi=dx12
```

확인 필요:

- DX11에서는 기존 Login/GameSelect/InGame 진입이 깨지지 않는지 확인.
- DX12에서는 scene update/render entry가 호출되는지만 확인. LoL InGame 전체 visual parity는 `CRHISceneRenderer` 이관 전 완료 기준으로 잡지 않는다.
- 새 standalone backend project가 생기지 않았는지 확인.
