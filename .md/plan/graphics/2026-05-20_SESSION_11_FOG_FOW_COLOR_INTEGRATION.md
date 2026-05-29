Session - DX11 fog of war color integration

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shaders/FogOfWarWorld.hlsl

기존 코드:

```cpp
cbuffer FogWorldCB : register(b0)
{
    row_major matrix g_matViewProj;
    float4 g_vWorldRect;
    float4 g_vFogParams;
};
```

아래로 교체:

```cpp
cbuffer FogWorldCB : register(b0)
{
    row_major matrix g_matViewProj;
    float4 g_vWorldRect;
    float4 g_vFogParams;
    float4 g_vUnexploredColor;
    float4 g_vExploredColor;
};
```

기존 코드:

```cpp
float4 PS(PS_INPUT input) : SV_TARGET
{
    float2 uv = (input.vWorldPos.xz - g_vWorldRect.xy) / g_vWorldRect.zw;

    if (uv.x < 0.f || uv.x > 1.f || uv.y < 0.f || uv.y > 1.f)
        return float4(0.f, 0.f, 0.f, g_vFogParams.x);

    float fog = g_FogTexture.Sample(g_Sampler, uv).r;
    float explored = smoothstep(0.02f, 0.55f, fog);
    float visible = smoothstep(g_vFogParams.z, 1.f, fog);
    float alpha = lerp(g_vFogParams.x, g_vFogParams.y, explored);
    alpha = lerp(alpha, 0.f, visible);

    return float4(0.f, 0.f, 0.f, alpha);
}
```

아래로 교체:

```cpp
float4 PS(PS_INPUT input) : SV_TARGET
{
    const float2 uv = (input.vWorldPos.xz - g_vWorldRect.xy) / g_vWorldRect.zw;

    if (uv.x < 0.f || uv.x > 1.f || uv.y < 0.f || uv.y > 1.f)
        return float4(g_vUnexploredColor.rgb, g_vFogParams.x);

    const float fog = g_FogTexture.Sample(g_Sampler, uv).r;
    const float explored = smoothstep(0.02f, 0.55f, fog);
    const float visible = smoothstep(g_vFogParams.z, 1.f, fog);
    float alpha = lerp(g_vFogParams.x, g_vFogParams.y, explored);
    alpha = lerp(alpha, 0.f, visible);

    const float3 fogColor = lerp(
        g_vUnexploredColor.rgb,
        g_vExploredColor.rgb,
        explored);

    return float4(fogColor, alpha);
}
```

1-2. C:/Users/user/Desktop/Winters/Engine/Private/Renderer/FogOfWarRenderer.cpp

기존 코드:

```cpp
    struct FogWorldCB
    {
        DirectX::XMFLOAT4X4 matViewProj;
        DirectX::XMFLOAT4 vWorldRect;
        DirectX::XMFLOAT4 vFogParams;
    };
```

아래로 교체:

```cpp
    struct FogWorldCB
    {
        DirectX::XMFLOAT4X4 matViewProj;
        DirectX::XMFLOAT4 vWorldRect;
        DirectX::XMFLOAT4 vFogParams;
        DirectX::XMFLOAT4 vUnexploredColor;
        DirectX::XMFLOAT4 vExploredColor;
    };
```

기존 코드:

```cpp
    cb.matViewProj = matViewProj.m;
    cb.vWorldRect = DirectX::XMFLOAT4(-half, -half, fWorldSize, fWorldSize);
    cb.vFogParams = DirectX::XMFLOAT4(0.72f, 0.35f, 0.50f, 0.f);
    std::memcpy(mapped.pData, &cb, sizeof(cb));
```

아래로 교체:

```cpp
    cb.matViewProj = matViewProj.m;
    cb.vWorldRect = DirectX::XMFLOAT4(-half, -half, fWorldSize, fWorldSize);
    cb.vFogParams = DirectX::XMFLOAT4(0.64f, 0.28f, 0.50f, 0.f);
    cb.vUnexploredColor = DirectX::XMFLOAT4(0.026f, 0.038f, 0.035f, 1.f);
    cb.vExploredColor = DirectX::XMFLOAT4(0.070f, 0.088f, 0.076f, 1.f);
    std::memcpy(mapped.pData, &cb, sizeof(cb));
```

2. 검증

미검증.

- `git diff --check -- Shaders/FogOfWarWorld.hlsl Engine/Private/Renderer/FogOfWarRenderer.cpp`
- `& 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\fxc.exe' /T ps_5_0 /E PS /Fo NUL Shaders/FogOfWarWorld.hlsl`
- `& 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\fxc.exe' /T vs_5_0 /E VS /Fo NUL Shaders/FogOfWarWorld.hlsl`
- `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Engine/Include/Engine.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`
- `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`
- F5에서 Fog of War가 검정 필름처럼 보이지 않고 맵 팔레트에 붙는지 확인한다.
- 챔피언 실루엣과 HP bar가 Fog/FOW 레이어 때문에 묻히면 실패다.
