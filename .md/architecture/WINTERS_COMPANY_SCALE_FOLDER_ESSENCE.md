# Winters Company Scale Folder Essence

이 문서는 Winters를 출시, 패치, 라이브 운영, 대규모 협업이 가능한 상용 게임/엔진 구조로 키우기 위한 폴더 본질을 고정한다.

목표는 특정 회사의 내부 폴더명을 흉내 내는 것이 아니다. 대형 라이브 게임, 액션 RPG, 오픈월드 게임, 범용 엔진이 공통으로 요구하는 repo 원자를 Winters에 맞게 남기는 것이다.

## 규모 전제

Winters는 아래 규모를 견딜 수 있는 구조를 목표로 한다.

- 개발자, 기획자, 디자이너, 아티스트, 애니메이터, 사운드, QA, 마케팅, 운영, 빌드/릴리즈, 보안, 데이터 분석이 동시에 작업한다.
- 출시 후 패치, 핫픽스, 시즌 업데이트, 콘텐츠 추가, 서버 운영, 리플레이, QA 재현이 가능해야 한다.
- 한 팀의 편의를 위해 다른 팀의 편집 원본, 실행 계약, 산출물이 섞이면 안 된다.

## 구조 보존 원칙

이 리팩터링은 기존 폴더를 갈아엎지 않는다.

- 기존 폴더 이름은 유지한다.
- 기존 폴더 구조는 유지한다.
- 기존 파일 이동과 폴더 rename은 첫 선택지가 아니다.
- 새 폴더는 현재 구조 안에 담을 수 없는 원자가 실제로 생길 때만 추가한다.
- 새 폴더는 가능한 한 기존 루트 아래에 추가한다. 예: `Data/...`, `Tools/...`, `Services/...`.
- `.vcxproj`, `.vcxproj.filters`, `CMakeLists.txt`, `cmake/`는 source owner가 아니다. 새 C++ 파일, 새 build target, 새 cook/package 계약이 생길 때만 따라 수정한다.

즉, 목표는 새 이름으로 정리하는 것이 아니라 현재 Winters 구조에 원자 의미를 부여하는 것이다.

## Repo에 남는 세 본질

폴더의 본질은 팀 이름이나 직무 이름이 아니다. repo에 남는 본질은 셋뿐이다.

```text
편집 원본
실행 계약
산출물
```

한 폴더가 이 셋 중 둘 이상을 동시에 직접 소유하면 다음 리팩터링 대상이다.

## 판정 질문

모든 폴더와 파일은 아래 질문으로만 판정한다.

```text
누가 의미를 고치는가?
무엇이 실행을 결정하는가?
무엇이 출시/패치/검증 산출물인가?
```

답이 하나로 떨어지지 않으면 폴더를 새로 만들기 전에 먼저 의미를 더 작게 나눈다.

## 최종 원자

| 원자 | 본질 |
|---|---|
| ProductPresentation | 플레이어가 보는 제품별 경험 |
| EngineCapability | 제품을 모르는 실행 능력 |
| GameplayTruth | 결정론적 게임 규칙과 상태 변화 |
| AuthorityExecution | command를 받아 truth를 실행하고 배포 |
| BackendState | 계정, 매치, 상점, 프로필, 운영 상태 |
| GameDesignSource | 규칙, 수치, 보상, 모드, 밸런스 원본 |
| PresentationDesignSource | UI, FX, 카메라, 연출, 배치 의도 원본 |
| ArtSource | 모델, 텍스처, 애니메이션, 시네마틱 원본 |
| AudioSource | 음악, 효과음, 보이스, 믹스 원본 |
| RuntimeAsset | 런타임이 로드하는 cooked asset |
| AuthoringTool | 원본을 만들고 수정하는 도구 |
| ValidationEvidence | 테스트, 재현, golden case, QA gate |
| BuildGraph | 무엇을 어떻게 빌드하는가 |
| CookPipeline | 원본을 runtime asset으로 변환 |
| PackagePatchDeploy | 출시물, 패치, 배포 manifest |
| LiveOpsContract | telemetry, crash, live config, incident |
| PublishingSource | 스크린샷, 영상, store copy, press kit |
| Documentation | 행동 규칙, 구조 결정, 협업 계약 |
| ExternalDependency | 외부 dependency |
| GeneratedOutput | 빌드, 런타임, 로컬 산출물 |

이 목록 밖의 새 폴더는 만들지 않는다. 먼저 의미를 위 원자 중 하나로 바꾼다.

## 현재 폴더 의미

기존 폴더명은 유지하고, 의미만 아래 원자로 수렴시킨다.

| 현재 항목 | 원자 |
|---|---|
| `Client/` | ProductPresentation |
| `EldenRingClient/` | ProductPresentation |
| `Engine/` | EngineCapability |
| `Shared/GameSim/` | GameplayTruth |
| `Shared/Network/` | AuthorityExecution과 ProductPresentation 사이의 protocol contract |
| `Shared/Replay/` | ValidationEvidence 또는 RuntimeAsset. 용도별 확인 필요 |
| `Shared/Schemas/` | 실행 계약 schema |
| `Server/` | AuthorityExecution |
| `Services/` | BackendState |
| `Data/Gameplay/` | GameDesignSource |
| `Data/GameModes/` | GameDesignSource |
| `Data/LoL/FX/` | PresentationDesignSource |
| `Data/*.dat`, `Data/*.navgrid` | RuntimeAsset 후보. 편집 원본 위치 확인 필요 |
| `Shaders/` | EngineCapability와 RuntimeAsset 사이의 shader source |
| `EldenRingEditor/` | AuthoringTool |
| `Tools/SimLab/` | ValidationEvidence |
| `Tools/EldenAssetPipeline/` | CookPipeline |
| `Tools/WintersAssetConverter/` | CookPipeline |
| `Tools/ChampionData/` | AuthoringTool 또는 GameDesignSource. 확인 필요 |
| `Tools/Bin/`, `Tools/Intermediate/` | GeneratedOutput |
| `Tools/External/` | ExternalDependency |
| `cmake/`, `Winters.sln`, `.vcxproj` | BuildGraph |
| `Engine/External/`, `Engine/ThirdPartyLib/` | ExternalDependency |
| `Engine/Bin/`, `EngineSDK/`, `out/`, `Profiles/`, `Replay/`, logs, captures | GeneratedOutput |
| `.md/`, `AGENTS.md`, `CLAUDE*.md`, `winters-skills/` | Documentation |

## 추가 폴더 원칙

새 폴더는 현재 구조가 원자를 담을 수 없을 때만 만든다.

추가 후보:
- `Data/Art/`: art source가 repo 원본으로 들어올 때만 추가한다.
- `Data/Audio/`: audio source가 repo 원본으로 들어올 때만 추가한다.
- `Data/Publishing/`: marketing/public asset 원본이 repo에 들어올 때만 추가한다.
- `Tools/Validation/`: `Tools/SimLab/`로 QA gate를 표현할 수 없을 때만 추가한다.
- `Tools/Cook/`: 기존 asset pipeline과 converter가 cook 계약을 담을 수 없을 때만 추가한다.
- `Tools/Release/`: package, patch, deploy manifest 계약이 생길 때만 추가한다.
- `Services/Telemetry/`, `Services/LiveOps/`: backend state와 live ops 계약을 분리해야 할 때만 추가한다.

추가하지 않는 것:
- 단순 정리를 위한 빈 폴더
- 이름이 더 좋아 보인다는 이유의 rename
- build tool 편의를 위한 source owner 폴더

## Client

```text
Client = ProductPresentation
```

Client는 결과를 만들지 않는다.

Client가 하는 일:
- 입력을 제품 의도로 바꾼다.
- snapshot/event/cue를 view state로 바꾼다.
- view state를 화면, 소리, UI, 디버그 표시로 바꾼다.

Client가 하지 않는 일:
- 최종 피해, HP, 쿨타임, 승패 판정
- Engine runtime primitive 소유
- backend state 소유
- build/cook/package 소유

## Engine

```text
Engine = EngineCapability
```

Engine은 게임 명사를 모른다.

Engine이 하는 일:
- platform, window, frame loop
- RHI, renderer, resource, audio, input, UI primitive
- ECS/storage/scheduler 같은 범용 runtime primitive
- profiler, debug, capture primitive

Engine이 하지 않는 일:
- champion, skill, boss, item, quest, minion, turret, match rule
- 제품 UI/HUD rule
- server authority
- 기획 수치
- QA/marketing/live ops 산출물

## Server

```text
Server = AuthorityExecution
```

Server는 보여주는 곳이 아니라 판정하는 곳이다.

Server가 하는 일:
- command intake
- GameTruth tick
- snapshot/event/cue/replay record emission
- room/session authority
- bot command generation
- lag compensation, anti-cheat gate

Server가 하지 않는 일:
- client visual success 판정
- UI, animation, camera, local feel
- account/payment/profile backend state
- 기획 원본 수치 편집

## GameSim

```text
Shared/GameSim = GameplayTruth
```

GameSim은 화면을 모른다.

GameSim이 하는 일:
- command 해석
- rule 적용
- state transition
- event/cue 생성

GameSim이 하지 않는 일:
- renderer, UI, ImGui, DX type include
- mesh, texture, shader, animation file ownership
- backend account state

## Data

Data는 한 원자가 아니다.

기존 `Data/` 구조는 유지한다. 다만 의미는 아래 원자로 분리해서 본다.

```text
GameDesignSource
PresentationDesignSource
ArtSource
AudioSource
RuntimeAsset
LiveOpsContract
PublishingSource
```

같은 champion이라도 gameplay 수치와 visual asset은 같은 원본이 아니다.

## Tools

Tools는 한 원자가 아니다.

기존 `Tools/` 구조는 유지한다. 다만 의미는 아래 실행 계약으로 분리해서 본다.

```text
AuthoringTool
ValidationEvidence
CookPipeline
PackagePatchDeploy
```

도구 산출물은 source가 아니다.

## Build

```text
Build = BuildGraph + CookPipeline + PackagePatchDeploy
```

빌드는 owner가 아니다. 빌드는 source를 출시 가능한 산출물로 바꾸는 계약이다.

`.vcxproj`, `.filters`, `CMakeLists.txt`, `cmake/` 수정은 시작점이 아니다.

수정하는 경우:
- 새 `.h/.cpp`가 빌드에 들어가야 한다.
- 새 target, library, tool executable이 생긴다.
- cook/package/deploy step이 build graph와 연결된다.
- CI나 release gate가 실제로 실행 계약을 가져야 한다.

## QA

```text
QA = ValidationEvidence
```

QA는 감상이 아니라 재현 가능한 증거다.

QA가 남기는 것:
- repro input
- replay/golden case
- test result
- certification gate

## Marketing

```text
Marketing = PublishingSource
```

Marketing은 gameplay truth를 바꾸지 않는다.

Marketing이 남기는 것:
- capture source
- trailer/screenshot
- store copy
- press kit

## Operations

```text
Operations = LiveOpsContract
```

Operations는 출시 후 상태를 다룬다.

Operations가 남기는 것:
- telemetry schema
- dashboard query
- crash/incident record
- live config

## 진행 순서

디테일 리팩터는 아래 순서로 진행한다.

1. 기존 루트 폴더에 원자 계약을 문서로 고정한다.
2. `Data/`에서 편집 원본과 runtime asset을 구분한다.
3. `Tools/`에서 authoring, validation, cook, release 계약을 구분한다.
4. `Shared/GameSim`, `Server`, `Client`의 truth 흐름을 계약 기준으로 점검한다.
5. 새 C++ 파일이나 새 build target이 생긴 뒤에만 `.vcxproj`, `.filters`, `CMakeLists.txt`, `cmake/`를 수정한다.
6. 마지막에 package, patch, deploy, telemetry, QA gate를 build/release 계약으로 연결한다.

## 금지

- 기존 폴더 rename을 리팩터링의 출발점으로 삼지 않는다.
- 기존 구조 이동을 정리 작업처럼 먼저 하지 않는다.
- ProductPresentation이 GameplayTruth를 만들지 않는다.
- EngineCapability가 제품 명사를 소유하지 않는다.
- AuthorityExecution이 visual success를 판정하지 않는다.
- BackendState가 in-match truth를 소유하지 않는다.
- Data fallback C++ table을 두 번째 truth로 만들지 않는다.
- GeneratedOutput을 편집 원본으로 쓰지 않는다.
- BuildGraph를 source owner로 착각하지 않는다.
- ExternalDependency를 Winters 의미 소유권으로 흡수하지 않는다.

## 완료 기준

- 기존 폴더명과 기존 구조가 보존된다.
- 추가 폴더는 빠진 원자가 실제로 필요할 때만 생긴다.
- 모든 폴더는 최종 원자 하나로 설명된다.
- 한 폴더가 둘 이상의 원자를 가지면 다음 리팩터링 대상이다.
- source, runtime asset, generated output이 분리된다.
- 출시/패치/검증 산출물이 runtime source와 섞이지 않는다.
