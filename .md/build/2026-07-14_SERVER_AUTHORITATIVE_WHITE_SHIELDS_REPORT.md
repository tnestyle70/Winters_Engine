# Session - Server-Authoritative White Shields (2026-07-14)

> Current status (2026-07-20): the common server-authoritative shield amount, absorption, three-second duration, snapshot bridge, and white effective-health bar remain current. The white WFX palette for Lee Sin W1 and Riven E is superseded by the explicit champion-specific green palette in `2026-07-20_LEESIN_RIVEN_WFX_TINT_REGRESSION_RESULT.md`; Yasuo passive WFX remains white.

## Outcome

Yasuo passive, Riven E, and Lee Sin W1 now share one deterministic server-authoritative shield path. The server owns amount, absorption, replacement, and exact expiry; the existing snapshot `shield` field transports remaining effective health; local and world health bars render the remaining amount as a semi-transparent white segment; and the three actor-attached shield WFX use white RGB and a three-second authored lifetime.

This report supersedes the runtime-status portions of `2026-07-14_CHAMPION_SHIELD_FX_REPORT.md` that described Riven E as visual-only, Riven E as 1.5 seconds, and Yasuo passive as one second. That older report remains the historical record of the first FX composition pass.

## Authoritative Flow

```text
Client Cast / Incoming Damage
    -> Server GameSim champion rule
    -> CShieldSystem::Grant / Absorb / Execute
    -> ChampionComponent.shield mirror
    -> EntitySnapshot.shield
    -> Client ChampionComponent.shield
    -> local HUD + world health bar

Server accepted cast / Yasuo passive activation
    -> replicated EffectTrigger
    -> client visual hook
    -> actor-attached white WFX (3 seconds)
```

`ShieldComponent` is the server gameplay truth. Champion-specific Yasuo/Riven fields are compatibility mirrors only; the client does not decide absorption, expiry, or remaining amount.

## Implemented Server Simulation

### Common timed shield

- Added `Shared/GameSim/Components/ShieldComponent.h` with current amount, original maximum, and absolute expiry tick.
- Added `Shared/GameSim/Systems/Shield/ShieldSystem.*`:
  - `Grant`: validates finite positive data, creates or replaces the current shield, and refreshes expiry.
  - `Absorb`: consumes post-resistance damage before health and returns only overflow damage.
  - `Execute`: deterministically iterates shield owners and clears depleted, expired, dead, or invalid shields.
  - `Clear`: removes the authoritative component and zeroes snapshot/compatibility mirrors.
- Three seconds is exactly 90 ticks at the 30 Hz simulation rate. A shield granted at tick `T` is active through `T+89` and inactive at `T+90`.
- Recasts use replacement/refresh semantics, not additive stacking. The newest grant becomes the one active shield and receives a fresh three-second window.
- Zero damage cannot consume Yasuo Flow or activate a shield. NaN/Inf grant inputs are rejected.
- `ShieldComponent` is registered in keyframe version 5, so amount and expiry survive deterministic save/restore.

### Champion connections

| Champion | Server trigger | Amount | Duration | Result |
|---|---|---:|---:|---|
| Riven | E cast frame | 70 | 90 ticks | Shield is granted to Riven. |
| Lee Sin | Valid W1 cast on an allied champion/minion/ward | 80 | 90 ticks | Shield is granted to Lee Sin, then the existing target dash begins. |
| Yasuo | First positive incoming damage while Flow is full | `FlowMax` (currently 100) | 90 ticks | Flow becomes 0 and the same hit is absorbed by the new shield. |

Riven and Lee Sin amounts/durations are authored in `Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json` and propagated through the generated definition pack. The checked pack hash is `0xC35A14A7`.

Lee Sin W1 now calls `CanCastSafeguard` before resource/cooldown/action/effect commitment. Invalid enemy, null, self, dead, missing-transform, and out-of-range targets return `ChampionRuleBlocked`; they no longer consume mana/cooldown or play an accepted-cast cue without producing the dash/shield.

### Damage order

The relevant order is:

```text
raw/scaled damage -> crit -> typed resistance -> common timed shield -> Annie E legacy shield -> HP
```

This gives the common shield real effective-health behavior: damage below the remaining shield leaves HP unchanged, while only the excess reaches HP.

## Replication and Presentation

### Snapshot bridge

- No FlatBuffers schema change was required; `EntitySnapshot.shield` already existed.
- `SnapshotBuilder` now copies `ChampionComponent.shield` for every champion instead of selecting only Yasuo state.
- `SnapshotApplier` already copies the value into the client `ChampionComponent.shield`.
- `Scene_InGame` forwards the value to both `ActorHUDState.Shield` and `UIWorldHealthBarDesc.fShield`.
- The generated EngineSDK health-bar header was synced from the Engine public header and verified byte-for-byte.

### Semi-transparent white effective-health bar

- Both local-player HUD and world champion bars render a white-blue segment at approximately 60% alpha.
- With missing HP, the segment begins at the current HP edge and extends right.
- At full HP, the segment overlays the rightmost part of the HP fill, so the shield remains visible instead of being clipped outside the bar.
- Shield overflow is clamped to the bar width; zero shield and invalid maximum HP render nothing.
- The RHI path owns the normal draw. ImGui draws the same segment only as a fallback, preventing the previous double-alpha overlay risk.
- Existing HP damage-trail calculation remains HP-only, so losing shield does not create a false red HP trail.

### White shield WFX

- `Data/LoL/FX/Champions/Yasuo/passive_shield.wfx`
- `Data/LoL/FX/Champions/Riven/Riven_E_Shield.wfx`
- `Data/LoL/FX/Champions/LeeSin/w1_cast.wfx`

All main shell/rim/glow emitters use a three-second lifetime and equal RGB channels. Referenced shell/rim/glow textures are grayscale, so a white tint produces a genuinely white shield rather than preserving a colored source texture. Riven and Yasuo follow the owning champion; Lee Sin W1 remains attached to Lee Sin, matching the existing successful visual route.

## Verification

- Shared boundary pre-build gate: PASS.
- Definition pack check: PASS, hash `0xC35A14A7`, 17 champions / 85 skills.
- WFX JSON parse, main-emitter 3.0-second lifetime, equal white RGB, and referenced asset existence: PASS for all three files.
- Engine public header to EngineSDK header sync: PASS.
- `Shared/GameSim/Include/GameSim.vcxproj`, Debug x64: PASS.
- `Tools/SimLab/SimLab.vcxproj`, Debug x64: PASS.
- `Tools/Bin/Debug/SimLab.exe --shield-only`: PASS.
  - Riven 70, Lee Sin 80, Yasuo 100, duration 90 ticks.
  - Covers invalid numeric grants, Lee Sin W pre-cost target rejection, partial absorption, overflow, depletion, exact expiry, replacement/refresh, keyframe restore, zero-damage Yasuo guard, and one 3000 ms passive cue.
- Full `Tools/Bin/Debug/SimLab.exe` 1,800-tick 5v5 suite: PASS.
  - Same-seed hash: `85A270CA375932B7`.
  - Seed+1 hash: `1C930208430B1685`.
- `Server/Include/Server.vcxproj`, Debug x64: PASS.
- `Client/Include/Client.vcxproj`, Debug x64: PASS.
- Scoped `git diff --check`: PASS; only repository line-ending conversion warnings were printed.
- Remaining compiler diagnostics are pre-existing C4275 DLL-interface warnings in Engine ECS headers; this change adds no build error.

## Explicit Boundaries

- Yasuo Flow currently starts full and is set to zero on activation, but this codebase still has no server movement-distance recharge path. Therefore the shield can activate once per spawn/runtime reset. A correct repeating passive needs a separately authored deterministic Flow recharge rule rather than an arbitrary timer hidden in the shield system.
- Gameplay amount and the white health-bar segment clear immediately when the shield is depleted. The attached WFX is still a one-shot three-second cue, so it does not stop early on depletion.
- For the same reason, reconnect/late join or Chronobreak restore during an active shield restores the snapshot amount and white health-bar segment but does not recreate the already-consumed activation WFX. Solving both lifecycle ends cleanly requires replicated remaining expiry/source state or a generic shield visual tracker with start/stop/rebuild behavior.
- Riven and Lee Sin visuals currently take their three seconds from WFX authoring rather than the server duration parameter. They are synchronized for this requirement, but changing the server duration later must update the visual lifecycle or add duration transport.
- Lee Sin W1 preserves the existing targeted behavior: it requires a different allied target and shields Lee Sin. Self-cast Safeguard is not introduced by this pass.
- Only one common timed shield is active per champion. Multi-source stacking/priority rules are intentionally not invented in this first shared implementation.

## In-game Verification Checklist

1. Riven E at full HP: one white sphere follows Riven and a translucent white HP segment shows 70 effective HP for three seconds.
2. Damage Riven for less than 70: HP stays unchanged while only the white segment shrinks. Deal damage above the remainder: only overflow reduces HP.
3. Lee Sin W1 an allied champion/minion/ward: Lee Sin receives an 80-point white segment and attached white sphere while dashing. Attempt W1 on an enemy: no mana/cooldown/accepted cue should be committed.
4. Yasuo at full Flow: a positive hit creates one 100-point passive shield, consumes Flow, absorbs the same hit, and emits one white sphere. A zero-damage request must not activate it.
5. Let each shield expire without damage: amount, health-bar segment, and WFX should be gone at three seconds.
6. Fully deplete a shield early: the health-bar segment must disappear immediately; the sphere currently finishes its fixed three-second cue as documented above.
7. Observe the same shield from a second client: the remaining white health-bar amount must match the server owner after each hit.
8. Move and despawn each owner while the effect is active: attached layers must follow movement and clean up when the anchor becomes invalid.
