Session - 이미 데이터 주도화된 챔피언 스폰 조립부(GameRoomSpawn.cpp 629-747)를 재사용 가능한 ServerEntityFactory::BuildChampionEntity로 순수 추출하고, SimLab same-seed 해시 불변(67F2A97563B8DB04)으로 byte-parity를 증명한다.

현재 상태 메모(이미 반영된 것, 다시 만들지 않음):
- 스폰은 이미 데이터 주도다. `SpawnChampionForLobbySlot`은 `ServerData::GetLoLSpawnObjectDefinitionPack()`(loadout/collider)와 `ServerData::GetLoLGameplayDefinitionPack().FindChampion()`(stat/skill loadout)를 읽는다. gold/level/rune/respawn/collider 하드코딩은 이미 제거됨.
- 챔피언별 sim component 부착은 이미 `AttachChampionSimComponents`(Server/Private/Game/Factory/ChampionSimComponentTable.*)로 함수화됨.
- 남은 일은 "조립 시퀀스가 GameRoom 메서드 본문에 인라인"이라는 것뿐. 이를 자립 factory 함수로 추출한다(동작 불변, 순수 리팩터).

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Server/Private/Game/Factory/ServerChampionEntityFactory.h

새 파일:

```cpp
#pragma once

#include "ECS/Entity.h"
#include "ECS/Components/VisionComponents.h"
#include "WintersMath.h"

class CWorld;
struct LobbySlotState;

namespace ServerEntityFactory
{
    // 데이터(SpawnObjectDefinitionPack + GameplayDefinitionPack)로 챔피언 1기의
    // 서버 권위 gameplay component를 조립한다. AI/네트워크/세션/포즈 등 GameRoom 특화
    // wiring은 호출자가 한다. entity를 생성하고 그 EntityID를 반환한다.
    EntityID BuildChampionEntity(CWorld& world, const LobbySlotState& slot, const Vec3& spawnPos);
}

// GameRoom 분할 spawn(미니언/구조물/정글/챔피언)이 공유하는 가시성 헬퍼.
VisibilityComponent BuildServerVisibleToAll();
```

1-2. C:/Users/tnest/Desktop/Winters/Server/Private/Game/Factory/ServerChampionEntityFactory.cpp

본문은 기존 `SpawnChampionForLobbySlot`의 조립부(629-747)를 그대로 옮긴 것이다(`m_world`->`world`, `spawnPos`는 인자). `BuildServerVisibleToAll`/`AssignDefaultBotSkillRanks`도 여기로 이동한다(기존 동작 동일). include는 GameRoomSpawn.cpp와 같은 closure를 보장하려 `Game/GameRoom.h`를 포함한다.

새 파일:

```cpp
#include "Server/Private/Game/Factory/ServerChampionEntityFactory.h"

#include "Game/GameRoom.h"
#include "Server/Private/Game/GameRoomInternal.h"
#include "Server/Private/Game/GameRoomSmokeRoster.h"
#include "Server/Private/Game/Factory/ChampionSimComponentTable.h"
#include "Server/Private/Data/LoLGameplayDefinitionPack.h"

#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/ChampionDefinitionComponent.h"
#include "Shared/GameSim/Components/ChampionScore.h"
#include "Shared/GameSim/Components/GoldComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/InventoryComponent.h"
#include "Shared/GameSim/Components/RespawnComponent.h"
#include "Shared/GameSim/Components/RuneComponent.h"
#include "Shared/GameSim/Components/SkillLoadoutComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Registries/ChampionStats/ChampionStatsRegistry.h"
#include "Shared/GameSim/Systems/Experience/ExperienceSystem.h"
#include "Shared/GameSim/Systems/SkillRank/SkillRankSystem.h"
#include "Shared/GameSim/Systems/Stat/StatSystem.h"

#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/World.h"

#include <algorithm>

namespace
{
    void AssignDefaultBotSkillRanks(SkillRankComponent& ranks, u8_t championLevel)
    {
        ranks = SkillRankComponent{};
        CSkillRankSystem::SyncPointsForLevel(ranks, championLevel);

        static constexpr u8_t kLevelOrder[] =
        {
            static_cast<u8_t>(eSkillSlot::Q),
            static_cast<u8_t>(eSkillSlot::W),
            static_cast<u8_t>(eSkillSlot::E),
            static_cast<u8_t>(eSkillSlot::Q),
            static_cast<u8_t>(eSkillSlot::Q),
            static_cast<u8_t>(eSkillSlot::R),
            static_cast<u8_t>(eSkillSlot::W),
            static_cast<u8_t>(eSkillSlot::W),
            static_cast<u8_t>(eSkillSlot::E),
            static_cast<u8_t>(eSkillSlot::E),
            static_cast<u8_t>(eSkillSlot::Q),
            static_cast<u8_t>(eSkillSlot::W),
            static_cast<u8_t>(eSkillSlot::E),
            static_cast<u8_t>(eSkillSlot::R),
            static_cast<u8_t>(eSkillSlot::Q),
            static_cast<u8_t>(eSkillSlot::W),
            static_cast<u8_t>(eSkillSlot::E),
            static_cast<u8_t>(eSkillSlot::R),
        };

        const u8_t count = std::min<u8_t>(
            championLevel,
            static_cast<u8_t>(sizeof(kLevelOrder) / sizeof(kLevelOrder[0])));
        for (u8_t i = 0; i < count && ranks.pointsAvailable > 0u; ++i)
            CSkillRankSystem::TryLevelSkill(ranks, kLevelOrder[i]);
    }
}

VisibilityComponent BuildServerVisibleToAll()
{
    VisibilityComponent visibility{};
    visibility.teamVisibilityMask = static_cast<u8_t>(
        (1u << TeamByte(eTeam::Blue)) |
        (1u << TeamByte(eTeam::Red)));
    return visibility;
}

EntityID ServerEntityFactory::BuildChampionEntity(
    CWorld& world, const LobbySlotState& slot, const Vec3& spawnPos)
{
    const EntityHandle entityHandle = world.CreateEntityHandle();
    const EntityID entity = entityHandle.GetIndex();

    TransformComponent transform{};
    transform.SetPosition(spawnPos);
    world.AddComponent<TransformComponent>(entity, transform);

    const SpawnObjectDefinitionPack& objectDefs = ServerData::GetLoLSpawnObjectDefinitionPack();
    const SpawnLoadoutPolicyDef& spawnPolicy = objectDefs.spawnLoadout;
    const GameplayDefinitionPack& definitions = ServerData::GetLoLGameplayDefinitionPack();
    const ChampionGameplayDef* championDef = definitions.FindChampion(slot.champion);

    StatComponent stat{};
    f32_t spatialRadius = 0.75f;
    f32_t sightRange = 19.f;
    if (championDef)
    {
        ChampionDefinitionComponent identity{};
        identity.championDefId = championDef->id;
        world.AddComponent<ChampionDefinitionComponent>(entity, identity);

        SkillLoadoutComponent loadout{};
        for (u8_t skillSlot = 0u; skillSlot < kChampionSkillSlotCount; ++skillSlot)
            loadout.skills[skillSlot] = championDef->skillLoadout[skillSlot];
        world.AddComponent<SkillLoadoutComponent>(entity, loadout);

        stat = CStatSystem::BuildBaseStats(
            championDef->stats,
            championDef->legacyChampion,
            spawnPolicy.startLevel);
        spatialRadius = championDef->stats.spatialRadius;
        sightRange = championDef->stats.sightRange;
    }
    else
    {
        const ChampionStatsDef legacyStats =
            CChampionStatsRegistry::Instance().Resolve(slot.champion);
        stat = CStatSystem::BuildBaseStats(legacyStats, spawnPolicy.startLevel);
        spatialRadius = legacyStats.spatialRadius;
        sightRange = legacyStats.sightRange;
    }
    stat.hpMax = ResolveServerChampionMaxHpForSlot(slot, stat.hpMax);
    world.AddComponent<StatComponent>(entity, stat);

    HealthComponent health{};
    health.fCurrent = stat.hpMax;
    health.fMaximum = stat.hpMax;
    health.bIsDead = false;
    world.AddComponent<HealthComponent>(entity, health);

    RespawnComponent respawn{};
    respawn.spawnPos = spawnPos;
    respawn.respawnDelay = spawnPolicy.respawnDelaySec;
    world.AddComponent<RespawnComponent>(entity, respawn);

    SkillStateComponent skillState{};
    world.AddComponent<SkillStateComponent>(entity, skillState);

    CExperienceSystem::InitializeChampionExperience(world, entity, stat.level);

    SkillRankComponent skillRank{};
    if (slot.bBot && !slot.bDummy)
        AssignDefaultBotSkillRanks(skillRank, stat.level);
    else
        CSkillRankSystem::SyncPointsForLevel(skillRank, stat.level);
    world.AddComponent<SkillRankComponent>(entity, skillRank);

    GoldComponent gold{};
    gold.amount = spawnPolicy.startGold;
    world.AddComponent<GoldComponent>(entity, gold);

    InventoryComponent inventory{};
    world.AddComponent<InventoryComponent>(entity, inventory);

    RuneLoadoutComponent runeLoadout{};
    runeLoadout.eRunes[0] = spawnPolicy.startRune;
    runeLoadout.iCount = spawnPolicy.startRuneCount;
    world.AddComponent<RuneLoadoutComponent>(entity, runeLoadout);
    world.AddComponent<RuneRuntimeComponent>(entity, RuneRuntimeComponent{});

    ChampionScoreComponent score{};
    world.AddComponent<ChampionScoreComponent>(entity, score);

    SummonerSpellStateComponent summonerSpellState{};
    world.AddComponent<SummonerSpellStateComponent>(entity, summonerSpellState);

    ChampionComponent champion{};
    champion.id = slot.champion;
    champion.team = static_cast<eTeam>(slot.team);
    champion.hp = health.fCurrent;
    champion.maxHp = health.fMaximum;
    champion.mana = stat.manaMax;
    champion.maxMana = stat.manaMax;
    champion.moveSpeed = stat.moveSpeed;
    champion.level = stat.level;
    world.AddComponent<ChampionComponent>(entity, champion);

    AttachChampionSimComponents(world, entity, slot.champion);

    SpatialAgentComponent spatial{};
    spatial.kind = eSpatialKind::Character;
    spatial.team = slot.team;
    spatial.radius = spatialRadius;
    world.AddComponent<SpatialAgentComponent>(entity, spatial);

    const ChampionColliderProfileDef& colliderProfile = objectDefs.championCollider;
    ColliderComponent collider{};
    collider.vHalfExtents = { spatial.radius, colliderProfile.bodyHeight, spatial.radius };
    collider.vOffset = { 0.f, colliderProfile.bodyOffsetY, 0.f };
    collider.bIsTrigger = false;
    world.AddComponent<ColliderComponent>(entity, collider);

    VisionSourceComponent vision{};
    vision.sightRange = sightRange;
    world.AddComponent<VisionSourceComponent>(entity, vision);
    world.AddComponent<VisibilityComponent>(entity, BuildServerVisibleToAll());
    world.AddComponent<TargetableTag>(entity);

    return entity;
}
```

1-3. C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoomSpawn.cpp

기존 코드:

```cpp
#include "Server/Private/Game/Factory/ChampionSimComponentTable.h"
```

아래에 추가:

```cpp
#include "Server/Private/Game/Factory/ServerChampionEntityFactory.h"
```

`AssignDefaultBotSkillRanks`와 `BuildServerVisibleToAll`는 factory.cpp로 이동했으므로 anonymous namespace에서 제거한다.

기존 코드:

```cpp
namespace
{
    void AssignDefaultBotSkillRanks(SkillRankComponent& ranks, u8_t championLevel)
    {
        ranks = SkillRankComponent{};
        CSkillRankSystem::SyncPointsForLevel(ranks, championLevel);

        static constexpr u8_t kLevelOrder[] =
        {
            static_cast<u8_t>(eSkillSlot::Q),
            static_cast<u8_t>(eSkillSlot::W),
            static_cast<u8_t>(eSkillSlot::E),
            static_cast<u8_t>(eSkillSlot::Q),
            static_cast<u8_t>(eSkillSlot::Q),
            static_cast<u8_t>(eSkillSlot::R),
            static_cast<u8_t>(eSkillSlot::W),
            static_cast<u8_t>(eSkillSlot::W),
            static_cast<u8_t>(eSkillSlot::E),
            static_cast<u8_t>(eSkillSlot::E),
            static_cast<u8_t>(eSkillSlot::Q),
            static_cast<u8_t>(eSkillSlot::W),
            static_cast<u8_t>(eSkillSlot::E),
            static_cast<u8_t>(eSkillSlot::R),
            static_cast<u8_t>(eSkillSlot::Q),
            static_cast<u8_t>(eSkillSlot::W),
            static_cast<u8_t>(eSkillSlot::E),
            static_cast<u8_t>(eSkillSlot::R),
        };

        const u8_t count = std::min<u8_t>(
            championLevel,
            static_cast<u8_t>(sizeof(kLevelOrder) / sizeof(kLevelOrder[0])));
        for (u8_t i = 0; i < count && ranks.pointsAvailable > 0u; ++i)
            CSkillRankSystem::TryLevelSkill(ranks, kLevelOrder[i]);
    }

    constexpr int32_t kStageChampionSpawnWalkableSearchRadius = 16;
    constexpr f32_t kChampionAIInitialDecisionDelaySec = 0.35f;

    VisibilityComponent BuildServerVisibleToAll()
    {
        VisibilityComponent visibility{};
        visibility.teamVisibilityMask = static_cast<u8_t>(
            (1u << TeamByte(eTeam::Blue)) |
            (1u << TeamByte(eTeam::Red)));
        return visibility;
    }
}
```

아래로 교체:

```cpp
namespace
{
    constexpr int32_t kStageChampionSpawnWalkableSearchRadius = 16;
    constexpr f32_t kChampionAIInitialDecisionDelaySec = 0.35f;
}
```

`SpawnChampionForLobbySlot`의 조립부를 factory 호출로 교체한다(tail의 dummy/patrol/AI/pose/netId/session/trace는 그대로 둔다).

기존 코드:

```cpp
    const EntityHandle entityHandle = m_world.CreateEntityHandle();
    const EntityID entity = entityHandle.GetIndex();

    const Vec3 spawnPos = GetSpawnPositionForLobbySlot(slot);

    TransformComponent transform{};
    transform.SetPosition(spawnPos);
    m_world.AddComponent<TransformComponent>(entity, transform);
```

(이 블록부터 `m_world.AddComponent<TargetableTag>(entity);`까지가 조립부다.)

아래로 교체:

```cpp
    const Vec3 spawnPos = GetSpawnPositionForLobbySlot(slot);
    const EntityID entity = ServerEntityFactory::BuildChampionEntity(m_world, slot, spawnPos);
```

즉 `const EntityHandle entityHandle = m_world.CreateEntityHandle();` 줄부터 `m_world.AddComponent<TargetableTag>(entity);` 줄까지(조립부 전체)를 위 2줄로 교체한다. 이후 `if (slot.bDummy)`부터 시작하는 tail은 그대로 유지한다.

1-4. C:/Users/tnest/Desktop/Winters/Server/Include/Server.vcxproj (+ Server.vcxproj.filters)

확인 필요:
- 새 `ServerChampionEntityFactory.cpp/.h`를 Server 프로젝트에 등록한다. 기존 `..\Private\Game\Factory\ChampionSimComponentTable.cpp/.h` 항목 옆에 동일 패턴으로 추가.

2. 검증

미검증:
- 빌드 미검증
- SimLab same-seed 해시 불변 미검증

검증 명령:
- python .\Tools\LoLData\Build-LoLDefinitionPack.py --root . --check  (데이터 변경 없음 확인)
- MSBuild GameSim/Server/Client/SimLab.vcxproj Debug x64
- .\Tools\Bin\Debug\SimLab.exe  (또는 same-seed 실행 모드)

통과 기준 (순수 리팩터):
- 4개 프로젝트 빌드 성공.
- SimLab same-seed 해시 == 67F2A97563B8DB04 (변하면 추출이 동작을 바꾼 것 -> 즉시 원인 분석).
- git diff: GameRoomSpawn.cpp는 조립부 삭제 + factory 호출 2줄 + 헬퍼 제거만, 새 factory.* 2파일 추가.

확인 필요:
- 새 factory 파일이 Server.vcxproj에 포함되는지(빌드 누락 시 link 에러).
- `Game/GameRoom.h` include closure가 factory.cpp의 SummonerSpellStateComponent/kChampionSkillSlotCount 등을 전부 제공하는지(빌드로 확인).
