[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [switch]$SkipFullPipeline,
    [switch]$AllowKnownYoneEContractGap,
    [string]$ReportPath = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
Set-Location $RepoRoot

if ([string]::IsNullOrWhiteSpace($ReportPath)) {
    $stamp = Get-Date -Format "yyyy-MM-dd_HHmmss"
    $ReportPath = Join-Path $RepoRoot ".md\build\${stamp}_BOT_AI_VALIDATION_HARNESS_REPORT.md"
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

    $lines = @($Text -split "`r?`n" | ForEach-Object { $_.TrimEnd() })
    if ($lines.Count -le $LineCount) {
        return ($lines -join "`n").TrimEnd()
    }

    return (($lines | Select-Object -Last $LineCount) -join "`n").TrimEnd()
}

function Invoke-CmdStep {
    param(
        [string]$Name,
        [string]$CommandLine
    )

    Write-Host "[BotAIHarness] $Name"
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

function Invoke-RgNoMatchStep {
    param(
        [string]$Name,
        [string[]]$Paths,
        [string]$Pattern,
        [string]$FailNote
    )

    Write-Host "[BotAIHarness] $Name"
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $raw = & rg -n $Pattern @Paths 2>&1
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
        Add-StepResult $Name "FAIL" $exitCode $sw.Elapsed.TotalSeconds $outputText $FailNote
        throw "$Name found forbidden matches."
    }

    Add-StepResult $Name "FAIL" $exitCode $sw.Elapsed.TotalSeconds $outputText "rg failed."
    throw "$Name failed with rg exit code $exitCode"
}

function Invoke-YoneEContractAudit {
    Write-Host "[BotAIHarness] Yone E stage-2 contract audit"
    $sw = [System.Diagnostics.Stopwatch]::StartNew()

    $aiPath = Join-Path $RepoRoot "Shared\GameSim\Systems\ChampionAI\ChampionAISystem.cpp"
    $commandPath = Join-Path $RepoRoot "Shared\GameSim\Systems\CommandExecutor\CommandExecutor.cpp"
    $dataPath = Join-Path $RepoRoot "Data\LoL\ServerPrivate\Gameplay\SkillGameplayDefs.json"

    $aiText = Get-Content $aiPath -Raw
    $commandText = Get-Content $commandPath -Raw
    $json = Get-Content $dataPath -Raw | ConvertFrom-Json
    $skill = $json.skills | Where-Object { $_.key -eq "skill.yone.e" } | Select-Object -First 1

    $issues = New-Object System.Collections.Generic.List[string]
    if (-not $skill) {
        $issues.Add("skill.yone.e was not found in SkillGameplayDefs.json.") | Out-Null
    }

    $aiRequestsStage2 =
        $aiText.Contains("yone-e-soul-return") -and
        $aiText.Contains("cmd.itemId = 2u")
    if (-not $aiRequestsStage2) {
        $issues.Add("Yone AI does not clearly emit itemId=2 for E return.") | Out-Null
    }

    if (-not $commandText.Contains("bRequestedStage2 = cmd.itemId == 2u")) {
        $issues.Add("CommandExecutor stage-2 itemId contract was not found.") | Out-Null
    }

    if ($skill) {
        $stageCount = [int]$skill.stage.count
        $stageWindow = [double]$skill.stage.windowSeconds
        if ($stageCount -lt 2) {
            $issues.Add("skill.yone.e stage.count is $stageCount; expected >= 2 for itemId=2 recast.") | Out-Null
        }
        if ($stageWindow -le 0.0) {
            $issues.Add("skill.yone.e stage.windowSeconds is $stageWindow; expected > 0 for stage-2 recast.") | Out-Null
        }
    }

    $sw.Stop()
    $output = if ($issues.Count -gt 0) { ($issues -join "`n") } else { "Yone E stage-2 contract is consistent." }

    if ($issues.Count -eq 0) {
        Add-StepResult "Yone E stage-2 contract audit" "PASS" 0 $sw.Elapsed.TotalSeconds $output
        return
    }

    if ($AllowKnownYoneEContractGap) {
        Add-StepResult "Yone E stage-2 contract audit" "WARN" 0 $sw.Elapsed.TotalSeconds $output "Known gap allowed for collaboration bootstrap only."
        return
    }

    Add-StepResult "Yone E stage-2 contract audit" "FAIL" 1 $sw.Elapsed.TotalSeconds $output
    throw "Yone E stage-2 contract audit failed."
}

function Write-Report {
    $hasFail = @($Results | Where-Object { $_.Status -eq "FAIL" }).Count -gt 0
    $hasWarn = @($Results | Where-Object { $_.Status -eq "WARN" }).Count -gt 0
    $overall = if ($hasFail) { "FAIL" } elseif ($hasWarn) { "WARN" } else { "PASS" }

    $lines = New-Object System.Collections.Generic.List[string]
    $lines.Add("# Bot AI Validation Harness Report") | Out-Null
    $lines.Add("") | Out-Null
    $lines.Add("- Date: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss zzz')") | Out-Null
    $lines.Add("- Repo: $RepoRoot") | Out-Null
    $lines.Add("- Configuration: $Configuration") | Out-Null
    $lines.Add("- Overall: $overall") | Out-Null
    $lines.Add("") | Out-Null
    $lines.Add("## Steps") | Out-Null
    $lines.Add("") | Out-Null

    foreach ($result in $Results) {
        $lines.Add("- $($result.Status) $($result.Name) exit=$($result.ExitCode) seconds=$($result.Seconds)") | Out-Null
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

    $lines | Set-Content -Path $ReportPath -Encoding UTF8
    Write-Host "[BotAIHarness] Report: $ReportPath"
    Write-Host "[BotAIHarness] Overall: $overall"
}

try {
    Invoke-CmdStep "git diff --check" "git diff --check"
    Invoke-RgNoMatchStep `
        "ChampionAI dependency boundary audit" `
        @("Shared/GameSim/Systems/ChampionAI", "Shared/GameSim/Components/ChampionAIComponent.h") `
        "Client/|Renderer|ImGui|d3d11|d3d12|ID3D11|ID3D12|DX11Shader|DX11Pipeline" `
        "Champion AI Shared/GameSim code must not depend on client/render/UI/DX concrete symbols."
    Invoke-YoneEContractAudit

    if (-not $SkipFullPipeline) {
        Invoke-CmdStep `
            "LoL data-driven pipeline and SimLab regression" `
            "powershell -NoProfile -ExecutionPolicy Bypass -File Tools\LoLData\Verify-LoLDataDrivenPipeline.ps1 -Configuration $Configuration"
    }
    else {
        Add-StepResult "LoL data-driven pipeline and SimLab regression" "WARN" 0 0 "" "Skipped by -SkipFullPipeline."
    }
}
catch {
    $Failure = $_
}
finally {
    Write-Report
}

if ($Failure) {
    throw $Failure
}
