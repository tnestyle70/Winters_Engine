param(
    [string]$WintersRoot = "",

    [string]$SimLabPath = "",

    [ValidateRange(300, 18000)]
    [int]$TickLimit = 300,

    [ValidateRange(1, 9223372036854775807)]
    [int64]$Seed = 42,

    [string]$ActorCounts = "1,2,4,8",

    [ValidateRange(10, 3600)]
    [int]$TimeoutSeconds = 300,

    [string]$OutputRoot = ""
)

$ErrorActionPreference = "Stop"
$invariant = [Globalization.CultureInfo]::InvariantCulture
$nativeTraceRecordBytes = 528
$utf8WithoutBom = New-Object Text.UTF8Encoding($false)

if (-not $WintersRoot) {
    $WintersRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
}
else {
    $WintersRoot = (Resolve-Path -LiteralPath $WintersRoot).Path
}
if (-not $SimLabPath) {
    $SimLabPath = Join-Path $WintersRoot "Tools\Bin\Debug\SimLab.exe"
}
if (-not (Test-Path -LiteralPath $SimLabPath -PathType Leaf)) {
    throw "Debug SimLab executable was not found: $SimLabPath"
}

$parsedActorCounts = @(
    $ActorCounts.Split(',') |
        ForEach-Object {
            $parsedActorCount = 0
            if (-not [int]::TryParse($_.Trim(), [ref]$parsedActorCount)) {
                throw "ActorCounts must be a comma-separated integer list."
            }
            $parsedActorCount
        } |
        Sort-Object -Unique
)
if ($parsedActorCounts.Count -eq 0 -or $parsedActorCounts[0] -ne 1) {
    throw "ActorCounts must contain the single-actor baseline 1."
}
foreach ($actorCount in $parsedActorCounts) {
    if ($actorCount -lt 1 -or $actorCount -gt 32) {
        throw "ActorCounts entries must be in [1, 32]."
    }
}

$activeProcesses = @(
    Get-Process msbuild, cl, link, SimLab, python -ErrorAction SilentlyContinue
)
if ($activeProcesses.Count -ne 0) {
    $owners = ($activeProcesses | ForEach-Object {
        "$($_.ProcessName):$($_.Id)"
    }) -join ","
    throw "The build/actor lane is not idle: $owners"
}

function ConvertTo-NativePathArgument([string]$Value) {
    if ($Value.Contains('"')) {
        throw "Native path arguments cannot contain a quote character."
    }
    return '"' + $Value + '"'
}

function Start-RedirectedNativeProcess(
    [string]$FilePath,
    [string[]]$Arguments,
    [string]$WorkingDirectory,
    [string]$StdoutPath,
    [string]$StderrPath) {
    $startInfo = New-Object Diagnostics.ProcessStartInfo
    $startInfo.FileName = $FilePath
    $startInfo.Arguments = $Arguments -join " "
    $startInfo.WorkingDirectory = $WorkingDirectory
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true

    $process = New-Object Diagnostics.Process
    $process.StartInfo = $startInfo
    if (-not $process.Start()) {
        $process.Dispose()
        throw "Failed to start native actor process: $FilePath"
    }
    return [pscustomobject]@{
        Process = $process
        StdoutTask = $process.StandardOutput.ReadToEndAsync()
        StderrTask = $process.StandardError.ReadToEndAsync()
        Stdout = $StdoutPath
        Stderr = $StderrPath
        OutputWritten = $false
    }
}

function Save-RedirectedProcessOutput([object]$Record) {
    if ($Record.OutputWritten) {
        return
    }
    $stdout = $Record.StdoutTask.GetAwaiter().GetResult()
    $stderr = $Record.StderrTask.GetAwaiter().GetResult()
    [IO.File]::WriteAllText($Record.Stdout, $stdout, $utf8WithoutBom)
    [IO.File]::WriteAllText($Record.Stderr, $stderr, $utf8WithoutBom)
    $Record.OutputWritten = $true
}

function Complete-RedirectedNativeProcess([object]$Record) {
    $Record.Process.WaitForExit()
    Save-RedirectedProcessOutput $Record
    return $Record.Process.ExitCode
}

function Invoke-RedirectedNativeCommand(
    [string]$FilePath,
    [string[]]$Arguments,
    [string]$WorkingDirectory,
    [string]$StdoutPath,
    [string]$StderrPath,
    [int]$WatchdogSeconds,
    [string]$Description) {
    $record = Start-RedirectedNativeProcess `
        $FilePath `
        $Arguments `
        $WorkingDirectory `
        $StdoutPath `
        $StderrPath
    try {
        if (-not $record.Process.WaitForExit($WatchdogSeconds * 1000)) {
            Stop-OwnedProcesses @($record)
            $record = $null
            throw "$Description timed out."
        }
        $exitCode = Complete-RedirectedNativeProcess $record
        if ($exitCode -ne 0) {
            $stderr = Get-Content -Raw -LiteralPath $StderrPath
            throw "$Description failed with code ${exitCode}: $stderr"
        }
    }
    finally {
        if ($null -ne $record) {
            Stop-OwnedProcesses @($record)
        }
    }
}

function Stop-OwnedProcesses([object[]]$Records) {
    foreach ($record in @($Records)) {
        if ($null -eq $record -or $null -eq $record.Process) {
            continue
        }
        try {
            $record.Process.Refresh()
            if (-not $record.Process.HasExited) {
                Stop-Process -Id $record.Process.Id -Force `
                    -ErrorAction SilentlyContinue
                $null = $record.Process.WaitForExit(5000)
            }
            Save-RedirectedProcessOutput $record
        }
        catch {
            # Best-effort cleanup must not hide the original actor failure.
        }
        finally {
            try {
                $record.Process.Dispose()
            }
            catch {
            }
        }
    }
}

function Assert-JsonProperties(
    [object]$Object,
    [string[]]$Names,
    [string]$Context) {
    foreach ($name in $Names) {
        if ($null -eq $Object.PSObject.Properties[$name]) {
            throw "$Context is missing required property $name."
        }
    }
}

if (-not $OutputRoot) {
    $sessionId = "{0}_{1}" -f `
        (Get-Date -Format "yyyyMMdd_HHmmss_fff"), `
        ([Guid]::NewGuid().ToString("N").Substring(0, 8))
    $OutputRoot = Join-Path $WintersRoot `
        ".md\build\evidence\s029_ai_actor_scaling\$sessionId\active"
}
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
$OutputRoot = (Resolve-Path -LiteralPath $OutputRoot).Path

$definitionPath = Join-Path $WintersRoot `
    "Data\LoL\SharedContract\DefinitionManifest.json"
if (-not (Test-Path -LiteralPath $definitionPath -PathType Leaf)) {
    throw "Definition manifest was not found: $definitionPath"
}
$pythonCommand = Get-Command python -ErrorAction Stop
$simLabSha256 = (
    Get-FileHash -Algorithm SHA256 -LiteralPath $SimLabPath
).Hash
$rulesHash = $simLabSha256.ToLowerInvariant()
$definitionSha256 = (
    Get-FileHash -Algorithm SHA256 -LiteralPath $definitionPath
).Hash
$definitionHash = $definitionSha256.ToLowerInvariant()
$policyPath = Join-Path $OutputRoot "active_canary_retreat.wbc"
$policyGeneratorStdout = Join-Path $OutputRoot "policy_generator_stdout.txt"
$policyGeneratorStderr = Join-Path $OutputRoot "policy_generator_stderr.txt"
$policyGeneratorRecord = $null
try {
    $policyGeneratorRecord = Start-RedirectedNativeProcess `
        $pythonCommand.Source `
        @(
            "-B",
            "Tools/AIResearch/tests/BuildActiveMacroCanaryPolicy.py",
            "--output",
            (ConvertTo-NativePathArgument $policyPath)) `
        $WintersRoot `
        $policyGeneratorStdout `
        $policyGeneratorStderr
    if (-not $policyGeneratorRecord.Process.WaitForExit(
            $TimeoutSeconds * 1000)) {
        Stop-OwnedProcesses @($policyGeneratorRecord)
        $policyGeneratorRecord = $null
        throw "Active macro canary policy generation timed out."
    }
    $policyGeneratorExitCode = `
        Complete-RedirectedNativeProcess $policyGeneratorRecord
    if ($policyGeneratorExitCode -ne 0) {
        $stderr = Get-Content -Raw -LiteralPath $policyGeneratorStderr
        throw "Active macro canary policy generation failed with code " +
            "${policyGeneratorExitCode}: $stderr"
    }
}
finally {
    if ($null -ne $policyGeneratorRecord) {
        Stop-OwnedProcesses @($policyGeneratorRecord)
    }
}
$policySha256 = (
    Get-FileHash -Algorithm SHA256 -LiteralPath $policyPath
).Hash
$policyHash = $policySha256.ToLowerInvariant()

$computer = Get-CimInstance Win32_ComputerSystem
$batches = [System.Collections.Generic.List[object]]::new()
$referenceTraceSha256 = $null
$referenceMetadataSha256 = $null
$referenceCanarySha256 = $null
$referenceValidationEpisodeSha256 = $null
$referenceStateHash = $null
$baselineTransitionsPerSecond = 0.0
$previousTransitionsPerSecond = 0.0
$monotonicThroughput = $true
$recommendedActorCount = 1
$startedUtc = (Get-Date).ToUniversalTime().ToString("o")

try {
foreach ($actorCount in $parsedActorCounts) {
    $batchDirectory = Join-Path $OutputRoot `
        ("actors_{0:D2}" -f $actorCount)
    New-Item -ItemType Directory -Path $batchDirectory | Out-Null

    $records = [System.Collections.Generic.List[object]]::new()
    $batchWatch = [Diagnostics.Stopwatch]::StartNew()
    for ($actorIndex = 0; $actorIndex -lt $actorCount; ++$actorIndex) {
        $actorDirectory = Join-Path $batchDirectory `
            ("actor_{0:D2}" -f $actorIndex)
        New-Item -ItemType Directory -Path $actorDirectory | Out-Null
        $stdoutPath = Join-Path $actorDirectory "stdout.txt"
        $stderrPath = Join-Path $actorDirectory "stderr.txt"
        $started = Start-RedirectedNativeProcess `
            $SimLabPath `
            @(
                "--run-ai-active-macro-episode",
                (ConvertTo-NativePathArgument $actorDirectory),
                (ConvertTo-NativePathArgument $policyPath),
                $policyHash,
                "blue",
                $rulesHash,
                $definitionHash,
                $Seed.ToString($invariant),
                $TickLimit.ToString($invariant)) `
            $WintersRoot `
            $stdoutPath `
            $stderrPath
        $records.Add([pscustomobject]@{
            ActorIndex = $actorIndex
            Directory = $actorDirectory
            Stdout = $stdoutPath
            Stderr = $stderrPath
            Process = $started.Process
            StdoutTask = $started.StdoutTask
            StderrTask = $started.StderrTask
            OutputWritten = $false
        }) | Out-Null
    }

    $peakAggregateRssBytes = 0L
    $peakAggregatePrivateBytes = 0L
    while ($true) {
        $runningCount = 0
        $aggregateRssBytes = 0L
        $aggregatePrivateBytes = 0L
        foreach ($record in $records) {
            $process = $record.Process
            if ($process.HasExited) {
                continue
            }
            ++$runningCount
            $process.Refresh()
            $aggregateRssBytes += $process.WorkingSet64
            $aggregatePrivateBytes += $process.PrivateMemorySize64
        }
        $peakAggregateRssBytes = [Math]::Max(
            $peakAggregateRssBytes,
            $aggregateRssBytes)
        $peakAggregatePrivateBytes = [Math]::Max(
            $peakAggregatePrivateBytes,
            $aggregatePrivateBytes)
        if ($runningCount -eq 0) {
            break
        }
        if ($batchWatch.Elapsed.TotalSeconds -gt $TimeoutSeconds) {
            Stop-OwnedProcesses $records
            throw "Active actor batch $actorCount exceeded $TimeoutSeconds seconds."
        }
        Start-Sleep -Milliseconds 10
    }
    $batchWatch.Stop()

    $actors = [System.Collections.Generic.List[object]]::new()
    $traceHashes = [System.Collections.Generic.List[string]]::new()
    $metadataHashes = [System.Collections.Generic.List[string]]::new()
    $canaryHashes = [System.Collections.Generic.List[string]]::new()
    $validationEpisodeHashes = `
        [System.Collections.Generic.List[string]]::new()
    $stateHashes = [System.Collections.Generic.List[string]]::new()
    $totalTransitions = [uint64]0
    $totalEvaluations = [uint64]0
    $totalAppliedInterventions = [uint64]0
    $totalSafetyOverrides = [uint64]0
    $totalCompletedTicks = [uint64]0
    $totalCpuSeconds = 0.0

    foreach ($record in $records) {
        $exitCode = Complete-RedirectedNativeProcess $record
        if ($exitCode -ne 0) {
            $stderr = Get-Content -Raw -LiteralPath $record.Stderr
            throw "Active actor $($record.ActorIndex) exited with code " +
                "${exitCode}: $stderr"
        }
        $totalCpuSeconds += $record.Process.TotalProcessorTime.TotalSeconds
        $passLines = @(
            Get-Content -LiteralPath $record.Stdout -Encoding UTF8 |
                Where-Object { $_ -match '^\[SimLab\]\[AIActive\] PASS:' }
        )
        if ($passLines.Count -ne 1) {
            $stderr = Get-Content -Raw -LiteralPath $record.Stderr
            throw "Active actor $($record.ActorIndex) did not emit one PASS result: $stderr"
        }

        $tracePath = Join-Path $record.Directory "decision_trace_v1.bin"
        $metadataPath = Join-Path $record.Directory "episode_metadata.json"
        $canaryPath = Join-Path $record.Directory "active_policy_canary_v1.json"
        foreach ($requiredPath in @($tracePath, $metadataPath, $canaryPath)) {
            if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
                throw "Active actor artifact is missing: $requiredPath"
            }
        }
        $traceSha256 = (
            Get-FileHash -Algorithm SHA256 -LiteralPath $tracePath
        ).Hash
        $metadataSha256 = (
            Get-FileHash -Algorithm SHA256 -LiteralPath $metadataPath
        ).Hash
        $canarySha256 = (
            Get-FileHash -Algorithm SHA256 -LiteralPath $canaryPath
        ).Hash
        $report = Get-Content -Raw -LiteralPath $canaryPath | ConvertFrom-Json
        $metadata = `
            Get-Content -Raw -LiteralPath $metadataPath | ConvertFrom-Json

        Assert-JsonProperties $report @(
            "schema_version",
            "artifact_type",
            "runtime_mode",
            "scenario_id",
            "side",
            "seed",
            "tick_limit",
            "completed_tick",
            "policy_revision",
            "source_policy_revision",
            "policy_sha256",
            "policy_binary_sha256_prefix",
            "transition_count",
            "evaluated_decision_count",
            "applied_decision_count",
            "agreement_decision_count",
            "intervention_decision_count",
            "applied_intervention_count",
            "safety_override_count",
            "terminal",
            "truncated",
            "outcome",
            "final_state_hash",
            "dataset_usage",
            "eligible_as_imitation_expert_input",
            "performance_claim") `
            "Active actor $($record.ActorIndex) report"
        Assert-JsonProperties $metadata @(
            "schema_version",
            "episode_id",
            "scenario_id",
            "timeline_epoch",
            "branch_id",
            "seed",
            "rules_hash",
            "definition_hash",
            "policy_revision",
            "transitions") `
            "Active actor $($record.ActorIndex) metadata"

        if ([int]$report.schema_version -ne 1 -or
            $report.artifact_type -ne "ActiveMacroCanaryReportV1" -or
            $report.runtime_mode -ne `
                "SIMLAB_DEBUG_CHECKPOINT_TWO_PASS_CANARY" -or
            $report.scenario_id -ne "simlab-1v1-active-macro-lane-v1" -or
            $report.side -ne "blue" -or
            [uint64]$report.seed -ne [uint64]$Seed -or
            [int]$report.tick_limit -ne $TickLimit -or
            [int]$report.completed_tick -ne $TickLimit -or
            [uint64]$report.policy_revision -lt 1 -or
            [uint64]$report.source_policy_revision -lt 1 -or
            [uint64]$report.source_policy_revision -gt
                [uint64]$report.policy_revision -or
            $report.dataset_usage -ne `
                "EVALUATION_AND_DAGGER_STATE_DISCOVERY_ONLY" -or
            [bool]$report.eligible_as_imitation_expert_input -or
            $report.performance_claim -ne "CANARY_ONLY_NOT_PROMOTED" -or
            $report.policy_sha256 -ne $policyHash -or
            $report.policy_binary_sha256_prefix -ne
                $policyHash.Substring(0, 16) -or
            [bool]$report.terminal -or
            -not [bool]$report.truncated -or
            $report.outcome -ne "time_limit") {
            throw "Active actor $($record.ActorIndex) provenance gate failed."
        }
        if ([int]$report.transition_count -lt 1 -or
            [int]$report.evaluated_decision_count -lt 1 -or
            [int]$report.applied_decision_count -lt 1 -or
            [int]$report.intervention_decision_count -lt 1 -or
            [int]$report.applied_intervention_count -lt 1 -or
            [int]$report.evaluated_decision_count -ne
                ([int]$report.agreement_decision_count +
                    [int]$report.intervention_decision_count) -or
            [int]$report.intervention_decision_count -ne
                ([int]$report.applied_intervention_count +
                    [int]$report.safety_override_count) -or
            [int]$report.applied_decision_count -gt
                [int]$report.evaluated_decision_count -or
            [int]$report.applied_intervention_count -gt
                [int]$report.applied_decision_count) {
            throw "Active actor $($record.ActorIndex) counter gate failed."
        }
        if ([int]$metadata.schema_version -ne 1 -or
            $metadata.episode_id -ne
                ("simlab-active-macro-v1-blue-{0}-r{1}-p{2}" -f
                    $Seed,
                    $report.policy_revision,
                    $policyHash.Substring(0, 16)) -or
            $metadata.scenario_id -ne "simlab-1v1-active-macro-lane-v1" -or
            [uint64]$metadata.timeline_epoch -ne 1 -or
            [uint64]$metadata.branch_id -ne 0 -or
            [uint64]$metadata.seed -ne [uint64]$Seed -or
            $metadata.rules_hash -ne $rulesHash -or
            $metadata.definition_hash -ne $definitionHash -or
            [uint64]$metadata.policy_revision -ne
                [uint64]$report.policy_revision -or
            $metadata.transitions.Count -ne [int]$report.transition_count -or
            (Get-Item -LiteralPath $tracePath).Length -ne
                ([int64]$report.transition_count * $nativeTraceRecordBytes)) {
            throw "Active actor $($record.ActorIndex) artifact contract failed."
        }
        for ($index = 0; $index -lt $metadata.transitions.Count; ++$index) {
            $transition = $metadata.transitions[$index]
            Assert-JsonProperties $transition @(
                "trace_index",
                "next_state_hash",
                "reward",
                "terminal",
                "truncated") `
                "Active actor $($record.ActorIndex) transition $index"
            $isLast = $index -eq ($metadata.transitions.Count - 1)
            $isTerminal = [bool]$transition.terminal
            $isTruncated = [bool]$transition.truncated
            if ($isTerminal -and $isTruncated) {
                throw "Active actor $($record.ActorIndex) has an invalid boundary."
            }
            if (($isTerminal -or $isTruncated) -ne $isLast) {
                throw "Active actor $($record.ActorIndex) boundary index is invalid."
            }
        }

        $validationEpisodePath = `
            Join-Path $record.Directory "validation_episode_v1.jsonl"
        Invoke-RedirectedNativeCommand `
            $pythonCommand.Source `
            @(
                "-B",
                "Tools/AIResearch/ExportAiEpisodeV1.py",
                "--input",
                (ConvertTo-NativePathArgument $tracePath),
                "--metadata",
                (ConvertTo-NativePathArgument $metadataPath),
                "--output",
                (ConvertTo-NativePathArgument $validationEpisodePath)) `
            $WintersRoot `
            (Join-Path $record.Directory "export_stdout.txt") `
            (Join-Path $record.Directory "export_stderr.txt") `
            $TimeoutSeconds `
            "Active actor $($record.ActorIndex) AiEpisodeV1 export"
        Invoke-RedirectedNativeCommand `
            $pythonCommand.Source `
            @(
                "-B",
                "Tools/AIResearch/ValidateAiEpisode.py",
                "--input",
                (ConvertTo-NativePathArgument $validationEpisodePath)) `
            $WintersRoot `
            (Join-Path $record.Directory "validate_stdout.txt") `
            (Join-Path $record.Directory "validate_stderr.txt") `
            $TimeoutSeconds `
            "Active actor $($record.ActorIndex) AiEpisodeV1 validation"
        $validationEpisodeRecords = @(
            Get-Content -LiteralPath $validationEpisodePath -Encoding UTF8
        )
        if ($validationEpisodeRecords.Count -ne
                [int]$report.transition_count) {
            throw "Active actor $($record.ActorIndex) JSONL count is invalid."
        }
        $validationEpisodeSha256 = (
            Get-FileHash -Algorithm SHA256 `
                -LiteralPath $validationEpisodePath
        ).Hash

        $traceHashes.Add($traceSha256) | Out-Null
        $metadataHashes.Add($metadataSha256) | Out-Null
        $canaryHashes.Add($canarySha256) | Out-Null
        $validationEpisodeHashes.Add($validationEpisodeSha256) | Out-Null
        $stateHashes.Add($report.final_state_hash) | Out-Null
        $totalTransitions += [uint64]$report.transition_count
        $totalEvaluations += [uint64]$report.evaluated_decision_count
        $totalAppliedInterventions += `
            [uint64]$report.applied_intervention_count
        $totalSafetyOverrides += [uint64]$report.safety_override_count
        $totalCompletedTicks += [uint64]$report.completed_tick
        $actors.Add([pscustomobject][ordered]@{
            actor_index = $record.ActorIndex
            transition_count = [uint64]$report.transition_count
            evaluated_decision_count = `
                [uint64]$report.evaluated_decision_count
            applied_intervention_count = `
                [uint64]$report.applied_intervention_count
            safety_override_count = [uint64]$report.safety_override_count
            final_state_hash = $report.final_state_hash
            decision_trace_sha256 = $traceSha256
            metadata_sha256 = $metadataSha256
            canary_report_sha256 = $canarySha256
            validation_episode_sha256 = $validationEpisodeSha256
            validation_episode_bytes = `
                [uint64](Get-Item -LiteralPath $validationEpisodePath).Length
            native_artifact_bytes = `
                [uint64](Get-Item -LiteralPath $tracePath).Length +
                [uint64](Get-Item -LiteralPath $metadataPath).Length +
                [uint64](Get-Item -LiteralPath $canaryPath).Length
        }) | Out-Null
    }

    $distinctTraceHashes = @($traceHashes | Sort-Object -Unique)
    $distinctMetadataHashes = @($metadataHashes | Sort-Object -Unique)
    $distinctCanaryHashes = @($canaryHashes | Sort-Object -Unique)
    $distinctValidationEpisodeHashes = `
        @($validationEpisodeHashes | Sort-Object -Unique)
    $distinctStateHashes = @($stateHashes | Sort-Object -Unique)
    if ($distinctTraceHashes.Count -ne 1 -or
        $distinctMetadataHashes.Count -ne 1 -or
        $distinctCanaryHashes.Count -ne 1 -or
        $distinctValidationEpisodeHashes.Count -ne 1 -or
        $distinctStateHashes.Count -ne 1) {
        throw "Same-seed active actor isolation failed for count $actorCount."
    }
    if ($null -eq $referenceTraceSha256) {
        $referenceTraceSha256 = $distinctTraceHashes[0]
        $referenceMetadataSha256 = $distinctMetadataHashes[0]
        $referenceCanarySha256 = $distinctCanaryHashes[0]
        $referenceValidationEpisodeSha256 = `
            $distinctValidationEpisodeHashes[0]
        $referenceStateHash = $distinctStateHashes[0]
    }
    elseif ($referenceTraceSha256 -ne $distinctTraceHashes[0] -or
        $referenceMetadataSha256 -ne $distinctMetadataHashes[0] -or
        $referenceCanarySha256 -ne $distinctCanaryHashes[0] -or
        $referenceValidationEpisodeSha256 -ne
            $distinctValidationEpisodeHashes[0] -or
        $referenceStateHash -ne $distinctStateHashes[0]) {
        throw "Active actor-count scaling changed deterministic output at count $actorCount."
    }

    $batchSeconds = $batchWatch.Elapsed.TotalSeconds
    $aggregateTicks = $totalCompletedTicks
    $ticksPerSecond = $aggregateTicks / $batchSeconds
    $transitionsPerSecond = $totalTransitions / $batchSeconds
    if ($baselineTransitionsPerSecond -eq 0.0) {
        $baselineTransitionsPerSecond = $transitionsPerSecond
    }
    $throughputSpeedup = `
        $transitionsPerSecond / $baselineTransitionsPerSecond
    $parallelEfficiency = $throughputSpeedup / $actorCount
    if ($previousTransitionsPerSecond -gt 0.0 -and
        $transitionsPerSecond -lt $previousTransitionsPerSecond * 0.90) {
        $monotonicThroughput = $false
    }
    $previousTransitionsPerSecond = $transitionsPerSecond
    if ($parallelEfficiency -ge 0.50) {
        $recommendedActorCount = $actorCount
    }

    $batches.Add([pscustomobject][ordered]@{
        actor_count = $actorCount
        end_to_end_batch_wall_seconds = $batchSeconds
        total_cpu_seconds = $totalCpuSeconds
        average_cpu_cores = $totalCpuSeconds / $batchSeconds
        aggregate_ticks = $aggregateTicks
        aggregate_ticks_per_second = $ticksPerSecond
        aggregate_transitions = $totalTransitions
        transitions_per_second = $transitionsPerSecond
        evaluations_per_second = $totalEvaluations / $batchSeconds
        applied_interventions_per_second = `
            $totalAppliedInterventions / $batchSeconds
        safety_overrides_per_second = `
            $totalSafetyOverrides / $batchSeconds
        throughput_speedup = $throughputSpeedup
        parallel_efficiency = $parallelEfficiency
        sampled_peak_aggregate_rss_mib = $peakAggregateRssBytes / 1MB
        sampled_peak_aggregate_private_mib = `
            $peakAggregatePrivateBytes / 1MB
        decision_trace_sha256 = $distinctTraceHashes[0]
        metadata_sha256 = $distinctMetadataHashes[0]
        canary_report_sha256 = $distinctCanaryHashes[0]
        validation_episode_sha256 = `
            $distinctValidationEpisodeHashes[0]
        final_state_hash = $distinctStateHashes[0]
        actors = $actors
    }) | Out-Null
    Write-Host (
        "[ActiveAiActorScaling] actors=$actorCount " +
        "transitions_per_sec=$($transitionsPerSecond.ToString('F3', $invariant)) " +
        "speedup=$($throughputSpeedup.ToString('F3', $invariant)) " +
        "efficiency=$($parallelEfficiency.ToString('F3', $invariant))")
    Stop-OwnedProcesses $records
}
}
finally {
    if ($null -ne $records) {
        Stop-OwnedProcesses $records
    }
}

$report = [ordered]@{
    schema_version = 1
    status = "PASS"
    runtime_mode = "SIMLAB_DEBUG_CHECKPOINT_TWO_PASS_CANARY"
    dataset_usage = "EVALUATION_AND_DAGGER_STATE_DISCOVERY_ONLY"
    eligible_as_imitation_expert_input = $false
    post_batch_jsonl_export_validation_included = $true
    jsonl_export_included_in_throughput = $false
    started_utc = $startedUtc
    completed_utc = (Get-Date).ToUniversalTime().ToString("o")
    tick_limit_per_actor = $TickLimit
    seed = [uint64]$Seed
    seed_mode = "SAME_SEED_REPLICA"
    actor_counts = $parsedActorCounts
    logical_processor_count = [Environment]::ProcessorCount
    physical_memory_bytes = [uint64]$computer.TotalPhysicalMemory
    simlab_path = (Get-Item -LiteralPath $SimLabPath).FullName
    simlab_sha256 = $simLabSha256
    definition_sha256 = $definitionSha256
    policy_sha256 = $policySha256
    deterministic_trace_sha256 = $referenceTraceSha256
    deterministic_metadata_sha256 = $referenceMetadataSha256
    deterministic_canary_report_sha256 = $referenceCanarySha256
    deterministic_validation_episode_sha256 = `
        $referenceValidationEpisodeSha256
    deterministic_final_state_hash = $referenceStateHash
    aggregate_throughput_monotonic_with_10_percent_tolerance = `
        $monotonicThroughput
    recommended_actor_count_at_50_percent_efficiency = `
        $recommendedActorCount
    batches = $batches
}
$reportPath = Join-Path $OutputRoot "active_ai_actor_scaling_v1.json"
[IO.File]::WriteAllText(
    $reportPath,
    ($report | ConvertTo-Json -Depth 10),
    $utf8WithoutBom)

Write-Host (
    "[ActiveAiActorScaling] PASS: native_canary_only=true " +
    "recommended_actors=$recommendedActorCount report=$reportPath")
