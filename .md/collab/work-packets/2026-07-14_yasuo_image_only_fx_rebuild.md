# Work Packet: Yasuo Image-Only Q3 / EQ / R FX Rebuild

## Metadata

- ID: `2026-07-14_yasuo_image_only_fx_rebuild`
- Status: `Handoff`
- Agent: `Codex`
- Owner: Desktop
- Branch: `main` (shared dirty working tree, no commit created)
- Base: current shared working tree after `2026-07-14_yasuo_e_dash_trail_yaw` handoff
- Started: `2026-07-14`

## Objective

Retire the active FBX `MeshParticle` paths from Yasuo Q3 tornado, EQ, and R visuals, then rebuild those cues from existing RGBA particle textures using only billboard-backed WFX emitters whose size is controlled by width and height.

The gameplay screenshots `03`, `04`, `07`, `08`, `09`, `10`, and `11` under `Client/Bin/Resource/Texture/UI/이펙트 이미지/Yasuo` are visual targets only. They are opaque full-frame captures and must not be referenced by runtime WFX.

## Owned Paths

- `Data/LoL/FX/Champions/Yasuo/q_tornado.wfx`
- `Data/LoL/FX/Champions/Yasuo/eq_ring.wfx`
- `Data/LoL/FX/Champions/Yasuo/eq_inner_wind.wfx`
- `Data/LoL/FX/Champions/Yasuo/eq_wind_ring.wfx` (retire)
- `Data/LoL/FX/Champions/Yasuo/r_sword_glow.wfx`
- `Data/LoL/FX/Champions/Yasuo/r_land_impact.wfx`
- `Client/Public/GameObject/Champion/Yasuo/YasuoFxPresets.h` (Q/R image-only signatures only)
- `Client/Public/GameObject/Champion/Yasuo/Yasuo_Tuning.h` (retired Q3 mesh-only scale/color fields only)
- `Client/Private/GameObject/Champion/Yasuo/YasuoFxPresets.cpp` (Q/R image-only cue context and EQ cue consolidation only)
- `Client/Private/GameObject/Champion/Yasuo/Yasuo_Skills.cpp` (Q/R call sites only)
- `Data/LoL/ClientPublic/Visual/ObjectVisualDefs.json` (two retired Yasuo Q/R mesh preload entries only)
- `Client/Private/Data/Generated/LoLVisualDefinitions.generated.cpp` (matching two generated preload definitions and table rows only)
- `Client/Private/GameObject/FX/FxLegacyManifest.cpp` (Yasuo Q3/EQ/R render-type metadata only)
- `.md/build/2026-07-14_YASUO_IMAGE_ONLY_FX_REBUILD_REPORT.md`
- `.md/collab/work-packets/2026-07-14_yasuo_image_only_fx_rebuild.md`
- `.md/collab/ACTIVE_WORK_PACKETS.md` (this row only)

## Read-Only / Excluded Paths

- `Client/Bin/Resource/Texture/UI/이펙트 이미지/Yasuo/**`
- `Client/Bin/Resource/Texture/Character/Yasuo/particles/**`
- `Client/Private/UI/WfxEffectToolPanel.cpp`
- `Engine/**`, `EngineSDK/**`, `Shared/**`, `Server/**`
- `Client/Bin/**`, `Server/Bin/**`, and all running Winters processes

## Validation

- Parse every changed WFX as JSON.
- Require no `MeshParticle` render type and no `model` field in the five active Q3/EQ/R cue files.
- Require every referenced texture to exist under the runtime resource root.
- Require cue names to remain unique and the retired `Yasuo.EQ.WindRing` route to have no runtime call.
- Run scoped `git diff --check` and Debug x64 syntax/type gates for the changed Client translation units.
- Defer full link and in-game visual A/B while the user's currently running Client and Server remain untouched.

## Handoff Notes

- `MeshParticle` ignores WFX `width` and `height`; it uses only `scale[x,y,z]` for the FBX world matrix.
- WFX Preview spawns a new in-memory preview without immediately deleting the previous instance. A previously non-zero FBX can remain visible until its lifetime expires.
- Do not keep a second `.wfx` with the same cue name under `Data/LoL/FX`; recursive registry load order would make the winner nondeterministic.
- The five active Q3/EQ/R WFX files now contain only Billboard, GroundDecal, and ShockwaveRing emitters; JSON, texture, cue-uniqueness, and retired-reference checks passed.
- The two unused Q/R FBX preload definitions and the EQ third-cue route were removed; LoL definition-pack parity check passed at `0xB6EEDF6E`.
- Debug x64 direct syntax/type gates and the full serial Client build passed. The resulting executable is `Client/Bin/Debug/WintersGame.exe`.
- A fresh-client Q3/EQ/R visual A/B remains the only pending manual gate. The shared working tree was not committed or pushed.
