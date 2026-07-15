# Work Packet: Yasuo E Dash Trail Runtime Yaw

## Metadata

- ID: `2026-07-14_yasuo_e_dash_trail_yaw`
- Status: `Handoff`
- Agent: `Codex`
- Owner: Desktop
- Branch: `main` (shared dirty working tree, no commit created)
- Base: current shared working tree after `2026-07-14_yasuo_eq_fx_minion_vision` handoff
- Started: `2026-07-14`

## Objective

Orient `Yasuo.E.DashTrail` at spawn time from Yasuo toward the E target by passing the runtime horizontal direction through `FxCueContext::vForward`, which the existing cue player converts to a radian yaw.

## Owned Paths

- `Client/Public/GameObject/Champion/Yasuo/YasuoFxPresets.h` (`SpawnEDashTrail` signature only)
- `Client/Private/GameObject/Champion/Yasuo/YasuoFxPresets.cpp` (`SpawnEDashTrail` cue context only)
- `Client/Private/GameObject/Champion/Yasuo/Yasuo_Skills.cpp` (Yasuo E target-direction resolution and two call sites only)
- `.md/build/2026-07-14_YASUO_E_DASH_TRAIL_YAW_REPORT.md`
- `.md/collab/work-packets/2026-07-14_yasuo_e_dash_trail_yaw.md`
- `.md/collab/ACTIVE_WORK_PACKETS.md` (this row only)

## Read-Only / Excluded Paths

- `Data/LoL/FX/Champions/Yasuo/e_dash_trail.wfx`
- `Client/Private/GameObject/FX/FxCuePlayer.cpp`
- `Engine/**`, `EngineSDK/**`, `Shared/**`, `Server/**`
- `Client/Bin/**`, `Server/Bin/**`, and all running Winters processes

## Validation

- Confirm `Yasuo -> E target` XZ normalization and authoritative-command fallback.
- Confirm `CFxCuePlayer` applies `WintersMath::YawFromDirectionXZ` to billboard and mesh emitters in radians.
- Parse the untouched WFX as JSON and preserve its SHA-256.
- Run scoped `git diff --check` and a Debug x64 syntax/type gate for the two changed Client translation units.

## Handoff Notes

- Do not add an FX-specific body-mesh yaw correction. Any asset-axis adjustment remains the WFX emitter's static `yaw`/`rotation.y` offset.
- Do not overwrite the concurrently authored dimensions, colors, or emitter list in `e_dash_trail.wfx`.
- Runtime direction, renderer propagation, WFX parsing/hash preservation, scoped diff checks, and the two changed-TU Client syntax/type gates all passed.
- The current Client PID 57024 predates this change; the four-direction in-game visual A/B remains for the next fresh Debug build.
