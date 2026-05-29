# DX12 메인 RHI 안정화 계획

작성일: 2026-05-07
대상:
- `C:\Users\user\Desktop\Winters\.codex\worktrees\dx12-bootstrap-sandbox`
- `Engine/Public/RHI`
- `Engine/Public/Platform`
- `Engine/Private/RHI/DX12`
- `Tools/DX12SmokeHost`

목적:
- DX12를 실험 백엔드가 아니라 Winters의 메인 explicit RHI 기준으로 고정한다.
- Effect Tool, RenderGraph, GPU Compute, Console/Vulkan 확장이 DX11 immediate context에 다시 묶이지 않게 한다.
- 현재 샌드박스에서 clear/present가 안정적으로 빌드되는 구조를 먼저 만든다.

## 1. 현재 판단

샌드박스는 DX12 bootstrap이 일부 박제되어 있다. 최초 작성 당시에는 `CDX12Device`, `CDX12Queue`, `CDX12SwapChain`, `CDX12CommandList`와 `Tools/DX12SmokeHost`는 존재하지만 public surface/capability 계약은 아직 없었다. 2026-05-09 기준 현재 코드에는 `RHISurfaceDesc`, `RHICapabilities`, `IPlatformWindow`, `IPlatformSurface` 1차 계약이 반영되어 있다.

최초 작성 당시 `DX12SmokeHost`는 솔루션 등록 전 상태였다. 2026-05-09 기준 현재 `DX12SmokeHost`는 `Winters.sln`에 등록되어 있으며, `Debug-DX12` / `Release-DX12` 검증 루트로 동작한다.

## 2. 이번 샌드박스 반영 범위

이번 작업은 DX12를 메인으로 밀기 위한 최소 안정화다.

```txt
Engine/Public/RHI/RHISurface.h
Engine/Public/RHI/RHICapabilities.h
Engine/Public/Platform/PlatformTypes.h
Engine/Public/Platform/IPlatformWindow.h
Engine/Public/Platform/IPlatformSurface.h
Engine/Public/RHI/RHITypes.h
Engine/Public/RHI/IRHIDevice.h
Engine/Public/Platform/CWin32Window.h
Engine/Include/EngineConfig.h
Winters.sln
Tools/DX12SmokeHost/DX12SmokeHost.vcxproj
```

이번에 하지 않는 것:

```txt
Vulkan backend 실구현
Console SDK 구현
DX12 visual parity 전체 완성
DX12 ImGui backend 완성
Effect Tool GPU Compute
Fiber 기반 DX12 멀티스레드 command recording
```

## 3. 구조 원칙

Public RHI는 DX12/Vulkan/Console의 explicit model을 기준으로 둔다.

```txt
Engine/Public/RHI
  backend enum
  opaque handle
  surface desc
  capability desc
  interface

Engine/Private/RHI/DX12
  d3d12.h
  dxgi1_6.h
  D3D12MA
  fence
  command allocator
  swapchain
```

Win32 `HWND`는 당장 `CWin32Window`에 남아 있지만, RHI는 `RHISurfaceDesc`를 받을 수 있게 만든다. 다음 단계에서 `CWin32Window` 자체를 private platform 구현으로 밀어 Public Win32 leak를 줄인다.

## 4. 구현 단계

### 4-1. Surface와 capability 계약 추가

`RHISurfaceDesc`는 Win32, Android, iOS, Xbox, PS5 surface를 SDK 없이 표현한다.

`RHICapabilities`는 renderer가 `backend == DX12` 분기를 직접 늘리지 않고 기능 기준으로 판단하게 하는 최소 계약이다.

완료 기준:

```txt
IRHIDevice가 RHISurfaceDesc 기반 CreateSwapChain overload를 제공한다.
DX11 기존 RHIWindowHandle 경로는 그대로 컴파일된다.
DX12/Metal/Xbox/PS5 enum slot이 public contract에 생긴다.
```

### 4-2. DX12SmokeHost를 솔루션 검증 루트로 등록

`Winters.sln`에 `DX12SmokeHost`를 등록한다. Debug-DX12와 Release-DX12에서만 Build.0로 묶는다.

Client, Server, AssetConverter는 DX12 config에서 자동 fallback 또는 비빌드 상태를 유지한다. DX12 구조 안정화 전 Client를 DX12 Build.0에 묶지 않는다.

완료 기준:

```txt
Winters.sln Debug-DX12|x64
  Engine Build.0
  DX12SmokeHost Build.0
  Client/Server/AssetConverter는 DX11 config alias 또는 비빌드
```

### 4-3. DX12 device bootstrap 안정화

현 단계의 DX12 목표는 clear/present다. `BeginFrame`은 frame allocator fence를 기다린 뒤 allocator를 reset하고, clear command를 기록해 submit한다.

주의:

```txt
GPU가 읽고 있는 command allocator를 reset하면 안 된다.
frame index별 fence value를 기록해야 한다.
DX12Queue::Signal은 monotonic fence value를 사용해야 한다.
```

### 4-4. Effect Tool 연결 기준

Effect Tool은 지금 바로 시작할 수 있지만 GPU Compute와 Indirect Draw는 DX12 command model이 더 굳은 뒤에 들어간다.

Effect Tool 선행 가능:

```txt
.wfx asset
graph data model
parameter store
runtime SoA
CPU tick
editor panel
legacy Fx bridge
```

DX12 이후 진입:

```txt
GPU Compute emitter
RWStructuredBuffer attribute pool
DrawIndirect
async upload queue
GPU sort
```

## 5. 이번 작업 완료 기준

```txt
1. 계획서가 .md/TODO/05-07/RHI 아래 존재한다.
2. 샌드박스에 RHISurface/RHICapabilities public contract가 추가된다.
3. IRHIDevice가 RHISurfaceDesc overload와 GetCapabilities를 제공한다.
4. CWin32Window가 IPlatformWindow/IPlatformSurface bridge를 제공한다.
5. EngineConfig와 RHITypes가 DX12 이후 backend slot을 가진다.
6. DX12SmokeHost가 Winters.sln에 등록된다.
7. Debug-DX12 Engine 또는 DX12SmokeHost 빌드 검증을 시도하고 결과를 기록한다.
```

## 6. 다음 단계

이번 작업 뒤에는 RH-9 Public DX11 leak 제거와 RH-10 CommandList 실제화를 진행한다.

```txt
RH-9
  Engine/Public + Engine/Include + Client/Public에서 d3d11.h, ID3D11, RHI/DX11 직접 노출 제거

RH-10
  IRHICommandList SetPipeline/SetBindGroup/SetVertexBuffer/SetIndexBuffer 실제화
  DX11 immediate emulation
  DX12 command list binding path 구현
```

## 7. 샌드박스 반영 결과

반영 위치:

```txt
C:\Users\user\Desktop\Winters\.codex\worktrees\dx12-bootstrap-sandbox
```

반영 파일:

```txt
Engine/Public/RHI/RHISurface.h
Engine/Public/RHI/RHICapabilities.h
Engine/Public/Platform/PlatformTypes.h
Engine/Public/Platform/IPlatformWindow.h
Engine/Public/Platform/IPlatformSurface.h
Engine/Public/RHI/RHITypes.h
Engine/Public/RHI/IRHIDevice.h
Engine/Public/Platform/CWin32Window.h
Engine/Include/EngineConfig.h
Engine/Private/Framework/CEngineApp.cpp
Engine/Private/RHI/DX12/DX12Device.h
Engine/Private/RHI/DX12/DX12Device.cpp
Engine/Include/Engine.vcxproj
Engine/Include/Engine.vcxproj.filters
Winters.sln
```

빌드 검증:

```txt
명령:
MSBuild Winters.sln /t:Clean /p:Configuration=Debug-DX12 /p:Platform=x64
MSBuild Winters.sln /m /p:Configuration=Debug-DX12 /p:Platform=x64

결과:
Engine Debug-DX12 빌드 통과
DX12SmokeHost Debug-DX12 빌드 통과

산출물:
Engine/Bin/Debug-DX12/WintersEngine.dll
Engine/Bin/Debug-DX12/DX12SmokeHost.exe
```

주의:

```txt
첫 빌드는 stale PCH C1853으로 실패했다.
Clean 후 재빌드에서 통과했다.
남은 경고는 기존 DLL export/STL 노출 계열 C4251/C4275가 대부분이다.
```
