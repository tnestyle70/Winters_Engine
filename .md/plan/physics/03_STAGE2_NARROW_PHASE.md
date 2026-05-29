# Stage 2 — Narrow Phase (SAT OBB-OBB, GJK + EPA Convex)

## 목표

Stage 1 단순 프리미티브로 처리 안 되는 **임의 회전 박스·컨벡스 메시** 충돌 정밀 판정.

## 언제 쓰는가

| 게임 요소 | 사용 Narrow Phase |
|---|---|
| 챔피언 (Capsule) | Stage 1 Capsule vs Capsule |
| 미니언/몬스터 (Capsule) | Stage 1 |
| 투사체 (Sphere/Capsule) | Stage 1 |
| 포탑/넥서스 (OBB) | **SAT OBB-OBB** |
| 기둥/장애물 (OBB) | **SAT OBB-OBB** |
| 회전된 히트박스 (OBB) | **SAT OBB-OBB** |
| 불규칙 지형 조각 (Convex) | **GJK+EPA** |
| 엘든링 모작 무기 (Convex) | **GJK+EPA** |

## SAT (Separating Axis Theorem)

두 볼록 도형이 분리 축 하나라도 존재하면 충돌 X. 없으면 충돌 O.

### OBB vs OBB (15개 축 검사)

```
검사할 축:
  - A 의 local 축 3개 (x, y, z)
  - B 의 local 축 3개
  - A x B 의 축 외적 9개 (3 × 3)
= 총 15개
```

```cpp
struct OBB {
    Vec3   center;
    Mat3   rotation;    // 3 축 (열 벡터)
    Vec3   halfExtents;

    Vec3 Axis(i32_t i) const { return Vec3(rotation[0][i], rotation[1][i], rotation[2][i]); }
};

bool IntersectOBB_SAT(const OBB& a, const OBB& b, Vec3* outNormal = nullptr, f32_t* outDepth = nullptr)
{
    const Vec3 axesA[3] = { a.Axis(0), a.Axis(1), a.Axis(2) };
    const Vec3 axesB[3] = { b.Axis(0), b.Axis(1), b.Axis(2) };

    Vec3 bestAxis;
    f32_t minOverlap = FLT_MAX;

    auto CheckAxis = [&](const Vec3& axis) -> bool {
        if (LengthSquared(axis) < 1e-6f) return true;    // 평행 축 skip

        Vec3 a0 = ProjectOnto(a, axis);      // {min, max}
        Vec3 b0 = ProjectOnto(b, axis);
        f32_t overlap = std::min(a0.max, b0.max) - std::max(a0.min, b0.min);
        if (overlap <= 0.f) return false;    // 분리축 발견 → 충돌 X

        if (overlap < minOverlap) {
            minOverlap = overlap;
            bestAxis = axis;
        }
        return true;
    };

    for (i32_t i = 0; i < 3; ++i) if (!CheckAxis(axesA[i])) return false;
    for (i32_t j = 0; j < 3; ++j) if (!CheckAxis(axesB[j])) return false;
    for (i32_t i = 0; i < 3; ++i)
        for (i32_t j = 0; j < 3; ++j)
            if (!CheckAxis(Cross(axesA[i], axesB[j]))) return false;

    if (outNormal) *outNormal = Normalize(bestAxis);
    if (outDepth)  *outDepth  = minOverlap;
    return true;
}
```

### 접촉점 생성 (Contact Manifold)

충돌 여부만으론 부족. 어디 접촉했는지 필요:

```cpp
struct ContactPoint {
    Vec3  position;       // 월드 공간
    Vec3  normal;
    f32_t penetration;
};

struct ContactManifold {
    EntityID entityA, entityB;
    std::array<ContactPoint, 4> points;   // 최대 4개 (면-면 접촉)
    i32_t    count = 0;
};
```

OBB-OBB 접촉은 **Sutherland-Hodgman Clipping** 으로 접촉 면적 구한 뒤 꼭짓점 4개 선택.
Erin Catto 의 GDC 영상 참고.

## GJK (Gilbert-Johnson-Keerthi)

임의 볼록 도형 간 충돌 판정. 핵심 개념: **Minkowski Difference** A ⊖ B 가 원점을 포함하면 충돌.

### Support 함수

Convex 도형의 임의 방향 `d` 로의 **가장 먼 점**:

```cpp
struct IConvexShape {
    virtual Vec3 Support(const Vec3& direction) const = 0;
};

// Sphere Support
Vec3 SphereShape::Support(const Vec3& dir) const {
    return center + Normalize(dir) * radius;
}

// Capsule Support
Vec3 CapsuleShape::Support(const Vec3& dir) const {
    f32_t dot0 = Dot(dir, p0);
    f32_t dot1 = Dot(dir, p1);
    Vec3 base = (dot0 > dot1) ? p0 : p1;
    return base + Normalize(dir) * radius;
}

// Convex Mesh Support
Vec3 ConvexMeshShape::Support(const Vec3& dir) const {
    f32_t bestDot = -FLT_MAX;
    Vec3 best;
    for (const Vec3& v : vertices) {
        f32_t d = Dot(dir, v);
        if (d > bestDot) { bestDot = d; best = v; }
    }
    return best;
}

// Minkowski Support
Vec3 SupportMinkowski(const IConvexShape& a, const IConvexShape& b, const Vec3& dir) {
    return a.Support(dir) - b.Support(-dir);
}
```

### GJK 알고리즘 (의사 코드)

```
1. d = 임의 방향 (예: B 중심 - A 중심)
2. simplex = { SupportMinkowski(A, B, d) }
3. d = -simplex[0]
4. loop:
     p = SupportMinkowski(A, B, d)
     if Dot(p, d) < 0: return NO_COLLISION    // 원점이 d 방향 바깥
     simplex.push(p)
     if DoSimplex(simplex, d): return COLLISION
```

`DoSimplex` 는 simplex 크기별 (Line/Triangle/Tetrahedron) 케이스 분기. 원점을
포함할 수 있는지 판별 + 다음 탐색 방향 `d` 갱신.

### EPA (Expanding Polytope Algorithm)

GJK 이 COLLISION 반환 후, **침투 깊이 + 법선** 계산용.

```
1. GJK 의 최종 simplex (Tetrahedron) 를 초기 polytope 로
2. loop:
     closestFace = 원점과 가장 가까운 면
     p = SupportMinkowski(A, B, closestFace.normal)
     d = Dot(p, closestFace.normal) - closestFace.distance
     if d < ε: return closestFace   // 수렴
     polytope.expand(p)
```

결과: 침투 법선 = closestFace.normal, 침투량 = closestFace.distance.

## 구현 스케치

```cpp
// Engine/Public/Physics/Collision/NarrowPhase/GJK_EPA.h
class CGJKEPASolver
{
public:
    struct Result {
        bool_t bCollision;
        Vec3   normal;
        f32_t  penetration;
        Vec3   contactA;
        Vec3   contactB;
    };

    Result Solve(const IConvexShape& a, const IConvexShape& b);

private:
    bool GJK(const IConvexShape& a, const IConvexShape& b, std::vector<Vec3>& simplex);
    Result EPA(const IConvexShape& a, const IConvexShape& b, std::vector<Vec3>& simplex);

    bool DoSimplex(std::vector<Vec3>& simplex, Vec3& direction);
};
```

## Dispatch Table

셰이프 타입 조합별로 올바른 Narrow Phase 함수 호출:

```cpp
using CollisionFn = bool(*)(const ColliderComponent&, const ColliderComponent&, ContactManifold&);

CollisionFn g_dispatchTable[ShapeType::Count][ShapeType::Count] = {
    /* AABB vs */    { Collide_AABB_AABB,    Collide_AABB_Sphere,  /* ... */ },
    /* Sphere vs */  { nullptr,              Collide_Sphere_Sphere, /* ... */ },
    /* Capsule vs */ { /* ... */,            /* ... */,            Collide_Capsule_Capsule },
    /* OBB vs */     { /* ... */,            /* ... */,            /* ... */, Collide_OBB_OBB },
    /* Convex vs */  { nullptr,              nullptr,              nullptr,   nullptr, Collide_Convex_Convex_GJK_EPA },
};
```

비대칭 처리 (`nullptr` 인 쪽은 swap 후 대칭 함수 호출).

## 디버그 시각화

- **SAT 축** 시각화: 분리 축 15개 각각 그리고, 분리 발견 시 빨강, 중첩이면 노랑
- **GJK Simplex** 애니메이션: 반복마다 simplex 그리기 (교육용)
- **EPA Polytope** 3D 시각화
- **Contact Manifold** 접촉점 + 법선 파랑 화살표

## 성능

- SAT OBB-OBB: ~15 개 dot product + min/max = 매우 빠름 (1μs 미만)
- GJK: 보통 5~10 iteration. Convex 정점 수가 적으면 (10~20) 수 μs
- **Early out**: AABB 교차 먼저 검사 후 Narrow Phase (Broad Phase 의 역할)

## 단위 테스트

```cpp
TEST(NarrowPhase, OBB_vs_OBB_Rotated_45)
{
    OBB a = MakeOBB({0,0,0}, Mat3::Identity(), {1,1,1});
    OBB b = MakeOBB({2,0,0}, Mat3::RotationY(PI/4), {1,1,1});
    Vec3 n; f32_t d;
    EXPECT_TRUE(IntersectOBB_SAT(a, b, &n, &d));
    EXPECT_GT(d, 0.f);
    EXPECT_LT(d, 2.f);
}

TEST(NarrowPhase, GJK_Convex_vs_Convex)
{
    ConvexMeshShape a = MakeTetrahedron(...);
    ConvexMeshShape b = MakeCube(...);
    CGJKEPASolver solver;
    auto r = solver.Solve(a, b);
    EXPECT_TRUE(r.bCollision);
}
```

## 구현 순서

1. `OBB` 구조 + `ProjectOnto` 헬퍼
2. SAT OBB-OBB 충돌 여부만
3. Contact Manifold 생성 (Sutherland-Hodgman)
4. `IConvexShape` + Sphere/Capsule/ConvexMesh Support 함수
5. GJK 알고리즘 (Line/Triangle/Tetrahedron case)
6. EPA (polytope expansion)
7. Dispatch Table
8. 디버그 시각화 (EPA polytope 가장 까다로움)
