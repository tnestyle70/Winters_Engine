Session - ShaderResource 전용 texture usage와 DX11/DX12 texture state transition 첫 수직 절편 완성
좌표: 신규 좌표 후보 · 축: C4 수명은 선언된다, C8 검증이 병목
관련: 2026-07-21_RHI_DX12_BUFFER_STATE_TRANSITION_PLAN.md, 2026-07-21_RHI_DX12_BUFFER_STATE_TRANSITION_RESULT.md

## 1. 결정 기록

① 문제·제약: `RHITextureDesc::usageFlags`는 4개 비트를 선언하지만 소비처는 0곳이고, 두 backend의 `CreateTexture`는 실제로 ShaderResource 전용 2D texture만 만든다. texture transition 구현도 0개다.
② 순진한 해법의 실패: RenderTarget·DepthStencil·UAV를 한 번에 허용하면 DX11 view 3종과 DX12 descriptor/state/render-pass 수명이 동시에 필요해 첫 오류의 원인을 분리할 수 없다.
③ 메커니즘: 2C-1은 `usageFlags == ShaderResource`, `depth == 1`만 fail-closed 허용하고 실제 probe가 왕복하는 `ShaderResource/CopyDest` 전이만 구현한다. 나머지 usage·depth·state는 음성 probe로 거부를 고정한다.
④ 대조: DX11은 상태 장벽을 드라이버가 흡수하므로 handle·usage·state·frame 검증 후 semantic no-op, DX12는 wrapper state와 실제 `ResourceBarrier`를 사용한다.
⑤ 대가: RenderTarget·DepthStencil·UAV 조합은 계속 생성 실패하며 `supportsResourceTransitions=false`를 유지한다. 2C-2/2C-3에서 view/descriptor/render-pass probe와 함께 별도 개방한다.

## 2. 반영해야 하는 코드

### 2-1. C:/Users/user/Desktop/Winters/Engine/Private/RHI/DX11/CDX11Device.cpp

`IsSupportedRHIBufferTransitionState` 함수 바로 아래에 추가한다.

기존 코드:

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

아래에 추가:

```cpp
    bool_t IsShaderResourceOnlyTextureUsage(u32_t usageFlags)
    {
        return usageFlags == static_cast<u32_t>(eRHITextureUsage::ShaderResource);
    }

    bool_t IsSupportedRHITextureTransitionState(eRHIResourceState state)
    {
        switch (state)
        {
        case eRHIResourceState::ShaderResource:
        case eRHIResourceState::CopyDest:
            return true;
        default:
            return false;
        }
    }
```

`CDX11FrameCommandList`의 texture overload를 아래로 교체한다.

기존 코드:

```cpp
    bool_t TransitionResource(
        RHITextureHandle,
        eRHIResourceState) override
    {
        OutputDebugStringA(
            "[CDX11FrameCommandList] texture transition rejected: not implemented\n");
        return false;
    }
```

아래로 교체:

```cpp
    bool_t TransitionResource(
        RHITextureHandle handle,
        eRHIResourceState newState) override
    {
        if (!m_Owner.m_bFrameRecording ||
            !IsSupportedRHITextureTransitionState(newState) ||
            !m_Owner.m_pTables)
        {
            return false;
        }

        CDX11TextureObject* pTexture =
            m_Owner.m_pTables->textureTable.Lookup(handle);
        return pTexture &&
            IsShaderResourceOnlyTextureUsage(pTexture->desc.usageFlags);
    }
```

`CDX11Device::CreateTexture`의 첫 guard를 아래로 교체한다.

기존 코드:

```cpp
    if (!m_pDevice || !m_pTables || desc.width == 0 || desc.height == 0)
        return {};
```

아래로 교체:

```cpp
    if (!m_pDevice || !m_pTables ||
        desc.width == 0 || desc.height == 0 || desc.depth != 1 ||
        !IsShaderResourceOnlyTextureUsage(desc.usageFlags))
    {
        OutputDebugStringA(
            "[CDX11Device] CreateTexture rejected: only ShaderResource 2D usage is implemented\n");
        return {};
    }
```

### 2-2. C:/Users/user/Desktop/Winters/Engine/Private/RHI/DX12/DX12Device.cpp

`TryToDX12BufferState` 함수 바로 아래에 추가한다.

기존 코드:

```cpp
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
```

아래에 추가:

```cpp
    bool_t IsShaderResourceOnlyTextureUsage(u32_t usageFlags)
    {
        return usageFlags == static_cast<u32_t>(eRHITextureUsage::ShaderResource);
    }

    bool_t TryToDX12TextureState(
        eRHIResourceState state,
        D3D12_RESOURCE_STATES& outState)
    {
        switch (state)
        {
        case eRHIResourceState::ShaderResource:
            outState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            return true;
        case eRHIResourceState::CopyDest:
            outState = D3D12_RESOURCE_STATE_COPY_DEST;
            return true;
        default:
            return false;
        }
    }
```

`CDX12FrameCommandList`의 texture overload를 아래로 교체한다.

기존 코드:

```cpp
    bool_t TransitionResource(
        RHITextureHandle,
        eRHIResourceState) override
    {
        OutputDebugStringA(
            "[CDX12FrameCommandList] texture transition rejected: not implemented\n");
        return false;
    }
```

아래로 교체:

```cpp
    bool_t TransitionResource(
        RHITextureHandle handle,
        eRHIResourceState newState) override
    {
        if (!m_Owner.m_pTables || !m_Owner.m_pCommandList ||
            !m_Owner.m_bFrameRecording)
        {
            OutputDebugStringA(
                "[CDX12FrameCommandList] texture transition rejected: frame is not recording\n");
            return false;
        }

        CDX12Texture* pTexture =
            m_Owner.m_pTables->textureTable.Lookup(handle);
        if (!pTexture || !pTexture->pResource ||
            !IsShaderResourceOnlyTextureUsage(pTexture->desc.usageFlags))
        {
            OutputDebugStringA(
                "[CDX12FrameCommandList] texture transition rejected: invalid texture or usage\n");
            return false;
        }

        D3D12_RESOURCE_STATES targetState = D3D12_RESOURCE_STATE_COMMON;
        if (!TryToDX12TextureState(newState, targetState))
        {
            OutputDebugStringA(
                "[CDX12FrameCommandList] texture transition rejected: unsupported state\n");
            return false;
        }

        if (pTexture->state == targetState)
            return true;

        const D3D12_RESOURCE_BARRIER barrier = MakeTransitionBarrier(
            pTexture->pResource.Get(),
            pTexture->state,
            targetState);
        m_Owner.m_pCommandList->ResourceBarrier(1, &barrier);
        ++m_Diagnostics.emittedResourceBarrierCount;
        pTexture->state = targetState;
        return true;
    }
```

`CDX12Device::CreateTexture`의 첫 guard를 아래로 교체한다.

기존 코드:

```cpp
    if (!m_pDevice || !m_pTables || desc.width == 0 || desc.height == 0)
        return {};
```

아래로 교체:

```cpp
    if (!m_pDevice || !m_pTables ||
        desc.width == 0 || desc.height == 0 || desc.depth != 1 ||
        !IsShaderResourceOnlyTextureUsage(desc.usageFlags))
    {
        OutputDebugStringA(
            "[CDX12Device] CreateTexture rejected: only ShaderResource 2D usage is implemented\n");
        return {};
    }
```

### 2-3. C:/Users/user/Desktop/Winters/Engine/Private/Framework/CEngineApp.cpp

`RunRHIBufferTransitionProbe` 함수 바로 아래에 texture probe 결과 타입과 함수를 추가한다.

기존 코드:

```cpp
        return result;
    }

    bool_t WriteRHISelectionProbeReport(
```

아래에 추가:

```cpp
    struct RHITextureTransitionProbeResult
    {
        const char* pStatus = "not_run";
        u64_t emittedBarrierCount = 0;
        const char* pValidation = "not_run";
    };

    RHITextureTransitionProbeResult RunRHITextureTransitionProbe(IRHIDevice& device)
    {
        RHITextureTransitionProbeResult result{};
        RHITextureDesc textureDesc{};
        textureDesc.width = 2;
        textureDesc.height = 2;
        textureDesc.depth = 1;
        textureDesc.mipLevels = 1;
        textureDesc.format = eRHIFormat::R8G8B8A8_UNorm;
        textureDesc.usageFlags =
            static_cast<u32_t>(eRHITextureUsage::ShaderResource);
        textureDesc.debugName = "RHI Texture Transition Probe";

        constexpr u32_t kUnsupportedUsageFlags[] =
        {
            static_cast<u32_t>(eRHITextureUsage::RenderTarget),
            static_cast<u32_t>(eRHITextureUsage::DepthStencil),
            static_cast<u32_t>(eRHITextureUsage::UnorderedAccess),
            static_cast<u32_t>(eRHITextureUsage::ShaderResource) |
                static_cast<u32_t>(eRHITextureUsage::RenderTarget),
        };
        for (u32_t usageFlags : kUnsupportedUsageFlags)
        {
            RHITextureDesc invalidUsageDesc = textureDesc;
            invalidUsageDesc.usageFlags = usageFlags;
            if (device.CreateTexture(invalidUsageDesc).IsValid())
            {
                result.pStatus = "unsupported_usage_accepted";
                return result;
            }
        }

        RHITextureDesc invalidDepthDesc = textureDesc;
        invalidDepthDesc.depth = 2;
        if (device.CreateTexture(invalidDepthDesc).IsValid())
        {
            result.pStatus = "unsupported_depth_accepted";
            return result;
        }

        const u32_t texels[4] =
        {
            0xFFFFFFFFu, 0xFF000000u,
            0xFF000000u, 0xFFFFFFFFu,
        };
        const RHITextureHandle texture = device.CreateTexture(
            textureDesc,
            texels,
            textureDesc.width * static_cast<u32_t>(sizeof(u32_t)));
        if (!texture.IsValid())
        {
            result.pStatus = "create_texture_failed";
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
        constexpr eRHIResourceState kRejectedStates[] =
        {
            eRHIResourceState::Common,
            eRHIResourceState::VertexConstant,
            eRHIResourceState::IndexBuffer,
            eRHIResourceState::RenderTarget,
            eRHIResourceState::DepthRead,
            eRHIResourceState::DepthWrite,
            eRHIResourceState::UnorderedAccess,
            eRHIResourceState::CopySource,
            eRHIResourceState::Present,
        };
        bool_t rejectedStates = true;
        for (eRHIResourceState state : kRejectedStates)
        {
            if (pCommandList->TransitionResource(texture, state))
                rejectedStates = false;
        }

        const bool_t toCopyDest = pCommandList->TransitionResource(
            texture,
            eRHIResourceState::CopyDest);
        const bool_t toShaderResource = pCommandList->TransitionResource(
            texture,
            eRHIResourceState::ShaderResource);
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

            if (!rejectedStates)
                result.pStatus = "unsupported_state_accepted";
            else if (result.emittedBarrierCount != 2)
                result.pStatus = "barrier_count_mismatch";
            else if (diagnostics.validationErrorCount != 0)
                result.pStatus = "validation_failed";
#if defined(_DEBUG)
            else if (!diagnostics.validationAvailable)
                result.pStatus = "validation_unavailable";
#endif
            else
                result.pStatus = toCopyDest && toShaderResource
                    ? "pass"
                    : "transition_failed";
        }
        else
        {
            result.pValidation = "not_applicable";
            result.pStatus = rejectedStates &&
                toCopyDest && toShaderResource &&
                result.emittedBarrierCount == 0
                ? "pass"
                : "semantic_noop_mismatch";
        }

        return result;
    }
```

`WriteRHISelectionProbeReport` signature 끝을 아래로 교체한다.

기존 코드:

```cpp
        const RHICapabilities* pCapabilities,
        const RHIBufferTransitionProbeResult* pBufferTransitionProbe)
```

아래로 교체:

```cpp
        const RHICapabilities* pCapabilities,
        const RHIBufferTransitionProbeResult* pBufferTransitionProbe,
        const RHITextureTransitionProbeResult* pTextureTransitionProbe)
```

buffer transition report 로컬 값 바로 아래에 texture 값을 추가한다.

기존 코드:

```cpp
        const char* pBufferTransitionValidation = pBufferTransitionProbe
            ? pBufferTransitionProbe->pValidation
            : "not_run";
```

아래에 추가:

```cpp
        const char* pTextureTransitionStatus = pTextureTransitionProbe
            ? pTextureTransitionProbe->pStatus
            : "not_run";
        const u64_t textureTransitionBarriers = pTextureTransitionProbe
            ? pTextureTransitionProbe->emittedBarrierCount
            : 0;
        const char* pTextureTransitionValidation = pTextureTransitionProbe
            ? pTextureTransitionProbe->pValidation
            : "not_run";
```

report buffer와 `sprintf_s` 전체를 아래로 교체한다. format placeholder와 vararg 순서를 한 블록으로 고정한다.

기존 코드:

```cpp
        char report[1152]{};
        sprintf_s(
            report,
            "source=%s\nrequested=%s\nmodule=%s\nselected=%s\n"
            "status=%s\nreason=%s\nfallback=%s\nfallback_reason=%s\n"
            "capability_backend=%s\nfeature_tier=%s\n"
            "supports_compute=%s\nsupports_async_compute=%s\n"
            "supports_bindless=%s\nsupports_resource_transitions=%s\n"
            "api_requires_explicit_states=%s\nframe_resource_slots=%u\n"
            "buffer_transition_probe=%s\nbuffer_transition_barriers=%llu\n"
            "buffer_transition_validation=%s\n",
            pSource,
            EngineRHIBackendName(requested),
            pModule,
            pSelected,
            pStatus,
            pReason,
            bFallbackUsed ? "yes" : "no",
            pFallbackReason,
            pCapabilityBackend,
            pFeatureTier,
            pCompute,
            pAsyncCompute,
            pBindless,
            pResourceTransitions,
            pApiRequiresExplicitStates,
            frameResourceSlots,
            pBufferTransitionStatus,
            static_cast<unsigned long long>(bufferTransitionBarriers),
            pBufferTransitionValidation);
```

아래로 교체:

```cpp
        char report[1536]{};
        sprintf_s(
            report,
            "source=%s\nrequested=%s\nmodule=%s\nselected=%s\n"
            "status=%s\nreason=%s\nfallback=%s\nfallback_reason=%s\n"
            "capability_backend=%s\nfeature_tier=%s\n"
            "supports_compute=%s\nsupports_async_compute=%s\n"
            "supports_bindless=%s\nsupports_resource_transitions=%s\n"
            "api_requires_explicit_states=%s\nframe_resource_slots=%u\n"
            "buffer_transition_probe=%s\nbuffer_transition_barriers=%llu\n"
            "buffer_transition_validation=%s\n"
            "texture_transition_probe=%s\ntexture_transition_barriers=%llu\n"
            "texture_transition_validation=%s\n",
            pSource,
            EngineRHIBackendName(requested),
            pModule,
            pSelected,
            pStatus,
            pReason,
            bFallbackUsed ? "yes" : "no",
            pFallbackReason,
            pCapabilityBackend,
            pFeatureTier,
            pCompute,
            pAsyncCompute,
            pBindless,
            pResourceTransitions,
            pApiRequiresExplicitStates,
            frameResourceSlots,
            pBufferTransitionStatus,
            static_cast<unsigned long long>(bufferTransitionBarriers),
            pBufferTransitionValidation,
            pTextureTransitionStatus,
            static_cast<unsigned long long>(textureTransitionBarriers),
            pTextureTransitionValidation);
```

window/device 초기화 실패 호출 두 곳의 동일한 tail을 각각 아래로 교체한다.

기존 코드:

```cpp
            nullptr,
            nullptr);
```

아래로 교체:

```cpp
            nullptr,
            nullptr,
            nullptr);
```

기존 코드:

```cpp
    RHIBufferTransitionProbeResult bufferTransitionProbe{};
    if (bRHIProbeOnly)
        bufferTransitionProbe = RunRHIBufferTransitionProbe(*m_pDevice);
```

아래로 교체:

```cpp
    RHIBufferTransitionProbeResult bufferTransitionProbe{};
    RHITextureTransitionProbeResult textureTransitionProbe{};
    if (bRHIProbeOnly)
    {
        bufferTransitionProbe = RunRHIBufferTransitionProbe(*m_pDevice);
        textureTransitionProbe = RunRHITextureTransitionProbe(*m_pDevice);
    }
```

기존 코드:

```cpp
        bRHIProbeOnly ? &bufferTransitionProbe : nullptr);
    if (bRHIProbeOnly)
    {
        m_bRunning = false;
        return std::strcmp(bufferTransitionProbe.pStatus, "pass") == 0;
    }
```

아래로 교체:

```cpp
        bRHIProbeOnly ? &bufferTransitionProbe : nullptr,
        bRHIProbeOnly ? &textureTransitionProbe : nullptr);
    if (bRHIProbeOnly)
    {
        m_bRunning = false;
        return std::strcmp(bufferTransitionProbe.pStatus, "pass") == 0 &&
            std::strcmp(textureTransitionProbe.pStatus, "pass") == 0;
    }
```

### 2-4. C:/Users/user/Desktop/Winters/Tools/Harness/Run-RHIBackendSelectionTruthGate.ps1

필수 report key 배열을 아래로 교체한다.

기존 코드:

```powershell
            foreach ($key in @(
                'source', 'requested', 'module', 'selected',
                'status', 'reason', 'fallback', 'fallback_reason',
                'capability_backend', 'feature_tier',
                'supports_compute', 'supports_async_compute',
                'supports_bindless', 'supports_resource_transitions',
                'api_requires_explicit_states', 'frame_resource_slots',
                'buffer_transition_probe', 'buffer_transition_barriers',
                'buffer_transition_validation')) {
```

아래로 교체:

```powershell
            foreach ($key in @(
                'source', 'requested', 'module', 'selected',
                'status', 'reason', 'fallback', 'fallback_reason',
                'capability_backend', 'feature_tier',
                'supports_compute', 'supports_async_compute',
                'supports_bindless', 'supports_resource_transitions',
                'api_requires_explicit_states', 'frame_resource_slots',
                'buffer_transition_probe', 'buffer_transition_barriers',
                'buffer_transition_validation',
                'texture_transition_probe', 'texture_transition_barriers',
                'texture_transition_validation')) {
```

DX11 expected map을 아래로 교체한다.

기존 코드:

```powershell
                    @{
                        capability_backend = 'DX11'
                        feature_tier = 'LegacyDX11'
                        supports_compute = 'no'
                        supports_async_compute = 'no'
                        supports_bindless = 'no'
                        supports_resource_transitions = 'no'
                        api_requires_explicit_states = 'no'
                        frame_resource_slots = '1'
                        buffer_transition_probe = 'pass'
                        buffer_transition_barriers = '0'
                        buffer_transition_validation = 'not_applicable'
                    }
```

아래로 교체:

```powershell
                    @{
                        capability_backend = 'DX11'
                        feature_tier = 'LegacyDX11'
                        supports_compute = 'no'
                        supports_async_compute = 'no'
                        supports_bindless = 'no'
                        supports_resource_transitions = 'no'
                        api_requires_explicit_states = 'no'
                        frame_resource_slots = '1'
                        buffer_transition_probe = 'pass'
                        buffer_transition_barriers = '0'
                        buffer_transition_validation = 'not_applicable'
                        texture_transition_probe = 'pass'
                        texture_transition_barriers = '0'
                        texture_transition_validation = 'not_applicable'
                    }
```

DX12 expected map을 아래로 교체한다.

기존 코드:

```powershell
                    @{
                        capability_backend = 'DX12'
                        feature_tier = 'ExplicitDesktop'
                        supports_compute = 'no'
                        supports_async_compute = 'no'
                        supports_bindless = 'no'
                        supports_resource_transitions = 'no'
                        api_requires_explicit_states = 'yes'
                        frame_resource_slots = '2'
                        buffer_transition_probe = 'pass'
                        buffer_transition_barriers = '2'
                        buffer_transition_validation = 'pass'
                    }
```

아래로 교체:

```powershell
                    @{
                        capability_backend = 'DX12'
                        feature_tier = 'ExplicitDesktop'
                        supports_compute = 'no'
                        supports_async_compute = 'no'
                        supports_bindless = 'no'
                        supports_resource_transitions = 'no'
                        api_requires_explicit_states = 'yes'
                        frame_resource_slots = '2'
                        buffer_transition_probe = 'pass'
                        buffer_transition_barriers = '2'
                        buffer_transition_validation = 'pass'
                        texture_transition_probe = 'pass'
                        texture_transition_barriers = '2'
                        texture_transition_validation = 'pass'
                    }
```

default/미등록 Vulkan expected map을 아래로 교체한다.

기존 코드:

```powershell
                    @{
                        capability_backend = 'None'
                        feature_tier = 'None'
                        supports_compute = 'no'
                        supports_async_compute = 'no'
                        supports_bindless = 'no'
                        supports_resource_transitions = 'no'
                        api_requires_explicit_states = 'no'
                        frame_resource_slots = '0'
                        buffer_transition_probe = 'not_run'
                        buffer_transition_barriers = '0'
                        buffer_transition_validation = 'not_run'
                    }
```

아래로 교체:

```powershell
                    @{
                        capability_backend = 'None'
                        feature_tier = 'None'
                        supports_compute = 'no'
                        supports_async_compute = 'no'
                        supports_bindless = 'no'
                        supports_resource_transitions = 'no'
                        api_requires_explicit_states = 'no'
                        frame_resource_slots = '0'
                        buffer_transition_probe = 'not_run'
                        buffer_transition_barriers = '0'
                        buffer_transition_validation = 'not_run'
                        texture_transition_probe = 'not_run'
                        texture_transition_barriers = '0'
                        texture_transition_validation = 'not_run'
                    }
```

## 3. 검증

예측:
- 기존 `CreateTexture` 호출 6개 경로와 신규 probe 1개는 모두 ShaderResource usage이다. probe는 실제 제품 호출자와 같은 initial-data upload 경로를 실행한다.
- DX11 texture probe는 `pass/0/not_applicable`, DX12 Debug는 `pass/2/pass`, 미등록 Vulkan은 `not_run/0/not_run`을 보고한다.
- DX12의 두 barrier는 `ShaderResource -> CopyDest -> ShaderResource`이며 queue fence 완료 뒤 InfoQueue ERROR/CORRUPTION은 0이다.
- RenderTarget·DepthStencil·UnorderedAccess·ShaderResource|RenderTarget usage, `depth=2`, 그리고 ShaderResource/CopyDest 외 모든 texture state가 두 backend에서 실패한다.
- `supportsResourceTransitions`는 두 backend 모두 `no`를 유지한다.

검증 명령:
- `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Engine\Include\Engine.vcxproj /p:Configuration=Debug /p:Platform=x64 /m /nr:false /v:minimal`
- `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m /nr:false /v:minimal`
- `powershell -NoProfile -ExecutionPolicy Bypass -File Tools\Harness\Run-RHIBackendSelectionTruthGate.ps1 -Configuration Debug`
- repo root cwd에서 `Client\Bin\Debug\WintersGame.exe --rhi=dx11 --run-seconds=2`, exit 0
- `rg -n "usageFlags" Engine Client --glob '!EngineSDK/**'`로 기존 호출 usage 확인
- path-scoped `git diff --check`

미검증:
- color/depth attachment view 생성, render-pass load/store, UAV descriptor와 UAV barrier
- DX12 normal LoL product parity와 Vulkan backend

확인 필요:
- 없음. 현재 호출자는 모두 ShaderResource 전용이고 이번 절편은 public header를 변경하지 않는다.

## 서브 에이전트 비평

비평 주체: `/root/rhi_truth_gate_critique` read-only 검토.

초기 판정: `FAIL — P0 0, P1 2, P2 1`. 구현 시작 불가.

- 수용 — CEngineApp report/harness가 산문과 부분 조각뿐이어서 format/vararg/map 손상을 막지 못함: signature, 로컬 값, 1536-byte report, 전체 `sprintf_s`, 실패 호출 tail, key 배열과 backend별 map을 실제 기존 코드 기준 전체 교체 블록으로 보강했다.
- 수용 — 선언한 fail-closed usage/depth/state 계약을 유효 전이 두 개만으로 검증함: 허용 state 계약을 `ShaderResource/CopyDest`로 좁히고 unsupported usage 4종, `depth=2`, 나머지 state 9종 음성 probe를 추가했다. 잘못된 state가 barrier/state를 바꾸면 기대 count 2도 실패한다.
- 수용(P2) — 기존 호출자 수 7개 오기: 기존 6개 + 신규 probe 1개로 수정했고, probe가 actual caller와 같은 initial-data upload path를 사용하게 했다.

재비평 판정: `PASS — P0 0, P1 0, P2 0`. 구현 시작 가능.
