#include "Server/Private/Data/RuntimeGameplayDefinitionOverlay.h"

#include "Server/Private/Data/LoLGameplayDefinitionPack.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

#if defined(_DEBUG)
#include "Server/Private/Data/ThirdParty/json.hpp"

#include <Windows.h>

#include <cmath>
#include <fstream>
#include <sstream>
#endif

namespace
{
    std::atomic<const GameplayDefinitionPack*> g_pActiveRuntimePack{ nullptr };
    std::atomic<const SpawnObjectDefinitionPack*> g_pActiveRuntimeSpawnObjectPack{ nullptr };
    std::atomic<u32_t> g_runtimeRevision{ 0u };

#if defined(_DEBUG)
    using json = nlohmann::json;

    struct RuntimePackStorage
    {
        std::vector<ChampionGameplayDef> champions;
        std::vector<SkillGameplayDef> skills;
        std::vector<SummonerSpellGameplayDef> summonerSpells;
        EconomyGameplayDef economyStorage{};
        // displayName 은 코드젠 문자열 리터럴 포인터 공유 (JSON 이름은 무시 = 구조 소관).
        std::vector<ItemDef> items;
        GameplayDefinitionPack pack{};
        // SpawnObject 팩 사본 + 소유 배열 (jungleCamps/minions 포인터가 이 벡터를 가리킨다).
        std::vector<JungleCampGameDefEntry> jungleCamps;
        std::vector<MinionCombatDefEntry> minions;
        SpawnObjectDefinitionPack spawnPack{};
    };

    std::mutex& ReloadMutex()
    {
        static std::mutex s_mutex;
        return s_mutex;
    }

    // 이전 세대 팩은 다른 룸 틱 스레드가 읽는 중일 수 있어 해제하지 않는다.
    // Debug 디자이너 도구 — 누적량 = 성공 리로드 횟수뿐.
    std::vector<std::unique_ptr<RuntimePackStorage>>& StorageGenerations()
    {
        static std::vector<std::unique_ptr<RuntimePackStorage>> s_generations;
        return s_generations;
    }

    // Tools/LoLData/Build-LoLDefinitionPack.py definition_key() 와 동일한 FNV-1a 32.
    u32_t Fnv1a32(const std::string& text)
    {
        u32_t value = 2166136261u;
        for (const char ch : text)
        {
            value ^= static_cast<u8_t>(ch);
            value *= 16777619u;
        }
        return value;
    }

    std::string ToLowerAscii(std::string text)
    {
        for (char& ch : text)
        {
            if (ch >= 'A' && ch <= 'Z')
                ch = static_cast<char>(ch - 'A' + 'a');
        }
        return text;
    }

    bool_t ReadFileText(const std::wstring& path, std::string& outText)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
            return false;
        std::ostringstream buffer;
        buffer << file.rdbuf();
        outText = buffer.str();
        return true;
    }

    // GameRoomNav.cpp / WalkabilityAuthority.cpp 의 워크스페이스 규약과 동일:
    // 시작 디렉터리에서 상위 8단계까지 Winters.sln 마커를 찾아 <root>/<relative>.
    bool_t TryResolveFromDirectory(
        const std::wstring& startDir,
        const wchar_t* relative,
        std::wstring& outPath)
    {
        std::wstring dir = startDir;
        for (int depth = 0; depth < 8; ++depth)
        {
            if (dir.empty())
                break;
            const std::wstring marker = dir + L"\\Winters.sln";
            if (GetFileAttributesW(marker.c_str()) != INVALID_FILE_ATTRIBUTES)
            {
                const std::wstring candidate = dir + L"\\" + relative;
                if (GetFileAttributesW(candidate.c_str()) != INVALID_FILE_ATTRIBUTES)
                {
                    outPath = candidate;
                    return true;
                }
                return false;
            }
            const size_t slash = dir.find_last_of(L"\\/");
            if (slash == std::wstring::npos)
                break;
            dir.resize(slash);
        }
        return false;
    }

    bool_t ResolveWorkspaceDataFile(const wchar_t* relative, std::wstring& outPath)
    {
        wchar_t exePath[MAX_PATH]{};
        const DWORD len = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        if (len > 0 && len < MAX_PATH)
        {
            std::wstring exeDir = exePath;
            const size_t slash = exeDir.find_last_of(L"\\/");
            if (slash != std::wstring::npos)
            {
                exeDir.resize(slash);
                if (TryResolveFromDirectory(exeDir, relative, outPath))
                    return true;
            }
        }

        wchar_t cwd[MAX_PATH]{};
        const DWORD cwdLen = GetCurrentDirectoryW(MAX_PATH, cwd);
        if (cwdLen > 0 && cwdLen < MAX_PATH &&
            TryResolveFromDirectory(cwd, relative, outPath))
        {
            return true;
        }

        return false;
    }

    ChampionGameplayDef* FindChampionMutable(
        std::vector<ChampionGameplayDef>& champions,
        DefinitionKey key)
    {
        for (ChampionGameplayDef& champion : champions)
        {
            if (champion.key == key)
                return &champion;
        }
        return nullptr;
    }

    SkillGameplayDef* FindSkillMutableByKey(
        std::vector<SkillGameplayDef>& skills,
        DefinitionKey key)
    {
        for (SkillGameplayDef& skill : skills)
        {
            if (skill.key == key)
                return &skill;
        }
        return nullptr;
    }

    SkillGameplayDef* FindSkillMutableById(
        std::vector<SkillGameplayDef>& skills,
        SkillDefId id)
    {
        if (!id.IsValid())
            return nullptr;
        for (SkillGameplayDef& skill : skills)
        {
            if (skill.id.value == id.value)
                return &skill;
        }
        return nullptr;
    }

    SummonerSpellGameplayDef* FindSummonerSpellMutable(
        std::vector<SummonerSpellGameplayDef>& spells,
        DefinitionKey key)
    {
        for (SummonerSpellGameplayDef& spell : spells)
        {
            if (spell.key == key)
                return &spell;
        }
        return nullptr;
    }

    // Build-LoLDefinitionPack.py SKILL_EFFECT_PARAM_IDS 와 1:1 (이름 정확 일치 필수).
    struct SkillEffectParamName
    {
        const char* name;
        eSkillEffectParamId id;
    };

    constexpr SkillEffectParamName kSkillEffectParamNames[] =
    {
        { "baseDamage", eSkillEffectParamId::BaseDamage },
        { "damagePerRank", eSkillEffectParamId::DamagePerRank },
        { "range", eSkillEffectParamId::Range },
        { "speed", eSkillEffectParamId::Speed },
        { "moveSpeedMul", eSkillEffectParamId::MoveSpeedMul },
        { "stunDurationSec", eSkillEffectParamId::StunDurationSec },
        { "slowDurationSec", eSkillEffectParamId::SlowDurationSec },
        { "airborneDurationSec", eSkillEffectParamId::AirborneDurationSec },
        { "markDurationSec", eSkillEffectParamId::MarkDurationSec },
        { "stackWindowSec", eSkillEffectParamId::StackWindowSec },
        { "maxStacks", eSkillEffectParamId::MaxStacks },
        { "gap", eSkillEffectParamId::Gap },
        { "dashDistance", eSkillEffectParamId::DashDistance },
        { "dashDurationSec", eSkillEffectParamId::DashDurationSec },
        { "targetDashDurationSec", eSkillEffectParamId::TargetDashDurationSec },
        { "dashDelaySec", eSkillEffectParamId::DashDelaySec },
        { "effectDurationSec", eSkillEffectParamId::EffectDurationSec },
        { "tickIntervalSec", eSkillEffectParamId::TickIntervalSec },
        { "refreshDurationSec", eSkillEffectParamId::RefreshDurationSec },
        { "vanishDurationSec", eSkillEffectParamId::VanishDurationSec },
        { "missingHealthDamageRatio", eSkillEffectParamId::MissingHealthDamageRatio },
        { "minHealthAmount", eSkillEffectParamId::MinHealthAmount },
        { "healBaseAmount", eSkillEffectParamId::HealBaseAmount },
        { "healAmountPerRank", eSkillEffectParamId::HealAmountPerRank },
        { "rectLength", eSkillEffectParamId::RectLength },
        { "rectLengthPerRank", eSkillEffectParamId::RectLengthPerRank },
        { "rectWidth", eSkillEffectParamId::RectWidth },
        { "formationDelaySec", eSkillEffectParamId::FormationDelaySec },
        { "damagePerSpear", eSkillEffectParamId::DamagePerSpear },
        { "halfWidth", eSkillEffectParamId::HalfWidth },
        { "disarmDurationSec", eSkillEffectParamId::DisarmDurationSec },
        { "tornadoSpeed", eSkillEffectParamId::TornadoSpeed },
        { "tornadoDurationSec", eSkillEffectParamId::TornadoDurationSec },
        { "tornadoRadius", eSkillEffectParamId::TornadoRadius },
        { "tornadoDamage", eSkillEffectParamId::TornadoDamage },
        { "dashAreaRadius", eSkillEffectParamId::DashAreaRadius },
        { "dashAreaDamage", eSkillEffectParamId::DashAreaDamage },
        { "bonusAd", eSkillEffectParamId::BonusAd },
        { "bonusAttackSpeed", eSkillEffectParamId::BonusAttackSpeed },
        { "totalAdRatio", eSkillEffectParamId::TotalAdRatio },
        { "bonusAdRatio", eSkillEffectParamId::BonusAdRatio },
        { "apRatio", eSkillEffectParamId::ApRatio },
        { "nonEpicBaseDamage", eSkillEffectParamId::NonEpicBaseDamage },
        { "nonEpicDamagePerRank", eSkillEffectParamId::NonEpicDamagePerRank },
        { "cooldownRefundSec", eSkillEffectParamId::CooldownRefundSec },
        { "manaRestoreFlat", eSkillEffectParamId::ManaRestoreFlat },
        { "castTimeSec", eSkillEffectParamId::CastTimeSec },
        { "manaCostPerRank", eSkillEffectParamId::ManaCostPerRank },
        { "cooldownReductionPerRank", eSkillEffectParamId::CooldownReductionPerRank },
        { "halfAngleCos", eSkillEffectParamId::HalfAngleCos },
        { "radius", eSkillEffectParamId::Radius },
        { "shieldDurationSec", eSkillEffectParamId::ShieldDurationSec },
        { "shieldBaseAmount", eSkillEffectParamId::ShieldBaseAmount },
        { "shieldAmountPerRank", eSkillEffectParamId::ShieldAmountPerRank },
        { "shieldArmorPerRank", eSkillEffectParamId::ShieldArmorPerRank },
    };

    // Build-LoLDefinitionPack.py SUMMON_POLICY_PARAM_IDS 와 1:1.
    struct SummonPolicyParamName
    {
        const char* name;
        eSummonPolicyParamId id;
    };

    constexpr SummonPolicyParamName kSummonPolicyParamNames[] =
    {
        { "durationSec", eSummonPolicyParamId::DurationSec },
        { "moveSpeed", eSummonPolicyParamId::MoveSpeed },
        { "attackRange", eSummonPolicyParamId::AttackRange },
        { "sightRange", eSummonPolicyParamId::SightRange },
        { "attackCooldownSec", eSummonPolicyParamId::AttackCooldownSec },
        { "baseAttackDamage", eSummonPolicyParamId::BaseAttackDamage },
        { "attackDamagePerRank", eSummonPolicyParamId::AttackDamagePerRank },
        { "baseHp", eSummonPolicyParamId::BaseHp },
        { "hpPerRank", eSummonPolicyParamId::HpPerRank },
        { "radius", eSummonPolicyParamId::Radius },
        { "roleType", eSummonPolicyParamId::RoleType },
        { "lane", eSummonPolicyParamId::Lane },
    };

    // champions.json "stats" 필드명 -> ChampionStatBlock 멤버 (전 20필드).
    struct ChampionStatFieldName
    {
        const char* name;
        f32_t ChampionStatBlock::* member;
    };

    constexpr ChampionStatFieldName kChampionStatFields[] =
    {
        { "baseHp", &ChampionStatBlock::baseHp },
        { "hpPerLevel", &ChampionStatBlock::hpPerLevel },
        { "baseMana", &ChampionStatBlock::baseMana },
        { "manaPerLevel", &ChampionStatBlock::manaPerLevel },
        { "baseAd", &ChampionStatBlock::baseAd },
        { "adPerLevel", &ChampionStatBlock::adPerLevel },
        { "baseAp", &ChampionStatBlock::baseAp },
        { "apPerLevel", &ChampionStatBlock::apPerLevel },
        { "baseArmor", &ChampionStatBlock::baseArmor },
        { "armorPerLevel", &ChampionStatBlock::armorPerLevel },
        { "baseMr", &ChampionStatBlock::baseMr },
        { "mrPerLevel", &ChampionStatBlock::mrPerLevel },
        { "baseAttackSpeed", &ChampionStatBlock::baseAttackSpeed },
        { "attackSpeedRatio", &ChampionStatBlock::attackSpeedRatio },
        { "attackSpeedPerLevel", &ChampionStatBlock::attackSpeedPerLevel },
        { "baseAttackRange", &ChampionStatBlock::baseAttackRange },
        { "baseMoveSpeed", &ChampionStatBlock::baseMoveSpeed },
        { "navArriveRadius", &ChampionStatBlock::navArriveRadius },
        { "spatialRadius", &ChampionStatBlock::spatialRadius },
        { "sightRange", &ChampionStatBlock::sightRange },
    };

    // 코드젠 validate_skill_effect_param_domain 과 동일 도메인.
    bool_t IsValidSkillEffectValue(eSkillEffectParamId id, f32_t value)
    {
        if (!std::isfinite(value))
            return false;
        if (id == eSkillEffectParamId::HalfAngleCos)
            return value >= -1.f && value <= 1.f;
        return value >= 0.f && value <= 1000000.f;
    }

    // 코드젠 validate_summon_policy_param_domain 과 동일 도메인.
    bool_t IsValidSummonPolicyValue(eSummonPolicyParamId id, f32_t value)
    {
        if (!std::isfinite(value) || value < 0.f || value > 1000000.f)
            return false;
        if (id == eSummonPolicyParamId::RoleType || id == eSummonPolicyParamId::Lane)
            return value <= 255.f && std::floor(value) == value;
        return true;
    }

    // 필드가 있으면 도메인 검증 후 덮어쓰고, 없으면 소성값 유지.
    bool_t TryOverlayNumber(
        const json& node,
        const char* field,
        f32_t minValue,
        f32_t maxValue,
        f32_t& outTarget,
        std::string& outError)
    {
        if (!node.contains(field))
            return true;
        if (!node[field].is_number())
        {
            outError = std::string(field) + " must be a number";
            return false;
        }
        const f32_t value = node[field].get<f32_t>();
        if (!std::isfinite(value) || value < minValue || value > maxValue)
        {
            outError = std::string(field) + " out of range";
            return false;
        }
        outTarget = value;
        return true;
    }

    template <size_t N>
    bool_t TryOverlayRankValues(
        const json& node,
        const char* arrayField,
        const char* scalarField,
        u8_t expectedCount,
        f32_t maxValue,
        f32_t (&outValues)[N],
        std::string& outError)
    {
        if (expectedCount == 0u || expectedCount > N)
        {
            outError = std::string(arrayField) + " invalid rank count";
            return false;
        }

        if (node.contains(arrayField))
        {
            const json& values = node[arrayField];
            if (!values.is_array() || values.size() != expectedCount)
            {
                outError = std::string(arrayField) + " rank count mismatch";
                return false;
            }
            for (u8_t index = 0u; index < expectedCount; ++index)
            {
                if (!values[index].is_number())
                {
                    outError = std::string(arrayField) + " contains a non-number";
                    return false;
                }
                const f32_t value = values[index].get<f32_t>();
                if (!std::isfinite(value) || value < 0.f || value > maxValue)
                {
                    outError = std::string(arrayField) + " value out of range";
                    return false;
                }
                outValues[index] = value;
            }
            return true;
        }

        if (!node.contains(scalarField) || !node[scalarField].is_number())
        {
            outError = std::string(arrayField) + " or " + scalarField + " missing";
            return false;
        }
        const f32_t value = node[scalarField].get<f32_t>();
        if (!std::isfinite(value) || value < 0.f || value > maxValue)
        {
            outError = std::string(scalarField) + " out of range";
            return false;
        }
        for (u8_t index = 0u; index < expectedCount; ++index)
            outValues[index] = value;
        return true;
    }

    bool_t HasOnlyKnownKeys(
        const json& object,
        const char* const* knownKeys,
        std::size_t knownKeyCount,
        std::string& outUnknownKey);

    bool_t TryParseDamageFormula(
        const json& node,
        const std::string& path,
        DamageFormulaDef& outFormula,
        std::string& outError)
    {
        if (!node.is_object())
        {
            outError = path + " must be an object";
            return false;
        }

        static constexpr const char* kKnownKeys[] =
        {
            "type", "flags", "flatByRank", "totalAdRatioByRank",
            "bonusAdRatioByRank", "apRatioByRank", "targetMaxHpRatioByRank",
            "targetMissingHpRatioByRank",
        };
        std::string unknownKey;
        if (!HasOnlyKnownKeys(node, kKnownKeys,
                sizeof(kKnownKeys) / sizeof(kKnownKeys[0]), unknownKey))
        {
            outError = path + " unknown field: " + unknownKey;
            return false;
        }

        const std::string type = node.value("type", std::string{});
        eDamageType damageType = eDamageType::Physical;
        if (type == "Magic")
            damageType = eDamageType::Magic;
        else if (type == "True")
            damageType = eDamageType::True;
        else if (type != "Physical")
        {
            outError = path + ".type invalid";
            return false;
        }

        if (!node.contains("flags") || !node["flags"].is_array())
        {
            outError = path + ".flags must be an array";
            return false;
        }
        u32_t flags = DamageFlag_None;
        for (const json& flagNode : node["flags"])
        {
            if (!flagNode.is_string())
            {
                outError = path + ".flags contains a non-string";
                return false;
            }
            const std::string flag = flagNode.get<std::string>();
            if (flag == "CanCrit")
                flags |= DamageFlag_CanCrit;
            else if (flag == "CanLifesteal")
                flags |= DamageFlag_CanLifesteal;
            else if (flag == "OnHit")
                flags |= DamageFlag_OnHit;
            else
            {
                outError = path + ".flags invalid value: " + flag;
                return false;
            }
        }

        struct RankedField
        {
            const char* name;
            f32_t (DamageFormulaDef::*values)[DamageFormulaDef::kMaxRank];
        };
        static constexpr RankedField kFields[] =
        {
            { "flatByRank", &DamageFormulaDef::flatByRank },
            { "totalAdRatioByRank", &DamageFormulaDef::totalAdRatioByRank },
            { "bonusAdRatioByRank", &DamageFormulaDef::bonusAdRatioByRank },
            { "apRatioByRank", &DamageFormulaDef::apRatioByRank },
            { "targetMaxHpRatioByRank", &DamageFormulaDef::targetMaxHpRatioByRank },
            { "targetMissingHpRatioByRank", &DamageFormulaDef::targetMissingHpRatioByRank },
        };

        DamageFormulaDef parsed{};
        u8_t rankCount = 0u;
        for (const RankedField& field : kFields)
        {
            if (!node.contains(field.name) || !node[field.name].is_array())
            {
                outError = path + "." + field.name + " must be an array";
                return false;
            }
            const json& values = node[field.name];
            if (values.empty() || values.size() > DamageFormulaDef::kMaxRank)
            {
                outError = path + "." + field.name + " rank count out of range";
                return false;
            }
            if (rankCount == 0u)
                rankCount = static_cast<u8_t>(values.size());
            else if (values.size() != rankCount)
            {
                outError = path + "." + field.name + " rank count mismatch";
                return false;
            }
            f32_t* target = parsed.*(field.values);
            for (u8_t index = 0u; index < rankCount; ++index)
            {
                if (!values[index].is_number())
                {
                    outError = path + "." + field.name + " contains a non-number";
                    return false;
                }
                const f32_t value = values[index].get<f32_t>();
                if (!std::isfinite(value) || value < -1000000.f || value > 1000000.f)
                {
                    outError = path + "." + field.name + " value out of range";
                    return false;
                }
                target[index] = value;
            }
        }

        parsed.bValid = true;
        parsed.rankCount = rankCount;
        parsed.type = damageType;
        parsed.flags = flags;
        outFormula = parsed;
        return true;
    }

    // 필드가 있으면 정수 도메인 검증 후 덮어쓰고, 없으면 소성값 유지.
    bool_t TryOverlayUnsigned(
        const json& node,
        const char* field,
        u64_t minValue,
        u64_t maxValue,
        u64_t& outTarget,
        std::string& outError)
    {
        if (!node.contains(field))
            return true;
        if (!node[field].is_number_unsigned())
        {
            outError = std::string(field) + " must be an unsigned integer";
            return false;
        }
        const u64_t value = node[field].get<u64_t>();
        if (value < minValue || value > maxValue)
        {
            outError = std::string(field) + " out of range";
            return false;
        }
        outTarget = value;
        return true;
    }

    bool_t TryOverlayUnsigned32(
        const json& node,
        const char* field,
        u64_t minValue,
        u64_t maxValue,
        u32_t& outTarget,
        std::string& outError)
    {
        u64_t value = outTarget;
        if (!TryOverlayUnsigned(node, field, minValue, maxValue, value, outError))
            return false;
        outTarget = static_cast<u32_t>(value);
        return true;
    }

    bool_t TryOverlayUnsigned16(
        const json& node,
        const char* field,
        u64_t minValue,
        u64_t maxValue,
        u16_t& outTarget,
        std::string& outError)
    {
        u64_t value = outTarget;
        if (!TryOverlayUnsigned(node, field, minValue, maxValue, value, outError))
            return false;
        outTarget = static_cast<u16_t>(value);
        return true;
    }

    bool_t TryOverlayUnsigned8(
        const json& node,
        const char* field,
        u64_t minValue,
        u64_t maxValue,
        u8_t& outTarget,
        std::string& outError)
    {
        u64_t value = outTarget;
        if (!TryOverlayUnsigned(node, field, minValue, maxValue, value, outError))
            return false;
        outTarget = static_cast<u8_t>(value);
        return true;
    }

    bool_t ApplyChampionsJson(
        const json& root,
        RuntimePackStorage& storage,
        std::string& outError)
    {
        if (!root.contains("champions") || !root["champions"].is_array())
        {
            outError = "champions[] missing";
            return false;
        }

        for (const json& entry : root["champions"])
        {
            const std::string name = entry.value("champion", std::string{});
            if (name.empty())
            {
                outError = "champion name missing";
                return false;
            }

            const DefinitionKey key = Fnv1a32("champion." + ToLowerAscii(name));
            ChampionGameplayDef* pChampion = FindChampionMutable(storage.champions, key);
            if (!pChampion)
            {
                // 신규 챔피언 추가는 구조 변경 = 코드젠+리빌드 소관.
                outError = "unknown champion (run codegen + rebuild first): " + name;
                return false;
            }

            if (entry.contains("stats"))
            {
                const json& stats = entry["stats"];
                if (!stats.is_object())
                {
                    outError = name + ".stats must be an object";
                    return false;
                }
                for (const auto& item : stats.items())
                {
                    const ChampionStatFieldName* pField = nullptr;
                    for (const ChampionStatFieldName& candidate : kChampionStatFields)
                    {
                        if (item.key() == candidate.name)
                        {
                            pField = &candidate;
                            break;
                        }
                    }
                    if (!pField)
                    {
                        outError = name + ".stats unknown field: " + item.key();
                        return false;
                    }
                    if (!item.value().is_number())
                    {
                        outError = name + ".stats." + item.key() + " must be a number";
                        return false;
                    }
                    const f32_t value = item.value().get<f32_t>();
                    if (!std::isfinite(value) || value < 0.f || value > 1000000.f)
                    {
                        outError = name + ".stats." + item.key() + " out of range";
                        return false;
                    }
                    pChampion->stats.*(pField->member) = value;
                }
            }

            if (!entry.contains("skills") || !entry["skills"].is_array())
                continue;

            for (const json& skillEntry : entry["skills"])
            {
                if (!skillEntry.contains("slot") || !skillEntry["slot"].is_number_unsigned())
                {
                    outError = name + ".skills slot missing";
                    return false;
                }
                const u32_t slot = skillEntry["slot"].get<u32_t>();
                if (slot >= kChampionSkillSlotCount)
                {
                    outError = name + ".skills slot out of range";
                    return false;
                }
                SkillGameplayDef* pSkill =
                    FindSkillMutableById(storage.skills, pChampion->skillLoadout[slot]);
                if (!pSkill)
                {
                    outError = name + " skill loadout miss (run codegen + rebuild first)";
                    return false;
                }

                // 수치 필드만 오버레이. 구조 필드(targetMode/stageCount/skillId 등)는 무시 = 코드젠 소관.
                std::string fieldError;
                const u8_t expectedRankCount = slot == static_cast<u32_t>(eSkillSlot::BasicAttack)
                    ? 1u
                    : (slot == static_cast<u32_t>(eSkillSlot::R) ? 3u : 5u);
                if (!TryOverlayRankValues(
                        skillEntry, "cooldownSecByRank", "cooldownSec",
                        expectedRankCount, 3600.f,
                        pSkill->cooldown.cooldownSecByRank, fieldError) ||
                    !TryOverlayRankValues(
                        skillEntry, "manaCostByRank", "manaCost",
                        expectedRankCount, 1000000.f,
                        pSkill->cost.manaCostByRank, fieldError) ||
                    !TryOverlayNumber(skillEntry, "rangeMax", 0.f, 1000000.f, pSkill->range.rangeMax, fieldError) ||
                    !TryOverlayNumber(skillEntry, "stageWindowSec", 0.f, 60.f, pSkill->stage.stageWindowSec, fieldError))
                {
                    outError = name + ".skills[" + std::to_string(slot) + "] " + fieldError;
                    return false;
                }
                pSkill->cooldown.rankCount = expectedRankCount;
                pSkill->cost.rankCount = expectedRankCount;
                pSkill->cooldown.cooldownSec = pSkill->cooldown.cooldownSecByRank[0];
                pSkill->cost.manaCost = pSkill->cost.manaCostByRank[0];

                if (skillEntry.contains("stages") && skillEntry["stages"].is_array())
                {
                    const json& stages = skillEntry["stages"];
                    for (u8_t stageIndex = 0;
                        stageIndex < stages.size() && stageIndex < kSkillAtomStageMax;
                        ++stageIndex)
                    {
                        if (!TryOverlayNumber(stages[stageIndex], "lockDurationSec", 0.f, 10.f,
                            pSkill->stage.lockDurationSec[stageIndex], fieldError))
                        {
                            outError = name + ".skills[" + std::to_string(slot) + "].stages " + fieldError;
                            return false;
                        }
                    }
                }
            }
        }

        return true;
    }

    bool_t ApplySkillEffectsJson(
        const json& root,
        RuntimePackStorage& storage,
        std::string& outError)
    {
        if (!root.contains("skillEffects") || !root["skillEffects"].is_array())
        {
            outError = "skillEffects[] missing";
            return false;
        }

        for (const json& entry : root["skillEffects"])
        {
            const std::string key = entry.value("key", std::string{});
            if (key.empty())
            {
                outError = "skillEffects entry key missing";
                return false;
            }
            SkillGameplayDef* pSkill =
                FindSkillMutableByKey(storage.skills, Fnv1a32(key));
            if (!pSkill)
            {
                outError = "unknown skill key (run codegen + rebuild first): " + key;
                return false;
            }

            if (entry.contains("params"))
            {
                const json& params = entry["params"];
                if (!params.is_object())
                {
                    outError = key + ".params must be an object";
                    return false;
                }

                // scalingTableId / gameplayPolicyId / replicatedCueId 는 유지, 파라미터만 재구성.
                SkillEffectSpec effect = pSkill->effect;
                effect.paramCount = 0;
                for (const auto& item : params.items())
                {
                    eSkillEffectParamId paramId = eSkillEffectParamId::None;
                    for (const SkillEffectParamName& candidate : kSkillEffectParamNames)
                    {
                        if (item.key() == candidate.name)
                        {
                            paramId = candidate.id;
                            break;
                        }
                    }
                    if (paramId == eSkillEffectParamId::None)
                    {
                        outError = key + ".params unknown id: " + item.key();
                        return false;
                    }
                    if (!item.value().is_number())
                    {
                        outError = key + ".params." + item.key() + " must be a number";
                        return false;
                    }
                    const f32_t value = item.value().get<f32_t>();
                    if (!IsValidSkillEffectValue(paramId, value))
                    {
                        outError = key + ".params." + item.key() + " out of domain";
                        return false;
                    }
                    if (effect.paramCount >= kSkillEffectParamMax)
                    {
                        outError = key + " has too many params";
                        return false;
                    }
                    effect.params[effect.paramCount].id = paramId;
                    effect.params[effect.paramCount].value = value;
                    ++effect.paramCount;
                }
                pSkill->effect = effect;
            }

            if (entry.contains("summonPolicy"))
            {
                const json& summon = entry["summonPolicy"];
                if (!summon.is_object())
                {
                    outError = key + ".summonPolicy must be an object";
                    return false;
                }

                SummonPolicySpec policy{};
                for (const auto& item : summon.items())
                {
                    eSummonPolicyParamId paramId = eSummonPolicyParamId::None;
                    for (const SummonPolicyParamName& candidate : kSummonPolicyParamNames)
                    {
                        if (item.key() == candidate.name)
                        {
                            paramId = candidate.id;
                            break;
                        }
                    }
                    if (paramId == eSummonPolicyParamId::None)
                    {
                        outError = key + ".summonPolicy unknown id: " + item.key();
                        return false;
                    }
                    if (!item.value().is_number())
                    {
                        outError = key + ".summonPolicy." + item.key() + " must be a number";
                        return false;
                    }
                    const f32_t value = item.value().get<f32_t>();
                    if (!IsValidSummonPolicyValue(paramId, value))
                    {
                        outError = key + ".summonPolicy." + item.key() + " out of domain";
                        return false;
                    }
                    if (policy.paramCount >= kSummonPolicyParamMax)
                    {
                        outError = key + " has too many summon params";
                        return false;
                    }
                    policy.params[policy.paramCount].id = paramId;
                    policy.params[policy.paramCount].value = value;
                    ++policy.paramCount;
                }
                policy.bValid = policy.paramCount > 0;
                pSkill->summonPolicy = policy;
            }

            if (!entry.contains("damage"))
            {
                outError = key + ".damage missing";
                return false;
            }
            DamageFormulaDef damage{};
            if (!TryParseDamageFormula(entry["damage"], key + ".damage", damage, outError))
                return false;
            pSkill->effect.damage = damage;
        }

        return true;
    }

    bool_t ApplySummonerSpellsJson(
        const json& root,
        RuntimePackStorage& storage,
        std::string& outError)
    {
        if (!root.contains("summonerSpells") || !root["summonerSpells"].is_array())
        {
            outError = "summonerSpells[] missing";
            return false;
        }

        for (const json& entry : root["summonerSpells"])
        {
            const std::string key = entry.value("key", std::string{});
            if (key.empty())
            {
                outError = "summonerSpells entry key missing";
                return false;
            }
            SummonerSpellGameplayDef* pSpell =
                FindSummonerSpellMutable(storage.summonerSpells, Fnv1a32(key));
            if (!pSpell)
            {
                outError = "unknown summoner spell key: " + key;
                return false;
            }

            std::string fieldError;
            if (!TryOverlayNumber(entry, "rangeMax", 0.f, 1000000.f, pSpell->rangeMax, fieldError) ||
                !TryOverlayNumber(entry, "cooldownSec", 0.f, 1000000.f, pSpell->cooldownSec, fieldError))
            {
                outError = key + " " + fieldError;
                return false;
            }
        }

        return true;
    }

    // EconomyGameplayDefs.json 필드명 -> 경제 정의 멤버 (그룹별 1:1).
    template <typename TDef>
    struct EconomyFieldName
    {
        const char* name;
        f32_t TDef::* member;
    };

    constexpr EconomyFieldName<EconomyChampionKillRewardDef> kEconomyChampionKillFields[] =
    {
        { "killerGold", &EconomyChampionKillRewardDef::killerGold },
        { "assistGold", &EconomyChampionKillRewardDef::assistGold },
        { "firstBloodBonusGold", &EconomyChampionKillRewardDef::firstBloodBonusGold },
        { "victimNextLevelXPFactor", &EconomyChampionKillRewardDef::victimNextLevelXPFactor },
        { "shareRadius", &EconomyChampionKillRewardDef::shareRadius },
    };

    constexpr EconomyFieldName<EconomyMinionRewardDef> kEconomyMinionFields[] =
    {
        { "soloXP", &EconomyMinionRewardDef::soloXP },
        { "sharedXP", &EconomyMinionRewardDef::sharedXP },
        { "gold", &EconomyMinionRewardDef::gold },
        { "maxGold", &EconomyMinionRewardDef::maxGold },
        { "growthAmount", &EconomyMinionRewardDef::growthAmount },
        { "growthIntervalSec", &EconomyMinionRewardDef::growthIntervalSec },
    };

    constexpr EconomyFieldName<EconomyJungleRewardDef> kEconomyJungleFields[] =
    {
        { "smallCampGold", &EconomyJungleRewardDef::smallCampGold },
        { "smallCampXP", &EconomyJungleRewardDef::smallCampXP },
        { "epicGold", &EconomyJungleRewardDef::epicGold },
        { "epicXP", &EconomyJungleRewardDef::epicXP },
        { "baronGold", &EconomyJungleRewardDef::baronGold },
        { "baronXP", &EconomyJungleRewardDef::baronXP },
    };

    // 존재 필드만 덮어쓰되 미지 키는 에러 (champions.json stats 오버레이와 동일 규약).
    template <typename TDef, std::size_t N>
    bool_t OverlayEconomyGroup(
        const json& node,
        const std::string& groupName,
        const EconomyFieldName<TDef> (&fields)[N],
        TDef& outDef,
        std::string& outError)
    {
        if (!node.is_object())
        {
            outError = groupName + " must be an object";
            return false;
        }
        for (const auto& item : node.items())
        {
            const EconomyFieldName<TDef>* pField = nullptr;
            for (const EconomyFieldName<TDef>& candidate : fields)
            {
                if (item.key() == candidate.name)
                {
                    pField = &candidate;
                    break;
                }
            }
            if (!pField)
            {
                outError = groupName + " unknown field: " + item.key();
                return false;
            }
            if (!item.value().is_number())
            {
                outError = groupName + "." + item.key() + " must be a number";
                return false;
            }
            const f32_t value = item.value().get<f32_t>();
            if (!std::isfinite(value) || value < 0.f || value > 1000000.f)
            {
                outError = groupName + "." + item.key() + " out of range";
                return false;
            }
            outDef.*(pField->member) = value;
        }
        return true;
    }

    bool_t HasOnlyKnownKeys(
        const json& node,
        const char* const* knownKeys,
        std::size_t knownKeyCount,
        std::string& outUnknownKey)
    {
        for (const auto& item : node.items())
        {
            bool_t bKnown = false;
            for (std::size_t index = 0; index < knownKeyCount; ++index)
            {
                if (item.key() == knownKeys[index])
                {
                    bKnown = true;
                    break;
                }
            }
            if (!bKnown)
            {
                outUnknownKey = item.key();
                return false;
            }
        }
        return true;
    }

    bool_t ApplyEconomyJson(
        const json& root,
        RuntimePackStorage& storage,
        std::string& outError)
    {
        EconomyGameplayDef& economy = storage.economyStorage;

        static constexpr const char* kKnownRootKeys[] =
        {
            "schemaVersion", "dataVersion", "buildHash",
            "xpCurve", "championKill", "minions", "turretGold",
            "jungle", "passiveGold", "timers",
        };
        std::string unknownKey;
        if (!HasOnlyKnownKeys(root, kKnownRootKeys,
            sizeof(kKnownRootKeys) / sizeof(kKnownRootKeys[0]), unknownKey))
        {
            outError = "unknown field: " + unknownKey;
            return false;
        }

        if (root.contains("xpCurve"))
        {
            const json& curve = root["xpCurve"];
            constexpr std::size_t kCurveLength =
                ChampionExperienceCurveDef::kMaxChampionLevel - 1u;  // 레벨 1..17
            if (!curve.is_array() || curve.size() != kCurveLength)
            {
                outError = "xpCurve must be an array of 17 numbers";
                return false;
            }
            for (std::size_t index = 0; index < kCurveLength; ++index)
            {
                if (!curve[index].is_number())
                {
                    outError = "xpCurve entries must be numbers";
                    return false;
                }
                const f32_t value = curve[index].get<f32_t>();
                if (!std::isfinite(value) || value < 0.f || value > 1000000.f)
                {
                    outError = "xpCurve value out of range";
                    return false;
                }
                economy.xpRequiredForNextLevel[index + 1u] = value;
            }
        }

        if (root.contains("championKill") &&
            !OverlayEconomyGroup(root["championKill"], "championKill",
                kEconomyChampionKillFields, economy.championKill, outError))
        {
            return false;
        }

        if (root.contains("minions"))
        {
            const json& minions = root["minions"];
            if (!minions.is_object())
            {
                outError = "minions must be an object";
                return false;
            }
            struct MinionGroup
            {
                const char* name;
                EconomyMinionRewardDef* target;
            };
            const MinionGroup groups[] =
            {
                { "melee", &economy.melee },
                { "ranged", &economy.ranged },
                { "siege", &economy.siege },
                { "super", &economy.super },
            };
            for (const auto& item : minions.items())
            {
                const MinionGroup* pGroup = nullptr;
                for (const MinionGroup& candidate : groups)
                {
                    if (item.key() == candidate.name)
                    {
                        pGroup = &candidate;
                        break;
                    }
                }
                if (!pGroup)
                {
                    outError = "minions unknown field: " + item.key();
                    return false;
                }
                if (!OverlayEconomyGroup(item.value(), "minions." + item.key(),
                    kEconomyMinionFields, *pGroup->target, outError))
                {
                    return false;
                }
            }
        }

        std::string fieldError;
        if (!TryOverlayNumber(root, "turretGold", 0.f, 1000000.f, economy.turretGold, fieldError))
        {
            outError = fieldError;
            return false;
        }

        if (root.contains("jungle") &&
            !OverlayEconomyGroup(root["jungle"], "jungle",
                kEconomyJungleFields, economy.jungle, outError))
        {
            return false;
        }

        if (root.contains("passiveGold"))
        {
            const json& passive = root["passiveGold"];
            static constexpr const char* kKnownPassiveKeys[] =
            {
                "startTick", "intervalTicks", "perGrant",
            };
            if (!passive.is_object() ||
                !HasOnlyKnownKeys(passive, kKnownPassiveKeys,
                    sizeof(kKnownPassiveKeys) / sizeof(kKnownPassiveKeys[0]), unknownKey))
            {
                outError = passive.is_object()
                    ? "passiveGold unknown field: " + unknownKey
                    : "passiveGold must be an object";
                return false;
            }
            if (!TryOverlayUnsigned(passive, "startTick", 0ull, 1000000ull,
                    economy.passiveGoldStartTick, fieldError) ||
                !TryOverlayUnsigned(passive, "intervalTicks", 1ull, 1000000ull,
                    economy.passiveGoldIntervalTicks, fieldError) ||
                !TryOverlayUnsigned32(passive, "perGrant", 0ull, 1000000ull,
                    economy.passiveGoldPerGrant, fieldError))
            {
                outError = "passiveGold " + fieldError;
                return false;
            }
        }

        if (root.contains("timers"))
        {
            const json& timers = root["timers"];
            static constexpr const char* kKnownTimerKeys[] =
            {
                "assistCreditWindowSec", "recallDurationSec",
            };
            if (!timers.is_object() ||
                !HasOnlyKnownKeys(timers, kKnownTimerKeys,
                    sizeof(kKnownTimerKeys) / sizeof(kKnownTimerKeys[0]), unknownKey))
            {
                outError = timers.is_object()
                    ? "timers unknown field: " + unknownKey
                    : "timers must be an object";
                return false;
            }
            if (!TryOverlayNumber(timers, "assistCreditWindowSec", 0.f, 1000000.f,
                    economy.assistCreditWindowSec, fieldError) ||
                !TryOverlayNumber(timers, "recallDurationSec", 0.f, 1000000.f,
                    economy.recallDurationSec, fieldError))
            {
                outError = "timers " + fieldError;
                return false;
            }
        }

        return true;
    }

    // ItemGameplayDefs.json stats 필드명 -> ItemStatModifier 멤버 (코드젠 ITEM_STAT_FIELDS 와 1:1).
    struct ItemStatFieldName
    {
        const char* name;
        f32_t ItemStatModifier::* member;
    };

    constexpr ItemStatFieldName kItemStatFields[] =
    {
        { "flatAd", &ItemStatModifier::flatAd },
        { "flatAp", &ItemStatModifier::flatAp },
        { "flatHealth", &ItemStatModifier::flatHealth },
        { "flatMana", &ItemStatModifier::flatMana },
        { "flatArmor", &ItemStatModifier::flatArmor },
        { "flatMr", &ItemStatModifier::flatMr },
        { "bonusAttackSpeed", &ItemStatModifier::bonusAttackSpeed },
        { "critChance", &ItemStatModifier::critChance },
        { "abilityHaste", &ItemStatModifier::abilityHaste },
        { "flatMoveSpeed", &ItemStatModifier::flatMoveSpeed },
        { "lifeSteal", &ItemStatModifier::lifeSteal },
        { "flatMagicPen", &ItemStatModifier::flatMagicPen },
        { "lethality", &ItemStatModifier::lethality },
    };

    // 아이템 스탯 도메인: 음수 허용 (코드젠 validate_item_stat_number 와 동일).
    bool_t IsValidItemStatValue(f32_t value)
    {
        return std::isfinite(value) && value >= -1000000.f && value <= 1000000.f;
    }

    bool_t ApplyItemsJson(
        const json& root,
        RuntimePackStorage& storage,
        std::string& outError)
    {
        // 코드젠 팩에 아이템 정의가 없으면 오버레이도 생략 = 컴파일된 기본 표 폴백 유지.
        if (storage.items.empty())
            return true;

        static constexpr const char* kKnownRootKeys[] =
        {
            "schemaVersion", "dataVersion", "buildHash", "items",
        };
        std::string unknownKey;
        if (!HasOnlyKnownKeys(root, kKnownRootKeys,
            sizeof(kKnownRootKeys) / sizeof(kKnownRootKeys[0]), unknownKey))
        {
            outError = "unknown field: " + unknownKey;
            return false;
        }

        if (!root.contains("items") || !root["items"].is_array())
        {
            outError = "items[] missing";
            return false;
        }

        for (const json& entry : root["items"])
        {
            if (!entry.is_object())
            {
                outError = "items entry must be an object";
                return false;
            }
            // name 은 구조(표시) 소관이라 허용하되 무시한다.
            static constexpr const char* kKnownItemKeys[] =
            {
                "itemId", "price", "stats", "onHitDamage", "name",
            };
            if (!HasOnlyKnownKeys(entry, kKnownItemKeys,
                sizeof(kKnownItemKeys) / sizeof(kKnownItemKeys[0]), unknownKey))
            {
                outError = "items unknown field: " + unknownKey;
                return false;
            }
            if (!entry.contains("itemId") || !entry["itemId"].is_number_unsigned())
            {
                outError = "items entry itemId missing";
                return false;
            }
            const u64_t itemId = entry["itemId"].get<u64_t>();
            ItemDef* pItem = nullptr;
            for (ItemDef& item : storage.items)
            {
                if (item.itemId == itemId)
                {
                    pItem = &item;
                    break;
                }
            }
            if (!pItem)
            {
                // 신규 아이템 추가는 구조 변경 = 코드젠+리빌드 소관.
                outError = "unknown itemId (run codegen + rebuild first): "
                    + std::to_string(itemId);
                return false;
            }

            std::string fieldError;
            if (!TryOverlayUnsigned16(entry, "price", 0ull, 65535ull, pItem->price, fieldError))
            {
                outError = "items[" + std::to_string(itemId) + "] " + fieldError;
                return false;
            }

            if (entry.contains("stats"))
            {
                const json& stats = entry["stats"];
                if (!stats.is_object())
                {
                    outError = "items[" + std::to_string(itemId) + "].stats must be an object";
                    return false;
                }
                for (const auto& item : stats.items())
                {
                    const ItemStatFieldName* pField = nullptr;
                    for (const ItemStatFieldName& candidate : kItemStatFields)
                    {
                        if (item.key() == candidate.name)
                        {
                            pField = &candidate;
                            break;
                        }
                    }
                    if (!pField)
                    {
                        outError = "items[" + std::to_string(itemId)
                            + "].stats unknown field: " + item.key();
                        return false;
                    }
                    if (!item.value().is_number())
                    {
                        outError = "items[" + std::to_string(itemId)
                            + "].stats." + item.key() + " must be a number";
                        return false;
                    }
                    const f32_t value = item.value().get<f32_t>();
                    if (!IsValidItemStatValue(value))
                    {
                        outError = "items[" + std::to_string(itemId)
                            + "].stats." + item.key() + " out of range";
                        return false;
                    }
                    pItem->stats.*(pField->member) = value;
                }
            }
            if (entry.contains("onHitDamage"))
            {
                DamageFormulaDef damage{};
                if (!TryParseDamageFormula(
                        entry["onHitDamage"],
                        "items[" + std::to_string(itemId) + "].onHitDamage",
                        damage,
                        outError))
                {
                    return false;
                }
                pItem->onHitDamage = damage;
            }
        }

        return true;
    }

    // SpawnObjectGameplayDefs.json 수치 필드 -> 정의 멤버 (코드젠 *_FIELDS 와 1:1).
    constexpr EconomyFieldName<ChampionColliderProfileDef> kSpawnChampionColliderFields[] =
    {
        { "bodyHeight", &ChampionColliderProfileDef::bodyHeight },
        { "bodyOffsetY", &ChampionColliderProfileDef::bodyOffsetY },
    };

    constexpr EconomyFieldName<TurretAIGameDef> kSpawnTurretAIFields[] =
    {
        { "attackRange", &TurretAIGameDef::attackRange },
        { "attackCooldownMax", &TurretAIGameDef::attackCooldownMax },
        { "attackDamage", &TurretAIGameDef::attackDamage },
        { "nexusAttackDamage", &TurretAIGameDef::nexusAttackDamage },
        { "projectileSpeed", &TurretAIGameDef::projectileSpeed },
        { "turretSightRange", &TurretAIGameDef::turretSightRange },
        { "structureSightRange", &TurretAIGameDef::structureSightRange },
        { "bodyHeight", &TurretAIGameDef::bodyHeight },
        { "bodyOffsetY", &TurretAIGameDef::bodyOffsetY },
    };

    constexpr EconomyFieldName<JungleCampGameDef> kSpawnJungleCampFields[] =
    {
        { "maxHp", &JungleCampGameDef::maxHp },
        { "radius", &JungleCampGameDef::radius },
        { "attackRange", &JungleCampGameDef::attackRange },
        { "attackDamage", &JungleCampGameDef::attackDamage },
        { "attackCooldown", &JungleCampGameDef::attackCooldown },
        { "moveSpeed", &JungleCampGameDef::moveSpeed },
        { "baseArmor", &JungleCampGameDef::baseArmor },
        { "baseMr", &JungleCampGameDef::baseMr },
        { "aggroRange", &JungleCampGameDef::aggroRange },
        { "leashRange", &JungleCampGameDef::leashRange },
    };

    constexpr EconomyFieldName<MinionCombatDef> kSpawnMinionFields[] =
    {
        { "moveSpeed", &MinionCombatDef::moveSpeed },
        { "attackRange", &MinionCombatDef::attackRange },
        { "sightRange", &MinionCombatDef::sightRange },
        { "attackDamage", &MinionCombatDef::attackDamage },
        { "attackCooldownMax", &MinionCombatDef::attackCooldownMax },
        { "maxHp", &MinionCombatDef::maxHp },
    };

    constexpr EconomyFieldName<MinionWaveRangedProjectileDef> kSpawnMinionWaveProjectileFields[] =
    {
        { "speed", &MinionWaveRangedProjectileDef::speed },
        { "hitRadius", &MinionWaveRangedProjectileDef::hitRadius },
        { "forwardOffset", &MinionWaveRangedProjectileDef::forwardOffset },
        { "spawnHeight", &MinionWaveRangedProjectileDef::spawnHeight },
        { "maxDistancePadding", &MinionWaveRangedProjectileDef::maxDistancePadding },
    };

    // OverlayEconomyGroup 과 동일 규약이되 식별자 키(subKind/roleType)는 건너뛴다.
    template <typename TDef, std::size_t N>
    bool_t OverlaySpawnObjectEntry(
        const json& node,
        const std::string& entryName,
        const char* idField,
        const EconomyFieldName<TDef> (&fields)[N],
        TDef& outDef,
        std::string& outError)
    {
        for (const auto& item : node.items())
        {
            if (item.key() == idField)
                continue;
            const EconomyFieldName<TDef>* pField = nullptr;
            for (const EconomyFieldName<TDef>& candidate : fields)
            {
                if (item.key() == candidate.name)
                {
                    pField = &candidate;
                    break;
                }
            }
            if (!pField)
            {
                outError = entryName + " unknown field: " + item.key();
                return false;
            }
            if (!item.value().is_number())
            {
                outError = entryName + "." + item.key() + " must be a number";
                return false;
            }
            const f32_t value = item.value().get<f32_t>();
            if (!std::isfinite(value) || value < 0.f || value > 1000000.f)
            {
                outError = entryName + "." + item.key() + " out of range";
                return false;
            }
            outDef.*(pField->member) = value;
        }
        return true;
    }

    bool_t ApplySpawnObjectJson(
        const json& root,
        RuntimePackStorage& storage,
        std::string& outError)
    {
        static constexpr const char* kKnownRootKeys[] =
        {
            "schemaVersion", "dataVersion", "buildHash",
            "spawnLoadout", "championCollider", "structure",
            "defaultJungleCamp", "jungleCamps", "defaultMinion",
            "minions", "minionWave",
        };
        std::string unknownKey;
        if (!HasOnlyKnownKeys(root, kKnownRootKeys,
            sizeof(kKnownRootKeys) / sizeof(kKnownRootKeys[0]), unknownKey))
        {
            outError = "unknown field: " + unknownKey;
            return false;
        }

        SpawnObjectDefinitionPack& pack = storage.spawnPack;
        std::string fieldError;

        if (root.contains("spawnLoadout"))
        {
            const json& loadout = root["spawnLoadout"];
            // startRune 은 구조(열거형 이름) 소관이라 허용하되 무시한다.
            static constexpr const char* kKnownLoadoutKeys[] =
            {
                "startGold", "startLevel", "startRune", "startRuneCount", "respawnDelaySec",
            };
            if (!loadout.is_object() ||
                !HasOnlyKnownKeys(loadout, kKnownLoadoutKeys,
                    sizeof(kKnownLoadoutKeys) / sizeof(kKnownLoadoutKeys[0]), unknownKey))
            {
                outError = loadout.is_object()
                    ? "spawnLoadout unknown field: " + unknownKey
                    : "spawnLoadout must be an object";
                return false;
            }
            if (!TryOverlayUnsigned32(loadout, "startGold", 0ull, 1000000ull,
                    pack.spawnLoadout.startGold, fieldError) ||
                !TryOverlayUnsigned8(loadout, "startLevel", 1ull, 18ull,
                    pack.spawnLoadout.startLevel, fieldError) ||
                !TryOverlayUnsigned8(loadout, "startRuneCount", 0ull, 255ull,
                    pack.spawnLoadout.startRuneCount, fieldError) ||
                !TryOverlayNumber(loadout, "respawnDelaySec", 0.f, 1000000.f,
                    pack.spawnLoadout.respawnDelaySec, fieldError))
            {
                outError = "spawnLoadout " + fieldError;
                return false;
            }
        }

        if (root.contains("championCollider") &&
            !OverlayEconomyGroup(root["championCollider"], "championCollider",
                kSpawnChampionColliderFields, pack.championCollider, outError))
        {
            return false;
        }

        if (root.contains("structure"))
        {
            const json& structure = root["structure"];
            static constexpr const char* kKnownStructureKeys[] =
            {
                "turretMaxHp", "inhibitorMaxHp", "nexusMaxHp", "turretAI",
            };
            if (!structure.is_object() ||
                !HasOnlyKnownKeys(structure, kKnownStructureKeys,
                    sizeof(kKnownStructureKeys) / sizeof(kKnownStructureKeys[0]), unknownKey))
            {
                outError = structure.is_object()
                    ? "structure unknown field: " + unknownKey
                    : "structure must be an object";
                return false;
            }
            if (!TryOverlayNumber(structure, "turretMaxHp", 0.f, 1000000.f,
                    pack.structure.turretMaxHp, fieldError) ||
                !TryOverlayNumber(structure, "inhibitorMaxHp", 0.f, 1000000.f,
                    pack.structure.inhibitorMaxHp, fieldError) ||
                !TryOverlayNumber(structure, "nexusMaxHp", 0.f, 1000000.f,
                    pack.structure.nexusMaxHp, fieldError))
            {
                outError = "structure " + fieldError;
                return false;
            }
            if (structure.contains("turretAI") &&
                !OverlayEconomyGroup(structure["turretAI"], "structure.turretAI",
                    kSpawnTurretAIFields, pack.structure.turretAI, outError))
            {
                return false;
            }
        }

        if (root.contains("defaultJungleCamp") &&
            !OverlayEconomyGroup(root["defaultJungleCamp"], "defaultJungleCamp",
                kSpawnJungleCampFields, pack.defaultJungleCamp, outError))
        {
            return false;
        }

        if (root.contains("jungleCamps"))
        {
            const json& camps = root["jungleCamps"];
            if (!camps.is_array())
            {
                outError = "jungleCamps must be an array";
                return false;
            }
            for (const json& entry : camps)
            {
                if (!entry.is_object() ||
                    !entry.contains("subKind") || !entry["subKind"].is_number_unsigned())
                {
                    outError = "jungleCamps entry subKind missing";
                    return false;
                }
                const u64_t subKind = entry["subKind"].get<u64_t>();
                JungleCampGameDefEntry* pCamp = nullptr;
                for (JungleCampGameDefEntry& camp : storage.jungleCamps)
                {
                    if (camp.subKind == subKind)
                    {
                        pCamp = &camp;
                        break;
                    }
                }
                if (!pCamp)
                {
                    // 신규 캠프 추가는 구조 변경 = 코드젠+리빌드 소관.
                    outError = "unknown jungle subKind (run codegen + rebuild first): "
                        + std::to_string(subKind);
                    return false;
                }
                if (!OverlaySpawnObjectEntry(entry,
                    "jungleCamps[" + std::to_string(subKind) + "]", "subKind",
                    kSpawnJungleCampFields, pCamp->value, outError))
                {
                    return false;
                }
            }
        }

        if (root.contains("defaultMinion") &&
            !OverlayEconomyGroup(root["defaultMinion"], "defaultMinion",
                kSpawnMinionFields, pack.defaultMinion, outError))
        {
            return false;
        }

        if (root.contains("minions"))
        {
            const json& minions = root["minions"];
            if (!minions.is_array())
            {
                outError = "minions must be an array";
                return false;
            }
            for (const json& entry : minions)
            {
                if (!entry.is_object() ||
                    !entry.contains("roleType") || !entry["roleType"].is_number_unsigned())
                {
                    outError = "minions entry roleType missing";
                    return false;
                }
                const u64_t roleType = entry["roleType"].get<u64_t>();
                MinionCombatDefEntry* pMinion = nullptr;
                for (MinionCombatDefEntry& minion : storage.minions)
                {
                    if (minion.roleType == roleType)
                    {
                        pMinion = &minion;
                        break;
                    }
                }
                if (!pMinion)
                {
                    // 신규 롤 추가는 구조 변경 = 코드젠+리빌드 소관.
                    outError = "unknown minion roleType (run codegen + rebuild first): "
                        + std::to_string(roleType);
                    return false;
                }
                if (!OverlaySpawnObjectEntry(entry,
                    "minions[" + std::to_string(roleType) + "]", "roleType",
                    kSpawnMinionFields, pMinion->value, outError))
                {
                    return false;
                }
            }
        }

        if (root.contains("minionWave"))
        {
            const json& wave = root["minionWave"];
            static constexpr const char* kKnownWaveKeys[] =
            {
                "waveIntervalTicks", "initialDelayTicks", "perMinionDelayTicks",
                "siegeWavePeriod", "timeGrowthPerMinute", "timeGrowthCapMinutes",
                "rangedProjectile", "corpseDeathTimerSec",
            };
            if (!wave.is_object() ||
                !HasOnlyKnownKeys(wave, kKnownWaveKeys,
                    sizeof(kKnownWaveKeys) / sizeof(kKnownWaveKeys[0]), unknownKey))
            {
                outError = wave.is_object()
                    ? "minionWave unknown field: " + unknownKey
                    : "minionWave must be an object";
                return false;
            }
            if (!TryOverlayUnsigned(wave, "waveIntervalTicks", 1ull, 1000000ull,
                    pack.minionWave.waveIntervalTicks, fieldError) ||
                !TryOverlayUnsigned(wave, "initialDelayTicks", 0ull, 1000000ull,
                    pack.minionWave.initialDelayTicks, fieldError) ||
                !TryOverlayUnsigned(wave, "perMinionDelayTicks", 0ull, 1000000ull,
                    pack.minionWave.perMinionDelayTicks, fieldError) ||
                !TryOverlayUnsigned32(wave, "siegeWavePeriod", 1ull, 1000000ull,
                    pack.minionWave.siegeWavePeriod, fieldError) ||
                !TryOverlayUnsigned32(wave, "timeGrowthCapMinutes", 0ull, 1000000ull,
                    pack.minionWave.timeGrowthCapMinutes, fieldError) ||
                !TryOverlayNumber(wave, "timeGrowthPerMinute", 0.f, 1000000.f,
                    pack.minionWave.timeGrowthPerMinute, fieldError) ||
                !TryOverlayNumber(wave, "corpseDeathTimerSec", 0.f, 1000000.f,
                    pack.minionWave.corpseDeathTimerSec, fieldError))
            {
                outError = "minionWave " + fieldError;
                return false;
            }
            if (wave.contains("rangedProjectile") &&
                !OverlayEconomyGroup(wave["rangedProjectile"], "minionWave.rangedProjectile",
                    kSpawnMinionWaveProjectileFields, pack.minionWave.rangedProjectile, outError))
            {
                return false;
            }
        }

        return true;
    }
#endif
}

namespace ServerData
{
    const GameplayDefinitionPack& GetActiveLoLGameplayDefinitionPack()
    {
        if (const GameplayDefinitionPack* pRuntime =
            g_pActiveRuntimePack.load(std::memory_order_acquire))
        {
            return *pRuntime;
        }
        return GetLoLGameplayDefinitionPack();
    }

    const SpawnObjectDefinitionPack& GetActiveLoLSpawnObjectDefinitionPack()
    {
        if (const SpawnObjectDefinitionPack* pRuntime =
            g_pActiveRuntimeSpawnObjectPack.load(std::memory_order_acquire))
        {
            return *pRuntime;
        }
        return GetLoLSpawnObjectDefinitionPack();
    }

    bool_t TryReloadRuntimeGameplayDefinitions(std::string& outError)
    {
#if !defined(_DEBUG)
        outError = "debug-only";
        return false;
#else
        std::lock_guard<std::mutex> lock(ReloadMutex());

        const GameplayDefinitionPack& base = GetLoLGameplayDefinitionPack();
        auto pStorage = std::make_unique<RuntimePackStorage>();
        pStorage->champions.assign(base.champions, base.champions + base.championCount);
        pStorage->skills.assign(base.skills, base.skills + base.skillCount);
        pStorage->summonerSpells.assign(
            base.summonerSpellDefs, base.summonerSpellDefs + base.summonerSpellCount);
        // 코드젠 팩에 경제 정의가 없으면 기본값(bValid=false)을 유지해 소비처가 상수 폴백을 쓴다.
        if (base.economy)
            pStorage->economyStorage = *base.economy;
        // 코드젠 팩에 아이템 정의가 없으면 비워둬 pack.items = nullptr = 컴파일된 기본 표 폴백.
        if (base.items && base.itemCount > 0u)
            pStorage->items.assign(base.items, base.items + base.itemCount);

        // SpawnObject 팩은 스폰 시점 복사 의미론 — 리로드 후 다음 스폰/웨이브부터 신값(M4).
        const SpawnObjectDefinitionPack& spawnBase = GetLoLSpawnObjectDefinitionPack();
        pStorage->spawnPack = spawnBase;
        pStorage->jungleCamps.assign(
            spawnBase.jungleCamps, spawnBase.jungleCamps + spawnBase.jungleCampCount);
        pStorage->minions.assign(
            spawnBase.minions, spawnBase.minions + spawnBase.minionCount);
        pStorage->spawnPack.jungleCamps = pStorage->jungleCamps.data();
        pStorage->spawnPack.jungleCampCount = pStorage->jungleCamps.size();
        pStorage->spawnPack.minions = pStorage->minions.data();
        pStorage->spawnPack.minionCount = pStorage->minions.size();

        struct SourceFile
        {
            const wchar_t* relative;
            const char* name;
            bool_t (*apply)(const json&, RuntimePackStorage&, std::string&);
        };
        const SourceFile sources[] =
        {
            { L"Data\\Gameplay\\ChampionGameData\\champions.json",
                "champions.json", &ApplyChampionsJson },
            { L"Data\\LoL\\ServerPrivate\\Gameplay\\SkillEffectGameplayDefs.json",
                "SkillEffectGameplayDefs.json", &ApplySkillEffectsJson },
            { L"Data\\LoL\\ServerPrivate\\Gameplay\\SummonerSpellGameplayDefs.json",
                "SummonerSpellGameplayDefs.json", &ApplySummonerSpellsJson },
            { L"Data\\LoL\\ServerPrivate\\Gameplay\\EconomyGameplayDefs.json",
                "EconomyGameplayDefs.json", &ApplyEconomyJson },
            { L"Data\\LoL\\ServerPrivate\\Gameplay\\ItemGameplayDefs.json",
                "ItemGameplayDefs.json", &ApplyItemsJson },
            { L"Data\\LoL\\ServerPrivate\\Gameplay\\SpawnObjectGameplayDefs.json",
                "SpawnObjectGameplayDefs.json", &ApplySpawnObjectJson },
        };

        for (const SourceFile& source : sources)
        {
            std::wstring path;
            if (!ResolveWorkspaceDataFile(source.relative, path))
            {
                outError = std::string(source.name) + ": workspace path not found";
                return false;
            }
            std::string text;
            if (!ReadFileText(path, text))
            {
                outError = std::string(source.name) + ": read failed";
                return false;
            }
            const json root = json::parse(text, nullptr, false);
            if (root.is_discarded())
            {
                outError = std::string(source.name) + ": json parse failed";
                return false;
            }
            std::string applyError;
            if (!source.apply(root, *pStorage, applyError))
            {
                outError = std::string(source.name) + ": " + applyError;
                return false;
            }
        }

        GameplayDefinitionPack& pack = pStorage->pack;
        pack.manifest = base.manifest;
        pack.manifest.uDataVersion =
            base.manifest.uDataVersion + g_runtimeRevision.load(std::memory_order_relaxed) + 1u;
        pack.champions = pStorage->champions.data();
        pack.championCount = pStorage->champions.size();
        pack.skills = pStorage->skills.data();
        pack.skillCount = pStorage->skills.size();
        pack.summonerSpellDefs = pStorage->summonerSpells.data();
        pack.summonerSpellCount = pStorage->summonerSpells.size();
        pack.economy = &pStorage->economyStorage;
        pack.items = pStorage->items.empty() ? nullptr : pStorage->items.data();
        pack.itemCount = pStorage->items.size();

        pStorage->spawnPack.manifest.uDataVersion =
            spawnBase.manifest.uDataVersion +
            g_runtimeRevision.load(std::memory_order_relaxed) + 1u;

        g_pActiveRuntimePack.store(&pack, std::memory_order_release);
        g_pActiveRuntimeSpawnObjectPack.store(
            &pStorage->spawnPack, std::memory_order_release);
        StorageGenerations().push_back(std::move(pStorage));
        g_runtimeRevision.fetch_add(1u, std::memory_order_relaxed);
        return true;
#endif
    }

    void ClearRuntimeGameplayDefinitions()
    {
        g_pActiveRuntimePack.store(nullptr, std::memory_order_release);
        g_pActiveRuntimeSpawnObjectPack.store(nullptr, std::memory_order_release);
    }

    u32_t GetRuntimeGameplayDefinitionRevision()
    {
        return g_runtimeRevision.load(std::memory_order_relaxed);
    }
}
