# S17 RHI Scene Renderer SNAPSHOT Progress Report - 2026-06-23

## 기준 문서

- `.md/plan/rhi/sessions/S17_RHI_SCENE_RENDERER_CODEX_HANDOFF.md`
- `.md/plan/rhi/sessions/S13_LOL_TO_ELDEN_SHARED_RHI_RENDER_PIPELINE.md`

## 이번 반영 요약

- `RenderWorldSnapshot` 계약을 Engine public renderer 영역에 추가했다.
- `CRHIMeshResource`, `CRHIMaterialResource`, `CRHISceneRenderer`를 Engine에 추가했다.
- Elden probe cube renderer가 앱 전용 shader/pipeline/bind group을 직접 소유하지 않고, mesh/material resource를 만든 뒤 `RenderWorldSnapshot`을 `CRHISceneRenderer::Render()`에 넘기도록 연결했다.
- Visual Studio `.vcxproj/.filters`와 CMake source group에 신규 Engine renderer 파일을 등록했다.
- 빌드 위생 보강으로 snapshot data struct의 불필요한 DLL export를 제거하고, 단일 mesh resource는 vector 대신 단일 slice 소유로 줄여 신규 C4251 경고를 제거했다.
- S17 금지사항인 새 `Smoke.exe`, `DX12.exe`, 별도 DX12 프로젝트 추가는 하지 않았다.

## 변경 파일

- `Engine/Public/Renderer/RenderWorldSnapshot.h`
- `Engine/Public/Renderer/RHIMeshResource.h`
- `Engine/Private/Renderer/RHIMeshResource.cpp`
- `Engine/Public/Renderer/RHIMaterialResource.h`
- `Engine/Private/Renderer/RHIMaterialResource.cpp`
- `Engine/Public/Renderer/RHISceneRenderer.h`
- `Engine/Private/Renderer/RHISceneRenderer.cpp`
- `EldenRingClient/Public/EldenRingRHITestCubeRenderer.h`
- `EldenRingClient/Private/EldenRingRHITestCubeRenderer.cpp`
- `Engine/Include/Engine.vcxproj`
- `Engine/Include/Engine.vcxproj.filters`
- `cmake/WintersEngine.cmake`
- build generated SDK headers:
  - `EngineSDK/inc/Renderer/RenderWorldSnapshot.h`
  - `EngineSDK/inc/Renderer/RHIMeshResource.h`
  - `EngineSDK/inc/Renderer/RHIMaterialResource.h`
  - `EngineSDK/inc/Renderer/RHISceneRenderer.h`

## 검증 결과

### Build

- CMake/Ninja target build: PASS
  - command: `cmake --build out/build/msvc-ninja --config Debug --target WintersEngine WintersElden WintersEldenRingEditor`
- Full solution build: PASS
  - command: `msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64 /m /nr:false /v:minimal /clp:ErrorsOnly;Summary`
  - result: warning 165, error 0

### Runtime smoke

각 앱은 8초 생존 후 `CloseMainWindow()`로 정상 종료했다. forced close는 없었다.

| App | Args | Alive 8s | ExitCode | ForcedClose |
| --- | --- | --- | --- | --- |
| `EldenRingClient/Bin/Debug/WintersElden.exe` | `--scene=probe` | PASS | 0 | false |
| `EldenRingClient/Bin/Debug/WintersElden.exe` | `--scene=probe --rhi=dx11` | PASS | 0 | false |
| `EldenRingEditor/Bin/Debug/WintersEldenRingEditor.exe` | none | PASS | 0 | false |
| `Client/Bin/Debug/WintersGame.exe` | none | PASS | 0 | false |

### Hygiene / boundary

- `git diff --check`: PASS
  - LF/CRLF warning만 출력, whitespace error 없음.
- process cleanup: PASS
  - `WintersElden`, `WintersEldenRingEditor`, `WintersGame` 잔존 프로세스 없음.
- DX boundary audit: PARTIAL
  - 이번 신규 `RenderWorldSnapshot` / `CRHISceneRenderer` 경로는 DX11/DX12 public type을 새로 노출하지 않았다.
  - 기존 `Engine/Public`, `Engine/Include`, `Client/Public`, `EngineSDK/inc`에는 `DX11Shader`, `DX11Pipeline` public 노출이 아직 남아 있다.
  - 이는 S17 `LOLPORT` 단계 잔여 항목으로 분류한다.

## 기준 문서 대비 반영률

| Phase | 상태 | 추정 반영률 | 근거 |
| --- | --- | ---: | --- |
| RINGFIX | 대체로 완료 | 90-95% | DX12 descriptor ring frame partition, frame-time upload guard, 4종 app smoke 통과. 시각 캡처/pixel diff는 미수행. |
| SNAPSHOT | 수직 슬라이스 반영 | 40-45% | Engine snapshot contract와 shared scene renderer가 생겼고 Elden probe가 실제로 해당 경로를 탄다. LoL map/champion/minion/FX snapshot feeding은 아직 미연결. |
| LOLPORT | 미착수 | 0-5% | 기존 DX11 public renderer/API 제거 작업은 아직 남아 있다. 이번 변경은 새 공용 RHI renderer 경로 추가까지만 수행. |
| S17 전체 | 진행 중 | 약 45% | RINGFIX 안정화 + SNAPSHOT 최소 runtime slice 완료. S17 최종 DoD인 LoL normal scene renderer 전환과 DX11 public 누수 제거 전까지 완료로 볼 수 없다. |

## 남은 작업

- LoL `Scene_InGame` map/champion/minion/object render 입력을 `RenderWorldSnapshot`으로 채우는 adapter 추가.
- `CRHISceneRenderer`에 실제 LoL/Elden 공용 material/model resource path 연결.
- FX/debug item 렌더 경로를 snapshot 계약에 맞춰 확장.
- `Engine/Public`와 `Client/Public`의 `DX11Shader` / `DX11Pipeline` 노출 제거 또는 내부화.
- Playwright/PIX/RenderDoc에 준하는 visual capture 또는 pixel-diff 기준 추가.
- `EngineSDK/inc/Renderer/*` 생성 헤더를 repo에 포함할지, 빌드 생성물로 둘지 팀 규칙 확정.
