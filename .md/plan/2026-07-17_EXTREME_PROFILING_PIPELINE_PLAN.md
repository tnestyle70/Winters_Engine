Session - 극한 프로파일링 파이프라인 1차: Release 계측 점화 + GPU 패스 타이머/통합 드로우 통계 + 리플레이 자동 캡처 하네스 → Release 베이스라인 실측 → 상위 병목 1건 최적화 수치화 → 이력서 원장 파이프라인
좌표: 신규 좌표 후보 (측정 인프라 완성 — "계측 없는 최적화 금지"의 기계화) · 축: C8, C2
관련: .md/guide/PROFILING_NARROWING_PLAYBOOK.md(같은 세션 신규), .md/plan/performance/2026-06-12_ENGINE_FULL_OPTIMIZATION_MASTER_PLAN.md, .md/plan/2026-06-04_100FPS_BOTTLENECK_SESSIONS_PLAN.md, .md/plan/2026-07-17_S040_SESSION_HANDOVER.md

## 1. 결정 기록

① 문제·제약: 이력서에 낼 성능 수치가 필요한데 현존 수치 전량(17.8→9ms, 9.54ms/94드로콜)이 /Od Debug 캡처다 — `WINTERS_PROFILING`이 Debug 3곳에만 정의(Engine.vcxproj:54, Client.vcxproj:53, EldenRingClient:53)라 Release는 계측 0, GPU 쿼리 발행(`CDX11Device.cpp:979/1004`의 `#ifdef`)도 0. GPU 분해는 프레임 전체 1쌍뿐, 드로우콜 총계는 부분 카운터 3종(Mesh/Model/RHI)이 합산 불가, 캡처는 수동 F12뿐이라 무인 before/after 비교가 불가능하다.
② 순진한 해법의 실패: (a) Debug 수치 그대로 사용 → /Od에서는 병목 순위 자체가 왜곡되고 면접에서 "Release는?"에 반증된다(`.md/이력서/면접/12_PERFORMANCE_MEASUREMENT.md:198`). (b) VTune/RenderDoc만 외부 부착 → 전 트리 GPU 마커 0개(ID3DUserDefinedAnnotation 부재 — rg 0건)라 무라벨 캡처, 고정 시나리오 부재로 수치 비교 불능.
③ 메커니즘: Release에 `WINTERS_PROFILING` 정의(기존 계측·Tracy·GPU 쿼리 전부가 이 게이트 뒤라 vcxproj 2줄로 점화) + 디바이스 깔때기 2곳·원시 5곳의 relaxed-atomic 프레임 통계(`Draw::Total`) + 기존 GpuTimingSlot 링 확장 패스 타임스탬프·파이프라인 통계·주석 마커 + `--replay=`/`--run-seconds=`/`--profile-capture-on-exit` 자동 캡처 → `analyze_profiler_capture.py` 게이트 → 수치 원장.
④ 대조: UE는 Development 구성 + stat unit/GPU + Insights + RDG 패스 마커가 기본. Winters는 Tracy가 이미 벤더링(TracyD3D11.hpp 미사용)이므로 자체 링 확장이 최소 경로 — Tracy D3D11 존 도입은 캡처 워크플로가 커서 후속. 드로우 카운트를 프로파일러 `AddCounter`로 직접 치지 않는 이유: 호출당 전역 mutex+O(n) strcmp(`CPUProfiler.cpp:367-404`)라 드로우 밀도 계측이 실측을 오염시킨다 — atomic 집계 후 프레임당 1회 게이지 방출.
⑤ 대가: Release에 계측 상시 포함 → 스코프 155곳의 mutex 비용이 실측에 섞인다(자기 오버헤드 게이트 1% 초과 시 캡처 무효로 방어). 외부 배포 빌드가 필요해지는 순간 이 선택은 틀리게 되고 Shipping 구성 분리가 필요하다. 패스 타이머는 기존 슬롯의 "pending이면 그 프레임 무측정" 특성을 물려받고, 프레임당 패스 16개 초과분은 조용히 미계측(초과 시 uPassCount 고정)이다.

## 2. 반영해야 하는 코드

### 2-1. C:/Users/user/Desktop/Winters/Engine/Include/Engine.vcxproj

Release ClCompile의 PreprocessorDefinitions (line 83 부근).

기존 코드:

```text
<PreprocessorDefinitions>WINTERS_ENGINE_EXPORTS;IMGUI_API=__declspec(dllexport);WIN32;NDEBUG;_WINDOWS;_USRDLL;%(PreprocessorDefinitions)</PreprocessorDefinitions>
```

아래로 교체:

```text
<PreprocessorDefinitions>WINTERS_ENGINE_EXPORTS;IMGUI_API=__declspec(dllexport);WIN32;NDEBUG;_WINDOWS;_USRDLL;WINTERS_PROFILING;%(PreprocessorDefinitions)</PreprocessorDefinitions>
```

### 2-2. C:/Users/user/Desktop/Winters/Client/Include/Client.vcxproj

Release ClCompile의 PreprocessorDefinitions (line 86).

기존 코드:

```text
<PreprocessorDefinitions>IMGUI_API=__declspec(dllimport);WIN32;NDEBUG;_WINDOWS;NOMINMAX;%(PreprocessorDefinitions)</PreprocessorDefinitions>
```

아래로 교체:

```text
<PreprocessorDefinitions>IMGUI_API=__declspec(dllimport);WIN32;NDEBUG;_WINDOWS;NOMINMAX;WINTERS_PROFILING;%(PreprocessorDefinitions)</PreprocessorDefinitions>
```

### 2-3. 새 파일 C:/Users/user/Desktop/Winters/Engine/Private/Core/Profiler/RenderFrameStats.h

Engine DLL 내부 전용(Private 헤더 — Client에서 include 금지, DLL 경계 밖으로 나가면 인스턴스가 갈라진다).

```cpp
#pragma once
#include "WintersTypes.h"
#include <atomic>

// 디바이스 드로우 깔때기(DX11Buffer/CDX11FrameCommandList)와 원시 드로우 사이트,
// 상태 바인드 사이트가 공유하는 프레임 통계.
// 드로우마다 프로파일러 AddCounter(전역 mutex + O(n) strcmp)를 잡지 않도록
// relaxed atomic 으로 모으고, CDX11Device::EndFrame 이 프레임당 1회 게이지로 방출·리셋한다.
namespace RenderFrameStats
{
    inline std::atomic<uint64_t> s_uDrawCalls{ 0 };
    inline std::atomic<uint64_t> s_uDrawIndices{ 0 };
    inline std::atomic<uint64_t> s_uBindShader{ 0 };
    inline std::atomic<uint64_t> s_uBindPipeline{ 0 };
    inline std::atomic<uint64_t> s_uBindTexture{ 0 };
    inline std::atomic<uint64_t> s_uBindBlend{ 0 };

    inline void AddDraw(uint64_t indexOrVertexCount)
    {
        s_uDrawCalls.fetch_add(1u, std::memory_order_relaxed);
        s_uDrawIndices.fetch_add(indexOrVertexCount, std::memory_order_relaxed);
    }
    inline void AddBindShader()   { s_uBindShader.fetch_add(1u, std::memory_order_relaxed); }
    inline void AddBindPipeline() { s_uBindPipeline.fetch_add(1u, std::memory_order_relaxed); }
    inline void AddBindTexture()  { s_uBindTexture.fetch_add(1u, std::memory_order_relaxed); }
    inline void AddBindBlend()    { s_uBindBlend.fetch_add(1u, std::memory_order_relaxed); }
}
```

### 2-4. C:/Users/user/Desktop/Winters/Engine/Private/RHI/DX11/DX11Buffer.cpp

파일 상단 include 블록에 `#include "Core/Profiler/RenderFrameStats.h"` 추가.

기존 코드:

```cpp
void DX11Buffer::DrawIndexed(ID3D11DeviceContext* context) const
{
    assert(context);
    if (m_pIndexBuffer)
        context->DrawIndexed(m_IndexCount, 0, 0);
    else
        context->Draw(m_VertexCount, 0);
}

void DX11Buffer::DrawIndexedRange(
    ID3D11DeviceContext* context,
    uint32_t startIndex,
    uint32_t indexCount) const
{
    assert(context);
    if (indexCount == 0)
        return;

    if (m_pIndexBuffer)
        context->DrawIndexed(indexCount, startIndex, 0);
    else
        context->Draw(indexCount, startIndex);
}
```

아래로 교체:

```cpp
void DX11Buffer::DrawIndexed(ID3D11DeviceContext* context) const
{
    assert(context);
    RenderFrameStats::AddDraw(m_pIndexBuffer ? m_IndexCount : m_VertexCount);
    if (m_pIndexBuffer)
        context->DrawIndexed(m_IndexCount, 0, 0);
    else
        context->Draw(m_VertexCount, 0);
}

void DX11Buffer::DrawIndexedRange(
    ID3D11DeviceContext* context,
    uint32_t startIndex,
    uint32_t indexCount) const
{
    assert(context);
    if (indexCount == 0)
        return;

    RenderFrameStats::AddDraw(indexCount);
    if (m_pIndexBuffer)
        context->DrawIndexed(indexCount, startIndex, 0);
    else
        context->Draw(indexCount, startIndex);
}
```

### 2-5. C:/Users/user/Desktop/Winters/Engine/Public/RHI/IRHIDevice.h

기존 코드:

```cpp
    virtual void BeginFrame(f32_t r = 0.0f, f32_t g = 0.0f, f32_t b = 0.0f, f32_t a = 1.0f) = 0;
    virtual void EndFrame() = 0;
```

아래에 추가:

```cpp
    // GPU 패스 스코프: 주석 마커(RenderDoc/PIX 라벨) + WINTERS_PROFILING 시 패스별 타임스탬프.
    // pName 은 static storage 리터럴이어야 하며(게이지 이름으로 그대로 저장) 중첩은 지원하지 않는다.
    virtual void BeginGpuPass(const char* pName) { (void)pName; }
    virtual void EndGpuPass() {}
```

후속 동기화: UpdateLib.bat 실행 필요 (Engine public header 변경).

### 2-6. C:/Users/user/Desktop/Winters/Engine/Private/RHI/DX11/CDX11Device.h

`GpuTimingSlot` 블록(line 141-155)을 패스 타임스탬프·파이프라인 통계 확장으로 교체하고, 클래스 public 영역에 `BeginGpuPass/EndGpuPass` override 선언, 멤버에 `ID3DUserDefinedAnnotation` 추가. (`<d3d11_1.h>` include 필요.)

기존 코드:

```cpp
    // GPU 프레임 타임스탬프: disjoint+begin/end 쿼리를 N슬롯 링으로 두고
    // 수 프레임 지연 후 non-blocking readback 한다 (GPU::FrameUs 카운터).
    static constexpr uint32 kGpuTimingSlots = 4u;
    struct GpuTimingSlot
    {
        Microsoft::WRL::ComPtr<ID3D11Query> pDisjoint;
        Microsoft::WRL::ComPtr<ID3D11Query> pBegin;
        Microsoft::WRL::ComPtr<ID3D11Query> pEnd;
        uint64_t uSourceRHIFrame = 0;
        bool bPending = false;
    };
    GpuTimingSlot   m_GpuTimingSlots[kGpuTimingSlots];
    uint32          m_uGpuTimingWriteIndex = 0;
    uint64_t        m_uGpuTimingFrameSerial = 0;
    bool            m_bGpuTimingReady = false;
```

아래로 교체:

```cpp
    // GPU 프레임 타임스탬프: disjoint+begin/end 쿼리를 N슬롯 링으로 두고
    // 수 프레임 지연 후 non-blocking readback 한다 (GPU::FrameUs 카운터).
    // 같은 disjoint 아래에 패스별 begin/end 쌍(BeginGpuPass)과
    // 파이프라인 통계 쿼리를 함께 실어 패스 분해/프리미티브 카운트를 얻는다.
    static constexpr uint32 kGpuTimingSlots = 4u;
    static constexpr uint32 kMaxGpuPassesPerFrame = 16u;
    struct GpuTimingSlot
    {
        Microsoft::WRL::ComPtr<ID3D11Query> pDisjoint;
        Microsoft::WRL::ComPtr<ID3D11Query> pBegin;
        Microsoft::WRL::ComPtr<ID3D11Query> pEnd;
        Microsoft::WRL::ComPtr<ID3D11Query> pPipelineStats;
        Microsoft::WRL::ComPtr<ID3D11Query> pPassBegin[kMaxGpuPassesPerFrame];
        Microsoft::WRL::ComPtr<ID3D11Query> pPassEnd[kMaxGpuPassesPerFrame];
        const char* passNames[kMaxGpuPassesPerFrame] = {};
        uint32 uPassCount = 0;
        uint64_t uSourceRHIFrame = 0;
        bool bPending = false;
    };
    GpuTimingSlot   m_GpuTimingSlots[kGpuTimingSlots];
    uint32          m_uGpuTimingWriteIndex = 0;
    uint64_t        m_uGpuTimingFrameSerial = 0;
    bool            m_bGpuTimingReady = false;
    bool            m_bGpuFrameTimingActive = false;
    bool            m_bGpuPassOpen = false;
    Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> m_pAnnotation;
```

public 영역 `BeginFrame/EndFrame` override 선언 아래에 추가:

```cpp
    void BeginGpuPass(const char* pName) override;
    void EndGpuPass() override;
```

### 2-7. C:/Users/user/Desktop/Winters/Engine/Private/RHI/DX11/CDX11Device.cpp

(a) include 블록에 `#include "Core/Profiler/RenderFrameStats.h"` 추가. Initialize에서 annotation 획득: `m_pContext.As(&m_pAnnotation);` (실패 무해 — null 유지).

(b) `CreateGpuTimingQueries`: 슬롯 루프 안에서 기존 3쿼리 생성 뒤 pipeline stats + 패스 쌍 생성 추가:

```cpp
    D3D11_QUERY_DESC statsDesc{ D3D11_QUERY_PIPELINE_STATISTICS, 0 };
    // ... 슬롯 루프 내부, 기존 disjoint/begin/end 생성 성공 뒤:
        if (FAILED(m_pDevice->CreateQuery(&statsDesc, slot.pPipelineStats.GetAddressOf())))
            return false;
        for (uint32 i = 0; i < kMaxGpuPassesPerFrame; ++i)
        {
            if (FAILED(m_pDevice->CreateQuery(&stampDesc, slot.pPassBegin[i].GetAddressOf())) ||
                FAILED(m_pDevice->CreateQuery(&stampDesc, slot.pPassEnd[i].GetAddressOf())))
                return false;
        }
```

(c) `BeginFrame`의 WINTERS_PROFILING 블록: 발행 성공 경로에 `slot.uPassCount = 0; m_bGpuFrameTimingActive = true;` + `m_pContext->Begin(slot.pPipelineStats.Get());`, 스킵 경로(pending)에는 `m_bGpuFrameTimingActive = false;`.

(d) `EndFrame`의 WINTERS_PROFILING 블록: `m_pContext->End(slot.pPipelineStats.Get());`를 End(pEnd)와 End(pDisjoint) 사이에 추가, 블록 끝에 `m_bGpuFrameTimingActive = false;`. Present 직전에 프레임 통계 방출:

```cpp
    WINTERS_PROFILE_GAUGE("Draw::Total", RenderFrameStats::s_uDrawCalls.exchange(0, std::memory_order_relaxed));
    WINTERS_PROFILE_GAUGE("Draw::Indices", RenderFrameStats::s_uDrawIndices.exchange(0, std::memory_order_relaxed));
    WINTERS_PROFILE_GAUGE("Bind::Shader", RenderFrameStats::s_uBindShader.exchange(0, std::memory_order_relaxed));
    WINTERS_PROFILE_GAUGE("Bind::Pipeline", RenderFrameStats::s_uBindPipeline.exchange(0, std::memory_order_relaxed));
    WINTERS_PROFILE_GAUGE("Bind::Texture", RenderFrameStats::s_uBindTexture.exchange(0, std::memory_order_relaxed));
    WINTERS_PROFILE_GAUGE("Bind::Blend", RenderFrameStats::s_uBindBlend.exchange(0, std::memory_order_relaxed));
```

(e) `ReadGpuTimingResults`: 프레임 Us 게이지 방출 뒤(WINTERS_PROFILE_GAUGE 2줄 아래), 같은 disjoint.Frequency로 패스·통계 readback 추가:

```cpp
        for (uint32 i = 0; i < slot.uPassCount; ++i)
        {
            UINT64 passBegin = 0, passEnd = 0;
            if (m_pContext->GetData(slot.pPassBegin[i].Get(), &passBegin, sizeof(passBegin),
                    D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK ||
                m_pContext->GetData(slot.pPassEnd[i].Get(), &passEnd, sizeof(passEnd),
                    D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
                continue;
            if (slot.passNames[i] && passEnd > passBegin)
                WINTERS_PROFILE_GAUGE(slot.passNames[i],
                    (passEnd - passBegin) * 1000000ull / disjoint.Frequency);
        }
        D3D11_QUERY_DATA_PIPELINE_STATISTICS stats{};
        if (m_pContext->GetData(slot.pPipelineStats.Get(), &stats, sizeof(stats),
                D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK)
        {
            WINTERS_PROFILE_GAUGE("GPU::IAPrimitives", stats.IAPrimitives);
            WINTERS_PROFILE_GAUGE("GPU::VSInvocations", stats.VSInvocations);
            WINTERS_PROFILE_GAUGE("GPU::PSInvocations", stats.PSInvocations);
            WINTERS_PROFILE_GAUGE("GPU::CSInvocations", stats.CSInvocations);
        }
```

(f) 새 멤버 함수 `BeginGpuPass/EndGpuPass` 구현 (`ReadGpuTimingResults` 아래에 추가):

```cpp
void CDX11Device::BeginGpuPass(const char* pName)
{
    if (m_pAnnotation && pName)
    {
        wchar_t wide[64];
        size_t i = 0;
        for (; i + 1 < _countof(wide) && pName[i]; ++i)
            wide[i] = static_cast<wchar_t>(pName[i]);
        wide[i] = L'\0';
        m_pAnnotation->BeginEvent(wide);
    }
#ifdef WINTERS_PROFILING
    if (m_bGpuFrameTimingActive && !m_bGpuPassOpen)
    {
        GpuTimingSlot& slot = m_GpuTimingSlots[m_uGpuTimingWriteIndex];
        if (slot.uPassCount < kMaxGpuPassesPerFrame)
        {
            slot.passNames[slot.uPassCount] = pName;
            m_pContext->End(slot.pPassBegin[slot.uPassCount].Get());
            m_bGpuPassOpen = true;
        }
    }
#endif
}

void CDX11Device::EndGpuPass()
{
#ifdef WINTERS_PROFILING
    if (m_bGpuFrameTimingActive && m_bGpuPassOpen)
    {
        GpuTimingSlot& slot = m_GpuTimingSlots[m_uGpuTimingWriteIndex];
        m_pContext->End(slot.pPassEnd[slot.uPassCount].Get());
        ++slot.uPassCount;
        m_bGpuPassOpen = false;
    }
#endif
    if (m_pAnnotation)
        m_pAnnotation->EndEvent();
}
```

(g) RHI 깔때기 카운트 — `CDX11FrameCommandList::Draw/DrawIndexed`:

기존 코드:

```cpp
    void Draw(u32_t vertexCount, u32_t instanceCount, u32_t firstVertex, u32_t firstInstance) override
    {
        ID3D11DeviceContext* pContext = m_Owner.GetContext();
        if (pContext)
            pContext->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
    }
```

아래로 교체:

```cpp
    void Draw(u32_t vertexCount, u32_t instanceCount, u32_t firstVertex, u32_t firstInstance) override
    {
        ID3D11DeviceContext* pContext = m_Owner.GetContext();
        if (pContext)
        {
            RenderFrameStats::AddDraw(vertexCount);
            pContext->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
        }
    }
```

`DrawIndexed`도 같은 형태로 `RenderFrameStats::AddDraw(indexCount);`를 DrawIndexedInstanced 직전에 추가.

### 2-8. Engine 원시 드로우 5사이트 + 바인드 4사이트 (함수명 앵커, 각 1줄)

각 파일 include 블록에 `#include "Core/Profiler/RenderFrameStats.h"` 추가 후:

| 파일 | 함수/위치 | 추가 |
|---|---|---|
| Engine/Private/Renderer/UIRenderer.cpp:195 | Impl::Flush의 `pContext->Draw(vertices.size(), 0)` 직전 | `RenderFrameStats::AddDraw(vertices.size());` |
| Engine/Private/Renderer/PlaneRenderer.cpp:276 | Render의 `DrawIndexed(6,0,0)` 직전 | `RenderFrameStats::AddDraw(6u);` |
| Engine/Private/Renderer/PlaneRenderer.cpp:397 | RenderBatched의 `DrawIndexed(6,0,0)` 직전 | `RenderFrameStats::AddDraw(6u);` |
| Engine/Private/Renderer/FogOfWarRenderer.cpp:430 | `DrawIndexed(6,0,0)` 직전 | `RenderFrameStats::AddDraw(6u);` |
| Engine/Private/Renderer/PostFxPass.cpp:682 | DrawFullscreen 람다의 `Draw(3,0)` 직전 | `RenderFrameStats::AddDraw(3u);` |
| Engine/Private/RHI/DX11/DX11Shader.cpp | `DX11Shader::Bind` 진입부 | `RenderFrameStats::AddBindShader();` |
| Engine/Private/RHI/DX11/DX11Pipeline.cpp | `DX11Pipeline::Bind` 진입부 | `RenderFrameStats::AddBindPipeline();` |
| Engine/Private/Resource/Texture.cpp | `CTexture::Bind` 진입부 | `RenderFrameStats::AddBindTexture();` |
| Engine/Private/RHI/DX11/BlendStateCache.cpp | `CBlendStateCache::Bind` 진입부 | `RenderFrameStats::AddBindBlend();` |

### 2-9. C:/Users/user/Desktop/Winters/Engine/Include/EngineConfig.h

기존 코드:

```cpp
    // ── 렌더링 ───────────────────────────────────────────────
    bool     vsync        = true;    // Present VSync 사용 여부
    uint32   targetFPS    = 60;      // CPU frame limiter. 0이면 제한 없음
```

아래에 추가:

```cpp
    // ── 프로파일링 하네스 ────────────────────────────────────
    uint32   runSeconds   = 0;       // 0이면 무제한. N초 경과 시 자동 종료
    bool     profileCaptureOnExit = false; // 자동 종료 직전 프로파일러 타임라인 JSON 캡처
```

후속 동기화: UpdateLib.bat 실행 필요.

### 2-10. C:/Users/user/Desktop/Winters/Engine/Private/Framework/CEngineApp.cpp (+ CEngineApp.h)

CEngineApp.h 멤버에 `uint32 m_uRunSeconds = 0;` `bool_t m_bProfileCaptureOnExit = false;` 추가, config 소비 지점(Initialize)에서 대입. Run 루프의 RunElapsedUs 게이지 직후에 추가:

기존 코드:

```cpp
        WINTERS_PROFILE_GAUGE("Frame::WallUs", wallUs);
        WINTERS_PROFILE_GAUGE("Frame::RunElapsedUs", runElapsedUs);
```

아래에 추가:

```cpp
        // 무인 프로파일링 하네스: N초 경과 시 (옵션) 타임라인 캡처 후 자동 종료.
        if (m_bRunning && m_uRunSeconds > 0u &&
            runElapsedUs >= static_cast<uint64_t>(m_uRunSeconds) * 1000000ull)
        {
            if (m_bProfileCaptureOnExit)
                bSaveProfilerJson = true;
            m_bRunning = false;
        }
```

(주의: `bSaveProfilerJson` 저장 블록이 `Profiler_End()` 뒤에 이미 있으므로 이 위치가 캡처 순서를 보존한다.)

### 2-11. C:/Users/user/Desktop/Winters/Client/Private/main.cpp

익명 namespace에 파서 추가(기존 `ParseRequestedTargetFPS` 패턴 복제):

```cpp
    uint32_t ParseRunSeconds()
    {
        const wchar_t* pValue = FindCommandLineValue(L"--run-seconds=");
        if (!pValue)
            return 0u;
        wchar_t* pEnd = nullptr;
        const unsigned long parsed = std::wcstoul(pValue, &pEnd, 10);
        return (pEnd == pValue) ? 0u : static_cast<uint32_t>(parsed);
    }
```

wWinMain의 config 채우기 블록에 추가:

```cpp
    config.runSeconds = ParseRunSeconds();
    config.profileCaptureOnExit = HasCommandLineFlag(L"--profile-capture-on-exit");
```

### 2-12. C:/Users/user/Desktop/Winters/Client/Private/GameApp.cpp

`--replay=<path>` 부트: Scene_MyInfo::OpenReplay(Scene_MyInfo.cpp:252-267)와 동일한 체인을 CLI에서 직행. include에 `Scene/Scene_MatchLoading.h`, `Scene/Scene_InGame.h` 추가 + 경로 파서(main.cpp의 quoted-value 파서 패턴).

기존 코드:

```cpp
	auto pLogin = CScene_Login::Create();

	CGameInstance::Get()->Change_Scene(
		static_cast<uint32_t>(eSceneID::Login),
		std::move(pLogin));

	return true;
```

아래로 교체:

```cpp
	// --replay=<path>: 로그인/로비를 건너뛰고 리플레이 재생으로 직행 (프로파일링 재현 시나리오).
	const std::wstring replayBootPath = ParseReplayBootPath();
	if (!replayBootPath.empty())
	{
		auto pLoadingMatch = CScene_MatchLoading::Create(
			[replayBootPath]() -> std::unique_ptr<IScene>
			{
				return std::unique_ptr<IScene>(new CScene_InGame(replayBootPath));
			}, 1.f);

		CGameInstance::Get()->Change_Scene(
			static_cast<u32_t>(eSceneID::MatchLoading),
			std::move(pLoadingMatch));

		return true;
	}

	auto pLogin = CScene_Login::Create();

	CGameInstance::Get()->Change_Scene(
		static_cast<uint32_t>(eSceneID::Login),
		std::move(pLogin));

	return true;
```

### 2-13. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameRender.cpp

`CScene_InGame::OnRender`의 기존 `WINTERS_PROFILE_SCOPE` 패스 블록 경계마다 `pDevice->BeginGpuPass("GPU::<패스>")` / `pDevice->EndGpuPass()` 쌍 삽입 (pDevice는 :349에서 이미 획득). 대상 패스(순서 고정): `GPU::NormalPass`, `GPU::SSAO`, `GPU::Map`, `GPU::Actors`(챔피언~프롭 블록 묶음), `GPU::Shadows`, `GPU::Transparent`, `GPU::FoW`, `GPU::FX`, `GPU::PostFx`. 블록 구조는 적용 시 실코드 순서를 따르고 조기 return/조건 분기에서 Begin/End 쌍이 깨지지 않게 블록 스코프 안쪽에 배치한다.

### 2-14. 새 파일 C:/Users/user/Desktop/Winters/Tools/Profiler/run_profile_session.ps1

빌드 → 리플레이 자동 캡처 → 분석 → 원장 append 원커맨드. (전문은 적용 시 작성 — 빌드 커맨드는 레포 관례인 vswhere 경유 MSBuild, Engine→Client 순차, 게이트는 `analyze_profiler_capture.py`. `확인 필요: 솔루션/프로젝트 빌드 단위와 기존 검증 스크립트의 MSBuild 호출 관례를 Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1에서 복제`)

### 2-15. 새 파일 C:/Users/user/Desktop/Winters/.md/plan/performance/PROFILING_LEDGER.md

실측 원장(스크립트가 append): 일시 | 구성 | 시나리오 | frames | median/p95/p99 ms | Draw::Total | GPU FrameUs | 게이트 | 메모.

## 3. 검증 — 예측을 먼저 쓴다

예측:
- Engine+Client Release 빌드 그린. Release 실행 시 F3 오버레이가 살아 있고(현재 Release는 죽어 있음), `GPU::FrameUs`·`Draw::Total`·`GPU::Pass::*`(정확히는 `GPU::Map` 등 리터럴 이름) 게이지가 0이 아니다.
- `Draw::Total`은 기존 부분 카운터(Mesh::DrawCalls+RHI::SceneDrawCalls+Model::CombinedDrawCalls) 합계 이상이다 (원시 5사이트 포함하므로). ~94 드로콜 씬에서 대략 90~150 범위 예상.
- `WintersGame.exe --replay=Replay/room1_tick1_871.wrpl --run-seconds=40 --profile-capture-on-exit --uncapped --no-vsync --rhi=dx11` 이 로그인 없이 인게임 재생에 진입하고, 40초 후 `profiler.json` + `Profiles/profiler_*.json`을 남기고 스스로 종료한다.
- SimLab 골든 해시 불변 (이 변경은 클라 렌더/계측 전용 — 서버·GameSim 무접촉).
- Bot AI 경계: 본 계획은 Bot AI/GameSim을 건드리지 않으며, Bot AI는 GameCommand 생산자로서 게임플레이 진실을 직접 변경하지 않는다는 계약이 유지된다.
- 깨질 수 있는 것과 게이트: Release 계측 오버헤드가 프레임을 오염 → analyze의 profiler-overhead 게이트(예산 1%)가 잡는다. 패스 Begin/End 쌍 불일치(조기 return) → 잡을 게이트 없음(육안 게이지 확인) — 그게 발견이다.

검증 명령:
- 빌드: vswhere 경유 MSBuild로 Engine.vcxproj → Client.vcxproj, Configuration=Release (schema 무관 프로젝트라 /m 무관, 관례 유지 시 /m:1)
- 실행: 위 예측의 WintersGame.exe 커맨드 (작업 디렉토리 Client/Bin/Release, Replay/*.wrpl 복사 선행)
- 분석: `python Tools/Profiler/analyze_profiler_capture.py <capture.json> --target-fps 144`
- 회귀: SimLab.exe 무인자 (골든 해시)

미검증:
- Release /O2에서의 실제 병목 순위 (이 계획의 산출물 그 자체)
- DX12 백엔드의 BeginGpuPass 기본 no-op 경로 (DX12 런타임 사용 여부 자체가 미확정)

확인 필요:
- CScene_InGame(replayPath) 부트가 로그인 계정 컨텍스트 없이 리소스 로딩까지 완주하는지 (Scene_MatchLoading 로더 의존)
- 새 파일(RenderFrameStats.h)의 빌드 프로젝트 포함 확인 (헤더 전용이라 vcxproj ClInclude 등록은 브라우징용)
- Client vcxproj Release에 WINTERS_PROFILING 추가 시 EngineSDK/inc의 ProfilerAPI.h 미러가 최신인지 (UpdateLib.bat 선행)
