# Winters DataDriven Query Cutover Report

Date: 2026-06-23

## Result

DataDriven runtime query cutover passed.

This slice did not delete the legacy `ChampionGameDataDB` yet. It moved high-frequency authoritative gameplay readers away from direct DB calls and into a single `GameplayDefinitionQuery` bridge that reads the current tick's immutable `GameplayDefinitionPack`.

## Essence

Frame code should not know whether a value came from legacy generated tables, JSON cook output, or a future hot-reload pack.

Frame code should ask only:

```text
entity + slot + current tick definitions -> gameplay fact
```

The atomic runtime identity chain is now:

```text
EntityHandle / EntityID
-> ChampionDefinitionComponent
-> SkillLoadoutComponent
-> GameplayDefinitionPack
-> ChampionGameplayDef / SkillGameplayDef / SummonerSpellGameplayDef
```

Legacy fallback remains inside `GameplayDefinitionQuery` only so old client smoke and partially migrated paths do not regress while reader count is reduced.

## Code Changes

- Added `TickContext::pDefinitions`.
- Server tick now attaches `ServerData::GetLoLGameplayDefinitionPack()` to every simulation tick.
- Added `Shared/GameSim/Definitions/GameplayDefinitionQuery.h/.cpp`.
- Added `GameplayDefinitionQuery.cpp` to `Shared/GameSim/Include/GameSim.vcxproj`.
- Migrated `ChampionAI` gameplay reads:
  - attack range fallback
  - skill action lock
  - skill range
  - flash range
- Migrated `MoveSystem` action-lock read.
- Migrated `CommandExecutor` gameplay reads:
  - cooldown
  - skill range
  - two-stage checks
  - stage window
  - Kalista passive dash distance/duration/grace
  - flash range/cooldown
- Migrated champion hook range reads:
  - Kalista E rend range
  - Sylas E chain range
  - Sylas R hijack range

## What Was Deliberately Left

Direct `ChampionGameDataDB` remains where the current contract is not yet purely gameplay:

- `CommandExecutor` visual yaw reads.
- `MoveSystem` visual yaw read.
- Zed/Irelia visual yaw reads.
- Basic attack/action timing reads still coupled to animation/action duration.
- Client visual prediction/timing compatibility reads in `Scene_InGame`.
- Legacy facades: `ChampionRuntimeDefaults`, `ChampionStatsRegistry`, and `ChampionGameDataDB` itself.

These should be removed only after pose/facing/timing contracts are split into gameplay action timing and client visual timing.

## Verification

Command:

```powershell
powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1
```

Result:

```text
Definition pack freshness: PASS
Legacy ownership audit: PASS
Client visual timing parity: mismatchCount 0
GameSim Debug x64: PASS
Server Debug x64: PASS
Client Debug x64: PASS
SimLab Debug x64: PASS
SimLab same-seed hash: 67F2A97563B8DB04
SimLab seed+1 hash: 5DA19645E291A29B
git diff --check: PASS
```

Known warnings remain existing encoding / DLL-interface warnings:

```text
C4828
C4251 / C4275
LF will be replaced by CRLF
```

## Next Cut

1. Split gameplay action timing from client visual timing.
2. Move `visualYawOffset`, `animPlaySpeed`, `castFrame`, and `recoveryFrame` fully out of Shared authoritative readers.
3. Replace remaining client prediction reads with ClientPublic visual definition queries or server replicated action data.
4. Collapse `ChampionRuntimeDefaults` and `ChampionStatsRegistry` behind the new pack query.
5. Delete `ChampionGameDataDB` only after direct runtime reader count reaches zero and the same pipeline passes.
