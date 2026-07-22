#include "Server/Private/Data/LoLGameplayDefinitionPack.h"

namespace
{
    inline constexpr u32_t kBuildHash = 0xBEC4359Eu;

    ChampionGameplayDef MakeChampion_ANNIE()
    {
        ChampionGameplayDef def{};
        def.key = 0xA3618A11u;
        def.id.value = 1u;
        def.legacyChampion = eChampion::ANNIE;
        def.dataVersion = 1u;
        def.authoringHash = kBuildHash;
        def.stats.resourceKind = eChampionResourceKind::Mana;
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
        def.stats.basicAttackWindupSec = 0.25f;
        def.stats.baseAttackRange = 6.25f;
        def.stats.baseMoveSpeed = 5.f;
        def.stats.navArriveRadius = 0.15f;
        def.stats.spatialRadius = 0.75f;
        def.stats.sightRange = 19.f;
        def.stats.resourceRegenPerSec = 3.f;
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
        def.stats.resourceKind = eChampionResourceKind::Mana;
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
        def.stats.basicAttackWindupSec = 0.208333f;
        def.stats.baseAttackRange = 6.f;
        def.stats.baseMoveSpeed = 5.f;
        def.stats.navArriveRadius = 0.15f;
        def.stats.spatialRadius = 0.75f;
        def.stats.sightRange = 19.f;
        def.stats.resourceRegenPerSec = 3.f;
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
        def.stats.resourceKind = eChampionResourceKind::Mana;
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
        def.stats.basicAttackWindupSec = 0.25f;
        def.stats.baseAttackRange = 5.5f;
        def.stats.baseMoveSpeed = 5.f;
        def.stats.navArriveRadius = 0.15f;
        def.stats.spatialRadius = 0.75f;
        def.stats.sightRange = 19.f;
        def.stats.resourceRegenPerSec = 3.f;
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
        def.stats.resourceKind = eChampionResourceKind::Mana;
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
        def.stats.basicAttackWindupSec = 0.25f;
        def.stats.baseAttackRange = 1.5f;
        def.stats.baseMoveSpeed = 5.f;
        def.stats.navArriveRadius = 0.15f;
        def.stats.spatialRadius = 0.75f;
        def.stats.sightRange = 19.f;
        def.stats.resourceRegenPerSec = 3.f;
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
        def.stats.resourceKind = eChampionResourceKind::None;
        def.stats.baseHp = 690.f;
        def.stats.hpPerLevel = 98.f;
        def.stats.baseMana = 0.f;
        def.stats.manaPerLevel = 0.f;
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
        def.stats.basicAttackWindupSec = 0.25f;
        def.stats.baseAttackRange = 1.5f;
        def.stats.baseMoveSpeed = 5.f;
        def.stats.navArriveRadius = 0.15f;
        def.stats.spatialRadius = 0.75f;
        def.stats.sightRange = 19.f;
        def.stats.resourceRegenPerSec = 0.f;
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
        def.stats.resourceKind = eChampionResourceKind::Mana;
        def.stats.baseHp = 630.f;
        def.stats.hpPerLevel = 115.f;
        def.stats.baseMana = 350.f;
        def.stats.manaPerLevel = 50.f;
        def.stats.baseAd = 85.f;
        def.stats.adPerLevel = 10.5f;
        def.stats.baseAp = 0.f;
        def.stats.apPerLevel = 0.f;
        def.stats.baseArmor = 36.f;
        def.stats.armorPerLevel = 4.7f;
        def.stats.baseMr = 30.f;
        def.stats.mrPerLevel = 2.05f;
        def.stats.baseAttackSpeed = 0.656f;
        def.stats.attackSpeedRatio = 0.656f;
        def.stats.attackSpeedPerLevel = 0.025f;
        def.stats.basicAttackWindupSec = 0.2f;
        def.stats.baseAttackRange = 2.1f;
        def.stats.baseMoveSpeed = 5.f;
        def.stats.navArriveRadius = 0.15f;
        def.stats.spatialRadius = 0.75f;
        def.stats.sightRange = 19.f;
        def.stats.resourceRegenPerSec = 3.f;
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
        def.stats.resourceKind = eChampionResourceKind::Mana;
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
        def.stats.basicAttackWindupSec = 0.25f;
        def.stats.baseAttackRange = 1.5f;
        def.stats.baseMoveSpeed = 5.f;
        def.stats.navArriveRadius = 0.15f;
        def.stats.spatialRadius = 0.75f;
        def.stats.sightRange = 19.f;
        def.stats.resourceRegenPerSec = 3.f;
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
        def.stats.resourceKind = eChampionResourceKind::Mana;
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
        def.stats.basicAttackWindupSec = 0.25f;
        def.stats.baseAttackRange = 5.5f;
        def.stats.baseMoveSpeed = 5.f;
        def.stats.navArriveRadius = 0.15f;
        def.stats.spatialRadius = 0.75f;
        def.stats.sightRange = 19.f;
        def.stats.resourceRegenPerSec = 3.f;
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
        def.stats.resourceKind = eChampionResourceKind::Mana;
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
        def.stats.basicAttackWindupSec = 0.166667f;
        def.stats.baseAttackRange = 5.5f;
        def.stats.baseMoveSpeed = 5.f;
        def.stats.navArriveRadius = 0.15f;
        def.stats.spatialRadius = 0.75f;
        def.stats.sightRange = 19.f;
        def.stats.resourceRegenPerSec = 3.f;
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
        def.stats.resourceKind = eChampionResourceKind::Energy;
        def.stats.baseHp = 645.f;
        def.stats.hpPerLevel = 108.f;
        def.stats.baseMana = 200.f;
        def.stats.manaPerLevel = 0.f;
        def.stats.baseAd = 85.f;
        def.stats.adPerLevel = 8.69999981f;
        def.stats.baseAp = 0.f;
        def.stats.apPerLevel = 0.f;
        def.stats.baseArmor = 34.f;
        def.stats.armorPerLevel = 4.7f;
        def.stats.baseMr = 32.f;
        def.stats.mrPerLevel = 2.05f;
        def.stats.baseAttackSpeed = 0.651f;
        def.stats.attackSpeedRatio = 0.651f;
        def.stats.attackSpeedPerLevel = 0.03f;
        def.stats.basicAttackWindupSec = 0.166667f;
        def.stats.baseAttackRange = 1.5f;
        def.stats.baseMoveSpeed = 5.f;
        def.stats.navArriveRadius = 0.15f;
        def.stats.spatialRadius = 0.75f;
        def.stats.sightRange = 19.f;
        def.stats.resourceRegenPerSec = 10.f;
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
        def.stats.resourceKind = eChampionResourceKind::Mana;
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
        def.stats.basicAttackWindupSec = 0.166667f;
        def.stats.baseAttackRange = 1.5f;
        def.stats.baseMoveSpeed = 5.f;
        def.stats.navArriveRadius = 0.15f;
        def.stats.spatialRadius = 0.75f;
        def.stats.sightRange = 19.f;
        def.stats.resourceRegenPerSec = 3.f;
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
        def.stats.resourceKind = eChampionResourceKind::None;
        def.stats.baseHp = 630.f;
        def.stats.hpPerLevel = 100.f;
        def.stats.baseMana = 0.f;
        def.stats.manaPerLevel = 0.f;
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
        def.stats.basicAttackWindupSec = 0.25f;
        def.stats.baseAttackRange = 1.5f;
        def.stats.baseMoveSpeed = 5.f;
        def.stats.navArriveRadius = 0.15f;
        def.stats.spatialRadius = 0.75f;
        def.stats.sightRange = 19.f;
        def.stats.resourceRegenPerSec = 0.f;
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
        def.stats.resourceKind = eChampionResourceKind::Mana;
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
        def.stats.basicAttackWindupSec = 0.166667f;
        def.stats.baseAttackRange = 1.5f;
        def.stats.baseMoveSpeed = 5.f;
        def.stats.navArriveRadius = 0.15f;
        def.stats.spatialRadius = 0.75f;
        def.stats.sightRange = 19.f;
        def.stats.resourceRegenPerSec = 3.f;
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
        def.stats.resourceKind = eChampionResourceKind::None;
        def.stats.baseHp = 630.f;
        def.stats.hpPerLevel = 109.f;
        def.stats.baseMana = 0.f;
        def.stats.manaPerLevel = 0.f;
        def.stats.baseAd = 97.f;
        def.stats.adPerLevel = 13.5f;
        def.stats.baseAp = 0.f;
        def.stats.apPerLevel = 0.f;
        def.stats.baseArmor = 34.f;
        def.stats.armorPerLevel = 4.6f;
        def.stats.baseMr = 32.f;
        def.stats.mrPerLevel = 2.05f;
        def.stats.baseAttackSpeed = 0.658f;
        def.stats.attackSpeedRatio = 0.658f;
        def.stats.attackSpeedPerLevel = 0.0275f;
        def.stats.basicAttackWindupSec = 0.25f;
        def.stats.baseAttackRange = 1.5f;
        def.stats.baseMoveSpeed = 5.f;
        def.stats.navArriveRadius = 0.15f;
        def.stats.spatialRadius = 0.75f;
        def.stats.sightRange = 19.f;
        def.stats.resourceRegenPerSec = 0.f;
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
        def.stats.resourceKind = eChampionResourceKind::Flow;
        def.stats.baseHp = 590.f;
        def.stats.hpPerLevel = 110.f;
        def.stats.baseMana = 0.f;
        def.stats.manaPerLevel = 0.f;
        def.stats.baseAd = 80.f;
        def.stats.adPerLevel = 10.f;
        def.stats.baseAp = 0.f;
        def.stats.apPerLevel = 0.f;
        def.stats.baseArmor = 30.f;
        def.stats.armorPerLevel = 4.6f;
        def.stats.baseMr = 32.f;
        def.stats.mrPerLevel = 2.05f;
        def.stats.baseAttackSpeed = 0.697f;
        def.stats.attackSpeedRatio = 0.697f;
        def.stats.attackSpeedPerLevel = 0.033f;
        def.stats.basicAttackWindupSec = 0.175f;
        def.stats.baseAttackRange = 2.5f;
        def.stats.baseMoveSpeed = 5.f;
        def.stats.navArriveRadius = 0.15f;
        def.stats.spatialRadius = 0.75f;
        def.stats.sightRange = 19.f;
        def.stats.resourceRegenPerSec = 0.f;
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
        def.stats.resourceKind = eChampionResourceKind::None;
        def.stats.baseHp = 620.f;
        def.stats.hpPerLevel = 105.f;
        def.stats.baseMana = 0.f;
        def.stats.manaPerLevel = 0.f;
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
        def.stats.basicAttackWindupSec = 0.245098f;
        def.stats.baseAttackRange = 1.5f;
        def.stats.baseMoveSpeed = 5.f;
        def.stats.navArriveRadius = 0.15f;
        def.stats.spatialRadius = 0.75f;
        def.stats.sightRange = 19.f;
        def.stats.resourceRegenPerSec = 0.f;
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
        def.stats.resourceKind = eChampionResourceKind::Energy;
        def.stats.baseHp = 654.f;
        def.stats.hpPerLevel = 99.f;
        def.stats.baseMana = 200.f;
        def.stats.manaPerLevel = 0.f;
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
        def.stats.basicAttackWindupSec = 0.25f;
        def.stats.baseAttackRange = 1.5f;
        def.stats.baseMoveSpeed = 5.f;
        def.stats.navArriveRadius = 0.15f;
        def.stats.spatialRadius = 0.75f;
        def.stats.sightRange = 19.f;
        def.stats.resourceRegenPerSec = 10.f;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 1u;
        def.cooldown.rankCount = 1u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 0.60000002f;
        def.range.rangeMax = 6.25f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 0.60000002f;
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
        def.stage.lockDurationSec[0] = 0.7f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 1.1f;
        def.effect.params[1].id = eSkillEffectParamId::ShieldAmountPerRank;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 45.f;
        def.effect.params[2].id = eSkillEffectParamId::ShieldArmorPerRank;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 5.f;
        def.effect.params[3].id = eSkillEffectParamId::ShieldBaseAmount;
        def.effect.params[3].rankCount = static_cast<u8_t>(1u);
        def.effect.params[3].valueByRank[0] = 50.f;
        def.effect.params[4].id = eSkillEffectParamId::ShieldDurationSec;
        def.effect.params[4].rankCount = static_cast<u8_t>(1u);
        def.effect.params[4].valueByRank[0] = 3.f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.4f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::None;
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
        def.input.activation = eSkillInputActivation::Press;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 80.f;
        def.effect.params[1].id = eSkillEffectParamId::DamagePerRank;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 35.f;
        def.effect.params[2].id = eSkillEffectParamId::StunDurationSec;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 1.25f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 0.5f;
        def.stage.commandLockSec[0] = 0.26666667f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::QueueUntilUnlock;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
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
        def.input.activation = eSkillInputActivation::Press;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 150.f;
        def.effect.params[1].id = eSkillEffectParamId::DamagePerRank;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 75.f;
        def.effect.params[2].id = eSkillEffectParamId::Radius;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 3.f;
        def.effect.params[3].id = eSkillEffectParamId::Range;
        def.effect.params[3].rankCount = static_cast<u8_t>(1u);
        def.effect.params[3].valueByRank[0] = 6.f;
        def.effect.params[4].id = eSkillEffectParamId::StunDurationSec;
        def.effect.params[4].rankCount = static_cast<u8_t>(1u);
        def.effect.params[4].valueByRank[0] = 1.25f;
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
        def.stage.lockDurationSec[0] = 0.9f;
        def.stage.commandLockSec[0] = 0.26666667f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::QueueUntilUnlock;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::Press;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 70.f;
        def.effect.params[1].id = eSkillEffectParamId::DamagePerRank;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 45.f;
        def.effect.params[2].id = eSkillEffectParamId::HalfAngleCos;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 0.76604444f;
        def.effect.params[3].id = eSkillEffectParamId::Range;
        def.effect.params[3].rankCount = static_cast<u8_t>(1u);
        def.effect.params[3].valueByRank[0] = 6.f;
        def.effect.params[4].id = eSkillEffectParamId::StunDurationSec;
        def.effect.params[4].rankCount = static_cast<u8_t>(1u);
        def.effect.params[4].valueByRank[0] = 1.25f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 0.6f;
        def.stage.commandLockSec[0] = 0.26666667f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::QueueUntilUnlock;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 1u;
        def.cooldown.rankCount = 1u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 1.72000003f;
        def.range.rangeMax = 6.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 1.72000003f;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 0.35f;
        def.effect.params[1].id = eSkillEffectParamId::Speed;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 18.f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 0.7f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
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
        def.input.activation = eSkillInputActivation::Press;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 10.f;
        def.effect.params[1].id = eSkillEffectParamId::Range;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 400.f;
        def.effect.params[2].id = eSkillEffectParamId::Speed;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 24.f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 0.5f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
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
        def.effect.paramCount = static_cast<u8_t>(3u);
        def.effect.params[0].id = eSkillEffectParamId::EffectDurationSec;
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 5.f;
        def.effect.params[1].id = eSkillEffectParamId::MaxStacks;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 4.f;
        def.effect.params[2].id = eSkillEffectParamId::SlowDurationSec;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 1.5f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.5f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::None;
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
        def.input.activation = eSkillInputActivation::Press;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 250.f;
        def.effect.params[1].id = eSkillEffectParamId::Range;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 200.f;
        def.effect.params[2].id = eSkillEffectParamId::Speed;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 20.f;
        def.effect.params[3].id = eSkillEffectParamId::StunDurationSec;
        def.effect.params[3].rankCount = static_cast<u8_t>(1u);
        def.effect.params[3].valueByRank[0] = 2.f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 1.f;
        def.stage.commandLockSec[0] = 0.26666667f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::QueueUntilUnlock;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::Press;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 45.f;
        def.effect.params[1].id = eSkillEffectParamId::MoveSpeedMul;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 0.5f;
        def.effect.params[2].id = eSkillEffectParamId::Range;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 12.f;
        def.effect.params[3].id = eSkillEffectParamId::SlowDurationSec;
        def.effect.params[3].rankCount = static_cast<u8_t>(1u);
        def.effect.params[3].valueByRank[0] = 1.5f;
        def.effect.params[4].id = eSkillEffectParamId::Speed;
        def.effect.params[4].rankCount = static_cast<u8_t>(1u);
        def.effect.params[4].valueByRank[0] = 24.f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 0.6f;
        def.stage.commandLockSec[0] = 0.26666667f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::QueueUntilUnlock;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::Press;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 0.1f;
        def.effect.params[1].id = eSkillEffectParamId::MaxStacks;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 5.f;
        def.effect.params[2].id = eSkillEffectParamId::Speed;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 20.f;
        def.effect.params[3].id = eSkillEffectParamId::StackWindowSec;
        def.effect.params[3].rankCount = static_cast<u8_t>(1u);
        def.effect.params[3].valueByRank[0] = 6.f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 0.65f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
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
        def.input.activation = eSkillInputActivation::Press;
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
        def.effect.damage.totalAdRatioByRank[2] = 2.f;
        def.effect.damage.totalAdRatioByRank[3] = 2.f;
        def.effect.damage.totalAdRatioByRank[4] = 2.f;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 0.75f;
        def.effect.params[1].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 80.f;
        def.effect.params[2].id = eSkillEffectParamId::BonusAdRatio;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 0.6f;
        def.effect.params[3].id = eSkillEffectParamId::CastTimeSec;
        def.effect.params[3].rankCount = static_cast<u8_t>(1u);
        def.effect.params[3].valueByRank[0] = 0.25f;
        def.effect.params[4].id = eSkillEffectParamId::DamagePerRank;
        def.effect.params[4].rankCount = static_cast<u8_t>(1u);
        def.effect.params[4].valueByRank[0] = 50.f;
        def.effect.params[5].id = eSkillEffectParamId::HalfWidth;
        def.effect.params[5].rankCount = static_cast<u8_t>(1u);
        def.effect.params[5].valueByRank[0] = 0.5f;
        def.effect.params[6].id = eSkillEffectParamId::Radius;
        def.effect.params[6].rankCount = static_cast<u8_t>(1u);
        def.effect.params[6].valueByRank[0] = 7.5f;
        def.effect.params[7].id = eSkillEffectParamId::Speed;
        def.effect.params[7].rankCount = static_cast<u8_t>(1u);
        def.effect.params[7].valueByRank[0] = 20.f;
        def.target.shape[0] = eTargetShape::Ground;
        def.stage.lockDurationSec[0] = 0.25f;
        def.stage.commandLockSec[0] = 0.25f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::ForcedMotion;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::Press;
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
        def.effect.damage.flatByRank[0] = 200.f;
        def.effect.damage.flatByRank[1] = 200.f;
        def.effect.damage.flatByRank[2] = 200.f;
        def.effect.damage.flatByRank[3] = 200.f;
        def.effect.damage.flatByRank[4] = 300.f;
        def.effect.damage.totalAdRatioByRank[0] = 2.f;
        def.effect.damage.totalAdRatioByRank[1] = 2.f;
        def.effect.damage.totalAdRatioByRank[2] = 2.f;
        def.effect.damage.totalAdRatioByRank[3] = 2.f;
        def.effect.damage.totalAdRatioByRank[4] = 2.f;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 0.4f;
        def.effect.params[1].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 20.f;
        def.effect.params[2].id = eSkillEffectParamId::CastTimeSec;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 0.25f;
        def.effect.params[3].id = eSkillEffectParamId::CooldownRefundSec;
        def.effect.params[3].rankCount = static_cast<u8_t>(1u);
        def.effect.params[3].valueByRank[0] = 1.5f;
        def.effect.params[4].id = eSkillEffectParamId::DamagePerRank;
        def.effect.params[4].rankCount = static_cast<u8_t>(1u);
        def.effect.params[4].valueByRank[0] = 25.f;
        def.effect.params[5].id = eSkillEffectParamId::HalfWidth;
        def.effect.params[5].rankCount = static_cast<u8_t>(1u);
        def.effect.params[5].valueByRank[0] = 0.6f;
        def.effect.params[6].id = eSkillEffectParamId::Speed;
        def.effect.params[6].rankCount = static_cast<u8_t>(1u);
        def.effect.params[6].valueByRank[0] = 20.f;
        def.effect.params[7].id = eSkillEffectParamId::TotalAdRatio;
        def.effect.params[7].rankCount = static_cast<u8_t>(1u);
        def.effect.params[7].valueByRank[0] = 1.3f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 0.25f;
        def.stage.commandLockSec[0] = 0.25f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::QueueUntilUnlock;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::Press;
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
        def.effect.paramCount = static_cast<u8_t>(6u);
        def.effect.params[0].id = eSkillEffectParamId::ApRatio;
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 1.1f;
        def.effect.params[1].id = eSkillEffectParamId::BonusAdRatio;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 1.f;
        def.effect.params[2].id = eSkillEffectParamId::CastTimeSec;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 1.f;
        def.effect.params[3].id = eSkillEffectParamId::HalfWidth;
        def.effect.params[3].rankCount = static_cast<u8_t>(1u);
        def.effect.params[3].valueByRank[0] = 1.6f;
        def.effect.params[4].id = eSkillEffectParamId::NonEpicBaseDamage;
        def.effect.params[4].rankCount = static_cast<u8_t>(3u);
        def.effect.params[4].valueByRank[0] = 150.f;
        def.effect.params[4].valueByRank[1] = 225.f;
        def.effect.params[4].valueByRank[2] = 300.f;
        def.effect.params[5].id = eSkillEffectParamId::Speed;
        def.effect.params[5].rankCount = static_cast<u8_t>(1u);
        def.effect.params[5].valueByRank[0] = 20.f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 1.f;
        def.stage.commandLockSec[0] = 0.26666667f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::QueueUntilUnlock;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::Press;
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
        def.effect.damage.totalAdRatioByRank[0] = 2.f;
        def.effect.damage.totalAdRatioByRank[1] = 2.f;
        def.effect.damage.totalAdRatioByRank[2] = 2.f;
        def.effect.damage.totalAdRatioByRank[3] = 2.f;
        def.effect.damage.totalAdRatioByRank[4] = 2.f;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 0.9f;
        def.effect.params[1].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 80.f;
        def.effect.params[2].id = eSkillEffectParamId::BonusAdRatio;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 1.f;
        def.effect.params[3].id = eSkillEffectParamId::CastTimeSec;
        def.effect.params[3].rankCount = static_cast<u8_t>(1u);
        def.effect.params[3].valueByRank[0] = 0.25f;
        def.effect.params[4].id = eSkillEffectParamId::DamagePerRank;
        def.effect.params[4].rankCount = static_cast<u8_t>(1u);
        def.effect.params[4].valueByRank[0] = 55.f;
        def.effect.params[5].id = eSkillEffectParamId::HalfWidth;
        def.effect.params[5].rankCount = static_cast<u8_t>(1u);
        def.effect.params[5].valueByRank[0] = 0.8f;
        def.effect.params[6].id = eSkillEffectParamId::ManaRestoreFlat;
        def.effect.params[6].rankCount = static_cast<u8_t>(1u);
        def.effect.params[6].valueByRank[0] = 60.f;
        def.effect.params[7].id = eSkillEffectParamId::MarkDurationSec;
        def.effect.params[7].rankCount = static_cast<u8_t>(1u);
        def.effect.params[7].valueByRank[0] = 4.f;
        def.effect.params[8].id = eSkillEffectParamId::Speed;
        def.effect.params[8].rankCount = static_cast<u8_t>(1u);
        def.effect.params[8].valueByRank[0] = 17.f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 0.25f;
        def.stage.commandLockSec[0] = 0.25f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::QueueUntilUnlock;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 1u;
        def.cooldown.rankCount = 1u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 1.66999996f;
        def.range.rangeMax = 1.5f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 1.66999996f;
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
        def.effect.paramCount = static_cast<u8_t>(5u);
        def.effect.params[0].id = eSkillEffectParamId::AcquireRange;
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 8.f;
        def.effect.params[1].id = eSkillEffectParamId::LifetimeSec;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 8.f;
        def.effect.params[2].id = eSkillEffectParamId::RespawnSec;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 1.5f;
        def.effect.params[3].id = eSkillEffectParamId::SideDotThreshold;
        def.effect.params[3].rankCount = static_cast<u8_t>(1u);
        def.effect.params[3].valueByRank[0] = 0.55f;
        def.effect.params[4].id = eSkillEffectParamId::TargetMaxHpRatio;
        def.effect.params[4].rankCount = static_cast<u8_t>(1u);
        def.effect.params[4].valueByRank[0] = 0.03f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 0.65f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
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
        def.effect.paramCount = static_cast<u8_t>(2u);
        def.effect.params[0].id = eSkillEffectParamId::EffectDurationSec;
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 5.f;
        def.effect.params[1].id = eSkillEffectParamId::MaxStacks;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 2.f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.4f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::None;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 70.f;
        def.effect.params[1].id = eSkillEffectParamId::DashDistance;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 3.f;
        def.effect.params[2].id = eSkillEffectParamId::DashDurationSec;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 0.25f;
        def.effect.params[3].id = eSkillEffectParamId::Radius;
        def.effect.params[3].rankCount = static_cast<u8_t>(1u);
        def.effect.params[3].valueByRank[0] = 1.f;
        def.effect.params[4].id = eSkillEffectParamId::Range;
        def.effect.params[4].rankCount = static_cast<u8_t>(1u);
        def.effect.params[4].valueByRank[0] = 4.f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 0.5f;
        def.stage.commandLockSec[0] = 0.5f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::ForcedMotion;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::Press;
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
        def.effect.paramCount = static_cast<u8_t>(6u);
        def.effect.params[0].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 80.f;
        def.effect.params[1].id = eSkillEffectParamId::ChallengeDurationSec;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 8.f;
        def.effect.params[2].id = eSkillEffectParamId::HealAmount;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 40.f;
        def.effect.params[3].id = eSkillEffectParamId::HealDurationSec;
        def.effect.params[3].rankCount = static_cast<u8_t>(1u);
        def.effect.params[3].valueByRank[0] = 5.f;
        def.effect.params[4].id = eSkillEffectParamId::HealIntervalSec;
        def.effect.params[4].rankCount = static_cast<u8_t>(1u);
        def.effect.params[4].valueByRank[0] = 0.5f;
        def.effect.params[5].id = eSkillEffectParamId::HealRadius;
        def.effect.params[5].rankCount = static_cast<u8_t>(1u);
        def.effect.params[5].valueByRank[0] = 6.f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 1.2f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 50.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 6.5f;
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
        def.effect.paramCount = static_cast<u8_t>(3u);
        def.effect.params[0].id = eSkillEffectParamId::MoveSpeedMul;
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 0.5f;
        def.effect.params[1].id = eSkillEffectParamId::Radius;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 0.8f;
        def.effect.params[2].id = eSkillEffectParamId::SlowDurationSec;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 1.5f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 1.5f;
        def.stage.commandLockSec[0] = 1.5f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::StationaryChannel;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 1u;
        def.cooldown.rankCount = 1u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 0.60000002f;
        def.range.rangeMax = 1.5f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 0.60000002f;
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
        def.stage.lockDurationSec[0] = 0.65f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
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
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::None;
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
        def.input.activation = eSkillInputActivation::Press;
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
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.45f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::None;
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
        def.input.activation = eSkillInputActivation::Press;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 150.f;
        def.effect.params[1].id = eSkillEffectParamId::DamagePerRank;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 150.f;
        def.effect.params[2].id = eSkillEffectParamId::MissingHealthDamageRatio;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 0.25f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 1.5f;
        def.stage.commandLockSec[0] = 0.26666667f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::QueueUntilUnlock;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
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
        def.input.activation = eSkillInputActivation::Press;
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
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.5f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::None;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 1u;
        def.cooldown.rankCount = 1u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 0.60000002f;
        def.range.rangeMax = 2.1f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 0.60000002f;
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
        def.stage.lockDurationSec[0] = 0.466667f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
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
        def.input.activation = eSkillInputActivation::PressRecast;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 50.f;
        def.cooldown.cooldownSec = 5.f;
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
        def.cooldown.cooldownSecByRank[0] = 5.f;
        def.cooldown.cooldownSecByRank[1] = 5.f;
        def.cooldown.cooldownSecByRank[2] = 5.f;
        def.cooldown.cooldownSecByRank[3] = 5.f;
        def.cooldown.cooldownSecByRank[4] = 5.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Magic;
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
        def.effect.damage.apRatioByRank[0] = 1.f;
        def.effect.damage.apRatioByRank[1] = 1.f;
        def.effect.damage.apRatioByRank[2] = 1.f;
        def.effect.damage.apRatioByRank[3] = 1.f;
        def.effect.damage.apRatioByRank[4] = 1.f;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 30.f;
        def.effect.params[1].id = eSkillEffectParamId::DamagePerRank;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 40.f;
        def.effect.params[2].id = eSkillEffectParamId::MarkDurationSec;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 5.f;
        def.effect.params[3].id = eSkillEffectParamId::Radius;
        def.effect.params[3].rankCount = static_cast<u8_t>(1u);
        def.effect.params[3].valueByRank[0] = 1.5f;
        def.effect.params[4].id = eSkillEffectParamId::StunDurationSec;
        def.effect.params[4].rankCount = static_cast<u8_t>(1u);
        def.effect.params[4].valueByRank[0] = 0.75f;
        def.target.shape[0] = eTargetShape::Ground;
        def.stage.lockDurationSec[0] = 0.9f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
        def.target.shape[1] = eTargetShape::Ground;
        def.stage.lockDurationSec[1] = 0.45f;
        def.stage.commandLockSec[1] = 0.f;
        def.stage.movePolicy[1] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[1] = true;
        def.stage.bPresentationLoopWhileActive[1] = false;
        def.facing.mode[1] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 15.f;
        def.cooldown.cooldownSec = 5.f;
        def.range.rangeMax = 6.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 15.f;
        def.cost.manaCostByRank[1] = 15.f;
        def.cost.manaCostByRank[2] = 15.f;
        def.cost.manaCostByRank[3] = 15.f;
        def.cost.manaCostByRank[4] = 15.f;
        def.cooldown.cooldownSecByRank[0] = 5.f;
        def.cooldown.cooldownSecByRank[1] = 4.f;
        def.cooldown.cooldownSecByRank[2] = 3.f;
        def.cooldown.cooldownSecByRank[3] = 2.5f;
        def.cooldown.cooldownSecByRank[4] = 2.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_CanLifesteal | DamageFlag_OnHit;
        def.effect.damage.flatByRank[0] = 50.f;
        def.effect.damage.flatByRank[1] = 83.f;
        def.effect.damage.flatByRank[2] = 200.f;
        def.effect.damage.flatByRank[3] = 200.f;
        def.effect.damage.flatByRank[4] = 200.f;
        def.effect.damage.totalAdRatioByRank[0] = 2.f;
        def.effect.damage.totalAdRatioByRank[1] = 2.f;
        def.effect.damage.totalAdRatioByRank[2] = 2.f;
        def.effect.damage.totalAdRatioByRank[3] = 2.f;
        def.effect.damage.totalAdRatioByRank[4] = 2.f;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 5.f;
        def.effect.params[1].id = eSkillEffectParamId::DamagePerRank;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 20.f;
        def.effect.params[2].id = eSkillEffectParamId::Gap;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 1.35f;
        def.effect.params[3].id = eSkillEffectParamId::Speed;
        def.effect.params[3].rankCount = static_cast<u8_t>(1u);
        def.effect.params[3].valueByRank[0] = 14.f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 0.36f;
        def.stage.commandLockSec[0] = 0.36f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::ForcedMotion;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 3u;
        def.cooldown.rankCount = 3u;
        def.cost.manaCost = 100.f;
        def.cooldown.cooldownSec = 125.f;
        def.range.rangeMax = 12.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 100.f;
        def.cost.manaCostByRank[1] = 100.f;
        def.cost.manaCostByRank[2] = 100.f;
        def.cooldown.cooldownSecByRank[0] = 125.f;
        def.cooldown.cooldownSecByRank[1] = 105.f;
        def.cooldown.cooldownSecByRank[2] = 85.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 3u;
        def.effect.damage.type = eDamageType::Magic;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 125.f;
        def.effect.damage.flatByRank[1] = 200.f;
        def.effect.damage.flatByRank[2] = 275.f;
        def.effect.damage.totalAdRatioByRank[0] = 0.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.f;
        def.effect.damage.totalAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.7f;
        def.effect.damage.apRatioByRank[1] = 0.7f;
        def.effect.damage.apRatioByRank[2] = 0.7f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.f;
        def.effect.paramCount = static_cast<u8_t>(10u);
        def.effect.params[0].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 125.f;
        def.effect.params[1].id = eSkillEffectParamId::DisarmDurationSec;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 1.5f;
        def.effect.params[2].id = eSkillEffectParamId::EffectDurationSec;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 2.5f;
        def.effect.params[3].id = eSkillEffectParamId::MarkDurationSec;
        def.effect.params[3].rankCount = static_cast<u8_t>(1u);
        def.effect.params[3].valueByRank[0] = 5.f;
        def.effect.params[4].id = eSkillEffectParamId::MoveSpeedMul;
        def.effect.params[4].rankCount = static_cast<u8_t>(1u);
        def.effect.params[4].valueByRank[0] = 0.5f;
        def.effect.params[5].id = eSkillEffectParamId::Range;
        def.effect.params[5].rankCount = static_cast<u8_t>(1u);
        def.effect.params[5].valueByRank[0] = 8.f;
        def.effect.params[6].id = eSkillEffectParamId::RectLength;
        def.effect.params[6].rankCount = static_cast<u8_t>(1u);
        def.effect.params[6].valueByRank[0] = 5.f;
        def.effect.params[7].id = eSkillEffectParamId::RectWidth;
        def.effect.params[7].rankCount = static_cast<u8_t>(1u);
        def.effect.params[7].valueByRank[0] = 7.5f;
        def.effect.params[8].id = eSkillEffectParamId::SlowDurationSec;
        def.effect.params[8].rankCount = static_cast<u8_t>(1u);
        def.effect.params[8].valueByRank[0] = 0.5f;
        def.effect.params[9].id = eSkillEffectParamId::Speed;
        def.effect.params[9].rankCount = static_cast<u8_t>(1u);
        def.effect.params[9].valueByRank[0] = 15.f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 0.65f;
        def.stage.commandLockSec[0] = 0.26666667f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::QueueUntilUnlock;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::PressRelease;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 70.f;
        def.cooldown.cooldownSec = 6.f;
        def.range.rangeMax = 0.f;
        def.stage.stageCount = 2u;
        def.stage.stageWindowSec = 4.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 70.f;
        def.cost.manaCostByRank[1] = 75.f;
        def.cost.manaCostByRank[2] = 80.f;
        def.cost.manaCostByRank[3] = 85.f;
        def.cost.manaCostByRank[4] = 90.f;
        def.cooldown.cooldownSecByRank[0] = 6.f;
        def.cooldown.cooldownSecByRank[1] = 5.f;
        def.cooldown.cooldownSecByRank[2] = 5.f;
        def.cooldown.cooldownSecByRank[3] = 5.f;
        def.cooldown.cooldownSecByRank[4] = 4.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 80.f;
        def.effect.damage.flatByRank[1] = 120.f;
        def.effect.damage.flatByRank[2] = 160.f;
        def.effect.damage.flatByRank[3] = 180.f;
        def.effect.damage.flatByRank[4] = 200.f;
        def.effect.damage.totalAdRatioByRank[0] = 2.f;
        def.effect.damage.totalAdRatioByRank[1] = 0.4f;
        def.effect.damage.totalAdRatioByRank[2] = 2.f;
        def.effect.damage.totalAdRatioByRank[3] = 2.f;
        def.effect.damage.totalAdRatioByRank[4] = 2.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.5f;
        def.effect.damage.apRatioByRank[1] = 0.5f;
        def.effect.damage.apRatioByRank[2] = 0.5f;
        def.effect.damage.apRatioByRank[3] = 0.5f;
        def.effect.damage.apRatioByRank[4] = 0.5f;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 0.f;
        def.effect.params[1].id = eSkillEffectParamId::DamagePerRank;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 10.f;
        def.effect.params[2].id = eSkillEffectParamId::HalfWidth;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 2.2f;
        def.effect.params[3].id = eSkillEffectParamId::Range;
        def.effect.params[3].rankCount = static_cast<u8_t>(1u);
        def.effect.params[3].valueByRank[0] = 6.f;
        def.charge.bEnabled = true;
        def.charge.bAutoRelease = true;
        def.charge.maxHoldSec = 4.f;
        def.charge.minRangeScale = 1.f;
        def.charge.maxRangeScale = 1.f;
        def.charge.minDamageScale = 1.f;
        def.charge.maxDamageScale = 3.f;
        def.charge.minStunSec = 0.f;
        def.charge.maxStunSec = 0.f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 5.f;
        def.stage.commandLockSec[0] = 4.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::StationaryChannel;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = true;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
        def.target.shape[1] = eTargetShape::Direction;
        def.stage.lockDurationSec[1] = 0.4f;
        def.stage.commandLockSec[1] = 0.f;
        def.stage.movePolicy[1] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[1] = true;
        def.stage.bPresentationLoopWhileActive[1] = false;
        def.facing.mode[1] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 1u;
        def.cooldown.rankCount = 1u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 1.66999996f;
        def.range.rangeMax = 1.5f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 1.66999996f;
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
        def.stage.lockDurationSec[0] = 0.65f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
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
        def.input.activation = eSkillInputActivation::PressRecast;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 60.f;
        def.effect.params[1].id = eSkillEffectParamId::Radius;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 2.2f;
        def.effect.params[2].id = eSkillEffectParamId::StunDurationSec;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 1.f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 2.f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = true;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.7f;
        def.stage.commandLockSec[1] = 0.f;
        def.stage.movePolicy[1] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[1] = true;
        def.stage.bPresentationLoopWhileActive[1] = false;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 70.f;
        def.effect.params[1].id = eSkillEffectParamId::DashDurationSec;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 0.22f;
        def.effect.params[2].id = eSkillEffectParamId::Gap;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 1.f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 0.6f;
        def.stage.commandLockSec[0] = 0.6f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::ForcedMotion;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
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
        def.input.activation = eSkillInputActivation::Press;
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
        def.effect.paramCount = static_cast<u8_t>(2u);
        def.effect.params[0].id = eSkillEffectParamId::EffectDurationSec;
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 8.f;
        def.effect.params[1].id = eSkillEffectParamId::MaxStacks;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 3.f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.6f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::None;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
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
        def.effect.paramCount = static_cast<u8_t>(1u);
        def.effect.params[0].id = eSkillEffectParamId::EffectDurationSec;
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 5.f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.5f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = false;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::None;
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
        def.input.activation = eSkillInputActivation::Press;
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
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
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
        def.input.activation = eSkillInputActivation::Press;
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
        def.effect.damage.totalAdRatioByRank[0] = 2.f;
        def.effect.damage.totalAdRatioByRank[1] = 2.f;
        def.effect.damage.totalAdRatioByRank[2] = 2.f;
        def.effect.damage.totalAdRatioByRank[3] = 2.f;
        def.effect.damage.totalAdRatioByRank[4] = 2.f;
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
        def.effect.params[0].id = eSkillEffectParamId::DamagePerSpear;
        def.effect.params[0].rankCount = static_cast<u8_t>(5u);
        def.effect.params[0].valueByRank[0] = 30.f;
        def.effect.params[0].valueByRank[1] = 30.f;
        def.effect.params[0].valueByRank[2] = 40.f;
        def.effect.params[0].valueByRank[3] = 50.f;
        def.effect.params[0].valueByRank[4] = 60.f;
        def.effect.params[1].id = eSkillEffectParamId::MoveSpeedMul;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 0.55f;
        def.effect.params[2].id = eSkillEffectParamId::SlowDurationSec;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 2.f;
        def.effect.params[3].id = eSkillEffectParamId::StunDurationSec;
        def.effect.params[3].rankCount = static_cast<u8_t>(1u);
        def.effect.params[3].valueByRank[0] = 1.f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.4f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::None;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 50.f;
        def.cooldown.cooldownSec = 5.f;
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
        def.cooldown.cooldownSecByRank[0] = 5.f;
        def.cooldown.cooldownSecByRank[1] = 4.f;
        def.cooldown.cooldownSecByRank[2] = 1.f;
        def.cooldown.cooldownSecByRank[3] = 3.f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_OnHit;
        def.effect.damage.flatByRank[0] = 70.f;
        def.effect.damage.flatByRank[1] = 100.f;
        def.effect.damage.flatByRank[2] = 200.f;
        def.effect.damage.flatByRank[3] = 300.f;
        def.effect.damage.flatByRank[4] = 350.f;
        def.effect.damage.totalAdRatioByRank[0] = 2.f;
        def.effect.damage.totalAdRatioByRank[1] = 2.f;
        def.effect.damage.totalAdRatioByRank[2] = 2.f;
        def.effect.damage.totalAdRatioByRank[3] = 2.f;
        def.effect.damage.totalAdRatioByRank[4] = 2.f;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 70.f;
        def.effect.params[1].id = eSkillEffectParamId::Radius;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 0.6f;
        def.effect.params[2].id = eSkillEffectParamId::Speed;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 27.f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 0.3f;
        def.stage.commandLockSec[0] = 0.26666667f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::QueueUntilUnlock;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::PressRecast;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::StageDependent;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 1.f;
        def.effect.params[1].id = eSkillEffectParamId::DashDurationSec;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 0.45f;
        def.effect.params[2].id = eSkillEffectParamId::EffectDurationSec;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 4.f;
        def.effect.params[3].id = eSkillEffectParamId::Radius;
        def.effect.params[3].rankCount = static_cast<u8_t>(1u);
        def.effect.params[3].valueByRank[0] = 2.5f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.5f;
        def.stage.commandLockSec[0] = 0.26666667f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::QueueUntilUnlock;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Direction;
        def.stage.lockDurationSec[1] = 0.45f;
        def.stage.commandLockSec[1] = 0.26666667f;
        def.stage.movePolicy[1] = eSkillActionMovePolicy::QueueUntilUnlock;
        def.stage.bCreatesActionState[1] = true;
        def.stage.bPresentationLoopWhileActive[1] = false;
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
        def.input.activation = eSkillInputActivation::Press;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 0.8660254f;
        def.effect.params[1].id = eSkillEffectParamId::Range;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 12.f;
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
        def.stage.commandLockSec[0] = 0.26666667f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::QueueUntilUnlock;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 1u;
        def.cooldown.rankCount = 1u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 0.60000002f;
        def.range.rangeMax = 5.5f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 0.60000002f;
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
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
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
        def.effect.paramCount = static_cast<u8_t>(4u);
        def.effect.params[0].id = eSkillEffectParamId::MarkDurationSec;
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 4.f;
        def.effect.params[1].id = eSkillEffectParamId::MaxStacks;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 3.f;
        def.effect.params[2].id = eSkillEffectParamId::MoveSpeedMul;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 0.65f;
        def.effect.params[3].id = eSkillEffectParamId::SlowDurationSec;
        def.effect.params[3].rankCount = static_cast<u8_t>(1u);
        def.effect.params[3].valueByRank[0] = 1.f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 0.6f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
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
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 0.6f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::Press;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 4.f;
        def.effect.params[1].id = eSkillEffectParamId::HealAmountPerRank;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 75.f;
        def.effect.params[2].id = eSkillEffectParamId::HealBaseAmount;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 250.f;
        def.effect.params[3].id = eSkillEffectParamId::MinHealthAmount;
        def.effect.params[3].rankCount = static_cast<u8_t>(1u);
        def.effect.params[3].valueByRank[0] = 1.f;
        def.effect.params[4].id = eSkillEffectParamId::Radius;
        def.effect.params[4].rankCount = static_cast<u8_t>(1u);
        def.effect.params[4].valueByRank[0] = 6.f;
        def.target.shape[0] = eTargetShape::Ground;
        def.stage.lockDurationSec[0] = 0.6f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 35.f;
        def.effect.params[1].id = eSkillEffectParamId::EffectDurationSec;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 4.f;
        def.effect.params[2].id = eSkillEffectParamId::Radius;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 4.f;
        def.effect.params[3].id = eSkillEffectParamId::TickIntervalSec;
        def.effect.params[3].rankCount = static_cast<u8_t>(1u);
        def.effect.params[3].valueByRank[0] = 0.6f;
        def.target.shape[0] = eTargetShape::Ground;
        def.stage.lockDurationSec[0] = 0.6f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 1u;
        def.cooldown.rankCount = 1u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 0.60000002f;
        def.range.rangeMax = 1.5f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 0.60000002f;
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
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::PressRecast;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
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
        def.cooldown.cooldownSecByRank[2] = 5.f;
        def.cooldown.cooldownSecByRank[3] = 5.f;
        def.cooldown.cooldownSecByRank[4] = 5.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 200.f;
        def.effect.damage.flatByRank[1] = 200.f;
        def.effect.damage.flatByRank[2] = 200.f;
        def.effect.damage.flatByRank[3] = 200.f;
        def.effect.damage.flatByRank[4] = 200.f;
        def.effect.damage.totalAdRatioByRank[0] = 1.f;
        def.effect.damage.totalAdRatioByRank[1] = 1.f;
        def.effect.damage.totalAdRatioByRank[2] = 1.f;
        def.effect.damage.totalAdRatioByRank[3] = 1.f;
        def.effect.damage.totalAdRatioByRank[4] = 1.f;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 0.6f;
        def.effect.params[1].id = eSkillEffectParamId::Radius;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 3.5f;
        def.effect.params[2].id = eSkillEffectParamId::SlowDurationSec;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 1.5f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.6f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.45f;
        def.stage.commandLockSec[1] = 0.f;
        def.stage.movePolicy[1] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[1] = true;
        def.stage.bPresentationLoopWhileActive[1] = false;
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
        def.input.activation = eSkillInputActivation::PressRecast;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::StageDependent;
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
        def.effect.damage.flatByRank[2] = 150.f;
        def.effect.damage.flatByRank[3] = 200.f;
        def.effect.damage.flatByRank[4] = 200.f;
        def.effect.damage.totalAdRatioByRank[0] = 1.f;
        def.effect.damage.totalAdRatioByRank[1] = 1.f;
        def.effect.damage.totalAdRatioByRank[2] = 1.f;
        def.effect.damage.totalAdRatioByRank[3] = 1.f;
        def.effect.damage.totalAdRatioByRank[4] = 1.f;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(5u);
        def.effect.params[0].valueByRank[0] = 95.f;
        def.effect.params[0].valueByRank[1] = 95.f;
        def.effect.params[0].valueByRank[2] = 150.f;
        def.effect.params[0].valueByRank[3] = 200.f;
        def.effect.params[0].valueByRank[4] = 200.f;
        def.effect.params[1].id = eSkillEffectParamId::DashDurationSec;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 0.18f;
        def.effect.params[2].id = eSkillEffectParamId::Gap;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 1.f;
        def.effect.params[3].id = eSkillEffectParamId::MarkDurationSec;
        def.effect.params[3].rankCount = static_cast<u8_t>(1u);
        def.effect.params[3].valueByRank[0] = 3.f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 0.6f;
        def.stage.commandLockSec[0] = 0.26666667f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::QueueUntilUnlock;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
        def.target.shape[1] = eTargetShape::Unit;
        def.stage.lockDurationSec[1] = 0.6f;
        def.stage.commandLockSec[1] = 0.6f;
        def.stage.movePolicy[1] = eSkillActionMovePolicy::ForcedMotion;
        def.stage.bCreatesActionState[1] = true;
        def.stage.bPresentationLoopWhileActive[1] = false;
        def.facing.mode[1] = eSkillFacingMode::TowardsTarget;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 3u;
        def.cooldown.rankCount = 3u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 15.f;
        def.range.rangeMax = 3.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cost.manaCostByRank[1] = 0.f;
        def.cost.manaCostByRank[2] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 15.f;
        def.cooldown.cooldownSecByRank[1] = 15.f;
        def.cooldown.cooldownSecByRank[2] = 10.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 3u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 350.f;
        def.effect.damage.flatByRank[1] = 400.f;
        def.effect.damage.flatByRank[2] = 500.f;
        def.effect.damage.totalAdRatioByRank[0] = 1.f;
        def.effect.damage.totalAdRatioByRank[1] = 1.f;
        def.effect.damage.totalAdRatioByRank[2] = 1.f;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 1.f;
        def.effect.params[1].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 150.f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 0.6f;
        def.stage.commandLockSec[0] = 0.26666667f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::QueueUntilUnlock;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::PressRecast;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::StageDependent;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 0.18f;
        def.effect.params[1].id = eSkillEffectParamId::Gap;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 0.25f;
        def.effect.params[2].id = eSkillEffectParamId::ShieldBaseAmount;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 80.f;
        def.effect.params[3].id = eSkillEffectParamId::ShieldDurationSec;
        def.effect.params[3].rankCount = static_cast<u8_t>(1u);
        def.effect.params[3].valueByRank[0] = 3.f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 0.6f;
        def.stage.commandLockSec[0] = 0.6f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::ForcedMotion;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.45f;
        def.stage.commandLockSec[1] = 0.f;
        def.stage.movePolicy[1] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[1] = true;
        def.stage.bPresentationLoopWhileActive[1] = false;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 1u;
        def.cooldown.rankCount = 1u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 0.60000002f;
        def.range.rangeMax = 1.5f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 0.60000002f;
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
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::Press;
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
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.6f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::None;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
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
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 0.6f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::Press;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 0.25f;
        def.effect.params[1].id = eSkillEffectParamId::EffectDurationSec;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 7.f;
        def.effect.params[2].id = eSkillEffectParamId::MoveSpeedMul;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 1.35f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.6f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::None;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
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
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::None;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 1u;
        def.cooldown.rankCount = 1u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 0.60000002f;
        def.range.rangeMax = 1.5f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 0.60000002f;
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
        def.stage.lockDurationSec[0] = 0.65f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
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
        def.input.activation = eSkillInputActivation::Press;
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
        def.effect.params[0].id = eSkillEffectParamId::ShieldBaseAmount;
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 70.f;
        def.effect.params[1].id = eSkillEffectParamId::ShieldDurationSec;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 3.f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.5f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::Press;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 0.75f;
        def.effect.params[1].id = eSkillEffectParamId::Radius;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 2.25f;
        def.effect.params[2].id = eSkillEffectParamId::StackWindowSec;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 2.f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 0.45f;
        def.stage.commandLockSec[0] = 0.26666667f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::QueueUntilUnlock;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::PressRecast;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::StageDependent;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 100.f;
        def.effect.params[1].id = eSkillEffectParamId::BonusAd;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 20.f;
        def.effect.params[2].id = eSkillEffectParamId::DamagePerRank;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 50.f;
        def.effect.params[3].id = eSkillEffectParamId::EffectDurationSec;
        def.effect.params[3].rankCount = static_cast<u8_t>(1u);
        def.effect.params[3].valueByRank[0] = 15.f;
        def.effect.params[4].id = eSkillEffectParamId::HalfAngleCos;
        def.effect.params[4].rankCount = static_cast<u8_t>(1u);
        def.effect.params[4].valueByRank[0] = 0.70710678f;
        def.effect.params[5].id = eSkillEffectParamId::Range;
        def.effect.params[5].rankCount = static_cast<u8_t>(1u);
        def.effect.params[5].valueByRank[0] = 10.5f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.8f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::None;
        def.target.shape[1] = eTargetShape::Direction;
        def.stage.lockDurationSec[1] = 0.6f;
        def.stage.commandLockSec[1] = 0.f;
        def.stage.movePolicy[1] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[1] = true;
        def.stage.bPresentationLoopWhileActive[1] = false;
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
        def.input.activation = eSkillInputActivation::Press;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 2.5f;
        def.effect.params[1].id = eSkillEffectParamId::StunDurationSec;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 0.75f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.6f;
        def.stage.commandLockSec[0] = 0.26666667f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::QueueUntilUnlock;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::None;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 1u;
        def.cooldown.rankCount = 1u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 0.60000002f;
        def.range.rangeMax = 1.5f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 0.60000002f;
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
        def.effect.paramCount = static_cast<u8_t>(5u);
        def.effect.params[0].id = eSkillEffectParamId::ApRatio;
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 0.6f;
        def.effect.params[1].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 60.f;
        def.effect.params[2].id = eSkillEffectParamId::MaxStacks;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 3.f;
        def.effect.params[3].id = eSkillEffectParamId::Radius;
        def.effect.params[3].rankCount = static_cast<u8_t>(1u);
        def.effect.params[3].valueByRank[0] = 2.75f;
        def.effect.params[4].id = eSkillEffectParamId::StackWindowSec;
        def.effect.params[4].rankCount = static_cast<u8_t>(1u);
        def.effect.params[4].valueByRank[0] = 5.f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 0.6f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::PressRecast;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 0.75f;
        def.effect.params[1].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 65.f;
        def.effect.params[2].id = eSkillEffectParamId::DamagePerRank;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 25.f;
        def.effect.params[3].id = eSkillEffectParamId::DashDistance;
        def.effect.params[3].rankCount = static_cast<u8_t>(1u);
        def.effect.params[3].valueByRank[0] = 3.25f;
        def.effect.params[4].id = eSkillEffectParamId::DashDurationSec;
        def.effect.params[4].rankCount = static_cast<u8_t>(1u);
        def.effect.params[4].valueByRank[0] = 0.16f;
        def.effect.params[5].id = eSkillEffectParamId::Gap;
        def.effect.params[5].rankCount = static_cast<u8_t>(1u);
        def.effect.params[5].valueByRank[0] = 0.85f;
        def.effect.params[6].id = eSkillEffectParamId::MoveSpeedMul;
        def.effect.params[6].rankCount = static_cast<u8_t>(1u);
        def.effect.params[6].valueByRank[0] = 0.6f;
        def.effect.params[7].id = eSkillEffectParamId::Radius;
        def.effect.params[7].rankCount = static_cast<u8_t>(1u);
        def.effect.params[7].valueByRank[0] = 0.55f;
        def.effect.params[8].id = eSkillEffectParamId::SlowDurationSec;
        def.effect.params[8].rankCount = static_cast<u8_t>(1u);
        def.effect.params[8].valueByRank[0] = 1.f;
        def.effect.params[9].id = eSkillEffectParamId::Speed;
        def.effect.params[9].rankCount = static_cast<u8_t>(1u);
        def.effect.params[9].valueByRank[0] = 26.f;
        def.effect.params[10].id = eSkillEffectParamId::TargetDashDurationSec;
        def.effect.params[10].rankCount = static_cast<u8_t>(1u);
        def.effect.params[10].valueByRank[0] = 0.22f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 0.35f;
        def.stage.commandLockSec[0] = 0.35f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::ForcedMotion;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
        def.target.shape[1] = eTargetShape::Direction;
        def.stage.lockDurationSec[1] = 0.5f;
        def.stage.commandLockSec[1] = 0.5f;
        def.stage.movePolicy[1] = eSkillActionMovePolicy::ForcedMotion;
        def.stage.bCreatesActionState[1] = true;
        def.stage.bPresentationLoopWhileActive[1] = false;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
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
        def.effect.damage.type = eDamageType::Magic;
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
        def.effect.paramCount = static_cast<u8_t>(2u);
        def.effect.params[0].id = eSkillEffectParamId::FormationDelaySec;
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 0.5f;
        def.effect.params[1].id = eSkillEffectParamId::Radius;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 1.65f;
        def.target.shape[0] = eTargetShape::Ground;
        def.stage.lockDurationSec[0] = 0.55f;
        def.stage.commandLockSec[0] = 0.26666667f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::QueueUntilUnlock;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::Press;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 45.f;
        def.effect.params[1].id = eSkillEffectParamId::Range;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 10.f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 0.8f;
        def.stage.commandLockSec[0] = 0.26666667f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::QueueUntilUnlock;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
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
        def.effect.damage.type = eDamageType::Magic;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 75.f;
        def.effect.damage.flatByRank[1] = 100.f;
        def.effect.damage.flatByRank[2] = 125.f;
        def.effect.damage.flatByRank[3] = 150.f;
        def.effect.damage.flatByRank[4] = 175.f;
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
        def.effect.params[0].id = eSkillEffectParamId::HealDamageRatio;
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 0.5f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 0.45f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 1u;
        def.cooldown.rankCount = 1u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 0.60000002f;
        def.range.rangeMax = 1.5f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 0.60000002f;
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
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 6.f;
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
        def.cooldown.cooldownSecByRank[0] = 6.f;
        def.cooldown.cooldownSecByRank[1] = 5.f;
        def.cooldown.cooldownSecByRank[2] = 5.f;
        def.cooldown.cooldownSecByRank[3] = 5.f;
        def.cooldown.cooldownSecByRank[4] = 5.f;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 4.f;
        def.effect.params[1].id = eSkillEffectParamId::Radius;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 6.f;
        def.effect.params[2].id = eSkillEffectParamId::RefreshDurationSec;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 0.25f;
        def.effect.params[3].id = eSkillEffectParamId::TickIntervalSec;
        def.effect.params[3].rankCount = static_cast<u8_t>(1u);
        def.effect.params[3].valueByRank[0] = 0.1f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 0.5f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 5.f;
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
        def.cooldown.cooldownSecByRank[0] = 5.f;
        def.cooldown.cooldownSecByRank[1] = 4.5f;
        def.cooldown.cooldownSecByRank[2] = 4.f;
        def.cooldown.cooldownSecByRank[3] = 3.5f;
        def.cooldown.cooldownSecByRank[4] = 3.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 80.f;
        def.effect.damage.flatByRank[1] = 100.f;
        def.effect.damage.flatByRank[2] = 200.f;
        def.effect.damage.flatByRank[3] = 200.f;
        def.effect.damage.flatByRank[4] = 200.f;
        def.effect.damage.totalAdRatioByRank[0] = 2.f;
        def.effect.damage.totalAdRatioByRank[1] = 2.f;
        def.effect.damage.totalAdRatioByRank[2] = 2.f;
        def.effect.damage.totalAdRatioByRank[3] = 2.f;
        def.effect.damage.totalAdRatioByRank[4] = 2.f;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 25.f;
        def.effect.params[1].id = eSkillEffectParamId::Radius;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 0.9f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 0.45f;
        def.stage.commandLockSec[0] = 0.26666667f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::QueueUntilUnlock;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 3u;
        def.cooldown.rankCount = 3u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 120.f;
        def.range.rangeMax = 6.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cost.manaCostByRank[1] = 0.f;
        def.cost.manaCostByRank[2] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 120.f;
        def.cooldown.cooldownSecByRank[1] = 100.f;
        def.cooldown.cooldownSecByRank[2] = 80.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 3u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 300.f;
        def.effect.damage.flatByRank[1] = 400.f;
        def.effect.damage.flatByRank[2] = 500.f;
        def.effect.damage.totalAdRatioByRank[0] = 2.f;
        def.effect.damage.totalAdRatioByRank[1] = 2.f;
        def.effect.damage.totalAdRatioByRank[2] = 2.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.apRatioByRank[0] = 0.f;
        def.effect.damage.apRatioByRank[1] = 0.f;
        def.effect.damage.apRatioByRank[2] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[0] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[1] = 0.f;
        def.effect.damage.targetMaxHpRatioByRank[2] = 0.f;
        def.effect.damage.targetMissingHpRatioByRank[0] = 0.12f;
        def.effect.damage.targetMissingHpRatioByRank[1] = 0.16f;
        def.effect.damage.targetMissingHpRatioByRank[2] = 0.2f;
        def.effect.paramCount = static_cast<u8_t>(5u);
        def.effect.params[0].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 0.f;
        def.effect.params[1].id = eSkillEffectParamId::DashDurationSec;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 0.18f;
        def.effect.params[2].id = eSkillEffectParamId::MoveSpeedMul;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 0.6f;
        def.effect.params[3].id = eSkillEffectParamId::Radius;
        def.effect.params[3].rankCount = static_cast<u8_t>(1u);
        def.effect.params[3].valueByRank[0] = 2.f;
        def.effect.params[4].id = eSkillEffectParamId::SlowDurationSec;
        def.effect.params[4].rankCount = static_cast<u8_t>(1u);
        def.effect.params[4].valueByRank[0] = 1.f;
        def.target.shape[0] = eTargetShape::Ground;
        def.stage.lockDurationSec[0] = 0.8f;
        def.stage.commandLockSec[0] = 0.8f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::ForcedMotion;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::PressRelease;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 5.f;
        def.range.rangeMax = 5.f;
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
        def.cooldown.cooldownSecByRank[0] = 5.f;
        def.cooldown.cooldownSecByRank[1] = 5.f;
        def.cooldown.cooldownSecByRank[2] = 5.f;
        def.cooldown.cooldownSecByRank[3] = 5.f;
        def.cooldown.cooldownSecByRank[4] = 5.f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Magic;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 80.f;
        def.effect.damage.flatByRank[1] = 135.f;
        def.effect.damage.flatByRank[2] = 190.f;
        def.effect.damage.flatByRank[3] = 245.f;
        def.effect.damage.flatByRank[4] = 300.f;
        def.effect.damage.totalAdRatioByRank[0] = 1.f;
        def.effect.damage.totalAdRatioByRank[1] = 1.f;
        def.effect.damage.totalAdRatioByRank[2] = 1.f;
        def.effect.damage.totalAdRatioByRank[3] = 2.f;
        def.effect.damage.totalAdRatioByRank[4] = 1.f;
        def.effect.damage.bonusAdRatioByRank[0] = 0.f;
        def.effect.damage.bonusAdRatioByRank[1] = 0.f;
        def.effect.damage.bonusAdRatioByRank[2] = 0.f;
        def.effect.damage.bonusAdRatioByRank[3] = 0.f;
        def.effect.damage.bonusAdRatioByRank[4] = 0.f;
        def.effect.damage.apRatioByRank[0] = 1.f;
        def.effect.damage.apRatioByRank[1] = 1.f;
        def.effect.damage.apRatioByRank[2] = 1.f;
        def.effect.damage.apRatioByRank[3] = 1.f;
        def.effect.damage.apRatioByRank[4] = 1.f;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 80.f;
        def.effect.params[1].id = eSkillEffectParamId::DashDurationSec;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 0.26f;
        def.effect.params[2].id = eSkillEffectParamId::Radius;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 0.75f;
        def.effect.params[3].id = eSkillEffectParamId::StunDurationSec;
        def.effect.params[3].rankCount = static_cast<u8_t>(1u);
        def.effect.params[3].valueByRank[0] = 2.f;
        def.charge.bEnabled = true;
        def.charge.bAutoRelease = true;
        def.charge.maxHoldSec = 4.f;
        def.charge.minRangeScale = 0.5f;
        def.charge.maxRangeScale = 1.f;
        def.charge.minDamageScale = 1.f;
        def.charge.maxDamageScale = 1.f;
        def.charge.minStunSec = 0.25f;
        def.charge.maxStunSec = 2.f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 0.7f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = true;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
        def.target.shape[1] = eTargetShape::Direction;
        def.stage.lockDurationSec[1] = 0.3f;
        def.stage.commandLockSec[1] = 0.3f;
        def.stage.movePolicy[1] = eSkillActionMovePolicy::ForcedMotion;
        def.stage.bCreatesActionState[1] = true;
        def.stage.bPresentationLoopWhileActive[1] = false;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 1u;
        def.cooldown.rankCount = 1u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 0.60000002f;
        def.range.rangeMax = 2.5f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 0.f;
        def.cooldown.cooldownSecByRank[0] = 0.60000002f;
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
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 0.1f;
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
        def.cooldown.cooldownSecByRank[0] = 0.1f;
        def.cooldown.cooldownSecByRank[1] = 0.1f;
        def.cooldown.cooldownSecByRank[2] = 0.1f;
        def.cooldown.cooldownSecByRank[3] = 0.1f;
        def.cooldown.cooldownSecByRank[4] = 0.1f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Magic;
        def.effect.damage.flags = DamageFlag_None;
        def.effect.damage.flatByRank[0] = 200.f;
        def.effect.damage.flatByRank[1] = 200.f;
        def.effect.damage.flatByRank[2] = 200.f;
        def.effect.damage.flatByRank[3] = 200.f;
        def.effect.damage.flatByRank[4] = 200.f;
        def.effect.damage.totalAdRatioByRank[0] = 1.f;
        def.effect.damage.totalAdRatioByRank[1] = 1.f;
        def.effect.damage.totalAdRatioByRank[2] = 1.f;
        def.effect.damage.totalAdRatioByRank[3] = 1.f;
        def.effect.damage.totalAdRatioByRank[4] = 1.f;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 80.f;
        def.effect.params[1].id = eSkillEffectParamId::DashDistance;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 5.5f;
        def.effect.params[2].id = eSkillEffectParamId::DashDurationSec;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 0.25f;
        def.effect.params[3].id = eSkillEffectParamId::EffectDurationSec;
        def.effect.params[3].rankCount = static_cast<u8_t>(1u);
        def.effect.params[3].valueByRank[0] = 0.5f;
        def.effect.params[4].id = eSkillEffectParamId::Gap;
        def.effect.params[4].rankCount = static_cast<u8_t>(1u);
        def.effect.params[4].valueByRank[0] = 0.75f;
        def.effect.params[5].id = eSkillEffectParamId::MarkDurationSec;
        def.effect.params[5].rankCount = static_cast<u8_t>(1u);
        def.effect.params[5].valueByRank[0] = 10.f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 0.4f;
        def.stage.commandLockSec[0] = 0.4f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::ForcedMotion;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
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
        def.input.activation = eSkillInputActivation::Press;
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
        def.cooldown.cooldownSecByRank[2] = 2.f;
        def.cooldown.cooldownSecByRank[3] = 1.5f;
        def.cooldown.cooldownSecByRank[4] = 1.5f;
        def.effect.damage.bValid = true;
        def.effect.damage.rankCount = 5u;
        def.effect.damage.type = eDamageType::Physical;
        def.effect.damage.flags = DamageFlag_OnHit;
        def.effect.damage.flatByRank[0] = 60.f;
        def.effect.damage.flatByRank[1] = 80.f;
        def.effect.damage.flatByRank[2] = 200.f;
        def.effect.damage.flatByRank[3] = 250.f;
        def.effect.damage.flatByRank[4] = 300.f;
        def.effect.damage.totalAdRatioByRank[0] = 2.f;
        def.effect.damage.totalAdRatioByRank[1] = 2.f;
        def.effect.damage.totalAdRatioByRank[2] = 2.f;
        def.effect.damage.totalAdRatioByRank[3] = 2.f;
        def.effect.damage.totalAdRatioByRank[4] = 2.f;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 1.25f;
        def.effect.params[1].id = eSkillEffectParamId::DashAreaDamage;
        def.effect.params[1].rankCount = static_cast<u8_t>(5u);
        def.effect.params[1].valueByRank[0] = 70.f;
        def.effect.params[1].valueByRank[1] = 90.f;
        def.effect.params[1].valueByRank[2] = 200.f;
        def.effect.params[1].valueByRank[3] = 250.f;
        def.effect.params[1].valueByRank[4] = 300.f;
        def.effect.params[2].id = eSkillEffectParamId::DashAreaRadius;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 2.5f;
        def.effect.params[3].id = eSkillEffectParamId::EffectDurationSec;
        def.effect.params[3].rankCount = static_cast<u8_t>(1u);
        def.effect.params[3].valueByRank[0] = 0.5f;
        def.effect.params[4].id = eSkillEffectParamId::Radius;
        def.effect.params[4].rankCount = static_cast<u8_t>(1u);
        def.effect.params[4].valueByRank[0] = 0.8f;
        def.effect.params[5].id = eSkillEffectParamId::Speed;
        def.effect.params[5].rankCount = static_cast<u8_t>(1u);
        def.effect.params[5].valueByRank[0] = 25.f;
        def.effect.params[6].id = eSkillEffectParamId::StackWindowSec;
        def.effect.params[6].rankCount = static_cast<u8_t>(1u);
        def.effect.params[6].valueByRank[0] = 6.f;
        def.effect.params[7].id = eSkillEffectParamId::TornadoDamage;
        def.effect.params[7].rankCount = static_cast<u8_t>(5u);
        def.effect.params[7].valueByRank[0] = 100.f;
        def.effect.params[7].valueByRank[1] = 120.f;
        def.effect.params[7].valueByRank[2] = 200.f;
        def.effect.params[7].valueByRank[3] = 250.f;
        def.effect.params[7].valueByRank[4] = 300.f;
        def.effect.params[8].id = eSkillEffectParamId::TornadoDurationSec;
        def.effect.params[8].rankCount = static_cast<u8_t>(1u);
        def.effect.params[8].valueByRank[0] = 1.5f;
        def.effect.params[9].id = eSkillEffectParamId::TornadoRadius;
        def.effect.params[9].rankCount = static_cast<u8_t>(1u);
        def.effect.params[9].valueByRank[0] = 2.25f;
        def.effect.params[10].id = eSkillEffectParamId::TornadoSpeed;
        def.effect.params[10].rankCount = static_cast<u8_t>(1u);
        def.effect.params[10].valueByRank[0] = 18.f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 0.5f;
        def.stage.commandLockSec[0] = 0.26666667f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::QueueUntilUnlock;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
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
        def.effect.damage.totalAdRatioByRank[0] = 3.f;
        def.effect.damage.totalAdRatioByRank[1] = 3.f;
        def.effect.damage.totalAdRatioByRank[2] = 3.f;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 1.f;
        def.effect.params[1].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 200.f;
        def.effect.params[2].id = eSkillEffectParamId::Gap;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 1.1f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.6f;
        def.stage.commandLockSec[0] = 0.6f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::ForcedMotion;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::None;
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
        def.input.activation = eSkillInputActivation::Press;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 4.f;
        def.effect.params[1].id = eSkillEffectParamId::FormationDelaySec;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 0.25f;
        def.effect.params[2].id = eSkillEffectParamId::RectLength;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 1.6f;
        def.effect.params[3].id = eSkillEffectParamId::RectLengthPerRank;
        def.effect.params[3].rankCount = static_cast<u8_t>(1u);
        def.effect.params[3].valueByRank[0] = 0.35f;
        def.effect.params[4].id = eSkillEffectParamId::RectWidth;
        def.effect.params[4].rankCount = static_cast<u8_t>(1u);
        def.effect.params[4].valueByRank[0] = 0.5f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 0.25f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::Press;
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
        def.stage.lockDurationSec[0] = 0.75f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
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
        def.input.activation = eSkillInputActivation::PressRecast;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 4.f;
        def.effect.params[1].id = eSkillEffectParamId::DashDurationSec;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 0.25f;
        def.effect.params[2].id = eSkillEffectParamId::EffectDurationSec;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 5.f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 0.75f;
        def.stage.commandLockSec[0] = 0.75f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::ForcedMotion;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
        def.target.shape[1] = eTargetShape::Direction;
        def.stage.lockDurationSec[1] = 0.6f;
        def.stage.commandLockSec[1] = 0.6f;
        def.stage.movePolicy[1] = eSkillActionMovePolicy::ForcedMotion;
        def.stage.bCreatesActionState[1] = true;
        def.stage.bPresentationLoopWhileActive[1] = false;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 75.f;
        def.effect.params[1].id = eSkillEffectParamId::Radius;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 0.85f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 0.6f;
        def.stage.commandLockSec[0] = 0.26666667f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::QueueUntilUnlock;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::Press;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 0.75f;
        def.effect.params[1].id = eSkillEffectParamId::BaseDamage;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 150.f;
        def.effect.params[2].id = eSkillEffectParamId::DashDelaySec;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 0.5f;
        def.effect.params[3].id = eSkillEffectParamId::DashDurationSec;
        def.effect.params[3].rankCount = static_cast<u8_t>(1u);
        def.effect.params[3].valueByRank[0] = 0.16f;
        def.effect.params[4].id = eSkillEffectParamId::Gap;
        def.effect.params[4].rankCount = static_cast<u8_t>(1u);
        def.effect.params[4].valueByRank[0] = 0.75f;
        def.effect.params[5].id = eSkillEffectParamId::Radius;
        def.effect.params[5].rankCount = static_cast<u8_t>(1u);
        def.effect.params[5].valueByRank[0] = 1.7f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 1.2f;
        def.stage.commandLockSec[0] = 1.2f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::ForcedMotion;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 65.f;
        def.effect.params[1].id = eSkillEffectParamId::Radius;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 1.5f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 0.6f;
        def.stage.commandLockSec[0] = 0.26666667f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::QueueUntilUnlock;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::Press;
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
        def.effect.paramCount = static_cast<u8_t>(2u);
        def.effect.params[0].id = eSkillEffectParamId::MissingHealthDamageRatio;
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 0.1f;
        def.effect.params[1].id = eSkillEffectParamId::TargetHealthThresholdRatio;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 0.5f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 0.65f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
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
        def.input.activation = eSkillInputActivation::Press;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::Direct;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 40.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 2.5f;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 65.f;
        def.effect.params[1].id = eSkillEffectParamId::DamagePerRank;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 20.f;
        def.effect.params[2].id = eSkillEffectParamId::MoveSpeedMul;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 0.6f;
        def.effect.params[3].id = eSkillEffectParamId::Radius;
        def.effect.params[3].rankCount = static_cast<u8_t>(1u);
        def.effect.params[3].valueByRank[0] = 2.75f;
        def.effect.params[4].id = eSkillEffectParamId::SlowDurationSec;
        def.effect.params[4].rankCount = static_cast<u8_t>(1u);
        def.effect.params[4].valueByRank[0] = 1.5f;
        def.target.shape[0] = eTargetShape::Self;
        def.stage.lockDurationSec[0] = 0.6f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::None;
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
        def.input.activation = eSkillInputActivation::Press;
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
        def.cost.manaCostByRank[1] = 70.f;
        def.cost.manaCostByRank[2] = 65.f;
        def.cost.manaCostByRank[3] = 60.f;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 70.f;
        def.effect.params[1].id = eSkillEffectParamId::DamagePerRank;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 25.f;
        def.effect.params[2].id = eSkillEffectParamId::Radius;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 0.45f;
        def.effect.params[3].id = eSkillEffectParamId::Speed;
        def.effect.params[3].rankCount = static_cast<u8_t>(1u);
        def.effect.params[3].valueByRank[0] = 12.f;
        def.target.shape[0] = eTargetShape::Direction;
        def.stage.lockDurationSec[0] = 0.5f;
        def.stage.commandLockSec[0] = 0.26666667f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::QueueUntilUnlock;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
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
        def.input.activation = eSkillInputActivation::PressRecast;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::StageDependent;
        def.cost.rankCount = 3u;
        def.cooldown.rankCount = 3u;
        def.cost.manaCost = 0.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 6.25f;
        def.stage.stageCount = 2u;
        def.stage.stageWindowSec = 4.f;
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
        def.effect.paramCount = static_cast<u8_t>(4u);
        def.effect.params[0].id = eSkillEffectParamId::EffectDurationSec;
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 4.f;
        def.effect.params[1].id = eSkillEffectParamId::Gap;
        def.effect.params[1].rankCount = static_cast<u8_t>(1u);
        def.effect.params[1].valueByRank[0] = 0.75f;
        def.effect.params[2].id = eSkillEffectParamId::MarkDurationSec;
        def.effect.params[2].rankCount = static_cast<u8_t>(1u);
        def.effect.params[2].valueByRank[0] = 3.f;
        def.effect.params[3].id = eSkillEffectParamId::VanishDurationSec;
        def.effect.params[3].rankCount = static_cast<u8_t>(1u);
        def.effect.params[3].valueByRank[0] = 0.75f;
        def.target.shape[0] = eTargetShape::Unit;
        def.stage.lockDurationSec[0] = 1.5f;
        def.stage.commandLockSec[0] = 1.5f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::ForcedMotion;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsTarget;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.25f;
        def.stage.commandLockSec[1] = 0.25f;
        def.stage.movePolicy[1] = eSkillActionMovePolicy::ForcedMotion;
        def.stage.bCreatesActionState[1] = true;
        def.stage.bPresentationLoopWhileActive[1] = false;
        def.facing.mode[1] = eSkillFacingMode::None;
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
        def.input.activation = eSkillInputActivation::PressRecast;
        def.target.bValid = true;
        def.target.resolvePolicy = eTargetResolvePolicy::StageDependent;
        def.cost.rankCount = 5u;
        def.cooldown.rankCount = 5u;
        def.cost.manaCost = 40.f;
        def.cooldown.cooldownSec = 3.f;
        def.range.rangeMax = 6.5f;
        def.stage.stageCount = 2u;
        def.stage.stageWindowSec = 4.f;
        def.effect.scalingTableId = 0u;
        def.effect.gameplayPolicyId = 0u;
        def.effect.replicatedCueId = 0u;
        def.cost.manaCostByRank[0] = 40.f;
        def.cost.manaCostByRank[1] = 35.f;
        def.cost.manaCostByRank[2] = 30.f;
        def.cost.manaCostByRank[3] = 25.f;
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
        def.effect.params[0].rankCount = static_cast<u8_t>(1u);
        def.effect.params[0].valueByRank[0] = 4.f;
        def.target.shape[0] = eTargetShape::Ground;
        def.stage.lockDurationSec[0] = 0.5f;
        def.stage.commandLockSec[0] = 0.f;
        def.stage.movePolicy[0] = eSkillActionMovePolicy::Allow;
        def.stage.bCreatesActionState[0] = true;
        def.stage.bPresentationLoopWhileActive[0] = false;
        def.facing.mode[0] = eSkillFacingMode::TowardsCommandDirection;
        def.target.shape[1] = eTargetShape::Self;
        def.stage.lockDurationSec[1] = 0.25f;
        def.stage.commandLockSec[1] = 0.25f;
        def.stage.movePolicy[1] = eSkillActionMovePolicy::ForcedMotion;
        def.stage.bCreatesActionState[1] = true;
        def.stage.bPresentationLoopWhileActive[1] = false;
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
        def.cooldownSec = 20.f;
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
        def.respawnDelaySecByLevel[0u] = 10.f;
        def.respawnDelaySecByLevel[1u] = 10.f;
        def.respawnDelaySecByLevel[2u] = 12.f;
        def.respawnDelaySecByLevel[3u] = 12.f;
        def.respawnDelaySecByLevel[4u] = 14.f;
        def.respawnDelaySecByLevel[5u] = 16.f;
        def.respawnDelaySecByLevel[6u] = 20.f;
        def.respawnDelaySecByLevel[7u] = 25.f;
        def.respawnDelaySecByLevel[8u] = 28.f;
        def.respawnDelaySecByLevel[9u] = 32.5f;
        def.respawnDelaySecByLevel[10u] = 35.f;
        def.respawnDelaySecByLevel[11u] = 37.5f;
        def.respawnDelaySecByLevel[12u] = 40.f;
        def.respawnDelaySecByLevel[13u] = 42.5f;
        def.respawnDelaySecByLevel[14u] = 45.f;
        def.respawnDelaySecByLevel[15u] = 47.5f;
        def.respawnDelaySecByLevel[16u] = 50.f;
        def.respawnDelaySecByLevel[17u] = 52.5f;
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
        def.turretMaxHp = 6000.f;
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
        def.respawnDelaySec = 30.f;
        return def;
    }

    JungleCampGameDef MakeJungleCamp_0()
    {
        JungleCampGameDef def{};
        def.maxHp = 10000.f;
        def.radius = 2.5f;
        def.attackRange = 4.f;
        def.attackDamage = 120.f;
        def.attackCooldown = 1.2f;
        def.moveSpeed = 2.5f;
        def.baseArmor = 30.f;
        def.baseMr = 30.f;
        def.aggroRange = 2.5f;
        def.leashRange = 8.f;
        def.respawnDelaySec = 30.f;
        return def;
    }

    JungleCampGameDef MakeJungleCamp_1()
    {
        JungleCampGameDef def{};
        def.maxHp = 10000.f;
        def.radius = 2.2f;
        def.attackRange = 3.f;
        def.attackDamage = 90.f;
        def.attackCooldown = 1.5f;
        def.moveSpeed = 4.f;
        def.baseArmor = 30.f;
        def.baseMr = 30.f;
        def.aggroRange = 2.5f;
        def.leashRange = 8.f;
        def.respawnDelaySec = 30.f;
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
        def.respawnDelaySec = 30.f;
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
        def.respawnDelaySec = 30.f;
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
        def.respawnDelaySec = 30.f;
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
        def.respawnDelaySec = 30.f;
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
        def.respawnDelaySec = 30.f;
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
        def.respawnDelaySec = 30.f;
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
        def.respawnDelaySec = 30.f;
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
        def.respawnDelaySec = 30.f;
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
        def.respawnDelaySec = 30.f;
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
        def.attackDamage = 10.f;
        def.attackCooldownMax = 1.666667f;
        def.maxHp = 225.f;
        return def;
    }

    MinionCombatDef MakeMinionCombat_1()
    {
        MinionCombatDef def{};
        def.moveSpeed = 4.f;
        def.attackRange = 5.6f;
        def.sightRange = 14.f;
        def.attackDamage = 20.f;
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

    MinionBehaviorDef MakeMinionBehaviorDef()
    {
        MinionBehaviorDef def{};
        def.pathAgentRadius = 0.5f;
        def.laneClearanceRadius = 0.5f;
        def.softSeparationRadiusScale = 0.65f;
        def.softSeparationWeight = 0.35f;
        def.defaultSeparationWeight = 0.55f;
        def.softSeparationMaxStep = 0.18f;
        def.lanePathRebuildIntervalSec = 1.f;
        def.chasePathRebuildIntervalSec = 0.2f;
        def.pathTargetRefreshDistanceSq = 0.1225f;
        def.pathWaypointArriveRadius = 0.35f;
        def.flowFieldProgressSlackSq = 0.01f;
        def.structureAcquireRangePadding = 0.75f;
        def.targetScanIntervalSec = 0.15f;
        def.attackExitRangePadding = 0.18f;
        def.meleeAttackWindupSec = 0.3666667f;
        def.rangedAttackWindupSec = 0.4666667f;
        def.attackRecoverySec = 0.3666667f;
        def.pathBuildBudgetPerTick = 4u;
        def.blockedFramesBeforeRepath = static_cast<u8_t>(6u);
        def.flowFieldStallFramesBeforePathFallback = static_cast<u8_t>(4u);
        def.targetScanStaggerBuckets = 10u;
        def.rangedRoleType = static_cast<u8_t>(1u);
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
        def.startX = 5.f;
        def.rangedProjectile.speed = 14.f;
        def.rangedProjectile.hitRadius = 0.45f;
        def.rangedProjectile.forwardOffset = 0.45f;
        def.rangedProjectile.spawnHeight = 0.85f;
        def.rangedProjectile.maxDistancePadding = 2.f;
        def.formationSlots[0] = MinionSpawnSlotDef{ static_cast<u8_t>(0u), 3.6f, -0.9f };
        def.formationSlots[1] = MinionSpawnSlotDef{ static_cast<u8_t>(0u), 4.8f, 0.f };
        def.formationSlots[2] = MinionSpawnSlotDef{ static_cast<u8_t>(0u), 6.f, 0.9f };
        def.formationSlots[3] = MinionSpawnSlotDef{ static_cast<u8_t>(1u), 0.f, -0.9f };
        def.formationSlots[4] = MinionSpawnSlotDef{ static_cast<u8_t>(1u), 1.2f, 0.f };
        def.formationSlots[5] = MinionSpawnSlotDef{ static_cast<u8_t>(1u), 2.4f, 0.9f };
        def.formationSlotCount = static_cast<u8_t>(6u);
        def.siegeSlot = MinionSpawnSlotDef{ static_cast<u8_t>(2u), 7.2f, 0.f };
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
        def.turretGold = 1500.f;
        def.turretTeamGold = 1000.f;
        def.jungle.smallCampGold = 80.f;
        def.jungle.smallCampXP = 240.f;
        def.jungle.epicGold = 0.f;
        def.jungle.epicXP = 0.f;
        def.jungle.baronGold = 0.f;
        def.jungle.baronXP = 0.f;
        def.objectives.teamGoldPerChampion = 2000.f;
        def.objectives.buffDurationSec = 300.f;
        def.objectives.baronRecallDurationMultiplier = 0.5f;
        def.objectives.baronAuraRadius = 12.f;
        def.objectives.baronMinionHpMultiplier = 3.f;
        def.objectives.baronMinionAttackDamageMultiplier = 2.f;
        def.objectives.baronMinionScaleMultiplier = 2.f;
        def.objectives.elderAttackDamageMultiplier = 1.7f;
        def.objectives.elderBurnDurationSec = 3.f;
        def.objectives.elderBurnTickIntervalSec = 1.f;
        def.objectives.elderBurnTargetMaxHpRatioPerTick = 0.01f;
        def.objectives.elderExecuteThresholdRatio = 0.2f;
        def.objectives.blueManaRegenPerSec = 10.f;
        def.objectives.redHealthRegenPerSec = 10.f;
        def.objectives.redBurnDurationSec = 3.f;
        def.objectives.redBurnTickIntervalSec = 1.f;
        def.objectives.redBurnDamagePerTick = 10.f;
        def.objectives.teamLevelGrant = 3u;
        def.passiveGoldStartTick = 3300ull;
        def.passiveGoldIntervalTicks = 30ull;
        def.passiveGoldPerGrant = 2u;
        def.assistCreditWindowSec = 10.f;
        def.recallDurationSec = 6.f;
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

    ItemDef MakeItem_1004()
    {
        ItemDef def{};
        def.itemId = 1004u;
        def.price = 200u;
        def.displayName = "Faerie Charm";
        return def;
    }

    ItemDef MakeItem_1006()
    {
        ItemDef def{};
        def.itemId = 1006u;
        def.price = 300u;
        def.displayName = "Rejuvenation Bead";
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
        def.price = 300u;
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
        def.price = 850u;
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

    ItemDef MakeItem_1082()
    {
        ItemDef def{};
        def.itemId = 1082u;
        def.price = 350u;
        def.stats.flatAp = 15.f;
        def.stats.flatHealth = 50.f;
        def.displayName = "Dark Seal";
        return def;
    }

    ItemDef MakeItem_1083()
    {
        ItemDef def{};
        def.itemId = 1083u;
        def.price = 450u;
        def.stats.flatAd = 7.f;
        def.displayName = "Cull";
        return def;
    }

    ItemDef MakeItem_1086()
    {
        ItemDef def{};
        def.itemId = 1086u;
        def.price = 400u;
        def.stats.flatAd = 8.f;
        def.stats.bonusAttackSpeed = 0.15f;
        def.displayName = "Doran's Bow";
        return def;
    }

    ItemDef MakeItem_1101()
    {
        ItemDef def{};
        def.itemId = 1101u;
        def.price = 450u;
        def.displayName = "Scorchclaw Pup";
        return def;
    }

    ItemDef MakeItem_1102()
    {
        ItemDef def{};
        def.itemId = 1102u;
        def.price = 450u;
        def.displayName = "Gustwalker Hatchling";
        return def;
    }

    ItemDef MakeItem_1103()
    {
        ItemDef def{};
        def.itemId = 1103u;
        def.price = 450u;
        def.displayName = "Mosstomper Seedling";
        return def;
    }

    ItemDef MakeItem_1105()
    {
        ItemDef def{};
        def.itemId = 1105u;
        def.price = 450u;
        def.displayName = "Mosstomper Seedling";
        return def;
    }

    ItemDef MakeItem_1106()
    {
        ItemDef def{};
        def.itemId = 1106u;
        def.price = 450u;
        def.displayName = "Gustwalker Hatchling";
        return def;
    }

    ItemDef MakeItem_1107()
    {
        ItemDef def{};
        def.itemId = 1107u;
        def.price = 450u;
        def.displayName = "Scorchclaw Pup";
        return def;
    }

    ItemDef MakeItem_1120()
    {
        ItemDef def{};
        def.itemId = 1120u;
        def.price = 450u;
        def.stats.flatHealth = 150.f;
        def.stats.flatArmor = 8.f;
        def.stats.flatMr = 8.f;
        def.displayName = "Doran's Helm";
        return def;
    }

    ItemDef MakeItem_2003()
    {
        ItemDef def{};
        def.itemId = 2003u;
        def.price = 50u;
        def.displayName = "Health Potion";
        return def;
    }

    ItemDef MakeItem_2019()
    {
        ItemDef def{};
        def.itemId = 2019u;
        def.price = 1100u;
        def.stats.flatAd = 15.f;
        def.stats.flatArmor = 30.f;
        def.displayName = "Steel Sigil";
        return def;
    }

    ItemDef MakeItem_2020()
    {
        ItemDef def{};
        def.itemId = 2020u;
        def.price = 1337u;
        def.stats.flatAd = 25.f;
        def.stats.abilityHaste = 10.f;
        def.stats.lethality = 5.f;
        def.displayName = "The Brutalizer";
        return def;
    }

    ItemDef MakeItem_2021()
    {
        ItemDef def{};
        def.itemId = 2021u;
        def.price = 1150u;
        def.stats.flatAd = 15.f;
        def.stats.flatHealth = 250.f;
        def.displayName = "Tunneler";
        return def;
    }

    ItemDef MakeItem_2022()
    {
        ItemDef def{};
        def.itemId = 2022u;
        def.price = 250u;
        def.stats.abilityHaste = 5.f;
        def.displayName = "Glowing Mote";
        return def;
    }

    ItemDef MakeItem_2031()
    {
        ItemDef def{};
        def.itemId = 2031u;
        def.price = 150u;
        def.displayName = "Refillable Potion";
        return def;
    }

    ItemDef MakeItem_2051()
    {
        ItemDef def{};
        def.itemId = 2051u;
        def.price = 950u;
        def.stats.flatHealth = 150.f;
        def.displayName = "Guardian's Horn";
        return def;
    }

    ItemDef MakeItem_2055()
    {
        ItemDef def{};
        def.itemId = 2055u;
        def.price = 75u;
        def.displayName = "Control Ward";
        return def;
    }

    ItemDef MakeItem_2065()
    {
        ItemDef def{};
        def.itemId = 2065u;
        def.price = 2200u;
        def.stats.flatAp = 50.f;
        def.stats.abilityHaste = 15.f;
        def.stats.percentMoveSpeed = 0.04f;
        def.displayName = "Shurelya's Battlesong";
        return def;
    }

    ItemDef MakeItem_2138()
    {
        ItemDef def{};
        def.itemId = 2138u;
        def.price = 500u;
        def.displayName = "Elixir of Iron";
        return def;
    }

    ItemDef MakeItem_2139()
    {
        ItemDef def{};
        def.itemId = 2139u;
        def.price = 500u;
        def.displayName = "Elixir of Sorcery";
        return def;
    }

    ItemDef MakeItem_2140()
    {
        ItemDef def{};
        def.itemId = 2140u;
        def.price = 500u;
        def.displayName = "Elixir of Wrath";
        return def;
    }

    ItemDef MakeItem_2141()
    {
        ItemDef def{};
        def.itemId = 2141u;
        def.price = 300u;
        def.displayName = "Cappa Juice";
        return def;
    }

    ItemDef MakeItem_2420()
    {
        ItemDef def{};
        def.itemId = 2420u;
        def.price = 1600u;
        def.stats.flatAp = 40.f;
        def.stats.flatArmor = 25.f;
        def.displayName = "Seeker's Armguard";
        return def;
    }

    ItemDef MakeItem_2501()
    {
        ItemDef def{};
        def.itemId = 2501u;
        def.price = 3300u;
        def.stats.flatAd = 30.f;
        def.stats.flatHealth = 550.f;
        def.displayName = "Overlord's Bloodmail";
        return def;
    }

    ItemDef MakeItem_2502()
    {
        ItemDef def{};
        def.itemId = 2502u;
        def.price = 2800u;
        def.stats.flatHealth = 400.f;
        def.stats.flatArmor = 50.f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Unending Despair";
        return def;
    }

    ItemDef MakeItem_2503()
    {
        ItemDef def{};
        def.itemId = 2503u;
        def.price = 2800u;
        def.stats.flatAp = 80.f;
        def.stats.flatMana = 600.f;
        def.stats.abilityHaste = 20.f;
        def.displayName = "Blackfire Torch";
        return def;
    }

    ItemDef MakeItem_2504()
    {
        ItemDef def{};
        def.itemId = 2504u;
        def.price = 2900u;
        def.stats.flatHealth = 400.f;
        def.stats.flatMr = 80.f;
        def.displayName = "Kaenic Rookern";
        return def;
    }

    ItemDef MakeItem_2508()
    {
        ItemDef def{};
        def.itemId = 2508u;
        def.price = 900u;
        def.stats.flatAp = 30.f;
        def.displayName = "Fated Ashes";
        return def;
    }

    ItemDef MakeItem_2510()
    {
        ItemDef def{};
        def.itemId = 2510u;
        def.price = 3100u;
        def.stats.flatAp = 60.f;
        def.stats.flatHealth = 300.f;
        def.stats.bonusAttackSpeed = 0.2f;
        def.stats.abilityHaste = 20.f;
        def.displayName = "Dusk and Dawn";
        return def;
    }

    ItemDef MakeItem_2512()
    {
        ItemDef def{};
        def.itemId = 2512u;
        def.price = 2650u;
        def.stats.bonusAttackSpeed = 0.45f;
        def.stats.critChance = 0.25f;
        def.stats.percentMoveSpeed = 0.04f;
        def.displayName = "Fiendhunter Bolts";
        return def;
    }

    ItemDef MakeItem_2517()
    {
        ItemDef def{};
        def.itemId = 2517u;
        def.price = 3100u;
        def.stats.flatAd = 65.f;
        def.displayName = "Endless Hunger";
        return def;
    }

    ItemDef MakeItem_2520()
    {
        ItemDef def{};
        def.itemId = 2520u;
        def.price = 3200u;
        def.stats.flatAd = 55.f;
        def.stats.abilityHaste = 15.f;
        def.stats.lethality = 22.f;
        def.displayName = "Bastionbreaker";
        return def;
    }

    ItemDef MakeItem_2522()
    {
        ItemDef def{};
        def.itemId = 2522u;
        def.price = 2800u;
        def.stats.flatAp = 90.f;
        def.stats.flatMana = 300.f;
        def.stats.abilityHaste = 10.f;
        def.displayName = "Actualizer";
        return def;
    }

    ItemDef MakeItem_2523()
    {
        ItemDef def{};
        def.itemId = 2523u;
        def.price = 2800u;
        def.stats.flatAd = 55.f;
        def.stats.critChance = 0.25f;
        def.displayName = "Hexoptics C44";
        return def;
    }

    ItemDef MakeItem_2524()
    {
        ItemDef def{};
        def.itemId = 2524u;
        def.price = 2300u;
        def.stats.flatHealth = 200.f;
        def.stats.flatArmor = 20.f;
        def.stats.flatMr = 20.f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Bandlepipes";
        return def;
    }

    ItemDef MakeItem_2525()
    {
        ItemDef def{};
        def.itemId = 2525u;
        def.price = 2600u;
        def.stats.flatHealth = 600.f;
        def.stats.abilityHaste = 20.f;
        def.displayName = "Protoplasm Harness";
        return def;
    }

    ItemDef MakeItem_2526()
    {
        ItemDef def{};
        def.itemId = 2526u;
        def.price = 2250u;
        def.stats.flatHealth = 200.f;
        def.stats.flatMana = 300.f;
        def.displayName = "Whispering Circlet";
        return def;
    }

    ItemDef MakeItem_3003()
    {
        ItemDef def{};
        def.itemId = 3003u;
        def.price = 2900u;
        def.stats.flatAp = 70.f;
        def.stats.flatMana = 600.f;
        def.stats.abilityHaste = 25.f;
        def.displayName = "Archangel's Staff";
        return def;
    }

    ItemDef MakeItem_3004()
    {
        ItemDef def{};
        def.itemId = 3004u;
        def.price = 2900u;
        def.stats.flatAd = 35.f;
        def.stats.flatMana = 500.f;
        def.stats.abilityHaste = 15.f;
        def.manaflow.bValid = true;
        def.manaflow.rechargeSec = 8.f;
        def.manaflow.maxCharges = 4u;
        def.manaflow.manaPerTrigger = 3u;
        def.manaflow.championMultiplier = 2u;
        def.manaflow.maxBonusMana = 360u;
        def.manaflow.transformItemId = 3042u;
        def.maxManaBonusAdRatio = 0.02f;
        def.displayName = "Manamune";
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

    ItemDef MakeItem_3008()
    {
        ItemDef def{};
        def.itemId = 3008u;
        def.price = 1000u;
        def.stats.flatMoveSpeed = 45.f;
        def.displayName = "Gluttonous Greaves";
        return def;
    }

    ItemDef MakeItem_3009()
    {
        ItemDef def{};
        def.itemId = 3009u;
        def.price = 1000u;
        def.stats.flatMoveSpeed = 55.f;
        def.displayName = "Boots of Swiftness";
        return def;
    }

    ItemDef MakeItem_3020()
    {
        ItemDef def{};
        def.itemId = 3020u;
        def.price = 1100u;
        def.stats.flatMagicPen = 12.f;
        def.stats.flatMoveSpeed = 45.f;
        def.displayName = "Sorcerer's Shoes";
        return def;
    }

    ItemDef MakeItem_3024()
    {
        ItemDef def{};
        def.itemId = 3024u;
        def.price = 900u;
        def.stats.flatMana = 300.f;
        def.stats.flatArmor = 25.f;
        def.stats.abilityHaste = 10.f;
        def.displayName = "Glacial Buckler";
        return def;
    }

    ItemDef MakeItem_3026()
    {
        ItemDef def{};
        def.itemId = 3026u;
        def.price = 3200u;
        def.stats.flatAd = 55.f;
        def.stats.flatArmor = 45.f;
        def.displayName = "Guardian Angel";
        return def;
    }

    ItemDef MakeItem_3031()
    {
        ItemDef def{};
        def.itemId = 3031u;
        def.price = 3500u;
        def.stats.flatAd = 75.f;
        def.stats.critChance = 0.25f;
        def.stats.critDamageBonus = 0.3f;
        def.displayName = "Infinity Edge";
        return def;
    }

    ItemDef MakeItem_3032()
    {
        ItemDef def{};
        def.itemId = 3032u;
        def.price = 3100u;
        def.stats.flatAd = 50.f;
        def.stats.bonusAttackSpeed = 0.4f;
        def.displayName = "Yun Tal Wildarrows";
        return def;
    }

    ItemDef MakeItem_3033()
    {
        ItemDef def{};
        def.itemId = 3033u;
        def.price = 3000u;
        def.stats.flatAd = 35.f;
        def.stats.critChance = 0.25f;
        def.stats.armorPenPercent = 0.3f;
        def.displayName = "Mortal Reminder";
        return def;
    }

    ItemDef MakeItem_3035()
    {
        ItemDef def{};
        def.itemId = 3035u;
        def.price = 1450u;
        def.stats.flatAd = 20.f;
        def.stats.armorPenPercent = 0.18f;
        def.displayName = "Last Whisper";
        return def;
    }

    ItemDef MakeItem_3036()
    {
        ItemDef def{};
        def.itemId = 3036u;
        def.price = 3300u;
        def.stats.flatAd = 35.f;
        def.stats.critChance = 0.25f;
        def.stats.armorPenPercent = 0.35f;
        def.displayName = "Lord Dominik's Regards";
        return def;
    }

    ItemDef MakeItem_3041()
    {
        ItemDef def{};
        def.itemId = 3041u;
        def.price = 1500u;
        def.stats.flatAp = 20.f;
        def.stats.flatHealth = 100.f;
        def.displayName = "Mejai's Soulstealer";
        return def;
    }

    ItemDef MakeItem_3042()
    {
        ItemDef def{};
        def.itemId = 3042u;
        def.price = 2900u;
        def.bPurchasable = false;
        def.stats.flatAd = 35.f;
        def.stats.flatMana = 1000.f;
        def.stats.abilityHaste = 15.f;
        def.maxManaBonusAdRatio = 0.02f;
        def.displayName = "Muramana";
        return def;
    }

    ItemDef MakeItem_3044()
    {
        ItemDef def{};
        def.itemId = 3044u;
        def.price = 1100u;
        def.stats.flatAd = 15.f;
        def.stats.flatHealth = 200.f;
        def.displayName = "Phage";
        return def;
    }

    ItemDef MakeItem_3046()
    {
        ItemDef def{};
        def.itemId = 3046u;
        def.price = 2650u;
        def.stats.bonusAttackSpeed = 0.65f;
        def.stats.critChance = 0.25f;
        def.stats.percentMoveSpeed = 0.1f;
        def.displayName = "Phantom Dancer";
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

    ItemDef MakeItem_3050()
    {
        ItemDef def{};
        def.itemId = 3050u;
        def.price = 2200u;
        def.stats.flatHealth = 300.f;
        def.stats.flatArmor = 25.f;
        def.stats.flatMr = 25.f;
        def.stats.abilityHaste = 10.f;
        def.displayName = "Zeke's Convergence";
        return def;
    }

    ItemDef MakeItem_3051()
    {
        ItemDef def{};
        def.itemId = 3051u;
        def.price = 1200u;
        def.stats.flatAd = 20.f;
        def.stats.bonusAttackSpeed = 0.2f;
        def.displayName = "Hearthbound Axe";
        return def;
    }

    ItemDef MakeItem_3053()
    {
        ItemDef def{};
        def.itemId = 3053u;
        def.price = 3200u;
        def.stats.flatHealth = 400.f;
        def.displayName = "Sterak's Gage";
        return def;
    }

    ItemDef MakeItem_3057()
    {
        ItemDef def{};
        def.itemId = 3057u;
        def.price = 900u;
        def.stats.abilityHaste = 10.f;
        def.displayName = "Sheen";
        return def;
    }

    ItemDef MakeItem_3065()
    {
        ItemDef def{};
        def.itemId = 3065u;
        def.price = 2700u;
        def.stats.flatHealth = 400.f;
        def.stats.flatMr = 50.f;
        def.stats.abilityHaste = 10.f;
        def.displayName = "Spirit Visage";
        return def;
    }

    ItemDef MakeItem_3066()
    {
        ItemDef def{};
        def.itemId = 3066u;
        def.price = 800u;
        def.stats.flatHealth = 200.f;
        def.stats.percentMoveSpeed = 0.04f;
        def.displayName = "Winged Moonplate";
        return def;
    }

    ItemDef MakeItem_3067()
    {
        ItemDef def{};
        def.itemId = 3067u;
        def.price = 800u;
        def.stats.flatHealth = 200.f;
        def.stats.abilityHaste = 10.f;
        def.displayName = "Kindlegem";
        return def;
    }

    ItemDef MakeItem_3068()
    {
        ItemDef def{};
        def.itemId = 3068u;
        def.price = 2700u;
        def.stats.flatHealth = 350.f;
        def.stats.flatArmor = 50.f;
        def.stats.abilityHaste = 10.f;
        def.displayName = "Sunfire Aegis";
        return def;
    }

    ItemDef MakeItem_3070()
    {
        ItemDef def{};
        def.itemId = 3070u;
        def.price = 400u;
        def.stats.flatMana = 240.f;
        def.displayName = "Tear of the Goddess";
        return def;
    }

    ItemDef MakeItem_3071()
    {
        ItemDef def{};
        def.itemId = 3071u;
        def.price = 3000u;
        def.stats.flatAd = 40.f;
        def.stats.flatHealth = 400.f;
        def.stats.abilityHaste = 20.f;
        def.displayName = "Black Cleaver";
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

    ItemDef MakeItem_3073()
    {
        ItemDef def{};
        def.itemId = 3073u;
        def.price = 3000u;
        def.stats.flatAd = 40.f;
        def.stats.flatHealth = 450.f;
        def.stats.bonusAttackSpeed = 0.2f;
        def.displayName = "Experimental Hexplate";
        return def;
    }

    ItemDef MakeItem_3074()
    {
        ItemDef def{};
        def.itemId = 3074u;
        def.price = 3300u;
        def.stats.flatAd = 65.f;
        def.stats.abilityHaste = 15.f;
        def.stats.lifeSteal = 0.12f;
        def.displayName = "Ravenous Hydra";
        return def;
    }

    ItemDef MakeItem_3075()
    {
        ItemDef def{};
        def.itemId = 3075u;
        def.price = 2450u;
        def.stats.flatHealth = 150.f;
        def.stats.flatArmor = 75.f;
        def.displayName = "Thornmail";
        return def;
    }

    ItemDef MakeItem_3076()
    {
        ItemDef def{};
        def.itemId = 3076u;
        def.price = 800u;
        def.stats.flatArmor = 30.f;
        def.displayName = "Bramble Vest";
        return def;
    }

    ItemDef MakeItem_3077()
    {
        ItemDef def{};
        def.itemId = 3077u;
        def.price = 1200u;
        def.stats.flatAd = 20.f;
        def.displayName = "Tiamat";
        return def;
    }

    ItemDef MakeItem_3078()
    {
        ItemDef def{};
        def.itemId = 3078u;
        def.price = 3333u;
        def.stats.flatAd = 36.f;
        def.stats.flatHealth = 333.f;
        def.stats.bonusAttackSpeed = 0.3f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Trinity Force";
        return def;
    }

    ItemDef MakeItem_3082()
    {
        ItemDef def{};
        def.itemId = 3082u;
        def.price = 1000u;
        def.stats.flatArmor = 40.f;
        def.displayName = "Warden's Mail";
        return def;
    }

    ItemDef MakeItem_3083()
    {
        ItemDef def{};
        def.itemId = 3083u;
        def.price = 3100u;
        def.stats.flatHealth = 1000.f;
        def.displayName = "Warmog's Armor";
        return def;
    }

    ItemDef MakeItem_3084()
    {
        ItemDef def{};
        def.itemId = 3084u;
        def.price = 3000u;
        def.stats.flatHealth = 900.f;
        def.displayName = "Heartsteel";
        return def;
    }

    ItemDef MakeItem_3085()
    {
        ItemDef def{};
        def.itemId = 3085u;
        def.price = 2650u;
        def.stats.bonusAttackSpeed = 0.4f;
        def.stats.critChance = 0.25f;
        def.stats.percentMoveSpeed = 0.04f;
        def.displayName = "Runaan's Hurricane";
        return def;
    }

    ItemDef MakeItem_3086()
    {
        ItemDef def{};
        def.itemId = 3086u;
        def.price = 1200u;
        def.stats.bonusAttackSpeed = 0.15f;
        def.stats.critChance = 0.15f;
        def.stats.percentMoveSpeed = 0.04f;
        def.displayName = "Zeal";
        return def;
    }

    ItemDef MakeItem_3087()
    {
        ItemDef def{};
        def.itemId = 3087u;
        def.price = 3000u;
        def.stats.flatAd = 45.f;
        def.stats.flatAp = 45.f;
        def.stats.bonusAttackSpeed = 0.3f;
        def.stats.percentMoveSpeed = 0.04f;
        def.displayName = "Statikk Shiv";
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

    ItemDef MakeItem_3091()
    {
        ItemDef def{};
        def.itemId = 3091u;
        def.price = 2800u;
        def.stats.flatMr = 45.f;
        def.stats.bonusAttackSpeed = 0.5f;
        def.displayName = "Wit's End";
        return def;
    }

    ItemDef MakeItem_3094()
    {
        ItemDef def{};
        def.itemId = 3094u;
        def.price = 2650u;
        def.stats.bonusAttackSpeed = 0.35f;
        def.stats.critChance = 0.25f;
        def.stats.percentMoveSpeed = 0.04f;
        def.displayName = "Rapid Firecannon";
        return def;
    }

    ItemDef MakeItem_3097()
    {
        ItemDef def{};
        def.itemId = 3097u;
        def.price = 3200u;
        def.stats.flatAd = 50.f;
        def.stats.bonusAttackSpeed = 0.2f;
        def.stats.critChance = 0.25f;
        def.displayName = "Stormrazor";
        return def;
    }

    ItemDef MakeItem_3100()
    {
        ItemDef def{};
        def.itemId = 3100u;
        def.price = 2900u;
        def.stats.flatAp = 100.f;
        def.stats.abilityHaste = 10.f;
        def.stats.percentMoveSpeed = 0.06f;
        def.displayName = "Lich Bane";
        return def;
    }

    ItemDef MakeItem_3102()
    {
        ItemDef def{};
        def.itemId = 3102u;
        def.price = 3000u;
        def.stats.flatAp = 105.f;
        def.stats.flatMr = 40.f;
        def.displayName = "Banshee's Veil";
        return def;
    }

    ItemDef MakeItem_3107()
    {
        ItemDef def{};
        def.itemId = 3107u;
        def.price = 2300u;
        def.stats.flatAp = 30.f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Redemption";
        return def;
    }

    ItemDef MakeItem_3108()
    {
        ItemDef def{};
        def.itemId = 3108u;
        def.price = 850u;
        def.stats.flatAp = 25.f;
        def.stats.abilityHaste = 10.f;
        def.displayName = "Fiendish Codex";
        return def;
    }

    ItemDef MakeItem_3109()
    {
        ItemDef def{};
        def.itemId = 3109u;
        def.price = 2300u;
        def.stats.flatHealth = 200.f;
        def.stats.flatArmor = 40.f;
        def.stats.abilityHaste = 10.f;
        def.displayName = "Knight's Vow";
        return def;
    }

    ItemDef MakeItem_3110()
    {
        ItemDef def{};
        def.itemId = 3110u;
        def.price = 2500u;
        def.stats.flatMana = 400.f;
        def.stats.flatArmor = 75.f;
        def.stats.abilityHaste = 20.f;
        def.displayName = "Frozen Heart";
        return def;
    }

    ItemDef MakeItem_3111()
    {
        ItemDef def{};
        def.itemId = 3111u;
        def.price = 1250u;
        def.stats.flatMr = 20.f;
        def.stats.flatMoveSpeed = 45.f;
        def.displayName = "Mercury's Treads";
        return def;
    }

    ItemDef MakeItem_3112()
    {
        ItemDef def{};
        def.itemId = 3112u;
        def.price = 950u;
        def.stats.flatAp = 50.f;
        def.stats.flatHealth = 150.f;
        def.displayName = "Guardian's Orb";
        return def;
    }

    ItemDef MakeItem_3113()
    {
        ItemDef def{};
        def.itemId = 3113u;
        def.price = 900u;
        def.stats.flatAp = 30.f;
        def.stats.percentMoveSpeed = 0.04f;
        def.displayName = "Aether Wisp";
        return def;
    }

    ItemDef MakeItem_3114()
    {
        ItemDef def{};
        def.itemId = 3114u;
        def.price = 600u;
        def.displayName = "Forbidden Idol";
        return def;
    }

    ItemDef MakeItem_3115()
    {
        ItemDef def{};
        def.itemId = 3115u;
        def.price = 2900u;
        def.stats.flatAp = 80.f;
        def.stats.bonusAttackSpeed = 0.5f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Nashor's Tooth";
        return def;
    }

    ItemDef MakeItem_3116()
    {
        ItemDef def{};
        def.itemId = 3116u;
        def.price = 2600u;
        def.stats.flatAp = 65.f;
        def.stats.flatHealth = 400.f;
        def.displayName = "Rylai's Crystal Scepter";
        return def;
    }

    ItemDef MakeItem_3118()
    {
        ItemDef def{};
        def.itemId = 3118u;
        def.price = 2700u;
        def.stats.flatAp = 90.f;
        def.stats.flatMana = 600.f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Malignance";
        return def;
    }

    ItemDef MakeItem_3119()
    {
        ItemDef def{};
        def.itemId = 3119u;
        def.price = 2400u;
        def.stats.flatHealth = 550.f;
        def.stats.flatMana = 500.f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Winter's Approach";
        return def;
    }

    ItemDef MakeItem_3123()
    {
        ItemDef def{};
        def.itemId = 3123u;
        def.price = 800u;
        def.stats.flatAd = 15.f;
        def.displayName = "Executioner's Calling";
        return def;
    }

    ItemDef MakeItem_3124()
    {
        ItemDef def{};
        def.itemId = 3124u;
        def.price = 3000u;
        def.stats.flatAd = 30.f;
        def.stats.flatAp = 30.f;
        def.stats.bonusAttackSpeed = 0.25f;
        def.displayName = "Guinsoo's Rageblade";
        return def;
    }

    ItemDef MakeItem_3133()
    {
        ItemDef def{};
        def.itemId = 3133u;
        def.price = 1050u;
        def.stats.flatAd = 20.f;
        def.stats.abilityHaste = 10.f;
        def.displayName = "Caulfield's Warhammer";
        return def;
    }

    ItemDef MakeItem_3134()
    {
        ItemDef def{};
        def.itemId = 3134u;
        def.price = 1000u;
        def.stats.flatAd = 20.f;
        def.stats.lethality = 10.f;
        def.displayName = "Serrated Dirk";
        return def;
    }

    ItemDef MakeItem_3135()
    {
        ItemDef def{};
        def.itemId = 3135u;
        def.price = 3000u;
        def.stats.flatAp = 95.f;
        def.stats.magicPenPercent = 0.4f;
        def.displayName = "Void Staff";
        return def;
    }

    ItemDef MakeItem_3137()
    {
        ItemDef def{};
        def.itemId = 3137u;
        def.price = 3000u;
        def.stats.flatAp = 75.f;
        def.stats.abilityHaste = 20.f;
        def.stats.magicPenPercent = 0.3f;
        def.displayName = "Cryptbloom";
        return def;
    }

    ItemDef MakeItem_3139()
    {
        ItemDef def{};
        def.itemId = 3139u;
        def.price = 3200u;
        def.stats.flatAd = 50.f;
        def.stats.flatMr = 35.f;
        def.stats.lifeSteal = 0.1f;
        def.active.bValid = true;
        def.active.kind = eItemActiveKind::Cleanse;
        def.active.cooldownSec = 90.f;
        def.active.durationSec = 0.f;
        def.displayName = "Mercurial Scimitar";
        return def;
    }

    ItemDef MakeItem_3140()
    {
        ItemDef def{};
        def.itemId = 3140u;
        def.price = 1300u;
        def.stats.flatMr = 30.f;
        def.active.bValid = true;
        def.active.kind = eItemActiveKind::Cleanse;
        def.active.cooldownSec = 90.f;
        def.active.durationSec = 0.f;
        def.displayName = "Quicksilver Sash";
        return def;
    }

    ItemDef MakeItem_3142()
    {
        ItemDef def{};
        def.itemId = 3142u;
        def.price = 2800u;
        def.stats.flatAd = 55.f;
        def.stats.percentMoveSpeed = 0.04f;
        def.stats.lethality = 18.f;
        def.displayName = "Youmuu's Ghostblade";
        return def;
    }

    ItemDef MakeItem_3143()
    {
        ItemDef def{};
        def.itemId = 3143u;
        def.price = 2700u;
        def.stats.flatHealth = 350.f;
        def.stats.flatArmor = 75.f;
        def.displayName = "Randuin's Omen";
        return def;
    }

    ItemDef MakeItem_3144()
    {
        ItemDef def{};
        def.itemId = 3144u;
        def.price = 600u;
        def.stats.bonusAttackSpeed = 0.2f;
        def.displayName = "Scout's Slingshot";
        return def;
    }

    ItemDef MakeItem_3145()
    {
        ItemDef def{};
        def.itemId = 3145u;
        def.price = 1100u;
        def.stats.flatAp = 45.f;
        def.displayName = "Hextech Alternator";
        return def;
    }

    ItemDef MakeItem_3146()
    {
        ItemDef def{};
        def.itemId = 3146u;
        def.price = 3000u;
        def.stats.flatAd = 40.f;
        def.stats.flatAp = 80.f;
        def.displayName = "Hextech Gunblade";
        return def;
    }

    ItemDef MakeItem_3147()
    {
        ItemDef def{};
        def.itemId = 3147u;
        def.price = 1300u;
        def.stats.flatAp = 30.f;
        def.stats.flatHealth = 200.f;
        def.displayName = "Haunting Guise";
        return def;
    }

    ItemDef MakeItem_3152()
    {
        ItemDef def{};
        def.itemId = 3152u;
        def.price = 2650u;
        def.stats.flatAp = 60.f;
        def.stats.flatHealth = 350.f;
        def.stats.abilityHaste = 20.f;
        def.displayName = "Hextech Rocketbelt";
        return def;
    }

    ItemDef MakeItem_3153()
    {
        ItemDef def{};
        def.itemId = 3153u;
        def.price = 3200u;
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
        def.displayName = "Blade of The Ruined King";
        return def;
    }

    ItemDef MakeItem_3155()
    {
        ItemDef def{};
        def.itemId = 3155u;
        def.price = 1300u;
        def.stats.flatAd = 25.f;
        def.stats.flatMr = 25.f;
        def.displayName = "Hexdrinker";
        return def;
    }

    ItemDef MakeItem_3156()
    {
        ItemDef def{};
        def.itemId = 3156u;
        def.price = 3100u;
        def.stats.flatAd = 60.f;
        def.stats.flatMr = 40.f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Maw of Malmortius";
        return def;
    }

    ItemDef MakeItem_3157()
    {
        ItemDef def{};
        def.itemId = 3157u;
        def.price = 3250u;
        def.stats.flatAp = 105.f;
        def.stats.flatArmor = 50.f;
        def.active.bValid = true;
        def.active.kind = eItemActiveKind::Stasis;
        def.active.cooldownSec = 120.f;
        def.active.durationSec = 3.f;
        def.displayName = "Zhonya's Hourglass";
        return def;
    }

    ItemDef MakeItem_3158()
    {
        ItemDef def{};
        def.itemId = 3158u;
        def.price = 900u;
        def.stats.abilityHaste = 10.f;
        def.stats.flatMoveSpeed = 45.f;
        def.displayName = "Ionian Boots of Lucidity";
        return def;
    }

    ItemDef MakeItem_3161()
    {
        ItemDef def{};
        def.itemId = 3161u;
        def.price = 3100u;
        def.stats.flatAd = 45.f;
        def.stats.flatHealth = 450.f;
        def.displayName = "Spear of Shojin";
        return def;
    }

    ItemDef MakeItem_3165()
    {
        ItemDef def{};
        def.itemId = 3165u;
        def.price = 2850u;
        def.stats.flatAp = 75.f;
        def.stats.flatHealth = 350.f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Morellonomicon";
        return def;
    }

    ItemDef MakeItem_3168()
    {
        ItemDef def{};
        def.itemId = 3168u;
        def.price = 1000u;
        def.stats.flatMoveSpeed = 45.f;
        def.displayName = "Immortal Path";
        return def;
    }

    ItemDef MakeItem_3170()
    {
        ItemDef def{};
        def.itemId = 3170u;
        def.price = 1000u;
        def.stats.flatMoveSpeed = 65.f;
        def.displayName = "Swiftmarch";
        return def;
    }

    ItemDef MakeItem_3171()
    {
        ItemDef def{};
        def.itemId = 3171u;
        def.price = 900u;
        def.stats.abilityHaste = 20.f;
        def.stats.flatMoveSpeed = 45.f;
        def.displayName = "Crimson Lucidity";
        return def;
    }

    ItemDef MakeItem_3172()
    {
        ItemDef def{};
        def.itemId = 3172u;
        def.price = 1100u;
        def.stats.bonusAttackSpeed = 0.4f;
        def.stats.flatMoveSpeed = 45.f;
        def.stats.lifeSteal = 0.05f;
        def.displayName = "Gunmetal Greaves";
        return def;
    }

    ItemDef MakeItem_3173()
    {
        ItemDef def{};
        def.itemId = 3173u;
        def.price = 1250u;
        def.stats.flatMr = 30.f;
        def.stats.flatMoveSpeed = 45.f;
        def.displayName = "Chainlaced Crushers";
        return def;
    }

    ItemDef MakeItem_3174()
    {
        ItemDef def{};
        def.itemId = 3174u;
        def.price = 1200u;
        def.stats.flatArmor = 35.f;
        def.stats.flatMoveSpeed = 45.f;
        def.displayName = "Armored Advance";
        return def;
    }

    ItemDef MakeItem_3175()
    {
        ItemDef def{};
        def.itemId = 3175u;
        def.price = 1100u;
        def.stats.magicPenPercent = 0.08f;
        def.stats.flatMagicPen = 18.f;
        def.stats.flatMoveSpeed = 45.f;
        def.displayName = "Spellslinger's Shoes";
        return def;
    }

    ItemDef MakeItem_3177()
    {
        ItemDef def{};
        def.itemId = 3177u;
        def.price = 950u;
        def.stats.flatAd = 30.f;
        def.stats.flatHealth = 150.f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Guardian's Blade";
        return def;
    }

    ItemDef MakeItem_3179()
    {
        ItemDef def{};
        def.itemId = 3179u;
        def.price = 2800u;
        def.stats.flatAd = 60.f;
        def.stats.abilityHaste = 15.f;
        def.stats.lethality = 18.f;
        def.displayName = "Umbral Glaive";
        return def;
    }

    ItemDef MakeItem_3181()
    {
        ItemDef def{};
        def.itemId = 3181u;
        def.price = 3000u;
        def.stats.flatAd = 40.f;
        def.stats.flatHealth = 500.f;
        def.stats.percentMoveSpeed = 0.04f;
        def.displayName = "Hullbreaker";
        return def;
    }

    ItemDef MakeItem_3184()
    {
        ItemDef def{};
        def.itemId = 3184u;
        def.price = 950u;
        def.stats.flatAd = 25.f;
        def.stats.flatHealth = 150.f;
        def.stats.lifeSteal = 0.05f;
        def.displayName = "Guardian's Hammer";
        return def;
    }

    ItemDef MakeItem_3190()
    {
        ItemDef def{};
        def.itemId = 3190u;
        def.price = 2200u;
        def.stats.flatHealth = 200.f;
        def.stats.flatArmor = 30.f;
        def.stats.flatMr = 30.f;
        def.stats.abilityHaste = 10.f;
        def.displayName = "Locket of the Iron Solari";
        return def;
    }

    ItemDef MakeItem_3211()
    {
        ItemDef def{};
        def.itemId = 3211u;
        def.price = 1250u;
        def.stats.flatHealth = 200.f;
        def.stats.flatMr = 35.f;
        def.displayName = "Spectre's Cowl";
        return def;
    }

    ItemDef MakeItem_3222()
    {
        ItemDef def{};
        def.itemId = 3222u;
        def.price = 2300u;
        def.stats.flatHealth = 250.f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Mikael's Blessing";
        return def;
    }

    ItemDef MakeItem_3302()
    {
        ItemDef def{};
        def.itemId = 3302u;
        def.price = 3000u;
        def.stats.flatAd = 30.f;
        def.stats.bonusAttackSpeed = 0.35f;
        def.displayName = "Terminus";
        return def;
    }

    ItemDef MakeItem_3340()
    {
        ItemDef def{};
        def.itemId = 3340u;
        def.price = 0u;
        def.bPurchasable = false;
        def.active.bValid = true;
        def.active.kind = eItemActiveKind::Ward;
        def.active.cooldownSec = 0.f;
        def.active.durationSec = 0.f;
        def.displayName = "Stealth Ward";
        return def;
    }

    ItemDef MakeItem_3504()
    {
        ItemDef def{};
        def.itemId = 3504u;
        def.price = 2200u;
        def.stats.flatAp = 45.f;
        def.stats.percentMoveSpeed = 0.04f;
        def.displayName = "Ardent Censer";
        return def;
    }

    ItemDef MakeItem_3508()
    {
        ItemDef def{};
        def.itemId = 3508u;
        def.price = 3050u;
        def.stats.flatAd = 50.f;
        def.stats.critChance = 0.25f;
        def.stats.abilityHaste = 20.f;
        def.spellblade.bValid = true;
        def.spellblade.cooldownSec = 1.5f;
        def.spellblade.baseAdRatio = 1.25f;
        def.spellblade.critChanceFlatScale = 50.f;
        def.spellblade.manaRestoreRatio = 0.5f;
        def.displayName = "Essence Reaver";
        return def;
    }

    ItemDef MakeItem_3599()
    {
        ItemDef def{};
        def.itemId = 3599u;
        def.price = 0u;
        def.bPurchasable = false;
        def.active.bValid = true;
        def.active.kind = eItemActiveKind::KalistaOathsworn;
        def.active.cooldownSec = 0.f;
        def.active.durationSec = 0.f;
        def.displayName = "Kalista's Black Spear";
        return def;
    }

    ItemDef MakeItem_3742()
    {
        ItemDef def{};
        def.itemId = 3742u;
        def.price = 2900u;
        def.stats.flatHealth = 350.f;
        def.stats.flatArmor = 55.f;
        def.stats.percentMoveSpeed = 0.04f;
        def.displayName = "Dead Man's Plate";
        return def;
    }

    ItemDef MakeItem_3748()
    {
        ItemDef def{};
        def.itemId = 3748u;
        def.price = 3300u;
        def.stats.flatAd = 40.f;
        def.stats.flatHealth = 600.f;
        def.displayName = "Titanic Hydra";
        return def;
    }

    ItemDef MakeItem_3801()
    {
        ItemDef def{};
        def.itemId = 3801u;
        def.price = 800u;
        def.stats.flatHealth = 200.f;
        def.displayName = "Crystalline Bracer";
        return def;
    }

    ItemDef MakeItem_3802()
    {
        ItemDef def{};
        def.itemId = 3802u;
        def.price = 1200u;
        def.stats.flatAp = 40.f;
        def.stats.flatMana = 300.f;
        def.stats.abilityHaste = 10.f;
        def.displayName = "Lost Chapter";
        return def;
    }

    ItemDef MakeItem_3803()
    {
        ItemDef def{};
        def.itemId = 3803u;
        def.price = 1300u;
        def.stats.flatHealth = 300.f;
        def.stats.flatMana = 375.f;
        def.displayName = "Catalyst of Aeons";
        return def;
    }

    ItemDef MakeItem_3814()
    {
        ItemDef def{};
        def.itemId = 3814u;
        def.price = 3000u;
        def.stats.flatAd = 50.f;
        def.stats.flatHealth = 250.f;
        def.stats.lethality = 15.f;
        def.displayName = "Edge of Night";
        return def;
    }

    ItemDef MakeItem_3865()
    {
        ItemDef def{};
        def.itemId = 3865u;
        def.price = 400u;
        def.stats.flatHealth = 30.f;
        def.displayName = "World Atlas";
        return def;
    }

    ItemDef MakeItem_3869()
    {
        ItemDef def{};
        def.itemId = 3869u;
        def.price = 400u;
        def.stats.flatHealth = 200.f;
        def.displayName = "Celestial Opposition";
        return def;
    }

    ItemDef MakeItem_3870()
    {
        ItemDef def{};
        def.itemId = 3870u;
        def.price = 400u;
        def.stats.flatHealth = 200.f;
        def.displayName = "Dream Maker";
        return def;
    }

    ItemDef MakeItem_3871()
    {
        ItemDef def{};
        def.itemId = 3871u;
        def.price = 400u;
        def.stats.flatHealth = 200.f;
        def.displayName = "Zaz'Zak's Realmspike";
        return def;
    }

    ItemDef MakeItem_3876()
    {
        ItemDef def{};
        def.itemId = 3876u;
        def.price = 400u;
        def.stats.flatHealth = 200.f;
        def.displayName = "Solstice Sleigh";
        return def;
    }

    ItemDef MakeItem_3877()
    {
        ItemDef def{};
        def.itemId = 3877u;
        def.price = 400u;
        def.stats.flatHealth = 200.f;
        def.displayName = "Bloodsong";
        return def;
    }

    ItemDef MakeItem_3916()
    {
        ItemDef def{};
        def.itemId = 3916u;
        def.price = 800u;
        def.stats.flatAp = 25.f;
        def.displayName = "Oblivion Orb";
        return def;
    }

    ItemDef MakeItem_4005()
    {
        ItemDef def{};
        def.itemId = 4005u;
        def.price = 2400u;
        def.stats.flatAp = 60.f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Imperial Mandate";
        return def;
    }

    ItemDef MakeItem_4401()
    {
        ItemDef def{};
        def.itemId = 4401u;
        def.price = 2800u;
        def.stats.flatHealth = 400.f;
        def.stats.flatMr = 55.f;
        def.stats.percentMoveSpeed = 0.04f;
        def.displayName = "Force of Nature";
        return def;
    }

    ItemDef MakeItem_4628()
    {
        ItemDef def{};
        def.itemId = 4628u;
        def.price = 2700u;
        def.stats.flatAp = 75.f;
        def.stats.abilityHaste = 25.f;
        def.displayName = "Horizon Focus";
        return def;
    }

    ItemDef MakeItem_4629()
    {
        ItemDef def{};
        def.itemId = 4629u;
        def.price = 3000u;
        def.stats.flatAp = 70.f;
        def.stats.flatHealth = 350.f;
        def.stats.abilityHaste = 25.f;
        def.stats.percentMoveSpeed = 0.04f;
        def.displayName = "Cosmic Drive";
        return def;
    }

    ItemDef MakeItem_4630()
    {
        ItemDef def{};
        def.itemId = 4630u;
        def.price = 1100u;
        def.stats.flatAp = 25.f;
        def.stats.magicPenPercent = 0.13f;
        def.displayName = "Blighting Jewel";
        return def;
    }

    ItemDef MakeItem_4632()
    {
        ItemDef def{};
        def.itemId = 4632u;
        def.price = 1600u;
        def.stats.flatAp = 40.f;
        def.stats.flatMr = 25.f;
        def.displayName = "Verdant Barrier";
        return def;
    }

    ItemDef MakeItem_4633()
    {
        ItemDef def{};
        def.itemId = 4633u;
        def.price = 3100u;
        def.stats.flatAp = 70.f;
        def.stats.flatHealth = 350.f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Riftmaker";
        return def;
    }

    ItemDef MakeItem_4642()
    {
        ItemDef def{};
        def.itemId = 4642u;
        def.price = 900u;
        def.stats.flatAp = 20.f;
        def.stats.abilityHaste = 10.f;
        def.displayName = "Bandleglass Mirror";
        return def;
    }

    ItemDef MakeItem_4645()
    {
        ItemDef def{};
        def.itemId = 4645u;
        def.price = 3200u;
        def.stats.flatAp = 110.f;
        def.stats.flatMagicPen = 15.f;
        def.displayName = "Shadowflame";
        return def;
    }

    ItemDef MakeItem_4646()
    {
        ItemDef def{};
        def.itemId = 4646u;
        def.price = 2800u;
        def.stats.flatAp = 90.f;
        def.stats.percentMoveSpeed = 0.06f;
        def.stats.flatMagicPen = 15.f;
        def.displayName = "Stormsurge";
        return def;
    }

    ItemDef MakeItem_6333()
    {
        ItemDef def{};
        def.itemId = 6333u;
        def.price = 3300u;
        def.stats.flatAd = 60.f;
        def.stats.flatArmor = 50.f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Death's Dance";
        return def;
    }

    ItemDef MakeItem_6609()
    {
        ItemDef def{};
        def.itemId = 6609u;
        def.price = 3000u;
        def.stats.flatAd = 45.f;
        def.stats.flatHealth = 450.f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Chempunk Chainsword";
        return def;
    }

    ItemDef MakeItem_6610()
    {
        ItemDef def{};
        def.itemId = 6610u;
        def.price = 3100u;
        def.stats.flatAd = 45.f;
        def.stats.flatHealth = 400.f;
        def.stats.abilityHaste = 10.f;
        def.lightshieldStrike.bValid = true;
        def.lightshieldStrike.cooldownSec = 5.f;
        def.lightshieldStrike.critDamageMultiplier = 1.8f;
        def.lightshieldStrike.healBaseAdRatio = 1.f;
        def.lightshieldStrike.healMissingHealthRatio = 0.06f;
        def.displayName = "Sundered Sky";
        return def;
    }

    ItemDef MakeItem_6616()
    {
        ItemDef def{};
        def.itemId = 6616u;
        def.price = 2250u;
        def.stats.flatAp = 35.f;
        def.stats.abilityHaste = 10.f;
        def.displayName = "Staff of Flowing Water";
        return def;
    }

    ItemDef MakeItem_6617()
    {
        ItemDef def{};
        def.itemId = 6617u;
        def.price = 2200u;
        def.stats.flatAp = 25.f;
        def.stats.flatHealth = 200.f;
        def.stats.abilityHaste = 20.f;
        def.displayName = "Moonstone Renewer";
        return def;
    }

    ItemDef MakeItem_6620()
    {
        ItemDef def{};
        def.itemId = 6620u;
        def.price = 2200u;
        def.stats.flatAp = 35.f;
        def.stats.flatHealth = 200.f;
        def.stats.abilityHaste = 20.f;
        def.displayName = "Echoes of Helia";
        return def;
    }

    ItemDef MakeItem_6621()
    {
        ItemDef def{};
        def.itemId = 6621u;
        def.price = 2500u;
        def.stats.flatAp = 45.f;
        def.displayName = "Dawncore";
        return def;
    }

    ItemDef MakeItem_6631()
    {
        ItemDef def{};
        def.itemId = 6631u;
        def.price = 3300u;
        def.stats.flatAd = 40.f;
        def.stats.flatHealth = 450.f;
        def.stats.bonusAttackSpeed = 0.25f;
        def.displayName = "Stridebreaker";
        return def;
    }

    ItemDef MakeItem_6653()
    {
        ItemDef def{};
        def.itemId = 6653u;
        def.price = 3000u;
        def.stats.flatAp = 60.f;
        def.stats.flatHealth = 300.f;
        def.displayName = "Liandry's Torment";
        return def;
    }

    ItemDef MakeItem_6655()
    {
        ItemDef def{};
        def.itemId = 6655u;
        def.price = 2750u;
        def.stats.flatAp = 100.f;
        def.stats.flatMana = 600.f;
        def.stats.abilityHaste = 10.f;
        def.displayName = "Luden's Echo";
        return def;
    }

    ItemDef MakeItem_6657()
    {
        ItemDef def{};
        def.itemId = 6657u;
        def.price = 2600u;
        def.stats.flatAp = 45.f;
        def.stats.flatHealth = 350.f;
        def.stats.flatMana = 500.f;
        def.displayName = "Rod of Ages";
        return def;
    }

    ItemDef MakeItem_6660()
    {
        ItemDef def{};
        def.itemId = 6660u;
        def.price = 900u;
        def.stats.flatHealth = 150.f;
        def.stats.abilityHaste = 5.f;
        def.displayName = "Bami's Cinder";
        return def;
    }

    ItemDef MakeItem_6662()
    {
        ItemDef def{};
        def.itemId = 6662u;
        def.price = 2900u;
        def.stats.flatHealth = 300.f;
        def.stats.flatArmor = 50.f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Iceborn Gauntlet";
        return def;
    }

    ItemDef MakeItem_6664()
    {
        ItemDef def{};
        def.itemId = 6664u;
        def.price = 2800u;
        def.stats.flatHealth = 400.f;
        def.stats.flatMr = 40.f;
        def.stats.abilityHaste = 10.f;
        def.displayName = "Hollow Radiance";
        return def;
    }

    ItemDef MakeItem_6665()
    {
        ItemDef def{};
        def.itemId = 6665u;
        def.price = 3200u;
        def.stats.flatHealth = 350.f;
        def.stats.flatArmor = 45.f;
        def.stats.flatMr = 45.f;
        def.displayName = "Jak'Sho, The Protean";
        return def;
    }

    ItemDef MakeItem_6670()
    {
        ItemDef def{};
        def.itemId = 6670u;
        def.price = 1300u;
        def.stats.flatAd = 15.f;
        def.stats.critChance = 0.2f;
        def.displayName = "Noonquiver";
        return def;
    }

    ItemDef MakeItem_6672()
    {
        ItemDef def{};
        def.itemId = 6672u;
        def.price = 3000u;
        def.stats.flatAd = 45.f;
        def.stats.bonusAttackSpeed = 0.4f;
        def.stats.percentMoveSpeed = 0.04f;
        def.displayName = "Kraken Slayer";
        return def;
    }

    ItemDef MakeItem_6673()
    {
        ItemDef def{};
        def.itemId = 6673u;
        def.price = 3000u;
        def.stats.flatAd = 55.f;
        def.stats.critChance = 0.25f;
        def.displayName = "Immortal Shieldbow";
        return def;
    }

    ItemDef MakeItem_6675()
    {
        ItemDef def{};
        def.itemId = 6675u;
        def.price = 2650u;
        def.stats.bonusAttackSpeed = 0.4f;
        def.stats.critChance = 0.25f;
        def.stats.percentMoveSpeed = 0.04f;
        def.displayName = "Navori Flickerblade";
        return def;
    }

    ItemDef MakeItem_6676()
    {
        ItemDef def{};
        def.itemId = 6676u;
        def.price = 3000u;
        def.stats.flatAd = 50.f;
        def.stats.critChance = 0.25f;
        def.stats.lethality = 10.f;
        def.displayName = "The Collector";
        return def;
    }

    ItemDef MakeItem_6690()
    {
        ItemDef def{};
        def.itemId = 6690u;
        def.price = 775u;
        def.stats.flatAd = 15.f;
        def.stats.percentMoveSpeed = 0.04f;
        def.displayName = "Rectrix";
        return def;
    }

    ItemDef MakeItem_6692()
    {
        ItemDef def{};
        def.itemId = 6692u;
        def.price = 2900u;
        def.stats.flatAd = 60.f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Eclipse";
        return def;
    }

    ItemDef MakeItem_6694()
    {
        ItemDef def{};
        def.itemId = 6694u;
        def.price = 3000u;
        def.stats.flatAd = 45.f;
        def.stats.abilityHaste = 15.f;
        def.stats.armorPenPercent = 0.35f;
        def.displayName = "Serylda's Grudge";
        return def;
    }

    ItemDef MakeItem_6695()
    {
        ItemDef def{};
        def.itemId = 6695u;
        def.price = 2500u;
        def.stats.flatAd = 55.f;
        def.stats.lethality = 15.f;
        def.displayName = "Serpent's Fang";
        return def;
    }

    ItemDef MakeItem_6696()
    {
        ItemDef def{};
        def.itemId = 6696u;
        def.price = 2750u;
        def.stats.flatAd = 55.f;
        def.stats.abilityHaste = 20.f;
        def.stats.lethality = 18.f;
        def.displayName = "Axiom Arc";
        return def;
    }

    ItemDef MakeItem_6697()
    {
        ItemDef def{};
        def.itemId = 6697u;
        def.price = 2800u;
        def.stats.flatAd = 55.f;
        def.stats.abilityHaste = 10.f;
        def.stats.lethality = 18.f;
        def.displayName = "Hubris";
        return def;
    }

    ItemDef MakeItem_6698()
    {
        ItemDef def{};
        def.itemId = 6698u;
        def.price = 2850u;
        def.stats.flatAd = 55.f;
        def.stats.abilityHaste = 10.f;
        def.stats.lethality = 18.f;
        def.displayName = "Profane Hydra";
        return def;
    }

    ItemDef MakeItem_6699()
    {
        ItemDef def{};
        def.itemId = 6699u;
        def.price = 3000u;
        def.stats.flatAd = 55.f;
        def.stats.abilityHaste = 10.f;
        def.stats.lethality = 10.f;
        def.displayName = "Voltaic Cyclosword";
        return def;
    }

    ItemDef MakeItem_8010()
    {
        ItemDef def{};
        def.itemId = 8010u;
        def.price = 2900u;
        def.stats.flatAp = 65.f;
        def.stats.flatHealth = 400.f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Bloodletter's Curse";
        return def;
    }

    ItemDef MakeItem_8020()
    {
        ItemDef def{};
        def.itemId = 8020u;
        def.price = 2650u;
        def.stats.flatHealth = 350.f;
        def.stats.flatMr = 45.f;
        def.stats.abilityHaste = 15.f;
        def.displayName = "Abyssal Mask";
        return def;
    }

    const ItemDef kItemDefs[] =
    {
        MakeItem_1001(),
        MakeItem_1004(),
        MakeItem_1006(),
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
        MakeItem_1082(),
        MakeItem_1083(),
        MakeItem_1086(),
        MakeItem_1101(),
        MakeItem_1102(),
        MakeItem_1103(),
        MakeItem_1105(),
        MakeItem_1106(),
        MakeItem_1107(),
        MakeItem_1120(),
        MakeItem_2003(),
        MakeItem_2019(),
        MakeItem_2020(),
        MakeItem_2021(),
        MakeItem_2022(),
        MakeItem_2031(),
        MakeItem_2051(),
        MakeItem_2055(),
        MakeItem_2065(),
        MakeItem_2138(),
        MakeItem_2139(),
        MakeItem_2140(),
        MakeItem_2141(),
        MakeItem_2420(),
        MakeItem_2501(),
        MakeItem_2502(),
        MakeItem_2503(),
        MakeItem_2504(),
        MakeItem_2508(),
        MakeItem_2510(),
        MakeItem_2512(),
        MakeItem_2517(),
        MakeItem_2520(),
        MakeItem_2522(),
        MakeItem_2523(),
        MakeItem_2524(),
        MakeItem_2525(),
        MakeItem_2526(),
        MakeItem_3003(),
        MakeItem_3004(),
        MakeItem_3006(),
        MakeItem_3008(),
        MakeItem_3009(),
        MakeItem_3020(),
        MakeItem_3024(),
        MakeItem_3026(),
        MakeItem_3031(),
        MakeItem_3032(),
        MakeItem_3033(),
        MakeItem_3035(),
        MakeItem_3036(),
        MakeItem_3041(),
        MakeItem_3042(),
        MakeItem_3044(),
        MakeItem_3046(),
        MakeItem_3047(),
        MakeItem_3050(),
        MakeItem_3051(),
        MakeItem_3053(),
        MakeItem_3057(),
        MakeItem_3065(),
        MakeItem_3066(),
        MakeItem_3067(),
        MakeItem_3068(),
        MakeItem_3070(),
        MakeItem_3071(),
        MakeItem_3072(),
        MakeItem_3073(),
        MakeItem_3074(),
        MakeItem_3075(),
        MakeItem_3076(),
        MakeItem_3077(),
        MakeItem_3078(),
        MakeItem_3082(),
        MakeItem_3083(),
        MakeItem_3084(),
        MakeItem_3085(),
        MakeItem_3086(),
        MakeItem_3087(),
        MakeItem_3089(),
        MakeItem_3091(),
        MakeItem_3094(),
        MakeItem_3097(),
        MakeItem_3100(),
        MakeItem_3102(),
        MakeItem_3107(),
        MakeItem_3108(),
        MakeItem_3109(),
        MakeItem_3110(),
        MakeItem_3111(),
        MakeItem_3112(),
        MakeItem_3113(),
        MakeItem_3114(),
        MakeItem_3115(),
        MakeItem_3116(),
        MakeItem_3118(),
        MakeItem_3119(),
        MakeItem_3123(),
        MakeItem_3124(),
        MakeItem_3133(),
        MakeItem_3134(),
        MakeItem_3135(),
        MakeItem_3137(),
        MakeItem_3139(),
        MakeItem_3140(),
        MakeItem_3142(),
        MakeItem_3143(),
        MakeItem_3144(),
        MakeItem_3145(),
        MakeItem_3146(),
        MakeItem_3147(),
        MakeItem_3152(),
        MakeItem_3153(),
        MakeItem_3155(),
        MakeItem_3156(),
        MakeItem_3157(),
        MakeItem_3158(),
        MakeItem_3161(),
        MakeItem_3165(),
        MakeItem_3168(),
        MakeItem_3170(),
        MakeItem_3171(),
        MakeItem_3172(),
        MakeItem_3173(),
        MakeItem_3174(),
        MakeItem_3175(),
        MakeItem_3177(),
        MakeItem_3179(),
        MakeItem_3181(),
        MakeItem_3184(),
        MakeItem_3190(),
        MakeItem_3211(),
        MakeItem_3222(),
        MakeItem_3302(),
        MakeItem_3340(),
        MakeItem_3504(),
        MakeItem_3508(),
        MakeItem_3599(),
        MakeItem_3742(),
        MakeItem_3748(),
        MakeItem_3801(),
        MakeItem_3802(),
        MakeItem_3803(),
        MakeItem_3814(),
        MakeItem_3865(),
        MakeItem_3869(),
        MakeItem_3870(),
        MakeItem_3871(),
        MakeItem_3876(),
        MakeItem_3877(),
        MakeItem_3916(),
        MakeItem_4005(),
        MakeItem_4401(),
        MakeItem_4628(),
        MakeItem_4629(),
        MakeItem_4630(),
        MakeItem_4632(),
        MakeItem_4633(),
        MakeItem_4642(),
        MakeItem_4645(),
        MakeItem_4646(),
        MakeItem_6333(),
        MakeItem_6609(),
        MakeItem_6610(),
        MakeItem_6616(),
        MakeItem_6617(),
        MakeItem_6620(),
        MakeItem_6621(),
        MakeItem_6631(),
        MakeItem_6653(),
        MakeItem_6655(),
        MakeItem_6657(),
        MakeItem_6660(),
        MakeItem_6662(),
        MakeItem_6664(),
        MakeItem_6665(),
        MakeItem_6670(),
        MakeItem_6672(),
        MakeItem_6673(),
        MakeItem_6675(),
        MakeItem_6676(),
        MakeItem_6690(),
        MakeItem_6692(),
        MakeItem_6694(),
        MakeItem_6695(),
        MakeItem_6696(),
        MakeItem_6697(),
        MakeItem_6698(),
        MakeItem_6699(),
        MakeItem_8010(),
        MakeItem_8020(),
    };

    RuneGameplayDef MakeRune_1()
    {
        RuneGameplayDef def{};
        def.key = 0x3557DC5Bu;
        def.legacyRuneId = static_cast<eRuneId>(1u);
        def.bEnabled = true;
        def.maxStacks = 5u;
        return def;
    }

    RuneGameplayDef MakeRune_2()
    {
        RuneGameplayDef def{};
        def.key = 0xE35E1EFCu;
        def.legacyRuneId = static_cast<eRuneId>(2u);
        def.bEnabled = false;
        def.maxStacks = 0u;
        return def;
    }

    RuneGameplayDef MakeRune_3()
    {
        RuneGameplayDef def{};
        def.key = 0x4B7BFCB3u;
        def.legacyRuneId = static_cast<eRuneId>(3u);
        def.bEnabled = false;
        def.maxStacks = 0u;
        return def;
    }

    const RuneGameplayDef kRuneDefs[] =
    {
        MakeRune_1(),
        MakeRune_2(),
        MakeRune_3(),
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
            { 2u, 1u, kBuildHash, 0u, eDataPackVisibility::ServerPrivate },
            kChampions,
            sizeof(kChampions) / sizeof(kChampions[0]),
            kSkills,
            sizeof(kSkills) / sizeof(kSkills[0]),
            kSummonerSpells,
            sizeof(kSummonerSpells) / sizeof(kSummonerSpells[0]),
            &economyDef,
            kItemDefs,
            sizeof(kItemDefs) / sizeof(kItemDefs[0]),
            kRuneDefs,
            sizeof(kRuneDefs) / sizeof(kRuneDefs[0]),
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
            MakeMinionBehaviorDef(),
            MakeMinionWaveDef(),
        };
        return pack;
    }
}
