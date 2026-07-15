# Yasuo EQ FX and Allied Minion Vision Report

- Date: 2026-07-14
- Status: source/type/WFX validation PASS; fresh-build runtime A/B PENDING
- Scope: Yasuo E ring, E-to-Q input buffer, EQ wind layer, tornado hit billboard, replicated allied-minion vision

## Current-code audit

### E during Q input

The Client is already sending Q while Yasuo E is active. The loss occurs on the authoritative side: Yasuo E creates a `ForcedMotion` `ActionStateComponent`, and `CDefaultCommandExecutor::ExecuteCommand` rejects Q as `ActionBlocked` before `HandleCastSkill` can call `YasuoGameSim::ResolveQVariantStage`.

The downstream EQ path already exists and should not be duplicated:

1. `ResolveQVariantStage` returns stage 4 while E is active.
2. `OnQ` applies caster-centered area damage and optional airborne for a charged Q.
3. the server emits `ActionStart` and `EffectTrigger` with stage 4.
4. `CEventApplier` maps stage 4 to animation key `spell1c`.
5. `Yasuo::Visual::OnCastAccepted_Q_Visual` calls `YasuoFx::SpawnEQRing`.

The fix therefore stores one Q intent during the E lock and runs that original command once through the existing executor after the E action lock and dash finish. The buffer is a checkpoint-registered GameSim component so replay, Chrono restore, bots, and human clients follow the same authority path.

Current timing from gameplay definitions is E lock `0.4 s` (12 ticks at 30 Hz), dash `0.25 s` (about 8 ticks), and E-active stage window `0.5 s` (15 ticks). The expected transition is `spell3 -> spell1c` at E start +12 ticks.

### E, EQ, and tornado-hit visuals

`Yasuo.E.DashTrail` is already called, but the concurrently saved `e_dash_trail.wfx` no longer contains the prior ring emitter. The requested ring texture is a 2x2, four-frame atlas and currently has no live reference. To prevent overwriting another WFX session, the ring is split into `Yasuo.E.DashRing` and played alongside the existing trail.

`yasuo_basic_attack_wind_ring_02.png` and `yasuo_q_tornado_blast.png` are also currently unused. They are introduced as independent additive WFX cues so color/alpha/scale/lifetime remain editable without modifying PNG pixels. The tornado cue is assigned only to `pszAttachedCue`; using a spawn cue would duplicate the existing traveling tornado visual.

All three source PNGs contain opaque black background pixels, so the WFX uses additive blending and `alpha_clip: 0.0` rather than alpha blending.

### Allied minion vision

The server already spawns minions with `SpatialAgentComponent`, `VisionSourceComponent`, and `VisibilityComponent`. Local-only Client minions do too. Only server-replicated Client minions are missing `VisionSourceComponent`: their snapshot path creates gameplay state, then defers spatial/visibility setup until asynchronous visual construction, and never adds a vision source.

The repair is an idempotent snapshot runtime-tag helper that installs `SpatialAgent(Unit)`, data-derived `VisionSource`, and `Visibility` before `QueueNetworkVisual`. It reuses the existing 10 Hz Vision system, unit-cell target-query dedupe, spatial index, and local-team-only FoW raster. No new scan loop, cache, schema field, or Engine system is added.

FoW source positions are intentionally not cell-deduplicated: collapsing multiple minions inside an 8 m cell to the first position can visibly remove valid sight near the cell boundary. Performance acceptance is therefore measured rather than achieved by reducing correctness.

## Conflict record

During this audit, another session rewrote `Data/LoL/FX/Champions/Yasuo/e_dash_trail.wfx` from six emitters to three and removed the smoke/ring/spark layers. This packet treats that file as read-only and creates separate WFX files. The change is not reverted or overwritten.

## Implemented result

### Server-authoritative E-to-Q buffer

- `CDefaultCommandExecutor` intercepts only Yasuo Q commands rejected by the active E forced-motion lock.
- The first Q command is stored in a checkpoint-registered `YasuoEqInputBufferComponent`; repeated Q input during the same E is coalesced.
- Buffering does not spend mana, start cooldown, replace the E `ActionState`, or emit Q gameplay/FX immediately.
- After both the E action lock and dash component end, the original command is reconstructed with its sequence, session, issued tick, rewind, target, direction, and ground position and is passed through the existing executor exactly once.
- `bExecuting` makes both Q-stage resolution sites select stage 4. The existing authoritative path then emits stage-4 action/effect state, and the Client maps it to `spell1c` plus `SpawnEQRing`.
- A changed E action sequence, cast-blocking state, invalid handle, champion replacement, or `CancelRuntime` discards the buffer. A dead issuer is rejected by the normal executor and cannot fire the Q.

### Yasuo visual layers

- `Yasuo.E.DashRing` is played beside the existing dash trail, attached to Yasuo. Its four-frame 2x2 atlas shrinks from radius `1.15` to `0.18` over its lifetime.
- `Yasuo.EQ.WindRing` is added to the existing EQ ground/inner-wind composition. RGBA, size, and lifetime are data-only WFX values for follow-up tuning.
- `Yasuo.Q.TornadoHit` is registered as the tornado projectile's target-attached contact cue. The existing `contactOrdinal` key prevents duplicate playback and the traveling tornado visual is not duplicated.
- Existing `e_dash_trail.wfx`, `eq_ring.wfx`, and `eq_inner_wind.wfx` were not overwritten.

### Replicated allied-minion vision

- The network minion snapshot path now idempotently installs or refreshes `SpatialAgent(Unit)`, role-derived `VisionSource`, and `Visibility` before asynchronous visual creation.
- The spatial radius/team match server spawn data. Sight ranges remain the shared melee/ranged/siege/super/Tibbers values `12/14/16/14/14 m`.
- Both teams keep vision-source components, while the existing Engine FoW path filters to the locally controlled team. Team control changes therefore need no component rebuild.
- The existing 10 Hz Vision update, unit-cell target dedupe, and spatial-index queries are reused; no second scan/cache/update loop was introduced.

## Validation record

- Changed translation-unit Debug x64 syntax/type gates using the current project tlog defines/includes and `cl.exe /Zs`: PASS.
  - `Shared/GameSim/Champions/Yasuo/YasuoGameSim.cpp`
  - `Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`
  - `Server/Private/Game/GameRoomTick.cpp`
  - `Client/Private/GameObject/Champion/Yasuo/YasuoFxPresets.cpp`
  - `Client/Private/GameObject/Projectile/ProjectileVisualCatalog.cpp`
  - `Client/Private/Network/Client/SnapshotApplier.cpp`
- Two Client files initially reported compiler `C1041` while parallel `/Zs` processes shared a PDB. Sequential reruns with `/FS` passed; this was compiler-output contention, not a source error.
- `Tools/Harness/Check-SharedBoundary.ps1`: PASS, exit 0.
- New WFX JSON parse: 3/3 PASS. Full WFX scan: 123 files, zero parse failures. Each new cue has one definition and one C++ reference, and every texture path exists.
- Scoped `git diff --check`: PASS; only the repository's existing LF-to-CRLF notices were printed.
- Independent semantic review: PASS for single-fire execution, action-sequence validation, stage 4 propagation, checkpoint/replay state, cancellation, and no immediate cooldown/mana/event mutation.
- Live-process preservation: PASS. `WintersGame.exe` PID 57024 and `WintersServer.exe` PID 25288 were neither terminated nor overwritten.

## Deferred fresh-build runtime gate

The running Client and Server were started before these source changes, so they cannot prove or display this implementation. A normal Debug link was intentionally not performed over those live binaries. New WFX discovery also requires a fresh Client start or an explicit WFX reload.

After the current test session closes, rebuild both Debug targets and verify in this order:

1. E alone: the ring follows Yasuo, plays the four atlas frames, and shrinks toward the end.
2. E then Q during the 0.4 s lock: Q does not fire immediately; when the E lock/dash ends, one `spell1c` circular Q and one EQ FX composition appear.
3. Repeat Q during the same E: only the first buffered Q executes.
4. Q3 tornado through one or more enemies: `yasuo_q_tornado_blast.png` appears once on each contacted target.
5. Move an allied network minion beyond champion sight: its role-specific FoW circle reveals terrain; an enemy minion does not reveal local FoW.
6. In the same 5v5 scene, record `Vision::TickVisibility` and FoW profiler deltas against the previous build.

One non-blocking timing detail remains: the buffered Q is released in the Yasuo late tick after the cooldown-system phase, so its displayed cooldown retains one additional fixed tick compared with an ordinary early-phase Q cast. Gameplay execution and single-fire EQ behavior are unaffected.
