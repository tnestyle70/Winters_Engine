# Work Packet: Server-Authoritative White Shields

## Metadata

- ID: `2026-07-14_server_authoritative_white_shields`
- Status: `Handoff`
- Agent: `Codex`
- Owner: Desktop
- Branch: `main` (shared dirty working tree, no commit created)
- Base: current shared working tree after `2026-07-14_champion_shield_fx`
- Started: `2026-07-14`

## Objective

Implement one deterministic server-authoritative timed-shield path for Yasuo passive, Riven E, and Lee Sin W1. Each shield lasts three seconds, absorbs incoming damage before health, replicates through the existing snapshot `shield` field, renders as a semi-transparent white effective-health segment, and drives a target-attached white shield WFX without creating client gameplay truth.

## Owned Paths

- `Shared/GameSim/Components/ShieldComponent.h` (generic timed-shield runtime state only)
- `Shared/GameSim/Systems/Shield/ShieldSystem.cpp` + `.h` (grant, expiry, absorption, snapshot mirror)
- `Shared/GameSim/Core/Checkpoint/WorldKeyframe.cpp` (timed-shield registration only, if required)
- `Shared/GameSim/Include/GameSim.vcxproj` (new shield system compile entry only)
- `Shared/GameSim/Systems/Damage/DamagePipeline.cpp` + `.h` (common shield absorption call only)
- `Shared/GameSim/Systems/Damage/DamageQueueSystem.cpp` (common timed-shield expiration tick only)
- `Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp` (Lee Sin W1 pre-cost safeguard target gate only)
- `Shared/GameSim/Champions/Yasuo/**` (passive shield bridge only)
- `Shared/GameSim/Champions/Riven/**` (E shield hook/data only)
- `Shared/GameSim/Champions/LeeSin/**` (W1 shield and pre-cast target validation only)
- `Server/Private/Game/SnapshotBuilder.cpp` (generic champion shield snapshot selection only)
- `Client/Private/Network/Client/SnapshotApplier.cpp` (shield mirror only, if required)
- `Client/Private/GameObject/Champion/Yasuo/**` (three-second shield visual only)
- `Client/Private/GameObject/Champion/Riven/**` (three-second shield visual only)
- `Client/Private/GameObject/Champion/LeeSin/**` (W1 target attachment/three-second shield visual only)
- `Data/LoL/FX/Champions/Yasuo/passive_shield.wfx`
- `Data/LoL/FX/Champions/Riven/Riven_E_Shield.wfx`
- `Data/LoL/FX/Champions/LeeSin/w1_cast.wfx`
- `Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json` (Riven E / Lee Sin W shield values only)
- `Data/LoL/ServerPrivate/Gameplay/SkillGameplayDefs.json`, `Data/LoL/SharedContract/DefinitionManifest.json`, and `Server/Private/Data/Generated/LoLGameplayDefinitions.generated.cpp` (generated pack outputs)
- `Client/Private/Scene/Scene_InGame.cpp` (existing shield-to-health-bar bridge only)
- `Engine/Public/Manager/UI/WorldHealthBarState.h` (shield view field only, if existing UI data cannot express it)
- `EngineSDK/inc/Manager/UI/WorldHealthBarState.h` (Engine build-synced derivative only)
- `Tools/SimLab/**` (focused deterministic shield probes only)
- `.md/build/2026-07-14_SERVER_AUTHORITATIVE_WHITE_SHIELDS_REPORT.md`
- `.md/build/2026-07-14_CHAMPION_SHIELD_FX_REPORT.md` (supersession note only)
- `.md/collab/work-packets/2026-07-14_server_authoritative_white_shields.md`
- `.md/collab/ACTIVE_WORK_PACKETS.md` (this row only)

## Coordinated / Confirm Before Edit

- `Engine/Private/Manager/UI/UI_Manager.cpp`
- `Engine/Private/Manager/UI/ActorHUDPanel.cpp`

Packet `2026-07-11_s004_loading_hud_bush_retirement` still had a stale `Active` row, but its result document records completed Engine/Client builds and a finished result. The row was reconciled to `Handoff` before any UI edit. This packet may therefore make only narrow shield-segment additions in `UI_Manager.cpp`; `ActorHUDPanel.cpp` remains unchanged because its existing `hp.fill` screen-rect query is sufficient.

## Read-Only / Excluded Paths

- `Shared/Schemas/Snapshot.fbs` and generated schema files unless audit disproves the existing `shield` bridge
- unrelated champion skills, damage modifiers, HUD retirement changes, and existing dirty edits
- source PNG/FBX binaries under `Client/Bin/Resource`
- Jax shield gameplay/visual behavior

## Validation

- Deterministic SimLab probes: grant, exact three-second expiry, partial absorption, depletion, health spillover, recast replacement/refresh, and snapshot-visible current shield.
- WFX parse and semantic checks: main shell lifetime three seconds, white RGBA tint, target attachment for Lee Sin W1.
- Scoped `git diff --check` followed by Debug x64 `GameSim`, `SimLab`, `Server`, and `Client` builds.
- Record any UI ownership limitation and the exact in-game verification checklist in the build report.

## Handoff Notes

- Added one generic deterministic `ShieldComponent` / `CShieldSystem` path with finite input validation, replacement/refresh semantics, post-resistance absorption, exact `T+90` expiry, and keyframe v5 registration.
- Wired Riven E (70), Lee Sin W1 (80), and Yasuo full-Flow passive (100 current FlowMax) to the common server truth. Lee Sin W1 now rejects invalid targets before cost/cooldown/event commitment.
- Generalized snapshot replication and added semi-transparent white local/world health-bar segments in both RHI and fallback paths without duplicate drawing.
- Converted all three shield WFX to white main layers and three-second lifetime.
- Debug x64 GameSim, SimLab, Server, and Client builds pass. Focused and full 1,800-tick SimLab suites pass. See the build report for exact hashes, known WFX lifecycle boundaries, and the in-game checklist.
