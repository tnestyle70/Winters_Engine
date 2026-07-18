Session - 전 챔피언 리소스를 마나·기력·무자원·야스오 기류로 분리하고 HUD/아이템/서버 권위를 한 계약으로 닫는다
좌표: 신규 좌표 후보 · 축: C7 권위와 정합성, C8 검증이 병목
관련: 2026-07-18_ESSENCE_REAVER_MANAMUNE_MURAMANA_PLAN.md, 2026-07-18_GAMEFEEL_EZREAL_BASELINE_ANALYSIS_AND_REMEDIATION_RESULT.md

## 1. 결정 기록

① 문제·제약: 현재 17명 전원이 `baseMana`를 가져 리 신·제드도 파란 마나바, 요네·리븐·가렌·비에고도 리소스바가 보인다. 16.14.1 기준 기력은 최대 200/초당 10, 야스오 기류는 0→100 이동 충전이어야 한다.
② 순진한 해법의 실패: HUD에서 챔피언 ID만 분기하면 서버 비용·회복·아이템 `flatMana`/정수 약탈자/마나무네가 계속 마나로 동작하고 로컬/월드바/리플레이가 갈라진다. `bUsesPassiveResource`도 현재 야스오 아트 선택만 표현한다.
③ 메커니즘: `champions.json`의 `resourceKind/resourceRegenPerSec`를 생성 정의와 `StatComponent`까지 전달하고, 서버가 현재 리소스(`ChampionComponent::mana`)를 권위적으로 계산하며 클라이언트는 종류만 소비한다.
④ 대조: 별도 Energy/Flow 네트워크 필드를 추가하는 대신 기존 `mana/maxMana` 스냅샷 필드를 범용 현재 리소스로 유지한다. 패킷 호환과 수정 면적을 지키되 명칭 부채는 감수한다.
⑤ 대가: `mana`라는 이름과 실제 의미가 어긋나며 `StatComponent` ABI/계약 해시가 의도적으로 변한다. 향후 복수 리소스 챔피언을 넣으면 단일 `resourceKind` 설계는 틀리므로 그때 별도 리소스 채널이 필요하다.

## 2. 반영해야 하는 코드

### 소유권과 단계

- Shared/Server: 리소스 종류·최대치·기력 회복·야스오 기류/보호막·아이템 호환의 게임플레이 진실을 소유한다.
- Client/Engine: Shared 종류를 색/가시성으로 투영하고 요네 E 영혼을 프레젠테이션 엔티티로 분류한다. 권위 수치를 만들지 않는다.
- Data/Generator: 17명 분류의 단일 원본이다. 생성물은 수기 편집하지 않고 두 생성기 실행 후 반영한다.
- 적용 순서: 데이터 계약 → Shared/Server → 아이템 게이트 → 야스오 → Client/Engine HUD·요네 → 생성 → `/m:1` 빌드 → SimLab/인게임 검증.
- 병행 Claude 세션과 아래 파일이 겹치면 구현을 시작하지 않는다. 세션 handoff와 `git diff -- <파일>`을 먼저 비교하고 생성물은 양쪽 원본 변경이 합쳐진 뒤 한 번만 갱신한다.
- 이번 RESULT의 완료 범위는 리소스 종류/최대치/자연 회복/비용/HUD/아이템 게이트다. 리 신 질풍격 반환·단계별 재시전 비용과 제드 W 중첩 적중 반환은 `확인 필요`가 해소되기 전까지 완료 판정에서 제외한다.

### 2-1. C:/Users/user/Desktop/Winters/Data/Gameplay/ChampionGameData/champions.json

수치 원본은 [Riot Data Dragon 문서](https://developer.riotgames.com/docs/lol)와 16.14.1 챔피언 JSON([Lee Sin](https://ddragon.leagueoflegends.com/cdn/16.14.1/data/en_US/champion/LeeSin.json), [Zed](https://ddragon.leagueoflegends.com/cdn/16.14.1/data/en_US/champion/Zed.json), [Yasuo](https://ddragon.leagueoflegends.com/cdn/16.14.1/data/en_US/champion/Yasuo.json))을 1순위로 고정한다. 공개 JSON에 없는 야스오 이동 계수·보호막 성장치는 동일 버전 Riot game-bin의 [Yasuo 추출본](https://raw.communitydragon.org/16.14/game/data/characters/yasuo/yasuo.bin.json)을 2차 근거로 사용하고 RESULT에 출처/버전을 남긴다.

17명 분류:

| 종류 | 챔피언 | `baseMana` | `manaPerLevel` | `resourceRegenPerSec` |
|---|---|---:|---:|---:|
| Mana | IRELIA, KALISTA, SYLAS, ANNIE, ASHE, FIORA, EZREAL, JAX, MASTERYI, KINDRED | 기존값 유지 | 기존값 유지 | 0 |
| Energy | LEESIN, ZED | 200 | 0 | 10 |
| Flow | YASUO | 0 | 0 | 0 |
| None | GAREN, VIEGO, RIVEN, YONE | 0 | 0 | 0 |

Riot Data Dragon은 YONE의 내부 `partype`을 Flow로 표기하지만 스킬 비용과 영구 충전 바가 없는 구조다. 사용자 요구에 따라 Winters의 **표시/아이템 리소스 계약은 None**으로 투영하며 E 지속시간은 기존 `YoneStateComponent::fETimer`가 별도로 소유한다.

Mana 10명의 각 `stats`에서 `manaPerLevel` 바로 아래에 추가:

```json
"resourceKind": "Mana",
"resourceRegenPerSec": 0.0,
```

LEESIN/ZED의 공통 기존 코드:

```json
"baseMana": 300.0,
"manaPerLevel": 50.0,
```

아래로 교체:

```json
"baseMana": 200.0,
"manaPerLevel": 0.0,
"resourceKind": "Energy",
"resourceRegenPerSec": 10.0,
```

YASUO의 같은 두 줄을 아래로 교체:

```json
"baseMana": 0.0,
"manaPerLevel": 0.0,
"resourceKind": "Flow",
"resourceRegenPerSec": 0.0,
```

GAREN/VIEGO/RIVEN/YONE의 같은 두 줄을 아래로 교체:

```json
"baseMana": 0.0,
"manaPerLevel": 0.0,
"resourceKind": "None",
"resourceRegenPerSec": 0.0,
```

ZED Q 기존 코드:

```json
"manaCost": 75.0,
```

아래로 교체:

```json
"manaCostByRank": [75.0, 70.0, 65.0, 60.0, 55.0],
```

ZED W 기존 코드:

```json
"manaCost": 40.0,
```

아래로 교체:

```json
"manaCostByRank": [40.0, 35.0, 30.0, 25.0, 20.0],
```

ZED E 기존 코드:

```json
"manaCost": 50.0,
```

아래로 교체:

```json
"manaCost": 40.0,
```

LEESIN Q/W/E 1차 시전 비용 50, R 0과 무자원/Flow 챔피언 전 슬롯 0은 현재값을 유지한다. 리 신 재시전 비용·질풍격 에너지 반환과 제드 W 동일 대상 중첩 적중 반환은 §3 `확인 필요`의 후속 스킬 충실도 게이트로 분리한다.

### 2-2. C:/Users/user/Desktop/Winters/Data/LoL/Schemas/champions.json.schema.json

기존 코드:

```json
"baseHp", "hpPerLevel", "baseMana", "manaPerLevel",
```

아래로 교체:

```json
"baseHp", "hpPerLevel", "baseMana", "manaPerLevel", "resourceKind", "resourceRegenPerSec",
```

기존 코드:

```json
"manaPerLevel": {"$ref": "#/$defs/nonNegativeNumber"},
```

아래에 추가:

```json
"resourceKind": {"enum": ["Mana", "Energy", "None", "Flow"]},
"resourceRegenPerSec": {"$ref": "#/$defs/nonNegativeNumber"},
```

### 2-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/ChampionStatsDef.h

`struct ChampionStatsDef` 바로 위에 추가:

```cpp
enum class eChampionResourceKind : u8_t
{
    Mana = 0,
    Energy,
    None,
    Flow,
};
```

기존 코드:

```cpp
    f32_t baseMana = 300.f;
    f32_t manaPerLevel = 50.f;
```

아래로 교체:

```cpp
    f32_t baseMana = 300.f;
    f32_t manaPerLevel = 50.f;
    eChampionResourceKind resourceKind = eChampionResourceKind::Mana;
    f32_t resourceRegenPerSec = 0.f;
```

### 2-4. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/ChampionGameplayDef.h

기존 코드:

```cpp
#include "Shared/GameSim/Definitions/DefinitionIds.h"
```

아래에 추가:

```cpp
#include "Shared/GameSim/Definitions/ChampionStatsDef.h"
```

`ChampionStatBlock`의 기존 코드:

```cpp
    f32_t baseMana = 300.f;
    f32_t manaPerLevel = 50.f;
```

아래로 교체:

```cpp
    f32_t baseMana = 300.f;
    f32_t manaPerLevel = 50.f;
    eChampionResourceKind resourceKind = eChampionResourceKind::Mana;
    f32_t resourceRegenPerSec = 0.f;
```

### 2-5. C:/Users/user/Desktop/Winters/Tools/ChampionData/build_champion_game_data.py

`STAT_FIELDS` 정의 바로 위에 추가:

```python
RESOURCE_KINDS = {"Mana", "Energy", "None", "Flow"}
```

`normalize_champion`의 기존 코드:

```python
    stats = {
        key: as_float(stats_source.get(key, default), f"champions[{name}].stats.{key}")
        for key, default in STAT_FIELDS.items()
    }
```

아래로 교체:

```python
    resource_kind = stats_source.get("resourceKind")
    if resource_kind not in RESOURCE_KINDS:
        fail(f"champions[{name}].stats.resourceKind must be one of {sorted(RESOURCE_KINDS)}")
    stats = {
        "resourceKind": resource_kind,
        **{
            key: as_float(stats_source.get(key, default), f"champions[{name}].stats.{key}")
            for key, default in STAT_FIELDS.items()
        },
        "resourceRegenPerSec": as_float(
            stats_source.get("resourceRegenPerSec"),
            f"champions[{name}].stats.resourceRegenPerSec"),
    }
```

생성 C++의 기존 코드:

```python
        for key, value in champion["stats"].items():
            lines.append(f"        data.stats.{key} = {cpp_float(value)};")
```

아래로 교체:

```python
        for key, value in champion["stats"].items():
            if key == "resourceKind":
                lines.append(
                    f"        data.stats.resourceKind = eChampionResourceKind::{value};")
                continue
            lines.append(f"        data.stats.{key} = {cpp_float(value)};")
```

### 2-6. C:/Users/user/Desktop/Winters/Tools/LoLData/Build-LoLDefinitionPack.py

기존 코드:

```python
        for field, value in record["stats"].items():
            lines.append(f"        def.stats.{field} = {cpp_float(value)};")
```

아래로 교체:

```python
        for field, value in record["stats"].items():
            if field == "resourceKind":
                lines.append(
                    f"        def.stats.resourceKind = eChampionResourceKind::{value};")
                continue
            lines.append(f"        def.stats.{field} = {cpp_float(value)};")
```

### 2-7. C:/Users/user/Desktop/Winters/Server/Private/Data/RuntimeGameplayDefinitionOverlay.cpp

`kChampionStatFields`의 기존 코드:

```cpp
        { "manaPerLevel", &ChampionStatBlock::manaPerLevel },
```

아래에 추가:

```cpp
        { "resourceRegenPerSec", &ChampionStatBlock::resourceRegenPerSec },
```

`for (const auto& item : stats.items())` 첫 줄 아래에 추가:

```cpp
                    if (item.key() == "resourceKind")
                    {
                        if (!item.value().is_string())
                        {
                            outError = name + ".stats.resourceKind must be a string";
                            return false;
                        }
                        const std::string resourceKind = item.value().get<std::string>();
                        if (resourceKind == "Mana")
                            pChampion->stats.resourceKind = eChampionResourceKind::Mana;
                        else if (resourceKind == "Energy")
                            pChampion->stats.resourceKind = eChampionResourceKind::Energy;
                        else if (resourceKind == "None")
                            pChampion->stats.resourceKind = eChampionResourceKind::None;
                        else if (resourceKind == "Flow")
                            pChampion->stats.resourceKind = eChampionResourceKind::Flow;
                        else
                        {
                            outError = name + ".stats.resourceKind has an unknown value";
                            return false;
                        }
                        continue;
                    }
```

### 2-8. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/StatComponent.h

기존 코드:

```cpp
#include "../Definitions/LoLMatchContext.h"
```

아래에 추가:

```cpp
#include "../Definitions/ChampionStatsDef.h"
```

기존 코드:

```cpp
    eChampion championId = eChampion::NONE;
    u8_t level = 1;
    u16_t reservedIdentityAlignment = 0u;
```

아래로 교체:

```cpp
    eChampion championId = eChampion::NONE;
    u8_t level = 1;
    eChampionResourceKind resourceKind = eChampionResourceKind::Mana;
    u8_t reservedIdentityAlignment = 0u;
```

기존 코드:

```cpp
    f32_t hpMax = 0.f;
    f32_t manaMax = 0.f;
```

아래로 교체:

```cpp
    f32_t hpMax = 0.f;
    f32_t manaMax = 0.f;
    f32_t resourceRegenPerSec = 0.f;
```

정적 단언은 실제 컴파일러 레이아웃 실측값으로 아래처럼 갱신한다:

```cpp
static_assert(sizeof(StatComponent) == 148u);
static_assert(offsetof(StatComponent, buffMaskHash) == 4u);
static_assert(offsetof(StatComponent, abilityHaste) == 140u);
static_assert(offsetof(StatComponent, bDirty) == 144u);
```

### 2-9. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/Stat/StatSystem.h

`class CWorld;` 아래에 추가:

```cpp
struct TickContext;
```

기존 코드:

```cpp
    static void Execute(CWorld& world);
    static void Execute(CWorld& world, const GameplayDefinitionPack& definitions);
```

아래로 교체:

```cpp
    static void Execute(CWorld& world);
    static void Execute(CWorld& world, const GameplayDefinitionPack& definitions);
    static void TickResourceRegeneration(CWorld& world, const TickContext& tc);
```

### 2-10. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/Stat/StatSystem.cpp

`BuildBaseStatsFromValues`의 기존 코드:

```cpp
        stat.hpMax = CCombatFormula::ResolveStatAtLevel(def.baseHp, def.hpPerLevel, resolvedLevel);
        stat.manaMax = CCombatFormula::ResolveStatAtLevel(def.baseMana, def.manaPerLevel, resolvedLevel);
```

아래로 교체:

```cpp
        stat.hpMax = CCombatFormula::ResolveStatAtLevel(def.baseHp, def.hpPerLevel, resolvedLevel);
        stat.manaMax = CCombatFormula::ResolveStatAtLevel(def.baseMana, def.manaPerLevel, resolvedLevel);
        stat.resourceKind = def.resourceKind;
        stat.resourceRegenPerSec = def.resourceRegenPerSec;
```

아이템 합산의 기존 코드:

```cpp
            stat.hpMax += itemStats.flatHealth;
            stat.manaMax += itemStats.flatMana;
            if (pItemRuntime && pItemRuntime->slots[i].itemId == pItem->itemId)
                stat.manaMax += static_cast<f32_t>(pItemRuntime->slots[i].bonusMana);
```

아래로 교체:

```cpp
            stat.hpMax += itemStats.flatHealth;
            if (stat.resourceKind == eChampionResourceKind::Mana)
            {
                stat.manaMax += itemStats.flatMana;
                if (pItemRuntime && pItemRuntime->slots[i].itemId == pItem->itemId)
                    stat.manaMax += static_cast<f32_t>(pItemRuntime->slots[i].bonusMana);
            }
```

마나 기반 AD의 기존 코드:

```cpp
            if (pItem && pItem->maxManaBonusAdRatio > 0.f)
                stat.bonusAd += stat.manaMax * pItem->maxManaBonusAdRatio;
```

아래로 교체:

```cpp
            if (pItem &&
                stat.resourceKind == eChampionResourceKind::Mana &&
                pItem->maxManaBonusAdRatio > 0.f)
            {
                stat.bonusAd += stat.manaMax * pItem->maxManaBonusAdRatio;
            }
```

두 `Execute` 오버로드 아래에 추가:

```cpp
void CStatSystem::TickResourceRegeneration(CWorld& world, const TickContext& tc)
{
    const auto entities = DeterministicEntityIterator<StatComponent>::CollectSorted(world);
    for (EntityID entity : entities)
    {
        const StatComponent& stat = world.GetComponent<StatComponent>(entity);
        if (stat.resourceKind != eChampionResourceKind::Energy ||
            stat.resourceRegenPerSec <= 0.f ||
            !world.HasComponent<ChampionComponent>(entity) ||
            !world.HasComponent<HealthComponent>(entity))
        {
            continue;
        }

        const HealthComponent& health = world.GetComponent<HealthComponent>(entity);
        if (health.bIsDead || health.fCurrent <= 0.f)
            continue;

        ChampionComponent& champion = world.GetComponent<ChampionComponent>(entity);
        champion.maxMana = (std::max)(0.f, stat.manaMax);
        if (!std::isfinite(champion.mana))
            champion.mana = 0.f;
        champion.mana = (std::clamp)(
            champion.mana + stat.resourceRegenPerSec * tc.fDt,
            0.f,
            champion.maxMana);
    }
}
```

`<cmath>`가 없으므로 `<algorithm>` 아래에 추가:

```cpp
#include <cmath>
```

### 2-11. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomTick.cpp

기존 코드:

```cpp
	CStatSystem::Execute(m_world, definitions);
	CBuffSystem::AdvanceDurationsAfterStat(m_world, tc);
```

아래로 교체:

```cpp
	CStatSystem::Execute(m_world, definitions);
	CStatSystem::TickResourceRegeneration(m_world, tc);
	CBuffSystem::AdvanceDurationsAfterStat(m_world, tc);
```

### 2-12. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/Item/ItemEffectSystem.cpp

정수 약탈자 피해 계산의 기존 코드:

```cpp
        outResolution.manaRestore = (std::max)(
            0.f,
            bonusDamage * item->spellblade.manaRestoreRatio);
```

아래로 교체:

```cpp
        outResolution.manaRestore =
            stat.resourceKind == eChampionResourceKind::Mana
                ? (std::max)(0.f, bonusDamage * item->spellblade.manaRestoreRatio)
                : 0.f;
```

`OnDamageLanded`에서 `ItemRuntimeComponent& runtime` 아래에 추가:

```cpp
    const StatComponent* sourceStat =
        world.TryGetComponent<StatComponent>(request.source);
    const bool_t bUsesMana = sourceStat &&
        sourceStat->resourceKind == eChampionResourceKind::Mana;
```

정수 약탈자 회복의 기존 조건:

```cpp
            if (world.HasComponent<ChampionComponent>(request.source))
```

아래로 교체:

```cpp
            if (bUsesMana && world.HasComponent<ChampionComponent>(request.source))
```

마나무네 루프의 기존 코드:

```cpp
    for (u8_t slot = 0u;
        slot < inventory.count && slot < InventoryComponent::kMaxSlots;
        ++slot)
    {
        const ItemDef* item = CItemRegistry::Instance().Find(inventory.itemIds[slot]);
        if (!item || !item->manaflow.bValid)
            continue;
```

아래로 교체:

```cpp
    for (u8_t slot = 0u;
        slot < inventory.count && slot < InventoryComponent::kMaxSlots;
        ++slot)
    {
        if (!bUsesMana)
            break;

        const ItemDef* item = CItemRegistry::Instance().Find(inventory.itemIds[slot]);
        if (!item || !item->manaflow.bValid)
            continue;
```

주문검 피해/쿨다운 소비는 모든 리소스 종류에서 유지하되 마나 회복·`bonusMana` 누적·무라마나 변환만 Mana에서 허용한다.

### 2-13. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/GameplayComponents.h

`YasuoStateComponent`의 기존 코드:

```cpp
    f32_t fPassiveFlow = 100.f;
    f32_t fPassiveFlowMax = 100.f;
    f32_t fPassiveShieldRemaining = 0.f;
    f32_t fPassiveShieldMax = 100.f;
    f32_t fPassiveShieldTimer = 0.f;
```

아래로 교체:

```cpp
    f32_t fPassiveFlow = 0.f;
    f32_t fPassiveFlowMax = 100.f;
    f32_t fPassiveShieldRemaining = 0.f;
    f32_t fPassiveShieldMax = 125.f;
    f32_t fPassiveShieldTimer = 0.f;
    f32_t fPassiveLastX = 0.f;
    f32_t fPassiveLastZ = 0.f;
    bool_t bPassivePositionInitialized = false;
    u8_t reservedPassivePositionAlignment[3]{};
    u64_t uPassiveLastObservedDiscontinuityTick = 0u;
```

정적 단언을 아래로 교체:

```cpp
static_assert(sizeof(YasuoStateComponent) == 56u);
static_assert(offsetof(YasuoStateComponent, qStackTimer) == 4u);
static_assert(offsetof(YasuoStateComponent, eActiveTimer) == 12u);
static_assert(offsetof(YasuoStateComponent, fPassiveLastX) == 36u);
static_assert(offsetof(YasuoStateComponent, uPassiveLastObservedDiscontinuityTick) == 48u);
```

### 2-14. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Yasuo/YasuoGameSim.h

`namespace YasuoGameSim`의 첫 선언으로 추가:

```cpp
    f32_t ResolvePassiveFlowDistancePerPoint(u8_t level);
    f32_t ResolvePassiveShieldAmount(u8_t level);
    bool_t CanTriggerPassiveShieldFromSource(CWorld& world, EntityID source);
```

### 2-15. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Yasuo/YasuoGameSim.cpp

`namespace YasuoGameSim`의 첫 구현으로 추가:

```cpp
    f32_t ResolvePassiveFlowDistancePerPoint(u8_t level)
    {
        if (level >= 13u)
            return 0.45f;
        if (level >= 7u)
            return 0.50f;
        return 0.55f;
    }

    f32_t ResolvePassiveShieldAmount(u8_t level)
    {
        constexpr f32_t kLevelOneShield = 125.f;
        constexpr f32_t kLevelEighteenShield = 600.f;
        constexpr f32_t kLevelEighteenGrowthMultiplier = 17.f;
        const u8_t resolvedLevel = (std::clamp)(level, static_cast<u8_t>(1u), static_cast<u8_t>(18u));
        return kLevelOneShield +
            (kLevelEighteenShield - kLevelOneShield) /
                kLevelEighteenGrowthMultiplier *
                CCombatFormula::GrowthMultiplier(resolvedLevel);
    }

    bool_t CanTriggerPassiveShieldFromSource(CWorld& world, EntityID source)
    {
        return source != NULL_ENTITY &&
            (world.HasComponent<ChampionComponent>(source) ||
                world.HasComponent<JungleComponent>(source));
    }
```

상단 include에 추가:

```cpp
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Systems/Combat/CombatFormula.h"
```

`OnR`에서 `target == NULL_ENTITY` 조기 반환 블록 바로 아래에 추가:

```cpp
        if (world.HasComponent<YasuoStateComponent>(ctx.casterEntity))
        {
            YasuoStateComponent& state =
                world.GetComponent<YasuoStateComponent>(ctx.casterEntity);
            state.fPassiveFlow = state.fPassiveFlowMax;
        }
```

`Tick`의 `YasuoStateComponent` 람다에서 E 타이머 처리 바로 아래에 추가:

```cpp
                    if (!world.HasComponent<ChampionComponent>(entity) ||
                        !world.HasComponent<TransformComponent>(entity) ||
                        world.GetComponent<ChampionComponent>(entity).id != eChampion::YASUO)
                    {
                        return;
                    }

                    const Vec3 position =
                        world.GetComponent<TransformComponent>(entity).GetPosition();
                    const PositionDiscontinuityComponent* discontinuity =
                        world.TryGetComponent<PositionDiscontinuityComponent>(entity);
                    const bool_t bNewDiscontinuity = discontinuity &&
                        discontinuity->uTick > state.uPassiveLastObservedDiscontinuityTick;
                    if (discontinuity)
                    {
                        state.uPassiveLastObservedDiscontinuityTick = (std::max)(
                            state.uPassiveLastObservedDiscontinuityTick,
                            discontinuity->uTick);
                    }
                    const bool_t bDead =
                        world.HasComponent<HealthComponent>(entity) &&
                        (world.GetComponent<HealthComponent>(entity).bIsDead ||
                            world.GetComponent<HealthComponent>(entity).fCurrent <= 0.f);

                    if (!state.bPassivePositionInitialized || bNewDiscontinuity || bDead)
                    {
                        state.fPassiveLastX = position.x;
                        state.fPassiveLastZ = position.z;
                        state.bPassivePositionInitialized = true;
                        return;
                    }

                    const f32_t dx = position.x - state.fPassiveLastX;
                    const f32_t dz = position.z - state.fPassiveLastZ;
                    state.fPassiveLastX = position.x;
                    state.fPassiveLastZ = position.z;
                    const f32_t distance = std::sqrt(dx * dx + dz * dz);
                    const u8_t level = world.GetComponent<ChampionComponent>(entity).level;
                    const f32_t distancePerPoint =
                        ResolvePassiveFlowDistancePerPoint(level);
                    if (std::isfinite(distance) && distance > 0.f && distancePerPoint > 0.f)
                    {
                        state.fPassiveFlow = (std::min)(
                            state.fPassiveFlowMax,
                            state.fPassiveFlow + distance / distancePerPoint);
                    }
```

이 샘플링은 MoveSystem·E 대시·강제 이동을 한 경로에서 합산한다. 리스폰 이동이 `YasuoGameSim::Tick` 뒤에 일어나도 다음 tick에서 `uTick > uPassiveLastObservedDiscontinuityTick`으로 잡히므로 리콜/리스폰/R 순간이동은 기준점만 재설정하고 기류를 주지 않는다.

### 2-16. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/Damage/DamagePipeline.cpp

상단 include에 추가:

```cpp
#include "Shared/GameSim/Champions/Yasuo/YasuoGameSim.h"
```

`TryActivateYasuoPassiveShield`의 기존 서명/초기 gate:

```cpp
    void TryActivateYasuoPassiveShield(
        CWorld& world,
        const TickContext& tc,
        EntityID target,
        f32_t incomingDamage)
    {
        if (incomingDamage <= 0.f ||
            target == NULL_ENTITY ||
            !world.HasComponent<ChampionComponent>(target) ||
            !world.HasComponent<YasuoStateComponent>(target))
        {
            return;
        }
```

아래로 교체:

```cpp
    void TryActivateYasuoPassiveShield(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        EntityID target,
        f32_t incomingDamage)
    {
        if (incomingDamage <= 0.f ||
            !YasuoGameSim::CanTriggerPassiveShieldFromSource(world, source) ||
            target == NULL_ENTITY ||
            !world.HasComponent<ChampionComponent>(target) ||
            !world.HasComponent<YasuoStateComponent>(target))
        {
            return;
        }
```

기존 보호막 부여 코드:

```cpp
        constexpr f32_t kPassiveShieldDurationSec = 3.f;
        if (CShieldSystem::Grant(
                world,
                tc,
                target,
                state.fPassiveFlowMax,
                kPassiveShieldDurationSec))
```

아래로 교체:

```cpp
        constexpr f32_t kPassiveShieldDurationSec = 3.f;
        state.fPassiveShieldMax =
            YasuoGameSim::ResolvePassiveShieldAmount(champion.level);
        if (CShieldSystem::Grant(
                world,
                tc,
                target,
                state.fPassiveShieldMax,
                kPassiveShieldDurationSec))
```

실제 피해 경로의 기존 호출:

```cpp
    TryActivateYasuoPassiveShield(world, tc, req.target, amount);
```

아래로 교체:

```cpp
    TryActivateYasuoPassiveShield(world, tc, req.source, req.target, amount);
```

`WouldDamageKill`의 기존 코드:

```cpp
        bYasuoShieldWillActivate =
            amount > 0.f &&
            state.fPassiveShieldRemaining <= 0.f &&
            state.fPassiveFlowMax > 0.f &&
            state.fPassiveFlow >= state.fPassiveFlowMax;
        if (bYasuoShieldWillActivate)
            genericShield = state.fPassiveFlowMax;
```

아래로 교체:

```cpp
        bYasuoShieldWillActivate =
            amount > 0.f &&
            YasuoGameSim::CanTriggerPassiveShieldFromSource(world, req.source) &&
            state.fPassiveShieldRemaining <= 0.f &&
            state.fPassiveFlowMax > 0.f &&
            state.fPassiveFlow >= state.fPassiveFlowMax;
        if (bYasuoShieldWillActivate)
        {
            genericShield = YasuoGameSim::ResolvePassiveShieldAmount(
                world.GetComponent<ChampionComponent>(req.target).level);
        }
```

### 2-17. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/Damage/DamageQueueSystem.cpp

상단 include에 추가:

```cpp
#include "Shared/GameSim/Champions/Yasuo/YasuoGameSim.h"
```

기존 readiness 함수:

```cpp
    bool_t IsYasuoPassiveShieldReady(CWorld& world, EntityID target)
    {
        if (target == NULL_ENTITY ||
            !world.HasComponent<ChampionComponent>(target) ||
            !world.HasComponent<YasuoStateComponent>(target))
        {
            return false;
        }

        const ChampionComponent& champion =
            world.GetComponent<ChampionComponent>(target);
        const YasuoStateComponent& state =
            world.GetComponent<YasuoStateComponent>(target);
        return champion.id == eChampion::YASUO &&
            state.fPassiveShieldRemaining <= 0.f &&
            state.fPassiveFlowMax > 0.f &&
            state.fPassiveFlow >= state.fPassiveFlowMax;
    }
```

아래로 교체:

```cpp
    bool_t IsYasuoPassiveShieldReady(
        CWorld& world,
        const DamageRequest& request,
        f32_t& outFlowBefore)
    {
        outFlowBefore = 0.f;
        if (request.target == NULL_ENTITY ||
            !YasuoGameSim::CanTriggerPassiveShieldFromSource(world, request.source) ||
            !world.HasComponent<ChampionComponent>(request.target) ||
            !world.HasComponent<YasuoStateComponent>(request.target))
        {
            return false;
        }

        const ChampionComponent& champion =
            world.GetComponent<ChampionComponent>(request.target);
        const YasuoStateComponent& state =
            world.GetComponent<YasuoStateComponent>(request.target);
        outFlowBefore = state.fPassiveFlow;
        return champion.id == eChampion::YASUO &&
            state.fPassiveShieldRemaining <= 0.f &&
            state.fPassiveFlowMax > 0.f &&
            state.fPassiveFlow >= state.fPassiveFlowMax;
    }
```

damage loop의 기존 코드:

```cpp
        const bool_t bYasuoPassiveShieldReady =
            IsYasuoPassiveShieldReady(world, request.target);
```

아래로 교체:

```cpp
        f32_t yasuoFlowBefore = 0.f;
        const bool_t bYasuoPassiveShieldReady =
            IsYasuoPassiveShieldReady(world, request, yasuoFlowBefore);
```

기존 FX 조건:

```cpp
        if (bYasuoPassiveShieldReady && result.bWasShielded)
            EnqueueYasuoPassiveShieldVisual(world, tc, request.target);
```

아래로 교체:

```cpp
        const bool_t bYasuoPassiveFlowConsumed =
            bYasuoPassiveShieldReady &&
            world.HasComponent<YasuoStateComponent>(request.target) &&
            world.GetComponent<YasuoStateComponent>(request.target).fPassiveFlow <
                yasuoFlowBefore;
        if (bYasuoPassiveFlowConsumed)
            EnqueueYasuoPassiveShieldVisual(world, tc, request.target);
```

### 2-18. C:/Users/user/Desktop/Winters/Server/Private/Game/SnapshotBuilder.cpp

기존 코드:

```cpp
        if (world.HasComponent<YasuoStateComponent>(entity))
```

아래로 교체:

```cpp
        if (world.HasComponent<ChampionComponent>(entity) &&
            world.GetComponent<ChampionComponent>(entity).id == eChampion::YASUO &&
            world.HasComponent<YasuoStateComponent>(entity))
```

### 2-19. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp

상단 include에 추가:

```cpp
#include "Shared/GameSim/Champions/Yasuo/YasuoGameSim.h"
```

기존 코드:

```cpp
                yasuoState.fPassiveShieldMax = (champ.maxMana > 0.f) ? champ.maxMana : 100.f;
```

아래로 교체:

```cpp
                yasuoState.fPassiveShieldMax =
                    YasuoGameSim::ResolvePassiveShieldAmount(es->level());
```

### 2-20. 새 파일: C:/Users/user/Desktop/Winters/Engine/Public/Manager/UI/UIResourceKind.h

```cpp
#pragma once

#include "WintersTypes.h"

namespace Engine
{
    enum class UIResourceKind : u8_t
    {
        Mana = 0,
        Energy,
        None,
        Flow,
    };
}
```

### 2-21. C:/Users/user/Desktop/Winters/Engine/Public/Manager/UI/ActorHUDState.h

기존 코드:

```cpp
#include "ECS/Entity.h"
```

아래에 추가:

```cpp
#include "Manager/UI/UIResourceKind.h"
```

`ActorHUDState`의 `LocalEntity` 아래에 추가:

```cpp
        UIResourceKind ResourceKind = UIResourceKind::Mana;
```

`bUsesPassiveResource`는 이번 단계에서 ABI 호환용 프레젠테이션 캐시로 남기되 `ResourceKind == Flow`에서만 true로 동기화한다.

### 2-22. C:/Users/user/Desktop/Winters/Engine/Public/Manager/UI/WorldHealthBarState.h

기존 코드:

```cpp
#include "ECS/Entity.h"
```

아래에 추가:

```cpp
#include "Manager/UI/UIResourceKind.h"
```

`UIWorldHealthBarDesc`의 `Kind` 아래에 추가:

```cpp
        UIResourceKind ResourceKind = UIResourceKind::None;
```

### 2-23. C:/Users/user/Desktop/Winters/Client/Public/GameObject/Champion/Yone/Yone_Components.h

`YoneStateComponent` 바로 위에 추가:

```cpp
struct YoneSoulPresentationTag
{
};
```

### 2-24. C:/Users/user/Desktop/Winters/Client/Private/ECS/Systems/YoneSoulSpawnSystem.cpp

기존 코드:

```cpp
    HealthComponent hp{};
    if (world.HasComponent<HealthComponent>(owner))
        hp = world.GetComponent<HealthComponent>(owner);
    world.AddComponent<HealthComponent>(soul, hp);
```

아래로 교체:

```cpp
    world.AddComponent<YoneSoulPresentationTag>(
        soul,
        YoneSoulPresentationTag{});
```

영혼은 `ChampionComponent`를 렌더 경로 호환 때문에 유지하지만 `HealthComponent`, `TargetableTag`, 권위 상태는 갖지 않는다.

### 2-25. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

상단 Yone 시스템 include 아래에 추가:

```cpp
#include "GameObject/Champion/Yone/Yone_Components.h"
```

익명 namespace의 `ToLoLUIContentId` 아래에 추가:

```cpp
    Engine::UIResourceKind ToUIResourceKind(eChampionResourceKind resourceKind)
    {
        switch (resourceKind)
        {
        case eChampionResourceKind::Energy: return Engine::UIResourceKind::Energy;
        case eChampionResourceKind::None: return Engine::UIResourceKind::None;
        case eChampionResourceKind::Flow: return Engine::UIResourceKind::Flow;
        case eChampionResourceKind::Mana:
        default: return Engine::UIResourceKind::Mana;
        }
    }

    Engine::UIResourceKind ResolveUIResourceKind(
        CWorld& world,
        EntityID entity,
        eChampion champion)
    {
        if (world.HasComponent<StatComponent>(entity))
            return ToUIResourceKind(world.GetComponent<StatComponent>(entity).resourceKind);
        return ToUIResourceKind(BuildDefaultChampionStatsDef(champion).resourceKind);
    }
```

`SyncActorHUDStateToEngineUI`에서 `Champion`을 얻은 직후 추가:

```cpp
    State.ResourceKind = ResolveUIResourceKind(m_World, Entity, Champion.id);
    State.bUsesPassiveResource = State.ResourceKind == Engine::UIResourceKind::Flow;
```

기존 Passive 대입 4줄 뒤에 추가:

```cpp
    if (Champion.id == eChampion::YASUO &&
        m_World.HasComponent<YasuoStateComponent>(Entity))
    {
        const YasuoStateComponent& Yasuo =
            m_World.GetComponent<YasuoStateComponent>(Entity);
        State.PassiveValue = Yasuo.fPassiveFlow;
        State.PassiveMax = Yasuo.fPassiveFlowMax;
        State.PassiveShield = Yasuo.fPassiveShieldRemaining;
        State.PassiveShieldMax = Yasuo.fPassiveShieldMax;
    }
```

`SyncStatusPanelStateToEngineUI`, `SyncWorldHealthBarsToEngineUI`, `SyncAIResourceStateToEngine`의 각 Champion 람다 첫 줄에 추가:

```cpp
            if (m_World.HasComponent<YoneSoulPresentationTag>(Entity))
                return;
```

월드바의 `Bar.Kind` 아래에 추가:

```cpp
            Bar.ResourceKind = ResolveUIResourceKind(m_World, Entity, Champion.id);
```

### 2-26. C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/ActorHUDPanel.cpp

`IsVisibleForBind`의 Mana/Passive 분기를 아래로 교체:

```cpp
            if (bind == "hasResourceBar")
                return State.ResourceKind != UIResourceKind::None;
            if (bind == "usesManaBar")
                return State.ResourceKind == UIResourceKind::Mana;
            if (bind == "usesEnergyBar")
                return State.ResourceKind == UIResourceKind::Energy;
            if (bind == "usesPassiveBar")
                return State.ResourceKind == UIResourceKind::Flow;
            if (bind == "passiveShieldVisible")
                return State.ResourceKind == UIResourceKind::Flow && State.PassiveShield > 0.f;
```

기존 `TextColorForBind` 함수 전체를 아래로 교체:

```cpp
        ImU32 TextColorForBind(const ActorHUDState& State, const std::string& bind)
        {
            if (bind == "mpText")
            {
                if (State.ResourceKind == UIResourceKind::Energy)
                    return IM_COL32(255, 218, 71, 255);
                if (State.ResourceKind == UIResourceKind::Flow)
                    return IM_COL32(245, 248, 255, 255);
                return IM_COL32(190, 238, 255, 255);
            }
            if (bind == "gold")
                return IM_COL32(255, 217, 91, 255);
            if (bind == "level")
                return IM_COL32(245, 231, 177, 255);
            if (bind == "ad" || bind == "ap" || bind == "armor" || bind == "mr")
                return IM_COL32(220, 226, 214, 255);
            return IM_COL32(240, 245, 225, 255);
        }
```

호출부의 기존 코드:

```cpp
                TextColorForBind(layoutText.strBind),
```

아래로 교체:

```cpp
                TextColorForBind(State, layoutText.strBind),
```

`TextForBind`의 `mpText` 블록을 아래로 교체:

```cpp
            if (bind == "mpText")
            {
                if (State.ResourceKind == UIResourceKind::None)
                    return {};
                if (State.ResourceKind == UIResourceKind::Flow)
                    return ToRoundedString(State.PassiveValue) + " / " + ToRoundedString(State.PassiveMax);
                return ToRoundedString(State.Mp) + " / " + ToRoundedString(State.MaxMp);
            }
```

기본 레이아웃의 기존 코드:

```cpp
        addElement("mp.bg", "bar.empty", nullptr, 287.f, 143.f, 319.f, 14.f);
        addElement("mp.fill", "bar.mp.fill", nullptr, 289.f, 146.f, 315.f, 9.f, "mpRatio", "usesManaBar");
```

아래로 교체:

```cpp
        addElement("mp.bg", "bar.empty", nullptr, 287.f, 143.f, 319.f, 14.f, nullptr, "hasResourceBar");
        addElement("mp.fill", "bar.mp.fill", nullptr, 289.f, 146.f, 315.f, 9.f, "mpRatio", "usesManaBar");
        addElement("energy.fill", "bar.energy.fill", nullptr, 289.f, 146.f, 315.f, 9.f, "mpRatio", "usesEnergyBar");
```

### 2-27. C:/Users/user/Desktop/Winters/Client/Bin/Resource/UI/hud_atlas_manifest.json

`bar.mp.fill` 객체 바로 위에 추가:

```json
"bar.energy.fill": {
    "texture": "hud",
    "x": 1,
    "y": 876,
    "w": 460,
    "h": 17
},
```

이는 `clarity_hudatlas.png`에서 확인한 노란 스트립이다. 원본 PNG는 수정하지 않는다.

### 2-28. C:/Users/user/Desktop/Winters/Client/Bin/Resource/UI/hud_irelia_layout.json

`mp.bg` 객체에 추가:

```json
"visibleBind": "hasResourceBar",
```

`mp.fill` 객체 바로 아래에 추가:

```json
{
    "ID": "energy.fill",
    "sprite": "bar.energy.fill",
    "visibleBind": "usesEnergyBar",
    "bind": "mpRatio",
    "rect": [289.00, 146.00, 315.00, 9.00]
},
```

### 2-29. C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp

`SetActorHUDState`의 기존 코드:

```cpp
    m_ActorHUDState = *pState;
    if (ShouldUseActorHUDPassiveResource(m_ActorHUDState.iActorContentId))
    {
        m_ActorHUDState.bUsesPassiveResource = true;
        if (m_ActorHUDState.PassiveMax <= 0.f)
            m_ActorHUDState.PassiveMax = (m_ActorHUDState.MaxMp > 0.f) ? m_ActorHUDState.MaxMp : 100.f;
        if (m_ActorHUDState.PassiveShieldMax <= 0.f)
            m_ActorHUDState.PassiveShieldMax = m_ActorHUDState.PassiveMax;
    }
```

아래로 교체:

```cpp
    m_ActorHUDState = *pState;
    if (ShouldUseActorHUDPassiveResource(m_ActorHUDState.iActorContentId) &&
        m_ActorHUDState.ResourceKind == UIResourceKind::Mana)
    {
        m_ActorHUDState.ResourceKind = UIResourceKind::Flow;
    }
    m_ActorHUDState.bUsesPassiveResource =
        m_ActorHUDState.ResourceKind == UIResourceKind::Flow;
    if (m_ActorHUDState.bUsesPassiveResource)
    {
        if (m_ActorHUDState.PassiveMax <= 0.f)
            m_ActorHUDState.PassiveMax = (m_ActorHUDState.MaxMp > 0.f) ? m_ActorHUDState.MaxMp : 100.f;
        if (m_ActorHUDState.PassiveShieldMax <= 0.f)
            m_ActorHUDState.PassiveShieldMax = m_ActorHUDState.PassiveMax;
    }
```

기존 `BuildHealthBarScreenRects` 전체:

```cpp
static HealthBarScreenRects BuildHealthBarScreenRects(const ImVec2& center, f32_t width, f32_t height)
{
    HealthBarScreenRects rects{};
    rects.BarMin = ImVec2(center.x - width * 0.5f, center.y - height * 0.5f);
    rects.BarMax = ImVec2(center.x + width * 0.5f, center.y + height * 0.5f);
    rects.FillMin = ImVec2(
        rects.BarMin.x + width * 0.012f,
        rects.BarMin.y + height * 0.08f);
    rects.FillMax = ImVec2(
        rects.BarMax.x - width * 0.012f,
        rects.BarMin.y + height * 0.60f);
    rects.ManaMin = ImVec2(
        rects.BarMin.x + width * 0.012f,
        rects.BarMin.y + height * 0.68f);
    rects.ManaMax = ImVec2(
        rects.BarMax.x - width * 0.012f,
        rects.BarMax.y - height * 0.10f);
    return rects;
}
```

아래로 교체:

```cpp
static HealthBarScreenRects BuildHealthBarScreenRects(
    const ImVec2& center,
    f32_t width,
    f32_t height,
    bool_t bHasResource)
{
    HealthBarScreenRects rects{};
    const f32_t visibleHeight = bHasResource ? height : height * 0.68f;
    rects.BarMin = ImVec2(center.x - width * 0.5f, center.y - height * 0.5f);
    rects.BarMax = ImVec2(center.x + width * 0.5f, rects.BarMin.y + visibleHeight);
    rects.FillMin = ImVec2(
        rects.BarMin.x + width * 0.012f,
        rects.BarMin.y + height * 0.08f);
    rects.FillMax = ImVec2(
        rects.BarMax.x - width * 0.012f,
        bHasResource
            ? rects.BarMin.y + height * 0.60f
            : rects.BarMax.y - height * 0.08f);
    if (bHasResource)
    {
        rects.ManaMin = ImVec2(
            rects.BarMin.x + width * 0.012f,
            rects.BarMin.y + height * 0.68f);
        rects.ManaMax = ImVec2(
            rects.BarMax.x - width * 0.012f,
            rects.BarMax.y - height * 0.10f);
    }
    else
    {
        rects.ManaMin = rects.BarMax;
        rects.ManaMax = rects.BarMax;
    }
    return rects;
}
```

ImGui 경로의 기존 코드:

```cpp
        const f32_t clamped = std::clamp(Bar.fCurrent / Bar.fMaximum, 0.f, 1.f);
        const HealthBarScreenRects rects = BuildHealthBarScreenRects(screen, w, h);
```

아래로 교체:

```cpp
        const f32_t clamped = std::clamp(Bar.fCurrent / Bar.fMaximum, 0.f, 1.f);
        const bool_t bHasResource =
            Bar.ResourceKind != UIResourceKind::None && Bar.fManaMaximum > 0.f;
        const HealthBarScreenRects rects =
            BuildHealthBarScreenRects(screen, w, h, bHasResource);
```

ImGui의 기존 리소스 블록 전체를 아래로 교체:

```cpp
        if (bHasResource)
        {
            const f32_t resourceRatio = std::clamp(
                Bar.fManaCurrent / Bar.fManaMaximum,
                0.f,
                1.f);
            const ImU32 resourceFillColor =
                Bar.ResourceKind == UIResourceKind::Energy
                    ? IM_COL32(235, 190, 48, 255)
                    : Bar.ResourceKind == UIResourceKind::Flow
                        ? IM_COL32(235, 241, 248, 255)
                        : IM_COL32(36, 125, 226, 255);
            const ImU32 resourceTopColor =
                Bar.ResourceKind == UIResourceKind::Energy
                    ? IM_COL32(255, 237, 139, 92)
                    : Bar.ResourceKind == UIResourceKind::Flow
                        ? IM_COL32(255, 255, 255, 92)
                        : IM_COL32(108, 210, 255, 92);

            pDraw->AddRectFilled(rects.ManaMin, rects.ManaMax, IM_COL32(6, 13, 25, 235));
            if (resourceRatio > 0.f)
            {
                const f32_t resourceW =
                    (rects.ManaMax.x - rects.ManaMin.x) * resourceRatio;
                const ImVec2 resourceFillMax(
                    rects.ManaMin.x + resourceW,
                    rects.ManaMax.y);
                pDraw->AddRectFilled(
                    rects.ManaMin,
                    resourceFillMax,
                    resourceFillColor);
                pDraw->AddRectFilled(
                    rects.ManaMin,
                    ImVec2(resourceFillMax.x, rects.ManaMin.y + 1.f),
                    resourceTopColor);
            }
        }
```

RHI 경로의 기존 코드:

```cpp
        const f32_t clamped = std::clamp(Bar.fCurrent / Bar.fMaximum, 0.f, 1.f);
        const HealthBarScreenRects rects = BuildHealthBarScreenRects(s, w, h);
```

아래로 교체:

```cpp
        const f32_t clamped = std::clamp(Bar.fCurrent / Bar.fMaximum, 0.f, 1.f);
        const bool_t bHasResource =
            Bar.ResourceKind != UIResourceKind::None && Bar.fManaMaximum > 0.f;
        const HealthBarScreenRects rects =
            BuildHealthBarScreenRects(s, w, h, bHasResource);
```

RHI의 기존 리소스 블록 전체를 아래로 교체:

```cpp
        if (bHasResource)
        {
            const f32_t resourceRatio = std::clamp(
                Bar.fManaCurrent / Bar.fManaMaximum,
                0.f,
                1.f);
            const Vec4 resourceFillColor =
                Bar.ResourceKind == UIResourceKind::Energy
                    ? Vec4(0.92f, 0.75f, 0.19f, 1.f)
                    : Bar.ResourceKind == UIResourceKind::Flow
                        ? Vec4(0.92f, 0.95f, 0.97f, 1.f)
                        : Vec4(0.14f, 0.49f, 0.89f, 1.f);
            const Vec4 resourceTopColor =
                Bar.ResourceKind == UIResourceKind::Energy
                    ? Vec4(1.f, 0.93f, 0.55f, 0.36f)
                    : Bar.ResourceKind == UIResourceKind::Flow
                        ? Vec4(1.f, 1.f, 1.f, 0.36f)
                        : Vec4(0.42f, 0.82f, 1.f, 0.36f);

            m_pRHIUIRenderer->DrawImage(
                nullptr,
                rects.ManaMin.x,
                rects.ManaMin.y,
                rects.ManaMax.x - rects.ManaMin.x,
                rects.ManaMax.y - rects.ManaMin.y,
                uvFull,
                Vec4(0.024f, 0.05f, 0.098f, 0.92f));
            if (resourceRatio > 0.f)
            {
                const f32_t resourceW =
                    (rects.ManaMax.x - rects.ManaMin.x) * resourceRatio;
                m_pRHIUIRenderer->DrawImage(
                    nullptr,
                    rects.ManaMin.x,
                    rects.ManaMin.y,
                    resourceW,
                    rects.ManaMax.y - rects.ManaMin.y,
                    uvFull,
                    resourceFillColor);
                m_pRHIUIRenderer->DrawImage(
                    nullptr,
                    rects.ManaMin.x,
                    rects.ManaMin.y,
                    resourceW,
                    1.0f,
                    uvFull,
                    resourceTopColor);
            }
        }
```

### 2-30. C:/Users/user/Desktop/Winters/Tools/SimLab/main.cpp

`CONFIRM_NEEDED`: 구현 직전 Claude 세션의 `Tools/SimLab/main.cpp` 변경과 현재 `RunServerAuthoritativeShieldProbe`, item/data-contract probe의 최신 helper/expected hash를 다시 읽어야 완전한 컴파일 가능 테스트 본문을 쓸 수 있다. 현 SimLab에는 아래 계약을 자동으로 잡는다는 근거가 없으므로 단순 실행만으로 완료 처리하지 않는다.

추가할 probe 계약:

- `RunChampionResourceKindProbe`: 17명 종류 10/2/1/4, Energy 최대 200·10/s·dead 0·clamp·same-seed, None/Flow max 0을 단언한다.
- `RunResourceItemCompatibilityProbe`: 정수 약탈자 피해는 유지하되 비Mana 회복 0, `flatMana`/마나무네 bonus/변환/최대마나 AD는 비Mana에서 0을 단언한다.
- `RunYasuoFlowProbe`: 시작 0, 레벨 경계 0.55/0.50/0.45, 일반 이동/E 대시, R 100, R·리콜·리스폰 discontinuity 0 gain을 단언한다.
- 기존 `RunServerAuthoritativeShieldProbe`: 테스트 setup에서 Flow를 명시적으로 100으로 채우고 챔피언/정글만 125→600 보호막·Flow 0·FX 1회, 미니언/포탑은 미발동을 단언하도록 교체한다.
- Yone soul tag/HUD는 Client/Engine 전용이라 SimLab에 억지로 링크하지 않는다. `--banpick-smoke` 또는 전용 client smoke가 없으면 §3에 자동 게이트 없음으로 남기고 인게임 캡처로 검증한다.

## 3. 검증

### 예측

- 데이터 분류는 정확히 Mana 10 / Energy 2 / Flow 1 / None 4이며 리 신·제드는 레벨과 무관하게 최대 200, 생존 중 초당 10 회복, 사망 중 0 회복, 200에서 clamp된다.
- ZED Q/W는 스킬 랭크별 공식 비용, E는 40을 차감한다. None/Flow의 비용 0은 `InsufficientResource`를 만들지 않는다.
- 정수 약탈자 주문검 피해는 Energy/None/Flow에서도 발생하지만 마나 회복은 Mana만, `flatMana`·마나무네 누적/변환·최대 마나 비례 AD도 Mana만 발생한다.
- 요네·리븐뿐 아니라 누락됐던 가렌·비에고의 로컬/월드 리소스바와 텍스트가 사라진다. 리 신·제드는 atlas y=876 노란색, 야스오는 흰색 Flow로 보인다.
- 요네 E 영혼은 모델/FX가 남지만 체력바·TAB 중복 행·AIResourceState가 없고 본체 HUD는 유지된다.
- 야스오는 0 Flow로 시작하고 이동 거리 0.55/0.50/0.45 world-unit당 1을 얻어 100에서 멈춘다. 리콜/리스폰/R 순간이동은 충전하지 않고 성공한 R은 즉시 100을 만든다.
- Flow 100에서 적 챔피언/정글 몬스터 피해만 레벨 1→18에서 125→600 보호막을 3초 부여하고 Flow를 0으로 만든다. 미니언/포탑/자해는 발동하지 않는다.
- 생성 해시(`0x8CBBD326`), 팩 해시(`0x0A70BCD7`), ordered contract(`0x1F416E7039DD7C48`)는 데이터/ABI 입력 변화로 의도 변동한다. 새 실측값만 RESULT와 SimLab 기대값에 기록한다.
- Bot AI는 GameCommand 생산자이며 게임플레이 진실을 직접 변경하지 않는다. 봇은 동일 비용/회복/서버 판정을 소비하며 MidDefense 결정성이 의도 밖으로 변하면 회귀다.

### 검증 명령

```powershell
git status --short
git diff --check
python Tools/LoLData/Test-ChampionGameDataSchema.py
python Tools/ChampionData/build_champion_game_data.py
python Tools/ChampionData/build_champion_game_data.py --check
python Tools/LoLData/Build-LoLDefinitionPack.py
python Tools/LoLData/Build-LoLDefinitionPack.py --check
msbuild Engine/Include/Engine.vcxproj /m:1 /p:Configuration=Debug /p:Platform=x64
.\UpdateLib.bat
msbuild Winters.sln /m:1 /p:Configuration=Debug /p:Platform=x64
Tools\Bin\Debug\SimLab.exe 600 1234
Tools\Bin\Debug\SimLab.exe 600 1234
```

두 SimLab 실행은 same-seed 전체 해시가 동일해야 한다. 해시 고정 단언이 실패하면 데이터/컴포넌트 계약 입력임을 확인한 뒤 기대값을 한 번만 갱신하고 재실행한다.

인게임 수동 게이트:

1. 리 신/제드: 스킬 사용 직후 노란 로컬·월드바 감소, 1초에 약 10 회복, 죽어 있는 동안 정지, 리콜/리스폰 후 최대치.
2. 요네/리븐/가렌/비에고: 로컬 HUD 배경·fill·`0 / 0` 텍스트와 월드 리소스바가 모두 없음.
3. 요네 E: 영혼 모델/FX는 보이지만 영혼 머리 위 체력바와 TAB 중복 줄은 없음.
4. 야스오: 정지 시 0 유지, 걷기/E 대시로 충전, R 성공 시 100, 리콜/R 위치 점프로 추가 충전 없음, 챔피언/정글 피격 시 흰 바 0과 보호막 발생.
5. 아이템: Energy/None/Flow에게 정수 약탈자 회복 0, 마나무네 보너스 마나/변환 0; Mana 챔피언의 기존 동작은 유지.

### 미검증

- 계획서 작성 단계에서는 소스/생성물/빌드를 변경하거나 실행하지 않았다. 구현 후 위 명령과 실제 F5 교차 체감으로 닫는다.
- Riot 클라이언트 픽셀과 월드바 RGB의 완전 일치는 실측 없음. atlas 노란 스트립은 저장소 원본 좌표를 사용하고 월드바는 같은 계열 색을 사용한다.

### 확인 필요

- 구현 직전 Claude 세션이 수정한 동일 파일/생성물 diff와 이 계획의 기준점을 재대조한다. 충돌 시 원본 계약 변경을 먼저 병합하고 이 문서의 no-op 앵커를 최신 코드로 갱신한다.
- 리 신 질풍격: [16.14 Lee Sin game-bin 추출본](https://raw.communitydragon.org/16.14/game/data/characters/leesin/leesin.bin.json)의 레벨 1/7/13 반환 10/15/20, 첫 타 2배와 두 번의 기본 공격 소비를 이번 리소스 기반 작업에 포함할지 별도 스킬 충실도 좌표로 분리할지 결정해야 한다. 공격 속도 보너스는 별도 범위다.
- 제드 W 동일 대상 중첩 적중 에너지 반환 30/35/40/45/50은 [16.14 Zed game-bin 추출본](https://raw.communitydragon.org/16.14/game/data/characters/zed/zed.bin.json)에서 확인되지만 현재 Q projectile/E dedup 경로가 cast/source 상관 ID를 보존하지 않는다. 추측으로 환급하지 말고 cast correlation 설계를 별도 계획으로 확정한 뒤 추가한다.
- 리 신 Q/W/E 재시전의 현재 공식 에너지 비용은 Data Dragon 단일 `cost`만으로 단계별 판정이 불충분하다. 16.14 game-bin spell stage cost를 재확인한 뒤 스키마가 `manaCostByStage`를 요구하는지 판결한다.
- 새 Engine public header의 프로젝트 포함 여부 확인과 Engine public 변경 후 `UpdateLib.bat` 동기화가 필요하다. `EngineSDK/inc`는 직접 편집하지 않는다.
- `StatComponent`와 `YasuoStateComponent` 실제 MSVC offset은 첫 빌드에서 확인한다. 예상 148/56과 다르면 임의 padding이 아니라 컴파일러 실측에 맞춰 정적 단언/예약 필드를 조정한다.
- 챔피언 소환물이 야스오 보호막의 “챔피언 피해”로 취급돼야 하는지는 owner attribution 정책이 현재 DamageRequest에 없으므로 `CONFIRM_NEEDED`다. 이번 기본 gate는 직접 ChampionComponent source와 JungleComponent source만 허용한다.

적용 후 `C:/Users/user/Desktop/Winters/.md/plan/2026-07-18_CHAMPION_RESOURCE_DIFFERENTIATION_RESULT.md`에 예측 vs 실측, 판결, ⑤ 갱신만 기록한다.
