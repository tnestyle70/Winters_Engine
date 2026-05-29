# EffectTool 구현 계획 인덱스

작성일: 2026-05-07
상태: 신규 TODO 계획 묶음
권위 문서:
- `.md/plan/EffectTool/17_NIAGARA_FULL_REWRITE_MASTER.md`
- `.md/plan/EffectTool/27_AAA_VFX_INSIGHTS_AND_MASTER_MATERIALS_BAKE.md`
- `.md/plan/EffectTool/28_DX12_MIGRATION_FX_INTEGRATION_BAKE.md`

목적:
- DX12 이주가 끝나는 즉시 LoL 이펙트 제작에 들어갈 수 있도록 EffectTool 구현 순서를 코드베이스 기준으로 고정한다.
- 그레이스케일 텍스처를 데이터로 쓰고, 색/움직임/라이팅은 Master Material HLSL과 Material Instance 파라미터가 만드는 구조를 1차 워크플로우로 삼는다.
- LoL 스킬 FX를 먼저 깎되, Elden 액션RPG의 보스 장판, 검기, 볼류메트릭, 6-way smoke, 대량 GPU 파티클까지 이어질 수 있게 RHI와 데이터 모델을 막아두지 않는다.

---

## 0. CLAUDE.md 기준 구조 파악

`CLAUDE.md`의 2026-05-07 운영 기준은 다음 4 트랙으로 정리된다.

```txt
Track 1 Graphics
  DX11 PBR/GGX/SSAO 일부 반영. RHI facade 통과 중이며 DX11 native bridge 잔여가 있다.

Track 2 RHI
  DX12 Scaffold가 존재한다. Engine Debug-DX12 config, CDX12Device, D3D12MA, DX12SmokeHost가 있다.
  현재 DX12는 clear/present와 handle 초기 뼈대 중심이고, 실 렌더 PSO/BindGroup/CommandList는 아직 얕다.

Track 3 Fiber JobSystem
  FiberShell은 들어갔지만 CFiberPool, WaitForCounter yield, worker slot 안정화는 별도 트랙이다.

Track 4 Gameplay
  Phase 7 Champion ECS/Scene split은 거의 완료됐다.
  FX hook은 챔피언 모듈로 이동했고, 이제 EffectTool 자산화로 흡수할 수 있는 상태다.
```

EffectTool 쪽 권위는 17번 마스터이고, 27번과 28번이 5/7 기준으로 중요한 정정을 추가했다.

```txt
17
  Niagara급 System / Emitter / Module / Script 모델과 EFX-0~9 전체 로드맵.

27
  노드 그래프보다 Master Material 3종과 Material Instance 워크플로우를 우선한다.
  그레이스케일 텍스처는 색이 아니라 마스크/노이즈/디졸브/흐름 데이터다.

28
  DX12 primary, DX11 legacy maintenance.
  DXC, D3D12MA, Root Signature, ImGui DX12 backend, GPU compute 순서를 RHI 트랙과 맞춘다.
```

본 TODO 묶음은 17/27/28을 코드베이스 실행 순서로 내린다.

---

## 1. 현재 코드베이스 기준선

실측한 현재 FX/RHI 파일:

```txt
Engine/Public/FX/FxAsset.h
Engine/Private/FX/FxAsset.cpp
Engine/Public/FX/ParameterMap.h
Engine/Public/FX/ParticlePool.h
Engine/Public/ECS/Components/FxInstanceComponent.h

Client/Public/GameObject/FX/FxBillboardComponent.h
Client/Public/GameObject/FX/FxBeamComponent.h
Client/Public/GameObject/FX/FxRibbonComponent.h
Client/Public/GameObject/FX/FxMeshComponent.h
Client/Public/GameObject/FX/LegacyFxAdapter.h
Client/Private/GameObject/FX/LegacyFxAdapter.cpp
Client/Public/GameObject/FX/FxSystem.h
Client/Private/GameObject/FX/FxSystem.cpp
Client/Public/GameObject/FX/FxBeamSystem.h
Client/Private/GameObject/FX/FxBeamSystem.cpp
Client/Public/GameObject/FX/FxMeshSystem.h
Client/Private/GameObject/FX/FxMeshSystem.cpp

Shaders/FxSprite.hlsl
Shaders/FxMesh.hlsl

Engine/Public/RHI/IRHIDevice.h
Engine/Public/RHI/IRHICommandList.h
Engine/Public/RHI/RHIDescriptors.h
Engine/Public/RHI/ShaderCompiler.h
Engine/Private/RHI/ShaderCompiler.cpp
Engine/Private/RHI/DX12/*
```

핵심 사실:

```txt
1. Engine FX v1은 이미 FxAssetHandle, CFxAssetRegistry, FxEmitterDesc, CFxParameterMap, CParticlePool을 가진다.
2. .wfx 로더는 현재 Engine/Private/FX/FxAsset.cpp의 수동 string parser다.
3. Client FX runtime은 FxBillboard/FxBeam/FxRibbon/FxMesh 컴포넌트를 World에 직접 넣는다.
4. LegacyFxAdapter가 이미 legacy component <-> FxAsset 변환을 일부 제공한다.
5. 챔피언 preset callsite는 약 100개다. 11개 챔피언 FxPresets 파일이 직접 CFxSystem::Spawn 또는 CFxMeshSystem::Spawn을 호출한다.
6. 현재 렌더 경로는 CPlaneRenderer / CFxStaticMeshRenderer를 통해 DX11 native handle과 BlendStateCache에 기대고 있다.
7. ShaderCompiler는 DXC/DXIL, SPIR-V, FXC/DXBC 대상이 이미 있다.
8. IRHICommandList에는 DrawIndexedIndirect, Dispatch, TransitionResource가 있으나 DX12 구현 상당수는 no-op 또는 초기 구현이다.
9. DX12CommandList의 SetPipeline, SetBindGroup, SetVertexBuffer, SetIndexBuffer, TransitionResource는 실 구현이 필요하다.
10. DX12PipelineState는 현재 desc 보관 중심이고 실제 ID3D12PipelineState 생성 경로가 필요하다.
```

따라서 EffectTool의 첫 구현은 신규 대형 그래프가 아니다. 기존 FX v1을 자산으로 흡수하고, DX12 RHI의 실 렌더 기초를 보강한 뒤, Master Material 3종으로 LoL FX를 깎는 순서가 맞다.

---

## 2. 구현 문서 묶음

```txt
00_EFFECT_TOOL_IMPLEMENTATION_INDEX.md
  전체 인덱스, CLAUDE.md 구조 파악, 순서 고정.

01_CURRENT_CODEBASE_AUDIT_AND_ENTRY_GATES.md
  현재 코드 실측, 막힌 RHI/FX 지점, 단계별 진입 관문.

02_EFX0_LEGACY_BRIDGE_AND_ASSETIZATION.md
  11 챔피언 preset과 직접 spawn callsite를 manifest/.wfx/.wmi로 흡수하는 계획.

03_EFX1_WFX_WMI_SCHEMA_AND_ROUNDTRIP.md
  수동 parser를 structured JSON reader/writer로 교체하고 canonical round-trip을 만든다.

04_EFX2_RUNTIME_SOA_AND_ECS_SYSTEMS.md
  World-owned FxSystemInstanceStorage, SoA DataSet, FxTick/FxSnapshot 시스템.

05_EFX3_DX12_MASTER_MATERIAL_RENDERER.md
  DX12 primary Master Material 3종, RootSignature/PSO, Sprite/Trail/Mesh renderer.

06_EFX4_EDITOR_PREVIEW_AND_HOT_RELOAD.md
  기존 EffectTuner를 Material Instance/Preview/Curve/Save/Reload 워크플로우로 확장.

07_EFX5_GPU_COMPUTE_DATA_INTERFACE_AND_ELDEN_PATH.md
  GPU compute, DataInterface 6종, Elden 대비 볼류메트릭/6-way/대량 파티클.

08_VALIDATION_SMOKE_AND_FREEZE_RULES.md
  빌드, grep, smoke, 성능, 시각 검증 명령과 freeze 규칙.
```

---

## 3. 실행 순서

권장 순서:

```txt
S0. 기준선 고정
  - Debug|x64 / Debug-DX12|x64 빌드 확인
  - 기존 Irelia/Yasuo/Ezreal FX visual smoke 기준 이미지 확보
  - DX12SmokeHost clear/present 재확인

EFX-0. Legacy bridge + preset assetization
  - 직접 spawn callsite를 분류하고 manifest 작성
  - LegacyFxAdapter를 이용해 초기 .wfx/.wmi dump
  - behavior preserving 원칙으로 기존 spawn은 유지하면서 asset 경로를 병행 검증

EFX-1. Structured asset layer
  - 수동 string parser 제거 준비
  - .wfx system/emitter/renderer schema
  - .wmi material instance schema
  - canonical save/load/save semantic equality

EFX-2. Runtime storage
  - FxInstanceComponent는 POD handle만 유지
  - CFxSystemInstanceStorage는 CWorld owned
  - FxSpawnRequestSystem, FxTickSystem, FxRenderSnapshotSystem 순서 고정

RHI-G0. DX12 실 렌더 기초 보강
  - CreateBuffer/CreateTexture/CreateShader/CreateSampler public RHI 확장
  - DX12 SetPipeline/SetBindGroup/SetVB/SetIB/TransitionResource 실 구현
  - DX12PipelineState가 실제 ID3D12PipelineState를 생성

EFX-3. Master Material renderer
  - M_VFX_Particle_Generic
  - M_VFX_Trail
  - M_VFX_Volumetric
  - LoL 이펙트는 Particle/Trail 중심으로 먼저 제작

EFX-4. Editor preview + hot reload
  - Client ImGui panel에서 시작
  - DX12 Editor 별도 exe는 RHI 안정 후 분리

EFX-5. GPU compute + Elden path
  - Track 2 W10-13 visual parity 이후 진입
```

---

## 4. 첫 구현 타겟

LoL 첫 타겟은 Irelia가 아니라 `M_VFX_Particle_Generic`의 범용성을 확인하기 쉬운 세트로 잡는다.

```txt
Target A: Irelia Q mark / W spin / E beam
  이유: 현재 FX가 잘 보이고, billboard + mesh + beam 세 경로를 모두 건드린다.

Target B: Yasuo Q slash / WindWall
  이유: trail/mesh/ground decal 조합 검증에 좋다.

Target C: Ezreal Q projectile
  이유: projectile 이동, head/glow/trail 조합과 네트워크 event applier 경로를 확인할 수 있다.
```

1차 완료 기준:

```txt
1. 위 3개 챔피언의 핵심 FX가 .wfx + .wmi asset 경로로 스폰된다.
2. 기존 legacy direct spawn 경로와 시각 결과가 크게 달라지지 않는다.
3. DX11 legacy path와 DX12 primary path에서 같은 .wfx/.wmi를 읽는다.
4. Master Material 파라미터만 바꿔 fire/ice/poison/void 4개 변주가 가능하다.
```

---

## 5. 금지 사항

```txt
1. EFX-0에서 직접 spawn callsite를 한 번에 모두 제거하지 않는다.
2. Engine/Public/FX/v2에서 ID3D11, ID3D12, CDX11Device를 include하지 않는다.
3. Scene_InGame 전용 함수나 champion-specific 상수를 Engine FX에 넣지 않는다.
4. 노드 그래프 에디터를 Master Material 3종보다 먼저 만들지 않는다.
5. DX12 RHI command/pipeline/bind 구현이 no-op인 상태로 EFX-3 visual parity를 합격 처리하지 않는다.
6. 수동 string parser를 확장해서 .wfx v2를 만들지 않는다. structured JSON reader/writer로 교체한다.
```

---

## 6. 다음 세션 시작 명령

```txt
1. 본 TODO 묶음 00 -> 08 순서로 읽기
2. .md/plan/EffectTool/17_NIAGARA_FULL_REWRITE_MASTER.md §0.1, §13 다시 읽기
3. .md/plan/EffectTool/27_AAA_VFX_INSIGHTS_AND_MASTER_MATERIALS_BAKE.md §0.1, §1 읽기
4. .md/plan/EffectTool/28_DX12_MIGRATION_FX_INTEGRATION_BAKE.md §1, §4, §5 읽기
5. 02_EFX0_LEGACY_BRIDGE_AND_ASSETIZATION.md부터 코드 진입
```
