# Yasuo E Dash Trail Runtime Yaw Report

- Date: 2026-07-14
- Status: source/type validation PASS; fresh-client visual A/B PENDING
- Scope: runtime orientation of `Yasuo.E.DashTrail` only

## Direction

The cue player already converts a normalized XZ forward vector into radians with `WintersMath::YawFromDirectionXZ`, internally `atan2(direction.x, direction.z)`, and adds it to both billboard yaw and mesh `rotation.y`. This change therefore supplies the E target direction through the existing cue context instead of adding another yaw owner or modifying WFX data.

## Implemented result

- `ResolveEDashForward` computes the normalized horizontal direction from Yasuo's current Transform to the E target Transform.
- If either Transform or the target is unavailable, the authoritative EffectTrigger command direction is used. If that vector is also empty, Yasuo's current Transform yaw is used.
- The pre-existing fallback check was corrected so a zero vector is not mistaken for +Z and a valid -Z direction is not rejected.
- `SpawnEDashTrail` now receives that forward vector and assigns it to `FxCueContext::vForward`.
- The current FX mesh renderer is also passed into the cue context. The WFX's first `MeshParticle` afterimage is no longer skipped and receives the same runtime yaw as the two non-billboard streak emitters.
- Both the legacy/local E accepted path and the authoritative visual-event path use the same direction resolver.
- `e_dash_trail.wfx` remains unchanged. Its authored `yaw` and `rotation.y` stay at zero, so they remain available as emitter-local asset-axis offsets if visual tuning later finds an FBX-specific 180-degree correction necessary.

## Validation record

- Client Debug x64 direct syntax/type gate using the current project include/define environment and `cl.exe /Zs`:
  - `Yasuo_Skills.cpp`: PASS
  - `YasuoFxPresets.cpp`: PASS
- Direction/yaw probe: PASS.
  - +Z -> `0`
  - +X -> `+1.570796`
  - -Z -> `+3.141593`
  - -X -> `-1.570796`
- `e_dash_trail.wfx` JSON parse: PASS, 3 emitters.
- WFX SHA-256 preserved: `B1FF322A8472C3F32412B3AE6043D858E608D15B7C4790A8EBBAD0381CE248A9`.
- Repository-wide `SpawnEDashTrail` call-site check: declaration, definition, and two E call sites only; no stale signature remains.
- Scoped `git diff --check`: PASS; only the repository's existing LF-to-CRLF notices were printed.
- Live-process preservation: PASS. `WintersGame.exe` PID 57024 and `WintersServer.exe` PID 25288 were neither terminated nor overwritten.

## Deferred visual gate

The running Client predates this source change. After its next Debug rebuild/restart, cast E toward targets in the four cardinal directions and verify that the afterimage FBX, white streak, and black cut all face the dash direction. If only the FBX is exactly reversed while the streaks are correct, tune only that emitter's WFX `rotation.y` by PI; do not add a champion-body yaw offset in code.

Attached anchor offsets are a separate position contract. This packet changes orientation only; if an offset drifts after attachment, handle that independently instead of expanding the yaw fix into generic anchor behavior.
