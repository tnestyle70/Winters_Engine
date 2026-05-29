Session - DX11 UI and indicator unlit shader split

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shaders/UIPlane.hlsl

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

Texture2D g_DiffuseMap : register(t0);
SamplerState g_Sampler : register(s0);

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
    output.vTexCoord = lerp(g_vUVRect.xy, g_vUVRect.zw, input.vTexCoord);
    output.vTexCoord += g_vUVScroll;

    return output;
}

float4 PS(PS_INPUT input) : SV_TARGET
{
    float4 texColor = g_DiffuseMap.Sample(g_Sampler, input.vTexCoord);
    texColor *= g_vTint;

    if (g_fErodeThreshold > 0.0f)
    {
        const float erodeMask = dot(texColor.rgb, float3(0.299f, 0.587f, 0.114f));
        clip(erodeMask - g_fErodeThreshold);
    }

    if (g_fAlphaClip > 0.0f)
        clip(texColor.a - g_fAlphaClip);

    return texColor;
}
```

1-2. C:/Users/user/Desktop/Winters/Engine/Public/Framework/CEngineApp.h

기존 코드:

```cpp
    DX11Shader* GetFxSpriteShader() { return m_pFxSpriteShader.get(); }
    DX11Pipeline* GetFxSpritePipeline() { return m_pFxSpritePipeline.get(); }
    DX11Shader* GetFxMeshShader() { return m_pFxMeshShader.get(); }
    DX11Pipeline* GetFxMeshPipeline() { return m_pFxMeshPipeline.get(); }
```

아래로 교체:

```cpp
    DX11Shader* GetFxSpriteShader() { return m_pFxSpriteShader.get(); }
    DX11Pipeline* GetFxSpritePipeline() { return m_pFxSpritePipeline.get(); }
    DX11Shader* GetFxMeshShader() { return m_pFxMeshShader.get(); }
    DX11Pipeline* GetFxMeshPipeline() { return m_pFxMeshPipeline.get(); }
    DX11Shader* GetUIPlaneShader() { return m_pUIPlaneShader.get(); }
    DX11Pipeline* GetUIPlanePipeline() { return m_pUIPlanePipeline.get(); }
```

기존 코드:

```cpp
    unique_ptr<DX11Shader> m_pFxSpriteShader = { nullptr };
    unique_ptr<DX11Pipeline> m_pFxSpritePipeline = { nullptr };
    unique_ptr<DX11Shader> m_pFxMeshShader = { nullptr };
    unique_ptr<DX11Pipeline> m_pFxMeshPipeline = { nullptr };
    unique_ptr<CBlendStateCache> m_pBlendStateCache = { nullptr };
```

아래로 교체:

```cpp
    unique_ptr<DX11Shader> m_pFxSpriteShader = { nullptr };
    unique_ptr<DX11Pipeline> m_pFxSpritePipeline = { nullptr };
    unique_ptr<DX11Shader> m_pFxMeshShader = { nullptr };
    unique_ptr<DX11Pipeline> m_pFxMeshPipeline = { nullptr };
    unique_ptr<DX11Shader> m_pUIPlaneShader = { nullptr };
    unique_ptr<DX11Pipeline> m_pUIPlanePipeline = { nullptr };
    unique_ptr<CBlendStateCache> m_pBlendStateCache = { nullptr };
```

1-3. C:/Users/user/Desktop/Winters/Engine/Private/Framework/CEngineApp.cpp

기존 코드:

```cpp
    m_pSkinnedPipeline = unique_ptr<DX11Pipeline>(new DX11Pipeline());
    if (!m_pSkinnedPipeline->CreateSkinnedMesh(pDev, m_pSkinnedShader->GetVSBlob())) return false;

    wchar_t meshPBRPath[MAX_PATH] = {};
```

아래로 교체:

```cpp
    m_pSkinnedPipeline = unique_ptr<DX11Pipeline>(new DX11Pipeline());
    if (!m_pSkinnedPipeline->CreateSkinnedMesh(pDev, m_pSkinnedShader->GetVSBlob())) return false;

    wchar_t uiPlanePath[MAX_PATH] = {};
    if (!WintersResolveContentPath(L"Shaders/UIPlane.hlsl", uiPlanePath, MAX_PATH))
    {
        OutputDebugStringA("[CEngineApp] UIPlane.hlsl path resolve failed\n");
        return false;
    }
    m_pUIPlaneShader = unique_ptr<DX11Shader>(new DX11Shader());
    if (!m_pUIPlaneShader->Load(pDev, uiPlanePath)) return false;

    m_pUIPlanePipeline = unique_ptr<DX11Pipeline>(new DX11Pipeline());
    if (!m_pUIPlanePipeline->CreateMesh(pDev, m_pUIPlaneShader->GetVSBlob())) return false;

    wchar_t meshPBRPath[MAX_PATH] = {};
```

기존 코드:

```cpp
    OutputDebugStringA("[CEngineApp] Shared Mesh3D/Skinned3D + PBR + Fx shaders/pipelines ready\n");
```

아래로 교체:

```cpp
    OutputDebugStringA("[CEngineApp] Shared Mesh3D/Skinned3D + PBR + Fx + UIPlane shaders/pipelines ready\n");
```

기존 코드:

```cpp
    m_pFxSpritePipeline.reset();
    m_pFxSpriteShader.reset();
    m_pSkinnedPBRPipeline.reset();
```

아래로 교체:

```cpp
    m_pFxSpritePipeline.reset();
    m_pFxSpriteShader.reset();
    m_pUIPlanePipeline.reset();
    m_pUIPlaneShader.reset();
    m_pSkinnedPBRPipeline.reset();
```

1-4. C:/Users/user/Desktop/Winters/Client/Private/Scene/InGameBootstrapBridge.cpp

기존 코드:

```cpp
            scene.m_pAttackRangePlane = CPlaneRenderer::Create(
                pRhiDevice,
                pGI->Get_MeshShader(),
                pGI->Get_MeshPipeline());
```

아래로 교체:

```cpp
            scene.m_pAttackRangePlane = CPlaneRenderer::Create(
                pRhiDevice,
                pGI->GetUIPlaneShader(),
                pGI->GetUIPlanePipeline());
```

기존 코드:

```cpp
            scene.m_pTurretProjectilePlane = CPlaneRenderer::Create(
                pRhiDevice,
                pGI->Get_MeshShader(),
                pGI->Get_MeshPipeline());
```

아래로 교체:

```cpp
            scene.m_pTurretProjectilePlane = CPlaneRenderer::Create(
                pRhiDevice,
                pGI->GetUIPlaneShader(),
                pGI->GetUIPlanePipeline());
```

1-5. C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp

챔피언 체력바는 `CUIRenderer`의 unlit screen-space 경로를 이미 탄다. 문제는 `HealthBarGreen.png`와 `HealthBarRed.png`가 완전 불투명 baked-color 이미지라 조명처럼 보이는 것이다. 이번 세션에서는 PNG를 체력 fill 소스로 쓰지 않고, 절차적 색 fill로 고정한다.

기존 코드:

```cpp
            const eTeam team = UI_Resolve_Team(m_pWorld, id);
            const bool_t bAlly = (team != eTeam::TEAM_END && team == localTeam);
            void* pBarSRV = bAlly ? m_pSRV_HPBarGreen : m_pSRV_HPBarRed;

            if (pBarSRV)
            {
                pDraw->AddImage(
                    reinterpret_cast<ImTextureID>(pBarSRV),
                    rects.BarMin,
                    rects.BarMax,
                    ImVec2(0.f, 0.f),
                    ImVec2(1.f, 1.f),
                    IM_COL32(255, 255, 255, 255));
            }
            else
            {
                pDraw->AddRectFilled(rects.BarMin, rects.BarMax, IM_COL32(10, 10, 10, 220));
                pDraw->AddRectFilled(
                    rects.FillMin,
                    rects.FillMax,
                    bAlly ? IM_COL32(80, 205, 70, 255) : IM_COL32(225, 48, 48, 255));
            }
```

아래로 교체:

```cpp
            const eTeam team = UI_Resolve_Team(m_pWorld, id);
            const bool_t bAlly = (team != eTeam::TEAM_END && team == localTeam);

            pDraw->AddRectFilled(rects.BarMin, rects.BarMax, IM_COL32(10, 10, 10, 226));
            pDraw->AddRectFilled(
                rects.FillMin,
                rects.FillMax,
                bAlly ? IM_COL32(74, 190, 72, 255) : IM_COL32(218, 54, 54, 255));
            pDraw->AddRect(
                rects.FillMin,
                rects.FillMax,
                IM_COL32(255, 255, 255, 32),
                0.f,
                0,
                1.0f);
```

기존 코드:

```cpp
            const eTeam team = UI_Resolve_Team(m_pWorld, id);
            const bool_t bAlly = (team != eTeam::TEAM_END && team == localTeam);
            void* pBarSRV = bAlly ? m_pSRV_HPBarGreen : m_pSRV_HPBarRed;

            if (pBarSRV)
            {
                m_pRHIUIRenderer->DrawImage(
                    pBarSRV,
                    rects.BarMin.x,
                    rects.BarMin.y,
                    rects.BarMax.x - rects.BarMin.x,
                    rects.BarMax.y - rects.BarMin.y,
                    uvFull,
                    UI_WhiteVec());
            }
            else
            {
                m_pRHIUIRenderer->DrawImage(
                    nullptr,
                    rects.BarMin.x,
                    rects.BarMin.y,
                    rects.BarMax.x - rects.BarMin.x,
                    rects.BarMax.y - rects.BarMin.y,
                    uvFull,
                    Vec4(0.04f, 0.04f, 0.04f, 0.88f));
                m_pRHIUIRenderer->DrawImage(
                    nullptr,
                    rects.FillMin.x,
                    rects.FillMin.y,
                    rects.FillMax.x - rects.FillMin.x,
                    rects.FillMax.y - rects.FillMin.y,
                    uvFull,
                    bAlly ? Vec4(0.31f, 0.80f, 0.27f, 1.0f) : Vec4(0.88f, 0.19f, 0.19f, 1.0f));
            }
```

아래로 교체:

```cpp
            const eTeam team = UI_Resolve_Team(m_pWorld, id);
            const bool_t bAlly = (team != eTeam::TEAM_END && team == localTeam);

            m_pRHIUIRenderer->DrawImage(
                nullptr,
                rects.BarMin.x,
                rects.BarMin.y,
                rects.BarMax.x - rects.BarMin.x,
                rects.BarMax.y - rects.BarMin.y,
                uvFull,
                Vec4(0.04f, 0.04f, 0.04f, 0.89f));
            m_pRHIUIRenderer->DrawImage(
                nullptr,
                rects.FillMin.x,
                rects.FillMin.y,
                rects.FillMax.x - rects.FillMin.x,
                rects.FillMax.y - rects.FillMin.y,
                uvFull,
                bAlly ? Vec4(0.29f, 0.74f, 0.28f, 1.0f) : Vec4(0.85f, 0.21f, 0.21f, 1.0f));
            m_pRHIUIRenderer->DrawImage(
                nullptr,
                rects.FillMin.x,
                rects.FillMin.y,
                rects.FillMax.x - rects.FillMin.x,
                1.0f,
                uvFull,
                Vec4(1.0f, 1.0f, 1.0f, 0.10f));
```

기존 코드:

```cpp
            const bool_t bBlueTeam = minion.team == eTeam::Blue;
            void* pBarSRV = bBlueTeam ? m_pSRV_MinionBlueHPBar : m_pSRV_MinionRedHPBar;

            pDraw->AddRectFilled(barMin, barMax, IM_COL32(14, 14, 14, 232));
            if (fillW > 0.5f)
            {
                if (pBarSRV)
                {
                    pDraw->AddImage(
                        reinterpret_cast<ImTextureID>(pBarSRV),
                        barMin,
                        fillMax,
                        ImVec2(0.f, 0.f),
                        ImVec2(clamped, 1.f),
                        IM_COL32(255, 255, 255, 255));
                }
                else
                {
                    pDraw->AddRectFilled(
                        barMin,
                        fillMax,
                        bBlueTeam ? IM_COL32(55, 150, 255, 255) : IM_COL32(230, 54, 54, 255));
                }
            }
```

아래로 교체:

```cpp
            const bool_t bBlueTeam = minion.team == eTeam::Blue;

            pDraw->AddRectFilled(barMin, barMax, IM_COL32(14, 14, 14, 232));
            if (fillW > 0.5f)
            {
                pDraw->AddRectFilled(
                    barMin,
                    fillMax,
                    bBlueTeam ? IM_COL32(48, 132, 224, 255) : IM_COL32(218, 58, 48, 255));
                pDraw->AddLine(
                    barMin,
                    ImVec2(fillMax.x, barMin.y),
                    IM_COL32(255, 255, 255, 36),
                    1.0f);
            }
```

기존 코드:

```cpp
            const bool_t bBlueTeam = minion.team == eTeam::Blue;
            void* pBarSRV = bBlueTeam ? m_pSRV_MinionBlueHPBar : m_pSRV_MinionRedHPBar;

            m_pRHIUIRenderer->DrawImage(
                nullptr,
                x,
                y,
                w,
                h,
                uvFull,
                Vec4(0.055f, 0.055f, 0.055f, 0.91f));

            if (fillW <= 0.5f)
                return;

            if (pBarSRV)
            {
                m_pRHIUIRenderer->DrawImage(
                    pBarSRV,
                    x,
                    y,
                    fillW,
                    h,
                    Vec4(0.f, 0.f, clamped, 1.f),
                    UI_WhiteVec());
            }
            else
            {
                m_pRHIUIRenderer->DrawImage(
                    nullptr,
                    x,
                    y,
                    fillW,
                    h,
                    uvFull,
                    bBlueTeam ? Vec4(0.22f, 0.58f, 1.0f, 1.0f) : Vec4(0.90f, 0.21f, 0.21f, 1.0f));
            }
```

아래로 교체:

```cpp
            const bool_t bBlueTeam = minion.team == eTeam::Blue;

            m_pRHIUIRenderer->DrawImage(
                nullptr,
                x,
                y,
                w,
                h,
                uvFull,
                Vec4(0.055f, 0.055f, 0.055f, 0.91f));

            if (fillW <= 0.5f)
                return;

            m_pRHIUIRenderer->DrawImage(
                nullptr,
                x,
                y,
                fillW,
                h,
                uvFull,
                bBlueTeam ? Vec4(0.19f, 0.52f, 0.88f, 1.0f) : Vec4(0.86f, 0.23f, 0.19f, 1.0f));
            m_pRHIUIRenderer->DrawImage(
                nullptr,
                x,
                y,
                fillW,
                1.0f,
                uvFull,
                Vec4(1.0f, 1.0f, 1.0f, 0.12f));
```

2. 검증

미검증:

- `git diff --check`
- `& 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\fxc.exe' /T ps_5_0 /E PS /Fo NUL Shaders/UIPlane.hlsl`
- `& 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\fxc.exe' /T vs_5_0 /E VS /Fo NUL Shaders/UIPlane.hlsl`
- `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`

수동 확인:

- F5 실행 전 `WintersGame.exe`가 떠 있으면 종료한다.
- A 키를 눌러 Attack Range 원이 `Mesh3D.hlsl`의 조명/SSAO/point light를 먹지 않는지 확인한다.
- Attack Range 원이 여전히 cyan인 경우, 그것은 원본 `UI_AttackRange.png`의 baked cyan 색이다. 이 세션의 목표는 월드 조명 오염 제거이며, 색 자체를 중립화하려면 다음 세션에서 alpha-mask/tint 리소스로 교체한다.
- 챔피언 체력바와 미니언 체력바가 PNG baked highlight에 끌려가지 않고 일정한 녹색/빨강/파랑 계열 fill로 보이는지 확인한다.
- 이후 Session 08 point light accent를 적용해도 A Range, turret projectile plane, 체력바가 더 밝아지거나 푸르게 변하지 않는지 확인한다.
