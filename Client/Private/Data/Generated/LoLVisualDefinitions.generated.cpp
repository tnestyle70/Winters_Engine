#include "Client/Private/Data/LoLVisualDefinitionPack.h"

namespace
{
    ClientData::ChampionVisualDefinition MakeChampionVisual_IRELIA()
    {
        ClientData::ChampionVisualDefinition def{};
        def.key = 0xEB28CFB8u;
        def.legacyChampion = eChampion::IRELIA;
        def.modelYawOffsetRadians = 3.14159265f;
        def.skills[0].stageCount = 1u;
        def.skills[0].replicatedCueId = 0u;
        def.skills[0].stages[0].animationPlaybackSpeed = 1.25f;
        def.skills[0].stages[0].castFrame = 0.f;
        def.skills[0].stages[0].recoveryFrame = 0.f;
        def.skills[0].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[0].stages[1].castFrame = 0.f;
        def.skills[0].stages[1].recoveryFrame = 0.f;
        def.skills[1].stageCount = 1u;
        def.skills[1].replicatedCueId = 0u;
        def.skills[1].stages[0].animationPlaybackSpeed = 1.2f;
        def.skills[1].stages[0].castFrame = 0.f;
        def.skills[1].stages[0].recoveryFrame = 0.f;
        def.skills[1].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[1].castFrame = 0.f;
        def.skills[1].stages[1].recoveryFrame = 0.f;
        def.skills[2].stageCount = 2u;
        def.skills[2].replicatedCueId = 0u;
        def.skills[2].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[0].castFrame = 0.f;
        def.skills[2].stages[0].recoveryFrame = 0.f;
        def.skills[2].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[1].castFrame = 0.f;
        def.skills[2].stages[1].recoveryFrame = 0.f;
        def.skills[3].stageCount = 2u;
        def.skills[3].replicatedCueId = 0u;
        def.skills[3].stages[0].animationPlaybackSpeed = 1.05f;
        def.skills[3].stages[0].castFrame = 0.f;
        def.skills[3].stages[0].recoveryFrame = 0.f;
        def.skills[3].stages[1].animationPlaybackSpeed = 1.05f;
        def.skills[3].stages[1].castFrame = 0.f;
        def.skills[3].stages[1].recoveryFrame = 0.f;
        def.skills[4].stageCount = 1u;
        def.skills[4].replicatedCueId = 0u;
        def.skills[4].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[0].castFrame = 0.f;
        def.skills[4].stages[0].recoveryFrame = 0.f;
        def.skills[4].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[1].castFrame = 0.f;
        def.skills[4].stages[1].recoveryFrame = 0.f;
        return def;
    }

    ClientData::ChampionVisualDefinition MakeChampionVisual_YASUO()
    {
        ClientData::ChampionVisualDefinition def{};
        def.key = 0xBA78D203u;
        def.legacyChampion = eChampion::YASUO;
        def.modelYawOffsetRadians = 3.14159265f;
        def.skills[0].stageCount = 1u;
        def.skills[0].replicatedCueId = 0u;
        def.skills[0].stages[0].animationPlaybackSpeed = 0.85f;
        def.skills[0].stages[0].castFrame = 0.f;
        def.skills[0].stages[0].recoveryFrame = 0.f;
        def.skills[0].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[0].stages[1].castFrame = 0.f;
        def.skills[0].stages[1].recoveryFrame = 0.f;
        def.skills[1].stageCount = 1u;
        def.skills[1].replicatedCueId = 0u;
        def.skills[1].stages[0].animationPlaybackSpeed = 0.85f;
        def.skills[1].stages[0].castFrame = 0.f;
        def.skills[1].stages[0].recoveryFrame = 0.f;
        def.skills[1].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[1].castFrame = 0.f;
        def.skills[1].stages[1].recoveryFrame = 0.f;
        def.skills[2].stageCount = 1u;
        def.skills[2].replicatedCueId = 0u;
        def.skills[2].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[0].castFrame = 0.f;
        def.skills[2].stages[0].recoveryFrame = 0.f;
        def.skills[2].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[1].castFrame = 0.f;
        def.skills[2].stages[1].recoveryFrame = 0.f;
        def.skills[3].stageCount = 1u;
        def.skills[3].replicatedCueId = 0u;
        def.skills[3].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[0].castFrame = 0.f;
        def.skills[3].stages[0].recoveryFrame = 0.f;
        def.skills[3].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[1].castFrame = 0.f;
        def.skills[3].stages[1].recoveryFrame = 0.f;
        def.skills[4].stageCount = 1u;
        def.skills[4].replicatedCueId = 0u;
        def.skills[4].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[0].castFrame = 0.f;
        def.skills[4].stages[0].recoveryFrame = 0.f;
        def.skills[4].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[1].castFrame = 0.f;
        def.skills[4].stages[1].recoveryFrame = 0.f;
        return def;
    }

    ClientData::ChampionVisualDefinition MakeChampionVisual_KALISTA()
    {
        ClientData::ChampionVisualDefinition def{};
        def.key = 0x20024CB3u;
        def.legacyChampion = eChampion::KALISTA;
        def.modelYawOffsetRadians = 3.14159265f;
        def.skills[0].stageCount = 1u;
        def.skills[0].replicatedCueId = 0u;
        def.skills[0].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[0].stages[0].castFrame = 0.f;
        def.skills[0].stages[0].recoveryFrame = 0.f;
        def.skills[0].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[0].stages[1].castFrame = 0.f;
        def.skills[0].stages[1].recoveryFrame = 0.f;
        def.skills[1].stageCount = 1u;
        def.skills[1].replicatedCueId = 0u;
        def.skills[1].stages[0].animationPlaybackSpeed = 2.8f;
        def.skills[1].stages[0].castFrame = 0.f;
        def.skills[1].stages[0].recoveryFrame = 0.f;
        def.skills[1].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[1].castFrame = 0.f;
        def.skills[1].stages[1].recoveryFrame = 0.f;
        def.skills[2].stageCount = 1u;
        def.skills[2].replicatedCueId = 0u;
        def.skills[2].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[0].castFrame = 0.f;
        def.skills[2].stages[0].recoveryFrame = 0.f;
        def.skills[2].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[1].castFrame = 0.f;
        def.skills[2].stages[1].recoveryFrame = 0.f;
        def.skills[3].stageCount = 1u;
        def.skills[3].replicatedCueId = 0u;
        def.skills[3].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[0].castFrame = 0.f;
        def.skills[3].stages[0].recoveryFrame = 0.f;
        def.skills[3].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[1].castFrame = 0.f;
        def.skills[3].stages[1].recoveryFrame = 0.f;
        def.skills[4].stageCount = 1u;
        def.skills[4].replicatedCueId = 0u;
        def.skills[4].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[0].castFrame = 0.f;
        def.skills[4].stages[0].recoveryFrame = 0.f;
        def.skills[4].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[1].castFrame = 0.f;
        def.skills[4].stages[1].recoveryFrame = 0.f;
        return def;
    }

    ClientData::ChampionVisualDefinition MakeChampionVisual_GAREN()
    {
        ClientData::ChampionVisualDefinition def{};
        def.key = 0x08F519B1u;
        def.legacyChampion = eChampion::GAREN;
        def.modelYawOffsetRadians = 3.14159265f;
        def.skills[0].stageCount = 1u;
        def.skills[0].replicatedCueId = 0u;
        def.skills[0].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[0].stages[0].castFrame = 0.f;
        def.skills[0].stages[0].recoveryFrame = 0.f;
        def.skills[0].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[0].stages[1].castFrame = 0.f;
        def.skills[0].stages[1].recoveryFrame = 0.f;
        def.skills[1].stageCount = 1u;
        def.skills[1].replicatedCueId = 0u;
        def.skills[1].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[0].castFrame = 0.f;
        def.skills[1].stages[0].recoveryFrame = 0.f;
        def.skills[1].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[1].castFrame = 0.f;
        def.skills[1].stages[1].recoveryFrame = 0.f;
        def.skills[2].stageCount = 1u;
        def.skills[2].replicatedCueId = 0u;
        def.skills[2].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[0].castFrame = 0.f;
        def.skills[2].stages[0].recoveryFrame = 0.f;
        def.skills[2].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[1].castFrame = 0.f;
        def.skills[2].stages[1].recoveryFrame = 0.f;
        def.skills[3].stageCount = 1u;
        def.skills[3].replicatedCueId = 0u;
        def.skills[3].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[0].castFrame = 0.f;
        def.skills[3].stages[0].recoveryFrame = 0.f;
        def.skills[3].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[1].castFrame = 0.f;
        def.skills[3].stages[1].recoveryFrame = 0.f;
        def.skills[4].stageCount = 1u;
        def.skills[4].replicatedCueId = 0u;
        def.skills[4].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[0].castFrame = 0.f;
        def.skills[4].stages[0].recoveryFrame = 0.f;
        def.skills[4].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[1].castFrame = 0.f;
        def.skills[4].stages[1].recoveryFrame = 0.f;
        return def;
    }

    ClientData::ChampionVisualDefinition MakeChampionVisual_ZED()
    {
        ClientData::ChampionVisualDefinition def{};
        def.key = 0x556E51C7u;
        def.legacyChampion = eChampion::ZED;
        def.modelYawOffsetRadians = 3.14159265f;
        def.skills[0].stageCount = 1u;
        def.skills[0].replicatedCueId = 0u;
        def.skills[0].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[0].stages[0].castFrame = 0.f;
        def.skills[0].stages[0].recoveryFrame = 0.f;
        def.skills[0].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[0].stages[1].castFrame = 0.f;
        def.skills[0].stages[1].recoveryFrame = 0.f;
        def.skills[1].stageCount = 1u;
        def.skills[1].replicatedCueId = 0u;
        def.skills[1].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[0].castFrame = 0.f;
        def.skills[1].stages[0].recoveryFrame = 0.f;
        def.skills[1].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[1].castFrame = 0.f;
        def.skills[1].stages[1].recoveryFrame = 0.f;
        def.skills[2].stageCount = 2u;
        def.skills[2].replicatedCueId = 0u;
        def.skills[2].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[0].castFrame = 0.f;
        def.skills[2].stages[0].recoveryFrame = 0.f;
        def.skills[2].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[1].castFrame = 0.f;
        def.skills[2].stages[1].recoveryFrame = 0.f;
        def.skills[3].stageCount = 1u;
        def.skills[3].replicatedCueId = 0u;
        def.skills[3].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[0].castFrame = 0.f;
        def.skills[3].stages[0].recoveryFrame = 0.f;
        def.skills[3].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[1].castFrame = 0.f;
        def.skills[3].stages[1].recoveryFrame = 0.f;
        def.skills[4].stageCount = 1u;
        def.skills[4].replicatedCueId = 0u;
        def.skills[4].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[0].castFrame = 0.f;
        def.skills[4].stages[0].recoveryFrame = 0.f;
        def.skills[4].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[1].castFrame = 0.f;
        def.skills[4].stages[1].recoveryFrame = 0.f;
        return def;
    }

    ClientData::ChampionVisualDefinition MakeChampionVisual_RIVEN()
    {
        ClientData::ChampionVisualDefinition def{};
        def.key = 0xAF33CE6Eu;
        def.legacyChampion = eChampion::RIVEN;
        def.modelYawOffsetRadians = 3.14159265f;
        def.skills[0].stageCount = 1u;
        def.skills[0].replicatedCueId = 0u;
        def.skills[0].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[0].stages[0].castFrame = 0.f;
        def.skills[0].stages[0].recoveryFrame = 0.f;
        def.skills[0].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[0].stages[1].castFrame = 0.f;
        def.skills[0].stages[1].recoveryFrame = 0.f;
        def.skills[1].stageCount = 1u;
        def.skills[1].replicatedCueId = 0u;
        def.skills[1].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[0].castFrame = 0.f;
        def.skills[1].stages[0].recoveryFrame = 0.f;
        def.skills[1].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[1].castFrame = 0.f;
        def.skills[1].stages[1].recoveryFrame = 0.f;
        def.skills[2].stageCount = 1u;
        def.skills[2].replicatedCueId = 0u;
        def.skills[2].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[0].castFrame = 0.f;
        def.skills[2].stages[0].recoveryFrame = 0.f;
        def.skills[2].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[1].castFrame = 0.f;
        def.skills[2].stages[1].recoveryFrame = 0.f;
        def.skills[3].stageCount = 1u;
        def.skills[3].replicatedCueId = 0u;
        def.skills[3].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[0].castFrame = 0.f;
        def.skills[3].stages[0].recoveryFrame = 0.f;
        def.skills[3].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[1].castFrame = 0.f;
        def.skills[3].stages[1].recoveryFrame = 0.f;
        def.skills[4].stageCount = 1u;
        def.skills[4].replicatedCueId = 0u;
        def.skills[4].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[0].castFrame = 0.f;
        def.skills[4].stages[0].recoveryFrame = 0.f;
        def.skills[4].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[1].castFrame = 0.f;
        def.skills[4].stages[1].recoveryFrame = 0.f;
        return def;
    }

    ClientData::ChampionVisualDefinition MakeChampionVisual_EZREAL()
    {
        ClientData::ChampionVisualDefinition def{};
        def.key = 0xD5D4F0F3u;
        def.legacyChampion = eChampion::EZREAL;
        def.modelYawOffsetRadians = 3.14159265f;
        def.skills[0].stageCount = 1u;
        def.skills[0].replicatedCueId = 0u;
        def.skills[0].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[0].stages[0].castFrame = 0.f;
        def.skills[0].stages[0].recoveryFrame = 0.f;
        def.skills[0].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[0].stages[1].castFrame = 0.f;
        def.skills[0].stages[1].recoveryFrame = 0.f;
        def.skills[1].stageCount = 1u;
        def.skills[1].replicatedCueId = 0u;
        def.skills[1].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[0].castFrame = 0.f;
        def.skills[1].stages[0].recoveryFrame = 0.f;
        def.skills[1].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[1].castFrame = 0.f;
        def.skills[1].stages[1].recoveryFrame = 0.f;
        def.skills[2].stageCount = 1u;
        def.skills[2].replicatedCueId = 0u;
        def.skills[2].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[0].castFrame = 0.f;
        def.skills[2].stages[0].recoveryFrame = 0.f;
        def.skills[2].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[1].castFrame = 0.f;
        def.skills[2].stages[1].recoveryFrame = 0.f;
        def.skills[3].stageCount = 1u;
        def.skills[3].replicatedCueId = 0u;
        def.skills[3].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[0].castFrame = 0.f;
        def.skills[3].stages[0].recoveryFrame = 0.f;
        def.skills[3].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[1].castFrame = 0.f;
        def.skills[3].stages[1].recoveryFrame = 0.f;
        def.skills[4].stageCount = 1u;
        def.skills[4].replicatedCueId = 0u;
        def.skills[4].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[0].castFrame = 0.f;
        def.skills[4].stages[0].recoveryFrame = 0.f;
        def.skills[4].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[1].castFrame = 0.f;
        def.skills[4].stages[1].recoveryFrame = 0.f;
        return def;
    }

    ClientData::ChampionVisualDefinition MakeChampionVisual_FIORA()
    {
        ClientData::ChampionVisualDefinition def{};
        def.key = 0x4D3C3313u;
        def.legacyChampion = eChampion::FIORA;
        def.modelYawOffsetRadians = 3.14159265f;
        def.skills[0].stageCount = 1u;
        def.skills[0].replicatedCueId = 0u;
        def.skills[0].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[0].stages[0].castFrame = 0.f;
        def.skills[0].stages[0].recoveryFrame = 0.f;
        def.skills[0].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[0].stages[1].castFrame = 0.f;
        def.skills[0].stages[1].recoveryFrame = 0.f;
        def.skills[1].stageCount = 1u;
        def.skills[1].replicatedCueId = 0u;
        def.skills[1].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[0].castFrame = 0.f;
        def.skills[1].stages[0].recoveryFrame = 0.f;
        def.skills[1].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[1].castFrame = 0.f;
        def.skills[1].stages[1].recoveryFrame = 0.f;
        def.skills[2].stageCount = 1u;
        def.skills[2].replicatedCueId = 0u;
        def.skills[2].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[0].castFrame = 0.f;
        def.skills[2].stages[0].recoveryFrame = 0.f;
        def.skills[2].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[1].castFrame = 0.f;
        def.skills[2].stages[1].recoveryFrame = 0.f;
        def.skills[3].stageCount = 1u;
        def.skills[3].replicatedCueId = 0u;
        def.skills[3].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[0].castFrame = 0.f;
        def.skills[3].stages[0].recoveryFrame = 0.f;
        def.skills[3].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[1].castFrame = 0.f;
        def.skills[3].stages[1].recoveryFrame = 0.f;
        def.skills[4].stageCount = 1u;
        def.skills[4].replicatedCueId = 0u;
        def.skills[4].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[0].castFrame = 0.f;
        def.skills[4].stages[0].recoveryFrame = 0.f;
        def.skills[4].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[1].castFrame = 0.f;
        def.skills[4].stages[1].recoveryFrame = 0.f;
        return def;
    }

    ClientData::ChampionVisualDefinition MakeChampionVisual_JAX()
    {
        ClientData::ChampionVisualDefinition def{};
        def.key = 0x0F445E37u;
        def.legacyChampion = eChampion::JAX;
        def.modelYawOffsetRadians = 3.14159265f;
        def.skills[0].stageCount = 1u;
        def.skills[0].replicatedCueId = 0u;
        def.skills[0].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[0].stages[0].castFrame = 0.f;
        def.skills[0].stages[0].recoveryFrame = 0.f;
        def.skills[0].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[0].stages[1].castFrame = 0.f;
        def.skills[0].stages[1].recoveryFrame = 0.f;
        def.skills[1].stageCount = 1u;
        def.skills[1].replicatedCueId = 0u;
        def.skills[1].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[0].castFrame = 0.f;
        def.skills[1].stages[0].recoveryFrame = 0.f;
        def.skills[1].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[1].castFrame = 0.f;
        def.skills[1].stages[1].recoveryFrame = 0.f;
        def.skills[2].stageCount = 1u;
        def.skills[2].replicatedCueId = 0u;
        def.skills[2].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[0].castFrame = 0.f;
        def.skills[2].stages[0].recoveryFrame = 0.f;
        def.skills[2].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[1].castFrame = 0.f;
        def.skills[2].stages[1].recoveryFrame = 0.f;
        def.skills[3].stageCount = 2u;
        def.skills[3].replicatedCueId = 0u;
        def.skills[3].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[0].castFrame = 0.f;
        def.skills[3].stages[0].recoveryFrame = 0.f;
        def.skills[3].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[1].castFrame = 0.f;
        def.skills[3].stages[1].recoveryFrame = 0.f;
        def.skills[4].stageCount = 1u;
        def.skills[4].replicatedCueId = 0u;
        def.skills[4].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[0].castFrame = 0.f;
        def.skills[4].stages[0].recoveryFrame = 0.f;
        def.skills[4].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[1].castFrame = 0.f;
        def.skills[4].stages[1].recoveryFrame = 0.f;
        return def;
    }

    ClientData::ChampionVisualDefinition MakeChampionVisual_LEESIN()
    {
        ClientData::ChampionVisualDefinition def{};
        def.key = 0xB01E6158u;
        def.legacyChampion = eChampion::LEESIN;
        def.modelYawOffsetRadians = 3.14159265f;
        def.skills[0].stageCount = 1u;
        def.skills[0].replicatedCueId = 0u;
        def.skills[0].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[0].stages[0].castFrame = 0.f;
        def.skills[0].stages[0].recoveryFrame = 0.f;
        def.skills[0].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[0].stages[1].castFrame = 0.f;
        def.skills[0].stages[1].recoveryFrame = 0.f;
        def.skills[1].stageCount = 2u;
        def.skills[1].replicatedCueId = 0u;
        def.skills[1].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[0].castFrame = 0.f;
        def.skills[1].stages[0].recoveryFrame = 0.f;
        def.skills[1].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[1].castFrame = 0.f;
        def.skills[1].stages[1].recoveryFrame = 0.f;
        def.skills[2].stageCount = 2u;
        def.skills[2].replicatedCueId = 0u;
        def.skills[2].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[0].castFrame = 0.f;
        def.skills[2].stages[0].recoveryFrame = 0.f;
        def.skills[2].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[1].castFrame = 0.f;
        def.skills[2].stages[1].recoveryFrame = 0.f;
        def.skills[3].stageCount = 2u;
        def.skills[3].replicatedCueId = 0u;
        def.skills[3].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[0].castFrame = 0.f;
        def.skills[3].stages[0].recoveryFrame = 0.f;
        def.skills[3].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[1].castFrame = 0.f;
        def.skills[3].stages[1].recoveryFrame = 0.f;
        def.skills[4].stageCount = 1u;
        def.skills[4].replicatedCueId = 0u;
        def.skills[4].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[0].castFrame = 0.f;
        def.skills[4].stages[0].recoveryFrame = 0.f;
        def.skills[4].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[1].castFrame = 0.f;
        def.skills[4].stages[1].recoveryFrame = 0.f;
        return def;
    }

    ClientData::ChampionVisualDefinition MakeChampionVisual_KINDRED()
    {
        ClientData::ChampionVisualDefinition def{};
        def.key = 0xD30227C9u;
        def.legacyChampion = eChampion::KINDRED;
        def.modelYawOffsetRadians = 3.14159265f;
        def.skills[0].stageCount = 1u;
        def.skills[0].replicatedCueId = 0u;
        def.skills[0].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[0].stages[0].castFrame = 0.f;
        def.skills[0].stages[0].recoveryFrame = 0.f;
        def.skills[0].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[0].stages[1].castFrame = 0.f;
        def.skills[0].stages[1].recoveryFrame = 0.f;
        def.skills[1].stageCount = 1u;
        def.skills[1].replicatedCueId = 0u;
        def.skills[1].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[0].castFrame = 0.f;
        def.skills[1].stages[0].recoveryFrame = 0.f;
        def.skills[1].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[1].castFrame = 0.f;
        def.skills[1].stages[1].recoveryFrame = 0.f;
        def.skills[2].stageCount = 1u;
        def.skills[2].replicatedCueId = 0u;
        def.skills[2].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[0].castFrame = 0.f;
        def.skills[2].stages[0].recoveryFrame = 0.f;
        def.skills[2].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[1].castFrame = 0.f;
        def.skills[2].stages[1].recoveryFrame = 0.f;
        def.skills[3].stageCount = 1u;
        def.skills[3].replicatedCueId = 0u;
        def.skills[3].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[0].castFrame = 0.f;
        def.skills[3].stages[0].recoveryFrame = 0.f;
        def.skills[3].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[1].castFrame = 0.f;
        def.skills[3].stages[1].recoveryFrame = 0.f;
        def.skills[4].stageCount = 1u;
        def.skills[4].replicatedCueId = 0u;
        def.skills[4].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[0].castFrame = 0.f;
        def.skills[4].stages[0].recoveryFrame = 0.f;
        def.skills[4].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[1].castFrame = 0.f;
        def.skills[4].stages[1].recoveryFrame = 0.f;
        return def;
    }

    ClientData::ChampionVisualDefinition MakeChampionVisual_MASTERYI()
    {
        ClientData::ChampionVisualDefinition def{};
        def.key = 0x7C99C014u;
        def.legacyChampion = eChampion::MASTERYI;
        def.modelYawOffsetRadians = 3.14159265f;
        def.skills[0].stageCount = 1u;
        def.skills[0].replicatedCueId = 0u;
        def.skills[0].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[0].stages[0].castFrame = 0.f;
        def.skills[0].stages[0].recoveryFrame = 0.f;
        def.skills[0].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[0].stages[1].castFrame = 0.f;
        def.skills[0].stages[1].recoveryFrame = 0.f;
        def.skills[1].stageCount = 1u;
        def.skills[1].replicatedCueId = 0u;
        def.skills[1].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[0].castFrame = 0.f;
        def.skills[1].stages[0].recoveryFrame = 0.f;
        def.skills[1].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[1].castFrame = 0.f;
        def.skills[1].stages[1].recoveryFrame = 0.f;
        def.skills[2].stageCount = 1u;
        def.skills[2].replicatedCueId = 0u;
        def.skills[2].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[0].castFrame = 0.f;
        def.skills[2].stages[0].recoveryFrame = 0.f;
        def.skills[2].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[1].castFrame = 0.f;
        def.skills[2].stages[1].recoveryFrame = 0.f;
        def.skills[3].stageCount = 1u;
        def.skills[3].replicatedCueId = 0u;
        def.skills[3].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[0].castFrame = 0.f;
        def.skills[3].stages[0].recoveryFrame = 0.f;
        def.skills[3].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[1].castFrame = 0.f;
        def.skills[3].stages[1].recoveryFrame = 0.f;
        def.skills[4].stageCount = 1u;
        def.skills[4].replicatedCueId = 0u;
        def.skills[4].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[0].castFrame = 0.f;
        def.skills[4].stages[0].recoveryFrame = 0.f;
        def.skills[4].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[1].castFrame = 0.f;
        def.skills[4].stages[1].recoveryFrame = 0.f;
        return def;
    }

    ClientData::ChampionVisualDefinition MakeChampionVisual_ANNIE()
    {
        ClientData::ChampionVisualDefinition def{};
        def.key = 0xA3618A11u;
        def.legacyChampion = eChampion::ANNIE;
        def.modelYawOffsetRadians = 3.14159265f;
        def.skills[0].stageCount = 1u;
        def.skills[0].replicatedCueId = 0u;
        def.skills[0].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[0].stages[0].castFrame = 0.f;
        def.skills[0].stages[0].recoveryFrame = 0.f;
        def.skills[0].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[0].stages[1].castFrame = 0.f;
        def.skills[0].stages[1].recoveryFrame = 0.f;
        def.skills[1].stageCount = 1u;
        def.skills[1].replicatedCueId = 0u;
        def.skills[1].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[0].castFrame = 0.f;
        def.skills[1].stages[0].recoveryFrame = 0.f;
        def.skills[1].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[1].castFrame = 0.f;
        def.skills[1].stages[1].recoveryFrame = 0.f;
        def.skills[2].stageCount = 1u;
        def.skills[2].replicatedCueId = 0u;
        def.skills[2].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[0].castFrame = 0.f;
        def.skills[2].stages[0].recoveryFrame = 0.f;
        def.skills[2].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[1].castFrame = 0.f;
        def.skills[2].stages[1].recoveryFrame = 0.f;
        def.skills[3].stageCount = 1u;
        def.skills[3].replicatedCueId = 0u;
        def.skills[3].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[0].castFrame = 0.f;
        def.skills[3].stages[0].recoveryFrame = 0.f;
        def.skills[3].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[1].castFrame = 0.f;
        def.skills[3].stages[1].recoveryFrame = 0.f;
        def.skills[4].stageCount = 1u;
        def.skills[4].replicatedCueId = 0u;
        def.skills[4].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[0].castFrame = 0.f;
        def.skills[4].stages[0].recoveryFrame = 0.f;
        def.skills[4].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[1].castFrame = 0.f;
        def.skills[4].stages[1].recoveryFrame = 0.f;
        return def;
    }

    ClientData::ChampionVisualDefinition MakeChampionVisual_ASHE()
    {
        ClientData::ChampionVisualDefinition def{};
        def.key = 0xEC6E77EFu;
        def.legacyChampion = eChampion::ASHE;
        def.modelYawOffsetRadians = 3.14159265f;
        def.skills[0].stageCount = 1u;
        def.skills[0].replicatedCueId = 0u;
        def.skills[0].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[0].stages[0].castFrame = 0.f;
        def.skills[0].stages[0].recoveryFrame = 0.f;
        def.skills[0].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[0].stages[1].castFrame = 0.f;
        def.skills[0].stages[1].recoveryFrame = 0.f;
        def.skills[1].stageCount = 1u;
        def.skills[1].replicatedCueId = 0u;
        def.skills[1].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[0].castFrame = 0.f;
        def.skills[1].stages[0].recoveryFrame = 0.f;
        def.skills[1].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[1].castFrame = 0.f;
        def.skills[1].stages[1].recoveryFrame = 0.f;
        def.skills[2].stageCount = 1u;
        def.skills[2].replicatedCueId = 0u;
        def.skills[2].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[0].castFrame = 0.f;
        def.skills[2].stages[0].recoveryFrame = 0.f;
        def.skills[2].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[1].castFrame = 0.f;
        def.skills[2].stages[1].recoveryFrame = 0.f;
        def.skills[3].stageCount = 1u;
        def.skills[3].replicatedCueId = 0u;
        def.skills[3].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[0].castFrame = 0.f;
        def.skills[3].stages[0].recoveryFrame = 0.f;
        def.skills[3].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[1].castFrame = 0.f;
        def.skills[3].stages[1].recoveryFrame = 0.f;
        def.skills[4].stageCount = 1u;
        def.skills[4].replicatedCueId = 0u;
        def.skills[4].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[0].castFrame = 0.f;
        def.skills[4].stages[0].recoveryFrame = 0.f;
        def.skills[4].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[1].castFrame = 0.f;
        def.skills[4].stages[1].recoveryFrame = 0.f;
        return def;
    }

    ClientData::ChampionVisualDefinition MakeChampionVisual_VIEGO()
    {
        ClientData::ChampionVisualDefinition def{};
        def.key = 0xF0FB5992u;
        def.legacyChampion = eChampion::VIEGO;
        def.modelYawOffsetRadians = 3.14159265f;
        def.skills[0].stageCount = 1u;
        def.skills[0].replicatedCueId = 0u;
        def.skills[0].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[0].stages[0].castFrame = 0.f;
        def.skills[0].stages[0].recoveryFrame = 0.f;
        def.skills[0].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[0].stages[1].castFrame = 0.f;
        def.skills[0].stages[1].recoveryFrame = 0.f;
        def.skills[1].stageCount = 1u;
        def.skills[1].replicatedCueId = 0u;
        def.skills[1].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[0].castFrame = 0.f;
        def.skills[1].stages[0].recoveryFrame = 0.f;
        def.skills[1].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[1].castFrame = 0.f;
        def.skills[1].stages[1].recoveryFrame = 0.f;
        def.skills[2].stageCount = 2u;
        def.skills[2].replicatedCueId = 0u;
        def.skills[2].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[0].castFrame = 0.f;
        def.skills[2].stages[0].recoveryFrame = 0.f;
        def.skills[2].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[1].castFrame = 0.f;
        def.skills[2].stages[1].recoveryFrame = 0.f;
        def.skills[3].stageCount = 1u;
        def.skills[3].replicatedCueId = 0u;
        def.skills[3].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[0].castFrame = 0.f;
        def.skills[3].stages[0].recoveryFrame = 0.f;
        def.skills[3].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[1].castFrame = 0.f;
        def.skills[3].stages[1].recoveryFrame = 0.f;
        def.skills[4].stageCount = 1u;
        def.skills[4].replicatedCueId = 0u;
        def.skills[4].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[0].castFrame = 0.f;
        def.skills[4].stages[0].recoveryFrame = 0.f;
        def.skills[4].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[1].castFrame = 0.f;
        def.skills[4].stages[1].recoveryFrame = 0.f;
        return def;
    }

    ClientData::ChampionVisualDefinition MakeChampionVisual_YONE()
    {
        ClientData::ChampionVisualDefinition def{};
        def.key = 0xC7C340B1u;
        def.legacyChampion = eChampion::YONE;
        def.modelYawOffsetRadians = 3.14159265f;
        def.skills[0].stageCount = 1u;
        def.skills[0].replicatedCueId = 0u;
        def.skills[0].stages[0].animationPlaybackSpeed = 0.85f;
        def.skills[0].stages[0].castFrame = 0.f;
        def.skills[0].stages[0].recoveryFrame = 0.f;
        def.skills[0].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[0].stages[1].castFrame = 0.f;
        def.skills[0].stages[1].recoveryFrame = 0.f;
        def.skills[1].stageCount = 1u;
        def.skills[1].replicatedCueId = 0u;
        def.skills[1].stages[0].animationPlaybackSpeed = 0.85f;
        def.skills[1].stages[0].castFrame = 0.f;
        def.skills[1].stages[0].recoveryFrame = 0.f;
        def.skills[1].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[1].castFrame = 0.f;
        def.skills[1].stages[1].recoveryFrame = 0.f;
        def.skills[2].stageCount = 1u;
        def.skills[2].replicatedCueId = 0u;
        def.skills[2].stages[0].animationPlaybackSpeed = 0.85f;
        def.skills[2].stages[0].castFrame = 0.f;
        def.skills[2].stages[0].recoveryFrame = 0.f;
        def.skills[2].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[1].castFrame = 0.f;
        def.skills[2].stages[1].recoveryFrame = 0.f;
        def.skills[3].stageCount = 2u;
        def.skills[3].replicatedCueId = 0u;
        def.skills[3].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[0].castFrame = 0.f;
        def.skills[3].stages[0].recoveryFrame = 0.f;
        def.skills[3].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[1].castFrame = 0.f;
        def.skills[3].stages[1].recoveryFrame = 0.f;
        def.skills[4].stageCount = 1u;
        def.skills[4].replicatedCueId = 0u;
        def.skills[4].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[0].castFrame = 0.f;
        def.skills[4].stages[0].recoveryFrame = 0.f;
        def.skills[4].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[1].castFrame = 0.f;
        def.skills[4].stages[1].recoveryFrame = 0.f;
        return def;
    }

    ClientData::ChampionVisualDefinition MakeChampionVisual_SYLAS()
    {
        ClientData::ChampionVisualDefinition def{};
        def.key = 0xB749515Cu;
        def.legacyChampion = eChampion::SYLAS;
        def.modelYawOffsetRadians = 3.14159265f;
        def.skills[0].stageCount = 1u;
        def.skills[0].replicatedCueId = 0u;
        def.skills[0].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[0].stages[0].castFrame = 0.f;
        def.skills[0].stages[0].recoveryFrame = 0.f;
        def.skills[0].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[0].stages[1].castFrame = 0.f;
        def.skills[0].stages[1].recoveryFrame = 0.f;
        def.skills[1].stageCount = 1u;
        def.skills[1].replicatedCueId = 0u;
        def.skills[1].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[0].castFrame = 0.f;
        def.skills[1].stages[0].recoveryFrame = 0.f;
        def.skills[1].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[1].castFrame = 0.f;
        def.skills[1].stages[1].recoveryFrame = 0.f;
        def.skills[2].stageCount = 1u;
        def.skills[2].replicatedCueId = 0u;
        def.skills[2].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[0].castFrame = 0.f;
        def.skills[2].stages[0].recoveryFrame = 0.f;
        def.skills[2].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[1].castFrame = 0.f;
        def.skills[2].stages[1].recoveryFrame = 0.f;
        def.skills[3].stageCount = 2u;
        def.skills[3].replicatedCueId = 0u;
        def.skills[3].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[0].castFrame = 0.f;
        def.skills[3].stages[0].recoveryFrame = 0.f;
        def.skills[3].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[1].castFrame = 0.f;
        def.skills[3].stages[1].recoveryFrame = 0.f;
        def.skills[4].stageCount = 1u;
        def.skills[4].replicatedCueId = 0u;
        def.skills[4].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[0].castFrame = 0.f;
        def.skills[4].stages[0].recoveryFrame = 0.f;
        def.skills[4].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[1].castFrame = 0.f;
        def.skills[4].stages[1].recoveryFrame = 0.f;
        return def;
    }

    const ClientData::ChampionVisualDefinition kChampionVisuals[] =
    {
        MakeChampionVisual_ANNIE(),
        MakeChampionVisual_ASHE(),
        MakeChampionVisual_EZREAL(),
        MakeChampionVisual_FIORA(),
        MakeChampionVisual_GAREN(),
        MakeChampionVisual_IRELIA(),
        MakeChampionVisual_JAX(),
        MakeChampionVisual_KALISTA(),
        MakeChampionVisual_KINDRED(),
        MakeChampionVisual_LEESIN(),
        MakeChampionVisual_MASTERYI(),
        MakeChampionVisual_RIVEN(),
        MakeChampionVisual_SYLAS(),
        MakeChampionVisual_VIEGO(),
        MakeChampionVisual_YASUO(),
        MakeChampionVisual_YONE(),
        MakeChampionVisual_ZED(),
    };
}

namespace ClientData
{
    const ChampionVisualDefinition* FindChampionVisualDefinition(eChampion champion)
    {
        for (const ChampionVisualDefinition& definition : kChampionVisuals)
        {
            if (definition.legacyChampion == champion)
                return &definition;
        }
        return nullptr;
    }

    f32_t ResolveChampionModelYawOffset(eChampion champion)
    {
        const ChampionVisualDefinition* definition = FindChampionVisualDefinition(champion);
        return definition ? definition->modelYawOffsetRadians : 0.f;
    }
}
