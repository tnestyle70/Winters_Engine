# Gameplay Formula / Skill Rank / Animation Data Driven 구현 결과 보고서

작성일: 2026-07-16
Session - 감사 보고서와 교정 계획을 기준으로 현재 구현된 17개 챔피언의 BA/Q/W/E/R 공식, 아이템 온힛, 스킬 비용·쿨다운·랭크, 시각 타이밍을 검증 가능한 단일 데이터 경로로 이관했다.

## 0. 최종 판정

**현재 런타임에 구현되어 있는 17개 챔피언, 85개 BA/Q/W/E/R 슬롯의 수치 소유 구조는 Data Driven 합격이다.**

다음 항목을 완료했다.

| 범위 | 결과 | 근거 |
|---|---|---|
| 챔피언 기본 스탯 | PASS | 17개 챔피언 JSON과 생성물 일치, hash `0x9D6886A7` |
| 스킬 피해 공식 | PASS | 85/85 슬롯에 명시적 `damage`, pack hash `0x8E9EF70F` |
| 랭크별 비용·쿨다운 | PASS | rank 배열을 정의·코드젠·조회 경로가 공통 소비 |
| 스킬 레벨 제한 | PASS | Q/W/E 1·3·5·7·9, R 6·11·16을 `SkillRankSystem`에서 단일 처리 |
| 방어력/마법저항력 | PASS | 물리/마법/고정 타입과 기존 공통 저항 공식 유지 |
| 생명력 흡수 | PASS | 방어·보호막 적용 후 실제 감소량 × lifesteal |
| 아이템 온힛 | PASS | `ItemDef.onHitDamage` 추가, BORK 10% 최대 체력 물리 피해 적용 |
| 애니메이션 수치 | PASS | 17명, 96 stage의 playback/cast/recovery를 ClientPublic JSON으로 이관 |
| 기획자 임시 조정 | PASS(경계 포함) | gameplay는 서버 practice overlay, visual은 review draft로 분리 |
| 기존 이중 공식 경로 | PASS | 빈 `SkillScalingRegistry/Table` reader 및 파일 제거 |

단, 이 판정은 **현재 구현의 수치 소유 구조**에 대한 판정이다. 피오라 급소 패시브의 정확한 피해 타입·대상 체력 기준·방향 판정·보상, 제드 패시브와 R 죽음의 표식의 최종 의미 분리처럼 제품 명세가 확정되지 않은 신규 기능까지 구현 완료라고 주장하지 않는다. 기존 제드 R의 대상 잃은 체력 30% 공식은 데이터로 이관했지만, 별도 제드 패시브를 임의로 만들지는 않았다.

## 1. 단일 진실 경로

### 1.1 서버 권위 gameplay

```text
Data/Gameplay/ChampionGameData/champions.json
Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json
Data/LoL/ServerPrivate/Gameplay/ItemGameplayDefs.json
    -> validate / codegen
    -> immutable generated definitions
    -> Shared/GameSim query
    -> DamageRequest
    -> DamagePipeline
    -> Snapshot / Event
    -> Client presentation
```

클라이언트는 피해량, 적중 결과, 체력 회복을 새로 계산하지 않는다. 챔피언 hook은 타깃, 다단 적중, 투사체, 변형 stage를 선택하지만 같은 랭크 피해 수치를 두 번째 C++ 상수로 소유하지 않는다.

### 1.2 클라이언트 visual

```text
Data/LoL/ClientPublic/Visual/ChampionVisualDefs.json
    -> validate / codegen
    -> LoLVisualDefinitions.generated.cpp
    -> SkillRegistry
    -> animation playback / cast / recovery timing
```

`SkillTimingPanel`의 저장 파일은 `Client/Bin/Resource/Config/Practice/practice_visual_timing_overrides.json` review draft다. release 값은 반드시 `ChampionVisualDefs.json`에 병합하고 codegen/build를 통과해야 한다.

## 2. 실제 피해 공식

rank `r`의 저항 적용 전 피해는 다음 하나의 구조로 계산한다.

```text
raw(r) = flat[r]
       + totalADRatio[r] * totalAD
       + bonusADRatio[r] * bonusAD
       + apRatio[r] * AP
       + targetMaxHPRatio[r] * targetMaxHP
       + targetMissingHPRatio[r] * (targetMaxHP - targetCurrentHP)
```

피해 타입별 유효 저항 `R`의 multiplier는 기존 공통 공식을 그대로 사용한다.

```text
R >= 0: multiplier = 100 / (100 + R)
R <  0: multiplier = 2 - 100 / (100 - R)
postResistance = raw * multiplier
```

그 뒤 치명타 정책, Yasuo/일반 보호막, Annie E 보호막, 체력 floor를 적용한다. 최종 피해는 실제 체력 감소량이다.

```text
finalDamage = previousHP - currentHP
lifestealHeal = finalDamage * source.lifesteal
sourceHP = min(sourceMaxHP, sourceHP + lifestealHeal)
```

따라서 overkill 예정량이나 보호막에 흡수된 양은 흡혈로 되돌아오지 않는다.

## 3. 데이터 구조와 소비 코드

공통 공식 형식은 `DamageFormulaDef` 하나다.

```cpp
struct DamageFormulaDef
{
    static constexpr u8_t kMaxRank = 5u;
    bool_t bValid = false;
    u8_t rankCount = 1u;
    eDamageType type = eDamageType::Physical;
    u32_t flags = DamageFlag_None;
    f32_t flatByRank[kMaxRank]{};
    f32_t totalAdRatioByRank[kMaxRank]{};
    f32_t bonusAdRatioByRank[kMaxRank]{};
    f32_t apRatioByRank[kMaxRank]{};
    f32_t targetMaxHpRatioByRank[kMaxRank]{};
    f32_t targetMissingHpRatioByRank[kMaxRank]{};
};
```

`GameplayDefinitionQuery::BuildSkillDamageRequest`가 현재 rank의 여섯 계수를 선택해 `DamageRequest`로 만든다. `DamagePipeline`은 JSON이나 registry를 다시 조회하지 않고 해석 완료된 request만 계산한다.

기존 `scalingTableId` 생성 호환 필드는 한 migration 동안 0으로 남지만 reader는 없다. 빈 registry와 table 파일, 등록 호출은 제거했기 때문에 두 번째 공식 경로가 런타임에 존재하지 않는다.

## 4. 17개 챔피언 / 85개 슬롯 이관

대상 roster는 Irelia, Yasuo, Kalista, Garen, Zed, Riven, Ezreal, Fiora, Jax, Lee Sin, Kindred, Master Yi, Annie, Ashe, Viego, Yone, Sylas다.

- BA/Q/W/E/R 85개 모두 `damage` 객체를 가진다.
- 피해가 없는 이동·방어·소환 스킬도 명시적 zero formula를 가져 누락과 0 피해를 구분한다.
- 현재 타입 분포는 Physical 75, Magic 9, True 1이다. zero formula도 명시 타입을 포함한다.
- 기본 공격은 `totalADRatio=1`, `CanCrit|CanLifesteal|OnHit` 계약으로 통일했다.
- Ezreal Q rank 3 probe는 `flat=70`, `totalAD=1.3`, `AP=0.4`를 생성물에서 직접 확인한다.
- Zed R은 기존 의미를 보존해 `targetMissingHPRatio=0.3`을 canonical data로 옮겼다.

다음 네 경로는 stage/누적 규칙 때문에 중앙 단순 교체에서 제외했지만 숫자 자체는 canonical params에 있다.

| 경로 | 이유 | 데이터 |
|---|---|---|
| Yasuo Q | base/tornado/dash-area 변형 | 각 variant param 필수 검증 |
| Kalista E | spear 수 누적 | `baseDamage`, `damagePerSpear` |
| Lee Sin Q | Q1/Q2 및 projectile 시점 | projectile 생성도 `BuildSkillDamageRequest` 사용 |
| Ezreal R | non-epic 연속 적중 감쇠 | base/per-rank param 필수 검증 |

codegen은 위 variant param 누락을 오류로 처리한다. 따라서 예전처럼 C++ fallback이 조용히 살아남지 않는다.

## 5. 아이템과 생명력 흡수

34개 아이템은 기존 가격/flat stat 정의를 유지하고 `onHitDamage` 계약을 추가했다. 현재 실제 passive가 연결된 항목은 BORK다.

```json
{
  "itemId": 3153,
  "stats": {
    "flatAd": 40.0,
    "bonusAttackSpeed": 0.25,
    "lifeSteal": 0.1
  },
  "onHitDamage": {
    "type": "Physical",
    "targetMaxHpRatioByRank": [0.1]
  }
}
```

기본 공격의 `OnHit` request에서 인벤토리를 한 번 순회하고 같은 피해 타입의 item formula를 합산한다. SimLab은 기본 공격 100 + 최대 체력 1000의 10%=100, 총 200 피해와 20% lifesteal=40 회복을 검증했다.

BORK 10%는 이번 구현 기준값이다. 기획 변경은 JSON의 `0.1`만 수정하면 되며 C++ 수정은 필요 없다. 향후 한 공격에 물리/마법 혼합 온힛을 허용할 때는 타입별 별도 request로 확장해야 한다. 현재 코드는 타입이 다른 passive를 조용히 합산하지 않는다.

## 6. 랭크, 비용, 쿨다운

- `ChampionGameData`와 `SkillAtomData`에 rank별 mana/cooldown 배열을 추가했다.
- adapter, DB, query, Debug runtime overlay가 같은 rank 선택 함수를 사용한다.
- Ezreal Q의 mana `[28,31,34,37,40]`, cooldown `[3,3,3,3,3]`처럼 rank별 값이 실제 데이터다.
- Q/W/E 다음 rank 요구 레벨은 `2*r-1`, R은 `6+5*(r-1)`이다.
- 잘못된 레벨업은 point를 소비하지 않는다.

## 7. 애니메이션 수치 조정

초기 감사 당시 ClientPublic JSON의 cast/recovery가 대부분 0이어서 그대로 우선 적용하면 정상 등록값을 지우는 회귀가 발생했다. cutover 전에 기존 17명 등록값을 96 stage로 materialize했다.

- 96/96 stage playback speed > 0
- 85 stage cast frame > 0; 나머지 0은 즉시/2단 stage의 의도된 값
- 87 stage recovery frame > 0
- 모든 stage에서 `castFrame >= 0`, `recoveryFrame >= castFrame`

codegen은 champion/slot 누락, 비양수 playback, 음수 cast, recovery < cast를 실패시킨다. `SkillRegistry`는 생성된 canonical timing을 등록 직후 적용하며, 패널은 visual 값만 수정한다. 과거의 `const_cast` 및 gameplay lock duration 직접 수정은 제거했다.

## 8. 기획자 튜닝 절차

### Gameplay 수치

1. `Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json` 또는 `ItemGameplayDefs.json` 수정
2. `python Tools/LoLData/Build-LoLDefinitionPack.py`
3. `python Tools/LoLData/Build-LoLDefinitionPack.py --check`
4. SimLab build/run
5. Client/Server build

Debug practice overlay는 빠른 실험용이며 release truth가 아니다. 승인된 값은 반드시 위 JSON으로 환원한다.

### 애니메이션 수치

1. 인게임 `Skill Timing` 패널에서 champion/slot/stage 선택
2. playback/cast/recovery 조정 후 draft 저장
3. draft를 `ChampionVisualDefs.json`에 review/merge
4. codegen check와 Client build
5. 서버를 띄운 정상 F5 흐름에서 눈 검증

## 9. 검증 결과

| 검증 | 결과 |
|---|---|
| LoL definition pack `--check` | PASS, `0x8E9EF70F`, champions 17, skills 85 |
| ChampionGameData `--check` | PASS, `0x9D6886A7`, champions 17 |
| Shared boundary check | PASS |
| SimLab FormulaData | PASS, 17 champions / 85 skills |
| SimLab BORK | PASS, 10% max-HP on-hit + post-mitigation lifesteal |
| SimLab SkillRank | PASS, Q/W/E 및 R gate |
| SimLab same-seed | PASS, `D9B579C1425033BB` |
| SimLab seed+1 sensitivity | PASS, `71291010C7E00163` |
| Client Debug x64 `/m:1` | PASS, `Client/Bin/Debug/WintersGame.exe` |
| Server Debug x64 `/m:1` | PASS, `Server/Bin/Debug/WintersServer.exe` |

Client/Server의 기존 DLL interface C4251/C4275 warning은 남아 있지만 이번 변경으로 새 compile error나 test failure는 없다. 병렬 `/m`에서는 Client와 Server가 Shared FlatBuffers codegen을 동시에 호출해 간헐적으로 `flatc`가 실패할 수 있어, codegen target 직렬화 전까지 schema-owning aggregate 검증은 `/m:1`을 사용한다.

직접 인게임 검증 handoff를 위해 빌드 직후 `WintersServer.exe`를 숨김 창으로 실행했다. 이 세션의 PID는 47644이며 TCP 8086, 9000 listen 상태를 확인했다.

## 10. 남은 경계와 회귀 위험

| 위험 | 현재 방어 | 후속 조건 |
|---|---|---|
| variant 스킬을 중앙 formula가 덮어씀 | 4개 경로 명시 제외 + param validator | 새 variant 추가 시 같은 계약 등록 |
| item 혼합 피해 타입 | 다른 타입은 합산하지 않음 | 타입별 별도 request 설계 |
| generated compatibility `scalingTableId` | reader 0, 값 0 | schema migration 창 종료 후 필드 제거 |
| visual timing의 체감 회귀 | 96 stage materialize + validator | 정상 서버 F5에서 사용자 눈 검증 |
| Debug overlay 값이 release로 오인 | UI/문서에서 temporary 명시 | 승인값 JSON 환원 필수 |
| 피오라/제드 신규 passive 의미 오구현 | 임의 구현 금지 | 기획 명세 확정 후 formula + trigger 조건 추가 |

피오라 급소 패시브는 최소한 피해 타입, 현재/최대/잃은 체력 중 기준, 발동 방향 판정, cooldown, heal/move-speed 보상이 확정되어야 한다. 제드는 현재 R 표식 pop의 잃은 체력 30%와 별도 passive execute를 같은 기능으로 취급할지 분리할지 결정이 필요하다. 이는 Data Driven 기반의 실패가 아니라 아직 authoring할 제품 규칙이 확정되지 않은 상태다.

## 11. 주요 변경 파일

- `Shared/GameSim/Definitions/DamageTypes.h`
- `Shared/GameSim/Definitions/GameplayDefinitionQuery.cpp`
- `Shared/GameSim/Systems/Damage/DamageQueueSystem.cpp`
- `Shared/GameSim/Systems/Damage/DamagePipeline.cpp`
- `Shared/GameSim/Systems/SkillRank/SkillRankSystem.cpp`
- `Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json`
- `Data/LoL/ServerPrivate/Gameplay/ItemGameplayDefs.json`
- `Data/LoL/ClientPublic/Visual/ChampionVisualDefs.json`
- `Tools/LoLData/Build-LoLDefinitionPack.py`
- `Tools/ChampionData/build_champion_game_data.py`
- `Client/Private/GamePlay/SkillRegistry.cpp`
- `Client/Private/UI/SkillTimingPanel.cpp`
- `Client/Private/UI/ChampionTuner.cpp`
- `Tools/SimLab/main.cpp`

## 12. 결론

이전 상태는 JSON, 챔피언 C++ hook, 비어 있는 scaling registry, 임시 tuner가 동시에 수치 진실처럼 보이는 구조였다. 현재 상태는 **canonical JSON -> 검증/codegen -> immutable definition -> server-authoritative request/pipeline** 한 경로다. 현재 구현된 17명/85슬롯과 BORK, 비용·쿨다운·랭크, visual timing은 기획자가 데이터로 조정하고 회귀 테스트로 검증할 수 있다.

따라서 “현재 구현의 Data Driven 전환”은 완료로 판정한다. “LoL의 모든 미구현 패시브/스킬 기능 완성”은 별도 gameplay backlog이며, 미확정 명세를 이번 완료 판정에 숨기지 않는다.
