# Winters Current Codebase Brief

This file is the compact project-context bridge for agents. It is not an archive, changelog, or full inventory.

Read this when work touches gameplay, networking, Shared/GameSim, server authority, AI, champion skills, animation replication, or FX cues.

## Core Direction

- Winters is moving client-only combat feel onto a server-authoritative GameSim.
- The fixed ownership flow is:

```text
Client Input -> GameCommand -> Server GameSim -> Snapshot/Event -> Client Visual
```

- Server owns gameplay truth: position, movement, HP, mana, cooldowns, damage, hit validation, skill validation, projectiles, structures, minions, game end, and bot command generation.
- Client owns presentation: input send, weak prediction where allowed, interpolation, rendering, animation playback, FX playback, UI, ImGui, and debug trace.
- Client code must not become the source of authoritative gameplay results unless the task explicitly targets an offline/local-only smoke path.

## Required Mental Model

- `Shared/GameSim` is the shared deterministic gameplay layer used by server authority.
- Server-side command execution consumes `GameCommand` and mutates gameplay truth.
- Client receives server Snapshot/Event data and applies visual state, animation, FX, and UI.
- Bot AI is a server-side `GameCommand` producer. It must not directly mutate Transform, HP, cooldowns, damage, or other gameplay truth.
- Local prediction is only a temporary feel layer. It must not fork permanent truth away from server Snapshot/Event results.

## Hooks And Cues

- Gameplay execution belongs in Shared/Server gameplay hooks, not client visual hooks.
- Client `VisualHookRegistry` is for animation/FX/sound presentation driven by server cues.
- Legacy `CSkillHookRegistry` may still exist for older local-only or compatibility paths, but do not route authoritative server events back into legacy gameplay hooks.
- FX should be server cue single-source: a server event/cue should produce one client visual playback, not duplicate local plus network playback.
- For champion skills, check both sides before changing behavior:
  - server/shared gameplay acceptance and simulation
  - client Snapshot/Event or FX cue application
  - client visual hook registration and renderer path

## Current Slice Priorities

The current priority is not advanced AI. It is stabilizing five-client server authority and restoring the combat feel on top of that path.

Stable direction:

```text
S2_LoadingBarrier
S3_StageStructureBinding
S4_MovementVerticalSlice
S5_AnimationReplicationSingleSource
S6_FxCueSingleSource
S7_BasicAttackVerticalSlice
S8_ServerSkillStandard
S10_BotAIStage1
```

When a task touches these areas, preserve the sequence unless the user explicitly changes priority.

## High-Risk Mistakes

- Do not make a client-only visual fix that changes gameplay truth.
- Do not add a debug flag that silently changes normal F5 runtime behavior.
- Do not hide roster, map, minion, or champion systems with forced flags unless the user explicitly asks for a temporary local visual lab.
- Do not treat client-only render filtering as load-time or gameplay isolation. Server Debug may still spawn smoke bots, minions, structures, and events.
- Do not use server logs as proof that client FX rendered. Server logs prove simulation; client FX needs event/cue application and renderer verification.
- Do not register both legacy skill hooks and visual hooks in a way that plays the same FX twice.
- Do not let bot AI directly edit gameplay components. It should emit commands only.

## Read Next

Use these only when the task touches the domain:

- Server authority master context: `.md/TODO/05-09/ServerAICompletion.md`
- Champion server rules: `.md/TODO/05-11/CHAMPION_SERVER_AUTHORITY_SUCCESS_RULES.md`
- Bot command handoff: `.md/TODO/05-11/AI_STAGE1_SERVER_COMMAND_HANDOFF.md`
- Successful Stage1 server AI smoke notes: `.md/TODO/05-11/STAGE1_SERVER_AI_SMOKE_SUCCESS.md`
- Engine conventions: `.md/architecture/WINTERS_ENGINE_CONVENTIONS.md`

## Verification Baseline

- For client-facing gameplay/FX changes, build Client Debug x64.
- For server-authoritative gameplay changes, build both Server Debug x64 and Client Debug x64.
- Runtime proof should distinguish:
  - server sim acceptance logs
  - server Snapshot/Event or FX cue emission
  - client event/cue application logs
  - actual visual rendering

## Document Rule

Keep this brief short. Add only behavior-changing context and pointers that an agent cannot reliably infer from `rg` alone.
