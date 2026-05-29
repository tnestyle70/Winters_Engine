# EFX-5 GPU Compute, DataInterface, Elden 대비 경로

작성일: 2026-05-07
상태: 구현 계획
의존:
- `05_EFX3_DX12_MASTER_MATERIAL_RENDERER.md`
- `06_EFX4_EDITOR_PREVIEW_AND_HOT_RELOAD.md`
- Track 2 DX12 W10-13 visual parity

목적:
- LoL 런타임 MVP 이후 Elden 액션RPG급 이펙트로 확장할 GPU compute와 DataInterface를 설계한다.
- DX12 primary path에서 대량 파티클, indirect draw, 6-way smoke, volumetric custom을 처리한다.
- DX11은 legacy fallback으로 유지하되, GPU feature의 합격 기준은 DX12로 둔다.

---

## 1. 진입 조건

```txt
[ ] EFX-3 Particle/Trail/Mesh renderer DX12 출력
[ ] EFX-4 .wfx/.wmi edit-preview-save-reload
[ ] DX12 buffer/texture/shader/sampler RHI API 안정
[ ] DX12 UAV/SRV transition 안정
[ ] DrawIndexedIndirect 구현
[ ] Dispatch 구현
[ ] ShaderCompiler DXIL compute compile 통과
```

EFX-5는 Track 2 W10-13 visual parity 이후 진입한다.

---

## 2. GPU compute 목표

1차:

```txt
8192 particles
64 emitters
1 frame GPU tick 1.5 ms 이하
CPU fallback 동일 asset 가능
```

2차:

```txt
32768 particles
indirect draw
GPU sort 또는 approximate bucket sort
async compute queue 검토
```

---

## 3. 신규 파일

GPU:

```txt
Engine/Public/FX/v2/GPU/FxGpuComputeDispatch.h
Engine/Public/FX/v2/GPU/FxGpuBufferLayout.h
Engine/Public/FX/v2/GPU/FxGpuTickTypes.h

Engine/Private/FX/v2/GPU/FxGpuComputeDispatch.cpp
Engine/Private/FX/v2/GPU/FxGpuBufferLayout.cpp

Shaders/FX/v2/Compute/FxParticleUpdate_CS.hlsl
Shaders/FX/v2/Compute/FxParticleSpawn_CS.hlsl
Shaders/FX/v2/Compute/FxBuildDrawArgs_CS.hlsl
```

DataInterface:

```txt
Engine/Public/FX/v2/DataInterface/IFxDataInterface.h
Engine/Public/FX/v2/DataInterface/FxDICurve.h
Engine/Public/FX/v2/DataInterface/FxDITexture.h
Engine/Public/FX/v2/DataInterface/FxDIStaticMesh.h
Engine/Public/FX/v2/DataInterface/FxDISpline.h
Engine/Public/FX/v2/DataInterface/FxDIGrid2D.h
Engine/Public/FX/v2/DataInterface/FxDICollisionQuery.h

Engine/Private/FX/v2/DataInterface/FxDICurve.cpp
Engine/Private/FX/v2/DataInterface/FxDITexture.cpp
Engine/Private/FX/v2/DataInterface/FxDIStaticMesh.cpp
Engine/Private/FX/v2/DataInterface/FxDISpline.cpp
Engine/Private/FX/v2/DataInterface/FxDIGrid2D.cpp
Engine/Private/FX/v2/DataInterface/FxDICollisionQuery.cpp

Shaders/FX/v2/DataInterface/FxDI_Curve.hlsli
Shaders/FX/v2/DataInterface/FxDI_Texture.hlsli
Shaders/FX/v2/DataInterface/FxDI_StaticMesh.hlsli
Shaders/FX/v2/DataInterface/FxDI_Spline.hlsli
Shaders/FX/v2/DataInterface/FxDI_Grid2D.hlsli
Shaders/FX/v2/DataInterface/FxDI_CollisionQuery.hlsli
```

Elden samples:

```txt
Client/Bin/Resource/FX/Elden/Spell/StarRain.wfx
Client/Bin/Resource/FX/Elden/Spell/BossTelegraph_Ring.wfx
Client/Bin/Resource/FX/Elden/Smoke/SixWay_SmokeColumn.wfx
Client/Bin/Resource/FX/Elden/Trail/SwordArc_Heavy.wfx
```

---

## 4. GPU buffer layout

Particle buffer:

```txt
StructuredBuffer / RWStructuredBuffer

PositionAge
  xyz = world position
  w = age

VelocityLifetime
  xyz = velocity
  w = lifetime

ColorRandom
  rgba = color or packed random data

SizeRotation
  x = sizeX
  y = sizeY
  z = rotation
  w = materialRandom

Custom0..N
  graph/DI custom payload
```

Draw args:

```txt
RHIIndirectDrawIndexedArgs
  indexCountPerInstance
  instanceCount
  startIndexLocation
  baseVertexLocation
  startInstanceLocation
```

Barriers:

```txt
Phase 5 compute
  UAV write

Phase 6 snapshot or args build
  UAV barrier

Phase 7 render
  UAV -> SRV transition
```

---

## 5. Compute stages

### EFX5-1. ParticleUpdate

Inputs:

```txt
Particle buffer UAV
Emitter parameter CBV
DeltaTime
Random seed
```

Outputs:

```txt
updated age
updated position
kill flag
```

완료 기준:

```txt
[ ] gravity/drag update
[ ] age kill
[ ] 1024 particles compute update
```

### EFX5-2. ParticleSpawn

Inputs:

```txt
free list or append counter
spawn count
emitter shape parameters
```

Outputs:

```txt
new particles in UAV buffer
active count
```

1차 단순화:

```txt
CPU가 spawn count와 base index를 계산하고 GPU update만 수행한다.
```

2차:

```txt
GPU append/free list.
```

### EFX5-3. BuildDrawArgs

Inputs:

```txt
active particle count
renderer topology
```

Outputs:

```txt
RHIIndirectDrawIndexedArgs buffer
```

완료 기준:

```txt
[ ] DrawIndexedIndirect path로 sprite 출력
```

---

## 6. DataInterface 6종

### Curve

용도:

```txt
Color over life
Alpha over life
Size over life
```

CPU/GPU:

```txt
CPU: control point interpolation
GPU: baked 1D texture 또는 structured buffer sample
```

### Texture

용도:

```txt
grayscale mask
noise
flowmap
6-way lighting texture
```

정책:

```txt
1. Source는 PNG 가능.
2. Cooked는 DDS BC4/BC5/BC7로 간다.
3. R-only grayscale은 BC4 target.
4. RG flowmap은 BC5 target.
```

### StaticMesh

용도:

```txt
mesh surface spawn
blade/sword arc mesh sampling
Elden boss mesh emit
```

1차:

```txt
CPU surface point sample.
```

2차:

```txt
GPU triangle buffer sample.
```

### Spline

용도:

```txt
trail path
projectile path
spell ribbon
```

### Grid2D

용도:

```txt
LoL ground AoE distribution
influence map 연동 가능성
```

### CollisionQuery

용도:

```txt
Elden projectile ground collision
smoke floor contact
soft particle 보조
```

주의:

```txt
CollisionQuery는 Engine Physics Stage와 연결 전까지 임시 성공값을 내지 말고 explicit unsupported fallback을 낸다.
```

---

## 7. Elden sample targets

### StarRain

요구:

```txt
GPU spawn/update
thousands particles
gravity-like fall
emissive trail
indirect draw
```

완료 기준:

```txt
[ ] 8192 particles visible
[ ] GPU tick 1.5 ms 이하
```

### BossTelegraph_Ring

요구:

```txt
ground decal
polar coordinates
dissolve in/out
stylized low-PBR unlit
```

완료 기준:

```txt
[ ] telegraph ring radius matches parameter
[ ] alpha/dissolve readable
```

### SixWay_SmokeColumn

요구:

```txt
6-way lighting texture two packed maps
simple sun direction response
soft alpha
```

완료 기준:

```txt
[ ] sun direction 변경 시 lighting response 변화
[ ] PBR material path 사용 0
```

### SwordArc_Heavy

요구:

```txt
trail material
edge emission
noise dissolve
mesh or ribbon
```

완료 기준:

```txt
[ ] slash silhouette readable
[ ] trail fade/edge emission 정상
```

---

## 8. Graph compiler 위치

EFX-5에서 전체 노드 그래프 에디터를 만들지 않는다.

우선순위:

```txt
1. Master Material static HLSL
2. Material Instance parameters
3. Fixed module CPU/GPU update
4. DataInterface 6종
5. Graph -> VM/HLSL compiler
```

이유:

```txt
LoL FX는 Master Material과 fixed modules만으로 충분히 많이 만든다.
Graph compiler는 포트폴리오 가치가 높지만, 초반 콘텐츠 생산성을 막으면 안 된다.
```

---

## 9. 검증

Grep:

```powershell
rg "RWStructuredBuffer|AppendStructuredBuffer|ConsumeStructuredBuffer" Shaders/FX/v2/Compute
rg "DrawIndexedIndirect" Engine/Private/RHI Engine/Private/FX/v2
rg "TransitionResource" Engine/Private/FX/v2 Engine/Private/RHI/DX12
rg "PBR|Roughness|Metallic" Shaders/FX/v2
```

기대:

```txt
FX compute shader has UAV usage.
DX12 RHI implements indirect draw.
FX shaders do not depend on PBR material model.
```

성능:

```txt
LoL CPU baseline
  1024 particles / 16 emitters / 0.5 ms

DX12 GPU target
  8192 particles / 64 emitters / 1.5 ms

Elden stress
  32768 particles / visual stress, not gameplay target
```

완료 기준:

```txt
[ ] DX12 compute path visible
[ ] CPU fallback visible
[ ] StarRain sample
[ ] SixWay smoke sample
[ ] Boss telegraph sample
[ ] Sword arc sample
```
