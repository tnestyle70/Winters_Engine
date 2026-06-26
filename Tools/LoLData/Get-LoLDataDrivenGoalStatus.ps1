[CmdletBinding()]
param(
    [string]$Root = "",
    [string]$CriteriaPath = "",
    [string]$OutputDir = "",
    [switch]$FailWhenIncomplete
)

$ErrorActionPreference = "Stop"

if ($Root.Length -eq 0) {
    $Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
}
if ($CriteriaPath.Length -eq 0) {
    $CriteriaPath = Join-Path $Root "Data\LoL\SharedContract\DataDrivenGoalCriteria.json"
}
if ($OutputDir.Length -eq 0) {
    $date = Get-Date -Format "MM-dd"
    $OutputDir = Join-Path $Root ".md\TODO\$date"
}

function Get-NestedValue {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Object,
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [object]$Fallback = $null
    )

    $current = $Object
    foreach ($part in $Path.Split(".")) {
        if ($null -eq $current) {
            return $Fallback
        }
        if (-not ($current.PSObject.Properties.Name -contains $part)) {
            return $Fallback
        }
        $current = $current.$part
    }
    return $current
}

function Get-PhaseRank {
    param(
        [string]$Phase,
        [object[]]$PhaseOrder
    )

    for ($index = 0; $index -lt $PhaseOrder.Count; ++$index) {
        if ([string]$PhaseOrder[$index] -eq $Phase) {
            return $index
        }
    }

    return 9999
}

function New-GoalStatus {
    param(
        [object]$Goal,
        [object]$Audit,
        [object[]]$PhaseOrder
    )

    $rawCurrent = Get-NestedValue -Object $Audit -Path $Goal.auditPath -Fallback 999999
    $current = [int]$rawCurrent
    $target = [int]$Goal.targetMax
    $phase = [string]$Goal.phase

    return [ordered]@{
        key = [string]$Goal.key
        phase = $phase
        phaseRank = Get-PhaseRank -Phase $phase -PhaseOrder $PhaseOrder
        name = [string]$Goal.name
        currentCount = $current
        targetMax = $target
        achieved = ($current -le $target)
        remaining = [Math]::Max(0, $current - $target)
        nextFocus = [string]$Goal.nextFocus
    }
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$auditPath = Join-Path $OutputDir "LOL_DATA_DRIVEN_AUDIT.json"
$statusPath = Join-Path $OutputDir "LOL_DATA_DRIVEN_GOAL_STATUS.json"
$markdownPath = Join-Path $OutputDir "LOL_DATA_DRIVEN_GOAL_STATUS.md"

powershell -ExecutionPolicy Bypass -File (Join-Path $Root "Tools\LoLData\Collect-LoLLegacyDataAudit.ps1") `
    -Root $Root `
    -OutputPath $auditPath | Out-Null

$criteria = Get-Content -Raw -Encoding UTF8 -Path $CriteriaPath | ConvertFrom-Json
$audit = Get-Content -Raw -Encoding UTF8 -Path $auditPath | ConvertFrom-Json

$manifestPath = Join-Path $Root "Data\LoL\SharedContract\DefinitionManifest.json"
$buildHash = ""
if (Test-Path $manifestPath) {
    $manifest = Get-Content -Raw -Encoding UTF8 -Path $manifestPath | ConvertFrom-Json
    if ($manifest.PSObject.Properties.Name -contains "buildHash") {
        $buildHash = $manifest.buildHash
    }
}

$phaseOrder = @($criteria.phaseOrder)
$goals = @()
foreach ($goal in $criteria.goals) {
    $goals += New-GoalStatus -Goal $goal -Audit $audit -PhaseOrder $phaseOrder
}

$unfinished = @($goals | Where-Object { -not $_.achieved })
$nextGoal = $null
if ($unfinished.Count -gt 0) {
    $nextGoal = $unfinished |
        Sort-Object `
            @{Expression = { $_.phaseRank }; Ascending = $true },
            @{Expression = { $_.remaining }; Descending = $true } |
        Select-Object -First 1
}

$status = [ordered]@{
    generatedAtUtc = (Get-Date).ToUniversalTime().ToString("o")
    root = (Resolve-Path $Root).Path
    sourceDocument = $criteria.sourceDocument
    buildHash = $buildHash
    complete = ($unfinished.Count -eq 0)
    completedGoalCount = @($goals | Where-Object { $_.achieved }).Count
    totalGoalCount = @($goals).Count
    nextGoal = $nextGoal
    goals = $goals
    auditPath = $auditPath
    statusPath = $statusPath
    markdownPath = $markdownPath
    fullVerificationCommand = $criteria.fullVerificationCommand
    completionDefinition = $criteria.completionDefinition
}

$status | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 -Path $statusPath

$lines = @()
$lines += "# LoL Data-Driven Goal Status"
$lines += ""
$lines += "generatedAtUtc: $($status.generatedAtUtc)"
$lines += "buildHash: $($status.buildHash)"
$lines += "complete: $($status.complete)"
$lines += "completedGoalCount: $($status.completedGoalCount) / $($status.totalGoalCount)"
$lines += ""
if ($null -ne $nextGoal) {
    $lines += "## Next Focus"
    $lines += ""
    $lines += "- phase: $($nextGoal.phase)"
    $lines += "- key: $($nextGoal.key)"
    $lines += "- remaining: $($nextGoal.remaining)"
    $lines += "- action: $($nextGoal.nextFocus)"
    $lines += ""
}
$lines += "## Goals"
$lines += ""
foreach ($goal in $goals) {
    $mark = if ($goal.achieved) { "PASS" } else { "TODO" }
    $lines += "- [$mark] $($goal.phase) $($goal.key): current=$($goal.currentCount), targetMax=$($goal.targetMax)"
}
$lines += ""
$lines += "## Gate"
$lines += ""
$lines += "- $($status.fullVerificationCommand)"
$lines += "- $($status.completionDefinition)"

$lines | Set-Content -Encoding UTF8 -Path $markdownPath

Write-Output ($status | ConvertTo-Json -Depth 8)

if ($FailWhenIncomplete -and -not $status.complete) {
    exit 2
}
