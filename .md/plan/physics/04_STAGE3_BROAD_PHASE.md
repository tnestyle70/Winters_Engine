# Stage 3 — Broad Phase (SAP → Dynamic AABB Tree)

## 목표

N 개 오브젝트 간 **O(N²) 충돌 검사 회피**. 가능한 **충돌 후보 쌍 (pair)** 을 빠르게 추출.

## 왜 Broad Phase 인가

- 100 개 콜라이더 → N² = 10,000 검사. Narrow Phase 각 μs 면 10ms. 프레임 예산 과도.
- Broad Phase 로 공간 지역성 활용 → 후보 쌍 수십 개로 축소
- Raycast 도 트리 순회로 빠르게

## 알고리즘 비교

| 방법 | 빌드 | 쿼리 | 동적 업데이트 | 난이도 |
|---|---|---|---|---|
| Brute Force (N²) | — | O(N²) | — | 🟢 |
| Uniform Grid | O(N) | O(1~k) | O(1) | 🟢 |
| **Sweep and Prune** | O(N log N) | O(N + pairs) | O(log N) | 🟡 |
| Octree / Quadtree | O(N log N) | O(log N) | 느림 | 🟡 |
| **Dynamic AABB Tree** | O(N log N) | O(log N) | O(log N) | 🔴 |

**결정**: 초기 Sweep and Prune → Phase 1b 병렬화 이후 Dynamic AABB Tree.

## Sweep and Prune (SAP)

각 축에 대해 AABB 의 min/max 를 정렬 → 구간 겹침 쌍 찾기.

```cpp
struct SAPEndpoint {
    EntityID entity;
    f32_t    value;
    bool_t   isMin;
};

class CSweepAndPrune
{
public:
    void Insert(EntityID e, const AABB& box);
    void Remove(EntityID e);
    void Update(EntityID e, const AABB& newBox);

    // 현재 프레임의 충돌 후보 쌍 반환
    const std::vector<std::pair<EntityID, EntityID>>& GetPairs();

private:
    std::vector<SAPEndpoint> m_endpointsX;
    std::vector<SAPEndpoint> m_endpointsY;
    std::vector<SAPEndpoint> m_endpointsZ;

    // 현재 activePairs — 이전 프레임에서 겹쳤던 쌍 유지
    std::unordered_set<u64_t> m_activePairs;

    void InsertionSort(std::vector<SAPEndpoint>& axis);   // 이미 정렬된 배열에 점진 삽입
};
```

### 핵심 트릭: Temporal Coherence

매 프레임 **Insertion Sort** 로 업데이트 — 대부분 오브젝트는 프레임 간 크게 안 움직이므로
거의 정렬된 상태. Insertion Sort 가 O(N + k) (k = 이동 수).

정렬 과정에서 endpoint 가 교차할 때 pair 추가/제거 이벤트 발생.

### 장점
- 구현 단순
- 좁은 맵에서 매우 빠름
- 동적 업데이트 간단

### 단점
- 3축 각각 정렬 → 메모리 3배
- 한 축에 몰려있으면 성능 저하 (탑다운 MOBA 에서 Y 축이 거의 쓸모없음 → 2D SAP 로 축소 가능)

## Dynamic AABB Tree (Bullet/Box2D 방식)

이진 트리. 내부 노드 = 두 자식의 AABB 유니온. 리프 = 실제 오브젝트.

```cpp
struct TreeNode {
    AABB      aabb;
    EntityID  entity;      // 리프만 사용, 내부는 INVALID
    i32_t     parent;
    i32_t     child1, child2;
    i32_t     height;      // 균형 검사용
    bool_t    IsLeaf() const { return child1 == -1; }
};

class CDynamicAABBTree
{
public:
    i32_t Insert(const AABB& aabb, EntityID entity);
    void  Remove(i32_t proxyID);
    bool  Update(i32_t proxyID, const AABB& newAABB);   // 리턴 true = 트리 재배치 필요

    // 쿼리 — 콜백 호출
    void Query(const AABB& aabb, std::function<void(EntityID)> cb) const;
    void Raycast(const Ray& ray, std::function<bool(EntityID, f32_t& maxT)> cb) const;

    // 이전 프레임 쌍 대비 변경된 것만 (broad phase 최적화)
    std::vector<std::pair<EntityID, EntityID>> FindChangedPairs();

private:
    std::vector<TreeNode> m_nodes;
    i32_t m_rootNode = -1;
    std::vector<i32_t> m_freeList;

    void InsertLeaf(i32_t leaf);
    void RemoveLeaf(i32_t leaf);
    i32_t Balance(i32_t iA);

    f32_t ComputeCost(i32_t node, const AABB& aabb);   // SAH 기반 비용
};
```

### 삽입 알고리즘 (SAH — Surface Area Heuristic)

```
1. 리프 노드 생성
2. 루트부터 탐색:
     - 현재 노드 + 리프의 유니온 AABB 면적 = cost
     - 각 자식 방향 내려갔을 때 cost 증가량 비교
     - 작은 쪽 선택
3. 선택한 위치에 형제로 삽입
4. 상위 노드들 AABB 재계산
5. 균형 검사 (회전)
```

### Fat AABB (팽창 AABB)

매 프레임 Update 비용 줄이려고 AABB 를 여유롭게 저장:
```cpp
fatAABB.min -= Vec3(MARGIN);
fatAABB.max += Vec3(MARGIN);
```
실제 AABB 가 fatAABB 에서 벗어나지 않으면 트리 재배치 생략.

### Raycast on Tree

```cpp
void CDynamicAABBTree::Raycast(const Ray& ray, Callback cb) const
{
    std::stack<i32_t> stack;
    stack.push(m_rootNode);

    while (!stack.empty()) {
        i32_t idx = stack.top(); stack.pop();
        if (idx == -1) continue;

        const TreeNode& node = m_nodes[idx];
        f32_t tMin, tMax;
        if (!Intersect(ray, node.aabb, tMin, tMax)) continue;
        if (tMin > ray.maxDist) continue;

        if (node.IsLeaf()) {
            cb(node.entity, ray.maxDist);   // 콜백이 ray.maxDist 줄일 수 있음
        } else {
            stack.push(node.child1);
            stack.push(node.child2);
        }
    }
}
```

## 쿼리 시스템

```cpp
class CBroadPhaseSystem : public ISystem
{
public:
    void Execute(CWorld& world, f32_t dt) override
    {
        auto& bp = world.GetResource<PhysicsWorldResource>().broadPhase;

        // 1. Dirty Transform 이 있는 Collider 업데이트
        world.ForEach<ColliderComponent, TransformComponent>(
            [&](EntityID e, ColliderComponent& c, TransformComponent& t) {
            if (t.IsDirty()) {
                AABB worldAABB = ComputeWorldAABB(c, t);
                bp->Update(c.broadPhaseID, worldAABB);
            }
        });

        // 2. 이번 프레임 후보 쌍 수집
        m_candidatePairs = bp->FindChangedPairs();

        // 3. NarrowPhaseSystem 이 소비
    }

private:
    std::vector<std::pair<EntityID, EntityID>> m_candidatePairs;
};
```

## 병렬화 (Phase 1b 이후)

Dynamic AABB Tree 의 업데이트는 본질적으로 순차적. 하지만:
- **쿼리** (Raycast, Overlap) 는 병렬 가능 — 트리는 읽기 전용
- **빌드 from scratch** 시 Morton code + Radix sort 기반 병렬 빌드 가능

## MOBA 특화 — 2D SAP

탑다운 뷰이고 챔피언 높이 차이 거의 없음 → Y 축 생략:
```cpp
// 2D SAP — X, Z 축만 관리
// Y 는 Narrow Phase 에서만 체크 (지형 높이 차 등)
```
메모리 33% 절약 + 성능 향상.

## 디버그

- ImGui 에 현재 후보 쌍 수 / 활성 쌍 수 / 트리 깊이 / 균형 지수
- DebugDraw 로 AABB 트리 내부 노드 와이어프레임 (깊이별 색상)
- 클릭하면 해당 노드의 자식 엔티티 강조

## 구현 순서

1. Brute Force `CBroadPhase_Naive` (O(N²), 테스트 기준)
2. `CSweepAndPrune` — 1D 부터 시작 → 3축 확장
3. Insertion Sort 기반 Update
4. `CDynamicAABBTree` 기본 (Insert/Remove/Query)
5. SAH 기반 최적 삽입
6. Tree Balance (회전)
7. Fat AABB (Update 최적화)
8. Raycast on Tree
9. `IBroadPhase` 인터페이스로 교체 가능하게
