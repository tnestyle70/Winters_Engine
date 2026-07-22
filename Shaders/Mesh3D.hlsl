struct PointLightData
{
    float3 vPosition;
    float fRadius;
    float3 vColor;
    float fIntensity;
};

cbuffer CBPerFrame : register(b0)
{
    row_major matrix g_matViewProj;
    float3 g_vCameraWorld;
    float g_fFramePad0;
    float3 g_vLightDirWorld;
    float g_fLightIntensity;
    float3 g_vLightColor;
    uint g_iPointLightCount;
    PointLightData g_PointLights[4];
    float2 g_vScreenSize;
    float2 g_vFramePad1;
};

cbuffer CBPerObject : register(b1)
{
    row_major matrix g_matWorld;
    row_major matrix g_matWorldInvTranspose;
    float4 g_vMaterialOverrideColor;
    float4 g_vMaterialOverrideParams;
    uint g_bUseGrassTint;
    uint3 g_vObjectPad;
};

Texture2D g_DiffuseMap : register(t0);
Texture2D g_AmbientOcclusionMap : register(t5);
Texture2D g_GrassTintMap : register(t6);
SamplerState g_Sampler : register(s0);
SamplerState g_GrassTintSampler : register(s6);

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
    float3 vNormal : NORMAL;
    float2 vTexCoord : TEXCOORD0;
    float3 vWorldPos : TEXCOORD1;
    float2 vGrassTintUV : TEXCOORD2;
};

float2 ResolveGrassTintUV(float3 localPosition)
{
    // Map11 foliage vertices are authored in the Riot map frame:
    // X=[0,15000], Z=[-15000,0]. GrassTint_SRX uses that full-map projection.
    return saturate(float2(localPosition.x, -localPosition.z) / 15000.f);
}

float3 SrgbToLinearApprox(float3 color)
{
    return pow(saturate(color), float3(2.2f, 2.2f, 2.2f));
}

float3 LinearToSrgbApprox(float3 color)
{
    return pow(saturate(color), float3(1.f / 2.2f, 1.f / 2.2f, 1.f / 2.2f));
}

float SampleScreenAO(float4 screenPos)
{
    const float2 screenSize = max(g_vScreenSize, float2(1.f, 1.f));
    const float2 uv = saturate(screenPos.xy / screenSize);
    return g_AmbientOcclusionMap.SampleLevel(g_Sampler, uv, 0).r;
}

float StylizedMapRamp(float ndotl)
{
    const float wrap = 0.34f;
    const float wrapped = saturate((ndotl + wrap) / (1.0f + wrap));
    const float soft = smoothstep(0.06f, 0.86f, wrapped);
    return lerp(soft * soft, soft, 0.55f);
}

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
    color += EvaluatePointLightAccent(baseColor, N, worldPos);
    color *= lerp(0.90f, 1.04f, top);
    const float contactAO = saturate(ao);
    color *= lerp(0.52f, 1.f, contactAO);

    return saturate(color);
}

float3 ApplyHoverOutline(float3 color, float3 normalWS, float3 worldPos)
{
    const float intensity = saturate(g_vMaterialOverrideParams.a);
    if (intensity <= 0.001f)
        return color;

    const float3 N = normalize(normalWS);
    const float3 V = normalize(g_vCameraWorld - worldPos);
    const float rim = pow(1.0f - saturate(dot(N, V)), 2.35f);
    const float mask = smoothstep(0.28f, 0.88f, rim) * intensity;
    return saturate(lerp(color, saturate(g_vMaterialOverrideParams.rgb), mask));
}

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;

    const float4 worldPos = mul(float4(input.vPosition, 1.0f), g_matWorld);
    output.vPosition = mul(worldPos, g_matViewProj);
    output.vNormal = normalize(mul(float4(input.vNormal, 0.0f), g_matWorldInvTranspose).xyz);
    output.vTexCoord = input.vTexCoord;
    output.vWorldPos = worldPos.xyz;
    output.vGrassTintUV = ResolveGrassTintUV(input.vPosition);

    return output;
}

float4 PS(PS_INPUT input) : SV_TARGET
{
    float4 texColor = g_DiffuseMap.Sample(g_Sampler, input.vTexCoord);
    if (g_bUseGrassTint != 0u)
    {
        const float3 grassTint =
            g_GrassTintMap.Sample(g_GrassTintSampler, input.vGrassTintUV).rgb;
        texColor.rgb = saturate(texColor.rgb * grassTint * 2.f);
    }
    clip(texColor.a - 0.05f);
    if (g_vMaterialOverrideColor.a >= 0.999f)
        return float4(g_vMaterialOverrideColor.rgb, texColor.a);

    const float ao = SampleScreenAO(input.vPosition);
    const float3 baseLinear = SrgbToLinearApprox(texColor.rgb);
    const float3 colorLinear = ApplyStylizedDiffuse(baseLinear, normalize(input.vNormal),
        input.vWorldPos, ao);
    const float3 outlinedLinear = ApplyHoverOutline(colorLinear, normalize(input.vNormal),
        input.vWorldPos);
    const float3 outlinedSrgb = LinearToSrgbApprox(outlinedLinear);
    if (g_vMaterialOverrideColor.a > 0.001f)
    {
        const float3 tinted = saturate(
            outlinedSrgb * saturate(g_vMaterialOverrideColor.rgb));
        return float4(
            tinted,
            texColor.a * saturate(g_vMaterialOverrideColor.a));
    }
    return float4(outlinedSrgb, texColor.a);
}
