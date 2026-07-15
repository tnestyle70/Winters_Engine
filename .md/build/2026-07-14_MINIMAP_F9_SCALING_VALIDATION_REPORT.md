# F9 Minimap Scaling Validation Report

- Date: 2026-07-14
- Scope: F9 minimap projection scaling only
- Result: source/type validation PASS, fresh-client runtime A/B PENDING

## User-observed failure

The captured F9 panel changed `Top Lane Scale/Weight` and `Bottom Lane Scale/Weight`, but champion and map positions did not move as expected. This was not a stale executable: the running Debug client had been built immediately before launch.

The controls were connected to `ResolveChampionLaneScale -> ResolveIconRadius`, so they changed only champion portrait radius. They never reached `WorldToMinimap`, `ProjectWorldToMinimapUv`, click inversion, camera bounds, or `CVisionSystem::FowProjection`. Near the two base corners, the top and bottom UV bands also overlapped and `min(topFactor, bottomFactor)` let the smaller value mask the other slider. The labels therefore described coordinate scaling while the implementation only warped portrait size.

## Applied direction

- Removed the misleading top/bottom portrait-radius path.
- Kept the corrected S020 orthogonal, uniform basis centered at `(104.5, 0)` with canonical extent `94.385`.
- Added one runtime `World Extent` value, clamped to `70..160`, that rebuilds all three projection anchors together.
- `CMinimapPanel` owns the Client runtime projection used by minimap rendering, click inversion, and camera bounds.
- `CScene_InGame` mirrors an accepted edit into `CVisionSystem::FowProjection`; UI code does not own or mutate the Engine system directly.
- When anchors change, the Engine clears projection-space explored texels before forcing the next FoW rebuild. Old explored pixels cannot be reinterpreted in the new basis.

## Completed validation

- Scoped `git diff --check`: PASS; only Git's existing LF→CRLF notices were printed.
- Client direct syntax/type gate, `MinimapPanel.cpp` + `Scene_InGameImGui.cpp`, Debug x64 tlog defines/includes with `cl.exe /Zs`: PASS (`exit 0`).
- Engine direct syntax/type gate, `VisionSystem.cpp`, Debug x64 tlog defines/includes with `cl.exe /Zs`: PASS (`exit 0`).
- Deterministic projection probe: PASS.
  - extent 70: equal basis length `98.994949`, dot `0`, center UV `(0.5, 0.5)`, round-trip error `0`.
  - extent 94.385: equal basis length `133.480547`, dot approximately `1.8e-12`, round-trip error approximately `4.28e-14`.
  - extent 160: equal basis length `226.274170`, dot `0`, round-trip error `0`.
  - For the same off-center world point, UV distance from center decreased `0.502062 -> 0.372351 -> 0.219652` as extent increased, confirming smaller extent moves content outward and larger extent inward.
- Review of scheduler/render order: PASS. The F9 edit happens after the current scene render; the next frame's Vision worker finishes before texture upload and render, so CPU FoW clear plus forced rebuild is sufficient.
- Live-process preservation: PASS. `WintersGame.exe` PID 57024 and `WintersServer.exe` PID 25288 were neither terminated nor overwritten during validation.

## Deferred runtime gate

The normal Client Debug link and fresh-client A/B capture are intentionally pending because the user is testing with the current executable. Source changes cannot hot-reload into that process. Once that client is closed, rebuild Client Debug, reconnect to the still-running server, and compare extent `70 / 94.385 / 160` in one unchanged scene. Final runtime PASS requires icons, click mapping, camera box, and FoW boundary to move together and reset to recover `94.385`.

## Adjacent findings outside this packet

- `SetFowLocalTeam` and `ClearFowLocalTeam` do not clear explored history. Red↔Blue Take Control can retain the previous team's explored texels. This should be a separate small patch before Take Control validation.
- A failed GPU texture map currently cannot preserve the dirty flag because the upload API returns no success value.
- The first InGame render can occur before the first Vision update after a Loading-scene transition, so an initialized-zero GPU FoW texture would remove a possible one-frame artifact.

These findings did not block the projection fix and were not folded into its owned slice.
