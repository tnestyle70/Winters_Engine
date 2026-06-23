#include "Server/Private/Game/Factory/ChampionSimComponentTable.h"

#include "ECS/Components/GameplayComponents.h"
#include "ECS/World.h"
#include "Shared/GameSim/Components/AnnieSimComponent.h"
#include "Shared/GameSim/Components/AsheSimComponent.h"
#include "Shared/GameSim/Components/FioraSimComponent.h"
#include "Shared/GameSim/Components/JaxSimComponent.h"
#include "Shared/GameSim/Components/KindredSimComponent.h"
#include "Shared/GameSim/Components/LeeSinSimComponent.h"
#include "Shared/GameSim/Components/MasterYiComponent.h"
#include "Shared/GameSim/Components/ViegoSimComponent.h"
#include "Shared/GameSim/Components/YoneSimComponent.h"

namespace
{
    struct ChampionSimEntry
    {
        eChampion champion = eChampion::NONE;
        void (*install)(CWorld&, EntityID) = nullptr;
    };

    template <typename T>
    void InstallSim(CWorld& world, EntityID entity)
    {
        world.AddComponent<T>(entity, T{});
    }

    constexpr ChampionSimEntry kChampionSimTable[] =
    {
        { eChampion::YASUO, &InstallSim<YasuoStateComponent> },
        { eChampion::ASHE, &InstallSim<AsheSimComponent> },
        { eChampion::ANNIE, &InstallSim<AnnieSimComponent> },
        { eChampion::FIORA, &InstallSim<FioraSimComponent> },
        { eChampion::JAX, &InstallSim<JaxSimComponent> },
        { eChampion::VIEGO, &InstallSim<ViegoSimComponent> },
        { eChampion::YONE, &InstallSim<YoneSimComponent> },
        { eChampion::LEESIN, &InstallSim<LeeSinSimComponent> },
        { eChampion::KINDRED, &InstallSim<KindredSimComponent> },
        { eChampion::MASTERYI, &InstallSim<MasterYiSimComponent> },
    };
}

void AttachChampionSimComponents(CWorld& world, EntityID entity, eChampion champion)
{
    for (const ChampionSimEntry& entry : kChampionSimTable)
    {
        if (entry.champion == champion)
        {
            entry.install(world, entity);
            return;
        }
    }
}
