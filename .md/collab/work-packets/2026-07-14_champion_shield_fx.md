# Work Packet: Champion Shield FX

## Metadata

- ID: `2026-07-14_champion_shield_fx`
- Status: `Handoff`
- Agent: `Codex`
- Owner: Desktop
- Branch: `main` (shared dirty working tree, no commit created)
- Base: current shared working tree after the Yasuo E target-reuse-ring implementation
- Started: `2026-07-14`

## Objective

Use the existing Lee Sin W shield visual as the implementation reference for Jax R, Riven E, and Yasuo passive shields. Keep champion-specific runtime FBX/particle artwork and use the supplied UI captures as appearance references only, with attached bounded-lifetime visual paths that do not create client gameplay truth. Audit and document the existing Jax E animation-frame/playback fix in the same handoff.

## Owned Paths

- `Client/Private/GameObject/Champion/Jax/**` (R visual hook/preset only)
- `Client/Public/GameObject/Champion/Jax/**` (R visual declaration/tuning only)
- `Client/Private/GameObject/Champion/Riven/**` (E visual hook/preset only)
- `Client/Public/GameObject/Champion/Riven/**` (E visual declaration/tuning only)
- `Client/Private/GameObject/Champion/Yasuo/**` (passive shield visual hook/preset only)
- `Client/Public/GameObject/Champion/Yasuo/**` (passive shield visual declaration/tuning only)
- `Shared/GameSim/Systems/Damage/DamageQueueSystem.cpp` (Yasuo passive shield activation visual event only)
- `Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h` (passive visual hook variant only)
- `Client/Private/Scene/Scene_InGame.cpp` (Riven local cast-frame duplicate-visual suppression only)
- `Data/LoL/FX/Champions/Jax/**` (new/updated R shield WFX only)
- `Data/LoL/FX/Champions/Riven/**` (new/updated E shield WFX only)
- `Data/LoL/FX/Champions/Yasuo/**` (new/updated passive shield WFX only)
- `.md/build/2026-07-14_CHAMPION_SHIELD_FX_REPORT.md`
- `.md/collab/work-packets/2026-07-14_champion_shield_fx.md`
- `.md/collab/ACTIVE_WORK_PACKETS.md` (this row only)

## Read-Only / Excluded Paths

- Lee Sin gameplay, visual, and WFX files (reference only)
- server damage, shield amount, cooldown, and buff-authority behavior outside the passive activation event bridge
- source PNG/FBX binaries under `Client/Bin/Resource`
- unrelated Jax/Riven/Yasuo skills and existing dirty changes

## Validation

- Confirm each visual starts from the correct accepted/passive-trigger event and is not duplicated between local and network-authoritative paths.
- Confirm each visual follows its champion, stops with a bounded lifetime, and dies when its attachment becomes invalid.
- Confirm supplied RGBA textures are sampled with a compatible blend/depth mode and the Lee Sin reference mesh/plane route does not alter gameplay truth.
- Parse all touched WFX files, run scoped `git diff --check`, and build Client Debug x64.
- Record exact code/document evidence for the Jax E animation-frame/playback fix.

## Handoff Notes

- Jax R and Riven E retain their existing champion FBX shells and add Lee Sin-style rim/outer-glow layers; Yasuo passive now has an authoritative activation event and a four-layer attached WFX.
- Riven local/practice E duplicate visual playback is suppressed. Riven E remains visual-only because the existing server GameSim has no Riven E shield truth; this is explicit in the report.
- Jax E exact-name animation replay failure and substring-match fix are documented in `.md/build/2026-07-14_CHAMPION_SHIELD_FX_REPORT.md`.
- Final Debug x64 builds PASS for Client, Server, and SimLab; deterministic SimLab PASS.
