#include "Manager/UI/LuaUIHost.h"

#include "Scripting/LuaRuntime.h"
#include "Renderer/UIRenderer.h"
#include "Manager/UI/Font_Manager.h"
#include "Manager/UI/UIAtlasManifest.h"
#include "../../../../Shared/GameSim/Definitions/ItemDef.h"
#include "RHI/IRHIDevice.h"
#include "RHI/RHITypes.h"
#include "WintersPaths.h"

#include <d3d11.h>
#include <DirectXTK/WICTextureLoader.h>

extern "C"
{
#include "../../../External/lua-5.4.8/src/lua.h"
#include "../../../External/lua-5.4.8/src/lauxlib.h"
}

#include <Windows.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

namespace
{
    void ReleaseSRV(void*& pSRV)
    {
        if (!pSRV)
            return;

        static_cast<ID3D11ShaderResourceView*>(pSRV)->Release();
        pSRV = nullptr;
    }

    std::wstring Utf8ToWide(const char* pText)
    {
        if (!pText || pText[0] == '\0')
            return {};

        const int iChars = MultiByteToWideChar(CP_UTF8, 0, pText, -1, nullptr, 0);
        if (iChars <= 0)
            return {};

        std::wstring Result;
        Result.resize(static_cast<std::size_t>(iChars));
        MultiByteToWideChar(CP_UTF8, 0, pText, -1, Result.data(), iChars);
        if (!Result.empty() && Result.back() == L'\0')
            Result.pop_back();
        return Result;
    }

    ImU32 LuaColor(lua_State* pState, int iFirstArg)
    {
        const int r = static_cast<int>(luaL_optinteger(pState, iFirstArg, 255));
        const int g = static_cast<int>(luaL_optinteger(pState, iFirstArg + 1, 255));
        const int b = static_cast<int>(luaL_optinteger(pState, iFirstArg + 2, 255));
        const int a = static_cast<int>(luaL_optinteger(pState, iFirstArg + 3, 255));
        return IM_COL32(r, g, b, a);
    }
}

namespace Engine
{
    CLuaUIHost::CLuaUIHost() = default;
    CLuaUIHost::~CLuaUIHost() = default;

    bool_t CLuaUIHost::Initialize(IRHIDevice* pDevice, CUIRenderer* pRenderer, CFont_Manager* pFontManager)
    {
        m_pDevice = pDevice;
        m_pRenderer = pRenderer;
        m_pFontManager = pFontManager;
        m_pLua = std::make_unique<CLuaRuntime>();
        if (!m_pLua->Initialize())
            return false;

        RegisterLuaApi();
        m_pShopAtlasManifest = std::make_unique<CUIAtlasManifest>();
        const bool_t bShopAtlasLoaded =
            m_pShopAtlasManifest->LoadFromJson(L"Resource/UI/itemshop_atlas_manifest.json") ||
            m_pShopAtlasManifest->LoadFromJson(L"Client/Bin/Resource/UI/itemshop_atlas_manifest.json");
        if (bShopAtlasLoaded)
        {
            m_pShopAtlasManifest->ForEachTexture(
                [this](const std::string&, UIAtlasTextureDef& Texture)
                {
                    if (Texture.strPath.empty())
                        return;

                    void* pSRV = nullptr;
                    if (LoadTextureSRV(Texture.strPath.c_str(), &pSRV))
                        Texture.pSRV = pSRV;
                });
        }
        ReloadScripts();
        return true;
    }

    void CLuaUIHost::Shutdown()
    {
        ReleaseLuaImages();
        m_pShopAtlasManifest.reset();
        if (m_pLua)
            m_pLua->Shutdown();
        m_pLua.reset();
    }

    void CLuaUIHost::ReloadScripts()
    {
        if (m_pLua)
        {
            if (!m_pLua->LoadFile(L"Resource/UI/Lua/itemshop_catalog.lua"))
                m_pLua->LoadFile(L"Client/Bin/Resource/UI/Lua/itemshop_catalog.lua");
            if (!m_pLua->LoadFile(L"Resource/UI/Lua/ui_boot.lua"))
                m_pLua->LoadFile(L"Client/Bin/Resource/UI/Lua/ui_boot.lua");
        }
    }

    void CLuaUIHost::SetActiveScreen(const char* pScreenID)
    {
        m_strActiveScreen = (pScreenID && pScreenID[0] != '\0') ? pScreenID : "InGame";
    }

    void CLuaUIHost::SetChampionHUDState(const ChampionHUDState& State)
    {
        m_HudState = State;
    }

    void CLuaUIHost::SetBuyItemCallback(void(*pfn)(void*, u16_t), void* pUser)
    {
        m_pfnBuyItem = pfn;
        m_pBuyItemUser = pUser;
    }

    void CLuaUIHost::SetLevelSkillCallback(void(*pfn)(void*, u8_t), void* pUser)
    {
        m_pfnLevelSkill = pfn;
        m_pLevelSkillUser = pUser;
    }

    void CLuaUIHost::SetBoolean(const char* pName, bool_t bValue)
    {
        if (!pName)
            return;

        if (std::strcmp(pName, "InGameShopOpen") == 0)
            m_bInGameShopOpen = bValue;
    }

    bool_t CLuaUIHost::GetBoolean(const char* pName) const
    {
        if (!pName)
            return false;

        if (std::strcmp(pName, "InGameShopOpen") == 0)
            return m_bInGameShopOpen;

        return false;
    }

    void CLuaUIHost::DrawRHI(u32_t iScreenW, u32_t iScreenH)
    {
        (void)iScreenW;
        (void)iScreenH;
        if (!m_pLua)
            return;

        PushFrameState();
        m_pLua->CallGlobal("WintersUIRenderRHI");
    }

    void CLuaUIHost::DrawOverlay(ImDrawList* pDraw)
    {
        if (!m_pLua)
            return;

        m_pCurrentDrawList = pDraw;
        PushFrameState();
        m_pLua->CallGlobal("WintersUIRenderOverlay");
        m_pCurrentDrawList = nullptr;
    }

    void CLuaUIHost::DrawTunerImGui()
    {
        if (!ImGui::CollapsingHeader("Lua UI"))
            return;

        ImGui::Text("Active Screen: %s", m_strActiveScreen.c_str());
        bool bShopOpen = m_bInGameShopOpen;
        if (ImGui::Checkbox("Lua Shop Open", &bShopOpen))
            m_bInGameShopOpen = bShopOpen;
        if (ImGui::Button("Reload Lua UI"))
            ReloadScripts();
        if (m_pLua && !m_pLua->GetLastError().empty())
            ImGui::TextWrapped("Lua: %s", m_pLua->GetLastError().c_str());
    }

    void CLuaUIHost::RegisterLuaApi()
    {
        if (!m_pLua || !m_pLua->GetState())
            return;

        lua_State* pState = m_pLua->GetState();
        lua_pushlightuserdata(pState, this);
        lua_setfield(pState, LUA_REGISTRYINDEX, "WintersLuaUIHost");

        lua_newtable(pState);
        lua_pushcfunction(pState, &CLuaUIHost::LuaLog);
        lua_setfield(pState, -2, "Log");
        lua_pushcfunction(pState, &CLuaUIHost::LuaGetActiveScreen);
        lua_setfield(pState, -2, "GetActiveScreen");
        lua_pushcfunction(pState, &CLuaUIHost::LuaGetBool);
        lua_setfield(pState, -2, "GetBool");
        lua_pushcfunction(pState, &CLuaUIHost::LuaSetBool);
        lua_setfield(pState, -2, "SetBool");
        lua_pushcfunction(pState, &CLuaUIHost::LuaToggleShop);
        lua_setfield(pState, -2, "ToggleShop");
        lua_pushcfunction(pState, &CLuaUIHost::LuaBuyItem);
        lua_setfield(pState, -2, "BuyItem");
        lua_pushcfunction(pState, &CLuaUIHost::LuaLevelSkill);
        lua_setfield(pState, -2, "LevelSkill");
        lua_pushcfunction(pState, &CLuaUIHost::LuaGetHudState);
        lua_setfield(pState, -2, "GetHudState");
        lua_pushcfunction(pState, &CLuaUIHost::LuaText);
        lua_setfield(pState, -2, "Text");
        lua_pushcfunction(pState, &CLuaUIHost::LuaButtonRect);
        lua_setfield(pState, -2, "ButtonRect");
        lua_pushcfunction(pState, &CLuaUIHost::LuaDrawSprite);
        lua_setfield(pState, -2, "DrawSprite");
        lua_pushcfunction(pState, &CLuaUIHost::LuaDrawImage);
        lua_setfield(pState, -2, "DrawImage");
        lua_pushcfunction(pState, &CLuaUIHost::LuaRect);
        lua_setfield(pState, -2, "Rect");
        lua_pushcfunction(pState, &CLuaUIHost::LuaRectFilled);
        lua_setfield(pState, -2, "RectFilled");
        lua_pushcfunction(pState, &CLuaUIHost::LuaPushClipRect);
        lua_setfield(pState, -2, "PushClipRect");
        lua_pushcfunction(pState, &CLuaUIHost::LuaPopClipRect);
        lua_setfield(pState, -2, "PopClipRect");
        lua_pushcfunction(pState, &CLuaUIHost::LuaGetInputState);
        lua_setfield(pState, -2, "GetInputState");
        lua_pushcfunction(pState, &CLuaUIHost::LuaGetDisplaySize);
        lua_setfield(pState, -2, "GetDisplaySize");
        lua_setglobal(pState, "UI");
    }

    void CLuaUIHost::PushFrameState()
    {
        if (!m_pLua || !m_pLua->GetState())
            return;

        lua_State* pState = m_pLua->GetState();
        lua_newtable(pState);
        lua_pushstring(pState, m_strActiveScreen.c_str());
        lua_setfield(pState, -2, "activeScreen");
        lua_pushboolean(pState, m_bInGameShopOpen ? 1 : 0);
        lua_setfield(pState, -2, "inGameShopOpen");
        lua_setglobal(pState, "WintersUIFrame");
    }

    void CLuaUIHost::PushHudState(lua_State* pState) const
    {
        lua_newtable(pState);

        auto PushNumber = [pState](const char* pName, f32_t Value)
        {
            lua_pushnumber(pState, static_cast<lua_Number>(Value));
            lua_setfield(pState, -2, pName);
        };
        auto PushInteger = [pState](const char* pName, lua_Integer Value)
        {
            lua_pushinteger(pState, Value);
            lua_setfield(pState, -2, pName);
        };

        PushNumber("hp", m_HudState.Hp);
        PushNumber("maxHp", m_HudState.MaxHp);
        PushNumber("mp", m_HudState.Mp);
        PushNumber("maxMp", m_HudState.MaxMp);
        PushInteger("gold", static_cast<lua_Integer>(m_HudState.Gold));
        PushInteger("level", static_cast<lua_Integer>(m_HudState.Level));
        PushInteger("skillPoints", static_cast<lua_Integer>(m_HudState.SkillPoints));
        lua_pushboolean(pState, m_bInGameShopOpen ? 1 : 0);
        lua_setfield(pState, -2, "shopOpen");
        lua_newtable(pState);
        for (u32_t Index = 0; Index < m_HudState.InventoryItemIds.size(); ++Index)
        {
            lua_pushinteger(pState, static_cast<lua_Integer>(m_HudState.InventoryItemIds[Index]));
            lua_rawseti(pState, -2, static_cast<lua_Integer>(Index + 1u));
        }
        lua_setfield(pState, -2, "inventoryItemIds");

        char Text[64] = {};
        std::snprintf(Text, sizeof(Text), "%.0f / %.0f", m_HudState.Hp, m_HudState.MaxHp);
        lua_pushstring(pState, Text);
        lua_setfield(pState, -2, "hpText");
        std::snprintf(Text, sizeof(Text), "%.0f / %.0f", m_HudState.Mp, m_HudState.MaxMp);
        lua_pushstring(pState, Text);
        lua_setfield(pState, -2, "mpText");
    }

    CLuaUIHost* CLuaUIHost::GetHost(lua_State* pState)
    {
        lua_getfield(pState, LUA_REGISTRYINDEX, "WintersLuaUIHost");
        CLuaUIHost* pHost = static_cast<CLuaUIHost*>(lua_touserdata(pState, -1));
        lua_pop(pState, 1);
        return pHost;
    }

    int CLuaUIHost::LuaLog(lua_State* pState)
    {
        const char* pText = luaL_optstring(pState, 1, "");
        std::string Message = "[LuaUI] ";
        Message += pText;
        Message += "\n";
        OutputDebugStringA(Message.c_str());
        return 0;
    }

    int CLuaUIHost::LuaGetActiveScreen(lua_State* pState)
    {
        CLuaUIHost* pHost = GetHost(pState);
        lua_pushstring(pState, pHost ? pHost->m_strActiveScreen.c_str() : "");
        return 1;
    }

    int CLuaUIHost::LuaGetBool(lua_State* pState)
    {
        CLuaUIHost* pHost = GetHost(pState);
        const char* pName = luaL_optstring(pState, 1, "");
        lua_pushboolean(pState, pHost && pHost->GetBoolean(pName));
        return 1;
    }

    int CLuaUIHost::LuaSetBool(lua_State* pState)
    {
        CLuaUIHost* pHost = GetHost(pState);
        if (pHost)
            pHost->SetBoolean(luaL_optstring(pState, 1, ""), lua_toboolean(pState, 2) != 0);
        return 0;
    }

    int CLuaUIHost::LuaToggleShop(lua_State* pState)
    {
        CLuaUIHost* pHost = GetHost(pState);
        if (pHost)
            pHost->m_bInGameShopOpen = !pHost->m_bInGameShopOpen;
        return 0;
    }

    int CLuaUIHost::LuaBuyItem(lua_State* pState)
    {
        CLuaUIHost* pHost = GetHost(pState);
        const u16_t ItemId = static_cast<u16_t>(luaL_checkinteger(pState, 1));
        if (pHost && pHost->m_pfnBuyItem && CItemRegistry::Instance().Find(ItemId))
            pHost->m_pfnBuyItem(pHost->m_pBuyItemUser, ItemId);
        return 0;
    }

    int CLuaUIHost::LuaLevelSkill(lua_State* pState)
    {
        CLuaUIHost* pHost = GetHost(pState);
        const u8_t Slot = static_cast<u8_t>(luaL_checkinteger(pState, 1));
        if (pHost && pHost->m_pfnLevelSkill)
            pHost->m_pfnLevelSkill(pHost->m_pLevelSkillUser, Slot);
        return 0;
    }

    int CLuaUIHost::LuaGetHudState(lua_State* pState)
    {
        CLuaUIHost* pHost = GetHost(pState);
        if (pHost)
            pHost->PushHudState(pState);
        else
            lua_newtable(pState);
        return 1;
    }

    int CLuaUIHost::LuaText(lua_State* pState)
    {
        CLuaUIHost* pHost = GetHost(pState);
        if (!pHost || !pHost->m_pCurrentDrawList)
            return 0;

        const char* pText = luaL_optstring(pState, 2, "");
        const f32_t X = static_cast<f32_t>(luaL_optnumber(pState, 3, 0.0));
        const f32_t Y = static_cast<f32_t>(luaL_optnumber(pState, 4, 0.0));
        pHost->m_pCurrentDrawList->AddText(ImVec2(X, Y), IM_COL32(245, 231, 177, 255), pText);
        return 0;
    }

    int CLuaUIHost::LuaButtonRect(lua_State* pState)
    {
        (void)luaL_optstring(pState, 1, "");
        const f32_t X = static_cast<f32_t>(luaL_optnumber(pState, 2, 0.0));
        const f32_t Y = static_cast<f32_t>(luaL_optnumber(pState, 3, 0.0));
        const f32_t W = static_cast<f32_t>(luaL_optnumber(pState, 4, 0.0));
        const f32_t H = static_cast<f32_t>(luaL_optnumber(pState, 5, 0.0));

        const ImGuiIO& IO = ImGui::GetIO();
        const bool_t bInside =
            IO.MousePos.x >= X &&
            IO.MousePos.x <= X + W &&
            IO.MousePos.y >= Y &&
            IO.MousePos.y <= Y + H;
        lua_pushboolean(pState, bInside && IO.MouseClicked[0]);
        return 1;
    }

    void* CLuaUIHost::FindOrLoadImageSRV(const char* pPath)
    {
        if (!pPath || pPath[0] == '\0')
            return nullptr;

        auto it = m_ImageSRVs.find(pPath);
        if (it != m_ImageSRVs.end())
            return it->second;

        const std::wstring Path = Utf8ToWide(pPath);
        if (Path.empty())
            return nullptr;

        void* pSRV = nullptr;
        if (!LoadTextureSRV(Path.c_str(), &pSRV))
            return nullptr;

        m_ImageSRVs.emplace(pPath, pSRV);
        return pSRV;
    }

    bool_t CLuaUIHost::LoadTextureSRV(const wchar_t* pPath, void** ppOut)
    {
        if (!m_pDevice || !pPath || !ppOut)
            return false;

        *ppOut = nullptr;
        ID3D11Device* pNativeDevice = static_cast<ID3D11Device*>(
            m_pDevice->GetNativeHandle(eNativeHandleType::DX11Device));
        if (!pNativeDevice)
            return false;

        wchar_t ResolvedPath[MAX_PATH] = {};
        const wchar_t* pLoadPath = pPath;
        if (WintersResolveContentPath(pPath, ResolvedPath, MAX_PATH))
            pLoadPath = ResolvedPath;

        ID3D11ShaderResourceView* pSRV = nullptr;
        const DirectX::WIC_LOADER_FLAGS Flags =
            DirectX::WIC_LOADER_IGNORE_SRGB | DirectX::WIC_LOADER_FORCE_RGBA32;
        const HRESULT hr = DirectX::CreateWICTextureFromFileEx(
            pNativeDevice,
            pLoadPath,
            0,
            D3D11_USAGE_DEFAULT,
            D3D11_BIND_SHADER_RESOURCE,
            0,
            0,
            Flags,
            nullptr,
            &pSRV);
        if (FAILED(hr))
            return false;

        *ppOut = pSRV;
        return true;
    }

    void CLuaUIHost::ReleaseLuaImages()
    {
        for (auto& Pair : m_ImageSRVs)
            ReleaseSRV(Pair.second);
        m_ImageSRVs.clear();

        if (m_pShopAtlasManifest)
        {
            m_pShopAtlasManifest->ForEachTexture(
                [](const std::string&, UIAtlasTextureDef& Texture)
                {
                    ReleaseSRV(Texture.pSRV);
                });
        }
    }

    int CLuaUIHost::LuaDrawImage(lua_State* pState)
    {
        CLuaUIHost* pHost = GetHost(pState);
        if (!pHost || !pHost->m_pCurrentDrawList)
            return 0;

        const char* pPath = luaL_optstring(pState, 1, "");
        const f32_t X = static_cast<f32_t>(luaL_optnumber(pState, 2, 0.0));
        const f32_t Y = static_cast<f32_t>(luaL_optnumber(pState, 3, 0.0));
        const f32_t W = static_cast<f32_t>(luaL_optnumber(pState, 4, 0.0));
        const f32_t H = static_cast<f32_t>(luaL_optnumber(pState, 5, 0.0));
        const f32_t Alpha = static_cast<f32_t>(luaL_optnumber(pState, 6, 1.0));

        void* pSRV = pHost->FindOrLoadImageSRV(pPath);
        if (!pSRV)
            return 0;

        pHost->m_pCurrentDrawList->AddImage(
            reinterpret_cast<ImTextureID>(pSRV),
            ImVec2(X, Y),
            ImVec2(X + W, Y + H),
            ImVec2(0.f, 0.f),
            ImVec2(1.f, 1.f),
            IM_COL32(255, 255, 255, static_cast<int>(std::clamp(Alpha, 0.f, 1.f) * 255.f)));
        return 0;
    }

    int CLuaUIHost::LuaDrawSprite(lua_State* pState)
    {
        CLuaUIHost* pHost = GetHost(pState);
        if (!pHost || !pHost->m_pCurrentDrawList || !pHost->m_pShopAtlasManifest)
            return 0;

        const char* pSpriteID = luaL_optstring(pState, 1, "");
        const UISpriteDef* pSprite = pHost->m_pShopAtlasManifest->FindSprite(pSpriteID);
        if (!pSprite)
            return 0;

        const UIAtlasTextureDef* pTexture = pHost->m_pShopAtlasManifest->FindTexture(pSprite->strTextureID);
        if (!pTexture || !pTexture->pSRV)
            return 0;

        const f32_t X = static_cast<f32_t>(luaL_optnumber(pState, 2, 0.0));
        const f32_t Y = static_cast<f32_t>(luaL_optnumber(pState, 3, 0.0));
        const f32_t W = static_cast<f32_t>(luaL_optnumber(pState, 4, 0.0));
        const f32_t H = static_cast<f32_t>(luaL_optnumber(pState, 5, 0.0));
        const f32_t Alpha = static_cast<f32_t>(luaL_optnumber(pState, 6, 1.0));
        const Vec4 UV = pHost->m_pShopAtlasManifest->ResolveUVRect(*pSprite);
        pHost->m_pCurrentDrawList->AddImage(
            reinterpret_cast<ImTextureID>(pTexture->pSRV),
            ImVec2(X, Y),
            ImVec2(X + W, Y + H),
            ImVec2(UV.x, UV.y),
            ImVec2(UV.z, UV.w),
            IM_COL32(255, 255, 255, static_cast<int>(std::clamp(Alpha, 0.f, 1.f) * 255.f)));
        return 0;
    }

    int CLuaUIHost::LuaRect(lua_State* pState)
    {
        CLuaUIHost* pHost = GetHost(pState);
        if (!pHost || !pHost->m_pCurrentDrawList)
            return 0;

        const f32_t X = static_cast<f32_t>(luaL_optnumber(pState, 1, 0.0));
        const f32_t Y = static_cast<f32_t>(luaL_optnumber(pState, 2, 0.0));
        const f32_t W = static_cast<f32_t>(luaL_optnumber(pState, 3, 0.0));
        const f32_t H = static_cast<f32_t>(luaL_optnumber(pState, 4, 0.0));
        const f32_t Thickness = static_cast<f32_t>(luaL_optnumber(pState, 9, 1.0));
        pHost->m_pCurrentDrawList->AddRect(
            ImVec2(X, Y),
            ImVec2(X + W, Y + H),
            LuaColor(pState, 5),
            0.f,
            0,
            Thickness);
        return 0;
    }

    int CLuaUIHost::LuaRectFilled(lua_State* pState)
    {
        CLuaUIHost* pHost = GetHost(pState);
        if (!pHost || !pHost->m_pCurrentDrawList)
            return 0;

        const f32_t X = static_cast<f32_t>(luaL_optnumber(pState, 1, 0.0));
        const f32_t Y = static_cast<f32_t>(luaL_optnumber(pState, 2, 0.0));
        const f32_t W = static_cast<f32_t>(luaL_optnumber(pState, 3, 0.0));
        const f32_t H = static_cast<f32_t>(luaL_optnumber(pState, 4, 0.0));
        pHost->m_pCurrentDrawList->AddRectFilled(
            ImVec2(X, Y),
            ImVec2(X + W, Y + H),
            LuaColor(pState, 5));
        return 0;
    }

    int CLuaUIHost::LuaPushClipRect(lua_State* pState)
    {
        CLuaUIHost* pHost = GetHost(pState);
        if (!pHost || !pHost->m_pCurrentDrawList)
            return 0;

        const f32_t X = static_cast<f32_t>(luaL_optnumber(pState, 1, 0.0));
        const f32_t Y = static_cast<f32_t>(luaL_optnumber(pState, 2, 0.0));
        const f32_t W = static_cast<f32_t>(luaL_optnumber(pState, 3, 0.0));
        const f32_t H = static_cast<f32_t>(luaL_optnumber(pState, 4, 0.0));
        pHost->m_pCurrentDrawList->PushClipRect(
            ImVec2(X, Y),
            ImVec2(X + W, Y + H),
            true);
        return 0;
    }

    int CLuaUIHost::LuaPopClipRect(lua_State* pState)
    {
        CLuaUIHost* pHost = GetHost(pState);
        if (pHost && pHost->m_pCurrentDrawList)
            pHost->m_pCurrentDrawList->PopClipRect();
        return 0;
    }

    int CLuaUIHost::LuaGetInputState(lua_State* pState)
    {
        const ImGuiIO& IO = ImGui::GetIO();
        lua_newtable(pState);
        lua_pushnumber(pState, IO.MousePos.x);
        lua_setfield(pState, -2, "mouseX");
        lua_pushnumber(pState, IO.MousePos.y);
        lua_setfield(pState, -2, "mouseY");
        lua_pushnumber(pState, IO.MouseWheel);
        lua_setfield(pState, -2, "mouseWheel");
        lua_pushboolean(pState, IO.MouseClicked[0] ? 1 : 0);
        lua_setfield(pState, -2, "leftClicked");
        lua_pushboolean(pState, IO.MouseClicked[1] ? 1 : 0);
        lua_setfield(pState, -2, "rightClicked");
        lua_pushboolean(pState, IO.MouseDown[0] ? 1 : 0);
        lua_setfield(pState, -2, "leftDown");
        lua_pushboolean(pState, IO.MouseDown[1] ? 1 : 0);
        lua_setfield(pState, -2, "rightDown");
        return 1;
    }

    int CLuaUIHost::LuaGetDisplaySize(lua_State* pState)
    {
        const ImVec2 Display = ImGui::GetIO().DisplaySize;
        lua_newtable(pState);
        lua_pushnumber(pState, Display.x);
        lua_setfield(pState, -2, "w");
        lua_pushnumber(pState, Display.y);
        lua_setfield(pState, -2, "h");
        return 1;
    }

    int CLuaUIHost::LuaNoOp(lua_State*)
    {
        return 0;
    }
}
