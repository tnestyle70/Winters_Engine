# Winters Engine API Modernization — Master Plan

> **작성일**: 2026-05-02
> **목적**: 언리얼 엔진 5 오픈소스 아키텍처를 기준으로 Winters Engine 의 API 를 완전 분리·현대화
> **범위**: 12개 서브시스템 전면 재설계 (Module / Object / Actor-Component / World / Rendering / Network / FX / Asset / Editor / Gameplay / Animation / Build)
> **전제**: 현재 DX11 단일 백엔드, ECS 초기 단계, Scene_InGame 3000줄 모놀리식 → UE5 급 모듈화

---

## 1. Why — 현재 Winters vs UE5 Gap 분석

### 1.1 현재 문제점 (코드베이스 진단)

| 문제 | 현재 상태 | UE5 대응 | 심각도 |
|------|----------|---------|--------|
| **Scene_InGame 모놀리식** | 3000줄, 렌더/입력/네트워크/UI 전부 한 클래스 | GameMode + PlayerController + Subsystem 분리 | ★★★ |
| **CGameInstance 단일 창구 과부하** | 8개 DX11 leak getter + 모든 매니저 포워딩 | Engine/Module/Subsystem 자동 등록 | ★★★ |
| **컴포넌트 = POD 구조체** | 행동(로직) 없음, 시스템만 로직 보유 | UActorComponent (데이터+로직+틱) | ★★ |
| **수동 렌더 호출** | `m_Irelia.Render()` 나열 | SceneProxy + MeshDrawCommand 자동 수집 | ★★★ |
| **셰이더 하드코딩** | 6개 HLSL 고정, 재질 시스템 0% | Material Graph + USF 모듈화 | ★★★ |
| **네트워크 = FlatBuffers 수동 직렬화** | 모든 필드 수동 pack/unpack | UPROPERTY(Replicated) 자동 직렬화 | ★★ |
| **이펙트 = 수동 빌보드/메시** | FxBillboard + FxMesh 하드코딩 | Niagara 모듈 그래프 + GPU 파티클 | ★★★ |
| **에디터 = ImGui 즉석 코딩** | 패널마다 ImGui:: 직접 호출 | Details Panel + Content Browser 프레임워크 | ★★ |
| **애니메이션 = PlayAnimationByName** | 이름 문자열 매칭, 상태 머신 없음 | AnimBP + StateMachine + Montage | ★★ |
| **모듈 시스템 없음** | DLL 1개, 컴파일 단위 분리 없음 | 100+ 모듈 동적 로드/언로드 | ★★ |
| **리플렉션/직렬화 없음** | 수동 타입 정보, 에디터 노출 수동 | UClass/UProperty 자동 리플렉션 | ★★★ |
| **에셋 로딩 동기 전용** | LoadModel 블로킹 호출 | Async StreamableManager + SoftRef | ★★ |

### 1.2 목표 아키텍처 (UE5 기반 재해석)

```
┌─────────────────────────────────────────────────────────────────────┐
│  WintersEditor.exe (에디터 모드)  /  WintersLOL.exe (게임 모드)      │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │                    Game Module Layer                          │  │
│  │  ┌─────────┐ ┌──────────┐ ┌───────────┐ ┌────────────────┐  │  │
│  │  │GameMode │ │PlayerCtrl│ │ Champion  │ │  Skill/Buff    │  │  │
│  │  │GameState│ │Pawn/Char │ │ Component │ │  Component     │  │  │
│  │  └─────────┘ └──────────┘ └───────────┘ └────────────────┘  │  │
│  └───────────────────────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │                  Engine Framework Layer                       │  │
│  │  ┌────────┐ ┌──────────┐ ┌───────────┐ ┌──────────────────┐ │  │
│  │  │WObject │ │WActor    │ │WComponent │ │WWorld/WLevel     │ │  │
│  │  │WCLASS  │ │WPawn     │ │WSceneCmp  │ │WSubsystem       │ │  │
│  │  │WPROP   │ │WCharacter│ │WMeshCmp   │ │WGameInstance    │ │  │
│  │  └────────┘ └──────────┘ └───────────┘ └──────────────────┘ │  │
│  └───────────────────────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │                  Engine Core Layer                            │  │
│  │  ┌────────┐ ┌──────────┐ ┌───────────┐ ┌──────────────────┐ │  │
│  │  │Module  │ │RHI       │ │Renderer   │ │Network           │ │  │
│  │  │System  │ │(DX11/12) │ │(RenderGrph│ │(NetDriver/Repl)  │ │  │
│  │  ├────────┤ ├──────────┤ │ MeshDraw) │ ├──────────────────┤ │  │
│  │  │Asset   │ │Niagara   │ ├───────────┤ │Animation         │ │  │
│  │  │Manager │ │FX System │ │Material   │ │(AnimBP/Montage)  │ │  │
│  │  ├────────┤ ├──────────┤ │Graph      │ ├──────────────────┤ │  │
│  │  │JobSys  │ │Physics   │ ├───────────┤ │Editor Framework  │ │  │
│  │  │TaskGrph│ │(PhysX)   │ │Audio      │ │(Panel/Details)   │ │  │
│  │  └────────┘ └──────────┘ └───────────┘ └──────────────────┘ │  │
│  └───────────────────────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │                    Platform Layer                             │  │
│  │  Win32 / D3D11 / D3D12 / Vulkan / FMOD / WinHTTP            │  │
│  └───────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 2. Sub-Plan Index

| # | 파일 | 제목 | UE5 대응 | 핵심 산출물 |
|---|------|------|---------|------------|
| 01 | [01_MODULE_SYSTEM.md](01_MODULE_SYSTEM.md) | 모듈 시스템 | FModuleManager / IModuleInterface | IWintersModule, CModuleManager, IMPLEMENT_MODULE 매크로 |
| 02 | [02_OBJECT_MODEL.md](02_OBJECT_MODEL.md) | 오브젝트 모델 & 리플렉션 | UObject / UClass / UPROPERTY | WObject, WClass, WPROPERTY, 자동 직렬화/에디터 노출 |
| 03 | [03_ACTOR_COMPONENT.md](03_ACTOR_COMPONENT.md) | 액터-컴포넌트 모델 | AActor / UActorComponent / USceneComponent | WActor, WActorComponent, WSceneComponent, WMeshComponent |
| 04 | [04_WORLD_SUBSYSTEM.md](04_WORLD_SUBSYSTEM.md) | 월드 & 서브시스템 | UWorld / UWorldSubsystem / UGameInstance | WWorld, WLevel, WWorldSubsystem, WGameInstance |
| 05 | [05_RENDERING_PIPELINE.md](05_RENDERING_PIPELINE.md) | 렌더링 파이프라인 | RDG / FMeshDrawCommand / FMaterial | CRenderGraph, CMeshDrawCommand, CMaterialInstance |
| 06 | [06_NETWORK_REPLICATION.md](06_NETWORK_REPLICATION.md) | 네트워크 리플리케이션 | NetDriver / FRepLayout / RPC | CNetDriver, WREPLICATED, WRPC 매크로, 자동 직렬화 |
| 07 | [07_NIAGARA_FX.md](07_NIAGARA_FX.md) | Niagara 이펙트 시스템 | UNiagaraSystem / UNiagaraEmitter | CNiagaraSystem, CParticleEmitter, GPU Compute |
| 08 | [08_ASSET_MANAGEMENT.md](08_ASSET_MANAGEMENT.md) | 에셋 매니지먼트 | FAssetRegistry / FStreamableManager | CAssetRegistry, TSoftObjectPtr, 비동기 로딩 |
| 09 | [09_EDITOR_FRAMEWORK.md](09_EDITOR_FRAMEWORK.md) | 에디터 프레임워크 | SDetailsView / SContentBrowser | CDetailsPanel, CContentBrowser, CPropertyEditor |
| 10 | [10_GAMEPLAY_FRAMEWORK.md](10_GAMEPLAY_FRAMEWORK.md) | 게임플레이 프레임워크 | AGameMode / APlayerController / APawn | WGameMode, WPlayerController, WPawn, WCharacter |
| 11 | [11_ANIMATION_SYSTEM.md](11_ANIMATION_SYSTEM.md) | 애니메이션 시스템 | UAnimInstance / UAnimMontage / FAnimNode | CAnimInstance, CAnimStateMachine, CAnimMontage |
| 12 | [12_BUILD_PIPELINE.md](12_BUILD_PIPELINE.md) | 빌드 & 플러그인 파이프라인 | UBT / UHT / Plugin | 모듈 빌드 자동화, 코드 생성, 플러그인 시스템 |

---

## 3. 의존 그래프

```
01 Module System ◄── Foundation for all
    ↓
02 Object Model ◄── Reflection needed by everything
    ↓
    ├── 03 Actor-Component ◄── Uses WObject, WPROPERTY
    │       ↓
    │   04 World/Subsystem ◄── Manages Actors
    │       ↓
    │   10 Gameplay Framework ◄── GameMode/PlayerController extends Actor
    │
    ├── 05 Rendering Pipeline ◄── Material uses reflection for properties
    │       ↓
    │   07 Niagara FX ◄── Uses RenderGraph for GPU particles
    │
    ├── 06 Network Replication ◄── Serializes reflected properties
    │
    ├── 08 Asset Management ◄── Loads reflected types
    │       ↓
    │   09 Editor Framework ◄── Edits reflected properties
    │
    └── 11 Animation System ◄── AnimBP uses reflection

12 Build Pipeline ◄── Generates reflection code (parallel with 02)
```

---

## 4. 구현 우선순위 (Phase 분리)

### Phase A (즉시, 4-6주) — 기반

| 순서 | 서브시스템 | 이유 |
|------|----------|------|
| A-1 | 01 Module System | 나머지 전부의 전제 조건 |
| A-2 | 02 Object Model | 리플렉션 없으면 에디터/네트워크/직렬화 수동 |
| A-3 | 12 Build Pipeline | 코드 생성기 (UHT 대응) — Object Model 과 병행 |

### Phase B (4-6주) — 프레임워크

| 순서 | 서브시스템 | 이유 |
|------|----------|------|
| B-1 | 03 Actor-Component | 게임 오브젝트 구조의 핵심 |
| B-2 | 04 World/Subsystem | Scene_InGame 분해의 전제 |
| B-3 | 10 Gameplay Framework | GameMode/Controller 도입 → Scene 200줄 |

### Phase C (4-6주) — 렌더링 & 에셋

| 순서 | 서브시스템 | 이유 |
|------|----------|------|
| C-1 | 05 Rendering Pipeline | RenderGraph + Material + MeshDrawCommand |
| C-2 | 08 Asset Management | 비동기 로딩 + 스트리밍 |
| C-3 | 11 Animation System | AnimBP + Montage |

### Phase D (4-6주) — 도구 & 네트워크

| 순서 | 서브시스템 | 이유 |
|------|----------|------|
| D-1 | 06 Network Replication | 리플렉션 기반 자동 직렬화 |
| D-2 | 07 Niagara FX | GPU 파티클 시스템 |
| D-3 | 09 Editor Framework | Details Panel + Content Browser |

---

## 5. 네이밍 컨벤션 (UE5 매핑)

| UE5 | Winters 대응 | 접두사 | 예시 |
|-----|-------------|--------|------|
| `UObject` | `WObject` | `W` | `WObject`, `WActor`, `WComponent` |
| `AActor` | `WActor` | `W` | `WPawn`, `WCharacter` |
| `UActorComponent` | `WActorComponent` | `W` | `WSceneComponent`, `WMeshComponent` |
| `FStruct` | POD struct | 없음 | `TransformData`, `MeshDrawCommand` |
| `UCLASS()` | `WCLASS()` | — | `WCLASS(Blueprintable)` |
| `UPROPERTY()` | `WPROPERTY()` | — | `WPROPERTY(Replicated, EditAnywhere)` |
| `UFUNCTION()` | `WFUNCTION()` | — | `WFUNCTION(Server, Reliable)` |
| `TObjectPtr<>` | `TWObjectPtr<>` | — | `TWObjectPtr<WMeshComponent>` |
| `IModuleInterface` | `IWintersModule` | `I` | — |
| `FModuleManager` | `CModuleManager` | `C` | — |

> **`W` 접두사 선택 이유**: Winters 의 `W`, UE 의 `U`/`A`/`F` 3접두사를 `W` 하나로 통일. 기존 `C` 접두사는 엔진 내부 구현 클래스에 유지.

---

## 6. DLL 경계 원칙

```
WintersEngine.dll (Engine Core + Framework)
    exports: WObject, WActor, WComponent, WWorld, WGameInstance
    exports: CModuleManager, CAssetRegistry, CRenderGraph
    exports: IWintersModule interface

WintersRuntime.dll (Gameplay Framework)  [Phase B 신설]
    exports: WGameMode, WPlayerController, WPawn, WCharacter
    depends: WintersEngine.dll

WintersEditor.dll (Editor)  [Phase D 신설]
    exports: CDetailsPanel, CContentBrowser, CPropertyEditor
    depends: WintersEngine.dll + WintersRuntime.dll

GameModule.dll (Game-Specific)  [기존 Client 리팩터]
    exports: AGameMode_LOL, AChampionCharacter
    depends: WintersEngine.dll + WintersRuntime.dll
```

---

## 7. 기존 코드와의 공존 전략

**원칙: Big-Bang 리팩터 금지. 레이어별 점진 교체.**

1. **새 API 옆에 공존**: 기존 `CGameInstance::Get()->Method()` 유지하면서 새 `WWorld::GetSubsystem<T>()` 병행
2. **Adapter 패턴**: 기존 `ModelRenderer` → `WStaticMeshComponent` 내부에서 `ModelRenderer` 래핑
3. **Shim 유지**: `CTransform` → `WSceneComponent::GetRelativeTransform()` shim 레이어
4. **Feature Flag**: `#define WINTERS_USE_NEW_FRAMEWORK 1` 로 점진 전환
5. **테스트 기반 이주**: 각 Phase 합격 게이트 통과 후 다음 Phase 진입

---

## 8. UE5 vs Winters 최적화 비교 목표

| 영역 | UE5 | Winters 목표 | 차별화 |
|------|-----|-------------|--------|
| DrawCall Batching | MeshDrawCommand + Indirect | MeshDrawCommand + Multi-Draw Indirect | MOBA 특화 LOD (카메라 고정) |
| GPU Particles | Niagara GPU Sim | Compute Shader Particle + SoA | LoL FX 최적화 (빌보드 특화) |
| Network Bandwidth | Property Replication + Delta | Snapshot + Delta + Huffman | MOBA 10인 고빈도 최적화 |
| Asset Loading | Async + Streaming Levels | Async + Champion Prefetch Pool | 챔프 선택 시 사전 로딩 |
| Animation | AnimBP + Montage + IK | AnimBP + Montage + Root Motion | MOBA 특화 (스킬 락 타이밍) |
| Memory | Pool Allocator + Slab | Pool + SoA Component Store | ECS 캐시 친화 유지 |

---

## 9. Client API 사용 예시 (Before → After)

### Before (현재)

```cpp
// Scene_InGame.cpp — 3000줄 중 일부
void CScene_InGame::OnEnter()
{
    // 수동 모델 로드
    m_pIreliaModel = CModel::Create(L"Irelia/irelia.wmesh");
    m_pIreliaRenderer = CModelRenderer::Create(pDevice, pShader, pPipeline, m_pIreliaModel);
    m_pIreliaRenderer->LoadMeshTexture(0, L"Irelia/irelia_base.png");
    m_IreliaTransform.SetPosition({0, 0, 0});
    m_IreliaTransform.SetScale({0.01f, 0.01f, 0.01f});

    // ECS 엔티티 수동 생성
    auto eIrelia = m_world.CreateEntity();
    m_world.AddComponent<TransformComponent>(eIrelia);
    m_world.AddComponent<HealthComponent>(eIrelia, {1500.f, 1500.f});
    m_world.AddComponent<ChampionComponent>(eIrelia, {eChampionId::Irelia, eTeam::Blue});
    // ... 50줄 더
}

void CScene_InGame::OnUpdate(f32_t dt)
{
    // 수동 동기화
    SyncECSTransformsFromLegacy();
    UpdateTargeting();
    UpdateCombatInput();
    // ... 500줄 더
}

void CScene_InGame::OnRender()
{
    m_pIreliaRenderer->Render(m_IreliaTransform.GetWorldMatrix());
    m_pYasuoRenderer->Render(m_YasuoTransform.GetWorldMatrix());
    // ... 챔프마다 수동 나열
}
```

### After (목표)

```cpp
// GameMode_LOL.cpp — 50줄
void AGameMode_LOL::OnMatchStart()
{
    // 챔프 스폰 = 1줄
    auto* irelia = GetWorld()->SpawnActor<AChampionCharacter>(
        ChampionTable::Irelia, FVector{0, 0, 0}, ETeam::Blue);
}

// AChampionCharacter.cpp — 컴포넌트 조립
AChampionCharacter::AChampionCharacter()
{
    // 컴포넌트 자동 구성
    MeshComp = CreateDefaultSubobject<WSkeletalMeshComponent>("Mesh");
    SetRootComponent(MeshComp);

    HealthComp = CreateDefaultSubobject<WHealthComponent>("Health");
    SkillComp = CreateDefaultSubobject<WSkillComponent>("Skills");

    // 네트워크 자동 리플리케이션
    bReplicates = true;
}

// 렌더 = 자동 (SceneProxy가 MeshDrawCommand 수집)
// 네트워크 = 자동 (WPROPERTY(Replicated) 필드 자동 직렬화)
// 에디터 = 자동 (WPROPERTY(EditAnywhere) 필드 ImGui 자동 노출)
```

---

## 10. 검증 기준 (전체)

| Phase | 합격 게이트 |
|-------|-----------|
| A 완료 | Module 동적 로드 + WObject 리플렉션으로 에디터 프로퍼티 자동 노출 |
| B 완료 | WActor+WComponent 로 이렐리아 스폰, Scene_InGame 200줄 이하 |
| C 완료 | RenderGraph 기반 Forward+, Material Instance 실시간 튜닝 |
| D 완료 | WREPLICATED 로 2-client 동기화, Niagara 로 이렐리아 Q 이펙트 |
