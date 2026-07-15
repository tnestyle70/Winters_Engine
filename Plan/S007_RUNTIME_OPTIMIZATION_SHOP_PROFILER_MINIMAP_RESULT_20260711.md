Session - 첫 미니언 스폰 hitch와 정상 F5 중복 렌더를 줄이고, 454개 아이템 카탈로그·Profiler 저장·비율 기반 미니맵을 빌드 가능한 상태로 반영한 결과를 고정한다.

## 1. 결론

- Engine, Client, Server Debug x64는 모두 compile/link에 성공했다.
- Claude가 작업 중인 이력서·면접 문서와 다른 dirty 파일은 수정 범위에서 제외하고 보존했다.
- 첫 미니언 스폰 hitch의 코드상 직접 후보였던 다음 경로를 제거했다.
  - 첫 network visual bind의 `run` 재생과 base animation owner의 `run` 재생이 겹치던 경로.
  - 모든 animation 성공마다 실행되던 동기 `OutputDebugStringA`.
  - 첫 두 role만 예열하여 siege/super/Tibbers가 cold-create로 빠지던 pool 구성.
  - prewarm renderer가 실제 texture override를 갖지 않던 불완전 예열.
  - 첫 bind 시 container rehash/reallocation 가능성.
- 정상 F5에서 실험용 RHI scene snapshot과 legacy visible renderer가 동시에 map을 제출하던 중복 경로를 끊었다. RHI snapshot은 이제 `--rhi-scene-only` parity lab에서만 실행된다.
- Resource의 item PNG 454개를 recursive scan하고 상대 경로를 유일한 `assetKey`로 사용한다. 화면 슬롯 추정 generator가 같은 icon을 여러 번 고르던 기존 Lua 카탈로그는 폐기했다.
- 454개 icon은 `1024×2048` RGBA atlas 한 장과 454개 sprite로 묶었다. 정상 Lua shop은 454개 WIC texture/SRV를 개별 사전 로드하지 않는다.
- F8 `Item Shop Catalog`에서 검색, enabled, order, section, 비등록 icon의 preview price를 저장·적용·재로드할 수 있다. 서버에 등록된 15개 item은 `CItemRegistry` 가격을 read-only로 사용하고 각 ID의 primary 한 개만 구매 가능하다.
- Profiler Details에서 두 개의 `Save`가 같은 ImGui ID로 충돌하던 문제를 제거했다. F4의 archive/root 저장도 한 번 capture하고 같은 cached frame을 두 경로에 기록하도록 바꿨다.
- 미니맵은 고정 `252px` 대신 화면 높이의 기본 `35%`를 사용한다. F8 `Minimap Layout`에서 비율, 오른쪽/아래 여백, 전체 icon scale, champion scale을 조절한다. 렌더와 클릭 판정은 같은 rect 계산을 사용한다.
- minion/structure/jungle/ward icon은 quad로 단순화했고, 시야에 보이는 champion의 circular portrait는 별도 마지막 pass에서 그려 다른 icon에 가려지지 않는다. 17개 roster portrait는 scene 진입 때 prewarm한다.
- 300FPS 달성은 아직 확정하지 않았다. 기존 JSON은 S007 이전 캡처이고, 새로운 정상 F5 uncapped 캡처가 없으므로 3.333ms 통과를 주장할 수 없다.

## 2. 문제와 원인 근거

### 2-1. 첫 미니언 스폰 hitch

- 첫 wave는 `3 melee + 3 ranged × 3 lane × 2 team = 36`개다.
- 기존 pool도 melee/ranged 9개씩 두 팀, 총 36개였지만 다음 비용이 bind frame에 남아 있었다.
  - `Ensure_NetworkVisual`이 `run`을 먼저 시작한 뒤 `UpdateMinionVisual`이 invalid base state를 보고 다시 `run` 시작.
  - `ModelRenderer::PlayAnimationByNameAdvanced`가 성공할 때마다 Debug Output 호출.
  - 별도 network bind 성공 trace.
  - siege/super/Tibbers role renderer 미예열.
  - prewarm path와 cold path의 texture binding 불일치.
- 최신 pre-S007 stable row에는 `MinionVisual::ColdCreate` 3회, 합계 `0.7542ms`, 최대 `0.3698ms`가 남아 있었다.
- 역할별 warm capacity를 team당 `9/9/3/3/1`로 바꾸고, 72개 concurrent high-water를 reserve했다. 첫 정상 wave의 기대값은 `PoolHit=36`, `ColdCreate=0`이다.

### 2-2. 정상 F5 중복 map 제출

- 최신 pre-S007 캡처:
  - Frame `13.1878ms`
  - Update `3.7601ms`
  - Render `9.1741ms`
  - GPU `8.704ms`
  - `Map::RHISceneSnapshot=4.8899ms`, snapshot mesh `1080`
  - legacy `Mesh::DrawCalls=462`
  - `RHISceneOnly=0`
- `RHISceneOnly=0`인데 RHI snapshot mesh 1080개와 legacy map draw가 함께 존재하므로 정상 F5가 두 visible mesh submission family를 모두 실행한 캡처다.
- 최근 7개 JSON에서 제거 대상 `Map/SceneObjects/Champion::RHISceneSnapshot` scope 합은 `4.8135~5.7692ms`, 평균 `5.1119ms`였다.
- 최신 값을 단순 차감하면 `13.1878 - 5.3386 = 7.8492ms`다. 큰 절감 후보이지만 300FPS budget `3.333ms`보다 여전히 약 `4.516ms` 크다.
- RHI map 1080회와 legacy mesh 462회 두 계열만 비교하면 이번 gate는 `1080 / (1080 + 462) = 70.0%`를 제거한다. UI, FX, fullscreen pass를 포함한 전체 draw 비율이나 GPU ms 절감량으로 해석하면 안 된다.

### 2-3. 상점 중복과 누락

- 이전 Lua catalog는 57행이지만 unique icon path는 50개였다.
- 실제 duplicate group은 `3854×3`, `3113×3`, `2012×2`, `3742×2`, `3803×2`였다. 첨부 화면의 동일한 3200 icon 3개는 `3113` 세 행이다.
- screenshot slot matcher가 각 슬롯을 독립적으로 고르고 icon path의 one-to-one 제약을 두지 않은 것이 원인이다.
- Resource `Texture/UI/Items/**/*.png`는 454개이고 RGBA pixel duplicate는 0개다. 즉 source image가 중복된 것이 아니라 catalog assignment가 중복됐다.
- 현재 runtime catalog는 Resource를 직접 recursive scan한다. 같은 numeric prefix의 다른 이미지와 비숫자 파일도 상대 경로 `assetKey`로 구분한다.
- 실제 구매 가격은 Server/Shared의 `CItemRegistry`가 결정하므로 Client JSON 가격을 구매 가능 item에 임의 적용하면 표시와 차감액이 달라진다. 이 때문에 등록 15개 가격은 editor에서 잠그고, 비등록 image에만 preview price를 허용했다.

### 2-4. Profiler Save

- `DrawCompactHeader()`의 `SmallButton("Save")`와 Details `Draw_ControlBar()`의 `Button("Save")`가 같은 `Profiler` window에 동시에 나타나 동일 ID를 만들었다.
- compact header Save만 남겨 visible ID를 하나로 만들었다.
- F4는 timestamp archive와 root `profiler.json`을 연속 force-capture하여 같은 raw frame으로 StableView sample/EMA를 두 번 갱신했다. 첫 저장만 force capture하고 두 번째는 cached display frame을 쓰도록 수정했다.

### 2-5. 미니맵 크기와 portrait

- 기존 `fSize=252`는 720p의 35%지만 1080p에서는 23.3%, 1440p에서는 17.5%가 되어 해상도가 커질수록 작아졌다.
- world projection 보정과 panel pixel 크기는 별도 문제다. 이번 변경은 projection을 다시 흔들지 않고 panel rect와 icon scale만 viewport ratio로 계산한다.
- champion circular portrait 경로는 이미 존재했지만 기본 반지름 5.5px, ECS 순회 순서, render-frame lazy load 때문에 작거나 다른 icon에 가려질 수 있었다.
- portrait를 마지막 pass로 분리하고 panel ratio에 따라 확대하며 loading/scene initialization에서 prewarm한다. 실패한 null texture는 cache하지 않는다.

## 3. 실제 반영 구조

### 3-1. Minion/Render

- `Engine/Private/Renderer/ModelRenderer.cpp`
  - animation 성공 trace 제거, bounded missing-animation trace 유지.
- `Client/Private/Manager/Minion_Manager.cpp`
  - role pool `9/9/3/3/1`, texture prebind, container reserve, 중복 `run`과 성공 bind log 제거.
- `Client/Private/Scene/Scene_InGame.cpp`
  - 즉시 연속이던 actor surface projection 한 번 제거.
- `Client/Private/Scene/Scene_InGameRender.cpp`
  - map/champion/scene-object RHI snapshot을 `bRHISceneOnly`로 gate.

### 3-2. Shop catalog/atlas

- `Data/LoL/ClientPublic/UI/ItemShopCatalog.json`
  - tracked authoring override source. 최초 빈 array는 Resource scan 기본값을 뜻하며 F8 Save 때 deterministic 454행으로 확장된다.
- `Tools/build_item_shop_atlas.py`
  - 454 icon을 atlas와 `item:` sprite manifest로 재생성하는 tracked tool.
- `Client/Private/GamePlay/LoLUIContentRegistry.cpp`
  - scan, JSON merge, authoritative price/primary mapping, F8 editor.
- `Engine/Public/Manager/UI/ActorHUDAssets.h`
- `Engine/Private/Manager/UI/UI_Manager.cpp`
- `Engine/Private/Manager/UI/LuaUIHost.cpp`
  - DTO owned copy와 `UI.GetShopItems()` 연결.
- runtime generated/ignored asset:
  - `Client/Bin/Resource/Texture/UI/item_icons_atlas.png`
  - `Client/Bin/Resource/UI/itemshop_atlas_manifest.json`
  - `Client/Bin/Resource/UI/Lua/itemshop_catalog.lua`
  - `Client/Bin/Resource/UI/Lua/ui_boot.lua`

### 3-3. Profiler/Minimap

- `Engine/Private/Manager/Profiler/ProfilerOverlay.cpp`
  - duplicate Save 제거.
- `Engine/Private/Framework/CEngineApp.cpp`
- `Engine/Private/GameInstance.cpp`
  - capture once/write twice.
- `Client/Private/UI/MinimapPanel.cpp`
  - ratio rect, tuner, quad icon, portrait last pass/prewarm/counters.
- `Client/Private/Scene/Scene_InGameImGui.cpp`
  - F8에서 `Minimap Layout`, `Item Shop Catalog` 창 진입.
- `Client/Private/Scene/Scene_InGameLifecycle.cpp`
  - portrait prewarm.

## 4. 자동 검증 결과

- 관련 파일 `git diff --check`: PASS. 기존 LF/CRLF 변환 안내만 존재.
- `UI_Manager.cpp`에 있던 legacy invalid UTF-8 주석/문자열: strict UTF-8 decode PASS. 로직은 바꾸지 않고 깨진 주석을 ASCII로 최소 정규화했다.
- item atlas tool idempotent 실행: PASS.

```text
item atlas: icons=454 uniqueAssetKeys=454
```

- item 정적 검증:

```text
AssetFiles=454
UniqueAssetKeys=454
AtlasSprites=454
UniqueAtlasSprites=454
MissingSprites=0
OrphanSprites=0
AtlasSize=1024x2048 RGBA
ServerItemDefs=15
IdsWithPrimaryCandidate=15
```

- 최신 저장 JSON parse: PASS.
  - `C:/Users/user/Desktop/Winters/profiler.json`
  - `C:/Users/user/Desktop/Winters/Profiles/profiler_20260711_210619.json`
  - 두 파일은 2026-07-11 21:06:19 캡처이며 S007 이전 데이터다.
- Engine Debug x64 build/link: PASS.
  - `Engine/Bin/Debug/WintersEngine.dll`, 2026-07-11 22:12:48.
- Client Debug x64 build/link: PASS.
  - `Client/Bin/Debug/WintersGame.exe`, 2026-07-11 22:13:40.
- Server Debug x64 build/link: PASS.
  - `Server/Bin/Debug/WintersServer.exe`, 2026-07-11 22:14:16.
- Shared boundary check와 FlatBuffers codegen: PASS.
- 남은 build warning은 기존 DLL interface/export 계열 `C4251/C4275`다. 이번 S007 변경에서 compile/link error는 0건이다.

## 5. 사용자 인게임 검증 절차

이번 세션에서는 사용자가 직접 인게임을 확인하는 방향이므로 Client/Server 창을 자동 실행하지 않았다.

### 5-1. 첫 wave hitch

1. 새 Server와 새 Client로 정상 F5 게임을 시작한다.
2. 첫 wave 직전 F4로 한 번 저장하고, 첫 36개 미니언 visual이 모두 bind된 직후 F4로 다시 저장한다.
3. 기대 결과:
   - 첫 wave `MinionVisual::PoolHit=36`.
   - `MinionVisual::ColdCreate=0`.
   - `MinionVisual::Created`는 frame bind budget 때문에 여러 frame에 나뉘어도 정상이다.
   - Output 창에 animation 성공 로그와 network visual 성공 batch 로그가 쏟아지지 않는다.
   - 첫 spawn 순간의 눈에 띄는 stall이 줄어든다.

### 5-2. Shop

1. `P`로 상점을 연다.
2. 454개 Resource icon이 scroll 영역에 중복 asset row 없이 나타나는지 확인한다.
3. F8을 눌러 `Item Shop Catalog` 창에서 검색한다.
4. 비등록 icon의 order, section, preview price, enabled를 바꾸고 `Save + Apply`를 누른다.
5. 기대 결과:
   - Lua shop이 즉시 reload되어 새 순서/표시값을 사용한다.
   - 저장 파일은 `Data/LoL/ClientPublic/UI/ItemShopCatalog.json`이다.
   - 등록 15개 가격은 `(server)`로 read-only다.
   - `buy` 행만 우클릭 구매를 서버로 보낸다.
   - 새 item PNG를 Resource에 추가한 뒤에는 `py -3 Tools/build_item_shop_atlas.py`를 다시 실행해야 atlas sprite가 생긴다.

### 5-3. Profiler

1. F3으로 Profiler를 열고 `Details`를 누른다.
2. 상단 `Save`가 한 개만 보이고 Dear ImGui conflicting ID popup이 뜨지 않는지 확인한다.
3. Save 또는 F4 뒤 `profiler.json` parse와 최신 timestamp를 확인한다.
4. F4 archive/root JSON의 raw frame과 StableView sample/EMA가 동일한지 확인한다.

### 5-4. Minimap/portrait

1. F8 `Minimap Layout`에서 `Viewport Height Ratio`를 바꾼다.
2. 패널 크기와 icon 크기가 함께 바뀌는지 확인한다.
3. 미니맵 클릭 이동이 보이는 패널 rect와 정확히 일치하는지 확인한다.
4. 시야에 들어온 아군/적 champion의 원형 portrait가 구조물·미니언 위에 그려지는지 확인한다.
5. 기대 counter:
   - `Minimap::ChampionPortraitCount`는 현재 보이는 champion portrait 수.
   - `Minimap::PortraitLoadFailure=0`.

### 5-5. 300FPS 재캡처

정상 F5와 같은 roster/map/minion/champion/UI/FX를 유지하고 아래 인자로 실행한다.

```text
Client/Bin/Debug/WintersGame.exe --uncapped --no-vsync --rhi=dx11
```

- `--rhi-scene-only`는 사용하지 않는다.
- profiler overlay를 닫고 동일 해상도·카메라·게임 시간·minion 수에서 측정한다.
- 확인값:
  - `Frame::LimiterActive=0`
  - `RHISceneOnly=0`
  - `Map::RHISceneSnapshot`과 관련 mesh counter가 없거나 0
  - 첫 wave 뒤 `MinionVisual::ColdCreate=0`
- 최소 30개 같은 조건 capture에서 CPU Frame과 GPU Frame의 median/P95를 비교한다.
- CPU와 GPU가 모두 `3.333ms` 이하일 때만 300FPS 달성으로 판정한다.

## 6. 300FPS까지 남은 실제 우선순위

1. post-S007 uncapped 캡처를 먼저 확보한다. 이번 큰 중복 제거 뒤 병목 순위가 바뀌므로 fresh evidence 없이 다음 대형 renderer를 추가하지 않는다.
2. Profiler hot path를 thread-local accumulate 후 EndFrame merge로 바꾼다.
   - 기존 한 frame 약 492 scope call, draw counter 최소 924회가 global mutex와 선형 검색을 사용한다.
   - 검증 counter: `Profiler::ScopeCalls`, `Profiler::CounterCalls`, `Profiler::MergeUs`, `Profiler::DroppedRawEvents`.
3. 기존 combined map renderer 내부에 spatial cluster metadata와 material atlas/array를 추가한다.
   - 현재 legacy map은 `CombinedDrawCalls=395`, `MaterialBinds=434`, submitted index 약 218만이다.
   - 새 renderer를 병렬 유지하지 않고 기존 buffer path를 강화한다.
4. frame CB 1회 갱신, cull 뒤 object CB upload, dirty bone palette를 먼저 적용한 후 skinned minion direct instancing을 구현한다.
   - 현재 DX11 API 자리는 있지만 `RHISceneRenderer`는 instanceCount 1, shader는 `SV_InstanceID`/palette offset이 없어 실제 미니언 GPU instancing은 구현되지 않았다.
5. champion/jungle/ambient animation cadence와 snapshot/surface projection을 visibility/dirty actor 기준으로 줄인다.
   - 기존 animation 관련 최신 합은 약 1.33ms, snapshot 있는 frame 추가 비용은 평균 약 1.43ms다.

이번 결과는 첫 번째 큰 중복 경로와 명확한 spawn/UI 비용을 제거한 단계다. 300FPS를 숫자로 확정하는 다음 입력은 사용자의 post-S007 정상 F5 uncapped JSON이다.

