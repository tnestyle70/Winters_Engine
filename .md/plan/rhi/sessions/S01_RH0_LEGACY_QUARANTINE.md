# S01. RH-0 Legacy Quarantine

> 2026-05-25 적용: `CGameInstance`에 `*_LegacyDX11` RHI 게터를 추가하고, `Scene_InGame`의 직접 DX11 사용처를 해당 이름으로 전환한다.
> 기존 이름은 호환 shim으로 남겨 다음 세션에서 점진 제거한다.

목표: 동작을 바꾸지 않고 DX11 직접 의존 지점을 전부 표시한다. 이 세션은 RHI 전환의 안전 난간이다.

## 입력 문서

- `.md/plan/rhi/00_RHI_MIGRATION_MASTER.md`
- `.md/plan/rhi/01_RHI_PHASE_0_FOUNDATION.md`
- `.md/plan/rhi/sessions/00_RHI_SESSION_INDEX.md`

## 작업 범위

- `CGameInstance`의 이름이 애매한 DX11 getter를 `_Legacy`로 분리한다.
- Public/Client 헤더의 DX11 leak 위치에 RH-2 TODO를 붙인다.
- 파일 이동은 하지 않는다.
- 기능 변경은 하지 않는다.

## 1. Getter rename

대상:

- `Get_RHIDevice() -> Get_DX11Device_Legacy()`
- `Get_MeshShader() -> Get_MeshShader_Legacy()`
- `Get_MeshPipeline() -> Get_MeshPipeline_Legacy()`
- `Get_BlendStateCache() -> Get_BlendStateCache_Legacy()`
- `Get_FxSpriteShader() -> Get_FxSpriteShader_Legacy()`
- `Get_FxSpritePipeline() -> Get_FxSpritePipeline_Legacy()`
- `Get_FxMeshShader() -> Get_FxMeshShader_Legacy()`
- `Get_FxMeshPipeline() -> Get_FxMeshPipeline_Legacy()`

## 2. Leak marker

대상 후보:

- `Engine/Include/GameInstance.h`
- `Engine/Public/Framework/CEngineApp.h`
- `Engine/Public/Manager/UI/UI_Manager.h`
- `Engine/Public/Renderer/PlaneRenderer.h`
- `Engine/Public/Renderer/FxStaticMeshRenderer.h`
- `Engine/Public/Resource/Mesh.h`
- `Engine/Public/Resource/Model.h`
- `Engine/Public/Resource/Texture.h`
- `Engine/Public/Resource/ResourceCache.h`
- `Engine/Public/RHI/CDX11Device.h`
- `Engine/Public/RHI/DX11/*.h`
- `Client/Public/GameObject/FX/FxSystem.h`
- `Client/Private/Scene/Scene_InGame.cpp`

주석 형식:

```cpp
// RH-2 TODO: backend native DX11 type 제거. IRHICommandList/IRHIDevice 기반으로 교체.
```

## 합격 기준

```powershell
rg -n "Get_RHIDevice\(\)|Get_MeshShader\(\)|Get_MeshPipeline\(\)|Get_BlendStateCache\(\)" Engine Client
```

결과가 0 hit여야 한다. 단 `_Legacy`는 허용한다.

```powershell
msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64
```

빌드가 통과해야 한다. deprecated warning은 허용한다.

## 하지 말 것

- `Engine/Public/RHI/DX11`를 아직 이동하지 않는다.
- `CDX11Device.h`를 아직 private로 이동하지 않는다.
- Renderer signature를 아직 갈아엎지 않는다.
- `Smoke.vcxproj`, `DX12.vcxproj`를 만들지 않는다.
> 2026-05-25 correction: 최종 push된 `main`에는 `*_LegacyDX11` getter 이름이 남아 있지 않다. 현재 실제 코드는 `CGameInstance::Get_RHIDevice()`가 `IRHIDevice*`를 반환하고, `Get_MeshShader()` / `Get_MeshPipeline()` / `Get_BlendStateCache()`가 DX11 concrete shim으로 남아 있다. 다음 구현은 과거 rename 패치가 아니라 `S11_NEXT_IMPLEMENTATION_HANDOFF.md`의 audit-first 규칙으로 진행한다.
