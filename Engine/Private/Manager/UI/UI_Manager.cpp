#define _CRT_SECURE_NO_WARNINGS
#include "Manager/UI/UI_Manager.h"
#include "ChampionHUDPanel.h"
#include "Manager/UI/LuaUIHost.h"
#include "RHI/RHITypes.h"
#include "Core/CInput.h"
#include "ECS/Components/CoreComponents.h"          // HealthComponent
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/GameplayComponents.h"      // Minion/Champion/Structure team 조회
#include "../../../../Shared/GameSim/Components/GoldComponent.h"
#include "../../../../Shared/GameSim/Components/InventoryComponent.h"
#include "../../../../Shared/GameSim/Components/SkillRankComponent.h"
#include "../../../../Shared/GameSim/Components/StatComponent.h"
#include "../../../../Shared/GameSim/Definitions/ItemDef.h"
#include "WintersMath.h"
#include "WintersPaths.h"
#include <d3d11.h>
#include <DirectXTK/WICTextureLoader.h>
#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <memory>

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
    constexpr const wchar_t* kPathMinionBlueHPBar = L"Resource/Texture/UI/MinionBlueHPBar.png";
    constexpr const wchar_t* kPathMinionRedHPBar = L"Resource/Texture/UI/MinionRedHPBar.png";
    constexpr const wchar_t* kPathTurretBlueHPBar = L"Resource/Texture/UI/TurretHpBarBlue.png";
    constexpr const wchar_t* kPathTurretRedHPBar = L"Resource/Texture/UI/TurretHpBarRed.png";
    constexpr const wchar_t* kPathCursorDefault = L"Resource/Texture/UI/hover_precise.png";
    constexpr const wchar_t* kPathCursorEnemy = L"Resource/Texture/UI/hover_enemy_precise.png";
    constexpr const wchar_t* kPathCursorAttack = L"Resource/Texture/UI/Cursor_Attack_Small.png";
    constexpr const wchar_t* kPathAbilityAtlas = L"Resource/Texture/UI/HUD/clarity_abilityatlas.png";
    constexpr const wchar_t* kPathChampionHUDIrelia = L"Resource/Texture/UI/HUD_Irelia_2.png";
    constexpr const wchar_t* kPathHUDHit = L"Resource/Texture/UI/HUD/lol_ingame_hit.png";
    constexpr const wchar_t* kPathHUDStun = L"Resource/Texture/UI/HUD/lol_ingame_stun.png";
    constexpr const wchar_t* kPathSkillRankPip = L"Resource/Texture/Character/Irelia/particles/defaultcoloroverlifetime.png";
    constexpr const wchar_t* kPathSkillUpgrade = L"Resource/Texture/UI/SkillUpgrade.png";
    constexpr const wchar_t* kPathHUDManifest = L"Resource/UI/hud_atlas_manifest.json";
    constexpr const wchar_t* kPathHUDManifestFallback = L"Client/Bin/Resource/UI/hud_atlas_manifest.json";
    constexpr const wchar_t* kPathHUDLayout = L"Resource/UI/hud_irelia_layout.json";
    constexpr const wchar_t* kPathHUDLayoutFallback = L"Client/Bin/Resource/UI/hud_irelia_layout.json";
    constexpr const wchar_t* kPathInGameShopReference = L"Resource/Texture/UI/상점1.png";
    constexpr const wchar_t* kPathInGameShopManifest = L"Resource/UI/itemshop_atlas_manifest.json";
    constexpr const wchar_t* kPathInGameShopManifestFallback = L"Client/Bin/Resource/UI/itemshop_atlas_manifest.json";
    constexpr const wchar_t* kPathFontHud = L"Resource/Texture/UI/ux/fonts/beaufortforlol-bold.otf";
    constexpr const wchar_t* kPathFontShop = L"Resource/Texture/UI/ux/fonts/spiegel-semibold.otf";
    constexpr const wchar_t* kPathFontShopBody = L"Resource/Texture/UI/ux/fonts/spiegel-regular.otf";
    constexpr const wchar_t* kPathFontFallback = L"Resource/Texture/UI/ux/fonts/notosanscjk-regular.ttf";
    constexpr u8_t kKillFeedObjectChampion = 1;
    constexpr u8_t kKillFeedObjectTurret = 2;
    constexpr u8_t kKillFeedObjectInhibitor = 3;
    constexpr u8_t kKillFeedObjectDragon = 4;
    constexpr u8_t kKillFeedObjectBaron = 5;
    constexpr f32_t kChampionHUDRefW = 861.f;
    constexpr f32_t kChampionHUDRefH = 167.f;

    struct ChampionHudAssetDef
    {
        eChampion champion = eChampion::END;
        const wchar_t* pPortrait = nullptr;
        const wchar_t* pPassive = nullptr;
        const wchar_t* pSkillIcons[4]{};
        const wchar_t* pPassiveBar = nullptr;
    };

    constexpr ChampionHudAssetDef kChampionHudAssets[] =
    {
        {
            eChampion::IRELIA,
            L"Resource/Texture/UI/Champion/Portraits/irelia_circle_0.png",
            L"Resource/Texture/UI/Champion/Icon2d/Irelia_Icon2d/irelia_passive.png",
            {
                L"Resource/Texture/UI/Champion/Icon2d/Irelia_Icon2d/irelia_q.png",
                L"Resource/Texture/UI/Champion/Icon2d/Irelia_Icon2d/irelia_w.png",
                L"Resource/Texture/UI/Champion/Icon2d/Irelia_Icon2d/irelia_e.png",
                L"Resource/Texture/UI/Champion/Icon2d/Irelia_Icon2d/irelia_r.png",
            },
        },
        {
            eChampion::YASUO,
            L"Resource/Texture/UI/Champion/Portraits/yasuo_circle.png",
            L"Resource/Texture/UI/Champion/Icon2d/Yasuo_Icon2d/yasuo_passive.png",
            {
                L"Resource/Texture/UI/Champion/Icon2d/Yasuo_Icon2d/yasuo_q1.png",
                L"Resource/Texture/UI/Champion/Icon2d/Yasuo_Icon2d/yasuo_w.png",
                L"Resource/Texture/UI/Champion/Icon2d/Yasuo_Icon2d/yasuo_e.png",
                L"Resource/Texture/UI/Champion/Icon2d/Yasuo_Icon2d/yasuo_r.png",
            },
            L"Resource/Texture/UI/Champion_Yasuo_PassiveBar.png",
        },
        {
            eChampion::KALISTA,
            L"Resource/Texture/UI/Champion/Portraits/kalista_circle_0.png",
            L"Resource/Texture/UI/Champion/Icon2d/Kalista_Icon2d/kalista_passive.png",
            {
                L"Resource/Texture/UI/Champion/Icon2d/Kalista_Icon2d/kalista_q.png",
                L"Resource/Texture/UI/Champion/Icon2d/Kalista_Icon2d/kalista_w.png",
                L"Resource/Texture/UI/Champion/Icon2d/Kalista_Icon2d/kalista_e.png",
                L"Resource/Texture/UI/Champion/Icon2d/Kalista_Icon2d/kalista_r.png",
            },
        },
        {
            eChampion::SYLAS,
            L"Resource/Texture/UI/Champion/Portraits/sylas_circle_0.png",
            L"Resource/Texture/UI/Champion/Icon2d/Sylas_Icon2d/sylasp.png",
            {
                L"Resource/Texture/UI/Champion/Icon2d/Sylas_Icon2d/sylasq.png",
                L"Resource/Texture/UI/Champion/Icon2d/Sylas_Icon2d/sylasw.png",
                L"Resource/Texture/UI/Champion/Icon2d/Sylas_Icon2d/sylase.png",
                L"Resource/Texture/UI/Champion/Icon2d/Sylas_Icon2d/sylasr.png",
            },
        },
        {
            eChampion::VIEGO,
            L"Resource/Texture/UI/Champion/Portraits/viego_circle.png",
            L"Resource/Texture/UI/Champion/Icon2d/Viego_Icon2d/viego_passive.png",
            {
                L"Resource/Texture/UI/Champion/Icon2d/Viego_Icon2d/viego_q.png",
                L"Resource/Texture/UI/Champion/Icon2d/Viego_Icon2d/viego_w.png",
                L"Resource/Texture/UI/Champion/Icon2d/Viego_Icon2d/viego_e.png",
                L"Resource/Texture/UI/Champion/Icon2d/Viego_Icon2d/viego_r.png",
            },
        },
        {
            eChampion::ANNIE,
            L"Resource/Texture/UI/Champion/Portraits/annie_circle.png",
            L"Resource/Texture/UI/Champion/Icon2d/Annie_Icon2d/annie_passive.png",
            {
                L"Resource/Texture/UI/Champion/Icon2d/Annie_Icon2d/annie_q.png",
                L"Resource/Texture/UI/Champion/Icon2d/Annie_Icon2d/annie_w.png",
                L"Resource/Texture/UI/Champion/Icon2d/Annie_Icon2d/annie_e.png",
                L"Resource/Texture/UI/Champion/Icon2d/Annie_Icon2d/annie_r1.png",
            },
        },
        {
            eChampion::ASHE,
            L"Resource/Texture/UI/Champion/Portraits/ashe_circle.png",
            L"Resource/Texture/UI/Champion/Icon2d/Ashe_Icon2d/ashe_p.png",
            {
                L"Resource/Texture/UI/Champion/Icon2d/Ashe_Icon2d/ashe_q.png",
                L"Resource/Texture/UI/Champion/Icon2d/Ashe_Icon2d/ashe_w.png",
                L"Resource/Texture/UI/Champion/Icon2d/Ashe_Icon2d/ashe_e.png",
                L"Resource/Texture/UI/Champion/Icon2d/Ashe_Icon2d/ashe_r.png",
            },
        },
        {
            eChampion::FIORA,
            L"Resource/Texture/UI/Champion/Portraits/fiora_circle.png",
            L"Resource/Texture/UI/Champion/Icon2d/Fiora_Icon2d/fiora_p.png",
            {
                L"Resource/Texture/UI/Champion/Icon2d/Fiora_Icon2d/fiora_q.png",
                L"Resource/Texture/UI/Champion/Icon2d/Fiora_Icon2d/fiora_w.png",
                L"Resource/Texture/UI/Champion/Icon2d/Fiora_Icon2d/fiora_e.png",
                L"Resource/Texture/UI/Champion/Icon2d/Fiora_Icon2d/fiora_r.png",
            },
        },
        {
            eChampion::GAREN,
            L"Resource/Texture/UI/Champion/Portraits/garen_circle.png",
            L"Resource/Texture/UI/Champion/Icon2d/Garen_Icon2d/garen_passive.png",
            {
                L"Resource/Texture/UI/Champion/Icon2d/Garen_Icon2d/garen_q.png",
                L"Resource/Texture/UI/Champion/Icon2d/Garen_Icon2d/garen_w.png",
                L"Resource/Texture/UI/Champion/Icon2d/Garen_Icon2d/garen_e1.png",
                L"Resource/Texture/UI/Champion/Icon2d/Garen_Icon2d/garen_r.png",
            },
        },
        {
            eChampion::RIVEN,
            L"Resource/Texture/UI/Champion/Portraits/riven_circle.png",
            L"Resource/Texture/UI/Champion/Icon2d/Riven_Icon2d/rivenrunicblades.png",
            {
                L"Resource/Texture/UI/Champion/Icon2d/Riven_Icon2d/rivenbrokenwings.png",
                L"Resource/Texture/UI/Champion/Icon2d/Riven_Icon2d/rivenkishout.png",
                L"Resource/Texture/UI/Champion/Icon2d/Riven_Icon2d/rivenpathoftheexile.png",
                L"Resource/Texture/UI/Champion/Icon2d/Riven_Icon2d/rivenbladeoftheexile.png",
            },
        },
        {
            eChampion::ZED,
            L"Resource/Texture/UI/Champion/Portraits/zed_circle_0.png",
            L"Resource/Texture/UI/Champion/Icon2d/Zed_Icon2d/zedp.png",
            {
                L"Resource/Texture/UI/Champion/Icon2d/Zed_Icon2d/zedq.png",
                L"Resource/Texture/UI/Champion/Icon2d/Zed_Icon2d/zedw.png",
                L"Resource/Texture/UI/Champion/Icon2d/Zed_Icon2d/zede.png",
                L"Resource/Texture/UI/Champion/Icon2d/Zed_Icon2d/zedr.png",
            },
        },
        {
            eChampion::EZREAL,
            L"Resource/Texture/UI/Champion/Portraits/ezreal_circle.png",
            L"Resource/Texture/UI/Champion/Icon2d/Ezreal_Icon2d/ezreal_passive.png",
            {
                L"Resource/Texture/UI/Champion/Icon2d/Ezreal_Icon2d/ezreal_q.png",
                L"Resource/Texture/UI/Champion/Icon2d/Ezreal_Icon2d/ezreal_w.png",
                L"Resource/Texture/UI/Champion/Icon2d/Ezreal_Icon2d/ezreal_e.png",
                L"Resource/Texture/UI/Champion/Icon2d/Ezreal_Icon2d/ezreal_r.png",
            },
        },
        {
            eChampion::YONE,
            L"Resource/Texture/UI/Champion/Portraits/yone_circle_0.png",
            L"Resource/Texture/UI/Champion/Icon2d/Yone_Icon2d/yonepassive.png",
            {
                L"Resource/Texture/UI/Champion/Icon2d/Yone_Icon2d/yoneq.png",
                L"Resource/Texture/UI/Champion/Icon2d/Yone_Icon2d/yonew.png",
                L"Resource/Texture/UI/Champion/Icon2d/Yone_Icon2d/yonee.png",
                L"Resource/Texture/UI/Champion/Icon2d/Yone_Icon2d/yoner.png",
            },
        },
        {
            eChampion::JAX,
            L"Resource/Texture/UI/Champion/Portraits/jax_circle_0.png",
            L"Resource/Texture/UI/Champion/Icon2d/Jax_Icon2D/jaxp.png",
            {
                L"Resource/Texture/UI/Champion/Icon2d/Jax_Icon2D/jaxq.png",
                L"Resource/Texture/UI/Champion/Icon2d/Jax_Icon2D/jaxw.png",
                L"Resource/Texture/UI/Champion/Icon2d/Jax_Icon2D/jaxe.png",
                L"Resource/Texture/UI/Champion/Icon2d/Jax_Icon2D/jaxr.png",
            },
        },
        {
            eChampion::MASTERYI,
            L"Resource/Texture/UI/Champion/Portraits/masteryi_circle_0.png",
            L"Resource/Texture/UI/Champion/Icon2d/MasterYi_Icon2d/masteryi_passive1.png",
            {
                L"Resource/Texture/UI/Champion/Icon2d/MasterYi_Icon2d/masteryi_q.png",
                L"Resource/Texture/UI/Champion/Icon2d/MasterYi_Icon2d/masteryi_w.png",
                L"Resource/Texture/UI/Champion/Icon2d/MasterYi_Icon2d/masteryi_e1.png",
                L"Resource/Texture/UI/Champion/Icon2d/MasterYi_Icon2d/masteryi_r.png",
            },
        },
        {
            eChampion::KINDRED,
            L"Resource/Texture/UI/Champion/Portraits/kindred_circle.png",
            L"Resource/Texture/UI/Champion/Icon2d/Kindred_Icon2d/kindred_passive.png",
            {
                L"Resource/Texture/UI/Champion/Icon2d/Kindred_Icon2d/kindred_q.png",
                L"Resource/Texture/UI/Champion/Icon2d/Kindred_Icon2d/kindred_w.png",
                L"Resource/Texture/UI/Champion/Icon2d/Kindred_Icon2d/kindred_e.png",
                L"Resource/Texture/UI/Champion/Icon2d/Kindred_Icon2d/kindred_r.png",
            },
        },
        {
            eChampion::LEESIN,
            L"Resource/Texture/UI/Champion/Portraits/leesin_circle_0.png",
            L"Resource/Texture/UI/Champion/Icon2d/LeeSin_Icon2d/leesinpassive.png",
            {
                L"Resource/Texture/UI/Champion/Icon2d/LeeSin_Icon2d/leesinq1.png",
                L"Resource/Texture/UI/Champion/Icon2d/LeeSin_Icon2d/leesinw1.png",
                L"Resource/Texture/UI/Champion/Icon2d/LeeSin_Icon2d/leesine1.png",
                L"Resource/Texture/UI/Champion/Icon2d/LeeSin_Icon2d/leesinr.png",
            },
        },
    };

    const ChampionHudAssetDef* FindChampionHudAssets(eChampion champion)
    {
        for (const ChampionHudAssetDef& def : kChampionHudAssets)
        {
            if (def.champion == champion)
                return &def;
        }
        return nullptr;
    }

    const ChampionHudAssetDef* ResolveChampionHudAssets(eChampion champion)
    {
        if (const ChampionHudAssetDef* pDef = FindChampionHudAssets(champion))
            return pDef;
        return &kChampionHudAssets[0];
    }

    f32_t UI_Clamp01(f32_t v)
    {
        if (v < 0.f)
            return 0.f;
        if (v > 1.f)
            return 1.f;
        return v;
    }

    ImU32 UI_ColorWithAlpha(i32_t r, i32_t g, i32_t b, f32_t alpha)
    {
        const i32_t a = static_cast<i32_t>(UI_Clamp01(alpha) * 255.f);
        return IM_COL32(r, g, b, a);
    }

    ImU32 UI_DamageColor(u8_t iDamageType, bool_t bWasCrit, bool_t bKilled, f32_t alpha)
    {
        if (bKilled)
            return UI_ColorWithAlpha(255, 82, 74, alpha);
        if (bWasCrit)
            return UI_ColorWithAlpha(255, 170, 58, alpha);

        switch (iDamageType)
        {
        case 1:
            return UI_ColorWithAlpha(126, 202, 255, alpha);
        case 2:
            return UI_ColorWithAlpha(248, 248, 248, alpha);
        default:
            return UI_ColorWithAlpha(255, 229, 180, alpha);
        }
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

    void AppendItemStatLine(std::vector<std::string>& Lines, const char* pLabel, f32_t Value, bool_t bPercent = false)
    {
        if (Value <= 0.f)
            return;

        char Buffer[64]{};
        if (bPercent)
            std::snprintf(Buffer, sizeof(Buffer), "+%.0f%% %s", Value * 100.f, pLabel);
        else
            std::snprintf(Buffer, sizeof(Buffer), "+%.0f %s", Value, pLabel);
        Lines.emplace_back(Buffer);
    }

    std::vector<std::string> BuildItemStatLines(u16_t iItemId)
    {
        std::vector<std::string> Lines;
        const ItemDef* pItem = CItemRegistry::Instance().Find(iItemId);
        if (!pItem)
            return Lines;

        const ItemStatModifier& Stats = pItem->stats;
        AppendItemStatLine(Lines, "Attack Damage", Stats.flatAd);
        AppendItemStatLine(Lines, "Ability Power", Stats.flatAp);
        AppendItemStatLine(Lines, "Health", Stats.flatHealth);
        AppendItemStatLine(Lines, "Mana", Stats.flatMana);
        AppendItemStatLine(Lines, "Armor", Stats.flatArmor);
        AppendItemStatLine(Lines, "Magic Resist", Stats.flatMr);
        AppendItemStatLine(Lines, "Attack Speed", Stats.bonusAttackSpeed, true);
        AppendItemStatLine(Lines, "Crit Chance", Stats.critChance, true);
        AppendItemStatLine(Lines, "Move Speed", Stats.flatMoveSpeed);
        AppendItemStatLine(Lines, "Life Steal", Stats.lifeSteal, true);

        if (Lines.empty())
            Lines.emplace_back("No direct stats");
        return Lines;
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

HRESULT CUI_Manager::Initialize(CWorld* pWorld,
    IRHIDevice* pDevice,
    uint32_t iWinSizeX, uint32_t iWinSizeY)
{
    if (m_bInitialized) return S_OK;
    m_pWorld = pWorld;
    m_pDevice = pDevice;

    m_iWinSizeX = iWinSizeX;
    m_iWinSizeY = iWinSizeY;

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
    if (FAILED(Load_TextureSRV(kPathMinionBlueHPBar, &m_pSRV_MinionBlueHPBar)))
    {
        OutputDebugStringA("[UI_Manager] MinionBlueHPBar.png load failed - blue minion HP bars use fallback fill\n");
        m_pSRV_MinionBlueHPBar = nullptr;
    }
    if (FAILED(Load_TextureSRV(kPathMinionRedHPBar, &m_pSRV_MinionRedHPBar)))
    {
        OutputDebugStringA("[UI_Manager] MinionRedHPBar.png load failed - red minion HP bars use fallback fill\n");
        m_pSRV_MinionRedHPBar = nullptr;
    }
    if (FAILED(Load_TextureSRV(kPathTurretBlueHPBar, &m_pSRV_TurretBlueHPBar)))
    {
        OutputDebugStringA("[UI_Manager] TurretHpBarBlue.png load failed - blue turret HP bars use fallback fill\n");
        m_pSRV_TurretBlueHPBar = nullptr;
    }
    if (FAILED(Load_TextureSRV(kPathTurretRedHPBar, &m_pSRV_TurretRedHPBar)))
    {
        OutputDebugStringA("[UI_Manager] TurretHpBarRed.png load failed - red turret HP bars use fallback fill\n");
        m_pSRV_TurretRedHPBar = nullptr;
    }

    Load_TextureSRV(kPathCursorDefault, &m_pSRV_CursorDefault);
    Load_TextureSRV(kPathCursorEnemy, &m_pSRV_CursorEnemy);
    Load_TextureSRV(kPathCursorAttack, &m_pSRV_CursorAttack);
    if (FAILED(Load_TextureSRV(kPathAbilityAtlas, &m_pSRV_AbilityAtlas)))
    {
        OutputDebugStringA("[UI_Manager] clarity_abilityatlas.png load failed - ability atlas elements skipped\n");
        m_pSRV_AbilityAtlas = nullptr;
    }
    if (FAILED(Load_TextureSRV(kPathChampionHUDIrelia, &m_pSRV_ChampionHUDBase)))
    {
        OutputDebugStringA("[UI_Manager] HUD_Irelia_2.png load failed - champion HUD base skipped\n");
        m_pSRV_ChampionHUDBase = nullptr;
    }
    if (FAILED(Load_TextureSRV(kPathHUDHit, &m_pSRV_HUDHit)))
    {
        OutputDebugStringA("[UI_Manager] lol_ingame_hit.png load failed - hit flash skipped\n");
        m_pSRV_HUDHit = nullptr;
    }
    if (FAILED(Load_TextureSRV(kPathHUDStun, &m_pSRV_HUDStun)))
    {
        OutputDebugStringA("[UI_Manager] lol_ingame_stun.png load failed - stun flash skipped\n");
        m_pSRV_HUDStun = nullptr;
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

    LoadChampionHUDAssets();
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
    ReleaseChampionHUDAssets();
    m_pRHIUIRenderer.reset();
    ReleaseSRV(m_pSRV_HPBarGreen);
    ReleaseSRV(m_pSRV_HPBarRed);
    ReleaseSRV(m_pSRV_MinionBlueHPBar);
    ReleaseSRV(m_pSRV_MinionRedHPBar);
    ReleaseSRV(m_pSRV_TurretBlueHPBar);
    ReleaseSRV(m_pSRV_TurretRedHPBar);
    ReleaseSRV(m_pSRV_ChampionHUDBase);
    ReleaseSRV(m_pSRV_CursorDefault);
    ReleaseSRV(m_pSRV_CursorEnemy);
    ReleaseSRV(m_pSRV_CursorAttack);
    ReleaseSRV(m_pSRV_AbilityAtlas);
    ReleaseSRV(m_pSRV_HUDHit);
    ReleaseSRV(m_pSRV_HUDStun);
    ReleaseSRV(m_pSRV_SkillRankPip);
    ReleaseSRV(m_pSRV_SkillUpgrade);
    ReleaseSRV(m_pSRV_InGameShopReference);
    for (KillFeedPortraitCache& Portrait : m_KillFeedPortraits)
        ReleaseSRV(Portrait.pSRV);
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
    for (InGameShopItemView& Item : m_InGameShopItems)
        ReleaseSRV(Item.pSRV);
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

HRESULT CUI_Manager::Load_TextureSRV(const wchar_t* pPath, void** ppOut)
{
    ID3D11Device* pNativeDevice = GetNativeDX11Device(m_pDevice);
    if (!pNativeDevice || !ppOut) return E_FAIL;
    *ppOut = nullptr;

    // WIC 로 PNG → ID3D11ShaderResourceView* 직접 생성
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

void CUI_Manager::LoadChampionHUDAssets()
{
    ReleaseChampionHUDAssets();

    m_pChampionHudPanel = std::make_unique<CChampionHudPanel>();
    m_pChampionHudPanel->Initialize(m_pRHIUIRenderer.get(), &m_HudAtlasManifest);
    m_pChampionHudPanel->SetReferenceTexture(m_pSRV_ChampionHUDBase);
    m_pChampionHudPanel->SetPassiveBarTexture(m_pSRV_ChampionPassiveBar);
    m_pChampionHudPanel->SetTextFont(FindUIFont("hud"));
    m_pChampionHudPanel->SetReferenceAlpha(m_fHUDReferenceAlpha);
    m_pChampionHudPanel->SetShowReference(m_bShowChampionHUDReference);

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

    if (!m_pChampionHudPanel->LoadLayout(kPathHUDLayout) &&
        !m_pChampionHudPanel->LoadLayout(kPathHUDLayoutFallback))
    {
        OutputDebugStringA("[UI_Manager] hud_irelia_layout.json load failed - using built-in HUD layout\n");
    }

    const eChampion InitialChampion =
        (m_PlayerChampion != eChampion::END) ? m_PlayerChampion : eChampion::IRELIA;
    LoadChampionHUDAssetsForChampion(InitialChampion);
}

void CUI_Manager::LoadChampionHUDAssetsForChampion(eChampion eChampionID)
{
    if (!m_pChampionHudPanel)
        return;

    const ChampionHudAssetDef* pDef = ResolveChampionHudAssets(eChampionID);
    if (!pDef)
        return;
    if (m_eLoadedChampionHudAssets == pDef->champion)
        return;

    ReleaseSRV(m_pSRV_ChampionPortrait);
    ReleaseSRV(m_pSRV_ChampionPassiveIcon);
    ReleaseSRV(m_pSRV_ChampionPassiveBar);
    for (void*& pIconSRV : m_pSRV_ChampionSkillIcons)
        ReleaseSRV(pIconSRV);

    m_pChampionHudPanel->SetPortraitTexture(nullptr);
    m_pChampionHudPanel->SetPassiveBarTexture(nullptr);
    for (u32_t i = 0; i < m_pSRV_ChampionSkillIcons.size(); ++i)
        m_pChampionHudPanel->SetSkillIconTexture(i, nullptr);

    m_eLoadedChampionHudAssets = pDef->champion;

    if (pDef->pPortrait && SUCCEEDED(Load_TextureSRV(pDef->pPortrait, &m_pSRV_ChampionPortrait)))
        m_pChampionHudPanel->SetPortraitTexture(m_pSRV_ChampionPortrait);
    else
        OutputDebugStringA("[UI_Manager] champion portrait load failed - portrait skipped\n");

    if (pDef->pPassive)
        (void)Load_TextureSRV(pDef->pPassive, &m_pSRV_ChampionPassiveIcon);

    if (pDef->pPassiveBar &&
        SUCCEEDED(Load_TextureSRV(pDef->pPassiveBar, &m_pSRV_ChampionPassiveBar)))
    {
        m_pChampionHudPanel->SetPassiveBarTexture(m_pSRV_ChampionPassiveBar);
    }

    for (u32_t i = 0; i < m_pSRV_ChampionSkillIcons.size(); ++i)
    {
        if (pDef->pSkillIcons[i] &&
            SUCCEEDED(Load_TextureSRV(pDef->pSkillIcons[i], &m_pSRV_ChampionSkillIcons[i])))
        {
            m_pChampionHudPanel->SetSkillIconTexture(i, m_pSRV_ChampionSkillIcons[i]);
        }
    }
}

void CUI_Manager::ReleaseChampionHUDAssets()
{
    m_pChampionHudPanel.reset();

    m_HudAtlasManifest.ForEachTexture(
        [](const std::string&, UIAtlasTextureDef& Texture)
        {
            ReleaseSRV(Texture.pSRV);
        });
    m_HudAtlasManifest.Clear();

    m_eLoadedChampionHudAssets = eChampion::END;
    ReleaseSRV(m_pSRV_ChampionPortrait);
    ReleaseSRV(m_pSRV_ChampionPassiveIcon);
    ReleaseSRV(m_pSRV_ChampionPassiveBar);
    for (void*& pIconSRV : m_pSRV_ChampionSkillIcons)
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

void CUI_Manager::Bind_World(CWorld* pWorld)
{
    const bool_t bNewWorld = pWorld != nullptr && pWorld != m_pWorld;
    m_pWorld = pWorld;
    if (bNewWorld)
    {
        ResetGameContextHUDStats();
        m_ChampionHealthBarTrails.clear();
    }
}

void CUI_Manager::Render_Overlay(const Mat4& matVP)
{
    if (!m_pWorld) return;
    const DirectX::XMMATRIX mVP = matVP.ToXMMATRIX();
    ChampionHUDState HudState{};

    const f32_t fUIDt = ImGui::GetIO().DeltaTime;
    UpdateChampionHealthBarTrails(fUIDt);

    if (m_bShowChampionHUD)
    {
        BuildChampionHUDState(HudState);
        Update_HUDStatusTimers(
            HudState.LocalEntity,
            HudState.Hp,
            HudState.bStunned,
            ImGui::GetIO().DeltaTime);
    }

    const bool_t bUseRHI =
        m_pRHIUIRenderer &&
        m_pRHIUIRenderer->IsReady() &&
        m_iWinSizeX > 0 &&
        m_iWinSizeY > 0;
    if (bUseRHI)
    {
        m_pRHIUIRenderer->Begin(m_iWinSizeX, m_iWinSizeY);
        if (m_bShowHealthBars)
        {
            Draw_HealthBars_RHI(mVP);
            Draw_MinionHealthBars_RHI(mVP);
            Draw_TurretHealthBars_RHI(mVP);
        }
        if (m_bShowChampionHUD)
            DrawChampionHUDRHI(HudState);
        if (m_bUseLuaUI && m_pLuaUIHost)
        {
            m_pLuaUIHost->SetChampionHUDState(HudState);
            m_pLuaUIHost->DrawRHI(m_iWinSizeX, m_iWinSizeY);
        }
        m_pRHIUIRenderer->End();
    }
    ImDrawList* pDraw = ImGui::GetBackgroundDrawList();
    if (m_bShowHealthBars)
    {
        if (bUseRHI)
            DrawHealthBarBarcodeOverlay(pDraw, mVP);
        else
        {
            Draw_HealthBars(pDraw, mVP);
            Draw_MinionHealthBars(pDraw, mVP);
            Draw_TurretHealthBars(pDraw, mVP);
        }
    }
    Draw_DamageFloaters(pDraw, mVP, ImGui::GetIO().DeltaTime);
    // Phase B+: Draw_PlayerHUD / Scoreboard ...

    ImDrawList* pFG = ImGui::GetForegroundDrawList();
    Draw_GameContextHUD(pFG);
    Draw_KillFeedBanners(pFG, ImGui::GetIO().DeltaTime);
    if (m_bShowChampionHUD) DrawChampionHUDOverlay(pFG, HudState);
    if (m_bUseLuaUI && m_pLuaUIHost)
    {
        m_pLuaUIHost->SetChampionHUDState(HudState);
        m_pLuaUIHost->DrawOverlay(pFG);
        m_bInGameShopOpen = m_pLuaUIHost->GetBoolean("InGameShopOpen");
    }
}

void CUI_Manager::Render_Cursor()
{
    if (!m_bShowMouseCursor)
        return;

    if (m_pRHIUIRenderer && m_pRHIUIRenderer->IsReady())
    {
        m_pRHIUIRenderer->Begin(m_iWinSizeX, m_iWinSizeY);
        Draw_MouseCursor_RHI();
        m_pRHIUIRenderer->End();
        return;
    }

    if (!ImGui::GetCurrentContext())
        return;
    ImDrawList* pFG = ImGui::GetForegroundDrawList();
    if (!pFG)
        return;

    Draw_MouseCursor(pFG);
}

void CUI_Manager::Set_PlayerChampion(eChampion champ)
{
    m_PlayerChampion = champ;
}

void CUI_Manager::Set_AttackMode(bool_t b)
{
    m_bAttackMode = b;
    Apply_CursorMode();
}

void CUI_Manager::Set_EnemyHoverCursor(bool_t b)
{
    m_bEnemyHoverCursor = b;
    Apply_CursorMode();
}

void CUI_Manager::Apply_CursorMode()
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

    for (InGameShopItemView& Item : m_InGameShopItems)
        ReleaseSRV(Item.pSRV);
    m_InGameShopItems.clear();

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

    static constexpr u16_t kShopItemIds[] =
    {
        1055, 1056, 1036, 1042, 1028, 1029, 1033,
        1001, 1052, 1037, 1043, 1053, 1058, 1038,
        3153,
    };

    for (const u16_t itemId : kShopItemIds)
    {
        const ItemDef* pItem = CItemRegistry::Instance().Find(itemId);
        if (!pItem)
            continue;

        InGameShopItemView Item{};
        Item.iItemId = pItem->itemId;
        Item.iPrice = pItem->price;
        Item.pName = pItem->displayName;
        wchar_t IconPath[MAX_PATH] = {};
        if (UI_TryFindItemIconPath(Item.iItemId, IconPath, MAX_PATH))
        {
            Item.strIconPath = IconPath;
            if (FAILED(Load_TextureSRV(Item.strIconPath.c_str(), &Item.pSRV)))
                Item.pSRV = nullptr;
        }
        m_InGameShopItems.push_back(Item);
    }

    if (!m_InGameShopItems.empty())
        m_iSelectedInGameShopItemId = m_InGameShopItems.front().iItemId;
    else
        m_iSelectedInGameShopItemId = 0;

    m_strInGameShopStatus = "Left click selects, right click buys";
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

    for (u32_t Index = 0; Index < m_InGameShopItems.size(); ++Index)
    {
        const u32_t Row = Index / 7u;
        const u32_t Col = Index % 7u;
        if (Row >= 2u)
            break;

        const f32_t X = kStartX + static_cast<f32_t>(Col) * kIconStepX;
        const f32_t Y = kStartY + static_cast<f32_t>(Row) * kRowStepY;
        const ImVec2 CellMin = ToShop(X, Y);
        const ImVec2 CellMax = ToShop(X + kIconSize, Y + kIconSize);
        const bool_t bHovered = PointInRect(IO.MousePos, CellMin, CellMax);

        const InGameShopItemView& Item = m_InGameShopItems[Index];
        if (bHovered && IO.MouseClicked[0])
        {
            m_iSelectedInGameShopItemId = Item.iItemId;
            m_strInGameShopStatus = Item.pName ? Item.pName : "Item selected";
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
    if (!pSelectedItem && !m_InGameShopItems.empty())
        pSelectedItem = &m_InGameShopItems.front();

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
            pSelectedItem->pName ? pSelectedItem->pName : "Selected Item");

        char DetailPriceText[32]{};
        std::snprintf(DetailPriceText, sizeof(DetailPriceText), "%u", static_cast<u32_t>(pSelectedItem->iPrice));
        UI_DrawOutlinedText(
            pDraw,
            pFont,
            FontSize,
            ToShop(820.f, 158.f),
            IM_COL32(255, 217, 91, 255),
            DetailPriceText);

        const std::vector<std::string> StatLines = BuildItemStatLines(pSelectedItem->iItemId);
        for (u32_t LineIndex = 0; LineIndex < StatLines.size() && LineIndex < 8u; ++LineIndex)
        {
            UI_DrawOutlinedText(
                pDraw,
                pFont,
                SmallFontSize,
                ToShop(820.f, 190.f + static_cast<f32_t>(LineIndex) * 18.f),
                MutedColor,
                StatLines[LineIndex].c_str());
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

bool_t CUI_Manager::TryApplyPredictedInGamePurchase(u16_t iItemId)
{
    if (!m_pWorld)
        return false;

    const ItemDef* pItem = CItemRegistry::Instance().Find(iItemId);
    if (!pItem)
        return false;

    bool_t bApplied = false;
    bool_t bFoundLocal = false;
    m_pWorld->ForEach<ChampionComponent, LocalPlayerTag>(
        [&](EntityID Entity, ChampionComponent&, LocalPlayerTag&)
        {
            if (bFoundLocal)
                return;
            bFoundLocal = true;

            if (!m_pWorld->HasComponent<GoldComponent>(Entity) ||
                !m_pWorld->HasComponent<InventoryComponent>(Entity))
            {
                return;
            }

            GoldComponent& Gold = m_pWorld->GetComponent<GoldComponent>(Entity);
            InventoryComponent& Inventory = m_pWorld->GetComponent<InventoryComponent>(Entity);
            if (Inventory.count >= InventoryComponent::kMaxSlots)
            {
                m_strInGameShopStatus = "Inventory full";
                return;
            }
            if (Gold.amount < pItem->price)
            {
                m_strInGameShopStatus = "Not enough gold";
                return;
            }

            Gold.amount -= pItem->price;
            Inventory.itemIds[Inventory.count++] = pItem->itemId;

            //Item 구매 시 스탯 적용
            if (m_pWorld->HasComponent<StatComponent>(Entity))
            {
                StatComponent& stat = m_pWorld->GetComponent<StatComponent>(Entity);
                stat.bonusAd += pItem->stats.flatAd;
                stat.ap += pItem->stats.flatAp;
                stat.hpMax += pItem->stats.flatHealth;
                stat.manaMax += pItem->stats.flatMana;
                stat.bonusArmor += pItem->stats.flatArmor;
                stat.bonusMr += pItem->stats.flatMr;
            }

            m_iInGameGold = Gold.amount;
            m_InGameInventorySlots.fill(0);
            const u32_t Count = std::min<u32_t>(Inventory.count, InventoryComponent::kMaxSlots);
            for (u32_t Index = 0; Index < Count; ++Index)
                m_InGameInventorySlots[Index] = Inventory.itemIds[Index];
            bApplied = true;
        });

    return bApplied;
}

bool_t CUI_Manager::TryBuyInGameItem(u16_t iItemId)
{
    if (iItemId == 0 || !m_pfnBuyItem)
    {
        OutputDebugStringA("[UI_Manager] BuyItem command callback missing\n");
        return false;
    }

    m_pfnBuyItem(m_pBuyItemUser, iItemId);
    if (TryApplyPredictedInGamePurchase(iItemId))
        m_strInGameShopStatus = "Buy requested";
    OutputDebugStringA("[UI_Manager] BuyItem command requested\n");
    return true;
}

void CUI_Manager::Push_DamageNumber(const Vec3& vWorldPos, f32_t fAmount,
    u8_t iDamageType, bool_t bWasCrit, bool_t bKilled)
{
    if (fAmount <= 0.f)
        return;

    if (m_DamageFloaters.size() >= 128)
        m_DamageFloaters.erase(m_DamageFloaters.begin());

    DamageFloater floater{};
    floater.vWorldPos = vWorldPos;
    floater.fAmount = fAmount;
    floater.fLifetime = bKilled ? (m_fDamageFloaterLife + 0.25f) : m_fDamageFloaterLife;
    floater.fRisePixels = bWasCrit ? (m_fDamageFloaterRise + 12.f) : m_fDamageFloaterRise;
    floater.iDamageType = iDamageType;
    floater.bWasCrit = bWasCrit;
    floater.bKilled = bKilled;

    const u32_t seed =
        static_cast<u32_t>(m_DamageFloaters.size() * 37u) ^
        static_cast<u32_t>(fAmount * 17.f);
    floater.fXJitter = static_cast<f32_t>(static_cast<i32_t>(seed % 17u) - 8) * 1.5f;

    m_DamageFloaters.push_back(floater);
}

void CUI_Manager::Push_KillFeedBanner(eChampion eSourceChampion, eChampion eTargetChampion,
    u8_t iObjectKind, bool_t bSourceAlly, const char* pMessage)
{
    if (!pMessage || pMessage[0] == '\0')
        return;

    if (m_KillFeedBanners.size() >= 5)
        m_KillFeedBanners.erase(m_KillFeedBanners.begin());

    KillFeedBanner banner{};
    banner.eSourceChampion = eSourceChampion;
    banner.eTargetChampion = eTargetChampion;
    banner.iObjectKind = iObjectKind;
    banner.bSourceAlly = bSourceAlly;
    banner.strMessage = pMessage;
    m_KillFeedBanners.push_back(banner);
}

void CUI_Manager::RecordGameContextChampionKill(u8_t iSourceTeam, u8_t iTargetTeam,
    bool_t bLocalSource, bool_t bLocalTarget)
{
    (void)iTargetTeam;

    if (iSourceTeam == static_cast<u8_t>(eTeam::Blue))
        ++m_GameContextHUD.iBlueKills;
    else if (iSourceTeam == static_cast<u8_t>(eTeam::Red))
        ++m_GameContextHUD.iRedKills;

    if (bLocalSource)
        ++m_GameContextHUD.iLocalKills;
    if (bLocalTarget)
        ++m_GameContextHUD.iLocalDeaths;
}

void CUI_Manager::RecordGameContextMinionKill()
{
    ++m_GameContextHUD.iLocalMinionKills;
}

void CUI_Manager::SetGameContextServerTimeMs(u64_t iServerTimeMs)
{
    m_GameContextHUD.iServerTimeMs = iServerTimeMs;
    m_GameContextHUD.bHasServerTime = true;
}

void CUI_Manager::ResetGameContextHUDStats()
{
    m_GameContextHUD = GameContextHUDState{};
}

void CUI_Manager::Draw_GameContextHUD(ImDrawList* pDraw)
{
    if (!m_bShowGameContextHUD || !pDraw)
        return;

    ImFont* pFont = FindUIFont("hud");
    if (!pFont)
        return;

    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    const f32_t screenW = (m_iWinSizeX > 0) ? static_cast<f32_t>(m_iWinSizeX) : displaySize.x;
    const f32_t screenH = (m_iWinSizeY > 0) ? static_cast<f32_t>(m_iWinSizeY) : displaySize.y;
    if (screenW <= 0.f || screenH <= 0.f)
        return;

    const f32_t hudW = m_fGameContextHUDWidth;
    const f32_t hudH = m_fGameContextHUDHeight;
    const ImVec2 root(screenW - hudW - 18.f, 8.f);
    const ImVec2 max(root.x + hudW, root.y + hudH);

    if (!UI_DrawManifestSprite(pDraw, m_HudAtlasManifest, "gamecontext.background", root, max, 1.f))
        pDraw->AddRectFilled(root, max, IM_COL32(14, 28, 34, 210), 0.f);

    constexpr f32_t kFontSize = 16.f;
    constexpr f32_t kSmallFontSize = 15.f;
    const ImU32 BlueColor = IM_COL32(112, 178, 255, 255);
    const ImU32 RedColor = IM_COL32(245, 104, 104, 255);
    const ImU32 TextColor = IM_COL32(238, 224, 177, 255);

    char buffer[32]{};
    std::snprintf(buffer, sizeof(buffer), "%u", static_cast<u32_t>(m_GameContextHUD.iBlueKills));
    UI_DrawOutlinedText(pDraw, pFont, kFontSize, ImVec2(root.x + 26.f, root.y + 8.f), BlueColor, buffer);

    UI_DrawManifestSprite(
        pDraw,
        m_HudAtlasManifest,
        "gamecontext.vs",
        ImVec2(root.x + 55.f, root.y + 7.f),
        ImVec2(root.x + 79.f, root.y + 26.f),
        1.f);

    std::snprintf(buffer, sizeof(buffer), "%u", static_cast<u32_t>(m_GameContextHUD.iRedKills));
    UI_DrawOutlinedText(pDraw, pFont, kFontSize, ImVec2(root.x + 90.f, root.y + 8.f), RedColor, buffer);

    UI_DrawManifestSprite(
        pDraw,
        m_HudAtlasManifest,
        "gamecontext.kda.icon",
        ImVec2(root.x + 130.f, root.y + 8.f),
        ImVec2(root.x + 146.f, root.y + 27.f),
        1.f);
    std::snprintf(buffer, sizeof(buffer), "%u/%u/%u",
        static_cast<u32_t>(m_GameContextHUD.iLocalKills),
        static_cast<u32_t>(m_GameContextHUD.iLocalDeaths),
        static_cast<u32_t>(m_GameContextHUD.iLocalAssists));
    UI_DrawOutlinedText(pDraw, pFont, kSmallFontSize, ImVec2(root.x + 153.f, root.y + 9.f), TextColor, buffer);

    UI_DrawManifestSprite(
        pDraw,
        m_HudAtlasManifest,
        "gamecontext.minion.icon",
        ImVec2(root.x + 222.f, root.y + 5.f),
        ImVec2(root.x + 243.f, root.y + 30.f),
        1.f);
    std::snprintf(buffer, sizeof(buffer), "%u",
        static_cast<u32_t>(m_GameContextHUD.iLocalMinionKills));
    UI_DrawOutlinedText(pDraw, pFont, kSmallFontSize, ImVec2(root.x + 250.f, root.y + 9.f), TextColor, buffer);

    UI_DrawManifestSprite(
        pDraw,
        m_HudAtlasManifest,
        "gamecontext.time.icon",
        ImVec2(root.x + 296.f, root.y + 8.f),
        ImVec2(root.x + 316.f, root.y + 26.f),
        1.f);
    const u64_t totalSec = m_GameContextHUD.bHasServerTime
        ? (m_GameContextHUD.iServerTimeMs / 1000ull)
        : 0ull;
    std::snprintf(buffer, sizeof(buffer), "%llu:%02llu",
        static_cast<unsigned long long>(totalSec / 60ull),
        static_cast<unsigned long long>(totalSec % 60ull));
    UI_DrawOutlinedText(pDraw, pFont, kSmallFontSize, ImVec2(root.x + 322.f, root.y + 9.f), TextColor, buffer);
}

void* CUI_Manager::FindOrLoadKillFeedPortrait(eChampion eChampionID)
{
    if (eChampionID == eChampion::NONE || eChampionID == eChampion::END)
        return nullptr;

    for (const KillFeedPortraitCache& Portrait : m_KillFeedPortraits)
    {
        if (Portrait.eChampionID == eChampionID)
            return Portrait.pSRV;
    }

    void* pSRV = nullptr;
    if (const ChampionHudAssetDef* pDef = FindChampionHudAssets(eChampionID))
    {
        if (pDef->pPortrait)
            (void)Load_TextureSRV(pDef->pPortrait, &pSRV);
    }

    KillFeedPortraitCache cache{};
    cache.eChampionID = eChampionID;
    cache.pSRV = pSRV;
    m_KillFeedPortraits.push_back(cache);
    return pSRV;
}

void CUI_Manager::Draw_KillFeedCircleImage(ImDrawList* pDraw, const ImVec2& vCenter,
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

void CUI_Manager::Draw_KillFeedBanners(ImDrawList* pDraw, f32_t fDeltaTime)
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

        Draw_KillFeedCircleImage(
            pDraw,
            sourceCenter,
            kRadius,
            FindOrLoadKillFeedPortrait(banner.eSourceChampion),
            tintColor,
            banner.bSourceAlly ? allyColor : enemyColor);

        UI_DrawOutlinedText(pDraw, pFont, kFontSize, textPos, textColor, pMessage);

        if (banner.iObjectKind == kKillFeedObjectChampion)
        {
            Draw_KillFeedCircleImage(
                pDraw,
                targetCenter,
                kRadius,
                FindOrLoadKillFeedPortrait(banner.eTargetChampion),
                tintColor,
                banner.bSourceAlly ? enemyColor : allyColor);
            continue;
        }

        const char* pLabel = nullptr;
        ImU32 badgeColor = neutralColor;
        switch (banner.iObjectKind)
        {
        case kKillFeedObjectTurret:
            pLabel = "포탑";
            badgeColor = banner.bSourceAlly ? enemyColor : allyColor;
            break;
        case kKillFeedObjectInhibitor:
            pLabel = "억제기";
            badgeColor = banner.bSourceAlly ? enemyColor : allyColor;
            break;
        case kKillFeedObjectDragon:
            pLabel = "용";
            break;
        case kKillFeedObjectBaron:
            pLabel = "내셔";
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

void CUI_Manager::Draw_MouseCursor(ImDrawList* pDraw)
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

void CUI_Manager::Draw_MouseCursor_RHI()
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

static bool_t UI_CanLevelSkillByChampionLevel(u8_t slot, u8_t currentRank, u8_t championLevel)
{
    if (slot == 4)
    {
        if (currentRank == 0)
            return championLevel >= 6;
        if (currentRank == 1)
            return championLevel >= 11;
        if (currentRank == 2)
            return championLevel >= 16;
        return false;
    }

    if (slot >= 1 && slot <= 3)
        return championLevel >= static_cast<u8_t>(1 + currentRank * 2);

    return false;
}

static bool_t UI_PointInRect(const ImVec2& p, const ImVec2& a, const ImVec2& b)
{
    return p.x >= a.x && p.x <= b.x && p.y >= a.y && p.y <= b.y;
}

void CUI_Manager::Update_HUDStatusTimers(EntityID localEntity, f32_t hp, bool_t bStunned, f32_t dt)
{
    if (localEntity == NULL_ENTITY)
    {
        m_fHUDHitFlashTimer = (m_fHUDHitFlashTimer > dt) ? (m_fHUDHitFlashTimer - dt) : 0.f;
        m_fHUDStunFlashTimer = (m_fHUDStunFlashTimer > dt) ? (m_fHUDStunFlashTimer - dt) : 0.f;
        return;
    }

    if (m_LastHUDLocalEntity != localEntity)
    {
        m_LastHUDLocalEntity = localEntity;
        m_fLastHUDHP = hp;
        m_bHasLastHUDHP = true;
        m_bWasLocalStunned = bStunned;
    }
    else
    {
        if (m_bHasLastHUDHP && hp + 0.01f < m_fLastHUDHP)
            m_fHUDHitFlashTimer = m_fHUDHitFlashDuration;
        if (bStunned && !m_bWasLocalStunned)
            m_fHUDStunFlashTimer = m_fHUDStunFlashDuration;

        m_fLastHUDHP = hp;
        m_bHasLastHUDHP = true;
        m_bWasLocalStunned = bStunned;
    }

    m_fHUDHitFlashTimer = (m_fHUDHitFlashTimer > dt) ? (m_fHUDHitFlashTimer - dt) : 0.f;
    m_fHUDStunFlashTimer = (m_fHUDStunFlashTimer > dt) ? (m_fHUDStunFlashTimer - dt) : 0.f;
}
//피해 체력 범위가 오른쪽에서부터 사라짐
void CUI_Manager::UpdateChampionHealthBarTrails(f32_t dt)
{
    if (!m_pWorld)
        return;

    dt = std::clamp(dt, 0.f, 0.1f);

    std::vector<EntityID> visibleChampions;
    m_pWorld->ForEach<HealthComponent, TransformComponent>(
        [&](EntityID id, HealthComponent& hp, TransformComponent&)
        {
            if (!m_pWorld->HasComponent<ChampionComponent>(id) ||
                hp.bIsDead || hp.fMaximum <= 0.f)
                return;

            visibleChampions.push_back(id);

            const f32_t ratio = std::clamp(hp.fCurrent / hp.fMaximum, 0.f, 1.f);
            ChampionHealthBarTrailState& state = m_ChampionHealthBarTrails[id];
            if (!state.bInitialized)
            {
                state.fLastRatio = ratio;
                state.fTrailRatio = ratio;
                state.bInitialized = true;
                return;
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
    );
    for (auto it = m_ChampionHealthBarTrails.begin(); it != m_ChampionHealthBarTrails.end();)
    {
        if (std::find(visibleChampions.begin(), visibleChampions.end(), it->first) == visibleChampions.end())
            it = m_ChampionHealthBarTrails.erase(it);
        else
            ++it;
    }
}

f32_t CUI_Manager::ResolveChampionHealthTrailRatio(EntityID entity, f32_t currentRatio) const
{
    const auto it = m_ChampionHealthBarTrails.find(entity);
    if (it == m_ChampionHealthBarTrails.end())
        return currentRatio;

    return std::max(currentRatio, std::clamp(it->second.fTrailRatio, 0.f, 1.f));
}

void CUI_Manager::Draw_HUDStatusFlash(ImDrawList* pDraw, const ImVec2& root, f32_t hudW, f32_t hudH)
{
    if (!m_bShowHUDStatusFlash || !pDraw)
        return;

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    auto drawFlash = [&](void* pSRV, f32_t timer, f32_t duration)
    {
        if (timer <= 0.f || duration <= 0.f)
            return;

        f32_t alphaRatio = timer / duration;
        if (alphaRatio < 0.f)
            alphaRatio = 0.f;
        if (alphaRatio > 1.f)
            alphaRatio = 1.f;

        const f32_t targetW = display.x;
        const f32_t targetH = targetW * 0.5f;
        const ImVec2 a(0.f, (display.y - targetH) * 0.5f);
        const ImVec2 b(targetW, a.y + targetH);
        const u8_t alpha = static_cast<u8_t>(180.f * alphaRatio);

        if (pSRV)
        {
            pDraw->AddImage(
                reinterpret_cast<ImTextureID>(pSRV),
                a,
                b,
                ImVec2(0.f, 0.f),
                ImVec2(1.f, 1.f),
                IM_COL32(255, 255, 255, alpha));
        }
    };

    (void)root;
    (void)hudW;
    (void)hudH;

    drawFlash(m_pSRV_HUDHit, m_fHUDHitFlashTimer, m_fHUDHitFlashDuration);
    drawFlash(m_pSRV_HUDStun, m_fHUDStunFlashTimer, m_fHUDStunFlashDuration);
}

void CUI_Manager::BuildChampionHUDState(ChampionHUDState& State)
{
    if (m_PlayerChampion != eChampion::END)
        State.Champion = m_PlayerChampion;
    State.Gold = m_iInGameGold;
    State.InventoryItemIds = m_InGameInventorySlots;
    State.bShopOpen = m_bInGameShopOpen;

    bool_t bFoundLocal = false;
    if (!m_pWorld)
        return;

    m_pWorld->ForEach<ChampionComponent, LocalPlayerTag>(
        [&](EntityID Entity, ChampionComponent& Champion, LocalPlayerTag&)
        {
            if (bFoundLocal)
                return;

            State.Champion = Champion.id;
            State.LocalEntity = Entity;
            State.Hp = Champion.hp;
            State.MaxHp = Champion.maxHp;
            State.Mp = Champion.mana;
            State.MaxMp = Champion.maxMana;
            State.Shield = Champion.shield;
            State.Level = Champion.level;
            if (Champion.id == eChampion::YASUO)
            {
                State.bUsesPassiveResource = true;
                State.PassiveValue = Champion.mana;
                State.PassiveMax = (Champion.maxMana > 0.f) ? Champion.maxMana : 100.f;
                State.PassiveShield = Champion.shield;
                State.PassiveShieldMax = State.PassiveMax;
                if (m_pWorld->HasComponent<YasuoStateComponent>(Entity))
                {
                    YasuoStateComponent& YasuoState = m_pWorld->GetComponent<YasuoStateComponent>(Entity);
                    State.PassiveValue = YasuoState.fPassiveFlow;
                    State.PassiveMax = (YasuoState.fPassiveFlowMax > 0.f)
                        ? YasuoState.fPassiveFlowMax
                        : 100.f;
                    State.PassiveShield = YasuoState.fPassiveShieldRemaining;
                    State.PassiveShieldMax = (YasuoState.fPassiveShieldMax > 0.f)
                        ? YasuoState.fPassiveShieldMax
                        : State.PassiveMax;
                }
            }
            for (u32_t Index = 0; Index < State.Cooldowns.size(); ++Index)
            {
                State.Cooldowns[Index] = Champion.cooldowns[Index];
                State.MaxCooldowns[Index] = 0.f;
            }

            if (m_pWorld->HasComponent<SkillStateComponent>(Entity))
            {
                SkillStateComponent& SkillState = m_pWorld->GetComponent<SkillStateComponent>(Entity);
                for (u32_t Index = 0; Index < State.Cooldowns.size(); ++Index)
                {
                    const SkillSlotRuntime& Slot = SkillState.slots[Index + 1u];
                    State.Cooldowns[Index] = Slot.cooldownRemaining;
                    State.MaxCooldowns[Index] = Slot.cooldownDuration;
                    if (State.Cooldowns[Index] <= 0.f)
                        State.MaxCooldowns[Index] = 0.f;
                    else if (State.MaxCooldowns[Index] < State.Cooldowns[Index])
                        State.MaxCooldowns[Index] = State.Cooldowns[Index];
                }
            }

            if (m_pWorld->HasComponent<HealthComponent>(Entity))
            {
                HealthComponent& Health = m_pWorld->GetComponent<HealthComponent>(Entity);
                State.Hp = Health.fCurrent;
                State.MaxHp = Health.fMaximum;
            }

            if (m_pWorld->HasComponent<ExperienceComponent>(Entity))
            {
                ExperienceComponent& Experience = m_pWorld->GetComponent<ExperienceComponent>(Entity);
                State.XpCurrent = Experience.current;
                State.XpRequired = Experience.requiredForNextLevel;
                State.XpRatio = (Experience.requiredForNextLevel > 0.f)
                    ? (Experience.current / Experience.requiredForNextLevel)
                    : 0.f;
                if (Experience.level > 0)
                    State.Level = Experience.level;
            }

            if (m_pWorld->HasComponent<SkillRankComponent>(Entity))
            {
                SkillRankComponent& Ranks = m_pWorld->GetComponent<SkillRankComponent>(Entity);
                for (u32_t Index = 0; Index < SkillRankComponent::kSlotCount; ++Index)
                    State.SkillRanks[Index] = Ranks.ranks[Index];
                State.SkillPoints = Ranks.pointsAvailable;
            }

            if (m_pWorld->HasComponent<GoldComponent>(Entity))
            {
                State.Gold = m_pWorld->GetComponent<GoldComponent>(Entity).amount;
                m_iInGameGold = State.Gold;
            }

            if (m_pWorld->HasComponent<InventoryComponent>(Entity))
            {
                InventoryComponent& Inventory = m_pWorld->GetComponent<InventoryComponent>(Entity);
                State.InventoryItemIds.fill(0);
                m_InGameInventorySlots.fill(0);
                const u32_t Count = std::min<u32_t>(Inventory.count, InventoryComponent::kMaxSlots);
                for (u32_t Index = 0; Index < Count; ++Index)
                {
                    State.InventoryItemIds[Index] = Inventory.itemIds[Index];
                    m_InGameInventorySlots[Index] = Inventory.itemIds[Index];
                }
            }

            if (m_pWorld->HasComponent<StatComponent>(Entity))
            {
                StatComponent& Stat = m_pWorld->GetComponent<StatComponent>(Entity);
                State.Ad = Stat.ad;
                State.Ap = Stat.ap;
                State.Armor = Stat.armor;
                State.Mr = Stat.mr;
                State.AttackSpeed = Stat.attackSpeed;
                State.AttackRange = Stat.attackRange;
                State.MoveSpeed = Stat.moveSpeed;
                State.CritChance = Stat.critChance;
                State.AbilityHaste = Stat.abilityHaste;
                if (Stat.level > 0)
                    State.Level = Stat.level;
                if (Stat.hpMax > 0.f)
                    State.MaxHp = Stat.hpMax;
                if (Stat.manaMax > 0.f)
                    State.MaxMp = Stat.manaMax;
            }

            if (State.bUsesPassiveResource)
            {
                State.Mp = State.PassiveValue;
                State.MaxMp = State.PassiveMax;
            }

            State.bStunned = m_pWorld->HasComponent<StunComponent>(Entity);
            bFoundLocal = true;
        });
}

void CUI_Manager::DrawChampionHUDRHI(const ChampionHUDState& State)
{
    if (!m_pChampionHudPanel)
        return;

    LoadChampionHUDAssetsForChampion(State.Champion);
    m_pChampionHudPanel->SetShowReference(m_bShowChampionHUDReference);
    m_pChampionHudPanel->SetReferenceAlpha(m_fHUDReferenceAlpha);
    m_pChampionHudPanel->DrawRHI(State, m_iWinSizeX, m_iWinSizeY);
}

void CUI_Manager::DrawChampionHUDOverlay(ImDrawList* pDraw, const ChampionHUDState& State)
{
    if (!pDraw)
        return;

    LoadChampionHUDAssetsForChampion(State.Champion);

    const ImVec2 Display = ImGui::GetIO().DisplaySize;
    f32_t HudW = m_fHUDWidth;
    f32_t HudH = m_fHUDHeight;
    if (HudW > Display.x - 24.f)
    {
        const f32_t Scale = (Display.x - 24.f) / HudW;
        HudW *= Scale;
        HudH *= Scale;
    }

    const f32_t ScaleX = HudW / kChampionHUDRefW;
    const f32_t ScaleY = HudH / kChampionHUDRefH;
    const ImVec2 Root((Display.x - HudW) * 0.5f, Display.y - HudH);
    const bool_t bRHIBaseDrawn =
        m_pRHIUIRenderer &&
        m_pRHIUIRenderer->IsReady() &&
        m_iWinSizeX > 0 &&
        m_iWinSizeY > 0;

    if (!bRHIBaseDrawn && m_bShowChampionHUDReference && m_pSRV_ChampionHUDBase)
    {
        const u8_t ReferenceAlpha =
            static_cast<u8_t>(UI_Clamp01(m_fHUDReferenceAlpha) * 255.f);
        pDraw->AddImage(
            reinterpret_cast<ImTextureID>(m_pSRV_ChampionHUDBase),
            Root,
            ImVec2(Root.x + HudW, Root.y + HudH),
            ImVec2(0.f, 0.f),
            ImVec2(1.f, 1.f),
            IM_COL32(255, 255, 255, ReferenceAlpha));
    }

    Draw_HUDStatusFlash(pDraw, Root, HudW, HudH);
    if (m_pChampionHudPanel)
        m_pChampionHudPanel->DrawTextOverlay(State);

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
        for (u32_t RankIndex = 1; RankIndex < SkillRankComponent::kSlotCount; ++RankIndex)
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
            UI_CanLevelSkillByChampionLevel(SkillSlot, Rank, State.Level);

        ImVec2 SkillIconMin = ToPosition(kSkillSlotFallbackCenterX[Index] - 22.f, kSkillSlotCooldownCenterY - 22.f);
        ImVec2 SkillIconMax = ToPosition(kSkillSlotFallbackCenterX[Index] + 22.f, kSkillSlotCooldownCenterY + 22.f);
        ImVec2 SkillCooldownCenter = ToPosition(kSkillSlotFallbackCenterX[Index], kSkillSlotCooldownCenterY);
        f32_t SkillCooldownRadius = kSkillSlotCooldownRadius * std::min(ScaleX, ScaleY);
        if (m_pChampionHudPanel)
        {
            CChampionHudPanel::LayoutRect SkillScreenRect{};
            if (m_pChampionHudPanel->FindElementScreenRect(
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
                m_pSRV_ChampionSkillIcons[Index],
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

    if (m_pSRV_ChampionPassiveIcon)
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
            reinterpret_cast<ImTextureID>(m_pSRV_ChampionPassiveIcon),
            PassiveMin,
            PassiveMax);
    }

    ImFont* pFont = FindUIFont("hud");
    if (pFont)
    {
        static constexpr f32_t kInventorySlotX[6] =
        {
            646.f,
            688.f,
            730.f,
            772.f,
            646.f,
            688.f,
        };
        static constexpr f32_t kInventorySlotY[6] =
        {
            59.f,
            59.f,
            59.f,
            59.f,
            101.f,
            101.f,
        };

        const f32_t FontSize = pFont->LegacySize * ScaleY * 0.68f;
        for (u32_t Index = 0; Index < State.InventoryItemIds.size(); ++Index)
        {
            const u16_t ItemId = State.InventoryItemIds[Index];
            if (ItemId == 0)
                continue;

            const InGameShopItemView* pItemView = FindInGameShopItem(ItemId);
            if (pItemView && pItemView->pSRV)
            {
                const ImVec2 IconMin = ToPosition(kInventorySlotX[Index], kInventorySlotY[Index]);
                const ImVec2 IconMax = ToPosition(
                    kInventorySlotX[Index] + 36.f,
                    kInventorySlotY[Index] + 36.f);
                pDraw->AddImage(
                    reinterpret_cast<ImTextureID>(pItemView->pSRV),
                    IconMin,
                    IconMax,
                    ImVec2(0.f, 0.f),
                    ImVec2(1.f, 1.f),
                    IM_COL32(255, 255, 255, 255));
                continue;
            }

            const std::string Text = std::to_string(static_cast<u32_t>(ItemId));
            const ImVec2 TextSize = pFont->CalcTextSizeA(FontSize, FLT_MAX, 0.f, Text.c_str());
            ImVec2 Position = ToPosition(kInventorySlotX[Index] + 18.f, kInventorySlotY[Index] + 13.f);
            Position.x -= TextSize.x * 0.5f;
            Position.y -= TextSize.y * 0.5f;
            UI_DrawOutlinedText(
                pDraw,
                pFont,
                FontSize,
                Position,
                IM_COL32(245, 231, 177, 255),
                Text.c_str());
        }
    }

    if (!m_bUseLuaUI)
        DrawInGameShop(pDraw);
}

// World → Screen 투영 (Scene_Editor/Scene_InGame RenderDebug 와 동일 로직)
static bool UI_WorldToScreen(const DirectX::XMMATRIX& mVP, const Vec3& w, ImVec2& out,
    uint32_t iWinX, uint32_t iWinY)
{
    DirectX::XMVECTOR v = DirectX::XMVectorSet(w.x, w.y, w.z, 1.f);
    v = DirectX::XMVector4Transform(v, mVP);
    const f32_t wComp = DirectX::XMVectorGetW(v);
    if (wComp <= 0.01f) return false;
    const f32_t nx = DirectX::XMVectorGetX(v) / wComp;
    const f32_t ny = DirectX::XMVectorGetY(v) / wComp;
    out.x = (nx * 0.5f + 0.5f) * static_cast<f32_t>(iWinX);
    out.y = (1.f - (ny * 0.5f + 0.5f)) * static_cast<f32_t>(iWinY);
    return true;
}

// ─────────────────────────────────────────────────────────────
// 팀 조회 — Minion/Champion/Structure 컴포넌트 중 존재하는 것에서 team 필드 추출.
//   없으면 eTeam::TEAM_END 반환 (중립/정글몹 등 — Red 텍스처로 폴백).
//   ForEach<Health, Transform> 3-컴포넌트 한도로 team 을 함께 쿼리할 수 없어
//   Has/GetComponent 호출로 per-entity 확인. N ≤ 107 라 오버헤드 미미.
// ─────────────────────────────────────────────────────────────
void CUI_Manager::Draw_DamageFloaters(ImDrawList* pDraw,
    const DirectX::XMMATRIX& mVP, f32_t fDeltaTime)
{
    if (fDeltaTime < 0.f)
        fDeltaTime = 0.f;
    if (fDeltaTime > 0.1f)
        fDeltaTime = 0.1f;

    for (DamageFloater& floater : m_DamageFloaters)
        floater.fAge += fDeltaTime;

    m_DamageFloaters.erase(
        std::remove_if(m_DamageFloaters.begin(), m_DamageFloaters.end(),
            [](const DamageFloater& floater)
            {
                return floater.fAge >= floater.fLifetime;
            }),
        m_DamageFloaters.end());

    if (!m_bShowDamageFloaters)
        return;

    ImFont* pFont = FindUIFont("hud");
    if (!pFont)
        return;

    for (const DamageFloater& floater : m_DamageFloaters)
    {
        ImVec2 screen{};
        if (!UI_WorldToScreen(mVP, floater.vWorldPos, screen, m_iWinSizeX, m_iWinSizeY))
            continue;

        const f32_t t = UI_Clamp01(floater.fAge / floater.fLifetime);
        const f32_t alpha = 1.f - t;
        screen.x += floater.fXJitter;
        screen.y -= floater.fRisePixels * t;

        char text[32]{};
        std::snprintf(text, sizeof(text), "%.0f", floater.fAmount);

        const f32_t fontSize = floater.bWasCrit ? 26.f : 20.f;
        const ImVec2 textSize = pFont->CalcTextSizeA(fontSize, FLT_MAX, 0.f, text);
        const ImVec2 pos(screen.x - textSize.x * 0.5f, screen.y - textSize.y * 0.5f);
        const ImU32 color = UI_DamageColor(
            floater.iDamageType,
            floater.bWasCrit,
            floater.bKilled,
            alpha);

        UI_DrawOutlinedText(pDraw, pFont, fontSize, pos, color, text);
    }
}

static eTeam UI_Resolve_Team(CWorld* pW, EntityID id)
{
    if (pW->HasComponent<MinionStateComponent>(id))
        return pW->GetComponent<MinionStateComponent>(id).team;
    if (pW->HasComponent<ChampionComponent>(id))
        return pW->GetComponent<ChampionComponent>(id).team;
    if (pW->HasComponent<StructureComponent>(id))
        return pW->GetComponent<StructureComponent>(id).team;
    return eTeam::TEAM_END;
}

static eTeam UI_Resolve_LocalTeam(CWorld* pW)
{
    eTeam localTeam = eTeam::Blue;
    bool_t bFound = false;

    if (!pW)
        return localTeam;

    pW->ForEach<ChampionComponent, LocalPlayerTag>(
        [&](EntityID, ChampionComponent& champion, LocalPlayerTag&)
        {
            if (bFound)
                return;

            localTeam = champion.team;
            bFound = true;
        });

    return localTeam;
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
};

static HealthBarScreenRects BuildHealthBarScreenRects(const ImVec2& center, f32_t width, f32_t height)
{
    HealthBarScreenRects rects{};
    rects.BarMin = ImVec2(center.x - width * 0.5f, center.y - height * 0.5f);
    rects.BarMax = ImVec2(center.x + width * 0.5f, center.y + height * 0.5f);
    rects.FillMin = ImVec2(
        rects.BarMin.x + width * 0.012f,
        rects.BarMin.y + height * 0.08f);
    rects.FillMax = ImVec2(
        rects.BarMax.x - width * 0.012f,
        rects.BarMin.y + height * 0.60f);
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

// ─────────────────────────────────────────────────────────────
// Create — Timer/Sound/Scene Manager 와 동일 팩토리 패턴
// ─────────────────────────────────────────────────────────────
unique_ptr<CUI_Manager> CUI_Manager::Create()
{
    return unique_ptr<CUI_Manager>(new CUI_Manager());
}

// ─────────────────────────────────────────────────────────────
// Draw_HealthBars - champion-attached LoL style bars.
//   Fill: procedural unlit color; do not sample baked HP bar PNGs.
//   Depleted area: dark backing.
//   Barcode: 100 HP ticks, with 1000 HP major ticks.
// ─────────────────────────────────────────────────────────────
void CUI_Manager::Draw_HealthBars(ImDrawList* pDraw, const DirectX::XMMATRIX& mVP)
{
    if (!m_pWorld || !pDraw)
        return;

    const f32_t w = m_fHPBarWidth;
    const f32_t h = m_fHPBarHeight;
    const f32_t yOff = m_fHPBarYOffset;
    const eTeam localTeam = UI_Resolve_LocalTeam(m_pWorld);

    m_pWorld->ForEach<HealthComponent, TransformComponent>(
        [&](EntityID id, HealthComponent& hp, TransformComponent& tf)
        {
            if (!m_pWorld->HasComponent<ChampionComponent>(id))
                return;
            if (hp.bIsDead || hp.fMaximum <= 0.f)
                return;

            const Vec3 p = tf.GetPosition();
            ImVec2 screen;
            if (!UI_WorldToScreen(mVP, { p.x, p.y + yOff, p.z }, screen,
                m_iWinSizeX, m_iWinSizeY))
                return;

            const f32_t clamped = std::clamp(hp.fCurrent / hp.fMaximum, 0.f, 1.f);
            const HealthBarScreenRects rects = BuildHealthBarScreenRects(screen, w, h);

            const eTeam team = UI_Resolve_Team(m_pWorld, id);
            const bool_t bAlly = (team != eTeam::TEAM_END && team == localTeam);

            pDraw->AddRectFilled(rects.BarMin, rects.BarMax, IM_COL32(10, 10, 10, 226));
            const f32_t fillW = rects.FillMax.x - rects.FillMin.x;
            const f32_t trailRatio = ResolveChampionHealthTrailRatio(id, clamped);
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

                pDraw->AddRectFilled(
                    rects.FillMin,
                    fillMax,
                    fillColor);
                pDraw->AddRectFilled(
                    rects.FillMin,
                    ImVec2(fillMax.x, rects.FillMin.y + 1.f),
                    topColor);
            }

            DrawHealthBarcode(pDraw, rects.FillMin, rects.FillMax, hp.fMaximum);
            pDraw->AddRect(rects.BarMin, rects.BarMax, IM_COL32(0, 0, 0, 240), 0.f, 0, 1.25f);
        });
}

// ─────────────────────────────────────────────────────────────
// OnImGui_Tuner — "UI Manager" 패널
// ─────────────────────────────────────────────────────────────
void CUI_Manager::Draw_HealthBars_RHI(const DirectX::XMMATRIX& mVP)
{
    if (!m_pWorld || !m_pRHIUIRenderer)
        return;

    const f32_t w = m_fHPBarWidth;
    const f32_t h = m_fHPBarHeight;
    const f32_t yOff = m_fHPBarYOffset;
    const eTeam localTeam = UI_Resolve_LocalTeam(m_pWorld);
    const Vec4 uvFull(0.f, 0.f, 1.f, 1.f);

    m_pWorld->ForEach<HealthComponent, TransformComponent>(
        [&](EntityID id, HealthComponent& hp, TransformComponent& tf)
        {
            if (!m_pWorld->HasComponent<ChampionComponent>(id))
                return;
            if (hp.bIsDead || hp.fMaximum <= 0.f)
                return;

            const Vec3 p = tf.GetPosition();
            ImVec2 s;
            if (!UI_WorldToScreen(mVP, { p.x, p.y + yOff, p.z }, s,
                m_iWinSizeX, m_iWinSizeY))
                return;

            const f32_t clamped = std::clamp(hp.fCurrent / hp.fMaximum, 0.f, 1.f);
            const HealthBarScreenRects rects = BuildHealthBarScreenRects(s, w, h);

            const eTeam team = UI_Resolve_Team(m_pWorld, id);
            const bool_t bAlly = (team != eTeam::TEAM_END && team == localTeam);

            m_pRHIUIRenderer->DrawImage(
                nullptr,
                rects.BarMin.x,
                rects.BarMin.y,
                rects.BarMax.x - rects.BarMin.x,
                rects.BarMax.y - rects.BarMin.y,
                uvFull,
                Vec4(0.04f, 0.04f, 0.04f, 0.89f));

            const f32_t fillW = rects.FillMax.x - rects.FillMin.x;
            const f32_t trailRatio = ResolveChampionHealthTrailRatio(id, clamped);
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

        });
}

void CUI_Manager::Draw_MinionHealthBars(ImDrawList* pDraw, const DirectX::XMMATRIX& mVP)
{
    if (!m_pWorld || !pDraw)
        return;

    const f32_t w = m_fMinionHPBarWidth;
    const f32_t h = m_fMinionHPBarHeight;
    const f32_t yOff = m_fMinionHPBarYOffset;

    m_pWorld->ForEach<MinionComponent, TransformComponent>(
        [&](EntityID id, MinionComponent& minion, TransformComponent& tf)
        {
            f32_t hpCurrent = minion.hp;
            f32_t hpMaximum = minion.maxHp;
            bool_t bDead = (hpCurrent <= 0.f);
            if (m_pWorld->HasComponent<HealthComponent>(id))
            {
                const HealthComponent& hp = m_pWorld->GetComponent<HealthComponent>(id);
                hpCurrent = hp.fCurrent;
                if (hp.fMaximum > 0.f)
                    hpMaximum = hp.fMaximum;
                bDead = hp.bIsDead || hpCurrent <= 0.f;
            }
            if (bDead || hpMaximum <= 0.f)
                return;

            const Vec3 p = tf.GetPosition();
            ImVec2 screen;
            if (!UI_WorldToScreen(mVP, { p.x, p.y + yOff, p.z }, screen,
                m_iWinSizeX, m_iWinSizeY))
                return;

            const f32_t clamped = std::clamp(hpCurrent / hpMaximum, 0.f, 1.f);
            const ImVec2 barMin(screen.x - w * 0.5f, screen.y - h * 0.5f);
            const ImVec2 barMax(screen.x + w * 0.5f, screen.y + h * 0.5f);
            const f32_t fillW = w * clamped;
            const ImVec2 fillMax(barMin.x + fillW, barMax.y);

            const bool_t bBlueTeam = minion.team == eTeam::Blue;

            pDraw->AddRectFilled(barMin, barMax, IM_COL32(14, 14, 14, 232));
            if (fillW > 0.5f)
            {
                pDraw->AddRectFilled(
                    barMin,
                    fillMax,
                    bBlueTeam ? IM_COL32(48, 134, 230, 255) : IM_COL32(224, 56, 50, 255));
                pDraw->AddRectFilled(
                    barMin,
                    ImVec2(fillMax.x, barMin.y + 1.f),
                    bBlueTeam ? IM_COL32(118, 204, 255, 84) : IM_COL32(255, 128, 96, 84));
            }
        });
}

void CUI_Manager::Draw_MinionHealthBars_RHI(const DirectX::XMMATRIX& mVP)
{
    if (!m_pWorld || !m_pRHIUIRenderer)
        return;

    const f32_t w = m_fMinionHPBarWidth;
    const f32_t h = m_fMinionHPBarHeight;
    const f32_t yOff = m_fMinionHPBarYOffset;
    const Vec4 uvFull(0.f, 0.f, 1.f, 1.f);

    m_pWorld->ForEach<MinionComponent, TransformComponent>(
        [&](EntityID id, MinionComponent& minion, TransformComponent& tf)
        {
            f32_t hpCurrent = minion.hp;
            f32_t hpMaximum = minion.maxHp;
            bool_t bDead = (hpCurrent <= 0.f);
            if (m_pWorld->HasComponent<HealthComponent>(id))
            {
                const HealthComponent& hp = m_pWorld->GetComponent<HealthComponent>(id);
                hpCurrent = hp.fCurrent;
                if (hp.fMaximum > 0.f)
                    hpMaximum = hp.fMaximum;
                bDead = hp.bIsDead || hpCurrent <= 0.f;
            }
            if (bDead || hpMaximum <= 0.f)
                return;

            const Vec3 p = tf.GetPosition();
            ImVec2 screen;
            if (!UI_WorldToScreen(mVP, { p.x, p.y + yOff, p.z }, screen,
                m_iWinSizeX, m_iWinSizeY))
                return;

            const f32_t clamped = std::clamp(hpCurrent / hpMaximum, 0.f, 1.f);
            const f32_t x = screen.x - w * 0.5f;
            const f32_t y = screen.y - h * 0.5f;
            const f32_t fillW = w * clamped;

            const bool_t bBlueTeam = minion.team == eTeam::Blue;

            m_pRHIUIRenderer->DrawImage(
                nullptr,
                x,
                y,
                w,
                h,
                uvFull,
                Vec4(0.055f, 0.055f, 0.055f, 0.91f));

            if (fillW <= 0.5f)
                return;

            m_pRHIUIRenderer->DrawImage(
                nullptr,
                x,
                y,
                fillW,
                h,
                uvFull,
                bBlueTeam ? Vec4(0.19f, 0.53f, 0.91f, 1.0f) : Vec4(0.88f, 0.22f, 0.20f, 1.0f));
            m_pRHIUIRenderer->DrawImage(
                nullptr,
                x,
                y,
                fillW,
                1.0f,
                uvFull,
                bBlueTeam ? Vec4(0.46f, 0.80f, 1.0f, 0.33f) : Vec4(1.0f, 0.50f, 0.38f, 0.33f));
        });
}

void CUI_Manager::Draw_TurretHealthBars(ImDrawList* pDraw, const DirectX::XMMATRIX& mVP)
{
    if (!m_pWorld || !pDraw)
        return;

    const f32_t w = m_fTurretHPBarWidth;
    const f32_t h = m_fTurretHPBarHeight;
    const f32_t yOff = m_fTurretHPBarYOffset;

    m_pWorld->ForEach<TurretComponent, TransformComponent>(
        [&](EntityID id, TurretComponent& turret, TransformComponent& tf)
        {
            f32_t hpCurrent = turret.hp;
            f32_t hpMaximum = turret.maxHp;
            bool_t bDead = (hpCurrent <= 0.f);
            if (m_pWorld->HasComponent<HealthComponent>(id))
            {
                const HealthComponent& hp = m_pWorld->GetComponent<HealthComponent>(id);
                hpCurrent = hp.fCurrent;
                if (hp.fMaximum > 0.f)
                    hpMaximum = hp.fMaximum;
                bDead = hp.bIsDead || hpCurrent <= 0.f;
            }
            if (bDead || hpMaximum <= 0.f)
                return;

            const Vec3 p = tf.GetPosition();
            ImVec2 screen;
            if (!UI_WorldToScreen(mVP, { p.x, p.y + yOff, p.z }, screen,
                m_iWinSizeX, m_iWinSizeY))
                return;

            screen.x += m_fTurretHPBarScreenOffsetX;
            screen.y += m_fTurretHPBarScreenOffsetY;

            const f32_t clamped = std::clamp(hpCurrent / hpMaximum, 0.f, 1.f);
            const ImVec2 barMin(screen.x - w * 0.5f, screen.y - h * 0.5f);
            const ImVec2 barMax(screen.x + w * 0.5f, screen.y + h * 0.5f);
            const f32_t fillW = w * clamped;

            const bool_t bBlueTeam = turret.team == eTeam::Blue;
            void* pSRV = bBlueTeam ? m_pSRV_TurretBlueHPBar : m_pSRV_TurretRedHPBar;

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
                        bBlueTeam ? IM_COL32(58, 144, 232, 255) : IM_COL32(225, 64, 56, 255));
                }
            }

            pDraw->AddRect(barMin, barMax, IM_COL32(0, 0, 0, 238), 0.f, 0, 1.25f);
        });
}

void CUI_Manager::Draw_TurretHealthBars_RHI(const DirectX::XMMATRIX& mVP)
{
    if (!m_pWorld || !m_pRHIUIRenderer)
        return;

    const f32_t w = m_fTurretHPBarWidth;
    const f32_t h = m_fTurretHPBarHeight;
    const f32_t yOff = m_fTurretHPBarYOffset;
    const Vec4 uvFull(0.f, 0.f, 1.f, 1.f);

    m_pWorld->ForEach<TurretComponent, TransformComponent>(
        [&](EntityID id, TurretComponent& turret, TransformComponent& tf)
        {
            f32_t hpCurrent = turret.hp;
            f32_t hpMaximum = turret.maxHp;
            bool_t bDead = (hpCurrent <= 0.f);
            if (m_pWorld->HasComponent<HealthComponent>(id))
            {
                const HealthComponent& hp = m_pWorld->GetComponent<HealthComponent>(id);
                hpCurrent = hp.fCurrent;
                if (hp.fMaximum > 0.f)
                    hpMaximum = hp.fMaximum;
                bDead = hp.bIsDead || hpCurrent <= 0.f;
            }
            if (bDead || hpMaximum <= 0.f)
                return;

            const Vec3 p = tf.GetPosition();
            ImVec2 screen;
            if (!UI_WorldToScreen(mVP, { p.x, p.y + yOff, p.z }, screen,
                m_iWinSizeX, m_iWinSizeY))
                return;

            screen.x += m_fTurretHPBarScreenOffsetX;
            screen.y += m_fTurretHPBarScreenOffsetY;

            const f32_t clamped = std::clamp(hpCurrent / hpMaximum, 0.f, 1.f);
            const f32_t x = screen.x - w * 0.5f;
            const f32_t y = screen.y - h * 0.5f;
            const f32_t fillW = w * clamped;

            const bool_t bBlueTeam = turret.team == eTeam::Blue;
            void* pSRV = bBlueTeam ? m_pSRV_TurretBlueHPBar : m_pSRV_TurretRedHPBar;

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
                return;

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
                    bBlueTeam ? Vec4(0.23f, 0.57f, 0.91f, 1.f) : Vec4(0.88f, 0.25f, 0.22f, 1.f));
            }
        });
}

void CUI_Manager::DrawHealthBarBarcodeOverlay(ImDrawList* pDraw, const DirectX::XMMATRIX& mVP)
{
    if (!m_pWorld || !pDraw)
        return;

    const f32_t w = m_fHPBarWidth;
    const f32_t h = m_fHPBarHeight;
    const f32_t yOff = m_fHPBarYOffset;

    m_pWorld->ForEach<HealthComponent, TransformComponent>(
        [&](EntityID id, HealthComponent& hp, TransformComponent& tf)
        {
            if (!m_pWorld->HasComponent<ChampionComponent>(id))
                return;
            if (hp.bIsDead || hp.fMaximum <= 0.f)
                return;

            const Vec3 p = tf.GetPosition();
            ImVec2 screen;
            if (!UI_WorldToScreen(mVP, { p.x, p.y + yOff, p.z }, screen,
                m_iWinSizeX, m_iWinSizeY))
                return;

            const HealthBarScreenRects rects = BuildHealthBarScreenRects(screen, w, h);
            DrawHealthBarcode(pDraw, rects.FillMin, rects.FillMax, hp.fMaximum);
            pDraw->AddRect(rects.BarMin, rects.BarMax, IM_COL32(0, 0, 0, 240), 0.f, 0, 1.25f);
        });
}

void CUI_Manager::OnImGui_Tuner()
{
    if (!ImGui::Begin("UI Manager")) { ImGui::End(); return; }

    bool b = (m_bShowHealthBars != 0);
    if (ImGui::Checkbox("HP Bars", &b)) m_bShowHealthBars = b;

    ImGui::Text("HP Green: %s", m_pSRV_HPBarGreen ? "loaded" : "FALLBACK");
    ImGui::Text("HP Red: %s", m_pSRV_HPBarRed ? "loaded" : "FALLBACK");
    ImGui::TextDisabled("World HP bars: champion only, ally green / enemy red");
    ImGui::SliderFloat("Bar Width (px)",   &m_fHPBarWidth,   20.f, 200.f);
    ImGui::SliderFloat("Bar Height (px)",  &m_fHPBarHeight,   3.f,  32.f);
    ImGui::SliderFloat("Y Offset (m)",     &m_fHPBarYOffset, 0.5f,  6.f);
    ImGui::Text("Minion Blue HP: %s", m_pSRV_MinionBlueHPBar ? "loaded" : "FALLBACK");
    ImGui::Text("Minion Red HP: %s", m_pSRV_MinionRedHPBar ? "loaded" : "FALLBACK");
    ImGui::TextDisabled("Minion HP bars: team blue/red texture, dark depleted backing");
    ImGui::SliderFloat("Minion Bar Width (px)", &m_fMinionHPBarWidth, 20.f, 100.f);
    ImGui::SliderFloat("Minion Bar Height (px)", &m_fMinionHPBarHeight, 3.f, 16.f);
    ImGui::SliderFloat("Minion Y Offset (m)", &m_fMinionHPBarYOffset, 0.5f, 3.f);
    ImGui::Text("Turret Blue HP: %s", m_pSRV_TurretBlueHPBar ? "loaded" : "FALLBACK");
    ImGui::Text("Turret Red HP: %s", m_pSRV_TurretRedHPBar ? "loaded" : "FALLBACK");
    ImGui::TextDisabled("Turret HP bars: team blue/red PNG, dark depleted backing");
    ImGui::SliderFloat("Turret Bar Width (px)", &m_fTurretHPBarWidth, 50.f, 240.f);
    ImGui::SliderFloat("Turret Bar Height (px)", &m_fTurretHPBarHeight, 6.f, 40.f);
    ImGui::SliderFloat("Turret Y Offset (m)", &m_fTurretHPBarYOffset, 1.f, 8.f);
    ImGui::SliderFloat("Turret Screen X (px)", &m_fTurretHPBarScreenOffsetX, -120.f, 120.f);
    ImGui::SliderFloat("Turret Screen Y (px)", &m_fTurretHPBarScreenOffsetY, -120.f, 120.f);

    ImGui::Separator();
    ImGui::Checkbox("Show Mouse Cursor", &m_bShowMouseCursor);
    ImGui::SliderFloat("Cursor Size", &m_fCursorSize, 16.f, 64.f, "%.0f");
    ImGui::Checkbox("Use Lua UI", &m_bUseLuaUI);
    if (m_pLuaUIHost)
        m_pLuaUIHost->DrawTunerImGui();
    ImGui::Text("Shop Reference: %s", m_pSRV_InGameShopReference ? "loaded" : "FALLBACK");
    ImGui::SliderFloat("Shop Reference Alpha", &m_fInGameShopReferenceAlpha, 0.10f, 1.00f, "%.2f");
    ImGui::Checkbox("Show Champion HUD", &m_bShowChampionHUD);
    ImGui::Text("HUD Layout: JSON 861x167 bottom-center");
    ImGui::Text("HUD Atlas: %s", m_HudAtlasManifest.FindTexture("hud") ? "loaded" : "FALLBACK");
    ImGui::Text("Ability Atlas: %s", m_pSRV_AbilityAtlas ? "loaded" : "FALLBACK");
    ImGui::Text("Champion Portrait: %s", m_pSRV_ChampionPortrait ? "loaded" : "FALLBACK");
    ImGui::Text("Hit Flash: %s", m_pSRV_HUDHit ? "loaded" : "FALLBACK");
    ImGui::Text("Stun Flash: %s", m_pSRV_HUDStun ? "loaded" : "FALLBACK");
    ImGui::Checkbox("HUD Reference", &m_bShowChampionHUDReference);
    ImGui::SliderFloat("HUD Reference Alpha", &m_fHUDReferenceAlpha, 0.0f, 1.0f, "%.2f");
    ImGui::Checkbox("HUD Hit/Stun Flash", &m_bShowHUDStatusFlash);
    ImGui::SliderFloat("Hit Flash Sec", &m_fHUDHitFlashDuration, 0.1f, 2.0f, "%.2f");
    ImGui::SliderFloat("Stun Flash Sec", &m_fHUDStunFlashDuration, 0.1f, 2.0f, "%.2f");
    if (m_pChampionHudPanel)
        m_pChampionHudPanel->DrawLayoutTunerImGui();
    if (ImGui::Button("Test Hit Flash"))
        m_fHUDHitFlashTimer = m_fHUDHitFlashDuration;
    ImGui::SameLine();
    if (ImGui::Button("Test Stun Flash"))
        m_fHUDStunFlashTimer = m_fHUDStunFlashDuration;

    ImGui::Separator();
    ImGui::Checkbox("Show Damage Floaters", &m_bShowDamageFloaters);
    ImGui::SliderFloat("Damage Life", &m_fDamageFloaterLife, 0.35f, 2.0f, "%.2f");
    ImGui::SliderFloat("Damage Rise Pixels", &m_fDamageFloaterRise, 12.f, 120.f, "%.0f");
    ImGui::Text("Damage Floaters: %u", static_cast<u32_t>(m_DamageFloaters.size()));
    if (ImGui::Button("Test Damage 123"))
    {
        bool_t bPushed = false;
        if (m_pWorld)
        {
            m_pWorld->ForEach<ChampionComponent, LocalPlayerTag, TransformComponent>(
                [&](EntityID, ChampionComponent&, LocalPlayerTag&, TransformComponent& tf)
                {
                    if (bPushed)
                        return;

                    Vec3 pos = tf.GetPosition();
                    pos.y += 2.1f;
                    Push_DamageNumber(pos, 123.f, 0u, false, false);
                    bPushed = true;
                });
        }

        if (!bPushed)
            Push_DamageNumber({ 0.f, 2.1f, 0.f }, 123.f, 0u, false, false);
    }

    ImGui::TextDisabled("Phase B+ 확장: PlayerHUD / Scoreboard");

    ImGui::End();
}

NS_END
