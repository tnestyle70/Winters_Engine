Session - 서버 스폰의 하드코딩 리터럴과 챔피언별 if 체인을 resolver + 데이터 주도 component table로 교체해 "데이터 -> 엔티티 조립"을 함수로 만든다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Server/Private/Game/Factory/ChampionSimComponentTable.h

챔피언별 sim component 부착을 `if` 체인이 아니라 데이터 주도 table로 표현한다(북극성 1.2의 ECS 논거: 새 챔피언은 표에 한 줄 추가).

새 파일:

```cpp
#pragma once

#include "Shared/GameSim/Definitions/ChampionTypes.h"

class CWorld;

// 선택된 champion에 해당하는 sim-only component를 부착한다.
// 새 챔피언 추가 = ChampionSimComponentTable.cpp 표에 한 줄 추가.
void AttachChampionSimComponents(CWorld& world, EntityID entity, eChampion champion);
```

확인 필요:
- `eChampion`을 선언한 헤더 경로 확인. 현재 `slot.champion`이 `eChampion`이므로 `GameRoomSpawn.cpp`가 이미 include하는 헤더로 맞춘다(`ChampionTypes.h`가 아니면 교정).
- `EntityID` 선언 헤더(`ECS/Entity.h`) include 필요 여부 확인.

1-2. C:/Users/tnest/Desktop/Winters/Server/Private/Game/Factory/ChampionSimComponentTable.cpp

현재 `GameRoomSpawn.cpp:795-814`의 if 체인을 그대로 table로 옮긴다(값/동작 동일).

새 파일:

```cpp
#include "Server/Private/Game/Factory/ChampionSimComponentTable.h"

#include "ECS/World.h"
#include "Shared/GameSim/Components/YasuoStateComponent.h"
#include "Shared/GameSim/Components/AsheSimComponent.h"
#include "Shared/GameSim/Components/AnnieSimComponent.h"
#include "Shared/GameSim/Components/FioraSimComponent.h"
#include "Shared/GameSim/Components/JaxSimComponent.h"
#include "Shared/GameSim/Components/ViegoSimComponent.h"
#include "Shared/GameSim/Components/YoneSimComponent.h"
#include "Shared/GameSim/Components/LeeSinSimComponent.h"
#include "Shared/GameSim/Components/KindredSimComponent.h"
#include "Shared/GameSim/Components/MasterYiComponent.h"

namespace
{
    struct ChampionSimEntry
    {
        eChampion champion;
        void (*install)(CWorld&, EntityID);
    };

    template<typename T>
    void InstallSim(CWorld& world, EntityID entity)
    {
        world.AddComponent<T>(entity, T{});
    }

    constexpr ChampionSimEntry kChampionSimTable[] =
    {
        { eChampion::YASUO,    &InstallSim<YasuoStateComponent> },
        { eChampion::ASHE,     &InstallSim<AsheSimComponent>    },
        { eChampion::ANNIE,    &InstallSim<AnnieSimComponent>   },
        { eChampion::FIORA,    &InstallSim<FioraSimComponent>   },
        { eChampion::JAX,      &InstallSim<JaxSimComponent>     },
        { eChampion::VIEGO,    &InstallSim<ViegoSimComponent>   },
        { eChampion::YONE,     &InstallSim<YoneSimComponent>    },
        { eChampion::LEESIN,   &InstallSim<LeeSinSimComponent>  },
        { eChampion::KINDRED,  &InstallSim<KindredSimComponent> },
        { eChampion::MASTERYI, &InstallSim<MasterYiSimComponent>},
    };
}

void AttachChampionSimComponents(CWorld& world, EntityID entity, eChampion champion)
{
    for (const ChampionSimEntry& entry : kChampionSimTable)
    {
        if (entry.champion == champion)
        {
            entry.install(world, entity);
            return;
        }
    }
}
```

확인 필요:
- 각 sim component 헤더의 실제 파일명/경로를 `GameRoomSpawn.cpp`의 기존 include와 대조해 교정한다(특히 `MasterYiComponent` vs `MasterYiSimComponent`).

1-3. C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoomSpawn.cpp

파일 상단 include 블록에 아래를 추가(기존 `MinionCombatDef`/registry include 근처):

기존 코드:

```cpp
#include "Server/Private/Game/GameRoomInternal.h"
```

아래에 추가:

```cpp
#include "Server/Private/Game/Factory/ChampionSimComponentTable.h"
#include "Shared/GameSim/Registries/SpawnPolicy/SpawnPolicyDB.h"
#include "Shared/GameSim/Registries/ObjectGameData/ObjectGameDataDB.h"
```

확인 필요:
- `GameRoomSpawn.cpp`가 실제로 include하는 첫 줄을 확인하고, 위 anchor(`GameRoomInternal.h`)가 아니면 실제 include 블록 마지막 줄 아래로 옮긴다.

`EntityID CGameRoom::SpawnChampionForLobbySlot(LobbySlotState& slot)` 안에서:

기존 코드:

```cpp
    StatComponent stat = CStatSystem::BuildBaseStats(statsDef, 6);
```

아래로 교체:

```cpp
    const SpawnLoadoutPolicyDef& loadout = SpawnPolicyDB::ResolveLoadout();
    StatComponent stat = CStatSystem::BuildBaseStats(statsDef, loadout.startLevel);
```

기존 코드:

```cpp
    RespawnComponent respawn{};
    respawn.spawnPos = spawnPos;
    respawn.respawnDelay = kDefaultChampionRespawnDelaySec;
    m_world.AddComponent<RespawnComponent>(entity, respawn);
```

아래로 교체:

```cpp
    RespawnComponent respawn{};
    respawn.spawnPos = spawnPos;
    respawn.respawnDelay = loadout.respawnDelaySec;
    m_world.AddComponent<RespawnComponent>(entity, respawn);
```

기존 코드:

```cpp
    GoldComponent gold{};
    gold.amount = 10000;
    m_world.AddComponent<GoldComponent>(entity, gold);
```

아래로 교체:

```cpp
    GoldComponent gold{};
    gold.amount = loadout.startGold;
    m_world.AddComponent<GoldComponent>(entity, gold);
```

기존 코드:

```cpp
    RuneLoadoutComponent runeLoadout{};
    runeLoadout.eRunes[0] = eRuneId::LethalTempo;
    runeLoadout.iCount = 1u;
    m_world.AddComponent<RuneLoadoutComponent>(entity, runeLoadout);
```

아래로 교체:

```cpp
    RuneLoadoutComponent runeLoadout{};
    runeLoadout.eRunes[0] = loadout.startRune;
    runeLoadout.iCount = loadout.startRuneCount;
    m_world.AddComponent<RuneLoadoutComponent>(entity, runeLoadout);
```

기존 코드:

```cpp
    if (slot.champion == eChampion::YASUO)
        m_world.AddComponent<YasuoStateComponent>(entity, YasuoStateComponent{});
    if (slot.champion == eChampion::ASHE)
        m_world.AddComponent<AsheSimComponent>(entity, AsheSimComponent{});
    if (slot.champion == eChampion::ANNIE)
        m_world.AddComponent<AnnieSimComponent>(entity, AnnieSimComponent{});
    if (slot.champion == eChampion::FIORA)
        m_world.AddComponent<FioraSimComponent>(entity, FioraSimComponent{});
    if (slot.champion == eChampion::JAX)
        m_world.AddComponent<JaxSimComponent>(entity, JaxSimComponent{});
    if (slot.champion == eChampion::VIEGO)
        m_world.AddComponent<ViegoSimComponent>(entity, ViegoSimComponent{});
    if (slot.champion == eChampion::YONE)
        m_world.AddComponent<YoneSimComponent>(entity, YoneSimComponent{});
    if (slot.champion == eChampion::LEESIN)
        m_world.AddComponent<LeeSinSimComponent>(entity, LeeSinSimComponent{});
    if (slot.champion == eChampion::KINDRED)
        m_world.AddComponent<KindredSimComponent>(entity, KindredSimComponent{});
    if (slot.champion == eChampion::MASTERYI)
        m_world.AddComponent<MasterYiSimComponent>(entity, MasterYiSimComponent{});
```

아래로 교체:

```cpp
    AttachChampionSimComponents(m_world, entity, slot.champion);
```

기존 코드:

```cpp
    ColliderComponent collider{};
    collider.vHalfExtents = { spatial.radius, 1.8f, spatial.radius };
    collider.vOffset = { 0.f, 0.9f, 0.f };
    collider.bIsTrigger = false;
    m_world.AddComponent<ColliderComponent>(entity, collider);
```

아래로 교체:

```cpp
    const ChampionColliderProfileDef& colliderProfile = ObjectGameDataDB::ResolveChampionCollider();
    ColliderComponent collider{};
    collider.vHalfExtents = { spatial.radius, colliderProfile.bodyHeight, spatial.radius };
    collider.vOffset = { 0.f, colliderProfile.bodyOffsetY, 0.f };
    collider.bIsTrigger = false;
    m_world.AddComponent<ColliderComponent>(entity, collider);
```

1-4. C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoomSpawn.cpp

`EntityID CGameRoom::SpawnServerStructure(...)`의 turret AI 하드코딩을 resolver로 교체한다.

line 537-543 근처 `TurretAIComponent` 설정 블록에서 아래 리터럴을:

```text
attackRange = 7.75f
attackCooldownMax = 1.0f
attackDamage = 180f(Nexus) / 150f
projectileSpeed = 18f
```

아래 값으로 교체한다(정확한 기존 코드 라인은 구현 직전 확인):

```cpp
    const TurretAIGameDef& turretAI = ObjectGameDataDB::ResolveStructure().turretAI;
    ai.attackRange = turretAI.attackRange;
    ai.attackCooldownMax = turretAI.attackCooldownMax;
    ai.attackDamage = bNexus ? turretAI.nexusAttackDamage : turretAI.attackDamage;
    ai.projectileSpeed = turretAI.projectileSpeed;
```

확인 필요:
- `SpawnServerStructure` 안 `TurretAIComponent` 지역변수명(`ai` 등)과 nexus 판정 조건(`bNexus`/tier)을 실제 코드로 확인 후 표현을 맞춘다.
- `ResolveStageStructureMaxHp`(107-114)와 fallback 구조물 위치/HP(344-356)도 `ObjectGameDataDB::ResolveStructureMaxHp`로 교체하되, 위치 좌표는 map placement이므로 데이터화 대상에서 제외한다(SpawnPolicy/Map 소유, 09 시리즈).

1-5. C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoomSpawn.cpp

`EntityID CGameRoom::SpawnServerJungleFromStageEntry(...)`의 `Resolve*Jungle*` 호출을 단일 resolver로 모은다.

`StatComponent`/HP 설정에서 개별 `ResolveStageJungleMaxHp/Radius/AttackRange/AttackDamage/AttackCooldown/MoveSpeed(entry.subKind)` 호출들을:

```cpp
    const JungleCampGameDef camp = ObjectGameDataDB::ResolveJungleCamp(entry.subKind);
```

한 번 호출 후 `camp.maxHp / camp.radius / camp.attackRange / camp.attackDamage / camp.attackCooldown / camp.moveSpeed / camp.baseArmor / camp.baseMr`로 치환한다.

확인 필요:
- 기존 `Resolve*Jungle*` 6개 함수의 정확한 호출 위치와 인자(`entry.subKind`)를 확인.
- 치환 후 기존 `Resolve*Jungle*` static 함수가 더 이상 호출되지 않으면 같은 파일에서 함께 제거한다(미사용 정리). 호출이 남아 있으면 제거하지 않는다.

2. 검증

미검증:
- 빌드 미검증
- F5 런타임에서 챔피언/구조물/정글 스폰 동작 불변 미검증

검증 명령:
- git diff --check
- & "C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" .\Server\Include\Server.vcxproj /m /p:Configuration=Debug /p:Platform=x64

수동 확인 (서버 로그만으로 판정 금지):
- F5: 챔피언 스폰 시 gold/level/rune/respawn/collider가 기존과 동일한지.
- F5: 챔피언별 sim component(예: Yasuo Q stack)가 기존과 동일하게 작동하는지.
- F5: 타워 사거리/피해/투사체 속도, 정글 캠프 HP/사거리가 기존과 동일한지.

확인 필요:
- 새 `Server/Private/Game/Factory/ChampionSimComponentTable.h/.cpp`가 Server 빌드 프로젝트에 포함되는지 확인.
- 01 계획서의 resolver(`SpawnPolicyDB`, `ObjectGameDataDB`)가 먼저 반영/빌드되어 있어야 한다.

전제:
- 본 세션은 01(데이터 def/resolver) 반영 후 진행한다.
