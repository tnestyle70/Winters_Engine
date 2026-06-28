# Winters Engine 해부기 9 - DataDriven Definition Pack

DataDriven의 본질은 "JSON을 읽는다"가 아니다.

Winters의 원칙은 오히려 반대에 가깝다.

> JSON은 authoring/cook input이고, runtime frame code는 검증된 immutable definition pack을 읽는다.

## 문제 정의

초기에는 챔피언 스킬 수치나 쿨다운, 사거리, animation timing을 C++에 직접 박는 것이 빠르다.

하지만 챔피언이 늘어나고, AI policy와 visual timing, summon policy가 들어오면 값의 소유권이 흐려진다.

문제는 다음 형태로 나타난다.

- gameplay 수치가 여러 C++ 파일에 흩어진다.
- visual timing이 Server authority에 새어 들어간다.
- designer가 관리해야 할 값과 programmer가 관리해야 할 값이 섞인다.
- legacy table을 언제 지워도 되는지 알 수 없다.
- ClientPublic visual 값과 ServerPrivate gameplay 값의 경계가 흐려진다.

이 문제를 단순히 JSON으로 바꾸는 것만으로는 해결할 수 없다.

runtime이 매 프레임 JSON string을 뒤지면 성능과 안정성이 나빠진다. 그리고 JSON 파일이 있다고 해서 데이터 소유권이 자동으로 정리되는 것도 아니다.

## Winters의 접근

Winters는 authoring data와 runtime data를 분리한다.

구조:

```text
Data/LoL/ServerPrivate
-> gameplay truth
-> skill effect
-> cooldown/range
-> AI policy

Data/LoL/ClientPublic
-> visual timing
-> animation playback
-> cast/recovery frame
-> model yaw

Build-LoLDefinitionPack.py
-> generated immutable pack
-> runtime lookup
```

## 코드 근거

관련 파일:

- `Shared/GameSim/Definitions/GameplayDefinitionPack.h`
- `Shared/GameSim/Definitions/GameplayDefinitionQuery.h`
- `Tools/LoLData/Build-LoLDefinitionPack.py`
- `Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1`
- `Data/LoL/ServerPrivate`
- `Data/LoL/ClientPublic`

핵심 구조:

```cpp
struct GameplayDefinitionPack
{
    DataPackManifest manifest{};
    const ChampionGameplayDef* champions = nullptr;
    std::size_t championCount = 0u;
    const SkillGameplayDef* skills = nullptr;
    std::size_t skillCount = 0u;
    const SummonerSpellGameplayDef* summonerSpellDefs = nullptr;
    std::size_t summonerSpellCount = 0u;
};
```

`GameplayDefinitionQuery`는 runtime code가 definition pack에서 값을 조회하는 경계다.

```cpp
namespace GameplayDefinitionQuery
{
    f32_t ResolveSkillRange(...);
    f32_t ResolveSkillCooldown(...);
    f32_t ResolveSkillEffectParam(...);
    u64_t ResolveSkillActionLockTicks(...);
}
```

## 검증 루프

DataDriven 전환은 감으로 하면 위험하다.

Winters는 다음 검증 파이프라인으로 전환 상태를 추적한다.

```text
Definition pack freshness
Legacy ownership audit
Data-driven goal status
Raw product asset path audit
Client visual timing parity
Build GameSim / Server / Client / SimLab
SimLab deterministic regression
git diff --check
```

대표 스크립트:

```powershell
powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1
```

## 왜 중요한가

DataDriven은 협업 구조와도 연결된다.

Designer가 관리할 visual/presentation 값과 Server가 소유할 gameplay truth 값을 분리해야 한다. 또한 runtime code가 어떤 데이터 소스를 읽는지 명확해야 한다.

이 경계가 없으면 코드 리팩터링을 해도 값의 주인이 계속 흔들린다.

## 면접에서 말할 포인트

이 도메인을 설명할 때는 "JSON으로 뺐습니다"보다 이렇게 말하는 것이 좋다.

> JSON authoring과 runtime immutable definition pack을 분리하고, ServerPrivate gameplay와 ClientPublic visual ownership을 audit로 검증했다.

## 이 글을 이력서 문장으로 압축하면

> 챔피언/스킬 하드코딩 값을 generated `GameplayDefinitionPack`으로 이전하고, legacy reader count와 visual/gameplay ownership을 audit하는 DataDriven 전환 파이프라인을 구축했습니다.

## 다음 글

다음 글에서는 LeeSin/Sylas 같은 챔피언 스킬과 AI combo를 server-authoritative command sequencing 문제로 설명한다.

