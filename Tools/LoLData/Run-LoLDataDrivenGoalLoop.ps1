[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [int]$MaxIterations = 1,
    [switch]$SkipFullVerify
)

$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

function Invoke-Checked {
    param(
        [string]$Name,
        [scriptblock]$Command
    )

    Write-Host "[LoLGoalLoop] $Name"
    & $Command
    if ($LASTEXITCODE -ne 0) {
        throw "$Name failed with exit code $LASTEXITCODE"
    }
}

Push-Location $Root
try {
    for ($iteration = 1; $iteration -le $MaxIterations; ++$iteration) {
        Write-Host "[LoLGoalLoop] Iteration $iteration / $MaxIterations"

        $statusJson = powershell -ExecutionPolicy Bypass -File Tools/LoLData/Get-LoLDataDrivenGoalStatus.ps1
        if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne 2) {
            throw "Goal status failed with exit code $LASTEXITCODE"
        }

        $status = ($statusJson | Out-String) | ConvertFrom-Json
        if ($status.complete) {
            Write-Host "[LoLGoalLoop] COMPLETE before verification"
            if (-not $SkipFullVerify) {
                Invoke-Checked "Full data-driven verification" {
                    powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1 -Configuration $Configuration
                }
            }
            Write-Host "[LoLGoalLoop] COMPLETE"
            exit 0
        }

        Write-Host "[LoLGoalLoop] NEXT phase=$($status.nextGoal.phase) key=$($status.nextGoal.key) remaining=$($status.nextGoal.remaining)"
        Write-Host "[LoLGoalLoop] ACTION $($status.nextGoal.nextFocus)"

        if (-not $SkipFullVerify) {
            Invoke-Checked "Full data-driven verification" {
                powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1 -Configuration $Configuration
            }
        }

        if ($iteration -lt $MaxIterations) {
            Write-Host "[LoLGoalLoop] Code migration slice must be applied before the next iteration can reduce counts."
        }
    }
}
finally {
    Pop-Location
}

Write-Host "[LoLGoalLoop] INCOMPLETE"
exit 2
