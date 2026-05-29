#include "BRDF/BRDF_GGX.hlsli"

#define MAX_FORWARD_POINT_LIGHTS 4

struct PointLightData
{
    float3 positionWorld;
    float  radius;
    float3 color;
    float  intensity;
};

cbuffer PerFrame : register(b0)
{
    row_major matrix g_matViewProj;
    float3 g_vCameraWorld;
    float  g_fPerFramePad0;
    float3 g_vLightDirWorld;
    float  g_fLightIntensity;
    float3 g_vLightColor;
    uint   g_uPointLightCount;
    PointLightData g_PointLights[MAX_FORWARD_POINT_LIGHTS];
    float2 g_vScreenSize;
    float2 g_vPerFramePad1;
};

cbuffer PerObject : register(b1)
{
    row_major matrix g_matWorld;
    row_major matrix g_matWorldInvTranspose;
};

cbuffer PerMaterial : register(b3)
{
    float3 g_vAlbedoTint;
    float  g_fMetallic;
    float  g_fRoughness;
    float  g_fAmbientOcclusion;
    float  g_fEmissiveIntensity;
    float  g_fMaterialPad0;
    float3 g_vEmissiveTint;
    float  g_fMaterialPad1;
};

Texture2D g_AlbedoMap : register(t0);
Texture2D g_AmbientOcclusionMap : register(t5);
SamplerState g_LinearWrap : register(s0);

struct VS_INPUT
{
    float3 vPosition : POSITION;
    float3 vNormal   : NORMAL;
    float2 vTexCoord : TEXCOORD0;
    float3 vTangent  : TANGENT;
};

struct PS_INPUT
{
    float4 vPosition : SV_POSITION;
    float3 vWorldPos : TEXCOORD0;
    float3 vNormal   : TEXCOORD1;
    float2 vTexCoord : TEXCOORD2;
};

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;

    float4 worldPos = mul(float4(input.vPosition, 1.0f), g_matWorld);
    output.vPosition = mul(worldPos, g_matViewProj);
    output.vWorldPos = worldPos.xyz;
    output.vNormal = normalize(mul(float4(input.vNormal, 0.0f), g_matWorldInvTranspose).xyz);
    output.vTexCoord = input.vTexCoord;

    return output;
}

float3 EvaluatePointLight(
    PointLightData light,
    float3 worldPos,
    float3 N,
    float3 V,
    float3 albedo,
    float metallic,
    float roughness)
{
    float3 toLight = light.positionWorld - worldPos;
    float  distanceToLight = length(toLight);
    float3 L = toLight / max(distanceToLight, 1e-4f);

    float attenuation = saturate(1.0f - distanceToLight / max(light.radius, 1e-4f));
    attenuation *= attenuation;

    float3 brdf = BRDF_CookTorrance(N, V, L, albedo, metallic, roughness);
    float3 radiance = light.color * light.intensity * attenuation;
    return brdf * radiance;
}

float4 PS(PS_INPUT input) : SV_TARGET
{
    float4 albedoSample = g_AlbedoMap.Sample(g_LinearWrap, input.vTexCoord);
    clip(albedoSample.a - 0.05f);

    float3 albedo = albedoSample.rgb * g_vAlbedoTint;
    float metallic = saturate(g_fMetallic);
    float roughness = clamp(g_fRoughness, 0.04f, 1.0f);

    float3 N = normalize(input.vNormal);
    float3 V = normalize(g_vCameraWorld - input.vWorldPos);
    float2 aoUV = input.vPosition.xy / max(g_vScreenSize, float2(1.0f, 1.0f));
    float aoVisibility = g_AmbientOcclusionMap.SampleLevel(g_LinearWrap, aoUV, 0).r;

    float3 color = float3(0.0f, 0.0f, 0.0f);

    float3 L = normalize(-g_vLightDirWorld);
    float3 brdf = BRDF_CookTorrance(N, V, L, albedo, metallic, roughness);
    color += brdf * (g_vLightColor * g_fLightIntensity);

    [loop]
    for (uint i = 0; i < min(g_uPointLightCount, (uint)MAX_FORWARD_POINT_LIGHTS); ++i)
    {
        color += EvaluatePointLight(g_PointLights[i], input.vWorldPos, N, V, albedo, metallic, roughness);
    }

    float3 ambient = float3(0.03f, 0.03f, 0.03f) * albedo * (g_fAmbientOcclusion * aoVisibility);
    float3 emissive = g_vEmissiveTint * g_fEmissiveIntensity;

    color = ambient + color + emissive;
    color = color / (color + float3(1.0f, 1.0f, 1.0f));
    color = pow(saturate(color), 1.0f / 2.2f);

    return float4(color, albedoSample.a);
}
