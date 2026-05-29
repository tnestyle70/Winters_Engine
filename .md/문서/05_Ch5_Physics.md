# Ch5. Physics (Rigid / Cloth / Destruction / Fluid)

> Winters 현재: LoL식 capsule + nav grid. **강체/천/파괴/유체 전부 없음.**
> 레퍼런스: `UnrealEngine/Engine/Source/Runtime/Experimental/Chaos/Public/Chaos/`, `Runtime/PhysicsCore/Public/`.

---

## 1. 기초 원리 — 물리는 두 종류로 갈라진다

| 분류 | 목적 | 정확도 | 예 |
|------|------|--------|-----|
| **Gameplay 물리** | 게임 규칙에 직결 | 결정적, 단순 | 캐릭터 capsule, 발사체 raycast, 점프 |
| **Visual 물리** | 보기에 좋게 | 비결정적, 화려 | 망토, 파괴된 건물, 머리카락, 천 |

이 둘을 섞으면 멀티플레이가 깨진다. **Gameplay는 서버 권위 + 결정적**, **Visual은 클라 only + 비결정적**.

Winters의 `Shared/GameSim/`은 **gameplay physics**만 들어가야 하고, `Client/Private/`는 **visual physics**까지 자유롭게.

---

## 2. 핵심 — 6대 구성요소

### 2.1 Broad Phase / Narrow Phase

물리 = **충돌 찾기 + 응답 계산**.

```text
Broad Phase: O(N²) 후보를 O(N log N)으로 축소
  - AABB Tree (UE5 Chaos AABBTree.h)
  - Sweep & Prune
  - Spatial Hash
Narrow Phase: 후보 쌍의 실제 충돌점 계산
  - GJK + EPA (convex)
  - SAT (Separating Axis Theorem)
  - Triangle mesh query
```

UE5 Chaos: `Source/Runtime/Experimental/Chaos/Public/Chaos/AABBTree.h`, `BoundingVolumeHierarchy.h`.

### 2.2 CollisionShape (게임플레이 query primitive)

`Source/Runtime/PhysicsCore/Public/CollisionShape.h:8~50`:

```cpp
namespace ECollisionShape
{
    enum Type { Line, Box, Sphere, Capsule };
}

struct FCollisionShape
{
    ECollisionShape::Type ShapeType;
    union
    {
        struct { float HalfExtentX, HalfExtentY, HalfExtentZ; } Box;
        struct { float Radius; } Sphere;
        struct { float Radius; HalfHeight; } Capsule;
    };
};
```

캐릭터 hit detection, ability 범위 query는 거의 이 4개로 끝.

### 2.3 Rigid Body

물체 = 질량 + 관성텐서 + 위치 + 회전 + 속도 + 각속도.

```cpp
// Chaos 등가
struct FRigidParticle
{
    FVec3   X;        // position
    FQuat   R;        // rotation
    FVec3   V;        // linear velocity
    FVec3   W;        // angular velocity
    FMatrix33 I;      // inertia tensor
    float   M;        // mass
    EObjectStateType ObjectState;  // Static / Kinematic / Dynamic / Sleeping
};
```

**Sleep**: 안 움직이면 simulation 제외. 1000개 박스가 쌓여 있어도 idle 후엔 CPU 0%.

### 2.4 Constraint Solver

조인트(힌지, 볼소켓, 6-DoF), 충돌 응답, 마찰을 **반복 푸는** 비선형 방정식.

UE5 Chaos: PBD (Position Based Dynamics) + Sequential Impulse 하이브리드.

```text
for iter in 0..N:
    for each constraint c:
        c.Solve()  // 위치/속도를 살짝 보정
```

iteration 늘리면 안정, 줄이면 빠름. AAA는 보통 16~32회.

### 2.5 Character Controller

게임플레이 친화 capsule. 보통의 rigid body 시뮬레이션이 아니라 **kinematic + ground sweep + slope/step handling**.

```cpp
// 의사 코드
void CharacterMove(Vec3 desiredVelocity, float dt)
{
    Vec3 dp = desiredVelocity * dt;
    if (groundCheck())   dp += gravityCompensation;
    sweep capsule along dp:
        if hit floor at angle < walkable: slide along
        if hit step < stepHeight: snap up
        else: stop
    apply final position
}
```

UE5 `UCharacterMovementComponent`가 ~10000줄. 게임 캐릭 이동의 90%가 여기에 있다.

### 2.6 Cloth / Destruction / Fluid

- **Cloth**: triangle mesh + spring/bending constraint. UE5 `ChaosCloth/`.
- **Destruction**: precomputed Voronoi fracture pieces + connection graph. UE5 `GeometryCollectionCore/`.
- **Fluid**: SPH / FLIP / Eulerian grid. UE5는 Niagara fluid (5.2+).

---

## 3. 심화

### 3.1 결정적 물리 (Deterministic Physics)

격투 게임 / RTS / 리플레이 / 멀티 lockstep에 필수. 단순 float가 아니라:
- **fixed timestep** (가변 dt 금지)
- **single-thread** 또는 deterministic parallel reduction
- **float bit-exact** 또는 fixed-point

UE5는 deterministic mode 지원하지만 옵셔널. 멀티 lockstep RTS는 별도 엔진 권장 (또는 Chaos deterministic mode + 모든 단순화).

Winters는 **서버 권위 + lockstep 아님** (snapshot replication). 따라서 server gameplay physics만 deterministic 보장하면 됨.

### 3.2 CCD (Continuous Collision Detection)

빠른 발사체가 얇은 벽을 통과하는 tunneling 방지.

UE5 Chaos: `Source/Runtime/Experimental/Chaos/Public/Chaos/CCDModification.h`, `CCDUtilities.h`.

원리: 한 프레임 전 위치와 현재 위치 사이를 swept volume으로 query.

### 3.3 멀티스레드

UE5 Chaos는 island 분리. 서로 안 닿는 물체 그룹은 독립 island, 각자 다른 스레드에서 solve.

```text
Island 1: 산 위의 박스 100개 → Worker 1
Island 2: 강 위의 통나무 50개 → Worker 2
Island 3: 캐릭터 ragdoll → Worker 3
```

### 3.4 Visual vs Gameplay 분리

```text
Server: 단순 capsule + raycast + 게임플레이 결과만
Client visual:
   - Server snapshot으로 capsule 위치 보간
   - 그 위에 cloth/ragdoll/destruction 시뮬 (서버 동기화 X)
   - hit 시 server damage가 도착, 그 시점에 visual ragdoll 시작
```

이 분리가 깨지면 50ms ping 차이만으로 다른 플레이어가 다른 결과를 보는 사고.

### 3.5 PhysX vs Chaos vs Jolt

| 엔진 | 강점 | 약점 | 라이선스 |
|------|------|------|---------|
| **PhysX 4** (NVIDIA) | 게임 업계 표준, 안정 | GPU 의존, 닫혀가는 중 | BSD-style |
| **Chaos** (UE5) | 결정적 모드, mesh deformation, 파괴 | UE5 종속, 무거움 | UE EULA (외부 사용 까다로움) |
| **Jolt** | 빠름, MIT, multi-thread 잘 짜여 있음, 호라이즌 채택 | 기능 적음 (cloth 없음) | MIT |
| **자체** | 100% 통제 | 작성 비용 1~2 인년 | — |

Winters 1차 권장: **Jolt 통합** (LoL/로아 급은 Jolt 충분, MIT라 깨끗).
오픈월드 가면 PhysX 또는 Chaos 포팅 검토.

---

## 4. Winters 매핑

### 4.1 현재 상태

- `Engine/Public/ECS/SpatialIndex.h` — 2D grid query (LoL fog/range query)
- 충돌은 capsule × capsule 거리만
- nav는 `Engine/Public/Manager/Navigation/` (grid 기반)
- physics constraint / cloth / destruction 없음

### 4.2 Ch5 추가 헤더 (제안)

```cpp
// Engine/Public/Physics/CollisionShape.h
enum class eCollisionShape : u8_t { Line, Box, Sphere, Capsule, ConvexHull, TriangleMesh };

struct CollisionShape
{
    eCollisionShape type;
    union {
        struct { f32_t hx, hy, hz; }            box;
        struct { f32_t radius; }                sphere;
        struct { f32_t radius, halfHeight; }    capsule;
        struct { u32_t convexId; }              convex;
        struct { u32_t meshId; }                trimesh;
    };
};

// Engine/Public/Physics/RigidBody.h
enum class eRigidState : u8_t { Static, Kinematic, Dynamic, Sleeping };

struct RigidBodyComponent
{
    Vec3      position;
    Quat      rotation;
    Vec3      linearVelocity;
    Vec3      angularVelocity;
    f32_t     mass;
    Mat3      invInertia;
    eRigidState state;
    u32_t     filterGroup;   // collision matrix
};

// Engine/Public/Physics/PhysicsWorld.h
class WINTERS_ENGINE CPhysicsWorld
{
public:
    void Initialize();
    void Step(f32_t dt);                         // fixed dt 권장

    bool Raycast(const Vec3& from, const Vec3& dir, f32_t maxDist, RaycastHit& out) const;
    bool SweepCapsule(...) const;
    void OverlapSphere(...) const;

    EntityID CreateBody(const RigidBodyDesc& desc, const CollisionShape& shape);
    void DestroyBody(EntityID id);
};

// Engine/Public/Physics/CharacterController.h
class WINTERS_ENGINE CCharacterController
{
public:
    void Move(const Vec3& desiredVelocity, f32_t dt);
    bool IsGrounded() const;
    f32_t maxSlopeAngle;
    f32_t stepHeight;
};
```

### 4.3 Bot AI / GameSim 불변식 재확인

- **서버 gameplay physics**: capsule sweep, raycast 정도면 끝. 결정적이어야 함.
- **클라 visual physics**: cloth, ragdoll, destruction은 클라만. 서버는 cue("death")만 보냄.
- Bot AI는 **물리를 모른다**. AI는 `MoveTarget` Component 갱신만, 실제 이동 sweep은 `MovementSystem`이 처리.

### 4.4 단계별

```text
Ch5-Stage1  CollisionShape + Raycast / Sweep (현재 capsule×capsule을 정식 wrap)
Ch5-Stage2  RigidBody + PhysicsWorld (정적 + 키네마틱)
Ch5-Stage3  Dynamic body + Constraint solver (간단한 박스 떨어뜨리기)
Ch5-Stage4  CharacterController (step/slope/slide)
Ch5-Stage5  Jolt 또는 Chaos 통합 (3rd party engine 백엔드)
Ch5-Stage6  Cloth (망토, 머리카락) — 클라 only
Ch5-Stage7  Destruction (geometry collection)
Ch5-Stage8  Ragdoll (Ch4 Animation과 결합)
Ch5-Stage9  Determinism / CCD / Multi-thread island
```

### 4.5 게임별 적용

| 게임 | 필요 Stage |
|------|-----------|
| LoL (현재) | Stage 1 (capsule + raycast)면 충분 |
| 로아 | Stage 1, 2, 4, 6 (캐릭 + 망토) |
| 엘든링 | Stage 1~8 + active ragdoll |
| GTA6 | Stage 1~9 전부 + 차량 / 군중 / 파괴 |

---

## 5. 검증 명령

```powershell
# Physics smoke
.\Client\Bin\Debug-DX12\WintersGame.exe --phys-debug --phys-show-aabb

# 기대 로그
# [Phys] world initialized: 0 dynamic, 142 static, broadphase=AABBTree
# [Phys] step dt=0.0167 islands=3 solver_iter=16 contacts=12
# [Phys] character walked 12.4m, grounded=true, slope=8.3deg
```

---

## 6. 다음 챕터로

Ch5 Stage 4까지 가야 **Ch9 AI Navigation**의 NavMesh가 character controller와 정합. Ch4 Stage 9 (physics-driven anim)는 Ch5 Stage 8 (ragdoll) 의존.
