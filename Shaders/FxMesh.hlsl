//-Constant Buffers
cbuffer CBPerFrame : register(b0)
{
    row_major matrix g_matViewProj;
    float3 g_vCameraWorld;
    float g_fFxTime;
    float3 g_vLightDirWorld;
    float g_fLightIntensity;
};

cbuffer CBPerObject : register(b1)
{
    row_major matrix g_matWorld;
};

cbuffer CBFxParams : register(b2)
{
    float4 g_vTint; //RGB Tine , A = alpha multiplier
    float4 g_vUVRect;
    float2 g_vUVScroll;
    float g_fAlphaClip;
    float g_fErodeThreshold;
    float4 g_vStyleColorA; // rgb = hot/top color, a = emission intensity
    float4 g_vStyleColorB; // rgb = outline/bottom color, a = brush contrast
    float4 g_vRimColor;    // rgb = rim color, a = rim intensity
    float4 g_vStyleParams; // x = mode, y = rim power, z = cell low, w = cell high
    float4 g_vTimeParams;  // x = elapsed, y = normalized age, z = random, w = reserved
    float4 g_vMagicScrollA; // xy = primary scroll, zw = secondary scroll
    float4 g_vMagicShape;   // x = contrast, y = edge width, z = dissolve speed, w = distort strength
    float4 g_vMagicCore;    // x = center power, y = core intensity, z = edge intensity, w = reserved
};

Texture2D g_DiffuseMap : register(t0);
Texture2D g_ErodeMap : register(t1);
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
    float3 vWorldPos : TEXCOORD1;
    float3 vWorldNormal : TEXCOORD2;
};

// ── Vertex Shader ──
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;
    float4 worldPos = mul(float4(input.vPosition, 1.f), g_matWorld);
    output.vPosition = mul(worldPos, g_matViewProj);
    output.vWorldPos = worldPos.xyz;
    output.vWorldNormal = normalize(mul(float4(input.vNormal, 0.f), g_matWorld).xyz);

    // UV rect (atlas frame) + scroll
    float2 uv = input.vTexCoord;
    uv = lerp(g_vUVRect.xy, g_vUVRect.zw, uv);
    uv += g_vUVScroll;
    output.vTexCoord = uv;
    return output;
}

float ComputeBrush(float4 texColor)
{
    return saturate(max(max(texColor.r, texColor.g), texColor.b));
}

float ComputeRim(float3 normal, float3 worldPos)
{
    float3 viewDir = normalize(g_vCameraWorld - worldPos);
    float rimBase = 1.f - saturate(dot(normal, viewDir));
    return pow(rimBase, max(g_vStyleParams.y, 0.001f));
}

float3 ApplyFxStyle(float4 texColor, float3 normal, float3 worldPos)
{
    const float styleMode = g_vStyleParams.x;
    if (styleMode < 0.5f)
        return texColor.rgb * g_vTint.rgb;

    const float brushContrast = max(g_vStyleColorB.a, 0.001f);
    const float brush = pow(ComputeBrush(texColor), brushContrast);
    const float emissionIntensity = max(g_vStyleColorA.a, 0.f);
    const float rim = ComputeRim(normal, worldPos);
    const float3 rimRGB = g_vRimColor.rgb * rim * g_vRimColor.a;

    if (styleMode < 1.5f)
    {
        const float3 mainColor = lerp(g_vStyleColorB.rgb, g_vStyleColorA.rgb, brush);
        return mainColor * g_vTint.rgb * emissionIntensity + rimRGB;
    }

    if (styleMode < 2.5f)
    {
        const float cellLow = g_vStyleParams.z;
        const float cellHigh = max(g_vStyleParams.w, cellLow + 0.001f);
        const float3 lightDir = normalize(-g_vLightDirWorld);
        const float nDotL = dot(normal, lightDir);
        const float cellLit = (nDotL > cellHigh) ? 1.f : ((nDotL > cellLow) ? 0.55f : 0.18f);
        return texColor.rgb * g_vTint.rgb * cellLit * emissionIntensity + rimRGB;
    }

    const float gradY = normal.y * 0.5f + 0.5f;
    const float3 gradRGB = lerp(g_vStyleColorB.rgb, g_vStyleColorA.rgb, saturate(gradY));
    return gradRGB * texColor.rgb * g_vTint.rgb * emissionIntensity + rimRGB;
}

// ── Pixel Shader ──
float4 ApplyMagicSurface(PS_INPUT input)
{
    const float elapsed = g_vTimeParams.x;
    const float age = saturate(g_vTimeParams.y);
    const float random = g_vTimeParams.z;

    float2 uvA = input.vTexCoord + g_vMagicScrollA.xy * elapsed + random * 0.11f;
    float2 uvB = input.vTexCoord * 1.7f + g_vMagicScrollA.zw * elapsed + random * 0.37f;

    float2 distortVec = float2(
        g_DiffuseMap.Sample(g_Sampler, uvB * 0.7f + 1.3f).r * 2.f - 1.f,
        g_DiffuseMap.Sample(g_Sampler, uvB * 0.7f + 5.7f).r * 2.f - 1.f
    );
    uvA += distortVec * g_vMagicShape.w;

    const float nLow = g_DiffuseMap.Sample(g_Sampler, uvA).r;
    const float nHigh = g_DiffuseMap.Sample(g_Sampler, uvA * 2.7f + 0.5f).r;
    const float contrast = max(g_vMagicShape.x, 0.001f);
    const float n = pow(saturate(nLow * 0.7f + nHigh * 0.3f), contrast);

    const float2 fromCenter = input.vTexCoord - 0.5f;
    const float centerPower = max(g_vMagicCore.x, 0.001f);
    const float centerMask = pow(saturate(1.f - length(fromCenter) * 2.f), centerPower);

    const float edgeWidth = max(g_vMagicShape.y, 0.001f);
    const float dissolved = n - age * g_vMagicShape.z;
    clip(dissolved + edgeWidth);

    const float edgeMask = 1.f - smoothstep(0.f, edgeWidth, dissolved);
    const float coreMask = saturate(dissolved / edgeWidth);
    const float rim = ComputeRim(normalize(input.vWorldNormal), input.vWorldPos);

    const float emissionIntensity = max(g_vStyleColorA.a, 0.f);
    const float3 coreRGB = g_vStyleColorA.rgb * coreMask * g_vMagicCore.y;
    const float3 edgeRGB = g_vStyleColorB.rgb * edgeMask * g_vMagicCore.z;
    const float3 rimRGB = g_vRimColor.rgb * rim * g_vRimColor.a;
    const float3 hue = lerp(float3(0.9f, 0.9f, 1.1f),
        float3(1.1f, 1.0f, 0.9f), saturate(random));

    float alpha = saturate(coreMask + edgeMask) * centerMask * g_vTint.a;
    if (g_fAlphaClip > 0.f)
        clip(alpha - g_fAlphaClip);

    float3 finalRGB = (coreRGB + edgeRGB + rimRGB) * hue * g_vTint.rgb * emissionIntensity;
    finalRGB *= alpha;
    return float4(finalRGB, alpha);
}

float4 PS(PS_INPUT input) : SV_TARGET
{
    float4 texColor = g_DiffuseMap.Sample(g_Sampler, input.vTexCoord);
    texColor.a *= g_vTint.a;
    const float styleMode = g_vStyleParams.x;
    
    if (g_fErodeThreshold > 0.f)
    {
        float fErodeNoise = g_ErodeMap.Sample(g_Sampler, input.vTexCoord).r;
        if (fErodeNoise < g_fErodeThreshold)   // ★ 변수명 일치 (erodeNoize → fErodeNoise)
            discard;
    }

    if (styleMode >= 3.5f && styleMode < 4.5f)
        return ApplyMagicSurface(input);

    if (g_fAlphaClip > 0.f)
        clip(texColor.a - g_fAlphaClip);

    if (styleMode >= 0.5f)
    {
        const float brush = ComputeBrush(texColor);
        const float rim = ComputeRim(normalize(input.vWorldNormal), input.vWorldPos);
        texColor.a *= saturate(brush + rim * 0.35f);
    }

    texColor.rgb = ApplyFxStyle(texColor, normalize(input.vWorldNormal), input.vWorldPos);
    if (styleMode >= 0.5f)
        texColor.rgb *= texColor.a;
    return texColor;
}
