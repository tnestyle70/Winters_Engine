# Ch8. GameplayAbilitySystem (GAS) — Tags / Effects / Attributes

> Winters 현재: `Client/Public/GameObject/SkillTable.h` (챔프별 hardcoded), `Client/Private/GamePlay/SkillRegistry.cpp`, `Shared/GameSim/Components/SkillState*` (서버 권위 cooldown).
> memory 박제 `project_phase_b11d_v31_ezreal_pending.md`: Registry 3종 + hookId 4분할 + ChampionRegistry BanPick 보정안.
> 레퍼런스: `UnrealEngine/Engine/Plugins/Runtime/GameplayAbilities/Source/GameplayAbilities/Public/`.

---

## 1. 기초 원리 — 150 챔프 × 4 스킬 × 보스 100가지 패턴

문제의 본질:
- 챔프마다 스킬 4개 = 600개
- 각 스킬은 cooldown, mana cost, range, damage, status effect, animation, fx, sound, condition
- 패시브, 아이템, 룬, 버프, 디버프, 마법저항, 방어구 관통...
- 보스 마다 phase 별 패턴 다름

이걸 `if (champ == Ezreal) cast()`로 짜면 600 함수 + 수만 if. 유지 불가.

**GAS의 통찰**:
1. 스킬은 **데이터 + 작은 trigger 코드** 조합이다
2. 스탯(HP/MP/AD/AP)은 **자동 dirty 추적되는 attribute**다
3. 상태(stun/silence/airborne)는 **tag**다. 트리/오너십이 있는 string.
4. 효과(데미지, 회복, 슬로우)는 **GameplayEffect**다. 적용/제거가 1급.
5. 스킬은 **AbilityTask**(작은 빌딩블록: projectile / channel / dash)의 시퀀스다.

이 5가지로 600개를 데이터 + 30~50개 빌딩블록 함수로 압축.

---

## 2. 핵심 — UE5 GAS 5대 구성

### 2.1 GameplayTag

계층 string. `"Status.Stun"`, `"Damage.Magic.Fire"`, `"Cooldown.Skill.Q"`.

```cpp
// Source/Runtime/GameplayTags/Classes/GameplayTagsManager.h
FGameplayTag SkillCooldown_Q = FGameplayTag::RequestGameplayTag("Cooldown.Skill.Q");

// 컨테이너
FGameplayTagContainer Tags;
Tags.AddTag(FGameplayTag::RequestGameplayTag("Status.Stun"));
Tags.AddTag(FGameplayTag::RequestGameplayTag("Status.Silence"));

if (Tags.HasTag(FGameplayTag::RequestGameplayTag("Status")))   // true (계층 매치)
```

매치 규칙:
- `HasTag(parent)` → 모든 자식 매치 (`Status` ⊃ `Status.Stun`)
- `HasTagExact(tag)` → 정확히 일치

### 2.2 AttributeSet

`Plugins/Runtime/GameplayAbilities/Source/GameplayAbilities/Public/AttributeSet.h`:

```cpp
// 매크로로 attribute 정의 (PROPERTY → setter / getter / replication 자동)
ATTRIBUTE_ACCESSORS(UMyAttributeSet, Health)
ATTRIBUTE_ACCESSORS(UMyAttributeSet, MaxHealth)
ATTRIBUTE_ACCESSORS(UMyAttributeSet, Mana)

UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Health)
FGameplayAttributeData Health;

UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxHealth)
FGameplayAttributeData MaxHealth;
```

**자동화 포인트**:
- Attribute 변경은 `PreAttributeChange` / `PostGameplayEffectExecute` 훅으로 인터셉트 (`Health = min(Health, MaxHealth)`)
- 클램프, 보정, replicated dirty 자동
- 데미지 = "Health -= X" 아님. **GameplayEffect(Health, -X) Apply**.

### 2.3 GameplayEffect

지속 시간, 스택, 매그니튜드, modifier로 상태 변경.

```cpp
// 데이터 자산 (UGameplayEffect)
Class: GE_FireDamage
  DurationPolicy: Instant            // 즉발 / Duration / Infinite
  Modifiers:
    - Attribute: Health
      Operation: Add
      Magnitude: -50               // 또는 ScalableFloat (level curve)
  GrantedTags:
    - Damage.Magic.Fire
  PeriodicEffect: None
```

```cpp
// 적용
FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(GE_FireDamage, Level, Ctx);
ASC->ApplyGameplayEffectSpecToTarget(*Spec.Data, TargetASC);
```

`Duration`이면 자동 expire. `PeriodicEffect`로 dot 자동 tick.

### 2.4 GameplayAbility

`Plugins/Runtime/GameplayAbilities/Source/GameplayAbilities/Public/Abilities/GameplayAbility.h:40~75`:

```cpp
/**
 * UGameplayAbility
 *  Abilities define custom gameplay logic that can be activated or triggered.
 *
 *  The main features provided by the AbilitySystem for GameplayAbilities are:
 *      -CanUse functionality:
 *          -Cooldowns
 *          -Costs (mana, stamina, etc)
 *      -Replication support
 *          -Client/Server communication for ability activation
 *          -Client prediction for ability activation
 *      -Instancing support
 *          -Abilities can be non-instanced (native only)
 *          -Instanced per owner
 *          -Instanced per execution (default)
 *      -Basic, extendable support for:
 *          -Input binding
 *          -'Giving' abilities (that can be used) to actors
 */
```

핵심 함수 시그니처:

```cpp
class UGameplayAbility : public UObject
{
public:
    virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                    const FGameplayTagContainer* SourceTags, ...) const;
    virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                 const FGameplayAbilityActivationInfo ActivationInfo, ...);
    virtual void EndAbility(...);
    virtual void CancelAbility(...);

    // 코드 안에서 위와 같이 호출
    void CommitAbility(...);           // cooldown + cost 모두 적용
    void CommitAbilityCooldown(...);
    void CommitAbilityCost(...);
};
```

### 2.5 AbilityTask (빌딩블록)

스킬이 직접 코드를 다 짜지 않는다. 짧은 비동기 task로 분해.

UE5 내장 task 예:
```text
UAbilityTask_PlayMontageAndWait      ← Ch4 montage 재생 + 끝/notify wait
UAbilityTask_WaitTargetData          ← 사용자가 위치 선택 대기
UAbilityTask_WaitInputPress          ← 추가 입력 wait
UAbilityTask_WaitGameplayEvent       ← 이벤트 tag 발생 wait
UAbilityTask_ApplyRootMotion          ← 대시
UAbilityTask_SpawnActor              ← 발사체 spawn
```

```cpp
void UGA_Ezreal_Q::ActivateAbility(...)
{
    if (!CommitAbility(...))   return;   // cooldown + mana 검사 + 차감

    UAbilityTask_PlayMontageAndWait* M = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this, NAME_None, EzrealQMontage);
    M->OnCompleted.AddDynamic(this, &UGA_Ezreal_Q::OnMontageEnd);
    M->Activate();
    // 코드는 여기서 return. OnMontageEnd 콜백에서 EndAbility.
}
```

---

## 3. 심화

### 3.1 Prediction / Confirmation

GAS는 **client prediction**이 1급:

```text
t=0   client: Q press. CanActivate 검사. 통과 → ActivateAbility 시작 (cooldown 시작, fx 재생).
              서버에 "I'm activating Q at t=0" RPC.
              PredictionKey 발급.
t=50  server: RPC 도착. 같은 CanActivate 검사. 통과/실패.
              결과 클라에 보냄. 통과면 같은 prediction key로 commit.
t=100 client: 결과 도착. 일치하면 무시. 불일치면 rollback (cooldown 회복, fx 취소).
```

UE5: `FPredictionKey`, `FScopedPredictionWindow`.

### 3.2 GameplayCue

fx + sound + montage 묶음. 서버가 trigger, 클라가 재생.

```cpp
// 서버
ASC->ExecuteGameplayCue(FGameplayTag::RequestGameplayTag("Cue.Ability.Ezreal.Q.Impact"), Ctx);

// 클라: 같은 tag로 등록된 UGameplayCueNotify가 spawn → particle/sound/decal 자동
```

Ch4 Animation Notify, Ch6 Audio Cue가 여기서 합류.

### 3.3 Tag-driven cancellation

```cpp
// ability spec
CancelAbilitiesWithTag = "Status.Channel"   // 이 ability 시전 시, channel 류는 취소
BlockAbilitiesWithTag  = "Status.Stun"      // stun이면 시전 불가
ActivationBlockedTags  = "Cooldown.Skill.Q" // Q 쿨다운 중 시전 불가
```

데이터로 제어. 코드 추가 없음.

### 3.4 Modifier 계산 그래프

데미지 = 공격력 × 스킬계수 + 보정 - 방어 + 관통 - 저항 ... 이걸 ScalableFloat / Curve / Calc class로 데이터화.

```cpp
class UMyDamageCalc : public UGameplayEffectExecutionCalculation
{
    virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& Params, ...) const override
    {
        float AD = GetCapturedAttribute(Source, AttackDamage);
        float Armor = GetCapturedAttribute(Target, Armor);
        float Final = AD * (100.f / (100.f + Armor));
        Params.ModifiableAttribute(Target_Health, -Final);
    }
};
```

이 패턴이 LoL의 모든 데미지 공식의 단일 진입점이 된다.

### 3.5 AbilitySystemComponent (ASC)

actor마다 1개. 모든 GAS state(attribute, tag, active effect, granted ability, cue queue)를 들고 있고 replication 처리.

```cpp
class UAbilitySystemComponent : public UGameplayTasksComponent, public IGameplayTagAssetInterface
{
public:
    UPROPERTY(ReplicatedUsing=OnRep_ActivateAbilities)
    FGameplayAbilitySpecContainer ActivatableAbilities;

    UPROPERTY(Replicated)
    FActiveGameplayEffectsContainer ActiveGameplayEffects;

    FGameplayTagCountContainer GameplayTagCountContainer;
    FActiveGameplayCueContainer ActiveGameplayCues;
};
```

---

## 4. Winters 매핑

### 4.1 현재 상태

- `Shared/GameSim/Components/SkillState*` — 서버 권위 cooldown 1차
- `Client/Public/GameObject/SkillTable.h` — 챔프별 SkillDef hardcoded
- `Client/Private/GamePlay/SkillRegistry.cpp` — 등록 로직

memory `project_phase_b11d_v31_ezreal_pending.md` 박제:
> Registry 3종 + hookId 4분할 + ChampionRegistry BanPick + GenericPendingHit + Damage 공통 + eChampion 명시값.

이게 Winters의 GAS 1차 등가.

### 4.2 Ch8 추가 헤더 (제안)

```cpp
// Shared/GameSim/Abilities/AbilitySystem.h
class WINTERS_SHARED CAbilitySystem
{
public:
    void Initialize(World& world);
    void GrantAbility(EntityID owner, AbilityID ability);
    void RemoveAbility(EntityID owner, AbilityID ability);

    bool TryActivateAbility(EntityID owner, AbilityID ability, const ActivationParams& params);
    void CancelAbility    (EntityID owner, AbilityID ability);

    void ApplyEffect(EntityID source, EntityID target, GameplayEffectID effect);
    void RemoveEffect(EntityID target, ActiveEffectHandle handle);
};

// Shared/GameSim/Abilities/AbilityDef.h
struct AbilityDef
{
    AbilityID    id;
    char         name[32];
    f32_t        cooldown;
    f32_t        cost_mana;
    GameplayTagContainer cancelTags;     // 시전 시 취소할 자기 tag
    GameplayTagContainer blockTags;      // 가지면 시전 불가
    // 시퀀스: AbilityTask들의 데이터 표현
    std::vector<AbilityTaskInstance> tasks;
};

// Shared/GameSim/Abilities/AbilityTask.h
enum class eAbilityTaskType : u8_t {
    PlayMontage, WaitTarget, WaitInput, Dash, SpawnProjectile, ApplyEffect, WaitEvent
};

// Shared/GameSim/Abilities/GameplayTag.h
struct GameplayTag { u32_t hash; const char* debugName; };   // 계층 hash
class CGameplayTagManager { /* singleton, .tags asset 로딩 */ };

// Shared/GameSim/Abilities/GameplayEffect.h
struct GameplayEffectDef
{
    enum class eDuration { Instant, Duration, Infinite } durationPolicy;
    f32_t duration;
    f32_t period;
    std::vector<EffectModifier> modifiers;
    GameplayTagContainer grantedTags;
};

// Shared/GameSim/Abilities/AttributeSet.h
// ECS friendly: 그냥 component
struct HealthComponent     { f32_t value, max; };
struct ManaComponent       { f32_t value, max; };
struct AttackDamageComp    { f32_t value; };
struct ArmorComponent      { f32_t value; };
struct MoveSpeedComponent  { f32_t value; };
// PreAttributeChange는 ECS system이 처리 (clamp 등)

// Shared/GameSim/Abilities/GameplayCue.h
// 서버 → 클라 cue. 단일 source of truth.
struct GameplayCueEvent
{
    u32_t tagHash;        // "Cue.Ability.Ezreal.Q.Impact"
    EntityID source;
    EntityID target;
    Vec3 location;
};
```

### 4.3 Bot AI 불변식 (재확인 — 핵심)

CLAUDE.md 2026-05-12 박제:
> AI는 `GameplayEffect.Apply` / `AbilityTask.Execute`를 직접 호출하지 않는다.
> AI는 `GameCommand`를 만들고, executor가 `AbilitySystem::TryActivate`를 호출한다.
> AttributeSet 변경은 서버 GameSim에서만.

즉:
```text
[잘못된 코드]
botAI.AttackTarget(target):
    target.Health -= 50           ← 금지
    target.GameplayTags.Add("Stunned")  ← 금지

[올바른 코드]
botAI.AttackTarget(target):
    return GameCommand::CastAbility(self, "BasicAttack", target)
            ↓
        DefaultCommandExecutor receives
            ↓
        AbilitySystem::TryActivate(self, "BasicAttack", {target})
            ↓ (CanActivate 통과 시)
        BasicAttack ability run → projectile / effect / damage 정상 경로
```

### 4.4 단계별

```text
Ch8-Stage1  GameplayTag + 계층 매치
Ch8-Stage2  AttributeSet (HP/MP/AD/Armor 등) ECS component
Ch8-Stage3  GameplayEffect Instant (즉발 데미지) + tag grant
Ch8-Stage4  GameplayEffect Duration / Infinite (버프, dot)
Ch8-Stage5  AbilityDef + TryActivateAbility + cooldown/cost
Ch8-Stage6  AbilityTask 빌딩블록 (PlayMontage, WaitTarget, SpawnProjectile)
Ch8-Stage7  GameplayCue (서버 trigger, 클라 fx+sound+montage)
Ch8-Stage8  Client prediction + reconciliation (Ch7과 묶임)
Ch8-Stage9  Damage calculation graph (ModifierCalculation class)
Ch8-Stage10 Tag-driven cancel/block 시스템
Ch8-Stage11 AbilityEditor (Ch12 Editor + Ch15 Data) → 디자이너 자율
```

### 4.5 게임별 적용

| 게임 | 필요 Stage |
|------|-----------|
| LoL (현재) | Stage 1~7 + Stage 8 + Stage 9 |
| 로스트아크 | Stage 1~10 + 직업 패시브/룬 (modifier 계산 복잡) |
| 엘든링 | Stage 1~10 + 무기 인챈트 / 마법 변주 / 패시브 효과 |
| GTA6 | GAS는 약하게. 대신 vehicle/weapon system이 별도 |

---

## 5. 검증 명령

```powershell
.\Client\Bin\Debug-DX12\WintersGame.exe --gas-debug --gas-show-active

# 기대 로그
# [GAS] entity#42 (Ezreal) granted abilities: [Q,W,E,R,BasicAttack]
# [GAS] entity#42 TryActivate(Q): CanActivate=true, cooldown=0/8s, mana=80/300
# [GAS] entity#42 ability Q started, predictionKey=1234
# [GAS] entity#42 ability Q committed: cooldown=8s, mana=50
# [GAS] entity#42 GE_Q_Damage applied to entity#88 (-120 HP), tags=[Damage.Magic]
# [GAS] entity#42 cue trigger: Cue.Ability.Ezreal.Q.Impact @ (12,0,34)
```

---

## 6. 다음 챕터로

Ch8 Stage 7까지 가야 **Ch9 AI**의 BehaviorTree가 `TryActivateAbility`를 통한 정상 cast 가능. Ch11 Sequencer/Cinematics에서 보스 패턴은 GAS ability sequence로 작성됨.
