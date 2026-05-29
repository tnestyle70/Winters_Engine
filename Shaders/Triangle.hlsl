// ─────────────────────────────────────────────────────────────────
//  Triangle.hlsl  |  Phase 0-B  |  최소 삼각형 셰이더
//
//  MVP 행렬 없음 — NDC 좌표 직접 사용
//  ConstantBuffer 없음 — 이 단계에서는 불필요
//
//  Vertex 구조체: { float3 Position, float4 Color }
//  CPU 쪽 Vertex 구조체와 시멘틱 이름이 반드시 일치해야 한다.
// ─────────────────────────────────────────────────────────────────

struct PSInput
{
    float4 pos : SV_POSITION;
    float4 col : COLOR;
};

// ── Vertex Input ──────────────────────────────────────────────
// DX11Pipeline::Create() InputLayout과 시멘틱/오프셋 일치:
//   POSITION : float3 (offset  0)
//   COLOR    : float4 (offset 12)
struct VSInput
{
    float3 pos : POSITION;
    float4 col : COLOR;
};

// ── Vertex Shader ─────────────────────────────────────────────
PSInput VS(VSInput v)
{
    PSInput o;
    o.pos = float4(v.pos, 1.0f);
    o.col = v.col;
    return o;
}

// ── Pixel Shader ──────────────────────────────────────────────
float4 PS(PSInput p) : SV_TARGET
{
    return p.col;
}
