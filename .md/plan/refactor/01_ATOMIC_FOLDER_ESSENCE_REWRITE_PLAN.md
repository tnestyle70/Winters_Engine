Session - Winters 폴더 원자를 편집 원본, 실행 계약, 산출물 단위로 재작성한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/.md/architecture/WINTERS_COMPANY_SCALE_FOLDER_ESSENCE.md

파일 전체를 아래로 교체:

````md
# Winters Atomic Folder Essence

Winters는 출시, 패치, 라이브 운영, 대규모 협업이 가능한 상용 게임/엔진 구조를 목표로 한다.

폴더의 본질은 사람 이름이나 팀 이름이 아니다. repo에 남는 본질은 셋뿐이다.

```text
편집 원본
실행 계약
산출물
```

어떤 항목이 이 셋 중 둘 이상이면 아직 덜 나눈 것이다.

## 판정 질문

```text
누가 의미를 고치는가?
무엇이 실행을 결정하는가?
무엇이 출시/패치/검증 산출물인가?
```

답이 하나로 떨어지지 않으면 폴더를 나눈다.

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

Data는 아래 원본으로 나뉘어야 한다.

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

Tools는 아래 실행 계약으로 나뉘어야 한다.

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

## Winters 목표 매핑

| 현재 | 목표 원자 |
|---|---|
| `Client/` | ProductPresentation |
| `EldenRingClient/` | ProductPresentation |
| `Engine/` | EngineCapability |
| `Shared/GameSim/` | GameplayTruth |
| `Server/` | AuthorityExecution |
| `Services/` | BackendState |
| `Data/Gameplay` | GameDesignSource |
| `Data/UI`, `Data/FX`, `Data/Placement` | PresentationDesignSource |
| `Data/Art` | ArtSource |
| `Data/Audio` | AudioSource |
| `Client/Bin/Resource` | RuntimeAsset |
| `EldenRingEditor/` | AuthoringTool |
| `Tools/Validation`, `Tools/SimLab` | ValidationEvidence |
| `Tools/Cook` | CookPipeline |
| `cmake/`, `Winters.sln`, `.vcxproj` | BuildGraph |
| `Tools/Release` | PackagePatchDeploy |
| `Services/Telemetry`, `Services/LiveOps` | LiveOpsContract |
| `Data/Publishing` | PublishingSource |
| `.md/`, `AGENTS.md`, `CLAUDE*.md` | Documentation |
| `Engine/External`, `Engine/ThirdPartyLib`, `Tools/External` | ExternalDependency |
| `out/`, `Bin/`, logs, captures | GeneratedOutput |

없는 목표 폴더는 다음 세션에서 만든다. 이번 문서는 이동을 실행하지 않는다.

## 금지

- ProductPresentation이 GameplayTruth를 만들지 않는다.
- EngineCapability가 제품 명사를 소유하지 않는다.
- AuthorityExecution이 visual success를 판정하지 않는다.
- BackendState가 in-match truth를 소유하지 않는다.
- Data fallback C++ table을 두 번째 truth로 만들지 않는다.
- GeneratedOutput을 편집 원본으로 쓰지 않는다.
- BuildGraph를 source owner로 착각하지 않는다.
- ExternalDependency를 Winters 의미 소유권으로 흡수하지 않는다.

## 완료 기준

- 모든 폴더는 최종 원자 하나로 설명된다.
- 한 폴더가 둘 이상의 원자를 가지면 다음 리팩터링 대상이다.
- source, runtime asset, generated output이 분리된다.
- 출시/패치/검증 산출물이 runtime source와 섞이지 않는다.
````

2. 검증

미검증:
- 문서 교체 미검증.
- 실제 폴더 이동 미실행.
- 빌드 미실행.

검증 명령:
- git diff --check
- Get-Content -Encoding UTF8 .md/architecture/WINTERS_COMPANY_SCALE_FOLDER_ESSENCE.md -TotalCount 180

확인 필요:
- `Data/Gameplay`, `Data/UI`, `Data/FX`, `Data/Placement`, `Data/Art`, `Data/Audio`, `Data/Publishing` 목표 폴더를 새로 만들지, 기존 Data 하위 구조에 매핑만 먼저 할지 확인.
- `Tools/Validation`, `Tools/Cook`, `Tools/Release` 목표 폴더를 새로 만들지, 기존 Tools 하위 구조에 매핑만 먼저 할지 확인.
- `Services/Telemetry`, `Services/LiveOps` 목표 폴더가 현재 backend 구조에 필요한지 확인.
