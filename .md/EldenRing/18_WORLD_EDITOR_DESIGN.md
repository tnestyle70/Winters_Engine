# World Editor 상세 설계 (UE5 Editor급)

> 작성: 2026-06-23. 시스템 식별자: `WORLD_EDITOR`.
> 선행 문서: `17_UE5_GRADE_EDITOR_SUITE_MASTER.md`(5대 시스템 매핑·순서·게이트), `12_UE5_REFERENCE_DX12_RHI_EDITOR_BIG_PICTURE.md`(Phase A~J·게이트 G0~G9),
> `plan/EldenRingEditor/01_DX12_IMGUI_EDITOR_BOOTSTRAP.md`, `03_EDITOR_ASSET_CATALOG_MAP_ASSEMBLY.md`, `06_ELDENRING_PBR_MATERIAL_RESOLVER.md`,
> `21_WORLD_PARTITION_DESIGN.md`.
> 이 문서는 World Editor 한 시스템만 깊게 다룬다. FX/Sequencer/World Partition/Boss는 형제 문서로 분리한다.

---

## 0. 한 줄 목표 + 시스템 경계

**한 줄 목표**: EldenRingEditor(DX12 ImGui 셸) 위에 **command 기반 World Editor**를 올려, Content Browser에서 선택한 `.wmesh`를 뷰포트에 프리뷰하고, gizmo로 배치/변형하고, 모든 편집을 Undo/Redo 가능한 command로 표현해 `.wcell` JSON으로 저장하고, **같은 데이터를 런타임(EldenRingClient showcase scene)이 그대로 로드**하게 만든다.

**시스템 경계 (이 문서가 다루는 것)**:
- 에디터 셸 패널: Viewport, Content Browser, World Outliner, Details(Inspector), Log/Transaction Stack.
- 편집 코어: `CEditorTransaction`(command stack + redo), ray-pick selection, Transform Gizmo, drag-drop placement.
- 런타임 contract: `.wcell` JSON 포맷(cell descriptor + placement 배열), `CWorldCellDocument`(로드/저장), placement → `ModelRenderer` 프리뷰.

**경계 밖 (다른 시스템 문서가 다룸)**:
- cell streaming state machine / streaming source / data layer / HLOD → `21_WORLD_PARTITION_DESIGN.md` (`WORLD_PARTITION` 시스템). 본 문서는 cell **편집/저장 contract**까지만 책임지고, 런타임 streaming은 그 contract를 소비한다.
- material 채널 resolve → `06` (PBR Material Resolver). 본 문서는 placement가 참조하는 `.wmesh` path만 다루고 material 자동 매칭은 위임한다.
- FX/Sequencer/Boss → 형제 문서.
- gameplay 판정(collision/nav/spawn authority) → Server GameSim. 에디터가 만드는 것은 **presentation/placement seed**일 뿐이다.

---

## 1. UE5 실제 아키텍처 (깊이)

UE5 Editor는 "런타임을 편집하는 별도 앱"이 아니라 **런타임 위에 editor-only 레이어를 얹은 같은 프로세스**다. 핵심 통찰은 *모든 편집이 transaction(command)으로 표현되고, editor와 game이 같은 `UWorld`/`AActor` 데이터를 공유*한다는 점이다.

### 1.1 Slate (UI 프레임워크)

- `SWidget` 트리 + `SCompoundWidget`. retained-mode가 아니라 매 프레임 `OnPaint`로 그리되, layout은 `SlatePrepass`로 캐싱. `SDockTab`/`FTabManager`가 도킹 레이아웃을 소유.
- **왜 그렇게 설계했나**: 에디터 패널을 독립 위젯으로 분리해 레이아웃을 사용자가 재구성하게 하고, 게임 UI(UMG)와 같은 렌더 백엔드를 쓰되 에디터 전용 위젯은 게임 빌드에서 컴파일 제외(`WITH_EDITOR`)하기 위해서다.

### 1.2 Editor Mode (`GLevelEditorModeTools`, `FEdMode`/`UEdMode`)

- 현재 활성 "모드"(Select / Landscape / Foliage / Mesh Paint …)가 입력·뷰포트 오버레이·gizmo 동작을 가로챈다. 기본은 `FEdModeDefault`(선택 모드).
- `UEdMode::Enter/Exit`, `InputKey`, `Render`, `HandleClick`. 모드가 ray-pick과 gizmo interaction을 해석.
- **왜**: 뷰포트 동작을 모드별로 교체 가능하게 해 단일 뷰포트가 배치/페인트/터레인을 모두 처리.

### 1.3 Transform Gizmo (`FWidget` 레거시 → `UInteractiveGizmoManager`/`UTransformGizmo`)

- translate/rotate/scale 핸들. 축(X/Y/Z), 평면, 화면공간. world/local space 토글, grid snap(`FEditorViewportClient::GetSnapSize`).
- 신형 `UGizmoElementBase` + `IGizmoStateTarget`. drag 시작에 `BeginTransformEdit`로 transaction 열고, drag 끝에 commit.
- **왜**: gizmo 자체가 transaction을 열고 닫아 모든 transform 편집이 자동으로 undo 가능.

### 1.4 Viewport (`FEditorViewportClient`, `SLevelViewport`)

- 퍼스펙티브/직교, 카메라(`FViewportCameraTransform`), grid/snap, hit-proxy 기반 picking(`HHitProxy` — 픽셀에 actor/component id 인코딩 후 GPU readback).
- **왜 hit-proxy**: 복잡한 mesh를 정확히 픽셀 단위로 픽하기 위해 별도 hit-proxy pass를 렌더해 GPU에서 id를 읽는다. (단순 씬에선 CPU ray-AABB로 충분.)

### 1.5 Details Panel (`SDetailsView`, `IDetailCustomization`)

- 선택 객체의 `UPROPERTY`를 reflection(`UClass`/`FProperty`)으로 순회해 자동 위젯 생성. `IDetailCustomization`으로 커스터마이즈.
- 값 변경 시 `FPropertyChangedEvent` 발행 + `PreEditChange/PostEditChange`로 transaction wrapping.
- **왜 reflection**: 타입별 인스펙터를 손으로 안 짜도 되게. (Winters는 reflection이 없으므로 **per-type ImGui 인스펙터**로 대체.)

### 1.6 World Outliner (`SSceneOutliner`)

- `UWorld`의 actor 계층(folder/attachment) 트리. 선택은 `USelection`(`GEditor->GetSelectedActors()`)이 단일 소스. Outliner/Viewport/Details가 같은 selection을 공유.
- **왜**: selection을 한 곳(`USelection`)에 두어 모든 패널이 동기화.

### 1.7 Content Browser + AssetRegistry (`IAssetRegistry`, `FAssetData`)

- `AssetRegistry`가 디스크의 `.uasset`을 스캔해 메타데이터(class, tags, dependencies)만 메모리에 캐시. 실제 로드는 lazy. drag-drop으로 actor 스폰.
- **왜 메타데이터 캐시**: 수만 에셋을 풀로드하지 않고 목록/검색/의존성만 빠르게.

### 1.8 Transaction system (`GEditor->BeginTransaction`, `FScopedTransaction`, `UTransBuffer`)

- 핵심. `FScopedTransaction tx(NSLOCTEXT(...))` 생성 → 변경 전 `Object->Modify()` 호출(객체 상태 snapshot) → 스코프 종료 시 commit.
- Undo/Redo는 `UTransactor`(`UTransBuffer`)가 snapshot diff를 역재생. `UObject::Serialize`를 통해 상태 복원.
- **왜 snapshot-기반**: UE는 거대한 reflective object를 다루므로 명령 객체를 일일이 만드는 대신 변경 전후 직렬화 스냅을 저장. (Winters는 객체가 작고 reflection이 없으므로 **명시적 command 객체**가 더 단순·명확.)

### 1.9 PIE (Play In Editor)

- editor world를 복제해 game world를 만들고 같은 프로세스에서 simulate. editor data가 곧 game data임을 보장.
- **왜**: "에디터에서 본 것 = 게임에서 도는 것" 불변식.

### 1.10 UE5가 World Editor를 이렇게 설계한 철학 요약

1. **단일 selection 소스 + 단일 world data** → 모든 패널이 같은 진실을 본다.
2. **모든 mutation = transaction** → undo/redo가 공짜로 따라온다(처음부터 transaction wrapping).
3. **editor는 runtime을 우회하지 않는다** → PIE로 editor data를 그대로 실행.
4. **AssetRegistry는 메타만, 로드는 lazy** → 대규모 에셋에서도 브라우저가 가볍다.

---

## 2. Winters 현재 구조 (실측 근거)

### 2.1 재사용 가능한 실제 코드

| 자산 | 파일:라인 | World Editor 활용 |
|---|---|---|
| 에디터 셸 진입 | `EldenRingEditor/Private/main.cpp:38` (`wWinMain` → `WintersRun`), `EldenRingEditor/Private/EldenRingEditorApp.cpp:13` (`OnInit` → `Change_Scene`) | 셸은 이미 빌드/실행됨. World Editor는 `CEldenRingEditorScene`을 채운다. |
| 빈 에디터 씬 | `EldenRingEditor/Public/EldenRingEditorScene.h:7`, `EldenRingEditor/Private/EldenRingEditorScene.cpp:31` (`OnImGui()` 빈 함수) | **여기에 도크스페이스+패널을 채워 넣는 것이 핵심 작업.** |
| Scene 인터페이스 | `Engine/Include/IScene.h:7` (`OnEnter/OnUpdate/OnRender/OnImGui`) | 에디터 씬도 이 5함수만 구현. |
| RHI device 접근 | `Engine/Include/GameInstance.h:137` (`Get_RHIDevice()`) | 뷰포트 렌더가 device 획득. |
| **mesh 렌더러** | `Engine/Public/Renderer/ModelRenderer.h:22` | `Initialize(wmeshPath)`, `UpdateTransform(Mat4)`, `UpdateCamera(viewProj, eye)`, `RenderFrustumCulled` — placement 프리뷰 즉시 재사용. |
| **picking 프리미티브** | `ModelRenderer.h:98-101` `HasValidAABB()`, `GetLocalAABBMin/Max()` (주석 "Local AABB - Picking Editor") | ray-AABB pick의 입력. **이미 의도적으로 노출됨.** |
| **hover highlight** | `ModelRenderer.h:89-90` `SetHoverOutline(color)`, `ClearHoverOutline()` | 선택/hover 시각 피드백 즉시 재사용. |
| material override | `ModelRenderer.h:87` `SetMaterialOverrideColor` | 선택 강조 fallback. |
| **편집 baseline (사실상 prototype World Editor)** | `EldenRingClient/Private/EldenLimgraveShowcaseScene.cpp` 전체 | drag-place 편집(`DragFloat3` Position/Rotation/Scale, `ApplyMapPlacementTransform` `:715`), JSON save/load(`SaveMapPlacementDraft` `:819`, `LoadMapPlacementDraft` `:874`), free camera(`UpdateFreeCamera` `:1068`), `SpawnInstance` `:983`, 선택 리스트박스+프레이밍. **단, undo/redo 없음 · command 없음 · ray-pick 없음 · gizmo 없음.** |
| content path 해석 | `Engine/Include/WintersPaths.h:12` `WintersResolveContentPath`, showcase의 `ResolveRepoRelativePath` `:95` | 에셋 스캔/로드 경로 해석. |
| WMesh 포맷 | `Engine/Public/AssetFormat/Mesh/WMeshFormat.h` (`MeshMetaHeader`, `SubMeshDesc`, `SubMeshBounds` AABB) | Content Browser 메타(서브메시 수/바운드) 추출 가능. |
| ImGui 셸 | `Engine/Public/Editor/ImGuiLayer.h:5` | BeginFrame/EndFrame로 패널 그림. |

### 2.2 미구현 (World Editor 신규)

| 미구현 | 현재 상태 (근거) | 비고 |
|---|---|---|
| **DX12 ImGui dockspace** | `ImGuiLayer.h`는 아직 DX11 전용 baseline. 01 plan의 DX12 backend 분기 **미적용**(현 헤더에 `eRHIBackend m_eBackend`/`DX12BackendState` 없음). | **선행 게이트 G3.** 01 plan을 먼저 반영해야 에디터가 DX12에서 ImGui를 띄운다. |
| `CEditorTransaction` (command stack + redo) | 코드 전무. `grep CEditorTransaction` → `.md` 문서만 히트. | World Editor 핵심 신규. |
| ray-pick selection | `ModelRenderer`가 AABB는 노출하나 picking 로직(ray 생성/교차/최근접) 없음. | 신규. |
| Transform Gizmo | 없음. ImGuizmo 미통합. | 신규(자체 또는 ImGuizmo). |
| `.wcell` 포맷 / `CWorldCellDocument` | `grep .wcell` → 문서만. showcase는 ad-hoc `map_placement.txt`/`*_draft.txt` 파이프 텍스트. | 신규 — JSON cell contract 정식화. |
| `CAssetCatalog` (Engine 공용) | Engine에 `CAssetCatalog` 없음. showcase는 디렉터리 직접 순회. | 본 시스템은 editor-local catalog로 시작, Engine 승격은 후순위. |
| `CAssetStreamingSystem` | 미구현(17문서 명시). | World Partition 시스템 소관. 본 문서는 cell save까지, streaming 연동은 그 시스템에 위임. |

**결론**: 편집 UX·mesh 프리뷰·JSON I/O·free camera는 showcase에 이미 **검증된 형태로 존재**한다. World Editor의 진짜 신규 작업은 (a) **command 계층**으로 편집을 감싸 undo/redo 확보, (b) **ray-pick + gizmo**로 뷰포트 직접 조작, (c) **`.wcell` 정식 contract**다. "화면 먼저 크게"가 아니라 이 세 가지를 작게 증명하는 것이 본 설계의 척추다.

---

## 3. Winters 설계

### 3.1 포맷 스키마 (JSON 초기 → `.wcell` 승격)

초기 `cell.json`(diff 가능, 빠른 수정). 안정화 후 `.wcell` binary 승격(17문서 5절). placement는 showcase의 검증된 필드(tile/kind/name/model/wmesh/position/rotationDeg/scale/animated)를 **정규화**해 계승한다.

```jsonc
{
  "schema": "winters.world.cell.v1",
  "cellId": "m60_42_36_00",          // World Partition seed key (21문서)
  "area": 60, "blockX": 42, "blockY": 36, "variant": 0,
  "cellSizeMeters": 64.0,
  "origin": [0.0, 0.0, 0.0],          // cell 로컬 원점(월드 기준)
  "dataLayer": "Base",                // Base|Gameplay|RaidPhase|Cinematic|EditorOnly (21문서 위임)
  "placements": [
    {
      "id": 1001,                      // cell 내 안정적 placement id (undo/저장 키)
      "kind": "Asset",                 // MapPiece|Asset|Enemy|Npc (showcase kind 계승)
      "name": "AEG099_720_inst0",
      "wmesh": "Client/Bin/Resource/EldenRing/Assets/.../AEG099_720.wmesh",
      "position": [12.5, 0.0, -8.0],
      "rotationDeg": [0.0, 35.0, 0.0],
      "scale": [1.0, 1.0, 1.0],
      "animated": false,
      "transformResolved": true        // false면 reference-only(03문서: "MSB transform parser required")
    }
  ],
  "references": [                       // transform 없는 항목 — 배치하지 않고 목록만
    { "kind": "MapPiece", "model": "m60_..._part3", "reason": "MSB transform parser required" }
  ]
}
```

**불변식**:
- `transformResolved=false` 또는 `references[]` 항목은 **draw call에 들어가지 않는다**(12문서 Phase D/E, 03문서). Outliner에는 "reference only"로만 보인다.
- gameplay truth 필드 없음. collision/nav/spawn authority는 Server GameSim seed(별도)로 간다 — cell json은 **presentation placement seed**.
- `id`는 cell 내에서만 안정적이면 충분(undo target + 저장 round-trip key).

### 3.2 런타임 클래스 계층 (C++ 시그니처)

Winters 타입(`f32_t`/`u32_t`/`Vec3`/`Mat4`) 사용. 모든 타입은 **editor/EldenRingClient 측**(Client/Public 아님)에 두어 DX11/DX12 concrete type을 공개 경계에 노출하지 않는다. `IRHIDevice*`/`ModelRenderer`는 이미 Engine public이므로 그대로 사용.

```cpp
// ── 1) Cell document (런타임 contract: 로드/저장) ───────────────────────────
struct WorldPlacement
{
    u32_t       id          = 0;
    std::string kind;                 // "MapPiece"|"Asset"|"Enemy"|"Npc"
    std::string name;
    std::string wmesh;                // Resource 상대경로
    Vec3        position{0,0,0};
    Vec3        rotationDeg{0,0,0};
    Vec3        scale{1,1,1};
    bool        animated         = false;
    bool        transformResolved = true;
};

struct WorldReference                  // transform 없음 → 배치 금지, 목록만
{
    std::string kind;
    std::string model;
    std::string reason;
};

class CWorldCellDocument               // 순수 데이터 + JSON I/O (런타임/에디터 공용)
{
public:
    bool   Load(const std::string& strJsonRelative);   // WintersResolveContentPath 사용
    bool   Save(const std::string& strJsonRelative) const;
    const std::string& CellId() const { return m_cellId; }

    std::vector<WorldPlacement>& Placements() { return m_placements; }
    std::vector<WorldReference>& References() { return m_references; }

    WorldPlacement*  FindPlacement(u32_t id);
    u32_t            AllocPlacementId();                // 단조 증가
private:
    std::string m_cellId; u32_t m_area=0, m_blockX=0, m_blockY=0, m_variant=0;
    f32_t m_cellSizeMeters=64.f; Vec3 m_origin{0,0,0}; std::string m_dataLayer="Base";
    std::vector<WorldPlacement> m_placements;
    std::vector<WorldReference> m_references;
    u32_t m_nextId = 1;
};

// ── 2) Placement 프리뷰 (cell data → ModelRenderer 인스턴스) ─────────────────
class CWorldPreview                     // truth 아님: 화면 표현만
{
public:
    void   Rebuild(const CWorldCellDocument& doc);      // 전체 재생성(로드 직후)
    void   SyncPlacement(const WorldPlacement& p);      // 단건 transform/anim 반영
    void   AddPlacement(const WorldPlacement& p);
    void   RemovePlacement(u32_t id);
    void   Render(const Mat4& matViewProj, const Vec3& vEye);
    void   Update(f32_t dt);

    // ray-pick 지원: placement id별 world AABB 질의
    bool   GetWorldAABB(u32_t id, Vec3& outMin, Vec3& outMax) const;
    void   SetHover(u32_t id, bool on);                 // ModelRenderer::SetHoverOutline 위임
private:
    struct Inst { u32_t id; std::unique_ptr<ModelRenderer> renderer; };
    std::vector<Inst> m_instances;
};

// ── 3) Transaction (command stack + redo) — World Editor 핵심 ────────────────
class IEditorCommand
{
public:
    virtual ~IEditorCommand() = default;
    virtual void Do()   = 0;            // 적용(최초/redo)
    virtual void Undo() = 0;            // 역적용
    virtual const char* Name() const = 0;
};

class CEditorTransaction
{
public:
    void  Push(std::unique_ptr<IEditorCommand> cmd);   // Do() 호출 후 undo 스택에 적재, redo 스택 clear
    bool  CanUndo() const { return !m_undo.empty(); }
    bool  CanRedo() const { return !m_redo.empty(); }
    void  Undo();                       // undo.top().Undo() → redo로 이동
    void  Redo();                       // redo.top().Do()   → undo로 이동
    void  Clear();
    // gizmo drag: 시작~끝을 한 command로 묶음(중간 프레임은 미리보기만, commit 시 1개 push)
    void  BeginInteractive(std::unique_ptr<IEditorCommand> cmd);  // Do() 즉시, 스택엔 아직 미적재
    void  UpdateInteractive(const Mat4& matWorld);                // 미리보기 갱신
    void  EndInteractive();                                       // 최종값으로 undo 스택 push
    const std::vector<std::string>& History() const { return m_historyLabels; }
private:
    std::vector<std::unique_ptr<IEditorCommand>> m_undo, m_redo;
    std::unique_ptr<IEditorCommand>              m_interactive;
    std::vector<std::string>                     m_historyLabels;  // Transaction Stack 패널 표시용
};

// ── 4) 구체 command (cell doc + preview를 함께 mutate) ───────────────────────
class CTransformPlacementCommand : public IEditorCommand   // 이동/회전/스케일
{
public:
    CTransformPlacementCommand(CWorldCellDocument*, CWorldPreview*,
                               u32_t id, const Vec3& posOld, const Vec3& rotOld, const Vec3& sclOld,
                                         const Vec3& posNew, const Vec3& rotNew, const Vec3& sclNew);
    void Do() override; void Undo() override; const char* Name() const override { return "Transform"; }
    void SetNew(const Vec3& p, const Vec3& r, const Vec3& s);  // interactive 갱신용
private: /* doc*, preview*, id, old/new TRS */
};
class CAddPlacementCommand    : public IEditorCommand { /* drag-drop 배치 */ };
class CDeletePlacementCommand : public IEditorCommand { /* 삭제 */ };

// ── 5) Selection (UE USelection 대응: 단일 진실) ────────────────────────────
class CEditorSelection { public: u32_t Active() const; void Set(u32_t id); void Clear(); /* multi 후순위 */ };

// ── 6) Ray-pick (CPU ray vs world-AABB) ─────────────────────────────────────
struct EditorRay { Vec3 origin; Vec3 dir; };
class CEditorPicker
{
public:
    static EditorRay ScreenToWorldRay(f32_t mouseX, f32_t mouseY, u32_t vpW, u32_t vpH,
                                      const Mat4& view, const Mat4& proj, const Vec3& eye);
    static bool RayAABB(const EditorRay& r, const Vec3& aabbMin, const Vec3& aabbMax, f32_t& outT);
    u32_t PickNearest(const EditorRay& r, const CWorldPreview& preview);  // 0 = miss
};

// ── 7) Transform Gizmo (자체 최소 구현; ImGuizmo 통합은 옵션) ─────────────────
enum class eGizmoMode : u32_t { Translate, Rotate, Scale };
enum class eGizmoSpace: u32_t { World, Local };
class CTransformGizmo
{
public:
    void SetMode(eGizmoMode m); void SetSpace(eGizmoSpace s); void SetSnap(f32_t t, f32_t r, f32_t s);
    // 반환 true = 이번 프레임 drag 중. matInOut을 직접 갱신.
    bool Manipulate(const Mat4& view, const Mat4& proj, Mat4& matInOut, bool mouseDown, f32_t mx, f32_t my);
    bool IsDragging() const;
};

// ── 8) 에디터 씬 (CEldenRingEditorScene 확장 = IScene 구현) ───────────────────
//   OnImGui()에서 DockSpace + 패널, OnRender()에서 CWorldPreview::Render.
```

### 3.3 에디터 패널 (ImGui) 구성과 기능

`CEldenRingEditorScene::OnImGui()`가 `ImGui::DockSpaceOverViewport()`로 도크스페이스를 깔고 아래 패널을 띄운다(UE의 `FTabManager` 대응).

| 패널 | UE 대응 | 기능 | 데이터 소스 |
|---|---|---|---|
| **Viewport** | `SLevelViewport` | free camera(showcase `UpdateFreeCamera` 이식), gizmo 오버레이, 좌클릭 ray-pick, drag-drop 수신. ImGui child window + `OnRender`에서 RHI 씬 그림. | `CWorldPreview`, `CEditorPicker`, `CTransformGizmo` |
| **Content Browser** | `SContentBrowser`/AssetRegistry | resource root 스캔 → `.wmesh`/PNG/JSON 트리·테이블. 서브메시 수/AABB 메타(WMeshFormat). 항목을 뷰포트로 drag → `CAddPlacementCommand`. | editor-local catalog(디렉터리 순회 + WMeshFormat 헤더) |
| **World Outliner** | `SSceneOutliner` | cell → placement 트리. `references[]`는 "reference only" 회색 표시(배치 안 됨). 선택 = `CEditorSelection`(뷰포트·Details와 동기). | `CWorldCellDocument` |
| **Details (Inspector)** | `SDetailsView` | 선택 placement의 TRS `DragFloat3`(showcase UI 이식), kind/name/wmesh/animated. 값 변경 → `CTransformPlacementCommand`(interactive). reflection 없으므로 per-type ImGui. | 선택 placement |
| **Transaction Stack** | (UE는 Edit 메뉴 history) | undo/redo 히스토리 라벨 리스트, Undo/Redo 버튼(Ctrl+Z/Y). | `CEditorTransaction::History()` |
| **Log** | Output Log | 로드/저장/픽 실패 사유. | OutputDebugString 미러 |

상단 **메뉴/툴바**: Cell 열기/저장(`CWorldCellDocument::Load/Save`), Gizmo 모드(W/E/R = Translate/Rotate/Scale), Space(World/Local), Snap 토글, Undo/Redo.

---

## 4. 데이터 흐름 (presentation/truth 경계 명시)

```text
[에디터]                                              [런타임 / EldenRingClient]
Content Browser (스캔된 .wmesh)
   │ drag-drop
   ▼
CAddPlacementCommand ── Do() ─► CWorldCellDocument.placements[]  (편집 중 메모리 진실)
   │                                  │
   ▼                                  ▼
CEditorTransaction (undo/redo)   CWorldPreview (ModelRenderer 인스턴스 = 화면 표현)
   │ Save                             ▲
   ▼                                  │ Render(viewProj, eye)  ← OnRender
cell.json  ──────────────────────────┘  (.wcell 승격은 안정화 후)
   │
   │  *** 같은 파일을 런타임이 로드 ***
   ▼
CWorldCellDocument::Load  ──►  [WORLD_PARTITION] CAssetStreamingSystem (Required/Optional 묶음)
                                        │  (이 연동은 21문서 시스템 소관)
                                        ▼
                                 CWorldPreview / 런타임 RenderWorldSnapshot
```

**presentation/truth 경계**:
- **presentation (이 시스템 책임)**: placement transform, 어떤 `.wmesh`를 어디에 그릴지, 카메라, 선택/하이라이트, undo/redo. 전부 client/editor 로컬. 네트워크/판정에 직접 영향 없음.
- **truth (이 시스템 책임 아님)**: 충돌/네비/스폰 권위·hitbox·damage·phase는 **Server GameSim**. cell json의 placement는 서버가 collision/nav cell을 굽기 위한 **seed 후보**일 뿐, 에디터가 판정을 만들지 않는다(17문서 원칙 3, 12문서 Phase E/I).
- **CAssetStreamingSystem 경유 불변식**: 에디터가 만든 cell.json은 런타임에서 streaming system을 거쳐 로드된다. 에디터 전용 우회 로드 경로를 만들지 않는다(현 showcase의 직접 `ModelRenderer::Initialize`는 prototype이며, 정식 경로는 streaming 연동 시 그쪽으로 합류).

---

## 5. 구현 순서 (단계별 + 완료기준 + 게이트)

전제(17문서 4절): RHI G2(텍스처+라이트+스태틱메시 표시)와 **에디터 셸 DX12 dockspace G3**가 선행. **G3 전에는 패널을 크게 벌리지 않는다.**

| 단계 | 내용 | 완료기준 | 게이트 |
|---|---|---|---|
| **S0** | 선행 확인: `ImGuiLayer` DX12 분기(01 plan) 반영 여부 점검. 미반영이면 01 먼저. `CEldenRingEditorScene::OnImGui`에 `DockSpaceOverViewport` + 빈 패널 4개(Viewport/Content/Outliner/Details) 등록. | EldenRingEditor(`--rhi=dx12`)에서 도크스페이스 + 빈 패널 4개가 보인다. | **G3 Editor shell** |
| **S1** | `CWorldCellDocument`(JSON Load/Save) + `CWorldPreview`(placement→ModelRenderer). 손으로 만든 `cell.json` 1개 로드 → 뷰포트에 `.wmesh` 1개 표시. free camera 이식(showcase `UpdateFreeCamera`). | cell.json seed → 뷰포트 mesh 프리뷰 + 카메라 이동. **(런타임 contract 먼저 증명)** | G4 Asset catalog(부분) |
| **S2** | Content Browser: resource root 스캔, `.wmesh` 트리/테이블 + WMeshFormat 메타. 선택 항목 뷰포트로 drag-drop → `CAddPlacementCommand`. | 브라우저에서 `.wmesh` drag → 뷰포트에 인스턴스 생성 + Outliner에 추가. | G4 |
| **S3** | `CEditorTransaction` + `CAddPlacementCommand`/`CDeletePlacementCommand`. Undo/Redo 버튼·단축키, Transaction Stack 패널. | 배치/삭제가 Ctrl+Z/Ctrl+Y로 정확히 되돌려지고 다시 적용됨. | (시스템 게이트: Command 무결성) |
| **S4** | ray-pick(`CEditorPicker`, ray-AABB) + `CEditorSelection`. 좌클릭 선택, hover/select 하이라이트(`ModelRenderer::SetHoverOutline`). Outliner↔Viewport↔Details selection 동기. | 뷰포트 클릭으로 정확히 선택, 세 패널 selection 일치. | (선택 동기) |
| **S5** | `CTransformGizmo`(자체 또는 ImGuizmo) + `CTransformPlacementCommand`(interactive: drag 시작~끝 1 command). Details TRS도 같은 command 경유. grid snap. | gizmo로 이동/회전/스케일 → 1회 undo로 drag 전체 되돌림. Details 값과 일치. | **시스템 완료 게이트** |
| **S6** | Cell 저장 → cell.json round-trip. `references[]`(transform 없음)은 reference-only로만 표시(배치 금지). 런타임(showcase 또는 probe)에서 같은 cell.json 로드 검증. | 편집→저장→**런타임이 같은 cell.json 로드해 동일 배치 재현**(왕복 닫힘). | G5(streaming 핸드오프 직전) |

각 단계는 "편집→저장→런타임 preview" 왕복으로 닫는다. S5 통과 전 FX/Sequencer 패널로 넘어가지 않는다(G6/G7 차단 규칙).

---

## 6. 코드 스켈레톤 (컴파일 가능 형태)

배치 위치: `EldenRingEditor/Public|Private/World/`. Engine public 경계 변경 없음(전부 editor 측, `ModelRenderer`/`IRHIDevice`/`WintersResolveContentPath`만 소비). 따라서 SDK sync 불필요(7절 참조).

```cpp
// EldenRingEditor/Public/World/EditorTransaction.h
#pragma once
#include "WintersTypes.h"
#include "WintersMath.h"
#include <memory>
#include <vector>
#include <string>

class IEditorCommand
{
public:
    virtual ~IEditorCommand() = default;
    virtual void        Do()   = 0;
    virtual void        Undo() = 0;
    virtual const char* Name() const = 0;
};

class CEditorTransaction
{
public:
    void Push(std::unique_ptr<IEditorCommand> cmd)
    {
        if (!cmd) return;
        cmd->Do();
        m_historyLabels.emplace_back(cmd->Name());
        m_undo.push_back(std::move(cmd));
        m_redo.clear();
    }
    bool CanUndo() const { return !m_undo.empty(); }
    bool CanRedo() const { return !m_redo.empty(); }
    void Undo()
    {
        if (m_undo.empty()) return;
        std::unique_ptr<IEditorCommand> cmd = std::move(m_undo.back());
        m_undo.pop_back();
        cmd->Undo();
        m_redo.push_back(std::move(cmd));
    }
    void Redo()
    {
        if (m_redo.empty()) return;
        std::unique_ptr<IEditorCommand> cmd = std::move(m_redo.back());
        m_redo.pop_back();
        cmd->Do();
        m_undo.push_back(std::move(cmd));
    }
    void Clear() { m_undo.clear(); m_redo.clear(); m_historyLabels.clear(); }
    const std::vector<std::string>& History() const { return m_historyLabels; }

private:
    std::vector<std::unique_ptr<IEditorCommand>> m_undo;
    std::vector<std::unique_ptr<IEditorCommand>> m_redo;
    std::vector<std::string>                     m_historyLabels;
};
```

```cpp
// EldenRingEditor/Public/World/WorldCellDocument.h
#pragma once
#include "WintersTypes.h"
#include "WintersMath.h"
#include <string>
#include <vector>

struct WorldPlacement
{
    u32_t       id = 0;
    std::string kind;
    std::string name;
    std::string wmesh;
    Vec3        position{ 0.f, 0.f, 0.f };
    Vec3        rotationDeg{ 0.f, 0.f, 0.f };
    Vec3        scale{ 1.f, 1.f, 1.f };
    bool        animated = false;
    bool        transformResolved = true;
};

struct WorldReference { std::string kind; std::string model; std::string reason; };

class CWorldCellDocument
{
public:
    bool Load(const std::string& strJsonRelative);   // WintersResolveContentPath / repo-relative walk
    bool Save(const std::string& strJsonRelative) const;

    const std::string& CellId() const { return m_cellId; }
    std::vector<WorldPlacement>& Placements() { return m_placements; }
    const std::vector<WorldPlacement>& Placements() const { return m_placements; }
    std::vector<WorldReference>& References() { return m_references; }

    WorldPlacement* FindPlacement(u32_t id)
    {
        for (WorldPlacement& p : m_placements)
            if (p.id == id) return &p;
        return nullptr;
    }
    u32_t AllocPlacementId() { return m_nextId++; }

private:
    std::string m_cellId;
    u32_t  m_area = 0, m_blockX = 0, m_blockY = 0, m_variant = 0;
    f32_t  m_cellSizeMeters = 64.f;
    Vec3   m_origin{ 0.f, 0.f, 0.f };
    std::string m_dataLayer = "Base";
    std::vector<WorldPlacement> m_placements;
    std::vector<WorldReference> m_references;
    u32_t m_nextId = 1;
};
```

```cpp
// EldenRingEditor/Public/World/TransformPlacementCommand.h
#pragma once
#include "World/EditorTransaction.h"
#include "World/WorldCellDocument.h"

class CWorldPreview;   // forward; 프리뷰 동기화 위임

class CTransformPlacementCommand final : public IEditorCommand
{
public:
    CTransformPlacementCommand(CWorldCellDocument* pDoc, CWorldPreview* pPreview, u32_t id,
                               const Vec3& posOld, const Vec3& rotOld, const Vec3& sclOld,
                               const Vec3& posNew, const Vec3& rotNew, const Vec3& sclNew)
        : m_pDoc(pDoc), m_pPreview(pPreview), m_id(id)
        , m_posOld(posOld), m_rotOld(rotOld), m_sclOld(sclOld)
        , m_posNew(posNew), m_rotNew(rotNew), m_sclNew(sclNew) {}

    void Do()   override { Apply(m_posNew, m_rotNew, m_sclNew); }
    void Undo() override { Apply(m_posOld, m_rotOld, m_sclOld); }
    const char* Name() const override { return "Transform Placement"; }
    void SetNew(const Vec3& p, const Vec3& r, const Vec3& s) { m_posNew = p; m_rotNew = r; m_sclNew = s; }

private:
    void Apply(const Vec3& p, const Vec3& r, const Vec3& s);  // cpp: doc 필드 갱신 + preview SyncPlacement
    CWorldCellDocument* m_pDoc;
    CWorldPreview*      m_pPreview;
    u32_t  m_id;
    Vec3   m_posOld, m_rotOld, m_sclOld;
    Vec3   m_posNew, m_rotNew, m_sclNew;
};
```

```cpp
// EldenRingEditor/Public/World/EditorPicker.h  (ray-AABB CPU pick)
#pragma once
#include "WintersMath.h"
#include "WintersTypes.h"

struct EditorRay { Vec3 origin; Vec3 dir; };

class CEditorPicker
{
public:
    // NDC 역변환으로 월드 ray 생성. proj/view는 showcase와 동일한 Mat4 사용.
    static EditorRay ScreenToWorldRay(f32_t mouseX, f32_t mouseY, u32_t vpW, u32_t vpH,
                                      const Mat4& matView, const Mat4& matProj, const Vec3& vEye);
    // slab method. 교차 시 outT(원점→교차 거리) 반환.
    static bool RayAABB(const EditorRay& ray, const Vec3& aabbMin, const Vec3& aabbMax, f32_t& outT);
};
```

```cpp
// EldenRingEditor/Public/World/EldenRingWorldEditorScene.h  (IScene 구현 = 셸에 등록)
#pragma once
#include "IScene.h"
#include "World/EditorTransaction.h"
#include "World/WorldCellDocument.h"
#include <memory>

class CWorldPreview;
class CEditorAssetCatalog;
class CTransformGizmo;

class CEldenRingWorldEditorScene final : public IScene
{
public:
    static std::unique_ptr<CEldenRingWorldEditorScene> Create();
    ~CEldenRingWorldEditorScene() override;

    bool OnEnter() override;
    void OnExit()  override;
    void OnUpdate(f32_t dt) override;     // free camera + interactive drag commit
    void OnRender() override;             // CWorldPreview::Render(viewProj, eye)
    void OnImGui()  override;             // DockSpace + 패널 + gizmo

private:
    CEldenRingWorldEditorScene() = default;
    void DrawViewportPanel();  void DrawContentBrowserPanel();
    void DrawOutlinerPanel();  void DrawDetailsPanel();  void DrawTransactionPanel();

    CWorldCellDocument                 m_doc;
    std::unique_ptr<CWorldPreview>     m_pPreview;
    std::unique_ptr<CEditorAssetCatalog> m_pCatalog;
    std::unique_ptr<CTransformGizmo>   m_pGizmo;
    CEditorTransaction                 m_txn;
    u32_t  m_selectedId = 0;             // CEditorSelection 최소형(단일 선택)
    // free camera 상태(showcase 이식): m_vCamPos, m_fYaw, m_fPitch, m_fAspect ...
};
```

`CEldenRingEditorApp::OnInit`은 `CEldenRingEditorScene::Create()` 대신 `CEldenRingWorldEditorScene::Create()`를 띄우도록 1줄 교체(`EldenRingEditorApp.cpp:20`).

---

## 7. 검증 · 리스크 · 과설계 방지

### 7.1 빌드 타겟별 MSBuild

```powershell
$msb = 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe'
# 에디터(이 시스템 주 타겟)
& $msb Winters.sln /t:EldenRingEditor /m /p:Configuration=Debug-DX12 /p:Platform=x64 /v:minimal
# Engine 변경이 있었다면(원칙상 없어야 함)
& $msb Winters.sln /t:Engine        /m /p:Configuration=Debug        /p:Platform=x64 /v:minimal
# LoL 영향 없음 확인(visual smoke 유지)
& $msb Winters.sln /t:Client        /m /p:Configuration=Debug        /p:Platform=x64 /v:minimal
git diff --check
```

- `.vcxproj`/`.filters`: 새 파일(`EldenRingEditor/.../World/*.h|cpp`)을 `EldenRingEditor.vcxproj`에 추가해야 한다. 계획서 규칙상 XML 변경은 코드 세션에서 **확인 후** 반영(검증의 "확인 필요"). UTF-8(`/utf-8`) 누락 시 한글 주석 CP949 오판 주의(MEMORY 규칙).
- **SDK sync 불필요**: 본 시스템은 Engine public header를 바꾸지 않는다(editor-local 타입만). 만약 `ModelRenderer`/`IRHIDevice`에 신규 public이 필요해지면 그 세션은 `UpdateLib.bat` 동기화를 검증에 포함.

### 7.2 게이트 막힐 때 대응

| 막힌 게이트 | 증상 | 대응 |
|---|---|---|
| **G3 (셸 dockspace)** | `--rhi=dx12`에서 ImGui 안 뜸 | World Editor 패널 확장 중단. `01_DX12_IMGUI_EDITOR_BOOTSTRAP.md`의 `ImGuiLayer` DX12 분기부터 반영(현 헤더 미반영 확인됨). DX11(`--rhi=dx11`)로 임시 진행은 가능하나 셸 목표는 DX12. |
| G2 (스태틱 에셋) | 뷰포트에 mesh 안 보임 | placement 확장 중단, RHI/ModelRenderer 경로부터. showcase가 이미 표시하므로 보통 통과. |
| G4 (catalog) | drag-drop/스캔 실패 | hot-reload·대량 스캔 멈추고 단일 `.wmesh` 경로부터. |
| 시스템 게이트 (command 무결성) | undo 후 doc/preview 불일치 | command가 doc과 preview를 **항상 함께** mutate하도록 강제(Apply 한 곳). 부분 적용 금지. |

### 7.3 과설계 방지 (Karpathy 가드레일)

- **multi-select·folder 계층·hit-proxy GPU pick 금지(초기)**: 단일 선택 + CPU ray-AABB로 시작. showcase가 이미 단일 선택으로 충분함을 증명.
- **reflection 흉내 금지**: per-type ImGui 인스펙터. UE `SDetailsView` 일반화 이식 안 함.
- **snapshot-기반 transaction 금지**: Winters 객체는 작다 → 명시적 command(old/new TRS)가 더 단순. UE `Object->Modify()` 직렬화 모델 이식 안 함.
- **streaming/data layer/HLOD를 이 문서에서 구현 금지**: cell **저장 contract**까지만. 런타임 streaming은 `WORLD_PARTITION` 시스템이 cell.json을 소비.
- **showcase 코드 중복 금지**: free camera·JSON 추출 헬퍼는 showcase에서 검증됨 → 공유 가능하면 추출, 아니면 editor로 최소 이식하되 복붙 누적 금지(gotchas 2026-06-05 Perf/Plan 규칙).
- **모든 변경 = command**: "처음부터 command 기반"이 본 시스템의 유일한 비타협 규칙. transform/add/delete를 직접 mutate하는 우회 경로를 만들지 않는다.

---

## 8. Codex 요구사항 프롬프트 (복붙용)

```text
SYSTEM=WORLD_EDITOR

너는 Winters 엔진의 EldenRingEditor 위에 UE5 Editor급 World Editor를 구축하는 시니어 엔진/툴 엔지니어다.
목표: Content Browser에서 선택한 .wmesh를 DX12 뷰포트에 프리뷰하고, ray-pick + Transform Gizmo로 배치/변형하고,
모든 편집을 Undo/Redo 가능한 command(CEditorTransaction)로 표현해 cell.json(.wcell 승격 예정)에 저장하고,
같은 데이터를 런타임이 그대로 로드하게 만든다.

[절대 원칙 — 위반 시 작업 무효]
1. UE5는 reference depot. UE 코드 복사/모듈 링크/object model(UObject/Slate) 이식 금지. 개념만 Winters식 재구성.
2. "에디터 화면 먼저 크게" 금지. runtime contract(cell.json + CWorldCellDocument + CWorldPreview)를 먼저 작게 증명한 뒤
   에디터 패널이 그 contract를 편집하게 한다. 모든 단계 완료기준 = "편집→저장→런타임 preview" 왕복.
3. 에디터가 만든 cell.json은 런타임에서 CAssetStreamingSystem(WORLD_PARTITION 시스템)을 거쳐 로드된다.
   에디터 전용 우회 로드 경로 금지. gameplay 판정(collision/nav/spawn/hitbox)은 Server GameSim. cell은 presentation seed.
4. Engine→Client 의존 역전 금지. Engine public header에 DX11/DX12 concrete type 노출 금지.
   본 시스템 신규 타입은 전부 EldenRingEditor/.../World/ 에 둔다(Engine public 변경 금지).
5. 모든 편집(배치/삭제/변형)은 IEditorCommand로 표현해 Undo/Redo가 처음부터 가능해야 한다. 직접 mutate 우회 금지.
6. normal F5 LoL DX11 runtime을 우회·은폐 금지. /t:Client visual smoke 계속 검증.

[환경]
- 저장소: C:/Users/tnest/Desktop/Winters
- 에디터: EldenRingEditor/ (WintersEldenRingEditor.exe, --rhi=dx12). 진입: main.cpp -> EldenRingEditorApp::OnInit -> Change_Scene.
- 재사용 코드(근거): ModelRenderer(Engine/Public/Renderer/ModelRenderer.h: Initialize/UpdateTransform/UpdateCamera/
  RenderFrustumCulled, HasValidAABB/GetLocalAABBMin|Max "Picking Editor", SetHoverOutline), IRHIDevice(GameInstance::Get_RHIDevice),
  WintersResolveContentPath(Engine/Include/WintersPaths.h), WMeshFormat(Engine/Public/AssetFormat/Mesh/WMeshFormat.h).
- 검증된 편집 baseline: EldenRingClient/Private/EldenLimgraveShowcaseScene.cpp (drag-place DragFloat3, JSON save/load,
  UpdateFreeCamera, SpawnInstance). 이걸 command/ray-pick/gizmo로 승격하는 것이 본 작업. undo/redo·pick·gizmo는 신규.

[먼저 읽을 문서 — 순서대로]
1. .md/EldenRing/18_WORLD_EDITOR_UE5_GRADE_DESIGN.md  ← 이 설계(포맷/클래스/패널/순서/게이트/스켈레톤)
2. .md/EldenRing/17_UE5_GRADE_EDITOR_SUITE_MASTER.md  ← 5대 시스템 매핑·게이트
3. .md/EldenRing/12_UE5_REFERENCE_DX12_RHI_EDITOR_BIG_PICTURE.md  ← Phase A~J·게이트 G0~G9
4. .md/plan/EldenRingEditor/01_DX12_IMGUI_EDITOR_BOOTSTRAP.md(셸 DX12 선행), 03(catalog/map assembly)
5. .md/EldenRing/21_WORLD_PARTITION_DESIGN.md(cell seed key 계약), CLAUDE.md, .claude/gotchas.md

[작업 범위 — WORLD_EDITOR만]
- DX12 ImGui DockSpace + 패널: Viewport, Content Browser, World Outliner, Details(Inspector), Transaction Stack, Log.
- 편집 코어: CEditorTransaction(command stack + redo), CTransform/Add/DeletePlacementCommand,
  CEditorPicker(screen→world ray, ray-AABB), CEditorSelection(단일 선택), CTransformGizmo(Translate/Rotate/Scale, World/Local, snap).
- 런타임 contract: CWorldCellDocument(cell.json Load/Save), CWorldPreview(placement→ModelRenderer).
- 산출: 선택 .wmesh 뷰포트 프리뷰 + gizmo 배치 + Undo/Redo + cell.json 저장→런타임 동일 재현.

[작업 루프 — 게이트 통과까지]
1. S0 선행: ImGuiLayer DX12 분기(01 plan) 반영 여부 확인. 미반영이면 01 먼저. DockSpace + 빈 패널 4개. → G3.
2. S1 contract 먼저: CWorldCellDocument(JSON) + CWorldPreview. 손수 만든 cell.json 1개 → 뷰포트 mesh 1개 + free camera.
3. S2 Content Browser drag-drop → CAddPlacementCommand. S3 CEditorTransaction + Undo/Redo + Transaction Stack.
4. S4 ray-pick + selection 동기(Outliner/Viewport/Details). S5 Gizmo + CTransformPlacementCommand(interactive: drag 전체 1 command) + snap.
5. S6 Save → cell.json round-trip → 런타임(showcase/probe)이 같은 cell.json 로드해 동일 배치 재현(왕복 닫힘).
6. 막히면 사유 분류 보고(특히 G3 셸, 의존 역전, command 무결성). 나머지는 계속.

[빌드 검증]
- 에디터: MSBuild Winters.sln /t:EldenRingEditor /p:Configuration=Debug-DX12 /p:Platform=x64 /v:minimal
- LoL 영향: /t:Client /p:Configuration=Debug (visual smoke 유지)
- Engine 변경 없어야 정상. 있으면 /t:Engine 빌드 + UpdateLib.bat SDK sync 확인.
- git diff --check. 새 파일은 EldenRingEditor.vcxproj/.filters에 추가(확인 후), /utf-8 확인.

[완료 기준 — WORLD_EDITOR]
- Content Browser에서 선택한 .wmesh가 DX12 뷰포트에 표시된다.
- ray-pick으로 선택되고 Outliner/Viewport/Details selection이 일치한다.
- Gizmo 이동/회전/스케일이 1회 Undo로 drag 전체 되돌려지고 Redo로 재적용된다.
- cell.json 저장 후 런타임이 같은 파일을 로드해 동일 배치를 재현한다(transform 없는 항목은 reference-only로 미배치).

[금지 사항]
- multi-select / folder 계층 / GPU hit-proxy pick (초기 금지, 단일 선택 + CPU ray-AABB).
- snapshot-기반 transaction(UE Object->Modify 모델) 이식. 명시적 command(old/new TRS)만.
- streaming/data layer/HLOD를 이 시스템에서 구현(= WORLD_PARTITION 소관). cell 저장 contract까지만.
- showcase 편집 코드 복붙 누적. 공유 가능하면 추출, 아니면 최소 이식.
- 직접 mutate 우회 경로(모든 편집은 command 경유).

[시작]
지금: (1) 위 문서를 읽고, (2) S0 선행(ImGuiLayer DX12 dockspace) 충족 여부와 현재 EldenRingEditorScene/ModelRenderer 코드 상태를
집계해 보고, (3) S1 runtime contract(CWorldCellDocument + CWorldPreview + cell.json seed) 최소 구현부터 시작하라.
막히면 사유를 분류해 보고하고 나머지는 계속 진행하라.
```
