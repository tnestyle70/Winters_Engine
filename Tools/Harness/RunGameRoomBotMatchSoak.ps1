param(
    [ValidateRange(1, 4294967295)]
    [uint32]$TickCount = 1800,

    [ValidateRange(1, 9223372036854775807)]
    [int64]$Seed = 42,

    [ValidateRange(1, 16)]
    [int]$Runs = 1,

    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [ValidateRange(1, 4294967295)]
    [uint32]$HeartbeatTicks = 1800,

    [ValidateRange(64, 65536)]
    [uint64]$PrivateLimitMiB = 2048,

    [switch]$SkipServerBuild
)

$ErrorActionPreference = "Stop"

$workspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$sourcePath = Join-Path $PSScriptRoot "GameRoomBotMatchSoak.cpp"
$vsWhere = Join-Path ${env:ProgramFiles(x86)} `
    "Microsoft Visual Studio\Installer\vswhere.exe"

if (-not (Test-Path -LiteralPath $vsWhere)) {
    throw "vswhere.exe was not found: $vsWhere"
}

$installationPath = & $vsWhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
if (-not $installationPath) {
    throw "A Visual Studio C++ toolchain was not found."
}

$vsDevCmd = Join-Path $installationPath "Common7\Tools\VsDevCmd.bat"
$msbuild = Join-Path $installationPath "MSBuild\Current\Bin\MSBuild.exe"
if (-not (Test-Path -LiteralPath $vsDevCmd) -or
    -not (Test-Path -LiteralPath $msbuild)) {
    throw "Visual Studio build tools are incomplete."
}

if (-not $SkipServerBuild) {
    $buildProcesses = Get-Process msbuild, cl, link -ErrorAction SilentlyContinue
    if ($buildProcesses) {
        $ids = ($buildProcesses | ForEach-Object { $_.Id }) -join ","
        throw "A compiler process already owns the build lane: $ids"
    }

    $serverProject = Join-Path $workspaceRoot "Server\Include\Server.vcxproj"
    & $msbuild $serverProject /m:1 /nr:false /t:Build `
        /p:Configuration=$Configuration /p:Platform=x64 /verbosity:minimal
    if ($LASTEXITCODE -ne 0) {
        throw "Server $Configuration x64 freshness build failed with exit code $LASTEXITCODE."
    }
}

$serverIntermediate = Join-Path $workspaceRoot `
    "Server\Bin\Intermediate\$Configuration"
$linkCommandTlog = Join-Path $serverIntermediate `
    "Server.tlog\link.command.1.tlog"
if (-not (Test-Path -LiteralPath $linkCommandTlog)) {
    throw "Server link input manifest is missing: $linkCommandTlog"
}
$linkInputs = Get-Content -LiteralPath $linkCommandTlog `
    -Encoding Unicode -TotalCount 1
if (-not $linkInputs -or -not $linkInputs.StartsWith("^")) {
    throw "Server link input manifest is invalid: $linkCommandTlog"
}
$serverIntermediatePrefix = $serverIntermediate.TrimEnd('\') + '\'
$serverObjects = @(
    $linkInputs.Substring(1).Split('|') |
        Where-Object {
            $_.EndsWith(".OBJ", [StringComparison]::OrdinalIgnoreCase) -and
            $_.StartsWith(
                $serverIntermediatePrefix,
                [StringComparison]::OrdinalIgnoreCase) -and
            -not [IO.Path]::GetFileName($_).Equals(
                "main.obj",
                [StringComparison]::OrdinalIgnoreCase)
        } |
        ForEach-Object {
            if (-not (Test-Path -LiteralPath $_)) {
                throw "Fresh Server link object is missing: $_"
            }
            Get-Item -LiteralPath $_
        } |
        Sort-Object FullName -Unique
)
if ($serverObjects.Count -eq 0) {
    throw "No Server $Configuration object files were found."
}

$gameSimLibrary = Join-Path $workspaceRoot `
    "Shared\GameSim\Bin\$Configuration\WintersGameSim.lib"
$engineLibrary = Join-Path $workspaceRoot `
    "Engine\Bin\$Configuration\WintersEngine.lib"
if (-not (Test-Path -LiteralPath $gameSimLibrary) -or
    -not (Test-Path -LiteralPath $engineLibrary)) {
    throw "The $Configuration GameSim or Engine import library is missing."
}

$outputDirectory = Join-Path $env:TEMP "WintersGameRoomBotMatchSoak"
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$executablePath = Join-Path $outputDirectory `
    "GameRoomBotMatchSoak_$Configuration.exe"
$serverPublic = Join-Path $workspaceRoot "Server\Public"
$engineSdkInclude = Join-Path $workspaceRoot "EngineSDK\inc"
$flatBuffersInclude = Join-Path $workspaceRoot `
    "Engine\ThirdPartyLib\FlatBuffers\Inc"
$quotedObjects = ($serverObjects | ForEach-Object {
    '"{0}"' -f $_.FullName
}) -join " "
$compileFlags = if ($Configuration -eq "Debug") {
    "/MDd /Od /D _DEBUG"
}
else {
    "/MD /O2 /Ob2 /GL /D NDEBUG"
}

$compileCommand = (
    '"{0}" -no_logo -arch=x64 -host_arch=x64 >nul && ' +
    'cd /d "{1}" && ' +
    'cl.exe /nologo /std:c++20 /utf-8 /EHsc /W4 /WX {6} ' +
    '/D NOMINMAX /D WIN32_LEAN_AND_MEAN ' +
    '/I"{2}" /I"{3}" /I"{4}" /I"{5}" ' +
    '"{7}" {8} "{9}" "{10}" ws2_32.lib Mswsock.lib psapi.lib /Fe:"{11}"'
) -f $vsDevCmd, $outputDirectory, $workspaceRoot, $serverPublic, `
    $engineSdkInclude, $flatBuffersInclude, $compileFlags, $sourcePath, `
    $quotedObjects, $gameSimLibrary, $engineLibrary, $executablePath

& $env:ComSpec /d /s /c $compileCommand
if ($LASTEXITCODE -ne 0) {
    throw "GameRoom bot match soak compilation failed with exit code $LASTEXITCODE."
}

$engineRuntime = Join-Path $workspaceRoot "EngineSDK\bin\$Configuration"
$evidenceRoot = Join-Path $workspaceRoot ".md\build\evidence\s024_bot_soak"
New-Item -ItemType Directory -Force -Path $evidenceRoot | Out-Null
$sessionId = "{0}_{1}" -f `
    (Get-Date -Format "yyyyMMdd_HHmmss_fff"), `
    ([Guid]::NewGuid().ToString("N").Substring(0, 8))
$caseDirectory = Join-Path $evidenceRoot `
    ("{0}_ticks_{1}_seed_{2}_{3}" -f `
        $Configuration.ToLowerInvariant(), $TickCount, $Seed, $sessionId)
New-Item -ItemType Directory -Path $caseDirectory | Out-Null
$previousPath = $env:PATH
$previousLocation = Get-Location
$results = @()
try {
    $env:PATH = "$engineRuntime;$previousPath"
    for ($run = 1; $run -le $Runs; ++$run) {
        $runDirectory = Join-Path $caseDirectory ("run_{0:D2}" -f $run)
        New-Item -ItemType Directory -Path $runDirectory | Out-Null
        Set-Location -LiteralPath $runDirectory

        # Each run has its own working directory, so a fixed room id keeps the
        # authoritative input identical without replay path collisions.
        $roomId = 2401
        $output = [System.Collections.Generic.List[string]]::new()
        & $executablePath $TickCount $Seed $roomId `
            $HeartbeatTicks $PrivateLimitMiB 2>&1 | ForEach-Object {
                $line = $_.ToString()
                $output.Add($line)
                Write-Host $line
            }
        $exitCode = $LASTEXITCODE
        $outputPath = Join-Path $runDirectory "soak_output.txt"
        $output | Set-Content -LiteralPath $outputPath -Encoding utf8
        if ($exitCode -ne 0) {
            throw "GameRoom bot match soak run $run failed with exit code $exitCode."
        }

        $resultLine = @($output | Where-Object { $_ -match '^RESULT status=PASS ' })
        if ($resultLine.Count -ne 1) {
            throw "Run $run did not emit exactly one PASS result line."
        }
        if ($resultLine[0] -notmatch 'replay_hash=([0-9A-F]{16})') {
            throw "Run $run did not emit a replay record-stream hash."
        }
        $replayHash = $Matches[1]
        if ($resultLine[0] -notmatch 'world_hash=([0-9A-F]{16})') {
            throw "Run $run did not emit a final world hash."
        }
        $results += [pscustomobject]@{
            Run = $run
            ReplayHash = $replayHash
            WorldHash = $Matches[1]
            Result = $resultLine[0]
            Output = $outputPath
        }
    }
}
finally {
    Set-Location -LiteralPath $previousLocation
    $env:PATH = $previousPath
}

if ($Runs -gt 1) {
    $distinctReplayHashes = @($results.ReplayHash | Sort-Object -Unique)
    $distinctWorldHashes = @($results.WorldHash | Sort-Object -Unique)
    if ($distinctReplayHashes.Count -ne 1 -or
        $distinctWorldHashes.Count -ne 1) {
        throw "Same-seed determinism failed: replay=$($distinctReplayHashes -join ',') world=$($distinctWorldHashes -join ',')"
    }
    Write-Host "DETERMINISM status=PASS runs=$Runs replay_hash=$($distinctReplayHashes[0]) world_hash=$($distinctWorldHashes[0])"
}

Write-Host "EVIDENCE path=$caseDirectory"
Write-Host "GameRoom bot match soak PASS: configuration=$Configuration ticks=$TickCount seed=$Seed runs=$Runs"
