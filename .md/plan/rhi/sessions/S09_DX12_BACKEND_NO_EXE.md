# S09. DX12 Backend, No Standalone Exe

목표: DX12를 별도 실행 프로젝트가 아니라 RHI backend로 추가한다.

## 금지

- `DX12.vcxproj` 금지
- `Smoke.vcxproj` 금지
- `DX12.exe` 금지
- `Smoke.exe`를 목표 산출물로 두는 것 금지

## 허용

DX12 구현 위치:

```text
Engine/Private/RHI/DX12/
```

검증 방식:

- `WintersGame.exe --rhi dx12`
- 또는 `EngineConfig.rhiBackend = eRHIBackend::DX12`
- 또는 Debug config에서 `WINTERS_RHI_DX12` define

## 새 파일

```text
DX12Device.h/.cpp
DX12SwapChain.h/.cpp
DX12Queue.h/.cpp
DX12CommandList.h/.cpp
DX12CommandPool.h/.cpp
DX12Buffer.h/.cpp
DX12Texture.h/.cpp
DX12Shader.h/.cpp
DX12Sampler.h/.cpp
DX12PipelineState.h/.cpp
DX12DescriptorHeap.h/.cpp
DX12Fence.h/.cpp
DX12MemoryAllocator.h/.cpp
DX12PipelineCache.h/.cpp
```

## ThirdParty

D3D12MA는 `Engine/ThirdPartyLib/D3D12MA/`에 편입한다.

단, 편입은 RH-5 세션에서 한다. S01~S08 중에는 추가하지 않는다.

## 합격 기준

Compile-only:

- Engine 빌드 통과
- DX12 backend 생성 코드 컴파일
- `WintersGame.exe --rhi dx12`가 첫 프레임 직전까지 진입

Visual parity:

- LoL 기본 씬 frame diff 기준 통과
- D3D12 debug layer error 0
- PSO cache 저장/로드 동작

## 기존 문서 보정

기존 `06_RHI_PHASE_5_DX12_BACKEND.md`의 `Debug-DX12 / Release-DX12`는 별도 exe가 아니라 같은 solution configuration에서 backend define을 켜는 의미로 해석한다.
