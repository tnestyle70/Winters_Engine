param(
    [string]$WintersRoot = "",
    [string]$SimLabPath = "",
    [string]$OutputRoot = "",
    [UInt64]$Seed = 42,
    [switch]$BuildMeasuredCorpus,
    [int]$MeasuredSeedsPerScenario = 8,
    [string]$MeasuredCorpusOutput = ""
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
    $OutputRoot = Join-Path $env:TEMP "WintersLiveAiEpisodeSmokeProbe"
}

function Assert-LastExitCode([string]$Step) {
    if ($LASTEXITCODE -ne 0) {
        throw "$Step failed with exit code $LASTEXITCODE."
    }
}

$rulesArtifact = $SimLabPath
$definitionSource = Join-Path $WintersRoot `
    "Data\LoL\SharedContract\DefinitionManifest.json"
$rulesHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $rulesArtifact).Hash.ToLowerInvariant()
$definitionHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $definitionSource).Hash.ToLowerInvariant()
if ($rulesHash -eq ("0" * 64) -or $definitionHash -eq ("0" * 64)) {
    throw "Live smoke inputs must use actual non-placeholder hashes."
}

$runDirectories = @(
    (Join-Path $OutputRoot "run_first"),
    (Join-Path $OutputRoot "run_second")
)

Push-Location $WintersRoot
try {
    foreach ($runDirectory in $runDirectories) {
        New-Item -ItemType Directory -Force -Path $runDirectory | Out-Null
        & $SimLabPath `
            --export-ai-research-smoke `
            $runDirectory `
            $rulesHash `
            $definitionHash `
            $Seed
        Assert-LastExitCode "SimLab live AI research export"

        $capturePath = Join-Path $runDirectory "decision_trace_v1.bin"
        $metadataPath = Join-Path $runDirectory "episode_metadata.json"
        $episodePath = Join-Path $runDirectory "episode_v1.jsonl"
        & python -B Tools/AIResearch/ExportAiEpisodeV1.py `
            --input $capturePath `
            --metadata $metadataPath `
            --output $episodePath
        Assert-LastExitCode "AiEpisodeV1 live export"

        & python -B Tools/AIResearch/ValidateAiEpisode.py `
            --input $episodePath
        Assert-LastExitCode "AiEpisodeV1 live raw validation"

        & python -B Tools/AIResearch/ValidateAiEpisode.py `
            --input $episodePath `
            --promotion
        Assert-LastExitCode "AiEpisodeV1 live promotion validation"

        $metadata = Get-Content -Raw -LiteralPath $metadataPath | ConvertFrom-Json
        if ($metadata.transitions.Count -lt 1) {
            throw "Live metadata contains no transitions."
        }
        for ($index = 0; $index -lt $metadata.transitions.Count; ++$index) {
            $transition = $metadata.transitions[$index]
            $isLast = $index -eq ($metadata.transitions.Count - 1)
            $isTerminal = [bool]$transition.terminal
            $isTruncated = [bool]$transition.truncated
            if ($isTerminal -and $isTruncated) {
                throw "Live metadata cannot be terminal and truncated at index $index."
            }
            $hasBoundary = $isTerminal -or $isTruncated
            if ($hasBoundary -ne $isLast) {
                throw "Live metadata terminal/truncated boundary is invalid at index $index."
            }
        }
    }

    $artifactNames = @(
        "decision_trace_v1.bin",
        "episode_metadata.json",
        "episode_v1.jsonl"
    )
    $artifactHashes = @{}
    foreach ($artifactName in $artifactNames) {
        $firstPath = Join-Path $runDirectories[0] $artifactName
        $secondPath = Join-Path $runDirectories[1] $artifactName
        $firstHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $firstPath).Hash
        $secondHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $secondPath).Hash
        if ($firstHash -ne $secondHash) {
            throw "Live AI research artifact is not deterministic: $artifactName"
        }
        $artifactHashes[$artifactName] = $firstHash
    }

    Write-Host (
        "[AILiveEpisodeSmoke] PASS: native live trace -> metadata -> canonical " +
        "JSONL; raw/promotion validation; two-run SHA equality; " +
        "episode_sha256=" + $artifactHashes["episode_v1.jsonl"]
    )

    if ($BuildMeasuredCorpus) {
        if ($MeasuredSeedsPerScenario -lt 2 -or $MeasuredSeedsPerScenario -gt 256) {
            throw "MeasuredSeedsPerScenario must be in [2, 256]."
        }
        if (-not $MeasuredCorpusOutput) {
            $MeasuredCorpusOutput = Join-Path $OutputRoot `
                "measured_corpus\ai_episode_v1.jsonl"
        }

        $scenarioFamilies = @("fight", "retreat", "farm", "siege")
        $sides = @("blue", "red")
        $expectedKinds = @{
            fight = 2
            retreat = 1
            farm = 3
            siege = 4
        }
        $classCounts = [ordered]@{
            retreat = 0
            fight = 0
            farm = 0
            siege = 0
        }
        $kindNames = @{
            1 = "retreat"
            2 = "fight"
            3 = "farm"
            4 = "siege"
        }
        $legalMaskHistogram = [ordered]@{}
        $records = New-Object System.Collections.Generic.List[string]
        $measuredRoot = Join-Path $OutputRoot "measured_episode_runs"

        for ($familyIndex = 0; $familyIndex -lt $scenarioFamilies.Count; ++$familyIndex) {
            $family = $scenarioFamilies[$familyIndex]
            for ($sideIndex = 0; $sideIndex -lt $sides.Count; ++$sideIndex) {
                $side = $sides[$sideIndex]
                for ($scenarioIndex = 0; $scenarioIndex -lt $MeasuredSeedsPerScenario; ++$scenarioIndex) {
                    # Blue/Red are a correlated mirror pair. They must share the
                    # same frozen split group and seed; only the episode/run id
                    # carries side identity.
                    $scenarioId = "bc-$family-$($scenarioIndex.ToString('D3'))"
                    $scenarioRunId = "$scenarioId-$side"
                    [UInt64]$scenarioSeed = `
                        $Seed + [UInt64]($familyIndex * 10000 + $scenarioIndex)
                    $repeatDirectories = @(
                        (Join-Path $measuredRoot "$scenarioRunId\repeat_a"),
                        (Join-Path $measuredRoot "$scenarioRunId\repeat_b")
                    )

                    foreach ($repeatDirectory in $repeatDirectories) {
                        New-Item -ItemType Directory -Force -Path $repeatDirectory | Out-Null
                        & $SimLabPath `
                            --export-ai-research-episode `
                            $repeatDirectory `
                            $family `
                            $scenarioId `
                            $side `
                            $rulesHash `
                            $definitionHash `
                            $scenarioSeed
                        Assert-LastExitCode "SimLab measured AI episode export"

                        $capturePath = Join-Path $repeatDirectory "decision_trace_v1.bin"
                        $metadataPath = Join-Path $repeatDirectory "episode_metadata.json"
                        $episodePath = Join-Path $repeatDirectory "episode_v1.jsonl"
                        & python -B Tools/AIResearch/ExportAiEpisodeV1.py `
                            --input $capturePath `
                            --metadata $metadataPath `
                            --output $episodePath
                        Assert-LastExitCode "Measured AiEpisodeV1 export"

                        & python -B Tools/AIResearch/ValidateAiEpisode.py `
                            --input $episodePath `
                            --promotion
                        Assert-LastExitCode "Measured AiEpisodeV1 promotion validation"
                    }

                    foreach ($artifactName in $artifactNames) {
                        $firstPath = Join-Path $repeatDirectories[0] $artifactName
                        $secondPath = Join-Path $repeatDirectories[1] $artifactName
                        $firstHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $firstPath).Hash
                        $secondHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $secondPath).Hash
                        if ($firstHash -ne $secondHash) {
                            throw "Measured AI episode is not deterministic: $scenarioRunId/$artifactName"
                        }
                    }

                    $episodePath = Join-Path $repeatDirectories[0] "episode_v1.jsonl"
                    $episodeLines = @(Get-Content -LiteralPath $episodePath -Encoding UTF8)
                    if ($episodeLines.Count -ne 1) {
                        throw "Measured scenario must contain exactly one decision: $scenarioRunId"
                    }
                    $record = $episodeLines[0] | ConvertFrom-Json
                    if ([int]$record.selected_candidate_kind -ne [int]$expectedKinds[$family]) {
                        throw "Measured scenario selected unexpected candidate: $scenarioRunId"
                    }
                    $kindName = $kindNames[[int]$record.selected_candidate_kind]
                    $classCounts[$kindName] = [int]$classCounts[$kindName] + 1
                    $legalMaskKey = "0x{0:X8}" -f `
                        [UInt32]$record.action_mask.legal_candidate_mask
                    if (-not $legalMaskHistogram.Contains($legalMaskKey)) {
                        $legalMaskHistogram[$legalMaskKey] = 0
                    }
                    $legalMaskHistogram[$legalMaskKey] = `
                        [int]$legalMaskHistogram[$legalMaskKey] + 1
                    $records.Add($episodeLines[0])
                }
            }
        }

        $corpusDirectory = Split-Path -Parent $MeasuredCorpusOutput
        New-Item -ItemType Directory -Force -Path $corpusDirectory | Out-Null
        $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
        $temporaryCorpus = "$MeasuredCorpusOutput.tmp"
        [System.IO.File]::WriteAllText(
            $temporaryCorpus,
            (($records.ToArray() -join "`n") + "`n"),
            $utf8NoBom)
        Move-Item -Force -LiteralPath $temporaryCorpus -Destination $MeasuredCorpusOutput

        & python -B Tools/AIResearch/ValidateAiEpisode.py `
            --input $MeasuredCorpusOutput `
            --promotion
        Assert-LastExitCode "Merged measured AiEpisodeV1 promotion validation"

        $corpusSha = (Get-FileHash -Algorithm SHA256 -LiteralPath $MeasuredCorpusOutput).Hash.ToLowerInvariant()
        $manifest = [ordered]@{
            schema_version = 1
            artifact_type = "MeasuredBehaviorCloningCorpusV1"
            coverage_scope = "RETREAT_VS_ONE_MACRO_BOOTSTRAP"
            simlab_sha256 = $rulesHash
            definition_sha256 = $definitionHash
            behavior_policy_revision = 1
            seed_base = [UInt64]$Seed
            seeds_per_family = $MeasuredSeedsPerScenario
            frozen_scenario_group_count = $scenarioFamilies.Count * $MeasuredSeedsPerScenario
            mirrored_pair_count = $scenarioFamilies.Count * $MeasuredSeedsPerScenario
            record_count = $records.Count
            class_counts = $classCounts
            legal_candidate_mask_histogram = $legalMaskHistogram
            dropped_record_count = 0
            corpus_sha256 = $corpusSha
            corpus_path = [System.IO.Path]::GetFullPath($MeasuredCorpusOutput)
        }
        $manifestPath = "$MeasuredCorpusOutput.manifest.json"
        $temporaryManifest = "$manifestPath.tmp"
        [System.IO.File]::WriteAllText(
            $temporaryManifest,
            (($manifest | ConvertTo-Json -Depth 8) + "`n"),
            $utf8NoBom)
        Move-Item -Force -LiteralPath $temporaryManifest -Destination $manifestPath

        Write-Host (
            "[AIMeasuredCorpus] PASS: groups=" +
            ($scenarioFamilies.Count * $MeasuredSeedsPerScenario) +
            " mirrored_records=" + $records.Count +
            " classes=" + ($classCounts | ConvertTo-Json -Compress) +
            " corpus_sha256=" + $corpusSha +
            " output=" + $MeasuredCorpusOutput
        )
    }
}
finally {
    Pop-Location
}
