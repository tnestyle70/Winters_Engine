# Physics 아키텍처 — ECS 통합 + 디렉토리

## 디렉토리

```
Engine/Public/Physics/
├── Core/
│   ├── PhysicsWorld.h           월드 루트 (중력, step, 월드 경계)
│   ├── PhysicsTypes.h           enum (ShapeType/BodyType/LayerMask)
│   └── PhysicsConfig.h          타임스텝, iteration, 튜닝 값
├── Collision/
│   ├── Shape/
│   │   ├── IShape.h             추상 인터페이스
│   │   ├── ShapeAABB.h
│   │   ├── ShapeSphere.h
│   │   ├── ShapeCapsule.h       ← MOBA 핵심
│   │   ├── ShapeOBB.h
│   │   └── ShapeConvex.h        GJK 입력
│   ├── NarrowPhase/
│   │   ├── AABB_vs_AABB.h
│   │   ├── Sphere_vs_Sphere.h
│   │   ├── Capsule_vs_Capsule.h
│   │   ├── SAT_OBB.h
│   │   └── GJK_EPA.h
│   ├── BroadPhase/
│   │   ├── IBroadPhase.h        추상 (SAP ↔ BVH 교체 가능)
│   │   ├── SweepAndPrune.h
│   │   └── DynamicAABBTree.h    ← 최종
│   ├── Raycast/
│   │   ├── RayAABB.h
│   │   ├── RaySphere.h
│   │   ├── RayCapsule.h
│   │   └── RaycastQuery.h       씬 전체 레이캐스트
│   └── Manifold.h               접촉 매니폴드 (법선, 관통량, 접촉점)
├── Dynamics/
│   ├── RigidBody.h              질량, 관성텐서, 속도
│   ├── Integrator.h             Semi-implicit Euler, Verlet
│   ├── ForceGenerator.h         중력, 바람, 드래그
│   └── MassProperties.h         셰이프별 관성텐서 계산
├── Constraints/
│   ├── IConstraint.h            추상
│   ├── ContactConstraint.h      충돌 응답
│   ├── DistanceConstraint.h     고정 거리
│   ├── HingeConstraint.h        경첩 (문/팔꿈치)
│   ├── SliderConstraint.h       미끄럼
│   └── SequentialImpulseSolver.h Erin Catto 방식
├── PBD/
│   ├── ParticleSystem.h         점 질량 배열
│   ├── DistanceConstraint_PBD.h 거리 유지
│   ├── BendingConstraint.h      천의 굽힘
│   ├── VolumeConstraint.h       부피 보존 (소프트바디)
│   ├── ClothSolver.h
│   ├── RopeSolver.h
│   └── XPBD.h                   확장 PBD (stiffness 안정화)
├── CCD/
│   ├── TimeOfImpact.h           이분 탐색 TOI
│   ├── SweptShape.h             원기둥 스윕
│   └── MotionClamp.h            1 프레임 이동 제한
└── Systems/
    ├── PhysicsStepSystem.h      통합 실행 시스템
    ├── BroadPhaseSystem.h
    ├── NarrowPhaseSystem.h
    ├── DynamicsSystem.h
    ├── ConstraintSystem.h
    ├── PBDSystem.h
    └── CCDSystem.h
```

## ECS 컴포넌트

```cpp
// Engine/Public/ECS/Components/PhysicsComponents.h

// 모든 콜라이더가 가짐
struct ColliderComponent {
    enum class Shape : u8_t { AABB, Sphere, Capsule, OBB, Convex } shape;
    union {
        struct { Vec3 halfExtents; }                aabb;
        struct { f32_t radius; }                    sphere;
        struct { f32_t radius; Vec3 axis; f32_t halfLen; } capsule;
        struct { Vec3 halfExtents; Quat rotation; } obb;
        struct { u32_t convexHullID; }              convex;
    };
    u32_t      layerMask   = 0xFFFFFFFFu;
    u32_t      groupID     = 0;
    bool_t     bTrigger    = false;   // true = 이벤트만, 물리 응답 없음
    bool_t     bStatic     = false;
};

// Dynamic 오브젝트만 가짐
struct RigidBodyComponent {
    f32_t      mass;
    f32_t      invMass;               // 0 = 무한 질량 (Kinematic/Static)
    Mat3       invInertiaLocal;       // 로컬 공간 관성 역행렬
    Mat3       invInertiaWorld;       // 매 프레임 갱신

    Vec3       linearVelocity   = Vec3::Zero();
    Vec3       angularVelocity  = Vec3::Zero();
    Vec3       accumForce       = Vec3::Zero();
    Vec3       accumTorque      = Vec3::Zero();

    f32_t      linearDamping    = 0.01f;
    f32_t      angularDamping   = 0.05f;
    f32_t      restitution      = 0.3f;   // 반발 계수
    f32_t      friction         = 0.5f;

    bool_t     bSleeping = false;      // 정적 상태일 때 연산 제외
};

// PBD 파티클 시스템 (천/로프)
struct ParticleComponent {
    std::vector<Vec3>     positions;
    std::vector<Vec3>     velocities;
    std::vector<Vec3>     prevPositions;
    std::vector<f32_t>    invMasses;
    std::vector<std::pair<u32_t, u32_t>> distanceEdges;  // 거리 제약 쌍
    std::vector<f32_t>    restLengths;
};
```

## 싱글턴 월드 리소스

```cpp
struct PhysicsWorldResource
{
    Vec3        gravity       = { 0.f, -9.81f, 0.f };
    f32_t       fixedTimestep = 1.f / 60.f;
    f32_t       accumulator   = 0.f;
    i32_t       velocityIters = 8;
    i32_t       positionIters = 3;

    unique_ptr<IBroadPhase>             broadPhase;
    std::vector<ContactManifold>        contactManifolds;
    std::vector<unique_ptr<IConstraint>> jointConstraints;
};
```

CWorld 에 리소스로 등록. BotSystem/ Rendering 시스템이 쿼리.

## Fixed Timestep (중요)

물리는 **가변 dt 쓰면 시뮬레이션 불안정**. 항상 고정 스텝 (예: 60Hz) 으로 실행하고
잉여 시간은 보간.

```cpp
// Engine/Public/Physics/Systems/PhysicsStepSystem.h
class CPhysicsStepSystem : public ISystem
{
public:
    void Execute(CWorld& world, f32_t dt) override
    {
        auto& res = world.GetResource<PhysicsWorldResource>();

        res.accumulator += dt;
        while (res.accumulator >= res.fixedTimestep)
        {
            Step(world, res.fixedTimestep);
            res.accumulator -= res.fixedTimestep;
        }
        // 선택: alpha = res.accumulator / res.fixedTimestep 로 render interpolation
    }

private:
    void Step(CWorld& world, f32_t dt)
    {
        // 1. Force → Velocity
        DynamicsSystem::IntegrateForces(world, dt);
        // 2. Broad Phase
        BroadPhaseSystem::UpdatePairs(world);
        // 3. Narrow Phase
        NarrowPhaseSystem::GenerateContacts(world);
        // 4. Constraint Solver
        ConstraintSystem::Solve(world, dt);
        // 5. Velocity → Position
        DynamicsSystem::IntegratePositions(world, dt);
        // 6. CCD 보정 (옵션)
        CCDSystem::ResolveHighSpeed(world);
        // 7. PBD 소프트바디 (별도 풀)
        PBDSystem::Simulate(world, dt);
    }
};
```

## 레이어 / 그룹 / 마스크

충돌 필터링:
```
canCollide(A, B) = 
    (A.layerMask & B.groupBit) != 0 &&
    (B.layerMask & A.groupBit) != 0 &&
    A.groupID != B.groupID  // 같은 그룹 내부 제외
```

MOBA 예시 그룹:
- `Group_Champion`
- `Group_Minion`
- `Group_Monster`
- `Group_Projectile`
- `Group_Structure`
- `Group_Terrain`
- `Group_Hitbox`
- `Group_Hurtbox`

Hitbox 레이어마스크 = `Group_Hurtbox` 만 (자기 팀 제외 로직은 별도).

## 이벤트 시스템

```cpp
// 충돌 이벤트 (Trigger 전용 또는 충돌 응답 성공 시)
struct CollisionEvent {
    EntityID    entityA;
    EntityID    entityB;
    Vec3        contactPoint;
    Vec3        normal;
    f32_t       penetration;
    u32_t       flags;   // Enter / Stay / Exit
};

// 이벤트 버스로 전파 (Phase 1 EventBus 활용)
EventBus::Publish<CollisionEvent>(ev);

// 게임 로직 구독 (예: Hitbox 히트 처리)
EventBus::Subscribe<CollisionEvent>([](const CollisionEvent& ev) {
    if (HasComponent<HitboxComponent>(ev.entityA) && HasComponent<HurtboxComponent>(ev.entityB))
        ApplyDamage(ev.entityA, ev.entityB);
});
```

## 병렬화 (Phase 1b JobSystem 이후)

- **Broad Phase**: BVH 빌드는 병렬 (Morton Code 정렬 + Radix)
- **Narrow Phase**: Pair 리스트를 Job 으로 나눠 병렬 collision detection
- **Constraint Solver**: Island 기반 분할 → 각 Island 독립 해결
- **PBD**: Graph coloring 으로 병렬 distance constraint

## 결정론 (Determinism)

서버 권위 멀티플레이에선 클라-서버 시뮬 결과 일치 필수:
- `f32_t` → `i32_t` fixed-point 고려 (과도하면 `f64_t`)
- 연산 순서 고정 (Entity ID 오름차순)
- 하드웨어 FPU 모드 고정 (`_controlfp`)

MOBA 에선 서버 결과만 권위 → 클라이언트 물리는 예측용이라 결정론 불필요할 수 있음.
