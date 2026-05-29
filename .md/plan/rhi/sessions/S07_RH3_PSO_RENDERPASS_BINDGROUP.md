# S07. RH-3 PSO, RenderPass, BindGroup

목표: DX12/Vulkan/Console backend로 갈 때 state가 흩어지지 않게 PSO와 binding 모델을 고정한다.

## 새 인터페이스

```text
Engine/Public/RHI/IRHIPipelineState.h
Engine/Public/RHI/IRHIRenderPass.h
Engine/Public/RHI/IRHIBindGroup.h
Engine/Public/RHI/IRHIBindGroupLayout.h
```

## 설계 원칙

- PipelineState는 immutable이다.
- BindGroupLayout은 shader reflection 또는 명시 desc로 만든다.
- BindGroup은 생성 시 immutable을 기본으로 한다.
- 갱신은 `UpdateBindGroup()` 별도 API로 둔다.
- RenderPass는 color/depth attachment와 load/store op를 명시한다.

## DX11 backend 대응

DX11은 PSO가 native로 없다. 내부에서 다음 state 묶음을 캐시한다.

- input layout
- VS/PS/CS
- rasterizer state
- blend state
- depth stencil state
- primitive topology

## 합격 기준

```powershell
rg -n "IRHIPipelineState|IRHIRenderPass|IRHIBindGroup|UpdateBindGroup" Engine/Public/RHI Engine/Private/RHI
msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64
```

## 주의

- `SetBlendState` 같은 세부 state API를 public hot path로 확산시키지 않는다.
- 새 shader 추가는 PSO desc 추가로 끝나야 한다.
