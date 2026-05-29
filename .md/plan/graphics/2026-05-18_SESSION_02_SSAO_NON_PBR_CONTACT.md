Session - non-PBR diffuse 챔피언도 NormalPass/SSAO/contact AO를 받게 한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Engine/Private/Renderer/ModelRenderer.cpp

`ModelRenderer::RenderNormalPassWithVisibility` 안에서 아래 코드를:

```cpp
    if (!m_pImpl->bReady || !m_pImpl->pSharedModel || !m_pImpl->bUsePBR)
        return;
```

아래로 교체:

```cpp
    if (!m_pImpl->bReady || !m_pImpl->pSharedModel)
        return;
```

스키닝 렌더링 분기에서 아래 코드를:

```cpp
        if (m_pImpl->bUsePBR)
        {
            m_pImpl->cbPerFrame.Bind(pContext, 0);
            if (m_pImpl->pMaterialPBR)
                m_pImpl->pMaterialPBR->Bind(pDevice);
            if (pAmbientOcclusionSRV)
                pContext->PSSetShaderResources(5, 1, &pAmbientOcclusionSRV);
        }
        else
        {
            m_pImpl->cbPerFrame.Bind(pContext, 0);
        }
```

아래로 교체:

```cpp
        if (m_pImpl->bUsePBR)
        {
            m_pImpl->cbPerFrame.Bind(pContext, 0);
            if (m_pImpl->pMaterialPBR)
                m_pImpl->pMaterialPBR->Bind(pDevice);
        }
        else
        {
            m_pImpl->cbPerFrame.Bind(pContext, 0);
        }
        if (pAmbientOcclusionSRV)
            pContext->PSSetShaderResources(5, 1, &pAmbientOcclusionSRV);
```

정적 메시 렌더링 분기에서 아래 코드를:

```cpp
    if (m_pImpl->bUsePBR)
    {
        m_pImpl->cbPerFrame.Bind(pContext, 0);
        if (m_pImpl->pMaterialPBR)
            m_pImpl->pMaterialPBR->Bind(pDevice);
        if (pAmbientOcclusionSRV)
            pContext->PSSetShaderResources(5, 1, &pAmbientOcclusionSRV);
    }
    else
    {
        m_pImpl->cbPerFrame.Bind(pContext, 0);
    }
```

아래로 교체:

```cpp
    if (m_pImpl->bUsePBR)
    {
        m_pImpl->cbPerFrame.Bind(pContext, 0);
        if (m_pImpl->pMaterialPBR)
            m_pImpl->pMaterialPBR->Bind(pDevice);
    }
    else
    {
        m_pImpl->cbPerFrame.Bind(pContext, 0);
    }
    if (pAmbientOcclusionSRV)
        pContext->PSSetShaderResources(5, 1, &pAmbientOcclusionSRV);
```

1-2. C:/Users/user/Desktop/Winters/Client/Private/Scene/InGameRenderBridge.cpp

기존 코드:

```cpp
    if (pContext && scene.m_pNormalPass)
    {
        scene.m_pNormalPass->Begin(pDevice);

        scene.m_World.ForEach<ChampionComponent, RenderComponent, TransformComponent>(
```

아래로 교체:

```cpp
    if (pContext && scene.m_pNormalPass)
    {
        scene.m_pNormalPass->Begin(pDevice);

        scene.m_Map.UpdateCamera(vp, cameraWorld);
        scene.m_Map.UpdateTransform(scene.m_MapTransform.GetWorldMatrix());
        scene.m_Map.RenderNormalPass(
            scene.m_pNormalPass->GetStaticShader(),
            scene.m_pNormalPass->GetStaticPipeline(),
            scene.m_pNormalPass->GetSkinnedShader(),
            scene.m_pNormalPass->GetSkinnedPipeline());

        scene.m_World.ForEach<ChampionComponent, RenderComponent, TransformComponent>(
```

normal pass champion loop 안에서 아래 코드를:

```cpp
                if (rc.bSceneManaged || !rc.bVisible || !rc.pRenderer || !rc.pRenderer->UsesPBR())
                    return;
```

아래로 교체:

```cpp
                if (rc.bSceneManaged || !rc.bVisible || !rc.pRenderer)
                    return;
```

champion main render loop 안에서 아래 코드를:

```cpp
                rc.pRenderer->SetAmbientOcclusionSRV(
                    rc.pRenderer->UsesPBR() ? pAmbientOcclusionSRV : nullptr);
```

아래로 교체:

```cpp
                rc.pRenderer->SetAmbientOcclusionSRV(pAmbientOcclusionSRV);
```

1-3. C:/Users/user/Desktop/Winters/Client/Private/Scene/InGameBootstrapBridge.cpp

기존 코드:

```cpp
            scene.m_pSSAOPass = Engine::CSSAOPass::Create(pRhiDevice, g_iWinSizeX, g_iWinSizeY);
            if (scene.m_pSSAOPass)
                scene.m_pSSAOPass->SetEnabled(false);
            scene.m_pFogOfWarRenderer = CFogOfWarRenderer::Create(
```

아래로 교체:

```cpp
            scene.m_pSSAOPass = Engine::CSSAOPass::Create(pRhiDevice, g_iWinSizeX, g_iWinSizeY);
            if (scene.m_pSSAOPass)
            {
                scene.m_pSSAOPass->SetEnabled(true);
                scene.m_pSSAOPass->SetRadius(1.10f);
                scene.m_pSSAOPass->SetIntensity(1.25f);
            }
            scene.m_pFogOfWarRenderer = CFogOfWarRenderer::Create(
```

1-4. C:/Users/user/Desktop/Winters/Shaders/Mesh3D.hlsl

기존 코드:

```hlsl
Texture2D g_DiffuseMap : register(t0);
SamplerState g_Sampler : register(s0);
```

아래로 교체:

```hlsl
Texture2D g_DiffuseMap : register(t0);
Texture2D g_AmbientOcclusionMap : register(t5);
SamplerState g_Sampler : register(s0);
```

기존 코드:

```hlsl
float3 ApplyStylizedDiffuse(float3 baseColor, float3 normalWS, float3 worldPos)
{
```

아래로 교체:

```hlsl
float SampleScreenAO(float4 screenPos)
{
    const float2 screenSize = max(g_vScreenSize, float2(1.0f, 1.0f));
    const float2 uv = saturate(screenPos.xy / screenSize);
    return g_AmbientOcclusionMap.SampleLevel(g_Sampler, uv, 0).r;
}

float3 ApplyStylizedDiffuse(float3 baseColor, float3 normalWS, float3 worldPos, float ao)
{
```

`ApplyStylizedDiffuse` 안에서 아래 코드를:

```hlsl
    color *= lerp(0.86f, 1.08f, top);

    return saturate(color);
}
```

아래로 교체:

```hlsl
    color *= lerp(0.86f, 1.08f, top);
    color *= lerp(0.62f, 1.00f, saturate(ao));

    return saturate(color);
}
```

기존 코드:

```hlsl
    const float3 color = ApplyStylizedDiffuse(texColor.rgb, normalize(input.vNormal), input.vWorldPos);
    return float4(color, texColor.a);
```

아래로 교체:

```hlsl
    const float ao = SampleScreenAO(input.vPosition);
    const float3 color = ApplyStylizedDiffuse(texColor.rgb, normalize(input.vNormal), input.vWorldPos, ao);
    return float4(color, texColor.a);
```

1-5. C:/Users/user/Desktop/Winters/Shaders/Skinned3D.hlsl

기존 코드:

```hlsl
Texture2D g_DiffuseMap : register(t0);
SamplerState g_Sampler : register(s0);
```

아래로 교체:

```hlsl
Texture2D g_DiffuseMap : register(t0);
Texture2D g_AmbientOcclusionMap : register(t5);
SamplerState g_Sampler : register(s0);
```

기존 코드:

```hlsl
float3 ApplyStylizedDiffuse(float3 baseColor, float3 normalWS, float3 worldPos)
{
```

아래로 교체:

```hlsl
float SampleScreenAO(float4 screenPos)
{
    const float2 screenSize = max(g_vScreenSize, float2(1.0f, 1.0f));
    const float2 uv = saturate(screenPos.xy / screenSize);
    return g_AmbientOcclusionMap.SampleLevel(g_Sampler, uv, 0).r;
}

float3 ApplyStylizedDiffuse(float3 baseColor, float3 normalWS, float3 worldPos, float ao)
{
```

`ApplyStylizedDiffuse` 안에서 아래 코드를:

```hlsl
    color *= lerp(0.86f, 1.08f, top);

    return saturate(color);
}
```

아래로 교체:

```hlsl
    color *= lerp(0.86f, 1.08f, top);
    color *= lerp(0.62f, 1.00f, saturate(ao));

    return saturate(color);
}
```

기존 코드:

```hlsl
    const float3 color = ApplyStylizedDiffuse(texColor.rgb, normalize(input.vNormal), input.vWorldPos);
    return float4(color, texColor.a);
```

아래로 교체:

```hlsl
    const float ao = SampleScreenAO(input.vPosition);
    const float3 color = ApplyStylizedDiffuse(texColor.rgb, normalize(input.vNormal), input.vWorldPos, ao);
    return float4(color, texColor.a);
```

2. 검증

미검증:
- 빌드 미검증
- SSAO compute dispatch 런타임 비용 미측정
- map depth를 normal pass에 넣은 뒤 발밑 contact AO 품질 미검증

검증 명령:
- git diff --check
- msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64

수동 확인:
- 일반 F5 flow에서 roster/map/minion/snapshot/champion 시스템을 숨기지 않고 확인.
- SSAO on/off screenshot을 비교해 챔피언 내부 접힘과 발밑 접지가 어두워지는지 확인.
- fog of war, HUD, FX sprite가 SSAO SRV 바인딩 뒤 깨지지 않는지 확인.
