# run_profile_session.ps1 — 빌드 → 리플레이 무인 캡처 → 분석 → 원장 append 원커맨드.
# 사용:  powershell -File Tools\Profiler\run_profile_session.ps1 [-Label "lazy-pose-after"] [-SkipBuild]
# 전제:  Release 구성에 WINTERS_PROFILING 정의(2026-07-17 이후), 리플레이 파일이 repo 루트 Replay\ 에 존재.
param(
    [string]$Configuration = "Release",
    [string]$ReplayPath = "Replay\room1_tick1_1393.wrpl",
    [int]$RunSeconds = 40,
    [int]$AnalyzeTargetFps = 144,
    [string]$Label = "",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location $Root

if (-not (Test-Path (Join-Path $Root $ReplayPath))) {
    throw "리플레이 파일 없음: $ReplayPath"
}

if (-not $SkipBuild) {
    $VsWhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $VsWhere)) { throw "vswhere.exe not found" }
    $MSBuild = & $VsWhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" |
        Select-Object -First 1
    if (-not $MSBuild) { throw "MSBuild.exe not found" }

    foreach ($Project in @("Engine\Include\Engine.vcxproj", "Client\Include\Client.vcxproj")) {
        Write-Host "[build] $Project ($Configuration)"
        & $MSBuild (Join-Path $Root $Project) /t:Build "/p:Configuration=$Configuration" /p:Platform=x64 /m:1 /v:minimal
        if ($LASTEXITCODE -ne 0) { throw "빌드 실패: $Project" }
    }
}

# 무인 캡처 실행 (조건 고정: uncapped, no-vsync, dx11 — PLAYBOOK §0)
$env:WINTERS_PROFILE_HISTORY_FRAMES = '6000'
$Exe = Join-Path $Root "Client\Bin\$Configuration\WintersGame.exe"
if (-not (Test-Path $Exe)) { throw "클라이언트 실행 파일 없음: $Exe" }

Write-Host "[run] $ReplayPath / $RunSeconds s / uncapped"
$ReplayArg = "--replay=" + ($ReplayPath -replace '\\', '/')
$p = Start-Process -FilePath $Exe -ArgumentList $ReplayArg, "--run-seconds=$RunSeconds", "--profile-capture-on-exit", "--uncapped", "--no-vsync", "--rhi=dx11" -PassThru -WorkingDirectory $Root
$exited = $p.WaitForExit(($RunSeconds + 90) * 1000)
if (-not $exited) { $p.Kill(); throw "클라이언트가 제한 시간 내 종료되지 않음" }
if ($p.ExitCode -ne 0) { throw "클라이언트 비정상 종료: exit=$($p.ExitCode)" }

# 최신 캡처 선택 + 분석
$Capture = Get-ChildItem (Join-Path $Root "Profiles\profiler_*.json") | Sort-Object LastWriteTime | Select-Object -Last 1
if (-not $Capture) { throw "캡처 파일 없음 (Profiles\)" }
Write-Host "[capture] $($Capture.Name)"

$AnalysisRaw = & python (Join-Path $Root "Tools\Profiler\analyze_profiler_capture.py") $Capture.FullName --target-fps $AnalyzeTargetFps
$Analysis = $AnalysisRaw | ConvertFrom-Json
$AnalysisPath = $Capture.FullName -replace '\.json$', '_analysis.json'
$AnalysisRaw | Out-File -FilePath $AnalysisPath -Encoding utf8

$W = $Analysis.workFrameMs
$G = $Analysis.gpuFrameMs
$Ov = $Analysis.profilerPreviousEndFrameMs
$Drops = $Analysis.drops
$Fps = [math]::Round($Analysis.effectiveFps, 1)
$DropsTotal = $Drops.scopeStats + $Drops.counters + $Drops.rawEvents

# 마지막 프레임의 Draw::Total 게이지
$CaptureJson = Get-Content $Capture.FullName -Raw | ConvertFrom-Json
$LastFrame = $CaptureJson.frames[$CaptureJson.frames.Count - 1]
$DrawTotal = ($LastFrame.counters | Where-Object { $_.name -eq 'Draw::Total' } | Select-Object -First 1).value

# 원장 append
$Stamp = Get-Date -Format "yyyy-MM-dd HH:mm"
$Scenario = (Split-Path $ReplayPath -Leaf) + " uncapped no-vsync ${RunSeconds}s"
$Row = "| $Stamp | $Configuration | $Scenario | $($Analysis.frameCount) | $([math]::Round($W.median,3))ms | $([math]::Round($W.p95,3))ms | $([math]::Round($W.p99,3))ms | $Fps | $([math]::Round($G.median,3))ms | $DrawTotal | $([math]::Round($Ov.p95,3))ms | $DropsTotal | $Label ``$($Capture.Name)`` |"
Add-Content -Path (Join-Path $Root ".md\plan\performance\PROFILING_LEDGER.md") -Value $Row -Encoding utf8

Write-Host ""
Write-Host "=== LEDGER ROW ==="
Write-Host $Row
Write-Host ""
Write-Host "=== 이력서 스니펫 원료 (측정 방법과 세트로만 사용) ==="
Write-Host "Frame median $([math]::Round($W.median,3))ms / p99 $([math]::Round($W.p99,3))ms / 유효 $Fps FPS"
Write-Host "GPU median $([math]::Round($G.median,3))ms / Draw::Total $DrawTotal / 오버헤드 p95 $([math]::Round($Ov.p95,3))ms / 드롭 $DropsTotal"
Write-Host "조건: $Configuration + WINTERS_PROFILING, $Scenario, 1280x720"
