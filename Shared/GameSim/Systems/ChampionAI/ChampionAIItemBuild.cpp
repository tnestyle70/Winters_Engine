#include "Shared/GameSim/Systems/ChampionAI/ChampionAIItemBuild.h"

#include <iterator>

namespace
{
    // Doran -> 신발 -> 코어 순. 인벤토리 6칸 한도 내.
    constexpr u16_t kAdCasterBuild[] = { 1055u, 1001u, 1036u, 1037u, 1038u, 3072u };
    constexpr u16_t kAttackSpeedBuild[] = { 1055u, 3006u, 1042u, 1043u, 1038u, 3153u };
    constexpr u16_t kApBuild[] = { 1056u, 3020u, 1052u, 1058u, 3089u, 1028u };
    constexpr u16_t kBruiserBuild[] = { 1054u, 3047u, 1036u, 1011u, 3078u, 3065u };
}

ChampionAIItemBuildOrder GetChampionAIItemBuildOrder(eChampion champion)
{
    switch (champion)
    {
    case eChampion::ANNIE:
    case eChampion::SYLAS:
        return { kApBuild, static_cast<u32_t>(std::size(kApBuild)) };
    case eChampion::ASHE:
    case eChampion::KALISTA:
    case eChampion::KINDRED:
    case eChampion::YASUO:
    case eChampion::YONE:
    case eChampion::MASTERYI:
        return { kAttackSpeedBuild, static_cast<u32_t>(std::size(kAttackSpeedBuild)) };
    case eChampion::GAREN:
    case eChampion::JAX:
    case eChampion::IRELIA:
    case eChampion::VIEGO:
        return { kBruiserBuild, static_cast<u32_t>(std::size(kBruiserBuild)) };
    default:
        return { kAdCasterBuild, static_cast<u32_t>(std::size(kAdCasterBuild)) };
    }
}
