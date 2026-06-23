# Winters Data-Driven P3 LeeSin MarkDurationSec 결과 보고서

## 1. 결론

이번 반영은 LeeSin Q의 표식 지속시간을 코드 고정값의 소유물에서 서버 전용 JSON definition의 소비 대상으로 이동한 작업이다.

전체 DataDriven 검증 파이프라인은 통과했다.

```text
[LoLDataDriven] PASS
```

생성된 LoL definition pack hash:

```text
0x58295D30
```

## 2. 이번에 확정한 원자

새 원자:

```cpp
eSkillEffectParamId::MarkDurationSec
```

이 값은 일반 `DurationSec`가 아니다.

- `SlowDurationSec`: 둔화 상태가 유지되는 시간
- `StunDurationSec`: 기절 상태가 유지되는 시간
- `AirborneDurationSec`: 에어본 상태가 유지되는 시간
- `MarkDurationSec`: 표식이 살아있어 후속 입력을 허용하는 시간

LeeSin Q1은 즉시 피해만 주는 스킬이 아니라, 대상에게 Q2의 조건이 되는 표식을 남긴다. 따라서 이 값은 Q 효과의 일부이지만, 단순 피해/둔화/에어본 지속시간과 섞이지 않는 별도 원자로 분리했다.

## 3. 반영 내용

### 3.1 공용 스킬 효과 원자 추가

파일:

```text
Shared/GameSim/Definitions/SkillAtomData.h
```

추가:

```cpp
MarkDurationSec
```

### 3.2 JSON authoring 키 추가

파일:

```text
Tools/LoLData/Build-LoLDefinitionPack.py
```

추가:

```python
"markDurationSec": "MarkDurationSec"
```

### 3.3 LeeSin Q 데이터 추가

파일:

```text
Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json
```

추가:

```json
"markDurationSec": 3.0
```

### 3.4 호출 경계 수정

기존:

```cpp
LeeSinGameSim::ApplySonicWaveMark(CWorld& world, EntityID source, EntityID target)
```

변경:

```cpp
LeeSinGameSim::ApplySonicWaveMark(CWorld& world, const TickContext& tc, EntityID source, EntityID target)
```

이유:

`GameplayDefinitionQuery`는 `TickContext.pDefinitions`를 통해 immutable definition pack을 읽는다. 따라서 LeeSin Q projectile hit 지점에서 별도 싱글턴이나 우회 경로를 만들지 않고, 이미 프레임에 존재하는 `TickContext& tc`를 그대로 전달했다.

## 4. 런타임 흐름

이제 LeeSin Q 표식 생성은 아래 흐름을 따른다.

```text
ServerPrivate JSON
-> Build-LoLDefinitionPack.py
-> LoLGameplayDefinitions.generated.cpp
-> TickContext.pDefinitions
-> GameplayDefinitionQuery::ResolveSkillEffectParam(...)
-> LeeSinQMarkComponent.fRemainingSec
```

이 흐름에서 GameSim은 더 이상 표식 지속시간의 최종 소유자가 아니다. GameSim은 표식을 생성하고 감소시키는 실행 책임만 가진다.

## 5. 회귀 방지

기존 상수는 즉시 삭제하지 않았다.

```cpp
kLeeSinQMarkDurationSec
```

이 상수는 이제 주인이 아니라 fallback이다. definition pack이 없는 비정상/테스트 경로에서 이전 런타임 동작을 유지하기 위한 안전장치다.

삭제 조건은 다음과 같다.

```text
정상 Server/GameSim/SimLab 경로가 모두 TickContext.pDefinitions를 가진다는 것이 확인된다.
그 뒤 fallback 상수를 제거하고 audit candidate를 실제로 줄인다.
```

## 6. 검증 결과

### 6.1 생성기 문법 검증

```powershell
python -m py_compile Tools/LoLData/Build-LoLDefinitionPack.py
```

결과: PASS

### 6.2 Definition pack 생성

```powershell
python Tools/LoLData/Build-LoLDefinitionPack.py --root .
```

결과:

```text
Generated LoL definition pack 0x58295D30
Champions: 17, skills: 85, summoner spells: 1
```

### 6.3 Definition pack freshness check

```powershell
python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check
```

결과:

```text
Checked LoL definition pack 0x58295D30
Champions: 17, skills: 85, summoner spells: 1
```

### 6.4 Legacy data audit

```powershell
powershell -ExecutionPolicy Bypass -File Tools/LoLData/Collect-LoLLegacyDataAudit.ps1
```

결과:

```text
skillEffectHardcodeCandidates = 156
skillEffectDataQueryReaders = 76
```

reader 수는 75에서 76으로 증가했다. hardcode candidate가 유지된 이유는 fallback 상수를 아직 회귀 방지용으로 남겼기 때문이다.

### 6.5 전체 DataDriven 파이프라인

```powershell
powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1
```

결과: PASS

주요 확인:

- Definition pack freshness PASS
- Legacy ownership audit PASS
- Client visual timing parity mismatch 0
- GameSim build PASS
- Server build PASS
- Client build PASS
- SimLab deterministic regression PASS
- same-seed replay hash: `67F2A97563B8DB04`
- seed+1 hash: `5DA19645E291A29B`
- whitespace validation PASS

기존 C4828/C4251/C4275 경고와 CRLF 경고가 출력되었지만, 이번 변경으로 인한 실패는 없었다.

## 7. 본질 검토

이번 변경이 본질에 가까운 이유:

- JSON은 수치를 소유한다.
- Generated pack은 런타임 불변 데이터만 제공한다.
- `TickContext`는 프레임 안에서 definition pack을 전달한다.
- GameSim은 표식 적용/감소/소멸이라는 실행 책임만 가진다.
- Server projectile hit 경계는 이미 `TickContext`를 갖고 있으므로 새 전역 접근 경로를 만들지 않는다.

아직 남은 의심:

```text
fallback 상수가 오래 남으면 다시 코드가 데이터의 주인이 된다.
```

따라서 다음 단계는 fallback 삭제가 아니라, fallback이 없어도 되는 경로를 하나씩 증명하는 것이다.

## 8. 다음 단계

1. `TickContext.pDefinitions`가 없는 fallback 경로를 audit한다.
2. 남은 champion effect literals를 즉시 효과 수치와 상태/표식 정책 수치로 분류한다.
3. Viego/Yone/Zed/Irelia 쪽에서 같은 방식으로 `SkillEffectParam` reader를 늘린다.
4. 모든 정상 경로가 definition pack을 안정적으로 받으면 fallback 상수 삭제 단계를 시작한다.
