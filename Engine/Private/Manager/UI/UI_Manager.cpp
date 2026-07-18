#define _CRT_SECURE_NO_WARNINGS
#include "Manager/UI/UI_Manager.h"
#include "ActorHUDPanel.h"
#include "Framework/CEngineApp.h"
#include "Manager/UI/LuaUIHost.h"
#include "RHI/RHITypes.h"
#include "Core/CInput.h"
#include "ProfilerAPI.h"
#include "WintersMath.h"
#include "WintersPaths.h"
#include <d3d11.h>
#include <DirectXTK/WICTextureLoader.h>
#include <algorithm>
#include <cerrno>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <utility>

NS_BEGIN(Engine)

namespace
{
    ID3D11Device* GetNativeDX11Device(IRHIDevice* pDevice)
    {
        if (!pDevice)
            return nullptr;
        return static_cast<ID3D11Device*>(
            pDevice->GetNativeHandle(eNativeHandleType::DX11Device));
    }

    void ReleaseSRV(void*& pSRV)
    {
        if (!pSRV)
            return;

        static_cast<ID3D11ShaderResourceView*>(pSRV)->Release();
        pSRV = nullptr;
    }

    constexpr const wchar_t* kPathHPBarGreen = L"Resource/Texture/UI/HealthBarGreen.png";
    constexpr const wchar_t* kPathHPBarRed = L"Resource/Texture/UI/HealthBarRed.png";
    constexpr const wchar_t* kPathUnitBlueHPBar = L"Resource/Texture/UI/MinionBlueHPBar.png";
    constexpr const wchar_t* kPathUnitRedHPBar = L"Resource/Texture/UI/MinionRedHPBar.png";
    constexpr const wchar_t* kPathStructureBlueHPBar = L"Resource/Texture/UI/TurretHpBarBlue.png";
    constexpr const wchar_t* kPathStructureRedHPBar = L"Resource/Texture/UI/TurretHpBarRed.png";
    constexpr const wchar_t* kPathCursorDefault = L"Resource/Texture/UI/hover_precise.png";
    constexpr const wchar_t* kPathCursorEnemy = L"Resource/Texture/UI/hover_enemy_precise.png";
    constexpr const wchar_t* kPathCursorAttack = L"Resource/Texture/UI/Cursor_Attack_Small.png";
    constexpr const wchar_t* kPathPingWheelCursor = L"Resource/Texture/UI/ux/cursors/radialmenucursor_ping.png";
    constexpr const wchar_t* kPathPingDefault = L"Resource/Texture/UI/ux/minimap/pings/ping.png";
    constexpr const wchar_t* kPathPingOnMyWay = L"Resource/Texture/UI/ux/minimap/pings/on_my_way_new.png";
    constexpr const wchar_t* kPathPingDanger = L"Resource/Texture/UI/ux/minimap/pings/caution.png";
    constexpr const wchar_t* kPathPingAssist = L"Resource/Texture/UI/ux/minimap/pings/assist.png";
    constexpr const wchar_t* kPathPingMissing = L"Resource/Texture/UI/ux/minimap/pings/mia_new.png";
    constexpr const wchar_t* kPathOffscreenPingAtlas = L"Resource/Texture/UI/HUD/offscreenping_atlas.png";
    constexpr const wchar_t* kPathStatsPanelAtlas = L"Resource/Texture/UI/HUD/statspanel_atlas.png";
    constexpr const wchar_t* kPathAbilityAtlas = L"Resource/Texture/UI/HUD/clarity_abilityatlas.png";
    constexpr const wchar_t* kPathActorHUDDefault = L"Resource/Texture/UI/HUD_Irelia_2.png";
    constexpr const wchar_t* kPathSkillRankPip = L"Resource/Texture/Character/Irelia/particles/defaultcoloroverlifetime.png";
    constexpr const wchar_t* kPathSkillUpgrade = L"Resource/Texture/UI/SkillUpgrade.png";
    constexpr const wchar_t* kPathHUDManifest = L"Resource/UI/hud_atlas_manifest.json";
    constexpr const wchar_t* kPathHUDManifestFallback = L"UI/hud_atlas_manifest.json";
    constexpr const wchar_t* kPathHUDLayout = L"Resource/UI/hud_irelia_layout.json";
    constexpr const wchar_t* kPathHUDLayoutFallback = L"Client/Bin/Resource/UI/hud_irelia_layout.json";
    constexpr const wchar_t* kPathInGameShopReference = L"Resource/Texture/UI/상점1.png";
    constexpr const wchar_t* kPathStatusPanel = L"Resource/Texture/UI/StatusPannel_final.png";
    constexpr const wchar_t* kPathKillFeedTowerBlue =
        L"Resource/Texture/UI/InGameUI/minimap_tower_blue.png";
    constexpr const wchar_t* kPathKillFeedTowerRed =
        L"Resource/Texture/UI/InGameUI/minimap_tower_red.png";
    constexpr const wchar_t* kPathKillFeedInhibitorBlue =
        L"Resource/Texture/UI/InGameUI/minimap_inhibitor_blue.png";
    constexpr const wchar_t* kPathKillFeedInhibitorRed =
        L"Resource/Texture/UI/InGameUI/minimap_inhibitor_red.png";
    constexpr u8_t kUITeamBlue = 0u;
    constexpr u8_t kUITeamRed = 1u;
    constexpr u8_t kUIInvalidTeam = 255u;
    constexpr const wchar_t* kPathInGameShopManifest = L"Resource/UI/itemshop_atlas_manifest.json";
    constexpr const wchar_t* kPathInGameShopManifestFallback = L"UI/itemshop_atlas_manifest.json";
    constexpr const wchar_t* kPathFontHud = L"Resource/Texture/UI/ux/fonts/beaufortforlol-bold.otf";
    constexpr const wchar_t* kPathFontShop = L"Resource/Texture/UI/ux/fonts/spiegel-semibold.otf";
    constexpr const wchar_t* kPathFontShopBody = L"Resource/Texture/UI/ux/fonts/spiegel-regular.otf";
    constexpr const wchar_t* kPathFontFallback = L"Resource/Texture/UI/ux/fonts/notosanscjk-regular.ttf";
    constexpr u8_t kKillFeedObjectActor = 1;
    constexpr u8_t kKillFeedObjectStructure = 2;
    constexpr u8_t kKillFeedObjectObjective = 3;
    constexpr u8_t kKillFeedObjectDragon = 4;
    constexpr u8_t kKillFeedObjectBaron = 5;
    constexpr f32_t kActorHUDRefW = 861.f;
    constexpr f32_t kActorHUDRefH = 167.f;
    constexpr f32_t kInventorySlotX[6] =
    {
        646.f, 688.f, 730.f,
        646.f, 688.f, 730.f,
    };
    constexpr f32_t kInventorySlotY[6] =
    {
        59.f, 59.f, 59.f,
        101.f, 101.f, 101.f,
    };
    constexpr f32_t kInventorySlotSize = 36.f;
    constexpr u8_t kUIContentNone = 0u;
    constexpr u8_t kUIContentDefault = 1u;
    constexpr u8_t kUIContentEnd = 255u;
    constexpr u32_t kStatusPanelLayoutSettingsVersion = 1u;
    constexpr u32_t kStatusPanelLayoutTraceLimit = 12u;

    void TraceStatusPanelLayoutSettings(const char* pMessage)
    {
        static u32_t s_iTraceCount = 0u;
        if (!pMessage || s_iTraceCount >= kStatusPanelLayoutTraceLimit)
            return;

        OutputDebugStringA(pMessage);
        ++s_iTraceCount;
    }

    bool_t BuildStatusPanelLayoutSettingsPath(std::filesystem::path& OutPath)
    {
        wchar_t LocalAppData[32768]{};
        const DWORD Length = GetEnvironmentVariableW(
            L"LOCALAPPDATA",
            LocalAppData,
            static_cast<DWORD>(std::size(LocalAppData)));
        if (Length == 0u || Length >= std::size(LocalAppData))
            return false;

        OutPath = std::filesystem::path(LocalAppData) /
            L"Winters" / L"Developer" / L"status_panel_layout.ini";
        return true;
    }

    void TrimStatusPanelSetting(std::string& Value)
    {
        const auto IsWhitespace = [](unsigned char Character)
        {
            return Character == ' ' || Character == '\t' ||
                Character == '\r' || Character == '\n';
        };

        std::size_t First = 0u;
        while (First < Value.size() &&
            IsWhitespace(static_cast<unsigned char>(Value[First])))
        {
            ++First;
        }

        std::size_t Last = Value.size();
        while (Last > First &&
            IsWhitespace(static_cast<unsigned char>(Value[Last - 1u])))
        {
            --Last;
        }

        Value = Value.substr(First, Last - First);
    }

    bool_t ReadStatusPanelSettingValues(
        const std::filesystem::path& Path,
        std::unordered_map<std::string, std::string>& OutValues)
    {
        OutValues.clear();
        std::ifstream File(Path);
        if (!File.is_open())
            return false;

        std::string Line;
        while (std::getline(File, Line))
        {
            TrimStatusPanelSetting(Line);
            if (Line.empty() || Line[0] == '#' || Line[0] == ';')
                continue;

            const std::size_t Separator = Line.find('=');
            if (Separator == std::string::npos)
                return false;

            std::string Key = Line.substr(0u, Separator);
            std::string Value = Line.substr(Separator + 1u);
            TrimStatusPanelSetting(Key);
            TrimStatusPanelSetting(Value);
            if (Key.empty() || Value.empty())
                return false;

            OutValues[std::move(Key)] = std::move(Value);
        }
        return !File.bad();
    }

    bool_t ReadStatusPanelU32(
        const std::unordered_map<std::string, std::string>& Values,
        const char* pKey,
        u32_t& OutValue)
    {
        const auto It = Values.find(pKey);
        if (It == Values.end())
            return false;

        errno = 0;
        char* pEnd = nullptr;
        const unsigned long Parsed = std::strtoul(It->second.c_str(), &pEnd, 10);
        if (errno == ERANGE || pEnd == It->second.c_str() || !pEnd || *pEnd != '\0' ||
            Parsed > static_cast<unsigned long>((std::numeric_limits<u32_t>::max)()))
        {
            return false;
        }

        OutValue = static_cast<u32_t>(Parsed);
        return true;
    }

    bool_t ReadStatusPanelBool(
        const std::unordered_map<std::string, std::string>& Values,
        const char* pKey,
        bool_t& OutValue)
    {
        u32_t Parsed = 0u;
        if (!ReadStatusPanelU32(Values, pKey, Parsed) || Parsed > 1u)
            return false;

        OutValue = Parsed != 0u;
        return true;
    }

    bool_t ReadStatusPanelFloat(
        const std::unordered_map<std::string, std::string>& Values,
        const char* pKey,
        f32_t fMinimum,
        f32_t fMaximum,
        f32_t& OutValue)
    {
        const auto It = Values.find(pKey);
        if (It == Values.end())
            return false;

        errno = 0;
        char* pEnd = nullptr;
        const f32_t Parsed = std::strtof(It->second.c_str(), &pEnd);
        if (errno == ERANGE || pEnd == It->second.c_str() || !pEnd || *pEnd != '\0' ||
            !std::isfinite(Parsed) || Parsed < fMinimum || Parsed > fMaximum)
        {
            return false;
        }

        OutValue = Parsed;
        return true;
    }


    bool_t IsValidUIContentId(u8_t iContentId)
    {
        return iContentId != kUIContentNone && iContentId != kUIContentEnd;
    }

    f32_t UI_Clamp01(f32_t v)
    {
        if (v < 0.f)
            return 0.f;
        if (v > 1.f)
            return 1.f;
        return v;
    }

    bool_t UI_ResolveShieldSegment(
        f32_t fCurrent,
        f32_t fMaximum,
        f32_t fShield,
        f32_t& fOutStartRatio,
        f32_t& fOutEndRatio)
    {
        if (fMaximum <= 0.f || fShield <= 0.f)
            return false;

        const f32_t fHpRatio = UI_Clamp01(fCurrent / fMaximum);
        const f32_t fShieldRatio = UI_Clamp01(fShield / fMaximum);
        fOutEndRatio = (std::min)(1.f, fHpRatio + fShieldRatio);
        fOutStartRatio = (std::max)(
            0.f,
            (std::min)(fHpRatio, 1.f - fShieldRatio));
        return fOutEndRatio > fOutStartRatio + 0.0001f;
    }

    ImU32 UI_ColorWithAlpha(i32_t r, i32_t g, i32_t b, f32_t alpha)
    {
        const i32_t a = static_cast<i32_t>(UI_Clamp01(alpha) * 255.f);
        return IM_COL32(r, g, b, a);
    }

    ImU32 UI_ColorFromVec4(const Vec4& vColor, f32_t alphaMul)
    {
        const i32_t r = static_cast<i32_t>(UI_Clamp01(vColor.x) * 255.f);
        const i32_t g = static_cast<i32_t>(UI_Clamp01(vColor.y) * 255.f);
        const i32_t b = static_cast<i32_t>(UI_Clamp01(vColor.z) * 255.f);
        const i32_t a = static_cast<i32_t>(UI_Clamp01(vColor.w * alphaMul) * 255.f);
        return IM_COL32(r, g, b, a);
    }

    ImU32 UI_DamageColor(u8_t iDamageType, bool_t bWasCrit, bool_t bKilled, f32_t alpha)
    {
        (void)bWasCrit;
        (void)bKilled;

        switch (iDamageType)
        {
        case 1:
            return UI_ColorWithAlpha(80, 168, 255, alpha);
        case 2:
            return UI_ColorWithAlpha(248, 248, 248, alpha);
        case 0:
        default:
            return UI_ColorWithAlpha(255, 82, 74, alpha);
        }
    }

    void UI_DrawPingWheelIcon(ImDrawList* pDraw, void* pSRV,
        const ImVec2& Center, f32_t Size, bool_t bSelected)
    {
        if (!pDraw || Size <= 0.f)
            return;

        const f32_t Half = Size * 0.5f;
        const ImVec2 Min(Center.x - Half, Center.y - Half);
        const ImVec2 Max(Center.x + Half, Center.y + Half);
        const ImU32 Back = bSelected
            ? IM_COL32(28, 56, 78, 210)
            : IM_COL32(8, 12, 18, 142);
        const ImU32 Border = bSelected
            ? IM_COL32(80, 204, 255, 245)
            : IM_COL32(128, 150, 166, 126);
        const ImU32 Tint = bSelected
            ? IM_COL32(255, 255, 255, 255)
            : IM_COL32(230, 236, 242, 210);

        pDraw->AddCircleFilled(Center, Half * 0.86f, Back, 32);
        if (pSRV)
        {
            pDraw->AddImage(
                reinterpret_cast<ImTextureID>(pSRV),
                Min,
                Max,
                ImVec2(0.f, 0.f),
                ImVec2(1.f, 1.f),
                Tint);
        }
        pDraw->AddCircle(Center, Half * 0.88f, Border, 32, bSelected ? 2.6f : 1.4f);
    }

    struct UIPingAtlasSprite
    {
        f32_t X0 = 0.f;
        f32_t Y0 = 0.f;
        f32_t X1 = 0.f;
        f32_t Y1 = 0.f;
    };

    ImVec2 UI_PingAtlasUV(f32_t x, f32_t y)
    {
        constexpr f32_t kAtlasSize = 1024.f;
        return ImVec2(x / kAtlasSize, y / kAtlasSize);
    }

    void UI_DrawPingAtlasSpriteCentered(
        ImDrawList* pDraw,
        void* pAtlasSRV,
        const UIPingAtlasSprite& Sprite,
        const ImVec2& Center,
        f32_t MaxSize,
        ImU32 Tint)
    {
        if (!pDraw || !pAtlasSRV || MaxSize <= 0.f)
            return;

        const f32_t SrcW = std::max(1.f, Sprite.X1 - Sprite.X0);
        const f32_t SrcH = std::max(1.f, Sprite.Y1 - Sprite.Y0);
        const f32_t Scale = MaxSize / std::max(SrcW, SrcH);
        const f32_t W = SrcW * Scale;
        const f32_t H = SrcH * Scale;
        const ImVec2 Min(Center.x - W * 0.5f, Center.y - H * 0.5f);
        const ImVec2 Max(Center.x + W * 0.5f, Center.y + H * 0.5f);

        pDraw->AddImage(
            reinterpret_cast<ImTextureID>(pAtlasSRV),
            Min,
            Max,
            UI_PingAtlasUV(Sprite.X0, Sprite.Y0),
            UI_PingAtlasUV(Sprite.X1, Sprite.Y1),
            Tint);
    }

    UIPingAtlasSprite UI_ResolvePingAtlasBaseSprite(u8_t iDirection)
    {
        switch (iDirection)
        {
        case 1: return UIPingAtlasSprite{ 142.f, 25.f, 238.f, 109.f };
        case 2: return UIPingAtlasSprite{ 268.f, 19.f, 352.f, 116.f };
        case 3: return UIPingAtlasSprite{ 28.f, 19.f, 112.f, 115.f };
        case 4: return UIPingAtlasSprite{ 382.f, 25.f, 479.f, 109.f };
        case 0:
        default:
            return UIPingAtlasSprite{ 28.f, 19.f, 112.f, 115.f };
        }
    }

    UIPingAtlasSprite UI_ResolvePingAtlasIconSprite(u8_t iDirection)
    {
        switch (iDirection)
        {
        case 1: return UIPingAtlasSprite{ 960.f, 202.f, 1018.f, 250.f };
        case 2: return UIPingAtlasSprite{ 960.f, 450.f, 1018.f, 508.f };
        case 3: return UIPingAtlasSprite{ 956.f, 140.f, 1018.f, 205.f };
        case 4: return UIPingAtlasSprite{ 960.f, 8.f, 1018.f, 100.f };
        case 0:
        default:
            return UIPingAtlasSprite{ 960.f, 202.f, 1018.f, 250.f };
        }
    }

    void UI_DrawPingAtlasMarker(ImDrawList* pDraw, void* pAtlasSRV,
        u8_t iDirection, const ImVec2& Center, f32_t Size, bool_t bSelected,
        f32_t Alpha = 1.f)
    {
        if (!pDraw || !pAtlasSRV || Size <= 0.f)
            return;

        const f32_t ClampedAlpha = UI_Clamp01(Alpha);
        if (bSelected)
        {
            pDraw->AddCircleFilled(
                Center,
                Size * 0.48f,
                UI_ColorWithAlpha(30, 144, 205, 0.28f * ClampedAlpha),
                32);
        }

        UI_DrawPingAtlasSpriteCentered(
            pDraw,
            pAtlasSRV,
            UI_ResolvePingAtlasBaseSprite(iDirection),
            Center,
            Size,
            UI_ColorWithAlpha(255, 255, 255, ClampedAlpha));

        f32_t IconSize = Size * 0.50f;
        if (iDirection == 3u)
            IconSize = Size * 0.58f;
        else if (iDirection == 4u)
            IconSize = Size * 0.62f;

        UI_DrawPingAtlasSpriteCentered(
            pDraw,
            pAtlasSRV,
            UI_ResolvePingAtlasIconSprite(iDirection),
            Center,
            IconSize,
            UI_ColorWithAlpha(255, 255, 255, ClampedAlpha));
    }

    Vec4 UI_WhiteVec(f32_t alpha = 1.f)
    {
        return Vec4(1.f, 1.f, 1.f, UI_Clamp01(alpha));
    }

    Vec4 UI_AtlasUVRect(f32_t x0, f32_t y0, f32_t x1, f32_t y1)
    {
        constexpr f32_t kAtlasSize = 1024.f;
        return Vec4(x0 / kAtlasSize, y0 / kAtlasSize, x1 / kAtlasSize, y1 / kAtlasSize);
    }

    bool UI_TryFindFirstFile(const std::wstring& strPattern, wchar_t* pOutPath, size_t iOutPathChars)
    {
        if (!pOutPath || iOutPathChars == 0)
            return false;

        WIN32_FIND_DATAW FindData{};
        HANDLE hFind = FindFirstFileW(strPattern.c_str(), &FindData);
        if (hFind == INVALID_HANDLE_VALUE)
            return false;

        bool bFound = false;
        do
        {
            if ((FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
                continue;

            std::wstring strPath = strPattern;
            const size_t Slash = strPath.find_last_of(L"\\/");
            if (Slash != std::wstring::npos)
                strPath.resize(Slash + 1);
            else
                strPath.clear();
            strPath += FindData.cFileName;

            if (strPath.size() + 1 <= iOutPathChars)
            {
                wcscpy_s(pOutPath, iOutPathChars, strPath.c_str());
                bFound = true;
            }
            break;
        } while (FindNextFileW(hFind, &FindData));

        FindClose(hFind);
        return bFound;
    }

    bool UI_TryFindItemIconPath(u16_t iItemId, wchar_t* pOutPath, size_t iOutPathChars)
    {
        if (!pOutPath || iOutPathChars == 0)
            return false;

        const std::wstring ItemPrefix = std::to_wstring(static_cast<u32_t>(iItemId)) + L"_*.png";
        std::vector<std::wstring> Patterns;

        wchar_t ExePath[MAX_PATH] = {};
        if (GetModuleFileNameW(nullptr, ExePath, MAX_PATH) > 0)
        {
            if (wchar_t* pSlash = wcsrchr(ExePath, L'\\'))
            {
                *(pSlash + 1) = L'\0';
                Patterns.emplace_back(std::wstring(ExePath) + L"Resource\\Texture\\UI\\Items\\" + ItemPrefix);
            }
        }

        wchar_t Cwd[MAX_PATH] = {};
        if (GetCurrentDirectoryW(MAX_PATH, Cwd) > 0)
        {
            std::wstring Base = Cwd;
            if (!Base.empty() && Base.back() != L'\\')
                Base.push_back(L'\\');
            Patterns.emplace_back(Base + L"Resource\\Texture\\UI\\Items\\" + ItemPrefix);
            Patterns.emplace_back(Base + L"Client\\Bin\\Resource\\Texture\\UI\\Items\\" + ItemPrefix);
        }

        Patterns.emplace_back(L"Resource\\Texture\\UI\\Items\\" + ItemPrefix);
        Patterns.emplace_back(L"Client\\Bin\\Resource\\Texture\\UI\\Items\\" + ItemPrefix);

        for (const std::wstring& Pattern : Patterns)
        {
            if (UI_TryFindFirstFile(Pattern, pOutPath, iOutPathChars))
                return true;
        }

        return false;
    }

    void UI_DrawOutlinedText(ImDrawList* pDraw, ImFont* pFont, f32_t fFontSize,
        const ImVec2& pos, ImU32 col, const char* pText)
    {
        if (!pDraw || !pText || !pFont)
            return;

        constexpr f32_t kOutline = 1.35f;
        const ImU32 outlineCol = IM_COL32(0, 0, 0, 220);
        pDraw->AddText(pFont, fFontSize, ImVec2(pos.x - kOutline, pos.y), outlineCol, pText);
        pDraw->AddText(pFont, fFontSize, ImVec2(pos.x + kOutline, pos.y), outlineCol, pText);
        pDraw->AddText(pFont, fFontSize, ImVec2(pos.x, pos.y - kOutline), outlineCol, pText);
        pDraw->AddText(pFont, fFontSize, ImVec2(pos.x, pos.y + kOutline), outlineCol, pText);
        pDraw->AddText(pFont, fFontSize, pos, col, pText);
    }

    void UI_DrawCooldownPie(ImDrawList* pDraw, const ImVec2& Center, f32_t Radius, f32_t Ratio)
    {
        if (!pDraw || Radius <= 0.f)
            return;

        Ratio = UI_Clamp01(Ratio);
        if (Ratio <= 0.f)
            return;

        const ImU32 Color = IM_COL32(0, 0, 0, 150);
        if (Ratio >= 0.995f)
        {
            pDraw->AddCircleFilled(Center, Radius, Color, 40);
            return;
        }

        constexpr f32_t kStartAngle = -0.5f * WintersMath::kPi;
        const f32_t EndAngle = kStartAngle - Ratio * WintersMath::kTwoPi;
        pDraw->PathClear();
        pDraw->PathLineTo(Center);
        pDraw->PathArcTo(Center, Radius, EndAngle, kStartAngle, 32);
        pDraw->PathFillConvex(Color);
    }

    void DrawSkillRankPips(ImDrawList* pDraw, void* pPipSRV, const ImVec2& Center, f32_t Scale, u8_t Rank, u8_t MaxRank)
    {
        if (!pDraw || MaxRank == 0)
            return;

        const u8_t Filled = (Rank > MaxRank) ? MaxRank : Rank;
        const f32_t Radius = 3.f * Scale;
        const f32_t Step = 8.f * Scale;
        const f32_t StartX = Center.x - Step * static_cast<f32_t>(MaxRank - 1u) * 0.5f;

        for (u8_t Index = 0; Index < MaxRank; ++Index)
        {
            const ImVec2 PipCenter(StartX + Step * static_cast<f32_t>(Index), Center.y);
            const ImVec2 Min(PipCenter.x - Radius, PipCenter.y - Radius);
            const ImVec2 Max(PipCenter.x + Radius, PipCenter.y + Radius);
            const ImU32 Color = Index < Filled
                ? IM_COL32(255, 255, 255, 245)
                : IM_COL32(12, 14, 18, 230);

            if (pPipSRV)
                pDraw->AddImage(reinterpret_cast<ImTextureID>(pPipSRV), Min, Max, ImVec2(0.f, 0.f), ImVec2(1.f, 1.f), Color);
            else
                pDraw->AddCircleFilled(PipCenter, Radius, Color, 16);
        }
    }

    void DrawSkillLearnFlash(ImDrawList* pDraw, void* pSkillIconSRV, const ImVec2& Min, const ImVec2& Max, f32_t Timer)
    {
        if (!pDraw || Timer <= 0.f)
            return;

        const f32_t Alpha = UI_Clamp01(Timer / 2.f);
        pDraw->AddRectFilled(Min, Max, IM_COL32(255, 255, 255, static_cast<i32_t>(52.f * Alpha)), 4.f);

        if (pSkillIconSRV)
        {
            pDraw->AddImage(
                reinterpret_cast<ImTextureID>(pSkillIconSRV),
                Min,
                Max,
                ImVec2(0.f, 0.f),
                ImVec2(1.f, 1.f),
                IM_COL32(255, 255, 255, static_cast<i32_t>(190.f * Alpha)));
        }

        pDraw->AddRect(Min, Max, IM_COL32(255, 236, 160, static_cast<i32_t>(220.f * Alpha)), 4.f, 0, 2.f);
    }

    bool_t UI_DrawManifestSprite(ImDrawList* pDraw,
        const CUIAtlasManifest& Manifest,
        const char* pSpriteID,
        const ImVec2& Min,
        const ImVec2& Max,
        f32_t fAlpha = 1.f)
    {
        if (!pDraw || !pSpriteID)
            return false;

        const UISpriteDef* pSprite = Manifest.FindSprite(pSpriteID);
        if (!pSprite)
            return false;

        const UIAtlasTextureDef* pTexture = Manifest.FindTexture(pSprite->strTextureID);
        if (!pTexture || !pTexture->pSRV)
            return false;

        const Vec4 UV = Manifest.ResolveUVRect(*pSprite);
        pDraw->AddImage(
            reinterpret_cast<ImTextureID>(pTexture->pSRV),
            Min,
            Max,
            ImVec2(UV.x, UV.y),
            ImVec2(UV.z, UV.w),
            IM_COL32(255, 255, 255, static_cast<i32_t>(UI_Clamp01(fAlpha) * 255.f)));
        return true;
    }
}

CUI_Manager::~CUI_Manager() = default;

bool_t CUI_Manager::LoadStatusPanelLayoutSettings()
{
    std::filesystem::path SettingsPath;
    if (!BuildStatusPanelLayoutSettingsPath(SettingsPath))
    {
        TraceStatusPanelLayoutSettings(
            "[UI_Manager] Status panel layout path unavailable; compiled defaults retained\n");
        return false;
    }

    std::error_code ExistsError;
    if (!std::filesystem::exists(SettingsPath, ExistsError))
    {
        TraceStatusPanelLayoutSettings(
            "[UI_Manager] Status panel layout settings not found; compiled defaults retained\n");
        return false;
    }

    std::unordered_map<std::string, std::string> Values;
    if (!ReadStatusPanelSettingValues(SettingsPath, Values))
    {
        TraceStatusPanelLayoutSettings(
            "[UI_Manager] Status panel layout settings unreadable; compiled defaults retained\n");
        return false;
    }

    StatusPanelLayout Loaded{};
    u32_t Version = 0u;
    const bool_t bValid =
        ReadStatusPanelU32(Values, "version", Version) &&
        Version == kStatusPanelLayoutSettingsVersion &&
        ReadStatusPanelBool(Values, "previewPanel", Loaded.bPreviewPanel) &&
        ReadStatusPanelBool(Values, "lockAssetAspect", Loaded.bLockAssetAspect) &&
        ReadStatusPanelFloat(Values, "panelWidth", 320.f, 1491.f, Loaded.fPanelWidth) &&
        ReadStatusPanelFloat(Values, "panelHeight", 220.f, 900.f, Loaded.fPanelHeight) &&
        ReadStatusPanelFloat(Values, "panelOffsetX", -640.f, 640.f, Loaded.fPanelOffsetX) &&
        ReadStatusPanelFloat(Values, "panelOffsetY", -480.f, 480.f, Loaded.fPanelOffsetY) &&
        ReadStatusPanelFloat(Values, "blueContentX", 0.f, 700.f, Loaded.fBlueContentX) &&
        ReadStatusPanelFloat(Values, "redContentX", 700.f, 1450.f, Loaded.fRedContentX) &&
        ReadStatusPanelFloat(Values, "rowStartY", 40.f, 160.f, Loaded.fRowStartY) &&
        ReadStatusPanelFloat(Values, "rowSpacingY", 60.f, 125.f, Loaded.fRowSpacingY) &&
        ReadStatusPanelFloat(Values, "portraitOffsetX", -80.f, 180.f, Loaded.fPortraitOffsetX) &&
        ReadStatusPanelFloat(Values, "portraitOffsetY", -40.f, 100.f, Loaded.fPortraitOffsetY) &&
        ReadStatusPanelFloat(Values, "portraitSize", 16.f, 100.f, Loaded.fPortraitSize) &&
        ReadStatusPanelFloat(Values, "levelOffsetX", -40.f, 100.f, Loaded.fLevelOffsetX) &&
        ReadStatusPanelFloat(Values, "levelOffsetY", -40.f, 100.f, Loaded.fLevelOffsetY) &&
        ReadStatusPanelFloat(Values, "levelFontScale", 0.35f, 2.f, Loaded.fLevelFontScale) &&
        ReadStatusPanelFloat(Values, "spellOffsetX", -40.f, 260.f, Loaded.fSpellOffsetX) &&
        ReadStatusPanelFloat(Values, "spellOffsetY", -40.f, 100.f, Loaded.fSpellOffsetY) &&
        ReadStatusPanelFloat(Values, "spellSize", 12.f, 72.f, Loaded.fSpellSize) &&
        ReadStatusPanelFloat(Values, "spellSpacingY", 12.f, 90.f, Loaded.fSpellSpacingY) &&
        ReadStatusPanelFloat(Values, "kdaOffsetX", -40.f, 360.f, Loaded.fKdaOffsetX) &&
        ReadStatusPanelFloat(Values, "kdaOffsetY", -40.f, 120.f, Loaded.fKdaOffsetY) &&
        ReadStatusPanelFloat(Values, "kdaFontScale", 0.35f, 2.f, Loaded.fKdaFontScale) &&
        ReadStatusPanelFloat(Values, "itemOffsetX", 80.f, 520.f, Loaded.fItemOffsetX) &&
        ReadStatusPanelFloat(Values, "itemOffsetY", -40.f, 120.f, Loaded.fItemOffsetY) &&
        ReadStatusPanelFloat(Values, "itemSize", 14.f, 72.f, Loaded.fItemSize) &&
        ReadStatusPanelFloat(Values, "itemSpacingX", 16.f, 90.f, Loaded.fItemSpacingX) &&
        ReadStatusPanelFloat(Values, "objectiveOffsetX", -160.f, 160.f, Loaded.fObjectiveOffsetX) &&
        ReadStatusPanelFloat(Values, "objectiveY", 0.f, 100.f, Loaded.fObjectiveY) &&
        ReadStatusPanelFloat(Values, "objectiveFontScale", 0.35f, 2.f, Loaded.fObjectiveFontScale);
    if (!bValid)
    {
        TraceStatusPanelLayoutSettings(
            "[UI_Manager] Status panel layout settings invalid; compiled defaults retained\n");
        return false;
    }

    m_StatusPanelLayout = Loaded;
    TraceStatusPanelLayoutSettings(
        "[UI_Manager] Status panel layout settings loaded\n");
    return true;
}

bool_t CUI_Manager::SaveStatusPanelLayoutSettings()
{
    std::filesystem::path SettingsPath;
    if (!BuildStatusPanelLayoutSettingsPath(SettingsPath))
    {
        TraceStatusPanelLayoutSettings(
            "[UI_Manager] Status panel layout save failed: path unavailable\n");
        return false;
    }

    std::error_code DirectoryError;
    std::filesystem::create_directories(SettingsPath.parent_path(), DirectoryError);
    if (DirectoryError)
    {
        TraceStatusPanelLayoutSettings(
            "[UI_Manager] Status panel layout save failed: directory unavailable\n");
        return false;
    }

    std::filesystem::path TemporaryPath = SettingsPath;
    TemporaryPath += L".tmp";

    FILE* pFile = nullptr;
    if (_wfopen_s(&pFile, TemporaryPath.c_str(), L"wb") != 0 || !pFile)
    {
        TraceStatusPanelLayoutSettings(
            "[UI_Manager] Status panel layout save failed: temp file unavailable\n");
        return false;
    }

    const i32_t WriteResult = fprintf(
        pFile,
        "version=%u\n"
        "previewPanel=%u\n"
        "lockAssetAspect=%u\n"
        "panelWidth=%.9g\n"
        "panelHeight=%.9g\n"
        "panelOffsetX=%.9g\n"
        "panelOffsetY=%.9g\n"
        "blueContentX=%.9g\n"
        "redContentX=%.9g\n"
        "rowStartY=%.9g\n"
        "rowSpacingY=%.9g\n"
        "portraitOffsetX=%.9g\n"
        "portraitOffsetY=%.9g\n"
        "portraitSize=%.9g\n"
        "levelOffsetX=%.9g\n"
        "levelOffsetY=%.9g\n"
        "levelFontScale=%.9g\n"
        "spellOffsetX=%.9g\n"
        "spellOffsetY=%.9g\n"
        "spellSize=%.9g\n"
        "spellSpacingY=%.9g\n"
        "kdaOffsetX=%.9g\n"
        "kdaOffsetY=%.9g\n"
        "kdaFontScale=%.9g\n"
        "itemOffsetX=%.9g\n"
        "itemOffsetY=%.9g\n"
        "itemSize=%.9g\n"
        "itemSpacingX=%.9g\n"
        "objectiveOffsetX=%.9g\n"
        "objectiveY=%.9g\n"
        "objectiveFontScale=%.9g\n",
        kStatusPanelLayoutSettingsVersion,
        m_StatusPanelLayout.bPreviewPanel ? 1u : 0u,
        m_StatusPanelLayout.bLockAssetAspect ? 1u : 0u,
        m_StatusPanelLayout.fPanelWidth,
        m_StatusPanelLayout.fPanelHeight,
        m_StatusPanelLayout.fPanelOffsetX,
        m_StatusPanelLayout.fPanelOffsetY,
        m_StatusPanelLayout.fBlueContentX,
        m_StatusPanelLayout.fRedContentX,
        m_StatusPanelLayout.fRowStartY,
        m_StatusPanelLayout.fRowSpacingY,
        m_StatusPanelLayout.fPortraitOffsetX,
        m_StatusPanelLayout.fPortraitOffsetY,
        m_StatusPanelLayout.fPortraitSize,
        m_StatusPanelLayout.fLevelOffsetX,
        m_StatusPanelLayout.fLevelOffsetY,
        m_StatusPanelLayout.fLevelFontScale,
        m_StatusPanelLayout.fSpellOffsetX,
        m_StatusPanelLayout.fSpellOffsetY,
        m_StatusPanelLayout.fSpellSize,
        m_StatusPanelLayout.fSpellSpacingY,
        m_StatusPanelLayout.fKdaOffsetX,
        m_StatusPanelLayout.fKdaOffsetY,
        m_StatusPanelLayout.fKdaFontScale,
        m_StatusPanelLayout.fItemOffsetX,
        m_StatusPanelLayout.fItemOffsetY,
        m_StatusPanelLayout.fItemSize,
        m_StatusPanelLayout.fItemSpacingX,
        m_StatusPanelLayout.fObjectiveOffsetX,
        m_StatusPanelLayout.fObjectiveY,
        m_StatusPanelLayout.fObjectiveFontScale);

    const bool_t bFlushed = WriteResult >= 0 && fflush(pFile) == 0;
    const bool_t bClosed = fclose(pFile) == 0;
    if (!bFlushed || !bClosed)
    {
        DeleteFileW(TemporaryPath.c_str());
        TraceStatusPanelLayoutSettings(
            "[UI_Manager] Status panel layout save failed: write incomplete\n");
        return false;
    }

    if (!MoveFileExW(
        TemporaryPath.c_str(),
        SettingsPath.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        DeleteFileW(TemporaryPath.c_str());
        TraceStatusPanelLayoutSettings(
            "[UI_Manager] Status panel layout save failed: commit failed\n");
        return false;
    }

    TraceStatusPanelLayoutSettings(
        "[UI_Manager] Status panel layout settings saved\n");
    return true;
}

HRESULT CUI_Manager::Initialize(CWorld* pWorld,
    IRHIDevice* pDevice,
    uint32_t iWinSizeX, uint32_t iWinSizeY)
{
    if (m_bInitialized) return S_OK;
    m_pWorld = pWorld;
    m_pDevice = pDevice;

    m_iWinSizeX = iWinSizeX;
    m_iWinSizeY = iWinSizeY;
    m_StatusPanelLayout = StatusPanelLayout{};
    m_strStatusPanelLayoutSaveMessage.clear();
    LoadStatusPanelLayoutSettings();

    m_pRHIUIRenderer = CUIRenderer::Create(pDevice);
    if (!m_pRHIUIRenderer)
        OutputDebugStringA("[UI_Manager] RHI UI renderer unavailable - gameplay UI requires RHI asset pass\n");

    m_pFontManager = std::make_unique<CFont_Manager>();
    m_pFontManager->AddFont("fallback", kPathFontFallback, 17.f);
    m_pFontManager->AddFont("hud", kPathFontHud, 17.f);
    m_pFontManager->AddFont("shop", kPathFontShop, 18.f);
    m_pFontManager->AddFont("shop.body", kPathFontShopBody, 15.f);

    if (FAILED(Load_TextureSRV(kPathHPBarGreen, &m_pSRV_HPBarGreen)))
    {
        OutputDebugStringA("[UI_Manager] HealthBarGreen.png load failed - ally HP bars use fallback fill\n");
        m_pSRV_HPBarGreen = nullptr;
    }
    if (FAILED(Load_TextureSRV(kPathHPBarRed, &m_pSRV_HPBarRed)))
    {
        OutputDebugStringA("[UI_Manager] HealthBarRed.png load failed - enemy HP bars use fallback fill\n");
        m_pSRV_HPBarRed = nullptr;
    }
    if (FAILED(Load_TextureSRV(kPathUnitBlueHPBar, &m_pSRV_UnitBlueHPBar)))
    {
        OutputDebugStringA("[UI_Manager] UnitBlueHPBar.png load failed - blue minion HP bars use fallback fill\n");
        m_pSRV_UnitBlueHPBar = nullptr;
    }
    if (FAILED(Load_TextureSRV(kPathUnitRedHPBar, &m_pSRV_UnitRedHPBar)))
    {
        OutputDebugStringA("[UI_Manager] UnitRedHPBar.png load failed - red minion HP bars use fallback fill\n");
        m_pSRV_UnitRedHPBar = nullptr;
    }
    if (FAILED(Load_TextureSRV(kPathStructureBlueHPBar, &m_pSRV_StructureBlueHPBar)))
    {
        OutputDebugStringA("[UI_Manager] StructureHpBarBlue.png load failed - blue structure HP bars use fallback fill\n");
        m_pSRV_StructureBlueHPBar = nullptr;
    }
    if (FAILED(Load_TextureSRV(kPathStructureRedHPBar, &m_pSRV_StructureRedHPBar)))
    {
        OutputDebugStringA("[UI_Manager] StructureHpBarRed.png load failed - red structure HP bars use fallback fill\n");
        m_pSRV_StructureRedHPBar = nullptr;
    }

    Load_TextureSRV(kPathCursorDefault, &m_pSRV_CursorDefault);
    Load_TextureSRV(kPathCursorEnemy, &m_pSRV_CursorEnemy);
    Load_TextureSRV(kPathCursorAttack, &m_pSRV_CursorAttack);
    // 로딩 씬은 프레임 스톨 동안에도 반응하는 네이티브 커서를 쓴다
    // (SetLoadingCursorMode). 그 네이티브 커서에 같은 이미지를 심어
    // 텍스처 커서 ↔ 네이티브 커서 전환이 보이지 않게 한다.
    if (!CEngineApp::Get().GetWindow().SetCursorImageFromFile(kPathCursorDefault))
        OutputDebugStringA("[UI_Manager] loading cursor image install failed - native arrow fallback\n");
    LoadPingWheelAssets();
    if (FAILED(Load_TextureSRV(kPathStatsPanelAtlas, &m_pSRV_StatsPanelAtlas)))
    {
        OutputDebugStringA("[UI_Manager] statspanel_atlas.png load failed - critical indicators skipped\n");
        m_pSRV_StatsPanelAtlas = nullptr;
    }
    if (FAILED(Load_TextureSRV(kPathAbilityAtlas, &m_pSRV_AbilityAtlas)))
    {
        OutputDebugStringA("[UI_Manager] clarity_abilityatlas.png load failed - ability atlas elements skipped\n");
        m_pSRV_AbilityAtlas = nullptr;
    }
    if (FAILED(Load_TextureSRV(kPathActorHUDDefault, &m_pSRV_ActorHUDBase)))
    {
        OutputDebugStringA("[UI_Manager] ActorHUD_Default.png load failed - actor HUD base skipped\n");
        m_pSRV_ActorHUDBase = nullptr;
    }
    if (FAILED(Load_TextureSRV(kPathSkillRankPip, &m_pSRV_SkillRankPip)))
    {
        OutputDebugStringA("[UI_Manager] skill rank pip texture load failed - circle fallback used\n");
        m_pSRV_SkillRankPip = nullptr;
    }
    if (FAILED(Load_TextureSRV(kPathSkillUpgrade, &m_pSRV_SkillUpgrade)))
    {
        OutputDebugStringA("[UI_Manager] SkillUpgrade.png load failed - triangle fallback used\n");
        m_pSRV_SkillUpgrade = nullptr;
    }
    if (FAILED(Load_TextureSRV(kPathStatusPanel, &m_pSRV_StatusPanel)))
    {
        OutputDebugStringA("[UI_Manager] StatusPannel_final.png load failed - Tab status panel skipped\n");
        m_pSRV_StatusPanel = nullptr;
    }

    LoadActorHUDAssets();
    LoadInGameShopAssets();
    m_pLuaUIHost = std::make_unique<CLuaUIHost>();
    if (m_pLuaUIHost->Initialize(m_pDevice, m_pRHIUIRenderer.get(), m_pFontManager.get()))
    {
        m_pLuaUIHost->SetBoolean("InGameShopOpen", m_bInGameShopOpen);
        m_pLuaUIHost->SetBuyItemCallback(m_pfnBuyItem, m_pBuyItemUser);
        m_pLuaUIHost->SetLevelSkillCallback(m_pfnLevelSkill, m_pLevelSkillUser);
    }
    else
    {
        OutputDebugStringA("[UI_Manager] Lua UI host initialize failed - native UI remains active\n");
        m_pLuaUIHost.reset();
    }

    m_bInitialized = true;
    return S_OK;
}

void CUI_Manager::Shutdown()
{
    if (m_pLuaUIHost)
    {
        m_pLuaUIHost->Shutdown();
        m_pLuaUIHost.reset();
    }
    ReleaseActorHUDAssets();
    m_pRHIUIRenderer.reset();
    ReleaseSRV(m_pSRV_HPBarGreen);
    ReleaseSRV(m_pSRV_HPBarRed);
    ReleaseSRV(m_pSRV_UnitBlueHPBar);
    ReleaseSRV(m_pSRV_UnitRedHPBar);
    ReleaseSRV(m_pSRV_StructureBlueHPBar);
    ReleaseSRV(m_pSRV_StructureRedHPBar);
    ReleaseSRV(m_pSRV_ActorHUDBase);
    ReleaseSRV(m_pSRV_CursorDefault);
    ReleaseSRV(m_pSRV_CursorEnemy);
    ReleaseSRV(m_pSRV_CursorAttack);
    ReleaseSRV(m_pSRV_PingWheelCursor);
    ReleaseSRV(m_pSRV_PingDefault);
    ReleaseSRV(m_pSRV_PingOnMyWay);
    ReleaseSRV(m_pSRV_PingDanger);
    ReleaseSRV(m_pSRV_PingAssist);
    ReleaseSRV(m_pSRV_PingMissing);
    ReleaseSRV(m_pSRV_OffscreenPingAtlas);
    ReleaseSRV(m_pSRV_StatsPanelAtlas);
    m_MapPingMarkers.clear();
    ReleaseSRV(m_pSRV_AbilityAtlas);
    ReleaseSRV(m_pSRV_SkillRankPip);
    ReleaseSRV(m_pSRV_SkillUpgrade);
    ReleaseSRV(m_pSRV_InGameShopReference);
    ReleaseSRV(m_pSRV_StatusPanel);
    ReleaseStatusPanelSpellIconCache();
    for (KillFeedPortraitCache& Portrait : m_KillFeedPortraits)
        ReleaseSRV(Portrait.pSRV);
    ReleaseSRV(m_pSRV_KillFeedTowerBlue);
    ReleaseSRV(m_pSRV_KillFeedTowerRed);
    ReleaseSRV(m_pSRV_KillFeedInhibitorBlue);
    ReleaseSRV(m_pSRV_KillFeedInhibitorRed);
    m_KillFeedPortraits.clear();
    m_KillFeedBanners.clear();
    m_InGameShopAtlasManifest.ForEachTexture(
        [](const std::string&, UIAtlasTextureDef& Texture)
        {
            ReleaseSRV(Texture.pSRV);
        });
    m_InGameShopAtlasManifest.Clear();
    if (m_pFontManager)
        m_pFontManager->Clear();
    m_pFontManager.reset();
    m_pWorld = nullptr;
    m_pDevice = nullptr;
    m_bInitialized = false;
    ReleaseInGameShopItemTextures();
    m_InGameShopItems.clear();

}

bool_t CUI_Manager::Begin_RawImagePass(u32_t iScreenWidth, u32_t iScreenHeight, bool_t bPointSample)
{
    if (!m_pRHIUIRenderer || !m_pRHIUIRenderer->IsReady() ||
        iScreenWidth == 0 || iScreenHeight == 0)
    {
        return false;
    }

    m_pRHIUIRenderer->Begin(
        iScreenWidth,
        iScreenHeight,
        bPointSample ? eUISamplerMode::PointClamp : eUISamplerMode::LinearClamp);
    return true;
}

void CUI_Manager::Draw_RawImage(void* pTextureSRV,
    f32_t fX, f32_t fY, f32_t fW, f32_t fH,
    const Vec4& vUVRect,
    const Vec4& vColor)
{
    if (!m_pRHIUIRenderer || !m_pRHIUIRenderer->IsReady())
        return;

    m_pRHIUIRenderer->DrawImage(pTextureSRV, fX, fY, fW, fH, vUVRect, vColor);
}

void CUI_Manager::End_RawImagePass()
{
    if (m_pRHIUIRenderer && m_pRHIUIRenderer->IsReady())
        m_pRHIUIRenderer->End();
}

void CUI_Manager::Draw_RawImageCircle(void* pTextureSRV,
    f32_t fX, f32_t fY, f32_t fW, f32_t fH,
    const Vec4& vUVRect, const Vec4& vColor, u32_t iSegmentCount)
{
    if (!m_pRHIUIRenderer || !m_pRHIUIRenderer->IsReady())
        return;

    m_pRHIUIRenderer->DrawImageCircle(
        pTextureSRV, fX, fY, fW, fH,
        vUVRect, vColor, iSegmentCount);
}

HRESULT CUI_Manager::Load_TextureSRV(const wchar_t* pPath, void** ppOut)
{
    ID3D11Device* pNativeDevice = GetNativeDX11Device(m_pDevice);
    if (!pNativeDevice || !ppOut) return E_FAIL;
    *ppOut = nullptr;

    // WIC ???PNG ??ID3D11ShaderResourceView* 직접 ??성
    wchar_t resolvedPath[MAX_PATH] = {};
    const wchar_t* pLoadPath = pPath;
    if (WintersResolveContentPath(pPath, resolvedPath, MAX_PATH))
        pLoadPath = resolvedPath;

    ID3D11ShaderResourceView* pSRV = nullptr;
    const DirectX::WIC_LOADER_FLAGS flags =
        DirectX::WIC_LOADER_IGNORE_SRGB | DirectX::WIC_LOADER_FORCE_RGBA32;
    HRESULT hr = DirectX::CreateWICTextureFromFileEx(
        pNativeDevice,
        pLoadPath,
        0,
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_SHADER_RESOURCE,
        0,
        0,
        flags,
        nullptr,
        &pSRV);
    if (SUCCEEDED(hr))
        *ppOut = pSRV;
    if (FAILED(hr))
    {
        wchar_t cwd[MAX_PATH] = L"";
        ::GetCurrentDirectoryW(MAX_PATH, cwd);
        wchar_t msg[MAX_PATH * 4];
        swprintf_s(msg, L"[UI_Manager] WIC fail hr=0x%08X path=[%ls] resolved=[%ls] cwd=[%ls]\n",
            hr, pPath, pLoadPath, cwd);
        ::OutputDebugStringW(msg);
    }
    return hr;
}

void CUI_Manager::LoadPingWheelAssets()
{
    ReleaseSRV(m_pSRV_PingWheelCursor);
    ReleaseSRV(m_pSRV_PingDefault);
    ReleaseSRV(m_pSRV_PingOnMyWay);
    ReleaseSRV(m_pSRV_PingDanger);
    ReleaseSRV(m_pSRV_PingAssist);
    ReleaseSRV(m_pSRV_PingMissing);
    ReleaseSRV(m_pSRV_OffscreenPingAtlas);

    if (FAILED(Load_TextureSRV(kPathOffscreenPingAtlas, &m_pSRV_OffscreenPingAtlas)))
        OutputDebugStringA("[UI_Manager] offscreenping_atlas.png load failed - legacy ping images used\n");
    if (FAILED(Load_TextureSRV(kPathPingWheelCursor, &m_pSRV_PingWheelCursor)))
        OutputDebugStringA("[UI_Manager] radialmenucursor_ping.png load failed - ping wheel uses ping icon fallback\n");
    if (FAILED(Load_TextureSRV(kPathPingDefault, &m_pSRV_PingDefault)))
        OutputDebugStringA("[UI_Manager] ping.png load failed - ping wheel center fallback used\n");
    if (FAILED(Load_TextureSRV(kPathPingOnMyWay, &m_pSRV_PingOnMyWay)))
        OutputDebugStringA("[UI_Manager] on_my_way_new.png load failed - on-my-way ping fallback used\n");
    if (FAILED(Load_TextureSRV(kPathPingDanger, &m_pSRV_PingDanger)))
        OutputDebugStringA("[UI_Manager] caution.png load failed - danger ping fallback used\n");
    if (FAILED(Load_TextureSRV(kPathPingAssist, &m_pSRV_PingAssist)))
        OutputDebugStringA("[UI_Manager] assist.png load failed - assist ping fallback used\n");
    if (FAILED(Load_TextureSRV(kPathPingMissing, &m_pSRV_PingMissing)))
        OutputDebugStringA("[UI_Manager] mia_new.png load failed - missing ping fallback used\n");
}

void CUI_Manager::LoadActorHUDAssets()
{
    ReleaseActorHUDAssets();

    m_pActorHudPanel = std::make_unique<CActorHudPanel>();
    m_pActorHudPanel->Initialize(m_pRHIUIRenderer.get(), &m_HudAtlasManifest);
    m_pActorHudPanel->SetReferenceTexture(m_pSRV_ActorHUDBase);
    m_pActorHudPanel->SetPassiveBarTexture(m_pSRV_ActorPassiveBar);
    m_pActorHudPanel->SetTextFont(FindUIFont("hud"));
    m_pActorHudPanel->SetReferenceAlpha(m_fHUDReferenceAlpha);
    m_pActorHudPanel->SetShowReference(m_bShowActorHUDReference);

    bool_t bManifestLoaded = m_HudAtlasManifest.LoadFromJson(kPathHUDManifest);
    if (!bManifestLoaded)
        bManifestLoaded = m_HudAtlasManifest.LoadFromJson(kPathHUDManifestFallback);

    if (bManifestLoaded)
    {
        m_HudAtlasManifest.ForEachTexture(
            [this](const std::string&, UIAtlasTextureDef& Texture)
            {
                if (Texture.strPath.empty())
                    return;

                void* pSRV = nullptr;
                if (SUCCEEDED(Load_TextureSRV(Texture.strPath.c_str(), &pSRV)))
                    Texture.pSRV = pSRV;
            });
    }
    else
    {
        OutputDebugStringA("[UI_Manager] hud_atlas_manifest.json load failed - HUD atlas sprites skipped\n");
    }

    if (!m_pActorHudPanel->LoadLayout(kPathHUDLayout) &&
        !m_pActorHudPanel->LoadLayout(kPathHUDLayoutFallback))
    {
        OutputDebugStringA("[UI_Manager] hud_irelia_layout.json load failed - using built-in HUD layout\n");
    }

    const u8_t iInitialActorContentId =
        (m_iPlayerActorContentId != kUIContentEnd) ? m_iPlayerActorContentId : kUIContentDefault;
    LoadActorHUDAssetsForActor(iInitialActorContentId);
}

void CUI_Manager::LoadActorHUDAssetsForActor(u8_t iActorContentId)
{
    if (!m_pActorHudPanel)
        return;

    const ActorHudAssetDef* pDef = ResolveActorHudAssets(iActorContentId);
    if (!pDef)
        return;
    if (m_iLoadedActorHudContentId == pDef->iActorContentId)
        return;

    ReleaseSRV(m_pSRV_ActorPortrait);
    ReleaseSRV(m_pSRV_ActorPassiveIcon);
    ReleaseSRV(m_pSRV_ActorPassiveBar);
    for (void*& pIconSRV : m_pSRV_ActorSkillIcons)
        ReleaseSRV(pIconSRV);

    m_pActorHudPanel->SetPortraitTexture(nullptr);
    m_pActorHudPanel->SetPassiveBarTexture(nullptr);
    for (u32_t i = 0; i < m_pSRV_ActorSkillIcons.size(); ++i)
        m_pActorHudPanel->SetSkillIconTexture(i, nullptr);

    m_iLoadedActorHudContentId = pDef->iActorContentId;
    m_iLoadedSkillIconContentIds.fill(pDef->iActorContentId);

    if (pDef->PortraitPath() && SUCCEEDED(Load_TextureSRV(pDef->PortraitPath(), &m_pSRV_ActorPortrait)))
        m_pActorHudPanel->SetPortraitTexture(m_pSRV_ActorPortrait);
    else
        OutputDebugStringA("[UI_Manager] actor portrait load failed - portrait skipped\n");

    if (pDef->PassivePath())
        (void)Load_TextureSRV(pDef->PassivePath(), &m_pSRV_ActorPassiveIcon);

    if (pDef->PassiveBarPath() &&
        SUCCEEDED(Load_TextureSRV(pDef->PassiveBarPath(), &m_pSRV_ActorPassiveBar)))
    {
        m_pActorHudPanel->SetPassiveBarTexture(m_pSRV_ActorPassiveBar);
    }

    for (u32_t i = 0; i < m_pSRV_ActorSkillIcons.size(); ++i)
    {
        if (pDef->SkillIconPath(i) &&
            SUCCEEDED(Load_TextureSRV(pDef->SkillIconPath(i), &m_pSRV_ActorSkillIcons[i])))
        {
            m_pActorHudPanel->SetSkillIconTexture(i, m_pSRV_ActorSkillIcons[i]);
        }
    }
}

void CUI_Manager::ApplyActorHUDSkillIconOverrides(const ActorHUDState& State)
{
    if (!m_pActorHudPanel)
        return;

    for (u32_t i = 0; i < State.SkillIconContentIds.size(); ++i)
    {
        u8_t desired = State.SkillIconContentIds[i];
        if (!IsValidUIContentId(desired))
            desired = State.iActorContentId;
        if (m_iLoadedSkillIconContentIds[i] == desired)
            continue;

        const ActorHudAssetDef* pDef = ResolveActorHudAssets(desired);
        if (!pDef || !pDef->SkillIconPath(i))
            continue;

        ReleaseSRV(m_pSRV_ActorSkillIcons[i]);
        m_pActorHudPanel->SetSkillIconTexture(i, nullptr);
        if (SUCCEEDED(Load_TextureSRV(pDef->SkillIconPath(i), &m_pSRV_ActorSkillIcons[i])))
            m_pActorHudPanel->SetSkillIconTexture(i, m_pSRV_ActorSkillIcons[i]);

        m_iLoadedSkillIconContentIds[i] = desired;
    }
}

void CUI_Manager::ReleaseActorHUDAssets()
{
    m_pActorHudPanel.reset();

    m_HudAtlasManifest.ForEachTexture(
        [](const std::string&, UIAtlasTextureDef& Texture)
        {
            ReleaseSRV(Texture.pSRV);
        });
    m_HudAtlasManifest.Clear();

    m_iLoadedActorHudContentId = kUIContentEnd;
    m_iLoadedSkillIconContentIds.fill(kUIContentEnd);
    ReleaseSRV(m_pSRV_ActorPortrait);
    ReleaseSRV(m_pSRV_ActorPassiveIcon);
    ReleaseSRV(m_pSRV_ActorPassiveBar);
    for (void*& pIconSRV : m_pSRV_ActorSkillIcons)
        ReleaseSRV(pIconSRV);
}

ImFont* CUI_Manager::FindUIFont(const char* pTag) const
{
    if (m_pFontManager)
    {
        if (pTag)
        {
            if (ImFont* pFont = m_pFontManager->FindFont(pTag))
                return pFont;
        }
        if (ImFont* pFallbackFont = m_pFontManager->GetFallbackFont())
            return pFallbackFont;
    }

    return ImGui::GetFont();
}

void CUI_Manager::SetInGameShopOpen(bool_t b)
{
    m_bInGameShopOpen = b;
    if (m_pLuaUIHost)
        m_pLuaUIHost->SetBoolean("InGameShopOpen", b);
}

void CUI_Manager::ToggleInGameShop()
{
    SetInGameShopOpen(!GetInGameShopOpen());
}

bool_t CUI_Manager::GetInGameShopOpen() const
{
    return m_pLuaUIHost ? m_pLuaUIHost->GetBoolean("InGameShopOpen") : m_bInGameShopOpen;
}

void CUI_Manager::SetStatusPanelOpen(bool_t b)
{
    m_bStatusPanelOpen = b;
}

void CUI_Manager::ToggleStatusPanel()
{
    SetStatusPanelOpen(!GetStatusPanelOpen());
}

void CUI_Manager::SetActiveLuaUIScreen(const char* pScreenID)
{
    if (m_pLuaUIHost)
        m_pLuaUIHost->SetActiveScreen(pScreenID);
}

void CUI_Manager::ReloadLuaUI()
{
    if (m_pLuaUIHost)
        m_pLuaUIHost->ReloadScripts();
}

void CUI_Manager::SetInGameBuyItemCallback(void(*pfn)(void*, u16_t), void* pUser)
{
    m_pfnBuyItem = pfn;
    m_pBuyItemUser = pUser;
    if (m_pLuaUIHost)
        m_pLuaUIHost->SetBuyItemCallback(pfn, pUser);
}

void CUI_Manager::SetLevelSkillCallback(void(*pfn)(void*, u8_t), void* pUser)
{
    m_pfnLevelSkill = pfn;
    m_pLevelSkillUser = pUser;
    if (m_pLuaUIHost)
        m_pLuaUIHost->SetLevelSkillCallback(pfn, pUser);
}

void CUI_Manager::SetInventoryReorderCallback(
    void(*pfn)(void*, u8_t, u8_t, u16_t), void* pUser)
{
    m_pfnInventoryReorder = pfn;
    m_pInventoryReorderUser = pUser;
    if (!pfn)
    {
        m_iInventoryDragSource = -1;
        m_iInventoryDragHover = -1;
        m_iInventoryDragItemId = 0u;
    }
}

bool_t CUI_Manager::IsPointerOverActorInventory() const
{
    if (m_iInventoryDragSource >= 0)
        return true;

    const ImVec2 Display = ImGui::GetIO().DisplaySize;
    f32_t HudW = m_fHUDWidth;
    f32_t HudH = m_fHUDHeight;
    if (HudW > Display.x - 24.f)
    {
        const f32_t Scale = (Display.x - 24.f) / HudW;
        HudW *= Scale;
        HudH *= Scale;
    }
    const f32_t ScaleX = HudW / kActorHUDRefW;
    const f32_t ScaleY = HudH / kActorHUDRefH;
    const ImVec2 Root((Display.x - HudW) * 0.5f, Display.y - HudH);
    const ImVec2 Mouse = ImGui::GetIO().MousePos;
    for (u8_t slot = 0u; slot < 6u; ++slot)
    {
        const f32_t left = Root.x + kInventorySlotX[slot] * ScaleX;
        const f32_t top = Root.y + kInventorySlotY[slot] * ScaleY;
        const f32_t right = left + kInventorySlotSize * ScaleX;
        const f32_t bottom = top + kInventorySlotSize * ScaleY;
        if (Mouse.x >= left && Mouse.x <= right &&
            Mouse.y >= top && Mouse.y <= bottom)
        {
            return true;
        }
    }
    return false;
}

void CUI_Manager::Bind_World(CWorld* pWorld)
{
    const bool_t bNewWorld = pWorld != nullptr && pWorld != m_pWorld;
    m_pWorld = pWorld;
    if (bNewWorld)
    {
        ResetMatchContextHUDStats();
        m_CharacterHealthBarTrails.clear();
    }
}

void CUI_Manager::RegisterActorHUDAssets(const ActorHUDAssetDesc* pAssets, u32_t iAssetCount)
{
    m_ActorHudAssets.clear();
    if (!pAssets || iAssetCount == 0u)
    {
        m_iLoadedActorHudContentId = kUIContentEnd;
        m_iLoadedSkillIconContentIds.fill(kUIContentEnd);
        return;
    }

    m_ActorHudAssets.reserve(iAssetCount);
    for (u32_t i = 0; i < iAssetCount; ++i)
    {
        const ActorHUDAssetDesc& Source = pAssets[i];
        if (!IsValidUIContentId(Source.iContentId))
            continue;

        ActorHudAssetDef Def{};
        Def.iActorContentId = Source.iContentId;
        if (Source.pPortraitPath)
            Def.strPortrait = Source.pPortraitPath;
        if (Source.pPassiveIconPath)
            Def.strPassive = Source.pPassiveIconPath;
        for (u32_t Index = 0; Index < Def.SkillIcons.size(); ++Index)
        {
            if (Source.SkillIconPaths[Index])
                Def.SkillIcons[Index] = Source.SkillIconPaths[Index];
        }
        if (Source.pPassiveBarPath)
            Def.strPassiveBar = Source.pPassiveBarPath;
        Def.bUsesPassiveResource = Source.bUsesPassiveResource;
        m_ActorHudAssets.push_back(std::move(Def));
    }

    m_iLoadedActorHudContentId = kUIContentEnd;
    m_iLoadedSkillIconContentIds.fill(kUIContentEnd);
}

void CUI_Manager::ClearActorHUDAssets()
{
    m_ActorHudAssets.clear();
    m_iLoadedActorHudContentId = kUIContentEnd;
    m_iLoadedSkillIconContentIds.fill(kUIContentEnd);
}

void CUI_Manager::RegisterStatusPanelSpellIconAssets(const UIIconAssetDesc* pAssets, u32_t iAssetCount)
{
    ReleaseStatusPanelSpellIconCache();
    m_StatusPanelSpellIconAssets.clear();

    if (!pAssets || iAssetCount == 0u)
        return;

    m_StatusPanelSpellIconAssets.reserve(iAssetCount);
    for (u32_t i = 0; i < iAssetCount; ++i)
    {
        const UIIconAssetDesc& Source = pAssets[i];
        if (Source.iContentId == 0u || !Source.pIconPath)
            continue;

        StatusPanelIconAssetDef Def{};
        Def.iContentId = Source.iContentId;
        Def.strIconPath = Source.pIconPath;
        m_StatusPanelSpellIconAssets.push_back(std::move(Def));
    }
}

void CUI_Manager::ClearStatusPanelSpellIconAssets()
{
    ReleaseStatusPanelSpellIconCache();
    m_StatusPanelSpellIconAssets.clear();
}

void CUI_Manager::SetStatusPanelDefaultSpellIds(const u16_t* pSpellIds, u32_t iSpellCount)
{
    m_StatusPanelDefaultSpellIds.fill(0u);
    if (!pSpellIds || iSpellCount == 0u)
        return;

    const u32_t iCopyCount =
        std::min<u32_t>(iSpellCount, static_cast<u32_t>(m_StatusPanelDefaultSpellIds.size()));
    for (u32_t i = 0; i < iCopyCount; ++i)
        m_StatusPanelDefaultSpellIds[i] = pSpellIds[i];
}

void CUI_Manager::RegisterInGameShopItems(const UIShopItemAssetDesc* pItems, u32_t iItemCount)
{
    ReleaseInGameShopItemTextures();
    m_InGameShopItems.clear();

    if (!pItems || iItemCount == 0u)
    {
        if (m_pLuaUIHost)
            m_pLuaUIHost->SetShopItems(nullptr, 0u);
        m_iSelectedInGameShopItemId = 0u;
        return;
    }

    m_InGameShopItems.reserve(iItemCount);
    for (u32_t i = 0; i < iItemCount; ++i)
    {
        const UIShopItemAssetDesc& Source = pItems[i];
        if (Source.iItemId == 0u && (!Source.pAssetKey || Source.pAssetKey[0] == '\0'))
            continue;

        InGameShopItemView Item{};
        Item.iItemId = Source.iItemId;
        Item.iPrice = Source.iPrice;
        Item.iOrder = Source.iOrder;
        if (Source.pAssetKey)
            Item.strAssetKey = Source.pAssetKey;
        if (Source.pSection)
            Item.strSection = Source.pSection;
        if (Source.pDisplayName)
            Item.strName = Source.pDisplayName;
        if (Source.pIconPath)
            Item.strIconPath = Source.pIconPath;
        if (Source.pIconSprite)
            Item.strIconSprite = Source.pIconSprite;
        Item.bEnabled = Source.bEnabled;
        Item.bPurchasable = Source.bPurchasable;
        if (Source.pStatLines && Source.iStatLineCount > 0u)
        {
            Item.StatLines.reserve(Source.iStatLineCount);
            for (u32_t LineIndex = 0; LineIndex < Source.iStatLineCount; ++LineIndex)
            {
                if (Source.pStatLines[LineIndex])
                    Item.StatLines.emplace_back(Source.pStatLines[LineIndex]);
            }
        }
        m_InGameShopItems.push_back(std::move(Item));
    }

    if (m_pLuaUIHost)
        m_pLuaUIHost->SetShopItems(pItems, iItemCount);

    LoadInGameShopItemTextures();

    const auto SelectedIt = std::find_if(
        m_InGameShopItems.begin(),
        m_InGameShopItems.end(),
        [](const InGameShopItemView& Item)
        {
            return Item.bEnabled && Item.bPurchasable && Item.iItemId != 0u;
        });
    m_iSelectedInGameShopItemId = SelectedIt != m_InGameShopItems.end()
        ? SelectedIt->iItemId
        : 0u;
}

void CUI_Manager::ClearInGameShopItems()
{
    ReleaseInGameShopItemTextures();
    m_InGameShopItems.clear();
    if (m_pLuaUIHost)
        m_pLuaUIHost->SetShopItems(nullptr, 0u);
    m_iSelectedInGameShopItemId = 0u;
}

void CUI_Manager::SetActorHUDState(const ActorHUDState* pState)
{
    if (!pState)
    {
        ClearActorHUDState();
        return;
    }

    m_ActorHUDState = *pState;
    if (ShouldUseActorHUDPassiveResource(m_ActorHUDState.iActorContentId) ||
        m_ActorHUDState.ResourceKind == eUIResourceKind::Flow)
    {
        m_ActorHUDState.bUsesPassiveResource = true;
        m_ActorHUDState.ResourceKind = eUIResourceKind::Flow;
        if (m_ActorHUDState.PassiveMax <= 0.f)
            m_ActorHUDState.PassiveMax = (m_ActorHUDState.MaxMp > 0.f) ? m_ActorHUDState.MaxMp : 100.f;
        if (m_ActorHUDState.PassiveShieldMax <= 0.f)
            m_ActorHUDState.PassiveShieldMax = m_ActorHUDState.PassiveMax;
    }

    m_ActorHUDState.bShopOpen = m_bInGameShopOpen;
    m_iInGameGold = m_ActorHUDState.Gold;
    m_InGameInventorySlots = m_ActorHUDState.InventoryItemIds;
    m_bHasActorHUDState = true;
}

void CUI_Manager::ClearActorHUDState()
{
    m_ActorHUDState = ActorHUDState{};
    m_bHasActorHUDState = false;
}

void CUI_Manager::SetStatusPanelState(const StatusPanelMatchScore* pScore,
    const StatusPanelActorRow* pBlueRows, u32_t iBlueCount,
    const StatusPanelActorRow* pRedRows, u32_t iRedCount)
{
    m_StatusPanelScore = pScore ? *pScore : StatusPanelMatchScore{};

    m_StatusPanelBlueRows.clear();
    if (pBlueRows && iBlueCount > 0u)
        m_StatusPanelBlueRows.assign(pBlueRows, pBlueRows + iBlueCount);

    m_StatusPanelRedRows.clear();
    if (pRedRows && iRedCount > 0u)
        m_StatusPanelRedRows.assign(pRedRows, pRedRows + iRedCount);

    m_bHasStatusPanelState = true;
}

void CUI_Manager::ClearStatusPanelState()
{
    m_StatusPanelScore = StatusPanelMatchScore{};
    m_StatusPanelBlueRows.clear();
    m_StatusPanelRedRows.clear();
    m_bHasStatusPanelState = false;
}

void CUI_Manager::SetWorldHealthBars(const UIWorldHealthBarDesc* pBars, u32_t iBarCount, u8_t iLocalTeam)
{
    m_WorldHealthBars.clear();
    m_iWorldHealthBarLocalTeam = iLocalTeam;

    if (pBars && iBarCount > 0u)
        m_WorldHealthBars.assign(pBars, pBars + iBarCount);

    m_bHasWorldHealthBarState = true;
}

void CUI_Manager::ClearWorldHealthBars()
{
    m_WorldHealthBars.clear();
    m_iWorldHealthBarLocalTeam = kUIInvalidTeam;
    m_bHasWorldHealthBarState = false;
    m_CharacterHealthBarTrails.clear();
}
void CUI_Manager::SetMatchContextHUDScoreStats(
    u16_t iBlueKills, u16_t iRedKills,
    u16_t iLocalKills, u16_t iLocalDeaths, u16_t iLocalAssists)
{
    m_MatchContextHUD.iBlueKills = iBlueKills;
    m_MatchContextHUD.iRedKills = iRedKills;
    m_MatchContextHUD.iLocalKills = iLocalKills;
    m_MatchContextHUD.iLocalDeaths = iLocalDeaths;
    m_MatchContextHUD.iLocalAssists = iLocalAssists;
}

const CUI_Manager::ActorHudAssetDef* CUI_Manager::FindActorHudAssets(u8_t iActorContentId) const
{
    for (const ActorHudAssetDef& Def : m_ActorHudAssets)
    {
        if (Def.iActorContentId == iActorContentId)
            return &Def;
    }
    return nullptr;
}

const CUI_Manager::ActorHudAssetDef* CUI_Manager::ResolveActorHudAssets(u8_t iActorContentId) const
{
    if (const ActorHudAssetDef* pDef = FindActorHudAssets(iActorContentId))
        return pDef;
    if (!m_ActorHudAssets.empty())
        return &m_ActorHudAssets.front();
    return nullptr;
}

bool_t CUI_Manager::ShouldUseActorHUDPassiveResource(u8_t iActorContentId) const
{
    const ActorHudAssetDef* pDef = FindActorHudAssets(iActorContentId);
    return pDef ? pDef->bUsesPassiveResource : false;
}

void CUI_Manager::Render_Overlay(const Mat4& matVP)
{
    if (!m_pWorld) return;
    const DirectX::XMMATRIX mVP = matVP.ToXMMATRIX();
    ActorHUDState HudState = m_ActorHUDState;
    if (!m_bHasActorHUDState)
    {
        if (m_iPlayerActorContentId != kUIContentEnd)
        {
            HudState.iActorContentId = m_iPlayerActorContentId;
            HudState.SkillIconContentIds.fill(m_iPlayerActorContentId);
        }
        HudState.Gold = m_iInGameGold;
        HudState.InventoryItemIds = m_InGameInventorySlots;
    }
    HudState.bShopOpen = m_bInGameShopOpen;

    const f32_t fUIDt = ImGui::GetIO().DeltaTime;
    {
        WINTERS_PROFILE_SCOPE("UI::HealthTrailUpdate");
        UpdateCharacterHealthBarTrails(fUIDt);
    }

    const bool_t bUseRHI =
        m_pRHIUIRenderer &&
        m_pRHIUIRenderer->IsReady() &&
        m_iWinSizeX > 0 &&
        m_iWinSizeY > 0;
    if (bUseRHI)
    {
        WINTERS_PROFILE_SCOPE("UI::RHIOverlay");
        m_pRHIUIRenderer->Begin(m_iWinSizeX, m_iWinSizeY);
        m_pRHIUIRenderer->ReserveQuads(768u);
        if (m_bShowHealthBars)
        {
            WINTERS_PROFILE_SCOPE("UI::RHIHealthBars");
            {
                WINTERS_PROFILE_SCOPE("UI::RHICharacterHealthBars");
                DrawHealthBarsRHI(mVP);
            }
            {
                WINTERS_PROFILE_SCOPE("UI::RHIUnitHealthBars");
                DrawUnitHealthBarsRHI(mVP);
            }
            {
                WINTERS_PROFILE_SCOPE("UI::RHIStructureHealthBars");
                DrawStructureHealthBarsRHI(mVP);
            }
        }
        if (m_bShowActorHUD)
        {
            WINTERS_PROFILE_SCOPE("UI::RHIActorHUD");
            DrawActorHUDRHI(HudState);
        }
        if (m_bUseLuaUI && m_pLuaUIHost)
        {
            WINTERS_PROFILE_SCOPE("UI::RHILua");
            m_pLuaUIHost->SetActorHUDState(HudState);
            m_pLuaUIHost->DrawRHI(m_iWinSizeX, m_iWinSizeY);
        }
        m_pRHIUIRenderer->End();
    }
    ImDrawList* pDraw = ImGui::GetBackgroundDrawList();
    if (m_bShowHealthBars)
    {
        if (bUseRHI)
        {
            WINTERS_PROFILE_SCOPE("UI::HealthBarcodeOverlay");
            DrawHealthBarBarcodeOverlay(pDraw, mVP);
        }
        else
        {
            WINTERS_PROFILE_SCOPE("UI::ImGuiHealthBars");
            DrawHealthBars(pDraw, mVP);
            DrawUnitHealthBars(pDraw, mVP);
            DrawStructureHealthBars(pDraw, mVP);
        }
    }
    {
        WINTERS_PROFILE_SCOPE("UI::DamageFloaters");
        DrawDamageFloaters(pDraw, mVP, ImGui::GetIO().DeltaTime);
    }
    // Phase B+: Draw_PlayerHUD / Scoreboard ...

    ImDrawList* pFG = ImGui::GetForegroundDrawList();
    {
        WINTERS_PROFILE_SCOPE("UI::MatchContextHUD");
        DrawMatchContextHUD(pFG);
    }
    {
        WINTERS_PROFILE_SCOPE("UI::KillFeed");
        DrawKillFeedBanners(pFG, ImGui::GetIO().DeltaTime);
    }
    {
        WINTERS_PROFILE_SCOPE("UI::MapPings");
        DrawMapPings(pFG, mVP, ImGui::GetIO().DeltaTime);
    }
    if (m_bShowActorHUD)
    {
        WINTERS_PROFILE_SCOPE("UI::ActorHUDOverlay");
        DrawActorHUDOverlay(pFG, HudState);
    }
    if (m_bUseLuaUI && m_pLuaUIHost)
    {
        WINTERS_PROFILE_SCOPE("UI::LuaOverlay");
        m_pLuaUIHost->SetActorHUDState(HudState);
        m_pLuaUIHost->DrawOverlay(pFG);
        m_bInGameShopOpen = m_pLuaUIHost->GetBoolean("InGameShopOpen");
    }
    {
        WINTERS_PROFILE_SCOPE("UI::StatusPanel");
        DrawStatusPanel(m_bStatusPanelOpen ? pFG : ImGui::GetBackgroundDrawList());
    }
    {
        WINTERS_PROFILE_SCOPE("UI::PingWheel");
        DrawPingWheel(pFG);
    }
}

void CUI_Manager::DrawPingWheel(ImDrawList* pDraw)
{
    if (!m_bPingWheelVisible || !pDraw)
        return;

    constexpr f32_t kRingRadius = 72.f;
    constexpr f32_t kOuterRadius = 102.f;
    constexpr f32_t kIconSize = 46.f;
    constexpr f32_t kCenterIconSize = 54.f;

    const ImVec2 Center(m_fPingWheelCenterX, m_fPingWheelCenterY);
    pDraw->AddCircleFilled(Center, kOuterRadius, IM_COL32(4, 8, 13, 104), 48);
    pDraw->AddCircle(Center, kOuterRadius, IM_COL32(44, 88, 116, 128), 48, 1.5f);
    pDraw->AddCircle(Center, 28.f, IM_COL32(86, 220, 255, 92), 32, 1.3f);

    if (m_pSRV_OffscreenPingAtlas)
    {
        UI_DrawPingAtlasMarker(pDraw, m_pSRV_OffscreenPingAtlas,
            static_cast<u8_t>(ePingWheelDirection::OnMyWay),
            ImVec2(Center.x + kRingRadius, Center.y),
            kIconSize, m_ePingWheelDirection == ePingWheelDirection::OnMyWay);
        UI_DrawPingAtlasMarker(pDraw, m_pSRV_OffscreenPingAtlas,
            static_cast<u8_t>(ePingWheelDirection::Danger),
            ImVec2(Center.x, Center.y - kRingRadius),
            kIconSize, m_ePingWheelDirection == ePingWheelDirection::Danger);
        UI_DrawPingAtlasMarker(pDraw, m_pSRV_OffscreenPingAtlas,
            static_cast<u8_t>(ePingWheelDirection::Assist),
            ImVec2(Center.x, Center.y + kRingRadius),
            kIconSize, m_ePingWheelDirection == ePingWheelDirection::Assist);
        UI_DrawPingAtlasMarker(pDraw, m_pSRV_OffscreenPingAtlas,
            static_cast<u8_t>(ePingWheelDirection::Missing),
            ImVec2(Center.x - kRingRadius, Center.y),
            kIconSize, m_ePingWheelDirection == ePingWheelDirection::Missing);
        UI_DrawPingAtlasMarker(pDraw, m_pSRV_OffscreenPingAtlas,
            static_cast<u8_t>(m_ePingWheelDirection),
            Center,
            kCenterIconSize,
            m_ePingWheelDirection == ePingWheelDirection::None);
        return;
    }

    UI_DrawPingWheelIcon(pDraw, m_pSRV_PingOnMyWay,
        ImVec2(Center.x + kRingRadius, Center.y),
        kIconSize, m_ePingWheelDirection == ePingWheelDirection::OnMyWay);
    UI_DrawPingWheelIcon(pDraw, m_pSRV_PingDanger,
        ImVec2(Center.x, Center.y - kRingRadius),
        kIconSize, m_ePingWheelDirection == ePingWheelDirection::Danger);
    UI_DrawPingWheelIcon(pDraw, m_pSRV_PingAssist,
        ImVec2(Center.x, Center.y + kRingRadius),
        kIconSize, m_ePingWheelDirection == ePingWheelDirection::Assist);
    UI_DrawPingWheelIcon(pDraw, m_pSRV_PingMissing,
        ImVec2(Center.x - kRingRadius, Center.y),
        kIconSize, m_ePingWheelDirection == ePingWheelDirection::Missing);

    UI_DrawPingWheelIcon(pDraw,
        m_pSRV_PingWheelCursor ? m_pSRV_PingWheelCursor : m_pSRV_PingDefault,
        Center,
        kCenterIconSize,
        m_ePingWheelDirection == ePingWheelDirection::None);
}

void CUI_Manager::Render_Cursor()
{
    if (!m_bShowMouseCursor)
        return;

    if (m_pRHIUIRenderer && m_pRHIUIRenderer->IsReady())
    {
        m_pRHIUIRenderer->Begin(m_iWinSizeX, m_iWinSizeY);
        DrawMouseCursorRHI();
        m_pRHIUIRenderer->End();
        return;
    }

    if (!ImGui::GetCurrentContext())
        return;
    ImDrawList* pFG = ImGui::GetForegroundDrawList();
    if (!pFG)
        return;

    DrawMouseCursor(pFG);
}

void CUI_Manager::Set_PlayerActorContent(u8_t iActorContentId)
{
    m_iPlayerActorContentId = iActorContentId;
}

void CUI_Manager::Set_AttackMode(bool_t b)
{
    m_bAttackMode = b;
    ApplyCursorMode();
}

void CUI_Manager::Set_EnemyHoverCursor(bool_t b)
{
    m_bEnemyHoverCursor = b;
    ApplyCursorMode();
}

void CUI_Manager::Set_PingWheel(bool_t bVisible,
    f32_t fCenterX, f32_t fCenterY,
    f32_t fMouseX, f32_t fMouseY)
{
    m_bPingWheelVisible = bVisible;
    m_fPingWheelCenterX = fCenterX;
    m_fPingWheelCenterY = fCenterY;
    m_fPingWheelMouseX = fMouseX;
    m_fPingWheelMouseY = fMouseY;
    m_ePingWheelDirection = bVisible
        ? ResolvePingWheelDirection()
        : ePingWheelDirection::None;
}

void CUI_Manager::Push_MapPing(const Vec3& vWorldPos, u8_t iDirection)
{
    ePingWheelDirection eDirection = ePingWheelDirection::None;
    if (iDirection <= static_cast<u8_t>(ePingWheelDirection::Missing))
        eDirection = static_cast<ePingWheelDirection>(iDirection);

    MapPingMarker Marker{};
    Marker.vWorldPos = vWorldPos;
    Marker.eDirection = eDirection;
    Marker.fLifetime = 3.f;
    m_MapPingMarkers.push_back(Marker);

    constexpr size_t kMaxMapPingMarkers = 32u;
    if (m_MapPingMarkers.size() > kMaxMapPingMarkers)
        m_MapPingMarkers.erase(m_MapPingMarkers.begin());
}

CUI_Manager::ePingWheelDirection CUI_Manager::ResolvePingWheelDirection() const
{
    const f32_t dx = m_fPingWheelMouseX - m_fPingWheelCenterX;
    const f32_t dy = m_fPingWheelMouseY - m_fPingWheelCenterY;
    constexpr f32_t kDeadZone = 18.f;
    if (dx * dx + dy * dy < kDeadZone * kDeadZone)
        return ePingWheelDirection::None;

    const f32_t ax = dx < 0.f ? -dx : dx;
    const f32_t ay = dy < 0.f ? -dy : dy;
    if (ax >= ay)
        return dx >= 0.f
            ? ePingWheelDirection::OnMyWay
            : ePingWheelDirection::Missing;

    return dy < 0.f
        ? ePingWheelDirection::Danger
        : ePingWheelDirection::Assist;
}

void* CUI_Manager::ResolvePingSRV(ePingWheelDirection eDirection) const
{
    switch (eDirection)
    {
    case ePingWheelDirection::OnMyWay:
        return m_pSRV_PingOnMyWay ? m_pSRV_PingOnMyWay : m_pSRV_PingDefault;
    case ePingWheelDirection::Danger:
        return m_pSRV_PingDanger ? m_pSRV_PingDanger : m_pSRV_PingDefault;
    case ePingWheelDirection::Assist:
        return m_pSRV_PingAssist ? m_pSRV_PingAssist : m_pSRV_PingDefault;
    case ePingWheelDirection::Missing:
        return m_pSRV_PingMissing ? m_pSRV_PingMissing : m_pSRV_PingDefault;
    case ePingWheelDirection::None:
    default:
        return m_pSRV_PingDefault;
    }
}

void CUI_Manager::ApplyCursorMode()
{
    if (m_bAttackMode)
        m_CursorMode = eCursorMode::Attack;
    else if (m_bEnemyHoverCursor)
        m_CursorMode = eCursorMode::Enemy;
    else
        m_CursorMode = eCursorMode::Default;
}

void CUI_Manager::LoadInGameShopAssets()
{
    ReleaseSRV(m_pSRV_InGameShopReference);
    m_InGameShopAtlasManifest.ForEachTexture(
        [](const std::string&, UIAtlasTextureDef& Texture)
        {
            ReleaseSRV(Texture.pSRV);
        });
    m_InGameShopAtlasManifest.Clear();

    ReleaseInGameShopItemTextures();

    bool_t bShopManifestLoaded = m_InGameShopAtlasManifest.LoadFromJson(kPathInGameShopManifest);
    if (!bShopManifestLoaded)
        bShopManifestLoaded = m_InGameShopAtlasManifest.LoadFromJson(kPathInGameShopManifestFallback);

    if (bShopManifestLoaded)
    {
        m_InGameShopAtlasManifest.ForEachTexture(
            [this](const std::string&, UIAtlasTextureDef& Texture)
            {
                if (Texture.strPath.empty())
                    return;

                void* pSRV = nullptr;
                if (SUCCEEDED(Load_TextureSRV(Texture.strPath.c_str(), &pSRV)))
                    Texture.pSRV = pSRV;
            });
    }
    else
    {
        OutputDebugStringA("[UI_Manager] itemshop_atlas_manifest.json load failed - shop atlas sprites skipped\n");
    }

    if (FAILED(Load_TextureSRV(kPathInGameShopReference, &m_pSRV_InGameShopReference)))
        OutputDebugStringA("[UI_Manager] in-game shop reference texture load failed\n");

    LoadInGameShopItemTextures();

    const auto SelectedIt = std::find_if(
        m_InGameShopItems.begin(),
        m_InGameShopItems.end(),
        [](const InGameShopItemView& Item)
        {
            return Item.bEnabled && Item.bPurchasable && Item.iItemId != 0u;
        });
    m_iSelectedInGameShopItemId = SelectedIt != m_InGameShopItems.end()
        ? SelectedIt->iItemId
        : 0u;

    m_strInGameShopStatus = "Left click selects, right click buys";
}

void CUI_Manager::LoadInGameShopItemTextures()
{
    for (InGameShopItemView& Item : m_InGameShopItems)
    {
        ReleaseSRV(Item.pSRV);
        if (Item.strIconPath.empty())
        {
            wchar_t IconPath[MAX_PATH] = {};
            if (UI_TryFindItemIconPath(Item.iItemId, IconPath, MAX_PATH))
                Item.strIconPath = IconPath;
        }

        if (!Item.strIconPath.empty() &&
            FAILED(Load_TextureSRV(Item.strIconPath.c_str(), &Item.pSRV)))
        {
            Item.pSRV = nullptr;
        }
    }
}

void CUI_Manager::ReleaseInGameShopItemTextures()
{
    for (InGameShopItemView& Item : m_InGameShopItems)
        ReleaseSRV(Item.pSRV);
}

void CUI_Manager::DrawStatusPanel(ImDrawList* pDraw)
{
    const i32_t CurrentFrame = ImGui::GetFrameCount();
    const i32_t FramesSinceTuner = CurrentFrame - m_iStatusPanelLayoutTunerFrame;
    const bool_t bTunerPreviewActive =
        m_StatusPanelLayout.bPreviewPanel &&
        FramesSinceTuner >= 0 &&
        FramesSinceTuner <= 1;
    if (!pDraw || (!m_bStatusPanelOpen && !bTunerPreviewActive) || !m_pSRV_StatusPanel)
        return;

    constexpr f32_t kPanelW = 1491.f;
    constexpr f32_t kPanelH = 600.f;
    const ImVec2 Display = ImGui::GetIO().DisplaySize;
    if (Display.x <= 0.f || Display.y <= 0.f)
        return;

    f32_t DrawW = std::min(std::max(m_StatusPanelLayout.fPanelWidth, 320.f), Display.x);
    f32_t DrawH = m_StatusPanelLayout.bLockAssetAspect
        ? DrawW * (kPanelH / kPanelW)
        : std::max(m_StatusPanelLayout.fPanelHeight, 220.f);
    const f32_t MaxDrawH = Display.y * 0.92f;
    if (DrawH > MaxDrawH)
    {
        DrawH = MaxDrawH;
        if (m_StatusPanelLayout.bLockAssetAspect)
            DrawW = DrawH * (kPanelW / kPanelH);
    }

    const f32_t MaxDrawX = std::max(0.f, Display.x - DrawW);
    const f32_t MaxDrawY = std::max(0.f, Display.y - DrawH);
    const f32_t DrawX = std::clamp(
        (Display.x - DrawW) * 0.5f + m_StatusPanelLayout.fPanelOffsetX,
        0.f,
        MaxDrawX);
    const f32_t DrawY = std::clamp(
        (Display.y - DrawH) * 0.12f + m_StatusPanelLayout.fPanelOffsetY,
        0.f,
        MaxDrawY);
    const ImVec2 Root(DrawX, DrawY);
    const f32_t ScaleX = DrawW / kPanelW;
    const f32_t ScaleY = DrawH / kPanelH;

    pDraw->AddImage(
        reinterpret_cast<ImTextureID>(m_pSRV_StatusPanel),
        Root,
        ImVec2(DrawX + DrawW, DrawY + DrawH),
        ImVec2(0.f, 0.f),
        ImVec2(1.f, 1.f),
        IM_COL32(255, 255, 255, 255));

    std::vector<StatusPanelActorRow> BlueRows;
    std::vector<StatusPanelActorRow> RedRows;
    StatusPanelMatchScore Score{};
    BuildStatusPanelRows(BlueRows, RedRows, Score);
    DrawStatusPanelObjectiveScores(pDraw, Root, ScaleX, ScaleY, Score);
    DrawStatusPanelRows(pDraw, Root, ScaleX, ScaleY, BlueRows, true);
    DrawStatusPanelRows(pDraw, Root, ScaleX, ScaleY, RedRows, false);
}

void CUI_Manager::BuildStatusPanelRows(
    std::vector<StatusPanelActorRow>& BlueRows,
    std::vector<StatusPanelActorRow>& RedRows,
    StatusPanelMatchScore& Score) const
{
    BlueRows = m_StatusPanelBlueRows;
    RedRows = m_StatusPanelRedRows;
    Score = m_StatusPanelScore;
}

void CUI_Manager::DrawStatusPanelObjectiveScores(ImDrawList* pDraw, const ImVec2& Root,
    f32_t fScaleX, f32_t fScaleY, const StatusPanelMatchScore& Score)
{
    if (!pDraw)
        return;

    ImFont* pFont = FindUIFont("shop");
    if (!pFont)
        pFont = ImGui::GetFont();

    const f32_t FontSize = pFont->LegacySize * std::min(fScaleX, fScaleY) *
        m_StatusPanelLayout.fObjectiveFontScale;
    const ImU32 BlueColor = IM_COL32(84, 164, 223, 255);
    const ImU32 RedColor = IM_COL32(218, 76, 73, 255);
    const u16_t BlueValues[4] =
    {
        Score.iBlueDragons,
        Score.iBlueBarons,
        Score.iBlueDestroyedStructures,
        Score.iBlueDestroyedObjectives,
    };
    const u16_t RedValues[4] =
    {
        Score.iRedDestroyedObjectives,
        Score.iRedDestroyedStructures,
        Score.iRedBarons,
        Score.iRedDragons,
    };
    constexpr f32_t kBlueX[4] = { 220.f, 346.f, 474.f, 578.f };
    constexpr f32_t kRedX[4] = { 887.f, 1036.f, 1150.f, 1268.f };

    char Buffer[16]{};
    for (u32_t Index = 0; Index < 4u; ++Index)
    {
        std::snprintf(Buffer, sizeof(Buffer), "%u", static_cast<u32_t>(BlueValues[Index]));
        UI_DrawOutlinedText(
            pDraw,
            pFont,
            FontSize,
            ImVec2(
                Root.x + (kBlueX[Index] + m_StatusPanelLayout.fObjectiveOffsetX) * fScaleX,
                Root.y + m_StatusPanelLayout.fObjectiveY * fScaleY),
            BlueColor,
            Buffer);

        std::snprintf(Buffer, sizeof(Buffer), "%u", static_cast<u32_t>(RedValues[Index]));
        UI_DrawOutlinedText(
            pDraw,
            pFont,
            FontSize,
            ImVec2(
                Root.x + (kRedX[Index] - m_StatusPanelLayout.fObjectiveOffsetX) * fScaleX,
                Root.y + m_StatusPanelLayout.fObjectiveY * fScaleY),
            RedColor,
            Buffer);
    }
}

void CUI_Manager::DrawStatusPanelRows(ImDrawList* pDraw, const ImVec2& Root,
    f32_t fScaleX, f32_t fScaleY,
    const std::vector<StatusPanelActorRow>& Rows, bool_t bBlueSide)
{
    if (!pDraw)
        return;

    constexpr f32_t kAuthoredRowTop[5] = { 91.f, 187.f, 284.f, 381.f, 478.f };
    constexpr f32_t kAuthoredAverageSpacing = 96.75f;
    const u32_t Count = std::min<u32_t>(static_cast<u32_t>(Rows.size()), 5u);
    for (u32_t Index = 0; Index < Count; ++Index)
    {
        const f32_t AuthoredOffset = kAuthoredRowTop[Index] - kAuthoredRowTop[0];
        const f32_t RowTop = m_StatusPanelLayout.fRowStartY + AuthoredOffset *
            (m_StatusPanelLayout.fRowSpacingY / kAuthoredAverageSpacing);
        DrawStatusPanelActorRow(pDraw, Root, fScaleX, fScaleY, Rows[Index], RowTop, bBlueSide);
    }
}

void CUI_Manager::DrawStatusPanelActorRow(ImDrawList* pDraw, const ImVec2& Root,
    f32_t fScaleX, f32_t fScaleY,
    const StatusPanelActorRow& Row, f32_t fRowTop, bool_t bBlueSide)
{
    if (!pDraw)
        return;

    auto ToPanel = [&](f32_t X, f32_t Y) -> ImVec2
    {
        return ImVec2(Root.x + X * fScaleX, Root.y + Y * fScaleY);
    };

    ImFont* pFont = FindUIFont("shop.body");
    if (!pFont)
        pFont = ImGui::GetFont();

    const f32_t Scale = std::min(fScaleX, fScaleY);
    const f32_t SmallFontSize = pFont->LegacySize * Scale * m_StatusPanelLayout.fLevelFontScale;
    const f32_t KdaFontSize = pFont->LegacySize * Scale * m_StatusPanelLayout.fKdaFontScale;
    const ImU32 TextColor = IM_COL32(230, 224, 186, 255);
    const ImU32 MutedColor = IM_COL32(149, 143, 132, 255);
    const ImU32 BorderColor = bBlueSide
        ? IM_COL32(70, 146, 183, 225)
        : IM_COL32(160, 72, 78, 225);

    const f32_t BaseX = bBlueSide
        ? m_StatusPanelLayout.fBlueContentX
        : m_StatusPanelLayout.fRedContentX;
    const f32_t PortraitX = BaseX + m_StatusPanelLayout.fPortraitOffsetX;
    const f32_t PortraitY = fRowTop + m_StatusPanelLayout.fPortraitOffsetY;
    const ImVec2 PortraitMin = ToPanel(PortraitX, PortraitY);
    const ImVec2 PortraitMax = ToPanel(
        PortraitX + m_StatusPanelLayout.fPortraitSize,
        PortraitY + m_StatusPanelLayout.fPortraitSize);
    void* pPortrait = FindOrLoadKillFeedPortrait(Row.iActorContentId);
    if (pPortrait)
    {
        pDraw->AddImage(
            reinterpret_cast<ImTextureID>(pPortrait),
            PortraitMin,
            PortraitMax,
            ImVec2(0.f, 0.f),
            ImVec2(1.f, 1.f),
            IM_COL32(255, 255, 255, 255));
    }
    else
    {
        pDraw->AddRectFilled(PortraitMin, PortraitMax, IM_COL32(19, 28, 32, 235), 2.f);
    }
    pDraw->AddRect(PortraitMin, PortraitMax, BorderColor, 2.f, 0, 1.5f);

    char Buffer[64]{};
    std::snprintf(Buffer, sizeof(Buffer), "%u", static_cast<u32_t>(Row.iLevel));
    UI_DrawOutlinedText(
        pDraw,
        pFont,
        SmallFontSize,
        ToPanel(
            PortraitX + m_StatusPanelLayout.fLevelOffsetX,
            PortraitY + m_StatusPanelLayout.fLevelOffsetY),
        TextColor,
        Buffer);

    const f32_t SpellX = BaseX + m_StatusPanelLayout.fSpellOffsetX;
    for (u32_t Index = 0; Index < Row.SummonerSpellIds.size(); ++Index)
    {
        const f32_t SpellY = fRowTop + m_StatusPanelLayout.fSpellOffsetY +
            static_cast<f32_t>(Index) * m_StatusPanelLayout.fSpellSpacingY;
        const ImVec2 SpellMin = ToPanel(SpellX, SpellY);
        const ImVec2 SpellMax = ToPanel(
            SpellX + m_StatusPanelLayout.fSpellSize,
            SpellY + m_StatusPanelLayout.fSpellSize);
        void* pSpellSRV = FindOrLoadStatusPanelSummonerSpellIcon(Row.SummonerSpellIds[Index]);
        if (pSpellSRV)
        {
            pDraw->AddImage(
                reinterpret_cast<ImTextureID>(pSpellSRV),
                SpellMin,
                SpellMax,
                ImVec2(0.f, 0.f),
                ImVec2(1.f, 1.f),
                IM_COL32(255, 255, 255, 255));
        }
        else
        {
            pDraw->AddRectFilled(SpellMin, SpellMax, IM_COL32(28, 38, 43, 235), 2.f);
            std::snprintf(Buffer, sizeof(Buffer), "%u", static_cast<u32_t>(Row.SummonerSpellIds[Index]));
            UI_DrawOutlinedText(pDraw, pFont, SmallFontSize * 0.7f,
                ImVec2(SpellMin.x + 3.f * Scale, SpellMin.y + 5.f * Scale),
                TextColor, Buffer);
        }
        pDraw->AddRect(SpellMin, SpellMax, IM_COL32(94, 85, 64, 230), 2.f, 0, 1.f);

        const f32_t Cooldown = Row.SummonerCooldowns[Index];
        const f32_t Duration = Row.SummonerCooldownDurations[Index];
        if (Cooldown > 0.f)
        {
            const f32_t Ratio = (Duration > 0.f) ? (Cooldown / Duration) : 1.f;
            UI_DrawCooldownPie(
                pDraw,
                ImVec2((SpellMin.x + SpellMax.x) * 0.5f, (SpellMin.y + SpellMax.y) * 0.5f),
                (SpellMax.x - SpellMin.x) * 0.5f,
                Ratio);
            std::snprintf(Buffer, sizeof(Buffer), "%.0f", Cooldown);
            const ImVec2 TextSize = pFont->CalcTextSizeA(SmallFontSize * 0.78f, FLT_MAX, 0.f, Buffer);
            UI_DrawOutlinedText(
                pDraw,
                pFont,
                SmallFontSize * 0.78f,
                ImVec2(
                    (SpellMin.x + SpellMax.x - TextSize.x) * 0.5f,
                    (SpellMin.y + SpellMax.y - TextSize.y) * 0.5f),
                IM_COL32(245, 245, 245, 255),
                Buffer);
        }
    }

    std::snprintf(Buffer, sizeof(Buffer), "%u / %u / %u",
        static_cast<u32_t>(Row.iKills),
        static_cast<u32_t>(Row.iDeaths),
        static_cast<u32_t>(Row.iAssists));
    UI_DrawOutlinedText(
        pDraw,
        pFont,
        KdaFontSize,
        ToPanel(
            BaseX + m_StatusPanelLayout.fKdaOffsetX,
            fRowTop + m_StatusPanelLayout.fKdaOffsetY),
        TextColor,
        Buffer);

    const f32_t ItemStartX = BaseX + m_StatusPanelLayout.fItemOffsetX;
    const f32_t ItemY = fRowTop + m_StatusPanelLayout.fItemOffsetY;
    for (u32_t Index = 0; Index < Row.InventoryItemIds.size(); ++Index)
    {
        const f32_t ItemX = ItemStartX +
            static_cast<f32_t>(Index) * m_StatusPanelLayout.fItemSpacingX;
        const ImVec2 ItemMin = ToPanel(ItemX, ItemY);
        const ImVec2 ItemMax = ToPanel(
            ItemX + m_StatusPanelLayout.fItemSize,
            ItemY + m_StatusPanelLayout.fItemSize);
        pDraw->AddRectFilled(ItemMin, ItemMax, IM_COL32(10, 15, 19, 130), 2.f);
        pDraw->AddRect(ItemMin, ItemMax, IM_COL32(72, 64, 48, 165), 2.f, 0, 1.f);

        const u16_t ItemId = Row.InventoryItemIds[Index];
        if (ItemId == 0)
            continue;

        // Item slots draw inventory icons only; item cooldown state is not part of this panel data.
        const InGameShopItemView* pItemView = FindInGameShopItem(ItemId);
        if (pItemView && pItemView->pSRV)
        {
            pDraw->AddImage(
                reinterpret_cast<ImTextureID>(pItemView->pSRV),
                ItemMin,
                ItemMax,
                ImVec2(0.f, 0.f),
                ImVec2(1.f, 1.f),
                IM_COL32(255, 255, 255, 255));
        }
        else
        {
            std::snprintf(Buffer, sizeof(Buffer), "%u", static_cast<u32_t>(ItemId));
            UI_DrawOutlinedText(
                pDraw,
                pFont,
                SmallFontSize * 0.7f,
                ImVec2(ItemMin.x + 2.f * Scale, ItemMin.y + 9.f * Scale),
                MutedColor,
                Buffer);
        }
    }
}

const wchar_t* CUI_Manager::FindStatusPanelSpellIconPath(u16_t iSpellId) const
{
    for (const StatusPanelIconAssetDef& Def : m_StatusPanelSpellIconAssets)
    {
        if (Def.iContentId == iSpellId)
            return Def.IconPath();
    }
    return nullptr;
}

void CUI_Manager::ReleaseStatusPanelSpellIconCache()
{
    for (StatusPanelSpellIconCache& SpellIcon : m_StatusPanelSpellIconCache)
        ReleaseSRV(SpellIcon.pSRV);
    m_StatusPanelSpellIconCache.clear();
}

void* CUI_Manager::FindOrLoadStatusPanelSummonerSpellIcon(u16_t iSpellId)
{
    if (iSpellId == 0)
        return nullptr;

    for (const StatusPanelSpellIconCache& SpellIcon : m_StatusPanelSpellIconCache)
    {
        if (SpellIcon.iSpellId == iSpellId)
            return SpellIcon.pSRV;
    }

    const wchar_t* pPath = FindStatusPanelSpellIconPath(iSpellId);

    void* pSRV = nullptr;
    if (pPath)
        (void)Load_TextureSRV(pPath, &pSRV);

    StatusPanelSpellIconCache Cache{};
    Cache.iSpellId = iSpellId;
    Cache.pSRV = pSRV;
    m_StatusPanelSpellIconCache.push_back(Cache);
    return pSRV;
}

void CUI_Manager::DrawInGameShop(ImDrawList* pDraw)
{
    if (!pDraw || !m_bInGameShopOpen || m_InGameShopItems.empty())
        return;

    const ImVec2 Display = ImGui::GetIO().DisplaySize;
    constexpr f32_t kShopReferenceW = 1165.f;
    constexpr f32_t kShopReferenceH = 736.f;
    f32_t AvailableH = Display.y - m_fHUDHeight - 2.f;
    if (AvailableH < 360.f)
        AvailableH = Display.y - 24.f;

    f32_t Scale = 1.f;
    if (Display.x > 0.f)
        Scale = std::min(Scale, Display.x / kShopReferenceW);
    if (AvailableH > 0.f)
        Scale = std::min(Scale, AvailableH / kShopReferenceH);
    if (Scale <= 0.f)
        Scale = 1.f;

    const ImVec2 ShopMin(0.f, 0.f);
    const ImVec2 ShopMax(
        ShopMin.x + kShopReferenceW * Scale,
        ShopMin.y + kShopReferenceH * Scale);

    auto ToShop = [&](f32_t X, f32_t Y) -> ImVec2
    {
        return ImVec2(ShopMin.x + X * Scale, ShopMin.y + Y * Scale);
    };
    auto DrawSprite = [&](const char* pID, f32_t X, f32_t Y, f32_t W, f32_t H, f32_t Alpha = 1.f) -> bool_t
    {
        return UI_DrawManifestSprite(
            pDraw,
            m_InGameShopAtlasManifest,
            pID,
            ToShop(X, Y),
            ToShop(X + W, Y + H),
            Alpha);
    };
    auto PointInRect = [](const ImVec2& P, const ImVec2& A, const ImVec2& B) -> bool_t
    {
        return P.x >= A.x && P.x <= B.x && P.y >= A.y && P.y <= B.y;
    };

    pDraw->AddRectFilled(ShopMin, ShopMax, IM_COL32(0, 9, 12, 138));
    const bool_t bAtlasBaseDrawn = DrawSprite("main.panel", 102.f, 3.f, 632.f, 694.f, 0.98f);
    DrawSprite("main.panel", 734.f, 3.f, 408.f, 694.f, 0.88f);
    DrawSprite("left.panel", 16.f, 52.f, 84.f, 575.f, 0.98f);
    DrawSprite("bottom.bag.panel", 16.f, 636.f, 84.f, 93.f, 0.98f);
    DrawSprite("tab.active", 103.f, 3.f, 630.f, 50.f, 0.96f);
    DrawSprite("header.strip", 103.f, 52.f, 630.f, 43.f, 0.94f);
    DrawSprite("search.box", 113.f, 70.f, 610.f, 32.f, 0.96f);
    DrawSprite("search.icon", 122.f, 78.f, 20.f, 20.f, 0.88f);
    DrawSprite("close.x", 1109.f, 16.f, 18.f, 18.f, 0.88f);

    if (!bAtlasBaseDrawn)
    {
        pDraw->AddRectFilled(ToShop(102.f, 3.f), ToShop(1142.f, 697.f), IM_COL32(0, 28, 35, 226));
        pDraw->AddRect(ToShop(102.f, 3.f), ToShop(1142.f, 697.f), IM_COL32(181, 145, 75, 255), 0.f, 0, 2.f);
    }

    if (m_pSRV_InGameShopReference)
    {
        pDraw->AddImage(
            reinterpret_cast<ImTextureID>(m_pSRV_InGameShopReference),
            ShopMin,
            ShopMax,
            ImVec2(0.f, 0.f),
            ImVec2(1.f, 1.f),
            IM_COL32(255, 255, 255, static_cast<i32_t>(UI_Clamp01(m_fInGameShopReferenceAlpha) * 255.f)));
    }

    ImFont* pFont = FindUIFont("shop.body");
    if (!pFont)
        pFont = ImGui::GetFont();
    ImFont* pTitleFont = FindUIFont("shop");
    if (!pTitleFont)
        pTitleFont = pFont;
    const ImGuiIO& IO = ImGui::GetIO();
    const f32_t TitleFontSize = pTitleFont->LegacySize * Scale * 0.84f;
    const f32_t FontSize = pFont->LegacySize * Scale * 0.78f;
    const f32_t SmallFontSize = pFont->LegacySize * Scale * 0.68f;
    const ImU32 NormalColor = IM_COL32(230, 224, 186, 255);
    const ImU32 HoverColor = IM_COL32(255, 241, 151, 255);
    const ImU32 MutedColor = IM_COL32(157, 150, 132, 255);
    constexpr f32_t kIconSize = 40.f;
    constexpr f32_t kIconStepX = 56.f;
    constexpr f32_t kRowStepY = 114.f;
    constexpr f32_t kStartX = 172.f;
    constexpr f32_t kStartY = 200.f;

    UI_DrawOutlinedText(pDraw, pTitleFont, TitleFontSize, ToShop(190.f, 21.f), NormalColor, "Recommended");
    UI_DrawOutlinedText(pDraw, pTitleFont, TitleFontSize, ToShop(371.f, 21.f), HoverColor, "All Items");
    UI_DrawOutlinedText(pDraw, pTitleFont, TitleFontSize, ToShop(581.f, 21.f), NormalColor, "Item Sets");
    UI_DrawOutlinedText(pDraw, pTitleFont, TitleFontSize, ToShop(755.f, 27.f), MutedColor, "Owned Items");
    UI_DrawOutlinedText(pDraw, pFont, SmallFontSize, ToShop(150.f, 80.f), MutedColor, "Click to search");
    UI_DrawOutlinedText(pDraw, pTitleFont, TitleFontSize, ToShop(170.f, 174.f), NormalColor, "Starter");
    UI_DrawOutlinedText(pDraw, pTitleFont, TitleFontSize, ToShop(170.f, 288.f), NormalColor, "Basic");

    static constexpr const char* kCategorySprites[] =
    {
        "category.grid",
        "category.star",
        "category.book",
        "category.sword",
        "category.shield",
        "category.boot",
    };
    for (u32_t i = 0; i < 6; ++i)
    {
        const f32_t X = 123.f + static_cast<f32_t>(i) * 46.f;
        const f32_t Size = (i == 1u) ? 26.f : 22.f;
        DrawSprite(kCategorySprites[i], X, 121.f, Size, Size, i == 1u ? 1.f : 0.82f);
        if (i == 1u)
            pDraw->AddLine(ToShop(X - 4.f, 154.f), ToShop(X + 30.f, 154.f), IM_COL32(221, 178, 82, 255), 2.f * Scale);
    }

    UI_DrawOutlinedText(
        pDraw,
        pFont,
        SmallFontSize,
        ToShop(752.f, 438.f),
        IM_COL32(245, 231, 177, 255),
        m_strInGameShopStatus.c_str());

    char GoldText[32]{};
    std::snprintf(GoldText, sizeof(GoldText), "%u", static_cast<u32_t>(m_iInGameGold));
    UI_DrawOutlinedText(
        pDraw,
        pFont,
        FontSize,
        ToShop(383.f, 708.f),
        IM_COL32(245, 231, 177, 255),
        GoldText);

    DrawSprite("gold.coin", 346.f, 708.f, 18.f, 15.f, 0.95f);

    for (u32_t Index = 0; Index < m_InGameInventorySlots.size(); ++Index)
    {
        const f32_t SlotX = 754.f + static_cast<f32_t>(Index) * 54.f;
        const f32_t SlotY = 54.f;
        DrawSprite("slot.frame", SlotX, SlotY, 40.f, 40.f, 0.82f);

        const u16_t ItemId = m_InGameInventorySlots[Index];
        if (ItemId == 0)
            continue;

        const InGameShopItemView* pItemView = FindInGameShopItem(ItemId);
        if (pItemView && pItemView->pSRV)
        {
            pDraw->AddImage(
                reinterpret_cast<ImTextureID>(pItemView->pSRV),
                ToShop(SlotX + 3.f, SlotY + 3.f),
                ToShop(SlotX + 37.f, SlotY + 37.f),
                ImVec2(0.f, 0.f),
                ImVec2(1.f, 1.f),
                IM_COL32(255, 255, 255, 255));
        }
    }

    u32_t DisplayIndex = 0u;
    for (u32_t Index = 0; Index < m_InGameShopItems.size(); ++Index)
    {
        const InGameShopItemView& Item = m_InGameShopItems[Index];
        if (!Item.bEnabled || !Item.bPurchasable)
            continue;

        const u32_t Row = DisplayIndex / 7u;
        const u32_t Col = DisplayIndex % 7u;
        if (Row >= 2u)
            break;
        ++DisplayIndex;

        const f32_t X = kStartX + static_cast<f32_t>(Col) * kIconStepX;
        const f32_t Y = kStartY + static_cast<f32_t>(Row) * kRowStepY;
        const ImVec2 CellMin = ToShop(X, Y);
        const ImVec2 CellMax = ToShop(X + kIconSize, Y + kIconSize);
        const bool_t bHovered = PointInRect(IO.MousePos, CellMin, CellMax);

        if (bHovered && IO.MouseClicked[0])
        {
            m_iSelectedInGameShopItemId = Item.iItemId;
            m_strInGameShopStatus = !Item.strName.empty() ? Item.strName : "Item selected";
        }
        if (bHovered && IO.MouseClicked[1])
        {
            m_iSelectedInGameShopItemId = Item.iItemId;
            TryBuyInGameItem(Item.iItemId);
        }

        DrawSprite("slot.frame", X - 1.f, Y - 1.f, kIconSize + 2.f, kIconSize + 2.f, 0.86f);
        if (Item.pSRV)
        {
            pDraw->AddImage(
                reinterpret_cast<ImTextureID>(Item.pSRV),
                CellMin,
                CellMax,
                ImVec2(0.f, 0.f),
                ImVec2(1.f, 1.f),
                IM_COL32(255, 255, 255, 255));
        }
        else
        {
            pDraw->AddRectFilled(CellMin, CellMax, IM_COL32(9, 29, 35, 236));
            char FallbackText[16]{};
            std::snprintf(FallbackText, sizeof(FallbackText), "%u", static_cast<u32_t>(Item.iItemId));
            UI_DrawOutlinedText(
                pDraw,
                pFont,
                SmallFontSize,
                ImVec2(CellMin.x + 2.f * Scale, CellMin.y + 10.f * Scale),
                bHovered ? HoverColor : NormalColor,
                FallbackText);
        }

        pDraw->AddRect(
            CellMin,
            CellMax,
            (bHovered || m_iSelectedInGameShopItemId == Item.iItemId)
                ? IM_COL32(244, 211, 126, 255)
                : IM_COL32(92, 85, 80, 210),
            0.f,
            0,
            (bHovered || m_iSelectedInGameShopItemId == Item.iItemId) ? 2.f : 1.f);

        char PriceText[16]{};
        std::snprintf(PriceText, sizeof(PriceText), "%u", static_cast<u32_t>(Item.iPrice));
        const ImVec2 PriceSize = pFont->CalcTextSizeA(SmallFontSize, FLT_MAX, 0.f, PriceText);
        ImVec2 PricePos = ToShop(X + kIconSize * 0.5f, Y + 44.f);
        PricePos.x -= PriceSize.x * 0.5f;
        UI_DrawOutlinedText(
            pDraw,
            pFont,
            SmallFontSize,
            PricePos,
            bHovered ? HoverColor : NormalColor,
            PriceText);
    }

    const InGameShopItemView* pSelectedItem = FindInGameShopItem(m_iSelectedInGameShopItemId);
    if (!pSelectedItem)
    {
        const auto SelectedIt = std::find_if(
            m_InGameShopItems.begin(),
            m_InGameShopItems.end(),
            [](const InGameShopItemView& Item)
            {
                return Item.bEnabled && Item.bPurchasable && Item.iItemId != 0u;
            });
        if (SelectedIt != m_InGameShopItems.end())
            pSelectedItem = &*SelectedIt;
    }

    if (pSelectedItem)
    {
        DrawSprite("slot.frame", 754.f, 126.f, 54.f, 54.f, 0.95f);
        if (pSelectedItem->pSRV)
        {
            pDraw->AddImage(
                reinterpret_cast<ImTextureID>(pSelectedItem->pSRV),
                ToShop(760.f, 132.f),
                ToShop(802.f, 174.f),
                ImVec2(0.f, 0.f),
                ImVec2(1.f, 1.f),
                IM_COL32(255, 255, 255, 255));
        }

        UI_DrawOutlinedText(
            pDraw,
            pTitleFont,
            TitleFontSize,
            ToShop(820.f, 130.f),
            NormalColor,
            !pSelectedItem->strName.empty() ? pSelectedItem->strName.c_str() : "Selected Item");

        char DetailPriceText[32]{};
        std::snprintf(DetailPriceText, sizeof(DetailPriceText), "%u", static_cast<u32_t>(pSelectedItem->iPrice));
        UI_DrawOutlinedText(
            pDraw,
            pFont,
            FontSize,
            ToShop(820.f, 158.f),
            IM_COL32(255, 217, 91, 255),
            DetailPriceText);

        for (u32_t LineIndex = 0; LineIndex < pSelectedItem->StatLines.size() && LineIndex < 8u; ++LineIndex)
        {
            UI_DrawOutlinedText(
                pDraw,
                pFont,
                SmallFontSize,
                ToShop(820.f, 190.f + static_cast<f32_t>(LineIndex) * 18.f),
                MutedColor,
                pSelectedItem->StatLines[LineIndex].c_str());
        }

        const ImVec2 BuyMin = ToShop(751.f, 431.f);
        const ImVec2 BuyMax = ToShop(1119.f, 460.f);
        const bool_t bBuyHovered = PointInRect(IO.MousePos, BuyMin, BuyMax);
        if (!DrawSprite(bBuyHovered ? "button.long" : "button.dim", 751.f, 431.f, 368.f, 29.f, 0.94f))
        {
            pDraw->AddRectFilled(BuyMin, BuyMax, bBuyHovered ? IM_COL32(43, 48, 49, 244) : IM_COL32(34, 36, 38, 235));
            pDraw->AddRect(BuyMin, BuyMax, IM_COL32(170, 154, 125, 220));
        }

        const char* pBuyText = "BUY";
        const ImVec2 BuyTextSize = pFont->CalcTextSizeA(FontSize, FLT_MAX, 0.f, pBuyText);
        UI_DrawOutlinedText(
            pDraw,
            pFont,
            FontSize,
            ImVec2((BuyMin.x + BuyMax.x - BuyTextSize.x) * 0.5f, (BuyMin.y + BuyMax.y - BuyTextSize.y) * 0.5f),
            bBuyHovered ? HoverColor : NormalColor,
            pBuyText);

        if (bBuyHovered && IO.MouseClicked[0])
            TryBuyInGameItem(pSelectedItem->iItemId);
    }
}

const CUI_Manager::InGameShopItemView* CUI_Manager::FindInGameShopItem(u16_t iItemId) const
{
    for (const InGameShopItemView& Item : m_InGameShopItems)
    {
        if (Item.iItemId == iItemId)
            return &Item;
    }

    return nullptr;
}


bool_t CUI_Manager::TryBuyInGameItem(u16_t iItemId)
{
    if (iItemId == 0 || !m_pfnBuyItem)
    {
        OutputDebugStringA("[UI_Manager] BuyItem command callback missing\n");
        return false;
    }

    const InGameShopItemView* pItem = FindInGameShopItem(iItemId);
    if (!pItem || !pItem->bEnabled || !pItem->bPurchasable)
    {
        OutputDebugStringA("[UI_Manager] BuyItem rejected: item is not purchasable\n");
        return false;
    }

    m_pfnBuyItem(m_pBuyItemUser, iItemId);
    m_strInGameShopStatus = "Buy requested";
    OutputDebugStringA("[UI_Manager] BuyItem command requested\n");
    return true;
}

void CUI_Manager::Push_DamageNumber(const Vec3& vWorldPos, f32_t fAmount,
    u8_t iDamageType, bool_t bWasCrit, bool_t bKilled,
    bool_t bShowCriticalIndicator)
{
    if (fAmount <= 0.f)
        return;

    if (m_DamageFloaters.size() >= 128)
        m_DamageFloaters.erase(m_DamageFloaters.begin());

    DamageFloater floater{};
    floater.vWorldPos = vWorldPos;
    floater.fAmount = fAmount;
    floater.fLifetime = bKilled ? (m_fDamageFloaterLife + 0.25f) : m_fDamageFloaterLife;
    floater.iDamageType = iDamageType;
    floater.bWasCrit = bWasCrit;
    floater.bShowCriticalIndicator = bShowCriticalIndicator;
    floater.bKilled = bKilled;

    const u32_t seed =
        static_cast<u32_t>(m_DamageFloaters.size() * 37u) ^
        static_cast<u32_t>(fAmount * 17.f);
    floater.fXJitter = static_cast<f32_t>(static_cast<i32_t>(seed % 17u) - 8) * 1.5f;

    m_DamageFloaters.push_back(floater);
}

void CUI_Manager::Push_WorldText(const Vec3& vWorldPos, const char* pText,
    const Vec4& vColor, f32_t fLifetime)
{
    if (!pText || pText[0] == '\0')
        return;

    if (m_WorldTextFloaters.size() >= 128)
        m_WorldTextFloaters.erase(m_WorldTextFloaters.begin());

    WorldTextFloater floater{};
    floater.vWorldPos = vWorldPos;
    floater.strText = pText;
    floater.vColor = vColor;
    floater.fLifetime = std::max(0.2f, fLifetime);

    const u32_t seed =
        static_cast<u32_t>(m_WorldTextFloaters.size() * 53u) ^
        static_cast<u32_t>(floater.strText.size() * 19u);
    floater.fXJitter = static_cast<f32_t>(static_cast<i32_t>(seed % 13u) - 6) * 1.25f;

    m_WorldTextFloaters.push_back(floater);
}

void CUI_Manager::Push_GoldText(const Vec3& vWorldPos, u32_t iGoldAmount,
    f32_t fLifetime)
{
    if (iGoldAmount == 0u)
        return;

    if (m_WorldTextFloaters.size() >= 128)
        m_WorldTextFloaters.erase(m_WorldTextFloaters.begin());

    char text[32]{};
    std::snprintf(text, sizeof(text), "+%u", static_cast<unsigned int>(iGoldAmount));

    WorldTextFloater floater{};
    floater.vWorldPos = vWorldPos;
    floater.strText = text;
    floater.vColor = Vec4{ 1.f, 0.86f, 0.32f, 1.f };
    floater.fLifetime = std::max(0.2f, fLifetime);
    const u32_t seed =
        static_cast<u32_t>(m_WorldTextFloaters.size() * 59u) ^
        static_cast<u32_t>(iGoldAmount * 23u);
    floater.fXJitter =
        static_cast<f32_t>(static_cast<i32_t>(seed % 13u) - 6) * 1.25f;
    floater.iGoldAmount = iGoldAmount;
    floater.bShowGoldIcon = true;

    m_WorldTextFloaters.push_back(floater);
}

void CUI_Manager::Push_KillFeedBanner(u8_t iSourceActorContentId, u8_t iTargetActorContentId,
    u8_t iObjectKind, u8_t iTargetTeam, bool_t bSourceAlly,
    bool_t bSourceMinion, const char* pMessage)
{
    if (!pMessage || pMessage[0] == '\0')
        return;

    if (m_KillFeedBanners.size() >= 5)
        m_KillFeedBanners.erase(m_KillFeedBanners.begin());

    KillFeedBanner banner{};
    banner.iSourceActorContentId = iSourceActorContentId;
    banner.iTargetActorContentId = iTargetActorContentId;
    banner.iObjectKind = iObjectKind;
    banner.iTargetTeam = iTargetTeam;
    banner.bSourceAlly = bSourceAlly;
    banner.bSourceMinion = bSourceMinion;
    banner.strMessage = pMessage;
    m_KillFeedBanners.push_back(banner);
}

void CUI_Manager::RecordMatchContextActorKill(u8_t iSourceTeam, u8_t iTargetTeam,
    bool_t bLocalSource, bool_t bLocalTarget)
{
    (void)iTargetTeam;

    if (iSourceTeam == kUITeamBlue)
        ++m_MatchContextHUD.iBlueKills;
    else if (iSourceTeam == kUITeamRed)
        ++m_MatchContextHUD.iRedKills;

    if (bLocalSource)
        ++m_MatchContextHUD.iLocalKills;
    if (bLocalTarget)
        ++m_MatchContextHUD.iLocalDeaths;
}

void CUI_Manager::RecordMatchContextUnitKill()
{
    ++m_MatchContextHUD.iLocalUnitKills;
}

void CUI_Manager::SetMatchContextServerTimeMs(u64_t iServerTimeMs)
{
    m_MatchContextHUD.iServerTimeMs = iServerTimeMs;
    m_MatchContextHUD.bHasServerTime = true;
}

void CUI_Manager::ResetMatchContextHUDStats()
{
    m_MatchContextHUD = MatchContextHUDState{};
}

void CUI_Manager::DrawMatchContextHUD(ImDrawList* pDraw)
{
    if (!m_bShowMatchContextHUD || !pDraw)
        return;

    ImFont* pFont = FindUIFont("hud");
    if (!pFont)
        return;

    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    const f32_t screenW = (m_iWinSizeX > 0) ? static_cast<f32_t>(m_iWinSizeX) : displaySize.x;
    const f32_t screenH = (m_iWinSizeY > 0) ? static_cast<f32_t>(m_iWinSizeY) : displaySize.y;
    if (screenW <= 0.f || screenH <= 0.f)
        return;

    const f32_t hudW = m_fMatchContextHUDWidth;
    const f32_t hudH = m_fMatchContextHUDHeight;
    const ImVec2 root(screenW - hudW - 18.f, 8.f);
    const ImVec2 max(root.x + hudW, root.y + hudH);

    if (!UI_DrawManifestSprite(pDraw, m_HudAtlasManifest, "gamecontext.background", root, max, 1.f))
        pDraw->AddRectFilled(root, max, IM_COL32(14, 28, 34, 210), 0.f);

    constexpr f32_t kFontSize = 16.f;
    constexpr f32_t kSmallFontSize = 15.f;
    const ImU32 BlueColor = IM_COL32(112, 178, 255, 255);
    const ImU32 RedColor = IM_COL32(245, 104, 104, 255);
    const ImU32 TextColor = IM_COL32(238, 224, 177, 255);

    u16_t iBlueKills = m_MatchContextHUD.iBlueKills;
    u16_t iRedKills = m_MatchContextHUD.iRedKills;
    u16_t iLocalKills = m_MatchContextHUD.iLocalKills;
    u16_t iLocalDeaths = m_MatchContextHUD.iLocalDeaths;
    u16_t iLocalAssists = m_MatchContextHUD.iLocalAssists;


    char buffer[32]{};
    std::snprintf(buffer, sizeof(buffer), "%u", static_cast<u32_t>(iBlueKills));
    UI_DrawOutlinedText(pDraw, pFont, kFontSize, ImVec2(root.x + 26.f, root.y + 8.f), BlueColor, buffer);

    UI_DrawManifestSprite(
        pDraw,
        m_HudAtlasManifest,
        "gamecontext.vs",
        ImVec2(root.x + 55.f, root.y + 7.f),
        ImVec2(root.x + 79.f, root.y + 26.f),
        1.f);

    std::snprintf(buffer, sizeof(buffer), "%u", static_cast<u32_t>(iRedKills));
    UI_DrawOutlinedText(pDraw, pFont, kFontSize, ImVec2(root.x + 90.f, root.y + 8.f), RedColor, buffer);

    UI_DrawManifestSprite(
        pDraw,
        m_HudAtlasManifest,
        "gamecontext.kda.icon",
        ImVec2(root.x + 130.f, root.y + 8.f),
        ImVec2(root.x + 146.f, root.y + 27.f),
        1.f);
    std::snprintf(buffer, sizeof(buffer), "%u/%u/%u",
        static_cast<u32_t>(iLocalKills),
        static_cast<u32_t>(iLocalDeaths),
        static_cast<u32_t>(iLocalAssists));
    UI_DrawOutlinedText(pDraw, pFont, kSmallFontSize, ImVec2(root.x + 153.f, root.y + 9.f), TextColor, buffer);

    UI_DrawManifestSprite(
        pDraw,
        m_HudAtlasManifest,
        "gamecontext.minion.icon",
        ImVec2(root.x + 222.f, root.y + 5.f),
        ImVec2(root.x + 243.f, root.y + 30.f),
        1.f);
    std::snprintf(buffer, sizeof(buffer), "%u",
        static_cast<u32_t>(m_MatchContextHUD.iLocalUnitKills));
    UI_DrawOutlinedText(pDraw, pFont, kSmallFontSize, ImVec2(root.x + 250.f, root.y + 9.f), TextColor, buffer);

    UI_DrawManifestSprite(
        pDraw,
        m_HudAtlasManifest,
        "gamecontext.time.icon",
        ImVec2(root.x + 296.f, root.y + 8.f),
        ImVec2(root.x + 316.f, root.y + 26.f),
        1.f);
    const u64_t totalSec = m_MatchContextHUD.bHasServerTime
        ? (m_MatchContextHUD.iServerTimeMs / 1000ull)
        : 0ull;
    std::snprintf(buffer, sizeof(buffer), "%llu:%02llu",
        static_cast<unsigned long long>(totalSec / 60ull),
        static_cast<unsigned long long>(totalSec % 60ull));
    UI_DrawOutlinedText(pDraw, pFont, kSmallFontSize, ImVec2(root.x + 322.f, root.y + 9.f), TextColor, buffer);
}

void* CUI_Manager::FindOrLoadKillFeedPortrait(u8_t iActorContentId)
{
    if (!IsValidUIContentId(iActorContentId))
        return nullptr;

    for (const KillFeedPortraitCache& Portrait : m_KillFeedPortraits)
    {
        if (Portrait.iActorContentId == iActorContentId)
            return Portrait.pSRV;
    }

    void* pSRV = nullptr;
    if (const ActorHudAssetDef* pDef = FindActorHudAssets(iActorContentId))
    {
        if (pDef->PortraitPath())
            (void)Load_TextureSRV(pDef->PortraitPath(), &pSRV);
    }

    KillFeedPortraitCache cache{};
    cache.iActorContentId = iActorContentId;
    cache.pSRV = pSRV;
    m_KillFeedPortraits.push_back(cache);
    return pSRV;
}

ImVec2 UI_StatsPanelAtlasUV(f32_t x, f32_t y)
{
    constexpr f32_t kAtlasSize = 512.f;
    return ImVec2(x / kAtlasSize, y / kAtlasSize);
}

void* CUI_Manager::FindOrLoadKillFeedObjectIcon(
    u8_t iObjectKind, u8_t iTargetTeam)
{
    void** ppSRV = nullptr;
    const wchar_t* pPath = nullptr;
    if (iObjectKind == kKillFeedObjectStructure)
    {
        if (iTargetTeam == kUITeamBlue)
        {
            ppSRV = &m_pSRV_KillFeedTowerBlue;
            pPath = kPathKillFeedTowerBlue;
        }
        else if (iTargetTeam == kUITeamRed)
        {
            ppSRV = &m_pSRV_KillFeedTowerRed;
            pPath = kPathKillFeedTowerRed;
        }
    }

    else if (iObjectKind == kKillFeedObjectObjective)
    {
        if (iTargetTeam == kUITeamBlue)
        {
            ppSRV = &m_pSRV_KillFeedInhibitorBlue;
            pPath = kPathKillFeedInhibitorBlue;
        }
        else if (iTargetTeam == kUITeamRed)
        {
            ppSRV = &m_pSRV_KillFeedInhibitorRed;
            pPath = kPathKillFeedInhibitorRed;
        }
    }

    if (!ppSRV || !pPath)
        return nullptr;
    if (!*ppSRV)
        (void)Load_TextureSRV(pPath, ppSRV);
    return *ppSRV;
}

void CUI_Manager::DrawKillFeedCircleImage(ImDrawList* pDraw, const ImVec2& vCenter,
    f32_t fRadius, void* pSRV, ImU32 iTintColor, ImU32 iBorderColor)
{
    if (!pDraw || fRadius <= 0.f)
        return;

    const ImVec2 min(vCenter.x - fRadius, vCenter.y - fRadius);
    const ImVec2 max(vCenter.x + fRadius, vCenter.y + fRadius);
    if (pSRV)
    {
        pDraw->AddImageRounded(
            pSRV,
            min,
            max,
            ImVec2(0.f, 0.f),
            ImVec2(1.f, 1.f),
            iTintColor,
            fRadius,
            ImDrawFlags_RoundCornersAll);
    }
    else
    {
        pDraw->AddCircleFilled(vCenter, fRadius, IM_COL32(24, 28, 34, 220), 48);
    }
    pDraw->AddCircle(vCenter, fRadius, iBorderColor, 48, 2.5f);
}

void CUI_Manager::DrawKillFeedBanners(ImDrawList* pDraw, f32_t fDeltaTime)
{
    if (!pDraw)
        return;

    if (fDeltaTime < 0.f)
        fDeltaTime = 0.f;
    if (fDeltaTime > 0.1f)
        fDeltaTime = 0.1f;

    for (KillFeedBanner& banner : m_KillFeedBanners)
        banner.fAge += fDeltaTime;

    m_KillFeedBanners.erase(
        std::remove_if(m_KillFeedBanners.begin(), m_KillFeedBanners.end(),
            [](const KillFeedBanner& banner)
            {
                return banner.fAge >= banner.fLifetime;
            }),
        m_KillFeedBanners.end());

    if (m_KillFeedBanners.empty())
        return;

    ImFont* pFont = FindUIFont("fallback");
    if (!pFont)
        return;

    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    const f32_t screenW = (m_iWinSizeX > 0) ? static_cast<f32_t>(m_iWinSizeX) : displaySize.x;
    const f32_t screenH = (m_iWinSizeY > 0) ? static_cast<f32_t>(m_iWinSizeY) : displaySize.y;
    if (screenW <= 0.f || screenH <= 0.f)
        return;

    constexpr f32_t kRadius = 28.f;
    constexpr f32_t kGap = 16.f;
    constexpr f32_t kFontSize = 24.f;
    constexpr f32_t kLabelFontSize = 17.f;
    constexpr f32_t kRowHeight = 76.f;
    const f32_t centerX = screenW * 0.5f;
    const f32_t baseY = screenH * 0.27f;

    for (u32_t i = 0; i < m_KillFeedBanners.size(); ++i)
    {
        const KillFeedBanner& banner = m_KillFeedBanners[i];
        const f32_t fadeIn = UI_Clamp01(banner.fAge / 0.12f);
        const f32_t fadeOut = UI_Clamp01((banner.fLifetime - banner.fAge) / 0.35f);
        const f32_t alpha = std::min(fadeIn, fadeOut);
        if (alpha <= 0.f)
            continue;

        const char* pMessage = banner.strMessage.c_str();
        const ImVec2 textSize = pFont->CalcTextSizeA(kFontSize, FLT_MAX, 0.f, pMessage);
        const f32_t totalW = kRadius * 4.f + kGap * 2.f + textSize.x;
        const f32_t centerY = baseY + static_cast<f32_t>(i) * kRowHeight;
        const f32_t left = centerX - totalW * 0.5f;

        const ImVec2 bgMin(left - 14.f, centerY - kRadius - 9.f);
        const ImVec2 bgMax(left + totalW + 14.f, centerY + kRadius + 9.f);
        pDraw->AddRectFilled(
            bgMin,
            bgMax,
            UI_ColorWithAlpha(7, 10, 16, 0.68f * alpha),
            8.f);

        const ImU32 allyColor = UI_ColorWithAlpha(87, 165, 255, alpha);
        const ImU32 enemyColor = UI_ColorWithAlpha(232, 84, 84, alpha);
        const ImU32 neutralColor = UI_ColorWithAlpha(220, 198, 126, alpha);
        const ImU32 textColor = UI_ColorWithAlpha(238, 241, 245, alpha);
        const ImU32 tintColor = UI_ColorWithAlpha(255, 255, 255, alpha);

        const ImVec2 sourceCenter(left + kRadius, centerY);
        const ImVec2 textPos(sourceCenter.x + kRadius + kGap, centerY - textSize.y * 0.5f);
        const ImVec2 targetCenter(textPos.x + textSize.x + kGap + kRadius, centerY);

        if (banner.bSourceMinion && m_pSRV_OffscreenPingAtlas)
        {
            pDraw->AddCircleFilled(
                sourceCenter,
                kRadius,
                UI_ColorWithAlpha(24, 28, 34, 0.92f * alpha),
                48);
            UI_DrawPingAtlasSpriteCentered(
                pDraw,
                m_pSRV_OffscreenPingAtlas,
                UIPingAtlasSprite{ 970.f, 264.f, 1014.f, 315.f },
                sourceCenter,
                kRadius * 1.86f,
                tintColor);
            pDraw->AddCircle(
                sourceCenter,
                kRadius,
                banner.bSourceAlly ? allyColor : enemyColor,
                48,
                2.5f);
        }
        else
        {
            DrawKillFeedCircleImage(
                pDraw,
                sourceCenter,
                kRadius,
                FindOrLoadKillFeedPortrait(banner.iSourceActorContentId),
                tintColor,
                banner.bSourceAlly ? allyColor : enemyColor);
        }

        UI_DrawOutlinedText(pDraw, pFont, kFontSize, textPos, textColor, pMessage);

        if (banner.iObjectKind == kKillFeedObjectActor)
        {
            DrawKillFeedCircleImage(
                pDraw,
                targetCenter,
                kRadius,
                FindOrLoadKillFeedPortrait(banner.iTargetActorContentId),
                tintColor,
                banner.bSourceAlly ? enemyColor : allyColor);
            continue;
        }

        if (banner.iObjectKind == kKillFeedObjectStructure ||
            banner.iObjectKind == kKillFeedObjectObjective)
        {
            if (void* pObjectIcon = FindOrLoadKillFeedObjectIcon(
                    banner.iObjectKind,
                    banner.iTargetTeam))
            {
                DrawKillFeedCircleImage(
                    pDraw,
                    targetCenter,
                    kRadius,
                    pObjectIcon,
                    tintColor,
                    banner.bSourceAlly ? enemyColor : allyColor);
                continue;
            }
        }

        const char* pLabel = nullptr;
        ImU32 badgeColor = neutralColor;
        switch (banner.iObjectKind)
        {
        case kKillFeedObjectStructure:
            pLabel = "Tower";
            badgeColor = banner.bSourceAlly ? enemyColor : allyColor;
            break;
        case kKillFeedObjectObjective:
            pLabel = "Inhibitor";
            badgeColor = banner.bSourceAlly ? enemyColor : allyColor;
            break;
        case kKillFeedObjectDragon:
            pLabel = "Dragon";
            break;
        case kKillFeedObjectBaron:
            pLabel = "Baron";
            break;
        default:
            pLabel = "";
            break;
        }

        pDraw->AddCircleFilled(targetCenter, kRadius, UI_ColorWithAlpha(22, 26, 34, 0.92f * alpha), 48);
        pDraw->AddCircle(targetCenter, kRadius, badgeColor, 48, 2.5f);
        if (pLabel && pLabel[0] != '\0')
        {
            const ImVec2 labelSize = pFont->CalcTextSizeA(kLabelFontSize, FLT_MAX, 0.f, pLabel);
            const ImVec2 labelPos(
                targetCenter.x - labelSize.x * 0.5f,
                targetCenter.y - labelSize.y * 0.5f);
            UI_DrawOutlinedText(pDraw, pFont, kLabelFontSize, labelPos, textColor, pLabel);
        }
    }
}

void CUI_Manager::DrawMouseCursor(ImDrawList* pDraw)
{
    void* pSRV = nullptr;
    switch (m_CursorMode)
    {
    case eCursorMode::Enemy:
        pSRV = m_pSRV_CursorEnemy;
        break;
    case eCursorMode::Attack:
        pSRV = m_pSRV_CursorAttack;
        break;
    case eCursorMode::Default:
    default:
        pSRV = m_pSRV_CursorDefault;
        break;
    }
    if (!pSRV) return;

    const ImVec2 mouse = ImGui::GetIO().MousePos;
    ImVec2 a{ mouse.x, mouse.y };
    ImVec2 b{ mouse.x + m_fCursorSize, mouse.y + m_fCursorSize };
    pDraw->AddImage((ImTextureID)pSRV, a, b);
}

void CUI_Manager::DrawMouseCursorRHI()
{
    if (!m_pRHIUIRenderer)
        return;

    void* pSRV = nullptr;
    switch (m_CursorMode)
    {
    case eCursorMode::Enemy:
        pSRV = m_pSRV_CursorEnemy;
        break;
    case eCursorMode::Attack:
        pSRV = m_pSRV_CursorAttack;
        break;
    case eCursorMode::Default:
    default:
        pSRV = m_pSRV_CursorDefault;
        break;
    }
    if (!pSRV)
        return;

    const CInput& input = CInput::Get();
    const f32_t x = static_cast<f32_t>(input.GetMouseX());
    const f32_t y = static_cast<f32_t>(input.GetMouseY());
    m_pRHIUIRenderer->DrawImage(
        pSRV,
        x,
        y,
        m_fCursorSize,
        m_fCursorSize,
        Vec4(0.f, 0.f, 1.f, 1.f),
        UI_WhiteVec());
}

static u8_t UI_MaxSkillRankForSlot(u8_t slot)
{
    if (slot == 4)
        return 3;
    if (slot >= 1 && slot <= 3)
        return 5;
    return 0;
}

static bool_t UI_CanLevelSkillByActorLevel(u8_t slot, u8_t currentRank, u8_t actorLevel)
{
    if (slot == 4)
    {
        if (currentRank == 0)
            return actorLevel >= 6;
        if (currentRank == 1)
            return actorLevel >= 11;
        if (currentRank == 2)
            return actorLevel >= 16;
        return false;
    }

    if (slot >= 1 && slot <= 3)
        return actorLevel >= static_cast<u8_t>(1 + currentRank * 2);

    return false;
}

static bool_t UI_PointInRect(const ImVec2& p, const ImVec2& a, const ImVec2& b)
{
    return p.x >= a.x && p.x <= b.x && p.y >= a.y && p.y <= b.y;
}

// Clamp cursor position to the window bounds.
void CUI_Manager::UpdateCharacterHealthBarTrails(f32_t dt)
{
    if (!m_bHasWorldHealthBarState)
    {
        m_CharacterHealthBarTrails.clear();
        return;
    }

    dt = std::clamp(dt, 0.f, 0.1f);

    std::vector<EntityID> visibleCharacters;
    for (const UIWorldHealthBarDesc& Bar : m_WorldHealthBars)
    {
        if (Bar.Kind != UIWorldHealthBarKind::Character ||
            Bar.Entity == NULL_ENTITY ||
            Bar.bDead ||
            Bar.fMaximum <= 0.f)
        {
            continue;
        }

        visibleCharacters.push_back(Bar.Entity);

        const f32_t ratio = std::clamp(Bar.fCurrent / Bar.fMaximum, 0.f, 1.f);
        CharacterHealthBarTrailState& state = m_CharacterHealthBarTrails[Bar.Entity];
        if (!state.bInitialized)
        {
            state.fLastRatio = ratio;
            state.fTrailRatio = ratio;
            state.bInitialized = true;
            continue;
        }

        if (ratio + 0.001f < state.fLastRatio)
        {
            state.fTrailRatio = std::max(state.fTrailRatio, state.fLastRatio);
            state.fHoldSec = m_fHealthBarTrailHoldSec;
        }
        else if (ratio > state.fLastRatio)
        {
            state.fTrailRatio = ratio;
        }

        state.fLastRatio = ratio;

        if (state.fHoldSec > 0.f)
        {
            state.fHoldSec = std::max(0.f, state.fHoldSec - dt);
        }
        else if (state.fTrailRatio > ratio)
        {
            state.fTrailRatio = std::max(
                ratio,
                state.fTrailRatio - m_fHealthBarTrailShrinkSpeed * dt);
        }
    }

    for (auto it = m_CharacterHealthBarTrails.begin(); it != m_CharacterHealthBarTrails.end();)
    {
        if (std::find(visibleCharacters.begin(), visibleCharacters.end(), it->first) == visibleCharacters.end())
            it = m_CharacterHealthBarTrails.erase(it);
        else
            ++it;
    }
}
f32_t CUI_Manager::ResolveCharacterHealthTrailRatio(EntityID entity, f32_t currentRatio) const
{
    const auto it = m_CharacterHealthBarTrails.find(entity);
    if (it == m_CharacterHealthBarTrails.end())
        return currentRatio;

    return std::max(currentRatio, std::clamp(it->second.fTrailRatio, 0.f, 1.f));
}


void CUI_Manager::DrawActorHUDRHI(const ActorHUDState& State)
{
    if (!m_pActorHudPanel)
        return;

    LoadActorHUDAssetsForActor(State.iActorContentId);
    ApplyActorHUDSkillIconOverrides(State);
    m_pActorHudPanel->SetShowReference(m_bShowActorHUDReference);
    m_pActorHudPanel->SetReferenceAlpha(m_fHUDReferenceAlpha);
    m_pActorHudPanel->DrawRHI(State, m_iWinSizeX, m_iWinSizeY);

    f32_t fShieldStartRatio = 0.f;
    f32_t fShieldEndRatio = 0.f;
    CActorHudPanel::LayoutRect HpRect{};
    if (m_pRHIUIRenderer &&
        UI_ResolveShieldSegment(
            State.Hp,
            State.MaxHp,
            State.Shield,
            fShieldStartRatio,
            fShieldEndRatio) &&
        m_pActorHudPanel->FindElementScreenRect(
            "hp.fill",
            static_cast<f32_t>(m_iWinSizeX),
            static_cast<f32_t>(m_iWinSizeY),
            HpRect))
    {
        const f32_t fStartX = HpRect.fX + HpRect.fW * fShieldStartRatio;
        const f32_t fWidth = HpRect.fW * (fShieldEndRatio - fShieldStartRatio);
        m_pRHIUIRenderer->DrawImage(
            nullptr,
            fStartX,
            HpRect.fY,
            fWidth,
            HpRect.fH,
            Vec4(0.f, 0.f, 1.f, 1.f),
            Vec4(0.96f, 0.98f, 1.f, 0.60f));
    }
}

void CUI_Manager::DrawActorHUDOverlay(ImDrawList* pDraw, const ActorHUDState& State)
{
    if (!pDraw)
        return;

    LoadActorHUDAssetsForActor(State.iActorContentId);
    ApplyActorHUDSkillIconOverrides(State);

    const ImVec2 Display = ImGui::GetIO().DisplaySize;
    f32_t HudW = m_fHUDWidth;
    f32_t HudH = m_fHUDHeight;
    if (HudW > Display.x - 24.f)
    {
        const f32_t Scale = (Display.x - 24.f) / HudW;
        HudW *= Scale;
        HudH *= Scale;
    }

    const f32_t ScaleX = HudW / kActorHUDRefW;
    const f32_t ScaleY = HudH / kActorHUDRefH;
    const ImVec2 Root((Display.x - HudW) * 0.5f, Display.y - HudH);
    const bool_t bRHIBaseDrawn =
        m_pRHIUIRenderer &&
        m_pRHIUIRenderer->IsReady() &&
        m_iWinSizeX > 0 &&
        m_iWinSizeY > 0;

    if (!bRHIBaseDrawn && m_bShowActorHUDReference && m_pSRV_ActorHUDBase)
    {
        const u8_t ReferenceAlpha =
            static_cast<u8_t>(UI_Clamp01(m_fHUDReferenceAlpha) * 255.f);
        pDraw->AddImage(
            reinterpret_cast<ImTextureID>(m_pSRV_ActorHUDBase),
            Root,
            ImVec2(Root.x + HudW, Root.y + HudH),
            ImVec2(0.f, 0.f),
            ImVec2(1.f, 1.f),
            IM_COL32(255, 255, 255, ReferenceAlpha));
    }

    if (!bRHIBaseDrawn && m_pActorHudPanel)
    {
        f32_t fShieldStartRatio = 0.f;
        f32_t fShieldEndRatio = 0.f;
        CActorHudPanel::LayoutRect HpRect{};
        if (UI_ResolveShieldSegment(
                State.Hp,
                State.MaxHp,
                State.Shield,
                fShieldStartRatio,
                fShieldEndRatio) &&
            m_pActorHudPanel->FindElementScreenRect(
                "hp.fill", Display.x, Display.y, HpRect))
        {
            const f32_t fStartX = HpRect.fX + HpRect.fW * fShieldStartRatio;
            const f32_t fEndX = HpRect.fX + HpRect.fW * fShieldEndRatio;
            pDraw->AddRectFilled(
                ImVec2(fStartX, HpRect.fY),
                ImVec2(fEndX, HpRect.fY + HpRect.fH),
                IM_COL32(244, 249, 255, 153));
        }

    }

    if (m_pActorHudPanel)
        m_pActorHudPanel->DrawTextOverlay(State);

    auto ToPosition = [&](f32_t X, f32_t Y) -> ImVec2
    {
        return ImVec2(Root.x + X * ScaleX, Root.y + Y * ScaleY);
    };
    auto ToRoundedString = [](f32_t Value) -> std::string
    {
        const i32_t RoundedValue = static_cast<i32_t>(Value + ((Value >= 0.f) ? 0.5f : -0.5f));
        return std::to_string(RoundedValue);
    };
    auto DrawCenteredTextAtScreenCenter = [&](const std::string& Text, const ImVec2& Center, ImU32 Color)
    {
        ImFont* pFont = FindUIFont("hud");
        const f32_t FontSize = pFont ? (pFont->LegacySize * ScaleY) : (ImGui::GetFontSize() * ScaleY);
        if (!pFont)
            return;
        const ImVec2 TextSize = pFont->CalcTextSizeA(FontSize, FLT_MAX, 0.f, Text.c_str());
        const ImVec2 Position(Center.x - TextSize.x * 0.5f, Center.y - TextSize.y * 0.5f);
        UI_DrawOutlinedText(pDraw, pFont, FontSize, Position, Color, Text.c_str());
    };
    auto DrawSkillKeyLabel = [&](const char* pText, const ImVec2& IconMin, const ImVec2& IconMax)
    {
        ImFont* pFont = FindUIFont("hud");
        if (!pFont || !pText)
            return;

        const f32_t FontSize = pFont->LegacySize * ScaleY * 0.74f;
        const ImVec2 TextSize = pFont->CalcTextSizeA(FontSize, FLT_MAX, 0.f, pText);
        const ImVec2 Position(
            IconMax.x - TextSize.x + 1.f * ScaleX,
            IconMax.y - TextSize.y + 3.f * ScaleY);
        UI_DrawOutlinedText(pDraw, pFont, FontSize, Position, IM_COL32(210, 216, 222, 255), pText);
    };

    if (State.LethalTempoStacks > 0u)
    {
        ImFont* pRuneFont = FindUIFont("hud");
        if (pRuneFont)
        {
            char Text[16]{};
            std::snprintf(
                Text,
                sizeof(Text),
                "LT %u/%u",
                static_cast<u32_t>(State.LethalTempoStacks),
                static_cast<u32_t>(State.LethalTempoMaxStacks));

            const f32_t FontSize = pRuneFont->LegacySize * ScaleY * 0.72f;
            UI_DrawOutlinedText(
                pDraw,
                pRuneFont,
                FontSize,
                ToPosition(318.f, 57.f),
                IM_COL32(245, 231, 177, 255),
                Text);
        }
    }

    constexpr f32_t kSkillSlotFallbackCenterX[4] = { 372.f, 430.25f, 491.75f, 553.f };
    constexpr f32_t kSkillSlotCooldownCenterY = 83.f;
    constexpr f32_t kSkillSlotCooldownRadius = 22.f;
    constexpr const char* SkillSlotLayoutIDs[4] = { "skill.q", "skill.w", "skill.e", "skill.r" };
    constexpr const char* SkillSlotLabels[4] = { "Q", "W", "E", "R" };
    const ImGuiIO& IO = ImGui::GetIO();
    const f32_t DeltaTime = IO.DeltaTime;

    if (State.LocalEntity != m_LastSkillRankHUDLocalEntity)
    {
        m_LastSkillRankHUDLocalEntity = State.LocalEntity;
        m_LastHUDSkillRanks = State.SkillRanks;
        m_fSkillLearnFlashTimers.fill(0.f);
    }
    else
    {
        for (u32_t RankIndex = 1; RankIndex < static_cast<u32_t>(State.SkillRanks.size()); ++RankIndex)
        {
            if (State.SkillRanks[RankIndex] > m_LastHUDSkillRanks[RankIndex])
                m_fSkillLearnFlashTimers[RankIndex - 1u] = 2.f;
            m_LastHUDSkillRanks[RankIndex] = State.SkillRanks[RankIndex];
        }
    }

    for (f32_t& Timer : m_fSkillLearnFlashTimers)
        Timer = (Timer > DeltaTime) ? (Timer - DeltaTime) : 0.f;

    const ImVec2 ShopButtonMin = ToPosition(642.f, 132.f);
    const ImVec2 ShopButtonMax = ToPosition(760.f, 160.f);
    if (IO.MouseClicked[0] && UI_PointInRect(IO.MousePos, ShopButtonMin, ShopButtonMax))
        ToggleInGameShop();

    ImVec2 SkillIconMins[4]{};
    ImVec2 SkillIconMaxs[4]{};
    for (u32_t Index = 0; Index < 4; ++Index)
    {
        const u8_t SkillSlot = static_cast<u8_t>(Index + 1);
        const u8_t Rank = State.SkillRanks[SkillSlot];
        const u8_t MaxRank = UI_MaxSkillRankForSlot(SkillSlot);
        const bool_t bCanLevel =
            State.LocalEntity != NULL_ENTITY &&
            State.SkillPoints > 0 &&
            Rank < MaxRank &&
            UI_CanLevelSkillByActorLevel(SkillSlot, Rank, State.Level);

        ImVec2 SkillIconMin = ToPosition(kSkillSlotFallbackCenterX[Index] - 22.f, kSkillSlotCooldownCenterY - 22.f);
        ImVec2 SkillIconMax = ToPosition(kSkillSlotFallbackCenterX[Index] + 22.f, kSkillSlotCooldownCenterY + 22.f);
        ImVec2 SkillCooldownCenter = ToPosition(kSkillSlotFallbackCenterX[Index], kSkillSlotCooldownCenterY);
        f32_t SkillCooldownRadius = kSkillSlotCooldownRadius * std::min(ScaleX, ScaleY);
        if (m_pActorHudPanel)
        {
            CActorHudPanel::LayoutRect SkillScreenRect{};
            if (m_pActorHudPanel->FindElementScreenRect(
                SkillSlotLayoutIDs[Index],
                Display.x,
                Display.y,
                SkillScreenRect))
            {
                SkillCooldownCenter = ImVec2(
                    SkillScreenRect.fX + SkillScreenRect.fW * 0.5f,
                    SkillScreenRect.fY + SkillScreenRect.fH * 0.5f);
                SkillCooldownRadius = std::min(SkillScreenRect.fW, SkillScreenRect.fH) * 0.5f;
                SkillIconMin = ImVec2(SkillScreenRect.fX, SkillScreenRect.fY);
                SkillIconMax = ImVec2(SkillScreenRect.fX + SkillScreenRect.fW, SkillScreenRect.fY + SkillScreenRect.fH);
            }
        }
        const ImVec2 SkillRankPipCenter(
            SkillCooldownCenter.x,
            SkillIconMax.y + 6.f * ScaleY);
        SkillIconMins[Index] = SkillIconMin;
        SkillIconMaxs[Index] = SkillIconMax;

        if (State.Cooldowns[Index] > 0.f && State.MaxCooldowns[Index] > 0.f)
        {
            const f32_t CooldownRatio = State.Cooldowns[Index] / State.MaxCooldowns[Index];
            UI_DrawCooldownPie(
                pDraw,
                SkillCooldownCenter,
                SkillCooldownRadius,
                CooldownRatio);
        }

        if (bCanLevel)
        {
            const f32_t UpgradeButtonW = SkillIconMax.x - SkillIconMin.x;
            const f32_t UpgradeButtonH = SkillIconMax.y - SkillIconMin.y;
            const ImVec2 ArrowMin(
                SkillCooldownCenter.x - UpgradeButtonW * 0.5f,
                SkillIconMin.y - UpgradeButtonH - 2.f * ScaleY);
            const ImVec2 ArrowMax(
                SkillCooldownCenter.x + UpgradeButtonW * 0.5f,
                SkillIconMin.y - 2.f * ScaleY);
            if (IO.MouseClicked[0] && UI_PointInRect(IO.MousePos, ArrowMin, ArrowMax))
            {
                if (m_pfnLevelSkill)
                {
                    m_pfnLevelSkill(m_pLevelSkillUser, SkillSlot);
                    OutputDebugStringA("[UI_Manager] LevelSkill command requested\n");
                }
                else
                {
                    OutputDebugStringA("[UI_Manager] LevelSkill command callback missing\n");
                }
            }

            if (m_pSRV_SkillUpgrade)
            {
                pDraw->AddImage(
                    reinterpret_cast<ImTextureID>(m_pSRV_SkillUpgrade),
                    ArrowMin,
                    ArrowMax,
                    ImVec2(0.f, 0.f),
                    ImVec2(1.f, 1.f),
                    IM_COL32(255, 255, 255, 245));
            }
            else
            {
                pDraw->AddTriangleFilled(
                    ImVec2((ArrowMin.x + ArrowMax.x) * 0.5f, ArrowMin.y),
                    ImVec2(ArrowMax.x, ArrowMax.y),
                    ImVec2(ArrowMin.x, ArrowMax.y),
                    IM_COL32(245, 218, 112, 235));
            }
        }

        DrawSkillRankPips(
            pDraw,
            m_pSRV_SkillRankPip,
            SkillRankPipCenter,
            std::min(ScaleX, ScaleY),
            Rank,
            MaxRank);

        if (m_fSkillLearnFlashTimers[Index] > 0.f)
        {
            DrawSkillLearnFlash(
                pDraw,
                m_pSRV_ActorSkillIcons[Index],
                SkillIconMin,
                SkillIconMax,
                m_fSkillLearnFlashTimers[Index]);
        }

        if (State.Cooldowns[Index] > 0.f)
        {
            const std::string StrCooldownText = ToRoundedString(State.Cooldowns[Index]);
            DrawCenteredTextAtScreenCenter(StrCooldownText, SkillCooldownCenter, IM_COL32(255, 255, 255, 255));
        }

        DrawSkillKeyLabel(SkillSlotLabels[Index], SkillIconMin, SkillIconMax);
    }

    if (m_pSRV_ActorPassiveIcon)
    {
        const f32_t SkillIconH = SkillIconMaxs[0].y - SkillIconMins[0].y;
        const f32_t PassiveSize = SkillIconH * 0.68f;
        const f32_t PassiveGap = 7.f * std::min(ScaleX, ScaleY);
        const f32_t SkillQCenterY = (SkillIconMins[0].y + SkillIconMaxs[0].y) * 0.5f;
        const ImVec2 PassiveMin(
            SkillIconMins[0].x - PassiveGap - PassiveSize,
            SkillQCenterY - PassiveSize * 0.5f);
        const ImVec2 PassiveMax(
            PassiveMin.x + PassiveSize,
            PassiveMin.y + PassiveSize);
        pDraw->AddImage(
            reinterpret_cast<ImTextureID>(m_pSRV_ActorPassiveIcon),
            PassiveMin,
            PassiveMax);
    }

    ImFont* pFont = FindUIFont("hud");
    if (pFont)
    {
        ImVec2 SlotMins[6]{};
        ImVec2 SlotMaxs[6]{};
        i8_t HoverSlot = -1;
        for (u8_t Slot = 0u; Slot < 6u; ++Slot)
        {
            SlotMins[Slot] = ToPosition(kInventorySlotX[Slot], kInventorySlotY[Slot]);
            SlotMaxs[Slot] = ToPosition(
                kInventorySlotX[Slot] + kInventorySlotSize,
                kInventorySlotY[Slot] + kInventorySlotSize);
            if (UI_PointInRect(IO.MousePos, SlotMins[Slot], SlotMaxs[Slot]))
                HoverSlot = static_cast<i8_t>(Slot);
        }

        if (IO.MouseClicked[0] && HoverSlot >= 0 &&
            State.InventoryItemIds[static_cast<u8_t>(HoverSlot)] != 0u)
        {
            m_iInventoryDragSource = HoverSlot;
            m_iInventoryDragHover = HoverSlot;
            m_iInventoryDragItemId =
                State.InventoryItemIds[static_cast<u8_t>(HoverSlot)];
        }
        if (m_iInventoryDragSource >= 0)
            m_iInventoryDragHover = HoverSlot;
        if (IO.MouseReleased[0] && m_iInventoryDragSource >= 0)
        {
            if (m_iInventoryDragHover >= 0 &&
                m_iInventoryDragHover != m_iInventoryDragSource &&
                m_pfnInventoryReorder)
            {
                m_pfnInventoryReorder(
                    m_pInventoryReorderUser,
                    static_cast<u8_t>(m_iInventoryDragSource),
                    static_cast<u8_t>(m_iInventoryDragHover),
                    m_iInventoryDragItemId);
            }
            m_iInventoryDragSource = -1;
            m_iInventoryDragHover = -1;
            m_iInventoryDragItemId = 0u;
        }

        const f32_t FontSize = pFont->LegacySize * ScaleY * 0.68f;
        for (u32_t Index = 0; Index < State.InventoryItemIds.size(); ++Index)
        {
            const ImVec2 IconMin = SlotMins[Index];
            const ImVec2 IconMax = SlotMaxs[Index];
            const u16_t ItemId = State.InventoryItemIds[Index];
            pDraw->AddRectFilled(
                IconMin, IconMax, IM_COL32(13, 17, 22, 205), 2.f);
            const bool_t bDragSource =
                m_iInventoryDragSource == static_cast<i8_t>(Index);
            const bool_t bDragTarget =
                m_iInventoryDragSource >= 0 &&
                m_iInventoryDragHover == static_cast<i8_t>(Index);
            pDraw->AddRect(
                IconMin,
                IconMax,
                bDragTarget ? IM_COL32(245, 205, 92, 255) :
                    bDragSource ? IM_COL32(120, 176, 238, 255) :
                    IM_COL32(91, 99, 109, 230),
                2.f,
                0,
                bDragTarget ? 2.5f : 1.f);

            if (ItemId != 0u)
            {
                const InGameShopItemView* pItemView = FindInGameShopItem(ItemId);
                if (pItemView && pItemView->pSRV)
                {
                    pDraw->AddImage(
                        reinterpret_cast<ImTextureID>(pItemView->pSRV),
                        IconMin,
                        IconMax,
                        ImVec2(0.f, 0.f),
                        ImVec2(1.f, 1.f),
                        bDragSource
                            ? IM_COL32(255, 255, 255, 150)
                            : IM_COL32(255, 255, 255, 255));
                }
                else
                {
                    const std::string Text =
                        std::to_string(static_cast<u32_t>(ItemId));
                    const ImVec2 TextSize =
                        pFont->CalcTextSizeA(FontSize, FLT_MAX, 0.f, Text.c_str());
                    ImVec2 Position(
                        (IconMin.x + IconMax.x - TextSize.x) * 0.5f,
                        (IconMin.y + IconMax.y - TextSize.y) * 0.5f);
                    UI_DrawOutlinedText(
                        pDraw, pFont, FontSize, Position,
                        IM_COL32(245, 231, 177, 255), Text.c_str());
                }
            }

            const std::string KeyText = std::to_string(Index + 1u);
            const f32_t KeyFontSize = FontSize * 0.82f;
            const ImVec2 KeyTextSize =
                pFont->CalcTextSizeA(KeyFontSize, FLT_MAX, 0.f, KeyText.c_str());
            const ImVec2 KeyPosition(
                IconMax.x - KeyTextSize.x - 2.f * ScaleX,
                IconMax.y - KeyTextSize.y + 1.f * ScaleY);
            UI_DrawOutlinedText(
                pDraw, pFont, KeyFontSize, KeyPosition,
                IM_COL32(230, 235, 240, 255), KeyText.c_str());
        }
    }

    if (!m_bUseLuaUI)
        DrawInGameShop(pDraw);
}

// World-to-screen projection shared by editor and in-game render debug.
static bool UI_WorldToScreen(const DirectX::XMMATRIX& mVP, const Vec3& w, ImVec2& out,
    uint32_t iWinX, uint32_t iWinY)
{
    DirectX::XMVECTOR v = DirectX::XMVectorSet(w.x, w.y, w.z, 1.f);
    v = DirectX::XMVector4Transform(v, mVP);

    const f32_t wComp = DirectX::XMVectorGetW(v);
    if (wComp <= 0.01f)
        return false;

    const f32_t nx = DirectX::XMVectorGetX(v) / wComp;
    const f32_t ny = DirectX::XMVectorGetY(v) / wComp;
    out.x = (nx * 0.5f + 0.5f) * static_cast<f32_t>(iWinX);
    out.y = (1.f - (ny * 0.5f + 0.5f)) * static_cast<f32_t>(iWinY);

    constexpr f32_t kScreenCullMargin = 96.f;
    return out.x >= -kScreenCullMargin &&
        out.x <= static_cast<f32_t>(iWinX) + kScreenCullMargin &&
        out.y >= -kScreenCullMargin &&
        out.y <= static_cast<f32_t>(iWinY) + kScreenCullMargin;
}

// ------------------------------------------------------------------------------
// Resolve the team field from Unit, Character, or Structure components.
// ForEach<Health, Transform> excludes entities without those components.
void CUI_Manager::DrawMapPings(ImDrawList* pDraw,
    const DirectX::XMMATRIX& mVP, f32_t fDeltaTime)
{
    if (!pDraw)
        return;

    if (fDeltaTime < 0.f)
        fDeltaTime = 0.f;
    if (fDeltaTime > 0.1f)
        fDeltaTime = 0.1f;

    for (MapPingMarker& Marker : m_MapPingMarkers)
        Marker.fAge += fDeltaTime;

    m_MapPingMarkers.erase(
        std::remove_if(
            m_MapPingMarkers.begin(),
            m_MapPingMarkers.end(),
            [](const MapPingMarker& Marker)
            {
                return Marker.fAge >= Marker.fLifetime;
            }),
        m_MapPingMarkers.end());

    for (const MapPingMarker& Marker : m_MapPingMarkers)
    {
        Vec3 vDrawPos = Marker.vWorldPos;
        vDrawPos.y += 1.15f;

        ImVec2 Screen{};
        if (!UI_WorldToScreen(mVP, vDrawPos, Screen, m_iWinSizeX, m_iWinSizeY))
            continue;

        const f32_t Ratio = Marker.fLifetime > 0.f
            ? UI_Clamp01(Marker.fAge / Marker.fLifetime)
            : 1.f;
        constexpr f32_t kFadeStart = 0.68f;
        const f32_t Alpha = Ratio <= kFadeStart
            ? 1.f
            : UI_Clamp01(1.f - ((Ratio - kFadeStart) / (1.f - kFadeStart)));
        const f32_t Size = 42.f + (1.f - Ratio) * 8.f;
        const f32_t Half = Size * 0.5f;
        const ImVec2 Min(Screen.x - Half, Screen.y - Half);
        const ImVec2 Max(Screen.x + Half, Screen.y + Half);

        if (m_pSRV_OffscreenPingAtlas)
        {
            UI_DrawPingAtlasMarker(
                pDraw,
                m_pSRV_OffscreenPingAtlas,
                static_cast<u8_t>(Marker.eDirection),
                Screen,
                Size * 1.34f,
                false,
                Alpha);
            continue;
        }

        pDraw->AddCircleFilled(
            Screen,
            Half * 0.82f,
            UI_ColorWithAlpha(5, 12, 18, 0.42f * Alpha),
            32);
        if (void* pSRV = ResolvePingSRV(Marker.eDirection))
        {
            pDraw->AddImage(
                reinterpret_cast<ImTextureID>(pSRV),
                Min,
                Max,
                ImVec2(0.f, 0.f),
                ImVec2(1.f, 1.f),
                UI_ColorWithAlpha(255, 255, 255, Alpha));
        }
        pDraw->AddCircle(
            Screen,
            Half * 0.84f,
            UI_ColorWithAlpha(64, 210, 255, 0.72f * Alpha),
            32,
            1.6f);
    }
}

void CUI_Manager::DrawDamageFloaters(ImDrawList* pDraw,
    const DirectX::XMMATRIX& mVP, f32_t fDeltaTime)
{
    if (fDeltaTime < 0.f)
        fDeltaTime = 0.f;
    if (fDeltaTime > 0.1f)
        fDeltaTime = 0.1f;

    for (DamageFloater& floater : m_DamageFloaters)
        floater.fAge += fDeltaTime;
    for (WorldTextFloater& floater : m_WorldTextFloaters)
        floater.fAge += fDeltaTime;

    m_DamageFloaters.erase(
        std::remove_if(m_DamageFloaters.begin(), m_DamageFloaters.end(),
            [](const DamageFloater& floater)
            {
                return floater.fAge >= floater.fLifetime;
            }),
        m_DamageFloaters.end());
    m_WorldTextFloaters.erase(
        std::remove_if(m_WorldTextFloaters.begin(), m_WorldTextFloaters.end(),
            [](const WorldTextFloater& floater)
            {
                return floater.fAge >= floater.fLifetime;
            }),
        m_WorldTextFloaters.end());

    if (!m_bShowDamageFloaters)
        return;

    ImFont* pDamageFont = FindUIFont("hud");
    ImFont* pWorldTextFont = FindUIFont("fallback");

    if (!pWorldTextFont)
        pWorldTextFont = pDamageFont;

    if (!pDamageFont)
        pDamageFont = pWorldTextFont;

    if (!pDamageFont || !pWorldTextFont)
        return;

    for (const DamageFloater& floater : m_DamageFloaters)
    {
        ImVec2 screen{};
        if (!UI_WorldToScreen(mVP, floater.vWorldPos, screen, m_iWinSizeX, m_iWinSizeY))
            continue;

        const f32_t t = UI_Clamp01(floater.fAge / floater.fLifetime);
        const f32_t alpha = 1.f - t;
        screen.x += floater.fXJitter;

        char text[32]{};

        std::snprintf(text, sizeof(text), "%.0f", floater.fAmount);

        const f32_t baseFontSize = floater.bWasCrit ? 26.f : 20.f;
        const f32_t fontSize = baseFontSize * (1.f + 0.18f * t);
        const ImVec2 textSize = pDamageFont->CalcTextSizeA(fontSize, FLT_MAX, 0.f, text);
        ImVec2 pos(screen.x - textSize.x * 0.5f, screen.y - textSize.y * 0.5f);
        const ImU32 color = UI_DamageColor(
            floater.iDamageType,
            floater.bWasCrit,
            floater.bKilled,
            alpha);

        if (floater.bShowCriticalIndicator && m_pSRV_StatsPanelAtlas)
        {
            const f32_t iconSize = fontSize * 1.08f;
            const f32_t gap = 3.f;
            const f32_t totalW = iconSize + gap + textSize.x;
            const ImVec2 iconMin(
                screen.x - totalW * 0.5f,
                screen.y - iconSize * 0.5f);
            const ImVec2 iconMax(iconMin.x + iconSize, iconMin.y + iconSize);
            pDraw->AddImage(
                reinterpret_cast<ImTextureID>(m_pSRV_StatsPanelAtlas),
                iconMin,
                iconMax,
                UI_StatsPanelAtlasUV(232.f, 488.f),
                UI_StatsPanelAtlasUV(256.f, 512.f),
                UI_ColorWithAlpha(255, 255, 255, alpha));
            pos.x = iconMax.x + gap;
        }

        UI_DrawOutlinedText(pDraw, pDamageFont, fontSize, pos, color, text);
    }

    for (const WorldTextFloater& floater : m_WorldTextFloaters)
    {
        ImVec2 screen{};
        if (!UI_WorldToScreen(mVP, floater.vWorldPos, screen, m_iWinSizeX, m_iWinSizeY))
            continue;

        const f32_t t = UI_Clamp01(floater.fAge / floater.fLifetime);
        const f32_t alpha = 1.f - t;
        screen.x += floater.fXJitter;

        const f32_t fontSize = 19.f * (1.f + 0.16f * t);
        const char* pText = floater.strText.c_str();
        if (floater.bShowGoldIcon)
        {
            ImFont* pGoldFont = pDamageFont ? pDamageFont : pWorldTextFont;
            const ImVec2 textSize = pGoldFont->CalcTextSizeA(fontSize, FLT_MAX, 0.f, pText);
            const f32_t iconSize = fontSize * 0.88f;
            const f32_t gap = 4.f;
            const f32_t totalW = iconSize + gap + textSize.x;
            const ImVec2 iconMin(screen.x - totalW * 0.5f, screen.y - iconSize * 0.5f);
            const ImVec2 iconMax(iconMin.x + iconSize, iconMin.y + iconSize);
            const bool_t bIconDrawn = UI_DrawManifestSprite(
                pDraw,
                m_InGameShopAtlasManifest,
                "gold.coin",
                iconMin,
                iconMax,
                alpha);
            const f32_t textX = bIconDrawn
                ? iconMax.x + gap
                : screen.x - textSize.x * 0.5f;
            const ImVec2 pos(textX, screen.y - textSize.y * 0.5f);
            const ImU32 color = UI_ColorFromVec4(floater.vColor, alpha);

            UI_DrawOutlinedText(pDraw, pGoldFont, fontSize, pos, color, pText);
            continue;
        }

        const ImVec2 textSize = pWorldTextFont->CalcTextSizeA(fontSize, FLT_MAX, 0.f, pText);
        const ImVec2 pos(screen.x - textSize.x * 0.5f, screen.y - textSize.y * 0.5f);
        const ImU32 color = UI_ColorFromVec4(floater.vColor, alpha);

        UI_DrawOutlinedText(pDraw, pWorldTextFont, fontSize, pos, color, pText);
    }
}

static bool_t UI_IsTeamAlly(u8_t iTeam, u8_t iLocalTeam)
{
    return iTeam != kUIInvalidTeam &&
        iLocalTeam != kUIInvalidTeam &&
        iTeam == iLocalTeam;
}
static ImVec2 UI_AtlasUV(f32_t x, f32_t y)
{
    constexpr f32_t kAtlasSize = 1024.f;
    return ImVec2(x / kAtlasSize, y / kAtlasSize);
}

struct HealthBarScreenRects
{
    ImVec2 BarMin{};
    ImVec2 BarMax{};
    ImVec2 FillMin{};
    ImVec2 FillMax{};
    ImVec2 ManaMin{};
    ImVec2 ManaMax{};
};

static HealthBarScreenRects BuildHealthBarScreenRects(
    const ImVec2& center,
    f32_t width,
    f32_t height,
    bool_t bHasResource)
{
    HealthBarScreenRects rects{};
    const f32_t effectiveHeight = bHasResource ? height : height * 0.68f;
    rects.BarMin = ImVec2(
        center.x - width * 0.5f,
        center.y - effectiveHeight * 0.5f);
    rects.BarMax = ImVec2(
        center.x + width * 0.5f,
        center.y + effectiveHeight * 0.5f);
    rects.FillMin = ImVec2(
        rects.BarMin.x + width * 0.012f,
        rects.BarMin.y + effectiveHeight * 0.08f);
    rects.FillMax = ImVec2(
        rects.BarMax.x - width * 0.012f,
        bHasResource
            ? rects.BarMin.y + height * 0.60f
            : rects.BarMax.y - effectiveHeight * 0.08f);
    rects.ManaMin = ImVec2(
        rects.BarMin.x + width * 0.012f,
        rects.BarMin.y + height * 0.68f);
    rects.ManaMax = ImVec2(
        rects.BarMax.x - width * 0.012f,
        rects.BarMax.y - height * 0.10f);
    return rects;
}

static void DrawHealthBarcode(ImDrawList* pDraw,
    const ImVec2& fillMin,
    const ImVec2& fillMax,
    f32_t maxHP)
{
    if (!pDraw || maxHP <= 100.f)
        return;

    constexpr f32_t kHealthBarSegmentHp = 100.f;
    constexpr i32_t kMajorSegmentStep = 10;
    constexpr f32_t kMinMinorTickGapPx = 2.0f;

    const f32_t fillW = fillMax.x - fillMin.x;
    const f32_t fillH = fillMax.y - fillMin.y;
    f32_t lastMinorX = -FLT_MAX;

    const i32_t segmentCount = static_cast<i32_t>(maxHP / kHealthBarSegmentHp);
    for (i32_t i = 1; i <= segmentCount; ++i)
    {
        const f32_t segmentHP = static_cast<f32_t>(i) * kHealthBarSegmentHp;
        if (segmentHP >= maxHP)
            break;

        const f32_t x = fillMin.x + fillW * (segmentHP / maxHP);
        const bool_t bMajor = (i % kMajorSegmentStep) == 0;

        if (!bMajor && x - lastMinorX < kMinMinorTickGapPx)
            continue;
        if (!bMajor)
            lastMinorX = x;

        const f32_t y0 = bMajor ? fillMin.y : fillMin.y + fillH * 0.22f;
        const f32_t thickness = bMajor ? 1.5f : 1.0f;
        pDraw->AddLine(
            ImVec2(x, y0),
            ImVec2(x, fillMax.y),
            IM_COL32(0, 0, 0, bMajor ? 230 : 175),
            thickness);
    }
}

// ------------------------------------------------------------------------------
// Factory
// ------------------------------------------------------------------------------
unique_ptr<CUI_Manager> CUI_Manager::Create()
{
    return unique_ptr<CUI_Manager>(new CUI_Manager());
}

// DrawHealthBars - character-attached combat bars.
//   Fill: procedural unlit color; do not sample baked HP bar PNGs.
//   Depleted area: dark backing.
//   Barcode: 100 HP ticks, with 1000 HP major ticks.
void CUI_Manager::DrawHealthBars(ImDrawList* pDraw, const DirectX::XMMATRIX& mVP)
{
    if (!m_bHasWorldHealthBarState || !pDraw)
        return;

    const f32_t w = m_fHPBarWidth;
    const f32_t h = m_fHPBarHeight;
    const f32_t yOff = m_fHPBarYOffset;

    for (const UIWorldHealthBarDesc& Bar : m_WorldHealthBars)
    {
        if (Bar.Kind != UIWorldHealthBarKind::Character ||
            Bar.bDead ||
            Bar.fMaximum <= 0.f)
        {
            continue;
        }

        const Vec3 p = Bar.vWorldPos;
        ImVec2 screen;
        if (!UI_WorldToScreen(mVP, { p.x, p.y + yOff, p.z }, screen,
            m_iWinSizeX, m_iWinSizeY))
        {
            continue;
        }

        const f32_t clamped = std::clamp(Bar.fCurrent / Bar.fMaximum, 0.f, 1.f);
        const bool_t bHasResource =
            Bar.ResourceKind != eUIResourceKind::None;
        const HealthBarScreenRects rects =
            BuildHealthBarScreenRects(screen, w, h, bHasResource);
        const bool_t bAlly = UI_IsTeamAlly(Bar.iTeam, m_iWorldHealthBarLocalTeam);

        pDraw->AddRectFilled(rects.BarMin, rects.BarMax, IM_COL32(10, 10, 10, 226));
        const f32_t fillW = rects.FillMax.x - rects.FillMin.x;
        const f32_t trailRatio = ResolveCharacterHealthTrailRatio(Bar.Entity, clamped);
        if (trailRatio > clamped + 0.002f)
        {
            const f32_t trailMinX = rects.FillMin.x + fillW * clamped;
            const f32_t trailMaxX = rects.FillMin.x + fillW * trailRatio;
            pDraw->AddRectFilled(
                ImVec2(trailMinX, rects.FillMin.y),
                ImVec2(trailMaxX, rects.FillMax.y),
                IM_COL32(255, 18, 8, 242));
            pDraw->AddRectFilled(
                ImVec2(trailMinX, rects.FillMin.y),
                ImVec2(trailMaxX, rects.FillMin.y + 1.f),
                IM_COL32(255, 128, 84, 110));
        }

        if (clamped > 0.f)
        {
            const ImVec2 fillMax(rects.FillMin.x + fillW * clamped, rects.FillMax.y);
            const ImU32 fillColor = bAlly ? IM_COL32(74, 190, 62, 255) : IM_COL32(218, 52, 48, 255);
            const ImU32 topColor = bAlly ? IM_COL32(142, 255, 118, 72) : IM_COL32(255, 132, 104, 72);

            pDraw->AddRectFilled(rects.FillMin, fillMax, fillColor);
            pDraw->AddRectFilled(rects.FillMin, ImVec2(fillMax.x, rects.FillMin.y + 1.f), topColor);
        }

        f32_t shieldStartRatio = 0.f;
        f32_t shieldEndRatio = 0.f;
        if (UI_ResolveShieldSegment(
                Bar.fCurrent,
                Bar.fMaximum,
                Bar.fShield,
                shieldStartRatio,
                shieldEndRatio))
        {
            pDraw->AddRectFilled(
                ImVec2(rects.FillMin.x + fillW * shieldStartRatio, rects.FillMin.y),
                ImVec2(rects.FillMin.x + fillW * shieldEndRatio, rects.FillMax.y),
                IM_COL32(244, 249, 255, 153));
        }

        const f32_t manaMax = Bar.fManaMaximum;
        const f32_t manaRatio = (manaMax > 0.f)
            ? std::clamp(Bar.fManaCurrent / manaMax, 0.f, 1.f)
            : 0.f;
        if (bHasResource && manaMax > 0.f)
        {
            pDraw->AddRectFilled(rects.ManaMin, rects.ManaMax, IM_COL32(6, 13, 25, 235));
            if (manaRatio > 0.f)
            {
                const f32_t manaW = (rects.ManaMax.x - rects.ManaMin.x) * manaRatio;
                const ImVec2 manaFillMax(rects.ManaMin.x + manaW, rects.ManaMax.y);
                const ImU32 resourceFill =
                    Bar.ResourceKind == eUIResourceKind::Energy
                        ? IM_COL32(236, 191, 36, 255)
                        : Bar.ResourceKind == eUIResourceKind::Flow
                            ? IM_COL32(226, 235, 244, 255)
                            : IM_COL32(36, 125, 226, 255);
                const ImU32 resourceHighlight =
                    Bar.ResourceKind == eUIResourceKind::Energy
                        ? IM_COL32(255, 235, 118, 110)
                        : Bar.ResourceKind == eUIResourceKind::Flow
                            ? IM_COL32(255, 255, 255, 120)
                            : IM_COL32(108, 210, 255, 92);
                pDraw->AddRectFilled(rects.ManaMin, manaFillMax, resourceFill);
                pDraw->AddRectFilled(
                    rects.ManaMin,
                    ImVec2(manaFillMax.x, rects.ManaMin.y + 1.f),
                    resourceHighlight);
            }
        }

        DrawHealthBarcode(pDraw, rects.FillMin, rects.FillMax, Bar.fMaximum);
        pDraw->AddRect(rects.BarMin, rects.BarMax, IM_COL32(0, 0, 0, 240), 0.f, 0, 1.25f);
    }
}

void CUI_Manager::DrawHealthBarsRHI(const DirectX::XMMATRIX& mVP)
{
    if (!m_bHasWorldHealthBarState || !m_pRHIUIRenderer)
        return;

    const f32_t w = m_fHPBarWidth;
    const f32_t h = m_fHPBarHeight;
    const f32_t yOff = m_fHPBarYOffset;
    const Vec4 uvFull(0.f, 0.f, 1.f, 1.f);

    for (const UIWorldHealthBarDesc& Bar : m_WorldHealthBars)
    {
        if (Bar.Kind != UIWorldHealthBarKind::Character ||
            Bar.bDead ||
            Bar.fMaximum <= 0.f)
        {
            continue;
        }

        const Vec3 p = Bar.vWorldPos;
        ImVec2 s;
        if (!UI_WorldToScreen(mVP, { p.x, p.y + yOff, p.z }, s,
            m_iWinSizeX, m_iWinSizeY))
        {
            continue;
        }

        const f32_t clamped = std::clamp(Bar.fCurrent / Bar.fMaximum, 0.f, 1.f);
        const bool_t bHasResource =
            Bar.ResourceKind != eUIResourceKind::None;
        const HealthBarScreenRects rects =
            BuildHealthBarScreenRects(s, w, h, bHasResource);
        const bool_t bAlly = UI_IsTeamAlly(Bar.iTeam, m_iWorldHealthBarLocalTeam);

        m_pRHIUIRenderer->DrawImage(
            nullptr,
            rects.BarMin.x,
            rects.BarMin.y,
            rects.BarMax.x - rects.BarMin.x,
            rects.BarMax.y - rects.BarMin.y,
            uvFull,
            Vec4(0.04f, 0.04f, 0.04f, 0.89f));

        const f32_t fillW = rects.FillMax.x - rects.FillMin.x;
        const f32_t trailRatio = ResolveCharacterHealthTrailRatio(Bar.Entity, clamped);
        if (trailRatio > clamped + 0.002f)
        {
            const f32_t trailMinX = rects.FillMin.x + fillW * clamped;
            const f32_t trailMaxX = rects.FillMin.x + fillW * trailRatio;
            m_pRHIUIRenderer->DrawImage(
                nullptr,
                trailMinX,
                rects.FillMin.y,
                trailMaxX - trailMinX,
                rects.FillMax.y - rects.FillMin.y,
                uvFull,
                Vec4(1.0f, 0.055f, 0.025f, 0.95f));
            m_pRHIUIRenderer->DrawImage(
                nullptr,
                trailMinX,
                rects.FillMin.y,
                trailMaxX - trailMinX,
                1.0f,
                uvFull,
                Vec4(1.0f, 0.50f, 0.33f, 0.43f));
        }

        if (clamped > 0.f)
        {
            m_pRHIUIRenderer->DrawImage(
                nullptr,
                rects.FillMin.x,
                rects.FillMin.y,
                fillW * clamped,
                rects.FillMax.y - rects.FillMin.y,
                uvFull,
                bAlly ? Vec4(0.29f, 0.75f, 0.24f, 1.0f) : Vec4(0.85f, 0.20f, 0.19f, 1.0f));
            m_pRHIUIRenderer->DrawImage(
                nullptr,
                rects.FillMin.x,
                rects.FillMin.y,
                fillW * clamped,
                1.0f,
                uvFull,
                bAlly ? Vec4(0.56f, 1.0f, 0.46f, 0.28f) : Vec4(1.0f, 0.52f, 0.41f, 0.28f));
        }

        f32_t shieldStartRatio = 0.f;
        f32_t shieldEndRatio = 0.f;
        if (UI_ResolveShieldSegment(
                Bar.fCurrent,
                Bar.fMaximum,
                Bar.fShield,
                shieldStartRatio,
                shieldEndRatio))
        {
            m_pRHIUIRenderer->DrawImage(
                nullptr,
                rects.FillMin.x + fillW * shieldStartRatio,
                rects.FillMin.y,
                fillW * (shieldEndRatio - shieldStartRatio),
                rects.FillMax.y - rects.FillMin.y,
                uvFull,
                Vec4(0.96f, 0.98f, 1.f, 0.60f));
        }

        const f32_t manaMax = Bar.fManaMaximum;
        const f32_t manaRatio = (manaMax > 0.f)
            ? std::clamp(Bar.fManaCurrent / manaMax, 0.f, 1.f)
            : 0.f;
        if (bHasResource && manaMax > 0.f)
        {
            m_pRHIUIRenderer->DrawImage(
                nullptr,
                rects.ManaMin.x,
                rects.ManaMin.y,
                rects.ManaMax.x - rects.ManaMin.x,
                rects.ManaMax.y - rects.ManaMin.y,
                uvFull,
                Vec4(0.024f, 0.05f, 0.098f, 0.92f));
            if (manaRatio > 0.f)
            {
                const f32_t manaW = (rects.ManaMax.x - rects.ManaMin.x) * manaRatio;
                m_pRHIUIRenderer->DrawImage(
                    nullptr,
                    rects.ManaMin.x,
                    rects.ManaMin.y,
                    manaW,
                    rects.ManaMax.y - rects.ManaMin.y,
                    uvFull,
                    Bar.ResourceKind == eUIResourceKind::Energy
                        ? Vec4(0.93f, 0.75f, 0.14f, 1.f)
                        : Bar.ResourceKind == eUIResourceKind::Flow
                            ? Vec4(0.89f, 0.93f, 0.97f, 1.f)
                            : Vec4(0.14f, 0.49f, 0.89f, 1.0f));
                m_pRHIUIRenderer->DrawImage(
                    nullptr,
                    rects.ManaMin.x,
                    rects.ManaMin.y,
                    manaW,
                    1.0f,
                    uvFull,
                    Bar.ResourceKind == eUIResourceKind::Energy
                        ? Vec4(1.f, 0.92f, 0.46f, 0.43f)
                        : Bar.ResourceKind == eUIResourceKind::Flow
                            ? Vec4(1.f, 1.f, 1.f, 0.47f)
                            : Vec4(0.42f, 0.82f, 1.0f, 0.36f));
            }
        }
    }
}

void CUI_Manager::DrawUnitHealthBars(ImDrawList* pDraw, const DirectX::XMMATRIX& mVP)
{
    if (!m_bHasWorldHealthBarState || !pDraw)
        return;

    const f32_t h = m_fUnitHPBarHeight;
    const f32_t yOff = m_fUnitHPBarYOffset;

    for (const UIWorldHealthBarDesc& Bar : m_WorldHealthBars)
    {
        if (Bar.Kind != UIWorldHealthBarKind::Unit ||
            Bar.bDead ||
            Bar.fMaximum <= 0.f)
        {
            continue;
        }

        const f32_t w = m_fUnitHPBarWidth *
            ((Bar.fWidthScale > 0.f) ? Bar.fWidthScale : 1.f);

        const Vec3 p = Bar.vWorldPos;
        ImVec2 screen;
        if (!UI_WorldToScreen(mVP, { p.x, p.y + yOff, p.z }, screen,
            m_iWinSizeX, m_iWinSizeY))
        {
            continue;
        }

        const f32_t clamped = std::clamp(Bar.fCurrent / Bar.fMaximum, 0.f, 1.f);
        const ImVec2 barMin(screen.x - w * 0.5f, screen.y - h * 0.5f);
        const ImVec2 barMax(screen.x + w * 0.5f, screen.y + h * 0.5f);
        const f32_t fillW = w * clamped;
        const ImVec2 fillMax(barMin.x + fillW, barMax.y);
        const bool_t bAllyTeam = UI_IsTeamAlly(Bar.iTeam, m_iWorldHealthBarLocalTeam);

        pDraw->AddRectFilled(barMin, barMax, IM_COL32(14, 14, 14, 232));
        if (fillW > 0.5f)
        {
            pDraw->AddRectFilled(
                barMin,
                fillMax,
                bAllyTeam ? IM_COL32(48, 134, 230, 255) : IM_COL32(224, 56, 50, 255));
            pDraw->AddRectFilled(
                barMin,
                ImVec2(fillMax.x, barMin.y + 1.f),
                bAllyTeam ? IM_COL32(118, 204, 255, 84) : IM_COL32(255, 128, 96, 84));
        }
    }
}

void CUI_Manager::DrawUnitHealthBarsRHI(const DirectX::XMMATRIX& mVP)
{
    if (!m_bHasWorldHealthBarState || !m_pRHIUIRenderer)
        return;

    const f32_t h = m_fUnitHPBarHeight;
    const f32_t yOff = m_fUnitHPBarYOffset;
    const Vec4 uvFull(0.f, 0.f, 1.f, 1.f);

    for (const UIWorldHealthBarDesc& Bar : m_WorldHealthBars)
    {
        if (Bar.Kind != UIWorldHealthBarKind::Unit ||
            Bar.bDead ||
            Bar.fMaximum <= 0.f)
        {
            continue;
        }

        const f32_t w = m_fUnitHPBarWidth *
            ((Bar.fWidthScale > 0.f) ? Bar.fWidthScale : 1.f);

        const Vec3 p = Bar.vWorldPos;
        ImVec2 screen;
        if (!UI_WorldToScreen(mVP, { p.x, p.y + yOff, p.z }, screen,
            m_iWinSizeX, m_iWinSizeY))
        {
            continue;
        }

        const f32_t clamped = std::clamp(Bar.fCurrent / Bar.fMaximum, 0.f, 1.f);
        const f32_t x = screen.x - w * 0.5f;
        const f32_t y = screen.y - h * 0.5f;
        const f32_t fillW = w * clamped;
        const bool_t bAllyTeam = UI_IsTeamAlly(Bar.iTeam, m_iWorldHealthBarLocalTeam);

        m_pRHIUIRenderer->DrawImage(
            nullptr,
            x,
            y,
            w,
            h,
            uvFull,
            Vec4(0.055f, 0.055f, 0.055f, 0.91f));

        if (fillW <= 0.5f)
            continue;

        m_pRHIUIRenderer->DrawImage(
            nullptr,
            x,
            y,
            fillW,
            h,
            uvFull,
            bAllyTeam ? Vec4(0.19f, 0.53f, 0.91f, 1.0f) : Vec4(0.88f, 0.22f, 0.20f, 1.0f));
        m_pRHIUIRenderer->DrawImage(
            nullptr,
            x,
            y,
            fillW,
            1.0f,
            uvFull,
            bAllyTeam ? Vec4(0.46f, 0.80f, 1.0f, 0.33f) : Vec4(1.0f, 0.50f, 0.38f, 0.33f));
    }
}

void CUI_Manager::DrawStructureHealthBars(ImDrawList* pDraw, const DirectX::XMMATRIX& mVP)
{
    if (!m_bHasWorldHealthBarState || !pDraw)
        return;

    const f32_t w = m_fStructureHPBarWidth;
    const f32_t h = m_fStructureHPBarHeight;
    const f32_t yOff = m_fStructureHPBarYOffset;

    for (const UIWorldHealthBarDesc& Bar : m_WorldHealthBars)
    {
        if (Bar.Kind != UIWorldHealthBarKind::Structure ||
            Bar.bDead ||
            Bar.fMaximum <= 0.f)
        {
            continue;
        }

        const Vec3 p = Bar.vWorldPos;
        ImVec2 screen;
        if (!UI_WorldToScreen(mVP, { p.x, p.y + yOff, p.z }, screen,
            m_iWinSizeX, m_iWinSizeY))
        {
            continue;
        }

        screen.x += m_fStructureHPBarScreenOffsetX;
        screen.y += m_fStructureHPBarScreenOffsetY;

        const f32_t clamped = std::clamp(Bar.fCurrent / Bar.fMaximum, 0.f, 1.f);
        const ImVec2 barMin(screen.x - w * 0.5f, screen.y - h * 0.5f);
        const ImVec2 barMax(screen.x + w * 0.5f, screen.y + h * 0.5f);
        const f32_t fillW = w * clamped;
        const bool_t bAllyTeam = UI_IsTeamAlly(Bar.iTeam, m_iWorldHealthBarLocalTeam);
        void* pSRV = bAllyTeam ? m_pSRV_StructureBlueHPBar : m_pSRV_StructureRedHPBar;

        pDraw->AddRectFilled(barMin, barMax, IM_COL32(8, 8, 8, 232));
        if (pSRV)
        {
            pDraw->AddImage(
                reinterpret_cast<ImTextureID>(pSRV),
                barMin,
                barMax,
                ImVec2(0.f, 0.f),
                ImVec2(1.f, 1.f),
                IM_COL32(28, 24, 22, 178));
        }

        if (fillW > 0.5f)
        {
            if (pSRV)
            {
                pDraw->AddImage(
                    reinterpret_cast<ImTextureID>(pSRV),
                    barMin,
                    ImVec2(barMin.x + fillW, barMax.y),
                    ImVec2(0.f, 0.f),
                    ImVec2(clamped, 1.f),
                    IM_COL32(255, 255, 255, 255));
            }
            else
            {
                pDraw->AddRectFilled(
                    barMin,
                    ImVec2(barMin.x + fillW, barMax.y),
                    bAllyTeam ? IM_COL32(58, 144, 232, 255) : IM_COL32(225, 64, 56, 255));
            }
        }

        pDraw->AddRect(barMin, barMax, IM_COL32(0, 0, 0, 238), 0.f, 0, 1.25f);
    }
}

void CUI_Manager::DrawStructureHealthBarsRHI(const DirectX::XMMATRIX& mVP)
{
    if (!m_bHasWorldHealthBarState || !m_pRHIUIRenderer)
        return;

    const f32_t w = m_fStructureHPBarWidth;
    const f32_t h = m_fStructureHPBarHeight;
    const f32_t yOff = m_fStructureHPBarYOffset;
    const Vec4 uvFull(0.f, 0.f, 1.f, 1.f);

    for (const UIWorldHealthBarDesc& Bar : m_WorldHealthBars)
    {
        if (Bar.Kind != UIWorldHealthBarKind::Structure ||
            Bar.bDead ||
            Bar.fMaximum <= 0.f)
        {
            continue;
        }

        const Vec3 p = Bar.vWorldPos;
        ImVec2 screen;
        if (!UI_WorldToScreen(mVP, { p.x, p.y + yOff, p.z }, screen,
            m_iWinSizeX, m_iWinSizeY))
        {
            continue;
        }

        screen.x += m_fStructureHPBarScreenOffsetX;
        screen.y += m_fStructureHPBarScreenOffsetY;

        const f32_t clamped = std::clamp(Bar.fCurrent / Bar.fMaximum, 0.f, 1.f);
        const f32_t x = screen.x - w * 0.5f;
        const f32_t y = screen.y - h * 0.5f;
        const f32_t fillW = w * clamped;
        const bool_t bAllyTeam = UI_IsTeamAlly(Bar.iTeam, m_iWorldHealthBarLocalTeam);
        void* pSRV = bAllyTeam ? m_pSRV_StructureBlueHPBar : m_pSRV_StructureRedHPBar;

        m_pRHIUIRenderer->DrawImage(
            nullptr,
            x,
            y,
            w,
            h,
            uvFull,
            Vec4(0.03f, 0.03f, 0.03f, 0.91f));

        if (pSRV)
        {
            m_pRHIUIRenderer->DrawImage(
                pSRV,
                x,
                y,
                w,
                h,
                uvFull,
                Vec4(0.11f, 0.095f, 0.085f, 0.70f));
        }

        if (fillW <= 0.5f)
            continue;

        if (pSRV)
        {
            m_pRHIUIRenderer->DrawImage(
                pSRV,
                x,
                y,
                fillW,
                h,
                Vec4(0.f, 0.f, clamped, 1.f),
                Vec4(1.f, 1.f, 1.f, 1.f));
        }
        else
        {
            m_pRHIUIRenderer->DrawImage(
                nullptr,
                x,
                y,
                fillW,
                h,
                uvFull,
                bAllyTeam ? Vec4(0.23f, 0.57f, 0.91f, 1.f) : Vec4(0.88f, 0.25f, 0.22f, 1.f));
        }
    }
}

void CUI_Manager::DrawHealthBarBarcodeOverlay(ImDrawList* pDraw, const DirectX::XMMATRIX& mVP)
{
    if (!m_bHasWorldHealthBarState || !pDraw)
        return;

    const f32_t w = m_fHPBarWidth;
    const f32_t h = m_fHPBarHeight;
    const f32_t yOff = m_fHPBarYOffset;

    for (const UIWorldHealthBarDesc& Bar : m_WorldHealthBars)
    {
        if (Bar.Kind != UIWorldHealthBarKind::Character ||
            Bar.bDead ||
            Bar.fMaximum <= 0.f)
        {
            continue;
        }

        const Vec3 p = Bar.vWorldPos;
        ImVec2 screen;
        if (!UI_WorldToScreen(mVP, { p.x, p.y + yOff, p.z }, screen,
            m_iWinSizeX, m_iWinSizeY))
        {
            continue;
        }

        const HealthBarScreenRects rects = BuildHealthBarScreenRects(
            screen,
            w,
            h,
            Bar.ResourceKind != eUIResourceKind::None);
        DrawHealthBarcode(pDraw, rects.FillMin, rects.FillMax, Bar.fMaximum);
        pDraw->AddRect(rects.BarMin, rects.BarMax, IM_COL32(0, 0, 0, 240), 0.f, 0, 1.25f);
    }
}
void CUI_Manager::OnImGui_StatusPanelLayoutTuner()
{
    constexpr f32_t kPanelW = 1491.f;
    constexpr f32_t kPanelH = 600.f;
    m_iStatusPanelLayoutTunerFrame = ImGui::GetFrameCount();

    ImGui::SetNextWindowSize(ImVec2(390.f, 680.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(330.f, 360.f), ImVec2(560.f, 900.f));
    if (!ImGui::Begin("Status Panel Layout"))
    {
        ImGui::End();
        return;
    }

    ImGui::PushID("StatusPanelLayout");
    ImGui::TextDisabled("Changes apply immediately.");
    ImGui::Text("Texture: 1491 x 600 (%.3f:1)", kPanelW / kPanelH);

    ImGui::SeparatorText("Panel");
    ImGui::Checkbox("Preview Panel", &m_StatusPanelLayout.bPreviewPanel);
    ImGui::Checkbox("Lock Asset Aspect", &m_StatusPanelLayout.bLockAssetAspect);
    ImGui::SliderFloat(
        "Panel Width",
        &m_StatusPanelLayout.fPanelWidth,
        320.f,
        kPanelW,
        "%.0f px");
    if (m_StatusPanelLayout.bLockAssetAspect)
        m_StatusPanelLayout.fPanelHeight = m_StatusPanelLayout.fPanelWidth * (kPanelH / kPanelW);
    ImGui::BeginDisabled(m_StatusPanelLayout.bLockAssetAspect);
    ImGui::SliderFloat(
        "Panel Height",
        &m_StatusPanelLayout.fPanelHeight,
        220.f,
        900.f,
        "%.1f px");
    ImGui::EndDisabled();
    ImGui::SliderFloat(
        "Panel X",
        &m_StatusPanelLayout.fPanelOffsetX,
        -640.f,
        640.f,
        "%+.0f px");
    ImGui::SliderFloat(
        "Panel Y",
        &m_StatusPanelLayout.fPanelOffsetY,
        -480.f,
        480.f,
        "%+.0f px");

    ImGui::SeparatorText("Team Rows");
    ImGui::SliderFloat("Blue Content X", &m_StatusPanelLayout.fBlueContentX, 0.f, 700.f, "%.0f");
    ImGui::SliderFloat("Red Content X", &m_StatusPanelLayout.fRedContentX, 700.f, 1450.f, "%.0f");
    ImGui::SliderFloat("Row Start Y", &m_StatusPanelLayout.fRowStartY, 40.f, 160.f, "%.1f");
    ImGui::SliderFloat("Row Spacing Y", &m_StatusPanelLayout.fRowSpacingY, 60.f, 125.f, "%.2f");
    ImGui::TextDisabled("Element offsets below are shared by both teams.");

    ImGui::SeparatorText("Portrait / Level");
    ImGui::SliderFloat("Portrait X", &m_StatusPanelLayout.fPortraitOffsetX, -80.f, 180.f, "%.1f");
    ImGui::SliderFloat("Portrait Y", &m_StatusPanelLayout.fPortraitOffsetY, -40.f, 100.f, "%.1f");
    ImGui::SliderFloat("Portrait Size", &m_StatusPanelLayout.fPortraitSize, 16.f, 100.f, "%.1f");
    ImGui::SliderFloat("Level X", &m_StatusPanelLayout.fLevelOffsetX, -40.f, 100.f, "%.1f");
    ImGui::SliderFloat("Level Y", &m_StatusPanelLayout.fLevelOffsetY, -40.f, 100.f, "%.1f");
    ImGui::SliderFloat("Level Font Scale", &m_StatusPanelLayout.fLevelFontScale, 0.35f, 2.f, "%.2f");

    ImGui::SeparatorText("Summoner Spells");
    ImGui::SliderFloat("Spell X", &m_StatusPanelLayout.fSpellOffsetX, -40.f, 260.f, "%.1f");
    ImGui::SliderFloat("Spell Y", &m_StatusPanelLayout.fSpellOffsetY, -40.f, 100.f, "%.1f");
    ImGui::SliderFloat("Spell Size", &m_StatusPanelLayout.fSpellSize, 12.f, 72.f, "%.1f");
    ImGui::SliderFloat("Spell Spacing Y", &m_StatusPanelLayout.fSpellSpacingY, 12.f, 90.f, "%.1f");

    ImGui::SeparatorText("KDA");
    ImGui::SliderFloat("KDA X", &m_StatusPanelLayout.fKdaOffsetX, -40.f, 360.f, "%.1f");
    ImGui::SliderFloat("KDA Y", &m_StatusPanelLayout.fKdaOffsetY, -40.f, 120.f, "%.1f");
    ImGui::SliderFloat("KDA Font Scale", &m_StatusPanelLayout.fKdaFontScale, 0.35f, 2.f, "%.2f");

    ImGui::SeparatorText("Items");
    ImGui::SliderFloat("Item X", &m_StatusPanelLayout.fItemOffsetX, 80.f, 520.f, "%.1f");
    ImGui::SliderFloat("Item Y", &m_StatusPanelLayout.fItemOffsetY, -40.f, 120.f, "%.1f");
    ImGui::SliderFloat("Item Size", &m_StatusPanelLayout.fItemSize, 14.f, 72.f, "%.1f");
    ImGui::SliderFloat("Item Spacing X", &m_StatusPanelLayout.fItemSpacingX, 16.f, 90.f, "%.1f");

    ImGui::SeparatorText("Objective Scores");
    ImGui::SliderFloat(
        "Objective Mirror X",
        &m_StatusPanelLayout.fObjectiveOffsetX,
        -160.f,
        160.f,
        "%+.1f");
    ImGui::SliderFloat("Objective Y", &m_StatusPanelLayout.fObjectiveY, 0.f, 100.f, "%.1f");
    ImGui::SliderFloat(
        "Objective Font Scale",
        &m_StatusPanelLayout.fObjectiveFontScale,
        0.35f,
        2.f,
        "%.2f");

    if (ImGui::Button("Reset Status Panel Layout"))
        m_StatusPanelLayout = StatusPanelLayout{};
    if (ImGui::Button("Save Status Panel Layout"))
    {
        m_strStatusPanelLayoutSaveMessage = SaveStatusPanelLayoutSettings()
            ? "Saved to %LOCALAPPDATA%/Winters/Developer/status_panel_layout.ini"
            : "Save failed; current in-memory values were kept.";
    }
    if (!m_strStatusPanelLayoutSaveMessage.empty())
        ImGui::TextWrapped("%s", m_strStatusPanelLayoutSaveMessage.c_str());

    ImGui::PopID();
    ImGui::End();
}

void CUI_Manager::OnImGui_Tuner()
{
    if (!ImGui::Begin("UI Manager")) { ImGui::End(); return; }

    bool b = (m_bShowHealthBars != 0);
    if (ImGui::Checkbox("HP Bars", &b)) m_bShowHealthBars = b;

    ImGui::Text("HP Green: %s", m_pSRV_HPBarGreen ? "loaded" : "FALLBACK");
    ImGui::Text("HP Red: %s", m_pSRV_HPBarRed ? "loaded" : "FALLBACK");
    ImGui::TextDisabled("World HP bars: character only, ally green / enemy red");
    ImGui::SliderFloat("Bar Width (px)",   &m_fHPBarWidth,   20.f, 200.f);
    ImGui::SliderFloat("Bar Height (px)",  &m_fHPBarHeight,   3.f,  32.f);
    ImGui::SliderFloat("Y Offset (m)",     &m_fHPBarYOffset, 0.5f,  6.f);
    ImGui::Text("Unit Blue HP: %s", m_pSRV_UnitBlueHPBar ? "loaded" : "FALLBACK");
    ImGui::Text("Unit Red HP: %s", m_pSRV_UnitRedHPBar ? "loaded" : "FALLBACK");
    ImGui::TextDisabled("Unit HP bars: team blue/red texture, dark depleted backing");
    ImGui::SliderFloat("Unit Bar Width (px)", &m_fUnitHPBarWidth, 20.f, 100.f);
    ImGui::SliderFloat("Unit Bar Height (px)", &m_fUnitHPBarHeight, 3.f, 16.f);
    ImGui::SliderFloat("Unit Y Offset (m)", &m_fUnitHPBarYOffset, 0.5f, 3.f);
    ImGui::Text("Structure Blue HP: %s", m_pSRV_StructureBlueHPBar ? "loaded" : "FALLBACK");
    ImGui::Text("Structure Red HP: %s", m_pSRV_StructureRedHPBar ? "loaded" : "FALLBACK");
    ImGui::TextDisabled("Structure HP bars: team blue/red PNG, dark depleted backing");
    ImGui::SliderFloat("Structure Bar Width (px)", &m_fStructureHPBarWidth, 50.f, 240.f);
    ImGui::SliderFloat("Structure Bar Height (px)", &m_fStructureHPBarHeight, 6.f, 40.f);
    ImGui::SliderFloat("Structure Y Offset (m)", &m_fStructureHPBarYOffset, 1.f, 8.f);
    ImGui::SliderFloat("Structure Screen X (px)", &m_fStructureHPBarScreenOffsetX, -120.f, 120.f);
    ImGui::SliderFloat("Structure Screen Y (px)", &m_fStructureHPBarScreenOffsetY, -120.f, 120.f);

    ImGui::Separator();
    ImGui::Checkbox("Show Mouse Cursor", &m_bShowMouseCursor);
    ImGui::SliderFloat("Cursor Size", &m_fCursorSize, 16.f, 64.f, "%.0f");
    ImGui::Checkbox("Use Lua UI", &m_bUseLuaUI);
    if (m_pLuaUIHost)
        m_pLuaUIHost->DrawTunerImGui();
    ImGui::Text("Shop Reference: %s", m_pSRV_InGameShopReference ? "loaded" : "FALLBACK");
    ImGui::SliderFloat("Shop Reference Alpha", &m_fInGameShopReferenceAlpha, 0.10f, 1.00f, "%.2f");
    ImGui::Text("Status Panel: %s", m_pSRV_StatusPanel ? "loaded" : "FALLBACK");
    ImGui::TextDisabled("Use the Status Panel Layout window for placement and sizing.");
    ImGui::Checkbox("Show Actor HUD", &m_bShowActorHUD);
    ImGui::Text("HUD Layout: JSON 861x167 bottom-center");
    ImGui::Text("HUD Atlas: %s", m_HudAtlasManifest.FindTexture("hud") ? "loaded" : "FALLBACK");
    ImGui::Text("Ability Atlas: %s", m_pSRV_AbilityAtlas ? "loaded" : "FALLBACK");
    ImGui::Text("Actor Portrait: %s", m_pSRV_ActorPortrait ? "loaded" : "FALLBACK");
    ImGui::Checkbox("HUD Reference", &m_bShowActorHUDReference);
    ImGui::SliderFloat("HUD Reference Alpha", &m_fHUDReferenceAlpha, 0.0f, 1.0f, "%.2f");
    if (m_pActorHudPanel)
        m_pActorHudPanel->DrawLayoutTunerImGui();

    ImGui::Separator();
    ImGui::Checkbox("Show Damage Floaters", &m_bShowDamageFloaters);
    ImGui::SliderFloat("Damage Life", &m_fDamageFloaterLife, 0.35f, 2.0f, "%.2f");
    ImGui::Text("Damage Floaters: %u", static_cast<u32_t>(m_DamageFloaters.size()));
    if (ImGui::Button("Test Damage 123"))
    {
        bool_t bPushed = false;
        for (const UIWorldHealthBarDesc& Bar : m_WorldHealthBars)
        {
            if (Bar.Kind != UIWorldHealthBarKind::Character || Bar.bDead)
                continue;

            Vec3 pos = Bar.vWorldPos;
            pos.y += 2.1f;
            Push_DamageNumber(pos, 123.f, 0u, false, false);
            bPushed = true;
            break;
        }

        if (!bPushed)
            Push_DamageNumber({ 0.f, 2.1f, 0.f }, 123.f, 0u, false, false);
    }

    ImGui::TextDisabled("Phase B+: PlayerHUD / Scoreboard");

    ImGui::End();
}

NS_END
