#pragma once
#include "Engine_Defines.h"
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"
#include "GameContext.h"
#include "World.h"
#include "Entity.h"
#include "RHI/IRHIDevice.h"
#include "Manager/UI/ChampionHUDState.h"
#include "Manager/UI/Font_Manager.h"
#include "Manager/UI/UIAtlasManifest.h"
#include "Renderer/UIRenderer.h"

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

NS_BEGIN(Engine)

class CChampionHudPanel;
class CLuaUIHost;

enum class eCursorMode : uint8_t
{
    Default,
    Enemy,
    Attack,
};

class CUI_Manager final
{
public:
    ~CUI_Manager();

    // 팩토리 — CGameInstance::Initialize_Engine 에서 호출, unique_ptr 로 소유
    static unique_ptr<CUI_Manager> Create();

    HRESULT Initialize(CWorld* pWorld,
        IRHIDevice* pDevice,
        uint32_t iWinSizeX, uint32_t iWinSizeY);
    void    Shutdown();

    // Scene::OnRender 끝에서 1회 호출 — 모든 UI 오버레이 일괄 렌더
    void    Render_Overlay(const Mat4& matVP);

    void Render_Cursor();
    bool_t Begin_RawImagePass(u32_t iScreenWidth, u32_t iScreenHeight, bool_t bPointSample);
    void Draw_RawImage(void* pTextureSRV,
        f32_t fX, f32_t fY, f32_t fW, f32_t fH,
        const Vec4& vUVRect,
        const Vec4& vColor);
    void End_RawImagePass();

    void Draw_RawImageCircle(void* pTextureSRV,
        f32_t fX, f32_t fY, f32_t fW, f32_t fH,
        const Vec4& vUVRect,
        const Vec4& vColor,
        u32_t iSegmentCount = 48);

    // ImGui 튜닝 패널 (Scene::OnImGui 에서 호출)
    void    OnImGui_Tuner();

    void Bind_World(CWorld* pWorld);
    void    Set_ShowHealthBars(bool_t b) { m_bShowHealthBars = b; }
    bool_t  Get_ShowHealthBars() const { return m_bShowHealthBars; }
    void    Set_CursorMode(eCursorMode mode) { m_CursorMode = mode; }
    eCursorMode Get_CursorMode() const { return m_CursorMode; }
    void    Set_PlayerChampion(eChampion champ);
    void    Set_AttackMode(bool_t b);
    void    Set_EnemyHoverCursor(bool_t b);
    void    Set_PingWheel(bool_t bVisible,
        f32_t fCenterX, f32_t fCenterY,
        f32_t fMouseX, f32_t fMouseY);
    void    Push_MapPing(const Vec3& vWorldPos, u8_t iDirection);
    bool_t  Get_AttackMode() const { return m_bAttackMode; }
    void    Set_ShowChampionHUD(bool_t b) { m_bShowChampionHUD = b; }
    bool_t  Get_ShowChampionHUD() const { return m_bShowChampionHUD; }
    void    Set_ShowMouseCursor(bool_t b) { m_bShowMouseCursor = b; }
    bool_t  Get_ShowMouseCursor() const { return m_bShowMouseCursor; }
    void    Push_DamageNumber(const Vec3& vWorldPos, f32_t fAmount,
        u8_t iDamageType, bool_t bWasCrit, bool_t bKilled);
    void    Push_WorldText(const Vec3& vWorldPos, const char* pText,
        const Vec4& vColor, f32_t fLifetime);
    void    Push_GoldText(const Vec3& vWorldPos, u32_t iGoldAmount,
        f32_t fLifetime);
    void Push_KillFeedBanner(eChampion eSourceChampion, eChampion eTargetChampion,
        u8_t iObjectKind, bool_t bSourceAlly, const char* pMessage);
    void RecordGameContextChampionKill(u8_t iSourceTeam, u8_t iTargetTeam,
        bool_t bLocalSource, bool_t bLocalTarget);
    void RecordGameContextMinionKill();
    void SetGameContextServerTimeMs(u64_t iServerTimeMs);

    //인게임 아이템 상점
    void SetInGameShopOpen(bool_t b);
    void ToggleInGameShop();
    bool_t GetInGameShopOpen() const;
    void SetStatusPanelOpen(bool_t b);
    void ToggleStatusPanel();
    bool_t GetStatusPanelOpen() const { return m_bStatusPanelOpen; }
    //Lua Setting
    void SetActiveLuaUIScreen(const char* pScreenID);
    void ReloadLuaUI();

    void SetInGameGold(u32_t iGold) { m_iInGameGold = iGold; }
    void SetInGameBuyItemCallback(void(*pfn)(void*, u16_t), void* pUser);
    void SetLevelSkillCallback(void(*pfn)(void*, u8_t), void* pUser);

private:
    struct DamageFloater
    {
        Vec3 vWorldPos{};
        f32_t fAmount = 0.f;
        f32_t fAge = 0.f;
        f32_t fLifetime = 1.15f;
        f32_t fXJitter = 0.f;
        u8_t iDamageType = 0;
        bool_t bWasCrit = false;
        bool_t bKilled = false;
    };
    struct WorldTextFloater
    {
        Vec3 vWorldPos{};
        std::string strText;
        Vec4 vColor{ 1.f, 1.f, 1.f, 1.f };
        f32_t fAge = 0.f;
        f32_t fLifetime = 0.8f;
        f32_t fXJitter = 0.f;
        u32_t iGoldAmount = 0;
        bool_t bShowGoldIcon = false;
    };
    struct KillFeedBanner
    {
        eChampion eSourceChampion = eChampion::NONE;
        eChampion eTargetChampion = eChampion::NONE;
        u8_t iObjectKind = 0;
        bool_t bSourceAlly = false;
        f32_t fAge = 0.f;
        f32_t fLifetime = 3.f;
        std::string strMessage;
    };

    struct KillFeedPortraitCache
    {
        eChampion eChampionID = eChampion::NONE;
        void* pSRV = nullptr;
    };

    enum class ePingWheelDirection : uint8_t
    {
        None,
        OnMyWay,
        Danger,
        Assist,
        Missing,
    };

    struct MapPingMarker
    {
        Vec3 vWorldPos{};
        ePingWheelDirection eDirection = ePingWheelDirection::None;
        f32_t fAge = 0.f;
        f32_t fLifetime = 3.f;
    };

    struct GameContextHUDState
    {
        u16_t iBlueKills = 0;
        u16_t iRedKills = 0;
        u16_t iLocalKills = 0;
        u16_t iLocalDeaths = 0;
        u16_t iLocalAssists = 0;
        u16_t iLocalMinionKills = 0;
        u64_t iServerTimeMs = 0;
        bool_t bHasServerTime = false;
    };

    struct StatusPanelMatchScore
    {
        u16_t iBlueDragons = 0;
        u16_t iBlueBarons = 0;
        u16_t iBlueDestroyedTurrets = 0;
        u16_t iBlueDestroyedInhibitors = 0;
        u16_t iRedDragons = 0;
        u16_t iRedBarons = 0;
        u16_t iRedDestroyedTurrets = 0;
        u16_t iRedDestroyedInhibitors = 0;
    };

    struct StatusPanelChampionRow
    {
        EntityID Entity = NULL_ENTITY;
        eChampion Champion = eChampion::END;
        u8_t iTeam = 0;
        u8_t iLevel = 1;
        u16_t iKills = 0;
        u16_t iDeaths = 0;
        u16_t iAssists = 0;
        std::array<u16_t, 2> SummonerSpellIds{};
        std::array<f32_t, 2> SummonerCooldowns{};
        std::array<f32_t, 2> SummonerCooldownDurations{};
        // Status rows render inventory item icons here. Cooldowns are only for summoner spells.
        std::array<u16_t, 6> InventoryItemIds{};
    };

    struct StatusPanelSpellIconCache
    {
        u16_t iSpellId = 0;
        void* pSRV = nullptr;
    };

    void    DrawHealthBars(ImDrawList* pDraw, const DirectX::XMMATRIX& mVP);
    void    DrawHealthBarsRHI(const DirectX::XMMATRIX& mVP);
    void    DrawHealthBarBarcodeOverlay(ImDrawList* pDraw, const DirectX::XMMATRIX& mVP);
    void    DrawMinionHealthBars(ImDrawList* pDraw, const DirectX::XMMATRIX& mVP);
    void    DrawMinionHealthBarsRHI(const DirectX::XMMATRIX& mVP);
    void    DrawTurretHealthBars(ImDrawList* pDraw, const DirectX::XMMATRIX& mVP);
    void    DrawTurretHealthBarsRHI(const DirectX::XMMATRIX& mVP);
    void    DrawMouseCursor(ImDrawList* pDraw);
    void    DrawMouseCursorRHI();
    void    DrawPingWheel(ImDrawList* pDraw);
    void    DrawMapPings(ImDrawList* pDraw, const DirectX::XMMATRIX& mVP, f32_t fDeltaTime);
    void*   ResolvePingSRV(ePingWheelDirection eDirection) const;
    void    BuildChampionHUDState(ChampionHUDState& State);
    void    DrawChampionHUDRHI(const ChampionHUDState& State);
    void    DrawChampionHUDOverlay(ImDrawList* pDraw, const ChampionHUDState& State);
    void    DrawDamageFloaters(ImDrawList* pDraw, const DirectX::XMMATRIX& mVP, f32_t fDeltaTime);
    void    DrawGameContextHUD(ImDrawList* pDraw);
    void    DrawKillFeedBanners(ImDrawList* pDraw, f32_t fDeltaTime);
    void    DrawKillFeedCircleImage(ImDrawList* pDraw, const ImVec2& vCenter,
        f32_t fRadius, void* pSRV, ImU32 iTintColor, ImU32 iBorderColor);
    void*   FindOrLoadKillFeedPortrait(eChampion eChampionID);
    void    ResetGameContextHUDStats();
    void    LoadPingWheelAssets();
    ePingWheelDirection ResolvePingWheelDirection() const;
    void    DrawHUDStatusFlash(ImDrawList* pDraw, const ImVec2& root, f32_t hudW, f32_t hudH);
    void    UpdateHUDStatusTimers(EntityID localEntity, f32_t hp, bool_t bStunned, f32_t dt);
    void UpdateChampionHealthBarTrails(f32_t dt);
    f32_t ResolveChampionHealthTrailRatio(EntityID entity, f32_t currentRatio) const;
    void    ApplyCursorMode();

    //인게임 상점
    struct InGameShopItemView
    {
        u16_t iItemId = 0;
        u16_t iPrice = 0;
        const char* pName = nullptr;
        std::wstring strIconPath;
        void* pSRV = nullptr;
    };

    void LoadInGameShopAssets();
    void DrawInGameShop(ImDrawList* pDraw);
    void DrawStatusPanel(ImDrawList* pDraw);
    void BuildStatusPanelRows(
        std::vector<StatusPanelChampionRow>& BlueRows,
        std::vector<StatusPanelChampionRow>& RedRows,
        StatusPanelMatchScore& Score) const;
    void DrawStatusPanelObjectiveScores(ImDrawList* pDraw, const ImVec2& Root,
        f32_t fScaleX, f32_t fScaleY, const StatusPanelMatchScore& Score);
    void DrawStatusPanelRows(ImDrawList* pDraw, const ImVec2& Root,
        f32_t fScaleX, f32_t fScaleY,
        const std::vector<StatusPanelChampionRow>& Rows, bool_t bBlueSide);
    void DrawStatusPanelChampionRow(ImDrawList* pDraw, const ImVec2& Root,
        f32_t fScaleX, f32_t fScaleY,
        const StatusPanelChampionRow& Row, f32_t fRowTop, bool_t bBlueSide);
    void* FindOrLoadStatusPanelSummonerSpellIcon(u16_t iSpellId);
    const InGameShopItemView* FindInGameShopItem(u16_t iItemId) const;
    bool_t TryApplyPredictedInGamePurchase(u16_t iItemId);
    bool_t TryBuyInGameItem(u16_t iItemId);
    void LoadChampionHUDAssets();
    void LoadChampionHUDAssetsForChampion(eChampion eChampionID);
    void ApplyChampionHUDSkillIconOverrides(const ChampionHUDState& State);
    void ReleaseChampionHUDAssets();
    ImFont* FindUIFont(const char* pTag) const;

    std::vector<InGameShopItemView> m_InGameShopItems;
    std::array<u16_t, 6> m_InGameInventorySlots{};
    void* m_pSRV_InGameShopReference = nullptr;
    void* m_pSRV_StatusPanel = nullptr;
    std::vector<StatusPanelSpellIconCache> m_StatusPanelSpellIconCache;
    CUIAtlasManifest m_InGameShopAtlasManifest;
    bool_t m_bInGameShopOpen = false;
    bool_t m_bStatusPanelOpen = false;
    f32_t m_fInGameShopReferenceAlpha = 0.14f;
    f32_t m_fStatusPanelDrawWidth = 600.f;
    f32_t m_fStatusPanelDrawHeight = 400.f;
    f32_t m_fStatusPanelOffsetY = 0.f;
    u16_t m_iSelectedInGameShopItemId = 0;
    u32_t m_iInGameGold = 10000;
    std::string m_strInGameShopStatus = "Press P to open shop";

    void(*m_pfnBuyItem)(void*, u16_t) = nullptr;
    void* m_pBuyItemUser = nullptr;
    void(*m_pfnLevelSkill)(void*, u8_t) = nullptr;
    void* m_pLevelSkillUser = nullptr;

    // Phase B+ 확장 훅 (지금은 스텁 선언만)
    // void Draw_PlayerHUD();
    // void Draw_Scoreboard();

    // PNG 텍스처 로드 헬퍼 (WIC 경유)
    HRESULT Load_TextureSRV(const wchar_t* pPath, void** ppOut);

    CWorld* m_pWorld = nullptr;
    IRHIDevice* m_pDevice = nullptr;
    std::unique_ptr<CUIRenderer> m_pRHIUIRenderer;
    std::unique_ptr<CFont_Manager> m_pFontManager;
    bool_t                  m_bInitialized = false;

    // Champion-attached HP bar resources.
    void* m_pSRV_HPBarGreen = nullptr;
    void* m_pSRV_HPBarRed = nullptr;
    void* m_pSRV_MinionBlueHPBar = nullptr;
    void* m_pSRV_MinionRedHPBar = nullptr;
    void* m_pSRV_TurretBlueHPBar = nullptr;
    void* m_pSRV_TurretRedHPBar = nullptr;
    void* m_pSRV_ChampionHUDBase = nullptr;
    void* m_pSRV_HUDHit = nullptr;
    void* m_pSRV_HUDStun = nullptr;
    CUIAtlasManifest m_HudAtlasManifest;
    std::unique_ptr<CChampionHudPanel> m_pChampionHudPanel;
    std::unique_ptr<CLuaUIHost> m_pLuaUIHost;
    bool_t m_bUseLuaUI = true;
    eChampion m_eLoadedChampionHudAssets = eChampion::END;
    std::array<eChampion, 4> m_eLoadedSkillIconChampions{
        eChampion::END, eChampion::END, eChampion::END, eChampion::END };
    void* m_pSRV_ChampionPortrait = nullptr;
    void* m_pSRV_ChampionPassiveIcon = nullptr;
    void* m_pSRV_ChampionPassiveBar = nullptr;
    std::array<void*, 4> m_pSRV_ChampionSkillIcons{};
    void* m_pSRV_SkillRankPip = nullptr;
    void* m_pSRV_SkillUpgrade = nullptr;
    std::array<f32_t, 4> m_fSkillLearnFlashTimers{};
    std::array<u8_t, 5> m_LastHUDSkillRanks{};
    EntityID m_LastSkillRankHUDLocalEntity = NULL_ENTITY;

    // 토글 + 튜닝
    void* m_pSRV_CursorDefault = nullptr;
    void* m_pSRV_CursorEnemy = nullptr;
    void* m_pSRV_CursorAttack = nullptr;
    void* m_pSRV_PingWheelCursor = nullptr;
    void* m_pSRV_PingDefault = nullptr;
    void* m_pSRV_PingOnMyWay = nullptr;
    void* m_pSRV_PingDanger = nullptr;
    void* m_pSRV_PingAssist = nullptr;
    void* m_pSRV_PingMissing = nullptr;
    void* m_pSRV_OffscreenPingAtlas = nullptr;
    eCursorMode               m_CursorMode = eCursorMode::Default;
    f32_t                     m_fCursorSize = 32.f;
    bool_t                    m_bShowMouseCursor = true;
    bool_t                    m_bAttackMode = false;
    bool_t                    m_bEnemyHoverCursor = false;
    bool_t                    m_bPingWheelVisible = false;
    f32_t                     m_fPingWheelCenterX = 0.f;
    f32_t                     m_fPingWheelCenterY = 0.f;
    f32_t                     m_fPingWheelMouseX = 0.f;
    f32_t                     m_fPingWheelMouseY = 0.f;
    ePingWheelDirection       m_ePingWheelDirection = ePingWheelDirection::None;

    void* m_pSRV_AbilityAtlas = nullptr;
    eChampion                 m_PlayerChampion = eChampion::END;
    f32_t                     m_fHUDWidth = 861.f;
    f32_t                     m_fHUDHeight = 167.f;
    bool_t                    m_bShowChampionHUD = true;
    bool_t                    m_bShowChampionHUDReference = true;
    f32_t                     m_fHUDReferenceAlpha = 0.10f;
    bool_t                    m_bShowHUDStatusFlash = true;
    f32_t                     m_fHUDHitFlashDuration = 0.5f;
    f32_t                     m_fHUDStunFlashDuration = 0.5f;
    f32_t                     m_fHUDHitFlashTimer = 0.f;
    f32_t                     m_fHUDStunFlashTimer = 0.f;
    EntityID                  m_LastHUDLocalEntity = NULL_ENTITY;
    f32_t                     m_fLastHUDHP = 0.f;
    bool_t                    m_bHasLastHUDHP = false;
    bool_t                    m_bWasLocalStunned = false;

    bool_t  m_bShowHealthBars = true;
    f32_t   m_fHPBarWidth = 104.f;    // 화면 픽셀
    f32_t   m_fHPBarHeight = 20.f;
    f32_t   m_fHPBarYOffset = 2.75f;    // 월드 좌표 머리 위 높이 (m)

    struct ChampionHealthBarTrailState
    {
        f32_t fLastRatio = 1.f;
        f32_t fTrailRatio = 1.f;
        f32_t fHoldSec = 0.f;
        bool_t bInitialized = false;
    };

    std::unordered_map<EntityID, ChampionHealthBarTrailState> m_ChampionHealthBarTrails;
    f32_t m_fHealthBarTrailHoldSec = 0.04f;
    f32_t m_fHealthBarTrailShrinkSpeed = 3.75f;

    f32_t   m_fMinionHPBarWidth = 43.088f;
    f32_t   m_fMinionHPBarHeight = 3.f;
    f32_t   m_fMinionHPBarYOffset = 1.189f;

    f32_t   m_fTurretHPBarWidth = 125.5f;
    f32_t   m_fTurretHPBarHeight = 14.f;
    f32_t   m_fTurretHPBarYOffset = 4.75f;
    f32_t   m_fTurretHPBarScreenOffsetX = 0.f;
    f32_t   m_fTurretHPBarScreenOffsetY = 0.f;
    std::vector<DamageFloater> m_DamageFloaters;
    std::vector<WorldTextFloater> m_WorldTextFloaters;
    std::vector<KillFeedBanner> m_KillFeedBanners;
    std::vector<KillFeedPortraitCache> m_KillFeedPortraits;
    std::vector<MapPingMarker> m_MapPingMarkers;
    GameContextHUDState m_GameContextHUD{};
    bool_t  m_bShowGameContextHUD = true;
    f32_t   m_fGameContextHUDWidth = 382.f;
    f32_t   m_fGameContextHUDHeight = 34.f;
    bool_t  m_bShowDamageFloaters = true;
    f32_t   m_fDamageFloaterLife = 1.15f;

    uint32_t m_iWinSizeX = 0;
    uint32_t m_iWinSizeY = 0;

private:
    CUI_Manager() = default;   // private — Create() 팩토리만 생성 허용
};

NS_END
