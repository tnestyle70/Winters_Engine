Session - DX11 ground contact shadow decal

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shaders/ContactShadowPlane.hlsl

새 파일:

```cpp
cbuffer CBPerFrame : register(b0)
{
    row_major matrix g_matViewProj;
};

cbuffer CBPerObject : register(b1)
{
    row_major matrix g_matWorld;
};

cbuffer CBFxParams : register(b2)
{
    float4 g_vTint;
    float4 g_vUVRect;
    float2 g_vUVScroll;
    float g_fAlphaClip;
    float g_fErodeThreshold;

    float4 g_vStyleColorA;
    float4 g_vStyleColorB;
    float4 g_vRimColor;
    float4 g_vStyleParams;
    float4 g_vTimeParams;
    float4 g_vMagicScrollA;
    float4 g_vMagicShape;
    float4 g_vMagicCore;
};

struct VS_INPUT
{
    float3 vPosition : POSITION;
    float3 vNormal : NORMAL;
    float2 vTexCoord : TEXCOORD0;
    float3 vTangent : TANGENT;
};

struct PS_INPUT
{
    float4 vPosition : SV_POSITION;
    float2 vTexCoord : TEXCOORD0;
};

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;

    const float4 worldPos = mul(float4(input.vPosition, 1.0f), g_matWorld);
    output.vPosition = mul(worldPos, g_matViewProj);
    output.vTexCoord = input.vTexCoord;

    return output;
}

float4 PS(PS_INPUT input) : SV_TARGET
{
    const float2 centered = input.vTexCoord * 2.0f - 1.0f;
    const float2 ellipse = float2(centered.x * 0.82f, centered.y * 1.18f);
    const float d2 = dot(ellipse, ellipse);

    const float broad = 1.0f - smoothstep(0.18f, 1.0f, d2);
    const float core = 1.0f - smoothstep(0.0f, 0.36f, d2);
    const float alpha = saturate(broad * 0.54f + core * 0.18f) * g_vTint.a;

    clip(alpha - max(g_fAlphaClip, 0.001f));

    return float4(g_vTint.rgb, alpha);
}
```

1-2. C:/Users/user/Desktop/Winters/Engine/Public/Framework/CEngineApp.h

기존 코드:

```cpp
    DX11Shader* GetUIPlaneShader() { return m_pUIPlaneShader.get(); }
    DX11Pipeline* GetUIPlanePipeline() { return m_pUIPlanePipeline.get(); }
    CBlendStateCache* GetBlendStateCache() { return m_pBlendStateCache.get(); }
```

아래로 교체:

```cpp
    DX11Shader* GetUIPlaneShader() { return m_pUIPlaneShader.get(); }
    DX11Pipeline* GetUIPlanePipeline() { return m_pUIPlanePipeline.get(); }
    DX11Shader* GetContactShadowShader() { return m_pContactShadowShader.get(); }
    DX11Pipeline* GetContactShadowPipeline() { return m_pContactShadowPipeline.get(); }
    CBlendStateCache* GetBlendStateCache() { return m_pBlendStateCache.get(); }
```

기존 코드:

```cpp
    unique_ptr<DX11Shader> m_pUIPlaneShader = { nullptr };
    unique_ptr<DX11Pipeline> m_pUIPlanePipeline = { nullptr };
    unique_ptr<CBlendStateCache> m_pBlendStateCache = { nullptr };
```

아래로 교체:

```cpp
    unique_ptr<DX11Shader> m_pUIPlaneShader = { nullptr };
    unique_ptr<DX11Pipeline> m_pUIPlanePipeline = { nullptr };
    unique_ptr<DX11Shader> m_pContactShadowShader = { nullptr };
    unique_ptr<DX11Pipeline> m_pContactShadowPipeline = { nullptr };
    unique_ptr<CBlendStateCache> m_pBlendStateCache = { nullptr };
```

1-3. C:/Users/user/Desktop/Winters/Engine/Private/Framework/CEngineApp.cpp

기존 코드:

```cpp
    m_pUIPlaneShader = unique_ptr<DX11Shader>(new DX11Shader());
    if (!m_pUIPlaneShader->Load(pDev, uiPlanePath)) return false;

    m_pUIPlanePipeline = unique_ptr<DX11Pipeline>(new DX11Pipeline());
    if (!m_pUIPlanePipeline->CreateMesh(pDev, m_pUIPlaneShader->GetVSBlob())) return false;

    OutputDebugStringA("[CEngineApp] Shared Mesh3D/Skinned3D + PBR + Fx + UIPlane shaders/pipelines ready\n");
```

아래로 교체:

```cpp
    m_pUIPlaneShader = unique_ptr<DX11Shader>(new DX11Shader());
    if (!m_pUIPlaneShader->Load(pDev, uiPlanePath)) return false;

    m_pUIPlanePipeline = unique_ptr<DX11Pipeline>(new DX11Pipeline());
    if (!m_pUIPlanePipeline->CreateMesh(pDev, m_pUIPlaneShader->GetVSBlob())) return false;

    wchar_t contactShadowPath[MAX_PATH] = {};
    if (!WintersResolveContentPath(L"Shaders/ContactShadowPlane.hlsl", contactShadowPath, MAX_PATH))
    {
        OutputDebugStringA("[CEngineApp] ContactShadowPlane.hlsl path resolve failed\n");
        return false;
    }
    m_pContactShadowShader = unique_ptr<DX11Shader>(new DX11Shader());
    if (!m_pContactShadowShader->Load(pDev, contactShadowPath)) return false;

    m_pContactShadowPipeline = unique_ptr<DX11Pipeline>(new DX11Pipeline());
    if (!m_pContactShadowPipeline->CreateMesh(pDev, m_pContactShadowShader->GetVSBlob())) return false;

    OutputDebugStringA("[CEngineApp] Shared Mesh3D/Skinned3D + PBR + Fx + UIPlane + ContactShadow shaders/pipelines ready\n");
```

기존 코드:

```cpp
    m_pUIPlanePipeline.reset();
    m_pUIPlaneShader.reset();
    m_pFxSpritePipeline.reset();
```

아래로 교체:

```cpp
    m_pContactShadowPipeline.reset();
    m_pContactShadowShader.reset();
    m_pUIPlanePipeline.reset();
    m_pUIPlaneShader.reset();
    m_pFxSpritePipeline.reset();
```

1-4. C:/Users/user/Desktop/Winters/Engine/Include/GameInstance.h

기존 코드:

```cpp
    DX11Shader* Get_UIPlaneShader();
    DX11Pipeline* Get_UIPlanePipeline();
```

아래로 교체:

```cpp
    DX11Shader* Get_UIPlaneShader();
    DX11Pipeline* Get_UIPlanePipeline();
    DX11Shader* Get_ContactShadowShader();
    DX11Pipeline* Get_ContactShadowPipeline();
```

1-5. C:/Users/user/Desktop/Winters/Engine/Private/GameInstance.cpp

기존 코드:

```cpp
DX11Pipeline* CGameInstance::Get_UIPlanePipeline()
{
    return CEngineApp::Get().GetUIPlanePipeline();
}

HRESULT CGameInstance::Initialize_Engine(uint32_t iNumScenes)
```

아래로 교체:

```cpp
DX11Pipeline* CGameInstance::Get_UIPlanePipeline()
{
    return CEngineApp::Get().GetUIPlanePipeline();
}

DX11Shader* CGameInstance::Get_ContactShadowShader()
{
    return CEngineApp::Get().GetContactShadowShader();
}

DX11Pipeline* CGameInstance::Get_ContactShadowPipeline()
{
    return CEngineApp::Get().GetContactShadowPipeline();
}

HRESULT CGameInstance::Initialize_Engine(uint32_t iNumScenes)
```

1-6. C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_InGame.h

기존 코드:

```cpp
    CPlaneRenderer* GetTurretProjectilePlane() const { return m_pTurretProjectilePlane.get(); }
    CTexture* GetTurretProjectileTexture() const { return m_pTurretProjectileTex.get(); }
    f32_t GetTurretProjectileQuadSize() const { return TURRET_PROJECTILE_QUAD_SIZE; }
```

아래로 교체:

```cpp
    CPlaneRenderer* GetTurretProjectilePlane() const { return m_pTurretProjectilePlane.get(); }
    CTexture* GetTurretProjectileTexture() const { return m_pTurretProjectileTex.get(); }
    CPlaneRenderer* GetContactShadowPlane() const { return m_pContactShadowPlane.get(); }
    f32_t GetTurretProjectileQuadSize() const { return TURRET_PROJECTILE_QUAD_SIZE; }
```

기존 코드:

```cpp
    std::unique_ptr<CPlaneRenderer> m_pAttackRangePlane;
    std::unique_ptr<CTexture>       m_pAttackRangeTex;
    std::unique_ptr<CPlaneRenderer> m_pTurretProjectilePlane;
    std::unique_ptr<CTexture>       m_pTurretProjectileTex;
```

아래로 교체:

```cpp
    std::unique_ptr<CPlaneRenderer> m_pAttackRangePlane;
    std::unique_ptr<CTexture>       m_pAttackRangeTex;
    std::unique_ptr<CPlaneRenderer> m_pTurretProjectilePlane;
    std::unique_ptr<CTexture>       m_pTurretProjectileTex;
    std::unique_ptr<CPlaneRenderer> m_pContactShadowPlane;
```

1-7. C:/Users/user/Desktop/Winters/Client/Private/Scene/InGameBootstrapBridge.cpp

기존 코드:

```cpp
            if (scene.m_pTurretProjectilePlane && scene.m_pTurretProjectileTex)
            {
                scene.m_pTurretProjectilePlane->SetTexture(scene.m_pTurretProjectileTex.get());
                scene.m_pTurretProjectilePlane->SetBlendCache(
                    pGI->Get_BlendStateCache(),
                    eBlendPreset::AlphaBlend);
                scene.m_pTurretProjectilePlane->SetFxParams(
                    { 0.85f, 0.95f, 1.0f, 1.0f },
                    { 0.f, 0.f, 1.f, 1.f },
                    { 0.f, 0.f },
                    0.02f,
                    0.f);
            }
        }

        if (!bUseDX12RHI)
```

아래로 교체:

```cpp
            if (scene.m_pTurretProjectilePlane && scene.m_pTurretProjectileTex)
            {
                scene.m_pTurretProjectilePlane->SetTexture(scene.m_pTurretProjectileTex.get());
                scene.m_pTurretProjectilePlane->SetBlendCache(
                    pGI->Get_BlendStateCache(),
                    eBlendPreset::AlphaBlend);
                scene.m_pTurretProjectilePlane->SetFxParams(
                    { 0.85f, 0.95f, 1.0f, 1.0f },
                    { 0.f, 0.f, 1.f, 1.f },
                    { 0.f, 0.f },
                    0.02f,
                    0.f);
            }

            scene.m_pContactShadowPlane = CPlaneRenderer::Create(
                pRhiDevice,
                pGI->Get_ContactShadowShader(),
                pGI->Get_ContactShadowPipeline());

            if (scene.m_pContactShadowPlane)
            {
                scene.m_pContactShadowPlane->SetBlendCache(
                    pGI->Get_BlendStateCache(),
                    eBlendPreset::AlphaBlend);
                scene.m_pContactShadowPlane->SetFxParams(
                    { 0.015f, 0.018f, 0.020f, 0.44f },
                    { 0.f, 0.f, 1.f, 1.f },
                    { 0.f, 0.f },
                    0.003f,
                    0.f);
            }
        }

        if (!bUseDX12RHI)
```

1-8. C:/Users/user/Desktop/Winters/Client/Private/Scene/InGameRenderBridge.cpp

`FlushTransformForRender` 함수 바로 아래, 익명 namespace 닫기 전 아래에 추가:

```cpp
    Mat4 BuildContactShadowWorld(const TransformComponent& tf,
        f32_t fSize,
        f32_t fYOffset)
    {
        const Vec3 vPos = tf.GetPosition();
        const DirectX::XMMATRIX matScale =
            DirectX::XMMatrixScaling(fSize, 1.f, fSize * 0.72f);
        const DirectX::XMMATRIX matTrans =
            DirectX::XMMatrixTranslation(vPos.x, vPos.y + fYOffset, vPos.z);

        Mat4 matWorld{};
        DirectX::XMStoreFloat4x4(
            reinterpret_cast<DirectX::XMFLOAT4X4*>(&matWorld.m),
            matScale * matTrans);

        return matWorld;
    }
```

Session 09 적용 후 기존 코드:

```cpp
    CStructure_Manager::Get()->Render(vp, cameraWorld, pAmbientOcclusionSRV);
    CJungle_Manager::Get()->Render(vp, cameraWorld, pAmbientOcclusionSRV);
    CMinion_Manager::Get()->Render(vp, cameraWorld, pAmbientOcclusionSRV);

    if (scene.m_pFogOfWarRenderer)
```

아래로 교체:

```cpp
    CStructure_Manager::Get()->Render(vp, cameraWorld, pAmbientOcclusionSRV);
    CJungle_Manager::Get()->Render(vp, cameraWorld, pAmbientOcclusionSRV);
    CMinion_Manager::Get()->Render(vp, cameraWorld, pAmbientOcclusionSRV);

    if (!bUseDX12RHI && scene.m_pContactShadowPlane)
    {
        WINTERS_PROFILE_SCOPE("ContactShadow::Render");
        scene.m_World.ForEach<ChampionComponent, RenderComponent, TransformComponent>(
            [&](EntityID e, ChampionComponent&, RenderComponent& rc, TransformComponent& tf)
            {
                if (rc.bSceneManaged || !rc.bVisible || !rc.pRenderer)
                    return;
                if (!UI::IsRenderableForLocal(scene.m_World, e, localTeam))
                    return;

                FlushTransformForRender(tf);

                const Mat4 matShadow = BuildContactShadowWorld(tf, 1.22f, 0.055f);
                scene.m_pContactShadowPlane->SetWorld(matShadow);
                scene.m_pContactShadowPlane->Render(pDevice, vp);
            });
    }

    if (scene.m_pFogOfWarRenderer)
```

2. 검증

미검증.

- `git diff --check -- Shaders/ContactShadowPlane.hlsl Engine/Public/Framework/CEngineApp.h Engine/Private/Framework/CEngineApp.cpp Engine/Include/GameInstance.h Engine/Private/GameInstance.cpp Client/Public/Scene/Scene_InGame.h Client/Private/Scene/InGameBootstrapBridge.cpp Client/Private/Scene/InGameRenderBridge.cpp`
- `& 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\fxc.exe' /T ps_5_0 /E PS /Fo NUL Shaders/ContactShadowPlane.hlsl`
- `& 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\fxc.exe' /T vs_5_0 /E VS /Fo NUL Shaders/ContactShadowPlane.hlsl`
- `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Engine/Include/Engine.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`
- `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`
- Public header 변경이 있으므로 빌드가 통과하면 `UpdateLib.bat`로 SDK 헤더 동기화를 확인한다.
- F5에서 같은 카메라로 champion 발밑이 SSAO 토글과 별개로 안정적인 타원형 접촉감을 갖는지 확인한다.
- 그림자가 스킬 범위, 체력바, UI, Fog of War 위로 떠 보이면 실패다.
