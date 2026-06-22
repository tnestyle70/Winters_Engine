# Winters Clone / Ignore 구조 정리 보고서

작성일: 2026-06-22
대상 로컬 경로: `C:\Users\tnest\Desktop\Winters`
대상 브랜치: `main`
원격 저장소: `https://github.com/tnestyle70/Winter_Engine.git`

## 0. 결론

- fresh clone 직후 보이는 것은 Git이 추적하는 파일뿐이다. 현재 `git ls-files` 기준 추적 파일은 2,464개다.
- `Client/Bin/Resource/`는 `.gitignore`로 제외된다. 실행/시각 리소스는 clone 후 반드시 별도 복원해야 한다.
- `Tools/Bin/flatc.exe`, `Engine/ThirdPartyLib/**/Bin/**`, `Engine/ThirdPartyLib/**/Lib/**`는 `.gitignore` 예외로 추적될 수 있게 열려 있고, 현재 재현 검증 스크립트는 통과했다.
- 현재 로컬 작업트리는 수정/미추적 파일이 있다. 이 변경들은 commit/push하지 않으면 서브 데스크탑 clone에 내려오지 않는다.
- 현재 `.gitmodules` 파일은 없다. `Engine/External/imgui`는 submodule이 아니라 추적 파일로 vendored 되어 있다.

검증 결과:

```text
powershell -ExecutionPolicy Bypass -File Tools\VerifyPullRepro.ps1
PASS: pull reproducibility inputs and local asset boundary are valid.
```

## 1. Fresh Clone 기준 최상위 구조

clone 명령:

```bat
git clone https://github.com/tnestyle70/Winter_Engine.git C:\Users\<USER>\Desktop\Winters
cd /d C:\Users\<USER>\Desktop\Winters
```

현재 Git 추적 파일 수 기준 최상위 분포:

| 항목 | 추적 파일 수 | clone 포함 여부 |
|---|---:|---|
| `Engine/` | 851 | 포함 |
| `.md/` | 600 | 포함 |
| `Client/` | 296 | 포함. 단 `Client/Bin/Resource`, `Debug`, `Release` 제외 |
| `Shared/` | 227 | 포함 |
| `EngineSDK/` | 152 | `inc/` 중심 포함. `bin/`, `lib/`는 generated local |
| `Data/` | 114 | 포함 |
| `Services/` | 61 | 포함. `Services/.env` 제외 |
| `Server/` | 48 | 포함. `Server/Bin/` 제외 |
| `Tools/` | 28 | 포함. `Tools/Bin/flatc.exe` 포함, tool build output 제외 |
| `Shaders/` | 16 | 포함 |
| `winters-skills/` | 14 | 포함 |
| `EldenRingClient/` | 13 | 포함. `EldenRingClient/Bin/` 제외 |
| `.toolkit/` | 10 | 포함 |
| `EldenRingEditor/` | 6 | 포함. `EldenRingEditor/Bin/` 제외 |
| `cmake/` | 5 | 포함 |
| `Replay/` | 3 | 포함 |
| `Profiles/` | 1 | 포함 |
| `Plan/` | 2 | 포함. 현재 미추적 `Plan/S02`, `Plan/S03*`는 commit 전까지 제외 |
| 루트 문서/솔루션/이미지/로그 | 각 1 | 추적된 파일만 포함 |

Fresh clone에서 보이는 큰 구조:

```text
Winters/
  .gitignore
  AGENTS.md
  CLAUDE.md
  CLAUDE_Legacy.md
  CMakeLists.txt
  CMakePresets.json
  UpdateLib.bat
  Winters.sln
  TODO.md
  refactoring.md
  .md/
  .toolkit/
  .vscode/
  Client/
  cmake/
  Data/
  EldenRingClient/
  EldenRingEditor/
  Engine/
  EngineSDK/
  Plan/
  Profiles/
  Replay/
  Server/
  Services/
  Shaders/
  Shared/
  Tools/
  winters-skills/
```

## 2. Fresh Clone 상세 폴더 지도

아래는 clone 직후 확인해야 하는 주요 폴더 지도다. 개별 파일 전체 나열이 아니라, 빌드/런타임 판단에 필요한 계층 중심이다.

```text
.md/
  architecture/
  architecture/Legacy/
  architecture/UE5_REFERENCE/
  archive/
  build/
  EldenRing/
  GameDesign/
  guide/
  plan/
  plan/ai/
  plan/backend/
  plan/Champion/
  plan/effect_tool/
  plan/EffectTool/
  plan/EldenRingEditor/
  plan/engine/
  plan/engine-api/
  plan/graphics/
  plan/performance/
  plan/physics/
  plan/refactor/
  plan/Replay/
  plan/rhi/
  plan/rhi/sessions/
  plan/security/
  plan/sim/
  plan/WintersFormat/
  process/
  research/
  roadmap/
  TODO/
  논문/
  문서/
  이력서/

Client/
  Bin/
    Shader/                 # 추적됨
    Debug/                  # clone 제외, build output
    Release/                # clone 제외, build output
    Intermediate/           # clone 제외, build intermediate
    Resource/               # clone 제외, runtime assets
  Include/
  Private/
    ClientShell/
    Dev/
    ECS/Systems/
    GameMode/LOL/
    GameModule/LOL/
    GameObject/
    GameObject/Champion/<Champion>/
    GameObject/FX/
    GameObject/Projectile/
    GamePlay/
    Manager/
    Map/
    Network/
    Replay/
    Scene/
    UI/
  Public/
    ClientShell/
    Dev/
    ECS/Systems/
    GameMode/LOL/
    GameModule/LOL/
    GameObject/
    GameObject/Champion/<Champion>/
    GameObject/FX/
    GameObject/Projectile/
    GamePlay/
    Manager/
    Map/
    Network/
    Replay/
    Scene/
    UI/

Data/
  GameModes/
  Gameplay/ChampionGameData/
  LoL/FX/Champions/<Champion>/
  LoL/FX/Object/Minion/
  LoL/FX/Object/Turret/

Engine/
  External/
    imgui/
    lua-5.4.8/
    tracy/
  Include/
  Private/
    AI/
    AssetFormat/
    Core/
    ECS/
    Editor/
    Framework/
    FX/
    Manager/
    Platform/
    Renderer/
    Resource/
    RHI/DX11/
    RHI/DX12/
    Scene/
    Scripting/
    Sound/
    Tools/AssetConverter/
  Public/
    AI/
    AssetFormat/
    Core/
    ECS/
    Editor/
    Framework/
    FX/
    Manager/
    Platform/
    Renderer/
    Resource/
    RHI/
    Scene/
    Scripting/
    Sound/
  ThirdPartyLib/
    Assimp/
      Bin/Debug/
      Bin/Release/
      Inc/
      Lib/Debug/
      Lib/Release/
    DirectXTK/
      Bin/Debug/
      Bin/Release/
      Inc/
      Lib/Debug/
      Lib/Release/
    FlatBuffers/Inc/
    FMOD/
      Bin/
      Inc/
      Lib/

EngineSDK/
  inc/                     # 추적됨
  bin/                     # clone 제외, generated/local
  lib/                     # clone 제외, generated/local

EldenRingClient/
  Include/
  Private/
  Public/
  Bin/                     # clone 제외

EldenRingEditor/
  Private/
  Public/
  Bin/                     # clone 제외

Server/
  Include/
  Private/Game/
  Private/Network/
  Private/Security/
  Public/Game/
  Public/Network/
  Public/Security/
  Bin/                     # clone 제외

Services/
  cmd/
  internal/
  migrations/
  pkg/
  .env.example             # clone 포함
  .env                     # clone 제외

Shared/
  GameSim/
    Champions/<Champion>/
    Components/
    Core/
    Definitions/
    Feedback/
    Generated/
    Include/
    Registries/
    Replication/
    Systems/
  Network/
  Replay/
  Schemas/
    Generated/cpp/
    Generated/go/

Shaders/
  BRDF/
  SSAO/

Tools/
  Bin/
    flatc.exe              # clone 포함
    Debug/                 # clone 제외, tool build output
    Release/               # clone 제외, tool build output
  ChampionData/
  EldenAssetPipeline/
  External/LeagueToolkitProbe/
  MushroomZero/
  SimLab/
  WintersAssetConverter/
```

## 3. `.gitignore` 기준 clone에서 빠지는 구조

루트 `.gitignore`의 큰 정책:

```text
.vs/
*.user
*.suo
.claude/
.codex/
.worktree-backups/
[Dd]ebug/
[Rr]elease/
x64/
x86/
[Oo]bj/
ipch/
out/
.builds/
/Client/Bin/Debug*/
/Client/Bin/Release*/
**/[Bb]in/[Ii]ntermediate/
*.exe, *.pdb, *.ilk, *.obj, *.tlog, *.log, ...
/Client/Bin/Resource/
/Client/Bin/Resource.zip
imgui.ini
Services/.env
```

예외로 clone에 포함될 수 있게 열린 항목:

```text
!Tools/Bin/flatc.exe
!Engine/ThirdPartyLib/**/Bin/
!Engine/ThirdPartyLib/**/Bin/**
!Engine/ThirdPartyLib/**/Lib/
!Engine/ThirdPartyLib/**/Lib/**
```

현재 실제 로컬에서 ignored로 잡힌 항목:

```text
.claude/
.vs/
Client/Bin/Debug/
Client/Bin/Intermediate/
Client/Bin/Release/
Client/Bin/Resource/
Client/Include/Client.vcxproj.user
EldenRingClient/Bin/
EldenRingClient/Include/EldenRingClient.vcxproj.user
EldenRingEditor/Bin/
Engine/Bin/
Engine/Include/Engine.vcxproj.user
EngineSDK/bin/
EngineSDK/lib/
Server/Bin/
Server/Include/Server.vcxproj.user
Shared/GameSim/Bin/
Tools/Bin/Debug/
Tools/Bin/Release/
Tools/External/LeagueToolkitProbe/bin/
Tools/External/LeagueToolkitProbe/obj/
Tools/External/lol2gltf/
Tools/Intermediate/
Winters.slnLaunch.user
imgui.ini
out/
```

`Engine/External/imgui/.gitignore`와 Android 예제 `.gitignore`는 vendored ImGui의 예제 build/cache/IDE 산출물을 제외한다. Winters 본체 기준으로는 루트 `.gitignore`가 더 중요하다.

## 4. `Client/Bin/Resource` 복원 대상

`Client/Bin/Resource`는 clone에 포함되지 않는다. 현재 로컬의 상위 구조는 다음과 같다.

```text
Client/Bin/Resource/
  EldenRing/
    Assets/
    Characters/
    FullGame/
    Manifests/
    Maps/
    Runtime/
    SourceBundles/
    UI/
  Font/
  Sound/
  Texture/
    Character/
    FX/
    MAP/
    Object/
    UI/
  UI/
    Lua/
  Texture.zip              # 현재 로컬에 존재, 약 12.1 GiB
```

현재 확인값:

```text
Client/Bin/Resource.zip 없음
Client/Bin/Resource/Texture.zip 있음
Length: 12,962,303,446 bytes
LastWriteTime: 2026-06-18 11:43:39
```

서브 데스크탑에서 전체 런타임을 맞추려면 가장 단순한 정답은 `Client/Bin/Resource/` 전체를 같은 경로로 복사하는 것이다.

```bat
robocopy C:\Users\tnest\Desktop\Winters\Client\Bin\Resource ^
  C:\Users\<USER>\Desktop\Winters\Client\Bin\Resource /E
```

외장 드라이브나 NAS를 거칠 때:

```bat
robocopy C:\Users\tnest\Desktop\Winters\Client\Bin\Resource D:\WintersTransfer\Resource /E
robocopy D:\WintersTransfer\Resource C:\Users\<USER>\Desktop\Winters\Client\Bin\Resource /E
```

Elden showcase만 최소 이전하려면 기존 문서 `.md/EldenRing/15_DESKTOP_TRANSFER_PACKAGE.md` 기준으로 다음만 묶을 수 있다.

```text
Client/Bin/Resource/EldenRing/Runtime/
Client/Bin/Resource/EldenRing/Assets/
Client/Bin/Resource/EldenRing/Maps/
Client/Bin/Resource/EldenRing/Manifests/
```

하지만 Winters 전체 F5/LoL UI/맵/사운드까지 같은 상태로 맞추려면 최소 패키지보다 `Client/Bin/Resource/` 전체 복원이 안전하다.

## 5. 서브 데스크탑에서 추가/확인할 것

### 5.1 Git으로 받는 것

clone으로 바로 받아야 하는 항목:

```text
source code
Winters.sln
CMakeLists.txt / CMakePresets.json
Shaders/
Data/
Shared/Schemas/Generated/
EngineSDK/inc/
Tools/Bin/flatc.exe
Engine/ThirdPartyLib/Assimp/{Bin,Lib}
Engine/ThirdPartyLib/DirectXTK/{Bin,Lib}
Engine/ThirdPartyLib/FMOD/{Bin,Lib}
Engine/External/imgui
```

확인 명령:

```powershell
Test-Path Engine\External\imgui\imgui.cpp
Test-Path Engine\ThirdPartyLib\Assimp\Lib\Debug\assimp-vc143-mtd.lib
Test-Path Engine\ThirdPartyLib\DirectXTK\Lib\Debug\DirectXTK.lib
Test-Path Engine\ThirdPartyLib\FMOD\Lib\fmod_vc.lib
Test-Path Tools\Bin\flatc.exe
powershell -ExecutionPolicy Bypass -File Tools\VerifyPullRepro.ps1
```

`Tools\VerifyPullRepro.ps1 -RequireTracked`는 필요한 binary input이 실제 Git 추적 상태인지까지 더 엄격하게 확인한다.

### 5.2 별도 복원해야 하는 것

반드시 별도 복원:

```text
Client/Bin/Resource/
```

상황별 복원:

```text
Services/.env                         # backend 로컬 실행 시, Services/.env.example에서 생성
Client/Include/*.vcxproj.user         # 개인 VS 디버그 설정. 보통 복사 불필요
Winters.slnLaunch.user                # 개인 솔루션 launch 설정. 보통 복사 불필요
.claude/                              # 에이전트 로컬 상태. 보통 복사 불필요
.vs/                                  # Visual Studio 로컬 캐시. 복사 금지에 가까움
imgui.ini                             # ImGui 창 배치. 필요할 때만 복사
Tools/External/lol2gltf/              # 로컬 외부 도구/산출물. 해당 파이프라인 쓸 때만 복원
```

복원하지 말고 새로 생성할 것:

```text
Client/Bin/Debug/
Client/Bin/Release/
Engine/Bin/
Server/Bin/
Shared/GameSim/Bin/
Tools/Bin/Debug/
Tools/Bin/Release/
Tools/Intermediate/
out/
EngineSDK/bin/
EngineSDK/lib/
```

이들은 build output 또는 generated local output이다.

### 5.3 로컬 개발 도구

일반 C++ 클라이언트/서버 빌드:

```text
Visual Studio 2022 이상
Desktop development with C++
Windows SDK
MSBuild
```

파이프라인/도구 작업:

```text
Python 3.x
Blender, WitchyBND, UXM, Soulstruct, texconv.exe
```

Go backend 작업:

```text
Go toolchain
Docker Desktop 또는 Postgres/Redis/Kafka 로컬 대체 환경
Services/.env
```

## 6. Build / 실행 순서

권장 fresh clone 순서:

```bat
git clone https://github.com/tnestyle70/Winter_Engine.git C:\Users\<USER>\Desktop\Winters
cd /d C:\Users\<USER>\Desktop\Winters
powershell -ExecutionPolicy Bypass -File Tools\VerifyPullRepro.ps1
robocopy <RESOURCE_SOURCE>\Resource C:\Users\<USER>\Desktop\Winters\Client\Bin\Resource /E
```

Visual Studio 기준:

```text
1. Winters.sln 열기
2. Debug | x64 선택
3. Engine 빌드
4. Client 빌드
5. Server가 필요하면 Server 빌드
6. F5 실행
```

`UpdateLib.bat` 역할:

```text
Engine public header -> EngineSDK/inc 동기화
Engine import/runtime output -> EngineSDK/lib, Client/Bin/Debug, Client/Bin/Release 쪽 배포
ThirdParty runtime DLL -> Client/Bin/<Config> 복사
```

주의: 런타임 리소스 기준 경로는 `Client/Bin/Resource`다. `Client/Bin/Debug/Resource` 또는 `Client/Bin/Release/Resource`를 기준으로 삼지 않는다.

## 7. 현재 로컬 작업트리 주의

아래는 현재 로컬에는 있지만 fresh clone에는 그대로 반영되지 않는 항목이다.

수정된 추적 파일:

```text
.md/architecture/WINTERS_CODEBASE_COMPASS.md
Client/Private/DynamicCamera.cpp
Client/Private/Network/Client/EventApplier.cpp
Client/Private/Scene/Scene_InGame.cpp
Client/Private/UI/MinimapPanel.cpp
Client/Public/DynamicCamera.h
Client/Public/Network/Client/EventApplier.h
Client/Public/Scene/Scene_InGame.h
Client/Public/UI/MinimapPanel.h
Engine/Include/GameInstance.h
Engine/Private/GameInstance.cpp
Engine/Private/Manager/UI/UI_Manager.cpp
Engine/Public/Manager/UI/UI_Manager.h
EngineSDK/inc/GameInstance.h
Server/Private/Game/GameRoom.cpp
Server/Private/Game/GameRoomInternal.h
Shared/GameSim/Components/RespawnComponent.h
```

이 수정은 commit/push해야 서브 데스크탑 clone에 반영된다.

미추적 항목:

```text
.md/TODO/06-22/
.md/architecture/CLASS_SERVANT_FOUNDATION_DIRECTION.md
.md/plan/2026-06-22_MINIMAP_CAMERA_BOX_CLICK_MOVE.md
.md/plan/refactor/08_INGAME_SKILL_HOOK_ATOM_BOUNDARY_PLAN.md
.md/plan/refactor/09_LOL_DATA_ATOM_EXTRACTION_COLLAB_PLAN.md
.md/plan/refactor/10_LOL_DATA_AUTHORITY_PATCH_PIPELINE_PLAN.md
Plan/S02_OPENAI_STYLE_BOT_AI_ENVIRONMENT_PLAN.md
Plan/S03_MINIMAP_CAMERA_JUMP_RESULT.md
Plan/S03_MINIMAP_CAMERA_JUMP_SESSION.md
Shared/GameSim/Definitions/DataPackManifest.h
Shared/GameSim/Definitions/LoLPublicGameHintData.h
Tools/LoLData/
Tools/VerifyMinionProjectileAssets.ps1
Tools/VerifyPingWheelAssets.ps1
```

이 항목은 `git add` 후 commit/push해야 clone에 포함된다. 의도적으로 로컬에만 둘 작업이면 복사 대상에서 제외한다.

## 8. 서브 데스크탑 체크리스트

```text
[ ] repo clone 완료
[ ] `powershell -ExecutionPolicy Bypass -File Tools\VerifyPullRepro.ps1` 통과
[ ] `Client/Bin/Resource/` 전체 복원
[ ] 필요 시 `Services/.env` 생성
[ ] Visual Studio C++ workload / Windows SDK 설치 확인
[ ] Engine 빌드
[ ] Client 빌드
[ ] `Client/Bin/Debug/WintersEngine.dll` 등 runtime DLL 배포 확인
[ ] F5 실행
[ ] Elden 파이프라인 작업 시 `.md/EldenRing/15_DESKTOP_TRANSFER_PACKAGE.md`의 ToolchainArchives 복원
```

## 9. 관련 문서

```text
.md/build/CLONE_REPRO_REQUIREMENTS.md
.md/build/THIRDPARTY_INTEGRATION_GUIDE.md
.md/EldenRing/15_DESKTOP_TRANSFER_PACKAGE.md
AGENTS.md
.claude/gotchas.md
.md/architecture/WINTERS_CODEBASE_COMPASS.md
```
