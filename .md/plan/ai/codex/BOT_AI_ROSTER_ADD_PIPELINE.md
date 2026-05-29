# Bot AI Roster Add Pipeline

Created: 2026-05-13

Purpose: give future bot additions a small, repeatable checklist that preserves the current S10 server-authoritative AI invariant.

## Invariant

Bot AI is a GameCommand producer and must not directly mutate gameplay truth.

```text
Bot reads CWorld
-> Bot emits GameCommand
-> CDefaultCommandExecutor validates/applies it
-> Server Snapshot/Event
-> Client animation/fx only
```

## Current Entry Points

```text
Server/Private/Game/GameRoom.cpp::EnsureBotAIStage1SmokeRoster
Server/Private/Game/GameRoom.cpp::SpawnChampionForLobbySlot
Shared/GameSim/Definitions/MapSpawnPoints.cpp::GetGameSimRosterLane
Shared/GameSim/Systems/BotLaneAIPolicy.cpp::GetBotChampionProfile
Shared/GameSim/Systems/BotLaneAISystem.cpp::CBotLaneAISystem::Execute
Shared/GameSim/Systems/CommandExecutor.cpp::CDefaultCommandExecutor::ExecuteCommand
```

## Add A Smoke Bot

1. Reserve a smoke roster slot in `GameRoom.cpp`.
2. Add `ConfigureBotSmokeSlot(...)` in `EnsureBotAIStage1SmokeRoster`.
3. Map the slot to the intended lane in `GetGameSimRosterLane`.
4. Add or reuse a `BotChampionProfile` in `BotLaneAIPolicy.cpp`.
5. Use only command paths that are already server-owned or executor-validated.
6. Verify logs show both `BotAI` command production and `Command` acceptance.

## Ezreal Mid Smoke Baseline

```text
slot 0: human, blue
slot 1: blue Jax top
slot 2: blue Ezreal mid
slot 5: red Sylas dummy
slot 6: red Fiora top
slot 7: red Ashe mid
```

Ezreal's Stage1 bot profile is intentionally Q-focused. Q is the current server-owned Ezreal projectile standard, so the bot can poke/fight Ashe without adding new gameplay truth writes inside AI.

Expected logs:

```text
[BotAI] Stage1 smoke bots enabled: blue Jax top, blue Ezreal mid, red Fiora top, red Ashe mid
[BotAI] ... champ=12 team=0 lane=1 ... cmd=CastSkill slot=1 reason=utility-kite-skill ...
[Command] cast-skill accept reason=ok champion=12 ... slot=1 ...
[SkillProjectile] queued kind=... source=... slot=1 ...
[BotAI] ... champ=7 team=1 lane=1 ... cmd=CastSkill slot=2 reason=bt-ashe-w-volley ...
```

## Gotchas

- Do not move a bot by writing `TransformComponent` from AI.
- Do not apply damage from AI.
- Keep lane targeting range-gated before priority comparisons.
- Prefer one champion-specific profile first, then graduate repeated behavior into shared policy helpers.
