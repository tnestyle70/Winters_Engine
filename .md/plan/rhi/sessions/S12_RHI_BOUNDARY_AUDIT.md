# S12. RHI Boundary Audit

작성일: 2026-05-25

목표: DX12 Smoke project를 다시 만들지 않고, 현재 `main`에서 남아 있는 DX11 직접 의존과 RHI shim을 실제 검색 결과로 고정한다.

## 기준 규칙

- 기준 문서: `S11_NEXT_IMPLEMENTATION_HANDOFF.md`
- standalone backend exe/project 금지
- `DX12.exe`, `DX12.vcxproj`, `Smoke.vcxproj`, `Tools/DX12SmokeHost` 재생성 금지
- RHI backend는 `WintersEngine.dll` 내부 구현으로 유지

## 검색 명령

```powershell
rg -n "d3d11\.h|ID3D11|ID3D11Device|ID3D11DeviceContext|IDXGI|D3D11_|DXGI_|Microsoft::WRL::ComPtr<\s*ID3D11" Engine/Include Engine/Public Client/Public Client/Private --glob '!**/Bin/**'
rg -n "CDX11Device|DX11Shader|DX11Pipeline|CBlendStateCache|RHI/DX11" Engine Client --glob '!**/Bin/**' --glob '!Engine/ThirdPartyLib/**' --glob '!EngineSDK/**'
rg -n "Get_RHIDevice\(|Get_MeshShader\(|Get_MeshPipeline\(|Get_BlendStateCache\(" Engine Client --glob '!**/Bin/**' --glob '!Engine/ThirdPartyLib/**' --glob '!EngineSDK/**'
```

## Audit Result

### 1. Client private native DX11 direct usage

현재 `Client/Public`에는 `ID3D11*` native pointer 직접 노출이 없다.

`Client/Private`에서 직접 native DX11을 만지는 파일은 2개다.

- `Client/Private/GameObject/FX/FxSystem.cpp`
  - `<d3d11.h>` include
  - `IRHIDevice::GetNativeHandle(eNativeHandleType::DX11Device)`를 `ID3D11Device*`로 cast
  - `GetNativeDX11Context()` helper는 현재 create path에서 실사용되지 않는다.
  - 1차 제거 후보: native device/context helper 제거 후 `IRHIDevice` + legacy shader/pipeline null check만 유지

- `Client/Private/Scene/InGameRenderBridge.cpp`
  - `<d3d11.h>` include
  - `IRHIDevice::GetNativeHandle(eNativeHandleType::DX11DeviceContext)`를 `ID3D11DeviceContext*`로 cast
  - 현재 목적은 normal pass DX11 path gate로 보인다.
  - 1차 제거 후보: native context pointer 대신 `pDevice && pDevice->GetBackend() != eRHIBackend::DX12` 조건으로 gate

### 2. Public / SDK DX11 concrete shim

DX11 native pointer는 아니지만, Public/SDK에 DX11 concrete class가 아직 API 모양으로 남아 있다.

- `Engine/Include/GameInstance.h`
  - `DX11Shader`, `DX11Pipeline`, `CBlendStateCache` forward declaration
  - `Get_MeshShader()`, `Get_MeshPipeline()`, `Get_BlendStateCache()` 계열 getter

- `Engine/Public/Framework/CEngineApp.h`
  - shared shader/pipeline/blend cache를 `DX11Shader`, `DX11Pipeline`, `CBlendStateCache`로 소유
  - backend shader registry 또는 RHI pipeline/resource owner로 옮길 대상

- `Engine/Public/Renderer/PlaneRenderer.h`
- `Engine/Public/Renderer/ModelRenderer.h`
- `Engine/Public/Renderer/NormalPass.h`
- `Engine/Public/Renderer/FxStaticMeshRenderer.h`
- `Engine/Public/Renderer/TriangleRenderer.h`
- `Client/Public/GameObject/FX/FxSystem.h`
- `Client/Public/GameObject/FX/FxBeamSystem.h`

이 묶음은 즉시 삭제보다 render helper별 RHI facade를 만든 뒤 signature를 바꿔야 한다.

### 3. Client shim call sites

`Client/Private/Scene/InGameBootstrapBridge.cpp`가 현재 가장 큰 consumer다.

- plane setup: `Get_UIPlaneShader()`, `Get_UIPlanePipeline()`, `Get_BlendStateCache()`
- contact shadow setup: `Get_ContactShadowShader()`, `Get_ContactShadowPipeline()`, `Get_BlendStateCache()`
- FX setup: `Get_FxSpriteShader()`, `Get_FxSpritePipeline()`, `Get_BlendStateCache()`
- FX mesh setup: `Get_MeshShader()`, `Get_MeshPipeline()`, `Get_FxMeshShader()`, `Get_FxMeshPipeline()`, `Get_BlendStateCache()`

다음 치환은 `InGameBootstrapBridge.cpp`에서 한 번에 지우기보다 renderer/fx 생성 API를 먼저 RHI-friendly하게 바꾸고 호출부를 따라오는 방식이 안전하다.

### 4. RHI-ready usage

다음 파일들은 이미 `IRHIDevice*`를 통로로 쓰고 있다.

- `Client/Private/UI/ImageScenePresenter.cpp`
- `Client/Private/Scene/Scene_MatchLoading.cpp`
- `Client/Private/Scene/Scene_BanPick.cpp`
- `Client/Private/Scene/InGameLifecycleBridge.cpp`
- `Client/Private/Scene/InGameBootstrapBridge.cpp`
- `Engine/Private/Renderer/ModelRenderer.cpp`

단, 일부는 내부에서 다시 DX11 shader/pipeline shim으로 내려가므로 완전한 backend-neutral 상태는 아니다.

## Next Patch Order

1. Client private의 `<d3d11.h>` 직접 include 2곳 제거
2. `FxSystem.cpp` native helper 제거
3. `InGameRenderBridge.cpp` normal pass gate를 native context pointer에서 RHI backend check로 변경
4. `git diff --check`
5. Client Debug build

## 완료 기준

- `Client/Private`에서 `d3d11.h`, `ID3D11Device`, `ID3D11DeviceContext` hit가 0개
- `Client/Public`에 새 DX11 concrete leak 없음
- standalone smoke project 없음
- 기존 DX11 runtime path는 유지

## Actual Result

- `Client/Private/GameObject/FX/FxSystem.cpp`에서 `<d3d11.h>`와 native DX11 helper를 제거했다.
- `Client/Private/Scene/InGameRenderBridge.cpp`에서 native DX11 context cast를 제거하고 `eRHIBackend::DX11` gate로 normal pass를 유지했다.
- 남은 DX11 concrete shim은 Public renderer/framework signature 정리 단계에서 처리한다.
