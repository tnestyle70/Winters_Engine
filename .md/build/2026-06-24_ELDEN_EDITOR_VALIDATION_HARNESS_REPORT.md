# S17 RHI Validation Harness Report

- Date: 2026-06-24 05:55:06 +09:00
- Repo: `C:\Users\tnest\Desktop\Winters`
- Configuration: `Debug`
- Platform: `x64`
- Overall: `PASS`

## Steps

- `PASS` `git diff --check` exit=`0` seconds=`0.33`
- `PASS` `Client/Public and Shared concrete graphics audit` exit=`1` seconds=`0.39`
  - Notes: No matches.
- `PASS` `Focused common RHI public header audit` exit=`1` seconds=`0.04`
  - Notes: No matches.
- `PASS` `CMake/Ninja S17 targets` exit=`0` seconds=`4.63`
- `PASS` `MSBuild Winters.sln` exit=`0` seconds=`37.86`
- `SKIP` `Runtime smoke` exit=`0` seconds=`0`
  - Notes: Skipped by parameter.

## Output Tail

### git diff --check

```text
git : warning: in the working copy of '.md/collab/ACTIVE_WORK_PACKETS.md', LF will be replaced by CRLF the next time Gi
t touches it
At C:\Users\tnest\Desktop\Winters\Tools\Harness\Run-S17RhiValidation.ps1:329 char:44
+     Invoke-NativeStep "git diff --check" { git diff --check }
+                                            ~~~~~~~~~~~~~~~~
    + CategoryInfo          : NotSpecified: (warning: in the... Git touches it:String) [], RemoteException
    + FullyQualifiedErrorId : NativeCommandError

warning: in the working copy of 'Client/Private/Manager/Jungle_Manager.cpp', LF will be replaced by CRLF the next time
Git touches it
warning: in the working copy of 'Client/Private/Manager/Structure_Manager.cpp', LF will be replaced by CRLF the next ti
me Git touches it
warning: in the working copy of 'Client/Private/Scene/Scene_InGameRender.cpp', LF will be replaced by CRLF the next tim
e Git touches it
warning: in the working copy of 'Client/Public/Manager/AmbientProp_Manager.h', LF will be replaced by CRLF the next tim
e Git touches it
warning: in the working copy of 'Client/Public/Manager/Jungle_Manager.h', LF will be replaced by CRLF the next time Git
 touches it
warning: in the working copy of 'Client/Public/Manager/Minion_Manager.h', LF will be replaced by CRLF the next time Git
 touches it
warning: in the working copy of 'Client/Public/Manager/Structure_Manager.h', LF will be replaced by CRLF the next time
Git touches it
```

### CMake/Ninja S17 targets

```text
[0/2] Re-checking globbed directories...
cmd.exe : -- GLOB mismatch!
At C:\Users\tnest\Desktop\Winters\Tools\Harness\Run-S17RhiValidation.ps1:109 char:20
+             $raw = & cmd.exe /c $CommandLine 2>&1
+                    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    + CategoryInfo          : NotSpecified: (-- GLOB mismatch!:String) [], RemoteException
    + FullyQualifiedErrorId : NativeCommandError

The following files were added:
  +C:/Users/tnest/Desktop/Winters/.md/build/2026-06-24_ELDEN_EDITOR_VALIDATION_PANELS_REPORT.md
  +C:/Users/tnest/Desktop/Winters/.md/build/2026-06-24_S17_LOL_RUNTIME_SNAPSHOT_BRIDGE_HARNESS_REPORT.md
  +C:/Users/tnest/Desktop/Winters/.md/build/2026-06-24_S17_LOL_RUNTIME_SNAPSHOT_BRIDGE_REPORT.md
  +C:/Users/tnest/Desktop/Winters/.md/collab/work-packets/2026-06-24_elden_editor_validation_panels.md
  +C:/Users/tnest/Desktop/Winters/.md/collab/work-packets/2026-06-24_s17_scene_object_snapshot_bridge.md
  +C:/Users/tnest/Desktop/Winters/.md/plan/EldenRingEditor/10_2026-06-24_EDITOR_VALIDATION_PANELS_AND_DESKTOP_HANDOFF_D
ESIGN.md
[1/2] Re-running CMake...
-- Configuring done (0.7s)
-- Generating done (0.5s)
-- Build files have been written to: C:/Users/tnest/Desktop/Winters/out/build/msvc-ninja
[0/4] Re-checking globbed directories...
ninja: no work to do.
```

### MSBuild Winters.sln

```text
msbuild 버전 18.7.8+1ac568fee(.NET Framework용)

빌드했습니다.
    경고 91개
    오류 0개

경과 시간: 00:00:34.88
```
