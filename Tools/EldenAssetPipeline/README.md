# Elden Asset Pipeline Tools

This folder contains the local tooling for the Elden Ring asset extraction pipeline.
The tools assume raw game assets stay outside the repo and only manifests, scripts,
and Winters engine code are tracked.

## Flow

```text
UXM / WitchyBND output
-> MATBIN XML / FXR XML
-> normalized JSON manifests
-> Blender material auto-binding
-> WintersAssetConverter .wskel/.wmesh/.wmat/.wanim
-> WintersElden asset probe scene
```

## Tools

| File | Purpose |
|---|---|
| `elden_pipeline.py` | Parses Witchy MATBIN/FXR XML and builds Blender material binding manifests. |
| `blender_apply_materials.py` | Runs inside Blender to inspect FBX materials or apply a binding manifest and re-export FBX. |

## Examples

Parse a WitchyBND MATBIN XML into a material manifest:

```bat
python Tools\EldenAssetPipeline\elden_pipeline.py parse-matbin ^
  --input "C:\path\to\C[c3000]_WP_Crossbow_Metal.matbin.xml" ^
  --texture-root "C:\Users\tnest\Desktop\EldenRing\chr3000\chr3000.fbm" ^
  --out "C:\Users\tnest\Desktop\EldenRing\_winters_probe\matbin_manifest.json"
```

Parse a WitchyBND FXR XML and resolve its integer candidates against local SFX
resources:

```bat
python Tools\EldenAssetPipeline\elden_pipeline.py parse-fxr ^
  --input "C:\path\to\f000523390.fxr.xml" ^
  --out "C:\Users\tnest\Desktop\EldenRing\_winters_probe\fxr_manifest.json"

python Tools\EldenAssetPipeline\elden_pipeline.py resolve-fxr ^
  --fxr-manifest "C:\Users\tnest\Desktop\EldenRing\_winters_probe\fxr_manifest.json" ^
  --resource-root "C:\Users\tnest\Desktop\EldenRing\sfx" ^
  --out "C:\Users\tnest\Desktop\EldenRing\_winters_probe\fxr_resources.json"
```

Inspect FBX material names through Blender:

```bat
"C:\Users\tnest\Downloads\blender-4.2.18-windows-x64\blender-4.2.18-windows-x64\blender.exe" ^
  --factory-startup --background ^
  --python Tools\EldenAssetPipeline\blender_apply_materials.py -- ^
  --input "C:\Users\tnest\Desktop\EldenRing\chr3000\chr3000.fbx" ^
  --inspect-json "C:\Users\tnest\Desktop\EldenRing\_winters_probe\chr3000_materials.json"
```

Build Blender material bindings:

```bat
python Tools\EldenAssetPipeline\elden_pipeline.py build-bindings ^
  --materials-json "C:\Users\tnest\Desktop\EldenRing\_winters_probe\chr3000_materials.json" ^
  --matbin-manifest "C:\Users\tnest\Desktop\EldenRing\_winters_probe\matbin_manifest.json" ^
  --fxr-resource-manifest "C:\Users\tnest\Desktop\EldenRing\_winters_probe\fxr_resources.json" ^
  --texture-root "C:\Users\tnest\Desktop\EldenRing\chr3000\chr3000.fbm" ^
  --out "C:\Users\tnest\Desktop\EldenRing\_winters_probe\chr3000_bindings.json"
```

Binding priority:

1. exact MATBIN material match
2. existing FBX image slots from the Blender inspect JSON
3. FXR/SFX resource manifest hints for `P[...]` effect materials
4. texture suffix fallback from `--texture-root`

The existing FBX image pass is intentionally kept in the loop because extracted
FBX files sometimes preserve corrected texture references even when material
tokens and texture stems differ.

The FXR/SFX pass builds texture groups such as `31211 -> s31211_a.png` and also
supports a conservative `P[WP_A_7050] -> s87050` fallback for already-extracted
effect meshes that no longer carry image slots in FBX.

Apply the bindings and re-export a normalized FBX:

```bat
"C:\Users\tnest\Downloads\blender-4.2.18-windows-x64\blender-4.2.18-windows-x64\blender.exe" ^
  --factory-startup --background ^
  --python Tools\EldenAssetPipeline\blender_apply_materials.py -- ^
  --input "C:\Users\tnest\Desktop\EldenRing\chr3000\chr3000.fbx" ^
  --bindings "C:\Users\tnest\Desktop\EldenRing\_winters_probe\chr3000_bindings.json" ^
  --output "C:\Users\tnest\Desktop\EldenRing\normalized\chr3000\chr3000_bound.fbx"
```

Convert a low-bone probe mesh into Winters binary:

```bat
Tools\Bin\Debug\WintersAssetConverter.exe skel ^
  "C:\Users\tnest\Desktop\EldenRing\sfx\WP_A_7051.fbx" ^
  -o "C:\Users\tnest\Desktop\EldenRing\_winters_probe\sfx\WP_A_7051.wskel"

Tools\Bin\Debug\WintersAssetConverter.exe mesh ^
  "C:\Users\tnest\Desktop\EldenRing\sfx\WP_A_7051.fbx" ^
  --skel "C:\Users\tnest\Desktop\EldenRing\_winters_probe\sfx\WP_A_7051.wskel" ^
  -o "C:\Users\tnest\Desktop\EldenRing\_winters_probe\sfx\WP_A_7051.wmesh"

Tools\Bin\Debug\WintersAssetConverter.exe info ^
  "C:\Users\tnest\Desktop\EldenRing\_winters_probe\sfx\WP_A_7051.wmesh"
```

## Notes

- `parse-matbin` consumes WitchyBND XML, not raw MATBIN binaries.
- `parse-fxr` consumes WitchyBND XML, not raw FXR binaries.
- Use Blender `--factory-startup --background` for repeatable CLI runs. It avoids user add-ons that may expect a foreground GPU context.
- Elden character skeletons currently exceed the engine's 256 bone shader limit, so use low-bone FX/weapon meshes for the first Winters binary render smoke test.
