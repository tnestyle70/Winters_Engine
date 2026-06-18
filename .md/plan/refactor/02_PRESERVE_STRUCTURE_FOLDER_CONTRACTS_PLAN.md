Session - 기존 Winters 폴더명과 구조를 유지한 채 루트 폴더 협업 계약부터 고정한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Client/README.md

새 파일:

```md
# Client

원자: ProductPresentation

Client는 플레이어가 보는 제품 경험을 소유한다.

소유:
- player input
- camera, feel, interpolation
- animation, FX, audio playback
- HUD, UI, debug presentation
- snapshot/event/cue를 view state로 바꾸는 코드

소유하지 않음:
- 최종 HP, 피해, 쿨타임, 승패
- Engine runtime primitive
- Server authority
- Backend state
- Build, cook, package 계약

구조 규칙:
- 기존 `Client/` 폴더명과 구조를 유지한다.
- 새 폴더는 ProductPresentation 하위 의미가 기존 구조에 담기지 않을 때만 추가한다.
- 새 `.h/.cpp`를 추가하기 전에는 `.vcxproj`, `.filters`, `CMakeLists.txt`를 수정하지 않는다.
```

1-2. C:/Users/tnest/Desktop/Winters/Engine/README.md

새 파일:

```md
# Engine

원자: EngineCapability

Engine은 제품을 모르는 실행 능력을 소유한다.

소유:
- platform, window, frame loop
- RHI, renderer, resource runtime
- input, audio, UI primitive
- profiler, debug, capture primitive
- 제품 중립 runtime primitive

소유하지 않음:
- champion, skill, item, quest, boss, minion, turret
- 제품 HUD, 제품 UI rule
- server authority
- 기획 수치
- QA, marketing, live ops 산출물

구조 규칙:
- 기존 `Engine/` 폴더명과 구조를 유지한다.
- 게임 명사가 필요하면 Engine 밖의 ProductPresentation, GameplayTruth, GameDesignSource로 보낸다.
- Engine public header 변경 후에만 SDK 동기화를 검토한다.
```

1-3. C:/Users/tnest/Desktop/Winters/Shared/README.md

새 파일:

```md
# Shared

원자: GameplayTruth, protocol contract, replay/schema contract

Shared는 Client와 Server가 공유해야 하는 실행 계약을 소유한다.

소유:
- `Shared/GameSim/`: deterministic gameplay truth
- `Shared/Network/`: command, snapshot, event protocol
- `Shared/Replay/`: replay record contract
- `Shared/Schemas/`: shared data/schema contract

소유하지 않음:
- renderer, UI, ImGui, DX type
- Client visual feel
- Server process ownership
- Backend account state
- cooked art/audio asset

구조 규칙:
- 기존 `Shared/` 폴더명과 구조를 유지한다.
- GameSim은 화면을 몰라야 한다.
- Network와 Replay는 truth를 만들지 않고 전달/재현 계약만 가진다.
```

1-4. C:/Users/tnest/Desktop/Winters/Server/README.md

새 파일:

```md
# Server

원자: AuthorityExecution

Server는 command를 받아 gameplay truth를 실행하고 결과를 배포한다.

소유:
- command intake
- room/session authority
- GameSim tick
- snapshot/event/cue emission
- replay record emission
- bot command generation
- lag compensation, anti-cheat gate

소유하지 않음:
- Client visual success
- UI, animation, camera, local feel
- account, payment, store, profile backend state
- 기획 원본 수치 편집

구조 규칙:
- 기존 `Server/` 폴더명과 구조를 유지한다.
- gameplay 결과는 Client presentation이 아니라 Server authority와 GameSim truth로 증명한다.
```

1-5. C:/Users/tnest/Desktop/Winters/Data/README.md

새 파일:

```md
# Data

원자: GameDesignSource, PresentationDesignSource, ArtSource, AudioSource, RuntimeAsset, LiveOpsContract, PublishingSource

Data는 하나의 원자가 아니다. 사람이 고치는 원본과 런타임이 로드하는 산출물을 구분한다.

현재 의미:
- `Gameplay/`: GameDesignSource
- `GameModes/`: GameDesignSource
- `LoL/FX/`: PresentationDesignSource
- `*.dat`, `*.navgrid`: RuntimeAsset 후보

추가 후보:
- `Art/`: art source가 repo 원본으로 들어올 때만 추가
- `Audio/`: audio source가 repo 원본으로 들어올 때만 추가
- `Publishing/`: marketing/public asset 원본이 repo에 들어올 때만 추가

구조 규칙:
- 기존 `Data/` 폴더명과 구조를 유지한다.
- 새 폴더는 실제 원자가 기존 구조에 담기지 않을 때만 추가한다.
- C++ fallback table을 Data와 다른 두 번째 truth로 만들지 않는다.
```

1-6. C:/Users/tnest/Desktop/Winters/Tools/README.md

새 파일:

```md
# Tools

원자: AuthoringTool, ValidationEvidence, CookPipeline, PackagePatchDeploy

Tools는 하나의 원자가 아니다. 사람이 원본을 만드는 도구, 검증 증거, cook pipeline, release pipeline을 구분한다.

현재 의미:
- `SimLab/`: ValidationEvidence
- `EldenAssetPipeline/`: CookPipeline
- `WintersAssetConverter/`: CookPipeline
- `ChampionData/`: AuthoringTool 또는 GameDesignSource. 확인 필요
- `Bin/`, `Intermediate/`: GeneratedOutput
- `External/`: ExternalDependency

추가 후보:
- `Validation/`: `SimLab/`만으로 QA gate를 표현할 수 없을 때만 추가
- `Cook/`: 기존 pipeline과 converter가 cook 계약을 담을 수 없을 때만 추가
- `Release/`: package, patch, deploy manifest 계약이 생길 때만 추가

구조 규칙:
- 기존 `Tools/` 폴더명과 구조를 유지한다.
- tool output을 source처럼 직접 편집하지 않는다.
- 새 tool executable이 생길 때만 build graph 수정을 검토한다.
```

1-7. C:/Users/tnest/Desktop/Winters/Services/README.md

새 파일:

```md
# Services

원자: BackendState, LiveOpsContract

Services는 match 밖의 backend state와 live service 계약을 소유한다.

소유:
- account
- matchmaking
- profile
- store
- entitlement
- service migrations
- telemetry/live ops 계약이 backend와 분리되어야 할 때의 service boundary

소유하지 않음:
- in-match gameplay truth
- Client visual state
- Engine runtime primitive
- cooked asset source

구조 규칙:
- 기존 `Services/` 폴더명과 Go 구조를 유지한다.
- `Telemetry/`, `LiveOps/` 같은 새 폴더는 backend state와 별도 계약이 필요할 때만 추가한다.
```

1-8. C:/Users/tnest/Desktop/Winters/cmake/README.md

새 파일:

```md
# cmake

원자: BuildGraph

`cmake/`, `CMakeLists.txt`, `.vcxproj`, `.filters`, `Winters.sln`은 무엇을 어떻게 빌드하는지 정하는 실행 계약이다.

소유:
- build target
- source inclusion
- tool executable build
- generated output 경로
- cook, package, deploy step과의 연결

소유하지 않음:
- gameplay truth
- product presentation
- engine capability 의미
- authored data 원본

수정 기준:
- 새 `.h/.cpp`가 빌드에 들어간다.
- 새 library, executable, build target이 생긴다.
- 새 cook/package/deploy step이 build graph와 연결된다.
- CI나 release gate가 실제 실행 계약을 가져야 한다.

구조 규칙:
- 아키텍처 리팩터의 시작점으로 `cmake/`나 `.vcxproj`를 먼저 수정하지 않는다.
- source owner가 확정된 뒤 build graph를 따라 맞춘다.
```

2. 검증

미검증:
- README 추가 미실행.
- 빌드 미실행.
- 런타임 검증 미실행.

검증 명령:
- git diff --check
- Get-Content -Encoding UTF8 Client/README.md
- Get-Content -Encoding UTF8 Engine/README.md
- Get-Content -Encoding UTF8 Shared/README.md
- Get-Content -Encoding UTF8 Server/README.md
- Get-Content -Encoding UTF8 Data/README.md
- Get-Content -Encoding UTF8 Tools/README.md
- Get-Content -Encoding UTF8 Services/README.md
- Get-Content -Encoding UTF8 cmake/README.md

확인 필요:
- S1에서는 `.vcxproj`, `.vcxproj.filters`, `CMakeLists.txt`, `cmake/` build script를 수정하지 않는다.
- 새 C++ 파일이나 새 build target이 생기는 세션에서만 project inclusion을 확인한다.
- `Data/*.dat`, `Data/*.navgrid`의 편집 원본 위치를 확인한다.
- `Tools/ChampionData/`가 AuthoringTool인지 GameDesignSource인지 확인한다.
