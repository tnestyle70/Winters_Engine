
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
    output.vTexCoord = input.vTexCoord;

    return output;
}

float4 PS(PS_INPUT input) : SV_TARGET
{
    const float2 centered = input.vTexCoord * 2.f - 1.f;
    const float2 ellipse = float2(centered.x * 0.82f, centered.y * 1.18f);
    const float d2 = dot(ellipse, ellipse);

    const float broad = 1.f - smoothstep(0.18f, 1.f, d2);
    const float core = 1.f - smoothstep(0.f, 0.36f, d2);
    const float alpha = saturate(broad * 0.54f + core * 0.18f) * g_vTint.a;

    clip(alpha - max(g_fAlphaClip, 0.001f));

    return float4(g_vTint.rgb, alpha);
}
