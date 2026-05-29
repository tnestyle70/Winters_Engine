// ─────────────────────────────────────────────────────────────────
//  Default3D.hlsl  |  MVP 변환 + 버텍스 컬러 셰이더
//
//  cbuffer PerFrame  (b0): ViewProjection 행렬
//  cbuffer PerObject (b1): World 행렬
//
//  VS: Local → World(g_World) → Clip(g_ViewProjection) 변환
//  PS: unlit — 버텍스 컬러 출력 (향후 디퓨즈 라이팅 확장 가능)
// ─────────────────────────────────────────────────────────────────

cbuffer CBPerFrame : register(b0)
{
    row_major float4x4 g_ViewProjection;
};

cbuffer CBPerObject : register(b1)
{
    row_major float4x4 g_World;
};

struct VSInput
{
    float3 pos    : POSITION;
    float3 normal : NORMAL;
    float4 col    : COLOR;
};

struct PSInput
{
    float4 pos    : SV_POSITION;
    float3 normal : NORMAL;
    float4 col    : COLOR;
};

// ── Vertex Shader ─────────────────────────────────────────────
PSInput VS(VSInput v)
{
    PSInput o;

    // Local → World
    float4 worldPos = mul(float4(v.pos, 1.0f), g_World);

    // World → Clip
    o.pos = mul(worldPos, g_ViewProjection);

    // Normal → World (균등 스케일 가정)
    o.normal = normalize(mul(v.normal, (float3x3)g_World));

    o.col = v.col;
    return o;
}

// ── Pixel Shader ──────────────────────────────────────────────
float4 PS(PSInput p) : SV_TARGET
{
    // 간단한 디퓨즈 라이팅 (태양광 방향 고정)
    float3 lightDir = normalize(float3(0.5f, 1.0f, -0.3f));
    float  ndotl    = max(dot(p.normal, lightDir), 0.0f);

    // Ambient + Diffuse
    float3 ambient = p.col.rgb * 0.3f;
    float3 diffuse = p.col.rgb * ndotl * 0.7f;

    return float4(ambient + diffuse, p.col.a);
}
