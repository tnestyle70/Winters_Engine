#include "Game/ServerAICommandProducer.h"

#include "Game/LobbyAuthority.h"
#include "Shared/GameSim/Definitions/MapDataFormats.h"
#include "Shared/GameSim/Systems/ChampionAI/ChampionAISystem.h"

namespace
{
    constexpr u8_t kChampionAILaneTop =
        static_cast<u8_t>(Winters::Map::eLane::Top);
    constexpr u8_t kChampionAILaneMid =
        static_cast<u8_t>(Winters::Map::eLane::Mid);
    constexpr u8_t kChampionAILaneBot =
        static_cast<u8_t>(Winters::Map::eLane::Bot);

    bool_t IsValidChampionAILane(u8_t lane)
    {
        return lane == kChampionAILaneTop ||
            lane == kChampionAILaneMid ||
            lane == kChampionAILaneBot;
    }
}

void CServerAICommandProducer::Execute(
    CWorld& world,
    const TickContext& tc,
    std::vector<GameCommand>& outCommands)
{
    CChampionAISystem::Execute(world, tc, outCommands);
}

u8_t CServerAICommandProducer::ResolveInitialBotLane(
    const LobbySlotState& slot,
    u8_t rosterFallbackLane)
{
    if (!slot.bBot || slot.bDummy)
        return rosterFallbackLane;

    if (IsValidChampionAILane(slot.botLane))
        return slot.botLane;

    static constexpr u8_t kBotLanes[] =
    {
        kChampionAILaneTop,
        kChampionAILaneMid,
        kChampionAILaneBot,
    };

    const u32_t seed =
        static_cast<u32_t>(slot.slotId) * 1103515245u ^
        static_cast<u32_t>(slot.team) * 2654435761u ^
        static_cast<u32_t>(slot.botDifficulty) * 2246822519u ^
        static_cast<u32_t>(slot.champion) * 3266489917u;

    return kBotLanes[seed % static_cast<u32_t>(sizeof(kBotLanes) / sizeof(kBotLanes[0]))];
}
