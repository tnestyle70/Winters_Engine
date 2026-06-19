# S1 Folder Contract Code Concept

S1은 기존 Winters 폴더 이름과 구조를 유지한 채, 각 루트 폴더의 소유권을 가장 작은 협업 계약으로 고정하는 단계다.

S1의 구현 코드는 runtime C++가 아니다. S1의 구현 코드는 각 루트 폴더에 배치한 `README.md`다.

이 문서는 S1이 무엇을 해결하는지, 왜 `.vcxproj`나 `cmake`가 시작점이 아닌지, 그리고 각 `README.md`가 이후 코드 작성에서 어떤 경계로 작동하는지 정리한다.

## S1의 본질

S1의 본질은 폴더 이동이 아니다.

S1의 본질:
- 기존 폴더 이름을 유지한다.
- 기존 폴더 구조를 유지한다.
- 각 루트 폴더가 소유하는 원자를 하나로 고정한다.
- 각 루트 폴더가 소유하지 않는 것을 명시한다.
- 새 폴더와 build graph 수정이 필요한 조건을 늦춘다.

S1에서 하지 않는 것:
- 기존 폴더 rename
- 기존 파일 대규모 이동
- `.vcxproj`, `.filters`, `CMakeLists.txt` 선행 수정
- runtime behavior 변경
- EngineSDK 직접 수정

S1은 코드가 움직이기 전에 사람이 같은 기준으로 판단하게 만드는 단계다.

## 구현 단위

S1의 구현 단위는 아래 8개 파일이다.

| 파일 | 원자 |
|---|---|
| `Client/README.md` | ProductPresentation |
| `Engine/README.md` | EngineCapability |
| `Shared/README.md` | GameplayTruth, protocol contract, replay/schema contract |
| `Server/README.md` | AuthorityExecution |
| `Data/README.md` | GameDesignSource, PresentationDesignSource, ArtSource, AudioSource, RuntimeAsset, LiveOpsContract, PublishingSource |
| `Tools/README.md` | AuthoringTool, ValidationEvidence, CookPipeline, PackagePatchDeploy |
| `Services/README.md` | BackendState, LiveOpsContract |
| `cmake/README.md` | BuildGraph |

각 파일은 같은 구조를 가진다.

```text
원자
소유
소유하지 않음
구조 규칙
```

이 네 줄기가 S1의 코드다.

## 왜 README가 코드인가

S1에서 `README.md`는 설명문이 아니라 변경 게이트다.

새 코드를 넣기 전에 질문한다.

```text
이 변경은 해당 폴더의 원자에 속하는가?
이 변경은 README의 '소유'에 들어가는가?
이 변경은 README의 '소유하지 않음'을 침범하는가?
새 폴더나 build graph 수정 없이 기존 구조 안에서 표현 가능한가?
```

답이 흔들리면 코드를 쓰기 전에 원자를 다시 나눈다.

## Client 구현 계약

파일: `Client/README.md`

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

코드 판단:
- 입력, 카메라, 보간, 애니메이션, FX playback, HUD는 `Client/`에 둘 수 있다.
- 피해량, 쿨타임, 승패, 최종 위치 판정은 `Client/`에 두면 안 된다.
- Client가 필요한 것은 truth가 아니라 presentation state다.

## Engine 구현 계약

파일: `Engine/README.md`

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

코드 판단:
- renderer, resource, RHI, profiler primitive는 `Engine/`에 둘 수 있다.
- champion, skill, item, boss 같은 제품 명사는 `Engine/`에 두면 안 된다.
- Engine public header를 바꾸면 `EngineSDK/inc`를 직접 고치지 않고 SDK 동기화 필요 여부만 검토한다.

## Shared 구현 계약

파일: `Shared/README.md`

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

코드 판단:
- deterministic rule, component, command, snapshot schema는 `Shared/`에 둘 수 있다.
- `Shared/GameSim`은 Engine, Renderer, UI, ImGui, DX type을 include하면 안 된다.
- `Shared/Network`와 `Shared/Replay`는 truth 생성자가 아니라 전달과 재현 계약이다.

## Server 구현 계약

파일: `Server/README.md`

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

코드 판단:
- command intake, room authority, GameSim tick, snapshot emission은 `Server/`에 둘 수 있다.
- 서버가 animation 성공, camera feel, UI 상태를 판단하면 안 된다.
- 계정, 결제, 상점, 프로필은 `Services/` 경계다.

## Data 구현 계약

파일: `Data/README.md`

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

코드 판단:
- 기획자가 바꿔야 하는 수치와 규칙은 `Data/Gameplay` 또는 `Data/GameModes`에 둔다.
- FX, UI, placement 의도는 gameplay truth가 아니라 presentation source다.
- `.dat`, `.navgrid`는 runtime asset 후보라서 편집 원본인지 산출물인지 S2에서 판정한다.

## Tools 구현 계약

파일: `Tools/README.md`

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

코드 판단:
- converter, importer, validator, release packager는 `Tools/`에 둘 수 있다.
- tool output은 source가 아니다.
- `Tools/ChampionData/`는 S2/S3에서 AuthoringTool인지 GameDesignSource인지 확인한다.

## Services 구현 계약

파일: `Services/README.md`

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

코드 판단:
- account, matchmaking, profile, store, entitlement는 `Services/`에 둔다.
- match 안의 HP, 위치, 스킬 결과는 `Services/`에 두면 안 된다.
- live ops가 backend state와 분리될 정도로 커질 때만 새 service boundary를 만든다.

## BuildGraph 구현 계약

파일: `cmake/README.md`

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

코드 판단:
- `.vcxproj`, `.filters`, `CMakeLists.txt`, `cmake/`는 source owner가 아니다.
- 새 C++ 파일, 새 tool executable, 새 target이 생긴 뒤에만 수정한다.
- build graph는 아키텍처를 결정하지 않고 확정된 소유권을 실행 가능하게 만든다.

## S1에서 코드 작성 시 판정 규칙

새 코드를 작성하기 전에는 아래 순서로 판단한다.

```text
1. 이 변경은 runtime behavior인가, 협업 계약인가?
2. runtime behavior라면 truth, presentation, capability, backend state 중 무엇인가?
3. 기존 루트 README의 '소유'에 들어가는가?
4. '소유하지 않음'을 침범하지 않는가?
5. 기존 폴더 구조 안에서 표현 가능한가?
6. 새 `.h/.cpp`, 새 executable, 새 target이 없으면 build graph를 건드리지 않는다.
```

## S1 완료 기준

S1이 완료됐다는 기준:
- 루트 폴더마다 원자 계약이 있다.
- 코드 작성 전 owner를 판단할 수 있다.
- 기존 폴더명과 구조를 유지한다.
- 새 폴더는 실제 원자가 기존 구조에 담기지 않을 때만 추가한다.
- `.vcxproj`, `.filters`, `CMakeLists.txt`, `cmake/` 수정은 source owner 확정 뒤로 밀린다.

## S1 이후 다음 단계

S2는 `Data/`에서 시작한다.

S2의 핵심 질문:

```text
이 파일은 사람이 고치는 편집 원본인가?
이 파일은 런타임이 읽는 cooked/runtime asset인가?
이 파일은 도구가 만든 산출물인가?
```

S2에서 먼저 볼 항목:
- `Data/Gameplay/`
- `Data/GameModes/`
- `Data/LoL/FX/`
- `Data/*.dat`
- `Data/*.navgrid`
- `Tools/ChampionData/`

S2 전에는 `.vcxproj`, `.filters`, `CMakeLists.txt`, `cmake/`를 먼저 수정하지 않는다.
