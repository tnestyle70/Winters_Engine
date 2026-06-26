#pragma once

#include "LoLMatchContext.h"
#include "WintersTypes.h"

inline constexpr u32_t kSkinTextureSlotMax = 8;

struct SkinDef
{
    u32_t skinId = 0;
    eChampion championId = eChampion::NONE;
    const char* fbxPathOverride = nullptr;
    const wchar_t* texturePathOverride[kSkinTextureSlotMax] = {};
    const char* animPrefixOverride = nullptr;
    u32_t fxOverrideHookId = 0;
    const char* displayName = nullptr;
};
