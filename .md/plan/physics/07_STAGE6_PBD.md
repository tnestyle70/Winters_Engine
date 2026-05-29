# Stage 6 — PBD (Position Based Dynamics)

## 목표

천·로프·소프트바디 시뮬레이션. Matthias Müller 2007 논문. Constraint Solver 와 완전 다른 접근.

## 왜 PBD 인가

- Constraint Solver (Stage 5) 는 rigid 강체 전용 — 변형 가능 물체 어려움
- Mass-Spring 시스템은 explicit Euler 로 불안정 (스프링 상수 크면 폭발)
- **PBD**: 속도가 아닌 **위치를 직접 투영** → 무조건 안정
- 실시간 천/로프/물/변형에 업계 표준

## 핵심 아이디어

```
일반 물리: F → a → v → x
PBD:       x 를 제약 조건 만족하도록 직접 수정 → v = (x_new - x_old) / dt
```

## 알고리즘 (논문 원본)

```
1. predict: x' = x + v × dt + (F/m) × dt²
2. collision detection → generate contact constraints
3. for iter in 1..N:
     project each constraint onto x'
4. v = (x' - x) / dt
5. x = x'
```

## Particle System

기본 데이터:

```cpp
struct ParticleSystem
{
    std::vector<Vec3>    positions;         // 현재 위치 x
    std::vector<Vec3>    prevPositions;     // 이전 위치 (velocity 계산용)
    std::vector<Vec3>    velocities;
    std::vector<Vec3>    predictedPositions;// x' (prediction step 후)
    std::vector<f32_t>   invMasses;
    std::vector<u32_t>   flags;             // pinned (고정) / dynamic

    std::vector<unique_ptr<IPBDConstraint>> constraints;
};
```

## PBD 제약 조건

각 제약은 제약 함수 C(x) 가 0 되도록 위치 투영.

### Distance Constraint (거리 유지)

```
C(p1, p2) = |p1 - p2| - restLength = 0
```

투영:
```cpp
void DistanceConstraint::Project(std::vector<Vec3>& x, const std::vector<f32_t>& w) {
    Vec3 delta = x[i1] - x[i2];
    f32_t d = Length(delta);
    f32_t C = d - restLength;
    if (std::abs(C) < 1e-6f) return;

    Vec3 n = delta / d;
    f32_t wSum = w[i1] + w[i2];
    Vec3 corr1 = -(w[i1] / wSum) * C * n;
    Vec3 corr2 =  (w[i2] / wSum) * C * n;

    x[i1] += stiffness * corr1;
    x[i2] += stiffness * corr2;
}
```

용례:
- **천 구조 제약** (stretching) — 메시 엣지마다
- **로프 링크** — 파티클 체인

### Bending Constraint

천의 굽힘 저항. 두 삼각형 공유 엣지에 대해 이면각 유지.

```
C = arccos(n1 · n2) - θ_rest
```

### Volume Constraint

소프트바디 부피 유지. 전체 메시 부피가 초기 부피 일정.

### Collision Constraint

동적 생성. 접촉 발생 시 추가.

```cpp
struct ParticleContactConstraint {
    u32_t particleIdx;
    Vec3  contactPoint;
    Vec3  normal;
    f32_t penetration;
    // C = dot(particle - contactPoint, normal) - 0 >= 0
};
```

## 천 (Cloth) 시뮬레이션

격자 파티클 배열 → 거리 제약 + 굽힘 제약.

```cpp
class CClothSolver
{
public:
    // 격자 크기 (w × h), 파티클 간격 spacing, 질량 mass
    void CreateGrid(i32_t w, i32_t h, f32_t spacing, f32_t massPerParticle);

    // 핀 고정 (네 귀퉁이 중 원하는 것)
    void Pin(i32_t idx);

    void Step(f32_t dt, i32_t iterations = 10);

    // GPU 업로드용 정점 데이터 생성
    void UpdateRenderVertices(std::vector<Vertex>& out) const;

private:
    ParticleSystem m_particles;
    i32_t m_width, m_height;
};
```

제약 구조:
- **Structural** (stretch): 가로/세로 이웃 (4)
- **Shear** (전단): 대각선 이웃 (2)
- **Bending** (굽힘): 두 칸 건너 이웃 (2)

## 로프 (Rope)

1D 파티클 체인. 아주 단순.

```cpp
class CRopeSolver
{
public:
    void Create(const Vec3& start, const Vec3& end, i32_t segments);
    void Step(f32_t dt, i32_t iterations = 20);   // 로프는 더 많은 iter 필요

    // 끝단 핀 고정
    void PinStart(bool_t pin);
    void PinEnd(bool_t pin);
};
```

## Soft Body

테트라헤드럴 메시 + Distance + Volume Constraint.

```cpp
class CSoftBodySolver
{
public:
    // 입력: 표면 메시 → 내부 테트라 생성 (TetGen 또는 수동)
    void CreateFromTetMesh(const TetMesh& tet);

    void Step(f32_t dt, i32_t iterations = 5);
};
```

## XPBD (Extended PBD)

기본 PBD 의 문제: Iteration 수 / 해상도에 따라 stiffness 변동.
**XPBD** 는 compliance 기반으로 해결:

```
Δλ = -(C + α̃λ) / (∇C × M⁻¹ × ∇C^T + α̃)
     where α̃ = compliance / dt²
```

참고: Macklin et al. 2016 "XPBD: Position-Based Simulation of Compliant Constrained Dynamics".

```cpp
struct XPBDConstraint {
    f32_t compliance = 0.f;        // 0 = rigid, 높으면 부드러움
    f32_t lambda     = 0.f;        // 누적 multiplier
};
```

## 파티클 vs 강체 결합

천이 강체 위에 떨어지는 경우:
- 파티클 위치를 강체 콜라이더와 비교 → Contact Constraint 생성
- 강체 영향력은 작게 반영 (일방향 결합 허용)

## 병렬화

**Graph Coloring**: 제약 조건을 색칠해서 같은 색은 공유 파티클 없음 → 병렬 투영.

```cpp
// 같은 색 제약들은 독립적으로 투영 가능
std::vector<std::vector<u32_t>> colors;  // 각 색 그룹의 제약 인덱스
```

Phase 1b 이후 JobSystem 으로 각 색 그룹 병렬 실행.

## GPU PBD (선택)

Compute Shader 로 파티클 투영 → 수만 개 파티클 시뮬 가능.

```hlsl
// Shaders/Physics/PBD_DistanceConstraint.hlsl
[numthreads(64, 1, 1)]
void ProjectDistanceConstraints(uint3 id : SV_DispatchThreadID)
{
    uint c = id.x;
    if (c >= g_constraintCount) return;
    
    uint2 idx = g_constraintPairs[c];
    float3 x1 = g_positions[idx.x];
    float3 x2 = g_positions[idx.y];
    // ... 투영
    g_positions[idx.x] = x1 + corr1;
    g_positions[idx.y] = x2 + corr2;
}
```

같은 색 그룹끼리만 dispatch → data race 없음.

## 시각화

- 파티클 점군 (작은 sphere)
- 제약 조건 선 (색상 = 제약 타입)
- 핀 고정 파티클 빨강

## 연습모드 / 연출 용례

- 엘든링 모작: 캐릭터 망토/로브
- MOBA: 부서지는 깃발, 천장 촛대, 물리 장식
- VFX: 폭발 후 떨어지는 천, 용의 날개

## 단위 테스트

```cpp
TEST(PBD, DistanceConstraint_Convergence)
{
    ParticleSystem ps;
    ps.positions = {{0,0,0}, {3,0,0}};
    ps.invMasses = {1, 1};
    DistanceConstraint dc{0, 1, 2.f, 1.f};   // 목표 거리 2

    for (i32_t i = 0; i < 10; ++i)
        dc.Project(ps.positions, ps.invMasses);

    f32_t d = Length(ps.positions[1] - ps.positions[0]);
    EXPECT_NEAR(d, 2.f, 0.01f);
}

TEST(PBD, ClothGridFall)
{
    CClothSolver cloth;
    cloth.CreateGrid(10, 10, 0.1f, 0.01f);
    cloth.Pin(0);       // 한 귀퉁이 고정
    cloth.Pin(9);       // 반대 귀퉁이 고정

    for (i32_t i = 0; i < 600; ++i) cloth.Step(1.f/60.f, 10);

    // 중앙 파티클은 아래로 처짐
    Vec3 center = cloth.GetParticle(55);
    EXPECT_LT(center.y, -0.2f);
}
```

## 구현 순서

1. `ParticleSystem` 기본 (positions/velocities/prevPositions)
2. Prediction + Velocity 업데이트
3. `DistanceConstraint` + Project
4. Collision Constraint (파티클-정적지면)
5. `CClothSolver` 격자 생성 + 구조적 제약
6. Bending Constraint
7. `CRopeSolver` 구현
8. XPBD 로 stiffness 안정화
9. Graph Coloring 병렬화 준비
10. (선택) GPU Compute Shader 이식
11. Soft Body (고급)
