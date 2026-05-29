Texture2D<float>   g_AOInput     : register(t0);
Texture2D<float>   g_DepthBuffer : register(t1);
RWTexture2D<float> g_AOOutput    : register(u0);

cbuffer CBBlur : register(b0)
{
    float2 g_vScreenSize;
    float2 g_vBlurPad;
};

[numthreads(8, 8, 1)]
void CS_Main(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= (uint)g_vScreenSize.x || DTid.y >= (uint)g_vScreenSize.y)
        return;

    const int2 center = int2(DTid.xy);
    const float centerDepth = g_DepthBuffer.Load(uint3(center, 0));

    float weightedAO = 0.0f;
    float totalWeight = 0.0f;

    [unroll]
    for (int y = -2; y <= 2; ++y)
    {
        [unroll]
        for (int x = -2; x <= 2; ++x)
        {
            const int2 sampleCoord = clamp(center + int2(x, y), int2(0, 0), int2(g_vScreenSize) - 1);
            const float sampleAO = g_AOInput.Load(uint3(sampleCoord, 0));
            const float sampleDepth = g_DepthBuffer.Load(uint3(sampleCoord, 0));

            const float spatialWeight = exp(-0.5f * float(x * x + y * y));
            const float depthWeight = exp(-abs(sampleDepth - centerDepth) * 80.0f);
            const float weight = spatialWeight * depthWeight;

            weightedAO += sampleAO * weight;
            totalWeight += weight;
        }
    }

    g_AOOutput[center] = (totalWeight > 0.0f) ? (weightedAO / totalWeight) : 1.0f;
}
