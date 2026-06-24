# Session - 2026-06-24 Effect Tool UE-Style Editor Guide

목표: 현재 Winters의 `EffectTuner`와 `WFX Effect Tool` 상태를 코드 기준으로 정리하고, EldenRing/UE Niagara급으로 가기 위한 설계 원칙과 단계별 개선 방향을 남긴다.

## 0. 결론

새 FX 시스템을 옆에 하나 더 만들면 안 된다. 현재 가장 살아 있는 축은 `CWfxEffectToolPanel` + `.wfx` + `CFxCuePlayer`이며, 여기에 `EffectTuner`의 즉석 튜닝 능력과 Elden editor shell의 패널 구조를 흡수하는 방향이 맞다.

최종 형태는 다음 구조다.

```text
Content Browser
  -> .wfx asset selection
  -> dependency check / thumbnail / tags
WFX Editor
  -> System / Emitter stack
  -> Module graph metadata
  -> Details / curve / timeline
Preview
  -> same CFxCuePlayer runtime path
  -> isolated editor preview or in-game anchored preview
Cook / Hot Reload
  -> .wfx authoring data
  -> validated runtime emitter desc
  -> future .wfxbin
```

## 1. 반영해야 하는 코드

이 문서는 즉시 구현 패치가 아니라 설계 가이드다. 아래 항목은 현재 코드 상태와 다음 구현 진입점을 나눈 것이다.

### 1.1 현재 상태

`Client/Private/UI/EffectTuner.cpp`

- Irelia 전용 legacy/live tuner다.
- preset enum이 `QTrail`, `QMark`, `WSpin`, `WStage2Slash`, `EBeam`, `RPulse`로 하드코딩되어 있다.
- color, width, height, lifetime, atlas, UV scroll, blend, mesh material, depth mode를 ImGui slider로 조정한다.
- Irelia live tuning 구조체를 직접 만지고 `Spawn Test`, `Save Preset (Clipboard)`, `Dump EFX-0 Manifest`, `Dump Current .wfx`, `Load .wfx + Spawn` 기능을 제공한다.
- 장점: 빠른 인게임 체감 튜닝.
- 한계: champion-specific, C++ paste 중심, asset browser/graph/undo/transaction 부재.

`Client/Private/UI/WfxEffectToolPanel.cpp`

- 현재 주력으로 삼아야 하는 WFX 툴이다.
- tabs: `WFX Assets`, `Textures`, `Inspector`, `Preview`.
- `.wfx` catalog scan, texture scan, load/save, emitter add/duplicate/delete, selected texture apply, Irelia Q quick cue builder, edited asset preview가 있다.
- preview는 `CFxSystem::GetAssetRegistry().RegisterOrReplaceByName` 후 `CFxCuePlayer::PlayAll`로 실제 runtime cue path를 사용한다.
- 장점: asset-file 중심, runtime path와 연결, 다중 emitter 편집 가능.
- 한계: ImGui form editor 수준이며 graph/curve/timeline/undo/hot reload 경계가 아직 약하다.

`Client/Private/UI/WfxAssetCatalog.cpp`

- `.wfx` 파일과 texture directory를 recursive scan한다.
- load error, emitter count, render type summary, missing texture/model resource를 표시할 수 있다.
- UE Content Browser로 확장하기 좋은 출발점이다.

`Client/Private/GameObject/FX/WfxDocument.cpp`

- `.wfx` load/save 문서 wrapper다.
- 저장은 구조화된 writer를 갖고 있지만 runtime loader 쪽은 아직 `Engine/Private/FX/FxAsset.cpp`의 수동 string parser에 의존한다.
- 다음 단계에서는 structured JSON parser와 schema validator가 우선이다.

`Engine/Public/FX/FxAsset.h`

- 현재 `.wfx` runtime contract의 핵심이다.
- `FxAsset`, `FxEmitterDesc`, `FxAnchorDesc`, `FxLifecycleDesc`, render type `Billboard/Ribbon/Beam/GroundDecal/MeshParticle/ShockwaveRing`이 있다.
- lifecycle, anchor, material, atlas, ribbon, segment, wind-wall block flag까지 이미 들어 있다.

`Client/Private/GameObject/FX/FxCuePlayer.cpp`

- cue name으로 asset registry를 찾고 `Billboard/GroundDecal/ShockwaveRing`, `Beam`, `Ribbon`, `MeshParticle`를 runtime component로 spawn한다.
- server cue single-source 방향과 맞다. editor preview도 이 경로를 써야 한다.

`EldenRingEditor/Private/EldenRingEditorScene.cpp`

- DockSpace 기반 editor shell, Content Browser placeholder, FX Graph validation panel, Sequencer panel, World Partition panel이 이미 있다.
- 현재 FX Graph panel은 load/validate/compile probe 수준이다.
- Elden급 통합 editor는 이 shell에 WFX editor panel을 이식하는 쪽이 맞다.

### 1.2 본질 구조

UE Niagara를 그대로 복제하지 않는다. Winters에 필요한 본질만 남긴다.

- System: 하나의 `.wfx` asset.
- Emitter: render/lifecycle/spawn 단위.
- Module: spawn/init/update/render stage의 parameterized operation.
- Graph metadata: editor가 보기 위한 노드/edge/position/curve 정보.
- Runtime emitter desc: 실제 client가 즉시 읽는 validated data.
- Cue binding: gameplay/server event가 호출하는 stable cue name.
- Preview context: world position, attach entity, forward, target segment, lifetime override, size override, seed.

### 1.3 개선 설계

1. `CWfxEffectToolPanel`을 공식 WFX editor panel로 승격한다.
2. `CEffectTuner`는 legacy champion-specific tuner로 명시하고, 새 기능은 WFX editor에 넣는다.
3. `.wfx`는 designer-facing authoring file이자 runtime-readable seed로 유지하되, parser를 structured JSON reader/writer로 교체한다.
4. editor graph metadata와 runtime emitter desc를 같은 파일에 저장하되, runtime은 desc만 읽어도 동작해야 한다.
5. preview는 별도 fake renderer가 아니라 `CFxCuePlayer::PlayAll`을 계속 사용한다.
6. in-game tuning은 real map/champion/anchor에서 확인하는 preview mode로 남기고, asset authoring은 editor shell로 이동한다.
7. server-authoritative gameplay에서는 cue name/event/seed만 truth이고, FX graph 자체는 presentation data다.

## 2. UE식 패널 설계

### 2.1 Workspace

권장 dock layout:

```text
Top Toolbar
  Play / Pause / Restart / Save / Reload / Validate / Cook

Left
  Content Browser
  WFX asset tree
  Texture / mesh dependency list

Center
  Preview Viewport
  Graph Canvas

Right
  Details
  Selected emitter/module/material/anchor/lifecycle inspector

Bottom
  Timeline / Curve
  Validation / Log
```

현재 `WFX Assets`, `Textures`, `Inspector`, `Preview` tabs는 이 layout으로 나눠 옮기면 된다. 기능을 다시 만들 필요는 없다.

### 2.2 Content Browser

현재 `CWfxAssetCatalog`를 확장한다.

- `.wfx`, `.wtex`, `.wmesh`, `.fbx`, `.png`를 같은 catalog item 모델로 본다.
- champion, skill, render type, missing dependency, last load error, emitter count를 column으로 표시한다.
- selected asset은 `CWfxDocument`로 열고, dependency click은 details inspector로 이동한다.
- missing resource는 preview 전에 막지 말고 validation issue로 노출한다.

### 2.3 System / Emitter Stack

Niagara의 stack UX를 Winters식으로 줄이면 다음 네 stage면 충분하다.

```text
System
  User Parameters
Emitter
  Spawn
  Init
  Update
  Render
```

초기 구현은 graph canvas보다 stack editor가 먼저다. 지금 `FxEmitterDesc`가 이미 flat field를 갖고 있으므로 stage별로 같은 데이터를 묶어 보여주면 된다.

### 2.4 Graph

`Engine/Public/FX/Graph/FxGraph.h`의 node model을 editor metadata로 사용한다.

- graph는 artist/designer UX다.
- compiler는 graph를 runtime emitter desc 또는 exec plan으로 bake한다.
- graph가 없는 legacy `.wfx`도 load되어야 한다.
- graph를 저장해도 `FxEmitterDesc`가 같이 저장되어 runtime fallback이 가능해야 한다.

### 2.5 Preview

preview는 세 mode로 나눈다.

- Isolated Preview: editor scene 안의 고정 ground/grid/target dummy에서 반복 재생.
- InGame Preview: 현재 `Scene_InGame`의 player, mouse direction, target segment를 이용해 실제 맵에서 확인.
- Cue Playback Preview: server event와 같은 cue name/path로 `CFxCuePlayer`를 호출해 중복 재생 여부를 잡는다.

필수 controls:

- play / pause / restart
- one-shot / loop
- scrub time
- slow motion
- fixed seed
- attach to player
- use mouse direction
- target segment length
- show lifetime bars per emitter

### 2.6 Hot Reload

저장 후 곧바로 다음을 수행한다.

```text
Save .wfx
  -> Validate schema/dependencies
  -> RegisterOrReplaceByName
  -> Restart preview instance
  -> Report changed emitters/resources
```

normal F5 runtime을 우회하지 않는다. editor-only preview는 명시적인 tool mode에서만 돌린다.

## 3. 협업 분리

Designer:

- cue name, emitter stack, timing, size, color, anchor, lifecycle, preview context를 조정한다.
- C++를 수정하지 않는다.

Artist:

- texture, mesh, atlas frame, material intent를 공급한다.
- missing dependency를 catalog에서 확인한다.

Gameplay Developer:

- server/shared event에서 cue name과 gameplay seed를 결정한다.
- FX의 시각 수치를 gameplay truth로 쓰지 않는다.

Engine/Rendering Developer:

- render type, material, shader, particle pool, graph compiler, validator를 확장한다.
- Client/Public/Shared에 DX concrete type을 노출하지 않는다.

Tools Developer:

- WFX editor shell, transaction, content browser, graph canvas, hot reload, cook pipeline을 소유한다.

## 4. 단계별 계획

### Phase 0 - 현재 툴 정리

- `CWfxEffectToolPanel`을 공식 WFX editor panel로 문서화한다.
- `CEffectTuner`는 legacy Irelia quick tuner로 남긴다.
- F7 WFX panel, F10 legacy debug panel의 역할을 분리한다.

검증:

- F7로 WFX panel open.
- `.wfx` scan/load/save/preview 동작 확인.
- 기존 `EffectTuner` spawn test가 사라지지 않는지 확인.

### Phase 1 - Content Browser 강화

- WFX/texture/model dependency catalog를 통합한다.
- missing resource, load error, render type, emitter count를 항상 표시한다.
- selected asset의 dependency graph를 보여준다.

검증:

- `Data/LoL/FX/Champions` scan.
- missing texture/model issue가 UI에 표시됨.

### Phase 2 - Details / Stack Editor

- flat inspector를 `System`, `Emitter`, `Spawn`, `Init`, `Update`, `Render`, `Material`, `Anchor`, `Lifecycle` sections로 재배치한다.
- field edit은 transaction/dirty flag를 남긴다.
- save 전 validate를 통과해야 한다.

검증:

- emitter duplicate/delete 후 undo/redo 가능.
- save/load round-trip 후 field diff 없음.

### Phase 3 - Preview Viewport / Timeline

- isolated preview world를 만든다.
- emitter lifetime/start delay/fade in/out을 timeline bar로 표시한다.
- play/pause/restart/scrub/fixed seed를 제공한다.

검증:

- same seed, same time에서 같은 preview가 나온다.
- edited asset과 saved asset preview가 일치한다.

### Phase 4 - InGame Hot Reload

- `.wfx` save 후 runtime registry reload.
- active preview cue restart.
- server cue path와 duplicate local playback을 검출하는 debug row 추가.

검증:

- `CFxCuePlayer::PlayAll` path로만 preview 재생.
- server event cue와 local preview가 동시에 중복 재생되지 않음.

### Phase 5 - Graph / Module

- `CFxGraph` metadata를 `.wfx`에 저장한다.
- graph validate -> compile -> emitter desc bake 흐름을 만든다.
- 초기 module은 `SpawnBurst`, `InitPosition`, `InitVelocity`, `InitLifetime`, `InitColor`, `Age`, `SizeOverLife`, `ColorOverLife`, renderer nodes만 둔다.

검증:

- graph 없는 legacy `.wfx` load 가능.
- graph 있는 `.wfx` save/load 가능.
- compile 결과가 현재 emitter desc와 일치.

### Phase 6 - Elden급 통합 Editor

- `EldenRingEditorScene`의 DockSpace shell에 Content Browser, WFX Graph, Preview Viewport를 통합한다.
- LoL in-game WFX panel은 runtime-context preview로 축소한다.
- 장기적으로 `.wfxbin` cook과 asset bundle 검증으로 이동한다.

검증:

- `WintersEldenRingEditor.exe`에서 WFX asset 선택/validate/preview.
- `WintersGame.exe`에서 같은 cue name이 같은 runtime path로 재생.

## 5. 검증

문서 작성 검증:

```powershell
git diff --check -- AGENTS.md .md/plan/2026-06-24_EFFECT_TOOL_UE_STYLE_EDITOR_GUIDE.md
```

향후 구현 검증:

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

```powershell
Client\Bin\Debug\WintersGame.exe --banpick-smoke --smoke-start --smoke-champion=IRELIA --fps=60 --no-vsync
```

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Winters.sln /p:Configuration=Debug /p:Platform=x64 /m
```

확인 필요:

- 현재 full S17 harness는 로컬 PDB write issue가 있었으므로 재시도 전 `WintersEngine.pdb` lock 여부 확인.
- WFX editor 구현 단계에서는 `Client/Include/Client.vcxproj` 등록 여부 확인.
- Engine public header 변경 시 `UpdateLib.bat` 또는 빌드 산출 SDK sync 확인.

## 6. 운영 규칙

- 설계/가이드/핸드오프 문서는 날짜를 붙여 `.md/plan/YYYY-MM-DD_<TOPIC>.md`에 남긴다.
- 기존 `.md/plan/EffectTool/**`은 장기 master/reference로 보고, 당일 결정과 이어받기 문서는 날짜 plan 문서에 남긴다.
- FX gameplay truth는 서버 cue/event/seed가 소유한다.
- `.wfx` 수치는 presentation data이며, damage/range/hit 판정의 근거가 아니다.
- preview가 필요해도 normal F5 roster/map/champion/snapshot/FX 흐름을 숨기지 않는다.
