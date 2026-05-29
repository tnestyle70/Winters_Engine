# Elden Shared RHI Rendering Entry

작성일: 2026-05-25

기준 문서: `.md/plan/rhi/sessions/S13_LOL_TO_ELDEN_SHARED_RHI_RENDER_PIPELINE.md`

## 결정

Elden 클라이언트는 별도 렌더러를 만들지 않는다.

먼저 LoL `Scene_InGame` 렌더링을 공용 RHI renderer로 정리하고, Elden은 그 renderer에 `RenderWorldSnapshot`을 넘기는 소비자가 된다.

## 이유

- LoL은 이미 map, champion, skinned mesh, FX, UI, debug draw가 있어 RHI renderer 검증 밀도가 높다.
- Elden은 world partition, third-person camera, action combat 때문에 scene/gameplay complexity가 크다.
- 렌더러까지 Elden에서 새로 만들면 DX11 legacy와 DX12 path가 다시 복제된다.
- 따라서 renderer는 Engine 공용, game-specific code는 snapshot extraction까지만 담당한다.

## Elden 최초 vertical slice

1. `CEldenGameModule`을 추가한다.
2. `GameModuleRegistry`의 Elden placeholder를 실제 module로 바꾼다.
3. `Scene_EldenFieldTest`를 추가한다.
4. `Scene_EldenFieldTest`는 player mesh, ground mesh, camera만 구성한다.
5. 렌더링은 `CRHISceneRenderer`만 호출한다.

```text
Scene_EldenFieldTest
  -> Build RenderWorldSnapshot
  -> CRHISceneRenderer::Render(snapshot)
  -> CRenderGraph
  -> IRHICommandList
  -> DX11 / DX12 backend
```

## LoL 선행 작업

Elden 전에 LoL에서 먼저 끝낼 것:

- `CEngineApp` frame loop를 backend-neutral로 정리
- DX11 backend의 RHI resource/command contract 구현
- `InGameRenderBridge`를 snapshot extraction 구조로 변경
- `InGameBootstrapBridge`의 `Get_*Shader/Get_*Pipeline/Get_BlendStateCache` 의존 제거
- `ModelRenderer`, `PlaneRenderer`, `NormalPass`, `FxStaticMeshRenderer`를 RHI renderer path로 치환

## Elden이 재사용할 공용 계층

- `IRHIDevice`
- `IRHICommandList`
- `CRenderGraph`
- `CRHISceneRenderer`
- `CRHIMeshResource`
- `CRHIMaterialResource`
- `CGPUScene`
- `CGPUDrivenPipeline`

## 금지

- Elden 전용 DX12 renderer 생성 금지
- `DX12.exe`, `DX12.vcxproj`, `Smoke.vcxproj`, `Tools/DX12SmokeHost` 재생성 금지
- Elden scene에서 `ID3D11*`, `DX11Shader`, `DX11Pipeline`, `CBlendStateCache` 직접 사용 금지

## 완료 기준

첫 완료 기준은 화려한 Elden gameplay가 아니다.

```text
WintersGame.exe --rhi=dx12 --product=elden
  -> Scene_EldenFieldTest enters
  -> ground mesh visible
  -> player mesh visible
  -> camera moves
  -> same CRHISceneRenderer path as LoL
```

그 다음에 `WintersElden.exe` 분리를 진행한다.
