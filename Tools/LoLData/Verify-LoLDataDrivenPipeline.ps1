param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [switch]$RequireComplete
)

$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

function Invoke-Checked {
    param(
        [string]$Name,
        [scriptblock]$Command
    )

    Write-Host "[LoLDataDriven] $Name"
    & $Command
    if ($LASTEXITCODE -ne 0) {
        throw "$Name failed with exit code $LASTEXITCODE"
    }
}

$VsWhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $VsWhere)) {
    throw "vswhere.exe not found: $VsWhere"
}

$MSBuild = & $VsWhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" |
    Select-Object -First 1
if (-not $MSBuild) {
    throw "MSBuild.exe not found"
}

Push-Location $Root
try {
    Invoke-Checked "Definition pack freshness" {
        python Tools/LoLData/Build-LoLDefinitionPack.py --check
    }
    Invoke-Checked "Champion schema mutation contract" {
        python Tools/LoLData/Test-ChampionGameDataSchema.py
    }
    Invoke-Checked "Legacy ownership audit" {
        powershell -ExecutionPolicy Bypass -File Tools/LoLData/Collect-LoLLegacyDataAudit.ps1
    }
    Invoke-Checked "Draft round-trip" {
        powershell -ExecutionPolicy Bypass -File Tools/LoLData/Test-LoLDataDrivenDraftRoundTrip.ps1 -Root $Root -NoWrite
    }
    Invoke-Checked "Data-driven goal status" {
        if ($RequireComplete) {
            powershell -ExecutionPolicy Bypass -File Tools/LoLData/Get-LoLDataDrivenGoalStatus.ps1 `
                -NoWrite -FailWhenIncomplete
        }
        else {
            powershell -ExecutionPolicy Bypass -File Tools/LoLData/Get-LoLDataDrivenGoalStatus.ps1 -NoWrite
        }
    }
    Invoke-Checked "Raw product asset path audit" {
        powershell -ExecutionPolicy Bypass -File Tools/LoLData/FindRawAssetPaths.ps1
    }
    Invoke-Checked "Client visual timing parity" {
        powershell -ExecutionPolicy Bypass -File Tools/LoLData/Export-LoLChampionVisualTimingSeed.ps1 -Check
    }

    $Projects = @(
        "Shared/GameSim/Include/GameSim.vcxproj",
        "Server/Include/Server.vcxproj",
        "Client/Include/Client.vcxproj",
        "Tools/SimLab/SimLab.vcxproj"
    )
    foreach ($Project in $Projects) {
        Invoke-Checked "Build $Project" {
            & $MSBuild $Project /t:Build "/p:Configuration=$Configuration" /p:Platform=x64 /m:1 /v:minimal
        }
    }

    $SimLab = "Tools/Bin/$Configuration/SimLab.exe"
    Invoke-Checked "SimLab deterministic regression" {
        & $SimLab
    }
    Invoke-Checked "Whitespace validation" {
        git diff --check
    }
}
finally {
    Pop-Location
}

Write-Host "[LoLDataDriven] PASS"
