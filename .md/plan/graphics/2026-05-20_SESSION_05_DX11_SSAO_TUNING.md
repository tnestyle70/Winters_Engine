Session - DX11 reference SSAO tuning controls and visibility check

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Engine/Public/Renderer/SSAOPass.h

기존 코드:

```cpp
        f32_t GetIntensity() const { return m_fIntensity; }
        void SetIntensity(f32_t value) { m_fIntensity = (value < 0.1f) ? 0.1f : value; }

    private:
        CSSAOPass();
```

아래로 교체:

```cpp
        f32_t GetIntensity() const { return m_fIntensity; }
        void SetIntensity(f32_t value) { m_fIntensity = (value < 0.1f) ? 0.1f : value; }

        f32_t GetThicknessHeuristic() const { return m_fThicknessHeuristic; }
        void SetThicknessHeuristic(f32_t value)
        {
            m_fThicknessHeuristic = (value < 0.0f) ? 0.0f : ((value > 0.5f) ? 0.5f : value);
        }

    private:
        CSSAOPass();
```

의도:
- `GTAO_CS.hlsl`는 이미 `g_fThicknessHeuristic`을 사용한다.
- 현재 UI에서 radius/intensity만 바꿀 수 있어 접촉부 판정 민감도를 조정할 수 없다.
- DX11 reference 튜닝에서는 radius, intensity, thickness 세 값을 같이 잡는다.

1-2. C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_InGame.h

기존 코드:

```cpp
    bool_t GetSSAOEnabled() const { return m_pSSAOPass ? m_pSSAOPass->GetEnabled() : false; }
    void SetSSAOEnabled(bool_t b) { if (m_pSSAOPass) m_pSSAOPass->SetEnabled(b); }
    f32_t GetSSAORadius() const { return m_pSSAOPass ? m_pSSAOPass->GetRadius() : 1.25f; }
    void SetSSAORadius(f32_t v) { if (m_pSSAOPass) m_pSSAOPass->SetRadius(v); }
    f32_t GetSSAOIntensity() const { return m_pSSAOPass ? m_pSSAOPass->GetIntensity() : 1.5f; }
    void SetSSAOIntensity(f32_t v) { if (m_pSSAOPass) m_pSSAOPass->SetIntensity(v); }
```

아래로 교체:

```cpp
    bool_t IsSSAOAvailable() const { return m_pSSAOPass != nullptr; }
    void* GetSSAOOutputSRVNative() const
    {
        return m_pSSAOPass ? m_pSSAOPass->GetOutputSRVNative() : nullptr;
    }
    bool_t GetSSAOEnabled() const { return m_pSSAOPass ? m_pSSAOPass->GetEnabled() : false; }
    void SetSSAOEnabled(bool_t b) { if (m_pSSAOPass) m_pSSAOPass->SetEnabled(b); }
    f32_t GetSSAORadius() const { return m_pSSAOPass ? m_pSSAOPass->GetRadius() : 1.25f; }
    void SetSSAORadius(f32_t v) { if (m_pSSAOPass) m_pSSAOPass->SetRadius(v); }
    f32_t GetSSAOIntensity() const { return m_pSSAOPass ? m_pSSAOPass->GetIntensity() : 1.5f; }
    void SetSSAOIntensity(f32_t v) { if (m_pSSAOPass) m_pSSAOPass->SetIntensity(v); }
    f32_t GetSSAOThicknessHeuristic() const
    {
        return m_pSSAOPass ? m_pSSAOPass->GetThicknessHeuristic() : 0.05f;
    }
    void SetSSAOThicknessHeuristic(f32_t v)
    {
        if (m_pSSAOPass) m_pSSAOPass->SetThicknessHeuristic(v);
    }
```

의도:
- F1 패널이 backend/availability를 거짓으로 보여주지 않게 한다.
- AO 결과 SRV를 F1 패널에서 직접 미리보기로 확인할 수 있게 한다.
- `EngineSDK/inc`는 직접 수정하지 않는다. 필요 시 빌드 전 `UpdateLib.bat`가 동기화한다.

1-3. C:/Users/user/Desktop/Winters/Client/Private/UI/RenderDebug.cpp

기존 코드:

```cpp
        ImGui::Separator();
        ImGui::SeparatorText("SSAO");
        bool bSSAO = pScene->GetSSAOEnabled();
        if (ImGui::Checkbox("SSAO", &bSSAO)) pScene->SetSSAOEnabled(bSSAO);

        f32_t fSSAORadius = pScene->GetSSAORadius();
        if (ImGui::SliderFloat("SSAO radius", &fSSAORadius, 0.05f, 4.0f))
            pScene->SetSSAORadius(fSSAORadius);

        f32_t fSSAOIntensity = pScene->GetSSAOIntensity();
        if (ImGui::SliderFloat("SSAO intensity", &fSSAOIntensity, 0.1f, 4.0f))
            pScene->SetSSAOIntensity(fSSAOIntensity);

        ImGui::SeparatorText("Debug Draw");
```

아래로 교체:

```cpp
        ImGui::Separator();
        ImGui::SeparatorText("SSAO");
        const bool bSSAOAvailable = pScene->IsSSAOAvailable();
        ImGui::Text("Runtime: DX11 reference");
        if (!bSSAOAvailable)
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.25f, 1.0f), "SSAO unavailable on this backend");

        ImGui::BeginDisabled(!bSSAOAvailable);
        {
            bool bSSAO = pScene->GetSSAOEnabled();
            if (ImGui::Checkbox("SSAO", &bSSAO)) pScene->SetSSAOEnabled(bSSAO);

            f32_t fSSAORadius = pScene->GetSSAORadius();
            if (ImGui::SliderFloat("SSAO radius", &fSSAORadius, 0.05f, 4.0f, "%.2f"))
                pScene->SetSSAORadius(fSSAORadius);

            f32_t fSSAOIntensity = pScene->GetSSAOIntensity();
            if (ImGui::SliderFloat("SSAO intensity", &fSSAOIntensity, 0.1f, 4.0f, "%.2f"))
                pScene->SetSSAOIntensity(fSSAOIntensity);

            f32_t fSSAOThickness = pScene->GetSSAOThicknessHeuristic();
            if (ImGui::SliderFloat("SSAO thickness", &fSSAOThickness, 0.0f, 0.5f, "%.3f"))
                pScene->SetSSAOThicknessHeuristic(fSSAOThickness);

            if (ImGui::Button("Soft"))
            {
                pScene->SetSSAOEnabled(true);
                pScene->SetSSAORadius(0.9f);
                pScene->SetSSAOIntensity(1.1f);
                pScene->SetSSAOThicknessHeuristic(0.04f);
            }
            ImGui::SameLine();
            if (ImGui::Button("Reference"))
            {
                pScene->SetSSAOEnabled(true);
                pScene->SetSSAORadius(1.2f);
                pScene->SetSSAOIntensity(1.6f);
                pScene->SetSSAOThicknessHeuristic(0.05f);
            }
            ImGui::SameLine();
            if (ImGui::Button("Stress"))
            {
                pScene->SetSSAOEnabled(true);
                pScene->SetSSAORadius(2.5f);
                pScene->SetSSAOIntensity(3.5f);
                pScene->SetSSAOThicknessHeuristic(0.10f);
            }

            if (void* pAOSRV = pScene->GetSSAOOutputSRVNative())
            {
                ImGui::Text("AO preview");
                ImGui::Image(static_cast<ImTextureID>(pAOSRV), ImVec2(220.f, 124.f));
            }
        }
        ImGui::EndDisabled();

        ImGui::SeparatorText("Debug Draw");
```

의도:
- F1에서 조절해도 안 변하는지 즉시 알 수 있게 AO texture preview를 붙인다.
- backend가 DX12이거나 SSAO pass가 생성되지 않았을 때 no-op 슬라이더를 만지는 일을 막는다.
- `Soft / Reference / Stress` 버튼으로 육안 차이를 빠르게 만든다.
- 이 세션은 DX11 reference 렌더만 대상으로 한다. DX12/Vulkan RHI 포팅은 별도 세션으로 분리한다.

1-4. C:/Users/user/Desktop/Winters/Client/Private/UI/RenderDebug.cpp

기존 코드:

```cpp
        ImGui::SetNextWindowSize(ImVec2(300.f, 280.f), ImGuiCond_FirstUseEver);
```

아래로 교체:

```cpp
        ImGui::SetNextWindowSize(ImVec2(340.f, 460.f), ImGuiCond_FirstUseEver);
```

의도:
- AO preview와 preset 버튼이 들어가면 기존 높이로는 첫 사용 시 튜닝 영역이 잘린다.

1-5. C:/Users/user/Desktop/Winters/Shaders/Mesh3D.hlsl

기존 코드:

```cpp
    color *= lerp(0.62f, 1.f, saturate(ao));
```

아래로 교체:

```cpp
    const float contactAO = saturate(ao);
    color *= lerp(0.52f, 1.f, contactAO);
```

의도:
- DX11 reference에서 SSAO on/off 차이를 더 쉽게 본다.
- 최종 튜닝 시 너무 탁하면 `0.52f`를 `0.58f~0.64f`로 되돌린다.

1-6. C:/Users/user/Desktop/Winters/Shaders/Skinned3D.hlsl

기존 코드:

```cpp
    color *= lerp(0.62f, 1.00f, saturate(ao));
```

아래로 교체:

```cpp
    const float contactAO = saturate(ao);
    color *= lerp(0.52f, 1.00f, contactAO);
```

의도:
- 맵과 챔피언의 AO 응답 강도를 맞춘다.
- `Mesh3D.hlsl`과 `Skinned3D.hlsl`의 diffuse-only lighting 경로를 계속 같은 공식으로 유지한다.

1-7. C:/Users/user/Desktop/Winters/Client/Private/Scene/InGameRenderBridge.cpp

기존 코드:

```cpp
    scene.m_Map.UpdateCamera(vp, cameraWorld);
    scene.m_Map.UpdateTransform(scene.m_MapTransform.GetWorldMatrix());
    scene.m_Map.SetAmbientOcclusionSRV(pAmbientOcclusionSRV);
    scene.m_Map.Render();
```

변경 없음.

의도:
- 이미 맵에 AO SRV를 연결해 둔 상태다.
- 이번 세션에서는 이 연결을 유지하고, F1 preview와 shader response로 실제 차이를 검증한다.

2. 검증

미검증:
- DX11 F1 Render Debug에서 `SSAO unavailable` 문구가 뜨지 않는지 확인.
- `AO preview`가 흰 화면이 아니라 맵/챔피언 접촉부가 어두운 grayscale로 보이는지 확인.
- `Soft`, `Reference`, `Stress` preset이 각각 즉시 다른 화면 결과를 만드는지 확인.
- `SSAO` checkbox off/on에서 맵 전체 밝기가 아니라 발밑, 벽/돌 틈, 챔피언 접촉부가 달라지는지 확인.

검증 명령:
- `git diff --check`
- `fxc /T ps_5_0 /E PS /Fo NUL Shaders/Mesh3D.hlsl`
- `fxc /T ps_5_0 /E PS /Fo NUL Shaders/Skinned3D.hlsl`
- `msbuild Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`

수동 확인:
- Visual Studio 실행 인자 없이 DX11 기본 실행 또는 `--rhi=dx11`로 실행.
- F1 Render Debug에서 `Stress`를 누른 뒤 발밑 접촉 그림자가 눈에 보이는지 확인.
- 최종값 후보는 먼저 `Reference`에서 시작하고, 과하면 intensity를 낮춘다.

확인 필요:
- `Debug-DX12` 구성에서 DX11 fallback으로 실제 실행되는지, 아니면 `--rhi=dx12` 인자를 넣고 있는지 Visual Studio Debugging 설정 확인.
- DX12/Vulkan SSAO 포팅은 이번 세션 범위 밖이다.
