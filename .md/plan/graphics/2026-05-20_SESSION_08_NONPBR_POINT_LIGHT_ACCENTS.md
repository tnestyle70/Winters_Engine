Session - DX11 non-PBR point light accents

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shaders/Mesh3D.hlsl

`StylizedMapRamp` 바로 아래, `ApplyStylizedDiffuse` 바로 위에 아래 코드를 추가한다.

기존 코드:

```cpp
float StylizedMapRamp(float ndotl)
{
    const float wrap = 0.34f;
    const float wrapped = saturate((ndotl + wrap) / (1.0f + wrap));
    const float soft = smoothstep(0.06f, 0.86f, wrapped);
    return lerp(soft * soft, soft, 0.55f);
}

float3 ApplyStylizedDiffuse(float3 baseColor, float3 normalWS, float3 worldPos, float ao)
```

아래에 추가:

```cpp
float3 EvaluatePointLightAccent(float3 baseColor, float3 normalWS, float3 worldPos)
{
    const float3 N = normalize(normalWS);
    float3 accum = float3(0.0f, 0.0f, 0.0f);
    const uint lightCount = min(g_iPointLightCount, 4u);

    [unroll]
    for (uint i = 0u; i < 4u; ++i)
    {
        const float active = (i < lightCount) ? 1.0f : 0.0f;
        const float3 toLight = g_PointLights[i].vPosition - worldPos;
        const float distanceToLight = length(toLight);
        const float radius = max(g_PointLights[i].fRadius, 0.001f);
        const float attenuation = saturate(1.0f - distanceToLight / radius);
        const float3 Lp = toLight / max(distanceToLight, 0.001f);
        const float facing = smoothstep(0.05f, 0.95f,
            saturate(dot(N, Lp) * 0.5f + 0.5f));

        accum += baseColor * g_PointLights[i].vColor *
            (g_PointLights[i].fIntensity * attenuation * attenuation * facing * active * 0.08f);
    }

    return accum;
}
```

`ApplyStylizedDiffuse` 내부에서 아래 코드를 교체한다.

기존 코드:

```cpp
    color += baseColor * lerp(ambientLow, ambientHigh, top) * 0.30f;
    color += float3(0.35f, 0.50f, 0.78f) * grazing * 0.06f;
    color *= lerp(0.90f, 1.04f, top);
    const float contactAO = saturate(ao);
```

아래로 교체:

```cpp
    color += baseColor * lerp(ambientLow, ambientHigh, top) * 0.30f;
    color += float3(0.35f, 0.50f, 0.78f) * grazing * 0.06f;
    color += EvaluatePointLightAccent(baseColor, N, worldPos);
    color *= lerp(0.90f, 1.04f, top);
    const float contactAO = saturate(ao);
```

1-2. C:/Users/user/Desktop/Winters/Shaders/Skinned3D.hlsl

`StylizedChampionRamp` 바로 아래, `ApplyStylizedDiffuse` 바로 위에 아래 코드를 추가한다.

기존 코드:

```cpp
float StylizedChampionRamp(float ndotl)
{
    const float wrap = 0.42f;
    const float wrapped = saturate((ndotl + wrap) / (1.0f + wrap));
    const float soft = smoothstep(0.04f, 0.90f, wrapped);
    return lerp(soft * 0.78f, soft, soft);
}

float3 ApplyStylizedDiffuse(float3 baseColor, float3 normalWS, float3 worldPos, float ao)
```

아래에 추가:

```cpp
float3 EvaluatePointLightAccent(float3 baseColor, float3 normalWS, float3 worldPos)
{
    const float3 N = normalize(normalWS);
    float3 accum = float3(0.0f, 0.0f, 0.0f);
    const uint lightCount = min(g_iPointLightCount, 4u);

    [unroll]
    for (uint i = 0u; i < 4u; ++i)
    {
        const float active = (i < lightCount) ? 1.0f : 0.0f;
        const float3 toLight = g_PointLights[i].vPosition - worldPos;
        const float distanceToLight = length(toLight);
        const float radius = max(g_PointLights[i].fRadius, 0.001f);
        const float attenuation = saturate(1.0f - distanceToLight / radius);
        const float3 Lp = toLight / max(distanceToLight, 0.001f);
        const float facing = smoothstep(0.05f, 0.95f,
            saturate(dot(N, Lp) * 0.5f + 0.5f));

        accum += baseColor * g_PointLights[i].vColor *
            (g_PointLights[i].fIntensity * attenuation * attenuation * facing * active * 0.10f);
    }

    return accum;
}
```

`ApplyStylizedDiffuse` 내부에서 아래 코드를 교체한다.

기존 코드:

```cpp
    color += baseColor * lerp(ambientLow, ambientHigh, top) * 0.34f;
    color += baseColor * float3(0.70f, 0.82f, 1.00f) * topLight * 0.10f;
    color += float3(0.48f, 0.68f, 1.00f) * rim * 0.26f;
    color *= lerp(0.88f, 1.10f, top);
```

아래로 교체:

```cpp
    color += baseColor * lerp(ambientLow, ambientHigh, top) * 0.34f;
    color += baseColor * float3(0.70f, 0.82f, 1.00f) * topLight * 0.10f;
    color += EvaluatePointLightAccent(baseColor, N, worldPos);
    color += float3(0.48f, 0.68f, 1.00f) * rim * 0.26f;
    color *= lerp(0.88f, 1.10f, top);
```

1-3. C:/Users/user/Desktop/Winters/Engine/Private/Renderer/ModelRenderer.cpp

`CModelRenderer::UpdateCamera` 내부의 point light 기본값을 아래처럼 교체한다. 이 세션은 Forward+ 구현이 아니라 기존 4개 상수 point light 슬롯을 non-PBR diffuse accent로 약하게 쓰는 단계다.

기존 코드:

```cpp
    data.pointLightCount = Engine::kMaxForwardPointLights;
    data.pointLights[0] = { { -4.0f, 5.0f,  1.5f }, 10.0f, { 1.00f, 0.45f, 0.35f }, 4.0f };
    data.pointLights[1] = { {  4.0f, 5.0f, -1.5f }, 10.0f, { 0.35f, 0.55f, 1.00f }, 4.0f };
    data.pointLights[2] = { { 12.0f, 5.0f,  2.0f }, 12.0f, { 0.40f, 1.00f, 0.55f }, 3.5f };
    data.pointLights[3] = { { 20.0f, 5.0f,  0.0f }, 12.0f, { 1.00f, 0.85f, 0.30f }, 3.5f };
```

아래로 교체:

```cpp
    data.pointLightCount = 2u;
    data.pointLights[0] = { { -4.0f, 5.0f,  1.5f }, 9.0f, { 0.95f, 0.44f, 0.34f }, 1.6f };
    data.pointLights[1] = { {  4.0f, 5.0f, -1.5f }, 9.0f, { 0.34f, 0.52f, 1.00f }, 1.6f };
    data.pointLights[2] = { { 12.0f, 5.0f,  2.0f }, 12.0f, { 0.40f, 1.00f, 0.55f }, 0.0f };
    data.pointLights[3] = { { 20.0f, 5.0f,  0.0f }, 12.0f, { 1.00f, 0.85f, 0.30f }, 0.0f };
```

2. 검증

미검증:

- `git diff --check -- Shaders/Mesh3D.hlsl Shaders/Skinned3D.hlsl Engine/Private/Renderer/ModelRenderer.cpp`
- `& 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\fxc.exe' /T ps_5_0 /E PS /Fo NUL Shaders/Mesh3D.hlsl`
- `& 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\fxc.exe' /T vs_5_0 /E VS /Fo NUL Shaders/Mesh3D.hlsl`
- `& 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\fxc.exe' /T ps_5_0 /E PS /Fo NUL Shaders/Skinned3D.hlsl`
- `& 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\fxc.exe' /T vs_5_0 /E VS /Fo NUL Shaders/Skinned3D.hlsl`
- `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`

수동 확인:

- F5 실행 전에 `WintersGame.exe`가 이미 떠 있으면 링크가 실패하므로 먼저 종료한다.
- 같은 카메라, 같은 맵, 같은 챔피언 기준으로 Session 07/08A 결과와 비교한다.
- 맵 전체가 밝아지면 실패다. 무기, 장비, 챔피언 외곽, 일부 지형 모서리에 약한 warm/cool 색 분리만 생겨야 한다.
- 챔피언 피부나 흰 머리가 과노출되면 `ModelRenderer::UpdateCamera`의 `pointLights[*].fIntensity`를 먼저 낮춘다.
- A Range, 체력바, 미니언 체력바가 다시 파란색/조명 먹은 PNG처럼 보이면 Session 08A UI 분리 패치가 먼저 깨진 것이므로 이 세션 값을 조정하지 않는다.
