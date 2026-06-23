# 2026-06-24 Collab Harness Bootstrap Report

## 목적

노트북과 데스크탑을 서로 다른 작업자로 보고, S17 RHI migration과 이후 refactoring을 conflict가 적은 방식으로 이어가기 위한 협업 규칙과 검증 harness를 추가했다.

## 반영 파일

- `.md/collab/ACTIVE_WORK_PACKETS.md`
- `.md/collab/OWNERSHIP_MATRIX.md`
- `.md/collab/GIT_SYNC_RULES.md`
- `.md/collab/HARNESS_RULES.md`
- `.md/collab/work-packets/2026-06-24_s17_collab_harness_bootstrap.md`
- `Tools/Harness/Run-S17RhiValidation.ps1`
- `.md/build/2026-06-24_S17_RHI_VALIDATION_HARNESS_REPORT.md`

## 핵심 규칙

- 두 장비는 같은 파일을 동시에 수정하지 않는다.
- 작업 전 `.md/collab/ACTIVE_WORK_PACKETS.md`에 owner, branch, owned paths, validation, report path를 기록한다.
- `EngineSDK/inc/**`, `.vcxproj`, `.filters`, public header는 always-lock 파일로 취급한다.
- push 전 `Tools/Harness/Run-S17RhiValidation.ps1`를 공통 기준으로 실행한다.
- 보고서는 날짜별 새 파일로 남기고 기존 보고서를 덮어쓰지 않는다.

## Harness 검증 결과

실행:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Tools\Harness\Run-S17RhiValidation.ps1 -ReportPath .md\build\2026-06-24_S17_RHI_VALIDATION_HARNESS_REPORT.md
```

결과:

- `git diff --check`: PASS
- `Client/Public` + `Shared` concrete graphics audit: PASS, 0건
- 신규 공용 RHI public header focused audit: PASS, 0건
- CMake/Ninja `WintersEngine`, `WintersElden`, `WintersEldenRingEditor`: PASS
- MSBuild `Winters.sln` Debug x64: PASS, warning 265, error 0
- runtime smoke: PASS
  - `WintersElden_probe_dx12`: 8초 생존
  - `WintersElden_probe_dx11`: 8초 생존
  - `WintersEldenRingEditor`: 8초 생존
  - `WintersGame`: 8초 생존

상세 report:

- `.md/build/2026-06-24_S17_RHI_VALIDATION_HARNESS_REPORT.md`

## 참고

CMake 단계에서 새 report 파일 추가로 `-- GLOB mismatch!`가 출력되었고 CMake가 자동 reconfigure를 수행했다. 최종 exit code는 0이며 이후 Ninja/MSBuild/runtime smoke 모두 통과했다.

## 다음 작업 권장

- 노트북: `codex/rhi-laptop-*` 브랜치에서 Engine/RHI/core renderer 경로를 소유.
- 데스크탑: `codex/rhi-desktop-*` 브랜치에서 LoL runtime bridge와 visual verification을 소유.
- 공용 public header/project/SDK 파일은 work packet에 명시적으로 lock을 잡은 장비만 수정.
