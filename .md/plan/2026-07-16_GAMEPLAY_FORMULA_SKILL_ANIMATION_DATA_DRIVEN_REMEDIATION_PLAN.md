Session - 17개 챔피언 피해 공식, 아이템 패시브, 스킬 랭크, 인게임 수치/애니메이션 튜닝을 단일 Data Driven 경로로 연결한다.

> 구현 완료 결과: `.md/build/2026-07-16_GAMEPLAY_FORMULA_DATA_DRIVEN_IMPLEMENTATION_REPORT.md`

1. 반영해야 하는 코드

작업 예산은 바닥 70% / 천장 30%로 고정한다. 바닥 70%는 공용 수식 schema·17개 현행 공식 무변경 이관·BORK/랭크/회귀 테스트에 사용한다. 천장 30%는 기획자가 인게임에서 수치와 애니메이션을 바꾸고 draft를 산출하는 한 번의 공개 가능한 튜닝 데모에 사용한다. 첫 데모 마감은 2026-07-18로 제안한다.

서버 권위 경계는 유지한다.

```text
Canonical JSON -> validate/codegen -> immutable gameplay pack
Client tuner -> typed practice command -> Server/GameSim temporary overlay
Server result -> Snapshot/Event -> Client observation
```

visual timing은 ClientPublic JSON이 소유하고, 피해/스탯/item passive/action lock은 ServerPrivate 또는 Shared authoring JSON이 소유한다. client visual 수치가 gameplay 결과를 결정하게 만들지 않는다.

1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/DamageTypes.h

새 파일:

```cpp
#pragma once

#include "WintersTypes.h"

#include <cstdint>

enum class eDamageType : uint8_t
{
    Physical = 0,
    Magic = 1,
    True = 2,
};

enum eDamageFlags : uint32_t
{
    DamageFlag_None = 0,
    DamageFlag_CanCrit = 1u << 0,
    DamageFlag_CanLifesteal = 1u << 1,
    DamageFlag_OnHit = 1u << 2,
};

enum class eDamageSourceKind : uint8_t
{
    Unknown = 0,
    BasicAttack = 1,
    Skill = 2,
    Item = 3,
    Rune = 4,
};

struct DamageFormulaDef
{
    static constexpr u8_t kMaxRank = 5u;

    bool_t bValid = false;
    u8_t rankCount = 1u;
    eDamageType type = eDamageType::Physical;
    u32_t flags = DamageFlag_None;
    f32_t flatByRank[kMaxRank]{};
    f32_t totalAdRatioByRank[kMaxRank]{};
    f32_t bonusAdRatioByRank[kMaxRank]{};
    f32_t apRatioByRank[kMaxRank]{};
    f32_t targetMaxHpRatioByRank[kMaxRank]{};
    f32_t targetMissingHpRatioByRank[kMaxRank]{};
};

inline u8_t ClampDamageFormulaRank(const DamageFormulaDef& formula, u8_t rank)
{
    const u8_t count = formula.rankCount > 0u
        ? (formula.rankCount < DamageFormulaDef::kMaxRank
            ? formula.rankCount
            : DamageFormulaDef::kMaxRank)
        : 1u;
    const u8_t resolved = rank > 0u ? rank : 1u;
    return resolved < count ? resolved : count;
}

inline f32_t ResolveDamageFormulaRankedValue(
    const DamageFormulaDef& formula,
    const f32_t values[DamageFormulaDef::kMaxRank],
    u8_t rank)
{
    return values[ClampDamageFormulaRank(formula, rank) - 1u];
}
```

1-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/DamageRequestComponent.h

기존 코드:

```cpp
#include "WintersTypes.h"
#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
```

아래로 교체:

```cpp
#include "WintersTypes.h"
#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Definitions/DamageTypes.h"
```

삭제할 범위: `enum class eDamageType` 시작부터 `enum class eDamageSourceKind` 종료까지 삭제한다. `DamageRequest` 구조체는 그대로 유지해 network/event 결과와 기존 enqueue API를 깨지 않는다.

1-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/SkillAtomData.h

기존 코드:

```cpp
#include "LoLMatchContext.h"
#include "SkillTypes.h"
#include "WintersTypes.h"
```

아래로 교체:

```cpp
#include "LoLMatchContext.h"
#include "SkillTypes.h"
#include "Shared/GameSim/Definitions/DamageTypes.h"
#include "WintersTypes.h"
```

기존 scalar `SkillCostSpec`, `SkillCooldownSpec` 두 구조체를 삭제하고 `SkillSlotBinding` 앞에 아래를 추가:

```cpp
inline constexpr u8_t kSkillRankValueMax = 5u;

struct SkillCostSpec
{
    u8_t rankCount = 1u;
    f32_t manaCostByRank[kSkillRankValueMax]{};
};

struct SkillCooldownSpec
{
    u8_t rankCount = 1u;
    f32_t cooldownSecByRank[kSkillRankValueMax]{};
};
```

cooldown/mana rank 값이 geometry/status `params`에 섞이지 않게 한다.

기존 코드:

```cpp
struct SkillEffectSpec
{
    u16_t scalingTableId = 0;
    u32_t gameplayPolicyId = 0;
    u32_t replicatedCueId = 0;
    u8_t paramCount = 0;
    SkillEffectParam params[kSkillEffectParamMax] = {};
};
```

아래로 교체:

```cpp
struct SkillEffectSpec
{
    DamageFormulaDef damage{};
    u16_t scalingTableId = 0;
    u32_t gameplayPolicyId = 0;
    u32_t replicatedCueId = 0;
    u8_t paramCount = 0;
    SkillEffectParam params[kSkillEffectParamMax] = {};
};
```

`scalingTableId`는 이 단계에서 읽기 호환을 위해 남기되 모든 reader를 1-8에서 제거한다. reader count 0과 SimLab 통과 뒤 필드, `SkillScalingTable`, registry를 삭제한다. 두 공식 경로를 장기 공존시키지 않는다.

1-4. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/ItemDef.h

기존 코드:

```cpp
#include "WintersTypes.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Definitions/DamageTypes.h"
#include "WintersTypes.h"
```

기존 코드:

```cpp
struct ItemDef
{
    u16_t itemId = 0;
    u16_t price = 0;
    ItemStatModifier stats{};
    const char* displayName = nullptr;
};
```

아래로 교체:

```cpp
struct ItemDef
{
    u16_t itemId = 0;
    u16_t price = 0;
    ItemStatModifier stats{};
    const char* displayName = nullptr;
    DamageFormulaDef onHitDamage{};
};
```

`onHitDamage`는 기존 `ItemRegistry.cpp`의 4항 aggregate initializer가 깨지지 않도록 `displayName` 뒤에 추가한다.

이 slice는 basic-attack physical on-hit만 연다. active item, unique group, cooldown은 요청 범위를 넘기므로 별도 schema가 확정될 때까지 추가하지 않는다.

1-5. C:/Users/user/Desktop/Winters/Tools/LoLData/Build-LoLDefinitionPack.py

`ITEM_STAT_FIELDS` 바로 아래에 추가:

```python
DAMAGE_TYPES = {
    "Physical": "Physical",
    "Magic": "Magic",
    "True": "True",
}

DAMAGE_FLAGS = {
    "CanCrit": "DamageFlag_CanCrit",
    "CanLifesteal": "DamageFlag_CanLifesteal",
    "OnHit": "DamageFlag_OnHit",
}

DAMAGE_FORMULA_FIELDS = (
    "flatByRank",
    "totalAdRatioByRank",
    "bonusAdRatioByRank",
    "apRatioByRank",
    "targetMaxHpRatioByRank",
    "targetMissingHpRatioByRank",
)


def normalize_damage_formula(source: object, path: str) -> dict | None:
    if source is None:
        return None
    source = require_object(source, path)
    damage_type = source.get("type")
    if damage_type not in DAMAGE_TYPES:
        fail(f"{path}.type must be one of {sorted(DAMAGE_TYPES)}")

    flags_source = require_array(source.get("flags", []), f"{path}.flags")
    flags = []
    for index, flag in enumerate(flags_source):
        if flag not in DAMAGE_FLAGS:
            fail(f"{path}.flags[{index}] must be one of {sorted(DAMAGE_FLAGS)}")
        if flag in flags:
            fail(f"{path}.flags contains duplicate {flag}")
        flags.append(flag)

    fields = {}
    rank_count = 0
    for field in DAMAGE_FORMULA_FIELDS:
        values_source = require_array(source.get(field, []), f"{path}.{field}")
        if len(values_source) > 5:
            fail(f"{path}.{field} must contain at most 5 ranks")
        values = [
            validate_skill_effect_param_domain(
                field,
                legacy.as_float(value, f"{path}.{field}[{index}]"),
                f"{path}.{field}[{index}]",
            )
            for index, value in enumerate(values_source)
        ]
        if values:
            if rank_count not in (0, len(values)):
                fail(f"{path} rank-array lengths must match")
            rank_count = len(values)
        fields[field] = values

    if rank_count == 0:
        fail(f"{path} requires at least one non-empty rank array")
    for field in DAMAGE_FORMULA_FIELDS:
        if not fields[field]:
            fields[field] = [0.0] * rank_count

    return {
        "type": DAMAGE_TYPES[damage_type],
        "flags": [DAMAGE_FLAGS[flag] for flag in flags],
        "rankCount": rank_count,
        **fields,
    }
```

`normalize_skill_effect_root`의 기존 코드:

```python
        records.append({"key": key, "params": params, "summonPolicyParams": summon_params})
```

아래로 교체:

```python
        records.append(
            {
                "key": key,
                "params": params,
                "summonPolicyParams": summon_params,
                "damage": normalize_damage_formula(
                    item.get("damage"),
                    f"skillEffects[{index}].damage",
                ),
            }
        )
```

`apply_skill_effect_params`의 기존 코드:

```python
        skill["effectParams"] = record.get("params", [])
        skill["summonPolicyParams"] = record.get("summonPolicyParams", [])
```

아래로 교체:

```python
        skill["effectParams"] = record.get("params", [])
        skill["summonPolicyParams"] = record.get("summonPolicyParams", [])
        skill["damageFormula"] = record.get("damage")
```

`normalize_items_root`에서 `stats` 생성 바로 아래에 추가:

```python
        on_hit_damage = normalize_damage_formula(
            item.get("onHitDamage"),
            f"items[{index}].onHitDamage",
        )
```

기존 코드:

```python
        records.append({"itemId": item_id, "price": price, "name": name, "stats": stats})
```

아래로 교체:

```python
        records.append(
            {
                "itemId": item_id,
                "price": price,
                "name": name,
                "stats": stats,
                "onHitDamage": on_hit_damage,
            }
        )
```

server C++ emitter에는 아래 helper를 추가하고 skill `def.effect.damage`, item `def.onHitDamage`에 동일하게 사용한다.

```python
def emit_damage_formula_cpp(lines: list[str], target: str, formula: dict | None) -> None:
    if formula is None:
        return
    lines.append(f"        {target}.bValid = true;")
    lines.append(f"        {target}.rankCount = {formula['rankCount']}u;")
    lines.append(f"        {target}.type = eDamageType::{formula['type']};")
    flag_expr = " | ".join(formula["flags"]) if formula["flags"] else "DamageFlag_None"
    lines.append(f"        {target}.flags = {flag_expr};")
    cpp_fields = {
        "flatByRank": "flatByRank",
        "totalAdRatioByRank": "totalAdRatioByRank",
        "bonusAdRatioByRank": "bonusAdRatioByRank",
        "apRatioByRank": "apRatioByRank",
        "targetMaxHpRatioByRank": "targetMaxHpRatioByRank",
        "targetMissingHpRatioByRank": "targetMissingHpRatioByRank",
    }
    for source_field, cpp_field in cpp_fields.items():
        for index, value in enumerate(formula[source_field]):
            lines.append(
                f"        {target}.{cpp_field}[{index}] = {cpp_float(value)};"
            )
```

CONFIRM_NEEDED: item C++ emission 함수의 정확한 anchor는 implementation turn에서 `generate_server_cpp`의 item loop 전체를 다시 확인한 뒤 지정한다. 생성 파일을 직접 편집하지 않는다.

1-6. C:/Users/user/Desktop/Winters/Server/Private/Data/RuntimeGameplayDefinitionOverlay.cpp

anonymous namespace의 숫자 overlay helper 옆에 추가:

```cpp
template <size_t N>
bool_t TryOverlayRankArray(
    const json& object,
    const char* field,
    u8_t expectedCount,
    f32_t maxValue,
    f32_t (&outValues)[N],
    std::string& outError)
{
    if (!object.contains(field) || !object[field].is_array())
    {
        outError = std::string(field) + " must be an array";
        return false;
    }

    const json& values = object[field];
    if (expectedCount == 0u || expectedCount > N || values.size() != expectedCount)
    {
        outError = std::string(field) + " rank count mismatch";
        return false;
    }

    for (u8_t index = 0u; index < expectedCount; ++index)
    {
        if (!values[index].is_number())
        {
            outError = std::string(field) + " contains a non-number";
            return false;
        }
        const f32_t value = values[index].get<f32_t>();
        if (!std::isfinite(value) || value < 0.f || value > maxValue)
        {
            outError = std::string(field) + " value out of range";
            return false;
        }
        outValues[index] = value;
    }
    return true;
}
```

`ApplyChampionsJson`의 scalar cooldown/mana overlay block:

```cpp
                if (!TryOverlayNumber(skillEntry, "cooldownSec", 0.f, 3600.f, pSkill->cooldown.cooldownSec, fieldError) ||
                    !TryOverlayNumber(skillEntry, "manaCost", 0.f, 1000000.f, pSkill->cost.manaCost, fieldError) ||
                    !TryOverlayNumber(skillEntry, "rangeMax", 0.f, 1000000.f, pSkill->range.rangeMax, fieldError) ||
```

아래로 교체:

```cpp
                const u8_t expectedRankCount = slot == static_cast<u32_t>(eSkillSlot::BasicAttack)
                    ? 1u
                    : (slot == static_cast<u32_t>(eSkillSlot::R) ? 3u : 5u);
                if (!TryOverlayRankArray(
                        skillEntry, "cooldownSecByRank", expectedRankCount, 3600.f,
                        pSkill->cooldown.cooldownSecByRank, fieldError) ||
                    !TryOverlayRankArray(
                        skillEntry, "manaCostByRank", expectedRankCount, 1000000.f,
                        pSkill->cost.manaCostByRank, fieldError) ||
                    !TryOverlayNumber(skillEntry, "rangeMax", 0.f, 1000000.f, pSkill->range.rangeMax, fieldError) ||
```

성공한 뒤 `pSkill->cooldown.rankCount`와 `pSkill->cost.rankCount`를 `expectedRankCount`로 기록한다. canonical migration이 끝난 뒤 scalar key는 unknown-field validation에서 거부한다.

`ApplySkillEffectsJson`에서 `params` 처리 직후 아래 로직을 추가한다. 실제 구현은 codegen과 동일한 `type/flags/6개 rank array` 검증 helper 하나를 공유하는 형태로 작성한다.

```cpp
            if (entry.contains("damage"))
            {
                DamageFormulaDef parsed{};
                if (!TryParseDamageFormula(entry["damage"], key + ".damage", parsed, outError))
                    return false;
                pSkill->effect.damage = parsed;
            }
```

`ApplyItemsJson`의 known key:

```cpp
            static constexpr const char* kKnownItemKeys[] =
            {
                "itemId", "price", "stats", "name",
            };
```

아래로 교체:

```cpp
            static constexpr const char* kKnownItemKeys[] =
            {
                "itemId", "price", "stats", "onHitDamage", "name",
            };
```

`stats` 처리 뒤에 추가:

```cpp
            if (entry.contains("onHitDamage"))
            {
                DamageFormulaDef parsed{};
                if (!TryParseDamageFormula(
                        entry["onHitDamage"],
                        "items[" + std::to_string(itemId) + "].onHitDamage",
                        parsed,
                        outError))
                {
                    return false;
                }
                pItem->onHitDamage = parsed;
            }
```

hot reload는 기존처럼 새 key 추가를 거부한다. 기존 skill/item의 수치만 교체하고 실패 시 active pack을 보존한다.

1-7. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/GameplayDefinitionQuery.h

기존 forward declaration 바로 아래에 추가:

```cpp
struct DamageRequest;
```

`ResolveSkillEffectParam` 선언 아래에 추가:

```cpp
    bool_t BuildSkillDamageRequest(
        CWorld& world,
        EntityID source,
        EntityID target,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot,
        u8_t rank,
        eTeam sourceTeam,
        eDamageSourceKind sourceKind,
        DamageRequest& outRequest);
```

1-8. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/GameplayDefinitionQuery.cpp

include 영역에 추가:

```cpp
#include "Shared/GameSim/Components/DamageRequestComponent.h"
```

`ResolveSkillEffectParam` 함수 바로 아래에 추가:

```cpp
    bool_t BuildSkillDamageRequest(
        CWorld& world,
        EntityID source,
        EntityID target,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot,
        u8_t rank,
        eTeam sourceTeam,
        eDamageSourceKind sourceKind,
        DamageRequest& outRequest)
    {
        const SkillGameplayDef* skill =
            FindSkill(world, source, tc, fallbackChampion, slot);
        if (!skill || !skill->effect.damage.bValid)
            return false;

        const DamageFormulaDef& formula = skill->effect.damage;
        outRequest = {};
        outRequest.source = source;
        outRequest.target = target;
        outRequest.sourceTeam = sourceTeam;
        outRequest.type = formula.type;
        outRequest.flatAmount = ResolveDamageFormulaRankedValue(
            formula, formula.flatByRank, rank);
        outRequest.adRatioOverride = ResolveDamageFormulaRankedValue(
            formula, formula.totalAdRatioByRank, rank);
        outRequest.bonusAdRatioOverride = ResolveDamageFormulaRankedValue(
            formula, formula.bonusAdRatioByRank, rank);
        outRequest.apRatioOverride = ResolveDamageFormulaRankedValue(
            formula, formula.apRatioByRank, rank);
        outRequest.targetMaxHpRatioOverride = ResolveDamageFormulaRankedValue(
            formula, formula.targetMaxHpRatioByRank, rank);
        outRequest.targetMissingHpRatioOverride = ResolveDamageFormulaRankedValue(
            formula, formula.targetMissingHpRatioByRank, rank);
        outRequest.skillId = static_cast<u16_t>(
            (static_cast<u32_t>(fallbackChampion) << 8u) | slot);
        outRequest.rank = rank > 0u ? rank : 1u;
        outRequest.iSourceSlot = slot;
        outRequest.eSourceKind = sourceKind;
        outRequest.flags = formula.flags;
        return true;
    }
```

`ResolveSkillCooldown`에서 `CooldownReductionPerRank`, `ResolveSkillManaCost`에서 `ManaCostPerRank`를 읽는 블록은 삭제하고 각각 `cooldownSecByRank`, `manaCostByRank`의 clamp된 rank 값을 반환한다.

```cpp
const u8_t rank = ResolveSkillRankForScaling(world, entity, slot);
const u8_t count = skill->cooldown.rankCount > 0u
    ? skill->cooldown.rankCount
    : 1u;
const u8_t index = static_cast<u8_t>((std::min<u8_t>)(rank, count) - 1u);
return skill->cooldown.cooldownSecByRank[index];
```

mana도 같은 방식으로 `skill->cost.rankCount`와 `manaCostByRank`를 사용한다.

practice override는 `DamageFormulaDef`와 cooldown/mana rank array용 typed operation을 추가하기 전까지 기존 scalar params에만 적용된다. 이 상태를 숨기지 말고 1-35 UI에서 formula array를 read-only로 표시한다.

1-9. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/Damage/DamagePipeline.cpp

삭제할 코드:

```cpp
#include "Shared/GameSim/Registries/SkillScaling/SkillScalingRegistry.h"
```

`ResolveDamageFlags`, `ResolveDamageType`, `BuildRawDamage`에서 `CSkillScalingRegistry::Instance().FindBySkillId`를 읽는 모든 블록을 삭제한다. `DamageRequest`에 이미 해석된 scalar만 계산한다.

`BuildRawDamage`의 핵심은 아래로 고정한다.

```cpp
f32_t BuildRawDamage(CWorld& world, const DamageRequest& req)
{
    f32_t amount = req.flatAmount != 0.f ? req.flatAmount : req.amount;
    if (req.source != NULL_ENTITY && world.HasComponent<StatComponent>(req.source))
    {
        const StatComponent& source = world.GetComponent<StatComponent>(req.source);
        amount += source.ad * req.adRatioOverride;
        amount += source.bonusAd * req.bonusAdRatioOverride;
        amount += source.ap * req.apRatioOverride;
    }
    amount += ResolveTargetMaxHp(world, req.target) * req.targetMaxHpRatioOverride;
    amount += ResolveTargetMissingHp(world, req.target) * req.targetMissingHpRatioOverride;
    return amount;
}
```

이 변경으로 0을 sentinel로 사용하던 registry override 문제가 사라진다. `ResolveDamageFlags`는 `return req.flags;`, `ResolveDamageType`은 `return req.type;`으로 교체한다.

1-10. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/Damage/DamageQueueSystem.cpp

`CDamageQueueSystem::Execute`의 기존 코드:

```cpp
        const DamageRequest request = world.GetComponent<DamageRequestComponent>(entity);
```

아래로 교체:

```cpp
        DamageRequest request = world.GetComponent<DamageRequestComponent>(entity);
        AccumulateBasicAttackItemOnHit(world, request);
```

anonymous namespace의 `TryMarkChampionDeathCredit` 위에 추가:

```cpp
    void AccumulateBasicAttackItemOnHit(CWorld& world, DamageRequest& request)
    {
        if (request.eSourceKind != eDamageSourceKind::BasicAttack ||
            (request.flags & DamageFlag_OnHit) == 0u ||
            request.source == NULL_ENTITY ||
            !world.HasComponent<InventoryComponent>(request.source))
        {
            return;
        }

        const InventoryComponent& inventory =
            world.GetComponent<InventoryComponent>(request.source);
        for (u8_t index = 0u;
            index < inventory.count && index < InventoryComponent::kMaxSlots;
            ++index)
        {
            const ItemDef* item = CItemRegistry::Instance().Find(inventory.itemIds[index]);
            if (!item || !item->onHitDamage.bValid)
                continue;

            const DamageFormulaDef& formula = item->onHitDamage;
            if (formula.type != request.type)
                continue;

            request.flatAmount += ResolveDamageFormulaRankedValue(
                formula, formula.flatByRank, 1u);
            request.adRatioOverride += ResolveDamageFormulaRankedValue(
                formula, formula.totalAdRatioByRank, 1u);
            request.bonusAdRatioOverride += ResolveDamageFormulaRankedValue(
                formula, formula.bonusAdRatioByRank, 1u);
            request.apRatioOverride += ResolveDamageFormulaRankedValue(
                formula, formula.apRatioByRank, 1u);
            request.targetMaxHpRatioOverride += ResolveDamageFormulaRankedValue(
                formula, formula.targetMaxHpRatioByRank, 1u);
            request.targetMissingHpRatioOverride += ResolveDamageFormulaRankedValue(
                formula, formula.targetMissingHpRatioByRank, 1u);
        }
    }
```

필요 include:

```cpp
#include "Shared/GameSim/Components/InventoryComponent.h"
#include "Shared/GameSim/Definitions/ItemDef.h"
```

이 위치는 melee/projectile을 구분하지 않고 충돌 후 enqueue된 basic-attack request에 한 번 적용한다. Ashe Q의 추가 시각 화살처럼 `bApplyDamageOnHit=false`인 request에는 적용되지 않는다. Ezreal Q는 `Skill` source kind이므로 BORK basic-attack passive를 발동하지 않는다.

1-11. C:/Users/user/Desktop/Winters/Data/LoL/ServerPrivate/Gameplay/ItemGameplayDefs.json

item 3153의 기존 `stats` 바로 아래에 추가:

```json
"onHitDamage": {
  "type": "Physical",
  "flags": [],
  "flatByRank": [0.0],
  "targetMaxHpRatioByRank": [0.0]
}
```

CONFIRM_NEEDED: `targetMaxHpRatioByRank[0]`의 최종 기획 비율. 값이 확정되기 전 0으로 shipping하면 기능이 없는 것과 같으므로 validation에서 item 3153의 0을 실패 처리한다. 근접/원거리 차등 비율이 필요하면 단일 on-hit 배열로 구현하지 말고 attacker range class를 schema에 먼저 추가한다.

1-12. C:/Users/user/Desktop/Winters/Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json

기존 `params`는 geometry/status만 남기고 피해 수치는 `damage` 객체로 이동한다. 아래 값은 현재 동작 보존용 1차 이관표다.

```jsonc
// 대표 형식. 실제 파일에서는 각 기존 key 객체 안에 damage를 넣는다.
{"key":"skill.irelia.q", "damage":{"type":"Physical","flags":[],"flatByRank":[70,95,120,145,170]}},
{"key":"skill.irelia.w", "damage":{"type":"Physical","flags":[],"flatByRank":[70,110,150,190,230]}},
{"key":"skill.irelia.e", "damage":{"type":"Physical","flags":[],"flatByRank":[100,130,160,190,220]}},
{"key":"skill.irelia.r", "damage":{"type":"Physical","flags":[],"flatByRank":[250,250,250]}},

{"key":"skill.yasuo.q", "damage":{"type":"Physical","flags":["OnHit"],"flatByRank":[60,60,60,60,60]}},
{"key":"skill.yasuo.e", "damage":{"type":"Magic","flags":[],"flatByRank":[80,80,80,80,80]}},
{"key":"skill.yasuo.r", "damage":{"type":"Physical","flags":[],"flatByRank":[200,200,200]}},

{"key":"skill.kalista.q", "damage":{"type":"Physical","flags":["OnHit"],"flatByRank":[70,70,70,70,70]}},
{"key":"skill.garen.r", "damage":{"type":"True","flags":[],"flatByRank":[150,300,450],"targetMissingHpRatioByRank":[0.25,0.25,0.25]}},
{"key":"skill.zed.q", "damage":{"type":"Physical","flags":[],"flatByRank":[70,95,120,145,170]}},
{"key":"skill.zed.e", "damage":{"type":"Physical","flags":[],"flatByRank":[65,85,105,125,145]}},
{"key":"skill.zed.r", "damage":{"type":"Physical","flags":[],"flatByRank":[0,0,0],"targetMissingHpRatioByRank":[0.30,0.30,0.30]}},
{"key":"skill.riven.r", "damage":{"type":"Physical","flags":[],"flatByRank":[100,150,200]}},

{"key":"skill.annie.q", "damage":{"type":"Magic","flags":[],"flatByRank":[115,150,185,220,255]}},
{"key":"skill.annie.w", "damage":{"type":"Magic","flags":[],"flatByRank":[115,160,205,250,295]}},
{"key":"skill.annie.r", "damage":{"type":"Magic","flags":[],"flatByRank":[225,300,375]}},

{"key":"skill.ezreal.q", "damage":{"type":"Physical","flags":["OnHit"],"flatByRank":[20,45,70,95,120],"totalAdRatioByRank":[1.3,1.3,1.3,1.3,1.3],"apRatioByRank":[0.4,0.4,0.4,0.4,0.4]}},
{"key":"skill.ezreal.w", "damage":{"type":"Magic","flags":[],"flatByRank":[80,135,190,245,300],"bonusAdRatioByRank":[1,1,1,1,1],"apRatioByRank":[0.9,0.9,0.9,0.9,0.9]}},
{"key":"skill.ezreal.e", "damage":{"type":"Magic","flags":[],"flatByRank":[80,130,180,230,280],"bonusAdRatioByRank":[0.6,0.6,0.6,0.6,0.6],"apRatioByRank":[0.75,0.75,0.75,0.75,0.75]}},
{"key":"skill.ezreal.r", "damage":{"type":"Magic","flags":[],"flatByRank":[350,550,750],"bonusAdRatioByRank":[1,1,1],"apRatioByRank":[1.1,1.1,1.1]}},

{"key":"skill.fiora.q", "damage":{"type":"Physical","flags":["OnHit"],"flatByRank":[70,70,70,70,70]}},
{"key":"skill.fiora.r", "damage":{"type":"Physical","flags":[],"flatByRank":[80,80,80]}},
{"key":"skill.jax.q", "damage":{"type":"Physical","flags":[],"flatByRank":[70,70,70,70,70]}},
{"key":"skill.jax.e", "damage":{"type":"Physical","flags":[],"flatByRank":[60,60,60,60,60]}},
{"key":"skill.leesin.q", "damage":{"type":"Physical","flags":["OnHit"],"flatByRank":[55,80,105,130,155]}},
{"key":"skill.leesin.r", "damage":{"type":"Physical","flags":[],"flatByRank":[150,150,150]}},
{"key":"skill.kindred.w", "damage":{"type":"Physical","flags":[],"flatByRank":[35,35,35,35,35]}},
{"key":"skill.kindred.e", "damage":{"type":"Physical","flags":["OnHit"],"flatByRank":[80,80,80,80,80]}},
{"key":"skill.ashe.w", "damage":{"type":"Physical","flags":[],"flatByRank":[45,45,45,45,45]}},
{"key":"skill.ashe.r", "damage":{"type":"Physical","flags":[],"flatByRank":[250,250,250]}},
{"key":"skill.viego.q", "damage":{"type":"Physical","flags":["OnHit"],"flatByRank":[65,65,65,65,65]}},
{"key":"skill.viego.w", "damage":{"type":"Physical","flags":[],"flatByRank":[55,55,55,55,55]}},
{"key":"skill.viego.r", "damage":{"type":"Physical","flags":[],"flatByRank":[150,150,150]}},
{"key":"skill.yone.q", "damage":{"type":"Physical","flags":["OnHit"],"flatByRank":[75,75,75,75,75]}},
{"key":"skill.yone.w", "damage":{"type":"Physical","flags":[],"flatByRank":[65,65,65,65,65]}},
{"key":"skill.yone.r", "damage":{"type":"Physical","flags":[],"flatByRank":[150,150,150]}},
{"key":"skill.sylas.e", "damage":{"type":"Physical","flags":[],"flatByRank":[65,90,115,140,165]}}
```

위 1차 이관은 현재 플레이 결과를 보존한다. Annie/Irelia off-by-one을 고쳐 rank 1을 `base`로 만들지 여부는 golden baseline을 현재값으로 먼저 고정한 뒤 별도 balance change로 수행한다.

CONFIRM_NEEDED 목록:

- Annie Q/W/R/E의 의도된 rank 1 값과 AP 계수
- Fiora passive가 `Target.MaxHP`인지 `Target.MissingHP`인지, true/physical 여부와 비율
- Zed passive와 R이 만료 순간 missing HP인지 mark 이후 누적 피해인지
- BORK 최대 체력 비율과 근접/원거리 분기
- Kalista E base/per-spear/rank/AD 계수
- Garen Q/W/E, R의 확정 damage type
- Riven Q/W/R 잃은 체력 계수
- Kindred Q/passive/E 체력 계수
- Master Yi passive/Q/E
- Sylas Q/W
- Yone passive split/E echo/W 체력 계수
- Lee Sin Q1/Q2/E/W2 계수
- Yasuo/Viego/Jax/Ashe의 rank와 AD/AP 계수

1-13. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Annie/AnnieGameSim.cpp

anchor: `f32_t RankedDamage`와 Q/W/R `ApplyMagicDamage` 호출.

`RankedDamage`를 삭제하고 각 damage enqueue를 다음 패턴으로 교체한다.

```cpp
DamageRequest request{};
if (GameplayDefinitionQuery::BuildSkillDamageRequest(
        world, ctx.casterEntity, target, *ctx.pTickCtx,
        eChampion::ANNIE, static_cast<u8_t>(eSkillSlot::Q),
        ctx.skillRank, ctx.casterTeam, eDamageSourceKind::Skill, request))
{
    EnqueueDamageRequest(world, request);
}
```

W/R도 slot만 바꿔 동일 경로를 사용한다. E shield는 피해 공식과 분리해 기존 param을 유지하되 `base + perRank × (rank-1)`로 바꿀지는 CONFIRM_NEEDED 확정 뒤 적용한다.

1-14. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Irelia/IreliaGameSim.cpp

anchor: `qBaseDamage + qDamagePerRank`, `wBaseDamage + wDamagePerRank`, `eBaseDamage + eDamagePerRank`, R `rDamage`.

네 수동 조립을 1-13의 `BuildSkillDamageRequest` 패턴으로 교체한다. 1차 migration JSON에는 현재 실제 rank 값 70/95/...를 넣어 동작을 보존한다. 그 다음 balance commit에서 의도값 45/70/...로 바꾸면 코드 변경 없이 수치만 변경된다.

1-15. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Ezreal/EzrealGameSim.cpp

anchor: `ResolveRankedValue`, `BuildEssenceFluxDetonationDamage`, `LaunchMysticShot`, `LaunchArcaneShift`, `LaunchTrueshotBarrage`.

projectile spawn 인자의 flat/ratio를 `DamageFormulaDef`에서 채운 `DamageRequest`로 전달하도록 교체한다. R non-epic flat 대체는 별도 `NonEpicDamageFormula` schema가 생기기 전까지 현재 params를 유지한다. generic formula 이관 뒤 `ResolveRankedValue`의 피해용 reader를 삭제한다.

1-16. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Fiora/FioraGameSim.cpp

anchor: `OnQ`, `OnW`, `OnE`, `OnR`, `ConsumeBasicAttackDamage`.

Q/R flat 피해와 E `bladeworkDamageBonus`는 formula builder로 교체한다.

CONFIRM_NEEDED: passive vital 상태 구조, 방향 판정, 대상 MaxHP/MissingHP 기준, damage type, heal/MS 보상. 이 값이 확정되기 전에는 임의의 Fiora passive를 구현하지 않는다. W는 damage formula가 생긴 뒤 riposte 성공 여부에 따라 stun/slow를 결정하는 authoritative 상태가 필요하다.

1-17. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Garen/GarenGameSim.cpp

anchor: `request.flatAmount = baseDamage + damagePerRank * rankIndex`.

해당 블록을 `BuildSkillDamageRequest`로 교체한다. Q/W/E는 CONFIRM_NEEDED 수치/상태 정의 후 같은 파일에 추가하며 client-only 구현을 만들지 않는다.

1-18. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Zed/ZedGameSim.cpp

anchor: Q projectile `baseDamage + damagePerRank`, E damage, R tick의 `missingHealth * mark.fMissingHealthDamageRatio`.

Q/E는 공용 formula로 교체한다. R은 `DamageFormulaDef`를 mark 생성 시 복사하지 말고 만료 시 active pack에서 다시 읽어 Debug reload 의미를 명확히 한다.

CONFIRM_NEEDED: Zed passive와 Death Mark의 기준. 누적 피해형으로 확정되면 `ZedDeathMarkComponent`에 `fAccumulatedPostMarkDamage`를 추가하고 DamageQueue의 최종 `result.finalAmount`만 기록한다.

1-19. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Riven/RivenGameSim.cpp

anchor: `OnQ`, `OnW`, `OnE`, R windslash `request.flatAmount`.

R2를 공용 formula로 교체한다. Q/W damage 및 R missing-health amplification은 CONFIRM_NEEDED 값을 받은 뒤 formula와 authoritative hit query를 함께 추가한다.

1-20. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Jax/JaxGameSim.cpp

anchor: `OnQ`, `OnW`, `ReleaseJaxCounterStrike`, `ConsumeBasicAttackDamage`.

Q/E는 formula request, W/R on-hit bonus는 해당 skill formula의 ranked flat을 읽는다. 현재 flat 보존 후 rank/ratio 값은 JSON만 조정한다.

1-21. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Kalista/KalistaGameSim.cpp

anchor: Q projectile damage와 E `baseDamage + damagePerSpear * stack.stackCount`.

Q는 formula로 교체한다. E는 stack-dependent이므로 `flatByRank + DamagePerSpear(rank) × stackCount`를 명시하는 별도 `damagePerStackByRank` schema가 필요하다.

CONFIRM_NEEDED: 현재 1 spear에서 base+1×perSpear가 맞는지, 첫 spear를 base로 보고 추가 spear만 perSpear인지. 이 결정 전 fallback 20/30을 JSON 이관했다고 보고하지 않는다.

1-22. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Kindred/KindredGameSim.cpp

anchor: W tick enqueue와 `ConsumeBasicAttackDamage` E pounce.

W/E를 formula로 교체하고 W enqueue의 강제 rank 1을 실제 W rank로 바꾼다. Q/passive는 CONFIRM_NEEDED 공식 승인 후 구현한다.

1-23. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/LeeSin/LeeSinGameSim.cpp

anchor: Q2, E, R damage enqueue.

Q2/R을 formula로 교체한다. E 피해와 W2는 CONFIRM_NEEDED. Q1의 owner는 다음 절에서 이 파일로 이동한다.

1-24. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp

삭제할 범위: `IsServerProjectileSkill`의 Lee Sin 특례와 `ResolveServerOwnedSkillCommand` 안 Q projectile 생성에서 speed/radius/`55 + 25*(rank-1)`을 조립하는 블록.

Lee Sin Q1 발사 API를 `LeeSinGameSim`으로 옮기고 CommandExecutor는 champion hook registry만 호출한다. 이 파일에 champion balance 숫자를 남기지 않는다.

1-25. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/MasterYi/MasterYiGameSim.cpp

anchor: `RegisterHooks`의 R-only 등록.

CONFIRM_NEEDED: passive/Q/W/E의 authoritative behavior와 formula. 승인 전 빈 flat 피해를 임시로 추가하지 않는다. 승인 후 Q/W/E hook을 같은 파일에 등록하고 피해는 공용 builder만 사용한다.

1-26. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Ashe/AsheGameSim.cpp

anchor: `ConsumeBasicAttackDamage`, W/R projectile 생성.

W/R을 formula로 교체한다. Q의 BA bonus는 `skill.ashe.q.damage`를 읽되 첫 실제 damage projectile request에만 합산한다. 나머지 4개 시각 화살은 기존처럼 damage/on-hit를 적용하지 않는다.

1-27. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Viego/ViegoGameSim.cpp

anchor: Q/W/R `BaseDamage` reader.

세 skill damage enqueue를 공용 formula로 교체한다. possession이 복사한 skill rank와 pack lookup champion이 어긋나지 않는 SimLab case를 추가한다.

1-28. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Yasuo/YasuoGameSim.cpp

anchor: Q normal/tornado/dash-area, E, R damage enqueue.

normal Q/E/R은 공용 formula로 교체한다. tornado/dash-area는 한 skill의 variant formula가 필요하므로 `params` fallback을 유지하거나 `damageVariants` schema를 추가한다.

CONFIRM_NEEDED: variant를 별도 formula로 데이터화할지 normal Q formula의 multiplier로 둘지.

1-29. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Yone/YoneGameSim.cpp

anchor: Q/W/R damage enqueue, E return.

Q/W/R은 공용 formula로 교체한다. passive split damage와 E echo는 CONFIRM_NEEDED. E가 확정되면 mark 기간 `result.finalAmount`를 기록하고 return 시 formula 비율로 별도 damage request를 만든다.

1-30. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Sylas/SylasGameSim.cpp

anchor: E2 projectile damage와 R hijack.

E2를 공용 formula로 교체한다. Q/W는 CONFIRM_NEEDED. 훔친 R은 원본 owner skill formula와 Sylas stat ratio conversion 규칙을 명시적으로 선택해야 하며, 임의 fallback을 만들지 않는다.

1-31. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/SkillRank/SkillRankSystem.cpp

기존 코드:

```cpp
bool CSkillRankSystem::TryLevelSkill(SkillRankComponent& ranks, u8_t slot)
```

아래로 교체:

```cpp
bool CSkillRankSystem::TryLevelSkill(
    SkillRankComponent& ranks,
    u8_t championLevel,
    u8_t slot)
{
    if (slot >= SkillRankComponent::kSlotCount || ranks.pointsAvailable == 0u)
        return false;

    const u8_t nextRank = static_cast<u8_t>(ranks.ranks[slot] + 1u);
    if (nextRank > GetMaxRankForSlot(slot))
        return false;

    const u8_t requiredLevel = slot == static_cast<u8_t>(eSkillSlot::R)
        ? static_cast<u8_t>(6u + (nextRank - 1u) * 5u)
        : static_cast<u8_t>(nextRank * 2u - 1u);
    if (championLevel < requiredLevel)
        return false;

    ranks.ranks[slot] = nextRank;
    --ranks.pointsAvailable;
    return true;
}
```

기존 world overload:

```cpp
bool CSkillRankSystem::TryLevelSkill(CWorld& world, EntityID entity, u8_t slot)
{
    if (!world.HasComponent<SkillRankComponent>(entity))
        return false;
    return TryLevelSkill(world.GetComponent<SkillRankComponent>(entity), slot);
}
```

아래로 교체하고 `ChampionComponent.h`를 include한다.

```cpp
bool CSkillRankSystem::TryLevelSkill(CWorld& world, EntityID entity, u8_t slot)
{
    if (!world.HasComponent<SkillRankComponent>(entity) ||
        !world.HasComponent<ChampionComponent>(entity))
    {
        return false;
    }

    return TryLevelSkill(
        world.GetComponent<SkillRankComponent>(entity),
        world.GetComponent<ChampionComponent>(entity).level,
        slot);
}
```

이 코드는 새 규칙을 만드는 것이 아니라 현재 authoritative `CommandExecutor::HandleLevelSkill`의 Q/W/E 1/3/5/7/9와 R 6/11/16 규칙을 공용 owner로 이동한다. 모든 직접 호출부는 해당 시점의 champion level을 넘긴다.

CONFIRM_NEEDED: 요구 레벨 배열 자체를 이후 `champions.json` 또는 progression policy JSON에서 조절할 필요가 있는지. 1차 수정은 현행 동작을 보존하면서 중복 구현과 우회 경로만 제거한다.

1-32. C:/Users/user/Desktop/Winters/Client/Private/GamePlay/SkillRegistry.cpp

include 영역에 추가:

```cpp
#include "Data/LoLVisualDefinitionPack.h"
```

`ApplyVerificationTiming` 위에 추가:

```cpp
    SkillDef ApplyAuthoredVisualTiming(eChampion champion, u8_t slot, SkillDef def)
    {
        const ClientData::ChampionVisualDefinition* visual =
            ClientData::FindChampionVisualDefinition(champion);
        if (!visual || slot >= ClientData::kVisualSkillSlotCount)
            return def;

        const ClientData::SkillVisualDefinition& skill = visual->skills[slot];
        const ClientData::SkillVisualStageDef& stage1 = skill.stages[0];
        def.visualPlaySpeed = stage1.animationPlaybackSpeed;
        def.visualCastFrame = stage1.castFrame;
        def.visualRecoveryFrame = stage1.recoveryFrame;
        if (skill.stageCount >= 2u)
        {
            const ClientData::SkillVisualStageDef& stage2 = skill.stages[1];
            def.stage2VisualPlaySpeed = stage2.animationPlaybackSpeed;
            def.stage2VisualCastFrame = stage2.castFrame;
            def.stage2VisualRecoveryFrame = stage2.recoveryFrame;
        }
        return def;
    }
```

기존 코드:

```cpp
    SkillDef legacy = ApplyVerificationTiming(def);
```

아래로 교체:

```cpp
    SkillDef legacy = ApplyVerificationTiming(
        ApplyAuthoredVisualTiming(champ, slot, def));
```

이 변경으로 `_Registration.cpp`의 timing은 fallback이 되고 `ChampionVisualDefs.json`이 실제 visual truth가 된다. gameplay cooldown/lock은 덮지 않는다.

1-33. C:/Users/user/Desktop/Winters/Data/LoL/ClientPublic/Visual/ChampionVisualDefs.json

17명 × 5 slot × stage의 `animationPlaybackSpeed`, `castFrame`, `recoveryFrame`을 실제 값으로 채운다. 기존 `_Registration.cpp`와 `SkillTable.cpp` 값을 1차 migration seed로 사용한다.

형식:

```json
{
  "animationPlaybackSpeed": 1.0,
  "castFrame": 6.0,
  "recoveryFrame": 14.0
}
```

CONFIRM_NEEDED: 같은 champion/slot이 `SkillTable.cpp`와 `_Registration.cpp`에서 다른 경우 어느 값을 보존할지. 자동 선택하지 않고 Model & Anim Lab에서 한 번 캡처한 값을 승인한다.

1-34. C:/Users/user/Desktop/Winters/Client/Private/UI/SkillTimingPanel.cpp

기존 `const_cast<SkillDef&>(g_SkillTable[i])` 기반 편집을 삭제한다. panel은 `ChampionVisualDefs` 기반 draft model을 편집하고 `CSkillRegistry`의 Debug visual override API에 적용한다.

기존 champion label ternary:

```cpp
            const char* champName =
                (d.champ == eChampion::IRELIA) ? "Irelia" :
                (d.champ == eChampion::YASUO) ? "Yasuo" : "?";
```

아래로 교체:

```cpp
            const char* champName = ResolveChampionDisplayName(d.champ);
```

UI에 다음 상태를 항상 표시한다.

```cpp
ImGui::TextDisabled("Visual only: playback/cast/recovery. Server action lock is not edited here.");
ImGui::Text("Source: Data/LoL/ClientPublic/Visual/ChampionVisualDefs.json");
```

`Save Visual Draft`는 `Resource/Config/Practice/practice_visual_timing_overrides.json`에 저장하고, canonical JSON 직접 덮어쓰기는 하지 않는다.

1-35. C:/Users/user/Desktop/Winters/Client/Private/UI/ChampionTuner.cpp

`kParamOptions`를 현재 14개 수동 목록으로 유지하지 말고 `eSkillEffectParamId` 전체 metadata table에서 생성한다. damage formula arrays는 scalar practice wire로는 안전하게 표현할 수 없으므로 read-only current pack 표와 `Export Formula Draft`를 먼저 제공한다.

`Reload Definitions From JSON (Server)` status 문자열:

```cpp
"Definition reload requested: champions.json / SkillEffectGameplayDefs.json / SummonerSpellGameplayDefs.json"
```

아래로 교체:

```cpp
"Definition reload requested: champions / skill effects / spells / economy / items / spawns"
```

저장 UI에는 아래 경고를 추가한다.

```cpp
ImGui::TextWrapped(
    "Practice JSON is a temporary session draft. Release truth changes only after "
    "canonical JSON validation, codegen, SimLab, and build succeed.");
```

천장 30% 데모의 완료 조건은 다음 하나다: 기획자가 Annie Q rank 배열과 한 animation cast frame을 바꾸고, draft diff와 SimLab 결과를 같은 화면에서 확인한다.

1-36. C:/Users/user/Desktop/Winters/Tools/SimLab/main.cpp

`RunItemMoveSpeedScaleProbe` 아래에 다음 probe를 추가한다.

```cpp
bool_t RunGameplayFormulaDataDrivenProbe()
{
    // 17 champions x BA/Q/W/E/R x every legal rank.
    // Pack formula -> DamageRequest -> DamagePipeline final HP delta를 golden table과 비교한다.
    // Armor 0/100/-50, AD/AP/bonusAD, maxHP/missingHP를 각각 고립해 검증한다.
    // Debug overlay와 generated pack 입력은 동일 JSON에서 같은 값을 반환해야 한다.
    return true;
}

bool_t RunBladeOfTheRuinedKingProbe()
{
    // melee와 projectile BA 각각 1회.
    // 3153 on-hit maxHP damage가 충돌 시점에 정확히 한 번 적용되고
    // lifesteal은 실제 final HP loss 기준으로 계산되는지 검증한다.
    // Ashe Q의 시각 화살, Ezreal Q, miss/immune/shield 케이스는 추가 proc 0회여야 한다.
    return true;
}

bool_t RunSkillRankGateProbe()
{
    // Q/W/E rank gate와 R 6/11/16 gate, point 보존, invalid request를 검증한다.
    return true;
}
```

위 block은 계획상의 test contract다. `return true` placeholder로 구현 완료를 주장하지 않는다. CONFIRM_NEEDED: 실제 반영 turn에서 기존 SimLab fixture와 golden table 형식을 다시 확인해 완전한 assertion/실패 로그 body로 교체한다. placeholder 상태로 build gate에 연결하지 않는다.

1-37. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/ChampionRuntimeDefaults.cpp

삭제할 코드:

```cpp
void RegisterDefaultChampionSkillScalingTables()
{
}
```

선언과 `GameRoom.cpp` 호출도 함께 삭제한다. `rg "CSkillScalingRegistry|SkillScalingTable|scalingTableId"` reader count가 0일 때 registry 파일과 project entry를 제거한다. generated compatibility 필드는 한 migration 동안만 0으로 남긴다.

1-38. C:/Users/user/Desktop/Winters/Client/Public/GamePlay/SkillRegistry.h

`ResolveVisualData` 선언 아래에 추가:

```cpp
    bool_t ApplyVisualTimingOverride(
        eChampion champion,
        u8_t slot,
        u8_t stage,
        f32_t playbackSpeed,
        f32_t castFrame,
        f32_t recoveryFrame);
```

1-39. C:/Users/user/Desktop/Winters/Client/Private/GamePlay/SkillRegistry.cpp

`ResolveVisualData` 함수 아래에 추가:

```cpp
bool_t CSkillRegistry::ApplyVisualTimingOverride(
    eChampion champion,
    u8_t slot,
    u8_t stage,
    f32_t playbackSpeed,
    f32_t castFrame,
    f32_t recoveryFrame)
{
    if (!std::isfinite(playbackSpeed) || playbackSpeed <= 0.01f ||
        !std::isfinite(castFrame) || castFrame < 0.f ||
        !std::isfinite(recoveryFrame) || recoveryFrame < castFrame)
    {
        return false;
    }

    const u32_t key = MakeSkillKey(champion, slot);
    auto legacyIt = m_LegacyMap.find(key);
    if (legacyIt == m_LegacyMap.end())
        return false;

    SkillDef& def = legacyIt->second;
    if (stage >= 2u)
    {
        if (def.stageCount < 2u)
            return false;
        def.stage2VisualPlaySpeed = playbackSpeed;
        def.stage2VisualCastFrame = castFrame;
        def.stage2VisualRecoveryFrame = recoveryFrame;
    }
    else
    {
        def.visualPlaySpeed = playbackSpeed;
        def.visualCastFrame = castFrame;
        def.visualRecoveryFrame = recoveryFrame;
    }

    m_VisualAtoms[key] = SkillDefAdapters::BuildSkillVisualData(def);
    return true;
}
```

include 영역에 추가:

```cpp
#include <cmath>
```

이 API는 client visual atom만 바꾸며 `m_GameAtoms`, server cooldown, mana, damage, action lock을 바꾸지 않는다.

1-40. C:/Users/user/Desktop/Winters/Data/Gameplay/ChampionGameData/champions.json

각 skill의 scalar `cooldownSec`, `manaCost`를 rank 배열로 교체한다. BA는 1개, Q/W/E는 5개, R은 3개다.

기존 Ezreal Q:

```json
"cooldownSec": 3.0,
"manaCost": 28.0
```

아래로 교체:

```json
"cooldownSecByRank": [3.0, 2.75, 2.5, 2.25, 2.0],
"manaCostByRank": [28.0, 31.0, 34.0, 37.0, 40.0]
```

기존 Ezreal E/R의 현재 결과를 보존하는 1차 migration 값은 다음이지만, 0초 cooldown을 shipping 값으로 승인하지 않는다.

```json
"cooldownSecByRank": [3.0, 0.0, 0.0, 0.0, 0.0]
```

```json
"cooldownSecByRank": [3.0, 0.0, 0.0]
```

CONFIRM_NEEDED: 17명 Q/W/E/R의 최종 cooldown/mana rank 배열. 현재 모든 기본 cooldown 3초와 Ezreal E/R rank 2 이후 0초는 placeholder/결함으로 분류하고 기획 승인표를 받은 뒤 canonical 값으로 교체한다.

1-41. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/ChampionGameData.h

`ChampionGameDataSkill`의 기존 scalar:

```cpp
    f32_t cooldownSec = 0.f;
    f32_t rangeMax = 0.f;
    f32_t manaCost = 0.f;
```

아래로 교체:

```cpp
    u8_t rankCount = 1u;
    f32_t cooldownSecByRank[kSkillRankValueMax]{};
    f32_t manaCostByRank[kSkillRankValueMax]{};
    f32_t rangeMax = 0.f;
```

`SkillAtomData.h`의 `kSkillRankValueMax`를 include해서 동일 상수를 사용한다.

1-42. C:/Users/user/Desktop/Winters/Tools/ChampionData/build_champion_game_data.py

`normalize_skill`의 scalar cooldown/mana 항목을 다음으로 교체한다.

```python
        "cooldownSecByRank": [
            as_float(value, f"{path}.cooldownSecByRank[{rank}]")
            for rank, value in enumerate(skill.get("cooldownSecByRank", []))
        ],
        "manaCostByRank": [
            as_float(value, f"{path}.manaCostByRank[{rank}]")
            for rank, value in enumerate(skill.get("manaCostByRank", []))
        ],
```

배열 길이는 BA 1, Q/W/E 5, R 3인지 검증한다. `append_skill`은 `rankCount`와 두 배열을 모두 생성 C++에 기록한다. scalar fallback은 migration commit 한 번만 허용하고 canonical JSON 변환 뒤 제거한다.

1-43. C:/Users/user/Desktop/Winters/Tools/LoLData/Build-LoLDefinitionPack.py

`server_skill_json`과 server C++ emitter의 scalar `cost.mana`, `cooldown.seconds` 출력을 rank 배열로 교체한다.

```python
"cost": {"manaByRank": record["manaCostByRank"]},
"cooldown": {"secondsByRank": record["cooldownSecByRank"]},
```

```python
lines.append(f"        def.cost.rankCount = {len(record['manaCostByRank'])}u;")
lines.append(f"        def.cooldown.rankCount = {len(record['cooldownSecByRank'])}u;")
for rank, value in enumerate(record["manaCostByRank"]):
    lines.append(f"        def.cost.manaCostByRank[{rank}] = {cpp_float(value)};")
for rank, value in enumerate(record["cooldownSecByRank"]):
    lines.append(f"        def.cooldown.cooldownSecByRank[{rank}] = {cpp_float(value)};")
```

`CooldownReductionPerRank`, `ManaCostPerRank`는 `SKILL_EFFECT_PARAM_IDS`, runtime overlay name table, ChampionTuner 옵션에서 삭제한다. rank 수치는 오직 `champions.json` 배열이 소유한다.

1-44. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/SkillDefGameDataAdapter.h

`BuildSkillGameAtomBundle`의 scalar 대입:

```cpp
        data.cost.manaCost = def.manaCost;
        data.cooldown.cooldownSec = def.cooldownSec;
```

아래로 교체:

```cpp
        data.cost.rankCount = 1u;
        data.cost.manaCostByRank[0] = def.manaCost;
        data.cooldown.rankCount = 1u;
        data.cooldown.cooldownSecByRank[0] = def.cooldownSec;
```

`BuildChampionGameDataSkill`의 scalar 대입:

```cpp
        data.cooldownSec = def.cooldownSec;
        data.rangeMax = def.rangeMax;
        data.manaCost = def.manaCost;
```

아래로 교체:

```cpp
        data.rankCount = 1u;
        data.cooldownSecByRank[0] = def.cooldownSec;
        data.rangeMax = def.rangeMax;
        data.manaCostByRank[0] = def.manaCost;
```

이 adapter는 legacy `SkillDef`가 rank 배열을 소유하지 않으므로 명시적으로 rank 1 fallback만 만든다. canonical server pack의 rank 배열을 덮어쓰는 경로로 사용하지 않는다.

1-45. C:/Users/user/Desktop/Winters/Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.h

기존 선언:

```cpp
    f32_t ResolveSkillCooldown(eChampion champion, u8_t slot);
```

아래로 교체:

```cpp
    f32_t ResolveSkillCooldown(eChampion champion, u8_t slot, u8_t rank = 1u);
```

1-46. C:/Users/user/Desktop/Winters/Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.cpp

기존 함수:

```cpp
    f32_t ResolveSkillCooldown(eChampion champion, u8_t slot)
    {
        if (const ChampionGameDataSkill* pSkill = FindSkill(champion, slot))
        {
            return pSkill->cooldownSec;
        }

        if (slot == static_cast<u8_t>(eSkillSlot::BasicAttack))
        {
            const StatComponent stat = BuildStat(champion, 1);
            return (stat.attackSpeed > 0.f) ? (1.f / stat.attackSpeed) : 1.f;
        }

        return 1.f;
    }
```

아래로 교체:

```cpp
    f32_t ResolveSkillCooldown(eChampion champion, u8_t slot, u8_t rank)
    {
        if (const ChampionGameDataSkill* pSkill = FindSkill(champion, slot))
        {
            const u8_t count = std::clamp<u8_t>(pSkill->rankCount, 1u, kSkillRankValueMax);
            const u8_t sanitizedRank = std::clamp<u8_t>(rank, 1u, count);
            return pSkill->cooldownSecByRank[sanitizedRank - 1u];
        }

        if (slot == static_cast<u8_t>(eSkillSlot::BasicAttack))
        {
            const StatComponent stat = BuildStat(champion, 1);
            return (stat.attackSpeed > 0.f) ? (1.f / stat.attackSpeed) : 1.f;
        }

        return 1.f;
    }
```

`GameplayDefinitionQuery`의 legacy fallback 호출에는 이미 구한 rank를 넘긴다. `ChampionRuntimeDefaults`는 기본 인자 rank 1을 사용해 기존 bootstrap 의미를 보존한다.

1-47. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/SkillRank/SkillRankSystem.h

기존 선언:

```cpp
    static bool TryLevelSkill(SkillRankComponent& ranks, u8_t slot);
```

아래로 교체:

```cpp
    static bool TryLevelSkill(
        SkillRankComponent& ranks,
        u8_t championLevel,
        u8_t slot);
```

world overload도 entity의 authoritative `ChampionComponent.level`을 읽은 뒤 위 overload를 호출하도록 시그니처와 구현을 함께 바꾼다.

1-48. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp

`HandleLevelSkill`에서 `maxRank` 계산부터 rank/point 직접 변경까지의 중복 블록을 삭제하고 아래로 교체한다. Viego의 `pProgressionRank` 선택과 마지막 point 동기화는 유지한다.

```cpp
    const u8_t championLevel = world.HasComponent<ChampionComponent>(cmd.issuerEntity)
        ? world.GetComponent<ChampionComponent>(cmd.issuerEntity).level
        : 1u;
    if (!CSkillRankSystem::TryLevelSkill(
            progressionRank,
            championLevel,
            cmd.slot))
    {
        return;
    }
```

이 교체로 현재의 Q/W/E 1/3/5/7/9, R 6/11/16 결과는 바뀌지 않고 규칙 owner만 공용 system으로 이동한다.

1-49. C:/Users/user/Desktop/Winters/Shared/GameSim/Spawn/ChampionGameplayAssembly.cpp

기존 코드:

```cpp
        for (u8_t i = 0; i < count && ranks.pointsAvailable > 0u; ++i)
            CSkillRankSystem::TryLevelSkill(ranks, kLevelOrder[i]);
```

아래로 교체:

```cpp
        for (u8_t i = 0; i < count && ranks.pointsAvailable > 0u; ++i)
        {
            CSkillRankSystem::TryLevelSkill(
                ranks,
                static_cast<u8_t>(i + 1u),
                kLevelOrder[i]);
        }
```

봇 초기 랭크도 각 레벨 시점의 gate를 통과한다. 실패한 순서가 생기면 조용히 다른 skill을 올리지 말고 SimLab에서 level order 결함으로 실패시킨다.

1-50. C:/Users/user/Desktop/Winters/Client/Private/GamePlay/SharedGameSimSmoke.cpp

기존 반복 호출의 두 번째 인자로 level 18을 추가한다.

```cpp
            (void)CSkillRankSystem::TryLevelSkill(ranks, 18u, 1u);
            (void)CSkillRankSystem::TryLevelSkill(ranks, 18u, 2u);
            (void)CSkillRankSystem::TryLevelSkill(ranks, 18u, 3u);
            (void)CSkillRankSystem::TryLevelSkill(ranks, 18u, 4u);
```

각 줄은 현재 Q/W/E/R 반복문 안의 기존 호출을 slot별로 교체한다.

1-51. C:/Users/user/Desktop/Winters/Tools/SimLab/main.cpp

Yone E 학습 호출:

```cpp
        if (!CSkillRankSystem::TryLevelSkill(ranks, static_cast<u8_t>(eSkillSlot::E)))
```

아래로 교체:

```cpp
        if (!CSkillRankSystem::TryLevelSkill(
                ranks,
                1u,
                static_cast<u8_t>(eSkillSlot::E)))
```

1-36의 `RunSkillRankGateProbe`에서는 level 1/2/3/5/6/11/16 경계를 직접 호출해 성공·실패 때 point/rank가 정확히 보존되는지 확인한다.

2. 검증

미검증:

- 이번 세션은 전수 감사와 수정 계획 문서 작성만 수행한다. gameplay code/build/runtime은 변경하지 않았다.
- Fiora/Zed/BORK와 누락 챔피언의 기획 수치가 CONFIRM_NEEDED이므로 임의 수치 구현은 미검증이다.

검증 명령:

```powershell
python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check
rg -n "CSkillScalingRegistry|SkillScalingTable|scalingTableId" Shared Server Tools/SimLab
msbuild Tools/SimLab/SimLab.vcxproj /p:Configuration=Debug /p:Platform=x64
Tools/SimLab/Bin/Debug/SimLab.exe
msbuild Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64
msbuild Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64
git diff --check
```

자동 합격 조건:

- 17명 모든 현재 구현 damage가 migration 전후 동일한 golden 결과를 낸다.
- 의도적 balance change인 Annie/Irelia rank 1 변경은 별도 golden update와 문서화된 diff로만 들어간다.
- 3153 구매 전/후 차이는 flat stat + 확정된 maxHP on-hit뿐이며 proc은 한 번이다.
- lifesteal은 shield/health floor 이후 실제 HP 감소량 기준이다.
- Q/W/E와 R skill rank gate가 point를 잃지 않고 잘못된 요청을 거부한다.
- Debug reload pack과 generated pack의 formula hash/value가 같다.
- `ChampionVisualDefs.json`의 playback/cast/recovery 세 필드가 registry visual atom에 실제 반영된다.
- animation draft가 server action lock, damage, cooldown을 바꾸지 않는다.

수동 확인:

- 서버를 띄운 Debug match에서 Armor 0/100, target MaxHP/MissingHP, lifesteal 전후를 관찰한다.
- melee BA와 Ashe/Ezreal/Kalista projectile BA에서 BORK proc 횟수를 확인한다.
- 17명 BA/Q/W/E/R을 rank별로 한 번씩 사용하고 damage event와 HP delta를 대조한다.
- Model & Anim Lab에서 cast/recovery frame 변경이 실제 hook/transition에 반영되고 idle/run 복귀가 server lock보다 먼저 gameplay 입력을 열지 않는지 확인한다.

확인 필요:

- 새 `DamageTypes.h`가 GameSim project에 포함되는지 확인한다. 헤더이므로 `.vcxproj` XML은 사용자 요청 없이 계획에 직접 추가하지 않는다.
- 삭제 대상 `SkillScalingRegistry`의 reader count 0과 project entry 제거를 확인한다.
- visual timing seed와 canonical `ChampionVisualDefs.json`이 역전되지 않는지 codegen `--check`로 확인한다.
- Fiora/Zed/BORK 및 누락 챔피언의 CONFIRM_NEEDED 값을 기획 승인한 뒤에만 behavior golden을 확정한다.
