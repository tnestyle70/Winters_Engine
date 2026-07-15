# Work Packet: Minimap F9 Projection Sync

## Metadata

- ID: `2026-07-14_minimap_f9_projection_sync`
- Status: `Handoff`
- Agent: `Codex`
- Owner: Desktop
- Branch: `main` (shared dirty working tree, no commit created)
- Base: current shared working tree after S020 projection correction and concurrent champion portrait prewarm handoff
- Started: `2026-07-14`

## Objective

Record the user-observed F9 minimap scaling failure, remove the misleading lane portrait-radius controls, and promote the S020 orthogonal/uniform three-point projection to one live runtime source shared by minimap render, click inversion, camera bounds, and Fog of War.

## Owned Paths

- `Client/Private/UI/MinimapPanel.cpp`
- `Client/Public/UI/MinimapPanel.h`
- `Client/Private/Scene/Scene_InGameImGui.cpp` (`DrawTunerImGui` result bridge only)
- `Engine/Private/ECS/Systems/VisionSystem.cpp` (`SetFowProjection` history invalidation only)
- `.md/plan/2026-07-14_MINIMAP_F9_SCALING_FAILURE_AND_FIX_PLAN.md`
- `.md/build/2026-07-14_MINIMAP_F9_SCALING_VALIDATION_REPORT.md`
- `.md/collab/work-packets/2026-07-14_minimap_f9_projection_sync.md`
- `.md/collab/ACTIVE_WORK_PACKETS.md` (this row only)

## Read-Only / Excluded Paths

- `Client/Private/Scene/Scene_InGameInput.cpp`
- `Client/Private/Scene/Scene_InGameLifecycle.cpp`
- `Client/Private/Scene/Scene_InGameRender.cpp`
- `Client/Public/Scene/Scene_InGame.h`
- `Engine/**` except the owned private `VisionSystem.cpp` block
- `Engine/Public/**`, `EngineSDK/**`
- `Client/Include/Client.vcxproj`, `Client/Include/Client.vcxproj.filters`
- `Client/Bin/**`, `Server/Bin/**`, and all running Winters processes
- Existing S006/S007/S020 plan and result documents

## Validation

- Preserve concurrent portrait-prewarm changes observed at `2026-07-14 20:44:26`.
- Validate S020 projection invariants and UV/world round trips with a deterministic math probe.
- Verify a changed projection clears projection-space FoW exploration history before rebuild.
- Run scoped `git diff --check`.
- While `WintersGame.exe` is running, use Client `ClCompile` only with isolated `IntDir`, disabled project references, and disabled pre/post-build events.
- Defer normal Debug link/output replacement and fresh-client visual verification until the user closes the current client.

## Handoff Notes

- The running server is outside this client-only slice and must not be restarted.
- The observed failure is not a dead ImGui slider: the old controls changed portrait radius only and did not change minimap coordinates or projection.
- UI returns a projection edit result; the Client scene bridge owns mutation of `CVisionSystem`.
- No stage, commit, pull, rebase, reset, or process termination is authorized.
- Source/type and projection-math gates pass. Normal Client Debug link and same-scene runtime A/B remain pending until the current client is closed.
