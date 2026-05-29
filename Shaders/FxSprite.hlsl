//-Constant Buffers
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

//Input / Output - VTX Mesh 44 bytes와 호환!
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
    float2 vLocalUV : TEXCOORD1;
};

// ── Vertex Shader ──
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;
    float4 worldPos = mul(float4(input.vPosition, 1.f), g_matWorld);
    output.vPosition = mul(worldPos, g_matViewProj);

    float2 localUV = input.vTexCoord;
    float2 uv = lerp(g_vUVRect.xy, g_vUVRect.zw, localUV);
    uv += g_vUVScroll;

    output.vTexCoord = uv;
    output.vLocalUV = localUV;
    return output;
}

float ComputeBrush(float4 texColor)
{
    return saturate(max(max(texColor.r, texColor.g), texColor.b));
}

float ComputeSpriteRim(float2 localUV)
{
    float edge = min(min(localUV.x, 1.f - localUV.x), min(localUV.y, 1.f - localUV.y));
    return pow(saturate(1.f - edge * 2.f), max(g_vStyleParams.y, 0.001f));
}

float3 ApplyFxStyle(float4 texColor, float2 localUV)
{
    const float styleMode = g_vStyleParams.x;
    float3 baseColor = texColor.rgb * g_vTint.rgb;

    if (styleMode < 0.5f)
        return baseColor;

    const float rim = ComputeSpriteRim(localUV);
    const float brushContrast = max(g_vStyleColorB.a, 0.001f);
    const float brush = pow(ComputeBrush(texColor), brushContrast);
    const float3 rimRGB = g_vRimColor.rgb * rim * g_vRimColor.a;

    if (styleMode < 1.5f)
    {
        const float emissionIntensity = max(g_vStyleColorA.a, 0.f);
        const float3 mainColor = lerp(g_vStyleColorB.rgb, g_vStyleColorA.rgb, brush);
        return mainColor * g_vTint.rgb * emissionIntensity + rimRGB;
    }

    const float cellLow = g_vStyleParams.z;
    const float cellHigh = max(g_vStyleParams.w, cellLow + 0.001f);
    const float luminance = dot(baseColor, float3(0.299f, 0.587f, 0.114f));
    const float cell = smoothstep(cellLow, cellHigh, luminance);
    const float3 stylized = lerp(g_vStyleColorB.rgb, g_vStyleColorA.rgb, cell);
    return stylized * g_vTint.rgb + rimRGB;
}

float4 ApplyMagicSurface(float2 uv, float2 localUV)
{
    const float elapsed = g_vTimeParams.x;
    const float age = saturate(g_vTimeParams.y);
    const float random = g_vTimeParams.z;

    float2 uvA = uv + g_vMagicScrollA.xy * elapsed + random * 0.11f;
    float2 uvB = uv * 1.7f + g_vMagicScrollA.zw * elapsed + random * 0.37f;

    float2 distortVec = float2(
        g_DiffuseMap.Sample(g_Sampler, uvB * 0.7f + 1.3f).r * 2.f - 1.f,
        g_DiffuseMap.Sample(g_Sampler, uvB * 0.7f + 5.7f).r * 2.f - 1.f
    );
    uvA += distortVec * g_vMagicShape.w;

    const float nLow = g_DiffuseMap.Sample(g_Sampler, uvA).r;
    const float nHigh = g_DiffuseMap.Sample(g_Sampler, uvA * 2.7f + 0.5f).r;
    const float n = pow(saturate(nLow * 0.7f + nHigh * 0.3f), max(g_vMagicShape.x, 0.001f));

    const float2 fromCenter = localUV - 0.5f;
    const float centerMask = pow(saturate(1.f - length(fromCenter) * 2.f), max(g_vMagicCore.x, 0.001f));

    const float edgeWidth = max(g_vMagicShape.y, 0.001f);
    const float dissolved = n - age * g_vMagicShape.z;
    clip(dissolved + edgeWidth);

    const float edgeMask = 1.f - smoothstep(0.f, edgeWidth, dissolved);
    const float coreMask = saturate(dissolved / edgeWidth);
    const float rim = ComputeSpriteRim(localUV);

    const float3 coreRGB = g_vStyleColorA.rgb * coreMask * g_vMagicCore.y;
    const float3 edgeRGB = g_vStyleColorB.rgb * edgeMask * g_vMagicCore.z;
    const float3 rimRGB = g_vRimColor.rgb * rim * g_vRimColor.a;
    const float3 hue = lerp(float3(0.9f, 0.9f, 1.1f),
        float3(1.1f, 1.0f, 0.9f), saturate(random));

    float alpha = saturate(coreMask + edgeMask) * centerMask * g_vTint.a;
    if (g_fAlphaClip > 0.f)
        clip(alpha - g_fAlphaClip);

    const float emissionIntensity = max(g_vStyleColorA.a, 0.f);
    float3 finalRGB = (coreRGB + edgeRGB + rimRGB) * hue * g_vTint.rgb * emissionIntensity;
    finalRGB *= alpha;
    return float4(finalRGB, alpha);
}

// ── Pixel Shader ──
float4 PS(PS_INPUT input) : SV_TARGET
{
    float4 texColor = g_DiffuseMap.Sample(g_Sampler, input.vTexCoord);
    texColor.a *= g_vTint.a;

    if (g_fErodeThreshold > 0.f)
    {
        float erodeMask = saturate(dot(texColor.rgb, float3(0.299f, 0.587f, 0.114f)));
        clip(erodeMask - g_fErodeThreshold);
    }

    if (g_fAlphaClip > 0.f)
        clip(texColor.a - g_fAlphaClip);

    const float styleMode = g_vStyleParams.x;
    if (styleMode >= 3.5f && styleMode < 4.5f)
        return ApplyMagicSurface(input.vTexCoord, input.vLocalUV);

    texColor.rgb = ApplyFxStyle(texColor, input.vLocalUV);
    return texColor;
}
