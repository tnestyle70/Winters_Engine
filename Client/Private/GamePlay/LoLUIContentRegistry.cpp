#include "GamePlay/LoLUIContentRegistry.h"

#include "Client/Private/Data/LoLVisualDefinitionPack.h"
#include "GameInstance.h"
#include "Manager/UI/ActorHUDAssets.h"
#include "Shared/GameSim/Components/ChampionScore.h"
#include "Shared/GameSim/Components/KalistaBondComponent.h"
#include "Shared/GameSim/Definitions/ItemDef.h"
#include "Shared/GameSim/Definitions/LoLMatchContext.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <iterator>
#include <limits>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Client
{
    namespace
    {
        constexpr u8_t Content(eChampion eChampionId)
        {
            return static_cast<u8_t>(eChampionId);
        }

        void AppendItemStatLine(std::vector<std::string>& Lines, const char* pLabel, f32_t Value, bool_t bPercent = false)
        {
            if (Value <= 0.f)
                return;

            char Buffer[64]{};
            if (bPercent)
                std::snprintf(Buffer, sizeof(Buffer), "+%.0f%% %s", Value * 100.f, pLabel);
            else
                std::snprintf(Buffer, sizeof(Buffer), "+%.0f %s", Value, pLabel);
            Lines.emplace_back(Buffer);
        }

        std::vector<std::string> BuildItemStatLines(const ItemStatModifier& Stats)
        {
            std::vector<std::string> Lines;
            AppendItemStatLine(Lines, "Attack Damage", Stats.flatAd);
            AppendItemStatLine(Lines, "Ability Power", Stats.flatAp);
            AppendItemStatLine(Lines, "Health", Stats.flatHealth);
            AppendItemStatLine(Lines, "Mana", Stats.flatMana);
            AppendItemStatLine(Lines, "Armor", Stats.flatArmor);
            AppendItemStatLine(Lines, "Magic Resist", Stats.flatMr);
            AppendItemStatLine(Lines, "Attack Speed", Stats.bonusAttackSpeed, true);
            AppendItemStatLine(Lines, "Crit Chance", Stats.critChance, true);
            AppendItemStatLine(Lines, "Crit Damage", Stats.critDamageBonus, true);
            AppendItemStatLine(Lines, "Ability Haste", Stats.abilityHaste);
            AppendItemStatLine(Lines, "Move Speed", Stats.percentMoveSpeed, true);
            AppendItemStatLine(Lines, "Armor Pen", Stats.armorPenPercent, true);
            AppendItemStatLine(Lines, "Bonus Armor Pen", Stats.bonusArmorPenPercent, true);
            AppendItemStatLine(Lines, "Magic Pen", Stats.magicPenPercent, true);
            AppendItemStatLine(Lines, "Magic Pen", Stats.flatMagicPen);
            AppendItemStatLine(Lines, "Lethality", Stats.lethality);
            AppendItemStatLine(Lines, "Move Speed", Stats.flatMoveSpeed);
            AppendItemStatLine(Lines, "Life Steal", Stats.lifeSteal, true);

            if (Lines.empty())
                Lines.emplace_back("No direct stats");
            return Lines;
        }

        constexpr Engine::ActorHUDAssetDesc kLoLActorHudAssets[] =
        {
            {
                Content(eChampion::IRELIA),
                L"Resource/Texture/UI/Champion/Portraits/irelia_circle_0.png",
                L"Resource/Texture/UI/Champion/Icon2d/Irelia_Icon2d/irelia_passive.png",
                {
                    L"Resource/Texture/UI/Champion/Icon2d/Irelia_Icon2d/irelia_q.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Irelia_Icon2d/irelia_w.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Irelia_Icon2d/irelia_e.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Irelia_Icon2d/irelia_r.png",
                },
            },
            {
                Content(eChampion::YASUO),
                L"Resource/Texture/UI/Champion/Portraits/yasuo_circle.png",
                L"Resource/Texture/UI/Champion/Icon2d/Yasuo_Icon2d/yasuo_passive.png",
                {
                    L"Resource/Texture/UI/Champion/Icon2d/Yasuo_Icon2d/yasuo_q1.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Yasuo_Icon2d/yasuo_w.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Yasuo_Icon2d/yasuo_e.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Yasuo_Icon2d/yasuo_r.png",
                },
                L"Resource/Texture/UI/Champion_Yasuo_PassiveBar.png",
                true,
            },
            {
                Content(eChampion::KALISTA),
                L"Resource/Texture/UI/Champion/Portraits/kalista_circle_0.png",
                L"Resource/Texture/UI/Champion/Icon2d/Kalista_Icon2d/kalista_passive.png",
                {
                    L"Resource/Texture/UI/Champion/Icon2d/Kalista_Icon2d/kalista_q.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Kalista_Icon2d/kalista_w.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Kalista_Icon2d/kalista_e.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Kalista_Icon2d/kalista_r.png",
                },
            },
            {
                Content(eChampion::SYLAS),
                L"Resource/Texture/UI/Champion/Portraits/sylas_circle_0.png",
                L"Resource/Texture/UI/Champion/Icon2d/Sylas_Icon2d/sylasp.png",
                {
                    L"Resource/Texture/UI/Champion/Icon2d/Sylas_Icon2d/sylasq.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Sylas_Icon2d/sylasw.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Sylas_Icon2d/sylase.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Sylas_Icon2d/sylasr.png",
                },
            },
            {
                Content(eChampion::VIEGO),
                L"Resource/Texture/UI/Champion/Portraits/viego_circle.png",
                L"Resource/Texture/UI/Champion/Icon2d/Viego_Icon2d/viego_passive.png",
                {
                    L"Resource/Texture/UI/Champion/Icon2d/Viego_Icon2d/viego_q.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Viego_Icon2d/viego_w.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Viego_Icon2d/viego_e.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Viego_Icon2d/viego_r.png",
                },
            },
            {
                Content(eChampion::ANNIE),
                L"Resource/Texture/UI/Champion/Portraits/annie_circle.png",
                L"Resource/Texture/UI/Champion/Icon2d/Annie_Icon2d/annie_passive.png",
                {
                    L"Resource/Texture/UI/Champion/Icon2d/Annie_Icon2d/annie_q.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Annie_Icon2d/annie_w.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Annie_Icon2d/annie_e.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Annie_Icon2d/annie_r1.png",
                },
            },
            {
                Content(eChampion::ASHE),
                L"Resource/Texture/UI/Champion/Portraits/ashe_circle.png",
                L"Resource/Texture/UI/Champion/Icon2d/Ashe_Icon2d/ashe_p.png",
                {
                    L"Resource/Texture/UI/Champion/Icon2d/Ashe_Icon2d/ashe_q.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Ashe_Icon2d/ashe_w.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Ashe_Icon2d/ashe_e.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Ashe_Icon2d/ashe_r.png",
                },
            },
            {
                Content(eChampion::FIORA),
                L"Resource/Texture/UI/Champion/Portraits/fiora_circle.png",
                L"Resource/Texture/UI/Champion/Icon2d/Fiora_Icon2d/fiora_p.png",
                {
                    L"Resource/Texture/UI/Champion/Icon2d/Fiora_Icon2d/fiora_q.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Fiora_Icon2d/fiora_w.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Fiora_Icon2d/fiora_e.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Fiora_Icon2d/fiora_r.png",
                },
            },
            {
                Content(eChampion::GAREN),
                L"Resource/Texture/UI/Champion/Portraits/garen_circle.png",
                L"Resource/Texture/UI/Champion/Icon2d/Garen_Icon2d/garen_passive.png",
                {
                    L"Resource/Texture/UI/Champion/Icon2d/Garen_Icon2d/garen_q.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Garen_Icon2d/garen_w.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Garen_Icon2d/garen_e1.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Garen_Icon2d/garen_r.png",
                },
            },
            {
                Content(eChampion::RIVEN),
                L"Resource/Texture/UI/Champion/Portraits/riven_circle.png",
                L"Resource/Texture/UI/Champion/Icon2d/Riven_Icon2d/rivenrunicblades.png",
                {
                    L"Resource/Texture/UI/Champion/Icon2d/Riven_Icon2d/rivenbrokenwings.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Riven_Icon2d/rivenkishout.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Riven_Icon2d/rivenpathoftheexile.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Riven_Icon2d/rivenbladeoftheexile.png",
                },
            },
            {
                Content(eChampion::ZED),
                L"Resource/Texture/UI/Champion/Portraits/zed_circle_0.png",
                L"Resource/Texture/UI/Champion/Icon2d/Zed_Icon2d/zedp.png",
                {
                    L"Resource/Texture/UI/Champion/Icon2d/Zed_Icon2d/zedq.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Zed_Icon2d/zedw.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Zed_Icon2d/zede.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Zed_Icon2d/zedr.png",
                },
            },
            {
                Content(eChampion::EZREAL),
                L"Resource/Texture/UI/Champion/Portraits/ezreal_circle.png",
                L"Resource/Texture/UI/Champion/Icon2d/Ezreal_Icon2d/ezreal_passive.png",
                {
                    L"Resource/Texture/UI/Champion/Icon2d/Ezreal_Icon2d/ezreal_q.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Ezreal_Icon2d/ezreal_w.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Ezreal_Icon2d/ezreal_e.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Ezreal_Icon2d/ezreal_r.png",
                },
            },
            {
                Content(eChampion::YONE),
                L"Resource/Texture/UI/Champion/Portraits/yone_circle_0.png",
                L"Resource/Texture/UI/Champion/Icon2d/Yone_Icon2d/yonepassive.png",
                {
                    L"Resource/Texture/UI/Champion/Icon2d/Yone_Icon2d/yoneq.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Yone_Icon2d/yonew.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Yone_Icon2d/yonee.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Yone_Icon2d/yoner.png",
                },
            },
            {
                Content(eChampion::JAX),
                L"Resource/Texture/UI/Champion/Portraits/jax_circle_0.png",
                L"Resource/Texture/UI/Champion/Icon2d/Jax_Icon2D/jaxp.png",
                {
                    L"Resource/Texture/UI/Champion/Icon2d/Jax_Icon2D/jaxq.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Jax_Icon2D/jaxw.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Jax_Icon2D/jaxe.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Jax_Icon2D/jaxr.png",
                },
            },
            {
                Content(eChampion::MASTERYI),
                L"Resource/Texture/UI/Champion/Portraits/masteryi_circle_0.png",
                L"Resource/Texture/UI/Champion/Icon2d/MasterYi_Icon2d/masteryi_passive1.png",
                {
                    L"Resource/Texture/UI/Champion/Icon2d/MasterYi_Icon2d/masteryi_q.png",
                    L"Resource/Texture/UI/Champion/Icon2d/MasterYi_Icon2d/masteryi_w.png",
                    L"Resource/Texture/UI/Champion/Icon2d/MasterYi_Icon2d/masteryi_e1.png",
                    L"Resource/Texture/UI/Champion/Icon2d/MasterYi_Icon2d/masteryi_r.png",
                },
            },
            {
                Content(eChampion::KINDRED),
                L"Resource/Texture/UI/Champion/Portraits/kindred_circle.png",
                L"Resource/Texture/UI/Champion/Icon2d/Kindred_Icon2d/kindred_passive.png",
                {
                    L"Resource/Texture/UI/Champion/Icon2d/Kindred_Icon2d/kindred_q.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Kindred_Icon2d/kindred_w.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Kindred_Icon2d/kindred_e.png",
                    L"Resource/Texture/UI/Champion/Icon2d/Kindred_Icon2d/kindred_r.png",
                },
            },
            {
                Content(eChampion::LEESIN),
                L"Resource/Texture/UI/Champion/Portraits/leesin_circle_0.png",
                L"Resource/Texture/UI/Champion/Icon2d/LeeSin_Icon2d/leesinpassive.png",
                {
                    L"Resource/Texture/UI/Champion/Icon2d/LeeSin_Icon2d/leesinq1.png",
                    L"Resource/Texture/UI/Champion/Icon2d/LeeSin_Icon2d/leesinw1.png",
                    L"Resource/Texture/UI/Champion/Icon2d/LeeSin_Icon2d/leesine1.png",
                    L"Resource/Texture/UI/Champion/Icon2d/LeeSin_Icon2d/leesinr.png",
                },
            },
        };

        constexpr Engine::UIIconAssetDesc kLoLStatusPanelSpellIcons[] =
        {
            {
                ChampionScoreComponent::kSummonerSpellFlash,
                L"Resource/Texture/UI/DELETE/Materials/HotBar/hotbar_activated_flash.png",
            },
            {
                ChampionScoreComponent::kSummonerSpellIgnite,
                L"Resource/Texture/UI/Champion/Icon2d/Garen_Icon2d/summonerignite.png",
            },
        };

        constexpr u16_t kLoLDefaultStatusPanelSpellIds[] =
        {
            ChampionScoreComponent::kSummonerSpellFlash,
            ChampionScoreComponent::kSummonerSpellIgnite,
        };

        struct LoLShopCatalogEntry
        {
            u16_t iItemId = 0u;
            const char* pAssetKey = nullptr;
            const char* pSection = nullptr;
            const wchar_t* pIconPath = nullptr;
            const char* pIconSprite = nullptr;
            bool_t bPurchasable = true;
        };

        // Preferred legacy icon ordering. Canonical availability, price, and
        // stats come from the generated Data Dragon 16.14.1 item projection.
        constexpr LoLShopCatalogEntry kLoLShopCatalog[] =
        {
            { 1055, "1055_marksman_t1_doransblade.png", "legacy", L"Resource/Texture/UI/Items/1055_marksman_t1_doransblade.png", "item:1055_marksman_t1_doransblade.png" },
            { 1056, "1056_mage_t1_doransring.png", "legacy", L"Resource/Texture/UI/Items/1056_mage_t1_doransring.png", "item:1056_mage_t1_doransring.png" },
            { 1054, "1054_tank_t1_doransshield.png", "legacy", L"Resource/Texture/UI/Items/1054_tank_t1_doransshield.png", "item:1054_tank_t1_doransshield.png" },
            { 1001, "1001_class_t1_bootsofspeed.png", "legacy", L"Resource/Texture/UI/Items/1001_class_t1_bootsofspeed.png", "item:1001_class_t1_bootsofspeed.png" },
            { 3006, "3006_class_t2_berserkersgreaves.png", "legacy", L"Resource/Texture/UI/Items/3006_class_t2_berserkersgreaves.png", "item:3006_class_t2_berserkersgreaves.png" },
            { 3020, "3020_class_t2_sorcerersshoes.png", "legacy", L"Resource/Texture/UI/Items/3020_class_t2_sorcerersshoes.png", "item:3020_class_t2_sorcerersshoes.png" },
            { 3047, "3047_class_t2_ninjatabi.png", "legacy", L"Resource/Texture/UI/Items/3047_class_t2_ninjatabi.png", "item:3047_class_t2_ninjatabi.png" },
            { 3111, "3111_class_t2_mercurystreads.png", "legacy", L"Resource/Texture/UI/Items/3111_class_t2_mercurystreads.png", "item:3111_class_t2_mercurystreads.png" },
            { 3158, "3158_class_t2_ionianbootsoflucidity.png", "legacy", L"Resource/Texture/UI/Items/3158_class_t2_ionianbootsoflucidity.png", "item:3158_class_t2_ionianbootsoflucidity.png" },
            { 1036, "1036_class_t1_longsword.png", "legacy", L"Resource/Texture/UI/Items/1036_class_t1_longsword.png", "item:1036_class_t1_longsword.png" },
            { 1042, "1042_base_t1_dagger.png", "legacy", L"Resource/Texture/UI/Items/1042_base_t1_dagger.png", "item:1042_base_t1_dagger.png" },
            { 1043, "1043_base_t2_recurvebow.png", "legacy", L"Resource/Texture/UI/Items/1043_base_t2_recurvebow.png", "item:1043_base_t2_recurvebow.png" },
            { 1052, "1052_mage_t2_amptome.png", "legacy", L"Resource/Texture/UI/Items/1052_mage_t2_amptome.png", "item:1052_mage_t2_amptome.png" },
            { 1053, "1053_fighter_t2_vampiricscepter.png", "legacy", L"Resource/Texture/UI/Items/1053_fighter_t2_vampiricscepter.png", "item:1053_fighter_t2_vampiricscepter.png" },
            { 1028, "1028_base_t1_rubycrystal.png", "legacy", L"Resource/Texture/UI/Items/1028_base_t1_rubycrystal.png", "item:1028_base_t1_rubycrystal.png" },
            { 1029, "1029_base_t1_clotharmor.png", "legacy", L"Resource/Texture/UI/Items/1029_base_t1_clotharmor.png", "item:1029_base_t1_clotharmor.png" },
            { 1031, "1031_base_t2_chainvest.png", "legacy", L"Resource/Texture/UI/Items/1031_base_t2_chainvest.png", "item:1031_base_t2_chainvest.png" },
            { 1033, "1033_base_t1_magicmantle.png", "legacy", L"Resource/Texture/UI/Items/1033_base_t1_magicmantle.png", "item:1033_base_t1_magicmantle.png" },
            { 1057, "1057_tank_t2_negatroncloak.png", "legacy", L"Resource/Texture/UI/Items/1057_tank_t2_negatroncloak.png", "item:1057_tank_t2_negatroncloak.png" },
            { 1011, "1011_class_t2_giantsbelt.png", "legacy", L"Resource/Texture/UI/Items/1011_class_t2_giantsbelt.png", "item:1011_class_t2_giantsbelt.png" },
            { 1018, "1018_base_t1_cloakagility.png", "legacy", L"Resource/Texture/UI/Items/1018_base_t1_cloakagility.png", "item:1018_base_t1_cloakagility.png" },
            { 1026, "1026_mage_t1_blastingwand.png", "legacy", L"Resource/Texture/UI/Items/1026_mage_t1_blastingwand.png", "item:1026_mage_t1_blastingwand.png" },
            { 1027, "1027_base_t1_saphirecrystal.png", "legacy", L"Resource/Texture/UI/Items/1027_base_t1_saphirecrystal.png", "item:1027_base_t1_saphirecrystal.png" },
            { 1037, "1037_class_t1_pickaxe.png", "legacy", L"Resource/Texture/UI/Items/1037_class_t1_pickaxe.png", "item:1037_class_t1_pickaxe.png" },
            { 1058, "1058_mage_t1_largerod.png", "legacy", L"Resource/Texture/UI/Items/1058_mage_t1_largerod.png", "item:1058_mage_t1_largerod.png" },
            { 1038, "1038_marksman_t1_bfsword.png", "legacy", L"Resource/Texture/UI/Items/1038_marksman_t1_bfsword.png", "item:1038_marksman_t1_bfsword.png" },
            { 3031, "3031_marksman_t3_infinityedge.png", "legacy", L"Resource/Texture/UI/Items/3031_marksman_t3_infinityedge.png", "item:3031_marksman_t3_infinityedge.png" },
            { 3072, "3072_fighter_t3_bloodthirster.png", "legacy", L"Resource/Texture/UI/Items/3072_fighter_t3_bloodthirster.png", "item:3072_fighter_t3_bloodthirster.png" },
            { 3078, "3078_fighter_t4_trinityforce.png", "legacy", L"Resource/Texture/UI/Items/3078_fighter_t4_trinityforce.png", "item:3078_fighter_t4_trinityforce.png" },
            { 3153, "3153_fighter_t3_bladeoftheruinedking.png", "legacy", L"Resource/Texture/UI/Items/3153_fighter_t3_bladeoftheruinedking.png", "item:3153_fighter_t3_bladeoftheruinedking.png" },
            { 3089, "3089_mage_t3_deathcap.png", "legacy", L"Resource/Texture/UI/Items/3089_mage_t3_deathcap.png", "item:3089_mage_t3_deathcap.png" },
            { 3157, "3157_mage_t3_zhonyashourglass.png", "legacy", L"Resource/Texture/UI/Items/3157_mage_t3_zhonyashourglass.png", "item:3157_mage_t3_zhonyashourglass.png" },
            { 3065, "3065_tank_t3_spiritvisage.png", "legacy", L"Resource/Texture/UI/Items/3065_tank_t3_spiritvisage.png", "item:3065_tank_t3_spiritvisage.png" },
            { 3742, "3742_tank_t3_deadmansplate.png", "legacy", L"Resource/Texture/UI/Items/3742_tank_t3_deadmansplate.png", "item:3742_tank_t3_deadmansplate.png" },
            { 3042, "3042_marksman_t3_muramana.png", "inventory", L"Resource/Texture/UI/Items/3042_marksman_t3_muramana.png", "item:3042_marksman_t3_muramana.png", false },
            { 3340, "3340_class_t1_wardingtotem.png", "inventory", L"Resource/Texture/UI/Items/3340_class_t1_wardingtotem.png", "item:3340_class_t1_wardingtotem.png", false },
            { kKalistaOathswornItemId, "3599_kalistapassiveitem.png", "inventory", L"Resource/Texture/UI/Items/3599_kalistapassiveitem.png", "item:3599_kalistapassiveitem.png", false },
        };

        struct RuntimeShopEntry
        {
            u16_t iItemId = 0u;
            std::string strAssetKey;
            std::string strSection;
            std::wstring strIconPath;
            std::string strIconSprite;
            bool_t bEnabled = true;
            bool_t bPurchasable = true;
        };

        u16_t ParseItemId(const std::string& AssetKey)
        {
            u32_t value = 0u;
            bool_t bHasDigit = false;
            for (char character : AssetKey)
            {
                if (character < '0' || character > '9')
                    break;
                bHasDigit = true;
                value = value * 10u + static_cast<u32_t>(character - '0');
                if (value > (std::numeric_limits<u16_t>::max)())
                    return 0u;
            }
            return bHasDigit ? static_cast<u16_t>(value) : 0u;
        }

        std::string ResolveShopSection(const std::string& AssetKey)
        {
            if (AssetKey.find("doran") != std::string::npos ||
                AssetKey.find("support") != std::string::npos)
                return "starter";
            if (AssetKey.find("_t1_") != std::string::npos)
                return "basic";
            if (AssetKey.find("_t2_") != std::string::npos)
                return "epic";
            if (AssetKey.find("_t3_") != std::string::npos ||
                AssetKey.find("_t4_") != std::string::npos)
                return "legendary";
            return "resource";
        }

        RuntimeShopEntry MakeRuntimeShopEntry(
            u16_t ItemId,
            const std::string& AssetKey,
            bool_t bEnabled,
            bool_t bPurchasable = true)
        {
            RuntimeShopEntry entry{};
            entry.iItemId = ItemId;
            entry.strAssetKey = AssetKey;
            entry.strSection = ResolveShopSection(AssetKey);
            entry.strIconPath =
                L"Resource/Texture/UI/Items/" +
                std::wstring(AssetKey.begin(), AssetKey.end());
            entry.strIconSprite = "item:" + AssetKey;
            entry.bEnabled = bEnabled;
            entry.bPurchasable = bPurchasable;
            return entry;
        }

        std::vector<RuntimeShopEntry> BuildRuntimeShopCatalog()
        {
            std::vector<RuntimeShopEntry> catalog;
            std::unordered_set<std::string> assetKeys;
            std::unordered_set<u16_t> itemIds;
            catalog.reserve(std::size(kLoLShopCatalog) + 384u);

            for (const LoLShopCatalogEntry& entry : kLoLShopCatalog)
            {
                if (entry.bPurchasable &&
                    !ClientData::FindShopItemPresentationDefinition(entry.iItemId))
                    continue;
                catalog.push_back(MakeRuntimeShopEntry(
                    entry.iItemId,
                    entry.pAssetKey ? entry.pAssetKey : "",
                    true,
                    entry.bPurchasable));
                assetKeys.insert(catalog.back().strAssetKey);
                itemIds.insert(entry.iItemId);
            }

            const std::filesystem::path candidates[] =
            {
                std::filesystem::path(L"Resource/Texture/UI/Items"),
                std::filesystem::path(L"Client/Bin/Resource/Texture/UI/Items"),
            };
            std::filesystem::path itemDirectory;
            for (const std::filesystem::path& candidate : candidates)
            {
                std::error_code existsError;
                if (std::filesystem::is_directory(candidate, existsError))
                {
                    itemDirectory = candidate;
                    break;
                }
            }

            std::vector<RuntimeShopEntry> discovered;
            std::error_code iterateError;
            for (std::filesystem::directory_iterator it(itemDirectory, iterateError), end;
                 !iterateError && it != end;
                 it.increment(iterateError))
            {
                std::error_code typeError;
                if (!it->is_regular_file(typeError))
                    continue;

                std::string extension = it->path().extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(),
                    [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
                if (extension != ".png")
                    continue;

                const std::string assetKey = it->path().filename().string();
                if (assetKey.empty() || !assetKeys.insert(assetKey).second)
                    continue;

                const u16_t itemId = ParseItemId(assetKey);
                if (itemId == 0u ||
                    itemIds.find(itemId) != itemIds.end() ||
                    !ClientData::FindShopItemPresentationDefinition(itemId))
                {
                    continue;
                }
                itemIds.insert(itemId);
                discovered.push_back(MakeRuntimeShopEntry(
                    itemId, assetKey, true));
            }

            std::sort(discovered.begin(), discovered.end(),
                [](const RuntimeShopEntry& left, const RuntimeShopEntry& right)
                {
                    if (left.strSection != right.strSection)
                        return left.strSection < right.strSection;
                    if (left.iItemId != right.iItemId)
                        return left.iItemId < right.iItemId;
                    return left.strAssetKey < right.strAssetKey;
                });
            catalog.insert(
                catalog.end(),
                std::make_move_iterator(discovered.begin()),
                std::make_move_iterator(discovered.end()));
            return catalog;
        }

        std::vector<RuntimeShopEntry>& GetRuntimeShopCatalog()
        {
            static std::vector<RuntimeShopEntry> s_Catalog =
                BuildRuntimeShopCatalog();
            return s_Catalog;
        }

        void RegisterLoLShopItems(Engine::CGameInstance& GameInstance)
        {
            const std::vector<RuntimeShopEntry>& Catalog = GetRuntimeShopCatalog();
            std::vector<std::vector<std::string>> StatTextStorage;
            std::vector<std::vector<const char*>> StatLineStorage;
            std::vector<Engine::UIShopItemAssetDesc> Items;
            const u32_t kItemCount = static_cast<u32_t>(Catalog.size());
            StatTextStorage.reserve(kItemCount);
            StatLineStorage.reserve(kItemCount);
            Items.reserve(kItemCount);

            for (u32_t Index = 0u; Index < kItemCount; ++Index)
            {
                if (!Catalog[Index].bEnabled)
                    continue;

                const RuntimeShopEntry& Entry = Catalog[Index];
                const ClientData::ShopItemPresentationDefinition* pItem =
                    ClientData::FindShopItemPresentationDefinition(Entry.iItemId);
                if (!pItem && Entry.bPurchasable)
                    continue;
                StatTextStorage.push_back(pItem
                    ? BuildItemStatLines(pItem->stats)
                    : std::vector<std::string>{ "Champion-bound item" });
                const std::vector<std::string>& Lines = StatTextStorage.back();

                std::vector<const char*>& LinePointers = StatLineStorage.emplace_back();
                LinePointers.reserve(Lines.size());
                for (const std::string& Line : Lines)
                    LinePointers.push_back(Line.c_str());

                Engine::UIShopItemAssetDesc Desc{};
                Desc.iItemId = Entry.iItemId;
                Desc.iPrice = pItem ? pItem->price : 0u;
                Desc.iOrder = Index;
                Desc.pAssetKey = Entry.strAssetKey.c_str();
                Desc.pSection = Entry.strSection.c_str();
                Desc.pDisplayName = pItem
                    ? pItem->displayName
                    : "Black Spear";
                Desc.pIconPath = Entry.strIconPath.c_str();
                Desc.pIconSprite = Entry.strIconSprite.c_str();
                Desc.pStatLines = LinePointers.empty() ? nullptr : LinePointers.data();
                Desc.iStatLineCount = static_cast<u32_t>(LinePointers.size());
                Desc.bEnabled = true;
                Desc.bPurchasable = Entry.bPurchasable && pItem != nullptr;
                Items.push_back(Desc);
            }

            GameInstance.UI_Register_InGameShopItems(
                Items.data(),
                static_cast<u32_t>(Items.size()));
            GameInstance.UI_Reload_Lua();
        }
    }

    void RegisterLoLUIContent(Engine::CGameInstance& GameInstance)
    {
        GameInstance.UI_Register_ActorHUDAssets(
            kLoLActorHudAssets,
            static_cast<u32_t>(sizeof(kLoLActorHudAssets) / sizeof(kLoLActorHudAssets[0])));
        GameInstance.UI_Register_StatusPanelSpellIconAssets(
            kLoLStatusPanelSpellIcons,
            static_cast<u32_t>(sizeof(kLoLStatusPanelSpellIcons) / sizeof(kLoLStatusPanelSpellIcons[0])));
        GameInstance.UI_Set_StatusPanelDefaultSpellIds(
            kLoLDefaultStatusPanelSpellIds,
            static_cast<u32_t>(sizeof(kLoLDefaultStatusPanelSpellIds) / sizeof(kLoLDefaultStatusPanelSpellIds[0])));
        RegisterLoLShopItems(GameInstance);
    }

    u32_t GetLoLShopEditorEntryCount()
    {
        return static_cast<u32_t>(GetRuntimeShopCatalog().size());
    }

    LoLShopEditorEntryView GetLoLShopEditorEntry(u32_t Index)
    {
        LoLShopEditorEntryView View{};
        const std::vector<RuntimeShopEntry>& Catalog = GetRuntimeShopCatalog();
        if (Index >= Catalog.size())
            return View;

        const RuntimeShopEntry& Entry = Catalog[Index];
        View.iItemId = Entry.iItemId;
        View.pAssetKey = Entry.strAssetKey.c_str();
        View.pSection = Entry.strSection.c_str();
        View.bEnabled = Entry.bEnabled;
        const ClientData::ShopItemPresentationDefinition* pItem =
            ClientData::FindShopItemPresentationDefinition(View.iItemId);
        View.iPrice = pItem ? pItem->price : 0u;
        View.pDisplayName = pItem
            ? pItem->displayName
            : (Entry.bPurchasable ? Entry.strAssetKey.c_str() : "Black Spear");
        View.bRegistered = pItem != nullptr || !Entry.bPurchasable;
        View.bPurchasable =
            Entry.bPurchasable && pItem != nullptr && View.iItemId != 0u;
        return View;
    }

    void SetLoLShopEditorEntryEnabled(u32_t Index, bool_t bEnabled)
    {
        std::vector<RuntimeShopEntry>& Catalog = GetRuntimeShopCatalog();
        if (Index < Catalog.size())
            Catalog[Index].bEnabled = bEnabled;
    }

    bool_t MoveLoLShopEditorEntry(u32_t Index, bool_t bMoveUp)
    {
        std::vector<RuntimeShopEntry>& Catalog = GetRuntimeShopCatalog();
        if (Index >= Catalog.size())
            return false;
        if (bMoveUp && Index == 0u)
            return false;
        if (!bMoveUp && Index + 1u >= Catalog.size())
            return false;

        const u32_t Other = bMoveUp ? Index - 1u : Index + 1u;
        std::swap(Catalog[Index], Catalog[Other]);
        return true;
    }

    void ReloadLoLShopEditorCatalog()
    {
        GetRuntimeShopCatalog() = BuildRuntimeShopCatalog();
    }

    void ReapplyLoLShopItems(Engine::CGameInstance& GameInstance)
    {
        RegisterLoLShopItems(GameInstance);
    }
}
