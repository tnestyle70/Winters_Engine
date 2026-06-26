#include "GamePlay/LoLUIContentRegistry.h"

#include "GameInstance.h"
#include "Manager/UI/ActorHUDAssets.h"
#include "Shared/GameSim/Components/ChampionScore.h"
#include "Shared/GameSim/Definitions/ItemDef.h"
#include "Shared/GameSim/Definitions/LoLMatchContext.h"

#include <cstdio>
#include <string>
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

        constexpr u16_t kLoLShopItemIds[] =
        {
            1055, 1056, 1036, 1042, 1028, 1029, 1033,
            1001, 1052, 1037, 1043, 1053, 1058, 1038,
            3153,
        };

        void RegisterLoLShopItems(Engine::CGameInstance& GameInstance)
        {
            std::vector<std::vector<std::string>> StatTextStorage;
            std::vector<std::vector<const char*>> StatLineStorage;
            std::vector<Engine::UIShopItemAssetDesc> Items;
            StatTextStorage.reserve(sizeof(kLoLShopItemIds) / sizeof(kLoLShopItemIds[0]));
            StatLineStorage.reserve(sizeof(kLoLShopItemIds) / sizeof(kLoLShopItemIds[0]));
            Items.reserve(sizeof(kLoLShopItemIds) / sizeof(kLoLShopItemIds[0]));

            for (const u16_t iItemId : kLoLShopItemIds)
            {
                const ItemDef* pItem = CItemRegistry::Instance().Find(iItemId);
                if (!pItem)
                    continue;

                StatTextStorage.push_back(BuildItemStatLines(pItem->stats));
                const std::vector<std::string>& Lines = StatTextStorage.back();

                std::vector<const char*>& LinePointers = StatLineStorage.emplace_back();
                LinePointers.reserve(Lines.size());
                for (const std::string& Line : Lines)
                    LinePointers.push_back(Line.c_str());

                Engine::UIShopItemAssetDesc Desc{};
                Desc.iItemId = pItem->itemId;
                Desc.iPrice = pItem->price;
                Desc.pDisplayName = pItem->displayName;
                Desc.pStatLines = LinePointers.empty() ? nullptr : LinePointers.data();
                Desc.iStatLineCount = static_cast<u32_t>(LinePointers.size());
                Items.push_back(Desc);
            }

            GameInstance.UI_Register_InGameShopItems(
                Items.data(),
                static_cast<u32_t>(Items.size()));
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
}
