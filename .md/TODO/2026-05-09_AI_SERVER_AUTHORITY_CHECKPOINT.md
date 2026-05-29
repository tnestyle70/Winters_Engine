# 2026-05-09 AI Server Authority Checkpoint

## Current Focus

- LoL bot AI must be server-authoritative.
- Client should render, send input, apply snapshots/events, and show debug UI only.
- Immediate bugs:
  - Ezreal right-click move command reaches client input but may be dropped on server if `sessionId -> EntityID` binding is missing.
  - Red bot can appear near blue nexus because server/client roster spawn coordinates had a `slot 5~7 -> x=127` special case.

## Code Changes In This Pass

- Added shared spawn definitions:
  - `Shared/GameSim/Definitions/MapSpawnPoints.h`
  - `Shared/GameSim/Definitions/MapSpawnPoints.cpp`
- Server and client now call `GetGameSimRosterSpawnPosition()` for roster champion spawn.
- Server bot lane goal now uses `GetGameSimLaneGatherPosition()` instead of hard-coded `{0, 1, 0}`.
- Server command drain now calls `ResolveControlledEntityForSession()` and recovers from lobby slot `netId` if `m_sessionToEntity` is stale/missing.
- Client in-game Hello binding now trusts server Hello `myNetId` first, then falls back to copied roster context.
- `CGameSessionClient::OnHello()` refreshes local slot identity from roster data when possible.
- Temporary command logs added:
  - Client: first 32 `SendMove` commands with session/net/pos.
  - Server: first drops for missing controlled entity and first 32 accepted move commands.
- Bot structure scan now requires `TargetableTag` before attacking structures.

## Verify Next

1. Start server + client through BanPick/roster flow.
2. Select Ezreal as local human champion.
3. Right-click ground and check logs:
   - Client should show `[Command] client send move ...`.
   - Server should show `[Room] cmd batch ...`, then `[Command] move ...`.
   - If server shows `[Command] drop ... no-controlled-entity`, inspect lobby slot `sessionId/netId`.
4. Confirm red bot slots no longer spawn at `x=127`.
5. Confirm red bot first moves toward red-side lane gather point near `x=-5`, not blue nexus.

## Follow-Up Design

- Keep high-level AI in `Shared/GameSim`.
- LoL executes AI on server only.
- Elden single-player can execute AI locally, while co-op/PvP/server modes execute AI on host/server.
- Next AI design layer: lane assignment, target priority policy, attack/move command arbitration, and debug ImGui panels for current state/target/utility score.
