#pragma once

#include "Manager/UI/ActorHUDAssets.h"
#include "Manager/UI/ActorHUDState.h"
#include "WintersTypes.h"
#include "WintersMath.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class CUIRenderer;
class IRHIDevice;
struct ImDrawList;
struct ImFont;
struct lua_State;

namespace Engine
{
    class CLuaRuntime;
    class CUIAtlasManifest;
    class CFont_Manager;

    class CLuaUIHost final
    {
    public:
        CLuaUIHost();
        ~CLuaUIHost();

        bool_t Initialize(IRHIDevice* pDevice, CUIRenderer* pRenderer, CFont_Manager* pFontManager);
        void Shutdown();

        void ReloadScripts();
        void SetActiveScreen(const char* pScreenID);
        void SetActorHUDState(const ActorHUDState& State);
        void SetShopItems(const UIShopItemAssetDesc* pItems, u32_t iItemCount);
        void SetBuyItemCallback(void(*pfn)(void*, u16_t), void* pUser);
        void SetLevelSkillCallback(void(*pfn)(void*, u8_t), void* pUser);

        void SetBoolean(const char* pName, bool_t bValue);
        bool_t GetBoolean(const char* pName) const;

        void DrawRHI(u32_t iScreenW, u32_t iScreenH);
        void DrawOverlay(ImDrawList* pDraw);
        void DrawTunerImGui();

    private:
        void RegisterLuaApi();
        void PushFrameState();
        void PushHudState(lua_State* pState) const;

        static CLuaUIHost* GetHost(lua_State* pState);
        static int LuaLog(lua_State* pState);
        static int LuaGetActiveScreen(lua_State* pState);
        static int LuaGetBool(lua_State* pState);
        static int LuaSetBool(lua_State* pState);
        static int LuaToggleShop(lua_State* pState);
        static int LuaBuyItem(lua_State* pState);
        static int LuaLevelSkill(lua_State* pState);
        static int LuaGetHudState(lua_State* pState);
        static int LuaGetShopItems(lua_State* pState);
        static int LuaText(lua_State* pState);
        static int LuaButtonRect(lua_State* pState);
        static int LuaDrawImage(lua_State* pState);
        static int LuaDrawSprite(lua_State* pState);
        static int LuaRect(lua_State* pState);
        static int LuaRectFilled(lua_State* pState);
        static int LuaPushClipRect(lua_State* pState);
        static int LuaPopClipRect(lua_State* pState);
        static int LuaGetInputState(lua_State* pState);
        static int LuaGetDisplaySize(lua_State* pState);
        static int LuaNoOp(lua_State* pState);

        void* FindOrLoadImageSRV(const char* pPath);
        bool_t LoadTextureSRV(const wchar_t* pPath, void** ppOut);
        void ReleaseLuaImages();

        std::unique_ptr<CLuaRuntime> m_pLua;
        IRHIDevice* m_pDevice = nullptr;
        CUIRenderer* m_pRenderer = nullptr;
        CFont_Manager* m_pFontManager = nullptr;
        ImDrawList* m_pCurrentDrawList = nullptr;
        std::unordered_map<std::string, void*> m_ImageSRVs{};
        std::unique_ptr<CUIAtlasManifest> m_pShopAtlasManifest;

        struct LuaShopItemView
        {
            u16_t ItemId = 0u;
            u16_t Price = 0u;
            u32_t Order = 0u;
            std::string AssetKey;
            std::string Section;
            std::string DisplayName;
            std::string IconPath;
            std::string IconSprite;
            bool_t Enabled = true;
            bool_t Purchasable = false;
        };

        std::vector<LuaShopItemView> m_ShopItems{};

        ActorHUDState m_HudState{};
        std::string m_strActiveScreen = "InGame";
        bool_t m_bInGameShopOpen = false;

        void(*m_pfnBuyItem)(void*, u16_t) = nullptr;
        void* m_pBuyItemUser = nullptr;
        void(*m_pfnLevelSkill)(void*, u8_t) = nullptr;
        void* m_pLevelSkillUser = nullptr;
    };
}
