# S04. RH-2 CommandList

> 2026-05-25 적용: `IRHICommandList`/Pool/Fence/Semaphore 계약을 추가하고, `CDX11CommandList`로 DX11 immediate-context command list skeleton을 구현한다.
> 실제 Renderer 호출 이전은 다음 단계에서 `CPlaneRenderer -> Cube/Triangle -> Model` 순서로 좁혀간다.

목표: 렌더 호출을 `ID3D11DeviceContext*` 직접 호출에서 `IRHICommandList` 호출로 옮긴다.

## 새 인터페이스

```text
Engine/Public/RHI/IRHICommandList.h
Engine/Public/RHI/IRHICommandPool.h
Engine/Public/RHI/IRHIFence.h
Engine/Public/RHI/IRHISemaphore.h
```

## 필수 CommandList API

- `Begin()`
- `End()`
- `BeginRenderPass(...)`
- `EndRenderPass()`
- `SetVertexBuffer(...)`
- `SetIndexBuffer(...)`
- `SetPrimitiveTopology(...)`
- `SetViewport(...)`
- `SetScissor(...)`
- `SetShader(...)`
- `SetPipelineState(...)`
- `SetTexture(...)`
- `SetSampler(...)`
- `SetConstantBuffer(...)`
- `Draw(...)`
- `DrawIndexed(...)`
- `DrawIndexedInstanced(...)`
- `Dispatch(...)`
- `ResourceBarrier(...)`
- `UpdateBuffer(...)`

## DX11 immediate emulation

`CDX11CommandList::Begin()`은 no-op가 아니다.

반드시 처리:

- default RTV/DSV bind
- viewport bind
- per-frame state reset

`CDX11CommandList::End()` 처리:

- active render pass 종료
- RTV/DSV unbind
- debug build에서 dangling SRV/UAV marker 점검

## 첫 이관 대상

작은 순서로 옮긴다.

1. `CPlaneRenderer`
2. `CubeRenderer`
3. `TriangleRenderer`
4. `CMesh`
5. `CModel`
6. `ModelRenderer`
7. `CFxStaticMeshRenderer`
8. `CFxSystem`
9. UI/ImGui escape 정리

## 합격 기준

```powershell
rg -n "Render\(ID3D11DeviceContext|ID3D11DeviceContext\*" Engine/Public Client/Public
```

0 hit를 목표로 한다. 단 private backend 구현부는 허용한다.

## 주의

- DX11의 `ResourceBarrier`는 no-op이어도 호출은 남긴다.
- DX12/Vulkan으로 옮길 때 필요한 barrier discipline을 지금부터 강제한다.
