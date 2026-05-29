# Phase G v3 — Effect Tool Master (Niagara급 + Elden + Multi-Game, **현 진행 상태 반영**)

**작성일**: 2026-05-07
**상태**: ⚠️ **deprecated / replaced-by** [`17_NIAGARA_FULL_REWRITE_MASTER.md`](17_NIAGARA_FULL_REWRITE_MASTER.md). v3 의 Niagara 매핑/라이프사이클/그래프 런타임 내용은 17에 흡수됨.
**v2 와의 차이**:
- v2 는 박제 직전 청사진 (greenfield 가정 일부 잔존). v3 는 **EFX-0/1/2 헤더 박제 후 시점** 의 정정 + 다음 박제 단계 명세.
- v3 는 Niagara 의 **UNiagaraSystem / UNiagaraEmitter / FNiagaraSystemInstance / FNiagaraEmitterInstance / FNiagaraSystemSimulation** 4 레이어 매핑을 **Winters 클래스 매핑표** 까지 박제.
- v3 는 **Lifecycle 상태머신** (Inactive → Active → Inactive(Cleanup) → Complete + Pool 반환) 을 8 단계로 박제.
- v3 는 EFX-4 Scene_EffectTool 의 **Hot Reload 내부 흐름** 까지 sequence diagram 으로 박제.
**보강 문서 (v3 짝꿍)**:
- [`14_NIAGARA_REFERENCE_DEEP_MAP.md`](14_NIAGARA_REFERENCE_DEEP_MAP.md) — Niagara 코드 매핑 + Winters 차용 패턴
- [`15_EFX_LIFECYCLE_AND_GRAPH_RUNTIME.md`](15_EFX_LIFECYCLE_AND_GRAPH_RUNTIME.md) — 상태머신 + Tick 순서 + Worker-Safe + Hot Reload
- [`16_EFX_PROGRESS_AND_NEXT_ACTIONS.md`](16_EFX_PROGRESS_AND_NEXT_ACTIONS.md) — 현 박제 인용 + 다음 N 단계 박제 가이드 + GATE 통과 체크
**권위 문서**:
- [`17_NIAGARA_FULL_REWRITE_MASTER.md`](17_NIAGARA_FULL_REWRITE_MASTER.md) — v4 권위 마스터. v2/v3/14/15/16 을 흡수.
- [`11_EFFECT_TOOL_NIAGARA_ELDEN_V2_REVIEW.md`](11_EFFECT_TOOL_NIAGARA_ELDEN_V2_REVIEW.md) — Codex 6 결함 정정 매트릭스는 v2 → v3 그대로 유효.
**선행 (CLAUDE.md 기준 2026-05-07)**:
- DX11 단일 백엔드 가동 (W4 SSAO + W6 IRHI 부분 통과)
- Fiber JobSystem Phase 5-A MVP (FiberShell M0 박제)
- ECS Generation v1 (EntityHandle, NULL_ENTITY_HANDLE) 진입 검토 단계
- EngineSDK 자동 동기화 (`UpdateLib.bat`)

---

## §0. 한 줄 비전

> **Unreal Niagara 의 4 레이어 (Asset / Instance / Graph+Compile / Editor) 패턴을 Winters DX11 + ECS 위에 그대로 차용**, 단 **HLSL 셰이더 컴파일 대신 CPU 노드 인터프리터 + Expression VM** 으로 단순화. **모든 게임 (LoL / Elden / Class&Servant) 가 동일 시스템 사용** — `eGameProduct` 분기 없이 `.wfx` 자산만 분리.

**핵심 차이 (Niagara 대비)**:
- Niagara 는 그래프 → HLSL → VectorVM 바이트코드 → GPU/CPU 시뮬. Winters 는 그래프 → 노드 인터프리터 + Expression VM 바이트코드 → CPU 시뮬 (EFX-2~5). GPU 는 EFX-7 보류.
- Niagara 는 `UEdGraphSchema_Niagara` 기반 UE Graph Editor 재사용. Winters 는 ImGui 기반 Scene_EffectTool 자체 박제 (EFX-4).
- Niagara 는 `FNiagaraDataSet` 더블 버퍼 (spawn 누적 + update 더블 버퍼링). Winters 는 단일 SoA + swap-back kill (EFX-2 단순화).

---

## §1. v2 → v3 갱신 매트릭스

### 1.1 박제 진행 상태 (v3 신규 박제)

| Stage | v2 명세 | 2026-05-07 실측 | 갭 |
|---|---|---|---|
| **EFX-0 Legacy Bridge** | LegacyFxAdapter + FxAssetFromPreset (Irelia/Yasuo/Annie/Yone 22 자산) | ✅ `Client/Public/GameObject/FX/LegacyFxAdapter.h` (43줄) + `.cpp` 박제. ✅ `IreliaFxPresets.cpp:31-37` `SpawnBillboardAsset()` 헬퍼가 이미 LegacyFx 통과. 🟡 Yasuo/Annie/Yone preset 은 미통과 (직접 `FxBillboardComponent` 사용) | Yasuo/Annie/Yone preset 의 LegacyFx 통과 + 22 자산 `.wfx` 영속화 |
| **EFX-1 FxAsset + ParameterMap** | 5 namespace + .wfx JSON + Handle | ✅ `Engine/Public/FX/FxAsset.h` (131줄) + `Engine/Public/FX/ParameterMap.h` (156줄) 박제. ✅ FxAssetRegistry 가 generation slot 패턴 (`Slot.generation`, `bAlive`) 박제. ❌ `.wfx` JSON 로더 미박제 (`LoadFromFile` 선언만) | JSON 파서 (rapidjson 사용 또는 자체) + 22 자산 박제 |
| **EFX-2 ParticlePool SoA + Deterministic** | SoA + xoroshiro + CommandBuffer | ✅ `Engine/Public/FX/ParticlePool.h` (68줄) — Pos/Vel/Color/Lifetime/Age/Size/UV + 4 int slot + 8 vec4 slot. ✅ `Engine/Public/FX/DeterministicRandom.h` (CXoroshiro128). ❌ CFxSimulationSystem 미박제, CommandBuffer 통합 미박제 | Simulation System 박제, Worker-Safe Spawn 통합 |
| **EFX-3 Multi-Render Type 6 종** | Billboard/Ribbon/Beam/Decal/Mesh/Shockwave | 🟡 **컴포넌트 4 박제 / 시스템 3 박제**. `FxBillboardComponent` (90줄) + `FxRibbonComponent` (76줄) + `FxBeamComponent` (65줄) + `FxMeshComponent`. `FxSystem.cpp` + `FxBeamSystem.cpp` + `FxMeshSystem.cpp` 박제. ❌ `FxRibbonSystem.cpp` 미박제. ❌ `FxGroundDecalComponent` / `FxShockwaveComponent` 미박제 | Ribbon System 박제, Decal/Shockwave 컴포넌트+시스템 박제, RenderGraph 통합 보류 |
| **EFX-4 Scene_EffectTool** | 4 패널 + Hot Reload + Game Select 통합 | ❌ 미박제. 🟡 `Scene_Editor.cpp` 가 일반 에디터로 존재. `EffectTuner.cpp` 는 여전히 Irelia 전용 hardcode | Scene_EffectTool 신규 박제, EffectTuner 분해 |
| **EFX-5 Node Executor + Expression VM** | DAG + Bytecode VM | ❌ 미박제. `FxNodeDesc` 자료형만 `FxAsset.h:27` 에 박제 | Executor + VM + 30+ 표준 노드 |
| **EFX-6 Elden VFX Pack** | 6 작품 검증 | ❌ 미박제. `Resource/FX/` 폴더 자체 0건 | EFX-3/4/5 후 진입 |
| **EFX-7 GPU Compute** | RHI/RG 안정 후 | ❌ 보류 | NEXTGEN_FRAMEWORK_MASTER §1 6 단계 후 |

### 1.2 v2 → v3 정정 사항

| # | v2 명세 | v3 정정 | 사유 |
|---|---|---|---|
| 1 | `FxAsset.h` 의 `FxEmitterDesc` 가 `RHITextureHandle hMaterial` 만 보유 | v3 실측: `FxEmitterDesc` 가 **`RHITextureHandle hMaterial` + `wstring_t strTexturePath` 둘 다 보유** (FxAsset.h:43,47). raw path 가 **점진 마이그 중** 자료 — v3 는 string path 를 **transitional storage** 로 명시. EFX-4 Tool 진입 시 path → handle 자동 변환 | 점진 마이그 — 22 챔프 preset 마이그 중 raw path 가 일부 잔존 |
| 2 | `FxBillboardComponent` 가 `FxAssetHandle hAsset` 만 보유 | v3 실측: `FxBillboardComponent` 가 **`const wchar_t* texturePath` + `shared_ptr<wstring_t> texturePathOwner` + `FxAssetHandle hAsset`** 3 가지 동시 보유 (FxBillboardComponent.h:22-24). v2 의 핸들 전환은 **부분 진입 상태** — raw path 잔존 = legacy 호환 + shared_ptr = 동적 path 안전 + handle = asset 시스템 진입 | 3 단 점진 마이그 |
| 3 | `Scene_EffectTool` 박제 | v3 실측: 미박제. `Scene_Editor.cpp` 가 일반 에디터로 존재. v3 는 **Scene_Editor 에서 분리하여 신규 박제** 명시 | EFX-4 진입 전 |
| 4 | `Bin/Resource/FX/` 자산 디렉토리 | v3 실측: 폴더 자체 0건. `.wfx` 자산 0개. v3 는 **EFX-0 마이그 결과물 보관 위치** 로 명시 + 디렉토리 생성 책임 명시 | 자산 시스템 진입 prerequisite |
| 5 | `FxAssetRegistry` 의 `generation` 패턴 | v3 실측: 박제 완료 (`FxAsset.h:120` `Slot.generation`). v3 는 **handle 안전 검증 = `ResolveSlot(handle)` 의 generation 비교** 패턴 박제 | 박제 확인 |
| 6 | EFX-6 Elden Pack 6 작품 | v3 정정: **5 작품 + Class&Servant 1 작품** 으로 재편. 보스 telegraph / 검기 / 충격파 / 마법진 / 지속영역 + Servant 소환 = 6 작품 | Class & Servant 통합 비전 강화 |

---

## §2. Niagara 4 레이어 ↔ Winters 매핑 (v3 신규)

### 2.1 4 레이어 구조 매핑

```
Niagara                                    Winters EFX
═══════════════════════════════════════════════════════════════════
Layer 1: Asset (디자이너가 만지는 데이터)
─────────────────────────────────────────────────────────────────
UNiagaraSystem                          ↔  FxAsset (FxAsset.h:84)
  └─ FNiagaraEmitterHandle 들              └─ std::vector<FxEmitterDesc>
UNiagaraEmitter                          ↔  FxEmitterDesc (FxAsset.h:35)
  └─ Spawn / Update / Event 스크립트       └─ std::vector<FxNodeDesc>
UNiagaraScript                           ↔  FxNodeDesc (FxAsset.h:27)
  └─ FNiagaraVMExecutableData              └─ std::vector<u8_t> bytecodeBlob

Layer 2: Runtime Instance (월드에 살아있는 것)
─────────────────────────────────────────────────────────────────
UNiagaraComponent                        ↔  FxInstanceComponent (신규 — EFX-2 박제 예정)
FNiagaraSystemInstance                   ↔  CFxSystemInstance (신규 — EFX-2)
FNiagaraEmitterInstance                  ↔  CFxEmitterInstance (신규 — EFX-2)
FNiagaraDataSet (더블 버퍼)              ↔  CParticlePool (단일 SoA, ParticlePool.h:12)
FNiagaraSystemSimulation (월드 단위 배칭) ↔  CFxSimulationSystem (신규 — Phase 11)

Layer 3: Graph + Compilation
─────────────────────────────────────────────────────────────────
UNiagaraGraph (UEdGraph 기반)            ↔  FxGraph (신규 — EFX-5)
UNiagaraNode + 8 종 서브클래스            ↔  FxNodeDesc (단일 자료형, EFX-5 에서 다형 분기)
FNiagaraHlslTranslator                   ↔  CFxNodeCompiler (신규 — EFX-5)
                                            ★ Winters 는 HLSL 안 만들고 바이트코드 직접
FNiagaraScriptExecutionContext           ↔  CFxExpressionVM (신규 — EFX-5)

Layer 4: Editor UI
─────────────────────────────────────────────────────────────────
FNiagaraSystemToolkit                    ↔  CScene_EffectTool (신규 — EFX-4)
SNiagaraGraphNode (Slate)                ↔  ImGui Node Editor (EFX-4 / EFX-5)
FNiagaraStackViewModel (Stack UI)        ↔  Inspector 패널 (EFX-4)
UEdGraphSchema_Niagara                   ↔  CFxNodeRegistry (신규 — EFX-5)
```

### 2.2 핵심 패턴 차용 (4 가지)

#### Pattern A — Component 가 Instance 를 소유, Instance 가 Pool 을 참조

Niagara:
```
UNiagaraComponent
    └─ TSharedRef<FNiagaraSystemInstance> SystemInstance
            └─ TArray<FNiagaraEmitterInstancePtr> EmitterInstances
                    └─ FNiagaraDataSet ParticleDataSet
```

Winters (EFX-2 박제 예정):
```cpp
// ECS 컴포넌트 = 핸들만 보관 (POD 유지)
struct FxInstanceComponent
{
    FxAssetHandle    hAsset;
    EntityHandle     hAttachTo;       // Generation 안전
    Vec3             vAttachOffset;
    f32_t            fAge = 0.f;
    f32_t            fLifetime = 3.f;
    bool_t           bLoop = false;
    eFxLifecycleState state = eFxLifecycleState::Inactive;
    u32_t            uInstanceSlot = FX_INVALID_INSTANCE;  // CFxSimulationSystem 의 슬롯
    std::array<u32_t, 4> aPoolIndices{};   // 최대 4 emitter / asset
};

// 시스템 = Heavy state 보관 (system 수명)
class CFxSimulationSystem : public ISystem
{
private:
    std::vector<CFxSystemInstance>  m_vecSystemInstances;   // POD index = uInstanceSlot
    std::vector<CParticlePool>      m_vecPoolPerEmitter;    // emitter 별 pool
};
```

**왜 이렇게?** ECS Component 는 POD 유지 (PITFALLS P-19). Heavy state (CParticlePool, RNG seed, 노드 실행 컨텍스트) 는 System 측에 분리. Render 패스는 RenderWorldSnapshot 만 read.

#### Pattern B — ParameterMap 5 namespace + Hash key (Niagara 핵심)

Niagara 의 `FNiagaraVariable + FNiagaraParameterStore` 를 **단일 `CFxParameterMap`** 으로 단순화. namespace 가 의미 분리, hashkey 가 lookup.

Winters 박제 (✅ ParameterMap.h:112-155):
```cpp
class CFxParameterMap final
{
    template<typename T>
    void Set(eFxNamespace ns, std::string_view name, const T& value);
    template<typename T>
    T Get(eFxNamespace ns, std::string_view name, const T& fallback = {}) const;
    // 내부: u64_t key = (ns 8bit) | (type 8bit) | (nameHash 32bit) — 충돌 0
};
```

#### Pattern C — Asset Registry + Generation Slot (Niagara 의 Asset Manager 패턴)

Niagara 는 `UNiagaraSystem` UObject 자체가 영속. Winters 는 `FxAsset` POD + `Slot.generation` 으로 동등 안전성.

Winters 박제 (✅ FxAsset.h:117-127):
```cpp
struct Slot
{
    FxAsset asset;
    u32_t generation = 1;
    bool_t bAlive = false;
    wstring_t path;
};

// FxAssetHandle = RHIHandle<FxAssetTag> = (index 32 + generation 32)
// ResolveSlot(handle) 가 generation 비교 → stale handle 검출
```

#### Pattern D — Hot Reload (Niagara 의 Asset Reimport 패턴)

Niagara: 에디터에서 그래프 변경 → `UNiagaraSystem::PostEditChangeProperty()` → `UNiagaraSystem::RequestCompile()` → 모든 활성 `FNiagaraSystemInstance` 재초기화.

Winters (EFX-4 박제 예정):
```cpp
class CScene_EffectTool
{
    void DetectAssetChanges()
    {
        for (auto h : m_vecLoadedAssets)
        {
            if (FileWasModified(GetAssetPath(h)))
            {
                CFxAssetRegistry::Instance().ReloadFromFile(h);
                // Pool 들 reset, ParameterMap 초기화, Spawn 재시작
                CFxSimulationSystem::Instance().OnAssetReloaded(h);
            }
        }
    }
};
```

---

## §3. Stage 로드맵 v3 (8 Stage, 진행 상태 + 사이즈 갱신)

| Stage | 이름 | 박제 상태 | 남은 분량 | v3 신규 박제 |
|---|---|---|---|---|
| **EFX-0** | Legacy Bridge | 🟡 50% | 2 days | Yasuo/Annie/Yone preset LegacyFx 통과 + 22 자산 `.wfx` 저장 |
| **EFX-1** | FxAsset + ParameterMap + .wfx | 🟡 70% (헤더 ✅, 로더 ❌) | 5 days | rapidjson 로더 + LoadDirectory + 핫리로드 보강 |
| **EFX-2** | ParticlePool SoA + Deterministic + Simulation System | 🟡 40% (헤더 ✅, 시스템 ❌) | 2 weeks | CFxSimulationSystem (Phase 11) + CFxSystemInstance + CFxEmitterInstance + Lifecycle 상태머신 + CommandBuffer Worker-Safe Spawn |
| **EFX-3** | Multi-Render Type 6 종 | 🟡 50% (Billboard/Ribbon/Beam/Mesh 컴포넌트 ✅, Decal/Shockwave ❌) | 3 weeks | FxRibbonSystem 박제 + GroundDecal/Shockwave 컴포넌트+시스템 + RenderGraph 통합 보류 |
| **EFX-4** | Scene_EffectTool + Hot Reload | ❌ 0% | 3 weeks | Scene_EffectTool 신규 + 4 패널 + DockSpace + Hot Reload + Game Select 통합 |
| **EFX-5** | Node Executor + Expression VM | ❌ 0% | 3 weeks | CFxNodeRegistry + 30+ 표준 노드 + CFxNodeCompiler + CFxExpressionVM (바이트코드) |
| **EFX-6** | Elden VFX Pack 6 작품 | ❌ 0% | 2 weeks | 6 `.wfx` 자산 박제 + EFX-4 Tool 검증 |
| **EFX-7** | GPU Compute | ❌ 0% (보류) | TBD | NEXTGEN §1 6 (GPU Scene) 후 진입 |

**총 잔여**: ~14 weeks (EFX-7 제외).

---

## §4. EFX-0 Legacy Bridge — 잔여 박제 (v3 갱신)

### 4.1 현 진행 (실측 인용)

```cpp
// Client/Private/GameObject/Champion/Irelia/IreliaFxPresets.cpp:31-37
EntityID SpawnBillboardAsset(CWorld& world, const FxBillboardComponent& fx, const char* pszAssetName)
{
    CFxAssetRegistry& registry = CFxSystem::GetAssetRegistry();
    const FxAssetHandle handle =
        LegacyFx::FxAssetFromBillboard(registry, fx, pszAssetName);
    return CFxSystem::SpawnFromAsset(world, handle, fx.vWorldPos, fx.attachTo);
}

// Client/Public/GameObject/FX/LegacyFxAdapter.h:21-25
FxAssetHandle FxAssetFromBillboard(CFxAssetRegistry& registry,
    const FxBillboardComponent& src,
    const char* pszAssetName);
```

✅ Irelia 7 preset (Q Trail / Q Mark / W Spin / W Stage2 Slash / E Beam / R Pulse / BA Slash) 이 이미 LegacyFx 통과.

### 4.2 잔여 작업

#### 4.2.1 Yasuo / Annie / Yone preset 의 LegacyFx 통과

**현 상태** (실측):
- `YasuoFxPresets.cpp` 미확인 — 추정: 직접 `FxBillboardComponent` 사용 + `CFxSystem::Spawn()` 직호출
- `Yone_Skills.cpp` — `FxPresets` 미존재 (CLAUDE.md 4.2 인용)

**박제 작업**:
1. `YasuoFxPresets.cpp` 의 모든 `CFxSystem::Spawn(world, fxTmpl)` 을 `SpawnBillboardAsset(world, fxTmpl, "Yasuo_*")` 로 치환
2. `AnnieFxPresets.cpp` 동일
3. Yone 은 `FxPresets` 없음 — `Yone_Skills.cpp` 에서 직접 `SpawnBillboardAsset` 사용

#### 4.2.2 22 자산 `.wfx` 영속화

**현 흐름**:
```
[일회성 자산화]
SpawnBillboardAsset → FxAssetFromBillboard → registry.RegisterOrReplaceByName("Irelia_Q_Trail")
   ↓ ★ in-memory 만 — 프로세스 종료 시 소멸
```

**박제 작업** (EFX-1 와 결합):
1. `LegacyFx::DumpAllAssetsToWfx(registry, L"Client/Bin/Resource/FX/")` 추가
2. 첫 실행 시 22 자산 `.wfx` JSON 으로 저장
3. 다음 실행부터는 `CFxAssetRegistry::LoadDirectory(L"Client/Bin/Resource/FX/")` 가 22 자산 로드 — preset 코드는 `RegisterOrReplaceByName` 의 의미가 변경: 이미 로드된 자산이면 skip, 없으면 일회성 자산화.

**디렉토리 구조**:
```
Client/Bin/Resource/FX/
├── Champions/
│   ├── Irelia/
│   │   ├── q_trail.wfx           ← LegacyFx 변환 결과
│   │   ├── q_mark.wfx
│   │   ├── w_spin.wfx
│   │   ├── w_stage2_slash.wfx
│   │   ├── e_beam.wfx            ← Mesh 타입
│   │   ├── r_pulse.wfx
│   │   └── ba_slash.wfx
│   ├── Yasuo/                     ← 5 자산
│   ├── Annie/                     ← 5 자산
│   └── Yone/                      ← 5 자산 (Yone_Skills 에서 직접 사용)
└── (EFX-6 진입 시 Bosses/, Spells/ 추가)
```

### 4.3 EFX-0 합격 기준

- [ ] Yasuo/Annie/Yone preset 가 LegacyFx 통과 (`SpawnBillboardAsset` 헬퍼 사용)
- [ ] `LegacyFx::DumpAllAssetsToWfx` 박제 + 첫 실행 시 22 `.wfx` 저장
- [ ] `Bin/Resource/FX/Champions/` 22 파일 존재 검증
- [ ] 기존 game code 0 변경 (Scene_InGame / hooks 호출 unchanged)
- [ ] InGame 실행 시 22 effect 시각 결과 동일 (LegacyFx 통과 전후 비교)

---

## §5. EFX-1 FxAsset + ParameterMap + .wfx JSON — 잔여 박제

### 5.1 현 진행 (실측)

✅ **헤더 박제**:
- `Engine/Public/FX/FxAsset.h` (131줄) — FxAssetHandle / FxNodeDesc / FxEmitterDesc / FxAsset / CFxAssetRegistry
- `Engine/Public/FX/ParameterMap.h` (156줄) — eFxNamespace / eFxParameterType / FxParameterID / CFxParameterMap

✅ **Registry generation 패턴 박제**:
```cpp
// FxAsset.h:117-127
struct Slot
{
    FxAsset asset;
    u32_t generation = 1;
    bool_t bAlive = false;
    wstring_t path;
};
```

❌ **미박제**:
- `LoadFromFile` / `LoadDirectory` / `ReloadFromFile` 의 **JSON 파서 본체**
- `Engine/Private/FX/FxAsset.cpp` 의 `LoadFromFile` 구현 (선언만 헤더에 존재)
- `.wfx` schema 정식 명세

### 5.2 .wfx schema v3 (JSON)

```json
{
  "version": "1.0",
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
      "blend_mode": "Additive",
      "material_path": "Texture/FX/Irelia/q_trail_mask.png",
      "lifetime": 1.5,
      "fade_in": 0.05,
      "fade_out": 0.3,
      "uv_scroll_u": 0.0,
      "uv_scroll_v": 0.5,
      "ribbon": {
        "width": 0.35,
        "point_count": 16
      },
      "atlas": {
        "cols": 1,
        "rows": 1,
        "frame_count": 1,
        "fps": 0.0,
        "loop": true
      },
      "nodes": [
        { "type": "SpawnRate",      "rate": "Emitter.SpawnRate" },
        { "type": "InitPosition",   "value": "Emitter.Position" },
        { "type": "InitColor",      "value": "User.TintColor" },
        { "type": "UpdateLifetime", "delta": "System.DeltaTime" }
      ]
    }
  ]
}
```

**필드 매핑** (FxEmitterDesc 와 1:1):
- `name` → `strName`
- `render_type` → `renderType` (enum 문자열)
- `max_particles` → `maxParticles`
- `spawn_rate` → `spawnRate`
- `blend_mode` → `blendMode` (enum 문자열)
- `material_path` → `strTexturePath` (transitional, EFX-4 Tool 진입 시 → `hMaterial`)
- `lifetime` → `fLifetime`
- `fade_in / fade_out` → `fFadeIn / fFadeOut`
- `uv_scroll_u / uv_scroll_v` → `fUvScrollU / fUvScrollV`
- `ribbon.width / point_count` → `fWidth / iRibbonPointCount`
- `atlas.{cols, rows, frame_count, fps, loop}` → `iAtlasCols / iAtlasRows / iAtlasFrameCount / fAtlasFps / bAtlasLoop`
- `nodes[]` → `std::vector<FxNodeDesc> nodes`

### 5.3 JSON 라이브러리 선택

**옵션 비교**:

| 후보 | 장점 | 단점 | v3 추천 |
|---|---|---|---|
| **rapidjson** (header-only) | 빠름, header-only | C++17 미지원, 매크로 많음 | 🟡 |
| **nlohmann/json** (header-only) | 직관적, std STL 친화 | 컴파일 시간 ⬆ | ⭐ |
| **simdjson** | 초고속 | 파서 전용, write 미지원 | ❌ |
| **자체** | 외부 의존 0 | 박제 비용 ⬆ | ❌ |

**v3 추천**: `nlohmann/json` (header-only single-include `json.hpp`). `Engine/ThirdPartyLib/nlohmann/json.hpp` 박제. 컴파일 시간은 EFX-1 진입 시점에 측정 후 결정.

### 5.4 LoadFromFile 본체 박제 (의사 코드)

```cpp
// Engine/Private/FX/FxAsset.cpp
FxAssetHandle CFxAssetRegistry::LoadFromFile(const wstring_t& path)
{
    std::ifstream ifs(path);
    if (!ifs) return FxAssetHandle{};

    nlohmann::json j;
    ifs >> j;

    FxAsset asset{};
    asset.strName = j.at("name").get<std::string>();

    // user_params
    if (j.contains("user_params"))
    {
        for (auto& [key, val] : j["user_params"].items())
        {
            if (val.is_number_float())
                asset.initialUserParams.Set<f32_t>(eFxNamespace::User, key, val.get<f32_t>());
            else if (val.is_array() && val.size() == 4)
                asset.initialUserParams.Set<Vec4>(eFxNamespace::User, key,
                    Vec4{val[0], val[1], val[2], val[3]});
            // ... 기타 타입
        }
    }

    // emitters
    for (auto& je : j["emitters"])
    {
        FxEmitterDesc emitter{};
        emitter.strName = je.at("name").get<std::string>();
        emitter.renderType = ParseRenderType(je.at("render_type").get<std::string>());
        emitter.maxParticles = je.value("max_particles", 1024u);
        emitter.spawnRate = je.value("spawn_rate", 60.f);
        emitter.blendMode = ParseBlendMode(je.value("blend_mode", "AlphaBlend"));
        emitter.strTexturePath = j2w(je.value("material_path", ""));
        emitter.fLifetime = je.value("lifetime", 3.f);
        // ... ribbon / atlas / nodes 파싱
        asset.emitters.push_back(std::move(emitter));
    }

    FxAssetHandle h = RegisterOrReplaceByName(std::move(asset));
    Slot* pSlot = ResolveSlot(h);
    if (pSlot) pSlot->path = path;
    return h;
}

bool_t CFxAssetRegistry::ReloadFromFile(FxAssetHandle handle)
{
    Slot* pSlot = ResolveSlot(handle);
    if (!pSlot || pSlot->path.empty()) return false;
    LoadFromFile(pSlot->path);   // RegisterOrReplaceByName 가 같은 이름이면 덮어씀
    return true;
}

u32_t CFxAssetRegistry::LoadDirectory(const wstring_t& directoryPath)
{
    u32_t uCount = 0;
    for (auto& entry : std::filesystem::recursive_directory_iterator(directoryPath))
    {
        if (entry.is_regular_file() && entry.path().extension() == L".wfx")
        {
            if (LoadFromFile(entry.path().wstring()).IsValid())
                ++uCount;
        }
    }
    return uCount;
}
```

### 5.5 EFX-1 합격 기준

- [ ] `nlohmann/json.hpp` 박제 + 빌드 통과
- [ ] `LoadFromFile` 본체 박제 + 22 자산 로드 검증
- [ ] `LoadDirectory` 박제 + recursive scan 검증
- [ ] `ReloadFromFile` 박제 + path 보관 검증 (Slot.path)
- [ ] EFX-0 의 `DumpAllAssetsToWfx` 가 LoadFromFile 의 inverse 검증 (round-trip 무손실)
- [ ] `EngineSDK/inc/FX/FxAsset.h` 자동 동기화 (`UpdateLib.bat`)

---

## §6. EFX-2 ParticlePool + Simulation System — 잔여 박제 (v3 신규)

### 6.1 현 진행 (실측)

✅ `Engine/Public/FX/ParticlePool.h` (68줄) — SoA 컬럼 + custom int 4 + custom vec4 8
✅ `Engine/Public/FX/DeterministicRandom.h` (CXoroshiro128)
✅ `Engine/Private/FX/ParticlePool.cpp` + `Engine/Private/FX/DeterministicRandom.cpp`

❌ `CFxSimulationSystem` (Phase 11 ECS System) 미박제
❌ `CFxSystemInstance` / `CFxEmitterInstance` (Niagara Pattern A 차용) 미박제
❌ `FxInstanceComponent` (POD ECS 컴포넌트) 미박제
❌ Lifecycle 상태머신 미박제
❌ CommandBuffer Worker-Safe Spawn 미박제

### 6.2 신규 박제 — Component / System / Instance

**파일 트리** (v3 박제 예정):
```
Engine/Public/FX/
├── FxAsset.h                  ✅
├── ParameterMap.h             ✅
├── ParticlePool.h             ✅
├── DeterministicRandom.h      ✅
├── FxLifecycle.h              ❌ 신규 — eFxLifecycleState enum
├── FxSystemInstance.h         ❌ 신규 — Niagara Pattern A 차용
├── FxEmitterInstance.h        ❌ 신규
└── FxSimulationSystem.h       ❌ 신규 — ISystem 상속

Engine/Public/ECS/Components/
└── FxInstanceComponent.h      ❌ 신규 — POD 컴포넌트
```

**FxInstanceComponent** (POD):
```cpp
// Engine/Public/ECS/Components/FxInstanceComponent.h
#pragma once
#include "Defines.h"
#include "WintersMath.h"
#include "ECS/Entity.h"
#include "FX/FxAsset.h"
#include "FX/FxLifecycle.h"

struct FxInstanceComponent
{
    FxAssetHandle           hAsset{};
    EntityHandle            hAttachTo = NULL_ENTITY_HANDLE;   // ECS Generation 안전
    Vec3                    vAttachOffset{};
    f32_t                   fAge = 0.f;
    f32_t                   fLifetime = 3.f;
    bool_t                  bLoop = false;

    eFxLifecycleState       state = eFxLifecycleState::Inactive;
    u32_t                   uInstanceSlot = FX_INVALID_INSTANCE;   // CFxSimulationSystem slot

    // 4 emitter / asset 한도
    std::array<u32_t, 4>    aPoolIndices{};
};
```

**FxLifecycle.h**:
```cpp
// Engine/Public/FX/FxLifecycle.h
#pragma once
#include "WintersTypes.h"

constexpr u32_t FX_INVALID_INSTANCE = 0xFFFFFFFFu;

enum class eFxLifecycleState : u8_t
{
    Inactive,        // 컴포넌트만 존재, system 안 활성 X (pool slot 없음)
    Active,          // tick 받음, spawn/update 실행
    Completing,      // spawn 멈춤, 잔여 particle 소멸 대기
    Complete,        // 모든 particle 소멸, system 측 cleanup 대기
    PoolReturned,    // pool slot 반환 완료, 컴포넌트 제거 대기
};
```

**CFxSystemInstance** (Heavy state, system 측):
```cpp
// Engine/Public/FX/FxSystemInstance.h
#pragma once
#include "Defines.h"
#include "FX/FxAsset.h"
#include "FX/ParameterMap.h"
#include "FX/FxEmitterInstance.h"
#include "FX/DeterministicRandom.h"

class CFxSystemInstance final
{
public:
    void Initialize(FxAssetHandle hAsset, EntityID owner, u64_t rngSeed);
    void Tick(f32_t fDeltaTime, const TransformComponent& tx, CFxParameterMap& worldParams);
    void RequestComplete();   // spawn 중지, 잔여 소멸 대기
    void Reset();             // hot-reload 용

    bool_t IsActive() const;
    bool_t IsComplete() const;

private:
    FxAssetHandle                          m_hAsset{};
    EntityID                               m_OwnerEntity = NULL_ENTITY;
    CFxParameterMap                        m_LocalParams;   // System.* / Emitter.* / User.*
    std::vector<CFxEmitterInstance>        m_vecEmitters;
    CXoroshiro128                          m_Rng;
    eFxLifecycleState                      m_State = eFxLifecycleState::Inactive;
    f32_t                                  m_fAge = 0.f;
};
```

**CFxEmitterInstance** (1 emitter / asset emitter):
```cpp
// Engine/Public/FX/FxEmitterInstance.h
#pragma once
#include "FX/ParticlePool.h"

class CFxEmitterInstance final
{
public:
    void Initialize(const FxEmitterDesc& desc, CXoroshiro128* pRng);
    void Tick(f32_t fDeltaTime, CFxParameterMap& sysParams);
    void RequestComplete();
    bool_t IsComplete() const;

    const CParticlePool& GetPool() const;

private:
    void Spawn_Phase(f32_t fDeltaTime, CFxParameterMap& sysParams);
    void Update_Phase(f32_t fDeltaTime, CFxParameterMap& sysParams);
    void Cull_Phase();   // age >= lifetime 인 particle swap-back kill

    const FxEmitterDesc*  m_pDesc = nullptr;
    CParticlePool         m_Pool;
    CXoroshiro128*        m_pRng = nullptr;
    f32_t                 m_fSpawnAccumulator = 0.f;   // 분수 spawn 보존
    bool_t                m_bSpawnEnabled = true;
};
```

**CFxSimulationSystem** (ISystem, Phase 11):
```cpp
// Engine/Public/FX/FxSimulationSystem.h
#pragma once
#include "ECS/ISystem.h"
#include "FX/FxSystemInstance.h"

class CFxSimulationSystem final : public ISystem
{
public:
    u32_t GetPhase() const override { return 11; }   // CLAUDE.md 4.2 의 Phase 5/6/7/8/10 후

    void DescribeAccess(CSystemAccessBuilder& b) override
    {
        b.Read<TransformComponent>()
         .Write<FxInstanceComponent>()
         .UnknownWritesAll();   // CParticlePool — TODO: 분리 (NEXTGEN §1 3 SystemAccess 진입 후)
    }

    void Execute(CWorld& world, f32_t fDeltaTime) override;

    // 호출자
    EntityID SpawnFromAsset(CWorld& world, FxAssetHandle h, const Vec3& pos,
                             EntityID attachTo = NULL_ENTITY);
    void DeferSpawnFromAsset(CCommandBuffer& cb, FxAssetHandle h, const Vec3& pos,
                              EntityID attachTo = NULL_ENTITY);

    // Hot-reload 후크
    void OnAssetReloaded(FxAssetHandle h);

    // Render 패스 (RenderWorldSnapshot 추출)
    struct FxRenderSnapshot
    {
        std::vector<FxBillboardComponent>  billboards;
        std::vector<FxRibbonComponent>     ribbons;
        std::vector<FxBeamComponent>       beams;
        // ... 6 타입
    };
    void BuildRenderSnapshot(FxRenderSnapshot& outSnapshot) const;

private:
    std::vector<CFxSystemInstance>  m_vecInstances;
    std::vector<u32_t>              m_vecFreeSlots;   // pool 슬롯 재사용
    u64_t                           m_uNextRngSeed = 0xCAFEBABE;
};
```

### 6.3 Lifecycle 상태머신 (8 단계)

상세는 [`15_EFX_LIFECYCLE_AND_GRAPH_RUNTIME.md`](15_EFX_LIFECYCLE_AND_GRAPH_RUNTIME.md) 참조. 요약:

```
1. SpawnFromAsset(world, hAsset, pos, attachTo)
   → world.CreateEntity() + AddComponent(FxInstanceComponent)
   → m_vecInstances.emplace_back(CFxSystemInstance) + state = Inactive

2. Phase 11 Tick() 첫 호출
   → state Inactive → Active
   → CFxSystemInstance::Initialize(hAsset, owner, rngSeed)

3. 매 frame (state == Active)
   → CFxSystemInstance::Tick(dt, tx, worldParams)
       → 각 CFxEmitterInstance::Tick():
           → Spawn_Phase() (spawn rate * dt → particle 추가)
           → Update_Phase() (모든 active particle 의 age/pos/color 갱신)
           → Cull_Phase() (age >= lifetime → swap-back kill)

4. age >= asset.fLifetime AND !bLoop
   → CFxSystemInstance::RequestComplete()
   → state Active → Completing
   → 모든 emitter.bSpawnEnabled = false (spawn 중지)

5. 모든 emitter 의 active particle 0
   → state Completing → Complete

6. CFxSimulationSystem::Execute() 가 state == Complete 검출
   → m_vecInstances 의 슬롯 반환 (m_vecFreeSlots.push)
   → state Complete → PoolReturned

7. 다음 frame 의 Component cleanup phase
   → world.RemoveComponent<FxInstanceComponent>(entity)
   → world.DestroyEntity(entity) (외부 owner 가 attached 이면 skip)

8. 슬롯 재사용
   → 다음 SpawnFromAsset 가 m_vecFreeSlots 에서 pop
```

### 6.4 EFX-2 합격 기준

- [ ] `FxLifecycle.h` 박제 (5 state enum)
- [ ] `FxInstanceComponent` POD 박제 + ECS 등록
- [ ] `CFxSystemInstance` / `CFxEmitterInstance` 박제
- [ ] `CFxSimulationSystem` Phase 11 박제 + ISystem 등록
- [ ] LegacyFx 의 `SpawnFromAsset` 호출이 새 시스템 통과 (기존 동작 보존)
- [ ] Lifecycle 상태머신 8 단계 검증 (debug print 또는 assert)
- [ ] CommandBuffer Worker-Safe Spawn 박제 (Fiber JobSystem 안 race 0)
- [ ] InGame 22 effect 시각 결과 동일 (EFX-0 통과 + EFX-2 통과)
- [ ] xoroshiro128 deterministic RNG 검증 (같은 seed = 같은 sequence)

---

## §7. EFX-3 Multi-Render Type — 잔여 박제

### 7.1 현 진행 (실측)

✅ **컴포넌트 4 박제**:
- `FxBillboardComponent` (90줄) — 26 멤버, atlas + UV scroll + facingMode 보류
- `FxRibbonComponent` (76줄) — 16 point + width + lifetime
- `FxBeamComponent` (65줄) — start/end entity + offset + width + UV scroll
- `FxMeshComponent` — mesh + texture + material props

✅ **시스템 3 박제**:
- `FxSystem.cpp` — Billboard 시뮬+렌더
- `FxBeamSystem.cpp` — Beam 시뮬+렌더
- `FxMeshSystem.cpp` — Mesh 시뮬+렌더

❌ **시스템 미박제**:
- `FxRibbonSystem.cpp` — Ribbon 시뮬+렌더
- `FxGroundDecalComponent` + `FxGroundDecalSystem.cpp`
- `FxShockwaveComponent` + `FxShockwaveSystem.cpp`

### 7.2 FxRibbonSystem 박제

**FxRibbonSystem.h**:
```cpp
// Client/Public/GameObject/FX/FxRibbonSystem.h
class CFxRibbonSystem final
{
public:
    static std::unique_ptr<CFxRibbonSystem> Create(IRHIDevice* pDevice, ...);
    void Update(CWorld& world, f32_t fDeltaTime);
    void Render(CWorld& world, const CDynamicCamera* pCamera);

    static EntityID Spawn(CWorld& world, const FxRibbonComponent& tmpl);

private:
    // 알고리즘:
    // 1. Update 단계:
    //    - attachTo 추종 → vEndOffset 위치 = head
    //    - points 배열 shift (오래된 point pop, 새 point push)
    //    - fElapsed += dt
    // 2. Render 단계:
    //    - point 쌍 → quad strip (4 vertex per pair)
    //    - UV V = age / lifetime (꼬리 갈수록 1.0)
    //    - dynamic VB upload (매 frame, ribbon 64*16=1024 vertex 한도)

    struct RibbonVertex
    {
        Vec3 pos;
        Vec2 uv;
        Vec4 color;
    };
    std::vector<RibbonVertex>  m_vecVB_CPU;
    RHIBufferHandle            m_hDynamicVB;
};
```

### 7.3 FxGroundDecalComponent 박제

```cpp
// Client/Public/GameObject/FX/FxGroundDecalComponent.h
struct FxGroundDecalComponent
{
    Vec3   vWorldPos{};
    f32_t  fRadius = 1.f;
    f32_t  fYaw = 0.f;
    Vec4   vColor{1, 1, 1, 1};

    const wchar_t* texturePath = nullptr;
    std::shared_ptr<const wstring_t> texturePathOwner = {};
    FxAssetHandle hAsset{};
    u32_t iEmitterIndex = 0;

    f32_t  fGrowDuration = 0.5f;
    f32_t  fFadeOutDuration = 0.3f;
    f32_t  fLifetime = 2.f;
    f32_t  fElapsed = 0.f;

    eBlendPreset blendMode = eBlendPreset::AlphaBlend;
    bool bPendingDelete = false;
};
```

**렌더링 알고리즘** (DX11):
- Quad on plane (XZ-plane normal-up)
- View-projection 적용 + depth-aware (지면과의 거리에 따라 alpha)
- UV: world position projection (decal 회전 = `fYaw`)
- Grow-in: `fRadius_actual = fRadius * smoothstep(0, fGrowDuration, fElapsed)`
- Fade-out: `alpha *= 1.0 - smoothstep(fLifetime - fFadeOutDuration, fLifetime, fElapsed)`

### 7.4 FxShockwaveComponent 박제

```cpp
// Client/Public/GameObject/FX/FxShockwaveComponent.h
struct FxShockwaveComponent
{
    Vec3   vWorldPos{};
    f32_t  fStartRadius = 0.f;
    f32_t  fEndRadius = 10.f;
    f32_t  fDuration = 1.f;
    f32_t  fElapsed = 0.f;
    f32_t  fThickness = 0.2f;
    Vec4   vColor{1, 1, 1, 1};

    const wchar_t* texturePath = nullptr;
    std::shared_ptr<const wstring_t> texturePathOwner = {};
    FxAssetHandle hAsset{};
    u32_t iEmitterIndex = 0;

    eBlendPreset blendMode = eBlendPreset::Additive;
    bool bPendingDelete = false;
};
```

**렌더링 알고리즘** (DX11):
- Ring quad (inner radius, outer radius)
- `t = fElapsed / fDuration ∈ [0, 1]`
- `inner = lerp(fStartRadius, fEndRadius - fThickness, t)`
- `outer = lerp(fStartRadius + fThickness, fEndRadius, t)`
- alpha fade: `alpha = (1.0 - t) * smoothstep(0, 0.1, t)` (등장 0.1 secs + 선형 fade)

### 7.5 EFX-3 합격 기준

- [ ] `FxRibbonSystem.cpp` 박제 + Irelia Q Trail 의 LegacyFx 통과 시 ribbon 변환 자동 (`render_type: "Ribbon"` 인 자산만)
- [ ] `FxGroundDecalComponent` + `FxGroundDecalSystem` 박제 + Yasuo W (WindWall) 또는 Annie W (Tibbers warning) 적용 검증
- [ ] `FxShockwaveComponent` + `FxShockwaveSystem` 박제 + Annie R (Tibbers 소환 충격파) 적용 검증
- [ ] 6 타입 모두 RenderGraph 패스로 등록 (NEXTGEN §1 5 RenderGraph 진입 후 보강 — 그 전까지는 Immediate-mode)
- [ ] EFX-4 Tool 의 Preview Viewport 가 6 타입 모두 표시 (EFX-4 진입 후)

---

## §8. EFX-4 Scene_EffectTool — 전량 박제

### 8.1 현 진행 (실측)

❌ 미박제. `Scene_Editor.cpp` 가 일반 에디터로 존재 (Loader/Texture 검사 등). EffectTuner.cpp 는 여전히 Irelia 7 hardcode.

### 8.2 신규 박제

**파일**:
```
Client/Public/Scene/Scene_EffectTool.h         ← 신규
Client/Private/Scene/Scene_EffectTool.cpp     ← 신규
Client/Public/UI/EffectToolPanels/             ← 신규 디렉토리
├── AssetBrowserPanel.h
├── PreviewViewportPanel.h
├── InspectorPanel.h
├── TimelinePanel.h
└── NodeGraphPanel.h        ← EFX-5 진입 시 박제
Client/Private/UI/EffectToolPanels/
├── AssetBrowserPanel.cpp
├── PreviewViewportPanel.cpp
├── InspectorPanel.cpp
└── TimelinePanel.cpp
```

### 8.3 Scene_EffectTool 본체

```cpp
// Client/Public/Scene/Scene_EffectTool.h
#pragma once
#include "Scene/IScene.h"
#include "FX/FxAsset.h"
#include "ECS/Entity.h"
#include <vector>

class CWorld;
namespace UI {
    class CAssetBrowserPanel;
    class CPreviewViewportPanel;
    class CInspectorPanel;
    class CTimelinePanel;
}

class CScene_EffectTool : public IScene
{
public:
    static std::unique_ptr<CScene_EffectTool> Create();

    bool_t OnEnter() override;
    void OnExit() override;
    void OnUpdate(f32_t fDeltaTime) override;
    void OnRender() override;
    void OnImGui() override;

private:
    CScene_EffectTool() = default;

    void DetectAssetChanges();   // Hot reload — 매 0.5sec 검사

    std::unique_ptr<CWorld>                   m_pPreviewWorld;
    std::unique_ptr<CDynamicCamera>           m_pPreviewCamera;

    std::unique_ptr<UI::CAssetBrowserPanel>   m_pAssetBrowser;
    std::unique_ptr<UI::CPreviewViewportPanel> m_pPreviewViewport;
    std::unique_ptr<UI::CInspectorPanel>      m_pInspector;
    std::unique_ptr<UI::CTimelinePanel>       m_pTimeline;

    FxAssetHandle             m_hSelectedAsset{};
    EntityID                  m_PreviewEntity = NULL_ENTITY;
    std::vector<FxAssetHandle> m_vecLoadedAssets;
    std::unordered_map<FxAssetHandle, std::filesystem::file_time_type> m_AssetMTimes;

    f32_t                     m_fTimelineSec = 0.f;
    bool_t                    m_bPlay = true;
    bool_t                    m_bLoop = true;
    f32_t                     m_fPlaySpeed = 1.f;
    f32_t                     m_fHotReloadCheckTimer = 0.f;
};
```

### 8.4 4 패널 명세

#### Asset Browser (좌측, 폭 250px)

```cpp
class UI::CAssetBrowserPanel
{
public:
    void Draw(FxAssetHandle& outSelected);

private:
    void DrawTree(const std::filesystem::path& root);
    void DrawContextMenu(const std::filesystem::path& path);

    std::filesystem::path m_RootPath = L"Client/Bin/Resource/FX/";
    std::string           m_strFilter;   // 검색 필터
};
```

**기능**:
- `Bin/Resource/FX/` recursive 트리 (Champions/Bosses/Spells/Player/...)
- 클릭 시 미리보기 viewport 에 spawn
- 우클릭: New / Duplicate / Delete / Rename / Open in External Editor (notepad++ etc)
- 검색 필터 (`fnv1a` hash 검색)
- "Reload All" 버튼 → `CFxAssetRegistry::Instance()->LoadDirectory(...)`

#### Preview Viewport (중앙, 가변 폭)

```cpp
class UI::CPreviewViewportPanel
{
public:
    void Draw(FxAssetHandle hSelected, EntityID& ioPreviewEntity);

private:
    void RenderToTexture(CWorld& world, CDynamicCamera& camera);

    RHITextureHandle m_hRT;            // off-screen RT
    Vec3             m_vCameraPos = {0, 2, -5};
    Vec3             m_vCameraTarget = {0, 1, 0};
    f32_t            m_fCameraYaw = 0.f;
    f32_t            m_fCameraPitch = 0.f;
    bool_t           m_bShowGrid = true;
    bool_t           m_bShowAxes = true;
};
```

**기능**:
- 별도 RT (1024×768 default)
- Dynamic camera (마우스 우클릭 = 회전, 휠 = 줌, WASD = 이동)
- 그리드 (1m 간격 5×5)
- 좌표축 (R/G/B = X/Y/Z)
- 배경: 솔리드 색 / cubemap / Elden boss arena 미리보기 모델 (옵션)
- 미리보기 entity = `CFxSystem::SpawnFromAsset(m_pPreviewWorld, hSelected, Vec3::Zero)`

#### Inspector (우측 상단, 폭 320px)

```cpp
class UI::CInspectorPanel
{
public:
    void Draw(FxAssetHandle hSelected, FxAsset* pMutableAsset);

private:
    void DrawAssetHeader(FxAsset& asset);
    void DrawEmitterDesc(FxEmitterDesc& emitter);
    void DrawNodeList(std::vector<FxNodeDesc>& nodes);   // EFX-5 진입 시
    void DrawUserParams(CFxParameterMap& params);

    i32_t m_iSelectedEmitterIdx = 0;
};
```

**기능**:
- 선택된 asset 의 모든 FxEmitterDesc 슬라이더
  - render_type 콤보박스 (6 종)
  - max_particles 슬라이더 (1 ~ 10000)
  - spawn_rate 슬라이더 (0 ~ 1000)
  - blend_mode 콤보박스
  - material_path 텍스트 입력 + 텍스처 picker (drag&drop)
  - lifetime / fade_in / fade_out 슬라이더
  - render_type 별 추가 슬라이더 (ribbon.width / atlas.cols 등)
- ParameterMap User.* 슬라이더 (선택된 asset 의 initialUserParams)
- "Apply" 버튼 → asset 저장 (`.wfx` 덮어쓰기) + ReloadFromFile

#### Timeline (하단, 높이 100px)

```cpp
class UI::CTimelinePanel
{
public:
    void Draw(f32_t& ioTimeSec, bool_t& ioPlay, bool_t& ioLoop, f32_t& ioPlaySpeed);

private:
    f32_t m_fMaxDuration = 5.f;   // longest emitter lifetime
};
```

**기능**:
- Play / Pause / Stop / Loop 토글
- 시간 스크럽 (0 ~ longest emitter lifetime)
- Play Speed 슬라이더 (0.1× ~ 4×)
- Frame-by-frame stepping (← →)
- 시간 = `fTimelineSec`, 미리보기 instance 의 `fAge` 와 동기

### 8.5 Hot Reload 흐름

```
[VFX artist: 외부 에디터로 .wfx 저장]
         ↓
[Win32 ReadDirectoryChangesW 또는 0.5s polling]
         ↓
CScene_EffectTool::DetectAssetChanges()
         ↓
for each loaded asset:
    if (filesystem::last_write_time(path) > m_AssetMTimes[h])
         ↓
        CFxAssetRegistry::ReloadFromFile(h)
            ↓
            Slot 의 asset 덮어쓰기 (generation 증가 X — 같은 핸들 유효)
            ↓
        CFxSimulationSystem::OnAssetReloaded(h)
            ↓
            m_vecInstances 중 hAsset == h 인 것:
                Reset() → Initialize(h, ...) 재호출
            ↓
        시각 결과 즉시 반영 (재시작 0)
```

### 8.6 Game Select 통합

```cpp
// Multi-Game Architecture (Scene_GameSelect.cpp)
if (ImGui::Button("Editor → Effect Tool"))
{
    Change_Scene(CScene_EffectTool::Create());
}
```

자산 디렉토리는 모든 게임 공유 — `Bin/Resource/FX/` 안에 `Champions/` (LoL) + `Bosses/` (Elden) + `ClassServant/` (CS) 자식.

### 8.7 EFX-4 합격 기준

- [ ] `Scene_EffectTool` 박제 + Game Select 진입 가능
- [ ] 4 패널 모두 박제 + DockSpace 동작
- [ ] Asset Browser 가 22 자산 (EFX-0 결과) 표시
- [ ] Preview Viewport 가 선택된 자산 미리보기 (camera control 동작)
- [ ] Inspector 가 emitter desc 편집 + Apply 시 저장 + reload
- [ ] Timeline 의 play/pause/loop/speed 모두 동작
- [ ] Hot Reload 가 0.5s 주기로 동작 (외부 에디터로 `.wfx` 수정 → 자동 반영)
- [ ] EffectTuner.cpp 의 Irelia 7 hardcode 정리 (deprecated 표시 또는 제거)

---

## §9. EFX-5 Node Executor + Expression VM — 전량 박제

### 9.1 노드 라이브러리 (30+ 표준 노드)

| Category | 노드 | namespace 흐름 |
|---|---|---|
| **Spawn** | SpawnRate / SpawnBurst / SpawnPerSecond / SpawnFromEvent | Emitter.SpawnRate → spawn 결정 |
| **Init Position** | InitPositionAtPoint / InitPositionInBox / InitPositionInSphere / InitPositionInCone | → Particle.Position |
| **Init Velocity** | InitVelocityRandom / InitVelocityCone / InitVelocityFromTarget | → Particle.Velocity |
| **Init Color/Size** | InitColor / InitColorFromUser / InitSize / InitSizeRandom | → Particle.Color, .Size |
| **Update Motion** | UpdateGravity / UpdateDrag / UpdateOrbit / UpdateCurlNoise / UpdateAttractor | Particle.Velocity → Particle.Position |
| **Update Visual** | UpdateColorOverLife / UpdateSizeOverLife / UpdateAlphaOverLife / UpdateAtlasFrame | Particle.Age → Particle.Color/Size/UV |
| **Update Logic** | UpdateLifetime / UpdateKillIfBelowY / UpdateKillIfOutOfBounds | Particle.Age → death |
| **Event** | EventOnDeath / EventOnSpawn / EventOnHit | Event.* trigger |
| **Math/Util** | Add / Sub / Mul / Div / Lerp / Clamp / Saturate / Length / Normalize / Cross / Dot | param 변환 |

### 9.2 Expression VM 바이트코드

```cpp
// Engine/Public/FX/FxExpressionVM.h
enum class eFxOp : u8_t
{
    NoOp = 0,

    // Stack 조작
    PushConstFloat,    // payload: f32 (4 byte)
    PushConstVec3,     // payload: Vec3 (12 byte)
    PushConstVec4,
    PushConstInt,
    Pop,

    // Param 로드/저장
    LoadParam,         // payload: FxParameterID (8 byte)
    StoreParam,        // payload: FxParameterID

    // Particle attribute (현재 particle index)
    LoadParticleAttr,  // payload: eParticleAttr (1 byte)
    StoreParticleAttr,

    // Float 연산
    AddF, SubF, MulF, DivF, NegF,
    SinF, CosF, SqrtF, LerpF, ClampF, SaturateF,

    // Vec3 연산
    AddV3, SubV3, MulV3F, NormalizeV3, LengthV3, CrossV3, DotV3,

    // 분기
    JumpIf,            // payload: i16 offset
    Jump,
    Return,
};

class CFxExpressionVM final
{
public:
    void Execute(const std::vector<u8_t>& bytecode,
                 CFxParameterMap& params,
                 CParticlePool& pool,
                 u32_t uParticleIdx);   // 현재 particle (Particle.* attr 의 인덱스)

private:
    std::vector<FxValue> m_Stack;
    u32_t                m_PC = 0;     // Program Counter
};
```

### 9.3 노드 → 바이트코드 컴파일

```cpp
// Engine/Public/FX/FxNodeCompiler.h
class CFxNodeCompiler final
{
public:
    bool_t CompileEmitter(const FxEmitterDesc& emitter,
                          std::vector<u8_t>& outSpawnBytecode,
                          std::vector<u8_t>& outUpdateBytecode);

private:
    // 위상 정렬 후 노드를 spawn/update 그룹으로 분리
    bool_t TopologicalSort(const std::vector<FxNodeDesc>& nodes,
                           std::vector<u32_t>& outOrder);

    // 노드 1개를 바이트코드 chunk 로 변환
    void EmitNode(const FxNodeDesc& node, std::vector<u8_t>& outBytecode);
};
```

**Niagara FNiagaraHlslTranslator 와의 차이**:
- Niagara 는 그래프 → HLSL 텍스트 → VectorVM 바이트코드 (2 stage)
- Winters 는 그래프 → 바이트코드 직접 (1 stage)
- 단순화 비용: HLSL 에서 가능한 SIMD 자동 최적화 포기 → CPU 수동 SIMD 노드 (`AddV3SIMD`) 박제 옵션 EFX-7 시 추가

### 9.4 EFX-5 합격 기준

- [ ] 30+ 표준 노드 박제 + Registry 등록
- [ ] CFxNodeCompiler 박제 + 위상 정렬 검증
- [ ] CFxExpressionVM 박제 + 50+ opcode 테스트 통과
- [ ] EFX-4 Inspector 의 노드 그래프 패널 박제 + 노드 추가/제거/연결 동작
- [ ] 22 LegacyFx 자산 중 1 개 (Irelia Q Trail) 가 노드 그래프로 표현 가능 검증
- [ ] Bytecode round-trip (compile → save .wfx → load → compile → 동일 결과) 검증

---

## §10. EFX-6 Elden VFX Pack — 6 작품

[12_EFFECT_TOOL_NIAGARA_V2_MASTER.md §9](12_EFFECT_TOOL_NIAGARA_V2_MASTER.md#9-efx-6-elden-vfx-pack--검증-작품-6-종) 와 동일. v3 추가:
- 각 작품 박제 후 EFX-4 Tool 에서 시각 검증 + Class&Servant 활용 검증
- `Bin/Resource/FX/Bosses/Margit/`, `Bin/Resource/FX/Spells/`, `Bin/Resource/FX/ClassServant/` 자산 디렉토리 박제

---

## §11. PITFALLS GATE A~H 통과 매트릭스 v3

| GATE | v2 | v3 보강 |
|---|---|---|
| A 사실 수집 | Codex 6 결함 + FxBillboardComponent 인용 | + FxAsset.h 실측 (FxAssetRegistry generation slot) + IreliaFxPresets.cpp:31-37 인용 (LegacyFx 통과 확인) |
| B TODO 0 | Phase 시간표 + EFX-7 보류 명시 | + 각 Stage 의 합격 기준 체크박스 박제 (§4.3, §5.5, §6.4, §7.5, §8.7, §9.4) |
| C 호출 경로 grep | LegacyFxAdapter 가 IreliaFx 등 wrap | + Yasuo/Annie/Yone preset 통과 진입 검증 (EFX-0 잔여) |
| D ECS 책임 | RenderWorldSnapshot 패턴 | + FxInstanceComponent 가 POD 유지 + Heavy state (CFxSystemInstance) 가 system 측 분리 명시 (§6.2) |
| E 향후 자료형 | FxAssetHandle = RHIHandle<FxAssetTag> | + FxValue = std::variant 박제 + FxParameterID 64-bit key (ns 8 + type 8 + nameHash 32) 검증 |
| F Scheduler | Phase 11 단독 | + DescribeAccess 박제 (§6.2) + UnknownWritesAll 명시 (NEXTGEN §1 3 SystemAccess 진입 후 분리) |
| G Owner Scope | CFxAssetRegistry = Tier-1 | + CFxSimulationSystem = Tier-2 (CWorld owned) + CParticlePool 가 system 안 (system 수명) 명시 |
| H 인용 의미 + 행동 보존 + include | EFX-0 동작 보존 + subdir | + EngineSDK 동기화 (`UpdateLib.bat` 자동) + WINTERS_ENGINE 매크로 (FxAsset.h:95 인용) + `"FX/FxAsset.h"` 사용 검증 |

---

## §12. 다음 진입 명령 v3

```
"Effect Tool v3 진입.

선행 (CLAUDE.md 2026-05-07 기준):
- DX11 단일 백엔드 가동 (W4 SSAO 박제 완료)
- Fiber JobSystem Phase 5-A MVP (FiberShell M0 박제, M1 yield 미박제)
- ECS Generation v1 (EntityHandle, NULL_ENTITY_HANDLE) 진입 검토 단계

진입 순서 (현 박제 상태 반영):

1. EFX-0 잔여 (2 days)
   - Yasuo/Annie/Yone preset 의 LegacyFx 통과
   - LegacyFx::DumpAllAssetsToWfx 박제
   - 22 자산 .wfx 첫 저장
   합격: 22 자산 .wfx 존재 + InGame 22 effect 시각 동일

2. EFX-1 잔여 (5 days)
   - nlohmann/json.hpp 박제
   - LoadFromFile / LoadDirectory / ReloadFromFile 본체
   - .wfx schema v3 (JSON) 22 자산 라운드트립
   합격: 22 자산 load + reload 검증 + EngineSDK 동기화

3. EFX-2 잔여 (2 weeks)
   - FxLifecycle.h (5 state enum)
   - FxInstanceComponent (POD)
   - CFxSystemInstance / CFxEmitterInstance
   - CFxSimulationSystem (Phase 11)
   - LegacyFx::SpawnFromAsset 가 새 시스템 통과
   - CommandBuffer Worker-Safe Spawn
   합격: 8 단계 라이프사이클 검증 + 22 effect 시각 동일

4. EFX-3 잔여 (3 weeks)
   - FxRibbonSystem 박제
   - FxGroundDecal 컴포넌트+시스템
   - FxShockwave 컴포넌트+시스템
   합격: 6 타입 모두 InGame 동작

5. EFX-4 (3 weeks)
   - Scene_EffectTool 신규
   - 4 패널 + DockSpace + Hot Reload + Game Select 통합
   - EffectTuner Irelia hardcode 정리

6. EFX-5 (3 weeks)
   - 30+ 표준 노드 + Registry
   - CFxNodeCompiler + CFxExpressionVM
   - EFX-4 Inspector 노드 그래프 패널

7. EFX-6 (2 weeks)
   - 6 Elden 작품 박제 + EFX-4 검증

진입 전 PITFALLS GATE A~H 8 단계 의무 통과 (§11 매트릭스).

상세는 보강 문서 3 종:
- 14_NIAGARA_REFERENCE_DEEP_MAP.md — Niagara 매핑 깊이
- 15_EFX_LIFECYCLE_AND_GRAPH_RUNTIME.md — 상태머신 + Tick + Hot Reload
- 16_EFX_PROGRESS_AND_NEXT_ACTIONS.md — 진행 + 박제 가이드"
```

---

**END OF EFFECT TOOL V3 MASTER**
