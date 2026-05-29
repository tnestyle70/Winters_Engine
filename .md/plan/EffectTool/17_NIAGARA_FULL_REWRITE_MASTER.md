# 17. EffectTool Niagara 급 풀 재작성 마스터 (디자이너 워크플로우 + LoL/Elden 멀티 도메인)

작성일: 2026-05-07
권위: 본 17 = v4 마스터. 12 V2 / 13 V3 / 14 Niagara 깊이 맵 / 15 Lifecycle / 16 진행 액션 흡수.
참조 코드:
- Niagara plugin (Unreal 5 clone 실측): `C:/Users/user/Desktop/UnrealEngine/UnrealEngine/Engine/Plugins/FX/Niagara/Source/{Niagara,NiagaraEditor,NiagaraVertexFactories,NiagaraShader}/`
- VectorVM runtime 실측: `C:/Users/user/Desktop/UnrealEngine/UnrealEngine/Engine/Source/Runtime/VectorVM/`
- Winters 현 FX: `Client/Public/GameObject/FX/`, `Engine/Public/FX/`, `Engine/Public/ECS/Components/FxInstanceComponent.h`
- 셰이더: `Shaders/FxMesh.hlsl`, `Shaders/FxSprite.hlsl`
- 챔프 FxPresets: `Client/Private/GameObject/Champion/{Irelia,Yasuo,Annie,Yone,Garen,Kalista,Riven,Zed,Ashe,Fiora,Jax}/`

목적:
- Niagara 수준 effect tool 풀 박제 (System / Emitter / Module / Script 4-stage 모델)
- 디자이너 워크플로우 우선 (Stack + Graph + Curve + Viewport + Scratch pad + Live preview + Hot reload)
- LoL / Elden 멀티 도메인 (도메인 상수 InitDesc 주입, 박제 함정 P-11 회피)
- RHI 멀티 백엔드 호환 (DX11 가동 / DX12 Track 2 W7-9 Scaffold / Vulkan W14-17)
- ECS POD 호환 (FxInstanceComponent = CWorld owned, P-10 회피)

### §0.1 2026-05-07 Codex 코드베이스 실측 정정

본 절은 `CLAUDE.md` + Winters 코드 + Unreal Niagara clone 을 다시 읽고 17번 마스터에 즉시 반영한 정정이다.

```txt
1. 현재 Winters 에 `Engine/Public/FX/v2/` 는 아직 없다.
   실존 파일은 `Engine/Public/FX/{FxAsset,ParameterMap,ParticlePool,DeterministicRandom}.h`.
   EFX-0 은 v1 파일 삭제가 아니라 v2 namespace/folder scaffold + adapter 유지로 진입한다.

2. `FxInstanceComponent` 에 `unique_ptr<CFxSystemInstance>` 를 넣지 않는다.
   현재 ECS component 관례는 값 복사 POD. Runtime instance 는 World-owned storage/pool 이 소유하고,
   component 는 `FxSystemInstanceHandle` + `FxAssetHandle` + attach/lifetime 값만 보관한다.

3. `ISystem` 실제 계약은 `GetPhase / Execute / GetName / DescribeAccess` 다.
   계획서의 `OnUpdate` 표기는 모두 `Execute` 로 정정한다.

4. `Client/LoL/` 와 `Client/Elden/` 루트는 아직 없다.
   현 단계 도메인 분리는 `Client/Public/FX/Domains/{LoL,Elden}/` 로 시작하고,
   향후 `WintersLOL/WintersElden` 프로젝트 분리 때 이동한다.

5. 현재 `.wfx` 로더는 `FxAsset.cpp` 의 수동 string parser 다.
   EFX-1 은 이 parser 확장이 아니라 structured JSON reader/writer + round-trip 검증으로 교체한다.

6. Niagara 는 `Engine/Source/Runtime/Niagara` 가 아니라 plugin 아래에 있고,
   VectorVM 만 `Engine/Source/Runtime/VectorVM` 에 있다.

7. 기존 Client runtime class `CFxSystem` 과 충돌하지 않도록 v2 asset class 는
   `CFxSystemAsset / CFxEmitterAsset / CFxScriptAsset` 로 suffix 를 붙인다.
```

박제 진입 전 8 단계 관문 (`.md/process/PLAN_AUTHORING_PITFALLS.md`) 적용 결과:
- 관문 A (사전 결정 0 TBD, P-1+P-6): §1 표에 TBD 0
- 관문 B (PIMPL 추측 의사코드 금지, P-2): 본 마스터 = 신규 박제 (PIMPL 미사용 결정), 부속 박제 시 헤더+cpp 동시
- 관문 C (모든 render path 동시 박제, P-3): §6 Renderer 6 종 한 번에 인터페이스 박제
- 관문 D (Scene 직접 의존 금지, P-4): §12 의 ECS request component 패턴
- 관문 E (bitmask 폭, P-7): submesh visibility 는 기존 `VisibilityMask = std::array<u64_t, 32>` (2048 submesh) 재사용
- 관문 F (인용 의미 일치, P-8): Niagara 소스는 줄 번호 + 요약 근거 동반
- 관문 G (ECS Scheduler 동시성 모델, P-9): §12.2 Fx Tick phase 표 1 곳에 한정
- 관문 H (Owner Scope 매트릭스, P-10): §6.5 표 박제 (FxSystemInstanceStorage = `CWorld` owned, FxAssetRegistry = `CGameInstance` Tier-1)

---

## §1 사전 결정 (TBD 0 강제)

본 마스터 본문 §3 이후 박제 진입을 위해, 다음 결정 항목 표 1 곳에 한정 (codex 스타일 §8.3 의 비교 1~2 곳 예외 적용).

| 결정 항목 | 결정값 | 근거 |
|---|---|---|
| 박제 범위 | 본 17 = 인덱스 + 핵심 인터페이스 + 단계 일정. 부속 18~26 신규 (Asset/Runtime/Renderer/Compiler/VM+GPU/DataInterface/Editor/Domain/HotReload) | 한 파일 2000 줄 한도 + 후속 부속 분리로 박제 함정 P-1 (사전 결정 미박제) 회피 |
| 자산 직렬화 포맷 | `.wfx` JSON v1 (16 EFX-1 박제 계승) + `.wfxbin` 쿠킹 산출 (Stage EFX-9) | JSON = 디자이너 git diff / merge 가능, Binary = 배포 빌드 메모리 절감 (Niagara cooked vs editor 모델 직접 차용) |
| Compile target | CPU VM bytecode + GPU HLSL 양 경로 | DX11 가동 (CPU 기본) / DX12 진입 시 GPU compute (HLSL → DXIL). Niagara 양 경로 직접 차용 |
| RHI 백엔드 | RH-7 추상 통과 (`IRHIDevice / IRHIPipelineState / IRHIBindGroup`) | Track 2 W7-9 Scaffold 와 정합. Renderer 본체 = DX11 native handle 추출 금지, IRHI handle 통과만 |
| Editor 프레임워크 | ImGui (16 EFX-4 계승) + ImNodes/ImNodeEditor 노드 그래프 | WPF/Slate clone 만들면 4~6 개월 소요, ImGui = 1~2 주. 디자이너는 라이브 프리뷰가 핵심 |
| Engine 공용 위치 | 신규 `Engine/Public/FX/v2/` + `Engine/Private/FX/v2/` 를 추가하되, 기존 `Engine/Public/FX/` v1 파일은 EFX-0 adapter 로 보존 | 현 실존 파일은 `Engine/Public/FX/{FxAsset,ParameterMap,ParticlePool,DeterministicRandom}.h`. 삭제/이동부터 하면 Client 빌드가 깨짐 |
| 게임 도메인 분리 | 현 단계 = `Client/Public/FX/Domains/{LoL,Elden}/`. 향후 프로젝트 split 시 `WintersLOL/WintersElden` 로 이동 | `Client/LoL/` / `Client/Elden/` 루트는 현재 없음. 박제 함정 P-5/P-11 회피 |
| World 소유권 | FxSystemInstanceStorage = `CWorld` owned, FxAssetRegistry = `CGameInstance` Tier-1 | P-10 회피. ECS component 는 handle 만 들고 실제 instance 는 world 수명에 묶음 |
| Submesh visibility | 기존 `Engine/Public/ECS/Components/MeshGroupVisibilityComponent.h` 의 `VisibilityMask = std::array<u64_t, 32>` (2048 submesh) 재사용 | P-7 회피. 신규 mask 추가 X |
| EntityID alias | 기존 `ECS/Entity.h` 의 `EntityID` (u32_t) 재사용 | P-17 회피. typedef 일괄 변경 = ABI 폭발 |
| Determinism | 기존 `Engine/Public/FX/DeterministicRandom.h` 의 `CXoroshiro128` 재사용. EFX-0 에서 v2 include bridge 제공 | Sim-04a v2 / Sim-10 v2 deterministic 가정 충족. 시각 FX 와 판정 FX 분리 RNG |
| 13~16 와의 관계 | 13 V3 마스터 deprecated, replaced-by 17. 14 Niagara 깊이 맵 = 본 §3 의 reference. 15 Lifecycle = 본 §5.4 흡수. 16 진행 액션 = 본 §13 의 Stage EFX-0~9 로 확장 | 마스터 권위 일원화 |

추가 결정:
- 박제 함정 P-13 (미존재 API): Niagara 의 `FNiagaraSystemInstance::Reset` 같은 헬퍼는 본 17 박제 시 헤더 + cpp 동시 검증 강제. grep 검증 필수
- 박제 함정 P-19 (Render/Sim 결합): Renderer 는 read-only `FxRenderSnapshot` 받기. Sim 갱신은 별도 phase
- 박제 함정 P-9 (ECS Scheduler): Fx Spawn / Fx Tick / Fx Render 3 phase 분리, 같은 phase 내 read+write 0 시스템만 묶음

---

## §2 비전 (Niagara 급 + 디자이너 + 멀티 도메인)

### §2.1 Niagara 급의 의미

Niagara (Unreal 5) 의 핵심 패턴 7 가지 (Codex 로컬 소스 정찰 결과):

```txt
1. 4 stage script 모델
   SystemSpawn / SystemUpdate / EmitterSpawn / EmitterUpdate / ParticleSpawn / ParticleUpdate
   각 stage 가 독립 함수. Spawn = 1 회 / Update = 매 tick

2. SoA 메모리 레이아웃
   FNiagaraDataSet 가 Float / Int / Half buffer 별도 할당
   FNiagaraDataBuffer = double-buffer + generation counter

3. Parameter store binding
   FNiagaraParameterStore = offset table + dirty flag
   Editor 빌드 시점에 offset 고정 → 런타임 lookup 0

4. CPU VM + GPU compute 양 경로
   VectorVM = opcode 기반 (Add/Sub/Mul/Div/Normalize/External/Output)
   GPU = HLSL compute shader, RDG (Render Dependency Graph) 통합

5. Module inline expansion
   FNiagaraHlslTranslator 가 module 함수 호출을 inline 으로 펼침
   SIMD 친화적 코드 생성

6. Multi-renderer
   Sprite / Mesh / Ribbon / Beam / Light / Decal / Component 7 종
   각각 vertex factory + DataSet binding

7. Data interface
   Curve / Texture / SkeletalMesh / StaticMesh / Spline / Grid2D / Grid3D / CollisionQuery
   외부 데이터 주입점, CPU 함수 포인터 + GPU shader template 양 경로
```

본 17 의 Winters 마이그 비율:
- 80% 차용: 4 stage script / SoA / Parameter store / VM 양 경로 / Module inline / Multi-renderer 6 종 / DI 6 종
- 50% 단순화: VM opcode 30 → 12 (Niagara 의 LWC / async GPU trace 등 미박제)
- 0% 차용: Blueprint 통합 (Winters 는 ECS), Component renderer (Blueprint actor spawn 미박제), Slate UI (ImGui 로 대체)

### §2.2 디자이너 워크플로우의 의미

디자이너가 한 번도 코드 안 만지고 다음을 할 수 있어야 함:

```txt
A. 신규 이펙트 박제 (10 분 이내)
   1. EffectTool 창 열기 (ImGui)
   2. 신규 System 자산 생성 → 자동 빈 Emitter 1 개 추가
   3. Emitter Spawn 모듈 = SpawnRate 노드 (값 100/sec)
   4. ParticleSpawn 모듈 = SpawnPosition 노드 + InitialVelocity 노드
   5. ParticleUpdate 모듈 = Gravity 노드 + DragVelocity 노드
   6. Render = SpriteRenderer 추가, texture 드래그 드롭
   7. Viewport 탭 = 즉시 라이브 프리뷰
   8. 저장 = `.wfx` JSON 산출

B. 챔프 스킬에 결합 (5 분 이내)
   1. 챔프 SkillTable 에서 hookId 입력
   2. 본 시스템이 SkillCastEvent → FxSystem.SpawnFromAsset(hookId, position) 자동 호출
   3. InGame 즉시 동작

C. 파라미터 튜닝 (실시간)
   1. Stack 탭에서 모듈 클릭 → 우측 Detail 패널
   2. 슬라이더 / 색 스포이드 / 곡선 에디터 즉시 입력
   3. Viewport 동시 갱신 (즉시 hot reload, recompile X)

D. 곡선 (Curve) 기반 애니메이션
   1. Color 모듈 = "Color Over Lifetime" 슬롯 노출
   2. 곡선 에디터 클릭 → 시간 0 ~ 1 사이 색 보간
   3. 즉시 반영
```

박제 함정 P-19 (Render / Sim 결합) 회피: Editor UI 는 자산 수정만, Sim 자체에 직접 호출 X. Hot reload 는 자산 변경 → recompile (async) → SystemInstance reset 순.

### §2.3 LoL / Elden 멀티 도메인

도메인 차이:

```txt
LoL
- 짧은 FX (1 ~ 3 초)
- 카메라 = isometric (탑다운)
- 한타 50 개 동시 FX
- 챔프 12 종 + 미니언 6 종 + 정글 몹 6 종
- Submesh 전형 5 ~ 20 (FxMeshComponent 로 충분)
- Budget = 한타 5 ms CPU / 4 ms GPU

Elden
- 긴 FX (5 ~ 15 초, 보스 telegraph + windup + impact + decal + shockwave 다단)
- 카메라 = third-person 어깨걸이
- 동시 1 ~ 3 보스 + 플레이어 1 ~ 4
- Submesh 전형 50 ~ 200 (보스 1 체)
- Budget = 보스 fight 8 ms CPU / 12 ms GPU
```

Engine 공용 = 도메인 상수 0. 게임-specific = `Client/Public/FX/Domains/LoL/LoLFxBudget.h`, `Client/Public/FX/Domains/Elden/EldenFxBudget.h`. InitDesc 주입. 향후 게임별 솔루션 분리 때 해당 폴더만 `WintersLOL/` / `WintersElden/` 로 이동.

박제 함정 P-11 회피 사례:

```cpp
// 안티 패턴 (P-11 위반)
class CFxSystemInstance {
public:
    static constexpr u32_t MAX_PARTICLES = 4096; // LoL 도메인 상수
};

// 패턴 (P-11 회피)
struct FxSystemInitDesc {
    u32_t uMaxParticles = 4096;       // 게임 도메인 InitDesc 주입
    u32_t uMaxEmitters = 16;
    f32_t fBudgetMs = 5.0f;
};

class CFxSystemInstance {
public:
    static unique_ptr<CFxSystemInstance> Create(const FxSystemInitDesc& desc);
};

// LoL Bootstrap
namespace Winters::LoL {
    inline FxSystemInitDesc MakeFxSystemDesc() {
        return { 4096, 16, 5.0f };
    }
}

// Elden Bootstrap
namespace Winters::Elden {
    inline FxSystemInitDesc MakeFxSystemDesc() {
        return { 16384, 64, 8.0f };
    }
}
```

---

## §3 Niagara reference 매핑 (Codex 로컬 소스 정찰 결과 압축)

본 절 = §4 ~ §10 의 박제 근거. Unreal 소스는 줄 번호 + 요약 근거를 남긴다 (관문 F, P-8 회피).

### §3.1 자산 계층

```txt
Niagara                            Winters v2
UNiagaraSystem                  →  CFxSystemAsset (asset)
UNiagaraEmitter                 →  CFxEmitterAsset (asset)
UNiagaraScript                  →  CFxScriptAsset (asset, compiled bytecode/HLSL)
UNiagaraScriptSource            →  CFxScriptSource (asset, original Graph)
UNiagaraGraph                   →  CFxGraph (asset, node DAG)
UNiagaraNode (FunctionCall/Op/  →  CFxNode (FxNodeFunctionCall / FxNodeOp / ...)
              Input/Output/If)
```

6 stage script 명칭 1:1 차용:

```txt
SystemSpawn       1 회, 시스템 시작 시
SystemUpdate      매 tick
EmitterSpawn      Emitter 시작 시
EmitterUpdate     매 tick (Emitter 단위)
ParticleSpawn     Particle 1 개 생성 시
ParticleUpdate    매 tick (Particle 별)
```

### §3.2 Runtime 시뮬레이션

```txt
FNiagaraSystemInstance          →  CFxSystemInstance (runtime, World owned)
FNiagaraEmitterInstance         →  CFxEmitterInstance
FNiagaraDataSet (SoA)           →  CFxDataSet
FNiagaraDataBuffer (double-buf) →  FxDataBuffer
FNiagaraParameterStore          →  CFxParameterStore
FNiagaraParameterStoreBinding   →  FxParameterBinding
```

소스 근거 요약 (`NiagaraEmitterInstance.cpp:9-54`):
- 생성자는 ParentSystemInstance 를 받고, `Init()` 에서 `ParticleDataSet` 존재를 확인한다.
- `GetNumParticles()` 는 GPU 실행 컨텍스트가 있으면 GPU-side count/fence 경로를 타고, CPU 경로는 current particle data 의 instance count 를 읽는다.

Winters 적용: `CFxEmitterInstance` 가 `eExecMode { CPU, GPU }` enum + `CFxDataSet* pCurrent / pPrevious` 보유.

### §3.3 VM (CPU)

```txt
VectorVM (opcode + register)    →  CFxVM (opcode 12 종)
FNiagaraScriptExecutionContext  →  FxScriptExecContext
GetVMExternalFunction           →  IFxDataInterface::GetCPUFunction
```

Niagara 30+ opcode 중 12 종만 차용:

```txt
1.  ADD / SUB / MUL / DIV / NEG / RECIP / SQRT (산술)
2.  DOT / CROSS / NORMALIZE (벡터)
3.  CMPLT / CMPGT / CMPEQ + SELECT (비교 + masked select, branch 대신)
4.  EXTERNAL (Data Interface 호출)
5.  OUTPUT (Dataset attribute write)
6.  RAND_FLOAT / RAND_RANGE (xoroshiro128)
7.  LERP / CLAMP / SMOOTHSTEP
8.  LOAD_CONST / LOAD_PARAM (Parameter store offset)
9.  LOAD_ATTR (이전 stage 출력)
10. CALL_MODULE (inline expanded, no actual call op)
11. SIN / COS / ATAN2 / FRAC
12. NOOP (padding)
```

박제 함정 P-13 (미존재 API) 회피: Niagara 의 LWC / async GPU trace / nanite_mesh / chaos_collision 4 opcode 그룹 = 본 17 미박제 결정. 향후 부속 박제 시 grep 검증.

### §3.4 GPU compute

```txt
FNiagaraGpuComputeDispatchInterface  →  CFxGpuComputeDispatch
FNiagaraGPUSystemTick                →  FxGpuSystemTick
SimulationStage (multi-stage)        →  FxSimulationStage
FNiagaraGpuReadbackManager           →  FxGpuReadback (rare, Sim cache 용)
```

소스 근거 요약 (`NiagaraGPUSystemTick.cpp:19-91`, `NiagaraGpuComputeDispatchInterface.h:31-42`):
- GPU tick 은 SystemInstance 로 초기화되고, GPU compute dispatch interface 는 World/Scene/FXSystem 경계에서 얻는 렌더 객체다.
- Winters 는 이를 `CFxGpuComputeDispatch` 로 축소하되, DX11 기본 경로는 CPU VM 으로 유지한다.

Winters 적용: DX11 = CPU VM 강제 (UAV 미지원 일부 디바이스). DX12 진입 시 GPU compute 활성. RH-7 capability `bSupportsCompute` 검사.

### §3.5 Compiler / Translator

```txt
FNiagaraHlslTranslator              →  CFxHlslTranslator (Graph → HLSL)
FNiagaraVMTranslator                →  CFxVMTranslator (Graph → bytecode)
FNiagaraCodeChunk                   →  FxCodeChunk (DAG node)
FNiagaraParameterMapHistory         →  FxParameterMapHistory
```

Niagara 의 module inline expansion 직접 차용:

```txt
SystemUpdate Stack:
  - [Module] SolveForces
    - [Function] CalcGravity
      - [Op] Multiply (deltaTime * gravity)
    - [Function] ApplyDrag
      - [Op] Multiply (vel * (1 - drag))
  - [Module] AgeAndKill
    - [Op] Add (age + deltaTime)
    - [Op] CmpGt (age > maxAge)
    - [Op] Output (Kill flag)

Compiler 가 SolveForces / AgeAndKill 모듈을 인라인 펼침 → 단일 함수 본문 = SIMD 친화 + register 재사용
```

### §3.6 Renderer

```txt
FNiagaraRenderer abstract       →  IFxRenderer
FNiagaraRendererSprites         →  CFxSpriteRenderer
FNiagaraRendererMeshes          →  CFxMeshRenderer
FNiagaraRendererRibbons         →  CFxRibbonRenderer
FNiagaraRendererLights          →  CFxLightRenderer
FNiagaraRendererDecals          →  CFxDecalRenderer
FNiagaraRendererComponents      →  미박제 (Blueprint actor spawn, Winters 는 ECS)
```

Winters 신규 1 종 추가:
- `CFxBeamRenderer` (현 `FxBeamSystem` 흡수, Niagara Ribbon 의 변형)

소스 근거 요약 (`NiagaraRendererSprites.h:41-58`):
- Sprite renderer 는 frame dynamic data, source particle buffer, blend/sort/cull flag, Float/Half/Int SRV 와 stride 를 별도 보관한다.

Winters DX11 적용: SRV 대신 Structured Buffer + VS 에서 SV_VertexID / SV_InstanceID 로 인덱싱.

### §3.7 Data Interface

```txt
UNiagaraDataInterfaceBase           →  IFxDataInterface (abstract)
FNiagaraDataInterfaceBindingInstance →  FxDataInterfaceBindingInstance
GetVMExternalFunction               →  IFxDataInterface::GetCPUFunction
BuildShaderParameters               →  IFxDataInterface::BuildShaderParameters
```

차용 6 종:

```txt
CFxDICurve              float curve 보간 (Color over lifetime, Size over lifetime)
CFxDITexture            2D texture sampling (mask / noise)
CFxDIStaticMesh         mesh vertex/triangle 접근 (FxMeshRenderer 와 별개, mesh surface spawn 용)
CFxDISpline             spline 따라 위치/탄젠트
CFxDIGrid2D             neighbor query (collision detect, smoke field)
CFxDICollisionQuery     ray cast (간단한 충돌)
```

미차용 4 종: SkeletalMesh / Grid3D / AsyncGpuTrace / Landscape (Elden 진입 시 추가 검토).

### §3.8 Editor UI

```txt
FNiagaraSystemViewModel         →  CFxSystemViewModel
UNiagaraStackEntry              →  CFxStackEntry (System / Emitter / Particle / Render category)
SNiagaraGraphActionMenu         →  CFxGraphActionMenu (ImGui)
SNiagaraEmitterPreviewViewport  →  CFxPreviewViewport (ImGui Image + DX11/DX12 RTV)
FNiagaraStackCurveEditorOptions →  CFxCurveEditor (ImGui curve)
FNiagaraScratchPad              →  CFxScratchPad (인라인 모듈 작성)
```

소스 근거 요약 (`NiagaraStackEntry.cpp:22-33`):
- Stack category 는 System / Emitter / Particle / Render 축이고, subcategory 는 Settings / Spawn / Update / Event / SimulationStage / Render 축이다.

Winters 적용: 4 카테고리 + 6 서브카테고리 enum 그대로 차용.

### §3.9 Hot reload

```txt
UNiagaraScript::RequestCompile   →  CFxScriptAsset::RequestRecompile (async)
FNiagaraSystemViewModel::OnSystemCompiled → FxOnSystemRecompiled delegate
Cooked vs Editor 분리            →  .wfx (Editor) vs .wfxbin (cooked, Stage EFX-9)
```

---

## §4 4-Layer 아키텍처

본 17 의 핵심 구조. Niagara 와 1:1 대응.

```txt
Layer 1  Asset             정적 자산. CFxSystemAsset / CFxEmitterAsset / CFxScriptAsset / CFxGraph. 디스크 = .wfx JSON
Layer 2  Instance          런타임. CFxSystemInstance (CWorld owned) / CFxEmitterInstance / CFxDataSet
Layer 3  Compile           CFxHlslTranslator / CFxVMTranslator / CFxCodeChunk DAG / FxParameterMapHistory
Layer 4  Editor            ImGui. CFxSystemViewModel / CFxStackEntry / CFxGraphActionMenu / CFxCurveEditor
```

각 Layer 의 의존 방향:

```txt
Layer 4 (Editor)           Layer 3 (Compile) 호출 가능
         ↓                 Layer 2 (Instance) 호출 가능 (라이브 프리뷰)
Layer 3 (Compile)          Layer 1 (Asset) 만 의존
         ↓
Layer 2 (Instance)         Layer 1 + Layer 3 의 산출 (compiled script) 만 의존
         ↓
Layer 1 (Asset)            의존 0
```

Layer 1 (Asset) 은 게임 / 엔진 / 도메인 무관. Layer 2 (Instance) 는 ECS World 의존. Layer 3 (Compile) 은 Editor / 빌드 타임 양쪽 호출. Layer 4 (Editor) 는 Editor 빌드만.

빌드 분리:

```txt
Engine.dll                Layer 1 + Layer 2 + Layer 3 (런타임 컴파일도 가능)
Client.exe                Layer 1 + Layer 2 (cooked .wfxbin 로드만)
WintersEditor.exe         Layer 4 (ImGui 에디터, Layer 1 ~ 3 호출)
```

---

## §5 Layer 1 자산 (Asset)

### §5.1 헤더 박제 (인터페이스만, 본문 부속 18 박제)

`Engine/Public/FX/v2/Asset/FxSystemAsset.h`:

```cpp
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "FX/v2/Asset/FxEmitterHandle.h"
#include "FX/v2/Asset/FxParameterMap.h"
#include <vector>
#include <string>
#include <memory>

class CFxScriptAsset;

class WINTERS_ENGINE CFxSystemAsset
{
public:
    ~CFxSystemAsset();
    CFxSystemAsset(const CFxSystemAsset&) = delete;
    CFxSystemAsset& operator=(const CFxSystemAsset&) = delete;

    static std::unique_ptr<CFxSystemAsset> Create(const std::wstring& strName);

    const std::wstring& GetName() const;
    const std::vector<FxEmitterHandle>& GetEmitterHandles() const;
    CFxScriptAsset* GetSystemSpawnScript() const;
    CFxScriptAsset* GetSystemUpdateScript() const;
    const FxParameterMap& GetUserParameterMap() const;

    void AddEmitter(FxEmitterHandle handle);
    void RemoveEmitter(FxEmitterHandle handle);
    void SetSystemSpawnScript(std::unique_ptr<CFxScriptAsset> pScript);
    void SetSystemUpdateScript(std::unique_ptr<CFxScriptAsset> pScript);

    bool LoadFromJson(const std::wstring& strPath);
    bool SaveToJson(const std::wstring& strPath) const;

private:
    CFxSystemAsset();

    std::wstring m_strName;
    std::vector<FxEmitterHandle> m_vecEmitterHandles;
    std::unique_ptr<CFxScriptAsset> m_pSystemSpawnScript;
    std::unique_ptr<CFxScriptAsset> m_pSystemUpdateScript;
    FxParameterMap m_UserParams;
};
```

`Engine/Public/FX/v2/Asset/FxEmitterAsset.h`:

```cpp
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "FX/v2/Asset/FxAttributeBinding.h"
#include "FX/v2/Renderer/FxRendererProperties.h"
#include <memory>
#include <vector>

class CFxScriptAsset;

class WINTERS_ENGINE CFxEmitterAsset
{
public:
    ~CFxEmitterAsset();
    CFxEmitterAsset(const CFxEmitterAsset&) = delete;
    CFxEmitterAsset& operator=(const CFxEmitterAsset&) = delete;

    static std::unique_ptr<CFxEmitterAsset> Create(const std::wstring& strName);

    enum class eExecMode : u8_t { CPU = 0, GPU = 1 };

    eExecMode GetExecMode() const;
    void SetExecMode(eExecMode mode);

    CFxScriptAsset* GetEmitterSpawnScript() const;
    CFxScriptAsset* GetEmitterUpdateScript() const;
    CFxScriptAsset* GetParticleSpawnScript() const;
    CFxScriptAsset* GetParticleUpdateScript() const;

    const std::vector<std::unique_ptr<FxRendererProperties>>& GetRenderers() const;

private:
    CFxEmitterAsset();

    std::wstring m_strName;
    eExecMode m_eExecMode = eExecMode::CPU;

    std::unique_ptr<CFxScriptAsset> m_pEmitterSpawnScript;
    std::unique_ptr<CFxScriptAsset> m_pEmitterUpdateScript;
    std::unique_ptr<CFxScriptAsset> m_pParticleSpawnScript;
    std::unique_ptr<CFxScriptAsset> m_pParticleUpdateScript;

    std::vector<std::unique_ptr<FxRendererProperties>> m_vecRenderers;

    u32_t m_uMaxParticles = 4096;
};
```

`Engine/Public/FX/v2/Asset/FxScriptAsset.h`:

```cpp
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "FX/v2/Asset/FxParameterMap.h"
#include <vector>
#include <memory>

class CFxGraph;
struct FxVMExecutableData;

enum class eFxScriptUsage : u8_t
{
    SystemSpawn      = 0,
    SystemUpdate     = 1,
    EmitterSpawn     = 2,
    EmitterUpdate    = 3,
    ParticleSpawn    = 4,
    ParticleUpdate   = 5,
    SimulationStage  = 6,
    Module           = 7,
};

class WINTERS_ENGINE CFxScriptAsset
{
public:
    ~CFxScriptAsset();
    CFxScriptAsset(const CFxScriptAsset&) = delete;
    CFxScriptAsset& operator=(const CFxScriptAsset&) = delete;

    static std::unique_ptr<CFxScriptAsset> Create(eFxScriptUsage usage);

    eFxScriptUsage GetUsage() const;
    CFxGraph* GetSourceGraph() const;
    void SetSourceGraph(std::unique_ptr<CFxGraph> pGraph);

    const FxVMExecutableData* GetVMData() const;
    const std::vector<u8_t>& GetHlsl() const;

    bool RequestRecompile();
    bool IsCompileInFlight() const;
    bool IsCompileSucceeded() const;

private:
    CFxScriptAsset();

    eFxScriptUsage m_eUsage;
    std::unique_ptr<CFxGraph> m_pSourceGraph;
    std::unique_ptr<FxVMExecutableData> m_pVMData;
    std::vector<u8_t> m_vecHlsl;
    u32_t m_uCompileVersion = 0;
};
```

세부 본문 (cpp + JSON Save / Load + Recompile pipeline) = 부속 18 박제.

### §5.2 자산 직렬화 `.wfx` JSON v1 schema

```json
{
  "version": 1,
  "name": "Irelia_Q_Stab",
  "user_params": {
    "f_damage": 50.0,
    "v_color_blue": [0.4, 0.7, 1.0, 1.0]
  },
  "system_spawn_script": "<graph json>",
  "system_update_script": "<graph json>",
  "emitters": [
    {
      "name": "Stab_Trail",
      "exec_mode": "CPU",
      "max_particles": 256,
      "emitter_spawn_script": "<graph json>",
      "emitter_update_script": "<graph json>",
      "particle_spawn_script": "<graph json>",
      "particle_update_script": "<graph json>",
      "renderers": [
        {
          "type": "Sprite",
          "material": "Resource/FX/Trail_Add.material",
          "blend_mode": "Additive",
          "size_binding": "Particles.Size",
          "color_binding": "Particles.Color",
          "position_binding": "Particles.Position"
        }
      ]
    }
  ]
}
```

박제 함정 P-12 (음수 좌표 정수 truncation) 회피: 위치 / 속도 같은 float 값은 항상 `f32_t` 또는 `f64_t`. 좌표 → 정수 변환 시 `std::floor` 강제.

### §5.3 로더 / 세이버 책임

```txt
CFxJsonLoader
  Read .wfx → CFxSystemAsset, CFxEmitterAsset, CFxScriptAsset 트리 복원
  CFxGraph 의 노드 / 엣지 직렬화 = node_id (u32_t) + edge (src_node_id + src_pin + dst_node_id + dst_pin)
  현재 `Engine/Private/FX/FxAsset.cpp` 의 수동 string parser 를 확장하지 않고 structured JSON reader 로 교체

CFxJsonSaver
  CFxSystemAsset → JSON → 디스크
  Editor 만 호출 (Engine runtime 은 Loader 만). canonical writer 로 round-trip 비교 가능하게 고정 순서 출력

CFxBinaryLoader
  Read .wfxbin → CFxSystemAsset (cooked, Graph 제거 + VMData / HLSL 만 직렬화)
  Stage EFX-9 박제

CFxBinaryWriter
  Editor 만. Cooking 단계에서 .wfx → .wfxbin
```

---

## §6 Layer 2 Runtime (Instance)

### §6.1 헤더 박제

`Engine/Public/FX/v2/Instance/FxSystemInstance.h`:

```cpp
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "FX/v2/Asset/FxParameterMap.h"
#include <vector>
#include <memory>

class CFxSystemAsset;
class CFxEmitterInstance;
class CWorld;

struct FxSystemInitDesc
{
    u32_t uMaxParticles = 4096;
    u32_t uMaxEmitters = 16;
    f32_t fBudgetMs = 5.0f;
};

class WINTERS_ENGINE CFxSystemInstance
{
public:
    ~CFxSystemInstance();
    CFxSystemInstance(const CFxSystemInstance&) = delete;
    CFxSystemInstance& operator=(const CFxSystemInstance&) = delete;

    static std::unique_ptr<CFxSystemInstance> Create(
        CFxSystemAsset* pAsset,
        CWorld* pWorld,
        const FxSystemInitDesc& desc);

    enum class eState : u8_t
    {
        Inactive    = 0,
        Active      = 1,
        Completing  = 2,
        Complete    = 3,
        PoolReturned = 4,
    };

    eState GetState() const;
    CFxSystemAsset* GetAsset() const;
    CWorld* GetWorld() const;

    void Activate();
    void Deactivate(bool_t bImmediate);
    void Tick(f32_t fDeltaTime);

    const std::vector<std::unique_ptr<CFxEmitterInstance>>& GetEmitterInstances() const;

    FxParameterMap& GetUserParams();
    void SetWorldTransform(const Vec3& vPos, const Vec3& vEulerXYZ, const Vec3& vScale);

private:
    CFxSystemInstance();

    CFxSystemAsset* m_pAsset = nullptr;
    CWorld* m_pWorld = nullptr;
    eState m_eState = eState::Inactive;

    FxParameterMap m_UserParams;

    std::vector<std::unique_ptr<CFxEmitterInstance>> m_vecEmitterInstances;

    Vec3 m_vWorldPos{};
    Vec3 m_vWorldEulerXYZ{};
    Vec3 m_vWorldScale{ 1.f, 1.f, 1.f };
};
```

`Engine/Public/FX/v2/Instance/FxEmitterInstance.h` + `FxDataSet.h` + `FxParameterStore.h` = 부속 19 박제.

### §6.2 SoA DataSet 메모리 모델

`CFxDataSet` 구성:

```txt
- m_vecFloatBuffers   std::vector<std::vector<f32_t>>   슬롯 N x 입자 M
- m_vecIntBuffers     std::vector<std::vector<i32_t>>   슬롯 N x 입자 M
- m_vecHalfBuffers    std::vector<std::vector<u16_t>>   슬롯 N x 입자 M (half float)
- m_uNumInstances     u32_t                             현재 살아있는 입자 수
- m_uMaxInstances     u32_t                             할당 한계
- m_uGenerationId     u32_t                             double-buffer 의 generation
```

Float buffer N 개 = 3 (position xyz) + 3 (velocity xyz) + 4 (color rgba) + 1 (size) + 1 (age) + 1 (lifetime) + 사용자 정의 슬롯 K. 이 N 은 컴파일 시점에 ParameterMap 으로부터 결정.

Double-buffer:
- `pCurrent` = 이번 frame 시뮬레이션 결과
- `pPrevious` = 이전 frame 결과 (Renderer / interpolation 용)
- `Tick` 끝에 swap

박제 함정 P-19 (Render / Sim 결합) 회피: Renderer 는 항상 `pPrevious` 만 read.

### §6.3 Parameter store binding

`CFxParameterStore` 구성:

```txt
- m_vecParams           std::vector<u8_t>               byte buffer (raw memory)
- m_mapOffsetByName     std::map<u32_t (nameHash), u32_t (offset)>
- m_vecBindings         std::vector<FxParameterBinding> 외부 source 와 동기화
- m_bDirty              bool_t                          binding 갱신 필요 flag
```

Editor 빌드 시점에 `m_mapOffsetByName` 고정 → 런타임 lookup = `mapOffsetByName.find(hash)` 1 회만.

`FxParameterBinding`:

```cpp
struct FxParameterBinding
{
    u32_t uSrcNameHash;      // 외부 store 의 변수 hash
    u32_t uSrcOffset;        // src store 의 byte offset
    u32_t uDstOffset;        // 본 store 의 byte offset
    u32_t uByteSize;          // 복사 byte 수
    CFxParameterStore* pSrcStore = nullptr;
};
```

Tick 시작 때 `Bind() = src store → dst store memcpy` (변경된 변수만, dirty flag 체크).

### §6.4 ECS 통합

`Engine/Public/ECS/Components/FxInstanceComponent.h`:

```cpp
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"
#include "ECS/Entity.h"
#include "FX/FxAsset.h"
#include "RHI/RHIHandles.h"

struct FxSystemInstanceTag {};
using FxSystemInstanceHandle = RHIHandle<FxSystemInstanceTag>;

struct FxInstanceComponent
{
    FxSystemInstanceHandle hInstance{};
    FxAssetHandle hAsset{};
    EntityID attachTo = NULL_ENTITY;
    Vec3 vAttachOffset = { 0.f, 0.f, 0.f };
    f32_t fAge = 0.f;
    f32_t fLifetime = 3.f;
    bool_t bAutoDestroyOnComplete = true;
};
```

실측 정정: 기존 `Engine/Public/ECS/Components/FxInstanceComponent.h` 도 값 타입 component 다. `unique_ptr<CFxSystemInstance>` 는 ECS component store 복사/이동 규약과 충돌하므로 금지. 실제 instance 객체는 부속 19 의 World-owned `CFxSystemInstanceStorage` 가 소유한다.

`CFxTickSystem : public ISystem` (Engine/Public/ECS/Systems/FxTickSystem.h):

```cpp
class CFxTickSystem final : public ISystem
{
public:
    static std::unique_ptr<CFxTickSystem> Create();
    u32_t GetPhase() const override { return 5; }   // AI(1) → Nav(2) → Move(3) → Animation(4) → Fx(5) → Render(6)
    const char* GetName() const override { return "FxTickSystem"; }
    void Execute(CWorld& world, f32_t fDeltaTime) override;
    void DescribeAccess(CSystemAccessBuilder& builder) const override;
};
```

박제 함정 P-9 (ECS Scheduler 동시성) 회피: Fx Tick = phase 5. 같은 phase 에 다른 system 0 (FxTick 만 단독). FxRender 는 phase 6 (read-only consumer). FxSpawn = phase 0 (CommandBuffer 처리 직후).

### §6.5 World 소유권 / 멀티 도메인 매트릭스

| Owner | 자산 / 인스턴스 | 이유 |
|---|---|---|
| `CGameInstance` Tier-1 | `CFxAssetRegistry` (모든 .wfx asset 캐시) | 자산은 게임 도메인 무관, 전역 1 회 로드 |
| `CWorld` owned | `CFxSystemInstanceStorage` (재사용 인스턴스 storage/pool) | 인스턴스는 scene/world 수명과 같이 죽어야 함. 멀티 GameRoom / 멀티 Scene / Elden 멀티 World 호환 |
| `CFxSystemInstanceStorage` owned | `CFxSystemInstance` | ECS component 는 handle 만 보관하고 실제 객체는 storage 가 소유 |
| `CFxSystemInstance` owned | `CFxEmitterInstance` | 1:N 종속 |
| `CFxEmitterInstance` owned | `CFxDataSet` (current + previous) | 1:1 |

박제 함정 P-10 회피: Spatial / Vision / NavGrid 와 동일한 원칙. Engine 공용 클래스에 `static constexpr` 도메인 상수 박제 금지.

---

## §7 Layer 2 VM + GPU compute

### §7.1 CPU VM 헤더

`Engine/Public/FX/v2/VM/FxVM.h`:

```cpp
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include <vector>
#include <span>

enum class eFxOp : u8_t
{
    NOOP        = 0,
    ADD         = 1,
    SUB         = 2,
    MUL         = 3,
    DIV         = 4,
    NEG         = 5,
    DOT         = 6,
    CROSS       = 7,
    NORMALIZE   = 8,
    LERP        = 9,
    CLAMP       = 10,
    CMPLT       = 11,
    CMPGT       = 12,
    CMPEQ       = 13,
    SELECT      = 14,
    SIN         = 15,
    COS         = 16,
    ATAN2       = 17,
    FRAC        = 18,
    SQRT        = 19,
    RAND_FLOAT  = 20,
    RAND_RANGE  = 21,
    LOAD_CONST  = 22,
    LOAD_PARAM  = 23,
    LOAD_ATTR   = 24,
    EXTERNAL    = 25,
    OUTPUT      = 26,
};

struct FxVMInstruction
{
    eFxOp eOpCode = eFxOp::NOOP;
    u16_t uDstReg = 0;
    u16_t uSrcA = 0;
    u16_t uSrcB = 0;
    u16_t uSrcC = 0;
    u32_t uExtraOperand = 0;   // LOAD_CONST 의 const idx, EXTERNAL 의 DI idx 등
};

struct FxVMExecutableData
{
    std::vector<FxVMInstruction> vecInstructions;
    std::vector<f32_t> vecConstants;
    u32_t uNumRegisters = 0;
    u32_t uNumExternalCalls = 0;
};

class WINTERS_ENGINE CFxVM
{
public:
    static void Execute(
        const FxVMExecutableData& data,
        std::span<f32_t> registers,
        std::span<const u32_t> externalCallIndices,
        u32_t uNumInstancesToProcess);
};
```

세부 본문 (각 opcode 의 SIMD 구현, AVX2 / SSE2 fallback) = 부속 21 박제.

### §7.2 GPU compute headers (RH-7 통과)

`Engine/Public/FX/v2/GPU/FxGpuComputeDispatch.h`:

```cpp
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "RHI/IRHIDevice.h"
#include "RHI/IRHICommandList.h"
#include <memory>
#include <vector>

class CFxEmitterInstance;

struct FxGpuSystemTick
{
    CFxEmitterInstance* pEmitter = nullptr;
    u32_t uNumInstances = 0;
    u32_t uNumStages = 1;
};

class WINTERS_ENGINE CFxGpuComputeDispatch
{
public:
    ~CFxGpuComputeDispatch();
    static std::unique_ptr<CFxGpuComputeDispatch> Create(IRHIDevice* pDevice);

    void Enqueue(const FxGpuSystemTick& tick);
    void Dispatch(IRHICommandList* pCmdList);
    void EndFrame();

private:
    CFxGpuComputeDispatch();

    IRHIDevice* m_pDevice = nullptr;
    std::vector<FxGpuSystemTick> m_vecPending;
};
```

DX11 fallback: `bSupportsCompute = false` → `Enqueue` no-op + CPU VM 강제 (`eExecMode::CPU` 자동 다운그레이드 + 1 회 경고 로그).

DX12 / Vulkan: `bSupportsCompute = true` → 정상 dispatch.

GPU 셰이더 본문 (`Shaders/FX/FxParticleSim_CS.hlsl`) = 부속 21 박제.

---

## §8 Layer 3 Compile (Graph + Translator)

### §8.1 Graph 자료 모델 헤더

`Engine/Public/FX/v2/Compiler/FxGraph.h`:

```cpp
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include <vector>
#include <memory>

class CFxNode;

struct FxNodeId { u32_t value = 0; };
struct FxPinId { u32_t value = 0; };

struct FxEdge
{
    FxNodeId src;
    FxPinId  srcPin;
    FxNodeId dst;
    FxPinId  dstPin;
};

class WINTERS_ENGINE CFxGraph
{
public:
    ~CFxGraph();
    CFxGraph(const CFxGraph&) = delete;
    CFxGraph& operator=(const CFxGraph&) = delete;

    static std::unique_ptr<CFxGraph> Create();

    FxNodeId AddNode(std::unique_ptr<CFxNode> pNode);
    void RemoveNode(FxNodeId id);
    bool ConnectPin(const FxEdge& edge);
    void DisconnectPin(const FxEdge& edge);

    CFxNode* FindNode(FxNodeId id) const;
    const std::vector<FxEdge>& GetEdges() const;

    bool TopologicalSort(std::vector<CFxNode*>& outOrder) const;

private:
    CFxGraph();

    std::vector<std::unique_ptr<CFxNode>> m_vecNodes;
    std::vector<FxEdge> m_vecEdges;
};
```

### §8.2 Node 계층

`Engine/Public/FX/v2/Compiler/FxNode.h`:

```cpp
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include <vector>
#include <string>

enum class eFxPinType : u8_t
{
    Float = 0,
    Float2 = 1,
    Float3 = 2,
    Float4 = 3,
    Int = 4,
    Bool = 5,
    Curve = 6,
    Texture = 7,
    Spline = 8,
};

struct FxPin
{
    std::wstring strName;
    eFxPinType eType = eFxPinType::Float;
    bool_t bIsInput = true;
};

class WINTERS_ENGINE CFxNode
{
public:
    virtual ~CFxNode() = default;

    enum class eKind : u8_t
    {
        Input         = 0,    // Parameter / Attribute read
        Output        = 1,    // Dataset attribute write
        Op            = 2,    // Add / Sub / ...
        FunctionCall  = 3,    // Module 호출 (inline expanded)
        If            = 4,    // Conditional (mask+select)
        Const         = 5,    // 상수
        DataInterface = 6,    // External (Curve / Texture / ...)
    };

    virtual eKind GetKind() const = 0;
    virtual const std::vector<FxPin>& GetInputPins() const = 0;
    virtual const std::vector<FxPin>& GetOutputPins() const = 0;
    virtual const std::wstring& GetDisplayName() const = 0;
};
```

7 구현 (`FxNodeInput / FxNodeOutput / FxNodeOp / FxNodeFunctionCall / FxNodeIf / FxNodeConst / FxNodeDataInterface`) = 부속 22 박제.

### §8.3 Translator (Graph → HLSL / VM bytecode)

`Engine/Public/FX/v2/Compiler/FxHlslTranslator.h`:

```cpp
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include <vector>
#include <memory>

class CFxScriptAsset;
class CFxGraph;

class WINTERS_ENGINE CFxHlslTranslator
{
public:
    static std::unique_ptr<CFxHlslTranslator> Create();

    bool Translate(CFxScriptAsset* pScript, std::vector<u8_t>& outHlsl, std::vector<std::wstring>& outErrors);
};
```

`CFxVMTranslator` = 동일 시그니처, 출력 = `FxVMExecutableData`.

소스 근거 요약 (`NiagaraHlslTranslator.h:105-150`, `NiagaraHlslTranslator.cpp:1058-1060`, `NiagaraHlslTranslator.cpp:4727`):
- `FNiagaraCodeChunk` 는 translator 의 중간 표현이고, translator 는 ParameterMap HLSL 정의 생성과 function entry 처리를 별도 단계로 둔다.

Winters 의 `FxCodeChunk` = 동일 7 mode.

### §8.4 ParameterMapHistory

Niagara 의 `FNiagaraParameterMapHistory` 차용. Graph 에서 사용된 모든 변수의 namespace + type + 처음 / 마지막 read / write stage 추적. Compile 결과의 `FxParameterMap` 빌드.

5 namespace (16 EFX-1 박제 계승):

```txt
System.*           시스템 전체 1 회
Engine.*           Owner / DeltaTime / WorldTime
Emitter.*          Emitter 단위
Particles.*        Particle 단위
User.*             디자이너 직접 노출
```

부속 22 에서 `FxParameterMapHistory` 본체 박제.

---

## §9 Layer 2 Renderer (6 종, 한 번에 박제 = 관문 C / P-3)

### §9.1 추상 + 6 구현 헤더

`Engine/Public/FX/v2/Renderer/IFxRenderer.h`:

```cpp
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "RHI/IRHIDevice.h"
#include "RHI/IRHICommandList.h"

class CFxEmitterInstance;
struct FxRenderSnapshot;

class WINTERS_ENGINE IFxRenderer
{
public:
    virtual ~IFxRenderer() = default;

    virtual void Initialize(IRHIDevice* pDevice) = 0;
    virtual void BuildSnapshot(CFxEmitterInstance* pEmitter, FxRenderSnapshot& outSnapshot) const = 0;
    virtual void Render(IRHICommandList* pCmdList, const FxRenderSnapshot& snapshot) = 0;
    virtual void Shutdown() = 0;
};
```

6 구현 (모두 박제 함정 P-3 동시 박제):

```txt
CFxSpriteRenderer        Niagara FNiagaraRendererSprites 직접 차용. 빌보드 + atlas + UV scroll
CFxMeshRenderer          Niagara FNiagaraRendererMeshes 차용. Static mesh per-particle, MeshGroupVisibilityComponent 통합
CFxRibbonRenderer        Niagara FNiagaraRendererRibbons 차용. Ribbon trail (현 FxRibbonComponent 흡수)
CFxBeamRenderer          Niagara Ribbon 변형. Two-endpoint beam (현 FxBeamSystem 흡수)
CFxLightRenderer         Niagara FNiagaraRendererLights 차용. Dynamic point light per-particle
CFxDecalRenderer         Niagara FNiagaraRendererDecals 차용. Decal projection, Elden 보스 telegraph 핵심
```

각 구현의 헤더 / cpp / 셰이더 (`Shaders/FX/v2/{FxSprite,FxMesh,FxRibbon,FxBeam,FxLight,FxDecal}.hlsl`) = 부속 20 박제.

### §9.2 RenderSnapshot 분리 (Sim ↔ Render)

`Engine/Public/FX/v2/Renderer/FxRenderSnapshot.h`:

```cpp
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"
#include <vector>

struct FxRenderSnapshotInstance
{
    Vec3 vPos{};
    Vec3 vScale{ 1.f, 1.f, 1.f };
    Vec4 vColor{ 1.f, 1.f, 1.f, 1.f };
    f32_t fSize = 1.f;
    f32_t fNormalizedAge = 0.f;
    u32_t uMeshSubmeshIdx = 0;
};

struct FxRenderSnapshot
{
    eFxRenderType eType = eFxRenderType::Billboard;
    std::vector<FxRenderSnapshotInstance> vecInstances;
    void* pMaterialResource = nullptr;
    u32_t uSortKey = 0;
};
```

박제 함정 P-19 (Render / Sim 결합) 회피: Snapshot = read-only, Sim 갱신과 별 phase. ECS Phase 5 (FxTick) 가 Snapshot 빌드 → Phase 6 (FxRender) 가 read.

### §9.3 GPU sort

대규모 입자 (1000+ per emitter) 의 view-depth sort. Niagara 의 `NiagaraSortKeyGen.usf` 차용. 부속 20 박제.

DX11 fallback: CPU sort (std::sort 또는 radix sort). DX12 / Vulkan: GPU compute sort (bitonic).

---

## §10 Layer 2 DataInterface (6 종)

### §10.1 추상 헤더

`Engine/Public/FX/v2/DataInterface/IFxDataInterface.h`:

```cpp
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include <functional>
#include <span>
#include <vector>

class CFxParameterStore;

using FxCPUFunction = std::function<void(std::span<f32_t> inputs, std::span<f32_t> outputs, u32_t uNumInstances)>;

class WINTERS_ENGINE IFxDataInterface
{
public:
    virtual ~IFxDataInterface() = default;

    virtual const wchar_t* GetTypeName() const = 0;

    virtual FxCPUFunction GetCPUFunction(const std::wstring& strFunctionName) const = 0;

    virtual bool BuildShaderParameters(std::vector<u8_t>& outHlslSnippet) const = 0;
    virtual bool BindShaderResources(class IRHIBindGroup* pBindGroup) const = 0;

    virtual void TickPerFrame(class CFxSystemInstance* pSystemInstance) = 0;
};
```

6 구현 (`CFxDICurve / CFxDITexture / CFxDIStaticMesh / CFxDISpline / CFxDIGrid2D / CFxDICollisionQuery`) = 부속 23 박제.

### §10.2 디자이너 노출 함수 시그니처 표

| DI | 함수 | 입력 | 출력 |
|---|---|---|---|
| Curve | `Sample(t)` | float | float |
| Curve | `SampleColor(t)` | float | float4 |
| Texture | `Sample2D(uv)` | float2 | float4 |
| Texture | `Sample2DLod(uv, lod)` | float2, float | float4 |
| StaticMesh | `GetSurfacePoint(triIdx, bary)` | int, float3 | float3 (pos), float3 (normal) |
| Spline | `Position(t)` | float | float3 |
| Spline | `Tangent(t)` | float | float3 |
| Grid2D | `Find Nearest(pos, radius, count)` | float3, float, int | int[] |
| CollisionQuery | `RayCast(orig, dir, len)` | float3, float3, float | bool, float, float3 |

각 함수 = CPU 구현 + GPU template `.hlsl` snippet 양 경로 박제.

---

## §11 Layer 4 Editor (디자이너 UX)

### §11.1 Editor 진입점

`Tools/WintersEditor/Public/FX/CFxEditorTab.h` (Editor exe, ImGui):

```cpp
class WINTERS_EDITOR CFxEditorTab
{
public:
    static std::unique_ptr<CFxEditorTab> Create(IRHIDevice* pDevice);

    void OnImGui();
    void OpenAsset(const std::wstring& strPath);
    void SaveCurrent();

private:
    CFxEditorTab();

    std::unique_ptr<class CFxSystemViewModel> m_pViewModel;
    std::unique_ptr<class CFxStackPanel> m_pStackPanel;
    std::unique_ptr<class CFxGraphPanel> m_pGraphPanel;
    std::unique_ptr<class CFxCurveEditor> m_pCurveEditor;
    std::unique_ptr<class CFxPreviewViewport> m_pPreview;
    std::unique_ptr<class CFxParameterPanel> m_pParamPanel;
    std::unique_ptr<class CFxScratchPad> m_pScratchPad;
};
```

7 패널:

```txt
StackPanel             좌측. System / Emitter 트리 + Spawn / Update / Event / Render 카테고리 + 모듈 추가/제거
GraphPanel             중앙 (모듈 클릭 시). Niagara graph 의 ImNodeEditor 차용
CurveEditor            우측 상단 (curve pin 클릭 시). 시간 0~1 보간
PreviewViewport        우측 중앙. 라이브 프리뷰. DX11/DX12 RTV → ImGui::Image
ParameterPanel         우측 하단. 모듈 선택 시 슬라이더 / 색 스포이드 / 텍스처 드롭
ScratchPad             모달. 인라인 모듈 작성 (Niagara Scratch Pad 차용)
Toolbar                상단. Save / Recompile / Activate / Deactivate / Reset View
```

### §11.2 Stack 모델

`Engine/Public/FX/v2/Editor/FxStackEntry.h` (Editor + Engine 양쪽 사용 가능, Editor 빌드만 빌드):

```cpp
enum class eFxStackCategory : u8_t
{
    System = 0, Emitter = 1, Particle = 2, Render = 3,
};

enum class eFxStackSubcategory : u8_t
{
    Settings = 0, Spawn = 1, Update = 2, Event = 3, SimulationStage = 4, Render = 5,
};

class WINTERS_EDITOR CFxStackEntry
{
public:
    virtual ~CFxStackEntry() = default;

    virtual eFxStackCategory GetCategory() const = 0;
    virtual eFxStackSubcategory GetSubcategory() const = 0;
    virtual const std::wstring& GetDisplayName() const = 0;

    virtual std::vector<CFxStackEntry*> GetChildren() const = 0;

    virtual void OnImGuiHeader() = 0;
    virtual void OnImGuiBody() = 0;
};
```

소스 근거 요약 (`NiagaraStackEntry.cpp:22-33`):
- Stack category/subcategory 명칭은 §11.2 enum 설계와 1:1 대응한다.

Stack hierarchy 예시 (Irelia Q):

```txt
[System] Irelia_Q_Stab
  ├─ [Settings]    System Properties (활성화 / 우선순위 / 한도)
  ├─ [Spawn]       SystemSpawnScript modules
  │    └─ Set User Parameters (User.Damage = 50)
  ├─ [Update]      SystemUpdateScript modules
  │    └─ Tick Lifetime
  └─ [Emitter] Stab_Trail
       ├─ [Settings]    Emitter Properties (CPU/GPU, MaxParticles)
       ├─ [Spawn]       EmitterSpawnScript modules
       ├─ [Update]      EmitterUpdateScript modules
       │    └─ Spawn Rate (100/sec)
       ├─ [Particle Spawn]
       │    ├─ Initialize Particle (default)
       │    ├─ Spawn Position (Cone)
       │    └─ Initial Velocity (Cone, speed 5)
       ├─ [Particle Update]
       │    ├─ Solve Forces (gravity, drag)
       │    ├─ Color Over Lifetime (curve)
       │    └─ Age And Kill
       └─ [Render]
            └─ Sprite Renderer (texture, additive)
```

### §11.3 Hot Reload Sequence

```txt
1. 디자이너가 Stack 의 Spawn Rate 슬라이더 100 → 200 으로 변경
2. CFxParameterPanel::OnSliderChange 호출
3. CFxScriptAsset::SetUserParameter 호출 (asset modify)
4. CFxScriptAsset::RequestRecompile() 호출 (async 큐 추가)
5. Worker thread = CFxHlslTranslator + CFxVMTranslator 실행
6. 완료 시 Game thread 에 notify
7. CFxOnSystemRecompiled delegate 호출
8. PreviewViewport 의 CFxSystemInstance::Reset() 호출
9. Reset = 모든 입자 kill + parameter store rebind + 재 Activate
10. 즉시 다음 frame 부터 새 값 반영
```

박제 함정 P-13 (미존재 API) 회피: 위 8 단계의 모든 메서드 = 본 17 의 §5 / §6 헤더에 박제됨. 부속 박제 시 grep 검증 강제.

### §11.4 Curve Editor

ImGui 자체 구현 (ImCurveEdit). 4 종 곡선:

```txt
Float curve            1 차원, 시간 → float (size, alpha)
Float3 curve           3 차원, 시간 → float3 (velocity, position offset)
Color curve            HSV / RGB 모두, 시간 → float4
Spline                 위치 array, 매끄러운 보간
```

저장 형식 = `CFxDICurve` 의 control points + tangent.

### §11.5 ScratchPad

디자이너가 인라인 모듈 작성. 새 .wfx 자산 만들지 않고 본 system 내부에 모듈 정의 + 다른 system 으로 export 가능.

---

## §12 ECS / DLL / RHI 통합

### §12.1 ECS Phase 표 (관문 G, P-9 회피)

```txt
Phase 0    CommandBuffer drain (Spawn / Despawn 명령 처리)
Phase 1    AISystem
Phase 2    NavigationSystem
Phase 3    MovementSystem (Transform 적용)
Phase 4    AnimationSystem
Phase 5    FxTickSystem ← 본 17 의 시뮬레이션 (CPU VM execute, GPU compute enqueue)
Phase 6    FxRenderSnapshotSystem ← Snapshot build (read-only)
Phase 7    Renderer (FxRenderer.Render 호출)
```

같은 phase 시스템 0 (FxTick 단독). FxRenderSnapshot 도 단독. 현 코드의 FX render 는 `InGameRenderBridge.cpp:185-190` 에서 Scene render 후반에 직접 호출되므로, EFX-3 까지는 bridge adapter 로 유지하고 EFX-4 이후 Editor/Runtime 공용 render path 로 합친다.

### §12.2 ECS request component (관문 D, P-4 회피)

챔프 스킬에서 FX 스폰 시 Scene 직접 호출 금지:

```cpp
// 안티 패턴 (P-4 위반)
extern void Scene_InGame_SpawnFX(const std::wstring& path);

// 패턴 (P-4 회피)
struct FxSpawnRequestComponent
{
    std::wstring strAssetPath;
    Vec3 vWorldPos{};
    Vec3 vWorldEulerXYZ{};
    EntityID attachTo = NULL_ENTITY;
    f32_t fAutoDestroyAfterSec = 5.f;
};

class CFxSpawnRequestSystem final : public ISystem
{
public:
    static std::unique_ptr<CFxSpawnRequestSystem> Create();
    u32_t GetPhase() const override { return 0; }   // Phase 0 = CommandBuffer drain 직후
    const char* GetName() const override { return "FxSpawnRequestSystem"; }
    void Execute(CWorld& world, f32_t fDeltaTime) override;
    void DescribeAccess(CSystemAccessBuilder& builder) const override;
};
```

`CFxSpawnRequestSystem::Execute` 가 모든 `FxSpawnRequestComponent` 를 forEach 후 `CFxAssetRegistry::Find` + `CFxSystemInstanceStorage::Acquire` + request component 제거.

### §12.3 DLL 경계

```txt
Engine.dll               Layer 1 + Layer 2 + Layer 3 (CFxSystemAsset / CFxSystemInstance / CFxHlslTranslator)
                         WINTERS_ENGINE export class 만 6 ~ 8 개

Client.exe               FxSpawnRequestComponent 만 사용. 직접 인스턴스 생성 X

WintersEditor.exe        Layer 4 전체. WINTERS_EDITOR 매크로 가드
```

박제 함정 (CLAUDE.md §5.2 의 dllexport + unique_ptr) 회피: 모든 `WINTERS_ENGINE` class = copy ctor / assign 명시 `= delete`.

### §12.4 RHI 추상 (RH-7 통과)

Renderer 본체 = `IRHIDevice / IRHIPipelineState / IRHIBindGroup / IRHICommandList` 만 사용. DX11 native handle 추출 0 강제 (W6 caller 마이그 잔여 + 본 17 의 신규 박제 = 처음부터 IRHI facade 통과).

```cpp
// Renderer cpp (예시, CFxSpriteRenderer.cpp 부속 20 박제)
void CFxSpriteRenderer::Render(IRHICommandList* pCmdList, const FxRenderSnapshot& snapshot)
{
    pCmdList->SetPipelineState(m_pPipeline);                  // IRHIPipelineState
    pCmdList->SetBindGroup(0, m_pPerFrameBindGroup);          // PerFrame
    pCmdList->SetBindGroup(1, m_pPerEmitterBindGroup);        // PerEmitter
    pCmdList->DrawIndexedInstanced(6, snapshot.vecInstances.size(), 0, 0, 0);
}
```

`ID3D11DeviceContext*` / `ID3D12GraphicsCommandList*` 직접 사용 = 본 17 박제 시 0 hit 강제 (grep `ID3D1` Engine/Public/FX/v2/ → 결과 0).

---

## §13 단계 EFX-0 ~ EFX-9 (Stage 박제 일정)

| Stage | 명칭 | 의존 | 합격 기준 (검증 가능 목표, codex Goal-Driven Execution) |
|---|---|---|---|
| EFX-0 | Legacy Bridge | 16 EFX-0 계승 | 11 챔프 (Annie/Ashe/Fiora/Garen/Irelia/Jax/Kalista/Riven/Yasuo/Yone/Zed) 의 우선 변환 hook manifest 작성 후 해당 hook 이 `FxAssetHandle` / `SpawnFromAsset` 경로를 통과. 현재 champion preset 직접 spawn callsite 가 다수 있으므로 전역 `CFxSystem::Spawn` 0 hit 는 EFX-0 합격 기준으로 쓰지 않는다. |
| EFX-1 | `.wfx` JSON v1 | EFX-0 | EFX-0 manifest 자산 → `.wfx` dump + round-trip (저장→로드→canonical 저장→의미 비교) 통과. raw md5 는 canonical writer 산출물에만 적용. |
| EFX-2 | Runtime SoA | EFX-1 | 1024 입자 / 16 emitter Tick 1 frame 0.5 ms 이내 (CPU). `CFxDataSet` swap 후 race 0. |
| EFX-3 | Renderer 6 종 | EFX-2 | 6 renderer (Sprite / Mesh / Ribbon / Beam / Light / Decal) 모두 InGame 1 회 spawn + 화면 출력. RH-7 IRHI 통과 (DX11 native handle 추출 0). |
| EFX-4 | Editor MVP | EFX-3 | 7 패널 (Stack / Graph / Curve / Viewport / Parameter / ScratchPad / Toolbar) 모두 표시. 신규 .wfx 자산 박제 + 1 emitter + 1 sprite renderer 가능 + 즉시 라이브 프리뷰. |
| EFX-5 | Compile (Graph → VM/HLSL) | EFX-4 | 표준 모듈 9 종 (SpawnRate / SpawnPosition / InitialVelocity / Gravity / Drag / ColorOverLifetime / SizeOverLifetime / AgeAndKill / Output) 컴파일 통과. Editor 의 모듈 추가 시 즉시 recompile. |
| EFX-6 | DataInterface 6 종 | EFX-5 | 6 DI (Curve / Texture / StaticMesh / Spline / Grid2D / CollisionQuery) 모두 1 회 그래프 사용 + InGame spawn + 정상 출력. |
| EFX-7 | GPU compute | EFX-6, Track 2 W7-9 | DX12 진입 시 GPU compute path 활성. 8192 입자 / 64 emitter Tick 1 frame 1.5 ms 이내 (GPU). DX11 fallback CPU 강제 검증. |
| EFX-8 | Hot reload | EFX-4 (Editor) | Editor Stack 슬라이더 변경 → recompile (async) → SystemInstance Reset → 다음 frame 반영. recompile latency 200 ms 이내. |
| EFX-9 | Cooked binary `.wfxbin` | EFX-1, EFX-5 | Editor 의 Cook 명령 → manifest `.wfx` → `.wfxbin` 산출. Client.exe 가 `.wfxbin` 만 로드 (Graph 제거 검증, sizeof 절감 50% 이상). |

기간 예상 (코드베이스 실측 후 정정):

```txt
EFX-0    2~3 일     11 챔프 우선 hook manifest + v1 adapter 보존 + callsite 분류 검증
EFX-1    3~4 일     structured JSON 로더 / 세이버 + manifest 자산 dump + round-trip
EFX-2    1 주       Runtime SoA + Parameter store + FxTickSystem + storage handle
EFX-3    1~2 주     Renderer 6 종 (Sprite/Mesh/Ribbon/Beam/Light/Decal) + 셰이더
EFX-4    2~3 주     Editor 7 패널. 디자이너 UX 핵심 구간
EFX-5    1~2 주     Compile (Translator HLSL + VM 양 경로, 표준 모듈 9 종)
EFX-6    1~2 주     DI 6 종 + Elden telegraph/sword trail/shockwave smoke
EFX-7    3~4 주     GPU compute (Track 2 DX12 합격 후, 대량 입자/간접 드로우)
EFX-8    3~5 일     Hot reload (async compile + Reset)
EFX-9    3~5 일     Cooking (Editor 명령 + Client 로더)
─────────────
LoL runtime MVP      4~6 주
디자이너 Editor MVP  8~12 주
Elden/GPU 포함       16~22 주
```

박제 함정 P-14 (행동 정책 무의식적 변경) 회피: EFX-0 의 manifest hook 변환 = 행동 동일성 검증 필수 (전후 InGame screenshot 비교).

---

## §14 박제 함정 매트릭스 (P-1 ~ P-19 적용)

본 17 마스터에 적용된 함정 회피 매핑 (13~16 v3 의 누적 사례 + 본 17 신규).

| 함정 | 위치 | 회피 적용 |
|---|---|---|
| P-1 + P-6 | §1 사전 결정 | TBD 0, 12 항목 모두 결정값 박제 |
| P-2 | §5~§10 코드 박제 | PIMPL 미사용 결정. 모든 헤더 + cpp 동시 박제 (부속 18~26) |
| P-3 | §9 Renderer | 6 renderer 한 번에 인터페이스 박제 (Sprite/Mesh/Ribbon/Beam/Light/Decal) |
| P-4 | §12.2 | ECS request component (FxSpawnRequestComponent) 패턴 강제. extern / `static_cast<Scene_X*>` 0 |
| P-7 | §1 + §6.4 | submesh visibility = 기존 `VisibilityMask = std::array<u64_t, 32>` (2048) 재사용. 신규 mask 추가 X |
| P-8 | §3 | Niagara 소스 줄 번호 + 요약 근거 동반. `NiagaraEmitterInstance.cpp:9-54` / `NiagaraGPUSystemTick.cpp:19-91` / `NiagaraGpuComputeDispatchInterface.h:31-42` / `NiagaraStackEntry.cpp:22-33` / `NiagaraHlslTranslator.h:105-150` / `NiagaraRendererSprites.h:41-58` |
| P-9 | §12.1 | Phase 표 1 곳에 한정. Fx Tick = phase 5 단독. FxRenderSnapshot = phase 6 단독 |
| P-10 | §6.5 | Owner Scope 매트릭스. FxSystemInstance = `CWorld` owned. FxAssetRegistry = `CGameInstance` Tier-1 |
| P-11 | §1 + §2.3 + §11 | 도메인 상수 0 (Engine 공용). LoL/Elden InitDesc 주입 (`FxSystemInitDesc`). `Client/Public/FX/Domains/{LoL,Elden}/` 위치 분리 (5/7 codex 정정 후 §0.1.4 / §1 표와 정합) |
| P-12 | §1 + §6.2 | float 좌표 → 정수 변환 시 `std::floor` 강제. SoA 레이아웃 무관 |
| P-13 | §11.3 | Hot reload 8 단계 의 모든 메서드 = §5/§6 헤더에 박제. grep 검증 의무 |
| P-14 | §13 EFX-0 | manifest hook 변환 = 행동 동일성 검증 (InGame screenshot 비교) |
| P-15 | §6.4 | `FxInstanceComponent.h` 는 POD handle component 로 유지. `unique_ptr<CFxSystemInstance>` 금지, instance 객체는 World-owned storage 가 소유 |
| P-16 | §3.3 + §7.1 | `eFxOp` enum = 27 값. `static_assert(static_cast<u32_t>(eFxOp::OUTPUT) == 26)` 부속 21 |
| P-17 | §1 | EntityID alias = 기존 `ECS/Entity.h` 재사용. typedef 일괄 변경 0 |
| P-18 | §1 + §12.4 | RHI 인프라 = 기존 `IRHIDevice / IRHIBindGroup / IRHIPipelineState` 재사용. 신규 `FxRHIHandle` 박제 X |
| P-19 | §6.2 + §9.2 | Render / Sim 분리. `FxRenderSnapshot` = read-only. Sim Tick = phase 5, Render = phase 7 |

---

## §15 부속 박제 인덱스

본 17 마스터의 후속. 사용자 검토 후 박제 진입.

| 부속 | 명칭 | 박제 본문 |
|---|---|---|
| 18 | Asset Layer (`.wfx` JSON 로더 / 세이버) | `CFxSystemAsset / CFxEmitterAsset / CFxScriptAsset` cpp 본체 + JSON schema + manifest 자산 dump 명령 |
| 19 | Runtime Layer (SoA + Parameter store + Tick) | `CFxSystemInstance / CFxEmitterInstance / CFxDataSet / CFxParameterStore` cpp + `CFxTickSystem` ECS phase 5 |
| 20 | Renderer 6 종 (Sprite/Mesh/Ribbon/Beam/Light/Decal) | 6 renderer 의 cpp + 6 셰이더 (HLSL) + RH-7 IRHI 통과 검증 |
| 21 | VM + GPU compute | `CFxVM` cpp (12 opcode AVX2/SSE2 본체) + `CFxGpuComputeDispatch` cpp + GPU compute 셰이더 + DX11 fallback |
| 22 | Compile (Graph → HLSL/VM) | `CFxGraph / CFxNode 7 종 / CFxHlslTranslator / CFxVMTranslator / FxParameterMapHistory / FxCodeChunk` cpp |
| 23 | DataInterface 6 종 | `CFxDICurve / CFxDITexture / CFxDIStaticMesh / CFxDISpline / CFxDIGrid2D / CFxDICollisionQuery` cpp + GPU template `.hlsl` |
| 24 | Editor 7 패널 (ImGui) | `CFxStackPanel / CFxGraphPanel / CFxCurveEditor / CFxPreviewViewport / CFxParameterPanel / CFxScratchPad / CFxToolbar` cpp |
| 25 | LoL / Elden 도메인 분리 | `Winters::LoL::FxBudget / FxBootstrap` + `Winters::Elden::FxBudget / FxBootstrap` + manifest hookId 매핑 |
| 26 | Hot Reload + Cooked binary `.wfxbin` | `CFxScriptAsset::RequestRecompile` async pipeline + `.wfxbin` 포맷 + Editor cook 명령 + Client loader |
| 27 | AAA VFX 인사이트 + Master Material 3 종 (5/7 신규) | `M_VFX_Particle_Generic / M_VFX_Trail / M_VFX_Volumetric` HLSL 본문 + 13 HLSL 트릭 + 5 라이팅 모델 + 4 핵심 노브 + 그레이스케일 텍스처 = 데이터 인사이트 |
| 28 | DX12 이주 시 FX 통합 (5/7 신규) | RHI 백엔드 DX12 우선 + GPU compute path 활성 + D3D12MA 통합 (Track 2 W7-9) + ImGui DX12 backend + 부속별 영향 매트릭스 |

5/7 codex 추가 정정 (사용자 AAA VFX 통찰 + DX12 이주 반영):

```txt
- 17 §1 사전 결정 표의 "RHI 백엔드 = RH-7 추상 통과" 결정값 → 부속 28 에서 "DX12 우선 + DX11 maintenance only" 갱신
- 17 §2.1 Niagara 매핑 의 "Renderer 6 종 (Sprite/Mesh/Ribbon/Beam/Light/Decal)" → 부속 27 에서 "Master Material 3 종 (Particle / Trail / Volumetric) 우선, 디자이너 노드 그래프는 후순위" 정정
- 17 §13 Stage EFX-3 의 "Renderer 6 종 InGame 출력" 합격 기준 → 부속 27 에서 "M_VFX_Particle_Generic 머티리얼 인스턴스 30~50 파라미터 노브 + 4 핵심 노브 (UV pan / contrast / color-over-life / HDR emission) 디자이너 변주 검증"
- 17 §13 Stage EFX-4 의 "Editor MVP 7 패널" → 부속 27 후 진입. 노드 그래프 에디터 (CFxGraphPanel) = 후순위. 디자이너 1차 워크플로우 = 머티리얼 인스턴스 파라미터 슬라이더만
- 17 §13 Stage EFX-5 의 "Compile (Graph → VM/HLSL)" → 부속 22 의 노드 그래프 컴파일 = 2차 트랙. 1차 트랙 = master material 직접 HLSL (부속 27)
```

---

## §16 검증 명령

본 마스터 박제 후 다음 검증:

```txt
1. grep "CDX11Device" Engine/Public/FX/v2/  → 0 hit (EFX-0 scaffold 후)
2. grep "ID3D11" Engine/Public/FX/v2/       → 0 hit
3. grep "ID3D12" Engine/Public/FX/v2/       → 0 hit
4. grep "Scene_" Engine/Public/FX/v2/       → 0 hit (관문 D, P-4)
5. grep "static constexpr.*MAX_PARTICLES\|static constexpr.*LoL\|static constexpr.*Elden" Engine/Public/FX/v2/  → 0 (도메인 상수 금지)
6. grep "TBD:" .md/plan/EffectTool/17_NIAGARA_FULL_REWRITE_MASTER.md  → 0 (관문 A, P-1. "TBD 0" 문구 자체는 허용)
7. EFX-0 manifest 자산 dump → canonical round-trip semantic equality (EFX-1 합격)
8. 11 챔프 우선 hook manifest → SpawnFromAsset 통과 + 기존 direct spawn callsite 분류표 작성 (EFX-0 합격)
9. InGame Sprite Renderer 1 emitter 1024 입자 출력 (EFX-3 합격)
10. Editor 신규 .wfx 박제 + 라이브 프리뷰 (EFX-4 합격)
```

---

## §17 변경 이력

```txt
2026-04-21    Phase G 초안 (.md/plan/EffectTool/01-10 11 파일, 6295 줄)
2026-04-26    Phase FX v4 (eFxBlendMode 시도, codex 회피로 BlendStateCache 차용)
2026-05-04    Codex 6 결함 정정 + Niagara V2 (12_EFFECT_TOOL_NIAGARA_V2_MASTER.md)
2026-05-05    V3 마스터 (13) + Niagara 깊이 맵 (14) + Lifecycle (15) + 진행 액션 (16)
2026-05-07    본 17 = v4 마스터. 13 deprecated, replaced-by 17. 18~26 부속 박제 대기
```

후속:
- 사용자 검토 후 부속 18 (Asset Layer cpp 본체) 부터 진입 권고
- EFX-0 (Legacy Bridge) 가 16 의 다음 액션을 코드베이스 실측 기준으로 정정 계승. 첫 작업은 11 챔프 우선 hook manifest 작성 + direct spawn callsite 분류
- Track 2 W7-9 합격 후 EFX-7 (GPU compute) 진입
- WintersEditor.exe 신규 vcxproj 박제 (EFX-4 진입 직전)
