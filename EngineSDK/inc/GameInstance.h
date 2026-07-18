#pragma once

#include "Engine_Defines.h"
#include "WintersMath.h"
#include "IScene.h"
#include "Sound/SoundChannel.h"
#include "Manager/Profiler/ProfilerOverlay.h"
#include "Core/Profiler/CPUProfiler.h"

class CWorld;
class CJobSystem;
class CProfilerOverlay;
class DX11Shader;
class DX11Pipeline;
class CBlendStateCache;
class IRHIDevice;
class CFxAssetRegistry;

NS_BEGIN(Engine)

class CScene_Manager;
struct ActorHUDAssetDesc;
struct ActorHUDState;
    struct StatusPanelActorRow;
    struct StatusPanelMatchScore;
    struct UIIconAssetDesc;
    struct UIShopItemAssetDesc;
    struct UIWorldHealthBarDesc;

class ENGINE_DLL CGameInstance
{
    DECLARE_SINGLETON(CGameInstance)

private:
    CGameInstance();

public:
    ~CGameInstance();

public:
    HRESULT Initialize_Engine(uint32_t iNumScenes);
    void Shutdown_Engine();
    void Tick_Engine();

public: // Timer
    f32_t Get_TimeDelta(const wstring_t& strTimerTag);
    HRESULT Add_Timer(const wstring_t& strTimerTag);
    void Update_TimeDelta(const wstring_t& strTimerTag);

public: // Scene
    CScene_Manager* Get_SceneManager() { return m_pScene_Manager.get(); }
    HRESULT Change_Scene(uint32_t iNextSceneID, unique_ptr<IScene> pNewScene);
    void Clear_Resources(uint32_t iPrevSceneID);

public: // Sound
    void PlaySoundOn(const wstring_t& strSoundKey, eSoundChannel eChannel, f32_t fVolume);
    void PlayEffect(const wstring_t& strSoundKey, f32_t fVolume);
    void PlayBGM(const wstring_t& strSoundKey, f32_t fVolume);
    void StopChannel(eSoundChannel eChannel);
    void StopAllSounds();
    void SetChannelVolume(eSoundChannel eChannel, f32_t fVolume);
    void SetMasterVolume(f32_t fVolume);

public: // UI
    HRESULT UI_Initialize(CWorld* pWorld, IRHIDevice* pDevice,
        uint32_t iWinSizeX, uint32_t iWinSizeY);
    void UI_Bind_World(CWorld* pWorld);
    void UI_Shutdown();
    void UI_Render_Overlay(const Mat4& matVP);
    void UI_Render_Cursor();
    void SetLoadingCursorMode(bool_t bLoading);
    bool_t UI_Begin_RawImagePass(uint32_t iScreenWidth, uint32_t iScreenHeight, bool_t bPointSample);
    void UI_Draw_RawImage(void* pTextureSRV,
        f32_t fX, f32_t fY, f32_t fW, f32_t fH,
        const Vec4& vUVRect,
        const Vec4& vColor);
    void UI_End_RawImagePass();
    void UI_Draw_RawImageCircle(void* pTextureSRV,
        f32_t fX, f32_t fY, f32_t fW, f32_t fH,
        const Vec4& vUVRect,
        const Vec4& vColor,
        u32_t iSegmentCount = 48);
    void UI_OnImGui_Tuner();
    void UI_OnImGui_StatusPanelLayoutTuner();
    void UI_Set_ActiveLuaScreen(const char* pScreenID);
    void UI_Reload_Lua();
    void UI_Toggle_InGameShop();
    void UI_Set_InGameShopOpen(bool_t bOpen);
    void UI_Toggle_StatusPanel();
    void UI_Set_StatusPanelOpen(bool_t bOpen);
    void UI_Set_ShowHealthBars(bool_t b);
    void UI_Register_ActorHUDAssets(const ActorHUDAssetDesc* pAssets, u32_t iAssetCount);
    void UI_Clear_ActorHUDAssets();
    void UI_Register_StatusPanelSpellIconAssets(const UIIconAssetDesc* pAssets, u32_t iAssetCount);
    void UI_Clear_StatusPanelSpellIconAssets();
    void UI_Set_StatusPanelDefaultSpellIds(const u16_t* pSpellIds, u32_t iSpellCount);
    void UI_Register_InGameShopItems(const UIShopItemAssetDesc* pItems, u32_t iItemCount);
    void UI_Clear_InGameShopItems();
    void UI_Set_ActorHUDState(const ActorHUDState* pState);
    void UI_Clear_ActorHUDState();
    void UI_Set_StatusPanelState(const StatusPanelMatchScore* pScore,
        const StatusPanelActorRow* pBlueRows, u32_t iBlueCount,
        const StatusPanelActorRow* pRedRows, u32_t iRedCount);
    void UI_Clear_StatusPanelState();
    void UI_Set_WorldHealthBars(const UIWorldHealthBarDesc* pBars, u32_t iBarCount, u8_t iLocalTeam);
    void UI_Clear_WorldHealthBars();
    void UI_Set_MatchContextHUDScoreStats(
        u16_t iBlueKills, u16_t iRedKills,
        u16_t iLocalKills, u16_t iLocalDeaths, u16_t iLocalAssists);
    void UI_Set_PlayerActorContent(u8_t iActorContentId);
    void UI_Set_EnemyHoverCursor(bool_t bEnemyHover);
    void UI_Set_AttackMode(bool_t bAttackMode);
    void UI_Set_PingWheel(bool_t bVisible,
        f32_t fCenterX, f32_t fCenterY,
        f32_t fMouseX, f32_t fMouseY);
    void UI_Push_MapPing(const Vec3& vWorldPos, u8_t iDirection);
    void UI_Set_InGameBuyItemCallback(void(*pfn)(void*, u16_t), void* pUser);
    void UI_Set_LevelSkillCallback(void(*pfn)(void*, u8_t), void* pUser);
    void UI_Set_InventoryReorderCallback(
        void(*pfn)(void*, u8_t, u8_t, u16_t), void* pUser);
    bool_t UI_IsPointerOverActorInventory() const;
    void UI_Push_DamageNumber(const Vec3& vWorldPos, f32_t fAmount,
        u8_t iDamageType, bool_t bWasCrit, bool_t bKilled,
        bool_t bShowCriticalIndicator = false);
    void UI_Push_WorldText(const Vec3& vWorldPos, const char* pText,
        const Vec4& vColor, f32_t fLifetime);
    void UI_Push_GoldText(const Vec3& vWorldPos, u32_t iGoldAmount,
        f32_t fLifetime);
    //Kill, Slay, Destroy Log
    void UI_Push_KillFeedBanner(u8_t iSourceActorId, u8_t iTargetActorId,
        u8_t iObjectKind, u8_t iTargetTeam,
        bool_t bSourceAlly, bool_t bSourceMinion, const char* pMessage);
    void UI_RecordMatchContextActorKill(u8_t iSourceTeam, u8_t iTargetTeam,
        bool_t bLocalSource, bool_t bLocalTarget);
    void UI_RecordMatchContextUnitKill();
    void UI_SetMatchContextServerTimeMs(u64_t iServerTimeMs);

public: // Profiler
    void Profiler_Begin();
    void Profiler_End();
    void Profiler_Toggle();
    bool_t Profiler_IsOverlayVisible() const;
    void Profiler_DrawOverlay();
    bool_t Profiler_SaveJson(
        const char* pPath,
        bool_t bForceCapture = true,
        const char* pAliasPath = nullptr);
    class CCPUProfiler* Get_CPUProfiler() { return m_pProfiler.get(); }

public: // JobSystem
    CJobSystem* Get_JobSystem() const { return m_pJobSystem.get(); }

public: // FX assets
    ::CFxAssetRegistry* Get_FxAssetRegistry() const { return m_pFxAssetRegistry.get(); }

public: // Shared engine render resources
    IRHIDevice* Get_RHIDevice();
    DX11Shader* Get_MeshShader();
    DX11Pipeline* Get_MeshPipeline();
    CBlendStateCache* Get_BlendStateCache();
    DX11Shader* Get_FxSpriteShader();
    DX11Pipeline* Get_FxSpritePipeline();
    DX11Shader* Get_FxMeshShader();
    DX11Pipeline* Get_FxMeshPipeline();
    DX11Shader* Get_UIPlaneShader();
    DX11Pipeline* Get_UIPlanePipeline();
    DX11Shader* Get_ContactShadowShader();
    DX11Pipeline* Get_ContactShadowPipeline();
    bool_t Preload_ModelResource(const char* pPath);
    bool_t Preload_TextureResource(const wchar_t* pPath);

private:
    unique_ptr<class CTimer_Manager> m_pTimer_Manager = {};
    unique_ptr<class CScene_Manager> m_pScene_Manager = {};
    unique_ptr<class CSound_Manager> m_pSound_Manager = {};
    unique_ptr<class CUI_Manager> m_pUI_Manager = {};
    unique_ptr<class CCPUProfiler> m_pProfiler = {};
    unique_ptr<class CProfilerOverlay> m_pProfilerOverlay = {};
    unique_ptr<class CJobSystem> m_pJobSystem = {};
    unique_ptr<::CFxAssetRegistry> m_pFxAssetRegistry = {};

};

NS_END
