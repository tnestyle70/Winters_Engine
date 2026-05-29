#include "Framework/CEngineApp.h"
#include "IWintersApp.h"
#include "Core/CInput.h"
#include "GameInstance.h"
#include "Scene/Scene_Manager.h"
#include "WintersPaths.h"
#include "ProfilerAPI.h"
#include "RHI/DX11/CDX11Device.h"
#include "RHI/DX11/DX11Shader.h"
#include "RHI/DX11/DX11Pipeline.h"
#include "RHI/DX11/BlendStateCache.h"

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

#include <chrono>
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

    std::unique_ptr<IRHIDevice> CreateDX11DeviceForWindow(CWin32Window& window, const EngineConfig& config)
    {
        DeviceDesc devDesc;
        devDesc.hwnd       = window.GetHandle();
        devDesc.width      = config.windowWidth;
        devDesc.height     = config.windowHeight;
        devDesc.vsync      = ShouldUsePresentationVSync(config);
        devDesc.fullscreen = config.fullscreen;
        return CDX11Device::Create(devDesc);
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
        return false;
    }

    const auto tryDX11 = [&]() -> bool_t
    {
        m_pDevice = CreateDX11DeviceForWindow(m_Window, config);
        if (m_pDevice)
            OutputDebugStringA("[CEngineApp] RHI backend selected: DX11\n");
        return m_pDevice != nullptr;
    };

    switch (config.rhiBackend)
    {
    case eEngineRHIBackend::DX11:
    case eEngineRHIBackend::Auto:
        tryDX11();
        break;
    default:
        OutputDebugStringA("[CEngineApp] Requested RHI backend is not implemented on this platform\n");
        break;
    }

    if (!m_pDevice && config.allowRHIFallback && config.rhiBackend != eEngineRHIBackend::DX11)
    {
        OutputDebugStringA("[CEngineApp] Falling back to DX11 legacy backend\n");
        tryDX11();
    }

    if (!m_pDevice)
    {
        MessageBoxW(m_Window.GetHandle(),
            L"RHI device initialization failed.\n"
            L"Check the selected backend and graphics driver support.",
            L"[CEngineApp] RHI initialization failed", MB_OK | MB_ICONERROR);
        return false;
#if 0
        MessageBoxW(m_Window.GetHandle(),
            L"DX11 ?붾컮?댁뒪 珥덇린?붿뿉 ?ㅽ뙣?덉뒿?덈떎.\n"
            L"洹몃옒??移대뱶媛 D3D11??吏?먰븯?붿? ?뺤씤?섏꽭??",
            L"[CEngineApp] DX11 珥덇린???ㅽ뙣", MB_OK | MB_ICONERROR);
        return false;
#endif
    }


    m_bDX11RuntimeEnabled = true;

    if(!m_ImGui.Initialize(m_Window.GetHandle(), m_pDevice.get()))
    {
        OutputDebugStringA("[CEngineApp] ImGui initialization failed\n");
        return false;
    }


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
    const bool_t bLimitFrameRate = m_uTargetFPS > 0u;
    const FrameClock::duration targetFrameDuration = bLimitFrameRate
        ? std::chrono::duration_cast<FrameClock::duration>(
            std::chrono::duration<f64_t>(1.0 / static_cast<f64_t>(m_uTargetFPS)))
        : FrameClock::duration::zero();
    const CScopedFrameTimerResolution timerResolution(bLimitFrameRate);

    while (m_bRunning)
    {
        const auto frameStart = FrameClock::now();

        CGameInstance::Get()->Profiler_Begin();
        WINTERS_PROFILE_SCOPE("Frame");

        if (CInput::Get().IsKeyPressed(VK_F3))
            CGameInstance::Get()->Profiler_Toggle();

        if (!m_Window.PumpMessages())
        {
            m_bRunning = false;
            break;
        }

        CGameInstance::Get()->Update_TimeDelta(L"Timer_Default");
        float32 deltaTime = CGameInstance::Get()->Get_TimeDelta(L"Timer_Default");

        CGameInstance::Get()->Tick_Engine();

        {
            WINTERS_PROFILE_SCOPE("Update");
            Update(deltaTime);
        }
        {
            WINTERS_PROFILE_SCOPE("Render");
            Render();
        }

        CGameInstance::Get()->Profiler_End();

        CInput::Get().EndFrame();

        if (bLimitFrameRate)
            SleepUntilFrameTarget(frameStart + targetFrameDuration);
    }

    Shutdown();
    return 0;
}

void CEngineApp::Update(f32_t deltaTime)
{
    if (!m_bDX11RuntimeEnabled)
    {
        if (m_pGameApp)
            m_pGameApp->OnUpdate(deltaTime);
        return;
    }

    auto* pSceneManager = CGameInstance::Get()->Get_SceneManager();
    pSceneManager->Update(deltaTime);
    pSceneManager->LateUpdate(deltaTime);
}

void CEngineApp::Render()
{
    m_pDevice->BeginFrame(0.08f, 0.08f, 0.12f, 1.f);

    if (m_bDX11RuntimeEnabled)
    {
        m_ImGui.BeginFrame();

        auto* pSceneManager = CGameInstance::Get()->Get_SceneManager();
        pSceneManager->Render();

        pSceneManager->ImGui();

        if (m_pGameApp)
            m_pGameApp->OnImGui();

        DebugRender();

        //Profiler Overlay(F3 Toggle)
        CGameInstance::Get()->Profiler_DrawOverlay();

        m_ImGui.EndFrame();

        CGameInstance::Get()->UI_Render_Cursor();
    }
    else if (m_pGameApp)
    {
        m_pGameApp->OnRender();
    }
    m_pDevice->EndFrame();
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

        m_ImGui.ShutDown();
    }

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
