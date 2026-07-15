# Session - Lee Sin W Referenced Champion Shield FX (2026-07-14)

> Current-status note: [2026-07-14_SERVER_AUTHORITATIVE_WHITE_SHIELDS_REPORT.md](2026-07-14_SERVER_AUTHORITATIVE_WHITE_SHIELDS_REPORT.md) supersedes this report's runtime statements that Riven E is visual-only, Riven E lasts 1.5 seconds, and Yasuo passive lasts one second. This document remains the historical record of the initial FX composition pass.

## Outcome

Jax R, Riven E, and Yasuo passive now use the same successful visual composition principle as Lee Sin W: an actor-attached, bounded-lifetime shield made from a primary shell plus additive rim and outer-glow layers. Existing champion-specific FBX meshes and particle textures remain the shape source; the Lee Sin bubble/rim textures provide the readable spherical contour.

Jax E's animation replay/frame bug is also documented below with its exact cause and the code fix already present in the tree.

## Reference Audit and Asset Decision

`Data/LoL/FX/Champions/LeeSin/w1_cast.wfx` does not render a shield FBX. Its successful look comes from five attached additive billboard/decal layers, especially:

- `leesin_base_w_shield_1.png` for the main circular bubble/rim.
- `leesin_base_w_glow_outer.png` for the soft outer contour.

The supplied `Client/Bin/Resource/Texture/UI/이펙트 이미지/Jax/09.png` and `Riven/13.png` files are composed gameplay reference captures containing champion, terrain, and HUD pixels. They are therefore used as color/shape references only and are not sampled as runtime RGBA textures. Mapping either capture onto an FBX would project the screenshot background and UI into the shield.

## Implemented Visuals

### Jax R

Existing route retained:

`Jax R cast-frame visual hook -> SpawnRBuffAura -> Jax.R.Aura -> Data/LoL/FX/Champions/Jax/r_aura.wfx`

- Existing `jax_base_r_shield_geo.fbx` and `jax_base_r_fresnelmask.png` remain the 3D shell.
- Added Lee Sin-style main bubble and outer-glow layers, tinted amber/orange to match `Jax/09.png`.
- Converted the previous violet outer shell to amber so the long-lived aura reads as a single gold/orange shield rather than two competing palettes.
- Eight emitters now cover mesh shell, two bubble contours, body glow, outer shell, cast halo, ground sparks, and spotlight.
- Long-lived layers follow Jax for the authored 8-second R duration; billboard/decal layers carry bounded lifecycle and anchor-invalid cleanup.

### Riven E

Existing route retained:

`Riven E cast-frame visual hook -> SpawnEShield -> Riven.E.Shield -> Data/LoL/FX/Champions/Riven/Riven_E_Shield.wfx`

- Existing `riven_base_e_shieldmesh01.fbx` and `riven_base_e_sheildmeshtext.png` remain the green 3D shell.
- Added Lee Sin-style main bubble and outer-glow layers, tinted bright Riven green to match `Riven/13.png`.
- Seven emitters now cover mesh, bubble rim/glow, multiplicative shell, flare, rune, and ground flash.
- All attached billboard/decal layers have 1.5-second-or-shorter bounded lifetimes and anchor-invalid cleanup.
- Offline/practice previously dispatched both the Riven VisualHook and legacy SkillHook, producing the same E WFX twice. `Scene_InGame.cpp` now suppresses the VisualHook only when the Riven legacy cast hook is present. The network-authoritative path remains a single replicated EffectTrigger and cue-key dedupe.

### Yasuo passive

New authoritative visual route:

`DamageQueueSystem detects first real passive shield activation -> EffectTrigger(0x0051) -> VisualHookRegistry -> SpawnPassiveShield -> Yasuo.Passive.Shield`

- The event is emitted only when Flow was full before damage and that damage actually activated/used the passive shield.
- Subsequent hits against an already-active shield do not emit another activation effect.
- The effect follows Yasuo for one second and uses four emitters: Yasuo passive sphere FBX, cyan Lee Sin rim, cyan outer glow, and the Yasuo passive smoke atlas.
- The effect flags deliberately carry a non-basic-attack slot, preventing EventApplier from starting a BasicAttack animation for this passive-only cue.
- Client cue-key dedupe provides a second guard against replaying the same authoritative event.

## Jax E Animation Frame/Replay Fix

The Jax E authoring is:

- Stage 1 loop: `spell3_idle_cycle`, cast frame 1, recovery frame 48.
- Stage 2 release: `spell3_attack1`, cast frame 6, recovery frame 14.

The replay bug was not caused by those frame values. The network animation guard compared the renderer's current resolved animation name to the short authored key using exact equality. Jax's resolved model animation name contains the authored key but is not always byte-for-byte identical, so the guard believed the correct loop was not playing and restarted it from frame 0 on repeated updates.

`Client/Private/Scene/Scene_InGameNetwork.cpp` now uses:

`pCurrentAnim->GetName().find(animName) != std::string::npos`

That recognizes the already-running prefixed/resolved animation and stops the per-update restart. The stage 1 and stage 2 frame windows in `Jax_Registration.cpp` remain unchanged and are now allowed to advance normally.

## Verification

- WFX parse: PASS.
  - `Jax.R.Aura`: 8 emitters.
  - `Riven.E.Shield`: 7 emitters.
  - `Yasuo.Passive.Shield`: 4 emitters.
- Runtime asset existence: PASS for every referenced FBX and PNG.
- Cue names: PASS and unique within the loaded WFX tree.
- Passive event/visual registration compile: PASS after adding the explicit Shared gameplay-hook header include found by the first Client build.
- `Client/Include/Client.vcxproj`, Debug x64: PASS.
- `Server/Include/Server.vcxproj`, Debug x64: PASS.
- `Tools/SimLab/SimLab.vcxproj`, Debug x64: PASS.
- `Tools/Bin/Debug/SimLab.exe 300 42`: PASS, deterministic hash `6CDF24584FDAD776`.
- Scoped `git diff --check`: PASS.

## Explicit Boundaries and Follow-ups

- **Riven E is visual-only on the server-authoritative path today.** `RivenGameSim::RegisterHooks()` registers Q/W/R but not E, and `DamagePipeline` has no Riven shield consumption. The replicated E cast still produces the completed shield WFX, but it does not yet create authoritative shield HP. Adding that gameplay truth is a separate GameSim task, not hidden inside this FX pass.
- Riven Q's offline on-accepted path can still dispatch both its VisualHook and legacy SkillHook. That pre-existing Q-only duplication is outside this E shield change.
- Yasuo `Passive_Trigger(0x0051)` is not a normal skill-slot variant, so EventApplier resolves `ctx.pDef` as BasicAttack. The current passive callback ignores `pDef`, making this safe; any future callback expansion must not assume that definition is the passive.
- Yasuo's one-second passive duration is currently mirrored in server event data, client callback, and WFX rather than forwarded through `VisualHookContext`.
- Build verification does not replace the final visual pass. Shield sphere radius, vertical offset, alpha, and bloom should be tuned in WFX after the requested in-game comparison.

## In-game Visual Checklist

1. Jax R: amber sphere follows Jax for eight seconds; no violet shell remains; mesh and circular rim overlap around the body.
2. Riven E: one green shield appears per cast, follows the dash, and fades by 1.5 seconds without double brightness.
3. Yasuo passive: fill Flow, receive damage, and confirm one cyan shield activation; further shielded hits must not restart it.
4. Move, die, and despawn during each effect to confirm no billboard/decal remains at the old position.
5. Jax E stage 1: the counter loop advances instead of restarting at frame 0; stage 2 release reaches cast frame 6 and recovers at frame 14.
