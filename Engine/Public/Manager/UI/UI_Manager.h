#pragma once
#include "Engine_Defines.h"
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"
#include "World.h"
#include "Entity.h"
#include "RHI/IRHIDevice.h"
#include "Manager/UI/ActorHUDAssets.h"
#include "Manager/UI/ActorHUDState.h"
#include "Manager/UI/Font_Manager.h"
#include "Manager/UI/StatusPanelState.h"
#include "Manager/UI/UIAtlasManifest.h"
#include "Manager/UI/WorldHealthBarState.h"
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

class CActorHudPanel;
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
    void    OnImGui_StatusPanelLayoutTuner();

    void Bind_World(CWorld* pWorld);
    void    Set_ShowHealthBars(bool_t b) { m_bShowHealthBars = b; }
    bool_t  Get_ShowHealthBars() const { return m_bShowHealthBars; }
    void    Set_CursorMode(eCursorMode mode) { m_CursorMode = mode; }
    eCursorMode Get_CursorMode() const { return m_CursorMode; }
    void    RegisterActorHUDAssets(const ActorHUDAssetDesc* pAssets, u32_t iAssetCount);
    void    ClearActorHUDAssets();
    void    RegisterStatusPanelSpellIconAssets(const UIIconAssetDesc* pAssets, u32_t iAssetCount);
    void    ClearStatusPanelSpellIconAssets();
    void    SetStatusPanelDefaultSpellIds(const u16_t* pSpellIds, u32_t iSpellCount);
    void    RegisterInGameShopItems(const UIShopItemAssetDesc* pItems, u32_t iItemCount);
    void    ClearInGameShopItems();
    void    SetActorHUDState(const ActorHUDState* pState);
    void    ClearActorHUDState();
    void    SetStatusPanelState(const StatusPanelMatchScore* pScore,
        const StatusPanelActorRow* pBlueRows, u32_t iBlueCount,
        const StatusPanelActorRow* pRedRows, u32_t iRedCount);
    void    ClearStatusPanelState();
    void    SetWorldHealthBars(const UIWorldHealthBarDesc* pBars, u32_t iBarCount, u8_t iLocalTeam);
    void    ClearWorldHealthBars();
    void    SetMatchContextHUDScoreStats(
        u16_t iBlueKills, u16_t iRedKills,
        u16_t iLocalKills, u16_t iLocalDeaths, u16_t iLocalAssists);
    void    Set_PlayerActorContent(u8_t iActorContentId);
    void    Set_AttackMode(bool_t b);
    void    Set_EnemyHoverCursor(bool_t b);
    void    Set_PingWheel(bool_t bVisible,
        f32_t fCenterX, f32_t fCenterY,
        f32_t fMouseX, f32_t fMouseY);
    void    Push_MapPing(const Vec3& vWorldPos, u8_t iDirection);
    bool_t  Get_AttackMode() const { return m_bAttackMode; }
    void    Set_ShowActorHUD(bool_t b) { m_bShowActorHUD = b; }
    bool_t  Get_ShowActorHUD() const { return m_bShowActorHUD; }
    void    Set_ShowMouseCursor(bool_t b) { m_bShowMouseCursor = b; }
    bool_t  Get_ShowMouseCursor() const { return m_bShowMouseCursor; }
    void    Push_DamageNumber(const Vec3& vWorldPos, f32_t fAmount,
        u8_t iDamageType, bool_t bWasCrit, bool_t bKilled);
    void    Push_WorldText(const Vec3& vWorldPos, const char* pText,
        const Vec4& vColor, f32_t fLifetime);
    void    Push_GoldText(const Vec3& vWorldPos, u32_t iGoldAmount,
        f32_t fLifetime);
    void Push_KillFeedBanner(u8_t iSourceActorContentId, u8_t iTargetActorContentId,
        u8_t iObjectKind, u8_t iTargetTeam,
        bool_t bSourceAlly, const char* pMessage);
    void RecordMatchContextActorKill(u8_t iSourceTeam, u8_t iTargetTeam,
        bool_t bLocalSource, bool_t bLocalTarget);
    void RecordMatchContextUnitKill();
    void SetMatchContextServerTimeMs(u64_t iServerTimeMs);

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
        u8_t iSourceActorContentId = 0;
        u8_t iTargetActorContentId = 0;
        u8_t iObjectKind = 0;
        u8_t iTargetTeam = 255u;
        bool_t bSourceAlly = false;
        f32_t fAge = 0.f;
        f32_t fLifetime = 3.f;
        std::string strMessage;
    };

    struct KillFeedPortraitCache
    {
        u8_t iActorContentId = 0;
        void* pSRV = nullptr;
    };

    struct ActorHudAssetDef
    {
        u8_t iActorContentId = 255u;
        std::wstring strPortrait;
        std::wstring strPassive;
        std::array<std::wstring, 4> SkillIcons{};
        std::wstring strPassiveBar;
        bool_t bUsesPassiveResource = false;

        const wchar_t* PortraitPath() const { return strPortrait.empty() ? nullptr : strPortrait.c_str(); }
        const wchar_t* PassivePath() const { return strPassive.empty() ? nullptr : strPassive.c_str(); }
        const wchar_t* SkillIconPath(u32_t iIndex) const
        {
            return (iIndex < SkillIcons.size() && !SkillIcons[iIndex].empty())
                ? SkillIcons[iIndex].c_str()
                : nullptr;
        }
        const wchar_t* PassiveBarPath() const { return strPassiveBar.empty() ? nullptr : strPassiveBar.c_str(); }
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

    struct MatchContextHUDState
    {
        u16_t iBlueKills = 0;
        u16_t iRedKills = 0;
        u16_t iLocalKills = 0;
        u16_t iLocalDeaths = 0;
        u16_t iLocalAssists = 0;
        u16_t iLocalUnitKills = 0;
        u64_t iServerTimeMs = 0;
        bool_t bHasServerTime = false;
    };

    struct StatusPanelSpellIconCache
    {
        u16_t iSpellId = 0;
        void* pSRV = nullptr;
    };

    struct StatusPanelIconAssetDef
    {
        u16_t iContentId = 0u;
        std::wstring strIconPath;

        const wchar_t* IconPath() const { return strIconPath.empty() ? nullptr : strIconPath.c_str(); }
    };

    struct StatusPanelLayout
    {
        bool_t bPreviewPanel = true;
        bool_t bLockAssetAspect = true;
        f32_t fPanelWidth = 966.f;
        f32_t fPanelHeight = 388.7f;
        f32_t fPanelOffsetX = 18.f;
        f32_t fPanelOffsetY = 113.f;

        f32_t fBlueContentX = 40.f;
        f32_t fRedContentX = 814.f;
        f32_t fRowStartY = 101.f;
        f32_t fRowSpacingY = 99.36f;

        f32_t fPortraitOffsetX = -3.8f;
        f32_t fPortraitOffsetY = 7.4f;
        f32_t fPortraitSize = 76.6f;
        f32_t fLevelOffsetX = 60.9f;
        f32_t fLevelOffsetY = 57.4f;
        f32_t fLevelFontScale = 1.51f;

        f32_t fSpellOffsetX = 82.4f;
        f32_t fSpellOffsetY = 16.3f;
        f32_t fSpellSize = 38.8f;
        f32_t fSpellSpacingY = 31.3f;

        f32_t fKdaOffsetX = 150.8f;
        f32_t fKdaOffsetY = 33.4f;
        f32_t fKdaFontScale = 1.36f;

        f32_t fItemOffsetX = 277.8f;
        f32_t fItemOffsetY = 27.2f;
        f32_t fItemSize = 48.7f;
        f32_t fItemSpacingX = 56.2f;

        f32_t fObjectiveOffsetX = 13.8f;
        f32_t fObjectiveY = 40.2f;
        f32_t fObjectiveFontScale = 1.55f;
    };

    bool_t LoadStatusPanelLayoutSettings();
    bool_t SaveStatusPanelLayoutSettings();

    void    DrawHealthBars(ImDrawList* pDraw, const DirectX::XMMATRIX& mVP);
    void    DrawHealthBarsRHI(const DirectX::XMMATRIX& mVP);
    void    DrawHealthBarBarcodeOverlay(ImDrawList* pDraw, const DirectX::XMMATRIX& mVP);
    void    DrawUnitHealthBars(ImDrawList* pDraw, const DirectX::XMMATRIX& mVP);
    void    DrawUnitHealthBarsRHI(const DirectX::XMMATRIX& mVP);
    void    DrawStructureHealthBars(ImDrawList* pDraw, const DirectX::XMMATRIX& mVP);
    void    DrawStructureHealthBarsRHI(const DirectX::XMMATRIX& mVP);
    void    DrawMouseCursor(ImDrawList* pDraw);
    void    DrawMouseCursorRHI();
    void    DrawPingWheel(ImDrawList* pDraw);
    void    DrawMapPings(ImDrawList* pDraw, const DirectX::XMMATRIX& mVP, f32_t fDeltaTime);
    void*   ResolvePingSRV(ePingWheelDirection eDirection) const;
    void    DrawActorHUDRHI(const ActorHUDState& State);
    void    DrawActorHUDOverlay(ImDrawList* pDraw, const ActorHUDState& State);
    void    DrawDamageFloaters(ImDrawList* pDraw, const DirectX::XMMATRIX& mVP, f32_t fDeltaTime);
    void    DrawMatchContextHUD(ImDrawList* pDraw);
    void    DrawKillFeedBanners(ImDrawList* pDraw, f32_t fDeltaTime);
    void    DrawKillFeedCircleImage(ImDrawList* pDraw, const ImVec2& vCenter,
        f32_t fRadius, void* pSRV, ImU32 iTintColor, ImU32 iBorderColor);
    void*   FindOrLoadKillFeedPortrait(u8_t iActorContentId);
    void*   FindOrLoadKillFeedObjectIcon(u8_t iObjectKind, u8_t iTargetTeam);
    void    ResetMatchContextHUDStats();
    void    LoadPingWheelAssets();
    ePingWheelDirection ResolvePingWheelDirection() const;
    void UpdateCharacterHealthBarTrails(f32_t dt);
    f32_t ResolveCharacterHealthTrailRatio(EntityID entity, f32_t currentRatio) const;
    void    ApplyCursorMode();

    //인게임 상점
    struct InGameShopItemView
    {
        u16_t iItemId = 0;
        u16_t iPrice = 0;
        u32_t iOrder = 0u;
        std::string strAssetKey;
        std::string strSection;
        std::string strName;
        std::wstring strIconPath;
        std::string strIconSprite;
        std::vector<std::string> StatLines;
        bool_t bEnabled = true;
        bool_t bPurchasable = false;
        void* pSRV = nullptr;
    };

    void LoadInGameShopAssets();
    void LoadInGameShopItemTextures();
    void ReleaseInGameShopItemTextures();
    void DrawInGameShop(ImDrawList* pDraw);
    void DrawStatusPanel(ImDrawList* pDraw);
    void BuildStatusPanelRows(
        std::vector<StatusPanelActorRow>& BlueRows,
        std::vector<StatusPanelActorRow>& RedRows,
        StatusPanelMatchScore& Score) const;
    void DrawStatusPanelObjectiveScores(ImDrawList* pDraw, const ImVec2& Root,
        f32_t fScaleX, f32_t fScaleY, const StatusPanelMatchScore& Score);
    void DrawStatusPanelRows(ImDrawList* pDraw, const ImVec2& Root,
        f32_t fScaleX, f32_t fScaleY,
        const std::vector<StatusPanelActorRow>& Rows, bool_t bBlueSide);
    void DrawStatusPanelActorRow(ImDrawList* pDraw, const ImVec2& Root,
        f32_t fScaleX, f32_t fScaleY,
        const StatusPanelActorRow& Row, f32_t fRowTop, bool_t bBlueSide);
    void* FindOrLoadStatusPanelSummonerSpellIcon(u16_t iSpellId);
    const wchar_t* FindStatusPanelSpellIconPath(u16_t iSpellId) const;
    void ReleaseStatusPanelSpellIconCache();
    const InGameShopItemView* FindInGameShopItem(u16_t iItemId) const;
    bool_t TryBuyInGameItem(u16_t iItemId);
    void LoadActorHUDAssets();
    void LoadActorHUDAssetsForActor(u8_t iActorContentId);
    void ApplyActorHUDSkillIconOverrides(const ActorHUDState& State);
    void ReleaseActorHUDAssets();
    const ActorHudAssetDef* FindActorHudAssets(u8_t iActorContentId) const;
    const ActorHudAssetDef* ResolveActorHudAssets(u8_t iActorContentId) const;
    bool_t ShouldUseActorHUDPassiveResource(u8_t iActorContentId) const;
    ImFont* FindUIFont(const char* pTag) const;

    std::vector<InGameShopItemView> m_InGameShopItems;
    std::vector<ActorHudAssetDef> m_ActorHudAssets;
    ActorHUDState m_ActorHUDState{};
    bool_t m_bHasActorHUDState = false;
    std::array<u16_t, 6> m_InGameInventorySlots{};
    void* m_pSRV_InGameShopReference = nullptr;
    void* m_pSRV_StatusPanel = nullptr;
    std::vector<StatusPanelIconAssetDef> m_StatusPanelSpellIconAssets;
    std::vector<StatusPanelSpellIconCache> m_StatusPanelSpellIconCache;
    std::vector<StatusPanelActorRow> m_StatusPanelBlueRows;
    std::vector<StatusPanelActorRow> m_StatusPanelRedRows;
    StatusPanelMatchScore m_StatusPanelScore{};
    bool_t m_bHasStatusPanelState = false;
    std::vector<UIWorldHealthBarDesc> m_WorldHealthBars;
    u8_t m_iWorldHealthBarLocalTeam = 255u;
    bool_t m_bHasWorldHealthBarState = false;
    std::array<u16_t, 2> m_StatusPanelDefaultSpellIds{};
    CUIAtlasManifest m_InGameShopAtlasManifest;
    bool_t m_bInGameShopOpen = false;
    bool_t m_bStatusPanelOpen = false;
    f32_t m_fInGameShopReferenceAlpha = 0.14f;
    StatusPanelLayout m_StatusPanelLayout{};
    i32_t m_iStatusPanelLayoutTunerFrame = -2;
    std::string m_strStatusPanelLayoutSaveMessage;
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

    // Character-attached HP bar resources.
    void* m_pSRV_HPBarGreen = nullptr;
    void* m_pSRV_HPBarRed = nullptr;
    void* m_pSRV_UnitBlueHPBar = nullptr;
    void* m_pSRV_UnitRedHPBar = nullptr;
    void* m_pSRV_StructureBlueHPBar = nullptr;
    void* m_pSRV_StructureRedHPBar = nullptr;
    void* m_pSRV_ActorHUDBase = nullptr;
    CUIAtlasManifest m_HudAtlasManifest;
    std::unique_ptr<CActorHudPanel> m_pActorHudPanel;
    std::unique_ptr<CLuaUIHost> m_pLuaUIHost;
    bool_t m_bUseLuaUI = true;
    u8_t m_iLoadedActorHudContentId = 255u;
    std::array<u8_t, 4> m_iLoadedSkillIconContentIds{
        255u, 255u, 255u, 255u };
    void* m_pSRV_ActorPortrait = nullptr;
    void* m_pSRV_ActorPassiveIcon = nullptr;
    void* m_pSRV_ActorPassiveBar = nullptr;
    std::array<void*, 4> m_pSRV_ActorSkillIcons{};
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
    u8_t                      m_iPlayerActorContentId = 255u;
    f32_t                     m_fHUDWidth = 861.f;
    f32_t                     m_fHUDHeight = 167.f;
    bool_t                    m_bShowActorHUD = true;
    bool_t                    m_bShowActorHUDReference = true;
    f32_t                     m_fHUDReferenceAlpha = 0.10f;

    bool_t  m_bShowHealthBars = true;
    f32_t   m_fHPBarWidth = 104.f;    // 화면 픽셀
    f32_t   m_fHPBarHeight = 20.f;
    f32_t   m_fHPBarYOffset = 2.75f;    // 월드 좌표 머리 위 높이 (m)

    struct CharacterHealthBarTrailState
    {
        f32_t fLastRatio = 1.f;
        f32_t fTrailRatio = 1.f;
        f32_t fHoldSec = 0.f;
        bool_t bInitialized = false;
    };

    std::unordered_map<EntityID, CharacterHealthBarTrailState> m_CharacterHealthBarTrails;
    f32_t m_fHealthBarTrailHoldSec = 0.04f;
    f32_t m_fHealthBarTrailShrinkSpeed = 3.75f;

    f32_t   m_fUnitHPBarWidth = 43.088f;
    f32_t   m_fUnitHPBarHeight = 3.f;
    f32_t   m_fUnitHPBarYOffset = 1.189f;

    f32_t   m_fStructureHPBarWidth = 125.5f;
    f32_t   m_fStructureHPBarHeight = 14.f;
    f32_t   m_fStructureHPBarYOffset = 4.75f;
    f32_t   m_fStructureHPBarScreenOffsetX = 0.f;
    f32_t   m_fStructureHPBarScreenOffsetY = 0.f;
    std::vector<DamageFloater> m_DamageFloaters;
    std::vector<WorldTextFloater> m_WorldTextFloaters;
    std::vector<KillFeedBanner> m_KillFeedBanners;
    std::vector<KillFeedPortraitCache> m_KillFeedPortraits;
    void* m_pSRV_KillFeedTowerBlue = nullptr;
    void* m_pSRV_KillFeedTowerRed = nullptr;
    void* m_pSRV_KillFeedInhibitorBlue = nullptr;
    void* m_pSRV_KillFeedInhibitorRed = nullptr;
    std::vector<MapPingMarker> m_MapPingMarkers;
    MatchContextHUDState m_MatchContextHUD{};
    bool_t  m_bShowMatchContextHUD = true;
    f32_t   m_fMatchContextHUDWidth = 382.f;
    f32_t   m_fMatchContextHUDHeight = 34.f;
    bool_t  m_bShowDamageFloaters = true;
    f32_t   m_fDamageFloaterLife = 1.15f;

    uint32_t m_iWinSizeX = 0;
    uint32_t m_iWinSizeY = 0;

private:
    CUI_Manager() = default;   // private — Create() 팩토리만 생성 허용
};

NS_END
