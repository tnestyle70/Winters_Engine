Texture2D<float4> g_SceneColor : register(t0);
Texture2D<float4> g_BloomColor : register(t1);
SamplerState g_LinearClamp : register(s0);

cbuffer CBPostFx : register(b0)
{
    float2 g_vSourceTexelSize;
    float2 g_vBlurDirection;

    float g_fGamma;
    float g_fSaturation;
    float g_fGradeStrength;
    float g_fVignetteStrength;

    float3 g_vTint;
    float g_fVignetteInner;

    float g_fVignetteOuter;
    float g_fBloomThreshold;
    float g_fBloomIntensity;
    float g_fBloomSoftKnee;
};

struct VS_OUTPUT
{
    float4 vPosition : SV_POSITION;
    float2 vTexCoord : TEXCOORD0;
};

VS_OUTPUT VS_Fullscreen(uint uVertexID : SV_VertexID)
{
    VS_OUTPUT output;
    const float2 vertex = float2((uVertexID << 1) & 2, uVertexID & 2);
    output.vPosition = float4(
        vertex.x * 2.f - 1.f,
        1.f - vertex.y * 2.f,
        0.f,
        1.f);
    output.vTexCoord = vertex;
    return output;
}

float3 ExtractBloom(float3 color)
{
    const float brightness = max(max(color.r, color.g), color.b);
    const float threshold = max(g_fBloomThreshold, 0.f);
    const float knee = max(threshold * g_fBloomSoftKnee, 0.0001f);
    float soft = brightness - threshold + knee;
    soft = clamp(soft, 0.f, 2.f * knee);
    soft = soft * soft / (4.f * knee + 0.0001f);
    const float contribution = max(brightness - threshold, soft) /
        max(brightness, 0.0001f);
    return color * contribution;
}

float4 PS_BloomExtract(VS_OUTPUT input) : SV_TARGET
{
    const float2 offset = g_vSourceTexelSize * 0.5f;
    const float3 color = (
        g_SceneColor.SampleLevel(g_LinearClamp, input.vTexCoord + float2(-offset.x, -offset.y), 0.f).rgb +
        g_SceneColor.SampleLevel(g_LinearClamp, input.vTexCoord + float2( offset.x, -offset.y), 0.f).rgb +
        g_SceneColor.SampleLevel(g_LinearClamp, input.vTexCoord + float2(-offset.x,  offset.y), 0.f).rgb +
        g_SceneColor.SampleLevel(g_LinearClamp, input.vTexCoord + float2( offset.x,  offset.y), 0.f).rgb) * 0.25f;
    return float4(ExtractBloom(color), 1.f);
}

float4 PS_BloomBlur(VS_OUTPUT input) : SV_TARGET
{
    static const float kWeights[5] = {
        0.2270270270f,
        0.1945945946f,
        0.1216216216f,
        0.0540540541f,
        0.0162162162f
    };

    float3 color = g_SceneColor.SampleLevel(
        g_LinearClamp,
        input.vTexCoord,
        0.f).rgb * kWeights[0];

    [unroll]
    for (int i = 1; i < 5; ++i)
    {
        const float2 offset = g_vBlurDirection * g_vSourceTexelSize * (float)i;
        color += g_SceneColor.SampleLevel(
            g_LinearClamp,
            input.vTexCoord + offset,
            0.f).rgb * kWeights[i];
        color += g_SceneColor.SampleLevel(
            g_LinearClamp,
            input.vTexCoord - offset,
            0.f).rgb * kWeights[i];
    }

    return float4(color, 1.f);
}

float4 LoadSceneColor(float4 vPosition)
{
    uint uWidth = 0;
    uint uHeight = 0;
    g_SceneColor.GetDimensions(uWidth, uHeight);
    const uint2 maxPixel = uint2(max(uWidth, 1u) - 1u, max(uHeight, 1u) - 1u);
    const uint2 pixel = min(uint2(vPosition.xy), maxPixel);
    return g_SceneColor.Load(int3(pixel, 0));
}

float4 PS_Composite(VS_OUTPUT input) : SV_TARGET
{
    const float4 source = LoadSceneColor(input.vPosition);
    const bool bUseBloom = g_fBloomIntensity > 0.0001f;
    const bool bUseGrade = g_fGradeStrength > 0.0001f;
    if (!bUseBloom && !bUseGrade)
        return source;

    float3 color = source.rgb;
    if (bUseBloom)
    {
        color += g_BloomColor.SampleLevel(
            g_LinearClamp,
            input.vTexCoord,
            0.f).rgb * g_fBloomIntensity;
    }

    if (bUseGrade)
    {
        float3 graded = color;
        const float gamma = max(g_fGamma, 0.001f);
        if (abs(gamma - 1.f) > 0.0001f)
            graded = pow(max(graded, 0.f), 1.f / gamma);

        const float luminance = dot(graded, float3(0.299f, 0.587f, 0.114f));
        graded = lerp(luminance.xxx, graded, g_fSaturation);
        graded *= g_vTint;

        const float distanceFromCenter = length(input.vTexCoord - 0.5f);
        const float vignette = 1.f - g_fVignetteStrength * smoothstep(
            g_fVignetteInner,
            g_fVignetteOuter,
            distanceFromCenter);
        graded *= vignette;
        color = lerp(color, graded, saturate(g_fGradeStrength));
    }

    return float4(saturate(color), source.a);
}
