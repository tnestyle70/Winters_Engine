# Harness Rules

S17 RHI/SceneRenderer 계열 작업은 아래 harness를 공유 검증 기준으로 사용한다.

```powershell
Tools/Harness/Run-S17RhiValidation.ps1
```

## 기본 검증

- `git diff --check`
- `Client/Public`, `Shared`, 신규 공용 RHI renderer/resource public header의 DX11/DX12 concrete 노출 audit
- CMake/Ninja: `WintersEngine`, `WintersElden`, `WintersEldenRingEditor`
- MSBuild: `Winters.sln` Debug x64
- runtime smoke:
  - `WintersElden.exe --scene=probe`
  - `WintersElden.exe --scene=probe --rhi=dx11`
  - `WintersEldenRingEditor.exe`
  - `WintersGame.exe`

## 빠른 문서/규칙 변경

빌드 영향이 없는 문서-only 변경은 runtime smoke를 생략할 수 있다.

```powershell
Tools/Harness/Run-S17RhiValidation.ps1 -SkipRuntimeSmoke
```

단, RHI/Renderer/Resource/Client scene code를 수정한 경우 runtime smoke를 생략하지 않는다.

## Report

- harness는 기본적으로 `.md/build/YYYY-MM-DD_HHmmss_S17_RHI_VALIDATION_HARNESS_REPORT.md`를 생성한다.
- work packet 완료 보고서는 별도 요약 보고서로 `.md/build/YYYY-MM-DD_*.md`에 남긴다.

## 실패 시

- 첫 실패에서 중단한다.
- 생성된 harness report의 실패 step, exit code, output tail을 기준으로 수정한다.
- 수정 후 전체 harness를 다시 실행한다.
