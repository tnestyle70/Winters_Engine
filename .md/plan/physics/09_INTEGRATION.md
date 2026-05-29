# Physics 통합 — Socket / Hitbox / Hurtbox / AnimationEvent

## 목표

Stage 1~7 물리 모듈을 **Phase C-3 전투 판정 시스템** 과 연결. MOBA 피해 처리의 핵심.

## 전체 흐름

```
[애니메이션 Update]
      ↓
[Skeleton 본 월드 행렬 갱신]
      ↓
[SocketSystem 이 본 소켓 → 콜라이더 월드 좌표 계산]
      ↓
[AnimationEvent 가 Hitbox active/inactive 토글]
      ↓
[BroadPhaseSystem → NarrowPhaseSystem → CollisionEvent 발행]
      ↓
[HitDetectionSystem 이 Hitbox↔Hurtbox 페어 처리]
      ↓
[DamageSystem 이 실제 피해 계산 + DamageFont 생성]
```

## 컴포넌트 연결도

```
Entity: Player Champion
├── TransformComponent
├── ColliderComponent (Capsule — body hurtbox)
├── HurtboxComponent
├── SkeletonComponent
├── AnimatorComponent
└── [자식 엔티티] WeaponEntity
    ├── TransformComponent (socket 시스템이 갱신)
    ├── SocketComponent { parentEntity, boneIdx, localOffset }
    ├── ColliderComponent (Capsule — blade)
    ├── HitboxComponent { damage, active, layerMask }
    └── (옵션) MeshRenderer
```

## Socket System

매 프레임 본 월드 행렬 × 로컬 오프셋 → 소켓 월드 트랜스폼.

```cpp
// Engine/Public/ECS/Systems/SocketSystem.h
class CSocketSystem : public ISystem
{
public:
    void Execute(CWorld& world, f32_t dt) override
    {
        world.ForEach<SocketComponent, TransformComponent>(
            [&](EntityID e, SocketComponent& s, TransformComponent& t)
        {
            auto& skel = world.GetComponent<SkeletonComponent>(s.parentEntity);
            if (s.boneIdx >= skel.boneWorldMatrices.size()) return;

            Mat4 boneWorld = skel.boneWorldMatrices[s.boneIdx];
            Mat4 socketWorld = boneWorld * s.localOffset;

            t.SetMatrix(socketWorld);
            t.SetDirty();   // BroadPhase 갱신 트리거
        });
    }
};
```

**주의**: Socket 은 TransformSystem **이후** 실행돼야 함. SystemScheduler Phase 에서 순서 지정.

## Hitbox / Hurtbox

```cpp
enum class LayerMaskBits : u32_t {
    Hitbox_Player   = 1 << 0,
    Hurtbox_Player  = 1 << 1,
    Hitbox_Enemy    = 1 << 2,
    Hurtbox_Enemy   = 1 << 3,
    Hitbox_Neutral  = 1 << 4,   // 중립 몹 공격
    Hurtbox_Neutral = 1 << 5,
    Structure       = 1 << 6,
};

struct HitboxComponent {
    bool_t       active = false;           // AnimationEvent 로 토글
    f32_t        damage = 0.f;
    DamageType   damageType = DamageType::Physical;
    u32_t        layerMask;                // 대상 허트박스 필터
    EntityID     owner;                    // 소유자 (자기 자신 안 맞게)
    
    // 1 틱당 한 번만 히트
    std::unordered_set<EntityID> alreadyHit;
    
    // Pierce 설정
    bool_t       bPierce = false;
    u32_t        maxTargets = 1;
    
    // Knockback
    f32_t        knockbackForce = 0.f;
    Vec3         knockbackDir;             // 월드 또는 자기 forward 기준
};

struct HurtboxComponent {
    EntityID     owner;
    f32_t        damageMultiplier = 1.f;  // 머리 = 2.0 (치명타)
    u32_t        layerMask;
};
```

## AnimationEvent 시스템

`.wanim` 파일의 이벤트 트랙을 런타임에 디스패치.

```cpp
enum class AnimationEventType : u8_t {
    HitStart,
    HitEnd,
    Footstep,
    SFX,
    VFX_Spawn,
    DamageNumber,
    WeaponTrailStart,
    WeaponTrailEnd,
    Invulnerable_Begin,
    Invulnerable_End,
    Custom
};

struct AnimationEvent {
    f32_t                triggerTime;    // 애니 내부 시간 (0 ~ duration)
    AnimationEventType   type;
    std::string          payload;        // JSON/문자열 (skill slot, sfx id 등)
};

// AnimationComponent 에 이벤트 트랙 포함
struct AnimationComponent {
    u32_t currentAnimID;
    f32_t currentTime;
    f32_t prevTime;         // 이벤트 발화 판정용
    std::vector<AnimationEvent> events;   // 해당 애니의 이벤트 리스트
};
```

### Dispatch 시스템

```cpp
class CAnimationEventSystem : public ISystem
{
public:
    void Execute(CWorld& world, f32_t dt) override
    {
        world.ForEach<AnimationComponent>(
            [&](EntityID e, AnimationComponent& a)
        {
            for (const auto& ev : a.events) {
                // prevTime < triggerTime <= currentTime 사이 발화
                bool triggered = (a.prevTime < ev.triggerTime) && (ev.triggerTime <= a.currentTime);
                if (a.currentTime < a.prevTime) {   // 루프 wrap 처리
                    triggered = (ev.triggerTime > a.prevTime) || (ev.triggerTime <= a.currentTime);
                }
                if (triggered) {
                    DispatchEvent(world, e, ev);
                }
            }
            a.prevTime = a.currentTime;
        });
    }

private:
    void DispatchEvent(CWorld& world, EntityID e, const AnimationEvent& ev)
    {
        switch (ev.type) {
            case AnimationEventType::HitStart:
                ActivateHitbox(world, e, ev.payload);
                break;
            case AnimationEventType::HitEnd:
                DeactivateHitbox(world, e, ev.payload);
                break;
            case AnimationEventType::Footstep:
                PlayFootstepSFX(world, e);
                break;
            // ...
        }
    }

    void ActivateHitbox(CWorld& world, EntityID e, const std::string& skillSlot);
    void DeactivateHitbox(CWorld& world, EntityID e, const std::string& skillSlot);
};
```

## HitDetection System

BroadPhase + NarrowPhase 가 발행한 CollisionEvent 를 소비:

```cpp
class CHitDetectionSystem : public ISystem
{
public:
    void OnCreate(CWorld& world) override
    {
        EventBus::Subscribe<CollisionEvent>(
            [this, &world](const CollisionEvent& ev) { OnCollision(world, ev); });
    }

    void OnCollision(CWorld& world, const CollisionEvent& ev)
    {
        // Hitbox + Hurtbox 페어만 처리
        bool aIsHit = world.HasComponent<HitboxComponent>(ev.entityA);
        bool bIsHit = world.HasComponent<HitboxComponent>(ev.entityB);
        bool aIsHurt = world.HasComponent<HurtboxComponent>(ev.entityA);
        bool bIsHurt = world.HasComponent<HurtboxComponent>(ev.entityB);

        if (aIsHit && bIsHurt)      HandleHit(world, ev.entityA, ev.entityB, ev);
        else if (bIsHit && aIsHurt) HandleHit(world, ev.entityB, ev.entityA, ev);
    }

private:
    void HandleHit(CWorld& world, EntityID hitboxE, EntityID hurtboxE, const CollisionEvent& ev)
    {
        auto& hitbox  = world.GetComponent<HitboxComponent>(hitboxE);
        auto& hurtbox = world.GetComponent<HurtboxComponent>(hurtboxE);

        if (!hitbox.active) return;
        if (hitbox.owner == hurtbox.owner) return;   // 자해 방지
        if (hitbox.alreadyHit.count(hurtboxE)) return;

        // 레이어 체크
        if ((hitbox.layerMask & hurtbox.layerMask) == 0) return;

        // 피해 계산
        f32_t finalDamage = hitbox.damage * hurtbox.damageMultiplier;
        
        // 이벤트 발행
        EventBus::Publish<DamageEvent>({
            .source = hitbox.owner,
            .target = hurtbox.owner,
            .amount = finalDamage,
            .type   = hitbox.damageType,
            .point  = ev.contactPoint,
        });

        // 히트 기록 (중복 방지)
        hitbox.alreadyHit.insert(hurtboxE);

        // Pierce 체크
        if (!hitbox.bPierce || hitbox.alreadyHit.size() >= hitbox.maxTargets) {
            hitbox.active = false;
        }

        // Knockback
        if (hitbox.knockbackForce > 0.f) {
            auto& rb = world.GetComponent<RigidBodyComponent>(hurtbox.owner);
            rb.ApplyImpulse(hitbox.knockbackDir * hitbox.knockbackForce);
        }
    }
};
```

## DamageSystem

DamageEvent 를 받아 실제 HP 차감 + 데미지 폰트 스폰:

```cpp
class CDamageSystem : public ISystem
{
public:
    void OnCreate(CWorld& world) override
    {
        EventBus::Subscribe<DamageEvent>(
            [this, &world](const DamageEvent& ev) { ApplyDamage(world, ev); });
    }

private:
    void ApplyDamage(CWorld& world, const DamageEvent& ev)
    {
        auto& hp = world.GetComponent<HealthComponent>(ev.target);
        f32_t mitigated = CalculateMitigated(hp, ev.type, ev.amount);
        hp.current -= mitigated;

        // Phase C-2 DamageFont 스폰
        DamageFontRenderer::Spawn(ev.point, mitigated, GetDamageColor(ev.type), 1.5f);

        // 사망 체크
        if (hp.current <= 0.f) {
            EventBus::Publish<DeathEvent>({.entity = ev.target, .killer = ev.source});
        }
    }
};
```

## 이렐리아 Q 전체 예시

```
애니: irelia_q (0.5초 모션)
이벤트 트랙:
  t=0.00  HitStart { slot: "Q", damage: 80, hitbox: "Blade_Sweep" }
  t=0.15  VFX_Spawn { id: "q_slash_fx", attach: "RightHand" }
  t=0.25  SFX { id: "irelia_q_hit" }
  t=0.35  HitEnd { slot: "Q" }

Hitbox 엔티티 설정:
  SocketComponent{ parentEntity=ireliaE, boneIdx=BONE_BLADE_END, localOffset=I }
  ColliderComponent{ Capsule, radius=0.5m, half-length=1.5m }
  HitboxComponent{ owner=ireliaE, damage=80, active=false (초기), pierce=false,
                   layerMask=Hurtbox_Enemy }

런타임 흐름:
  t=0.00  HitStart → hitbox.active = true
  매 프레임  SocketSystem 이 Blade_End 본 좌표 추적
           BroadPhase 가 Hitbox AABB 갱신 → 적 Hurtbox 겹침 감지
           HitDetection 이 DamageEvent 발행
           DamageSystem 이 HP 차감 + 데미지 숫자 3D 빌보드
  t=0.35  HitEnd → hitbox.active = false
           (alreadyHit 리셋은 다음 Q 시전 시)
```

## CCD 연결

빠른 공격 (Yasuo Q3 회오리, 검기 투사체) 은 Hitbox 에 CCD 활성화:

```cpp
// Yasuo Q3 is a projectile with CCD
entity.AddComponent(HitboxComponent{...});
entity.AddComponent(CCDComponent{.bEnabled = true, .velocityThreshold = 0.f}); // 항상 CCD
```

`HitDetectionSystem` 은 CCD 를 거친 swept overlap 결과도 동일하게 처리.

## 멀티 Hitbox (파트별)

한 스킬이 여러 Hitbox (시전자 검 + 회오리 범위):

```
SylasUltimate  ← 플레이어 챔피언
├── Hitbox_PrimarySlash    SocketComponent{bone=RightHand}
├── Hitbox_SecondaryRadius SocketComponent{bone=Root}
└── Hitbox_GroundPulse     SocketComponent{bone=Root, offset={0,-0.5,0}}
```

각 Hitbox 는 독립 엔티티. AnimationEvent 가 각각 활성/비활성.

## 멀티 Hurtbox

챔피언 바디 전체 + 머리 전용:

```
Irelia Champion
├── HurtboxComponent { bodyPart = Body, multiplier = 1.0 }  ← 기본 바디 Capsule
└── [자식 엔티티]
    └── Hurtbox_Head { multiplier = 2.0 }  ← 머리 작은 Sphere
```

치명타 판정, 부위 피해 시스템 확장 여지.

## 디버그

- Hitbox 활성 상태 시각화 (초록 와이어, 비활성이면 회색)
- Hurtbox 파랑
- 최근 N 프레임 내 히트 이벤트 로그 (ImGui 창)
- 애니메이션 이벤트 타임라인 (현재 재생 위치 커서)

## 구현 순서

1. `SocketComponent` + `CSocketSystem` (Phase C-3 시작)
2. `HitboxComponent`, `HurtboxComponent`
3. `AnimationEvent` + 이벤트 트랙 파일 포맷
4. `CAnimationEventSystem` (prev/cur 시간 기반 dispatch)
5. `CHitDetectionSystem` (CollisionEvent 구독)
6. `DamageEvent` + `CDamageSystem`
7. Stage 7 CCD 와 연동 (고속 공격)
8. 이렐리아 Q 샘플 데모 완성
9. 멀티 Hitbox/Hurtbox (머리 크리티컬)
