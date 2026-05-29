cbuffer PerFrame : register(b0)
{
    row_major matrix g_matViewProj;
};

cbuffer PerObject : register(b1)
{
    row_major matrix g_matWorld;
    row_major matrix g_matWorldInvTranspose;
};

cbuffer CBBones : register(b2)
{
    row_major matrix g_BoneMatrices[256];
};

struct VS_INPUT
{
    float3 vPosition    : POSITION;
    float3 vNormal      : NORMAL;
    float2 vTexCoord    : TEXCOORD0;
    float3 vTangent     : TANGENT;
    uint4  iBoneIndices : BLENDINDICES;
    float4 fBoneWeights : BLENDWEIGHT;
};

struct PS_INPUT
{
    float4 vPosition : SV_POSITION;
    float3 vNormalWS : TEXCOORD0;
};

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;

    const matrix skinMatrix =
        g_BoneMatrices[input.iBoneIndices.x] * input.fBoneWeights.x +
        g_BoneMatrices[input.iBoneIndices.y] * input.fBoneWeights.y +
        g_BoneMatrices[input.iBoneIndices.z] * input.fBoneWeights.z +
        g_BoneMatrices[input.iBoneIndices.w] * input.fBoneWeights.w;

    const float4 skinnedPos = mul(float4(input.vPosition, 1.0f), skinMatrix);
    const float3 skinnedNormal = mul(float4(input.vNormal, 0.0f), skinMatrix).xyz;

    const float4 worldPos = mul(skinnedPos, g_matWorld);
    output.vPosition = mul(worldPos, g_matViewProj);
    output.vNormalWS = normalize(mul(float4(skinnedNormal, 0.0f), g_matWorldInvTranspose).xyz);

    return output;
}

float4 PS(PS_INPUT input) : SV_TARGET
{
    const float3 normalWS = normalize(input.vNormalWS);
    return float4(normalWS * 0.5f + 0.5f, 1.0f);
}
