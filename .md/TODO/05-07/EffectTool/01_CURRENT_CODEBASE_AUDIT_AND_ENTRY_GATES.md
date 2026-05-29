# EffectTool 현재 코드 감사와 진입 관문

작성일: 2026-05-07
상태: 신규 TODO 계획
출처:
- `CLAUDE.md`
- `.md/process/PLAN_AUTHORING_PITFALLS.md`
- `.md/plan/EffectTool/17_NIAGARA_FULL_REWRITE_MASTER.md`
- `.md/plan/EffectTool/27_AAA_VFX_INSIGHTS_AND_MASTER_MATERIALS_BAKE.md`
- `.md/plan/EffectTool/28_DX12_MIGRATION_FX_INTEGRATION_BAKE.md`

목적:
- EffectTool 구현 전에 실제 코드베이스의 FX/RHI 기준선을 고정한다.
- 박제 함정 P-1, P-2, P-3, P-9, P-10, P-18, P-19를 먼저 차단한다.

---

## 1. 실측 파일 목록

FX v1 Engine:

```txt
Engine/Public/FX/FxAsset.h
Engine/Private/FX/FxAsset.cpp
Engine/Public/FX/ParameterMap.h
Engine/Private/FX/ParticlePool.cpp
Engine/Public/FX/ParticlePool.h
Engine/Private/FX/DeterministicRandom.cpp
Engine/Public/FX/DeterministicRandom.h
Engine/Public/ECS/Components/FxInstanceComponent.h
```

FX v1 Client:

```txt
Client/Public/GameObject/FX/FxBillboardComponent.h
Client/Public/GameObject/FX/FxBeamComponent.h
Client/Public/GameObject/FX/FxRibbonComponent.h
Client/Public/GameObject/FX/FxMeshComponent.h
Client/Public/GameObject/FX/FxSystem.h
Client/Private/GameObject/FX/FxSystem.cpp
Client/Public/GameObject/FX/FxBeamSystem.h
Client/Private/GameObject/FX/FxBeamSystem.cpp
Client/Public/GameObject/FX/FxMeshSystem.h
Client/Private/GameObject/FX/FxMeshSystem.cpp
Client/Public/GameObject/FX/LegacyFxAdapter.h
Client/Private/GameObject/FX/LegacyFxAdapter.cpp
Client/Private/GameObject/FX/WindWallSystem.cpp
Client/Private/GameObject/FX/UltWaveSystem.cpp
```

FX 셰이더:

```txt
Shaders/FxSprite.hlsl
Shaders/FxMesh.hlsl
```

RHI:

```txt
Engine/Public/RHI/RHIHandles.h
Engine/Public/RHI/RHIDescriptors.h
Engine/Public/RHI/RHICapabilities.h
Engine/Public/RHI/IRHIDevice.h
Engine/Public/RHI/IRHICommandList.h
Engine/Public/RHI/IRHIPipelineState.h
Engine/Public/RHI/IRHIBindGroup.h
Engine/Public/RHI/ShaderCompiler.h
Engine/Private/RHI/ShaderCompiler.cpp
Engine/Private/RHI/DX12/DX12Device.cpp
Engine/Private/RHI/DX12/DX12CommandList.cpp
Engine/Private/RHI/DX12/DX12PipelineState.cpp
Engine/Private/RHI/DX12/DX12BindGroup.cpp
```

Editor seed:

```txt
Client/Public/UI/EffectTuner.h
Client/Private/UI/EffectTuner.cpp
Client/Private/Scene/InGameDebugBridge.cpp
```

---

## 2. 현재 FX 구조

Engine v1 asset:

```txt
FxAsset
  handle
  strName
  emitters
  initialUserParams

FxEmitterDesc
  renderType
  maxParticles
  spawnRate
  hMaterial / hMeshGeometry
  blendMode
  strTexturePath / strModelPath
  transform, velocity, scale, color
  lifetime, fade, atlas, uv scroll, alpha clip, erode threshold

CFxAssetRegistry
  Register
  RegisterOrReplaceByName
  LoadFromFile
  ReloadFromFile
  LoadDirectory
```

중요한 현재 한계:

```txt
1. FxAsset.cpp의 .wfx parser는 structured JSON parser가 아니다.
2. emitter block 추출, string/number 추출이 key 검색 기반이라 중첩 구조와 canonical round-trip에 약하다.
3. FxEmitterDesc는 legacy renderer의 필드를 많이 담고 있지만 Material Instance 개념이 없다.
4. initialUserParams는 있지만 30~50개 master material parameter layout과 cbuffer byte layout이 없다.
```

Client v1 runtime:

```txt
FxBillboardComponent
  texturePath, width/height, atlas, uv scroll, color, blend, lifetime

FxBeamComponent / FxRibbonComponent
  start/end, points, width, texture, fade, uv scroll

FxMeshComponent
  modelPath, texturePath, scale/rotation/spin, alpha/erode/depthWrite

CFxSystem
  Update/Render billboard
  Spawn / SpawnFromAsset

CFxBeamSystem
  Update/Render beam/ribbon
  SpawnFromAsset

CFxMeshSystem
  Update/Render mesh
  SpawnFromAsset
```

중요한 현재 한계:

```txt
1. CFxSystem::Create는 DX11Shader, DX11Pipeline, CBlendStateCache를 직접 요구한다.
2. FxSystem.cpp는 GetNativeDX11Device/GetNativeDX11Context 형태의 native bridge를 쓴다.
3. FxBeamSystem.cpp는 CPlaneRenderer를 segment renderer로 사용한다.
4. FxMeshSystem.cpp는 CFxStaticMeshRenderer에 의존한다.
5. Render path가 World query를 직접 수행한다.
6. Render snapshot 분리가 없다.
```

---

## 3. 현재 RHI 구조

이미 있는 좋은 기반:

```txt
RHIHandle<TTag>
  index 32 + generation 32 typed handle.

RHIDescriptors
  RHIPipelineDesc
  RHIRenderPassDesc
  RHIBindGroupLayoutDesc
  RHIBindGroupDesc
  RHIBufferDesc
  RHITextureDesc
  RHIIndirectDrawIndexedArgs

IRHICommandList
  Draw
  DrawIndexed
  DrawIndexedIndirect
  Dispatch
  UpdateBuffer
  TransitionResource

CShaderCompiler
  DXIL, SPIRV, DXBC target.
  DXC dynamic load path가 이미 존재한다.

RHICapabilities
  supportsCompute, supportsAsyncCompute, supportsBindless, requiresExplicitResourceStates.
```

막힌 지점:

```txt
1. IRHIDevice public interface에 CreateBuffer/CreateTexture/CreateShader/CreateSampler가 없다.
2. RHIBufferDesc/RHITextureDesc는 있지만 실제 public creation API가 없다.
3. DX12CommandList의 SetPipeline/SetBindGroup/SetVertexBuffer/SetIndexBuffer/TransitionResource가 no-op다.
4. DX12PipelineState는 RHIPipelineDesc 보관만 하고 실제 PSO 생성이 필요하다.
5. DX12BindGroup의 GetNativeHandle은 nullptr이다.
6. DX12 RootSignature builder는 내부 파일로 있지만 public RHI contract에 뿌리내린 상태는 아니다.
7. DX12 BeginFrame은 clear/present smoke용 경로라 일반 render pass와 별도 command recording이 필요하다.
```

EffectTool은 이 RHI 구멍을 피해갈 수 없다. EFX-3 진입 전에 `RHI-G0` 관문을 반드시 통과한다.

---

## 4. 현재 호출 경로

직접 FX spawn callsite는 약 100개다.

```txt
Client/Private/GameObject/Champion/Annie/Annie_FxPresets.cpp
Client/Private/GameObject/Champion/Ashe/Ashe_FxPresets.cpp
Client/Private/GameObject/Champion/Ezreal/Ezreal_FxPresets.cpp
Client/Private/GameObject/Champion/Fiora/Fiora_FxPresets.cpp
Client/Private/GameObject/Champion/Garen/GarenFxPresets.cpp
Client/Private/GameObject/Champion/Irelia/IreliaFxPresets.cpp
Client/Private/GameObject/Champion/Jax/Jax_FxPresets.cpp
Client/Private/GameObject/Champion/Kalista/KalistaFxPresets.cpp
Client/Private/GameObject/Champion/Riven/RivenFxPresets.cpp
Client/Private/GameObject/Champion/Yasuo/YasuoFxPresets.cpp
Client/Private/GameObject/Champion/Zed/ZedFxPresets.cpp

Client/Private/GameObject/FX/UltWaveSystem.cpp
Client/Private/Network/Client/EventApplier.cpp
Client/Private/Network/Client/SnapshotApplier.cpp
```

EFX-0은 이 callsite를 한 번에 갈아엎지 않는다. 먼저 manifest와 asset dump를 만든 뒤, 위험이 낮은 Irelia/Yasuo/Ezreal부터 asset 경로를 병행한다.

---

## 5. 관문 S0

EFX-0 코드 진입 전:

```txt
[ ] Engine Debug|x64 빌드
[ ] Client Debug|x64 빌드
[ ] Engine Debug-DX12|x64 빌드
[ ] DX12SmokeHost clear/present 8초 생존
[ ] Irelia FX direct local visual 기준 캡처
[ ] Yasuo FX direct local visual 기준 캡처
[ ] Ezreal FX direct local visual 기준 캡처
[ ] `rg "ID3D11|CDX11Device|RHI/DX11" Engine/Public/FX Engine/Private/FX` 현 상태 기록
[ ] `rg "CFxSystem::Spawn\\(|CFxMeshSystem::Spawn\\(" Client/Private/GameObject/Champion` 현 상태 기록
```

S0 실패 시 EffectTool 신규 코드는 시작하지 않는다. 먼저 DX12/RHI 또는 champion smoke 기준선을 고정한다.

---

## 6. 관문 RHI-G0

EFX-3 진입 전 필수:

```txt
[ ] IRHIDevice에 buffer/texture/shader/sampler creation API 추가
[ ] DX11 backend에서 신규 API legacy 구현
[ ] DX12 backend에서 신규 API D3D12MA 기반 구현
[ ] DX12CommandList SetPipeline 실 구현
[ ] DX12CommandList SetBindGroup 실 구현
[ ] DX12CommandList SetVertexBuffer / SetIndexBuffer 실 구현
[ ] DX12CommandList TransitionResource 실 구현
[ ] DX12PipelineState가 graphics/compute PSO 생성
[ ] RHIBindGroupLayoutDesc -> DX12 RootSignature 생성 경로 확정
[ ] ShaderCompiler DXIL path로 Master Material VS/PS 컴파일 통과
```

이 관문 전에는 EFX-0/EFX-1/EFX-2까지는 진행할 수 있다. EFX-3 visual renderer는 진행하지 않는다.

---

## 7. 관문 EFX-0

```txt
[ ] 11개 champion FxPresets 파일 callsite manifest 작성
[ ] LegacyFxAdapter로 billboard/mesh asset dump 성공
[ ] Irelia Q/W/E/R 핵심 emitter asset 이름 확정
[ ] Yasuo Q/W/E/R 핵심 emitter asset 이름 확정
[ ] Ezreal Q/W/E/R 핵심 emitter asset 이름 확정
[ ] 기존 direct spawn과 asset spawn을 같은 visual smoke에서 비교
[ ] behavior-preserving 원칙 위반 없음
```

EFX-0 합격 기준은 `CFxSystem::Spawn` 0 hit가 아니다. 기존 호출을 보존하면서 asset 경로가 같은 결과를 낼 수 있음을 확인하는 것이다.

---

## 8. 관문 EFX-1

```txt
[ ] structured JSON reader/writer 추가
[ ] .wfx v1 schema 문서와 코드 동시 반영
[ ] .wmi v1 schema 문서와 코드 동시 반영
[ ] 기존 FxAsset.cpp 수동 parser가 v2 path에 재사용되지 않음
[ ] load -> canonical save -> load semantic equality
[ ] canonical writer의 float precision, key order, path slash 정책 확정
```

---

## 9. 관문 EFX-2

```txt
[ ] FxInstanceComponent는 POD handle component 유지
[ ] CFxSystemInstanceStorage는 CWorld owned
[ ] FxSpawnRequestSystem phase 0
[ ] FxTickSystem phase 5
[ ] FxRenderSnapshotSystem phase 6
[ ] Renderer는 snapshot만 read
[ ] Render path에서 ECS World query 직접 호출 금지 경로 확보
[ ] 1024 particle / 16 emitter CPU tick budget 측정
```

---

## 10. 관문 EFX-3

```txt
[ ] RHI-G0 완료
[ ] Shaders/FX/v2/Master/* 생성
[ ] 모든 FX shader register에 space0 명시
[ ] M_VFX_Particle_Generic DXIL 컴파일
[ ] M_VFX_Trail DXIL 컴파일
[ ] M_VFX_Volumetric DXIL 컴파일
[ ] Particle/Trail/Mesh renderer가 IRHICommandList만 사용
[ ] Engine/Public/FX/v2에서 ID3D11/ID3D12 hit 0
[ ] DX12 path에서 Irelia Q mark 1회 출력
```

---

## 11. 관문 EFX-4

```txt
[ ] 기존 EffectTuner를 Irelia 전용에서 EffectTool panel로 확장
[ ] .wfx open/save
[ ] .wmi open/save
[ ] Material Instance core knob 슬라이더
[ ] Preview spawn/reset
[ ] Shader compile log 표시
[ ] DX11 ImGui fallback 유지
[ ] DX12 ImGui backend 진입 계획 확정
```

---

## 12. 관문 EFX-5

```txt
[ ] GPU buffer/UAV 생성 API 안정
[ ] compute root signature 또는 bind group contract 확정
[ ] Particle update compute shader DXIL 컴파일
[ ] 8192 particle / 64 emitter GPU tick 측정
[ ] CPU fallback 결과와 basic invariant 비교
[ ] Elden 6-way smoke sample asset 출력
[ ] Elden boss telegraph sample asset 출력
```

