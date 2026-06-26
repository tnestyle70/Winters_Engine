Session - 북극성 요구사항을 달성률 점수판으로 계측하고, 미달 항목이 0이 될 때까지 DOD migration slice loop를 반복한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Data/LoL/SharedContract/DataDrivenGoalCriteria.json

새 파일:

```json
{
  "schemaVersion": 1,
  "sourceDocument": ".md/plan/collab-pipeline/07_DATA_DRIVEN_FULL_CUTOVER_CODEX_REQUIREMENTS.md",
  "loopRule": "Measure -> choose earliest unfinished phase -> implement one reversible slice -> verify full pipeline -> write status report -> repeat.",
  "goals": [
    {
      "key": "P3SkillEffectHardcode",
      "phase": "P3",
      "name": "Champion skill gameplay/balance literals are removed",
      "auditPath": "phaseGoalCounts.p3SkillEffectHardcode",
      "targetMax": 0,
      "nextFocus": "Convert remaining champion skill gameplay literals to generated ServerPrivate gameplay atoms, then delete fallback constants only after readers are data-backed."
    },
    {
      "key": "P4VisualAuthorityLeak",
      "phase": "P4",
      "name": "Gameplay authority no longer reads visual timing/yaw fields",
      "auditPath": "phaseGoalCounts.p4VisualAuthorityLeak",
      "targetMax": 0,
      "nextFocus": "Split action timing from visual timing. Server/GameSim keeps action lock/window facts, ClientPublic owns animation/cast/recovery/yaw presentation facts."
    },
    {
      "key": "P5AiPolicyHardcode",
      "phase": "P5",
      "name": "Bot AI tactics and skill-rank policy are data-owned",
      "auditPath": "phaseGoalCounts.p5AiPolicyHardcode",
      "targetMax": 0,
      "nextFocus": "Move ChampionAIPolicy profile, combo, and bot skill-rank constants to ServerPrivate AI definition data."
    },
    {
      "key": "P6ObjectWaveHardcode",
      "phase": "P6",
      "name": "Minion, wave, jungle, structure, and placement game values are data-owned",
      "auditPath": "phaseGoalCounts.p6ObjectWaveHardcode",
      "targetMax": 0,
      "nextFocus": "Move object combat, wave timing, jungle, structure, turret, and placement fallback values into ServerPrivate object/map data."
    },
    {
      "key": "P7NetworkIdentityLegacy",
      "phase": "P7",
      "name": "Network identity uses stable DefinitionKey boundaries",
      "auditPath": "phaseGoalCounts.p7NetworkIdentityLegacy",
      "targetMax": 0,
      "nextFocus": "Add stable DefinitionKey paths for command/snapshot boundaries and remove dense pack-local champion id assumptions from network contracts."
    },
    {
      "key": "P8LegacyValueOwner",
      "phase": "P8",
      "name": "Legacy value owner paths have no runtime readers",
      "auditPath": "phaseGoalCounts.p8LegacyValueOwner",
      "targetMax": 0,
      "nextFocus": "After reader count reaches zero, delete ChampionGameDataDB value ownership, ChampionStatsRegistry defaults, ChampionRuntimeDefaults defaults, SkillTable, ChampionTable, and remaining fallback tables."
    }
  ],
  "fullVerificationCommand": "powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1",
  "completionDefinition": "All goals have currentCount <= targetMax and the full verification command passes."
}
```

1-2. C:/Users/tnest/Desktop/Winters/Tools/LoLData/Collect-LoLLegacyDataAudit.ps1

`$skillEffectQueryLines = Invoke-RgLines ...` 블록 바로 아래에 추가:

기존 코드:

```powershell
$skillEffectQueryLines = Invoke-RgLines `
    -Pattern "ResolveSkillEffectParam|eSkillEffectParamId::" `
    -Paths @("Shared\GameSim\Champions")
```

아래에 추가:

```powershell
$aiPolicyHardcodeLines = Invoke-RgLines `
    -Pattern "ChampionAIPolicy|AssignDefaultBotSkillRanks|TryExecute[A-Za-z]+ChampionCombat|BotSkill|SkillRank|combo|Combo" `
    -Paths @("Shared\GameSim\Systems\ChampionAI", "Server")

$networkIdentityLegacyLines = Invoke-RgLines `
    -Pattern "\bchampionId\b|ChampionId|ownerChampionDefId|eChampion" `
    -Paths @("Shared\Schemas", "Shared\GameSim\Systems\ReplicatedEventSerializer", "Client\Private\Network", "Client\Private\Scene", "Server\Private")

$legacyValueOwnerLines = Invoke-RgLines `
    -Pattern "ChampionGameDataDB|ChampionStatsRegistry|ChampionRuntimeDefaults|SkillTable|ChampionTable|ResolveMinionCombatDef|ServerMinionTuning" `
    -Paths @("Client", "Shared", "Server")
```

`$report = [ordered]@{ ... }` 내부에서 `visualFieldBreakdown = [ordered]@{ ... }` 블록 바로 아래에 추가:

기존 코드:

```powershell
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
```

아래에 추가:

```powershell
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
```

확인 필요: P5/P7/P8 정규식은 첫 도입 시 proxy metric이다. 첫 루프에서 false positive를 확인하고, 실제 북극성 위반만 남도록 audit pattern 또는 allowlist를 조정한다. 단, generated directory는 기존 `Invoke-RgLines` 제외 규칙을 그대로 따른다.

1-3. C:/Users/tnest/Desktop/Winters/Tools/LoLData/Get-LoLDataDrivenGoalStatus.ps1

새 파일:

```powershell
[CmdletBinding()]
param(
    [string]$Root = "",
    [string]$CriteriaPath = "",
    [string]$OutputDir = "",
    [switch]$FailWhenIncomplete
)

$ErrorActionPreference = "Stop"

if ($Root.Length -eq 0) {
    $Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
}
if ($CriteriaPath.Length -eq 0) {
    $CriteriaPath = Join-Path $Root "Data\LoL\SharedContract\DataDrivenGoalCriteria.json"
}
if ($OutputDir.Length -eq 0) {
    $date = Get-Date -Format "MM-dd"
    $OutputDir = Join-Path $Root ".md\TODO\$date"
}

function Get-NestedValue {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Object,
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [object]$Fallback = $null
    )

    $current = $Object
    foreach ($part in $Path.Split(".")) {
        if ($null -eq $current) {
            return $Fallback
        }
        if (-not ($current.PSObject.Properties.Name -contains $part)) {
            return $Fallback
        }
        $current = $current.$part
    }
    return $current
}

function New-GoalStatus {
    param(
        [object]$Goal,
        [object]$Audit
    )

    $rawCurrent = Get-NestedValue -Object $Audit -Path $Goal.auditPath -Fallback 999999
    $current = [int]$rawCurrent
    $target = [int]$Goal.targetMax

    return [ordered]@{
        key = [string]$Goal.key
        phase = [string]$Goal.phase
        name = [string]$Goal.name
        currentCount = $current
        targetMax = $target
        achieved = ($current -le $target)
        remaining = [Math]::Max(0, $current - $target)
        nextFocus = [string]$Goal.nextFocus
    }
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$auditPath = Join-Path $OutputDir "LOL_DATA_DRIVEN_AUDIT.json"
$statusPath = Join-Path $OutputDir "LOL_DATA_DRIVEN_GOAL_STATUS.json"
$markdownPath = Join-Path $OutputDir "LOL_DATA_DRIVEN_GOAL_STATUS.md"

powershell -ExecutionPolicy Bypass -File (Join-Path $Root "Tools\LoLData\Collect-LoLLegacyDataAudit.ps1") `
    -Root $Root `
    -OutputPath $auditPath | Out-Null

$criteria = Get-Content -Raw -Encoding UTF8 -Path $CriteriaPath | ConvertFrom-Json
$audit = Get-Content -Raw -Encoding UTF8 -Path $auditPath | ConvertFrom-Json

$manifestPath = Join-Path $Root "Data\LoL\SharedContract\DefinitionManifest.json"
$buildHash = ""
if (Test-Path $manifestPath) {
    $manifest = Get-Content -Raw -Encoding UTF8 -Path $manifestPath | ConvertFrom-Json
    if ($manifest.PSObject.Properties.Name -contains "buildHash") {
        $buildHash = $manifest.buildHash
    }
}

$goals = @()
foreach ($goal in $criteria.goals) {
    $goals += New-GoalStatus -Goal $goal -Audit $audit
}

$unfinished = @($goals | Where-Object { -not $_.achieved })
$nextGoal = $null
if ($unfinished.Count -gt 0) {
    $nextGoal = $unfinished | Sort-Object -Property remaining -Descending | Select-Object -First 1
}

$status = [ordered]@{
    generatedAtUtc = (Get-Date).ToUniversalTime().ToString("o")
    root = (Resolve-Path $Root).Path
    sourceDocument = $criteria.sourceDocument
    buildHash = $buildHash
    complete = ($unfinished.Count -eq 0)
    completedGoalCount = @($goals | Where-Object { $_.achieved }).Count
    totalGoalCount = @($goals).Count
    nextGoal = $nextGoal
    goals = $goals
    auditPath = $auditPath
    fullVerificationCommand = $criteria.fullVerificationCommand
    completionDefinition = $criteria.completionDefinition
}

$status | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 -Path $statusPath

$lines = @()
$lines += "# LoL Data-Driven Goal Status"
$lines += ""
$lines += "generatedAtUtc: $($status.generatedAtUtc)"
$lines += "buildHash: $($status.buildHash)"
$lines += "complete: $($status.complete)"
$lines += "completedGoalCount: $($status.completedGoalCount) / $($status.totalGoalCount)"
$lines += ""
if ($null -ne $nextGoal) {
    $lines += "## Next Focus"
    $lines += ""
    $lines += "- phase: $($nextGoal.phase)"
    $lines += "- key: $($nextGoal.key)"
    $lines += "- remaining: $($nextGoal.remaining)"
    $lines += "- action: $($nextGoal.nextFocus)"
    $lines += ""
}
$lines += "## Goals"
$lines += ""
foreach ($goal in $goals) {
    $mark = if ($goal.achieved) { "PASS" } else { "TODO" }
    $lines += "- [$mark] $($goal.phase) $($goal.key): current=$($goal.currentCount), targetMax=$($goal.targetMax)"
}
$lines += ""
$lines += "## Gate"
$lines += ""
$lines += "- $($status.fullVerificationCommand)"
$lines += "- $($status.completionDefinition)"

$lines | Set-Content -Encoding UTF8 -Path $markdownPath

Write-Output ($status | ConvertTo-Json -Depth 8)

if ($FailWhenIncomplete -and -not $status.complete) {
    exit 2
}
```

1-4. C:/Users/tnest/Desktop/Winters/Tools/LoLData/Run-LoLDataDrivenGoalLoop.ps1

새 파일:

```powershell
[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [int]$MaxIterations = 1,
    [switch]$SkipFullVerify
)

$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

function Invoke-Checked {
    param(
        [string]$Name,
        [scriptblock]$Command
    )

    Write-Host "[LoLGoalLoop] $Name"
    & $Command
    if ($LASTEXITCODE -ne 0) {
        throw "$Name failed with exit code $LASTEXITCODE"
    }
}

Push-Location $Root
try {
    for ($iteration = 1; $iteration -le $MaxIterations; ++$iteration) {
        Write-Host "[LoLGoalLoop] Iteration $iteration / $MaxIterations"

        $statusJson = powershell -ExecutionPolicy Bypass -File Tools/LoLData/Get-LoLDataDrivenGoalStatus.ps1
        if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne 2) {
            throw "Goal status failed with exit code $LASTEXITCODE"
        }

        $status = $statusJson | ConvertFrom-Json
        if ($status.complete) {
            Write-Host "[LoLGoalLoop] COMPLETE before verification"
            if (-not $SkipFullVerify) {
                Invoke-Checked "Full data-driven verification" {
                    powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1 -Configuration $Configuration
                }
            }
            Write-Host "[LoLGoalLoop] COMPLETE"
            exit 0
        }

        Write-Host "[LoLGoalLoop] NEXT phase=$($status.nextGoal.phase) key=$($status.nextGoal.key) remaining=$($status.nextGoal.remaining)"
        Write-Host "[LoLGoalLoop] ACTION $($status.nextGoal.nextFocus)"

        if (-not $SkipFullVerify) {
            Invoke-Checked "Full data-driven verification" {
                powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1 -Configuration $Configuration
            }
        }

        if ($iteration -lt $MaxIterations) {
            Write-Host "[LoLGoalLoop] Code migration slice must be applied before the next iteration can reduce counts."
        }
    }
}
finally {
    Pop-Location
}

Write-Host "[LoLGoalLoop] INCOMPLETE"
exit 2
```

확인 필요: 이 스크립트는 스스로 코드를 수정하지 않는다. 루프의 구현자는 Codex다. 스크립트는 현재 달성률, 다음 미달 항목, 전체 검증 통과 여부를 고정된 형식으로 산출한다.

1-5. C:/Users/tnest/Desktop/Winters/.md/plan/collab-pipeline/07_DATA_DRIVEN_FULL_CUTOVER_CODEX_REQUIREMENTS.md

`5. 원자 실행 프로토콜 (Per-Slice, 무회귀)`의 루프 설명 아래에 추가:

기존 코드:

```text
S-7. 보고 : ±7 리포트 1건 작성(`.md/TODO/<날짜>/`).
```

아래에 추가:

```text
S-8. 목표 점수판 갱신 : `Tools/LoLData/Get-LoLDataDrivenGoalStatus.ps1`로 북극성 요구사항별 달성률을 기록한다.
S-9. 다음 slice 선택 : 미달 goal 중 P3 -> P4 -> P5 -> P6 -> P7 -> P8 순서로 가장 이른 phase를 먼저 고른다. 같은 phase 안에서는 `remaining`이 큰 항목을 우선한다.
```

`6. 검증 게이트 (모든 slice 공통)`의 명령 목록 아래에 추가:

기존 코드:

```text
실행: `powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1`
```

아래에 추가:

```text
목표 루프 실행: `powershell -ExecutionPolicy Bypass -File Tools/LoLData/Run-LoLDataDrivenGoalLoop.ps1`
목표 상태만 확인: `powershell -ExecutionPolicy Bypass -File Tools/LoLData/Get-LoLDataDrivenGoalStatus.ps1`
```

2. 검증

미검증:
- 목표 점수판 JSON 기준 파일 미생성
- audit proxy metric의 false positive 정리 미검증
- goal loop 스크립트 미실행
- 전체 Verify 파이프라인과 goal loop 연동 미검증

검증 명령:
- powershell -ExecutionPolicy Bypass -File Tools/LoLData/Collect-LoLLegacyDataAudit.ps1
- powershell -ExecutionPolicy Bypass -File Tools/LoLData/Get-LoLDataDrivenGoalStatus.ps1
- powershell -ExecutionPolicy Bypass -File Tools/LoLData/Run-LoLDataDrivenGoalLoop.ps1 -SkipFullVerify
- powershell -ExecutionPolicy Bypass -File Tools/LoLData/Run-LoLDataDrivenGoalLoop.ps1
- powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1
- git diff --check

확인 필요:
- `phaseGoalCounts.p5AiPolicyHardcode`, `p7NetworkIdentityLegacy`, `p8LegacyValueOwner`는 첫 측정 후 false positive를 제거한다.
- goal loop가 incomplete일 때 exit code 2를 반환하는 것을 정상 미달 상태로 취급한다.
- normal verify pipeline은 모든 phase 완료 전에도 PASS할 수 있어야 한다. 목표 완료 여부는 goal loop가 별도 판단한다.
