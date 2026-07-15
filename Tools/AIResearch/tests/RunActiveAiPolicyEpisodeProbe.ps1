param(
    [string]$WintersRoot = "",
    [string]$SimLabPath = "",
    [string]$OutputRoot = "",
    [UInt64]$Seed = 42,
    [int]$TickLimit = 300
)

$ErrorActionPreference = "Stop"

if (-not $WintersRoot) {
    $WintersRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
}
if (-not $SimLabPath) {
    $SimLabPath = Join-Path $WintersRoot "Tools\Bin\Debug\SimLab.exe"
}
if (-not (Test-Path -LiteralPath $SimLabPath -PathType Leaf)) {
    throw "SimLab executable was not found; build Debug x64 first: $SimLabPath"
}
if (-not $OutputRoot) {
    $OutputRoot = Join-Path $env:TEMP "WintersActiveAiPolicyEpisodeProbe"
}
if ($TickLimit -lt 30 -or $TickLimit -gt 18000) {
    throw "TickLimit must be in [30, 18000]."
}

function Assert-LastExitCode([string]$Step) {
    if ($LASTEXITCODE -ne 0) {
        throw "$Step failed with exit code $LASTEXITCODE."
    }
}

$definitionSource = Join-Path $WintersRoot `
    "Data\LoL\SharedContract\DefinitionManifest.json"
$rulesHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $SimLabPath).Hash.ToLowerInvariant()
$definitionHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $definitionSource).Hash.ToLowerInvariant()
$policyPath = Join-Path $OutputRoot "active_canary_retreat_a.wbc"
$secondPolicyPath = Join-Path $OutputRoot "active_canary_retreat_b.wbc"
$runDirectories = @(
    (Join-Path $OutputRoot "run_first"),
    (Join-Path $OutputRoot "run_second")
)

Push-Location $WintersRoot
try {
    New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
    & python -B `
        Tools/AIResearch/tests/BuildActiveMacroCanaryPolicy.py `
        --output $policyPath
    Assert-LastExitCode "Active macro canary policy generation"
    & python -B `
        Tools/AIResearch/tests/BuildActiveMacroCanaryPolicy.py `
        --output $secondPolicyPath
    Assert-LastExitCode "Repeated active macro canary policy generation"
    $policyHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $policyPath).Hash.ToLowerInvariant()
    $secondPolicyHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $secondPolicyPath).Hash.ToLowerInvariant()
    if ($policyHash -ne $secondPolicyHash) {
        throw "Active macro canary policy generation is not deterministic."
    }

    foreach ($runDirectory in $runDirectories) {
        New-Item -ItemType Directory -Force -Path $runDirectory | Out-Null
        & $SimLabPath `
            --run-ai-active-macro-episode `
            $runDirectory `
            $policyPath `
            $policyHash `
            blue `
            $rulesHash `
            $definitionHash `
            $Seed `
            $TickLimit
        Assert-LastExitCode "SimLab active macro episode"

        $capturePath = Join-Path $runDirectory "decision_trace_v1.bin"
        $metadataPath = Join-Path $runDirectory "episode_metadata.json"
        $episodePath = Join-Path $runDirectory "episode_v1.jsonl"
        $reportPath = Join-Path $runDirectory "active_policy_canary_v1.json"
        & python -B Tools/AIResearch/ExportAiEpisodeV1.py `
            --input $capturePath `
            --metadata $metadataPath `
            --output $episodePath
        Assert-LastExitCode "Active macro AiEpisodeV1 export"

        # This is a visitation/query dataset, not an expert teacher dataset.
        # Raw validation is intentional; promotion/BC ingestion is forbidden
        # until an explicit correction sidecar supplies expert labels.
        & python -B Tools/AIResearch/ValidateAiEpisode.py `
            --input $episodePath
        Assert-LastExitCode "Active macro AiEpisodeV1 raw validation"

        $metadata = Get-Content -Raw -LiteralPath $metadataPath | ConvertFrom-Json
        $report = Get-Content -Raw -LiteralPath $reportPath | ConvertFrom-Json
        $episodeRecords = @(Get-Content -LiteralPath $episodePath -Encoding UTF8)
        if ($metadata.transitions.Count -lt 1 -or
            $episodeRecords.Count -ne $metadata.transitions.Count -or
            [int]$report.transition_count -ne $metadata.transitions.Count) {
            throw "Active macro transition counts are inconsistent."
        }
        if ($report.runtime_mode -ne "SIMLAB_DEBUG_CHECKPOINT_TWO_PASS_CANARY" -or
            $report.scenario_id -ne "simlab-1v1-active-macro-lane-v1" -or
            $report.dataset_usage -ne "EVALUATION_AND_DAGGER_STATE_DISCOVERY_ONLY" -or
            [bool]$report.eligible_as_imitation_expert_input -or
            $report.performance_claim -ne "CANARY_ONLY_NOT_PROMOTED" -or
            $report.policy_sha256 -ne $policyHash -or
            $report.policy_binary_sha256_prefix -ne $policyHash.Substring(0, 16)) {
            throw "Active macro report provenance is invalid."
        }
        if ([int]$report.evaluated_decision_count -lt 1 -or
            [int]$report.applied_decision_count -lt 1 -or
            [int]$report.intervention_decision_count -lt 1 -or
            [int]$report.applied_intervention_count -lt 1) {
            throw "Canary policy did not prove an applied learned-policy intervention."
        }
        if ([int]$report.evaluated_decision_count -ne
                ([int]$report.agreement_decision_count +
                    [int]$report.intervention_decision_count) -or
            [int]$report.intervention_decision_count -ne
                ([int]$report.applied_intervention_count +
                    [int]$report.safety_override_count) -or
            [int]$report.applied_decision_count -gt
                [int]$report.evaluated_decision_count -or
            [int]$report.applied_intervention_count -gt
                [int]$report.applied_decision_count) {
            throw "Active macro report counters violate their accounting invariants."
        }
        for ($index = 0; $index -lt $metadata.transitions.Count; ++$index) {
            $transition = $metadata.transitions[$index]
            $isLast = $index -eq ($metadata.transitions.Count - 1)
            $isTerminal = [bool]$transition.terminal
            $isTruncated = [bool]$transition.truncated
            if ($isTerminal -and $isTruncated) {
                throw "Active macro transition cannot be terminal and truncated."
            }
            if (($isTerminal -or $isTruncated) -ne $isLast) {
                throw "Active macro terminal/truncated boundary is invalid at index $index."
            }
        }
    }

    $artifactNames = @(
        "decision_trace_v1.bin",
        "episode_metadata.json",
        "episode_v1.jsonl",
        "active_policy_canary_v1.json"
    )
    $artifactHashes = [ordered]@{}
    foreach ($artifactName in $artifactNames) {
        $firstPath = Join-Path $runDirectories[0] $artifactName
        $secondPath = Join-Path $runDirectories[1] $artifactName
        $firstHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $firstPath).Hash
        $secondHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $secondPath).Hash
        if ($firstHash -ne $secondHash) {
            throw "Active macro artifact is not deterministic: $artifactName"
        }
        $artifactHashes[$artifactName] = $firstHash
    }

    $policyBytes = [System.IO.File]::ReadAllBytes($policyPath)
    if ($policyBytes.Length -lt 2) {
        throw "Canary policy is unexpectedly short."
    }
    $truncatedBytes = New-Object byte[] ($policyBytes.Length - 1)
    [System.Buffer]::BlockCopy(
        $policyBytes,
        0,
        $truncatedBytes,
        0,
        $truncatedBytes.Length)
    $truncatedPath = Join-Path $OutputRoot "active_canary_truncated.wbc"
    [System.IO.File]::WriteAllBytes($truncatedPath, $truncatedBytes)
    $truncatedHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $truncatedPath).Hash.ToLowerInvariant()
    $rejectedDirectory = Join-Path $OutputRoot `
        ("truncated_rejected_" + [Guid]::NewGuid().ToString("N"))
    & $SimLabPath `
        --run-ai-active-macro-episode `
        $rejectedDirectory `
        $truncatedPath `
        $truncatedHash `
        blue `
        $rulesHash `
        $definitionHash `
        $Seed `
        $TickLimit
    if ($LASTEXITCODE -ne 1 -or (Test-Path -LiteralPath $rejectedDirectory)) {
        throw "Truncated active policy artifact did not fail closed before output."
    }

    Write-Host (
        "[AIActivePolicyEpisode] PASS: checkpoint two-pass active intervention; " +
        "DAgger-only provenance; raw episode validation; two-run SHA equality; " +
        "truncated artifact fail-closed; episode_sha256=" +
        $artifactHashes["episode_v1.jsonl"] + " report_sha256=" +
        $artifactHashes["active_policy_canary_v1.json"])
}
finally {
    Pop-Location
}
