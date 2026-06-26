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
        $fullPath = Join-Path $Root $path
        if (Test-Path $fullPath) {
            $args += $fullPath
        }
    }
    if ($args.Count -eq 2) {
        return @()
    }
    $args += @(
        "-g", "!Client/Bin/**",
        "-g", "!Client/Intermediate/**",
        "-g", "!Client/Temp/**",
        "-g", "!Client/Private/Data/Generated/**",
        "-g", "!Server/Private/Data/Generated/**"
    )

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
        $fullPath = Join-Path $Root $path
        if (Test-Path $fullPath) {
            $args += $fullPath
        }
    }
    if ($args.Count -eq 2) {
        return @()
    }
    $args += @(
        "-g", "!Client/Bin/**",
        "-g", "!Client/Intermediate/**",
        "-g", "!Client/Temp/**",
        "-g", "!Client/Private/Data/Generated/**",
        "-g", "!Server/Private/Data/Generated/**"
    )

    $result = & rg @args 2>$null
    if ($LASTEXITCODE -gt 1) {
        throw "rg failed for pattern: $Pattern"
    }
    return @($result | ForEach-Object { Resolve-Path $_ | ForEach-Object { $_.Path } })
}

function Convert-RgLineBreakdown {
    param(
        [string[]]$Lines
    )

    $counts = @{}
    foreach ($line in $Lines) {
        if ($line -match "^(.*):(\d+):") {
            $path = $Matches[1]
            if (-not $counts.ContainsKey($path)) {
                $counts[$path] = 0
            }
            ++$counts[$path]
        }
    }

    return @($counts.GetEnumerator() |
        Sort-Object Name |
        ForEach-Object {
            [ordered]@{
                path = $_.Name
                count = $_.Value
            }
        })
}

$championJsonPath = Join-Path $Root "Data\Gameplay\ChampionGameData\champions.json"
$championJson = Get-Content -Raw -Path $championJsonPath | ConvertFrom-Json

$stageCount = 0
$animPlaySpeedCount = 0
$castFrameCount = 0
$recoveryFrameCount = 0
$visualYawOffsetCount = 0

foreach ($champion in $championJson.champions) {
    if ($null -ne $champion.visualYawOffset) { ++$visualYawOffsetCount }
    foreach ($skill in $champion.skills) {
        foreach ($stage in $skill.stages) {
            ++$stageCount
            if ($null -ne $stage.animPlaySpeed) { ++$animPlaySpeedCount }
            if ($null -ne $stage.castFrame) { ++$castFrameCount }
            if ($null -ne $stage.recoveryFrame) { ++$recoveryFrameCount }
        }
    }
}
$summonerSpellCount = if ($null -ne $championJson.summonerSpells) { @($championJson.summonerSpells).Count } else { 0 }

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

$visualFieldPattern = "visualYawOffset|animPlaySpeed|castFrame|recoveryFrame|ResolveVisualYawOffset|summonerSpells"

$visualLeakLines = Invoke-RgLines `
    -Pattern $visualFieldPattern `
    -Paths @("Data", "Client", "Shared", "Server", "Tools")

$visualClientPublicLines = Invoke-RgLines `
    -Pattern $visualFieldPattern `
    -Paths @("Data\LoL\ClientPublic\Visual")

$visualToolLines = Invoke-RgLines `
    -Pattern $visualFieldPattern `
    -Paths @("Tools")

$visualGameplayDataLines = Invoke-RgLines `
    -Pattern $visualFieldPattern `
    -Paths @("Data\Gameplay")

$visualClientLegacyLines = Invoke-RgLines `
    -Pattern $visualFieldPattern `
    -Paths @("Client")

$visualSharedLines = Invoke-RgLines `
    -Pattern $visualFieldPattern `
    -Paths @("Shared\GameSim")

$visualServerLines = Invoke-RgLines `
    -Pattern $visualFieldPattern `
    -Paths @("Server")

$serverObjectLines = Invoke-RgLines `
    -Pattern "ResolveStageStructureMaxHp|ResolveStageJungle|ServerMinionTuning::kWave|kWaveIntervalTicks|kInitialWaveDelayTicks|AssignDefaultBotSkillRanks|gold.amount|LethalTempo" `
    -Paths @("Server", "Shared", "Tools")

$projectileVisualLines = Invoke-RgLines `
    -Pattern "ProjectileVisualCatalog|ProjectileVisualDesc" `
    -Paths @("Client", "Shared", "Server")

$skillEffectHardcodeLines = Invoke-RgLines `
    -Pattern "constexpr\s+f32_t\s+k[A-Za-z0-9_]*(Damage|DamagePerRank|Range|Radius|Speed|DurationSec|MoveSpeedMul|Slow|Stun|Shield|Dash|Gap|Hp|Attack|Cooldown|Lifetime|Ratio|Distance|HalfAngleCos)" `
    -Paths @("Shared\GameSim\Champions")

$skillEffectQueryLines = Invoke-RgLines `
    -Pattern "ResolveSkillEffectParam|eSkillEffectParamId::" `
    -Paths @("Shared\GameSim\Champions")

$aiPolicyHardcodeLines = Invoke-RgLines `
    -Pattern "ChampionAIPolicy|AssignDefaultBotSkillRanks|TryExecute[A-Za-z]+ChampionCombat|BotSkill|SkillRank|combo|Combo" `
    -Paths @("Shared\GameSim\Systems\ChampionAI", "Server")

$networkIdentityLegacyLines = Invoke-RgLines `
    -Pattern "\bchampionId\b|ChampionId|ownerChampionDefId|eChampion" `
    -Paths @("Shared\Schemas", "Shared\GameSim\Systems\ReplicatedEventSerializer", "Client\Private\Network", "Client\Private\Scene", "Server\Private")

$legacyValueOwnerLines = Invoke-RgLines `
    -Pattern "ChampionGameDataDB|ChampionStatsRegistry|ChampionRuntimeDefaults|SkillTable|ChampionTable|ResolveMinionCombatDef|ServerMinionTuning" `
    -Paths @("Client", "Shared", "Server")

$report = [ordered]@{
    generatedAtUtc = (Get-Date).ToUniversalTime().ToString("o")
    root = (Resolve-Path $Root).Path
    championGameData = [ordered]@{
        path = $championJsonPath
        championCount = @($championJson.champions).Count
        summonerSpellCount = $summonerSpellCount
        skillStageCount = $stageCount
        visualYawOffsetCount = $visualYawOffsetCount
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
        skillEffectHardcodeCandidates = @($skillEffectHardcodeLines).Count
        skillEffectDataQueryReaders = @($skillEffectQueryLines).Count
    }
    skillEffectCutover = [ordered]@{
        hardcodeCandidateCount = @($skillEffectHardcodeLines).Count
        dataQueryReaderCount = @($skillEffectQueryLines).Count
        hardcodeCandidatesByFile = Convert-RgLineBreakdown -Lines $skillEffectHardcodeLines
        dataQueryReadersByFile = Convert-RgLineBreakdown -Lines $skillEffectQueryLines
    }
    visualFieldBreakdown = [ordered]@{
        total = @($visualLeakLines).Count
        expectedClientPublicVisual = @($visualClientPublicLines).Count
        toolExtraction = @($visualToolLines).Count
        legacyGameplaySource = @($visualGameplayDataLines).Count
        clientLegacyRuntime = @($visualClientLegacyLines).Count
        sharedGameSimAuthoritative = @($visualSharedLines).Count
        serverAuthoritative = @($visualServerLines).Count
        suspiciousAuthoritative = @($visualGameplayDataLines).Count + @($visualSharedLines).Count + @($visualServerLines).Count
    }
    phaseGoalCounts = [ordered]@{
        p3SkillEffectHardcode = @($skillEffectHardcodeLines).Count
        p4VisualAuthorityLeak = @($visualGameplayDataLines).Count + @($visualSharedLines).Count + @($visualServerLines).Count
        p5AiPolicyHardcode = @($aiPolicyHardcodeLines).Count
        p6ObjectWaveHardcode = @($serverObjectLines).Count
        p7NetworkIdentityLegacy = @($networkIdentityLegacyLines).Count
        p8LegacyValueOwner = @($legacyValueOwnerLines).Count
    }
    phaseGoalBreakdown = [ordered]@{
        p3SkillEffectHardcode = Convert-RgLineBreakdown -Lines $skillEffectHardcodeLines
        p4VisualAuthorityLeak = Convert-RgLineBreakdown -Lines @($visualGameplayDataLines + $visualSharedLines + $visualServerLines)
        p5AiPolicyHardcode = Convert-RgLineBreakdown -Lines $aiPolicyHardcodeLines
        p6ObjectWaveHardcode = Convert-RgLineBreakdown -Lines $serverObjectLines
        p7NetworkIdentityLegacy = Convert-RgLineBreakdown -Lines $networkIdentityLegacyLines
        p8LegacyValueOwner = Convert-RgLineBreakdown -Lines $legacyValueOwnerLines
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
