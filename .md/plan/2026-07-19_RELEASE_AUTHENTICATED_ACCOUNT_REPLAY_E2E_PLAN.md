Session - Release 3클라 인증 매치를 넥서스 종료부터 계정별 즉시 다시보기와 독립 재생까지 닫는다
좌표: 신규 좌표 후보 · 축: C7 권위와 정합성, C8 검증이 병목
관련: 2026-07-15_AWS_BACKEND_ACCOUNT_REPLAY_LIBRARY_PLAN.md · 2026-07-16_AWS_BACKEND_ACCOUNT_REPLAY_IMPLEMENTATION_REPORT.md · 2026-07-18_REPLAY_CHRONO_TIMELINE_PROFILE_RETURN_PLAN.md/RESULT.md

# 1. 결정 기록

① 문제·제약: Release 서버 1개·온라인 계정 클라 3개에서 넥서스 종료 후 60초 안에 세 계정 모두 같은 match replay를 보이고, 각 프로세스에서 독립 재생해야 한다. 현재 결과는 `room1_tick1_16309.wrpl`, cloud 0건이다.
② 순진한 해법의 실패: 서로 다른 계정으로 로그인하거나 `Replay/*.wrpl`을 복사해도 직접 TCP 게임 세션에는 signed match identity가 없어 `m_matchID`·participant ACL·upload job이 생기지 않는다.
③ 메커니즘: 로컬 Release 촬영을 `3인 matchmaking -> 공통 game_session_id -> signed UDP ticket -> Server authenticated roster -> WRPL upload -> ready library -> download-and-play` 한 경로로 고정한다.
④ 대조: 로컬 파일을 세 클라에 복제하면 재생은 되지만 계정 ACL·DB match·upload·프로필 UX를 검증하지 못한다. 사용자 결정에 따라 camera track 기록과 replay 시작 camera 강제 초기화는 모두 제외하고 현재 replay camera 동작을 그대로 유지한다.
⑤ 대가: 프로필은 ready 전까지 1초 polling을 최대 60회 수행한다(3클라 최대 180회). 고정 game session은 로컬 단일 매치 촬영용이며, 동시 다중 game server allocator가 붙으면 allocation별 session id로 교체해야 한다.

## 현재 코드 증거와 범위 판정

- `Server/Private/Game/GameRoomReplication.cpp`는 `m_matchID.empty()`이면 `room...wrpl`을 만들고 upload enqueue를 건너뛴다.
- `Server/Private/Game/GameRoomLobby.cpp`는 signed network identity가 있을 때만 `m_matchID`, `m_gameSessionID`, `m_userIDBySession`을 채운다.
- `Client/Private/Network/Client/GameSessionClient.cpp`는 `MatchAssignment`가 없으면 기본 TCP, 있으면 assignment의 transport/ticket을 사용한다.
- `Services/internal/matchmaking/service.go`는 현재 `matchSize = 2`, `gameSessionID = matchID` 고정이라, 미리 떠 있는 촬영 서버 1개와 3클라를 같은 세션으로 묶을 수 없다.
- `Client/Private/Scene/Scene_MyInfo.cpp`는 cloud list를 한 번 요청하고 `Download`만 제공한다. upload가 늦게 ready가 되면 수동 새로고침이 필요하다.
- WRPL v2 record type은 Snapshot/Event/Command뿐이다. camera eye/at/input record가 없으므로 동일 월드 tick은 복원하지만 원본 플레이어 POV는 복원하지 않는다.
- Compose YAML에는 Redis named volume이 없지만 실제 Redis image는 anonymous volume을 소유한다. capture launcher는 `docker compose down --remove-orphans` 후 PostgreSQL/MinIO named volume을 보존하고, 재기동 뒤 `matchmaking:*` key만 명시적으로 삭제·재검증한다.

## 소유권 경계

```text
Match/participant truth: PostgreSQL + Matchmaking + signed ticket
Gameplay/replay truth:   Server GameRoom + canonical WRPL
Replay ACL/storage:      Replay Service + MinIO/S3
Profile presentation:    Client MyInfo + account ReplayCache
Camera presentation:     기존 Client replay camera 유지(이번 범위에서 변경 없음)
```

- Server/GameSim truth를 Client가 self-report하지 않는다.
- camera track/pose 동일성은 acceptance 대상이 아니다. 각 프로세스의 기존 local camera 조작은 replay gameplay state나 다른 클라이언트에 전송되지 않는다.
- `로컬/디버그` 탭은 개발 폴더 진입점으로 유지하되 정상 Release acceptance에는 사용하지 않는다.
- capture launcher는 로컬 backend 컨테이너를 재시작한다. `down`에 `-v`를 절대 붙이지 않아 계정·match·replay 영속 데이터는 지우지 않는다.

## 단계와 예산

- 바닥 70%: 3인 assignment, signed identity, upload 환경, ready polling, checksum download, Release 빌드와 DB/로그 검증.
- 천장 30%: 실제 Release 3화면 촬영에서 넥서스 종료→프로필→각자 다시보기→동일 tick/독립 pause·seek·speed 조작을 한 번에 증명한다.
- 외부 마감 제안: 2026-07-20 첫 3클라 Release 증거 영상과 로그 묶음을 남긴다. 기능을 더 넓히기 전에 이 계획의 이해를 영상으로 환전한다.

# 2. 반영해야 하는 코드

## 2-1. C:/Users/user/Desktop/Winters/Services/pkg/config/config.go

`GameSessionConfig` 기존 코드:

```go
type GameSessionConfig struct {
	Host         string
	Port         int
	Transport    string
	TicketSecret string
	TicketTTL    time.Duration
}
```

아래로 교체:

```go
type GameSessionConfig struct {
	Host          string
	Port          int
	Transport     string
	TicketSecret  string
	TicketTTL     time.Duration
	GameSessionID string
	MatchSize     int
}
```

`gamePort` 파싱 바로 아래 기존 코드:

```go
	gamePort, _ := strconv.Atoi(getEnv("WINTERS_GAME_PORT", "9000"))
	matchTicketTTL, _ := time.ParseDuration(getEnv("WINTERS_MATCH_TICKET_TTL", "5m"))
```

아래로 교체:

```go
	gamePort, _ := strconv.Atoi(getEnv("WINTERS_GAME_PORT", "9000"))
	matchSize, _ := strconv.Atoi(getEnv("WINTERS_MATCH_SIZE", "2"))
	matchTicketTTL, _ := time.ParseDuration(getEnv("WINTERS_MATCH_TICKET_TTL", "5m"))
```

`GameSession` 초기화 기존 코드:

```go
		GameSession: GameSessionConfig{
			Host:         getEnv("WINTERS_GAME_HOST", "127.0.0.1"),
			Port:         gamePort,
			Transport:    getEnv("WINTERS_GAME_TRANSPORT", "udp"),
			TicketSecret: getEnv("WINTERS_MATCH_TICKET_SECRET", "winters-match-ticket-secret-change-in-production"),
			TicketTTL:    matchTicketTTL,
		},
```

아래로 교체:

```go
		GameSession: GameSessionConfig{
			Host:          getEnv("WINTERS_GAME_HOST", "127.0.0.1"),
			Port:          gamePort,
			Transport:     getEnv("WINTERS_GAME_TRANSPORT", "udp"),
			TicketSecret:  getEnv("WINTERS_MATCH_TICKET_SECRET", "winters-match-ticket-secret-change-in-production"),
			TicketTTL:     matchTicketTTL,
			GameSessionID: getEnv("WINTERS_GAME_SESSION_ID", ""),
			MatchSize:     matchSize,
		},
```

현재 `TicketTTL` 검증과 production 분기 사이 block:

```go
	if cfg.GameSession.TicketTTL <= 0 {
		return nil, fmt.Errorf("WINTERS_MATCH_TICKET_TTL must be positive")
	}
	if cfg.Environment == "production" {
```

아래로 교체:

```go
	if cfg.GameSession.TicketTTL <= 0 {
		return nil, fmt.Errorf("WINTERS_MATCH_TICKET_TTL must be positive")
	}
	if cfg.GameSession.MatchSize < 2 || cfg.GameSession.MatchSize > 10 {
		return nil, fmt.Errorf("WINTERS_MATCH_SIZE must be between 2 and 10")
	}
	if len(cfg.GameSession.GameSessionID) > 64 {
		return nil, fmt.Errorf("WINTERS_GAME_SESSION_ID must not exceed 64 bytes")
	}
	if cfg.Environment == "production" {
```

## 2-2. C:/Users/user/Desktop/Winters/Services/pkg/config/config_test.go

파일 끝의 기존 함수 전체:

```go
func TestLoadDevelopmentAllowsSeparatePublicMinIOEndpoint(t *testing.T) {
	t.Setenv("WINTERS_ENV", "development")
	t.Setenv("S3_ENDPOINT", "http://minio:9000")
	t.Setenv("S3_PUBLIC_ENDPOINT", "http://localhost:9000")

	cfg, err := Load()
	if err != nil {
		t.Fatalf("Load() error = %v", err)
	}
	if cfg.Replay.Endpoint != "http://minio:9000" || cfg.Replay.PublicEndpoint != "http://localhost:9000" {
		t.Fatalf("development S3 endpoints = %q/%q", cfg.Replay.Endpoint, cfg.Replay.PublicEndpoint)
	}
}
```

위 블록을 아래로 교체해 기존 테스트와 신규 테스트 두 개를 한 anchor에서 고정한다:

```go
func TestLoadDevelopmentAllowsSeparatePublicMinIOEndpoint(t *testing.T) {
	t.Setenv("WINTERS_ENV", "development")
	t.Setenv("S3_ENDPOINT", "http://minio:9000")
	t.Setenv("S3_PUBLIC_ENDPOINT", "http://localhost:9000")

	cfg, err := Load()
	if err != nil {
		t.Fatalf("Load() error = %v", err)
	}
	if cfg.Replay.Endpoint != "http://minio:9000" || cfg.Replay.PublicEndpoint != "http://localhost:9000" {
		t.Fatalf("development S3 endpoints = %q/%q", cfg.Replay.Endpoint, cfg.Replay.PublicEndpoint)
	}
}

func TestLoadGameSessionCaptureOverrides(t *testing.T) {
	t.Setenv("WINTERS_ENV", "development")
	t.Setenv("WINTERS_GAME_SESSION_ID", "capture-session-01")
	t.Setenv("WINTERS_MATCH_SIZE", "3")

	cfg, err := Load()
	if err != nil {
		t.Fatalf("Load() error = %v", err)
	}
	if cfg.GameSession.GameSessionID != "capture-session-01" ||
		cfg.GameSession.MatchSize != 3 {
		t.Fatalf("capture config = %q/%d",
			cfg.GameSession.GameSessionID,
			cfg.GameSession.MatchSize)
	}
}

func TestLoadRejectsInvalidMatchSize(t *testing.T) {
	t.Setenv("WINTERS_ENV", "development")
	t.Setenv("WINTERS_MATCH_SIZE", "1")

	if _, err := Load(); err == nil {
		t.Fatal("Load() accepted WINTERS_MATCH_SIZE=1")
	}
}
```

## 2-3. C:/Users/user/Desktop/Winters/Services/internal/matchmaking/model.go

`GameAllocation` 기존 코드:

```go
type GameAllocation struct {
	Host      string
	Port      int
	Transport string
}
```

아래로 교체:

```go
type GameAllocation struct {
	Host          string
	Port          int
	Transport     string
	GameSessionID string
}
```

## 2-4. C:/Users/user/Desktop/Winters/Services/internal/matchmaking/service.go

상수 묶음에서 아래 줄 삭제:

```go
	matchSize      = 2
```

`Service` 기존 코드:

```go
type Service struct {
	db           *pgxpool.Pool
	rdb          *redis.Client
	writer       *kafka.Writer
	ticketSigner *matchticket.Signer
	allocation   GameAllocation
}
```

아래로 교체:

```go
type Service struct {
	db           *pgxpool.Pool
	rdb          *redis.Client
	writer       *kafka.Writer
	ticketSigner *matchticket.Signer
	allocation   GameAllocation
	matchSize    int
}
```

`NewService`의 현재 전체 block:

```go
func NewService(
	db *pgxpool.Pool,
	rdb *redis.Client,
	writer *kafka.Writer,
	ticketSigner *matchticket.Signer,
	allocation GameAllocation,
) *Service {
	return &Service{
		db:           db,
		rdb:          rdb,
		writer:       writer,
		ticketSigner: ticketSigner,
		allocation:   allocation,
	}
}
```

아래로 교체:

```go
func NewService(
	db *pgxpool.Pool,
	rdb *redis.Client,
	writer *kafka.Writer,
	ticketSigner *matchticket.Signer,
	allocation GameAllocation,
	matchSize int,
) *Service {
	return &Service{
		db:           db,
		rdb:          rdb,
		writer:       writer,
		ticketSigner: ticketSigner,
		allocation:   allocation,
		matchSize:    matchSize,
	}
}
```

`tryMatch` 첫 현재 block:

```go
	results, err := s.rdb.ZRangeWithScores(ctx, queueKey, 0, -1).Result()
	if err != nil || len(results) < matchSize {
		return
	}
```

아래로 교체:

```go
	results, err := s.rdb.ZRangeWithScores(ctx, queueKey, 0, -1).Result()
	if err != nil || len(results) < s.matchSize {
		return
	}
```

group 확장 loop의 현재 줄:

```go
		for j := i + 1; j < len(results) && len(group) < matchSize; j++ {
```

아래로 교체:

```go
		for j := i + 1; j < len(results) && len(group) < s.matchSize; j++ {
```

group 완성 판정의 현재 줄:

```go
		if len(group) >= matchSize {
```

아래로 교체:

```go
		if len(group) >= s.matchSize {
```

`createMatch`의 match/session 생성 기존 코드:

```go
	matchID := uuid.New()
	gameSessionID := matchID.String()
```

아래로 교체:

```go
	matchID := uuid.New()
	gameSessionID := s.allocation.GameSessionID
	if gameSessionID == "" {
		gameSessionID = matchID.String()
	}
```

## 2-5. C:/Users/user/Desktop/Winters/Services/cmd/matchmaking/main.go

`matchmaking.NewService` 호출 기존 코드:

```go
	svc := matchmaking.NewService(
		db,
		rdb,
		writer,
		ticketSigner,
		matchmaking.GameAllocation{
			Host:      cfg.GameSession.Host,
			Port:      cfg.GameSession.Port,
			Transport: cfg.GameSession.Transport,
		})
```

아래로 교체:

```go
	svc := matchmaking.NewService(
		db,
		rdb,
		writer,
		ticketSigner,
		matchmaking.GameAllocation{
			Host:          cfg.GameSession.Host,
			Port:          cfg.GameSession.Port,
			Transport:     cfg.GameSession.Transport,
			GameSessionID: cfg.GameSession.GameSessionID,
		},
		cfg.GameSession.MatchSize)
```

## 2-6. C:/Users/user/Desktop/Winters/Services/docker-compose.yml

현재 game environment 연속 block:

```yaml
  WINTERS_GAME_HOST: 127.0.0.1
  WINTERS_GAME_PORT: "9000"
  WINTERS_GAME_TRANSPORT: udp
```

아래로 교체:

```yaml
  WINTERS_GAME_HOST: 127.0.0.1
  WINTERS_GAME_PORT: "9000"
  WINTERS_GAME_TRANSPORT: udp
  WINTERS_GAME_SESSION_ID: "${WINTERS_GAME_SESSION_ID:-}"
  WINTERS_MATCH_SIZE: "${WINTERS_MATCH_SIZE:-2}"
```

기본 Compose 실행은 여전히 2인/dynamic session이다. 아래 새 Release capture launcher만 같은 고정 session과 3인을 주입한다.

## 2-7. 새 파일: C:/Users/user/Desktop/Winters/Tools/Harness/StartReleaseAccountReplayCapture.ps1

전체 파일 본문:

```powershell
param(
    [ValidateRange(2, 10)]
    [int]$HumanPlayers = 3,
    [ValidateRange(60, 86400)]
    [int]$ServerLifetimeSeconds = 21600,
    [switch]$SkipBackendBuild
)

$ErrorActionPreference = "Stop"

$workspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$servicesRoot = Join-Path $workspaceRoot "Services"
$serverPath = Join-Path $workspaceRoot "Server\Bin\Release\WintersServer.exe"
if (-not (Test-Path -LiteralPath $serverPath)) {
    throw "Release server was not found: $serverPath"
}

$sessionId = [Guid]::NewGuid().ToString()
$ticketSecret = "winters-match-ticket-secret-change-in-production"
$replayToken = "winters-replay-internal-token-change-in-production"
$logRoot = Join-Path $workspaceRoot ".md\build\release-replay-capture\$sessionId"
New-Item -ItemType Directory -Force -Path $logRoot | Out-Null

$savedEnvironment = @{}
foreach ($name in @(
    "WINTERS_GAME_SESSION_ID",
    "WINTERS_MATCH_SIZE",
    "WINTERS_MATCH_TICKET_SECRET",
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

function Wait-FileText(
    [string]$Path,
    [string]$Pattern,
    [System.Diagnostics.Process]$Process) {
    for ($attempt = 0; $attempt -lt 120; ++$attempt) {
        if ($Process.HasExited) {
            throw "Release server exited before readiness. See $Path"
        }
        if ((Test-Path -LiteralPath $Path) -and
            (Select-String -LiteralPath $Path -SimpleMatch $Pattern -Quiet)) {
            return
        }
        Start-Sleep -Milliseconds 250
    }
    throw "Server readiness log timed out: $Pattern"
}

try {
    $env:WINTERS_GAME_SESSION_ID = $sessionId
    $env:WINTERS_MATCH_SIZE = $HumanPlayers.ToString()
    $env:WINTERS_MATCH_TICKET_SECRET = $ticketSecret
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

    # The Redis image owns an anonymous data volume even though compose declares no
    # named Redis volume. Clear matchmaking-only state explicitly so a previous
    # capture cannot leak queue/status/jointime into this session. Do not FLUSHDB:
    # unrelated local cache data is outside this launcher's ownership.
    $matchmakingKeys = @(
        & docker exec winters-redis redis-cli --raw --scan --pattern "matchmaking:*"
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to enumerate stale matchmaking Redis keys."
    }
    foreach ($key in $matchmakingKeys) {
        & docker exec winters-redis redis-cli --raw DEL $key | Out-Null
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to delete stale matchmaking Redis key: $key"
        }
    }

    $remainingMatchmakingKeys = @(
        & docker exec winters-redis redis-cli --raw --scan --pattern "matchmaking:*"
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    if ($LASTEXITCODE -ne 0 -or $remainingMatchmakingKeys.Count -ne 0) {
        throw "Capture requires empty matchmaking Redis state; remaining=$($remainingMatchmakingKeys.Count)"
    }

    $queueOutput = & docker exec winters-redis redis-cli --raw ZCARD matchmaking:queue
    $queueCount = ($queueOutput | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or $queueCount -ne "0") {
        throw "Capture requires an empty matchmaking queue; actual=$queueCount"
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

    $stdoutPath = Join-Path $logRoot "server.stdout.log"
    $stderrPath = Join-Path $logRoot "server.stderr.log"
    $server = Start-Process `
        -FilePath $serverPath `
        -ArgumentList @(
            "--net-transport=udp",
            "--smoke-seconds=$ServerLifetimeSeconds") `
        -WorkingDirectory $workspaceRoot `
        -WindowStyle Hidden `
        -RedirectStandardOutput $stdoutPath `
        -RedirectStandardError $stderrPath `
        -PassThru

    Wait-FileText $stdoutPath `
        "[Server] UDP signed match-ticket authentication enabled gameSession=$sessionId" `
        $server
    Wait-FileText $stdoutPath `
        "[Server] WintersServer v0.3 transport=udp" `
        $server

    [ordered]@{
        session_id = $sessionId
        expected_human_players = $HumanPlayers
        server_lifetime_seconds = $ServerLifetimeSeconds
        server_pid = $server.Id
        server_stdout = $stdoutPath
        server_stderr = $stderrPath
        matchmaking_queue_before_join = 0
        matchmaking_keys_cleared = $matchmakingKeys.Count
        started_at = [DateTimeOffset]::Now.ToString("o")
    } | ConvertTo-Json | Set-Content -Encoding UTF8 (Join-Path $logRoot "capture.json")

    Write-Host "Release account replay capture is ready."
    Write-Host "session=$sessionId players=$HumanPlayers serverPid=$($server.Id)"
    Write-Host "Open $HumanPlayers Release clients, sign in with distinct accounts, and join matchmaking."
    Write-Host "logs=$logRoot"
} finally {
    foreach ($entry in $savedEnvironment.GetEnumerator()) {
        [Environment]::SetEnvironmentVariable($entry.Key, $entry.Value, "Process")
    }
}
```

이 파일은 계정·ticket 원문을 로그에 쓰지 않는다. `down`은 이 전용 launcher의 명시적 local-capture 동작이며 `-v` 없이 실행해 account PostgreSQL과 replay MinIO를 보존한다. Redis anonymous volume 잔존 여부에 의존하지 않고 `matchmaking:*` key만 선택 삭제하며 `FLUSHDB`는 사용하지 않는다. redirected `Start-Process`는 interactive stdin EOF로 서버가 즉시 종료되므로 기존 `--smoke-seconds` 실행 모드를 6시간 기본값으로 사용하고 PID로 조기 종료할 수 있게 한다. 새 서버 시작 전 `WintersServer` process와 UDP 9000 endpoint를 이중 검사해 숨은 이전 서버와 bind race를 거부한다. 기존 직접 TCP 실행은 local replay smoke로만 남고 이 계획의 합격 경로가 아니다.

## 2-8. C:/Users/user/Desktop/Winters/Server/Private/main.cpp

signed ticket validator 설정의 기존 성공 로그:

```cpp
            std::cout << "[Server] UDP signed match-ticket authentication enabled"
                << " gameSession=" << runtimeOptions.gameSessionID << "\n";
```

launcher가 redirected stdout에서 실제 session 준비를 기다릴 수 있도록 아래로 교체:

```cpp
            std::cout << "[Server] UDP signed match-ticket authentication enabled"
                << " gameSession=" << runtimeOptions.gameSessionID << std::endl;
```

Replay upload queue 시작 기존 코드:

```cpp
    if (replayUploadQueue.IsEnabled())
        std::cout << "[Server] Replay upload queue enabled\n";
```

아래로 교체:

```cpp
    if (replayUploadQueue.IsEnabled())
        std::cout << "[Server] Replay upload queue enabled" << std::endl;
    else
        std::cout << "[Server] Replay upload queue disabled; local artifacts only" << std::endl;
```

transport 시작 성공 로그의 기존 코드:

```cpp
    std::cout << "[Server] WintersServer v0.3 transport="
        << NetworkModeName(runtimeOptions.networkMode)
        << " jobMode=" << JobModeName(runtimeOptions.jobMode)
        << " jobWorkers=" << runtimeOptions.jobWorkerCount
        << " endpoint=0.0.0.0:9000\n";
```

아래로 교체해 UDP bind 성공 뒤 readiness marker가 즉시 파일에 flush되게 한다:

```cpp
    std::cout << "[Server] WintersServer v0.3 transport="
        << NetworkModeName(runtimeOptions.networkMode)
        << " jobMode=" << JobModeName(runtimeOptions.jobMode)
        << " jobWorkers=" << runtimeOptions.jobWorkerCount
        << " endpoint=0.0.0.0:9000" << std::endl;
```

## 2-9. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomReplication.cpp

artifact size 계산부터 enqueue 분기까지 기존 코드:

```cpp
        std::error_code sizeError;
        artifact.sizeBytes = std::filesystem::file_size(path, sizeError);
        if (!artifact.matchID.empty() && bHasCompleteRoster && !sizeError)
        {
            if (!CReplayUploadQueue::Instance().Enqueue(std::move(artifact)))
            {
                const char* uploadMessage =
                    "[ReplayUpload] artifact retained locally: queue disabled or full\n";
                OutputDebugStringA(uploadMessage);
                std::cout << uploadMessage;
            }
        }
        else if (!artifact.matchID.empty())
        {
            const char* uploadMessage =
                "[ReplayUpload] artifact retained locally: match result or roster incomplete\n";
            OutputDebugStringA(uploadMessage);
            std::cout << uploadMessage;
        }
```

아래로 교체:

```cpp
        std::error_code sizeError;
        artifact.sizeBytes = std::filesystem::file_size(path, sizeError);
        if (artifact.matchID.empty())
        {
            std::cerr << "[ReplayUpload] skipped reason=missing_authenticated_match_identity"
                << " path=" << std::filesystem::path(path).string() << '\n';
        }
        else if (!bHasCompleteRoster)
        {
            std::cerr << "[ReplayUpload] skipped reason=incomplete_authenticated_roster"
                << " match=" << artifact.matchID
                << " participants=" << artifact.participants.size() << '\n';
        }
        else if (sizeError)
        {
            std::cerr << "[ReplayUpload] skipped reason=file_size_failed"
                << " match=" << artifact.matchID << '\n';
        }
        else
        {
            const std::string queuedMatchID = artifact.matchID;
            const size_t participantCount = artifact.participants.size();
            if (CReplayUploadQueue::Instance().Enqueue(std::move(artifact)))
            {
                std::cout << "[ReplayUpload] queued match=" << queuedMatchID
                    << " participants=" << participantCount << std::endl;
            }
            else
            {
                std::cerr << "[ReplayUpload] retained reason=queue_disabled_full_or_persist_failed"
                    << " match=" << queuedMatchID << '\n';
            }
        }
```

Release console/stdout만으로 local save, enqueue, ready를 구분할 수 있어야 한다. `OutputDebugString`은 성공 판정 근거로 쓰지 않는다.

## 2-9-A. C:/Users/user/Desktop/Winters/Server/Private/Backend/ReplayUploadQueue.cpp

upload worker의 현재 결과 로그 block:

```cpp
            const std::string text = message.str();
            OutputDebugStringA(text.c_str());
            std::cout << text;
```

아래로 교체해 redirected Release stdout에도 ready/deferred가 즉시 보이게 한다:

```cpp
            const std::string text = message.str();
            OutputDebugStringA(text.c_str());
            std::cout << text << std::flush;
```

## 2-10. C:/Users/user/Desktop/Winters/Client/Public/ClientShell/ClientShellBackendService.h

public replay API 기존 코드:

```cpp
	void RequestReplayLibrary();
	void RequestPostMatchRefresh();
	void RequestReplayDownload(const Client::CloudReplayItem& item);
```

아래로 교체:

```cpp
	void RequestReplayLibrary();
	void RequestPostMatchRefresh();
	void RequestReplayDownload(const Client::CloudReplayItem& item);
	void RequestReplayPlayback(const Client::CloudReplayItem& item);
	bool_t ConsumeReplayPlaybackPath(wstring_t& outPath);
	void CancelReplayPlaybackIntent();
```

private helper의 현재 연속 블록:

```cpp
	bool_t HasInFlightRequests() const;
	void RequestProfileSync();
	void TryStartPostMatchRefresh();
	void ApplyMatchStatus(const Client::MatchStatus& status);
	void DestroyClients();
	void TryFinishDeferredReset();
```

아래로 교체:

```cpp
	bool_t HasInFlightRequests() const;
	void RequestProfileSync();
	void TryStartPostMatchRefresh();
	void ApplyMatchStatus(const Client::MatchStatus& status);
	void DestroyClients();
	void TryFinishDeferredReset();
	void BeginReplayDownload(
		const Client::CloudReplayItem& item,
		bool_t bOpenAfterDownload);
```

현재 replay vector/user/status 멤버 블록:

```cpp
	std::vector<Client::CloudReplayItem> m_vCloudReplayItems{};
	std::string m_strUserID{};
	std::string m_strStatus{};
```

아래로 교체:

```cpp
	std::vector<Client::CloudReplayItem> m_vCloudReplayItems{};
	std::string m_strUserID{};
	std::string m_strStatus{};
	wstring_t m_strReadyReplayPlaybackPath{};
	u32_t m_uReplayPlaybackIntent = 0u;
```

## 2-11. C:/Users/user/Desktop/Winters/Client/Private/ClientShell/ClientShellBackendService.cpp

현재 표준 include block:

```cpp
#include "ClientShell/ClientShellDataStore.h"

#include <cstdlib>
```

아래로 교체:

```cpp
#include "ClientShell/ClientShellDataStore.h"

#include <cstdlib>
#include <utility>
```

`RequestReplayDownload`의 현재 전체 block:

```cpp
void CClientShellBackendService::RequestReplayDownload(
	const Client::CloudReplayItem& item)
{
	if (!m_bConfigured || !m_pReplayClient || m_bReplayRequestInFlight)
		return;

	m_bReplayRequestInFlight = true;
	m_strStatus = "Downloading replay...";
	const u32_t uGeneration = m_uGeneration;
	m_pReplayClient->DownloadMine(
		item,
		m_strUserID,
		[this, uGeneration](const Client::ReplayDownloadResult& result)
		{
			m_bReplayRequestInFlight = false;
			if (uGeneration == m_uGeneration)
			{
				if (result.success)
				{
					++m_uReplayLibraryRevision;
					m_strStatus = "Replay downloaded to account cache";
				}
				else
				{
					m_strStatus = result.error.empty()
						? "Replay download failed"
						: "Replay download failed: " + result.error;
				}
			}
			TryFinishDeferredReset();
		});
}
```

위 block을 아래로 교체하고 이어서 신규 메서드를 추가:

```cpp
void CClientShellBackendService::RequestReplayDownload(
	const Client::CloudReplayItem& item)
{
	BeginReplayDownload(item, false);
}

void CClientShellBackendService::RequestReplayPlayback(
	const Client::CloudReplayItem& item)
{
	BeginReplayDownload(item, true);
}

void CClientShellBackendService::BeginReplayDownload(
	const Client::CloudReplayItem& item,
	bool_t bOpenAfterDownload)
{
	if (!m_bConfigured || !m_pReplayClient || m_bReplayRequestInFlight)
		return;

	m_bReplayRequestInFlight = true;
	m_strStatus = bOpenAfterDownload
		? "Preparing replay playback..."
		: "Downloading replay...";
	const u32_t uGeneration = m_uGeneration;
	const u32_t uPlaybackIntent = bOpenAfterDownload
		? ++m_uReplayPlaybackIntent
		: m_uReplayPlaybackIntent;
	m_pReplayClient->DownloadMine(
		item,
		m_strUserID,
		[this, uGeneration, uPlaybackIntent, bOpenAfterDownload](
			const Client::ReplayDownloadResult& result)
		{
			m_bReplayRequestInFlight = false;
			if (uGeneration == m_uGeneration)
			{
				if (result.success)
				{
					++m_uReplayLibraryRevision;
					m_strStatus = "Replay downloaded to account cache";
					if (bOpenAfterDownload &&
						uPlaybackIntent == m_uReplayPlaybackIntent)
					{
						m_strReadyReplayPlaybackPath = result.localPath;
					}
				}
				else
				{
					m_strStatus = result.error.empty()
						? "Replay download failed"
						: "Replay download failed: " + result.error;
				}
			}
			TryFinishDeferredReset();
		});
}

bool_t CClientShellBackendService::ConsumeReplayPlaybackPath(
	wstring_t& outPath)
{
	if (m_strReadyReplayPlaybackPath.empty())
		return false;
	outPath = std::move(m_strReadyReplayPlaybackPath);
	m_strReadyReplayPlaybackPath.clear();
	return !outPath.empty();
}

void CClientShellBackendService::CancelReplayPlaybackIntent()
{
	++m_uReplayPlaybackIntent;
	m_strReadyReplayPlaybackPath.clear();
}
```

`DestroyClients`의 현재 전체 블록:

```cpp
void CClientShellBackendService::DestroyClients()
{
	m_pProfileClient.reset();
	m_pShopClient.reset();
	m_pMatchClient.reset();
	m_pReplayClient.reset();
	m_vCloudReplayItems.clear();
	m_bProfileRequestInFlight = false;
	m_bStoreRequestInFlight = false;
	m_bPurchaseRequestInFlight = false;
	m_bMatchRequestInFlight = false;
	m_bReplayRequestInFlight = false;
	m_bPostMatchRefreshPending = false;
	m_bMatchHistoryRefreshPending = false;
	m_bResetAfterCallbacks = false;
}
```

아래로 교체:

```cpp
void CClientShellBackendService::DestroyClients()
{
	m_pProfileClient.reset();
	m_pShopClient.reset();
	m_pMatchClient.reset();
	m_pReplayClient.reset();
	m_vCloudReplayItems.clear();
	m_strReadyReplayPlaybackPath.clear();
	++m_uReplayPlaybackIntent;
	m_bProfileRequestInFlight = false;
	m_bStoreRequestInFlight = false;
	m_bPurchaseRequestInFlight = false;
	m_bMatchRequestInFlight = false;
	m_bReplayRequestInFlight = false;
	m_bPostMatchRefreshPending = false;
	m_bMatchHistoryRefreshPending = false;
	m_bResetAfterCallbacks = false;
}
```

## 2-12. C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_MyInfo.h

private helper의 현재 연속 블록:

```cpp
	void ReloadLocalMatchRecords();
	void ReloadReplayItems();
	void DrawCloudReplayItems();
	void DrawReplayItems(const std::vector<ReplayListItem>& items, const char* pEmptyText);
	void OpenReplay(const wstring_t& path);
	void ChangeToMainMenu();
```

아래로 교체:

```cpp
	void ReloadLocalMatchRecords();
	void ReloadReplayItems();
	void DrawCloudReplayItems();
	void DrawReplayItems(const std::vector<ReplayListItem>& items, const char* pEmptyText);
	void OpenReplay(const wstring_t& path);
	void ChangeToMainMenu();
	bool_t HasExpectedCloudReplay() const;
	void UpdateExpectedReplayPolling(f32_t dt);
```

현재 마지막 상태 멤버 블록:

```cpp
	bool_t m_bBackRequested = false;
	bool_t m_bSceneTransitionStarted = false;
	u32_t m_uObservedReplayLibraryRevision = 0u;
```

아래로 교체:

```cpp
	bool_t m_bBackRequested = false;
	bool_t m_bSceneTransitionStarted = false;
	u32_t m_uObservedReplayLibraryRevision = 0u;
	std::string m_strExpectedReplayMatchID{};
	f32_t m_fReplayReadyPollRemainingSec = 0.f;
	f32_t m_fReplayReadyPollCooldownSec = 0.f;
```

## 2-13. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_MyInfo.cpp

namespace 상수의 현재 블록:

```cpp
namespace
{
	constexpr ImageSourceRect kBackButtonRect{ 30.f, 24.f, 170.f, 64.f };
}
```

아래로 교체:

```cpp
namespace
{
	constexpr ImageSourceRect kBackButtonRect{ 30.f, 24.f, 170.f, 64.f };
	constexpr f32_t kReplayReadyPollIntervalSec = 1.f;
	constexpr f32_t kReplayReadyPollTimeoutSec = 60.f;
}
```

`OnEnter`의 현재 전체 블록:

```cpp
bool CScene_MyInfo::OnEnter()
{
	m_ImageUI.Initialize(
		L"Texture/UI/MatchLoadingBackground.png",
		g_iWinSizeX,
		g_iWinSizeY);

	m_bBackRequested = false;
	m_bSceneTransitionStarted = false;
	ReloadReplayItems();
	ReloadLocalMatchRecords();

	CClientShellBackendService& backend = CClientShellBackendService::Instance();
	m_uObservedReplayLibraryRevision = backend.GetReplayLibraryRevision();
	backend.RequestMatchHistory();
	backend.RequestReplayLibrary();
	return true;
}
```

아래로 교체:

```cpp
bool CScene_MyInfo::OnEnter()
{
	m_ImageUI.Initialize(
		L"Texture/UI/MatchLoadingBackground.png",
		g_iWinSizeX,
		g_iWinSizeY);

	m_bBackRequested = false;
	m_bSceneTransitionStarted = false;
	ReloadReplayItems();
	ReloadLocalMatchRecords();

	const CClientShellSession& session = CClientShellSession::Instance();
	m_strExpectedReplayMatchID = session.HasMatchAssignment()
		? session.GetMatchAssignment().strMatchID
		: std::string{};
	m_fReplayReadyPollRemainingSec = m_strExpectedReplayMatchID.empty()
		? 0.f
		: kReplayReadyPollTimeoutSec;
	m_fReplayReadyPollCooldownSec = 0.f;

	CClientShellBackendService& backend = CClientShellBackendService::Instance();
	m_uObservedReplayLibraryRevision = backend.GetReplayLibraryRevision();
	backend.RequestMatchHistory();
	backend.RequestReplayLibrary();
	return true;
}
```

`OnExit`의 현재 전체 블록:

```cpp
void CScene_MyInfo::OnExit()
{
	m_ImageUI.Shutdown();
	m_vAccountReplayItems.clear();
	m_vDebugReplayItems.clear();
	m_vLocalMatchRecords.clear();
}
```

아래로 교체:

```cpp
void CScene_MyInfo::OnExit()
{
	CClientShellBackendService::Instance().CancelReplayPlaybackIntent();
	m_ImageUI.Shutdown();
	m_vAccountReplayItems.clear();
	m_vDebugReplayItems.clear();
	m_vLocalMatchRecords.clear();
}
```

`OnUpdate`의 현재 전체 block:

```cpp
void CScene_MyInfo::OnUpdate(f32_t /*dt*/)
{
	CClientShellBackendService::Instance().ProcessCallbacks();
	const u32_t replayRevision =
		CClientShellBackendService::Instance().GetReplayLibraryRevision();
	if (m_uObservedReplayLibraryRevision != replayRevision)
	{
		m_uObservedReplayLibraryRevision = replayRevision;
		ReloadReplayItems();
	}

	if (m_ImageUI.WasSourceRectClicked(kBackButtonRect))
		m_bBackRequested = true;

	if (m_bBackRequested && !m_bSceneTransitionStarted)
	{
		m_bBackRequested = false;
		ChangeToMainMenu();
		return;
	}
}
```

아래로 교체:

```cpp
void CScene_MyInfo::OnUpdate(f32_t dt)
{
	CClientShellBackendService& backend =
		CClientShellBackendService::Instance();
	backend.ProcessCallbacks();

	wstring_t replayPath;
	if (!m_bSceneTransitionStarted &&
		backend.ConsumeReplayPlaybackPath(replayPath))
	{
		OpenReplay(replayPath);
		return;
	}

	const u32_t replayRevision = backend.GetReplayLibraryRevision();
	if (m_uObservedReplayLibraryRevision != replayRevision)
	{
		m_uObservedReplayLibraryRevision = replayRevision;
		ReloadReplayItems();
	}
	UpdateExpectedReplayPolling(dt);

	if (m_ImageUI.WasSourceRectClicked(kBackButtonRect))
		m_bBackRequested = true;

	if (m_bBackRequested && !m_bSceneTransitionStarted)
	{
		m_bBackRequested = false;
		ChangeToMainMenu();
	}
}
```

현재 helper 삽입 anchor:

```cpp
void CScene_MyInfo::DrawCloudReplayItems()
{
```

위 시그니처 바로 앞에 아래 두 함수를 추가하고, 기존 `DrawCloudReplayItems` 본문은 유지한다:

```cpp
bool_t CScene_MyInfo::HasExpectedCloudReplay() const
{
	if (m_strExpectedReplayMatchID.empty())
		return true;
	const auto& items = CClientShellBackendService::Instance()
		.GetCloudReplayItems();
	return std::any_of(items.begin(), items.end(),
		[this](const Client::CloudReplayItem& item)
		{
			return item.matchId == m_strExpectedReplayMatchID;
		});
}

void CScene_MyInfo::UpdateExpectedReplayPolling(f32_t dt)
{
	if (m_strExpectedReplayMatchID.empty() || HasExpectedCloudReplay())
	{
		m_fReplayReadyPollRemainingSec = 0.f;
		return;
	}
	if (m_fReplayReadyPollRemainingSec <= 0.f)
		return;

	m_fReplayReadyPollRemainingSec = (std::max)(
		0.f, m_fReplayReadyPollRemainingSec - dt);
	m_fReplayReadyPollCooldownSec -= dt;
	CClientShellBackendService& backend =
		CClientShellBackendService::Instance();
	if (m_fReplayReadyPollCooldownSec <= 0.f &&
		!backend.IsReplayRequestInFlight())
	{
		m_fReplayReadyPollCooldownSec = kReplayReadyPollIntervalSec;
		backend.RequestReplayLibrary();
	}
}
```

현재 scene include 말미와 ImGui macro 사이 block:

```cpp
#include "Scene/Scene_MatchLoading.h"

#pragma push_macro("new")
```

아래로 교체:

```cpp
#include "Scene/Scene_MatchLoading.h"

#include <algorithm>

#pragma push_macro("new")
```

`DrawCloudReplayItems`의 현재 items/empty block:

```cpp
	const std::vector<Client::CloudReplayItem>& items =
		backend.GetCloudReplayItems();
	if (items.empty())
	{
		ImGui::TextUnformatted("No cloud replay is available for this account.");
		return;
	}
```

아래로 교체:

```cpp
	const std::vector<Client::CloudReplayItem>& items =
		backend.GetCloudReplayItems();
	if (!m_strExpectedReplayMatchID.empty() && !HasExpectedCloudReplay())
	{
		if (m_fReplayReadyPollRemainingSec > 0.f)
			ImGui::TextUnformatted("이번 경기 리플레이 업로드 처리 중...");
		else
			ImGui::TextUnformatted("리플레이 준비가 지연되고 있습니다. 서버 upload 로그를 확인하세요.");
	}
	if (items.empty())
	{
		ImGui::TextUnformatted("No cloud replay is available for this account.");
		return;
	}
```

cloud item 버튼 기존 코드:

```cpp
		if (!backend.IsReplayRequestInFlight() && ImGui::Button("Download"))
			backend.RequestReplayDownload(item);
```

아래로 교체:

```cpp
		if (!backend.IsReplayRequestInFlight() && ImGui::Button("다시보기"))
			backend.RequestReplayPlayback(item);
```

## 2-14. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameImGui.cpp

`DrawGameEndOverlay`의 마지막 버튼 기존 코드:

```cpp
        if (ImGui::Button("메인 메뉴로", ImVec2(160.f, 0.f)))
            m_bReturnToMainMenuRequested = true;
```

아래로 교체:

```cpp
        if (ImGui::Button("프로필 / 다시보기", ImVec2(180.f, 0.f)))
            m_bExitReplayToMyInfoRequested = true;
        ImGui::SameLine();
        if (ImGui::Button("메인 메뉴로", ImVec2(150.f, 0.f)))
            m_bReturnToMainMenuRequested = true;
```

## 2-15. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

`ChangeToMyInfoScene`의 현재 전체 블록:

```cpp
void CScene_InGame::ChangeToMyInfoScene()
{
    CClientShellBackendService::Instance().RequestPostMatchRefresh();

    CGameInstance::Get()->Change_Scene(
        static_cast<uint32_t>(eSceneID::MyInfo),
        CScene_MyInfo::Create());
}
```

`ChangeToMyInfoScene` 전체를 교체:

```cpp
void CScene_InGame::ChangeToMyInfoScene()
{
    if (m_bUsingSharedNetwork)
        CGameSessionClient::Instance().Disconnect();

    CClientShellBackendService::Instance().RequestPostMatchRefresh();

    CGameInstance::Get()->Change_Scene(
        static_cast<uint32_t>(eSceneID::MyInfo),
        CScene_MyInfo::Create());
}
```

## 2-16. 계획 적용 중 RESULT 계약

구현 세션은 첫 source edit와 동시에 아래 파일을 생성한다.

```text
C:/Users/user/Desktop/Winters/.md/plan/2026-07-19_RELEASE_AUTHENTICATED_ACCOUNT_REPLAY_E2E_RESULT.md
```

RESULT에는 계획 규칙대로 `예측 vs 실측`, `판결`, `⑤ 갱신`만 기록한다. 기존 replay/ClientShell dirty diff를 덮어쓰지 말고 각 anchor의 현재 상태를 다시 확인한다.

# 3. 검증

## 예측

- launcher 실행 후 backend match row 1개에 participant 3명과 공통 `game_session_id`가 기록되고, 세 Release client는 서로 다른 signed ticket으로 같은 UDP room에 들어간다.
- launcher는 기존 local backend를 `down`/재생성하고 `capture.json`에 `matchmaking_keys_cleared=<count>`, `matchmaking_queue_before_join=0`을 남긴다. PostgreSQL/MinIO 데이터는 유지된다.
- launcher ready 선언 전에 stdout에서 정확한 capture session의 signed-ticket marker와 `transport=udp endpoint=0.0.0.0:9000` marker를 모두 확인한다.
- 넥서스 종료 파일명은 `Replay/<canonical-match-uuid>.wrpl.pending`이며 `room1_tick...wrpl`이 아니다.
- Release stdout에 `Replay upload queue enabled`, `queued match=<id> participants=3`, `status=ready`가 순서대로 보인다. `missing_authenticated_match_identity`는 0회다.
- MyInfo 진입 첫 frame부터 “업로드 처리 중” 또는 ready replay 행이 보인다. local MinIO 기준 목표 ready 시간은 15초 이내, polling hard timeout은 60초다.
- 세 계정 `/replay/me`는 같은 match/replay id 1건을 반환한다. 각 클라이언트의 “다시보기”는 서로 다른 계정 ReplayCache에 checksum이 같은 WRPL을 저장하고 각자 새 `CReplayPlayer`를 만든다.
- 세 replay는 같은 first/last tick과 동일 snapshot/event sequence를 재생한다. 한 클라이언트의 pause/seek/speed는 다른 두 클라이언트에 영향을 주지 않는다.
- camera track과 시작 pose는 저장·동기화하지 않는다. 세 클라이언트의 camera가 같다는 보장은 없으며, 기존 replay camera 동작으로 각자 조작한다.
- Bot AI는 GameCommand 생산자이며 gameplay truth를 직접 변경하지 않는다. 이번 변경은 GameSim이나 bot command 경로를 수정하지 않는다.
- 깨질 수 있는 경계: 현재 signed UDP가 3클라 전체 경기에서 불안정할 수 있다. 아래 120초 Release preflight가 실패하면 profile/UI 작업을 완료 판정하지 않고 transport 문제를 별도 원인으로 남긴다.

## 검증 명령

서비스 정적/단위 검증:

```powershell
Set-Location C:/Users/user/Desktop/Winters/Services
gofmt -w pkg/config/config.go pkg/config/config_test.go internal/matchmaking/model.go internal/matchmaking/service.go cmd/matchmaking/main.go
go test ./...
go vet ./...
docker compose config --quiet
[scriptblock]::Create((Get-Content -LiteralPath C:/Users/user/Desktop/Winters/Tools/Harness/StartReleaseAccountReplayCapture.ps1 -Raw)) | Out-Null
```

Release 빌드(`/m:1`은 shared codegen 병렬 충돌 방지):

```powershell
& msbuild C:/Users/user/Desktop/Winters/Server/Include/Server.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /m:1
& msbuild C:/Users/user/Desktop/Winters/Client/Include/Client.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /m:1
& msbuild C:/Users/user/Desktop/Winters/Tools/Harness/ReplayClientSmoke.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /m:1
```

워크트리/포맷:

```powershell
Set-Location C:/Users/user/Desktop/Winters
git diff --check
git diff -- Services/pkg/config/config.go Services/pkg/config/config_test.go Services/internal/matchmaking Services/docker-compose.yml Server/Private/main.cpp Server/Private/Game/GameRoomReplication.cpp Server/Private/Backend/ReplayUploadQueue.cpp Client/Public/ClientShell/ClientShellBackendService.h Client/Private/ClientShell/ClientShellBackendService.cpp Client/Public/Scene/Scene_MyInfo.h Client/Private/Scene/Scene_MyInfo.cpp Client/Private/Scene/Scene_InGame.cpp Client/Private/Scene/Scene_InGameImGui.cpp Tools/Harness/StartReleaseAccountReplayCapture.ps1
```

Release 촬영 환경 시작:

```powershell
powershell -ExecutionPolicy Bypass -File C:/Users/user/Desktop/Winters/Tools/Harness/StartReleaseAccountReplayCapture.ps1 -HumanPlayers 3
```

120초 preflight:

```text
1. Release 클라 3개에서 서로 다른 온라인 계정 로그인.
2. launcher `capture.json`의 queue 0과 server stdout readiness marker 2개 확인.
3. 세 계정 모두 matchmaking join; status의 match_id/game_session_id가 동일한지 확인.
4. 120초 동안 세 클라가 move/BA/skill/snapshot을 지속 수신하고 disconnect 0인지 확인.
5. 실패 시 직접 TCP로 우회하지 않고 UDP session/ticket/frame 로그로 원인을 분리한다.
```

넥서스 종료 후 DB 판정:

```powershell
docker exec winters-postgres psql -U winters -d winters -c "SELECT id,status,game_session_id,completed_at FROM matches ORDER BY created_at DESC LIMIT 1;"
docker exec winters-postgres psql -U winters -d winters -c "SELECT match_id,user_id,slot,result FROM match_participants ORDER BY joined_at DESC NULLS LAST LIMIT 3;"
docker exec winters-postgres psql -U winters -d winters -c "SELECT id,match_id,status,size_bytes,record_count,ready_at FROM replays ORDER BY created_at DESC LIMIT 1;"
docker exec winters-postgres psql -U winters -d winters -c "SELECT replay_id,user_id,last_downloaded_at FROM replay_user_library ORDER BY user_id;"
```

3클라 수동 acceptance:

```text
1. 넥서스 파괴 → 각 클라 Game End에서 “프로필 / 다시보기”.
2. 수동 새로고침 없이 처리 중 행 → ready “다시보기” 전환.
3. 세 클라에서 각각 다시보기 클릭 → 각자 다운로드 후 replay scene 진입.
4. 모두 first seekable tick에서 pause: tick/score/entity/structure 상태가 같은지 촬영. camera 위치는 비교하지 않는다.
5. 클라 A만 seek, B만 pause, C만 speed 변경: 다른 프로세스 상태가 변하지 않는지 확인.
6. 세 cache 파일 SHA-256 동일, 경로 user UUID는 서로 다름.
7. replay 종료/프로필 복귀 후 live match 전적을 중복 저장하지 않는지 확인.
```

## 미검증

- 원본 플레이 당시 각 클라이언트의 camera path 및 세 replay의 동일 camera 시작 pose. 사용자 결정에 따라 기존 WRPL/camera를 그대로 쓰며 의도적으로 미지원이다.
- 서로 다른 PC/LAN/WAN의 3클라와 실제 AWS S3. 이번 acceptance는 한 PC의 Release 3프로세스 + local backend/MinIO다.
- 한 game server process에서 두 번째 매치를 연속 수용하는 room reset. launcher 1회당 capture match 1회를 기준으로 한다.

## 확인 필요

- 현재 dirty overlap은 계획 작성 시점에 `git status`와 각 실제 block을 다시 읽어 위 exact anchor에 반영했다. 구현 세션은 각 교체 직전 같은 block이 그대로인지 확인하고, 달라졌다면 해당 세션 변경을 보존한 채 이 계획의 의미를 병합한다. 이는 미확정 설계가 아니라 타 세션 변경 보호 절차다.
- `CONFIRM_NEEDED`: signed UDP 120초 preflight가 실패하면 이 계획의 계정/업로드/UI 결론은 유지하되, TCP ticket handshake 확장과 UDP 안정화 중 하나를 별도 계획으로 선택한다. 직접 TCP anonymous 접속은 합격 경로가 아니다.
- `CONFIRM_NEEDED`: local MinIO에서 실제 match replay가 15초 목표를 반복 초과하면 poll 주기를 줄이지 말고 upload checksum/part latency를 측정한다.

# 4. 서브 에이전트 비평

초안 read-only 비평:

- `/root/critique_nav_level_ui_plans`: P0 0 / P1 3, FAIL.
- `/root/diagnose_jungle_animation_fx`: P0 0 / P1 2, FAIL.
- `/root/diagnose_nav_chase_waypoint`: P0 0 / P1 1, FAIL.

처분:

- 범위 제거 — 비평은 “동일 camera 시작 pose” 계약을 구현하려면 full pose와 frame-order 보강이 필요하다고 정확히 지적했다. 그러나 사용자가 camera track/pose 변경 없이 기존 replay면 충분하다고 결정했으므로, 해당 계약과 DynamicCamera/Scene camera 변경을 계획에서 모두 삭제했다. 최종 acceptance는 동일 canonical world timeline과 프로세스별 독립 재생만 보장한다.
- 수용·구현 보정 — launcher가 `docker compose down --remove-orphans`로 backend를 재생성하고, 실제 Redis anonymous volume 발견에 따라 join 전 `matchmaking:*`만 선택 삭제한 뒤 잔존 key 0과 `ZCARD matchmaking:queue == 0`을 확인한다. PostgreSQL/MinIO named volume은 `-v`를 쓰지 않아 보존한다.
- 수용 — 1초 `HasExited` 판정을 폐기하고, Release stdout의 해당 `gameSession` signed-ticket marker와 UDP transport-ready marker가 둘 다 flush될 때까지 기다린다.
- 수용 — 현재 dirty 파일을 실제로 다시 읽어 `config_test`, backend header/cpp, MyInfo header/cpp, Scene cpp/ImGui의 구체적인 기존 block과 교체 block을 계획에 기록했다. 구현 직전 재확인은 타 세션 변경 보호 절차로만 남겼다.

재비평:

- `/root/diagnose_nav_chase_waypoint`: residual P0 0 / P1 0, PASS.
- `/root/diagnose_jungle_animation_fx`: residual P0 0 / P1 0, PASS.
- `/root/critique_nav_level_ui_plans`: 1차 delta에서 exact-anchor P1 1건을 추가 지적했다. §2-1/2-4/2-6/2-11/2-13의 현재 block을 모두 보강한 뒤 최종 residual P0 0 / P1 0, PASS.

최종 source-edit gate는 PASS다. 이 세션은 계획서만 작성하며 실제 구현은 다음 세션이 RESULT 계약과 함께 시작한다.

# 5. 인계 및 중단 기준

- 다른 세션은 이 PLAN을 먼저 읽고 같은 이름 RESULT를 만든 뒤 source edit를 시작한다.
- 구현 순서는 `config/matchmaking -> launcher -> Release UDP preflight -> server upload diagnostics -> MyInfo polling/download-open -> full E2E`다.
- preflight에서 세 client의 `MatchAssignment` 또는 authenticated identity가 하나라도 비면 UI를 손대며 증상을 숨기지 않는다.
- replay row가 `ready`인데 `/replay/me`가 비면 ACL/library transaction을, replay row 자체가 없으면 server enqueue/environment를, `room...wrpl`이면 match identity를 각각 원인으로 판정한다.

다른 세션 시작 문장:

```text
C:/Users/user/Desktop/Winters/.md/plan/2026-07-19_RELEASE_AUTHENTICATED_ACCOUNT_REPLAY_E2E_PLAN.md를 읽고 그대로 적용해 줘. 첫 source edit 전에 같은 이름 RESULT.md를 만들고, 현재 dirty 변경을 보존하면서 config/matchmaking→capture launcher→Release UDP preflight→upload diagnostics→MyInfo 자동 polling/download-open→Release 3클라 E2E 순서로 진행해. camera track/pose/camera 소스는 건드리지 말고 기존 replay camera를 유지해. P0/P1 회귀가 있으면 우회하지 말고 RESULT에 원인과 중단 지점을 기록해.
```
- 완료는 빌드 성공이 아니라 세 UUID 계정 library 3행, cache 3개, 독립 playback 3개와 Release 영상/로그 증거까지다.

# 6. 2026-07-19 실플레이 실패 보정 계획

## 6-1. 새 증거와 기존 결론의 폐기

- 실플레이 서버 로그는 `[ReplayUpload] skipped reason=missing_authenticated_match_identity`를 재현했다.
- 실행 중 서버는 argument 없는 TCP였고, `Client/Private/Scene/Scene_MainMenu.cpp`의 `RequestPlay()`는 matchmaking을 호출하지 않고 `m_bPlayRequested = true`만 설정했다.
- repository 전체에서 `RequestJoinQueue()`와 `RequestPollMatchStatus()`의 호출자는 0개였다. backend/client assignment 코드는 존재하지만 정상 UI flow에서 죽은 코드였다.
- PostgreSQL 최신 `matches`는 2026-07-16 smoke 1건뿐이며 사용자의 2026-07-19 승리 경기는 match row 자체가 없다.
- 따라서 기존 `IMPLEMENTED / AUTOMATED_PASS` 판결은 과대 판정이었다. launcher의 backend/server preflight만 통과했을 뿐 실제 Client가 assignment를 얻는 경로는 검증하지 않았다.

수정 메커니즘은 하나다.

```text
온라인 Game Start
  -> POST /matchmaking/join
  -> 0.5초 status polling
  -> MatchAssignment 저장
  -> 선택 product/CustomMode 진입
  -> assignment의 UDP ticket으로 server 연결
```

온라인 계정은 assignment가 생기기 전에 product scene으로 진입하지 않는다. offline account만 기존 direct launch를 유지한다. TCP anonymous session에 user ID를 self-report하거나 `room...wrpl`을 계정 library로 복제하는 우회는 거부한다.

## 6-2. C:/Users/user/Desktop/Winters/Client/Public/ClientShell/ClientShellBackendService.h

현재 getter block:

```cpp
	bool_t IsConfigured() const { return m_bConfigured; }
	bool_t IsPurchaseInFlight() const { return m_bPurchaseRequestInFlight; }
	bool_t IsStorefrontSyncInFlight() const { return m_bStoreRequestInFlight; }
	bool_t IsReplayRequestInFlight() const { return m_bReplayRequestInFlight; }
```

아래로 교체:

```cpp
	bool_t IsConfigured() const { return m_bConfigured; }
	bool_t IsPurchaseInFlight() const { return m_bPurchaseRequestInFlight; }
	bool_t IsStorefrontSyncInFlight() const { return m_bStoreRequestInFlight; }
	bool_t IsMatchRequestInFlight() const { return m_bMatchRequestInFlight; }
	bool_t IsReplayRequestInFlight() const { return m_bReplayRequestInFlight; }
```

## 6-2A. C:/Users/user/Desktop/Winters/Client/Public/Network/Backend/CHttpClient.h

현재 async method block:

```cpp
	void AsyncGet(const string& path, HttpCallback callback);
	void AsyncPost(const string& path, const string& jsonBody, HttpCallback callback);
```

아래로 교체:

```cpp
	void AsyncGet(const string& path, HttpCallback callback);
	void AsyncPost(const string& path, const string& jsonBody, HttpCallback callback);
	void AsyncDelete(const string& path, HttpCallback callback);
```

## 6-2B. C:/Users/user/Desktop/Winters/Client/Private/Network/Backend/CHttpClient.cpp

현재 `AsyncPost()` 전체:

```cpp
void CHttpClient::AsyncPost(const string & path, const string & jsonBody, HttpCallback callback)
{
	LaunchAsyncRequest("POST", path, jsonBody, callback);
}
```

바로 아래에 추가:

```cpp
void CHttpClient::AsyncDelete(const string& path, HttpCallback callback)
{
	LaunchAsyncRequest("DELETE", path, "", callback);
}
```

## 6-2C. C:/Users/user/Desktop/Winters/Client/Private/Network/Backend/MatchClient.cpp

현재 `LeaveQueue()` 전체:

```cpp
void CMatchClient::LeaveQueue(MatchCallback callback)
{
    m_pHttp->AsyncPost("/matchmaking/leave", "{}", [this, callback](const HttpResponse& resp) {
        callback(ParseResponse(resp));
        });
}
```

아래로 교체:

```cpp
void CMatchClient::LeaveQueue(MatchCallback callback)
{
    m_pHttp->AsyncDelete("/matchmaking/leave", [this, callback](const HttpResponse& resp) {
        callback(ParseResponse(resp));
        });
}
```

## 6-2D. C:/Users/user/Desktop/Winters/Client/Private/ClientShell/ClientShellBackendService.cpp

현재 `RequestLeaveQueue()` 전체:

```cpp
void CClientShellBackendService::RequestLeaveQueue()
{
	CClientShellSession::Instance().ClearMatchAssignment();
	CClientShellDataStore::Instance().SetLobbyIdle();

	if (!m_bConfigured || !m_pMatchClient)
	{
		m_strStatus = "Left offline queue";
		return;
	}

	if (m_bMatchRequestInFlight)
	{
		m_strStatus = "Matchmaking request already in flight";
		return;
	}

	m_bMatchRequestInFlight = true;
	m_strStatus = "Leaving queue...";

	const u32_t uGeneration = m_uGeneration;
	m_pMatchClient->LeaveQueue(
		[this, uGeneration](const Client::MatchStatus& status)
		{
			m_bMatchRequestInFlight = false;
			if (uGeneration == m_uGeneration)
			{
				CClientShellDataStore::Instance().SetLobbyIdle();
				m_strStatus = status.error.empty() ? "Left queue" : "Leave queue failed: " + status.error;
			}
			TryFinishDeferredReset();
		});
}
```

아래로 교체:

```cpp
void CClientShellBackendService::RequestLeaveQueue()
{
	if (!m_bConfigured || !m_pMatchClient)
	{
		CClientShellSession::Instance().ClearMatchAssignment();
		CClientShellDataStore::Instance().SetLobbyIdle();
		m_strStatus = "Left offline queue";
		return;
	}

	if (m_bMatchRequestInFlight)
	{
		m_strStatus = "Matchmaking request already in flight";
		return;
	}

	m_bMatchRequestInFlight = true;
	m_strStatus = "Leaving queue...";

	const u32_t uGeneration = m_uGeneration;
	m_pMatchClient->LeaveQueue(
		[this, uGeneration](const Client::MatchStatus& status)
		{
			m_bMatchRequestInFlight = false;
			if (uGeneration == m_uGeneration)
			{
				if (status.error.empty())
				{
					CClientShellSession::Instance().ClearMatchAssignment();
					CClientShellDataStore::Instance().SetLobbyIdle();
					m_strStatus = "Left queue";
				}
				else
				{
					m_strStatus = "Leave queue failed: " + status.error;
				}
			}
			TryFinishDeferredReset();
		});
}
```

## 6-2E. C:/Users/user/Desktop/Winters/Services/internal/matchmaking/service.go

`const` block 바로 아래에 추가:

```go
var claimPlayersScript = redis.NewScript(`
local count = tonumber(ARGV[1])
for index = 1, count do
    local member = ARGV[3 + index]
    if redis.call('GET', KEYS[1 + index]) ~= 'queued' then
        return 0
    end
    if redis.call('ZSCORE', KEYS[1], member) == false then
        return 0
    end
end
for index = 1, count do
    local member = ARGV[3 + index]
    redis.call('ZREM', KEYS[1], member)
    redis.call('SET', KEYS[1 + index], ARGV[2], 'EX', ARGV[3])
end
return 1
`)

var leaveQueueScript = redis.NewScript(`
local status = redis.call('GET', KEYS[2])
if status and (
    string.sub(status, 1, 9) == 'claiming:' or
    string.find(status, '"status":"matched"', 1, true)) then
    return 0
end
redis.call('ZREM', KEYS[1], ARGV[1])
redis.call('DEL', KEYS[2], KEYS[3])
return 1
`)

var releasePlayerClaimScript = redis.NewScript(`
if redis.call('GET', KEYS[2]) ~= ARGV[1] then
    return 0
end
redis.call('ZADD', KEYS[1], ARGV[2], ARGV[3])
redis.call('SET', KEYS[2], 'queued', 'EX', ARGV[4])
return 1
`)
```

현재 `Leave()` 전체:

```go
func (s *Service) Leave(ctx context.Context, userId uuid.UUID) error {
	s.rdb.ZRem(ctx, queueKey, userId.String())
	s.rdb.Del(ctx, statusPrefix+userId.String())
	s.rdb.Del(ctx, joinTimePrefix+userId.String())
	slog.Info("player left queue", "user_id", userId)
	return nil
}
```

아래로 교체:

```go
func (s *Service) Leave(ctx context.Context, userId uuid.UUID) error {
	removed, err := leaveQueueScript.Run(
		ctx,
		s.rdb,
		[]string{
			queueKey,
			statusPrefix + userId.String(),
			joinTimePrefix + userId.String(),
		},
		userId.String()).Int()
	if err != nil {
		return fmt.Errorf("leave queue atomically: %w", err)
	}
	if removed == 0 {
		return fmt.Errorf("%w: match already assigned", apperr.ErrAlreadyExists)
	}
	slog.Info("player left queue", "user_id", userId)
	return nil
}
```

`tryMatch()`의 현재 match 생성 block:

```go
			if err := s.createMatch(ctx, players, totalMMR/len(group)); err != nil {
				slog.Error("create match", "error", err)
				continue
			}
			for _, z := range group {
				used[z.Member.(string)] = true
			}
```

아래로 교체:

```go
			matchID := uuid.New()
			claimed, err := s.claimPlayers(ctx, matchID, players)
			if err != nil {
				slog.Error("claim match players", "error", err)
				continue
			}
			if !claimed {
				continue
			}

			if err := s.createMatch(
				ctx,
				matchID,
				players,
				totalMMR/len(group)); err != nil {
				s.releasePlayerClaims(ctx, matchID, group)
				slog.Error("create match", "error", err)
				continue
			}
			for _, z := range group {
				used[z.Member.(string)] = true
			}
```

현재 `calcRange()` 바로 위에 추가:

```go
func (s *Service) claimPlayers(
	ctx context.Context,
	matchID uuid.UUID,
	players []uuid.UUID,
) (bool, error) {
	keys := make([]string, 1, len(players)+1)
	keys[0] = queueKey
	arguments := make([]interface{}, 0, len(players)+3)
	arguments = append(
		arguments,
		len(players),
		"claiming:"+matchID.String(),
		int(statusTTL/time.Second))
	for _, player := range players {
		keys = append(keys, statusPrefix+player.String())
		arguments = append(arguments, player.String())
	}

	claimed, err := claimPlayersScript.Run(
		ctx,
		s.rdb,
		keys,
		arguments...).Int()
	if err != nil {
		return false, fmt.Errorf("claim queued players: %w", err)
	}
	return claimed == 1, nil
}

func (s *Service) releasePlayerClaims(
	ctx context.Context,
	matchID uuid.UUID,
	group []redis.Z,
) {
	claimValue := "claiming:" + matchID.String()
	for _, entry := range group {
		member, ok := entry.Member.(string)
		if !ok {
			continue
		}
		if _, err := releasePlayerClaimScript.Run(
			ctx,
			s.rdb,
			[]string{queueKey, statusPrefix + member},
			claimValue,
			entry.Score,
			member,
			int(statusTTL/time.Second)).Result(); err != nil {
			slog.Error(
				"release match player claim",
				"match_id", matchID,
				"user_id", member,
				"error", err)
		}
	}
}
```

현재 `createMatch()` signature와 첫 두 줄:

```go
func (s *Service) createMatch(ctx context.Context, players []uuid.UUID, avgMMR int) error {
	if s.ticketSigner == nil {
		return fmt.Errorf("match ticket signer is not configured")
	}
	matchID := uuid.New()
```

아래로 교체:

```go
func (s *Service) createMatch(
	ctx context.Context,
	matchID uuid.UUID,
	players []uuid.UUID,
	avgMMR int,
) error {
	if s.ticketSigner == nil {
		return fmt.Errorf("match ticket signer is not configured")
	}
```

이 claim은 leave와 같은 Redis key를 원자적으로 검사·변경한다. leave가 먼저면 claim이 0으로 실패하고, claim이 먼저면 leave가 `match already assigned`로 실패한다. 따라서 DB participant를 만든 뒤 해당 client가 취소 성공으로 빠지는 중간 상태가 없다.

## 6-3. C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_MainMenu.h

현재 private method block:

```cpp
	void RequestPlay();
	void RequestLogout();

	bool_t LaunchSelectedProduct();
```

아래로 교체:

```cpp
	void RequestPlay();
	void RequestLogout();
	void UpdateOnlineMatchmaking(f32_t dt);

	bool_t LaunchSelectedProduct();
```

현재 field tail:

```cpp
	bool_t m_bPlayRequested = false;
	bool_t m_bLogoutRequested = false;
	bool_t m_bShopRequested = false;
	bool_t m_bMyInfoRequested = false;
```

아래로 교체:

```cpp
	bool_t m_bPlayRequested = false;
	bool_t m_bLogoutRequested = false;
	bool_t m_bShopRequested = false;
	bool_t m_bMyInfoRequested = false;
	bool_t m_bWaitingForMatch = false;
	bool_t m_bCancellingMatch = false;
	bool_t m_bMatchLeaveIssued = false;
	f32_t m_fMatchPollCooldownSec = 0.f;
```

## 6-4. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_MainMenu.cpp

`OnEnter()` 초기화 block:

```cpp
	m_bPlayRequested = false;
	m_bLogoutRequested = false;
	m_bShopRequested = false;
	m_bMyInfoRequested = false;
```

아래로 교체:

```cpp
	m_bPlayRequested = false;
	m_bLogoutRequested = false;
	m_bShopRequested = false;
	m_bMyInfoRequested = false;
	m_bWaitingForMatch = false;
	m_bCancellingMatch = false;
	m_bMatchLeaveIssued = false;
	m_fMatchPollCooldownSec = 0.f;
```

현재 `OnUpdate()` 전체:

```cpp
void CScene_MainMenu::OnUpdate(f32_t /*dt*/)
{
	CClientShellBackendService::Instance().ProcessCallbacks();
	if (!CClientShellBackendService::Instance().GetStatus().empty())
		m_strStatus = CClientShellBackendService::Instance().GetStatus();

	if (m_ImageUI.WasSourceRectClicked(kGameStartRect))
		RequestPlay();
	if (m_ImageUI.WasSourceRectClicked(kShopButtonRect))
		m_bShopRequested = true;
	if (m_ImageUI.WasSourceRectClicked(kMyInfoPortraitRect))
		m_bMyInfoRequested = true;

	if (m_bLogoutRequested) { m_bLogoutRequested = false; ChangeToLogin(); return; }
	if (m_bShopRequested) { m_bShopRequested = false; ChangeToShop(); return; }
	if (m_bMyInfoRequested) { m_bMyInfoRequested = false; ChangeToMyInfo(); return; }
	if (m_bPlayRequested) { m_bPlayRequested = false; LaunchSelectedProduct(); return; }
}
```

아래로 교체:

```cpp
void CScene_MainMenu::OnUpdate(f32_t dt)
{
	CClientShellBackendService& backend =
		CClientShellBackendService::Instance();
	backend.ProcessCallbacks();
	if (!backend.GetStatus().empty())
		m_strStatus = backend.GetStatus();

	const bool_t bGameStartClicked =
		m_ImageUI.WasSourceRectClicked(kGameStartRect);
	if (m_bWaitingForMatch)
	{
		const bool_t bMatchAlreadyAssigned =
			CClientShellSession::Instance().HasMatchAssignment() &&
			CClientShellDataStore::Instance().GetLobbyState().bMatchReady;
		if (bGameStartClicked &&
			!m_bCancellingMatch &&
			!bMatchAlreadyAssigned)
		{
			m_bCancellingMatch = true;
			m_bMatchLeaveIssued = false;
			m_strStatus = "Cancelling matchmaking...";
		}
		UpdateOnlineMatchmaking(dt);
	}
	else
	{
		if (bGameStartClicked)
			RequestPlay();
		if (m_ImageUI.WasSourceRectClicked(kShopButtonRect))
			m_bShopRequested = true;
		if (m_ImageUI.WasSourceRectClicked(kMyInfoPortraitRect))
			m_bMyInfoRequested = true;
	}

	if (m_bLogoutRequested) { m_bLogoutRequested = false; ChangeToLogin(); return; }
	if (m_bShopRequested) { m_bShopRequested = false; ChangeToShop(); return; }
	if (m_bMyInfoRequested) { m_bMyInfoRequested = false; ChangeToMyInfo(); return; }
	if (m_bPlayRequested) { m_bPlayRequested = false; LaunchSelectedProduct(); return; }
}
```

현재 `RequestPlay()` 전체:

```cpp
void CScene_MainMenu::RequestPlay()
{
	m_strStatus = "Launching selected product...";
	m_bPlayRequested = true;
}
```

아래로 교체하고 바로 아래에 `UpdateOnlineMatchmaking()`을 추가:

```cpp
void CScene_MainMenu::RequestPlay()
{
	const CClientShellSession& session = CClientShellSession::Instance();
	if (!session.IsAuthenticated() || session.IsOfflineAccount())
	{
		m_strStatus = "Launching selected product...";
		m_bPlayRequested = true;
		return;
	}

	CClientShellBackendService& backend =
		CClientShellBackendService::Instance();
	if (!backend.IsConfigured())
	{
		m_strStatus = "Online matchmaking backend is unavailable";
		return;
	}

	m_bWaitingForMatch = true;
	m_fMatchPollCooldownSec = 0.5f;
	m_strStatus = "Joining authenticated match queue...";
	backend.RequestJoinQueue();
}

void CScene_MainMenu::UpdateOnlineMatchmaking(f32_t dt)
{
	CClientShellBackendService& backend =
		CClientShellBackendService::Instance();
	const ShellLobbyState& lobby =
		CClientShellDataStore::Instance().GetLobbyState();
	if (CClientShellSession::Instance().HasMatchAssignment() &&
		lobby.bMatchReady)
	{
		m_bWaitingForMatch = false;
		m_bCancellingMatch = false;
		m_bMatchLeaveIssued = false;
		m_strStatus = "Authenticated match found";
		m_bPlayRequested = true;
		return;
	}

	if (m_bCancellingMatch)
	{
		if (backend.IsMatchRequestInFlight())
			return;

		if (m_bMatchLeaveIssued)
		{
			m_bCancellingMatch = false;
			m_bMatchLeaveIssued = false;
			if (lobby.eQueueState == eLobbyQueueState::Idle)
			{
				m_bWaitingForMatch = false;
				m_fMatchPollCooldownSec = 0.f;
				m_strStatus = "Matchmaking cancelled";
			}
			return;
		}

		if (lobby.eQueueState == eLobbyQueueState::Idle)
		{
			m_bCancellingMatch = false;
			m_bWaitingForMatch = false;
			m_fMatchPollCooldownSec = 0.f;
			m_strStatus = "Matchmaking cancelled";
			return;
		}

		backend.RequestLeaveQueue();
		m_bMatchLeaveIssued = true;
		return;
	}

	if (lobby.eQueueState == eLobbyQueueState::Idle &&
		!backend.IsMatchRequestInFlight())
	{
		m_bWaitingForMatch = false;
		return;
	}

	m_fMatchPollCooldownSec -= dt;
	if (m_fMatchPollCooldownSec > 0.f ||
		backend.IsMatchRequestInFlight())
	{
		return;
	}

	m_fMatchPollCooldownSec = 0.5f;
	backend.RequestPollMatchStatus();
}
```

`OnImGui()`의 RP text block 바로 아래에 추가:

```cpp
	if (m_bWaitingForMatch)
	{
		drawSourceText(
			47.f,
			78.f,
			IM_COL32(120, 210, 255, 255),
			m_strStatus.c_str());
		drawSourceText(
			47.f,
			98.f,
			IM_COL32(210, 210, 210, 255),
			"Click Game Start again to cancel");
	}
```

## 6-5. 표준 실행 계약

- 영상 촬영용 Release acceptance는 반드시 `Tools/Harness/StartReleaseAccountReplayCapture.ps1 -HumanPlayers 3`로 backend와 signed UDP server를 함께 시작한다.
- 세 온라인 client가 모두 메인 메뉴 `Game Start`를 눌러 동일 match assignment를 얻기 전에는 CustomMode로 넘어가지 않는다.
- argument 없는 TCP server는 local/offline gameplay 용도일 뿐 account replay acceptance가 아니다. 온라인 client가 더 이상 이 경로로 조용히 진입하지 않으므로 `room...wrpl` 성공처럼 보이는 실패를 차단한다.

## 6-6. 보정 검증

1. `rg`로 `RequestJoinQueue()`와 `RequestPollMatchStatus()`의 실제 MainMenu callsite가 각각 1개 이상인지 확인한다.
2. Client Release x64 `/m:1` 빌드.
3. capture launcher로 backend/server 시작 후 test account 3개의 `/matchmaking/join`과 `/status`가 하나의 match/session과 ticket 3개를 반환하는지 확인한다.
4. 세 ticket을 UDP handshake probe로 각각 검증하고 server stdout에 authenticated participant 3명이 들어오는지 확인한다.
5. 3번째 계정 합류 전 Release client 한 개에서 Game Start를 다시 눌러 취소하고, DELETE 성공·Redis status/queue 제거·MainMenu 복귀를 확인한 뒤 다시 join할 수 있는지 확인한다.
6. poll callback이 `matched`가 되는 경계에서 Game Start를 재클릭해도 DELETE를 발행하지 않고 complete assignment를 우선해 DB participant 3행이 유지되는지 확인한다.
7. matcher tick과 DELETE를 반복 경쟁시켜 `leave 성공 + DB participant 생성` 조합이 0건인지 확인한다. claim이 먼저인 경우 leave는 실패하고 이후 matched status를 반환해야 한다.
8. 실제 Release client 3개에서 넥서스 종료 후 `Replay/<match-id>.wrpl.pending -> queued participants=3 -> status=ready -> /replay/me 3계정`을 확인한다.
9. 8번 전에는 `FULL_E2E_PASS`를 선언하지 않는다.

## 6-7. 보정 비평 gate

1차 독립 read-only 비평 `/root/replay_plan_critique`: P0 0 / P1 1, FAIL.

- 수용 — 매칭 대기 중 모든 UI 입력이 막히고 `RequestLeaveQueue()` 호출자가 없어 세 번째 계정이 오지 않으면 무기한 대기한다는 지적을 수용했다.
- 수용 — client `LeaveQueue()`의 POST와 backend DELETE route 불일치를 `CHttpClient::AsyncDelete()`로 정렬한다.
- 수용 — Game Start 재클릭을 취소 입력으로 정의하고, join/poll 요청 중이면 완료 뒤 leave를 발행하며, leave 성공 뒤에만 assignment/lobby를 지운다. leave 실패 시 대기 상태를 유지해 재시도할 수 있다.
- 범위 제한 — 재실행 시 잔존 Redis queue 자동 복구는 capture launcher가 join 전 `matchmaking:*`를 0건으로 만드는 이번 1-match Release acceptance의 전제 밖이다. 대신 정상 취소가 Redis 상태를 제거하고 같은 client가 다시 join 가능한지를 acceptance에 넣는다.

수정본은 같은 독립 에이전트에게 재비평을 받고, residual accepted 또는 보류 P0/P1이 0일 때만 source edit를 시작한다.

2차 재비평 `/root/replay_plan_critique`: P0 0 / P1 1, FAIL.

- 수용 — `matched` callback과 취소 클릭이 같은 frame에 오면 이미 DB match/participant가 확정됐는데 Redis status만 DELETE하여 2인 접속과 3인 DB roster가 갈라질 수 있다는 지적을 수용했다.
- 수용 — complete assignment 검사를 cancellation보다 먼저 수행하며, `OnUpdate()`도 complete assignment가 있으면 취소 intent를 만들지 않는다. join/poll 중 눌린 취소보다 callback의 `matched` 결과를 우선한다.
- 수용 — matched 경계 재클릭에서 DELETE 미발행과 participant 3행 유지를 acceptance에 추가했다.

2차 수정본의 residual P0/P1 0을 확인한 뒤에만 source edit를 시작한다.

3차 재비평 `/root/replay_plan_critique`: P0 0 / P1 1, FAIL.

- 수용 — matcher가 queue snapshot을 읽은 뒤 leave가 성공하고, 그 뒤 matcher가 DB participant를 만들 수 있는 server-side race를 수용했다.
- 수용 — Redis Lua claim으로 N명의 queue membership과 `queued` status를 한 번에 `claiming:<match>`로 전환한다. leave는 `claiming`과 `matched`를 삭제하지 않는다.
- 수용 — DB 생성 실패 때 자기 claim인 사용자만 원래 MMR score로 queue에 복구하며, matcher/leave 경쟁 acceptance를 추가한다.

3차 수정본은 residual P0/P1만 짧게 최종 확인한다.

최종 재비평 `/root/replay_plan_critique`: residual P0 0 / P1 0, PASS.

- atomic claim/leave가 동일 Redis key를 직렬화하고, claim 실패 rollback과 client polling 복귀가 맞음을 확인했다.
- 최종 source-edit gate는 PASS다.

# 7. 2026-07-20 Release 3-client live acceptance에서 발견한 replay loading 교착

## 7-1. 실행 증거와 원인

- Release x64 Client 3개가 서로 다른 인증 계정으로 동일 match `e0aa9154-7ae0-4699-9365-3dc465b0c0ce`에 입장했다.
- Release server console의 red nexus authoritative HP 명령 뒤 `GameEnd`, `.wrpl.pending`, `queued ... participants=3`, `status=ready`가 순서대로 발생했다.
- 세 계정 모두 MyInfo `Cloud / account` 탭에서 같은 69.12 MiB replay와 각자 다른 계정명을 확인했고, replay service의 세 `/download` 요청도 HTTP 200이었다.
- 그러나 세 client 모두 replay MatchLoading 카드 화면에서 2분 이상 전환하지 않았다. 세 process는 응답 중이었다.
- `CScene_MyInfo::OpenReplay()`는 live game 종료 때 남은 `CLoLMatchContextRuntime::Context().bUseNetworkRoster == true`를 정리하지 않고 `CScene_MatchLoading`을 만든다.
- `CScene_MatchLoading::OnUpdate()`는 callback이 replay scene을 만든다는 사실을 알 수 없고 오직 stale `context.bUseNetworkRoster`를 보고 live network loading으로 판정한다. MyInfo 진입 때 `CGameSessionClient`는 이미 disconnect됐으므로 `bNetworkLoading == false`이고, asset loader가 준비된 뒤에도 network roster gate에서 영구 return한다.
- replay file, account ACL, download, decoder가 실패한 것이 아니라 live match context lifecycle 누락이다.

## 7-2. 범위와 불변식

- camera track은 추가하지 않는다. 기존 WRPL snapshot/event camera 동작을 그대로 사용한다.
- live match loading의 `connected + lobby state + authoritative local roster identity` gate는 변경하지 않는다.
- replay 진입 직전에만 이전 live roster context를 비운다. replay `CScene_InGame`은 live network를 연결하지 않고 WRPL snapshot/event로 world를 재구성한다.
- MyInfo cloud replay와 CustomMode local replay 두 product entry를 같은 규약으로 맞춘다. command-line replay는 process 시작 시 context가 기본값이므로 별도 source 변경이 필요 없다.

## 7-3. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_MyInfo.cpp

현재 include anchor:

```cpp
#include "GameInstance.h"
#include "Replay/LocalMatchRecord.h"
```

아래로 교체:

```cpp
#include "GameInstance.h"
#include "GamePlay/LoLMatchContextRuntime.h"
#include "Replay/LocalMatchRecord.h"
```

현재 `OpenReplay()` transition block:

```cpp
	m_bSceneTransitionStarted = true;
	auto pLoadingMatch = CScene_MatchLoading::Create(
```

아래로 교체:

```cpp
	m_bSceneTransitionStarted = true;
	Client::CLoLMatchContextRuntime::Instance().Reset();
	auto pLoadingMatch = CScene_MatchLoading::Create(
```

## 7-4. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_CustomMode.cpp

현재 `OpenReplay()` transition block:

```cpp
	m_bSceneTransitionStarted = true;
	auto pLoadingMatch = CScene_MatchLoading::Create(
```

아래로 교체:

```cpp
	m_bSceneTransitionStarted = true;
	Client::CLoLMatchContextRuntime::Instance().Reset();
	auto pLoadingMatch = CScene_MatchLoading::Create(
```

이 파일은 이미 `GamePlay/LoLMatchContextRuntime.h`를 include하므로 새 include가 필요 없다.

## 7-5. 검증 및 완료 기준

1. Client Release x64 `/m:1` build PASS.
2. 현재 ready replay를 세 계정의 MyInfo에서 각각 다시 실행하고, 세 창 모두 제한 시간 120초 안에 `Replay Chrono Break` panel과 인게임 world를 표시한다.
3. 각 process의 replay control을 독립 조작해 한 client의 pause/main-menu 상태가 다른 두 process에 전파되지 않음을 확인한다.
4. 세 replay 창에서 `메인 메뉴로`를 각각 클릭하고, 세 계정 session을 유지한 MainMenu 복귀 화면을 확인한다.
5. 복귀 직후 세 client가 다시 `Game Start`를 눌러 새 matchmaking assignment와 server room에 입장하고 두 번째 인게임 화면을 표시한다.
6. 두 번째 game도 nexus authoritative 종료가 가능하고 server가 첫 match와 다른 canonical match ID로 replay를 발행해야 한다. 적어도 새 game 진입까지 실패하면 즉시 재경기 lifecycle 결함으로 분리해 수정 후 전체 시나리오를 반복한다.
7. live screenshots, server/replay logs, account IDs, match IDs를 `.md/build/release-replay-capture/4cb0387b-ede3-483a-8390-642db7681dd2`에 보존하고 RESULT를 `FULL_E2E_PASS` 또는 정확한 blocker로 갱신한다.

## 7-6. 독립 비평 gate

1차 read-only 비평 `/root/replay_plan_critique`: P0 0 / P1 3 / P2 1, FAIL.

- 수용 — `Reset()`만 하면 replay `CreateECSEntities()`가 local practice fallback을 다시 만들고 WRPL에 없는 `net=1005` ghost가 남을 수 있다. replay bootstrap에서는 fallback/roster spawn을 건너뛰고 첫 authoritative full snapshot이 roster를 만들게 한다.
- 1차 보정 뒤 재수용 — canonical WRPL은 의도적으로 `yourNetId=0`으로 기록되므로 snapshot local ID에 의존하지 않는다. 첫 full snapshot에 존재하는 champion 중 가장 낮은 NetId를 결정론적 spectator focus로 선택하고 seek 뒤에도 같은 규칙으로 재결합한다.
- 수용 — CustomMode replay는 `OnEnter()`에서 연 live lobby connection을 replay 진입 전에 `Disconnect()`해야 한다.
- 수용 — match completion 뒤 Redis `matched` 상태가 300초 남아 즉시 재매칭을 막는다. replay completion의 idempotent 후처리로, 현재 status가 완료된 **같은 match ID**일 때만 atomic delete한다.
- 수용 — 화면 표시만 보지 않고 first full snapshot, tick 진행, ghost 부재, 세 process의 독립 pause/seek, MainMenu의 동일 user ID를 검증한다.

보정본은 같은 독립 에이전트에게 재비평하여 residual accepted/held P0/P1이 0일 때만 source를 수정한다.

2차 재비평: P0 0 / P1 1, FAIL.

- 수용 — canonical WRPL의 `yourNetId=0` 계약을 반영해 lowest champion NetId spectator focus를 계획했다.

3차 재비평: P0 0 / P1 1, FAIL.

- 수용 — paused seek는 다음 successful update를 보장하지 않으므로 공통 `ApplyReplaySpectatorFocus()`를 update와 seek 성공 경로 양쪽에서 호출하도록 보정했다.

최종 재비평: residual P0 0 / P1 0, PASS. Source-edit gate를 통과했다.

## 7-7. 보정 source 명세

### 7-7-1. `Client/Private/Scene/Scene_MyInfo.cpp`

§7-3의 include 및 replay 진입 직전 `CLoLMatchContextRuntime::Reset()` 명세를 유지한다. MyInfo는 live game의 `ChangeToMyInfoScene()`에서 shared session을 이미 disconnect하므로 추가 disconnect는 하지 않는다.

### 7-7-2. `Client/Private/Scene/Scene_CustomMode.cpp`

현재 `OpenReplay()` transition anchor:

```cpp
	m_bSceneTransitionStarted = true;
	auto pLoadingMatch = CScene_MatchLoading::Create(
```

아래로 교체:

```cpp
	m_bSceneTransitionStarted = true;
	CGameSessionClient::Instance().Disconnect();
	Client::CLoLMatchContextRuntime::Instance().Reset();
	auto pLoadingMatch = CScene_MatchLoading::Create(
```

이 순서는 lobby callback/network를 먼저 정리한 뒤 roster context를 폐기한다. live match를 시작하는 `StartMatchLoadingScene()`에는 적용하지 않는다.

### 7-7-3. `Client/Private/Scene/Scene_InGameLifecycle.cpp`

현재 `CreateECSEntities()` 시작:

```cpp
void CScene_InGame::CreateECSEntities()
{
    MatchContext& context = Client::CLoLMatchContextRuntime::Instance().Context();
    CInGameRosterSpawner::EnsureLocalRosterFallback(context);
    m_PlayerEntity = NULL_ENTITY;
```

아래로 교체:

```cpp
void CScene_InGame::CreateECSEntities()
{
    m_PlayerEntity = NULL_ENTITY;
    if (m_bReplayPlaybackMode)
    {
        CreateMapEntity();
        Winters::DevSmoke::Log(
            "[ECS:ReplayBootstrap] roster spawn deferred to authoritative replay snapshot\n");
        return;
    }

    MatchContext& context = Client::CLoLMatchContextRuntime::Instance().Context();
    CInGameRosterSpawner::EnsureLocalRosterFallback(context);
```

live/network/local-only bootstrap은 기존 동작을 유지한다. replay만 stage map entity 외 champion roster 선생성을 하지 않는다.

### 7-7-4. `Client/Private/Scene/Scene_InGameImGui.cpp`

`Client/Public/Scene/Scene_InGame.h`의 replay private method anchor:

```cpp
    void UpdateReplayPlayback(f32_t dt);
    bool_t SeekReplayToTick(u64_t targetTick);
```

아래로 교체:

```cpp
    void ApplyReplaySpectatorFocus();
    void UpdateReplayPlayback(f32_t dt);
    bool_t SeekReplayToTick(u64_t targetTick);
```

`UpdateReplayPlayback()` 위에 공통 helper를 추가한다.

```cpp
void CScene_InGame::ApplyReplaySpectatorFocus()
{
    if (!m_pEntityIdMap)
        return;

    NetEntityId focusNetId = NULL_NET_ENTITY;
    m_pEntityIdMap->ForEachBinding(
        [this, &focusNetId](NetEntityId netId, EntityID entity)
        {
            if (m_World.IsAlive(entity) &&
                m_World.HasComponent<ChampionComponent>(entity) &&
                (focusNetId == NULL_NET_ENTITY || netId < focusNetId))
            {
                focusNetId = netId;
            }
        });
    ApplyAuthoritativePlayerNetId(focusNetId);
}
```

현재 `UpdateReplayPlayback()` record 적용 block:

```cpp
    if (m_pReplayPlayer->Update(
        dt,
        m_World,
        *m_pEntityIdMap,
        *m_pSnapshotApplier,
        *m_pEventApplier))
    {
        ProjectGameplayActorsToMapSurface();
    }
```

아래로 교체:

```cpp
    if (m_pReplayPlayer->Update(
        dt,
        m_World,
        *m_pEntityIdMap,
        *m_pSnapshotApplier,
        *m_pEventApplier))
    {
        ApplyReplaySpectatorFocus();
        ProjectGameplayActorsToMapSurface();
    }
```

canonical replay는 모든 계정이 같은 파일을 받으며 `yourNetId=0`이다. 따라서 account/live stale identity를 복원하지 않고, authoritative snapshot의 가장 낮은 champion NetId를 spectator focus로 사용한다. 같은 full snapshot에 대해 결정론적이며 `ResetForReplaySeek()` 뒤 record가 재적용될 때도 재결합한다. 별도 camera track은 만들지 않는다.

현재 `SeekReplayToTick()` 성공 block:

```cpp
    if (bApplied)
    {
        ProjectGameplayActorsToMapSurface();
        m_strReplayStatus = "Replay Chrono seek complete";
        return true;
    }
```

아래로 교체:

```cpp
    if (bApplied)
    {
        ApplyReplaySpectatorFocus();
        ProjectGameplayActorsToMapSurface();
        m_strReplayStatus = "Replay Chrono seek complete";
        return true;
    }
```

paused seek에서도 즉시 focus를 재결합하며 다음 `Update()` 성공 여부에 의존하지 않는다.

### 7-7-5. `Services/internal/replay/repository.go`

import에 `github.com/redis/go-redis/v9`를 추가하고 repository를 다음과 같이 확장한다.

```go
type Repository struct {
	db  *pgxpool.Pool
	rdb *redis.Client
}

func NewRepository(db *pgxpool.Pool, rdb *redis.Client) *Repository {
	return &Repository{db: db, rdb: rdb}
}
```

repository 선언 아래에 현재 status가 같은 completed match에 묶인 경우만 삭제하는 Lua를 추가한다.

```go
const matchmakingStatusPrefix = "matchmaking:status:"

var releaseCompletedMatchStatusScript = redis.NewScript(`
local raw = redis.call('GET', KEYS[1])
if not raw then
    return 0
end
local ok, status = pcall(cjson.decode, raw)
if not ok or status['status'] ~= 'matched' or status['match_id'] ~= ARGV[1] then
    return 0
end
return redis.call('DEL', KEYS[1])
`)
```

`CompleteMatch()`가 사용할 idempotent helper를 추가한다.

```go
func (r *Repository) releaseCompletedMatchStatuses(
	ctx context.Context,
	matchID uuid.UUID,
	players []MatchCompletionPlayer,
) error {
	for _, player := range players {
		if _, err := releaseCompletedMatchStatusScript.Run(
			ctx,
			r.rdb,
			[]string{matchmakingStatusPrefix + player.UserID.String()},
			matchID.String(),
		).Int(); err != nil {
			return fmt.Errorf("release completed matchmaking status for %s: %w", player.UserID, err)
		}
	}
	return nil
}
```

현재 completed idempotency block:

```go
	if status == "completed" {
		return tx.Commit(ctx)
	}
```

아래로 교체:

```go
	if status == "completed" {
		if err := tx.Commit(ctx); err != nil {
			return fmt.Errorf("commit completed match lookup: %w", err)
		}
		return r.releaseCompletedMatchStatuses(ctx, matchID, players)
	}
```

현재 신규 completion의 마지막 block:

```go
	if err := tx.Commit(ctx); err != nil {
		return fmt.Errorf("commit match completion: %w", err)
	}
	return nil
```

아래로 교체:

```go
	if err := tx.Commit(ctx); err != nil {
		return fmt.Errorf("commit match completion: %w", err)
	}
	return r.releaseCompletedMatchStatuses(ctx, matchID, players)
```

DB commit 뒤 Redis가 일시 실패하면 endpoint는 실패하고 server upload worker가 재시도한다. 재시도는 completed DB path로 들어와 같은-match conditional delete만 다시 수행하므로 다른 새 match 상태를 지우지 않는다.

### 7-7-6. `Services/cmd/replay/main.go`

import에 다음을 추가한다.

```go
	"winters-backend/pkg/cache"
```

DB pool 생성/defer block 아래에 추가:

```go
	rdb, err := cache.NewClient(ctx, cfg.Redis)
	if err != nil {
		slog.Error("failed to connect redis", "error", err)
		os.Exit(1)
	}
	defer rdb.Close()
```

현재 repository 생성:

```go
	repository := replay.NewRepository(db)
```

아래로 교체:

```go
	repository := replay.NewRepository(db, rdb)
```

### 7-7-7. 보정 acceptance

1. `gofmt` 및 `go test ./...`, Client Release x64 `/m:1`을 통과한다.
2. replay service image를 rebuild/recreate하고 health를 확인한다.
3. Redis에 `matched + same match`, `matched + different match`, `queued`, key 없음을 준비하여 completion retry를 호출한다. 같은 match만 삭제되고 나머지는 보존되어야 한다.
4. 실제 Release client 3개가 각자 로그인한 채 같은 account replay를 열고 first full snapshot 이후 tick이 증가하며 예상 champion/structure world를 표시한다. `[ECS:RosterFallback]`과 `net=1005` ghost가 없어야 하고, lowest champion NetId에 `LocalPlayerTag`·HUD·follow target이 결합되어야 한다.
5. client 1만 pause/seek했을 때 client 2·3 tick은 계속 증가해야 하며, client 1은 seek full snapshot 재적용 뒤 같은 deterministic spectator focus를 다시 획득해야 한다.
6. 세 client가 `메인 메뉴로`를 눌러 원래 user ID session의 MainMenu로 복귀한다.
7. 즉시 세 client가 Game Start를 눌러 첫 match와 다른 match ID를 받고 두 번째 server room/in-game까지 진입한다.
8. 두 번째 nexus도 종료하여 두 번째 replay가 3계정 library에 표시되고, 완료 직후 Redis matched status 3개가 사라지는 것을 확인한다.

# 8. 2026-07-20 프로필 기본 폭과 Replay Chrono 상단 배치

① 문제·제약: 실측 1080×759 Release 창에서 저장된 작은 ImGui 크기와 78자 cache filename 때문에 `재생` 버튼이 잘리고, 176px 높이의 Chrono 창이 하단 HUD를 덮는다.
② 순진한 해법의 실패: `ImGuiCond_FirstUseEver` 수치만 키우면 기존 `imgui.ini`가 우선되어 이미 실행한 사용자에게 적용되지 않는다.
③ 메커니즘: MyInfo scene이 나타날 때 넓은 2열 레이아웃을 적용하고 replay 행은 고정 action 열로 만든다. Chrono는 최대 int32 Draw 문자열로 계산한 성능 overlay 우측과 상단 HUD 좌측 사이에 배치하고 긴 status는 짧은 표식+전체 tooltip으로 보존한다.
④ 대조: 매 프레임 `Always`로 프로필 창을 고정하지 않는다. 진입 시 기본값만 복원해 같은 scene 안의 수동 resize는 유지한다.
⑤ 대가: MyInfo 재진입은 사용자가 이전에 저장한 작은 layout도 의도적으로 넓은 entry layout으로 되돌린다. 공식 최소 해상도는 없어 1080×759보다 좁은 창의 완전 무스크롤 보장은 `CONFIRM_NEEDED`다.

## 8-1. 사용자 작업 계약

```text
사용자 작업: 플레이어가 프로필 진입 즉시 replay 파일명과 재생 버튼을 보고 실행하며, 인게임에서는 하단 HUD를 가리지 않고 Chrono를 조작한다.
대상 범위: 내 프로필, 전적 기록, 리플레이 3개 창과 Replay Chrono Break 창.
필수 데이터: 계정/전적, replay 생성시각·짧은 match ID·크기·재생 버튼, timeline/time/restart/pause/speed/복귀/status.
핵심 행동: 기존 재생·Chrono·프로필 복귀·메인 메뉴 복귀를 그대로 유지한다.
제외: replay 데이터/권위/카메라/버튼 동작 변경, 새 저장 옵션 추가.
권위/저장: Client presentation layout만 변경. gameplay/replay authority와 persist owner 변경 없음.
완료 증거: Release MyInfo 첫 진입과 replay 상단 패널을 캡처하고 HUD 비중첩·재생 버튼 노출을 확인한다.
```

| 범위 | 필수 | 변경 |
|---|---|---|
| MyInfo 왼쪽 열 | 프로필, 전적 | 화면 높이에 맞춰 세로 공간 확대 |
| MyInfo replay 열 | 3개 탭, 파일명, 재생 버튼 | 남는 화면 폭 전체 사용 |
| Replay Chrono | 기존 모든 control/status | 하단 중앙에서 좌상단 안전 영역으로 이동·압축 |
| 데이터/권위 | account library, WRPL player | 변경 없음 |

기본 화면:

```text
MyInfo                                      Replay
┌──────────────┐  ┌────────────────────────────────────┐
│ 내 프로필     │  │ [Cloud/account][내 리플레이][로컬] │
└──────────────┘  │ date · match 1c7678dd · size    재생 │
┌──────────────┐  │ date · match 4a150a39 · size    재생 │
│ 전적 기록     │  └────────────────────────────────────┘
└──────────────┘

Replay in-game
 [성능 overlay max] ┌─ Replay Chrono Break — dynamic×160 ┐ [상단 HUD]
│ timeline                                                │
│ time                                                     │
│ restart / pause / speed                                  │
│ 복귀                                                     │
│ status                                                   │
└─────────────────────────────────────────────────────────┘
                        [kill-feed는 y≈168부터]
                         gameplay view
                         bottom HUD (비중첩)
```

Primary/Secondary action 예산: 신규 action 0개. 기존 workflow control을 삭제·복제하지 않는다. view/draft/persist/apply/ack는 layout-only라 해당 없음이다.

## 8-2. 반영해야 하는 코드

### 8-2-1. `Client/Private/Scene/Scene_MyInfo.cpp`

`OnImGui()`의 `store/profile` 조회 바로 아래에 추가:

```cpp
	const ImGuiIO& io = ImGui::GetIO();
	constexpr f32_t kOuterMargin = 24.f;
	constexpr f32_t kColumnGap = 16.f;
	constexpr f32_t kContentTop = 96.f;
	constexpr f32_t kProfileHeight = 210.f;
	const f32_t contentHeight = (std::max)(
		420.f,
		io.DisplaySize.y - kContentTop - kOuterMargin);
	const f32_t leftWidth = std::clamp(
		io.DisplaySize.x * 0.30f,
		280.f,
		320.f);
	const f32_t replayX = kOuterMargin + leftWidth + kColumnGap;
	const f32_t replayWidth = (std::max)(
		320.f,
		io.DisplaySize.x - replayX - kOuterMargin);
	const f32_t historyY = kContentTop + kProfileHeight + kColumnGap;
	const f32_t historyHeight = (std::max)(
		180.f,
		contentHeight - kProfileHeight - kColumnGap);
```

현재 세 창의 position/size block:

```cpp
	ImGui::SetNextWindowPos(ImVec2(60.f, 110.f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(330.f, 210.f), ImGuiCond_FirstUseEver);
```

아래로 교체:

```cpp
	ImGui::SetNextWindowPos(
		ImVec2(kOuterMargin, kContentTop),
		ImGuiCond_Appearing);
	ImGui::SetNextWindowSize(
		ImVec2(leftWidth, kProfileHeight),
		ImGuiCond_Appearing);
```

```cpp
	ImGui::SetNextWindowPos(ImVec2(60.f, 340.f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(330.f, 300.f), ImGuiCond_FirstUseEver);
```

아래로 교체:

```cpp
	ImGui::SetNextWindowPos(
		ImVec2(kOuterMargin, historyY),
		ImGuiCond_Appearing);
	ImGui::SetNextWindowSize(
		ImVec2(leftWidth, historyHeight),
		ImGuiCond_Appearing);
```

```cpp
	ImGui::SetNextWindowPos(ImVec2(430.f, 110.f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(480.f, 530.f), ImGuiCond_FirstUseEver);
```

아래로 교체:

```cpp
	ImGui::SetNextWindowPos(
		ImVec2(replayX, kContentTop),
		ImGuiCond_Appearing);
	ImGui::SetNextWindowSize(
		ImVec2(replayWidth, contentHeight),
		ImGuiCond_Appearing);
```

`DrawCloudReplayItems()`의 현재 item loop:

```cpp
	for (const Client::CloudReplayItem& item : items)
	{
		ImGui::PushID(item.replayId.c_str());
		ImGui::Text("match %s | %.2f MiB | v%d",
			item.matchId.c_str(),
			static_cast<double>(item.sizeBytes) / (1024.0 * 1024.0),
			item.formatVersion);
		ImGui::SameLine();
		if (!backend.IsReplayRequestInFlight() && ImGui::Button("다시보기"))
			backend.RequestReplayPlayback(item);
		ImGui::PopID();
	}
```

아래로 교체:

```cpp
	const f32_t actionWidth =
		ImGui::CalcTextSize("다시보기").x +
		ImGui::GetStyle().FramePadding.x * 2.f;
	if (ImGui::BeginTable(
		"CloudReplayRows",
		2,
		ImGuiTableFlags_SizingStretchProp |
		ImGuiTableFlags_NoSavedSettings))
	{
		ImGui::TableSetupColumn("Replay", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn(
			"Action",
			ImGuiTableColumnFlags_WidthFixed,
			actionWidth);
		for (const Client::CloudReplayItem& item : items)
		{
			ImGui::PushID(item.replayId.c_str());
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			const std::string shortMatchId = item.matchId.size() > 8u
				? item.matchId.substr(0u, 8u)
				: item.matchId;
			const char* pCreatedAt = item.createdAt.empty()
				? "unknown time"
				: item.createdAt.c_str();
			ImGui::Text("%s | match %s | %.2f MiB",
				pCreatedAt,
				shortMatchId.c_str(),
				static_cast<double>(item.sizeBytes) / (1024.0 * 1024.0));
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip(
					"match %s\nreplay %s\ncreated %s\nformat v%d",
					item.matchId.c_str(),
					item.replayId.c_str(),
					pCreatedAt,
					item.formatVersion);
			}
			ImGui::TableSetColumnIndex(1);
			if (!backend.IsReplayRequestInFlight() && ImGui::Button("다시보기"))
				backend.RequestReplayPlayback(item);
			ImGui::PopID();
		}
		ImGui::EndTable();
	}
```

`DrawReplayItems()`의 현재 item loop:

```cpp
	for (const ReplayListItem& item : items)
	{
		ImGui::PushID(item.path.c_str());
		ImGui::TextUnformatted(item.displayName.c_str());
		ImGui::SameLine();
		if (ImGui::Button("재생"))
			OpenReplay(item.path);
		ImGui::PopID();
	}
```

아래로 교체:

```cpp
	const f32_t actionWidth =
		ImGui::CalcTextSize("재생").x +
		ImGui::GetStyle().FramePadding.x * 2.f;
	if (ImGui::BeginTable(
		"CachedReplayRows",
		2,
		ImGuiTableFlags_SizingStretchProp |
		ImGuiTableFlags_NoSavedSettings))
	{
		ImGui::TableSetupColumn("Replay", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn(
			"Action",
			ImGuiTableColumnFlags_WidthFixed,
			actionWidth);
		for (const ReplayListItem& item : items)
		{
			ImGui::PushID(item.path.c_str());
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			const std::string shortName = item.displayName.size() > 8u
				? item.displayName.substr(0u, 8u)
				: item.displayName;
			ImGui::Text("%s | %.2f MiB",
				shortName.c_str(),
				static_cast<double>(item.fileSizeBytes) / (1024.0 * 1024.0));
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", item.displayName.c_str());
			ImGui::TableSetColumnIndex(1);
			if (ImGui::Button("재생"))
				OpenReplay(item.path);
			ImGui::PopID();
		}
		ImGui::EndTable();
	}
```

### 8-2-2. `Client/Private/Scene/Scene_InGameImGui.cpp`

`DrawReplayControlPanel()` replay mode의 현재 size/position 계산:

```cpp
        const f32_t availableWidth = (std::max)(320.f, io.DisplaySize.x - 40.f);
        const ImVec2 windowSize((std::min)(920.f, availableWidth), 176.f);
        ImGui::SetNextWindowPos(
            ImVec2(
                (io.DisplaySize.x - windowSize.x) * 0.5f,
                io.DisplaySize.y - windowSize.y - 20.f),
            ImGuiCond_Always);
```

아래로 교체:

```cpp
        constexpr f32_t kPanelTop = 4.f;
        constexpr f32_t kPerfOverlayLeft = 10.f;
        constexpr f32_t kPanelGap = 16.f;
        constexpr f32_t kMatchHudLeftReserve = 400.f;
        const f32_t panelLeft =
            kPerfOverlayLeft +
            ImGui::CalcTextSize(
                "Draw: 2147483647 verts, 2147483647 indices").x +
            ImGui::GetStyle().WindowPadding.x * 2.f +
            kPanelGap;
        const f32_t panelRight =
            io.DisplaySize.x - kMatchHudLeftReserve - kPanelGap;
        const f32_t availableWidth = (std::max)(
            240.f,
            panelRight - panelLeft);
        const ImVec2 windowSize((std::min)(640.f, availableWidth), 160.f);
        ImGui::SetNextWindowPos(
            ImVec2(panelLeft, kPanelTop),
            ImGuiCond_Always);
```

현재 `Time`과 playback control block:

```cpp
            ImGui::Text("Time: %.2f / %.2f sec", currentSec, totalSec);

            if (ImGui::Button("Restart"))
            {
                if (SeekReplayToTick(firstTick))
                    m_pReplayPlayer->SetPaused(false);
            }
            ImGui::SameLine();
            const bool_t bPaused = m_pReplayPlayer->IsPaused();
            if (ImGui::Button(bPaused ? "Play" : "Pause"))
                m_pReplayPlayer->SetPaused(!bPaused);
            ImGui::SameLine();
            f32_t speed = m_pReplayPlayer->GetPlaybackRate();
            ImGui::SetNextItemWidth(180.f);
            if (ImGui::SliderFloat("Speed", &speed, 0.25f, 4.f, "%.2fx"))
                m_pReplayPlayer->SetPlaybackRate(speed);
```

아래로 교체:

```cpp
            ImGui::Text("Time: %.2f / %.2f sec", currentSec, totalSec);
            if (ImGui::Button("Restart"))
            {
                if (SeekReplayToTick(firstTick))
                    m_pReplayPlayer->SetPaused(false);
            }
            ImGui::SameLine();
            const bool_t bPaused = m_pReplayPlayer->IsPaused();
            if (ImGui::Button(bPaused ? "Play" : "Pause"))
                m_pReplayPlayer->SetPaused(!bPaused);
            ImGui::SameLine();
            f32_t speed = m_pReplayPlayer->GetPlaybackRate();
            ImGui::SetNextItemWidth(80.f);
            if (ImGui::SliderFloat("##ReplaySpeed", &speed, 0.25f, 4.f, "%.2fx"))
                m_pReplayPlayer->SetPlaybackRate(speed);
```

현재 복귀 버튼 block 뒤 status 출력:

```cpp
            if (ImGui::Button("프로필로 돌아가기"))
                m_bExitReplayToMyInfoRequested = true;
            ImGui::SameLine();
            if (ImGui::Button("메인 메뉴로"))
                m_bReturnToMainMenuRequested = true;
        }

        ImGui::TextUnformatted(
            m_strReplayStatus.empty() ? "Replay playback" : m_strReplayStatus.c_str());
```

아래로 교체:

```cpp
            if (ImGui::Button("프로필로 돌아가기"))
                m_bExitReplayToMyInfoRequested = true;
            ImGui::SameLine();
            if (ImGui::Button("메인 메뉴로"))
                m_bReturnToMainMenuRequested = true;
        }

        const char* pStatus = m_strReplayStatus.empty()
            ? "Replay playback"
            : m_strReplayStatus.c_str();
        if (ImGui::CalcTextSize(pStatus).x <= ImGui::GetContentRegionAvail().x)
        {
            ImGui::TextUnformatted(pStatus);
        }
        else
        {
            ImGui::TextUnformatted("Replay status (hover for details)");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", pStatus);
        }
```

## 8-3. 검증

예측:

- 1080×759, 96 DPI Release MyInfo 첫 진입에서 왼쪽 열은 약 320px, replay 열은 약 696px, 높이는 약 639px이며 longest cache filename에도 action 열의 `재생` 버튼이 보인다.
- 작은 크기를 seed한 `imgui.ini`가 있어도 leave/re-enter 시 `ImGuiCond_Appearing`으로 entry layout을 회복하고, 같은 scene 안에서는 resize가 가능하다. 재진입마다 persisted user resize를 버리는 것은 의도된 정책이다.
- Cloud 행은 생성시각·짧은 match ID·크기를 표시하고 hover tooltip으로 full match/replay ID를 제공한다. cache 행은 짧은 match ID·크기와 full filename tooltip을 제공한다.
- Replay Chrono Break는 최대 `Draw: 2147483647 verts, 2147483647 indices`의 실제 font width+padding+16px 뒤에서 시작하고 상단 HUD 시작 x=`DisplaySize.x-400`보다 16px 앞에서 끝난다. y는 `[4,164]`라 첫 kill-feed banner 이론상 상단 y≈168보다 4px 위이며 하단 HUD와도 겹치지 않는다. 긴 status도 독립 행의 hover tooltip으로 전문 접근이 가능하다.
- Client presentation-only 변경이므로 replay checksum, service/DB/Redis에는 변동이 없다.

검증 명령:

- `msbuild Client/Include/Client.vcxproj /p:Configuration=Release /p:Platform=x64 /m:1`
- 관련 파일 `git diff --check`
- Release MyInfo에 작은 `imgui.ini`를 seed한 첫 진입·leave/re-enter, longest filename row, empty/error 상태 캡처
- replay 실행 후 Chrono와 성능 overlay·상단 HUD·active kill-feed가 함께 있는 캡처

미검증: 공식 최소 해상도는 프로젝트에 선언이 없어 관측 환경 1080×759·96 DPI만 수동 판정한다.

확인 필요: 없음.

## 8-4. 서브 에이전트 비평

비평 주체: `/root/replay_plan_critique`.

- P1 `696px만으로 78자 cache filename 뒤 재생 버튼 보장 불가`: 수용. Cloud/cache 모두 2열 table과 고정 action 열을 사용하고 full ID/name은 tooltip으로 이동했다.
- P1 `중앙 x, y=20 Chrono가 상단 HUD와 kill-feed에 겹침`: 수용. observed 1080×759에서 Chrono를 `[20,20]..[660,148]`로 압축·이동해 HUD와 첫 banner 사이에 각각 20px를 확보했다.
- P2 `Appearing은 저장 default가 아니라 매 진입 reset`: 수용. ⑤와 acceptance에 의도된 entry reset 정책을 명시했다.
- P2 `DPI·seeded-small·longest row·empty/error·HUD/kill-feed QA 누락`: 수용. 96 DPI와 각 캡처 조건을 검증에 추가했다.
- 재비평 P1 `좌상단 Chrono가 (10,30) 성능 overlay와 겹침`: 수용. x=220, y=12, 1080 폭 440×144로 옮기고 time/control을 별도 행으로 만들어 네 content 행이 맞도록 보정했다.
- 재비평 P1 `x=220이 auto-size 성능 overlay 실제 우측과 겹침`: 수용. 실측 우측 약 x=284 뒤인 x=300으로 이동했다.
- 재비평 P1 `return+status 한 행에서 긴 정상 status가 잘림`: 수용. 360×160의 5행 layout으로 바꾸고 status를 독립 행으로 복원했다.
- 재비평 P1 `성능 overlay 우측이 Draw 자릿수에 따라 증가`: 수용. 실제 font로 최대 non-negative int32 Draw 문자열 폭과 window padding을 계산해 16px 뒤에서 panel을 시작한다.
- 재비평 P1 `360px 독립 행도 긴 status/error 전문을 보장하지 않음`: 수용. 한 행에 맞으면 전문, 넘으면 `Replay status (hover for details)`와 full tooltip을 표시한다.
- 최종 재비평 P1 `1080 폭의 약 258px content에서 보이는 Speed label까지 합치면 control 행이 약 17px 잘림`: 수용. 값은 slider 내부 `%.2fx`로 유지하고 ID는 `##ReplaySpeed`로 숨겨 Restart·Pause·80px slider 행을 content 폭 안에 둔다.
- 최종 재비평 P1 `status 현재/교체 block 역전`: 수용. 실제 현재 source를 단순 `TextUnformatted`로, 교체 후 코드를 짧은 라벨+full tooltip fallback으로 바로잡았다.

수정본 재비평 상태: `P0 0 / P1 0 — PASS`.

# 2026-07-20 Dynamic authenticated party size closure

Session - Release authenticated matchmaking must form one replay-bearing match from the clients that actually joined, without a fixed 3-client requirement
Goal: none · Axis: C5 exact count / C7 authority and consistency / C8 verification bottleneck
Related: `2026-07-19_RELEASE_AUTHENTICATED_ACCOUNT_REPLAY_E2E_PLAN.md` / same-name RESULT

## 9-1. Decision record

- Problem/constraint: `WINTERS_MATCH_SIZE=3` persisted in the Docker matchmaking container after the three-client acceptance run. With two clients the queue could never complete; with one or four clients the operator had to recreate the service with a different exact count.
- Evidence: replay is not three-player-specific. `CGameRoom::FinalizeReplayRecorder()` iterates `m_authenticatedParticipants`; replay completion accepts 1..10 players; `Repository::MarkReady()` grants access from every `match_participants` row. Only matchmaking used an exact `matchSize` threshold.
- Mechanism: replace exact size with a cohort window. The oldest queued account opens a five-second assembly window. All compatible accounts joining inside that window, capped at ten, form one match at the deadline. A singleton starts after five seconds, while 2/3/4 clients clicked within the same window share one match and one replay artifact.
- Race boundary: joining/leaving increments a Redis queue revision. The Lua claim validates the observed revision before claiming a cohort, so a client that joins between candidate read and claim forces a retry rather than being split accidentally.
- Tradeoff: an automatic queue cannot know whether the first user intends solo or is waiting for more users. The five-second cohort window is the bounded inference rule; a future party/lobby Ready contract can replace it without changing replay ownership.
- Authority/integration: matchmaking owns participant grouping and signed tickets; Server owns the authoritative roster and one WRPL; replay service creates per-account library ACLs. Client replay playback remains unchanged.
- Exit cost: restore exact-size matchmaking by replacing the cohort selector with an exact threshold; replay upload, storage, and client playback require no rollback.

## 9-2. Source changes

### `Services/pkg/config/config.go`

In `GameSessionConfig`, replace `MatchSize int` with:

```go
	MatchMaxSize        int
	MatchAssemblyWindow time.Duration
```

Replace the exact-size parse with:

```go
	matchMaxSize, _ := strconv.Atoi(getEnv("WINTERS_MATCH_MAX_SIZE", "10"))
	matchAssemblyWindow, _ := time.ParseDuration(
		getEnv("WINTERS_MATCH_ASSEMBLY_WINDOW", "5s"))
```

Populate those fields and replace exact-size validation with:

```go
	if cfg.GameSession.MatchMaxSize < 1 || cfg.GameSession.MatchMaxSize > 10 {
		return nil, fmt.Errorf("WINTERS_MATCH_MAX_SIZE must be between 1 and 10")
	}
	if cfg.GameSession.MatchAssemblyWindow <= 0 ||
		cfg.GameSession.MatchAssemblyWindow > time.Minute {
		return nil, fmt.Errorf("WINTERS_MATCH_ASSEMBLY_WINDOW must be between 1ns and 1m")
	}
```

### `Services/cmd/matchmaking/main.go`

Replace the final `NewService` argument with:

```go
		cfg.GameSession.MatchMaxSize,
		cfg.GameSession.MatchAssemblyWindow)
```

### `Services/internal/matchmaking/service.go`

- Add `queueRevisionKey = "matchmaking:queue:revision"`.
- `Join` writes queue/status/join time and increments the revision in one Redis transaction.
- `Leave` and failed-claim restore increment the same revision in Lua.
- `claimPlayersScript` receives the observed revision and refuses the claim when the queue changed.
- Replace `matchSize` with `matchMaxSize` and `matchAssemblyWindow`.
- Add this candidate shape and selector; `tryMatch` loads all join times, calls the selector, then claims every returned group.

```go
type queuedMatchCandidate struct {
	entry    redis.Z
	joinedAt time.Time
}

func buildReadyMatchGroups(
	candidates []queuedMatchCandidate,
	now time.Time,
	assemblyWindow time.Duration,
	maxMatchSize int,
) [][]redis.Z {
	if len(candidates) == 0 || assemblyWindow <= 0 || maxMatchSize <= 0 {
		return nil
	}

	ordered := append([]queuedMatchCandidate(nil), candidates...)
	sort.SliceStable(ordered, func(i, j int) bool {
		if ordered[i].joinedAt.Equal(ordered[j].joinedAt) {
			return fmt.Sprint(ordered[i].entry.Member) <
				fmt.Sprint(ordered[j].entry.Member)
		}
		return ordered[i].joinedAt.Before(ordered[j].joinedAt)
	})

	used := make([]bool, len(ordered))
	groups := make([][]redis.Z, 0, len(ordered))
	for i := range ordered {
		if used[i] {
			continue
		}
		deadline := ordered[i].joinedAt.Add(assemblyWindow)
		if now.Before(deadline) {
			break
		}
		waitSeconds := int(now.Sub(ordered[i].joinedAt) / time.Second)
		allowedRange := baseRange + (waitSeconds/30)*rangeExpand
		group := []redis.Z{ordered[i].entry}
		used[i] = true
		for j := i + 1; j < len(ordered) && len(group) < maxMatchSize; j++ {
			if used[j] || ordered[j].joinedAt.After(deadline) {
				continue
			}
			mmrDiff := ordered[j].entry.Score - ordered[i].entry.Score
			if mmrDiff < 0 {
				mmrDiff = -mmrDiff
			}
			if int(mmrDiff) <= allowedRange {
				group = append(group, ordered[j].entry)
				used[j] = true
			}
		}
		groups = append(groups, group)
	}
	return groups
}
```

### `Services/docker-compose.yml`

Replace the persistent exact size with policy bounds:

```yaml
  WINTERS_MATCH_MAX_SIZE: "${WINTERS_MATCH_MAX_SIZE:-10}"
  WINTERS_MATCH_ASSEMBLY_WINDOW: "${WINTERS_MATCH_ASSEMBLY_WINDOW:-5s}"
```

### `Tools/Harness/StartReleaseAccountReplayCapture.ps1`

- Change `HumanPlayers` validation to 1..10.
- Stop exporting/saving `WINTERS_MATCH_SIZE`; the parameter remains only an expected-client count for capture reporting.
- Report the dynamic assembly policy and never leave an exact participant count in Docker.

### `Services/internal/matchmaking/service_test.go` (new file)

Complete intended body:

```go
package matchmaking

import (
	"fmt"
	"testing"
	"time"

	"github.com/redis/go-redis/v9"
)

func testQueuedCandidates(count int, joinedAt time.Time) []queuedMatchCandidate {
	candidates := make([]queuedMatchCandidate, count)
	for i := range candidates {
		candidates[i] = queuedMatchCandidate{
			entry: redis.Z{Score: 1000, Member: fmt.Sprintf("player-%02d", i)},
			joinedAt: joinedAt.Add(time.Duration(i) * 100 * time.Millisecond),
		}
	}
	return candidates
}

func TestBuildReadyMatchGroupsSupportsOneThroughFourPlayers(t *testing.T) {
	joinedAt := time.Unix(1000, 0)
	for count := 1; count <= 4; count++ {
		t.Run(fmt.Sprintf("players_%d", count), func(t *testing.T) {
			groups := buildReadyMatchGroups(
				testQueuedCandidates(count, joinedAt),
				joinedAt.Add(6*time.Second),
				5*time.Second,
				10)
			if len(groups) != 1 || len(groups[0]) != count {
				t.Fatalf("groups = %v, want one group of %d", groups, count)
			}
		})
	}
}

func TestBuildReadyMatchGroupsWaitsForOldestDeadline(t *testing.T) {
	joinedAt := time.Unix(1000, 0)
	groups := buildReadyMatchGroups(
		testQueuedCandidates(4, joinedAt),
		joinedAt.Add(4*time.Second),
		5*time.Second,
		10)
	if len(groups) != 0 {
		t.Fatalf("groups = %v before assembly deadline", groups)
	}
}

func TestBuildReadyMatchGroupsLeavesLateJoinForNextCohort(t *testing.T) {
	joinedAt := time.Unix(1000, 0)
	candidates := testQueuedCandidates(2, joinedAt)
	candidates[1].joinedAt = joinedAt.Add(6 * time.Second)
	groups := buildReadyMatchGroups(
		candidates,
		joinedAt.Add(6*time.Second),
		5*time.Second,
		10)
	if len(groups) != 1 || len(groups[0]) != 1 {
		t.Fatalf("groups = %v, want ready singleton and pending late join", groups)
	}
}

func TestBuildReadyMatchGroupsCapsCohortAtMaximum(t *testing.T) {
	joinedAt := time.Unix(1000, 0)
	groups := buildReadyMatchGroups(
		testQueuedCandidates(4, joinedAt),
		joinedAt.Add(6*time.Second),
		5*time.Second,
		3)
	if len(groups) != 2 || len(groups[0]) != 3 || len(groups[1]) != 1 {
		t.Fatalf("groups = %v, want group sizes 3 and 1", groups)
	}
}
```

### `Services/internal/replay/service_test.go`

Extend `fakeRepository` to capture `CompleteMatch` players, then add a table test that calls `Service.CompleteMatch` with 1, 2, 3, and 4 players and verifies the exact count reaches the repository. This locks replay to dynamic participant counts rather than a three-player fixture.

## 9-3. Verification

Expected observations:

- One account: after about five seconds, status becomes `matched`; the DB match has one participant.
- Two, three, or four accounts joining inside one window: every account receives the same match/game-session ID and a distinct signed ticket; DB participant count equals the number joined.
- Replay completion accepts the same 1/2/3/4 participant lists and creates one account-library row per match participant.
- A join/leave concurrent with claim cannot create a partial group; revision mismatch retries on the next matcher tick.

Commands:

- `cd Services && gofmt -w cmd/matchmaking/main.go internal/matchmaking/service.go internal/matchmaking/service_test.go internal/replay/service_test.go pkg/config/config.go pkg/config/config_test.go`
- `cd Services && go test ./...`
- `cd Services && docker compose --profile app up -d --build --force-recreate matchmaking replay`
- Live HTTP/Redis/PostgreSQL matrix for cohort sizes 1, 2, 3, 4 with a short test assembly window.
- `msbuild Server/Include/Server.vcxproj /p:Configuration=Release /p:Platform=x64 /m:1`
- `msbuild Client/Include/Client.vcxproj /p:Configuration=Release /p:Platform=x64 /m:1`
- relevant `git diff --check`.

Unverified before implementation: actual one-player GUI+nexus replay playback. Backend/replay count contracts and Release builds are mandatory; existing three-account full GUI playback remains valid because playback is count-independent.

## 9-4. Independent sub-agent critique

Critic: `/root/replay_plan_critique`.

Status: pending. Implementation must not begin until accepted/held P0 and P1 findings are zero.

## 9-5. First critique disposition and revised atomic design

First critique: `/root/replay_plan_critique` reported `P0 1 / P1 4 — FAIL`.

- P0 missing `config_test.go` migration: accepted. Exact test replacements are added below.
- P1 queue-revision ordering and participant-by-participant restore: accepted. The revised order is `GET revision -> ZRANGE -> MGET join times -> single claim Lua(compare revision, validate all, remove all, set all, INCR once)`. Failed DB creation uses one restore Lua that validates every claiming status before restoring every member and increments once.
- P1 selector-only tests do not cover Redis races and second-resolution timestamps: accepted. Join time changes to Unix milliseconds with a legacy-seconds parser; exact deadline tests and opt-in live Redis integration tests are mandatory.
- P1 multiple cohorts conflict with one fixed game session/server: accepted. One matcher tick may allocate only one cohort. Before selection it queries for an existing `allocated`/`running` match on the fixed `game_session_id`; queued leftovers wait until replay completion marks the active match completed.
- P1 ambient policy leak and insufficient ACL proof: accepted. The launcher explicitly sets and verifies max=10/window=5s. PostgreSQL integration verifies N library rows, participant authorization, and nonparticipant denial for N=1/2/3/4.
- P2 max boundary: accepted. Selector tests include 10 and 11 candidates; only the first ten are selected and the remainder stay queued behind the active allocation.

This section supersedes conflicting plural-group or participant-by-participant restore wording in §9-1..§9-3.

### Revised selector contract

`buildReadyMatchGroup` returns at most one group. It orders candidates by millisecond join time, waits until the oldest candidate's five-second deadline, selects compatible candidates that joined no later than that deadline up to `MatchMaxSize`, and leaves every other candidate queued. `tryMatch` returns after the single claim/create/restore attempt.

```go
type queuedMatchCandidate struct {
	entry    redis.Z
	joinedAt time.Time
}

func buildReadyMatchGroup(
	candidates []queuedMatchCandidate,
	now time.Time,
	assemblyWindow time.Duration,
	maxMatchSize int,
) []redis.Z {
	if len(candidates) == 0 || assemblyWindow <= 0 || maxMatchSize <= 0 {
		return nil
	}
	ordered := append([]queuedMatchCandidate(nil), candidates...)
	sort.SliceStable(ordered, func(i, j int) bool {
		if ordered[i].joinedAt.Equal(ordered[j].joinedAt) {
			return fmt.Sprint(ordered[i].entry.Member) <
				fmt.Sprint(ordered[j].entry.Member)
		}
		return ordered[i].joinedAt.Before(ordered[j].joinedAt)
	})
	deadline := ordered[0].joinedAt.Add(assemblyWindow)
	if now.Before(deadline) {
		return nil
	}
	waitSeconds := int(now.Sub(ordered[0].joinedAt) / time.Second)
	allowedRange := baseRange + (waitSeconds/30)*rangeExpand
	group := []redis.Z{ordered[0].entry}
	for i := 1; i < len(ordered) && len(group) < maxMatchSize; i++ {
		if ordered[i].joinedAt.After(deadline) {
			continue
		}
		mmrDiff := ordered[i].entry.Score - ordered[0].entry.Score
		if mmrDiff < 0 {
			mmrDiff = -mmrDiff
		}
		if int(mmrDiff) <= allowedRange {
			group = append(group, ordered[i].entry)
		}
	}
	return group
}
```

### Exact config test replacement

In `Services/pkg/config/config_test.go`, replace `TestLoadGameSessionCaptureOverrides` and `TestLoadRejectsInvalidMatchSize` with:

```go
func TestLoadGameSessionDynamicMatchOverrides(t *testing.T) {
	t.Setenv("WINTERS_ENV", "development")
	t.Setenv("WINTERS_GAME_SESSION_ID", "capture-session-01")
	t.Setenv("WINTERS_MATCH_MAX_SIZE", "4")
	t.Setenv("WINTERS_MATCH_ASSEMBLY_WINDOW", "2500ms")

	cfg, err := Load()
	if err != nil {
		t.Fatalf("Load() error = %v", err)
	}
	if cfg.GameSession.GameSessionID != "capture-session-01" ||
		cfg.GameSession.MatchMaxSize != 4 ||
		cfg.GameSession.MatchAssemblyWindow != 2500*time.Millisecond {
		t.Fatalf("dynamic match config = %+v", cfg.GameSession)
	}
}

func TestLoadDynamicMatchDefaults(t *testing.T) {
	t.Setenv("WINTERS_ENV", "development")
	for _, key := range []string{"WINTERS_MATCH_MAX_SIZE", "WINTERS_MATCH_ASSEMBLY_WINDOW"} {
		unsetEnv(t, key)
	}
	cfg, err := Load()
	if err != nil {
		t.Fatal(err)
	}
	if cfg.GameSession.MatchMaxSize != 10 ||
		cfg.GameSession.MatchAssemblyWindow != 5*time.Second {
		t.Fatalf("dynamic match defaults = %+v", cfg.GameSession)
	}
}

func TestLoadRejectsInvalidDynamicMatchPolicy(t *testing.T) {
	for _, test := range []struct{ max, window string }{
		{"0", "5s"}, {"11", "5s"}, {"10", "0s"}, {"10", "61s"},
	} {
		t.Run(test.max+"_"+test.window, func(t *testing.T) {
			t.Setenv("WINTERS_ENV", "development")
			t.Setenv("WINTERS_MATCH_MAX_SIZE", test.max)
			t.Setenv("WINTERS_MATCH_ASSEMBLY_WINDOW", test.window)
			if _, err := Load(); err == nil {
				t.Fatal("Load() accepted invalid dynamic match policy")
			}
		})
	}
}
```

Add `time` to that test file's imports.

### Redis atomic integration contract

`Services/internal/matchmaking/service_test.go` remains the one new matchmaking test file, but its complete implementation must additionally provide opt-in tests under `WINTERS_TEST_REDIS_ADDR` that:

1. seed a queued group and revision R;
2. mutate join/leave to R+1 after the reader captured R and prove claim(R) changes nothing;
3. claim two users atomically and prove queue has neither and both statuses are `claiming:<match>` with exactly one revision increment;
4. run whole-group restore and prove both queue members/statuses return together with exactly one revision increment;
5. verify millisecond deadline at `deadline-1ms`, `deadline`, and `deadline+1ms`;
6. verify 1/2/3/4, max 10, and 11 candidates selects only ten.

The test uses only keys under `matchmaking:test:<run-id>:` by parameterizing the Lua helper keys; it must not flush shared Redis or touch live `matchmaking:*` keys.

### Single-server allocation guard

Before queue selection, `tryMatch` executes an `EXISTS` query for `matches.game_session_id = allocation.GameSessionID AND status IN ('allocated','running')`. When true, it returns without claiming. After a successful create it returns immediately. Completion already changes the row to `completed`, which unlocks the next cohort. The live matrix must prove a second queued cohort remains queued while the first match is allocated and is matched only after the first row becomes completed.

### Launcher policy ownership

`StartReleaseAccountReplayCapture.ps1` must save, set, and restore these process variables:

```powershell
"WINTERS_MATCH_MAX_SIZE",
"WINTERS_MATCH_ASSEMBLY_WINDOW"
```

Before Docker recreation it sets:

```powershell
$env:WINTERS_MATCH_MAX_SIZE = "10"
$env:WINTERS_MATCH_ASSEMBLY_WINDOW = "5s"
```

After startup it reads the matchmaking container environment and fails unless the two values are exactly `10` and `5s`. `WINTERS_MATCH_SIZE` is neither read nor written by the new service.

### PostgreSQL replay ACL integration

Add `Services/internal/replay/repository_integration_test.go`, gated by `WINTERS_TEST_DATABASE_URL`. For participant counts 1, 2, 3, and 4 it creates isolated users/match/replay rows, calls `Repository.MarkReady`, and proves:

- `replay_user_library` row count equals N;
- `GetAuthorized` succeeds for every participant;
- `GetAuthorized` returns `apperr.ErrNotFound` for an outsider;
- test rows are removed by UUID-scoped cleanup.

The live verification runs this integration test against the Docker PostgreSQL instance; a skipped integration test is not a PASS for this session.

### Revised verification gates

- `go test ./...` baseline and post-change PASS.
- `WINTERS_TEST_REDIS_ADDR=127.0.0.1:6379 go test ./internal/matchmaking -run Redis -count=1` PASS.
- `WINTERS_TEST_DATABASE_URL=postgres://winters:winters_dev_2026@127.0.0.1:5433/winters?sslmode=disable go test ./internal/replay -run RepositoryDynamicParticipantACL -count=1` PASS.
- Docker matchmaking image recreated with `WINTERS_MATCH_MAX_SIZE=10`, `WINTERS_MATCH_ASSEMBLY_WINDOW=5s`, and no `WINTERS_MATCH_SIZE`.
- Live authenticated HTTP matrix 1/2/3/4 returns one common match per cohort and exact DB participant counts.
- Active-allocation hold/release test PASS.
- Release Server and Client `/m:1` builds, followed by `git diff --check`, PASS.

Re-critique status: pending. Source editing remains blocked until residual P0/P1 is zero.

## 9-6. Second critique disposition and allocation uniqueness

Second critique: `/root/replay_plan_critique` reported `P0 1 / P1 2 — FAIL`.

- P0 outsider error mismatch: accepted. `Repository.GetAuthorized()` intentionally maps an absent ACL row to `apperr.ErrForbidden`; the integration expectation is corrected from `ErrNotFound` to `ErrForbidden`.
- P1 non-atomic allocation `EXISTS`: accepted. The precheck remains an optimization only. A PostgreSQL partial unique index becomes the authority: at most one `allocated`/`running` row may exist per `game_session_id`. Competing matchers may claim different Redis cohorts, but only one DB insert commits; every loser executes the new atomic whole-group restore.
- P1 second match rejected by stale room identity: rejected with current code and runtime evidence. `CGameRoom::OnSessionLeave()` calls `ResetMatchStateLocked()` after the ended match's last session leaves; `ResetMatchStateLocked()` clears both `m_matchID` and `m_gameSessionID`, rebuilds lobby authority, and the prior Release three-client acceptance successfully entered a second signed match on the same server. The allocation index unlocks only when replay completion marks the first DB row `completed`; the live gate still repeats completion -> all clients leave -> server reset marker -> second assignment -> UDP handshake, so this rejection is evidence-bound rather than assumed.
- Empty `GameSessionID`: accepted as an unsupported authenticated Release allocation state. The launcher and Release server already require a non-empty shared game session ID. Matchmaking startup now rejects empty `WINTERS_GAME_SESSION_ID` when dynamic fixed-server matching is enabled instead of inventing per-match IDs for the same port.

### New migration files

`Services/migrations/000011_unique_active_game_session.up.sql` complete body:

```sql
CREATE UNIQUE INDEX uq_matches_active_game_session
    ON matches(game_session_id)
    WHERE game_session_id IS NOT NULL
      AND status IN ('allocated', 'running');
```

`Services/migrations/000011_unique_active_game_session.down.sql` complete body:

```sql
DROP INDEX IF EXISTS uq_matches_active_game_session;
```

`createMatch` treats PostgreSQL unique-violation as an allocation conflict, returns a typed/recognizable error to `tryMatch`, and the caller restores the entire claimed cohort with one Lua call. No partial match row or participant row commits because insertion and participants remain in one transaction.

### Corrected ACL expectation

The repository integration assertion is:

```go
	if _, err := repository.GetAuthorized(ctx, replayID, outsiderID);
		!errors.Is(err, apperr.ErrForbidden) {
		t.Fatalf("outsider authorization error = %v, want forbidden", err)
	}
```

### Additional mandatory gates

- Start two matcher service instances against the same Redis/PostgreSQL and one fixed `game_session_id`; enqueue two ready cohorts concurrently. Assert exactly one active match row, one complete participant set, no partial DB rows, and the losing cohort fully restored.
- Complete the winning match, disconnect its clients, observe `[GameRoom] Match reset after game end; lobby back to SeatSelect`, then prove the waiting cohort receives a new match and completes signed UDP handshake.
- Migration up/down/up applies cleanly against the current Docker database.

Final re-critique status: pending. Source editing remains blocked until residual accepted/held P0/P1 is zero.

## 9-7. Third critique disposition: server-ready lease and explicit fixed-session contract

Third critique: `/root/replay_plan_critique` reported `P0 0 / P1 2 — FAIL`.

- P1 DB completion precedes room reset: accepted. A completed replay row is no longer used as server-capacity truth. Redis owns a per-game-session allocation lease from atomic cohort claim until the authoritative server has no sessions, has called `ResetMatchStateLocked()`, and posts an authenticated ready notification. The matcher cannot claim another cohort while that lease exists.
- P1 empty fixed `GameSessionID`: accepted. The current backend is explicitly one fixed local game server, not an allocator. `WINTERS_GAME_SESSION_ID` becomes non-empty by contract; development/Compose defaults to `winters-local-game-session`, the Release capture launcher overrides it with a per-run UUID, and matchmaking startup rejects an empty value. The unsupported empty-ID fallback to `matchID.String()` is removed.

This section supersedes §9-5/§9-6 language saying DB `completed` unlocks allocation. DB completion makes replay/account data ready; only the post-reset server callback makes the fixed game server available for the next match.

### Allocation lease state machine

```text
queue window closes
  -> atomic Redis claim validates revision + all members + no allocation lease
  -> Redis lease matchmaking:allocation:<game-session-id> = <match-id>
  -> transactional DB match/participants/outbox insert
  -> clients receive signed match tickets
  -> nexus ends game; replay upload completes; DB match becomes completed
  -> last game session disconnects
  -> GameRoom copies match/session IDs, resets world and clears identity
  -> server asynchronously POSTs /internal/game-sessions/<session-id>/ready
  -> matchmaking atomically deletes lease only when its value equals <match-id>
  -> next queued cohort may be claimed
```

The ready callback body is exactly:

```json
{"match_id":"<uuid>"}
```

It uses the existing `REPLAY_INTERNAL_TOKEN`/`WINTERS_REPLAY_INTERNAL_TOKEN` bearer secret as the shared server-to-backend internal credential. The server reads a new `WINTERS_MATCHMAKING_SERVICE_URL` URL; the Release capture launcher sets it to `http://127.0.0.1:8083`. Compose already exposes the same internal token to matchmaking through the common environment.

### Server callback ordering and retry contract

In `CGameRoom::OnSessionLeave`, when the last session leaves an ended game, copy `m_matchID` and `m_gameSessionID`, call `ResetMatchStateLocked()`, then enqueue the copied pair to `CReplayUploadQueue::NotifyGameSessionReady`. No network request occurs while holding `m_stateMutex`.

`ReplayUploadQueue` gains a second bounded work-item deque for ready notifications. The existing worker processes replay uploads and ready callbacks; ready callbacks receive bounded retry/backoff and never release a lease locally. A failed callback keeps the lease closed (safe failure) and logs `[MatchmakingReady] failed`; a successful callback logs the session and match. Shutdown with drain processes both queues. This avoids a detached thread and avoids blocking the GameRoom lock.

The new matchmaking internal route uses constant-time bearer validation, parses the session ID from the route and a UUID match ID from the body, then invokes one Lua compare-delete:

```lua
if redis.call('GET', KEYS[1]) ~= ARGV[1] then
    return 0
end
redis.call('DEL', KEYS[1])
redis.call('INCR', KEYS[2])
return 1
```

A mismatched/stale match ID returns conflict and cannot unlock a newer allocation. An already-cleared lease is idempotent only for the same completed notification during retry: the handler reports success after confirming there is no active DB `allocated`/`running` row for that session; otherwise it reports conflict. The route is mounted outside JWT user routes under `/internal` and is protected only by the internal bearer credential.

### Atomic claim/restore amendment

The cohort claim Lua receives the queue key, revision key, allocation lease key, status keys, expected revision, game-session ID, match ID, TTL, and all members. It performs all validation before mutation, sets the lease and all claiming statuses, removes the whole cohort, and increments the revision once. The whole-group restore Lua validates the lease and every claiming status before restoring all members, deleting the lease, and incrementing once. PostgreSQL migration `000011` remains the second authority against a lost/replaced Redis dataset or competing service instance.

### Exact configuration direction

In `Services/pkg/config/config.go`, replace `MatchSize int` with:

```go
MatchMaxSize        int
MatchAssemblyWindow time.Duration
```

Development defaults are `GameSessionID=winters-local-game-session`, `MatchMaxSize=10`, and `MatchAssemblyWindow=5s`. Validation requires a non-empty ID of at most 64 bytes, max size 1..10, and window greater than zero and at most one minute. `WINTERS_MATCH_SIZE` is removed from code, Compose, launcher, and tests.

In `Services/docker-compose.yml`, the common environment becomes:

```yaml
WINTERS_GAME_SESSION_ID: "${WINTERS_GAME_SESSION_ID:-winters-local-game-session}"
WINTERS_MATCH_MAX_SIZE: "${WINTERS_MATCH_MAX_SIZE:-10}"
WINTERS_MATCH_ASSEMBLY_WINDOW: "${WINTERS_MATCH_ASSEMBLY_WINDOW:-5s}"
```

### Additional acceptance tests

- Unit: internal ready route rejects missing/wrong tokens; accepts the correct token; stale match ID cannot clear a lease.
- Redis integration: claim creates the session lease; a second claimant cannot claim; DB `completed` alone does not remove it; correct ready notification removes it; stale notification cannot remove a later lease.
- Live fixed-server lifecycle: cohort A completes replay but remains connected, cohort B stays queued; after A's final disconnect and server reset marker, callback clears the lease, B matches and completes signed UDP handshake.
- Dynamic matrix: fresh isolated runs with N=1,2,3,4 create exactly one match row, N participant rows, one replay object, and N account-library ACL rows. N is never supplied as a required match size.

Third re-critique status: pending. Source editing remains blocked until residual accepted/held P0/P1 is zero.

## 9-8. Fourth critique disposition: durable PostgreSQL capacity and ready outbox

Fourth critique: `/root/replay_plan_critique` reported `P0 0 / P1 2 / P2 1 — FAIL`.

- P1 Redis TTL/eviction/restart can lose capacity: accepted. Redis no longer owns game-server capacity. PostgreSQL `game_server_capacities` is the durable authority and remains occupied across match completion, Redis loss, and backend restart until the server-ready compare-and-clear transaction succeeds.
- P1 callback or pre-DB claim crash can strand state: accepted. Server-ready notifications use a persisted sidecar/outbox loaded on server restart. Redis claims include a manifest and matcher reconciliation restores a claim with no DB match or finalizes matched status for an existing DB match.
- P2 replay token scope: accepted. Add a distinct `MATCHMAKING_INTERNAL_TOKEN` / `WINTERS_MATCHMAKING_INTERNAL_TOKEN`; replay upload credentials cannot release game-server capacity.

This section supersedes §9-6's index-only capacity authority and §9-7's Redis allocation lease. The partial unique match index remains defense-in-depth, but it is not the readiness lock.

### Durable capacity migration

`Services/migrations/000011_unique_active_game_session.up.sql` is replaced by:

```sql
CREATE UNIQUE INDEX uq_matches_active_game_session
    ON matches(game_session_id)
    WHERE game_session_id IS NOT NULL
      AND status IN ('allocated', 'running');

CREATE TABLE game_server_capacities (
    game_session_id TEXT PRIMARY KEY,
    active_match_id UUID REFERENCES matches(id) ON DELETE SET NULL,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
```

The down migration drops the table before the index. `createMatch` opens one transaction, inserts the capacity row if absent, locks it `FOR UPDATE`, rejects when `active_match_id IS NOT NULL`, inserts match/participants/outbox, then assigns `active_match_id=matchID` and commits. Replay completion may mark the match `completed` but does not touch capacity.

`ReleaseGameSession(ctx, gameSessionID, matchID)` locks the capacity row and implements three outcomes:

```text
active_match_id == matchID -> set NULL and succeed
active_match_id IS NULL    -> succeed idempotently
active_match_id != matchID -> conflict; stale callback must not unlock a newer match
```

The matcher performs a capacity-row precheck before reading/claiming Redis as an optimization. Concurrent matcher instances remain safe because `createMatch` serializes on the capacity row; a loser restores its whole Redis cohort.

### Recoverable Redis claim manifest

Atomic claim still compares the queue revision and validates every member, but no capacity lease key exists. In the same Lua mutation it writes `matchmaking:claim:<match-id>` as JSON containing game-session ID and every member's user ID, MMR score, and millisecond join time. The manifest TTL is separate from player status TTL and is at least 24 hours.

On each matcher startup and before a new claim, bounded `reconcileClaims` scans claim manifests:

- no corresponding DB match: one Lua restore validates claiming statuses where present, re-adds every manifest member not already matched, restores join-time/status, removes the manifest, and increments queue revision once;
- corresponding DB match exists: regenerate signed status payloads from the persisted match participants and allocation, set every participant to matched, remove the manifest;
- malformed manifest: log and leave it for explicit operator repair rather than silently dropping users.

Normal create failure calls the same whole-group restore. Normal create success finalizes all matched statuses and deletes the manifest atomically. A matcher crash after Redis claim/before DB insert therefore restores on restart; a crash after DB commit/before Redis finalize completes matched delivery on restart. Redis restart may lose queue state, but cannot make the fixed game server available because PostgreSQL capacity remains occupied; affected users can rejoin and remain queued behind that capacity.

### Durable server-ready sidecar/outbox

Add a `GameSessionReadyNotification { gameSessionID, matchID }` work type to `ReplayUploadQueue`. Before `ResetMatchStateLocked()`, `OnSessionLeave` calls `StageGameSessionReady`, which atomically writes `Replay/match_ready_<match-id>.pending.json`. It then resets the room, releases `m_stateMutex`, and calls `PublishGameSessionReady` to place the staged item on the existing worker. A fresh server process treats every valid pending-ready sidecar as publishable because its room starts empty/reset.

The worker POSTs to `WINTERS_MATCHMAKING_SERVICE_URL/internal/game-sessions/<session-id>/ready` using `WINTERS_MATCHMAKING_INTERNAL_TOKEN`, retries with bounded backoff, and retains the sidecar on failure. Failed items remain eligible for periodic retry while the worker is alive; startup also reloads them. Success removes the sidecar. Thus:

- reset then server crash before HTTP: sidecar survives and the fresh/reset server republishes;
- matchmaking outage: sidecar remains and periodic retry eventually delivers;
- crash before reset after sidecar stage: restarting destroys the old in-memory room, so publishing from the fresh server is safe;
- stale duplicated sidecar: DB compare-and-clear is idempotent for null and rejects a different active match.

`StartFromEnvironment` validates replay upload and matchmaking-ready credential groups independently. The ready path can be enabled even if replay upload is disabled, and vice versa.

### Internal credential and configuration

`GameSessionConfig` also contains `InternalTokenSecret`; config reads `MATCHMAKING_INTERNAL_TOKEN`, requires at least 32 bytes, and production rejects the development default. Compose supplies `winters-matchmaking-internal-token-change-in-production`. The Release server receives the same value through `WINTERS_MATCHMAKING_INTERNAL_TOKEN`; it never uses the replay internal token for the capacity endpoint.

### Crash and durability verification additions

- Keep match A's DB row completed and clients connected; wait beyond 600 seconds in a fake-clock/service test and prove capacity remains occupied and B cannot create.
- Delete/restart Redis after A completes but before its last disconnect; prove PostgreSQL capacity still blocks B.
- Crash/recreate matchmaking after atomic Redis claim and before DB insert; startup reconciliation restores the exact cohort atomically.
- Crash/restart matchmaking after DB commit and before matched-status finalization; reconciliation recreates every signed matched status.
- Stop matchmaking, end/disconnect A so the ready sidecar is staged, restart the game server, then restart matchmaking; prove the sidecar clears A's capacity and is deleted only after HTTP success.
- Duplicate/stale ready notification cannot clear B's active capacity.

Fourth re-critique status: pending. Source editing remains blocked until residual accepted/held P0/P1 is zero.

## 9-9. Fifth critique disposition: deletion invariant, DB status recovery, startup reconciliation

Fifth critique: `/root/replay_plan_critique` reported `P0 0 / P1 3 / P2 1 — FAIL`.

- P1 capacity FK `ON DELETE SET NULL`: accepted. It becomes default `NO ACTION`; a match cannot be deleted while it is the active capacity owner. Ready compare-and-clear must run first.
- P1 DB commit plus total Redis loss strands tickets: accepted. `Status` and `Join` gain PostgreSQL active-allocation recovery, independent of Redis manifests.
- P1 stage failure policy: accepted. Durable stage success is required before in-process room reset; failures remain in a retryable pending-reset state. A fresh server synchronously reconciles the durable DB capacity before opening UDP, closing the crash/disk-failure recovery path.
- P2 bounded scan starvation: accepted. Reconciliation uses Redis `SCAN` cursor until it returns zero; each matcher cycle has a time budget but retains the cursor, and startup performs a complete pass before accepting new claims.

### Correct capacity FK

```sql
CREATE TABLE game_server_capacities (
    game_session_id TEXT PRIMARY KEY,
    active_match_id UUID REFERENCES matches(id),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
```

No delete action may implicitly free capacity.

### PostgreSQL-backed match-status recovery

Add `lookupActiveAllocationForUser(ctx, userID)`, querying:

```sql
SELECT m.id, c.game_session_id
FROM game_server_capacities c
JOIN matches m ON m.id = c.active_match_id
JOIN match_participants p ON p.match_id = m.id
WHERE p.user_id = $1
LIMIT 1
```

The capacity reference, not `matches.status`, is the active-allocation predicate: replay completion may already have changed the match row to `completed` while the ended room still owns connected sessions and has not reset.

`Status` uses Redis first; on Redis miss it calls this query, issues a fresh signed ticket for the persisted match/user/session, writes the reconstructed matched JSON back to Redis, and returns it. `Join` performs the same active-participant lookup before queue insertion and returns already-assigned rather than adding a participant to a second queue. Therefore DB commit followed by Redis restart/flush/eviction does not strand active participants or capacity.

### Reset staging and fresh-server reconciliation

`CGameRoom` gains a pending-reset identity state. On the last disconnect of an ended match:

1. attempt atomic ready-sidecar persistence;
2. only after success, reset the room and publish asynchronously outside `m_stateMutex`;
3. on persistence failure, keep the ended empty room and retry staging from `Phase_CheckGameEnd`; do not admit a new match into that room.

If the process crashes before staging succeeds, the new process has an empty room. Before `room->Start()` and before TCP/UDP `Start()`, `ReplayUploadQueue::StartFromEnvironment` synchronously calls the authenticated matchmaking endpoint to read the current capacity owner for `WINTERS_GAME_SESSION_ID`; when present it posts ready for that exact match ID. The compare-and-clear completes before the network port opens. If matchmaking reconciliation is unavailable, authenticated server startup fails closed. A delayed duplicate contains the old exact match ID and cannot clear a newer capacity.

This startup recovery is only enabled when both `WINTERS_MATCHMAKING_SERVICE_URL` and `WINTERS_GAME_SESSION_ID` are configured. The Release authenticated launcher always configures both; local unauthenticated Debug remains unchanged.

### Added failure tests

- Capacity FK rejects deleting its active match until ready clear.
- Commit active match, flush Redis entirely, call participant `Status`, and prove the exact persisted match/session returns with a newly signed ticket; outsider remains `none`; `Join` for that participant is rejected.
- Force sidecar persistence failure: room stays ended/unreset and does not publish ready. Restore writable storage and prove tick retry stages, resets, and publishes.
- Crash before stage success, restart with the same game-session ID, and prove synchronous startup lookup/compare-clear finishes before UDP binds.
- Redis manifest reconciliation scans through more keys than one batch and eventually visits every cursor page.

Fifth re-critique status: PASS (`P0 0 / P1 0 / P2 1`). Implementation gate is open.

The remaining P2 is accepted: the Redis-total-loss regression also sets `matches.status='completed'` while the capacity row remains occupied, then verifies participant status recovery and join rejection still use the capacity reference. The single-server-instance deployment assumption is explicit for this fixed local Release pipeline; the launcher already rejects an existing `WintersServer` process and occupied UDP port.

# 10. Custom Lobby direct admission restoration

Session - Release local capture must admit authenticated accounts directly into one open server lobby without matchmaking queues

## ① 문제 좌표

- 관찰: Release `Game Start`는 `Joining authenticated match queue` 후 bounded assembly를 기다리고 assignment가 생긴 뒤에야 CustomMode를 연다. 사진의 `Server Required`는 assignment 이후 UDP handshake가 실패한 결과다.
- 런타임 실측: 사진 확인 직후 `WintersServer` process는 0개였고 PostgreSQL capacity에는 match `d1e0bce2-5142-44db-bb18-7f0c504b5054`가 남아 있었다. backend가 고정 서버의 실제 연결 가능 여부와 무관하게 match를 할당할 수 있었다.
- 구조 원인: 수동으로 사람이 순차 입장하고 host가 시작하는 Custom Lobby를, 일정 시간 참가자를 모아 일괄 확정하는 일반 matchmaking cohort로 잘못 모델링했다.
- Debug 차이: Debug의 직접/local smoke 경로는 matchmaking assignment를 기다리지 않는다. Release는 계정 Replay identity를 얻기 위해 signed UDP assignment를 추가하면서 불필요한 Redis queue까지 함께 끼워 넣었다. `_DEBUG` 최적화 차이나 dirty worktree가 원인이 아니다.

## ② 소유권

- PostgreSQL: 현재 열린 match, capacity, 인증 참가자, Replay ACL의 영속 진실.
- Release server: 실제 로비, slot, host start, authoritative roster와 WRPL.
- Client: 로그인 계정으로 direct lobby admission을 요청하고 응답의 signed ticket으로 즉시 서버에 접속.
- Redis: 이 촬영용 Custom Lobby의 queue/status/join-time/claim 소유권에서 완전히 제외한다.

## ③ 변경 메커니즘

### `Services/internal/matchmaking/service.go`

기존 `Join(ctx, userID) error -> Redis ZADD -> RunMatcher -> 5초 cohort -> createMatch` 경로를 아래 direct admission으로 교체한다.

```go
func (s *Service) Join(ctx context.Context, userID uuid.UUID) (MatchStatus, error) {
    matchID, err := s.admitLobbyParticipant(ctx, userID)
    if err != nil {
        return MatchStatus{}, err
    }
    return s.issueMatchStatus(matchID, userID, s.allocation.GameSessionID)
}
```

`admitLobbyParticipant`는 한 PostgreSQL transaction에서 capacity row를 `FOR UPDATE`한다.

```text
active_match_id IS NULL
  -> status=allocated match 생성
  -> 요청 계정을 가장 낮은 빈 slot에 INSERT
  -> capacity가 새 match를 가리킴

active_match_id exists AND match.status=allocated
  -> 기존 participant면 ticket 재발급
  -> 아니면 가장 낮은 빈 slot에 INSERT

match.status != allocated OR participant 10명
  -> 새 입장 거부; 기존 game/replay 상태는 변경하지 않음
```

Redis Lua queue/leave/claim/restore/finalize, assembly window, matcher ticker, Redis status recovery를 삭제한다. Outbox publisher는 queue matcher와 분리한 `RunOutboxPublisher`로 유지한다. `Status`는 PostgreSQL active capacity와 participant만 조회해 ticket을 재발급한다. `Leave`는 `allocated` lobby에서만 참가자를 제거하고 마지막 참가자가 떠나면 match를 `aborted`, capacity를 `NULL`로 만든다.

### `Services/internal/matchmaking/handler.go`, `cmd/matchmaking/main.go`

- `POST /join`은 `queued`가 아니라 완성된 `MatchStatus`를 즉시 반환한다.
- service startup에서 Redis client와 matcher goroutine을 제거하고 outbox publisher만 실행한다.
- internal active/ready compare-and-clear endpoint는 Replay 종료 후 동일 서버 재사용에 필요하므로 유지한다.

### `Services/pkg/config/config.go`, `Services/docker-compose.yml`

- `WINTERS_MATCH_ASSEMBLY_WINDOW`와 `MatchAssemblyWindow`를 삭제한다.
- `WINTERS_MATCH_MAX_SIZE`는 queue batch 크기가 아니라 열린 Custom Lobby의 최대 human admission 수 1..10으로만 유지한다.

### `Services/internal/replay/repository.go`, `Services/cmd/replay/main.go`

- match completion 후 Redis `matchmaking:status:*`를 지우는 Lua와 Redis repository dependency를 삭제한다.
- Replay ready와 계정 library는 계속 PostgreSQL `match_participants`에서 생성한다.

### `Client/Private/Scene/Scene_MainMenu.cpp`

기존 문구와 polling 전제를 direct lobby 의미로 교체한다.

```cpp
m_strStatus = "Joining authenticated custom lobby...";
backend.RequestJoinQueue();
```

Join 응답 자체가 complete assignment이므로 다음 update에서 바로 CustomMode로 이동한다. 0.5초 status polling과 `Searching for match` 표시는 정상 경로에서 발생하지 않는다. 기존 method 이름은 ABI/범위 최소화를 위해 유지하되 사용자 노출 문구와 backend 동작에는 queue가 없다.

### `Tools/Harness/StartReleaseAccountReplayCapture.ps1`

- assembly-window 환경 설정·검사를 제거한다.
- Release server window를 `Hidden`이 아닌 `Normal`로 띄워 사용자가 실행 여부를 직접 확인할 수 있게 한다. 로그 파일과 PID readiness 검사는 유지한다.
- `HumanPlayers`는 안내할 client 수일 뿐 admission 조건이 아니다.

## ④ 실패·경계 조건

- 서버가 실행되지 않으면 signed ticket이 있어도 UDP handshake는 실패한다. capture launcher가 server process와 UDP readiness를 먼저 증명한 뒤 client 실행을 안내한다.
- 이미 Loading/InGame/Completed인 match에는 새 participant를 붙이지 않는다.
- 동시 Join은 capacity row lock과 `(match_id, user_id)`, `(match_id, slot)` unique constraint로 직렬화한다.
- Leave와 Join이 교차해도 마지막 participant cleanup과 새 match allocation이 같은 capacity row lock을 사용한다.
- Replay complete request의 participant set은 PostgreSQL participant set과 계속 정확히 일치해야 한다. 인증 없이 파일만 계정에 복제하는 우회는 하지 않는다.

## ⑤ 검증·종료 기준

1. Redis `matchmaking:queue`, `matchmaking:status:*`, `matchmaking:jointime:*`, `matchmaking:claim:*`을 만들지 않고 1·2·3·4명이 동일 allocated match에 즉시 admission된다.
2. 두 client Join 응답이 즉시 같은 `match_id/game_session_id`, 서로 다른 signed ticket을 반환한다.
3. allocated lobby에서 leave/rejoin, 마지막 leave capacity cleanup, 11번째 입장 거부를 통합 테스트한다.
4. Release visible server + client 2개가 동일 CustomMode roster에 접속하고 `Server Required`가 나오지 않는다.
5. 게임 종료 후 server authenticated participant 2명과 DB participant 2명이 일치하고 Replay ready/library 2계정이 생성된다.
6. `go test ./...`, Docker rebuild/config, Release Server/Client x64 build, 관련 `git diff --check`를 통과한다.

## 계획 비평 gate

이 §10 delta는 기존 Replay 독립 비평자에게 read-only 재비평을 요청한다. 수용 또는 보류 P0/P1이 0이 되기 전에는 source edit를 시작하지 않는다.

## §10 독립 비평 1차 disposition

독립 비평 결과: `P0 0 / P1 3 / P2 2 — FAIL`.

- P1 StartGame 이후에도 `allocated`: 수용. server가 실제 seated authenticated roster로 internal start-lock transaction을 성공시킨 뒤에만 Loading을 적용한다.
- P1 ticket 예약자와 실제 접속자 불일치: 수용. start-lock이 no-show reservation을 제거하고 실제 seated roster만 최종 participant로 확정한다. running 이후 user leave는 participant를 삭제하지 않는다.
- P1 이전 match ticket의 reset room 선점: 수용. signed UDP ticket 검증은 서명·만료·game-session뿐 아니라 matchmaking internal active-capacity 조회의 exact `match_id` 일치까지 fail-closed로 검사한다.
- P2 초기 `MatchCreated` roster 부정확: 수용. 첫 admission 때 outbox를 만들지 않고 start-lock에서 최종 roster와 MMR로 한 번 생성한다.
- P2 1..10 전 범위: 수용. PostgreSQL direct admission은 N=1..10 전체와 11번째 거부를 parameterize한다. live GUI는 사용자의 실제 촬영 구성인 2-client를 필수선으로 하고 1/3/10은 backend/UDP 계약으로 검증한다.

### Start-lock 상세 계약

새 internal endpoint:

```text
POST /internal/game-sessions/{game_session_id}/start
Authorization: Bearer <MATCHMAKING_INTERNAL_TOKEN>
{
  "match_id": "<uuid>",
  "players": [
    {"user_id":"<uuid>", "slot":0, "team":0}
  ]
}
```

`Service.StartGameSession`은 capacity row와 active match를 한 transaction에서 잠그고 다음을 실행한다.

```text
active_match_id == request.match_id
match.status == allocated
players 1..10, user/slot unique
모든 request user가 기존 ticket-authorized participant
  -> request에 없는 no-show reservation DELETE
  -> 실제 player의 slot/team/joined_at 확정
  -> matches.status=running, started_at=NOW()
  -> 최종 roster/MMR 기반 MatchCreated outbox INSERT
```

같은 exact roster로 재요청한 `running` start는 멱등 성공하고, 다른 roster/match는 conflict다.

`CGameRoom::ApplyLobbyAuthorityResult`가 `bBeginLoading`을 받으면 spawn 전에 `CReplayUploadQueue::LockGameSessionRoster`를 동기 호출한다. 실패하면 `CLobbyAuthority::RollbackStartGame`으로 ChampionSelect와 slot lock/ready 상태를 복원하고 loading/spawn을 수행하지 않는다. 성공 뒤에만 기존 spawn/loading 경로를 계속한다.

### Active-capacity ticket 검증

`CReplayUploadQueue::IsActiveGameSessionMatch(matchID)`는 기존 authenticated internal active endpoint를 동기 조회한다. Release signed UDP validator는 ticket 서명·만료·`game_session_id` 검증 뒤 이 exact active match 검증까지 성공해야 identity를 승인한다. 따라서:

```text
A 완료/ready-clear -> B allocate -> A의 아직 만료 전 ticket handshake
  active capacity == B
  A ticket match != B
  reject
  B ticket은 accept
```

authenticated UDP server는 matchmaking URL/token이 없으면 startup을 fail-closed한다. Debug `--udp-dev-allow-empty-ticket` smoke는 이 검증을 사용하지 않는다.

### Replay participant 확정

Replay `CompleteMatch`의 exact cardinality 검증은 유지한다. 차이를 느슨하게 허용하는 대신 start-lock이 DB participant를 실제 seated authenticated roster로 먼저 정규화한다. 검증에 `DB reservation 3명 중 server seated 2명 -> start-lock 후 DB 2명 -> actual 2-player completion/Replay ACL PASS`를 추가한다.

§10 2차 독립 재비평 상태: pending. Source edit gate는 계속 닫혀 있다.

## §10 독립 비평 2차 disposition: reservation/final roster 분리와 durable async start

2차 결과: `P0 0 / P1 3 / P2 0 — FAIL`.

- P1 commit 후 응답 유실: 지적은 수용하되 동기 ACK/rollback 해법은 채택하지 않는다. 서버 권위 tick을 HTTP에 묶지 않기 위해 local roster를 먼저 동결하고 start sidecar를 atomic stage한 뒤 기존 backend worker가 idempotent start를 비동기 재시도한다. sidecar stage 실패 때만 Loading을 거부한다. 응답 유실은 같은 start request 재전송으로 수렴한다.
- P1 same-match no-show ticket: 수용. ticket reservation과 최종 `match_participants`를 분리하고, 서버도 finalized roster만 Loading/InGame 재접속과 Replay 대상으로 사용한다.
- P1 handshake HTTP/TOCTOU: 수용. handshake에서는 HTTP를 호출하지 않는다. capacity generation을 signed ticket과 server local room authority에 넣어 O(1) 검증한다.
- slot swap unique 충돌: 수용. provisional admission은 별도 table로 이동하므로 final participant table은 start 때 비어 있으며, final slot/team을 한 번에 INSERT한다.

### Migration `000012_custom_lobby_admission`

```sql
ALTER TABLE game_server_capacities
    ADD COLUMN generation BIGINT NOT NULL DEFAULT 0 CHECK (generation >= 0);

ALTER TABLE matches
    ADD COLUMN game_session_generation BIGINT NOT NULL DEFAULT 0 CHECK (game_session_generation >= 0);

CREATE TABLE match_lobby_admissions (
    match_id UUID NOT NULL REFERENCES matches(id) ON DELETE CASCADE,
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    admitted_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    PRIMARY KEY (match_id, user_id)
);
```

첫 direct admission이 새 match를 만들 때 capacity generation을 1 증가시키고 같은 값을 match에 저장한다. 이후 admission은 `match_lobby_admissions`에만 기록한다. `match_participants`는 실제 경기 참가자로 확정되기 전까지 비어 있다.

### Ticket generation과 room local gate

Go match ticket v1 payload에 positive generation `n`을 추가한다. C++ `UdpAuthenticatedIdentity`/`ServerSessionIdentity`도 이를 전달한다.

```json
{"m":"match uuid","u":"user uuid","g":"session","n":12,"e":expiry}
```

`CGameRoom`은 `m_gameSessionGeneration`과 `m_lastCompletedGameSessionGeneration`을 소유한다.

- 빈 SeatSelect/ChampionSelect room의 첫 ticket은 `generation > lastCompleted`만 허용하고 current match/generation을 고정한다.
- 같은 room의 추가 ticket은 exact match/session/generation만 허용한다.
- Loading/InGame에서는 exact match/session/generation뿐 아니라 immutable finalized user set membership도 요구한다.
- reset은 current generation을 completed floor로 올린 뒤 current/finalized roster를 비운다.
- fresh server startup은 internal active endpoint가 capacity의 현재 generation을 항상 반환하게 하고, stale active match를 clear한 뒤 그 generation을 room completed floor로 주입한다. 따라서 재시작 직후에도 과거 ticket은 거부된다.

### Durable async start sidecar

호스트 StartGame 검증이 `bBeginLoading`을 만들면 실제 seated human slot에서 `(user_id, slot, team)` roster를 구성한다. 이때 모든 seat가 authenticated session에 매핑되어야 한다.

```text
1. server local finalized roster 동결
2. Replay/match_start_<match>.start.pending.json atomic stage
3. stage 성공 뒤에만 기존 spawn/loading 진행
4. backend worker는 start notification을 replay upload보다 우선 처리하고 성공할 때까지 재시도
5. success 뒤 sidecar 삭제
```

backend `StartGameSession`은 idempotent하다.

- `allocated`: admission superset에 request roster가 포함되는지 검증하고, final `match_participants`를 INSERT하며 `running`/started_at과 final MatchCreated outbox를 commit한다.
- `running`: 저장된 exact user/slot/team roster가 request와 같으면 성공, 다르면 conflict.
- no-show admission은 final participant로 복사하지 않는다. 필요하면 start commit 뒤 admission table을 삭제한다.

HTTP commit 후 response가 유실돼도 sidecar가 같은 roster를 다시 보내고 `running + exact roster`가 성공하므로 분산 rollback이 없다. HTTP 호출은 worker thread에서만 수행되어 IOCP/room tick을 막지 않는다. Replay upload queue는 pending start를 먼저 비워야 같은 match artifact를 처리한다.

### Fresh restart와 미완료 start

startup reconciliation은 pending start sidecar를 먼저 적재한다. active match가 존재하면서 그 match의 pending start가 있으면 무조건 ready-clear하지 않고 start idempotent publish를 먼저 시도한다. completed replay/ready sidecar가 있으면 기존 ready 경로를 우선한다. backend가 unavailable하면 authenticated Release server는 기존과 같이 bind 전에 fail-closed한다.

### 추가 회귀

- 3 admission / 2 seated start -> DB final participant 2 -> 2-player completion/ACL PASS.
- 제거된 no-show의 같은-match ticket은 Loading/InGame local finalized roster gate에서 거부.
- A generation N ready/reset -> B generation N+1 allocate -> A ticket 먼저 도착해도 reject, B accept.
- server restart가 capacity generation N floor를 복구한 뒤 A stale ticket reject.
- start DB commit 후 HTTP response drop -> same sidecar retry -> exact running roster idempotent success.
- A:slot0/B:slot1 admission 순서와 무관하게 final A:slot1/B:slot0 INSERT PASS.

§10 3차 독립 재비평 상태: pending. Source edit gate는 계속 닫혀 있다.

## §10 독립 비평 3차 disposition: PendingStart와 non-resumable restart

3차 결과: `P0 0 / P1 3 / P2 1 — FAIL`.

- P1 ACK 전 Loading: 수용. `eRoomPhase::StartPending`을 추가하고 backend exact ACK를 worker/tick 경계에서 소비한 뒤에만 Loading/spawn한다.
- P1 allocated lobby abort 뒤 stale generation: 수용하되 불필요한 상태 전이를 제거한다. queue가 없는 direct Custom Lobby에서는 assigned admission의 cancel/마지막-leave abort를 지원하지 않는다. 첫 admission이 만든 allocated lobby는 host start 또는 server process restart까지 같은 match/generation으로 유지한다. disconnect ticket은 같은 열린 lobby 재접속에 유효하다.
- P1 fresh restart 의미: 수용. Winters server는 world/session resume를 지원하지 않으므로 startup은 completed가 아닌 기존 active match를 exact match/generation으로 `aborted + capacity clear`한 뒤 generation floor를 복구한다. pending start sidecar는 삭제한다. completed match는 기존 ready compare-clear로 처리한다.
- P2 Status: 수용. allocated는 admission table, running/completed active는 final participant table에서 조회한다.

### PendingStart two-phase runtime

`eRoomPhase::StartPending`을 `ChampionSelect`와 `Loading` 사이에 추가한다.

```text
host StartGame validation PASS
  -> lobby phase StartPending
  -> seated authenticated roster + lobby revision 동결
  -> start sidecar atomic stage
  -> stage 실패: ChampionSelect rollback
  -> stage 성공: worker publish/retry, server tick은 계속 동작

backend exact ACK
  -> tick에서 ACK consume
  -> finalized user set 고정
  -> phase Loading
  -> spawn/loading/broadcast 수행
```

StartPending 동안 join/leave/slot/champion/bot mutation과 재차 StartGame을 거부한다. network disconnect는 session transport만 정리하되 frozen roster/admission 확정을 바꾸지 않는다. backend 응답이 유실되면 sidecar가 같은 request를 재전송하고 `running + exact roster` 멱등 ACK로 수렴한다. room mutex나 IOCP callback에서 HTTP를 호출하지 않는다.

### Direct lobby lifecycle 단순화

- `POST /join`: 즉시 현재 allocated lobby admission + signed ticket. active lobby가 없으면 새 match/generation을 생성한다.
- `DELETE /leave`: queue cancellation 의미를 제거한다. admission ticket 발급 후에는 idempotent no-op 또는 conflict로 응답하며 capacity/participant를 삭제하지 않는다.
- disconnected client는 같은 ticket 또는 `Status` 재발급 ticket으로 같은 allocated lobby에 재접속할 수 있다.
- 새로운 match generation은 정상 completed ready/reset 또는 fresh-server abort/clear 뒤에만 생성된다.

이 제약으로 `B allocate -> all leave/abort -> C allocate` 중간 상태 자체가 사라진다. 이전 완료 match A의 ticket은 room `lastCompletedGeneration` 이하라 거부되고, 현재 열린 B ticket은 의도대로 재접속 가능하다.

### Fresh server abort contract

internal active 응답은 항상 아래를 포함한다.

```json
{"occupied":true,"match_id":"...","generation":12,"status":"allocated|running|completed"}
```

새 internal abort endpoint는 exact `(game_session_id, match_id, generation)` compare-and-clear transaction이다.

```text
allocated/running -> matches.status=aborted, capacity=NULL, admissions 삭제
completed         -> abort 거부; ready endpoint 사용
different owner   -> conflict
```

fresh authenticated server startup:

```text
active none      -> returned generation을 room floor로 설정
active completed -> ready compare-clear, generation을 room floor로 설정
active allocated/running -> abort compare-clear, pending start sidecar 삭제,
                            generation을 room floor로 설정
backend unavailable/conflict -> UDP bind 전 fail-closed
```

### Status와 start result

- `Status(allocated)`: `match_lobby_admissions` membership으로 generation을 조회해 ticket 재발급.
- `Status(running/completed active)`: `match_participants` membership으로 재발급.
- start worker는 `Pending | Confirmed`만 노출한다. HTTP/network failure는 Pending으로 재시도한다. Backend explicit conflict는 server log에 fatal start conflict를 남기고 Loading하지 않는다.

### 갱신된 검증

- StartPending 동안 DB ACK 전 phase/spawn/tick=0 유지, ACK 뒤 한 번만 Loading/spawn.
- start commit 후 response drop, retry exact ACK, 한 번만 Loading.
- StartPending 중 roster command/새 session admission 거부.
- allocated client disconnect/reconnect same ticket PASS; direct leave가 capacity를 지우지 않음.
- fresh restart allocated/running abort+clear+generation floor; completed ready-clear.
- A completed generation N 뒤 stale A reject, B generation N+1 accept.
- Status allocated admission/running final participant 각각 PASS.

§10 4차 독립 재비평 상태: pending. Source edit gate는 계속 닫혀 있다.

## §10 4차 독립 재비평 종결

결과: `P0 0 / P1 0 / P2 2 — PASS`. Source edit gate를 연다.

- P2 start ACK 순서: 수용. worker는 DB ACK 뒤 thread-safe `Confirmed(match,generation,rosterHash)`를 먼저 게시하고 sidecar를 삭제한다. tick consume은 현재 pending match/generation/rosterHash와 exact 일치할 때만 Loading을 승인한다. 오래된 ACK는 폐기한다.
- P2 wire/UI/leave: 수용. `StartPending`은 server 내부 phase로만 추가하고 wire `LobbyPhase`는 `ChampionSelect`로 유지한다. 기존 schema 재생성은 하지 않는다. 사용자 노출 상태는 server lobby message로 `finalizing authenticated roster`를 표시한다. `/leave`는 assigned admission에 `409 conflict`를 반환하고, MainMenu의 queue cancel UI/polling을 제거한다.

# 2026-07-20 최종 복구 결정 — Custom Lobby와 Replay 권한 분리

이 절은 앞선 고정 인원·5초 cohort/assembly-window 및 StartPending 설계를 폐기하고 최종 구현 방향을 대체한다.

## 확정 원인

- 요구사항은 “실제 참가 계정마다 같은 경기 Replay를 제공”하는 것이었다.
- 이를 “먼저 N명을 모아야 경기 하나를 생성”하는 matchmaking 문제로 잘못 모델링했다.
- 그 결과 Debug의 기존 직접 진입 흐름 앞에 Redis queue, 5초 assembly window, cancel/poll UI가 들어가 Release에서만 지연과 cohort 분리가 발생했다.
- Replay 파일 수와 Replay 접근 권한 수를 혼동한 것이 핵심 오류다. WRPL은 경기당 1개이고, 접근 권한은 실제 참가 계정 수만큼 생성하면 된다.

## 최종 계약

1. 로그인 계정이 Game Start를 누르면 Backend는 첫 사용자부터 즉시 현재 open Custom Lobby의 signed UDP ticket을 반환한다.
2. 이후 2~10번째 계정도 host가 경기를 시작하기 전이면 동일 match ID의 ticket을 즉시 받는다. 시간창과 최소 인원 조건은 없다.
3. Client MainMenu는 join 요청 한 번만 보내며 queue poll/cancel UI를 사용하지 않는다.
4. Server는 기존 host Start 흐름대로 즉시 Loading/InGame으로 전환한다. Replay 때문에 별도 StartPending 대기를 추가하지 않는다.
5. 경기 종료 artifact에 포함된 실제 인증 참가자만 Backend의 `match_participants`로 확정한다. 입장권만 받고 플레이하지 않은 계정은 제외한다.
6. Replay `MarkReady`는 확정된 `match_participants`로 `replay_user_library` ACL을 만든다. Redis matchmaking 상태는 이 과정에 관여하지 않는다.
7. 완료 artifact 처리 순서는 `CompleteMatch(실제 roster 확정) -> replay reserve/upload -> MarkReady(ACL 생성) -> capacity ready`다.
8. Release 촬영 서버는 사용자가 직접 보고 종료할 수 있는 `cmd.exe /k`의 자식 프로세스로 실행한다. 숨겨진 Codex 소유 서버를 남기지 않는다.

## 합격 기준

- Redis `matchmaking:*` key 0개인 상태에서 1·2·3·4·10명 즉시 동일 lobby admission PASS.
- assembly window 환경변수와 matcher worker가 code/compose/launcher에 없음.
- 실제 플레이 계정 N명과 admission-only outsider 1명을 구성했을 때 Replay library N개, outsider 0개.
- Release Server/Client x64 build PASS.
- 보이는 Release Server CMD 1개, Release Client 2개, UDP 9000 확인.

## 2026-07-20 최종 독립 비평 disposition

독립 비평 결과는 `P0 0 / P1 3 / P2 2 / FAIL`이었다. Custom Lobby 즉시 진입과 실제 참가자 기반 Replay ACL 분리 방향은 유지하되, 아래 P1 세 건을 모두 수용하고 수정·재비평 전에는 완료 판정을 내리지 않는다.

1. **Replay 업로드와 capacity ready 순서 — 수용**
   - `CompleteMatch -> upload -> MarkReady`가 실패한 artifact는 durable sidecar를 유지한 채 worker 앞쪽에 재등록한다.
   - 같은 match의 artifact가 남아 있는 동안 ready notification은 실행하지 않는다.
   - fresh-server 시작 시 pending artifact를 먼저 적재하고, active completed match에 같은 artifact가 있으면 업로드가 성공할 때까지 capacity를 clear하지 않는다.
2. **완료 generation floor — 수용**
   - room reset 때 완료된 generation을 `lastCompletedGeneration`으로 보존한다.
   - 비어 있는 room의 첫 ticket도 `generation > lastCompletedGeneration`일 때만 수용한다.
   - fresh-server reconciliation이 backend capacity의 최신 generation을 읽어 room floor에 주입한다.
3. **보이는 CMD 실행 계약 — 수용**
   - Release capture launcher도 `StartVisibleReleaseServer.cmd`를 통해 실제 `cmd.exe /k` 자식으로 서버를 실행한다.
   - readiness는 숨은 redirected process의 로그 파일이 아니라 visible CMD의 자식 `WintersServer.exe`와 UDP 9000으로 확인한다.

P2의 사용되지 않는 start endpoint와 Client queue API 이름 정리는 이번 사용자 요구의 즉시 복구 경로를 바꾸지 않는 후속 정리로 기록한다. 현재 정상 경로에서 호출되지 않는지 정적 검색과 회귀 테스트로 확인하며, P1 종결보다 앞서 범위를 확장하지 않는다.

# 2026-07-20 Visual Studio Release 서버 실행 계약 종결

Session - Docker Backend가 켜진 상태에서 Visual Studio의 Server Release 시작만으로 인증 UDP 게임 서버가 반복 기동되게 한다.
좌표: 없음 · 축: C7 권위와 정합성, C8 검증이 병목
관련: 2026-07-19_RELEASE_AUTHENTICATED_ACCOUNT_REPLAY_E2E_PLAN.md / RESULT.md

## 1. 결정 기록

① 문제·제약: MinIO가 TCP 9000을 점유하는 환경에서 인자 없는 Release Server는 기본 TCP로 시작해 즉시 종료되고, Client 2개 모두 `Server Required`가 됐다.
② 순진한 해법의 실패: 포트만 바꾸거나 Release에 인증 기본값을 하드코딩하면 Backend가 발급한 UDP endpoint/session과 다시 어긋나고 제품 실행 계약까지 오염한다.
③ 메커니즘: local Release의 고정 game-session ID를 `winters-local-game-session` 하나로 통일하고, 추적되는 VS Release 디버거 프로필이 UDP/Backend 환경을 주입하며, authenticated UDP 단일 인스턴스 mutex를 Backend reconciliation보다 먼저 획득한다.
④ 대조: 일회성 UUID는 격리에는 유리하지만 capture script가 Docker에 남긴 UUID와 다음 VS 실행이 불일치했다. 경기 식별은 고유 match ID가 맡으므로 고정 로컬 서버 ID가 맞다.
⑤ 대가: 이 프로필은 로컬 개발용이며 실제 배포 launcher/orchestrator를 대체하지 않는다. Docker Backend가 꺼져 있으면 Release 서버는 의도대로 fail-closed한다.

## 2. 반영해야 하는 코드

### 2-1. C:/Users/user/Desktop/Winters/Server/Private/main.cpp

기존 코드:

```cpp
#include <WinSock2.h>
#include <bcrypt.h>
```

아래로 교체:

```cpp
#include <WinSock2.h>
#include <Windows.h>
#include <bcrypt.h>
```

기존 코드:

```cpp
    bool_t UsesUdp(eServerNetworkMode mode)
    {
        return mode == eServerNetworkMode::Udp ||
            mode == eServerNetworkMode::Dual;
    }
```

아래에 추가:

```cpp
    struct Win32HandleCloser
    {
        void operator()(void* value) const noexcept
        {
            if (value)
                ::CloseHandle(static_cast<HANDLE>(value));
        }
    };

    using ScopedWin32Handle = std::unique_ptr<void, Win32HandleCloser>;

    bool_t AcquireAuthenticatedUdpInstanceGuard(
        const ServerRuntimeOptions& runtimeOptions,
        ScopedWin32Handle& outGuard)
    {
        outGuard.reset();
        if (!UsesUdp(runtimeOptions.networkMode) ||
            runtimeOptions.bUdpDevAllowEmptyTicket)
        {
            return true;
        }

        constexpr const wchar_t* kMutexName =
            L"Local\\WintersServer.AuthenticatedUdp9000";
        HANDLE const rawHandle = ::CreateMutexW(nullptr, FALSE, kMutexName);
        const DWORD createError = ::GetLastError();
        if (!rawHandle)
        {
            std::cerr << "[ERROR] Failed to create authenticated UDP instance guard error="
                << createError << '\n';
            return false;
        }

        ScopedWin32Handle guard(rawHandle);
        if (createError == ERROR_ALREADY_EXISTS)
        {
            std::cerr << "[ERROR] Another authenticated UDP WintersServer is already running; "
                "startup stopped before backend reconciliation\n";
            return false;
        }

        outGuard = std::move(guard);
        return true;
    }
```

기존 코드:

```cpp
    ServerRuntimeOptions runtimeOptions{};
    if (!ParseRuntimeOptions(argc, argv, runtimeOptions))
        return 5;

    const u32_t smokeSeconds = ParseSmokeSeconds(argc, argv);
```

아래로 교체:

```cpp
    ServerRuntimeOptions runtimeOptions{};
    if (!ParseRuntimeOptions(argc, argv, runtimeOptions))
        return 5;

    ScopedWin32Handle authenticatedUdpInstanceGuard;
    if (!AcquireAuthenticatedUdpInstanceGuard(
        runtimeOptions,
        authenticatedUdpInstanceGuard))
    {
        return 9;
    }

    const u32_t smokeSeconds = ParseSmokeSeconds(argc, argv);
```

### 2-2. C:/Users/user/Desktop/Winters/Server/Include/Server.vcxproj

기존 코드:

```xml
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'">
    <OutDir>$(ProjectDir)..\Bin\Release\</OutDir>
    <IntDir>$(ProjectDir)..\Bin\Intermediate\Release\</IntDir>
    <TargetName>WintersServer</TargetName>
    <LocalDebuggerWorkingDirectory>$(MSBuildThisFileDirectory)..\..\</LocalDebuggerWorkingDirectory>
  </PropertyGroup>
```

아래로 교체:

```xml
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'">
    <OutDir>$(ProjectDir)..\Bin\Release\</OutDir>
    <IntDir>$(ProjectDir)..\Bin\Intermediate\Release\</IntDir>
    <TargetName>WintersServer</TargetName>
    <LocalDebuggerWorkingDirectory>$(MSBuildThisFileDirectory)..\..\</LocalDebuggerWorkingDirectory>
    <LocalDebuggerCommandArguments>--net-transport=udp</LocalDebuggerCommandArguments>
    <LocalDebuggerEnvironment>WINTERS_GAME_SESSION_ID=winters-local-game-session&#xA;WINTERS_MATCH_TICKET_SECRET=winters-match-ticket-secret-change-in-production&#xA;WINTERS_MATCHMAKING_SERVICE_URL=http://127.0.0.1:8083&#xA;WINTERS_MATCHMAKING_INTERNAL_TOKEN=winters-matchmaking-internal-token-change-in-production&#xA;WINTERS_REPLAY_SERVICE_URL=http://127.0.0.1:8087&#xA;WINTERS_REPLAY_INTERNAL_TOKEN=winters-replay-internal-token-change-in-production</LocalDebuggerEnvironment>
    <LocalDebuggerMergeEnvironment>true</LocalDebuggerMergeEnvironment>
    <DebuggerFlavor>WindowsLocalDebugger</DebuggerFlavor>
  </PropertyGroup>
```

### 2-3. C:/Users/user/Desktop/Winters/Winters.slnLaunch.user

기존 코드:

```json
[
  {
    "Name": "새 프로필",
    "Projects": [
      {
        "Path": "Server\\Include\\Server.vcxproj",
        "Action": "Start"
      },
      {
        "Path": "Client\\Include\\Client.vcxproj",
        "Action": "Start"
      }
    ]
  }
]
```

아래로 교체:

```json
[
  {
    "Name": "Release Server",
    "Projects": [
      {
        "Path": "Server\\Include\\Server.vcxproj",
        "Action": "Start"
      }
    ]
  },
  {
    "Name": "Release Client",
    "Projects": [
      {
        "Path": "Client\\Include\\Client.vcxproj",
        "Action": "Start"
      }
    ]
  }
]
```

### 2-4. C:/Users/user/Desktop/Winters/Tools/Harness/StartVisibleReleaseServer.cmd

기존 코드:

```bat
set "WINTERS_SERVER_LIFETIME_SECONDS=%~2"
if "%WINTERS_SERVER_LIFETIME_SECONDS%"=="" set "WINTERS_SERVER_LIFETIME_SECONDS=21600"

echo [ReleaseCapture] session=%WINTERS_GAME_SESSION_ID%
echo [ReleaseCapture] close this CMD window or press Ctrl+C to stop WintersServer.
Server\Bin\Release\WintersServer.exe --net-transport=udp --smoke-seconds=%WINTERS_SERVER_LIFETIME_SECONDS%
```

아래로 교체:

```bat
echo [ReleaseCapture] session=%WINTERS_GAME_SESSION_ID%
echo [ReleaseCapture] type q then press Enter, or close this CMD window, to stop WintersServer.
Server\Bin\Release\WintersServer.exe --net-transport=udp
```

### 2-5. C:/Users/user/Desktop/Winters/Tools/Harness/StartReleaseAccountReplayCapture.ps1

기존 코드:

```powershell
param(
    [ValidateRange(1, 10)]
    [int]$HumanPlayers = 3,
    [ValidateRange(60, 86400)]
    [int]$ServerLifetimeSeconds = 21600,
    [switch]$SkipBackendBuild,
    [switch]$SkipDefinitionCheck
)
```

아래로 교체:

```powershell
param(
    [ValidateRange(1, 10)]
    [int]$HumanPlayers = 3,
    [switch]$SkipBackendBuild,
    [switch]$SkipDefinitionCheck
)
```

기존 코드:

```powershell
$sessionId = [Guid]::NewGuid().ToString()
$ticketSecret = "winters-match-ticket-secret-change-in-production"
$replayToken = "winters-replay-internal-token-change-in-production"
$logRoot = Join-Path $workspaceRoot ".md\build\release-replay-capture\$sessionId"
```

아래로 교체:

```powershell
$sessionId = "winters-local-game-session"
$captureRunId = [Guid]::NewGuid().ToString()
$ticketSecret = "winters-match-ticket-secret-change-in-production"
$replayToken = "winters-replay-internal-token-change-in-production"
$logRoot = Join-Path $workspaceRoot ".md\build\release-replay-capture\$captureRunId"
```

기존 코드:

```powershell
    $serverCommand = '"{0}" "{1}" "{2}"' -f `
        $visibleServerLauncher, $sessionId, $ServerLifetimeSeconds
```

아래로 교체:

```powershell
    $serverCommand = '"{0}" "{1}"' -f `
        $visibleServerLauncher, $sessionId
```

기존 코드:

```powershell
    [ordered]@{
        session_id = $sessionId
        requested_client_count = $HumanPlayers
        server_lifetime_seconds = $ServerLifetimeSeconds
        server_console_pid = $serverConsole.Id
```

아래로 교체:

```powershell
    [ordered]@{
        capture_run_id = $captureRunId
        session_id = $sessionId
        requested_client_count = $HumanPlayers
        server_console_pid = $serverConsole.Id
```

### 2-6. C:/Users/user/Desktop/Winters/.claude/gotchas.md

기존 코드:

```text
Format: `YYYY-MM-DD - [Area] mistake -> prevention rule/check`.
```

아래에 추가:

```text
- 2026-07-20 - [Release local launch] Release build 성공과 authenticated game-server 실행 성공은 별개다. VS가 Server를 인자 없이 실행하면 기본 TCP 9000이 MinIO와 충돌하고 Backend ticket의 UDP/session 계약도 불일치하며, 중복 서버가 bind 실패 전에 기존 capacity를 정리할 수도 있다 -> local Release는 추적되는 VS 프로필과 capture launcher 모두 `winters-local-game-session` + UDP + 동일 개발용 인증 환경을 사용하고, Backend reconciliation 전 authenticated UDP 단일 인스턴스 guard를 획득하며, 검증 시 TCP 9000=MinIO와 UDP 9000=WintersServer가 동시에 존재하는지 확인한다.
```

## 3. 검증

예측:
- Docker matchmaking의 `WINTERS_GAME_SESSION_ID`, VS Release 프로필, capture launcher가 모두 `winters-local-game-session`으로 일치한다.
- Server Release build 후 VS 프로필과 동일한 인자·환경으로 시작하면 MinIO TCP 9000을 유지한 채 WintersServer가 UDP 9000에 bind하고 조기 종료하지 않는다.
- 정상 서버가 떠 있는 동안 두 번째 서버를 시작하면 exit 9로 Backend reconciliation 전에 종료되고, 기존 active match/generation은 변하지 않는다.
- Debug 실행 설정과 Client/Replay 데이터·게임플레이 코드는 변하지 않는다.

검증 명령:
- `msbuild Server/Include/Server.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /m:1`
- `msbuild Client/Include/Client.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /m:1`
- `docker compose --profile app up -d --force-recreate` 후 matchmaking container의 session 환경과 HTTP health 4종 확인.
- VS 프로필 동등 환경으로 `WintersServer.exe`를 실행해 UDP 9000 PID, TCP 9000 MinIO PID, 서버 생존을 확인한 뒤 정상 종료.
- 첫 서버가 보유한 active match/generation을 기록한 뒤 두 번째 같은 서버를 실행하고 exit 9 및 DB 값 불변을 확인.
- PowerShell XML parse로 Release `LocalDebuggerCommandArguments`와 여섯 환경변수를 확인하고 관련 파일 `git diff --check` 실행.
- 사용자 수동 acceptance: VS `Release|x64` + `Release Server` 프로필에서 F5 1회, 종료, Ctrl+F5 1회. 두 실행 모두 `transport=udp`, `gameSession=winters-local-game-session`, `Replay upload queue enabled`, UDP 9000을 확인한다.

미검증:
- 사용자 수동 acceptance 전에는 실제 Visual Studio F5/Ctrl+F5 항목만 `CONFIRM_NEEDED`로 남긴다.

확인 필요:
- 실제 Visual Studio UI에서 `Release Server` 프로필의 F5/Ctrl+F5 수동 acceptance.
- `Winters.slnLaunch.user`는 이 workspace 사용자 설정이다. 다른 checkout에서는 최초 1회 Server-only profile 선택이 필요하다.

## 서브 에이전트 비평

1차 독립 비평: `P0 0 / P1 2 / P2 3 — FAIL`.

- P1 중복 서버의 선행 capacity 파괴: 수용. authenticated UDP named mutex를 reconciliation보다 먼저 획득하고 duplicate exit 9 + DB 불변 회귀를 추가했다.
- P1 실제 VS acceptance 부재: 수용. server-only solution launch profile과 F5/Ctrl+F5 수동 acceptance를 종료 기준에 추가했다.
- P2 환경 병합: 수용. `LocalDebuggerMergeEnvironment=true`를 명시한다.
- P2 smoke/q 모순: 수용. 대화형 VS/visible CMD에서는 smoke timeout을 제거해 `q + Enter` 정상 종료를 복구한다.
- P2 capture 추적성: 수용. stable session과 별도 `capture_run_id`를 `capture.json`에 함께 기록한다.

수정 계획의 독립 델타 재비평 전까지 제품 소스 수정 게이트는 닫혀 있다.

2차 독립 델타 비평: `P0 0 / P1 1 / P2 2 — FAIL`.

- P1 helper가 `UsesUdp` 정의보다 앞선 위치: 수용. helper 삽입 anchor를 `UsesUdp` 함수 바로 아래로 이동했다.
- P2 `.slnLaunch.user` portability: 수용. 현재 workspace 사용자 설정으로 범위를 한정하고 다른 checkout 최초 선택을 확인 필요에 남겼다.
- P2 수동 acceptance 표기 모순: 수용. 확인 필요에 실제 F5/Ctrl+F5를 명시했다. capture run ID는 `capture.json` 증거 디렉터리 식별자이며 server stdout 파일화를 주장하지 않는다.

3차 독립 델타 재비평: `P0 0 / P1 0 / P2 0 — PASS`.

- 이전 P1의 `UsesUdp` 선언 순서 수정 확인: helper anchor가 실제 `UsesUdp()` 정의 바로 아래이며 C3861 위험이 해소됐다.
- guard 획득은 `ParseRuntimeOptions()` 직후이고 `ReplayUploadQueue::StartFromEnvironment()` 및 그 내부 `ReconcileFreshServerCapacity()`보다 앞선다. 중복 프로세스는 exit 9로 Backend mutation 전에 종료하며 handle은 `main()` 전체 수명 동안 유지된다.
- VS Release 환경·workspace-local launch profile·visible launcher의 `q + Enter`·stable session과 별도 `capture_run_id`가 실제 앵커와 일치한다.
- 빌드·런타임·실제 VS F5/Ctrl+F5는 아직 수행했다고 주장하지 않고 검증/확인 필요에 남겨 두었다.

독립 비평의 P0/P1 잔존이 0이므로 제품 소스 수정 게이트를 통과했다.

# 2026-07-20 다계정 Replay 관전자 identity 종결

Session - canonical WRPL 하나를 공유하는 2~10개 계정이 각자 플레이한 챔피언을 기본 관전자 대상으로 복원한다
좌표: 신규 좌표 후보 · 축: C7 권위와 정합성, C8 검증이 병목
관련: 2026-07-19_RELEASE_AUTHENTICATED_ACCOUNT_REPLAY_E2E_PLAN.md / RESULT.md

## 1. 결정 기록

- 문제·제약: 실매치 `64fbeee5-b1e7-46b9-99e8-4bab5423cc4a`의 두 계정 cache는 checksum과 404,077,880-byte WRPL이 동일하다. 첫 snapshot에는 `net 1=LeeSin(17)`, `net 2=Yasuo(2)`를 포함한 champion 10개가 모두 있으나 canonical snapshot의 `yourNetId=0`과 `ApplyReplaySpectatorFocus()`의 최소 NetId 선택 때문에 두 client가 모두 net 1을 잡았다.
- 추진한 대안과 실패: 계정별 WRPL 복제는 동일 authoritative match를 중복 저장한다. `slot+1` 또는 champion id 추론은 bot·팀 이동·동일 champion에서 깨지고, 로컬 직전 선택 champion은 다른 PC나 나중 재생에서 사라진다.
- 메커니즘: Server가 authenticated user와 lobby spawn 뒤 확정된 champion NetId를 completion artifact에 함께 봉인한다. Replay service가 이를 `match_participants.replay_net_id`에 저장하고 account-authorized list 응답에 싣는다. Client는 download intent와 함께 그 NetId를 scene까지 전달하고 snapshot materialize/seek 뒤 정확한 entity를 바인딩한다.
- 대조군: WRPL은 계속 경기당 하나이고 `yourNetId=0`인 canonical spectator 파일이다. account metadata가 없는 legacy/local replay만 기존 최소 NetId fallback을 쓴다. metadata가 0이 아닌데 entity가 없으면 다른 champion으로 조용히 바꾸지 않는다.
- 대가: migration 한 쌍과 server→Go replay service→client metadata plumbing이 추가된다. 대신 2·3·10인 모두 파일 복제 없이 user→NetId가 명시적이고 동일 champion 중복에도 모호하지 않다.

## 2. 현재 코드 증거와 ownership

- `CGameRoom::Phase_BroadcastSnapshot()`은 replay snapshot을 `yourNetId=NULL_NET_ENTITY`로 한 번만 기록하므로 WRPL은 의도적으로 account-neutral이다.
- `CGameRoom::ApplyLobbyAuthorityResult()`에는 `userID -> sessionId -> LobbySlotState.netId`의 권위 mapping이 이미 존재한다. 이 시점이 perspective identity의 단일 생산자다.
- Replay service는 `match_participants`에서 account ACL을 만들고 `/replay/me`를 제공한다. perspective는 replay 파일 형식이 아니라 account authorization metadata의 소유다.
- Client `CloudReplayItem -> CClientShellBackendService -> CScene_MyInfo -> CScene_InGame`이 account replay 재생 intent의 소비 경로다. local/debug replay는 perspective 0으로 기존 spectator fallback을 유지한다.

## 3. 반영해야 하는 코드

### 3-1. C:/Users/user/Desktop/Winters/Server/Public/Game/GameRoom.h

기존 `AuthenticatedMatchParticipant`를 아래로 교체:

```cpp
    struct AuthenticatedMatchParticipant
    {
        u32_t sessionId = 0u;
        u8_t team = 0xFFu;
        NetEntityId replayNetId = NULL_NET_ENTITY;
    };
```

### 3-2. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomLobby.cpp

기존 코드:

```cpp
                    participant->second.team = slot.team;
                    finalUserIDs.push_back(user->second);
```

아래로 교체:

```cpp
                    participant->second.team = slot.team;
                    participant->second.replayNetId = slot.netId;
                    finalUserIDs.push_back(user->second);
```

### 3-3. C:/Users/user/Desktop/Winters/Server/Public/Backend/ReplayUploadQueue.h

`ReplayUploadParticipant::assists` 아래에 추가:

```cpp
    u32_t perspectiveNetId = 0u;
```

### 3-4. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomReplication.cpp

`<filesystem>` 위에 `<algorithm>`을 include한다. participant loop는 `team`, `winningTeam`뿐 아니라 `replayNetId != NULL_NET_ENTITY`를 완전 roster 조건으로 검사하고 아래 필드를 복사한다.

```cpp
            uploadParticipant.perspectiveNetId = participant.replayNetId;
```

loop 직후 아래를 추가해 custom lobby의 unordered account map 순서를 authoritative NetId 순서로 고정한다.

```cpp
        std::sort(
            artifact.participants.begin(), artifact.participants.end(),
            [](const ReplayUploadParticipant& lhs,
               const ReplayUploadParticipant& rhs)
            {
                return lhs.perspectiveNetId < rhs.perspectiveNetId;
            });
```

### 3-5. C:/Users/user/Desktop/Winters/Server/Private/Backend/ReplayUploadQueue.cpp

`ArtifactToJson()`과 `CompleteMatch()`의 participant JSON object에 아래를 추가:

```cpp
                {"perspective_net_id", participant.perspectiveNetId},
```

`JsonToArtifact()`의 assists 대입 아래에 추가한다. 0 default는 pre-change durable sidecar 호환이다.

```cpp
                participant.perspectiveNetId =
                    item.value("perspective_net_id", 0u);
```

### 3-6. 새 파일: C:/Users/user/Desktop/Winters/Services/migrations/000013_replay_participant_perspective.up.sql

```sql
ALTER TABLE match_participants
    ADD COLUMN replay_net_id BIGINT
        CHECK (replay_net_id IS NULL OR
               (replay_net_id > 0 AND replay_net_id <= 4294967295));
```

### 3-7. 새 파일: C:/Users/user/Desktop/Winters/Services/migrations/000013_replay_participant_perspective.down.sql

```sql
ALTER TABLE match_participants
    DROP COLUMN IF EXISTS replay_net_id;
```

### 3-8. C:/Users/user/Desktop/Winters/Services/internal/replay/model.go

`Replay`에는 account-authorized 응답용 필드를, `MatchCompletionPlayer`에는 server 입력 필드를 추가한다.

```go
	PerspectiveNetID int64 `json:"perspective_net_id,omitempty"`
```

```go
	PerspectiveNetID int64 `json:"perspective_net_id"`
```

### 3-9. C:/Users/user/Desktop/Winters/Services/internal/replay/service.go

`CompleteMatch()`는 user/result와 0 또는 uint32 범위의 perspective를 검증하고 0이 아닌 perspective 중복을 거부한 뒤 repository에 넘긴다. 0은 legacy sidecar에만 허용한다.

### 3-10. C:/Users/user/Desktop/Winters/Services/internal/replay/repository.go

- authorized get/list query는 `replay_user_library`와 같은 `(match_id,user_id)`의 `match_participants`를 join해 `COALESCE(replay_net_id,0)`을 `Replay.PerspectiveNetID`에 scan한다.
- completion participant lock query도 `COALESCE(replay_net_id,0)`을 읽는다.
- custom lobby INSERT와 정상 completion UPDATE는 `NULLIF(perspective_net_id,0)`을 기록한다.
- 이미 completed인 retry는 result 또는 기존 non-zero perspective가 다르면 conflict, 기존 perspective가 0이고 요청이 non-zero이면 같은 transaction에서 보강한다.
- 기존 ACL 생성 SQL과 WRPL row는 바꾸지 않는다.

### 3-11. C:/Users/user/Desktop/Winters/Services/internal/replay/service_test.go

dynamic 1·2·3·4·10명 fixture는 `PerspectiveNetID: int64(index + 1)`을 넣고, 같은 non-zero perspective 중복이 `ErrInvalidInput`인지 별도 검증한다.

### 3-12. C:/Users/user/Desktop/Winters/Services/internal/replay/repository_integration_test.go

dynamic ACL과 custom lobby 1·2·3·4·10명 fixture에 서로 다른 `replay_net_id`를 넣고 `GetAuthorized`/`ListAuthorized`가 각 user 값을 보존하는지 비교한다. outsider 거부와 count 검증은 유지한다.

### 3-13. C:/Users/user/Desktop/Winters/Client/Public/Network/Backend/ReplayClient.h

`CloudReplayItem::downloaded` 아래에 추가:

```cpp
    u32_t perspectiveNetId = 0u;
```

### 3-14. C:/Users/user/Desktop/Winters/Client/Private/Network/Backend/ReplayClient.cpp

`ListMine()`의 downloaded 대입 아래에 추가:

```cpp
                item.perspectiveNetId = value.value("perspective_net_id", 0u);
```

### 3-15. C:/Users/user/Desktop/Winters/Client/Public/ClientShell/ClientShellBackendService.h

기존 consume 선언을 아래로 교체하고 ready path 옆에 perspective member를 추가한다.

```cpp
	bool_t ConsumeReplayPlaybackPath(
		wstring_t& outPath,
		u32_t& outPerspectiveNetId);
```

```cpp
	u32_t m_uReadyReplayPerspectiveNetId = 0u;
```

### 3-16. C:/Users/user/Desktop/Winters/Client/Private/ClientShell/ClientShellBackendService.cpp

`BeginReplayDownload()`은 item perspective를 값 capture하고 playback path와 같은 intent check 안에서 ready perspective에 저장한다. `ConsumeReplayPlaybackPath()`은 path와 perspective를 함께 반환한 뒤 둘 다 clear한다. `CancelReplayPlaybackIntent()`와 `DestroyClients()`도 perspective를 0으로 clear한다.

### 3-17. C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_MyInfo.h

기존 선언을 아래로 교체:

```cpp
	void OpenReplay(const wstring_t& path, u32_t perspectiveNetId = 0u);
```

### 3-18. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_MyInfo.cpp

`OnUpdate()`은 perspective를 path와 함께 consume해 `OpenReplay(replayPath, perspectiveNetId)`를 호출한다. `OpenReplay()` lambda는 둘을 값 capture하고 `new CScene_InGame(path, perspectiveNetId)`를 생성한다. local/debug list 호출은 기본값 0을 유지한다.

### 3-19. C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_InGame.h

기존 replay constructor를 아래로 교체하고 `m_strReplayPath` 아래에 member를 추가한다.

```cpp
    explicit CScene_InGame(
        const wstring_t& replayPath,
        NetEntityId replayPerspectiveNetId = NULL_NET_ENTITY);
```

```cpp
    NetEntityId m_replayPerspectiveNetId = NULL_NET_ENTITY;
```

### 3-20. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

기존 replay constructor를 아래로 교체:

```cpp
CScene_InGame::CScene_InGame(
    const wstring_t& replayPath,
    NetEntityId replayPerspectiveNetId)
    : m_bReplayPlaybackMode(true)
    , m_strReplayPath(replayPath)
    , m_replayPerspectiveNetId(replayPerspectiveNetId)
{
}
```

### 3-21. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameImGui.cpp

`ApplyReplaySpectatorFocus()`의 map null 검사 바로 아래에 추가한다. metadata가 유효하면 정확한 target만 허용하고, 없는 legacy/local replay만 기존 최소 NetId loop로 내려간다.

```cpp
    if (m_replayPerspectiveNetId != NULL_NET_ENTITY)
    {
        if (!ApplyAuthoritativePlayerNetId(m_replayPerspectiveNetId))
            m_strReplayStatus = "Replay account perspective is unavailable";
        return;
    }
```

## 4. 단계와 검증

1. schema/server metadata 봉인 -> Go unit·integration에서 1·2·3·4·10명 distinct perspective와 outsider 0을 검증한다.
2. account list/client intent 전달 -> `/replay/me` 응답의 계정별 perspective와 C++ Release compile을 검증한다.
3. reported replay 복구 -> migration 적용 뒤 해당 match의 DB slot 0/1과 실파일 net 1/2 증거에 한정해 `zxcv3=1`, `zxcv4=2`를 조건부 backfill하고 두 user API 응답을 확인한다.
4. runtime -> 기존 두 cache checksum은 그대로 두고 account replay를 열었을 때 zxcv3 net 1 LeeSin, zxcv4 net 2 Yasuo가 기본 focus가 되는지 확인한다. 3인 신규 match는 세 account의 distinct non-zero perspective와 각 기본 focus를 확인한다.

예측:

- WRPL record/snapshot/event/checksum은 변하지 않고 account list JSON만 perspective를 추가한다.
- 2·3개 client가 같은 파일을 받아도 서로 다른 replay perspective를 갖는다.
- seek 뒤에도 같은 requested NetId를 다시 잡으며 다른 client의 pause/seek/camera 상태에는 영향이 없다.
- legacy/local replay는 perspective 0이므로 기존 deterministic 최소 champion focus를 유지한다.

검증 명령:

- `gofmt -w Services/internal/replay/model.go Services/internal/replay/service.go Services/internal/replay/repository.go Services/internal/replay/service_test.go Services/internal/replay/repository_integration_test.go`
- `cd Services; go test ./internal/replay`
- `cd Services; $env:WINTERS_TEST_DATABASE_URL='postgres://winters:winters@127.0.0.1:5433/winters?sslmode=disable'; go test ./internal/replay -run 'TestRepository' -count=1`
- `msbuild Server/Include/Server.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /m:1`
- `msbuild Client/Include/Client.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /m:1`
- migration/replay service rebuild 후 SQL column/check, 두 account의 authenticated `/replay/me`, 기존 WRPL first snapshot binary probe, 관련 `git diff --check`.

미검증:

- 실제 화면에서 두 account replay의 LeeSin/Yasuo 기본 focus와 신규 3-account visual focus는 서로 다른 로그인 화면 입력 없이는 자동 판정하지 않는다.

확인 필요:

- 최종 빌드 뒤 기존 두 계정 replay를 다시 열어 visual focus를 확인한 결과. VS F5/Ctrl+F5 acceptance는 이미 PASS했고 다시 요청하지 않는다.

## 5. 서브 에이전트 비평

독립 read-only 비평: pending. Source edit gate는 닫혀 있다.

## 6. 1차 독립 비평 disposition

1차 비평: `P0 0 / P1 5 / P2 3 — FAIL`. 모든 지적을 수용하며 source edit gate는 계속 닫는다.

- P1 기존 match backfill 입증 부족: 수용. 과거 `match_participants.slot`은 unordered artifact index이므로 lobby slot/NetId 증거로 사용하지 않는다. `zxcv3↔LeeSin`, `zxcv4↔Yasuo` 사용자 확인 전에는 기존 DB를 수정하지 않는다.
- P1 account ReplayCache perspective 유실: 수용. download가 WRPL 옆 atomic `.perspective` sidecar를 쓰고 `ReplayLibrary`가 이를 `ReplayListItem`에 읽어 cache 재생에도 전달한다.
- P1 repository query/scanner 미종결: 수용. `r.` alias-qualified column set과 perspective 전용 scanner 두 개를 명시한다.
- P1 legacy/idempotency 행렬 누락: 수용. legacy 0, 0→non-zero 보강, same retry, mismatch conflict, non-zero+legacy0 no-erase, duplicate rejection을 unit/integration에 고정한다.
- P1 prose-only dirty edit: 수용. 아래 §7의 exact anchor/code가 §3의 요약을 대체한다. 다른 세션 dirty line은 이 anchor 외에는 수정하지 않는다.
- P2 seek 문구가 perspective 실패를 덮음: 수용. focus 함수가 bool을 반환하고 update/seek가 false를 보존한다.
- P2 배포 순서: 수용. `migration → replay service → server → client`로 고정한다.
- P2 3-account acceptance 모순: 수용. 3인은 Go/DB/API distinct perspective 자동 계약으로 종결하고, 실제 visual acceptance는 사용자가 이미 가진 2계정 기존 match의 LeeSin/Yasuo 재생만 대상으로 한다.

## 7. 수정된 exact source 계약

### 7-1. Server participant 봉인

§3-1~3-3은 그대로 적용한다. `Server/Private/Game/GameRoomReplication.cpp`의 include와 participant loop 전체는 다음 exact edit로 적용한다.

기존 코드:

```cpp
#include <filesystem>
```

바로 위에 추가:

```cpp
#include <algorithm>
```

기존 코드:

```cpp
        for (const auto& [userID, participant] : m_authenticatedParticipants)
        {
            if (participant.team == 0xFFu || m_winningTeam == 0xFFu)
            {
                bHasCompleteRoster = false;
                break;
            }
            ReplayUploadParticipant uploadParticipant{};
            uploadParticipant.userID = userID;
            uploadParticipant.result = participant.team == m_winningTeam
                ? "win"
                : "loss";
            artifact.participants.push_back(std::move(uploadParticipant));
        }
```

아래로 교체:

```cpp
        for (const auto& [userID, participant] : m_authenticatedParticipants)
        {
            if (participant.team == 0xFFu ||
                participant.replayNetId == NULL_NET_ENTITY ||
                m_winningTeam == 0xFFu)
            {
                bHasCompleteRoster = false;
                break;
            }
            ReplayUploadParticipant uploadParticipant{};
            uploadParticipant.userID = userID;
            uploadParticipant.result = participant.team == m_winningTeam
                ? "win"
                : "loss";
            uploadParticipant.perspectiveNetId = participant.replayNetId;
            artifact.participants.push_back(std::move(uploadParticipant));
        }
        std::sort(
            artifact.participants.begin(), artifact.participants.end(),
            [](const ReplayUploadParticipant& lhs,
               const ReplayUploadParticipant& rhs)
            {
                return lhs.perspectiveNetId < rhs.perspectiveNetId;
            });
```

### 7-2. Replay upload durable JSON

§3-5의 세 exact insertion을 적용한다. `item.value("perspective_net_id", 0u)`가 key 없는 legacy sidecar를 0으로 복원하며, server 신규 artifact는 Finalize gate에서 non-zero만 enqueue한다.

### 7-3. Go model·validation

§3-6~3-8 migration/model을 적용한다. `Services/internal/replay/service.go`의 기존 `CompleteMatch()` 전체를 아래로 교체하고 helper를 바로 아래에 둔다.

```go
func (s *Service) CompleteMatch(
	ctx context.Context,
	matchID uuid.UUID,
	request MatchCompletionRequest,
) error {
	if matchID == uuid.Nil || len(request.Players) == 0 || len(request.Players) > 10 {
		return apperr.ErrInvalidInput
	}
	if err := validateMatchCompletionPlayers(request.Players); err != nil {
		return err
	}
	return s.repo.CompleteMatch(ctx, matchID, request.Players)
}

func validateMatchCompletionPlayers(players []MatchCompletionPlayer) error {
	seenUsers := make(map[uuid.UUID]struct{}, len(players))
	seenPerspectives := make(map[int64]struct{}, len(players))
	for _, player := range players {
		if player.UserID == uuid.Nil ||
			(player.Result != "win" && player.Result != "loss" && player.Result != "draw") ||
			player.PerspectiveNetID < 0 ||
			player.PerspectiveNetID > int64(^uint32(0)) {
			return apperr.ErrInvalidInput
		}
		if _, duplicate := seenUsers[player.UserID]; duplicate {
			return apperr.ErrInvalidInput
		}
		seenUsers[player.UserID] = struct{}{}
		if player.PerspectiveNetID != 0 {
			if _, duplicate := seenPerspectives[player.PerspectiveNetID]; duplicate {
				return apperr.ErrInvalidInput
			}
			seenPerspectives[player.PerspectiveNetID] = struct{}{}
		}
	}
	return nil
}
```

### 7-4. Repository qualified access scan

`replayColumns` 바로 아래에 다음 qualified constant를 추가한다.

```go
const authorizedReplayColumns = `
    r.id, r.match_id, r.status, r.object_key, COALESCE(r.upload_id, ''),
    COALESCE(r.size_bytes, 0), COALESCE(r.checksum_sha256, ''),
    r.format_version, r.tick_rate, r.record_count, r.snapshot_count,
    r.event_count, r.command_count, r.first_tick, r.last_tick,
    r.created_at, r.ready_at, r.expires_at`
```

`GetAuthorized()` 전체를 아래로 교체한다.

```go
func (r *Repository) GetAuthorized(
	ctx context.Context,
	replayID, userID uuid.UUID,
) (Replay, error) {
	item, err := scanReplayWithPerspective(r.db.QueryRow(ctx,
		`SELECT `+authorizedReplayColumns+`, COALESCE(mp.replay_net_id, 0)
		 FROM replays r
		 JOIN replay_user_library l ON l.replay_id = r.id
		 JOIN match_participants mp
		   ON mp.match_id = r.match_id AND mp.user_id = l.user_id
		 WHERE r.id = $1 AND l.user_id = $2 AND l.hidden_at IS NULL`,
		replayID, userID))
	if errors.Is(err, pgx.ErrNoRows) {
		return Replay{}, apperr.ErrForbidden
	}
	if err != nil {
		return Replay{}, fmt.Errorf("get authorized replay: %w", err)
	}
	return item, nil
}
```

`ListAuthorized()` query의 SELECT/FROM/JOIN 부분을 아래로 교체한다.

```go
	rows, err := r.db.Query(ctx,
		`SELECT `+authorizedReplayColumns+`,
		        l.last_downloaded_at IS NOT NULL,
		        COALESCE(mp.replay_net_id, 0)
		 FROM replays r
		 JOIN replay_user_library l ON l.replay_id = r.id
		 JOIN match_participants mp
		   ON mp.match_id = r.match_id AND mp.user_id = l.user_id
		 WHERE l.user_id = $1 AND l.hidden_at IS NULL AND r.status = 'ready'
		   AND ($2::timestamptz IS NULL OR (r.created_at, r.id) < ($2, $3))
		 ORDER BY r.created_at DESC, r.id DESC LIMIT $4`,
		userID, cursorTime, cursorID, limit)
```

기존 `scanReplayWithDownloaded()` 전체를 아래 두 함수로 교체하고 list loop 호출은 `scanReplayWithDownloadedAndPerspective(rows)`로 교체한다.

```go
func scanReplayWithPerspective(row pgx.Row) (Replay, error) {
	var item Replay
	err := row.Scan(
		&item.ID, &item.MatchID, &item.Status, &item.ObjectKey, &item.UploadID,
		&item.SizeBytes, &item.ChecksumSHA256, &item.FormatVersion, &item.TickRate,
		&item.RecordCount, &item.SnapshotCount, &item.EventCount, &item.CommandCount,
		&item.FirstTick, &item.LastTick, &item.CreatedAt, &item.ReadyAt, &item.ExpiresAt,
		&item.PerspectiveNetID,
	)
	return item, err
}

func scanReplayWithDownloadedAndPerspective(row pgx.Row) (Replay, error) {
	var item Replay
	err := row.Scan(
		&item.ID, &item.MatchID, &item.Status, &item.ObjectKey, &item.UploadID,
		&item.SizeBytes, &item.ChecksumSHA256, &item.FormatVersion, &item.TickRate,
		&item.RecordCount, &item.SnapshotCount, &item.EventCount, &item.CommandCount,
		&item.FirstTick, &item.LastTick, &item.CreatedAt, &item.ReadyAt, &item.ExpiresAt,
		&item.Downloaded, &item.PerspectiveNetID,
	)
	return item, err
}
```

### 7-5. Repository completion identity

`CompleteMatch()`는 시작 시 `validateMatchCompletionPlayers(players)`를 호출한다. participant lock은 `user_id, result, COALESCE(replay_net_id,0)`을 scan하는 `{result string; perspectiveNetID int64}` map으로 교체한다. custom INSERT는 아래 column/value를 사용한다.

```go
			if _, err := tx.Exec(ctx, `
				INSERT INTO match_participants (
					match_id, user_id, slot, result, replay_net_id, joined_at)
				VALUES ($1, $2, $3, $4, NULLIF($5, 0), NOW())`,
				matchID, player.UserID, index, player.Result,
				player.PerspectiveNetID); err != nil {
```

completed retry loop 정책은 아래 exact block이다.

```go
	for _, player := range players {
		current, ok := existing[player.UserID]
		if !ok {
			return apperr.ErrForbidden
		}
		if status == "completed" {
			if current.result != player.Result ||
				(current.perspectiveNetID != 0 &&
				 player.PerspectiveNetID != 0 &&
				 current.perspectiveNetID != player.PerspectiveNetID) {
				return apperr.ErrIdempotencyConflict
			}
			if current.perspectiveNetID == 0 && player.PerspectiveNetID != 0 {
				if _, err := tx.Exec(ctx, `
					UPDATE match_participants SET replay_net_id = $3
					WHERE match_id = $1 AND user_id = $2 AND replay_net_id IS NULL`,
					matchID, player.UserID, player.PerspectiveNetID); err != nil {
					return fmt.Errorf("backfill replay perspective: %w", err)
				}
			}
		}
	}
```

정상 completion UPDATE는 아래로 교체한다. legacy 0은 기존 non-zero를 지우지 않는다.

```go
		if _, err := tx.Exec(ctx,
			`UPDATE match_participants
			 SET result = $3,
			     replay_net_id = COALESCE(NULLIF($4, 0), replay_net_id),
			     joined_at = COALESCE(joined_at, NOW())
			 WHERE match_id = $1 AND user_id = $2`,
			matchID, player.UserID, player.Result,
			player.PerspectiveNetID); err != nil {
```

### 7-6. Go test matrix

`service_test.go` dynamic players에는 §3-11의 distinct perspective를 넣는다. 아래 test를 `TestCompleteMatchAcceptsDynamicParticipantCounts` 바로 아래에 추가한다.

```go
func TestCompleteMatchRejectsDuplicatePerspective(t *testing.T) {
	service := NewService(&fakeRepository{}, &fakeStorage{}, testReplayConfig())
	err := service.CompleteMatch(
		context.Background(), uuid.New(), MatchCompletionRequest{Players: []MatchCompletionPlayer{
			{UserID: uuid.New(), Result: "win", PerspectiveNetID: 7},
			{UserID: uuid.New(), Result: "loss", PerspectiveNetID: 7},
		}})
	if !errors.Is(err, apperr.ErrInvalidInput) {
		t.Fatalf("duplicate perspective error = %v, want invalid input", err)
	}
}
```

`repository_integration_test.go`는 각 participant/player fixture에 `PerspectiveNetID=index+1`을 넣고 authorized result와 list result의 값을 비교한다. custom completion 뒤 동일 players retry PASS, legacy 0 retry no-erase, 한 user mismatch conflict를 차례로 검증한다. 별도 legacy fixture는 perspective 0 completion→NULL DB/list 0→non-zero retry backfill→same retry no-op 순으로 검증한다.

### 7-7. Account cache perspective sidecar

`Client/Public/Replay/ReplayLibrary.h`의 `ReplayListItem::bLocalDebug` 아래에 추가:

```cpp
	u32_t perspectiveNetId = 0u;
```

`Client/Private/Replay/ReplayLibrary.cpp`에 `<fstream>`, `<limits>`를 include하고 `item.fileSizeBytes` 대입 아래에 추가:

```cpp
			if (!bLocalDebug)
			{
				std::ifstream perspectiveInput(
					path.wstring() + L".perspective");
				u64_t perspectiveNetId = 0u;
				if (perspectiveInput >> perspectiveNetId &&
					perspectiveNetId <= (std::numeric_limits<u32_t>::max)())
				{
					item.perspectiveNetId =
						static_cast<u32_t>(perspectiveNetId);
				}
			}
```

`ReplayDownloadResult`에는 `u32_t perspectiveNetId = 0u;`를 추가한다. `DownloadReplay()` 시작에서 `result.perspectiveNetId = item.perspectiveNetId;`를 대입한다. WRPL의 `MoveFileExW` 성공 뒤 `destination + L".perspective"` 임시 파일에 decimal perspective와 newline을 쓰고 flush/close한 뒤 `MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH`로 atomic rename한다. sidecar 실패는 `replay perspective metadata write failed`로 download 실패 처리한다.

### 7-8. Client intent exact edits

§3-13~3-15를 적용한다. `BeginReplayDownload()` success intent block은 아래로 교체:

```cpp
					if (bOpenAfterDownload &&
						uPlaybackIntent == m_uReplayPlaybackIntent)
					{
						m_strReadyReplayPlaybackPath = result.localPath;
						m_uReadyReplayPerspectiveNetId =
							result.perspectiveNetId;
					}
```

`ConsumeReplayPlaybackPath()` 전체를 아래로 교체:

```cpp
bool_t CClientShellBackendService::ConsumeReplayPlaybackPath(
	wstring_t& outPath,
	u32_t& outPerspectiveNetId)
{
	if (m_strReadyReplayPlaybackPath.empty())
		return false;
	outPath = std::move(m_strReadyReplayPlaybackPath);
	outPerspectiveNetId = m_uReadyReplayPerspectiveNetId;
	m_strReadyReplayPlaybackPath.clear();
	m_uReadyReplayPerspectiveNetId = 0u;
	return !outPath.empty();
}
```

`CancelReplayPlaybackIntent()`과 `DestroyClients()`의 path clear 바로 아래에 각각 `m_uReadyReplayPerspectiveNetId = 0u;`를 추가한다.

### 7-9. MyInfo cloud·cache 전달

§3-17을 적용한다. `OnUpdate()`의 replay consume block을 아래로 교체:

```cpp
	wstring_t replayPath;
	u32_t perspectiveNetId = 0u;
	if (!m_bSceneTransitionStarted &&
		backend.ConsumeReplayPlaybackPath(replayPath, perspectiveNetId))
	{
		OpenReplay(replayPath, perspectiveNetId);
		return;
	}
```

cache button 호출을 아래로 교체:

```cpp
			if (ImGui::Button("재생"))
				OpenReplay(item.path, item.perspectiveNetId);
```

`OpenReplay()` 전체를 아래로 교체:

```cpp
void CScene_MyInfo::OpenReplay(
	const wstring_t& path,
	u32_t perspectiveNetId)
{
	if (m_bSceneTransitionStarted)
		return;

	m_bSceneTransitionStarted = true;
	Client::CLoLMatchContextRuntime::Instance().Reset();
	auto pLoadingMatch = CScene_MatchLoading::Create(
		[path, perspectiveNetId]() -> std::unique_ptr<IScene>
		{
			return std::unique_ptr<IScene>(
				new CScene_InGame(path, perspectiveNetId));
		}, 1.f);

	CGameInstance::Get()->Change_Scene(
		static_cast<u32_t>(eSceneID::MatchLoading),
		std::move(pLoadingMatch));
}
```

### 7-10. Replay scene focus failure 보존

§3-19~3-20을 적용한다. `Scene_InGame.h`의 `ApplyReplaySpectatorFocus` 반환형을 `bool_t`로 교체한다. `Scene_InGameImGui.cpp` 함수 전체를 아래로 교체:

```cpp
bool_t CScene_InGame::ApplyReplaySpectatorFocus()
{
	if (!m_pEntityIdMap)
		return false;

	if (m_replayPerspectiveNetId != NULL_NET_ENTITY)
	{
		if (ApplyAuthoritativePlayerNetId(m_replayPerspectiveNetId))
			return true;
		m_strReplayStatus = "Replay account perspective is unavailable";
		return false;
	}

	NetEntityId focusNetId = NULL_NET_ENTITY;
	m_pEntityIdMap->ForEachBinding(
		[this, &focusNetId](NetEntityId netId, EntityID entity)
		{
			if (m_World.IsAlive(entity) &&
				m_World.HasComponent<ChampionComponent>(entity) &&
				(focusNetId == NULL_NET_ENTITY || netId < focusNetId))
			{
				focusNetId = netId;
			}
		});
	return ApplyAuthoritativePlayerNetId(focusNetId);
}
```

`UpdateReplayPlayback()`에서 replay player update 성공 block은 focus false이면 projection 뒤 즉시 return한다. `SeekReplayToTick()`은 snapshot seek 성공 후 focus가 false이면 projection만 수행하고 false를 반환하며 unavailable status를 덮지 않는다. focus true일 때만 `Replay Chrono seek complete`를 쓴다.

### 7-11. 배포·과거 경기 복구 순서

1. migration image/apply.
2. replay service deploy와 Go/DB/API 검증.
3. Server deploy; 신규 artifact non-zero perspective 검증.
4. Client deploy; Cloud 즉시 재생과 account cache sidecar 재생 검증.
5. 기존 match는 사용자 계정↔champion 확인을 받은 뒤에만 completed retry endpoint 또는 정확한 조건부 SQL로 perspective를 보강한다. 확인 전 DB mutation은 금지한다.

수정 계획의 독립 델타 재비평 전까지 source edit gate는 닫혀 있다.

## 8. 2차 델타 보정 — account cache fail-closed와 exact test

중간 재비평의 P1을 수용한다. §7-7의 “WRPL rename 뒤 sidecar write” 문장은 폐기하고 아래 계약으로 대체한다.

### 8-1. Cache visibility와 malformed metadata

- account Cloud item의 perspective 0은 재생·다운로드를 거부한다. legacy upload 자체는 보존하지만 user→NetId가 복구되기 전 임의 champion으로 재생하지 않는다.
- account cache의 `.perspective`가 missing, zero, malformed, truncated, uint32 overflow이면 `ReplayListItem.perspectiveNetId=0`으로 두되 UI가 재생 버튼을 disable한다. 최소 NetId fallback은 `bLocalDebug=true`인 local/debug WRPL에만 허용한다.
- download는 `.wrpl.part` 무결성 검증 뒤 `.perspective.part -> .perspective`를 먼저 atomic rename하고, 마지막에 `.wrpl.part -> .wrpl`을 rename한다. 따라서 새 account WRPL이 목록에 나타나는 순간 non-zero sidecar가 이미 존재한다. 같은 replay의 기존 WRPL이 있으면 새 sidecar는 동일 account perspective이므로 WRPL replace 실패 시에도 잘못된 다른 entity를 가리키지 않는다.

`Client/Private/Network/Backend/ReplayClient.cpp` anonymous namespace의 `DownloadReplay()` 바로 위에 추가:

```cpp
	bool_t WriteReplayPerspectiveSidecar(
		const std::filesystem::path& replayPath,
		u32_t perspectiveNetId)
	{
		if (perspectiveNetId == 0u)
			return false;

		const std::filesystem::path sidecar =
			replayPath.wstring() + L".perspective";
		const std::filesystem::path temporary =
			sidecar.wstring() + L".part";
		std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
		output << perspectiveNetId << '\n';
		output.flush();
		const bool_t outputGood = output.good();
		output.close();
		if (!outputGood || !MoveFileExW(
			temporary.c_str(), sidecar.c_str(),
			MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		{
			std::error_code ignored;
			std::filesystem::remove(temporary, ignored);
			return false;
		}
		return true;
	}
```

`DownloadReplay()`의 metadata validation 조건에 아래를 추가:

```cpp
			item.perspectiveNetId == 0u ||
```

`ReplayDownloadResult result{}` 바로 아래에 추가:

```cpp
		result.perspectiveNetId = item.perspectiveNetId;
```

기존 digest/prefix/MoveFileEx 결합 block 전체를 아래로 교체:

```cpp
		if (cryptoStatus < 0 || !outputGood || !validPrefix ||
			formatVersion != static_cast<u16_t>(item.formatVersion) ||
			DigestToHex(digest) != item.checksumSha256)
		{
			std::filesystem::remove(temporary, fileError);
			result.error = "downloaded replay failed integrity validation";
			return result;
		}
		if (!WriteReplayPerspectiveSidecar(destination, item.perspectiveNetId))
		{
			std::filesystem::remove(temporary, fileError);
			result.error = "replay perspective metadata write failed";
			return result;
		}
		if (!MoveFileExW(
			temporary.c_str(), destination.c_str(),
			MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		{
			std::filesystem::remove(temporary, fileError);
			result.error = "downloaded replay cache publish failed";
			return result;
		}

		result.success = true;
		result.localPath = destination.wstring();
		return result;
```

### 8-2. Account cache UI fail-closed

`Client/Private/Scene/Scene_MyInfo.cpp::DrawReplayItems()`의 button block을 아래로 교체:

```cpp
			const bool_t bCanPlay =
				item.bLocalDebug || item.perspectiveNetId != 0u;
			if (!bCanPlay)
				ImGui::BeginDisabled();
			if (ImGui::Button("재생"))
				OpenReplay(item.path, item.perspectiveNetId);
			if (!bCanPlay)
			{
				ImGui::EndDisabled();
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
					ImGui::SetTooltip(
						"계정 관전자 정보가 없습니다. Cloud / account에서 다시 받으세요.");
			}
```

`DrawCloudReplayItems()` action block을 아래로 교체:

```cpp
			const bool_t bCanPlay =
				!backend.IsReplayRequestInFlight() &&
				item.perspectiveNetId != 0u;
			if (!bCanPlay)
				ImGui::BeginDisabled();
			if (ImGui::Button("다시보기"))
				backend.RequestReplayPlayback(item);
			if (!bCanPlay)
			{
				ImGui::EndDisabled();
				if (item.perspectiveNetId == 0u &&
					ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				{
					ImGui::SetTooltip("계정 관전자 정보 복구가 필요합니다.");
				}
			}
```

`CClientShellBackendService::BeginReplayDownload()` 첫 guard 바로 아래에 추가:

```cpp
	if (item.perspectiveNetId == 0u)
	{
		m_strStatus = "Replay account perspective is unavailable";
		return;
	}
```

### 8-3. Repository lock map exact block

`CompleteMatch()`의 aborted status 검사 바로 아래부터 기존 `rows.Close()`와 `seen` validation loop 끝까지를 아래로 교체:

```go
	if err := validateMatchCompletionPlayers(players); err != nil {
		return err
	}

	type storedParticipant struct {
		result           string
		perspectiveNetID int64
	}
	rows, err := tx.Query(ctx,
		`SELECT user_id, result, COALESCE(replay_net_id, 0)
		 FROM match_participants WHERE match_id = $1 FOR UPDATE`,
		matchID)
	if err != nil {
		return fmt.Errorf("lock match participants: %w", err)
	}
	existing := make(map[uuid.UUID]storedParticipant)
	for rows.Next() {
		var userID uuid.UUID
		var result *string
		var perspectiveNetID int64
		if err := rows.Scan(&userID, &result, &perspectiveNetID); err != nil {
			rows.Close()
			return fmt.Errorf("scan match participant: %w", err)
		}
		stored := storedParticipant{perspectiveNetID: perspectiveNetID}
		if result != nil {
			stored.result = *result
		}
		existing[userID] = stored
	}
	if err := rows.Err(); err != nil {
		rows.Close()
		return fmt.Errorf("iterate match participants: %w", err)
	}
	rows.Close()
```

custom INSERT 직후 map 대입은 아래로 교체:

```go
			existing[player.UserID] = storedParticipant{
				result: player.Result,
				perspectiveNetID: player.PerspectiveNetID,
			}
```

### 8-4. Update/seek exact block

`UpdateReplayPlayback()`의 player update 성공 block을 아래로 교체:

```cpp
	if (m_pReplayPlayer->Update(
		dt,
		m_World,
		*m_pEntityIdMap,
		*m_pSnapshotApplier,
		*m_pEventApplier))
	{
		const bool_t bFocusApplied = ApplyReplaySpectatorFocus();
		ProjectGameplayActorsToMapSurface();
		if (!bFocusApplied)
			return;
	}
```

`SeekReplayToTick()`의 `if (bApplied)` block을 아래로 교체:

```cpp
	if (bApplied)
	{
		const bool_t bFocusApplied = ApplyReplaySpectatorFocus();
		ProjectGameplayActorsToMapSurface();
		if (!bFocusApplied)
			return false;
		m_strReplayStatus = "Replay Chrono seek complete";
		return true;
	}
```

### 8-5. Integration retry matrix exact test

`Services/internal/replay/repository_integration_test.go` 끝에 아래 test를 추가한다.

```go
func TestRepositoryReplayPerspectiveLegacyRetryMatrix(t *testing.T) {
	databaseURL := os.Getenv("WINTERS_TEST_DATABASE_URL")
	if databaseURL == "" {
		t.Skip("WINTERS_TEST_DATABASE_URL is required")
	}
	ctx := context.Background()
	db, err := pgxpool.New(ctx, databaseURL)
	if err != nil {
		t.Fatal(err)
	}
	defer db.Close()
	repository := NewRepository(db)

	matchID := uuid.New()
	replayID := uuid.New()
	userID := uuid.New()
	name := "perspective_" + userID.String()
	if _, err := db.Exec(ctx, `
		INSERT INTO users (id, username, email, password)
		VALUES ($1, $2, $3, 'integration')`,
		userID, name[:32], name+"@test.invalid"); err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() {
		_, _ = db.Exec(context.Background(),
			`DELETE FROM matches WHERE id = $1`, matchID)
		_, _ = db.Exec(context.Background(),
			`DELETE FROM users WHERE id = $1`, userID)
	})
	if _, err := db.Exec(ctx, `
		INSERT INTO matches (id, status, game_session_id, game_session_generation)
		VALUES ($1, 'allocated', $2, 1)`, matchID, "perspective-"+matchID.String()); err != nil {
		t.Fatal(err)
	}
	if _, err := db.Exec(ctx, `
		INSERT INTO match_lobby_admissions (match_id, user_id)
		VALUES ($1, $2)`, matchID, userID); err != nil {
		t.Fatal(err)
	}
	legacy := []MatchCompletionPlayer{{UserID: userID, Result: "win"}}
	if err := repository.CompleteMatch(ctx, matchID, legacy); err != nil {
		t.Fatalf("legacy completion: %v", err)
	}
	if _, err := db.Exec(ctx, `
		INSERT INTO replays (
			id, match_id, status, object_key, size_bytes,
			checksum_sha256, format_version, tick_rate,
			record_count, snapshot_count, event_count, command_count,
			first_tick, last_tick, expires_at)
		VALUES ($1, $2, 'uploading', $3, 1024, $4, 2, 30,
			3, 1, 1, 1, 1, 30, $5)`,
		replayID, matchID, "perspective-integration/"+replayID.String(),
		strings.Repeat("c", 64), time.Now().Add(time.Hour)); err != nil {
		t.Fatal(err)
	}
	if _, err := repository.MarkReady(ctx, replayID); err != nil {
		t.Fatal(err)
	}
	item, err := repository.GetAuthorized(ctx, replayID, userID)
	if err != nil || item.PerspectiveNetID != 0 {
		t.Fatalf("legacy perspective = %d, error = %v", item.PerspectiveNetID, err)
	}

	backfill := []MatchCompletionPlayer{{
		UserID: userID, Result: "win", PerspectiveNetID: 7,
	}}
	if err := repository.CompleteMatch(ctx, matchID, backfill); err != nil {
		t.Fatalf("backfill completion: %v", err)
	}
	if err := repository.CompleteMatch(ctx, matchID, backfill); err != nil {
		t.Fatalf("same perspective retry: %v", err)
	}
	if err := repository.CompleteMatch(ctx, matchID, legacy); err != nil {
		t.Fatalf("legacy retry after backfill: %v", err)
	}
	item, err = repository.GetAuthorized(ctx, replayID, userID)
	if err != nil || item.PerspectiveNetID != 7 {
		t.Fatalf("backfilled perspective = %d, error = %v", item.PerspectiveNetID, err)
	}

	mismatch := []MatchCompletionPlayer{{
		UserID: userID, Result: "win", PerspectiveNetID: 8,
	}}
	if err := repository.CompleteMatch(ctx, matchID, mismatch); !errors.Is(err, apperr.ErrIdempotencyConflict) {
		t.Fatalf("mismatch error = %v, want conflict", err)
	}
}
```

### 8-6. 2·3·10인 distinct test 보강

기존 custom lobby integration loop의 player 생성 loop를 아래로 교체:

```go
			for index, userID := range userIDs[:count] {
				players = append(players, MatchCompletionPlayer{
					UserID: userID, Result: "win",
					PerspectiveNetID: int64(index + 1),
				})
			}
```

participant/library count 검사 뒤 아래를 추가:

```go
			for index, userID := range userIDs[:count] {
				item, err := repository.GetAuthorized(ctx, replayID, userID)
				if err != nil {
					t.Fatalf("participant %s authorization: %v", userID, err)
				}
				if item.PerspectiveNetID != int64(index+1) {
					t.Fatalf("participant %s perspective = %d, want %d",
						userID, item.PerspectiveNetID, index+1)
				}
				page, err := repository.ListAuthorized(
					ctx, userID, 10, nil, uuid.Nil)
				if err != nil {
					t.Fatalf("participant %s replay list: %v", userID, err)
				}
				if len(page) != 1 ||
					page[0].ID != replayID ||
					page[0].PerspectiveNetID != int64(index+1) {
					t.Fatalf(
						"participant %s list perspective = %+v, want replay %s perspective %d",
						userID, page, replayID, index+1)
				}
			}
```

이 검증은 1·2·3·4·10명 loop 전체에서 단건 `GetAuthorized`와 실제 `/replays/me`가 사용하는 `ListAuthorized`를 모두 확인하므로 3-account는 별도 수동 visual 요구 없이 권위 metadata 계약을 닫는다.

2차 보정의 독립 델타 재비평 전까지 source edit gate는 닫혀 있다.

## 9. 3차 델타 보정 — Replay cache file-contract probe

2차 재비평의 P1 자동 검증 요구와 P2 복구 절차를 수용한다. §8-1의 anonymous sidecar writer 계획은 폐기하고, account cache 열거와 publish가 같은 좁은 `CReplayLibrary` 계약을 사용하게 한다. 신규 파일은 만들지 않는다.

### 9-1. `ReplayLibrary` metadata 상태와 atomic publish

`Client/Public/Replay/ReplayLibrary.h`의 `ReplayListItem` 바로 아래에 추가:

```cpp
enum class eReplayPerspectiveMetadataStatus : u8_t
{
	Missing,
	Valid,
	Invalid,
};
```

`CReplayLibrary`의 `ListLocalDebugReplays()` 선언 바로 아래에 추가:

```cpp
	static eReplayPerspectiveMetadataStatus ReadReplayPerspectiveMetadata(
		const wstring_t& replayPath,
		u32_t& outPerspectiveNetId);
	static bool_t PublishAccountReplayCache(
		const wstring_t& temporaryReplayPath,
		const wstring_t& finalReplayPath,
		u32_t perspectiveNetId,
		std::string& outError);
```

`Client/Private/Replay/ReplayLibrary.cpp` include에는 `<fstream>`, `<limits>`를 추가한다. `ListReplayFiles()`의 `item.fileSizeBytes` 대입 아래에 추가:

```cpp
			if (!bLocalDebug)
			{
				CReplayLibrary::ReadReplayPerspectiveMetadata(
					item.path,
					item.perspectiveNetId);
			}
```

`GetLocalDebugReplayDirectory()` 바로 아래에 다음 두 함수 전체를 추가한다. sidecar final을 먼저 게시하고 WRPL final을 마지막에 게시한다. 신규 destination의 WRPL publish가 실패하면 방금 게시한 orphan sidecar도 제거한다. 동일 replay destination에 기존 WRPL이 있었으면 account와 replay ID가 같으므로 새 sidecar를 유지해도 perspective가 다른 파일과 결합되지 않는다.

```cpp
eReplayPerspectiveMetadataStatus CReplayLibrary::ReadReplayPerspectiveMetadata(
	const wstring_t& replayPath,
	u32_t& outPerspectiveNetId)
{
	outPerspectiveNetId = 0u;
	if (replayPath.empty())
		return eReplayPerspectiveMetadataStatus::Invalid;

	const std::filesystem::path sidecar = replayPath + L".perspective";
	std::error_code ec;
	const bool_t exists = std::filesystem::exists(sidecar, ec);
	if (ec)
		return eReplayPerspectiveMetadataStatus::Invalid;
	if (!exists)
		return eReplayPerspectiveMetadataStatus::Missing;

	std::ifstream input(sidecar, std::ios::binary);
	u64_t value = 0u;
	if (!(input >> value) || value == 0u ||
		value > (std::numeric_limits<u32_t>::max)())
	{
		return eReplayPerspectiveMetadataStatus::Invalid;
	}
	input >> std::ws;
	if (!input.eof())
		return eReplayPerspectiveMetadataStatus::Invalid;

	outPerspectiveNetId = static_cast<u32_t>(value);
	return eReplayPerspectiveMetadataStatus::Valid;
}

bool_t CReplayLibrary::PublishAccountReplayCache(
	const wstring_t& temporaryReplayPath,
	const wstring_t& finalReplayPath,
	u32_t perspectiveNetId,
	std::string& outError)
{
	outError.clear();
	const std::filesystem::path temporaryReplay(temporaryReplayPath);
	const std::filesystem::path finalReplay(finalReplayPath);
	if (perspectiveNetId == 0u || temporaryReplay.empty() ||
		finalReplay.empty() || temporaryReplay == finalReplay)
	{
		outError = "invalid replay cache publish metadata";
		return false;
	}

	std::error_code ec;
	if (!std::filesystem::is_regular_file(temporaryReplay, ec) || ec)
	{
		outError = "temporary replay cache file is unavailable";
		return false;
	}
	const bool_t finalReplayExisted = std::filesystem::exists(finalReplay, ec);
	if (ec)
	{
		outError = "replay cache destination is unavailable";
		return false;
	}

	const std::filesystem::path sidecar =
		finalReplay.wstring() + L".perspective";
	const std::filesystem::path temporarySidecar =
		sidecar.wstring() + L".part";
	std::filesystem::remove(temporarySidecar, ec);
	ec.clear();
	std::ofstream output(
		temporarySidecar,
		std::ios::binary | std::ios::trunc);
	output << perspectiveNetId << '\n';
	output.flush();
	const bool_t outputGood = output.good();
	output.close();
	if (!outputGood || !MoveFileExW(
		temporarySidecar.c_str(),
		sidecar.c_str(),
		MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
	{
		std::filesystem::remove(temporarySidecar, ec);
		outError = "replay perspective metadata write failed";
		return false;
	}

	if (!MoveFileExW(
		temporaryReplay.c_str(),
		finalReplay.c_str(),
		MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
	{
		if (!finalReplayExisted)
			std::filesystem::remove(sidecar, ec);
		outError = "downloaded replay cache publish failed";
		return false;
	}
	return true;
}
```

### 9-2. `ReplayClient`는 library publish 계약만 호출

§8-1의 `WriteReplayPerspectiveSidecar()` 전체 추가 계획은 폐기한다. `Client/Private/Network/Backend/ReplayClient.cpp::DownloadReplay()`의 integrity 검사와 publish block 전체를 아래로 교체한다:

```cpp
		if (cryptoStatus < 0 || !outputGood || !validPrefix ||
			formatVersion != static_cast<u16_t>(item.formatVersion) ||
			DigestToHex(digest) != item.checksumSha256)
		{
			std::filesystem::remove(temporary, fileError);
			result.error = "downloaded replay failed integrity validation";
			return result;
		}
		if (!CReplayLibrary::PublishAccountReplayCache(
			temporary.wstring(),
			destination.wstring(),
			item.perspectiveNetId,
			result.error))
		{
			std::filesystem::remove(temporary, fileError);
			return result;
		}

		result.success = true;
		result.localPath = destination.wstring();
		return result;
```

이 함수의 metadata guard에는 `item.perspectiveNetId == 0u`를 포함하고, `ReplayDownloadResult result{}` 직후 `result.perspectiveNetId = item.perspectiveNetId;`를 넣는 §8 계약은 유지한다.

### 9-3. `ReplayClientSmoke` offline file-contract probe

`Tools/Harness/ReplayClientSmoke.cpp` include에 `Replay/ReplayLibrary.h`, `<Windows.h>`, `<filesystem>`, `<fstream>`, `<vector>`를 추가한다. anonymous namespace의 `ReadEnvironment()` 아래에 다음 함수 전체를 추가한다:

```cpp
	const ReplayListItem* FindReplay(
		const std::vector<ReplayListItem>& items,
		const wstring_t& path)
	{
		for (const ReplayListItem& item : items)
		{
			if (item.path == path)
				return &item;
		}
		return nullptr;
	}

	bool RunPerspectiveFileContractProbe(std::string& outError)
	{
		const std::string accountUserID =
			"replay-smoke-" + std::to_string(GetCurrentProcessId());
		const std::filesystem::path accountDirectory(
			CReplayLibrary::GetAccountDataDirectory(accountUserID));
		const std::filesystem::path cacheDirectory(
			CReplayLibrary::GetAccountReplayCacheDirectory(accountUserID));
		if (accountDirectory.empty() || cacheDirectory.empty())
		{
			outError = "probe account directory is unavailable";
			return false;
		}

		std::error_code ec;
		std::filesystem::remove_all(accountDirectory, ec);
		std::filesystem::create_directories(cacheDirectory, ec);
		if (ec)
		{
			outError = "probe cache directory create failed";
			return false;
		}
		auto cleanup = [&]()
		{
			std::error_code ignored;
			std::filesystem::remove_all(accountDirectory, ignored);
		};
		auto fail = [&](const char* message)
		{
			outError = message;
			cleanup();
			return false;
		};
		auto writeText = [&](const std::filesystem::path& path,
			const char* text)
		{
			std::ofstream output(path, std::ios::binary | std::ios::trunc);
			output << text;
			output.flush();
			return output.good();
		};

		const std::filesystem::path validFinal = cacheDirectory / L"valid.wrpl";
		const std::filesystem::path validPart = validFinal.wstring() + L".part";
		if (!writeText(validPart, "WRPL-probe"))
			return fail("probe replay write failed");
		std::string publishError;
		if (!CReplayLibrary::PublishAccountReplayCache(
			validPart.wstring(), validFinal.wstring(), 7u, publishError))
		{
			outError = publishError;
			cleanup();
			return false;
		}
		u32_t perspectiveNetId = 0u;
		if (CReplayLibrary::ReadReplayPerspectiveMetadata(
			validFinal.wstring(), perspectiveNetId) !=
			eReplayPerspectiveMetadataStatus::Valid || perspectiveNetId != 7u)
		{
			return fail("valid perspective metadata was not preserved");
		}

		const std::filesystem::path missing = cacheDirectory / L"missing.wrpl";
		const std::filesystem::path malformed = cacheDirectory / L"malformed.wrpl";
		const std::filesystem::path overflow = cacheDirectory / L"overflow.wrpl";
		if (!writeText(missing, "WRPL-probe") ||
			!writeText(malformed, "WRPL-probe") ||
			!writeText(overflow, "WRPL-probe") ||
			!writeText(malformed.wstring() + L".perspective", "broken\n") ||
			!writeText(overflow.wstring() + L".perspective", "4294967296\n"))
		{
			return fail("invalid metadata fixture write failed");
		}
		if (CReplayLibrary::ReadReplayPerspectiveMetadata(
			missing.wstring(), perspectiveNetId) !=
			eReplayPerspectiveMetadataStatus::Missing || perspectiveNetId != 0u)
		{
			return fail("missing metadata did not fail closed");
		}
		if (CReplayLibrary::ReadReplayPerspectiveMetadata(
			malformed.wstring(), perspectiveNetId) !=
			eReplayPerspectiveMetadataStatus::Invalid || perspectiveNetId != 0u)
		{
			return fail("malformed metadata did not fail closed");
		}
		if (CReplayLibrary::ReadReplayPerspectiveMetadata(
			overflow.wstring(), perspectiveNetId) !=
			eReplayPerspectiveMetadataStatus::Invalid || perspectiveNetId != 0u)
		{
			return fail("overflow metadata did not fail closed");
		}

		const std::vector<ReplayListItem> items =
			CReplayLibrary::ListAccountReplayCache(accountUserID);
		const ReplayListItem* validItem = FindReplay(items, validFinal.wstring());
		const ReplayListItem* missingItem = FindReplay(items, missing.wstring());
		const ReplayListItem* malformedItem = FindReplay(items, malformed.wstring());
		const ReplayListItem* overflowItem = FindReplay(items, overflow.wstring());
		if (!validItem || validItem->perspectiveNetId != 7u ||
			!missingItem || missingItem->perspectiveNetId != 0u ||
			!malformedItem || malformedItem->perspectiveNetId != 0u ||
			!overflowItem || overflowItem->perspectiveNetId != 0u)
		{
			return fail("account replay listing violated perspective contract");
		}

		const std::filesystem::path sidecarFailurePart =
			cacheDirectory / L"sidecar-failure.part";
		const std::filesystem::path unpublished =
			cacheDirectory / L"absent-parent" / L"unpublished.wrpl";
		if (!writeText(sidecarFailurePart, "WRPL-probe") ||
			CReplayLibrary::PublishAccountReplayCache(
				sidecarFailurePart.wstring(),
				unpublished.wstring(), 9u, publishError) ||
			std::filesystem::exists(unpublished, ec))
		{
			return fail("sidecar failure exposed a replay cache file");
		}

		cleanup();
		outError.clear();
		return true;
	}
```

`main()` signature를 `int main(int argc, char** argv)`로 교체하고 첫 줄에 다음을 추가한다:

```cpp
	std::string fileContractError;
	if (!RunPerspectiveFileContractProbe(fileContractError))
	{
		std::cerr << "replay_perspective_contract=fail reason="
			<< fileContractError << "\n";
		return 3;
	}
	if (argc == 2 && std::string(argv[1]) == "--perspective-contract-only")
	{
		std::cout << "replay_perspective_contract=pass\n";
		return 0;
	}
```

live smoke의 `localPath` 선언 아래에 추가:

```cpp
	u32_t expectedPerspectiveNetId = 0u;
```

selected replay null 검증 직후, `DownloadMine()` 호출 전에 추가:

```cpp
			if (selected->perspectiveNetId == 0u)
			{
				error = "cloud replay perspective is unavailable";
				downloadCompleted = true;
				return;
			}
			expectedPerspectiveNetId = selected->perspectiveNetId;
```

download callback의 `success = result.success;`를 아래로 교체한다:

```cpp
					success = result.success &&
						result.perspectiveNetId == expectedPerspectiveNetId;
```

timeout 성공 판정 직전에 다음 account-cache 동일성 검증을 추가한다:

```cpp
	if (success)
	{
		const std::vector<ReplayListItem> cached =
			CReplayLibrary::ListAccountReplayCache(userID);
		const ReplayListItem* cachedItem = FindReplay(cached, localPath);
		if (!cachedItem ||
			cachedItem->perspectiveNetId != expectedPerspectiveNetId)
		{
			success = false;
			error = "cloud and account-cache perspectives differ";
		}
	}
```

offline 회귀 명령은 다음과 같다.

```powershell
& msbuild C:/Users/user/Desktop/Winters/Tools/Harness/ReplayClientSmoke.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /m:1
& C:/Users/user/Desktop/Winters/Tools/Harness/Bin/Release/ReplayClientSmoke.exe --perspective-contract-only
```

### 9-4. 기존 match/cache 복구 절차 명시

§7-11의 5번 뒤에 다음 순서를 적용한다.

6. 사용자 계정↔champion 확인 후 DB `replay_net_id`를 보강한다.
7. 기존 `%LOCALAPPDATA%/Winters/Accounts/<user>/ReplayCache`의 sidecar 없는 파일은 임의 champion fallback으로 열지 않는다.
8. 각 계정에서 Cloud `다시보기`를 한 번 실행해 동일 canonical WRPL과 계정별 non-zero `.perspective` sidecar를 재다운로드한다.
9. Cloud 즉시 재생과 재다운로드 뒤 account cache 재생이 같은 `perspectiveNetId`를 전달하는지 live smoke/log로 검증한다.

이 3차 보정의 최신 문서에 대해 독립 델타 재비평 P0/P1 0을 받은 뒤에만 source edit gate를 연다.

## 10. 최종 독립 델타 재비평 판정과 disposition

- 최종 판정: `P0 0 / P1 0 / P2 0 — PASS`.
- 수용: sidecar final을 WRPL final보다 먼저 게시하고 신규 WRPL publish 실패 때 orphan sidecar를 정리한다.
- 수용: account cache의 missing·zero·malformed·overflow metadata는 perspective 0으로 fail-closed하고 UI와 Backend 양쪽에서 재생을 막는다. 최소 NetId fallback은 local/debug replay에만 허용한다.
- 수용: `ReplayClientSmoke --perspective-contract-only`가 file publish/listing 실패 계약을 검증하고 live smoke가 Cloud list→download result→account cache의 동일 perspective를 검증한다.
- 수용: completed retry의 legacy 0 보강·동일 retry·mismatch conflict·non-zero no-erase와 1·2·3·4·10명 `GetAuthorized`/`ListAuthorized` perspective assertion을 exact test로 둔다.
- 수용: 기존 sidecar 없는 cache는 DB 보강 뒤 Cloud 재다운로드로 복구한다.
- 최종 재비평에서 rejected/held P0/P1은 없다.

따라서 `.md/계획서작성규칙.md` §0 pass line을 충족했고 source edit gate를 연다.
