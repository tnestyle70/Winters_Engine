Session - Phase 0 측정 인프라: 프로파일러 JSON에 트렁케이션 플래그를 넣고, GPU 프레임 타임스탬프 카운터를 추가하고, F4 캡처를 타임스탬프 아카이브로 저장하며 F11로 프레임 리미터를 토글한다.

참고: 마스터 플랜 Phase 0의 S1(카운터 중복 strcmp 수정)은 `Engine/Private/Core/Profiler/CPUProfiler.cpp`의 `AddCounter`에 이미 반영되어 있어 제외. S5(워커 스레드 캡처)는 `ProfilerEvent.threadId` + thread_local 스택으로 이미 동작하므로 검증 항목으로만 둔다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Engine/Private/Manager/Profiler/ProfilerOverlay.cpp

`Save_DisplayFrameToJson` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
	out << "  \"rawEventCount\": " << m_vDisplayEvents.size() << ",\n";
```

아래에 추가:

```cpp
	out << "  \"scopeStatCap\": " << PROFILER_MAX_SCOPE_STATS_PER_FRAME << ",\n";
	out << "  \"counterCap\": " << PROFILER_MAX_COUNTERS_PER_FRAME << ",\n";
	out << "  \"rawEventCap\": " << PROFILER_MAX_TREE_EVENTS_PER_FRAME << ",\n";
	out << "  \"truncatedScopes\": "
		<< (m_vDisplayStats.size() >= PROFILER_MAX_SCOPE_STATS_PER_FRAME ? "true" : "false") << ",\n";
	out << "  \"truncatedCounters\": "
		<< (m_vDisplayCounters.size() >= PROFILER_MAX_COUNTERS_PER_FRAME ? "true" : "false") << ",\n";
	out << "  \"truncatedRawEvents\": "
		<< (m_vDisplayEvents.size() >= PROFILER_MAX_TREE_EVENTS_PER_FRAME ? "true" : "false") << ",\n";
```

1-2. C:/Users/tnest/Desktop/Winters/Engine/Private/RHI/DX11/CDX11Device.h

private 영역에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    bool    CreateDepthStencil(uint32 width, uint32 height);
```

아래에 추가:

```cpp
    bool    CreateGpuTimingQueries();
    void    ReadGpuTimingResults();
```

기존 코드:

```cpp
    D3D11_VIEWPORT  m_Viewport  = {};
    bool            m_bVSync    = true;
    uint32          m_Width     = 1280;
    uint32          m_Height    = 720;
```

아래에 추가:

```cpp
    // GPU 프레임 타임스탬프: disjoint+begin/end 쿼리를 N슬롯 링으로 두고
    // 수 프레임 지연 후 non-blocking readback 한다 (GPU::FrameUs 카운터).
    static constexpr uint32 kGpuTimingSlots = 4u;
    struct GpuTimingSlot
    {
        Microsoft::WRL::ComPtr<ID3D11Query> pDisjoint;
        Microsoft::WRL::ComPtr<ID3D11Query> pBegin;
        Microsoft::WRL::ComPtr<ID3D11Query> pEnd;
        bool bPending = false;
    };
    GpuTimingSlot   m_GpuTimingSlots[kGpuTimingSlots];
    uint32          m_uGpuTimingWriteIndex = 0;
    bool            m_bGpuTimingReady = false;
```

1-3. C:/Users/tnest/Desktop/Winters/Engine/Private/RHI/DX11/CDX11Device.cpp

기존 코드:

```cpp
#include "WintersPCH.h"
#include "RHI/DX11/CDX11Device.h"
#include "WintersCore.h"
```

아래로 교체:

```cpp
#include "WintersPCH.h"
#include "RHI/DX11/CDX11Device.h"
#include "WintersCore.h"
#include "ProfilerAPI.h"
```

`bool CDX11Device::Initialize(const DeviceDesc& desc)` 안에서 아래 기존 코드를:

```cpp
    // ── 기본 렌더타겟 바인딩 ─────────────────────────────────
    m_pContext->OMSetRenderTargets(1, m_pRenderTargetView.GetAddressOf(), m_pDepthStencilView.Get());

    return true;
```

아래로 교체:

```cpp
    // ── 기본 렌더타겟 바인딩 ─────────────────────────────────
    m_pContext->OMSetRenderTargets(1, m_pRenderTargetView.GetAddressOf(), m_pDepthStencilView.Get());

    // GPU 타이밍 쿼리는 실패해도 디바이스 초기화를 막지 않는다.
    m_bGpuTimingReady = CreateGpuTimingQueries();

    return true;
```

`void CDX11Device::BeginFrame(float32 r, float32 g, float32 b, float32 a)` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    // 매 프레임 RT + 뷰포트 재설정 (DISCARD 스왑체인 안정성)
    m_pContext->OMSetRenderTargets(1, m_pRenderTargetView.GetAddressOf(), m_pDepthStencilView.Get());
    m_pContext->RSSetViewports(1, &m_Viewport);
```

아래에 추가:

```cpp
    if (m_bGpuTimingReady)
    {
        GpuTimingSlot& slot = m_GpuTimingSlots[m_uGpuTimingWriteIndex];
        if (!slot.bPending)
        {
            m_pContext->Begin(slot.pDisjoint.Get());
            m_pContext->End(slot.pBegin.Get());
        }
    }
```

`void CDX11Device::EndFrame()` 전체를 아래로 교체:

```cpp
void CDX11Device::EndFrame()
{
    if (m_bGpuTimingReady)
    {
        GpuTimingSlot& slot = m_GpuTimingSlots[m_uGpuTimingWriteIndex];
        if (!slot.bPending)
        {
            m_pContext->End(slot.pEnd.Get());
            m_pContext->End(slot.pDisjoint.Get());
            slot.bPending = true;
            m_uGpuTimingWriteIndex = (m_uGpuTimingWriteIndex + 1u) % kGpuTimingSlots;
        }
    }

    // SyncInterval: 1 = VSync, 0 = 즉시 표시
    HRESULT hr = m_pSwapChain->Present(m_bVSync ? 1 : 0, 0);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
        OutputDebugStringA("[CDX11Device] DEVICE REMOVED/RESET — 드라이버 재설치 후 재실행 필요\n");
    }
    else if (FAILED(hr))
    {
        OutputDebugStringA("[CDX11Device] Present FAILED — HRESULT 오류\n");
    }

    if (m_bGpuTimingReady)
        ReadGpuTimingResults();
}
```

`unique_ptr<CDX11Device> CDX11Device::Create(const DeviceDesc& desc)` 바로 위에 추가:

기존 코드:

```cpp
unique_ptr<CDX11Device> CDX11Device::Create(const DeviceDesc& desc)
```

위에 추가:

```cpp
bool CDX11Device::CreateGpuTimingQueries()
{
    D3D11_QUERY_DESC disjointDesc{ D3D11_QUERY_TIMESTAMP_DISJOINT, 0 };
    D3D11_QUERY_DESC stampDesc{ D3D11_QUERY_TIMESTAMP, 0 };

    for (GpuTimingSlot& slot : m_GpuTimingSlots)
    {
        if (FAILED(m_pDevice->CreateQuery(&disjointDesc, slot.pDisjoint.GetAddressOf())) ||
            FAILED(m_pDevice->CreateQuery(&stampDesc, slot.pBegin.GetAddressOf())) ||
            FAILED(m_pDevice->CreateQuery(&stampDesc, slot.pEnd.GetAddressOf())))
        {
            OutputDebugStringA("[CDX11Device] GPU timing query creation failed\n");
            return false;
        }
    }
    return true;
}

void CDX11Device::ReadGpuTimingResults()
{
    for (GpuTimingSlot& slot : m_GpuTimingSlots)
    {
        if (!slot.bPending)
            continue;

        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint{};
        if (m_pContext->GetData(slot.pDisjoint.Get(), &disjoint, sizeof(disjoint),
                D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
            continue;

        UINT64 beginTicks = 0;
        UINT64 endTicks = 0;
        if (m_pContext->GetData(slot.pBegin.Get(), &beginTicks, sizeof(beginTicks),
                D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK ||
            m_pContext->GetData(slot.pEnd.Get(), &endTicks, sizeof(endTicks),
                D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
            continue;

        slot.bPending = false;

        if (disjoint.Disjoint || disjoint.Frequency == 0 || endTicks <= beginTicks)
            continue;

        const uint64_t gpuUs = (endTicks - beginTicks) * 1000000ull / disjoint.Frequency;
        WINTERS_PROFILE_COUNT("GPU::FrameUs", gpuUs);
    }
}
```

1-4. C:/Users/tnest/Desktop/Winters/Engine/Private/Framework/CEngineApp.cpp

anonymous namespace 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    bool_t ShouldUsePresentationVSync(const EngineConfig& config)
    {
        return config.vsync && config.targetFPS == 0u;
    }
```

아래에 추가:

```cpp
    void BuildProfilerCapturePath(char* pOut, size_t sizeBytes)
    {
        CreateDirectoryW(L"Profiles", nullptr);

        SYSTEMTIME st{};
        GetLocalTime(&st);
        sprintf_s(pOut, sizeBytes, "Profiles/profiler_%04u%02u%02u_%02u%02u%02u.json",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    }
```

`int32 CEngineApp::Run()` 안에서 아래 코드를:

```cpp
    const bool_t bLimitFrameRate = m_uTargetFPS > 0u;
```

아래로 교체:

```cpp
    bool_t bLimitFrameRate = m_uTargetFPS > 0u;
```

`int32 CEngineApp::Run()` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
                if (CInput::Get().IsKeyPressed(VK_F4))
                {
                    bSaveProfilerJson = true;
                    bOpenProfilerOverlayAfterSave = true;
                }
```

아래에 추가:

```cpp
                // F11: 캡 해제 측정용 프레임 리미터 토글 (targetFPS 설정이 있을 때만 의미)
                if (m_uTargetFPS > 0u && CInput::Get().IsKeyPressed(VK_F11))
                    bLimitFrameRate = !bLimitFrameRate;
```

`int32 CEngineApp::Run()` 안에서 아래 코드를:

```cpp
        if (bSaveProfilerJson)
            CGameInstance::Get()->Profiler_SaveJson("profiler.json");
```

아래로 교체:

```cpp
        if (bSaveProfilerJson)
        {
            char capturePath[96] = {};
            BuildProfilerCapturePath(capturePath, sizeof(capturePath));
            CGameInstance::Get()->Profiler_SaveJson(capturePath);
            CGameInstance::Get()->Profiler_SaveJson("profiler.json");
        }
```

2. 검증

미검증:
- 빌드 미검증 (작성 시점)
- 런타임에서 GPU::FrameUs 카운터가 합리적 값(현재 씬 기준 1000us 미만 예상)인지 미검증
- F11 토글 시 Frame::LimiterActive 카운터가 0/1로 바뀌고 FPS가 풀리는지 미검증

검증 명령:
- git diff --check
- msbuild Engine 프로젝트 Debug x64

수동 확인:
- F4 캡처 후 `Profiles/profiler_*.json`과 루트 `profiler.json`이 모두 생성되는지 확인.
- 캡처 JSON에 `truncatedRawEvents` 등 플래그가 기록되는지 확인.
- 캡처 JSON `counters`에 `GPU::FrameUs`가 있는지, CPU `Frame`보다 훨씬 작은지 확인 (CPU 바운드 가설 검증).
- F11로 리미터 해제 후 캡 해제 베이스라인 캡처를 1개 보관 (Phase 1 게이트 기준).
- rawEvents에 메인 외 threadId(워커) 이벤트가 계속 기록되는지 확인 (S5).

후속 동기화:
- Engine public header 변경 없음 (CDX11Device.h는 Engine/Private) → UpdateLib.bat 불필요.
