# Phase RH-5 Sub-plan: DX12 Backend

> 2026-05-25 업데이트: DX12는 별도 `DX12.exe`/`DX12.vcxproj`가 아니라 RHI backend다.
> 세션 실행 계획은 `.md/plan/rhi/sessions/S09_DX12_BACKEND_NO_EXE.md`를 따른다.
> 기존 `Debug-DX12`/`Release-DX12` 표현은 standalone project가 아니라 같은 Winters 실행 파일에서 DX12 backend define/config를 켜는 의미로 해석한다.

**작성일**: 2026-04-30 (Codex 2차 검토 보정 2026-04-30)
**상위 문서**: `00_RHI_MIGRATION_MASTER.md` §2
**범위**: `Engine/Private/RHI/DX12/*` 전체 박제 + Winters.sln 에 Debug-DX12 / Release-DX12 컨피그 추가 + **D3D12MA 외부 라이브러리 편입**
**합격**: **compile-only 합격 (3~4주) + visual parity 합격 (2~3주 추가)** — 동일 LoL 빌드가 DX12 컨피그에서 시각 결과 동일

**한 줄**: **★ Codex 2차 보정 — compile-only / visual parity 분리 + 모든 HRESULT 체크 + handle API 일관 (RH-4 이후) + D3D12MA 외부 라이브러리 편입 계획 명시.**

---

## ★ Codex 2차 검토 변경 요약

| 변경 | 이전 RH-5 (1차) | 신규 RH-5 (2차) |
|---|---|---|
| 합격 단계 | 단일 (시각 결과 동일) | **compile-only (3~4주) + visual parity (2~3주)** (P2-21) |
| HRESULT 체크 | 누락 | **모든 D3D12 호출에 HRESULT 체크 + log** (P1-18) |
| Buffer barrier API | `IRHIBuffer*` 직접 | **`RHIBufferHandle` 기반** (P1-19) |
| D3D12MA | 단순 mention | **ThirdPartyLib 편입 계획 (license / lib / vcxproj) 명시** (P1-20) |

---

## 1. 신규 파일 목록

```
Engine/Private/RHI/DX12/
├── DX12Device.h / .cpp           (CDX12Device : public IRHIDevice)
├── DX12SwapChain.h / .cpp        (CDX12SwapChain : public IRHISwapChain)
├── DX12Queue.h / .cpp            (CDX12Queue : public IRHIQueue)
├── DX12CommandList.h / .cpp      (CDX12CommandList : public IRHICommandList)
├── DX12CommandPool.h / .cpp      (CDX12CommandPool — ID3D12CommandAllocator wrap)
├── DX12Buffer.h / .cpp           (CDX12Buffer : public IRHIBuffer)
├── DX12Texture.h / .cpp          (CDX12Texture : public IRHITexture)
├── DX12Shader.h / .cpp           (CDX12Shader : public IRHIShader, DXC 사용)
├── DX12Sampler.h / .cpp          (CDX12Sampler : public IRHISampler)
├── DX12PipelineState.h / .cpp    (CDX12PipelineState : public IRHIPipelineState)
├── DX12RootSignature.h / .cpp    (★ DX12 특유 — BindGroupLayout 으로 wrap)
├── DX12DescriptorHeap.h / .cpp   (★ DX12 특유 — BindGroup 의 backing storage)
├── DX12Fence.h / .cpp
├── DX12MemoryAllocator.h / .cpp  (D3D12MA wrapper)
└── DX12PipelineCache.h / .cpp    (★ ID3D12PipelineLibrary — 디스크 PSO 캐시)
```

총 14쌍 = ~28 파일.

---

## 2. 핵심 구현 시그니처

### 2.1 CDX12Device::Create (★ Codex P1-18 — HRESULT 체크 전수)

```cpp
std::unique_ptr<CDX12Device> CDX12Device::Create(const RHIDeviceDesc& desc)
{
    auto pInstance = std::unique_ptr<CDX12Device>(new CDX12Device());

    HRESULT hr = S_OK;

    // 1. Debug layer
    if (desc.bDebug)
    {
        ComPtr<ID3D12Debug> pDebug;
        hr = D3D12GetDebugInterface(IID_PPV_ARGS(&pDebug));
        if (SUCCEEDED(hr))
            pDebug->EnableDebugLayer();
        else
            OutputDebugStringA("[CDX12Device] Debug layer unavailable (continuing)\n");
    }

    // 2. DXGI Factory
    ComPtr<IDXGIFactory6> pFactory;
    hr = CreateDXGIFactory2(desc.bDebug ? DXGI_CREATE_FACTORY_DEBUG : 0,
                             IID_PPV_ARGS(&pFactory));
    if (FAILED(hr))
    {
        OutputDebugStringA("[CDX12Device] CreateDXGIFactory2 failed\n");
        return nullptr;
    }

    // 3. Adapter
    ComPtr<IDXGIAdapter1> pAdapter;
    hr = pFactory->EnumAdapterByGpuPreference(
        0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
        IID_PPV_ARGS(&pAdapter));
    if (FAILED(hr))
    {
        OutputDebugStringA("[CDX12Device] No DX12-capable adapter found\n");
        return nullptr;
    }

    // 4. ID3D12Device
    hr = D3D12CreateDevice(pAdapter.Get(), D3D_FEATURE_LEVEL_12_0,
        IID_PPV_ARGS(&pInstance->m_pDevice));
    if (FAILED(hr))
    {
        char msg[128];
        sprintf_s(msg, "[CDX12Device] D3D12CreateDevice failed (hr=0x%08X)\n", hr);
        OutputDebugStringA(msg);
        return nullptr;
    }

    // 5. Graphics Queue
    pInstance->m_pGraphicsQueue = CDX12Queue::Create(
        pInstance->m_pDevice.Get(), eRHIQueueType::Graphics);
    if (!pInstance->m_pGraphicsQueue)
    {
        OutputDebugStringA("[CDX12Device] Graphics queue creation failed\n");
        return nullptr;
    }

    // 6. Memory allocator (D3D12MA)
    pInstance->m_pAllocator = CDX12MemoryAllocator::Create(
        pInstance->m_pDevice.Get(), pAdapter.Get());
    if (!pInstance->m_pAllocator)
    {
        OutputDebugStringA("[CDX12Device] D3D12MA initialization failed\n");
        return nullptr;
    }

    // 7. Descriptor heaps
    if (!pInstance->InitDescriptorHeaps())
    {
        OutputDebugStringA("[CDX12Device] Descriptor heap init failed\n");
        return nullptr;
    }

    // 8. SwapChain
    pInstance->m_pSwapChain = CDX12SwapChain::Create(
        pInstance.get(), pFactory.Get(),
        pInstance->m_pGraphicsQueue->GetD3D12Queue(),
        desc);
    if (!pInstance->m_pSwapChain)
    {
        OutputDebugStringA("[CDX12Device] SwapChain creation failed\n");
        return nullptr;
    }

    // 9. PSO cache
    pInstance->m_pPSOCache = CDX12PipelineCache::Create(pInstance->m_pDevice.Get());
    pInstance->m_pPSOCache->LoadFromDisk("Cache/PSO.bin");   // 실패 시 빈 cache

    return pInstance;
}
```

### 2.2 CDX12CommandList — 진짜 deferred 모델

```cpp
void CDX12CommandList::Begin()
{
    m_pAllocator->Reset();
    m_pCmdList->Reset(m_pAllocator, nullptr);
}

void CDX12CommandList::End()
{
    m_pCmdList->Close();
}

void CDX12CommandList::SetVertexBuffer(u32_t slot, RHIBufferHandle vb, u32_t offset)
{
    if (!vb.IsValid()) return;
    auto* pIBuf = m_pDevice->ResolveBuffer(vb);
    if (!pIBuf) return;
    auto* pBuf = static_cast<CDX12Buffer*>(pIBuf);

    D3D12_VERTEX_BUFFER_VIEW view{};
    view.BufferLocation = pBuf->GetGPUAddress() + offset;
    view.SizeInBytes    = pBuf->GetSizeBytes() - offset;
    view.StrideInBytes  = pBuf->GetStrideBytes();

    m_pCmdList->IASetVertexBuffers(slot, 1, &view);
}

// ★ Codex P1-19 보정 — RHIBufferHandle 기반 (RH-4 handle API 일관)
void CDX12CommandList::ResourceBarrier(RHIBufferHandle h,
    eRHIResourceState before, eRHIResourceState after)
{
    if (!h.IsValid()) return;
    auto* pIBuf = m_pDevice->ResolveBuffer(h);
    if (!pIBuf) return;
    auto* pBuf12 = static_cast<CDX12Buffer*>(pIBuf);

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource   = pBuf12->GetD3DResource();
    barrier.Transition.StateBefore = ConvertState(before);
    barrier.Transition.StateAfter  = ConvertState(after);
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    m_pCmdList->ResourceBarrier(1, &barrier);
    pIBuf->SetCurrentState(after);
}
```

### 2.3 CDX12Queue::Submit

```cpp
void CDX12Queue::Submit(IRHICommandList* const* pCmdLists, u32_t count)
{
    std::vector<ID3D12CommandList*> rawLists(count);
    for (u32_t i = 0; i < count; ++i)
        rawLists[i] = static_cast<CDX12CommandList*>(pCmdLists[i])->GetD3D12List();

    m_pQueue->ExecuteCommandLists(count, rawLists.data());

    // Fence signal
    ++m_FenceValue;
    m_pQueue->Signal(m_pFence.Get(), m_FenceValue);
}
```

---

## 3. Winters.sln 컨피그 추가

```xml
<!-- Engine.vcxproj -->
<ProjectConfiguration Include="Debug-DX12|x64">
    <Configuration>Debug-DX12</Configuration>
    <Platform>x64</Platform>
</ProjectConfiguration>
<ProjectConfiguration Include="Release-DX12|x64">
    <Configuration>Release-DX12</Configuration>
    <Platform>x64</Platform>
</ProjectConfiguration>
```

각 컨피그의 `<PreprocessorDefinitions>` 에 `WINTERS_RHI_DX12` 추가.

`CGameInstance::Initialize` 에서 RHI 백엔드 선택:
```cpp
#if defined(WINTERS_RHI_DX12)
    m_pRHIDevice = CDX12Device::Create(deviceDesc);
#elif defined(WINTERS_RHI_VULKAN)
    m_pRHIDevice = CVulkanDevice::Create(deviceDesc);
#else
    m_pRHIDevice = CDX11Device::Create(deviceDesc);   // default
#endif
```

---

## 4. PSO 캐시

DX12 의 PSO 컴파일 = 수백 ms. 첫 frame 100개 PSO = 10초 stutter.

```cpp
class CDX12PipelineCache
{
public:
    void LoadFromDisk(const char* path);   // ID3D12PipelineLibrary::Deserialize
    void SaveToDisk(const char* path);     // ID3D12PipelineLibrary::Serialize

    ID3D12PipelineState* TryLoad(u64_t hash, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc);
    void                 Store  (u64_t hash, ID3D12PipelineState* pso);
};
```

PSO hash = (셰이더 hash) ^ (state desc hash). 다음 실행 시 컴파일 0회.

---

## 5. D3D12MA 외부 라이브러리 편입 (★ Codex P1-20)

### 5.1 D3D12MemoryAllocator 정보
- **GitHub**: https://github.com/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator
- **License**: MIT
- **버전**: 2.0.1 (2024 기준 latest)
- **언어**: C++17
- **단일 헤더**: `D3D12MemAlloc.h` + `D3D12MemAlloc.cpp` (총 2 파일)

### 5.2 ThirdPartyLib 편입 절차

```
Engine/ThirdPartyLib/
└── D3D12MA/                       (★ 신규)
    ├── README.md                  (license + version note)
    ├── Inc/
    │   └── D3D12MemAlloc.h
    └── Src/
        └── D3D12MemAlloc.cpp
```

**Engine.vcxproj 추가 ItemGroup**:
```xml
<ItemGroup Label="ThirdParty: D3D12MA">
  <ClInclude Include="..\ThirdPartyLib\D3D12MA\Inc\D3D12MemAlloc.h" />
  <ClCompile Include="..\ThirdPartyLib\D3D12MA\Src\D3D12MemAlloc.cpp">
    <!-- D3D12MA 자체는 warning 가능성 높음 — Level3 강제 -->
    <WarningLevel>Level3</WarningLevel>
  </ClCompile>
</ItemGroup>

<ItemDefinitionGroup>
  <ClCompile>
    <AdditionalIncludeDirectories>
      $(ProjectDir)..\ThirdPartyLib\D3D12MA\Inc;
      %(AdditionalIncludeDirectories)
    </AdditionalIncludeDirectories>
  </ClCompile>
</ItemDefinitionGroup>
```

### 5.3 합격
- ✅ ThirdPartyLib/D3D12MA 폴더 + 2 파일 배치
- ✅ Engine.vcxproj 에 편입 + WINTERS_RHI_DX12 컨피그에서만 빌드
- ✅ License 명시 (MIT)

---

## 6. 합격 (★ Codex 2차 보정 — compile-only / visual parity 분리)

### 6.1 Compile-only 합격 (3~4주, mechanical translation)
- ✅ Winters.sln 에 Debug-DX12 / Release-DX12 컨피그
- ✅ 14쌍 DX12 백엔드 파일 박제
- ✅ D3D12MA 편입 완료
- ✅ DX12 컨피그 빌드 통과 (warning OK, error 0)
- ✅ DX12 빌드 시 LoL 실행 → crash 없이 첫 frame 도달

### 6.2 Visual parity 합격 (2~3주 추가, sync/barrier 디버깅)
- ✅ DX12 빌드 → LoL 시각 결과 동일 (frame diff < 1px)
- ✅ DX12 debug layer error 0건 + warning 1차 정리
- ✅ PSO 캐시 디스크 저장/로드 동작 (2번째 실행 시 PSO 컴파일 시간 0)
- ✅ Frame in flight 2 buffer 안정 동작
- ✅ Resource barrier 전수 검증 (debug layer 의 transition assertion 통과)

---

## 7. 추후 박제

RH-4 합격 + ER 진입 시점 직전. 현재는 outline + Codex 2차 보정 반영.
