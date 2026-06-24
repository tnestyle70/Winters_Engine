# S17 RHI Validation Harness Report

- Date: 2026-06-24 05:56:26 +09:00
- Repo: `C:\Users\tnest\Desktop\Winters`
- Configuration: `Debug`
- Platform: `x64`
- Overall: `PASS`

## Steps

- `PASS` `git diff --check` exit=`0` seconds=`0.21`
- `PASS` `Client/Public and Shared concrete graphics audit` exit=`1` seconds=`0.06`
  - Notes: No matches.
- `PASS` `Focused common RHI public header audit` exit=`1` seconds=`0.05`
  - Notes: No matches.
- `PASS` `CMake/Ninja S17 targets` exit=`0` seconds=`5.43`
- `PASS` `MSBuild Winters.sln` exit=`0` seconds=`27.02`
- `PASS` `Runtime smoke` exit=`0` seconds=`40.31`

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

warning: in the working copy of 'Client/Private/Manager/AmbientProp_Manager.cpp', LF will be replaced by CRLF the next
time Git touches it
warning: in the working copy of 'Client/Private/Manager/Jungle_Manager.cpp', LF will be replaced by CRLF the next time
Git touches it
warning: in the working copy of 'Client/Private/Manager/Minion_Manager.cpp', LF will be replaced by CRLF the next time
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
  +C:/Users/tnest/Desktop/Winters/.md/build/2026-06-24_ELDEN_EDITOR_VALIDATION_HARNESS_REPORT.md
[1/2] Re-running CMake...
-- Configuring done (0.9s)
-- Generating done (0.6s)
-- Build files have been written to: C:/Users/tnest/Desktop/Winters/out/build/msvc-ninja
[0/4] Re-checking globbed directories...
ninja: no work to do.
```

### MSBuild Winters.sln

```text
msbuild 버전 18.7.8+1ac568fee(.NET Framework용)

빌드했습니다.
    경고 78개
    오류 0개

경과 시간: 00:00:23.46
```

### Runtime smoke

```text

Name                    AliveAfterSeconds ExitCode Cleanup
----                    ----------------- -------- -------
WintersElden_probe_dx12              True          killed
WintersElden_probe_dx11              True          killed
WintersEldenRingEditor               True          killed
WintersGame                          True          killed
```
