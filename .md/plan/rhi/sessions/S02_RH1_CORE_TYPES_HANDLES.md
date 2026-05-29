# S02. RH-1 Core Types And Handles

목표: DX12/Vulkan/Console을 담을 수 있는 RHI 공통 타입과 handle 기반 리소스 모델을 만든다.

## 작업 범위

새 public RHI 헤더를 추가한다.

```text
Engine/Public/RHI/RHITypes.h
Engine/Public/RHI/RHIDescriptors.h
Engine/Public/RHI/RHIHandles.h
Engine/Public/RHI/RHIResourceTable.h
Engine/Public/RHI/IRHIDevice.h
Engine/Public/RHI/IRHIBuffer.h
Engine/Public/RHI/IRHITexture.h
Engine/Public/RHI/IRHIShader.h
Engine/Public/RHI/IRHISampler.h
Engine/Public/RHI/IRHISwapChain.h
Engine/Public/RHI/IRHIQueue.h
```

## 설계 결정

- DLL 경계에서 `unique_ptr<IRHIBuffer>`를 반환하지 않는다.
- 리소스 생성은 handle 반환이다.
- 실제 리소스 ownership은 Engine 내부 `CRHIResourceTable`이 가진다.
- handle은 RH-4에서 64-bit generation으로 강화한다. RH-1에서는 타입을 먼저 고정한다.

## 최소 타입

필수 enum:

- `eRHIBackend`
- `eRHIFormat`
- `eRHIBufferUsage`
- `eRHIBindFlags`
- `eRHIMemoryUsage`
- `eRHIShaderStage`
- `eRHIResourceState`
- `eRHIIndexFormat`

필수 desc:

- `RHIDeviceDesc`
- `RHIBufferDesc`
- `RHITextureDesc`
- `RHIShaderDesc`
- `RHISamplerDesc`

필수 handle:

- `RHIBufferHandle`
- `RHITextureHandle`
- `RHIShaderHandle`
- `RHISamplerHandle`
- `RHISwapChainHandle`

## 합격 기준

```powershell
rg -n "class IRHIDevice|struct RHIBufferDesc|struct RHIBufferHandle|enum class eRHIBackend" Engine/Public/RHI
msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64
```

## 주의

- 이 세션에서 기존 `DX11Buffer` 호출부를 전부 교체하지 않는다.
- `IBuffer.h`는 당장 삭제하지 않는다.
- `GetNativeHandle()` escape hatch는 RH-2/RH-3까지 임시 허용하되 public API 기본 경로로 쓰지 않는다.
