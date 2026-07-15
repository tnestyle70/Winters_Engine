# Session - Ezreal W / startup cursor / item shop regression stop report

Date: 2026-07-13  
Stop condition: runtime code work and builds are stopped at the user's request. Resume only after the user's in-game verification.

## 1. Outcome summary

| Area | Current state | Automated evidence | Remaining manual gate |
|---|---|---|---|
| Ezreal W movement freeze | Server-authoritative W cast, pending launch, short action lock, queued move release are covered by a deterministic probe. A stale local skill-animation runtime is cleared when the one-shot animation finishes or its local action lock transitions out. | SimLab `--ezreal-only` passed. The W probe observes cast tick 500, lock/launch tick 508, a move queued at tick 501, projectile launch, queued move release, and positive X movement. The Debug client executable was refreshed after the client changes. | Reproduce with the real server and client. The original interactive hang was not captured in a debugger, so a remaining network/client-FX hang must not be declared impossible until manual play passes. |
| Cursor changes only after InGame | Scene-owned loading cursor toggles were removed. OS cursor lifetime belongs to the window; the LoL texture cursor is rendered globally by the engine every DX11 frame. | `SetLoadingCursorMode` has zero references. Engine Debug and Client Debug builds passed in the cursor lane. | Verify from the first visible app frame through Login/Menu/BanPick/Loading/InGame, then confirm the Windows cursor is restored after exit. |
| Shop contains non-Summoner's-Rift assets | The recursive 454-PNG catalog and runtime editor/empty JSON fallback were removed. The shop now registers only the 15 IDs backed by authoritative `ItemDef`, with explicit item-atlas sprite keys. | Validator passed: 15 entries, 15 unique IDs, 15 authoritative definitions, 15 icons, 15 atlas sprites, and both reference images found. Client Debug passed in the shop lane. | Verify layout, selection, right-click purchase, price, inventory reflection, and reopen behavior in game. The current result is a functional 15-item legacy subset, not every rasterized item visible in `상점1.png`/`상점2.png`. |

## 2. Ezreal W evidence and fix boundary

### Authoritative server path

The intended path remains:

```text
Client W input
-> GameCommand
-> Server/GameSim accepts W
-> QueueUntilUnlock action state (8-tick cap)
-> pending cast launch at tick 508
-> Essence Flux projectile
-> queued right-click move released on unlock
-> snapshot/event presentation
```

The focused SimLab case in `Tools/SimLab/main.cpp` verifies:

- W is accepted at tick 500.
- `ActionStateComponent.movePolicy` is `QueueUntilUnlock`.
- `lockEndTick == EzrealPendingCastComponent.uLaunchTick == 508`.
- A Move command issued at tick 501 is retained.
- At tick 508 the pending cast is removed, the W projectile exists, the queued move is consumed, and the champion position advances.

This rules out a deterministic GameSim infinite pending-cast or permanent server action-lock in the tested path.

### Client legacy/offline stale runtime

The Ezreal W visual definition expects recovery frame 10, while the inspected `ezreal_spell2.wanim` one-shot ends earlier. The old local path could replace the skill animation with idle/run while leaving `m_ActiveSkill` alive, so later local input still saw a stale action runtime.

The local presentation path now clears the active runtime when either:

- the configured recovery frame is reached; or
- the one-shot animator reports that playback has ended.

It also clears an already-fired skill runtime when the local action lock transitions to unlocked before switching to an idle/run/end-transition animation. Debug builds emit bounded `[SkillRuntime]` traces for the early-animation-end and lock-transition cases.

### What is not proven yet

The user's original interactive freeze was not captured with a debugger or profiler stack. The deterministic server probe passed and the stale local runtime was repaired, but the following still require real play:

- server-connected W cast followed by repeated right-click input;
- first-use WFX/model load behavior;
- W miss, W hit, W mark then Q/basic attack detonation;
- network snapshots continuing to advance after the W event.

If the game still becomes unresponsive, the next step is a hang capture/thread stack at the exact frozen frame, not another speculative timing edit.

## 3. Cursor ownership after the rollback

Cursor ownership is now one continuous lifetime:

```text
CWin32Window::Create
-> hide the OS cursor once

CEngineApp DX11 render loop
-> UI_Render_Cursor every frame, independent of scene

CWin32Window::Destroy
-> restore the OS cursor once
```

Loading and MatchLoading no longer own cursor visibility. This prevents scene entry/exit from swapping the LoL cursor back to the Windows arrow. Window cursor visibility is idempotent, and shutdown restores the host cursor.

## 4. Item shop rollback

### Removed regression path

- recursive registration of every PNG under `Resource/Texture/UI/Items`;
- non-Summoner's-Rift/legacy-mode assets entering the live shop simply because a file exists;
- the empty runtime `ItemShopCatalog.json` override/fallback;
- the ImGui item-catalog editor call that participated in this duplicate ownership;
- the atlas/catalog script acting as a second runtime catalog writer.

`Tools/generate_itemshop_catalog_from_reference.py` is now validation-only. It checks the C++ catalog, `ItemDef`, icon files, atlas sprites, Lua layout contract, and the two reference-image dimensions without writing runtime files.

### Current authoritative catalog

The live catalog is deliberately limited to these 15 functional items:

```text
1055 Doran's Blade          450
1056 Doran's Ring           400
1036 Long Sword             350
1042 Dagger                 250
1028 Ruby Crystal           400
1029 Cloth Armor            300
1033 Null-Magic Mantle      400
1001 Boots                  300
1052 Amplifying Tome        400
1037 Pickaxe                875
1043 Recurve Bow            700
1053 Vampiric Scepter       900
1058 Needlessly Large Rod  1200
1038 B. F. Sword           1300
3153 Blade of the Ruined King 3000
```

Names and prices come from `CItemRegistry`/`ItemDef`; the UI does not invent a second price table. Each entry has an explicit `item:<assetKey>` sprite in `itemshop_atlas_manifest.json` and a direct icon-path fallback.

The Lua catalog consumes `UI.GetShopItems()` and lays the 15 entries out in the legacy two-row reference coordinate system. It no longer hardcodes an independent list.

### Deliberate limitation

`상점1.png` and `상점2.png` visually contain substantially more items than the 15 authoritative definitions. Those additional rasterized icons were not promoted to purchasable items because their gameplay stats, prices, recipes, and IDs are not all authoritative in the current GameSim.

Therefore the current state should be described as:

> Reference-styled atlas layout with the 15 server-supported Summoner's Rift items, not full pixel/content parity with every item visible in the two screenshots.

Full reference parity is a separate data task: identify every reference icon, add verified `ItemDef`/recipe behavior, then expose it through the same single catalog. Adding presentation-only fake purchases would recreate the ownership bug.

## 5. Verification performed before stopping

```text
PASS  SimLab --ezreal-only focused projectile/action-lock probe
PASS  Engine Debug build in cursor lane
PASS  Client Debug build in cursor lane
PASS  Client Debug build in shop lane
PASS  final shop validator (15/15/15/15, two reference images)
PASS  focused git diff --check (line-ending warnings only)
PASS  SetLoadingCursorMode residual-reference count = 0
INFO  Client/Bin/Debug/WintersGame.exe refreshed at 2026-07-13 19:45:47
PASS  no WintersGame/WintersServer/SimLab/MSBuild compiler processes left running
```

No final interactive GUI session was launched after these changes because the user chose to stop and perform the in-game verification personally.

## 6. Manual in-game checklist

1. Start the server, then the normal client.
2. Confirm the LoL cursor appears on the first app screen, remains through menu/loading/InGame, and the Windows cursor returns after exit.
3. Select Ezreal, cast W into empty space, and right-click during the cast and immediately after it. Repeat at least ten times.
4. Repeat W against a champion/structure, then trigger the mark with Q and basic attack.
5. Confirm game time, animation, network movement, and other units continue updating after W.
6. Open the shop at the valid purchase location. Confirm only the 15-item curated set is interactive and no unrelated resource-folder item appears.
7. Verify atlas icons, displayed prices, left-click selection, right-click purchase, gold deduction, inventory update, close, and reopen.

If W fails again, record whether the window is truly unresponsive or only movement is rejected, and capture the last Debug Output lines containing `[NetworkMoveLock]` or `[SkillRuntime]`.

## 7. Resume gate

Do not resume RHI/performance/AI feature work until the three manual checks above pass. On resume:

- W failure -> capture hang/movement-lock evidence first;
- cursor failure -> identify the first scene/frame where texture cursor ownership is lost;
- shop mismatch -> decide explicitly between the current 15-item functional subset and a full verified ItemDef expansion matching every reference icon.

