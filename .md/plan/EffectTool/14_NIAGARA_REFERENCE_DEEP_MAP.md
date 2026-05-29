# Niagara Reference Deep Map — Winters EFX 차용 패턴 + 클래스/파일 매핑

**작성일**: 2026-05-07
**상태**: ⚠️ v3 보강 참고문서. 권위 매핑은 [`17_NIAGARA_FULL_REWRITE_MASTER.md`](17_NIAGARA_FULL_REWRITE_MASTER.md) 의 §3.
**목적**: Effect Tool v3 박제 시 **Niagara 의 어떤 클래스/파일/패턴** 을 어떻게 차용했는지 명확히 인용 + 차용 안 한 부분 명시 + 차이 사유 박제
**참조**:
- Unreal Engine 5.7 source: `C:\Users\user\Desktop\UnrealEngine\UnrealEngine`
- Niagara 위치: `Engine/Plugins/FX/Niagara/Source/`
- Winters v4 마스터: [`17_NIAGARA_FULL_REWRITE_MASTER.md`](17_NIAGARA_FULL_REWRITE_MASTER.md)

---

## §1. Niagara 모듈 구조 (Engine/Plugins/FX/Niagara/Source/)

```
Niagara/                   ← 런타임 본체 (Asset + Instance + Renderer)
NiagaraCore/               ← 데이터 인터페이스 추상층 (UNiagaraDataInterfaceBase)
NiagaraShader/             ← VM 바이트코드 + HLSL 컴파일 결과 컨테이너
NiagaraVertexFactories/    ← GPU 렌더링 입력 (sprite/mesh/ribbon/light/decal/volume)
NiagaraEditor/             ← ★ 그래프 에디터, 컴파일러, HLSL 번역기, Toolkit
NiagaraEditorWidgets/      ← Slate UI 위젯 (SNiagaraGraphNode 등)
NiagaraAnimNotifies/       ← 애니메이션 노티 → Niagara 트리거
NiagaraBlueprintNodes/     ← BP 에서 호출용 노드
```

**Winters 차용**:
- ✅ Niagara (런타임) — Pattern A/B/C/D 모두 차용
- ✅ NiagaraEditor (그래프 + 컴파일러) — EFX-4 (ImGui 대체) + EFX-5 (바이트코드 직접)
- ❌ NiagaraShader (HLSL/VectorVM) — Winters 는 CPU 바이트코드만 (EFX-7 보류 후 GPU 진입 시 검토)
- ❌ NiagaraVertexFactories — Winters 는 DX11 PlaneRenderer + 자체 VB 직접 박제
- ❌ NiagaraAnimNotifies — Winters 는 SkillHook 사용
- ❌ NiagaraBlueprintNodes — Winters 는 BP 없음

---

## §2. 4 레이어 매핑 (상세)

### 2.1 Layer 1 — Asset (디자이너가 만지는 데이터)

| Niagara | Niagara 파일 | Winters | Winters 파일 | 매핑 |
|---|---|---|---|---|
| `UNiagaraSystem` | `Niagara/Classes/NiagaraSystem.h` | `FxAsset` | `Engine/Public/FX/FxAsset.h:84` | 1:1 — 시스템 1개 = 자산 1개. 여러 emitter 묶음 |
| `FNiagaraEmitterHandle` | `Niagara/Classes/NiagaraEmitterHandle.h` | `FxEmitterDesc` | `Engine/Public/FX/FxAsset.h:35` | Niagara 는 emitter handle 이 별도 자산 (UNiagaraEmitter) 참조. Winters 는 inline (FxEmitterDesc 가 FxAsset 안 vector) |
| `UNiagaraEmitter` | `Niagara/Classes/NiagaraEmitter.h` | (FxEmitterDesc 와 통합) | — | Niagara 의 emitter 는 자체 자산 (재사용 가능). Winters 는 단순화 — emitter 는 system 내부 |
| `UNiagaraScript` | `Niagara/Classes/NiagaraScript.h` | `FxNodeDesc` + bytecodeBlob | `FxAsset.h:27` | Niagara 는 script 가 별도 자산. Winters 는 bytecode 가 FxNodeDesc 안 vector<u8_t> |
| `FNiagaraVMExecutableData` | `Niagara/Public/NiagaraExecutableData.h` | `bytecodeBlob` | `FxAsset.h:32` | VM bytecode 보관. Winters 는 단일 vector<u8_t> 단순화 |

**차용 패턴**:
- Niagara 는 자산 분리 (System / Emitter / Script 모두 별도 UAsset). 재사용 ⬆ + 자산 관리 비용 ⬆
- Winters 는 inline (FxAsset 1 자산에 모든 emitter + script 통합). 재사용 = `FxAssetHandle` 복사. 자산 관리 비용 ⬇

**구체 인용**:
```cpp
// Niagara (UE 5.7) — Engine/Plugins/FX/Niagara/Source/Niagara/Classes/NiagaraSystem.h
UCLASS()
class NIAGARA_API UNiagaraSystem : public UFXSystemAsset
{
    UPROPERTY()
    TArray<FNiagaraEmitterHandle> EmitterHandles;
    // FNiagaraEmitterHandle 가 UNiagaraEmitter* 참조 — 실제 자산 분리
};

// Winters — Engine/Public/FX/FxAsset.h:84
struct FxAsset
{
    FxAssetHandle              handle{};
    std::string                strName;
    std::vector<FxEmitterDesc> emitters;        // ★ inline — Niagara 와 차이
    CFxParameterMap            initialUserParams;
};
```

### 2.2 Layer 2 — Runtime Instance (월드에 살아있는 것)

| Niagara | Niagara 파일 | Winters | Winters 파일 | 매핑 |
|---|---|---|---|---|
| `UNiagaraComponent` | `Niagara/Classes/NiagaraComponent.h` | `FxInstanceComponent` | `Engine/Public/ECS/Components/FxInstanceComponent.h` (EFX-2 신규) | Niagara 는 USceneComponent 상속, Winters 는 ECS POD 컴포넌트 |
| `FNiagaraSystemInstance` | `Niagara/Public/NiagaraSystemInstance.h` | `CFxSystemInstance` | `Engine/Public/FX/FxSystemInstance.h` (EFX-2 신규) | 라이브 인스턴스 — emitter 들 + parameter map + lifecycle 보관 |
| `FNiagaraEmitterInstance` | `Niagara/Classes/NiagaraEmitterInstance.h` | `CFxEmitterInstance` | `Engine/Public/FX/FxEmitterInstance.h` (EFX-2 신규) | abstract — Spawn/Update/Cull phase 분리 |
| `FNiagaraEmitterInstanceImpl` | `Niagara/Internal/NiagaraEmitterInstanceImpl.h` | (Winters 는 단일 클래스) | — | Niagara 는 CPU/GPU 분기 (Impl/GpuImpl). Winters 는 EFX-7 까지 CPU 단일 |
| `FNiagaraDataSet` | `Niagara/Classes/NiagaraDataSet.h` | `CParticlePool` | `Engine/Public/FX/ParticlePool.h:12` ✅ | particle attribute SoA 보관. Niagara 는 더블 버퍼, Winters 는 단일 + swap-back kill |
| `FNiagaraDataBuffer` | `Niagara/Classes/NiagaraDataSet.h` | (CParticlePool 안 vector 들) | `ParticlePool.h:58-66` | Niagara 의 buffer = SoA 컬럼 1개 분리. Winters 는 vector<T> 직접 |
| `FNiagaraSystemSimulation` | `Niagara/Public/NiagaraSystemSimulation.h` | `CFxSimulationSystem` | `Engine/Public/FX/FxSimulationSystem.h` (EFX-2 신규) | Per-world 단위. 모든 system instance 배칭 tick |

**차용 패턴 — Component 가 Instance 를 소유, Instance 가 Pool 을 참조** (Pattern A):

Niagara:
```cpp
// UNiagaraComponent.h
class UNiagaraComponent : public USceneComponent
{
    TSharedRef<FNiagaraSystemInstance> SystemInstance;
    // ★ Component = USceneComponent (heavy, OOP)
    // ★ SystemInstance = TSharedRef (heap, 별도 lifetime)
};

// FNiagaraSystemInstance.h
class FNiagaraSystemInstance
{
    TArray<FNiagaraEmitterInstancePtr> EmitterInstances;
    UNiagaraSystem* System;   // 자산 참조
    EExecutionState ExecutionState;   // Active / Inactive / Complete / Disabled
};

// FNiagaraEmitterInstanceImpl.h (CPU)
class FNiagaraEmitterInstanceImpl
{
    FNiagaraDataSet ParticleDataSet;   // SoA + 더블 버퍼
    TArray<FNiagaraScriptExecutionContext> ScriptContexts;   // Spawn/Update/Event
};
```

Winters (EFX-2 박제 예정):
```cpp
// FxInstanceComponent (POD, ECS)
struct FxInstanceComponent
{
    FxAssetHandle           hAsset{};
    EntityHandle            hAttachTo;
    eFxLifecycleState       state;
    u32_t                   uInstanceSlot = FX_INVALID_INSTANCE;   // ★ slot index 만 보관
    // heavy state X — POD 유지
};

// CFxSystemInstance (heavy state, system 측 vector 안)
class CFxSystemInstance
{
    FxAssetHandle                          m_hAsset;
    CFxParameterMap                        m_LocalParams;
    std::vector<CFxEmitterInstance>        m_vecEmitters;
    CXoroshiro128                          m_Rng;
    eFxLifecycleState                      m_State;
};

// CFxEmitterInstance
class CFxEmitterInstance
{
    const FxEmitterDesc*  m_pDesc;
    CParticlePool         m_Pool;             // ★ SoA — Niagara 와 동등
    CXoroshiro128*        m_pRng;
    f32_t                 m_fSpawnAccumulator;
};
```

**차이 사유**:
- ECS POD: PITFALLS P-19 (Render/Sim 결합) 회피. 컴포넌트 = 핸들/슬롯만 보관. Heavy state 는 system 측.
- Niagara 는 OOP — `USceneComponent` 자체가 heavy. UE 의 GC 가 lifecycle 관리.
- Winters 는 ECS — World/Entity/Component 분리. lifecycle 은 명시적 (CFxSimulationSystem 가 m_vecFreeSlots 로 슬롯 재사용).

### 2.3 Layer 3 — Graph + Compilation

| Niagara | Niagara 파일 | Winters | Winters 파일 | 매핑 |
|---|---|---|---|---|
| `UNiagaraGraph` | `NiagaraEditor/Public/NiagaraGraph.h` | `FxGraph` | `Engine/Public/FX/FxGraph.h` (EFX-5 신규) | UEdGraph 기반 (Unreal 공통 그래프). Winters 는 자체 박제 |
| `UNiagaraNode` | `NiagaraEditor/Public/NiagaraNode.h` | `FxNodeDesc` (단일 자료형) | `FxAsset.h:27` | Niagara 는 8 종 서브클래스 (UCLASS). Winters 는 단일 자료형 + strType 필드로 분기 |
| `UNiagaraNodeFunctionCall` | `NiagaraEditor/Public/NiagaraNodeFunctionCall.h` | `FxNodeDesc{strType="FunctionCall"}` | — | 모듈 호출 |
| `UNiagaraNodeOp` | `NiagaraEditor/Public/NiagaraNodeOp.h` | `FxNodeDesc{strType="Add", "Mul", ...}` | — | 사칙연산 |
| `UNiagaraNodeCustomHlsl` | `NiagaraEditor/Public/NiagaraNodeCustomHlsl.h` | (Winters 는 미지원) | — | 사용자 HLSL 직접 작성 — Winters 는 CPU 바이트코드만 |
| `UNiagaraNodeParameterMapGet/Set` | `NiagaraEditor/Public/NiagaraNodeParameterMap*.h` | `FxNodeDesc{strType="ParamGet"/"ParamSet"}` | — | ParameterMap 5 namespace 통신 |
| `FNiagaraHlslTranslator` | `NiagaraEditor/Private/NiagaraHlslTranslator.h` | `CFxNodeCompiler` | `Engine/Public/FX/FxNodeCompiler.h` (EFX-5 신규) | ★ Niagara 는 HLSL 텍스트 → VectorVM. Winters 는 바이트코드 직접 |
| `FNiagaraCompiler` | `NiagaraEditor/Private/NiagaraCompiler.h` | (CFxNodeCompiler 와 통합) | — | 오케스트레이션 |
| `FNiagaraScriptExecutionContext` | `Niagara/Classes/NiagaraScriptExecutionContext.h` | `CFxExpressionVM` | `Engine/Public/FX/FxExpressionVM.h` (EFX-5 신규) | VM 실행 컨텍스트 |
| `FNiagaraVMExecutableData` | `Niagara/Public/NiagaraExecutableData.h` | `FxNodeDesc::bytecodeBlob` | `FxAsset.h:32` | bytecode 보관 |

**차용 패턴 — 그래프 → 바이트코드 컴파일**:

Niagara (2-stage):
```
UNiagaraGraph (UEdGraph)
    ↓ FNiagaraHlslTranslator (graph → HLSL text + intermediate code chunks)
    ↓ HLSL text
    ↓ FNiagaraCompiler (HLSL → VectorVM bytecode + GPU compute shader)
    ↓
FNiagaraVMExecutableData
    + GPU compute shader
```

Winters (1-stage):
```
FxGraph (자체 박제)
    ↓ CFxNodeCompiler (graph → bytecode 직접, 위상 정렬 + EmitNode)
    ↓
FxNodeDesc::bytecodeBlob
```

**왜 1-stage?**:
- HLSL 단계 생략 = SIMD 자동 최적화 포기 → CPU 수동 SIMD opcode 박제 옵션 (EFX-7)
- 디버깅 단순화 — bytecode 가 직접 노드 1:1 대응 (HLSL 중간 단계의 디버깅 어려움 회피)
- 박제 비용 ⬇ — HLSL 파서 박제 안 함

**Niagara HlslTranslator 의 핵심 로직** (참고):
```cpp
// NiagaraHlslTranslator.h (UE 5.7)
class FNiagaraHlslTranslator
{
    // mode 별 분기:
    enum class ENiagaraScriptUsage
    {
        Module, Function, DynamicInput,
        Particle::Spawn, Particle::Update, Particle::EventCallback,
        Emitter::Spawn, Emitter::Update,
        System::Spawn, System::Update,
        SimulationStage,
    };

    // 노드 순회 → HLSL 코드 chunk 누적
    int32 CompileGraph(UNiagaraGraph* Graph);

    // 각 mode 별 BodyChunk 생성 (SpawnBody/UpdateBody/SimulationStageBody)
    FString GenerateHlslText();
};
```

Winters 의 CFxNodeCompiler (EFX-5 박제 예정):
```cpp
class CFxNodeCompiler
{
    enum class eNodeMode : u8_t
    {
        Spawn,            // 새 particle 생성 시 1회
        Update,           // 매 frame 모든 active particle
        Event,            // 이벤트 (hit/death) trigger 시
    };

    bool_t CompileEmitter(const FxEmitterDesc& emitter,
                          std::vector<u8_t>& outSpawnBytecode,
                          std::vector<u8_t>& outUpdateBytecode);

private:
    bool_t TopologicalSort(...);   // DAG 위상 정렬
    void EmitNode(const FxNodeDesc& node, std::vector<u8_t>& outBytecode);
    // EmitNode 가 strType 별 분기:
    //   "InitPosition" → PushConst + StoreParticleAttr(Position)
    //   "UpdateGravity" → LoadParticleAttr(Velocity) + PushConst(0,-9.8,0) + AddV3 + StoreParticleAttr(Velocity)
    //   ...
};
```

### 2.4 Layer 4 — Editor UI

| Niagara | Niagara 파일 | Winters | Winters 파일 | 매핑 |
|---|---|---|---|---|
| `FNiagaraSystemToolkit` | `NiagaraEditor/Private/Toolkits/NiagaraSystemToolkit.h` | `CScene_EffectTool` | `Client/Public/Scene/Scene_EffectTool.h` (EFX-4 신규) | 에디터 entry — tab 레이아웃 + 패널 오케스트레이션 |
| `SNiagaraGraphNode` (Slate) | `NiagaraEditor/Private/Widgets/SNiagaraGraphNode.h` | (ImGui 노드 위젯) | `Client/Private/UI/EffectToolPanels/NodeGraphPanel.cpp` (EFX-5 신규) | 노드 그리기 — Slate vs ImGui |
| `UEdGraphSchema_Niagara` | `NiagaraEditor/Public/EdGraphSchema_Niagara.h` | `CFxNodeRegistry` | `Engine/Public/FX/FxNodeRegistry.h` (EFX-5 신규) | 핀 타입, 연결 규칙, 우클릭 메뉴 |
| `FNiagaraStackViewModel` | `NiagaraEditor/Public/ViewModels/Stack/NiagaraStackViewModel.h` | `CInspectorPanel` | `Client/Public/UI/EffectToolPanels/InspectorPanel.h` (EFX-4 신규) | 파라미터 트리 UI |
| `FNiagaraStackEntry` | `NiagaraEditor/Public/ViewModels/Stack/NiagaraStackEntry.h` | (Inspector 안 ImGui::CollapsingHeader) | — | 트리 노드 |

**차용 패턴 — Stack UI** (Niagara 시그니처):
- Niagara 의 Stack UI = 그래프와 별도의 **파라미터 중심 트리 뷰** (System / Emitter / Render / Spawn / Update / Event 카테고리)
- Winters 의 Inspector = ImGui `CollapsingHeader` + `TreeNode` 로 동등 박제
- 그래프와 Stack 둘 다 같은 데이터 (`FxAsset`) 의 다른 view — Niagara 의 큰 강점

---

## §3. ParameterMap 5 namespace 차용 (Pattern B 상세)

### 3.1 Niagara 5 namespace

```cpp
// Niagara 의 namespace 약속 (FNiagaraVariable 의 name prefix)
"System.*"         // 매 frame 갱신, 모든 emitter 공유
                   // 예: System.DeltaTime, System.WorldTime, System.SimulationDelta
"Emitter.*"        // emitter 단위
                   // 예: Emitter.SpawnRate, Emitter.Position, Emitter.LoopedAge
"Particle.*"       // 개별 particle (SoA 컬럼)
                   // 예: Particle.Position, Particle.Velocity, Particle.Color
"User.*"           // gameplay 코드가 set
                   // 예: User.PlayerPosition, User.BossPhase
"Engine.*"         // Niagara 엔진 내부 (Owner.Position, Owner.Scale)
"Module.*"         // 모듈 내부 입력 (function-local)
"Output.*"         // 노드 output pin
"Local.*"          // 그래프 local variable
```

### 3.2 Winters 5 namespace (단순화)

```cpp
// Engine/Public/FX/ParameterMap.h:14-20 (✅ 박제 완료)
enum class eFxNamespace : u8_t
{
    System,    // = Niagara System.*
    Emitter,   // = Niagara Emitter.*
    Particle,  // = Niagara Particle.*
    User,      // = Niagara User.*
    Event,     // = Niagara 의 Event payload (Niagara 는 별도 EventDataSet 처리)
};
```

**차이**:
- Niagara 는 8 namespace, Winters 는 5 단순화
- `Engine.*` (Owner) → `User.*` 안 통합 (gameplay 코드가 set)
- `Module.*` / `Output.*` / `Local.*` → 그래프 노드 input/output pin 의 임시 변수, namespace 분리 안 함 (VM 스택 사용)
- `Event.*` 는 신규 추가 — Niagara 는 EventDataSet 으로 처리, Winters 는 ParameterMap 통합

### 3.3 nameHash + type-safe lookup

```cpp
// Niagara 의 lookup
FNiagaraVariable Var(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Particle.Position"));
TConstArrayView<FVector> PositionData = DataBuffer.GetParameterData<FVector>(Var);

// Winters (✅ ParameterMap.h:46-74)
FxParameterID id = FxParameterID::Make(eFxNamespace::Particle, "Position", eFxParameterType::Float3);
// id.ToKey() = (ns 8 + type 8 + nameHash 32) bit u64 — 충돌 0
```

**충돌 분석**:
- nameHash 32-bit = fnv1a 충돌 확률 ≈ 1/4G — 단일 프로젝트 변수 1000 개 가정 시 charset 정상 = 충돌 0
- type 8-bit = 같은 이름 + 다른 타입 = 다른 key → 안전
- ns 8-bit = namespace 다르면 다른 key

---

## §4. Asset Registry + Generation Slot (Pattern C 상세)

### 4.1 Niagara 의 자산 관리

Niagara 는 **UObject GC 기반** — `UNiagaraSystem*` 자체가 자산. UE 의 reflection + GC 가 lifecycle 관리. Hot-reload = `PostEditChangeProperty()` → 모든 instance 재초기화.

### 4.2 Winters 의 자산 관리 (Pattern C)

**ECS 컨텍스트** — UE GC 없음. 따라서 generation slot 패턴으로 동등 안전성 박제.

```cpp
// Engine/Public/FX/FxAsset.h:117-127 (✅ 박제 완료)
struct Slot
{
    FxAsset asset;
    u32_t generation = 1;
    bool_t bAlive = false;
    wstring_t path;
};

// FxAssetHandle = RHIHandle<FxAssetTag> = 64-bit (index 32 + generation 32)
// 이미 Engine/Public/RHI/RHIHandles.h 에 박제된 패턴 차용 (P-18 회피)
```

**ResolveSlot 패턴**:
```cpp
// Engine/Public/FX/FxAsset.h:125-126
Slot* ResolveSlot(FxAssetHandle handle);
const Slot* ResolveSlot(FxAssetHandle handle) const;

// 구현 (예상):
Slot* CFxAssetRegistry::ResolveSlot(FxAssetHandle handle)
{
    if (!handle.IsValid()) return nullptr;
    u32_t idx = handle.GetIndex();
    u32_t gen = handle.GetGeneration();
    if (idx >= m_Slots.size()) return nullptr;
    Slot& slot = m_Slots[idx];
    if (!slot.bAlive || slot.generation != gen) return nullptr;   // ★ stale 검출
    return &slot;
}
```

**Niagara Object 패턴 vs Winters Generation 패턴**:

| 항목 | Niagara (UObject) | Winters (Generation) |
|---|---|---|
| 자산 lifetime | UE GC | 수동 `UnregisterAll()` + Slot.bAlive |
| Stale detection | `IsValid()` (UObject) | `Slot.generation != handle.gen` |
| Reload | `PostEditChangeProperty()` 자동 | `ReloadFromFile(handle)` 명시 |
| 메모리 | UE Heap (GC) | std::vector<Slot> (linear) |
| 박제 비용 | UE 의존 | 자체 박제 (~50 줄) |

---

## §5. Hot Reload (Pattern D 상세)

### 5.1 Niagara 의 Hot Reload

```
[에디터: 노드 그래프 변경]
   ↓
UNiagaraGraph::NotifyGraphChanged()
   ↓
UNiagaraSystem::PostEditChangeProperty()
   ↓
UNiagaraSystem::RequestCompile()
   ↓
FNiagaraSystemCompileTaskGraph (Worker 에서 컴파일)
   ↓ (완료 시)
UNiagaraSystem::OnCompileComplete()
   ↓
모든 활성 FNiagaraSystemInstance::Reset()
   ↓
시각 결과 즉시 반영
```

### 5.2 Winters 의 Hot Reload (EFX-4 박제 예정)

```
[VFX artist: 외부 에디터로 .wfx 저장 또는 Inspector Apply]
   ↓
[CScene_EffectTool::DetectAssetChanges() — 0.5s polling]
   ↓
filesystem::last_write_time(path) > m_AssetMTimes[h]
   ↓
CFxAssetRegistry::ReloadFromFile(handle)
   ↓ (Slot.asset 덮어쓰기, generation 증가 X — 기존 핸들 유효 유지)
   ↓
CFxSimulationSystem::OnAssetReloaded(handle)
   ↓
m_vecInstances 중 hAsset == handle 인 것:
   CFxSystemInstance::Reset() → Initialize() 재호출
   ↓
시각 결과 즉시 반영 (재시작 0)
```

**차이**:
- Niagara: 컴파일이 워커 thread (오래 걸림 — HLSL → VectorVM)
- Winters: 컴파일이 메인 thread (빠름 — bytecode 직접, HLSL 단계 생략)
- Niagara: Asset modification 감지 = UE Editor 내부 이벤트
- Winters: Asset modification 감지 = std::filesystem::last_write_time polling 0.5s

### 5.3 Generation 증가 vs 유지

**중요 결정**: Hot reload 시 generation 증가 X (handle 유지).

이유:
- `FxBillboardComponent::hAsset` 같이 컴포넌트가 핸들 보관 중 → generation 증가 시 모든 컴포넌트 stale 검출 → reload 후 effect 안 보임
- Niagara 도 동일 — `UNiagaraSystem` 자체는 동일 객체, 내부 데이터만 갱신

대신:
- Slot.asset 덮어쓰기
- `CFxSimulationSystem::OnAssetReloaded(h)` 가 명시적으로 모든 instance reset
- preview entity 가 `Reset() → Initialize()` 재진입 → spawn 재시작

---

## §6. Render 패스 매핑 (EFX-3 ↔ Niagara)

### 6.1 Niagara 의 6 Render Type

| Niagara 클래스 | 파일 | Winters 매핑 |
|---|---|---|
| `FNiagaraSpriteRenderer` | `Niagara/Public/NiagaraRendererSprites.h` | Billboard (FxBillboardComponent + FxSystem) ✅ 박제 |
| `FNiagaraMeshRenderer` | `Niagara/Public/NiagaraRendererMeshes.h` | Mesh (FxMeshComponent + FxMeshSystem) ✅ 박제 |
| `FNiagaraRibbonRenderer` | `Niagara/Public/NiagaraRendererRibbons.h` | Ribbon (FxRibbonComponent ✅ + FxRibbonSystem ❌) |
| `FNiagaraLightRenderer` | `Niagara/Public/NiagaraRendererLights.h` | (Winters 미박제 — Phase E Graphics 진입 시 검토) |
| `FNiagaraDecalRenderer` | `Niagara/Public/NiagaraRendererDecals.h` | GroundDecal (FxGroundDecalComponent ❌ + System ❌) |
| `FNiagaraComponentRenderer` | `Niagara/Public/NiagaraRendererComponents.h` | (Winters 미지원) |
| `FNiagaraVolumeRenderer` | `Niagara/Public/NiagaraRendererVolumes.h` | (Winters 미박제 — VolumetricFog Phase E 후 검토) |
| (Niagara 미지원) | — | Beam (FxBeamComponent + FxBeamSystem) ✅ 박제 — Niagara 는 Ribbon 으로 대체 |
| (Niagara 미지원) | — | Shockwave (FxShockwaveComponent ❌) — Winters 자체 추가 |

**Winters 신규 (Niagara 에 없는 것)**:
- **Beam** — Niagara 는 Ribbon 으로 두 점 연결. Winters 는 명시적 Beam 컴포넌트 (start/end entity 추종 + UV scroll). Elden lock-on indicator 같은 단순 case 에 효율적.
- **Shockwave** — Niagara 는 Mesh emitter + Update CurlNoise 등으로 충격파 박제. Winters 는 단일 Shockwave 컴포넌트 (radius 확장 + ring quad). 박제 비용 ⬇.

### 6.2 Vertex Factory 매핑

| Niagara | Winters |
|---|---|
| `FNiagaraSpriteVertexFactory` | DX11 dynamic VB (FxSystem.cpp 내부) |
| `FNiagaraMeshVertexFactory` | `CFxStaticMeshRenderer` (`Renderer/FxStaticMeshRenderer.h`) |
| `FNiagaraRibbonVertexFactory` | DX11 dynamic VB (FxRibbonSystem 박제 예정) |

**차이**:
- Niagara 는 RHI 추상층 (모든 백엔드 통과)
- Winters 는 DX11 직접 (RHI W6 부분만 통과 — IRHIDevice 까지만, draw call 은 native)

---

## §7. Data Interface 차용 (선택, EFX-7+ 진입 시)

### 7.1 Niagara DataInterface

```cpp
// NiagaraCore/Public/NiagaraDataInterfaceBase.h
UCLASS(abstract)
class NIAGARACORE_API UNiagaraDataInterfaceBase : public UNiagaraDataInterface
{
    virtual void BuildShaderParameters(...) = 0;   // GPU 바인딩
    virtual int32 PerInstanceDataSize() const { return 0; }
    // CPU/GPU 둘 다 지원
};
```

**구현 예시**:
- `UNiagaraDataInterfaceTexture` — 텍스처 샘플링
- `UNiagaraDataInterfaceSkeletalMesh` — 스켈레탈 메시 본 위치
- `UNiagaraDataInterfaceCollisionQuery` — Raycast/trace
- `UNiagaraDataInterfaceRenderTarget2D` — Render-to-texture
- `UNiagaraDataInterfaceGrid3DCollection` — 3D grid read/write

### 7.2 Winters Data Interface (EFX-7 또는 EFX-5 후반 박제 예정)

**현 단계 보류** — EFX-2~6 까지는 ParameterMap 만으로 충분. EFX-7 GPU 진입 시:

```cpp
// Engine/Public/FX/FxDataInterfaceBase.h (미박제)
class IFxDataInterface
{
public:
    virtual ~IFxDataInterface() = default;
    virtual std::string_view GetTypeName() const = 0;
    virtual u32_t GetPerInstanceDataSize() const { return 0; }
    virtual void BindShaderParameters(...) = 0;   // GPU 진입 시 박제
};
```

**구현 후보 (EFX-7+)**:
- `CFxDataInterfaceTexture` — 텍스처 샘플링 (Mesh particle 의 texture lookup)
- `CFxDataInterfaceCollisionQuery` — Phase D Physics BVH 사용 (collision 노드)
- `CFxDataInterfaceCurve` — 사용자 정의 curve (UI 슬라이더)

**박제 우선순위 ⬇** — EFX-2~6 동안은 ParameterMap + 표준 노드만으로 검증 우선.

---

## §8. Niagara 학습 순서 (Winters 박제 진입 전)

[12_EFFECT_TOOL_NIAGARA_V2_MASTER.md](12_EFFECT_TOOL_NIAGARA_V2_MASTER.md) 와 [13_EFFECT_TOOL_V3_MASTER.md](13_EFFECT_TOOL_V3_MASTER.md) 의 Stage 박제 진입 전, Niagara 의 다음 15 파일을 순서대로 통독 권장 (UE 5.7 source):

| # | 파일 (UE 5.7 path) | Winters Stage 매핑 |
|---|---|---|
| 1 | `Niagara/Classes/NiagaraSystem.h` | EFX-1 FxAsset |
| 2 | `Niagara/Classes/NiagaraEmitter.h` | EFX-1 FxEmitterDesc |
| 3 | `Niagara/Classes/NiagaraScript.h` | EFX-1 FxNodeDesc::bytecodeBlob |
| 4 | `Niagara/Public/NiagaraSystemInstance.h` | EFX-2 CFxSystemInstance |
| 5 | `Niagara/Classes/NiagaraEmitterInstance.h` | EFX-2 CFxEmitterInstance |
| 6 | `Niagara/Internal/NiagaraEmitterInstanceImpl.h` | EFX-2 CFxEmitterInstance 의 CPU impl |
| 7 | `Niagara/Classes/NiagaraDataSet.h` | EFX-2 CParticlePool ✅ |
| 8 | `NiagaraEditor/Public/NiagaraGraph.h` | EFX-5 FxGraph |
| 9 | `NiagaraEditor/Public/NiagaraNode.h` | EFX-5 FxNodeDesc |
| 10 | `NiagaraEditor/Private/NiagaraHlslTranslator.h` ⭐ | EFX-5 CFxNodeCompiler — **핵심** |
| 11 | `NiagaraEditor/Private/NiagaraCompiler.h` | EFX-5 CFxNodeCompiler |
| 12 | `Niagara/Public/NiagaraRenderer.h` | EFX-3 6 Render Type |
| 13 | `NiagaraCore/Public/NiagaraDataInterfaceBase.h` | EFX-7+ IFxDataInterface (보류) |
| 14 | `NiagaraEditor/Private/Toolkits/NiagaraSystemToolkit.h` | EFX-4 CScene_EffectTool |
| 15 | `NiagaraEditor/Public/ViewModels/Stack/NiagaraStackViewModel.h` | EFX-4 CInspectorPanel |

**진입 전 통독 = ~3 일 가정**. 통독 결과를 [`16_EFX_PROGRESS_AND_NEXT_ACTIONS.md`](16_EFX_PROGRESS_AND_NEXT_ACTIONS.md) 의 Lesson Learned 섹션에 박제 권장.

---

## §9. Winters 가 Niagara 와 의도적으로 다르게 박제한 부분

| 부분 | Niagara | Winters | 사유 |
|---|---|---|---|
| **자산 분리** | UNiagaraSystem / Emitter / Script 모두 별도 UAsset | FxAsset 1 자산에 모든 emitter + script inline | 박제 비용 ⬇ + 단순성 |
| **컴파일 단계** | Graph → HLSL → VectorVM (2-stage) | Graph → bytecode (1-stage) | HLSL 파서 박제 회피 + 디버깅 단순화 |
| **GPU 시뮬** | CPU/GPU 동시 지원 | CPU only (EFX-7 보류) | RHI/RG 안정 후 진입 |
| **OOP vs ECS** | UNiagaraComponent (USceneComponent 상속, OOP) | FxInstanceComponent (POD, ECS) | PITFALLS P-19 (Render/Sim 결합) 회피 |
| **GC** | UE GC | std::vector<Slot> + generation | UE 의존 회피 |
| **Editor** | Slate (SNiagaraGraphNode) | ImGui (Scene_EffectTool) | 기존 Winters ImGui 인프라 활용 |
| **Data Interface** | UNiagaraDataInterfaceBase 상속 + GPU 바인딩 자동 | IFxDataInterface 추상 (EFX-7+ 보류) | 박제 우선순위 ⬇ |
| **Render Type** | Sprite/Mesh/Ribbon/Light/Decal/Component/Volume (7 타입) | Billboard/Mesh/Ribbon/Beam/Decal/Shockwave (6 타입) | Light/Component/Volume 보류 + Beam/Shockwave 신규 |
| **namespace** | 8 (System/Emitter/Particle/User/Engine/Module/Output/Local) | 5 (System/Emitter/Particle/User/Event) | 단순화 |
| **Stack UI** | Slate FNiagaraStackViewModel | ImGui CInspectorPanel | 동등 박제, 라이브러리 차이 |

---

## §10. 결론 — 차용 비율

| Layer | 차용 비율 | 비고 |
|---|---|---|
| Asset (Layer 1) | 80% | inline 단순화 외 동등 |
| Runtime (Layer 2) | 70% | OOP → ECS POD 전환 |
| Graph + Compile (Layer 3) | 60% | HLSL 단계 생략 |
| Editor (Layer 4) | 50% | Slate → ImGui |
| Data Interface | 10% | EFX-7+ 보류 |
| GPU Path | 0% | 보류 |

**총 차용 비율 ≈ 50-60%**. 핵심 패턴 (ParameterMap 5 namespace + Asset Registry generation + Hot Reload + DAG 컴파일) 은 모두 차용. 인프라 (UE Slate / GC / RHI 추상층) 는 Winters 자체 박제.

---

**END OF NIAGARA REFERENCE DEEP MAP**
