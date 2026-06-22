[CmdletBinding()]
param(
    [string]$Root = "",
    [string]$OutputPath = "Data\LoL\ClientPublic\Visual\Champion\ChampionVisualTimingSeed.json",
    [string]$ReportPath = ".md\TODO\06-22\LOL_VISUAL_TIMING_SEED_PARITY.json"
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

function Write-JsonFile {
    param(
        [string]$Path,
        [object]$Value,
        [int]$Depth = 16
    )

    $parent = Split-Path -Parent $Path
    if ($parent.Length -gt 0) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }

    $json = $Value | ConvertTo-Json -Depth $Depth
    Set-Content -Path $Path -Value $json -Encoding UTF8
}

$sourcePath = Resolve-RepoPath "Data\Gameplay\ChampionGameData\champions.json"
$outputPathResolved = Resolve-RepoPath $OutputPath
$reportPathResolved = Resolve-RepoPath $ReportPath

$source = Get-Content -Raw -Encoding UTF8 -Path $sourcePath | ConvertFrom-Json

$championVisualRows = New-Object System.Collections.Generic.List[object]
$stageCount = 0
$mismatchCount = 0
$mismatches = New-Object System.Collections.Generic.List[object]

foreach ($champion in $source.champions) {
    $skillStageRows = New-Object System.Collections.Generic.List[object]

    foreach ($skill in $champion.skills) {
        $slot = [int]$skill.slot
        $stageNumber = 1

        foreach ($stage in $skill.stages) {
            ++$stageCount

            $row = [ordered]@{
                slot = $slot
                stage = $stageNumber
                playbackSpeed = [double]$stage.animPlaySpeed
                castFrame = [double]$stage.castFrame
                recoveryFrame = [double]$stage.recoveryFrame
            }

            $skillStageRows.Add($row)

            if ($row.playbackSpeed -ne [double]$stage.animPlaySpeed -or
                $row.castFrame -ne [double]$stage.castFrame -or
                $row.recoveryFrame -ne [double]$stage.recoveryFrame) {
                ++$mismatchCount
                $mismatches.Add([ordered]@{
                    champion = [string]$champion.champion
                    slot = $slot
                    stage = $stageNumber
                    reason = "visual timing seed differs from source"
                })
            }

            ++$stageNumber
        }
    }

    $championVisualRows.Add([ordered]@{
        champion = [string]$champion.champion
        dataVersion = [int]$champion.dataVersion
        modelYawOffset = [double]$champion.visualYawOffset
        skillStages = $skillStageRows
    })
}

$seed = [ordered]@{
    schemaVersion = 1
    source = "Data/Gameplay/ChampionGameData/champions.json"
    sourceSchemaVersion = [int]$source.schemaVersion
    champions = $championVisualRows
}

$report = [ordered]@{
    generatedAtUtc = (Get-Date).ToUniversalTime().ToString("o")
    root = (Resolve-Path $Root).Path
    source = $sourcePath
    output = $outputPathResolved
    championCount = @($source.champions).Count
    skillStageCount = $stageCount
    mismatchCount = $mismatchCount
    mismatches = $mismatches
}

Write-JsonFile -Path $outputPathResolved -Value $seed
Write-JsonFile -Path $reportPathResolved -Value $report

Write-Output ($report | ConvertTo-Json -Depth 16)

if ($mismatchCount -ne 0) {
    throw "visual timing seed parity failed: $mismatchCount mismatch(es)"
}
