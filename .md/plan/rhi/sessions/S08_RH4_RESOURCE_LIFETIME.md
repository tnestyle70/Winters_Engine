# S08. RH-4 Resource Lifetime

목표: RHI handle을 안전하게 만들고, backend resource lifetime을 Render thread 정책으로 고정한다.

## Handle 강화

64-bit handle:

- high 32 bits: generation
- low 32 bits: index

필수 기능:

- `IsValid()`
- `Index()`
- `Generation()`
- stale handle detect

## Resource table

```text
Engine/Public/RHI/RHIHandleTable.h
Engine/Private/RHI/RHIHandleTable.cpp
```

정책:

- create/destroy는 render thread 또는 engine init/shutdown에서만 허용한다.
- worker thread는 destroy 요청을 queue에 넣는다.
- frame boundary에서 deferred destroy를 flush한다.

## Deferred destroy

DX12/Vulkan은 GPU in-flight resource 해제가 위험하다.

필수:

- `DestroyLater(handle, frameIndex)`
- `FlushDeferredDestroy(completedFrameIndex)`
- debug build stale handle assert

## 합격 기준

```powershell
rg -n "Generation|DestroyLater|FlushDeferredDestroy|RHIHandleTable" Engine
msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64
```

## 후속

이 세션 이후 DX12 backend가 handle 기반으로 자연스럽게 들어올 수 있다.
