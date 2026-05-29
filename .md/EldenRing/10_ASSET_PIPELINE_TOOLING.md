# Elden Asset Pipeline Tooling

## Current Decision

RHI and `WintersElden.exe` boot can proceed after the engine side is ready. The asset pipeline does not need to wait for that. We can finish the extractor/manifest/binary preparation layer first.

The toolchain target is:

```text
WitchyBND XML
-> MATBIN/FXR normalized manifest
-> Blender material auto mapping
-> normalized FBX
-> WintersAssetConverter
-> .wskel/.wmesh/.wmat/.wanim
```

## Added Tools

| Tool | Role |
|---|---|
| `Tools/EldenAssetPipeline/elden_pipeline.py` | MATBIN XML parser, FXR XML parser, Blender binding manifest builder |
| `Tools/EldenAssetPipeline/blender_apply_materials.py` | Blender FBX material inspector and automatic material applier |
| `Tools/EldenAssetPipeline/README.md` | Usage examples |

## MATBIN Strategy

We use WitchyBND as the binary frontend.

```text
*.matbin
-> WitchyBND
-> *.matbin.xml
-> elden_pipeline.py parse-matbin
-> material_manifest.json
```

The parser extracts:

| Field | Usage |
|---|---|
| `ShaderPath` | Material shader family |
| `SourcePath` | Original FromSoftware material source path |
| `Params` | Future `.wmat` parameter seed |
| `Samplers` | Texture binding source |
| `Sampler.Type` | Role detection: albedo, normal, mask, emissive, roughness, specular, height |
| `Sampler.Path` | Texture source stem such as `c3000_WP_closbow_a.tif` |

When `--texture-root` is supplied, the parser resolves the MATBIN source texture stem to local `.png/.dds/.tif` files.

## FXR Strategy

FXR is more ambiguous than MATBIN. The first parser intentionally records the whole useful structure instead of pretending every action field is already known.

```text
*.fxr
-> WitchyBND
-> *.fxr.xml
-> elden_pipeline.py parse-fxr
-> fxr_manifest.json
```

The parser extracts:

| Field | Usage |
|---|---|
| `FXR id/version` | Effect identity |
| containers | Graph hierarchy |
| effects | Effect nodes |
| actions | Action id, fields, properties |
| integer candidates | Future model/texture/action resolver seed |

The resolver pass maps known integer candidates to local SFX resource groups.

```text
fxr_manifest.json
+ sfx resource root
-> resolve-fxr
-> fxr_resources.json
```

Current rules:

| Rule | Meaning |
|---|---|
| `s{ID}_*.png` exact match | FXR integer `31211` resolves to `s31211_a.png` |
| suffix role detection | `_a` albedo, `_n` normal, `_m/_1m/_3m` mask, `_em` emissive, `_r` roughness |
| action locations | keep action id, container path, field bucket, field index for each candidate |
| small integer filter | values below `--min-resource-id` are treated as flags/counts/noise |
| `P[...]` fallback | already-extracted FX mesh token `P[WP_A_7050]` can probe `s87050+` texture groups |

## Blender Auto Mapping

The Blender flow is split into two stable steps.

1. Inspect material names from FBX.
2. Build bindings from MATBIN + FXR/SFX resources + suffix fallback.
3. Apply bindings inside Blender and export normalized FBX.

```text
blender_apply_materials.py --inspect-json
-> chrXXXX_materials.json

elden_pipeline.py build-bindings
-> chrXXXX_bindings.json

blender_apply_materials.py --bindings --output
-> chrXXXX_bound.fbx
```

The binding builder resolves material names such as:

```text
#02# [1 | C[c3000]_WP_Crossbow_Metal | c3000]
```

into token:

```text
C[c3000]_WP_Crossbow_Metal
```

and then tries:

1. exact MATBIN material match
2. existing FBX image slots from Blender inspect JSON
3. FXR/SFX resource manifest hints for `P[...]` effect materials
4. normalized texture stem match
5. suffix fallback: `_a`, `_n`, `_m`, `_em`, `_r`, `_1m`, `_3m`
6. variant stripping: `_metal`, `_crystal`, `_cloth`, `_norich`, `_leather`, `_fabric`, `_wood`, `_iron`, `_rope`

Existing FBX images are part of the resolver on purpose. In the `chr3000`
probe, material token `C[c3000]_WP_MediumShield` maps to texture files named
`c3000_WP_MidiumShield_*`. The inspect-image pass preserves that mapping and
prevents broad suffix fallback from picking unrelated weapon textures.

## 2026-05-25 Validation

| Probe | Result |
|---|---|
| MATBIN sample `C[c3000]_WP_Crossbow_Metal.matbin.xml` | Parsed; local `_a/_n/_m` PNGs resolved |
| FXR sample `f000523390.fxr.xml` | Parsed; id `523390`, containers `12`, effects `11`, actions `180` |
| FXR resolver sample | Resolved texture IDs `31211`, `31251`, `34010`, `34020`, `34030` from action candidates |
| Blender inspect `chr3000.fbx` | 51 material records, 49 real material bindings, 2 helper materials unresolved |
| Blender apply dry-run `chr3000` | 49 applied, `Dots Stroke` and `Material` unresolved as expected |
| Blender normalized export | `C:/Users/tnest/Desktop/EldenRing/_winters_probe/normalized/chr3000_bound.fbx` generated |
| `.wmat` extraction from normalized FBX | 49 materials; first diffuse path resolved |
| `WP_A_7051` Winters binary | `.wskel/.wmesh/.wmat` generated after converter rebuild |
| `WP_A_7051` FXR/SFX binding | 6 FX materials resolved to `s87050/s87051/s87052` albedo+normal |

Current `WP_A_7051` material status:

| Item | Result |
|---|---|
| FBX material tokens | `P[WP_A_7050]_Glow` variants |
| FBX image slots | none |
| suffix fallback | unresolved by plain texture stem |
| FXR/SFX fallback | resolved to `s87050`, `s87051`, `s87052` based on `P[WP_A_7050]` and material slot pairs |
| next requirement | replace heuristic `P[...] -> s8....` with direct FXR XML from the matching effect bundle when available |

This means character-style FBX material rebinding is usable, and pure FX
materials now have a first resolver path driven by FXR IDs, SFX texture naming,
and a conservative `P[...]` fallback.

## First Render Target

For the first `WintersElden` or asset probe scene, use:

```text
C:/Users/tnest/Desktop/EldenRing/sfx/WP_A_7051.fbx
```

Reason:

| Item | Result |
|---|---|
| `.wskel` | success, 39 bones |
| `.wmesh` | success |
| `.wmat` | success after rebuilding `WintersAssetConverter.exe` |
| `.wmesh info` | submeshes 6, vertices 1485, stride 76 |
| normalized bound FBX | `C:/Users/tnest/Desktop/EldenRing/_winters_probe/normalized/WP_A_7051_bound.fbx` |
| normalized bound binary | `.wskel/.wmesh/.wmat` generated; first diffuse path `s87050_a.png` |

Do not use `chr3010` as the first `.wmesh` render target until the engine supports more than 256 bones.

## RHI / Client Boundary

After RHI work is done:

```text
WintersEngine.dll
-> WintersElden.exe
-> Scene_EldenAssetProbe
-> load .wmesh/.wskel/.wanim from extracted manifest
```

This remains separate from `WintersLOL.exe`. The LoL client should not become the Elden asset testbed.
