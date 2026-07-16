#include "Server/Private/Data/LoLGameplayDefinitionPack.h"

namespace
{
    inline constexpr u32_t kBuildHash = 0x8E9EF70Fu;

    ChampionGameplayDef MakeChampion_ANNIE()
    {
        ChampionGameplayDef def{};
        def.key = 0xA3618A11u;
        def.id.value = 1u;
        def.legacyChampion = eChampion::ANNIE;
        def.dataVersion = 1u;
        def.authoringHash = kBuildHash;
        def.stats.baseHp = 560.f;
        def.stats.hpPerLevel = 102.f;
        def.stats.baseMana = 418.f;
        def.stats.manaPerLevel = 25.f;
        def.stats.baseAd = 50.f;
        def.stats.adPerLevel = 2.65f;
        def.stats.baseAp = 0.f;
        def.stats.apPerLevel = 0.f;
        def.stats.baseArmor = 23.f;
        def.stats.armorPerLevel = 4.f;
        def.stats.baseMr = 30.f;
        def.stats.mrPerLevel = 1.3f;
        def.stats.baseAttackSpeed = 0.625f;
        def.stats.attackSpeedRatio = 0.625f;
        def.stats.attackSpeedPerLevel = 0.02f;
        def.stats.baseAttackRange = 6.25f;
        def.stats.baseMoveSpeed = 5.f;
        def.stats.navArriveRadius = 0.15f;
        def.stats.spatialRadius = 0.75f;
        def.stats.sightRange = 19.f;
        def.skillLoadout[0].value = 1u;
        def.skillLoadout[1].value = 3u;
        def.skillLoadout[2].value = 5u;
        def.skillLoadout[3].value = 2u;
        def.skillLoadout[4].value = 4u;
        return def;
    }

    ChampionGameplayDef MakeChampion_ASHE()
    {
        ChampionGameplayDef def{};
        def.key = 0xEC6E77EFu;
        def.id.value = 2u;
        def.legacyChampion = eChampion::ASHE;
        def.dataVersion = 1u;
        def.authoringHash = kBuildHash;
        def.stats.baseHp = 610.f;
        def.stats.hpPerLevel = 101.f;
        def.stats.baseMana = 280.f;
        def.stats.manaPerLevel = 35.f;
        def.stats.baseAd = 59.f;
        def.stats.adPerLevel = 2.95f;
        def.stats.baseAp = 0.f;
        def.stats.apPerLevel = 0.f;
        def.stats.baseArmor = 26.f;
        def.stats.armorPerLevel = 4.6f;
        def.stats.baseMr = 30.f;
        def.stats.mrPerLevel = 1.3f;
        def.stats.baseAttackSpeed = 0.658f;
        def.stats.attackSpeedRatio = 0.658f;
        def.stats.attackSpeedPerLevel = 0.0333f;
        def.stats.baseAttackRange = 6.f;
        def.stats.baseMoveSpeed = 5.f;
        def.stats.navArriveRadius = 0.15f;
        def.stats.spatialRadius = 0.75f;
        def.stats.sightRange = 19.f;
        def.skillLoadout[0].value = 6u;
        def.skillLoadout[1].value = 8u;
        def.skillLoadout[2].value = 10u;
        def.skillLoadout[3].value = 7u;
        def.skillLoadout[4].value = 9u;
        return def;
    }

    ChampionGameplayDef MakeChampion_EZREAL()
    {
        ChampionGameplayDef def{};
        def.key = 0xD5D4F0F3u;
        def.id.value = 3u;
        def.legacyChampion = eChampion::EZREAL;
        def.dataVersion = 1u;
        def.authoringHash = kBuildHash;
        def.stats.baseHp = 600.f;
        def.stats.hpPerLevel = 102.f;
        def.stats.baseMana = 375.f;
        def.stats.manaPerLevel = 70.f;
        def.stats.baseAd = 60.f;
        def.stats.adPerLevel = 2.75f;
        def.stats.baseAp = 0.f;
        def.stats.apPerLevel = 0.f;
        def.stats.baseArmor = 24.f;
        def.stats.armorPerLevel = 4.7f;
        def.stats.baseMr = 30.f;
        def.stats.mrPerLevel = 1.3f;
        def.stats.baseAttackSpeed = 0.625f;
        def.stats.attackSpeedRatio = 0.625f;
        def.stats.attackSpeedPerLevel = 0.025f;
        def.stats.baseAttackRange = 5.5f;
        def.stats.baseMoveSpeed = 5.f;
        def.stats.navArriveRadius = 0.15f;
        def.stats.spatialRadius = 0.75f;
        def.stats.sightRange = 19.f;
        def.skillLoadout[0].value = 11u;
        def.skillLoadout[1].value = 13u;
        def.skillLoadout[2].value = 15u;
        def.skillLoadout[3].value = 12u;
        def.skillLoadout[4].value = 14u;
        return def;
    }

    ChampionGameplayDef MakeChampion_FIORA()
    {
        ChampionGameplayDef def{};
        def.key = 0x4D3C3313u;
        def.id.value = 4u;
        def.legacyChampion = eChampion::FIORA;
        def.dataVersion = 1u;
        def.authoringHash = kBuildHash;
        def.stats.baseHp = 620.f;
        def.stats.hpPerLevel = 99.f;
        def.stats.baseMana = 300.f;
        def.stats.manaPerLevel = 50.f;
        def.stats.baseAd = 66.f;
        def.stats.adPerLevel = 3.3f;
        def.stats.baseAp = 0.f;
        def.stats.apPerLevel = 0.f;
        def.stats.baseArmor = 33.f;
        def.stats.armorPerLevel = 4.7f;
        def.stats.baseMr = 32.f;
        def.stats.mrPerLevel = 2.05f;
        def.stats.baseAttackSpeed = 0.69f;
        def.stats.attackSpeedRatio = 0.69f;
        def.stats.attackSpeedPerLevel = 0.032f;
        def.stats.baseAttackRange = 1.5f;
        def.stats.baseMoveSpeed = 5.f;
        def.stats.navArriveRadius = 0.15f;
        def.stats.spatialRadius = 0.75f;
        def.stats.sightRange = 19.f;
        def.skillLoadout[0].value = 16u;
        def.skillLoadout[1].value = 18u;
        def.skillLoadout[2].value = 20u;
        def.skillLoadout[3].value = 17u;
        def.skillLoadout[4].value = 19u;
        return def;
    }

    ChampionGameplayDef MakeChampion_GAREN()
    {
        ChampionGameplayDef def{};
        def.key = 0x08F519B1u;
        def.id.value = 5u;
        def.legacyChampion = eChampion::GAREN;
        def.dataVersion = 1u;
        def.authoringHash = kBuildHash;
        def.stats.baseHp = 690.f;
        def.stats.hpPerLevel = 98.f;
        def.stats.baseMana = 300.f;
        def.stats.manaPerLevel = 50.f;
        def.stats.baseAd = 69.f;
        def.stats.adPerLevel = 4.5f;
        def.stats.baseAp = 0.f;
        def.stats.apPerLevel = 0.f;
        def.stats.baseArmor = 38.f;
        def.stats.armorPerLevel = 4.2f;
        def.stats.baseMr = 32.f;
        def.stats.mrPerLevel = 2.05f;
        def.stats.baseAttackSpeed = 0.625f;
        def.stats.attackSpeedRatio = 0.625f;
        def.stats.attackSpeedPerLevel = 0.034f;
        def.stats.baseAttackRange = 1.5f;
        def.stats.baseMoveSpeed = 5.f;
        def.stats.navArriveRadius = 0.15f;
        def.stats.spatialRadius = 0.75f;
        def.stats.sightRange = 19.f;
        def.skillLoadout[0].value = 21u;
        def.skillLoadout[1].value = 23u;
        def.skillLoadout[2].value = 25u;
        def.skillLoadout[3].value = 22u;
        def.skillLoadout[4].value = 24u;
        return def;
    }

    ChampionGameplayDef MakeChampion_IRELIA()
    {
        ChampionGameplayDef def{};
        def.key = 0xEB28CFB8u;
        def.id.value = 6u;
        def.legacyChampion = eChampion::IRELIA;
        def.dataVersion = 1u;
        def.authoringHash = kBuildHash;
        def.stats.baseHp = 590.f;
        def.stats.hpPerLevel = 124.f;
        def.stats.baseMana = 350.f;
        def.stats.manaPerLevel = 50.f;
        def.stats.baseAd = 65.f;
        def.stats.adPerLevel = 3.5f;
        def.stats.baseAp = 0.f;
        def.stats.apPerLevel = 0.f;
        def.stats.baseArmor = 36.f;
        def.stats.armorPerLevel = 4.7f;
        def.stats.baseMr = 32.f;
        def.stats.mrPerLevel = 2.05f;
        def.stats.baseAttackSpeed = 0.656f;
        def.stats.attackSpeedRatio = 0.656f;
        def.stats.attackSpeedPerLevel = 0.025f;
        def.stats.baseAttackRange = 2.1f;
        def.stats.baseMoveSpeed = 5.f;
        def.stats.navArriveRadius = 0.15f;
        def.stats.spatialRadius = 0.75f;
        def.stats.sightRange = 19.f;
        def.skillLoadout[0].value = 26u;
        def.skillLoadout[1].value = 28u;
        def.skillLoadout[2].value = 30u;
        def.skillLoadout[3].value = 27u;
        def.skillLoadout[4].value = 29u;
        return def;
    }

    ChampionGameplayDef MakeChampion_JAX()
    {
        ChampionGameplayDef def{};
        def.key = 0x0F445E37u;
        def.id.value = 7u;
        def.legacyChampion = eChampion::JAX;
        def.dataVersion = 1u;
        def.authoringHash = kBuildHash;
        def.stats.baseHp = 665.f;
        def.stats.hpPerLevel = 103.f;
        def.stats.baseMana = 300.f;
        def.stats.manaPerLevel = 50.f;
        def.stats.baseAd = 68.f;
        def.stats.adPerLevel = 3.375f;
        def.stats.baseAp = 0.f;
        def.stats.apPerLevel = 0.f;
        def.stats.baseArmor = 36.f;
        def.stats.armorPerLevel = 4.6f;
        def.stats.baseMr = 32.f;
        def.stats.mrPerLevel = 2.05f;
        def.stats.baseAttackSpeed = 0.638f;
        def.stats.attackSpeedRatio = 0.638f;
        def.stats.attackSpeedPerLevel = 0.034f;
        def.stats.baseAttackRange = 1.5f;
        def.stats.baseMoveSpeed = 5.f;
        def.stats.navArriveRadius = 0.15f;
        def.stats.spatialRadius = 0.75f;
        def.stats.sightRange = 19.f;
        def.skillLoadout[0].value = 31u;
        def.skillLoadout[1].value = 33u;
        def.skillLoadout[2].value = 35u;
        def.skillLoadout[3].value = 32u;
        def.skillLoadout[4].value = 34u;
        return def;
    }

    ChampionGameplayDef MakeChampion_KALISTA()
    {
        ChampionGameplayDef def{};
        def.key = 0x20024CB3u;
        def.id.value = 8u;
        def.legacyChampion = eChampion::KALISTA;
        def.dataVersion = 1u;
        def.authoringHash = kBuildHash;
        def.stats.baseHp = 574.f;
        def.stats.hpPerLevel = 114.f;
        def.stats.baseMana = 300.f;
        def.stats.manaPerLevel = 45.f;
        def.stats.baseAd = 61.f;
        def.stats.adPerLevel = 3.25f;
        def.stats.baseAp = 0.f;
        def.stats.apPerLevel = 0.f;
        def.stats.baseArmor = 24.f;
        def.stats.armorPerLevel = 4.6f;
        def.stats.baseMr = 30.f;
        def.stats.mrPerLevel = 1.3f;
        def.stats.baseAttackSpeed = 0.694f;
        def.stats.attackSpeedRatio = 0.694f;
        def.stats.attackSpeedPerLevel = 0.045f;
        def.stats.baseAttackRange = 5.5f;
        def.stats.baseMoveSpeed = 5.f;
        def.stats.navArriveRadius = 0.15f;
        def.stats.spatialRadius = 0.75f;
        def.stats.sightRange = 19.f;
        def.skillLoadout[0].value = 36u;
        def.skillLoadout[1].value = 38u;
        def.skillLoadout[2].value = 40u;
        def.skillLoadout[3].value = 37u;
        def.skillLoadout[4].value = 39u;
        def.passiveDash.bValid = true;
        def.passiveDash.distance = 2.f;
        def.passiveDash.durationSec = 0.2f;
        def.passiveDash.inputGraceSec = 0.2f;
        return def;
    }

    ChampionGameplayDef MakeChampion_KINDRED()
    {
        ChampionGameplayDef def{};
        def.key = 0xD30227C9u;
        def.id.value = 9u;
        def.legacyChampion = eChampion::KINDRED;
        def.dataVersion = 1u;
        def.authoringHash = kBuildHash;
        def.stats.baseHp = 610.f;
        def.stats.hpPerLevel = 99.f;
        def.stats.baseMana = 300.f;
        def.stats.manaPerLevel = 50.f;
        def.stats.baseAd = 65.f;
        def.stats.adPerLevel = 3.25f;
        def.stats.baseAp = 0.f;
        def.stats.apPerLevel = 0.f;
        def.stats.baseArmor = 29.f;
        def.stats.armorPerLevel = 4.7f;
        def.stats.baseMr = 30.f;
        def.stats.mrPerLevel = 1.3f;
        def.stats.baseAttackSpeed = 0.625f;
        def.stats.attackSpeedRatio = 0.625f;
        def.stats.attackSpeedPerLevel = 0.034f;
        def.stats.baseAttackRange = 5.5f;
        def.stats.baseMoveSpeed = 5.f;
        def.stats.navArriveRadius = 0.15f;
        def.stats.spatialRadius = 0.75f;
        def.stats.sightRange = 19.f;
        def.skillLoadout[0].value = 41u;
        def.skillLoadout[1].value = 43u;
        def.skillLoadout[2].value = 45u;
        def.skillLoadout[3].value = 42u;
        def.skillLoadout[4].value = 44u;
        return def;
    }

    ChampionGameplayDef MakeChampion_LEESIN()
    {
        ChampionGameplayDef def{};
        def.key = 0xB01E6158u;
        def.id.value = 10u;
        def.legacyChampion = eChampion::LEESIN;
        def.dataVersion = 1u;
        def.authoringHash = kBuildHash;
        def.stats.baseHp = 645.f;
        def.stats.hpPerLevel = 108.f;
        def.stats.baseMana = 300.f;
        def.stats.manaPerLevel = 50.f;
        def.stats.baseAd = 66.f;
        def.stats.adPerLevel = 3.7f;
        def.stats.baseAp = 0.f;
        def.stats.apPerLevel = 0.f;
        def.stats.baseArmor = 34.f;
        def.stats.armorPerLevel = 4.7f;
        def.stats.baseMr = 32.f;
        def.stats.mrPerLevel = 2.05f;
        def.stats.baseAttackSpeed = 0.651f;
        def.stats.attackSpeedRatio = 0.651f;
        def.stats.attackSpeedPerLevel = 0.03f;
        def.stats.baseAttackRange = 1.5f;
        def.stats.baseMoveSpeed = 5.f;
        def.stats.navArriveRadius = 0.15f;
        def.stats.spatialRadius = 0.75f;
        def.stats.sightRange = 19.f;
        def.skillLoadout[0].value = 46u;
        def.skillLoadout[1].value = 48u;
        def.skillLoadout[2].value = 50u;
        def.skillLoadout[3].value = 47u;
        def.skillLoadout[4].value = 49u;
        return def;
    }

    ChampionGameplayDef MakeChampion_MASTERYI()
    {
        ChampionGameplayDef def{};
        def.key = 0x7C99C014u;
        def.id.value = 11u;
        def.legacyChampion = eChampion::MASTERYI;
        def.dataVersion = 1u;
        def.authoringHash = kBuildHash;
        def.stats.baseHp = 669.f;
        def.stats.hpPerLevel = 105.f;
        def.stats.baseMana = 300.f;
        def.stats.manaPerLevel = 50.f;
        def.stats.baseAd = 65.f;
        def.stats.adPerLevel = 2.5f;
        def.stats.baseAp = 0.f;
        def.stats.apPerLevel = 0.f;
        def.stats.baseArmor = 33.f;
        def.stats.armorPerLevel = 4.7f;
        def.stats.baseMr = 32.f;
        def.stats.mrPerLevel = 2.05f;
        def.stats.baseAttackSpeed = 0.679f;
        def.stats.attackSpeedRatio = 0.679f;
        def.stats.attackSpeedPerLevel = 0.025f;
        def.stats.baseAttackRange = 1.5f;
        def.stats.baseMoveSpeed = 5.f;
        def.stats.navArriveRadius = 0.15f;
        def.stats.spatialRadius = 0.75f;
        def.stats.sightRange = 19.f;
        def.skillLoadout[0].value = 51u;
        def.skillLoadout[1].value = 53u;
        def.skillLoadout[2].value = 55u;
        def.skillLoadout[3].value = 52u;
        def.skillLoadout[4].value = 54u;
        return def;
    }

    ChampionGameplayDef MakeChampion_RIVEN()
    {
        ChampionGameplayDef def{};
        def.key = 0xAF33CE6Eu;
        def.id.value = 12u;
        def.legacyChampion = eChampion::RIVEN;
        def.dataVersion = 1u;
        def.authoringHash = kBuildHash;
        def.stats.baseHp = 630.f;
        def.stats.hpPerLevel = 100.f;
        def.stats.baseMana = 300.f;
        def.stats.manaPerLevel = 50.f;
        def.stats.baseAd = 64.f;
        def.stats.adPerLevel = 3.f;
        def.stats.baseAp = 0.f;
        def.stats.apPerLevel = 0.f;
        def.stats.baseArmor = 33.f;
        def.stats.armorPerLevel = 4.4f;
        def.stats.baseMr = 32.f;
        def.stats.mrPerLevel = 2.05f;
        def.stats.baseAttackSpeed = 0.625f;
        def.stats.attackSpeedRatio = 0.625f;
        def.stats.attackSpeedPerLevel = 0.034f;
        def.stats.baseAttackRange = 1.5f;
        def.stats.baseMoveSpeed = 5.f;
        def.stats.navArriveRadius = 0.15f;
        def.stats.spatialRadius = 0.75f;
        def.stats.sightRange = 19.f;
        def.skillLoadout[0].value = 56u;
        def.skillLoadout[1].value = 58u;
        def.skillLoadout[2].value = 60u;
        def.skillLoadout[3].value = 57u;
        def.skillLoadout[4].value = 59u;
        return def;
    }

    ChampionGameplayDef MakeChampion_SYLAS()
    {
        ChampionGameplayDef def{};
        def.key = 0xB749515Cu;
        def.id.value = 13u;
        def.legacyChampion = eChampion::SYLAS;
        def.dataVersion = 1u;
        def.authoringHash = kBuildHash;
        def.stats.baseHp = 600.f;
        def.stats.hpPerLevel = 115.f;
        def.stats.baseMana = 400.f;
        def.stats.manaPerLevel = 70.f;
        def.stats.baseAd = 61.f;
        def.stats.adPerLevel = 3.f;
        def.stats.baseAp = 0.f;
        def.stats.apPerLevel = 0.f;
        def.stats.baseArmor = 32.f;
        def.stats.armorPerLevel = 4.7f;
        def.stats.baseMr = 32.f;
        def.stats.mrPerLevel = 2.05f;
        def.stats.baseAttackSpeed = 0.645f;
        def.stats.attackSpeedRatio = 0.645f;
        def.stats.attackSpeedPerLevel = 0.034f;
        def.stats.baseAttackRange = 1.5f;
        def.stats.baseMoveSpeed = 5.f;
        def.stats.navArriveRadius = 0.15f;
        def.stats.spatialRadius = 0.75f;
        def.stats.sightRange = 19.f;
        def.skillLoadout[0].value = 61u;
        def.skillLoadout[1].value = 63u;
        def.skillLoadout[2].value = 65u;
        def.skillLoadout[3].value = 62u;
        def.skillLoadout[4].value = 64u;
        return def;
    }

    ChampionGameplayDef MakeChampion_VIEGO()
    {
        ChampionGameplayDef def{};
        def.key = 0xF0FB5992u;
        def.id.value = 14u;
        def.legacyChampion = eChampion::VIEGO;
        def.dataVersion = 1u;
        def.authoringHash = kBuildHash;
        def.stats.baseHp = 630.f;
        def.stats.hpPerLevel = 109.f;
        def.stats.baseMana = 300.f;
        def.stats.manaPerLevel = 50.f;
        def.stats.baseAd = 57.f;
        def.stats.adPerLevel = 3.f;
        def.stats.baseAp = 0.f;
        def.stats.apPerLevel = 0.f;
        def.stats.baseArmor = 34.f;
        def.stats.armorPerLevel = 4.6f;
        def.stats.baseMr = 32.f;
        def.stats.mrPerLevel = 2.05f;
        def.stats.baseAttackSpeed = 0.658f;
        def.stats.attackSpeedRatio = 0.658f;
        def.stats.attackSpeedPerLevel = 0.025f;
        def.stats.baseAttackRange = 1.5f;
        def.stats.baseMoveSpeed = 5.f;
        def.stats.navArriveRadius = 0.15f;
        def.stats.spatialRadius = 0.75f;
        def.stats.sightRange = 19.f;
        def.skillLoadout[0].value = 66u;
        def.skillLoadout[1].value = 68u;
        def.skillLoadout[2].value = 70u;
        def.skillLoadout[3].value = 67u;
        def.skillLoadout[4].value = 69u;
        def.passiveSoul.bValid = true;
        def.passiveSoul.lifetimeSec = 5.f;
        def.passiveSoul.radius = 0.85f;
        return def;
    }

    ChampionGameplayDef MakeChampion_YASUO()
    {
        ChampionGameplayDef def{};
        def.key = 0xBA78D203u;
        def.id.value = 15u;
        def.legacyChampion = eChampion::YASUO;
        def.dataVersion = 1u;
        def.authoringHash = kBuildHash;
        def.stats.baseHp = 590.f;
        def.stats.hpPerLevel = 110.f;
        def.stats.baseMana = 300.f;
        def.stats.manaPerLevel = 50.f;
        def.stats.baseAd = 60.f;
        def.stats.adPerLevel = 3.f;
        def.stats.baseAp = 0.f;
        def.stats.apPerLevel = 0.f;
        def.stats.baseArmor = 30.f;
        def.stats.armorPerLevel = 4.6f;
        def.stats.baseMr = 32.f;
        def.stats.mrPerLevel = 2.05f;
        def.stats.baseAttackSpeed = 0.697f;
        def.stats.attackSpeedRatio = 0.697f;
        def.stats.attackSpeedPerLevel = 0.033f;
        def.stats.baseAttackRange = 2.5f;
        def.stats.baseMoveSpeed = 5.f;
        def.stats.navArriveRadius = 0.15f;
        def.stats.spatialRadius = 0.75f;
        def.stats.sightRange = 19.f;
        def.skillLoadout[0].value = 71u;
        def.skillLoadout[1].value = 73u;
        def.skillLoadout[2].value = 75u;
        def.skillLoadout[3].value = 72u;
        def.skillLoadout[4].value = 74u;
        return def;
    }

    ChampionGameplayDef MakeChampion_YONE()
    {
        ChampionGameplayDef def{};
        def.key = 0xC7C340B1u;
        def.id.value = 16u;
        def.legacyChampion = eChampion::YONE;
        def.dataVersion = 1u;
        def.authoringHash = kBuildHash;
        def.stats.baseHp = 620.f;
        def.stats.hpPerLevel = 105.f;
        def.stats.baseMana = 300.f;
        def.stats.manaPerLevel = 50.f;
        def.stats.baseAd = 60.f;
        def.stats.adPerLevel = 2.f;
        def.stats.baseAp = 0.f;
        def.stats.apPerLevel = 0.f;
        def.stats.baseArmor = 33.f;
        def.stats.armorPerLevel = 4.7f;
        def.stats.baseMr = 32.f;
        def.stats.mrPerLevel = 2.05f;
        def.stats.baseAttackSpeed = 0.625f;
        def.stats.attackSpeedRatio = 0.625f;
        def.stats.attackSpeedPerLevel = 0.025f;
        def.stats.baseAttackRange = 1.5f;
        def.stats.baseMoveSpeed = 5.f;
        def.stats.navArriveRadius = 0.15f;
        def.stats.spatialRadius = 0.75f;
        def.stats.sightRange = 19.f;
        def.skillLoadout[0].value = 76u;
        def.skillLoadout[1].value = 78u;
        def.skillLoadout[2].value = 80u;
        def.skillLoadout[3].value = 77u;
        def.skillLoadout[4].value = 79u;
        return def;
    }

    ChampionGameplayDef MakeChampion_ZED()
    {
        ChampionGameplayDef def{};
        def.key = 0x556E51C7u;
        def.id.value = 17u;
        def.legacyChampion = eChampion::ZED;
        def.dataVersion = 1u;
        def.authoringHash = kBuildHash;
        def.stats.baseHp = 654.f;
        def.stats.hpPerLevel = 99.f;
        def.stats.baseMana = 300.f;
        def.stats.manaPerLevel = 50.f;
        def.stats.baseAd = 63.f;
        def.stats.adPerLevel = 3.4f;
        def.stats.baseAp = 0.f;
        def.stats.apPerLevel = 0.f;
        def.stats.baseArmor = 32.f;
        def.stats.armorPerLevel = 4.7f;
        def.stats.baseMr = 32.f;
        def.stats.mrPerLevel = 2.05f;
        def.stats.baseAttackSpeed = 0.651f;
        def.stats.attackSpeedRatio = 0.651f;
        def.stats.attackSpeedPerLevel = 0.033f;
        def.stats.baseAttackRange = 1.5f;
        def.stats.baseMoveSpeed = 5.f;
        def.stats.navArriveRadius = 0.15f;
        def.stats.spatialRadius = 0.75f;
        def.stats.sightRange = 19.f;
        def.skillLoadout[0].value = 81u;
        def.skillLoadout[1].value = 83u;
        def.skillLoadout[2].value = 85u;
        def.skillLoadout[3].value = 82u;
        def.skillLoadout[4].value = 84u;
        return def;
    }

    SkillGameplayDef MakeSkill_ANNIE_BASIC_ATTACK()
    {
        SkillGameplayDef def{};
        def.key = 0x282148FAu;
        def.id.value = 1u;
        def.ownerChampionId.value = 1u;
        def.slot = 0u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 1u;
        def.cooldown.rankCount = 1u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 0.6f;
        def.range.rangeMax = 6.25f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 0.6f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 1u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_CanCrit | DamageFlag_CanLifesteal | DamageFlag_OnHit;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 1.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 0.8f;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
        def.target.shape[1] = eTargetShape::Unit;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsTarget;
        return def;
    }

    SkillGameplayDef MakeSkill_ANNIE_E()
    {
        SkillGameplayDef def{};
        def.key = 0x6E73BDE8u;
        def.id.value = 2u;
        def.ownerChampionId.value = 1u;
        def.slot = 3u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Contextual;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 40.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 0.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 40.f;
        def.cost.manaCostByRank[1] = 40.f;
        def.cost.manaCostByRank[2] = 40.f;
        def.cost.manaCostByRank[3] = 40.f;
        def.cost.manaCostByRank[4] = 40.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.flatByRank[1] = 0.f;
        def.effect.damage.flatByRank[2] = 0.f;
        def.effect.damage.flatByRank[3] = 0.f;
        def.effect.damage.flatByRank[4] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(5u);
        def.effect.params[0].id = eSkillEffectParamId::MoveSpeedMul;
        def.effect.params[0].value = 1.1f;
        def.effect.params[1].id = eSkillEffectParamId::ShieldAmountPerRank;
        def.effect.params[1].value = 45.f;
        def.effect.params[2].id = eSkillEffectParamId::ShieldArmorPerRank;
        def.effect.params[2].value = 5.f;
        def.effect.params[3].id = eSkillEffectParamId::ShieldBaseAmount;
        def.effect.params[3].value = 50.f;
        def.effect.params[4].id = eSkillEffectParamId::ShieldDurationSec;
        def.effect.params[4].value = 3.f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.4f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_ANNIE_Q()
    {
        SkillGameplayDef def{};
        def.key = 0x6273AB04u;
        def.id.value = 3u;
        def.ownerChampionId.value = 1u;
        def.slot = 1u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 60.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 6.25f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 60.f;
        def.cost.manaCostByRank[1] = 60.f;
        def.cost.manaCostByRank[2] = 60.f;
        def.cost.manaCostByRank[3] = 60.f;
        def.cost.manaCostByRank[4] = 60.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Magic;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 115.f;
        def.effect.damage.flatByRank[1] = 150.f;
        def.effect.damage.flatByRank[2] = 185.f;
        def.effect.damage.flatByRank[3] = 220.f;
        def.effect.damage.flatByRank[4] = 255.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(3u);
        def.effect.params[0].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[0].value = 80.f;
        def.effect.params[1].id = eSkillEffectParamId::DamagePerRank;
        def.effect.params[1].value = 35.f;
        def.effect.params[2].id = eSkillEffectParamId::StunDurationSec;
        def.effect.params[2].value = 1.25f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 0.5f;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
        def.target.shape[1] = eTargetShape::Unit;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsTarget;
        return def;
    }

    SkillGameplayDef MakeSkill_ANNIE_R()
    {
        SkillGameplayDef def{};
        def.key = 0x6573AFBDu;
        def.id.value = 4u;
        def.ownerChampionId.value = 1u;
        def.slot = 4u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 3u;
        def.cooldown.rankCount = 3u;
        def.cost.manaCost = 100.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 6.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 100.f;
        def.cost.manaCostByRank[1] = 100.f;
        def.cost.manaCostByRank[2] = 100.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 3u;
        def.effect.damage.type = eDamageType::Magic;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 225.f;
        def.effect.damage.flatByRank[1] = 300.f;
        def.effect.damage.flatByRank[2] = 375.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(5u);
        def.effect.params[0].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[0].value = 150.f;
        def.effect.params[1].id = eSkillEffectParamId::DamagePerRank;
        def.effect.params[1].value = 75.f;
        def.effect.params[2].id = eSkillEffectParamId::Radius;
        def.effect.params[2].value = 3.f;
        def.effect.params[3].id = eSkillEffectParamId::Range;
        def.effect.params[3].value = 6.f;
        def.effect.params[4].id = eSkillEffectParamId::StunDurationSec;
        def.effect.params[4].value = 1.25f;
        def.summonPolicy.bValid = true;
        def.summonPolicy.paramCount = static_cast<u8_t>(12u);
        def.summonPolicy.params[0].id = eSummonPolicyParamId::AttackCooldownSec;
        def.summonPolicy.params[0].value = 1.f;
        def.summonPolicy.params[1].id = eSummonPolicyParamId::AttackDamagePerRank;
        def.summonPolicy.params[1].value = 15.f;
        def.summonPolicy.params[2].id = eSummonPolicyParamId::AttackRange;
        def.summonPolicy.params[2].value = 2.2f;
        def.summonPolicy.params[3].id = eSummonPolicyParamId::BaseAttackDamage;
        def.summonPolicy.params[3].value = 40.f;
        def.summonPolicy.params[4].id = eSummonPolicyParamId::BaseHp;
        def.summonPolicy.params[4].value = 1000.f;
        def.summonPolicy.params[5].id = eSummonPolicyParamId::DurationSec;
        def.summonPolicy.params[5].value = 45.f;
        def.summonPolicy.params[6].id = eSummonPolicyParamId::HpPerRank;
        def.summonPolicy.params[6].value = 250.f;
        def.summonPolicy.params[7].id = eSummonPolicyParamId::Lane;
        def.summonPolicy.params[7].value = 255.f;
        def.summonPolicy.params[8].id = eSummonPolicyParamId::MoveSpeed;
        def.summonPolicy.params[8].value = 5.2f;
        def.summonPolicy.params[9].id = eSummonPolicyParamId::Radius;
        def.summonPolicy.params[9].value = 0.9f;
        def.summonPolicy.params[10].id = eSummonPolicyParamId::RoleType;
        def.summonPolicy.params[10].value = 4.f;
        def.summonPolicy.params[11].id = eSummonPolicyParamId::SightRange;
        def.summonPolicy.params[11].value = 14.f;
        def.target.shape[0] = eTargetShape::Ground;
        def.stage.lockDurationSec[0] = 1.2f;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
        def.target.shape[1] = eTargetShape::Ground;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsCommandDirection;
        return def;
    }

    SkillGameplayDef MakeSkill_ANNIE_W()
    {
        SkillGameplayDef def{};
        def.key = 0x6073A7DEu;
        def.id.value = 5u;
        def.ownerChampionId.value = 1u;
        def.slot = 2u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 70.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 6.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 70.f;
        def.cost.manaCostByRank[1] = 70.f;
        def.cost.manaCostByRank[2] = 70.f;
        def.cost.manaCostByRank[3] = 70.f;
        def.cost.manaCostByRank[4] = 70.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Magic;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 115.f;
        def.effect.damage.flatByRank[1] = 160.f;
        def.effect.damage.flatByRank[2] = 205.f;
        def.effect.damage.flatByRank[3] = 250.f;
        def.effect.damage.flatByRank[4] = 295.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(5u);
        def.effect.params[0].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[0].value = 70.f;
        def.effect.params[1].id = eSkillEffectParamId::DamagePerRank;
        def.effect.params[1].value = 45.f;
        def.effect.params[2].id = eSkillEffectParamId::HalfAngleCos;
        def.effect.params[2].value = 0.76604444f;
        def.effect.params[3].id = eSkillEffectParamId::Range;
        def.effect.params[3].value = 6.f;
        def.effect.params[4].id = eSkillEffectParamId::StunDurationSec;
        def.effect.params[4].value = 1.25f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 0.6f;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
        def.target.shape[1] = eTargetShape::Direction;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsCommandDirection;
        return def;
    }

    SkillGameplayDef MakeSkill_ASHE_BASIC_ATTACK()
    {
        SkillGameplayDef def{};
        def.key = 0x314C4E70u;
        def.id.value = 6u;
        def.ownerChampionId.value = 2u;
        def.slot = 0u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 1u;
        def.cooldown.rankCount = 1u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 1.72f;
        def.range.rangeMax = 6.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 1.72f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 1u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_CanCrit | DamageFlag_CanLifesteal | DamageFlag_OnHit;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 1.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(2u);
        def.effect.params[0].id = eSkillEffectParamId::Radius;
        def.effect.params[0].value = 0.35f;
        def.effect.params[1].id = eSkillEffectParamId::Speed;
        def.effect.params[1].value = 18.f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 0.7f;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
        def.target.shape[1] = eTargetShape::Unit;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsTarget;
        return def;
    }

    SkillGameplayDef MakeSkill_ASHE_E()
    {
        SkillGameplayDef def{};
        def.key = 0x468DE912u;
        def.id.value = 7u;
        def.ownerChampionId.value = 2u;
        def.slot = 3u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 400.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cost.manaCostByRank[1] = 0.f;
        def.cost.manaCostByRank[2] = 0.f;
        def.cost.manaCostByRank[3] = 0.f;
        def.cost.manaCostByRank[4] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.flatByRank[1] = 0.f;
        def.effect.damage.flatByRank[2] = 0.f;
        def.effect.damage.flatByRank[3] = 0.f;
        def.effect.damage.flatByRank[4] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(3u);
        def.effect.params[0].id = eSkillEffectParamId::Radius;
        def.effect.params[0].value = 10.f;
        def.effect.params[1].id = eSkillEffectParamId::Range;
        def.effect.params[1].value = 400.f;
        def.effect.params[2].id = eSkillEffectParamId::Speed;
        def.effect.params[2].value = 24.f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 0.5f;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
        def.target.shape[1] = eTargetShape::Direction;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsCommandDirection;
        return def;
    }

    SkillGameplayDef MakeSkill_ASHE_Q()
    {
        SkillGameplayDef def{};
        def.key = 0x528DFBF6u;
        def.id.value = 8u;
        def.ownerChampionId.value = 2u;
        def.slot = 1u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Contextual;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 50.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 0.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 50.f;
        def.cost.manaCostByRank[1] = 50.f;
        def.cost.manaCostByRank[2] = 50.f;
        def.cost.manaCostByRank[3] = 50.f;
        def.cost.manaCostByRank[4] = 50.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 20.f;
        def.effect.damage.flatByRank[1] = 20.f;
        def.effect.damage.flatByRank[2] = 20.f;
        def.effect.damage.flatByRank[3] = 20.f;
        def.effect.damage.flatByRank[4] = 20.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(4u);
        def.effect.params[0].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[0].value = 20.f;
        def.effect.params[1].id = eSkillEffectParamId::EffectDurationSec;
        def.effect.params[1].value = 5.f;
        def.effect.params[2].id = eSkillEffectParamId::MaxStacks;
        def.effect.params[2].value = 4.f;
        def.effect.params[3].id = eSkillEffectParamId::SlowDurationSec;
        def.effect.params[3].value = 1.5f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.5f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_ASHE_R()
    {
        SkillGameplayDef def{};
        def.key = 0x518DFA63u;
        def.id.value = 9u;
        def.ownerChampionId.value = 2u;
        def.slot = 4u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 3u;
        def.cooldown.rankCount = 3u;
        def.cost.manaCost = 100.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 200.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 100.f;
        def.cost.manaCostByRank[1] = 100.f;
        def.cost.manaCostByRank[2] = 100.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 3u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 250.f;
        def.effect.damage.flatByRank[1] = 250.f;
        def.effect.damage.flatByRank[2] = 250.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(4u);
        def.effect.params[0].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[0].value = 250.f;
        def.effect.params[1].id = eSkillEffectParamId::Range;
        def.effect.params[1].value = 200.f;
        def.effect.params[2].id = eSkillEffectParamId::Speed;
        def.effect.params[2].value = 20.f;
        def.effect.params[3].id = eSkillEffectParamId::StunDurationSec;
        def.effect.params[3].value = 3.f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 1.f;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
        def.target.shape[1] = eTargetShape::Direction;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsCommandDirection;
        return def;
    }

    SkillGameplayDef MakeSkill_ASHE_W()
    {
        SkillGameplayDef def{};
        def.key = 0x548DFF1Cu;
        def.id.value = 10u;
        def.ownerChampionId.value = 2u;
        def.slot = 2u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 75.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 9.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 75.f;
        def.cost.manaCostByRank[1] = 75.f;
        def.cost.manaCostByRank[2] = 75.f;
        def.cost.manaCostByRank[3] = 75.f;
        def.cost.manaCostByRank[4] = 75.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 45.f;
        def.effect.damage.flatByRank[1] = 45.f;
        def.effect.damage.flatByRank[2] = 45.f;
        def.effect.damage.flatByRank[3] = 45.f;
        def.effect.damage.flatByRank[4] = 45.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(5u);
        def.effect.params[0].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[0].value = 45.f;
        def.effect.params[1].id = eSkillEffectParamId::MoveSpeedMul;
        def.effect.params[1].value = 0.5f;
        def.effect.params[2].id = eSkillEffectParamId::Range;
        def.effect.params[2].value = 12.f;
        def.effect.params[3].id = eSkillEffectParamId::SlowDurationSec;
        def.effect.params[3].value = 1.5f;
        def.effect.params[4].id = eSkillEffectParamId::Speed;
        def.effect.params[4].value = 24.f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 0.6f;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
        def.target.shape[1] = eTargetShape::Direction;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsCommandDirection;
        return def;
    }

    SkillGameplayDef MakeSkill_EZREAL_BASIC_ATTACK()
    {
        SkillGameplayDef def{};
        def.key = 0x80E66B0Cu;
        def.id.value = 11u;
        def.ownerChampionId.value = 3u;
        def.slot = 0u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 1u;
        def.cooldown.rankCount = 1u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 0.5f;
        def.range.rangeMax = 5.5f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 0.5f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 1u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_CanCrit | DamageFlag_CanLifesteal | DamageFlag_OnHit;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 1.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(4u);
        def.effect.params[0].id = eSkillEffectParamId::BonusAttackSpeed;
        def.effect.params[0].value = 0.1f;
        def.effect.params[1].id = eSkillEffectParamId::MaxStacks;
        def.effect.params[1].value = 5.f;
        def.effect.params[2].id = eSkillEffectParamId::Speed;
        def.effect.params[2].value = 20.f;
        def.effect.params[3].id = eSkillEffectParamId::StackWindowSec;
        def.effect.params[3].value = 6.f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 0.65f;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
        def.target.shape[1] = eTargetShape::Unit;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsTarget;
        return def;
    }

    SkillGameplayDef MakeSkill_EZREAL_E()
    {
        SkillGameplayDef def{};
        def.key = 0x65F08C56u;
        def.id.value = 12u;
        def.ownerChampionId.value = 3u;
        def.slot = 3u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 70.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 4.75f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 70.f;
        def.cost.manaCostByRank[1] = 70.f;
        def.cost.manaCostByRank[2] = 70.f;
        def.cost.manaCostByRank[3] = 70.f;
        def.cost.manaCostByRank[4] = 70.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Magic;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 80.f;
        def.effect.damage.flatByRank[1] = 130.f;
        def.effect.damage.flatByRank[2] = 180.f;
        def.effect.damage.flatByRank[3] = 230.f;
        def.effect.damage.flatByRank[4] = 280.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.6f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.6f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.6f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.6f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.6f;
        def.effect.damage.apRatioByRank[0] = 0.75f;
        def.effect.damage.apRatioByRank[1] = 0.75f;
        def.effect.damage.apRatioByRank[2] = 0.75f;
        def.effect.damage.apRatioByRank[3] = 0.75f;
        def.effect.damage.apRatioByRank[4] = 0.75f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(8u);
        def.effect.params[0].id = eSkillEffectParamId::ApRatio;
        def.effect.params[0].value = 0.75f;
        def.effect.params[1].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[1].value = 80.f;
        def.effect.params[2].id = eSkillEffectParamId::BonusAdRatio;
        def.effect.params[2].value = 0.6f;
        def.effect.params[3].id = eSkillEffectParamId::CastTimeSec;
        def.effect.params[3].value = 0.25f;
        def.effect.params[4].id = eSkillEffectParamId::DamagePerRank;
        def.effect.params[4].value = 50.f;
        def.effect.params[5].id = eSkillEffectParamId::HalfWidth;
        def.effect.params[5].value = 0.5f;
        def.effect.params[6].id = eSkillEffectParamId::Radius;
        def.effect.params[6].value = 7.5f;
        def.effect.params[7].id = eSkillEffectParamId::Speed;
        def.effect.params[7].value = 20.f;
        def.target.shape[0] = eTargetShape::Ground;
        def.stage.lockDurationSec[0] = 0.25f;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
        def.target.shape[1] = eTargetShape::Ground;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsCommandDirection;
        return def;
    }

    SkillGameplayDef MakeSkill_EZREAL_Q()
    {
        SkillGameplayDef def{};
        def.key = 0x59F07972u;
        def.id.value = 13u;
        def.ownerChampionId.value = 3u;
        def.slot = 1u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 28.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 12.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 28.f;
        def.cost.manaCostByRank[1] = 31.f;
        def.cost.manaCostByRank[2] = 34.f;
        def.cost.manaCostByRank[3] = 37.f;
        def.cost.manaCostByRank[4] = 40.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_OnHit;
        def.effect.damage.flatByRank[0] = 20.f;
        def.effect.damage.flatByRank[1] = 45.f;
        def.effect.damage.flatByRank[2] = 70.f;
        def.effect.damage.flatByRank[3] = 95.f;
        def.effect.damage.flatByRank[4] = 120.f;
        def.effect.damage.totalAdRatioByRank[0] = 1.3f;
        def.effect.damage.totalAdRatioByRank[1] = 1.3f;
        def.effect.damage.totalAdRatioByRank[2] = 1.3f;
        def.effect.damage.totalAdRatioByRank[3] = 1.3f;
        def.effect.damage.totalAdRatioByRank[4] = 1.3f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.4f;
        def.effect.damage.apRatioByRank[1] = 0.4f;
        def.effect.damage.apRatioByRank[2] = 0.4f;
        def.effect.damage.apRatioByRank[3] = 0.4f;
        def.effect.damage.apRatioByRank[4] = 0.4f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(8u);
        def.effect.params[0].id = eSkillEffectParamId::ApRatio;
        def.effect.params[0].value = 0.4f;
        def.effect.params[1].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[1].value = 20.f;
        def.effect.params[2].id = eSkillEffectParamId::CastTimeSec;
        def.effect.params[2].value = 0.25f;
        def.effect.params[3].id = eSkillEffectParamId::CooldownRefundSec;
        def.effect.params[3].value = 1.5f;
        def.effect.params[4].id = eSkillEffectParamId::DamagePerRank;
        def.effect.params[4].value = 25.f;
        def.effect.params[5].id = eSkillEffectParamId::HalfWidth;
        def.effect.params[5].value = 0.6f;
        def.effect.params[6].id = eSkillEffectParamId::Speed;
        def.effect.params[6].value = 20.f;
        def.effect.params[7].id = eSkillEffectParamId::TotalAdRatio;
        def.effect.params[7].value = 1.3f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 0.25f;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
        def.target.shape[1] = eTargetShape::Direction;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsCommandDirection;
        return def;
    }

    SkillGameplayDef MakeSkill_EZREAL_R()
    {
        SkillGameplayDef def{};
        def.key = 0x58F077DFu;
        def.id.value = 14u;
        def.ownerChampionId.value = 3u;
        def.slot = 4u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 3u;
        def.cooldown.rankCount = 3u;
        def.cost.manaCost = 100.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 250.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 100.f;
        def.cost.manaCostByRank[1] = 100.f;
        def.cost.manaCostByRank[2] = 100.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 3u;
        def.effect.damage.type = eDamageType::Magic;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 350.f;
        def.effect.damage.flatByRank[1] = 550.f;
        def.effect.damage.flatByRank[2] = 750.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 1.f;
        def.effect.damage.bonusAdRatioByRank[1] = 1.f;
        def.effect.damage.bonusAdRatioByRank[2] = 1.f;
        def.effect.damage.apRatioByRank[0] = 1.1f;
        def.effect.damage.apRatioByRank[1] = 1.1f;
        def.effect.damage.apRatioByRank[2] = 1.1f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(9u);
        def.effect.params[0].id = eSkillEffectParamId::ApRatio;
        def.effect.params[0].value = 1.1f;
        def.effect.params[1].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[1].value = 350.f;
        def.effect.params[2].id = eSkillEffectParamId::BonusAdRatio;
        def.effect.params[2].value = 1.f;
        def.effect.params[3].id = eSkillEffectParamId::CastTimeSec;
        def.effect.params[3].value = 1.f;
        def.effect.params[4].id = eSkillEffectParamId::DamagePerRank;
        def.effect.params[4].value = 200.f;
        def.effect.params[5].id = eSkillEffectParamId::HalfWidth;
        def.effect.params[5].value = 1.6f;
        def.effect.params[6].id = eSkillEffectParamId::NonEpicBaseDamage;
        def.effect.params[6].value = 150.f;
        def.effect.params[7].id = eSkillEffectParamId::NonEpicDamagePerRank;
        def.effect.params[7].value = 75.f;
        def.effect.params[8].id = eSkillEffectParamId::Speed;
        def.effect.params[8].value = 20.f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 1.f;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
        def.target.shape[1] = eTargetShape::Direction;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsCommandDirection;
        return def;
    }

    SkillGameplayDef MakeSkill_EZREAL_W()
    {
        SkillGameplayDef def{};
        def.key = 0x53F07000u;
        def.id.value = 15u;
        def.ownerChampionId.value = 3u;
        def.slot = 2u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 50.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 12.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 50.f;
        def.cost.manaCostByRank[1] = 50.f;
        def.cost.manaCostByRank[2] = 50.f;
        def.cost.manaCostByRank[3] = 50.f;
        def.cost.manaCostByRank[4] = 50.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Magic;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 80.f;
        def.effect.damage.flatByRank[1] = 135.f;
        def.effect.damage.flatByRank[2] = 190.f;
        def.effect.damage.flatByRank[3] = 245.f;
        def.effect.damage.flatByRank[4] = 300.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 1.f;
        def.effect.damage.bonusAdRatioByRank[1] = 1.f;
        def.effect.damage.bonusAdRatioByRank[2] = 1.f;
        def.effect.damage.bonusAdRatioByRank[3] = 1.f;
        def.effect.damage.bonusAdRatioByRank[4] = 1.f;
        def.effect.damage.apRatioByRank[0] = 0.9f;
        def.effect.damage.apRatioByRank[1] = 0.9f;
        def.effect.damage.apRatioByRank[2] = 0.9f;
        def.effect.damage.apRatioByRank[3] = 0.9f;
        def.effect.damage.apRatioByRank[4] = 0.9f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(9u);
        def.effect.params[0].id = eSkillEffectParamId::ApRatio;
        def.effect.params[0].value = 0.9f;
        def.effect.params[1].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[1].value = 80.f;
        def.effect.params[2].id = eSkillEffectParamId::BonusAdRatio;
        def.effect.params[2].value = 1.f;
        def.effect.params[3].id = eSkillEffectParamId::CastTimeSec;
        def.effect.params[3].value = 0.25f;
        def.effect.params[4].id = eSkillEffectParamId::DamagePerRank;
        def.effect.params[4].value = 55.f;
        def.effect.params[5].id = eSkillEffectParamId::HalfWidth;
        def.effect.params[5].value = 0.8f;
        def.effect.params[6].id = eSkillEffectParamId::ManaRestoreFlat;
        def.effect.params[6].value = 60.f;
        def.effect.params[7].id = eSkillEffectParamId::MarkDurationSec;
        def.effect.params[7].value = 4.f;
        def.effect.params[8].id = eSkillEffectParamId::Speed;
        def.effect.params[8].value = 17.f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 0.25f;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
        def.target.shape[1] = eTargetShape::Direction;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsCommandDirection;
        return def;
    }

    SkillGameplayDef MakeSkill_FIORA_BASIC_ATTACK()
    {
        SkillGameplayDef def{};
        def.key = 0xAD7414ACu;
        def.id.value = 16u;
        def.ownerChampionId.value = 4u;
        def.slot = 0u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 1u;
        def.cooldown.rankCount = 1u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 1.67f;
        def.range.rangeMax = 1.5f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 1.67f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 1u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_CanCrit | DamageFlag_CanLifesteal | DamageFlag_OnHit;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 1.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 1.f;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
        def.target.shape[1] = eTargetShape::Unit;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsTarget;
        return def;
    }

    SkillGameplayDef MakeSkill_FIORA_E()
    {
        SkillGameplayDef def{};
        def.key = 0x64CA69B6u;
        def.id.value = 17u;
        def.ownerChampionId.value = 4u;
        def.slot = 3u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Contextual;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 40.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 0.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 40.f;
        def.cost.manaCostByRank[1] = 40.f;
        def.cost.manaCostByRank[2] = 40.f;
        def.cost.manaCostByRank[3] = 40.f;
        def.cost.manaCostByRank[4] = 40.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_OnHit;
        def.effect.damage.flatByRank[0] = 30.f;
        def.effect.damage.flatByRank[1] = 30.f;
        def.effect.damage.flatByRank[2] = 30.f;
        def.effect.damage.flatByRank[3] = 30.f;
        def.effect.damage.flatByRank[4] = 30.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(3u);
        def.effect.params[0].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[0].value = 30.f;
        def.effect.params[1].id = eSkillEffectParamId::EffectDurationSec;
        def.effect.params[1].value = 5.f;
        def.effect.params[2].id = eSkillEffectParamId::MaxStacks;
        def.effect.params[2].value = 2.f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.4f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_FIORA_Q()
    {
        SkillGameplayDef def{};
        def.key = 0x58CA56D2u;
        def.id.value = 18u;
        def.ownerChampionId.value = 4u;
        def.slot = 1u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Contextual;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 20.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 4.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 20.f;
        def.cost.manaCostByRank[1] = 20.f;
        def.cost.manaCostByRank[2] = 20.f;
        def.cost.manaCostByRank[3] = 20.f;
        def.cost.manaCostByRank[4] = 20.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_OnHit;
        def.effect.damage.flatByRank[0] = 70.f;
        def.effect.damage.flatByRank[1] = 70.f;
        def.effect.damage.flatByRank[2] = 70.f;
        def.effect.damage.flatByRank[3] = 70.f;
        def.effect.damage.flatByRank[4] = 70.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(5u);
        def.effect.params[0].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[0].value = 70.f;
        def.effect.params[1].id = eSkillEffectParamId::DashDistance;
        def.effect.params[1].value = 3.f;
        def.effect.params[2].id = eSkillEffectParamId::DashDurationSec;
        def.effect.params[2].value = 0.25f;
        def.effect.params[3].id = eSkillEffectParamId::Radius;
        def.effect.params[3].value = 1.f;
        def.effect.params[4].id = eSkillEffectParamId::Range;
        def.effect.params[4].value = 4.f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.5f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_FIORA_R()
    {
        SkillGameplayDef def{};
        def.key = 0x57CA553Fu;
        def.id.value = 19u;
        def.ownerChampionId.value = 4u;
        def.slot = 4u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 3u;
        def.cooldown.rankCount = 3u;
        def.cost.manaCost = 100.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 5.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 100.f;
        def.cost.manaCostByRank[1] = 100.f;
        def.cost.manaCostByRank[2] = 100.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 3u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 80.f;
        def.effect.damage.flatByRank[1] = 80.f;
        def.effect.damage.flatByRank[2] = 80.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(1u);
        def.effect.params[0].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[0].value = 80.f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 2.f;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
        def.target.shape[1] = eTargetShape::Unit;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsTarget;
        return def;
    }

    SkillGameplayDef MakeSkill_FIORA_W()
    {
        SkillGameplayDef def{};
        def.key = 0x52CA4D60u;
        def.id.value = 20u;
        def.ownerChampionId.value = 4u;
        def.slot = 2u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Contextual;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 50.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 0.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 50.f;
        def.cost.manaCostByRank[1] = 50.f;
        def.cost.manaCostByRank[2] = 50.f;
        def.cost.manaCostByRank[3] = 50.f;
        def.cost.manaCostByRank[4] = 50.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.flatByRank[1] = 0.f;
        def.effect.damage.flatByRank[2] = 0.f;
        def.effect.damage.flatByRank[3] = 0.f;
        def.effect.damage.flatByRank[4] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(4u);
        def.effect.params[0].id = eSkillEffectParamId::MoveSpeedMul;
        def.effect.params[0].value = 0.5f;
        def.effect.params[1].id = eSkillEffectParamId::Radius;
        def.effect.params[1].value = 0.8f;
        def.effect.params[2].id = eSkillEffectParamId::Range;
        def.effect.params[2].value = 6.f;
        def.effect.params[3].id = eSkillEffectParamId::SlowDurationSec;
        def.effect.params[3].value = 1.5f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 1.5f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_GAREN_BASIC_ATTACK()
    {
        SkillGameplayDef def{};
        def.key = 0x9CBD0F3Au;
        def.id.value = 21u;
        def.ownerChampionId.value = 5u;
        def.slot = 0u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 1u;
        def.cooldown.rankCount = 1u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 0.6f;
        def.range.rangeMax = 1.5f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 0.6f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 1u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_CanCrit | DamageFlag_CanLifesteal | DamageFlag_OnHit;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 1.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 1.f;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
        def.target.shape[1] = eTargetShape::Unit;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsTarget;
        return def;
    }

    SkillGameplayDef MakeSkill_GAREN_E()
    {
        SkillGameplayDef def{};
        def.key = 0x0BA68428u;
        def.id.value = 22u;
        def.ownerChampionId.value = 5u;
        def.slot = 3u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Contextual;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 1.65f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cost.manaCostByRank[1] = 0.f;
        def.cost.manaCostByRank[2] = 0.f;
        def.cost.manaCostByRank[3] = 0.f;
        def.cost.manaCostByRank[4] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.flatByRank[1] = 0.f;
        def.effect.damage.flatByRank[2] = 0.f;
        def.effect.damage.flatByRank[3] = 0.f;
        def.effect.damage.flatByRank[4] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 3.f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_GAREN_Q()
    {
        SkillGameplayDef def{};
        def.key = 0xFFA67144u;
        def.id.value = 23u;
        def.ownerChampionId.value = 5u;
        def.slot = 1u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Contextual;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 0.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cost.manaCostByRank[1] = 0.f;
        def.cost.manaCostByRank[2] = 0.f;
        def.cost.manaCostByRank[3] = 0.f;
        def.cost.manaCostByRank[4] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.flatByRank[1] = 0.f;
        def.effect.damage.flatByRank[2] = 0.f;
        def.effect.damage.flatByRank[3] = 0.f;
        def.effect.damage.flatByRank[4] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.6f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_GAREN_R()
    {
        SkillGameplayDef def{};
        def.key = 0x02A675FDu;
        def.id.value = 24u;
        def.ownerChampionId.value = 5u;
        def.slot = 4u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 3u;
        def.cooldown.rankCount = 3u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 4.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cost.manaCostByRank[1] = 0.f;
        def.cost.manaCostByRank[2] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 3u;
        def.effect.damage.type = eDamageType::True;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 150.f;
        def.effect.damage.flatByRank[1] = 300.f;
        def.effect.damage.flatByRank[2] = 450.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.25f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.25f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.25f;
        def.effect.paramCount = static_cast<u8_t>(3u);
        def.effect.params[0].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[0].value = 150.f;
        def.effect.params[1].id = eSkillEffectParamId::DamagePerRank;
        def.effect.params[1].value = 150.f;
        def.effect.params[2].id = eSkillEffectParamId::MissingHealthDamageRatio;
        def.effect.params[2].value = 0.25f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 1.5f;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
        def.target.shape[1] = eTargetShape::Unit;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsTarget;
        return def;
    }

    SkillGameplayDef MakeSkill_GAREN_W()
    {
        SkillGameplayDef def{};
        def.key = 0xFDA66E1Eu;
        def.id.value = 25u;
        def.ownerChampionId.value = 5u;
        def.slot = 2u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Contextual;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 0.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cost.manaCostByRank[1] = 0.f;
        def.cost.manaCostByRank[2] = 0.f;
        def.cost.manaCostByRank[3] = 0.f;
        def.cost.manaCostByRank[4] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.flatByRank[1] = 0.f;
        def.effect.damage.flatByRank[2] = 0.f;
        def.effect.damage.flatByRank[3] = 0.f;
        def.effect.damage.flatByRank[4] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.5f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_IRELIA_BASIC_ATTACK()
    {
        SkillGameplayDef def{};
        def.key = 0x2444750Du;
        def.id.value = 26u;
        def.ownerChampionId.value = 6u;
        def.slot = 0u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 1u;
        def.cooldown.rankCount = 1u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 0.6f;
        def.range.rangeMax = 2.1f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 0.6f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 1u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_CanCrit | DamageFlag_CanLifesteal | DamageFlag_OnHit;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 1.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 0.46f;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
        def.target.shape[1] = eTargetShape::Unit;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsTarget;
        return def;
    }

    SkillGameplayDef MakeSkill_IRELIA_E()
    {
        SkillGameplayDef def{};
        def.key = 0x0B476F15u;
        def.id.value = 27u;
        def.ownerChampionId.value = 6u;
        def.slot = 3u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Contextual;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 50.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 9.f;
        def.stage.stageCount = 2u;
        def.stage.stageWindowSec = 3.5f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 50.f;
        def.cost.manaCostByRank[1] = 50.f;
        def.cost.manaCostByRank[2] = 50.f;
        def.cost.manaCostByRank[3] = 50.f;
        def.cost.manaCostByRank[4] = 50.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 100.f;
        def.effect.damage.flatByRank[1] = 130.f;
        def.effect.damage.flatByRank[2] = 160.f;
        def.effect.damage.flatByRank[3] = 190.f;
        def.effect.damage.flatByRank[4] = 220.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(4u);
        def.effect.params[0].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[0].value = 70.f;
        def.effect.params[1].id = eSkillEffectParamId::DamagePerRank;
        def.effect.params[1].value = 30.f;
        def.effect.params[2].id = eSkillEffectParamId::Radius;
        def.effect.params[2].value = 1.5f;
        def.effect.params[3].id = eSkillEffectParamId::StunDurationSec;
        def.effect.params[3].value = 0.75f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.9f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.45f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_IRELIA_Q()
    {
        SkillGameplayDef def{};
        def.key = 0xF7474F99u;
        def.id.value = 28u;
        def.ownerChampionId.value = 6u;
        def.slot = 1u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Contextual;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 20.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 6.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 20.f;
        def.cost.manaCostByRank[1] = 20.f;
        def.cost.manaCostByRank[2] = 20.f;
        def.cost.manaCostByRank[3] = 20.f;
        def.cost.manaCostByRank[4] = 20.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 70.f;
        def.effect.damage.flatByRank[1] = 95.f;
        def.effect.damage.flatByRank[2] = 120.f;
        def.effect.damage.flatByRank[3] = 145.f;
        def.effect.damage.flatByRank[4] = 170.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(4u);
        def.effect.params[0].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[0].value = 45.f;
        def.effect.params[1].id = eSkillEffectParamId::DamagePerRank;
        def.effect.params[1].value = 25.f;
        def.effect.params[2].id = eSkillEffectParamId::DashDurationSec;
        def.effect.params[2].value = 0.25f;
        def.effect.params[3].id = eSkillEffectParamId::Gap;
        def.effect.params[3].value = 1.35f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.36f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_IRELIA_R()
    {
        SkillGameplayDef def{};
        def.key = 0xF4474AE0u;
        def.id.value = 29u;
        def.ownerChampionId.value = 6u;
        def.slot = 4u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Contextual;
        def.cost.rankCount = 3u;
        def.cooldown.rankCount = 3u;
        def.cost.manaCost = 100.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 12.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 100.f;
        def.cost.manaCostByRank[1] = 100.f;
        def.cost.manaCostByRank[2] = 100.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 3u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 250.f;
        def.effect.damage.flatByRank[1] = 250.f;
        def.effect.damage.flatByRank[2] = 250.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(9u);
        def.effect.params[0].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[0].value = 250.f;
        def.effect.params[1].id = eSkillEffectParamId::DisarmDurationSec;
        def.effect.params[1].value = 1.5f;
        def.effect.params[2].id = eSkillEffectParamId::EffectDurationSec;
        def.effect.params[2].value = 2.5f;
        def.effect.params[3].id = eSkillEffectParamId::MoveSpeedMul;
        def.effect.params[3].value = 0.5f;
        def.effect.params[4].id = eSkillEffectParamId::Range;
        def.effect.params[4].value = 8.f;
        def.effect.params[5].id = eSkillEffectParamId::RectLength;
        def.effect.params[5].value = 5.f;
        def.effect.params[6].id = eSkillEffectParamId::RectWidth;
        def.effect.params[6].value = 7.5f;
        def.effect.params[7].id = eSkillEffectParamId::SlowDurationSec;
        def.effect.params[7].value = 0.5f;
        def.effect.params[8].id = eSkillEffectParamId::Speed;
        def.effect.params[8].value = 15.f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.65f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_IRELIA_W()
    {
        SkillGameplayDef def{};
        def.key = 0xF94752BFu;
        def.id.value = 30u;
        def.ownerChampionId.value = 6u;
        def.slot = 2u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Contextual;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 70.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 0.f;
        def.stage.stageCount = 2u;
        def.stage.stageWindowSec = 4.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 70.f;
        def.cost.manaCostByRank[1] = 70.f;
        def.cost.manaCostByRank[2] = 70.f;
        def.cost.manaCostByRank[3] = 70.f;
        def.cost.manaCostByRank[4] = 70.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 70.f;
        def.effect.damage.flatByRank[1] = 110.f;
        def.effect.damage.flatByRank[2] = 150.f;
        def.effect.damage.flatByRank[3] = 190.f;
        def.effect.damage.flatByRank[4] = 230.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(4u);
        def.effect.params[0].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[0].value = 30.f;
        def.effect.params[1].id = eSkillEffectParamId::DamagePerRank;
        def.effect.params[1].value = 40.f;
        def.effect.params[2].id = eSkillEffectParamId::HalfWidth;
        def.effect.params[2].value = 2.2f;
        def.effect.params[3].id = eSkillEffectParamId::Range;
        def.effect.params[3].value = 6.f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 5.f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.4f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_JAX_BASIC_ATTACK()
    {
        SkillGameplayDef def{};
        def.key = 0x85A49308u;
        def.id.value = 31u;
        def.ownerChampionId.value = 7u;
        def.slot = 0u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 1u;
        def.cooldown.rankCount = 1u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 1.67f;
        def.range.rangeMax = 1.5f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 1.67f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 1u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_CanCrit | DamageFlag_CanLifesteal | DamageFlag_OnHit;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 1.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 1.f;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
        def.target.shape[1] = eTargetShape::Unit;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsTarget;
        return def;
    }

    SkillGameplayDef MakeSkill_JAX_E()
    {
        SkillGameplayDef def{};
        def.key = 0x28121B8Au;
        def.id.value = 32u;
        def.ownerChampionId.value = 7u;
        def.slot = 3u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 50.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 0.f;
        def.stage.stageCount = 2u;
        def.stage.stageWindowSec = 2.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 50.f;
        def.cost.manaCostByRank[1] = 50.f;
        def.cost.manaCostByRank[2] = 50.f;
        def.cost.manaCostByRank[3] = 50.f;
        def.cost.manaCostByRank[4] = 50.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 60.f;
        def.effect.damage.flatByRank[1] = 60.f;
        def.effect.damage.flatByRank[2] = 60.f;
        def.effect.damage.flatByRank[3] = 60.f;
        def.effect.damage.flatByRank[4] = 60.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(3u);
        def.effect.params[0].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[0].value = 60.f;
        def.effect.params[1].id = eSkillEffectParamId::Radius;
        def.effect.params[1].value = 2.2f;
        def.effect.params[2].id = eSkillEffectParamId::StunDurationSec;
        def.effect.params[2].value = 1.f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 2.f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.7f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_JAX_Q()
    {
        SkillGameplayDef def{};
        def.key = 0x34122E6Eu;
        def.id.value = 33u;
        def.ownerChampionId.value = 7u;
        def.slot = 1u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Contextual;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 65.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 7.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 65.f;
        def.cost.manaCostByRank[1] = 65.f;
        def.cost.manaCostByRank[2] = 65.f;
        def.cost.manaCostByRank[3] = 65.f;
        def.cost.manaCostByRank[4] = 65.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 70.f;
        def.effect.damage.flatByRank[1] = 70.f;
        def.effect.damage.flatByRank[2] = 70.f;
        def.effect.damage.flatByRank[3] = 70.f;
        def.effect.damage.flatByRank[4] = 70.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(3u);
        def.effect.params[0].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[0].value = 70.f;
        def.effect.params[1].id = eSkillEffectParamId::DashDurationSec;
        def.effect.params[1].value = 0.22f;
        def.effect.params[2].id = eSkillEffectParamId::Gap;
        def.effect.params[2].value = 1.f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.6f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_JAX_R()
    {
        SkillGameplayDef def{};
        def.key = 0x33122CDBu;
        def.id.value = 34u;
        def.ownerChampionId.value = 7u;
        def.slot = 4u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Contextual;
        def.cost.rankCount = 3u;
        def.cooldown.rankCount = 3u;
        def.cost.manaCost = 100.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 0.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 100.f;
        def.cost.manaCostByRank[1] = 100.f;
        def.cost.manaCostByRank[2] = 100.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 3u;
        def.effect.damage.type = eDamageType::Magic;
        def.effect.damage.flags = DamageFlag_OnHit;
        def.effect.damage.flatByRank[0] = 70.f;
        def.effect.damage.flatByRank[1] = 70.f;
        def.effect.damage.flatByRank[2] = 70.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(3u);
        def.effect.params[0].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[0].value = 70.f;
        def.effect.params[1].id = eSkillEffectParamId::EffectDurationSec;
        def.effect.params[1].value = 8.f;
        def.effect.params[2].id = eSkillEffectParamId::MaxStacks;
        def.effect.params[2].value = 3.f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.6f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_JAX_W()
    {
        SkillGameplayDef def{};
        def.key = 0x36123194u;
        def.id.value = 35u;
        def.ownerChampionId.value = 7u;
        def.slot = 2u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Contextual;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 30.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 0.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 30.f;
        def.cost.manaCostByRank[1] = 30.f;
        def.cost.manaCostByRank[2] = 30.f;
        def.cost.manaCostByRank[3] = 30.f;
        def.cost.manaCostByRank[4] = 30.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Magic;
        def.effect.damage.flags = DamageFlag_OnHit;
        def.effect.damage.flatByRank[0] = 45.f;
        def.effect.damage.flatByRank[1] = 45.f;
        def.effect.damage.flatByRank[2] = 45.f;
        def.effect.damage.flatByRank[3] = 45.f;
        def.effect.damage.flatByRank[4] = 45.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(2u);
        def.effect.params[0].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[0].value = 45.f;
        def.effect.params[1].id = eSkillEffectParamId::EffectDurationSec;
        def.effect.params[1].value = 5.f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.5f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_KALISTA_BASIC_ATTACK()
    {
        SkillGameplayDef def{};
        def.key = 0xDEEE3D6Cu;
        def.id.value = 36u;
        def.ownerChampionId.value = 8u;
        def.slot = 0u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 1u;
        def.cooldown.rankCount = 1u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 0.5f;
        def.range.rangeMax = 5.5f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 0.5f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 1u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_CanCrit | DamageFlag_CanLifesteal | DamageFlag_OnHit;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 1.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 0.6f;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
        def.target.shape[1] = eTargetShape::Unit;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsTarget;
        return def;
    }

    SkillGameplayDef MakeSkill_KALISTA_E()
    {
        SkillGameplayDef def{};
        def.key = 0xE5D91576u;
        def.id.value = 37u;
        def.ownerChampionId.value = 8u;
        def.slot = 3u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 30.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 12.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 30.f;
        def.cost.manaCostByRank[1] = 30.f;
        def.cost.manaCostByRank[2] = 30.f;
        def.cost.manaCostByRank[3] = 30.f;
        def.cost.manaCostByRank[4] = 30.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 20.f;
        def.effect.damage.flatByRank[1] = 20.f;
        def.effect.damage.flatByRank[2] = 20.f;
        def.effect.damage.flatByRank[3] = 20.f;
        def.effect.damage.flatByRank[4] = 20.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(5u);
        def.effect.params[0].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[0].value = 20.f;
        def.effect.params[1].id = eSkillEffectParamId::DamagePerSpear;
        def.effect.params[1].value = 30.f;
        def.effect.params[2].id = eSkillEffectParamId::MoveSpeedMul;
        def.effect.params[2].value = 0.55f;
        def.effect.params[3].id = eSkillEffectParamId::SlowDurationSec;
        def.effect.params[3].value = 2.f;
        def.effect.params[4].id = eSkillEffectParamId::StunDurationSec;
        def.effect.params[4].value = 1.f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.4f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_KALISTA_Q()
    {
        SkillGameplayDef def{};
        def.key = 0xD9D90292u;
        def.id.value = 38u;
        def.ownerChampionId.value = 8u;
        def.slot = 1u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 50.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 16.5f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 50.f;
        def.cost.manaCostByRank[1] = 50.f;
        def.cost.manaCostByRank[2] = 50.f;
        def.cost.manaCostByRank[3] = 50.f;
        def.cost.manaCostByRank[4] = 50.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_OnHit;
        def.effect.damage.flatByRank[0] = 70.f;
        def.effect.damage.flatByRank[1] = 70.f;
        def.effect.damage.flatByRank[2] = 70.f;
        def.effect.damage.flatByRank[3] = 70.f;
        def.effect.damage.flatByRank[4] = 70.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(3u);
        def.effect.params[0].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[0].value = 70.f;
        def.effect.params[1].id = eSkillEffectParamId::Radius;
        def.effect.params[1].value = 0.6f;
        def.effect.params[2].id = eSkillEffectParamId::Speed;
        def.effect.params[2].value = 27.f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 0.3f;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
        def.target.shape[1] = eTargetShape::Direction;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsCommandDirection;
        return def;
    }

    SkillGameplayDef MakeSkill_KALISTA_R()
    {
        SkillGameplayDef def{};
        def.key = 0xD8D900FFu;
        def.id.value = 39u;
        def.ownerChampionId.value = 8u;
        def.slot = 4u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 3u;
        def.cooldown.rankCount = 3u;
        def.cost.manaCost = 100.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 0.f;
        def.stage.stageCount = 2u;
        def.stage.stageWindowSec = 4.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 100.f;
        def.cost.manaCostByRank[1] = 100.f;
        def.cost.manaCostByRank[2] = 100.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 3u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.flatByRank[1] = 0.f;
        def.effect.damage.flatByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(4u);
        def.effect.params[0].id = eSkillEffectParamId::AirborneDurationSec;
        def.effect.params[0].value = 1.f;
        def.effect.params[1].id = eSkillEffectParamId::DashDurationSec;
        def.effect.params[1].value = 0.45f;
        def.effect.params[2].id = eSkillEffectParamId::EffectDurationSec;
        def.effect.params[2].value = 4.f;
        def.effect.params[3].id = eSkillEffectParamId::Radius;
        def.effect.params[3].value = 2.5f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 0.5f;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
        def.target.shape[1] = eTargetShape::Direction;
        def.stage.lockDurationSec[1] = 0.45f;
        def.facing.mode[1] = eSkillFacingMode::TowardsCommandDirection;
        return def;
    }

    SkillGameplayDef MakeSkill_KALISTA_W()
    {
        SkillGameplayDef def{};
        def.key = 0xD3D8F920u;
        def.id.value = 40u;
        def.ownerChampionId.value = 8u;
        def.slot = 2u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 12.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cost.manaCostByRank[1] = 0.f;
        def.cost.manaCostByRank[2] = 0.f;
        def.cost.manaCostByRank[3] = 0.f;
        def.cost.manaCostByRank[4] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.flatByRank[1] = 0.f;
        def.effect.damage.flatByRank[2] = 0.f;
        def.effect.damage.flatByRank[3] = 0.f;
        def.effect.damage.flatByRank[4] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(2u);
        def.effect.params[0].id = eSkillEffectParamId::HalfAngleCos;
        def.effect.params[0].value = 0.8660254f;
        def.effect.params[1].id = eSkillEffectParamId::Range;
        def.effect.params[1].value = 12.f;
        def.summonPolicy.bValid = true;
        def.summonPolicy.paramCount = static_cast<u8_t>(4u);
        def.summonPolicy.params[0].id = eSummonPolicyParamId::DurationSec;
        def.summonPolicy.params[0].value = 12.f;
        def.summonPolicy.params[1].id = eSummonPolicyParamId::MoveSpeed;
        def.summonPolicy.params[1].value = 3.5f;
        def.summonPolicy.params[2].id = eSummonPolicyParamId::Radius;
        def.summonPolicy.params[2].value = 0.45f;
        def.summonPolicy.params[3].id = eSummonPolicyParamId::SightRange;
        def.summonPolicy.params[3].value = 10.f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 0.5f;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
        def.target.shape[1] = eTargetShape::Direction;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsCommandDirection;
        return def;
    }

    SkillGameplayDef MakeSkill_KINDRED_BASIC_ATTACK()
    {
        SkillGameplayDef def{};
        def.key = 0x3303D122u;
        def.id.value = 41u;
        def.ownerChampionId.value = 9u;
        def.slot = 0u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 1u;
        def.cooldown.rankCount = 1u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 0.6f;
        def.range.rangeMax = 5.5f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 0.6f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 1u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_CanCrit | DamageFlag_CanLifesteal | DamageFlag_OnHit;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 1.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 0.6f;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
        def.target.shape[1] = eTargetShape::Unit;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsTarget;
        return def;
    }

    SkillGameplayDef MakeSkill_KINDRED_E()
    {
        SkillGameplayDef def{};
        def.key = 0x466E3DB0u;
        def.id.value = 42u;
        def.ownerChampionId.value = 9u;
        def.slot = 3u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Contextual;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 70.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 5.5f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 70.f;
        def.cost.manaCostByRank[1] = 70.f;
        def.cost.manaCostByRank[2] = 70.f;
        def.cost.manaCostByRank[3] = 70.f;
        def.cost.manaCostByRank[4] = 70.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_OnHit;
        def.effect.damage.flatByRank[0] = 80.f;
        def.effect.damage.flatByRank[1] = 80.f;
        def.effect.damage.flatByRank[2] = 80.f;
        def.effect.damage.flatByRank[3] = 80.f;
        def.effect.damage.flatByRank[4] = 80.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(5u);
        def.effect.params[0].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[0].value = 80.f;
        def.effect.params[1].id = eSkillEffectParamId::MarkDurationSec;
        def.effect.params[1].value = 4.f;
        def.effect.params[2].id = eSkillEffectParamId::MaxStacks;
        def.effect.params[2].value = 3.f;
        def.effect.params[3].id = eSkillEffectParamId::MoveSpeedMul;
        def.effect.params[3].value = 0.65f;
        def.effect.params[4].id = eSkillEffectParamId::SlowDurationSec;
        def.effect.params[4].value = 1.f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.6f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_KINDRED_Q()
    {
        SkillGameplayDef def{};
        def.key = 0x3A6E2ACCu;
        def.id.value = 43u;
        def.ownerChampionId.value = 9u;
        def.slot = 1u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Contextual;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 35.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 5.5f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 35.f;
        def.cost.manaCostByRank[1] = 35.f;
        def.cost.manaCostByRank[2] = 35.f;
        def.cost.manaCostByRank[3] = 35.f;
        def.cost.manaCostByRank[4] = 35.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.flatByRank[1] = 0.f;
        def.effect.damage.flatByRank[2] = 0.f;
        def.effect.damage.flatByRank[3] = 0.f;
        def.effect.damage.flatByRank[4] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.6f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_KINDRED_R()
    {
        SkillGameplayDef def{};
        def.key = 0x3D6E2F85u;
        def.id.value = 44u;
        def.ownerChampionId.value = 9u;
        def.slot = 4u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 3u;
        def.cooldown.rankCount = 3u;
        def.cost.manaCost = 100.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 6.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 100.f;
        def.cost.manaCostByRank[1] = 100.f;
        def.cost.manaCostByRank[2] = 100.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 3u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.flatByRank[1] = 0.f;
        def.effect.damage.flatByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(5u);
        def.effect.params[0].id = eSkillEffectParamId::EffectDurationSec;
        def.effect.params[0].value = 4.f;
        def.effect.params[1].id = eSkillEffectParamId::HealAmountPerRank;
        def.effect.params[1].value = 75.f;
        def.effect.params[2].id = eSkillEffectParamId::HealBaseAmount;
        def.effect.params[2].value = 250.f;
        def.effect.params[3].id = eSkillEffectParamId::MinHealthAmount;
        def.effect.params[3].value = 1.f;
        def.effect.params[4].id = eSkillEffectParamId::Radius;
        def.effect.params[4].value = 6.f;
        def.target.shape[0] = eTargetShape::Ground;
        def.stage.lockDurationSec[0] = 0.6f;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
        def.target.shape[1] = eTargetShape::Ground;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsCommandDirection;
        return def;
    }

    SkillGameplayDef MakeSkill_KINDRED_W()
    {
        SkillGameplayDef def{};
        def.key = 0x386E27A6u;
        def.id.value = 45u;
        def.ownerChampionId.value = 9u;
        def.slot = 2u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Contextual;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 40.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 8.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 40.f;
        def.cost.manaCostByRank[1] = 40.f;
        def.cost.manaCostByRank[2] = 40.f;
        def.cost.manaCostByRank[3] = 40.f;
        def.cost.manaCostByRank[4] = 40.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 35.f;
        def.effect.damage.flatByRank[1] = 35.f;
        def.effect.damage.flatByRank[2] = 35.f;
        def.effect.damage.flatByRank[3] = 35.f;
        def.effect.damage.flatByRank[4] = 35.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(4u);
        def.effect.params[0].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[0].value = 35.f;
        def.effect.params[1].id = eSkillEffectParamId::EffectDurationSec;
        def.effect.params[1].value = 4.f;
        def.effect.params[2].id = eSkillEffectParamId::Radius;
        def.effect.params[2].value = 4.f;
        def.effect.params[3].id = eSkillEffectParamId::TickIntervalSec;
        def.effect.params[3].value = 0.6f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.6f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_LEESIN_BASIC_ATTACK()
    {
        SkillGameplayDef def{};
        def.key = 0x50845B6Du;
        def.id.value = 46u;
        def.ownerChampionId.value = 10u;
        def.slot = 0u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 1u;
        def.cooldown.rankCount = 1u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 0.6f;
        def.range.rangeMax = 1.5f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 0.6f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 1u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_CanCrit | DamageFlag_CanLifesteal | DamageFlag_OnHit;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 1.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 0.6f;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
        def.target.shape[1] = eTargetShape::Unit;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsTarget;
        return def;
    }

    SkillGameplayDef MakeSkill_LEESIN_E()
    {
        SkillGameplayDef def{};
        def.key = 0xE84A04B5u;
        def.id.value = 47u;
        def.ownerChampionId.value = 10u;
        def.slot = 3u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Contextual;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 50.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 4.f;
        def.stage.stageCount = 2u;
        def.stage.stageWindowSec = 3.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 50.f;
        def.cost.manaCostByRank[1] = 50.f;
        def.cost.manaCostByRank[2] = 50.f;
        def.cost.manaCostByRank[3] = 50.f;
        def.cost.manaCostByRank[4] = 50.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.flatByRank[1] = 0.f;
        def.effect.damage.flatByRank[2] = 0.f;
        def.effect.damage.flatByRank[3] = 0.f;
        def.effect.damage.flatByRank[4] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(3u);
        def.effect.params[0].id = eSkillEffectParamId::MoveSpeedMul;
        def.effect.params[0].value = 0.6f;
        def.effect.params[1].id = eSkillEffectParamId::Radius;
        def.effect.params[1].value = 3.5f;
        def.effect.params[2].id = eSkillEffectParamId::SlowDurationSec;
        def.effect.params[2].value = 1.5f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.6f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.45f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_LEESIN_Q()
    {
        SkillGameplayDef def{};
        def.key = 0xD449E539u;
        def.id.value = 48u;
        def.ownerChampionId.value = 10u;
        def.slot = 1u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Contextual;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 50.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 11.f;
        def.stage.stageCount = 2u;
        def.stage.stageWindowSec = 3.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 50.f;
        def.cost.manaCostByRank[1] = 50.f;
        def.cost.manaCostByRank[2] = 50.f;
        def.cost.manaCostByRank[3] = 50.f;
        def.cost.manaCostByRank[4] = 50.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_OnHit;
        def.effect.damage.flatByRank[0] = 55.f;
        def.effect.damage.flatByRank[1] = 80.f;
        def.effect.damage.flatByRank[2] = 105.f;
        def.effect.damage.flatByRank[3] = 130.f;
        def.effect.damage.flatByRank[4] = 155.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(4u);
        def.effect.params[0].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[0].value = 95.f;
        def.effect.params[1].id = eSkillEffectParamId::DashDurationSec;
        def.effect.params[1].value = 0.18f;
        def.effect.params[2].id = eSkillEffectParamId::Gap;
        def.effect.params[2].value = 1.f;
        def.effect.params[3].id = eSkillEffectParamId::MarkDurationSec;
        def.effect.params[3].value = 3.f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.6f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_LEESIN_R()
    {
        SkillGameplayDef def{};
        def.key = 0xD149E080u;
        def.id.value = 49u;
        def.ownerChampionId.value = 10u;
        def.slot = 4u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 3u;
        def.cooldown.rankCount = 3u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 3.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cost.manaCostByRank[1] = 0.f;
        def.cost.manaCostByRank[2] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 3u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 150.f;
        def.effect.damage.flatByRank[1] = 150.f;
        def.effect.damage.flatByRank[2] = 150.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(2u);
        def.effect.params[0].id = eSkillEffectParamId::AirborneDurationSec;
        def.effect.params[0].value = 1.f;
        def.effect.params[1].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[1].value = 150.f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 0.6f;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
        def.target.shape[1] = eTargetShape::Unit;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsTarget;
        return def;
    }

    SkillGameplayDef MakeSkill_LEESIN_W()
    {
        SkillGameplayDef def{};
        def.key = 0xD649E85Fu;
        def.id.value = 50u;
        def.ownerChampionId.value = 10u;
        def.slot = 2u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Contextual;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 50.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 7.f;
        def.stage.stageCount = 2u;
        def.stage.stageWindowSec = 3.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 50.f;
        def.cost.manaCostByRank[1] = 50.f;
        def.cost.manaCostByRank[2] = 50.f;
        def.cost.manaCostByRank[3] = 50.f;
        def.cost.manaCostByRank[4] = 50.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.flatByRank[1] = 0.f;
        def.effect.damage.flatByRank[2] = 0.f;
        def.effect.damage.flatByRank[3] = 0.f;
        def.effect.damage.flatByRank[4] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(4u);
        def.effect.params[0].id = eSkillEffectParamId::DashDurationSec;
        def.effect.params[0].value = 0.18f;
        def.effect.params[1].id = eSkillEffectParamId::Gap;
        def.effect.params[1].value = 0.25f;
        def.effect.params[2].id = eSkillEffectParamId::ShieldBaseAmount;
        def.effect.params[2].value = 80.f;
        def.effect.params[3].id = eSkillEffectParamId::ShieldDurationSec;
        def.effect.params[3].value = 3.f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.6f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.45f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_MASTERYI_BASIC_ATTACK()
    {
        SkillGameplayDef def{};
        def.key = 0x4BB4F7F1u;
        def.id.value = 51u;
        def.ownerChampionId.value = 11u;
        def.slot = 0u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 1u;
        def.cooldown.rankCount = 1u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 0.6f;
        def.range.rangeMax = 1.5f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 0.6f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 1u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_CanCrit | DamageFlag_CanLifesteal | DamageFlag_OnHit;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 1.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 0.6f;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
        def.target.shape[1] = eTargetShape::Unit;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsTarget;
        return def;
    }

    SkillGameplayDef MakeSkill_MASTERYI_E()
    {
        SkillGameplayDef def{};
        def.key = 0x32868FA1u;
        def.id.value = 52u;
        def.ownerChampionId.value = 11u;
        def.slot = 3u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Contextual;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 0.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cost.manaCostByRank[1] = 0.f;
        def.cost.manaCostByRank[2] = 0.f;
        def.cost.manaCostByRank[3] = 0.f;
        def.cost.manaCostByRank[4] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.flatByRank[1] = 0.f;
        def.effect.damage.flatByRank[2] = 0.f;
        def.effect.damage.flatByRank[3] = 0.f;
        def.effect.damage.flatByRank[4] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.6f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_MASTERYI_Q()
    {
        SkillGameplayDef def{};
        def.key = 0x26867CBDu;
        def.id.value = 53u;
        def.ownerChampionId.value = 11u;
        def.slot = 1u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Contextual;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 50.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 6.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 50.f;
        def.cost.manaCostByRank[1] = 50.f;
        def.cost.manaCostByRank[2] = 50.f;
        def.cost.manaCostByRank[3] = 50.f;
        def.cost.manaCostByRank[4] = 50.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.flatByRank[1] = 0.f;
        def.effect.damage.flatByRank[2] = 0.f;
        def.effect.damage.flatByRank[3] = 0.f;
        def.effect.damage.flatByRank[4] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.6f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_MASTERYI_R()
    {
        SkillGameplayDef def{};
        def.key = 0x23867804u;
        def.id.value = 54u;
        def.ownerChampionId.value = 11u;
        def.slot = 4u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 3u;
        def.cooldown.rankCount = 3u;
        def.cost.manaCost = 100.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 0.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 100.f;
        def.cost.manaCostByRank[1] = 100.f;
        def.cost.manaCostByRank[2] = 100.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 3u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.flatByRank[1] = 0.f;
        def.effect.damage.flatByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(3u);
        def.effect.params[0].id = eSkillEffectParamId::BonusAttackSpeed;
        def.effect.params[0].value = 0.25f;
        def.effect.params[1].id = eSkillEffectParamId::EffectDurationSec;
        def.effect.params[1].value = 7.f;
        def.effect.params[2].id = eSkillEffectParamId::MoveSpeedMul;
        def.effect.params[2].value = 1.35f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.6f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_MASTERYI_W()
    {
        SkillGameplayDef def{};
        def.key = 0x2086734Bu;
        def.id.value = 55u;
        def.ownerChampionId.value = 11u;
        def.slot = 2u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Contextual;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 50.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 0.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 50.f;
        def.cost.manaCostByRank[1] = 50.f;
        def.cost.manaCostByRank[2] = 50.f;
        def.cost.manaCostByRank[3] = 50.f;
        def.cost.manaCostByRank[4] = 50.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.flatByRank[1] = 0.f;
        def.effect.damage.flatByRank[2] = 0.f;
        def.effect.damage.flatByRank[3] = 0.f;
        def.effect.damage.flatByRank[4] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.6f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_RIVEN_BASIC_ATTACK()
    {
        SkillGameplayDef def{};
        def.key = 0x6F375023u;
        def.id.value = 56u;
        def.ownerChampionId.value = 12u;
        def.slot = 0u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 1u;
        def.cooldown.rankCount = 1u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 0.6f;
        def.range.rangeMax = 1.5f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 0.6f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 1u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_CanCrit | DamageFlag_CanLifesteal | DamageFlag_OnHit;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 1.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 1.f;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
        def.target.shape[1] = eTargetShape::Unit;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsTarget;
        return def;
    }

    SkillGameplayDef MakeSkill_RIVEN_E()
    {
        SkillGameplayDef def{};
        def.key = 0xDA282BCFu;
        def.id.value = 57u;
        def.ownerChampionId.value = 12u;
        def.slot = 3u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Contextual;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 0.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cost.manaCostByRank[1] = 0.f;
        def.cost.manaCostByRank[2] = 0.f;
        def.cost.manaCostByRank[3] = 0.f;
        def.cost.manaCostByRank[4] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.flatByRank[1] = 0.f;
        def.effect.damage.flatByRank[2] = 0.f;
        def.effect.damage.flatByRank[3] = 0.f;
        def.effect.damage.flatByRank[4] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(2u);
        def.effect.params[0].id = eSkillEffectParamId::ShieldBaseAmount;
        def.effect.params[0].value = 70.f;
        def.effect.params[1].id = eSkillEffectParamId::ShieldDurationSec;
        def.effect.params[1].value = 3.f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.5f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_RIVEN_Q()
    {
        SkillGameplayDef def{};
        def.key = 0xC6280C53u;
        def.id.value = 58u;
        def.ownerChampionId.value = 12u;
        def.slot = 1u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 4.5f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cost.manaCostByRank[1] = 0.f;
        def.cost.manaCostByRank[2] = 0.f;
        def.cost.manaCostByRank[3] = 0.f;
        def.cost.manaCostByRank[4] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.flatByRank[1] = 0.f;
        def.effect.damage.flatByRank[2] = 0.f;
        def.effect.damage.flatByRank[3] = 0.f;
        def.effect.damage.flatByRank[4] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(3u);
        def.effect.params[0].id = eSkillEffectParamId::AirborneDurationSec;
        def.effect.params[0].value = 0.75f;
        def.effect.params[1].id = eSkillEffectParamId::Radius;
        def.effect.params[1].value = 2.25f;
        def.effect.params[2].id = eSkillEffectParamId::StackWindowSec;
        def.effect.params[2].value = 4.f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 0.45f;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
        def.target.shape[1] = eTargetShape::Direction;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsCommandDirection;
        return def;
    }

    SkillGameplayDef MakeSkill_RIVEN_R()
    {
        SkillGameplayDef def{};
        def.key = 0xC7280DE6u;
        def.id.value = 59u;
        def.ownerChampionId.value = 12u;
        def.slot = 4u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 3u;
        def.cooldown.rankCount = 3u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 0.f;
        def.stage.stageCount = 2u;
        def.stage.stageWindowSec = 15.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cost.manaCostByRank[1] = 0.f;
        def.cost.manaCostByRank[2] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 3u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 100.f;
        def.effect.damage.flatByRank[1] = 150.f;
        def.effect.damage.flatByRank[2] = 200.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(6u);
        def.effect.params[0].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[0].value = 100.f;
        def.effect.params[1].id = eSkillEffectParamId::BonusAd;
        def.effect.params[1].value = 20.f;
        def.effect.params[2].id = eSkillEffectParamId::DamagePerRank;
        def.effect.params[2].value = 50.f;
        def.effect.params[3].id = eSkillEffectParamId::EffectDurationSec;
        def.effect.params[3].value = 15.f;
        def.effect.params[4].id = eSkillEffectParamId::HalfAngleCos;
        def.effect.params[4].value = 0.70710678f;
        def.effect.params[5].id = eSkillEffectParamId::Range;
        def.effect.params[5].value = 10.5f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 0.8f;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
        def.target.shape[1] = eTargetShape::Direction;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsCommandDirection;
        return def;
    }

    SkillGameplayDef MakeSkill_RIVEN_W()
    {
        SkillGameplayDef def{};
        def.key = 0xCC2815C5u;
        def.id.value = 60u;
        def.ownerChampionId.value = 12u;
        def.slot = 2u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 0.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cost.manaCostByRank[1] = 0.f;
        def.cost.manaCostByRank[2] = 0.f;
        def.cost.manaCostByRank[3] = 0.f;
        def.cost.manaCostByRank[4] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.flatByRank[1] = 0.f;
        def.effect.damage.flatByRank[2] = 0.f;
        def.effect.damage.flatByRank[3] = 0.f;
        def.effect.damage.flatByRank[4] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(2u);
        def.effect.params[0].id = eSkillEffectParamId::Radius;
        def.effect.params[0].value = 2.5f;
        def.effect.params[1].id = eSkillEffectParamId::StunDurationSec;
        def.effect.params[1].value = 0.75f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.6f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_SYLAS_BASIC_ATTACK()
    {
        SkillGameplayDef def{};
        def.key = 0x1CCBB4A9u;
        def.id.value = 61u;
        def.ownerChampionId.value = 13u;
        def.slot = 0u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 1u;
        def.cooldown.rankCount = 1u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 0.6f;
        def.range.rangeMax = 1.5f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 0.6f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 1u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_CanCrit | DamageFlag_CanLifesteal | DamageFlag_OnHit;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 1.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 0.6f;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
        def.target.shape[1] = eTargetShape::Unit;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsTarget;
        return def;
    }

    SkillGameplayDef MakeSkill_SYLAS_E()
    {
        SkillGameplayDef def{};
        def.key = 0x52638859u;
        def.id.value = 62u;
        def.ownerChampionId.value = 13u;
        def.slot = 3u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 65.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 6.f;
        def.stage.stageCount = 2u;
        def.stage.stageWindowSec = 3.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 65.f;
        def.cost.manaCostByRank[1] = 65.f;
        def.cost.manaCostByRank[2] = 65.f;
        def.cost.manaCostByRank[3] = 65.f;
        def.cost.manaCostByRank[4] = 65.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 65.f;
        def.effect.damage.flatByRank[1] = 90.f;
        def.effect.damage.flatByRank[2] = 115.f;
        def.effect.damage.flatByRank[3] = 140.f;
        def.effect.damage.flatByRank[4] = 165.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(11u);
        def.effect.params[0].id = eSkillEffectParamId::AirborneDurationSec;
        def.effect.params[0].value = 0.75f;
        def.effect.params[1].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[1].value = 65.f;
        def.effect.params[2].id = eSkillEffectParamId::DamagePerRank;
        def.effect.params[2].value = 25.f;
        def.effect.params[3].id = eSkillEffectParamId::DashDistance;
        def.effect.params[3].value = 3.25f;
        def.effect.params[4].id = eSkillEffectParamId::DashDurationSec;
        def.effect.params[4].value = 0.16f;
        def.effect.params[5].id = eSkillEffectParamId::Gap;
        def.effect.params[5].value = 0.85f;
        def.effect.params[6].id = eSkillEffectParamId::MoveSpeedMul;
        def.effect.params[6].value = 0.6f;
        def.effect.params[7].id = eSkillEffectParamId::Radius;
        def.effect.params[7].value = 0.55f;
        def.effect.params[8].id = eSkillEffectParamId::SlowDurationSec;
        def.effect.params[8].value = 1.f;
        def.effect.params[9].id = eSkillEffectParamId::Speed;
        def.effect.params[9].value = 26.f;
        def.effect.params[10].id = eSkillEffectParamId::TargetDashDurationSec;
        def.effect.params[10].value = 0.22f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 0.35f;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
        def.target.shape[1] = eTargetShape::Direction;
        def.stage.lockDurationSec[1] = 0.5f;
        def.facing.mode[1] = eSkillFacingMode::TowardsCommandDirection;
        return def;
    }

    SkillGameplayDef MakeSkill_SYLAS_Q()
    {
        SkillGameplayDef def{};
        def.key = 0x6663A7D5u;
        def.id.value = 63u;
        def.ownerChampionId.value = 13u;
        def.slot = 1u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Contextual;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 55.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 4.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 55.f;
        def.cost.manaCostByRank[1] = 55.f;
        def.cost.manaCostByRank[2] = 55.f;
        def.cost.manaCostByRank[3] = 55.f;
        def.cost.manaCostByRank[4] = 55.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.flatByRank[1] = 0.f;
        def.effect.damage.flatByRank[2] = 0.f;
        def.effect.damage.flatByRank[3] = 0.f;
        def.effect.damage.flatByRank[4] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.55f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_SYLAS_R()
    {
        SkillGameplayDef def{};
        def.key = 0x6363A31Cu;
        def.id.value = 64u;
        def.ownerChampionId.value = 13u;
        def.slot = 4u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 3u;
        def.cooldown.rankCount = 3u;
        def.cost.manaCost = 75.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 10.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 75.f;
        def.cost.manaCostByRank[1] = 75.f;
        def.cost.manaCostByRank[2] = 75.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 3u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.flatByRank[1] = 0.f;
        def.effect.damage.flatByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(2u);
        def.effect.params[0].id = eSkillEffectParamId::EffectDurationSec;
        def.effect.params[0].value = 45.f;
        def.effect.params[1].id = eSkillEffectParamId::Range;
        def.effect.params[1].value = 10.f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 0.8f;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
        def.target.shape[1] = eTargetShape::Unit;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsTarget;
        return def;
    }

    SkillGameplayDef MakeSkill_SYLAS_W()
    {
        SkillGameplayDef def{};
        def.key = 0x60639E63u;
        def.id.value = 65u;
        def.ownerChampionId.value = 13u;
        def.slot = 2u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Contextual;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 65.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 5.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 65.f;
        def.cost.manaCostByRank[1] = 65.f;
        def.cost.manaCostByRank[2] = 65.f;
        def.cost.manaCostByRank[3] = 65.f;
        def.cost.manaCostByRank[4] = 65.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.flatByRank[1] = 0.f;
        def.effect.damage.flatByRank[2] = 0.f;
        def.effect.damage.flatByRank[3] = 0.f;
        def.effect.damage.flatByRank[4] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.45f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_VIEGO_BASIC_ATTACK()
    {
        SkillGameplayDef def{};
        def.key = 0xB53AA0BFu;
        def.id.value = 66u;
        def.ownerChampionId.value = 14u;
        def.slot = 0u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 1u;
        def.cooldown.rankCount = 1u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 0.6f;
        def.range.rangeMax = 1.5f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 0.6f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 1u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_CanCrit | DamageFlag_CanLifesteal | DamageFlag_OnHit;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 1.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 0.75f;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
        def.target.shape[1] = eTargetShape::Unit;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsTarget;
        return def;
    }

    SkillGameplayDef MakeSkill_VIEGO_E()
    {
        SkillGameplayDef def{};
        def.key = 0xF7A7D173u;
        def.id.value = 67u;
        def.ownerChampionId.value = 14u;
        def.slot = 3u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Contextual;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 6.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cost.manaCostByRank[1] = 0.f;
        def.cost.manaCostByRank[2] = 0.f;
        def.cost.manaCostByRank[3] = 0.f;
        def.cost.manaCostByRank[4] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.flatByRank[1] = 0.f;
        def.effect.damage.flatByRank[2] = 0.f;
        def.effect.damage.flatByRank[3] = 0.f;
        def.effect.damage.flatByRank[4] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(4u);
        def.effect.params[0].id = eSkillEffectParamId::EffectDurationSec;
        def.effect.params[0].value = 4.f;
        def.effect.params[1].id = eSkillEffectParamId::Radius;
        def.effect.params[1].value = 6.f;
        def.effect.params[2].id = eSkillEffectParamId::RefreshDurationSec;
        def.effect.params[2].value = 0.25f;
        def.effect.params[3].id = eSkillEffectParamId::TickIntervalSec;
        def.effect.params[3].value = 0.1f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.75f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_VIEGO_Q()
    {
        SkillGameplayDef def{};
        def.key = 0x0BA7F0EFu;
        def.id.value = 68u;
        def.ownerChampionId.value = 14u;
        def.slot = 1u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Contextual;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 5.5f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cost.manaCostByRank[1] = 0.f;
        def.cost.manaCostByRank[2] = 0.f;
        def.cost.manaCostByRank[3] = 0.f;
        def.cost.manaCostByRank[4] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_OnHit;
        def.effect.damage.flatByRank[0] = 65.f;
        def.effect.damage.flatByRank[1] = 65.f;
        def.effect.damage.flatByRank[2] = 65.f;
        def.effect.damage.flatByRank[3] = 65.f;
        def.effect.damage.flatByRank[4] = 65.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(2u);
        def.effect.params[0].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[0].value = 65.f;
        def.effect.params[1].id = eSkillEffectParamId::Radius;
        def.effect.params[1].value = 0.9f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.6f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_VIEGO_R()
    {
        SkillGameplayDef def{};
        def.key = 0x0CA7F282u;
        def.id.value = 69u;
        def.ownerChampionId.value = 14u;
        def.slot = 4u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 3u;
        def.cooldown.rankCount = 3u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 6.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cost.manaCostByRank[1] = 0.f;
        def.cost.manaCostByRank[2] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 3u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 150.f;
        def.effect.damage.flatByRank[1] = 150.f;
        def.effect.damage.flatByRank[2] = 150.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(5u);
        def.effect.params[0].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[0].value = 150.f;
        def.effect.params[1].id = eSkillEffectParamId::DashDurationSec;
        def.effect.params[1].value = 0.18f;
        def.effect.params[2].id = eSkillEffectParamId::MoveSpeedMul;
        def.effect.params[2].value = 0.6f;
        def.effect.params[3].id = eSkillEffectParamId::Radius;
        def.effect.params[3].value = 2.f;
        def.effect.params[4].id = eSkillEffectParamId::SlowDurationSec;
        def.effect.params[4].value = 1.f;
        def.target.shape[0] = eTargetShape::Ground;
        def.stage.lockDurationSec[0] = 0.8f;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
        def.target.shape[1] = eTargetShape::Ground;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsCommandDirection;
        return def;
    }

    SkillGameplayDef MakeSkill_VIEGO_W()
    {
        SkillGameplayDef def{};
        def.key = 0x09A7EDC9u;
        def.id.value = 70u;
        def.ownerChampionId.value = 14u;
        def.slot = 2u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 4.f;
        def.stage.stageCount = 2u;
        def.stage.stageWindowSec = 4.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cost.manaCostByRank[1] = 0.f;
        def.cost.manaCostByRank[2] = 0.f;
        def.cost.manaCostByRank[3] = 0.f;
        def.cost.manaCostByRank[4] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 55.f;
        def.effect.damage.flatByRank[1] = 55.f;
        def.effect.damage.flatByRank[2] = 55.f;
        def.effect.damage.flatByRank[3] = 55.f;
        def.effect.damage.flatByRank[4] = 55.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(4u);
        def.effect.params[0].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[0].value = 55.f;
        def.effect.params[1].id = eSkillEffectParamId::DashDurationSec;
        def.effect.params[1].value = 0.26f;
        def.effect.params[2].id = eSkillEffectParamId::Radius;
        def.effect.params[2].value = 0.75f;
        def.effect.params[3].id = eSkillEffectParamId::StunDurationSec;
        def.effect.params[3].value = 0.75f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 0.7f;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
        def.target.shape[1] = eTargetShape::Direction;
        def.stage.lockDurationSec[1] = 0.3f;
        def.facing.mode[1] = eSkillFacingMode::TowardsCommandDirection;
        return def;
    }

    SkillGameplayDef MakeSkill_YASUO_BASIC_ATTACK()
    {
        SkillGameplayDef def{};
        def.key = 0x1036D0FCu;
        def.id.value = 71u;
        def.ownerChampionId.value = 15u;
        def.slot = 0u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 1u;
        def.cooldown.rankCount = 1u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 0.6f;
        def.range.rangeMax = 2.5f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 0.6f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 1u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_CanCrit | DamageFlag_CanLifesteal | DamageFlag_OnHit;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 1.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 0.5f;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
        def.target.shape[1] = eTargetShape::Unit;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsTarget;
        return def;
    }

    SkillGameplayDef MakeSkill_YASUO_E()
    {
        SkillGameplayDef def{};
        def.key = 0xF404D5E6u;
        def.id.value = 72u;
        def.ownerChampionId.value = 15u;
        def.slot = 3u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Contextual;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 4.75f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cost.manaCostByRank[1] = 0.f;
        def.cost.manaCostByRank[2] = 0.f;
        def.cost.manaCostByRank[3] = 0.f;
        def.cost.manaCostByRank[4] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Magic;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 80.f;
        def.effect.damage.flatByRank[1] = 80.f;
        def.effect.damage.flatByRank[2] = 80.f;
        def.effect.damage.flatByRank[3] = 80.f;
        def.effect.damage.flatByRank[4] = 80.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(6u);
        def.effect.params[0].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[0].value = 80.f;
        def.effect.params[1].id = eSkillEffectParamId::DashDistance;
        def.effect.params[1].value = 5.5f;
        def.effect.params[2].id = eSkillEffectParamId::DashDurationSec;
        def.effect.params[2].value = 0.25f;
        def.effect.params[3].id = eSkillEffectParamId::EffectDurationSec;
        def.effect.params[3].value = 0.5f;
        def.effect.params[4].id = eSkillEffectParamId::Gap;
        def.effect.params[4].value = 0.75f;
        def.effect.params[5].id = eSkillEffectParamId::MarkDurationSec;
        def.effect.params[5].value = 10.f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.4f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_YASUO_Q()
    {
        SkillGameplayDef def{};
        def.key = 0x0804F562u;
        def.id.value = 73u;
        def.ownerChampionId.value = 15u;
        def.slot = 1u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 5.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cost.manaCostByRank[1] = 0.f;
        def.cost.manaCostByRank[2] = 0.f;
        def.cost.manaCostByRank[3] = 0.f;
        def.cost.manaCostByRank[4] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_OnHit;
        def.effect.damage.flatByRank[0] = 60.f;
        def.effect.damage.flatByRank[1] = 60.f;
        def.effect.damage.flatByRank[2] = 60.f;
        def.effect.damage.flatByRank[3] = 60.f;
        def.effect.damage.flatByRank[4] = 60.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(12u);
        def.effect.params[0].id = eSkillEffectParamId::AirborneDurationSec;
        def.effect.params[0].value = 1.25f;
        def.effect.params[1].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[1].value = 60.f;
        def.effect.params[2].id = eSkillEffectParamId::DashAreaDamage;
        def.effect.params[2].value = 70.f;
        def.effect.params[3].id = eSkillEffectParamId::DashAreaRadius;
        def.effect.params[3].value = 2.5f;
        def.effect.params[4].id = eSkillEffectParamId::EffectDurationSec;
        def.effect.params[4].value = 0.5f;
        def.effect.params[5].id = eSkillEffectParamId::Radius;
        def.effect.params[5].value = 0.8f;
        def.effect.params[6].id = eSkillEffectParamId::Speed;
        def.effect.params[6].value = 25.f;
        def.effect.params[7].id = eSkillEffectParamId::StackWindowSec;
        def.effect.params[7].value = 6.f;
        def.effect.params[8].id = eSkillEffectParamId::TornadoDamage;
        def.effect.params[8].value = 100.f;
        def.effect.params[9].id = eSkillEffectParamId::TornadoDurationSec;
        def.effect.params[9].value = 1.5f;
        def.effect.params[10].id = eSkillEffectParamId::TornadoRadius;
        def.effect.params[10].value = 2.25f;
        def.effect.params[11].id = eSkillEffectParamId::TornadoSpeed;
        def.effect.params[11].value = 18.f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 0.5f;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
        def.target.shape[1] = eTargetShape::Direction;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsCommandDirection;
        return def;
    }

    SkillGameplayDef MakeSkill_YASUO_R()
    {
        SkillGameplayDef def{};
        def.key = 0x0704F3CFu;
        def.id.value = 74u;
        def.ownerChampionId.value = 15u;
        def.slot = 4u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Contextual;
        def.cost.rankCount = 3u;
        def.cooldown.rankCount = 3u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 14.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cost.manaCostByRank[1] = 0.f;
        def.cost.manaCostByRank[2] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 3u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 200.f;
        def.effect.damage.flatByRank[1] = 200.f;
        def.effect.damage.flatByRank[2] = 200.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(3u);
        def.effect.params[0].id = eSkillEffectParamId::AirborneDurationSec;
        def.effect.params[0].value = 1.f;
        def.effect.params[1].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[1].value = 200.f;
        def.effect.params[2].id = eSkillEffectParamId::Gap;
        def.effect.params[2].value = 1.1f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.6f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_YASUO_W()
    {
        SkillGameplayDef def{};
        def.key = 0x0204EBF0u;
        def.id.value = 75u;
        def.ownerChampionId.value = 15u;
        def.slot = 2u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 4.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cost.manaCostByRank[1] = 0.f;
        def.cost.manaCostByRank[2] = 0.f;
        def.cost.manaCostByRank[3] = 0.f;
        def.cost.manaCostByRank[4] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.flatByRank[1] = 0.f;
        def.effect.damage.flatByRank[2] = 0.f;
        def.effect.damage.flatByRank[3] = 0.f;
        def.effect.damage.flatByRank[4] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(5u);
        def.effect.params[0].id = eSkillEffectParamId::EffectDurationSec;
        def.effect.params[0].value = 4.f;
        def.effect.params[1].id = eSkillEffectParamId::FormationDelaySec;
        def.effect.params[1].value = 0.25f;
        def.effect.params[2].id = eSkillEffectParamId::RectLength;
        def.effect.params[2].value = 1.6f;
        def.effect.params[3].id = eSkillEffectParamId::RectLengthPerRank;
        def.effect.params[3].value = 0.35f;
        def.effect.params[4].id = eSkillEffectParamId::RectWidth;
        def.effect.params[4].value = 0.5f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 0.25f;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
        def.target.shape[1] = eTargetShape::Direction;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsCommandDirection;
        return def;
    }

    SkillGameplayDef MakeSkill_YONE_BASIC_ATTACK()
    {
        SkillGameplayDef def{};
        def.key = 0xE5C4B21Au;
        def.id.value = 76u;
        def.ownerChampionId.value = 16u;
        def.slot = 0u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 1u;
        def.cooldown.rankCount = 1u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 0.75f;
        def.range.rangeMax = 1.5f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 0.75f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 1u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_CanCrit | DamageFlag_CanLifesteal | DamageFlag_OnHit;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 1.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 0.9f;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
        def.target.shape[1] = eTargetShape::Unit;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsTarget;
        return def;
    }

    SkillGameplayDef MakeSkill_YONE_E()
    {
        SkillGameplayDef def{};
        def.key = 0x9A0044C8u;
        def.id.value = 77u;
        def.ownerChampionId.value = 16u;
        def.slot = 3u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Contextual;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 4.f;
        def.stage.stageCount = 2u;
        def.stage.stageWindowSec = 5.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cost.manaCostByRank[1] = 0.f;
        def.cost.manaCostByRank[2] = 0.f;
        def.cost.manaCostByRank[3] = 0.f;
        def.cost.manaCostByRank[4] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.flatByRank[1] = 0.f;
        def.effect.damage.flatByRank[2] = 0.f;
        def.effect.damage.flatByRank[3] = 0.f;
        def.effect.damage.flatByRank[4] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(3u);
        def.effect.params[0].id = eSkillEffectParamId::DashDistance;
        def.effect.params[0].value = 4.f;
        def.effect.params[1].id = eSkillEffectParamId::DashDurationSec;
        def.effect.params[1].value = 0.25f;
        def.effect.params[2].id = eSkillEffectParamId::EffectDurationSec;
        def.effect.params[2].value = 5.f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.75f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_YONE_Q()
    {
        SkillGameplayDef def{};
        def.key = 0x8E0031E4u;
        def.id.value = 78u;
        def.ownerChampionId.value = 16u;
        def.slot = 1u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Contextual;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 4.75f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cost.manaCostByRank[1] = 0.f;
        def.cost.manaCostByRank[2] = 0.f;
        def.cost.manaCostByRank[3] = 0.f;
        def.cost.manaCostByRank[4] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_OnHit;
        def.effect.damage.flatByRank[0] = 75.f;
        def.effect.damage.flatByRank[1] = 75.f;
        def.effect.damage.flatByRank[2] = 75.f;
        def.effect.damage.flatByRank[3] = 75.f;
        def.effect.damage.flatByRank[4] = 75.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(2u);
        def.effect.params[0].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[0].value = 75.f;
        def.effect.params[1].id = eSkillEffectParamId::Radius;
        def.effect.params[1].value = 0.85f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.9f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_YONE_R()
    {
        SkillGameplayDef def{};
        def.key = 0x9100369Du;
        def.id.value = 79u;
        def.ownerChampionId.value = 16u;
        def.slot = 4u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 3u;
        def.cooldown.rankCount = 3u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 10.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cost.manaCostByRank[1] = 0.f;
        def.cost.manaCostByRank[2] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 3u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 150.f;
        def.effect.damage.flatByRank[1] = 150.f;
        def.effect.damage.flatByRank[2] = 150.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(6u);
        def.effect.params[0].id = eSkillEffectParamId::AirborneDurationSec;
        def.effect.params[0].value = 0.75f;
        def.effect.params[1].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[1].value = 150.f;
        def.effect.params[2].id = eSkillEffectParamId::DashDelaySec;
        def.effect.params[2].value = 0.5f;
        def.effect.params[3].id = eSkillEffectParamId::DashDurationSec;
        def.effect.params[3].value = 0.16f;
        def.effect.params[4].id = eSkillEffectParamId::Gap;
        def.effect.params[4].value = 0.75f;
        def.effect.params[5].id = eSkillEffectParamId::Radius;
        def.effect.params[5].value = 1.7f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 1.2f;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
        def.target.shape[1] = eTargetShape::Direction;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsCommandDirection;
        return def;
    }

    SkillGameplayDef MakeSkill_YONE_W()
    {
        SkillGameplayDef def{};
        def.key = 0x8C002EBEu;
        def.id.value = 80u;
        def.ownerChampionId.value = 16u;
        def.slot = 2u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Contextual;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 6.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cost.manaCostByRank[1] = 0.f;
        def.cost.manaCostByRank[2] = 0.f;
        def.cost.manaCostByRank[3] = 0.f;
        def.cost.manaCostByRank[4] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 65.f;
        def.effect.damage.flatByRank[1] = 65.f;
        def.effect.damage.flatByRank[2] = 65.f;
        def.effect.damage.flatByRank[3] = 65.f;
        def.effect.damage.flatByRank[4] = 65.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(2u);
        def.effect.params[0].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[0].value = 65.f;
        def.effect.params[1].id = eSkillEffectParamId::Radius;
        def.effect.params[1].value = 1.5f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.9f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_ZED_BASIC_ATTACK()
    {
        SkillGameplayDef def{};
        def.key = 0x509039F8u;
        def.id.value = 81u;
        def.ownerChampionId.value = 17u;
        def.slot = 0u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 1u;
        def.cooldown.rankCount = 1u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 0.5f;
        def.range.rangeMax = 1.5f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 0.5f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 1u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_CanCrit | DamageFlag_CanLifesteal | DamageFlag_OnHit;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 1.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 1.f;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
        def.target.shape[1] = eTargetShape::Unit;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsTarget;
        return def;
    }

    SkillGameplayDef MakeSkill_ZED_E()
    {
        SkillGameplayDef def{};
        def.key = 0x9CC03A1Au;
        def.id.value = 82u;
        def.ownerChampionId.value = 17u;
        def.slot = 3u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Contextual;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 50.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 2.5f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 50.f;
        def.cost.manaCostByRank[1] = 50.f;
        def.cost.manaCostByRank[2] = 50.f;
        def.cost.manaCostByRank[3] = 50.f;
        def.cost.manaCostByRank[4] = 50.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 65.f;
        def.effect.damage.flatByRank[1] = 85.f;
        def.effect.damage.flatByRank[2] = 105.f;
        def.effect.damage.flatByRank[3] = 125.f;
        def.effect.damage.flatByRank[4] = 145.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(5u);
        def.effect.params[0].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[0].value = 65.f;
        def.effect.params[1].id = eSkillEffectParamId::DamagePerRank;
        def.effect.params[1].value = 20.f;
        def.effect.params[2].id = eSkillEffectParamId::MoveSpeedMul;
        def.effect.params[2].value = 0.6f;
        def.effect.params[3].id = eSkillEffectParamId::Radius;
        def.effect.params[3].value = 2.75f;
        def.effect.params[4].id = eSkillEffectParamId::SlowDurationSec;
        def.effect.params[4].value = 1.5f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.6f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_ZED_Q()
    {
        SkillGameplayDef def{};
        def.key = 0x88C01A9Eu;
        def.id.value = 83u;
        def.ownerChampionId.value = 17u;
        def.slot = 1u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Contextual;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 75.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 9.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 75.f;
        def.cost.manaCostByRank[1] = 75.f;
        def.cost.manaCostByRank[2] = 75.f;
        def.cost.manaCostByRank[3] = 75.f;
        def.cost.manaCostByRank[4] = 75.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 70.f;
        def.effect.damage.flatByRank[1] = 95.f;
        def.effect.damage.flatByRank[2] = 120.f;
        def.effect.damage.flatByRank[3] = 145.f;
        def.effect.damage.flatByRank[4] = 170.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(4u);
        def.effect.params[0].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[0].value = 70.f;
        def.effect.params[1].id = eSkillEffectParamId::DamagePerRank;
        def.effect.params[1].value = 25.f;
        def.effect.params[2].id = eSkillEffectParamId::Radius;
        def.effect.params[2].value = 0.45f;
        def.effect.params[3].id = eSkillEffectParamId::Speed;
        def.effect.params[3].value = 24.f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.7f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SkillGameplayDef MakeSkill_ZED_R()
    {
        SkillGameplayDef def{};
        def.key = 0x87C0190Bu;
        def.id.value = 84u;
        def.ownerChampionId.value = 17u;
        def.slot = 4u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 3u;
        def.cooldown.rankCount = 3u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 6.25f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cost.manaCostByRank[1] = 0.f;
        def.cost.manaCostByRank[2] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 3u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.flatByRank[1] = 0.f;
        def.effect.damage.flatByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.3f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.3f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.3f;
        def.effect.paramCount = static_cast<u8_t>(5u);
        def.effect.params[0].id = eSkillEffectParamId::EffectDurationSec;
        def.effect.params[0].value = 5.f;
        def.effect.params[1].id = eSkillEffectParamId::Gap;
        def.effect.params[1].value = 0.75f;
        def.effect.params[2].id = eSkillEffectParamId::MarkDurationSec;
        def.effect.params[2].value = 3.f;
        def.effect.params[3].id = eSkillEffectParamId::MissingHealthDamageRatio;
        def.effect.params[3].value = 0.3f;
        def.effect.params[4].id = eSkillEffectParamId::VanishDurationSec;
        def.effect.params[4].value = 0.75f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 1.5f;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
        def.target.shape[1] = eTargetShape::Unit;
        def.stage.lockDurationSec[1] = 0.6f;
        def.facing.mode[1] = eSkillFacingMode::TowardsTarget;
        return def;
    }

    SkillGameplayDef MakeSkill_ZED_W()
    {
        SkillGameplayDef def{};
        def.key = 0x8AC01DC4u;
        def.id.value = 85u;
        def.ownerChampionId.value = 17u;
        def.slot = 2u;
        def.legacySkillId = 0u;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Contextual;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 40.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 6.5f;
        def.stage.stageCount = 2u;
        def.stage.stageWindowSec = 5.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 40.f;
        def.cost.manaCostByRank[1] = 40.f;
        def.cost.manaCostByRank[2] = 40.f;
        def.cost.manaCostByRank[3] = 40.f;
        def.cost.manaCostByRank[4] = 40.f;
        def.cooldown.cooldownSecByRank[0] = 3.f;
        def.cooldown.cooldownSecByRank[1] = 3.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 0.f;
        def.effect.damage.flatByRank[1] = 0.f;
        def.effect.damage.flatByRank[2] = 0.f;
        def.effect.damage.flatByRank[3] = 0.f;
        def.effect.damage.flatByRank[4] = 0.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.totalAdRatioByRank[3] = 0.f;
        def.effect.damage.totalAdRatioByRank[4] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[3] = 0.f;
        def.effect.damage.apRatioByRank[4] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[4] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[3] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[4] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(1u);
        def.effect.params[0].id = eSkillEffectParamId::EffectDurationSec;
        def.effect.params[0].value = 5.f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.5f;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.25f;
        def.facing.mode[1] = eSkillFacingMode::None;
        return def;
    }

    SummonerSpellGameplayDef MakeSummonerSpell_4()
    {
        SummonerSpellGameplayDef def{};
        def.key = 0xAB862F55u;
        def.id.value = 1u;
        def.legacySpellId = 4u;
        def.rangeMax = 4.25f;
        def.cooldownSec = 300.f;
        def.gameplayPolicyId = 0u;
        def.replicatedCueId = 0u;
        return def;
    }

    SpawnLoadoutPolicyDef MakeSpawnLoadoutPolicy()
    {
        SpawnLoadoutPolicyDef def{};
        def.startGold = 10000u;
        def.startLevel = static_cast<u8_t>(6u);
        def.startRune = eRuneId::LethalTempo;
        def.startRuneCount = static_cast<u8_t>(0u);
        def.respawnDelaySec = 3.f;
        return def;
    }

    ChampionColliderProfileDef MakeChampionColliderProfile()
    {
        ChampionColliderProfileDef def{};
        def.bodyHeight = 1.8f;
        def.bodyOffsetY = 0.9f;
        return def;
    }

    StructureGameDef MakeStructureGameDef()
    {
        StructureGameDef def{};
        def.turretMaxHp = 3000.f;
        def.inhibitorMaxHp = 4000.f;
        def.nexusMaxHp = 5500.f;
        def.turretAI.attackRange = 7.75f;
        def.turretAI.attackCooldownMax = 1.f;
        def.turretAI.attackDamage = 150.f;
        def.turretAI.nexusAttackDamage = 180.f;
        def.turretAI.projectileSpeed = 18.f;
        def.turretAI.turretSightRange = 12.f;
        def.turretAI.structureSightRange = 10.f;
        def.turretAI.bodyHeight = 2.5f;
        def.turretAI.bodyOffsetY = 1.25f;
        return def;
    }

    JungleCampGameDef MakeDefaultJungleCamp()
    {
        JungleCampGameDef def{};
        def.maxHp = 450.f;
        def.radius = 1.f;
        def.attackRange = 1.7f;
        def.attackDamage = 40.f;
        def.attackCooldown = 1.4f;
        def.moveSpeed = 4.f;
        def.baseArmor = 20.f;
        def.baseMr = 20.f;
        def.aggroRange = 3.f;
        def.leashRange = 9.f;
        return def;
    }

    JungleCampGameDef MakeJungleCamp_0()
    {
        JungleCampGameDef def{};
        def.maxHp = 8000.f;
        def.radius = 2.5f;
        def.attackRange = 4.f;
        def.attackDamage = 120.f;
        def.attackCooldown = 1.2f;
        def.moveSpeed = 2.5f;
        def.baseArmor = 20.f;
        def.baseMr = 20.f;
        def.aggroRange = 2.5f;
        def.leashRange = 8.f;
        return def;
    }

    JungleCampGameDef MakeJungleCamp_1()
    {
        JungleCampGameDef def{};
        def.maxHp = 5000.f;
        def.radius = 2.2f;
        def.attackRange = 3.f;
        def.attackDamage = 90.f;
        def.attackCooldown = 1.5f;
        def.moveSpeed = 4.f;
        def.baseArmor = 20.f;
        def.baseMr = 20.f;
        def.aggroRange = 2.5f;
        def.leashRange = 8.f;
        return def;
    }

    JungleCampGameDef MakeJungleCamp_2()
    {
        JungleCampGameDef def{};
        def.maxHp = 2300.f;
        def.radius = 1.2f;
        def.attackRange = 2.f;
        def.attackDamage = 65.f;
        def.attackCooldown = 1.4f;
        def.moveSpeed = 4.f;
        def.baseArmor = 20.f;
        def.baseMr = 20.f;
        def.aggroRange = 3.f;
        def.leashRange = 9.f;
        return def;
    }

    JungleCampGameDef MakeJungleCamp_3()
    {
        JungleCampGameDef def{};
        def.maxHp = 2300.f;
        def.radius = 1.2f;
        def.attackRange = 2.f;
        def.attackDamage = 65.f;
        def.attackCooldown = 1.4f;
        def.moveSpeed = 4.f;
        def.baseArmor = 20.f;
        def.baseMr = 20.f;
        def.aggroRange = 3.f;
        def.leashRange = 9.f;
        return def;
    }

    JungleCampGameDef MakeJungleCamp_4()
    {
        JungleCampGameDef def{};
        def.maxHp = 450.f;
        def.radius = 1.f;
        def.attackRange = 1.6f;
        def.attackDamage = 40.f;
        def.attackCooldown = 1.6f;
        def.moveSpeed = 3.8f;
        def.baseArmor = 20.f;
        def.baseMr = 20.f;
        def.aggroRange = 3.f;
        def.leashRange = 9.f;
        return def;
    }

    JungleCampGameDef MakeJungleCamp_5()
    {
        JungleCampGameDef def{};
        def.maxHp = 450.f;
        def.radius = 1.2f;
        def.attackRange = 2.f;
        def.attackDamage = 40.f;
        def.attackCooldown = 1.4f;
        def.moveSpeed = 4.f;
        def.baseArmor = 20.f;
        def.baseMr = 20.f;
        def.aggroRange = 3.f;
        def.leashRange = 9.f;
        return def;
    }

    JungleCampGameDef MakeJungleCamp_6()
    {
        JungleCampGameDef def{};
        def.maxHp = 450.f;
        def.radius = 0.9f;
        def.attackRange = 1.5f;
        def.attackDamage = 40.f;
        def.attackCooldown = 1.3f;
        def.moveSpeed = 4.6f;
        def.baseArmor = 20.f;
        def.baseMr = 20.f;
        def.aggroRange = 3.f;
        def.leashRange = 9.f;
        return def;
    }

    JungleCampGameDef MakeJungleCamp_7()
    {
        JungleCampGameDef def{};
        def.maxHp = 450.f;
        def.radius = 0.8f;
        def.attackRange = 1.5f;
        def.attackDamage = 40.f;
        def.attackCooldown = 1.2f;
        def.moveSpeed = 4.4f;
        def.baseArmor = 20.f;
        def.baseMr = 20.f;
        def.aggroRange = 3.f;
        def.leashRange = 9.f;
        return def;
    }

    JungleCampGameDef MakeJungleCamp_8()
    {
        JungleCampGameDef def{};
        def.maxHp = 450.f;
        def.radius = 0.7f;
        def.attackRange = 1.4f;
        def.attackDamage = 40.f;
        def.attackCooldown = 1.25f;
        def.moveSpeed = 4.5f;
        def.baseArmor = 20.f;
        def.baseMr = 20.f;
        def.aggroRange = 3.f;
        def.leashRange = 9.f;
        return def;
    }

    JungleCampGameDef MakeJungleCamp_9()
    {
        JungleCampGameDef def{};
        def.maxHp = 450.f;
        def.radius = 0.7f;
        def.attackRange = 1.4f;
        def.attackDamage = 40.f;
        def.attackCooldown = 1.25f;
        def.moveSpeed = 4.5f;
        def.baseArmor = 20.f;
        def.baseMr = 20.f;
        def.aggroRange = 3.f;
        def.leashRange = 9.f;
        return def;
    }

    JungleCampGameDef MakeJungleCamp_10()
    {
        JungleCampGameDef def{};
        def.maxHp = 450.f;
        def.radius = 0.7f;
        def.attackRange = 1.4f;
        def.attackDamage = 40.f;
        def.attackCooldown = 1.25f;
        def.moveSpeed = 4.5f;
        def.baseArmor = 20.f;
        def.baseMr = 20.f;
        def.aggroRange = 3.f;
        def.leashRange = 9.f;
        return def;
    }

    MinionCombatDef MakeDefaultMinionCombat()
    {
        MinionCombatDef def{};
        def.moveSpeed = 4.f;
        def.attackRange = 1.5f;
        def.sightRange = 12.f;
        def.attackDamage = 40.f;
        def.attackCooldownMax = 1.666667f;
        def.maxHp = 225.f;
        return def;
    }

    MinionCombatDef MakeMinionCombat_0()
    {
        MinionCombatDef def{};
        def.moveSpeed = 4.f;
        def.attackRange = 1.5f;
        def.sightRange = 12.f;
        def.attackDamage = 40.f;
        def.attackCooldownMax = 1.666667f;
        def.maxHp = 225.f;
        return def;
    }

    MinionCombatDef MakeMinionCombat_1()
    {
        MinionCombatDef def{};
        def.moveSpeed = 4.f;
        def.attackRange = 8.f;
        def.sightRange = 14.f;
        def.attackDamage = 60.f;
        def.attackCooldownMax = 2.f;
        def.maxHp = 225.f;
        return def;
    }

    MinionCombatDef MakeMinionCombat_2()
    {
        MinionCombatDef def{};
        def.moveSpeed = 3.5f;
        def.attackRange = 10.f;
        def.sightRange = 16.f;
        def.attackDamage = 40.f;
        def.attackCooldownMax = 1.666667f;
        def.maxHp = 225.f;
        return def;
    }

    MinionCombatDef MakeMinionCombat_3()
    {
        MinionCombatDef def{};
        def.moveSpeed = 5.f;
        def.attackRange = 2.f;
        def.sightRange = 14.f;
        def.attackDamage = 100.f;
        def.attackCooldownMax = 1.666667f;
        def.maxHp = 500.f;
        return def;
    }

    MinionCombatDef MakeMinionCombat_4()
    {
        MinionCombatDef def{};
        def.moveSpeed = 5.2f;
        def.attackRange = 2.2f;
        def.sightRange = 14.f;
        def.attackDamage = 80.f;
        def.attackCooldownMax = 1.f;
        def.maxHp = 750.f;
        return def;
    }

    MinionWaveDef MakeMinionWaveDef()
    {
        MinionWaveDef def{};
        def.waveIntervalTicks = 900ull;
        def.initialDelayTicks = 300ull;
        def.perMinionDelayTicks = 10ull;
        def.siegeWavePeriod = 3u;
        def.timeGrowthCapMinutes = 30u;
        def.timeGrowthPerMinute = 0.025f;
        def.corpseDeathTimerSec = 1.5f;
        def.rangedProjectile.speed = 14.f;
        def.rangedProjectile.hitRadius = 0.45f;
        def.rangedProjectile.forwardOffset = 0.45f;
        def.rangedProjectile.spawnHeight = 0.85f;
        def.rangedProjectile.maxDistancePadding = 2.f;
        return def;
    }

    const JungleCampGameDefEntry kJungleCamps[] =
    {
        { static_cast<u8_t>(0u), MakeJungleCamp_0() },
        { static_cast<u8_t>(1u), MakeJungleCamp_1() },
        { static_cast<u8_t>(2u), MakeJungleCamp_2() },
        { static_cast<u8_t>(3u), MakeJungleCamp_3() },
        { static_cast<u8_t>(4u), MakeJungleCamp_4() },
        { static_cast<u8_t>(5u), MakeJungleCamp_5() },
        { static_cast<u8_t>(6u), MakeJungleCamp_6() },
        { static_cast<u8_t>(7u), MakeJungleCamp_7() },
        { static_cast<u8_t>(8u), MakeJungleCamp_8() },
        { static_cast<u8_t>(9u), MakeJungleCamp_9() },
        { static_cast<u8_t>(10u), MakeJungleCamp_10() },
    };

    const MinionCombatDefEntry kMinions[] =
    {
        { static_cast<u8_t>(0u), MakeMinionCombat_0() },
        { static_cast<u8_t>(1u), MakeMinionCombat_1() },
        { static_cast<u8_t>(2u), MakeMinionCombat_2() },
        { static_cast<u8_t>(3u), MakeMinionCombat_3() },
        { static_cast<u8_t>(4u), MakeMinionCombat_4() },
    };

    EconomyGameplayDef MakeEconomyDef()
    {
        EconomyGameplayDef def{};
        def.xpRequiredForNextLevel[1] = 280.f;
        def.xpRequiredForNextLevel[2] = 380.f;
        def.xpRequiredForNextLevel[3] = 480.f;
        def.xpRequiredForNextLevel[4] = 580.f;
        def.xpRequiredForNextLevel[5] = 680.f;
        def.xpRequiredForNextLevel[6] = 780.f;
        def.xpRequiredForNextLevel[7] = 880.f;
        def.xpRequiredForNextLevel[8] = 980.f;
        def.xpRequiredForNextLevel[9] = 1080.f;
        def.xpRequiredForNextLevel[10] = 1180.f;
        def.xpRequiredForNextLevel[11] = 1280.f;
        def.xpRequiredForNextLevel[12] = 1380.f;
        def.xpRequiredForNextLevel[13] = 1480.f;
        def.xpRequiredForNextLevel[14] = 1580.f;
        def.xpRequiredForNextLevel[15] = 1680.f;
        def.xpRequiredForNextLevel[16] = 1780.f;
        def.xpRequiredForNextLevel[17] = 1880.f;
        def.championKill.killerGold = 300.f;
        def.championKill.assistGold = 150.f;
        def.championKill.firstBloodBonusGold = 100.f;
        def.championKill.victimNextLevelXPFactor = 0.5f;
        def.championKill.shareRadius = 20.f;
        def.melee.soloXP = 61.75f;
        def.melee.sharedXP = 80.6f;
        def.melee.gold = 21.f;
        def.melee.maxGold = 0.f;
        def.melee.growthAmount = 0.f;
        def.melee.growthIntervalSec = 0.f;
        def.ranged.soloXP = 30.4f;
        def.ranged.sharedXP = 39.68f;
        def.ranged.gold = 14.f;
        def.ranged.maxGold = 0.f;
        def.ranged.growthAmount = 0.f;
        def.ranged.growthIntervalSec = 0.f;
        def.siege.soloXP = 95.f;
        def.siege.sharedXP = 124.f;
        def.siege.gold = 60.f;
        def.siege.maxGold = 90.f;
        def.siege.growthAmount = 3.f;
        def.siege.growthIntervalSec = 90.f;
        def.super.soloXP = 95.f;
        def.super.sharedXP = 124.f;
        def.super.gold = 60.f;
        def.super.maxGold = 90.f;
        def.super.growthAmount = 3.f;
        def.super.growthIntervalSec = 90.f;
        def.turretGold = 250.f;
        def.jungle.smallCampGold = 35.f;
        def.jungle.smallCampXP = 75.f;
        def.jungle.epicGold = 150.f;
        def.jungle.epicXP = 250.f;
        def.jungle.baronGold = 300.f;
        def.jungle.baronXP = 600.f;
        def.passiveGoldStartTick = 3300ull;
        def.passiveGoldIntervalTicks = 30ull;
        def.passiveGoldPerGrant = 2u;
        def.assistCreditWindowSec = 10.f;
        def.recallDurationSec = 2.f;
        def.bValid = true;
        return def;
    }

    ItemDef MakeItem_1001()
    {
        ItemDef def{};
        def.itemId = 1001u;
        def.price = 300u;
        def.stats.flatMoveSpeed = 25.f;
        def.displayName = "Boots";
        return def;
    }

    ItemDef MakeItem_1011()
    {
        ItemDef def{};
        def.itemId = 1011u;
        def.price = 900u;
        def.stats.flatHealth = 350.f;
        def.displayName = "Giant's Belt";
        return def;
    }

    ItemDef MakeItem_1018()
    {
        ItemDef def{};
        def.itemId = 1018u;
        def.price = 600u;
        def.stats.critChance = 0.15f;
        def.displayName = "Cloak of Agility";
        return def;
    }

    ItemDef MakeItem_1026()
    {
        ItemDef def{};
        def.itemId = 1026u;
        def.price = 850u;
        def.stats.flatAp = 45.f;
        def.displayName = "Blasting Wand";
        return def;
    }

    ItemDef MakeItem_1027()
    {
        ItemDef def{};
        def.itemId = 1027u;
        def.price = 400u;
        def.stats.flatMana = 300.f;
        def.displayName = "Sapphire Crystal";
        return def;
    }

    ItemDef MakeItem_1028()
    {
        ItemDef def{};
        def.itemId = 1028u;
        def.price = 400u;
        def.stats.flatHealth = 150.f;
        def.displayName = "Ruby Crystal";
        return def;
    }

    ItemDef MakeItem_1029()
    {
        ItemDef def{};
        def.itemId = 1029u;
        def.price = 300u;
        def.stats.flatArmor = 15.f;
        def.displayName = "Cloth Armor";
        return def;
    }

    ItemDef MakeItem_1031()
    {
        ItemDef def{};
        def.itemId = 1031u;
        def.price = 800u;
        def.stats.flatArmor = 40.f;
        def.displayName = "Chain Vest";
        return def;
    }

    ItemDef MakeItem_1033()
    {
        ItemDef def{};
        def.itemId = 1033u;
        def.price = 400u;
        def.stats.flatMr = 20.f;
        def.displayName = "Null-Magic Mantle";
        return def;
    }

    ItemDef MakeItem_1036()
    {
        ItemDef def{};
        def.itemId = 1036u;
        def.price = 350u;
        def.stats.flatAd = 10.f;
        def.displayName = "Long Sword";
        return def;
    }

    ItemDef MakeItem_1037()
    {
        ItemDef def{};
        def.itemId = 1037u;
        def.price = 875u;
        def.stats.flatAd = 25.f;
        def.displayName = "Pickaxe";
        return def;
    }

    ItemDef MakeItem_1038()
    {
        ItemDef def{};
        def.itemId = 1038u;
        def.price = 1300u;
        def.stats.flatAd = 40.f;
        def.displayName = "B. F. Sword";
        return def;
    }

    ItemDef MakeItem_1042()
    {
        ItemDef def{};
        def.itemId = 1042u;
        def.price = 250u;
        def.stats.bonusAttackSpeed = 0.1f;
        def.displayName = "Dagger";
        return def;
    }

    ItemDef MakeItem_1043()
    {
        ItemDef def{};
        def.itemId = 1043u;
        def.price = 700u;
        def.stats.bonusAttackSpeed = 0.15f;
        def.displayName = "Recurve Bow";
        return def;
    }

    ItemDef MakeItem_1052()
    {
        ItemDef def{};
        def.itemId = 1052u;
        def.price = 400u;
        def.stats.flatAp = 20.f;
        def.displayName = "Amplifying Tome";
        return def;
    }

    ItemDef MakeItem_1053()
    {
        ItemDef def{};
        def.itemId = 1053u;
        def.price = 900u;
        def.stats.flatAd = 15.f;
        def.stats.lifeSteal = 0.07f;
        def.displayName = "Vampiric Scepter";
        return def;
    }

    ItemDef MakeItem_1054()
    {
        ItemDef def{};
        def.itemId = 1054u;
        def.price = 450u;
        def.stats.flatHealth = 110.f;
        def.displayName = "Doran's Shield";
        return def;
    }

    ItemDef MakeItem_1055()
    {
        ItemDef def{};
        def.itemId = 1055u;
        def.price = 450u;
        def.stats.flatAd = 10.f;
        def.stats.flatHealth = 80.f;
        def.displayName = "Doran's Blade";
        return def;
    }

    ItemDef MakeItem_1056()
    {
        ItemDef def{};
        def.itemId = 1056u;
        def.price = 400u;
        def.stats.flatAp = 18.f;
        def.stats.flatHealth = 90.f;
        def.displayName = "Doran's Ring";
        return def;
    }

    ItemDef MakeItem_1057()
    {
        ItemDef def{};
        def.itemId = 1057u;
        def.price = 900u;
        def.stats.flatMr = 45.f;
        def.displayName = "Negatron Cloak";
        return def;
    }

    ItemDef MakeItem_1058()
    {
        ItemDef def{};
        def.itemId = 1058u;
        def.price = 1200u;
        def.stats.flatAp = 65.f;
        def.displayName = "Needlessly Large Rod";
        return def;
    }

    ItemDef MakeItem_3006()
    {
        ItemDef def{};
        def.itemId = 3006u;
        def.price = 1100u;
        def.stats.bonusAttackSpeed = 0.25f;
        def.stats.flatMoveSpeed = 45.f;
        def.displayName = "Berserker's Greaves";
        return def;
    }

    ItemDef MakeItem_3020()
    {
        ItemDef def{};
        def.itemId = 3020u;
        def.price = 1100u;
        def.stats.flatMoveSpeed = 45.f;
        def.stats.flatMagicPen = 15.f;
        def.displayName = "Sorcerer's Shoes";
        return def;
    }

    ItemDef MakeItem_3031()
    {
        ItemDef def{};
        def.itemId = 3031u;
        def.price = 3450u;
        def.stats.flatAd = 65.f;
        def.stats.critChance = 0.25f;
        def.displayName = "Infinity Edge";
        return def;
    }

    ItemDef MakeItem_3047()
    {
        ItemDef def{};
        def.itemId = 3047u;
        def.price = 1200u;
        def.stats.flatArmor = 25.f;
        def.stats.flatMoveSpeed = 45.f;
        def.displayName = "Plated Steelcaps";
        return def;
    }

    ItemDef MakeItem_3065()
    {
        ItemDef def{};
        def.itemId = 3065u;
        def.price = 2700u;
        def.stats.flatHealth = 450.f;
        def.stats.flatMr = 50.f;
        def.stats.abilityHaste = 10.f;
        def.displayName = "Spirit Visage";
        return def;
    }

    ItemDef MakeItem_3072()
    {
        ItemDef def{};
        def.itemId = 3072u;
        def.price = 3400u;
        def.stats.flatAd = 80.f;
        def.stats.lifeSteal = 0.15f;
        def.displayName = "Bloodthirster";
        return def;
    }

    ItemDef MakeItem_3078()
    {
        ItemDef def{};
        def.itemId = 3078u;
        def.price = 3333u;
        def.stats.flatAd = 45.f;
        def.stats.flatHealth = 300.f;
        def.stats.bonusAttackSpeed = 0.3f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Trinity Force";
        return def;
    }

    ItemDef MakeItem_3089()
    {
        ItemDef def{};
        def.itemId = 3089u;
        def.price = 3500u;
        def.stats.flatAp = 130.f;
        def.displayName = "Rabadon's Deathcap";
        return def;
    }

    ItemDef MakeItem_3111()
    {
        ItemDef def{};
        def.itemId = 3111u;
        def.price = 1200u;
        def.stats.flatMr = 20.f;
        def.stats.flatMoveSpeed = 45.f;
        def.displayName = "Mercury's Treads";
        return def;
    }

    ItemDef MakeItem_3153()
    {
        ItemDef def{};
        def.itemId = 3153u;
        def.price = 3000u;
        def.stats.flatAd = 40.f;
        def.stats.bonusAttackSpeed = 0.25f;
        def.stats.lifeSteal = 0.1f;
        def.onHitDamage.bValid = true;
        def.onHitDamage.rankCount = 1u;
        def.onHitDamage.type = eDamageType::Physical;
        def.onHitDamage.flags = DamageFlag_None;
        def.onHitDamage.flatByRank[0] = 0.f;
        def.onHitDamage.totalAdRatioByRank[0] = 0.f;
        def.onHitDamage.bonusAdRatioByRank[0] = 0.f;
        def.onHitDamage.apRatioByRank[0] = 0.f;
        def.onHitDamage.targetMaxHpRatioByRank[0] = 0.1f;
        def.onHitDamage.targetMissingHpRatioByRank[0] = 0.f;
        def.displayName = "Blade of the Ruined King";
        return def;
    }

    ItemDef MakeItem_3157()
    {
        ItemDef def{};
        def.itemId = 3157u;
        def.price = 3250u;
        def.stats.flatAp = 105.f;
        def.stats.flatArmor = 50.f;
        def.displayName = "Zhonya's Hourglass";
        return def;
    }

    ItemDef MakeItem_3158()
    {
        ItemDef def{};
        def.itemId = 3158u;
        def.price = 950u;
        def.stats.abilityHaste = 15.f;
        def.stats.flatMoveSpeed = 45.f;
        def.displayName = "Ionian Boots of Lucidity";
        return def;
    }

    ItemDef MakeItem_3742()
    {
        ItemDef def{};
        def.itemId = 3742u;
        def.price = 2900u;
        def.stats.flatHealth = 350.f;
        def.stats.flatArmor = 45.f;
        def.stats.flatMoveSpeed = 5.f;
        def.displayName = "Dead Man's Plate";
        return def;
    }

    const ItemDef kItemDefs[] =
    {
        MakeItem_1001(),
        MakeItem_1011(),
        MakeItem_1018(),
        MakeItem_1026(),
        MakeItem_1027(),
        MakeItem_1028(),
        MakeItem_1029(),
        MakeItem_1031(),
        MakeItem_1033(),
        MakeItem_1036(),
        MakeItem_1037(),
        MakeItem_1038(),
        MakeItem_1042(),
        MakeItem_1043(),
        MakeItem_1052(),
        MakeItem_1053(),
        MakeItem_1054(),
        MakeItem_1055(),
        MakeItem_1056(),
        MakeItem_1057(),
        MakeItem_1058(),
        MakeItem_3006(),
        MakeItem_3020(),
        MakeItem_3031(),
        MakeItem_3047(),
        MakeItem_3065(),
        MakeItem_3072(),
        MakeItem_3078(),
        MakeItem_3089(),
        MakeItem_3111(),
        MakeItem_3153(),
        MakeItem_3157(),
        MakeItem_3158(),
        MakeItem_3742(),
    };

    const ChampionGameplayDef kChampions[] =
    {
        MakeChampion_ANNIE(),
        MakeChampion_ASHE(),
        MakeChampion_EZREAL(),
        MakeChampion_FIORA(),
        MakeChampion_GAREN(),
        MakeChampion_IRELIA(),
        MakeChampion_JAX(),
        MakeChampion_KALISTA(),
        MakeChampion_KINDRED(),
        MakeChampion_LEESIN(),
        MakeChampion_MASTERYI(),
        MakeChampion_RIVEN(),
        MakeChampion_SYLAS(),
        MakeChampion_VIEGO(),
        MakeChampion_YASUO(),
        MakeChampion_YONE(),
        MakeChampion_ZED(),
    };

    const SkillGameplayDef kSkills[] =
    {
        MakeSkill_ANNIE_BASIC_ATTACK(),
        MakeSkill_ANNIE_E(),
        MakeSkill_ANNIE_Q(),
        MakeSkill_ANNIE_R(),
        MakeSkill_ANNIE_W(),
        MakeSkill_ASHE_BASIC_ATTACK(),
        MakeSkill_ASHE_E(),
        MakeSkill_ASHE_Q(),
        MakeSkill_ASHE_R(),
        MakeSkill_ASHE_W(),
        MakeSkill_EZREAL_BASIC_ATTACK(),
        MakeSkill_EZREAL_E(),
        MakeSkill_EZREAL_Q(),
        MakeSkill_EZREAL_R(),
        MakeSkill_EZREAL_W(),
        MakeSkill_FIORA_BASIC_ATTACK(),
        MakeSkill_FIORA_E(),
        MakeSkill_FIORA_Q(),
        MakeSkill_FIORA_R(),
        MakeSkill_FIORA_W(),
        MakeSkill_GAREN_BASIC_ATTACK(),
        MakeSkill_GAREN_E(),
        MakeSkill_GAREN_Q(),
        MakeSkill_GAREN_R(),
        MakeSkill_GAREN_W(),
        MakeSkill_IRELIA_BASIC_ATTACK(),
        MakeSkill_IRELIA_E(),
        MakeSkill_IRELIA_Q(),
        MakeSkill_IRELIA_R(),
        MakeSkill_IRELIA_W(),
        MakeSkill_JAX_BASIC_ATTACK(),
        MakeSkill_JAX_E(),
        MakeSkill_JAX_Q(),
        MakeSkill_JAX_R(),
        MakeSkill_JAX_W(),
        MakeSkill_KALISTA_BASIC_ATTACK(),
        MakeSkill_KALISTA_E(),
        MakeSkill_KALISTA_Q(),
        MakeSkill_KALISTA_R(),
        MakeSkill_KALISTA_W(),
        MakeSkill_KINDRED_BASIC_ATTACK(),
        MakeSkill_KINDRED_E(),
        MakeSkill_KINDRED_Q(),
        MakeSkill_KINDRED_R(),
        MakeSkill_KINDRED_W(),
        MakeSkill_LEESIN_BASIC_ATTACK(),
        MakeSkill_LEESIN_E(),
        MakeSkill_LEESIN_Q(),
        MakeSkill_LEESIN_R(),
        MakeSkill_LEESIN_W(),
        MakeSkill_MASTERYI_BASIC_ATTACK(),
        MakeSkill_MASTERYI_E(),
        MakeSkill_MASTERYI_Q(),
        MakeSkill_MASTERYI_R(),
        MakeSkill_MASTERYI_W(),
        MakeSkill_RIVEN_BASIC_ATTACK(),
        MakeSkill_RIVEN_E(),
        MakeSkill_RIVEN_Q(),
        MakeSkill_RIVEN_R(),
        MakeSkill_RIVEN_W(),
        MakeSkill_SYLAS_BASIC_ATTACK(),
        MakeSkill_SYLAS_E(),
        MakeSkill_SYLAS_Q(),
        MakeSkill_SYLAS_R(),
        MakeSkill_SYLAS_W(),
        MakeSkill_VIEGO_BASIC_ATTACK(),
        MakeSkill_VIEGO_E(),
        MakeSkill_VIEGO_Q(),
        MakeSkill_VIEGO_R(),
        MakeSkill_VIEGO_W(),
        MakeSkill_YASUO_BASIC_ATTACK(),
        MakeSkill_YASUO_E(),
        MakeSkill_YASUO_Q(),
        MakeSkill_YASUO_R(),
        MakeSkill_YASUO_W(),
        MakeSkill_YONE_BASIC_ATTACK(),
        MakeSkill_YONE_E(),
        MakeSkill_YONE_Q(),
        MakeSkill_YONE_R(),
        MakeSkill_YONE_W(),
        MakeSkill_ZED_BASIC_ATTACK(),
        MakeSkill_ZED_E(),
        MakeSkill_ZED_Q(),
        MakeSkill_ZED_R(),
        MakeSkill_ZED_W(),
    };

    const SummonerSpellGameplayDef kSummonerSpells[] =
    {
        MakeSummonerSpell_4(),
    };
}

namespace ServerData
{
    const GameplayDefinitionPack& GetLoLGameplayDefinitionPack()
    {
        static const EconomyGameplayDef economyDef = MakeEconomyDef();
        static const GameplayDefinitionPack pack =
        {
            { 1u, 1u, kBuildHash, 0u, eDataPackVisibility::ServerPrivate },
            kChampions,
            sizeof(kChampions) / sizeof(kChampions[0]),
            kSkills,
            sizeof(kSkills) / sizeof(kSkills[0]),
            kSummonerSpells,
            sizeof(kSummonerSpells) / sizeof(kSummonerSpells[0]),
            &economyDef,
            kItemDefs,
            sizeof(kItemDefs) / sizeof(kItemDefs[0]),
        };
        return pack;
    }

    const SpawnObjectDefinitionPack& GetLoLSpawnObjectDefinitionPack()
    {
        static const SpawnObjectDefinitionPack pack =
        {
            { 1u, 1u, kBuildHash, 0u, eDataPackVisibility::ServerPrivate },
            MakeSpawnLoadoutPolicy(),
            MakeChampionColliderProfile(),
            MakeStructureGameDef(),
            MakeDefaultJungleCamp(),
            kJungleCamps,
            sizeof(kJungleCamps) / sizeof(kJungleCamps[0]),
            MakeDefaultMinionCombat(),
            kMinions,
            sizeof(kMinions) / sizeof(kMinions[0]),
            MakeMinionWaveDef(),
        };
        return pack;
    }
}
