# 2026-07-19 정글 오브젝트 종료 압력·버프·F4 Balance Lab 계획서

```text
Session - 장로 드래곤/바론을 장기전 종료 장치로 만들고, 블루·레드 및 일반 정글 보상을 서버 권위로 연결하며 F4에서 반복 밸런스 검증 가능하게 함
좌표: 신규 좌표 후보 · 축 C7 권위·정합성, C8 검증 병목
관련: 2026-07-19_TURRET_MINION_NAV_REFRESH_KALISTA_STRUCTURE_CHASE_PLAN (보존, 이번 세션에서 구현하지 않음)
```

## 1. 결정 기록

```text
문제·제약: 현재 Baron/Dragon은 서버에 중립 몬스터·킬 카운트·일반 보상만 있고, 오브젝트 버프·팀 보상·처형·도트·강화 귀환·미니언 강화가 없다. F4는 전 정글몹 HP/AD 일괄 덮어쓰기만 지원한다.
가장 단순한 실패: 클라이언트에서 이펙트와 스탯만 올리면 서버 피해/보상/AI와 불일치하고, 일반 BuffComponent에 모든 기능을 억지로 넣으면 오라·도트·재생·미니언 원복 수명이 섞인다.
메커니즘: Economy/SpawnObject/Summoner JSON을 진실로 두고 ObjectiveBuffComponent 및 ObjectiveBurnComponent를 서버 GameSim에서 구동한다. DamageQueue는 장로 화상/처형과 레드 화상만 결합하고, ExperienceSystem은 오브젝트 팀 보상을 지급한다. 클라이언트는 지속 상태의 스냅샷 플래그와 장로 처형 단발 EffectTrigger만 소비한다.
대조: 실제 LoL의 일반 용 누적/영혼/장로 등장 조건은 이번 범위가 아니다. 현재 Dragon 모델 자체가 장로 모델이므로 subKind=Dragon 처치를 항상 Elder 처치로 해석한다.
대가: 오브젝트 전용 상태와 F4 튜닝 필드가 추가된다. 대신 서버 권위·리플레이 결정성·JSON hot reload를 유지하고, 추후 일반 용 체계가 필요하면 Dragon subKind 분리 지점이 명확하다.
```

### 1-1. 사용자 확정 계약

| 항목 | 확정값/행동 |
|---|---|
| Dragon 정체 | 현재 맵의 Dragon은 항상 Elder Dragon |
| Baron 팀 보상 | 같은 팀 5명 모두 정확히 +3레벨, 1인당 2,000골드(팀 총 10,000) |
| Elder 팀 보상 | Baron과 동일 |
| 특수 버프 수령 | 처치 시 살아 있는 같은 팀 챔피언만 |
| 버프 지속 | Baron/Elder/Blue/Red 모두 300초 |
| 재획득 | 중첩 없이 해당 버프 만료 시각만 갱신 |
| 사망 | 특수 버프 즉시 제거 |
| Flash | JSON 기본 쿨다운 20초 |
| Elder 공격력 | 최종 총 AD ×1.7 |
| Elder 화상 | 3초, 1초마다 대상 최대 HP 1% 마법 피해, 중첩 없이 갱신 |
| Elder 처형 | 유효 피해 후 챔피언 HP가 20% 이하이면 즉시 처형 |
| Baron 강화 귀환 | 기본 귀환 시간의 0.5배 |
| Baron 미니언 오라 | 12m, HP ×3, 시각 크기 ×2, 공격력 ×2, 범위 이탈 시 원복 |
| Baron/Elder 기본 방어 | HP 10,000, Armor/MR 30으로 시작 |
| 일반 정글 보상 | Blue/Red 포함 일반 캠프: 막타 챔피언 80골드, 240XP |
| Blue | 초당 마나 +10 |
| Red | 초당 체력 +10, 기본공격 시 3초 동안 초당 10 마법 피해 화상(초기 튜닝값) |

Red 화상 피해량은 원문에 수치가 없으므로 `10 damage / 1 sec / 3 sec`를 초기 JSON 값으로 둔다. 이 값은 F4에서 즉시 변경 가능하며 고정 밸런스 판정이 아니다.

### 1-2. 범위와 비범위

| 범위 | 포함 | 제외 |
|---|---|---|
| 서버 gameplay | 팀 보상, 버프 수명, 재생, 장로 AD/화상/처형, 레드 화상, 바론 귀환/미니언 오라 | 일반 용 종류·용 영혼·에픽 스폰 타이머 |
| 데이터 | Spawn/Economy/Summoner JSON, schema, runtime overlay, generated pack | 클라이언트 하드코딩 수치 |
| F4 | active JSON 편집기에 Economy/Objectives 추가, subKind별 HP/AD/Armor/MR, 공통 일반캠프 Gold/XP, refill/reset/clear buffs | 별도 objective practice override 계층 |
| 시각 | Baron/Elder/Blue/Red 표식, 강화 미니언 보라 표식, Elder execute breath/burst | 원본 Riot 파티클 graph 완전 이식 |
| 애니메이션 | Dragon 공격 clip/runtime bind 검증, 누락 fallback/진단 | 새 Dragon 피격 애니메이션 제작 |
| 기존 버그 | 이 기능으로 건드리는 미니언 배율/렌더 경로 회귀 방지 | 탑/바텀 waypoint, 타워 미니언 끼임, Kalista 제자리걸음 구현 |

## 2. 현재 코드 증거

- `SpawnObjectGameplayDefs.json`: Baron 8,000 HP, Dragon 5,000 HP, 양쪽 Armor/MR 20. Blue/Red 2,300 HP, 기타 450 HP.
- `EconomyGameplayDefs.json`: small 35/75, epic 150/250, baron 300/600으로 세 범주만 존재한다.
- `DamageQueueSystem.cpp`: subKind 0/1 킬 카운트와 kill feed는 있으나 버프 부여가 없다.
- `ExperienceSystem.cpp`: 정글 보상은 막타 골드 + 근처 생존 챔피언 동일 XP이며 Baron/Elder 팀 전체 보상은 없다.
- `CommandExecutor.cpp`: 귀환은 `EconomyGameplayDef.recallDurationSec`, Flash는 `SummonerSpellGameplayDefs.json`의 range/cooldown을 읽는다.
- `BuffComponent`: flat AD/AP/방어/공속/이속만 지원한다. 총 AD 배율, HP 재생, 오라, DoT를 표현하지 못한다.
- `Jungle_Manager.cpp`: Dragon 기본 공격을 `sru_dragon_flying_attack1`에 연결한다. 런타임 리소스에 해당 `.wanim`과 skeleton/mesh가 존재한다.
- Dragon 런타임 애니메이션은 flying attack/run/spell/landing/takeoff가 있으나 전용 hit reaction과 death clip은 없다. 따라서 “피격 시 반격 공격 애니메이션”은 동작 경로를 검증하고, “맞는 순간 몸이 움찔하는 피격 리액션”은 이번 기존 자산으로 보장할 수 없다.
- `DeathSystem`: 죽은 Jungle 엔티티는 파괴되지 않으므로 같은 엔티티를 캠프 anchor에서 복구할 수 있다.
- `ChampionTuner.cpp`: 실제 active F4는 Champions/Skills/Minions/Towers JSON 편집기와 3파일 원자적 `Save & Hot Load`이다. `Jungle Balance (Live)`는 `#if 0` legacy 블록이므로 구현 anchor로 사용하면 안 된다.

## 3. 권위·수명 설계

```text
Jungle lethal damage
  -> DamageQueueSystem kill credit (1회)
  -> ExperienceSystem::GrantKillRewards
       regular camp -> killer 80g + 240XP
       Baron/Elder  -> team roster +3 levels + 2000g each
                    -> living team members ObjectiveBuffComponent refresh(300s)
                    -> snapshot objectiveStateFlags refresh
  -> DeathSystem marks objective dead

Each authoritative tick
  -> CBuffSystem::TickObjectiveEffects
       expire/death cleanup
       Blue mana regen / Red health regen
       Elder/Baron stat dirty propagation
       Baron champion-to-allied-minion aura apply/unapply
       Objective burn tick requests
  -> CStatSystem (Elder final AD x1.7)
  -> commands / combat
  -> DamageQueueSystem
       Red basic attack burn refresh
       Elder champion damage burn refresh
       Elder <=20% execute in same resolved hit
  -> CBuffSystem::CleanupDeadObjectiveState
       same-tick death buff/burn/minion cleanup only (regen/DoT 재실행 금지)
  -> TickPracticeControls
  -> CStatSystem final recompute
  -> DeathSystem
```

### 3-1. 레벨 보상

`GrantChampionLevels(world, entity, 3)`은 현재 레벨부터 필요한 XP를 합산해 기존 `GrantExperience`를 호출한다. 목표 레벨은 18로 clamp하고 새 레벨의 XP 진행도는 0에서 시작한다. 이 경로를 사용해 Champion/Experience/Stat/SkillRank를 기존 한 소스로 동기화한다.

### 3-2. 화상과 처형 재귀 방지

- `DamageFlag_ElderBurn`, `DamageFlag_RedBurn`, `DamageFlag_ElderExecute`를 append한다.
- 위 플래그 요청은 새 burn/execute를 만들지 않는다.
- 화상은 종류별 한 슬롯을 가지며 같은 종류 재적용 시 source와 expireTick을 교체하고 nextTick은 더 늦추지 않는다.
- 처형은 원 피해가 실제 HP 피해를 1 이상 주고, source/target이 적 챔피언이며, 대상이 invulnerable/stasis가 아닌 상태에서만 평가한다.
- threshold 충족 시 새 `DamageRequest`를 만들지 않는다. `DamagePipeline::PromoteDamageResultToExecution`이 같은 대상의 `HealthComponent`를 `0/dead`로 만들고 Champion/Minion/Structure/Jungle HP mirror를 즉시 맞춘 뒤, 남은 HP를 `DamageResult.finalAmount`에 더하고 `bKilled=true`로 승격한다. 따라서 Damage event·champion kill hook·score·kill feed·reward는 기존 루프에서 각각 정확히 한 번만 실행된다.

### 3-3. 바론 미니언 원복

- `BaronEmpoweredMinionComponent`는 적용 당시 base max/current HP와 attackDamage, 적용 multiplier를 보관한다.
- 진입 시 현재 HP 비율을 보존해 maxHP를 3배로 만든다.
- 이탈·버프 사망·만료 시 현재 HP 비율을 보존해 base maxHP/AD로 원복한다.
- 서버 Transform scale은 변경하지 않는다. 스냅샷 state flag를 보고 클라이언트 렌더 world matrix만 ×2하여 충돌 반경/경로 탐색을 흔들지 않는다.
- JSON hot reload 직전 강화 미니언을 HP 비율 보존 원복하고, 새 SpawnObject base HP/AD를 lane minion과 `MinionComponent` mirror에 반영한 뒤 현재 오라로 재적용한다. 선택 캠프 Jungle도 새 maxHP/AD/Armor/MR로 갱신하되 현재 HP 비율을 보존하며, `Refill HP`만 전량 회복을 소유한다.

## 4. F4 사용자 계약

### 4-1. 조작 흐름

```text
[Balance / Units]
  Jungle & Objectives (Live)
  ┌ Target ──────────────────────────────────────────────┐
  │ [Baron ▼]  Apply target: selected subKind only       │
  └───────────────────────────────────────────────────────┘
  ┌ Monster stats/reward ────────────────────────────────┐
  │ Max HP [10000]  AD [120]  Armor [30]  MR [30]       │
  │ Regular Gold [80] XP [240]   (all non-epic camps)    │
  │ [Refill HP] [Reset at Camp]                           │
  └───────────────────────────────────────────────────────┘
  ┌ Objective buffs ─────────────────────────────────────┐
  │ Team gold each [2000]  Team levels [3] Duration[300] │
  │ Baron: Recall x[0.5] Aura[12] HPx[3] ADx[2] Scalex[2]│
  │ Elder: ADx[1.7] Burn[3]/Tick[1]/MaxHP[0.01] Exec[.2]│
  │ Blue mana/s[10] Red hp/s[10] Burn[3]/Tick[1]/Dmg[10]│
  │ [Save & Hot Load] [Reload JSON]                       │
  │ [Clear All Objective Buffs]                          │
  └───────────────────────────────────────────────────────┘
  Status: last server result / selected subKind / revision
```

사용자 작업 계약:

```text
사용자 작업: Debug room host가 정글/오브젝트 수치를 고르고 수정한 뒤, 저장·서버 반영·리비전 확인과 refill/reset으로 같은 조건을 반복 측정한다.
Primary: Save & Hot Load. Secondary: Reload/Discard from Disk, Refill HP, Reset at Camp, Clear All Objective Buffs.
권위/저장: canonical JSON -> Client draft -> 4파일 validate/atomic persist -> Debug Server runtime reload -> tool revision/command ack.
Authority mode: B(Canonical Data Editor). 별도 practice 수치 override는 만들지 않는다.
```

상태 계약은 기존 `clean/dirty/invalid/saving/applying/applied/error`를 Economy까지 확장하고, source 외부 변경은 `stale`로 표시해 덮어쓰기를 막는다. dirty 상태의 재로드는 `Discard Changes & Reload`로 명시하고 확인을 거친다. Release/offline/non-host에서는 저장 가능 여부와 hot-load 가능 여부를 같은 predicate로 뭉치지 않고, 실제 차단 이유와 다음 행동을 표시한다. 공식 최소 해상도/DPI는 저장소에 정의가 없어 `CONFIRM_NEEDED`이며, 현재 F4 창의 `ImGuiCond_FirstUseEver` 크기와 스크롤 child를 유지하고 1280×720/100%는 회귀 관찰값일 뿐 제품 최소 사양으로 선언하지 않는다.

### 4-2. 액션 의미

- monster/objective 값 편집은 active F4 draft의 SpawnObject/Economy JSON을 바꾸며 `Save & Hot Load`가 기존 원자적 저장 후 서버 definition reload를 수행한다. 별도 runtime override와 JSON 진실이 경쟁하지 않는다.
- `Refill HP`: 살아 있는 선택 subKind만 최대 HP로 채운다. 위치·AI·보상·버프를 바꾸지 않는다.
- `Reset at Camp`: 선택 subKind를 anchor로 되돌리고 HP/AI/action/target/death를 초기화한다. 버튼 동작 자체는 보상을 주지 않는다.
- `Clear All Objective Buffs`: 챔피언 Baron/Elder/Blue/Red와 대상 burn, 강화 미니언을 원복한다. 객관적인 반복 테스트 시작점이다.
- `Reload JSON`: 저장하지 않은 draft를 버리고 Champion/SkillEffect/SpawnObject/Economy 네 JSON을 다시 읽는다.
- `Reload Gameplay Definitions`: 기존 기능을 유지하며 JSON 수정값을 hot reload하고 objective tuning도 다음 tick부터 읽는다.
- 성공 표시는 4파일 저장 성공만으로 끝내지 않고 Debug Server의 `ReloadGameplayDefinitions` command ack와 증가한 `toolRevision`을 모두 관측한 뒤 `applied`로 바꾼다. 실패/timeout은 부분 성공으로 표시하지 않는다.

## 5. 반영해야 하는 코드

### 5-1. `Data/LoL/ServerPrivate/Gameplay/SummonerSpellGameplayDefs.json`

`"cooldownSec": 300.0` 아래로 교체:

```json
      "cooldownSec": 20.0,
```

### 5-2. `Data/LoL/ServerPrivate/Gameplay/SpawnObjectGameplayDefs.json`

subKind 0/1의 maxHp/baseArmor/baseMr를 각각 아래 값으로 교체한다.

```json
      "maxHp": 10000.0,
      "baseArmor": 30.0,
      "baseMr": 30.0,
```

### 5-3. `Data/LoL/ServerPrivate/Gameplay/EconomyGameplayDefs.json`

`jungle` 그룹을 아래로 교체하고 `objectives` 그룹을 `passiveGold` 바로 위에 추가한다.

```json
  "jungle": {
    "smallCampGold": 80.0,
    "smallCampXP": 240.0,
    "epicGold": 0.0,
    "epicXP": 0.0,
    "baronGold": 0.0,
    "baronXP": 0.0
  },
  "objectives": {
    "teamGoldPerChampion": 2000.0,
    "teamLevelGrant": 3,
    "buffDurationSec": 300.0,
    "baronRecallDurationMultiplier": 0.5,
    "baronAuraRadius": 12.0,
    "baronMinionHpMultiplier": 3.0,
    "baronMinionAttackDamageMultiplier": 2.0,
    "baronMinionScaleMultiplier": 2.0,
    "elderAttackDamageMultiplier": 1.7,
    "elderBurnDurationSec": 3.0,
    "elderBurnTickIntervalSec": 1.0,
    "elderBurnTargetMaxHpRatioPerTick": 0.01,
    "elderExecuteThresholdRatio": 0.2,
    "blueManaRegenPerSec": 10.0,
    "redHealthRegenPerSec": 10.0,
    "redBurnDurationSec": 3.0,
    "redBurnTickIntervalSec": 1.0,
    "redBurnDamagePerTick": 10.0
  },
```

### 5-4. `Data/LoL/Schemas/EconomyGameplayDefs.json.schema.json`

required에 `objectives`를 추가한다. `teamLevelGrant`만 `integer`, minimum 0, maximum 18로 검증하고 나머지 objective 값은 유한 number와 도메인별 minimum/maximum을 둔다. runtime overlay가 unknown field를 거부하므로 schema와 parser field table을 동시에 갱신한다.

### 5-5. `Shared/GameSim/Definitions/EconomyGameplayDef.h`

`EconomyJungleRewardDef` 아래에 추가:

```cpp
struct ObjectiveGameplayDef
{
    f32_t teamGoldPerChampion = 2000.f;
    u8_t teamLevelGrant = 3u;
    f32_t buffDurationSec = 300.f;
    f32_t baronRecallDurationMultiplier = 0.5f;
    f32_t baronAuraRadius = 12.f;
    f32_t baronMinionHpMultiplier = 3.f;
    f32_t baronMinionAttackDamageMultiplier = 2.f;
    f32_t baronMinionScaleMultiplier = 2.f;
    f32_t elderAttackDamageMultiplier = 1.7f;
    f32_t elderBurnDurationSec = 3.f;
    f32_t elderBurnTickIntervalSec = 1.f;
    f32_t elderBurnTargetMaxHpRatioPerTick = 0.01f;
    f32_t elderExecuteThresholdRatio = 0.2f;
    f32_t blueManaRegenPerSec = 10.f;
    f32_t redHealthRegenPerSec = 10.f;
    f32_t redBurnDurationSec = 3.f;
    f32_t redBurnTickIntervalSec = 1.f;
    f32_t redBurnDamagePerTick = 10.f;
};
```

`EconomyGameplayDef::jungle` 아래에 추가:

```cpp
    ObjectiveGameplayDef objectives{};
```

### 5-6. `Shared/GameSim/Components/GameplayComponents.h`

`JungleComponent` 아래에 objective POD 상태를 추가한다.

```cpp
enum class eObjectiveBuffKind : u8_t
{
    Baron = 0,
    Elder = 1,
    Blue = 2,
    Red = 3,
    Count = 4,
};

struct ObjectiveBuffComponent
{
    u64_t expireTicks[static_cast<u8_t>(eObjectiveBuffKind::Count)]{};
};

struct ObjectiveBurnState
{
    EntityID source = NULL_ENTITY;
    u64_t expireTick = 0u;
    u64_t nextTick = 0u;
};

struct ObjectiveBurnComponent
{
    ObjectiveBurnState elder{};
    ObjectiveBurnState red{};
};

struct BaronEmpoweredMinionComponent
{
    f32_t baseMaxHp = 0.f;
    f32_t baseAttackDamage = 0.f;
    f32_t hpMultiplier = 1.f;
    f32_t attackDamageMultiplier = 1.f;
    f32_t scaleMultiplier = 1.f;
};

```

### 5-7. `Shared/GameSim/Definitions/DamageTypes.h`

`DamageFlag_YoneSoulEcho` 아래에 추가:

```cpp
    DamageFlag_ElderBurn = 1u << 6,
    DamageFlag_RedBurn = 1u << 7,
    DamageFlag_ElderExecute = 1u << 8,
```

### 5-8. `Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h` 및 `Shared/Schemas/Command.fbs`

PracticeOperation append-only 추가. Objective 수치는 active JSON editor/hot reload가 소유하므로 별도 apply/clear override operation은 만들지 않는다.

```cpp
    RefillJungleHealth = 32,
    ResetJungleMonster = 33,
    ClearObjectiveBuffs = 34,
    Count = 35,
```

`flags` 계약은 0=legacy all, 1..11=`subKind + 1`로 하여 기존 F4/계약 호출을 깨지 않는다.

### 5-9. `Shared/GameSim/Systems/Buff/BuffSystem.h/.cpp`

`AdvanceDurationsAfterStat` 선언 아래에 추가:

```cpp
    static ObjectiveGameplayDef ResolveObjectiveTuning(const TickContext& tc);
    static bool_t HasObjectiveBuff(
        CWorld& world, EntityID entity, eObjectiveBuffKind kind);
    static void AddOrRefreshObjectiveBuff(
        CWorld& world, const TickContext& tc, EntityID entity,
        eObjectiveBuffKind kind, EntityID visualSource);
    static void TickObjectiveEffects(CWorld& world, const TickContext& tc);
    static void CleanupDeadObjectiveState(CWorld& world, const TickContext& tc);
    static void OnDamageResolved(
        CWorld& world, const TickContext& tc,
        const DamageRequest& request, DamageResult& result);
    static void ClearObjectiveBuffs(CWorld& world, const TickContext& tc);
    static void UnapplyAllBaronMinionEmpowerments(CWorld& world);
    static void RefreshBaronMinionEmpowerments(
        CWorld& world, const TickContext& tc);
```

`ResolveObjectiveTuning`은 `tc.pDefinitions->FindEconomy()->objectives`만 읽고 유효 팩이 없으면 `ObjectiveGameplayDef{}`를 반환한다. `TickObjectiveEffects`만 만료, Blue/Red 재생, DoT enqueue, Baron lane-minion 오라를 수행한다. `CleanupDeadObjectiveState`는 같은 tick 사망자의 슬롯·burn과 사망/만료로 무효가 된 미니언 강화만 제거하고 재생/DoT를 다시 실행하지 않는다. 오라는 `roleType <= kGameSimMinionRoleSuper`만 허용해 Tibbers(role 4)를 제외한다. 모든 순회는 `DeterministicEntityIterator<...>::CollectSorted`를 사용한다. 지속 표식은 이벤트가 아니라 snapshot truth로 복원하며, `ReplicatedEventComponent`는 Elder execute one-shot만 전달한다.

### 5-10. `Shared/GameSim/Systems/Experience/ExperienceSystem.h/.cpp`

public에 추가:

```cpp
    static void GrantChampionLevels(CWorld& world, EntityID entity, u8_t levels);
```

`GrantKillRewards`의 Jungle 분기를 아래 의미로 교체한다. 실제 구현은 `DeterministicEntityIterator<ChampionComponent>::CollectSorted(world)` 순서로 순회하고 `PracticeSpawnedTag`가 붙은 F4 생성 챔피언은 10인 authoritative roster 보상에서 제외한다.

```cpp
    if (jungle.subKind == 0u || jungle.subKind == 1u)
    {
        const ObjectiveGameplayDef tuning =
            CBuffSystem::ResolveObjectiveTuning(tc);
        const auto champions =
            DeterministicEntityIterator<ChampionComponent>::CollectSorted(world);
        for (EntityID entity : champions)
        {
            const ChampionComponent& champion =
                world.GetComponent<ChampionComponent>(entity);
            if (champion.team != rewardTeam)
                continue;
            if (world.HasComponent<PracticeSpawnedTag>(entity) ||
                !world.HasComponent<GoldComponent>(entity) ||
                !world.HasComponent<ExperienceComponent>(entity))
            {
                continue;
            }
            const u32_t gold = GrantGold(
                world, entity, tuning.teamGoldPerChampion);
            (void)GameplayFeedback::EnqueueGoldRewardFeedback(
                world, tc, entity, victim, gold);
            GrantChampionLevels(world, entity, tuning.teamLevelGrant);
            if (IsLivingChampionRecipient(world, entity))
            {
                CBuffSystem::AddOrRefreshObjectiveBuff(
                    world, tc, entity,
                    jungle.subKind == 0u
                        ? eObjectiveBuffKind::Baron
                        : eObjectiveBuffKind::Elder,
                    killer);
            }
        }
        return;
    }
```

일반 캠프는 nearby 공유 대신 killer 1명에게만 reward/override gold+XP를 지급한다. subKind 2/3이면 Blue/Red buff를 living champion killer에게 부여한다.

### 5-11. `Shared/GameSim/Systems/Stat/StatSystem.cpp`

`StatSystem.h`의 private `ApplyRuntimeModifiers` 마지막 인자 아래에 definitions 포인터를 추가하고, 팩 없는 legacy `Recompute`는 `nullptr`, 팩을 받는 `Recompute`는 `&definitions`를 넘긴다.

```cpp
        const GameplayDefinitionPack* pDefinitions);
```

`StatSystem.cpp`의 `stat.ad = stat.baseAd + stat.bonusAd;` 아래에 추가:

```cpp
    if (CBuffSystem::HasObjectiveBuff(
        world, entity, eObjectiveBuffKind::Elder))
    {
        const EconomyGameplayDef* pEconomy =
            pDefinitions ? pDefinitions->FindEconomy() : nullptr;
        const f32_t multiplier = pEconomy
            ? pEconomy->objectives.elderAttackDamageMultiplier
            : ObjectiveGameplayDef{}.elderAttackDamageMultiplier;
        stat.ad *= multiplier;
    }
```

`#include "Shared/GameSim/Systems/Buff/BuffSystem.h"`를 `StatSystem.cpp`의 system include 군에 추가한다. `TickObjectiveEffects`가 첫 Stat 전에 만료 슬롯을 0으로 만들므로 Stat 경로는 `expireTick != 0`만 본다. 별도 practice tuning override나 가상 tick API는 만들지 않는다.

### 5-12. `Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`

Recall duration 계산을 아래 의미로 교체:

```cpp
    recall.fDurationSec = pEconomy
        ? pEconomy->recallDurationSec
        : kRecallDurationSec;
    if (CBuffSystem::HasObjectiveBuff(
        world, cmd.issuerEntity, eObjectiveBuffKind::Baron))
    {
        recall.fDurationSec *= CBuffSystem::ResolveObjectiveTuning(tc)
            .baronRecallDurationMultiplier;
    }
```

### 5-13. `Shared/GameSim/Systems/Damage/DamagePipeline.h/.cpp`, `DamageQueueSystem.cpp`

`DamagePipeline.h`의 `ApplyDamageRequest` 선언 아래에 추가:

```cpp
bool_t PromoteDamageResultToExecution(
    CWorld& world,
    EntityID target,
    DamageResult& result);
```

`DamagePipeline.cpp`에서 기존 anonymous `MirrorHealth`를 재사용해 구현한다. 이 함수는 `result.bApplied && !result.bKilled`, 살아 있는 Health, remaining HP > 0일 때만 Health를 0/dead로 바꾸고 mirror를 맞춘 뒤 아래처럼 같은 result를 승격한다.

```cpp
    const f32_t remainingHealth = hp.fCurrent;
    hp.fCurrent = 0.f;
    hp.bIsDead = true;
    MirrorHealth(world, target, hp.fCurrent, hp.fMaximum);
    result.finalAmount += remainingHealth;
    result.bKilled = true;
    return true;
```

`DamageResult result = ApplyDamageRequest(...)` 바로 아래에 추가:

```cpp
        CBuffSystem::OnDamageResolved(world, tc, request, result);
```

`OnDamageResolved`는 Elder 조건이 맞을 때 위 helper만 호출하고 새 DamageRequest를 만들지 않는다. 이 호출이 result의 execute kill을 확정한 뒤 기존 item/champion hook, Damage event, score, kill feed, reward가 한 번만 진행된다. SimLab은 Damage event 1회, `TryMarkChampionDeathCredit` 이후 score/reward 1회, Irelia식 champion kill hook 1회를 각각 계수한다.

### 5-14. `Server/Private/Game/GameRoomTick.cpp`

`CAreaAuraSystem::Execute` 아래, 첫 StatSystem 앞에 추가:

```cpp
    CBuffSystem::TickObjectiveEffects(m_world, tc);
```

`Phase_SimulationSystems` 끝의 기존 순서:

```cpp
	CDamageQueueSystem::Execute(m_world, tc);
	CStatSystem::Execute(m_world, definitions);
	TickPracticeControls(tc);
	CDeathSystem::Execute(m_world, tc);
```

아래로 교체:

```cpp
	CDamageQueueSystem::Execute(m_world, tc);
	CBuffSystem::CleanupDeadObjectiveState(m_world, tc);
	TickPracticeControls(tc);
	CStatSystem::Execute(m_world, definitions);
	CDeathSystem::Execute(m_world, tc);
```

이 분리는 damage로 이번 tick에 죽은 챔피언의 buff/표식을 같은 snapshot 전에 제거하고, F4 Clear가 남긴 dirty Stat도 같은 snapshot 전에 재계산한다. cleanup 함수는 regen/DoT tick을 포함하지 않는다.

### 5-15. `Server/Private/Game/GameRoomCommands.cpp`

legacy `ApplyJungleStatOverride`는 기존 계약을 보존한다. 새 operation은 다음을 수행한다.

```text
RefillJungleHealth: flags target만 living full HP
ResetJungleMonster: flags target을 anchor로 이동, dead/AI/action/target/cooldown 초기화
ClearObjectiveBuffs: CBuffSystem::ClearObjectiveBuffs
```

Reset은 `SetPoseState(...Idle...)`, `MoveTargetComponent{}`, AttackChase/CombatAction 제거, Health/Jungle mirror 동기화를 모두 수행한다.

`ReloadGameplayDefinitions`의 `m_PracticeMinionAttackDamage.Clear();` 바로 아래에서 `CBuffSystem::UnapplyAllBaronMinionEmpowerments(m_world)`를 호출한다. 기존 living minion refresh는 `health.fCurrent = maxHp`를 쓰지 않고 reload 전 HP 비율을 새 maxHP에 곱하며 `MinionStateComponent.attackDamage`, `HealthComponent`, `MinionComponent` mirror를 함께 갱신한다. 그 뒤 `CBuffSystem::RefreshBaronMinionEmpowerments(m_world, reloadedTick)`을 호출한다.

같은 hot-load 블록의 turret refresh 뒤에 sorted Jungle refresh를 추가한다. 각 entity의 `spawnPack.ResolveJungleCamp(static_cast<u8_t>(jungle.subKind))`에서 maxHp/attackDamage/baseArmor/baseMr를 읽고 HP 비율을 보존해 `HealthComponent`, `JungleComponent`, `StatComponent`를 함께 갱신한다. 죽은 캠프는 current=0/dead를 유지하되 다음 `Reset at Camp`가 새 maxHP를 사용한다. 로그에 `jungleRefresh=`를 추가한다. JSON reload 자체는 버프/보상/HP 전량 회복을 수행하지 않는다.

### 5-16. `Server/Private/Data/RuntimeGameplayDefinitionOverlay.cpp` 및 `Tools/LoLData/Build-LoLDefinitionPack.py`

- economy known root에 `objectives` 추가.
- runtime overlay에는 `ObjectiveGameplayDef`의 f32 멤버 17개용 `EconomyFieldName` table을 추가하고, `teamLevelGrant`는 `is_number_unsigned`, 0..18 검증 후 `u8_t`로 별도 대입한다. f32 member-pointer template에 u8 필드를 억지로 넣지 않는다.
- generator에는 `ECONOMY_OBJECTIVE_FLOAT_FIELDS` 17개와 `teamLevelGrant = legacy.as_int(...)` 정규화를 추가한다. `append_economy_cpp`는 정수 필드에 `u` suffix, 나머지에 `cpp_float`를 사용한다.
- generated C++은 generator 실행 결과만 반영하고 수동 편집하지 않는다.

### 5-17. `Shared/GameSim/Core/Checkpoint/WorldKeyframe.cpp`

GameplayComponents 등록부에 추가:

```cpp
        reg.Register<ObjectiveBuffComponent>("ObjectiveBuffComponent");
        reg.Register<ObjectiveBurnComponent>("ObjectiveBurnComponent");
        reg.Register<BaronEmpoweredMinionComponent>("BaronEmpoweredMinionComponent");
```

### 5-18. `Shared/GameSim/Definitions/SnapshotStateFlags.h`, `Shared/Schemas/Snapshot.fbs`, `Shared/GameSim/Components/ReplicatedStateComponent.h`, `Server/Private/Game/SnapshotBuilder.cpp`, `Client/Private/Network/Client/SnapshotApplier.cpp`

기존 `EntitySnapshot.stateFlags`의 bit 7..30은 `ChampionAIComponent` debug state/action/intent와 이미 겹치므로 절대 재사용하지 않는다. `SnapshotStateFlags.h`의 Recall flag 바로 위에 별도 objective field용 상수를 추가한다.

```cpp
inline constexpr u32_t kObjectiveStateBaronBuffFlag = 1u << 0;
inline constexpr u32_t kObjectiveStateElderBuffFlag = 1u << 1;
inline constexpr u32_t kObjectiveStateBlueBuffFlag = 1u << 2;
inline constexpr u32_t kObjectiveStateRedBuffFlag = 1u << 3;
inline constexpr u32_t kObjectiveStateBaronEmpoweredMinionFlag = 1u << 4;
```

`EntitySnapshot` 끝에 append-only 두 필드를 추가한다.

```fbs
    objectiveStateFlags:uint = 0;
    visualScaleMultiplier:float = 1.0;
```

`ReplicatedStateComponent`의 `serverTick` 바로 위에 `u32_t objectiveStateFlags = 0u;`와 `f32_t visualScaleMultiplier = 1.f;`를 추가한다. SnapshotBuilder는 active objective slots와 empowered component를 `objectiveStateFlags`에 OR하고, empowered component의 실제 scale multiplier를 기록한다. SnapshotApplier는 두 값을 복제하고 entity row마다 `EventApplier::ReconcileObjectiveVisualSnapshot`을 호출한다.

`SnapshotBuilder.cpp`의 local `u32_t stateFlags = 0;` 아래에 추가:

```cpp
        u32_t objectiveStateFlags = 0u;
        f32_t visualScaleMultiplier = 1.f;
```

Health/Recall state 처리 뒤에 추가:

```cpp
        if (const ObjectiveBuffComponent* pObjective =
            world.TryGetComponent<ObjectiveBuffComponent>(entity))
        {
            if (pObjective->expireTicks[static_cast<u8_t>(eObjectiveBuffKind::Baron)] != 0u)
                objectiveStateFlags |= kObjectiveStateBaronBuffFlag;
            if (pObjective->expireTicks[static_cast<u8_t>(eObjectiveBuffKind::Elder)] != 0u)
                objectiveStateFlags |= kObjectiveStateElderBuffFlag;
            if (pObjective->expireTicks[static_cast<u8_t>(eObjectiveBuffKind::Blue)] != 0u)
                objectiveStateFlags |= kObjectiveStateBlueBuffFlag;
            if (pObjective->expireTicks[static_cast<u8_t>(eObjectiveBuffKind::Red)] != 0u)
                objectiveStateFlags |= kObjectiveStateRedBuffFlag;
        }
        if (const BaronEmpoweredMinionComponent* pEmpowered =
            world.TryGetComponent<BaronEmpoweredMinionComponent>(entity))
        {
            objectiveStateFlags |= kObjectiveStateBaronEmpoweredMinionFlag;
            visualScaleMultiplier = pEmpowered->scaleMultiplier;
        }
```

`CreateEntitySnapshot`의 마지막 기존 인자:

```cpp
			aiDebugCandidateTermWeightsOffset,
			aiDebugCandidateTermContributionsOffset));
```

아래로 교체:

```cpp
			aiDebugCandidateTermWeightsOffset,
			aiDebugCandidateTermContributionsOffset,
			objectiveStateFlags,
			visualScaleMultiplier));
```

### 5-19. `Client/Private/Manager/Minion_Manager.cpp`

render와 RHI snapshot 양쪽에서 같은 helper를 사용한다.

```cpp
    Mat4 ResolveMinionVisualWorldMatrix(
        CWorld& world, EntityID entity, TransformComponent& transform)
    {
        const Mat4& worldMatrix = transform.GetWorldMatrix();
        if (!world.HasComponent<ReplicatedStateComponent>(entity) ||
            (world.GetComponent<ReplicatedStateComponent>(entity)
                .objectiveStateFlags &
                kObjectiveStateBaronEmpoweredMinionFlag) == 0u)
        {
            return worldMatrix;
        }
        const f32_t scale = (std::max)(1.f,
            world.GetComponent<ReplicatedStateComponent>(entity)
                .visualScaleMultiplier);
        return Mat4::Scale(scale, scale, scale) * worldMatrix;
    }
```

서버 collision/nav scale은 바꾸지 않고 presentation float만 복제하므로 F4 scale multiplier가 즉시 보이면서 pathfinding은 흔들리지 않는다.

### 5-20. `Client/Private/UI/ChampionTuner.cpp`

`Client/Public/UI/ChampionTuner.h`의 enum 기존 코드:

```cpp
		Minions,
		Towers,
```

아래로 교체:

```cpp
		Minions,
		Towers,
		Objectives,
```

`kSpawnObjectBalanceDataPath` 바로 아래에 추가:

```cpp
	constexpr const wchar_t* kEconomyBalanceDataPath =
		L"Data\\LoL\\ServerPrivate\\Gameplay\\EconomyGameplayDefs.json";
```

`BalanceDataDraft`에서 각 기존 SpawnObject 필드 바로 아래에 아래 필드를 추가한다.

```cpp
		std::filesystem::path economyPath{};
		std::string economySource{};
		ordered_json economy{};
		int jungleSubKind = 0;
		bool_t bEconomyDirty = false;
```

`FindMinionEntry` 함수 바로 아래에 추가:

```cpp
	ordered_json* FindJungleEntry(BalanceDataDraft& draft, int subKind)
	{
		if (!draft.spawnObjects.contains("jungleCamps") ||
			!draft.spawnObjects["jungleCamps"].is_array())
		{
			return nullptr;
		}
		for (ordered_json& camp : draft.spawnObjects["jungleCamps"])
		{
			if (camp.value("subKind", -1) == subKind)
				return &camp;
		}
		return nullptr;
	}
```

`ValidateBalanceDraft` 바로 위에 다음 complete helper를 추가하고, 함수 마지막 `return true;` 바로 위에서 호출한다.

```cpp
	bool_t ValidateObjectiveBalanceDraft(
		BalanceDataDraft& draft,
		std::string& outError)
	{
		if (!draft.economy.contains("jungle") ||
			!draft.economy["jungle"].is_object() ||
			!draft.economy.contains("objectives") ||
			!draft.economy["objectives"].is_object())
		{
			outError = "Economy JSON is missing jungle or objectives.";
			return false;
		}

		for (int subKind = 0; subKind <= 10; ++subKind)
		{
			ordered_json* pCamp = FindJungleEntry(draft, subKind);
			if (!pCamp ||
				!ValidateNumber(*pCamp, "maxHp", 1.f, 1000000.f, outError, "jungleCamp") ||
				!ValidateNumber(*pCamp, "attackDamage", 0.f, 1000000.f, outError, "jungleCamp") ||
				!ValidateNumber(*pCamp, "baseArmor", 0.f, 1000000.f, outError, "jungleCamp") ||
				!ValidateNumber(*pCamp, "baseMr", 0.f, 1000000.f, outError, "jungleCamp"))
			{
				return false;
			}
		}

		ordered_json& jungle = draft.economy["jungle"];
		for (const char* pField : {
			"smallCampGold", "smallCampXP", "epicGold",
			"epicXP", "baronGold", "baronXP" })
		{
			if (!ValidateNumber(jungle, pField, 0.f, 1000000.f,
				outError, "economy.jungle"))
			{
				return false;
			}
		}

		ordered_json& objective = draft.economy["objectives"];
		if (!objective.contains("teamLevelGrant") ||
			!objective["teamLevelGrant"].is_number_unsigned() ||
			objective["teamLevelGrant"].get<u32_t>() > 18u)
		{
			outError = "economy.objectives.teamLevelGrant must be integer 0..18";
			return false;
		}
		struct ObjectiveRange { const char* pField; f32_t minValue; f32_t maxValue; };
		static const ObjectiveRange kRanges[] = {
			{ "teamGoldPerChampion", 0.f, 1000000.f },
			{ "buffDurationSec", 0.f, 3600.f },
			{ "baronRecallDurationMultiplier", 0.01f, 10.f },
			{ "baronAuraRadius", 0.f, 100.f },
			{ "baronMinionHpMultiplier", 1.f, 20.f },
			{ "baronMinionAttackDamageMultiplier", 1.f, 20.f },
			{ "baronMinionScaleMultiplier", 1.f, 10.f },
			{ "elderAttackDamageMultiplier", 1.f, 20.f },
			{ "elderBurnDurationSec", 0.f, 60.f },
			{ "elderBurnTickIntervalSec", 0.01f, 60.f },
			{ "elderBurnTargetMaxHpRatioPerTick", 0.f, 1.f },
			{ "elderExecuteThresholdRatio", 0.f, 1.f },
			{ "blueManaRegenPerSec", 0.f, 10000.f },
			{ "redHealthRegenPerSec", 0.f, 10000.f },
			{ "redBurnDurationSec", 0.f, 60.f },
			{ "redBurnTickIntervalSec", 0.01f, 60.f },
			{ "redBurnDamagePerTick", 0.f, 1000000.f },
		};
		for (const ObjectiveRange& range : kRanges)
		{
			if (!ValidateNumber(objective, range.pField,
				range.minValue, range.maxValue, outError, "economy.objectives"))
			{
				return false;
			}
		}
		return true;
	}
```

`ValidateBalanceDraft`의 마지막 `return true;` 아래로 교체:

```cpp
		if (!ValidateObjectiveBalanceDraft(draft, outError))
			return false;
		return true;
```

`LoadBalanceData`에서 `loaded.minionRole` 아래에 추가:

```cpp
		loaded.jungleSubKind = state.balanceData.jungleSubKind;
```

같은 함수의 SpawnObject path/read 조건 바로 뒤에 Economy path/read 조건을 각각 추가한다.

```cpp
			!ResolveBalanceDataPath(
				kEconomyBalanceDataPath, loaded.economyPath, error) ||
```

```cpp
			!ReadOrderedJson(
				loaded.economyPath, loaded.economySource,
				loaded.economy, error) ||
```

`SaveBalanceData`의 SpawnObject write 조건 아래에 추가:

```cpp
		if (draft.bEconomyDirty)
			writes.push_back({
				draft.economyPath, {}, {}, &draft.economySource,
				&draft.economy, 2 });
```

dirty reset의 `draft.bSpawnObjectDirty = false;` 아래에 추가:

```cpp
		draft.bEconomyDirty = false;
```

active `Render`의 `Towers` tab 바로 아래, `ImGui::EndTabBar()` 바로 위에 아래 complete tab을 추가한다. `#if 0` legacy renderer는 수정하지 않는다.

```cpp
			if (ImGui::BeginTabItem("Objectives"))
			{
				state.balanceCategory =
					static_cast<int>(eBalanceTunerCategory::Objectives);
				static const char* const kCamps[11] = {
					"Baron", "Elder Dragon", "Blue", "Red", "Gromp",
					"Camp 5", "Camp 6", "Camp 7", "Camp 8", "Camp 9", "Camp 10"
				};
				ImGui::SetNextItemWidth(220.f);
				ImGui::Combo("Jungle Camp", &draft.jungleSubKind, kCamps, 11);
				if (ordered_json* pCamp =
					FindJungleEntry(draft, draft.jungleSubKind))
				{
					ImGui::SeparatorText("Monster Stats");
					EditDragFloat(*pCamp, "maxHp", "Max Health", 10.f,
						1.f, 1000000.f, "%.0f", draft.bSpawnObjectDirty);
					EditDragFloat(*pCamp, "attackDamage", "Attack Damage", 1.f,
						0.f, 1000000.f, "%.0f", draft.bSpawnObjectDirty);
					EditDragFloat(*pCamp, "baseArmor", "Armor", 1.f,
						0.f, 1000000.f, "%.0f", draft.bSpawnObjectDirty);
					EditDragFloat(*pCamp, "baseMr", "Magic Resist", 1.f,
						0.f, 1000000.f, "%.0f", draft.bSpawnObjectDirty);
				}

				ordered_json& reward = draft.economy["jungle"];
				ordered_json& objective = draft.economy["objectives"];
				ImGui::SeparatorText("Regular Camp Reward");
				EditDragFloat(reward, "smallCampGold", "Gold", 1.f,
					0.f, 1000000.f, "%.0f", draft.bEconomyDirty);
				EditDragFloat(reward, "smallCampXP", "Experience", 1.f,
					0.f, 1000000.f, "%.0f", draft.bEconomyDirty);

				ImGui::SeparatorText("Objective Rewards / Duration");
				EditDragFloat(objective, "teamGoldPerChampion", "Team Gold / Champion",
					10.f, 0.f, 1000000.f, "%.0f", draft.bEconomyDirty);
				int teamLevels = objective.value("teamLevelGrant", 0);
				if (ImGui::DragInt("Team Levels", &teamLevels, 1.f, 0, 18,
					"%d", ImGuiSliderFlags_AlwaysClamp))
				{
					objective["teamLevelGrant"] = static_cast<u32_t>(teamLevels);
					draft.bEconomyDirty = true;
				}
				EditDragFloat(objective, "buffDurationSec", "Buff Duration (sec)",
					1.f, 0.f, 3600.f, "%.0f s", draft.bEconomyDirty);

				ImGui::SeparatorText("Baron");
				EditDragFloat(objective, "baronRecallDurationMultiplier", "Recall Multiplier",
					0.01f, 0.01f, 10.f, "%.2f", draft.bEconomyDirty);
				EditDragFloat(objective, "baronAuraRadius", "Aura Radius (m)",
					0.1f, 0.f, 100.f, "%.1f m", draft.bEconomyDirty);
				EditDragFloat(objective, "baronMinionHpMultiplier", "Minion Health Multiplier",
					0.1f, 1.f, 20.f, "%.1f", draft.bEconomyDirty);
				EditDragFloat(objective, "baronMinionAttackDamageMultiplier", "Minion Attack Multiplier",
					0.1f, 1.f, 20.f, "%.1f", draft.bEconomyDirty);
				EditDragFloat(objective, "baronMinionScaleMultiplier", "Minion Visual Scale",
					0.1f, 1.f, 10.f, "%.1f", draft.bEconomyDirty);

				ImGui::SeparatorText("Elder / Blue / Red");
				static const char* const kObjectiveFields[] = {
					"elderAttackDamageMultiplier", "elderBurnDurationSec",
					"elderBurnTickIntervalSec", "elderBurnTargetMaxHpRatioPerTick",
					"elderExecuteThresholdRatio", "blueManaRegenPerSec",
					"redHealthRegenPerSec", "redBurnDurationSec",
					"redBurnTickIntervalSec", "redBurnDamagePerTick"
				};
				for (const char* pField : kObjectiveFields)
					EditFloat(objective, pField, pField, draft.bEconomyDirty, "%.3f");

				const u32_t targetFlags =
					static_cast<u32_t>(draft.jungleSubKind + 1);
				ImGui::BeginDisabled(!bCanHotLoad ||
					draft.pendingHotLoadSequence != 0u);
				if (ImGui::Button("Refill HP"))
					SendPracticeCommand(pScene, state,
						ePracticeOperation::RefillJungleHealth, 0.f, targetFlags);
				ImGui::SameLine();
				if (ImGui::Button("Reset at Camp"))
					SendPracticeCommand(pScene, state,
						ePracticeOperation::ResetJungleMonster, 0.f, targetFlags);
				ImGui::SameLine();
				if (ImGui::Button("Clear All Objective Buffs"))
					SendPracticeCommand(pScene, state,
						ePracticeOperation::ClearObjectiveBuffs);
				ImGui::EndDisabled();
				ImGui::EndTabItem();
			}
```

active Render의 reload dirty 기존 코드:

```cpp
			if (draft.bChampionDirty || draft.bSkillEffectDirty ||
				draft.bSpawnObjectDirty)
```

아래로 교체:

```cpp
			if (draft.bChampionDirty || draft.bSkillEffectDirty ||
				draft.bSpawnObjectDirty || draft.bEconomyDirty)
```

Save tooltip 기존 문자열:

```cpp
				"Validate and save the three JSON files, then ask the authoritative "
```

아래로 교체:

```cpp
				"Validate and save the four JSON files, then ask the authoritative "
```

Reload tooltip 기존 문자열:

```cpp
				"Reread the three JSON files. This discards unsaved F4 edits and "
```

아래로 교체:

```cpp
				"Reread the four JSON files. This discards unsaved F4 edits and "
```

discard popup 기존 문자열:

```cpp
				"Reload JSON discards unsaved F4 values and rereads the three data files.");
```

아래로 교체:

```cpp
				"Reload JSON discards unsaved F4 values and rereads the four data files.");
```

이로써 path/source/document/stale check/temp/backup/rollback/source update/dirty reset이 모두 같은 4파일 transaction을 탄다.

### 5-21. `Shared/GameSim/Components/ReplicatedEventComponent.h`, `Client/Public/Network/Client/EventApplier.h`, `Client/Private/Network/Client/EventApplier.cpp`

기존 Ezreal effect 상수 아래에 one-shot 전용 ID만 append한다.

```cpp
inline constexpr u32_t kObjectiveEffectElderExecute = 0x4F424A01u;
```

지속 Baron/Elder/Blue/Red/강화 미니언 표식은 EffectTrigger apply/clear 이벤트로 소유하지 않는다. `EventApplier.h` public snapshot API에 아래를 추가한다.

```cpp
    void ReconcileObjectiveVisualSnapshot(
        CWorld& world,
        EntityIdMap& entityMap,
        NetEntityId uTargetNet,
        u32_t uObjectiveStateFlags);
```

private에는 `(objective bit << 32) | targetNet` key의 visual entity map과 이번 full snapshot key set을 둔다. 매 entity row에서 5개 bit를 비교해 present면 `Objective.Baron.Buff`, `Objective.Elder.Buff`, `Objective.Blue.Buff`, `Objective.Red.Buff`, `Objective.Baron.Minion` cue를 target에 attach/upsert하고 absent/dead면 즉시 파괴한다. `BeginSnapshotReconciliation`은 seen set을 비우고, `EndSnapshotReconciliation`은 full snapshot에 없던 key를 prune한다. `RebaseTimeline`은 map의 모든 visual을 파괴하고 비운다. 이 경로가 late join/full snapshot/rewind/death 복구의 진실이다.

`ApplyEffectTrigger`는 `kObjectiveEffectElderExecute`만 source→target direction으로 `Objective.Elder.Execute` one-shot breath/burst를 재생한다. cue event는 `DamageResult`를 execute로 승격한 같은 tick에 한 번 enqueue하며 지속 표식의 수명을 소유하지 않는다.

`EventApplier.h`의 `DestroyYasuoWindWallVisuals` 아래에 추가:

```cpp
    void DestroyObjectiveVisuals(CWorld& world, u64_t objectiveKey);
```

같은 header의 snapshot set/member 군에 추가:

```cpp
    std::unordered_map<u64_t, std::vector<EntityID>> m_objectiveVisualEntities;
    std::unordered_set<u64_t> m_snapshotObjectiveVisualKeys;
```

`EventApplier.cpp` include 군에 아래 include를 추가하고 anonymous namespace의 kill-feed 상수 위에 cue table을 추가한다.

```cpp
#include "Shared/GameSim/Definitions/SnapshotStateFlags.h"
```

```cpp
    struct ObjectiveVisualCue
    {
        u32_t flag;
        const char* pCue;
    };

    constexpr ObjectiveVisualCue kObjectiveVisualCues[] = {
        { kObjectiveStateBaronBuffFlag, "Objective.Baron.Buff" },
        { kObjectiveStateElderBuffFlag, "Objective.Elder.Buff" },
        { kObjectiveStateBlueBuffFlag, "Objective.Blue.Buff" },
        { kObjectiveStateRedBuffFlag, "Objective.Red.Buff" },
        { kObjectiveStateBaronEmpoweredMinionFlag, "Objective.Baron.Minion" },
    };

    u64_t BuildObjectiveVisualKey(u32_t flag, NetEntityId targetNet)
    {
        return (static_cast<u64_t>(flag) << 32u) |
            static_cast<u64_t>(targetNet);
    }
```

`DestroyYasuoWindWallVisuals` 함수 바로 아래에 다음 complete bodies를 추가한다.

```cpp
void CEventApplier::DestroyObjectiveVisuals(
    CWorld& world,
    u64_t objectiveKey)
{
    const auto it = m_objectiveVisualEntities.find(objectiveKey);
    if (it == m_objectiveVisualEntities.end())
        return;
    for (EntityID visualEntity : it->second)
        DestroyEntityIfAlive(world, visualEntity);
    m_objectiveVisualEntities.erase(it);
}

void CEventApplier::ReconcileObjectiveVisualSnapshot(
    CWorld& world,
    EntityIdMap& entityMap,
    NetEntityId uTargetNet,
    u32_t uObjectiveStateFlags)
{
    if (uTargetNet == NULL_NET_ENTITY)
        return;

    const EntityID target = ResolveLiveEntity(world, entityMap, uTargetNet);
    for (const ObjectiveVisualCue& cue : kObjectiveVisualCues)
    {
        const u64_t key = BuildObjectiveVisualKey(cue.flag, uTargetNet);
        if ((uObjectiveStateFlags & cue.flag) == 0u)
        {
            DestroyObjectiveVisuals(world, key);
            continue;
        }

        m_snapshotObjectiveVisualKeys.insert(key);
        if (target == NULL_ENTITY ||
            !world.HasComponent<TransformComponent>(target))
        {
            continue;
        }

        auto visualIt = m_objectiveVisualEntities.find(key);
        if (visualIt == m_objectiveVisualEntities.end() ||
            !HasLiveVisualEntity(world, visualIt->second))
        {
            DestroyObjectiveVisuals(world, key);
            FxCueContext fx{};
            fx.vWorldPos =
                world.GetComponent<TransformComponent>(target).GetPosition();
            fx.vForward = { 0.f, 0.f, 1.f };
            fx.attachTo = target;
            fx.pFxMeshRenderer = m_pFxMeshRenderer;
            std::vector<EntityID> spawned;
            CFxCuePlayer::PlayAll(world, cue.pCue, fx, &spawned);
            m_objectiveVisualEntities[key] = std::move(spawned);
            visualIt = m_objectiveVisualEntities.find(key);
        }

        const Vec3 position =
            world.GetComponent<TransformComponent>(target).GetPosition();
        for (EntityID visualEntity : visualIt->second)
        {
            if (!world.IsAlive(visualEntity))
                continue;
            if (world.HasComponent<FxMeshComponent>(visualEntity))
            {
                auto& visual = world.GetComponent<FxMeshComponent>(visualEntity);
                visual.attachTo = target;
                visual.vWorldPos = position;
            }
            if (world.HasComponent<FxBillboardComponent>(visualEntity))
            {
                auto& visual = world.GetComponent<FxBillboardComponent>(visualEntity);
                visual.attachTo = target;
                visual.vWorldPos = position;
            }
        }
    }
}
```

`BeginSnapshotReconciliation`의 `m_snapshotYasuoWindWallKeys.clear();` 아래에 추가:

```cpp
    m_snapshotObjectiveVisualKeys.clear();
```

`EndSnapshotReconciliation`의 stale wall 제거 loop 바로 아래에 추가:

```cpp
    std::vector<u64_t> staleObjectiveKeys;
    for (const auto& [objectiveKey, _] : m_objectiveVisualEntities)
    {
        if (!m_snapshotObjectiveVisualKeys.contains(objectiveKey))
            staleObjectiveKeys.push_back(objectiveKey);
    }
    for (u64_t objectiveKey : staleObjectiveKeys)
        DestroyObjectiveVisuals(world, objectiveKey);
```

`RebaseTimeline`의 기존 visual destroy loops 아래에 objective map destroy loop를 추가하고, clear 군에 map/set clear를 추가한다.

```cpp
    for (const auto& [_, visuals] : m_objectiveVisualEntities)
    {
        for (EntityID entity : visuals)
            DestroyEntityIfAlive(world, entity);
    }
```

```cpp
    m_objectiveVisualEntities.clear();
    m_snapshotObjectiveVisualKeys.clear();
```

`SnapshotApplier.cpp`의 existing replicated state assignment:

```cpp
        replicatedState.stateFlags = es->stateFlags();
        replicatedState.gameplayStateFlags = es->gameplayStateFlags();
```

아래로 교체:

```cpp
        replicatedState.stateFlags = es->stateFlags();
        replicatedState.objectiveStateFlags = es->objectiveStateFlags();
        replicatedState.visualScaleMultiplier = es->visualScaleMultiplier();
        replicatedState.gameplayStateFlags = es->gameplayStateFlags();
        if (m_pEventApplier)
        {
            m_pEventApplier->ReconcileObjectiveVisualSnapshot(
                world,
                entityMap,
                es->netId(),
                es->objectiveStateFlags());
        }
```

`ApplyEffectTrigger`의 `const u32_t effectId = ev->effectId();` 바로 아래에 추가:

```cpp
    if (effectId == kObjectiveEffectElderExecute)
    {
        const EntityID source = ResolveLiveEntity(
            world, entityMap, ev->sourceNet());
        const EntityID target = ResolveLiveEntity(
            world, entityMap, ev->targetNet());
        Vec3 start{ ev->posX(), ev->posY(), ev->posZ() };
        Vec3 end{
            start.x + ev->dirX(),
            start.y + ev->dirY(),
            start.z + ev->dirZ() };
        if (source != NULL_ENTITY &&
            world.HasComponent<TransformComponent>(source))
        {
            start = world.GetComponent<TransformComponent>(source).GetPosition();
        }
        if (target != NULL_ENTITY &&
            world.HasComponent<TransformComponent>(target))
        {
            end = world.GetComponent<TransformComponent>(target).GetPosition();
        }
        Vec3 forward = WintersMath::DirectionXZ(start, end, { 0.f, 0.f, 1.f });
        FxCueContext fx{};
        fx.vWorldPos = start;
        fx.vEndWorldPos = end;
        fx.vForward = forward;
        fx.bOverrideEndWorldPos = true;
        fx.pFxMeshRenderer = m_pFxMeshRenderer;
        std::vector<EntityID> spawned;
        CFxCuePlayer::PlayAll(
            world, "Objective.Elder.Execute", fx, &spawned);
        for (EntityID entity : spawned)
            m_timelineVisualEntities.push_back(world.GetEntityHandle(entity));
        return;
    }
```

### 5-22. 새 WFX 파일

아래 6개 파일은 동일한 GroundDecal/Beam 문법을 사용하며 Wfx registry가 `Data/LoL/FX`를 재귀 스캔해 자동 등록한다.

#### 새 파일: `Data/LoL/FX/Object/Jungle/baron_buff.wfx`

```json
{
  "schema": "WintersWfx",
  "version": 1,
  "name": "Objective.Baron.Buff",
  "emitters": [
    {
      "name": "baron_ground_mark",
      "render_type": "GroundDecal",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Kindred/particles/common_sru_baroncrackdark_swirl_mult_01.png",
      "lifetime": 600.0,
      "fade_in": 0.2,
      "fade_out": 0.25,
      "width": 3.4,
      "height": 3.4,
      "color": [0.85, 0.25, 1.35, 0.82],
      "attach_offset": [0.0, 0.03, 0.0],
      "billboard": false
    }
  ]
}
```

#### 새 파일: `Data/LoL/FX/Object/Jungle/elder_buff.wfx`

```json
{
  "schema": "WintersWfx",
  "version": 1,
  "name": "Objective.Elder.Buff",
  "emitters": [
    {
      "name": "elder_ground_mark",
      "render_type": "GroundDecal",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/LeeSin/particles/base_leesin_e_magicdragon.png",
      "lifetime": 600.0,
      "fade_in": 0.2,
      "fade_out": 0.25,
      "width": 3.6,
      "height": 3.6,
      "color": [1.25, 0.55, 0.18, 0.88],
      "attach_offset": [0.0, 0.035, 0.0],
      "billboard": false
    }
  ]
}
```

#### 새 파일: `Data/LoL/FX/Object/Jungle/blue_buff.wfx`

```json
{
  "schema": "WintersWfx",
  "version": 1,
  "name": "Objective.Blue.Buff",
  "emitters": [
    {
      "name": "blue_ground_mark",
      "render_type": "GroundDecal",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_e_buff_mult.png",
      "lifetime": 600.0,
      "fade_in": 0.15,
      "fade_out": 0.2,
      "width": 2.7,
      "height": 2.7,
      "color": [0.20, 0.65, 1.50, 0.88],
      "attach_offset": [0.0, 0.025, 0.0],
      "billboard": false
    }
  ]
}
```

#### 새 파일: `Data/LoL/FX/Object/Jungle/red_buff.wfx`

```json
{
  "schema": "WintersWfx",
  "version": 1,
  "name": "Objective.Red.Buff",
  "emitters": [
    {
      "name": "red_ground_mark",
      "render_type": "GroundDecal",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Fiora/particles/common_sru_junglebuff_redbuff_health_sparkle_2x2.png",
      "lifetime": 600.0,
      "fade_in": 0.15,
      "fade_out": 0.2,
      "width": 2.7,
      "height": 2.7,
      "color": [1.55, 0.22, 0.10, 0.88],
      "attach_offset": [0.0, 0.025, 0.0],
      "billboard": false
    }
  ]
}
```

#### 새 파일: `Data/LoL/FX/Object/Jungle/baron_minion.wfx`

```json
{
  "schema": "WintersWfx",
  "version": 1,
  "name": "Objective.Baron.Minion",
  "emitters": [
    {
      "name": "baron_minion_mark",
      "render_type": "GroundDecal",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Kindred/particles/common_sru_baroncrackdark_swirl_mult_01.png",
      "lifetime": 600.0,
      "fade_in": 0.1,
      "fade_out": 0.15,
      "width": 2.1,
      "height": 2.1,
      "color": [1.05, 0.30, 1.60, 0.90],
      "attach_offset": [0.0, 0.02, 0.0],
      "billboard": false
    }
  ]
}
```

#### 새 파일: `Data/LoL/FX/Object/Jungle/elder_execute.wfx`

```json
{
  "schema": "WintersWfx",
  "version": 1,
  "name": "Objective.Elder.Execute",
  "emitters": [
    {
      "name": "elder_breath",
      "render_type": "Beam",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/LeeSin/particles/base_leesin_e_magicdragon_highlights.png",
      "lifetime": 0.55,
      "fade_in": 0.04,
      "fade_out": 0.18,
      "width": 0.75,
      "height": 0.75,
      "color": [1.70, 0.60, 0.12, 1.0],
      "attach_offset": [0.0, 1.0, 0.0],
      "end_offset": [0.0, 0.0, 8.0],
      "billboard": true
    },
    {
      "name": "elder_execute_burst",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Annie/particles/common_firehit.png",
      "lifetime": 0.65,
      "fade_in": 0.03,
      "fade_out": 0.24,
      "width": 3.2,
      "height": 3.2,
      "color": [1.75, 0.45, 0.08, 1.0],
      "attach_offset": [0.0, 1.0, 0.0],
      "segment_t": 1.0,
      "billboard": true
    }
  ]
}
```

### 5-23. 생성 산출물과 프로젝트 파일

- `Shared/Schemas/Generated/cpp/Command_generated.h`: FlatBuffers 생성 결과.
- `Shared/Schemas/Generated/cpp/Snapshot_generated.h`: FlatBuffers 생성 결과.
- `Shared/Schemas/Generated/go/Shared/Schema/PracticeOperation.go`: FlatBuffers 생성 결과.
- `Server/Private/Data/Generated/LoLGameplayDefinitions.generated.cpp`: LoL definition generator 결과.
- 새 C++ 번역 단위는 만들지 않으므로 vcxproj 등록은 없다.
- WFX는 runtime scan 자산이므로 project 등록이 없다.

### 5-24. `Tools/LoLData/Test-F4BalanceContracts.py`

기존 `spawn = load_json(...)` 아래에 추가:

```python
    economy = load_json(
        root / "Data/LoL/ServerPrivate/Gameplay/EconomyGameplayDefs.json")
```

active tab tuple 기존 코드:

```python
    for tab in ('"Champions"', '"Skills"', '"Minions"', '"Towers"'):
```

아래로 교체:

```python
    for tab in ('"Champions"', '"Skills"', '"Minions"', '"Towers"',
                '"Objectives"'):
```

truth path tuple 기존 코드:

```python
    for path_token in ("kChampionBalanceDataPath", "kSkillEffectBalanceDataPath",
                       "kSpawnObjectBalanceDataPath"):
```

아래로 교체:

```python
    for path_token in ("kChampionBalanceDataPath", "kSkillEffectBalanceDataPath",
                       "kSpawnObjectBalanceDataPath", "kEconomyBalanceDataPath"):
```

위 path loop 아래에 다음 complete objective contract를 추가한다.

```python
    objectives_surface = tuner_surface.split(
        'BeginTabItem("Objectives")', 1)[1].split(
            'ImGui::EndTabBar()', 1)[0]
    for token in (
            '"teamGoldPerChampion"', '"teamLevelGrant"',
            '"buffDurationSec"', '"baronRecallDurationMultiplier"',
            '"baronAuraRadius"', '"baronMinionHpMultiplier"',
            '"baronMinionAttackDamageMultiplier"',
            '"baronMinionScaleMultiplier"',
            '"elderAttackDamageMultiplier"', '"elderBurnDurationSec"',
            '"elderBurnTickIntervalSec"',
            '"elderBurnTargetMaxHpRatioPerTick"',
            '"elderExecuteThresholdRatio"', '"blueManaRegenPerSec"',
            '"redHealthRegenPerSec"', '"redBurnDurationSec"',
            '"redBurnTickIntervalSec"', '"redBurnDamagePerTick"',
            "RefillJungleHealth", "ResetJungleMonster",
            "ClearObjectiveBuffs"):
        require(token in objectives_surface, f"active F4 objective contract {token}")
    for token in ("bEconomyDirty", "draft.economyPath",
                  "draft.economySource"):
        require(token in tuner, f"F4 economy draft contract {token}")
    save_slice = tuner.split("bool_t SaveBalanceData", 1)[1].split(
        "namespace UI", 1)[0]
    require("draft.bEconomyDirty" in save_slice and
            "&draft.economySource" in save_slice and
            "&draft.economy" in save_slice,
            "Economy participates in the atomic JSON transaction")
    require("four JSON files" in tuner_surface and
            "three JSON files" not in tuner_surface,
            "active F4 explains the four-file transaction")
    require(economy["objectives"]["teamLevelGrant"] == 3,
            "objective team level integer")
    require_close(economy["objectives"]["buffDurationSec"], 300.0,
                  "objective duration")
    require_close(economy["objectives"]["elderAttackDamageMultiplier"], 1.7,
                  "elder total AD multiplier")
```

기존 `command_schema = ...read_text(...)`와 append-only minion operation assertions 바로 아래에 추가:

```python

    for operation in (
            "RefillJungleHealth = 32", "ResetJungleMonster = 33",
            "ClearObjectiveBuffs = 34"):
        require(operation in command_schema,
                f"objective practice command {operation}")
```

기존 `snapshot_schema = ...read_text(...)` 바로 아래에 추가:

```python

    require("objectiveStateFlags:uint = 0" in snapshot_schema,
            "objective state has an append-only snapshot field")
    require("visualScaleMultiplier:float = 1.0" in snapshot_schema,
            "baron visual scale is snapshot replicated")
    event_applier = (root /
        "Client/Private/Network/Client/EventApplier.cpp").read_text(
            encoding="utf-8")
    for token in ("ReconcileObjectiveVisualSnapshot",
                  "m_snapshotObjectiveVisualKeys",
                  "DestroyObjectiveVisuals",
                  "kObjectiveEffectElderExecute"):
        require(token in event_applier,
                f"objective snapshot visual reconciliation {token}")
```

## 6. 검증

### 6-1. 정적/데이터

```powershell
python Tools/LoLData/Build-LoLDefinitionPack.py
python Tools/LoLData/Build-LoLDefinitionPack.py --check
python Tools/LoLData/Test-F4BalanceContracts.py
python Tools/LoLData/Test-BasicAttackTimingContract.py
& Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1 -Configuration Debug
$wfxTextures = @(
  'Client/Bin/Resource/Texture/Character/Kindred/particles/common_sru_baroncrackdark_swirl_mult_01.png',
  'Client/Bin/Resource/Texture/Character/LeeSin/particles/base_leesin_e_magicdragon.png',
  'Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_e_buff_mult.png',
  'Client/Bin/Resource/Texture/Character/Fiora/particles/common_sru_junglebuff_redbuff_health_sparkle_2x2.png',
  'Client/Bin/Resource/Texture/Character/LeeSin/particles/base_leesin_e_magicdragon_highlights.png',
  'Client/Bin/Resource/Texture/Character/Annie/particles/common_firehit.png'
)
if ($wfxTextures.Where({ -not (Test-Path -LiteralPath $_) }).Count) { exit 1 }
git diff --check
```

예상 관측:

- JSON/schema/runtime overlay/generated pack의 objective 18개 값이 일치한다.
- Flash cooldown query가 20.0을 반환한다.
- Baron/Dragon spawned Health/Stat은 10000/30/30이다.

### 6-2. Dragon animation bake 감사

`Tools/LoLData/Test-BasicAttackTimingContract.py`에 `DRAGON_MODEL_ROOT`, `DRAGON_ATTACK_CLIP`, `JUNGLE_MANAGER_PATH` 상수를 추가하고 기존 `read_wskel_hash`/`read_valid_wanim`을 그대로 재사용하는 `validate_dragon_attack_contract(errors)` 함수를 추가한다. 이 함수는 아래 exact 경로와 C++ mapping 문자열을 검사하며 `main()`의 champion loop 뒤에서 호출한다.

```text
C:/Users/user/Desktop/Winters/Client/Bin/Resource/Texture/Object/Jungle/Dragon/air/dragon_air_textured.wmesh/.wskel 존재 및 non-zero
C:/Users/user/Desktop/Winters/Client/Bin/Resource/Texture/Object/Jungle/Dragon/air/anims/sru_dragon_flying_attack1.wanim runtime header valid
clip skeleton hash == dragon_air_textured.wskel hash
durationTicks > 0, ticksPerSecond > 0
Client/Private/Manager/Jungle_Manager.cpp 안의 `sru_dragon_flying_attack1` exact mapping 존재
```

정적 contract는 bake/header/skeleton/mapping만 증명한다. `CombatAction -> ReplicatedAction BasicAttack -> client clip playback -> server windup hit` 정렬은 §6-5 runtime capture로 별도 판정한다. 전용 hit/death clip 부재는 오류로 위장하지 않고 RESULT의 자산 한계로 기록한다.

### 6-3. SimLab 결정성 시나리오

`Tools/SimLab/main.cpp`에 다음 contract를 추가한다.

1. 일반 캠프 kill: killer +80g/+240XP, 다른 팀원 변화 없음.
2. Baron kill: 같은 팀 5명 각 +2000g, 각 +3 level clamp, 생존자만 Baron buff.
3. Elder kill: Baron과 동일 보상, 생존자만 Elder buff.
4. 동일 buff 재획득: component 수 증가 없음, expireTick만 연장.
5. 300초/사망: buff 제거, Elder AD 원복, snapshot objective bit 제거, client reconciliation visual 파괴.
6. Blue/Red: 30 ticks 동안 mana/HP +10, cap 준수.
7. Red basic attack: 3 ticks ×10 magic, refresh no stack.
8. Elder: final AD ×1.7, burn 3×1% maxHP, >20% no execute, <=20% same hit execute, kill reward 1회.
9. Baron recall: 6초 정의에서 3초 channel.
10. Baron aura: enter HP/AD ×3/×2, visual flag set, leave ratio-preserving restore.
11. F4 refill: reward/buff/position 불변. Reset: anchor/health/AI만 복구. Clear buffs: all original stats restored.
12. rewind/keyframe: objective buff/burn/empower 상태 복원 후 hash 동일.
13. JSON hot reload 중 aura enter/stay/leave: ratio-preserving unapply → 새 base → 현재 multiplier 재적용, Health/Minion mirror 일치.

`main()`에 `--objective-buffs-only` 분기를 추가해 위 probe를 실제 실행한다.

### 6-4. 빌드

실제 solution/configuration 이름을 확인한 뒤 다음 관련 타깃을 순서대로 빌드한다.

```powershell
$msbuildCommand = Get-Command msbuild -ErrorAction SilentlyContinue
if ($msbuildCommand) {
  $msbuild = $msbuildCommand.Source
} else {
  $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
  if (Test-Path -LiteralPath $vswhere) {
    $msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
  }
  if (-not $msbuild) {
    $msbuild = Get-ChildItem 'C:/Program Files/Microsoft Visual Studio' -Recurse -Filter MSBuild.exe -ErrorAction SilentlyContinue |
      Where-Object { $_.FullName -match 'MSBuild\\Current\\Bin\\MSBuild.exe$' } |
      Select-Object -First 1 -ExpandProperty FullName
  }
}
if (-not $msbuild -or -not (Test-Path -LiteralPath $msbuild)) {
  throw 'MSBuild.exe was not found.'
}
& $msbuild Winters.sln /t:GameSim /p:Configuration=Debug /m:1
& $msbuild Winters.sln /t:SimLab /p:Configuration=Debug /m:1
& Tools/Bin/Debug/SimLab.exe --objective-buffs-only
& Tools/Bin/Debug/SimLab.exe --f4-balance-only
& $msbuild Winters.sln /t:Server /p:Configuration=Debug /m:1
& $msbuild Winters.sln /t:Client /p:Configuration=Debug /m:1
```

현재 solution project 이름 `GameSim/SimLab/Server/Client`를 사용한다. `Get-Command msbuild` 실패 시 `vswhere` 또는 설치된 `C:/Program Files/Microsoft Visual Studio/*/*/MSBuild/Current/Bin/MSBuild.exe`를 resolve하고 RESULT에 실제 경로를 기록한다. schema 생성 gotcha 때문에 전 빌드는 `/m:1`을 고정한다.

### 6-5. 인게임 시각 QA

- F4에서 Dragon 선택 → Reset → 피격 → Dragon이 타깃을 향해 회전하고 `sru_dragon_flying_attack1`이 재생되며 실제 hit timing에 피해가 들어오는지 캡처.
- Baron/Elder kill → 생존 아군 5분 표식, 사망 즉시 제거.
- Blue/Red kill → 챔피언 발밑 파랑/빨강 표식과 재생 수치 확인.
- Elder target 21%/20% 두 케이스 → 21% 생존, 20% breath/burst 후 사망.
- Baron champion이 미니언에게 접근/이탈 → 보라 표식, 2배 크기, HP/AD 배율 및 원복.
- F4 Refill/Reset/Clear Buffs를 연속 수행해 중복 이펙트·배율 누적·추가 보상이 없는지 확인.

미검증 시 “빌드 성공”을 “시각 검증 성공”으로 표현하지 않는다. 클라이언트 실행/캡처가 환경상 불가능하면 정확한 미검증 항목을 RESULT에 남긴다.

## 7. 예산·단계·롤백

```text
예산 상한: 전체 세션의 30%를 데이터/계약/계획·비평에, 45%를 Shared/Server 구현에,
20%를 Client/F4/WFX에, 5%를 문서·인계에 사용한다. 검증 실패 수정은 각 구현 예산 안에서 우선한다.
```

1. 데이터/스키마/코드젠 계약 → JSON pipeline 검사.
2. Shared objective 상태/보상/피해/버프 → SimLab.
3. F4 live override/reset → F4 contract + Server build.
4. snapshot/client visual/WFX → Client build + capture.
5. 전체 회귀/RESULT.

롤백은 단계별이다. 데이터 파싱 실패 시 generated 산출물까지 함께 되돌리고, gameplay 실패 시 Objective component/system 호출만 제거하면 기존 일반 정글 보상으로 돌아간다. client visual 실패는 서버 gameplay와 분리해 WFX/EventApplier만 롤백할 수 있다.

## 8. 서브 에이전트 비평

```text
비평 주체: /root/plan_critique (Huygens), read-only 코드·데이터·빌드 대조
P0-1 수용: active F4는 ChampionTuner JSON editor이고 Jungle Balance (Live)는 #if 0임. Economy를 네 번째 atomic document로 추가하고 active Objectives tab을 사용하며 수치 practice override를 제거함.
P1-1 수용: Elder execute는 새 request가 아니라 PromoteDamageResultToExecution으로 Health/mirror/result를 같은 hit에서 승격하고 event/hook/reward 1회를 검증함.
P1-2 수용: full objective tick과 death-only cleanup을 분리하고 DamageQueue -> cleanup -> practice controls -> final Stat -> Death 순서를 고정함.
P1-3 수용: Baron minion hot reload를 ratio-preserving unapply -> base refresh -> aura reapply로 바꾸고 Jungle/Health/Minion mirror를 함께 갱신함.
P1-4 수용·강화: scale float를 Snapshot에 append함. 추가 코드 감사에서 기존 stateFlags bit 7..30이 ChampionAI debug와 충돌함을 찾아 objectiveStateFlags를 별도 append-only field로 분리함.
P1-5 수용: 지속 WFX는 snapshot reconciliation Upsert/Prune을 진실로 두고 late join/full snapshot/death/rebase를 복구함. execute만 one-shot event이며 6개 texture Test-Path gate를 추가함.
P1-6 수용: Test-BasicAttackTimingContract.py의 exact Dragon 경로/함수/명령을 고정하고 runtime hit alignment capture를 별도 판정함.
P1-7 수용: PowerShell 실행 방식, GameSim target, /m:1, resolved MSBuild, 실제 SimLab 실행을 교정하고 pseudo API를 제거함.
P2 수용: team reward를 sorted EntityID로 순회하고 PracticeSpawnedTag를 제외함. teamLevelGrant를 u8/integer로 바꾸고 Tibbers(role 4)를 오라에서 제외함. F4 사용자 작업/authority/dirty-stale-ack/visual QA 계약을 보강함.
재비평 1차: P0 해소, exact plan P1 잔존으로 보류.
재비평 2차: persistent cue/MSBuild/F4 4-doc/snapshot body 해소, header path·문자열 exact block P1 잔존으로 보류.
최종 delta 재비평: 실제 Client/Public header 경로, dirty 조건, 세 문자열 exact block, objective_surface contract를 확인함. 최종 P0=0, P1=0 — 구현 게이트 통과.
```

## 9. RESULT 인계 계약

동일 이름의 `_RESULT.md`는 규칙대로 세 섹션만 둔다.

1. 예측 vs 실측: 수치·동작·시각, 명령/exit code/캡처, Dragon bake와 hit/death clip 한계, 프롬프트 비평을 이 섹션에 함께 기록한다.
2. 판결: 계획 유지/수정 반영/롤백과 근거.
3. ⑤ 갱신: schema/command/snapshot 호환성, 실측 후 달라진 대가와 이 설계가 틀리는 조건.

## 10. 적용 및 검증 종료 상태

```text
적용 상태: 계획의 데이터/Shared GameSim/Server/F4/Snapshot/Client WFX 범위를 모두 반영했다.
추가 안전 보정: F4에서 burn tick interval은 0보다 커야 하고 Elder max-HP burn ratio와 execute ratio는 [0, 1]이어야 저장되도록 제한했다. JSON schema의 Elder burn ratio도 maximum=1로 맞췄다.
빌드: GameSim, Server, Client, SimLab Debug x64 성공.
전용 회귀: objective-buffs-only exit 0, F4BalanceContracts PASS, BasicAttackTiming PASS, WFX 문서/texture 계약 PASS, git diff --check PASS.
전체 회귀: 일반 SimLab과 f4-balance-only는 현 작업 트리의 기존 Ezreal Q rank-3 데이터(200 damage, total AD 5.0)가 기존 probe 기대값(70, 1.3)과 달라 exit 1. Objective 전용 probe와 F4 wire/minion/cooldown 하위 probe는 통과했다.
시각 미검증: 실제 F5 화면에서 5분 지속 표식, Elder breath 접점, Baron minion submesh 외형, Dragon 타격 접점은 캡처하지 못했다. 빌드 성공과 시각 성공을 동일시하지 않는다.
```
