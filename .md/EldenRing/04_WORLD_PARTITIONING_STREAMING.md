# World Partitioning And Streaming

## 목표

EldenRing 클라이언트는 큰 필드/던전/레이드 아레나를 다룬다. 모든 오브젝트와 리소스를 한 번에 로드하는 구조는 초기에 편해도 포트폴리오 설득력이 약하다.

목표:

```
World = grid cells + data layers + streaming sources + async asset loader
```

Unreal World Partition과 비교했을 때 Winters가 직접 보여줘야 하는 핵심 영역이다.

## 기본 개념

| 개념 | 설명 |
|---|---|
| World | 전체 플레이 공간 |
| Cell | 고정 크기 grid로 나눈 streaming 단위 |
| Actor/Entity | ECS entity 또는 spawn descriptor |
| DataLayer | gameplay/editor/quest/raid phase 별 enable layer |
| StreamingSource | player/camera/raid interest 등 로딩 기준점 |
| HLOD | 먼 거리 셀을 저비용 proxy로 표시 |
| RuntimeHash | 위치 -> cell id 빠른 조회 |

## 디렉토리 구조

```
WintersElden/Bin/Resource/World/
├── FieldTest/
│   ├── FieldTest.world.json
│   ├── Cells/
│   │   ├── x000_z000.wcell
│   │   ├── x001_z000.wcell
│   │   └── ...
│   ├── HLOD/
│   │   ├── x000_z000_hlod.wmesh
│   │   └── ...
│   └── Nav/
│       ├── x000_z000.wnav
│       └── ...
└── RaidArena01/
```

초기에는 JSON metadata를 써도 된다. 런타임 안정화 후 `.wmap/.wcell` binary로 승격한다.

## World Descriptor

```json
{
  "name": "FieldTest",
  "cellSizeMeters": 64.0,
  "origin": [0.0, 0.0, 0.0],
  "cells": [
    {
      "id": "x000_z000",
      "bounds": [0.0, 0.0, 0.0, 64.0, 64.0, 64.0],
      "file": "Cells/x000_z000.wcell",
      "dataLayers": ["Base", "Gameplay"],
      "hlod": "HLOD/x000_z000_hlod.wmesh"
    }
  ]
}
```

## Cell Descriptor

```json
{
  "id": "x000_z000",
  "staticMeshes": [
    {
      "asset": "Map/FieldTest/Rock01.wmesh",
      "material": "Map/FieldTest/Rock01.wmat",
      "transform": {
        "position": [12.0, 0.0, 5.0],
        "rotation": [0.0, 45.0, 0.0],
        "scale": [1.0, 1.0, 1.0]
      }
    }
  ],
  "spawnPoints": [
    {
      "type": "Enemy",
      "archetype": "Soldier01",
      "position": [20.0, 0.0, 12.0]
    }
  ],
  "nav": "Nav/x000_z000.wnav"
}
```

## Runtime 시스템

```
CWorldPartitionSystem
├── CWorldDescriptor
├── CWorldCellRuntime
├── CStreamingSourceRegistry
├── CDataLayerSystem
├── CHLODSystem
├── CCellVisibilityResolver
└── CCellStreamingScheduler
```

## Streaming Source

플레이어, 카메라, 레이드 타겟이 로딩 기준점이 된다.

```cpp
struct StreamingSourceComponent
{
    Vec3 position;
    f32_t loadRadius = 160.f;
    f32_t visibleRadius = 220.f;
    f32_t unloadRadius = 280.f;
    u32_t priority = 0;
};
```

여러 source가 있을 때는 union으로 필요한 cell set을 계산한다.

예:

| source | 역할 |
|---|---|
| Player | 주변 gameplay cell 로드 |
| Camera | 시야 방향 prefetch |
| BossArena | 레이드 중 보스 아레나 고정 유지 |
| EditorCursor | 에디터에서 선택 영역 로드 |

## Cell 상태

```cpp
enum class eWorldCellState : uint8_t
{
    Unloaded,
    Queued,
    LoadingMetadata,
    LoadingAssets,
    CreatingEntities,
    LoadedHidden,
    Visible,
    Unloading
};
```

상태 전이:

```
Unloaded
  -> Queued
  -> LoadingMetadata
  -> LoadingAssets
  -> CreatingEntities
  -> LoadedHidden
  -> Visible
  -> Unloading
  -> Unloaded
```

## Cell 로드 단계

```
1. Read .wcell metadata
2. Collect asset dependencies
3. Async load meshes/textures/materials/nav
4. GPU upload
5. Instantiate ECS entities
6. Register collision/nav/hit volumes
7. Mark visible
```

## DataLayer

DataLayer는 같은 cell 안에서도 상황별 entity를 켜고 끄는 장치다.

예:

| DataLayer | 용도 |
|---|---|
| Base | 지형/기본 static mesh |
| Gameplay | 적/상호작용 오브젝트 |
| RaidPhase1 | 보스 1페이즈 오브젝트 |
| RaidPhase2 | 보스 2페이즈 오브젝트 |
| EditorOnly | 에디터 helper |
| Cinematic | 컷신 중만 표시 |

```cpp
struct DataLayerComponent
{
    u64_t layerMask = 0;
};
```

## HLOD 전략

초기에는 매우 단순하게 간다.

| 거리 | 표시 |
|---|---|
| near | 실제 cell entity |
| mid | 실제 static mesh, dynamic 생략 가능 |
| far | HLOD proxy mesh |
| very far | culled |

초기 구현:

1. HLOD proxy는 수동 `.wmesh` 하나
2. cell unload 전에 far proxy 표시
3. 실제 cell visible 시 proxy 숨김

## Editor 연동

World Partition Editor가 필요하다.

기능:

1. grid overlay 표시
2. 현재 로드된 cell 색상 표시
3. cell 강제 load/unload
4. selected cell asset dependency 표시
5. DataLayer toggle
6. streaming source radius 시각화
7. cell descriptor 저장

ImGui 패널:

```
World Partition
├── Current World
├── Streaming Sources
├── Loaded Cells
├── Data Layers
├── Cell Inspector
└── Budget
```

## 네트워크와 World Partition

서버도 world partition 개념을 가진다. 단, 서버는 렌더링 에셋이 아니라 시뮬레이션 cell을 관리한다.

```
Client World Cell
  - render mesh
  - material
  - FX
  - UI helper

Server Sim Cell
  - collision
  - nav
  - spawn table
  - entity authority
```

서버 관심 영역:

1. 플레이어 주변 sim cell active
2. raid arena는 full active
3. 멀리 있는 field cell은 AI sleep 또는 coarse sim

## 파일 포맷 승격

초기:

```
world.json + cell.json
```

중기:

```
.wmap + .wcell + .wnav
```

후기:

```
Content.winters bundle
```

## 구현 순서

| 단계 | 내용 | 완료 기준 |
|---|---|---|
| WP0 | grid math | world position -> cell coord |
| WP1 | source radius | player 주변 cell set 계산 |
| WP2 | JSON cell load | metadata 읽기 |
| WP3 | asset dependency list | mesh/material/texture 목록 수집 |
| WP4 | sync load baseline | cell 1개 표시 |
| WP5 | async loader 연결 | stall 없이 load |
| WP6 | unload | cell entity 제거 |
| WP7 | editor panel | load 상태 시각화 |
| WP8 | HLOD | far proxy 표시 |
| WP9 | `.wcell` binary | JSON 대체 |
