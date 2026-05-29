# EFX-2 Runtime SoA와 ECS 시스템

작성일: 2026-05-07
상태: 구현 계획
의존:
- `03_EFX1_WFX_WMI_SCHEMA_AND_ROUNDTRIP.md`
- `.md/plan/EffectTool/19_RUNTIME_LAYER_BAKE.md`

목적:
- legacy component를 직접 renderer가 읽는 구조에서, simulation과 render snapshot을 분리한 runtime으로 간다.
- FxInstanceComponent는 POD handle만 유지한다.
- 실제 instance storage와 SoA buffer는 CWorld 수명에 묶는다.

---

## 1. 현재 기준선

현재 Engine component:

```txt
Engine/Public/ECS/Components/FxInstanceComponent.h
```

현재 내용:

```txt
FxAssetHandle hAsset
EntityHandle hAttachTo
Vec3 vAttachOffset
fAge / fLifetime / bLoop
array pool indices 4
```

현재 Client component:

```txt
FxBillboardComponent
FxBeamComponent
FxRibbonComponent
FxMeshComponent
```

현재 문제:

```txt
1. Client component가 render와 simulation 필드를 모두 가진다.
2. Render system이 World query를 직접 한다.
3. CParticlePool은 있지만 System/Emitter/Instance 레벨 storage가 없다.
4. GPU path로 이어질 buffer abstraction이 없다.
```

---

## 2. 결정

```txt
1. FxInstanceComponent에 unique_ptr 또는 runtime object를 넣지 않는다.
2. CFxSystemInstanceStorage는 CWorld owned다.
3. FxAssetRegistry는 CGameInstance Tier-1 owned를 유지한다.
4. Simulation phase와 Render extraction phase를 분리한다.
5. Renderer는 FxRenderSnapshot만 읽는다.
6. DX11 legacy component renderer는 EFX-2 동안 adapter로 보존한다.
```

---

## 3. 신규 파일

```txt
Engine/Public/FX/v2/Runtime/FxSystemInstanceHandle.h
Engine/Public/FX/v2/Runtime/FxSpawnRequestComponent.h
Engine/Public/FX/v2/Runtime/FxSystemInstance.h
Engine/Public/FX/v2/Runtime/FxEmitterInstance.h
Engine/Public/FX/v2/Runtime/FxDataSet.h
Engine/Public/FX/v2/Runtime/FxParameterStore.h
Engine/Public/FX/v2/Runtime/FxSystemInstanceStorage.h
Engine/Public/FX/v2/Runtime/FxRenderSnapshot.h

Engine/Private/FX/v2/Runtime/FxSystemInstance.cpp
Engine/Private/FX/v2/Runtime/FxEmitterInstance.cpp
Engine/Private/FX/v2/Runtime/FxDataSet.cpp
Engine/Private/FX/v2/Runtime/FxParameterStore.cpp
Engine/Private/FX/v2/Runtime/FxSystemInstanceStorage.cpp

Engine/Public/FX/v2/Systems/FxSpawnRequestSystem.h
Engine/Public/FX/v2/Systems/FxTickSystem.h
Engine/Public/FX/v2/Systems/FxRenderSnapshotSystem.h

Engine/Private/FX/v2/Systems/FxSpawnRequestSystem.cpp
Engine/Private/FX/v2/Systems/FxTickSystem.cpp
Engine/Private/FX/v2/Systems/FxRenderSnapshotSystem.cpp
```

Client adapter:

```txt
Client/Public/GameObject/FX/FxV2LegacyBridge.h
Client/Private/GameObject/FX/FxV2LegacyBridge.cpp
```

---

## 4. Handle와 component

Handle:

```cpp
struct FxSystemInstanceTag {};
using FxSystemInstanceHandle = RHIHandle<FxSystemInstanceTag>;
```

Request component:

```cpp
struct FxSpawnRequestComponent
{
    FxAssetHandle hAsset{};
    wstring_t strAssetPath;
    Vec3 vWorldPos{};
    Vec3 vWorldEulerXYZ{};
    EntityID attachTo = NULL_ENTITY;
    Vec3 vAttachOffset{};
    f32_t fAutoDestroyAfterSec = 5.f;
    bool_t bLoop = false;
};
```

Runtime component:

```cpp
struct FxInstanceComponent
{
    FxAssetHandle hAsset{};
    FxSystemInstanceHandle hInstance{};
    EntityHandle hAttachTo = NULL_ENTITY_HANDLE;
    Vec3 vAttachOffset{};
    f32_t fAge = 0.f;
    f32_t fLifetime = 3.f;
    bool_t bLoop = false;
};
```

주의:

```txt
기존 FxInstanceComponent를 한 번에 깨지 않는다.
필요하면 `FxInstanceComponentV2`로 시작한 뒤 EFX-2 말에 교체한다.
```

---

## 5. DataSet

기본 SoA column:

```txt
Position       Vec3
Velocity       Vec3
Color          Vec4
Age            f32_t
Lifetime       f32_t
Size           Vec2
Rotation       f32_t
MaterialRandom f32_t
UV             Vec2
CustomFloat4   8 slots
CustomUInt     4 slots
```

Runtime class:

```cpp
class CFxDataSet final
{
public:
    void Initialize(u32_t uMaxInstances);
    void Reset();
    u32_t Spawn();
    void KillSwapBack(u32_t uIndex);
    void TickAge(f32_t fDeltaTime);
    u32_t GetNumInstances() const;
};
```

첫 구현은 기존 `CParticlePool`을 감싸거나 복제하지 말고, `CParticlePool`의 컬럼 설계를 참조해서 v2 `CFxDataSet`을 만든다. v2에는 renderer instance buffer로 보낼 자료형을 기준으로 column을 명확히 둔다.

---

## 6. ParameterStore

역할:

```txt
1. System / Emitter / Particle / User namespace 파라미터 저장
2. Material Instance cbuffer bytes와 runtime override 연결
3. Editor slider 값 반영
4. Hot reload 시 rebind
```

기본 구조:

```cpp
class CFxParameterStore final
{
public:
    void SetFloat(FxParameterID id, f32_t value);
    void SetFloat4(FxParameterID id, const Vec4& value);
    bool_t TryGetFloat(FxParameterID id, f32_t& outValue) const;
    bool_t TryGetFloat4(FxParameterID id, Vec4& outValue) const;

    const std::vector<u8_t>& GetMaterialBytes() const;
    void RebuildMaterialBytes(const CFxMaterialInstance& material);
};
```

---

## 7. ECS phase

현재 scheduler는 같은 phase에서 conflict 없는 system을 병렬 실행할 수 있다. Producer/Consumer는 반드시 다른 정수 phase를 사용한다.

EFX-2 phase:

```txt
Phase 0
  FxSpawnRequestSystem
  request component 소비, CFxSystemInstanceStorage Acquire

Phase 5
  FxTickSystem
  CPU tick, lifetime, attach follow, kill 처리

Phase 6
  FxRenderSnapshotSystem
  World/runtime storage를 읽어 render POD snapshot 생성

Phase 7
  Renderer bridge
  snapshot read-only
```

주의:

```txt
1. FxSpawnRequestSystem과 FxTickSystem을 같은 phase에 두지 않는다.
2. FxRenderSnapshotSystem은 FxTickSystem 뒤 phase여야 한다.
3. Renderer는 ECS World query를 하지 않는다.
```

---

## 8. World ownership

목표:

```txt
CWorld
  owns CFxSystemInstanceStorage

CGameInstance
  owns CFxAssetRegistry
  owns CFxMasterMaterialRegistry
```

구현 방식 후보:

```txt
Option A
  CWorld에 generic extension storage 추가.

Option B
  Scene/BootstrapBridge가 storage unique_ptr를 소유하고 systems에 raw pointer 주입.

EFX-2 선택
  Option B로 시작한다.
  이유: CWorld public ABI를 덜 건드리고, EFX-2 smoke가 빠르다.
  EFX-4 이후 World extension이 필요하면 별도 migration.
```

Storage init:

```cpp
struct FxRuntimeContext
{
    CFxSystemInstanceStorage* pStorage = nullptr;
    CFxAssetRegistry* pAssetRegistry = nullptr;
};
```

Scene bootstrap은 `FxRuntimeContext`를 시스템 생성 시 주입한다.

---

## 9. Legacy bridge

EFX-2 기간에는 두 runtime이 공존한다.

```txt
Legacy path
  FxBillboardComponent / FxBeamComponent / FxRibbonComponent / FxMeshComponent
  CFxSystem / CFxBeamSystem / CFxMeshSystem

V2 path
  FxSpawnRequestComponent
  FxInstanceComponentV2
  CFxSystemInstanceStorage
  FxRenderSnapshot
```

Bridge 역할:

```txt
1. EFX-0 asset spawn을 v2 request로 보낼 수 있다.
2. v2 renderer가 준비되지 않은 render type은 legacy component로 fallback 생성한다.
3. EFX-3 이후 fallback을 줄인다.
```

---

## 10. 구현 단계

### EFX2-1. Storage와 handle

완료 기준:

```txt
[ ] FxSystemInstanceHandle 추가
[ ] CFxSystemInstanceStorage Acquire/Release
[ ] generation stale handle 검증
[ ] Scene lifecycle에서 storage reset
```

### EFX2-2. Spawn request

완료 기준:

```txt
[ ] FxSpawnRequestComponent 추가
[ ] FxSpawnRequestSystem phase 0
[ ] request 소비 후 component 제거
[ ] asset path 또는 handle 모두 지원
```

### EFX2-3. CPU tick

완료 기준:

```txt
[ ] CFxEmitterInstance CPU tick
[ ] burst spawn 1회
[ ] spawn rate accumulation
[ ] age/lifetime kill
[ ] attachTo follow
```

### EFX2-4. Snapshot extraction

완료 기준:

```txt
[ ] FxRenderSnapshot POD 생성
[ ] Sprite/Mesh/Ribbon/Beam snapshot bucket 분리
[ ] renderer가 snapshot read-only로 출력할 수 있는 데이터 형태 확정
```

### EFX2-5. Legacy fallback

완료 기준:

```txt
[ ] unsupported renderer type은 legacy component spawn fallback
[ ] fallback 발생 로그 1회 제한
[ ] EFX-3에서 줄일 수 있도록 metrics counter 추가
```

---

## 11. 성능 기준

1차 CPU 기준:

```txt
1024 particles
16 emitters
1 frame tick 0.5 ms 이하
snapshot extraction 0.25 ms 이하
allocation per frame 0
```

Profiler counter:

```txt
FxV2::ActiveSystems
FxV2::ActiveEmitters
FxV2::ActiveParticles
FxV2::SpawnRequests
FxV2::LegacyFallbacks
FxV2::KilledParticles
```

---

## 12. 검증

Grep:

```powershell
rg "World\\.ForEach<.*Fx" Engine/Private/FX/v2 Engine/Public/FX/v2
rg "ID3D11|ID3D12|CDX11Device" Engine/Public/FX/v2 Engine/Private/FX/v2
rg "GetPhase\\(\\).*return 5|GetPhase\\(\\).*return 6" Engine/Private/FX/v2
```

Smoke:

```txt
1. Spawn request로 Irelia Q mark 1회 생성
2. 1초 뒤 auto destroy
3. legacy fallback count 확인
4. profiler overlay counter 확인
```

완료 기준:

```txt
[ ] Engine Debug build
[ ] Client Debug build
[ ] direct local smoke
[ ] ActiveParticles가 lifetime 후 0으로 복귀
[ ] per-frame allocation hotspot 없음
```

