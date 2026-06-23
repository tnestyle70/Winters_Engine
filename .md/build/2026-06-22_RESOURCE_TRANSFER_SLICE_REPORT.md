# Winters Resource Transfer Slice Report

Date: 2026-06-22

## Summary

`Client/Bin/Resource` 실행 경로는 이동하지 않았다. 압축 전 검토용 전송 staging만 별도로 만들었다.

전송 staging root:

```text
C:\WRT_20260622
```

각 slice 안에는 `Client/Bin/Resource/...` 상대경로가 그대로 들어 있다. 다른 장비에서는 필요한 slice의 `Client` 폴더를 Winters repo root에 덮어 복사하면 된다.

현재 staging은 하드링크 기반이다. 디스크를 크게 추가로 쓰지 않지만, 압축하면 실제 파일 내용이 archive에 들어간다. 압축 전에는 staging 안의 파일을 편집하지 말고 읽기/압축 대상으로만 사용한다.

## Slice Inventory

| Slice | Purpose | Files | Size |
|---|---:|---:|---:|
| `00_Base` | Winters/LoL runtime base: `Font`, `Sound`, `Texture`, `UI` | 63,695 | 9,475.13 MiB |
| `10_Manifest` | EldenRing manifests/catalogs | 31 | 47.45 MiB |
| `20_Limgrave` | EldenRing `Assets/LimgraveStatic` + `Maps/Limgrave` | 187 | 102.37 MiB |
| `21_StartCave` | EldenRing `Maps/StartingCave` | 7 | 1.99 MiB |
| `30_Characters` | EldenRing cooked runtime characters: `Runtime/Character` | 574 | 229.22 MiB |
| `40_EffectMesh` | EldenRing effect/weapon mesh probe assets | 913 | 341.56 MiB |
| `90_EmptyDirs` | Empty EldenRing source/output scaffold only | 0 | 0 MiB |

Generated helper files:

```text
C:\WRT_20260622\SLICE_MANIFEST.tsv
C:\WRT_20260622\RESTORE_NOTES_KO.md
```

## Restore Commands

Run from any command prompt after clone/pull on the target desktop. Replace `<USER>` as needed.

```bat
robocopy C:\WRT_20260622\00_Base\Client C:\Users\<USER>\Desktop\Winters\Client /E
robocopy C:\WRT_20260622\10_Manifest\Client C:\Users\<USER>\Desktop\Winters\Client /E
robocopy C:\WRT_20260622\20_Limgrave\Client C:\Users\<USER>\Desktop\Winters\Client /E
robocopy C:\WRT_20260622\21_StartCave\Client C:\Users\<USER>\Desktop\Winters\Client /E
robocopy C:\WRT_20260622\30_Characters\Client C:\Users\<USER>\Desktop\Winters\Client /E
robocopy C:\WRT_20260622\40_EffectMesh\Client C:\Users\<USER>\Desktop\Winters\Client /E
robocopy C:\WRT_20260622\90_EmptyDirs\Client C:\Users\<USER>\Desktop\Winters\Client /E
```

## Upload Decision

For full current runtime sync, upload all slices.

For Winters/LoL runtime only, upload `00_Base`.

For current Elden Limgrave showcase, upload:

```text
10_Manifest
20_Limgrave
21_StartCave
30_Characters
```

Add `40_EffectMesh` when effect/weapon mesh probe validation is needed.

`90_EmptyDirs` is optional. It only restores empty scaffold folders for future local extraction output.

## Resource Items Not Included

The following are intentionally not in the payload as real files:

```text
Client/Bin/Resource/EldenRing/FullGame
Client/Bin/Resource/EldenRing/SourceBundles
Client/Bin/Resource/EldenRing/UI
Client/Bin/Resource/EldenRing/Characters
```

Those folders currently remain as empty scaffold. If a target desktop needs to recook assets, regenerate them from that machine's extraction/source setup instead of uploading the old full extraction payload.

## 2026-06-22 MAP Texture Restore

`Texture/MAP/output/textures` is runtime-required by `Texture/MAP/output/sr_base_flip.wmat`. It must stay in `00_Base`.

Restored from:

```text
C:\Users\tnest\Downloads\Texture.zip
```

Restored target:

```text
Client/Bin/Resource/Texture/MAP/output/textures
```

Restored size:

```text
35,775 files
4,435.51 MiB
```

## Tools And Generated Outputs

Runtime-only use does not require the Elden extraction toolchain. The cooked files in `Client/Bin/Resource` are enough.

For asset recook work, the repo already contains:

```text
Tools/EldenAssetPipeline/
Tools/WintersAssetConverter/
```

`Tools/Bin/Debug/WintersAssetConverter.exe` exists locally but is not tracked. It can be rebuilt from `Tools/WintersAssetConverter`, so it does not need to be uploaded unless the target desktop cannot build tools yet.

External recook dependencies are not part of the repo/resource payload:

```text
Blender
WitchyBND
UXM Selective Unpack
Soulstruct Blender add-on
texconv.exe
Original local game extraction/source files
```

Build outputs and generated local binaries can stay out of Drive upload when they are reproducible from source.
