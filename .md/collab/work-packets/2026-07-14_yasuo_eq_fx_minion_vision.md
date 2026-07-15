# Work Packet: Yasuo EQ FX and Allied Minion Vision

## Metadata

- ID: `2026-07-14_yasuo_eq_fx_minion_vision`
- Status: `Handoff`
- Agent: `Codex`
- Owner: Desktop
- Branch: `main` (shared dirty working tree, no commit created)
- Base: current shared working tree after the minimap F9 projection handoff and concurrent S031 Yasuo WFX tuning
- Started: `2026-07-14`

## Objective

Buffer one Yasuo Q input during the authoritative E forced-motion lock and release it as the existing stage-4 EQ cast after E finishes; add isolated, WFX-tunable E ring, EQ wind ring, and tornado-hit visuals; and restore allied vision components for server-replicated minions.

## Owned Paths

- `Shared/GameSim/Champions/Yasuo/YasuoGameSim.h`
- `Shared/GameSim/Champions/Yasuo/YasuoGameSim.cpp`
- `Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp` (Yasuo E/Q buffer interception only)
- `Server/Private/Game/GameRoomTick.cpp` (Yasuo tick executor argument only)
- `Client/Private/GameObject/Champion/Yasuo/YasuoFxPresets.cpp`
- `Client/Private/GameObject/Projectile/ProjectileVisualCatalog.cpp`
- `Client/Private/Network/Client/SnapshotApplier.cpp` (network minion vision runtime tags only)
- `Data/LoL/FX/Champions/Yasuo/e_dash_ring.wfx`
- `Data/LoL/FX/Champions/Yasuo/eq_wind_ring.wfx`
- `Data/LoL/FX/Champions/Yasuo/q_tornado_hit.wfx`
- `.md/plan/2026-07-14_YASUO_EQ_FX_AND_ALLIED_MINION_VISION_PLAN.md`
- `.md/build/2026-07-14_YASUO_EQ_FX_AND_ALLIED_MINION_VISION_REPORT.md`
- `.md/collab/work-packets/2026-07-14_yasuo_eq_fx_minion_vision.md`
- `.md/collab/ACTIVE_WORK_PACKETS.md` (this row only)

## Read-Only / Excluded Paths

- `Data/LoL/FX/Champions/Yasuo/e_dash_trail.wfx` (actively rewritten by another WFX session)
- `Data/LoL/FX/Champions/Yasuo/eq_ring.wfx`
- `Data/LoL/FX/Champions/Yasuo/eq_inner_wind.wfx`
- `Client/Private/Network/Client/EventApplier.cpp`
- `Client/Private/GameObject/Champion/Yasuo/Yasuo_Skills.cpp`
- `Tools/SimLab/main.cpp` while its concurrent session remains active
- `Client/Include/Client.vcxproj`, `Client/Include/Client.vcxproj.filters`
- `Engine/**`, `EngineSDK/**`
- `Client/Bin/**`, `Server/Bin/**`, and all running Winters processes

## Validation

- Re-hash every dirty owned source immediately before editing and stop on a concurrent timestamp change.
- Validate new WFX as JSON and verify every texture path exists under `Client/Bin/Resource`.
- Run scoped `git diff --check`.
- Run isolated Client, GameSim, and Server compile/build gates without replacing the binaries currently used by the user.
- Runtime gate: E ring follows Yasuo and shrinks; E+Q releases one stage-4 `spell1c` after the E lock; EQ wind RGBA is WFX-tunable; tornado hit billboard attaches once per victim; only allied network minions reveal FoW.

## Handoff Notes

- Do not overwrite or merge the concurrently edited `e_dash_trail.wfx`; the E ring is deliberately a separate cue.
- The existing stage-4 gameplay, animation, and EffectTrigger paths are reused. No client-authored damage or local-only EQ truth is added.
- Network minion vision is repaired before asynchronous visual creation so vision does not depend on model load success or the visual queue budget.
- No stage, commit, pull, rebase, reset, process termination, or running-bin overwrite is authorized.
- All six changed translation units passed isolated Debug x64 syntax/type compilation, all three WFX files passed JSON/path/uniqueness checks, the Shared boundary lint passed, and scoped diff checks passed.
- The current PID 57024 Client and PID 25288 Server predate these changes. Fresh Debug links and the in-game visual/FoW A/B checklist remain for the user's next restart; see the linked build report.
