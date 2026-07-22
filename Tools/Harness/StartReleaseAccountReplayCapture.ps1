param(
    [ValidateRange(1, 10)]
    [int]$HumanPlayers = 3,
    [switch]$SkipBackendBuild,
    [switch]$SkipDefinitionCheck
)

$ErrorActionPreference = "Stop"

$workspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$servicesRoot = Join-Path $workspaceRoot "Services"
$serverPath = Join-Path $workspaceRoot "Server\Bin\Release\WintersServer.exe"
$visibleServerLauncher = Join-Path $workspaceRoot "Tools\Harness\StartVisibleReleaseServer.cmd"
if (-not (Test-Path -LiteralPath $serverPath)) {
    throw "Release server was not found: $serverPath"
}
if (-not (Test-Path -LiteralPath $visibleServerLauncher)) {
    throw "Visible Release server launcher was not found: $visibleServerLauncher"
}

if (-not $SkipDefinitionCheck) {
    Push-Location $workspaceRoot
    try {
        & python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check
        if ($LASTEXITCODE -ne 0) {
            throw "Release capture blocked: LoL definition pack is stale. Run the generator and rebuild Release."
        }
        & python Tools/ChampionData/build_champion_game_data.py --root . --check
        if ($LASTEXITCODE -ne 0) {
            throw "Release capture blocked: ChampionGameData is stale. Run the generator and rebuild Release."
        }
    } finally {
        Pop-Location
    }
}

$sessionId = "winters-local-game-session"
$captureRunId = [Guid]::NewGuid().ToString()
$ticketSecret = "winters-match-ticket-secret-change-in-production"
$replayToken = "winters-replay-internal-token-change-in-production"
$logRoot = Join-Path $workspaceRoot ".md\build\release-replay-capture\$captureRunId"
New-Item -ItemType Directory -Force -Path $logRoot | Out-Null

$savedEnvironment = @{}
foreach ($name in @(
    "WINTERS_GAME_SESSION_ID",
    "WINTERS_MATCH_MAX_SIZE",
    "WINTERS_MATCH_TICKET_SECRET",
    "WINTERS_MATCHMAKING_SERVICE_URL",
    "WINTERS_MATCHMAKING_INTERNAL_TOKEN",
    "WINTERS_REPLAY_SERVICE_URL",
    "WINTERS_REPLAY_INTERNAL_TOKEN")) {
    $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, "Process")
}

function Wait-Health([string]$Url) {
    for ($attempt = 0; $attempt -lt 60; ++$attempt) {
        try {
            $response = Invoke-WebRequest -UseBasicParsing -Uri $Url -TimeoutSec 2
            if ($response.StatusCode -eq 200) {
                return
            }
        } catch {
        }
        Start-Sleep -Milliseconds 500
    }
    throw "Health check timed out: $Url"
}

function Wait-VisibleServer(
    [System.Diagnostics.Process]$ConsoleProcess) {
    for ($attempt = 0; $attempt -lt 120; ++$attempt) {
        if ($ConsoleProcess.HasExited) {
            throw "Visible Release server console exited before readiness."
        }
        $serverProcess = Get-CimInstance Win32_Process -Filter "Name = 'WintersServer.exe'" |
            Where-Object { $_.ParentProcessId -eq $ConsoleProcess.Id } |
            Select-Object -First 1
        if ($null -ne $serverProcess -and
            (Get-NetUDPEndpoint -LocalPort 9000 -ErrorAction SilentlyContinue)) {
            return $serverProcess
        }
        Start-Sleep -Milliseconds 250
    }
    throw "Visible Release server readiness timed out."
}

try {
    $env:WINTERS_GAME_SESSION_ID = $sessionId
    $env:WINTERS_MATCH_MAX_SIZE = "10"
    $env:WINTERS_MATCH_TICKET_SECRET = $ticketSecret
    $env:WINTERS_MATCHMAKING_SERVICE_URL = "http://127.0.0.1:8083"
    $env:WINTERS_MATCHMAKING_INTERNAL_TOKEN = "winters-matchmaking-internal-token-change-in-production"
    $env:WINTERS_REPLAY_SERVICE_URL = "http://127.0.0.1:8087"
    $env:WINTERS_REPLAY_INTERNAL_TOKEN = $replayToken

    Push-Location $servicesRoot
    try {
        # Dedicated local capture run: recreate backend containers while preserving
        # pgdata/miniodata because this command intentionally omits -v.
        & docker compose --profile app down --remove-orphans
        if ($LASTEXITCODE -ne 0) {
            throw "docker compose down failed with exit code $LASTEXITCODE"
        }

        $composeArguments = @("--profile", "app", "up", "-d")
        if (-not $SkipBackendBuild) {
            $composeArguments += "--build"
        }
        & docker compose @composeArguments
        if ($LASTEXITCODE -ne 0) {
            throw "docker compose failed with exit code $LASTEXITCODE"
        }
    } finally {
        Pop-Location
    }

    foreach ($url in @(
        "http://127.0.0.1:8081/health",
        "http://127.0.0.1:8083/health",
        "http://127.0.0.1:8084/health",
        "http://127.0.0.1:8087/health")) {
        Wait-Health $url
    }

    $matchmakingEnvironment = @(
        & docker inspect services-matchmaking-1 `
            --format '{{range .Config.Env}}{{println .}}{{end}}'
    )
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to inspect matchmaking container environment."
    }
    foreach ($expected in @(
        "WINTERS_GAME_SESSION_ID=$sessionId",
        "WINTERS_MATCH_MAX_SIZE=10")) {
        if ($matchmakingEnvironment -notcontains $expected) {
            throw "Matchmaking container policy mismatch: missing $expected"
        }
    }

    $runningServers = @(
        Get-Process -Name "WintersServer" -ErrorAction SilentlyContinue
    )
    if ($runningServers.Count -ne 0) {
        $runningServerIds = ($runningServers.Id -join ",")
        throw "A WintersServer process is already running; pid=$runningServerIds"
    }
    if (Get-NetUDPEndpoint -LocalPort 9000 -ErrorAction SilentlyContinue) {
        throw "UDP port 9000 is already occupied. Stop the previous game server first."
    }

    $serverCommand = '"{0}" "{1}"' -f `
        $visibleServerLauncher, $sessionId
    $serverConsole = Start-Process `
        -FilePath $env:ComSpec `
        -ArgumentList @("/d", "/s", "/k", $serverCommand) `
        -WorkingDirectory $workspaceRoot `
        -WindowStyle Normal `
        -PassThru
    $server = Wait-VisibleServer $serverConsole

    [ordered]@{
        capture_run_id = $captureRunId
        session_id = $sessionId
        requested_client_count = $HumanPlayers
        server_console_pid = $serverConsole.Id
        server_pid = $server.ProcessId
        started_at = [DateTimeOffset]::Now.ToString("o")
    } | ConvertTo-Json | Set-Content -Encoding UTF8 (Join-Path $logRoot "capture.json")

    Write-Host "Release account replay capture is ready."
    Write-Host "session=$sessionId players=$HumanPlayers serverPid=$($server.ProcessId) consolePid=$($serverConsole.Id)"
    Write-Host "Open $HumanPlayers Release clients, sign in with distinct accounts, and join the custom lobby."
    Write-Host "Type q in the visible server CMD to stop it cleanly. capture=$logRoot"
} finally {
    foreach ($entry in $savedEnvironment.GetEnumerator()) {
        [Environment]::SetEnvironmentVariable($entry.Key, $entry.Value, "Process")
    }
}
