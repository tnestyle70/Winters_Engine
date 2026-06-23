# WINTERS DATA-DRIVEN P3 ASHE/JAX SKILL EFFECT PARAM REPORT

작성일: 2026-06-23

## 결론

P3 첫 조각으로 Ashe/Jax의 스킬 효과 수치를 `SkillEffectParam` 원자 구조로 분리했다.

이번 slice의 본질은 다음 하나다.

> 스킬 런타임 함수는 숫자의 주인이 아니다. 런타임 함수는 `skill.ashe.w`, `skill.jax.q` 같은 스킬 정의에서 이름 있는 param을 읽어 실행한다.

전체 `Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1` 검증은 `PASS`다.

## 반영 범위

### Shared/GameSim: 모양만 소유

반영 파일:

- `Shared/GameSim/Definitions/SkillAtomData.h`
- `Shared/GameSim/Definitions/GameplayDefinitionQuery.h`
- `Shared/GameSim/Definitions/GameplayDefinitionQuery.cpp`

추가한 원자:

```cpp
enum class eSkillEffectParamId : u8_t
{
    None = 0,
    BaseDamage,
    DamagePerRank,
    Range,
    Speed,
    MoveSpeedMul,
    StunDurationSec,
    Gap,
    DashDurationSec,
};

struct SkillEffectParam
{
    eSkillEffectParamId id = eSkillEffectParamId::None;
    f32_t value = 0.f;
};
```

Shared에는 값이 아니라 모양만 있다. 값은 `ServerPrivate` authoring JSON과 generated pack에 있다.

### ServerPrivate Data: 값의 소유자

반영 파일:

- `Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json`
- `Data/LoL/ServerPrivate/Gameplay/SkillGameplayDefs.json`
- `Server/Private/Data/Generated/LoLGameplayDefinitions.generated.cpp`

이번에 이관한 값:

- `skill.ashe.w`: `baseDamage`, `moveSpeedMul`, `range`, `speed`
- `skill.ashe.r`: `baseDamage`, `range`, `speed`, `stunDurationSec`
- `skill.jax.q`: `baseDamage`, `dashDurationSec`, `gap`
- `skill.jax.e`: `stunDurationSec`

새 build hash:

```text
0xFDB27FA8
```

### Tools: generated pipeline

반영 파일:

- `Tools/LoLData/Build-LoLDefinitionPack.py`

추가한 검증:

- `SkillEffectGameplayDefs.json`의 `key`가 실제 canonical skill key인지 확인한다.
- param 이름이 허용된 `SKILL_EFFECT_PARAM_IDS`에 있는지 확인한다.
- param 개수가 `kSkillEffectParamMax`를 넘으면 실패한다.
- skill effect data를 definition pack hash 입력에 포함한다.

### Runtime Reader: pack 우선 조회

반영 파일:

- `Shared/GameSim/Champions/Ashe/AsheGameSim.cpp`
- `Shared/GameSim/Champions/Jax/JaxGameSim.cpp`

서버 권위 tick에서는 `TickContext.pDefinitions`를 통해 generated pack 값을 먼저 읽는다.

로컬 smoke처럼 pack이 없는 경로는 기존 상수 fallback을 사용한다. 이 fallback은 회귀 방지용이며, 최종 삭제 대상이다. 즉 이번 slice는 “서버 권위 reader cutover”까지이고, “모든 legacy literal 삭제”는 아직 아니다.

## 본질 검증

### 정말 더 나눌 수 없는가?

이번 원자는 `id + value`다.

- `id`: 값의 의미
- `value`: 값 자체

여기에 champion, target, projectile, status effect, animation, visual cue를 섞지 않았다. 그래서 이 구조는 Ashe W에도, Jax Q에도, 이후 Annie R/Tibbers에도 같은 방식으로 붙는다.

### 의존성은 맞는가?

맞다.

- `Shared/GameSim`: param 모양과 deterministic lookup만 소유한다.
- `ServerPrivate`: 실제 gameplay balance 값을 소유한다.
- `Server`: generated pack을 tick context에 연결한다.
- `Client`: 이 값의 주인이 아니다.
- `Engine`: 이 변경을 모른다.

프레임 중 JSON 파싱은 없다. 프레임 중에는 `GameplayDefinitionPack -> SkillGameplayDef -> SkillEffectSpec.params`만 읽는다.

### 회귀 방지는 되었는가?

서버 권위 경로는 pack 값을 읽는다. 로컬 smoke 예외 경로는 기존 값 fallback으로 유지했다.

이 선택은 의도적이다. 지금 즉시 fallback을 삭제하면 Client local/offline smoke에서 `TickContext.pDefinitions == nullptr`인 경로가 영향을 받을 수 있다.

따라서 삭제 순서는 다음과 같이 남긴다.

1. 서버 권위 reader를 pack으로 전환한다.
2. local smoke 전용 test pack 또는 server-authoritative smoke 경로를 정리한다.
3. fallback literal reader count가 0이 되면 삭제한다.

## 검증 결과

통과:

```text
python -m py_compile Tools/LoLData/Build-LoLDefinitionPack.py
python Tools/LoLData/Build-LoLDefinitionPack.py --root .
python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check
git diff --check
powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1
```

`Verify-LoLDataDrivenPipeline.ps1` 결과:

- Definition pack freshness: PASS
- Legacy ownership audit: PASS
- Client visual timing parity mismatch: 0
- GameSim build: PASS
- Server build: PASS
- Client build: PASS
- SimLab build: PASS
- SimLab deterministic regression: PASS
- Same-seed hash: `67F2A97563B8DB04`
- Seed+1 hash: `5DA19645E291A29B`

관측된 기존 경고:

- C4828: 일부 기존 파일의 source character set 경고
- C4251/C4275: 기존 Engine/Client DLL interface 경고
- LF -> CRLF git warning

이번 변경과 직접 연결된 컴파일 에러는 없다.

## Legacy Audit Snapshot

검증 시점 audit:

```text
skillDefRelated: 238
championDefRelated: 203
visualFieldsInGameplayOrLegacy: 1489
serverObjectHardcode: 29
projectileVisualCatalog: 27
```

주의:

이번 slice는 “pack reader path 생성 + Ashe/Jax 일부 스킬 효과값 이관”이다. 아직 fallback literal이 남아 있으므로 audit count가 크게 줄어드는 단계는 아니다.

다음 slice부터는 reader count를 더 좁히고 fallback 삭제 조건을 명시해야 한다.

## 다음 방향

1. Annie Q/W/E/R/Tibbers 효과값을 `SkillEffectGameplayDefs.json`으로 이관한다.
2. Ashe/Jax의 component default gameplay 값 중 실제 balance 값인 항목을 별도 atom으로 분리한다.
3. local smoke 경로에 `pDefinitions`를 공급하거나, local-only smoke fallback을 명시적으로 격리한다.
4. `Collect-LoLLegacyDataAudit.ps1`에 `skillEffectHardcode` 항목을 추가한다.
5. fallback reader count가 0인 값부터 legacy 상수를 삭제한다.
