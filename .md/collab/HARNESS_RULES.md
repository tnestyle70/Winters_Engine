# Harness Rules

S17 RHI/SceneRenderer 계열 작업은 아래 harness를 공유 검증 기준으로 사용한다.

```powershell
Tools/Harness/Run-S17RhiValidation.ps1
```

## 기본 검증

- `git diff --check`
- `Client/Public`, `Shared`, 신규 공용 RHI renderer/resource public header의 DX11/DX12 concrete 노출 audit
- CMake/Ninja: `WintersEngine`, `WintersElden`, `WintersEldenRingEditor`, `WintersLoLEditor`
- MSBuild: `Winters.sln` Debug x64
- runtime smoke:
  - `WintersElden.exe --scene=probe`
  - `WintersElden.exe --scene=probe --rhi=dx11`
  - `WintersEldenRingEditor.exe`
  - `WintersLoLEditor.exe`
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

## Bot AI / GameSim 검증

Bot AI, ChampionAI, CommandExecutor, champion GameSim, gameplay definition data를 수정하는 packet은 아래 harness를 기본 검증 기준으로 사용한다.

```powershell
Tools/Harness/Run-BotAiValidation.ps1
```

기본 검증:

- `git diff --check`
- `Shared/GameSim/Systems/ChampionAI`와 AI component의 Client/Renderer/ImGui/DX concrete 의존성 audit
- Yone Bot E return command와 `skill.yone.e` stage-2 data contract audit
- `Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1`를 통한 GameSim/Server/Client/SimLab regression

빠른 문서/규칙 변경:

```powershell
Tools/Harness/Run-BotAiValidation.ps1 -SkipFullPipeline -AllowKnownYoneEContractGap
```

주의:

- `-AllowKnownYoneEContractGap`는 현재 Yone E stage-2 data mismatch를 고치기 전 협업 규칙/문서 변경을 검증하기 위한 임시 옵션이다.
- Yone E contract fix packet부터는 이 옵션 없이 실행해야 한다.
- Bot AI harness report는 `.md/build/YYYY-MM-DD_HHmmss_BOT_AI_VALIDATION_HARNESS_REPORT.md`에 생성된다.
