Session - F4에서 17개 챔피언의 실제 서버 피해를 랭크별·공격 형태별 한 경로로 편집하고 Release까지 승격한다.
좌표: 기존 `2026-07-19_CHAMPION_DAMAGE_DATA_AUTHORITY_CLOSURE` 후속 · 축: C7 데이터 권위와 C8 검증 · 현재 병목: scalar custom-hit가 랭크 표를 우회함
관련: `2026-07-19_CHAMPION_DAMAGE_DATA_AUTHORITY_CLOSURE_PLAN.md` · `2026-07-19_CHAMPION_DAMAGE_DATA_AUTHORITY_CLOSURE_RESULT.md` · `WINTERS_IMGUI_TOOL_DESIGN_GUIDE.md`

## 0. 사용자 작업 계약과 비평 게이트

- 목표: F4 `Balance > Skills`에서 Passive/Basic Attack·Q/W/E/R의 일반 피해뿐 아니라 야스오 Q1/Q2·Q3·EQ처럼 실제 공격 형태가 갈리는 피해도 Rank 1~5(궁극기 1~3) 표로 직접 편집한다.
- 명시 수치: 야스오 Q1/Q2 `[60,80,100,120,140]`, Q3 Tornado `[100,120,140,160,180]`, EQ `[70,90,110,130,150]`.
- 포함: 17 champions / 85 definition rows 전수 소유권 분류, custom projectile/recast/stack/empowered-basic-attack/passive 경로, JSON 저장, Debug hot-load, codegen, Release server/client 반영, 정적 계약과 SimLab.
- 제외: 현재 피해 요청 자체가 없는 Riven Q/W, Master Yi Q/E의 신규 전투 판정 설계. 이 슬롯은 F4에서 `Server damage execution: NOT_IMPLEMENTED`로 보이고 피해 행을 편집 불가로 만들어 무반영 편집을 금지한다. Master Yi W는 의도적 비피해 skill이며, Lee Sin E와 Riven R은 기존 canonical rank 행으로 정상 편집된다.
- 사용자 확인 불필요 가정: 명시되지 않은 현 밸런스 값은 바꾸지 않는다. 랭크 배열로 승격되는 기존 scalar는 모든 rank에 현재 값을 복제해 체감 회귀를 막는다.

## 1. 결정 기록

① 문제·제약: 일반 스킬 최종 피해는 `damage.*ByRank`지만, Yasuo Q variants, Lee Sin Q2, Kalista E spear, Ezreal R non-epic과 Ashe/Fiora/Jax/Kindred 강화 평타는 `params` scalar를 직접 읽는다. 현 F4는 이를 표 아래 scalar로 노출해 랭크별 편집이 불가능하고, 일반 `Flat Damage`와 중복돼 어느 값이 실제인지 알 수 없다.

② 순진한 해법의 실패: `base + damagePerRank` 두 scalar를 유지하면 임의의 `[60,80,105,120,140]` 같은 비선형 배열을 표현할 수 없다. 모든 `params.baseDamage`를 배열로 바꾸면 이동·지속시간 등 mechanics까지 불필요하게 흔들고 일반 formula와 두 번째 피해 원본을 만든다.

③ 메커니즘: `SkillEffectParam`은 scalar source/semantic contract를 유지하되 필요한 special-damage param만 1~5개 ranked value를 갖게 한다. struct layout/ABI는 의도적으로 확대되므로 전 타깃 full rebuild와 layout contract가 필수다. 일반 flat/ratio는 계속 `DamageFormulaDef`가 소유한다. custom single-flat 경로는 canonical `flatByRank`를 직접 조회하고, 진짜 추가 형태만 ranked param을 조회한다. F4는 같은 rank table 안에 variant 행을 렌더하고 기존 `Runtime Damage Params` 중복 editor를 제거한다.

④ 대조: 별도 `DamageVariantDef` schema는 의미상 깨끗하지만 새 enum/배열/JSON 노드/overlay를 한 벌 더 만들고 기존 param 기반 custom hooks를 전부 이관해야 한다. 이번에는 기존 `params` ABI에 rankCount/value 배열을 추가하되 generator가 배열 허용 key를 5개 effect-param 쌍으로 제한해 범용 오남용을 막는다.

⑤ 대가: special variant label은 F4의 effect-key/param 매핑이 소유한다. 새 variant가 생기면 JSON allowlist, F4 label, 소비 hook, 계약 테스트를 함께 추가해야 한다. 피해 요청이 없는 여섯 슬롯은 이번 F4 작업에서 전투 기능까지 임의 설계하지 않는다.

## 2. 현재 코드 증거와 전수 분류

| 분류 | 실제 권위 | F4 종결 방식 |
|---|---|---|
| 일반 Q/W/E/R flat·AD/AP/HP ratio | `damage.*ByRank -> BuildSkillDamageRequest -> DamageQueue` | 기존 rank 행 유지 |
| Yasuo Q1/Q2 | 현재 `params.baseDamage` scalar | canonical `damage.flatByRank`로 이관 |
| Yasuo Q3 / EQ | `tornadoDamage` / `dashAreaDamage` scalar | 두 ranked param 행 |
| Lee Sin Q1 / Q2 | Q1 canonical, Q2 `params.baseDamage` | canonical 행 + `Q2 Recast` ranked param 행 |
| Kalista E | `baseDamage + damagePerSpear * stacks` scalar | base는 canonical, spear는 ranked param 행 |
| Ezreal R champion/epic / non-epic | base+perRank scalar 두 벌 | primary는 canonical, non-epic은 ranked param 한 행 |
| Ashe Q, Fiora E, Jax W/R, Kindred E 강화 평타 | `params.baseDamage` scalar | 해당 스킬 canonical flat rank를 직접 소비 |
| Riven R | hook scalar를 제출하지만 queue가 canonical로 교체 | canonical rank 행만 노출 |
| Fiora/Sylas/Zed passive BA | 조건부 bonus가 basic-attack formula와 별도 scalar | Passive/Basic Attack selector + curated scalar mechanics |
| Zed R mark | cast 시 `missingHealthDamageRatio` scalar 캡처 | canonical rank `targetMissingHpRatioByRank`에서 캡처 |
| Lee Sin E1 | zero-flat request를 queue가 canonical formula로 교체 | canonical rank 행 + 실제 hook probe |
| Riven Q/W, Master Yi Q/E | damage request 없음 | 경고 + 피해 행 disabled |
| Master Yi W | 의도적 비피해 heal/channel | `Non-damaging skill` 안내, 피해 행 disabled |

전수 결과에서 Viego W charge, projectile pierce, Zed shadow처럼 별도 형태는 flat 원본이 다르지 않고 canonical request에 scale/target 정책만 적용하므로 variant 행을 만들지 않는다.

## 3. 파일별 구현

### 3-1. `Shared/GameSim/Definitions/SkillAtomData.h`

기존 `SkillEffectParam`과 resolver를 아래로 교체한다. scalar generated data는 rankCount 1/valueByRank[0]로 유지된다.

```cpp
struct SkillEffectParam
{
    eSkillEffectParamId id = eSkillEffectParamId::None;
    u8_t rankCount = 1u;
    f32_t valueByRank[kSkillRankValueMax]{};
};

inline f32_t ResolveSkillEffectParamRanked(
    const SkillEffectSpec& effect,
    eSkillEffectParamId id,
    u8_t rank,
    f32_t fallbackValue = 0.f)
{
    if (const SkillEffectParam* param = FindSkillEffectParam(effect, id))
    {
        const u8_t count = std::clamp<u8_t>(
            param->rankCount, 1u, kSkillRankValueMax);
        const u8_t resolvedRank = std::clamp<u8_t>(rank > 0u ? rank : 1u, 1u, count);
        return param->valueByRank[resolvedRank - 1u];
    }
    return fallbackValue;
}

inline f32_t ResolveSkillEffectParam(
    const SkillEffectSpec& effect,
    eSkillEffectParamId id,
    f32_t fallbackValue = 0.f)
{
    return ResolveSkillEffectParamRanked(effect, id, 1u, fallbackValue);
}
```

`<algorithm>`, `<cstddef>`, `<type_traits>` include가 없다면 같은 파일 상단에 추가한다. `static_assert(std::is_trivially_copyable_v<SkillEffectParam>)`, `sizeof(SkillEffectParam)==24`, `offsetof(rankCount)==1`, `offsetof(valueByRank)==4`를 추가한다. `SummonPolicyParam`은 계속 `{ id, value }` scalar이며 이번 migration 대상이 아니다.

### 3-2. `Shared/GameSim/Definitions/GameplayDefinitionQuery.h/.cpp`

기존 scalar query 아래에 명시 rank param과 canonical flat query를 추가한다.

```cpp
f32_t ResolveSkillEffectParamRanked(
    CWorld& world, EntityID entity, const TickContext& tc,
    eChampion fallbackChampion, u8_t slot, u8_t rank,
    eSkillEffectParamId param, f32_t fallbackValue = 0.f);

f32_t ResolveSkillFlatDamage(
    CWorld& world, EntityID entity, const TickContext& tc,
    eChampion fallbackChampion, u8_t slot, u8_t rank,
    f32_t fallbackValue = 0.f);

f32_t ResolveSkillTargetMissingHpRatio(
    CWorld& world, EntityID entity, const TickContext& tc,
    eChampion fallbackChampion, u8_t slot, u8_t rank,
    f32_t fallbackValue = 0.f);
```

```cpp
f32_t ResolveSkillEffectParamRanked(
    CWorld& world, EntityID entity, const TickContext& tc,
    eChampion fallbackChampion, u8_t slot, u8_t rank,
    eSkillEffectParamId param, f32_t fallbackValue)
{
    f32_t overrideValue = 0.f;
    if (TryResolvePracticeSkillEffectOverride(world, entity, slot, param, overrideValue))
        return overrideValue;
    if (const SkillGameplayDef* skill = FindSkill(
            world, entity, tc, fallbackChampion, slot))
        return ::ResolveSkillEffectParamRanked(
            skill->effect, param, rank, fallbackValue);
    return fallbackValue;
}

f32_t ResolveSkillFlatDamage(
    CWorld& world, EntityID entity, const TickContext& tc,
    eChampion fallbackChampion, u8_t slot, u8_t rank,
    f32_t fallbackValue)
{
    if (const SkillGameplayDef* skill = FindSkill(
            world, entity, tc, fallbackChampion, slot))
    {
        const f32_t flatOverride = ResolveSkillEffectParam(
            world, entity, tc, fallbackChampion, slot,
            eSkillEffectParamId::DamageFlatOverride, -1.f);
        if (flatOverride >= 0.f)
            return flatOverride;
        if (skill->effect.damage.bValid)
            return ResolveDamageFormulaRankedValue(
                skill->effect.damage, skill->effect.damage.flatByRank, rank);
    }
    return fallbackValue;
}

f32_t ResolveSkillTargetMissingHpRatio(
    CWorld& world, EntityID entity, const TickContext& tc,
    eChampion fallbackChampion, u8_t slot, u8_t rank,
    f32_t fallbackValue)
{
    if (const SkillGameplayDef* skill = FindSkill(
            world, entity, tc, fallbackChampion, slot))
    {
        const f32_t ratioOverride = ResolveSkillEffectParam(
            world, entity, tc, fallbackChampion, slot,
            eSkillEffectParamId::MissingHealthDamageRatio, -1.f);
        if (ratioOverride >= 0.f)
            return ratioOverride;
        if (skill->effect.damage.bValid)
            return ResolveDamageFormulaRankedValue(
                skill->effect.damage,
                skill->effect.damage.targetMissingHpRatioByRank,
                rank);
    }
    return fallbackValue;
}
```

### 3-3. `Tools/LoLData/Build-LoLDefinitionPack.py`

- `RANKED_DAMAGE_PARAM_SPECS`를 다음 exact pair로 추가한다: Yasuo Q `tornadoDamage`, `dashAreaDamage`; Lee Sin Q `baseDamage`; Kalista E `damagePerSpear`; Ezreal R `nonEpicBaseDamage`.
- pair에 해당하면 JSON array를 요구하고 skill slot rank count(5/3)와 정확히 같은 길이, finite `0..1,000,000`을 검증한다. 그 외 param은 기존 number만 허용한다.
- normalized param은 항상 `values` list로 보관한다.
- generated C++는 아래 형태로 쓴다.

```python
lines.append(
    f"        def.effect.params[{param_index}].rankCount = "
    f"static_cast<u8_t>({len(param['values'])}u);"
)
for rank_index, value in enumerate(param["values"]):
    lines.append(
        f"        def.effect.params[{param_index}].valueByRank[{rank_index}] = "
        f"{cpp_float(value)};"
    )
```

`required_variant_params`는 Ezreal `nonEpicDamagePerRank`를 제거하고 위 five-pair 존재/array 계약으로 대체한다. `server_skill_json()`은 기존 list shape를 유지해 scalar는 `{"id": id, "value": value}`, ranked는 `{"id": id, "values": [...]}`로 출력한다. `skill_effect_json()`은 기존 object-map shape를 유지해 scalar는 `params[id] = value`, ranked는 `params[id] = [...]`로 출력한다. 두 함수 모두 normalized `values`를 `param['value']`로 다시 읽지 않는다.

### 3-4. `Server/Private/Data/RuntimeGameplayDefinitionOverlay.cpp`

`ApplySkillEffectsJson` param loop에서 number는 rankCount 1, array는 1..5개를 검증해 `valueByRank`에 기록한다. array는 generator와 같은 five-pair만 허용하고 해당 entry의 `damage.flatByRank` 길이와 같아야 한다. 성공 후 기존 `pSkill->effect = effect`를 유지해 Debug `Save & Hot Load`가 즉시 같은 ranked values를 사용한다.

### 3-5. `Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json`

아래 값만 의미 변경한다.

```json
"skill.yasuo.q": {
  "params": {
    "tornadoDamage": [100.0, 120.0, 140.0, 160.0, 180.0],
    "dashAreaDamage": [70.0, 90.0, 110.0, 130.0, 150.0]
  },
  "damage": { "flatByRank": [60.0, 80.0, 100.0, 120.0, 140.0] }
}
```

기존값 보존 배열:

```json
"skill.leesin.q.params.baseDamage": [95, 95, 95, 95, 95]
"skill.kalista.e.params.damagePerSpear": [30, 30, 30, 30, 30]
"skill.ezreal.r.params.nonEpicBaseDamage": [150, 225, 300]
```

Yasuo Q `params.baseDamage`, Kalista E `params.baseDamage`, Ezreal R `params.baseDamage/damagePerRank/nonEpicDamagePerRank`은 standard/variant rank owner로 대체되므로 삭제한다. Ashe Q, Fiora E, Jax W/R, Kindred E의 `params.baseDamage`는 canonical flat로 소비 경로를 옮긴 뒤 삭제한다. `skill.zed.r.params.missingHealthDamageRatio`도 canonical target-missing-HP rank ratio로 이관한 뒤 삭제하며, `skill.zed.basic_attack.params.missingHealthDamageRatio`는 conditional passive owner이므로 유지한다. 다른 일반 hook의 legacy scalar는 이번 source-risk 범위에서 보존하되 F4에는 노출하지 않는다.

### 3-6. custom-hit GameSim consumers

- `YasuoGameSim.cpp`: Q1/Q2는 `ResolveSkillFlatDamage(... ctx.skillRank)`, Q3/EQ는 `ResolveSkillEffectParamRanked(... ctx.skillRank)`.
- `LeeSinGameSim.cpp`: Q2 `baseDamage`를 ranked resolver로 조회.
- `KalistaGameSim.cpp`: E base는 canonical flat, spear는 ranked resolver.
- `EzrealGameSim.cpp`: R primary는 canonical flat; non-epic은 `nonEpicBaseDamage` ranked resolver 하나로 조회하고 base+step branch 제거.
- `AsheGameSim.cpp`: Q cast에서 `ctx.skillRank` canonical flat을 `state.qBonusDamage`에 캡처하고 hit은 그 값을 소비한다. 기존 field를 재사용해 component ABI를 바꾸지 않는다.
- `FioraGameSim.cpp`, `JaxGameSim.cpp`: E/W/R cast에서 `ctx.skillRank` canonical flat을 즉시 기존 pending damage field에 캡처한다.
- `KindredSimComponent.h` / `KindredGameSim.cpp`: 기존 `mountingDreadHitCount` 뒤 alignment padding에 `u8_t uECastRank=1`을 명시하고 E cast rank를 캡처한다. 3타 hit은 이 rank로 canonical flat을 조회한다. component `sizeof/offsetof`와 keyframe round-trip을 검사한다.
- `ZedGameSim.cpp`: R mark의 `fMissingHealthDamageRatio`는 scalar param이 아니라 R canonical `targetMissingHpRatioByRank`의 `ctx.skillRank` 값을 캡처한다.

### 3-7. `Client/Private/UI/ChampionTuner.cpp`

- Skill selector를 `{ "Passive / Basic Attack", "Q", "W", "E", "R" }` 5개로 확장한다. 기존 `skillIndex=clamp(skillSlot-1,0,3)` / `skillSlot=skillIndex+1`을 `skillIndex=clamp(skillSlot,0,4)` / `skillSlot=skillIndex` direct mapping으로 교체하고 `SkillRankCount(0)=1`, `SkillRankCount(4)=3`, 나머지 5를 고정한다.
- `kRuntimeFlatOnlySkills`와 범용 `Runtime Damage Params` scalar editor를 삭제한다.
- rank table에서 canonical `Flat Damage`는 항상 실제 standard flat을 편집한다.
- effect-param mapping으로 배열이 존재할 때 다음 행을 같은 표에 추가한다: `Q3 Tornado Flat Damage`, `EQ Flat Damage`, `Q2 Recast Flat Damage`, `Damage Per Spear`, `Non-Epic Flat Damage`.
- `DrawRankRow`는 `ordered_json` array를 받는 기존 구현을 재사용한다.
- missing execution set 네 damage 슬롯(Riven Q/W, Master Yi Q/E)은 `Server damage execution: NOT_IMPLEMENTED`, Master Yi W는 `Non-damaging skill`을 표 위에 표시한다. cooldown/mechanics는 편집 가능하되 damage 행만 disabled 처리한다. Lee Sin E는 canonical rank 행을 활성화하고 E1 hook probe에 포함한다.
- Passive/Basic Attack에서 실제 conditional bonus를 가진 세 champion만 `Conditional Damage Mechanics` scalar section을 렌더한다: Fiora `Vital Target Max HP Ratio`, Sylas `Petricite Burst Flat Damage`/`Petricite Burst AP Ratio`, Zed `Contempt for the Weak Missing HP Ratio`. Zed threshold는 damage가 아닌 발동 mechanics이므로 기존 mechanics editor에 `Target Health Threshold Ratio`로 함께 둔다. 같은 값의 두 번째 editor는 만들지 않는다.

와이어프레임:

```text
[Champion: YASUO] [Skill: Q]
Value                       Rank 1 Rank 2 Rank 3 Rank 4 Rank 5
Cooldown (sec)               ...    ...    ...    ...    ...
Q1/Q2 Flat Damage             60     80    100    120    140
Q3 Tornado Flat Damage       100    120    140    160    180
EQ Flat Damage                70     90    110    130    150
Total AD Ratio               2.0    2.0    2.0    2.0    2.0
...
[Save & Hot Load] [Reload JSON]
```

행동 예산: 값 편집 1회 + 기존 단일 primary `Save & Hot Load` 1회. Release는 기존 정책대로 cook/build 안내를 유지한다. duplicate save/action을 만들지 않는다.

### 3-8. 계약과 회귀

`Tools/LoLData/Test-F4BalanceContracts.py`:

- five ranked-param pair와 rank length를 JSON에서 검사한다.
- Yasuo 세 명시 배열 exact value를 검사한다.
- generated `SkillGameplayDefs.json`의 param-list와 parity report의 skill-effect param-map에서 scalar/ranked shape 및 값 parity를 각각 검사한다.
- `Runtime Damage Params`/`kRuntimeFlatOnlySkills`가 활성 Skills surface에 없고 variant labels가 rank table 안에 존재함을 검사한다.
- missing execution warning set과 disabled path를 검사한다.
- custom consumers가 canonical flat 또는 ranked resolver를 쓰는지 source anchors로 검사한다.
- `skill.zed.r.params.missingHealthDamageRatio` 부재와 `skill.zed.basic_attack` 동명 conditional param 존재를 검사한다.

`Tools/SimLab/main.cpp`의 `RunChampionDamageAuthorityMatrixProbe` 앞에 `RunRankedVariantDamageProbe`를 추가한다.

```cpp
bool_t RunRankedVariantDamageProbe()
{
    // 1) Generated pack의 5 ranked params를 모든 rank에서 raw oracle과 query로 비교한다.
    // 2) Yasuo Q1/Q3/EQ hook을 stage별 dispatch하고 projectile/request를 Queue까지 흘려 최종 HP를 검사한다.
    // 3) Lee Q2 mark/recast와 Kalista E stack을 dispatch하고 Queue까지 흘려 flat+canonical ratio 최종 HP를 검사한다.
    // 4) Ezreal R GlobalBeam을 champion과 non-epic target에 접촉시켜 최종 HP를 검사한다.
    // 5) Ashe/Fiora/Jax/Kindred empowered BA를 서로 다른 cast/live rank로 실행해 cast-rank 고정을 검사한다.
    // 6) Fiora/Sylas/Zed passive BA의 conditional scalar를 실제 consumer->queue->HP로 검사한다.
    // 7) Lee Sin E1 hook이 canonical rank flat/ratio로 최종 HP를 감소시키는지 검사한다.
    // 8) scalar mechanic param을 rank 5로 ranked-resolve해도 rank 1 값으로 clamp되는지 검사한다.
    // 실패 로그는 effect/variant/rank/expected/actual을 출력한다.
    return true;
}
```

`--f4-balance-only`와 full run 모두 이 probe를 포함한다. 기존 `RunEzrealProjectileAuthorityProbe`, `RunSylasPassiveBasicAttackProbe`, `RunZedPassiveDeathMarkProbe`의 fixture/helper를 재사용하되 F4-only에서 필요한 assertion을 새 probe가 직접 호출할 수 있게 좁은 helper로 추출한다. 테스트가 production resolver 결과를 expected로 재사용하지 않도록 generated raw arrays를 oracle로 삼는다.

## 4. 구현·검증 순서

1. 독립 critique의 P0/P1을 0으로 만든다.
2. 적용 직전 target files의 diff와 JSON SHA-256을 캡처한다. 다른 세션 변경이 생기면 해당 hunk를 재대조한다.
3. struct/query/generator/overlay를 먼저 구현하고 정적 test를 실패 상태로 만든다.
4. JSON/UI/custom consumers를 구현해 test를 통과시킨다.
5. 현재 canonical inputs가 generation 동안 바뀌지 않았음을 SHA-256 전후로 확인하며 generator를 직렬 실행한다.
6. Debug F4 계약 + Debug SimLab `--f4-balance-only`/full + Debug Server/Client를 `/m:1`로 검증한다.
7. Release generator freshness + Release SimLab `--f4-balance-only`/full + Release Server/Client를 `/m:1`로 검증한다.
8. Debug host에서 Yasuo Q3 Rank 2를 임시 sentinel로 바꿔 `Save & Hot Load` 성공 revision을 확인하고, 재접속 없이 Q3 hook→queue→HP가 sentinel로 변하는 E2E probe/실클라 로그를 확인한 뒤 원래 값으로 복원한다.
9. F4 수동 화면 QA에서 Yasuo Q 표, Lee Q2/Kalista E/Ezreal R variant 행, missing-execution 경고, Save/Reload status를 확인한다.

검증 명령:

```powershell
python Tools/LoLData/Test-F4BalanceContracts.py --root .
python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check
python Tools/ChampionData/build_champion_game_data.py --root . --check

$msbuild = (Get-ChildItem 'C:/Program Files/Microsoft Visual Studio/2022' -Filter MSBuild.exe -Recurse |
  Where-Object FullName -Match 'MSBuild\\Current\\Bin\\MSBuild.exe' | Select-Object -First 1).FullName
& $msbuild Shared/GameSim/Include/GameSim.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
& $msbuild Tools/SimLab/SimLab.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
& Tools/Bin/Debug/SimLab.exe --f4-balance-only
& Tools/Bin/Debug/SimLab.exe
& $msbuild Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
& $msbuild Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1

& $msbuild Shared/GameSim/Include/GameSim.vcxproj /p:Configuration=Release /p:Platform=x64 /m:1
& $msbuild Tools/SimLab/SimLab.vcxproj /p:Configuration=Release /p:Platform=x64 /m:1
& Tools/Bin/Release/SimLab.exe --f4-balance-only
& Tools/Bin/Release/SimLab.exe
& $msbuild Server/Include/Server.vcxproj /p:Configuration=Release /p:Platform=x64 /m:1
& $msbuild Client/Include/Client.vcxproj /p:Configuration=Release /p:Platform=x64 /m:1
git diff --check
```

## 5. 예측·합격선·미검증

예측:

- Yasuo Q 표는 3개 형태 × 5 ranks를 보이고 저장 JSON, Debug overlay, generated Release pack이 동일하다.
- 일반 `Flat Damage`와 special scalar가 동시에 편집되는 슬롯은 0개가 된다.
- custom empowered attack 5종은 F4 canonical rank에 따라 실제 bonus가 달라진다.
- 기존 명시 없는 custom 값은 모든 rank에 종전 scalar가 복제되어 현재 게임 체감이 유지된다.

합격선:

- critique P0=0/P1=0.
- generator/check와 F4 static contract PASS.
- SimLab variant/custom-consumer probe + 기존 17 champions / 85 definition rows / 323 rank formulas damage-authority PASS. 이 숫자는 실제 cast 구현 수가 아니라 definition coverage임을 로그에 명시한다.
- Debug/Release GameSim, SimLab, Server, Client build PASS.
- full SimLab 외부 선행 실패가 있으면 새 variant probe와 관련 probe가 PASS이고 최초 외부 실패를 RESULT에 분리한다.
- 화면 QA에서 각 row label/Rank 1~5/disabled warning/Save status가 잘리지 않는다.

미검증/경계:

- F4에서 disabled로 표시한 네 미구현 damage slot의 신규 판정·타겟·FX 설계는 별도 gameplay closure가 필요하다. Master Yi W는 미구현 damage가 아니라 의도적 비피해 skill로 별도 표시한다.
- Release F4는 runtime hot-load를 허용하지 않는다. 저장된 authoring은 generator cook + Release rebuild 뒤 반영된다.

## 6. 인계와 충돌 경계

- 직접 owner: 본 PLAN/RESULT, ranked param ABI/query/generator/runtime overlay, F4 Skills slice, custom-hit consumer hunk, F4/SimLab 계약.
- generated files는 손편집하지 않고 마지막에 generator 단일 실행으로만 갱신한다.
- 현재 worktree의 replay, nav, inhibitor, FX, AI, HUD 변경은 모두 보존한다. 같은 파일의 비대상 hunk를 정리하거나 되돌리지 않는다.
- 구현 완료 뒤 같은 이름의 `_RESULT.md`에 예측 대 실측, hashes, 빌드 결과, manual QA 여부, 잔여 외부 실패를 기록한다.

## 7. 독립 비평 이력

- 1차 `/root/replay_plan_critique`: P0=1/P1=4, FAIL.
  - 수용(P0): normalized `values` 전환 시 C++ emitter뿐 아니라 `server_skill_json()`과 `skill_effect_json()`의 `param['value']` 두 소비자를 함께 이관하고 scalar/ranked generated JSON shape을 확정했다.
  - 수용(P1): Fiora/Sylas/Zed passive basic-attack conditional damage를 전수 분류에 추가하고 Passive/Basic Attack selector와 curated one-edit-path editor를 추가했다. Zed R duplicate scalar capture도 canonical rank ratio로 옮긴다.
  - 수용(P1): rank 시점은 모두 cast-rank로 확정했다. Ashe/Fiora/Jax는 기존 pending value field, Kindred는 alignment padding의 explicit `uECastRank`, projectile/recast는 기존 captured `rank`를 사용한다.
  - 수용(P1): resolver-only probe를 실제 hook/projectile/request/queue/HP consumer probe로 확장했다.
  - 수용(P1): `17/85/323`을 definition coverage로 고쳐 쓰고, executable-damage missing과 의도적 non-damage를 분리했다.
- 2차 `/root/replay_plan_critique`: P0=0/P1=4, FAIL.
  - 수용: `server_skill_json()`의 list와 `skill_effect_json()`의 object-map 기존 shape를 분리해 scalar/ranked exact output과 parity assertion을 명시했다.
  - 수용: Zed R canonical missing-HP query API, legacy scalar 삭제, basic-attack conditional scalar 유지 계약을 추가했다.
  - 수용: Yasuo/Lee/Kalista custom request를 Queue→HP까지 검증하고 Debug hot-load non-rank1 sentinel E2E를 추가했다.
- 추가 독립 critique(`/root/inhibitor_visual_audit`): P0=0/P1=3/P2=2, FAIL.
  - 수용: Lee Sin E1을 NOT_IMPLEMENTED에서 제거하고 canonical hook probe에 포함했다. 미구현 damage는 Riven Q/W, Master Yi Q/E 네 슬롯이다.
  - 수용: scalar source semantic은 유지하지만 physical ABI/layout은 확대됨을 명시하고 exact layout/trivial-copy, scalar rank clamp, SummonPolicy 비대상 계약을 추가했다.
  - 수용: Zed R duplicate param 삭제와 generated JSON shape parity, Basic Attack selector direct 0..4 mapping을 고정했다.
- 3차 `/root/replay_plan_critique`: P0=0/P1=0, PASS. generated JSON 두 shape, Zed R single owner, Queue→HP probes, Debug hot-load E2E, Lee E/layout 경계를 재대조해 구현 gate를 통과했다.
