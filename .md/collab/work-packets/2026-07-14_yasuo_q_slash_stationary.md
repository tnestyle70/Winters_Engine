# Work Packet: Yasuo Q Slash Stationary Visual

## Metadata

- ID: `2026-07-14_yasuo_q_slash_stationary`
- Status: `Handoff`
- Agent: `Codex`
- Owner: Desktop
- Branch: `main` (shared dirty working tree, no commit created)
- Base: current shared working tree after the Yasuo image-only FX handoff
- Started: `2026-07-14`

## Objective

Keep `Yasuo.Q.Slash` at its cast origin while preserving its runtime cast-direction yaw. Only `Yasuo.Q.Tornado` should receive forward travel velocity.

## Owned Paths

- `Client/Private/GameObject/Champion/Yasuo/YasuoFxPresets.cpp` (`SpawnQStraight` cue velocity override only)
- `Client/Public/GameObject/Champion/Yasuo/YasuoFxPresets.h` (`SpawnQStraight` stationary signature only)
- `Client/Private/GameObject/Champion/Yasuo/Yasuo_Skills.cpp` (`SpawnQStraight` call arguments only)
- `.md/collab/work-packets/2026-07-14_yasuo_q_slash_stationary.md`
- `.md/collab/ACTIVE_WORK_PACKETS.md` (this row only)

## Read-Only / Excluded Paths

- `Data/LoL/FX/Champions/Yasuo/q_slash.wfx`
- `Data/LoL/FX/Champions/Yasuo/e_dash_trail.wfx`
- `Client/Bin/**`, `Server/Bin/**`, and all running Winters processes
- `Engine/**`, `Shared/**`, `Server/**`

## Validation

- Confirm `SpawnQStraight` still supplies `vForward`, has no speed parameter, and no longer overrides velocity.
- Confirm `SpawnQTornado` retains its velocity override.
- Run a scoped Debug x64 syntax/type gate only if no compiler/linker job is active.
- Preserve the currently running Client and Server; defer link/restart to the user's validation cycle.

## Handoff Notes

- WFX yaw is in radians. A reversed non-billboard image emitter needs an authored yaw offset of PI (`3.141593`).
- MeshParticle direction correction uses Transform `rotation.y`, not the WFX `yaw` field.
- `SpawnQStraight` no longer accepts visual speed or overrides cue velocity; `q_slash.wfx` therefore uses its authored zero velocity at the cast origin.
- `SpawnQTornado` still overrides velocity from `qTornadoSpeed`, so Q3 remains the only traveling Q visual.
- Both changed Client translation units passed the Debug x64 syntax/type gate.
- The running Client PID 50848 and Server PID 5632 were not stopped or overwritten. A fresh build/restart is required before this code change appears in-game.
