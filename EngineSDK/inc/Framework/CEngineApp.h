#pragma once

#include "WintersTypes.h"
#include "EngineConfig.h"
#include "Platform/CWin32Window.h"
#include "Resource/ResourceCache.h"
#include "Editor/ImGuiLayer.h"

class IWintersApp;
class IRHIDevice;
class CBlendStateCache;
class DX11Pipeline;
class DX11Shader;

class CEngineApp
{
public:
    CEngineApp();
    ~CEngineApp();

    bool Initialize(IWintersApp* pGameApp, const EngineConfig& config);
    int32 Run();
    void Shutdown();

    static CEngineApp& Get() { return *m_spInstance; }
    IRHIDevice& GetDevice();
    CWin32Window& GetWindow() { return m_Window; }
    CResourceCache& GetResourceCache() { return m_ResourceCache; }

    DX11Shader* GetMeshShader() { return m_pMeshShader.get(); }
    DX11Pipeline* GetMeshPipeline() { return m_pMeshPipeline.get(); }
    DX11Shader* GetSkinnedShader() { return m_pSkinnedShader.get(); }
    DX11Pipeline* GetSkinnedPipeline() { return m_pSkinnedPipeline.get(); }
    DX11Shader* GetMeshPBRShader() { return m_pMeshPBRShader.get(); }
    DX11Pipeline* GetMeshPBRPipeline() { return m_pMeshPBRPipeline.get(); }
    DX11Shader* GetSkinnedPBRShader() { return m_pSkinnedPBRShader.get(); }
    DX11Pipeline* GetSkinnedPBRPipeline() { return m_pSkinnedPBRPipeline.get(); }
    DX11Shader* GetFxSpriteShader() { return m_pFxSpriteShader.get(); }
    DX11Pipeline* GetFxSpritePipeline() { return m_pFxSpritePipeline.get(); }
    DX11Shader* GetFxMeshShader() { return m_pFxMeshShader.get(); }
    DX11Pipeline* GetFxMeshPipeline() { return m_pFxMeshPipeline.get(); }
    DX11Shader* GetUIPlaneShader() { return m_pUIPlaneShader.get(); }
    DX11Pipeline* GetUIPlanePipeline() { return m_pUIPlanePipeline.get(); }
    DX11Shader* GetContactShadowShader() { return m_pContactShadowShader.get(); }
    DX11Pipeline* GetContactShadowPipeline() { return m_pContactShadowPipeline.get(); }
    CBlendStateCache* GetBlendStateCache() { return m_pBlendStateCache.get(); }

private:
    void Update(f32_t deltaTime);
    void Render();
    void DebugRender();

    bool InitializeSharedShaders();
    void ReleaseSharedShaders();

    IWintersApp* m_pGameApp = nullptr;
    CImGuiLayer m_ImGui = {};
    CWin32Window m_Window = {};
    unique_ptr<IRHIDevice> m_pDevice = { nullptr };
    bool m_bRunning = false;
    bool m_bSceneRuntimeEnabled = false;
    bool m_bDX11RuntimeEnabled = false;
    bool m_bImGuiRuntimeEnabled = false;
    bool m_bGameInitialized = false;
    bool m_bShutdown = false;
    bool m_bVSyncRequested = false;
    bool m_bPresentationVSync = false;
    uint32 m_uTargetFPS = 60;

    static CEngineApp* m_spInstance;

    CResourceCache m_ResourceCache{};

    unique_ptr<DX11Shader> m_pMeshShader = { nullptr };
    unique_ptr<DX11Pipeline> m_pMeshPipeline = { nullptr };
    unique_ptr<DX11Shader> m_pSkinnedShader = { nullptr };
    unique_ptr<DX11Pipeline> m_pSkinnedPipeline = { nullptr };
    unique_ptr<DX11Shader> m_pMeshPBRShader = { nullptr };
    unique_ptr<DX11Pipeline> m_pMeshPBRPipeline = { nullptr };
    unique_ptr<DX11Shader> m_pSkinnedPBRShader = { nullptr };
    unique_ptr<DX11Pipeline> m_pSkinnedPBRPipeline = { nullptr };
    unique_ptr<DX11Shader> m_pFxSpriteShader = { nullptr };
    unique_ptr<DX11Pipeline> m_pFxSpritePipeline = { nullptr };
    unique_ptr<DX11Shader> m_pFxMeshShader = { nullptr };
    unique_ptr<DX11Pipeline> m_pFxMeshPipeline = { nullptr };
    unique_ptr<DX11Shader> m_pUIPlaneShader = { nullptr };
    unique_ptr<DX11Pipeline> m_pUIPlanePipeline = { nullptr };
    unique_ptr<DX11Shader> m_pContactShadowShader = { nullptr };
    unique_ptr<DX11Pipeline> m_pContactShadowPipeline = { nullptr };
    unique_ptr<CBlendStateCache> m_pBlendStateCache = { nullptr };
};
