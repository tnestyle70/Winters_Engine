# Winters DataDriven Spawn/Object Policy Report

Date: 2026-06-23

## Result

Spawn/object policy cutover passed.

This slice moved server spawn gameplay values out of `GameRoomSpawn.cpp` and into small data definitions plus read-only resolvers. Runtime behavior is intended to stay identical: the same level, gold, rune, respawn delay, collider profile, structure values, jungle values, and minion combat values are still used.

## Essence

Spawn code should assemble entities. It should not own balance values.

The atom split is now:

```text
value shape
-> read-only resolver
-> server entity assembly
-> world components
```

Concretely:

```text
SpawnLoadoutPolicyDef
-> SpawnPolicyDB::ResolveLoadout()
-> SpawnChampionForLobbySlot / death respawn
-> StatComponent / GoldComponent / RuneLoadoutComponent / RespawnComponent
```

```text
StructureGameDef / JungleCampGameDef / ChampionColliderProfileDef / MinionCombatDef
-> ObjectGameDataDB
-> SpawnServerStructure / SpawnServerJungleFromStageEntry / SpawnServerMinion / SpawnChampionForLobbySlot
-> server gameplay components
```

Champion-specific sim state attachment is assembly logic, not balance data:

```text
eChampion
-> ChampionSimComponentTable
-> AttachChampionSimComponents()
-> champion sim-only components
```

## Code Changes

- Added spawn policy data:
  - `Shared/GameSim/Definitions/SpawnLoadoutPolicyDef.h`
  - `Shared/GameSim/Registries/SpawnPolicy/SpawnPolicyDB.h/.cpp`
- Added object gameplay data:
  - `Shared/GameSim/Definitions/ChampionColliderProfileDef.h`
  - `Shared/GameSim/Definitions/StructureGameDef.h`
  - `Shared/GameSim/Definitions/JungleCampGameDef.h`
  - `Shared/GameSim/Registries/ObjectGameData/ObjectGameDataDB.h/.cpp`
- Added server assembly table:
  - `Server/Private/Game/Factory/ChampionSimComponentTable.h/.cpp`
- Updated server spawn/death paths:
  - `Server/Private/Game/GameRoomSpawn.cpp`
  - `Server/Private/Game/GameRoom.cpp`
  - `Server/Private/Game/GameRoomInternal.h`
- Registered new compile units:
  - `Shared/GameSim/Include/GameSim.vcxproj`
  - `Server/Include/Server.vcxproj`

## What Changed In Practice

`GameRoomSpawn.cpp` no longer owns these champion spawn values directly:

```text
startLevel = 6
startGold = 10000
startRune = LethalTempo
startRuneCount = 1
respawnDelaySec = 3
championCollider = radius x 1.8 x radius, offset y 0.9
```

`GameRoomSpawn.cpp` no longer owns these object combat values directly:

```text
turret/inhibitor/nexus HP
turret AI range/cooldown/damage/projectile speed/sight/collider
jungle HP/radius/attack/move/armor/MR
minion combat resolver entrypoint
```

The old champion sim component `if` chain is now a server factory table:

```text
YASUO -> YasuoStateComponent
ASHE -> AsheSimComponent
ANNIE -> AnnieSimComponent
FIORA -> FioraSimComponent
JAX -> JaxSimComponent
VIEGO -> ViegoSimComponent
YONE -> YoneSimComponent
LEESIN -> LeeSinSimComponent
KINDRED -> KindredSimComponent
MASTERYI -> MasterYiSimComponent
```

## What Was Deliberately Left

Map/stage placement values remain in `GameRoomSpawn.cpp` because those are placement fallback data, not gameplay balance data:

```text
fallback structure positions
fallback structure tiers/lanes
structure radius by stage kind/tier
minion body collider
```

The data values are still compiled POD defaults, not cooked JSON output yet. That is intentional for this slice: first move ownership and readers, then replace the resolver backing store with generated/cooked data after parity is stable.

The legacy audit still reports `serverObjectHardcode = 29`. This is lower than the previous `43`, but the counter is broad and still catches items like bot skill-rank policy, wave tuning, and generic `gold.amount` assignments. It should be refined or split once spawn/object ownership has a cooked JSON source.

## Verification

Command:

```powershell
powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1
```

Result:

```text
Definition pack freshness: PASS
Build hash: 0x58678ADB
Client visual timing parity: mismatchCount 0
GameSim Debug x64: PASS
Server Debug x64: PASS
Client Debug x64: PASS
SimLab Debug x64: PASS
SimLab same-seed hash: 67F2A97563B8DB04
SimLab seed+1 hash: 5DA19645E291A29B
git diff --check: PASS
LoLDataDriven pipeline: PASS
```

Known warnings remain existing encoding / DLL-interface / line-ending warnings:

```text
C4828
C4251 / C4275
LF will be replaced by CRLF
```

## Next Cut

1. Move `SpawnLoadoutPolicyDef` and `ObjectGameDataDB` backing values from compiled defaults into `Data/LoL/ServerPrivate` JSON plus generated server pack.
2. Split bot skill-rank policy into data so `AssignDefaultBotSkillRanks()` stops owning level-up order.
3. Split map placement fallback from server spawn assembly so fallback stage objects become data, not code.
4. Extract minion collider/body profile into object data, keeping wave timing separate as match-flow data.
5. Keep deleting no legacy path until the generated data path has parity and the same pipeline passes.
