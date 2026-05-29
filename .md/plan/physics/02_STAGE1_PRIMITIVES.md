# Stage 1 — 프리미티브 충돌 (AABB / Sphere / Capsule / Ray)

## 목표

가장 기본적인 **볼륨 vs 볼륨** 및 **레이 vs 볼륨** 충돌 판정.

## 왜 먼저인가

- MOBA 스킬샷 90% 가 이 Stage 만으로 처리 가능
- Broad Phase (Stage 3) 의 AABB Tree 도 여기 의존
- Pick / Raycast / LoS 체크 전부 여기 사용

## 구현 항목

### AABB (Axis-Aligned Bounding Box)

```cpp
struct AABB {
    Vec3 min, max;

    Vec3 Center() const        { return (min + max) * 0.5f; }
    Vec3 HalfExtents() const   { return (max - min) * 0.5f; }
    bool Contains(const Vec3& p) const;
    bool Intersects(const AABB& other) const;
    AABB Union(const AABB& other) const;
    f32_t SurfaceArea() const;    // BVH 의 SAH 비용에 사용
    void Expand(f32_t margin);
};
```

AABB vs AABB:
```cpp
bool Overlap(const AABB& a, const AABB& b)
{
    if (a.max.x < b.min.x || a.min.x > b.max.x) return false;
    if (a.max.y < b.min.y || a.min.y > b.max.y) return false;
    if (a.max.z < b.min.z || a.min.z > b.max.z) return false;
    return true;
}
```

### Sphere

```cpp
struct Sphere {
    Vec3  center;
    f32_t radius;
};

// Sphere vs Sphere
bool Overlap(const Sphere& a, const Sphere& b)
{
    f32_t d = Length(a.center - b.center);
    return d <= (a.radius + b.radius);
}

// Sphere vs AABB
bool Overlap(const Sphere& s, const AABB& box)
{
    Vec3 closest = Clamp(s.center, box.min, box.max);
    f32_t d2 = LengthSquared(s.center - closest);
    return d2 <= (s.radius * s.radius);
}
```

### Capsule ★ MOBA 핵심

챔피언/투사체의 기본 콜라이더. 수직축 원기둥 + 양 끝 반구.

```cpp
struct Capsule {
    Vec3  p0, p1;          // 양 끝 중심
    f32_t radius;
};

// Capsule vs Capsule (가장 자주 쓰임)
bool Overlap(const Capsule& a, const Capsule& b, Vec3* outNormal, f32_t* outDepth)
{
    // 세그먼트 vs 세그먼트 최단거리 계산
    f32_t s, t;
    Vec3 ca, cb;
    ClosestPointSegmentSegment(a.p0, a.p1, b.p0, b.p1, s, t, ca, cb);

    Vec3 diff = ca - cb;
    f32_t d = Length(diff);
    f32_t sumR = a.radius + b.radius;

    if (d > sumR) return false;

    if (outNormal) *outNormal = d > 1e-6f ? diff / d : Vec3(0, 1, 0);
    if (outDepth)  *outDepth  = sumR - d;
    return true;
}
```

**세그먼트 간 최단 거리** 는 핵심 서브루틴:
```cpp
void ClosestPointSegmentSegment(
    const Vec3& p1, const Vec3& q1,
    const Vec3& p2, const Vec3& q2,
    f32_t& s, f32_t& t, Vec3& c1, Vec3& c2);
```
Ericson "Real-Time Collision Detection" 5.1.9 알고리즘 그대로 이식.

### Ray / Raycast

```cpp
struct Ray {
    Vec3  origin;
    Vec3  direction;    // 단위 벡터
    f32_t maxDist = FLT_MAX;
};

struct RayHit {
    EntityID entity;
    Vec3     point;
    Vec3     normal;
    f32_t    distance;
};
```

#### Ray vs AABB (Slab 방법)

```cpp
bool Intersect(const Ray& r, const AABB& box, f32_t& tMin, f32_t& tMax)
{
    tMin = 0.f;
    tMax = r.maxDist;
    for (i32_t i = 0; i < 3; ++i) {
        if (std::abs(r.direction[i]) < 1e-6f) {
            if (r.origin[i] < box.min[i] || r.origin[i] > box.max[i]) return false;
        } else {
            f32_t invD = 1.f / r.direction[i];
            f32_t t1 = (box.min[i] - r.origin[i]) * invD;
            f32_t t2 = (box.max[i] - r.origin[i]) * invD;
            if (t1 > t2) std::swap(t1, t2);
            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);
            if (tMin > tMax) return false;
        }
    }
    return true;
}
```

#### Ray vs Sphere

```cpp
bool Intersect(const Ray& r, const Sphere& s, f32_t& t)
{
    Vec3 m = r.origin - s.center;
    f32_t b = Dot(m, r.direction);
    f32_t c = Dot(m, m) - s.radius * s.radius;
    if (c > 0.f && b > 0.f) return false;
    f32_t disc = b * b - c;
    if (disc < 0.f) return false;
    t = -b - std::sqrt(disc);
    if (t < 0.f) t = 0.f;
    return t <= r.maxDist;
}
```

#### Ray vs Capsule

세그먼트-점 거리 방정식 기반. 2차방정식 해.

### 쿼리 API

```cpp
class CRaycastQuery
{
public:
    // 첫 번째 충돌만
    bool RaycastFirst(CWorld& world, const Ray& ray, u32_t layerMask, RayHit& out);

    // 모든 충돌 (정렬된 순서)
    std::vector<RayHit> RaycastAll(CWorld& world, const Ray& ray, u32_t layerMask);

    // 용도:
    // - 마우스 픽킹 (카메라 레이 → 월드)
    // - 스킬샷 라인 (챔피언 → 방향)
    // - 시야 체크 (Line of Sight)
};
```

Broad Phase (Stage 3) 의 BVH 트리 순회와 함께 효율화.

### 오버랩 쿼리

```cpp
// 특정 영역 내 모든 엔티티 찾기 (AOE 스킬)
std::vector<EntityID> OverlapSphere(CWorld& world, const Vec3& center, f32_t radius, u32_t layerMask);
std::vector<EntityID> OverlapCapsule(CWorld& world, const Capsule& cap, u32_t layerMask);
std::vector<EntityID> OverlapAABB(CWorld& world, const AABB& box, u32_t layerMask);
```

## 변환 (Transform 적용)

ColliderComponent 의 shape 은 **로컬 공간**. 월드 공간 계산은 Transform 결합.

```cpp
// AABB 로컬 → 월드 (회전이 있으면 새 AABB 로 팽창)
AABB TransformAABB(const AABB& local, const Mat4& worldMat)
{
    Vec3 corners[8];
    for (i32_t i = 0; i < 8; ++i) {
        Vec3 c = {
            (i & 1) ? local.max.x : local.min.x,
            (i & 2) ? local.max.y : local.min.y,
            (i & 4) ? local.max.z : local.min.z
        };
        corners[i] = TransformPoint(worldMat, c);
    }
    AABB result{corners[0], corners[0]};
    for (i32_t i = 1; i < 8; ++i) {
        result.min = Min(result.min, corners[i]);
        result.max = Max(result.max, corners[i]);
    }
    return result;
}
```

## 단위 테스트

Phase 테스트 주도 개발. GoogleTest 로 각 교차 함수 커버.

```cpp
TEST(PhysicsPrimitives, AABB_vs_AABB_Overlap)
{
    AABB a{{0, 0, 0}, {2, 2, 2}};
    AABB b{{1, 1, 1}, {3, 3, 3}};
    AABB c{{3, 3, 3}, {5, 5, 5}};
    EXPECT_TRUE(Overlap(a, b));
    EXPECT_FALSE(Overlap(a, c));
}

TEST(PhysicsPrimitives, Ray_vs_Sphere_Hit)
{
    Ray r{{0, 0, 0}, {1, 0, 0}};
    Sphere s{{5, 0, 0}, 1.f};
    f32_t t;
    EXPECT_TRUE(Intersect(r, s, t));
    EXPECT_NEAR(t, 4.f, 1e-4f);
}

TEST(PhysicsPrimitives, Capsule_vs_Capsule_Penetration)
{
    Capsule a{{0, 0, 0}, {0, 2, 0}, 1.f};
    Capsule b{{1, 1, 0}, {3, 1, 0}, 1.f};
    Vec3 n; f32_t d;
    EXPECT_TRUE(Overlap(a, b, &n, &d));
    EXPECT_GT(d, 0.f);
}
```

## 성능 고려

- AABB vs AABB 는 SIMD 로 3축 동시 비교 가능
- Ray vs AABB 는 precompute `invDirection` 캐싱
- 로컬 → 월드 AABB 계산 비용 큼 → Transform dirty flag 연동해서 캐싱

## 구현 순서

1. `Vec3` / `Mat3` / `Mat4` 유틸 확인 (DirectXMath 기반)
2. AABB 구조 + `Overlap(AABB, AABB)`
3. Sphere + Capsule + 교차 함수들
4. Ray 교차 (AABB/Sphere/Capsule)
5. `ClosestPointSegmentSegment` (Ericson 알고리즘)
6. 쿼리 API (`RaycastFirst`, `OverlapSphere`)
7. 단위 테스트 (최소 30개 케이스)
8. `TransformAABB` 등 월드 변환
