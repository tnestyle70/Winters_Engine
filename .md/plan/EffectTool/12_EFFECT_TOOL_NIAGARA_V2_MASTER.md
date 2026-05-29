# Phase G v2 — Effect Tool Master (Niagara급 + Elden Ring + Multi-Game 통합)

**작성일**: 2026-05-04
**상태**: ⚠️ **superseded by** [`17_NIAGARA_FULL_REWRITE_MASTER.md`](17_NIAGARA_FULL_REWRITE_MASTER.md). v2 는 Stage 순서/초기 설계 참고용.
**v1 폐기 / 보강 대상**:
- [`00_EFFECT_TOOL_PLAN_INDEX.md`](00_EFFECT_TOOL_PLAN_INDEX.md) (Stage 순서 v2 로 갱신)
- [`02~08_STAGE*.md`](02_STAGE1_GRAPH_DATA_MODEL.md) (Stage 순서 변경)
- [`07_STAGE6_NODE_EDITOR_IMGUI.md`](07_STAGE6_NODE_EDITOR_IMGUI.md) (Stage 6 → Stage 4 로 앞당김 — Codex 권고)
**Codex 검토 박제**: [`11_EFFECT_TOOL_NIAGARA_ELDEN_V2_REVIEW.md`](11_EFFECT_TOOL_NIAGARA_ELDEN_V2_REVIEW.md) (6 결함)
**선행**: NEXTGEN_FRAMEWORK_MASTER (rev 2) — ECS v2 / Fiber / RG / GPU Driven 인프라
**관련**: [`WINTERS_MULTIGAME_ARCHITECTURE.md`](../../architecture/WINTERS_MULTIGAME_ARCHITECTURE.md) — Multi-Game 비전 (LoL + Elden + Class & Servant)
**가이드**: [`PLAN_AUTHORING_PITFALLS.md`](../../process/PLAN_AUTHORING_PITFALLS.md)

---

## §0. 비전

**목표**: **Unreal Niagara급 노드 그래프 이펙트 시스템** + **엘든링 VFX 라이브러리** + **Class & Servant 차세대 게임 지원**. 단일 시스템이 LoL 스킬 이펙트 (간단 빌보드) ~ Elden Ring 보스 마법진 (Ribbon + Decal + Beam + Mesh + Compute) ~ Class & Servant Hybrid PvPvE 효과 (실시간 보스 telegraph + MOBA 챔프 스킬 + Servant 소환 효과) 모두 커버.

**한 줄 핵심**:
> 기존 hardcoded `IreliaFxPresets` 같은 챔프별 C++ 헬퍼를 **데이터-드리븐 `.wfx` 자산**으로 전환하고, **Niagara급 ParameterMap + Multi-Render Type (Billboard/Ribbon/Beam/Decal/Mesh) + Effect Tool Scene** 를 단일 시스템으로 박제. **모든 게임 (LoL/Elden/CS) 가 같은 시스템 사용** — `eGameProduct` 분기 없이 자산만 분리.

---

## §1. Codex 검토 6 결함 정정 매트릭스 (v1 → v2)

| # | v1 결함 | v2 정정 | 사유 |
|---|---|---|---|
| 1 | `Engine/Public/FX/` 신규 시스템을 **greenfield** 박제 — 기존 [FxSystem.h](../../../Client/Public/GameObject/FX/FxSystem.h) / [FxBillboardComponent](../../../Client/Public/GameObject/FX/FxBillboardComponent.h) / IreliaFxPresets 와 **이중화** | **Stage 0 Legacy Bridge** 신설 — `LegacyFxAdapter` + `FxAssetFromPreset` 변환 계층. 기존 Irelia/Yasuo/Annie preset 을 새 자산 시스템으로 자동 변환 | greenfield 폐기 = 점진 마이그 |
| 2 | 단순 DAG 노드만 박제 — Niagara 의 핵심 **ParameterMap** 없음 | **5 namespace ParameterMap** 박제: `System.*` / `Emitter.*` / `Particle.*` / `User.*` / `Event.*`. 노드 입출력이 namespace 통해 양방향 흐름 | Niagara 핵심 기능 |
| 3 | Stage 5 Rendering 이 **billboard 중심** — Elden 검기/장판/회피잔상/마법진 부족 | **Stage 3 Render Multi-Type 앞당김**: Billboard + **Ribbon/Trail + Beam + Ground Decal + Mesh Particle + Shockwave Ring** 동시 박제 | Elden VFX 라이브러리 prerequisite |
| 4 | Editor (Stage 6) 너무 늦음 — JSON 손편집으로 Elden VFX 못 버팀 | **Stage 4 Effect Tool Scene 앞당김**: `Scene_EffectTool` + Preview Viewport + Inspector + Timeline + Asset Browser + Hot Reload. **제품 독립** (Irelia 전용 EffectTuner 폐기) | 작업 효율 |
| 5 | 코드 일부가 현 ECS API 와 미스매치 — `world.GetStore<T>()` (없음), `ENGINE_DLL` (잘못된 매크로), `"Entity.h"` flat (AGENTS.md 위반) | 현재 [World.h:89](../../../Engine/Public/ECS/World.h:89) `ForEach/GetComponent/HasComponent` 중심 사용. **`WINTERS_ENGINE` + `"ECS/Entity.h"` + `"ECS/World.h"`** 컨벤션 통일 | PITFALLS P-8/P-13/P-18 회피 |
| 6 | Raw `const wchar_t* texturePath` / `const char* modelPath` ([FxBillboardComponent:18](../../../Client/Public/GameObject/FX/FxBillboardComponent.h:18) / [FxMeshComponent:24](../../../Client/Public/GameObject/FX/FxMeshComponent.h:24)) — Tool 저장/로드/핫리로드 위험 | **`FxAssetHandle` / `TextureHandle` / `ModelHandle` + string table** 전환. RHIHandle 패턴 (index 32 + generation 32) 차용 | ECS Generation 패턴 일관 |

---

## §2. v2 Stage 로드맵 (8 Stage)

| Stage | 이름 | 내용 | 의존 |
|---|---|---|---|
| **EFX-0** | **Legacy Bridge** | `LegacyFxAdapter` (FxBillboard/FxMesh → FxAsset) + `FxAssetFromPreset` (Irelia/Yasuo/Annie/Yone preset 자산화) + `EffectTuner` 호환 | (기존 코드만) |
| **EFX-1** | **FxAsset + ParameterMap + .wfx JSON** | 5 namespace (System/Emitter/Particle/User/Event) + 자산 포맷 + 핸들 (`FxAssetHandle`) | EFX-0 |
| **EFX-2** | **CPU ParticlePool SoA + Deterministic** | SoA particle store + deterministic RNG (xoroshiro128) + CommandBuffer 기반 spawn/kill (worker-safe) | EFX-1 |
| **EFX-3** | **Multi-Render Type MVP** | Billboard + **Ribbon/Trail + Beam + Ground Decal + Mesh Particle + Shockwave Ring** 6 타입 동시 | EFX-2 |
| **EFX-4** | **Effect Tool Scene + Editor MVP** | `Scene_EffectTool` (제품 독립) + Preview Viewport + Inspector + Timeline + Asset Browser + Hot Reload | EFX-3 |
| **EFX-5** | **Node Executor + Expression VM** | DAG 노드 컴파일 + 바이트코드 스택 머신 + ParameterMap 통합 | EFX-1, EFX-2 |
| **EFX-6** | **Elden VFX Pack 검증** | 보스 telegraph / sword trail / shockwave / magic circle / lingering field 6 작품 | EFX-3, EFX-4, EFX-5 |
| **EFX-7** | **GPU Compute + Indirect Draw** | RHI/RG 안정 후 진입. Compute Cull + IndirectDraw (GPU Driven 통합) | EFX-3 + RHI/RG 안정 + Fiber M3 |

**핵심 변경**:
- Editor (EFX-4) 가 v1 의 Stage 6 → **v2 의 Stage 4** 로 앞당김
- Render Multi-Type (EFX-3) 이 v1 의 Stage 5 (billboard only) → **v2 의 Stage 3 (6 타입)** 로 앞당김
- Legacy Bridge (EFX-0) **신설**
- GPU (EFX-7) 가 v1 의 Stage 7 → 끝까지 보류 (RHI/RG 안정 후)

---

## §3. EFX-0 Legacy Bridge — 기존 코드 자산화

### 3.1 현재 구조 (실측)

```cpp
// Client/Public/GameObject/FX/FxBillboardComponent.h L18
const wchar_t* texturePath = nullptr;   // ★ raw — Tool 부적합
// 26 멤버: pos / attachTo / velocity / 텍스처 / 색상 / 페이드 / 아틀라스 / UV / 블렌드 / 수명 / 회전

// Client/Public/GameObject/FX/FxMeshComponent.h L24-26
const char* modelPath = nullptr;
const wchar_t* texturePath = nullptr;
// material 멤버: vColor / blendMode / alphaClip / fade / UV scroll / erode

// Client/Private/UI/EffectTuner.cpp L29-32
"BA Slash", "Q Trail", "Q Mark", "W Spin (atlas)",
"W Stage2 Slash", "E Beam (Mesh)", "R Pulse"
// ★ Irelia 7 프리셋 하드코딩 — 제품 독립 X
```

### 3.2 LegacyFxAdapter (신규)

**파일**: `Client/Public/GameObject/FX/LegacyFxAdapter.h` (신규)

```cpp
#pragma once
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxMeshComponent.h"
#include "FX/FxAsset.h"   // EFX-1 신규

namespace LegacyFx
{
    // 기존 FxBillboardComponent 인스턴스 → 자산화 FxAsset 으로 변환.
    // 주로 IreliaFxPresets / YasuoFxPresets 등의 hardcoded preset 마이그용.
    FxAssetHandle FxAssetFromBillboard(const FxBillboardComponent& src,
                                        const char* pszAssetName);

    // 기존 FxMeshComponent 인스턴스 → 자산화.
    FxAssetHandle FxAssetFromMesh(const FxMeshComponent& src,
                                   const char* pszAssetName);

    // 자산 → 기존 FxBillboard/Mesh 컴포넌트 (역방향 — Spawn 시 호환)
    FxBillboardComponent BillboardFromAsset(FxAssetHandle handle,
                                             const Vec3& worldPos);
    FxMeshComponent      MeshFromAsset(FxAssetHandle handle,
                                       const Vec3& worldPos);

    // Irelia preset 7 종 → .wfx 자산 7 개로 일괄 변환 (마이그 helper).
    void RegisterIreliaPresetAssets();
    void RegisterYasuoPresetAssets();
    // ... 챔프 추가 시 헬퍼 추가
}
```

### 3.3 마이그 흐름

```
[v1 호출 패턴 — 그대로 동작]
IreliaFx::SpawnQTrail(world, owner, target);
    ↓ 내부 구현이 변경됨
    ↓ 기존: FxBillboardComponent tmpl; tmpl.texturePath = L"...";
    ↓ v2:   FxAssetHandle h = LegacyFx::g_IreliaQTrailAsset;
    ↓        CFxSystem::SpawnFromAsset(world, h, owner, target);

[v2 호출 패턴 — 신규]
CFxSystem::SpawnFromAsset(world, asset, owner, target);
    ↓ FxAsset 안의 emitter / parameter map 사용
```

기존 game code (Scene_InGame / hooks) **0 변경**. 프리셋 헬퍼 (IreliaFx::SpawnQTrail) 의 내부 구현만 자산 사용으로 교체.

---

## §4. EFX-1 FxAsset + ParameterMap + .wfx JSON

### 4.1 ParameterMap — Niagara 핵심

**5 namespace 박제**:

```cpp
// Engine/Public/FX/ParameterMap.h
enum class eFxNamespace : u8_t
{
    System,    // 매 frame 갱신, 모든 emitter 공유 (예: System.DeltaTime, System.WorldTime)
    Emitter,   // emitter 단위, 같은 emitter 의 모든 particle 공유 (예: Emitter.SpawnRate, Emitter.Position)
    Particle,  // 개별 particle 단위 (SoA 컬럼) (예: Particle.Position, Particle.Velocity)
    User,      // gameplay 코드가 set (예: User.BossPhase, User.PlayerHP)
    Event,     // 이벤트 payload (예: Event.HitNormal, Event.HitDamage)
};

struct FxParameterID
{
    eFxNamespace ns;
    u32_t        nameHash;   // fnv-1a 32-bit
    eFxParameterType type;   // float, vec3, vec4, int, color, ...
};

class CFxParameterMap
{
public:
    template<typename T> void   Set(eFxNamespace ns, std::string_view name, const T& v);
    template<typename T> T      Get(eFxNamespace ns, std::string_view name, const T& fallback = {}) const;
    bool                        Has(eFxNamespace ns, std::string_view name) const;
};
```

### 4.2 사용 예시 — Elden 보스 phase 변경 시 모든 effect 자동 갱신

```cpp
// Gameplay 코드 (BossAI 시스템)
world.GetFxParameterMap().Set(eFxNamespace::User, "BossPhase", 2);

// Effect 안의 노드 (데이터 드리븐)
//   GetUserParam("BossPhase") → if (== 2) → SpawnRate × 3.0
//                                  → ColorTint = red

// → 보스 페이즈 변경 시 모든 emitter 자동 반응. C++ 코드 변경 0.
```

### 4.3 FxAsset 자산 포맷

**파일**: `Engine/Public/FX/FxAsset.h`

```cpp
#pragma once
#include "RHI/RHIHandles.h"
#include "FX/ParameterMap.h"
#include <vector>

struct FxAssetTag {};
using FxAssetHandle = RHIHandle<FxAssetTag>;   // ★ RHIHandle 패턴 차용 (P-18 회피)

enum class eFxRenderType : u8_t
{
    Billboard,
    Ribbon,         // EFX-3
    Beam,           // EFX-3
    GroundDecal,    // EFX-3
    MeshParticle,   // EFX-3
    ShockwaveRing,  // EFX-3
};

struct FxEmitterDesc
{
    std::string                    strName;
    eFxRenderType                  renderType = eFxRenderType::Billboard;
    u32_t                          maxParticles = 1024;
    f32_t                          spawnRate = 60.f;       // particles/sec
    std::vector<FxNodeDesc>        nodes;                  // EFX-5 의 DAG
    RHITextureHandle               hMaterial;              // ★ raw path X — Handle
    RHIBufferHandle                hMeshGeometry;          // MeshParticle 용
    eBlendPreset                   blendMode = eBlendPreset::AlphaBlend;
};

struct FxAsset
{
    FxAssetHandle              handle{};
    std::string                strName;
    std::vector<FxEmitterDesc> emitters;
    CFxParameterMap            initialUserParams;   // .wfx 에 박제된 default User.* 값
};

class CFxAssetRegistry
{
public:
    FxAssetHandle Register(FxAsset asset);
    const FxAsset* Find(FxAssetHandle h) const;
    void          UnregisterAll();   // hot-reload 용

    // .wfx JSON 파일 로드
    FxAssetHandle LoadFromFile(const std::wstring& path);
    bool          ReloadFromFile(FxAssetHandle h);   // ★ hot-reload (Tool 핵심)
};
```

### 4.4 .wfx JSON 포맷 예시

```json
{
  "name": "Irelia_Q_Trail",
  "user_params": {
    "TrailLength": 1.5,
    "TintColor": [0.4, 0.7, 1.0, 1.0]
  },
  "emitters": [
    {
      "name": "Trail_Ribbon",
      "render_type": "Ribbon",
      "max_particles": 64,
      "spawn_rate": 30.0,
      "material": "Texture/FX/Irelia/q_trail_mask.png",
      "blend_mode": "Additive",
      "nodes": [
        { "type": "SpawnRate", "rate": "Emitter.SpawnRate" },
        { "type": "InitPosition", "value": "Emitter.Position" },
        { "type": "InitColor",    "value": "User.TintColor" },
        { "type": "UpdateLifetime", "delta": "System.DeltaTime" }
      ]
    }
  ]
}
```

---

## §5. EFX-2 CPU ParticlePool SoA + Deterministic

### 5.1 SoA Storage (Engine 통합)

```cpp
// Engine/Public/FX/ParticlePool.h
class CParticlePool
{
public:
    void   Initialize(u32_t maxParticles);
    u32_t  Spawn();                    // returns particle index (or kMaxU32 = full)
    void   KillSwapBack(u32_t idx);    // O(1) — 마지막 파티클과 swap

    // SoA 컬럼 — Particle.* namespace 의 컴포넌트
    Vec3*  GetPosColumn();
    Vec3*  GetVelocityColumn();
    Vec4*  GetColorColumn();
    f32_t* GetLifetimeColumn();
    f32_t* GetAgeColumn();
    f32_t* GetSizeColumn();
    Vec2*  GetUVColumn();              // 아틀라스 frame
    u32_t* GetCustomIntColumn(u32_t slot);   // 사용자 정의 int 슬롯 4개
    Vec4*  GetCustomVec4Column(u32_t slot);  // vec4 슬롯 8개

    u32_t  GetActiveCount() const;
    u32_t  GetCapacity() const;

private:
    u32_t              m_uActiveCount = 0;
    u32_t              m_uCapacity = 0;
    std::vector<Vec3>  m_vecPos;
    std::vector<Vec3>  m_vecVelocity;
    std::vector<Vec4>  m_vecColor;
    std::vector<f32_t> m_vecLifetime;
    std::vector<f32_t> m_vecAge;
    // ...
};
```

**원리**: SoA = Structure of Arrays = 각 attribute 가 별도 vector. SIMD 친화 + cache locality 우수.

### 5.2 Deterministic RNG

```cpp
// Engine/Public/FX/DeterministicRandom.h
class CXoroshiro128
{
public:
    void Seed(u64_t s);
    u64_t Next();
    f32_t NextFloat();      // [0, 1)
    f32_t NextRange(f32_t min, f32_t max);
private:
    u64_t m_state[2];
};
```

**왜 deterministic?** Class & Servant 의 PvPvE 멀티 + 서버 권위 검증 시 effect 시뮬레이션이 deterministic 해야 server/client 일치 가능. **RNG 시드를 emitter 단위로 보관 → 같은 시드 = 같은 결과**.

### 5.3 Worker-Safe Spawn — CommandBuffer 통합

```cpp
// EFX-2 의 spawn API
class CFxSystem
{
public:
    // 즉시 spawn (메인 스레드만)
    EntityID SpawnFromAsset(CWorld& world, FxAssetHandle h,
                             const Vec3& pos, EntityID attachTo = NULL_ENTITY);

    // 지연 spawn (worker thread 안전 — CommandBuffer 사용)
    void DeferSpawnFromAsset(CCommandBuffer& cb, FxAssetHandle h,
                              const Vec3& pos, EntityID attachTo = NULL_ENTITY);
};
```

ECS Generation v1 의 `CCommandBuffer` (`mutex` worker-safe) 통합. Fiber JobSystem 안에서 effect spawn 시 race 0.

---

## §6. EFX-3 Multi-Render Type — 6 타입

Elden Ring VFX 라이브러리의 **prerequisite**. Niagara 가 모두 갖춘 6 타입 병행 박제.

### 6.1 Billboard (기존 — FxBillboardComponent 마이그)

기존 [FxBillboardComponent](../../../Client/Public/GameObject/FX/FxBillboardComponent.h) 의 26 멤버 그대로 + `FxAssetHandle` 추가. 가장 단순.

### 6.2 Ribbon / Trail — 검기 / 회피 잔상

**원리**: spawn 된 particle 들을 **시간 순서로 연결한 quad strip**. 머리 (head) 가 emitter 위치 따라가고, 꼬리 (tail) 가 particle 수명에 따라 fade.

```cpp
class CFxRibbonRenderer
{
public:
    void RenderEmitter(IRHICommandList* pCmd, const FxEmitterDesc& emitter,
                       const CParticlePool& pool, const Mat4& viewProj);
    // 알고리즘:
    //  1. pool 의 active particle 들을 age 기준 정렬 (timestamp)
    //  2. 인접 particle 쌍 → quad (4 vertex)
    //  3. UV: V = age / lifetime (꼬리로 갈수록 1.0)
    //  4. dynamic VB (매 frame upload — Phase 2 에선 streaming)
};
```

**Elden 적용**: 검기 (Sword Trail) — 검 끝에 emitter 부착, 검 휘두르는 동안 particle 매 frame spawn.

### 6.3 Beam — 마법 광선 / 락온 표시

**원리**: 두 점 (start / end) 사이를 잇는 quad. UV scroll 으로 흐름 효과.

```cpp
struct FxBeamComponent
{
    EntityID hStart = NULL_ENTITY;       // start 추종 entity
    EntityID hEnd   = NULL_ENTITY;
    Vec3     vStartOffset{};
    Vec3     vEndOffset{};
    f32_t    fWidth = 0.5f;
    f32_t    fUVScrollSpeed = 1.f;
    RHITextureHandle hTexture;
};
```

**Elden 적용**: 보스 magic ray, lock-on indicator, magic missile trail.

### 6.4 Ground Decal — 보스 장판 / 마법진

**원리**: 지면 위에 평면 텍스처 투영. depth-aware (고저차 있어도 자연스럽게 적용). projector matrix 또는 simple plane.

```cpp
struct FxGroundDecalComponent
{
    Vec3   vWorldPos{};        // 중심
    f32_t  fRadius = 1.f;
    f32_t  fYaw = 0.f;
    Vec4   vColor{1, 1, 1, 1};
    RHITextureHandle hTexture;
    f32_t  fGrowDuration = 0.5f;   // 등장 시 0 → fRadius 보간
    f32_t  fFadeOutDuration = 0.3f;
};
```

**Elden 적용**: 보스 telegraph (붉은 장판 = 곧 폭발), magic circle, AoE 표시 (Class & Servant 의 챔프 ult 사거리).

### 6.5 Mesh Particle — 검 파편 / 돌가루

**원리**: particle 마다 mesh instance. 회전 + 크기 + 위치 SoA. **GPU instancing** (DX11 `DrawIndexedInstanced`).

```cpp
struct FxMeshParticleEmitter
{
    RHIBufferHandle  hMeshVB;
    RHIBufferHandle  hMeshIB;
    u32_t            iIndexCount;
    RHITextureHandle hTexture;
    // particle attribute (SoA): position / rotation (quat) / scale / color
};
```

**Elden 적용**: 검 파편 (검기 시), 돌가루 (보스 도약 후 착지), 영혼 잔상.

### 6.6 Shockwave Ring — 보스 도약 충격파

**원리**: 중심에서 외부로 확장하는 ring. radius = `fStartRadius + fAge × fExpansionSpeed`. quad 의 inner/outer 반경에 따라 fade.

```cpp
struct FxShockwaveComponent
{
    Vec3   vWorldPos{};
    f32_t  fStartRadius = 0.f;
    f32_t  fEndRadius = 10.f;
    f32_t  fDuration = 1.f;
    f32_t  fAge = 0.f;
    f32_t  fThickness = 0.2f;
    Vec4   vColor{1, 1, 1, 1};
    RHITextureHandle hTexture;
};
```

**Elden 적용**: 보스 도약 후 착지 충격파 (Margit, Godrick), 폭발 충격파.

**Class & Servant 적용**: 보스 telegraph 와 결합 — telegraph 후 실제 폭발 시점에 shockwave.

---

## §7. EFX-4 Effect Tool Scene — 제품 독립 Editor

### 7.1 v1 의 Irelia 전용 EffectTuner 폐기

```cpp
// 현재 (Client/Private/UI/EffectTuner.cpp L29-32, L69):
const char* kPresetNames[] = {
    "BA Slash", "Q Trail", "Q Mark", ...   // ★ Irelia 7 hardcode
};
ImGui::Begin("Effect Tuner — Irelia")      // ★ 제품 종속
```

→ 이걸 **EFX-6 Elden Pack 검증** + **Class & Servant** 까지 확장 불가능. 제품 독립 Scene 으로 **분리**.

### 7.2 Scene_EffectTool (신규)

**파일**: `Client/Public/Scene/Scene_EffectTool.h`

```cpp
#pragma once
#include "IScene.h"

class CScene_EffectTool : public IScene
{
public:
    static std::unique_ptr<CScene_EffectTool> Create();

    bool OnEnter() override;
    void OnExit() override;
    void OnUpdate(f32_t dt) override;
    void OnRender() override;
    void OnImGui() override;   // ★ 핵심 — 모든 패널 박제

private:
    CScene_EffectTool() = default;

    // 4 패널 (DockSpace 안)
    void DrawAssetBrowser();
    void DrawPreviewViewport();
    void DrawInspector();
    void DrawTimeline();

    // 핫리로드
    void DetectAssetChanges();   // .wfx 파일 변경 감지 → ReloadFromFile

    FxAssetHandle             m_hSelectedAsset{};
    EntityID                  m_PreviewEntity = NULL_ENTITY;
    std::vector<FxAssetHandle> m_vecLoadedAssets;
    f32_t                     m_fTimelineSec = 0.f;
    bool                      m_bPlay = true;
    bool                      m_bLoop = true;
    f32_t                     m_fPlaySpeed = 1.f;
};
```

### 7.3 4 패널

**Asset Browser** (좌측):
- `Client/Bin/Resource/FX/` 의 `.wfx` 파일 목록 트리
- 클릭 시 미리보기 viewport 에 spawn
- 우클릭 → New / Duplicate / Delete / Rename
- "Reload All" 버튼 (`CFxAssetRegistry::ReloadFromFile` 일괄)

**Preview Viewport** (중앙):
- 별도 RT 에 dynamic camera + 단일 emitter 미리보기
- 그리드 (1m 간격) + 좌표축
- 카메라 컨트롤 (마우스 우클릭 회전, WASD 이동)
- 배경: 솔리드 색 / cubemap / Elden boss arena 미리보기 모델

**Inspector** (우측 상단):
- 선택된 emitter 의 `FxEmitterDesc` 전체 슬라이더
  - render_type 콤보박스
  - max_particles / spawn_rate 슬라이더
  - blend_mode 콤보박스
  - material handle → 텍스처 picker
  - **노드 그래프 미리보기** (EFX-5 진입 후)
- ParameterMap User.* 슬라이더
- "Apply" → asset 저장 + 즉시 reload

**Timeline** (하단):
- 재생 / 일시정지 / 정지 / loop 토글
- 시간 스크럽 (0 ~ asset 의 longest emitter lifetime)
- Play Speed 슬라이더 (0.1× ~ 4×)
- Frame-by-frame stepping

### 7.4 Hot Reload — 작업 효율의 핵심

```cpp
void CScene_EffectTool::DetectAssetChanges()
{
    // Win32 ReadDirectoryChangesW 또는 매 frame 파일 mtime 비교
    for (auto h : m_vecLoadedAssets)
    {
        const auto& path = CFxAssetRegistry::Instance()->GetAssetPath(h);
        if (FileWasModified(path))
        {
            CFxAssetRegistry::Instance()->ReloadFromFile(h);
            // 미리보기 entity 가 그 asset 사용 중이면 즉시 시각 반영
        }
    }
}
```

**의의**: VFX artist 가 외부 텍스트 에디터 / 노드 에디터로 `.wfx` 편집 → 저장 → **인게임 미리보기 즉시 갱신** (재시작 0). Niagara 작업 효율의 핵심.

### 7.5 Game Select 통합

```cpp
// Multi-Game Architecture 의 Scene_GameSelect 에 4 번째 버튼:
"Editor → Effect Tool"
    ↓ Change_Scene(CScene_EffectTool::Create())
```

**모든 게임 (LoL / Elden / Class & Servant) 의 effect 가 같은 Tool 에서 작업** — 자산 폴더만 분리:
- `Data/LoL/FX/`
- `Data/Elden/FX/`
- `Data/ClassServant/FX/`

---

## §8. EFX-5 Node Executor + Expression VM

### 8.1 노드 = 순수 함수

```cpp
struct FxNodeDesc
{
    std::string         strType;        // "SpawnRate", "InitPosition", "UpdateGravity" 등
    std::vector<std::string> inputs;    // 입력 ParameterID 들
    std::vector<std::string> outputs;
    std::vector<u8_t>   bytecodeBlob;   // EFX-5 의 expression VM bytecode
};
```

### 8.2 Expression VM — 바이트코드

```cpp
enum class eFxOp : u8_t
{
    LoadParam,      // Particle.Position 같은 것 load
    StoreParam,     // 결과 저장
    ConstFloat,     // 상수
    ConstVec3,
    AddF, SubF, MulF, DivF,
    AddV3, SubV3, ...
    Length, Normalize, Cross, Dot,
    Sin, Cos, Lerp, Clamp,
    JumpIf, ...
};

class CFxExpressionVM
{
public:
    void Execute(const std::vector<u8_t>& bytecode,
                  CFxParameterMap& paramMap);
private:
    std::vector<FxValue> m_stack;
};
```

**왜 VM?** 노드 그래프를 매 frame 컴파일 X → 한 번 바이트코드 컴파일 후 매 frame 빠르게 실행. CPU 분기 최소화.

---

## §9. EFX-6 Elden VFX Pack — 검증 작품 6 종

| # | 작품 | 사용 타입 | 노드 |
|---|---|---|---|
| 1 | **Boss Telegraph** (붉은 장판 → 폭발) | GroundDecal + ShockwaveRing | Time-based color lerp + Shockwave at fAge=fTelegraphDuration |
| 2 | **Sword Trail** (검기) | Ribbon | head emitter follow weapon socket + tail fade |
| 3 | **Shockwave** (착지 충격파) | ShockwaveRing + MeshParticle (돌가루) | Single shockwave + N mesh particles upward velocity |
| 4 | **Magic Circle** (소환 마법진) | GroundDecal + Beam (수직 광선) | Decal grow + Beam grow + Particle spiral |
| 5 | **Lingering Field** (지속 데미지 영역) | GroundDecal + Billboard (불꽃 SoA) | Looping decal + Billboard particles inside circle (continuous spawn) |
| 6 | **Soul Liberation** (Yone R 스타일 / Class & Servant servant 소환) | Beam + Ribbon + MeshParticle | Vertical beam from caster + Spiral ribbon + Mesh wisps |

각 작품을 EFX-4 Tool 로 박제 + 게임 internal 테스트.

**Class & Servant 활용**:
- "Boss Telegraph" → MOBA 챔프 ult 사거리 표시 + Elden 보스 phase 시작 telegraph
- "Magic Circle" → Servant 소환 효과 (Class & Servant 핵심 요소)
- "Lingering Field" → MOBA 의 zoning 스킬 (Anivia Wall 류) + Elden 의 독 영역
- "Soul Liberation" → Yone R + 챔프 ult 트리거 + Servant 변신 효과

---

## §10. EFX-7 GPU Compute (보류)

**진입 조건**:
- ✅ EFX-1 ~ EFX-6 모두 완료 (CPU 시뮬 + 6 렌더 타입 + Tool + Elden Pack)
- ✅ NEXTGEN_FRAMEWORK_MASTER 의 RHI Compute 안정 (RHI/RG)
- ✅ Fiber JobSystem M3 (yield + wait list) 안정
- ✅ GPU Driven Pipeline (GPUScene + DrawIndexedIndirect) 안정

**왜 보류?** GPU Compute 는 디버깅 비용 매우 높음. CPU 시뮬 + DX11 instancing 으로 Elden Ring 수준 (1M+ particles 동시) 까지 충분히 가능. GPU 진입은 PvPvE 100v100 같은 극한 부하에서만 필요.

---

## §11. Multi-Game (Class & Servant) 통합

### 11.1 자산 디렉토리 분리

```
Data/
├── LoL/FX/
│   ├── Champions/
│   │   ├── Irelia/
│   │   │   ├── q_trail.wfx
│   │   │   ├── e_beam.wfx
│   │   │   └── ...
│   │   └── Yasuo/
│   └── Map/
│       └── tower_explosion.wfx
├── Elden/FX/
│   ├── Bosses/
│   │   ├── Margit/
│   │   │   ├── telegraph_redfield.wfx
│   │   │   └── shockwave_landing.wfx
│   │   └── Godrick/
│   ├── Spells/
│   │   └── magic_circle.wfx
│   └── Player/
│       └── sword_trail.wfx
└── ClassServant/FX/
    ├── Classes/
    │   ├── ClassA/
    │   └── ClassB/
    ├── Servants/
    │   └── boss_summon.wfx
    └── PvPvE/
        ├── lingering_field.wfx
        └── shockwave_burst.wfx
```

### 11.2 GameModule 별 자산 로드

```cpp
// LOLGameModule::InitializeClient
CFxAssetRegistry::Instance()->LoadDirectory(L"Data/LoL/FX/");

// EldenGameModule::InitializeClient
CFxAssetRegistry::Instance()->LoadDirectory(L"Data/Elden/FX/");

// ClassServantGameModule::InitializeClient — 양쪽 + 자체
CFxAssetRegistry::Instance()->LoadDirectory(L"Data/LoL/FX/");           // Class 가 LoL 챔프 풀 사용
CFxAssetRegistry::Instance()->LoadDirectory(L"Data/Elden/FX/");          // Servant = Elden 보스 일부
CFxAssetRegistry::Instance()->LoadDirectory(L"Data/ClassServant/FX/");   // 자체 hybrid effect
```

**Class & Servant 의 강점**: LoL FX + Elden FX **+ 자체 hybrid 자산** 모두 사용. `eGameProduct` 분기 X — 자산 핸들로만 구분.

### 11.3 ParameterMap 의 Multi-Game 활용

```cpp
// LoL — 챔프 phase
world.GetFxParameterMap().Set(eFxNamespace::User, "PlayerLevel", 18);

// Elden — 보스 phase
world.GetFxParameterMap().Set(eFxNamespace::User, "BossPhase", 2);

// Class & Servant — 양쪽 다
world.GetFxParameterMap().Set(eFxNamespace::User, "ClassPhase", 3);
world.GetFxParameterMap().Set(eFxNamespace::User, "ServantBondLevel", 5);

// Effect 안의 노드:
//   GetUserParam("ServantBondLevel") → Bond=5 시 effect 색깔/크기 강화
```

---

## §12. ECS 통합 (Multi-Game 안전)

### 12.1 Component (`FxInstanceComponent`)

```cpp
// Engine/Public/ECS/Components/FxInstanceComponent.h (신규)
struct FxInstanceComponent
{
    FxAssetHandle           hAsset{};
    EntityHandle            hAttachTo = NULL_ENTITY_HANDLE;   // ★ ECS Generation 안전
    Vec3                    vAttachOffset{};
    f32_t                   fAge = 0.f;
    f32_t                   fLifetime = 3.f;
    bool_t                  bLoop = false;

    // ParticlePool 참조 (emitter 별)
    std::array<u32_t, 4>    aPoolIndices{};   // 최대 4 emitter / asset
};
```

**ECS Generation 통합**: `EntityHandle hAttachTo` — 추적 대상 entity 가 죽어도 stale handle 자동 검출. PITFALLS P-19 (Render/Sim 결합) 회피 — Effect 시뮬 시스템이 ECS Query 로 attach 위치 추출 후 ParticlePool 갱신. 렌더 패스는 **RenderWorldSnapshot** 만 read.

### 12.2 시스템 — `CFxSimulationSystem` (Phase 11, EFX-2)

```cpp
class CFxSimulationSystem : public ISystem
{
public:
    u32_t GetPhase() const override { return 11; }   // Vision(5)/Turret(6/7)/BT(8)/MCTS(10) 후

    void DescribeAccess(CSystemAccessBuilder& b) override
    {
        b.Read<TransformComponent>()         // attach 추종
         .Write<FxInstanceComponent>()       // age 갱신
         .UnknownWritesAll();                // ParticlePool (외부 storage) — TODO: 분리
    }

    void Execute(CWorld& world, f32_t dt) override;
};
```

### 12.3 렌더 — RenderGraph 패스로 등록

```cpp
// CRenderGraph::AddPass("FxRender", setup, exec)
graph.AddPass(
    "FxRender",
    [&](CRgPassBuilder& b) { b.WriteTexture(hSwap); b.ReadBuffer(hCamera); },
    [&](CRgPassContext& ctx)
    {
        IRHICommandList* cmd = ctx.GetCommandList();

        // 6 렌더 타입 별 batch
        m_BillboardRenderer.Render(cmd, m_Snapshot.billboards, viewProj);
        m_RibbonRenderer.Render(cmd, m_Snapshot.ribbons, viewProj);
        m_BeamRenderer.Render(cmd, m_Snapshot.beams, viewProj);
        m_DecalRenderer.Render(cmd, m_Snapshot.decals, viewProj);
        m_MeshRenderer.Render(cmd, m_Snapshot.meshParticles, viewProj);
        m_ShockwaveRenderer.Render(cmd, m_Snapshot.shockwaves, viewProj);
    });
```

---

## §13. Phase 시간표

| Phase | 기간 | 내용 |
|---|---|---|
| **EFX-0** | 1 week | Legacy Bridge + Irelia/Yasuo/Annie/Yone preset → .wfx 자동 변환 |
| **EFX-1** | 2 weeks | FxAsset / ParameterMap / .wfx JSON / FxAssetHandle / Registry |
| **EFX-2** | 2 weeks | CPU ParticlePool SoA + Deterministic RNG + Worker-Safe Spawn |
| **EFX-3** | 4 weeks | Multi-Render Type 6 종 (Billboard 1주 + Ribbon 1주 + Beam 0.5주 + Decal 0.5주 + Mesh 0.5주 + Shockwave 0.5주) |
| **EFX-4** | 3 weeks | Scene_EffectTool + 4 패널 + Hot Reload + Game Select 통합 |
| **EFX-5** | 3 weeks | Node Executor + Expression VM + 노드 라이브러리 30+ |
| **EFX-6** | 2 weeks | Elden VFX Pack 6 작품 |
| **EFX-7** | (보류) | RHI/RG 안정 후 — GPU Compute + Indirect Draw |

**총 ~17 weeks** (약 4 개월). RHI/RG 안정 완료 후 진입 권장. 차세대 게임 framework (NEXTGEN_FRAMEWORK_MASTER §1 1~7 단계) 의 6 (GPU Scene) 직후 EFX-0 진입 권장.

---

## §14. PITFALLS GATE 통과 매트릭스

| GATE | 검증 |
|---|---|
| A 사실 수집 | §3.1 Codex 6 결함 + 현재 FX 코드 인용 (FxBillboardComponent / FxMeshComponent / EffectTuner) |
| B TODO 0 | Phase 시간표 + 의존 명시. EFX-7 GPU 는 보류 명시 (TODO X — 의도된 보류) |
| C 호출 경로 grep | LegacyFxAdapter 가 기존 IreliaFx::SpawnQTrail 등 전부 wrap. game code 변경 0 |
| D ECS 책임 | RenderWorldSnapshot 패턴 — Pass 가 ECS World 직접 의존 X. Snapshot 만 read. |
| E 향후 자료형 | FxAssetHandle = RHIHandle<FxAssetTag> (RHIHandles 패턴 차용 P-18 OK). ParameterID = (ns 8 + nameHash 32 + type 8) bit |
| F Scheduler | CFxSimulationSystem phase=11 단독 (충돌 0). DescribeAccess 박제 |
| G Owner Scope | CFxAssetRegistry = `CGameInstance` Tier-1 (프로세스 1개). CParticlePool = `CFxSimulationSystem` 안 (system 수명) |
| H 인용 의미 + 행동 보존 + include | EFX-0 Legacy Bridge 가 기존 effect 동작 보존 (시각 결과 동일). 모든 헤더 `"ECS/Entity.h"` subdir 보존. ParameterMap 추가가 기존 hardcoded preset 동작 0 변경 |

---

## §15. 다음 진입 명령

```
"Effect Tool v2 진입.
선행 검증: NEXTGEN_FRAMEWORK_MASTER §1 의 1~6 (ECS Generation + Worker-Safe CB
+ SystemAccess + Fiber M1 + RenderGraph + GPU Scene) 완료 후.

진입 순서 (EFX-0 → EFX-1 → ... → EFX-6, EFX-7 보류):

1. EFX-0 (1 week) — LegacyFxAdapter
   - FxAssetFromPreset (Irelia 7 / Yasuo 5 / Annie 5 / Yone 5 = 22 자산)
   - 기존 IreliaFx::SpawnQTrail 등 hook 내부 구현만 자산 사용
   - 기존 game code 0 변경 검증

2. EFX-1 (2 weeks) — FxAsset + ParameterMap
   - 5 namespace (System/Emitter/Particle/User/Event)
   - .wfx JSON 포맷 + FxAssetHandle (RHIHandle 패턴)
   - CFxAssetRegistry + LoadDirectory + ReloadFromFile

3. EFX-2 (2 weeks) — ParticlePool SoA + Deterministic
   - SoA 컬럼 (Pos / Vel / Color / Lifetime / Age / 커스텀 8개)
   - xoroshiro128 deterministic RNG
   - CommandBuffer 통합 worker-safe spawn

4. EFX-3 (4 weeks) — Multi-Render Type 6 종
   - Billboard / Ribbon / Beam / GroundDecal / MeshParticle / ShockwaveRing
   - 각 타입 별 RenderGraph 패스

5. EFX-4 (3 weeks) — Scene_EffectTool
   - 4 패널 (Asset Browser / Preview / Inspector / Timeline)
   - Hot Reload
   - Game Select 통합 — '/Editor/Effect Tool' 진입

6. EFX-5 (3 weeks) — Node Executor + Expression VM
   - 30+ 표준 노드 라이브러리
   - Bytecode VM

7. EFX-6 (2 weeks) — Elden VFX Pack
   - 6 작품: Boss Telegraph / Sword Trail / Shockwave / Magic Circle / Lingering Field / Soul Liberation
   - Class & Servant 활용 검증

진입 전 PITFALLS GATE A~H 8 단계 의무 통과."
```

---

**END OF EFFECT TOOL V2 MASTER**
