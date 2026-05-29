# Phase RH-1 Sub-plan: Interface Extraction (★ Codex 2차 보정판)

**작성일**: 2026-04-30 (Codex 2차 검토 보정 2026-04-30)
**상위 문서**: `00_RHI_MIGRATION_MASTER.md` §2
**범위**: 9개 인터페이스 헤더 + 2개 supporting (RHITypes / RHIDescriptors / RHIWindowHandle) + DX11 어댑터 (★ 신규 cpp 파일 X — 기존 `CDX11Device.cpp` 에 합침) + GameInstance Tier 2 신규 이름 추가
**합격**: 자원 생성이 `device->CreateBuffer(desc) -> RHIBufferHandle` 통과 (★ handle API), `Get_NewRHIDevice() -> IRHIDevice*` 정식 가동

**한 줄**: **★ Codex 2차 보정 — RH-1 = 9개 인터페이스 + handle API (Engine-owned, DLL CRT 충돌 방지) + `Get_NewRHIDevice()` 신규 이름 (RH-0 _Legacy 와 양립) + DX11Buffer 4종 병행 (alias X) + IBuffer.h shim 유지.**

---

## ★ Codex 2차 검토 변경 요약

| 변경 | 이전 RH-1 (1차) | 신규 RH-1 (2차) |
|---|---|---|
| 자원 생성 API | `unique_ptr<IRHIBuffer> CreateBuffer(desc)` | **`RHIBufferHandle CreateBuffer(desc)` + `Destroy(handle)`** (P0-5) |
| GameInstance 신규 getter | `Get_RHIDevice() -> IRHIDevice*` (이름 충돌) | **`Get_NewRHIDevice() -> IRHIDevice*`** (P0-4) |
| DX11 어댑터 cpp | `DX11DeviceAdapter.cpp` 신규 | **기존 `CDX11Device.cpp` 에 IRHIDevice impl 합침** (P0-6) |
| Buffer 4종 처리 | `using CDX11VertexBuffer = CDX11Buffer` alias | **기존 4종 유지 + 신규 `CDX11Buffer` 병행** (P0-7) |
| `IBuffer.h` 처리 | 삭제 + IRHIBuffer.h 교체 | **`IBuffer.h` shim 유지** (deprecated alias, P2-26) |
| HWND 처리 | `RHIDeviceDesc.hwnd` 직접 노출 | **`RHIWindowHandle` wrapper** (P1-9) |
| 신규 코드 `bool` | raw `bool` | **`bool_t` 강제** (P1-10) |
| backend 클래스 `final` | 미적용 | **`final` 키워드 추가** (P1-11) |
| `GetNativeHandle()` ownership | 모호 | **"borrowed pointer, AddRef X, 즉시 사용만"** 정책 명시 (P1-12) |
| Shader bytecode/reflection | 모호 | **lifetime = shader 객체 동일** 명시 (P2-27) |

---

## 1. 신규 파일 목록 (총 11개 헤더 + 0개 신규 cpp)

### 1.1 Engine/Public/RHI/ — 인터페이스 + 공용 타입

```
Engine/Public/RHI/
├── RHITypes.h            (신규 — enum 모음)
├── RHIDescriptors.h      (신규 — desc struct 모음)
├── RHIWindowHandle.h     (★ 신규 — HWND wrapper, P1-9)
├── RHIHandles.h          (★ 신규 — 64-bit packed handle, P1-16 사전 도입)
├── IRHIDevice.h          (신규)
├── IRHISwapChain.h       (신규)
├── IRHIQueue.h           (신규)
├── IRHIBuffer.h          (★ 신규 — IBuffer.h 와 병행)
├── IRHITexture.h         (신규)
├── IRHIShader.h          (신규)
├── IRHISampler.h         (신규)
├── IBuffer.h             (★ 기존 유지 + deprecated alias 추가, P2-26)
└── CDX11Device.h         (★ 수정 — : public IRHIDevice 추가)
```

### 1.2 Engine/Private/RHI/DX11/ — DX11 어댑터 (★ 신규 cpp 파일 X, P0-6)

```
Engine/Private/RHI/DX11/
├── CDX11Device.cpp       (★ 수정 — IRHIDevice impl 합침, 신규 cpp 만들지 않음)
├── DX11Buffer.cpp        (★ 신규 — IRHIBuffer impl, 기존 4종과 병행)
├── DX11Texture.cpp       (신규 — IRHITexture)
├── DX11Shader.cpp        (★ 수정 — IRHIShader impl 추가)
├── DX11Sampler.cpp       (신규 — IRHISampler)
├── DX11SwapChain.cpp     (신규 — IRHISwapChain wrapping)
├── DX11Queue.cpp         (신규 — IRHIQueue wrapping)
└── (기타 RH-0 그대로 — 9개 헤더는 RH-0 시점에서 Public 그대로 유지)
```

---

## 2. `Engine/Public/RHI/RHITypes.h` (신규, ★ 전문)

(이전 1차 plan §2 와 동일 — 변경 없음. 본 plan 에 한정 박제)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"

// ─────────────────────────────────────────────────────────────────
//  RHITypes.h  |  Backend-Neutral Enums
//
//  DX11/DX12/Vulkan 모두 이 enum 들로 표현.
// ─────────────────────────────────────────────────────────────────

enum class eRHIResourceState : u32_t
{
    Undefined           = 0,
    Common              = 1 << 0,
    VertexBuffer        = 1 << 1,
    IndexBuffer         = 1 << 2,
    ConstantBuffer      = 1 << 3,
    ShaderResource      = 1 << 4,
    UnorderedAccess     = 1 << 5,
    RenderTarget        = 1 << 6,
    DepthWrite          = 1 << 7,
    DepthRead           = 1 << 8,
    CopySource          = 1 << 9,
    CopyDest            = 1 << 10,
    Present             = 1 << 11,
};

enum class eRHIFormat : u32_t
{
    Unknown = 0,
    R8_UNorm,
    R8G8_UNorm,
    R8G8B8A8_UNorm,
    R8G8B8A8_UNorm_SRGB,
    B8G8R8A8_UNorm,
    B8G8R8A8_UNorm_SRGB,
    R16_Float,
    R16G16_Float,
    R16G16B16A16_Float,
    R16_UInt,
    R16G16_UInt,
    R32_Float,
    R32G32_Float,
    R32G32B32_Float,
    R32G32B32A32_Float,
    R32_UInt,
    R32G32_UInt,
    D16_UNorm,
    D24_UNorm_S8_UInt,
    D32_Float,
    D32_Float_S8_UInt,
    BC1_UNorm,
    BC3_UNorm,
    BC5_UNorm,
    BC7_UNorm,
};

enum class eRHIPrimitiveTopology : u8_t
{
    PointList = 0,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip,
};

enum class eRHIBufferType : u8_t
{
    Vertex = 0,
    Index,
    Constant,
    Structured,
    Indirect,
};

enum class eRHIBufferUsage : u8_t
{
    Immutable = 0,
    Default,
    Dynamic,
    Staging,
};

enum class eRHIIndexFormat : u8_t
{
    UInt16 = 0,
    UInt32,
};

enum class eRHITextureDimension : u8_t
{
    Texture1D = 0,
    Texture2D,
    Texture3D,
    TextureCube,
    Texture2DArray,
    TextureCubeArray,
};

enum class eRHITextureUsage : u32_t
{
    None             = 0,
    ShaderResource   = 1 << 0,
    RenderTarget     = 1 << 1,
    DepthStencil     = 1 << 2,
    UnorderedAccess  = 1 << 3,
    CopySource       = 1 << 4,
    CopyDest         = 1 << 5,
};

enum class eRHIShaderStage : u8_t
{
    Vertex = 0,
    Pixel,
    Geometry,
    Hull,
    Domain,
    Compute,
};

enum class eRHIFilter : u8_t
{
    MinMagMipPoint = 0,
    MinMagPointMipLinear,
    MinPointMagLinearMipPoint,
    MinMagLinearMipPoint,
    MinMagMipLinear,
    Anisotropic,
};

enum class eRHIAddressMode : u8_t
{
    Wrap = 0,
    Mirror,
    Clamp,
    Border,
};

enum class eRHIComparison : u8_t
{
    Never = 0,
    Less,
    Equal,
    LessEqual,
    Greater,
    NotEqual,
    GreaterEqual,
    Always,
};

enum class eRHICullMode : u8_t
{
    None = 0,
    Front,
    Back,
};

enum class eRHIFillMode : u8_t
{
    Wireframe = 0,
    Solid,
};

enum class eRHIBlend : u8_t
{
    Zero = 0,
    One,
    SrcColor,
    InvSrcColor,
    SrcAlpha,
    InvSrcAlpha,
    DstAlpha,
    InvDstAlpha,
    DstColor,
    InvDstColor,
};

enum class eRHIBlendOp : u8_t
{
    Add = 0,
    Subtract,
    RevSubtract,
    Min,
    Max,
};

enum class eRHILoadOp : u8_t
{
    Load = 0,
    Clear,
    DontCare,
};

enum class eRHIStoreOp : u8_t
{
    Store = 0,
    DontCare,
};

enum class eRHIQueueType : u8_t
{
    Graphics = 0,
    Compute,
    Copy,
};

enum class eRHINativeHandleType : u8_t
{
    DX11_Device = 0,
    DX11_DeviceContext,
    DX11_SwapChain,
    DX11_BackBufferRTV,
    DX11_DepthStencilView,
    DX12_Device,
    DX12_CommandQueue,
    DX12_SwapChain,
    Vulkan_Instance,
    Vulkan_Device,
    Vulkan_PhysicalDevice,
    Vulkan_Queue,
};
```

---

## 3. `Engine/Public/RHI/RHIWindowHandle.h` (★ 신규 — Codex P1-9 보정, 전문)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"

// ─────────────────────────────────────────────────────────────────
//  RHIWindowHandle  |  Platform-neutral window surface descriptor
//
//  ★ Codex P1-9 보정 — Vulkan/Linux 까지 목표면 HWND 직접 노출 X.
//
//  사용 예:
//    RHIWindowHandle wh;
//    wh.platform = eRHIWindowPlatform::Win32;
//    wh.win32.hwnd     = GetActiveWindow();
//    wh.win32.hinstance = GetModuleHandleW(nullptr);
// ─────────────────────────────────────────────────────────────────

enum class eRHIWindowPlatform : u8_t
{
    Win32 = 0,
    Xlib,           // Linux X11
    Wayland,        // Linux Wayland
    MacOS_NSView,   // macOS / MoltenVK
    Android,
};

struct RHIWin32WindowHandle
{
    void* hwnd       = nullptr;   // HWND
    void* hinstance  = nullptr;   // HINSTANCE
};

struct RHIXlibWindowHandle
{
    void* dpy        = nullptr;   // Display*
    u64_t window     = 0;         // Window (XID)
};

struct RHIWaylandWindowHandle
{
    void* display    = nullptr;   // wl_display*
    void* surface    = nullptr;   // wl_surface*
};

struct RHINSViewWindowHandle
{
    void* nsView     = nullptr;   // NSView*
};

struct RHIAndroidWindowHandle
{
    void* aNativeWindow = nullptr;   // ANativeWindow*
};

struct RHIWindowHandle
{
    eRHIWindowPlatform platform = eRHIWindowPlatform::Win32;

    union
    {
        RHIWin32WindowHandle    win32;
        RHIXlibWindowHandle     xlib;
        RHIWaylandWindowHandle  wayland;
        RHINSViewWindowHandle   nsview;
        RHIAndroidWindowHandle  android;
    };

    RHIWindowHandle() : win32{} {}
};
```

---

## 4. `Engine/Public/RHI/RHIHandles.h` (★ 신규 — Codex P1-16 사전 도입, 전문)

★ **Codex P1-16 보정 — 64-bit handle (32 index + 32 generation)**. RH-1 부터 도입 (RH-4 의 강화 버전이 본 위치 확장).

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"

// ─────────────────────────────────────────────────────────────────
//  RHIHandles.h  |  64-bit packed resource handle
//
//  ★ Codex P1-16 보정 — 32-bit (24 index + 8 gen) 은 generation wrap 너무 빠름.
//    64-bit (32 index + 32 generation) 으로 churn 안전.
//
//  특징:
//    - Trivially copyable (POD)
//    - Tag types 로 컴파일 타임 안전 (BufferHandle != TextureHandle)
//    - 0 (kInvalid) 는 빈 handle 보장
// ─────────────────────────────────────────────────────────────────

template<typename Tag>
struct RHIHandle
{
    u64_t value = 0;     // [63:32] generation, [31:0] index

    static constexpr u64_t kInvalid = 0;

    bool_t IsValid() const { return value != kInvalid; }

    u32_t Index()      const { return static_cast<u32_t>(value & 0xFFFFFFFFu); }
    u32_t Generation() const { return static_cast<u32_t>((value >> 32) & 0xFFFFFFFFu); }

    static RHIHandle Make(u32_t index, u32_t generation)
    {
        RHIHandle h;
        h.value = (static_cast<u64_t>(generation) << 32) | static_cast<u64_t>(index);
        return h;
    }

    bool_t operator==(const RHIHandle& o) const { return value == o.value; }
    bool_t operator!=(const RHIHandle& o) const { return value != o.value; }
};

// Tag types — 컴파일 타임 안전
struct RHIBufferTag       {};
struct RHITextureTag      {};
struct RHIShaderTag       {};
struct RHISamplerTag      {};
struct RHIPipelineTag     {};
struct RHIBindGroupTag    {};
struct RHIRenderPassTag   {};

using RHIBufferHandle     = RHIHandle<RHIBufferTag>;
using RHITextureHandle    = RHIHandle<RHITextureTag>;
using RHIShaderHandle     = RHIHandle<RHIShaderTag>;
using RHISamplerHandle    = RHIHandle<RHISamplerTag>;
using RHIPipelineHandle   = RHIHandle<RHIPipelineTag>;
using RHIBindGroupHandle  = RHIHandle<RHIBindGroupTag>;
using RHIRenderPassHandle = RHIHandle<RHIRenderPassTag>;

inline constexpr RHIBufferHandle    kInvalidBufferHandle    = {};
inline constexpr RHITextureHandle   kInvalidTextureHandle   = {};
inline constexpr RHIShaderHandle    kInvalidShaderHandle    = {};
inline constexpr RHISamplerHandle   kInvalidSamplerHandle   = {};
inline constexpr RHIPipelineHandle  kInvalidPipelineHandle  = {};
inline constexpr RHIBindGroupHandle kInvalidBindGroupHandle = {};
```

---

## 5. `Engine/Public/RHI/RHIDescriptors.h` (신규, ★ 전문)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "RHI/RHITypes.h"
#include "RHI/RHIWindowHandle.h"

// ─────────────────────────────────────────────────────────────────
//  RHIDescriptors.h  |  Backend-Neutral Resource Descriptors
//
//  ★ Codex P1-10 보정 — bool → bool_t 전수 적용
//  ★ Codex P1-9 보정 — HWND → RHIWindowHandle wrapper
// ─────────────────────────────────────────────────────────────────

struct RHIDeviceDesc
{
    RHIWindowHandle  windowHandle = {};
    u32_t            width        = 1280;
    u32_t            height       = 720;
    bool_t           bVSync       = true;
    bool_t           bFullscreen  = false;
    bool_t           bDebug       = false;
};

struct RHISwapChainDesc
{
    RHIWindowHandle  windowHandle = {};
    u32_t            width        = 1280;
    u32_t            height       = 720;
    eRHIFormat       colorFormat  = eRHIFormat::R8G8B8A8_UNorm;
    eRHIFormat       depthFormat  = eRHIFormat::D24_UNorm_S8_UInt;
    u32_t            bufferCount  = 2;
    bool_t           bVSync       = true;
};

struct RHIBufferDesc
{
    eRHIBufferType   type         = eRHIBufferType::Vertex;
    eRHIBufferUsage  usage        = eRHIBufferUsage::Immutable;
    u32_t            sizeBytes    = 0;
    u32_t            strideBytes  = 0;
    const void*      pInitialData = nullptr;
};

struct RHITextureDesc
{
    eRHITextureDimension dimension    = eRHITextureDimension::Texture2D;
    eRHIFormat           format       = eRHIFormat::R8G8B8A8_UNorm;
    u32_t                width        = 0;
    u32_t                height       = 0;
    u32_t                depth        = 1;
    u32_t                mipLevels    = 1;
    u32_t                arraySize    = 1;
    u32_t                sampleCount  = 1;
    u32_t                usageFlags   = 0;
    const void*          pInitialData = nullptr;
};

struct RHIShaderDesc
{
    eRHIShaderStage stage       = eRHIShaderStage::Vertex;
    const wchar_t*  filepath    = nullptr;
    const char*     entryPoint  = "main";
    const char*     shaderModel = "vs_5_0";
};

struct RHISamplerDesc
{
    eRHIFilter      filter        = eRHIFilter::MinMagMipLinear;
    eRHIAddressMode addressU      = eRHIAddressMode::Clamp;
    eRHIAddressMode addressV      = eRHIAddressMode::Clamp;
    eRHIAddressMode addressW      = eRHIAddressMode::Clamp;
    eRHIComparison  comparison    = eRHIComparison::Never;
    u32_t           maxAnisotropy = 1;
    f32_t           minLOD        = 0.f;
    f32_t           maxLOD        = 3.402823466e+38f;   // FLT_MAX
};

// (RH-3 추후) Pipeline / RenderPass / BindGroup descriptor — stub
struct RHIBlendDesc;
struct RHIDepthStencilDesc;
struct RHIRasterizerDesc;
struct RHIInputLayoutDesc;
struct RHIGraphicsPipelineDesc;
struct RHIRenderPassDesc;
struct RHIBindGroupLayoutDesc;
struct RHIBindGroupDesc;
```

---

## 6. `Engine/Public/RHI/IRHIBuffer.h` (★ 신규, Codex P2-26 보정 — IBuffer.h shim 유지, 전문)

★ **Codex P2-26 보정**: 기존 `IBuffer.h` 유지. `IRHIBuffer.h` 는 신규 추가. `IBuffer = IRHIBuffer` deprecated alias.

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "RHI/RHITypes.h"

// ─────────────────────────────────────────────────────────────────
//  IRHIBuffer  |  백엔드 중립 버퍼 인터페이스
//
//  ★ Codex P0-5 보정 — 본 인터페이스는 Engine-internal only.
//    Client 는 RHIBufferHandle (값) 만 사용 — DLL CRT 충돌 방지.
//    `IRHIDevice::ResolveBuffer(handle)` 만 본 인터페이스 노출.
// ─────────────────────────────────────────────────────────────────

class IRHIBuffer
{
public:
    virtual ~IRHIBuffer() = default;

    IRHIBuffer(const IRHIBuffer&) = delete;
    IRHIBuffer& operator=(const IRHIBuffer&) = delete;

    virtual eRHIBufferType   GetType()        const = 0;
    virtual eRHIBufferUsage  GetUsage()       const = 0;
    virtual u32_t            GetSizeBytes()   const = 0;
    virtual u32_t            GetStrideBytes() const = 0;

    // Map / Unmap (Dynamic only)
    virtual void* Map()   = 0;
    virtual void  Unmap() = 0;

    // Resource state
    virtual eRHIResourceState GetCurrentState() const = 0;
    virtual void              SetCurrentState(eRHIResourceState s) = 0;

    // ★ Codex P1-12 — Native handle ownership 정책:
    //   - Borrowed pointer 만 반환. AddRef X.
    //   - Caller 는 즉시 사용만 — 저장 / 다른 thread 전달 X.
    //   - wrong type 호출 시 nullptr (debug 빌드에서 assert).
    virtual void* GetNativeHandle(eRHINativeHandleType t) const = 0;

protected:
    IRHIBuffer() = default;
};
```

### 6.1 `Engine/Public/RHI/IBuffer.h` shim 유지 (★ Codex P2-26)

**기존 `IBuffer.h` 는 절대 삭제 X**. 다음으로 변경:

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "RHI/IRHIBuffer.h"      // ★ RH-1 신규
#include "RHI/RHITypes.h"

// ─────────────────────────────────────────────────────────────
//  IBuffer.h  |  Legacy shim — RH-1 부터 IRHIBuffer 사용 권장
//
//  기존 caller 호환을 위해 유지. 신규 코드는 IRHIBuffer 직접 사용.
//  RH-2 종료 시점에 IBuffer alias 폐기 검토.
// ─────────────────────────────────────────────────────────────

// 기존 enum 유지 (caller 가 사용)
enum class BufferType : u8_t
{
    Vertex,
    Index,
    Constant,
    Structured,
};

enum class BufferUsage : u8_t
{
    Immutable,
    Dynamic,
    Default,
    Staging,
};

// ★ deprecated alias — RH-1 부터 IRHIBuffer 사용
using IBuffer [[deprecated("Use IRHIBuffer (RH-1+)")]] = IRHIBuffer;
```

---

## 7. `Engine/Public/RHI/IRHITexture.h` (신규, ★ 전문)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "RHI/RHITypes.h"

class IRHITexture
{
public:
    virtual ~IRHITexture() = default;

    IRHITexture(const IRHITexture&) = delete;
    IRHITexture& operator=(const IRHITexture&) = delete;

    virtual eRHITextureDimension GetDimension()   const = 0;
    virtual eRHIFormat           GetFormat()      const = 0;
    virtual u32_t                GetWidth()       const = 0;
    virtual u32_t                GetHeight()      const = 0;
    virtual u32_t                GetDepth()       const = 0;
    virtual u32_t                GetMipLevels()   const = 0;
    virtual u32_t                GetArraySize()   const = 0;
    virtual u32_t                GetSampleCount() const = 0;
    virtual u32_t                GetUsageFlags()  const = 0;

    virtual eRHIResourceState    GetCurrentState() const = 0;
    virtual void                 SetCurrentState(eRHIResourceState s) = 0;

    // ★ Codex P1-12 — borrowed 정책 (IRHIBuffer 와 동일)
    virtual void* GetNativeHandle(eRHINativeHandleType t) const = 0;

protected:
    IRHITexture() = default;
};
```

---

## 8. `Engine/Public/RHI/IRHIShader.h` (★ Codex P2-27 보정, 전문)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "RHI/RHITypes.h"

// ─────────────────────────────────────────────────────────────────
//  IRHIShader  |  컴파일된 셰이더 객체
//
//  ★ Codex P2-27 보정 — bytecode/reflection lifetime 정책:
//    - GetBytecode() / GetBytecodeSize() 가 반환하는 포인터는
//      shader 객체 lifetime 동안 유효 (borrowed).
//    - PSO 가 본 bytecode 를 참조하므로, shader 가 PSO 보다 오래 살아야 함.
//    - DX11 input layout 생성용 VS bytecode 도 본 인터페이스로 접근.
// ─────────────────────────────────────────────────────────────────

class IRHIShader
{
public:
    virtual ~IRHIShader() = default;

    IRHIShader(const IRHIShader&) = delete;
    IRHIShader& operator=(const IRHIShader&) = delete;

    virtual eRHIShaderStage GetStage() const = 0;

    // Reflection (RH-3 PSO 자동 binding 추출용)
    virtual u32_t GetConstantBufferCount() const = 0;
    virtual u32_t GetTextureSlotCount()    const = 0;
    virtual u32_t GetSamplerSlotCount()    const = 0;

    // ★ Codex P2-27 — bytecode borrowed pointer.
    //   lifetime = shader 객체와 동일. PSO 생성 후 shader 보존 필수.
    virtual const void* GetBytecode()     const = 0;
    virtual u32_t       GetBytecodeSize() const = 0;

    virtual void* GetNativeHandle(eRHINativeHandleType t) const = 0;

protected:
    IRHIShader() = default;
};
```

---

## 9. `Engine/Public/RHI/IRHISampler.h` (신규, ★ 전문)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "RHI/RHITypes.h"

class IRHISampler
{
public:
    virtual ~IRHISampler() = default;

    IRHISampler(const IRHISampler&) = delete;
    IRHISampler& operator=(const IRHISampler&) = delete;

    virtual void* GetNativeHandle(eRHINativeHandleType t) const = 0;

protected:
    IRHISampler() = default;
};
```

---

## 10. `Engine/Public/RHI/IRHISwapChain.h` (신규, ★ 전문)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "RHI/RHITypes.h"
#include "RHI/RHIHandles.h"

class IRHISwapChain
{
public:
    virtual ~IRHISwapChain() = default;

    IRHISwapChain(const IRHISwapChain&) = delete;
    IRHISwapChain& operator=(const IRHISwapChain&) = delete;

    // ★ Handle API (P0-5)
    virtual RHITextureHandle GetCurrentBackBuffer() = 0;
    virtual RHITextureHandle GetDepthStencilBuffer() = 0;

    virtual u32_t GetCurrentBackBufferIndex() const = 0;
    virtual u32_t GetBufferCount()            const = 0;

    virtual u32_t GetWidth()  const = 0;
    virtual u32_t GetHeight() const = 0;

    virtual void  Resize(u32_t width, u32_t height) = 0;
    virtual void  Present() = 0;

    virtual void* GetNativeHandle(eRHINativeHandleType t) const = 0;

protected:
    IRHISwapChain() = default;
};
```

---

## 11. `Engine/Public/RHI/IRHIQueue.h` (신규, ★ 전문)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "RHI/RHITypes.h"

class IRHICommandList;   // RH-2

class IRHIQueue
{
public:
    virtual ~IRHIQueue() = default;

    IRHIQueue(const IRHIQueue&) = delete;
    IRHIQueue& operator=(const IRHIQueue&) = delete;

    virtual eRHIQueueType GetType() const = 0;

    virtual void Submit(IRHICommandList* const* pCmdLists, u32_t count) = 0;
    virtual void WaitIdle() = 0;

    virtual void* GetNativeHandle(eRHINativeHandleType t) const = 0;

protected:
    IRHIQueue() = default;
};
```

---

## 12. `Engine/Public/RHI/IRHIDevice.h` (★ 핵심 — Codex P0-5 handle API, 전문)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "RHI/RHITypes.h"
#include "RHI/RHIHandles.h"
#include "RHI/RHIDescriptors.h"

class IRHIBuffer;
class IRHITexture;
class IRHIShader;
class IRHISampler;
class IRHISwapChain;
class IRHIQueue;
class IRHICommandList;     // RH-2
class IRHIPipelineState;   // RH-3

// ─────────────────────────────────────────────────────────────────
//  IRHIDevice  |  RHI Root — 자원 생성 + Queue / SwapChain 제공
//
//  ★ Codex P0-5 보정 — 자원 생성 = handle 반환 (Engine-owned).
//    DLL boundary 에서 unique_ptr<IRHI*> 반환 시 Client CRT 가 Engine 의
//    concrete object 를 delete 하는 위험 제거.
//
//  사용:
//    RHIBufferHandle vb = device->CreateBuffer(RHIBufferDesc{...});
//    cmd->SetVertexBuffer(0, vb);
//    device->DestroyBuffer(vb);   // 명시 destroy
//
//  Internal:
//    IRHIBuffer* p = device->ResolveBuffer(vb);   // Engine-only 인터페이스 접근
// ─────────────────────────────────────────────────────────────────

class IRHIDevice
{
public:
    virtual ~IRHIDevice() = default;

    IRHIDevice(const IRHIDevice&) = delete;
    IRHIDevice& operator=(const IRHIDevice&) = delete;

    // ─────────────────────────────────────────────────
    // 자원 생성 — handle 반환 (★ Codex P0-5)
    // ─────────────────────────────────────────────────
    virtual RHIBufferHandle  CreateBuffer (const RHIBufferDesc&  desc) = 0;
    virtual RHITextureHandle CreateTexture(const RHITextureDesc& desc) = 0;
    virtual RHIShaderHandle  CreateShader (const RHIShaderDesc&  desc) = 0;
    virtual RHISamplerHandle CreateSampler(const RHISamplerDesc& desc) = 0;

    // ─────────────────────────────────────────────────
    // 자원 destroy — 명시 호출 (★ Codex P0-5)
    // ─────────────────────────────────────────────────
    virtual void DestroyBuffer (RHIBufferHandle  h) = 0;
    virtual void DestroyTexture(RHITextureHandle h) = 0;
    virtual void DestroyShader (RHIShaderHandle  h) = 0;
    virtual void DestroySampler(RHISamplerHandle h) = 0;

    // ─────────────────────────────────────────────────
    // 자원 resolve — Engine internal (백엔드 / 내부 시스템 한정)
    //   ★ borrowed pointer. lifetime = 본 device 가 관리.
    // ─────────────────────────────────────────────────
    virtual IRHIBuffer*  ResolveBuffer (RHIBufferHandle  h) = 0;
    virtual IRHITexture* ResolveTexture(RHITextureHandle h) = 0;
    virtual IRHIShader*  ResolveShader (RHIShaderHandle  h) = 0;
    virtual IRHISampler* ResolveSampler(RHISamplerHandle h) = 0;

    // ─────────────────────────────────────────────────
    // SwapChain / Queue (borrowed, Engine 소유)
    // ─────────────────────────────────────────────────
    virtual IRHISwapChain* GetSwapChain()     = 0;
    virtual IRHIQueue*     GetGraphicsQueue() = 0;
    virtual IRHIQueue*     GetComputeQueue()  = 0;
    virtual IRHIQueue*     GetCopyQueue()     = 0;

    // ─────────────────────────────────────────────────
    // Frame lifecycle
    // ─────────────────────────────────────────────────
    virtual void  BeginFrame() = 0;
    virtual void  EndFrame()   = 0;
    virtual u32_t GetCurrentFrameIndex() const = 0;
    virtual u32_t GetMaxFramesInFlight() const = 0;

    // ─────────────────────────────────────────────────
    // CommandList (RH-2 박제 후 정식)
    // ─────────────────────────────────────────────────
    virtual IRHICommandList* GetFrameCommandList() = 0;

    // Native escape (Codex P1-12)
    virtual void* GetNativeHandle(eRHINativeHandleType t) const = 0;

protected:
    IRHIDevice() = default;
};
```

---

## 13. `Engine/Public/RHI/CDX11Device.h` 수정 (★ Codex P0-2, P0-6, P1-11 보정, 전문)

★ 본 헤더는 RH-0 시점에 Public 그대로 유지. RH-2 종료 시점에 `Engine/Private/RHI/DX11/DX11Device.h` 로 이동.

```cpp
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "RHI/IRHIDevice.h"
#include "RHI/RHIDescriptors.h"
#include "RHI/RHIHandles.h"

#include <memory>
#include <vector>

// ─────────────────────────────────────────────────────────────────
//  CDX11Device  |  DirectX 11 Device — IRHIDevice 어댑터
//
//  ★ Codex P0-6 보정 — IRHIDevice impl 은 본 클래스 + CDX11Device.cpp
//    에 합쳐서 박제. 별도 DX11DeviceAdapter.cpp 신규 X.
//
//  ★ Codex P1-11 보정 — final 키워드 추가 (다중 상속 / 가상 함수 호출 최적화).
//
//  ★ RH-0 위치: Engine/Public/RHI/CDX11Device.h (그대로)
//  ★ RH-2 종료 후 위치: Engine/Private/RHI/DX11/DX11Device.h (이동)
// ─────────────────────────────────────────────────────────────────

class CDX11SwapChain;            // 신규
class CDX11Queue;                // 신규
class CDX11CommandList;          // RH-2 신규

template<typename T>
class CRHIResourceTable;         // 내부 lookup table — RH-1 부터 도입

class CDX11Device final : public IRHIDevice
{
public:
    ~CDX11Device() override;

    CDX11Device(const CDX11Device&)            = delete;
    CDX11Device& operator=(const CDX11Device&) = delete;
    CDX11Device(CDX11Device&&)                 = delete;
    CDX11Device& operator=(CDX11Device&&)      = delete;

    static std::unique_ptr<CDX11Device> Create(const RHIDeviceDesc& desc);

    // ─────────────────────────────────────────────────
    // IRHIDevice impl — handle API (★ Codex P0-5)
    // ─────────────────────────────────────────────────
    RHIBufferHandle  CreateBuffer (const RHIBufferDesc&  desc) override;
    RHITextureHandle CreateTexture(const RHITextureDesc& desc) override;
    RHIShaderHandle  CreateShader (const RHIShaderDesc&  desc) override;
    RHISamplerHandle CreateSampler(const RHISamplerDesc& desc) override;

    void DestroyBuffer (RHIBufferHandle  h) override;
    void DestroyTexture(RHITextureHandle h) override;
    void DestroyShader (RHIShaderHandle  h) override;
    void DestroySampler(RHISamplerHandle h) override;

    IRHIBuffer*  ResolveBuffer (RHIBufferHandle  h) override;
    IRHITexture* ResolveTexture(RHITextureHandle h) override;
    IRHIShader*  ResolveShader (RHIShaderHandle  h) override;
    IRHISampler* ResolveSampler(RHISamplerHandle h) override;

    IRHISwapChain* GetSwapChain()     override;
    IRHIQueue*     GetGraphicsQueue() override;
    IRHIQueue*     GetComputeQueue()  override;
    IRHIQueue*     GetCopyQueue()     override;

    void  BeginFrame() override;
    void  EndFrame()   override;
    u32_t GetCurrentFrameIndex() const override { return m_FrameIndex; }
    u32_t GetMaxFramesInFlight() const override { return 1; }

    IRHICommandList* GetFrameCommandList() override;   // RH-2 까지 nullptr

    void* GetNativeHandle(eRHINativeHandleType t) const override;

    // ─────────────────────────────────────────────────
    // Legacy bridge — RH-2 종료까지 한시 유지
    //   ★ Codex P0-2 — RH-2 종료 시점에 본 클래스가 Private 으로 이동
    // ─────────────────────────────────────────────────
    [[deprecated("Use IRHIDevice::CreateXxx (RH-1+)")]]
    ID3D11Device*        GetD3DDevice()  const { return m_pDevice.Get(); }

    [[deprecated("Use IRHICommandList (RH-2+)")]]
    ID3D11DeviceContext* GetD3DContext() const { return m_pContext.Get(); }

    // 기존 명 유지 (backward compat — RH-2 종료까지)
    [[deprecated("Use GetD3DDevice() Legacy or IRHIDevice (RH-1+)")]]
    ID3D11Device*        GetDevice()  const { return m_pDevice.Get(); }

    [[deprecated("Use GetD3DContext() Legacy or IRHICommandList (RH-2+)")]]
    ID3D11DeviceContext* GetContext() const { return m_pContext.Get(); }

    [[deprecated("Use IRHISwapChain (RH-1+)")]]
    ID3D11RenderTargetView* GetBackRTV() const { return m_pRenderTargetView.Get(); }

    [[deprecated("Use IRHISwapChain (RH-1+)")]]
    ID3D11DepthStencilView* GetDSV() const { return m_pDepthStencilView.Get(); }

private:
    CDX11Device() = default;

    bool_t Initialize(const RHIDeviceDesc& desc);
    bool_t CreateDeviceAndSwapChain(const RHIDeviceDesc& desc);
    bool_t CreateRenderTarget();
    bool_t CreateDepthStencil(u32_t width, u32_t height);

    // DX11 raw 자원
    Microsoft::WRL::ComPtr<ID3D11Device>            m_pDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>     m_pContext;
    Microsoft::WRL::ComPtr<IDXGISwapChain>          m_pSwapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  m_pRenderTargetView;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>         m_pDepthStencilBuffer;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView>  m_pDepthStencilView;

    D3D11_VIEWPORT m_Viewport   = {};
    bool_t         m_bVSync     = true;
    u32_t          m_Width      = 1280;
    u32_t          m_Height     = 720;
    u32_t          m_FrameIndex = 0;

    // RH-1 신규 — IRHISwapChain / IRHIQueue 어댑터 (CDX11Device 가 소유)
    std::unique_ptr<CDX11SwapChain> m_pRHISwapChain;
    std::unique_ptr<CDX11Queue>     m_pRHIGraphicsQueue;
    std::unique_ptr<CDX11Queue>     m_pRHIComputeQueue;
    std::unique_ptr<CDX11Queue>     m_pRHICopyQueue;

    // ★ Codex P0-5 — handle ↔ IRHI* lookup table (Engine-owned)
    std::unique_ptr<CRHIResourceTable<IRHIBuffer>>  m_pBufferTable;
    std::unique_ptr<CRHIResourceTable<IRHITexture>> m_pTextureTable;
    std::unique_ptr<CRHIResourceTable<IRHIShader>>  m_pShaderTable;
    std::unique_ptr<CRHIResourceTable<IRHISampler>> m_pSamplerTable;
};
```

---

## 14. `Engine/Private/RHI/DX11/CDX11Device.cpp` 수정 (★ Codex P0-6 — 신규 cpp X, 기존에 합침, 핵심부 박제)

★ **Codex P0-6 보정**: 별도 `DX11DeviceAdapter.cpp` 만들지 않음. 기존 `CDX11Device.cpp` 에 IRHIDevice impl 합침.

### 14.1 includes + Create / Initialize

```cpp
#include "RHI/CDX11Device.h"

// 기존 4종 — RH-1 에서는 그대로 유지 (alias X)
#include "RHI/DX11/DX11VertexBuffer.h"
#include "RHI/DX11/DX11IndexBuffer.h"
#include "RHI/DX11/DX11ConstantBuffer.h"
#include "RHI/DX11/DX11StructuredBuffer.h"

// RH-1 신규 — IRHIBuffer impl (4종 통합)
#include "RHI/DX11/DX11Buffer.h"           // 신규
#include "RHI/DX11/DX11Texture.h"          // 신규
#include "RHI/DX11/DX11Shader.h"           // 기존 — IRHIShader impl 추가됨
#include "RHI/DX11/DX11Sampler.h"          // 신규
#include "RHI/DX11/DX11SwapChain.h"        // 신규
#include "RHI/DX11/DX11Queue.h"            // 신규

#include "RHI/RHIResourceTable.h"          // 신규 — handle lookup table

#include <iostream>

std::unique_ptr<CDX11Device> CDX11Device::Create(const RHIDeviceDesc& desc)
{
    auto pInstance = std::unique_ptr<CDX11Device>(new CDX11Device());
    if (!pInstance->Initialize(desc))
        return nullptr;
    return pInstance;
}

CDX11Device::~CDX11Device() = default;

bool_t CDX11Device::Initialize(const RHIDeviceDesc& desc)
{
    m_Width  = desc.width;
    m_Height = desc.height;
    m_bVSync = desc.bVSync;

    if (!CreateDeviceAndSwapChain(desc)) return false;
    if (!CreateRenderTarget())            return false;
    if (!CreateDepthStencil(m_Width, m_Height)) return false;

    // RH-1 신규
    m_pRHISwapChain     = CDX11SwapChain::CreateFromExisting(this);
    m_pRHIGraphicsQueue = CDX11Queue::Create(this, eRHIQueueType::Graphics);
    m_pRHIComputeQueue  = CDX11Queue::Create(this, eRHIQueueType::Compute);
    m_pRHICopyQueue     = CDX11Queue::Create(this, eRHIQueueType::Copy);

    // ★ Codex P0-5 — handle table 초기화
    m_pBufferTable  = std::unique_ptr<CRHIResourceTable<IRHIBuffer>>(new CRHIResourceTable<IRHIBuffer>());
    m_pTextureTable = std::unique_ptr<CRHIResourceTable<IRHITexture>>(new CRHIResourceTable<IRHITexture>());
    m_pShaderTable  = std::unique_ptr<CRHIResourceTable<IRHIShader>>(new CRHIResourceTable<IRHIShader>());
    m_pSamplerTable = std::unique_ptr<CRHIResourceTable<IRHISampler>>(new CRHIResourceTable<IRHISampler>());

    return true;
}
```

### 14.2 자원 생성 / Destroy / Resolve (handle API)

```cpp
RHIBufferHandle CDX11Device::CreateBuffer(const RHIBufferDesc& desc)
{
    auto pBuf = CDX11Buffer::Create(m_pDevice.Get(), m_pContext.Get(), desc);
    if (!pBuf) return kInvalidBufferHandle;
    return m_pBufferTable->Insert<RHIBufferHandle>(std::move(pBuf));
}

RHITextureHandle CDX11Device::CreateTexture(const RHITextureDesc& desc)
{
    auto pTex = CDX11Texture::Create(m_pDevice.Get(), desc);
    if (!pTex) return kInvalidTextureHandle;
    return m_pTextureTable->Insert<RHITextureHandle>(std::move(pTex));
}

RHIShaderHandle CDX11Device::CreateShader(const RHIShaderDesc& desc)
{
    auto pSh = CDX11Shader::Create(m_pDevice.Get(), desc);
    if (!pSh) return kInvalidShaderHandle;
    return m_pShaderTable->Insert<RHIShaderHandle>(std::move(pSh));
}

RHISamplerHandle CDX11Device::CreateSampler(const RHISamplerDesc& desc)
{
    auto pSm = CDX11Sampler::Create(m_pDevice.Get(), desc);
    if (!pSm) return kInvalidSamplerHandle;
    return m_pSamplerTable->Insert<RHISamplerHandle>(std::move(pSm));
}

void CDX11Device::DestroyBuffer (RHIBufferHandle  h) { m_pBufferTable->Remove(h); }
void CDX11Device::DestroyTexture(RHITextureHandle h) { m_pTextureTable->Remove(h); }
void CDX11Device::DestroyShader (RHIShaderHandle  h) { m_pShaderTable->Remove(h); }
void CDX11Device::DestroySampler(RHISamplerHandle h) { m_pSamplerTable->Remove(h); }

IRHIBuffer*  CDX11Device::ResolveBuffer (RHIBufferHandle  h) { return m_pBufferTable->Resolve(h); }
IRHITexture* CDX11Device::ResolveTexture(RHITextureHandle h) { return m_pTextureTable->Resolve(h); }
IRHIShader*  CDX11Device::ResolveShader (RHIShaderHandle  h) { return m_pShaderTable->Resolve(h); }
IRHISampler* CDX11Device::ResolveSampler(RHISamplerHandle h) { return m_pSamplerTable->Resolve(h); }
```

### 14.3 SwapChain / Queue / Frame / Native handle

```cpp
IRHISwapChain* CDX11Device::GetSwapChain()     { return m_pRHISwapChain.get(); }
IRHIQueue*     CDX11Device::GetGraphicsQueue() { return m_pRHIGraphicsQueue.get(); }
IRHIQueue*     CDX11Device::GetComputeQueue()  { return m_pRHIComputeQueue.get(); }
IRHIQueue*     CDX11Device::GetCopyQueue()     { return m_pRHICopyQueue.get(); }

void CDX11Device::BeginFrame()
{
    f32_t clear[4] = { 0.f, 1.f, 1.f, 1.f };
    m_pContext->ClearRenderTargetView(m_pRenderTargetView.Get(), clear);
    m_pContext->ClearDepthStencilView(m_pDepthStencilView.Get(),
        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.f, 0);

    ID3D11RenderTargetView* rtvs[] = { m_pRenderTargetView.Get() };
    m_pContext->OMSetRenderTargets(1, rtvs, m_pDepthStencilView.Get());
    m_pContext->RSSetViewports(1, &m_Viewport);

    ++m_FrameIndex;
}

void CDX11Device::EndFrame()
{
    m_pSwapChain->Present(m_bVSync ? 1u : 0u, 0u);
}

IRHICommandList* CDX11Device::GetFrameCommandList()
{
    return nullptr;   // RH-2 까지
}

void* CDX11Device::GetNativeHandle(eRHINativeHandleType t) const
{
    switch (t)
    {
        case eRHINativeHandleType::DX11_Device:           return m_pDevice.Get();
        case eRHINativeHandleType::DX11_DeviceContext:    return m_pContext.Get();
        case eRHINativeHandleType::DX11_SwapChain:        return m_pSwapChain.Get();
        case eRHINativeHandleType::DX11_BackBufferRTV:    return m_pRenderTargetView.Get();
        case eRHINativeHandleType::DX11_DepthStencilView: return m_pDepthStencilView.Get();
        default: return nullptr;
    }
}

// CreateDeviceAndSwapChain / CreateRenderTarget / CreateDepthStencil 은 기존 구현 유지
// (DeviceDesc → RHIDeviceDesc 매개변수 + bool → bool_t)
```

---

## 15. `Engine/Private/RHI/DX11/DX11Buffer.h` (★ 신규 — Codex P0-7 병행, 전문)

★ **Codex P0-7 보정**: 기존 `CDX11VertexBuffer / IndexBuffer / ConstantBuffer / StructuredBuffer` 4종 그대로 유지. 신규 `CDX11Buffer` 는 IRHIBuffer 구현용으로 **병행** 추가. RH-2 caller 마이그 후 alias 검토.

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "RHI/IRHIBuffer.h"
#include "RHI/RHIDescriptors.h"

#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <memory>

// ─────────────────────────────────────────────────────────────────
//  CDX11Buffer  |  IRHIBuffer 구현 (4종 통합)
//
//  ★ Codex P0-7 보정 — 기존 CDX11VertexBuffer / IndexBuffer / ConstantBuffer /
//    StructuredBuffer 4종은 RH-1 에서 그대로 유지. 본 클래스는 신규 RHI 경로용.
//    RH-2 caller 마이그 완료 후 4종 deprecated 처리 검토.
// ─────────────────────────────────────────────────────────────────

class CDX11Buffer final : public IRHIBuffer
{
public:
    ~CDX11Buffer() override;

    CDX11Buffer(const CDX11Buffer&)            = delete;
    CDX11Buffer& operator=(const CDX11Buffer&) = delete;

    static std::unique_ptr<CDX11Buffer> Create(
        ID3D11Device* pDevice,
        ID3D11DeviceContext* pContext,
        const RHIBufferDesc& desc);

    // IRHIBuffer impl
    eRHIBufferType   GetType()        const override { return m_Type; }
    eRHIBufferUsage  GetUsage()       const override { return m_Usage; }
    u32_t            GetSizeBytes()   const override { return m_SizeBytes; }
    u32_t            GetStrideBytes() const override { return m_StrideBytes; }

    void* Map()   override;
    void  Unmap() override;

    eRHIResourceState GetCurrentState() const override { return m_State; }
    void              SetCurrentState(eRHIResourceState s) override { m_State = s; }

    // ★ Codex P1-12 — borrowed 정책
    void* GetNativeHandle(eRHINativeHandleType t) const override;

    // Legacy access (RH-2 까지)
    [[deprecated("Use IRHICommandList::SetVertexBuffer (RH-2+)")]]
    ID3D11Buffer* GetD3DBuffer() const { return m_pBuffer.Get(); }

private:
    CDX11Buffer() = default;

    bool_t Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext,
                      const RHIBufferDesc& desc);

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_pBuffer;
    eRHIBufferType    m_Type        = eRHIBufferType::Vertex;
    eRHIBufferUsage   m_Usage       = eRHIBufferUsage::Immutable;
    u32_t             m_SizeBytes   = 0;
    u32_t             m_StrideBytes = 0;
    eRHIResourceState m_State       = eRHIResourceState::Common;

    ID3D11DeviceContext* m_pContext = nullptr;   // borrowed (Map/Unmap)
};
```

---

## 16. `Engine/Public/RHI/RHIResourceTable.h` (★ 신규 — Codex P0-5 + P1-17, 전문)

★ Phase RH-4 의 사전 도입. RH-1 부터 사용. **Thread-safety 정책 명시 (P1-17)**.

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "RHI/RHIHandles.h"

#include <memory>
#include <vector>

// ─────────────────────────────────────────────────────────────────
//  CRHIResourceTable<T>  |  Handle ↔ unique_ptr<T> lookup table
//
//  ★ Codex P0-5 보정 — Engine-owned. unique_ptr deleter 가 Engine CRT 에서 실행.
//  ★ Codex P1-16 보정 — 64-bit handle (32 index + 32 generation).
//  ★ Codex P1-17 보정 — Thread-safety: render thread only.
//                       다른 thread 호출 시 debug assert.
//
//  RH-4 에서 thread-safety 강화 (mutex / job handoff) 검토.
// ─────────────────────────────────────────────────────────────────

template<typename T>
class CRHIResourceTable
{
public:
    CRHIResourceTable() = default;
    ~CRHIResourceTable() = default;

    CRHIResourceTable(const CRHIResourceTable&)            = delete;
    CRHIResourceTable& operator=(const CRHIResourceTable&) = delete;

    template<typename Handle>
    Handle Insert(std::unique_ptr<T> ptr)
    {
        AssertRenderThread();   // ★ Codex P1-17

        u32_t index;
        if (!m_FreeList.empty())
        {
            index = m_FreeList.back();
            m_FreeList.pop_back();
            m_Entries[index].pPtr = std::move(ptr);
            m_Entries[index].generation += 1;
            m_Entries[index].bAlive = true;
        }
        else
        {
            index = static_cast<u32_t>(m_Entries.size());
            m_Entries.push_back({ std::move(ptr), 1u, true });
        }
        return Handle::Make(index, m_Entries[index].generation);
    }

    template<typename Handle>
    T* Resolve(Handle h)
    {
        AssertRenderThread();

        u32_t i = h.Index();
        if (i >= m_Entries.size()) return nullptr;
        if (!m_Entries[i].bAlive)  return nullptr;
        if (m_Entries[i].generation != h.Generation()) return nullptr;   // use-after-free
        return m_Entries[i].pPtr.get();
    }

    template<typename Handle>
    void Remove(Handle h)
    {
        AssertRenderThread();

        u32_t i = h.Index();
        if (i >= m_Entries.size()) return;
        if (m_Entries[i].generation != h.Generation()) return;
        m_Entries[i].pPtr.reset();
        m_Entries[i].bAlive = false;
        m_FreeList.push_back(i);
    }

private:
    struct Entry
    {
        std::unique_ptr<T> pPtr;
        u32_t              generation = 1;
        bool_t             bAlive     = false;
    };

    void AssertRenderThread() const
    {
        // RH-4 에서 thread id 검증 추가. 현재는 placeholder.
    }

    std::vector<Entry> m_Entries;
    std::vector<u32_t> m_FreeList;
};
```

---

## 17. `Engine/Include/GameInstance.h` — Tier 2 신규 이름 추가 (★ Codex P0-4)

★ **Codex P0-4 보정**: `Get_RHIDevice()` 이름 재사용 X. RH-1 = `Get_NewRHIDevice()`. RH-2 종료 시점에 정식 rename.

```cpp
// ─────────────────────────────────────────────────────────────
// RHI Tier 2 Getters — RH-1 신규 (Legacy bridge 와 양립)
//
// ★ Codex P0-4 보정 — 이름 충돌 방지 위해 _NewRHIDevice 사용.
//   RH-2 종료 시 모든 caller _Legacy 마이그 후 _NewRHIDevice → _RHIDevice 로 rename.
// ─────────────────────────────────────────────────────────────
class IRHIDevice;
class IRHICommandList;   // RH-2

IRHIDevice*       Get_NewRHIDevice();         // ★ RH-1 신규
IRHICommandList*  Get_FrameCommandList();     // RH-2 까지 nullptr

// (RH-0 의 _Legacy 8개 그대로 유지 — RH-2 종료까지)
```

**`GameInstance.cpp` 추가**:

```cpp
IRHIDevice* CGameInstance::Get_NewRHIDevice()
{
    return m_pDX11Device.get();   // CDX11Device : public IRHIDevice
}

IRHICommandList* CGameInstance::Get_FrameCommandList()
{
    if (!m_pDX11Device) return nullptr;
    return m_pDX11Device->GetFrameCommandList();   // RH-2 까지 nullptr
}
```

---

## 18. 합격 게이트 (RH-1 전체, ★ 2차 보정판)

### 18.1 인터페이스 박제 합격
- ✅ `Engine/Public/RHI/` 에 9개 인터페이스 + 3개 supporting (RHITypes / RHIDescriptors / RHIWindowHandle / RHIHandles / RHIResourceTable) + IBuffer.h shim 유지
- ✅ 각 인터페이스 가 다음 패턴 준수: `virtual ~Xxx() = default; / 복사 금지 / protected ctor / GetNativeHandle escape`
- ✅ **모든 backend 구현 클래스 `final`** (Codex P1-11)
- ✅ **모든 신규 코드 `bool_t`** (raw bool 0건, Codex P1-10)

### 18.2 Handle API 합격
- ✅ `IRHIDevice::CreateBuffer(desc) -> RHIBufferHandle` 반환 (★ Codex P0-5)
- ✅ `Destroy* / Resolve*` 4종 동작
- ✅ Use-after-free generation check 동작 (test 케이스)
- ✅ `CRHIResourceTable<T>` thread-safety 정책 명시 (★ Codex P1-17)

### 18.3 어댑터 합격
- ✅ `CDX11Device : public IRHIDevice final` — `CDX11Device.cpp` 단일 파일에 합침 (★ Codex P0-6)
- ✅ `CDX11Buffer : public IRHIBuffer final` (4종 통합 신규)
- ✅ **기존 `CDX11VertexBuffer / IndexBuffer / ConstantBuffer / StructuredBuffer` 4종 그대로 유지** (★ Codex P0-7 — alias X)
- ✅ `CDX11Texture / CDX11Shader / CDX11Sampler / CDX11SwapChain / CDX11Queue` 박제

### 18.4 GameInstance 합격
- ✅ **`Get_NewRHIDevice() -> IRHIDevice*`** 정식 가동 (★ Codex P0-4 — `_RHIDevice` 와 양립)
- ✅ `Get_DX11Device_Legacy()` 와 공존 (RH-0 에서 8개 deprecated)
- ✅ `Get_FrameCommandList()` 시그니처 추가 (RH-2 까지 nullptr)

### 18.5 빌드 합격
- ✅ Engine.dll 빌드 통과
- ✅ Client.exe 빌드 통과
- ✅ EngineSDK 동기화 통과 (인터페이스 헤더 13개 SDK 에 복사)
- ✅ deprecated warning 만 발생, error 0건
- ✅ **Code block 줄바꿈 검증** — 본 plan 의 모든 cpp 블록을 IDE 에 복붙 시 컴파일 OK (★ Codex P0-8)

### 18.6 런타임 합격
- ✅ LoL 진입 → 무회귀 (어댑터 wrapping 만 추가)
- ✅ `device->CreateBuffer(...)` 호출 → 정상 RHIBufferHandle 반환
- ✅ `Get_NewRHIDevice()->GetNativeHandle(eRHINativeHandleType::DX11_DeviceContext)` = 기존 `GetContext()` 와 동일 포인터

---

## 19. 위험 / 디버깅 메모

| 위험 | 완화 |
|---|---|
| `IBuffer.h` shim alias 가 `[[deprecated]]` 와 함께 없으면 silent breakage | shim 에 `using IBuffer [[deprecated]] = IRHIBuffer;` 명시 (위 §6.1) |
| `eRHIFormat` 가 DXGI_FORMAT 와 1:1 매핑 안 됨 | DX11/12/VK 공통 부분집합만. 빠진 포맷은 추후 추가 |
| `GetNativeHandle()` wrong type 호출 시 nullptr — caller 가 null check 잊음 | debug build assert + log (백엔드별) |
| `D3D11_USAGE_DEFAULT` vs `eRHIBufferUsage::Default` 매핑 검증 | DX11Buffer.cpp 에 `MapEnum_RHIBufferUsage_To_D3D11()` 함수 명시 |
| ImGui DX11 backend 가 `GetNativeHandle(DX11_Device)` 로 받기 변경 시 기존 caller 다 수정 | ImGuiLayer.cpp 단 1곳만 수정 (현재 구조 확인 필요) |
| `RHIWindowHandle` union 사용 시 plat 별 init 누락 → 런타임 fallback 불가 | union ctor 에서 platform 별 zero-init 강제 |
| 64-bit handle 전환 시 기존 raw pointer 사용처와 호환 안 됨 | RH-1 단계는 handle / IRHI* 양쪽 API 공존 (Resolve 통해) |
| 기존 4종 buffer 클래스 + 신규 CDX11Buffer 병행 시 caller 혼란 | 코멘트로 명시 — "신규 RHI 경로는 CDX11Buffer, 레거시는 4종" |

---

## 20. 한 줄 (★ 2차 보정판)

**RH-1 = 9개 인터페이스 박제 + handle API (Engine-owned, DLL CRT 충돌 방지) + `Get_NewRHIDevice()` 신규 이름 + DX11Buffer 4종 병행 + IBuffer.h shim + RHIWindowHandle wrapper + bool_t / final / GetNativeHandle borrowed 정책 + CRHIResourceTable thread-safety 명시. CDX11Device.cpp 단일 파일에 합침 (별도 어댑터 cpp X). 2주. 합격 = `device->CreateBuffer(desc) -> RHIBufferHandle` 동작 + LoL 무회귀 + Codex 30건 중 P0/P1 보정 11건 모두 반영.**
