param(
    [string]$WintersRoot = "",

    [string]$HarnessPath = "",

    [ValidateRange(300, 54000)]
    [uint32]$TickCount = 1800,

    [ValidateRange(1, 9223372036854775807)]
    [int64]$Seed = 42,

    [string]$ActorCounts = "1,2,4,8",

    [ValidateRange(1, 4294967295)]
    [uint32]$HeartbeatTicks = 1800,

    [ValidateRange(64, 65536)]
    [uint64]$PrivateLimitMiB = 1024,

    [ValidateRange(10, 3600)]
    [int]$TimeoutSeconds = 300,

    [string]$OutputRoot = ""
)

$ErrorActionPreference = "Stop"
$invariant = [Globalization.CultureInfo]::InvariantCulture
$utf8WithoutBom = New-Object Text.UTF8Encoding($false)

if (-not $WintersRoot) {
    $WintersRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
}
else {
    $WintersRoot = (Resolve-Path -LiteralPath $WintersRoot).Path
}
if (-not $HarnessPath) {
    $HarnessPath = Join-Path $env:TEMP `
        "WintersGameRoomBotMatchSoak\GameRoomBotMatchSoak_Release.exe"
}
if (-not (Test-Path -LiteralPath $HarnessPath -PathType Leaf)) {
    throw "Release GameRoom soak harness was not found: $HarnessPath"
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
    Get-Process msbuild, cl, link -ErrorAction SilentlyContinue
    Get-Process -ErrorAction SilentlyContinue |
        Where-Object { $_.ProcessName -like "GameRoomBotMatchSoak*" }
)
if ($activeProcesses.Count -ne 0) {
    $owners = ($activeProcesses | ForEach-Object {
        "$($_.ProcessName):$($_.Id)"
    }) -join ","
    throw "The build/actor lane is not idle: $owners"
}

$engineRuntime = Join-Path $WintersRoot "EngineSDK\bin\Release"
if (-not (Test-Path -LiteralPath $engineRuntime -PathType Container)) {
    throw "Release EngineSDK runtime was not found: $engineRuntime"
}
if (-not $OutputRoot) {
    $sessionId = "{0}_{1}" -f `
        (Get-Date -Format "yyyyMMdd_HHmmss_fff"), `
        ([Guid]::NewGuid().ToString("N").Substring(0, 8))
    $OutputRoot = Join-Path $WintersRoot `
        ".md\build\evidence\s029_ai_actor_scaling\$sessionId"
}
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
$OutputRoot = (Resolve-Path -LiteralPath $OutputRoot).Path

function ConvertFrom-ResultLine([string]$Line) {
    $values = @{}
    foreach ($match in [regex]::Matches(
        $Line,
        '(?<key>[A-Za-z0-9_]+)=(?<value>"[^"]*"|\S+)')) {
        $value = $match.Groups["value"].Value
        if ($value.Length -ge 2 -and
            $value.StartsWith('"') -and
            $value.EndsWith('"')) {
            $value = $value.Substring(1, $value.Length - 2)
        }
        $values[$match.Groups["key"].Value] = $value
    }
    return $values
}

function ConvertTo-Double([hashtable]$Values, [string]$Name) {
    if (-not $Values.ContainsKey($Name)) {
        throw "Missing numeric result field: $Name"
    }
    return [double]::Parse(
        $Values[$Name],
        [Globalization.NumberStyles]::Float,
        $invariant)
}

function ConvertTo-UInt64([hashtable]$Values, [string]$Name) {
    if (-not $Values.ContainsKey($Name)) {
        throw "Missing integer result field: $Name"
    }
    return [uint64]::Parse($Values[$Name], $invariant)
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

$harnessItem = Get-Item -LiteralPath $HarnessPath
$harnessSha256 = (
    Get-FileHash -Algorithm SHA256 -LiteralPath $HarnessPath
).Hash
$serverPath = Join-Path $WintersRoot "Server\Bin\Release\WintersServer.exe"
$serverSha256 = if (Test-Path -LiteralPath $serverPath -PathType Leaf) {
    (Get-FileHash -Algorithm SHA256 -LiteralPath $serverPath).Hash
}
else {
    "MISSING"
}

$computer = Get-CimInstance Win32_ComputerSystem
$operatingSystem = Get-CimInstance Win32_OperatingSystem
$batches = [System.Collections.Generic.List[object]]::new()
$referenceReplayHash = $null
$referenceWorldHash = $null
$referenceFinalStateSha256 = $null
$baselineTicksPerSecond = 0.0
$previousTicksPerSecond = 0.0
$recommendedActorCount = 1
$monotonicThroughput = $true
$previousPath = $env:PATH
$startedUtc = (Get-Date).ToUniversalTime().ToString("o")

try {
    $env:PATH = "$engineRuntime;$previousPath"
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
            $roomId = 290000 + $actorCount * 100 + $actorIndex
            $started = Start-RedirectedNativeProcess `
                $HarnessPath `
                @(
                    $TickCount.ToString($invariant),
                    $Seed.ToString($invariant),
                    $roomId.ToString($invariant),
                    $HeartbeatTicks.ToString($invariant),
                    $PrivateLimitMiB.ToString($invariant)) `
                $actorDirectory `
                $stdoutPath `
                $stderrPath
            $records.Add([pscustomobject]@{
                ActorIndex = $actorIndex
                RoomId = $roomId
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
        $peakAggregateHandles = 0L
        while ($true) {
            $runningCount = 0
            $aggregateRssBytes = 0L
            $aggregatePrivateBytes = 0L
            $aggregateHandles = 0L
            foreach ($record in $records) {
                $process = $record.Process
                if ($process.HasExited) {
                    continue
                }
                ++$runningCount
                $process.Refresh()
                $aggregateRssBytes += $process.WorkingSet64
                $aggregatePrivateBytes += $process.PrivateMemorySize64
                $aggregateHandles += $process.HandleCount
            }
            $peakAggregateRssBytes = [Math]::Max(
                $peakAggregateRssBytes,
                $aggregateRssBytes)
            $peakAggregatePrivateBytes = [Math]::Max(
                $peakAggregatePrivateBytes,
                $aggregatePrivateBytes)
            $peakAggregateHandles = [Math]::Max(
                $peakAggregateHandles,
                $aggregateHandles)
            if ($runningCount -eq 0) {
                break
            }
            if ($batchWatch.Elapsed.TotalSeconds -gt $TimeoutSeconds) {
                Stop-OwnedProcesses $records
                throw "Actor batch $actorCount exceeded $TimeoutSeconds seconds."
            }
            Start-Sleep -Milliseconds 10
        }
        $batchWatch.Stop()

        $actors = [System.Collections.Generic.List[object]]::new()
        $replayHashes = [System.Collections.Generic.List[string]]::new()
        $worldHashes = [System.Collections.Generic.List[string]]::new()
        $finalStateHashes = [System.Collections.Generic.List[string]]::new()
        $totalCommandSequence = [uint64]0
        $totalReplayBytes = [uint64]0
        $totalCpuSeconds = 0.0
        $maxTickP99Us = 0.0
        $maxTickUs = 0.0
        $totalDeadlineMisses = [uint64]0

        foreach ($record in $records) {
            $exitCode = Complete-RedirectedNativeProcess $record
            if ($exitCode -ne 0) {
                $stderr = Get-Content -Raw -LiteralPath $record.Stderr
                throw "Actor $($record.ActorIndex) exited with code " +
                    "${exitCode}: $stderr"
            }
            $totalCpuSeconds += $record.Process.TotalProcessorTime.TotalSeconds
            $output = @(Get-Content -LiteralPath $record.Stdout -Encoding UTF8)
            $resultLines = @($output | Where-Object {
                $_ -match '^RESULT status=PASS '
            })
            if ($resultLines.Count -ne 1) {
                $stderr = Get-Content -Raw -LiteralPath $record.Stderr
                throw "Actor $($record.ActorIndex) did not emit one PASS result: $stderr"
            }
            $values = ConvertFrom-ResultLine $resultLines[0]
            foreach ($requiredName in @(
                "replay_hash",
                "world_hash",
                "steady_handle_delta")) {
                if (-not $values.ContainsKey($requiredName) -or
                    [string]::IsNullOrWhiteSpace($values[$requiredName])) {
                    throw "Actor $($record.ActorIndex) is missing result field " +
                        "$requiredName."
                }
            }
            if ((ConvertTo-UInt64 $values "ticks") -ne $TickCount -or
                (ConvertTo-UInt64 $values "seed") -ne [uint64]$Seed -or
                (ConvertTo-UInt64 $values "command_active_bots") -ne 10 -or
                (ConvertTo-UInt64 $values "inactive_bots") -ne 0) {
                throw "Actor $($record.ActorIndex) result contract is invalid."
            }

            $progressLines = @($output | Where-Object {
                $_ -match '^PROGRESS tick='
            })
            if ($progressLines.Count -eq 0) {
                throw "Actor $($record.ActorIndex) emitted no progress sample."
            }
            $progress = ConvertFrom-ResultLine $progressLines[-1]
            if ((ConvertTo-UInt64 $progress "tick") -ne $TickCount) {
                throw "Actor $($record.ActorIndex) final progress tick is invalid."
            }
            $commandSequence = ConvertTo-UInt64 $progress "command_seq_sum"

            $finalStatePath = Join-Path $record.Directory "final_state.bin"
            if (-not (Test-Path -LiteralPath $finalStatePath -PathType Leaf)) {
                throw "Actor $($record.ActorIndex) final state is missing."
            }
            $finalStateSha256 = (
                Get-FileHash -Algorithm SHA256 -LiteralPath $finalStatePath
            ).Hash
            $replayFiles = @(
                Get-ChildItem -LiteralPath (Join-Path $record.Directory "Replay") `
                    -Filter "*.wrpl" -File
            )
            if ($replayFiles.Count -ne 1) {
                throw "Actor $($record.ActorIndex) did not publish one replay."
            }

            $replayHash = $values["replay_hash"]
            $worldHash = $values["world_hash"]
            if ($replayHash -cnotmatch '^[0-9A-F]{16}$' -or
                $worldHash -cnotmatch '^[0-9A-F]{16}$') {
                throw "Actor $($record.ActorIndex) emitted an invalid hash."
            }
            $replayHashes.Add($replayHash) | Out-Null
            $worldHashes.Add($worldHash) | Out-Null
            $finalStateHashes.Add($finalStateSha256) | Out-Null
            $totalCommandSequence += $commandSequence
            $totalReplayBytes += [uint64]$replayFiles[0].Length
            $maxTickP99Us = [Math]::Max(
                $maxTickP99Us,
                (ConvertTo-Double $values "tick_p99_us"))
            $maxTickUs = [Math]::Max(
                $maxTickUs,
                (ConvertTo-Double $values "tick_max_us"))
            $totalDeadlineMisses += ConvertTo-UInt64 `
                $values "deadline_misses"

            $actors.Add([pscustomobject][ordered]@{
                actor_index = $record.ActorIndex
                room_id = $record.RoomId
                replay_hash = $replayHash
                world_hash = $worldHash
                final_state_sha256 = $finalStateSha256
                command_sequence_sum = $commandSequence
                wall_seconds = ConvertTo-Double $values "wall_sec"
                stop_seconds = ConvertTo-Double $values "stop_sec"
                tick_p99_us = ConvertTo-Double $values "tick_p99_us"
                tick_max_us = ConvertTo-Double $values "tick_max_us"
                deadline_misses = ConvertTo-UInt64 $values "deadline_misses"
                peak_private_mib = ConvertTo-Double $values "peak_private_mib"
                private_growth_mib = ConvertTo-Double $values "private_growth_mib"
                steady_handle_delta = [int64]$values["steady_handle_delta"]
                replay_records = ConvertTo-UInt64 $values "replay_records"
                replay_bytes = [uint64]$replayFiles[0].Length
            }) | Out-Null
        }

        $distinctReplayHashes = @($replayHashes | Sort-Object -Unique)
        $distinctWorldHashes = @($worldHashes | Sort-Object -Unique)
        $distinctFinalStateHashes = @($finalStateHashes | Sort-Object -Unique)
        if ($distinctReplayHashes.Count -ne 1 -or
            $distinctWorldHashes.Count -ne 1 -or
            $distinctFinalStateHashes.Count -ne 1) {
            throw "Same-seed actor isolation failed for count $actorCount."
        }
        if ($null -eq $referenceReplayHash) {
            $referenceReplayHash = $distinctReplayHashes[0]
            $referenceWorldHash = $distinctWorldHashes[0]
            $referenceFinalStateSha256 = $distinctFinalStateHashes[0]
        }
        elseif ($referenceReplayHash -ne $distinctReplayHashes[0] -or
            $referenceWorldHash -ne $distinctWorldHashes[0] -or
            $referenceFinalStateSha256 -ne $distinctFinalStateHashes[0]) {
            throw "Actor-count scaling changed deterministic output at count $actorCount."
        }

        $batchSeconds = $batchWatch.Elapsed.TotalSeconds
        $aggregateTicks = [uint64]$actorCount * [uint64]$TickCount
        $aggregateTicksPerSecond = $aggregateTicks / $batchSeconds
        if ($baselineTicksPerSecond -eq 0.0) {
            $baselineTicksPerSecond = $aggregateTicksPerSecond
        }
        $throughputSpeedup = $aggregateTicksPerSecond / $baselineTicksPerSecond
        $parallelEfficiency = $throughputSpeedup / $actorCount
        if ($previousTicksPerSecond -gt 0.0 -and
            $aggregateTicksPerSecond -lt $previousTicksPerSecond * 0.90) {
            $monotonicThroughput = $false
        }
        $previousTicksPerSecond = $aggregateTicksPerSecond
        if ($parallelEfficiency -ge 0.50 -and
            $maxTickP99Us -lt 33333.333) {
            $recommendedActorCount = $actorCount
        }

        $batch = [ordered]@{
            actor_count = $actorCount
            end_to_end_batch_wall_seconds = $batchSeconds
            total_cpu_seconds = $totalCpuSeconds
            average_cpu_cores = $totalCpuSeconds / $batchSeconds
            aggregate_ticks = $aggregateTicks
            aggregate_ticks_per_second = $aggregateTicksPerSecond
            bot_steps_per_second = $aggregateTicksPerSecond * 10.0
            commands_per_second = $totalCommandSequence / $batchSeconds
            replay_bytes_per_second = $totalReplayBytes / $batchSeconds
            throughput_speedup = $throughputSpeedup
            parallel_efficiency = $parallelEfficiency
            sampled_peak_aggregate_rss_mib = $peakAggregateRssBytes / 1MB
            sampled_peak_aggregate_private_mib = `
                $peakAggregatePrivateBytes / 1MB
            sampled_peak_aggregate_handles = $peakAggregateHandles
            max_tick_p99_us = $maxTickP99Us
            max_tick_us = $maxTickUs
            total_deadline_misses = $totalDeadlineMisses
            replay_hash = $distinctReplayHashes[0]
            world_hash = $distinctWorldHashes[0]
            final_state_sha256 = $distinctFinalStateHashes[0]
            actors = $actors
        }
        $batches.Add($batch) | Out-Null
        Write-Host (
            "[GameRoomActorScaling] actors=$actorCount " +
            "ticks_per_sec=$($aggregateTicksPerSecond.ToString('F3', $invariant)) " +
            "speedup=$($throughputSpeedup.ToString('F3', $invariant)) " +
            "efficiency=$($parallelEfficiency.ToString('F3', $invariant)) " +
            "p99_us=$($maxTickP99Us.ToString('F3', $invariant))")
        Stop-OwnedProcesses $records
    }
}
finally {
    if ($null -ne $records) {
        Stop-OwnedProcesses $records
    }
    $env:PATH = $previousPath
}

$report = [ordered]@{
    schema_version = 1
    status = "PASS"
    execution_model = "ONE_CGAMEROOM_PER_PROCESS"
    same_process_multi_room_validated = $false
    same_process_multi_room_blocker = `
        "Server hub/global room ownership and shared non-atomic static diagnostics require a separate gate."
    started_utc = $startedUtc
    completed_utc = (Get-Date).ToUniversalTime().ToString("o")
    winters_root = $WintersRoot
    harness_path = $harnessItem.FullName
    harness_bytes = [uint64]$harnessItem.Length
    harness_sha256 = $harnessSha256
    server_sha256 = $serverSha256
    tick_count_per_actor = $TickCount
    seed = [uint64]$Seed
    seed_mode = "SAME_SEED_REPLICA"
    room_id_formula = "290000 + actor_count * 100 + actor_index"
    actor_counts = $parsedActorCounts
    logical_processor_count = [Environment]::ProcessorCount
    physical_memory_bytes = [uint64]$computer.TotalPhysicalMemory
    free_physical_memory_bytes_at_start = `
        [uint64]$operatingSystem.FreePhysicalMemory * 1KB
    deterministic_replay_hash = $referenceReplayHash
    deterministic_world_hash = $referenceWorldHash
    deterministic_final_state_sha256 = $referenceFinalStateSha256
    aggregate_throughput_monotonic_with_10_percent_tolerance = `
        $monotonicThroughput
    recommended_actor_count_at_50_percent_efficiency = `
        $recommendedActorCount
    batches = $batches
}
$reportPath = Join-Path $OutputRoot "game_room_actor_scaling_v1.json"
[IO.File]::WriteAllText(
    $reportPath,
    ($report | ConvertTo-Json -Depth 10),
    $utf8WithoutBom)

Write-Host (
    "[GameRoomActorScaling] PASS: model=one-room-per-process " +
    "recommended_actors=$recommendedActorCount report=$reportPath")
