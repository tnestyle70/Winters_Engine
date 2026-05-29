static const float PI = 3.14159265359f;

cbuffer CBGTAO : register(b0)
{
    row_major matrix g_matViewProj;
    row_major matrix g_matViewProjInv;
    float2 g_vScreenSize;
    float  g_fRadius;
    float  g_fIntensity;
    float  g_fThicknessHeuristic;
    float3 g_vGTAOPad;
};

Texture2D<float>   g_DepthBuffer  : register(t0);
Texture2D<float4>  g_NormalBuffer : register(t1);
RWTexture2D<float> g_AOOutput     : register(u0);

SamplerState g_PointSampler : register(s0);

float3 ReconstructWorldPos(float2 uv, float depth)
{
    float2 ndc = uv * 2.0f - 1.0f;
    ndc.y = -ndc.y;
    float4 clip = float4(ndc, depth, 1.0f);
    float4 world = mul(clip, g_matViewProjInv);
    return world.xyz / max(world.w, 1e-5f);
}

[numthreads(8, 8, 1)]
void CS_Main(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= (uint)g_vScreenSize.x || DTid.y >= (uint)g_vScreenSize.y)
        return;

    const uint2 pixel = DTid.xy;
    const float centerDepth = g_DepthBuffer.Load(uint3(pixel, 0));
    if (centerDepth >= 0.99999f)
    {
        g_AOOutput[pixel] = 1.0f;
        return;
    }

    const float2 uv = (float2(pixel) + 0.5f) / g_vScreenSize;
    const float3 centerPos = ReconstructWorldPos(uv, centerDepth);
    const float3 normalWS = normalize(g_NormalBuffer.Load(uint3(pixel, 0)).xyz * 2.0f - 1.0f);

    const int kDirCount = 8;
    const int kStepCount = 4;
    const float screenRadius = max(g_fRadius * 0.05f, 1.0f / g_vScreenSize.x);

    float visibility = 0.0f;
    uint sampleCount = 0;

    [unroll]
    for (int dirIndex = 0; dirIndex < kDirCount; ++dirIndex)
    {
        const float angle = (2.0f * PI * dirIndex) / kDirCount;
        const float2 dir = float2(cos(angle), sin(angle));

        [unroll]
        for (int stepIndex = 1; stepIndex <= kStepCount; ++stepIndex)
        {
            const float stepT = (float)stepIndex / kStepCount;
            const float2 sampleUV = saturate(uv + dir * screenRadius * stepT);
            const float sampleDepth = g_DepthBuffer.SampleLevel(g_PointSampler, sampleUV, 0);

            if (sampleDepth >= 0.99999f)
            {
                visibility += 1.0f;
                sampleCount++;
                continue;
            }

            const float3 samplePos = ReconstructWorldPos(sampleUV, sampleDepth);
            const float3 toSample = samplePos - centerPos;
            const float distanceToSample = length(toSample);
            if (distanceToSample <= 1e-4f)
                continue;

            const float3 sampleDir = toSample / distanceToSample;
            const float ndotDir = saturate(dot(normalWS, sampleDir));
            const float distanceWeight = saturate(1.0f - distanceToSample / max(g_fRadius, 1e-4f));
            const float horizon = saturate((ndotDir - g_fThicknessHeuristic) / max(1.0f - g_fThicknessHeuristic, 1e-4f));
            const float occlusion = horizon * distanceWeight;

            visibility += (1.0f - occlusion);
            sampleCount++;
        }
    }

    const float ao = (sampleCount > 0) ? visibility / sampleCount : 1.0f;
    g_AOOutput[pixel] = pow(saturate(ao), g_fIntensity);
}
