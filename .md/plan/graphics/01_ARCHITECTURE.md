# Graphics 아키텍처 — RenderGraph + 디렉토리

## 디렉토리

```
Engine/Public/Renderer/
├── Core/
│   ├── IRenderer.h              Stage 위 추상
│   ├── RenderContext.h          프레임 컨텍스트 (카메라, 라이트, 씬)
│   ├── RenderTarget.h           RT + DSV 통합 관리
│   ├── RenderGraph.h            패스 DAG (Phase 2)
│   └── FrameConstants.h         b0 = PerFrame (VP, camera, time)
├── BRDF/
│   ├── BRDFCommon.hlsli
│   ├── BRDFLambert.hlsli
│   ├── BRDFCookTorrance.hlsli
│   ├── BRDFGGX.hlsli            핵심
│   ├── BRDFDisney.hlsli
│   └── BRDFTests.hlsl           단위 검증 (Furnace test)
├── PBR/
│   ├── Material.h               C++ 재질 기술
│   ├── MaterialShader.hlsl
│   ├── IBLPrecompute.h          irradiance + prefiltered + BRDF LUT 베이킹
│   └── IBLBaker.hlsl
├── GI/
│   ├── SSAO/
│   │   ├── SSAO.hlsl
│   │   └── HBAOPlus.hlsl
│   ├── SSR/
│   │   ├── HiZ.hlsl             Hi-Z 버퍼 빌드
│   │   └── SSR.hlsl             ray marching
│   ├── VXGI/
│   │   ├── Voxelize.hlsl        씬 → 3D 텍스처
│   │   └── ConeTracing.hlsl
│   └── DDGI/
│       ├── ProbeTrace.hlsl      probe 당 광선
│       ├── ProbeUpdate.hlsl
│       └── ProbeSampling.hlsl
├── PostFX/
│   ├── TAA/
│   │   ├── TAA.hlsl
│   │   ├── MotionVectors.hlsl
│   │   └── History.h            이전 프레임 저장
│   ├── Bloom/
│   │   ├── Bloom_Kawase.hlsl    (간이)
│   │   └── Bloom_FFT.hlsl       (정석)
│   ├── DoF/
│   │   └── DoF_FFT.hlsl         circular bokeh
│   ├── Tonemap.hlsl             ACES
│   └── Exposure.hlsl            eye adaptation
├── FFT/
│   ├── CooleyTukey.h            C++ (CPU reference)
│   ├── CooleyTukey.hlsl         Compute shader
│   ├── FFT2D.hlsl
│   └── FFT2D_Radix2.hlsl
├── Ocean/
│   ├── OceanSimulation.hlsl     Phillips spectrum → displacement
│   ├── OceanRender.hlsl
│   └── OceanTypes.h
├── PathTracer/
│   ├── PathTracer.hlsl
│   ├── BVH.h                    C++ 빌더 (Physics 와 공유)
│   ├── BVHTraversal.hlsl
│   ├── Sampler.hlsl             Halton/Sobol
│   └── PathTracerCPU.h          디버그용 CPU 레퍼런스
├── MonteCarlo/
│   ├── Random.hlsl              PCG/Philox
│   ├── Sampler_Halton.hlsl
│   ├── Sampler_Sobol.hlsl
│   ├── Sampler_Stratified.hlsl
│   └── ImportanceSampling.hlsl
└── Systems/
    ├── MaterialSystem.h
    ├── LightSystem.h
    ├── RenderGraphSystem.h
    ├── TAASystem.h
    ├── SSAOSystem.h
    ├── PathTracerSystem.h
    └── BloomSystem.h
```

## RenderGraph (Phase 2 동반)

모든 Stage 는 RenderGraph 위에 올림. 각 Stage = RenderGraph 노드.

```cpp
// 사용 예시
rg.AddPass("GBufferFill")
  .Output("Albedo", TextureDesc{...})
  .Output("Normal", TextureDesc{...})
  .Output("Depth",  TextureDesc{...})
  .Execute([this](CommandList& cmd) { /* DrawOpaqueGeometry */ });

rg.AddPass("SSAO")
  .Input("Normal").Input("Depth")
  .Output("Occlusion", TextureDesc{...})
  .Execute(...);

rg.AddPass("Lighting")
  .Input("Albedo").Input("Normal").Input("Depth").Input("Occlusion")
  .Output("HDR", TextureDesc{...})
  .Execute(...);

rg.AddPass("TAA")
  .Input("HDR").Input("MotionVectors").History("HistoryTAA")
  .Output("HDR_TAA")
  .Execute(...);

rg.AddPass("Bloom")
  .Input("HDR_TAA")
  .Output("HDR_Bloom")
  .Execute(...);

rg.AddPass("Tonemap")
  .Input("HDR_Bloom")
  .Output("Backbuffer")
  .Execute(...);

rg.Compile();
rg.Execute();
```

자동 배리어 / 자원 생명주기 / GPU 메모리 풀링.

## Shader 모듈화

HLSL `.hlsli` 를 공통 라이브러리처럼 사용.

```hlsl
// Shaders/Lighting/Opaque.hlsl
#include "BRDF/BRDFGGX.hlsli"
#include "MonteCarlo/Random.hlsli"

float4 PSMain(PSInput input) : SV_Target
{
    Material mat = GetMaterial(input);
    float3 N = normalize(input.normal);
    float3 V = normalize(g_cameraPos - input.worldPos);
    float3 L = normalize(g_lightDir);

    float3 brdf = EvaluateGGX(N, V, L, mat);
    float3 radiance = g_lightColor * saturate(dot(N, L));
    return float4(brdf * radiance, 1.0);
}
```

## C++ ↔ HLSL 공유 헤더

```cpp
// Engine/Public/Renderer/SharedShaderTypes.h
// 이 헤더는 .hlsl 에서도 include 가능 (define 분기)

#ifdef __cplusplus
    using float2 = DirectX::XMFLOAT2;
    using float3 = DirectX::XMFLOAT3;
    using float4 = DirectX::XMFLOAT4;
    using matrix = DirectX::XMFLOAT4X4;
#endif

struct MaterialGPU {
    float3 albedo;
    float  metallic;
    float  roughness;
    float  ao;
    float2 _pad;
    // 16-byte aligned
};

struct PerFrameGPU {
    matrix viewProj;
    matrix viewProjInv;
    float3 cameraPos;
    float  time;
    float2 screenSize;
    float2 _pad;
};
```

cbuffer 레이아웃 실수 방지.

## Material System

```cpp
class CMaterial
{
public:
    static unique_ptr<CMaterial> Create(const std::string& name);

    void SetTexture(const std::string& slot, CTexture* tex);
    void SetParameter(const std::string& name, const std::variant<f32_t, Vec3, Vec4>& val);

    // GPU 재질 구조체로 변환
    MaterialGPU ToGPU() const;

    // 어느 셰이더 사용할지 결정
    u32_t GetShaderPermutationID() const;

private:
    std::unordered_map<std::string, CTexture*> m_textures;
    std::unordered_map<std::string, Vec4>      m_parameters;

    // Shader Features
    bool_t m_bUseNormalMap     = false;
    bool_t m_bUseSkinning      = false;
    bool_t m_bUseDisney        = false;  // Phase E Stage 1
    bool_t m_bUseParallax      = false;
};
```

## Lighting Model

```cpp
struct Light
{
    enum class Type { Directional, Point, Spot, Area };
    Type   type;
    Vec3   position;       // point/spot/area
    Vec3   direction;      // dir/spot
    Vec3   color;
    f32_t  intensity;      // lumens / candela
    f32_t  range;
    f32_t  innerAngle, outerAngle;   // spot
    Vec2   areaSize;                 // area (rectangle)

    bool_t bCastShadow = true;
};

struct LightComponent : Light { /* ECS 컴포넌트 */ };
```

## GPU 리소스 풀 (Phase 2 RenderGraph 일부)

```cpp
class CTexturePool
{
public:
    ComPtr<ID3D11Texture2D> Allocate(const TextureDesc& desc);
    void Release(ComPtr<ID3D11Texture2D>& tex);   // 풀에 반환

    // 같은 크기/포맷 텍스처 재사용 → frame-wise GPU 메모리 절약
};
```

## HDR 파이프라인

전 파이프라인 HDR (float16). Tonemap 시 LDR 변환.

```
G-Buffer (RGB16F)
     ↓
Lighting (RGB16F)
     ↓
SSR / SSAO / GI (RGB16F 누적)
     ↓
TAA (RGB16F 누적)
     ↓
Bloom (RGB16F blur 추가)
     ↓
Exposure (auto-exposure)
     ↓
Tonemap ACES (LDR R8G8B8A8)
     ↓
UI composit
     ↓
Backbuffer
```

## Path Tracer vs Real-Time 일관성

목표: **Path Tracer (Stage 4) 결과를 Ground Truth 로 실시간 (Stage 5 GI) 결과 검증**.

```cpp
// 같은 재질, 같은 씬, 같은 카메라
// Path Tracer 로 수천 SPP 렌더 → reference.png
// Real-Time 렌더 결과 → compare
// MSE / PSNR / LPIPS 로 수치 평가
```

## 성능 프로파일링

- GPU 타이머 쿼리 (ID3D11Query)
- 각 RenderGraph 패스별 GPU 시간
- ImGui 에 스택 프로파일러
- PIX / RenderDoc 통합 (디버깅 마커)

## 결정 사항

- **색 공간**: 계산 = linear, 표시 = sRGB (Gamma 2.2 변환)
- **HDR 범위**: [0, 100000] (태양 직사광 대응)
- **기본 토네매퍼**: ACES (영화 업계 표준)
- **기본 anti-aliasing**: TAA (Stage 7)
- **VR / Multi-view**: Phase E 에선 미지원
