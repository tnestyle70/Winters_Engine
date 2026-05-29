# Ch1. RHI (Render Hardware Interface) — 기초부터 심화까지

> Winters 현재 상태: `Engine/Public/RHI/` 1차 contract + DX11 런타임 + DX12 SmokeHost 통과.
> 목표: DX12 / Vulkan / Metal / Console을 같은 추상 위에서 굴리는 production-tier RHI.
> 레퍼런스: `C:\Users\user\Desktop\UnrealEngine\UnrealEngine\Engine\Source\Runtime\RHI\` 그리고 `D3D12RHI/`, `VulkanRHI/`, `MetalRHI/`.

---

## 1. 기초 원리 — RHI가 왜 존재해야 하는가

게임 엔진이 GPU를 직접 호출하지 않는 이유는 단 하나다. **그래픽 API는 매 세대 바뀌고 플랫폼마다 다르다.**

```text
DX11   immediate context, hidden driver heuristics, fixed pipeline 잔재
DX12   explicit barrier, command list, descriptor heap, fence
Vulkan render pass, sync2, descriptor set, queue family
Metal  argument buffer, command encoder, indirect buffer
Console 각 콘솔사 전용 (NDA-locked)
```

엔진 위 코드(렌더러, 게임플레이)가 이걸 직접 만지면, 한 플랫폼 추가하는 데 수십만 줄을 다시 써야 한다. RHI는 그 사이에 끼는 **얇은 capability-aware 추상층**이다.

핵심 원칙:
1. **Don't pay for what you don't use** — DX11식 hidden heuristics 금지. barrier/sync는 명시.
2. **Common subset이 아니라 capability descriptor** — Vulkan에 없는 기능은 cap flag로 노출.
3. **Lifetime은 refcount + deferred release** — GPU가 아직 쓰는 자원을 CPU에서 free하면 죽는다.
4. **Validation layer 필수** — 디버그 빌드에서는 모든 호출이 검증된다.

---

## 2. 핵심 — UE5 RHI의 6가지 추상 객체

UE5는 다음 6개로 GPU 세계를 모델링한다:

| 객체 | 역할 | UE5 클래스 |
|------|------|------------|
| Device/RHI | API 진입점 | `FDynamicRHI` |
| Resource | GPU 메모리 (Texture/Buffer) | `FRHIResource`, `FRHITexture`, `FRHIBuffer` |
| View | Resource의 해석 (SRV/UAV/RTV) | `FRHIShaderResourceView` 등 |
| PipelineState | shader + blend + depth + raster 묶음 | `FGraphicsPipelineStateInitializer` |
| CommandList | GPU에 보낼 명령 record | `FRHICommandList` |
| Sync | CPU↔GPU, queue↔queue | `FRHIFence`, `FRHIGPUSemaphore` |

### UE5 `FDynamicRHI` 실코드 (발췌)

`UnrealEngine/Engine/Source/Runtime/RHI/Public/DynamicRHI.h:205~291`

```cpp
class FDynamicRHI
{
public:
    RHI_API virtual ~FDynamicRHI();

    virtual void Init() = 0;
    virtual void PostInit() {}
    virtual void Shutdown() = 0;

    virtual const TCHAR* GetName() = 0;
    virtual ERHIInterfaceType GetInterfaceType() const { return ERHIInterfaceType::Hidden; }

    // 모든 GPU 자원 생성은 여기로 모인다 — DX12/Vulkan/Metal이 각각 override
    virtual FSamplerStateRHIRef     RHICreateSamplerState(const FSamplerStateInitializerRHI& Init) = 0;
    virtual FRasterizerStateRHIRef  RHICreateRasterizerState(const FRasterizerStateInitializerRHI& Init) = 0;
    virtual FDepthStencilStateRHIRef RHICreateDepthStencilState(const FDepthStencilStateInitializerRHI& Init) = 0;
    virtual FBlendStateRHIRef       RHICreateBlendState(const FBlendStateInitializerRHI& Init) = 0;

    virtual FVertexShaderRHIRef     RHICreateVertexShader(TArrayView<const uint8> Code, const FSHAHash& Hash) = 0;
    virtual FPixelShaderRHIRef      RHICreatePixelShader(TArrayView<const uint8> Code, const FSHAHash& Hash) = 0;
    virtual FComputeShaderRHIRef    RHICreateComputeShader(TArrayView<const uint8> Code, const FSHAHash& Hash) = 0;

    virtual void RHIEndFrame(const FRHIEndFrameArgs& Args) = 0;
};
```

**관전 포인트**:
- 모든 함수가 `= 0`. 백엔드(DX12RHI, VulkanRHI 등)가 각자 구현.
- 반환은 `Ref` (refcount handle). 호출자가 명시 release할 필요 없다.
- `FSHAHash`: shader bytecode 해시. PSO 캐시 키.

### UE5 백엔드 등록 패턴

`Source/Runtime/D3D12RHI/Public/D3D12RHI.h` 같은 파일에서:

```cpp
class FD3D12DynamicRHIModule : public IDynamicRHIModule
{
public:
    virtual bool IsSupported() override;
    virtual FDynamicRHI* CreateRHI(ERHIFeatureLevel::Type RequestedFeatureLevel) override;
};

IMPLEMENT_MODULE(FD3D12DynamicRHIModule, D3D12RHI);
```

런타임은 ini의 `[/Script/Engine.RendererSettings] r.DefaultGraphicsRHI=DX12`를 보고 모듈을 로드하고, `CreateRHI`로 `FD3D12DynamicRHI` 인스턴스 하나를 만들어 글로벌 `GDynamicRHI`에 박는다. 위 코드의 모든 가상 호출이 이 인스턴스로 dispatch.

### CommandList: record/submit 분리

UE5는 게임 스레드/렌더 스레드/RHI 스레드로 분리되어 있다. 렌더 스레드가 **명령을 record**하고, RHI 스레드가 **GPU에 submit**한다.

`Source/Runtime/RHI/Public/RHICommandList.h` 코어 아이디어:

```cpp
class FRHICommandListImmediate : public FRHICommandList
{
public:
    // record. 즉시 GPU 호출 아님.
    void SetGraphicsPipelineState(FRHIGraphicsPipelineState* PSO, ...);
    void SetShaderResourceViewParameter(FRHIShader* Shader, uint32 Index, FRHIShaderResourceView* SRV);
    void DrawIndexedPrimitive(FRHIBuffer* IB, int32 BaseVtx, uint32 FirstIdx, uint32 NumPrim, ...);

    // RHI 스레드로 flush. 비동기 / 동기 모두 가능.
    void Flush();
    void WaitForRHIThreadTasks();
};
```

**왜 중요한가**: 게임 스레드 60fps + 렌더 스레드 60fps + GPU 60fps이 파이프라인 처리되도록 한다. 렌더가 GPU를 기다리지 않고, GPU가 렌더를 기다리지 않는다.

---

## 3. 심화 — production tier에서 신경 써야 하는 것

### 3.1 Barrier / Transition

DX11에서는 driver가 `ResourceState` 추적을 해줬다. DX12/Vulkan은 명시.

UE5 `FRHITransitionInfo`:

```cpp
struct FRHITransitionInfo
{
    union {
        FRHITexture*      Texture;
        FRHIBuffer*       Buffer;
        FRHIUnorderedAccessView* UAV;
    };
    ERHIAccess AccessBefore;   // SRVCompute, UAVCompute, RTV, DepthWrite ...
    ERHIAccess AccessAfter;
    EResourceTransitionFlags Flags;
};
```

`RHICmdList.Transition({...})` 한 번으로 여러 자원의 상태 전이를 모아 발급. **자동 추적이 아니라 명시.** RDG(Ch2)가 이걸 자동으로 잡아주는 것이 RDG의 존재 이유.

### 3.2 Descriptor Heap / Binding Model

DX12: `D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV` 힙에 view를 미리 박아두고 GPU handle만 넘김.
Vulkan: `VkDescriptorSet`을 미리 만들어 `vkCmdBindDescriptorSets`.

UE5는 `FRHIDescriptorHandle`로 통일하고 백엔드별로 매핑.

### 3.3 PSO 캐시

GraphicsPipelineState 생성은 ms 단위로 느릴 수 있다. 첫 실행에서 hitch가 생기는 주범.

UE5 해결책: `PipelineStateCache` + `PipelineFileCache` — 빌드 시 PSO를 cook하고 런타임에 prewarm.

```cpp
// Source/Runtime/RHI/Public/PipelineStateCache.h
namespace PipelineStateCache
{
    FGraphicsPipelineState* GetAndOrCreateGraphicsPipelineState(
        FRHICommandList& RHICmdList,
        const FGraphicsPipelineStateInitializer& Initializer,
        EApplyRendertargetOption ApplyFlags);
}
```

### 3.4 Multi-GPU / Async Compute

`FRHIGPUMask`로 어느 GPU에 명령을 보낼지 명시. SLI/AFR 또는 mGPU 워크로드에 사용. Compute queue는 별도 `FRHICommandList`로 graphics와 병렬 실행.

### 3.5 Validation Layer

`-rhivalidation` 또는 `r.RHIValidation=1`로 켜면 자원 생명주기, barrier 정합성, descriptor binding 정합성을 전부 검사. **Debug에서 켜고 Release에서 끈다.**

---

## 4. Winters 매핑

### 현재 Winters RHI 1차 contract (실코드)

`Engine/Public/RHI/RHISurface.h:7~37`:

```cpp
enum class eRHIPlatformSurfaceType : u32_t
{
    Unknown = 0,
    Win32HWND,
    AndroidNativeWindow,
    IOSMetalLayer,
    XboxNative,
    PS5Native,
};

struct WINTERS_ENGINE RHISurfaceDesc
{
    eRHIPlatformSurfaceType type = eRHIPlatformSurfaceType::Unknown;
    void* nativeHandle = nullptr;
    void* nativeDisplay = nullptr;
    u32_t width = 0;
    u32_t height = 0;
    bool_t vsync = true;
    bool_t fullscreen = false;
    eRHISurfaceLifecycleState lifecycleState = eRHISurfaceLifecycleState::Active;
};
```

UE5 `FRHIWindowSurface`에 해당. 현재 골격은 옳다. 다음 단계는 **Resource/CommandList/Sync 추상**을 같은 톤으로 추가.

### Winters Ch1 추가 헤더 (제안 anchor)

```text
Engine/Public/RHI/
  RHIResource.h           // FRHIResource 등가 — refcount, virtual ~
  RHITexture.h            // FRHITexture 등가
  RHIBuffer.h             // FRHIBuffer 등가
  RHIShader.h             // 컴파일된 shader binary handle
  RHIPipelineState.h      // GraphicsPSO / ComputePSO initializer + cache
  RHICommandList.h        // record API
  RHIFence.h, RHIQueue.h  // sync primitive
  RHIDescriptorHandle.h   // 백엔드 통합 handle
  IDynamicRHI.h           // FDynamicRHI 등가 — virtual create*

Engine/Private/RHI/
  DX11/  (legacy 유지)
  DX12/  (확장: barrier 자동 추적, PSO 캐시 prewarm)
  Vulkan/ (신규)
  Metal/  (신규)
  Null/   (CI/dedicated server)
```

### Winters Ch1 단계별 완료 기준

```text
Stage 1  IDynamicRHI + DX12 backend 1개로 Client visual parity
         (현재 SmokeHost는 통과, Client는 미완)
Stage 2  RHIResource refcount + deferred release
         (FRHITextureRef 등 RAII handle 도입)
Stage 3  Explicit barrier (RHITransitionInfo 등가)
         이 단계까지 와야 Ch2 RDG 시작 가능
Stage 4  Vulkan backend 추가 (모바일/리눅스/Switch)
Stage 5  Metal backend
Stage 6  Console (NDA 영역)
Stage 7  PSO cache + prewarm + validation layer
```

### Bot AI / GameSim과의 무관성

RHI는 **순수 렌더 레이어**다. Bot AI / GameSim은 RHI를 모른다.
- 서버: `Server/`가 RHI 호출하지 않도록 cmake 의존성에서 차단.
- AI: `Shared/GameSim/`는 `Engine/Public/RHI`를 include하지 못하도록 강제.

이게 `S10_BotAIStage1` 안정화 작업과 **충돌하지 않는** 이유. Ch1은 Client/Engine 축, AI는 Shared/GameSim 축, 디렉토리 차원에서 분리.

---

## 5. 검증 명령

```powershell
# DX12 SmokeHost 8초 생존 (현재 통과 중)
MSBuild Winters.sln /t:DX12SmokeHost /m /p:Configuration=Debug-DX12 /p:Platform=x64
$p = Start-Process ".\Engine\Bin\Debug-DX12\DX12SmokeHost.exe" -PassThru
Start-Sleep 8
$p.HasExited  # False여야 함

# Vulkan backend가 들어오면 동일 패턴으로 VulkanSmokeHost.exe
```

기대 로그:
```text
[RHI][DX12] device created (Adapter=NVIDIA RTX 4090, FeatureLevel=12_2)
[RHI][DX12] swapchain created (1920x1080, BGRA8, BufferCount=3)
[RHI][DX12] frame N present ok
```

---

## 6. 다음 챕터로

Ch1이 Stage 3까지(explicit barrier) 가야 **Ch2 RenderGraph**를 시작할 수 있다. Ch2가 자동 barrier 추적의 entry point다.
> 2026-05-25 update: `DX12SmokeHost` was removed. Current RHI validation should use Engine/Client build configurations and runtime backend selection, not standalone DX12 smoke executables.
