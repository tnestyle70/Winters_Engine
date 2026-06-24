# LoL Sound Extract Guide

> Last updated: 2026-06-24
> Scope: Winters LoL in-game champion SFX and map/object SFX extraction
> Runtime output: `Client/Bin/Resource/Sound/LoL`
> Main script: `Tools/LoLData/Export-LoLSounds.ps1`

## Core Decision

Sound extraction should use a repeatable asset pipeline, not manual Obsidian browsing.

The runtime target is WAV, not MP4. Winters already loads audio from `Client/Bin/Resource/Sound` through the FMOD sound manager, and WAV is the safest direct-use SFX format for the current runtime.

Important boundary:

- Source LoL WAD/BNK/WPK files are external licensed assets.
- Generated WAV files under `Client/Bin/Resource/Sound/LoL` are local runtime resources and are ignored by git through `/Client/Bin/Resource/`.
- Do not ship Riot audio without a separate rights check. Treat this pipeline as local development and reference extraction.

## Tool Chain

The current pipeline uses two tools:

- `Tools/External/LeagueToolkitProbe/LeagueToolkitProbe.exe`
  - Added commands: `wad-has`, `wad-extract`
  - Purpose: deterministic extraction from LoL `.wad.client` files.
- `Tools/External/vgmstream/vgmstream-cli.exe`
  - Purpose: decode Wwise `.bnk` streams into `.wav`.
  - The whole `Tools/External/vgmstream/` folder is local-only and ignored by git.

Build prerequisite:

```powershell
dotnet build Tools\External\LeagueToolkitProbe\LeagueToolkitProbe.csproj -v:minimal
```

## Main Command

From repo root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Tools\LoLData\Export-LoLSounds.ps1 -Clean
```

Default inputs:

- LoL install root: `C:\Riot Games\League of Legends`
- Champion WAD root: `C:\Riot Games\League of Legends\Game\DATA\FINAL\Champions`
- Existing map sound banks:
  - `Client\Bin\Resource\Texture\MAP\assets\sounds`
  - `Client\Bin\Resource\Texture\MAP\MAP12\assets\sounds`

Default intermediate output:

- `Tools\Bin\Intermediate\LoLSoundBanks`

Default runtime output:

- `Client\Bin\Resource\Sound\LoL`

The `-Clean` flag removes only the script-owned intermediate and LoL sound output roots before regenerating them.

## What Gets Extracted

Champion SFX are extracted from champion WADs:

```text
assets/sounds/wwise2016/sfx/characters/{asset}/skins/base/{asset}_base_sfx_audio.bnk
assets/sounds/wwise2016/sfx/characters/{asset}/skins/base/{asset}_base_sfx_events.bnk
```

The current champion set:

```text
Annie, Ashe, Ezreal, Fiora, Garen, Irelia, Jax, Kalista, Kindred,
LeeSin, MasterYi, Riven, Sylas, Viego, Yasuo, Yone, Zed
```

Map/object SFX are converted from existing extracted map sound banks. The default patterns intentionally include gameplay-relevant banks and avoid hundreds of ward skin banks:

```text
env_map*_audio.bnk
npc_global_*_sfx_audio.bnk
npc_map11*_sfx_audio.bnk
npc_map12*_sfx_audio.bnk
npc_map14*_sfx_audio.bnk
```

Use `-AllMapAudioBanks` only when a broad audit is needed:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Tools\LoLData\Export-LoLSounds.ps1 -Clean -AllMapAudioBanks
```

## Current Verified Result

Latest verified run:

```text
championCount: 17
championBankCount: 17
mapBankCount: 86
wavCount: 4294
skippedCount: 1
missingCount: 0
```

Report path:

```text
Client\Bin\Resource\Sound\LoL\_LoLSoundExportReport.json
```

The single skipped map bank in the verified run:

```text
Client\Bin\Resource\Texture\MAP\assets\sounds\wwise2016\sfx\shared\npc_map11_bluemini_sfx_audio.bnk
reason: vgmstream-exit-1
```

Champion bank failures are treated as hard failures. Map bank failures are logged as `skipped` because some map `*_audio.bnk` files are empty, external-only, or unsupported by vgmstream even though the filename matches.

## Output Shape

Champion output example:

```text
Client\Bin\Resource\Sound\LoL\Champions\Annie\annie_base_sfx_1_12749723.wav
Client\Bin\Resource\Sound\LoL\Champions\Annie\annie_base_sfx.vgmstream.log
```

Map output example:

```text
Client\Bin\Resource\Sound\LoL\Map\MAP\npc_global_minions_ordermelee_sfx_audio_1_*.wav
Client\Bin\Resource\Sound\LoL\Map\MAP\env_map11_ambience_sfx_audio.vgmstream.log
```

The numeric suffix comes from vgmstream subsong and stream name/hash data. Do not hand-rename these files as the source of truth. Runtime JSON should reference the selected files explicitly once designers decide which stream belongs to which action.

## Runtime Integration Direction

This extraction pass only makes audio usable by Winters. It does not decide gameplay meaning.

Keep the layers separated:

```text
Source audio bank -> extracted WAV catalog -> designer/runtime JSON mapping -> client playback cue
```

Recommended next contract:

```json
{
  "champion": "Annie",
  "sounds": {
    "basicAttack": [
      "LoL/Champions/Annie/annie_base_sfx_1_12749723.wav"
    ],
    "skillQ": [],
    "skillW": [],
    "skillE": [],
    "skillR": [],
    "death": []
  }
}
```

Designers should own the mapping from action names to chosen WAV files. Developers should own loading, validation, hot reload, and playback routing. The client should play sound through the existing `CGameInstance`/FMOD sound path instead of introducing a second audio owner.

## Gotchas

- Do not use Obsidian as the primary workflow for this task. It is useful for inspection, but it does not leave a repeatable build step.
- Do not output MP4 for in-game SFX. The current runtime path wants directly playable audio under `Resource/Sound`, and WAV is already supported.
- Do not stage generated WAV files. `Client/Bin/Resource` is restored/generated runtime content.
- If vgmstream is missing, restore `Tools/External/vgmstream/vgmstream-cli.exe` locally from the official vgmstream Windows CLI release.
- If a champion audio bank fails, stop and inspect that champion WAD path first.
- If a map bank fails, check the report. It may be a non-decodable or empty bank and can remain `skipped` unless a specific missing runtime sound depends on it.

## Verification Checklist

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Tools\LoLData\Export-LoLSounds.ps1 -Clean
```

Then check:

```powershell
Get-Content -Raw Client\Bin\Resource\Sound\LoL\_LoLSoundExportReport.json | ConvertFrom-Json
Get-ChildItem Client\Bin\Resource\Sound\LoL -Recurse -Filter *.wav | Measure-Object
git diff --check
```

Expected for the current baseline:

- `missingCount` is `0`.
- Champion banks all have `status = converted`.
- `wavCount` is in the same range as the current baseline unless the champion/map scope changed.
- `git diff --check` has no whitespace errors.
