# S10. Vulkan And Console Backends

목표: DX12 backend 이후 Vulkan/Console backend가 들어올 수 있는 경계를 미리 고정한다.

## Vulkan 위치

```text
Engine/Private/RHI/Vulkan/
```

별도 `Vulkan.exe` 또는 `SmokeVulkan.vcxproj`는 만들지 않는다.

## Console 위치

콘솔 backend는 공개 SDK를 repo에 직접 넣지 않는다. private backend adapter와 build flag만 둔다.

```text
Engine/Private/RHI/Console/
```

## 공통 RHI가 흡수해야 하는 차이

- queue family
- command pool
- descriptor set layout
- render pass/framebuffer
- resource state transition
- memory allocator
- shader compilation path
- swapchain present model

## Vulkan 선행 조건

- RH-3 RenderPass가 있어야 한다.
- RH-4 handle lifetime이 있어야 한다.
- DXC SPIR-V compile path가 있어야 한다.
- validation layer availability check가 있어야 한다.

## Console 선행 조건

- Public RHI가 native API type을 노출하지 않아야 한다.
- shader permutation key가 backend independent해야 한다.
- file/resource path가 platform abstraction을 지나야 한다.
- thread model이 render thread 정책으로 고정되어야 한다.

## 합격 기준

```powershell
rg -n "Vulkan|Console" Engine/Public/RHI Engine/Private/RHI .md/plan/rhi
```

Public에는 backend 이름 enum과 config 정도만 보여야 한다. Native type은 Private에만 있어야 한다.
