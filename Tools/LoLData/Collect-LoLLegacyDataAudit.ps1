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

    $args = @("-n", "-e", $Pattern)
    $pathCount = 0
    foreach ($path in $Paths) {
        $fullPath = Join-Path $Root $path
        if (Test-Path $fullPath) {
            $args += $fullPath
            ++$pathCount
        }
    }
    if ($pathCount -eq 0) {
        return @()
    }
    $args += @(
        "-g", "!Client/Bin/**",
        "-g", "!Client/Intermediate/**",
        "-g", "!Client/Temp/**",
        "-g", "!Client/Private/Data/Generated/**",
        "-g", "!Server/Private/Data/Generated/**",
        "-g", "!Shared/GameSim/Generated/**"
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

    $args = @("-l", "-e", $Pattern)
    $pathCount = 0
    foreach ($path in $Paths) {
        $fullPath = Join-Path $Root $path
        if (Test-Path $fullPath) {
            $args += $fullPath
            ++$pathCount
        }
    }
    if ($pathCount -eq 0) {
        return @()
    }
    $args += @(
        "-g", "!Client/Bin/**",
        "-g", "!Client/Intermediate/**",
        "-g", "!Client/Temp/**",
        "-g", "!Client/Private/Data/Generated/**",
        "-g", "!Server/Private/Data/Generated/**",
        "-g", "!Shared/GameSim/Generated/**"
    )

    $result = & rg @args 2>$null
    if ($LASTEXITCODE -gt 1) {
        throw "rg failed for pattern: $Pattern"
    }
    return @($result | ForEach-Object { (Resolve-Path $_).Path })
}

function Convert-RgLineBreakdown {
    param([string[]]$Lines)

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

function Read-JsonIfPresent {
    param([string]$RelativePath)

    $path = Join-Path $Root $RelativePath
    if (-not (Test-Path $path)) {
        return $null
    }
    return Get-Content -Raw -Encoding UTF8 -Path $path | ConvertFrom-Json
}

$championJsonPath = Join-Path $Root "Data\Gameplay\ChampionGameData\champions.json"
$championJson = Get-Content -Raw -Encoding UTF8 -Path $championJsonPath | ConvertFrom-Json
$assetVisualJson = Read-JsonIfPresent "Data\LoL\ClientPublic\Visual\ChampionAssetVisualDefs.json"
$aiJson = Read-JsonIfPresent "Data\LoL\ServerPrivate\AI\ChampionAIGameplayDefs.json"

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

$visualFieldPattern = "visualYawOffset|animPlaySpeed|castFrame|recoveryFrame|ResolveVisualYawOffset"
$visualLeakLines = Invoke-RgLines -Pattern $visualFieldPattern -Paths @("Data", "Client", "Shared", "Server", "Tools")
$visualClientPublicLines = Invoke-RgLines -Pattern $visualFieldPattern -Paths @("Data\LoL\ClientPublic\Visual")
$visualToolLines = Invoke-RgLines -Pattern $visualFieldPattern -Paths @("Tools")
$visualGameplayDataLines = Invoke-RgLines -Pattern $visualFieldPattern -Paths @("Data\Gameplay")
$visualClientLegacyLines = Invoke-RgLines -Pattern $visualFieldPattern -Paths @("Client")
$visualSharedLines = Invoke-RgLines -Pattern $visualFieldPattern -Paths @("Shared\GameSim")
$visualServerLines = Invoke-RgLines -Pattern $visualFieldPattern -Paths @("Server")

$skillEffectHardcodeLines = Invoke-RgLines `
    -Pattern "constexpr\s+f32_t\s+k[A-Za-z0-9_]*(Damage|DamagePerRank|Range|Radius|Speed|DurationSec|MoveSpeedMul|Slow|Stun|Shield|Dash|Gap|Hp|Attack|Cooldown|Lifetime|Ratio|Distance|HalfAngleCos)" `
    -Paths @("Shared\GameSim\Champions")

$runeTuningLiteralLines = Invoke-RgLines `
    -Pattern "RuneTuning::|kLethalTempo(MaxStacks|AttackSpeedPerStack)" `
    -Paths @("Shared\GameSim", "Server")

$packMissFallbackLines = Invoke-RgLines `
    -Pattern "ChampionGameDataDB::|ResetToDefaults\(|ResolveMinionCombatDef\(" `
    -Paths @("Client", "Shared\GameSim", "Server")

$clientGameplayLiteralLines = Invoke-RgLines `
    -Pattern "\.(cooldownSec|rangeMax|manaCost|lockDurationSec|visualCastFrame|visualRecoveryFrame|visualAnimPlaySpeed|targetMode|stage2TargetMode|stageCount|rotate|stage2Rotate)\s*=" `
    -Paths @("Client\Private\GameObject\Champion", "Client\Private\GameObject\SkillTable.cpp")

$aiValueOwnerLines = Invoke-RgLines `
    -Pattern "Make[A-Za-z]+Profile\(|static constexpr ChampionAIComboPlan|AssignDefaultBotSkillRanks" `
    -Paths @("Shared\GameSim\Systems\ChampionAI", "Server")

$objectWaveValueOwnerLines = Invoke-RgLines `
    -Pattern "ServerMinionTuning::|static constexpr MinionSpawnSlot|ResolveMinionCombatDef\(" `
    -Paths @("Client", "Shared\GameSim", "Server")

$networkIdentityRuntimeReaderLines = Invoke-RgLines `
    -Pattern "->championId\(\)|\.championId\(\)|static_cast<eChampion>\(.*championId" `
    -Paths @("Client\Private\Network", "Server\Private", "Shared\GameSim\Systems\ReplicatedEventSerializer")

$legacyValueOwnerReaderLines = Invoke-RgLines `
    -Pattern "ChampionGameDataDB::|CChampionStatsRegistry::|g_SkillTable|s_SkillTable|ResetToDefaults\(|ServerMinionTuning::" `
    -Paths @("Client", "Shared\GameSim", "Server")

$canonicalSources = @(
    "Data\Gameplay\ChampionGameData\champions.json",
    "Data\LoL\ServerPrivate\Gameplay\SkillEffectGameplayDefs.json",
    "Data\LoL\ServerPrivate\Gameplay\SummonerSpellGameplayDefs.json",
    "Data\LoL\ServerPrivate\Gameplay\SpawnObjectGameplayDefs.json",
    "Data\LoL\ServerPrivate\Gameplay\EconomyGameplayDefs.json",
    "Data\LoL\ServerPrivate\Gameplay\ItemGameplayDefs.json",
    "Data\LoL\ServerPrivate\AI\ChampionAIGameplayDefs.json",
    "Data\LoL\ServerPrivate\Gameplay\RuneGameplayDefs.json",
    "Data\LoL\ClientPublic\Visual\ChampionVisualDefs.json",
    "Data\LoL\ClientPublic\Visual\ObjectVisualDefs.json",
    "Data\LoL\ClientPublic\Visual\ChampionAssetVisualDefs.json"
)

$schemaCoverage = New-Object System.Collections.Generic.List[string]
$schemaDir = Join-Path $Root "Data\LoL\Schemas"
foreach ($source in $canonicalSources) {
    $schemaName = ([System.IO.Path]::GetFileName($source)) + ".schema.json"
    $schemaPath = Join-Path $schemaDir $schemaName
    if (Test-Path $schemaPath) {
        $schemaJson = Get-Content -Raw -Encoding UTF8 -Path $schemaPath | ConvertFrom-Json
        if ($schemaJson.PSObject.Properties.Name -contains '$schema') {
            $schemaCoverage.Add($source)
        }
    }
}

$reloadSourceFiles = @(
    "Server\Private\Data\RuntimeGameplayDefinitionOverlay.cpp",
    "Client\Private\Data\RuntimeVisualDefinitionOverlay.cpp"
)
$reloadText = ""
foreach ($relativePath in $reloadSourceFiles) {
    $path = Join-Path $Root $relativePath
    if (Test-Path $path) {
        $reloadText += Get-Content -Raw -Encoding UTF8 -Path $path
    }
}
$runtimeReloadDomains = New-Object System.Collections.Generic.List[string]
foreach ($source in $canonicalSources) {
    $fileName = [System.IO.Path]::GetFileName($source)
    if ($reloadText.Contains($fileName)) {
        $runtimeReloadDomains.Add($source)
    }
}

$draftRoundTripFailureCount = 1
$draftTestPath = Join-Path $Root "Tools\LoLData\Test-LoLDataDrivenDraftRoundTrip.ps1"
if (Test-Path $draftTestPath) {
    & powershell -ExecutionPolicy Bypass -File $draftTestPath -Root $Root -NoWrite | Out-Null
    $draftRoundTripFailureCount = if ($LASTEXITCODE -eq 0) { 0 } else { 1 }
}

$championModelCount = if ($null -ne $assetVisualJson) { @($assetVisualJson.models).Count } else { 0 }
$championUiCount = if ($null -ne $assetVisualJson) { @($assetVisualJson.ui).Count } else { 0 }
$aiProfileCount = if ($null -ne $aiJson) { @($aiJson.profiles).Count } else { 0 }

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
    coverage = [ordered]@{
        implementedChampionCount = @($championJson.champions).Count
        championModelCount = $championModelCount
        championUiCount = $championUiCount
        aiProfileCount = $aiProfileCount
    }
    tooling = [ordered]@{
        schemaCoverageCount = $schemaCoverage.Count
        schemaCoverage = $schemaCoverage
        runtimeReloadDomainCount = $runtimeReloadDomains.Count
        runtimeReloadDomains = $runtimeReloadDomains
        draftRoundTripFailureCount = $draftRoundTripFailureCount
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
        p3GameplayTuningLiteral = @($skillEffectHardcodeLines).Count + @($runeTuningLiteralLines).Count
        p3PackMissFallback = @($packMissFallbackLines).Count
        p4ClientGameplayLiteral = @($clientGameplayLiteralLines).Count
        p5AiPolicyHardcode = @($aiValueOwnerLines).Count
        p6ObjectWaveHardcode = @($objectWaveValueOwnerLines).Count
        p7NetworkIdentityRuntimeReader = @($networkIdentityRuntimeReaderLines).Count
        p8LegacyValueOwnerReader = @($legacyValueOwnerReaderLines).Count
    }
    phaseGoalBreakdown = [ordered]@{
        p3GameplayTuningLiteral = Convert-RgLineBreakdown -Lines @($skillEffectHardcodeLines + $runeTuningLiteralLines)
        p3PackMissFallback = Convert-RgLineBreakdown -Lines $packMissFallbackLines
        p4ClientGameplayLiteral = Convert-RgLineBreakdown -Lines $clientGameplayLiteralLines
        p5AiPolicyHardcode = Convert-RgLineBreakdown -Lines $aiValueOwnerLines
        p6ObjectWaveHardcode = Convert-RgLineBreakdown -Lines $objectWaveValueOwnerLines
        p7NetworkIdentityRuntimeReader = Convert-RgLineBreakdown -Lines $networkIdentityRuntimeReaderLines
        p8LegacyValueOwnerReader = Convert-RgLineBreakdown -Lines $legacyValueOwnerReaderLines
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
