# Winters Essence Boundary Refactor

Winters 폴더 전체를 본질 소유권 기준으로 다시 나눈다.

목표는 폴더를 예쁘게 정리하는 것이 아니라, 어떤 사람이 어떤 산출물을 만지고 어떤 런타임이 그것을 소비하는지 더 이상 헷갈리지 않는 구조를 만드는 것이다.

최종 구조의 기준은 하나다.

```text
기획 의도 -> 데이터/규칙 계약 -> 검증된 런타임 -> 표현/도구/서비스
```

## 규모 전제

Winters는 단일 샘플 프로젝트가 아니라, AAA급 게임과 엔진 제작 규모까지 확장 가능한 구조를 목표로 한다. 기준은 라이엇식 대형 라이브 게임 조직, 프롬소프트식 액션 RPG 제작 조직, 붉은사막/BlackSpace Engine급 대형 월드 제작 엔진, GTA급 오픈월드 엔진, Unreal/Unity급 범용 엔진이 감당하는 협업 규모다.

이 문장은 현재 Winters가 그 규모의 기능을 이미 갖췄다는 뜻이 아니다. 폴더와 소유권을 다시 잡을 때 수십 명, 나아가 수백 명이 동시에 작업해도 원본, 산출물, 런타임, 도구, 데이터, 서비스의 책임이 섞이지 않는 구조를 목표 제약으로 둔다는 뜻이다.

회사 규모 폴더 원자는 `.md/architecture/WINTERS_COMPANY_SCALE_FOLDER_ESSENCE.md`를 따른다.

## 최상위 원칙

한 파일, 한 모듈, 한 폴더는 하나의 본질만 가진다.

- Engine은 게임을 모르는 runtime primitive다.
- Shared/GameSim은 서버 권위 gameplay truth다.
- Server는 command를 받아 GameSim을 실행하고 snapshot/event/cue를 송신한다.
- Client는 제품별 presentation이다.
- EldenRingClient는 LoL Client와 별도 제품 presentation이다.
- EldenRingEditor는 runtime이 아니라 authoring workflow다.
- Data는 사람이 조정하는 의도와 cooked runtime asset의 출처다.
- Tools는 데이터를 만들고 검증하고 변환하는 오프라인 실행물이다.
- Services는 계정, 매치, 상점, 프로필 같은 게임 밖 backend state다.
- Build graph는 어떤 산출물을 어떻게 만드는지만 소유한다.
- Generated output은 편집 원본이 아니다.
- Docs는 행동 규칙과 결정만 남긴다. 코드나 `rg`가 답할 수 있는 목록은 문서화하지 않는다.

하나의 코드가 두 가지 이유로 바뀐다면 아직 덜 쪼갠 것이다.

## 협업 소유권

### 기획자

기획자는 게임의 의도를 소유한다.

- 승패 조건
- 게임 모드
- 챔피언/스킬 규칙
- 스탯, 쿨타임, 피해량, 성장식
- 아이템/보상/경제 수치
- AI policy 목표와 평가 기준
- SimLab golden case와 밸런스 검증 입력

기획자가 만지는 값은 C++ 분기가 아니라 Data, schema, generated table, spreadsheet export, SimLab case로 내려간다.

### 디자이너

디자이너는 플레이어가 보고 느끼는 표현 의도를 소유한다.

- UI layout, style, widget rule
- HUD/상점/상태창 배치
- FX graph, WFX, material preset
- animation timing, montage key, visual yaw/offset
- map placement, streaming cell, encounter placement
- editor-authored asset contract

디자이너가 만지는 값은 Client hardcode가 아니라 Data, editor output, cooked asset, visual definition으로 내려간다.

### 개발자

개발자는 계약과 실행을 소유한다.

- deterministic GameSim
- runtime loader와 validator
- RHI/renderer/resource/ECS primitive
- server authority, snapshot/event, replay
- network protocol과 schema/codegen
- build graph, deploy, cook, test
- tooling과 editor runtime boundary

개발자는 기획/디자인 값을 코드로 박지 않는다. 대신 그 값이 안전하게 로드되고 검증되고 재현되는 경계를 만든다.

## 본질 판정 질문

새 파일을 만들거나 기존 파일을 옮길 때 아래 순서로 판정한다.

1. 이것은 사람이 조정해야 하는 의도인가?
2. 이것은 서버가 결정해야 하는 gameplay truth인가?
3. 이것은 client가 보여주는 presentation인가?
4. 이것은 여러 제품이 공유하는 engine primitive인가?
5. 이것은 데이터를 만들거나 검증하는 offline tool인가?
6. 이것은 backend account/service state인가?
7. 이것은 build/deploy/generation 산출물인가?
8. 이것을 제거하면 normal F5 검증이 더 선명해지는가?

답이 둘 이상이면 파일을 나눈다.

## 폴더별 본질

### Build Graph

대상:
- `Winters.sln`
- `CMakeLists.txt`
- `CMakePresets.json`
- `cmake/`
- 각 `.vcxproj`

본질:
- 어떤 target을 어떤 source와 dependency로 빌드하는지 소유한다.

금지:
- browsing-only source map을 runtime owner처럼 취급하지 않는다.
- build 통과를 위해 의존성 방향을 뒤집지 않는다.
- MSBuild/CMake source list 불일치를 방치하지 않는다.

### Engine

대상:
- `Engine/`

본질:
- window, app host, frame loop
- RHI, renderer, render resource lifetime
- resource loading/cache primitive
- ECS core primitive
- generic spatial/query primitive
- UI rendering primitive
- audio playback primitive
- diagnostics/profiler/debug output
- low-frequency `CGameInstance` gateway

금지:
- LoL/Elden/champion/minion/turret/skill/HUD/shop/kill feed policy를 소유하지 않는다.
- `Shared/GameSim`, `Server`, `Client` 제품 코드를 include하지 않는다.
- Engine public API에 새 `ID3D11*` 또는 제품 타입을 밀어 올리지 않는다.
- Engine UI가 `CWorld`, GameSim component, 제품 manager를 직접 읽지 않는다.

현재 분리 후보:
- `Engine/Public/ECS/Components/GameplayComponents.h`
- `Engine/Public/ECS/Systems/MinionAISystem.h`
- `Engine/Public/ECS/Systems/TurretAISystem.h`
- `Engine/Public/ECS/Systems/TurretProjectileSystem.h`
- `Engine/Public/AI/BTNodes_Champion.h`
- `Engine/Public/FX/FxMaterialDesc.h`의 product-named mode
- `Engine/Public/Manager/UI/UI_Manager.h`의 LoL HUD/shop/kill feed state
- `Engine/Include/GameInstance.h`의 제품 UI forwarding

### EngineSDK

대상:
- `EngineSDK/`

본질:
- Engine public header와 binary의 배포 산출물이다.

금지:
- 직접 편집하지 않는다.
- 원본 소유권으로 판단하지 않는다.

수정 기준:
- Engine public header를 고치고 `UpdateLib.bat` 또는 Engine build로 동기화한다.

### Shared / GameSim

대상:
- `Shared/GameSim/`
- `Shared/Schemas/`
- `Shared/Replay/`

본질:
- deterministic gameplay data/rules
- `GameCommand`
- snapshot/event/cue schema
- champion/skill/stat/item truth
- replay contract
- server와 client가 공유하는 wire/data contract

금지:
- Engine, Client, Renderer, UI, ImGui, DX type을 include하지 않는다.
- visual asset path, mesh, texture, shader, animation file ownership을 갖지 않는다.
- gameplay truth를 Client hook으로 되돌리지 않는다.

현재 분리 후보:
- `Shared/GameSim/Components/*`가 Engine gameplay header를 re-export하는 구조
- `Shared/GameSim/Core/World/World.h`가 Engine `CWorld`에 기대는 temporary adapter
- `Shared/GameSim/Definitions/ChampionDef.h`의 visual asset field
- visual yaw/animation timing이 server truth와 섞인 runtime default

### Server

대상:
- `Server/`

본질:
- client command intake
- GameRoom/session authority
- GameSim execution
- snapshot/event/FX cue emission
- replay record
- bot command generation
- security/lag compensation

금지:
- client visual 성공을 서버 로그만으로 판정하지 않는다.
- bot AI가 truth component를 직접 고치지 않는다. command를 생산한다.
- normal F5를 smoke/lab shortcut으로 흐리지 않는다.

현재 분리 후보:
- Debug 기본 활성 smoke roster
- server default에 숨어 있는 lab/test-only branch

### Client / LoL

대상:
- `Client/`

본질:
- LoL product client
- input send
- weak prediction
- interpolation
- animation/FX/sound playback
- UI view-state build
- debug visualization
- server snapshot/event/cue consumption

금지:
- authoritative gameplay truth를 새로 만들지 않는다.
- Engine public API에 LoL 전용 타입을 요구하지 않는다.
- normal F5 roster/map/minion/champion/snapshot/UI/FX를 숨기지 않는다.

현재 분리 후보:
- `Client/Private/Scene/Scene_InGame.cpp`의 bootstrap/network/prediction/render/debug 혼합
- `Client/Private/Scene/InGameRosterSpawner.cpp`의 smoke flag 분기
- `Client/Private/GameObject/ChampionTable.cpp`와 legacy registration
- champion visual data와 GameSim data가 서로 물고 있는 구조

### EldenRingClient

대상:
- `EldenRingClient/`

본질:
- LoL과 별도인 action RPG product client
- Elden scene/camera/input/combat/world presentation
- Engine/RHI/resource contract 소비

금지:
- LoL Client 코드 안에 Elden 제품 의미를 섞지 않는다.
- Elden renderer를 새로 복제하지 않는다. 필요한 것은 Elden식 render snapshot이다.
- editor placement/save 기능을 runtime scene 안에 둔다면 임시 lab로 명시한다.

현재 분리 후보:
- runtime showcase scene 안의 placement editor/save 기능
- repo-relative resource edit path

### EldenRingEditor

대상:
- `EldenRingEditor/`

본질:
- content browser
- importer/converter/validator
- material resolver
- WFX/FX graph
- sequencer
- world partition/streaming cell editing

금지:
- normal runtime behavior를 숨기거나 우회하지 않는다.
- runtime gameplay truth나 renderer backend policy를 소유하지 않는다.

### Data

대상:
- `Data/`

본질:
- 기획/디자인 의도의 편집 원본
- runtime이 읽는 data contract
- cooked asset source
- product별 gameplay data와 visual data의 분리된 출처

규칙:
- gameplay stat/skill/cooldown/damage는 GameSim data contract로 간다.
- mesh, texture, shader, anim key, UI layout, WFX, material preset은 visual data contract로 간다.
- 같은 champion이라도 `ChampionGameData`와 `ChampionVisualDef`는 다른 소유권이다.

금지:
- C++ fallback table과 Data가 서로 다른 truth가 되지 않는다.
- validator 없는 data 확장을 runtime에 연결하지 않는다.

### Tools

대상:
- `Tools/`
- asset converter
- cook/codegen/validation scripts
- SimLab
- lab-only ML/AI bridge

본질:
- source data를 runtime artifact로 변환한다.
- schema/code를 생성한다.
- replay/golden test/league evaluation으로 검증한다.

금지:
- tool-only path가 normal F5 runtime을 대체하지 않는다.
- lab 결과를 검증 없이 Winters runtime source에 섞지 않는다.

### Services

대상:
- `Services/`

본질:
- auth
- profile
- matchmaking
- leaderboard
- payment/shop backend
- social/account/session service

금지:
- in-match gameplay truth를 Services에 두지 않는다.
- Client visual state를 Services가 소유하지 않는다.
- server authority path와 backend account state를 혼동하지 않는다.

### Shaders

대상:
- `Shaders/`

본질:
- shader source와 shader-side shared constants

금지:
- 제품 gameplay rule을 shader define으로 숨기지 않는다.
- root shader 복사 흐름을 resource truth로 오해하지 않는다.

### Replay

대상:
- `Replay/`

본질:
- authoritative command/snapshot/event 재현 자료

금지:
- replay 재생 편의를 위해 GameSim truth를 client visual path로 우회하지 않는다.

### Profiles / Logs / Captures / out

대상:
- `Profiles/`
- `out/`
- `Client/Bin/`
- `Engine/Bin/`
- `Server/Bin/`
- `profiler.json`
- runtime log/capture 파일

본질:
- 측정, 빌드, 실행 산출물

금지:
- 편집 원본으로 취급하지 않는다.
- 리팩터링 근거로 사용할 때는 재현 명령과 날짜를 함께 남긴다.

### Documentation

대상:
- `AGENTS.md`
- `CLAUDE.md`
- `CLAUDE_Legacy.md`
- `.claude/gotchas.md`
- `.md/architecture/`
- `.md/plan/`
- `.md/process/`
- `.md/GameDesign/`

본질:
- 행동 규칙
- 구조 결정
- 반복 실수
- 세션 계획
- 기획 의도

금지:
- 코드 목록을 문서로 복제하지 않는다.
- 오래된 규칙을 덧붙이며 보존하지 않는다. 교체한다.

## 리팩터링 순서

### S0. Folder Ownership Baseline

Winters top-level folder를 위 본질 단위로 고정한다.

검증:
- `rg --files`로 실제 owner와 build target을 확인한다.
- MSBuild/CMake/source map 불일치를 기록한다.
- generated output과 편집 원본을 구분한다.

### S1. Generated Output Quarantine

EngineSDK, Bin, out, log, capture, profiler 산출물을 source truth에서 제외한다.

검증:
- 수정 대상 계획에 `EngineSDK/inc`가 직접 들어가지 않는다.
- runtime resource 기준은 `Client/Bin/Resource`로 유지한다.

### S2. Engine De-Productization

Engine에서 제품 의미를 제거한다.

검증:
- Engine public/private/include에서 LoL, Elden, Champion, Minion, Turret, Skill, HUD, Shop, Kill, GameSim, Server, Client 검색 결과를 분류한다.
- generic primitive만 Engine에 남긴다.

### S3. GameSim Independence

Shared/GameSim이 Engine gameplay header에 기대지 않게 한다.

검증:
- `Shared/GameSim`에서 Engine, Renderer, UI, ImGui, DX include가 사라진다.
- SimLab deterministic golden test가 Engine visual path 없이 돈다.

### S4. Server Authority Cleanup

Server를 command intake, GameSim execution, snapshot/event/cue emission으로 좁힌다.

검증:
- bot은 command producer다.
- smoke roster/debug path는 명시적 lab flag에서만 켜진다.
- server log와 client visual 검증을 구분한다.

### S5. Client Product Presentation Split

LoL Client를 presentation owner로 좁힌다.

검증:
- `Scene_InGame` 책임을 bootstrap, network bridge, prediction, presentation, debug로 나눈다.
- Client는 Snapshot/Event/ViewState만 시각화한다.
- legacy local-only smoke path는 이름으로 격리한다.

### S6. Data Contract Split

기획 data와 디자인 data를 코드에서 분리한다.

검증:
- `ChampionGameData`와 `ChampionVisualDef`가 분리된다.
- gameplay 수치 변경은 GameSim data validator를 탄다.
- visual asset 변경은 Client visual loader/validator를 탄다.

### S7. UI Ownership Split

Engine UI와 Product UI를 나눈다.

검증:
- Engine UI는 draw primitive와 atlas/font/layout primitive만 남긴다.
- LoL HUD, shop, kill feed, HP bar view-state build는 Client가 소유한다.
- Engine UI가 `CWorld`나 GameSim component를 직접 읽지 않는다.

### S8. FX / Asset Pipeline Split

server cue, client playback, authored WFX, material preset을 분리한다.

검증:
- server cue 하나가 client visual playback 한 번으로 이어진다.
- product-named material mode는 Engine enum에서 빠지고 data preset으로 내려간다.
- legacy FX adapter는 명시적 migration path로만 남는다.

### S9. Elden Runtime / Editor Split

Elden runtime과 editor authoring을 분리한다.

검증:
- `EldenRingClient`는 runtime presentation만 가진다.
- placement/save/editor workflow는 `EldenRingEditor` 또는 Tools로 이동한다.
- LoL Client와 Elden Client가 scene/camera/input/combat code를 공유하지 않는다.

### S10. Tools / Services Boundary

offline tool과 backend service를 runtime source에서 분리한다.

검증:
- codegen/cook/convert/validation/lab script가 역할별로 나뉜다.
- Services는 account/backend state만 소유한다.
- in-match GameSim truth는 Services로 가지 않는다.

### S11. Build Graph Lock

정리된 owner 기준으로 build graph를 맞춘다.

검증:
- `Winters.sln`, `.vcxproj`, CMake target source list가 같은 소유권을 반영한다.
- filters 가상 폴더만으로 소유권을 판단하지 않는다.
- 각 target은 자기 본질 외 source를 build convenience로 끌어안지 않는다.

## 완료 조건

아래가 모두 참이면 Winters 폴더는 본질 기준으로 정리된 것이다.

- 기획자는 C++ 분기 없이 gameplay 수치를 바꿀 수 있다.
- 디자이너는 runtime truth를 건드리지 않고 UI/FX/placement를 바꿀 수 있다.
- 개발자는 Data contract와 validator로 변경을 검증할 수 있다.
- Engine은 제품 이름을 몰라도 빌드된다.
- Shared/GameSim은 Engine/Client/UI/DX 없이 deterministic하게 검증된다.
- Server는 Client visual 없이 authority를 검증한다.
- Client는 server event 없이 gameplay 결과를 만들지 않는다.
- Elden runtime과 editor authoring은 분리된다.
- Tools와 Services는 normal F5 runtime을 우회하지 않는다.
- generated output은 직접 편집 원본으로 쓰이지 않는다.

## 기본 검증 명령

```powershell
git diff --check
msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64
msbuild Shared/GameSim/Include/GameSim.vcxproj /p:Configuration=Debug /p:Platform=x64
msbuild Tools/SimLab/SimLab.vcxproj /p:Configuration=Debug /p:Platform=x64
cmake --build --preset engine-debug
cmake --build --preset elden-debug
```

각 세부 세션은 실제 h/cpp/vcxproj를 다시 inspect한 뒤, 기존 코드/교체 코드 단위의 적용 계획으로 쪼갠다.
