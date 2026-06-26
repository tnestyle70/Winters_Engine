#pragma once

#include "Shared/GameSim/Definitions/LoLMatchContext.h"
#include "GameObject/VisualEventData.h"
#include "WintersTypes.h"

#include <cstdint>

inline constexpr uint8_t kChampionVisualTextureSlotMax = 8;
inline constexpr uint8_t kChampionVisualPoseMax = 8;
inline constexpr uint8_t kChampionVisualActionMax = 16;
inline constexpr uint8_t kChampionVisualActionStageMax = 2;
inline constexpr uint8_t kChampionVisualActionEventMax = 4;

struct ChampionModelVisualData
{
    const char* displayName = nullptr;
    const char* fbxPath = nullptr;
    const wchar_t* shaderPath = L"Shaders/Mesh3D.hlsl";
    const wchar_t* defaultTexturePath = nullptr;
    const wchar_t* texturePath[kChampionVisualTextureSlotMax] = {};
    f32_t modelYawOffset = 0.f;
    f32_t modelScale = 0.01f;
};

struct ChampionPoseVisualData
{
    bool_t bValid = false;
    u16_t poseId = 0;
    const char* animationKey = nullptr;
    f32_t playbackSpeed = 1.f;
    bool_t bLoop = true;
};

struct ChampionActionVisualStageData
{
    u8_t stage = 1;
    const char* animationKey = nullptr;
    f32_t playbackSpeed = 1.f;
    bool_t bLoop = false;
    u8_t eventCount = 0;
    VisualEventData events[kChampionVisualActionEventMax] = {};
};

struct ChampionActionVisualData
{
    bool_t bValid = false;
    u16_t actionId = 0;
    u8_t stageCount = 1;
    ChampionActionVisualStageData stages[kChampionVisualActionStageMax] = {};
};

struct ChampionVisualData
{
    bool_t bValid = false;
    eChampion champion = eChampion::END;
    ChampionModelVisualData model{};
    ChampionPoseVisualData poses[kChampionVisualPoseMax] = {};
    ChampionActionVisualData actions[kChampionVisualActionMax] = {};
};
