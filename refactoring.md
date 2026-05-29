# Winters Refactoring Notes

This document tracks refactoring decisions while the engine/client/server/shared layout is being made understandable. Keep entries short and actionable; move detailed plans to separate docs when they grow.

## Current Priority

Make the client flow readable before adding more backend or gameplay features.

1. Clarify what each scene owns.
2. Clarify what the ClientShell layer owns.
3. Keep offline flow working while backend integration is incomplete.
4. Do not delete shell/backend code until references and replacement ownership are clear.

## Workspace And Filter Map

Current root solution map:

- `00. Docs`: markdown decisions and planning docs.
- `01. Client`: `Client.vcxproj`; owns input, scenes, shell, UI, rendering connection, prediction/presentation.
- `02. Engine`: `Engine.vcxproj`; owns renderer, ECS, resource loading, platform, input, sound, core engine services.
- `03. Server`: `Server.vcxproj`; owns room/session authority, command intake, server world, snapshots, replay/minion/AI entry points.
- `04. Shared`: solution-item map for shared protocol/GameSim/replay files; not a standalone build project yet.
- `05. Services`: Go backend solution items; auth/shop/profile/matchmaking/payment and shared backend packages.
- `06. Data`: game data solution items, including `Data/LoL/FX/*.wfx`.
- `07. Shaders`: root shader solution items; Client copies root `Shaders` to output after build.
- `08. Tools`: tool projects, currently `WintersAssetConverter`.

Current project filter roles:

- `Client.vcxproj.filters`: Client-local code plus directly compiled Shared/GameSim slices. It should not become the owner of global `Shaders`, `Data`, or `Services` browsing.
- `Engine.vcxproj.filters`: Engine runtime implementation only. Keep gameplay rules, backend services, and client scene policy out of Engine.
- `Server.vcxproj.filters`: Server authority code plus directly compiled Shared/GameSim slices.
- `WintersAssetConverter.vcxproj.filters`: asset conversion tool code plus the engine resource/asset format headers it edits against.

Build membership rules:

- `.sln` solution folders and `SolutionItems` are for visibility only.
- `.vcxproj.filters` controls Visual Studio grouping only.
- `.vcxproj` controls compile/link membership.
- Viewing-only files should stay as solution items or `None`; compile membership should change only when a build target truly owns the file.
- `EngineSDK` is deployment output from `UpdateLib.bat`; edit Engine public headers, not copied SDK headers.

Current dependency facts:

- `Engine` produces `WintersEngine`; its post-build runs `UpdateLib.bat`.
- `Client` links `WintersEngine.lib`, depends on Engine in the solution, runs `UpdateLib.bat` before build, runs `Shared/Schemas/run_codegen.bat`, compiles Shared/GameSim sources directly, and copies root `Shaders` into its output.
- `Server` has a project reference to Engine, runs `Shared/Schemas/run_codegen.bat`, and compiles the same Shared/GameSim sources directly.
- `Services` consumes generated Go schema files but is not part of the Visual Studio C++ build.
- `CMakeLists.txt` currently builds the Engine target and adds `WintersWorkspaceMap` for full-repo browsing; it is not yet the full Client/Server replacement.

Target dependency shape:

```text
Engine
  -> rendering, ECS, resources, platform, input, sound

Shared/GameSim
  -> deterministic gameplay rules used by Client and Server

Shared/Schemas
  -> FlatBuffers protocol contract for Client, Server, and Services

Client
  -> Engine + Shared/GameSim + Shared/Schemas + Shaders + Data

Server
  -> Engine support + Shared/GameSim + Shared/Schemas

Services
  -> Shared/Schemas generated Go + backend packages

Tools
  -> Engine asset/resource formats + third-party import libraries
```

Do not try to make every folder a build project at once. The clean future is probably a separate `SharedGameSim` static library, but the current safe path is to keep the existing `.vcxproj` build working while ownership is clarified.

## Dependency Cleanup Sessions

Session 0 - Workspace membership audit:

- Compare root folders, `Winters.sln` solution items, and project filters.
- Classify each file as compile-owned, header-owned, solution-item only, generated, or runtime data.
- Do not refactor code in this session.

Session 1 - Build graph baseline:

- Record which projects build which targets and which generated steps run.
- Verify `Engine`, `Client`, `Server`, and `WintersAssetConverter` Debug x64 builds independently.
- Keep a short list of build-only surprises such as missing project membership or dead link references.

Session 2 - Engine boundary:

- Check Engine for accidental Client/Server/GameSim policy ownership.
- Keep Engine as rendering/ECS/resource/platform infrastructure.
- Any Engine public header change requires `UpdateLib.bat` or a build that triggers it.

Session 3 - Shared/GameSim boundary:

- Separate deterministic gameplay data/rules from client visual data.
- Keep server-authoritative flow: Client Input -> GameCommand -> Server GameSim -> Snapshot/Event -> Client Visual.
- First major candidate is splitting client visual fields out of Shared `ChampionDef`.

Session 4 - Client flow:

- Finish `GameApp`, `Scene_Login`, `Scene_MainMenu`, `Scene_CustomMode`, `Scene_Loading`, and `Scene_InGame` ownership cleanup.
- Treat legacy `Scene_GameSelect` as disk-only until either removed or deliberately restored to the project.
- Keep backend login offline-first until the auth/profile user-id contract is real.

Session 5 - Champion registration:

- Migrate legacy `ChampionTable.cpp` entries into each champion registration file one at a time.
- After the table is empty, remove `RegisterAllLegacy()`.
- Zed should be late because `ZedFxPresets.cpp` still directly asks `FindChampionDef(eChampion::ZED)`.

Session 6 - Server authority:

- Untangle `GameRoom`, server world setup, command dispatch, snapshots, replay, minions, and AI entry points.
- Server owns gameplay truth; client visual hooks stay presentation-only.

Session 7 - Protocol and schemas:

- Treat `.fbs` files as the source contract.
- Generated C++ and generated Go should be browsable but not manually edited.
- Keep `run_codegen.bat` behavior visible before changing schema paths.

Session 8 - Runtime data, FX, and shaders:

- Keep WFX under root `Data/LoL/FX`.
- Keep HLSL/HLSLI under root `Shaders`.
- Runtime resource resolution should stay rooted at `Client/Bin/Resource`; do not rely on per-config copied Resource folders.

Session 9 - Services backend:

- Keep Go backend under root `Services`.
- Fix client/backend contract mismatches before treating online flow as production-ready.
- Auth must return or expose a backend user id usable by profile/inventory APIs.

Session 10 - CMake convergence:

- Use `WintersWorkspaceMap` as the browsing model.
- Only after the current `.vcxproj` ownership is clear, add CMake targets for Client, Server, Shared/GameSim, and Tools.
- Generated Visual Studio projects should preserve the same root mental model as `Winters.sln`.

## Client Shell Roles

`CClientShellSession`

- Owns current launcher/session state.
- Stores selected product and `GameLaunchConfig`.
- Stores account state: offline account, authenticated account, user id, display name, access token.
- Used by `GameApp`, `Scene_Login`, and `Scene_MainMenu`; legacy `Scene_GameSelect` exists on disk but is not part of the current flow.

`CClientShellDataStore`

- Owns UI-facing shell data for MainMenu.
- Stores profile summary, store items, friends, and lobby/matchmaking state.
- Provides offline seeded data so the shell can run without backend services.
- Should not own real GameSim rules or in-match state.

`CClientShellBackendService`

- Owns shell-level backend synchronization.
- Wraps Profile, Shop, and Matchmaking clients.
- Pushes backend results into `CClientShellDataStore`.
- Falls back to offline behavior when no authenticated backend session exists.

## Scene_Login Notes

Current state:

- `Scene_Login` creates `CAuthClient`, but online login has no visible UI path.
- The login arrow currently launches offline login.
- `RequestOnlineLogin()` exists but is effectively unused.
- `RequestGameSelect()` / `ChangeToGameSelect()` were removed; current flow is `GameApp` default LOL selection -> `Login` -> `MainMenu`.
- Online success stores email as user id, but backend profile APIs expect UUID user ids.

Decision:

- Treat `Scene_Login` as offline-first until backend login is wired intentionally.
- Later backend login needs a real UI path, user id extraction from auth result/JWT, and clear error handling.

## Scene_MainMenu Notes

Current state:

- `OnEnter()` seeds shell data, loads game modes, configures backend service, then requests initial sync.
- `LaunchSelectedProduct()` activates the selected game module and opens its initial scene.
- MainMenu currently depends on `CClientShellSession`, `CClientShellDataStore`, and `CClientShellBackendService`.

Cleanup:

- Removed the `--banpick-smoke` auto-play block from MainMenu `OnEnter()`.
- MainMenu should not silently skip itself into gameplay during normal scene cleanup.
- Updated private includes to use the renamed `GameApp.h` header.
- Removed unused ImGui panel helpers from MainMenu: navigation, home, lobby, store, profile, friends, and settings.
- Current MainMenu interaction is image-driven: click the gameplay/start source rect, then launch the selected product.

`LaunchSelectedProduct()` ownership:

- Reads selected product and base launch config from `CClientShellSession`.
- Reads selected game mode from `CClientShellDataStore`.
- Activates product through `CGameModuleRegistry`.
- Requests the active module's initial scene and changes scene through `CGameInstance`.

Question to resolve later:

- Should product launch state stay in `CClientShellSession`, or move to a clearer `ClientLaunchContext` name?
- Should MainMenu directly call `CClientShellBackendService`, or should there be a thinner `MainMenuModel`/`ShellModel` boundary?

## Backend Integration Gotchas

- Auth returns tokens but Client currently does not receive/store backend user id directly.
- Profile and inventory paths expect UUID user ids.
- `CMatchClient::LeaveQueue()` uses POST, while the Go matchmaking handler uses DELETE.
- `CHttpClient` async lifetime is risky because async lambdas capture raw `this`.

## Next Refactoring Candidates

1. Rename ClientShell concepts after ownership is agreed.
2. Split `Scene_MainMenu` into scene shell flow and panel/UI rendering.
3. Make backend login either clearly unavailable or fully wired.
4. Fix backend contract mismatches before treating online flow as real.

## InGame/GameSim Query Cleanup

Decision:

- Pure vector math belongs in `WintersMath`.
- ECS/world state reads shared by client and server belong in `GameplayStateQuery`.
- Scene-only picking and presentation helpers can stay in `GameplayQuery` until they are split further.

Cleanup:

- Removed `GameplayQuery::DistanceSqXZ`; use `WintersMath::DistanceSqXZ` directly.
- Moved gameplay radius and team lookup ownership to `Shared/GameSim/Systems/GameplayStateQuery`.
- Updated `GameplayQuery`, `Scene_InGame`, `CommandExecutor`, `AttackChaseSystem`, and `ZedGameSim` to use the shared radius/team query where applicable.

Next:

- Split `Scene_InGame` into smaller bridge/model responsibilities before changing gameplay behavior.
- Treat `GameRoom` cleanup separately because it owns server authority and lobby/gameplay orchestration.

## Minion Movement Radius Notes

Current server movement dependencies:

- `SpatialAgentComponent.radius` is the actor body radius used by avoidance and shared gameplay radius lookup.
- Server minions spawn with `SpatialAgentComponent.radius = 0.5f`.
- `ServerMinionTuning::kMinionLaneClearanceRadius` is the nav-grid clearance used by server minion lane/path movement.

Cleanup:

- Reduced `kMinionLaneClearanceRadius` from `0.75f` to `0.5f` so lane/path movement uses the same clearance as the actual minion body radius.

Next:

- If champions still get trapped by friendly minions, relax friendly-minion blocker handling in `MoveSystem` instead of shrinking shared gameplay radius first.

## Champion Registration Cleanup

Decision:

- `ChampionTable.cpp` is a legacy client visual fallback table, not the final gameplay source of truth.
- Champion-specific visual definitions should live beside each champion registration module.
- Keep legacy fallback entries only for champions that have not been migrated yet.
- Do not remove `GetChampionDisplayName()` until display-name ownership moves to a clearer catalog/helper.

First slice:

- Move Irelia's `ChampionDef` registration into `Irelia_Registration.cpp`.
- Remove Irelia from `ChampionTable.cpp` so the legacy table no longer duplicates migrated champions.

Next:

- Migrate Yasuo, Kalista, Garen, Zed, and Riven one champion at a time.
- After the legacy table becomes empty, remove `RegisterAllLegacy()` and then split client visual fields out of Shared `ChampionDef`.
