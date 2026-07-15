param(
    [string]$NypcRoot = "C:/Users/user/Desktop/NYPC",
    [string]$WintersRoot = "",
    [switch]$BuildCpp,
    [switch]$FullBuild,
    [int]$SimTicks = 1800,
    [UInt64]$Seed = 42
)

$ErrorActionPreference = "Stop"

if (-not $WintersRoot) {
    $WintersRoot = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
}

function Assert-LastExitCode([string]$Step) {
    if ($LASTEXITCODE -ne 0) {
        throw "$Step failed with exit code $LASTEXITCODE."
    }
}

Push-Location $WintersRoot
try {
    & python -B Tools/AIResearch/ValidateBridgeManifest.py `
        --manifest Tools/AIResearch/bridge_manifest.json `
        --nypc-root $NypcRoot `
        --winters-root $WintersRoot
    Assert-LastExitCode "Bridge manifest validation"

    & python -B -m unittest discover `
        -s Tools/AIResearch/tests `
        -p "test_*.py" `
        -v
    Assert-LastExitCode "AI research Python tests"

    $bcSmokeRoot = Join-Path ([System.IO.Path]::GetTempPath()) `
        ("winters-s022-bc-" + [Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Path $bcSmokeRoot | Out-Null
    try {
        $firstDataset = Join-Path $bcSmokeRoot "imitation-a.jsonl"
        $secondDataset = Join-Path $bcSmokeRoot "imitation-b.jsonl"
        $materializeCommon = @(
            "-B",
            "Tools/AIResearch/MaterializeImitationDataset.py",
            "--input", "Tools/AIResearch/fixtures/imitation_ranking_v1_golden.jsonl",
            "--corrections", "Tools/AIResearch/fixtures/ai_decision_correction_sidecar_v1_golden.json"
        )

        & python @materializeCommon --output $firstDataset
        Assert-LastExitCode "ImitationDecisionV1 first materialization"
        & python @materializeCommon --output $secondDataset
        Assert-LastExitCode "ImitationDecisionV1 second materialization"

        $firstDatasetHash = (Get-FileHash -Algorithm SHA256 $firstDataset).Hash
        $secondDatasetHash = (Get-FileHash -Algorithm SHA256 $secondDataset).Hash
        if ($firstDatasetHash -ne $secondDatasetHash) {
            throw "ImitationDecisionV1 canonical dataset SHA mismatch."
        }
        Write-Host (
            "[AIResearch] ImitationDecisionV1 deterministic hash: " +
            $firstDatasetHash
        )

        $firstReport = Join-Path $bcSmokeRoot "policy-a.json"
        $firstBinary = Join-Path $bcSmokeRoot "policy-a.wbc"
        $secondReport = Join-Path $bcSmokeRoot "policy-b.json"
        $secondBinary = Join-Path $bcSmokeRoot "policy-b.wbc"
        $bcCommon = @(
            "-B",
            "Tools/AIResearch/TrainImitationRankingBaseline.py",
            "--backend", "pytorch-masked-bc",
            "--input", "Tools/AIResearch/fixtures/imitation_ranking_v1_golden.jsonl",
            "--policy-id", "s022-contract-smoke",
            "--policy-revision", "8",
            "--minimum-groups", "8",
            "--fixture-contract"
        )

        & python @bcCommon `
            --output $firstReport `
            --runtime-output $firstBinary
        Assert-LastExitCode "PyTorch masked BC first deterministic smoke"

        & python @bcCommon `
            --output $secondReport `
            --runtime-output $secondBinary
        Assert-LastExitCode "PyTorch masked BC second deterministic smoke"

        $firstReportHash = (Get-FileHash -Algorithm SHA256 $firstReport).Hash
        $secondReportHash = (Get-FileHash -Algorithm SHA256 $secondReport).Hash
        $firstBinaryHash = (Get-FileHash -Algorithm SHA256 $firstBinary).Hash
        $secondBinaryHash = (Get-FileHash -Algorithm SHA256 $secondBinary).Hash
        if ($firstReportHash -ne $secondReportHash) {
            throw "PyTorch masked BC canonical report SHA mismatch."
        }
        if ($firstBinaryHash -ne $secondBinaryHash) {
            throw "PyTorch masked BC runtime binary SHA mismatch."
        }
        Write-Host (
            "[AIResearch] PyTorch masked BC deterministic hashes: " +
            "report=$firstReportHash binary=$firstBinaryHash"
        )
    }
    finally {
        Remove-Item -LiteralPath $bcSmokeRoot -Recurse -Force `
            -ErrorAction SilentlyContinue
    }

    & python -B Tools/AIResearch/ValidateAiEpisode.py `
        --input Tools/AIResearch/fixtures/ai_episode_v1_golden.jsonl `
        --promotion
    Assert-LastExitCode "AiEpisodeV1 promotion fixture"

    & powershell -NoProfile -ExecutionPolicy Bypass `
        -File Tools/AIResearch/tests/RunChampionAIResearchTypesProbe.ps1
    Assert-LastExitCode "AI research C++ contract probe"

    & powershell -NoProfile -ExecutionPolicy Bypass `
        -File Tools/AIResearch/tests/RunAiEpisodeV1CodecProbe.ps1
    Assert-LastExitCode "AiEpisodeV1 C++/Python codec probe"

    & powershell -NoProfile -ExecutionPolicy Bypass `
        -File Tools/AIResearch/tests/RunChampionAIInfluenceMapProbe.ps1
    Assert-LastExitCode "Champion AI influence map probe"

    & powershell -NoProfile -ExecutionPolicy Bypass `
        -File Tools/Harness/RunReplayCommandContractProbe.ps1
    Assert-LastExitCode "Replay command journal contract probe"

    & powershell -NoProfile -ExecutionPolicy Bypass `
        -File Tools/Harness/RunTimelineRebaseContractProbe.ps1
    Assert-LastExitCode "Timeline rebase contract probe"

    & powershell -NoProfile -ExecutionPolicy Bypass `
        -File Tools/Harness/Check-SharedBoundary.ps1
    Assert-LastExitCode "Shared boundary validation"

    if (-not $BuildCpp -and -not $FullBuild) {
        Write-Host "[AIResearch] PASS: contract validation (C++ builds skipped)."
        return
    }

    $vsWhere = Join-Path ${env:ProgramFiles(x86)} `
        "Microsoft Visual Studio/Installer/vswhere.exe"
    if (-not (Test-Path -LiteralPath $vsWhere)) {
        throw "vswhere.exe was not found: $vsWhere"
    }

    $msBuild = & $vsWhere -latest -products * `
        -requires Microsoft.Component.MSBuild `
        -find "MSBuild/**/Bin/MSBuild.exe" | Select-Object -First 1
    if (-not $msBuild -or -not (Test-Path -LiteralPath $msBuild)) {
        throw "MSBuild.exe was not found by vswhere."
    }

    & $msBuild Shared/GameSim/Include/GameSim.vcxproj `
        /t:Build /p:Configuration=Debug /p:Platform=x64 `
        /m:1 /nr:false /v:minimal
    Assert-LastExitCode "GameSim Debug x64 build"

    & $msBuild Tools/SimLab/SimLab.vcxproj `
        /t:Build /p:Configuration=Debug /p:Platform=x64 `
        /m:1 /nr:false /v:minimal
    Assert-LastExitCode "SimLab Debug x64 build"

    & powershell -NoProfile -ExecutionPolicy Bypass `
        -File Tools/AIResearch/tests/RunLiveAiEpisodeSmokeProbe.ps1 `
        -WintersRoot $WintersRoot `
        -SimLabPath (Join-Path $WintersRoot "Tools/Bin/Debug/SimLab.exe") `
        -Seed $Seed
    Assert-LastExitCode "Live AiEpisodeV1 smoke probe"

    & powershell -NoProfile -ExecutionPolicy Bypass `
        -File Tools/AIResearch/tests/RunActiveAiPolicyEpisodeProbe.ps1 `
        -WintersRoot $WintersRoot `
        -SimLabPath (Join-Path $WintersRoot "Tools/Bin/Debug/SimLab.exe") `
        -Seed $Seed
    Assert-LastExitCode "Active learned-policy episode probe"

    & Tools/Bin/Debug/SimLab.exe $SimTicks $Seed
    Assert-LastExitCode "SimLab deterministic run"

    if ($FullBuild) {
        & $msBuild Server/Include/Server.vcxproj `
            /t:Build /p:Configuration=Debug /p:Platform=x64 `
            /m:1 /nr:false /v:minimal
        Assert-LastExitCode "Server Debug x64 build"

        & $msBuild Client/Include/Client.vcxproj `
            /t:Build /p:Configuration=Debug /p:Platform=x64 `
            /m:1 /nr:false /v:minimal
        Assert-LastExitCode "Client Debug x64 build"
    }

    Write-Host "[AIResearch] PASS: requested validation matrix."
}
finally {
    Pop-Location
}
