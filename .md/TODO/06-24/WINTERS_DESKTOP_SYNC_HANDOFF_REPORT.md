# Winters Desktop Sync Handoff Report

작성일: 2026-06-24

## 목적

노트북에서 진행한 Winters 코드, 데이터, 문서, 계획서, 결과 보고서를 데스크탑에서도 그대로 이어받기 위한 동기화 기준을 정리한다.

핵심 원칙:

- 코드/문서/데이터 정의/생성 소스는 Git으로 공유한다.
- 빌드 산출물, 캐시, 로컬 실행 로그성 파일은 원칙적으로 Git에 넣지 않는다.
- 단, 이번 동기화에서는 남아 있던 `limgrave_*` 상태 파일도 데스크탑 재현용 스냅샷으로 포함한다.
- 런타임 리소스는 Git이 아니라 `Client/Bin/Resource/` 폴더를 별도로 동기화한다.

## 원격 기준

기준 브랜치:

- `main`

노트북에서 이미 푸시된 핵심 기준 커밋:

- `18ca031 Implement data-driven cutover and Yone AI tactics`

이 커밋에는 다음 흐름이 포함되어 있다.

- Data Driven cutover 진행분
- LoL server-private/client-public/shared-contract JSON 정의
- gameplay definition pack/query
- server gameplay definition generated source
- client visual definition generated source
- spawn/world bootstrap/server authority 분리 진행분
- Scene_InGame 분리 진행분
- RHI/Elden editor 진행 문서 및 일부 코드
- Y0 champion combat function pointer registry
- Y1 Yone AI tactics
- Yone E stage-2 return command fix
- 관련 결과 보고서

## 데스크탑에서 받을 것

데스크탑에서는 아래 순서로 받는다.

```powershell
cd C:\Users\tnest\Desktop\Winters
git fetch origin
git checkout main
git pull --ff-only origin main
```

정상이라면 코드/문서/진행 보고서는 Git만으로 동기화된다.

## Resource 동기화

`Client/Bin/Resource/`는 `.gitignore` 정책상 Git에 들어가지 않는다.

현재 정책:

```gitignore
/Client/Bin/Resource/
/Client/Bin/Resource.zip
```

따라서 데스크탑에서 게임을 실행하려면 노트북의 아래 폴더를 같은 경로로 맞춘다.

```text
C:\Users\tnest\Desktop\Winters\Client\Bin\Resource\
```

동기화 후 확인:

```powershell
Test-Path C:\Users\tnest\Desktop\Winters\Client\Bin\Resource
```

주의:

- `Client/Bin/Debug*/Resource`나 `Client/Bin/Release*/Resource`에 별도 복사본을 만들지 않는다.
- 런타임 리소스 기준 위치는 항상 `Client/Bin/Resource/`다.
- Resource만 맞으면 Git pull 이후 데스크탑에서도 같은 코드/데이터/문서 상태로 이어갈 수 있다.

## 노트북에서 검증된 파이프라인

최근 검증:

```powershell
git diff --check
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" Shared\GameSim\Include\GameSim.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
powershell -ExecutionPolicy Bypass -File Tools\LoLData\Verify-LoLDataDrivenPipeline.ps1
```

결과:

- `git diff --check`: 통과
- `GameSim Debug x64`: 오류 0
- `Server Debug x64`: 오류 0
- `Verify-LoLDataDrivenPipeline.ps1`: 최종 `PASS`
- SimLab same-seed replay OK
- SimLab seed sensitivity OK

기존 경고:

- Server/Client 빌드 중 EngineSDK `ISystem` DLL interface 계열 `C4275` 경고가 일부 출력된다.
- 이번 동기화 작업의 신규 실패는 아니다.

## 이어서 진행할 위치

다음 개발은 아래 문서들을 기준으로 이어간다.

- `.md/plan/collab-pipeline/07_DATA_DRIVEN_FULL_CUTOVER_CODEX_REQUIREMENTS.md`
- `.md/plan/collab-pipeline/09_DATA_DRIVEN_REMAINING_STRUCTURE_DESIGN.md`
- `.md/TODO/06-24/WINTERS_Y0_Y1_YONE_AI_REGISTRY_IMPLEMENTATION_REPORT.md`
- `.md/TODO/06-24/WINTERS_DATA_DRIVEN_P3_YONE_VIEGO_ZED_SKILL_EFFECT_PARAM_REPORT.md`

## 데스크탑에서 상태 확인

데스크탑에서 pull 이후:

```powershell
git status --short
git log -3 --oneline
```

기대 상태:

- `git status --short`는 로컬 수정이 없어야 한다.
- `git status --ignored --short Client\Bin\Resource`를 보면 Resource는 `!! Client/Bin/Resource/`처럼 ignored로 보일 수 있다.

## 결론

데스크탑 동기화의 본질은 두 가지다.

1. Git으로 `main` 최신 상태를 받는다.
2. Git 밖의 `Client/Bin/Resource/`만 별도 복사/동기화한다.

이 두 가지가 맞으면 데스크탑에서도 노트북의 진행 문서와 코드 상태를 그대로 공유하고 이어서 개발할 수 있다.
