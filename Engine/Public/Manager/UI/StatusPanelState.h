#pragma once

#include "ECS/Entity.h"
#include "WintersTypes.h"

#include <array>

namespace Engine
{
    struct StatusPanelMatchScore
    {
        u16_t iBlueDragons = 0;
        u16_t iBlueBarons = 0;
        u16_t iBlueDestroyedStructures = 0;
        u16_t iBlueDestroyedObjectives = 0;
        u16_t iRedDragons = 0;
        u16_t iRedBarons = 0;
        u16_t iRedDestroyedStructures = 0;
        u16_t iRedDestroyedObjectives = 0;
    };

    struct StatusPanelActorRow
    {
        EntityID Entity = NULL_ENTITY;
        u8_t iActorContentId = 255u;
        u8_t iTeam = 0;
        u8_t iLevel = 1;
        u16_t iKills = 0;
        u16_t iDeaths = 0;
        u16_t iAssists = 0;
        std::array<u16_t, 2> SummonerSpellIds{};
        std::array<f32_t, 2> SummonerCooldowns{};
        std::array<f32_t, 2> SummonerCooldownDurations{};
        std::array<u16_t, 6> InventoryItemIds{};
    };
}
