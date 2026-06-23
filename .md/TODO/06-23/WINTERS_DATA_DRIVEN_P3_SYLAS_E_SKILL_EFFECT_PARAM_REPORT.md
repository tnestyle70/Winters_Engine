# Winters DataDriven P3 Sylas E SkillEffect Param Cutover Report

작성일: 2026-06-23

## 1. 이번 목표

Sylas E 스킬의 런타임 숫자 소유권을 코드 상수에서 `Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json`으로 이동한다.

이번 슬라이스의 북극성은 하나다.

- 코드는 “무엇을 할지”만 실행한다.
- JSON은 “얼마나 강한지, 얼마나 빠른지, 얼마나 오래인지”를 소유한다.
- GameSim은 서버 권위 런타임이므로 JSON에서 빌드된 definition pack만 읽는다.
- Client visual, Engine, UI는 이 gameplay truth를 소유하지 않는다.

## 2. 원자 단위로 남긴 본질

Sylas E는 하나의 스킬 슬롯이지만 실제 gameplay atom은 둘이다.

1. E1 directional dash
   - `dashDistance`
   - `dashDurationSec`

2. E2 chain projectile and target dash
   - `speed`
   - `radius`
   - `baseDamage`
   - `damagePerRank`
   - `gap`
   - `targetDashDurationSec`
   - `airborneDurationSec`

처음에는 `dashDurationSec` 하나만으로 E1과 E2를 같이 표현할 수도 있었지만, 계속 의심하면 이것은 덜 본질적이다. E1의 dash duration과 E2의 target dash duration은 같은 “시간”처럼 보여도 의미가 다르다.

그래서 새 atom으로 `TargetDashDurationSec`를 추가했다.

## 3. 반영 파일

### 3.1 `Shared/GameSim/Definitions/SkillAtomData.h`

추가된 gameplay param id:

```cpp
TargetDashDurationSec
```

의미:

- 대상에게 맞은 뒤 caster가 target 쪽으로 붙는 E2 target dash 시간이다.
- E1의 자유 방향 dash 시간인 `DashDurationSec`와 분리된다.

### 3.2 `Tools/LoLData/Build-LoLDefinitionPack.py`

JSON key를 enum id로 매핑했다.

```python
"targetDashDurationSec": "TargetDashDurationSec",
```

이제 JSON에 `targetDashDurationSec`를 적으면 generated definition pack에 `eSkillEffectParamId::TargetDashDurationSec`로 들어간다.

### 3.3 `Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json`

`skill.sylas.e`에 E atom 값을 추가했다.

```json
{
  "key": "skill.sylas.e",
  "params": {
    "airborneDurationSec": 0.75,
    "baseDamage": 65.0,
    "damagePerRank": 25.0,
    "dashDistance": 3.25,
    "dashDurationSec": 0.16,
    "gap": 0.85,
    "radius": 0.55,
    "speed": 26.0,
    "targetDashDurationSec": 0.22
  }
}
```

### 3.4 `Shared/GameSim/Champions/Sylas/SylasGameSim.cpp`

코드가 직접 숫자를 결정하던 경로를 definition query 기반으로 바꿨다.

- E1 dash distance: `DashDistance`
- E1 dash duration: `DashDurationSec`
- E2 chain speed: `Speed`
- E2 hit radius: `Radius`
- E2 base damage: `BaseDamage`
- E2 damage per rank: `DamagePerRank`
- E2 target dash gap: `Gap`
- E2 target dash duration: `TargetDashDurationSec`
- E2 airborne duration: `AirborneDurationSec`

기존 fallback 상수는 남겼다. 이것은 아직 완전 컷오버 전 단계에서 회귀를 막기 위한 안전장치다. 최종 단계에서는 “fallback 없이 data validation 실패”로 바꾸는 것이 더 본질적이다.

### 3.5 `Server/Private/Data/Generated/LoLGameplayDefinitions.generated.cpp`

팩 재생성 결과:

```text
kBuildHash = 0xD8FC25E4
```

`skill.sylas.e`에 `TargetDashDurationSec`까지 포함되어 generated pack으로 들어갔다.

## 4. 검증 결과

### 4.1 Definition pack 생성

명령:

```powershell
python Tools/LoLData/Build-LoLDefinitionPack.py --root .
```

결과:

```text
Generated LoL definition pack 0xD8FC25E4
Champions: 17, skills: 85, summoner spells: 1
```

### 4.2 Definition pack freshness

명령:

```powershell
python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check
```

결과:

```text
Checked LoL definition pack 0xD8FC25E4
Champions: 17, skills: 85, summoner spells: 1
```

### 4.3 Legacy ownership audit

명령:

```powershell
powershell -ExecutionPolicy Bypass -File Tools/LoLData/Collect-LoLLegacyDataAudit.ps1
```

결과 핵심:

```text
skillEffectHardcodeCandidates = 156
skillEffectDataQueryReaders = 86
SylasGameSim.cpp dataQueryReaders = 10
```

해석:

- data query reader가 이전 76에서 86으로 증가했다.
- hardcode candidate는 156으로 유지된다.
- 이유는 fallback 상수가 아직 의도적으로 남아 있기 때문이다.
- 따라서 “완전 삭제”는 아니고 “런타임 결정권을 JSON으로 이동한 단계”다.

### 4.4 전체 DataDriven 파이프라인

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

참고:

- 첫 번째 전체 파이프라인 실행에서는 `Shared/Schemas/run_codegen.bat`의 `flatc` codegen 중 `공유 위반입니다.`가 한 번 발생했다.
- `cmd /c Shared\Schemas\run_codegen.bat` 단독 실행은 통과했다.
- 재실행한 전체 파이프라인도 최종 `PASS`했다.
- 따라서 이번 Sylas E 코드 변경으로 인한 컴파일 실패가 아니라 일시적인 파일 공유 위반으로 판단한다.

## 5. 회귀 관점

이번 변경은 기존 런타임 동작을 보존한다.

- JSON 값은 기존 Sylas E 상수와 동일하게 넣었다.
- definition query 실패 시 기존 fallback 값으로 동작한다.
- E1과 E2의 duration 의미를 분리했으므로 이후 밸런스 패치에서 한쪽 값만 수정할 수 있다.
- Server/GameSim만 gameplay truth를 읽고, Client visual 쪽으로 권위가 넘어가지 않는다.

## 6. 다음 방향

다음 P3 슬라이스에서는 같은 기준으로 남은 champion skill effect 값을 계속 JSON으로 내린다.

우선순위:

1. hardcode candidate가 높은 champion부터 진행한다.
2. fallback 상수는 당장 삭제하지 않고, data query reader를 먼저 늘린다.
3. 모든 champion skill이 JSON 값을 읽게 된 뒤, validation 단계에서 필수 param 누락을 실패로 바꾼다.
4. 그 다음 fallback 상수를 제거한다.

다음에 의심해야 할 질문:

- 이 값은 gameplay truth인가, visual playback인가?
- 이 값은 champion별 고정값인가, skill stage별 값인가?
- 같은 이름의 값이 실제로 같은 의미인가?
- 코드가 이 값을 계산해야 하는가, 데이터가 이 값을 소유해야 하는가?
- fallback이 아직 필요한가, 아니면 validation 실패가 더 본질적인가?
