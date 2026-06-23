[CmdletBinding()]
param(
    [string]$Configuration = "Debug",
    [string]$Platform = "x64",
    [int]$SmokeSeconds = 8,
    [switch]$SkipCMake,
    [switch]$SkipMsBuild,
    [switch]$SkipRuntimeSmoke,
    [string]$ReportPath = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
Set-Location $RepoRoot

if ([string]::IsNullOrWhiteSpace($ReportPath)) {
    $stamp = Get-Date -Format "yyyy-MM-dd_HHmmss"
    $ReportPath = Join-Path $RepoRoot ".md\build\${stamp}_S17_RHI_VALIDATION_HARNESS_REPORT.md"
}

$ReportDir = Split-Path -Parent $ReportPath
New-Item -ItemType Directory -Force -Path $ReportDir | Out-Null

$Results = New-Object System.Collections.Generic.List[object]
$Failure = $null

function Add-StepResult {
    param(
        [string]$Name,
        [string]$Status,
        [int]$ExitCode,
        [double]$Seconds,
        [string]$Output,
        [string]$Notes = ""
    )

    $Results.Add([pscustomobject]@{
        Name = $Name
        Status = $Status
        ExitCode = $ExitCode
        Seconds = [math]::Round($Seconds, 2)
        Output = $Output
        Notes = $Notes
    }) | Out-Null
}

function Get-OutputTail {
    param([string]$Text, [int]$LineCount = 80)

    if ([string]::IsNullOrWhiteSpace($Text)) {
        return ""
    }

    $lines = $Text -split "`r?`n" | ForEach-Object { $_.TrimEnd() }
    if ($lines.Count -le $LineCount) {
        return ($lines -join "`n").TrimEnd()
    }

    return (($lines | Select-Object -Last $LineCount) -join "`n").TrimEnd()
}

function Find-VsDevCmd {
    $candidates = @(
        "$env:ProgramFiles\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat",
        "$env:ProgramFiles\Microsoft Visual Studio\18\Enterprise\Common7\Tools\VsDevCmd.bat",
        "$env:ProgramFiles\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat",
        "$env:ProgramFiles\Microsoft Visual Studio\17\Community\Common7\Tools\VsDevCmd.bat",
        "$env:ProgramFiles\Microsoft Visual Studio\17\Enterprise\Common7\Tools\VsDevCmd.bat",
        "$env:ProgramFiles\Microsoft Visual Studio\17\BuildTools\Common7\Tools\VsDevCmd.bat"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $installPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if (-not [string]::IsNullOrWhiteSpace($installPath)) {
            $path = Join-Path $installPath "Common7\Tools\VsDevCmd.bat"
            if (Test-Path $path) {
                return $path
            }
        }
    }

    throw "VsDevCmd.bat was not found."
}

function Invoke-CmdStep {
    param(
        [string]$Name,
        [string]$CommandLine
    )

    Write-Host "[S17Harness] $Name"
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $outputText = ""
    $exitCode = 0

    try {
        $previousErrorActionPreference = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        try {
            $raw = & cmd.exe /c $CommandLine 2>&1
        }
        finally {
            $ErrorActionPreference = $previousErrorActionPreference
        }
        $exitCode = $LASTEXITCODE
        $outputText = ($raw | Out-String).TrimEnd()
        $sw.Stop()

        if ($exitCode -ne 0) {
            Add-StepResult $Name "FAIL" $exitCode $sw.Elapsed.TotalSeconds $outputText
            throw "$Name failed with exit code $exitCode"
        }

        Add-StepResult $Name "PASS" $exitCode $sw.Elapsed.TotalSeconds $outputText
    }
    catch {
        if ($Results.Count -eq 0 -or $Results[$Results.Count - 1].Name -ne $Name) {
            $sw.Stop()
            Add-StepResult $Name "FAIL" $exitCode $sw.Elapsed.TotalSeconds $outputText $_.Exception.Message
        }
        throw
    }
}

function Invoke-NativeStep {
    param(
        [string]$Name,
        [scriptblock]$Action
    )

    Write-Host "[S17Harness] $Name"
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $outputText = ""
    $exitCode = 0

    try {
        $previousErrorActionPreference = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        try {
            $raw = & $Action 2>&1
        }
        finally {
            $ErrorActionPreference = $previousErrorActionPreference
        }
        $exitCode = $LASTEXITCODE
        $outputText = ($raw | Out-String).TrimEnd()
        $sw.Stop()

        if ($exitCode -ne 0) {
            Add-StepResult $Name "FAIL" $exitCode $sw.Elapsed.TotalSeconds $outputText
            throw "$Name failed with exit code $exitCode"
        }

        Add-StepResult $Name "PASS" $exitCode $sw.Elapsed.TotalSeconds $outputText
    }
    catch {
        if ($Results.Count -eq 0 -or $Results[$Results.Count - 1].Name -ne $Name) {
            $sw.Stop()
            Add-StepResult $Name "FAIL" $exitCode $sw.Elapsed.TotalSeconds $outputText $_.Exception.Message
        }
        throw
    }
}

function Invoke-RgNoMatchStep {
    param(
        [string]$Name,
        [string[]]$Paths
    )

    Write-Host "[S17Harness] $Name"
    $pattern = "ID3D11|ID3D12|d3d11\.h|d3d12\.h|DX11Shader|DX11Pipeline"
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $raw = & rg -n $pattern @Paths 2>&1
    }
    finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
    $exitCode = $LASTEXITCODE
    $outputText = ($raw | Out-String).TrimEnd()
    $sw.Stop()

    if ($exitCode -eq 1) {
        Add-StepResult $Name "PASS" $exitCode $sw.Elapsed.TotalSeconds "" "No matches."
        return
    }

    if ($exitCode -eq 0) {
        Add-StepResult $Name "FAIL" $exitCode $sw.Elapsed.TotalSeconds $outputText "Forbidden concrete graphics symbols found."
        throw "$Name found forbidden concrete graphics symbols."
    }

    Add-StepResult $Name "FAIL" $exitCode $sw.Elapsed.TotalSeconds $outputText "rg failed."
    throw "$Name failed with rg exit code $exitCode"
}

function Invoke-RuntimeSmoke {
    Write-Host "[S17Harness] Runtime smoke"
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $rows = New-Object System.Collections.Generic.List[object]
    $failed = $false

    $tests = @(
        @{ Name = "WintersElden_probe_dx12"; Exe = "EldenRingClient\Bin\$Configuration\WintersElden.exe"; Args = @("--scene=probe"); Cwd = "EldenRingClient\Bin\$Configuration" },
        @{ Name = "WintersElden_probe_dx11"; Exe = "EldenRingClient\Bin\$Configuration\WintersElden.exe"; Args = @("--scene=probe", "--rhi=dx11"); Cwd = "EldenRingClient\Bin\$Configuration" },
        @{ Name = "WintersEldenRingEditor"; Exe = "EldenRingEditor\Bin\$Configuration\WintersEldenRingEditor.exe"; Args = @(); Cwd = "EldenRingEditor\Bin\$Configuration" },
        @{ Name = "WintersGame"; Exe = "Client\Bin\$Configuration\WintersGame.exe"; Args = @(); Cwd = "Client\Bin\$Configuration" }
    )

    foreach ($test in $tests) {
        $exePath = Join-Path $RepoRoot $test.Exe
        $cwdPath = Join-Path $RepoRoot $test.Cwd
        if (-not (Test-Path $exePath)) {
            $rows.Add([pscustomobject]@{
                Name = $test.Name
                AliveAfterSeconds = $false
                ExitCode = "missing"
                Cleanup = "none"
            }) | Out-Null
            $failed = $true
            continue
        }

        if ($test.Args.Count -gt 0) {
            $process = Start-Process -FilePath $exePath -ArgumentList $test.Args -WorkingDirectory $cwdPath -PassThru -WindowStyle Hidden
        }
        else {
            $process = Start-Process -FilePath $exePath -WorkingDirectory $cwdPath -PassThru -WindowStyle Hidden
        }

        Start-Sleep -Seconds $SmokeSeconds
        $alive = -not $process.HasExited
        $exitCode = ""
        $cleanup = "none"

        if ($alive) {
            $cleanup = "closed"
            [void]$process.CloseMainWindow()
            Start-Sleep -Seconds 2
            if (-not $process.HasExited) {
                Stop-Process -Id $process.Id -Force
                $cleanup = "killed"
            }
        }
        else {
            $exitCode = [string]$process.ExitCode
            $failed = $true
        }

        $rows.Add([pscustomobject]@{
            Name = $test.Name
            AliveAfterSeconds = $alive
            ExitCode = $exitCode
            Cleanup = $cleanup
        }) | Out-Null
    }

    $sw.Stop()
    $outputText = ($rows | Format-Table -AutoSize | Out-String).TrimEnd()
    if ($failed) {
        Add-StepResult "Runtime smoke" "FAIL" 1 $sw.Elapsed.TotalSeconds $outputText
        throw "Runtime smoke failed."
    }

    Add-StepResult "Runtime smoke" "PASS" 0 $sw.Elapsed.TotalSeconds $outputText
}

function Write-HarnessReport {
    param([string]$FailureMessage = "")

    $overall = if ($Results | Where-Object { $_.Status -eq "FAIL" }) { "FAIL" } else { "PASS" }
    $lines = New-Object System.Collections.Generic.List[string]
    $lines.Add("# S17 RHI Validation Harness Report") | Out-Null
    $lines.Add("") | Out-Null
    $lines.Add(("- Date: {0}" -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss zzz'))) | Out-Null
    $lines.Add(('- Repo: `{0}`' -f $RepoRoot)) | Out-Null
    $lines.Add(('- Configuration: `{0}`' -f $Configuration)) | Out-Null
    $lines.Add(('- Platform: `{0}`' -f $Platform)) | Out-Null
    $lines.Add(('- Overall: `{0}`' -f $overall)) | Out-Null
    if (-not [string]::IsNullOrWhiteSpace($FailureMessage)) {
        $lines.Add(('- Failure: `{0}`' -f $FailureMessage)) | Out-Null
    }
    $lines.Add("") | Out-Null
    $lines.Add("## Steps") | Out-Null
    $lines.Add("") | Out-Null

    foreach ($result in $Results) {
        $lines.Add(('- `{0}` `{1}` exit=`{2}` seconds=`{3}`' -f $result.Status, $result.Name, $result.ExitCode, $result.Seconds)) | Out-Null
        if (-not [string]::IsNullOrWhiteSpace($result.Notes)) {
            $lines.Add("  - Notes: $($result.Notes)") | Out-Null
        }
    }

    $lines.Add("") | Out-Null
    $lines.Add("## Output Tail") | Out-Null
    $lines.Add("") | Out-Null

    foreach ($result in $Results) {
        $tail = Get-OutputTail $result.Output
        if ([string]::IsNullOrWhiteSpace($tail)) {
            continue
        }

        $lines.Add("### $($result.Name)") | Out-Null
        $lines.Add("") | Out-Null
        $lines.Add('```text') | Out-Null
        $lines.Add($tail) | Out-Null
        $lines.Add('```') | Out-Null
        $lines.Add("") | Out-Null
    }

    Set-Content -Path $ReportPath -Value $lines -Encoding UTF8
    Write-Host "[S17Harness] Report: $ReportPath"
}

try {
    Invoke-NativeStep "git diff --check" { git diff --check }

    Invoke-RgNoMatchStep "Client/Public and Shared concrete graphics audit" @(
        "Client\Public",
        "Shared"
    )

    Invoke-RgNoMatchStep "Focused common RHI public header audit" @(
        "Engine\Public\Renderer\RenderWorldSnapshot.h",
        "Engine\Public\Renderer\RHISceneRenderer.h",
        "Engine\Public\Renderer\RHIMeshResource.h",
        "Engine\Public\Renderer\RHIMaterialResource.h",
        "Engine\Public\Resource\Mesh.h",
        "Engine\Public\Resource\Model.h"
    )

    $vsDevCmd = Find-VsDevCmd
    if (-not $SkipCMake) {
        $cmakeCmd = '"' + $vsDevCmd + '" -arch=x64 -no_logo && cmake --build out/build/msvc-ninja --config ' + $Configuration + ' --target WintersEngine WintersElden WintersEldenRingEditor'
        Invoke-CmdStep "CMake/Ninja S17 targets" $cmakeCmd
    }
    else {
        Add-StepResult "CMake/Ninja S17 targets" "SKIP" 0 0 "" "Skipped by parameter."
    }

    if (-not $SkipMsBuild) {
        $msbuildCmd = '"' + $vsDevCmd + '" -arch=' + $Platform + ' -no_logo && msbuild Winters.sln /p:Configuration=' + $Configuration + ' /p:Platform=' + $Platform + ' /m /nr:false /v:minimal /clp:ErrorsOnly;Summary'
        Invoke-CmdStep "MSBuild Winters.sln" $msbuildCmd
    }
    else {
        Add-StepResult "MSBuild Winters.sln" "SKIP" 0 0 "" "Skipped by parameter."
    }

    if (-not $SkipRuntimeSmoke) {
        Invoke-RuntimeSmoke
    }
    else {
        Add-StepResult "Runtime smoke" "SKIP" 0 0 "" "Skipped by parameter."
    }
}
catch {
    $Failure = $_.Exception.Message
}
finally {
    Write-HarnessReport $Failure
}

if (-not [string]::IsNullOrWhiteSpace($Failure)) {
    throw $Failure
}
