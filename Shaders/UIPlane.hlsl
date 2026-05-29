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

    const float4 worldPos = mul(float4(input.vPosition, 1.f), g_matWorld);
    output.vPosition = mul(worldPos, g_matViewProj);
    output.vTexCoord = lerp(g_vUVRect.xy, g_vUVRect.zw, input.vTexCoord) + g_vUVScroll;

    return output;
}

float4 PS(PS_INPUT input) : SV_TARGET
{
    float4 texColor = g_DiffuseMap.Sample(g_Sampler, input.vTexCoord);
    texColor *= g_vTint;

    clip(texColor.a - g_fAlphaClip);

    if (g_fErodeThreshold > 0.f)
    {
        const float brush = max(max(texColor.r, texColor.g), texColor.b);
        clip(brush - g_fErodeThreshold);
    }

    return texColor;
}
