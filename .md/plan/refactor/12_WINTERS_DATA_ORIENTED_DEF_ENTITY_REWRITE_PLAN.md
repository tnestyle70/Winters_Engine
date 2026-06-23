Session - Winters를 JSON authoring에서 불변 Def pack을 만들고 EntityHandle로 ECS 상태를 조립하는 Data-Driven/Data-Oriented 구조로 완전 전환한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Data/LoL

기존 구조:

```text
Data/Gameplay/ChampionGameData/champions.json
  champion identity
  gameplay stats
  skill gameplay
  summoner spell
  visual yaw
  animation playback speed
  cast/recovery frame
```

아래로 교체:

```text
Data/LoL/
  ServerPrivate/
    Gameplay/
      ChampionGameplayDefs.json
      SkillGameplayDefs.json
      SummonerSpellGameplayDefs.json
      MinionGameplayDefs.json
      StructureGameplayDefs.json
      JungleGameplayDefs.json
      ItemGameplayDefs.json
      RuneGameplayDefs.json
    Policy/
      SpawnPolicyDefs.json
      LoadoutPolicyDefs.json
      MinionWavePolicyDefs.json
      BotPolicyDefs.json
      GameplayPolicyBindings.json
  ClientPublic/
    Visual/
      ChampionVisualDefs.json
      SkillVisualDefs.json
      VisualEventDefs.json
    GameHint/
      PublicGameplayHints.json
  SharedContract/
    DefinitionManifest.json
    ActionCueManifest.json
  Test/
    SmokeScenarioDefs.json
```

파일을 필드 하나마다 나누지 않는다. 아래 네 조건이 같은 값은 하나의 Def에 응집한다.

```text
같은 직군이 소유한다.
같은 검증 규칙을 가진다.
같은 시점에 로드된다.
같은 런타임 경로에서 함께 읽힌다.
```

`ChampionGameplayDefs.json`의 한 항목은 아래 형태로 생성한다. 기존 17개 champion 값은
`Tools/LoLData/Build-LoLDefinitionPack.py`가 현재 source에서 손실 없이 이관한다.

```json
{
  "key": "irelia",
  "version": 1,
  "stats": {
    "baseHp": 600.0,
    "hpPerLevel": 100.0,
    "baseMana": 300.0,
    "manaPerLevel": 50.0,
    "baseAd": 65.0,
    "adPerLevel": 3.5,
    "baseArmor": 30.0,
    "armorPerLevel": 4.0,
    "baseMr": 30.0,
    "mrPerLevel": 1.25,
    "baseAttackSpeed": 0.9,
    "attackSpeedRatio": 0.9,
    "attackSpeedPerLevel": 0.025,
    "baseAttackRange": 2.1,
    "baseMoveSpeed": 5.0,
    "navArriveRadius": 0.15,
    "spatialRadius": 0.75,
    "sightRange": 19.0
  },
  "skillKeys": [
    "irelia.basic_attack",
    "irelia.q",
    "irelia.w",
    "irelia.e",
    "irelia.r"
  ],
  "passivePolicyKey": "irelia.passive"
}
```

`SkillGameplayDefs.json`의 한 항목은 아래 형태로 생성한다.

```json
{
  "key": "irelia.q",
  "target": {
    "shape": ["Unit"],
    "resolvePolicy": "Contextual"
  },
  "cost": { "mana": 0.0 },
  "cooldown": { "seconds": 0.6 },
  "range": { "max": 6.0 },
  "stage": {
    "count": 1,
    "inputWindowSeconds": 0.0,
    "actionLockSeconds": [0.36]
  },
  "facing": ["TowardsTarget"],
  "effectPolicyKey": "irelia.q",
  "replicatedCueKey": "irelia.q.accepted"
}
```

삭제할 데이터:

```text
ChampionGameplayDef 안의 displayName/model/texture/animation/FX
ChampionGameplayDef 안의 spawn position/scale/loadout
SkillGameplayDef 안의 animation key/playback speed/cast frame/recovery frame
ChampionGameplayDef 안의 summoner spells
ClientPublic 안의 server-only scaling table/hidden policy/account/MMR/payment data
```

1-2. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/DefinitionIds.h

새 파일:

```cpp
#pragma once

#include "WintersTypes.h"

struct DefinitionKey
{
    u32_t value = 0;

    bool_t IsValid() const { return value != 0; }
};

struct ChampionDefId
{
    u16_t value = 0;

    bool_t IsValid() const { return value != 0; }
};

struct SkillDefId
{
    u16_t value = 0;

    bool_t IsValid() const { return value != 0; }
};

struct SummonerSpellDefId
{
    u16_t value = 0;

    bool_t IsValid() const { return value != 0; }
};

struct GameplayPolicyId
{
    u16_t value = 0;

    bool_t IsValid() const { return value != 0; }
};

struct ItemDefId
{
    u16_t value = 0;

    bool_t IsValid() const { return value != 0; }
};

struct RuneDefId
{
    u16_t value = 0;

    bool_t IsValid() const { return value != 0; }
};

struct ScalingDefId
{
    u16_t value = 0;

    bool_t IsValid() const { return value != 0; }
};

struct RewardDefId
{
    u16_t value = 0;

    bool_t IsValid() const { return value != 0; }
};

struct MinionDefId
{
    u16_t value = 0;

    bool_t IsValid() const { return value != 0; }
};

struct StructureDefId
{
    u16_t value = 0;

    bool_t IsValid() const { return value != 0; }
};

struct JungleDefId
{
    u16_t value = 0;

    bool_t IsValid() const { return value != 0; }
};

inline constexpr ChampionDefId kInvalidChampionDefId{};
inline constexpr SkillDefId kInvalidSkillDefId{};
inline constexpr SummonerSpellDefId kInvalidSummonerSpellDefId{};
inline constexpr GameplayPolicyId kInvalidGameplayPolicyId{};
inline constexpr ItemDefId kInvalidItemDefId{};
inline constexpr RuneDefId kInvalidRuneDefId{};
inline constexpr ScalingDefId kInvalidScalingDefId{};
inline constexpr RewardDefId kInvalidRewardDefId{};
inline constexpr MinionDefId kInvalidMinionDefId{};
inline constexpr StructureDefId kInvalidStructureDefId{};
inline constexpr JungleDefId kInvalidJungleDefId{};
```

`DefinitionKey`는 JSON, manifest, snapshot에서 공유하는 안정 키다.
`ChampionDefId`, `SkillDefId`는 검증된 pack 내부의 dense index다.
`EntityHandle`은 월드 안의 살아있는 인스턴스다.
`NetEntityId`는 네트워크 인스턴스다.

아래 결합은 금지한다.

```text
JSON에 EntityHandle 저장
네트워크에 EntityHandle 전송
DefinitionKey를 EntityID로 사용
DefId를 재사용 가능한 entity lifetime으로 사용
매 프레임 문자열 key lookup
```

1-3. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/SkillAtomData.h

기존 코드:

```cpp
struct SkillGameAtomBundle
{
    bool_t bValid = false;
    SkillSlotBinding slot{};
    SkillTargetSpec target{};
    SkillCostSpec cost{};
    SkillCooldownSpec cooldown{};
    SkillRangeSpec range{};
    SkillStageSpec stage{};
    SkillFacingSpec facing{};
    SkillEffectSpec effect{};
};
```

아래로 교체:

```cpp
struct SkillGameplayDef
{
    DefinitionKey key{};
    SkillDefId id{};
    SkillTargetSpec target{};
    SkillCostSpec cost{};
    SkillCooldownSpec cooldown{};
    SkillRangeSpec range{};
    SkillStageSpec stage{};
    SkillFacingSpec facing{};
    SkillEffectSpec effect{};
};
```

파일 상단 기존 include 아래에 추가:

```cpp
#include "Shared/GameSim/Definitions/DefinitionIds.h"
```

기존 코드:

```cpp
struct SkillEffectSpec
{
    u16_t scalingTableId = 0;
    u32_t gameplayPolicyId = 0;
    u32_t replicatedCueId = 0;
};
```

아래로 교체:

```cpp
struct SkillEffectSpec
{
    ScalingDefId scaling{};
    GameplayPolicyId gameplayPolicy{};
    u32_t replicatedCueId = 0;
};
```

삭제할 코드:

```cpp
struct SkillSlotBinding
{
    bool_t bValid = false;
    eChampion champion = eChampion::END;
    u8_t slot = 0;
    u16_t skillId = 0;
};
```

스킬 정의는 champion/slot을 소유하지 않는다. champion의 loadout이 slot과 `SkillDefId`의 관계를 소유한다.

1-4. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/ChampionGameplayDef.h

새 파일:

```cpp
#pragma once

#include "Shared/GameSim/Definitions/ChampionStatsDef.h"
#include "Shared/GameSim/Definitions/DefinitionIds.h"
#include "Shared/GameSim/Definitions/SkillTypes.h"
#include "WintersTypes.h"

inline constexpr u8_t kChampionSkillSlotCount =
    static_cast<u8_t>(eSkillSlot::SLOT_END);

struct ChampionGameplayDef
{
    DefinitionKey key{};
    ChampionDefId id{};
    ChampionStatsDef stats{};
    SkillDefId skills[kChampionSkillSlotCount] = {};
    GameplayPolicyId passivePolicy{};
};
```

`ChampionGameplayDef`에는 spawn, loadout, visual, runtime state를 넣지 않는다.

1-5. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/GameplayDefinitionPack.h

새 파일:

```cpp
#pragma once

#include "Shared/GameSim/Definitions/ChampionGameplayDef.h"
#include "Shared/GameSim/Definitions/ObjectGameplayDef.h"
#include "Shared/GameSim/Definitions/SkillAtomData.h"
#include "Shared/GameSim/Definitions/SummonerSpellGameData.h"
#include "WintersTypes.h"

struct GameplayDefinitionPack
{
    u32_t schemaVersion = 0;
    u32_t contentHash = 0;
    const ChampionGameplayDef* champions = nullptr;
    u16_t championCount = 0;
    const SkillGameplayDef* skills = nullptr;
    u16_t skillCount = 0;
    const SummonerSpellGameplayDef* summonerSpells = nullptr;
    u16_t summonerSpellCount = 0;
    const MinionGameplayDef* minions = nullptr;
    u16_t minionCount = 0;
    const StructureGameplayDef* structures = nullptr;
    u16_t structureCount = 0;
    const JungleGameplayDef* jungle = nullptr;
    u16_t jungleCount = 0;

    const ChampionGameplayDef* TryGet(ChampionDefId id) const
    {
        return id.IsValid() && id.value <= championCount
            ? &champions[id.value - 1]
            : nullptr;
    }

    const SkillGameplayDef* TryGet(SkillDefId id) const
    {
        return id.IsValid() && id.value <= skillCount
            ? &skills[id.value - 1]
            : nullptr;
    }

    ChampionDefId ResolveChampion(DefinitionKey key) const
    {
        if (!key.IsValid())
            return kInvalidChampionDefId;

        for (u16_t index = 0; index < championCount; ++index)
        {
            if (champions[index].key.value == key.value)
                return champions[index].id;
        }

        return kInvalidChampionDefId;
    }
};
```

pack은 application/match 시작 전에 한 번 검증하고 match 동안 불변으로 유지한다.
Hot Reload를 위해 generation, observer, shared pointer를 지금 추가하지 않는다.
`ResolveChampion`의 선형 key lookup은 spawn/load 경계에서만 허용한다.
frame system은 component가 가진 dense DefId로 `TryGet`만 호출한다.

1-6. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Components/ChampionDefinitionComponent.h

새 파일:

```cpp
#pragma once

#include "Shared/GameSim/Definitions/DefinitionIds.h"

struct ChampionDefinitionComponent
{
    DefinitionKey key{};
    ChampionDefId defId{};
};
```

1-7. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Components/SkillLoadoutComponent.h

새 파일:

```cpp
#pragma once

#include "Shared/GameSim/Definitions/ChampionGameplayDef.h"

struct SkillLoadoutComponent
{
    SkillDefId skills[kChampionSkillSlotCount] = {};
};
```

`SkillLoadoutComponent`는 불변 정의 참조다.
`SkillStateComponent`는 cooldown/stage window 같은 가변 상태다.
`SkillRankComponent`는 rank/point 같은 가변 성장 상태다.
세 역할을 다시 합치지 않는다.

1-8. C:/Users/tnest/Desktop/Winters/Engine/Public/ECS/Components/GameplayComponents.h

기존 코드:

```cpp
struct ChampionComponent
{
    eChampion id = eChampion::END;
    eTeam     team = eTeam::Blue;
    f32_t     hp = 100.f;
    f32_t     maxHp = 100.f;
    f32_t     mana = 100.f;
    f32_t     maxMana = 100.f;
    f32_t shield = 0.f;
    f32_t     moveSpeed = 8.f;
    f32_t     cooldowns[4]{ 0.f, 0.f, 0.f, 0.f };
    uint8_t   level = 1;
};
```

삭제할 범위:
모든 reader가 아래 단일 owner로 이동한 뒤 `ChampionComponent` 전체를 삭제.

삭제 전환표:

```text
ChampionComponent.id            -> ChampionDefinitionComponent.key/defId
ChampionComponent.team          -> TeamComponent.team
ChampionComponent.hp/maxHp      -> HealthComponent
ChampionComponent.mana/maxMana  -> ManaComponent
ChampionComponent.shield        -> ShieldComponent 또는 기존 shield truth owner
ChampionComponent.moveSpeed     -> StatComponent.moveSpeed
ChampionComponent.cooldowns     -> SkillStateComponent
ChampionComponent.level         -> ExperienceComponent.level
```

`ChampionComponent` reader 58개를 한 번에 깨지 않는다. reader별 parity 전환 후 마지막에 struct를 삭제한다.

1-9. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Components/StatComponent.h

삭제할 코드:

```cpp
eChampion championId = eChampion::NONE;
u8_t level = 1;
```

삭제 조건:

```text
champion identity reader가 ChampionDefinitionComponent를 읽는다.
level reader가 ExperienceComponent.level을 읽는다.
CStatSystem::BuildBaseStats는 level을 매개변수로만 받고 StatComponent에 복제하지 않는다.
```

1-10. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/ChampionStatsDef.h

삭제할 코드:

```cpp
eChampion championId = eChampion::NONE;
```

`ChampionStatsDef`는 수치 정의만 소유한다. champion identity는
`ChampionGameplayDef.key/id`가 소유한다.

1-11. C:/Users/tnest/Desktop/Winters/Engine/Public/ECS/World.h

`CWorld::AddComponent(EntityID, ...)` 바로 아래에 추가:

```cpp
template<typename T> T* TryAddComponent(EntityHandle handle, const T& component = T{})
{
    EntityID entity = NULL_ENTITY;
    if (!TryResolveEntity(handle, entity))
        return nullptr;
    return &AddComponent<T>(entity, component);
}
```

기존 `TryGetComponent(EntityHandle)` overload는 그대로 재사용한다.
Factory/API 경계는 `EntityHandle`을 유지하고, system dense iteration 내부는 `EntityID`를 유지한다.

1-12. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Spawn/ChampionGameplayFactory.h

새 파일:

```cpp
#pragma once

#include "ECS/Entity.h"
#include "Shared/GameSim/Definitions/GameplayDefinitionPack.h"
#include "WintersMath.h"
#include "WintersTypes.h"

class CWorld;

struct ChampionGameplaySpawnArgs
{
    ChampionDefId championDefId{};
    u8_t team = 0;
    u8_t level = 1;
    Vec3 position{};
};

class CChampionGameplayFactory final
{
public:
    static EntityHandle Spawn(
        CWorld& world,
        const GameplayDefinitionPack& definitions,
        const ChampionGameplaySpawnArgs& args);

private:
    CChampionGameplayFactory() = delete;
};
```

1-13. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Spawn/ChampionGameplayFactory.cpp

새 파일:

```cpp
#include "Shared/GameSim/Spawn/ChampionGameplayFactory.h"

#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "Shared/GameSim/Components/ChampionDefinitionComponent.h"
#include "Shared/GameSim/Components/ChampionTag.h"
#include "Shared/GameSim/Components/ManaComponent.h"
#include "Shared/GameSim/Components/SkillLoadoutComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Components/TeamComponent.h"
#include "Shared/GameSim/Systems/Experience/ExperienceSystem.h"
#include "Shared/GameSim/Systems/SkillRank/SkillRankSystem.h"
#include "Shared/GameSim/Systems/Stat/StatSystem.h"

EntityHandle CChampionGameplayFactory::Spawn(
    CWorld& world,
    const GameplayDefinitionPack& definitions,
    const ChampionGameplaySpawnArgs& args)
{
    const ChampionGameplayDef* pDef = definitions.TryGet(args.championDefId);
    if (!pDef)
        return NULL_ENTITY_HANDLE;

    const EntityHandle handle = world.CreateEntityHandle();
    const EntityID entity = world.ResolveEntity(handle);
    if (entity == NULL_ENTITY)
        return NULL_ENTITY_HANDLE;

    TransformComponent transform{};
    transform.SetPosition(args.position);
    world.AddComponent<TransformComponent>(entity, transform);

    world.AddComponent<ChampionTag>(entity);
    world.AddComponent<ChampionDefinitionComponent>(
        entity,
        ChampionDefinitionComponent{ pDef->key, pDef->id });

    TeamComponent team{};
    team.team = static_cast<eTeam>(args.team);
    world.AddComponent<TeamComponent>(entity, team);

    StatComponent stat = CStatSystem::BuildBaseStats(pDef->stats, args.level);
    world.AddComponent<StatComponent>(entity, stat);

    HealthComponent health{};
    health.fCurrent = stat.hpMax;
    health.fMaximum = stat.hpMax;
    world.AddComponent<HealthComponent>(entity, health);

    ManaComponent mana{};
    mana.fCurrent = stat.manaMax;
    mana.fMaximum = stat.manaMax;
    world.AddComponent<ManaComponent>(entity, mana);

    SkillLoadoutComponent loadout{};
    for (u8_t slot = 0; slot < kChampionSkillSlotCount; ++slot)
        loadout.skills[slot] = pDef->skills[slot];
    world.AddComponent<SkillLoadoutComponent>(entity, loadout);
    world.AddComponent<SkillStateComponent>(entity);

    SkillRankComponent ranks{};
    CSkillRankSystem::SyncPointsForLevel(ranks, args.level);
    world.AddComponent<SkillRankComponent>(entity, ranks);
    CExperienceSystem::InitializeChampionExperience(world, entity, args.level);

    return handle;
}
```

Factory는 공통 gameplay component만 조립한다. bot, dummy, AI profile, rune, gold, respawn,
network binding, champion-specific state는 각각의 policy/서버 조립 단계가 소유한다.

1-14. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h

기존 코드:

```cpp
eChampion casterChampion = eChampion::NONE;
const SkillDef* pDef = nullptr;
```

아래로 교체:

```cpp
DefinitionKey casterKey{};
ChampionDefId casterDefId{};
const SkillGameplayDef* pDef = nullptr;
```

기존 코드:

```cpp
void Register(u32_t hookId, HookFn fn);
bool Dispatch(u32_t hookId, GameplayHookContext& ctx) const;
bool Has(u32_t hookId) const;
```

아래로 교체:

```cpp
void Register(GameplayPolicyId policyId, HookFn fn);
bool Dispatch(GameplayPolicyId policyId, GameplayHookContext& ctx) const;
bool Has(GameplayPolicyId policyId) const;
```

삭제할 코드:

```cpp
constexpr u32_t MakeGameplayHookId(eChampion champ, u16_t variant)
{
    return (static_cast<u32_t>(champ) << 16) | variant;
}
```

`GameplayHookVariant`와 champion enum 조합으로 hook id를 계산하지 않는다.
JSON의 `effectPolicyKey`를 generator가 dense `GameplayPolicyId`로 resolve하고,
champion GameSim 코드는 generated policy id에 함수 포인터를 한 번 등록한다.
현재 256x256 table은 동일 크기의 1차원 dense function table로 바꾼다.

1-15. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp

기존 코드:

```cpp
f32_t cooldown = ChampionGameDataDB::ResolveSkillCooldown(champion, slot);
```

아래로 교체:

```cpp
const SkillLoadoutComponent* pLoadout =
    world.TryGetComponent<SkillLoadoutComponent>(entity);
if (!pLoadout || slot >= kChampionSkillSlotCount)
    return 0.f;

const SkillGameplayDef* pSkill =
    definitions.TryGet(pLoadout->skills[slot]);
if (!pSkill)
    return 0.f;

const f32_t cooldown = pSkill->cooldown.cooldownSec;
```

동일한 방식으로 아래 global resolver를 제거한다.

```text
ResolveSkillRange
ResolveSkillCooldown
ResolveSkillTiming
ResolveSkillActionLockTicks
IsSkillTwoStage
ResolveSkillStageWindowSec
ResolvePassiveDash*
ResolveSummonerSpell*
```

`CommandExecutor`에는 `const GameplayDefinitionPack&`를 생성자/context로 주입한다.
매 cast마다 champion enum으로 table을 선형 검색하지 않는다.

1-16. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/Stat/StatSystem.cpp

기존 코드:

```cpp
const ChampionStatsDef def = CChampionStatsRegistry::Instance().Resolve(stat.championId);

const u8_t level = (stat.level > 0) ? stat.level : 1;
```

아래로 교체:

```cpp
const ChampionDefinitionComponent* pIdentity =
    world.TryGetComponent<ChampionDefinitionComponent>(entity);
const ExperienceComponent* pExperience =
    world.TryGetComponent<ExperienceComponent>(entity);
if (!pIdentity || !pExperience)
    return;

const ChampionGameplayDef* pChampion =
    definitions.TryGet(pIdentity->defId);
if (!pChampion)
    return;

const ChampionStatsDef& def = pChampion->stats;
const u8_t level = pExperience->level > 0 ? pExperience->level : 1;
```

`CStatSystem` 생성자 또는 Execute context에 `const GameplayDefinitionPack&`를 주입한다.
`CChampionStatsRegistry`를 다시 만들지 않는다.

1-17. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Registries/ChampionStats

삭제할 범위:
`ChampionStatsRegistry.h`, `ChampionStatsRegistry.cpp`와 모든 등록 caller를 삭제.

1-18. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/SummonerSpellGameData.h

기존 코드:

```cpp
struct SummonerSpellGameData
{
    bool_t bValid = false;
    u16_t spellId = 0;
    f32_t rangeMax = 0.f;
    f32_t cooldownSec = 0.f;
    u32_t gameplayPolicyId = 0;
    u32_t visualCueId = 0;
};
```

아래로 교체:

```cpp
struct SummonerSpellGameplayDef
{
    DefinitionKey key{};
    SummonerSpellDefId id{};
    f32_t rangeMax = 0.f;
    f32_t cooldownSec = 0.f;
    GameplayPolicyId gameplayPolicy{};
    u32_t replicatedCueId = 0;
};
```

파일 상단에 추가:

```cpp
#include "Shared/GameSim/Definitions/DefinitionIds.h"
```

1-19. C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoomSpawn.cpp

기존 코드:

```cpp
const EntityID entity = m_world.CreateEntity();

const ChampionStatsDef statsDef =
    CChampionStatsRegistry::Instance().Resolve(slot.champion);
StatComponent stat = CStatSystem::BuildBaseStats(statsDef, 6);
```

아래로 교체:

```cpp
const GameplayDefinitionPack& definitions = *m_pGameplayDefinitions;
const ChampionDefId championDefId =
    definitions.ResolveChampion(LegacyChampionKey(slot.champion));

ChampionGameplaySpawnArgs args{};
args.championDefId = championDefId;
args.team = slot.team;
args.level = m_spawnPolicy.initialLevel;
args.position = GetSpawnPositionForLobbySlot(slot);

const EntityHandle handle = CChampionGameplayFactory::Spawn(
    m_world,
    definitions,
    args);
const EntityID entity = m_world.ResolveEntity(handle);
if (entity == NULL_ENTITY)
    return NULL_ENTITY;
```

삭제할 하드코딩:

```cpp
StatComponent stat = CStatSystem::BuildBaseStats(statsDef, 6);
gold.amount = 10000;
runeLoadout.eRunes[0] = eRuneId::LethalTempo;
```

아래 소유자로 이동한다.

```text
level/gold/respawn -> SpawnPolicyDef
summoner/rune/item -> LoadoutPolicyDef 또는 match-selected loadout
bot skill rank -> BotPolicyDef
champion-specific state attachment -> GameplayPolicyId handler
```

1-20. C:/Users/tnest/Desktop/Winters/Client/Public/GameObject/ChampionSpawnService.h

기존 코드:

```cpp
struct ChampionSpawnResult
{
    EntityID entity = NULL_ENTITY;
    ModelRenderer* pRenderer = nullptr;
    const ChampionDef* pDef = nullptr;
};
```

아래로 교체:

```cpp
struct ChampionSpawnResult
{
    EntityHandle entity{};
    ModelRenderer* pRenderer = nullptr;
    DefinitionKey championKey{};
};
```

기존 코드:

```cpp
eChampion champion = eChampion::END;
```

아래로 교체:

```cpp
DefinitionKey championKey{};
```

Client는 ServerPrivate dense `ChampionDefId`를 저장하지 않는다.
Client visual DB는 같은 stable `DefinitionKey`를 자신의 local visual index로 resolve한다.

기존 코드:

```cpp
#include "ECS/Components/GameplayComponents.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Components/TeamComponent.h"
```

삭제할 코드:

```cpp
#include "GameObject/ChampionDef.h"
```

Client normal network path는 server snapshot entity를 mirror하고 visual만 붙인다.
Client local-only smoke만 `CChampionGameplayFactory`를 호출할 수 있다.

1-21. C:/Users/tnest/Desktop/Winters/Client/Private/GameObject/ChampionSpawnService.cpp

삭제할 범위:
`const ChampionDef* FindSpawnChampionDef(eChampion id)` 함수 전체를 삭제.

기존 코드:

```cpp
const ChampionDef* pDef = FindSpawnChampionDef(request.champion);
if (!pDef || !pDef->fbxPath)
{
    return result;
}
```

아래로 교체:

```cpp
const ChampionVisualData* pVisual =
    context.visualDefinitions.TryGet(request.championKey);
if (!pVisual || !pVisual->bValid || !pVisual->model.fbxPath)
    return result;
```

이 함수의 model, texture, animation, scale reader를 모두 `ChampionVisualData`로 바꾼다.
gameplay stats/health/mana/cooldown을 Client visual Def에서 만들지 않는다.

1-22. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

삭제할 함수:

```text
FindClientChampionDef
FindNetworkSkillDef
BuildLegacyHookBridge
SkillDef 기반 ResolveNetworkActionAnimName overload
SkillDef 기반 PlayLoopNetworkActionIfNeeded overload
```

교체 방향:

```text
ReplicatedAction.actionId/stage
-> ChampionVisualDataDB.TryFindAction(championKey, actionId)
-> ChampionActionVisualStageData
-> animation/VisualEvent/FX playback
```

Client가 `ChampionGameDataDB::ResolveVisualYawOffset`을 읽는 코드는
`ChampionVisualData.model.modelYawOffset`으로 교체한다.
Client local validation은 UX hint일 뿐이며 server cast acceptance를 대체하지 않는다.

1-23. C:/Users/tnest/Desktop/Winters/Tools/LoLData/Build-LoLDefinitionPack.py

CONFIRM_NEEDED:

```text
현재 champions.json 17개 전체, SkillTable.cpp, ChampionTable.cpp,
ChampionVisualTimingSeed.json, champion registration 파일의 실제 필드를 입력으로 읽어야 한다.
구현 직전 각 source의 최신 dirty change를 다시 읽고 완전한 파일 본문을 작성한다.
```

필수 명령:

```text
python Tools/LoLData/Build-LoLDefinitionPack.py extract
python Tools/LoLData/Build-LoLDefinitionPack.py validate
python Tools/LoLData/Build-LoLDefinitionPack.py generate
python Tools/LoLData/Build-LoLDefinitionPack.py check
```

필수 출력:

```text
ServerPrivate canonical JSON
ClientPublic visual JSON
SharedContract manifest
immutable generated Def arrays
stable DefinitionKey -> dense DefId table
legacy parity report
client/server package manifest
```

검증 실패 조건:

```text
duplicate key/hash collision
missing champion skill slot
invalid DefId reference
negative cooldown/cost/range/lock
invalid stage count
missing GameplayPolicy binding
missing visual action for replicated action
ServerPrivate field in ClientPublic output
legacy parity mismatch
```

1-24. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/ChampionGameData.h

삭제할 범위:
모든 새 runtime reader가 `ChampionGameplayDef`, `SkillGameplayDef`,
`SummonerSpellGameData`를 직접 읽은 뒤 파일 전체를 삭제.

1-25. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Registries/ChampionGameData

삭제할 범위:
`ChampionGameDataDB.h`, `ChampionGameDataDB.cpp`와 generated compatibility reader를 삭제.

삭제 조건:

```text
global singleton/static DB lookup이 없다.
GameSimContext가 immutable GameplayDefinitionPack을 명시적으로 소유한다.
spawn은 Def를 한 번 resolve한다.
frame system은 component의 dense DefId로 direct-index한다.
```

1-26. C:/Users/tnest/Desktop/Winters/Client/Private/GameObject/SkillTable.cpp

삭제할 범위:
`SkillDef s_SkillTable`과 `FindSkillDef`를 포함한 파일 전체를 삭제.

1-27. C:/Users/tnest/Desktop/Winters/Client/Private/GameObject/ChampionTable.cpp

삭제할 범위:
`ChampionDef s_ChampionTable`과 `FindChampionDef`를 포함한 파일 전체를 삭제.

1-28. C:/Users/tnest/Desktop/Winters/Client/Public/GamePlay/SkillRegistry.h

삭제할 코드:

```cpp
std::unordered_map<u32_t, SkillDef> m_LegacyMap{};
```

삭제할 함수:

```cpp
void Add(eChampion champ, u8_t slot, const SkillDef& def);
const SkillDef* Find(eChampion champ, u8_t slot) const;
bool_t ResolveGameAtoms(eChampion champ, u8_t slot, SkillGameAtomBundle& outData) const;
bool_t ResolveGameData(eChampion champ, u8_t slot, ChampionGameDataSkill& outData) const;
```

최종적으로 registry 자체를 `ChampionVisualDataDB`로 교체하고 visual lookup만 Client에 남긴다.

1-29. C:/Users/tnest/Desktop/Winters/Client/Public/GamePlay/ChampionRegistry.h

삭제할 범위:
모든 champion registration caller가 generated `ChampionVisualDataDB`로 이동한 뒤 파일 전체를 삭제.

1-30. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/SkillDef.h

삭제할 범위:
`SkillDef.h`, `SkillDefGameDataAdapter.h` 전체를 삭제.

1-31. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/ChampionDef.h

삭제할 범위:
`ChampionDef.h`, Client의 forwarding `GameObject/ChampionDef.h`,
`GameObject/SkillDef.h`, `SkillDefVisualDataAdapter.h` 전체를 삭제.

삭제 조건:

```text
GameSim hook context는 SkillGameplayDef를 읽는다.
Client action playback은 ChampionVisualData/SkillVisualData를 읽는다.
legacy registration cpp가 모두 삭제되었다.
```

1-32. C:/Users/tnest/Desktop/Winters/Shared/Schemas

기존 코드:

```text
Hello.fbs championId:ubyte
LobbyCommand.fbs championId:ubyte
LobbyTypes.fbs championId:ubyte
Snapshot.fbs championId/baseChampionId/visualChampionId/skillChampionId/spellbookChampionId:ubyte
Event.fbs sourceChampion/targetChampion:ubyte
```

아래로 교체:

```text
championKey:uint
baseChampionKey:uint
visualChampionKey:uint
skillChampionKey:uint
spellbookChampionKey:uint
sourceChampionKey:uint
targetChampionKey:uint
```

FlatBuffers generated header는 schema generator로 재생성하고 직접 편집하지 않는다.
schema version을 올리고 구버전 client/server handshake를 거절한다.
`NetEntityId`는 entity instance, `DefinitionKey`는 definition identity로 각각 전송한다.

1-33. C:/Users/tnest/Desktop/Winters/Client/Public/GameObject/ChampionVisualData.h

기존 코드:

```cpp
eChampion champion = eChampion::END;
```

아래로 교체:

```cpp
DefinitionKey championKey{};
```

파일 상단에 추가:

```cpp
#include "Shared/GameSim/Definitions/DefinitionIds.h"
```

Client visual DB는 `DefinitionKey -> local dense visual index`를 load 단계에서만 resolve한다.
frame animation path는 resolved visual index와 `actionId/stage`를 사용한다.

1-34. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/ObjectGameplayDef.h

새 파일:

```cpp
#pragma once

#include "Shared/GameSim/Definitions/DefinitionIds.h"
#include "WintersTypes.h"

struct MinionGameplayDef
{
    DefinitionKey key{};
    MinionDefId id{};
    f32_t maxHp = 0.f;
    f32_t attackDamage = 0.f;
    f32_t attackRange = 0.f;
    f32_t attackCooldownSec = 0.f;
    f32_t moveSpeed = 0.f;
    f32_t sightRange = 0.f;
    f32_t spatialRadius = 0.f;
};

struct StructureGameplayDef
{
    DefinitionKey key{};
    StructureDefId id{};
    f32_t maxHp = 0.f;
    f32_t attackDamage = 0.f;
    f32_t attackRange = 0.f;
    f32_t attackCooldownSec = 0.f;
    f32_t projectileSpeed = 0.f;
    f32_t spatialRadius = 0.f;
};

struct JungleGameplayDef
{
    DefinitionKey key{};
    JungleDefId id{};
    f32_t maxHp = 0.f;
    f32_t attackDamage = 0.f;
    f32_t attackRange = 0.f;
    f32_t attackCooldownSec = 0.f;
    f32_t moveSpeed = 0.f;
    f32_t spatialRadius = 0.f;
};
```

Minion/Structure/Jungle은 하나의 mega union Def로 합치지 않는다.
소비 system과 필수 component가 다르므로 category별 typed Def를 유지한다.

1-35. C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoomSpawn.cpp

삭제할 코드 범위:

```text
ResolveStageStructureMaxHp
ResolveStageJungleMaxHp
ResolveStageJungleRadius
ResolveStageJungleAttackRange
ResolveStageJungleAttackDamage
ResolveStageJungleAttackCooldown
SpawnServerMinion 내부 role별 combat 상수
```

아래로 교체:

```text
stage/map entry의 stable DefinitionKey
-> GameplayDefinitionPack의 typed MinionDefId/StructureDefId/JungleDefId resolve
-> Minion/Structure/Jungle 전용 factory
-> EntityHandle 생성
-> category별 POD component 조립
```

Item/Rune/Reward도 authoring key를 load 단계에서 typed DefId로 resolve한다.
하나의 `GenericDef` 또는 `GenericEntityFactory`에 모든 category를 합치지 않는다.

1-36. C:/Users/tnest/Desktop/Winters/Engine/Public/ECS

기존 소유권:

```text
EntityHandle/CWorld/ComponentStore가 Engine에 있어 Shared/GameSim이 EngineSDK를 include한다.
```

아래로 교체:

```text
Foundation/Core/WintersTypes.h
Foundation/Core/WintersMath.h
Foundation/ECS/Entity.h
Foundation/ECS/ComponentStore.h
Foundation/ECS/World.h
Foundation/ECS/World.cpp

Foundation <- Engine
Foundation <- Shared/GameSim
Foundation <- Client
Foundation <- Server
```

삭제할 범위:
Shared reader가 `Shared/GameSim/Components/TeamComponent.h`로 이동한 뒤
`Engine/Public/ECS/Components/GameplayComponents.h`의 `eTeam` 정의를 삭제.

CONFIRM_NEEDED:

```text
Data reader 전환이 끝난 뒤 별도 세션에서 현재 EngineSDK export macro와 project reference를 확인하고
파일 본문을 변경 없이 먼저 이동한다. GameSim이 Engine renderer/resource/UI를 include하지 않는 상태를
빌드로 증명한 뒤 compatibility forwarding header를 삭제한다.
```

1-37. C:/Users/tnest/Desktop/Winters/.md/architecture/WINTERS_CODEBASE_COMPASS.md

기존 `계층 책임` 아래에 추가:

```text
Data-Driven/Data-Oriented 북극성

Authoring JSON은 협업자의 source다.
Tools는 JSON을 검증하고 immutable product Def pack으로 cook한다.
Server/GameSim은 ServerPrivate gameplay Def와 SharedContract만 읽는다.
Client는 ClientPublic visual Def와 SharedContract만 읽는다.
Spawn 경계는 stable DefinitionKey를 dense DefId로 resolve하고 EntityHandle을 만든다.
Frame system은 문자열/JSON/global registry를 조회하지 않고 dense component와 DefId direct index만 읽는다.
EntityHandle은 process-local lifetime identity이며 저장/네트워크/data key가 아니다.
새로운 gameplay 원리는 code policy이고, 기존 policy의 수치/조합은 data다.
```

1-38. C:/Users/tnest/Desktop/Winters/.md/plan/refactor/09_LOL_DATA_ATOM_EXTRACTION_COLLAB_PLAN.md

기존의 필드별 JSON 분해 방향 위에 추가:

```text
12_WINTERS_DATA_ORIENTED_DEF_ENTITY_REWRITE_PLAN.md가 최종 북극성이다.
원자성은 scalar/file 개수가 아니라 소유자, 검증, 로드 시점, 소비 루프의 일치로 판단한다.
SkillTarget/Cost/Cooldown/Range를 물리적으로 각각의 파일로 분해하는 방향은 폐기한다.
이 값들은 SkillGameplayDef 안의 독립 field group으로 유지한다.
```

1-39. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Components/TeamComponent.h

새 파일:

```cpp
#pragma once

#include "WintersTypes.h"

enum class eTeam : u8_t
{
    Blue = 0,
    Red = 1,
    Neutral = 2,
    TEAM_END,
};

struct TeamComponent
{
    eTeam team = eTeam::Neutral;
};
```

`eTeam`과 `TeamComponent`는 LoL GameSim contract다. Engine/Foundation으로 올리지 않는다.

1-40. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Components/ChampionTag.h

새 파일:

```cpp
#pragma once

struct ChampionTag {};
```

1-41. C:/Users/tnest/Desktop/Winters/Server/Public/Game/GameRoom.h

기존 코드:

```cpp
static std::unique_ptr<CGameRoom> Create(u32_t roomId);
```

아래로 교체:

```cpp
static std::unique_ptr<CGameRoom> Create(
    u32_t roomId,
    const GameplayDefinitionPack& definitions);
```

기존 코드:

```cpp
CGameRoom(u32_t roomId);
```

아래로 교체:

```cpp
CGameRoom(u32_t roomId, const GameplayDefinitionPack& definitions);
```

`CWorld m_world;` 바로 위에 추가:

```cpp
const GameplayDefinitionPack* m_pGameplayDefinitions = nullptr;
```

Server process는 검증된 pack을 한 번 소유하고 모든 room에 const reference로 주입한다.
room이나 system이 pack을 복사하거나 다시 로드하지 않는다.

1-42. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Registries

삭제할 범위:

```text
ChampionStatsRegistry
ChampionGameDataDB
SkillScalingRegistry
RewardRegistry
ItemDef.h 내부 CItemRegistry
SkinRegistry의 gameplay/shared 소유권
```

아래로 교체:

```text
Champion stats       -> ChampionGameplayDef.stats
Skill scaling        -> GameplayDefinitionPack의 ScalingDefId direct index
Item gameplay stats  -> GameplayDefinitionPack의 ItemDefId direct index
Rune gameplay        -> GameplayDefinitionPack의 RuneDefId direct index
Reward/XP curve      -> GameplayDefinitionPack의 RewardDefId direct index
Skin/model/texture   -> ClientPublic ChampionVisualDataDB
```

display name, icon, price text, skin resource는 ClientPublic projection으로 분리한다.
Server gameplay item Def에는 판정에 필요한 price/stat/policy만 둔다.
`unordered_map` registry를 frame system에서 조회하지 않는다.

1-43. 전체 반영 순서

기존 코드:

```text
JSON/C++ table
-> ChampionGameDataDB/SkillRegistry/ChampionRegistry global lookup
-> eChampion + slot 재조회
-> EntityID 생성
-> 중복 runtime component
```

아래로 교체:

```text
S0. golden baseline
-> legacy audit/parity, SimLab hash, Server/Client smoke capture

S1. authoring ownership
-> ServerPrivate/ClientPublic/SharedContract/Test JSON 생성
-> schema/key/reference validation

S2. immutable Def pack
-> stable DefinitionKey 생성
-> deterministic sort
-> dense DefId 부여
-> generated arrays/manifest/hash 생성

S3. additive runtime path
-> GameplayDefinitionPack
-> ChampionDefinitionComponent/SkillLoadoutComponent
-> ChampionGameplayFactory(EntityHandle)
-> legacy path와 spawn/component parity

S4. server authority cutover
-> GameRoom/CommandExecutor/GameSim systems에 pack 주입
-> global ChampionGameDataDB lookup 제거
-> ServerPrivate package boundary 검증

S5. client visual cutover
-> ChampionVisualDataDB
-> snapshot actionId/stage 기반 animation/FX
-> SkillDef/ChampionDef visual reader 제거

S6. runtime truth normalization
-> ChampionComponent 중복 hp/mana/stat/cooldown/level 제거
-> StatComponent identity/level 제거
-> Team/Health/Mana/Experience/SkillState 단일 owner 확정

S7. object/item/rune/policy cutover
-> minion/structure/jungle/item/rune/wave/spawn/loadout hardcode를 typed Def로 전환
-> category별 factory와 parity 검증

S8. legacy deletion
-> SkillTable/ChampionTable/registries/adapters/generated compatibility 삭제
-> legacy symbol 0 검증

S9. dependency completion
-> ECS primitive를 Foundation으로 추출
-> Shared/GameSim -> Engine dependency 제거

S10. DataDriven 완료 gate
-> 17 champion + objects + policies + package + CI parity 통과
-> 이후에만 Data Hot Reload 설계 시작
-> 이후에만 Perforce depot/typemap/stream 설계 시작
```

2. 검증

미검증:

```text
이 문서는 구현 계획이며 새 Data-Driven runtime path는 아직 미반영이다.
현재 17 champion의 legacy 값 전체 이관은 Build-LoLDefinitionPack.py 구현 후 검증한다.
Hot Reload와 Perforce는 이 계획의 완료 조건에 포함하지 않는다.
```

검증 명령:

```powershell
git diff --check
python Tools/LoLData/Build-LoLDefinitionPack.py check
MSBuild.exe Shared\GameSim\Include\GameSim.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
MSBuild.exe Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
MSBuild.exe Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
MSBuild.exe Tools\SimLab\SimLab.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
Tools\Bin\Debug\SimLab.exe
```

구조 검증:

```powershell
rg -n "SkillDef|ChampionDef|SkillTable|ChampionTable|ChampionGameDataDB" Client Shared Server
rg -n "FindSkillDef|FindChampionDef|CSkillRegistry|CChampionRegistry" Client Shared Server
rg -n "visualYawOffset|animPlaySpeed|castFrame|recoveryFrame" Shared/GameSim Server
rg -n "ServerPrivate" Client Client/Bin/Resource
rg -n "ClientPublic/Visual|ChampionVisualData|SkillVisualData" Shared/GameSim Server
rg -n "#include .*Engine|#include .*Client|#include .*Renderer|#include .*UI|#include .*ImGui|#include .*DX" Shared/GameSim
```

통과 기준:

```text
Authoring JSON의 모든 key/reference/schema가 유효하다.
legacy와 새 Def pack의 gameplay/visual 값 mismatch가 0이다.
stable DefinitionKey collision이 0이다.
dense DefId는 deterministic sort에서 매번 동일하다.
Server와 Client가 같은 SharedContract manifest/hash를 사용한다.
ServerPrivate data가 Client package에 없다.
GameSim/Server가 Client visual data를 읽지 않는다.
frame path에서 JSON parse와 string key lookup이 0이다.
spawn/public lifetime API는 EntityHandle을 반환한다.
network schema는 NetEntityId와 stable DefinitionKey를 사용하고 EntityHandle을 전송하지 않는다.
ChampionComponent의 중복 truth reader가 0이다.
SkillDef/ChampionDef/Table/Registry compatibility reader가 0인 뒤 파일을 삭제한다.
GameSim, Server, Client, SimLab 빌드가 오류 0개다.
SimLab same-seed replay hash가 baseline과 동일하다.
```

런타임 확인:

```text
Server: spawn, stats, HP/mana, cooldown, range, target validation, damage, death, respawn,
summoner spell, rune/item/loadout, bot command, minion/object, snapshot/event/action cue를 확인한다.

Client: champion model/texture/scale/yaw, idle/run/basic attack, skill stage 1/2,
visual frame event, FX cue one-shot, projectile, UI hint, snapshot correction을 확인한다.

서버 로그만으로 visual 통과를 판정하지 않는다.
server simulation, snapshot/event emission, client apply, 실제 render playback을 각각 확인한다.
```

완료 판정:

```text
기획자는 ServerPrivate gameplay/policy JSON만 수정한다.
디자이너는 ClientPublic visual/FX JSON과 asset만 수정한다.
개발자는 schema, validator, factory, system, 새로운 GameplayPolicy만 수정한다.
새 champion이 기존 policy 조합만 사용하면 C++ 수정 없이 추가된다.
새로운 판정 원리가 필요할 때만 GameplayPolicy C++가 추가된다.
JSON은 load/cook 단계에서만 읽고 frame은 DefId와 dense component만 읽는다.
이 조건을 모두 만족한 뒤 DataDriven 구조 완료로 판정하고 Hot Reload/Perforce 단계로 이동한다.
```
