# Stage 5 — Constraint Solver (Sequential Impulse)

## 목표

**충돌 응답 + 조인트** 를 통일된 제약 조건 풀이로 처리. Erin Catto "Sequential Impulse" 방식
(Box2D/Bullet 기본).

## 왜 Constraint Solver 인가

- Stage 4 Rigid Body 만으론 물체끼리 겹침 반발/정지 불가
- 고전 Penalty Method (스프링-댐퍼) 는 큰 강성에서 불안정
- Constraint 는 **정확히 관통=0** 을 보장 (rigid 접촉), 스태킹 가능

## Sequential Impulse 개요

각 제약 조건을 반복적으로 해결:
```
for iteration in 1..N:
    for each constraint:
        J = Jacobian
        effectiveMass = 1 / (J × M⁻¹ × J^T)
        lambda = -J × v / effectiveMass + bias
        Δv = M⁻¹ × J^T × lambda
        v += Δv
```

## 제약 조건 종류

### 1. Contact Constraint (가장 중요)

두 물체가 겹침 → 법선 방향 속도 제거 + 반발/마찰.

```cpp
class CContactConstraint : public IConstraint
{
public:
    EntityID    bodyA, bodyB;
    Vec3        contactPoint;
    Vec3        normal;
    f32_t       penetration;
    f32_t       restitution;
    f32_t       friction;

    // Warm starting — 이전 프레임의 impulse 재사용으로 수렴 가속
    f32_t       normalImpulseAccum   = 0.f;
    f32_t       tangentImpulseAccum1 = 0.f;
    f32_t       tangentImpulseAccum2 = 0.f;

    void SolveVelocityConstraint(f32_t dt) override;
    void SolvePositionConstraint(f32_t dt) override;   // Position correction
};
```

#### Velocity Solve (법선)

```cpp
void SolveNormal(RigidBody& A, RigidBody& B)
{
    Vec3 rA = contactPoint - A.position;
    Vec3 rB = contactPoint - B.position;
    Vec3 vA = A.linearVel + Cross(A.angularVel, rA);
    Vec3 vB = B.linearVel + Cross(B.angularVel, rB);
    f32_t relVelN = Dot(vB - vA, normal);

    // Effective Mass
    f32_t eMass = A.invMass + B.invMass
                + Dot(Cross(A.invInertia * Cross(rA, normal), rA), normal)
                + Dot(Cross(B.invInertia * Cross(rB, normal), rB), normal);

    // Bias for position correction (Baumgarte 또는 Split Impulse)
    f32_t bias = -0.2f * std::max(penetration - 0.01f, 0.f) / dt;

    f32_t lambda = -(relVelN * (1 + restitution) + bias) / eMass;

    // Clamp: 인장력 방지 (법선은 밀어내는 힘만)
    f32_t oldImpulse = normalImpulseAccum;
    normalImpulseAccum = std::max(oldImpulse + lambda, 0.f);
    lambda = normalImpulseAccum - oldImpulse;

    Vec3 impulse = normal * lambda;
    A.linearVel  -= impulse * A.invMass;
    A.angularVel -= A.invInertia * Cross(rA, impulse);
    B.linearVel  += impulse * B.invMass;
    B.angularVel += B.invInertia * Cross(rB, impulse);
}
```

#### Velocity Solve (마찰)

접선 방향 2축 (tangent1, tangent2) 에 대해 Coulomb 마찰:
```
|tangent impulse| <= friction × |normal impulse|
```

### 2. Distance Constraint

두 물체의 특정 점 간 거리 고정.

```cpp
class CDistanceConstraint : public IConstraint
{
public:
    Vec3    localAnchorA, localAnchorB;
    f32_t   targetDistance;
    // Jacobian = [-u, -(rA × u), u, (rB × u)]  where u = (pB - pA) / |pB - pA|
};
```

용례: 매달린 상자, 체인.

### 3. Hinge (Revolute) Constraint

한 축만 회전 자유, 나머지 고정. 문/팔꿈치.

```cpp
class CHingeConstraint : public IConstraint
{
public:
    Vec3    localAnchorA, localAnchorB;
    Vec3    localAxisA;
    f32_t   lowerAngle, upperAngle;   // 범위 제한 (선택)
    bool_t  hasMotor;
    f32_t   motorSpeed, motorMaxTorque;
};
```

### 4. Slider (Prismatic) Constraint

한 축만 이동 자유, 회전은 전부 고정. 엘리베이터.

### 5. Fixed (Weld) Constraint

모든 자유도 고정. 두 물체 붙이기 (파편 조립).

### 6. Ball-Socket (Point) Constraint

3D 볼조인트. 3축 회전 자유, 위치는 고정.

## Iterative Solver (풀이)

```cpp
class CSequentialImpulseSolver
{
public:
    void Solve(std::vector<unique_ptr<IConstraint>>& constraints,
               std::vector<ContactManifold>& contacts,
               f32_t dt,
               i32_t velocityIters = 8,
               i32_t positionIters = 3)
    {
        // Warm Starting
        for (auto& c : contacts) c.ApplyAccumulatedImpulse();

        // Velocity iterations
        for (i32_t i = 0; i < velocityIters; ++i) {
            for (auto& c : contacts)      c.SolveVelocityConstraint(dt);
            for (auto& jc : constraints) jc->SolveVelocityConstraint(dt);
        }

        // Integrate velocity → position
        IntegratePositions(dt);

        // Position Correction (Split Impulse 방식)
        for (i32_t i = 0; i < positionIters; ++i) {
            for (auto& c : contacts)      c.SolvePositionConstraint(dt);
            for (auto& jc : constraints) jc->SolvePositionConstraint(dt);
        }
    }
};
```

## Island System

독립 물체 그룹으로 분할 → 병렬화 + Sleep 지원:

```cpp
struct Island
{
    std::vector<EntityID>              bodies;
    std::vector<ContactManifold*>      contacts;
    std::vector<IConstraint*>          constraints;
};

class CIslandManager
{
public:
    // Union-Find 로 연결된 바디 그룹화
    std::vector<Island> BuildIslands(CWorld& world);

    // 모든 바디가 sleep 임계 이하면 island 전체 sleep
    void CheckIslandSleep(Island& island);
};
```

각 Island 는 독립 → Phase 1b JobSystem 병렬 처리.

## Position Correction 방식 비교

### Baumgarte Stabilization

```
bias = (β / dt) × penetration
```

간단하지만 에너지 주입 → 튕김 불안정.

### Split Impulse (★ 권장)

속도 solver 와 position solver 분리. 속도는 순수 dynamics, 위치는 별도 pseudo-velocity 로 보정.

```cpp
// 위치 보정은 실제 velocity 에 영향 X
void SolvePositionConstraint(f32_t dt)
{
    Vec3 rA = contactPoint - A.position;
    Vec3 rB = contactPoint - B.position;
    f32_t sep = penetration;

    f32_t correction = std::clamp(0.2f * (sep - 0.005f), 0.f, 0.2f);
    // Pseudo velocity 로만 보정 — 실제 v 는 그대로
    Vec3 impulse = normal * correction;
    A.positionCorrection -= impulse * A.invMass;
    B.positionCorrection += impulse * B.invMass;
}
```

## Warm Starting

이전 프레임 impulse 를 캐시해서 초기값으로:
- Stacking 수렴 속도 10배 이상 향상
- Contact cache 는 ContactPoint 식별자 (position hash) 로 매칭

## 성능 튜닝

- **Velocity iterations**: 8 권장 (스택 안정)
- **Position iterations**: 3 권장
- 과도하면 성능 ↓, 부족하면 스택 덜덜 떨림
- Island 병렬화로 멀티코어 활용

## 디버그

- ImGui 에 iteration 별 residual (수렴도) 그래프
- DebugDraw 로 접촉점 빨강 화살표 (법선)
- 각 제약 조건의 현재 impulse 크기
- Island 색상 오버레이 (Union-Find 결과)

## 단위 테스트

```cpp
TEST(Constraint, TwoBoxesStackedRest)
{
    // 박스 2개를 쌓음 → 10초 후 완전 정지 예상
    auto& world = CreateTestWorld();
    auto top = CreateBox(world, {0, 2, 0}, 1.f);
    auto bot = CreateBox(world, {0, 0, 0}, 1.f);

    for (i32_t i = 0; i < 600; ++i)
        world.Step(1.f / 60.f);

    auto& topRB = world.GetComponent<RigidBodyComponent>(top);
    EXPECT_LT(Length(topRB.linearVelocity), 0.01f);
    EXPECT_LT(Length(topRB.angularVelocity), 0.01f);
}

TEST(Constraint, DistanceJoint)
{
    // 두 점 고정 거리 2m 유지
    auto a = CreateBox(world, {0, 5, 0}, 1.f);
    auto b = CreateBox(world, {3, 5, 0}, 1.f);
    AddDistanceConstraint(world, a, b, 2.f);

    for (i32_t i = 0; i < 120; ++i) world.Step(1.f / 60.f);

    f32_t d = Distance(GetPos(a), GetPos(b));
    EXPECT_NEAR(d, 2.f, 0.05f);
}
```

## 구현 순서

1. `IConstraint` 인터페이스 + `ContactConstraint` 기본
2. Velocity Solve (법선만) — 간단 반발
3. Warm Starting + Accumulated Impulse
4. 마찰 (접선 2축)
5. Position Correction (Split Impulse)
6. Island System (Union-Find)
7. `DistanceConstraint` / `HingeConstraint`
8. 성능 최적화 (캐시 친화 배열)
9. ImGui 튜너 (iteration 슬라이더)
