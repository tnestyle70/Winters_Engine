# Phase B-9 M7 - 5 Champion Batch Conversion + Stage3 Fast-Path Verification

Date: 2026-04-28

Premise: Garen already passed the `.wmesh + .wskel + .wanim` Stage3 fast-path check.

Goal: Convert Irelia/Yasuo/Sylas/Viego/Kalista, verify Stage3 fast-path for all 6 champions, then close Phase B-9.

References:
- `.md/plan/Champion/04_GAREN_WSKEL_WANIM_VERIFICATION.md`
- `Tools/convert_all_assets.bat`
- Garen baseline: `garen.wskel`, `garen.wmesh` (`submeshes=2 bones=72 vertices=4912 stride=76`), 31 `.wanim`

---

## 0. Preflight

### 0.1 Check FBX files

| Champion | Expected FBX |
|---|---|
| Irelia | `Client/Bin/Resource/Texture/Character/Irelia/irelia_fixed.fbx` |
| Yasuo | `Client/Bin/Resource/Texture/Character/Yasuo/yasuo_fixed.fbx` |
| Sylas | `Client/Bin/Resource/Texture/Character/Sylas/sylas.fbx` |
| Viego | `Client/Bin/Resource/Texture/Character/Viego/viego_fixed.fbx` |
| Kalista | `Client/Bin/Resource/Texture/Character/Kalista/kalista.fbx` |

PowerShell check:

```powershell
$files = @(
  "Client/Bin/Resource/Texture/Character/Irelia/irelia_fixed.fbx",
  "Client/Bin/Resource/Texture/Character/Yasuo/yasuo_fixed.fbx",
  "Client/Bin/Resource/Texture/Character/Sylas/sylas.fbx",
  "Client/Bin/Resource/Texture/Character/Viego/viego_fixed.fbx",
  "Client/Bin/Resource/Texture/Character/Kalista/kalista.fbx"
)
$files | Where-Object { -not (Test-Path $_) }
```

No output means pass. Stop before conversion if anything is missing.

### 0.2 Check converter

```bat
Tools\Bin\Debug\WintersAssetConverter.exe
```

If missing, build `Tools/WintersAssetConverter` in Debug x64 first.

### 0.3 Check Garen wmesh regression

```bat
Tools\Bin\Debug\WintersAssetConverter.exe info Client/Bin/Resource/Texture/Character/Garen/garen.wmesh
```

Expected:

```text
[Mesh] submeshes=2 bones=72 vertices=4912 ... stride=76
```

If `stride != 76`, stop. The Skinned3D input layout contract is broken.

---

## 1. Conversion

### 1.0 Batch behavior

`Tools/convert_all_assets.bat` now has these rules:

- Champion order: `skel -> mesh --skel -> anim --skel`
- Output basename follows FBX basename: `irelia_fixed.fbx -> irelia_fixed.wskel/.wmesh`
- Stale animation cleanup before regeneration:

```bat
if not exist "%DIR%\anims" mkdir "%DIR%\anims"
del /Q "%DIR%\anims\*.wanim" 2>nul
"%CONV%" anim "%DIR%\%FBX%" --skel "%DIR%\%BASE%.wskel" -o "%DIR%\anims"
```

- `champions` argument converts only the 6 champions and skips static meshes.

### 1.1 Champion-only conversion

Default M7 command:

```bat
cd Tools
convert_all_assets.bat champions
```

Expected:

```text
OK=6 FAIL=0
```

### 1.2 Full asset conversion

Use only when static meshes also need refresh:

```bat
cd Tools
convert_all_assets.bat
```

In this mode `OK` can be larger than 6.

---

## 2. Output Verification

Each champion folder must contain:

```text
<basename>.wskel
<basename>.wmesh
anims/*.wanim
```

| Champion | Basename | Current Anim Count |
|---|---|---|
| Irelia | `irelia_fixed` | 68 |
| Yasuo | `yasuo_fixed` | 44 |
| Sylas | `sylas` | 80 |
| Viego | `viego_fixed` | 83 |
| Kalista | `kalista` | 56 |
| Garen | `garen` | 31 |

### 2.1 Header spot check

Run at least once per champion:

```bat
WintersAssetConverter.exe info <champ>.wskel
WintersAssetConverter.exe info <champ>.wmesh
WintersAssetConverter.exe info anims\<one>.wanim
```

Pass criteria:

- `.wmesh` stride = 76
- `.wmesh` bone count = `.wskel` bone count
- `.wanim` `skel_hash` = `.wskel` hash

`CmdInfo` must print `WAnimTrailer::skel_hash` for `.wanim`.

---

## 3. F5 Verification

### 3.1 Fast-path logs

Expected logs per loaded champion:

```text
[CModel] .wmesh+.wskel fast-path: ...<champ>.wmesh
[CModel] wskel loaded: bones=N hash=...
[CModel] Loaded N wanim files
[CModel] Loaded from .wmesh: meshes=M textures=K
```

Forbidden logs:

```text
[CModel] wmesh has bones but wskel missing - Assimp fallback
[CModel] wskel/wmesh mismatch - Assimp fallback
[CModel] .wmesh build failed
```

### 3.2 Selection coverage

Currently selectable in BanPick:

- Irelia
- Yasuo
- Kalista
- Garen

Currently loaded as Scene/NPC:

- Sylas
- Viego

So M7 verifies 4 champions through BanPick and verifies Sylas/Viego through InGame load logs plus visible NPC rendering. Adding Sylas/Viego BanPick buttons is a separate UX task.

### 3.3 Visual/input checks

| Item | Pass Criteria |
|---|---|
| Model visibility | correct position, not bind pose |
| Idle | idle loops |
| Run | right-click move plays run |
| BA | enemy hover/right-click plays attack |
| Q/W/E/R | existing Irelia/Yasuo/Kalista/Garen logic stays working |
| Camera follow | selected player champion is followed |
| Skinning | no mesh explosion, disappearance, or bone twist |

---

## 4. Risks / Gotchas

| # | Risk | Response |
|---|---|---|
| 1 | Champion idle/run key differences | Keep ChampionTable plus substring animation matching |
| 2 | Sylas blueprint loading race | Keep existing OnEnter synchronous registration fallback |
| 3 | Kalista multi-material mesh | Preserve `material_index`; verify texture slots |
| 4 | Viego multi-submesh mesh | Preserve `material_index`; spot check texture slots |
| 5 | Bone count limit | Vertex indices are `uint32_t[4]`, but shader `g_BoneMatrices[256]` is still the runtime limit; writer rejects `bone_count >= 256` |
| 6 | Bone count mismatch | Model.cpp falls back to Assimp |
| 7 | Stale wanim files | Batch deletes `anims/*.wanim` before regeneration |
| 8 | wanim filename prefix | Runtime uses `entry.path().stem()` plus substring matching |

---

## 5. Done Criteria

- [ ] 5 champion outputs exist (`.wskel`, `.wmesh`, `anims/*.wanim`)
- [ ] 6 champions show fast-path logs
- [ ] 4 selectable champions enter from BanPick
- [ ] Sylas/Viego render correctly as Scene/NPC entities
- [ ] No `falling back to Assimp` logs, or failing champion is isolated
- [ ] CLAUDE.md Phase B-9 Gotchas updated
- [ ] MEMORY.md + `project_phase_b9_garen_wskel_wanim.md` updated

---

## 6. Next

- B-10: Add Annie/Ashe/Fiora/Riven/Jax/MasterYi/Kindred/Yone/Zed
- Optional B-11: Bake AnimEvent into SkillDef castFrame flow
- C-0 to C-8: Enter PBR/GGX/Forward+ graphics track

---

## Summary

Convert 5 remaining champions with champion-only batch, verify all 6 champions against Stage3 fast-path, and close B-9.
