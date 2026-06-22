Session - ChampionGameData에 섞인 visual playback field를 ClientPublic visual seed로 추출하고 runtime reader 변경 없이 parity로 고정한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Tools/LoLData/Export-LoLChampionVisualTimingSeed.ps1

새 파일:

```powershell
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
```

2. 검증

검증 명령:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Tools\LoLData\Export-LoLChampionVisualTimingSeed.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Tools\LoLData\Collect-LoLLegacyDataAudit.ps1 -OutputPath .md\TODO\06-22\LOL_DATA_LEGACY_AUDIT.json
git diff --check
MSBuild.exe Shared\GameSim\Include\GameSim.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
MSBuild.exe Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
MSBuild.exe Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
MSBuild.exe Tools\SimLab\SimLab.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

통과 기준:

```text
LOL_VISUAL_TIMING_SEED_PARITY.json의 mismatchCount가 0이다.
LOL_VISUAL_TIMING_SEED_PARITY.json의 championCount는 17이다.
LOL_VISUAL_TIMING_SEED_PARITY.json의 skillStageCount는 94다.
ChampionVisualTimingSeed.json의 champions 배열은 17개 champion을 가진다.
runtime reader 변경은 없다.
GameSim, Server, Client, SimLab 빌드는 오류 0개다.
```

확인 필요:

```text
이 단계는 일부러 ChampionGameData.h와 ChampionGameDataDB.cpp를 변경하지 않는다.
visualYawOffset, animPlaySpeed, castFrame, recoveryFrame 삭제는 seed parity 통과 후 다음 세션에서 진행한다.
ChampionVisualTimingSeed.json은 ClientPublic visual seed이며 ServerPrivate canonical 값이 아니다.
```
