#include "Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h"
#include "Shared/GameSim/Definitions/WardDefinitions.h"

namespace
{
    constexpr ChampionAIProfile MakeDefaultProfile()
    {
        return ChampionAIProfile{
            eChampion::END,
            1.5f,
            6.f,
            10.f,
            18.f,
            14.f,
            1.00f,
            0.00f,
            0.35f,
            0.55f,
            1.00f,
            1.00f,
            1.00f,
            1.00f,
            {},
            0
        };
    }

    constexpr ChampionAIProfile MakeAsheProfile()
    {
        return ChampionAIProfile{
            eChampion::ASHE,
            6.f,
            9.f,
            12.f,
            18.f,
            16.f,
            0.75f,
            1.25f,
            0.48f,
            0.65f,
            1.10f,
            1.20f,
            1.15f,
            0.80f,
            {
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::W), 0.f, 1.f },
            },
            1
        };
    }

    constexpr ChampionAIProfile MakeEzrealProfile()
    {
        return ChampionAIProfile{
            eChampion::EZREAL,
            6.25f,
            11.f,
            12.f,
            18.f,
            17.f,
            0.95f,
            1.20f,
            0.42f,
            0.62f,
            1.00f,
            1.10f,
            1.05f,
            0.85f,
            {
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::Q), 0.f, 1.f },
            },
            1
        };
    }

    constexpr ChampionAIProfile MakeFioraProfile()
    {
        return ChampionAIProfile{
            eChampion::FIORA,
            1.5f,
            6.f,
            10.f,
            18.f,
            14.f,
            1.20f,
            0.20f,
            0.50f,
            0.65f,
            0.90f,
            0.95f,
            1.10f,
            1.00f,
            {
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::Q), 0.f, 1.f },
            },
            1
        };
    }

    constexpr ChampionAIProfile MakeJaxProfile()
    {
        return ChampionAIProfile{
            eChampion::JAX,
            1.5f,
            7.f,
            10.f,
            18.f,
            14.f,
            1.25f,
            0.00f,
            0.50f,
            0.65f,
            0.80f,
            0.90f,
            1.00f,
            1.05f,
            {
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::Q), 0.f, 1.f },
            },
            1
        };
    }

    constexpr ChampionAIProfile MakeLeeSinProfile()
    {
        return ChampionAIProfile{
            eChampion::LEESIN,
            1.5f,
            7.f,
            10.f,
            18.f,
            14.f,
            1.15f,
            0.00f,
            0.35f,
            0.55f,
            1.00f,
            1.00f,
            1.00f,
            1.00f,
            {
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::Q), 0.f, 1.f },
            },
            1
        };
    }

    constexpr ChampionAIProfile MakeKindredProfile()
    {
        return ChampionAIProfile{
            eChampion::KINDRED,
            5.5f,
            9.f,
            12.f,
            18.f,
            16.f,
            0.85f,
            1.15f,
            0.42f,
            0.62f,
            1.05f,
            1.10f,
            1.10f,
            0.85f,
            {
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::Q), 0.f, 1.f },
            },
            1
        };
    }

    constexpr ChampionAIProfile MakeMasterYiProfile()
    {
        return ChampionAIProfile{
            eChampion::MASTERYI,
            1.5f,
            7.f,
            10.f,
            18.f,
            14.f,
            1.25f,
            0.00f,
            0.35f,
            0.55f,
            1.00f,
            1.00f,
            1.05f,
            1.05f,
            {
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::Q), 0.f, 1.f },
            },
            1
        };
    }

    constexpr ChampionAIProfile MakeAnnieProfile()
    {
        return ChampionAIProfile{
            eChampion::ANNIE,
            6.f,
            9.f,
            12.f,
            18.f,
            16.f,
            0.85f,
            1.00f,
            0.45f,
            0.62f,
            1.05f,
            1.10f,
            1.05f,
            0.80f,
            {
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::Q), 0.f, 1.f },
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::W), 0.f, 0.8f },
            },
            2
        };
    }

    constexpr ChampionAIProfile MakeGarenProfile()
    {
        return ChampionAIProfile{
            eChampion::GAREN,
            1.5f,
            6.5f,
            10.f,
            18.f,
            14.f,
            1.10f,
            0.00f,
            0.42f,
            0.60f,
            0.90f,
            0.95f,
            1.05f,
            1.05f,
            {
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::E), 0.f, 1.f },
            },
            1
        };
    }

    constexpr ChampionAIProfile MakeIreliaProfile()
    {
        return ChampionAIProfile{
            eChampion::IRELIA,
            1.75f,
            7.f,
            10.f,
            18.f,
            14.f,
            1.15f,
            0.10f,
            0.42f,
            0.60f,
            0.95f,
            1.00f,
            1.05f,
            1.00f,
            {
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::Q), 0.f, 1.f },
            },
            1
        };
    }

    constexpr ChampionAIProfile MakeKalistaProfile()
    {
        return ChampionAIProfile{
            eChampion::KALISTA,
            5.5f,
            9.f,
            12.f,
            18.f,
            16.f,
            0.85f,
            1.35f,
            0.45f,
            0.65f,
            1.10f,
            1.15f,
            1.10f,
            0.80f,
            {
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::Q), 0.f, 1.f },
            },
            1
        };
    }

    constexpr ChampionAIProfile MakeRivenProfile()
    {
        return ChampionAIProfile{
            eChampion::RIVEN,
            1.5f,
            7.f,
            10.f,
            18.f,
            14.f,
            1.15f,
            0.00f,
            0.40f,
            0.58f,
            0.90f,
            0.95f,
            1.05f,
            1.00f,
            {
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::Q), 0.f, 1.f },
            },
            1
        };
    }

    constexpr ChampionAIProfile MakeSylasProfile()
    {
        return ChampionAIProfile{
            eChampion::SYLAS,
            1.5f,
            7.f,
            10.f,
            18.f,
            14.f,
            1.05f,
            0.00f,
            0.42f,
            0.60f,
            0.95f,
            1.00f,
            1.00f,
            1.00f,
            {},
            0
        };
    }

    constexpr ChampionAIProfile MakeViegoProfile()
    {
        return ChampionAIProfile{
            eChampion::VIEGO,
            1.5f,
            7.f,
            10.f,
            18.f,
            14.f,
            1.10f,
            0.00f,
            0.40f,
            0.58f,
            0.95f,
            1.00f,
            1.05f,
            1.00f,
            {
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::Q), 0.f, 1.f },
            },
            1
        };
    }

    constexpr ChampionAIProfile MakeYasuoProfile()
    {
        return ChampionAIProfile{
            eChampion::YASUO,
            1.75f,
            7.f,
            10.f,
            18.f,
            14.f,
            1.10f,
            0.10f,
            0.42f,
            0.60f,
            0.95f,
            1.00f,
            1.05f,
            1.00f,
            {
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::Q), 0.f, 1.f },
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::E), 0.f, 0.9f },
            },
            2
        };
    }

    constexpr ChampionAIProfile MakeYoneProfile()
    {
        return ChampionAIProfile{
            eChampion::YONE,
            1.75f,
            7.f,
            10.f,
            18.f,
            14.f,
            1.10f,
            0.10f,
            0.42f,
            0.60f,
            0.95f,
            1.00f,
            1.05f,
            1.00f,
            {
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::Q), 0.f, 1.f },
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::W), 0.f, 0.8f },
            },
            2
        };
    }

    constexpr ChampionAIProfile MakeZedProfile()
    {
        return ChampionAIProfile{
            eChampion::ZED,
            1.5f,
            8.f,
            10.f,
            18.f,
            14.f,
            1.05f,
            0.20f,
            0.40f,
            0.58f,
            0.95f,
            1.00f,
            1.00f,
            0.95f,
            {
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::Q), 0.f, 1.f },
            },
            1
        };
    }
}

const ChampionAIProfile& GetChampionAIProfile(eChampion champion)
{
    static constexpr ChampionAIProfile s_Default = MakeDefaultProfile();
    static constexpr ChampionAIProfile s_Annie = MakeAnnieProfile();
    static constexpr ChampionAIProfile s_Ashe = MakeAsheProfile();
    static constexpr ChampionAIProfile s_Ezreal = MakeEzrealProfile();
    static constexpr ChampionAIProfile s_Fiora = MakeFioraProfile();
    static constexpr ChampionAIProfile s_Garen = MakeGarenProfile();
    static constexpr ChampionAIProfile s_Irelia = MakeIreliaProfile();
    static constexpr ChampionAIProfile s_Jax = MakeJaxProfile();
    static constexpr ChampionAIProfile s_Kalista = MakeKalistaProfile();
    static constexpr ChampionAIProfile s_Kindred = MakeKindredProfile();
    static constexpr ChampionAIProfile s_LeeSin = MakeLeeSinProfile();
    static constexpr ChampionAIProfile s_MasterYi = MakeMasterYiProfile();
    static constexpr ChampionAIProfile s_Riven = MakeRivenProfile();
    static constexpr ChampionAIProfile s_Sylas = MakeSylasProfile();
    static constexpr ChampionAIProfile s_Viego = MakeViegoProfile();
    static constexpr ChampionAIProfile s_Yasuo = MakeYasuoProfile();
    static constexpr ChampionAIProfile s_Yone = MakeYoneProfile();
    static constexpr ChampionAIProfile s_Zed = MakeZedProfile();

    switch (champion)
    {
    case eChampion::ANNIE:
        return s_Annie;
    case eChampion::ASHE:
        return s_Ashe;
    case eChampion::EZREAL:
        return s_Ezreal;
    case eChampion::FIORA:
        return s_Fiora;
    case eChampion::GAREN:
        return s_Garen;
    case eChampion::IRELIA:
        return s_Irelia;
    case eChampion::JAX:
        return s_Jax;
    case eChampion::KALISTA:
        return s_Kalista;
    case eChampion::KINDRED:
        return s_Kindred;
    case eChampion::LEESIN:
        return s_LeeSin;
    case eChampion::MASTERYI:
        return s_MasterYi;
    case eChampion::RIVEN:
        return s_Riven;
    case eChampion::SYLAS:
        return s_Sylas;
    case eChampion::VIEGO:
        return s_Viego;
    case eChampion::YASUO:
        return s_Yasuo;
    case eChampion::YONE:
        return s_Yone;
    case eChampion::ZED:
        return s_Zed;
    default:
        return s_Default;
    }
}

const ChampionAIComboPlan& GetChampionAIComboPlan(eChampion champion)
{
    static constexpr ChampionAIComboPlan s_Default{
        {
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::Q), 0, 0.f, 0.f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::BasicAttack), 0, 0.f, 0.f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::E), 0, 0.f, 0.f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::BasicAttack), 0, 0.f, 0.f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::R), 0, 0.f, 0.f, 0.35f, 0.60f },
        },
        5
    };

    static constexpr ChampionAIComboPlan s_Jax{
        {
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::Q), 0, 0.f, 7.0f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::W), 0, 0.f, 1.75f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::BasicAttack), 0, 0.f, 1.75f, 0.35f, 1.00f },
        },
        3
    };

    static constexpr ChampionAIComboPlan s_Fiora{
        {
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::Q), 0, 0.f, 4.0f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::E), 0, 0.f, 1.75f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::BasicAttack), 0, 0.f, 1.75f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::BasicAttack), 0, 0.f, 1.75f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::W), 0, 0.f, 2.5f, 0.35f, 1.00f },
        },
        5
    };

    static constexpr ChampionAIComboPlan s_Ashe{
        {
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::W), 0, 0.f, 9.0f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::Q), 0, 0.f, 6.0f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::BasicAttack), 0, 0.f, 6.0f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::BasicAttack), 0, 0.f, 6.0f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::BasicAttack), 0, 0.f, 6.0f, 0.35f, 1.00f },
        },
        5
    };

    static constexpr ChampionAIComboPlan s_Riven{
        {
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::Q), 0, 0.f, 4.5f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::BasicAttack), 0, 0.f, 1.75f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::Q), 0, 0.f, 4.5f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::BasicAttack), 0, 0.f, 1.75f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::Q), 0, 0.f, 4.5f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::W), 0, 0.f, 2.5f, 0.35f, 1.00f },
            ChampionAIComboStep{
                static_cast<u8_t>(eSkillSlot::E),
                0,
                0.f,
                4.0f,
                0.35f,
                1.00f,
                static_cast<u8_t>(eChampionAIComboTargetMode::AwayFromTarget)
            },
        },
        7
    };

    static constexpr ChampionAIComboPlan s_LeeSin{
        {
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::Q), 0, 0.f, 11.0f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::Q), 2, 0.f, 11.0f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::BasicAttack), 0, 0.f, 1.75f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::E), 0, 0.f, 3.5f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::BasicAttack), 0, 0.f, 1.75f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::E), 2, 0.f, 3.5f, 0.35f, 1.00f },
            ChampionAIComboStep{
                static_cast<u8_t>(eSkillSlot::W),
                kTrinketWardItemId,
                0.f,
                kWardPlacementRange,
                0.35f,
                1.00f,
                static_cast<u8_t>(eChampionAIComboTargetMode::WardBehindTarget)
            },
            ChampionAIComboStep{
                static_cast<u8_t>(eSkillSlot::W),
                0,
                0.f,
                7.0f,
                0.35f,
                1.00f,
                static_cast<u8_t>(eChampionAIComboTargetMode::LastOwnWard)
            },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::R), 0, 0.f, 3.0f, 0.35f, 0.70f },
        },
        9
    };

    static constexpr ChampionAIComboPlan s_Sylas{
        {
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::Q), 0, 0.f, 4.0f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::E), 0, 0.f, 6.0f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::E), 2, 0.f, 6.0f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::BasicAttack), 0, 0.f, 1.75f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::W), 0, 0.f, 5.0f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::BasicAttack), 0, 0.f, 1.75f, 0.35f, 1.00f },
            ChampionAIComboStep{
                static_cast<u8_t>(eSkillSlot::R),
                0,
                0.f,
                10.0f,
                0.35f,
                1.00f,
                static_cast<u8_t>(eChampionAIComboTargetMode::SylasHijackTarget)
            },
            ChampionAIComboStep{
                static_cast<u8_t>(eSkillSlot::R),
                0,
                0.f,
                10.0f,
                0.35f,
                1.00f,
                static_cast<u8_t>(eChampionAIComboTargetMode::SylasStolenUltimateTarget)
            },
        },
        8
    };

    switch (champion)
    {
    case eChampion::JAX:
        return s_Jax;
    case eChampion::FIORA:
        return s_Fiora;
    case eChampion::ASHE:
        return s_Ashe;
    case eChampion::RIVEN:
        return s_Riven;
    case eChampion::LEESIN:
        return s_LeeSin;
    case eChampion::SYLAS:
        return s_Sylas;
    default:
        return s_Default;
    }
}
