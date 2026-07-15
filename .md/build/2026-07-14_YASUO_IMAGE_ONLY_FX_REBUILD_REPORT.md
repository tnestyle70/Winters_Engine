# Yasuo Image-Only Q3 / EQ / R FX Rebuild Report

- Date: 2026-07-14
- Status: implementation and Client Debug x64 build PASS; fresh-client visual A/B PENDING
- Visual targets: `03/04` for Q3 tornado, `10/11` for EQ, `07/08/09` for R

## Why a mesh remained visible at width/height zero

The old WFX editor exposed `Width` and `Height` for every emitter type, but those fields do not control an FBX `MeshParticle`.

- `CFxCuePlayer::BuildCueBillboard` copies `width` and `height` into billboard-backed emitters.
- `CFxCuePlayer::BuildCueMesh` ignores those fields and copies only `scale[x,y,z]` into `FxMeshComponent::vScale`.
- `CFxMeshSystem` creates the mesh world matrix with `XMMatrixScaling(vScale.x, vScale.y, vScale.z)`.
- A true mesh scale of `[0,0,0]` is not clamped or restored by the loader or model cache, so the current instance should collapse visually.

There were three independent reasons a Preview could still look non-zero:

1. `Preview Edited` registers the in-memory WFX document and spawns another cue, but does not destroy the previously spawned preview entities. An earlier non-zero Q or R FBX therefore remains until its lifetime expires.
2. Editing JSON outside the tool does not update the in-memory document until `Load` is pressed.
3. The old Q and R cues also contained Billboard and GroundDecal emitters. Those layers still render even when the FBX itself is collapsed.

This is why changing Width/Height appeared to have no effect on `q_tornado.wfx` and `r_sword_glow.wfx`. FBX size was a Transform Scale contract, not a Width/Height contract.

## Reference-image decision

The seven files under `Client/Bin/Resource/Texture/UI/이펙트 이미지/Yasuo` are full gameplay screenshots, not particle sprites:

- Q3: `03.png`, `04.png`
- R: `07.png`, `08.png`, `09.png`
- EQ: `10.png`, `11.png`

Every sampled pixel has alpha 255 and the images include the map, champions, cursor, and HUD. Referencing them directly from WFX would render a rectangular gameplay capture. Their non-ASCII parent path would also be unsafe with the current ASCII WFX path parser/saver. They are therefore used only as composition and timing targets.

The active effects use existing transparent or additive-safe sprites under `Client/Bin/Resource/Texture/Character/Yasuo/particles`.

## Runtime route after the rebuild

### Q3 tornado

The authoritative visual route remains singular:

`EffectTrigger stage 3 -> Yasuo visual hook -> SpawnQTornado -> Yasuo.Q.Tornado`

`q_tornado.wfx` now contains seven image emitters:

- one moving ground shadow/decal;
- two animated core/afterimage billboards;
- three staggered four-frame wisp bands at lower, middle, and upper heights;
- one additive wind-ring layer.

All layers receive the existing cue velocity and Wind Wall blockability. No second travel cue was added to `ProjectileVisualCatalog`; the existing victim-attached `Yasuo.Q.TornadoHit` remains the hit-only route.

### EQ

`SpawnEQRing` now plays only two cues:

- `Yasuo.EQ.Ring`: a ground wind ring plus an expanding `ShockwaveRing`;
- `Yasuo.EQ.InnerWind`: an owner-attached camera ring, animated wind band, two-frame swirl, and core flash.

The former third `Yasuo.EQ.WindRing` cue and `eq_wind_ring.wfx` were removed. Its useful RGBA ring texture is now part of the two active cues, so EQ no longer stacks a redundant third route.

### R

`SpawnRLastBreath` no longer depends on `CFxStaticMeshRenderer`.

- `Yasuo.R.SwordGlow` layers the `07 -> 08 -> 09` sequence as staggered additive wind arcs, sword arc, echo, and flash attached to Yasuo.
- `Yasuo.R.LandImpact` layers the landing crack, cyan dome, expanding ring, wind blast, and hit flash at the target landing position.

Both `r_sword_glow.wfx` and `r_land_impact.wfx` are image-only. Removing only the old sword mesh would have been incomplete because the landing cue also contained a separate ground-blast FBX.

## Legacy retirement boundary

The active runtime path no longer references the retired Q3/EQ/R FBXs.

- Removed every `MeshParticle` emitter and `model` field from the five active WFX files.
- Removed Q3/R mesh-renderer parameters and gates from Yasuo FX preset code.
- Removed unused `qTornadoScale` and `qTornadoColor` mesh-era tuning fields.
- Removed the EQ third-cue constant, play call, and WFX file.
- Removed `fx.yasuo.q_tornado_blade` and `fx.yasuo.r_sword_wind` from `ObjectVisualDefs.json` and the generated visual-definition pack.
- Updated the legacy manifest's Yasuo render-type metadata to the new image-only emitter sets.

The raw FBX files remain in the shared resource archive because deleting source assets is not required to retire a runtime route and could affect future asset archaeology. Repository runtime code and active data contain no reference to those five retired model paths.

## Verification record

- Five changed WFX files parse as JSON: PASS.
- Required schema, cue name, and emitter lists: PASS.
- Referenced runtime textures exist: PASS.
- No `MeshParticle` or `model` remains in the five active cue files: PASS.
- Target cue uniqueness across `Data/LoL/FX`: PASS.
- `Yasuo.EQ.WindRing` active file and runtime call are absent: PASS.
- Retired Q3/EQ/R FBX runtime-reference scan: PASS.
- LoL definition-pack parity: `Build-LoLDefinitionPack.py --check` PASS, pack `0xB6EEDF6E`.
- Direct Debug x64 syntax/type gates for `YasuoFxPresets.cpp` and `Yasuo_Skills.cpp`: PASS.
- Full `Client/Include/Client.vcxproj`, `Debug|x64`, serial build: PASS, exit 0.
- Output: `Client/Bin/Debug/WintersGame.exe`.
- Scoped `git diff --check`: PASS; only existing LF-to-CRLF notices were emitted.

WFX SHA-256 values:

- `q_tornado.wfx`: `32DF73494F372B74F563F2040E800A1B9663ED6E7729A1293B7B8330D30E11F9`
- `eq_ring.wfx`: `3C6D250EAB201376FDA3E14E27B725DCF1BFF24E91AA7A703C35236DD7301E2D`
- `eq_inner_wind.wfx`: `67D5EEDA36A4C7C9C79698AA2B6541518106EA5EBB8B01E4E932CA7A8017698E`
- `r_sword_glow.wfx`: `D7E8BE181C44560762F00A171FA75F40D7C0C16978624EA4C9AF4630DA825762`
- `r_land_impact.wfx`: `4957D2D1434E9F1135F8EEC00AAAC91E5993EB0D42DF0B490BBCFF32E0F97743`

The build still prints the repository's existing C4275 DLL-interface warnings for Engine ECS systems; there were no new errors.

## Fresh-client visual A/B

Use a newly started Debug Client so the WFX registry and removed mesh preloads are fresh.

1. Q3: obtain tornado, cast in four directions, and confirm the entire layered column travels with the projectile. Confirm Wind Wall blocks the travel effect and the victim hit cue still appears once.
2. EQ: press Q during E and confirm one ground ring plus one owner-attached wind composition, with no third duplicate ring and no FBX fragment.
3. R: confirm the attached slash sequence reads in the `07 -> 08 -> 09` order and that the landing composition appears at the airborne target's landing position.
4. WFX Preview: after loading a file, wait longer than the old cue lifetime before comparing repeated previews. `Preview Edited` does not clear older instances.

Primary tuning controls are now consistent:

- overall image size: `width`, `height`;
- vertical placement: `attach_offset.y`;
- temporal staging: `start_delay`, `fade_in`, `fade_out`;
- extracted strip animation: `atlas_cols`, `atlas_frame_count`, `atlas_fps`;
- brightness and separation: additive `color` and alpha.

One renderer limitation remains explicit: a camera-facing Billboard ignores WFX yaw/roll. The image-only R sword arc is therefore a camera-facing approximation. If a later visual pass requires an exact diagonal screen-space slash, add a centered transparent slash sprite or generic billboard-roll support; do not restore the retired FBX route merely to rotate the quad.
