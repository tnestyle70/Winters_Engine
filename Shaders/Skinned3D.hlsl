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
};

// 1024+ bone rigs (Elden Ring bosses): bone palette moved from a cbuffer to a
// structured buffer SRV. row_major preserves the old CBBones memory layout.
struct SkinBoneMatrix
{
    row_major float4x4 m;
};
StructuredBuffer<SkinBoneMatrix> g_BoneMatrices : register(t8);

Texture2D g_DiffuseMap : register(t0);
Texture2D g_AmbientOcclusionMap : register(t5);
SamplerState g_Sampler : register(s0);

struct VS_INPUT
{
    float3 vPosition : POSITION;
    float3 vNormal : NORMAL;
    float2 vTexCoord : TEXCOORD0;
    float3 vTangent : TANGENT;
    uint4 iBoneIndices : BLENDINDICES;
    float4 fBoneWeights : BLENDWEIGHT;
};

struct PS_INPUT
{
    float4 vPosition : SV_POSITION;
    float3 vNormal : NORMAL;
    float2 vTexCoord : TEXCOORD0;
    float3 vWorldPos : TEXCOORD1;
};

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

float StylizedChampionRamp(float ndotl)
{
    const float wrap = 0.42f;
    const float wrapped = saturate((ndotl + wrap) / (1.0f + wrap));
    const float soft = smoothstep(0.04f, 0.90f, wrapped);
    return lerp(soft * 0.78f, soft, soft);
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
            (g_PointLights[i].fIntensity * attenuation * attenuation * facing * active * 0.075f);
    }

    return accum;
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
    color += baseColor * float3(0.70f, 0.82f, 1.00f) * topLight * 0.075f;
    color += EvaluatePointLightAccent(baseColor, N, worldPos);
    color += float3(0.48f, 0.68f, 1.00f) * rim * 0.21f;
    color *= lerp(0.88f, 1.06f, top);
    const float contactAO = saturate(ao);
    color *= lerp(0.58f, 1.00f, contactAO);

    return saturate(color);
}

float3 ApplyHoverOutline(float3 color, float3 normalWS, float3 worldPos)
{
    const float intensity = saturate(g_vMaterialOverrideParams.a);
    if (intensity <= 0.001f)
        return color;

    const float3 N = normalize(normalWS);
    const float3 V = normalize(g_vCameraWorld - worldPos);
    const float rim = pow(1.0f - saturate(dot(N, V)), 2.15f);
    const float mask = smoothstep(0.24f, 0.86f, rim) * intensity;
    return saturate(lerp(color, saturate(g_vMaterialOverrideParams.rgb), mask));
}

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;
    const matrix skinMatrix =
        g_BoneMatrices[input.iBoneIndices.x].m * input.fBoneWeights.x +
        g_BoneMatrices[input.iBoneIndices.y].m * input.fBoneWeights.y +
        g_BoneMatrices[input.iBoneIndices.z].m * input.fBoneWeights.z +
        g_BoneMatrices[input.iBoneIndices.w].m * input.fBoneWeights.w;

    const float4 skinned = mul(float4(input.vPosition, 1.0f), skinMatrix);
    const float3 skinnedNormal = mul(float4(input.vNormal, 0.0f), skinMatrix).xyz;
    const float4 worldPos = mul(skinned, g_matWorld);

    output.vPosition = mul(worldPos, g_matViewProj);
    output.vNormal = normalize(mul(float4(skinnedNormal, 0.0f), g_matWorldInvTranspose).xyz);
    output.vTexCoord = input.vTexCoord;
    output.vWorldPos = worldPos.xyz;

    return output;
}

float4 PS(PS_INPUT input) : SV_TARGET
{
    const float4 texColor = g_DiffuseMap.Sample(g_Sampler, input.vTexCoord);
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
