# S17 RHI Validation Harness Report

- Date: 2026-06-24 11:08:33 +09:00
- Repo: `C:\Users\tnest\Desktop\Winters`
- Configuration: `Debug`
- Platform: `x64`
- Overall: `PASS`

## Steps

- `PASS` `git diff --check` exit=`0` seconds=`0.28`
- `PASS` `Client/Public and Shared concrete graphics audit` exit=`1` seconds=`0.18`
  - Notes: No matches.
- `PASS` `Focused common RHI public header audit` exit=`1` seconds=`0.07`
  - Notes: No matches.
- `PASS` `CMake/Ninja S17 targets` exit=`0` seconds=`11.65`
- `PASS` `MSBuild Winters.sln` exit=`0` seconds=`57.59`
- `PASS` `Runtime smoke` exit=`0` seconds=`50.76`

## Output Tail

### git diff --check

```text
git : warning: in the working copy of '.md/collab/ACTIVE_WORK_PACKETS.md', LF will be replaced by CRLF the next time Gi
t touches it
At C:\Users\tnest\Desktop\Winters\Tools\Harness\Run-S17RhiValidation.ps1:330 char:44
+     Invoke-NativeStep "git diff --check" { git diff --check }
+                                            ~~~~~~~~~~~~~~~~
    + CategoryInfo          : NotSpecified: (warning: in the... Git touches it:String) [], RemoteException
    + FullyQualifiedErrorId : NativeCommandError

warning: in the working copy of 'Client/Private/Scene/Scene_InGameRender.cpp', LF will be replaced by CRLF the next tim
e Git touches it
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
  +C:/Users/tnest/Desktop/Winters/.md/build/2026-06-24_S18_POST_MERGE_RHI_VALIDATION_HARNESS_REPORT.md
  +C:/Users/tnest/Desktop/Winters/.md/build/2026-06-24_YONE_Q_MOVE_LOCK_INPUT_ANIMATION_REPORT.md
  +C:/Users/tnest/Desktop/Winters/.md/plan/2026-06-24_YONE_Q_MOVE_LOCK_INPUT_ANIMATION_PLAN.md
[1/2] Re-running CMake...
-- Configuring done (1.4s)
-- Generating done (0.9s)
-- Build files have been written to: C:/Users/tnest/Desktop/Winters/out/build/msvc-ninja
[0/4] Re-checking globbed directories...
[1/5] Linking CXX executable C:\Users\tnest\Desktop\Winters\EldenRingClient\Bin\Debug\WintersElden.exe; Copy WintersEngine runtime DLL to EldenRingClient output; Copy third-party runtime DLLs to EldenRingClient output; Copy shaders to EldenRingClient output
[2/5] Linking CXX executable C:\Users\tnest\Desktop\Winters\EldenRingEditor\Bin\Debug\WintersEldenRingEditor.exe; Copy WintersEngine runtime DLL to EldenRingEditor output; Copy third-party runtime DLLs to EldenRingEditor output; Copy shaders to EldenRingEditor output
```

### MSBuild Winters.sln

```text
msbuild 버전 18.7.8+1ac568fee(.NET Framework용)

빌드했습니다.
    경고 149개
    오류 0개

경과 시간: 00:00:52.56
```

### Runtime smoke

```text

Name                       AliveAfterSeconds ExitCode Cleanup
----                       ----------------- -------- -------
WintersElden_probe_dx12                 True          killed
WintersElden_probe_dx11                 True          killed
WintersEldenRingEditor                  True          killed
WintersGame                             True          killed
WintersGame_rhi_scene_only              True          killed
```
