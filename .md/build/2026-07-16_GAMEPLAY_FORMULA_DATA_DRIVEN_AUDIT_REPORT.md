Session - Data Driven cutover 직전의 17개 챔피언 피해 공식·아이템·랭크·애니메이션 ownership을 수식과 카운트로 고정한다.
좌표: 신규 좌표 후보 · 축: C7 · C8
관련: 2026-07-16_GAMEPLAY_FORMULA_SKILL_ANIMATION_DATA_DRIVEN_REMEDIATION_PLAN.md · 2026-07-16_GAMEPLAY_FORMULA_SKILL_ANIMATION_DATA_DRIVEN_REMEDIATION_RESULT.md

# Gameplay Formula / Data-Driven 전수 감사 보고서

> 이 문서는 **2026-07-16 cutover 직전 baseline**이다. 현재 판결은 짝 `_RESULT`와 상세 구현 보고서를 따른다.

작성일: 2026-07-16
대상: 현재 Winters 서버 권위 GameSim, 정의 팩, 17개 챔피언, 34개 아이템, 인게임 밸런스/애니메이션 튜너

## 0. 판정 요약

요청에는 16명이라고 적혀 있지만 실제 `champions.json`과 생성 팩에는 **17명**이 있다. 누락을 만들지 않기 위해 Irelia, Yasuo, Kalista, Garen, Zed, Riven, Ezreal, Fiora, Jax, Lee Sin, Kindred, Master Yi, Annie, Ashe, Viego, Yone, Sylas 전부를 조사했다.

정적 전수 카운트는 champion 17명, BA/Q/W/E/R slot 85개, skill effect entry 61개, item 34개다. 85개 slot의 `scalingTableId`는 모두 0이다.

이 baseline 시점의 판정은 **Data Driven 불합격(부분 통과)** 이다. 현재 상태 판정으로 읽지 않는다.

| 영역 | 판정 | 실제 상태 |
|---|---|---|
| 챔피언 기본 스탯 | 통과 | `champions.json -> 생성 팩/Debug overlay -> StatSystem`으로 이어진다. |
| 스킬 마나/쿨다운/사거리/action lock | 부분 통과 | JSON에 있으나 일부 챔피언 훅이 별도 effect param/fallback을 사용하고, 스킬 레벨별 쿨다운·마나는 Ezreal 일부만 별도 계산한다. |
| 스킬 피해 공식 | 불합격 | 공용 피해 파이프라인은 계수를 지원하지만 실제 `scalingTableId`는 전부 0, 등록 함수는 비어 있다. 대부분 챔피언이 C++에서 각자 피해를 조립한다. |
| 아이템 평면 스탯/가격 | 통과 | 34개 아이템의 가격과 flat stat은 JSON/정의 팩에서 적용된다. |
| 아이템 패시브/온힛 | 불합격 | `ItemDef`에 패시브 표현이 없다. 몰락한 왕의 검은 AD/AS/흡혈만 있고 체력 비례 피해는 0이다. |
| 생명력 흡수 | 부분 통과 | 실제 가한 최종 체력 피해 × lifesteal. `CanLifesteal` 기본 공격에만 붙는다. `spellVamp`는 필드만 있고 소비자가 없다. |
| 스킬 레벨 규칙 | 부분 통과 | authoritative `CommandExecutor`는 Q/W/E 1/3/5/7/9, R 6/11/16 제한을 적용한다. 그러나 공용 `SkillRankSystem`은 최대 랭크만 검사해 봇 초기화·SimLab·smoke 경로가 제한을 우회할 수 있다. |
| 애니메이션 수치 | 불합격 | server lock, generated playback speed, legacy cast/recovery frame이 서로 다른 세 군데에 있다. JSON의 cast/recovery frame은 현재 소비되지 않는다. |
| 인게임 튜닝 | 부분 통과 | 서버 권위 임시 override는 가능하지만 별도 scratch JSON이며 정식 authoring JSON에 round-trip하지 않는다. 애니메이션 튜너는 `const_cast` 메모리 수정이며 저장되지 않는다. |

가장 큰 본질 문제는 **수치 저장 위치가 없는 것**이 아니라 **수치를 실제 공식으로 소비하는 단일 경로가 없는 것**이다.

## 1. 실제 런타임 데이터 흐름

### 1.1 Release/일반 빌드

```text
Data/Gameplay/ChampionGameData/champions.json
Data/LoL/ServerPrivate/Gameplay/*.json
    -> Tools/LoLData/Build-LoLDefinitionPack.py
    -> Server/Private/Data/Generated/LoLGameplayDefinitions.generated.cpp
    -> GetLoLGameplayDefinitionPack()
    -> GameRoom TickContext::pDefinitions
    -> StatSystem / GameplayDefinitionQuery / champion GameSim hook
```

일반 런타임은 매 프레임 JSON 문자열을 읽지 않는다. JSON은 authoring/cook 입력이고 Release는 생성된 C++ immutable pack을 읽는다. 이 방향 자체는 올바르다.

### 1.2 Debug hot reload

`Server/Private/Data/RuntimeGameplayDefinitionOverlay.cpp:1534-1600`은 다음 6개 파일을 workspace에서 다시 읽어 생성 팩의 복사본을 덮는다.

1. `Data/Gameplay/ChampionGameData/champions.json`
2. `Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json`
3. `Data/LoL/ServerPrivate/Gameplay/SummonerSpellGameplayDefs.json`
4. `Data/LoL/ServerPrivate/Gameplay/EconomyGameplayDefs.json`
5. `Data/LoL/ServerPrivate/Gameplay/ItemGameplayDefs.json`
6. `Data/LoL/ServerPrivate/Gameplay/SpawnObjectGameplayDefs.json`

`ReloadGameplayDefinitions`는 Debug 전용이며 Release에서는 `debug-only`로 실패한다. 새로운 아이템/새로운 key는 hot reload만으로 추가할 수 없고 codegen + rebuild가 필요하다.

### 1.3 현재 끊긴 피해 데이터 경로

`DamagePipeline`은 `SkillScalingTable`을 읽을 준비가 되어 있지만 다음 근거로 실사용되지 않는다.

- `Shared/GameSim/Definitions/ChampionRuntimeDefaults.cpp:20-23`의 `RegisterDefaultChampionSkillScalingTables()`가 비어 있다.
- `champions.json`의 17 × 5 skill `scalingTableId`가 전부 0이다.
- 생성된 server/client game-data의 `scalingTableId`도 전부 0이다.
- 따라서 대부분의 챔피언은 `BaseDamage`, `DamagePerRank`를 읽은 뒤 자기 C++ 훅에서 제각각 `rank` 또는 `rank - 1`을 곱한다.

즉 `SkillScalingTable`은 구조와 파이프라인만 있고 데이터 연결이 없는 dead path다.

## 2. 공용 수식의 실제 코드

아래 식에서 `L`은 챔피언 레벨, `r`은 스킬 랭크, `G(L)`은 레벨 성장 배율이다.

### 2.1 레벨 성장 스탯

근거: `Shared/GameSim/Systems/Combat/CombatFormula.cpp:7-18`, `Shared/GameSim/Systems/Stat/StatSystem.cpp:139-171`

```text
G(1) = 0
G(L) = (L - 1) × (0.7025 + 0.0175 × (L - 1)), L > 1

레벨 스탯 = 기본값 + 성장값 × G(L)
```

이 식은 HP, Mana, base AD, AP, base Armor, base MR에 동일하게 적용된다.

```text
최종 AD    = 레벨 base AD + 아이템/버프 bonus AD
최종 AP    = 레벨 AP + 아이템/버프 AP
최종 Armor = 레벨 base Armor + 아이템/버프 bonus Armor
최종 MR    = 레벨 base MR + 아이템/버프 bonus MR
최대 HP    = 레벨 HP + 아이템 flat HP
최대 Mana  = 레벨 Mana + 아이템 flat Mana
```

### 2.2 공격 속도

```text
성장 AS 보너스 = attackSpeedPerLevel × G(L)
ratio = attackSpeedRatio > 0 ? attackSpeedRatio : baseAttackSpeed
AS = clamp(baseAttackSpeed + ratio × (성장 AS 보너스 + bonusAttackSpeed), 0.2, 3.003)
```

JSON의 `attackSpeedPerLevel`은 0.025가 2.5%를 뜻하는 소수 비율이다. 아이템 `bonusAttackSpeed`도 같은 단위다.

### 2.3 스킬 가속과 쿨다운

```text
실제 쿨다운 = 기본 쿨다운 × 100 / (100 + abilityHaste)
```

음수 haste는 0으로 고정된다.

### 2.4 원시 피해

근거: `Shared/GameSim/Systems/Damage/DamagePipeline.cpp:292-331`

```text
RawDamage = FlatDamage(r)
          + Source.TotalAD × TotalADRatio(r)
          + Source.BonusAD × BonusADRatio(r)
          + Source.AP × APRatio(r)
          + Target.MaxHP × TargetMaxHPRatio(r)
          + Target.MissingHP × TargetMissingHPRatio(r)
```

현재 `DamageRequest`는 위 다섯 계수를 모두 표현할 수 있다. 문제는 다수 챔피언 훅이 이 필드들을 채우지 않는다는 점이다.

### 2.5 방어력/마법 저항력과 관통 순서

근거: `Shared/GameSim/Systems/Combat/CombatFormula.cpp:20-79`

적용 순서:

1. flat resistance reduction을 base/bonus 비율로 분배
2. percent resistance reduction
3. bonus resistance percent penetration
4. total percent penetration
5. flat penetration, 물리의 경우 현재 `lethality + armorPen`

```text
R >= 0: 배율 = 100 / (100 + R)
R < 0 : 배율 = 2 - 100 / (100 - R)
FinalDamage = RawDamage × 배율
```

True damage는 저항력 단계를 건너뛴다. shield와 Kindred 체력 바닥을 거친 뒤 실제 HP 감소량이 `finalAmount`가 된다.

### 2.6 치명타와 생명력 흡수

```text
치명타 피해 = 저항력 적용 전 피해 × max(1, critDamage)
치명타 확률 = clamp(critChance, 0, 1)

흡혈 회복량 = 실제 최종 HP 감소량 × lifesteal
회복 후 HP = min(maxHP, 현재 HP + 흡혈 회복량)
```

기본 `critDamage`는 1.75다. Infinity Edge도 현재 crit damage를 올리지 않고 AD +65와 crit chance +25%만 준다. 기본 공격 request에 `OnHit | CanCrit | CanLifesteal`가 붙는다 (`CombatActionSystem.cpp:307`). 스킬은 명시적으로 `CanLifesteal`을 붙이지 않는 한 흡혈하지 않는다. `StatComponent::spellVamp`는 계산/아이템 정의/튜너 어디에서도 사용되지 않는다.

현재 lethality는 레벨 환산 없이 그대로 flat armor penetration으로 더해진다.

## 3. 17개 챔피언 기본 수치 원본

표기: `기본/레벨성장`, AS는 `기본/ratio/성장`, 사거리와 이동 속도는 Winters world unit이다.

| 챔피언 | HP | Mana | AD | AP | Armor | MR | AS | 이동 | BA 사거리 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Irelia | 590/124 | 350/50 | 65/3.5 | 0/0 | 36/4.7 | 32/2.05 | .656/.656/.025 | 5 | 2.1 |
| Yasuo | 590/110 | 300/50 | 60/3 | 0/0 | 30/4.6 | 32/2.05 | .697/.697/.033 | 5 | 2.5 |
| Kalista | 574/114 | 300/45 | 61/3.25 | 0/0 | 24/4.6 | 30/1.3 | .694/.694/.045 | 5 | 5.5 |
| Garen | 690/98 | 300/50 | 69/4.5 | 0/0 | 38/4.2 | 32/2.05 | .625/.625/.034 | 5 | 1.5 |
| Zed | 654/99 | 300/50 | 63/3.4 | 0/0 | 32/4.7 | 32/2.05 | .651/.651/.033 | 5 | 1.5 |
| Riven | 630/100 | 300/50 | 64/3 | 0/0 | 33/4.4 | 32/2.05 | .625/.625/.034 | 5 | 1.5 |
| Ezreal | 600/102 | 375/70 | 60/2.75 | 0/0 | 24/4.7 | 30/1.3 | .625/.625/.025 | 5 | 5.5 |
| Fiora | 620/99 | 300/50 | 66/3.3 | 0/0 | 33/4.7 | 32/2.05 | .69/.69/.032 | 5 | 1.5 |
| Jax | 665/103 | 300/50 | 68/3.375 | 0/0 | 36/4.6 | 32/2.05 | .638/.638/.034 | 5 | 1.5 |
| Lee Sin | 645/108 | 300/50 | 66/3.7 | 0/0 | 34/4.7 | 32/2.05 | .651/.651/.03 | 5 | 1.5 |
| Kindred | 610/99 | 300/50 | 65/3.25 | 0/0 | 29/4.7 | 30/1.3 | .625/.625/.034 | 5 | 5.5 |
| Master Yi | 669/105 | 300/50 | 65/2.5 | 0/0 | 33/4.7 | 32/2.05 | .679/.679/.025 | 5 | 1.5 |
| Annie | 560/102 | 418/25 | 50/2.65 | 0/0 | 23/4 | 30/1.3 | .625/.625/.02 | 5 | 6.25 |
| Ashe | 610/101 | 280/35 | 59/2.95 | 0/0 | 26/4.6 | 30/1.3 | .658/.658/.0333 | 5 | 6 |
| Viego | 630/109 | 300/50 | 57/3 | 0/0 | 34/4.6 | 32/2.05 | .658/.658/.025 | 5 | 1.5 |
| Yone | 620/105 | 300/50 | 60/2 | 0/0 | 33/4.7 | 32/2.05 | .625/.625/.025 | 5 | 1.5 |
| Sylas | 600/115 | 400/70 | 61/3 | 0/0 | 32/4.7 | 32/2.05 | .645/.645/.034 | 5 | 1.5 |

### 3.1 Q/W/E/R 기본 쿨다운·마나·사거리

표기: `CD초/Mana/Range`. 이것은 `champions.json`의 command admission/action 기본값이다. champion hook이 `SkillEffectGameplayDefs.json`의 별도 range를 읽으면 실제 hit query와 불일치할 수 있다.

| 챔피언 | Q | W | E | R |
|---|---|---|---|---|
| Irelia | 3/20/6 | 3/70/0 | 3/50/9 | 3/100/12 |
| Yasuo | 3/0/5 | 3/0/4 | 3/0/4.75 | 3/0/14 |
| Kalista | 3/50/16.5 | 3/0/12 | 3/30/12 | 3/100/0 |
| Garen | 3/0/0 | 3/0/0 | 3/0/1.65 | 3/0/4 |
| Zed | 3/75/9 | 3/40/6.5 | 3/50/2.5 | 3/0/6.25 |
| Riven | 3/0/4.5 | 3/0/0 | 3/0/0 | 3/0/0 |
| Ezreal | 3/28/12 | 3/50/12 | 3/70/4.75 | 3/100/250 |
| Fiora | 3/20/4 | 3/50/0 | 3/40/0 | 3/100/5 |
| Jax | 3/65/7 | 3/30/0 | 3/50/0 | 3/100/0 |
| Lee Sin | 3/50/11 | 3/50/7 | 3/50/4 | 3/0/3 |
| Kindred | 3/35/5.5 | 3/40/8 | 3/70/5.5 | 3/100/6 |
| Master Yi | 3/50/6 | 3/50/0 | 3/0/0 | 3/100/0 |
| Annie | 3/60/6.25 | 3/70/6 | 3/40/0 | 3/100/6 |
| Ashe | 3/50/0 | 3/75/9 | 3/0/400 | 3/100/200 |
| Viego | 3/0/5.5 | 3/0/4 | 3/0/6 | 3/0/6 |
| Yone | 3/0/4.75 | 3/0/6 | 3/0/4 | 3/0/10 |
| Sylas | 3/55/4 | 3/65/5 | 3/65/6 | 3/75/10 |

17명 모든 Q/W/E/R 기본 cooldown이 3초인 것은 데이터 파이프라인 성공이 아니라 placeholder 성격의 데이터가 일괄 들어간 상태다. 특히 Ezreal은 effect param의 rank reduction까지 적용하므로 현재 실제 쿨다운은 다음과 같다.

```text
Ezreal Q CD(r) = max(0, 3 - 0.25 × (r-1))
Ezreal E CD(r) = max(0, 3 - 3 × (r-1))
Ezreal R CD(r) = max(0, 3 - 15 × (r-1))
Ezreal Q Mana(r) = 28 + 3 × (r-1)
```

따라서 Ezreal E/R은 rank 2부터 0초가 될 수 있다. 이는 data-driven 구조 문제와 별도로 즉시 튜닝이 필요한 수치 결함이다. 또한 Ashe W는 admission range 9지만 effect hit range 12, Fiora W는 admission range 0이지만 effect hit range 6이다. 검증/시전 허용 범위와 실제 판정 범위를 한 source로 합쳐야 한다.

## 4. 17개 챔피언 실제 피해 공식

아래는 기대값이나 원작 수식이 아니라 **현재 코드가 실제로 만드는 값**이다. `AD`는 최종 AD, `bAD`는 bonus AD, `AP`는 최종 AP, `MH`는 대상 잃은 체력이다.

| 챔피언 | 현재 실제 공식 | 결손/위험 |
|---|---|---|
| Irelia | BA=`AD`; Q=`45+25r`; W=`30+40r`; E=`70+30r`; R=`250` | Q/W/E가 `r-1`이 아니라 `r`이라 rank 1이 70/70/100으로 시작. AD/AP 계수 없음. |
| Yasuo | BA=`AD`; Q=`60`, tornado=`100`, dash-area=`70`; E=`80`; R=`200` | 랭크/AD/AP 계수 없음. |
| Kalista | BA=`AD`; Q=`70`; E=`20+30×spearCount` fallback | E의 base/damagePerSpear가 JSON에 없어 C++ fallback 의존. 1 spear도 50이 된다. AD 계수 없음. |
| Garen | BA=`AD`; R true=`150+150(r-1)+0.25MH` | R만 server hook. Q/W/E 피해·방어·회전 로직 없음. |
| Zed | BA=`AD`; Q=`70+25(r-1)`; E=`65+20(r-1)`; R 만료=`현재 MH×0.30` | 패시브 잃은 체력 타격 없음. R은 mark 기간 누적 피해가 아니라 만료 순간 전체 잃은 체력을 사용한다. |
| Riven | BA=`AD`; Q=`0`; W=`0`; E shield=`70`; R2=`100+50(r-1)` | Q/W 피해 없음. R 잃은 체력 증폭과 AD 계수 없음. |
| Ezreal | BA=`AD`; Q=`20+25(r-1)+1.3AD+0.4AP`; W=`80+55(r-1)+1.0bAD+0.9AP`; E=`80+50(r-1)+0.6bAD+0.75AP`; R=`350+200(r-1)+1.0bAD+1.1AP` | 가장 완성도가 높다. 비챔피언/비에픽 R flat은 `150+75(r-1)`로 교체. 다만 수식 조립은 Ezreal 전용 C++. |
| Fiora | BA=`AD`; Q=`70`; W 피해=`0`; E 다음 2 BA 각각 `+30`; R 즉시=`80` | 패시브 vital 자체가 없다. 대상 최대 체력/잃은 체력 피해 모두 0. W 피해/반격 결과와 R vital/완료 heal-zone 없음. |
| Jax | BA=`AD`; Q=`70`; W 다음 BA=`+45`; E=`60`; R 활성 중 매 3번째 BA=`+70` | 전부 flat. 랭크/AD/AP 계수와 R 저항력 보너스 없음. |
| Lee Sin | BA=`AD`; Q1=`55+25(r-1)` hardcode; Q2=`95`; E=`0`; R=`150`; W shield=`80` | Q JSON base 95와 Q1 hardcode 55가 충돌. E 피해, W2, 모든 계수 없음. Q1은 공용 champion hook 밖 `CommandExecutor.cpp`에 있음. |
| Kindred | BA=`AD`; Q=`0`; W=`35` every 0.6s for 4s; E 3번째 BA=`+80`; R heal=`250+75(r-1)`, HP floor=1 | Q와 패시브/mark scaling 없음. W는 rank를 1로 고정해 enqueue. E 잃은/최대 체력 계수 없음. |
| Master Yi | BA=`AD`; Q/W/E 피해=`0`; R은 7초간 MS×1.35, bonus AS+.25 | passive/Q/W/E 구현 부재. |
| Annie | BA=`AD`; Q=`80+35r`; W=`70+45r`; R=`150+75r`; E shield=`50+45r`, Armor/MR=`5r`; Tibbers BA=`40+15r`, HP=`1000+250r` | `r-1`이 아니어서 Q/W/R/E와 summon rank 1이 한 단계 높다. AP 계수 없음. |
| Ashe | BA=`AD`, Q 활성 중 `AD+20`; W=`45`; R=`250` | W/R 랭크 및 AD/AP 계수 없음. Q는 flat bonus이고 multi-arrow 피해는 한 화살만 적용되는 별도 로직. |
| Viego | BA=`AD`; Q=`65`; W=`55`; R=`150` | 전부 flat, 랭크/AD/AP/대상 체력 계수 없음. passive possession은 별도 구현. |
| Yone | BA=`AD`; Q=`75`; W=`65`; R=`150`; E echo=`0` | passive 혼합 피해와 E 누적/재시전 피해 없음. 모든 피해 flat. |
| Sylas | BA=`AD`; Q=`0`; W=`0`; E2=`65+25(r-1)`; R은 45초 hijack | Q/W 부재. 훔친 궁극기 계수 변환은 별도 검증 필요. |

### 4.1 Fiora 요청 항목

현재 `FioraGameSim.cpp`와 `FioraSimComponent.h`에는 passive/vital/maxHP/missingHP/true-damage 상태가 없다. 따라서 **피오라 패시브 잃은 체력 피해는 수정이 덜 된 것이 아니라 구현 자체가 없다.**

계획에서 먼저 확정해야 할 기획 값:

- 기준 체력: 대상 `MaxHP`인지 `MissingHP`인지
- damage type: True/Physical
- 기본 비율과 bonus AD/AP 계수
- vital 방향 판정, 재생성 시간, heal/MS 보상

현재 파이프라인은 `targetMaxHpRatioOverride`와 `targetMissingHpRatioOverride`를 모두 지원하므로 결정만 확정되면 계산 엔진을 새로 만들 필요는 없다.

### 4.2 Zed 요청 항목

현재 잃은 체력 피해는 passive가 아니라 **R 만료 처리**에 있다 (`ZedGameSim.cpp:882-885`). 만료 순간 `maxHP-currentHP`의 30%를 물리 피해로 넣는다. mark 이후 받은 피해 누적 장부는 없다. Zed passive 잃은 체력 타격은 구현되어 있지 않다.

### 4.3 랭크 계산 불일치

정상적인 선형 랭크 표기의 기준은 다음으로 통일해야 한다.

```text
RankedValue(base, perRank, r) = base + perRank × (clamp(r,1,maxRank) - 1)
```

현재 Ezreal, Garen, Zed, Riven, Sylas는 이 형태를 쓰지만 Annie와 Irelia는 `base + perRank × r`을 써서 off-by-one이다.

## 5. 아이템 34개 JSON 수치와 패시브

표의 MoveSpeed는 JSON authoring 단위다. `StatSystem`은 `flatMoveSpeed × 0.01`을 Winters world 이동 속도에 더한다. 나머지 수치는 표의 값 그대로 해당 stat에 합산된다.

| ID | 아이템 | 가격 | 현재 적용 수치 |
|---:|---|---:|---|
| 1001 | Boots | 300 | MoveSpeed +25 |
| 1011 | Giant's Belt | 900 | HP +350 |
| 1018 | Cloak of Agility | 600 | Crit +15% |
| 1026 | Blasting Wand | 850 | AP +45 |
| 1027 | Sapphire Crystal | 400 | Mana +300 |
| 1028 | Ruby Crystal | 400 | HP +150 |
| 1029 | Cloth Armor | 300 | Armor +15 |
| 1031 | Chain Vest | 800 | Armor +40 |
| 1033 | Null-Magic Mantle | 400 | MR +20 |
| 1036 | Long Sword | 350 | AD +10 |
| 1037 | Pickaxe | 875 | AD +25 |
| 1038 | B. F. Sword | 1300 | AD +40 |
| 1042 | Dagger | 250 | AS +10% |
| 1043 | Recurve Bow | 700 | AS +15% |
| 1052 | Amplifying Tome | 400 | AP +20 |
| 1053 | Vampiric Scepter | 900 | AD +15, Lifesteal 7% |
| 1054 | Doran's Shield | 450 | HP +110 |
| 1055 | Doran's Blade | 450 | AD +10, HP +80 |
| 1056 | Doran's Ring | 400 | AP +18, HP +90 |
| 1057 | Negatron Cloak | 900 | MR +45 |
| 1058 | Needlessly Large Rod | 1200 | AP +65 |
| 3006 | Berserker's Greaves | 1100 | AS +25%, MoveSpeed +45 |
| 3020 | Sorcerer's Shoes | 1100 | MagicPen +15, MoveSpeed +45 |
| 3031 | Infinity Edge | 3450 | AD +65, Crit +25% |
| 3047 | Plated Steelcaps | 1200 | Armor +25, MoveSpeed +45 |
| 3065 | Spirit Visage | 2700 | HP +450, MR +50, Haste +10 |
| 3072 | Bloodthirster | 3400 | AD +80, Lifesteal 15% |
| 3078 | Trinity Force | 3333 | AD +45, HP +300, AS +30%, Haste +15 |
| 3089 | Rabadon's Deathcap | 3500 | AP +130 |
| 3111 | Mercury's Treads | 1200 | MR +20, MoveSpeed +45 |
| 3153 | Blade of the Ruined King | 3000 | AD +40, AS +25%, Lifesteal 10% |
| 3157 | Zhonya's Hourglass | 3250 | AP +105, Armor +50 |
| 3158 | Ionian Boots of Lucidity | 950 | Haste +15, MoveSpeed +45 |
| 3742 | Dead Man's Plate | 2900 | HP +350, Armor +45, MoveSpeed +5 |

`ItemDef`은 가격, 표시명, `ItemStatModifier`만 가진다. passive/active/on-hit/cooldown/unique group 대상 필드가 없다. 따라서 현재 BORK 공식은 다음뿐이다.

```text
구매 후 AD +40
bonus AS +0.25
lifesteal +0.10
대상 최대 체력 비례 온힛 피해 = 0
```

## 6. 스킬 레벨업 규칙

근거: `Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`, `Shared/GameSim/Systems/SkillRank/SkillRankSystem.cpp`, `Shared/GameSim/Spawn/ChampionGameplayAssembly.cpp`

authoritative 플레이어 명령의 현재 규칙:

```text
사용 가능 포인트 = championLevel - 이미 사용한 포인트
Q/W/E 최대 랭크 = 5
R 최대 랭크 = 3
Q/W/E 다음 랭크 요구 레벨 = 1 + currentRank * 2 = 1/3/5/7/9
R 다음 랭크 요구 레벨 = 6/11/16
```

구조 결함:

- 위 레벨 제한은 `CommandExecutor::HandleLevelSkill`에 다시 구현되어 있고 `SkillRankSystem::TryLevelSkill`에는 없다.
- `ChampionGameplayAssembly`, SimLab, client smoke가 공용 helper를 직접 부르면 레벨 제한을 우회한다.
- 요구 레벨 배열은 JSON이 아니라 C++ 상수다. 기획자가 데이터만으로 조절할 수 없다.
- 특정 스킬 선행 조건을 표현하는 schema는 없다.

따라서 일반 플레이어 명령은 현재 제한을 지키지만, 모든 생성·테스트 경로가 동일 규칙을 공유하지 않아 Data Driven 판정은 부분 통과다.

## 7. 인게임 수치 조정 도구 감사

### 7.1 가능한 것

`Client/Private/UI/ChampionTuner.cpp`는 네트워크 practice command를 통해 다음을 임시 조정한다.

- 16개 champion base/growth stat + 별도 effective AS
- 14개 item 가격/stat
- 14개 skill effect param
- JSON 저장/불러오기: `Resource/Config/Practice/practice_balance_overrides.json`
- Debug server의 6개 canonical JSON reload 요청

### 7.2 불가능하거나 위험한 것

- scratch override JSON은 canonical `champions.json`, `SkillEffectGameplayDefs.json`, `ItemGameplayDefs.json`에 merge되지 않는다.
- skill UI는 enum 전체가 아니라 14개 param만 노출한다. AD/AP/잃은 체력 계수, max stack 등 다수가 빠져 있다.
- item passive가 schema에 없어 BORK 체력 비례 피해를 조절할 수 없다.
- 서버가 없으면 gameplay override를 적용/관찰할 수 없다. 이는 서버 권위 구조상 정상이나, 오프라인 authoring/validation 경로가 따로 필요하다.
- 새 item/key는 Debug reload로 추가할 수 없다.
- 저장 후 validate/codegen/SimLab/build까지 이어지는 단일 버튼 또는 명령 안내가 없다.

## 8. 애니메이션 수치 전수 감사

현재 ownership이 세 갈래다.

| 수치 | authoring/코드 | 실제 소비자 | 판정 |
|---|---|---|---|
| 서버 action lock | `champions.json`의 `lockDurationSec` | GameSim command/action lock | 정상 |
| 네트워크 재생 속도 | `ChampionVisualDefs.json.animationPlaybackSpeed` -> generated client pack | `Scene_InGameNetwork.cpp`, `EventApplier.cpp` | 정상이나 hot reload 없음 |
| visual cast/recovery frame | `ChampionVisualDefs.json.castFrame/recoveryFrame` | 소비자 없음 | dead data |
| legacy cast/recovery/play speed | `SkillTable.cpp`, 각 champion `_Registration.cpp`의 `SkillDef` | local skill hook bridge | 살아 있는 중복 진실 |
| Skill Timing Tuner | `SkillTimingPanel.cpp` | `g_SkillTable`을 `const_cast`로 즉시 수정 | Debug 메모리 전용, 저장 없음 |

추가 문제:

- `SkillTimingPanel`의 champion label은 Irelia/Yasuo 외에는 `?`이다.
- 확정값은 주석대로 사람이 `SkillTable.cpp`에 복사해야 한다.
- generated visual pack의 17개 챔피언 95개 stage에서 cast/recovery 값은 전부 0이고, 코드는 playback speed만 읽는다.
- gameplay lock과 visual recovery의 정합성 검사가 자동화되어 있지 않다.

따라서 애니메이션 수치는 현재 기획자가 안전하게 직접 튜닝할 수 있는 Data Driven 상태가 아니다.

## 9. 수정 우선순위와 회귀 위험

### P0: 공식 진실 경로 단일화

1. skill effect에 rank별 피해/AD/AP/대상 체력 계수/damage type/flags를 구조화한다.
2. 모든 champion hook은 공용 builder로 `DamageRequest`를 만든다.
3. Annie/Irelia off-by-one을 표준 `rank-1`로 고친다.
4. `SkillScalingTable` dead path는 실제 pack에 연결하거나 제거해 두 번째 진실을 없앤다.

회귀 위험: 기존 flat 수치가 rank 1에서 달라질 수 있다. 17명 × 모든 rank golden test가 선행되어야 한다.

### P0: item passive

1. ItemDef에 typed on-hit formula를 추가한다.
2. projectile/melee 모두 **실제 충돌 시점**에 한 번만 on-hit가 적용되게 한다.
3. BORK의 대상 최대 체력 계수는 기획 확정값으로 JSON에 둔다.

회귀 위험: 발사 시점과 충돌 시점 중복, Ashe Q 다중 화살, Ezreal Q `OnHit`, Kalista projectile에서 다중 proc 위험이 높다.

### P1: 누락 챔피언 기능

- Fiora passive/vital, W damage/counter, R completion
- Zed passive 및 R 누적 피해 정의
- Garen Q/W/E
- Riven Q/W 및 R 잃은 체력 증폭
- Kindred Q/passive
- Master Yi passive/Q/W/E
- Sylas Q/W
- Yone passive/E echo
- Lee Sin E/W2와 Q1/Q2 데이터 통합
- 나머지 챔피언의 rank/AD/AP 계수

### P1: authoring/tuning

1. scratch override와 canonical authoring JSON을 구분해 표시한다.
2. `Save Draft -> Validate -> Codegen -> Debug Reload/SimLab` 흐름을 제공한다.
3. animation cast/recovery/playback을 `ChampionVisualDefs.json` 한 곳에 모으고 runtime consumer를 연결한다.
4. server lock은 gameplay JSON에 유지하고 visual recovery와의 정합성만 검증한다.

## 10. Data Driven 합격 기준

다음이 모두 충족되어야 합격으로 바뀐다.

- [ ] 17명 × BA/Q/W/E/R의 피해 수치와 계수가 canonical JSON/pack에서 검색 가능
- [ ] champion C++에 밸런스 숫자 fallback이 없음
- [ ] 모든 rank 예상 피해 golden test 통과
- [ ] Fiora/Zed/BORK의 max/missing HP 기준과 damage type이 데이터로 명시
- [ ] lifesteal/on-hit/crit 적용 여부가 flags로 데이터화되고 중복 proc test 통과
- [ ] 플레이어·봇·SimLab이 동일한 Q/W/E 및 R skill rank rule test 통과
- [ ] Debug reload와 generated Release pack의 동일 입력 parity test 통과
- [ ] 인게임 draft 저장 결과를 validate/codegen할 수 있음
- [ ] animation cast/recovery/playback의 단일 visual source와 실제 runtime consumer 존재
- [ ] server gameplay lock과 client visual timing을 분리하되 검증식이 존재

## 11. 조사 근거 파일

- `Data/Gameplay/ChampionGameData/champions.json`
- `Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json`
- `Data/LoL/ServerPrivate/Gameplay/ItemGameplayDefs.json`
- `Data/LoL/ClientPublic/Visual/ChampionVisualDefs.json`
- `Tools/LoLData/Build-LoLDefinitionPack.py`
- `Server/Private/Data/RuntimeGameplayDefinitionOverlay.cpp`
- `Shared/GameSim/Systems/Combat/CombatFormula.cpp`
- `Shared/GameSim/Systems/Damage/DamagePipeline.cpp`
- `Shared/GameSim/Systems/Stat/StatSystem.cpp`
- `Shared/GameSim/Systems/SkillRank/SkillRankSystem.cpp`
- `Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`
- `Shared/GameSim/Spawn/ChampionGameplayAssembly.cpp`
- `Shared/GameSim/Systems/Combat/CombatActionSystem.cpp`
- `Shared/GameSim/Definitions/SkillAtomData.h`
- `Shared/GameSim/Definitions/SkillScalingTable.h`
- `Shared/GameSim/Definitions/ItemDef.h`
- `Shared/GameSim/Definitions/ChampionRuntimeDefaults.cpp`
- `Shared/GameSim/Champions/*/*GameSim.cpp`
- `Client/Private/UI/ChampionTuner.cpp`
- `Client/Private/UI/SkillTimingPanel.cpp`
- `Client/Private/GameObject/SkillTable.cpp`
- `Client/Private/Scene/Scene_InGameNetwork.cpp`
- `Client/Private/Network/Client/EventApplier.cpp`
