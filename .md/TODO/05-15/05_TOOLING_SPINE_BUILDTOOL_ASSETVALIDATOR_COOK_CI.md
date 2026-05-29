# Tooling Spine BuildTool AssetValidator Cook CI

Session - BuildTool, AssetValidator, Cook step, CI build로 Winters의 손관리 빌드/에셋 파이프라인을 줄인다.

## 1. 반영해야 하는 코드

### 1-1. C:/Users/user/Desktop/Winters/Tools/WintersBuildTool

새 파일:
- module manifest를 읽고 `.sln`, `.vcxproj`, `.vcxproj.filters`를 생성하는 tool을 만든다.

반영:
- MVP 입력은 `.toolkit/module-manifest` 또는 신규 `*.Module.json` 중 하나로 고정한다.
- target은 `Client`, `Server`, `Engine`, `Tools`를 우선 지원한다.
- generated project는 기존 손관리 project와 diff 비교가 가능해야 한다.

확인 필요:
- C#으로 만들지 C++로 만들지 결정한다. 빠른 MVP는 C# 또는 Go가 유리하다.

### 1-2. C:/Users/user/Desktop/Winters/Tools/WintersBuildTool/ModuleRules

새 파일:
- module dependency, include path, library path, definitions를 선언하는 schema를 만든다.

반영:
- Runtime module이 Editor module을 include하지 못하도록 dependency 방향을 검증한다.
- Server target에서 UI/Render/Audio include가 들어오는 경우 경고 또는 실패 처리한다.
- `EngineSDK/inc`는 generated output으로 간주하고 source input에서 제외한다.

### 1-3. C:/Users/user/Desktop/Winters/Tools/AssetValidator

새 파일:
- cook 전 asset sanity check tool을 만든다.

반영:
- missing texture, missing mesh, missing animation, invalid `.wfx`, invalid `.wmesh`, duplicate asset id를 검사한다.
- LoL champion asset set과 Elden boss slice asset set을 profile로 나눈다.
- error/warning budget을 exit code로 반환해 CI에서 사용할 수 있게 한다.

### 1-4. C:/Users/user/Desktop/Winters/Tools/WintersCooker

새 파일:
- runtime content를 build output으로 복사/변환하는 cook step을 만든다.

반영:
- `.fbx` -> `.wmesh/.wskel/.wanim`, `.png` -> runtime texture copy, `.wfx` -> validated asset copy를 우선한다.
- Server cook은 visual asset을 제외하고 data/schema만 포함한다.
- Client cook은 champion, map, UI, FX profile을 포함한다.

### 1-5. C:/Users/user/Desktop/Winters/Tools/DerivedDataCache

새 파일:
- shader/mesh/fx conversion output을 content hash로 캐시한다.

반영:
- local disk backend만 MVP로 둔다.
- input file hash + converter version + platform key를 cache key로 만든다.
- cache hit/miss를 log와 summary json으로 남긴다.

### 1-6. C:/Users/user/Desktop/Winters/.github/workflows

새 파일:
- CI build workflow를 추가한다.

반영:
- Windows x64 Debug build.
- Services `go test ./...`.
- AssetValidator dry run.
- `git diff --check`.

확인 필요:
- 저장소가 GitHub Actions를 사용할지, local batch CI만 먼저 둘지 결정한다.

### 1-7. C:/Users/user/Desktop/Winters/UpdateLib.bat

목표:
- Engine public header 변경 후 SDK sync가 누락되지 않는다.

반영:
- BuildTool 또는 CI에서 Engine public header 변경 감지 시 `UpdateLib.bat` 필요 여부를 출력한다.
- EngineSDK 직접 편집은 금지하고 source -> SDK sync 흐름을 문서화한다.

### 1-8. C:/Users/user/Desktop/Winters/.md/문서/13_Ch13_Tooling.md

목표:
- 구현 결과와 tooling chapter가 어긋나지 않게 한다.

반영:
- BuildTool MVP가 생긴 뒤 stage status만 갱신한다.
- 계획 문서에 코드에서 확인 가능한 파일 목록을 길게 붙이지 않는다.

## 2. 검증

검증 명령:
- `Tools/Bin/Debug/WintersBuildTool.exe regen --dry-run`
- `Tools/Bin/Debug/AssetValidator.exe --profile LoL --strict`
- `Tools/Bin/Debug/WintersCooker.exe --platform Win64 --profile LoL --dry-run`
- `go test ./...` in `C:/Users/user/Desktop/Winters/Services`
- `msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64`

합격 기준:
- 신규 소스 파일 추가 시 `.vcxproj` 손등록 누락을 BuildTool이 감지한다.
- missing asset이 runtime crash가 아니라 cook/validate 단계에서 잡힌다.
- Server target에 Client/UI/Render dependency가 섞이면 실패한다.
