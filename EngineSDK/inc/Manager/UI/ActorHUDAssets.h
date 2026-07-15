#pragma once

#include "WintersTypes.h"

#include <array>

namespace Engine
{
    struct ActorHUDAssetDesc
    {
        u8_t iContentId = 255u;
        const wchar_t* pPortraitPath = nullptr;
        const wchar_t* pPassiveIconPath = nullptr;
        std::array<const wchar_t*, 4> SkillIconPaths{};
        const wchar_t* pPassiveBarPath = nullptr;
        bool_t bUsesPassiveResource = false;
    };

    struct UIIconAssetDesc
    {
        u16_t iContentId = 0u;
        const wchar_t* pIconPath = nullptr;
    };

    struct UIShopItemAssetDesc
    {
        u16_t iItemId = 0u;
        u16_t iPrice = 0u;
        u32_t iOrder = 0u;
        const char* pAssetKey = nullptr;
        const char* pSection = nullptr;
        const char* pDisplayName = nullptr;
        const wchar_t* pIconPath = nullptr;
        const char* pIconSprite = nullptr;
        const char* const* pStatLines = nullptr;
        u32_t iStatLineCount = 0u;
        bool_t bEnabled = true;
        bool_t bPurchasable = false;
    };
}
