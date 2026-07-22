#include "Client/Private/Data/LoLVisualDefinitionPack.h"
#include "Client/Private/Data/RuntimeVisualDefinitionOverlay.h"

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
        def.skills[0].stages[0].castFrame = 6.f;
        def.skills[0].stages[0].recoveryFrame = 14.f;
        def.skills[1].stageCount = 1u;
        def.skills[1].replicatedCueId = 0u;
        def.skills[1].stages[0].animationPlaybackSpeed = 1.2f;
        def.skills[1].stages[0].castFrame = 8.f;
        def.skills[1].stages[0].recoveryFrame = 18.f;
        def.skills[2].stageCount = 2u;
        def.skills[2].replicatedCueId = 0u;
        def.skills[2].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[0].castFrame = 0.f;
        def.skills[2].stages[0].recoveryFrame = 7.f;
        def.skills[2].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[1].castFrame = 6.f;
        def.skills[2].stages[1].recoveryFrame = 14.f;
        def.skills[3].stageCount = 2u;
        def.skills[3].replicatedCueId = 0u;
        def.skills[3].stages[0].animationPlaybackSpeed = 1.05f;
        def.skills[3].stages[0].castFrame = 8.f;
        def.skills[3].stages[0].recoveryFrame = 18.f;
        def.skills[3].stages[1].animationPlaybackSpeed = 1.05f;
        def.skills[3].stages[1].castFrame = 5.f;
        def.skills[3].stages[1].recoveryFrame = 13.f;
        def.skills[4].stageCount = 1u;
        def.skills[4].replicatedCueId = 0u;
        def.skills[4].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[0].castFrame = 7.f;
        def.skills[4].stages[0].recoveryFrame = 30.f;
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
        def.skills[1].stageCount = 1u;
        def.skills[1].replicatedCueId = 0u;
        def.skills[1].stages[0].animationPlaybackSpeed = 0.85f;
        def.skills[1].stages[0].castFrame = 0.f;
        def.skills[1].stages[0].recoveryFrame = 0.f;
        def.skills[2].stageCount = 1u;
        def.skills[2].replicatedCueId = 0u;
        def.skills[2].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[0].castFrame = 0.f;
        def.skills[2].stages[0].recoveryFrame = 0.f;
        def.skills[3].stageCount = 1u;
        def.skills[3].replicatedCueId = 0u;
        def.skills[3].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[0].castFrame = 0.f;
        def.skills[3].stages[0].recoveryFrame = 0.f;
        def.skills[4].stageCount = 1u;
        def.skills[4].replicatedCueId = 0u;
        def.skills[4].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[0].castFrame = 0.f;
        def.skills[4].stages[0].recoveryFrame = 0.f;
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
        def.skills[0].stages[0].castFrame = 6.f;
        def.skills[0].stages[0].recoveryFrame = 14.f;
        def.skills[1].stageCount = 1u;
        def.skills[1].replicatedCueId = 0u;
        def.skills[1].stages[0].animationPlaybackSpeed = 2.8f;
        def.skills[1].stages[0].castFrame = 4.f;
        def.skills[1].stages[0].recoveryFrame = 10.f;
        def.skills[2].stageCount = 1u;
        def.skills[2].replicatedCueId = 0u;
        def.skills[2].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[0].castFrame = 0.f;
        def.skills[2].stages[0].recoveryFrame = 0.f;
        def.skills[3].stageCount = 1u;
        def.skills[3].replicatedCueId = 0u;
        def.skills[3].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[0].castFrame = 0.f;
        def.skills[3].stages[0].recoveryFrame = 0.f;
        def.skills[4].stageCount = 2u;
        def.skills[4].replicatedCueId = 0u;
        def.skills[4].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[0].castFrame = 0.f;
        def.skills[4].stages[0].recoveryFrame = 0.f;
        def.skills[4].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[1].castFrame = 4.f;
        def.skills[4].stages[1].recoveryFrame = 10.f;
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
        def.skills[0].stages[0].castFrame = 6.f;
        def.skills[0].stages[0].recoveryFrame = 14.f;
        def.skills[1].stageCount = 1u;
        def.skills[1].replicatedCueId = 0u;
        def.skills[1].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[0].castFrame = 4.f;
        def.skills[1].stages[0].recoveryFrame = 10.f;
        def.skills[2].stageCount = 1u;
        def.skills[2].replicatedCueId = 0u;
        def.skills[2].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[0].castFrame = 1.f;
        def.skills[2].stages[0].recoveryFrame = 8.f;
        def.skills[3].stageCount = 1u;
        def.skills[3].replicatedCueId = 0u;
        def.skills[3].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[0].castFrame = 12.f;
        def.skills[3].stages[0].recoveryFrame = 60.f;
        def.skills[4].stageCount = 1u;
        def.skills[4].replicatedCueId = 0u;
        def.skills[4].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[0].castFrame = 24.f;
        def.skills[4].stages[0].recoveryFrame = 36.f;
        return def;
    }

    ClientData::ChampionVisualDefinition MakeChampionVisual_ZED()
    {
        ClientData::ChampionVisualDefinition def{};
        def.key = 0x556E51C7u;
        def.legacyChampion = eChampion::ZED;
        def.modelYawOffsetRadians = 3.14159265f;
        def.skills[0].stageCount = 2u;
        def.skills[0].replicatedCueId = 0u;
        def.skills[0].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[0].stages[0].castFrame = 6.f;
        def.skills[0].stages[0].recoveryFrame = 14.f;
        def.skills[0].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[0].stages[1].castFrame = 6.f;
        def.skills[0].stages[1].recoveryFrame = 14.f;
        def.skills[1].stageCount = 1u;
        def.skills[1].replicatedCueId = 0u;
        def.skills[1].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[0].castFrame = 4.f;
        def.skills[1].stages[0].recoveryFrame = 10.f;
        def.skills[2].stageCount = 2u;
        def.skills[2].replicatedCueId = 0u;
        def.skills[2].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[0].castFrame = 1.f;
        def.skills[2].stages[0].recoveryFrame = 8.f;
        def.skills[2].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[1].castFrame = 1.f;
        def.skills[2].stages[1].recoveryFrame = 5.f;
        def.skills[3].stageCount = 1u;
        def.skills[3].replicatedCueId = 0u;
        def.skills[3].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[0].castFrame = 6.f;
        def.skills[3].stages[0].recoveryFrame = 14.f;
        def.skills[4].stageCount = 2u;
        def.skills[4].replicatedCueId = 0u;
        def.skills[4].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[0].castFrame = 18.f;
        def.skills[4].stages[0].recoveryFrame = 30.f;
        def.skills[4].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[1].castFrame = 1.f;
        def.skills[4].stages[1].recoveryFrame = 5.f;
        return def;
    }

    ClientData::ChampionVisualDefinition MakeChampionVisual_RIVEN()
    {
        ClientData::ChampionVisualDefinition def{};
        def.key = 0xAF33CE6Eu;
        def.legacyChampion = eChampion::RIVEN;
        def.modelYawOffsetRadians = 3.14159265f;
        def.skills[0].stageCount = 2u;
        def.skills[0].replicatedCueId = 0u;
        def.skills[0].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[0].stages[0].castFrame = 6.f;
        def.skills[0].stages[0].recoveryFrame = 14.f;
        def.skills[0].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[0].stages[1].castFrame = 6.f;
        def.skills[0].stages[1].recoveryFrame = 14.f;
        def.skills[1].stageCount = 1u;
        def.skills[1].replicatedCueId = 0u;
        def.skills[1].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[0].castFrame = 4.f;
        def.skills[1].stages[0].recoveryFrame = 10.f;
        def.skills[2].stageCount = 1u;
        def.skills[2].replicatedCueId = 0u;
        def.skills[2].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[0].castFrame = 4.f;
        def.skills[2].stages[0].recoveryFrame = 10.f;
        def.skills[3].stageCount = 1u;
        def.skills[3].replicatedCueId = 0u;
        def.skills[3].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[0].castFrame = 3.f;
        def.skills[3].stages[0].recoveryFrame = 8.f;
        def.skills[4].stageCount = 2u;
        def.skills[4].replicatedCueId = 0u;
        def.skills[4].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[0].castFrame = 6.f;
        def.skills[4].stages[0].recoveryFrame = 14.f;
        def.skills[4].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[1].castFrame = 6.f;
        def.skills[4].stages[1].recoveryFrame = 14.f;
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
        def.skills[0].stages[0].castFrame = 6.f;
        def.skills[0].stages[0].recoveryFrame = 12.f;
        def.skills[1].stageCount = 1u;
        def.skills[1].replicatedCueId = 0u;
        def.skills[1].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[0].castFrame = 4.f;
        def.skills[1].stages[0].recoveryFrame = 10.f;
        def.skills[2].stageCount = 1u;
        def.skills[2].replicatedCueId = 0u;
        def.skills[2].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[0].castFrame = 4.f;
        def.skills[2].stages[0].recoveryFrame = 10.f;
        def.skills[3].stageCount = 1u;
        def.skills[3].replicatedCueId = 0u;
        def.skills[3].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[0].castFrame = 1.f;
        def.skills[3].stages[0].recoveryFrame = 12.f;
        def.skills[4].stageCount = 1u;
        def.skills[4].replicatedCueId = 0u;
        def.skills[4].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[0].castFrame = 16.f;
        def.skills[4].stages[0].recoveryFrame = 24.f;
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
        def.skills[0].stages[0].castFrame = 6.f;
        def.skills[0].stages[0].recoveryFrame = 14.f;
        def.skills[1].stageCount = 1u;
        def.skills[1].replicatedCueId = 0u;
        def.skills[1].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[0].castFrame = 4.f;
        def.skills[1].stages[0].recoveryFrame = 10.f;
        def.skills[2].stageCount = 1u;
        def.skills[2].replicatedCueId = 0u;
        def.skills[2].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[0].castFrame = 1.f;
        def.skills[2].stages[0].recoveryFrame = 18.f;
        def.skills[3].stageCount = 1u;
        def.skills[3].replicatedCueId = 0u;
        def.skills[3].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[0].castFrame = 1.f;
        def.skills[3].stages[0].recoveryFrame = 8.f;
        def.skills[4].stageCount = 1u;
        def.skills[4].replicatedCueId = 0u;
        def.skills[4].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[0].castFrame = 18.f;
        def.skills[4].stages[0].recoveryFrame = 36.f;
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
        def.skills[0].stages[0].castFrame = 6.f;
        def.skills[0].stages[0].recoveryFrame = 14.f;
        def.skills[1].stageCount = 1u;
        def.skills[1].replicatedCueId = 0u;
        def.skills[1].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[0].castFrame = 6.f;
        def.skills[1].stages[0].recoveryFrame = 12.f;
        def.skills[2].stageCount = 1u;
        def.skills[2].replicatedCueId = 0u;
        def.skills[2].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[0].castFrame = 1.f;
        def.skills[2].stages[0].recoveryFrame = 8.f;
        def.skills[3].stageCount = 2u;
        def.skills[3].replicatedCueId = 0u;
        def.skills[3].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[0].castFrame = 1.f;
        def.skills[3].stages[0].recoveryFrame = 48.f;
        def.skills[3].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[1].castFrame = 6.f;
        def.skills[3].stages[1].recoveryFrame = 14.f;
        def.skills[4].stageCount = 1u;
        def.skills[4].replicatedCueId = 0u;
        def.skills[4].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[0].castFrame = 4.f;
        def.skills[4].stages[0].recoveryFrame = 12.f;
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
        def.skills[0].stages[0].castFrame = 4.f;
        def.skills[0].stages[0].recoveryFrame = 12.f;
        def.skills[1].stageCount = 2u;
        def.skills[1].replicatedCueId = 0u;
        def.skills[1].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[0].castFrame = 4.f;
        def.skills[1].stages[0].recoveryFrame = 12.f;
        def.skills[1].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[1].castFrame = 4.f;
        def.skills[1].stages[1].recoveryFrame = 12.f;
        def.skills[2].stageCount = 2u;
        def.skills[2].replicatedCueId = 0u;
        def.skills[2].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[0].castFrame = 4.f;
        def.skills[2].stages[0].recoveryFrame = 12.f;
        def.skills[2].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[1].castFrame = 4.f;
        def.skills[2].stages[1].recoveryFrame = 12.f;
        def.skills[3].stageCount = 2u;
        def.skills[3].replicatedCueId = 0u;
        def.skills[3].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[0].castFrame = 4.f;
        def.skills[3].stages[0].recoveryFrame = 12.f;
        def.skills[3].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[1].castFrame = 4.f;
        def.skills[3].stages[1].recoveryFrame = 12.f;
        def.skills[4].stageCount = 1u;
        def.skills[4].replicatedCueId = 0u;
        def.skills[4].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[0].castFrame = 4.f;
        def.skills[4].stages[0].recoveryFrame = 12.f;
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
        def.skills[0].stages[0].castFrame = 4.f;
        def.skills[0].stages[0].recoveryFrame = 12.f;
        def.skills[1].stageCount = 1u;
        def.skills[1].replicatedCueId = 0u;
        def.skills[1].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[0].castFrame = 4.f;
        def.skills[1].stages[0].recoveryFrame = 12.f;
        def.skills[2].stageCount = 1u;
        def.skills[2].replicatedCueId = 0u;
        def.skills[2].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[0].castFrame = 4.f;
        def.skills[2].stages[0].recoveryFrame = 12.f;
        def.skills[3].stageCount = 1u;
        def.skills[3].replicatedCueId = 0u;
        def.skills[3].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[0].castFrame = 4.f;
        def.skills[3].stages[0].recoveryFrame = 12.f;
        def.skills[4].stageCount = 1u;
        def.skills[4].replicatedCueId = 0u;
        def.skills[4].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[0].castFrame = 4.f;
        def.skills[4].stages[0].recoveryFrame = 12.f;
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
        def.skills[0].stages[0].castFrame = 4.f;
        def.skills[0].stages[0].recoveryFrame = 12.f;
        def.skills[1].stageCount = 1u;
        def.skills[1].replicatedCueId = 0u;
        def.skills[1].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[0].castFrame = 4.f;
        def.skills[1].stages[0].recoveryFrame = 12.f;
        def.skills[2].stageCount = 1u;
        def.skills[2].replicatedCueId = 0u;
        def.skills[2].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[0].castFrame = 4.f;
        def.skills[2].stages[0].recoveryFrame = 12.f;
        def.skills[3].stageCount = 1u;
        def.skills[3].replicatedCueId = 0u;
        def.skills[3].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[0].castFrame = 4.f;
        def.skills[3].stages[0].recoveryFrame = 12.f;
        def.skills[4].stageCount = 1u;
        def.skills[4].replicatedCueId = 0u;
        def.skills[4].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[0].castFrame = 4.f;
        def.skills[4].stages[0].recoveryFrame = 12.f;
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
        def.skills[0].stages[0].castFrame = 6.f;
        def.skills[0].stages[0].recoveryFrame = 14.f;
        def.skills[1].stageCount = 1u;
        def.skills[1].replicatedCueId = 0u;
        def.skills[1].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[0].castFrame = 5.f;
        def.skills[1].stages[0].recoveryFrame = 10.f;
        def.skills[2].stageCount = 1u;
        def.skills[2].replicatedCueId = 0u;
        def.skills[2].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[0].castFrame = 5.f;
        def.skills[2].stages[0].recoveryFrame = 12.f;
        def.skills[3].stageCount = 1u;
        def.skills[3].replicatedCueId = 0u;
        def.skills[3].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[0].castFrame = 1.f;
        def.skills[3].stages[0].recoveryFrame = 8.f;
        def.skills[4].stageCount = 1u;
        def.skills[4].replicatedCueId = 0u;
        def.skills[4].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[0].castFrame = 12.f;
        def.skills[4].stages[0].recoveryFrame = 24.f;
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
        def.skills[0].stages[0].castFrame = 5.f;
        def.skills[0].stages[0].recoveryFrame = 12.f;
        def.skills[1].stageCount = 1u;
        def.skills[1].replicatedCueId = 0u;
        def.skills[1].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[0].castFrame = 1.f;
        def.skills[1].stages[0].recoveryFrame = 8.f;
        def.skills[2].stageCount = 1u;
        def.skills[2].replicatedCueId = 0u;
        def.skills[2].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[0].castFrame = 4.f;
        def.skills[2].stages[0].recoveryFrame = 10.f;
        def.skills[3].stageCount = 1u;
        def.skills[3].replicatedCueId = 0u;
        def.skills[3].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[0].castFrame = 1.f;
        def.skills[3].stages[0].recoveryFrame = 10.f;
        def.skills[4].stageCount = 1u;
        def.skills[4].replicatedCueId = 0u;
        def.skills[4].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[0].castFrame = 12.f;
        def.skills[4].stages[0].recoveryFrame = 22.f;
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
        def.skills[0].stages[0].castFrame = 6.f;
        def.skills[0].stages[0].recoveryFrame = 14.f;
        def.skills[1].stageCount = 1u;
        def.skills[1].replicatedCueId = 0u;
        def.skills[1].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[0].castFrame = 5.f;
        def.skills[1].stages[0].recoveryFrame = 11.f;
        def.skills[2].stageCount = 2u;
        def.skills[2].replicatedCueId = 0u;
        def.skills[2].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[0].castFrame = 0.f;
        def.skills[2].stages[0].recoveryFrame = 7.f;
        def.skills[2].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[1].castFrame = 4.f;
        def.skills[2].stages[1].recoveryFrame = 10.f;
        def.skills[3].stageCount = 1u;
        def.skills[3].replicatedCueId = 0u;
        def.skills[3].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[0].castFrame = 4.f;
        def.skills[3].stages[0].recoveryFrame = 14.f;
        def.skills[4].stageCount = 1u;
        def.skills[4].replicatedCueId = 0u;
        def.skills[4].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[0].castFrame = 10.f;
        def.skills[4].stages[0].recoveryFrame = 18.f;
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
        def.skills[0].stages[0].castFrame = 5.f;
        def.skills[0].stages[0].recoveryFrame = 12.f;
        def.skills[1].stageCount = 1u;
        def.skills[1].replicatedCueId = 0u;
        def.skills[1].stages[0].animationPlaybackSpeed = 0.85f;
        def.skills[1].stages[0].castFrame = 4.f;
        def.skills[1].stages[0].recoveryFrame = 10.f;
        def.skills[2].stageCount = 1u;
        def.skills[2].replicatedCueId = 0u;
        def.skills[2].stages[0].animationPlaybackSpeed = 0.85f;
        def.skills[2].stages[0].castFrame = 5.f;
        def.skills[2].stages[0].recoveryFrame = 12.f;
        def.skills[3].stageCount = 2u;
        def.skills[3].replicatedCueId = 0u;
        def.skills[3].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[0].castFrame = 6.f;
        def.skills[3].stages[0].recoveryFrame = 14.f;
        def.skills[3].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[1].castFrame = 0.f;
        def.skills[3].stages[1].recoveryFrame = 0.f;
        def.skills[4].stageCount = 1u;
        def.skills[4].replicatedCueId = 0u;
        def.skills[4].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[0].castFrame = 12.f;
        def.skills[4].stages[0].recoveryFrame = 24.f;
        return def;
    }

    ClientData::ChampionVisualDefinition MakeChampionVisual_SYLAS()
    {
        ClientData::ChampionVisualDefinition def{};
        def.key = 0xB749515Cu;
        def.legacyChampion = eChampion::SYLAS;
        def.modelYawOffsetRadians = 3.14159265f;
        def.skills[0].stageCount = 2u;
        def.skills[0].replicatedCueId = 0u;
        def.skills[0].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[0].stages[0].castFrame = 4.f;
        def.skills[0].stages[0].recoveryFrame = 12.f;
        def.skills[0].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[0].stages[1].castFrame = 4.f;
        def.skills[0].stages[1].recoveryFrame = 12.f;
        def.skills[1].stageCount = 1u;
        def.skills[1].replicatedCueId = 0u;
        def.skills[1].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[1].stages[0].castFrame = 4.f;
        def.skills[1].stages[0].recoveryFrame = 12.f;
        def.skills[2].stageCount = 1u;
        def.skills[2].replicatedCueId = 0u;
        def.skills[2].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[2].stages[0].castFrame = 4.f;
        def.skills[2].stages[0].recoveryFrame = 12.f;
        def.skills[3].stageCount = 2u;
        def.skills[3].replicatedCueId = 0u;
        def.skills[3].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[0].castFrame = 4.f;
        def.skills[3].stages[0].recoveryFrame = 12.f;
        def.skills[3].stages[1].animationPlaybackSpeed = 1.f;
        def.skills[3].stages[1].castFrame = 4.f;
        def.skills[3].stages[1].recoveryFrame = 12.f;
        def.skills[4].stageCount = 1u;
        def.skills[4].replicatedCueId = 0u;
        def.skills[4].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[0].castFrame = 4.f;
        def.skills[4].stages[0].recoveryFrame = 12.f;
        return def;
    }

    ClientData::ChampionModelVisualDefinition MakeChampionModelVisual_ANNIE()
    {
        ClientData::ChampionModelVisualDefinition def{};
        def.key = 0x1D803E1Eu;
        def.champion = eChampion::ANNIE;
        def.displayName = "Annie";
        def.animPrefix = "";
        def.idleAnimation = "annie_2012_idle1";
        def.runAnimation = "annie_2012_run";
        def.basicAttackAnimation = "annie_2012_attack1";
        def.basicAttackRange = 6.25f;
        def.mesh.resourceRelativePath = "Texture/Character/Annie/annie.fbx";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.defaultTexture.resourceRelativePath = L"Texture/Character/Annie/annie_base_2012_cm.png";
        def.textureSlots[0].resourceRelativePath = L"Texture/Character/Annie/annie_base_2012_cm.png";
        def.textureSlots[1].resourceRelativePath = L"Texture/Character/Annie/annie_base_2012_cm.png";
        def.textureSlots[2].resourceRelativePath = L"Texture/Character/Annie/annie_base_2012_cm.png";
        def.textureSlots[3].resourceRelativePath = L"Texture/Character/Annie/annie_base_2012_cm.png";
        def.textureSlots[4].resourceRelativePath = L"Texture/Character/Annie/annie_base_2012_cm.png";
        def.textureSlots[5].resourceRelativePath = L"Texture/Character/Annie/annie_base_2012_cm.png";
        def.textureSlots[6].resourceRelativePath = L"Texture/Character/Annie/annie_base_2012_cm.png";
        def.textureSlots[7].resourceRelativePath = L"Texture/Character/Annie/annie_base_2012_cm.png";
        def.spawnPositionX = 36.f;
        def.spawnPositionY = 1.f;
        def.spawnPositionZ = 0.f;
        def.spawnScale = 0.01f;
        return def;
    }

    ClientData::ChampionModelVisualDefinition MakeChampionModelVisual_ASHE()
    {
        ClientData::ChampionModelVisualDefinition def{};
        def.key = 0x59142B12u;
        def.champion = eChampion::ASHE;
        def.displayName = "Ashe";
        def.animPrefix = "ashe_";
        def.idleAnimation = "idle1";
        def.runAnimation = "run";
        def.basicAttackAnimation = "attack1";
        def.basicAttackRange = 6.f;
        def.mesh.resourceRelativePath = "Texture/Character/Ashe/ashe.fbx";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.defaultTexture.resourceRelativePath = L"Texture/Character/Ashe/ashe_base_2011_tx_cm.png";
        def.textureSlots[0].resourceRelativePath = L"Texture/Character/Ashe/ashe_base_2011_tx_cm.png";
        def.textureSlots[1].resourceRelativePath = L"Texture/Character/Ashe/ashe_base_2011_tx_cm.png";
        def.textureSlots[2].resourceRelativePath = L"Texture/Character/Ashe/ashe_base_2011_tx_cm.png";
        def.textureSlots[3].resourceRelativePath = L"Texture/Character/Ashe/ashe_base_2011_tx_cm.png";
        def.textureSlots[4].resourceRelativePath = L"Texture/Character/Ashe/ashe_base_2011_tx_cm.png";
        def.textureSlots[5].resourceRelativePath = L"Texture/Character/Ashe/ashe_base_2011_tx_cm.png";
        def.textureSlots[6].resourceRelativePath = L"Texture/Character/Ashe/ashe_base_2011_tx_cm.png";
        def.textureSlots[7].resourceRelativePath = L"Texture/Character/Ashe/ashe_base_2011_tx_cm.png";
        def.spawnPositionX = 39.f;
        def.spawnPositionY = 1.f;
        def.spawnPositionZ = 0.f;
        def.spawnScale = 0.01f;
        return def;
    }

    ClientData::ChampionModelVisualDefinition MakeChampionModelVisual_EZREAL()
    {
        ClientData::ChampionModelVisualDefinition def{};
        def.key = 0x100FF9A2u;
        def.champion = eChampion::EZREAL;
        def.displayName = "Ezreal";
        def.animPrefix = "ezreal_";
        def.idleAnimation = "idle";
        def.runAnimation = "run";
        def.basicAttackAnimation = "attack1";
        def.basicAttackRange = 5.5f;
        def.mesh.resourceRelativePath = "Texture/Character/Ezreal/ezreal.fbx";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.defaultTexture.resourceRelativePath = L"Texture/Character/Ezreal/ezreal_base_tx_cm.png";
        def.textureSlots[0].resourceRelativePath = L"Texture/Character/Ezreal/ezreal_base_tx_cm.png";
        def.textureSlots[1].resourceRelativePath = L"Texture/Character/Ezreal/ezreal_base_tx_cm.png";
        def.textureSlots[2].resourceRelativePath = L"Texture/Character/Ezreal/ezreal_base_tx_cm.png";
        def.textureSlots[3].resourceRelativePath = L"Texture/Character/Ezreal/ezreal_base_tx_cm.png";
        def.textureSlots[4].resourceRelativePath = L"Texture/Character/Ezreal/ezreal_base_tx_cm.png";
        def.textureSlots[5].resourceRelativePath = L"Texture/Character/Ezreal/ezreal_base_tx_cm.png";
        def.textureSlots[6].resourceRelativePath = L"Texture/Character/Ezreal/ezreal_base_tx_cm.png";
        def.textureSlots[7].resourceRelativePath = L"Texture/Character/Ezreal/ezreal_base_tx_cm.png";
        def.spawnPositionX = 27.f;
        def.spawnPositionY = 1.f;
        def.spawnPositionZ = 0.f;
        def.spawnScale = 0.01f;
        return def;
    }

    ClientData::ChampionModelVisualDefinition MakeChampionModelVisual_FIORA()
    {
        ClientData::ChampionModelVisualDefinition def{};
        def.key = 0xC07FC7D8u;
        def.champion = eChampion::FIORA;
        def.displayName = "Fiora";
        def.animPrefix = "fiora_";
        def.idleAnimation = "idle1";
        def.runAnimation = "run";
        def.basicAttackAnimation = "attack1";
        def.basicAttackRange = 1.5f;
        def.mesh.resourceRelativePath = "Texture/Character/Fiora/fiora.fbx";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.defaultTexture.resourceRelativePath = L"Texture/Character/Fiora/fiora_base_tx_cm.png";
        def.textureSlots[0].resourceRelativePath = L"Texture/Character/Fiora/fiora_base_tx_cm.png";
        def.textureSlots[1].resourceRelativePath = L"Texture/Character/Fiora/fiora_base_tx_cm.png";
        def.textureSlots[2].resourceRelativePath = L"Texture/Character/Fiora/fiora_base_tx_cm.png";
        def.textureSlots[3].resourceRelativePath = L"Texture/Character/Fiora/fiora_base_tx_cm.png";
        def.textureSlots[4].resourceRelativePath = L"Texture/Character/Fiora/fiora_base_tx_cm.png";
        def.textureSlots[5].resourceRelativePath = L"Texture/Character/Fiora/fiora_base_tx_cm.png";
        def.textureSlots[6].resourceRelativePath = L"Texture/Character/Fiora/fiora_base_tx_cm.png";
        def.textureSlots[7].resourceRelativePath = L"Texture/Character/Fiora/fiora_base_tx_cm.png";
        def.spawnPositionX = 30.f;
        def.spawnPositionY = 1.f;
        def.spawnPositionZ = 0.f;
        def.spawnScale = 0.01f;
        return def;
    }

    ClientData::ChampionModelVisualDefinition MakeChampionModelVisual_GAREN()
    {
        ClientData::ChampionModelVisualDefinition def{};
        def.key = 0x33AF4276u;
        def.champion = eChampion::GAREN;
        def.displayName = "Garen";
        def.animPrefix = "";
        def.idleAnimation = "garen_2013_idle1";
        def.runAnimation = "garen_2013_run";
        def.basicAttackAnimation = "garen_2013_attack_01";
        def.basicAttackRange = 1.5f;
        def.mesh.resourceRelativePath = "Texture/Character/Garen/garen.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.defaultTexture.resourceRelativePath = L"Texture/Character/Garen/garen_base_tx_cm.png";
        def.textureSlots[1].resourceRelativePath = L"Texture/Character/Garen/garen_base_tx_cm.png";
        def.spawnPositionX = 33.f;
        def.spawnPositionY = 1.f;
        def.spawnPositionZ = -6.f;
        def.spawnScale = 0.01f;
        return def;
    }

    ClientData::ChampionModelVisualDefinition MakeChampionModelVisual_IRELIA()
    {
        ClientData::ChampionModelVisualDefinition def{};
        def.key = 0xE6BACB31u;
        def.champion = eChampion::IRELIA;
        def.displayName = "Irelia";
        def.animPrefix = "irelia_";
        def.idleAnimation = "idle_01";
        def.runAnimation = "run_base";
        def.basicAttackAnimation = "attack_01";
        def.basicAttackRange = 1.5f;
        def.mesh.resourceRelativePath = "Texture/Character/Irelia/irelia_fixed.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.defaultTexture.resourceRelativePath = L"Texture/Character/Irelia/irelia_base_tx_cm.png";
        def.textureSlots[0].resourceRelativePath = L"Texture/Character/Irelia/irelia_base_blades_tx_cm.png";
        def.textureSlots[1].resourceRelativePath = L"Texture/Character/Irelia/irelia_base_tx_cm.png";
        def.textureSlots[2].resourceRelativePath = L"Texture/Character/Irelia/irelia_base_tx_cm.png";
        def.textureSlots[3].resourceRelativePath = L"Texture/Character/Irelia/irelia_base_blades_tx_cm.png";
        def.spawnPositionX = 24.f;
        def.spawnPositionY = 1.f;
        def.spawnPositionZ = -6.f;
        def.spawnScale = 0.01f;
        return def;
    }

    ClientData::ChampionModelVisualDefinition MakeChampionModelVisual_JAX()
    {
        ClientData::ChampionModelVisualDefinition def{};
        def.key = 0xE0444FC8u;
        def.champion = eChampion::JAX;
        def.displayName = "Jax";
        def.animPrefix = "";
        def.idleAnimation = "idle1_v04";
        def.runAnimation = "jax_run2";
        def.basicAttackAnimation = "attack_1";
        def.basicAttackRange = 1.5f;
        def.mesh.resourceRelativePath = "Texture/Character/Jax/jax.fbx";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.defaultTexture.resourceRelativePath = L"Texture/Character/Jax/jax_base_body_tx_cm.png";
        def.textureSlots[0].resourceRelativePath = L"Texture/Character/Jax/jax_base_body_tx_cm.png";
        def.textureSlots[1].resourceRelativePath = L"Texture/Character/Jax/jax_base_body_tx_cm.png";
        def.textureSlots[2].resourceRelativePath = L"Texture/Character/Jax/jax_base_body_tx_cm.png";
        def.textureSlots[3].resourceRelativePath = L"Texture/Character/Jax/jax_base_fish_tx_cm.png";
        def.textureSlots[4].resourceRelativePath = L"Texture/Character/Jax/jax_base_weapon_tx_cm.png";
        def.textureSlots[5].resourceRelativePath = L"Texture/Character/Jax/jax_base_body_tx_cm.png";
        def.textureSlots[6].resourceRelativePath = L"Texture/Character/Jax/jax_base_body_tx_cm.png";
        def.textureSlots[7].resourceRelativePath = L"Texture/Character/Jax/jax_base_body_tx_cm.png";
        def.spawnPositionX = 33.f;
        def.spawnPositionY = 1.f;
        def.spawnPositionZ = 0.f;
        def.spawnScale = 0.01f;
        return def;
    }

    ClientData::ChampionModelVisualDefinition MakeChampionModelVisual_KALISTA()
    {
        ClientData::ChampionModelVisualDefinition def{};
        def.key = 0xD7751F88u;
        def.champion = eChampion::KALISTA;
        def.displayName = "Kalista";
        def.animPrefix = "kalista_";
        def.idleAnimation = "idle1";
        def.runAnimation = "run";
        def.basicAttackAnimation = "attack1";
        def.basicAttackRange = 5.5f;
        def.mesh.resourceRelativePath = "Texture/Character/Kalista/kalista.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.defaultTexture.resourceRelativePath = L"Texture/Character/Kalista/kalista_base_tx_cm.png";
        def.textureSlots[0].resourceRelativePath = L"Texture/Character/Kalista/kalista_base_tx_cm.png";
        def.textureSlots[1].resourceRelativePath = L"Texture/Character/Kalista/kalista_base_tx_cm.png";
        def.textureSlots[2].resourceRelativePath = L"Texture/Character/Kalista/kalistaaltar_base_tx_cm.png";
        def.spawnPositionX = 30.f;
        def.spawnPositionY = 1.f;
        def.spawnPositionZ = -6.f;
        def.spawnScale = 0.01f;
        return def;
    }

    ClientData::ChampionModelVisualDefinition MakeChampionModelVisual_KINDRED()
    {
        ClientData::ChampionModelVisualDefinition def{};
        def.key = 0xCCA0C886u;
        def.champion = eChampion::KINDRED;
        def.displayName = "Kindred";
        def.animPrefix = "";
        def.idleAnimation = "skinned_mesh_lamb_idle";
        def.runAnimation = "skinned_mesh_lamb_run";
        def.basicAttackAnimation = "skinned_mesh_lamb_attack1";
        def.basicAttackRange = 5.5f;
        def.mesh.resourceRelativePath = "Texture/Character/Kindred/kindred.fbx";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.defaultTexture.resourceRelativePath = L"Texture/Character/Kindred/kindred_base_tx_cm.png";
        def.textureSlots[0].resourceRelativePath = L"Texture/Character/Kindred/kindred_base_tx_cm.png";
        def.textureSlots[1].resourceRelativePath = L"Texture/Character/Kindred/kindred_base_tx_cm.png";
        def.textureSlots[2].resourceRelativePath = L"Texture/Character/Kindred/kindred_base_tx_cm.png";
        def.textureSlots[3].resourceRelativePath = L"Texture/Character/Kindred/kindred_base_tx_cm.png";
        def.textureSlots[4].resourceRelativePath = L"Texture/Character/Kindred/kindred_base_tx_cm.png";
        def.textureSlots[5].resourceRelativePath = L"Texture/Character/Kindred/kindred_base_tx_cm.png";
        def.textureSlots[6].resourceRelativePath = L"Texture/Character/Kindred/kindred_base_tx_cm.png";
        def.textureSlots[7].resourceRelativePath = L"Texture/Character/Kindred/kindred_base_tx_cm.png";
        def.spawnPositionX = 42.f;
        def.spawnPositionY = 1.f;
        def.spawnPositionZ = 0.f;
        def.spawnScale = 0.01f;
        return def;
    }

    ClientData::ChampionModelVisualDefinition MakeChampionModelVisual_LEESIN()
    {
        ClientData::ChampionModelVisualDefinition def{};
        def.key = 0x8163A281u;
        def.champion = eChampion::LEESIN;
        def.displayName = "LeeSin";
        def.animPrefix = "";
        def.idleAnimation = "skinned_mesh_idle_passive";
        def.runAnimation = "skinned_mesh_run_base";
        def.basicAttackAnimation = "skinned_mesh_attack1";
        def.basicAttackRange = 1.5f;
        def.mesh.resourceRelativePath = "Texture/Character/LeeSin/leesin.fbx";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.defaultTexture.resourceRelativePath = L"Texture/Character/LeeSin/leesin_base_tx_cm.png";
        def.textureSlots[0].resourceRelativePath = L"Texture/Character/LeeSin/leesin_base_tx_cm.png";
        def.textureSlots[1].resourceRelativePath = L"Texture/Character/LeeSin/leesin_base_tx_cm.png";
        def.textureSlots[2].resourceRelativePath = L"Texture/Character/LeeSin/leesin_base_tx_cm.png";
        def.textureSlots[3].resourceRelativePath = L"Texture/Character/LeeSin/leesin_base_tx_cm.png";
        def.textureSlots[4].resourceRelativePath = L"Texture/Character/LeeSin/leesin_base_tx_cm.png";
        def.textureSlots[5].resourceRelativePath = L"Texture/Character/LeeSin/leesin_base_tx_cm.png";
        def.textureSlots[6].resourceRelativePath = L"Texture/Character/LeeSin/leesin_base_tx_cm.png";
        def.textureSlots[7].resourceRelativePath = L"Texture/Character/LeeSin/leesin_base_tx_cm.png";
        def.spawnPositionX = 39.f;
        def.spawnPositionY = 1.f;
        def.spawnPositionZ = 0.f;
        def.spawnScale = 0.01f;
        return def;
    }

    ClientData::ChampionModelVisualDefinition MakeChampionModelVisual_MASTERYI()
    {
        ClientData::ChampionModelVisualDefinition def{};
        def.key = 0x2E2C7BF9u;
        def.champion = eChampion::MASTERYI;
        def.displayName = "MasterYi";
        def.animPrefix = "";
        def.idleAnimation = "skinned_mesh_masteryi_2013_idle1";
        def.runAnimation = "skinned_mesh_masteryi_2013_run";
        def.basicAttackAnimation = "skinned_mesh_masteryi_2013_attack1";
        def.basicAttackRange = 1.5f;
        def.mesh.resourceRelativePath = "Texture/Character/MasterYi/masteryi.fbx";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.defaultTexture.resourceRelativePath = L"Texture/Character/MasterYi/masteryi_2013_tx_cm.png";
        def.textureSlots[0].resourceRelativePath = L"Texture/Character/MasterYi/masteryi_2013_tx_cm.png";
        def.textureSlots[1].resourceRelativePath = L"Texture/Character/MasterYi/masteryi_2013_tx_cm.png";
        def.textureSlots[2].resourceRelativePath = L"Texture/Character/MasterYi/masteryi_2013_tx_cm.png";
        def.textureSlots[3].resourceRelativePath = L"Texture/Character/MasterYi/masteryi_2013_tx_cm.png";
        def.textureSlots[4].resourceRelativePath = L"Texture/Character/MasterYi/masteryi_2013_tx_cm.png";
        def.textureSlots[5].resourceRelativePath = L"Texture/Character/MasterYi/masteryi_2013_tx_cm.png";
        def.textureSlots[6].resourceRelativePath = L"Texture/Character/MasterYi/masteryi_2013_tx_cm.png";
        def.textureSlots[7].resourceRelativePath = L"Texture/Character/MasterYi/masteryi_2013_tx_cm.png";
        def.spawnPositionX = 45.f;
        def.spawnPositionY = 1.f;
        def.spawnPositionZ = 0.f;
        def.spawnScale = 0.01f;
        return def;
    }

    ClientData::ChampionModelVisualDefinition MakeChampionModelVisual_RIVEN()
    {
        ClientData::ChampionModelVisualDefinition def{};
        def.key = 0xEA8A8B01u;
        def.champion = eChampion::RIVEN;
        def.displayName = "Riven";
        def.animPrefix = "riven_";
        def.idleAnimation = "idle1";
        def.runAnimation = "run";
        def.basicAttackAnimation = "attack1";
        def.basicAttackRange = 1.5f;
        def.mesh.resourceRelativePath = "Texture/Character/Riven/riven.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.defaultTexture.resourceRelativePath = L"Texture/Character/Riven/riven_base_tx_cm.png";
        def.spawnPositionX = 24.f;
        def.spawnPositionY = 1.f;
        def.spawnPositionZ = 0.f;
        def.spawnScale = 0.0165f;
        return def;
    }

    ClientData::ChampionModelVisualDefinition MakeChampionModelVisual_SYLAS()
    {
        ClientData::ChampionModelVisualDefinition def{};
        def.key = 0x55FD97BFu;
        def.champion = eChampion::SYLAS;
        def.displayName = "Sylas";
        def.animPrefix = "";
        def.idleAnimation = "skinned_mesh_sylas_idle";
        def.runAnimation = "skinned_mesh_sylas_run";
        def.basicAttackAnimation = "skinned_mesh_sylas_attack_01";
        def.basicAttackRange = 1.5f;
        def.mesh.resourceRelativePath = "Texture/Character/Sylas/sylas.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.defaultTexture.resourceRelativePath = L"Texture/Character/Sylas/sylas_base_tx_cm.png";
        def.textureSlots[0].resourceRelativePath = L"Texture/Character/Sylas/sylas_base_tx_cm.png";
        def.textureSlots[1].resourceRelativePath = L"Texture/Character/Sylas/sylas_base_tx_cm.png";
        def.textureSlots[2].resourceRelativePath = L"Texture/Character/Sylas/sylas_base_shackles_tx_cm.png";
        def.textureSlots[3].resourceRelativePath = L"Texture/Character/Sylas/sylas_base_shackles_tx_cm.png";
        def.textureSlots[4].resourceRelativePath = L"Texture/Character/Sylas/sylas_base_shackles_tx_cm.png";
        def.textureSlots[5].resourceRelativePath = L"Texture/Character/Sylas/sylas_base_shackles_tx_cm.png";
        def.spawnPositionX = -27.f;
        def.spawnPositionY = 1.f;
        def.spawnPositionZ = 6.f;
        def.spawnScale = 0.01f;
        return def;
    }

    ClientData::ChampionModelVisualDefinition MakeChampionModelVisual_VIEGO()
    {
        ClientData::ChampionModelVisualDefinition def{};
        def.key = 0x69563DE9u;
        def.champion = eChampion::VIEGO;
        def.displayName = "Viego";
        def.animPrefix = "viego_";
        def.idleAnimation = "idle1";
        def.runAnimation = "run";
        def.basicAttackAnimation = "attack1";
        def.basicAttackRange = 1.5f;
        def.mesh.resourceRelativePath = "Texture/Character/Viego/viego_fixed.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.defaultTexture.resourceRelativePath = L"Texture/Character/Viego/viego_base_body_tx_cm.png";
        def.textureSlots[3].resourceRelativePath = L"Texture/Character/Viego/viego_base_crown_sword_tx_cm.png";
        def.textureSlots[4].resourceRelativePath = L"Texture/Character/Viego/viego_base_crown_sword_tx_cm.png";
        def.textureSlots[5].resourceRelativePath = L"Texture/Character/Viego/viego_base_wraith_tx_cm.png";
        def.spawnPositionX = -30.f;
        def.spawnPositionY = 1.f;
        def.spawnPositionZ = 6.f;
        def.spawnScale = 0.01f;
        return def;
    }

    ClientData::ChampionModelVisualDefinition MakeChampionModelVisual_YASUO()
    {
        ClientData::ChampionModelVisualDefinition def{};
        def.key = 0x9CB6FB28u;
        def.champion = eChampion::YASUO;
        def.displayName = "Yasuo";
        def.animPrefix = "yasuo_";
        def.idleAnimation = "idle1";
        def.runAnimation = "run1";
        def.basicAttackAnimation = "attack1";
        def.basicAttackRange = 1.5f;
        def.mesh.resourceRelativePath = "Texture/Character/Yasuo/yasuo_fixed.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.defaultTexture.resourceRelativePath = L"Texture/Character/Yasuo/yasuo_base_tx_cm.png";
        def.textureSlots[0].resourceRelativePath = L"Texture/Character/Yasuo/yasuo_base_tx_cm.png";
        def.textureSlots[1].resourceRelativePath = L"Texture/Character/Yasuo/yasuo_base_tx_cm.png";
        def.textureSlots[2].resourceRelativePath = L"Texture/Character/Yasuo/yasuo_base_tx_cm.png";
        def.textureSlots[3].resourceRelativePath = L"Texture/Character/Yasuo/yasuo_base_tx_cm.png";
        def.spawnPositionX = 27.f;
        def.spawnPositionY = 1.f;
        def.spawnPositionZ = -6.f;
        def.spawnScale = 0.01f;
        return def;
    }

    ClientData::ChampionModelVisualDefinition MakeChampionModelVisual_YONE()
    {
        ClientData::ChampionModelVisualDefinition def{};
        def.key = 0xD8F0540Cu;
        def.champion = eChampion::YONE;
        def.displayName = "Yone";
        def.animPrefix = "yone_";
        def.idleAnimation = "idle1";
        def.runAnimation = "run1";
        def.basicAttackAnimation = "attack1";
        def.basicAttackRange = 1.5f;
        def.mesh.resourceRelativePath = "Texture/Character/Yone/yone.fbx";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.defaultTexture.resourceRelativePath = L"Texture/Character/Yone/yone_base_tx_cm.png";
        def.textureSlots[0].resourceRelativePath = L"Texture/Character/Yone/yone_base_tx_cm.png";
        def.textureSlots[1].resourceRelativePath = L"Texture/Character/Yone/yone_base_swords_tx_cm.png";
        def.textureSlots[2].resourceRelativePath = L"Texture/Character/Yone/yone_base_swords_tx_cm.png";
        def.textureSlots[3].resourceRelativePath = L"Texture/Character/Yone/yone_base_props_tx_cm.png";
        def.textureSlots[4].resourceRelativePath = L"Texture/Character/Yone/yone_base_tx_cm.png";
        def.textureSlots[5].resourceRelativePath = L"Texture/Character/Yone/yone_base_tx_cm.png";
        def.textureSlots[6].resourceRelativePath = L"Texture/Character/Yone/yone_base_tx_cm.png";
        def.textureSlots[7].resourceRelativePath = L"Texture/Character/Yone/yone_base_tx_cm.png";
        def.spawnPositionX = 45.f;
        def.spawnPositionY = 1.f;
        def.spawnPositionZ = 0.f;
        def.spawnScale = 0.01f;
        return def;
    }

    ClientData::ChampionModelVisualDefinition MakeChampionModelVisual_ZED()
    {
        ClientData::ChampionModelVisualDefinition def{};
        def.key = 0x2F213B68u;
        def.champion = eChampion::ZED;
        def.displayName = "Zed";
        def.animPrefix = "";
        def.idleAnimation = "zed_idle1";
        def.runAnimation = "zed_run";
        def.basicAttackAnimation = "zed_attack1";
        def.basicAttackRange = 1.5f;
        def.mesh.resourceRelativePath = "Texture/Character/Zed/zed.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.defaultTexture.resourceRelativePath = L"Texture/Character/Zed/zed_base_tx_cm.png";
        def.textureSlots[0].resourceRelativePath = L"Texture/Character/Zed/zed_base_tx_cm.png";
        def.textureSlots[1].resourceRelativePath = L"Texture/Character/Zed/zed_base_tx_cm.png";
        def.spawnPositionX = 36.f;
        def.spawnPositionY = 1.f;
        def.spawnPositionZ = -6.f;
        def.spawnScale = 0.01f;
        return def;
    }

    ClientData::ChampionUiVisualDefinition MakeChampionUiVisual_ANNIE()
    {
        ClientData::ChampionUiVisualDefinition def{};
        def.key = 0xC22ADDC5u;
        def.champion = eChampion::ANNIE;
        def.loadscreen.resourceRelativePath = L"Texture/Character/Annie/annieloadscreen.dds";
        def.portrait.resourceRelativePath = L"Texture/UI/Champion/Portraits/annie_square.png";
        return def;
    }

    ClientData::ChampionUiVisualDefinition MakeChampionUiVisual_ASHE()
    {
        ClientData::ChampionUiVisualDefinition def{};
        def.key = 0x8C90933Bu;
        def.champion = eChampion::ASHE;
        def.loadscreen.resourceRelativePath = L"Texture/Character/Ashe/asheloadscreen.dds";
        def.portrait.resourceRelativePath = L"Texture/UI/Champion/Portraits/ashe_square.png";
        return def;
    }

    ClientData::ChampionUiVisualDefinition MakeChampionUiVisual_EZREAL()
    {
        ClientData::ChampionUiVisualDefinition def{};
        def.key = 0x3FD5994Fu;
        def.champion = eChampion::EZREAL;
        def.loadscreen.resourceRelativePath = L"Texture/Character/Ezreal/ezrealloadscreen.dds";
        def.portrait.resourceRelativePath = L"Texture/UI/Champion/Portraits/ezreal_square_0.png";
        return def;
    }

    ClientData::ChampionUiVisualDefinition MakeChampionUiVisual_FIORA()
    {
        ClientData::ChampionUiVisualDefinition def{};
        def.key = 0x52EC1B5Fu;
        def.champion = eChampion::FIORA;
        def.loadscreen.resourceRelativePath = L"Texture/Character/Fiora/fioraloadscreen.dds";
        def.portrait.resourceRelativePath = L"Texture/UI/Champion/Portraits/fiora_square.png";
        return def;
    }

    ClientData::ChampionUiVisualDefinition MakeChampionUiVisual_GAREN()
    {
        ClientData::ChampionUiVisualDefinition def{};
        def.key = 0x9FFA86C5u;
        def.champion = eChampion::GAREN;
        def.loadscreen.resourceRelativePath = L"Texture/Character/Garen/garenloadscreen.dds";
        def.portrait.resourceRelativePath = L"Texture/UI/Champion/Portraits/garen_square.png";
        return def;
    }

    ClientData::ChampionUiVisualDefinition MakeChampionUiVisual_IRELIA()
    {
        ClientData::ChampionUiVisualDefinition def{};
        def.key = 0xAF279D3Cu;
        def.champion = eChampion::IRELIA;
        def.loadscreen.resourceRelativePath = L"Texture/Character/Irelia/irelialoadscreen.dds";
        def.portrait.resourceRelativePath = L"Texture/UI/Champion/Portraits/irelia_square_0.png";
        return def;
    }

    ClientData::ChampionUiVisualDefinition MakeChampionUiVisual_JAX()
    {
        ClientData::ChampionUiVisualDefinition def{};
        def.key = 0x612CDD33u;
        def.champion = eChampion::JAX;
        def.loadscreen.resourceRelativePath = L"Texture/Character/Jax/jaxloadscreen_0.dds";
        def.portrait.resourceRelativePath = L"Texture/UI/Champion/Portraits/jax_square_0.png";
        return def;
    }

    ClientData::ChampionUiVisualDefinition MakeChampionUiVisual_KALISTA()
    {
        ClientData::ChampionUiVisualDefinition def{};
        def.key = 0x255F3C7Fu;
        def.champion = eChampion::KALISTA;
        def.loadscreen.resourceRelativePath = L"Texture/Character/Kalista/kalistaloadscreen.dds";
        def.portrait.resourceRelativePath = L"Texture/UI/Champion/Portraits/kalista_square_0.png";
        return def;
    }

    ClientData::ChampionUiVisualDefinition MakeChampionUiVisual_KINDRED()
    {
        ClientData::ChampionUiVisualDefinition def{};
        def.key = 0x2D48DE15u;
        def.champion = eChampion::KINDRED;
        def.loadscreen.resourceRelativePath = L"Texture/Character/Kindred/kindredloadscreen.dds";
        def.portrait.resourceRelativePath = L"Texture/UI/Champion/Portraits/kindred_square.png";
        return def;
    }

    ClientData::ChampionUiVisualDefinition MakeChampionUiVisual_LEESIN()
    {
        ClientData::ChampionUiVisualDefinition def{};
        def.key = 0x0338AE7Cu;
        def.champion = eChampion::LEESIN;
        def.loadscreen.resourceRelativePath = L"Texture/Character/LeeSin/leesinloadscreen_0.dds";
        def.portrait.resourceRelativePath = L"Texture/UI/Champion/Portraits/leesin_square_0.png";
        return def;
    }

    ClientData::ChampionUiVisualDefinition MakeChampionUiVisual_MASTERYI()
    {
        ClientData::ChampionUiVisualDefinition def{};
        def.key = 0x576EEC10u;
        def.champion = eChampion::MASTERYI;
        def.loadscreen.resourceRelativePath = L"Texture/Character/MasterYi/masteryiloadscreen.dds";
        def.portrait.resourceRelativePath = L"Texture/UI/Champion/Portraits/masteryi_square_0.png";
        return def;
    }

    ClientData::ChampionUiVisualDefinition MakeChampionUiVisual_RIVEN()
    {
        ClientData::ChampionUiVisualDefinition def{};
        def.key = 0x4222D472u;
        def.champion = eChampion::RIVEN;
        def.loadscreen.resourceRelativePath = L"Texture/Character/Riven/rivenloadscreen.dds";
        def.portrait.resourceRelativePath = L"Texture/UI/Champion/Portraits/riven_square.png";
        return def;
    }

    ClientData::ChampionUiVisualDefinition MakeChampionUiVisual_SYLAS()
    {
        ClientData::ChampionUiVisualDefinition def{};
        def.key = 0x27574CE0u;
        def.champion = eChampion::SYLAS;
        def.loadscreen.resourceRelativePath = L"Texture/Character/Sylas/sylasloadscreen.dds";
        def.portrait.resourceRelativePath = L"Texture/UI/Champion/Portraits/sylas_square.png";
        return def;
    }

    ClientData::ChampionUiVisualDefinition MakeChampionUiVisual_VIEGO()
    {
        ClientData::ChampionUiVisualDefinition def{};
        def.key = 0x11DFAB8Eu;
        def.champion = eChampion::VIEGO;
        def.loadscreen.resourceRelativePath = L"Texture/Character/Viego/viegoloadscreen.dds";
        def.portrait.resourceRelativePath = L"Texture/UI/Champion/Portraits/viego_square.png";
        return def;
    }

    ClientData::ChampionUiVisualDefinition MakeChampionUiVisual_YASUO()
    {
        ClientData::ChampionUiVisualDefinition def{};
        def.key = 0xD7966ECFu;
        def.champion = eChampion::YASUO;
        def.loadscreen.resourceRelativePath = L"Texture/Character/Yasuo/yasuoloadscreen.dds";
        def.portrait.resourceRelativePath = L"Texture/UI/Champion/Portraits/yasuo_square.png";
        return def;
    }

    ClientData::ChampionUiVisualDefinition MakeChampionUiVisual_YONE()
    {
        ClientData::ChampionUiVisualDefinition def{};
        def.key = 0xFA5701CDu;
        def.champion = eChampion::YONE;
        def.loadscreen.resourceRelativePath = L"Texture/Character/Yone/yoneloadscreen.dds";
        def.portrait.resourceRelativePath = L"Texture/UI/Champion/Portraits/yone_square.png";
        return def;
    }

    ClientData::ChampionUiVisualDefinition MakeChampionUiVisual_ZED()
    {
        ClientData::ChampionUiVisualDefinition def{};
        def.key = 0xA0081CA3u;
        def.champion = eChampion::ZED;
        def.loadscreen.resourceRelativePath = L"Texture/Character/Zed/zedloadscreen.dds";
        def.portrait.resourceRelativePath = L"Texture/UI/Champion/Portraits/zed_square_0.png";
        return def;
    }

    ClientData::StructureVisualDefinition MakeStructureVisual_STRUCTURE_INHIBITOR_BLUE()
    {
        ClientData::StructureVisualDefinition def{};
        def.key = 0xD545E85Cu;
        def.kind = Winters::Map::eObjectKind::Structure_Inhibitor;
        def.team = eTeam::Blue;
        def.mesh.resourceRelativePath = "Texture/Object/Inhibitor/inhibitor_textured.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.submeshStateCount = static_cast<u8_t>(2u);
        def.submeshStates[0].submeshIndex = 0u;
        def.submeshStates[0].bVisibleWhenDestroyed = false;
        def.submeshStates[0].bVisibleWhenAlive = true;
        def.submeshStates[1].submeshIndex = 1u;
        def.submeshStates[1].bVisibleWhenDestroyed = true;
        def.submeshStates[1].bVisibleWhenAlive = false;
        return def;
    }

    ClientData::StructureVisualDefinition MakeStructureVisual_STRUCTURE_INHIBITOR_RED()
    {
        ClientData::StructureVisualDefinition def{};
        def.key = 0x11D7EA57u;
        def.kind = Winters::Map::eObjectKind::Structure_Inhibitor;
        def.team = eTeam::Red;
        def.mesh.resourceRelativePath = "Texture/Object/Inhibitor/inhibitor_red_textured.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.submeshStateCount = static_cast<u8_t>(2u);
        def.submeshStates[0].submeshIndex = 0u;
        def.submeshStates[0].bVisibleWhenDestroyed = false;
        def.submeshStates[0].bVisibleWhenAlive = true;
        def.submeshStates[1].submeshIndex = 1u;
        def.submeshStates[1].bVisibleWhenDestroyed = true;
        def.submeshStates[1].bVisibleWhenAlive = false;
        return def;
    }

    ClientData::StructureVisualDefinition MakeStructureVisual_STRUCTURE_NEXUS_BLUE()
    {
        ClientData::StructureVisualDefinition def{};
        def.key = 0x0F209297u;
        def.kind = Winters::Map::eObjectKind::Structure_Nexus;
        def.team = eTeam::Blue;
        def.mesh.resourceRelativePath = "Texture/Object/Nexus/nexus_textured.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.submeshStateCount = static_cast<u8_t>(2u);
        def.submeshStates[0].submeshIndex = 0u;
        def.submeshStates[0].bVisibleWhenDestroyed = true;
        def.submeshStates[0].bVisibleWhenAlive = false;
        def.submeshStates[1].submeshIndex = 1u;
        def.submeshStates[1].bVisibleWhenDestroyed = false;
        def.submeshStates[1].bVisibleWhenAlive = true;
        return def;
    }

    ClientData::StructureVisualDefinition MakeStructureVisual_STRUCTURE_NEXUS_RED()
    {
        ClientData::StructureVisualDefinition def{};
        def.key = 0x393F5242u;
        def.kind = Winters::Map::eObjectKind::Structure_Nexus;
        def.team = eTeam::Red;
        def.mesh.resourceRelativePath = "Texture/Object/Nexus/nexus_red_textured.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.submeshStateCount = static_cast<u8_t>(2u);
        def.submeshStates[0].submeshIndex = 0u;
        def.submeshStates[0].bVisibleWhenDestroyed = true;
        def.submeshStates[0].bVisibleWhenAlive = false;
        def.submeshStates[1].submeshIndex = 1u;
        def.submeshStates[1].bVisibleWhenDestroyed = false;
        def.submeshStates[1].bVisibleWhenAlive = true;
        return def;
    }

    ClientData::StructureVisualDefinition MakeStructureVisual_STRUCTURE_TURRET_BLUE()
    {
        ClientData::StructureVisualDefinition def{};
        def.key = 0xC3D20E18u;
        def.kind = Winters::Map::eObjectKind::Structure_Turret;
        def.team = eTeam::Blue;
        def.mesh.resourceRelativePath = "Texture/Object/Turret/turret_textured.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.submeshStateCount = static_cast<u8_t>(8u);
        def.submeshStates[0].submeshIndex = 0u;
        def.submeshStates[0].bVisibleWhenDestroyed = false;
        def.submeshStates[0].bVisibleWhenAlive = true;
        def.submeshStates[1].submeshIndex = 1u;
        def.submeshStates[1].bVisibleWhenDestroyed = false;
        def.submeshStates[1].bVisibleWhenAlive = false;
        def.submeshStates[2].submeshIndex = 2u;
        def.submeshStates[2].bVisibleWhenDestroyed = false;
        def.submeshStates[2].bVisibleWhenAlive = false;
        def.submeshStates[3].submeshIndex = 3u;
        def.submeshStates[3].bVisibleWhenDestroyed = true;
        def.submeshStates[3].bVisibleWhenAlive = false;
        def.submeshStates[4].submeshIndex = 4u;
        def.submeshStates[4].bVisibleWhenDestroyed = false;
        def.submeshStates[4].bVisibleWhenAlive = false;
        def.submeshStates[5].submeshIndex = 5u;
        def.submeshStates[5].bVisibleWhenDestroyed = false;
        def.submeshStates[5].bVisibleWhenAlive = false;
        def.submeshStates[6].submeshIndex = 6u;
        def.submeshStates[6].bVisibleWhenDestroyed = false;
        def.submeshStates[6].bVisibleWhenAlive = false;
        def.submeshStates[7].submeshIndex = 7u;
        def.submeshStates[7].bVisibleWhenDestroyed = false;
        def.submeshStates[7].bVisibleWhenAlive = false;
        return def;
    }

    ClientData::StructureVisualDefinition MakeStructureVisual_STRUCTURE_TURRET_RED()
    {
        ClientData::StructureVisualDefinition def{};
        def.key = 0x1A4C3A43u;
        def.kind = Winters::Map::eObjectKind::Structure_Turret;
        def.team = eTeam::Red;
        def.mesh.resourceRelativePath = "Texture/Object/Turret/turret_red_textured.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.submeshStateCount = static_cast<u8_t>(8u);
        def.submeshStates[0].submeshIndex = 0u;
        def.submeshStates[0].bVisibleWhenDestroyed = false;
        def.submeshStates[0].bVisibleWhenAlive = true;
        def.submeshStates[1].submeshIndex = 1u;
        def.submeshStates[1].bVisibleWhenDestroyed = false;
        def.submeshStates[1].bVisibleWhenAlive = false;
        def.submeshStates[2].submeshIndex = 2u;
        def.submeshStates[2].bVisibleWhenDestroyed = false;
        def.submeshStates[2].bVisibleWhenAlive = false;
        def.submeshStates[3].submeshIndex = 3u;
        def.submeshStates[3].bVisibleWhenDestroyed = true;
        def.submeshStates[3].bVisibleWhenAlive = false;
        def.submeshStates[4].submeshIndex = 4u;
        def.submeshStates[4].bVisibleWhenDestroyed = false;
        def.submeshStates[4].bVisibleWhenAlive = false;
        def.submeshStates[5].submeshIndex = 5u;
        def.submeshStates[5].bVisibleWhenDestroyed = false;
        def.submeshStates[5].bVisibleWhenAlive = false;
        def.submeshStates[6].submeshIndex = 6u;
        def.submeshStates[6].bVisibleWhenDestroyed = false;
        def.submeshStates[6].bVisibleWhenAlive = false;
        def.submeshStates[7].submeshIndex = 7u;
        def.submeshStates[7].bVisibleWhenDestroyed = false;
        def.submeshStates[7].bVisibleWhenAlive = false;
        return def;
    }

    ClientData::JungleVisualDefinition MakeJungleVisual_JUNGLE_BARON()
    {
        ClientData::JungleVisualDefinition def{};
        def.key = 0x051D4582u;
        def.subKind = 0u;
        def.mesh.resourceRelativePath = "Texture/Object/Jungle/Baron/baron_textured.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.textureOverrideCount = static_cast<u8_t>(3u);
        def.textureOverrides[0].meshIndex = 0u;
        def.textureOverrides[0].resourceRelativePath = L"Texture/Object/Jungle/Baron/sru_baron_tx_cm.png";
        def.textureOverrides[1].meshIndex = 1u;
        def.textureOverrides[1].resourceRelativePath = L"Texture/Object/Jungle/Baron/sru_baron_tx_cm.png";
        def.textureOverrides[2].meshIndex = 2u;
        def.textureOverrides[2].resourceRelativePath = L"Texture/Object/Jungle/Baron/baron_base_tx_cm.png";
        return def;
    }

    ClientData::JungleVisualDefinition MakeJungleVisual_JUNGLE_DRAGON()
    {
        ClientData::JungleVisualDefinition def{};
        def.key = 0x7239E7E1u;
        def.subKind = 1u;
        def.mesh.resourceRelativePath = "Texture/Object/Jungle/Dragon/air/dragon_air_textured.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.visualScaleMultiplier = 1.5f;
        def.textureOverrideCount = static_cast<u8_t>(0u);
        return def;
    }

    ClientData::JungleVisualDefinition MakeJungleVisual_JUNGLE_BLUE()
    {
        ClientData::JungleVisualDefinition def{};
        def.key = 0x66697226u;
        def.subKind = 2u;
        def.mesh.resourceRelativePath = "Texture/Object/Jungle/Blue/blue_textured.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.textureOverrideCount = static_cast<u8_t>(0u);
        return def;
    }

    ClientData::JungleVisualDefinition MakeJungleVisual_JUNGLE_RED()
    {
        ClientData::JungleVisualDefinition def{};
        def.key = 0xA873C295u;
        def.subKind = 3u;
        def.mesh.resourceRelativePath = "Texture/Object/Jungle/Red/red_textured.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.textureOverrideCount = static_cast<u8_t>(0u);
        return def;
    }

    ClientData::JungleVisualDefinition MakeJungleVisual_JUNGLE_KRUG()
    {
        ClientData::JungleVisualDefinition def{};
        def.key = 0xAEBC8BC3u;
        def.subKind = 4u;
        def.mesh.resourceRelativePath = "Texture/Object/Jungle/Krug/krug_textured.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.textureOverrideCount = static_cast<u8_t>(0u);
        return def;
    }

    ClientData::JungleVisualDefinition MakeJungleVisual_JUNGLE_GROMP()
    {
        ClientData::JungleVisualDefinition def{};
        def.key = 0x0EA09319u;
        def.subKind = 5u;
        def.mesh.resourceRelativePath = "Texture/Object/Jungle/Gromp/gromp_textured.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.textureOverrideCount = static_cast<u8_t>(0u);
        return def;
    }

    ClientData::JungleVisualDefinition MakeJungleVisual_JUNGLE_WOLF()
    {
        ClientData::JungleVisualDefinition def{};
        def.key = 0xA999EC72u;
        def.subKind = 6u;
        def.mesh.resourceRelativePath = "Texture/Object/Jungle/Wolf/wolf_textured.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.textureOverrideCount = static_cast<u8_t>(0u);
        return def;
    }

    ClientData::JungleVisualDefinition MakeJungleVisual_JUNGLE_RAZORBEAK()
    {
        ClientData::JungleVisualDefinition def{};
        def.key = 0x84EF25EFu;
        def.subKind = 7u;
        def.mesh.resourceRelativePath = "Texture/Object/Jungle/Razorbeak/razorbeak_textured.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.textureOverrideCount = static_cast<u8_t>(0u);
        return def;
    }

    ClientData::JungleVisualDefinition MakeJungleVisual_JUNGLE_RAZORBEAK_MINI()
    {
        ClientData::JungleVisualDefinition def{};
        def.key = 0xFDBFD9BFu;
        def.subKind = 8u;
        def.mesh.resourceRelativePath = "Texture/Object/Jungle/RazorbeakMini/razorbeakmini_textured.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.textureOverrideCount = static_cast<u8_t>(0u);
        return def;
    }

    ClientData::JungleVisualDefinition MakeJungleVisual_JUNGLE_WOLF_MINI()
    {
        ClientData::JungleVisualDefinition def{};
        def.key = 0x2012BBA8u;
        def.subKind = 9u;
        def.mesh.resourceRelativePath = "Texture/Object/Jungle/WolfMini/wolfmini_textured.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.textureOverrideCount = static_cast<u8_t>(0u);
        return def;
    }

    ClientData::JungleVisualDefinition MakeJungleVisual_JUNGLE_KRUG_MINI()
    {
        ClientData::JungleVisualDefinition def{};
        def.key = 0x98A5C2D3u;
        def.subKind = 10u;
        def.mesh.resourceRelativePath = "Texture/Object/Jungle/KrugMini/krugmini_textured.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.textureOverrideCount = static_cast<u8_t>(0u);
        return def;
    }

    ClientData::MinionVisualDefinition MakeMinionVisual_MINION_BLUE_MELEE()
    {
        ClientData::MinionVisualDefinition def{};
        def.key = 0x67787399u;
        def.type = 0u;
        def.team = 0u;
        def.mesh.resourceRelativePath = "Texture/Object/Minion_Order/Melee/order_melee_textured.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        return def;
    }

    ClientData::MinionVisualDefinition MakeMinionVisual_MINION_BLUE_RANGED()
    {
        ClientData::MinionVisualDefinition def{};
        def.key = 0x2EFD12B8u;
        def.type = 1u;
        def.team = 0u;
        def.mesh.resourceRelativePath = "Texture/Object/Minion_Order/Ranged/order_ranged_textured.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        return def;
    }

    ClientData::MinionVisualDefinition MakeMinionVisual_MINION_BLUE_SIEGE()
    {
        ClientData::MinionVisualDefinition def{};
        def.key = 0xF133060Au;
        def.type = 2u;
        def.team = 0u;
        def.mesh.resourceRelativePath = "Texture/Object/Minion_Order/Siege/order_siege_textured.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.visualScaleMultiplier = 1.5f;
        return def;
    }

    ClientData::MinionVisualDefinition MakeMinionVisual_MINION_BLUE_SUPER()
    {
        ClientData::MinionVisualDefinition def{};
        def.key = 0xE071C02Au;
        def.type = 3u;
        def.team = 0u;
        def.mesh.resourceRelativePath = "Texture/Object/Minion_Order/Super/order_super_textured.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        return def;
    }

    ClientData::MinionVisualDefinition MakeMinionVisual_MINION_BLUE_TIBBERS()
    {
        ClientData::MinionVisualDefinition def{};
        def.key = 0x3129CB48u;
        def.type = 4u;
        def.team = 0u;
        def.mesh.resourceRelativePath = "Texture/Character/Annie/tibber.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.textureAllMeshes.resourceRelativePath = L"Texture/Character/Annie/tibber_base.png";
        return def;
    }

    ClientData::MinionVisualDefinition MakeMinionVisual_MINION_RED_MELEE()
    {
        ClientData::MinionVisualDefinition def{};
        def.key = 0x75F928DCu;
        def.type = 0u;
        def.team = 1u;
        def.mesh.resourceRelativePath = "Texture/Object/Minion_Chaos/melee/chaos_melee_textured.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        return def;
    }

    ClientData::MinionVisualDefinition MakeMinionVisual_MINION_RED_RANGED()
    {
        ClientData::MinionVisualDefinition def{};
        def.key = 0x3ABEFFE3u;
        def.type = 1u;
        def.team = 1u;
        def.mesh.resourceRelativePath = "Texture/Object/Minion_Chaos/ranged/chaos_ranged_textured.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        return def;
    }

    ClientData::MinionVisualDefinition MakeMinionVisual_MINION_RED_SIEGE()
    {
        ClientData::MinionVisualDefinition def{};
        def.key = 0x3B45B777u;
        def.type = 2u;
        def.team = 1u;
        def.mesh.resourceRelativePath = "Texture/Object/Minion_Chaos/siege/chaos_siege_textured.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.visualScaleMultiplier = 1.5f;
        return def;
    }

    ClientData::MinionVisualDefinition MakeMinionVisual_MINION_RED_SUPER()
    {
        ClientData::MinionVisualDefinition def{};
        def.key = 0xB83A44BBu;
        def.type = 3u;
        def.team = 1u;
        def.mesh.resourceRelativePath = "Texture/Object/Minion_Chaos/super/chaos_super_textured.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        return def;
    }

    ClientData::MinionVisualDefinition MakeMinionVisual_MINION_RED_TIBBERS()
    {
        ClientData::MinionVisualDefinition def{};
        def.key = 0xB29C9655u;
        def.type = 4u;
        def.team = 1u;
        def.mesh.resourceRelativePath = "Texture/Character/Annie/tibber.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.textureAllMeshes.resourceRelativePath = L"Texture/Character/Annie/tibber_base.png";
        return def;
    }

    ClientData::AmbientPropVisualDefinition MakeAmbientPropVisual_AMBIENT_SRU_BIRD()
    {
        ClientData::AmbientPropVisualDefinition def{};
        def.key = 0xA85A9FD1u;
        def.kind = 0u;
        def.mesh.resourceRelativePath = "Texture/MAP/Map11_Rebuild/cooked/ambient/sru_bird/sru_bird.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.idleAnimation = "sru_bird_idle_tree1";
        return def;
    }

    ClientData::AmbientPropVisualDefinition MakeAmbientPropVisual_AMBIENT_SRU_DUCK()
    {
        ClientData::AmbientPropVisualDefinition def{};
        def.key = 0x34B3E9D7u;
        def.kind = 1u;
        def.mesh.resourceRelativePath = "Texture/MAP/Map11_Rebuild/cooked/ambient/sru_duck/sru_duck.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.idleAnimation = "sru_duck_idle1";
        return def;
    }

    ClientData::AmbientPropVisualDefinition MakeAmbientPropVisual_AMBIENT_CHEMTECH_FIREFLY()
    {
        ClientData::AmbientPropVisualDefinition def{};
        def.key = 0x1B263B6Cu;
        def.kind = 2u;
        def.mesh.resourceRelativePath = "Texture/MAP/Map11_Rebuild/cooked/ambient/chemtech_firefly_animated/chemtech_firefly_animated.wmesh";
        def.shader.runtimePath = L"Shaders/Mesh3D.hlsl";
        def.idleAnimation = "firefly_fairy_idle";
        return def;
    }

    ClientData::MapRuntimeVisualDefinition MakeMapRuntimeVisual()
    {
        ClientData::MapRuntimeVisualDefinition def{};
        def.baseMapMesh.resourceRelativePath = "Texture/MAP/output/sr_base_flip.wmesh";
        def.baseMapSurface.resourceRelativePath = L"Texture/MAP/output/sr_base_flip_surface.wmesh";
        def.fullLayerMapMesh.resourceRelativePath = "Texture/MAP/Map11_Rebuild/cooked/sr_base_flip_full_layers.wmesh";
        def.fullLayerMapSurface.resourceRelativePath = L"Texture/MAP/Map11_Rebuild/cooked/sr_base_flip_full_layers.wmesh";
        def.brushVolumeCsv.resourceRelativePath = L"Texture/MAP/Map11_Rebuild/manifest/map11_brush_volumes.csv";
        def.brushVolumeBinary.resourceRelativePath = L"Texture/MAP/Map11_Rebuild/cooked/map11_brush_volumes.wbrush";
        def.attackRangeTexture.resourceRelativePath = L"Texture/UI/UI_AttackRange.png";
        return def;
    }

    ClientData::FxMeshPreloadVisualDefinition MakeFxMeshPreloadVisual_FX_ASHE_BASE_ARROW()
    {
        ClientData::FxMeshPreloadVisualDefinition def{};
        def.key = 0xB9B8A25Du;
        def.mesh.resourceRelativePath = "Texture/Character/Ashe/particles/fbx/ashe_base_aa_arrow.fbx";
        def.texture.resourceRelativePath = L"Texture/Character/Ashe/particles/ashe_base_aa_arrowtext.png";
        return def;
    }

    ClientData::FxMeshPreloadVisualDefinition MakeFxMeshPreloadVisual_FX_IRELIA_E_BEAM()
    {
        ClientData::FxMeshPreloadVisualDefinition def{};
        def.key = 0x51D1BEACu;
        def.mesh.resourceRelativePath = "Texture/FX/Irelia/fbx/irelia_base_e_beam.fbx";
        def.texture.resourceRelativePath = L"Texture/FX/Irelia/irelia_base_e_beam_mult.png";
        return def;
    }

    ClientData::FxMeshPreloadVisualDefinition MakeFxMeshPreloadVisual_FX_IRELIA_E_BLADE()
    {
        ClientData::FxMeshPreloadVisualDefinition def{};
        def.key = 0x1F82AB6Fu;
        def.mesh.resourceRelativePath = "Texture/FX/Irelia/fbx/irelia_base_e_blade.fbx";
        def.texture.resourceRelativePath = L"Texture/FX/Irelia/irelia_base_blades_passive_4_texture.png";
        return def;
    }

    ClientData::FxMeshPreloadVisualDefinition MakeFxMeshPreloadVisual_FX_KALISTA_E_SPEAR_HOLD()
    {
        ClientData::FxMeshPreloadVisualDefinition def{};
        def.key = 0xA03C9307u;
        def.mesh.resourceRelativePath = "Texture/FX/Kalista/fbx/kalista_base_e_spear_hold.fbx";
        def.texture.resourceRelativePath = L"Texture/FX/Kalista/kalista_base_e_spear_glow.png";
        return def;
    }

    ClientData::FxMeshPreloadVisualDefinition MakeFxMeshPreloadVisual_FX_KALISTA_Q_SPEAR()
    {
        ClientData::FxMeshPreloadVisualDefinition def{};
        def.key = 0x8D288869u;
        def.mesh.resourceRelativePath = "Texture/FX/Kalista/fbx/kalista_base_q_mis_spear.fbx";
        def.texture.resourceRelativePath = L"Texture/FX/Kalista/kalista_base_q_mis_glow_color.png";
        return def;
    }

    ClientData::FxMeshPreloadVisualDefinition MakeFxMeshPreloadVisual_FX_YASUO_W_WINDWALL()
    {
        ClientData::FxMeshPreloadVisualDefinition def{};
        def.key = 0xD1115A64u;
        def.mesh.resourceRelativePath = "Texture/FX/Yasuo/fbx/yasuo_w_windwall_mesh.fbx";
        def.texture.resourceRelativePath = L"Texture/FX/Yasuo/color_yasuo_w_windwall_dust.png";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_1001()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 1001u;
        def.price = 300u;
        def.stats.flatMoveSpeed = 25.f;
        def.displayName = "Boots";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_1004()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 1004u;
        def.price = 200u;
        def.displayName = "Faerie Charm";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_1006()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 1006u;
        def.price = 300u;
        def.displayName = "Rejuvenation Bead";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_1011()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 1011u;
        def.price = 900u;
        def.stats.flatHealth = 350.f;
        def.displayName = "Giant's Belt";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_1018()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 1018u;
        def.price = 600u;
        def.stats.critChance = 0.15f;
        def.displayName = "Cloak of Agility";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_1026()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 1026u;
        def.price = 850u;
        def.stats.flatAp = 45.f;
        def.displayName = "Blasting Wand";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_1027()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 1027u;
        def.price = 300u;
        def.stats.flatMana = 300.f;
        def.displayName = "Sapphire Crystal";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_1028()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 1028u;
        def.price = 400u;
        def.stats.flatHealth = 150.f;
        def.displayName = "Ruby Crystal";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_1029()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 1029u;
        def.price = 300u;
        def.stats.flatArmor = 15.f;
        def.displayName = "Cloth Armor";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_1031()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 1031u;
        def.price = 800u;
        def.stats.flatArmor = 40.f;
        def.displayName = "Chain Vest";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_1033()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 1033u;
        def.price = 400u;
        def.stats.flatMr = 20.f;
        def.displayName = "Null-Magic Mantle";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_1036()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 1036u;
        def.price = 350u;
        def.stats.flatAd = 10.f;
        def.displayName = "Long Sword";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_1037()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 1037u;
        def.price = 875u;
        def.stats.flatAd = 25.f;
        def.displayName = "Pickaxe";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_1038()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 1038u;
        def.price = 1300u;
        def.stats.flatAd = 40.f;
        def.displayName = "B. F. Sword";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_1042()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 1042u;
        def.price = 250u;
        def.stats.bonusAttackSpeed = 0.1f;
        def.displayName = "Dagger";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_1043()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 1043u;
        def.price = 700u;
        def.stats.bonusAttackSpeed = 0.15f;
        def.displayName = "Recurve Bow";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_1052()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 1052u;
        def.price = 400u;
        def.stats.flatAp = 20.f;
        def.displayName = "Amplifying Tome";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_1053()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 1053u;
        def.price = 900u;
        def.stats.flatAd = 15.f;
        def.stats.lifeSteal = 0.07f;
        def.displayName = "Vampiric Scepter";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_1054()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 1054u;
        def.price = 450u;
        def.stats.flatHealth = 110.f;
        def.displayName = "Doran's Shield";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_1055()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 1055u;
        def.price = 450u;
        def.stats.flatAd = 10.f;
        def.stats.flatHealth = 80.f;
        def.displayName = "Doran's Blade";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_1056()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 1056u;
        def.price = 400u;
        def.stats.flatAp = 18.f;
        def.stats.flatHealth = 90.f;
        def.displayName = "Doran's Ring";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_1057()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 1057u;
        def.price = 850u;
        def.stats.flatMr = 45.f;
        def.displayName = "Negatron Cloak";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_1058()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 1058u;
        def.price = 1200u;
        def.stats.flatAp = 65.f;
        def.displayName = "Needlessly Large Rod";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_1082()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 1082u;
        def.price = 350u;
        def.stats.flatAp = 15.f;
        def.stats.flatHealth = 50.f;
        def.displayName = "Dark Seal";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_1083()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 1083u;
        def.price = 450u;
        def.stats.flatAd = 7.f;
        def.displayName = "Cull";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_1086()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 1086u;
        def.price = 400u;
        def.stats.flatAd = 8.f;
        def.stats.bonusAttackSpeed = 0.15f;
        def.displayName = "Doran's Bow";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_1101()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 1101u;
        def.price = 450u;
        def.displayName = "Scorchclaw Pup";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_1102()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 1102u;
        def.price = 450u;
        def.displayName = "Gustwalker Hatchling";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_1103()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 1103u;
        def.price = 450u;
        def.displayName = "Mosstomper Seedling";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_1105()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 1105u;
        def.price = 450u;
        def.displayName = "Mosstomper Seedling";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_1106()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 1106u;
        def.price = 450u;
        def.displayName = "Gustwalker Hatchling";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_1107()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 1107u;
        def.price = 450u;
        def.displayName = "Scorchclaw Pup";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_1120()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 1120u;
        def.price = 450u;
        def.stats.flatHealth = 150.f;
        def.stats.flatArmor = 8.f;
        def.stats.flatMr = 8.f;
        def.displayName = "Doran's Helm";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_2003()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 2003u;
        def.price = 50u;
        def.displayName = "Health Potion";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_2019()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 2019u;
        def.price = 1100u;
        def.stats.flatAd = 15.f;
        def.stats.flatArmor = 30.f;
        def.displayName = "Steel Sigil";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_2020()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 2020u;
        def.price = 1337u;
        def.stats.flatAd = 25.f;
        def.stats.abilityHaste = 10.f;
        def.stats.lethality = 5.f;
        def.displayName = "The Brutalizer";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_2021()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 2021u;
        def.price = 1150u;
        def.stats.flatAd = 15.f;
        def.stats.flatHealth = 250.f;
        def.displayName = "Tunneler";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_2022()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 2022u;
        def.price = 250u;
        def.stats.abilityHaste = 5.f;
        def.displayName = "Glowing Mote";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_2031()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 2031u;
        def.price = 150u;
        def.displayName = "Refillable Potion";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_2051()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 2051u;
        def.price = 950u;
        def.stats.flatHealth = 150.f;
        def.displayName = "Guardian's Horn";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_2055()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 2055u;
        def.price = 75u;
        def.displayName = "Control Ward";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_2065()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 2065u;
        def.price = 2200u;
        def.stats.flatAp = 50.f;
        def.stats.abilityHaste = 15.f;
        def.stats.percentMoveSpeed = 0.04f;
        def.displayName = "Shurelya's Battlesong";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_2138()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 2138u;
        def.price = 500u;
        def.displayName = "Elixir of Iron";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_2139()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 2139u;
        def.price = 500u;
        def.displayName = "Elixir of Sorcery";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_2140()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 2140u;
        def.price = 500u;
        def.displayName = "Elixir of Wrath";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_2141()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 2141u;
        def.price = 300u;
        def.displayName = "Cappa Juice";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_2420()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 2420u;
        def.price = 1600u;
        def.stats.flatAp = 40.f;
        def.stats.flatArmor = 25.f;
        def.displayName = "Seeker's Armguard";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_2501()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 2501u;
        def.price = 3300u;
        def.stats.flatAd = 30.f;
        def.stats.flatHealth = 550.f;
        def.displayName = "Overlord's Bloodmail";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_2502()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 2502u;
        def.price = 2800u;
        def.stats.flatHealth = 400.f;
        def.stats.flatArmor = 50.f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Unending Despair";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_2503()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 2503u;
        def.price = 2800u;
        def.stats.flatAp = 80.f;
        def.stats.flatMana = 600.f;
        def.stats.abilityHaste = 20.f;
        def.displayName = "Blackfire Torch";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_2504()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 2504u;
        def.price = 2900u;
        def.stats.flatHealth = 400.f;
        def.stats.flatMr = 80.f;
        def.displayName = "Kaenic Rookern";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_2508()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 2508u;
        def.price = 900u;
        def.stats.flatAp = 30.f;
        def.displayName = "Fated Ashes";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_2510()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 2510u;
        def.price = 3100u;
        def.stats.flatAp = 60.f;
        def.stats.flatHealth = 300.f;
        def.stats.bonusAttackSpeed = 0.2f;
        def.stats.abilityHaste = 20.f;
        def.displayName = "Dusk and Dawn";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_2512()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 2512u;
        def.price = 2650u;
        def.stats.bonusAttackSpeed = 0.45f;
        def.stats.critChance = 0.25f;
        def.stats.percentMoveSpeed = 0.04f;
        def.displayName = "Fiendhunter Bolts";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_2517()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 2517u;
        def.price = 3100u;
        def.stats.flatAd = 65.f;
        def.displayName = "Endless Hunger";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_2520()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 2520u;
        def.price = 3200u;
        def.stats.flatAd = 55.f;
        def.stats.abilityHaste = 15.f;
        def.stats.lethality = 22.f;
        def.displayName = "Bastionbreaker";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_2522()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 2522u;
        def.price = 2800u;
        def.stats.flatAp = 90.f;
        def.stats.flatMana = 300.f;
        def.stats.abilityHaste = 10.f;
        def.displayName = "Actualizer";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_2523()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 2523u;
        def.price = 2800u;
        def.stats.flatAd = 55.f;
        def.stats.critChance = 0.25f;
        def.displayName = "Hexoptics C44";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_2524()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 2524u;
        def.price = 2300u;
        def.stats.flatHealth = 200.f;
        def.stats.flatArmor = 20.f;
        def.stats.flatMr = 20.f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Bandlepipes";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_2525()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 2525u;
        def.price = 2600u;
        def.stats.flatHealth = 600.f;
        def.stats.abilityHaste = 20.f;
        def.displayName = "Protoplasm Harness";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_2526()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 2526u;
        def.price = 2250u;
        def.stats.flatHealth = 200.f;
        def.stats.flatMana = 300.f;
        def.displayName = "Whispering Circlet";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3003()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3003u;
        def.price = 2900u;
        def.stats.flatAp = 70.f;
        def.stats.flatMana = 600.f;
        def.stats.abilityHaste = 25.f;
        def.displayName = "Archangel's Staff";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3004()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3004u;
        def.price = 2900u;
        def.stats.flatAd = 35.f;
        def.stats.flatMana = 500.f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Manamune";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3006()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3006u;
        def.price = 1100u;
        def.stats.bonusAttackSpeed = 0.25f;
        def.stats.flatMoveSpeed = 45.f;
        def.displayName = "Berserker's Greaves";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3008()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3008u;
        def.price = 1000u;
        def.stats.flatMoveSpeed = 45.f;
        def.displayName = "Gluttonous Greaves";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3009()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3009u;
        def.price = 1000u;
        def.stats.flatMoveSpeed = 55.f;
        def.displayName = "Boots of Swiftness";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3020()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3020u;
        def.price = 1100u;
        def.stats.flatMagicPen = 12.f;
        def.stats.flatMoveSpeed = 45.f;
        def.displayName = "Sorcerer's Shoes";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3024()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3024u;
        def.price = 900u;
        def.stats.flatMana = 300.f;
        def.stats.flatArmor = 25.f;
        def.stats.abilityHaste = 10.f;
        def.displayName = "Glacial Buckler";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3026()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3026u;
        def.price = 3200u;
        def.stats.flatAd = 55.f;
        def.stats.flatArmor = 45.f;
        def.displayName = "Guardian Angel";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3031()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3031u;
        def.price = 3500u;
        def.stats.flatAd = 75.f;
        def.stats.critChance = 0.25f;
        def.stats.critDamageBonus = 0.3f;
        def.displayName = "Infinity Edge";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3032()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3032u;
        def.price = 3100u;
        def.stats.flatAd = 50.f;
        def.stats.bonusAttackSpeed = 0.4f;
        def.displayName = "Yun Tal Wildarrows";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3033()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3033u;
        def.price = 3000u;
        def.stats.flatAd = 35.f;
        def.stats.critChance = 0.25f;
        def.stats.armorPenPercent = 0.3f;
        def.displayName = "Mortal Reminder";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3035()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3035u;
        def.price = 1450u;
        def.stats.flatAd = 20.f;
        def.stats.armorPenPercent = 0.18f;
        def.displayName = "Last Whisper";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3036()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3036u;
        def.price = 3300u;
        def.stats.flatAd = 35.f;
        def.stats.critChance = 0.25f;
        def.stats.armorPenPercent = 0.35f;
        def.displayName = "Lord Dominik's Regards";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3041()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3041u;
        def.price = 1500u;
        def.stats.flatAp = 20.f;
        def.stats.flatHealth = 100.f;
        def.displayName = "Mejai's Soulstealer";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3042()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3042u;
        def.price = 2900u;
        def.stats.flatAd = 35.f;
        def.stats.flatMana = 1000.f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Muramana";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3044()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3044u;
        def.price = 1100u;
        def.stats.flatAd = 15.f;
        def.stats.flatHealth = 200.f;
        def.displayName = "Phage";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3046()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3046u;
        def.price = 2650u;
        def.stats.bonusAttackSpeed = 0.65f;
        def.stats.critChance = 0.25f;
        def.stats.percentMoveSpeed = 0.1f;
        def.displayName = "Phantom Dancer";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3047()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3047u;
        def.price = 1200u;
        def.stats.flatArmor = 25.f;
        def.stats.flatMoveSpeed = 45.f;
        def.displayName = "Plated Steelcaps";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3050()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3050u;
        def.price = 2200u;
        def.stats.flatHealth = 300.f;
        def.stats.flatArmor = 25.f;
        def.stats.flatMr = 25.f;
        def.stats.abilityHaste = 10.f;
        def.displayName = "Zeke's Convergence";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3051()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3051u;
        def.price = 1200u;
        def.stats.flatAd = 20.f;
        def.stats.bonusAttackSpeed = 0.2f;
        def.displayName = "Hearthbound Axe";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3053()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3053u;
        def.price = 3200u;
        def.stats.flatHealth = 400.f;
        def.displayName = "Sterak's Gage";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3057()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3057u;
        def.price = 900u;
        def.stats.abilityHaste = 10.f;
        def.displayName = "Sheen";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3065()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3065u;
        def.price = 2700u;
        def.stats.flatHealth = 400.f;
        def.stats.flatMr = 50.f;
        def.stats.abilityHaste = 10.f;
        def.displayName = "Spirit Visage";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3066()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3066u;
        def.price = 800u;
        def.stats.flatHealth = 200.f;
        def.stats.percentMoveSpeed = 0.04f;
        def.displayName = "Winged Moonplate";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3067()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3067u;
        def.price = 800u;
        def.stats.flatHealth = 200.f;
        def.stats.abilityHaste = 10.f;
        def.displayName = "Kindlegem";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3068()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3068u;
        def.price = 2700u;
        def.stats.flatHealth = 350.f;
        def.stats.flatArmor = 50.f;
        def.stats.abilityHaste = 10.f;
        def.displayName = "Sunfire Aegis";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3070()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3070u;
        def.price = 400u;
        def.stats.flatMana = 240.f;
        def.displayName = "Tear of the Goddess";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3071()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3071u;
        def.price = 3000u;
        def.stats.flatAd = 40.f;
        def.stats.flatHealth = 400.f;
        def.stats.abilityHaste = 20.f;
        def.displayName = "Black Cleaver";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3072()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3072u;
        def.price = 3400u;
        def.stats.flatAd = 80.f;
        def.stats.lifeSteal = 0.15f;
        def.displayName = "Bloodthirster";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3073()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3073u;
        def.price = 3000u;
        def.stats.flatAd = 40.f;
        def.stats.flatHealth = 450.f;
        def.stats.bonusAttackSpeed = 0.2f;
        def.displayName = "Experimental Hexplate";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3074()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3074u;
        def.price = 3300u;
        def.stats.flatAd = 65.f;
        def.stats.abilityHaste = 15.f;
        def.stats.lifeSteal = 0.12f;
        def.displayName = "Ravenous Hydra";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3075()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3075u;
        def.price = 2450u;
        def.stats.flatHealth = 150.f;
        def.stats.flatArmor = 75.f;
        def.displayName = "Thornmail";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3076()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3076u;
        def.price = 800u;
        def.stats.flatArmor = 30.f;
        def.displayName = "Bramble Vest";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3077()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3077u;
        def.price = 1200u;
        def.stats.flatAd = 20.f;
        def.displayName = "Tiamat";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3078()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3078u;
        def.price = 3333u;
        def.stats.flatAd = 36.f;
        def.stats.flatHealth = 333.f;
        def.stats.bonusAttackSpeed = 0.3f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Trinity Force";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3082()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3082u;
        def.price = 1000u;
        def.stats.flatArmor = 40.f;
        def.displayName = "Warden's Mail";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3083()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3083u;
        def.price = 3100u;
        def.stats.flatHealth = 1000.f;
        def.displayName = "Warmog's Armor";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3084()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3084u;
        def.price = 3000u;
        def.stats.flatHealth = 900.f;
        def.displayName = "Heartsteel";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3085()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3085u;
        def.price = 2650u;
        def.stats.bonusAttackSpeed = 0.4f;
        def.stats.critChance = 0.25f;
        def.stats.percentMoveSpeed = 0.04f;
        def.displayName = "Runaan's Hurricane";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3086()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3086u;
        def.price = 1200u;
        def.stats.bonusAttackSpeed = 0.15f;
        def.stats.critChance = 0.15f;
        def.stats.percentMoveSpeed = 0.04f;
        def.displayName = "Zeal";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3087()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3087u;
        def.price = 3000u;
        def.stats.flatAd = 45.f;
        def.stats.flatAp = 45.f;
        def.stats.bonusAttackSpeed = 0.3f;
        def.stats.percentMoveSpeed = 0.04f;
        def.displayName = "Statikk Shiv";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3089()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3089u;
        def.price = 3500u;
        def.stats.flatAp = 130.f;
        def.displayName = "Rabadon's Deathcap";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3091()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3091u;
        def.price = 2800u;
        def.stats.flatMr = 45.f;
        def.stats.bonusAttackSpeed = 0.5f;
        def.displayName = "Wit's End";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3094()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3094u;
        def.price = 2650u;
        def.stats.bonusAttackSpeed = 0.35f;
        def.stats.critChance = 0.25f;
        def.stats.percentMoveSpeed = 0.04f;
        def.displayName = "Rapid Firecannon";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3097()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3097u;
        def.price = 3200u;
        def.stats.flatAd = 50.f;
        def.stats.bonusAttackSpeed = 0.2f;
        def.stats.critChance = 0.25f;
        def.displayName = "Stormrazor";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3100()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3100u;
        def.price = 2900u;
        def.stats.flatAp = 100.f;
        def.stats.abilityHaste = 10.f;
        def.stats.percentMoveSpeed = 0.06f;
        def.displayName = "Lich Bane";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3102()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3102u;
        def.price = 3000u;
        def.stats.flatAp = 105.f;
        def.stats.flatMr = 40.f;
        def.displayName = "Banshee's Veil";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3107()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3107u;
        def.price = 2300u;
        def.stats.flatAp = 30.f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Redemption";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3108()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3108u;
        def.price = 850u;
        def.stats.flatAp = 25.f;
        def.stats.abilityHaste = 10.f;
        def.displayName = "Fiendish Codex";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3109()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3109u;
        def.price = 2300u;
        def.stats.flatHealth = 200.f;
        def.stats.flatArmor = 40.f;
        def.stats.abilityHaste = 10.f;
        def.displayName = "Knight's Vow";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3110()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3110u;
        def.price = 2500u;
        def.stats.flatMana = 400.f;
        def.stats.flatArmor = 75.f;
        def.stats.abilityHaste = 20.f;
        def.displayName = "Frozen Heart";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3111()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3111u;
        def.price = 1250u;
        def.stats.flatMr = 20.f;
        def.stats.flatMoveSpeed = 45.f;
        def.displayName = "Mercury's Treads";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3112()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3112u;
        def.price = 950u;
        def.stats.flatAp = 50.f;
        def.stats.flatHealth = 150.f;
        def.displayName = "Guardian's Orb";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3113()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3113u;
        def.price = 900u;
        def.stats.flatAp = 30.f;
        def.stats.percentMoveSpeed = 0.04f;
        def.displayName = "Aether Wisp";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3114()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3114u;
        def.price = 600u;
        def.displayName = "Forbidden Idol";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3115()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3115u;
        def.price = 2900u;
        def.stats.flatAp = 80.f;
        def.stats.bonusAttackSpeed = 0.5f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Nashor's Tooth";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3116()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3116u;
        def.price = 2600u;
        def.stats.flatAp = 65.f;
        def.stats.flatHealth = 400.f;
        def.displayName = "Rylai's Crystal Scepter";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3118()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3118u;
        def.price = 2700u;
        def.stats.flatAp = 90.f;
        def.stats.flatMana = 600.f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Malignance";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3119()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3119u;
        def.price = 2400u;
        def.stats.flatHealth = 550.f;
        def.stats.flatMana = 500.f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Winter's Approach";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3123()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3123u;
        def.price = 800u;
        def.stats.flatAd = 15.f;
        def.displayName = "Executioner's Calling";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3124()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3124u;
        def.price = 3000u;
        def.stats.flatAd = 30.f;
        def.stats.flatAp = 30.f;
        def.stats.bonusAttackSpeed = 0.25f;
        def.displayName = "Guinsoo's Rageblade";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3133()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3133u;
        def.price = 1050u;
        def.stats.flatAd = 20.f;
        def.stats.abilityHaste = 10.f;
        def.displayName = "Caulfield's Warhammer";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3134()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3134u;
        def.price = 1000u;
        def.stats.flatAd = 20.f;
        def.stats.lethality = 10.f;
        def.displayName = "Serrated Dirk";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3135()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3135u;
        def.price = 3000u;
        def.stats.flatAp = 95.f;
        def.stats.magicPenPercent = 0.4f;
        def.displayName = "Void Staff";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3137()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3137u;
        def.price = 3000u;
        def.stats.flatAp = 75.f;
        def.stats.abilityHaste = 20.f;
        def.stats.magicPenPercent = 0.3f;
        def.displayName = "Cryptbloom";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3139()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3139u;
        def.price = 3200u;
        def.stats.flatAd = 50.f;
        def.stats.flatMr = 35.f;
        def.stats.lifeSteal = 0.1f;
        def.displayName = "Mercurial Scimitar";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3140()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3140u;
        def.price = 1300u;
        def.stats.flatMr = 30.f;
        def.displayName = "Quicksilver Sash";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3142()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3142u;
        def.price = 2800u;
        def.stats.flatAd = 55.f;
        def.stats.percentMoveSpeed = 0.04f;
        def.stats.lethality = 18.f;
        def.displayName = "Youmuu's Ghostblade";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3143()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3143u;
        def.price = 2700u;
        def.stats.flatHealth = 350.f;
        def.stats.flatArmor = 75.f;
        def.displayName = "Randuin's Omen";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3144()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3144u;
        def.price = 600u;
        def.stats.bonusAttackSpeed = 0.2f;
        def.displayName = "Scout's Slingshot";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3145()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3145u;
        def.price = 1100u;
        def.stats.flatAp = 45.f;
        def.displayName = "Hextech Alternator";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3146()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3146u;
        def.price = 3000u;
        def.stats.flatAd = 40.f;
        def.stats.flatAp = 80.f;
        def.displayName = "Hextech Gunblade";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3147()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3147u;
        def.price = 1300u;
        def.stats.flatAp = 30.f;
        def.stats.flatHealth = 200.f;
        def.displayName = "Haunting Guise";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3152()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3152u;
        def.price = 2650u;
        def.stats.flatAp = 60.f;
        def.stats.flatHealth = 350.f;
        def.stats.abilityHaste = 20.f;
        def.displayName = "Hextech Rocketbelt";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3153()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3153u;
        def.price = 3200u;
        def.stats.flatAd = 40.f;
        def.stats.bonusAttackSpeed = 0.25f;
        def.stats.lifeSteal = 0.1f;
        def.displayName = "Blade of The Ruined King";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3155()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3155u;
        def.price = 1300u;
        def.stats.flatAd = 25.f;
        def.stats.flatMr = 25.f;
        def.displayName = "Hexdrinker";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3156()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3156u;
        def.price = 3100u;
        def.stats.flatAd = 60.f;
        def.stats.flatMr = 40.f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Maw of Malmortius";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3157()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3157u;
        def.price = 3250u;
        def.stats.flatAp = 105.f;
        def.stats.flatArmor = 50.f;
        def.displayName = "Zhonya's Hourglass";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3158()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3158u;
        def.price = 900u;
        def.stats.abilityHaste = 10.f;
        def.stats.flatMoveSpeed = 45.f;
        def.displayName = "Ionian Boots of Lucidity";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3161()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3161u;
        def.price = 3100u;
        def.stats.flatAd = 45.f;
        def.stats.flatHealth = 450.f;
        def.displayName = "Spear of Shojin";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3165()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3165u;
        def.price = 2850u;
        def.stats.flatAp = 75.f;
        def.stats.flatHealth = 350.f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Morellonomicon";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3168()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3168u;
        def.price = 1000u;
        def.stats.flatMoveSpeed = 45.f;
        def.displayName = "Immortal Path";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3170()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3170u;
        def.price = 1000u;
        def.stats.flatMoveSpeed = 65.f;
        def.displayName = "Swiftmarch";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3171()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3171u;
        def.price = 900u;
        def.stats.abilityHaste = 20.f;
        def.stats.flatMoveSpeed = 45.f;
        def.displayName = "Crimson Lucidity";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3172()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3172u;
        def.price = 1100u;
        def.stats.bonusAttackSpeed = 0.4f;
        def.stats.flatMoveSpeed = 45.f;
        def.stats.lifeSteal = 0.05f;
        def.displayName = "Gunmetal Greaves";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3173()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3173u;
        def.price = 1250u;
        def.stats.flatMr = 30.f;
        def.stats.flatMoveSpeed = 45.f;
        def.displayName = "Chainlaced Crushers";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3174()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3174u;
        def.price = 1200u;
        def.stats.flatArmor = 35.f;
        def.stats.flatMoveSpeed = 45.f;
        def.displayName = "Armored Advance";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3175()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3175u;
        def.price = 1100u;
        def.stats.magicPenPercent = 0.08f;
        def.stats.flatMagicPen = 18.f;
        def.stats.flatMoveSpeed = 45.f;
        def.displayName = "Spellslinger's Shoes";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3177()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3177u;
        def.price = 950u;
        def.stats.flatAd = 30.f;
        def.stats.flatHealth = 150.f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Guardian's Blade";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3179()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3179u;
        def.price = 2800u;
        def.stats.flatAd = 60.f;
        def.stats.abilityHaste = 15.f;
        def.stats.lethality = 18.f;
        def.displayName = "Umbral Glaive";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3181()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3181u;
        def.price = 3000u;
        def.stats.flatAd = 40.f;
        def.stats.flatHealth = 500.f;
        def.stats.percentMoveSpeed = 0.04f;
        def.displayName = "Hullbreaker";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3184()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3184u;
        def.price = 950u;
        def.stats.flatAd = 25.f;
        def.stats.flatHealth = 150.f;
        def.stats.lifeSteal = 0.05f;
        def.displayName = "Guardian's Hammer";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3190()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3190u;
        def.price = 2200u;
        def.stats.flatHealth = 200.f;
        def.stats.flatArmor = 30.f;
        def.stats.flatMr = 30.f;
        def.stats.abilityHaste = 10.f;
        def.displayName = "Locket of the Iron Solari";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3211()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3211u;
        def.price = 1250u;
        def.stats.flatHealth = 200.f;
        def.stats.flatMr = 35.f;
        def.displayName = "Spectre's Cowl";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3222()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3222u;
        def.price = 2300u;
        def.stats.flatHealth = 250.f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Mikael's Blessing";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3302()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3302u;
        def.price = 3000u;
        def.stats.flatAd = 30.f;
        def.stats.bonusAttackSpeed = 0.35f;
        def.displayName = "Terminus";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3340()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3340u;
        def.price = 0u;
        def.displayName = "Stealth Ward";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3504()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3504u;
        def.price = 2200u;
        def.stats.flatAp = 45.f;
        def.stats.percentMoveSpeed = 0.04f;
        def.displayName = "Ardent Censer";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3508()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3508u;
        def.price = 3050u;
        def.stats.flatAd = 50.f;
        def.stats.critChance = 0.25f;
        def.stats.abilityHaste = 20.f;
        def.displayName = "Essence Reaver";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3599()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3599u;
        def.price = 0u;
        def.displayName = "Kalista's Black Spear";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3742()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3742u;
        def.price = 2900u;
        def.stats.flatHealth = 350.f;
        def.stats.flatArmor = 55.f;
        def.stats.percentMoveSpeed = 0.04f;
        def.displayName = "Dead Man's Plate";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3748()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3748u;
        def.price = 3300u;
        def.stats.flatAd = 40.f;
        def.stats.flatHealth = 600.f;
        def.displayName = "Titanic Hydra";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3801()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3801u;
        def.price = 800u;
        def.stats.flatHealth = 200.f;
        def.displayName = "Crystalline Bracer";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3802()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3802u;
        def.price = 1200u;
        def.stats.flatAp = 40.f;
        def.stats.flatMana = 300.f;
        def.stats.abilityHaste = 10.f;
        def.displayName = "Lost Chapter";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3803()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3803u;
        def.price = 1300u;
        def.stats.flatHealth = 300.f;
        def.stats.flatMana = 375.f;
        def.displayName = "Catalyst of Aeons";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3814()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3814u;
        def.price = 3000u;
        def.stats.flatAd = 50.f;
        def.stats.flatHealth = 250.f;
        def.stats.lethality = 15.f;
        def.displayName = "Edge of Night";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3865()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3865u;
        def.price = 400u;
        def.stats.flatHealth = 30.f;
        def.displayName = "World Atlas";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3869()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3869u;
        def.price = 400u;
        def.stats.flatHealth = 200.f;
        def.displayName = "Celestial Opposition";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3870()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3870u;
        def.price = 400u;
        def.stats.flatHealth = 200.f;
        def.displayName = "Dream Maker";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3871()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3871u;
        def.price = 400u;
        def.stats.flatHealth = 200.f;
        def.displayName = "Zaz'Zak's Realmspike";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3876()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3876u;
        def.price = 400u;
        def.stats.flatHealth = 200.f;
        def.displayName = "Solstice Sleigh";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3877()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3877u;
        def.price = 400u;
        def.stats.flatHealth = 200.f;
        def.displayName = "Bloodsong";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_3916()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 3916u;
        def.price = 800u;
        def.stats.flatAp = 25.f;
        def.displayName = "Oblivion Orb";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_4005()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 4005u;
        def.price = 2400u;
        def.stats.flatAp = 60.f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Imperial Mandate";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_4401()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 4401u;
        def.price = 2800u;
        def.stats.flatHealth = 400.f;
        def.stats.flatMr = 55.f;
        def.stats.percentMoveSpeed = 0.04f;
        def.displayName = "Force of Nature";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_4628()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 4628u;
        def.price = 2700u;
        def.stats.flatAp = 75.f;
        def.stats.abilityHaste = 25.f;
        def.displayName = "Horizon Focus";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_4629()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 4629u;
        def.price = 3000u;
        def.stats.flatAp = 70.f;
        def.stats.flatHealth = 350.f;
        def.stats.abilityHaste = 25.f;
        def.stats.percentMoveSpeed = 0.04f;
        def.displayName = "Cosmic Drive";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_4630()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 4630u;
        def.price = 1100u;
        def.stats.flatAp = 25.f;
        def.stats.magicPenPercent = 0.13f;
        def.displayName = "Blighting Jewel";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_4632()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 4632u;
        def.price = 1600u;
        def.stats.flatAp = 40.f;
        def.stats.flatMr = 25.f;
        def.displayName = "Verdant Barrier";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_4633()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 4633u;
        def.price = 3100u;
        def.stats.flatAp = 70.f;
        def.stats.flatHealth = 350.f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Riftmaker";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_4642()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 4642u;
        def.price = 900u;
        def.stats.flatAp = 20.f;
        def.stats.abilityHaste = 10.f;
        def.displayName = "Bandleglass Mirror";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_4645()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 4645u;
        def.price = 3200u;
        def.stats.flatAp = 110.f;
        def.stats.flatMagicPen = 15.f;
        def.displayName = "Shadowflame";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_4646()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 4646u;
        def.price = 2800u;
        def.stats.flatAp = 90.f;
        def.stats.percentMoveSpeed = 0.06f;
        def.stats.flatMagicPen = 15.f;
        def.displayName = "Stormsurge";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_6333()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 6333u;
        def.price = 3300u;
        def.stats.flatAd = 60.f;
        def.stats.flatArmor = 50.f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Death's Dance";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_6609()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 6609u;
        def.price = 3000u;
        def.stats.flatAd = 45.f;
        def.stats.flatHealth = 450.f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Chempunk Chainsword";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_6610()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 6610u;
        def.price = 3100u;
        def.stats.flatAd = 45.f;
        def.stats.flatHealth = 400.f;
        def.stats.abilityHaste = 10.f;
        def.displayName = "Sundered Sky";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_6616()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 6616u;
        def.price = 2250u;
        def.stats.flatAp = 35.f;
        def.stats.abilityHaste = 10.f;
        def.displayName = "Staff of Flowing Water";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_6617()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 6617u;
        def.price = 2200u;
        def.stats.flatAp = 25.f;
        def.stats.flatHealth = 200.f;
        def.stats.abilityHaste = 20.f;
        def.displayName = "Moonstone Renewer";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_6620()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 6620u;
        def.price = 2200u;
        def.stats.flatAp = 35.f;
        def.stats.flatHealth = 200.f;
        def.stats.abilityHaste = 20.f;
        def.displayName = "Echoes of Helia";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_6621()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 6621u;
        def.price = 2500u;
        def.stats.flatAp = 45.f;
        def.displayName = "Dawncore";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_6631()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 6631u;
        def.price = 3300u;
        def.stats.flatAd = 40.f;
        def.stats.flatHealth = 450.f;
        def.stats.bonusAttackSpeed = 0.25f;
        def.displayName = "Stridebreaker";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_6653()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 6653u;
        def.price = 3000u;
        def.stats.flatAp = 60.f;
        def.stats.flatHealth = 300.f;
        def.displayName = "Liandry's Torment";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_6655()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 6655u;
        def.price = 2750u;
        def.stats.flatAp = 100.f;
        def.stats.flatMana = 600.f;
        def.stats.abilityHaste = 10.f;
        def.displayName = "Luden's Echo";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_6657()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 6657u;
        def.price = 2600u;
        def.stats.flatAp = 45.f;
        def.stats.flatHealth = 350.f;
        def.stats.flatMana = 500.f;
        def.displayName = "Rod of Ages";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_6660()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 6660u;
        def.price = 900u;
        def.stats.flatHealth = 150.f;
        def.stats.abilityHaste = 5.f;
        def.displayName = "Bami's Cinder";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_6662()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 6662u;
        def.price = 2900u;
        def.stats.flatHealth = 300.f;
        def.stats.flatArmor = 50.f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Iceborn Gauntlet";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_6664()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 6664u;
        def.price = 2800u;
        def.stats.flatHealth = 400.f;
        def.stats.flatMr = 40.f;
        def.stats.abilityHaste = 10.f;
        def.displayName = "Hollow Radiance";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_6665()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 6665u;
        def.price = 3200u;
        def.stats.flatHealth = 350.f;
        def.stats.flatArmor = 45.f;
        def.stats.flatMr = 45.f;
        def.displayName = "Jak'Sho, The Protean";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_6670()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 6670u;
        def.price = 1300u;
        def.stats.flatAd = 15.f;
        def.stats.critChance = 0.2f;
        def.displayName = "Noonquiver";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_6672()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 6672u;
        def.price = 3000u;
        def.stats.flatAd = 45.f;
        def.stats.bonusAttackSpeed = 0.4f;
        def.stats.percentMoveSpeed = 0.04f;
        def.displayName = "Kraken Slayer";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_6673()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 6673u;
        def.price = 3000u;
        def.stats.flatAd = 55.f;
        def.stats.critChance = 0.25f;
        def.displayName = "Immortal Shieldbow";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_6675()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 6675u;
        def.price = 2650u;
        def.stats.bonusAttackSpeed = 0.4f;
        def.stats.critChance = 0.25f;
        def.stats.percentMoveSpeed = 0.04f;
        def.displayName = "Navori Flickerblade";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_6676()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 6676u;
        def.price = 3000u;
        def.stats.flatAd = 50.f;
        def.stats.critChance = 0.25f;
        def.stats.lethality = 10.f;
        def.displayName = "The Collector";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_6690()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 6690u;
        def.price = 775u;
        def.stats.flatAd = 15.f;
        def.stats.percentMoveSpeed = 0.04f;
        def.displayName = "Rectrix";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_6692()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 6692u;
        def.price = 2900u;
        def.stats.flatAd = 60.f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Eclipse";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_6694()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 6694u;
        def.price = 3000u;
        def.stats.flatAd = 45.f;
        def.stats.abilityHaste = 15.f;
        def.stats.armorPenPercent = 0.35f;
        def.displayName = "Serylda's Grudge";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_6695()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 6695u;
        def.price = 2500u;
        def.stats.flatAd = 55.f;
        def.stats.lethality = 15.f;
        def.displayName = "Serpent's Fang";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_6696()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 6696u;
        def.price = 2750u;
        def.stats.flatAd = 55.f;
        def.stats.abilityHaste = 20.f;
        def.stats.lethality = 18.f;
        def.displayName = "Axiom Arc";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_6697()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 6697u;
        def.price = 2800u;
        def.stats.flatAd = 55.f;
        def.stats.abilityHaste = 10.f;
        def.stats.lethality = 18.f;
        def.displayName = "Hubris";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_6698()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 6698u;
        def.price = 2850u;
        def.stats.flatAd = 55.f;
        def.stats.abilityHaste = 10.f;
        def.stats.lethality = 18.f;
        def.displayName = "Profane Hydra";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_6699()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 6699u;
        def.price = 3000u;
        def.stats.flatAd = 55.f;
        def.stats.abilityHaste = 10.f;
        def.stats.lethality = 10.f;
        def.displayName = "Voltaic Cyclosword";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_8010()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 8010u;
        def.price = 2900u;
        def.stats.flatAp = 65.f;
        def.stats.flatHealth = 400.f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Bloodletter's Curse";
        return def;
    }

    ClientData::ShopItemPresentationDefinition MakeShopItem_8020()
    {
        ClientData::ShopItemPresentationDefinition def{};
        def.itemId = 8020u;
        def.price = 2650u;
        def.stats.flatHealth = 350.f;
        def.stats.flatMr = 45.f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Abyssal Mask";
        return def;
    }

    ClientData::LocalSmokeMinionCombatDefinition MakeLocalSmokeMinion_0()
    {
        ClientData::LocalSmokeMinionCombatDefinition def{};
        def.roleType = static_cast<u8_t>(0u);
        def.combat.moveSpeed = 4.f;
        def.combat.attackRange = 1.5f;
        def.combat.sightRange = 12.f;
        def.combat.attackDamage = 10.f;
        def.combat.attackCooldownMax = 1.666667f;
        def.combat.maxHp = 225.f;
        return def;
    }

    ClientData::LocalSmokeMinionCombatDefinition MakeLocalSmokeMinion_1()
    {
        ClientData::LocalSmokeMinionCombatDefinition def{};
        def.roleType = static_cast<u8_t>(1u);
        def.combat.moveSpeed = 4.f;
        def.combat.attackRange = 5.6f;
        def.combat.sightRange = 14.f;
        def.combat.attackDamage = 20.f;
        def.combat.attackCooldownMax = 2.f;
        def.combat.maxHp = 225.f;
        return def;
    }

    ClientData::LocalSmokeMinionCombatDefinition MakeLocalSmokeMinion_2()
    {
        ClientData::LocalSmokeMinionCombatDefinition def{};
        def.roleType = static_cast<u8_t>(2u);
        def.combat.moveSpeed = 3.5f;
        def.combat.attackRange = 10.f;
        def.combat.sightRange = 16.f;
        def.combat.attackDamage = 40.f;
        def.combat.attackCooldownMax = 1.666667f;
        def.combat.maxHp = 225.f;
        return def;
    }

    ClientData::LocalSmokeMinionCombatDefinition MakeLocalSmokeMinion_3()
    {
        ClientData::LocalSmokeMinionCombatDefinition def{};
        def.roleType = static_cast<u8_t>(3u);
        def.combat.moveSpeed = 5.f;
        def.combat.attackRange = 2.f;
        def.combat.sightRange = 14.f;
        def.combat.attackDamage = 100.f;
        def.combat.attackCooldownMax = 1.666667f;
        def.combat.maxHp = 500.f;
        return def;
    }

    ClientData::LocalSmokeMinionCombatDefinition MakeLocalSmokeMinion_4()
    {
        ClientData::LocalSmokeMinionCombatDefinition def{};
        def.roleType = static_cast<u8_t>(4u);
        def.combat.moveSpeed = 5.2f;
        def.combat.attackRange = 2.2f;
        def.combat.sightRange = 14.f;
        def.combat.attackDamage = 80.f;
        def.combat.attackCooldownMax = 1.f;
        def.combat.maxHp = 750.f;
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

    const ClientData::ChampionModelVisualDefinition kChampionModelVisuals[] =
    {
        MakeChampionModelVisual_ANNIE(),
        MakeChampionModelVisual_ASHE(),
        MakeChampionModelVisual_EZREAL(),
        MakeChampionModelVisual_FIORA(),
        MakeChampionModelVisual_GAREN(),
        MakeChampionModelVisual_IRELIA(),
        MakeChampionModelVisual_JAX(),
        MakeChampionModelVisual_KALISTA(),
        MakeChampionModelVisual_KINDRED(),
        MakeChampionModelVisual_LEESIN(),
        MakeChampionModelVisual_MASTERYI(),
        MakeChampionModelVisual_RIVEN(),
        MakeChampionModelVisual_SYLAS(),
        MakeChampionModelVisual_VIEGO(),
        MakeChampionModelVisual_YASUO(),
        MakeChampionModelVisual_YONE(),
        MakeChampionModelVisual_ZED(),
    };

    const ClientData::ChampionModelVisualPack kChampionModelVisualPack =
    {
        kChampionModelVisuals,
        static_cast<u32_t>(sizeof(kChampionModelVisuals) / sizeof(kChampionModelVisuals[0]))
    };

    const ClientData::ChampionUiVisualDefinition kChampionUiVisuals[] =
    {
        MakeChampionUiVisual_ANNIE(),
        MakeChampionUiVisual_ASHE(),
        MakeChampionUiVisual_EZREAL(),
        MakeChampionUiVisual_FIORA(),
        MakeChampionUiVisual_GAREN(),
        MakeChampionUiVisual_IRELIA(),
        MakeChampionUiVisual_JAX(),
        MakeChampionUiVisual_KALISTA(),
        MakeChampionUiVisual_KINDRED(),
        MakeChampionUiVisual_LEESIN(),
        MakeChampionUiVisual_MASTERYI(),
        MakeChampionUiVisual_RIVEN(),
        MakeChampionUiVisual_SYLAS(),
        MakeChampionUiVisual_VIEGO(),
        MakeChampionUiVisual_YASUO(),
        MakeChampionUiVisual_YONE(),
        MakeChampionUiVisual_ZED(),
    };

    const ClientData::StructureVisualDefinition kStructureVisuals[] =
    {
        MakeStructureVisual_STRUCTURE_INHIBITOR_BLUE(),
        MakeStructureVisual_STRUCTURE_INHIBITOR_RED(),
        MakeStructureVisual_STRUCTURE_NEXUS_BLUE(),
        MakeStructureVisual_STRUCTURE_NEXUS_RED(),
        MakeStructureVisual_STRUCTURE_TURRET_BLUE(),
        MakeStructureVisual_STRUCTURE_TURRET_RED(),
    };

    const ClientData::JungleVisualDefinition kJungleVisuals[] =
    {
        MakeJungleVisual_JUNGLE_BARON(),
        MakeJungleVisual_JUNGLE_DRAGON(),
        MakeJungleVisual_JUNGLE_BLUE(),
        MakeJungleVisual_JUNGLE_RED(),
        MakeJungleVisual_JUNGLE_KRUG(),
        MakeJungleVisual_JUNGLE_GROMP(),
        MakeJungleVisual_JUNGLE_WOLF(),
        MakeJungleVisual_JUNGLE_RAZORBEAK(),
        MakeJungleVisual_JUNGLE_RAZORBEAK_MINI(),
        MakeJungleVisual_JUNGLE_WOLF_MINI(),
        MakeJungleVisual_JUNGLE_KRUG_MINI(),
    };

    const ClientData::MinionVisualDefinition kMinionVisuals[] =
    {
        MakeMinionVisual_MINION_BLUE_MELEE(),
        MakeMinionVisual_MINION_BLUE_RANGED(),
        MakeMinionVisual_MINION_BLUE_SIEGE(),
        MakeMinionVisual_MINION_BLUE_SUPER(),
        MakeMinionVisual_MINION_BLUE_TIBBERS(),
        MakeMinionVisual_MINION_RED_MELEE(),
        MakeMinionVisual_MINION_RED_RANGED(),
        MakeMinionVisual_MINION_RED_SIEGE(),
        MakeMinionVisual_MINION_RED_SUPER(),
        MakeMinionVisual_MINION_RED_TIBBERS(),
    };

    const ClientData::AmbientPropVisualDefinition kAmbientPropVisuals[] =
    {
        MakeAmbientPropVisual_AMBIENT_SRU_BIRD(),
        MakeAmbientPropVisual_AMBIENT_SRU_DUCK(),
        MakeAmbientPropVisual_AMBIENT_CHEMTECH_FIREFLY(),
    };

    const ClientData::AmbientPropVisualPack kAmbientPropVisualPack =
    {
        { L"Texture/MAP/Map11_Rebuild/cooked/map11_ambient_props.wamb" },
        kAmbientPropVisuals,
        static_cast<u32_t>(sizeof(kAmbientPropVisuals) / sizeof(kAmbientPropVisuals[0]))
    };

    const ClientData::MapRuntimeVisualDefinition kMapRuntimeVisual = MakeMapRuntimeVisual();

    const ClientData::FxMeshPreloadVisualDefinition kFxMeshPreloadVisuals[] =
    {
        MakeFxMeshPreloadVisual_FX_ASHE_BASE_ARROW(),
        MakeFxMeshPreloadVisual_FX_IRELIA_E_BEAM(),
        MakeFxMeshPreloadVisual_FX_IRELIA_E_BLADE(),
        MakeFxMeshPreloadVisual_FX_KALISTA_E_SPEAR_HOLD(),
        MakeFxMeshPreloadVisual_FX_KALISTA_Q_SPEAR(),
        MakeFxMeshPreloadVisual_FX_YASUO_W_WINDWALL(),
    };

    const ClientData::FxMeshPreloadVisualPack kFxMeshPreloadVisualPack =
    {
        kFxMeshPreloadVisuals,
        static_cast<u32_t>(sizeof(kFxMeshPreloadVisuals) / sizeof(kFxMeshPreloadVisuals[0]))
    };

    const ClientData::ShopItemPresentationDefinition kShopItemPresentations[] =
    {
        MakeShopItem_1001(),
        MakeShopItem_1004(),
        MakeShopItem_1006(),
        MakeShopItem_1011(),
        MakeShopItem_1018(),
        MakeShopItem_1026(),
        MakeShopItem_1027(),
        MakeShopItem_1028(),
        MakeShopItem_1029(),
        MakeShopItem_1031(),
        MakeShopItem_1033(),
        MakeShopItem_1036(),
        MakeShopItem_1037(),
        MakeShopItem_1038(),
        MakeShopItem_1042(),
        MakeShopItem_1043(),
        MakeShopItem_1052(),
        MakeShopItem_1053(),
        MakeShopItem_1054(),
        MakeShopItem_1055(),
        MakeShopItem_1056(),
        MakeShopItem_1057(),
        MakeShopItem_1058(),
        MakeShopItem_1082(),
        MakeShopItem_1083(),
        MakeShopItem_1086(),
        MakeShopItem_1101(),
        MakeShopItem_1102(),
        MakeShopItem_1103(),
        MakeShopItem_1105(),
        MakeShopItem_1106(),
        MakeShopItem_1107(),
        MakeShopItem_1120(),
        MakeShopItem_2003(),
        MakeShopItem_2019(),
        MakeShopItem_2020(),
        MakeShopItem_2021(),
        MakeShopItem_2022(),
        MakeShopItem_2031(),
        MakeShopItem_2051(),
        MakeShopItem_2055(),
        MakeShopItem_2065(),
        MakeShopItem_2138(),
        MakeShopItem_2139(),
        MakeShopItem_2140(),
        MakeShopItem_2141(),
        MakeShopItem_2420(),
        MakeShopItem_2501(),
        MakeShopItem_2502(),
        MakeShopItem_2503(),
        MakeShopItem_2504(),
        MakeShopItem_2508(),
        MakeShopItem_2510(),
        MakeShopItem_2512(),
        MakeShopItem_2517(),
        MakeShopItem_2520(),
        MakeShopItem_2522(),
        MakeShopItem_2523(),
        MakeShopItem_2524(),
        MakeShopItem_2525(),
        MakeShopItem_2526(),
        MakeShopItem_3003(),
        MakeShopItem_3004(),
        MakeShopItem_3006(),
        MakeShopItem_3008(),
        MakeShopItem_3009(),
        MakeShopItem_3020(),
        MakeShopItem_3024(),
        MakeShopItem_3026(),
        MakeShopItem_3031(),
        MakeShopItem_3032(),
        MakeShopItem_3033(),
        MakeShopItem_3035(),
        MakeShopItem_3036(),
        MakeShopItem_3041(),
        MakeShopItem_3042(),
        MakeShopItem_3044(),
        MakeShopItem_3046(),
        MakeShopItem_3047(),
        MakeShopItem_3050(),
        MakeShopItem_3051(),
        MakeShopItem_3053(),
        MakeShopItem_3057(),
        MakeShopItem_3065(),
        MakeShopItem_3066(),
        MakeShopItem_3067(),
        MakeShopItem_3068(),
        MakeShopItem_3070(),
        MakeShopItem_3071(),
        MakeShopItem_3072(),
        MakeShopItem_3073(),
        MakeShopItem_3074(),
        MakeShopItem_3075(),
        MakeShopItem_3076(),
        MakeShopItem_3077(),
        MakeShopItem_3078(),
        MakeShopItem_3082(),
        MakeShopItem_3083(),
        MakeShopItem_3084(),
        MakeShopItem_3085(),
        MakeShopItem_3086(),
        MakeShopItem_3087(),
        MakeShopItem_3089(),
        MakeShopItem_3091(),
        MakeShopItem_3094(),
        MakeShopItem_3097(),
        MakeShopItem_3100(),
        MakeShopItem_3102(),
        MakeShopItem_3107(),
        MakeShopItem_3108(),
        MakeShopItem_3109(),
        MakeShopItem_3110(),
        MakeShopItem_3111(),
        MakeShopItem_3112(),
        MakeShopItem_3113(),
        MakeShopItem_3114(),
        MakeShopItem_3115(),
        MakeShopItem_3116(),
        MakeShopItem_3118(),
        MakeShopItem_3119(),
        MakeShopItem_3123(),
        MakeShopItem_3124(),
        MakeShopItem_3133(),
        MakeShopItem_3134(),
        MakeShopItem_3135(),
        MakeShopItem_3137(),
        MakeShopItem_3139(),
        MakeShopItem_3140(),
        MakeShopItem_3142(),
        MakeShopItem_3143(),
        MakeShopItem_3144(),
        MakeShopItem_3145(),
        MakeShopItem_3146(),
        MakeShopItem_3147(),
        MakeShopItem_3152(),
        MakeShopItem_3153(),
        MakeShopItem_3155(),
        MakeShopItem_3156(),
        MakeShopItem_3157(),
        MakeShopItem_3158(),
        MakeShopItem_3161(),
        MakeShopItem_3165(),
        MakeShopItem_3168(),
        MakeShopItem_3170(),
        MakeShopItem_3171(),
        MakeShopItem_3172(),
        MakeShopItem_3173(),
        MakeShopItem_3174(),
        MakeShopItem_3175(),
        MakeShopItem_3177(),
        MakeShopItem_3179(),
        MakeShopItem_3181(),
        MakeShopItem_3184(),
        MakeShopItem_3190(),
        MakeShopItem_3211(),
        MakeShopItem_3222(),
        MakeShopItem_3302(),
        MakeShopItem_3340(),
        MakeShopItem_3504(),
        MakeShopItem_3508(),
        MakeShopItem_3599(),
        MakeShopItem_3742(),
        MakeShopItem_3748(),
        MakeShopItem_3801(),
        MakeShopItem_3802(),
        MakeShopItem_3803(),
        MakeShopItem_3814(),
        MakeShopItem_3865(),
        MakeShopItem_3869(),
        MakeShopItem_3870(),
        MakeShopItem_3871(),
        MakeShopItem_3876(),
        MakeShopItem_3877(),
        MakeShopItem_3916(),
        MakeShopItem_4005(),
        MakeShopItem_4401(),
        MakeShopItem_4628(),
        MakeShopItem_4629(),
        MakeShopItem_4630(),
        MakeShopItem_4632(),
        MakeShopItem_4633(),
        MakeShopItem_4642(),
        MakeShopItem_4645(),
        MakeShopItem_4646(),
        MakeShopItem_6333(),
        MakeShopItem_6609(),
        MakeShopItem_6610(),
        MakeShopItem_6616(),
        MakeShopItem_6617(),
        MakeShopItem_6620(),
        MakeShopItem_6621(),
        MakeShopItem_6631(),
        MakeShopItem_6653(),
        MakeShopItem_6655(),
        MakeShopItem_6657(),
        MakeShopItem_6660(),
        MakeShopItem_6662(),
        MakeShopItem_6664(),
        MakeShopItem_6665(),
        MakeShopItem_6670(),
        MakeShopItem_6672(),
        MakeShopItem_6673(),
        MakeShopItem_6675(),
        MakeShopItem_6676(),
        MakeShopItem_6690(),
        MakeShopItem_6692(),
        MakeShopItem_6694(),
        MakeShopItem_6695(),
        MakeShopItem_6696(),
        MakeShopItem_6697(),
        MakeShopItem_6698(),
        MakeShopItem_6699(),
        MakeShopItem_8010(),
        MakeShopItem_8020(),
    };

    const ClientData::LocalSmokeMinionCombatDefinition kLocalSmokeMinionCombatDefinitions[] =
    {
        MakeLocalSmokeMinion_0(),
        MakeLocalSmokeMinion_1(),
        MakeLocalSmokeMinion_2(),
        MakeLocalSmokeMinion_3(),
        MakeLocalSmokeMinion_4(),
    };
}

namespace ClientData
{
    u32_t GetLoLClientVisualDefinitionBuildHash()
    {
        return 0xBEC4359Eu;
    }

    const ChampionVisualDefinition* FindChampionVisualDefinition(eChampion champion)
    {
        if (const ChampionVisualDefinition* runtime = FindRuntimeChampionVisualDefinition(champion))
            return runtime;
        for (const ChampionVisualDefinition& definition : kChampionVisuals)
        {
            if (definition.legacyChampion == champion)
                return &definition;
        }
        return nullptr;
    }

    const ChampionVisualDefinition* FindChampionVisualDefinition(DefinitionKey key)
    {
        if (const ChampionVisualDefinition* runtime = FindRuntimeChampionVisualDefinition(key))
            return runtime;
        for (const ChampionVisualDefinition& definition : kChampionVisuals)
        {
            if (definition.key == key)
                return &definition;
        }
        return nullptr;
    }

    eChampion ResolveChampionFromDefinitionKey(DefinitionKey key)
    {
        const ChampionVisualDefinition* definition = FindChampionVisualDefinition(key);
        return definition ? definition->legacyChampion : eChampion::END;
    }

    f32_t ResolveChampionModelYawOffset(eChampion champion)
    {
        const ChampionVisualDefinition* definition = FindChampionVisualDefinition(champion);
        return definition ? definition->modelYawOffsetRadians : 0.f;
    }

    const ChampionModelVisualPack& GetChampionModelVisualPack()
    {
        if (const ChampionModelVisualPack* runtime = GetRuntimeChampionModelVisualPack())
            return *runtime;
        return kChampionModelVisualPack;
    }

    const ChampionModelVisualDefinition* FindChampionModelVisualDefinition(eChampion champion)
    {
        if (const ChampionModelVisualDefinition* runtime = FindRuntimeChampionModelVisualDefinition(champion))
            return runtime;
        for (const ChampionModelVisualDefinition& definition : kChampionModelVisuals)
        {
            if (definition.champion == champion)
                return &definition;
        }
        return nullptr;
    }

    const ChampionUiVisualDefinition* FindChampionUiVisualDefinition(eChampion champion)
    {
        if (const ChampionUiVisualDefinition* runtime = FindRuntimeChampionUiVisualDefinition(champion))
            return runtime;
        for (const ChampionUiVisualDefinition& definition : kChampionUiVisuals)
        {
            if (definition.champion == champion)
                return &definition;
        }
        return nullptr;
    }

    const StructureVisualDefinition* FindStructureVisualDefinition(Winters::Map::eObjectKind kind, eTeam team)
    {
        for (const StructureVisualDefinition& definition : kStructureVisuals)
        {
            if (definition.kind == kind && definition.team == team)
                return &definition;
        }
        return nullptr;
    }

    const JungleVisualDefinition* FindJungleVisualDefinition(u32_t subKind)
    {
        for (const JungleVisualDefinition& definition : kJungleVisuals)
        {
            if (definition.subKind == subKind)
                return &definition;
        }
        return nullptr;
    }

    const AmbientPropVisualPack& GetAmbientPropVisualPack()
    {
        return kAmbientPropVisualPack;
    }

    const AmbientPropVisualDefinition* FindAmbientPropVisualDefinition(u32_t kind)
    {
        for (const AmbientPropVisualDefinition& definition : kAmbientPropVisuals)
        {
            if (definition.kind == kind)
                return &definition;
        }
        return nullptr;
    }

    const MapRuntimeVisualDefinition& GetMapRuntimeVisualDefinition()
    {
        return kMapRuntimeVisual;
    }

    const FxMeshPreloadVisualPack& GetFxMeshPreloadVisualPack()
    {
        return kFxMeshPreloadVisualPack;
    }

    const ShopItemPresentationDefinition* FindShopItemPresentationDefinition(u16_t itemId)
    {
        for (const ShopItemPresentationDefinition& definition : kShopItemPresentations)
        {
            if (definition.itemId == itemId)
                return &definition;
        }
        return nullptr;
    }

    const MinionCombatDef* FindLocalSmokeMinionCombatDefinition(u8_t roleType)
    {
        for (const LocalSmokeMinionCombatDefinition& definition : kLocalSmokeMinionCombatDefinitions)
        {
            if (definition.roleType == roleType)
                return &definition.combat;
        }
        return nullptr;
    }

    const MinionVisualDefinition* FindMinionVisualDefinition(u32_t type, u32_t team)
    {
        for (const MinionVisualDefinition& definition : kMinionVisuals)
        {
            if (definition.type == type && definition.team == team)
                return &definition;
        }
        return nullptr;
    }
}
