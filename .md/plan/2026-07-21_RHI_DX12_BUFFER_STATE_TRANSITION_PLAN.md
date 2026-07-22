Session - 공통 RHI buffer transition 계약을 DX11 semantic no-op과 DX12 실제 ResourceBarrier로 닫는다.
좌표: 신규 좌표 후보 · 축: C4 수명은 선언된다, C8 검증이 병목
관련: 2026-07-21_RHI_CAPABILITY_TRUTH_CONTRACT_PLAN.md / 2026-07-21_RHI_CAPABILITY_TRUTH_CONTRACT_RESULT.md

## 1. 결정 기록

① 문제·제약: `IRHICommandList`에 buffer/texture transition 2개가 있지만 DX11·DX12 구현 4개가 모두 빈 몸통이고 실제 호출자는 0곳이다. DX12 backbuffer만 `BeginFrame/EndFrame`에서 별도 barrier 2개를 직접 발행한다.
② 순진한 해법의 실패: 두 overload를 한 번에 “지원”으로 바꾸면 DX12 texture creation이 usage flags를 아직 resource flags/RTV/DSV/UAV로 완성하지 않은 상태를 숨기며 Step 2A truth contract를 다시 깨뜨린다.
③ 메커니즘: 이번 조각은 buffer만 닫는다. 상태는 기존 `CDX12Buffer::state`가 소유하고 열린 frame 안에서 검증→변환→중복 제거→`ResourceBarrier`→상태 갱신하며, probe는 `WaitIdle` 뒤 실제 barrier 수와 InfoQueue를 읽는다.
④ 대조: DX11은 동일한 논리 buffer-state 목록을 검증하되 native barrier 수 0인 semantic no-op이다. DX12 upload heap은 `GENERIC_READ` 고정 제약 때문에 호환 read-only 요청만 성공 no-op으로 흡수한다.
⑤ 대가: texture transition은 명시적 실패로 남고 `supportsResourceTransitions`도 false를 유지한다. buffer 계약만 통과했으므로 전체 resource transition 또는 DX12 제품 parity를 주장하지 않는다.

## 2. 반영해야 하는 코드

### 2-1. C:/Users/user/Desktop/Winters/Engine/Public/RHI/IRHICommandList.h

`IRHICommandList` class 선언 바로 위에 진단 구조체를 추가한다. 이 값은 제품 렌더링 결과가 아니라 conformance gate가 실제 native barrier 발행 수와 debug validation 오류를 확인하는 관측면이다.

기존 코드:

```cpp
class WINTERS_ENGINE IRHICommandList
```

바로 위에 추가:

```cpp
struct WINTERS_ENGINE RHICommandListDiagnostics
{
    u64_t emittedResourceBarrierCount = 0;
    u32_t validationErrorCount = 0;
    bool_t validationAvailable = false;
};

```

`IRHICommandList`의 Begin/End 선언을 아래로 교체한다.

기존 코드:

```cpp
    virtual void Begin() = 0;
    virtual void End() = 0;
```

아래로 교체:

```cpp
    virtual void Begin() = 0;
    virtual void End() = 0;
    virtual RHICommandListDiagnostics GetDiagnostics() const = 0;
```

기존 transition 선언을 아래로 교체한다. 반환값은 command recording이 해당 handle/state 조합을 실제로 수용했는지를 뜻한다.

기존 코드:

```cpp
    virtual void TransitionResource(RHIBufferHandle handle, eRHIResourceState newState) = 0;
    virtual void TransitionResource(RHITextureHandle handle, eRHIResourceState newState) = 0;
```

아래로 교체:

```cpp
    virtual bool_t TransitionResource(RHIBufferHandle handle, eRHIResourceState newState) = 0;
    virtual bool_t TransitionResource(RHITextureHandle handle, eRHIResourceState newState) = 0;
```

### 2-2. C:/Users/user/Desktop/Winters/Engine/Public/RHI/IRHIDevice.h

frame lifecycle 계약에 probe 전용으로도 사용할 수 있는 device idle 대기를 추가한다. 제품 프레임마다 호출하는 API가 아니라 명시적 동기화·shutdown·conformance 지점의 계약이며 실제 완료 확인 실패는 false다.

기존 코드:

```cpp
    virtual void BeginFrame(f32_t r = 0.0f, f32_t g = 0.0f, f32_t b = 0.0f, f32_t a = 1.0f) = 0;
    virtual void EndFrame() = 0;
```

아래로 교체:

```cpp
    virtual void BeginFrame(f32_t r = 0.0f, f32_t g = 0.0f, f32_t b = 0.0f, f32_t a = 1.0f) = 0;
    virtual void EndFrame() = 0;
    virtual bool_t WaitIdle() = 0;
```

### 2-3. C:/Users/user/Desktop/Winters/Engine/Private/RHI/DX11/CDX11Device.cpp

anonymous namespace의 `BytesPerPixelOf` 함수 바로 위에 논리 buffer-state 검증 함수를 추가한다.

기존 코드:

```cpp
    u32_t BytesPerPixelOf(eRHIFormat format)
```

바로 위에 추가:

```cpp
    bool_t IsSupportedRHIBufferTransitionState(eRHIResourceState state)
    {
        switch (state)
        {
        case eRHIResourceState::Common:
        case eRHIResourceState::VertexConstant:
        case eRHIResourceState::IndexBuffer:
        case eRHIResourceState::ShaderResource:
        case eRHIResourceState::CopyDest:
        case eRHIResourceState::CopySource:
            return true;
        default:
            return false;
        }
    }

```

`CDX11FrameCommandList`의 Begin/End를 아래로 교체한다. DX11은 native barrier를 발행하지 않으므로 진단 count는 항상 0이다.

기존 코드:

```cpp
    void Begin() override {}
    void End() override {}
```

아래로 교체:

```cpp
    void Begin() override { m_Diagnostics = {}; }
    void End() override {}

    RHICommandListDiagnostics GetDiagnostics() const override
    {
        return m_Diagnostics;
    }
```

빈 transition 2개를 아래로 교체한다. DX11은 native barrier를 발행하지 않지만 DX12와 같은 논리 상태 목록 및 handle을 검증한다. texture는 이번 slice에서 양쪽 backend 모두 명시적으로 실패한다.

기존 코드:

```cpp
    void TransitionResource(RHIBufferHandle, eRHIResourceState) override {}
    void TransitionResource(RHITextureHandle, eRHIResourceState) override {}
```

아래로 교체:

```cpp
    bool_t TransitionResource(
        RHIBufferHandle handle,
        eRHIResourceState newState) override
    {
        return m_Owner.m_bFrameRecording &&
            IsSupportedRHIBufferTransitionState(newState) &&
            m_Owner.m_pTables &&
            m_Owner.m_pTables->bufferTable.Lookup(handle) != nullptr;
    }

    bool_t TransitionResource(
        RHITextureHandle,
        eRHIResourceState) override
    {
        OutputDebugStringA(
            "[CDX11FrameCommandList] texture transition rejected: not implemented\n");
        return false;
    }
```

`CDX11FrameCommandList` private 영역의 기존 owner 바로 아래에 진단값을 추가한다.

기존 코드:

```cpp
    CDX11Device& m_Owner;
    CDX11PipelineState* m_pCurrentPipeline = nullptr;
```

아래로 교체:

```cpp
    CDX11Device& m_Owner;
    CDX11PipelineState* m_pCurrentPipeline = nullptr;
    RHICommandListDiagnostics m_Diagnostics{};
```

`CDX11Device::BeginFrame` 마지막 clear block을 아래로 교체해 열린 프레임을 기록한다.

기존 코드:

```cpp
    m_pContext->ClearDepthStencilView(
        m_pDepthStencilView.Get(),
        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
        1.f, 0
    );
}
```

아래로 교체:

```cpp
    m_pContext->ClearDepthStencilView(
        m_pDepthStencilView.Get(),
        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
        1.f, 0
    );
    m_bFrameRecording = true;
}
```

`CDX11Device::EndFrame` 시작에 frame close를 추가한다.

기존 코드:

```cpp
void CDX11Device::EndFrame()
{
#ifdef WINTERS_PROFILING
```

아래로 교체:

```cpp
void CDX11Device::EndFrame()
{
    m_bFrameRecording = false;
#ifdef WINTERS_PROFILING
```

`CDX11Device::EndFrame` 다음 `CreatePipeline` 함수 바로 위에 실제 GPU idle 대기를 추가한다. `D3D11_QUERY_EVENT`는 immediate-context에서 앞서 제출된 명령이 완료되면 `TRUE`가 된다.

기존 코드:

```cpp
RHIPipelineHandle CDX11Device::CreatePipeline(const RHIPipelineDesc& desc)
```

바로 위에 추가:

```cpp
bool_t CDX11Device::WaitIdle()
{
    if (!m_pDevice || !m_pContext)
        return false;

    D3D11_QUERY_DESC queryDesc{};
    queryDesc.Query = D3D11_QUERY_EVENT;

    Microsoft::WRL::ComPtr<ID3D11Query> pEventQuery;
    if (FAILED(m_pDevice->CreateQuery(&queryDesc, pEventQuery.GetAddressOf())))
        return false;

    m_pContext->End(pEventQuery.Get());
    m_pContext->Flush();

    for (;;)
    {
        BOOL completed = FALSE;
        const HRESULT result = m_pContext->GetData(
            pEventQuery.Get(),
            &completed,
            sizeof(completed),
            0);
        if (result == S_OK && completed)
            return true;
        if (FAILED(result))
            return false;
        SwitchToThread();
    }
}

```

### 2-4. C:/Users/user/Desktop/Winters/Engine/Private/RHI/DX11/CDX11Device.h

public frame API에 `WaitIdle`을 추가한다.

기존 코드:

```cpp
    void    EndFrame() override;
    void    BeginGpuPass(const char* pName) override;
```

아래로 교체:

```cpp
    void    EndFrame() override;
    bool_t  WaitIdle() override;
    void    BeginGpuPass(const char* pName) override;
```

frame recording 상태를 저장한다.

기존 코드:

```cpp
    bool            m_bVSync    = true;
    uint32          m_Width     = 1280;
```

아래로 교체:

```cpp
    bool            m_bVSync    = true;
    bool_t          m_bFrameRecording = false;
    uint32          m_Width     = 1280;
```

### 2-5. C:/Users/user/Desktop/Winters/Engine/Private/RHI/DX12/DX12Device.cpp

debug InfoQueue interface를 명시적으로 사용하도록 include를 추가한다.

기존 코드:

```cpp
#include <d3dcompiler.h>
#include <deque>
```

아래로 교체:

```cpp
#include <d3dcompiler.h>
#include <d3d12sdklayers.h>
#include <deque>
```

`ToInitialBufferState` 전체 함수 바로 아래에 buffer state와 validation helper를 추가한다. `Present/RenderTarget/Depth/UAV`는 이번 buffer resource가 지원하지 않으므로 state 변환 전에 거절한다.

기존 코드:

```cpp
    D3D12_RESOURCE_STATES ToInitialBufferState(eRHIBufferUsage usage)
    {
        switch (usage)
        {
        case eRHIBufferUsage::Index:
            return D3D12_RESOURCE_STATE_INDEX_BUFFER;
        case eRHIBufferUsage::Vertex:
        case eRHIBufferUsage::Constant:
            return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        default:
            return D3D12_RESOURCE_STATE_COMMON;
        }
    }
```

바로 아래에 추가:

```cpp
    bool_t IsSupportedDX12BufferState(eRHIResourceState state)
    {
        switch (state)
        {
        case eRHIResourceState::Common:
        case eRHIResourceState::VertexConstant:
        case eRHIResourceState::IndexBuffer:
        case eRHIResourceState::ShaderResource:
        case eRHIResourceState::CopyDest:
        case eRHIResourceState::CopySource:
            return true;
        default:
            return false;
        }
    }

    bool_t IsUploadHeapCompatibleBufferState(eRHIResourceState state)
    {
        switch (state)
        {
        case eRHIResourceState::VertexConstant:
        case eRHIResourceState::IndexBuffer:
        case eRHIResourceState::ShaderResource:
        case eRHIResourceState::CopySource:
            return true;
        default:
            return false;
        }
    }

    bool_t TryToDX12BufferState(
        eRHIResourceState state,
        D3D12_RESOURCE_STATES& outState)
    {
        switch (state)
        {
        case eRHIResourceState::Common:
            outState = D3D12_RESOURCE_STATE_COMMON;
            return true;
        case eRHIResourceState::VertexConstant:
            outState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
            return true;
        case eRHIResourceState::IndexBuffer:
            outState = D3D12_RESOURCE_STATE_INDEX_BUFFER;
            return true;
        case eRHIResourceState::ShaderResource:
            outState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            return true;
        case eRHIResourceState::CopyDest:
            outState = D3D12_RESOURCE_STATE_COPY_DEST;
            return true;
        case eRHIResourceState::CopySource:
            outState = D3D12_RESOURCE_STATE_COPY_SOURCE;
            return true;
        default:
            return false;
        }
    }

    bool_t TryGetDX12ValidationMessageCount(
        ID3D12Device* pDevice,
        u64_t& outCount)
    {
        outCount = 0;
        if (!pDevice)
            return false;

        Microsoft::WRL::ComPtr<ID3D12InfoQueue> pInfoQueue;
        if (FAILED(pDevice->QueryInterface(IID_PPV_ARGS(&pInfoQueue))))
            return false;

        outCount = pInfoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();
        return true;
    }

    u32_t CountDX12ValidationErrorsSince(
        ID3D12Device* pDevice,
        u64_t firstMessage)
    {
        if (!pDevice)
            return 0;

        Microsoft::WRL::ComPtr<ID3D12InfoQueue> pInfoQueue;
        if (FAILED(pDevice->QueryInterface(IID_PPV_ARGS(&pInfoQueue))))
            return 0;

        const u64_t messageCount =
            pInfoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();
        u32_t errorCount = 0;
        for (u64_t index = firstMessage; index < messageCount; ++index)
        {
            SIZE_T messageSize = 0;
            if (FAILED(pInfoQueue->GetMessage(index, nullptr, &messageSize)) ||
                messageSize == 0)
            {
                continue;
            }

            std::vector<u8_t> storage(messageSize);
            D3D12_MESSAGE* pMessage =
                reinterpret_cast<D3D12_MESSAGE*>(storage.data());
            if (FAILED(pInfoQueue->GetMessage(index, pMessage, &messageSize)))
                continue;

            if (pMessage->Severity == D3D12_MESSAGE_SEVERITY_ERROR ||
                pMessage->Severity == D3D12_MESSAGE_SEVERITY_CORRUPTION)
            {
                ++errorCount;
            }
        }
        return errorCount;
    }
```

`CDX12FrameCommandList`의 Begin/End를 아래로 교체한다. Begin은 이 probe 구간의 native barrier count와 InfoQueue 기준점을 초기화하고, GetDiagnostics는 그 기준점 이후 ERROR/CORRUPTION 메시지만 센다.

기존 코드:

```cpp
    void Begin() override {}
    void End() override {}
```

아래로 교체:

```cpp
    void Begin() override
    {
        m_Diagnostics = {};
        m_bValidationAvailable = m_Owner.m_bDebugLayerEnabled &&
            TryGetDX12ValidationMessageCount(
                m_Owner.m_pDevice.Get(),
                m_uValidationMessageBaseline);
    }

    void End() override {}

    RHICommandListDiagnostics GetDiagnostics() const override
    {
        RHICommandListDiagnostics diagnostics = m_Diagnostics;
        diagnostics.validationAvailable = m_bValidationAvailable;
        if (m_bValidationAvailable)
        {
            diagnostics.validationErrorCount = CountDX12ValidationErrorsSince(
                m_Owner.m_pDevice.Get(),
                m_uValidationMessageBaseline);
        }
        return diagnostics;
    }
```

빈 transition 2개를 아래로 교체한다. 일반 buffer barrier는 열린 frame command list에만 기록한다. upload heap의 실제 native state는 계속 `GENERIC_READ`다.

기존 코드:

```cpp
    void TransitionResource(RHIBufferHandle, eRHIResourceState) override {}
    void TransitionResource(RHITextureHandle, eRHIResourceState) override {}
```

아래로 교체:

```cpp
    bool_t TransitionResource(
        RHIBufferHandle handle,
        eRHIResourceState newState) override
    {
        if (!m_Owner.m_pTables || !m_Owner.m_pCommandList ||
            !m_Owner.m_bFrameRecording)
        {
            OutputDebugStringA(
                "[CDX12FrameCommandList] buffer transition rejected: frame is not recording\n");
            return false;
        }

        CDX12Buffer* pBuffer = m_Owner.m_pTables->bufferTable.Lookup(handle);
        if (!pBuffer)
        {
            OutputDebugStringA(
                "[CDX12FrameCommandList] buffer transition rejected: invalid handle\n");
            return false;
        }

        if (!IsSupportedDX12BufferState(newState))
        {
            OutputDebugStringA(
                "[CDX12FrameCommandList] buffer transition rejected: unsupported state\n");
            return false;
        }

        if (pBuffer->uploadHeap)
        {
            if (!IsUploadHeapCompatibleBufferState(newState))
            {
                OutputDebugStringA(
                    "[CDX12FrameCommandList] upload buffer transition rejected: "
                    "GENERIC_READ is fixed\n");
                return false;
            }
            return true;
        }

        D3D12_RESOURCE_STATES targetState = D3D12_RESOURCE_STATE_COMMON;
        if (!TryToDX12BufferState(newState, targetState))
            return false;

        ID3D12Resource* pResource = GetDX12BufferResource(
            *pBuffer,
            m_Owner.m_iFrameIndex);
        if (!pResource)
            return false;

        if (pBuffer->state == targetState)
            return true;

        const D3D12_RESOURCE_BARRIER barrier = MakeTransitionBarrier(
            pResource,
            pBuffer->state,
            targetState);
        m_Owner.m_pCommandList->ResourceBarrier(1, &barrier);
        ++m_Diagnostics.emittedResourceBarrierCount;
        pBuffer->state = targetState;
        return true;
    }

    bool_t TransitionResource(
        RHITextureHandle,
        eRHIResourceState) override
    {
        OutputDebugStringA(
            "[CDX12FrameCommandList] texture transition rejected: not implemented\n");
        return false;
    }
```

`CDX12FrameCommandList` private 영역의 기존 member block을 아래로 교체한다.

기존 코드:

```cpp
    CDX12Device& m_Owner;
    CDX12PipelineState* m_pCurrentPipeline = nullptr;
```

아래로 교체:

```cpp
    CDX12Device& m_Owner;
    CDX12PipelineState* m_pCurrentPipeline = nullptr;
    RHICommandListDiagnostics m_Diagnostics{};
    u64_t m_uValidationMessageBaseline = 0;
    bool_t m_bValidationAvailable = false;
```

`CDX12Device::CreateDevice`의 debug layer 성공 block에 상태 기록을 추가한다.

기존 코드:

```cpp
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pDebug))))
    {
        pDebug->EnableDebugLayer();
        factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
```

아래로 교체:

```cpp
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pDebug))))
    {
        pDebug->EnableDebugLayer();
        m_bDebugLayerEnabled = true;
        factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
```

`CDX12Device::EndFrame` 다음 fence helper 바로 위에 public idle 대기를 추가한다. queue에 새 fence를 signal하고 완료값을 재확인한 경우에만 true를 반환해 앞서 제출된 probe command list 완료를 fail-closed로 보장한다.

기존 코드:

```cpp
void CDX12Device::WaitForFenceValue(u64_t fenceValue)
```

바로 위에 추가:

```cpp
bool_t CDX12Device::WaitIdle()
{
    if (!m_pCommandQueue || !m_pFence || !m_hFenceEvent)
        return false;

    const u64_t fenceValue = m_uNextFenceValue++;
    if (FAILED(m_pCommandQueue->Signal(m_pFence.Get(), fenceValue)))
        return false;

    u64_t completedValue = m_pFence->GetCompletedValue();
    if (completedValue == UINT64_MAX)
        return false;

    if (completedValue < fenceValue)
    {
        if (FAILED(m_pFence->SetEventOnCompletion(fenceValue, m_hFenceEvent)))
            return false;
        if (WaitForSingleObject(m_hFenceEvent, INFINITE) != WAIT_OBJECT_0)
            return false;
    }

    completedValue = m_pFence->GetCompletedValue();
    if (completedValue == UINT64_MAX || completedValue < fenceValue)
        return false;

    for (u64_t& frameFenceValue : m_uFrameFenceValues)
        frameFenceValue = 0;
    return true;
}

```

### 2-6. C:/Users/user/Desktop/Winters/Engine/Private/RHI/DX12/DX12Device.h

public frame API에 `WaitIdle`을 추가한다.

기존 코드:

```cpp
	void EndFrame() override;

	IRHICommandList* GetFrameCommandList() override;
```

아래로 교체:

```cpp
	void EndFrame() override;
	bool_t WaitIdle() override;

	IRHICommandList* GetFrameCommandList() override;
```

frame recording 상태 바로 위에 debug layer 활성 여부를 추가한다.

기존 코드:

```cpp
	bool_t m_bVSync = true;
	bool_t m_bFrameRecording = false;
```

아래로 교체:

```cpp
	bool_t m_bVSync = true;
	bool_t m_bDebugLayerEnabled = false;
	bool_t m_bFrameRecording = false;
```

### 2-7. C:/Users/user/Desktop/Winters/Engine/Private/Framework/CEngineApp.cpp

anonymous namespace의 기존 `WriteRHISelectionProbeReport` 선언 바로 위에 probe result와 함수를 추가한다. probe buffer를 즉시 파괴하지 않는 이유는 `EndFrame`이 제출한 command list가 GPU에서 끝나기 전 resource를 release하지 않기 위해서다. probe-only process 종료 시 `CDX12Device` destructor가 `WaitForGpu()` 후 resource table을 정리한다.

기존 코드:

```cpp
    bool_t WriteRHISelectionProbeReport(
```

바로 위에 추가:

```cpp
    struct RHIBufferTransitionProbeResult
    {
        const char* pStatus = "not_run";
        u64_t emittedBarrierCount = 0;
        const char* pValidation = "not_run";
    };

    RHIBufferTransitionProbeResult RunRHIBufferTransitionProbe(IRHIDevice& device)
    {
        RHIBufferTransitionProbeResult result{};
        RHIBufferDesc bufferDesc{};
        bufferDesc.sizeBytes = 256;
        bufferDesc.usage = eRHIBufferUsage::Vertex;
        bufferDesc.memoryUsage = eRHIMemoryUsage::Default;
        bufferDesc.debugName = "RHI Buffer Transition Probe";

        const RHIBufferHandle buffer = device.CreateBuffer(bufferDesc);
        if (!buffer.IsValid())
        {
            result.pStatus = "create_buffer_failed";
            return result;
        }

        IRHICommandList* pCommandList = device.GetFrameCommandList();
        if (!pCommandList)
        {
            result.pStatus = "command_list_unavailable";
            return result;
        }

        pCommandList->Begin();
        device.BeginFrame(0.0f, 0.0f, 0.0f, 1.0f);
        const bool_t toCopyDest = pCommandList->TransitionResource(
            buffer,
            eRHIResourceState::CopyDest);
        const bool_t toVertex = pCommandList->TransitionResource(
            buffer,
            eRHIResourceState::VertexConstant);
        pCommandList->End();
        device.EndFrame();
        if (!device.WaitIdle())
        {
            result.pStatus = "idle_wait_failed";
            return result;
        }

        const RHICommandListDiagnostics diagnostics =
            pCommandList->GetDiagnostics();
        result.emittedBarrierCount =
            diagnostics.emittedResourceBarrierCount;

        const RHICapabilities capabilities = device.GetCapabilities();
        if (capabilities.apiRequiresExplicitResourceStates)
        {
            result.pValidation = diagnostics.validationAvailable
                ? (diagnostics.validationErrorCount == 0 ? "pass" : "fail")
                : "unavailable";

            if (result.emittedBarrierCount != 2)
                result.pStatus = "barrier_count_mismatch";
            else if (diagnostics.validationErrorCount != 0)
                result.pStatus = "validation_failed";
#if defined(_DEBUG)
            else if (!diagnostics.validationAvailable)
                result.pStatus = "validation_unavailable";
#endif
            else
                result.pStatus = toCopyDest && toVertex
                    ? "pass"
                    : "transition_failed";
        }
        else
        {
            result.pValidation = "not_applicable";
            result.pStatus = toCopyDest && toVertex &&
                result.emittedBarrierCount == 0
                ? "pass"
                : "semantic_noop_mismatch";
        }

        return result;
    }
```

`WriteRHISelectionProbeReport` signature의 마지막 인자를 아래처럼 확장한다.

기존 코드:

```cpp
        bool_t bFallbackUsed,
        const char* pFallbackReason,
        const RHICapabilities* pCapabilities)
```

아래로 교체:

```cpp
        bool_t bFallbackUsed,
        const char* pFallbackReason,
        const RHICapabilities* pCapabilities,
        const RHIBufferTransitionProbeResult* pBufferTransitionProbe)
```

`frameResourceSlots` 계산 바로 아래에 probe report 값을 추가한다.

기존 코드:

```cpp
        const u32_t frameResourceSlots =
            pCapabilities ? pCapabilities->frameResourceSlotCount : 0u;
```

바로 아래에 추가:

```cpp
        const char* pBufferTransitionStatus = pBufferTransitionProbe
            ? pBufferTransitionProbe->pStatus
            : "not_run";
        const u64_t bufferTransitionBarriers = pBufferTransitionProbe
            ? pBufferTransitionProbe->emittedBarrierCount
            : 0;
        const char* pBufferTransitionValidation = pBufferTransitionProbe
            ? pBufferTransitionProbe->pValidation
            : "not_run";
```

report buffer와 format/call을 아래로 교체한다.

기존 코드:

```cpp
        char report[896]{};
```

아래로 교체:

```cpp
        char report[1152]{};
```

기존 format 문자열 마지막:

```cpp
            "api_requires_explicit_states=%s\nframe_resource_slots=%u\n",
```

아래로 교체:

```cpp
            "api_requires_explicit_states=%s\nframe_resource_slots=%u\n"
            "buffer_transition_probe=%s\nbuffer_transition_barriers=%llu\n"
            "buffer_transition_validation=%s\n",
```

기존 `sprintf_s` 마지막 인자:

```cpp
            pApiRequiresExplicitStates,
            frameResourceSlots);
```

아래로 교체:

```cpp
            pApiRequiresExplicitStates,
            frameResourceSlots,
            pBufferTransitionStatus,
            static_cast<unsigned long long>(bufferTransitionBarriers),
            pBufferTransitionValidation);
```

window 생성 실패 report call의 마지막을 아래로 교체한다.

기존 코드:

```cpp
            false,
            "none",
            nullptr);
```

아래로 교체:

```cpp
            false,
            "none",
            nullptr,
            nullptr);
```

device 생성 실패 report call의 마지막을 아래로 교체한다.

기존 코드:

```cpp
            bFallbackUsed,
            fallbackReason.c_str(),
            nullptr);
```

아래로 교체:

```cpp
            bFallbackUsed,
            fallbackReason.c_str(),
            nullptr,
            nullptr);
```

device 생성 성공 뒤 기존 capability/report/probe-only 블록 전체를 아래로 교체한다.

기존 코드:

```cpp
    const RHICapabilities capabilities = m_pDevice->GetCapabilities();
    WriteRHISelectionProbeReport(
        rhiProbeReportPath,
        config.rhiBackend,
        createResult.moduleName.c_str(),
        RHIBackendName(m_pDevice->GetBackend()),
        RHIBackendCreateStatusName(createResult.status),
        createResult.reason.c_str(),
        bFallbackUsed,
        fallbackReason.c_str(),
        &capabilities);
    if (bRHIProbeOnly)
    {
        m_bRunning = false;
        return true;
    }
```

아래로 교체:

```cpp
    RHIBufferTransitionProbeResult bufferTransitionProbe{};
    if (bRHIProbeOnly)
        bufferTransitionProbe = RunRHIBufferTransitionProbe(*m_pDevice);

    const RHICapabilities capabilities = m_pDevice->GetCapabilities();
    WriteRHISelectionProbeReport(
        rhiProbeReportPath,
        config.rhiBackend,
        createResult.moduleName.c_str(),
        RHIBackendName(m_pDevice->GetBackend()),
        RHIBackendCreateStatusName(createResult.status),
        createResult.reason.c_str(),
        bFallbackUsed,
        fallbackReason.c_str(),
        &capabilities,
        bRHIProbeOnly ? &bufferTransitionProbe : nullptr);
    if (bRHIProbeOnly)
    {
        m_bRunning = false;
        return std::strcmp(bufferTransitionProbe.pStatus, "pass") == 0;
    }
```

### 2-8. C:/Users/user/Desktop/Winters/Tools/Harness/Run-RHIBackendSelectionTruthGate.ps1

필수 report key 배열 마지막을 아래로 교체한다.

기존 코드:

```powershell
                'supports_bindless', 'supports_resource_transitions',
                'api_requires_explicit_states', 'frame_resource_slots')) {
```

아래로 교체:

```powershell
                'supports_bindless', 'supports_resource_transitions',
                'api_requires_explicit_states', 'frame_resource_slots',
                'buffer_transition_probe', 'buffer_transition_barriers',
                'buffer_transition_validation')) {
```

DX11 expected map의 마지막을 아래로 교체한다.

기존 코드:

```powershell
                        api_requires_explicit_states = 'no'
                        frame_resource_slots = '1'
```

아래로 교체:

```powershell
                        api_requires_explicit_states = 'no'
                        frame_resource_slots = '1'
                        buffer_transition_probe = 'pass'
                        buffer_transition_barriers = '0'
                        buffer_transition_validation = 'not_applicable'
```

DX12 expected map의 마지막을 아래로 교체한다.

기존 코드:

```powershell
                        api_requires_explicit_states = 'yes'
                        frame_resource_slots = '2'
```

아래로 교체:

```powershell
                        api_requires_explicit_states = 'yes'
                        frame_resource_slots = '2'
                        buffer_transition_probe = 'pass'
                        buffer_transition_barriers = '2'
                        buffer_transition_validation = 'pass'
```

`default` expected map의 마지막을 아래로 교체한다.

기존 코드:

```powershell
                        api_requires_explicit_states = 'no'
                        frame_resource_slots = '0'
```

아래로 교체:

```powershell
                        api_requires_explicit_states = 'no'
                        frame_resource_slots = '0'
                        buffer_transition_probe = 'not_run'
                        buffer_transition_barriers = '0'
                        buffer_transition_validation = 'not_run'
```

## 3. 검증

예측:
- static search에서 `IRHICommandList` transition 반환형과 DX11/DX12 override가 모두 `bool_t`로 일치하고, 빈 buffer transition 구현은 0건이다.
- 기존 8개 selection truth case가 모두 통과한다. DX11은 `probe=pass/barriers=0/validation=not_applicable`, Debug DX12는 `pass/2/pass`, Vulkan 미등록 실패는 `not_run/0/not_run`을 보고한다.
- DX12 probe는 default-heap vertex buffer를 `VERTEX_AND_CONSTANT_BUFFER -> COPY_DEST -> VERTEX_AND_CONSTANT_BUFFER`로 기록·제출하고 queue fence 완료를 기다린 뒤, wrapper가 실제 `ResourceBarrier` 호출 직후 누적한 count 2와 그 구간 InfoQueue ERROR/CORRUPTION 0을 함께 증명한다.
- `supportsResourceTransitions`는 DX11/DX12 모두 계속 `no`다. texture transition 미구현을 숨기지 않는다.
- Engine→Client Debug 빌드와 일반 non-probe DX11 제품 smoke는 유지된다.

검증 명령:
- `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Engine\Include\Engine.vcxproj /p:Configuration=Debug /p:Platform=x64 /m /nr:false /v:minimal`
- `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m /nr:false /v:minimal`
- `powershell -NoProfile -ExecutionPolicy Bypass -File Tools\Harness\Run-RHIBackendSelectionTruthGate.ps1 -Configuration Debug`
- source/EngineSDK `IRHICommandList.h`, `IRHIDevice.h` SHA-256 equality
- 후속 동기화: Engine public header 변경 후 Engine build가 호출하는 `UpdateLib.bat` 실행 필요
- repo root cwd에서 `Client\Bin\Debug\WintersGame.exe --rhi=dx11 --run-seconds=2`, exit 0
- path-scoped `git diff --check`

미검증:
- DX12 texture transition, render-target/depth/UAV resource creation과 descriptor lifecycle
- DX12 normal LoL product parity와 Vulkan module
- Release DX12의 debug-layer unavailable 경로. Debug gate는 InfoQueue availability와 ERROR/CORRUPTION 0을 강제한다.

확인 필요:
- 없음. 현재 코드가 buffer state owner, open-frame 조건, probe-only lifetime과 Engine→Client 배포 순서를 모두 제공한다.

## 서브 에이전트 비평

비평 주체: `/root/rhi_truth_gate_critique` read-only 검토.

초기 판정: P0 0, P1 3, P2 2. 구현 시작 불가.

- 수용 — DX11 handle-only 성공이 잘못된 state와 texture 미구현을 숨김: DX11 buffer도 DX12와 같은 논리 상태 목록을 검증하고 texture는 false로 변경했다.
- 수용 — `bool_t`만으로 native barrier를 증명하지 못함: public command diagnostics를 추가해 DX12 실제 `ResourceBarrier` 발행 count 2와 Debug InfoQueue ERROR/CORRUPTION 0을 report/gate가 확인하도록 변경했다.
- 수용 — 기존 파일 편집 anchor 불충분: helper, class member, report schema, harness map 모두 실제 기존 코드와 `바로 위/아래 추가` 또는 `아래로 교체` 블록으로 구체화했다.
- 수용(P2) — public header SDK parity: Engine build 후 UpdateLib 동기화와 SHA-256 검증을 명시했다.
- 확인 — upload heap `GENERIC_READ` 고정, probe state 순서, destructor `WaitForGpu` 수명 처리는 타당하다는 비평 결과를 유지했다.

1차 재비평: P0 0, P1 1, P2 1. 구현 시작 불가.

- 수용 — `EndFrame` 직후 InfoQueue를 읽어 GPU 실행 시점 오류를 놓칠 수 있음: `IRHIDevice::WaitIdle`을 추가하고 DX12 queue fence, DX11 event query로 probe 제출 완료 후 diagnostics를 읽도록 변경했다. 일반 프레임에는 호출하지 않는다.
- 수용(P2) — frame 밖 transition 허용 비대칭: DX11도 `m_bFrameRecording`을 소유하고 열린 frame에서만 buffer semantic no-op을 성공시킨다.

2차 재비평: P0 0, P1 1, P2 0. 구현 시작 불가.

- 수용 — `void WaitIdle`이 Signal/query 실패를 숨겨 fail-open 가능: 반환형을 `bool_t`로 변경했다. DX12는 Signal·event wait·completed value를 모두 확인하고, DX11은 query 생성과 `GetData == S_OK && TRUE`에서만 true다. probe는 false면 diagnostics를 읽지 않고 `idle_wait_failed`로 종료한다.

3차 재비평: P0 0, P1 1, P2 0. 구현 시작 불가.

- 수용 — `ID3D12Fence::GetCompletedValue()`의 device-removed sentinel `UINT64_MAX`가 단순 대소 비교를 통과함: event 등록 전과 wait 후 두 완료값 모두 sentinel을 먼저 거부한다.

4차 최종 재비평: **PASS — P0 0, P1 0, P2 0. 구현 시작 가능.**
