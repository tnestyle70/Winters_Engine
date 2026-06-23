# World Partition + Asset Streaming 상세 설계

> 작성: 2026-06-23. 대상: Codex 및 EldenRing 팀.
> 선행: `17`(에디터 스위트 마스터), `12`(UE5 빅픽처·게이트 G0~G9), `04`(월드파티션 상세), `07`(에셋로더 런타임),
> `plan/EldenRingEditor/04`(Limgrave streaming seed).
> 시스템 식별자: **`WORLD_PARTITION`**(연동: `CAssetStreamingSystem` 런타임).

---

## 0. 한 줄 목표 + 시스템 경계

**한 줄 목표**: `build-map-placement`가 만든 `map_placement.json/.txt`를 grid cell(`.wcell`)로 승격하고, fake camera(에디터) / player(런타임) streaming source 이동에 따라 cell이 `Unloaded→…→Visible` 상태머신을 따라 전이하며, cell이 요구하는 에셋을 `CAssetStreamingSystem`이 priority 큐 + frame budget으로 비동기 로드해 stall 없이 mesh를 그리는 것.

**이 문서가 다루는 경계**:

| 다룬다 | 다루지 않는다(다른 문서) |
|---|---|
| `CWorldPartitionSystem`(Descriptor/CellRuntime/StreamingSourceRegistry/DataLayerSystem/HLODSystem/CellVisibilityResolver/CellStreamingScheduler) | FX graph(`.wfx`, 06문서) |
| `CAssetStreamingSystem`(handle/state/queue/worker/GPU upload budget/fallback/hot reload) | Sequencer(`.wseq`, 06문서) |
| `world.json`/`cell.json` → `.wmap`/`.wcell`/`.wnav` 포맷 + 승격 경로 | Boss/Hitbox(17문서 BOSS_TESTING) |
| WorldPartition 에디터 패널(grid overlay, cell inspector, streaming source, data layer, budget) | Material resolver(`.wmat`, 06문서) |
| Client cell(render) / Server sim cell(collision/nav/spawn) 경계 정의 | MSB transform parser 구현(파이프라인 영역) |

**완료 기준(게이트 G5 정렬)**: "에디터에서 fake camera를 이동 → cell state가 바뀌고 → Asset Streaming 패널에 handle state(Queued/LoadingDisk/Ready)가 보이고 → Visible cell의 mesh가 viewport에 그려지고 → transform 없는 reference는 draw call에 안 들어간다."

---

## 1. UE5 실제 아키텍처 (깊이)

### 1.1 World Partition의 존재 이유 (철학)

UE4의 `Persistent Level + Streaming Sublevel` 모델은 **레벨을 사람이 손으로 자른 단위**로만 스트리밍할 수 있었다. 오픈월드에서 이는 (a) 머지 충돌(여러 명이 한 `.umap`을 동시 편집), (b) 수동 셀 경계 관리 부담, (c) 액터 단위가 아닌 레벨 단위 로딩이라는 한계를 낳았다. UE5 World Partition은 이를 **자동 grid 분할 + 액터 단위 파일 + 런타임 spatial hash**로 대체했다.

### 1.2 핵심 클래스/개념

| UE5 개념 | 역할 | 데이터 흐름 |
|---|---|---|
| `UWorldPartition` | 월드의 파티션 subsystem 진입점 | 에디터 hash(`FWorldPartitionEditorHash`) + 런타임 hash(`UWorldPartitionRuntimeHash`) 소유 |
| **One File Per Actor (OFPA)** | 액터마다 `__ExternalActors__/.../<guid>.uasset` 1개 | `AActor`를 레벨 패키지에서 분리 → 머지 충돌 제거, 액터 단위 diff |
| `FWorldPartitionActorDesc` | 액터의 **로드하지 않고 아는 메타데이터**(bounds, class, refs, data layer) | 에디터/쿡 단계에서 actor desc만 스캔해 grid 배치 계산. 본체 uasset은 streaming 시점에만 로드 |
| `UWorldPartitionRuntimeSpatialHash` | 위치→cell 조회 spatial hash grid | grid는 여러 레벨(`HLOD0/1/2`)의 cell layer를 가짐. cell size·loading range가 grid 설정 |
| `UWorldPartitionRuntimeCell` | 런타임 streaming 단위 | 자기 안의 actor cluster를 streaming level로 묶어 async load. 상태머신(Unloaded/Loading/Loaded/Activated) |
| `UWorldPartitionStreamingSource` | 로딩 기준점 | player/camera/custom. position + radius(loading/activation) + priority + velocity 기반 prefetch |
| `UDataLayerAsset` / `UDataLayerInstance` | Editor Data Layer / Runtime Data Layer 분리 | Runtime Data Layer는 `Activated/Loaded/Unloaded` 상태로 같은 cell 안 액터 subset을 켬(quest/phase) |
| `AWorldPartitionHLOD` | 계층 LOD 프록시 | cell이 unloaded일 때 먼 거리 표현. HLOD0=merged static mesh, HLOD1=imposter 등 |
| `ALevelInstance` / `APackedLevelActor` | 재사용 레벨 인스턴스 / 정적 머지 프록시 | sub-level을 한 액터처럼 배치·인스턴싱 |

### 1.3 셀 상태 머신 (UE5 `EWorldPartitionRuntimeCellState`)

```text
Unloaded → Loading(streaming level async load) → Loaded(hidden) → Activated(visible/ticking)
         ← Unloading                            ← (deactivate)
```

UE5는 `bShouldBeLoaded`/`bShouldBeActivated`를 매 프레임 streaming source union으로 계산하고, `FStreamingGenerationContext`가 cell별 desired state를 만든 뒤 비동기 로드를 스케줄한다.

### 1.4 Async loading (별도 서브시스템)

| UE5 | 역할 |
|---|---|
| `FStreamableManager` | soft object path를 async/sync로 로드, handle(`FStreamableHandle`)로 lifetime/priority 관리 |
| `FAsyncLoadingThread` | package async load, 우선순위 큐, time-slice |
| `RequestAsyncLoad(paths, callback, priority)` | 여러 에셋을 묶어 요청, 완료 콜백 |
| `IoStore` / `FBulkData` | 쿡된 bulk data를 GPU/CPU로 스트리밍, 청크 단위 |

**UE5 통찰(반영할 것)**:
1. **메타데이터(actor desc)와 본체(uasset)를 분리** → 로드 안 하고 grid 배치/스트리밍 결정.
2. **cell = 에셋 요청 묶음** → cell streaming과 asset streaming은 별 시스템이지만 cell이 asset 핸들을 들고 desired state로 협조.
3. **streaming source는 union** → 여러 source(player/camera/raid)가 동시에 desired cell set을 만들고 합친다.
4. **HLOD로 unload 비용을 숨김** → 멀어진 cell은 unload하되 far proxy로 연속성 유지.

---

## 2. Winters 현재 구조 (실측 2026-06-23)

### 2.1 재사용 가능한 실제 코드

| 영역 | file:line 근거 | 재사용 방식 |
|---|---|---|
| **Nav grid** | `Engine/Public/Manager/Navigation/NavGrid.h:16` `CNavGrid`(512×512, 0.5u/cell), `:35` `WorldToCell`, `:36` `CellToWorld`, `:55-56` `SaveToFile/LoadFromFile`, `:53` `Load_Bits` | `.wnav`의 per-cell nav payload로 직결. grid math는 신규 cell 좌표계와 별개(nav는 2D 평지 슬라이스). |
| **Pathfinder** | `Engine/Public/Manager/Navigation/Pathfinder.h:14` `Find_Path`, `:19` `FindPathForRadius` | Server sim cell의 spawn/AI nav에 그대로. |
| **Mesh 리소스 캐시** | `Engine/Public/Renderer/RHIFxMeshResource.h:38` `LoadOrGet`, `:43` `Find`, `:36` `Create(IRHIDevice*)` | cell 에셋 로드의 GPU upload 백엔드. 단, 동기 API → 비동기 worker 앞단을 신규로 감싼다. |
| **Mesh 렌더러** | `Engine/Public/Renderer/ModelRenderer.h:28` `Initialize(fbxPath)`, `:30` `PrewarmModel`, `:31` `UpdateTransform`, `:42` `Render` | Client cell의 instance 렌더. `PrewarmModel`은 prefetch hook 후보. |
| **Map placement 산출** | `Tools/EldenAssetPipeline/elden_pipeline.py:4553` `build_map_placement_command`, schema `winters.elden.map_placement.v1`(`:4591`), `parts[]`{name,model,position,rotationDeg,scale,kind,wmesh}, `unresolved[]` | `.wcell` 승격의 입력. kind ∈ {MapPiece,Asset,Enemy,Player}. |
| **Limgrave 16타일** | `Client/Bin/Resource/EldenRing/Maps/Limgrave/m60_4{1..4}_3{5..8}_00/` | 타일 id가 곧 cell id 후보(`m60_42_36_00`). 좌표 41~44(x), 35~38(z) → 4×4 grid. |
| **런타임 placement 로더** | `EldenRingClient/Public/EldenLimgraveShowcaseScene.h:35` `EldenRuntimeMapPlacement`, `.cpp:607` `SpawnPlacements`, `:562` `TrySpawnMapPlacement`, `:644` `getline` 파싱(`|` 구분) | `map_placement.txt` 파서 패턴을 cell loader가 재사용. |
| **Probe scene** | `EldenRingClient/Public/EldenRingProbeScene.h` `m_pMapAssembly`(plan04 anchor) | WorldPartition 시스템을 붙일 scene host. |
| **스칼라/수학** | `Engine/Include/WintersTypes.h:29` `f32_t`, `:38` `u32_t`, `:39` `u64_t`, `:36` `u8_t`, `:31` `bool_t`, `:34` `i32_t`; `Engine/Include/WintersMath.h:35` `Vec3`(저장용 XMFLOAT3) | 신규 코드 전부 이 타입 사용. `Mat4`는 저장용. |

### 2.2 미구현 (신규로 작성)

- `CWorldPartitionSystem` 및 하위 7개 컴포넌트 — **존재하지 않음**(`grep -rl CWorldPartition Engine Client EldenRingClient` → 0건).
- `CAssetStreamingSystem` 및 handle/state/queue/worker/upload budget — **존재하지 않음**.
- `world.json`/`.wmap`/`.wcell`/`.wnav` 포맷 정의 및 로더 — **미정의**(04문서는 schema만 예시).
- map_placement의 transform이 현재 **모두 `position=0`**(MSB transform parser 미연동, `m423600_0000` 등 0벡터 확인) → plan04의 "transform 없는 reference는 draw call 제외" 규칙이 그대로 유효.

**의존 방향 확인**: `CNavGrid`/`CRHIFxMeshResourceCache`/`ModelRenderer`는 Engine(`WINTERS_ENGINE`) 소유. WorldPartition/AssetStreaming은 **Engine에 둔다**(LoL/Elden 공용 서비스). Client/Public·Shared에 DX11/DX12 concrete type을 노출하지 않고 `IRHIDevice*` opaque handle만 받는다(`RHIFxMeshResource.h:5,36` 패턴 그대로).

---

## 3. Winters 설계

### 3.1 포맷 스키마 (JSON 초기 → `.w*` 승격)

#### 3.1.1 `world.json` → `.wmap` (월드 디스크립터)

`map_placement` 타일 16개를 grid로 묶는 상위 인덱스. 04문서 예시를 cell 좌표계를 확정해 심화.

```jsonc
{
  "schema": "winters.elden.world.v1",
  "name": "Limgrave",
  "cellSizeMeters": 256.0,        // EldenRing 타일 1개 = 1 cell (m60_4X_3Y). UE grid loading range 대응
  "origin": [0.0, 0.0, 0.0],
  "gridDim": { "x": 4, "z": 4 },  // x: 41..44, z: 35..38
  "tileBase": { "x": 41, "z": 35 },
  "cells": [
    {
      "id": "m60_42_36_00",
      "coord": { "x": 1, "z": 1 },              // (tileX-41, tileZ-35)
      "boundsMin": [256.0, -64.0, 256.0],
      "boundsMax": [512.0, 192.0, 512.0],
      "file": "Cells/m60_42_36_00.wcell.json",  // 초기 JSON, 승격 시 .wcell
      "nav": "Nav/m60_42_36_00.wnav",
      "hlod": "HLOD/m60_42_36_00_hlod.wmesh",   // optional
      "dataLayers": ["Base", "Gameplay"],
      "placed": 307, "unresolved": 451          // map_placement.counts 미러(편집 진단용)
    }
  ]
}
```

#### 3.1.2 `cell.json` → `.wcell` (셀 디스크립터)

`map_placement.json.parts[]`를 cell 단위로 분리·승격. **`unresolved`/`transform==0`은 `placeable=false`로 명시**(draw call 제외 계약을 데이터에 박는다).

```jsonc
{
  "schema": "winters.elden.cell.v1",
  "id": "m60_42_36_00",
  "sourcePlacement": "Maps/Limgrave/m60_42_36_00/map_placement.json",
  "instances": [
    {
      "name": "m423600_0000",
      "model": "m423600",
      "kind": "MapPiece",                  // MapPiece|Asset (render). Enemy/Player는 spawnPoints로 분리
      "wmesh": "EldenRing/FullGame/map/.../m60_42_36_00_423600.wmesh",
      "wmat":  "EldenRing/FullGame/.../m423600.wmat",  // optional, 없으면 default
      "transform": { "position": [0,0,0], "rotationDeg": [0,0,0], "scale": [1,1,1] },
      "placeable": false,                  // transform 미해결 → renderer 제외, catalog-only
      "dataLayer": "Base",
      "required": true                     // Required(near mesh/collision) vs Optional(decal/ambient)
    }
  ],
  "spawnPoints": [
    { "kind": "Enemy",  "archetype": "Soldier01", "position": [20,0,12], "dataLayer": "Gameplay" },
    { "kind": "Player", "name": "c0000_5000",      "position": [0,0,0],   "dataLayer": "Base" }
  ],
  "nav": "Nav/m60_42_36_00.wnav"
}
```

#### 3.1.3 `.wnav` (네비 페이로드)

`CNavGrid::SaveToFile`(`NavGrid.h:55`) 산출을 그대로 사용. cell당 1 `.wnav` = origin(`Get_OriginX/Z`) + bit payload(`Get_Bits`, `kByteSize`). Server sim cell이 소비.

#### 3.1.4 승격 경로 (도구)

`elden_pipeline.py`에 `build-world-cells` 서브커맨드를 추가(이 시스템 범위의 도구 작업): `Maps/Limgrave/*/map_placement.json` 16개를 읽어 `world.json`(상위) + `Cells/*.wcell.json`(16개)로 변환. `transform==0 && kind!=Player`는 `placeable=false`. 이후 안정화되면 JSON→binary(`.wcell`) writer 추가.

### 3.2 런타임 클래스 계층 (C++ 시그니처)

```text
Engine (WINTERS_ENGINE, IRHIDevice* 만 받음)
└── CWorldPartitionSystem
    ├── CWorldDescriptor          // world.json/.wmap 파싱, cell 인덱스 + grid hash
    ├── CWorldCellRuntime[]       // cell별 상태 + 인스턴스 핸들 + asset 핸들 집합
    ├── CStreamingSourceRegistry  // StreamingSourceComponent[] union → desired cell set
    ├── CDataLayerSystem          // layerMask 토글 → 인스턴스 enable/disable
    ├── CHLODSystem               // unloaded cell far proxy 표시/숨김
    ├── CCellVisibilityResolver   // desired load/visible/unload 판정(상태머신 입력)
    └── CCellStreamingScheduler   // 상태 전이 펌프, CAssetStreamingSystem에 요청 발행

└── CAssetStreamingSystem
    ├── CAssetHandleRegistry      // AssetHandle(u32) → eAssetState + refcount
    ├── CAssetRequestQueue        // priority sort
    ├── CAssetDependencyResolver  // cell → mesh/mat/tex 의존 수집
    ├── CAsyncFileLoader          // worker thread: disk read/parse → 중간 버퍼
    ├── CGpuUploadQueue           // main/render thread: frame budget(4 jobs/16MB)
    └── CAssetStreamingBudget     // per-frame 한도
```

### 3.3 에디터 패널 (ImGui, EldenRingEditor)

04문서 패널 트리를 cell 좌표계 확정에 맞춰 구체화. **모든 편집은 command로** → 17문서 `CEditorTransaction` 연동(WORLD_EDITOR 시스템이 소유, 여기서는 placement transform 편집만 발행).

```text
World Partition  (CWorldPartitionPanel)
├── Current World        : world.json 선택/로드, grid 4×4 overlay, origin/cellSize 표시
├── Streaming Sources    : source 추가(Player/Camera/Editor), fake camera slider(x,y,z), radius(load/visible/unload) 시각화
├── Loaded Cells         : 16 cell 색상(Unloaded=회색 … Visible=초록), 강제 load/unload 버튼
├── Cell Inspector       : 선택 cell의 instances 표(name/kind/placeable/required), placeable 토글, transform edit
├── Data Layers          : Base/Gameplay/RaidPhase1.. 체크박스 → layerMask
└── Budget               : queued/loadingDisk/ready/failed count, GPU upload bytes this frame, cache MB
```

핵심 UX: fake camera slider를 움직이면 `CStreamingSourceRegistry`가 desired set을 재계산 → `Loaded Cells` 색상이 실시간으로 바뀌고 `Budget`의 handle count가 증가 → Visible cell mesh가 viewport에 등장. 이것이 패널 완료 기준의 시각 증거.

---

## 4. 데이터 흐름 (presentation/truth 경계)

```text
[에디터]                                   [도구]                         [런타임]
WorldPartition 패널                  elden_pipeline.py
  fake camera/source 편집      build-map-placement → map_placement.json
  cell instance transform 편집        │
        │                       build-world-cells
        ▼                             ▼
  CEditorTransaction(command)   world.json + Cells/*.wcell.json + Nav/*.wnav
        │                             │
        └─────── 저장 ────────────────┤
                                      ▼
                            CWorldPartitionSystem
                              CWorldDescriptor(parse) ──┐
                              CStreamingSourceRegistry  │ desired cell set
                              CCellVisibilityResolver ◄─┘
                              CCellStreamingScheduler
                                      │ AssetLoadRequest(priority)
                                      ▼
                            CAssetStreamingSystem
                              queue → async worker(disk/parse)
                              → CGpuUploadQueue(budget) → CRHIFxMeshResourceCache
                              → AssetHandle = Ready
                                      │
                  ┌───────────────────┴────────────────────┐
          [Client cell = presentation]            [Server sim cell = truth]
          ModelRenderer(UpdateTransform/Render)    collision / nav(.wnav→CNavGrid) /
          HLOD proxy / DataLayer 가시성            spawn table / entity authority
          (transform 없는 ref → draw 제외)         (CPathfinder, GameSim)
```

**경계 불변식**:
1. **presentation**: cell의 mesh/material/HLOD/가시성/FX는 Client(EldenRingClient)·Engine 렌더 경로. cell이 보이는지 여부는 클라가 판단해도 무방(연출).
2. **truth**: collision/nav/spawn/entity authority는 Server sim cell. 클라가 cell을 visible로 만들었다고 적이 스폰되거나 데미지가 나지 않는다. spawnPoint는 **후보**로 서버에 전달, 실제 스폰·판정은 GameSim.
3. **우회 금지**: 에디터가 만든 `.wcell`은 반드시 `CAssetStreamingSystem`을 통과해 로드. 에디터 전용 직접 mesh 로드 경로를 만들지 않는다. normal F5 LoL runtime을 숨기지 않는다.

---

## 5. 구현 순서 (단계 + 완료기준 + 게이트)

전제 게이트: **G2(추출 static mesh 1개가 `.wmesh/.wmat`로 viewport 표시)** 와 **G3(DX12 ImGui dockspace+viewport)** 가 선행. 미충족 시 WP 패널 확장 중단(17문서 게이트 우선 원칙).

| 단계 | 내용 | 완료 기준 | 게이트 |
|---|---|---|---|
| **S0** | grid math + cell id ↔ coord(`m60_4X_3Y` 파서). `CWorldDescriptor`가 `world.json` 16 cell 파싱 | `WorldToCellId(Vec3)` 유닛 검증, cell 16개 인덱싱 | WP0~WP1 |
| **S1** | `build-world-cells` 도구: `map_placement.json` → `world.json`+`Cells/*.wcell.json`. transform==0 → `placeable=false` | 16 `.wcell.json` 생성, placeable/unresolved 카운트가 map_placement.counts와 일치 | (도구) |
| **S2** | `CAssetHandleRegistry` + `eAssetState`. cell 의존(`CAssetDependencyResolver`)이 wmesh/wmat 목록 수집 | 패널에 handle→state 표 표시(전부 Unloaded) | G5 진입 |
| **S3** | `CStreamingSourceRegistry` + `CCellVisibilityResolver`. fake camera union → desired load/visible/unload | 패널 slider 이동 시 cell 색상(Unloaded↔Queued↔LoadedHidden↔Visible) 전이 | WP1, plan04 |
| **S4** | `CCellStreamingScheduler`(상태 펌프, **초기 main-thread sync baseline**) + `CGpuUploadQueue` budget(4/16MB). Visible cell instance를 `ModelRenderer`로 렌더 | Visible cell의 `placeable=true` mesh가 viewport에 그려짐. `placeable=false`는 draw 제외 | **G5** |
| **S5** | async 전환: `CAsyncFileLoader` worker thread + priority 큐. stall 없이 cell 전환 | fake camera 빠르게 이동해도 frame hitch 없이 load/unload | WP5 |
| **S6** | `CDataLayerSystem`(layerMask 토글) + unload 경로(refcount 0 → release) | Data Layer 체크박스로 Gameplay 인스턴스 on/off, 멀어진 cell unload 후 count 감소 | WP6 |
| **S7** | `CHLODSystem` far proxy(수동 `.wmesh` 1개) + Server sim cell 분리(`.wnav`→`CNavGrid`, spawnPoint→GameSim 후보) | 멀리 cell unload 시 HLOD proxy 표시, Server가 sim cell nav 로드 | WP8, G9 정렬 |
| **S8** | hot reload(`.wcell` dirty → atomic swap) + `.wcell` binary writer(JSON 대체) | 에디터에서 transform 편집·저장 후 런타임 즉시 반영 | WP9 |

게이트 G5(streaming state) 통과 전에는 FX/Sequencer 대량 로드 연결 금지. G9(server authority) 정렬: spawn/판정은 절대 client cell에서 만들지 않는다.

---

## 6. 코드 스켈레톤 (컴파일 가능 형태)

### 6.1 `Engine/Public/World/WorldPartitionTypes.h`

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"
#include <string>
#include <vector>

namespace Engine
{
    // 04문서 8단계 상태머신 — UE5 EWorldPartitionRuntimeCellState의 Winters 확장판
    enum class eWorldCellState : u8_t
    {
        Unloaded = 0,
        Queued,
        LoadingMetadata,
        LoadingAssets,
        CreatingEntities,
        LoadedHidden,
        Visible,
        Unloading
    };

    // 위치→cell 조회 union 입력. 04문서 StreamingSourceComponent + UE5 streaming source priority.
    struct StreamingSourceComponent
    {
        Vec3  position{ 0.f, 0.f, 0.f };
        f32_t loadRadius    = 160.f;
        f32_t visibleRadius = 220.f;
        f32_t unloadRadius  = 280.f;
        u32_t priority      = 0u;
        bool_t bActive      = true;
    };

    // 같은 cell 안 subset 토글(quest/raid phase). 04문서 DataLayerComponent.
    struct DataLayerComponent
    {
        u64_t layerMask = 0ull;   // bit per data layer
    };

    struct CellInstanceDesc
    {
        std::string strName;
        std::string strModel;
        std::string strWmesh;     // 비면 placeholder/hidden
        std::string strWmat;      // 비면 default material
        Vec3   vPosition{ 0.f, 0.f, 0.f };
        Vec3   vRotationDeg{ 0.f, 0.f, 0.f };
        Vec3   vScale{ 1.f, 1.f, 1.f };
        u64_t  layerBit = 1ull;   // Base
        bool_t bPlaceable = false;// transform 해결 전엔 false → draw 제외
        bool_t bRequired  = true; // Required(near/collision) vs Optional
    };

    struct CellSpawnPoint
    {
        std::string strKind;       // Enemy|Player (서버 후보)
        std::string strArchetype;
        Vec3   vPosition{ 0.f, 0.f, 0.f };
        u64_t  layerBit = 1ull;
    };

    struct CellDescriptor
    {
        std::string strId;                       // "m60_42_36_00"
        i32_t  iCoordX = 0;                      // (tileX - base)
        i32_t  iCoordZ = 0;
        Vec3   vBoundsMin{ 0.f, 0.f, 0.f };
        Vec3   vBoundsMax{ 0.f, 0.f, 0.f };
        std::string strNavPath;                  // .wnav
        std::string strHlodWmesh;                // optional
        std::vector<CellInstanceDesc> instances;
        std::vector<CellSpawnPoint>   spawnPoints;
    };

    struct WorldDescriptor
    {
        std::string strName;
        f32_t  fCellSizeMeters = 256.f;
        Vec3   vOrigin{ 0.f, 0.f, 0.f };
        i32_t  iGridDimX = 0;
        i32_t  iGridDimZ = 0;
        i32_t  iTileBaseX = 0;
        i32_t  iTileBaseZ = 0;
        std::vector<CellDescriptor> cells;       // 헤더만(에셋 본체는 streaming 시점에 로드)
    };
}
```

### 6.2 `Engine/Public/World/AssetStreamingSystem.h`

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "RHI/IRHIDevice.h"          // opaque IRHIDevice* 만 — concrete backend 비노출
#include <memory>
#include <string>
#include <vector>

namespace Engine
{
    class CRHIFxMeshResourceCache;   // 전방선언 — GPU upload 백엔드

    using AssetHandle = u32_t;       // 07문서 계약
    constexpr AssetHandle kInvalidAssetHandle = 0u;

    enum class eAssetKind : u8_t
    { Mesh, Skeleton, Animation, Texture, Material, Fx, Sequence, WorldCell, Sound };

    enum class eAssetState : u8_t
    { Unloaded, Queued, LoadingDisk, Decoding, WaitingGpuUpload, Ready, Failed };

    struct AssetLoadRequest
    {
        u64_t      pathHash = 0ull;
        std::string strPath;
        eAssetKind kind = eAssetKind::Mesh;
        u32_t      priority = 0u;       // 1000 player … 100 far HLOD
        bool_t     bBlockingAllowed = false;
        bool_t     bGpuRequired = true;
    };

    struct AssetStreamingBudget
    {
        u32_t maxUploadJobsPerFrame  = 4u;
        u32_t maxUploadBytesPerFrame = 16u * 1024u * 1024u;
        f32_t maxBlockingLoadMs      = 5.f;
    };

    struct AssetStreamingStats
    {
        u32_t queued = 0u, loadingDisk = 0u, decoding = 0u,
              waitingGpu = 0u, ready = 0u, failed = 0u;
        u32_t uploadBytesThisFrame = 0u;
        u64_t cacheBytes = 0ull;
    };

    class WINTERS_ENGINE CAssetStreamingSystem final
    {
    public:
        ~CAssetStreamingSystem();
        CAssetStreamingSystem(const CAssetStreamingSystem&) = delete;
        CAssetStreamingSystem& operator=(const CAssetStreamingSystem&) = delete;

        // pCache: GPU upload 백엔드(RHIFxMeshResource.h:36 Create로 만든 것)
        static std::unique_ptr<CAssetStreamingSystem> Create(
            IRHIDevice* pDevice, CRHIFxMeshResourceCache* pMeshCache);

        AssetHandle Request(const AssetLoadRequest& req); // 중복 path는 기존 handle + refcount++
        void        AddRef(AssetHandle h);
        void        Release(AssetHandle h);               // refcount 0 → unload 후보

        eAssetState GetState(AssetHandle h) const;
        bool_t      IsReady(AssetHandle h) const;

        // main/render thread: worker가 준비한 페이로드를 GPU로 budget 안에서 업로드
        void        PumpGpuUploads();
        // 매 프레임: worker 완료 수거 + 상태 전이
        void        Update(f32_t deltaTime);

        void        SetBudget(const AssetStreamingBudget& budget);
        AssetStreamingStats GetStats() const;

        // hot reload(S8)
        void        MarkDirty(const std::string& strPath);

    private:
        CAssetStreamingSystem();
        bool_t Initialize(IRHIDevice* pDevice, CRHIFxMeshResourceCache* pMeshCache);
        struct Impl;
        Impl* m_pImpl = nullptr;
    };
}
```

### 6.3 `Engine/Public/World/WorldPartitionSystem.h`

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"
#include "World/WorldPartitionTypes.h"
#include <memory>
#include <string>
#include <vector>

namespace Engine
{
    class CAssetStreamingSystem;

    // 런타임 cell 인스턴스 — 디스크립터 + 현재 상태 + 핸들 집합
    struct WorldCellRuntime
    {
        const CellDescriptor* pDesc = nullptr;
        eWorldCellState state = eWorldCellState::Unloaded;
        std::vector<u32_t> assetHandles;   // CAssetStreamingSystem AssetHandle
        f32_t fUnloadTimer = 0.f;
        bool_t bRequiredReady = false;     // Required 의존이 전부 Ready인가
    };

    struct CellStateCounts
    {
        u32_t unloaded=0, queued=0, loadingMeta=0, loadingAssets=0,
              creating=0, loadedHidden=0, visible=0, unloading=0;
    };

    // 렌더가 소비할 결과: 이번 프레임 그릴 수 있는 인스턴스(placeable & Ready만)
    struct VisibleInstance
    {
        const CellInstanceDesc* pInst = nullptr;
        Mat4 matWorld;
    };

    class WINTERS_ENGINE CWorldPartitionSystem final
    {
    public:
        ~CWorldPartitionSystem();
        CWorldPartitionSystem(const CWorldPartitionSystem&) = delete;
        CWorldPartitionSystem& operator=(const CWorldPartitionSystem&) = delete;

        static std::unique_ptr<CWorldPartitionSystem> Create(CAssetStreamingSystem* pStreaming);

        bool_t LoadWorld(const std::string& strWorldJsonPath);   // world.json/.wmap
        void   Unload();

        // streaming source 관리(fake camera는 SetSource로 갱신)
        void   SetSource(u32_t sourceId, const StreamingSourceComponent& src);
        void   RemoveSource(u32_t sourceId);

        // cell coord 조회(grid hash). 04문서 WP0
        bool_t WorldToCellId(const Vec3& vWorld, std::string& outCellId) const;

        // 매 프레임: source union → desired state → 상태 펌프 → streaming 요청
        void   Update(f32_t deltaTime);

        // 렌더 단계가 호출: placeable && Ready인 Visible cell 인스턴스만 채움
        void   CollectVisibleInstances(std::vector<VisibleInstance>& out) const;

        // 에디터 강제 조작 / 진단
        void   ForceLoadCell(const std::string& strCellId);
        void   ForceUnloadCell(const std::string& strCellId);
        void   SetDataLayerMask(u64_t mask);                     // CDataLayerSystem
        CellStateCounts GetStateCounts() const;
        const WorldDescriptor& GetDescriptor() const;
        const std::vector<WorldCellRuntime>& GetCells() const;   // 패널 표시용

    private:
        CWorldPartitionSystem();
        bool_t Initialize(CAssetStreamingSystem* pStreaming);
        struct Impl;   // CWorldDescriptor/Registry/Resolver/Scheduler/DataLayer/HLOD 캡슐화
        Impl* m_pImpl = nullptr;
    };
}
```

### 6.4 렌더 연동 (probe scene, plan04 anchor 정렬)

```cpp
// EldenRingClient/Private/EldenAssetProbeScene.cpp OnUpdate (plan04 1-4 anchor)
void CEldenRingAssetProbeScene::OnUpdate(f32_t deltaTime)
{
    if (m_pCubeRenderer) m_pCubeRenderer->Update(deltaTime);

    if (m_pWorldPartition)
    {
        Engine::StreamingSourceComponent src;
        src.position = m_vEditorCameraWorld;          // fake camera slider
        src.loadRadius = 280.f; src.visibleRadius = 220.f; src.unloadRadius = 360.f;
        m_pWorldPartition->SetSource(/*editorCursor*/0u, src);
        m_pAssetStreaming->Update(deltaTime);
        m_pWorldPartition->Update(deltaTime);
        m_pAssetStreaming->PumpGpuUploads();          // render-thread budget
    }
}

// OnRender: transform 없는 ref는 들어오지 않음(placeable 필터는 시스템 내부에서 끝)
void CEldenRingAssetProbeScene::OnRender()
{
    std::vector<Engine::VisibleInstance> visible;
    if (m_pWorldPartition) m_pWorldPartition->CollectVisibleInstances(visible);
    for (const auto& vi : visible)
    {
        ModelRenderer* r = ResolveRendererForWmesh(vi.pInst->strWmesh); // 캐시
        r->UpdateTransform(vi.matWorld);
        r->Render();
    }
}
```

---

## 7. 검증·리스크

### 7.1 빌드 타겟별 MSBuild

```powershell
$MSB = 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe'
# Engine(신규 World/* 시스템 — public header 추가 시 SDK sync)
& $MSB Winters.sln /t:Engine /m /p:Configuration=Debug /p:Platform=x64 /v:minimal
# Editor 패널
& $MSB Winters.sln /t:EldenRingEditor /m /p:Configuration=Debug-DX12 /p:Platform=x64 /v:minimal
# Elden 런타임(probe scene 연동)
& $MSB Winters.sln /t:EldenRingClient /m /p:Configuration=Debug-DX12 /p:Platform=x64 /v:minimal
# LoL 영향 smoke(공용 Engine 변경이므로 반드시)
& $MSB Winters.sln /t:Client /m /p:Configuration=Debug /p:Platform=x64 /v:minimal
git diff --check
```

Engine public header(`Engine/Public/World/*.h`) 추가 시 `UpdateLib.bat`/SDK sync로 `EngineSDK/inc/World/*` 동기화 확인(`RHIFxMeshResource.h`가 `EngineSDK/inc`에 미러된 것과 동일 패턴).

### 7.2 게이트 막힐 때 대응

| 막힘 | 대응 |
|---|---|
| G2(static mesh) 미통과 | WP 패널 확장 중단. `.wmesh` 단일 표시부터(plan02). cell 렌더는 G2 후. |
| S4에서 cell mesh 안 보임 | placeable=true인데 transform==0이면 원점 겹침. 카운트(placed/unresolved)와 inspector로 진단. **MSB transform parser 미연동이 근본 원인** → placeable=false 유지가 정답(억지로 켜지 말 것). |
| async(S5)에서 race/hitch | sync baseline(S4)로 롤백해 격리. worker는 disk/parse만, GPU 생성은 main(`PumpGpuUploads`)에서만(07문서 정책). |
| LoL F5 깨짐 | Engine 공용 변경이 LoL 렌더 경로를 건드렸는지. WorldPartition은 opt-in(scene이 Create 호출할 때만 활성), 기본 비활성 보장. |

### 7.3 과설계 방지 (Karpathy 가드레일)

- **OFPA·Level Instance·HLOD 다단계·spatial hash 최적화는 P3**. 초기엔 cell당 1 `.wcell`, 선형 16-cell 스캔, sync baseline, HLOD proxy 1개. 04문서 "cell 1개, fake camera, JSON metadata부터".
- DataLayer는 `u64_t layerMask` 비트 하나로 시작. 레이어 에디터 풀 UI 금지.
- 새 async 경로를 만들기 전 `CRHIFxMeshResourceCache`/`ModelRenderer` 동기 경로를 재사용·감싸기(gotchas 2026-06-05 perf 규칙).
- streaming source는 fake camera 1개로 증명 후 player/raid 확장. union 일반화 선반영 금지.

---

## 8. Codex 요구사항 프롬프트 (복붙용)

```text
SYSTEM=WORLD_PARTITION   # World Partition + Asset Streaming 전담

너는 Winters 엔진에 UE5 World Partition + Asset Streaming급 시스템을 구축하는 시니어 엔진 엔지니어다.
UE5(World Partition / FStreamableManager)는 reference depot일 뿐 — 코드 복사/모듈 링크/object model 이식 금지.
개념만 Winters .w* contract + Winters runtime으로 재구성한다.

[절대 원칙 — 위반 시 작업 무효]
1. UE5는 개념·UX·책임분리 관찰용. UE 코드/타입 복사 금지.
2. "에디터 화면 먼저 크게" 금지. runtime contract(world.json/.wcell + CWorldPartitionSystem/CAssetStreamingSystem)를
   작게 증명 → 에디터가 그 contract를 편집. 완료기준 = "fake camera 이동 → cell state 전이 → handle state →
   Visible cell mesh가 viewport에 보임".
3. 에디터가 만든 .wcell은 반드시 CAssetStreamingSystem 거쳐 로드. 에디터 전용 직접 로드 우회 금지.
   spawn/collision/nav/damage 판정은 Server sim cell(GameSim) 권위. client cell은 render presentation만.
4. Engine→Client 의존 역전 금지. WorldPartition/AssetStreaming은 Engine 소유. Client/Public·Shared에
   DX11/DX12 concrete type 노출 금지(IRHIDevice* opaque handle만).
5. normal F5 LoL runtime 우회·은폐 금지. WorldPartition은 scene이 Create할 때만 opt-in 활성.

[환경]
- 저장소: C:/Users/tnest/Desktop/Winters
- 런타임: EldenRingClient/ (--rhi=dx11 또는 Debug-DX12), Engine/(공용 RHI/렌더러/리소스)
- 에디터: EldenRingEditor/ (WintersEldenRingEditor.exe, DX12 ImGui 셸 빌드됨)
- 재사용: Engine CNavGrid(Navigation/NavGrid.h, 512×512, WorldToCell/CellToWorld/Save/LoadFromFile),
  CPathfinder, CRHIFxMeshResourceCache(Renderer/RHIFxMeshResource.h LoadOrGet/Find/Create(IRHIDevice*)),
  ModelRenderer(Renderer/ModelRenderer.h Initialize/UpdateTransform/Render/PrewarmModel)
- 입력 데이터: Client/Bin/Resource/EldenRing/Maps/Limgrave/m60_4{1..4}_3{5..8}_00/map_placement.json(.txt)
  schema winters.elden.map_placement.v1, parts[]{name,model,position,rotationDeg,scale,kind,wmesh}, unresolved[]
  (주의: 현재 대부분 transform==0 — MSB parser 미연동. transform==0 && kind!=Player는 placeable=false)
- 미구현(신규): CWorldPartitionSystem, CAssetStreamingSystem, world.json/.wmap/.wcell/.wnav

[먼저 읽을 문서 — 순서대로]
1. .md/EldenRing/18_WORLD_PARTITION_ASSET_STREAMING.md  ← 이 설계(포맷/클래스/순서/게이트)
2. .md/EldenRing/04_WORLD_PARTITIONING_STREAMING.md     ← cell 상태머신·source·DataLayer·HLOD 상세
3. .md/EldenRing/07_ASSET_LOADER_AND_STREAMING_RUNTIME.md ← handle/state/queue/upload budget/fallback/hot reload
4. .md/EldenRing/12_..._BIG_PICTURE.md (Phase E, 게이트 G5/G9), 17_..._EDITOR_SUITE_MASTER.md
5. .md/plan/EldenRingEditor/04_WORLD_PARTITION_STREAMING.md (Limgrave seed anchor)
6. CLAUDE.md / .claude/gotchas.md

[작업 범위 — WORLD_PARTITION]
- 포맷: world.json + Cells/*.wcell.json + Nav/*.wnav 정의·로더. 도구 build-world-cells(elden_pipeline.py)로
  map_placement.json 16타일 → world.json + 16 .wcell.json 승격. transform==0 → placeable=false.
- 런타임: CWorldPartitionSystem{WorldDescriptor, CellRuntime, StreamingSourceRegistry, DataLayerSystem,
  HLODSystem, CellVisibilityResolver, CellStreamingScheduler}, eWorldCellState 8단계.
  CAssetStreamingSystem{HandleRegistry, RequestQueue, DependencyResolver, AsyncFileLoader, GpuUploadQueue,
  Budget(4 jobs/16MB)}, AssetHandle(u32)/eAssetState. Required vs Optional, fallback(placeholder/default).
- 에디터: WorldPartition 패널(Current World grid overlay / Streaming Sources fake camera slider+radius /
  Loaded Cells 색상+강제 load·unload / Cell Inspector instances+placeable 토글 / Data Layers / Budget).
- 분리: Client cell(render: ModelRenderer/HLOD/가시성) / Server sim cell(.wnav→CNavGrid/spawn 후보→GameSim).

[작업 루프 — 게이트 통과까지]
1. 선행 게이트 확인: G2(static mesh .wmesh viewport 표시) G3(DX12 dockspace). 미충족 시 WP 패널 확장 멈추고 선행부터.
2. runtime contract 먼저: S0 grid math → S1 build-world-cells → S2 handle registry → S3 source/visibility →
   S4 sync baseline 렌더(G5) → S5 async → S6 datalayer/unload → S7 HLOD+server sim 분리 → S8 hot reload/binary.
3. 각 단계 완료기준은 "편집/소스 이동 → 상태 전이 → preview". S4에서 Visible cell mesh가 실제로 보여야 G5.
4. presentation/truth 구분: spawn/nav/판정은 Server sim cell. 절대 client cell에서 gameplay 결과 생성 금지.
5. 막히면 사유 분류 보고(특히 transform==0 미해결, 의존 역전, GPU upload race). 나머지는 계속.

[빌드 검증]
- Engine: MSBuild Winters.sln /t:Engine /p:Configuration=Debug /p:Platform=x64
  (Engine/Public/World/*.h 추가 시 UpdateLib.bat / EngineSDK/inc 동기화 확인)
- Editor: /t:EldenRingEditor /p:Configuration=Debug-DX12 /p:Platform=x64
- Elden:  /t:EldenRingClient /p:Configuration=Debug-DX12 /p:Platform=x64
- LoL smoke(공용 Engine 변경): /t:Client /p:Configuration=Debug /p:Platform=x64
- git diff --check

[완료 기준 — WORLD_PARTITION]
- fake camera 이동에 따라 16 cell이 Unloaded↔Queued↔LoadingAssets↔LoadedHidden↔Visible 전이.
- Visible cell의 placeable=true wmesh가 viewport에 그려지고, transform 없는(placeable=false) ref는 draw call 제외.
- Asset Streaming/Budget 패널에 handle state(Queued/LoadingDisk/Ready/Failed) + GPU upload bytes/frame 표시.
- async 전환 후 fake camera 빠른 이동에도 frame hitch 없음.
- Server sim cell이 .wnav→CNavGrid 로드, spawnPoint는 GameSim 후보로만 전달(판정은 서버).
- normal F5 LoL flow(roster/map/minion/snapshot/champion) 정상.

[금지 사항]
- UE5 코드/타입 복사·이식. OFPA/spatial hash 최적화/HLOD 다단계 선반영(P3로 미룸).
- Client/Public·Shared에 DX11/DX12 concrete type 노출. Engine→Client 의존 역전.
- transform==0 reference를 억지로 placeable=true로 켜서 원점에 겹쳐 그리기.
- worker thread에서 GPU resource 생성(disk/parse만, 업로드는 main PumpGpuUploads).
- 에디터 전용 직접 mesh 로드로 CAssetStreamingSystem 우회. client cell에서 spawn/판정 생성.

[시작]
지금: (1) 위 문서 읽고, (2) G2/G3 선행 게이트 충족 여부와 현재 코드(CNavGrid/CRHIFxMeshResourceCache/
map_placement) 상태 집계 보고, (3) S0 grid math + S1 build-world-cells부터 시작. 막히면 사유 분류 보고하고
나머지는 계속 진행하라.
```
