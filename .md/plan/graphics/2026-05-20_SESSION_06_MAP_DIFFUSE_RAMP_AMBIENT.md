Session - DX11 map diffuse ramp and ambient discipline

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shaders/Mesh3D.hlsl

기존 코드:

```cpp
float3 ApplyStylizedDiffuse(float3 baseColor, float3 normalWS, float3 worldPos, float ao)
{
    const float3 N = normalize(normalWS);
    const float3 L = normalize(-g_vLightDirWorld);
    const float3 V = normalize(g_vCameraWorld - worldPos);

    const float wrap = 0.45f;
    float key = saturate((dot(N, L) + wrap) / (1.0f + wrap));
    key = smoothstep(0.08f, 0.95f, key);

    const float top = saturate(N.y * 0.5f + 0.5f);
    const float rim = pow(1.0f - saturate(dot(N, V)), 3.5f) * smoothstep(0.10f, 0.75f, key);

    const float3 ambientLow = float3(0.36f, 0.42f, 0.62f);
    const float3 ambientHigh = float3(0.70f, 0.74f, 0.82f);
    const float3 shadowTint = float3(0.42f, 0.48f, 0.68f);
    const float3 keyTint = max(
        g_vLightColor * max(g_fLightIntensity, 0.001f),
        float3(0.001f, 0.001f, 0.001f));

    float3 color = baseColor * lerp(shadowTint, keyTint, key);
    color += baseColor * lerp(ambientLow, ambientHigh, top) * 0.48f;
    color += float3(0.52f, 0.70f, 1.00f) * rim * 0.22f;
    color *= lerp(0.86f, 1.08f, top);
    const float contactAO = saturate(ao);
    color *= lerp(0.52f, 1.f, contactAO);

    return saturate(color);
}
```

아래로 교체:

```cpp
float StylizedMapRamp(float ndotl)
{
    const float wrap = 0.34f;
    const float wrapped = saturate((ndotl + wrap) / (1.0f + wrap));
    const float soft = smoothstep(0.06f, 0.86f, wrapped);
    return lerp(soft * soft, soft, 0.55f);
}

float3 ApplyStylizedDiffuse(float3 baseColor, float3 normalWS, float3 worldPos, float ao)
{
    const float3 N = normalize(normalWS);
    const float3 L = normalize(-g_vLightDirWorld);
    const float3 V = normalize(g_vCameraWorld - worldPos);

    const float key = StylizedMapRamp(dot(N, L));
    const float top = saturate(N.y * 0.5f + 0.5f);
    const float grazing = pow(1.0f - saturate(dot(N, V)), 4.0f) * smoothstep(0.15f, 0.85f, key);

    const float3 ambientLow = float3(0.25f, 0.31f, 0.43f);
    const float3 ambientHigh = float3(0.48f, 0.54f, 0.62f);
    const float3 shadowTint = float3(0.32f, 0.39f, 0.55f);
    const float3 keyTint = max(
        g_vLightColor * max(g_fLightIntensity, 0.001f),
        float3(0.001f, 0.001f, 0.001f));

    float3 color = baseColor * lerp(shadowTint, keyTint, key);
    color += baseColor * lerp(ambientLow, ambientHigh, top) * 0.30f;
    color += float3(0.35f, 0.50f, 0.78f) * grazing * 0.06f;
    color *= lerp(0.90f, 1.04f, top);
    const float contactAO = saturate(ao);
    color *= lerp(0.52f, 1.f, contactAO);

    return saturate(color);
}
```

의도:
- 맵 전체를 씻어내던 ambient를 줄이고, shadow tint와 ramp로 큰 형태를 세운다.
- 맵에는 캐릭터처럼 강한 rim을 주지 않고, grazing accent만 아주 약하게 둔다.
- SSAO는 유지하되 전체 명암 도구가 아니라 접촉 그림자 도구로만 남긴다.

2. 검증

미검증:
- 맵 바닥 전체가 이전보다 덜 뿌옇고, 돌판 경계/고저차가 더 읽히는지 확인.
- 그림자 면이 검정으로 죽지 않고 청회색 계열로 남는지 확인.
- 캐릭터 셰이더는 아직 바꾸지 않았으므로 챔피언 가독성 평가는 다음 세션에서 한다.

검증 명령:
- `git diff --check`
- `fxc /T ps_5_0 /E PS /Fo NUL Shaders/Mesh3D.hlsl`
- `msbuild Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`

수동 확인:
- DX11 실행에서 F1 Render Debug `SSAO Reference` 기준으로 맵 톤 확인.
- 맵이 너무 어두우면 `ambientLow/ambientHigh`가 아니라 먼저 `shadowTint`와 `keyTint` 밸런스를 본다.
