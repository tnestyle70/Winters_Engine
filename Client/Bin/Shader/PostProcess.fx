// PostProcess.fx
// DX9 HLSL Post-Process Effect
// 화면 노이즈 / 왜곡 / 보라 공허 이펙트

// ──────────────────────────────────────────
// Textures
// ──────────────────────────────────────────
texture SceneMap;
texture NoiseMap;

sampler2D SceneTex = sampler_state
{
    Texture = <SceneMap>;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
    MipFilter = NONE;
    AddressU = CLAMP;
    AddressV = CLAMP;
};

sampler2D NoiseTex = sampler_state
{
    Texture = <NoiseMap>;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
    MipFilter = NONE;
    AddressU = WRAP;
    AddressV = WRAP;
};

// ──────────────────────────────────────────
// Parameters
// ──────────────────────────────────────────
float  fTime;          // 누적 시간 (초)
float  fNoiseAmt;      // 그레인 노이즈 강도   (0~1)
float  fDistortAmt;    // UV 왜곡 강도         (0~1)
float  fShakeX;        // 화면 흔들림 X 오프셋 (UV 공간)
float  fShakeY;        // 화면 흔들림 Y 오프셋 (UV 공간)
float4 vVoidTint;      // xyz = 보라 컬러, w = 블렌드 강도

// ──────────────────────────────────────────
// Vertex Shader  (pass-through, NDC 좌표)
// ──────────────────────────────────────────
struct VS_IN
{
    float4 pos : POSITION;
    float2 uv  : TEXCOORD0;
};

struct VS_OUT
{
    float4 pos : POSITION;
    float2 uv  : TEXCOORD0;
};

VS_OUT VS_Screen(VS_IN i)
{
    VS_OUT o;
    o.pos = i.pos;   // 이미 클립 공간 NDC
    o.uv  = i.uv;
    return o;
}

// ──────────────────────────────────────────
// Pixel Shader
// ──────────────────────────────────────────
float4 PS_PostProcess(float2 uv : TEXCOORD0) : COLOR0
{
    // 1. 화면 흔들림 (쉐이킹)
    uv.x += fShakeX;
    uv.y += fShakeY;

    // 2. UV 왜곡 (꼬리 공격 / 브레스 왜곡)
    if (fDistortAmt > 0.001f)
    {
        float2 noiseUV = uv * 4.0f + float2(fTime * 0.4f, -fTime * 0.25f);
        float2 distort = (tex2D(NoiseTex, noiseUV).xy * 2.0f - 1.0f);
        uv += distort * fDistortAmt * 0.04f;
    }

    // UV 클램프 (화면 밖 샘플링 방지)
    uv = saturate(uv);

    // 3. 씬 샘플링
    float4 scene = tex2D(SceneTex, uv);

    // 4. 그레인 노이즈 오버레이 (피격 충격)
    if (fNoiseAmt > 0.001f)
    {
        float2 grainUV = uv * 8.0f + float2(fTime * 23.7f, fTime * 17.3f);
        float  grain   = tex2D(NoiseTex, grainUV).r * 2.0f - 1.0f;
        scene.rgb += grain * fNoiseAmt * 0.25f;
    }

    // 5. 보라 공허 이펙트 + 비네팅 (브레스 상태)
    if (vVoidTint.w > 0.001f)
    {
        float2 c        = uv - 0.5f;
        float  vignette = saturate(1.0f - dot(c, c) * 2.8f);
        scene.rgb = lerp(scene.rgb,
                         scene.rgb * vignette + vVoidTint.xyz * 0.4f,
                         vVoidTint.w);
    }

    return saturate(scene);
}

// ──────────────────────────────────────────
// Technique
// ──────────────────────────────────────────
technique PostProcess
{
    pass P0
    {
        VertexShader    = compile vs_2_0 VS_Screen();
        PixelShader     = compile ps_2_0 PS_PostProcess();

        ZEnable         = FALSE;
        ZWriteEnable    = FALSE;
        AlphaBlendEnable= FALSE;
        CullMode        = NONE;
    }
}
