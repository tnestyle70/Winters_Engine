# Winters Engine Build Tools / CI/CD Concepts

> 2026-05-25 update: `DX12SmokeHost` has been removed from `Winters.sln`; active build targets are Engine, Client, Server, and WintersAssetConverter. DX12 remains a backend/configuration path, not a standalone executable project.

이 문서는 Winters Engine에 빌드 도구, 검증 게이트, CI/CD를 구축하기 전에 공유해야 할 개념 지도다. 목표는 "내 컴퓨터에서는 된다"를 "누가, 어디서, 언제 돌려도 같은 절차로 검증된다"로 바꾸는 것이다.

## 1. 전체 그림

빌드 파이프라인은 소스 파일을 실행 가능한 산출물로 바꾸는 공장이다.

```text
Source / Schema / Shader / Asset
  -> codegen
  -> compile
  -> link
  -> deploy runtime files
  -> validate / smoke
  -> package
  -> publish artifact or release
```

Winters의 현재 흐름은 대략 다음 계층으로 나뉜다.

```text
Winters.sln / *.vcxproj
  -> Engine / Server / Client / Tools build
  -> UpdateLib.bat copies Engine public SDK and runtime DLLs
  -> Shared/Schemas/run_codegen.bat generates FlatBuffers code
  -> Tools/WintersAssetConverter converts raw assets to Winters formats
  -> Client/Server consume generated code, EngineSDK, runtime DLLs
```

CI/CD는 이 흐름을 자동화하는 시스템이다. CI는 변경이 들어올 때마다 빌드와 검증을 수행한다. CD는 검증된 산출물을 패키징하고, 필요하면 릴리스나 배포 환경으로 승격한다.

## 2. 빌드 도구의 기본 원리

컴파일러는 `.cpp`를 목적 파일로 바꾼다. 링커는 목적 파일과 라이브러리를 묶어서 `.exe`, `.dll`, `.lib` 같은 산출물을 만든다.

빌드 시스템은 어떤 파일이 어떤 파일에 의존하는지 그래프로 관리한다. 한 파일이 바뀌면 그 파일에 의존하는 단계만 다시 실행하는 것이 증분 빌드다.

MSBuild는 Visual Studio의 `.sln`, `.vcxproj`를 읽어서 빌드 그래프를 실행한다. Winters는 현재 `Winters.sln` 안에 `Engine`, `Server`, `Client`, `WintersAssetConverter`, `DX12SmokeHost` 프로젝트를 가지고 있다.

CMake는 빌드 시스템을 직접 실행하는 도구라기보다, Ninja나 Visual Studio 프로젝트를 생성하는 메타 빌드 시스템이다. 현재 Winters의 CMake는 `WintersEngine` 대상부터 부트스트랩되어 있고, 전체 Client/Server/Tools 그래프는 아직 완성 전 단계로 보는 것이 안전하다.

Ninja는 빠른 증분 빌드를 위해 만들어진 실행기다. CMake + Ninja는 CI에서 빠르고 예측 가능하게 쓰기 좋다. 다만 Visual Studio 프로젝트와 완전히 같은 빌드 그래프가 되려면 모든 include, define, link, post-build, codegen 규칙이 CMake에 옮겨져야 한다.

## 3. Configuration과 Platform

Configuration은 빌드 성격이다. Winters에는 `Debug`, `Release`, `Debug-DX12`, `Release-DX12`가 있다.

Platform은 CPU/ABI 대상이다. 현재 솔루션 기준은 `x64`다.

Debug는 디버깅 심볼, 낮은 최적화, 더 많은 검사에 유리하다. Release는 최적화와 배포 산출물 확인에 유리하다. DX12 구성은 렌더 백엔드 전환을 검증하는 별도 축이다.

CI 매트릭스는 이 축을 여러 조합으로 돌리는 방식이다. 예를 들어 `Debug`, `Release`, `Debug-DX12`를 병렬로 돌리면 기본 디버그 안정성, 릴리스 링크 안정성, DX12 컴파일 안정성을 한 번에 확인할 수 있다.

## 4. Code Generation

Codegen은 사람이 직접 쓰지 않는 소스 파일을 생성하는 단계다. Winters에서는 `Shared/Schemas/run_codegen.bat`가 FlatBuffers 스키마에서 C++/Go generated 파일을 만든다.

Codegen이 빌드보다 먼저 실행되어야 하는 이유는 간단하다. 컴파일러는 generated header가 이미 존재한다고 가정한다. 스키마만 바뀌고 generated 파일이 갱신되지 않으면 컴파일은 우연히 통과해도 런타임 계약이 어긋날 수 있다.

좋은 codegen 게이트는 다음을 만족한다.

- 스키마 변경 시 generated 파일이 반드시 갱신된다.
- 생성기가 없으면 명확한 오류로 실패한다.
- CI와 로컬이 같은 명령을 사용한다.
- generated 결과가 repo에 커밋되는 정책인지, CI에서만 생성되는 정책인지 분명하다.

현재 Winters는 generated 파일이 저장소에 존재하므로, CI에서는 codegen을 실행한 뒤 `git diff --check` 또는 별도 diff 확인으로 stale generated 파일을 잡는 방향이 좋다.

## 5. SDK Deploy와 Runtime Deploy

`UpdateLib.bat`는 Engine 산출물을 `EngineSDK`와 Client runtime 위치로 복사한다. 이 단계는 단순 복사가 아니라 빌드 계약의 일부다.

Engine public header는 `EngineSDK/inc`로 동기화된다. Engine DLL/PDB와 ThirdParty DLL은 Client output으로 복사된다. Server도 post-build에서 Engine DLL과 필요한 ThirdParty DLL을 output으로 복사한다.

따라서 CI에서 Engine만 빌드하고 끝내면 "컴파일은 됐지만 실행 산출물은 불완전한" 상태가 생길 수 있다. 검증 단계는 compile, link, deploy copy가 모두 통과했는지 확인해야 한다.

## 6. Asset Cook / Asset Convert

게임 엔진에서 asset pipeline은 코드 빌드만큼 중요하다. 원본 FBX/GLB/PNG 등을 런타임이 빠르게 읽을 수 있는 cooked format으로 바꾸는 과정이 asset cook이다.

Winters에는 `Tools/WintersAssetConverter`와 `Tools/convert_all_assets.ps1`가 있다. 이 흐름은 다음 위험을 관리해야 한다.

- converter 자체가 빌드되어 있어야 한다.
- raw asset이 없을 수 있는 개발자/CI 환경을 구분해야 한다.
- source asset timestamp, converter timestamp, output timestamp를 비교해 증분 변환해야 한다.
- 실패한 asset 이름과 exit code가 로그에 남아야 한다.
- CI 기본 게이트는 작은 smoke set부터 시작하고, 전체 cook은 별도 full job이나 self-hosted runner로 분리하는 편이 안전하다.

## 7. CI의 기본 단위

Workflow는 자동화 파일이다. GitHub Actions 기준으로 `.github/workflows/*.yml`에 둔다.

Trigger는 언제 실행할지 정한다. 대표적으로 pull request, push, tag, 수동 실행이 있다.

Runner는 작업이 실행되는 머신이다. Winters는 Windows/MSVC 프로젝트라 기본 CI runner는 Windows가 맞다. hosted runner는 관리 부담이 낮지만 대형 assets나 GPU smoke에는 약하다. self-hosted runner는 빠르고 강력하지만 보안, 청소, 격리, 업데이트를 직접 책임져야 한다.

Job은 runner 하나에서 실행되는 작업 묶음이다. Step은 job 안의 한 명령이다. Matrix는 같은 job을 여러 구성으로 복제하는 기능이다.

Artifact는 job 결과물을 저장하는 것이다. 빌드 로그, 패키지 zip, smoke screenshot, crash dump 같은 것은 artifact로 남기면 디버깅이 쉬워진다.

Cache는 다음 실행을 빠르게 하기 위해 중간 산출물을 재사용하는 것이다. C++ 빌드 캐시는 잘못 설계하면 stale 산출물을 숨길 수 있으므로 처음부터 과하게 쓰지 않는다. 안정화 후 CMake/Ninja build tree, tool download cache, asset cook cache 순서로 도입하는 것이 좋다.

## 8. CD의 기본 단위

CD는 검증된 산출물을 다음 단계로 승격하는 일이다. 여기서 "배포"는 반드시 라이브 서버 반영만 뜻하지 않는다.

Winters의 첫 CD는 release candidate artifact를 만드는 정도가 적절하다.

```text
CI green
  -> package Client/Server/Tools/Engine symbols/logs
  -> upload artifact
  -> optional tag build
  -> optional GitHub Release draft
  -> later deploy to internal test machine or storage
```

실제 배포 대상이 정해지기 전에는 secrets, remote host, cloud bucket, Steam branch, launcher update 같은 부분을 확정하지 않는다. 이건 모르는 척 넘길 영역이 아니라 `CONFIRM_NEEDED`로 남겨야 하는 영역이다.

## 9. 검증 게이트의 단계

가장 싼 검증부터 가장 비싼 검증으로 쌓는다.

```text
format/static sanity
  -> codegen
  -> compile/link
  -> runtime deploy copy
  -> smoke executable launch
  -> asset validation
  -> multiplayer/server smoke
  -> packaging
  -> release promotion
```

초기 CI는 매 PR마다 `git diff --check`, FlatBuffers codegen, MSBuild Debug/Release, DX12 compile smoke 정도를 돌리는 것이 좋다.

무거운 full asset cook, long network simulation, performance regression은 PR마다 돌리면 개발 속도를 죽일 수 있다. nightly 또는 수동 workflow로 분리하는 편이 낫다.

## 10. 좋은 빌드 도구의 조건

로컬과 CI가 같은 명령을 사용해야 한다. CI 전용 YAML 안에만 중요한 빌드 지식이 있으면 로컬에서 재현하기 어렵다.

한 번 실패하면 어디서 실패했는지 로그가 분명해야 한다. `Build failed`보다 `FlatBuffers codegen failed`, `Server Release link failed`, `Asset cook missing converter`가 훨씬 좋다.

빌드 스크립트는 기본값이 안전해야 한다. raw asset 전체 변환처럼 오래 걸리거나 환경 의존적인 작업은 명시적으로 켜게 한다.

빌드 산출물은 `out/`이나 각 프로젝트의 기존 `Bin/`처럼 예측 가능한 곳에 둔다. 임시 파일과 release artifact를 섞지 않는다.

## 11. 보안과 재현성

CI는 저장소 권한을 가진 자동 실행 환경이다. 그래서 권한을 작게 잡아야 한다.

GitHub Actions에서는 workflow나 job에 `permissions: contents: read`처럼 최소 권한을 명시한다. release 생성 같은 단계만 `contents: write`가 필요하다.

외부 action은 버전 태그보다 commit SHA pinning이 더 강하다. 다만 초안 단계에서는 가독성을 위해 버전 태그를 쓰고, release 운영 단계에서 SHA pinning으로 굳히는 전략도 가능하다.

Secrets는 PR에서 함부로 노출되면 안 된다. fork PR, `pull_request_target`, 사용자 입력을 shell에 그대로 넣는 패턴은 특히 조심한다.

## 12. Winters에 맞는 구축 순서

첫째, 로컬 단일 진입점을 만든다. `Tools/Build/Invoke-WintersBuild.ps1`와 `Tools/Build/Invoke-WintersVerify.ps1` 같은 wrapper가 필요하다.

둘째, 기존 `Winters.sln`/MSBuild를 CI의 1차 진실로 삼는다. 이미 F5 흐름과 post-build가 여기에 있으므로 첫 CI 성공률이 높다.

셋째, CMake/Ninja 전체 그래프를 천천히 확장한다. 현재 CMake는 Engine 중심이므로 Server/Client/Tools까지 무리하게 한 번에 옮기면 빌드 시스템 리팩터가 본 작업을 잡아먹을 수 있다.

넷째, artifact packaging을 만든다. Client/Server/Tools binaries, runtime DLLs, PDB, logs를 묶어 재현 가능한 zip으로 남긴다.

다섯째, 실제 deploy target을 정한 뒤 CD를 붙인다. 내부 QA 폴더, 원격 Windows 테스트 머신, object storage, GitHub Release, Steam branch 중 무엇인지에 따라 보안과 절차가 달라진다.

## 13. 심화 주제

Self-hosted runner는 대형 Windows C++ 프로젝트에서 빌드 시간을 크게 줄일 수 있다. 하지만 runner 머신은 pull request 코드를 실행한다. 격리, workspace cleanup, 권한, 네트워크 접근 제한이 없으면 위험하다.

Distributed build는 Incredibuild, FASTBuild, SN-DBS 같은 도구로 C++ 컴파일을 여러 머신에 분산하는 방식이다. Winters가 커지면 고려할 수 있지만, 초기에 CI 안정성보다 먼저 도입할 이유는 적다.

Artifact promotion은 "같은 commit에서 만든 산출물"을 dev, QA, release 단계로 승격하는 방식이다. 단계마다 다시 빌드하면 소스는 같아도 산출물이 달라질 수 있다.

Build provenance는 어떤 commit, 어떤 toolchain, 어떤 runner image, 어떤 third-party version으로 만들었는지 기록하는 것이다. 나중에 crash dump를 받았을 때 PDB와 commit을 찾는 데 매우 중요하다.

Flaky build는 코드 문제가 아니라 환경/타이밍 때문에 가끔 실패하는 빌드다. flaky를 방치하면 팀은 CI를 믿지 않게 된다. 실패 로그, 재시도 정책, 격리된 test data, 시간 의존 제거가 필요하다.

## 14. 현재성 참고

CI 문법과 hosted runner 이미지는 시간이 지나며 바뀐다. 2026-05-21 기준 계획을 세울 때는 다음 공식 문서를 기준으로 삼았다.

- GitHub Actions workflow syntax: https://docs.github.com/en/actions/reference/workflows-and-actions/workflow-syntax
- GitHub Actions runner images: https://github.com/actions/runner-images
- GitHub dependency caching reference: https://docs.github.com/en/actions/reference/workflows-and-actions/dependency-caching
- actions/checkout: https://github.com/actions/checkout
- actions/upload-artifact: https://github.com/actions/upload-artifact
- microsoft/setup-msbuild: https://github.com/microsoft/setup-msbuild
- MSBuild command line reference: https://learn.microsoft.com/en-us/visualstudio/msbuild/msbuild-command-line-reference

