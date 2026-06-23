# Winters DataDriven P3 Yone/Viego/Zed SkillEffect Param Cutover Report

작성일: 2026-06-24

## 1. 이번 배치 목표

Yone, Viego, Zed의 서버 권위 skill effect 값을 코드 상수 소유에서 JSON authoring + generated immutable pack 조회로 이동했다.

북극성은 유지했다.

- JSON은 값의 원천이다.
- generated pack은 런타임에서 읽는 유일한 값 묶음이다.
- Shared/GameSim 코드는 값을 소유하지 않고, `entity + slot + param + TickContext`로 조회한 뒤 실행만 한다.
- Client visual/Engine/UI는 gameplay truth를 만들지 않는다.

## 2. 새로 추가한 atom

기존 atom을 억지로 재사용하지 않고, 실제로 의미가 분리되는 값만 추가했다.

```cpp
DashDelaySec
EffectDurationSec
TickIntervalSec
RefreshDurationSec
VanishDurationSec
MissingHealthDamageRatio
```

의심 결과:

- `EffectDurationSec`: Yone E soul 상태 지속, Viego E mist 지속, Zed shadow 지속처럼 “스킬 효과 상태가 남는 시간”이다.
- `DashDelaySec`: Yone R처럼 dash가 바로 시작되지 않는 지연 시간이다.
- `TickIntervalSec`: Viego E aura tick 간격이다.
- `RefreshDurationSec`: Viego E aura가 부여하는 상태의 refresh duration이다.
- `VanishDurationSec`: Zed R 시전자의 vanish/untargetable window다.
- `MissingHealthDamageRatio`: Zed R mark pop에서 missing health 기반 추가 피해 비율이다.

반대로 추가하지 않은 것:

- `range`: 이미 `SkillGameplayDef.range.rangeMax`가 소유한다. effect param으로 중복하지 않았다.
- `Zed R behind padding`: 기존 `Gap` atom으로 충분하다. target 주변에 남기는 간격이라는 의미가 같다.

## 3. 반영 내용

### 3.1 Yone

반영 파일:

- `Shared/GameSim/Champions/Yone/YoneGameSim.cpp`
- `Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json`

DataDriven으로 이동한 값:

- Q: `baseDamage`, `radius`
- W: `baseDamage`, `radius`
- E: `dashDistance`, `dashDurationSec`, `effectDurationSec`
- R: `baseDamage`, `radius`, `dashDelaySec`, `dashDurationSec`, `airborneDurationSec`

Range는 `ResolveSkillRange`로 읽는다.

### 3.2 Viego

반영 파일:

- `Shared/GameSim/Champions/Viego/ViegoGameSim.cpp`
- `Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json`

DataDriven으로 이동한 값:

- Q: `baseDamage`, `radius`
- W: `baseDamage`, `dashDurationSec`, `radius`, `stunDurationSec`
- E: `effectDurationSec`, `radius`, `tickIntervalSec`, `refreshDurationSec`
- R: `baseDamage`, `dashDurationSec`, `radius`, `slowDurationSec`, `moveSpeedMul`

Range는 `ResolveSkillRange`로 읽는다.

### 3.3 Zed

반영 파일:

- `Shared/GameSim/Champions/Zed/ZedGameSim.cpp`
- `Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json`

DataDriven으로 이동한 값:

- Q: `baseDamage`, `damagePerRank`, `speed`, `radius`
- W: `effectDurationSec`
- E: `baseDamage`, `damagePerRank`, `radius`, `slowDurationSec`, `moveSpeedMul`
- R: `effectDurationSec`, `gap`, `markDurationSec`, `missingHealthDamageRatio`, `vanishDurationSec`

Range는 `ResolveSkillRange`로 읽는다.

## 4. Definition pack 결과

재생성 결과:

```text
Generated LoL definition pack 0x7257D8A7
Champions: 17, skills: 85, summoner spells: 1
```

Freshness check:

```text
Checked LoL definition pack 0x7257D8A7
Champions: 17, skills: 85, summoner spells: 1
```

generated pack 확인:

```text
Server/Private/Data/Generated/LoLGameplayDefinitions.generated.cpp
kBuildHash = 0x7257D8A7
```

## 5. Audit 결과

이전 기준:

```text
skillEffectDataQueryReaders = 86
```

이번 배치 후:

```text
skillEffectDataQueryReaders = 132
```

파일별 신규 reader 묶음:

```text
YoneGameSim.cpp  = 14
ViegoGameSim.cpp = 16
ZedGameSim.cpp   = 16
```

`skillEffectHardcodeCandidates = 156`은 유지된다.

이유:

- fallback 상수는 아직 남겨 두었다.
- 현재 단계는 “런타임 결정권을 JSON pack으로 이동”하는 단계다.
- 모든 reader가 pack으로 붙은 뒤 validation을 강화하고 fallback 상수를 제거해야 hardcode count가 내려간다.

## 6. 전체 검증 결과

명령:

```powershell
powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1
```

결과:

```text
[LoLDataDriven] PASS
```

통과 항목:

- definition pack freshness
- legacy ownership audit
- client visual timing parity mismatch 0
- `Shared/GameSim/Include/GameSim.vcxproj`
- `Server/Include/Server.vcxproj`
- `Client/Include/Client.vcxproj`
- `Tools/SimLab/SimLab.vcxproj`
- SimLab deterministic regression
- whitespace validation

SimLab 결과:

```text
[SimLab] same-seed replay OK: hash=67F2A97563B8DB04
[SimLab] seed sensitivity OK: seed+1 hash=5DA19645E291A29B
[SimLab] PASS
```

## 7. Yasuo를 이번 배치에서 제외한 이유

Yasuo는 그냥 flat param을 더 추가해서 밀어 넣으면 오히려 본질이 깨진다.

문제:

- Yasuo Q 하나 안에 일반 Q, tornado Q, EQ area가 같이 있다.
- 현재 `SkillEffectSpec.params[]`는 skill 하나에 flat param 배열 하나만 가진다.
- 이 구조로는 `baseDamage`, `radius`, `speed`, `lifetimeSec`가 어떤 variant의 값인지 표현하기 어렵다.

따라서 Yasuo는 다음 슬라이스에서 먼저 `SkillEffectVariantSpec` 또는 동등한 구조를 도입해야 한다.

다음 방향:

```text
SkillEffectSpec
  -> common params
  -> variants[]
       key/stage/kind
       params[]
```

예상 variant:

- Yasuo Q normal wind
- Yasuo Q tornado
- Yasuo EQ area
- Yasuo E dash
- Yasuo R airborne hold / landing / damage

이렇게 해야 값 이름을 억지로 늘리지 않고도 “하나의 스킬 안에 여러 원자 effect가 있다”는 본질을 코드와 데이터가 같이 표현할 수 있다.

## 8. 다음 작업

1. Yasuo용 effect variant 구조를 설계하고 pack/generator/query에 추가한다.
2. Yasuo Q/E/R 값을 variant 단위 JSON으로 이동한다.
3. Irelia/Kindred의 남은 skill effect 값을 같은 방식으로 이동한다.
4. fallback 상수 제거 전, 모든 champion reader가 pack으로 붙었는지 audit한다.
5. 그 다음 validation을 “누락 시 fallback”에서 “누락 시 cook 실패”로 강화한다.
