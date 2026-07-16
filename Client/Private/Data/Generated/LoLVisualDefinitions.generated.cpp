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
        def.skills[4].stageCount = 1u;
        def.skills[4].replicatedCueId = 0u;
        def.skills[4].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[0].castFrame = 0.f;
        def.skills[4].stages[0].recoveryFrame = 0.f;
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
        def.skills[4].stageCount = 1u;
        def.skills[4].replicatedCueId = 0u;
        def.skills[4].stages[0].animationPlaybackSpeed = 1.f;
        def.skills[4].stages[0].castFrame = 18.f;
        def.skills[4].stages[0].recoveryFrame = 30.f;
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
        def.spawnScale = 0.015f;
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
        def.submeshStateCount = static_cast<u8_t>(0u);
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
        def.submeshStateCount = static_cast<u8_t>(0u);
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
        def.submeshStates[1].submeshIndex = 1u;
        def.submeshStates[1].bVisibleWhenDestroyed = false;
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
        def.submeshStates[1].submeshIndex = 1u;
        def.submeshStates[1].bVisibleWhenDestroyed = false;
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
        def.submeshStateCount = static_cast<u8_t>(7u);
        def.submeshStates[0].submeshIndex = 1u;
        def.submeshStates[0].bVisibleWhenDestroyed = true;
        def.submeshStates[1].submeshIndex = 2u;
        def.submeshStates[1].bVisibleWhenDestroyed = true;
        def.submeshStates[2].submeshIndex = 3u;
        def.submeshStates[2].bVisibleWhenDestroyed = true;
        def.submeshStates[3].submeshIndex = 4u;
        def.submeshStates[3].bVisibleWhenDestroyed = true;
        def.submeshStates[4].submeshIndex = 5u;
        def.submeshStates[4].bVisibleWhenDestroyed = true;
        def.submeshStates[5].submeshIndex = 6u;
        def.submeshStates[5].bVisibleWhenDestroyed = true;
        def.submeshStates[6].submeshIndex = 7u;
        def.submeshStates[6].bVisibleWhenDestroyed = true;
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
        def.submeshStateCount = static_cast<u8_t>(7u);
        def.submeshStates[0].submeshIndex = 1u;
        def.submeshStates[0].bVisibleWhenDestroyed = true;
        def.submeshStates[1].submeshIndex = 2u;
        def.submeshStates[1].bVisibleWhenDestroyed = true;
        def.submeshStates[2].submeshIndex = 3u;
        def.submeshStates[2].bVisibleWhenDestroyed = true;
        def.submeshStates[3].submeshIndex = 4u;
        def.submeshStates[3].bVisibleWhenDestroyed = true;
        def.submeshStates[4].submeshIndex = 5u;
        def.submeshStates[4].bVisibleWhenDestroyed = true;
        def.submeshStates[5].submeshIndex = 6u;
        def.submeshStates[5].bVisibleWhenDestroyed = true;
        def.submeshStates[6].submeshIndex = 7u;
        def.submeshStates[6].bVisibleWhenDestroyed = true;
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
        MakeChampionModelVisual_GAREN(),
        MakeChampionModelVisual_KALISTA(),
        MakeChampionModelVisual_RIVEN(),
        MakeChampionModelVisual_SYLAS(),
        MakeChampionModelVisual_VIEGO(),
        MakeChampionModelVisual_YASUO(),
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

    const ChampionModelVisualPack& GetChampionModelVisualPack()
    {
        return kChampionModelVisualPack;
    }

    const ChampionModelVisualDefinition* FindChampionModelVisualDefinition(eChampion champion)
    {
        for (const ChampionModelVisualDefinition& definition : kChampionModelVisuals)
        {
            if (definition.champion == champion)
                return &definition;
        }
        return nullptr;
    }

    const ChampionUiVisualDefinition* FindChampionUiVisualDefinition(eChampion champion)
    {
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
