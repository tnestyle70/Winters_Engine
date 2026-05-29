# Public Demo Release Pipeline

Session - GitHub Releases, itch, Steam Playtest 중 하나로 Winters 공개 데모 릴리즈 파이프라인을 증명한다.

## 1. 반영해야 하는 코드

### 1-1. C:/Users/user/Desktop/Winters/Tools/ReleaseBuilder

새 파일:
- release package를 만드는 command line tool을 만든다.

반영:
- build output, cooked content, required DLL, config, README, license/notice를 staging folder에 모은다.
- `WintersDemo_<version>_Win64.zip` 형식으로 패키징한다.
- manifest json에 file size, hash, build time, git commit, content profile을 기록한다.

### 1-2. C:/Users/user/Desktop/Winters/Tools/CrashReporter

새 파일:
- MVP crash log collector를 만든다.

반영:
- Windows unhandled exception minidump 또는 최소한 crash log file path를 남긴다.
- build version과 git commit을 crash log에 포함한다.
- upload server는 MVP 필수에서 제외하고 local crash folder 수집부터 한다.

### 1-3. C:/Users/user/Desktop/Winters/Engine/Public/Core/BuildVersion.h

새 파일:
- runtime에서 build version을 조회할 수 있게 한다.

반영:
- semver, git commit short hash, build config, content profile을 포함한다.
- Client main menu와 crash log가 같은 version string을 사용한다.

### 1-4. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_MainMenu.cpp

목표:
- 공개 데모에서 version, build channel, patch note link를 확인할 수 있다.

반영:
- main menu 하단에 build version을 표시한다.
- demo mode에서는 LoL vertical slice, Elden boss slice, Replay viewer entry를 명확히 노출한다.
- 로그인/백엔드가 꺼져도 offline demo entry가 동작해야 한다.

### 1-5. C:/Users/user/Desktop/Winters/Services/internal/telemetry

새 파일:
- MVP telemetry ingest service를 만든다.

반영:
- crash summary, session start/end, match result, replay upload result event를 받는다.
- storage는 Postgres 또는 local log file 중 하나로 시작한다.
- 개인정보/계정 정보는 MVP에서 제외한다.

### 1-6. C:/Users/user/Desktop/Winters/.md/release

새 파일:
- release note, QA checklist, rollback checklist를 둔다.

반영:
- `RELEASE_CHECKLIST.md`: build, cook, smoke, replay, crash, packaging 확인.
- `PATCH_NOTES_TEMPLATE.md`: player-facing 변경점.
- `ROLLBACK_CHECKLIST.md`: bad build를 previous release로 되돌리는 절차.

### 1-7. C:/Users/user/Desktop/Winters/.github/workflows/release.yml

새 파일:
- tagged build packaging workflow를 만든다.

반영:
- tag push 또는 manual dispatch로 실행한다.
- build -> asset validate -> cook -> package -> artifact upload 순서로 둔다.
- Steam/itch upload는 MVP에서 수동 절차 문서화 후 자동화한다.

확인 필요:
- 공개 채널은 처음에 GitHub Releases가 가장 단순하다. Steam Playtest는 계정/상점 준비가 된 뒤 진행한다.

## 2. 검증

검증 명령:
- `git diff --check`
- `msbuild Winters.sln /p:Configuration=Release /p:Platform=x64`
- `Tools/Bin/Release/AssetValidator.exe --profile Demo --strict`
- `Tools/Bin/Release/WintersCooker.exe --platform Win64 --profile Demo`
- `Tools/Bin/Release/ReleaseBuilder.exe --version <version> --profile Demo`

수동 검증:
- clean machine 또는 clean folder에서 zip 압축 해제 후 실행.
- LoL vertical slice 3분 smoke.
- Elden boss slice 1회 clear/fail smoke.
- Replay viewer sample `.wrpl` playback.
- crash test build에서 crash log 생성 확인.

합격 기준:
- zip 하나로 데모 실행이 가능하다.
- patch notes와 build version이 package manifest와 일치한다.
- bad build rollback 절차가 문서로 남아 있다.
