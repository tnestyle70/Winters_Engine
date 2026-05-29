# S03. RH-1 DX11 Device Adapter

> 2026-05-25 적용: `CDX11Device`가 `IRHIDevice`를 구현하도록 연결하고, `Create_Buffer`/`Destroy_Buffer`/`Resolve_Buffer`를 DX11 buffer resource table로 우선 구현한다.
> Texture/Shader/Sampler handle 생성은 S03 skeleton만 열어두고 후속 세션에서 실제 wrapper를 붙인다.

목표: 기존 `CDX11Device`를 유지하면서 `IRHIDevice` 구현체로 세운다. DX11은 legacy가 아니라 첫 backend가 된다.

## 작업 범위

- `CDX11Device : public IRHIDevice`
- 기존 `CDX11Device.cpp`에 RHI 생성 API를 붙인다.
- 새 `DX11DeviceAdapter.cpp`를 만들지 않는다. 중복 정의 위험이 크다.
- 기존 direct DX11 API는 `_Legacy` 경로로 잠시 유지한다.

## 구현 대상

`CDX11Device`가 구현해야 할 최소 API:

- `GetBackend() -> eRHIBackend::DX11`
- `BeginFrame()`
- `EndFrame()`
- `CreateBuffer(const RHIBufferDesc&, const void*) -> RHIBufferHandle`
- `DestroyBuffer(RHIBufferHandle)`
- `ResolveBuffer(RHIBufferHandle) -> IRHIBuffer*`
- `CreateTexture(...)`
- `CreateShader(...)`
- `CreateSampler(...)`

## DX11 리소스 wrapper

Private 구현으로 둔다.

```text
Engine/Private/RHI/DX11/DX11Buffer.h/.cpp
Engine/Private/RHI/DX11/DX11Texture.h/.cpp
Engine/Private/RHI/DX11/DX11Shader.h/.cpp
Engine/Private/RHI/DX11/DX11Sampler.h/.cpp
```

단, S03에서는 파일 이동을 강행하지 않는다. Public DX11 purge는 S05에서 한다.

## 합격 기준

```powershell
rg -n "public IRHIDevice|CreateBuffer\(const RHIBufferDesc" Engine
msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64
```

## 하지 말 것

- DX12 코드를 만들지 않는다.
- Vulkan 코드를 만들지 않는다.
- 별도 backend exe를 만들지 않는다.
- Renderer 전체를 한 번에 RHI로 옮기지 않는다.
