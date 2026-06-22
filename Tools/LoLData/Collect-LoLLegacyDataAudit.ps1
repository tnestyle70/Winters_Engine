[CmdletBinding()]
param(
    [string]$Root = "",
    [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"

if ($Root.Length -eq 0) {
    $Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
}

function Invoke-RgLines {
    param(
        [string]$Pattern,
        [string[]]$Paths
    )

    $args = @("-n", $Pattern)
    foreach ($path in $Paths) {
        $args += (Join-Path $Root $path)
    }
    $args += @("-g", "!Client/Bin/**", "-g", "!Client/Intermediate/**", "-g", "!Client/Temp/**")

    $result = & rg @args 2>$null
    if ($LASTEXITCODE -gt 1) {
        throw "rg failed for pattern: $Pattern"
    }
    return @($result)
}

function Invoke-RgFiles {
    param(
        [string]$Pattern,
        [string[]]$Paths
    )

    $args = @("-l", $Pattern)
    foreach ($path in $Paths) {
        $args += (Join-Path $Root $path)
    }
    $args += @("-g", "!Client/Bin/**", "-g", "!Client/Intermediate/**", "-g", "!Client/Temp/**")

    $result = & rg @args 2>$null
    if ($LASTEXITCODE -gt 1) {
        throw "rg failed for pattern: $Pattern"
    }
    return @($result | ForEach-Object { Resolve-Path $_ | ForEach-Object { $_.Path } })
}

$championJsonPath = Join-Path $Root "Data\Gameplay\ChampionGameData\champions.json"
$championJson = Get-Content -Raw -Path $championJsonPath | ConvertFrom-Json

$stageCount = 0
$animPlaySpeedCount = 0
$castFrameCount = 0
$recoveryFrameCount = 0

foreach ($champion in $championJson.champions) {
    foreach ($skill in $champion.skills) {
        foreach ($stage in $skill.stages) {
            ++$stageCount
            if ($null -ne $stage.animPlaySpeed) { ++$animPlaySpeedCount }
            if ($null -ne $stage.castFrame) { ++$castFrameCount }
            if ($null -ne $stage.recoveryFrame) { ++$recoveryFrameCount }
        }
    }
}

$skillRegistrationFiles = Invoke-RgFiles `
    -Pattern "SkillDef s|FindSkillDef|CSkillRegistry::Instance\(\)\.Add" `
    -Paths @("Client\Private\GameObject\Champion")

$championRegistrationFiles = Invoke-RgFiles `
    -Pattern "ChampionDef cd|CChampionRegistry::Instance\(\)\.Add" `
    -Paths @("Client\Private\GameObject\Champion")

$legacySkillLines = Invoke-RgLines `
    -Pattern "\bSkillDef\b|s_SkillTable|g_SkillTable|FindSkillDef|CSkillRegistry|BuildSkillVisualData|BuildSkillGameAtomBundle" `
    -Paths @("Client", "Shared", "Server")

$legacyChampionLines = Invoke-RgLines `
    -Pattern "\bChampionDef\b|s_ChampionTable|FindChampionDef|CChampionRegistry|ChampionCatalog" `
    -Paths @("Client", "Shared", "Server")

$visualLeakLines = Invoke-RgLines `
    -Pattern "visualYawOffset|animPlaySpeed|castFrame|recoveryFrame|ResolveVisualYawOffset|summonerSpells" `
    -Paths @("Data", "Client", "Shared", "Server", "Tools")

$serverObjectLines = Invoke-RgLines `
    -Pattern "ResolveStageStructureMaxHp|ResolveStageJungle|ServerMinionTuning::kWave|kWaveIntervalTicks|kInitialWaveDelayTicks|AssignDefaultBotSkillRanks|gold.amount|LethalTempo" `
    -Paths @("Server", "Shared", "Tools")

$projectileVisualLines = Invoke-RgLines `
    -Pattern "ProjectileVisualCatalog|ProjectileVisualDesc" `
    -Paths @("Client", "Shared", "Server")

$report = [ordered]@{
    generatedAtUtc = (Get-Date).ToUniversalTime().ToString("o")
    root = (Resolve-Path $Root).Path
    championGameData = [ordered]@{
        path = $championJsonPath
        championCount = @($championJson.champions).Count
        summonerSpellCount = @($championJson.summonerSpells).Count
        skillStageCount = $stageCount
        visualYawOffsetCount = @($championJson.champions).Count
        animPlaySpeedCount = $animPlaySpeedCount
        castFrameCount = $castFrameCount
        recoveryFrameCount = $recoveryFrameCount
    }
    legacyRegistrations = [ordered]@{
        skillRegistrationFileCount = @($skillRegistrationFiles).Count
        skillRegistrationFiles = $skillRegistrationFiles
        championRegistrationFileCount = @($championRegistrationFiles).Count
        championRegistrationFiles = $championRegistrationFiles
    }
    legacyLineCounts = [ordered]@{
        skillDefRelated = @($legacySkillLines).Count
        championDefRelated = @($legacyChampionLines).Count
        visualFieldsInGameplayOrLegacy = @($visualLeakLines).Count
        serverObjectHardcode = @($serverObjectLines).Count
        projectileVisualCatalog = @($projectileVisualLines).Count
    }
}

$json = $report | ConvertTo-Json -Depth 8

if ($OutputPath.Length -gt 0) {
    $resolvedOutput = if ([System.IO.Path]::IsPathRooted($OutputPath)) {
        $OutputPath
    } else {
        Join-Path $Root $OutputPath
    }
    $parent = Split-Path -Parent $resolvedOutput
    if ($parent.Length -gt 0) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }
    Set-Content -Path $resolvedOutput -Value $json -Encoding UTF8
}

Write-Output $json
