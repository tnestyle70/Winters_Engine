# S17 RHI Validation Harness Report

- Date: 2026-06-24 05:40:59 +09:00
- Repo: `C:\Users\tnest\Desktop\Winters`
- Configuration: `Debug`
- Platform: `x64`
- Overall: `PASS`

## Steps

- `PASS` `git diff --check` exit=`0` seconds=`0.08`
- `PASS` `Client/Public and Shared concrete graphics audit` exit=`1` seconds=`0.05`
  - Notes: No matches.
- `PASS` `Focused common RHI public header audit` exit=`1` seconds=`0.04`
  - Notes: No matches.
- `PASS` `CMake/Ninja S17 targets` exit=`0` seconds=`3.92`
- `PASS` `MSBuild Winters.sln` exit=`0` seconds=`131.81`
- `PASS` `Runtime smoke` exit=`0` seconds=`40.42`

## Output Tail

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
  +C:/Users/tnest/Desktop/Winters/.md/build/2026-06-24_S17_RHI_VALIDATION_HARNESS_REPORT.md
[1/2] Re-running CMake...
-- Configuring done (0.6s)
-- Generating done (0.3s)
-- Build files have been written to: C:/Users/tnest/Desktop/Winters/out/build/msvc-ninja
[0/4] Re-checking globbed directories...
ninja: no work to do.
```

### MSBuild Winters.sln

```text
msbuild 버전 18.7.8+1ac568fee(.NET Framework용)

빌드했습니다.
    경고 265개
    오류 0개

경과 시간: 00:02:09.13
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
