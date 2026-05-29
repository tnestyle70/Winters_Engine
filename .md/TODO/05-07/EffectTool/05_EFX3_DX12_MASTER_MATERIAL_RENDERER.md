# EFX-3 DX12 Master Material Renderer

작성일: 2026-05-07
상태: 구현 계획
의존:
- `04_EFX2_RUNTIME_SOA_AND_ECS_SYSTEMS.md`
- `.md/plan/EffectTool/20_RENDERER_6_BAKE.md`
- `.md/plan/EffectTool/27_AAA_VFX_INSIGHTS_AND_MASTER_MATERIALS_BAKE.md`
- `.md/plan/EffectTool/28_DX12_MIGRATION_FX_INTEGRATION_BAKE.md`

목적:
- Master Material 3종을 DX12 primary renderer로 구현한다.
- LoL FX 제작은 `M_VFX_Particle_Generic`과 `M_VFX_Trail`부터 시작한다.
- Elden 대비는 `M_VFX_Volumetric`의 최소 출력까지 준비한다.

---

## 1. RHI-G0 선행 조건

EFX-3은 DX12 visual renderer이므로 다음이 먼저 필요하다.

```txt
IRHIDevice
  CreateBuffer / DestroyBuffer
  CreateTexture / DestroyTexture
  CreateShader / DestroyShader
  CreateSampler / DestroySampler

IRHICommandList
  SetPipeline 실 구현
  SetBindGroup 실 구현
  SetVertexBuffer 실 구현
  SetIndexBuffer 실 구현
  TransitionResource 실 구현
  DrawIndexedIndirect DX12 구현

DX12 backend
  D3D12MA buffer/texture allocation
  RootSignature creation
  Graphics PSO creation
  Compute PSO creation
  DescriptorHeap/SRV/CBV/Sampler binding
```

현재 코드상 주의:

```txt
1. RHIBufferDesc/RHITextureDesc는 이미 있으나 public creation API가 없다.
2. DX12Buffer/DX12Texture 구현 파일은 있으나 CDX12Device public table과 연결이 필요하다.
3. DX12RootSignatureBuilder는 RHIBindGroupLayoutDesc 기반 내부 builder로 존재한다.
4. DX12PipelineState는 현재 desc copy 중심이다.
```

---

## 2. 신규 파일

Renderer:

```txt
Engine/Public/FX/v2/Renderer/FxRendererTypes.h
Engine/Public/FX/v2/Renderer/FxRenderer.h
Engine/Public/FX/v2/Renderer/FxSpriteRenderer.h
Engine/Public/FX/v2/Renderer/FxTrailRenderer.h
Engine/Public/FX/v2/Renderer/FxMeshRenderer.h
Engine/Public/FX/v2/Renderer/FxVolumetricRenderer.h

Engine/Private/FX/v2/Renderer/FxSpriteRenderer.cpp
Engine/Private/FX/v2/Renderer/FxTrailRenderer.cpp
Engine/Private/FX/v2/Renderer/FxMeshRenderer.cpp
Engine/Private/FX/v2/Renderer/FxVolumetricRenderer.cpp
```

Material:

```txt
Engine/Public/FX/v2/Material/FxMaterialConstants.h
Engine/Public/FX/v2/Material/FxMaterialGpuLayout.h
Engine/Private/FX/v2/Material/FxMasterMaterialRegistry.cpp
```

Shaders:

```txt
Shaders/FX/v2/Master/MasterCommon.hlsli
Shaders/FX/v2/Master/M_VFX_Particle_Generic_VS.hlsl
Shaders/FX/v2/Master/M_VFX_Particle_Generic_PS.hlsl
Shaders/FX/v2/Master/M_VFX_Trail_VS.hlsl
Shaders/FX/v2/Master/M_VFX_Trail_PS.hlsl
Shaders/FX/v2/Master/M_VFX_Volumetric_VS.hlsl
Shaders/FX/v2/Master/M_VFX_Volumetric_PS.hlsl
```

---

## 3. Master Material 3종

### EFX3-1. M_VFX_Particle_Generic

용도:

```txt
Billboard
Ground decal
Shockwave ring
Small mesh surface FX if topology is quad-like
```

HLSL 핵심:

```txt
UV pan
UV distortion
two-octave grayscale mask
contrast shaping
dissolve threshold
edge emission
center mask
fresnel
color over life
soft particle
atlas frame
per-instance random variation
```

Texture slots:

```txt
t0 MainMask
t1 NoiseA
t2 NoiseB
t3 ColorOverLife
t4 AlphaOverLife
s0 LinearClamp
s1 LinearWrap
```

Constant buffers:

```txt
b0 PerFrame
b1 PerEmitter
b2 PerMaterialParticle
```

### EFX3-2. M_VFX_Trail

용도:

```txt
Ribbon
Beam
Slash trail
Projectile trail
Sword arc
```

HLSL 핵심:

```txt
u along width
v along length
head/tail mask
flow map optional
edge emission
width over life
UV scroll
noise dissolve
```

Texture slots:

```txt
t0 MainMask
t1 TrailNoise
t2 FlowMap
t3 ColorOverLife
s0 LinearClamp
s1 LinearWrap
```

### EFX3-3. M_VFX_Volumetric

용도:

```txt
Smoke
Fog pillar
Elden boss spell cloud
6-way smoke preview
```

EFX-3 범위:

```txt
1. 실제 raymarch는 최소 steps로만 구현한다.
2. LoL MVP에서는 default off.
3. EFX-5에서 6-way lit smoke와 volumetric custom을 강화한다.
```

Texture slots:

```txt
t0 DensityNoise3D 또는 packed 2D noise
t1 SixWay0
t2 SixWay1
t3 ColorOverLife
s0 LinearWrap
```

---

## 4. Register 규칙

모든 신규 FX shader는 `space0`을 명시한다.

예:

```hlsl
cbuffer CBPerFrame : register(b0, space0)
{
    row_major matrix g_matViewProj;
    float3 g_vCameraPos;
    float g_fTime;
};

Texture2D g_MainMask : register(t0, space0);
SamplerState g_LinearWrap : register(s1, space0);
```

검증:

```powershell
rg "register\\([^\\)]*space0" Shaders/FX/v2
rg "register\\([^\\)]*\\)" Shaders/FX/v2
```

두 결과를 비교해서 모든 register가 space0를 가진다.

---

## 5. Renderer 구현 순서

### EFX3-1. Sprite renderer

입력:

```txt
FxRenderSnapshot.spriteInstances
quad vertex/index static buffer
material instance bytes
texture handles
```

출력:

```txt
DrawIndexedInstanced(6, instanceCount)
```

완료 기준:

```txt
[ ] DXIL compile
[ ] DX12 PSO creation
[ ] Irelia Q mark 1회 출력
[ ] additive blend
[ ] depth read/no write
```

### EFX3-2. Trail renderer

입력:

```txt
FxRenderSnapshot.trailInstances
point list or expanded dynamic vertex buffer
```

1차 구현:

```txt
CPU expanded dynamic vertex buffer
```

2차 구현:

```txt
GPU expansion or indirect draw
```

완료 기준:

```txt
[ ] Yasuo Q slash 출력
[ ] Irelia E beam 출력
[ ] UV scroll 동작
```

### EFX3-3. Mesh renderer

입력:

```txt
FxRenderSnapshot.meshInstances
model handle
material instance
```

1차 구현:

```txt
기존 CModel/CTexture cache를 사용하되, Draw path는 IRHICommandList로 분리한다.
```

주의:

```txt
CFxStaticMeshRenderer의 DX11 native path를 그대로 v2에 복사하지 않는다.
```

완료 기준:

```txt
[ ] Irelia E blade mesh 출력
[ ] Ezreal R missile mesh 출력
[ ] alpha clip / erode threshold 동작
```

### EFX3-4. Light/Decal renderer

EFX-3 말미 또는 EFX-4 이후로 둔다.

```txt
Light
  Point light spawn request로 시작. 실제 GI가 아니라 explicit dynamic light.

Decal
  Ground decal은 Particle_Generic quad로 먼저 처리.
  Deferred decal은 RenderGraph 안정 후 별도.
```

---

## 6. RHI API 확장안

필요 public API:

```cpp
class IRHIDevice
{
public:
    virtual RHIBufferHandle CreateBuffer(const RHIBufferDesc& desc, const void* pInitData) = 0;
    virtual void DestroyBuffer(RHIBufferHandle handle) = 0;

    virtual RHITextureHandle CreateTexture(const RHITextureDesc& desc, const void* pInitData) = 0;
    virtual void DestroyTexture(RHITextureHandle handle) = 0;

    virtual RHIShaderHandle CreateShader(const RHIShaderDesc& desc) = 0;
    virtual void DestroyShader(RHIShaderHandle handle) = 0;

    virtual RHISamplerHandle CreateSampler(const RHISamplerDesc& desc) = 0;
    virtual void DestroySampler(RHISamplerHandle handle) = 0;
};
```

추가 desc:

```txt
RHIShaderDesc
  stage
  target
  bytecode
  debugName

RHISamplerDesc
  filter
  addressU/V/W
  comparison
```

주의:

```txt
RHIHandle 패턴은 이미 있으므로 새 Fx 전용 handle을 만들지 않는다.
```

---

## 7. Material cbuffer layout

`PerMaterialParticle`는 16 byte alignment를 강제한다.

```cpp
struct FxCBMaterialParticle
{
    Vec4 vTintColor;
    Vec4 vEmissionColor;
    Vec4 vEdgeColor;
    Vec4 vFresnelColor;

    Vec4 vUvPanDistort;
    Vec4 vDissolveContrast;
    Vec4 vAtlas;
    Vec4 vSoftParticle;
};
```

의미:

```txt
vUvPanDistort
  x = panU
  y = panV
  z = distortionStrength
  w = noiseScale

vDissolveContrast
  x = dissolveThreshold
  y = edgeWidth
  z = contrast
  w = emissionIntensity
```

완료 기준:

```txt
[ ] static_assert(sizeof(FxCBMaterialParticle) % 16 == 0)
[ ] HLSL cbuffer와 C++ layout field 순서 문서화
[ ] .wmi parameter가 cbuffer bytes로 pack된다
```

---

## 8. LoL 첫 visual targets

```txt
Irelia
  Q mark
  W spin
  E beam
  E blade
  R pulse

Yasuo
  Q slash
  WindWall

Ezreal
  Q projectile head/trail
```

각 target은 다음 4 변주 테스트를 가진다.

```txt
Fire
Ice
Poison
Void
```

변주는 texture 교체 없이 `.wmi` parameter만 바꾼다.

---

## 9. 검증

Grep:

```powershell
rg "ID3D11|ID3D12|CDX11Device|RHI/DX11" Engine/Public/FX/v2 Engine/Private/FX/v2
rg "GetNativeHandle" Engine/Private/FX/v2
rg "register\\([^\\)]*space0" Shaders/FX/v2
```

기대:

```txt
Engine/Public/FX/v2 native hit 0
Engine/Private/FX/v2 native hit 0
GetNativeHandle hit 0
모든 shader register space0 명시
```

Build:

```txt
Engine Debug|x64
Engine Debug-DX12|x64
Client Debug|x64
```

Smoke:

```txt
DX12 path
  Irelia Q mark 1회 출력
  Particle_Generic additive blend 확인

DX11 legacy path
  기존 CFxSystem path 유지
  asset fallback이 legacy로 돌아갈 때 크래시 없음
```

완료 기준:

```txt
[ ] DX12 PSO 3개 생성
[ ] Particle renderer 출력
[ ] Trail renderer 출력
[ ] Mesh renderer 출력
[ ] 기존 LoL direct smoke regression 0
```

