# Stage 7 — CCD (Continuous Collision Detection)

## 목표

빠른 물체의 **프레임 간 관통 방지**. Discrete Collision 은 1 프레임에 30m 이동하는 투사체가
벽을 통과시킴.

## 문제 시나리오 (LoL 모작 특화)

- **루시안 R** — 0.1 초 안에 여러 투사체 연사, 각 투사체 속도 5000
- **애쉬 R** — 글로벌 화살, 속도 1600, 얇은 챔피언 관통 가능
- **카직스 E** — 빠른 점프로 벽 통과 위험
- 일반 평타 투사체도 빠를수록 안전하지 않음

Discrete CD 는 각 프레임 **시작 위치** 에서만 겹침 검사 → 프레임 사이 공간 못 봄.

## CCD 방식

### 1. Sweep Test (★ 권장)

물체의 이전 위치 → 현재 위치를 "스윕 볼륨" 으로 확장:

```
Capsule(p_prev, p_curr, radius) vs 정적 콜라이더
```

투사체 (Sphere 또는 작은 Capsule) 가 한 프레임에 그린 궤적이 바로 Capsule. 이걸 모든 정적 콜라이더와 교차 테스트.

```cpp
struct SweptSphere {
    Vec3  startPos, endPos;
    f32_t radius;
};

bool SweptSphere_vs_AABB(const SweptSphere& sphere, const AABB& aabb, f32_t& tHit);
bool SweptSphere_vs_Sphere(const SweptSphere& sphere, const Sphere& s, f32_t& tHit);
```

### 2. TOI (Time of Impact) — 정밀

이분 탐색으로 **첫 접촉 시간** 계산. Box2D 에서 사용.

```cpp
// [0, 1] 구간에서 A 와 B 가 처음 접촉하는 시간 t* 찾기
struct TOIInput {
    RigidBody    bodyA, bodyB;      // position + velocity
    f32_t        tMax = 1.f;
    f32_t        tolerance = 0.001f;
};

struct TOIOutput {
    f32_t  t;                        // [0, 1]
    bool_t bTouching;
    bool_t bFailed;
    Vec3   contactPoint;
    Vec3   contactNormal;
};

TOIOutput ComputeTOI(const TOIInput& in);
```

알고리즘 (Box2D):
1. 두 물체의 separation 함수 f(t) = signed distance (at time t)
2. f(0) < 0 면 이미 관통 → t=0
3. f(1) > 0 면 분리 유지 → 충돌 X
4. 이분 탐색 또는 root finding 으로 f(t) = 0 인 t 찾기

Newton-Raphson 또는 Brent's method 사용 가능.

### 3. Conservative Advancement

TOI 의 단순화 버전. 매 iteration:
```
Δt = signedDistance / relativeSpeed
t += Δt
if t > 1: no collision
if Δt < ε: found TOI
```

## 엔진 통합

### Bullet 대상 시스템

모든 rigid body 를 CCD 대상으로 하면 너무 비쌈. **속도 임계** 초과하는 물체만:

```cpp
struct CCDComponent {
    bool_t  bEnabled        = true;
    f32_t   velocityThreshold = 50.f;    // m/s
    f32_t   ccdSweptSphereRadius;        // AABB 대각선 × 0.5 권장
};
```

### 실행 순서

```
1. Integrate positions (tentative)
2. For each CCD body:
     if speed < threshold: skip
     TOI = ComputeTOI(body.prev, body.tentative, static_world)
     if TOI < 1:
         body.position = interpolate(prev, tentative, TOI)
         resolve contact at TOI
         추가 residual motion 반영 (sub-stepping)
3. Static collision detection on final positions
```

## MOBA 투사체 전용 간소화

LoL 스킬샷은 대부분:
- 직선 이동
- 시전자 vs 적 (팀 분류)
- 히트박스는 원기둥

간단한 레이캐스트 변형으로 충분:

```cpp
// 스킬샷 매 프레임 처리
void UpdateProjectile(ProjectileComponent& p, f32_t dt)
{
    Vec3 prevPos = p.position;
    Vec3 nextPos = p.position + p.direction * p.speed * dt;

    // 세그먼트 + 반지름 → Capsule vs 모든 적 Capsule
    Capsule sweep{prevPos, nextPos, p.hitRadius};
    for (EntityID enemy : GetEnemyList()) {
        if (Overlap(sweep, GetCapsule(enemy))) {
            // 가장 빠른 교차점 선택
            OnHit(p, enemy);
            break;
        }
    }
    p.position = nextPos;
}
```

이게 사실상 CCD 의 게임 특화 구현.

## 공격 Hitbox 스윕 (Phase C-3 Socket 연동)

이전 프레임 본 월드 행렬 → 현재 프레임 본 월드 행렬 → Hitbox 캡슐 스윕.

```cpp
// HitDetectionSystem
for each active Hitbox:
    prevSocket = boneWorldMatrix_lastFrame × localOffset
    currSocket = boneWorldMatrix_thisFrame × localOffset
    SweptCapsule hitSweep{
        prevSocket.Translation(),
        currSocket.Translation(),
        hitbox.radius
    };
    foreach Hurtbox within broad phase:
        if (Overlap(hitSweep, hurtboxCapsule))
            EventBus::Publish(CollisionEvent{...});
```

이게 "이렐리아 Q 가 플레이어를 관통하지 않게" 의 핵심.

## Multi-step CCD (복잡)

한 프레임에 여러 TOI 발생 가능 (예: 쏜 화살이 여러 적에게 순차 관통):

```
t = 0
while t < 1:
    toi = ComputeTOI_NextContact(body, t_start=t, t_end=1)
    if toi >= 1: break
    resolve contact at toi
    if pierce: continue
    if not pierce: 
        body.velocity.reflect(normal)
        t = toi
break
```

## 단위 테스트

```cpp
TEST(CCD, FastBulletThroughWall)
{
    // 속도 10000 의 구가 얇은 벽 (두께 0.1) 을 통과하려 함
    auto bullet = CreateSphere({0, 1, 0}, 0.1f, 10000.f /* vel along +X */);
    auto wall   = CreateStaticBox({5, 1, 0}, {0.05, 1, 1});

    world.EnableCCD(bullet);
    world.Step(1.f / 60.f);

    // 총알이 벽 앞에 멈춰야 함 (관통 X)
    EXPECT_LT(GetPosition(bullet).x, 5.f);
}

TEST(CCD, FastBulletNoCCD_Fails)
{
    // CCD 꺼놓으면 벽 통과
    auto bullet = CreateSphere({0, 1, 0}, 0.1f, 10000.f);
    auto wall   = CreateStaticBox({5, 1, 0}, {0.05, 1, 1});

    world.DisableCCD(bullet);
    world.Step(1.f / 60.f);

    // 관통 발생 (일반 관통 버그 재현)
    EXPECT_GT(GetPosition(bullet).x, 5.f);
}
```

## 성능

- CCD 는 Discrete 대비 5~10배 비쌈 → 필요한 물체만
- BroadPhase Tree 를 sweep 의 확장 AABB 로 쿼리 (sweep AABB = min/max of prev/curr positions + 반지름)
- 매 프레임 TOI 는 투사체 정도만 허용

## 구현 순서

1. `SweptSphere` + `SweptCapsule` 교차 함수 (vs AABB/Sphere/Capsule)
2. `Capsule vs Capsule Sweep` — Hitbox 판정 핵심
3. 투사체 ProjectileSystem 간단 버전
4. Hitbox 스윕 (Socket 연동) — Phase C-3 동반
5. `CCDComponent` + 속도 임계 스킵
6. 정밀 TOI (Box2D 방식) — 고속 강체용
7. Sub-stepping 반복 (multi-step TOI)
8. CCD on/off 디버그 토글
