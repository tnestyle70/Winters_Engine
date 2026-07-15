# Work Packet: Yasuo E Target Reuse Ring

## Metadata

- ID: `2026-07-14_yasuo_e_target_reuse_ring`
- Status: `Handoff`
- Agent: `Codex`
- Owner: Desktop
- Branch: `main` (shared dirty working tree, no commit created)
- Base: current shared working tree after the Yasuo stationary Q slash handoff
- Started: `2026-07-14`

## Objective

Make Yasuo E create a fixed-radius reuse marker on the dashed-through target. The marker must follow that target, erase smoothly around its circumference over the server-authoritative per-target E lockout, and disappear when the lockout expires. Correct the current Yasuo Q/E authored yaw fields against the known-good Yone Q conventions and complete Client/Server Debug x64 build verification.

## Owned Paths

- `Shared/GameSim/Champions/Yasuo/YasuoGameSim.h` (Yasuo E target-lock query only)
- `Shared/GameSim/Champions/Yasuo/YasuoGameSim.cpp` (Yasuo E relation state/create/expiry only)
- `Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp` (Yasuo E champion-rule precheck only)
- `Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json` (`skill.yasuo.e` mark duration only)
- `Data/LoL/ServerPrivate/Gameplay/SkillGameplayDefs.json` (`skill.yasuo.e` mark duration only)
- `Server/Private/Data/Generated/LoLGameplayDefinitions.generated.cpp` (generated Yasuo E mark duration only)
- `Client/Public/GameObject/Champion/Yasuo/YasuoFxPresets.h` (dedicated target-ring declaration only)
- `Client/Public/GameObject/Champion/Yasuo/Yasuo_Tuning.h` (visual target-lock duration mirror only)
- `Client/Private/GameObject/Champion/Yasuo/YasuoFxPresets.cpp` (split dash trail and target-ring cue contexts only)
- `Client/Private/GameObject/Champion/Yasuo/Yasuo_Skills.cpp` (pass accepted E target into target-ring visual only)
- `Client/Private/GameObject/FX/FxCuePlayer.cpp` (anchor-invalid billboard cleanup bridge only)
- `Client/Private/Scene/Scene_InGameLocalSkills.cpp` (Yasuo accepted-hook local duplicate suppression only)
- `Client/Private/GameObject/FX/FxSystem.cpp` (static atlas frame-zero selection only)
- `Engine/Public/FX/FxMaterialDesc.h` (`RadialWipe` style enum only)
- `Engine/Private/Renderer/RHIFxSpriteRenderer.cpp` (DX12 radial-wipe shader branch only)
- `Shaders/FxSprite.hlsl` (DX11 radial-wipe shader branch only)
- `Data/LoL/FX/Champions/Yasuo/e_dash_ring.wfx`
- `Data/LoL/FX/Champions/Yasuo/e_dash_trail.wfx` (direction fields only)
- `Data/LoL/FX/Champions/Yasuo/q_slash.wfx` (direction fields only)
- `.md/build/2026-07-14_YASUO_E_TARGET_REUSE_RING_REPORT.md`
- `.md/collab/work-packets/2026-07-14_yasuo_e_target_reuse_ring.md`
- `.md/collab/ACTIVE_WORK_PACKETS.md` (this row only)

## Read-Only / Excluded Paths

- Yasuo Q3/EQ/R WFX files and their current image-only composition
- snapshot schemas, generated snapshot bindings, and EventApplier snapshot reconciliation
- unrelated gameplay definitions, champion logic, FX emitters, renderer styles, and all existing dirty changes
- runtime texture binaries under `Client/Bin/Resource`

## Validation

- Confirm E is rejected only for a live `(Yasuo source, target)` relation and becomes legal after its 10-second mark duration.
- Confirm the accepted E event still carries the target entity and spawns one target-attached ring while the dash trail remains attached to Yasuo.
- Confirm ring diameter stays constant and the shader uses normalized lifetime for a smooth polar wipe.
- Confirm the 2x2 atlas selects its full-circle first cell when animation FPS is zero.
- Confirm Yasuo Q image planes use the Yone Q quarter-turn baseline and the E MeshParticle uses `rotation.y`, not the ignored `yaw` field.
- Parse all touched JSON/WFX documents, run `git diff --check`, then build Client and Server Debug x64.

## Handoff Notes

- Implemented server-authoritative per-target 10-second lockout, checkpoint-safe relation cleanup, target-attached fixed-radius radial wipe, static atlas frame selection, command-direction yaw correction, and local duplicate suppression.
- Final Debug x64 builds PASS for Client, Server, and SimLab. `SimLab.exe 300 42` PASS with same-seed hash `6CDF24584FDAD776`; `FxSprite.hlsl` VS/PS compile PASS.
- Final visual judgment remains the user in-game checklist in `.md/build/2026-07-14_YASUO_E_TARGET_REUSE_RING_REPORT.md`.
