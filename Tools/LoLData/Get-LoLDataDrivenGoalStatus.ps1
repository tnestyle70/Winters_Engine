[CmdletBinding()]
param(
    [string]$Root = "",
    [string]$CriteriaPath = "",
    [string]$OutputDir = "",
    [switch]$FailWhenIncomplete,
    [switch]$NoWrite
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
        if ($null -eq $current -or -not ($current.PSObject.Properties.Name -contains $part)) {
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

    $rawCurrent = Get-NestedValue -Object $Audit -Path $Goal.auditPath -Fallback $null
    if ($null -eq $rawCurrent) {
        throw "goal audit path not found: $($Goal.auditPath)"
    }
    $current = [int]$rawCurrent
    $comparison = [string]$Goal.comparison
    $target = [int]$Goal.target
    $phase = [string]$Goal.phase

    $achieved = switch ($comparison) {
        "max" { $current -le $target }
        "min" { $current -ge $target }
        "equal" { $current -eq $target }
        default { throw "unknown goal comparison: $comparison" }
    }

    $remaining = switch ($comparison) {
        "max" { [Math]::Max(0, $current - $target) }
        "min" { [Math]::Max(0, $target - $current) }
        "equal" { [Math]::Abs($target - $current) }
    }

    return [ordered]@{
        key = [string]$Goal.key
        phase = $phase
        phaseRank = Get-PhaseRank -Phase $phase -PhaseOrder $PhaseOrder
        name = [string]$Goal.name
        currentCount = $current
        comparison = $comparison
        target = $target
        achieved = $achieved
        remaining = $remaining
        nextFocus = [string]$Goal.nextFocus
    }
}

$auditPath = Join-Path $OutputDir "LOL_DATA_DRIVEN_AUDIT.json"
$statusPath = Join-Path $OutputDir "LOL_DATA_DRIVEN_GOAL_STATUS.json"
$markdownPath = Join-Path $OutputDir "LOL_DATA_DRIVEN_GOAL_STATUS.md"

if ($NoWrite) {
    $auditJson = & powershell -ExecutionPolicy Bypass -File (Join-Path $Root "Tools\LoLData\Collect-LoLLegacyDataAudit.ps1") -Root $Root
    if ($LASTEXITCODE -ne 0) {
        throw "legacy data audit failed with exit code $LASTEXITCODE"
    }
    $audit = $auditJson | ConvertFrom-Json
}
else {
    New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
    & powershell -ExecutionPolicy Bypass -File (Join-Path $Root "Tools\LoLData\Collect-LoLLegacyDataAudit.ps1") `
        -Root $Root `
        -OutputPath $auditPath | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "legacy data audit failed with exit code $LASTEXITCODE"
    }
    $audit = Get-Content -Raw -Encoding UTF8 -Path $auditPath | ConvertFrom-Json
}

$criteria = Get-Content -Raw -Encoding UTF8 -Path $CriteriaPath | ConvertFrom-Json

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
    auditPath = if ($NoWrite) { $null } else { $auditPath }
    statusPath = if ($NoWrite) { $null } else { $statusPath }
    markdownPath = if ($NoWrite) { $null } else { $markdownPath }
    fullVerificationCommand = $criteria.fullVerificationCommand
    completionDefinition = $criteria.completionDefinition
}

if (-not $NoWrite) {
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
        $lines += "- [$mark] $($goal.phase) $($goal.key): current=$($goal.currentCount), comparison=$($goal.comparison), target=$($goal.target)"
    }
    $lines += ""
    $lines += "## Gate"
    $lines += ""
    $lines += "- $($status.fullVerificationCommand)"
    $lines += "- $($status.completionDefinition)"

    $lines | Set-Content -Encoding UTF8 -Path $markdownPath
}

Write-Output ($status | ConvertTo-Json -Depth 8)

if ($FailWhenIncomplete -and -not $status.complete) {
    exit 2
}
