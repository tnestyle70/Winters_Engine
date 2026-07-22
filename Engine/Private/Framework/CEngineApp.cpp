#include "Framework/CEngineApp.h"
#include "IWintersApp.h"
#include "Core/CInput.h"
#include "GameInstance.h"
#include "Scene/Scene_Manager.h"
#include "WintersPaths.h"
#include "ProfilerAPI.h"
#include "RHI/RHIBackendRegistry.h"
#include "RHI/DX11/CDX11Device.h"
#include "RHI/DX11/DX11Shader.h"
#include "RHI/DX11/DX11Pipeline.h"
#include "RHI/DX11/BlendStateCache.h"

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

#include <chrono>
#include <cstring>
#include <string>
#include <thread>
#include <timeapi.h>

#pragma comment(lib, "winmm.lib")

CEngineApp* CEngineApp::m_spInstance = nullptr;

namespace
{
    using FrameClock = std::chrono::steady_clock;

    class CScopedFrameTimerResolution final
    {
    public:
        explicit CScopedFrameTimerResolution(bool_t bEnable)
        {
            if (bEnable)
                m_bActive = timeBeginPeriod(1u) == TIMERR_NOERROR;
        }

        ~CScopedFrameTimerResolution()
        {
            if (m_bActive)
                timeEndPeriod(1u);
        }

        CScopedFrameTimerResolution(const CScopedFrameTimerResolution&) = delete;
        CScopedFrameTimerResolution& operator=(const CScopedFrameTimerResolution&) = delete;

    private:
        bool_t m_bActive = false;
    };

    void SleepUntilFrameTarget(FrameClock::time_point targetTime)
    {
        const auto yieldThreshold = std::chrono::duration_cast<FrameClock::duration>(
            std::chrono::milliseconds(1));

        while (true)
        {
            const auto now = FrameClock::now();
            if (now >= targetTime)
                return;

            const auto remaining = targetTime - now;
            if (remaining > yieldThreshold)
                std::this_thread::sleep_until(targetTime - yieldThreshold);
            else
                std::this_thread::yield();
        }
    }

    bool_t ShouldUsePresentationVSync(const EngineConfig& config)
    {
        return config.vsync && config.targetFPS == 0u;
    }

    const char* EngineRHIBackendName(eEngineRHIBackend backend)
    {
        switch (backend)
        {
        case eEngineRHIBackend::Auto: return "Auto";
        case eEngineRHIBackend::DX12: return "DX12";
        case eEngineRHIBackend::DX11: return "DX11";
        case eEngineRHIBackend::Null: return "Null";
        case eEngineRHIBackend::Vulkan: return "Vulkan";
        case eEngineRHIBackend::Metal: return "Metal";
        case eEngineRHIBackend::Xbox: return "Xbox";
        case eEngineRHIBackend::PS5: return "PS5";
        default: return "Unknown";
        }
    }

    const char* RHIBackendName(eRHIBackend backend)
    {
        switch (backend)
        {
        case eRHIBackend::DX11: return "DX11";
        case eRHIBackend::DX12: return "DX12";
        case eRHIBackend::Vulkan: return "Vulkan";
        case eRHIBackend::Metal: return "Metal";
        case eRHIBackend::Xbox: return "Xbox";
        case eRHIBackend::PS5: return "PS5";
        default: return "Unknown";
        }
    }

    const char* RHIFeatureTierName(eRHIFeatureTier tier)
    {
        switch (tier)
        {
        case eRHIFeatureTier::LegacyDX11: return "LegacyDX11";
        case eRHIFeatureTier::ExplicitDesktop: return "ExplicitDesktop";
        case eRHIFeatureTier::MobileTiled: return "MobileTiled";
        case eRHIFeatureTier::Console: return "Console";
        default: return "Unknown";
        }
    }

    std::wstring ReadProcessEnvironmentValue(const wchar_t* pName)
    {
        if (!pName)
            return {};

        const DWORD required = GetEnvironmentVariableW(pName, nullptr, 0);
        if (required == 0)
            return {};

        std::wstring value(required, L'\0');
        const DWORD written = GetEnvironmentVariableW(
            pName,
            value.data(),
            required);
        if (written == 0 || written >= required)
            return {};

        value.resize(written);
        return value;
    }

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

    bool_t WriteRHISelectionProbeReport(
        const std::wstring& path,
        eEngineRHIBackend requested,
        const char* pModule,
        const char* pSelected,
        const char* pStatus,
        const char* pReason,
        bool_t bFallbackUsed,
        const char* pFallbackReason,
        const RHICapabilities* pCapabilities,
        const RHIBufferTransitionProbeResult* pBufferTransitionProbe,
        const RHITextureTransitionProbeResult* pTextureTransitionProbe)
    {
        if (path.empty() || !pModule || !pSelected || !pStatus ||
            !pReason || !pFallbackReason)
        {
            return false;
        }

        const std::wstring sourceValue = ReadProcessEnvironmentValue(
            L"WINTERS_INTERNAL_RHI_REQUEST_SOURCE");
        const char* pSource = "unknown";
        if (_wcsicmp(sourceValue.c_str(), L"command-line") == 0)
            pSource = "command-line";
        else if (_wcsicmp(sourceValue.c_str(), L"environment") == 0)
            pSource = "environment";
        else if (_wcsicmp(sourceValue.c_str(), L"default") == 0)
            pSource = "default";

        const char* pCapabilityBackend = pCapabilities
            ? RHIBackendName(pCapabilities->backend)
            : "None";
        const char* pFeatureTier = pCapabilities
            ? RHIFeatureTierName(pCapabilities->featureTier)
            : "None";
        const char* pCompute = pCapabilities && pCapabilities->supportsCompute ? "yes" : "no";
        const char* pAsyncCompute =
            pCapabilities && pCapabilities->supportsAsyncCompute ? "yes" : "no";
        const char* pBindless = pCapabilities && pCapabilities->supportsBindless ? "yes" : "no";
        const char* pResourceTransitions =
            pCapabilities && pCapabilities->supportsResourceTransitions ? "yes" : "no";
        const char* pApiRequiresExplicitStates =
            pCapabilities && pCapabilities->apiRequiresExplicitResourceStates ? "yes" : "no";
        const u32_t frameResourceSlots =
            pCapabilities ? pCapabilities->frameResourceSlotCount : 0u;

        const char* pBufferTransitionStatus = pBufferTransitionProbe
            ? pBufferTransitionProbe->pStatus
            : "not_run";
        const u64_t bufferTransitionBarriers = pBufferTransitionProbe
            ? pBufferTransitionProbe->emittedBarrierCount
            : 0;
        const char* pBufferTransitionValidation = pBufferTransitionProbe
            ? pBufferTransitionProbe->pValidation
            : "not_run";

        const char* pTextureTransitionStatus = pTextureTransitionProbe
            ? pTextureTransitionProbe->pStatus
            : "not_run";
        const u64_t textureTransitionBarriers = pTextureTransitionProbe
            ? pTextureTransitionProbe->emittedBarrierCount
            : 0;
        const char* pTextureTransitionValidation = pTextureTransitionProbe
            ? pTextureTransitionProbe->pValidation
            : "not_run";

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

        HANDLE file = CreateFileW(
            path.c_str(),
            GENERIC_WRITE,
            FILE_SHARE_READ,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return false;

        DWORD bytesWritten = 0;
        const BOOL written = WriteFile(
            file,
            report,
            static_cast<DWORD>(std::strlen(report)),
            &bytesWritten,
            nullptr);
        CloseHandle(file);
        return written && bytesWritten == std::strlen(report);
    }

    void BuildProfilerCapturePath(char* pOut, size_t sizeBytes)
    {
        CreateDirectoryW(L"Profiles", nullptr);

        SYSTEMTIME st{};
        GetLocalTime(&st);
        sprintf_s(pOut, sizeBytes,
            "Profiles/profiler_%04u%02u%02u_%02u%02u%02u_%03u.json",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
            st.wMilliseconds);
    }

}

CEngineApp::CEngineApp() = default;

CEngineApp::~CEngineApp()
{
    Shutdown();
}

IRHIDevice& CEngineApp::GetDevice()
{
    return *m_pDevice;
}

bool CEngineApp::Initialize(IWintersApp* pGameApp, const EngineConfig& config)
{
    m_spInstance = this;
    m_pGameApp  = pGameApp;
    m_uTargetFPS = config.targetFPS;
    m_bVSyncRequested = config.vsync;
    m_bPresentationVSync = ShouldUsePresentationVSync(config);
    m_uRunSeconds = config.runSeconds;
    m_bProfileCaptureOnExit = config.profileCaptureOnExit;

    const std::wstring rhiProbeReportPath = ReadProcessEnvironmentValue(
        L"WINTERS_RHI_PROBE_PATH");
    const bool_t bRHIProbeOnly = !rhiProbeReportPath.empty();

    char requestedTrace[160]{};
    sprintf_s(
        requestedTrace,
        "[CEngineApp] RHI requested=%s fallback=%s\n",
        EngineRHIBackendName(config.rhiBackend),
        config.allowRHIFallback ? "allowed" : "disabled");
    OutputDebugStringA(requestedTrace);

    if (config.vsync && m_uTargetFPS > 0u)
    {
        OutputDebugStringA("[CEngineApp] targetFPS cap active; RHI Present VSync disabled\n");
    }

    WindowDesc wndDesc;
    wndDesc.title      = config.windowTitle;
    wndDesc.width      = static_cast<int32>(config.windowWidth);
    wndDesc.height     = static_cast<int32>(config.windowHeight);
    wndDesc.fullscreen = config.fullscreen;

    if (!m_Window.Create(wndDesc))
    {
        OutputDebugStringA("[CEngineApp] Window creation failed\n");
        WriteRHISelectionProbeReport(
            rhiProbeReportPath,
            config.rhiBackend,
            "None",
            "None",
            "window_creation_failed",
            "window_creation_failed",
            false,
            "none",
            nullptr,
            nullptr,
            nullptr);
        return false;
    }

    RHIBackendCreateDesc rhiCreateDesc{};
    rhiCreateDesc.nativeWindow = m_Window.GetHandle();
    rhiCreateDesc.width = config.windowWidth;
    rhiCreateDesc.height = config.windowHeight;
    rhiCreateDesc.vsync = ShouldUsePresentationVSync(config);
    rhiCreateDesc.fullscreen = config.fullscreen;

    const eEngineRHIBackend primaryBackend =
        config.rhiBackend == eEngineRHIBackend::Auto
        ? eEngineRHIBackend::DX11
        : config.rhiBackend;

    RHIBackendCreateResult createResult =
        CRHIBackendRegistry::CreateDevice(primaryBackend, rhiCreateDesc);
    bool_t bFallbackUsed = false;
    std::string fallbackReason = "none";

    if (!createResult.Succeeded() && config.allowRHIFallback &&
        primaryBackend != eEngineRHIBackend::DX11)
    {
        bFallbackUsed = true;
        fallbackReason = createResult.reason;
        OutputDebugStringA("[CEngineApp] Falling back to DX11 backend\n");
        createResult = CRHIBackendRegistry::CreateDevice(
            eEngineRHIBackend::DX11,
            rhiCreateDesc);
    }

    if (createResult.Succeeded())
    {
        m_pDevice = std::move(createResult.pDevice);

        char selectedTrace[192]{};
        sprintf_s(
            selectedTrace,
            "[CEngineApp] RHI module=%s selected=%s reason=%s\n",
            createResult.moduleName.c_str(),
            RHIBackendName(m_pDevice->GetBackend()),
            createResult.reason.c_str());
        OutputDebugStringA(selectedTrace);
    }

    if (!m_pDevice)
    {
        WriteRHISelectionProbeReport(
            rhiProbeReportPath,
            config.rhiBackend,
            createResult.moduleName.c_str(),
            "None",
            RHIBackendCreateStatusName(createResult.status),
            createResult.reason.c_str(),
            bFallbackUsed,
            fallbackReason.c_str(),
            nullptr,
            nullptr,
            nullptr);
        if (!bRHIProbeOnly)
        {
            MessageBoxW(m_Window.GetHandle(),
                L"RHI device initialization failed.\n"
                L"Check the selected backend and graphics driver support.",
                L"[CEngineApp] RHI initialization failed", MB_OK | MB_ICONERROR);
        }
        return false;
    }

    RHIBufferTransitionProbeResult bufferTransitionProbe{};
    RHITextureTransitionProbeResult textureTransitionProbe{};
    if (bRHIProbeOnly)
    {
        bufferTransitionProbe = RunRHIBufferTransitionProbe(*m_pDevice);
        textureTransitionProbe = RunRHITextureTransitionProbe(*m_pDevice);
    }

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
        bRHIProbeOnly ? &bufferTransitionProbe : nullptr,
        bRHIProbeOnly ? &textureTransitionProbe : nullptr);
    if (bRHIProbeOnly)
    {
        m_bRunning = false;
        return std::strcmp(bufferTransitionProbe.pStatus, "pass") == 0 &&
            std::strcmp(textureTransitionProbe.pStatus, "pass") == 0;
    }

    m_bSceneRuntimeEnabled = true;
    m_bDX11RuntimeEnabled = (m_pDevice->GetBackend() == eRHIBackend::DX11);
    m_bImGuiRuntimeEnabled =
        m_bDX11RuntimeEnabled || (m_pDevice->GetBackend() == eRHIBackend::DX12);

    if (m_bImGuiRuntimeEnabled)
    {
        if (!m_ImGui.Initialize(m_Window.GetHandle(), m_pDevice.get()))
        {
            OutputDebugStringA("[CEngineApp] ImGui initialization failed\n");
            return false;
        }
    }

    if (m_bDX11RuntimeEnabled)
    {

    m_ResourceCache.Initialize(m_pDevice.get());



    if (!InitializeSharedShaders())
    {
        OutputDebugStringA("[CEngineApp] Shared shader/pipeline init FAILED\n");
        return false;
    }



    if (FAILED(CGameInstance::Get()->UI_Initialize(
        nullptr,
        m_pDevice.get(),
        static_cast<uint32_t>(m_Window.GetWidth()),
        static_cast<uint32_t>(m_Window.GetHeight()))))
    {
        OutputDebugStringA("[EngineApp] UI_Initialize FAILED\n");
        return false;
    }

    //Step 2.8 BlendStateCache  
    auto* pDX11Device = dynamic_cast<CDX11Device*>(m_pDevice.get());
    if (!pDX11Device)
        return false;

    m_pBlendStateCache = CBlendStateCache::Create(pDX11Device->GetDevice());
    if (!m_pBlendStateCache)
    {
        OutputDebugStringA("[CEngineApp] BlendStateCache ?앹꽦 ?ㅽ뙣\n");
        return false;
    }

    }
    else
    {
        OutputDebugStringA("[CEngineApp] Non-DX11 backend: skipping legacy DX11 renderer bootstrap\n");
    }

    if (FAILED(CGameInstance::Get()->Add_Timer(L"Timer_Default")))
    {
        OutputDebugStringA("[CEngineApp] Add_Timer(Timer_Default) failed\n");
        return false;
    }


    if (!m_pGameApp->OnInit())
    {
        OutputDebugStringA("[CEngineApp] Game OnInit() failed\n");
        return false;
    }
    m_bGameInitialized = true;

    m_bRunning = true;
    return true;
}

int32 CEngineApp::Run()
{
    bool_t bLimitFrameRate = m_uTargetFPS > 0u;
    const FrameClock::duration targetFrameDuration = bLimitFrameRate
        ? std::chrono::duration_cast<FrameClock::duration>(
            std::chrono::duration<f64_t>(1.0 / static_cast<f64_t>(m_uTargetFPS)))
        : FrameClock::duration::zero();
    const CScopedFrameTimerResolution timerResolution(bLimitFrameRate);

    WINTERS_PROFILE_THREAD_NAME("MainThread");
    const auto runStart = FrameClock::now();
    auto previousFrameStart = runStart;
    bool_t bHasPreviousFrameStart = false;

    while (m_bRunning)
    {
        const auto frameStart = FrameClock::now();
        const uint64_t cadenceUs = bHasPreviousFrameStart
            ? static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                frameStart - previousFrameStart).count())
            : 0u;
        previousFrameStart = frameStart;
        bHasPreviousFrameStart = true;
        bool_t bSaveProfilerJson = false;
        bool_t bOpenProfilerOverlayAfterSave = false;

        {
            CGameInstance::Get()->Profiler_Begin();
            WINTERS_PROFILE_SCOPE("Frame");
            if (cadenceUs > 0u)
                WINTERS_PROFILE_GAUGE("Frame::CadenceUs", cadenceUs);

            bool_t bWindowAlive = false;
            {
                WINTERS_PROFILE_SCOPE("Frame::PumpMessages");
                bWindowAlive = m_Window.PumpMessages();
            }

            if (!bWindowAlive)
            {
                m_bRunning = false;
            }
            else
            {
                if (CInput::Get().IsKeyPressed(VK_F3))
                    CGameInstance::Get()->Profiler_Toggle();
                // F12: 프로파일러 JSON 캡처 (F4 는 클라이언트 Structure Tuner 가 사용)
                if (CInput::Get().IsKeyPressed(VK_F12))
                {
                    bSaveProfilerJson = true;
                    bOpenProfilerOverlayAfterSave = true;
                }
                // F11: 캡 해제 측정용 프레임 리미터 토글 (targetFPS 설정이 있을 때만 의미)
                if (m_uTargetFPS > 0u && CInput::Get().IsKeyPressed(VK_F11))
                    bLimitFrameRate = !bLimitFrameRate;

                float32 deltaTime = 0.f;
                {
                    WINTERS_PROFILE_SCOPE("Frame::TimerUpdate");
                    CGameInstance::Get()->Update_TimeDelta(L"Timer_Default");
                    deltaTime = CGameInstance::Get()->Get_TimeDelta(L"Timer_Default");
                }

                {
                    WINTERS_PROFILE_SCOPE("Engine::ServiceTick");
                    CGameInstance::Get()->Tick_Engine();
                }

                {
                    WINTERS_PROFILE_SCOPE("Update");
                    Update(deltaTime);
                }
                {
                    WINTERS_PROFILE_SCOPE("Render");
                    Render();
                }

                WINTERS_PROFILE_GAUGE("Frame::LimiterActive", bLimitFrameRate ? 1u : 0u);
                WINTERS_PROFILE_GAUGE("Frame::TargetFPS", m_uTargetFPS);
                WINTERS_PROFILE_GAUGE("Frame::WindowWidth", static_cast<uint64_t>(m_Window.GetWidth()));
                WINTERS_PROFILE_GAUGE("Frame::WindowHeight", static_cast<uint64_t>(m_Window.GetHeight()));
                WINTERS_PROFILE_GAUGE("Frame::VSyncRequested", m_bVSyncRequested ? 1u : 0u);
                WINTERS_PROFILE_GAUGE("Frame::PresentationVSync", m_bPresentationVSync ? 1u : 0u);
                WINTERS_PROFILE_GAUGE("RHI::Backend", static_cast<uint64_t>(m_pDevice->GetBackend()));
            }
        }

        CInput::Get().EndFrame();

        if (m_bRunning && bLimitFrameRate)
        {
            WINTERS_PROFILE_SCOPE("Frame::LimiterWait");
            SleepUntilFrameTarget(frameStart + targetFrameDuration);
        }

        const auto frameEnd = FrameClock::now();
        const uint64_t wallUs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(frameEnd - frameStart).count());
        const uint64_t runElapsedUs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(frameEnd - runStart).count());
        WINTERS_PROFILE_GAUGE("Frame::WallUs", wallUs);
        WINTERS_PROFILE_GAUGE("Frame::RunElapsedUs", runElapsedUs);

        // 무인 프로파일링 하네스: N초 경과 시 (옵션) 타임라인 캡처 후 자동 종료.
        if (m_bRunning && m_uRunSeconds > 0u &&
            runElapsedUs >= static_cast<uint64_t>(m_uRunSeconds) * 1000000ull)
        {
            if (m_bProfileCaptureOnExit)
                bSaveProfilerJson = true;
            m_bRunning = false;
        }

        CGameInstance::Get()->Profiler_End();
        WINTERS_PROFILE_FRAME_MARK;
        if (bSaveProfilerJson)
        {
            char capturePath[96] = {};
            BuildProfilerCapturePath(capturePath, sizeof(capturePath));
            CGameInstance::Get()->Profiler_SaveJson(
                capturePath,
                true,
                "profiler.json");
        }
        if (bOpenProfilerOverlayAfterSave && !CGameInstance::Get()->Profiler_IsOverlayVisible())
            CGameInstance::Get()->Profiler_Toggle();

        if (!m_bRunning)
            break;
    }

    Shutdown();
    return 0;
}

void CEngineApp::Update(f32_t deltaTime)
{
    if (!m_bSceneRuntimeEnabled)
    {
        if (m_pGameApp)
            m_pGameApp->OnUpdate(deltaTime);
        return;
    }

    auto* pSceneManager = CGameInstance::Get()->Get_SceneManager();
    if (pSceneManager)
    {
        pSceneManager->Update(deltaTime);
        pSceneManager->LateUpdate(deltaTime);
    }
}

void CEngineApp::Render()
{
    if (!m_pDevice)
        return;

    {
        WINTERS_PROFILE_SCOPE("Render::BeginFrame");
        // 기본 클리어 색. WINTERS_CLEAR_RGB="r,g,b" 환경변수로 오버라이드 가능
        // (예: large-character 쇼케이스의 하늘색 배경). 미설정 시 기존 어두운 남색.
        static f32_t s_clear[3] = { 0.08f, 0.08f, 0.12f };
        static bool s_clearInit = false;
        if (!s_clearInit)
        {
            s_clearInit = true;
            char buf[64]{};
            size_t len = 0;
            if (getenv_s(&len, buf, sizeof(buf), "WINTERS_CLEAR_RGB") == 0 && len > 0)
            {
                float r = 0.f, g = 0.f, b = 0.f;
                if (sscanf_s(buf, "%f,%f,%f", &r, &g, &b) == 3)
                {
                    s_clear[0] = r; s_clear[1] = g; s_clear[2] = b;
                }
            }
        }
        m_pDevice->BeginFrame(s_clear[0], s_clear[1], s_clear[2], 1.f);
    }

    auto* pSceneManager = CGameInstance::Get()->Get_SceneManager();

    if (m_bDX11RuntimeEnabled)
    {
        {
            WINTERS_PROFILE_SCOPE("ImGui::BeginFrame");
            m_ImGui.BeginFrame();
        }

        if (pSceneManager)
        {
            {
                WINTERS_PROFILE_SCOPE("Scene::Render");
                pSceneManager->Render();
            }
            {
                WINTERS_PROFILE_SCOPE("Scene::ImGui");
                pSceneManager->ImGui();
            }
        }

        if (m_pGameApp)
        {
            WINTERS_PROFILE_SCOPE("App::ImGui");
            m_pGameApp->OnImGui();
        }

        {
            WINTERS_PROFILE_SCOPE("EngineDebug::Render");
            DebugRender();
        }

        // Profiler overlay: F3 toggles, F4 opens after JSON capture.
        if (CGameInstance::Get()->Profiler_IsOverlayVisible())
        {
            WINTERS_PROFILE_SCOPE("ProfilerOverlay::Render");
            CGameInstance::Get()->Profiler_DrawOverlay();
        }

        {
            WINTERS_PROFILE_SCOPE("ImGui::EndFrame");
            m_ImGui.EndFrame();
        }

        {
            WINTERS_PROFILE_SCOPE("UI::Cursor");
            CGameInstance::Get()->UI_Render_Cursor();
        }
    }
    else if (m_bSceneRuntimeEnabled)
    {
        if (m_bImGuiRuntimeEnabled)
        {
            WINTERS_PROFILE_SCOPE("ImGui::BeginFrame");
            m_ImGui.BeginFrame();
        }

        if (pSceneManager)
        {
            WINTERS_PROFILE_SCOPE("Scene::Render");
            pSceneManager->Render();
        }

        if (m_pGameApp)
        {
            WINTERS_PROFILE_SCOPE("App::Render");
            m_pGameApp->OnRender();
        }

        if (m_bImGuiRuntimeEnabled)
        {
            if (pSceneManager)
            {
                WINTERS_PROFILE_SCOPE("Scene::ImGui");
                pSceneManager->ImGui();
            }

            if (m_pGameApp)
            {
                WINTERS_PROFILE_SCOPE("App::ImGui");
                m_pGameApp->OnImGui();
            }

            {
                WINTERS_PROFILE_SCOPE("EngineDebug::Render");
                DebugRender();
            }

            {
                WINTERS_PROFILE_SCOPE("ImGui::EndFrame");
                m_ImGui.EndFrame();
            }
        }
    }
    else if (m_pGameApp)
    {
        WINTERS_PROFILE_SCOPE("App::Render");
        m_pGameApp->OnRender();
    }
    {
        WINTERS_PROFILE_SCOPE("Render::EndFrame");
        m_pDevice->EndFrame();
    }
}

void CEngineApp::DebugRender()
{
    ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowPos(ImVec2(10.f, 30.f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.35f);

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

    if (ImGui::Begin("##PerfOverlay", nullptr, flags))
    {
        ImGui::Text("Winters Engine");
        ImGui::Separator();
        ImGui::Text("FPS: %.1f  (%.3f ms)", io.Framerate,
                    io.Framerate > 0.f ? 1000.f / io.Framerate : 0.f);
        ImGui::Text("Draw: %d verts, %d indices",
                    io.MetricsRenderVertices, io.MetricsRenderIndices);
    }
    ImGui::End();
}

void CEngineApp::Shutdown()
{
    if (m_bShutdown)
        return;

    m_bShutdown = true;

    if (m_pGameApp && m_bGameInitialized)
    {
        m_pGameApp->OnShutdown();
    }

    m_pGameApp = nullptr;
    m_bGameInitialized = false;

    if (m_bDX11RuntimeEnabled)
    {
        CGameInstance::Get()->UI_Shutdown();

        m_ResourceCache.Clear();

        ReleaseSharedShaders();
    }

    if (m_bImGuiRuntimeEnabled)
        m_ImGui.ShutDown();

    m_bSceneRuntimeEnabled = false;
    m_bDX11RuntimeEnabled = false;
    m_bImGuiRuntimeEnabled = false;

    CGameInstance::Get()->Shutdown_Engine();

    m_pDevice.reset();

    m_Window.Destroy();
    m_spInstance = nullptr;
}

bool CEngineApp::InitializeSharedShaders()
{
    auto* pDX11Device = dynamic_cast<CDX11Device*>(m_pDevice.get());

    if (!pDX11Device)
        return false;

    ID3D11Device* pDev = pDX11Device->GetDevice();


    wchar_t meshPath[MAX_PATH] = {};
    if (!WintersResolveContentPath(L"Shaders/Mesh3D.hlsl", meshPath, MAX_PATH))
    {
        return false;
    }
    m_pMeshShader = unique_ptr<DX11Shader>(new DX11Shader());
    if (!m_pMeshShader->Load(pDev, meshPath)) return false;

    m_pMeshPipeline = unique_ptr<DX11Pipeline>(new DX11Pipeline());
    if (!m_pMeshPipeline->CreateMesh(pDev, m_pMeshShader->GetVSBlob())) return false;


    wchar_t skinnedPath[MAX_PATH] = {};
    if (!WintersResolveContentPath(L"Shaders/Skinned3D.hlsl", skinnedPath, MAX_PATH))
    {
        return false;
    }
    m_pSkinnedShader = unique_ptr<DX11Shader>(new DX11Shader());
    if (!m_pSkinnedShader->Load(pDev, skinnedPath)) return false;

    m_pSkinnedPipeline = unique_ptr<DX11Pipeline>(new DX11Pipeline());
    if (!m_pSkinnedPipeline->CreateSkinnedMesh(pDev, m_pSkinnedShader->GetVSBlob())) return false;

    wchar_t meshPBRPath[MAX_PATH] = {};
    if (!WintersResolveContentPath(L"Shaders/Mesh3D_PBR.hlsl", meshPBRPath, MAX_PATH))
    {
        OutputDebugStringA("[CEngineApp] Mesh3D_PBR.hlsl path resolve failed\n");
        return false;
    }
    m_pMeshPBRShader = unique_ptr<DX11Shader>(new DX11Shader());
    if (!m_pMeshPBRShader->Load(pDev, meshPBRPath)) return false;

    m_pMeshPBRPipeline = unique_ptr<DX11Pipeline>(new DX11Pipeline());
    if (!m_pMeshPBRPipeline->CreateMesh(pDev, m_pMeshPBRShader->GetVSBlob())) return false;

    wchar_t skinnedPBRPath[MAX_PATH] = {};
    if (!WintersResolveContentPath(L"Shaders/Skinned3D_PBR.hlsl", skinnedPBRPath, MAX_PATH))
    {
        OutputDebugStringA("[CEngineApp] Skinned3D_PBR.hlsl path resolve failed\n");
        return false;
    }
    m_pSkinnedPBRShader = unique_ptr<DX11Shader>(new DX11Shader());
    if (!m_pSkinnedPBRShader->Load(pDev, skinnedPBRPath)) return false;

    m_pSkinnedPBRPipeline = unique_ptr<DX11Pipeline>(new DX11Pipeline());
    if (!m_pSkinnedPBRPipeline->CreateSkinnedMesh(pDev, m_pSkinnedPBRShader->GetVSBlob())) return false;

    wchar_t fxSpritePath[MAX_PATH] = {};
    if (!WintersResolveContentPath(L"Shaders/FxSprite.hlsl", fxSpritePath, MAX_PATH))
    {
        OutputDebugStringA("[CEngineApp] FxSprite.hlsl path resolve failed\n");
        return false;
    }
    m_pFxSpriteShader = unique_ptr<DX11Shader>(new DX11Shader());
    if (!m_pFxSpriteShader->Load(pDev, fxSpritePath)) return false;

    wchar_t fxMeshPath[MAX_PATH] = {};
    if (!WintersResolveContentPath(L"Shaders/FxMesh.hlsl", fxMeshPath, MAX_PATH))
        return false;
    m_pFxMeshShader = unique_ptr<DX11Shader>(new DX11Shader());
    if (!m_pFxMeshShader->Load(pDev, fxMeshPath)) return false;
    m_pFxMeshPipeline = unique_ptr<DX11Pipeline>(new DX11Pipeline());
    if (!m_pFxMeshPipeline->CreateMesh(pDev, m_pFxMeshShader->GetVSBlob())) return false;

    m_pFxSpritePipeline = unique_ptr<DX11Pipeline>(new DX11Pipeline());
    if (!m_pFxSpritePipeline->CreateMesh(pDev, m_pFxSpriteShader->GetVSBlob())) return false;

    wchar_t uiPlanePath[MAX_PATH] = {};
    if (!WintersResolveContentPath(L"Shaders/UIPlane.hlsl", uiPlanePath, MAX_PATH))
    {
        OutputDebugStringA("[CEngineApp] UIPlane.hlsl path resolve failed\n");
        return false;
    }
    m_pUIPlaneShader = unique_ptr<DX11Shader>(new DX11Shader());
    if (!m_pUIPlaneShader->Load(pDev, uiPlanePath)) return false;

    m_pUIPlanePipeline = unique_ptr<DX11Pipeline>(new DX11Pipeline());
    if (!m_pUIPlanePipeline->CreateMesh(pDev, m_pUIPlaneShader->GetVSBlob())) return false;

    wchar_t contactShadowPath[MAX_PATH] = {};
    if (!WintersResolveContentPath(L"Shaders/ContactShadowPlane.hlsl", contactShadowPath, MAX_PATH))
    {
        OutputDebugStringA("[CEngineApp] ContactShadowPlane.hlsl path resolve failed\n");
        return false;
    }
    m_pContactShadowShader = unique_ptr<DX11Shader>(new DX11Shader());
    if (!m_pContactShadowShader->Load(pDev, contactShadowPath)) return false;

    m_pContactShadowPipeline = unique_ptr<DX11Pipeline>(new DX11Pipeline());
    if (!m_pContactShadowPipeline->CreateMesh(pDev, m_pContactShadowShader->GetVSBlob())) return false;
    OutputDebugStringA("[CEngineApp] Shared Mesh3D/Skinned3D + PBR + Fx + UIPlane + ContactShadow shaders/pipelines ready\n");

    return true;
}

void CEngineApp::ReleaseSharedShaders()
{
    m_pContactShadowPipeline.reset();
    m_pContactShadowShader.reset();
    m_pUIPlanePipeline.reset();
    m_pUIPlaneShader.reset();
    m_pFxSpritePipeline.reset();
    m_pFxSpriteShader.reset();
    m_pSkinnedPBRPipeline.reset();
    m_pSkinnedPBRShader.reset();
    m_pMeshPBRPipeline.reset();
    m_pMeshPBRShader.reset();
    m_pSkinnedPipeline.reset();
    m_pSkinnedShader.reset();
    m_pMeshPipeline.reset();
    m_pMeshShader.reset();
}
