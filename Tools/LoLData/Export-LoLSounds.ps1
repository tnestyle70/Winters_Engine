[CmdletBinding()]
param(
    [string]$Root = "",
    [string]$LeagueRoot = "C:\Riot Games\League of Legends",
    [string]$StageRoot = "Tools\Bin\Intermediate\LoLSoundBanks",
    [string]$OutputRoot = "Client\Bin\Resource\Sound\LoL",
    [switch]$Clean,
    [switch]$SkipMapBanks,
    [switch]$AllMapAudioBanks,
    [string[]]$MapBankNamePatterns = @(
        "env_map*_audio.bnk",
        "npc_global_*_sfx_audio.bnk",
        "npc_map11*_sfx_audio.bnk",
        "npc_map12*_sfx_audio.bnk",
        "npc_map14*_sfx_audio.bnk"
    )
)

$ErrorActionPreference = "Stop"

if ($Root.Length -eq 0) {
    $Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
}

function Resolve-RepoPath {
    param([string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }

    return Join-Path $Root $Path
}

function Assert-PathUnderRoot {
    param(
        [string]$Path,
        [string]$RootPath
    )

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $fullRoot = [System.IO.Path]::GetFullPath($RootPath)
    if (-not $fullRoot.EndsWith([System.IO.Path]::DirectorySeparatorChar)) {
        $fullRoot += [System.IO.Path]::DirectorySeparatorChar
    }

    if (-not $fullPath.StartsWith($fullRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clean a path outside the repo: $fullPath"
    }
}

function Write-JsonFile {
    param(
        [string]$Path,
        [object]$Value,
        [int]$Depth = 24
    )

    $parent = Split-Path -Parent $Path
    if ($parent.Length -gt 0) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }

    $json = $Value | ConvertTo-Json -Depth $Depth
    Set-Content -LiteralPath $Path -Value $json -Encoding UTF8
}

function Convert-AudioBank {
    param(
        [string]$BankPath,
        [string]$OutputDir,
        [string]$Stem,
        [string]$VgmstreamPath,
        [switch]$Required
    )

    if (-not (Test-Path -LiteralPath $BankPath)) {
        return [ordered]@{
            bank = $BankPath
            stem = $Stem
            status = "missing"
            wavCount = 0
        }
    }

    New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

    $logPath = Join-Path $OutputDir "$Stem.vgmstream.log"
    $outputPattern = Join-Path $OutputDir "${Stem}_?s_?n.wav"
    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        $captured = & $VgmstreamPath -i -S 0 -o $outputPattern $BankPath 2>&1
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }

    $captured | Set-Content -LiteralPath $logPath -Encoding UTF8

    if ($exitCode -ne 0) {
        if ($Required) {
            throw "vgmstream failed with exit code $exitCode for $BankPath. See $logPath"
        }

        return [ordered]@{
            bank = $BankPath
            output = $OutputDir
            stem = $Stem
            status = "skipped"
            reason = "vgmstream-exit-$exitCode"
            wavCount = 0
            log = $logPath
        }
    }

    $wavCount = @(Get-ChildItem -LiteralPath $OutputDir -Filter "${Stem}_*.wav" -File).Count
    return [ordered]@{
        bank = $BankPath
        output = $OutputDir
        stem = $Stem
        status = "converted"
        wavCount = $wavCount
        log = $logPath
    }
}

function Test-NamePattern {
    param(
        [string]$Name,
        [string[]]$Patterns
    )

    foreach ($pattern in $Patterns) {
        if ($Name -like $pattern) {
            return $true
        }
    }

    return $false
}

$rootResolved = (Resolve-Path $Root).Path
$leagueRootResolved = (Resolve-Path $LeagueRoot).Path
$stageRootResolved = Resolve-RepoPath $StageRoot
$outputRootResolved = Resolve-RepoPath $OutputRoot
$probeProjectPath = Resolve-RepoPath "Tools\External\LeagueToolkitProbe\LeagueToolkitProbe.csproj"
$probeExePath = Resolve-RepoPath "Tools\External\LeagueToolkitProbe\bin\Debug\net10.0\LeagueToolkitProbe.exe"
$vgmstreamPath = Resolve-RepoPath "Tools\External\vgmstream\vgmstream-cli.exe"

if (-not (Test-Path -LiteralPath $vgmstreamPath)) {
    throw "Missing vgmstream CLI: $vgmstreamPath"
}

if ($Clean) {
    foreach ($pathToClean in @($stageRootResolved, $outputRootResolved)) {
        if (Test-Path -LiteralPath $pathToClean) {
            Assert-PathUnderRoot -Path $pathToClean -RootPath $rootResolved
            Remove-Item -LiteralPath $pathToClean -Recurse -Force
        }
    }
}

New-Item -ItemType Directory -Force -Path $stageRootResolved | Out-Null
New-Item -ItemType Directory -Force -Path $outputRootResolved | Out-Null

& dotnet build $probeProjectPath -v:minimal | Out-Host
if ($LASTEXITCODE -ne 0) {
    throw "LeagueToolkitProbe build failed."
}

$champions = @(
    @{ Name = "Annie"; Asset = "annie" },
    @{ Name = "Ashe"; Asset = "ashe" },
    @{ Name = "Ezreal"; Asset = "ezreal" },
    @{ Name = "Fiora"; Asset = "fiora" },
    @{ Name = "Garen"; Asset = "garen" },
    @{ Name = "Irelia"; Asset = "irelia" },
    @{ Name = "Jax"; Asset = "jax" },
    @{ Name = "Kalista"; Asset = "kalista" },
    @{ Name = "Kindred"; Asset = "kindred" },
    @{ Name = "LeeSin"; Asset = "leesin" },
    @{ Name = "MasterYi"; Asset = "masteryi" },
    @{ Name = "Riven"; Asset = "riven" },
    @{ Name = "Sylas"; Asset = "sylas" },
    @{ Name = "Viego"; Asset = "viego" },
    @{ Name = "Yasuo"; Asset = "yasuo" },
    @{ Name = "Yone"; Asset = "yone" },
    @{ Name = "Zed"; Asset = "zed" }
)

$championResults = New-Object System.Collections.Generic.List[object]
$mapResults = New-Object System.Collections.Generic.List[object]
$missing = New-Object System.Collections.Generic.List[object]

foreach ($champion in $champions) {
    $name = [string]$champion.Name
    $asset = [string]$champion.Asset
    $wadPath = Join-Path $leagueRootResolved "Game\DATA\FINAL\Champions\$name.wad.client"
    $stageChampionRoot = Join-Path $stageRootResolved "Champions\$name"
    $outputChampionRoot = Join-Path $outputRootResolved "Champions\$name"
    $audioBankRelative = "assets/sounds/wwise2016/sfx/characters/$asset/skins/base/${asset}_base_sfx_audio.bnk"
    $eventsBankRelative = "assets/sounds/wwise2016/sfx/characters/$asset/skins/base/${asset}_base_sfx_events.bnk"

    if (-not (Test-Path -LiteralPath $wadPath)) {
        $missing.Add([ordered]@{ type = "champion-wad"; champion = $name; path = $wadPath })
        continue
    }

    New-Item -ItemType Directory -Force -Path $stageChampionRoot | Out-Null
    $extractLogPath = Join-Path $stageChampionRoot "$name.extract.log"
    $extractOutput = & $probeExePath wad-extract $wadPath $stageChampionRoot $audioBankRelative $eventsBankRelative 2>&1
    $exitCode = $LASTEXITCODE
    $extractOutput | Set-Content -LiteralPath $extractLogPath -Encoding UTF8
    if ($exitCode -ne 0) {
        throw "wad-extract failed with exit code $exitCode for $wadPath. See $extractLogPath"
    }

    $audioBankPath = Join-Path $stageChampionRoot $audioBankRelative
    $result = Convert-AudioBank -BankPath $audioBankPath -OutputDir $outputChampionRoot -Stem "${asset}_base_sfx" -VgmstreamPath $vgmstreamPath -Required
    $result["champion"] = $name
    $result["extractLog"] = $extractLogPath
    $championResults.Add($result)

    if ($result["status"] -ne "converted") {
        $missing.Add([ordered]@{ type = "champion-audio-bank"; champion = $name; path = $audioBankPath })
    }
}

if (-not $SkipMapBanks) {
    $mapSoundRoots = @(
        @{ Name = "MAP"; Path = Resolve-RepoPath "Client\Bin\Resource\Texture\MAP\assets\sounds" },
        @{ Name = "MAP12"; Path = Resolve-RepoPath "Client\Bin\Resource\Texture\MAP\MAP12\assets\sounds" }
    )

    foreach ($mapRoot in $mapSoundRoots) {
        if (-not (Test-Path -LiteralPath $mapRoot.Path)) {
            $missing.Add([ordered]@{ type = "map-sound-root"; map = $mapRoot.Name; path = $mapRoot.Path })
            continue
        }

        $banks = Get-ChildItem -LiteralPath $mapRoot.Path -Recurse -File -Filter "*_audio.bnk"
        foreach ($bank in $banks) {
            if ((-not $AllMapAudioBanks) -and (-not (Test-NamePattern -Name $bank.Name -Patterns $MapBankNamePatterns))) {
                continue
            }

            $stem = [System.IO.Path]::GetFileNameWithoutExtension($bank.Name)
            $outputMapRoot = Join-Path $outputRootResolved "Map\$($mapRoot.Name)"
            $result = Convert-AudioBank -BankPath $bank.FullName -OutputDir $outputMapRoot -Stem $stem -VgmstreamPath $vgmstreamPath
            $result["map"] = $mapRoot.Name
            $mapResults.Add($result)
        }
    }
}

$wavCount = @(Get-ChildItem -LiteralPath $outputRootResolved -Recurse -File -Filter "*.wav").Count
$skippedCount = 0
foreach ($result in $championResults) {
    if ($result["status"] -eq "skipped") {
        ++$skippedCount
    }
}
foreach ($result in $mapResults) {
    if ($result["status"] -eq "skipped") {
        ++$skippedCount
    }
}

$report = [ordered]@{
    generatedAtUtc = (Get-Date).ToUniversalTime().ToString("o")
    root = $rootResolved
    leagueRoot = $leagueRootResolved
    stageRoot = $stageRootResolved
    outputRoot = $outputRootResolved
    clean = [bool]$Clean
    championCount = $champions.Count
    championBankCount = $championResults.Count
    mapBankCount = $mapResults.Count
    wavCount = $wavCount
    skippedCount = $skippedCount
    missingCount = $missing.Count
    missing = $missing
    championResults = $championResults
    mapResults = $mapResults
}

$reportPath = Join-Path $outputRootResolved "_LoLSoundExportReport.json"
Write-JsonFile -Path $reportPath -Value $report
Write-Output ($report | ConvertTo-Json -Depth 24)

if ($missing.Count -ne 0) {
    Write-Warning "Sound export completed with $($missing.Count) missing item(s). See $reportPath"
}
