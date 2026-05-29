# Winters Client Shell Context Checkpoint

Last updated: 2026-05-08

This file is the durable handoff point for context compaction or a new session.
There is no programmable hook into Codex context compaction, so keep this file
current and ask the next session to read it if the conversation summary becomes
too small.

## Current Goal

Build this flow without mixing product-specific gameplay into the shared client:

```text
GameSelect -> Login -> MainMenu(ClientShell) -> Lobby/Matchmaking -> GameMode -> LOL InGame
```

## Completed

- GameModule layer:
  - `IGameModule`
  - `CGameModuleRegistry`
  - `CLOLGameModule`
  - placeholder modules for EldenRing and ClassServant
- `Scene_GameSelect`
- `CClientShellSession`
- `Scene_Login`
- `Scene_MainMenu` panel layout
- `CClientShellDataStore` offline data seed for Store/Profile/Friends
- `CClientShellBackendService`
  - reads `CClientShellSession::GetAccessToken()`
  - configures `CProfileClient` and `CShopClient`
  - hydrates `CClientShellDataStore`
  - routes store purchase through backend when online and offline data when offline
- Lobby/Matchmaking shell state
  - Home `Play` moves to Lobby instead of entering BanPick directly
  - Lobby `Find Match` uses `CMatchClient` when online and offline fallback when offline
  - `Enter Match` is the only current path to launch the selected product
- `gameMode.json` + `CGameModeCatalog`
  - `Client/Bin/Data/GameModes/gameMode.json`
  - modes: `summoners_rift`, `practice_tool`, `aram`
  - selected mode flows into `GameLaunchConfig::strGameModeID`
- `IGameMode` runtime shell
  - `Client/Public/GameMode/IGameMode.h`
  - `Client/Public/GameMode/LOL/LOLGameModeRuntime.h`
  - `Client/Private/GameMode/LOL/LOLGameModeRuntime.cpp`
  - `CLOLGameModule` now creates a LOL game mode runtime from `GameLaunchConfig::strGameModeID`

## Current Files

- `Client/Public/GameModule/*`
- `Client/Private/GameModule/*`
- `Client/Public/Scene/Scene_GameSelect.h`
- `Client/Private/Scene/Scene_GameSelect.cpp`
- `Client/Public/Scene/Scene_Login.h`
- `Client/Private/Scene/Scene_Login.cpp`
- `Client/Public/Scene/Scene_MainMenu.h`
- `Client/Private/Scene/Scene_MainMenu.cpp`
- `Client/Public/ClientShell/*`
- `Client/Private/ClientShell/*`

## Naming Notes

- In ClientShell data types, keep common acronyms uppercase:
  - `UserID`, `ItemID`
  - `MMR`, `RP`
- Do not normalize these back to `UserId`, `ItemId`, `Mmr`, or `Rp`.

## Build Notes

- Latest verified build:
  - command: `MSBuild Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal`
  - first run after changes failed in 177.99s because `GameModeCatalog.cpp` included `json.hpp` while debug `new` macro was active.
  - fixed by wrapping `json.hpp` include with `#pragma push_macro("new")`, `#undef new`, `#pragma pop_macro("new")`.
  - second run succeeded in 35.15s.
- Any new file that includes `Network/Backend/json.hpp` after `Defines.h` must use the same `new` macro guard.

## Next Steps

1. Move `ChampionCatalog` filtering behind selected `GameModeDef`.
2. Add a `SkillCatalog` facade over the current `CSkillRegistry`/legacy `FindSkillDef`.
3. Start replacing Scene-level skill lookups with `SkillCatalog`.
4. Later: split real mode rules into SummonersRift / PracticeTool / ARAM runtime classes.

## Backend Direction

Offline data is only a local cache/fallback. The intended final shape:

```text
Scene_Login
  -> CAuthClient
  -> CClientShellSession(accessToken)

MainMenu Panels
  -> CClientShellDataStore(cache)
  -> CShopClient / CProfileClient / CMatchClient
```

Backend clients must hydrate/update the shell data store, not bypass it.
