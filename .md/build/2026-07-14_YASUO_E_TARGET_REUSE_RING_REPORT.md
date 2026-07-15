# Session - Yasuo E Target Reuse Ring and Direction Finalization (2026-07-14)

## Outcome

Yasuo E now owns a server-authoritative, per-caster/per-target 10-second reuse lockout. A successful E creates one fixed-radius ring on the dashed-through target; the ring follows that target and erases around its circumference over the same 10-second interval. The dash trail remains attached to Yasuo and uses the accepted command direction, so the visual does not flip after the server dash has already moved the caster through the target.

The current implementation is build-complete. In-game appearance and exact art tuning still require the requested visual pass because compilation cannot verify perceived size, brightness, or clockwise/counter-clockwise feel.

## Current Code Evidence and Ownership

- Server truth: `Shared/GameSim/Champions/Yasuo/YasuoGameSim.cpp`
- Command rejection before cost/action commit: `Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`
- Authoring data: `Data/LoL/ServerPrivate/Gameplay/SkillGameplayDefs.json` and `SkillEffectGameplayDefs.json`
- Generated server data: `Server/Private/Data/Generated/LoLGameplayDefinitions.generated.cpp`
- Client cue bridge: `Client/Private/GameObject/Champion/Yasuo/Yasuo_Skills.cpp` and `YasuoFxPresets.cpp`
- Ring authoring: `Data/LoL/FX/Champions/Yasuo/e_dash_ring.wfx`
- Shared sprite rendering: `Shaders/FxSprite.hlsl` and `Engine/Private/Renderer/RHIFxSpriteRenderer.cpp`

The authoritative flow remains:

`Client command -> Server CanCastSweepingBlade -> Server E relation/action -> replicated accepted effect -> Client target-attached WFX`

## Implemented Behavior

### 1. Per-target E reuse lockout

- `YasuoSweepingBladeLockoutComponent` stores stable source and target `EntityHandle` values plus the absolute expiry tick.
- Lookup uses a sorted deterministic component iteration; different targets therefore maintain independent lockouts.
- `MarkDurationSec=10.0` is authored in both gameplay JSON sources and generated with `paramCount=6`, so parameter index 5 is reachable rather than falling back accidentally.
- The relation component is registered with the checkpoint registry. Expired relations and stale source/target handles are removed during Yasuo GameSim tick; Viego/runtime cancellation also removes relations owned by the caster.
- Cast validation now rejects null, self, dead, friendly/non-targetable, missing-transform, out-of-range, and effectively zero-distance targets before cooldown/action/effect commitment.
- The same target is legal again at the exact expiry tick; a different target remains legal during the first target's lockout.

### 2. Target-following radial cooldown marker

- `SpawnEDashTargetRing` receives the accepted target entity and attaches `Yasuo.E.DashRing` to it.
- The WFX keeps `start_radius=end_radius=1.15`, so the ring does not shrink as a whole.
- `style_mode=5` selects the new `RadialWipe` material path. The shader clips by polar angle using normalized lifetime, producing a circumference wipe rather than uniform scale-down.
- The source texture is a 2x2 atlas. When `atlas_fps=0`, `FxSystem` now deliberately selects frame 0 instead of sampling the entire atlas or advancing frames.
- `kill_when_anchor_invalid=true` prevents the billboard ring from becoming a detached ghost after its target disappears.

### 3. Q/E direction and yaw correction

- Plane-based Yasuo Q and E sprites use `yaw=1.5708` radians (quarter turn), matching the known-good Yone plane convention.
- The E FBX emitter uses `rotation.y=3.1416`; `yaw` is not a MeshParticle rotation field.
- Billboard emitters face the camera and therefore do not respond to authored yaw in the same way.
- E visual direction prioritizes the authoritative accepted command direction. Caster-to-target position is only a fallback, preventing a 180-degree flip after the caster has already crossed the target.
- `q_slash.wfx` is spawned at the cast origin without forward velocity; only the Q3 tornado cue travels.

This explains the earlier `3.14` result: the file was saved, but `yaw=3.14` on an FBX did not affect the mesh because that renderer reads `rotation.y`. For image planes the correct baseline is `1.5708`, not `3.1416`.

### 4. Duplicate and lifecycle safeguards

- The local accepted-hook bridge suppresses the duplicate Yasuo visual dispatch when the legacy hook is also present.
- `FxCuePlayer` transfers WFX `kill_when_anchor_invalid` into billboard runtime cleanup.
- Dash trail and target ring are separate cues, so changing the target marker lifetime does not stretch the short caster trail.

## Verification

- WFX parse: PASS for `e_dash_ring.wfx`, `e_dash_trail.wfx`, and `q_slash.wfx`.
- Gameplay JSON parse: PASS for both Yasuo gameplay definition sources.
- Runtime asset existence: PASS for all referenced ring/trail/Q textures and models.
- Generated data audit: PASS, Yasuo E `paramCount=6`, `MarkDurationSec=10.0`.
- HLSL compile: PASS, `FxSprite.hlsl` `VS` (`vs_5_0`) and `PS` (`ps_5_0`) through Windows SDK `fxc.exe`.
- `Client/Include/Client.vcxproj`, Debug x64: PASS.
- `Server/Include/Server.vcxproj`, Debug x64: PASS.
- `Tools/SimLab/SimLab.vcxproj`, Debug x64: PASS.
- `Tools/Bin/Debug/SimLab.exe 300 42`: PASS.
  - Same-seed hash: `6CDF24584FDAD776`
  - Seed+1 hash: `BA1792B98A5F7844`
- Scoped `git diff --check`: PASS.

## Known Boundaries

- `SkillGameplayDefs.json` still describes Yasuo E target metadata as `Self/Contextual`, while the live command path treats it as a direct unit target. Current runtime validation does not consume that metadata, so behavior is correct today; a future generic target validator must change the source authoring to `Unit/Direct` first.
- The server lockout, client tuning constant, and WFX lifetime all mirror 10 seconds. The accepted effect event does not yet carry the authoritative remaining lockout duration, so changing the value later requires updating all three visual/data mirrors together.
- The broad deterministic SimLab suite passed, but there is not yet a dedicated probe that asserts same-target rejection, different-target acceptance, exact expiry, and checkpoint round-trip as four separate Yasuo E test cases.
- Shader/build checks cannot validate artistic direction. Clockwise direction, first erased edge, perceived diameter, and readability over moving targets remain the in-game visual gate.

## In-game Visual Checklist

1. E target A once: one full ring appears under A and follows A while A moves.
2. Immediately E target A again: server rejects the cast; no new dash or duplicate ring appears.
3. E target B during A's lockout: dash succeeds and B receives its own ring.
4. Watch one ring for 10 seconds: diameter remains fixed while only the circumference wipes away.
5. Kill/despawn the marked target: the ring must disappear instead of remaining at the last position.
6. Cast E in four cardinal directions: trail faces the dash direction in all cases.
7. Cast normal Q and Q3: slash remains at the caster; only the tornado travels.

