Session - 챔피언/오브젝트 스폰 하드코딩을 POD def + read-only resolver로 추출하되 legacy와 동일 값(parity)으로 회귀 없이 데이터 층을 만든다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/SpawnLoadoutPolicyDef.h

match 시작 시 챔피언에게 주는 loadout(gold/level/rune/respawn)을 `GameRoomSpawn.cpp`에서 빼낸 POD다. `MinionCombatDef.h`와 같은 패턴(헤더 inline)으로 둔다.

새 파일:

```cpp
#pragma once

#include "WintersTypes.h"
#include "Shared/GameSim/Components/RuneComponent.h"

// match 시작 loadout 정책. Server만 소비한다. visual/Engine 의존 없음.
struct SpawnLoadoutPolicyDef
{
    u32_t startGold = 10000u;
    u8_t  startLevel = 6u;
    eRuneId startRune = eRuneId::LethalTempo;
    u8_t  startRuneCount = 1u;
    f32_t respawnDelaySec = 3.f;
};
```

확인 필요:
- `eRuneId`와 `RuneComponent.h` 경로가 맞는지 확인. 아니면 `RuneLoadoutComponent`를 선언한 헤더로 include를 교정한다.
- 현재 `kDefaultChampionRespawnDelaySec` 값(`GameRoomInternal.h:31`)이 3.f인지 확인하고 동일하게 맞춘다.

1-2. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/ChampionColliderProfileDef.h

챔피언 collider/spatial body 프로파일을 빼낸 POD다. 반경(x,z)은 `ChampionStatsDef::spatialRadius`에서 오므로 여기엔 height/offset만 둔다.

새 파일:

```cpp
#pragma once

#include "WintersTypes.h"

// 챔피언 충돌/스폰 body 프로파일. Server 판정용. 표시 전용 scale은 포함하지 않는다.
struct ChampionColliderProfileDef
{
    f32_t bodyHeight = 1.8f;   // GameRoomSpawn.cpp:823 halfExtents.y
    f32_t bodyOffsetY = 0.9f;  // GameRoomSpawn.cpp:824 offset.y
};
```

1-3. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/StructureGameDef.h

구조물 HP와 turret AI 수치를 빼낸 POD다. 현재 `GameRoomSpawn.cpp`의 `ResolveStageStructureMaxHp`(107-114)와 turret AI 리터럴(538-543)을 담는다.

새 파일:

```cpp
#pragma once

#include "WintersTypes.h"

enum class eStructureKind : u8_t
{
    Turret = 0,
    Inhibitor = 1,
    Nexus = 2,
};

struct TurretAIGameDef
{
    f32_t attackRange = 7.75f;        // GameRoomSpawn.cpp:538
    f32_t attackCooldownMax = 1.f;    // GameRoomSpawn.cpp:539
    f32_t attackDamage = 150.f;       // GameRoomSpawn.cpp:540-542 (non-nexus)
    f32_t nexusAttackDamage = 180.f;  // GameRoomSpawn.cpp:540-542 (nexus tier)
    f32_t projectileSpeed = 18.f;     // GameRoomSpawn.cpp:543
    f32_t turretSightRange = 12.f;    // GameRoomSpawn.cpp:570 (turret)
    f32_t structureSightRange = 10.f; // GameRoomSpawn.cpp:570 (other)
    f32_t bodyHeight = 2.5f;          // GameRoomSpawn.cpp:564 halfExtents.y
    f32_t bodyOffsetY = 1.25f;        // GameRoomSpawn.cpp:565 offset.y
};

struct StructureGameDef
{
    f32_t turretMaxHp = 3000.f;     // ResolveStageStructureMaxHp Turret
    f32_t inhibitorMaxHp = 4000.f;  // ResolveStageStructureMaxHp Inhibitor
    f32_t nexusMaxHp = 5500.f;      // ResolveStageStructureMaxHp Nexus
    TurretAIGameDef turretAI{};
};
```

1-4. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/JungleCampGameDef.h

정글 캠프 subKind별 수치를 빼낸 POD다. 현재 `GameRoomSpawn.cpp`의 `Resolve*Jungle*`(116-210) 스위치를 한 곳으로 모은다.

새 파일:

```cpp
#pragma once

#include "WintersTypes.h"

struct JungleCampGameDef
{
    f32_t maxHp = 1500.f;
    f32_t radius = 1.f;
    f32_t attackRange = 1.7f;
    f32_t attackDamage = 45.f;
    f32_t attackCooldown = 1.4f;
    f32_t moveSpeed = 4.f;
    f32_t baseArmor = 20.f;  // GameRoomSpawn.cpp:458
    f32_t baseMr = 20.f;     // GameRoomSpawn.cpp:460
};
```

확인 필요:
- 현재 `Resolve*Jungle*` 스위치의 subKind 분기값(0/1/2-3-5/4-6-7/8-9-10)을 1-6 resolver에 그대로 옮겼는지 parity로 확인.

1-5. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Registries/SpawnPolicy/SpawnPolicyDB.h

loadout 정책 read-only resolver. `ChampionGameDataDB`와 같은 namespace 스타일.

새 파일:

```cpp
#pragma once

#include "WintersTypes.h"
#include "Shared/GameSim/Definitions/SpawnLoadoutPolicyDef.h"

namespace SpawnPolicyDB
{
    // 현재는 전 챔피언 공통 default loadout. 추후 mode/team 키로 확장.
    const SpawnLoadoutPolicyDef& ResolveLoadout();
}
```

1-6. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Registries/SpawnPolicy/SpawnPolicyDB.cpp

새 파일:

```cpp
#include "Shared/GameSim/Registries/SpawnPolicy/SpawnPolicyDB.h"

namespace SpawnPolicyDB
{
    const SpawnLoadoutPolicyDef& ResolveLoadout()
    {
        static const SpawnLoadoutPolicyDef kDefault{};
        return kDefault;
    }
}
```

1-7. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Registries/ObjectGameData/ObjectGameDataDB.h

구조물/정글/챔피언 collider read-only resolver. minion은 이미 `ResolveMinionCombatDef`가 있으므로 여기서 같이 노출만 한다.

새 파일:

```cpp
#pragma once

#include "WintersTypes.h"
#include "Shared/GameSim/Definitions/StructureGameDef.h"
#include "Shared/GameSim/Definitions/JungleCampGameDef.h"
#include "Shared/GameSim/Definitions/ChampionColliderProfileDef.h"
#include "Shared/GameSim/Definitions/MinionCombatDef.h"

namespace ObjectGameDataDB
{
    const StructureGameDef& ResolveStructure();
    f32_t ResolveStructureMaxHp(eStructureKind kind);

    JungleCampGameDef ResolveJungleCamp(u8_t subKind);

    const ChampionColliderProfileDef& ResolveChampionCollider();

    // 기존 inline ResolveMinionCombatDef(roleType)를 단일 진입점으로 감싼다.
    MinionCombatDef ResolveMinion(u8_t roleType);
}
```

1-8. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Registries/ObjectGameData/ObjectGameDataDB.cpp

초기값은 legacy 하드코딩과 동일하게 둔다(parity). `ResolveJungleCamp`는 현재 `GameRoomSpawn.cpp`의 subKind 스위치를 그대로 옮긴다.

새 파일:

```cpp
#include "Shared/GameSim/Registries/ObjectGameData/ObjectGameDataDB.h"

namespace ObjectGameDataDB
{
    const StructureGameDef& ResolveStructure()
    {
        static const StructureGameDef kDefault{};
        return kDefault;
    }

    f32_t ResolveStructureMaxHp(eStructureKind kind)
    {
        const StructureGameDef& def = ResolveStructure();
        switch (kind)
        {
        case eStructureKind::Nexus:     return def.nexusMaxHp;
        case eStructureKind::Inhibitor: return def.inhibitorMaxHp;
        case eStructureKind::Turret:
        default:                        return def.turretMaxHp;
        }
    }

    JungleCampGameDef ResolveJungleCamp(u8_t subKind)
    {
        JungleCampGameDef def{};
        switch (subKind)
        {
        case 0:  def.maxHp = 8000.f; def.radius = 2.5f; def.attackRange = 4.f; def.attackDamage = 120.f; def.attackCooldown = 1.2f; def.moveSpeed = 2.5f; break;
        case 1:  def.maxHp = 5000.f; def.radius = 2.2f; def.attackRange = 3.f; def.attackDamage = 90.f;  def.attackCooldown = 1.5f; def.moveSpeed = 4.f;  break;
        case 2:
        case 3:  def.maxHp = 1500.f; def.radius = 1.2f; def.attackRange = 2.f; def.attackDamage = 65.f;  def.attackCooldown = 1.4f; def.moveSpeed = 4.f;  break;
        case 5:  def.maxHp = 1500.f; def.radius = 1.2f; def.attackRange = 2.f; def.attackDamage = 60.f;  def.attackCooldown = 1.4f; def.moveSpeed = 4.f;  break;
        case 8:
        case 9:
        case 10: def.maxHp = 1500.f; def.radius = 0.7f; def.attackRange = 1.4f; def.attackDamage = 25.f; def.attackCooldown = 1.25f; def.moveSpeed = 4.5f; break;
        default: def.maxHp = 1500.f; def.radius = 1.f;  def.attackRange = 1.7f; def.attackDamage = 45.f; def.attackCooldown = 1.4f;  def.moveSpeed = 4.f;  break;
        }
        return def;
    }

    const ChampionColliderProfileDef& ResolveChampionCollider()
    {
        static const ChampionColliderProfileDef kDefault{};
        return kDefault;
    }

    MinionCombatDef ResolveMinion(u8_t roleType)
    {
        return ResolveMinionCombatDef(roleType);
    }
}
```

확인 필요:
- `GameRoomSpawn.cpp:116-210`의 실제 `Resolve*Jungle*` 분기와 위 subKind 매핑이 1:1인지 대조(특히 radius 4-6-7, moveSpeed 분기). 다르면 cpp 원본 기준으로 교정한다.

2. 검증

미검증:
- 빌드 미검증
- resolver 반환값이 legacy 하드코딩과 byte 단위로 같은지 미검증

검증 명령:
- git diff --check
- & "C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" .\Server\Include\Server.vcxproj /m /p:Configuration=Debug /p:Platform=x64

parity 확인 (이 세션에서는 호출처를 바꾸지 않으므로 값 대조만):
- `SpawnPolicyDB::ResolveLoadout()` == { gold 10000, level 6, LethalTempo x1, respawn 3.0 }
- `ObjectGameDataDB::ResolveStructureMaxHp` == { Turret 3000, Inhibitor 4000, Nexus 5500 }
- `ObjectGameDataDB::ResolveJungleCamp(subKind)` == `GameRoomSpawn.cpp` 스위치 결과
- `ObjectGameDataDB::ResolveMinion(role)` == `ResolveMinionCombatDef(role)`

확인 필요:
- 새로 추가한 `.h/.cpp` 4개 + resolver 4개가 Server(및 Shared) 빌드 프로젝트에 포함되는지 확인.
- `SpawnPolicyDB`/`ObjectGameDataDB`는 Shared/GameSim 소유이며 Engine/Client/Renderer/UI/DX include가 없어야 한다.

후속:
- 이 세션은 데이터 층만 추가하고 호출처는 바꾸지 않는다. 실제 스폰 교체는 02 계획서(S2)에서 진행한다.
