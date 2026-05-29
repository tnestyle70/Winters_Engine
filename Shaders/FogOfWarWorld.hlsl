cbuffer FogWorldCB : register(b0)
{
    row_major matrix g_matViewProj;
    float4 g_vWorldRect;
    float4 g_vFogParams;
    float4 g_vUnexploredColor;
    float4 g_vExploredColor;
};

Texture2D g_FogTexture : register(t0);
SamplerState g_Sampler : register(s0);

struct VS_INPUT
{
    float3 vPosition : POSITION;
};

struct PS_INPUT
{
    float4 vPosition : SV_POSITION;
    float3 vWorldPos : TEXCOORD0;
};

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;
    float4 worldPos = float4(input.vPosition, 1.f);
    output.vPosition = mul(worldPos, g_matViewProj);
    output.vWorldPos = input.vPosition;
    return output;
}

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
