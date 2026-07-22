Session - 현재 65%인 LoL Data Driven을 zero-reader·DefinitionKey·기획자 round-trip까지 100%로 끝내고, MainMenu의 하드코딩 야스오 초상화를 제거해 기존 프로필 카드를 클릭 진입점으로 사용한다.
좌표: 신규 좌표 후보 · 축: C7 · C8
관련: 07_DATA_DRIVEN_FULL_CUTOVER_CODEX_REQUIREMENTS.md, 2026-07-16_GAMEPLAY_FORMULA_SKILL_ANIMATION_DATA_DRIVEN_REMEDIATION_PLAN.md

# 1. 결정 기록

① 문제·제약: 현재 17챔피언·85스킬 서버 수치 경로는 약 94%지만 전체는 약 65%이며, AI 17프로필·미니언 튜닝 26후보·network championId·legacy reader가 남아 있다.
② 순진한 해법의 실패: JSON과 새 resolver만 더하고 코드 fallback을 유지하면 두 값 소유자가 계속 살아서 “정상 경로 PASS”가 “완전 컷오버”를 증명하지 못한다.
③ 메커니즘: canonical JSON → 검증/codegen → immutable pack → fail-closed query 한 경로로 만들고, reader 0을 계측한 뒤 legacy를 삭제한다.
④ 대조: 런타임 툴이 source JSON을 즉시 덮어쓰게 하지 않고 session override와 review draft를 분리하며, 승인된 draft만 hash 검증 후 canonical source에 반영한다.
⑤ 대가: 단계별 byte parity와 full build가 필요해 단기 속도는 낮아지지만, 마지막에는 fallback·수동 복제·식별자 drift가 사라진다. 신규 gameplay 규칙 추가가 섞이면 이 계획은 틀리므로 별도 기능 slice로 분리한다.

# 2. 반영해야 하는 코드와 실행 순서

## 2-0. 현재 기준선과 100% 정의

현재 dirty worktree에서 직접 확인한 기준선:

| 항목 | 현재 |
|---|---:|
| 챔피언 / 스킬 슬롯 | 17 / 85 |
| gameplay stage / visual stage | 96 / 98 |
| SkillEffect 정의 / 유효 damage formula | 85 / 59 |
| 효과 파라미터 필드 | 265 |
| 아이템 / 아이템 on-hit formula | 34 / 1 |
| 정글 캠프 / 미니언 전투 타입 | 11 / 5 |
| ClientPublic UI portrait coverage | 17 / 17 |
| ClientPublic model definition coverage | 7 / 17 |
| AI profile | 17개 전부 C++ constexpr |
| 런타임 서버 JSON reload | 6개 source |
| 네트워크 주 champion identity | championId:ubyte |
| 핵심 실행 검증 | SimLab FormulaData/BORK/SkillRank/결정성 PASS |

100%는 퍼센트 추정이 아니라 다음 boolean gate가 모두 true인 상태다.

1. gameplay/balance/visual tunable literal owner가 generated 산출물 외 런타임 코드에 없다.
2. 모든 현재 구현 콘텐츠가 canonical source에서 누락 없이 생성된다.
3. pack miss가 legacy 값으로 내려가지 않고 검증/명시적 실패로 보인다.
4. AI, 룬, 미니언 행동/웨이브/placement, client visual asset이 데이터 소유다.
5. wire/save/manifest 식별자가 DefinitionKey를 사용하고 legacy byte ID를 읽지 않는다.
6. ChampionGameDataDB, ChampionStatsRegistry, ChampionRuntimeDefaults 값 함수, SkillTable, 하드코딩 ItemRegistry, ServerMinionTuning reader가 0이고 삭제된다.
7. JSON Schema, Debug hot reload, draft export, hash-guarded canonical apply가 연결된다.
8. final verifier가 read-only 상태로 freshness/audit/parity/build/SimLab/diff를 모두 통과한다.

## 2-A. 실행 배분과 마감 제안

제안 외부 시연 마감은 2026-08-14로 둔다. 매주 시간은 바닥 70% / 천장 30%로 고정한다.

| 순서 | 분류 | 결과물 | 종료 게이트 | 예상 지표 |
|---|---|---|---|---:|
| D0 | 천장 | MainMenu 기존 프로필 카드 클릭 → MyInfo | Client build + 3해상도 시각 smoke | 사용자 가시 기능 |
| P0 | 바닥 | 오탐 없는 audit와 read-only verifier | 동일 tree 반복 실행 결과 동일 | 측정 신뢰 |
| P3 | 바닥 | 챔피언 tail, 경제, 아이템, 룬 fail-closed | gameplay fallback 0 | 약 72% |
| P4 | 천장 | 17/17 model·visual asset와 client registration cutover | client literal owner 0 | 약 78% |
| P5 | 천장 | AI profile/combo live tuning 데모 | AI hardcode 0, AI command hash parity | 약 86% |
| P6 | 바닥 | 미니언 행동/포메이션/placement 데이터화 | object/wave hardcode 0 | 약 90% |
| P7 | 바닥 | DefinitionKey wire cutover | legacy network reader 0 | 약 94% |
| P8 | 바닥 | 모든 legacy owner 삭제 | zero-reader + clean build | 약 97% |
| P9 | 천장 | Schema + hot reload + draft round-trip | 기획자 1인 smoke | 100% |

각 phase는 별도 커밋 단위로 끝낸다. 순수 값 이관 slice에서는 SimLab same-seed hash가 변하면 다음 phase로 가지 않는다.

## 2-1. C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_MainMenu.h

실제 초상화 소유자는 Engine CScene_Manager가 아니라 CScene_MainMenu다. CScene_Manager에는 손대지 않는다.

기존 코드:

~~~cpp
	std::string m_strStatus{};
	CImageScenePresenter m_ImageUI{};
	std::unique_ptr<Engine::CTexture> m_pMyInfoPortrait{};
	bool_t m_bPlayRequested = false;
	bool_t m_bLogoutRequested = false;
	bool_t m_bShopRequested = false;
	bool_t m_bMyInfoRequested = false;
~~~

아래로 교체:

~~~cpp
	std::string m_strStatus{};
	CImageScenePresenter m_ImageUI{};
	bool_t m_bPlayRequested = false;
	bool_t m_bLogoutRequested = false;
	bool_t m_bShopRequested = false;
	bool_t m_bMyInfoRequested = false;
~~~

별도 popup 상태를 추가하지 않는다. 이미 CScene_MyInfo가 profile/history/replay와 뒤로가기를 소유하므로 기존 ChangeToMyInfo 경로를 재사용한다.

## 2-2. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_MainMenu.cpp

MainMenu1.png 실제 파일은 1280x720이지만 현재 Presenter는 1545x859 가상 source 좌표로 운용 중이다. 이 slice에서는 전체 버튼을 재좌표화하지 않고 기존 가상 좌표계를 유지한다.

기존 코드:

~~~cpp
	constexpr ImageSourceRect kGameStartRect{ 47.f, 24.f, 236.f, 70.f };
	// MainMenu1.png 소스 공간(1545x859) 기준: Game Start 아래 상점 버튼, 우상단 나의 정보 초상화.
	constexpr ImageSourceRect kShopButtonRect{ 95.f, 92.f, 284.f, 138.f };
	constexpr ImageSourceRect kMyInfoPortraitRect{ 1405.f, 24.f, 1505.f, 124.f };
~~~

아래로 교체:

~~~cpp
	constexpr ImageSourceRect kGameStartRect{ 47.f, 24.f, 236.f, 70.f };
	// 기존 1545x859 가상 source 좌표를 유지한다.
	// 프로필 진입 영역은 배경에 그려진 portrait + name/status 카드만 포함하고
	// 우측 설정/최소화/닫기 영역은 포함하지 않는다.
	constexpr ImageSourceRect kShopButtonRect{ 95.f, 92.f, 284.f, 138.f };
	constexpr ImageSourceRect kMyInfoProfileEntryRect{ 1275.f, 0.f, 1465.f, 100.f };
~~~

기존 코드:

~~~cpp
	if (IRHIDevice* pDevice = CGameInstance::Get()->Get_RHIDevice())
	{
		m_pMyInfoPortrait = Engine::CTexture::Create(
			pDevice,
			L"Texture/UI/Champion/Portraits/yasuo_square.png",
			Engine::eTexSamplerMode::Clamp,
			Engine::eTexColorSpace::IgnoreSRGB);
	}
~~~

삭제할 코드:

~~~cpp
	if (IRHIDevice* pDevice = CGameInstance::Get()->Get_RHIDevice())
	{
		m_pMyInfoPortrait = Engine::CTexture::Create(
			pDevice,
			L"Texture/UI/Champion/Portraits/yasuo_square.png",
			Engine::eTexSamplerMode::Clamp,
			Engine::eTexColorSpace::IgnoreSRGB);
	}
~~~

기존 코드:

~~~cpp
	if (m_ImageUI.WasSourceRectClicked(kMyInfoPortraitRect))
		m_bMyInfoRequested = true;
~~~

아래로 교체:

~~~cpp
	if (m_ImageUI.WasSourceRectClicked(kMyInfoProfileEntryRect))
		m_bMyInfoRequested = true;
~~~

삭제할 코드:

~~~cpp
	// 우상단 나의 정보 챔피언 초상화 버튼
	if (m_pMyInfoPortrait)
		m_ImageUI.DrawSourceImage(m_pMyInfoPortrait.get(), kMyInfoPortraitRect, Vec4(1.f, 1.f, 1.f, 1.f));
	else
		m_ImageUI.DrawSourceRect(kMyInfoPortraitRect, Vec4(0.05f, 0.05f, 0.08f, 0.9f));
	m_ImageUI.DrawSourceRectOutline(kMyInfoPortraitRect, Vec4(0.68f, 0.52f, 0.18f, 0.95f), 2.f);
~~~

남겨야 하는 기존 코드:

~~~cpp
void CScene_MainMenu::ChangeToMyInfo()
{
	CGameInstance::Get()->Change_Scene(
		static_cast<uint32_t>(eSceneID::MyInfo),
		CScene_MyInfo::Create());
}
~~~

이 결과는 “야스오 portrait texture를 새로 그림”이 아니라 “MainMenu1.png에 이미 존재하는 프로필 카드가 클릭 영역이며, 클릭하면 현재 profile scene이 열린다”이다.

## 2-3. C:/Users/user/Desktop/Winters/Data/LoL/SharedContract/DataDrivenGoalCriteria.json

기존 파일 전체를 아래로 교체한다. 기존 targetMax 전용 판정은 2-4의 comparison 판정으로 먼저 확장한다.

~~~json
{
  "schemaVersion": 2,
  "sourceDocument": ".md/plan/2026-07-17_DATA_DRIVEN_100_PERCENT_AND_MAIN_MENU_PROFILE_PLAN.md",
  "loopRule": "Measure -> choose earliest unfinished phase -> migrate one byte-identical reader slice -> verify -> remove zero-reader owner -> repeat.",
  "phaseOrder": ["P3", "P4", "P5", "P6", "P7", "P8", "P9"],
  "goals": [
    {
      "key": "P3GameplayTuningLiteral",
      "phase": "P3",
      "name": "Gameplay tuning literals outside generated code are zero",
      "auditPath": "phaseGoalCounts.p3GameplayTuningLiteral",
      "comparison": "max",
      "target": 0,
      "nextFocus": "Move remaining champion, item, economy, rune and formula fallback values to canonical ServerPrivate data."
    },
    {
      "key": "P3PackMissFallback",
      "phase": "P3",
      "name": "Definition queries do not fall back to legacy value owners",
      "auditPath": "phaseGoalCounts.p3PackMissFallback",
      "comparison": "max",
      "target": 0,
      "nextFocus": "Replace legacy fallback with validated coverage and explicit missing-definition failure."
    },
    {
      "key": "P4ClientGameplayLiteral",
      "phase": "P4",
      "name": "Client registration owns no gameplay truth or duplicated visual scalar",
      "auditPath": "phaseGoalCounts.p4ClientGameplayLiteral",
      "comparison": "max",
      "target": 0,
      "nextFocus": "Generate SkillDef/Champion visual records and keep registration files limited to hooks."
    },
    {
      "key": "P4ChampionModelCoverage",
      "phase": "P4",
      "name": "Every implemented champion has ClientPublic model and UI definitions",
      "auditPath": "coverage.championModelCount",
      "comparison": "min",
      "target": 17,
      "nextFocus": "Extract remaining registration asset paths into ChampionAssetVisualDefs.json."
    },
    {
      "key": "P5AiPolicyHardcode",
      "phase": "P5",
      "name": "Champion AI profile, skill rule and combo tuning are data-owned",
      "auditPath": "phaseGoalCounts.p5AiPolicyHardcode",
      "comparison": "max",
      "target": 0,
      "nextFocus": "Move ChampionAIPolicy Make*Profile and combo tables into the ServerPrivate pack."
    },
    {
      "key": "P5AiProfileCoverage",
      "phase": "P5",
      "name": "Every implemented champion has a generated AI profile",
      "auditPath": "coverage.aiProfileCount",
      "comparison": "min",
      "target": 17,
      "nextFocus": "Reject missing champion AI definitions during codegen."
    },
    {
      "key": "P6ObjectWaveHardcode",
      "phase": "P6",
      "name": "Minion behavior, wave formation, structure, jungle and placement tuning are data-owned",
      "auditPath": "phaseGoalCounts.p6ObjectWaveHardcode",
      "comparison": "max",
      "target": 0,
      "nextFocus": "Move ServerMinionTuning and wave formation values into SpawnObjectGameplayDefs."
    },
    {
      "key": "P7NetworkIdentityRuntimeReader",
      "phase": "P7",
      "name": "Runtime network code reads stable DefinitionKey fields only",
      "auditPath": "phaseGoalCounts.p7NetworkIdentityRuntimeReader",
      "comparison": "max",
      "target": 0,
      "nextFocus": "Append DefinitionKey fields, migrate writers/readers, then deprecate byte fields."
    },
    {
      "key": "P8LegacyValueOwnerReader",
      "phase": "P8",
      "name": "Legacy value owners have zero runtime readers",
      "auditPath": "phaseGoalCounts.p8LegacyValueOwnerReader",
      "comparison": "max",
      "target": 0,
      "nextFocus": "Delete reader-zero DB, tables, defaults and fallback registries."
    },
    {
      "key": "P9SchemaCoverage",
      "phase": "P9",
      "name": "Every canonical authoring source has a generated JSON Schema",
      "auditPath": "tooling.schemaCoverageCount",
      "comparison": "min",
      "target": 11,
      "nextFocus": "Emit schemas for eight gameplay/AI/rune and three visual sources."
    },
    {
      "key": "P9RuntimeReloadCoverage",
      "phase": "P9",
      "name": "Every tunable definition domain supports validated Debug reload",
      "auditPath": "tooling.runtimeReloadDomainCount",
      "comparison": "min",
      "target": 11,
      "nextFocus": "Add server AI/rune and client visual/asset reload without partial publication."
    },
    {
      "key": "P9DraftRoundTrip",
      "phase": "P9",
      "name": "Planner draft can be hash-checked, applied and regenerated",
      "auditPath": "tooling.draftRoundTripFailureCount",
      "comparison": "max",
      "target": 0,
      "nextFocus": "Add base-hash guarded apply-draft and round-trip regression fixtures."
    }
  ],
  "fullVerificationCommand": "powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1 -RequireComplete",
  "completionDefinition": "All goals are achieved, all legacy reader counts are zero, and the read-only full verification command passes."
}
~~~

## 2-4. C:/Users/user/Desktop/Winters/Tools/LoLData/Get-LoLDataDrivenGoalStatus.ps1

기존 코드:

~~~powershell
param(
    [string]$Root = "",
    [string]$CriteriaPath = "",
    [string]$OutputDir = "",
    [switch]$FailWhenIncomplete
)
~~~

아래로 교체:

~~~powershell
param(
    [string]$Root = "",
    [string]$CriteriaPath = "",
    [string]$OutputDir = "",
    [switch]$FailWhenIncomplete,
    [switch]$NoWrite
)
~~~

기존 New-GoalStatus 함수 전체를 아래로 교체한다.

~~~powershell
function New-GoalStatus {
    param(
        [object]$Goal,
        [object]$Audit,
        [object[]]$PhaseOrder
    )

    $rawCurrent = Get-NestedValue -Object $Audit -Path $Goal.auditPath -Fallback $null
    if ($null -eq $rawCurrent) {
        throw "goal audit path not found: $($Goal.auditPath)"
    }
    $current = [int]$rawCurrent
    $comparison = [string]$Goal.comparison
    $target = [int]$Goal.target
    $phase = [string]$Goal.phase

    $achieved = switch ($comparison) {
        "max" { $current -le $target }
        "min" { $current -ge $target }
        "equal" { $current -eq $target }
        default { throw "unknown goal comparison: $comparison" }
    }

    $remaining = switch ($comparison) {
        "max" { [Math]::Max(0, $current - $target) }
        "min" { [Math]::Max(0, $target - $current) }
        "equal" { [Math]::Abs($target - $current) }
    }

    return [ordered]@{
        key = [string]$Goal.key
        phase = $phase
        phaseRank = Get-PhaseRank -Phase $phase -PhaseOrder $PhaseOrder
        name = [string]$Goal.name
        currentCount = $current
        comparison = $comparison
        target = $target
        achieved = $achieved
        remaining = $remaining
        nextFocus = [string]$Goal.nextFocus
    }
}
~~~

New-Item과 status/markdown Set-Content를 if (-not $NoWrite)로 감싼다. NoWrite에서는 Collect-LoLLegacyDataAudit.ps1 stdout을 ConvertFrom-Json으로 직접 받아 파일을 만들지 않는다.

## 2-5. C:/Users/user/Desktop/Winters/Tools/LoLData/Collect-LoLLegacyDataAudit.ps1

현재 visual pattern의 summonerSpells는 visual authority leak가 아니므로 제거한다.

기존 코드:

~~~powershell
$visualFieldPattern = "visualYawOffset|animPlaySpeed|castFrame|recoveryFrame|ResolveVisualYawOffset|summonerSpells"
~~~

아래로 교체:

~~~powershell
$visualFieldPattern = "visualYawOffset|animPlaySpeed|castFrame|recoveryFrame|ResolveVisualYawOffset"
~~~

광범위한 eChampion/Combo 문자열 개수 대신 실제 value owner와 runtime reader를 별도 집계한다.

~~~powershell
$packMissFallbackLines = Invoke-RgLines `
    -Pattern "ChampionGameDataDB::|ResetToDefaults\(|ResolveMinionCombatDef\(" `
    -Paths @("Client", "Shared\GameSim", "Server")

$runeTuningLiteralLines = Invoke-RgLines `
    -Pattern "RuneTuning::|kLethalTempo(MaxStacks|AttackSpeedPerStack)" `
    -Paths @("Shared\GameSim", "Server")

$clientGameplayLiteralLines = Invoke-RgLines `
    -Pattern "\.(cooldownSec|rangeMax|manaCost|lockDurationSec|visualCastFrame|visualRecoveryFrame|visualAnimPlaySpeed)\s*=" `
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
~~~

JSON coverage는 regex가 아니라 ConvertFrom-Json으로 센다. report에는 아래 구조를 추가한다.

~~~powershell
    coverage = [ordered]@{
        implementedChampionCount = @($championJson.champions).Count
        championModelCount = $championModelCount
        championUiCount = $championUiCount
        aiProfileCount = $aiProfileCount
    }
    tooling = [ordered]@{
        schemaCoverageCount = $schemaCoverageCount
        runtimeReloadDomainCount = $runtimeReloadDomainCount
        draftRoundTripFailureCount = $draftRoundTripFailureCount
    }
    phaseGoalCounts = [ordered]@{
        p3GameplayTuningLiteral =
            @($skillEffectHardcodeLines).Count + @($runeTuningLiteralLines).Count
        p3PackMissFallback = @($packMissFallbackLines).Count
        p4ClientGameplayLiteral = @($clientGameplayLiteralLines).Count
        p5AiPolicyHardcode = @($aiValueOwnerLines).Count
        p6ObjectWaveHardcode = @($objectWaveValueOwnerLines).Count
        p7NetworkIdentityRuntimeReader = @($networkIdentityRuntimeReaderLines).Count
        p8LegacyValueOwnerReader = @($legacyValueOwnerReaderLines).Count
    }
~~~

CONFIRM_NEEDED: 각 phase 첫 slice에서 baseline 결과를 사람이 분류하고 오탐 path/symbol을 좁힌다. 수치가 줄었다는 이유만으로 완료 처리하지 않는다.

## 2-6. C:/Users/user/Desktop/Winters/Tools/LoLData/Export-LoLChampionVisualTimingSeed.ps1

param 블록에 [switch]$Check를 추가한다. 마지막 Write-JsonFile 두 호출은 아래로 교체한다.

~~~powershell
if ($Check) {
    if (-not (Test-Path $outputPathResolved)) {
        throw "visual timing seed missing: $outputPathResolved"
    }

    $actualSeed = Get-Content -Raw -Encoding UTF8 -Path $outputPathResolved |
        ConvertFrom-Json |
        ConvertTo-Json -Depth 16
    $expectedSeed = $seed | ConvertTo-Json -Depth 16
    if ($actualSeed -ne $expectedSeed) {
        throw "visual timing seed is stale"
    }
}
else {
    Write-JsonFile -Path $outputPathResolved -Value $seed
    Write-JsonFile -Path $reportPathResolved -Value $report
}
~~~

Check 모드는 worktree를 변경하지 않는다.

## 2-7. C:/Users/user/Desktop/Winters/Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1

기존 param 블록을 아래로 교체한다.

~~~powershell
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [switch]$RequireComplete
)
~~~

Definition pack freshness 아래에 legacy generator가 살아 있는 P3~P7 동안만 추가:

~~~powershell
    Invoke-Checked "Legacy champion data freshness" {
        python Tools/ChampionData/build_champion_game_data.py --check
    }
~~~

goal status와 visual timing 호출을 아래로 교체한다.

~~~powershell
    Invoke-Checked "Data-driven goal status" {
        $goalArgs = @(
            "-ExecutionPolicy", "Bypass",
            "-File", "Tools/LoLData/Get-LoLDataDrivenGoalStatus.ps1",
            "-NoWrite"
        )
        if ($RequireComplete) {
            $goalArgs += "-FailWhenIncomplete"
        }
        powershell @goalArgs
    }
    Invoke-Checked "Client visual timing parity" {
        powershell -ExecutionPolicy Bypass `
            -File Tools/LoLData/Export-LoLChampionVisualTimingSeed.ps1 `
            -Check
    }
~~~

최종 완료 게이트는 반드시 -RequireComplete로 실행한다.

## 2-8. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/ChampionGameplayDef.h

기존 ChampionStatBlock 전체를 아래로 교체한다.

~~~cpp
struct ChampionStatBlock
{
    f32_t baseHp{};
    f32_t hpPerLevel{};
    f32_t baseMana{};
    f32_t manaPerLevel{};
    f32_t baseAd{};
    f32_t adPerLevel{};
    f32_t baseAp{};
    f32_t apPerLevel{};
    f32_t baseArmor{};
    f32_t armorPerLevel{};
    f32_t baseMr{};
    f32_t mrPerLevel{};
    f32_t baseAttackSpeed{};
    f32_t attackSpeedRatio{};
    f32_t attackSpeedPerLevel{};
    f32_t baseAttackRange{};
    f32_t baseMoveSpeed{};
    f32_t navArriveRadius{};
    f32_t spatialRadius{};
    f32_t sightRange{};
};
~~~

codegen은 17챔피언 모든 필드를 필수로 검증한다.

## 2-9. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/EconomyGameplayDef.h

모든 숫자 initializer를 {}로 바꾸고 bValid를 제거한다. 최종 EconomyGameplayDef shape:

~~~cpp
struct EconomyGameplayDef
{
    f32_t xpRequiredForNextLevel[ChampionExperienceCurveDef::kMaxChampionLevel + 1]{};
    EconomyChampionKillRewardDef championKill{};
    EconomyMinionRewardDef melee{};
    EconomyMinionRewardDef ranged{};
    EconomyMinionRewardDef siege{};
    EconomyMinionRewardDef super{};
    f32_t turretGold{};
    EconomyJungleRewardDef jungle{};
    u64_t passiveGoldStartTick{};
    u64_t passiveGoldIntervalTicks{};
    u32_t passiveGoldPerGrant{};
    f32_t assistCreditWindowSec{};
    f32_t recallDurationSec{};
};
~~~

EconomyChampionKillRewardDef와 EconomyJungleRewardDef의 non-zero initializer도 {}로 교체한다.

## 2-10. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/MinionCombatDef.h

role ID 상수는 남기고 값 owner인 ResolveMinionCombatDef와 non-zero member default를 삭제한다.

~~~cpp
struct MinionCombatDef
{
    f32_t moveSpeed{};
    f32_t attackRange{};
    f32_t sightRange{};
    f32_t attackDamage{};
    f32_t attackCooldownMax{};
    f32_t maxHp{};
};
~~~

## 2-11. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/SpawnObjectDefinitionPack.h

MinionWaveDef의 non-zero defaults를 제거하고 행동/포메이션 shape를 추가한다.

~~~cpp
struct MinionBehaviorDef
{
    f32_t pathAgentRadius{};
    f32_t laneClearanceRadius{};
    f32_t softSeparationRadiusScale{};
    f32_t softSeparationWeight{};
    f32_t softSeparationMaxStep{};
    f32_t lanePathRebuildIntervalSec{};
    f32_t chasePathRebuildIntervalSec{};
    f32_t pathTargetRefreshDistanceSq{};
    f32_t pathWaypointArriveRadius{};
    u32_t pathBuildBudgetPerTick{};
    u8_t blockedFramesBeforeRepath{};
    u8_t flowFieldStallFramesBeforePathFallback{};
    f32_t flowFieldProgressSlackSq{};
    f32_t structureAcquireRangePadding{};
    f32_t targetScanIntervalSec{};
    u32_t targetScanStaggerBuckets{};
    u8_t rangedRoleType{};
    f32_t attackExitRangePadding{};
    f32_t meleeAttackWindupSec{};
    f32_t rangedAttackWindupSec{};
    f32_t attackRecoverySec{};
};

struct MinionWaveSpawnSlotDef
{
    u8_t roleType{};
    f32_t forwardOffset{};
    f32_t sideOffset{};
    bool_t siegeOnly{};
};

struct MinionWaveDef
{
    static constexpr u8_t kMaxSpawnSlotCount = 8u;
    u64_t waveIntervalTicks{};
    u64_t initialDelayTicks{};
    u64_t perMinionDelayTicks{};
    u32_t siegeWavePeriod{};
    u32_t timeGrowthCapMinutes{};
    f32_t timeGrowthPerMinute{};
    f32_t corpseDeathTimerSec{};
    f32_t fallbackWaveStartX{};
    MinionWaveRangedProjectileDef rangedProjectile{};
    MinionBehaviorDef behavior{};
    MinionWaveSpawnSlotDef spawnSlots[kMaxSpawnSlotCount]{};
    u8_t spawnSlotCount{};
};
~~~

MinionWaveRangedProjectileDef의 non-zero defaults도 {}로 교체한다.

## 2-12. C:/Users/user/Desktop/Winters/Data/LoL/ServerPrivate/Gameplay/SpawnObjectGameplayDefs.json

기존 minionWave object에 아래 값을 병합한다.

~~~json
{
  "fallbackWaveStartX": 5.0,
  "behavior": {
    "pathAgentRadius": 0.5,
    "laneClearanceRadius": 0.5,
    "softSeparationRadiusScale": 0.65,
    "softSeparationWeight": 0.35,
    "softSeparationMaxStep": 0.18,
    "lanePathRebuildIntervalSec": 1.0,
    "chasePathRebuildIntervalSec": 0.2,
    "pathTargetRefreshDistanceSq": 0.1225,
    "pathWaypointArriveRadius": 0.35,
    "pathBuildBudgetPerTick": 4,
    "blockedFramesBeforeRepath": 6,
    "flowFieldStallFramesBeforePathFallback": 4,
    "flowFieldProgressSlackSq": 0.01,
    "structureAcquireRangePadding": 0.75,
    "targetScanIntervalSec": 0.15,
    "targetScanStaggerBuckets": 10,
    "rangedRoleType": 1,
    "attackExitRangePadding": 0.18,
    "meleeAttackWindupSec": 0.366666645,
    "rangedAttackWindupSec": 0.466666639,
    "attackRecoverySec": 0.366666645
  },
  "spawnSlots": [
    { "roleType": 0, "forwardOffset": 3.6, "sideOffset": -0.9, "siegeOnly": false },
    { "roleType": 0, "forwardOffset": 4.8, "sideOffset": 0.0, "siegeOnly": false },
    { "roleType": 0, "forwardOffset": 6.0, "sideOffset": 0.9, "siegeOnly": false },
    { "roleType": 1, "forwardOffset": 0.0, "sideOffset": -0.9, "siegeOnly": false },
    { "roleType": 1, "forwardOffset": 1.2, "sideOffset": 0.0, "siegeOnly": false },
    { "roleType": 1, "forwardOffset": 2.4, "sideOffset": 0.9, "siegeOnly": false },
    { "roleType": 2, "forwardOffset": 7.2, "sideOffset": 0.0, "siegeOnly": true }
  ]
}
~~~

위 세 timing 값은 현재 f32 연산 0.22f/0.6f, 0.28f/0.6f의 round-trip decimal이다. extractor가 bit parity를 재검증한다.

## 2-13. C:/Users/user/Desktop/Winters/Server/Private/Game/ServerMinionWaveRuntime.cpp

EnqueueWave 안의 local MinionSpawnSlot, kRole*, kSpawnSlots, kSiegeSlot을 삭제한다. waveDef.spawnSlots만 순회하고 siegeOnly는 bSiegeWave일 때만 enqueue한다. fallback 위치는 waveDef.fallbackWaveStartX를 사용한다. dueTick은 실제 enqueue 순서 index로 계산한다.

기존 코드:

~~~cpp
	const MinionWaveDef& waveDef =
		ServerData::GetActiveLoLSpawnObjectDefinitionPack().minionWave;
~~~

아래로 교체:

~~~cpp
	const MinionWaveDef& waveDef =
		ServerData::GetActiveLoLSpawnObjectDefinitionPack().minionWave;
	if (waveDef.spawnSlotCount == 0u)
		return;
~~~

## 2-14. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomUnitAI.cpp

모든 ServerMinionTuning::k* read를 active SpawnObject pack의 minionWave.behavior에서 읽는다. 기존 float 연산 순서를 바꾸지 않는다.

Phase_ServerUnitAI 시작에서 behavior를 한 번 resolve하고, path budget부터 같은 reference를 사용한다.

기존 코드:

~~~cpp
    u32_t PathBuildBudget = ServerMinionTuning::kPathBuildBudgetPerTick;
~~~

아래로 교체:

~~~cpp
    const MinionBehaviorDef& behavior =
        ServerData::GetActiveLoLSpawnObjectDefinitionPack().minionWave.behavior;
    u32_t PathBuildBudget = behavior.pathBuildBudgetPerTick;
~~~

namespace helper와 TryMoveServerMinionToward/TryMoveServerMinionByFlowFields/TryResolveMinionMoveStep에는 const MinionBehaviorDef& parameter를 전달한다.

CONFIRM_NEEDED: 이 파일의 27개 read를 모두 포함한 완전 signature propagation block은 P6 세부 plan에서 작성한다. helper 내부에서 active global pack을 반복 조회하는 두 번째 경로를 만들지 않는다.

## 2-14a. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomNav.cpp

기존 코드:

~~~cpp
    m_pPathNavGrid = m_pNavGrid->BuildInflated(ServerMinionTuning::kPathAgentRadius);
    m_pMinionLaneNavGrid = m_pNavGrid->BuildInflated(ServerMinionTuning::kMinionLaneClearanceRadius);
~~~

아래로 교체:

~~~cpp
    const MinionBehaviorDef& behavior =
        ServerData::GetActiveLoLSpawnObjectDefinitionPack().minionWave.behavior;
    m_pPathNavGrid = m_pNavGrid->BuildInflated(behavior.pathAgentRadius);
    m_pMinionLaneNavGrid = m_pNavGrid->BuildInflated(behavior.laneClearanceRadius);
~~~

## 2-15. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomSpawn.cpp

minion attackWindup/recovery/scan/stagger는 minionWave.behavior에서 읽는다. AI 구성은 active gameplay pack을 사용한다.

기존 코드:

~~~cpp
    ChampionAIComponent ai{};
    const ChampionAIProfile& profile = GetChampionAIProfile(slot.champion);
~~~

아래로 교체:

~~~cpp
    ChampionAIComponent ai{};
    const GameplayDefinitionPack& definitions =
        ServerData::GetActiveLoLGameplayDefinitionPack();
    const ChampionAIProfile* pProfile =
        definitions.FindChampionAIProfile(slot.champion);
    if (!pProfile)
        return;
    const ChampionAIProfile& profile = *pProfile;
~~~

## 2-16. C:/Users/user/Desktop/Winters/Server/Public/Game/ServerMinionTuning.h

2-11~2-15 reader 전환 후 파일 전체를 삭제한다. 삭제 전 rg "ServerMinionTuning::" 결과 0을 요구한다.

## 2-17. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h

ChampionAIProfile/combo shape의 product default initializer를 {}로 바꾼다. ChampionAIComboPlan 다음에 추가:

~~~cpp
struct ChampionAIComboPlanEntry
{
    eChampion champion = eChampion::END;
    ChampionAIComboPlan value{};
};
~~~

삭제할 선언:

~~~cpp
const ChampionAIProfile& GetChampionAIProfile(eChampion champion);
const ChampionAIComboPlan& GetChampionAIComboPlan(eChampion champion);
~~~

shadow policy artifact/evaluator는 남긴다.

## 2-18. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/GameplayDefinitionPack.h

GameplayDefinitionPack 앞에 추가:

~~~cpp
struct ChampionAIProfile;
struct ChampionAIComboPlan;
struct ChampionAIComboPlanEntry;
struct RuneGameplayDef;
~~~

itemCount 아래에 추가:

~~~cpp
    const ChampionAIProfile* aiProfiles = nullptr;
    std::size_t aiProfileCount = 0u;
    const ChampionAIComboPlanEntry* aiComboPlans = nullptr;
    std::size_t aiComboPlanCount = 0u;
    const RuneGameplayDef* runes = nullptr;
    std::size_t runeCount = 0u;
~~~

method 목록 아래에 추가:

~~~cpp
    const ChampionAIProfile* FindChampionAIProfile(eChampion champion) const;
    const ChampionAIComboPlan* FindChampionAIComboPlan(eChampion champion) const;
    const RuneGameplayDef* FindRune(u8_t legacyRuneId) const;
    bool_t HasCompleteRuntimeCoverage() const;
~~~

FindEconomy/FindItems의 legacy fallback 주석을 삭제한다.

## 2-19. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/GameplayDefinitionPack.cpp

AI/rune lookup을 deterministic linear lookup으로 구현한다. HasCompleteRuntimeCoverage는 manifest의 expected count와 source set equality를 검사한다. 17/85를 런타임 코드에 다시 박지 않는다.

기존 코드:

~~~cpp
const EconomyGameplayDef* GameplayDefinitionPack::FindEconomy() const
{
    return (economy && economy->bValid) ? economy : nullptr;
}
~~~

아래로 교체:

~~~cpp
const EconomyGameplayDef* GameplayDefinitionPack::FindEconomy() const
{
    return economy;
}
~~~

AI/rune lookup은 기존 FindChampion(eChampion)과 같은 linear loop 형식을 따른다. HasCompleteRuntimeCoverage의 complete body는 manifest expected count field가 확정될 때 CONFIRM_NEEDED다.

## 2-20. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.cpp

MakeDefaultProfile부터 GetChampionAIComboPlan 끝까지 삭제한다. 그 뒤 shadow policy decoder/evaluator는 유지한다.

## 2-21. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp

모든 GetChampionAIProfile/GetChampionAIComboPlan을 tc.pDefinitions lookup으로 교체한다. missing profile/combo에서는 command를 만들지 않고 MissingDefinition debug reason을 기록한다. default combo fallback은 없다.

예시:

~~~cpp
        if (!tc.pDefinitions)
            return false;
        const ChampionAIProfile* pProfile =
            tc.pDefinitions->FindChampionAIProfile(champion.id);
        if (!pProfile)
            return false;
        const ChampionAIProfile& profile = *pProfile;
~~~

Bot AI는 계속 GameCommand 생산자이며 gameplay truth를 직접 변경하지 않는다.

## 2-22. C:/Users/user/Desktop/Winters/Client/Private/UI/AIDebugPanel.cpp

삭제:

~~~cpp
		const ChampionAIProfile& profile = GetChampionAIProfile(selectedAI.champion.id);
~~~

기존 profile 출력은 replicated debug 값으로 교체:

~~~cpp
		ImGui::Text("Authoritative range: champ %.1f   minion %.1f   structure %.1f   leash %.1f",
			debug.fChampionScanRange,
			debug.fMinionScanRange,
			debug.fStructureScanRange,
			debug.fLeashRange);
~~~

## 2-23. C:/Users/user/Desktop/Winters/Data/LoL/ServerPrivate/AI/ChampionAIGameplayDefs.json

새 파일. MakeDefaultProfile, 17 Make*Profile, default/Jax/Fiora/Ashe/Riven/LeeSin/Sylas combo를 byte-identical로 옮긴다.

CONFIRM_NEEDED: 전체 본문은 one-shot extractor가 C++ aggregate를 JSON으로 출력하고 SimLab parity를 통과한 뒤 확정한다. 수동 전사로 재입력하지 않는다.

최종 root contract:

~~~json
{
  "schemaVersion": 1,
  "defaultProfile": {},
  "defaultCombo": [],
  "profiles": [],
  "comboOverrides": []
}
~~~

## 2-24. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/RuneComponent.h

RuneTuning namespace를 삭제하고 eRuneId 아래에 추가:

~~~cpp
struct RuneGameplayDef
{
    DefinitionKey key = kInvalidDefinitionKey;
    eRuneId legacyRuneId = eRuneId::None;
    bool_t bEnabled{};
    u8_t maxStacks{};
};
~~~

필요 include:

~~~cpp
#include "Shared/GameSim/Definitions/DefinitionIds.h"
~~~

현재 kLethalTempoAttackSpeedPerStack은 StatSystem reader가 없는 dead tuning 상수다. 이를 pack reader로 새로 연결하면 gameplay 변경이므로 삭제만 하고, 실제 공속 효과 구현은 별도 기능 plan으로 분리한다.

## 2-25. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/Rune/RuneSystem.cpp

OnBasicAttackHitChampion에서 tc.pDefinitions->FindRune로 maxStacks를 읽는다.

기존 코드:

~~~cpp
	(void)tc;

	if (!AreEnemyChampions(world, source, target) ||
		!HasRune(world, source, eRuneId::LethalTempo))
		return;

	RuneRuntimeComponent& runtime = EnsureRuneRuntime(world, source);
	if (runtime.iLethalTempoStacks >= RuneTuning::kLethalTempoMaxStacks)
		return;
~~~

아래로 교체:

~~~cpp
	if (!AreEnemyChampions(world, source, target) ||
		!HasRune(world, source, eRuneId::LethalTempo) ||
		!tc.pDefinitions)
	{
		return;
	}

	const RuneGameplayDef* pRune =
		tc.pDefinitions->FindRune(static_cast<u8_t>(eRuneId::LethalTempo));
	if (!pRune || !pRune->bEnabled)
		return;

	RuneRuntimeComponent& runtime = EnsureRuneRuntime(world, source);
	if (runtime.iLethalTempoStacks >= pRune->maxStacks)
		return;
~~~

## 2-26. C:/Users/user/Desktop/Winters/Data/LoL/ServerPrivate/Gameplay/RuneGameplayDefs.json

새 파일 전체 내용:

~~~json
{
  "schemaVersion": 1,
  "runes": [
    {
      "key": "rune.lethal_tempo",
      "legacyRuneId": 1,
      "enabled": true,
      "maxStacks": 5
    },
    {
      "key": "rune.electrocute",
      "legacyRuneId": 2,
      "enabled": false,
      "maxStacks": 0
    },
    {
      "key": "rune.adaptive_force",
      "legacyRuneId": 3,
      "enabled": false,
      "maxStacks": 0
    }
  ]
}
~~~

## 2-27. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/ItemDef.h

기존 API:

~~~cpp
    void LoadFromItemDefs(const ItemDef* items, std::size_t count);
    void ResetToDefaults();
~~~

아래로 교체:

~~~cpp
    bool_t LoadFromItemDefs(const ItemDef* items, std::size_t count);
    void Clear();
~~~

## 2-28. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/ItemRegistry.cpp

상단 kItems[34]를 삭제한다. constructor는 비워 두며 LoadFromItemDefs null/0은 false, 성공은 전체 교체 후 true다. Clear는 vector를 비운다.

기존 코드:

~~~cpp
CItemRegistry::CItemRegistry()
{
    ResetToDefaults();
}
~~~

아래로 교체:

~~~cpp
CItemRegistry::CItemRegistry() = default;
~~~

기존 코드:

~~~cpp
void CItemRegistry::LoadFromItemDefs(const ItemDef* items, std::size_t count)
{
    if (!items || count == 0u)
        return;
    m_Items.assign(items, items + count);
}

void CItemRegistry::ResetToDefaults()
{
    m_Items.assign(kItems, kItems + sizeof(kItems) / sizeof(kItems[0]));
}
~~~

아래로 교체:

~~~cpp
bool_t CItemRegistry::LoadFromItemDefs(const ItemDef* items, std::size_t count)
{
    if (!items || count == 0u)
        return false;
    m_Items.assign(items, items + count);
    return true;
}

void CItemRegistry::Clear()
{
    m_Items.clear();
}
~~~

## 2-29. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/GameplayDefinitionQuery.cpp

ChampionGameDataDB include와 fallback return을 제거한다. miss는 ReportMissingGameplayDefinition과 counter를 남기고 cast/spawn rejection용 invalid sentinel을 반환한다.

기존 예:

~~~cpp
        return ChampionGameDataDB::ResolveSkillCooldown(
            ResolveLegacyFallbackChampion(world, entity, fallbackChampion, "skill-cooldown", slot),
            slot);
~~~

아래로 교체:

~~~cpp
        ReportMissingGameplayDefinition(
            world,
            entity,
            fallbackChampion,
            slot,
            "skill-cooldown");
        return 0.f;
~~~

## 2-30. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/ChampionRuntimeDefaults.cpp

gameplay reader는 GameplayDefinitionQuery로, champion visual yaw는 ClientPublic modelYawOffset으로 옮긴다. 순수 수학 helper를 기존 WintersMath/SkillAtomData owner로 옮긴 뒤 header/cpp를 P8에서 삭제한다.

CONFIRM_NEEDED: 순수 helper의 최종 owner는 Shared include graph 재검사 후 확정한다.

## 2-31. C:/Users/user/Desktop/Winters/Data/LoL/ClientPublic/Visual/ChampionAssetVisualDefs.json

models 7개를 17개로 채운다. UI 17개는 유지한다. 누락 10챔피언 값은 registration과 resource existence audit로 byte-identical 추출한다.

CONFIRM_NEEDED: 누락 10챔피언 전체 JSON은 실제 asset 경로를 확인한 뒤 붙인다. Build-LoLDefinitionPack.py에 model set == gameplay champion set 검증을 먼저 추가한다.

## 2-32. C:/Users/user/Desktop/Winters/Client/Private/GameObject/ChampionTable.cpp

GetChampionDisplayName switch fallback을 삭제한다. generated pack miss는 Unknown과 bounded failure로 보인다. 17/17 전환 후 accessor를 LoLVisualDefinitionPack에 흡수하고 P8에서 파일을 삭제한다.

기존 GetChampionDisplayName의 switch 전체:

~~~cpp
    switch (champ)
    {
    case eChampion::IRELIA: return "Irelia";
    case eChampion::YASUO: return "Yasuo";
    case eChampion::KALISTA: return "Kalista";
    case eChampion::SYLAS: return "Sylas";
    case eChampion::VIEGO: return "Viego";
    case eChampion::ANNIE: return "Annie";
    case eChampion::ASHE: return "Ashe";
    case eChampion::FIORA: return "Fiora";
    case eChampion::GAREN: return "Garen";
    case eChampion::RIVEN: return "Riven";
    case eChampion::ZED: return "Zed";
    case eChampion::EZREAL: return "Ezreal";
    case eChampion::YONE: return "Yone";
    case eChampion::JAX: return "Jax";
    case eChampion::MASTERYI: return "MasterYi";
    case eChampion::KINDRED: return "Kindred";
    case eChampion::LEESIN: return "LeeSin";
    default: return "(unnamed)";
    }
~~~

아래로 교체:

~~~cpp
    return "(unnamed)";
~~~

## 2-33. C:/Users/user/Desktop/Winters/Client/Private/GameObject/SkillTable.cpp

static s_SkillTable 85 record를 삭제한다. SkillRegistry가 generated ClientPublic record를 만들고 champion registration은 hook/callback만 추가한다.

삭제 범위는 static SkillDef s_SkillTable[] 선언 시작부터 g_SkillCount 선언까지다. FindSkillDef는 generated ClientData lookup을 호출하도록 교체한다.

CONFIRM_NEEDED: generated ClientPublic SkillDef adapter의 최종 API가 아직 없으므로 FindSkillDef 교체 함수의 완전 body는 P4 세부 plan에서 작성한다.

## 2-34. C:/Users/user/Desktop/Winters/Tools/LoLData/Build-LoLDefinitionPack.py

기존 코드:

~~~python
parser = argparse.ArgumentParser()
parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
parser.add_argument("--check", action="store_true")
~~~

아래에 추가:

~~~python
parser.add_argument("--apply-draft", type=Path)
parser.add_argument("--schema-dir", type=Path)
~~~

source 추가:

~~~python
ai_source = root / "Data" / "LoL" / "ServerPrivate" / "AI" / "ChampionAIGameplayDefs.json"
rune_source = root / "Data" / "LoL" / "ServerPrivate" / "Gameplay" / "RuneGameplayDefs.json"
~~~

검증/생성 규칙:

1. gameplay champion set == visual set == asset model set == UI set == AI set.
2. rank array/finite/range/unknown field/duplicate DefinitionKey를 거부.
3. product default로 누락 field를 채우지 않는다.
4. 모든 source를 build hash에 포함.
5. --check는 source/report를 쓰지 않는다.
6. normal codegen도 canonical source를 재직렬화하지 않는다.
7. --apply-draft만 source write를 하며 stale baseBuildHash를 거부.
8. temp tree 검증 후 atomic replace.
9. Server generated pack에 AI/Rune emit, Client에는 ServerPrivate 값 미포함.

CONFIRM_NEEDED: normalize_ai_root, normalize_rune_root, apply_tuning_draft, schema emitter의 완전한 함수 body는 P5/P9 세부 plan에서 작성한다.

## 2-35. C:/Users/user/Desktop/Winters/Server/Private/Data/RuntimeGameplayDefinitionOverlay.cpp

RuntimePackStorage에 AI profile/combo와 rune vector를 추가한다. reload sources에 AI/Rune JSON을 추가한다. 모든 source parse/apply 성공 전에는 active pointer나 ItemRegistry를 변경하지 않는다.

기존 sources[]의 마지막 항목:

~~~cpp
            { L"Data\\LoL\\ServerPrivate\\Gameplay\\SpawnObjectGameplayDefs.json",
                "SpawnObjectGameplayDefs.json", &ApplySpawnObjectJson },
~~~

아래에 추가:

~~~cpp
            { L"Data\\LoL\\ServerPrivate\\AI\\ChampionAIGameplayDefs.json",
                "ChampionAIGameplayDefs.json", &ApplyChampionAIJson },
            { L"Data\\LoL\\ServerPrivate\\Gameplay\\RuneGameplayDefs.json",
                "RuneGameplayDefs.json", &ApplyRuneJson },
~~~

CONFIRM_NEEDED: ApplyChampionAIJson/ApplyRuneJson과 RuntimePackStorage vector 추가의 완전 body는 P5/P9 세부 plan에서 작성한다.

## 2-36. C:/Users/user/Desktop/Winters/Client/Private/UI/ChampionTuner.cpp

P9에서 generated tuning metadata로 Champion/Skill formula/Economy/Item/Rune/AI/Minion/Structure/Jungle 전 domain을 표시한다. Client는 typed Practice command만 보내고 Server가 session override를 소유한다.

draft row contract:

~~~json
{
  "baseBuildHash": "0x00000000",
  "baseRuntimeRevision": 0,
  "domain": "skillEffect",
  "definitionKey": "skill.zed.q",
  "field": "damage.flatByRank",
  "index": 0,
  "value": 80.0
}
~~~

canonical apply는 Build-LoLDefinitionPack.py --apply-draft로만 한다.

CONFIRM_NEEDED: metadata packet/typed Practice operation schema가 아직 확정되지 않았으므로 Render의 완전 교체 body는 P9 세부 plan에서 작성한다.

## 2-37. C:/Users/user/Desktop/Winters/Client/Private/UI/SkillTimingPanel.cpp

SaveVisualDraft에 ClientPublic manifest base hash를 기록한다. visual draft도 P9 공통 apply-draft contract를 사용한다. “Drafts are not release truth” 문구는 유지한다.

기존 코드:

~~~cpp
        root["source"] = "Data/LoL/ClientPublic/Visual/ChampionVisualDefs.json";
        root["scope"] = "visual-only-debug-draft";
~~~

아래에 추가:

~~~cpp
        root["baseBuildHash"] =
            ClientData::GetLoLClientVisualDefinitionPack().manifest.uBuildHash;
~~~

CONFIRM_NEEDED: 실제 ClientData accessor 이름은 LoLVisualDefinitionPack.h의 최종 public API를 확인해 확정한다.

## 2-38. C:/Users/user/Desktop/Winters/Shared/Schemas/Snapshot.fbs

EntitySnapshot 마지막에 append:

~~~fbs
    championDefinitionKey:uint;
    baseChampionDefinitionKey:uint;
    visualChampionDefinitionKey:uint;
    skillChampionDefinitionKey:uint;
    spellbookChampionDefinitionKey:uint;
    actionSourceChampionDefinitionKey:uint;
~~~

writer가 한시적으로 byte+key를 함께 쓰고 reader는 key 우선이다. P7 종료 시 runtime legacy read를 삭제하고 old field는 reserved/deprecated로 남긴다.

## 2-39. C:/Users/user/Desktop/Winters/Shared/Schemas/Hello.fbs

table 마지막에 추가:

~~~fbs
    championDefinitionKey:uint;
~~~

## 2-40. C:/Users/user/Desktop/Winters/Shared/Schemas/LobbyTypes.fbs

LobbySlot 마지막에 추가:

~~~fbs
    championDefinitionKey:uint;
~~~

## 2-41. C:/Users/user/Desktop/Winters/Shared/Schemas/LobbyCommand.fbs

LobbyCommand 마지막에 추가:

~~~fbs
    championDefinitionKey:uint;
~~~

P7 종료 시 LobbyAuthority와 Client writer가 key만 읽고 쓴다.

## 2-41a. C:/Users/user/Desktop/Winters/Client/Private/Data/LoLVisualDefinitionPack.h

wire에서 받은 stable key를 현재 Client의 `eChampion` 기반 렌더 경계로 한 번만 변환할 API를 추가한다. network reader가 byte ID를 직접 cast하지 않게 만드는 adapter이며, source of truth는 generated ClientPublic pack이다.

기존 API:

~~~cpp
    const ChampionVisualDefinition* FindChampionVisualDefinition(eChampion champion);
    f32_t ResolveChampionModelYawOffset(eChampion champion);
    const ChampionModelVisualPack& GetChampionModelVisualPack();
    const ChampionModelVisualDefinition* FindChampionModelVisualDefinition(eChampion champion);
    const ChampionUiVisualDefinition* FindChampionUiVisualDefinition(eChampion champion);
~~~

아래로 교체:

~~~cpp
    const ChampionVisualDefinition* FindChampionVisualDefinition(DefinitionKey key);
    const ChampionVisualDefinition* FindChampionVisualDefinition(eChampion champion);
    eChampion ResolveLegacyChampion(DefinitionKey key);
    f32_t ResolveChampionModelYawOffset(DefinitionKey key);
    f32_t ResolveChampionModelYawOffset(eChampion champion);
    const ChampionModelVisualPack& GetChampionModelVisualPack();
    const ChampionModelVisualDefinition* FindChampionModelVisualDefinition(DefinitionKey key);
    const ChampionModelVisualDefinition* FindChampionModelVisualDefinition(eChampion champion);
    const ChampionUiVisualDefinition* FindChampionUiVisualDefinition(DefinitionKey key);
    const ChampionUiVisualDefinition* FindChampionUiVisualDefinition(eChampion champion);
~~~

`Client/Private/Data/Generated/LoLVisualDefinitions.generated.cpp`는 직접 편집하지 않는다. `Build-LoLDefinitionPack.py` generator가 key lookup과 `ResolveLegacyChampion` 구현을 생성한다. unknown/zero key는 `eChampion::END`이며 Yasuo 등의 임의 fallback을 금지한다.

## 2-41b. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/LoLMatchContext.h

Client lobby state가 wire key를 보존하도록 include와 필드를 추가한다. `champion`/`SelectedChampion`은 P7 동안 renderer/legacy scene adapter 결과로만 유지하고 wire truth로 사용하지 않는다.

상단에 추가:

~~~cpp
#include "Shared/GameSim/Definitions/DefinitionIds.h"
~~~

`GameRosterSlot::champion` 바로 위에 추가:

~~~cpp
    DefinitionKey championDefinitionKey = kInvalidDefinitionKey;
~~~

`MatchContext::SelectedChampion` 바로 위에 추가:

~~~cpp
    DefinitionKey SelectedChampionDefinitionKey = kInvalidDefinitionKey;
~~~

P8에서 scene/loader/spawner가 key overload를 소비하도록 바꾼 뒤 두 legacy champion 필드의 삭제 가능성을 별도 compile gate로 판정한다. P7 성공 조건은 삭제 자체가 아니라 “wire byte를 truth로 읽는 곳 0”이다.

## 2-41c. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomLobby.cpp

이미 include된 `RuntimeGameplayDefinitionOverlay.h`의 active pack으로 lobby/hello writer key를 계산한다.

`BroadcastLobbyStateLocked`의 `seatKind` 아래에 추가:

~~~cpp
        const ChampionGameplayDef* pChampionDef =
            ServerData::GetActiveLoLGameplayDefinitionPack().pack.FindChampion(slot.champion);
        const DefinitionKey championDefinitionKey =
            pChampionDef ? pChampionDef->key : kInvalidDefinitionKey;
~~~

FlatBuffers 재생성 후 `CreateLobbySlot` 마지막 인수에 추가:

~~~cpp
            slot.bLocked,
            championDefinitionKey));
~~~

`SendHelloToSessionLocked`의 builder 생성 아래에 추가:

~~~cpp
    const ChampionGameplayDef* pChampionDef =
        ServerData::GetActiveLoLGameplayDefinitionPack().pack.FindChampion(champion);
    const DefinitionKey championDefinitionKey =
        pChampionDef ? pChampionDef->key : kInvalidDefinitionKey;
~~~

`CreateHello` 마지막 인수에 추가:

~~~cpp
        ServerData::GetRuntimeGameplayDefinitionRevision(),
        championDefinitionKey);
~~~

non-empty champion slot에서 key가 0이면 packet을 보내지 않고 bounded `OutputDebugStringA`/server trace로 실패시킨다. 이 검증을 통해 “byte는 있는데 key가 없는” 혼합 상태를 조용히 허용하지 않는다.

## 2-41d. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/GameSessionClient.cpp

`SendLobbyCommand`에서 선택된 legacy enum을 ClientPublic generated pack의 key로 변환해 전송한다. `CreateLobbyCommand` 마지막 인수에 `championDefinitionKey`를 추가한다. `PickChampion`/`SetBotChampion`인데 lookup이 실패하면 frame을 보내지 않고 `client reject: unknown champion definition key`를 기록한다.

`OnLobbyState`의 기존 코드:

~~~cpp
			dst.champion = static_cast<eChampion>(src->championId());
~~~

아래로 교체:

~~~cpp
			dst.championDefinitionKey = src->championDefinitionKey();
			dst.champion = ClientData::ResolveLegacyChampion(dst.championDefinitionKey);
~~~

local human block의 `SelectedChampion` 대입 바로 위에 추가:

~~~cpp
				m_lobbyContext.SelectedChampionDefinitionKey = dst.championDefinitionKey;
~~~

`OnHello`의 기존 코드:

~~~cpp
	const eChampion helloChampion =
		static_cast<eChampion>(hello->championId());
~~~

아래로 교체:

~~~cpp
	const DefinitionKey helloChampionDefinitionKey = hello->championDefinitionKey();
	const eChampion helloChampion =
		ClientData::ResolveLegacyChampion(helloChampionDefinitionKey);
~~~

`MyNetId` 대입 다음에 추가:

~~~cpp
	m_lobbyContext.SelectedChampionDefinitionKey = helloChampionDefinitionKey;
~~~

P7 migration build에서도 `src->championId()`/`hello->championId()` fallback은 두지 않는다. old server/client 혼용은 manifest/protocol mismatch로 명시적으로 차단한다.

CONFIRM_NEEDED: `SendLobbyCommand`의 generated `CreateLobbyCommand` 최종 인수 순서는 `.fbs` append 후 `flatc`가 만든 signature로 확인한다.

## 2-41e. C:/Users/user/Desktop/Winters/Server/Private/Game/LobbyAuthority.cpp

`ApplyCommand`에서 switch 전 한 번만 key를 해석한다.

`m_lastMessage.clear();` 아래에 추가:

~~~cpp
    const DefinitionKey championDefinitionKey = command->championDefinitionKey();
    const ChampionGameplayDef* pChampionDef =
        ServerData::GetActiveLoLGameplayDefinitionPack().pack.FindChampion(championDefinitionKey);
    const eChampion requestedChampion =
        pChampionDef ? pChampionDef->legacyChampion : eChampion::END;
~~~

모든 `static_cast<eChampion>(command->championId())`를 `requestedChampion`으로 교체한다. `PickChampion`/`SetBotChampion`에서 key가 0, unknown, 또는 disabled champion이면 `unknown champion definition key`로 reject하고 revision/log에는 requested key도 남긴다. non-champion command는 key 0을 허용한다.

필요 include:

~~~cpp
#include "Server/Private/Data/RuntimeGameplayDefinitionOverlay.h"
~~~

P7 종료 grep gate:

~~~powershell
rg -n 'command->championId\(\)' Server/Private/Game/LobbyAuthority.cpp
~~~

출력 0줄이어야 한다.

## 2-41f. C:/Users/user/Desktop/Winters/Server/Private/Game/SnapshotBuilder.cpp

`CreateEntitySnapshot` 직전 active pack을 기준으로 여섯 legacy ID의 stable key를 계산한다. entity에 이미 `ChampionDefinitionComponent`가 있으면 primary key는 그 component를 우선하고, component와 legacy lookup 결과가 다르면 snapshot을 fail-fast한다.

추가할 local helper contract:

~~~cpp
        const auto resolveChampionDefinitionKey = [](u8_t legacyChampionId) -> DefinitionKey
        {
            if (legacyChampionId == 0u)
                return kInvalidDefinitionKey;
            const ChampionGameplayDef* pDefinition =
                ServerData::GetActiveLoLGameplayDefinitionPack().pack.FindChampion(
                    static_cast<eChampion>(legacyChampionId));
            return pDefinition ? pDefinition->key : kInvalidDefinitionKey;
        };

        const DefinitionKey championDefinitionKey =
            resolveChampionDefinitionKey(championId);
        const DefinitionKey baseChampionDefinitionKey =
            resolveChampionDefinitionKey(baseChampionId);
        const DefinitionKey visualChampionDefinitionKey =
            resolveChampionDefinitionKey(visualChampionId);
        const DefinitionKey skillChampionDefinitionKey =
            resolveChampionDefinitionKey(skillChampionId);
        const DefinitionKey spellbookChampionDefinitionKey =
            resolveChampionDefinitionKey(spellbookChampionId);
        const DefinitionKey actionSourceChampionDefinitionKey =
            resolveChampionDefinitionKey(actionSourceChampionId);
~~~

FlatBuffers 재생성 후 `CreateEntitySnapshot` 마지막 `projectileTraveledDist` 뒤에 여섯 key를 schema와 같은 순서로 추가한다. P7 migration 동안 byte와 key를 함께 쓰되, P7 종료 후 Client reader는 여섯 byte를 읽지 않는다.

CONFIRM_NEEDED: `ChampionDefinitionComponent` primary-key 우선 코드의 정확한 위치는 `item.entity`의 component 접근 타입과 `World` const API를 P7 slice에서 확인한다. generated call 전체는 인수가 100개 이상이므로 손으로 재작성하지 않고 schema regenerate 뒤 compiler로 고정한다.

## 2-41g. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp

상단 component include에 추가:

~~~cpp
#include "Shared/GameSim/Components/ChampionDefinitionComponent.h"
~~~

Hello 경로에서 `hello->championId()`를 `hello->championDefinitionKey()`로 교체하고 `ClientData::ResolveLegacyChampion` 결과만 `EnsureEntity`의 기존 legacy adapter 인수로 전달한다. key가 0/unknown이면 entity 생성을 중단하고 bounded debug trace를 남긴다.

snapshot loop의 기존 코드:

~~~cpp
        const u8_t snapshotChampionId = es->championId();
        const u8_t snapshotVisualChampionId =
            es->visualChampionId() != 0u ? es->visualChampionId() : snapshotChampionId;
~~~

아래로 교체:

~~~cpp
        const DefinitionKey snapshotChampionDefinitionKey =
            es->championDefinitionKey();
        const DefinitionKey snapshotVisualChampionDefinitionKey =
            es->visualChampionDefinitionKey() != kInvalidDefinitionKey
                ? es->visualChampionDefinitionKey()
                : snapshotChampionDefinitionKey;
        const eChampion snapshotChampion =
            ClientData::ResolveLegacyChampion(snapshotChampionDefinitionKey);
        const eChampion snapshotVisualChampion =
            ClientData::ResolveLegacyChampion(snapshotVisualChampionDefinitionKey);
~~~

`EnsureEntity`의 champion 인수는 `static_cast<u8_t>(snapshotChampion)`로만 전달한다. entity 생성 직후 champion entity에는 아래 component를 add-or-update한다.

~~~cpp
        ChampionDefinitionComponent championDefinition{};
        championDefinition.championDefId = ChampionDefId{ snapshotChampionDefinitionKey };
~~~

이후 form/spellbook/action/stat 처리의 `es->baseChampionId()`, `visualChampionId()`, `skillChampionId()`, `spellbookChampionId()`, `actionSourceChampionId()`, `championId()` reader를 대응 key reader + ClientPublic adapter로 전부 교체한다. 최종 grep은 0줄이어야 한다.

~~~powershell
rg -n 'es->(championId|baseChampionId|visualChampionId|skillChampionId|spellbookChampionId|actionSourceChampionId)\(\)|hello->championId\(\)' Client/Private/Network/Client/SnapshotApplier.cpp
~~~

CONFIRM_NEEDED: ECS의 add-or-update helper 이름과 현재 Form/Spellbook component가 key 필드를 수용하는지 확인한 뒤 P7 상세 plan에서 각 exact replacement body를 확정한다. 없으면 component에도 `DefinitionKey`를 추가하고 legacy enum은 render adapter로만 보존한다.

## 2-41h. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameNetwork.cpp

snapshot callback/roster callback의 raw `u8 championId` 경계를 `DefinitionKey championDefinitionKey`로 바꾸고, model/UI lookup은 `LoLVisualDefinitionPack`의 key overload를 호출한다. `static_cast<eChampion>(championId)`는 P7 종료 시 0건이어야 한다.

CONFIRM_NEEDED: 이 파일의 callback signature가 `SnapshotApplier.h`, `Scene_InGame.h`와 함께 바뀌므로 P7 세부 plan에서 세 파일의 exact declaration/definition/call-site 교체 블록을 한 묶음으로 작성한다.

## 2-41i. FlatBuffers 생성물과 protocol/replay 경계

`.fbs` 네 파일 변경 뒤 repo의 기존 FlatBuffers generation command로 `Shared/Schemas/Generated/cpp/*_generated.h`를 재생성한다. 생성물을 손으로 고치지 않는다. Server/Client protocol version 또는 build manifest가 다르면 연결 전에 거부한다.

CONFIRM_NEEDED: 현재 저장 replay가 Snapshot raw FlatBuffer를 장기 보존하는지, 그리고 최소 지원 replay version이 무엇인지 정책 결정이 필요하다. 결론 전까지 old byte fallback을 runtime reader에 넣지 않고, fixture migration tool 또는 “구버전 replay 명시적 미지원” 중 하나를 P7 시작 전에 선택한다.

## 2-42. C:/Users/user/Desktop/Winters/Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.cpp

P8에서 reader 0 후 header/cpp/generated ChampionGameData와 Tools/ChampionData generator를 함께 삭제한다. Build-LoLDefinitionPack.py가 유일 generator가 된다.

## 2-43. C:/Users/user/Desktop/Winters/Shared/GameSim/Registries/ChampionStats/ChampionStatsRegistry.cpp

Client smoke와 ChampionSpawnService를 generated pack/query로 바꾼 뒤 header/cpp를 삭제한다. test override는 production registry가 아니라 injected TickContext definition fixture로 제공한다.

## 2-44. C:/Users/user/Desktop/Winters/Client/Include/Client.vcxproj

MainMenu 변경에는 새 파일 등록이 없다. P4/P8 삭제 파일과 P5/P9 신규 파일의 project 포함은 실제 slice에서 확인한다.

CONFIRM_NEEDED: 신규/삭제 파일의 vcxproj와 filters 동기화.

## 2-45. JSON Schema 산출물

P9에서 generator가 11 schema를 생성한다.

~~~text
ChampionGameplay.schema.json
SkillEffectGameplay.schema.json
SummonerSpellGameplay.schema.json
EconomyGameplay.schema.json
ItemGameplay.schema.json
SpawnObjectGameplay.schema.json
RuneGameplay.schema.json
ChampionAIGameplay.schema.json
ChampionVisual.schema.json
ObjectVisual.schema.json
ChampionAssetVisual.schema.json
~~~

CONFIRM_NEEDED: 새 schema 파일 전체 본문은 generator field descriptor 확정 후 생성 결과를 붙인다. 수동 schema라는 두 번째 진실을 만들지 않는다.

# 3. Phase별 종료 조건과 롤백

## P0 — 측정 게이트

- audit raw regex count와 실제 runtime reader count를 분리한다.
- --check/goal status/verifier가 worktree를 변경하지 않는다.
- 동일 commit에서 verifier 두 번 실행 결과와 git status가 같다.

## D0 — MainMenu 프로필 카드

- Scene_MainMenu에서 yasuo_square.png literal과 texture member가 0이다.
- MainMenu1.png 기존 프로필 카드가 그대로 보인다.
- portrait/name/status 클릭은 MyInfo로 한 번만 전환한다.
- settings/minimize/close 클릭은 MyInfo로 전환하지 않는다.
- MyInfo 뒤로가기는 MainMenu를 다시 만든다.

## P3 — Gameplay tail, 경제, 아이템, 룬

- product non-zero default와 gameplay fallback이 0이다.
- ItemRegistry kItems/ResetToDefaults가 없다.
- FormulaData/BORK/SkillRank/룬 SimLab가 통과한다.

## P4 — Client visual/assets

- model/UI/timing set가 gameplay 17챔피언 set와 일치한다.
- registration은 hook/behavior만 소유한다.
- Client에 ServerPrivate 값 복제가 없다.

## P5 — AI

- 17 profile과 combo parity가 byte-identical이다.
- Make*Profile/static combo가 0이다.
- same-seed bot command/state hash가 같다.
- Bot AI는 GameCommand 생산자다.

## P6 — Object/wave/map

- ServerMinionTuning reader 0 후 삭제.
- spawn slot 순서와 dueTick이 같다.
- Stage1.dat authored placement가 우선이며 code fallback placement는 0이다.

## P7 — DefinitionKey wire

- 모든 writer/reader가 key를 사용한다.
- old packet/replay 호환 기간 후 runtime legacy read 0이다.
- flatc codegen과 /m:1 build가 통과한다.

## P8 — Legacy 삭제

- 삭제 직전 각 reader count 0 report를 남긴다.
- generated pack 하나만 build hash와 값을 소유한다.
- old generator/DB/registry/table/fallback include가 project에서 제거된다.

## P9 — 기획자 workflow

- 11 JSON source schema autocomplete/range error가 동작한다.
- invalid reload는 기존 generation을 유지한다.
- valid reload는 revision을 증가시킨다.
- stale draft는 거부되고 valid draft round-trip은 의도 field만 바꾼다.

# 4. 검증

## 예측

- MainMenu에서 Yasuo texture load/draw가 사라지고 기존 프로필 카드가 보인다.
- 카드 클릭은 기존 CScene_MyInfo를 열며 profile/history/replay는 변하지 않는다.
- 순수 값 이관에서 same-seed hash는 불변이고 seed+1 hash는 다르다.
- missing definition은 fallback 없이 counter/rejection으로 보인다.
- AI 이관 후 command hash는 동일하고 Client panel은 replicated 값만 표시한다.
- network old field는 호환용으로 남을 수 있지만 final runtime reader는 0이다.

## 검증 명령

~~~powershell
rg -n "yasuo_square|m_pMyInfoPortrait|kMyInfoPortraitRect" Client/Private/Scene Client/Public/Scene
python Tools/LoLData/Build-LoLDefinitionPack.py --check
python Tools/ChampionData/build_champion_game_data.py --check
powershell -ExecutionPolicy Bypass -File Tools/LoLData/Collect-LoLLegacyDataAudit.ps1
powershell -ExecutionPolicy Bypass -File Tools/LoLData/Get-LoLDataDrivenGoalStatus.ps1 -NoWrite
powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1
powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1 -RequireComplete
git diff --check
~~~

빌드는 /m:1로 실행한다.

~~~powershell
msbuild Shared/GameSim/Include/GameSim.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /v:minimal
msbuild Server/Include/Server.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /v:minimal
msbuild Client/Include/Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /v:minimal
msbuild Tools/SimLab/SimLab.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /v:minimal
Tools/Bin/Debug/SimLab.exe
~~~

수동 smoke:

1. 1280x720, 1920x1080, ultrawide에서 MainMenu 프로필 카드와 window control 경계 클릭.
2. MyInfo profile/history/replay와 뒤로가기 확인.
3. normal F5 17챔피언 model/animation/FX 확인.
4. Debug reload로 damage, AI range, minion formation, visual timing 각 1회 수정 후 authoritative 결과 capture.

## 미검증

- 이 계획 시점에는 MainMenu 변경을 적용하지 않았다.
- 누락 10챔피언 model JSON과 17 AI profile JSON 전체는 extractor/parity 전이다.
- DefinitionKey old replay 최소 지원 version은 미정이다.
- P9 schema generator와 draft apply 함수는 세부 plan 전에는 컴파일 검증되지 않는다.

## 확인 필요

- MainMenu 가상 source rect 1275,0,1465,100의 첫 F5 capture.
- AI extractor와 current aggregate field parity.
- Stage1.dat fallback 제거 전 모든 정상 stage의 authored spawn/waypoint 완비 여부.
- P7 old replay 지원 종료 version.
- 신규/삭제 파일 vcxproj 및 filters 반영.
- 최종 완료 전 dirty worktree의 CDX11Device.cpp trailing whitespace 정리.

# 5. 최종 100% 체크리스트

- [ ] MainMenu hardcoded Yasuo portrait load/draw 0
- [ ] MainMenu existing profile card click → CScene_MyInfo
- [ ] canonical source 11개 schema coverage
- [ ] gameplay/visual/AI/object/rune source set coverage complete
- [ ] runtime pack miss fallback 0
- [ ] champion/gameplay tuning literal owner 0
- [ ] client gameplay/timing/asset duplicate owner 0
- [ ] AI profile/combo hardcode 0
- [ ] ServerMinionTuning/formation hardcode 0
- [ ] DefinitionKey runtime wire reader 100%
- [ ] legacy value-owner reader 0 및 삭제
- [ ] Debug reload 11 domain, partial publication 0
- [ ] stale draft reject + valid draft round-trip PASS
- [ ] GameSim/Server/Client/SimLab build PASS
- [ ] FormulaData/BORK/SkillRank/AI/wave/wire/reload SimLab PASS
- [ ] same-seed deterministic hash PASS
- [ ] normal F5 roster/map/minion/snapshot/UI/FX smoke PASS
- [ ] git diff --check PASS

적용 후 2026-07-17_DATA_DRIVEN_100_PERCENT_AND_MAIN_MENU_PROFILE_RESULT.md에 예측 대 실측, 판결, 대가 갱신만 기록한다.
