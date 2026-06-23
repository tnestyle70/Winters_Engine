# 2026-06-24 S17 RHI Migration Continuation Report

기준 문서: `.md/plan/rhi/sessions/S17_RHI_SCENE_RENDERER_CODEX_HANDOFF.md`

## 이번 반영

- `Client/Public/GameObject/FX/FxSystem.h`, `FxBeamSystem.h` 공개 생성 API에서 `DX11Shader`, `DX11Pipeline` 노출을 제거했다.
- DX11 FX sprite shader/pipeline 의존은 `FxSystem.cpp`, `FxBeamSystem.cpp` 구현 내부에서 `CGameInstance`를 통해 해석하도록 옮겼다.
- `CModel`이 `.wmat` diffuse 경로를 legacy `CTexture`와 별개로 `RHITextureHandle`에도 캐시하도록 확장했다.
- `CModel::AppendRenderSnapshotMeshes`가 submesh별 RHI albedo texture/sampler를 `RenderMeshItem`에 싣도록 변경했다.
- `ModelRenderer::AppendRenderSnapshotMeshesFrustumCulled`를 추가해 기존 frustum mask를 snapshot 제출 경로에서도 재사용하게 했다.
- LoL InGame map snapshot 제출을 1개 샘플 mesh에서 visible submesh snapshot 제출로 확장했다.

## 검증

- `git diff --check`
  - PASS
  - CRLF 변환 경고만 있음.
- `rg -n "ID3D11|ID3D12|d3d11\.h|d3d12\.h|DX11Shader|DX11Pipeline" Client\Public Shared`
  - PASS: 0건.
- 신규 공용 SceneRenderer/Snapshot/Resource public header focused audit
  - PASS: `RenderWorldSnapshot`, `RHISceneRenderer`, `RHIMeshResource`, `RHIMaterialResource`, `Mesh`, `Model` 기준 DX11/DX12 concrete 노출 0건.
- CMake/Ninja
  - `WintersEngine`, `WintersElden`, `WintersEldenRingEditor`
  - PASS: error 0, 기존 DLL-interface warning만 있음.
- MSBuild
  - `Winters.sln /p:Configuration=Debug /p:Platform=x64`
  - PASS: warning 235, error 0, elapsed `00:01:07.26`.
- runtime smoke
  - `WintersElden.exe --scene=probe`: 8초 생존, cleanup kill.
  - `WintersElden.exe --scene=probe --rhi=dx11`: 8초 생존, cleanup kill.
  - `WintersEldenRingEditor.exe`: 8초 생존, cleanup kill.
  - `WintersGame.exe`: 8초 생존, cleanup kill.

## S17 기준 반영률

- RINGFIX: 90-95%
  - DX12 descriptor ring/frame partition 및 초기 데이터 guard는 유지.
- SNAPSHOT: 70-75%
  - 공용 snapshot, mesh slice, vertex layout, material texture handle, scene renderer 제출 경로가 동작.
  - 남은 범위: pass ordering, render state coverage, FX/debug item의 실제 공용 renderer 편입.
- LOLPORT: 30-35%
  - LoL map visible submesh가 RHI scene snapshot으로 제출되고 `.wmat` diffuse texture도 RHI handle로 연결됨.
  - legacy map draw는 아직 최종 화면 경로에 남겨둔 상태.
  - 챔피언/구조물/정글/미니언은 아직 기존 `ModelRenderer` direct render 경로가 주 경로.
- 공개 경계 정리: 55-60%
  - `Client/Public`/`Shared`의 DX11 concrete 노출은 제거됨.
  - `Engine/Public`에는 legacy `ModelRenderer`, `NormalPass`, `PlaneRenderer`, `CEngineApp` 등의 DX11 타입 노출이 아직 남아 있음.
- 전체 S17 migration: 약 65%

## 남은 작업

- map legacy draw를 feature flag/비교 모드로 낮추고 `CRHISceneRenderer` 출력만으로 visual parity 확인.
- champion, structure, jungle, minion renderer를 `RenderWorldSnapshot` append 경로로 단계적 편입.
- `Engine/Public/Renderer/ModelRenderer.h`의 normal pass DX11 API를 내부/adapter 쪽으로 밀어내 공개 Engine 경계 정리.
- snapshot renderer의 depth/blend/pass ordering을 LoL 실제 장면 기준으로 확장.
- full viewport screenshot 비교와 profiler counter JSON 기준을 추가해 “migration 완료” 판정을 자동화.
