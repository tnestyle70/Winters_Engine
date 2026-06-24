Session - S18 RHI scene-only parity gate를 추가한다.

RHI scene snapshot migration이 map, champion, structure, jungle, minion, ambient prop까지 도달했으므로, normal F5 경로는 유지하면서 명시적 비교 실행에서만 legacy world mesh draw를 생략하는 검증 게이트를 둔다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/Scene_InGameRender.cpp

기존 코드:

```cpp
    u32_t AppendChampionSnapshotMeshes(
        CWorld& world,
        RenderWorldSnapshot& snapshot,
        const Mat4& matViewProjection,
        const u8_t localTeam,
        bool_t bRevealAllForPlayback)
```

아래에 추가:

```cpp
    bool_t IsRHISceneOnlyMode()
    {
        return HasCommandLineToken(L"--rhi-scene-only") ||
            HasCommandLineToken(L"/rhi-scene-only");
    }
```

기존 코드:

```cpp
    const bool_t bUseDX12RHI = pDevice && pDevice->GetBackend() == eRHIBackend::DX12;
    const bool_t bUseDX11RHI = pDevice && pDevice->GetBackend() == eRHIBackend::DX11;
    const bool_t bSSAOEnabled = m_pSSAOPass && m_pSSAOPass->GetEnabled();
```

아래에 추가:

```cpp
    const bool_t bRHISceneReady = m_pRHISceneRenderer && m_pRHISceneRenderer->IsReady();
    const bool_t bRHISceneOnly = bRHISceneReady && IsRHISceneOnlyMode();
    WINTERS_PROFILE_COUNT("RHISceneOnly", bRHISceneOnly ? 1u : 0u);
```

기존 코드:

```cpp
    if (bUseDX11RHI && m_pNormalPass && bSSAOEnabled)
```

아래로 교체:

```cpp
    if (!bRHISceneOnly && bUseDX11RHI && m_pNormalPass && bSSAOEnabled)
```

기존 코드:

```cpp
        if (m_pRHISceneRenderer && m_pRHISceneRenderer->IsReady())
```

아래로 교체:

```cpp
        if (bRHISceneReady)
```

기존 코드:

```cpp
        {
            WINTERS_PROFILE_SCOPE("Map::DrawFrustumCulled");
            m_Map.RenderFrustumCulled(vp);
        }
```

아래로 교체:

```cpp
        if (!bRHISceneOnly)
        {
            WINTERS_PROFILE_SCOPE("Map::DrawFrustumCulled");
            m_Map.RenderFrustumCulled(vp);
        }
```

기존 코드:

```cpp
        m_World.ForEach<ChampionComponent, RenderComponent, TransformComponent>(
            [&](EntityID e, ChampionComponent& champion, RenderComponent& rc,
                TransformComponent& tf)
```

아래로 교체:

```cpp
        if (!bRHISceneOnly)
        {
            m_World.ForEach<ChampionComponent, RenderComponent, TransformComponent>(
                [&](EntityID e, ChampionComponent& champion, RenderComponent& rc,
                    TransformComponent& tf)
```

기존 champion legacy loop의 닫는 코드:

```cpp
            }
        );
```

아래로 교체:

```cpp
                }
            );
        }
```

기존 코드:

```cpp
    {
        WINTERS_PROFILE_SCOPE("Structure::Render");
        CStructure_Manager::Get()->Render(vp, cameraWorld, pAmbientOcclusionSRV, bRevealAllForPlayback);
    }
    {
        WINTERS_PROFILE_SCOPE("Jungle::Render");
        CJungle_Manager::Get()->Render(vp, cameraWorld, pAmbientOcclusionSRV);
    }
    CMinion_Manager::Get()->Render(vp, cameraWorld, pAmbientOcclusionSRV, bRevealAllForPlayback);

    CAmbientProp_Manager::Get()->Render(vp, cameraWorld);
```

아래로 교체:

```cpp
    if (!bRHISceneOnly)
    {
        {
            WINTERS_PROFILE_SCOPE("Structure::Render");
            CStructure_Manager::Get()->Render(vp, cameraWorld, pAmbientOcclusionSRV, bRevealAllForPlayback);
        }
        {
            WINTERS_PROFILE_SCOPE("Jungle::Render");
            CJungle_Manager::Get()->Render(vp, cameraWorld, pAmbientOcclusionSRV);
        }
        CMinion_Manager::Get()->Render(vp, cameraWorld, pAmbientOcclusionSRV, bRevealAllForPlayback);

        CAmbientProp_Manager::Get()->Render(vp, cameraWorld);
    }
```

기존 코드:

```cpp
    if (!bUseDX12RHI && m_pContactShadowPlane)
```

아래로 교체:

```cpp
    if (!bRHISceneOnly && !bUseDX12RHI && m_pContactShadowPlane)
```

2. 검증

```powershell
git diff --check
```

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Tools\Harness\Run-S17RhiValidation.ps1 -ReportPath .md\build\2026-06-24_S18_RHI_SCENE_ONLY_PARITY_HARNESS_REPORT.md
```

추가 수동 실행 기준:

```powershell
Client\Bin\Debug\WintersGame.exe --rhi-scene-only
```

성공 기준:

- normal F5에서는 legacy world mesh draw가 유지된다.
- `--rhi-scene-only`에서 RHI scene renderer가 준비된 경우 map/champion/scene object legacy mesh draw가 생략된다.
- RHI scene renderer가 준비되지 않으면 legacy draw가 유지되어 빈 화면 fallback을 만들지 않는다.
- `Run-S17RhiValidation.ps1` runtime smoke에 `WintersGame_rhi_scene_only`가 포함된다.
- `Client/Public`와 `Shared`에 신규 DX11/DX12 concrete 노출이 없다.
- S17 harness가 PASS한다.
