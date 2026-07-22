Session - F4 저장값을 canonical authoring truth로 복구하고 재발 방지 계약과 공개 main을 닫는다
좌표: 없음 · 축: C7 권위와 정합성, C8 검증이 병목
관련: 2026-07-20_F4_CHAMPION_RANKED_VARIANT_DAMAGE_CLOSURE_PLAN.md, 2026-07-20_F4_CHAMPION_RANKED_VARIANT_DAMAGE_CLOSURE_RESULT.md

## 1. 결정 기록

① 문제·제약: 공개 준비 중 F4가 저장한 3개 챔피언의 ranked 값 5개 배열, 15개 scalar leaf를 과거 계획서·테스트의 숫자로 되돌렸다. 원본 작업 폴더의 canonical JSON은 보존됐고 잘못된 커밋은 원격에 푸시되지 않았다.

② 순진한 해법의 실패: `Test-F4BalanceContracts.py`의 hard-coded 숫자를 회귀 oracle로 보면, 바로 앞의 canonical/generated parity 계약과 충돌하며 정상 F4 편집을 실패로 오판한다.

③ 메커니즘: 원본 작업 폴더의 현재 canonical JSON을 복구 기준으로 삼고, 검증은 값 자체가 아니라 schema·rank shape·domain·canonical/generated parity를 고정한다.

④ 대조: 과거 PLAN/RESULT 숫자는 당시 구현 기준선 기록이며 live authoring source가 아니다. exact 숫자 고정은 사용자가 명시적으로 balance baseline을 동결한 별도 테스트에서만 허용한다.

⑤ 대가: 임의 밸런스 변화 자체는 구조 테스트가 막지 않는다. 대신 F4 저장의 명시적 사용자 의도, git diff 리뷰, 생성물 parity가 변경 승인 경계가 된다. F4가 저장하는 네 canonical 파일 전체를 원본 작업 폴더와 normalized JSON equality로 비교해 선택 값 외의 의미도 그대로인지 검증한다.

## 2. 반영해야 하는 코드

### 2-1. `Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json`

현재 공개 작업 사본의 아래 Yasuo Q 블록을 원본 작업 폴더에 저장된 F4 값으로 교체한다.

기존 코드:

```json
"dashAreaDamage": [70.0, 90.0, 110.0, 130.0, 150.0],
"tornadoDamage": [100.0, 120.0, 140.0, 160.0, 180.0]
```

아래로 교체:

```json
"dashAreaDamage": [70.0, 90.0, 200.0, 250.0, 300.0],
"tornadoDamage": [100.0, 120.0, 200.0, 250.0, 300.0]
```

기존 코드:

```json
"flatByRank": [60.0, 80.0, 100.0, 120.0, 140.0]
```

아래로 교체:

```json
"flatByRank": [60.0, 80.0, 200.0, 250.0, 300.0]
```

현재 공개 작업 사본의 아래 Lee Sin Q2와 Kalista E variant 블록을 원본 작업 폴더의 F4 값으로 교체한다.

기존 코드:

```json
"baseDamage": [95.0, 95.0, 95.0, 95.0, 95.0]
```

아래로 교체:

```json
"baseDamage": [95.0, 95.0, 150.0, 200.0, 200.0]
```

기존 코드:

```json
"damagePerSpear": [30.0, 30.0, 30.0, 30.0, 30.0]
```

아래로 교체:

```json
"damagePerSpear": [30.0, 30.0, 40.0, 50.0, 60.0]
```

### 2-2. `Tools/LoLData/Test-F4BalanceContracts.py`

기존 코드:

```python
    ranked_variant_values = {
        ("skill.yasuo.q", "tornadoDamage"): [100, 120, 140, 160, 180],
        ("skill.yasuo.q", "dashAreaDamage"): [70, 90, 110, 130, 150],
        ("skill.leesin.q", "baseDamage"): [95, 95, 95, 95, 95],
        ("skill.kalista.e", "damagePerSpear"): [30, 30, 30, 30, 30],
        ("skill.ezreal.r", "nonEpicBaseDamage"): [150, 225, 300],
    }
    for (key, param_name), expected in ranked_variant_values.items():
        require_list(effect_by_key(effects, key)["params"][param_name], expected,
                     f"{key}.{param_name} ranked variant")
    require_list(effect_by_key(effects, "skill.yasuo.q")["damage"]["flatByRank"],
                 [60, 80, 100, 120, 140], "Yasuo Q1/Q2 flat damage")
```

아래로 교체:

```python
    ranked_variant_rank_counts = {
        ("skill.yasuo.q", "tornadoDamage"): 5,
        ("skill.yasuo.q", "dashAreaDamage"): 5,
        ("skill.leesin.q", "baseDamage"): 5,
        ("skill.kalista.e", "damagePerSpear"): 5,
        ("skill.ezreal.r", "nonEpicBaseDamage"): 3,
    }
    for (key, param_name), rank_count in ranked_variant_rank_counts.items():
        values = effect_by_key(effects, key)["params"][param_name]
        require(isinstance(values, list) and len(values) == rank_count,
                f"{key}.{param_name} ranked variant shape")
        require(all(isinstance(value, (int, float)) and
                    math.isfinite(float(value)) and float(value) >= 0.0
                    for value in values),
                f"{key}.{param_name} ranked variant domain")
```

`Yasuo params.baseDamage` 제거, Fiora range single-owner, Sylas magic damage처럼 구조·의미 소유권 검사는 유지한다. F4가 직접 편집하는 range, cooldown, damage, ranked variant, mechanics, minion attackRange, jungle/objective economy의 exact 숫자 fixture는 삭제하고 이미 존재하는 canonical/generated parity와 domain 검사를 사용한다.

기존 코드에서 아래 exact-value assertion을 삭제한다.

```python
    require_close(viego_w["rangeMax"], 5.0, "Viego W maximum range")
    require_close(fiora_w_skill["rangeMax"], 6.5, "Fiora W strip length")
    require_close(fiora_w_effect["params"]["radius"], 0.8,
                  "Fiora W half width")
    require_list(fiora_w_effect["damage"]["flatByRank"], [80] * 5,
                 "Fiora W canonical damage")
    require_close(sylas_q_effect["params"]["formationDelaySec"], 0.5,
                  "Sylas Q delay")
    require_close(sylas_q_effect["params"]["radius"], 1.65,
                  "Sylas Q radius")
    require_list(sylas_q_effect["damage"]["flatByRank"],
                 [70, 95, 120, 145, 170], "Sylas Q damage")
    require_close(sylas_w_effect["params"]["healDamageRatio"], 0.5,
                  "Sylas W heal ratio")
    require_list(sylas_w_effect["damage"]["flatByRank"],
                 [75, 100, 125, 150, 175], "Sylas W damage")
    require_close(lane_minions[1]["attackRange"], 5.6,
                  "ranged minion attack range")
    require_list(
        yasuo_e["cooldownSecByRank"],
        [0.1] * 5,
        "Yasuo E cooldown is 0.1 sec at every rank")
    require(economy["jungle"]["smallCampGold"] == 80.0 and
            economy["jungle"]["smallCampXP"] == 240.0,
            "regular jungle reward defaults")
    require(economy["objectives"]["teamGoldPerChampion"] == 2000.0 and
            economy["objectives"]["teamLevelGrant"] == 3,
            "objective shared team reward defaults")
```

`viego_w`, `fiora_w_skill`, `yasuo`, `yasuo_e`가 위 assertion 삭제로 사용되지 않으면 그 지역 변수만 함께 삭제한다. 이미 존재하는 전 챔피언 range/cooldown/damage rank/domain, 전 lane minion attackRange domain, canonical/generated parity 검사는 유지한다. Economy exact fixture를 대신해 F4가 노출하는 economy 필드가 유한한 비음수 숫자인지 확인하는 관계형 검사를 추가한다.

기존 `regular jungle reward defaults` / `objective shared team reward defaults` assertion을 삭제한 자리에 아래를 추가한다.

```python
    for section_name in ("jungle", "objectives"):
        for field, value in economy[section_name].items():
            if section_name == "objectives" and field == "teamLevelGrant":
                continue
            require(isinstance(value, (int, float)) and not isinstance(value, bool) and
                    math.isfinite(float(value)) and 0.0 <= float(value) <= 1000000.0,
                    f"{section_name}.{field} editable numeric domain")
    for field in ("elderBurnTickIntervalSec", "redBurnTickIntervalSec"):
        require(float(economy["objectives"][field]) >= 0.001,
                f"objectives.{field} positive interval")
    for field in ("elderBurnTargetMaxHpRatioPerTick", "elderExecuteThresholdRatio"):
        require(float(economy["objectives"][field]) <= 1.0,
                f"objectives.{field} ratio domain")
    team_level_grant = economy["objectives"]["teamLevelGrant"]
    require(isinstance(team_level_grant, int) and not isinstance(team_level_grant, bool) and
            0 <= team_level_grant <= 18,
            "objectives.teamLevelGrant integer domain")
```

### 2-3. `.claude/gotchas.md`

기존 코드:

```text
Format: `YYYY-MM-DD - [Area] mistake -> prevention rule/check`.
```

아래에 추가:

```text
- 2026-07-22 - [F4 canonical authoring] F4 `Save & Hot Load`가 저장한 canonical JSON을 과거 PLAN/RESULT의 예시 수치나 hard-coded 회귀 fixture로 덮으면 사용자의 최신 밸런스 작업이 유실된다 -> F4가 직접 저장하는 `Data/Gameplay/ChampionGameData/champions.json`, `Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json`, `SpawnObjectGameplayDefs.json`, `EconomyGameplayDefs.json`의 현재 저장본을 authoring truth로 취급한다. 같은 폴더의 `ChampionGameplayDefs.json`, `SkillGameplayDefs.json`과 generated C++/manifest는 authoring source가 아니다. 생성물 stale은 source rollback이 아니라 같은 source로 recook한다. F4 편집 가능 수치는 schema·domain·rank shape·canonical/generated parity로 검증하며 사용자가 명시적으로 baseline을 동결하지 않은 exact 숫자는 테스트에 고정하지 않는다.
```

### 2-4. `AGENTS.md`와 `CLAUDE.md`

각 문서의 `Codebase Compass` 섹션 아래에 같은 행동 규칙과 상세 문서 포인터를 추가한다.

아래에 추가:

```text
## Canonical Authoring Safety
- F4 `Save & Hot Load`가 저장하는 정확한 네 canonical JSON(`champions.json`, `SkillEffectGameplayDefs.json`, `SpawnObjectGameplayDefs.json`, `EconomyGameplayDefs.json`)은 과거 PLAN/RESULT, 테스트 fixture, generated 파일보다 우선한다. 같은 디렉터리의 `ChampionGameplayDefs.json`과 `SkillGameplayDefs.json`은 generated output이다. Generated freshness 실패는 source를 옛 값으로 되돌릴 권한이 아니며 현재 source로 recook해야 한다.
- F4 편집 가능 수치의 exact-value 테스트는 사용자가 해당 baseline을 명시적으로 동결한 경우에만 둔다. 기본 게이트는 schema·domain·rank shape·canonical/generated parity다. 상세 흐름은 `.md/architecture/WINTERS_DATA_ARCHITECTURE.md`의 F4/codegen 절을 따른다.
```

### 2-5. `.md/architecture/WINTERS_CODEBASE_COMPASS.md`

`DataDriven Definition Boundary`의 canonical authoring bullet 아래에 추가한다.

```text
- F4 canonical authoring에서 현재 저장된 source JSON이 최상위 값 권위다. Dated PLAN/RESULT의 숫자는 역사적 기준선이고 generated 파일은 파생물이다. Generator/test는 current source parity를 증명해야 하며 사용자 동결 선언 없는 editable exact value를 복원하지 않는다.
```

### 2-6. `.md/architecture/WINTERS_IMGUI_TOOL_DESIGN_GUIDE.md`

`### B. Canonical gameplay authoring` 아래에 추가한다.

```text
Canonical gameplay 수치의 최신 저장값은 사용자가 승인한 authoring truth다. 회귀 테스트는 기본적으로 schema·domain·rank shape·canonical/generated parity를 고정하고, 사용자가 명시적으로 동결하지 않은 editable exact value를 고정하거나 과거 문서 값으로 복원하지 않는다.
```

### 2-7. `CLAUDE_Legacy.md`

`High-Risk Mistakes` 아래에 추가한다.

```text
- Do not replace current F4-saved canonical gameplay JSON with numeric examples from an older plan, result, test fixture, or generated file. Recook generated outputs from the current source; freeze an exact editable value only when the user explicitly declares that balance baseline immutable.
```

### 2-8. `.md/architecture/WINTERS_DATA_ARCHITECTURE.md`

`## 1. 데이터 소유권 매트릭스`의 기존 흐름 뒤에 F4와 release cook의 두 경로, 값 권위 순서, 즉시 반영 범위를 상세히 추가한다.

아래에 추가:

```text
### 1-1. F4 canonical authoring과 runtime/codegen 흐름

권위 순서:

1. 사용자가 F4 `Save & Hot Load`로 저장한 현재 canonical JSON
2. 같은 JSON을 정규화·검증해 만든 generated JSON/C++와 build hash
3. 과거 PLAN/RESULT의 숫자와 테스트 fixture는 역사·구조 설명이며 값 권위가 아님

Debug 즉시 반영은 `ChampionTuner draft -> ValidateBalanceDraft -> stale-source 비교 -> 4파일 temp/backup 원자 저장 -> SetEnabled + ReloadGameplayDefinitions GameCommand -> room-host/_DEBUG gate -> TryReloadRuntimeGameplayDefinitions -> active runtime pack publish -> current actor refresh -> Snapshot toolRevision ack` 순서다. 저장 대상은 `champions.json`, `SkillEffectGameplayDefs.json`, `SpawnObjectGameplayDefs.json`, `EconomyGameplayDefs.json`이며 서버는 추가 canonical gameplay/AI 파일도 함께 다시 읽어 완전한 팩을 만든다.

Release 지속 반영은 current canonical JSON에서 `Build-LoLDefinitionPack.py`와 `build_champion_game_data.py`를 실행해 파생 JSON, manifest/build hash, Server/Client generated C++, legacy Shared generated 데이터를 recook한 뒤 전체 빌드한다. `--check` 실패는 generated가 source보다 오래됐다는 뜻이지 source를 generated/과거 fixture 값으로 되돌리라는 뜻이 아니다.

서버 매 tick은 `GetActiveLoLGameplayDefinitionPack()`을 `TickContext.pDefinitions`에 주입한다. `GameplayDefinitionQuery`와 champion GameSim이 rank-aware damage/param을 조회하고 `DamageRequest -> DamageQueue/DamagePipeline`이 결과를 만든다. Client는 이 수치를 gameplay truth로 재계산하지 않고 Snapshot/Event와 ClientPublic visual definition으로 표현한다.
```

### 2-9. generated 산출물

복구한 canonical source로 아래 명령을 실행하고 변경된 generated 파일만 stage한다.

```powershell
python Tools/LoLData/Build-LoLDefinitionPack.py --root .
python Tools/ChampionData/build_champion_game_data.py --root .
```

`Build-LoLDefinitionPack.py`가 현재 source에서 다시 만드는 파일은 `ChampionGameplayDefs.json`, `SkillGameplayDefs.json`, `DefinitionManifest.json`, Server/Client generated C++, AI generated include, parity report다. 두 번째 generator는 `champions.json`을 legacy Shared `ChampionGameData.generated.h/.cpp`로 동기화한다.

## 3. 검증

예측:
- F4가 저장하는 네 canonical JSON 전체는 원본 작업 폴더와 normalized JSON equality를 만족한다. 확인된 복구 차이는 `SkillEffectGameplayDefs.json`의 15개 scalar leaf뿐이며 나머지 세 파일은 구현 전부터 semantic diff 0이다. generated pack은 복구한 현재값을 그대로 가진다.
- F4 contract는 current value 변경을 허용하면서 schema·rank shape·domain·generated parity·single-owner 계약을 계속 검증한다.
- Debug x64 전체 빌드, RHI truth gate, Go tests, WFX, F4, BasicAttack, codegen `--check`, 공개 제외/secret/large-file 검사가 통과한다.
- 원본 작업 폴더는 수정하지 않고 공개 작업 사본만 amend한 뒤 `origin/main`으로 푸시한다.

검증 명령:

```powershell
@'
import json
from pathlib import Path

original = Path(r"C:\Users\user\Desktop\Winters")
public = Path(r"C:\Users\user\Desktop\Winters_public_source_2026-07-22")
paths = (
    "Data/Gameplay/ChampionGameData/champions.json",
    "Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json",
    "Data/LoL/ServerPrivate/Gameplay/SpawnObjectGameplayDefs.json",
    "Data/LoL/ServerPrivate/Gameplay/EconomyGameplayDefs.json",
)
for relative in paths:
    with (original / relative).open(encoding="utf-8-sig") as stream:
        expected = json.load(stream)
    with (public / relative).open(encoding="utf-8-sig") as stream:
        actual = json.load(stream)
    if actual != expected:
        raise SystemExit(f"F4 canonical semantic mismatch: {relative}")
    print(f"F4 canonical semantic match: {relative}")
'@ | python -
python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check
python Tools/ChampionData/build_champion_game_data.py --root . --check
python Tools/LoLData/Test-F4BalanceContracts.py --root .
python Tools/LoLData/Test-BasicAttackTimingContract.py
python Tools/LoLData/Test-WfxTintRegression.py --root .
powershell -ExecutionPolicy Bypass -File Tools/Harness/Run-RHIBackendSelectionTruthGate.ps1
msbuild Winters.sln /restore /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1
Push-Location Services; go test ./...; Pop-Location
git diff --check
# tracked 파일의 forbidden path/extension, 10 MiB 초과, secret pattern을 별도 PowerShell audit로 검사
```

미검증:
- 실제 F4 화면에서 값을 다시 저장하는 수동 조작은 하지 않는다. 원본 JSON과 코드 경로·자동 게이트로 복구를 확인한다.

확인 필요:
- 없음. 사용자가 현재 F4 저장값을 보존하라고 명시했고 원본 작업 폴더에서 exact 값을 확인했다.

## 서브 에이전트 비평

초안 판정: FAIL(P0 0, P1 3). 모두 수용했다.

- 수용 P1: ranked variant만 수정하면 Viego/Fiora/Sylas, Yasuo E, ranged minion, jungle/objective economy의 F4-editable exact fixture가 남는다. 위 assertion 전부를 삭제하거나 domain/관계형 검사로 교체한다.
- 수용 P1: `Data/**` broad glob은 generated JSON까지 authoring truth로 오해하게 한다. F4가 저장하는 정확한 네 파일을 열거하고 generated JSON/C++/manifest를 제외한다.
- 수용 P1: 선택 값 검사만으로는 “그 부분 그대로”를 증명하지 못한다. 원본과 공개 작업 사본의 네 F4 파일 전체 normalized JSON equality를 자동 검증한다.
- 수용 P2: “21개 값”을 실제 semantic diff인 “5개 배열의 15개 scalar leaf”로 정정하고 Go/공개 audit 명령을 검증 목록에 넣었다.

수정본 판정: PASS(P0 0, P1 0). Economy 관계형 검사와 네 canonical 파일 전체 JSON 구조 비교가 실행 가능한 코드로 명시됐으며 `.md/계획서작성규칙.md` §0을 통과했다.
