# Session - 2026-06-24 Map Bush Placement via MapData

## 결론

부쉬는 NavGrid의 속성이 아니다. 부쉬의 본질은 `맵 위에 배치된 하나의 가시성/비주얼 영역`이다.

따라서 Winters의 목표 형태는 다음 하나로 수렴한다.

`Editor placement -> StageData BushEntry -> Client visual instance + BushVolumeComponent -> Vision/BushVolumeIndex`

현재 로컬 자산 기준으로는 `sru_brush.png`가 존재하고, 이름 기준 부쉬 FBX 후보는 찾지 못했다. 그러므로 1차 구현 기본값은 `PNG billboard`다. FBX 또는 cooked mesh가 생기면 같은 `BushEntry`의 `renderKind/assetPath`만 바꿔서 모델 배치로 확장한다.

## 본질 단위

| 단위 | 의미 | 소유자 |
| --- | --- | --- |
| BushEntry | 저장되는 원본 데이터. 위치, 반경, 표시 자산, 표시 크기만 가진다. | Shared MapData |
| Editor placement | 맵 좌표를 찍고 BushEntry를 만든다. | Client Editor |
| Visual instance | PNG면 빌보드, mesh면 모델로 보여준다. | Client Visual |
| BushVolumeComponent | 실제 부쉬 판정용 원형 볼륨이다. | Engine ECS |
| CBushVolumeIndex | 위치가 어떤 부쉬 안인지 빠르게 질의한다. | Engine ECS |
| Vision rule | 같은 부쉬/true sight가 아니면 부쉬 안 대상은 보이지 않는다. | Engine Vision, Server GameSim direction |

남길 것만 남기면 `BushEntry`가 유일한 원본이다. PNG, FBX, CSV, 하드코딩 seed는 모두 원본이 아니라 입력/표시/임시 도구다.

## 현재 코드 증거

- `Shared/GameSim/Definitions/MapDataFormats.h`
  - 현재 `STAGE_VERSION = 4`.
  - `StageHeader`, `StructureEntry`, `JungleEntry`, `MinionWaypointEntry`가 고정 크기 POD로 정의되어 있다.
- `Client/Private/Map/MapDataIO.cpp`
  - `Save_Stage()`는 `Structure -> Jungle -> MinionWaypoint` 순서로 저장한다.
  - `Load_Stage()`는 header version으로 호환성을 확인하고 v4 이상에서 MinionWaypoint를 읽는다.
- `Shared/GameSim/Definitions/StageData.h`
  - 서버/공유 경로에서 `.dat`를 읽기 위한 `StageData`가 있고, 현재 structures/jungles/minionWaypoints만 가진다.
- `Client/Private/Scene/Scene_Editor.cpp`
  - `AddMode`는 `Structure`, `Jungle`, `MinionWP`, `NavGrid`를 가진다.
  - `TryPickGroundPlane()`와 `Handle_MousePlacement()`가 이미 맵 좌표 클릭 배치의 공통 입구다.
  - NavGrid는 별도 `.navgrid` 파일로 저장되고, Stage `.dat`와 분리되어 있다.
- `Engine/Public/ECS/Components/VisionComponents.h`
  - `BushVolumeComponent { center, radius, bushId }`가 이미 존재한다.
- `Engine/Private/ECS/BushVolumeIndex.cpp`
  - `BushVolumeComponent`를 모아 `QueryBushAt(pos)`로 XZ 원형 판정을 수행한다.
- `Engine/Private/ECS/Systems/VisionSystem.cpp`
  - 대상이 부쉬 안이면 같은 부쉬 안의 시야 또는 true sight만 허용한다.
- `Client/Private/Scene/Scene_InGameLifecycle.cpp`
  - 현재 InGame은 `.wbrush`, CSV, fallback 하드코딩 순서로 부쉬 볼륨을 임시 seed한다.
  - 이 경로는 최종 구조에서 `StageData::bushes` 로 대체되어야 한다.
- `Client/Bin/Resource/Texture/MAP/output/textures/assets/maps/kitpieces/srx/textures/sru_brush.png`
  - 현재 로컬에 존재하는 부쉬 시각 자산 후보.
- `Client/Bin/Resource/Texture/MAP/Map11_Rebuild/manifest/map11_brush_volumes.csv`
  - 현재 64개 brush volume 후보를 가진 임시/import 입력 자료.

## 목표 데이터 형태

Stage `.dat` v5에 `BushEntry` 블록을 추가한다.

필수 필드만 둔다.

- `u32_t bushId`
- `f32_t px, py, pz`
- `f32_t yaw`
- `f32_t radius`
- `f32_t width, height`
- `f32_t scale`
- `u32_t renderKind`
- `char assetPath[128]`
- `u32_t bVisible`
- `u32_t reserved[...]`

원칙:

- 위치와 반경은 gameplay/vision의 진실이다.
- width/height/scale/assetPath는 visual의 입력일 뿐이다.
- NavGrid에는 부쉬 정보를 섞지 않는다.
- PNG와 FBX는 같은 BushEntry로 표현한다.
- CSV와 `.wbrush`는 런타임 원본이 아니라 import/마이그레이션 도구로만 남긴다.

## 구현 방향

### 1. Shared MapData

- `Shared/GameSim/Definitions/MapDataFormats.h`
  - `STAGE_VERSION`을 5로 올린다.
  - `BushEntry`를 추가한다.
  - `static_assert(sizeof(BushEntry) == ...)`로 바이너리 크기를 고정한다.
- `Shared/GameSim/Definitions/StageData.h`
  - `std::vector<BushEntry> bushes`를 추가한다.
  - v5 이상에서만 bush block을 읽고, v4 이하는 빈 bushes로 둔다.

### 2. Client Stage I/O

- `Client/Private/Map/MapDataIO.cpp`
  - 저장 순서를 `Structure -> Jungle -> MinionWaypoint -> Bush`로 확장한다.
  - 로드는 v5 이상에서만 Bush를 읽는다.
  - 기존 Stage1.dat v4는 깨지지 않게 유지한다.

### 3. Editor Placement

- `Client/Public/Scene/Scene_Editor.h`
  - `eCategory::Bush`, `eAddMode::Bush`를 추가한다.
  - pending 값은 asset path, render kind, radius, width, height, scale만 둔다.
- `Client/Private/Scene/Scene_Editor.cpp`
  - Palette에 Bush 모드를 추가한다.
  - 클릭하면 `TryPickGroundPlane()` 결과로 BushEntry를 추가한다.
  - Hierarchy에 Bush 목록을 표시한다.
  - Inspector에서 위치, yaw, radius, size, scale, visible, assetPath를 수정한다.
  - Save/Load는 Stage `.dat`에만 묶고, NavGrid 저장과 섞지 않는다.

### 4. Client Runtime Visual

- InGame 로딩 시 `StageData::bushes`를 읽어 visual instance를 만든다.
- `renderKind == Billboard`면 PNG billboard를 사용한다.
- `renderKind == Mesh`면 cooked mesh/model renderer를 사용한다.
- visual 생성 실패는 vision 생성 실패가 아니다. 자산이 없어도 BushVolume은 생성되어야 디버깅이 가능하다.

### 5. Vision/Bush Runtime

- InGame 로딩 시 각 BushEntry마다 `BushVolumeComponent`를 가진 ECS entity를 만든다.
- `m_BushIndex.Build(m_World)`는 이 entity들을 기준으로 수행한다.
- 최종적으로 `SeedMap11BrushesFromBinaryForBootstrap`, `SeedMap11BrushesFromResourceForBootstrap`, `SeedPracticeBushesForBootstrap`는 normal F5 path에서 제거한다.
- 단, CSV import가 필요하면 editor/import tool로 옮긴다.

### 6. Server Authority

부쉬가 시야/은신 판정에 영향을 주는 순간, 서버도 같은 `StageData::bushes`를 읽어야 한다.

서버 방향:

- `LoadServerStageData()`가 v5 bushes를 읽는다.
- Server GameSim에 BushVolume 판정 또는 Shared 쪽 lightweight bush query를 둔다.
- Client는 시각화와 약한 예측만 한다.
- 최종 권위 흐름은 `Client Input -> Server GameSim/Vision -> Snapshot/Event -> Client Visual`이다.

## 협업 분리

| 역할 | 편집 대상 | 책임 |
| --- | --- | --- |
| 기획 | StageData 값, radius, bushId, gameplay 의도 | 어느 위치가 어떤 부쉬인지 결정 |
| 디자인 | PNG/FBX/cooked mesh, billboard 크기, 색감 | 어떻게 보이는지 결정 |
| 개발 | 저장 포맷, 에디터, 런타임 로딩, vision 연결 | 같은 데이터를 안정적으로 저장/재생 |

기획자와 디자이너가 만지는 값은 Stage `.dat`와 asset path다. 개발자가 만지는 것은 그 값을 읽는 파이프라인이다.

## 검증 파이프라인

### 문서/자산 검증

- `Test-Path Client/Bin/Resource/Texture/MAP/output/textures/assets/maps/kitpieces/srx/textures/sru_brush.png`
- `Test-Path Client/Bin/Resource/Texture/MAP/Map11_Rebuild/manifest/map11_brush_volumes.csv`
- `Get-ChildItem Client/Bin/Resource -Recurse -File | ? Extension -eq .fbx | ? Name -match 'brush|bush|grass|foliage|shrub'`
- `git diff --check -- .md/plan/2026-06-24_MAP_BUSH_PLACEMENT_MAPDATA_PLAN.md`

### 구현 후 저장 검증

- Editor에서 Bush 모드 진입.
- PNG billboard 기본 asset으로 3개 배치.
- Save Stage.
- Editor 재진입 후 3개가 같은 위치/radius/assetPath로 복원되는지 확인.
- Stage v4 파일을 로드했을 때 bushes가 빈 배열로 처리되는지 확인.

### 구현 후 런타임 검증

- InGame 진입 시 StageData bush count를 Debug log로 1회 출력한다.
- 자산 누락 상태에서도 BushVolume은 생성되는지 확인한다.
- 캐릭터가 radius 안팎으로 이동할 때 `VisibilityComponent::bInBush`와 `bushId`가 변하는지 확인한다.
- 같은 부쉬 안 대상은 보이고, 다른 위치의 대상은 true sight 없이는 보이지 않는지 확인한다.

### 구현 후 빌드 검증

- Client Debug x64 build
- Server Debug x64 build
- 가능하면 F5 normal path smoke 또는 기존 harness smoke

## 단계 계획

1. StageData v5
   - `BushEntry` 추가, shared loader 확장, v4 호환 확인.
2. Bush data owner
   - 최소 구현은 Manager를 새로 만들지 않고 StageData vector를 로드한다.
   - 에디터 선택/삭제/저장이 반복되며 복잡해지는 시점에만 `CBush_Manager`를 추가한다.
3. Editor Bush mode
   - NavGrid의 ground pick 흐름을 재사용한다.
   - 배치, 선택, inspector 수정, 저장/로드만 구현한다.
4. Runtime bridge
   - StageData bushes로 visual instance와 BushVolumeComponent를 만든다.
   - 기존 `.wbrush`/CSV/hardcoded seed를 fallback이 아닌 import path로 내린다.
5. Server/GameSim sync
   - 서버가 같은 BushEntry를 읽고 시야/은신의 권위를 갖게 한다.
6. Cleanup
   - normal F5 path에 남은 임시 brush seed를 제거한다.
   - Map11 CSV는 editor import seed 또는 migration tool로만 보관한다.

## 하지 않을 것

- NavGrid cell flag에 부쉬를 끼워 넣지 않는다.
- PNG/FBX 종류별로 서로 다른 gameplay 데이터를 만들지 않는다.
- 런타임에서 CSV를 계속 읽는 구조를 최종 구조로 두지 않는다.
- 비주얼 자산 실패를 gameplay volume 실패로 연결하지 않는다.
- 서버 권위가 필요한 시야 판정을 Client-only로 확정하지 않는다.

## 핸드오프 메모

다음 작업자는 먼저 `BushEntry`를 추가하고 v4 Stage 파일이 깨지지 않는지 확인해야 한다. 그다음 Editor의 `AddMode::Bush`를 붙이면 된다. 현재 이미 존재하는 `BushVolumeComponent`, `CBushVolumeIndex`, `VisionSystem`은 재사용 대상이며, 새 판정 시스템을 만들 필요가 없다.
