# Bot AI Validation Harness Report

- Date: 2026-06-24 05:49:12 +09:00
- Repo: C:\Users\user\Desktop\Winters
- Configuration: Debug
- Overall: WARN

## Steps

- PASS git diff --check exit=0 seconds=0.16
- PASS ChampionAI dependency boundary audit exit=1 seconds=0.02
  - Notes: No matches.
- WARN Yone E stage-2 contract audit exit=0 seconds=0.1
  - Notes: Known gap allowed for collaboration bootstrap only.
- WARN LoL data-driven pipeline and SimLab regression exit=0 seconds=0
  - Notes: Skipped by -SkipFullPipeline.

## Output Tail

### git diff --check

```text
cmd.exe : warning: in the working copy of '.md/EldenRing/02_ASSET_EXTRACTION_TO_WINTERS_BINARY_PIPELINE.md', LF will be
 replaced by CRLF the next time Git touches it
At C:\Users\user\Desktop\Winters\Tools\Harness\Run-BotAiValidation.ps1:77 char:20
+             $raw = & cmd.exe /c $CommandLine 2>&1
+                    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    + CategoryInfo          : NotSpecified: (warning: in the... Git touches it:String) [], RemoteException
    + FullyQualifiedErrorId : NativeCommandError

warning: in the working copy of '.md/EldenRing/10_ASSET_PIPELINE_TOOLING.md', LF will be replaced by CRLF the next time
 Git touches it
warning: in the working copy of '.md/collab/ACTIVE_WORK_PACKETS.md', LF will be replaced by CRLF the next time Git touc
hes it
warning: in the working copy of '.md/collab/GIT_SYNC_RULES.md', LF will be replaced by CRLF the next time Git touches i
t
warning: in the working copy of '.md/collab/HARNESS_RULES.md', LF will be replaced by CRLF the next time Git touches it
warning: in the working copy of '.md/collab/OWNERSHIP_MATRIX.md', LF will be replaced by CRLF the next time Git touches
 it
warning: in the working copy of 'Client/Private/GameObject/FX/FxLegacyManifest.cpp', LF will be replaced by CRLF the ne
xt time Git touches it
warning: in the working copy of 'Client/Private/Scene/Loader.cpp', LF will be replaced by CRLF the next time Git touche
s it
warning: in the working copy of 'Client/Private/Scene/Scene_Loading.cpp', LF will be replaced by CRLF the next time Git
 touches it
warning: in the working copy of 'Client/Private/Scene/Scene_MatchLoading.cpp', LF will be replaced by CRLF the next tim
e Git touches it
warning: in the working copy of 'Client/Private/UI/EffectTuner.cpp', LF will be replaced by CRLF the next time Git touc
hes it
warning: in the working copy of 'Client/Public/Scene/Loader.h', LF will be replaced by CRLF the next time Git touches i
t
warning: in the working copy of 'Server/Private/Network/IOCPCore.cpp', LF will be replaced by CRLF the next time Git to
uches it
warning: in the working copy of 'Server/Private/Network/Session_Manager.cpp', LF will be replaced by CRLF the next time
 Git touches it
warning: in the working copy of 'Server/Public/Network/Session_Manager.h', LF will be replaced by CRLF the next time Gi
t touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/Command_generated.h', LF will be replaced by CRLF the nex
t time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/Event_generated.h', LF will be replaced by CRLF the next
time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/LobbyTypes_generated.h', LF will be replaced by CRLF the
next time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/Snapshot_generated.h', LF will be replaced by CRLF the ne
xt time Git touches it
warning: in the working copy of 'Tools/EldenAssetPipeline/elden_pipeline.py', LF will be replaced by CRLF the next time
 Git touches it
warning: in the working copy of 'Tools/External/LeagueToolkitProbe/Program.cs', LF will be replaced by CRLF the next ti
me Git touches it
```

### Yone E stage-2 contract audit

```text
skill.yone.e stage.count is 1; expected >= 2 for itemId=2 recast.
skill.yone.e stage.windowSeconds is 0; expected > 0 for stage-2 recast.
```

