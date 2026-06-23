# S17 RHI LoL Snapshot Bridge Progress Report - 2026-06-23

## 기준 문서

- `.md/plan/rhi/sessions/S17_RHI_SCENE_RENDERER_CODEX_HANDOFF.md`
- `.md/plan/rhi/sessions/S13_LOL_TO_ELDEN_SHARED_RHI_RENDER_PIPELINE.md`
- `.md/architecture/WINTERS_CODEBASE_COMPASS.md`

## 이번 추가 반영

- `CRHISceneRenderer`가 두 vertex layout을 처리하도록 확장했다.
  - Elden probe: `POSITION/COLOR/TEXCOORD`
  - LoL `.wmesh`: `POSITION/NORMAL/TEXCOORD`
- `RenderMeshItem`에 `bDepthWrite`를 추가하고, Scene Renderer 내부에 depth-write on/off pipeline variant를 만들었다.
- `CMesh`가 기존 DX11 buffer를 유지하면서 snapshot용 RHI vertex/index buffer와 `RHIMeshSlice`를 병행 보관한다.
  - RHI index buffer는 현재 RHI가 `R32_UInt`만 지원하므로, 16-bit source index도 snapshot용으로 32-bit 변환해서 업로드한다.
- `CModel` / `ModelRenderer`에 `AppendRenderSnapshotMeshes(...)`를 추가했다.
- LoL `CScene_InGame` lifecycle에서 `CRHISceneRenderer`를 생성/정리한다.
- LoL `Scene_InGameRender.cpp`의 map render 블록에서 `RenderWorldSnapshot`을 만들고, map geometry 1개 mesh를 `CRHISceneRenderer::Render()`로 제출한다.
  - 기존 visual 회귀를 줄이기 위해 legacy map draw 직전, max 1 mesh, depth-write off로 제출한다.
  - material/texture 이관은 아직 하지 않았고 Scene Renderer default white texture를 사용한다.

## 검증 결과

### Build

- CMake/Ninja target build: PASS
  - command: `cmake --build out/build/msvc-ninja --config Debug --target WintersEngine WintersElden WintersEldenRingEditor`
- Full solution build: PASS
  - command: `msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64 /m /nr:false /v:minimal /clp:ErrorsOnly;Summary`
  - result: warning 150, error 0
- `git diff --check`: PASS
  - LF/CRLF warning only, whitespace error 없음.

### Runtime smoke

각 앱은 8초 생존 후 `CloseMainWindow()`로 정상 종료했다. forced close는 없었다.

| App | Args | Alive 8s | ExitCode | ForcedClose |
| --- | --- | --- | --- | --- |
| `EldenRingClient/Bin/Debug/WintersElden.exe` | `--scene=probe` | PASS | 0 | false |
| `EldenRingClient/Bin/Debug/WintersElden.exe` | `--scene=probe --rhi=dx11` | PASS | 0 | false |
| `EldenRingEditor/Bin/Debug/WintersEldenRingEditor.exe` | none | PASS | 0 | false |
| `Client/Bin/Debug/WintersGame.exe` | none | PASS | 0 | false |

### Boundary audit

- 신규 `RenderWorldSnapshot` / `CRHISceneRenderer` / LoL scene bridge 파일에는 `ID3D11*`, `ID3D12*`, `d3d11.h`, `d3d12.h`, `DX11Shader`, `DX11Pipeline` 신규 노출 없음.
- 전체 audit는 여전히 PARTIAL이다.
  - 기존 `Engine/Public/Renderer/ModelRenderer.h`, `NormalPass.h`, `PlaneRenderer.h`, `FxStaticMeshRenderer.h`
  - 기존 `Engine/Include/GameInstance.h`
  - 기존 `Client/Public/GameObject/FX/FxSystem.h`, `FxBeamSystem.h`
  - 위 DX11 public 노출은 S17 `LOLPORT` 잔여 작업이다.

## 기준 대비 상태 업데이트

| Phase | 이전 상태 | 현재 상태 | 추정 반영률 |
| --- | --- | --- | ---: |
| RINGFIX | 대체로 완료 | 유지 | 90-95% |
| SNAPSHOT | Elden probe 수직 슬라이스 | LoL map geometry 1 mesh가 `CRHISceneRenderer` path를 실제로 탐 | 55-60% |
| LOLPORT | 미착수에 가까움 | 기존 DX11 render path는 유지, snapshot shadow bridge만 추가 | 5-10% |
| S17 전체 | 약 45% | LoL bridge 포함 약 55% | 약 55% |

## 남은 작업

- LoL map 전체 submesh/material texture를 RHI material path로 이관.
- `CTexture` / material resolver가 RHI texture/sampler handle을 제공하도록 정리.
- champion 1체 static/skinned renderer path를 snapshot으로 넘기는 실제 visual cutover.
- FX sprite/debug item path를 `CRHISceneRenderer` 또는 별도 RHI renderer slice로 연결.
- `GameInstance` / `ModelRenderer` / FX public header의 `DX11Shader` / `DX11Pipeline` 제거.
- visual capture 또는 pixel-diff 검증 추가. 이번 smoke는 생존/종료 검증이며 화면 동등성 자동 판정은 하지 않았다.
