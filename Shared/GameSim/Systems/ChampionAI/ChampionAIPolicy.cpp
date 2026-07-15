#include "Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h"
#include "Shared/GameSim/Definitions/WardDefinitions.h"

#include <cmath>
#include <cstring>
#include <limits>

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
            10.f,
            12.f,
            18.f,
            16.f,
            0.90f,
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
            1.35f,
            0.00f,
            0.35f,
            0.55f,
            0.70f,
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

namespace
{
    constexpr size_t kChampionAIShadowHeaderBytesV1 = 56u;
    constexpr size_t kChampionAIShadowFileBytesV1 =
        kChampionAIShadowHeaderBytesV1 +
        static_cast<size_t>(kChampionAIShadowFeatureCountV1) * sizeof(f32_t) * 3u;
    constexpr u16_t kChampionAIShadowScalarFloat32V1 = 1u;
    constexpr u8_t kChampionAIShadowMagicV1[8] =
    {
        'W', 'B', 'C', 'P', 'O', 'L', '1', 0u,
    };

    class ChampionAIShadowBinaryReader final
    {
    public:
        ChampionAIShadowBinaryReader(const u8_t* bytes, size_t byteCount)
            : m_bytes(bytes), m_byteCount(byteCount)
        {
        }

        bool_t ReadU8(u8_t& outValue)
        {
            if (!CanRead(1u))
                return false;
            outValue = m_bytes[m_offset++];
            return true;
        }

        bool_t ReadU16(u16_t& outValue)
        {
            if (!CanRead(2u))
                return false;
            outValue = static_cast<u16_t>(m_bytes[m_offset]) |
                static_cast<u16_t>(static_cast<u16_t>(m_bytes[m_offset + 1u]) << 8u);
            m_offset += 2u;
            return true;
        }

        bool_t ReadU32(u32_t& outValue)
        {
            if (!CanRead(4u))
                return false;
            outValue = 0u;
            for (u32_t shift = 0u; shift < 32u; shift += 8u)
                outValue |= static_cast<u32_t>(m_bytes[m_offset++]) << shift;
            return true;
        }

        bool_t ReadU64(u64_t& outValue)
        {
            if (!CanRead(8u))
                return false;
            outValue = 0u;
            for (u32_t shift = 0u; shift < 64u; shift += 8u)
                outValue |= static_cast<u64_t>(m_bytes[m_offset++]) << shift;
            return true;
        }

        bool_t ReadF32(f32_t& outValue)
        {
            u32_t bits = 0u;
            if (!ReadU32(bits))
                return false;
            static_assert(sizeof(bits) == sizeof(outValue));
            std::memcpy(&outValue, &bits, sizeof(outValue));
            return true;
        }

        bool_t ReadMagic()
        {
            for (u8_t expected : kChampionAIShadowMagicV1)
            {
                u8_t actual = 0u;
                if (!ReadU8(actual) || actual != expected)
                    return false;
            }
            return true;
        }

        size_t Offset() const
        {
            return m_offset;
        }

    private:
        bool_t CanRead(size_t count) const
        {
            return m_bytes != nullptr &&
                m_offset <= m_byteCount &&
                count <= m_byteCount - m_offset;
        }

        const u8_t* m_bytes = nullptr;
        size_t m_byteCount = 0u;
        size_t m_offset = 0u;
    };

    bool_t IsFiniteArtifact(
        const ChampionAIShadowPolicyArtifactV1& artifact)
    {
        if (artifact.policyRevision == 0u ||
            artifact.policyRevision <= artifact.sourcePolicyRevision ||
            artifact.featureOrderSha256Prefix !=
                kChampionAIShadowFeatureOrderSha256PrefixV1)
        {
            return false;
        }

        const auto isCanonicalFloat = [](f32_t value)
        {
            return std::isfinite(value) &&
                std::fpclassify(value) != FP_SUBNORMAL;
        };
        for (u16_t i = 0u; i < kChampionAIShadowFeatureCountV1; ++i)
        {
            if (!isCanonicalFloat(artifact.normalizationMean[i]) ||
                !isCanonicalFloat(artifact.normalizationInverseScale[i]) ||
                artifact.normalizationInverseScale[i] <= 0.f ||
                !isCanonicalFloat(artifact.weights[i]))
            {
                return false;
            }
        }
        return true;
    }

    bool_t IsValidTraceHeader(const AiDecisionTraceV1& trace)
    {
        if (trace.schemaVersion != kAiDecisionTraceSchemaVersionV1 ||
            trace.byteSize != sizeof(AiDecisionTraceV1) ||
            trace.observation.schemaVersion != kAiObservationSchemaVersionV1 ||
            trace.observation.byteSize != sizeof(AiObservationV1) ||
            trace.actionMask.schemaVersion != kAiActionMaskSchemaVersionV1 ||
            trace.actionMask.byteSize != sizeof(AiActionMaskV1) ||
            trace.candidateCount != kChampionAIShadowCandidateCountV1)
        {
            return false;
        }

        const u32_t legalMask = trace.actionMask.legalCandidateMask;
        const u32_t illegalMask = trace.actionMask.illegalCandidateMask;
        if ((legalMask & ~kAiAllCandidateBitsV1) != 0u ||
            (illegalMask & ~kAiAllCandidateBitsV1) != 0u ||
            (legalMask & illegalMask) != 0u ||
            (legalMask | illegalMask) != kAiAllCandidateBitsV1)
        {
            return false;
        }

        const AiObservationV1& observation = trace.observation;
        const f32_t observedValues[] =
        {
            observation.selfHpRatio,
            observation.enemyHpRatio,
            observation.selfGold,
            observation.enemyGold,
            observation.enemyDistance,
            observation.attackRange,
            observation.turretDanger,
        };
        for (f32_t value : observedValues)
        {
            if (!std::isfinite(value))
                return false;
        }

        u32_t seenKinds = 0u;
        for (u8_t i = 0u; i < trace.candidateCount; ++i)
        {
            const AiCandidateEvidenceV1& candidate = trace.candidates[i];
            if (candidate.schemaVersion != kAiCandidateEvidenceSchemaVersionV1 ||
                candidate.byteSize != sizeof(AiCandidateEvidenceV1) ||
                candidate.candidateKind < static_cast<u8_t>(AiCandidateKindV1::Retreat) ||
                candidate.candidateKind > static_cast<u8_t>(AiCandidateKindV1::Siege))
            {
                return false;
            }

            const u32_t bit = 1u << (candidate.candidateKind - 1u);
            if ((seenKinds & bit) != 0u)
                return false;
            seenKinds |= bit;

            const bool_t bMaskLegal = (legalMask & bit) != 0u;
            const bool_t bFlagLegal =
                (candidate.flags & kAiCandidateLegalFlagV1) != 0u;
            if (bMaskLegal != bFlagLegal)
                return false;
        }

        return seenKinds == kAiAllCandidateBitsV1;
    }

    const AiCandidateEvidenceV1* FindCandidate(
        const AiDecisionTraceV1& trace,
        u8_t candidateKind)
    {
        for (u8_t i = 0u; i < trace.candidateCount; ++i)
        {
            if (trace.candidates[i].candidateKind == candidateKind)
                return &trace.candidates[i];
        }
        return nullptr;
    }

    u8_t ResolveTargetRelation(
        const AiObservationV1& observation,
        u32_t targetNetEntityId)
    {
        if (targetNetEntityId == 0u)
            return 0u;
        if (targetNetEntityId == observation.selfNetEntityId)
            return 1u;
        if (targetNetEntityId == observation.enemyChampionNetEntityId)
            return 2u;
        if (targetNetEntityId == observation.enemyMinionNetEntityId)
            return 3u;
        if (targetNetEntityId == observation.enemyStructureNetEntityId)
            return 4u;
        if (targetNetEntityId == observation.alliedWaveNetEntityId)
            return 5u;
        return 6u;
    }

    void BuildChampionAIShadowFeatures(
        const AiDecisionTraceV1& trace,
        const AiCandidateEvidenceV1& candidate,
        f64_t (&outFeatures)[kChampionAIShadowFeatureCountV1])
    {
        for (f64_t& value : outFeatures)
            value = 0.0;

        const u8_t candidateKind = candidate.candidateKind;
        outFeatures[candidateKind - 1u] = 1.0;
        outFeatures[4u + ResolveTargetRelation(
            trace.observation,
            candidate.targetNetEntityId)] = 1.0;

        const f64_t context[14] =
        {
            static_cast<f64_t>(trace.observation.capabilityFlags),
            static_cast<f64_t>(trace.observation.selfLevel),
            static_cast<f64_t>(trace.observation.enemyLevel),
            static_cast<f64_t>(trace.observation.selfHpRatio),
            static_cast<f64_t>(trace.observation.enemyHpRatio),
            static_cast<f64_t>(trace.observation.selfGold),
            static_cast<f64_t>(trace.observation.enemyGold),
            static_cast<f64_t>(trace.observation.enemyDistance),
            static_cast<f64_t>(trace.observation.attackRange),
            static_cast<f64_t>(trace.observation.turretDanger),
            static_cast<f64_t>(trace.actionMask.legalCandidateMask),
            static_cast<f64_t>(trace.actionMask.illegalCandidateMask),
            static_cast<f64_t>(trace.actionMask.availableActionMask),
            static_cast<f64_t>(trace.actionMask.availableSkillMask),
        };
        const u16_t base = static_cast<u16_t>(
            11u + (candidateKind - 1u) * 14u);
        for (u16_t i = 0u; i < 14u; ++i)
            outFeatures[base + i] = context[i];
    }
}

bool_t DecodeChampionAIShadowPolicyArtifactV1(
    const u8_t* bytes,
    size_t byteCount,
    ChampionAIShadowPolicyArtifactV1& outArtifact)
{
    if (bytes == nullptr || byteCount != kChampionAIShadowFileBytesV1)
        return false;

    ChampionAIShadowBinaryReader reader(bytes, byteCount);
    u16_t artifactSchemaVersion = 0u;
    u16_t headerBytes = 0u;
    u32_t fileBytes = 0u;
    u16_t traceSchemaVersion = 0u;
    u16_t observationSchemaVersion = 0u;
    u16_t actionSchemaVersion = 0u;
    u16_t featureCount = 0u;
    u16_t candidateCount = 0u;
    u16_t scalarType = 0u;
    u32_t reserved = 0u;
    ChampionAIShadowPolicyArtifactV1 decoded{};

    if (!reader.ReadMagic() ||
        !reader.ReadU16(artifactSchemaVersion) ||
        !reader.ReadU16(headerBytes) ||
        !reader.ReadU32(fileBytes) ||
        !reader.ReadU16(traceSchemaVersion) ||
        !reader.ReadU16(observationSchemaVersion) ||
        !reader.ReadU16(actionSchemaVersion) ||
        !reader.ReadU16(featureCount) ||
        !reader.ReadU16(candidateCount) ||
        !reader.ReadU16(scalarType) ||
        !reader.ReadU64(decoded.policyRevision) ||
        !reader.ReadU64(decoded.sourcePolicyRevision) ||
        !reader.ReadU64(decoded.featureOrderSha256Prefix) ||
        !reader.ReadU32(reserved))
    {
        return false;
    }

    if (artifactSchemaVersion != kChampionAIShadowPolicySchemaVersionV1 ||
        headerBytes != kChampionAIShadowHeaderBytesV1 ||
        fileBytes != byteCount ||
        traceSchemaVersion != kAiDecisionTraceSchemaVersionV1 ||
        observationSchemaVersion != kAiObservationSchemaVersionV1 ||
        actionSchemaVersion != kAiActionMaskSchemaVersionV1 ||
        featureCount != kChampionAIShadowFeatureCountV1 ||
        candidateCount != kChampionAIShadowCandidateCountV1 ||
        scalarType != kChampionAIShadowScalarFloat32V1 ||
        reserved != 0u ||
        reader.Offset() != kChampionAIShadowHeaderBytesV1)
    {
        return false;
    }

    for (u16_t i = 0u; i < featureCount; ++i)
    {
        if (!reader.ReadF32(decoded.normalizationMean[i]))
            return false;
    }
    for (u16_t i = 0u; i < featureCount; ++i)
    {
        if (!reader.ReadF32(decoded.normalizationInverseScale[i]))
            return false;
    }
    for (u16_t i = 0u; i < featureCount; ++i)
    {
        if (!reader.ReadF32(decoded.weights[i]))
            return false;
    }

    if (reader.Offset() != byteCount || !IsFiniteArtifact(decoded))
        return false;

    outArtifact = decoded;
    return true;
}

ChampionAIShadowDecisionV1 EvaluateChampionAIShadowPolicyV1(
    const ChampionAIShadowPolicyArtifactV1* artifact,
    const AiDecisionTraceV1& trace)
{
    ChampionAIShadowDecisionV1 result{};
    if (artifact == nullptr)
        return result;
    if (!IsFiniteArtifact(*artifact))
    {
        result.status = eChampionAIShadowStatusV1::InvalidArtifact;
        return result;
    }
    if (!IsValidTraceHeader(trace))
    {
        result.status = eChampionAIShadowStatusV1::InvalidTrace;
        return result;
    }

    result.legalCandidateMask = trace.actionMask.legalCandidateMask;
    result.activeCandidateKind = trace.selectedCandidateKind;
    if (result.activeCandidateKind <
            static_cast<u8_t>(AiCandidateKindV1::Retreat) ||
        result.activeCandidateKind >
            static_cast<u8_t>(AiCandidateKindV1::Siege) ||
        (result.legalCandidateMask &
            (1u << (result.activeCandidateKind - 1u))) == 0u)
    {
        result.status = eChampionAIShadowStatusV1::InvalidTrace;
        return result;
    }
    u8_t legalCount = 0u;
    for (u8_t kind = 1u; kind <= kChampionAIShadowCandidateCountV1; ++kind)
    {
        if ((result.legalCandidateMask & (1u << (kind - 1u))) != 0u)
            ++legalCount;
    }
    if (legalCount < 2u)
    {
        result.status = eChampionAIShadowStatusV1::InsufficientLegalCandidates;
        return result;
    }

    f64_t bestLogit = -std::numeric_limits<f64_t>::infinity();
    f64_t secondLogit = -std::numeric_limits<f64_t>::infinity();
    u8_t bestKind = 0u;
    u8_t secondKind = 0u;
    for (u8_t kind = 1u; kind <= kChampionAIShadowCandidateCountV1; ++kind)
    {
        const u32_t bit = 1u << (kind - 1u);
        if ((result.legalCandidateMask & bit) == 0u)
            continue;

        const AiCandidateEvidenceV1* candidate = FindCandidate(trace, kind);
        if (candidate == nullptr)
        {
            result.status = eChampionAIShadowStatusV1::InvalidTrace;
            return result;
        }

        f64_t features[kChampionAIShadowFeatureCountV1]{};
        BuildChampionAIShadowFeatures(trace, *candidate, features);
        f64_t logit = 0.0;
        for (u16_t i = 0u; i < kChampionAIShadowFeatureCountV1; ++i)
        {
            logit +=
                (features[i] - static_cast<f64_t>(artifact->normalizationMean[i])) *
                static_cast<f64_t>(artifact->normalizationInverseScale[i]) *
                static_cast<f64_t>(artifact->weights[i]);
        }
        if (!std::isfinite(logit) ||
            logit > static_cast<f64_t>(std::numeric_limits<f32_t>::max()) ||
            logit < -static_cast<f64_t>(std::numeric_limits<f32_t>::max()))
        {
            result.status = eChampionAIShadowStatusV1::InvalidArtifact;
            return result;
        }
        result.logits[kind - 1u] = static_cast<f32_t>(logit);

        if (logit > bestLogit)
        {
            secondLogit = bestLogit;
            secondKind = bestKind;
            bestLogit = logit;
            bestKind = kind;
        }
        else if (logit > secondLogit)
        {
            secondLogit = logit;
            secondKind = kind;
        }
    }

    if (bestKind == 0u || secondKind == 0u ||
        !std::isfinite(bestLogit) || !std::isfinite(secondLogit))
    {
        result.status = eChampionAIShadowStatusV1::InvalidTrace;
        return result;
    }

    result.status = eChampionAIShadowStatusV1::Evaluated;
    result.shadowCandidateKind = bestKind;
    result.bDisagreed = result.activeCandidateKind != bestKind;
    result.selectedMargin = static_cast<f32_t>(bestLogit - secondLogit);

    const AiCandidateEvidenceV1* selectedCandidate = FindCandidate(trace, bestKind);
    const AiCandidateEvidenceV1* runnerUpCandidate =
        FindCandidate(trace, secondKind);
    if (selectedCandidate == nullptr || runnerUpCandidate == nullptr)
    {
        result.status = eChampionAIShadowStatusV1::InvalidTrace;
        return result;
    }
    f64_t selectedFeatures[kChampionAIShadowFeatureCountV1]{};
    f64_t runnerUpFeatures[kChampionAIShadowFeatureCountV1]{};
    BuildChampionAIShadowFeatures(trace, *selectedCandidate, selectedFeatures);
    BuildChampionAIShadowFeatures(trace, *runnerUpCandidate, runnerUpFeatures);
    f64_t greatestAbsoluteContribution = -1.0;
    for (u16_t i = 0u; i < kChampionAIShadowFeatureCountV1; ++i)
    {
        // The normalization mean cancels in the selected-vs-runner-up
        // logit margin. Reporting this delta answers why the winner ranked
        // above its nearest alternative instead of showing a common offset.
        const f64_t contribution =
            (selectedFeatures[i] - runnerUpFeatures[i]) *
            static_cast<f64_t>(artifact->normalizationInverseScale[i]) *
            static_cast<f64_t>(artifact->weights[i]);
        const f64_t absoluteContribution = std::abs(contribution);
        if (absoluteContribution > greatestAbsoluteContribution)
        {
            greatestAbsoluteContribution = absoluteContribution;
            result.topFeatureIndex = i;
            result.topFeatureContribution = static_cast<f32_t>(contribution);
        }
    }
    return result;
}
