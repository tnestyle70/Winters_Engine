# Stage 4 — Rigid Body Dynamics

## 목표

힘/토크 적분 → 선/각 속도 → 위치/회전 갱신. 기본 뉴튼-오일러 방정식.

## 왜 Rigid Body 인가

MOBA 에선 제한적 사용:
- 파편, 시체 물리 연출
- 오브젝트 넉백 반응
- 부서지는 탑/억제기 파편

엘든링 모작 (Phase B) 에선 훨씬 중요:
- 적 사체 래그돌
- 무기 스위프 물리
- 환경 오브젝트 (의자/박스/횃불)

## 운동 방정식

### 선형 운동 (뉴턴 제2법칙)

```
F = m × a
a = F / m
v_new = v_old + a × dt
x_new = x_old + v_new × dt      ← Semi-implicit Euler
```

### 각 운동 (오일러 방정식)

```
τ = I × α + ω × (I × ω)
α = I⁻¹ × (τ - ω × (I × ω))
ω_new = ω_old + α × dt
q_new = q_old + (ω_new × q_old) × 0.5 × dt   ← Quaternion 적분
```

τ: 토크, I: 관성 텐서 (3x3), ω: 각속도, α: 각가속도, q: 쿼터니언.

## 컴포넌트

```cpp
struct RigidBodyComponent {
    enum class Type : u8_t { Dynamic, Kinematic, Static };
    Type        type = Type::Dynamic;

    f32_t       mass           = 1.f;
    f32_t       invMass        = 1.f;          // Static 이면 0
    Mat3        invInertiaLocal;                // Shape 로부터 계산
    Mat3        invInertiaWorld;                // 매 프레임 R × I⁻¹_local × R^T

    Vec3        linearVelocity   = Vec3::Zero();
    Vec3        angularVelocity  = Vec3::Zero();
    Vec3        accumForce       = Vec3::Zero();
    Vec3        accumTorque      = Vec3::Zero();

    f32_t       linearDamping    = 0.01f;
    f32_t       angularDamping   = 0.05f;
    f32_t       restitution      = 0.3f;
    f32_t       friction         = 0.5f;

    bool_t      bSleeping = false;
    f32_t       sleepTimer = 0.f;

    // Helper API
    void ApplyForce(const Vec3& force);
    void ApplyForceAtPoint(const Vec3& force, const Vec3& worldPoint);
    void ApplyTorque(const Vec3& torque);
    void ApplyImpulse(const Vec3& impulse);
    void ApplyImpulseAtPoint(const Vec3& impulse, const Vec3& worldPoint);
};
```

## 관성 텐서 계산

Shape 별로 해석적 공식:

```cpp
Mat3 ComputeInertiaTensor_Sphere(f32_t mass, f32_t radius) {
    f32_t i = (2.f / 5.f) * mass * radius * radius;
    return Mat3::Diagonal(i, i, i);
}

Mat3 ComputeInertiaTensor_Box(f32_t mass, const Vec3& halfExtents) {
    f32_t w = halfExtents.x * 2, h = halfExtents.y * 2, d = halfExtents.z * 2;
    return Mat3::Diagonal(
        (1.f / 12.f) * mass * (h*h + d*d),
        (1.f / 12.f) * mass * (w*w + d*d),
        (1.f / 12.f) * mass * (w*w + h*h)
    );
}

Mat3 ComputeInertiaTensor_Capsule(f32_t mass, f32_t radius, f32_t halfLen) {
    // 원기둥 + 반구 2개 → Parallel Axis Theorem
    // ... (생략)
}

Mat3 ComputeInertiaTensor_ConvexMesh(f32_t density, const std::vector<Vec3>& verts,
                                      const std::vector<i32_t>& indices) {
    // Volume 적분 (Tetrahedral decomposition)
    // ... (생략)
}
```

## 적분기 (Integrator)

### Semi-implicit (Symplectic) Euler ★ 기본

안정적이고 간단. 대부분 게임 선택.

```cpp
void Integrate_SemiImplicit(RigidBodyComponent& rb, TransformComponent& t, f32_t dt, const Vec3& gravity)
{
    if (rb.type != RigidBodyComponent::Type::Dynamic) return;

    // 중력 + 누적 힘
    Vec3 totalForce = rb.accumForce + gravity * rb.mass;

    // 선속도 갱신
    rb.linearVelocity += totalForce * rb.invMass * dt;
    rb.linearVelocity *= 1.f / (1.f + rb.linearDamping * dt);   // damping

    // 각속도 갱신
    Vec3 angAccel = rb.invInertiaWorld * rb.accumTorque;
    rb.angularVelocity += angAccel * dt;
    rb.angularVelocity *= 1.f / (1.f + rb.angularDamping * dt);

    // 위치 갱신 (갱신된 속도 사용 → semi-implicit)
    Vec3 newPos = t.GetPosition() + rb.linearVelocity * dt;
    t.SetPosition(newPos);

    // 회전 갱신 (쿼터니언)
    Quat q = t.GetRotationQuat();
    Quat omega = Quat(0, rb.angularVelocity.x, rb.angularVelocity.y, rb.angularVelocity.z);
    q = Normalize(q + omega * q * (0.5f * dt));
    t.SetRotationQuat(q);

    // 누적 초기화
    rb.accumForce  = Vec3::Zero();
    rb.accumTorque = Vec3::Zero();
}
```

### RK4 (선택)

정확도 ↑, 비용 ↑ 4배. 실시간 게임엔 오버킬. 참고용.

## Sleep (절전)

속도 임계 이하 상태가 N 초 지속 → 슬립 모드. 다음 프레임부터 연산 스킵.

```cpp
void CheckSleep(RigidBodyComponent& rb, f32_t dt)
{
    f32_t speedSq = LengthSquared(rb.linearVelocity) + LengthSquared(rb.angularVelocity);
    if (speedSq < SLEEP_THRESHOLD_SQ) {
        rb.sleepTimer += dt;
        if (rb.sleepTimer > 0.5f) {
            rb.bSleeping = true;
            rb.linearVelocity = Vec3::Zero();
            rb.angularVelocity = Vec3::Zero();
        }
    } else {
        rb.sleepTimer = 0.f;
    }
}
```

충돌/힘 인가 시 Wake up.

## Force Generator 패턴

지속적 외력 소스를 추상화:

```cpp
class IForceGenerator
{
public:
    virtual void Apply(RigidBodyComponent& rb, f32_t dt) = 0;
};

class CGravityForce : public IForceGenerator {
    Vec3 m_gravity;
public:
    void Apply(RigidBodyComponent& rb, f32_t dt) override {
        rb.ApplyForce(m_gravity * rb.mass);
    }
};

class CDragForce : public IForceGenerator {
    f32_t m_linearDrag, m_quadraticDrag;
public:
    void Apply(RigidBodyComponent& rb, f32_t dt) override {
        f32_t speed = Length(rb.linearVelocity);
        f32_t dragCoef = m_linearDrag + m_quadraticDrag * speed;
        rb.ApplyForce(-Normalize(rb.linearVelocity) * dragCoef);
    }
};

class CWindForce : public IForceGenerator { /* ... */ };
class CExplosionForce : public IForceGenerator { /* 반경 내 폭발 */ };
```

## Shape → Inertia 자동 계산

ColliderComponent 와 RigidBodyComponent 가 함께 있을 때:

```cpp
void CPhysicsWorld::BakeInertiaTensor(EntityID e, CWorld& world)
{
    auto& col = world.GetComponent<ColliderComponent>(e);
    auto& rb  = world.GetComponent<RigidBodyComponent>(e);

    Mat3 I;
    switch (col.shape) {
        case Shape::Sphere:  I = ComputeInertiaTensor_Sphere(rb.mass, col.sphere.radius); break;
        case Shape::AABB:    I = ComputeInertiaTensor_Box(rb.mass, col.aabb.halfExtents); break;
        case Shape::OBB:     I = ComputeInertiaTensor_Box(rb.mass, col.obb.halfExtents); break;
        case Shape::Capsule: I = ComputeInertiaTensor_Capsule(rb.mass, col.capsule.radius, col.capsule.halfLen); break;
        // ...
    }
    rb.invInertiaLocal = Inverse(I);
}
```

## World Inertia 갱신

회전이 바뀌면 world 관성 텐서도 갱신:
```cpp
void UpdateWorldInertia(RigidBodyComponent& rb, const Mat3& rotation)
{
    rb.invInertiaWorld = rotation * rb.invInertiaLocal * Transpose(rotation);
}
```

## 단위 테스트

```cpp
TEST(RigidBody, FreeFall)
{
    RigidBodyComponent rb; rb.mass = 1.f; rb.invMass = 1.f;
    TransformComponent t; t.SetPosition({0, 10, 0});
    Vec3 gravity{0, -9.81f, 0};

    // 1초 시뮬
    for (i32_t i = 0; i < 60; ++i)
        Integrate_SemiImplicit(rb, t, 1.f/60.f, gravity);

    // 위치 ~ 10 - 0.5*9.81*1 = 5.095
    EXPECT_NEAR(t.GetPosition().y, 5.095f, 0.2f);
    EXPECT_NEAR(rb.linearVelocity.y, -9.81f, 0.2f);
}

TEST(RigidBody, AngularMomentumConservation)
{
    RigidBodyComponent rb;
    rb.mass = 1.f; rb.invMass = 1.f;
    rb.invInertiaLocal = Mat3::Diagonal(1, 1, 1);
    rb.angularVelocity = {0, 5.f, 0};
    TransformComponent t;

    // 외력 없이 1초 회전
    for (i32_t i = 0; i < 60; ++i)
        Integrate_SemiImplicit(rb, t, 1.f/60.f, Vec3::Zero());

    // 각속도 보존 (damping 이 있으므로 약간 감소)
    EXPECT_GT(rb.angularVelocity.y, 4.5f);
}
```

## 구현 순서

1. `RigidBodyComponent` + Apply Force/Torque/Impulse API
2. Shape별 관성 텐서 계산 함수 (Sphere/Box/Capsule)
3. Semi-implicit Euler 적분기
4. Damping
5. Force Generator 인터페이스 + Gravity/Drag
6. Sleep 시스템
7. World Inertia 갱신
8. 단위 테스트
9. (선택) RK4 비교용
