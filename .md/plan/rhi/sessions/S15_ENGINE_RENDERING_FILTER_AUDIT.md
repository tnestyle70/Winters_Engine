# S15. Engine Rendering Filter Audit

작성일: 2026-05-28

목표: `Engine/Include/Engine.vcxproj.filters`가 만든 가상 폴더 착시를 걷어내고, 현재 렌더링/RHI 파일의 실사용 여부, 필터 재배치안, 삭제 후보를 먼저 고정한다. 이 문서는 코드 이동 전 기준표다.

## 원칙

- 이번 단계는 코드 이동이 아니다.
- `.vcxproj.filters`만 손으로 고치는 작업은 최종 상태가 아니다.
- CMake 생성 경로도 같이 보존하기 위해 `cmake/WintersEngine.cmake`의 `source_group` 기준을 함께 잡는다.
- `DX11*` 파일은 전부 삭제 대상이 아니라, 살아 있는 DX11 backend shim과 legacy renderer dependency를 먼저 분류한다.
- `Manager` 필터에는 실제 `Engine/Public/Manager`, `Engine/Private/Manager` 소유 파일만 남긴다.

## 현재 사실

실제 `Engine/Private/Manager`는 얇다.

```text
Engine/Private/Manager
  Navigation
    MapSurfaceSampler.cpp
    MapWalkableBaker.cpp
    NavGrid.cpp
    Pathfinder.cpp
  UI
    ChampionHUDPanel.cpp
    ChampionHUDPanel.h
    Font_Manager.cpp
    LuaUIHost.cpp
    UIAtlasManifest.cpp
    UI_Manager.cpp
  Profiler
    ProfilerOverlay.cpp
```

실제 `Engine/Public/Manager`도 같은 범위다.

```text
Engine/Public/Manager
  Navigation
    MapSurfaceSampler.h
    MapWalkableBaker.h
    NavGrid.h
    Pathfinder.h
  UI
    ChampionHUDState.h
    Font_Manager.h
    LuaUIHost.h
    UIAtlasManifest.h
    UI_Manager.h
  Profiler
    ProfilerOverlay.h
```

그런데 현재 `Engine.vcxproj.filters`는 실제 경로가 다른 파일을 `00. Manager` 아래에 넣고 있다. 예를 들면 `DX11Pipeline.cpp`는 실제로 `Engine/Private/RHI/DX11`에 있지만 필터에서는 `00. Manager/01. Pipeline`으로 보인다.

## 렌더링/RHI 실사용 파일 표

| 항목 | 실제 위치 | 현재 실사용 | 상태 | 목표 필터 |
|---|---|---|---|---|
| `CDX11Device` | `Engine/Private/RHI/DX11` | `CEngineApp`가 DX11 device/swapchain/backend로 생성 | Live | `02. RHI/01. DX11/Device` |
| `DX11Shader` | `Engine/Private/RHI/DX11` | `CEngineApp`, `NormalPass`, `FogOfWarRenderer`, `CubeRenderer`가 shader compile/bind에 사용 | Live, RHI 이관 대상 | `02. RHI/01. DX11/Shader` |
| `DX11Pipeline` | `Engine/Private/RHI/DX11` | `CEngineApp`, `ModelRenderer`, `PlaneRenderer`, `NormalPass`, `FxStaticMeshRenderer`, `CubeRenderer`가 input layout/raster/depth state로 사용 | Live, RHI 이관 대상 | `02. RHI/01. DX11/Pipeline` |
| `DX11Buffer` | `Engine/Private/RHI/DX11` | `CMesh`, `CubeRenderer` vertex/index buffer 경로 | Live | `02. RHI/01. DX11/Buffer` |
| `DX11ConstantBuffer` | `Engine/Private/RHI/DX11` | `ModelRenderer`, `CMaterialPBR`, `FxStaticMeshRenderer`, `CubeRenderer` constant buffer 경로 | Live | `02. RHI/01. DX11/Buffer` |
| `CBlendStateCache` | `Engine/Private/RHI/DX11` | `CEngineApp`, `PlaneRenderer`, `FxStaticMeshRenderer`, FX systems에서 alpha/additive blend state 사용 | Live | `02. RHI/01. DX11/StateCache` |
| `CSamplerStateCache` | `Engine/Private/RHI/DX11` | self reference만 확인됨. 호출자 없음 | Delete candidate | 삭제 또는 `02. RHI/01. DX11/StateCache` |
| Public RHI interfaces | `Engine/Public/RHI` | `IRHIDevice`, handle/descriptor contract | Live | `02. RHI/00. Interface` |
| `RHITextureLoader` | `Engine/Public/RHI`, `Engine/Private/RHI` | RHI texture handle 생성. `FxSystem`, `FxBeamSystem`, `RHIFxMeshResource`, utility plane texture에서 사용 | Live | `02. RHI/09. Utilities` |
| `CubeGeometry` | `Engine/Public/RHI/Geometry` | `CubeRenderer` CPU geometry source | Live if debug cube remains | `03. Renderer/Geometry` |
| `CubeRenderer` | `Engine/Public/Renderer`, `Engine/Private/Renderer` | debug/utility cube renderer | Live/optional | `03. Renderer/Debug` 또는 `03. Renderer/Geometry` |
| `ModelRenderer` | `Engine/Public/Renderer`, `Engine/Private/Renderer` | map/champion/minion/jungle/structure model draw path | Live | `03. Renderer/Model` |
| `NormalPass` | `Engine/Public/Renderer`, `Engine/Private/Renderer` | normal/depth prepass, SSAO input | Live | `03. Renderer/Passes/Normal` |
| `SSAOPass` | `Engine/Public/Renderer`, `Engine/Private/Renderer` | ambient occlusion pass | Live | `03. Renderer/Passes/SSAO` |
| `FogOfWarRenderer` | `Engine/Public/Renderer`, `Engine/Private/Renderer` | minimap/world overlay fog rendering | Live | `03. Renderer/Passes/FogOfWar` |
| `PlaneRenderer` | `Engine/Public/Renderer`, `Engine/Private/Renderer` | attack range, contact shadow, legacy FX plane path | Live | `03. Renderer/Plane` |
| `FxStaticMeshRenderer` | `Engine/Public/Renderer`, `Engine/Private/Renderer` | champion FX mesh path | Live | `03. Renderer/FX/StaticMesh` |
| `RHIFxSpriteRenderer` | `Engine/Public/Renderer`, `Engine/Private/Renderer` | RHI sprite/beam utility path | Live, RHI-forward | `03. Renderer/FX/RHI` |
| `RHIFxMeshResource` | `Engine/Public/Renderer`, `Engine/Private/Renderer` | RHI mesh resource cache, currently DX12-gated in initialize | Dormant/keep for RHI plan | `03. Renderer/FX/RHI` |
| `UIRenderer` | `Engine/Public/Renderer`, `Engine/Private/Renderer` | UI draw renderer | Live | `03. Renderer/UI` |
| `CMesh` | `Engine/Public/Resource`, `Engine/Private/Resource` | model submesh GPU buffers; currently uses `DX11Buffer` internally | Live, RHI 이관 대상 | `04. Resource/Mesh` |
| `CTexture` | `Engine/Public/Resource`, `Engine/Private/Resource` | texture load/bind; currently extracts native DX11 handles | Live, RHI 이관 대상 | `04. Resource/Texture` |
| `CModel` | `Engine/Public/Resource`, `Engine/Private/Resource` | cooked `.wmesh/.wmat/.wskel/.wanim` runtime model | Live | `04. Resource/Model` |

## 필터 재배치안

현재 `00. Manager`에서 빼야 할 것:

| 현재 필터 | 실제 의미 | 목표 필터 |
|---|---|---|
| `00. Manager/00. GraphicDev/00. RHI` | Public RHI interface | `02. RHI/00. Interface` |
| `00. Manager/00. GraphicDev/01. DX11` | DX11 backend device | `02. RHI/01. DX11/Device` |
| `00. Manager/01. Pipeline` | DX11 input layout/raster/depth state | `02. RHI/01. DX11/Pipeline` |
| `00. Manager/02. Shader/00. DX11` | DX11 HLSL compile/bind wrapper | `02. RHI/01. DX11/Shader` |
| `00. Manager/03. Buffer/DX11` | DX11 buffer and state cache | `02. RHI/01. DX11/Buffer` and `02. RHI/01. DX11/StateCache` |
| `00. Manager/04. ConstantBuffer` | DX11 constant buffer template | `02. RHI/01. DX11/Buffer` |
| `00. Manager/05. Geometry` | CPU geometry data | `03. Renderer/Geometry` |
| `00. Manager/06. Sound` | FMOD sound system | `06. Sound` |
| `00. Manager/09. Scene` | scene interface/manager | `05. Scene` |
| `00. Manager/11. Input` | platform input | `01. Core/Input` or `02. Platform/Input` |

`00. Manager`에 남길 것:

```text
00. Manager
  07. Navigation
    MapSurface
    NavGrid
    Pathfinder
  08. UI
    AtlasManifest
    HUD
    Font
    Lua
    UI_Manager
  10. Profiler
    ProfilerOverlay
```

## CMake source_group 기준

현재 `cmake/WintersEngine.cmake`는 다음처럼 전체 root 기준 source group을 만든다.

```cmake
source_group(TREE "${WINTERS_ROOT_DIR}" FILES
    ${WINTERS_IMGUI_SOURCES}
    ${WINTERS_LUA_SOURCES}
    ${WINTERS_ENGINE_PRIVATE_SOURCES}
    ${WINTERS_ENGINE_HEADERS}
)
```

다음 패치에서는 빌드 멤버십은 그대로 두고, source group만 더 명시적으로 나눈다.

```cmake
source_group(TREE "${WINTERS_ENGINE_DIR}/Public/RHI" PREFIX "02. RHI/Public" FILES ${WINTERS_ENGINE_RHI_PUBLIC_HEADERS})
source_group(TREE "${WINTERS_ENGINE_DIR}/Private/RHI" PREFIX "02. RHI/Private" FILES ${WINTERS_ENGINE_RHI_PRIVATE_FILES})
source_group(TREE "${WINTERS_ENGINE_DIR}/Public/Renderer" PREFIX "03. Renderer/Public" FILES ${WINTERS_ENGINE_RENDERER_PUBLIC_HEADERS})
source_group(TREE "${WINTERS_ENGINE_DIR}/Private/Renderer" PREFIX "03. Renderer/Private" FILES ${WINTERS_ENGINE_RENDERER_PRIVATE_FILES})
source_group(TREE "${WINTERS_ENGINE_DIR}/Public/Resource" PREFIX "04. Resource/Public" FILES ${WINTERS_ENGINE_RESOURCE_PUBLIC_HEADERS})
source_group(TREE "${WINTERS_ENGINE_DIR}/Private/Resource" PREFIX "04. Resource/Private" FILES ${WINTERS_ENGINE_RESOURCE_PRIVATE_FILES})
```

주의: 실제 파일 이동 없이 CMake 변수만 쪼개도 Visual Studio generated project의 구조 기준을 세울 수 있다.

## 삭제 후보

### 1. `CSamplerStateCache`

현재 확인 결과:

- 파일은 프로젝트에 포함됨.
- `Instance`, `Initialize`, `Get`, `BindAllPS` 구현이 있음.
- 외부 호출자가 보이지 않음.
- 실제 sampler binding은 `CTexture`, `UIRenderer`, `FogOfWarRenderer`, `SSAOPass` 등 각 renderer가 직접 처리한다.

판정: 1차 삭제 후보. 다음 패치에서 `rg "CSamplerStateCache|SamplerStateCache"` 재확인 후 `Engine.vcxproj`, `.filters`, CMake membership에서 제거한다.

### 2. 가짜 Manager 필터

코드 삭제 대상이 아니라 필터 삭제 대상이다.

- `00. Manager/GraphicDev`
- `00. Manager/Pipeline`
- `00. Manager/Shader`
- `00. Manager/Buffer`
- `00. Manager/ConstantBuffer`
- `00. Manager/Geometry`
- `00. Manager/Sound`
- `00. Manager/Scene`
- `00. Manager/Input`

판정: 소유권 착시를 만들기 때문에 필터 이동 대상.

### 3. `RHIFxMeshResource`

현재 `CRHIFxMeshResourceCache::Initialize`가 `eRHIBackend::DX12`를 요구한다. DX11 기본 런타임에서는 생성되지 않는 경로가 될 수 있다.

판정: 즉시 삭제 금지. RHI FX mesh 전환 계획에서 살릴지 결정한다.

### 4. `CubeRenderer` / `CubeGeometry`

현재 debug/utility cube 경로로 살아 있을 가능성이 있다.

판정: 즉시 삭제 금지. `Scene_InGame` debug cube 사용 여부 확인 후 별도 처리한다.

## 다음 실제 패치 순서

1. `cmake/WintersEngine.cmake`에 렌더/RHI/리소스/매니저 source group 변수를 추가한다.
2. `Engine/Include/Engine.vcxproj.filters`에서 `00. Manager` 가짜 항목을 목표 필터로 재배치한다.
3. 코드 파일 경로는 바꾸지 않는다.
4. `CSamplerStateCache`는 아직 삭제하지 않고, 필터 재배치 후 별도 삭제 패치로 분리한다.
5. `git diff --check`를 실행한다.
6. `cmake --build --preset engine-vs-debug`를 실행한다.

## 완료 기준

- Visual Studio `00. Manager`에는 실제 Manager 파일만 남는다.
- `DX11Pipeline`은 삭제되지 않고 `02. RHI/01. DX11/Pipeline`으로 보인다.
- CMake generated source group과 hand-maintained `.vcxproj.filters`의 방향이 같아진다.
- 삭제 후보와 live file이 섞이지 않는다.
- 다음 RHI 래핑 작업이 `DX11Shader/DX11Pipeline` 실사용 표를 기준으로 진행될 수 있다.
