Session - diffuse-only Mesh3D/Skinned3D를 texture-only 출력에서 stylized lighting 출력으로 바꾼다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shaders/Mesh3D.hlsl

기존 코드:

```hlsl
// ── Constant Buffers ──
cbuffer CBPerFrame : register(b0)
{
    row_major matrix g_matViewProj;
};

cbuffer CBPerObject : register(b1)
{
    row_major matrix g_matWorld;
};

// ── Texture ──
Texture2D g_DiffuseMap : register(t0);
SamplerState g_Sampler : register(s0);

// ── Input / Output ──
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
    float3 vNormal : NORMAL;
    float2 vTexCoord : TEXCOORD0;
    float3 vWorldPos : TEXCOORD1;
};

// ── Vertex Shader ──
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;

    float4 worldPos = mul(float4(input.vPosition, 1.f), g_matWorld);
    output.vPosition = mul(worldPos, g_matViewProj);
    output.vNormal = normalize(mul(input.vNormal, (float3x3) g_matWorld));
    output.vTexCoord = input.vTexCoord;
    output.vWorldPos = worldPos.xyz;

    return output;
}

// ── Pixel Shader ──
float4 PS(PS_INPUT input) : SV_TARGET
{
    float4 texColor = g_DiffuseMap.Sample(g_Sampler, input.vTexCoord);
    clip(texColor.a - 0.05f); //alpha < 0.05 픽셀은 discard!
    return texColor;
}
```

아래로 교체:

```hlsl
struct PointLightData
{
    float3 vPosition;
    float fRadius;
    float3 vColor;
    float fIntensity;
};

cbuffer CBPerFrame : register(b0)
{
    row_major matrix g_matViewProj;
    float3 g_vCameraWorld;
    float g_fFramePad0;
    float3 g_vLightDirWorld;
    float g_fLightIntensity;
    float3 g_vLightColor;
    uint g_iPointLightCount;
    PointLightData g_PointLights[4];
    float2 g_vScreenSize;
    float2 g_vFramePad1;
};

cbuffer CBPerObject : register(b1)
{
    row_major matrix g_matWorld;
    row_major matrix g_matWorldInvTranspose;
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
    float3 vNormal : NORMAL;
    float2 vTexCoord : TEXCOORD0;
    float3 vWorldPos : TEXCOORD1;
};

float3 ApplyStylizedDiffuse(float3 baseColor, float3 normalWS, float3 worldPos)
{
    const float3 N = normalize(normalWS);
    const float3 L = normalize(-g_vLightDirWorld);
    const float3 V = normalize(g_vCameraWorld - worldPos);

    const float wrap = 0.45f;
    float key = saturate((dot(N, L) + wrap) / (1.0f + wrap));
    key = smoothstep(0.08f, 0.95f, key);

    const float top = saturate(N.y * 0.5f + 0.5f);
    const float rim = pow(1.0f - saturate(dot(N, V)), 3.5f) * smoothstep(0.10f, 0.75f, key);

    const float3 ambientLow = float3(0.36f, 0.42f, 0.62f);
    const float3 ambientHigh = float3(0.70f, 0.74f, 0.82f);
    const float3 shadowTint = float3(0.42f, 0.48f, 0.68f);
    const float3 keyTint = max(
        g_vLightColor * max(g_fLightIntensity, 0.001f),
        float3(0.001f, 0.001f, 0.001f));

    float3 color = baseColor * lerp(shadowTint, keyTint, key);
    color += baseColor * lerp(ambientLow, ambientHigh, top) * 0.48f;
    color += float3(0.52f, 0.70f, 1.00f) * rim * 0.22f;
    color *= lerp(0.86f, 1.08f, top);

    return saturate(color);
}

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;

    const float4 worldPos = mul(float4(input.vPosition, 1.0f), g_matWorld);
    output.vPosition = mul(worldPos, g_matViewProj);
    output.vNormal = normalize(mul(float4(input.vNormal, 0.0f), g_matWorldInvTranspose).xyz);
    output.vTexCoord = input.vTexCoord;
    output.vWorldPos = worldPos.xyz;

    return output;
}

float4 PS(PS_INPUT input) : SV_TARGET
{
    const float4 texColor = g_DiffuseMap.Sample(g_Sampler, input.vTexCoord);
    clip(texColor.a - 0.05f);

    const float3 color = ApplyStylizedDiffuse(texColor.rgb, normalize(input.vNormal), input.vWorldPos);
    return float4(color, texColor.a);
}
```

1-2. C:/Users/user/Desktop/Winters/Shaders/Skinned3D.hlsl

기존 코드:

```hlsl
cbuffer CBPerFrame : register(b0)
{
    row_major matrix g_matViewProj;
};

cbuffer CBPerObject : register(b1)
{
    row_major matrix g_matWorld;
};

cbuffer CBBones : register(b2)
{
    row_major matrix g_BoneMatrices[256];
};

Texture2D g_DiffuseMap : register(t0);
SamplerState g_Sampler : register(s0);

struct VS_INPUT
{
    float3 vPosition : POSITION;
    float3 vNormal : NORMAL;
    float2 vTexCoord : TEXCOORD0;
    float3 vTangent : TANGENT;
    uint4 iBoneIndices : BLENDINDICES;
    float4 fBoneWeights : BLENDWEIGHT;
};

struct PS_INPUT
{
    float4 vPosition : SV_Position;
    float3 vNormal : NORMAL;
    float2 vTexCoord : TEXCOORD0;
    float3 vWorldPos : TEXCOORD1;
};

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;
    matrix skinMatrix =
        g_BoneMatrices[input.iBoneIndices.x] * input.fBoneWeights.x +
        g_BoneMatrices[input.iBoneIndices.y] * input.fBoneWeights.y +
        g_BoneMatrices[input.iBoneIndices.z] * input.fBoneWeights.z +
        g_BoneMatrices[input.iBoneIndices.w] * input.fBoneWeights.w;
    
    float4 skinned = mul(float4(input.vPosition, 1.f), skinMatrix);
    float4 worldPos = mul(skinned, g_matWorld);
    output.vPosition = mul(worldPos, g_matViewProj);
    output.vNormal = normalize(mul(input.vNormal, (float3x3) (skinMatrix)));
    output.vTexCoord = input.vTexCoord;
    output.vWorldPos = worldPos.xyz;
    
    return output;
}

float4 PS(PS_INPUT input) : SV_TARGET
{
    return g_DiffuseMap.Sample(g_Sampler, input.vTexCoord);
}
```

아래로 교체:

```hlsl
struct PointLightData
{
    float3 vPosition;
    float fRadius;
    float3 vColor;
    float fIntensity;
};

cbuffer CBPerFrame : register(b0)
{
    row_major matrix g_matViewProj;
    float3 g_vCameraWorld;
    float g_fFramePad0;
    float3 g_vLightDirWorld;
    float g_fLightIntensity;
    float3 g_vLightColor;
    uint g_iPointLightCount;
    PointLightData g_PointLights[4];
    float2 g_vScreenSize;
    float2 g_vFramePad1;
};

cbuffer CBPerObject : register(b1)
{
    row_major matrix g_matWorld;
    row_major matrix g_matWorldInvTranspose;
};

cbuffer CBBones : register(b2)
{
    row_major matrix g_BoneMatrices[256];
};

Texture2D g_DiffuseMap : register(t0);
SamplerState g_Sampler : register(s0);

struct VS_INPUT
{
    float3 vPosition : POSITION;
    float3 vNormal : NORMAL;
    float2 vTexCoord : TEXCOORD0;
    float3 vTangent : TANGENT;
    uint4 iBoneIndices : BLENDINDICES;
    float4 fBoneWeights : BLENDWEIGHT;
};

struct PS_INPUT
{
    float4 vPosition : SV_POSITION;
    float3 vNormal : NORMAL;
    float2 vTexCoord : TEXCOORD0;
    float3 vWorldPos : TEXCOORD1;
};

float3 ApplyStylizedDiffuse(float3 baseColor, float3 normalWS, float3 worldPos)
{
    const float3 N = normalize(normalWS);
    const float3 L = normalize(-g_vLightDirWorld);
    const float3 V = normalize(g_vCameraWorld - worldPos);

    const float wrap = 0.45f;
    float key = saturate((dot(N, L) + wrap) / (1.0f + wrap));
    key = smoothstep(0.08f, 0.95f, key);

    const float top = saturate(N.y * 0.5f + 0.5f);
    const float rim = pow(1.0f - saturate(dot(N, V)), 3.5f) * smoothstep(0.10f, 0.75f, key);

    const float3 ambientLow = float3(0.36f, 0.42f, 0.62f);
    const float3 ambientHigh = float3(0.70f, 0.74f, 0.82f);
    const float3 shadowTint = float3(0.42f, 0.48f, 0.68f);
    const float3 keyTint = max(
        g_vLightColor * max(g_fLightIntensity, 0.001f),
        float3(0.001f, 0.001f, 0.001f));

    float3 color = baseColor * lerp(shadowTint, keyTint, key);
    color += baseColor * lerp(ambientLow, ambientHigh, top) * 0.48f;
    color += float3(0.52f, 0.70f, 1.00f) * rim * 0.22f;
    color *= lerp(0.86f, 1.08f, top);

    return saturate(color);
}

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;
    const matrix skinMatrix =
        g_BoneMatrices[input.iBoneIndices.x] * input.fBoneWeights.x +
        g_BoneMatrices[input.iBoneIndices.y] * input.fBoneWeights.y +
        g_BoneMatrices[input.iBoneIndices.z] * input.fBoneWeights.z +
        g_BoneMatrices[input.iBoneIndices.w] * input.fBoneWeights.w;

    const float4 skinned = mul(float4(input.vPosition, 1.0f), skinMatrix);
    const float3 skinnedNormal = mul(float4(input.vNormal, 0.0f), skinMatrix).xyz;
    const float4 worldPos = mul(skinned, g_matWorld);

    output.vPosition = mul(worldPos, g_matViewProj);
    output.vNormal = normalize(mul(float4(skinnedNormal, 0.0f), g_matWorldInvTranspose).xyz);
    output.vTexCoord = input.vTexCoord;
    output.vWorldPos = worldPos.xyz;

    return output;
}

float4 PS(PS_INPUT input) : SV_TARGET
{
    const float4 texColor = g_DiffuseMap.Sample(g_Sampler, input.vTexCoord);
    clip(texColor.a - 0.05f);

    const float3 color = ApplyStylizedDiffuse(texColor.rgb, normalize(input.vNormal), input.vWorldPos);
    return float4(color, texColor.a);
}
```

1-3. C:/Users/user/Desktop/Winters/Engine/Private/Renderer/ModelRenderer.cpp

스키닝 렌더링 non-PBR 분기에서 아래 코드를:

```cpp
        else
        {
            m_pImpl->cbPerFrame.BindVS(pContext, 0);
        }
```

아래로 교체:

```cpp
        else
        {
            m_pImpl->cbPerFrame.Bind(pContext, 0);
        }
```

정적 메시 렌더링 non-PBR 분기에서 아래 코드를:

```cpp
    else
    {
        m_pImpl->cbPerFrame.BindVS(pContext, 0);
    }
```

아래로 교체:

```cpp
    else
    {
        m_pImpl->cbPerFrame.Bind(pContext, 0);
    }
```

1-4. C:/Users/user/Desktop/Winters/Client/Private/Scene/InGameRenderBridge.cpp

기존 코드:

```cpp
    scene.m_Map.UpdateCamera(vp);
    scene.m_Map.UpdateTransform(scene.m_MapTransform.GetWorldMatrix());
    scene.m_Map.Render();
```

아래로 교체:

```cpp
    scene.m_Map.UpdateCamera(vp, cameraWorld);
    scene.m_Map.UpdateTransform(scene.m_MapTransform.GetWorldMatrix());
    scene.m_Map.Render();
```

2. 검증

미검증:
- 빌드 미검증
- 인게임 챔피언 diffuse-only 조명 변화 미검증
- map이 같은 `Mesh3D.hlsl`을 사용하므로 맵 톤 변화 미검증

검증 명령:
- git diff --check
- msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64

수동 확인:
- 일반 F5 flow에서 roster/map/minion/snapshot/champion 시스템을 숨기지 않고 확인.
- Yone, Annie, Ashe 중 최소 2개 챔피언을 같은 카메라에서 before/after screenshot 비교.
- 스키닝 챔피언의 팔/무기/머리카락 normal 방향이 뒤집히지 않는지 확인.
