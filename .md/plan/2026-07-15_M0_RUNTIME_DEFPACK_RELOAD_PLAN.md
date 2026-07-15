Session - Debug 서버에 진실 JSON 3종(champions/SkillEffect/SummonerSpell) 런타임 오버레이 리로드를 넣어 "JSON 수정 → 리로드 op(25) → 즉시 반영 → 크로노 되감기" 기획자 루프를 개통한다 (기본 경로는 소성 팩 그대로 — SimLab 골든 85A270CA375932B7 불변; 배경/결정 근거는 `2026-07-15_FULL_DATA_DRIVEN_BALANCE_MASTER.md` D1~D6).

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Server/Private/Data/ThirdParty/json.hpp

새 파일 (서드파티 단일 헤더 — 전문 게재 생략, 복사로 생성):

```powershell
New-Item -ItemType Directory -Force Server\Private\Data\ThirdParty
Copy-Item Client\Public\Network\Backend\json.hpp Server\Private\Data\ThirdParty\json.hpp
```

nlohmann/json v3.12.0. Client 경계를 넘는 include 금지(2026-07-09 dependency-boundary gotcha)를 지키기 위한 Server 전용 사본. 레포 루트가 Server include 경로에 이미 있어 vcxproj include 경로 수정은 불필요.

1-2. C:/Users/user/Desktop/Winters/Server/Private/Data/RuntimeGameplayDefinitionOverlay.h

새 파일:

```cpp
#pragma once

#include "Shared/GameSim/Definitions/GameplayDefinitionPack.h"

#include <string>

// M0 디자이너 핫리로드 오버레이 (Debug 전용).
// 코드젠 팩(LoLGameplayDefinitions.generated.cpp)의 복사본 위에 진실 JSON 3종
// (champions.json / SkillEffectGameplayDefs.json / SummonerSpellGameplayDefs.json)의
// 수치를 덮어써 활성 팩으로 원자 교체한다. 파싱/검증 실패 시 활성 팩은 불변.
// 구조(스테이지 수/타겟 모드/키 집합)는 코드젠 소관 — 수치만 오버레이한다.
// SpawnObjectDefinitionPack 은 스폰 시점 복사 의미론이라 M0 범위 밖(M4).
namespace ServerData
{
    // 런타임 오버레이가 있으면 그것, 없으면 코드젠 팩. 모든 룸 틱 바인딩이 이걸 쓴다.
    const GameplayDefinitionPack& GetActiveLoLGameplayDefinitionPack();

    // 진실 JSON 3종을 재파싱해 활성 팩을 교체한다. Release 빌드는 항상 실패.
    bool_t TryReloadRuntimeGameplayDefinitions(std::string& outError);

    // 오버레이 해제 — 코드젠 팩으로 복귀.
    void ClearRuntimeGameplayDefinitions();

    // 0 = 오버레이 없음. 성공 리로드마다 +1. (M6 Hello 핸드셰이크 편입 예정)
    u32_t GetRuntimeGameplayDefinitionRevision();
}
```

1-3. C:/Users/user/Desktop/Winters/Server/Private/Data/RuntimeGameplayDefinitionOverlay.cpp

새 파일:

```cpp
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
    std::atomic<u32_t> g_runtimeRevision{ 0u };

#if defined(_DEBUG)
    using json = nlohmann::json;

    struct RuntimePackStorage
    {
        std::vector<ChampionGameplayDef> champions;
        std::vector<SkillGameplayDef> skills;
        std::vector<SummonerSpellGameplayDef> summonerSpells;
        GameplayDefinitionPack pack{};
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
        { "rectWidth", eSkillEffectParamId::RectWidth },
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
                if (!TryOverlayNumber(skillEntry, "cooldownSec", 0.f, 3600.f, pSkill->cooldown.cooldownSec, fieldError) ||
                    !TryOverlayNumber(skillEntry, "manaCost", 0.f, 1000000.f, pSkill->cost.manaCost, fieldError) ||
                    !TryOverlayNumber(skillEntry, "rangeMax", 0.f, 1000000.f, pSkill->range.rangeMax, fieldError) ||
                    !TryOverlayNumber(skillEntry, "stageWindowSec", 0.f, 60.f, pSkill->stage.stageWindowSec, fieldError))
                {
                    outError = name + ".skills[" + std::to_string(slot) + "] " + fieldError;
                    return false;
                }

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

        g_pActiveRuntimePack.store(&pack, std::memory_order_release);
        StorageGenerations().push_back(std::move(pStorage));
        g_runtimeRevision.fetch_add(1u, std::memory_order_relaxed);
        return true;
#endif
    }

    void ClearRuntimeGameplayDefinitions()
    {
        g_pActiveRuntimePack.store(nullptr, std::memory_order_release);
    }

    u32_t GetRuntimeGameplayDefinitionRevision()
    {
        return g_runtimeRevision.load(std::memory_order_relaxed);
    }
}
```

1-4. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h

기존 코드:

```cpp
    ApplyStructureStatOverride = 23,
    ClearStructureStatOverrides = 24,
    Count = 25,
};
```

아래로 교체:

```cpp
    ApplyStructureStatOverride = 23,
    ClearStructureStatOverrides = 24,
    ReloadGameplayDefinitions = 25,
    Count = 26,
};
```

`enum class ePracticeOperation` 아래 주석 블록의 기존 코드:

```cpp
// TakeControlRosterChampion: targetNet = authoritative 10-player roster bot NetId.
// ReplaceControlledChampion: flags = eChampion; replace only the current human slot.
```

아래에 추가:

```cpp
// ReloadGameplayDefinitions: 서버가 진실 JSON 3종을 재파싱해 활성 정의 팩을 교체 (Debug 전용, value/flags 미사용).
```

1-5. C:/Users/user/Desktop/Winters/Shared/Schemas/Command.fbs

기존 코드:

```text
    ApplyStructureStatOverride = 23,
    ClearStructureStatOverrides = 24
}
```

아래로 교체:

```text
    ApplyStructureStatOverride = 23,
    ClearStructureStatOverrides = 24,
    ReloadGameplayDefinitions = 25
}
```

(2026-07-14 practice wire enum gotcha 준수: 같은 숫자값을 fbs 에 append 하고 스키마 코드젠을 재실행한다 — 검증 섹션 참조.)

1-6. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomTick.cpp

기존 코드:

```cpp
#include "Server/Private/Data/LoLGameplayDefinitionPack.h"
```

아래에 추가:

```cpp
#include "Server/Private/Data/RuntimeGameplayDefinitionOverlay.h"
```

`CGameRoom::Tick()` 안의 기존 코드:

```cpp
	const GameplayDefinitionPack& definitions = ServerData::GetLoLGameplayDefinitionPack();
```

아래로 교체:

```cpp
	const GameplayDefinitionPack& definitions = ServerData::GetActiveLoLGameplayDefinitionPack();
```

1-7. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomCommands.cpp

`#include "Server/Private/Data/LoLGameplayDefinitionPack.h"` (line 11 근처) 바로 아래에 추가:

```cpp
#include "Server/Private/Data/RuntimeGameplayDefinitionOverlay.h"
```

`CGameRoom::TryHandlePracticeControl` 스위치 안, `case ePracticeOperation::ClearStructureStatOverrides:` 블록의 기존 코드:

```cpp
        return Finish(true, "structure-stats-restored");
    }

    default:
        return Finish(false, "operation-unknown");
```

아래로 교체:

```cpp
        return Finish(true, "structure-stats-restored");
    }

    case ePracticeOperation::ReloadGameplayDefinitions:
    {
        std::string reloadError;
        if (!ServerData::TryReloadRuntimeGameplayDefinitions(reloadError))
        {
            std::cerr << "[Data] runtime definition reload failed: "
                << reloadError << "\n";
            return Finish(false, "definition-reload-failed");
        }

        // 스탯 계열은 재계산 트리거 필요. 스킬 효과/쿨다운/마나/사거리는 쿼리 시점 해석이라 즉시 반영.
        u32_t refreshedCount = 0u;
        m_world.ForEach<StatComponent>(
            [&](EntityID, StatComponent& stat)
            {
                stat.bDirty = true;
                ++refreshedCount;
            });

        std::cerr << "[Data] runtime definitions reloaded rev="
            << ServerData::GetRuntimeGameplayDefinitionRevision()
            << " statRefresh=" << refreshedCount << "\n";
        return Finish(true, "definitions-reloaded");
    }

    default:
        return Finish(false, "operation-unknown");
```

같은 파일에서 룸 실행 경로가 소성 팩을 직접 바인딩하는 지점(일시정지 컨트롤 레인, line 1311-1319 근처)의 `ServerData::GetLoLGameplayDefinitionPack()` 호출을 `ServerData::GetActiveLoLGameplayDefinitionPack()` 으로 교체한다.

확인 필요:
- `std::cerr` 사용을 위해 include 블록에 `#include <iostream>` 이 없으면 추가 (현재 `<cstdio>` 까지만 확인됨).
- `rg "GetLoLGameplayDefinitionPack" Server Shared Client Tools` 로 잔여 호출부 전수 확인 후, **룸 실행 경로(GameRoomSpawn.cpp:717 포함)는 전부 GetActive 로 교체**. 예외(소성 팩 유지): `LoLGameplayDefinitions.generated.cpp` 자신, `RuntimeGameplayDefinitionOverlay.cpp` 의 base 복사, Tools/SimLab(골든 결정론 — SimLab 은 리로드 미사용).

1-8. C:/Users/user/Desktop/Winters/Client/Private/UI/ChampionTuner.cpp

`Render` 의 skill override 섹션, "Clear Server Overrides" 버튼 블록을 닫는 `ImGui::EndDisabled();` 바로 아래에 추가 (`"Override commands sent. Confirm values through gameplay and snapshots."` 문자열이 있는 Apply 블록과 같은 영역):

```cpp
		ImGui::BeginDisabled(!bCanSend);
		if (ImGui::Button("Reload Definitions From JSON (Server)"))
		{
			SendPracticeCommand(
				pScene,
				state,
				ePracticeOperation::ReloadGameplayDefinitions,
				0.f, 0u, 0u, {}, NULL_NET_ENTITY);
			state.status =
				"Definition reload requested: champions.json / SkillEffectGameplayDefs.json / SummonerSpellGameplayDefs.json";
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::TextDisabled("JSON 저장 -> 이 버튼 -> 즉시 반영 (Debug 서버)");
```

확인 필요:
- 로컬 헬퍼 `SendPracticeCommand` 시그니처가 관측 사용례 `(pScene, state, op, value, flags, slot, groundPos, targetNet)` 와 일치하는지 확인 후 인자 순서를 맞춘다.
- `bCanSend` 가 해당 스코프에 살아있는지(같은 함수 상단 line 902 에서 정의) 확인.

2. 검증

미검증:
- 빌드 미검증 (설계 계획서)
- 리로드 op 왕복/즉시 반영/되감기 생존 미검증

검증 명령:
- flatc 스키마 코드젠 재실행 (S014 와 동일 절차로 `Shared/Schemas/Generated/cpp/Command_generated.h` 재생성)
- git diff --check
- MSBuild Engine/GameSim/Server/Client/SimLab Debug x64 (기존 5빌드 절차)
- python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check   (JSON 미변경 = PASS 유지)
- Tools/SimLab/x64/Debug/SimLab.exe 1800 42   (골든 해시 85A270CA375932B7 **불변** — 리로드 미발동 시 소성 팩 그대로가 이 슬라이스의 결정론 증거)

수동 확인 (F5, Debug 서버 + practice 모드):
- SkillEffectGameplayDefs.json 의 `skill.annie.q` baseDamage 80 → 300 저장 → '9' 패널 "Reload Definitions From JSON (Server)" → Annie Q 피해가 즉시 300 (서버 로그 `[Data] runtime definitions reloaded rev=1`).
- champions.json 의 Irelia baseAd 65 → 200 저장 → 리로드 → 스탯 패널/평타 피해 즉시 반영 (StatComponent bDirty 경로).
- 되감기 30s → 재생: 리로드된 값 유지 (팩은 월드 상태가 아니라 키프레임 무관 — 의도된 동작).
- JSON 에 오타(미지 필드) 주입 → 리로드 → 거부 + 활성 팩 불변 + `[Data] runtime definition reload failed: ...` 로그.
- Release 빌드에서 op 25 가 조용히 무시되는지 (기존 practice 게이트와 동일).

확인 필요:
- 새 파일 3종(json.hpp 사본, Overlay .h/.cpp)이 Server.vcxproj 빌드에 포함되는지 확인.
- `ePracticeOperation::Count` vs 생성된 `PracticeOperation_MAX` 정합 static_assert 위치 확인 (`rg "PracticeOperation_MAX"`) — 있으면 25 반영으로 함께 갱신되는지 빌드로 검증.
- 클라 CommandSerializer 쪽 op 범위 검증 로직 존재 여부 (`rg "ePracticeOperation::Count" Client Shared`) — 있으면 신규 op 통과 확인.

알려진 캐리오버 (이 슬라이스 범위 밖, 마스터 M6):
- Kalista 패시브 대시 예측/액션 락 애니 페이싱은 클라 소성값 사용 — 해당 값 리로드 시 시각적 어긋남 가능.
- Hello 핸드셰이크는 리로드 revision 을 모름 — 드리프트 가시화는 M6.
- 세션 종료 시 JSON 변경분은 코드젠 재생성+커밋으로 소성값과 재수렴시킨다 (`Verify-LoLDataDrivenPipeline.ps1`).

롤백 범위:
- 신규 파일 3종 삭제 + 1-4/1-5/1-6/1-7/1-8 원복 (독립 슬라이스 — 다른 시스템 무변경).
