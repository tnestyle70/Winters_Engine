Session - P3 진행분(7챔피언 skill-effect 전환) 위에 정책 원자 계층/ fallback 삭제 / P4~P8을 얹어 데이터 주도 컷오버를 끝낸다.

배경(이 한 줄에만 압축): Codex가 07/08 문서로 P3를 Annie/Ashe/Jax/Kalista/Riven/Fiora/LeeSin까지 전환했고(skillEffectDataQueryReaders=76, hardcodeCandidates=156, pack hash 0x58295D30), 그 과정에서 (1)summon/buff/passive는 SkillEffectParam에 안 맞고 (2)kSkillEffectParamMax=16 한계가 보이고 (3)fallback 상수가 남아 audit이 안 떨어지는 구조가 드러났다. 이 계획서는 그 위에서 남은 구조 변경만 다룬다. 이미 된 P0~P2와 전환 완료 챔피언은 다루지 않는다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/SkillAtomData.h

`eSkillEffectParamId`에서 summon 계열을 분리하기 위해, summon 원자를 별도 정책 enum으로 옮긴다. SkillEffectParam은 "즉시 효과 수치(피해/CC/사거리/반경)"만 갖게 한다.

기존 코드:

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
    SlowDurationSec,
    AirborneDurationSec,
    MarkDurationSec,
    StackWindowSec,
    Gap,
    DashDistance,
    DashDurationSec,
    HalfAngleCos,
    Radius,
    ShieldDurationSec,
    ShieldBaseAmount,
    ShieldAmountPerRank,
    ShieldArmorPerRank,
    SummonDurationSec,
    SummonMoveSpeed,
    SummonAttackRange,
    SummonSightRange,
    SummonAttackCooldownSec,
    SummonBaseAttackDamage,
    SummonAttackDamagePerRank,
    SummonBaseHp,
    SummonHpPerRank,
    SummonRadius,
};
```

아래로 교체:

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
    SlowDurationSec,
    AirborneDurationSec,
    MarkDurationSec,
    StackWindowSec,
    Gap,
    DashDistance,
    DashDurationSec,
    HalfAngleCos,
    Radius,
    ShieldDurationSec,
    ShieldBaseAmount,
    ShieldAmountPerRank,
    ShieldArmorPerRank,
};

// 소환체 수치는 즉시 스킬 효과가 아니라 "소환 정책"이다.
// SkillEffectParam(16-cap)에서 분리해서 별도 원자로 둔다.
enum class eSummonPolicyParamId : u8_t
{
    None = 0,
    DurationSec,
    MoveSpeed,
    AttackRange,
    SightRange,
    AttackCooldownSec,
    BaseAttackDamage,
    AttackDamagePerRank,
    BaseHp,
    HpPerRank,
    Radius,
    RoleType,
    Lane,
};
```

`SkillEffectParam`/`SkillEffectSpec`/`FindSkillEffectParam`/`ResolveSkillEffectParam` 패턴 바로 아래에, 같은 모양의 summon policy 구조를 추가한다.

기존 코드:

```cpp
struct SkillEffectSpec
{
    u16_t scalingTableId = 0;
    u32_t gameplayPolicyId = 0;
    u32_t replicatedCueId = 0;
    u8_t paramCount = 0;
    SkillEffectParam params[kSkillEffectParamMax] = {};
};
```

아래에 추가:

```cpp
inline constexpr u8_t kSummonPolicyParamMax = 16;

struct SummonPolicyParam
{
    eSummonPolicyParamId id = eSummonPolicyParamId::None;
    f32_t value = 0.f;
};

struct SummonPolicySpec
{
    bool_t bValid = false;
    u8_t paramCount = 0;
    SummonPolicyParam params[kSummonPolicyParamMax] = {};
};

inline const SummonPolicyParam* FindSummonPolicyParam(
    const SummonPolicySpec& policy,
    eSummonPolicyParamId id)
{
    for (u8_t index = 0; index < policy.paramCount && index < kSummonPolicyParamMax; ++index)
    {
        if (policy.params[index].id == id)
            return &policy.params[index];
    }
    return nullptr;
}

inline f32_t ResolveSummonPolicyParam(
    const SummonPolicySpec& policy,
    eSummonPolicyParamId id,
    f32_t fallbackValue = 0.f)
{
    if (const SummonPolicyParam* param = FindSummonPolicyParam(policy, id))
        return param->value;
    return fallbackValue;
}
```

확인 필요: `kSkillEffectParamMax`(현재 16)는 summon 분리 후 즉시 효과만 남으면 충분하다. 그래도 Annie R처럼 즉시효과(baseDamage/damagePerRank/range/radius/stunDurationSec)만 5개라 여유가 생긴다. 분리 전까지 16을 넘기는 신규 스킬을 추가하지 않는다.

1-2. C:/Users/tnest/Desktop/Winters/Tools/LoLData/Build-LoLDefinitionPack.py

skill effect 키 매핑(예: `"markDurationSec": "MarkDurationSec"`) 옆에, summon 정책 키 매핑 테이블을 추가하고, `SkillEffectGameplayDefs.json`의 `summonPolicy` 블록을 정규화/검증해서 generated pack에 emit한다.

확인 필요: 현재 generator의 skill effect 정규화 함수명/emit 함수명은 작업 시점에 `rg "SkillEffect" Tools/LoLData/Build-LoLDefinitionPack.py`로 확정한다. summon 정규화는 그 함수를 그대로 본떠 추가한다. build hash에 summon 값이 포함되어야 한다(freshness gate가 잡도록).

1-3. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/GameplayDefinitionQuery.h

`ResolveSkillEffectParam(...)` 선언 바로 아래에 summon 정책 조회를 추가한다.

기존 코드:

```cpp
    f32_t ResolveSkillEffectParam(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot,
        eSkillEffectParamId param,
        f32_t fallbackValue = 0.f);
```

아래에 추가:

```cpp
    f32_t ResolveSummonPolicyParam(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        eChampion fallbackChampion,
        u8_t slot,
        eSummonPolicyParamId param,
        f32_t fallbackValue = 0.f);
```

`GameplayDefinitionQuery.cpp`에는 기존 `ResolveSkillEffectParam` 구현을 그대로 본떠, `SkillGameplayDef`의 summon policy spec을 찾아 `ResolveSummonPolicyParam`(SkillAtomData.h inline)을 호출하는 구현을 추가한다.

확인 필요: `SkillGameplayDef`(Shared/GameSim/Definitions/SkillGameplayDef.h)에 `SummonPolicySpec summon{}` 필드를 추가하고, generated emit이 이를 채우는지 확인. 기존 `effect` 필드 옆에 둔다.

1-4. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Champions/Annie/AnnieGameSim.cpp

`TibbersSpawnTuning`이 현재 `ResolveSkillEffectParam`의 summon* 원자에서 읽는 부분을, 1-1/1-3에서 추가한 `ResolveSummonPolicyParam`로 바꾼다. 즉시효과(R baseDamage/damagePerRank/range/radius/stunDurationSec)는 그대로 `ResolveSkillEffectParam` 유지.

확인 필요: 현재 Annie R summon 읽기 라인은 `rg "Summon" Shared/GameSim/Champions/Annie/AnnieGameSim.cpp`로 확정 후, summon* eSkillEffectParamId 호출만 eSummonPolicyParamId 호출로 1:1 치환한다. 값은 byte-identical 유지(동작 불변, SimLab same-seed 해시 불변이 증거).

또한 이 파일의 정책성 상수를 별도 원자로 옮길 준비를 한다(이번 slice가 아니라 P3 정책 분리 slice 대상).

기존 코드:

```cpp
    constexpr u8_t kTibbersRoleType = 4;
    // 0xff = any-lane: 서버 미니언 AI의 lane 타겟 필터를 우회한다 (소환수 전용).
    constexpr u8_t kTibbersLane = 0xff;
```

삭제하지 않는다(아직). 위 두 값은 `eSummonPolicyParamId::RoleType` / `::Lane`로 데이터화할 후보다. 데이터 reader 전환 후 fallback으로만 남기고, P3 fallback 삭제 서브페이즈에서 제거한다.

1-5. C:/Users/tnest/Desktop/Winters/Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json

`skill.annie.r`의 summon* 키들을 `summonPolicy` 하위 블록으로 옮긴다. 값은 그대로.

기존 코드:

```json
{
  "key": "skill.annie.r",
  "params": {
    "baseDamage": 150.0,
    "damagePerRank": 75.0,
    "range": 6.0,
    "radius": 3.0,
    "stunDurationSec": 1.25,
    "summonDurationSec": 45.0,
    "summonMoveSpeed": 5.2,
    "summonAttackRange": 2.2,
    "summonSightRange": 14.0,
    "summonAttackCooldownSec": 1.0,
    "summonBaseAttackDamage": 40.0,
    "summonAttackDamagePerRank": 15.0,
    "summonBaseHp": 1000.0,
    "summonHpPerRank": 250.0,
    "summonRadius": 0.9
  }
}
```

아래로 교체:

```json
{
  "key": "skill.annie.r",
  "params": {
    "baseDamage": 150.0,
    "damagePerRank": 75.0,
    "range": 6.0,
    "radius": 3.0,
    "stunDurationSec": 1.25
  },
  "summonPolicy": {
    "durationSec": 45.0,
    "moveSpeed": 5.2,
    "attackRange": 2.2,
    "sightRange": 14.0,
    "attackCooldownSec": 1.0,
    "baseAttackDamage": 40.0,
    "attackDamagePerRank": 15.0,
    "baseHp": 1000.0,
    "hpPerRank": 250.0,
    "radius": 0.9,
    "roleType": 4.0,
    "lane": 255.0
  }
}
```

확인 필요: JSON 실제 들여쓰기/필드 순서는 generator 정규화 출력과 맞춘다. `summonPolicy`가 generator에서 검증되지 않으면 freshness가 실패하므로 1-2를 먼저 반영한다.

2. 검증

미검증(설계 단계):
- summon 분리가 Annie R 동작을 바꾸지 않는지(SimLab same-seed 해시 불변) 미검증
- generator summon 정규화/emit 미반영
- 남은 P3 챔피언(Viego/Yone/Zed/Yasuo/Irelia/Kindred/Sylas/Garen/Ezreal/MasterYi) 미전환

검증 명령(각 slice 공통):
- python Tools/LoLData/Build-LoLDefinitionPack.py --root .
- python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check
- powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1
- (기대) SimLab same-seed 해시 = 67F2A97563B8DB04 불변, seed+1 해시 상이
- (기대) skillEffectHardcodeCandidates 가 fallback 삭제 서브페이즈에서 0으로 감소

확인 필요:
- 새 .h/.json 이 GameSim/Server/Client 프로젝트 빌드에 포함되는지 확인.
- Engine public header는 건드리지 않으므로 UpdateLib.bat 불필요(이번 slice 한정).

---

## 남은 구조 변경 로드맵 (이 계획서가 여는 트랙의 전체 그림)

위 1절은 "다음 1 slice"(summon 정책 원자 분리)다. 남은 전체는 아래 순서로 같은 원자 프로토콜(추출→pack 패리티→reader 전환→게이트→fallback 삭제)을 반복한다. 각 항목은 별도 slice로 쪼개 08 프롬프트로 실행한다.

P3 마무리 (skill effect 컷오버 완료):
- P3a 정책 원자 계층: SummonPolicy(위), 그리고 PassivePolicy(예: Annie kStunThreshold), BuffPolicy(예: kEShieldBuffDefId)를 같은 모양으로 추가.
- P3b 잔여 챔피언 전환: Viego(Q/W/E/R+soul), Yone(Q/W/E/R), Zed(Q/W/E/R+mark/shadow), Yasuo(Q/W/E/R), Irelia(Q/W/E/R), Kindred, Sylas(E 외), Garen, Ezreal, MasterYi. 챔피언이 아니라 "스킬 1슬롯" 단위로 쪼갠다.
- P3c 부분 전환 챔피언 잔여 슬롯: Ashe BA/Q/E, Jax W/R, Fiora E, Kalista 비-E, Riven E/R.
- P3d fallback 삭제 서브페이즈: 모든 실행 경로(Server/GameSim/SimLab/Client local smoke)에 TickContext.pDefinitions 보장 감사 → constexpr fallback 삭제 → skillEffectHardcodeCandidates 156→0. 이게 P3의 실제 종료.

P4 timing contract 분리 (07 문서 P4 구체화):
- visualYawOffset reader(CommandExecutor/MoveSystem/Zed/Irelia)와 Scene_InGame* 클라 prediction read를 ClientPublic visual query로 분리.
- 권위 action timing(action-lock ticks/stage window/windup)은 ServerPrivate gameplay pack 단일화.
- 종료: ChampionGameDataDB 직접 gameplay reader 0 (visual은 ClientPublic로 이동).

P5 봇 AI 정책 데이터화:
- ChampionAIPolicy.cpp 16 프로필+콤보+bot skill-rank → Data/LoL/ServerPrivate/AI/ChampionAIProfiles.json + generated pack.

P6 미니언/웨이브/맵:
- Client local ResolveMinionCombatDef() fallback 제거/격리.
- ServerMinionTuning balance 상수 → ServerPrivate wave 데이터(timing과 combat stat 원자 분리).
- GameRoomSpawn/MapSpawnPoints placement fallback → stage/map 데이터.

P7 네트워크 식별자:
- Snapshot/Command의 ubyte championId → 안정 DefinitionKey 경로 추가(송수신 변환, codegen).

P8 legacy 삭제:
- reader 0 증명 후 ChampionGameDataDB / ChampionStatsRegistry / ChampionRuntimeDefaults 기본값 / SkillTable / ChampionTable / 남은 constexpr fallback 삭제.

P9 (선택) Editor Inspector + Hot-Reload.

각 Phase 종료 조건·게이트·불변 규칙은 `07_DATA_DRIVEN_FULL_CUTOVER_CODEX_REQUIREMENTS.md`를 그대로 따른다. 실행은 `08_DATA_DRIVEN_FULL_CUTOVER_CODEX_PROMPT.md`의 [이번 실행 Phase]/[이번 slice 범위]만 바꿔 반복한다.
