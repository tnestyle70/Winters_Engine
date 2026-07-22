Session - Runtime Nav, Jungle, UI, Irelia Regression Recovery Result

Date: 2026-07-19
Status: IMPLEMENTED_AUTOMATED_PASS_MANUAL_VISUAL_PENDING
Plan: `2026-07-19_RUNTIME_NAV_JUNGLE_UI_IRELIA_REGRESSION_RECOVERY_PLAN.md`

## 1. Scope and outcome

- Removed the synchronous full derived-nav/flow-field rebuild from the structure-death tick. The released structure footprint now updates the base, champion-path, and minion-lane walkability locally without reallocating the grids.
- Corrected structure attack-chase arrival geometry, minion combat-to-lane waypoint recovery, stationary retaliation-only Baron behavior, Irelia Q wall transit, Dragon animation presentation, Elder attack FX, following level-up FX, Red buff WFX, Health Bar/Minimap tuner layout and persistence, and minimap champion team outlines.
- Preserved the current generated LoL definition pack. This session did not run the definition generators or overwrite the other session's pack hash `0x8B7942C0`.
- Existing victory-economy work was integrated and reverified: shared minion XP pool split, turret `1500` killer + `1000` to each other ally, team assist ping, AD ratio range up to `10`, and Ashe R stun `2s`.

## 2. Critique gate

- Initial independent critique: nav/structure/minion `P0 1 / P1 4`, UI/Irelia `P0 0 / P1 6`, jungle/animation/FX `P0 0 / P1 4`.
- Accepted findings and dispositions were recorded in the PLAN before source edits. A corrected plan received final read-only critique `P0 0 / P1 0` for all three slices.
- The final user-interview delta was critiqued three more times. Save All footer/message details and Irelia resolver-argument observability were corrected from `P1 3 -> P1 1 -> P1 0`; the delta source-edit gate passed at `P0 0 / P1 0`.
- Implementation-time inspection found two additional issues and the PLAN was updated before their edits:
  - base-only structure carve was incomplete because path resolution prefers the derived path grid;
  - raw POD keyframes serialized implicit padding in `ChampionAssistCreditComponent`.

## 3. Root causes and implemented corrections

### 3-1. Turret destruction freeze

The freeze was caused by doing work proportional to the whole map on the authoritative death tick: rebuild inflated path/lane grids, then rebuild flow fields. It was not necessary to make a single dead structure cell walkable.

The corrected path is:

1. restore base terrain walkability;
2. carve only still-alive structures;
3. patch only the changed structure neighborhood in the existing champion/minion derived grids using their configured clearance radii;
4. invalidate the pathfinder reachability cache, but do not call full `BuildInflated` or rebuild lane flow fields.

The probe kills a real structure by damage, advances the authoritative tick, and verifies base/path/lane cells plus champion/minion path reachability. It also asserts `derived_rebuild_calls=0` and that the second refresh is a no-op.

### 3-2. Kalista/Irelia structure-target stepping

Attack legality measured distance to the raw structure center, while navigation resolved a different walkable endpoint outside the obstacle footprint. The old move arrival radius ignored that center-to-endpoint offset. The actor therefore considered movement complete at the resolved endpoint while still being outside raw attack range; the next tick requested movement again, creating the stationary stepping loop.

`ResolveMoveArriveRadius` now subtracts the raw-target/resolved-endpoint offset and an attack slack from effective range. Structure nav rebuild was never the correct fix for this stepping bug; it belonged only to releasing destroyed structure obstacles.

### 3-3. Minion backward waypoint motion

The minion kept the exact lane waypoint index and a stale combat path while chasing. If combat pulled it past that waypoint, lane resume still targeted the old point behind it. This was a state-transition/progress-invariant bug, not merely a bad A* waypoint.

Combat-to-lane transition now clears stale runtime path data. A monotonic rebase helper advances at most one waypoint when the minion is inside the lane corridor and has already passed the current segment endpoint. The index never decreases. The long soak observed six lane slots, maximum stall `18` ticks, and `minion_opposed_yaw=0`.

### 3-4. Irelia Q wall traversal

Q previously reused ordinary per-tick movement segment clamping, so a cast that had already passed its target/range checks was stopped by intermediate wall collision. Q now validates a walkable landing on the target side at cast time and during target motion, traverses the segment without ordinary movement collision, then validates the final walkable/target-distance contract before applying damage. An invalid landing cancels, releases the action, and deals no impact damage.

Q damage is also tagged as `Skill/Q`, restoring the intended mobile-target kill reset behavior.

The cancellation fixture now advances Q for one tick before applying cannot-move, marks the current dash point unwalkable, and records the resolver call. It asserts exactly one call with `from=dashStart`, `rawTarget=cancelPositionBeforeResolve`, a non-origin resolved stop, and zero impact damage.

### 3-5. Baron, Dragon, level-up, and WFX

- Baron is authored as stationary at spawn. It is snapped to its anchor, never receives move/chase behavior, gains no proximity aggro before damage, and retaliates only against a champion currently within effective attack range. Out-of-range stationary attack commands are rejected without chase fallback.
- Dragon idle/move/attack all use the stable flying loop. Attack sequence changes are consumed without selecting the broken attack clip; the server attack cue instead plays `Objective.Elder.BasicAttack` FX.
- Baron keeps its valid attack clip; only movement authority was removed.
- Level-up `Recall.Channel` is attached to the champion entity for its one-second lifetime, so it follows rather than remaining at the original ground position.
- Red buff WFX uses the same GroundDecal shape/texture/timing as Blue with red color values.

### 3-6. UI Manager, Health Bars, and Minimap

- Health Bar and Champion Level controls use a left label column and a right control column with hidden stable IDs, removing label/slider overlap and truncation.
- World Health Bar settings load/save through strict, versioned, atomic `%LOCALAPPDATA%/Winters/Developer/world_health_bars.ini` persistence. Reset Selected, Reset All, and Save are distinct.
- Minimap controls are exposed inside UI Manager: panel ratio, right/bottom padding, icon scale, champion scale, projection extent, and champion outline thickness.
- Minimap settings load/save through `%LOCALAPPDATA%/Winters/Developer/minimap_layout.ini` with the same strict/atomic behavior.
- Champion portraits receive Blue/Red team-colored outlines; thickness is tunable from `0.5` to `8.0` px.
- Each saveable tab keeps its independent Save. The UI Manager body is a scroll child and the global `Save All` action remains in a fixed footer. Save All attempts HUD Layout, Health Bars, and Minimap independently, then reports all failed scopes instead of short-circuiting or claiming partial success. F4 Balance remains outside this presentation save path because it is a server-authoritative gameplay-data tool.

### 3-7. Same-seed world-keyframe correction

The first two long runs had identical replay hashes and semantic outcomes but different final keyframe byte hashes. Binary localization showed exactly `111` differing bytes, all in implicit alignment padding of three `ChampionAssistCreditComponent` values. The keyframe POD codec copies raw component bytes, so those indeterminate bytes violated byte determinism.

The implicit gaps were replaced with explicit zero-initialized reserved fields while preserving `Credit == 24 bytes` and the component `== 192 bytes`. Assist behavior and ABI size are unchanged. The repeated long soak now has identical replay and world hashes.

### 3-8. Map ping to AI behavior

The server-authoritative path is active and verified: a client `TeamPing` command is validated and projected to a walkable anchor, then written as a TTL-bound objective only to same-team bots. `Assist` lasts 180 ticks, resets bot decision cadence, moves bots toward the anchor, and switches to lane combat when an enemy becomes actionable. `Danger` lasts 90 ticks, affects only allies within the 12m danger radius, and emits retreat-to-safe-anchor behavior. Active combo/dive commitments are not interrupted mid-action, and the objective expires deterministically.

The integration probe verifies four allied bots receive Assist, act on the next decision tick, Danger eligibility/radius behavior is respected, replay commands are recorded, and the directive expires. This is the recommended shape for future `OnMyWay/Missing` semantics as well: ping changes a bounded AI objective/utility input, not a client-side movement teleport.

## 4. Verification

### 4-1. Builds

| Target | Configuration | Result |
|---|---|---|
| GameSim | Debug x64 | PASS |
| SimLab | Debug x64 | PASS |
| Server | Debug x64 | PASS |
| Server | Release x64 | PASS |
| Client | Debug x64 | PASS |
| Client | Release x64 | PASS |

Existing C4275 DLL-interface warnings remain; all final build attempts completed with zero compilation/link errors.
The first Release Client link attempt collided with another compiler process holding `WintersGame.lib` (`LNK1114`); no process was killed, and the immediate clean retry passed.

### 4-2. Automated regression

| Check | Result |
|---|---|
| Full SimLab | PASS, same-seed replay and seed sensitivity |
| `--irelia-q-only` | PASS: contact, moving target, wall transit, cancel resolver from/raw target, nearest resolved stop, zero cancel damage, kill reset, exclusions |
| `--objective-buffs-only` | PASS: rewards/buffs and stationary retaliation Baron |
| `--victory-economy-only` | PASS: XP pool `1/2/5 x4`, turret `1500 + 4x1000`, assist ping |
| `--f4-balance-only` | PASS: 17 champions, 85 skills, 323 rank cases plus live overrides |
| `--gamefeel-only` | PASS: attack chase resolved offset `2.000`, radius `4.950` |
| WFX shape/cue contract | PASS |
| `git diff --check` | PASS; line-ending warnings only |

### 4-3. Structure-nav and soak evidence

Debug 1,800 ticks, seed 42, two runs:

```text
DETERMINISM status=PASS runs=2
replay_hash=57C87299FE9673A4
world_hash=9DE26193C4CFDE9E
structure refresh: 17,414~20,597 us
first champion path query: 61~81 us
first minion path query: 5 us
derived rebuild calls: 0
minion opposed yaw: 0
```

Evidence: `.md/build/evidence/s024_bot_soak/debug_ticks_1800_seed_42_20260719_211307_283_2a5ca3e3`

Release 120 ticks:

```text
RESULT status=PASS
structure refresh: 1,715 us
first champion path query: 18 us
first minion path query: 0 us
deadline misses: 0
derived rebuild calls: 0
```

Evidence: `.md/build/evidence/s024_bot_soak/release_ticks_120_seed_42_20260719_211542_659_a430f214`

One prior Debug attempt completed gameplay successfully but exceeded the whole-soak 0.5% deadline limit by one tick (`10` misses where `9` were allowed); the immediate two-run repeat passed with `2` misses each. The structure refresh itself stayed below the `33,333 us` budget in every run.

## 5. Separated local override mismatch

`Test-F4BalanceContracts.py --root .` currently reports one failure:

```text
gold center expected [707,142], runtime file [734,148.5]
```

This is not an approved source-default change from this session. `ActorHUDPanel.cpp` still authors `[707,142]`; only ignored runtime file `Client/Bin/Resource/UI/hud_irelia_layout.json` contains the user-saved override. The contract expectation remains `707,142` and the local file was deliberately not overwritten. All GameSim-side F4 balance contracts pass.

## 6. Manual visual handoff

Automated code/build closure is complete. Normal F5 still needs visual confirmation for:

- Dragon stable flying loop and attack-only Elder FX;
- stationary Baron anchor and valid attack clip;
- level-up FX following a moving visible champion for one second;
- Health Bar label alignment and persistence after restart;
- UI Manager narrow-width scrolling, fixed Save All footer, independent Save, Save All success/failure message, and restart persistence;
- minimap panel/projection alignment, Blue/Red portrait outlines, outline thickness, and persistence after restart;
- live Irelia Q crossing a wall and rejecting an invalid target-side landing;
- Blue/Red buff decal color parity.

No visual PASS is claimed without that capture.

## 7. Prompt critique

The prompt was strong at reporting visible symptoms and desired outcomes, but initially mixed observations, hypotheses, and proposed fixes. “Waypoint calculation is wrong,” “rebuild mesh fixed it,” and “enlarging the minimap should align it” were hypotheses, not established causes. The request also combined nav performance, combat geometry, AI state transitions, animation, FX, persistence, HUD layout, and skill collision in one acceptance boundary.

A sharper issue template for future sessions is:

```text
reproduction -> observed evidence -> authoritative owner -> invariant -> measured acceptance criterion -> manual visual criterion
```

Using that separation is what exposed the two distinct turret issues: destroyed-obstacle refresh caused the frame stall, while raw-center/resolved-endpoint geometry caused the attack stepping. Treating both as “nav rebuild” would have preserved one of the bugs.
