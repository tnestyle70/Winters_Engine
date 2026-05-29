# Ch4. Animation Stack (StateMachine / BlendTree / Montage / IK / MotionMatching)

> Winters 현재: `Engine/Public/Resource/Animation.h, Animator.h, Bone.h, Skeleton.h` — skinned mesh + 단일 클립 재생.
> 챔프별 `_Skills.cpp`에 castFrame 숫자 박혀 있음. 150 챔프 가면 무너진다.
> 레퍼런스: `UnrealEngine/Engine/Source/Runtime/AnimGraphRuntime/Public/AnimNodes/`, `Runtime/Engine/Classes/Animation/AnimMontage.h`.

---

## 1. 기초 원리 — Animation은 데이터 그래프다

옛날 게임: `PlayClip("Attack")`. 끝. 1캐릭 ≤ 10클립일 때 OK.

문제 발생:
- 이동 중 공격: walk + attack을 어떻게 합쳐?
- 상체는 활 쏘고 하체는 달리기: 어떻게 따로?
- 부상 입었을 때 stagger 섞기: 어떻게?
- 손에 든 무기가 정확히 적 칼을 막기: 어떻게?
- 캐릭터 100, 모션 5000개: 어떻게 관리?

해결책: **포즈를 매 프레임 그래프로 합성한다.**

각 단계는 **포즈를 입력받아 포즈를 반환**하는 함수. 그래프 노드들이 합쳐져 최종 본 변환 산출.

```text
[Idle] ─┐
        ├─ BlendBy(speed) ──┐
[Run] ──┘                   ├─ Layer(upperBody, AttackMontage) ──┐
                            │                                    ├─ IK(footPlant, lookAt) → 최종 본
[Aim]   ─── BlendBy(yaw) ───┘                                    │
[Hit Reaction Additive] ─────── ApplyAdditive ───────────────────┘
```

이 그래프가 UE5의 **AnimGraph**. 데이터로 정의되고 런타임에 평가.

---

## 2. 핵심 — UE5 Animation 6대 구성요소

### 2.1 AnimSequence

원본 키프레임 데이터 (bone transform per frame). DCC 툴에서 import.

### 2.2 AnimBlueprint / AnimInstance

캐릭터 1종에 1개. 위 그래프를 정의.

```cpp
class UAnimInstance : public UObject
{
    // 매 tick:
    virtual void NativeUpdateAnimation(float DeltaSeconds);   // 상태 갱신
    virtual void NativeThreadSafeUpdateAnimation(...);        // 워커 스레드 안전
    // 그 후 worker thread가 AnimGraph 평가 → 본 변환
};
```

### 2.3 AnimNode

그래프의 한 노드. 모두 base class `FAnimNode_Base`.

`UnrealEngine/Engine/Source/Runtime/AnimGraphRuntime/Public/AnimNodes/`에 50개 넘게:

```text
AnimNode_BlendListByBool.h          bool 분기
AnimNode_BlendListByEnum.h          enum 분기
AnimNode_BlendListByInt.h           int 분기
AnimNode_BlendSpacePlayer.h         BlendSpace 재생 (2D blend, walk/run/jog)
AnimNode_LayeredBoneBlend.h         상체/하체 분리
AnimNode_ApplyAdditive.h            additive 포즈 합성
AnimNode_Mirror.h                   미러링 (왼손잡이 모션)
AnimNode_CopyPoseFromMesh.h         다른 메시 포즈 복사
AnimNode_PoseDriver.h               포즈 기반 트리거 (꼬리, 옷자락)
```

각 노드는:

```cpp
struct FAnimNode_BlendListByBool : public FAnimNode_BlendListBase
{
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Inputs, meta=(PinShownByDefault))
    bool bActiveValue = false;

    virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
    virtual void Evaluate_AnyThread(FPoseContext& Output) override;
};
```

### 2.4 State Machine

상태 전이 그래프. UE5에서는 AnimGraph 안의 한 노드(`FAnimNode_StateMachine`)다.

```text
[Locomotion] ──[bAttack]──→ [Attack]
     ↑                          │
     └────[OnAttackEnd]─────────┘
     ↓ [bDead]
   [Dead]
```

각 state는 또 다른 sub-AnimGraph. 한 state가 BlendSpace, 다른 state가 Sequence, 또 다른 state가 nested StateMachine.

### 2.5 AnimMontage

스킬/공격/처형 등 **이벤트성 모션**. AnimGraph state machine 위에 **레이어로 덮어쓴다**.

`Source/Runtime/Engine/Classes/Animation/AnimMontage.h:31~60`:

```cpp
USTRUCT()
struct FCompositeSection : public FAnimLinkableElement
{
    UPROPERTY(EditAnywhere, Category=Section)
    FName SectionName;

    /** Should this animation loop. */
    UPROPERTY(VisibleAnywhere, Category=Section)
    FName NextSectionName;

    /** Meta data that can be saved with the asset */
    UPROPERTY(Category=Section, Instanced, EditAnywhere)
    TArray<TObjectPtr<class UAnimMetaData>> MetaData;
};
```

**Section 시스템**: 한 montage 안에 여러 section. `JumpToSection("Combo2")`로 전환.

LoL Ezreal Q 1타/2타/3타 평타가 정확히 이 패턴이다. 현재 Winters의 `castFrame` hard-coded를 montage section + notify로 옮기는 게 Ch4 핵심.

### 2.6 AnimNotify

타임라인 위의 **점/구간 이벤트**.

```text
montage AttackCombo
  t=0.0  ──── 모션 시작
  t=0.2  [Notify: FxCue("slash_trail")]
  t=0.3  [Notify: GameplayEvent("hitFrame")]      ← 데미지 판정 발생
  t=0.5  [Notify: AudioCue("blade_hit")]
  t=0.7  [Notify: AnimMontageEnd]
```

서버는 `actionSeq` cue만 보내고, 클라가 montage의 notify를 처리해 fx/sound/damage prediction 호출. 이게 **single source of truth: animation track**.

---

## 3. 심화

### 3.1 BlendSpace

2D / 1D 입력에 따라 N개 클립을 블렌딩.
- `speed × direction` → walk/run/strafe 자동 합성
- `aim_yaw × aim_pitch` → aim offset

UE5: `Source/Runtime/Engine/Classes/Animation/BlendSpace.h`.

### 3.2 IK (Inverse Kinematics)

**FABRIK** — 발 끝 위치 주면 다리 본을 역산.
**TwoBone** — 어깨-팔꿈치-손 같은 3본 chain.
**FullBodyIK** — UE5 5.x 추가, 다관절 동시.

`Source/Runtime/AnimGraphRuntime/Public/BoneControllers/AnimNode_*IK*.h`.

GTA6의 발이 계단에 정확히 닿는 것 = FootIK + scene query.

### 3.3 Motion Matching

엘든링/GTA6 급 동작 자연스러움의 원천.

원리:
1. 수천 개 모션 클립을 DB에 통째로 박음
2. 매 프레임 입력 (캐릭 속도, 방향, 가속도, 다음 1초 trajectory) 계산
3. DB에서 가장 잘 맞는 frame을 cosine similarity로 찾음
4. 그 frame부터 N frame 재생, 다시 1번

UE5: `Source/Runtime/Engine/Public/Animation/AnimPoseSearchProvider.h`, `Plugins/Animation/PoseSearch/`.

장점: 상태 전이를 일일이 설계 안 해도 자연스러움. 단점: motion DB가 GB 단위 → Ch3 streaming 필수.

### 3.4 Anim Inertialization

상태 전이 시 lerp 대신 **속도/가속도까지 보존하는 spring blend**. 격투/스포츠 게임 필수.

`Source/Runtime/AnimGraphRuntime/Public/AnimNodes/AnimNode_Inertialization.h`.

### 3.5 Physics-driven Animation

- Ragdoll: 죽을 때 본을 rigid body로 전환
- Active ragdoll: 살아 있는 동안에도 hit reaction을 physics로 (소울 시리즈, GTA6)
- Anim physics solver: `Source/Runtime/Engine/Public/Animation/AnimPhysicsSolver.h`

### 3.6 멀티스레드 / Worker eval

AnimGraph evaluation은 워커 스레드에서. 게임 스레드와 분리.

`NativeThreadSafeUpdateAnimation`만 워커. 데이터 race에 매우 엄격.

---

## 4. Winters 매핑

### 4.1 현재 상태

```cpp
// Engine/Public/Resource/Animator.h   (요약)
class CAnimator
{
    void Play(const char* clipName, bool loop);
    void Update(f32_t deltaTime);
    Matrix GetBoneTransform(u32_t boneIdx) const;
};
```

각 챔프 `_Skills.cpp`에 `castFrame == 18`처럼 박혀 있음. → 데이터화 필수.

### 4.2 Ch4 추가 헤더 (제안)

```cpp
// Engine/Public/Animation/AnimNode.h
struct AnimUpdateContext { f32_t deltaTime; ... };
struct AnimPoseContext   { Matrix* boneTransforms; u32_t boneCount; ... };

class WINTERS_ENGINE IAnimNode
{
public:
    virtual ~IAnimNode() = default;
    virtual void Update  (const AnimUpdateContext& ctx) = 0;
    virtual void Evaluate(AnimPoseContext& out) = 0;
};

// Engine/Public/Animation/AnimNode_StateMachine.h
class WINTERS_ENGINE CAnimNode_StateMachine : public IAnimNode { ... };

// Engine/Public/Animation/AnimMontage.h
struct MontageSection
{
    char  name[32];
    f32_t startTime;
    f32_t endTime;
    char  nextSection[32];
};

struct MontageNotify
{
    f32_t  time;
    char   type[16];   // "FxCue" "GameplayEvent" "AudioCue"
    char   payload[64];
};

class WINTERS_ENGINE CAnimMontage
{
    std::vector<MontageSection> sections;
    std::vector<MontageNotify>  notifies;
    AnimSequenceRef sequence;
};
```

### 4.3 castFrame 숫자 → 데이터로

현재 `Client/Private/GameObject/Champion/Ezreal/Ezreal_Skills.cpp` 같은 곳에 박혀 있는 `if (currentFrame == 18) FireQ();`를 **montage notify**로 옮긴다.

```text
[1차]   .ability에 castFrame 숫자만 적기 (코드에서 제거)
[2차]   .anim 파일에 notify 트랙 추가 → animation tool에서 편집
[3차]   서버가 actionSeq → 클라 PlayMontage → notify가 FxCue/Damage 호출
        Damage는 서버 권위, FxCue는 클라 visual only
```

### 4.4 Bot AI 불변식 재확인

- AI는 **montage를 직접 재생하지 않는다.** AI → `GameCommand("CastQ")` → server `AbilitySystem::TryActivate` → 성공 시 actionSeq broadcast → 클라가 montage 재생.
- AI는 montage notify를 모른다. notify는 클라 visual / 서버 damage tick.

### 4.5 단계별

```text
Ch4-Stage1  Animator를 IAnimNode 그래프로 리팩터 (현재 Animator는 1 노드)
Ch4-Stage2  StateMachine 노드 + transition condition
Ch4-Stage3  Montage + Section + Notify (데이터 우선)
Ch4-Stage4  BlendSpace (2D 입력 → 클립 블렌딩)
Ch4-Stage5  LayeredBoneBlend (상체/하체 분리)
Ch4-Stage6  IK (FootIK, LookAt)
Ch4-Stage7  Inertialization (스무딩)
Ch4-Stage8  Motion Matching (Ch3 streaming 의존)
Ch4-Stage9  Physics-driven (Ch5 Physics 의존)
```

### 4.6 게임별 적용

| 게임 | 필요 Stage |
|------|-----------|
| LoL (현재) | Stage 1, 3, 5 (StateMachine + Montage + LayeredBlend) |
| 로아 PvE | Stage 1~7 |
| 엘든링 | Stage 1~9 |
| GTA6 | Stage 1~9 + 대화 lipsync + crowd anim variety |

---

## 5. 검증 명령

```powershell
# Animation graph dump
.\Client\Bin\Debug-DX12\WintersGame.exe --anim-dump-graph=Irelia

# 기대 출력
# [Anim] Irelia AnimInstance:
#   Root → StateMachine
#     ├── Locomotion (BlendSpace: speed × yaw)
#     ├── Combat (Montage slot)
#     └── Dead (Sequence)
#   Layered → UpperBody slot (Montage)
#   IK → FootIK (FABRIK), LookAt (TwoBone)
```

---

## 6. 다음 챕터로

Ch4 Stage 3까지 가야 **Ch6 Audio** notify와 **Ch8 GAS** GameplayCue가 같은 타임라인에서 trigger된다. Ch5 Physics 없으면 Ragdoll/Cloth는 못 한다.
