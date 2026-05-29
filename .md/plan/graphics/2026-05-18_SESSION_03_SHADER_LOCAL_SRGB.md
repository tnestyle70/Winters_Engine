Session - diffuse-only shader 안에서 sRGB decode/encode를 적용해 색 계산을 안정화한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shaders/Mesh3D.hlsl

기존 코드:

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

아래로 교체:

```hlsl
float3 SrgbToLinearApprox(float3 color)
{
    return pow(saturate(color), float3(2.2f, 2.2f, 2.2f));
}

float3 LinearToSrgbApprox(float3 color)
{
    return pow(saturate(color), float3(1.0f / 2.2f, 1.0f / 2.2f, 1.0f / 2.2f));
}

float SampleScreenAO(float4 screenPos)
{
    const float2 screenSize = max(g_vScreenSize, float2(1.0f, 1.0f));
    const float2 uv = saturate(screenPos.xy / screenSize);
    return g_AmbientOcclusionMap.SampleLevel(g_Sampler, uv, 0).r;
}

float3 ApplyStylizedDiffuse(float3 baseColor, float3 normalWS, float3 worldPos, float ao)
{
```

기존 코드:

```hlsl
    const float ao = SampleScreenAO(input.vPosition);
    const float3 color = ApplyStylizedDiffuse(texColor.rgb, normalize(input.vNormal), input.vWorldPos, ao);
    return float4(color, texColor.a);
```

아래로 교체:

```hlsl
    const float ao = SampleScreenAO(input.vPosition);
    const float3 baseLinear = SrgbToLinearApprox(texColor.rgb);
    const float3 colorLinear = ApplyStylizedDiffuse(baseLinear, normalize(input.vNormal), input.vWorldPos, ao);
    return float4(LinearToSrgbApprox(colorLinear), texColor.a);
```

1-2. C:/Users/user/Desktop/Winters/Shaders/Skinned3D.hlsl

기존 코드:

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

아래로 교체:

```hlsl
float3 SrgbToLinearApprox(float3 color)
{
    return pow(saturate(color), float3(2.2f, 2.2f, 2.2f));
}

float3 LinearToSrgbApprox(float3 color)
{
    return pow(saturate(color), float3(1.0f / 2.2f, 1.0f / 2.2f, 1.0f / 2.2f));
}

float SampleScreenAO(float4 screenPos)
{
    const float2 screenSize = max(g_vScreenSize, float2(1.0f, 1.0f));
    const float2 uv = saturate(screenPos.xy / screenSize);
    return g_AmbientOcclusionMap.SampleLevel(g_Sampler, uv, 0).r;
}

float3 ApplyStylizedDiffuse(float3 baseColor, float3 normalWS, float3 worldPos, float ao)
{
```

기존 코드:

```hlsl
    const float ao = SampleScreenAO(input.vPosition);
    const float3 color = ApplyStylizedDiffuse(texColor.rgb, normalize(input.vNormal), input.vWorldPos, ao);
    return float4(color, texColor.a);
```

아래로 교체:

```hlsl
    const float ao = SampleScreenAO(input.vPosition);
    const float3 baseLinear = SrgbToLinearApprox(texColor.rgb);
    const float3 colorLinear = ApplyStylizedDiffuse(baseLinear, normalize(input.vNormal), input.vWorldPos, ao);
    return float4(LinearToSrgbApprox(colorLinear), texColor.a);
```

2. 검증

미검증:
- 빌드 미검증
- PNG가 이미 SRGB SRV로 들어오는 경우 double-decode 가능성 미검증
- 전체 swapchain/RTV를 SRGB로 바꾸는 구조 변경은 미적용

검증 명령:
- git diff --check
- msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64

수동 확인:
- Annie/Ashe처럼 512 diffuse인 챔피언과 Yone/Jax처럼 1024 diffuse인 챔피언을 나눠 비교.
- 피부/천/금속 페인팅 영역이 과하게 어두워지면 이 세션만 되돌리고 `CTexture` SRGB SRV 경로를 별도 계획으로 작성.
