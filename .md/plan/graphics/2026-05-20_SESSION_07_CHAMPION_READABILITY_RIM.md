Session - DX11 champion readability rim and top light

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shaders/Skinned3D.hlsl

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
    color *= lerp(0.52f, 1.00f, contactAO);

    return saturate(color);
}
```

아래로 교체:

```cpp
float StylizedChampionRamp(float ndotl)
{
    const float wrap = 0.42f;
    const float wrapped = saturate((ndotl + wrap) / (1.0f + wrap));
    const float soft = smoothstep(0.04f, 0.90f, wrapped);
    return lerp(soft * 0.78f, soft, soft);
}

float3 ApplyStylizedDiffuse(float3 baseColor, float3 normalWS, float3 worldPos, float ao)
{
    const float3 N = normalize(normalWS);
    const float3 L = normalize(-g_vLightDirWorld);
    const float3 V = normalize(g_vCameraWorld - worldPos);

    const float key = StylizedChampionRamp(dot(N, L));
    const float top = saturate(N.y * 0.5f + 0.5f);
    const float rim = pow(1.0f - saturate(dot(N, V)), 3.0f) * smoothstep(0.08f, 0.82f, key);
    const float topLight = pow(top, 1.65f);

    const float3 ambientLow = float3(0.30f, 0.35f, 0.55f);
    const float3 ambientHigh = float3(0.55f, 0.61f, 0.76f);
    const float3 shadowTint = float3(0.35f, 0.40f, 0.64f);
    const float3 keyTint = max(
        g_vLightColor * max(g_fLightIntensity * 1.08f, 0.001f),
        float3(0.001f, 0.001f, 0.001f));

    float3 color = baseColor * lerp(shadowTint, keyTint, key);
    color += baseColor * lerp(ambientLow, ambientHigh, top) * 0.34f;
    color += baseColor * float3(0.70f, 0.82f, 1.00f) * topLight * 0.10f;
    color += float3(0.48f, 0.68f, 1.00f) * rim * 0.26f;
    color *= lerp(0.88f, 1.10f, top);
    const float contactAO = saturate(ao);
    color *= lerp(0.58f, 1.00f, contactAO);

    return saturate(color);
}
```

의도:
- 챔피언은 맵보다 더 읽혀야 하므로 rim/top light를 분리해서 실루엣을 세운다.
- 챔피언 AO는 맵보다 약하게 적용해 몸통이 탁해지는 것을 막는다.
- diffuse-only 원칙은 유지하고 roughness/metallic/PBR로 가지 않는다.

2. 검증

미검증:
- 챔피언 머리/어깨/무기 윤곽이 맵에서 더 분리되는지 확인.
- 흰 머리/밝은 스킨이 과노출되지 않는지 확인.
- 그림자 면이 검정이 아니라 보라/청색 계열로 남는지 확인.

검증 명령:
- `git diff --check`
- `fxc /T ps_5_0 /E PS /Fo NUL Shaders/Skinned3D.hlsl`
- `msbuild Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`

수동 확인:
- Irelia/Yasuo/Ashe처럼 머리색과 무기 색이 다른 챔피언으로 비교.
- 맵 위에서 줌을 당겼을 때 실루엣이 살아나는지 확인.
