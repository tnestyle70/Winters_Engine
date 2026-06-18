# 08. World Partition·공간 분할·스트리밍 — 박사 연구 심화

> 이 문서는 `00_PHD_Paper_Guide.md`의 개념 틀(특히 §1 "구현 vs 기여", §3 thesis statement, §4 구조, §7 평가)을 전제로 한다.
> World Partition은 본질적으로 **렌더링·시스템·네트워크의 교차로**다. 같은 질문 — "지금 이 순간, 공간의 어느 부분이 누구에게 필요한가" — 이 클라이언트에서는 **무엇을 메모리에 올리고 무엇을 그릴까(streaming + culling)**로, 서버에서는 **무엇을 누구에게 보낼까(relevance = Area of Interest)**로 나타난다. 이 동형성(isomorphism)을 인식하는 것이 이 분야 박사의 첫 번째 통찰이며, §4에서 `10_Server`의 AoI와 직접 연결한다.

---

## 0. 이 분야를 박사로 본다는 것 (구현 vs 기여 + top venue)

### 0.1 "World Partition을 구현했다"는 박사가 아니다

가이드 §1로 즉시 돌아가자.

> ❌ "UE5처럼 grid cell + data layer + one-file-per-actor + 거리 기반 async 스트리밍 + HLOD를 다 구현했다" → 이것은 **엔지니어링 결과물**이다.

UE5 World Partition, id Tech의 MegaTexture/streaming, Frostbite의 스트리밍 시스템은 이미 존재하고 산업 표준에 가깝다. 그것을 재현하는 것은 훌륭한 시니어 엔지니어링이지만 인류 지식에 새 것을 더하지 않는다. World Partition 박사의 기여는 다음 중 하나여야 한다(가이드 §5).

- **새 알고리즘**: 같은 메모리 예산에서 hitch(끊김)를 줄이거나, 같은 hitch 예산에서 더 큰 월드를 담는 스트리밍/prefetch 스케줄링. 또는 동적 대규모 씬을 점진적으로(incrementally) 재구성하는 공간 분할 갱신 알고리즘.
- **새 자료구조**: 동적 삽입·삭제가 빈번한 대규모 씬에서 cache 지역성·GPU 친화성·병렬 갱신을 동시에 만족하는 공간 색인.
- **새 시스템/아키텍처**: 기존 설계로 불가능하던 조합 — 예: **클라이언트 렌더 컬링과 서버 relevance를 단일 공간 질의 모델로 통합**(§4의 핵심 open problem).
- **새 이론/모델**: 메모리 예산 B와 이동 속도 v가 주어졌을 때 hitch 확률의 하한, 또는 가시성 판정의 보수성(conservativeness)과 정확도의 트레이드오프를 증명.
- **새 평가 방법·벤치마크**: 스트리밍 hitch·가시성 정확도를 재현 가능하게 측정하는 표준 워크로드(과소평가되지만 강력 — 가이드 §5-6).

### 0.2 이 분야의 심장: "예산 하의 적시성"과 "보수적 over-inclusion"

World Partition 연구를 관통하는 두 긴장점.

1. **예산 하의 적시성(timeliness under budget)** — 메모리·대역폭·프레임 시간은 유한하다. 플레이어가 움직이는 속도보다 **느리게** 셀을 로드하면 빈 공간(pop-in)이나 hitch가 보인다. 빠르게 로드하면 메모리·대역폭을 초과한다. 박사 명제는 거의 항상 **"이 트레이드오프 곡선을 새 기법으로 한 칸 옮긴다"** 형태다 — "메모리 B 하에서 hitch를 Z% 줄인다".

2. **보수적 over-inclusion** — 가시성·관련성 판정은 **틀리는 방향이 비대칭**이다. "보여야 할 것을 안 보냄"(false negative)은 치명적 버그(팝핑, 적이 안 보임)이고, "안 보여도 될 것을 보냄"(false positive)은 단지 낭비다. 그래서 거의 모든 컬링·AoI·스트리밍이 **보수적(conservative)** — 의심스러우면 포함한다. 박사 각도는 "보수성을 유지하면서 over-inclusion을 줄이는 더 tight한 bound".

이 두 긴장이 §1~§4 전체를 관통한다.

### 0.3 Top Venue

가이드 §9에서 World Partition은 단일 분야가 아니라 **세 분야에 걸친다**. 어느 기여인지에 따라 venue가 갈린다.

| 기여 성격 | Top Venue |
|------|-----------|
| 가시성·LOD·임포스터·가상 지오메트리(렌더링) | **SIGGRAPH, SIGGRAPH Asia, HPG, I3D, EGSR** / ACM TOG, IEEE TVCG |
| 스트리밍·메모리·IO 스케줄링·예측 prefetch(시스템) | **SOSP, OSDI, EuroSys, ASPLOS, FAST**(스토리지), USENIX ATC |
| 서버 relevance·AoI·대규모 분산 상태(네트워킹/분산) | **NSDI, SIGCOMM, NetGames**, DISC(이론) |
| 공간 자료구조·기하 질의(계산기하) | **SoCG**(Symp. on Computational Geometry), SODA |
| 산업(비학술, 인용용) | **GDC**(UE5 World Partition, Nanite 발표), Digital Dragons, CppCon |

> **주의(가이드 §9·§203):** GDC의 UE5 World Partition·Nanite 발표는 권위 있고 영향력이 크지만 **동료심사 학술 출판이 아니다.** 설계·사례·영향력 근거로 인용하되, 1차 기여 주장은 학술 venue에 건다. Nanite의 가상화 지오메트리는 SIGGRAPH 2021 강연(Karis et al.)에 학술적 서술이 있어 인용 가능.

---

## 1. 공간 분할 자료구조 (Grid / Quadtree / Octree / BVH / Spatial Hash)

### 1.1 핵심 원리

공간 분할(spatial partitioning)의 목적은 단 하나: **"점/영역 질의를 전수 검사 O(N)에서 부분 검사로 줄인다."** "이 반경 안의 엔티티는?", "이 절두체 안의 오브젝트는?", "가장 가까운 적은?" 같은 질의를 빠르게 답하려면 공간 근접성(spatial locality)을 자료구조에 인코딩해야 한다. 모든 공간 분할은 두 축의 트레이드오프 위에 있다.

- **공간 분할(space partitioning) vs 객체 분할(object partitioning)**: grid/quadtree/octree는 **공간 자체**를 고정 규칙으로 쪼갠다(객체가 셀에 들어감 — 한 객체가 여러 셀에 걸칠 수 있음). BVH는 **객체 집합**을 감싸는 bounding volume을 재귀로 쪼갠다(볼륨이 겹칠 수 있음 — 한 객체는 한 리프에만). 이 구분이 "갱신 비용 vs 질의 품질"을 가른다.
- **균일(uniform) vs 적응적(adaptive)**: 균일 grid는 셀 크기가 고정 → 좌표 → 셀 변환이 O(1) 산술이지만, 밀도가 불균일하면 빈 셀과 과밀 셀이 공존(teapot-in-a-stadium 문제). quadtree/octree/BVH는 밀도에 적응하지만 트리 순회(pointer chasing)·갱신 비용이 든다.
- **정적(static) vs 동적(dynamic)**: 지형·건물은 정적(한 번 빌드)이라 BVH(SAH)처럼 빌드가 비싸도 질의가 좋은 구조가 유리. 움직이는 엔티티(미니언·투사체)는 매 프레임 위치가 변해 **갱신 비용**이 지배적 → grid/spatial hash처럼 O(1) 재삽입이 유리. **이 정적/동적 분리가 실무 설계의 1번 결정**이다.

**Surface Area Heuristic (SAH)** — BVH/kd-tree 빌드의 품질을 결정하는 비용 모델. 한 노드를 두 자식 L, R로 분할하는 비용을 다음으로 추정한다.

```text
Cost(split) = C_trav + (SA(L)/SA(node)) · N_L · C_isect
                     + (SA(R)/SA(node)) · N_R · C_isect
```

직관: 광선(또는 질의)이 한 자식에 들어갈 확률은 그 자식의 **표면적에 비례**(기하 확률, surface-area measure). 표면적이 작고 객체가 적은 쪽으로 쪼갤수록 기대 질의 비용이 낮다. 모든 후보 분할면에서 이 비용을 최소화 → 분할 품질의 황금률(MacDonald & Booth 1990; Wald 2007이 빠른 빌드로 일반화).

**Spatial hashing** — 무한·희소(sparse) 공간을 유한 해시 테이블로. 셀 좌표 (i, j, k)를 해시 함수로 버킷에 매핑. Teschner et al.(2003)이 제안한 고전 해시:

```text
hash(i,j,k) = (i·p1) XOR (j·p2) XOR (k·p3)  mod  tableSize
   p1, p2, p3 = 큰 서로소 소수 (예: 73856093, 19349663, 83492791)
```

핵심 장점: **월드가 무한해도 메모리는 점유 셀 수에만 비례**(빈 공간은 0 비용). 오픈월드처럼 대부분이 비어 있는 공간에 이상적. 단점: 해시 충돌(서로 먼 두 셀이 같은 버킷) → 질의 시 충돌 분리 비용.

### 1.2 대표 기존 연구/시스템

- **Teschner, Heidelberger, Müller, Pomeranets, Gross. "Optimized Spatial Hashing for Collision Detection of Deformable Objects."** *VMV 2003.* — 위 해시 함수의 출처. 변형체 충돌에서 균일 grid를 해시로 압축해 대규모 동적 씬을 다룬 정전.
- **MacDonald, D. J., Booth, K. S. "Heuristics for Ray Tracing Using Space Subdivision."** *The Visual Computer, 1990.* — SAH의 원전. / **Wald, I. "On fast Construction of SAH-based Bounding Volume Hierarchies."** *RT 2007.* — 실시간 BVH 빌드.
- **Loose octree** (Thatcher Ulrich, *Game Programming Gems*, 2000): 표준 octree는 셀 경계에 걸친 객체를 상위 노드로 밀어 올려야 해 큰 객체가 루트에 몰린다. loose octree는 셀 경계를 2배로 **느슨하게(loose)** 잡아 객체가 항상 자기 크기에 맞는 깊이의 한 노드에 들어가게 한다 → 삽입 O(1)에 가깝고 큰 객체 문제 완화. 동적 씬에 강함.
- **kd-tree / BSP**: kd-tree는 축 정렬 분할면으로 공간을 재귀 이등분. 정적 씬·광선 추적에서 SAH kd-tree가 한때 황금 표준이었으나, 동적 씬·SIMD 친화성에서 BVH에 자리를 내줌(Wald et al. 서베이).
- **Karras, T. "Maximizing Parallelism in the Construction of BVHs, Octrees, and k-d Trees."** *HPG 2012.* — Morton code(Z-order curve) 정렬로 **GPU에서 트리를 병렬 빌드**. §1.5의 "GPU 친화 구조" open problem의 토대. LBVH(Linear BVH)의 핵심.
- **R-tree / R\*-tree** (Guttman 1984; Beckmann et al. 1990): 공간 DB의 동적 색인. 게임에는 드물지만 "동적 삽입·삭제에 견고한 균형 트리"의 레퍼런스로 의미 있음.

### 1.3 자료구조/알고리즘(의사코드)

**균일 grid + spatial hash (동적 엔티티 — Winters `CSpatialIndex`가 정확히 이 모델)**:

```text
struct Cell:  vector<EntityRef> entries        # 점유 셀만 해시에 존재

# O(1) 좌표 → 셀 키 (균일 grid)
CellCoord(p):  return ( floor((p.x - origin.x)/cellSize),
                        floor((p.z - origin.z)/cellSize) )   # 2.5D: y 무시
Key(cx,cz):    return (i64)cx << 32 | (u32)cz                # 또는 Teschner 해시

# 매 프레임 전체 재빌드 (동적 씬: 갱신 = 재삽입이 단순·캐시친화)
Rebuild(world):
    map.clear()
    for e in world.entities_with(SpatialAgent):
        map[ Key(CellCoord(e.pos)) ].push_back({e.id, e.pos, e.kind, e.team})

# 반경 질의: 겹치는 셀만 순회 (전수 O(N) → O(점유 근방 셀))
QueryRadius(center, r, kindMask, excludeTeam, out):
    rCells = ceil(r / cellSize)
    (cx,cz) = CellCoord(center)
    for dz in -rCells..rCells, dx in -rCells..rCells:
        for e in map[Key(cx+dx, cz+dz)]:
            if (e.kind & kindMask) and not (e.team in excludeTeam):
                if dist2(e.pos, center) <= (r + e.radius)^2: out.push_back(e.id)
```

**왜 "매 프레임 재빌드"가 동적 씬에서 합리적인가**: 엔티티가 수천 개 수준이고 대부분 매 프레임 움직이면, 증분 갱신(이전 셀에서 제거 + 새 셀에 삽입)의 부기 비용이 단순 clear+재삽입보다 크지 않고, 재빌드는 **분기 없는 순차 쓰기**라 캐시·prefetch에 유리. 이것이 MOBA 규모(수천 엔티티, 작은 고정맵)에서 grid가 트리를 이기는 이유. **대규모 오픈월드(수십만 정적+동적)에서는 이 가정이 깨진다** → §1.5의 open problem.

**Loose octree 삽입 (큰 정적 오브젝트 포함, 동적 씬)**:

```text
# 객체 반지름 r로 들어갈 깊이를 직접 계산 (느슨한 경계 = 2 × tightBound)
Insert(obj):
    depth = floor( log2( rootSize / (2 * obj.radius) ) )      # r에 맞는 레벨
    depth = clamp(depth, 0, maxDepth)
    node  = locate node at (obj.center, depth)                # O(1) 좌표 산술
    node.objects.push_back(obj)                               # 트리 하강 불필요

QueryFrustum(node, frustum, out):
    if not frustum.Intersects(node.looseBounds): return       # loose 경계로 컬
    for o in node.objects:
        if frustum.Intersects(o.bounds): out.push_back(o)
    for c in node.children: QueryFrustum(c, frustum, out)
```

**SAH BVH 빌드 (정적 지오메트리)**:

```text
Build(prims):
    if |prims| <= LEAF_MAX: return Leaf(prims)
    bestCost = INF
    for axis in {x,y,z}:
        sort prims by centroid[axis]                          # 또는 binned(Wald): 16 bins
        sweep & 누적 SA(L), SA(R):                            # prefix/suffix area
            cost = C_trav + SA(L)/SA·N_L·C_isect + SA(R)/SA·N_R·C_isect
            track min(cost, split)
    if bestCost >= |prims|·C_isect: return Leaf(prims)        # 분할이 손해면 리프
    (L,R) = partition at best split
    return Node( Build(L), Build(R) )
```

### 1.4 박사급 novel 각도 (open problems)

1. **동적 대규모 씬의 점진적(incremental) 재구성**: 정적 BVH는 빌드가 비싸고, 동적 grid는 대규모에서 메모리·재빌드가 폭발. "프레임당 일부만 갱신하면서도 질의 품질을 보장하는" 점진적 refit/rebuild 스케줄링이 미해결. baseline = 전체 재빌드 vs refit-only(품질 열화). 측정: 질의 비용 vs 갱신 예산의 Pareto.
2. **GPU 친화 공간 구조의 질의 일반화**: Karras식 LBVH는 빌드는 GPU 병렬이나, **렌더 컬링·물리·AoI가 동시에 같은 구조를 GPU에서 질의**하는 통합은 미해결. coherent 질의(절두체)와 incoherent 질의(랜덤 반경)를 한 구조로 효율적으로.
3. **혼합 정적/동적 색인의 자동 분리**: 어떤 객체를 정적 BVH에, 어떤 것을 동적 grid에 둘지를 **이동성 통계로 자동 분류**하고, 정적이 가끔 변할 때(파괴 가능 환경) 부분 재빌드. 
4. **밀도 적응적 셀 크기**: 균일 grid의 teapot-in-a-stadium을 해결하되 좌표→셀 O(1)을 유지하는 hierarchical hashing(여러 해상도 해시 그리드 동시 유지, Eitz & Lixu 류). 어느 해상도 집합이 주어진 밀도 분포에서 기대 질의 비용을 최소화하는가.

### 1.5 Thesis statement 예시

```text
"정적 지오메트리는 SAH-BVH로, 동적 엔티티는 다중 해상도 spatial hash로 분리하고
 프레임당 갱신을 refit 예산으로 스케줄링하는 하이브리드 공간 색인은,
 10만 동적 엔티티 오픈월드에서 전체 재빌드 대비 갱신 비용을 70% 줄이면서
 반경/절두체 질의 비용을 5% 이내로 유지한다."
```

falsifiable: "갱신 비용", "질의 비용", "10만 엔티티", "전체 재빌드 baseline"이 모두 측정 가능.

### 1.6 평가 방법 (가이드 §7)

- **Metric**: 빌드/갱신 시간(ms/frame), 질의 처리량(queries/ms), 질의당 방문 노드·객체 수(품질 proxy), 메모리(MB), 질의 결과의 정확도(false negative = 0이어야 함, 보수성 검증).
- **Baseline**: 전수 검사 O(N), 균일 grid, SAH-BVH(정적), 증분 grid 갱신. 동적 씬이면 refit-only vs full-rebuild.
- **Ablation**: 셀 크기 sweep(질의 vs 갱신), 해시 해상도 개수, SAH bins 수, Morton 정렬 ON/OFF.
- **통계**: 여러 씬·밀도 분포·이동 패턴(시드), 평균이 아니라 p99 갱신 시간(프레임 hitch 주범).
- **Threats to validity**: 합성 밀도 분포가 실제 레벨을 대표하는가, 캐시 효과의 HW 의존성.

### 1.7 Winters 연결점

> Winters에는 **이미 살아있는 균일 grid + spatial hash 색인**이 있다 — `Engine/Public/ECS/SpatialIndex.h`, `Engine/Private/ECS/Systems/SpatialHashSystem.cpp`. 이것이 §1의 트레이드오프를 손으로 만질 수 있는 testbed다.

- **현 구조 = 균일 grid를 해시로 표현**: `CSpatialIndex`는 `std::unordered_map<i64_t, std::vector<CellEntry>>`에 `MakeKey(cx,cz)`로 점유 셀만 저장한다 — §1.1의 "균일 grid + 희소 해시"의 정확한 구현. `Rebuild(world)`는 **매 프레임 전체 재빌드**(§1.3 의사코드 그대로)이고, `QueryRadius`/`QueryClosest`가 `kindMask`·`excludeTeamMask`로 필터한다. 2.5D(y 무시)인 것까지 MOBA 평면 가정과 일치.
- **MOBA = 작은 고정맵 baseline이 코드에 박혀 있음**: `LoLSpatialGridDesc()`는 `cellSize=8m, halfExtent=32` → **64×64 = 4096 셀의 작은 고정 그리드**. 이것이 §0.2의 "예산 하 적시성"이 **무시 가능한** 한쪽 극단(전부 메모리에 상주, 스트리밍 불필요). 박사 실험의 **대조군**으로 완벽하다.
- **오픈월드 = 반대 극단**: `.md/EldenRing/04_WORLD_PARTITIONING_STREAMING.md`의 `cellSizeMeters=64`, grid cell + data layer는 §1.5의 "대규모 동적 씬" 쪽. **동일 엔진에서 두 모드를 한 코드베이스로 비교**할 수 있다는 것이 Winters testbed의 결정적 강점(가이드 §7 ablation: 맵 규모를 독립변수로).
- **`DebugStats`가 곧 측정 인프라**: `CSpatialIndex::DebugStats{ totalEntities, occupiedCells, maxEntitiesInCell }`와 `GetOccupiedCellCenters`는 §1.6의 "셀 점유 분포·과밀 셀" metric을 **공짜로 제공**. CLAUDE.md가 의무화한 inspectable overlay 문화(셀 점유 시각화)가 곧 평가 도구.
- **다중 질의자 = §1.4-2의 동기**: `CVisionSystem`이 같은 `CSpatialIndex*`를 받아 vision 질의에 쓴다(`VisionSystem.h`). 즉 컬링·AI·vision이 **이미 하나의 색인을 공유** → "단일 구조로 여러 질의 유형" 연구가 자연스럽다.

---

## 2. 거리 기반 스트리밍과 셀 관리 (UE5 World Partition 모델)

### 2.1 핵심 원리

스트리밍의 본질: **"플레이어 주변 작업 집합(working set)만 메모리에 상주시키고, 멀어지면 내린다."** OS의 demand paging과 같은 발상이되, 게임은 **(a) 미래 접근을 공간적으로 예측 가능**(플레이어는 순간이동하지 않고 연속 이동)하고 **(b) hitch가 곧 품질 저하**라는 hard 실시간 제약이 다르다.

핵심 메커니즘 세 가지.

1. **거리 기반 로드/언로드 + hysteresis(이력)**: streaming source(플레이어/카메라)로부터의 거리로 셀 상태를 결정하되, **로드 반경 < 언로드 반경**으로 띄워(hysteresis) 경계에서 왔다 갔다 할 때 로드/언로드가 진동(thrashing)하지 않게 한다. Winters EldenRing 문서의 `loadRadius=160 < visibleRadius=220 < unloadRadius=280`이 정확히 이 패턴.
2. **다단계 비동기 파이프라인 + 프레임 예산**: 디스크 IO → 디코드/파싱 → 의존성 해소 → **GPU 업로드** → 엔티티 인스턴스화. DX11에서 GPU 리소스 생성은 device context에 묶여 워커 스레드에서 끝낼 수 없으므로(`07_ASSET_LOADER` §GPU Upload Queue), 워커는 파싱·중간 버퍼만, 메인/렌더 스레드의 upload phase에서 **프레임당 N개·M바이트 예산**으로 나눠 올린다 → 이것이 hitch를 막는 핵심.
3. **분리된 로드/표시(load ≠ show)**: 셀을 `LoadedHidden`(메모리엔 있으나 안 보임) 상태로 미리 올려두고, 실제로 보일 때 `Visible`로 전환. 이 분리가 **예측 prefetch**의 토대.

**UE5 World Partition 모델의 세 기둥**:

- **One File Per Actor (OFPA)**: 거대한 단일 레벨 파일을 액터마다 파일로 쪼갠다. 동기 = **협업(소스 컨트롤 충돌 감소)** + **선택적 로드**(필요한 액터 파일만). 스트리밍의 IO 단위를 액터 수준으로 미세화.
- **Runtime Grid / Runtime Hash**: 에디터의 액터 배치를 빌드 타임에 **셀로 분배(partition)**하고, 런타임엔 위치 → 셀 해시로 O(1) 조회. 여러 grid(예: 작은 디테일용 촘촘한 grid + 큰 랜드마크용 성긴 grid)를 레이어로.
- **Data Layers**: 같은 공간 셀 안에서도 **상황(낮/밤, 퀘스트 단계, 레이드 페이즈)**에 따라 액터 집합을 켜고 끈다 → 공간 분할과 직교(orthogonal)하는 두 번째 축. Winters `DataLayerComponent{ layerMask }`(EldenRing 문서)가 이것.

### 2.2 대표 기존 연구/시스템

- **Epic Games. "World Partition" (UE5 documentation & GDC 강연들).** — OFPA + runtime hash + data layer + HLOD의 산업 정전. 학술 아님(가이드 §9), 설계 레퍼런스로 인용.
- **Demand paging / working set 이론** (Denning, P. J. "The Working Set Model for Program Behavior." *CACM 1968*): 스트리밍의 이론적 조상. "최근 Δ 시간 동안 접근한 페이지 집합을 상주"가 곧 "최근 방문 셀을 상주". prefetch·예측 로딩의 OS 측 뿌리.
- **id Tech MegaTexture / Virtual Texturing** (Sean Barrett의 sparse virtual texture; van Waveren의 id Tech 5 발표): 텍스처를 타일로 쪼개 화면에 보이는 타일만 스트리밍. 가상 메모리를 그래픽스 자원에 적용한 고전 → 가상 지오메트리(Nanite, §3)의 사상적 선조.
- **RAGE / Frostbite / Decima 등 오픈월드 엔진의 스트리밍**: GDC 사례 다수. 공통적으로 (a) 타일/섹터 기반 (b) 우선순위 큐 (c) 거리·시야 기반 prefetch.
- **예측 prefetch 학술**: 디스크/스토리지 prefetch의 패턴 예측(Markov prefetcher 등, FAST/ATC 계열)과 게임의 공간적 예측을 잇는 연구는 의외로 희소 → §2.4의 기회.

### 2.3 자료구조/알고리즘(의사코드)

**거리 기반 셀 스케줄러 (다중 source union + hysteresis + 우선순위 — Winters `CCellStreamingScheduler` 모델)**:

```text
# 셀 상태 (EldenRing 문서 eWorldCellState)
enum CellState { Unloaded, Queued, LoadingMeta, LoadingAssets, CreatingEntities,
                 LoadedHidden, Visible, Unloading }

Update(sources, cells, budget):
    # 1) 모든 streaming source의 요구를 union (player + camera + boss arena + editor)
    want = {}                                   # cellId → max priority
    for s in sources:
        for c in cells_within(s.pos, s.loadRadius):
            want[c] = max(want[c], priorityOf(s, dist(s.pos, c.center)))

    # 2) 상태 전이 (hysteresis: load < unload 반경으로 thrash 방지)
    for c in cells:
        if c.id in want and c.state == Unloaded:
            enqueue_load(c, priority = want[c.id])      # Queued
        elif c.id not in want and dist_to_nearest_source(c) > c.unloadRadius:
            if c.state == Visible or c.state == LoadedHidden:
                begin_unload(c)                          # Unloading → Unloaded
        # 표시/숨김은 visibleRadius로 별도 판정 (load ≠ show)
        c.visible = (dist_to_nearest_source(c) <= visibleRadius)

    # 3) 프레임 예산 안에서만 진행 (hitch 방지의 핵심)
    spentBytes = 0; spentJobs = 0
    for c in load_queue sorted by priority desc:
        if spentJobs >= budget.maxJobsPerFrame: break
        if spentBytes + c.estBytes > budget.maxBytesPerFrame: continue
        step_load_pipeline(c)                            # 한 단계만 진행
        spentBytes += c.uploadedThisStep; spentJobs += 1
```

**예측 prefetch (속도 벡터 외삽 — open problem의 단순 baseline)**:

```text
Prefetch(player, dt, lookaheadSeconds):
    predicted = player.pos + player.velocity * lookaheadSeconds   # 1차 외삽
    # 이동 방향 셀을 미리 LoadedHidden으로 (낮은 우선순위로 큐)
    for c in cells_within(predicted, loadRadius):
        if c.state == Unloaded:
            enqueue_load(c, priority = PREFETCH_PRIORITY)   # gameplay보다 낮게
    # 카메라 시야 방향도 동일 (camera source의 forward로 prefetch)
```

**Required vs Optional 게이팅 (hitch 없이 표시 — `07_ASSET_LOADER` §Fallback)**:

```text
# 셀을 보이게 하는 최소 집합만 기다리고, 나머지는 도착하면 채운다
CanShow(cell):
    return all(a.state == Ready for a in cell.requiredAssets)   # collision, near mesh, fallback mat
ShowCell(cell):
    for a in cell.optionalAssets:                               # hi-res tex, far decal, ambient FX
        if a.state != Ready: use_fallback(a)                    # gray tex / default mat / skip FX
```

### 2.4 박사급 novel 각도 (open problems)

1. **메모리 예산 하 hitch 제거의 이론·알고리즘**: "메모리 B, 이동 속도 분포 V, 디스크 대역폭 D가 주어졌을 때 hitch 확률의 하한은?" 그리고 그 하한에 다가가는 스케줄러. 이는 **online competitive analysis**(미래를 모르는 캐시 교체)와 **공간 예측**의 결합 — 순수 LRU가 아니라 "공간적으로 곧 필요할 것"을 아는 캐시. 측정: 같은 B에서 hitch 횟수.
2. **학습 기반 예측 prefetch**: 1차 속도 외삽을 넘어, 플레이어의 **이동 패턴·미니맵 핑·목표 지점**을 신호로 다음 방문 셀을 예측(공간 Markov / 경량 시퀀스 모델). 데이터센터 prefetch 연구를 게임 공간에 일반화. baseline = 거리-only, 속도 외삽.
3. **절차적(procedural) 스트리밍**: 디스크에서 읽는 대신 **시드에서 절차적으로 생성**하는 콘텐츠(지형·식생)의 스트리밍. "생성 비용 vs 저장 비용" 트레이드오프, 결정론적 생성으로 네트워크 동기화(서버·클라가 같은 시드로 같은 월드).
4. **통합 LOD-스트리밍 스케줄러**: 셀을 "올린다/내린다"의 이진이 아니라, **어느 LOD로 올릴지**(§3)를 예산 안에서 동시 결정 — far 셀은 임포스터만, near는 풀 메시. 메모리·대역폭·시각 품질의 3차원 최적화.

### 2.5 Thesis statement 예시

```text
"플레이어 이동을 공간 Markov 모델로 예측해 prefetch 우선순위를 정하는 스트리밍 스케줄러는,
 고정 메모리 예산(예: 512MB working set) 하에서 거리 기반 LRU prefetch 대비
 90퍼센타일 카메라 이동 시 hitch(>2ms frame spike) 발생을 60% 줄이면서
 pop-in(빈 셀 노출) 프레임 비율을 동등하게 유지한다."
```

### 2.6 평가 방법 (가이드 §7)

- **Metric**: **스트리밍 지연/끊김(hitch)** — 셀 전환 시 frame time spike의 횟수·크기(p99/p99.9, 평균이 아니라 꼬리), pop-in 프레임 비율(요구되나 아직 없는 셀이 화면에 든 프레임), **메모리 예산** 준수(peak working set MB, 예산 초과 횟수), 로드 지연(요구 시점 → Ready 시점), 디스크/디코드 대역폭(MB/s).
- **Baseline**: 전부 상주(스트리밍 없음, hitch 0 메모리 ∞), 거리-only 로드(예측 없음), LRU 캐시, 속도 외삽 prefetch.
- **Ablation**: hysteresis ON/OFF(thrashing 측정), prefetch ON/OFF, required/optional 게이팅 ON/OFF, 프레임 예산 sweep(예산 ↓ → hitch ↑ 곡선).
- **재현성**: **결정론적 카메라 경로(camera fly-through)**로 같은 입력을 반복 — Winters의 결정론 리플레이(`10_Server`·프로파일러 문서 참조)가 있으면 prefetch ON/OFF를 byte-identical 입력으로 비교 가능(가이드 §7 강력한 무기). HW(디스크 NVMe/HDD)·씬 명시.
- **Threats to validity**: 합성 카메라 경로가 실제 플레이를 대표하는가, 디스크 캐시 warm/cold 상태 통제.

### 2.7 Winters 연결점

> 스트리밍의 **설계 청사진이 이미 문서로 깔려 있다** — `.md/EldenRing/04_WORLD_PARTITIONING_STREAMING.md`(셀 상태·source·HLOD)와 `07_ASSET_LOADER_AND_STREAMING_RUNTIME.md`(async IO·GPU 업로드·예산). 박사는 이 청사진의 **스케줄링 정책**을 연구 대상으로 삼는다.

- **셀 상태 기계가 이미 명세됨**: `eWorldCellState{ Unloaded … Visible … Unloading }`와 전이가 04 문서에 못박혀 있다 → §2.3 스케줄러의 상태 공간이 코드 레벨로 준비됨. `CCellStreamingScheduler`가 연구 대상 컴포넌트.
- **hysteresis가 데이터로 존재**: `StreamingSourceComponent{ loadRadius=160, visibleRadius=220, unloadRadius=280 }`는 §2.1 hysteresis의 실측 파라미터 → "이 반경 비율이 thrashing/메모리에 미치는 영향"이 즉석 ablation.
- **프레임 예산이 명시됨 = hitch metric의 직접 입력**: `07` 문서가 `max upload jobs per frame=4`, `max upload bytes per frame=16MB`, `max blocking load ms=5ms`를 못박는다 → §2.6의 "예산 하 hitch" 실험의 독립변수가 코드에 있다. 우선순위 표(`1000 player … 100 far HLOD`)도 §2.3 priority의 ground truth.
- **Required/Optional·Fallback 정책 명시**: `07` 문서의 fallback 표(mesh→placeholder, texture→gray, FX→skip)가 §2.3 게이팅 의사코드 그대로 → pop-in vs hitch 트레이드오프 실험 가능.
- **Debug Panel = 측정 인프라**: `07` 문서의 `Asset Streaming` 패널(queued/loading/failed/GPU upload bytes/cache memory/ref counts)과 `04` 문서의 World Partition 패널(loaded cell 색상·budget)이 §2.6 metric을 실시간 노출 → CLAUDE.md inspectable overlay 의무가 곧 평가 도구.
- **두 모드 대비 실험**: MOBA(작은 고정맵, 스트리밍 불필요, §1.7 `LoLSpatialGridDesc`) vs 오픈월드(64m 셀 스트리밍) → "스트리밍이 필요해지는 임계 월드 크기"를 동일 엔진에서 측정(가이드 §7).

---

## 3. 계층적 LOD (HLOD)와 임포스터(Impostor)

### 3.1 핵심 원리

LOD(Level of Detail)의 동기: **화면에서 작게 보이는 것에 풀 디테일을 쓰는 것은 낭비**(서브픽셀 삼각형은 GPU 래스터라이저를 굶긴다 — quad overdraw, 4×4 픽셀 미만 삼각형의 셰이딩 비효율). 거리에 따라 더 거친 표현으로 바꾼다. 세 계층.

1. **Discrete LOD (LOD0/1/2…)**: 같은 메시의 단순화 버전들을 미리 만들어 거리로 교체. 단점: 교체 순간 **팝핑(popping)** — 정점이 갑자기 사라짐.
2. **Hierarchical LOD (HLOD)**: 개별 메시 LOD를 넘어, **여러 오브젝트를 하나의 프록시 메시로 병합**. 먼 마을 = 집 100채 대신 "마을 프록시 메시 1개 + 1 draw call". draw call 수와 정점 수를 동시에 줄임 → **CPU(draw call) 병목과 GPU(정점) 병목을 함께** 공략. 공간 분할의 셀 계층과 자연히 결합(셀 → 그 자식 셀들의 프록시).
3. **Impostor / Billboard**: 3D 메시를 **2D 이미지(텍스처)**로 대체. 가장 거친 LOD. octahedral impostor는 여러 각도에서 미리 렌더한 아틀라스를 뷰 방향으로 블렌딩해 시차(parallax)를 흉내. 나무 숲처럼 수천 개 인스턴스에 결정적.

**Quadric Error Metrics (QEM) — 메시 단순화의 황금 표준** (Garland & Heckbert, SIGGRAPH 1997). 정점 v의 "오차"를 그 정점에 인접한 평면들까지의 **거리 제곱 합**으로 정의하고, 이를 4×4 대칭 행렬 Q(quadric)로 인코딩.

```text
한 평면 p = [a b c d]ᵀ  (ax+by+cz+d=0, 단위 법선),  점 v=[x y z 1]ᵀ
점→평면 거리² = (pᵀv)² = vᵀ (p pᵀ) v
정점 v의 quadric: Q(v) = Σ_{인접 평면 p} p pᵀ       # 4×4 대칭 행렬

# 간선 (v1,v2)를 한 점 v̄로 축약(collapse)하는 비용:
Q̄ = Q(v1) + Q(v2)
cost(collapse) = v̄ᵀ Q̄ v̄          # v̄는 ∂(v̄ᵀQ̄v̄)/∂v̄ = 0 푸는 최적점(Q̄ 역행렬), 또는 중점
```

알고리즘: 모든 간선의 collapse 비용을 우선순위 큐에 넣고, **가장 싼 간선부터 축약**하며 목표 삼각형 수까지 반복. quadric은 덧셈으로 누적되므로 빠르고, 결과 품질이 시각적으로 우수해 25년째 표준.

**Nanite — 가상화 지오메트리(Virtualized Geometry)** (Karis, Stachowiak et al., "A Deep Dive into Nanite Virtualized Geometry", SIGGRAPH 2021 Advances in Real-Time Rendering 강연). LOD의 패러다임 전환: 메시를 **클러스터(cluster, ~128 삼각형 그룹)**로 쪼개고, 클러스터들을 계층적으로 단순화해 **DAG(directed acyclic graph)**로 묶는다. 런타임에 화면 공간 오차(screen-space error)가 픽셀 단위 임계를 넘지 않는 **클러스터 cut**을 DAG에서 선택 → 한 메시 안에서도 부위별로 다른 LOD가 **이음새 없이(seam-free)** 공존, 픽셀당 약 1 삼각형으로 수렴. GPU 컴퓨트 기반 소프트웨어 래스터라이저로 작은 삼각형을 효율 처리. 핵심: **개별 LOD 모델·HLOD 수작업 프록시를 상당 부분 불필요하게** 만든다(정적 지오메트리 한정).

### 3.2 대표 기존 연구/시스템

- **Garland, M., Heckbert, P. "Surface Simplification Using Quadric Error Metrics."** *SIGGRAPH 1997.* — QEM 원전. 메시 단순화의 표준.
- **Hoppe, H. "Progressive Meshes."** *SIGGRAPH 1996.* — 연속(continuous) LOD. 간선 축약/정점 분할의 가역 시퀀스로 연속적 디테일 조절·점진적 전송. QEM과 함께 단순화의 두 기둥.
- **Luebke et al. *Level of Detail for 3D Graphics* (Morgan Kaufmann, 2002)** — LOD 분야의 표준 교과서(view-dependent LOD, HLOD 개념 포함).
- **Karis, B. et al. "A Deep Dive into Nanite Virtualized Geometry."** *SIGGRAPH 2021 Course (Advances in Real-Time Rendering).* — 가상화 지오메트리. cluster DAG·screen-space error LOD selection·software raster.
- **Octahedral Impostors** (Ryan Brucks/Epic; "Deep G-Buffer"·octahedral imposter 기법들): 임포스터 아틀라스를 octahedral 매핑으로 전 방향 커버, 뷰 의존 블렌딩으로 팝핑 완화.
- **Cignoni et al. "BDAM/Adaptive TetraPuzzles"** (지형·대규모 메시의 배치 단위 다해상도): 대규모 지형 LOD의 학술 계보.

### 3.3 자료구조/알고리즘(의사코드)

**QEM 메시 단순화 (HLOD 프록시 자동 생성)**:

```text
Simplify(mesh, targetTris):
    for v in vertices: Q[v] = Σ_{면 f∋v} plane(f)·plane(f)ᵀ      # 4×4 quadric
    pq = priority_queue()
    for edge (a,b): pq.push( cost = optimalCost(Q[a]+Q[b]), edge )
    while mesh.triCount > targetTris and not pq.empty():
        (cost, (a,b)) = pq.pop_min()
        if stale(a,b): continue                                 # 위상 변경으로 무효화된 항목
        v̄ = solve(Q[a]+Q[b])                                    # 최적 위치 (특이행렬이면 중점)
        collapse(a,b → v̄);  Q[v̄] = Q[a]+Q[b]
        for n in neighbors(v̄): pq.update(cost(Q[v̄]+Q[n]), (v̄,n))
```

**HLOD 트리 빌드 + 런타임 선택 (셀 계층과 결합)**:

```text
# 빌드 타임: 셀 octree/grid의 각 내부 노드에 자식들의 병합 프록시 생성
BuildHLOD(node):
    if node.isLeaf: node.proxy = node.meshes; return
    for c in node.children: BuildHLOD(c)
    merged = merge_meshes(collect_meshes(node.children))        # 정점 병합 + 텍스처 아틀라스
    node.proxy = Simplify(merged, targetTris(node.depth))       # QEM으로 단순화

# 런타임: 화면 공간 오차 기반 트리 cut 선택 (Nanite식 일반화)
SelectLOD(node, cam, out):
    err = node.geometricError * projectToScreen(node.bounds, cam) # 픽셀 단위 오차
    if err <= PIXEL_THRESHOLD or node.isLeaf:
        out.draw(node.proxy)                                    # 이 레벨로 충분 → 컷
    else:
        for c in node.children: SelectLOD(c, cam, out)          # 더 내려감
```

**팝핑 제거 — LOD 디더 크로스페이드(dither cross-fade)**:

```text
# 두 LOD 사이 전이 구간에서 둘을 stipple/dither 알파로 겹쳐 그림
DrawWithFade(obj, cam):
    (loA, loB, t) = lodPair(distance(obj, cam))                # t∈[0,1] 전이 진행도
    draw(loA, ditherAlpha = 1 - t)                              # screen-door / temporal dither
    draw(loB, ditherAlpha = t)                                 # TAA가 디더를 매끈하게 해소
```

### 3.4 박사급 novel 각도 (open problems)

1. **자동 프록시 품질 보장**: HLOD 프록시 자동 생성(QEM 병합)이 **시각적으로 충분한지**를 화면 공간 오차로 보장하고, 실루엣·UV seam·재질 경계를 보존하는 단순화. "주어진 픽셀 오차 예산에서 지각적 차이(FLIP/ΔE)를 최소화하는 프록시"가 미해결.
2. **팝핑의 지각적 제거**: discrete LOD 전환·임포스터↔메시 전환의 팝핑을 **temporal(TAA/dither)·기하(geomorph) 결합**으로 제거하되, 비용을 최소화. "어느 전이 기법이 어느 콘텐츠에서 팝핑을 가장 적은 비용으로 가리는가" user study(가이드 §7 정성).
3. **런타임 HLOD/임포스터 생성**: 절차적·동적 월드에서 프록시를 **빌드 타임이 아니라 런타임에** 생성(GPU 단순화·실시간 임포스터 베이크). 스트리밍(§2)과 결합 — 멀어지는 셀의 프록시를 그 자리에서 굽는다.
4. **동적 오브젝트의 HLOD**: Nanite·HLOD는 정적 지오메트리 전제. 군중·식생·파괴 가능 환경 같은 **동적·인스턴스 대량 오브젝트**의 계층적 LOD는 미해결(skinned mesh impostor, 군중 LOD).

### 3.5 Thesis statement 예시

```text
"셀 계층에서 QEM으로 자동 병합한 HLOD 프록시를 화면 공간 오차로 선택하고
 디더-temporal 전이로 잇는 파이프라인은, 오픈월드 원경에서 수작업 프록시 대비
 draw call을 80% 줄이면서 지각적 차이(FLIP)를 임계 이하로 유지하고
 LOD 전환 팝핑을 정지·이동 양쪽에서 제거한다."
```

### 3.6 평가 방법 (가이드 §7)

- **Metric**: **가시 품질** — PSNR/SSIM/FLIP(원경 프록시 vs 풀 디테일 ground truth), 팝핑(연속 프레임 간 픽셀 변화 spike, temporal flicker), 성능(draw call 수, 정점/삼각형 수, frame time), 메모리(프록시·임포스터 아틀라스 MB), 자동 생성 시간.
- **Baseline**: LOD 없음(풀 디테일), discrete LOD only, 수작업 HLOD 프록시, (정적이면) Nanite.
- **Ablation**: QEM target 삼각형 sweep(품질 vs 비용), 디더 전이 ON/OFF(팝핑), 임포스터 각도 수 sweep.
- **정성**: 팝핑 가시성·전이 자연스러움 user study(IRB, 가이드 §10).
- **Threats to validity**: 특정 콘텐츠(건물 vs 자연물)에 편향, FLIP이 게임 지각을 대표하는가.

### 3.7 Winters 연결점

> Winters는 **MOBA(고정 카메라, 작은 맵 → LOD 압박 적음)**와 **오픈월드(자유 카메라, 원경 → HLOD 필수)**를 모두 가져, LOD/HLOD가 **언제 필요한지**를 대조 실험할 수 있다.

- **HLOD가 스트리밍 청사진에 명세됨**: `04_WORLD_PARTITIONING_STREAMING.md`의 HLOD 전략 표(near=실제 entity, mid=static만, far=HLOD proxy, very far=culled)와 셀별 `hlod: "HLOD/x000_z000_hlod.wmesh"` 필드 → §3.3 HLOD 트리의 셀-레벨 매핑이 데이터로 존재. 초기 단계는 "수동 `.wmesh` 1개"(04 문서) → 박사 각도는 이를 **QEM 자동 생성**으로(§3.4-1).
- **메시 에셋·렌더러 인프라가 있음**: `Engine/Public/Renderer/ModelRenderer.h`(현재 diff에 수정 중)와 `FxStaticMeshRenderer`가 정적 메시 경로를 가짐 → 프록시 메시 LOD 선택·드로우를 얹을 지점. `.wmesh` 포맷이 LOD 체인을 담도록 확장하는 것이 자연스러운 챕터.
- **스트리밍과 결합 = §3.4-3 testbed**: §2.7의 프레임 예산·셀 상태 기계 위에서 "far 셀은 임포스터만 로드"하는 통합 LOD-스트리밍(§2.4-4)을 실험. far HLOD가 우선순위 표에서 `priority=100`(07 문서)으로 이미 최하위 → 예산 경합 실험의 입력.
- **두 모드 품질 대조**: MOBA는 카메라가 고정 고도·하향(top-down)이라 LOD 거리 분포가 좁음 vs 오픈월드는 지평선까지 → "카메라 모델이 LOD 정책에 미치는 영향"을 동일 엔진에서(가이드 §7).

---

## 4. 가시성·관련성(Relevance): 컬링과 서버측 관련성(AoI 연결)

### 4.1 핵심 원리 — 컬링과 relevance는 같은 질문의 두 얼굴

이 절의 중심 통찰(§0.1 재진술): **클라이언트 렌더 컬링과 서버 relevance는 동형(isomorphic)이다.**

- **클라이언트(렌더)**: "이 카메라에게 이 프레임에 **보이는** 오브젝트는?" → 안 보이면 **안 그린다**(GPU 절약).
- **서버(네트워크)**: "이 플레이어에게 지금 **관련 있는(relevant)** 엔티티는?" → 관련 없으면 **상태를 안 보낸다**(대역폭 절약). 이것이 **Area of Interest (AoI)**이고 `10_Server`의 핵심 주제다.

둘 다 **"관찰자(observer)를 중심으로 한 공간 질의"**이며, §0.2의 보수적 over-inclusion(false negative = 치명적)을 공유한다. 차이는 비용 모델(GPU draw vs 네트워크 byte)과 시간 척도(매 프레임 vs 매 tick)뿐. 박사 각도(§4.4-1)는 이 둘을 **하나의 공간 질의 엔진**으로 통합하는 것.

**클라이언트 컬링 4단계** (보수적·계층적):

1. **Frustum culling**: 카메라 절두체(6 평면) 밖 = 안 그림. AABB/구를 6 평면에 대해 검사. 공간 분할(§1)로 가속(절두체와 겹치는 셀만 검사).
2. **Occlusion culling**: 절두체 안이지만 **다른 물체에 가려진** 것 = 안 그림. **Hierarchical Z-Buffer (HZB)** (Greene, Kass, Miller, SIGGRAPH 1993): depth buffer의 mip 피라미드를 만들어, 오브젝트 AABB의 화면 영역을 덮는 mip 레벨의 가장 먼 depth와 비교 — 그보다 멀면 가려짐. **GPU occlusion culling**(Hi-Z + indirect draw)이 현대 표준.
3. **Portal culling**: 실내(방-문) 구조에서 **포털(문)**을 통해서만 다음 방이 보임 → 포털 절두체를 좁혀가며 가시 셀 전파. 셀-포털 그래프.
4. **Backface / small-triangle / detail culling**: 뒷면·서브픽셀·디테일 컬.

**Potentially Visible Set (PVS)** — 정적 씬에서 "각 영역(셀)에서 보일 수 있는 셀 집합"을 **사전 계산**(Quake의 BSP PVS가 고전). 런타임엔 조회만 → 동적 씬엔 부적합하나 정적 가시성의 상한 레퍼런스.

**서버 relevance / AoI 메커니즘**:

- **Grid/cell 기반 AoI**: 플레이어 주변 N×N 셀의 엔티티만 relevant. §1의 spatial hash 그대로 재사용 → "렌더 컬링과 AoI가 같은 색인" 동형성의 직접 증거.
- **관심 반경 + delta**: relevant set의 변화(들어옴/나감)를 delta로 전송. Winters `02_UDP..._DELTA_AOI` 문서의 added/changed/removed가 정확히 이것.
- **시야(line of sight)·fog of war**: MOBA는 거리뿐 아니라 **시야 차단(부시·지형)**이 relevance를 결정 → 가려진 적은 안 보내거나(정보 은폐 = anti-cheat 가치) 위치만 흐리게.

### 4.2 대표 기존 연구/시스템

- **Greene, N., Kass, M., Miller, G. "Hierarchical Z-Buffer Visibility."** *SIGGRAPH 1993.* — HZB occlusion culling 원전.
- **Teller, S., Séquin, C. "Visibility Preprocessing for Interactive Walkthroughs."** *SIGGRAPH 1991.* — 셀-포털 PVS의 정전.
- **Cohen-Or, D., Chrysanthou, Y., Silva, C., Durand, F. "A Survey of Visibility for Walkthrough Applications."** *IEEE TVCG 2003.* — 가시성·컬링 분야의 표준 서베이.
- **Hierarchical/GPU-driven culling**: GPU-driven rendering pipeline(Haar & Aaltonen, "GPU-Driven Rendering Pipelines", SIGGRAPH 2015 강연; Assassin's Creed/Frostbite 사례) — Hi-Z + compute culling + indirect draw로 CPU에서 GPU로 컬링 이양.
- **AoI / interest management (네트워크)**: Singhal & Zyda, *Networked Virtual Environments* (1999) — DIS/HLA의 interest management. **Aura/Nimbus 모델**(Benford & Fahlén, ECSCW 1993): 객체의 "aura"(영향 범위)와 관찰자의 "nimbus"(인지 범위)가 겹칠 때 relevant — relevance의 일반 이론. MMO의 grid AoI·sharding은 NSDI/NetGames 계열.
- **Donnybrook / Colyseus / 분산 상태**: 대규모 동시성에서 관심 기반 상태 분배(NSDI 계열). → `10_Server`와 직접 연결.

### 4.3 자료구조/알고리즘(의사코드)

**Hierarchical frustum culling (공간 분할 + 절두체)**:

```text
CullFrustum(node, frustum, out):
    r = frustum.Classify(node.bounds)               # Inside / Outside / Intersect
    if r == Outside: return                         # 노드 전체 컬 (자식 검사 생략)
    if r == Inside:                                 # 노드 전체 가시 → 평면 검사 생략하고 수집
        collect_all(node, out); return
    for o in node.objects:                          # 경계 노드만 개별 검사
        if frustum.Intersects(o.bounds): out.push_back(o)
    for c in node.children: CullFrustum(c, frustum, out)
```

**GPU occlusion culling (Hi-Z, 2-phase — 현대 표준)**:

```text
# Phase 1: 지난 프레임 가시 집합으로 깊이 그림 → Hi-Z 피라미드 빌드
draw(lastVisible);  HiZ = build_mip_pyramid(depthBuffer)
# Phase 2: 모든 오브젝트를 compute로 Hi-Z 테스트
for o in all_objects (parallel on GPU):
    rect = projectAABB(o.bounds, viewProj)          # 화면 사각형
    mip  = chooseMip(rect)                          # 사각형을 1~4 텍셀로 덮는 레벨
    if o.minDepth > HiZ.sample_farthest(rect, mip): o.culled = true   # 보수적
    else append(o, drawList)                        # indirect draw 인자에 추가
DrawIndirect(drawList)                              # CPU 개입 없이 GPU가 그림
```

**서버측 AoI — 렌더 컬링과 동일한 spatial hash 재사용 (동형성의 코드적 증명)**:

```text
# 같은 CSpatialIndex를 서버 tick에서 relevance에 사용
ComputeRelevant(player, index, fow):
    candidates = []
    index.QueryRadius(player.pos, AOI_RADIUS, ENTITY_MASK, /*excludeTeam*/0, candidates)
    relevant = []
    for e in candidates:
        if fow.IsVisible(player.team, e.pos)        # 시야(부시/지형) 차단 검사
           or e.alwaysRelevant:                     # 구조물·목표는 항상
            relevant.push_back(e)
    return relevant                                 # → SnapshotBuilder의 AOI 입력

# delta: 직전 tick relevant set과 비교 (02_UDP..._DELTA_AOI)
BuildDelta(prev, cur, snapshot):
    added   = cur - prev;  removed = prev - cur;  changed = {e∈cur∩prev : e.dirty}
    snapshot.write(added, changed, removed)         # removed로 client stale entity 정리
```

### 4.4 박사급 novel 각도 (open problems)

1. **클라 컬링 ⊕ 서버 relevance 통합 모델 [이 문서의 간판 open problem]**: 같은 공간 질의 엔진이 **(a) 클라에서 절두체+occlusion 컬링** (b) **서버에서 AoI relevance**를 동시에 제공하는 통합 아키텍처. 비용 모델(GPU draw vs network byte)을 파라미터화한 **단일 relevance 함수** R(observer, entity) → {render, replicate, both}. 기존엔 두 시스템이 독립 구현 → 일관성 버그(클라엔 보이는데 서버가 안 보냄)·중복 작업. 가이드 §5-3(새 아키텍처) 색.
2. **occlusion 기반 AoI(시야 차단 relevance)**: 거리 AoI를 넘어 **실제 가시선(LoS)·occlusion**으로 relevance를 좁힌다 — 벽 뒤 적은 안 보냄(대역폭↓ + **정보 은폐로 wallhack 차단**, `11_Security`와 교차). open: 서버에서 occlusion을 싸게 보수적으로 판정.
3. **보수성-tightness 트레이드오프의 정량 한계**: 보수적 컬링/AoI는 over-include한다. "false negative = 0을 보장하면서 over-inclusion을 최소화하는" bound의 이론적 한계와, 그에 다가가는 알고리즘. 측정: over-draw 비율·과전송 byte.
4. **예측적 relevance(prefetch와 통합)**: 곧 보일/관련될 엔티티를 미리 stream-in(클라) 또는 pre-replicate(서버) — §2.4-2 예측 prefetch를 relevance로 일반화.

### 4.5 Thesis statement 예시

```text
"클라이언트 절두체·occlusion 컬링과 서버 AoI relevance를 단일 공간 질의 엔진으로 통합하고
 비용 모델로 파라미터화한 relevance 함수는, false-negative(보여야 할 것 누락) 0을 유지하면서
 독립 구현 baseline 대비 서버 복제 대역폭을 35% 줄이고 클라-서버 가시성 불일치 버그를 제거하며,
 통합 색인의 질의 비용을 두 독립 색인 합 대비 낮춘다."
```

### 4.6 평가 방법 (가이드 §7)

- **Metric**: **가시성 정확도** — false negative율(보여야 할 것을 컬/누락 — 0이어야 함), over-inclusion(over-draw 비율·과전송 entity 수), 컬링 효율(컬된 비율, draw call 절감), **대역폭**(KB/tick, AoI ON/OFF), 클라-서버 가시성 불일치 횟수, 질의 비용(ms).
- **Baseline**: 컬링 없음(전부 그림/전부 보냄), frustum-only, 거리-only AoI, 두 시스템 독립 구현.
- **Ablation**: occlusion ON/OFF, FoW/LoS ON/OFF, 통합 vs 분리 색인, AoI 반경 sweep.
- **재현성**: 결정론 리플레이로 같은 매치를 반복(AoI ON/OFF 대역폭 비교), 카메라 경로 고정으로 컬링 측정.
- **Threats to validity**: occlusion 판정의 씬 의존성, AoI 반경이 게임플레이 공정성에 영향(시야 밖 적 정보).

### 4.7 Winters 연결점 — 동형성이 코드로 이미 존재

> **이 문서의 간판 연결점.** Winters는 `CSpatialIndex` 하나를 **렌더/AI/vision이 공유**하고, 서버 AoI delta가 별도 문서로 명세돼 있어, §4.4-1의 "통합 relevance 엔진" 연구가 거의 그대로 굴러간다.

- **시야·relevance 시스템이 살아 있음**: `Engine/Public/ECS/Systems/VisionSystem.h`의 `CVisionSystem`은 `CSpatialIndex*`와 `CBushVolumeIndex*`를 받아 `IsTargetVisible(source, target, sightRange)`로 **시야 차단(부시) 기반 가시성**을 판정하고, 결과를 **fog-of-war 텍스처**(`FOW_TEX_DIM=256`, `FOW_TEX_WORLD_SIZE=280`)로 굽는다. 이것이 §4.1의 "occlusion/LoS 기반 relevance"의 **MOBA 실증** — fog of war = 클라 가시성과 서버 정보 은폐의 교차점.
- **`IsTargetVisibleFast`에 true-sight·sightRangeSq 분기**: 보수적·거리² 기반 빠른 경로가 있어 §4.4-3(보수성-tightness)·§4.6(false-negative=0) 실험의 기성 구현. `VisRecord{ source, target, distance }` 디버그 레코드가 가시성 정확도 metric을 노출.
- **컬링과 AoI가 같은 색인을 공유 = §4.4-1의 토대**: `CSpatialIndex::QueryRadius`(§1.7)가 vision·AI에 이미 쓰이고, 같은 질의가 **서버 AoI**(`SnapshotBuilder`의 visible entity set, `02_UDP..._DELTA_AOI` §1-5/1-6)와 **클라 컬링** 양쪽에 자연히 연결 → "두 질의를 한 엔진으로" 통합이 코드 구조상 가능.
- **서버 AoI delta가 명세됨**: `.md/TODO/05-15/02_UDP_M1_M3_TRANSPORT_RELIABILITY_DELTA_AOI.md`가 per-session baseline cache, added/changed/**removed**(stale entity 정리), baseline mismatch 시 full resync를 못박는다 → §4.3 BuildDelta 의사코드의 ground truth. M3 검증("client AOI 밖 entity가 removed delta로 사라진다")이 §4.6 대역폭·정확도 실험 그 자체.
- **두 모드 대비**: MOBA(작은 맵, 모두 frustum 안일 수 있어 occlusion·AoI가 시야/부시 중심) vs 오픈월드(원경, frustum+occlusion+거리 AoI 모두 압박) → relevance 정책의 스케일 의존성(가이드 §7).
- **`10_Server`·`11_Security`로 직결**: 서버 relevance = AoI(`10_Server`), 시야 밖 정보 은폐 = wallhack 차단(`11_Security`). 이 문서의 §4가 두 도메인의 공간 질의 측 진입점.

---

## 종합. 통합 학위논문 구조 예시

가이드 §4의 "Three Papers Make a Thesis"로, **하나의 분야(관찰자 중심 공간 질의: 스트리밍·LOD·relevance) 안의 인접한 세 문제**를 묶는다. 관통 통찰은 §0.1의 **동형성**: 스트리밍·컬링·AoI는 모두 "관찰자에게 지금 무엇이 필요한가"의 변주.

> **학위논문 thesis statement:**
> "관찰자 중심 공간 질의를 단일 엔진으로 통합하면 — 예측적 스트리밍·자동 HLOD·통합 relevance를 한 색인 위에 얹어 —
> 고정 메모리·대역폭 예산 하에서 오픈월드의 hitch·팝핑·과전송을 동시에 줄이고 클라-서버 가시성을 일관되게 유지할 수 있다."

```text
Ch 1. 서론
   - 동기: 오픈월드+MOBA 하이브리드에서 메모리·대역폭은 유한, hitch·팝핑·과전송은 품질·공정성 직결.
   - 문제: 스트리밍/컬링/AoI가 독립 구현 → 예산 비효율·일관성 버그. 동형성 미활용.
   - Thesis statement(위) + 기여 4개 bullet.

Ch 2. 배경 및 관련 연구
   - 공간 분할(grid/octree/BVH/spatial hash), UE5 World Partition, QEM/Nanite, HZB/PVS/AoI.
   - gap: 스트리밍·LOD·relevance를 단일 공간 질의로 통합하고 예산 하 hitch·정확도를 동시 보장하는 것이 미해결.

Ch 3. [논문 1] 예측적 스트리밍과 동적 공간 색인          (→ §1 + §2)
   - 정적 BVH / 동적 다해상도 spatial hash 하이브리드 + 공간 Markov prefetch + 예산 스케줄러.
   - 평가: hitch(p99), pop-in, peak working set. baseline = 거리-only / LRU. venue: EuroSys / FAST / ASPLOS.

Ch 4. [논문 2] 셀 계층 자동 HLOD와 팝핑 없는 LOD 전이     (→ §3)
   - QEM 자동 프록시 + 화면공간 오차 cut + 디더-temporal 전이 (+ 런타임 생성).
   - 평가: FLIP/SSIM, 팝핑(temporal flicker), draw call. baseline = 수작업 HLOD / Nanite. venue: SIGGRAPH / HPG / I3D.

Ch 5. [논문 3] 클라 컬링 ⊕ 서버 relevance 통합 엔진      (→ §4)
   - 비용 파라미터화 relevance 함수 + occlusion 기반 AoI + 통합 색인.
   - 평가: false-negative=0, 대역폭, 가시성 불일치. baseline = 독립 구현 / 거리 AoI. venue: NSDI / SIGCOMM / EuroSys.

Ch 6. 종합 평가
   - Winters를 testbed로: MOBA 작은 고정맵 vs 오픈월드 대규모 스트리밍의 두 모드 대비.
   - 세 기여를 하나의 관찰자 중심 공간 질의 파이프라인으로 통합한 end-to-end 사례 연구.

Ch 7. 논의 / Ch 8. 결론 및 향후 연구
   - threats to validity(합성 카메라 경로·씬 대표성, HW 의존), 일반화 가능성(MMO·메타버스).
```

자가진단(가이드 §12): **"내 thesis는 무엇이고, 무엇으로 측정하며, 무엇과 비교했는가?"** — 위 구조는 셋 다 즉답된다(hitch·FLIP·대역폭/false-negative / 거리-only·수작업 HLOD·독립 구현 baseline).

---

## 참고문헌

> 표기: 확실히 검증 가능한 1차 문헌·도구만. 학술 논문은 저자·연도·venue, 도구는 제작자·성격 명시(가이드 §10 인용 규범). GDC/엔진 문서는 학술 아님을 명시.

**공간 분할 자료구조**
- Teschner, M., Heidelberger, B., Müller, M., Pomeranets, D., Gross, M. "Optimized Spatial Hashing for Collision Detection of Deformable Objects." *Vision, Modeling, and Visualization (VMV)*, 2003. (spatial hashing 정전)
- MacDonald, J. D., Booth, K. S. "Heuristics for Ray Tracing Using Space Subdivision." *The Visual Computer*, 6(3), 1990. (SAH 원전)
- Wald, I. "On fast Construction of SAH-based Bounding Volume Hierarchies." *IEEE Symp. on Interactive Ray Tracing (RT)*, 2007.
- Karras, T. "Maximizing Parallelism in the Construction of BVHs, Octrees, and k-d Trees." *High Performance Graphics (HPG)*, 2012. (GPU 병렬 빌드, LBVH)
- Ulrich, T. "Loose Octrees." in *Game Programming Gems* (Charles River Media), 2000. (동적 씬 octree)
- Guttman, A. "R-trees: A Dynamic Index Structure for Spatial Searching." *SIGMOD*, 1984.

**스트리밍·메모리·working set**
- Denning, P. J. "The Working Set Model for Program Behavior." *Communications of the ACM*, 11(5), 1968. (스트리밍의 이론적 조상)
- Epic Games. *Unreal Engine 5 — World Partition* (공식 문서 + GDC 강연들). — OFPA·runtime hash·data layer·HLOD (산업 레퍼런스, 학술 아님).
- van Waveren, J.M.P. "id Tech 5 Challenges: From Texture Virtualization to Massive Parallelization." *SIGGRAPH 2009 Beyond Programmable Shading course.* (가상 텍스처 스트리밍)

**LOD·HLOD·가상 지오메트리**
- Garland, M., Heckbert, P. S. "Surface Simplification Using Quadric Error Metrics." *SIGGRAPH*, 1997. (QEM)
- Hoppe, H. "Progressive Meshes." *SIGGRAPH*, 1996.
- Luebke, D., Reddy, M., Cohen, J., Varshney, A., Watson, B., Huebner, R. *Level of Detail for 3D Graphics.* Morgan Kaufmann, 2002. (표준 교과서)
- Karis, B., Stachowiak, R., et al. "A Deep Dive into Nanite Virtualized Geometry." *SIGGRAPH 2021 Course: Advances in Real-Time Rendering in Games.* (가상화 지오메트리)

**가시성·컬링·relevance(AoI)**
- Greene, N., Kass, M., Miller, G. "Hierarchical Z-Buffer Visibility." *SIGGRAPH*, 1993. (HZB occlusion)
- Teller, S. J., Séquin, C. H. "Visibility Preprocessing for Interactive Walkthroughs." *SIGGRAPH*, 1991. (셀-포털 PVS)
- Cohen-Or, D., Chrysanthou, Y., Silva, C. T., Durand, F. "A Survey of Visibility for Walkthrough Applications." *IEEE TVCG*, 9(3), 2003.
- Benford, S., Fahlén, L. "A Spatial Model of Interaction in Large Virtual Environments." *ECSCW*, 1993. (Aura/Nimbus relevance 모델)
- Singhal, S., Zyda, M. *Networked Virtual Environments: Design and Implementation.* Addison-Wesley, 1999. (interest management)
- Haar, U., Aaltonen, S. "GPU-Driven Rendering Pipelines." *SIGGRAPH 2015 Course: Advances in Real-Time Rendering.* (Hi-Z + indirect draw, 산업 사례)

**Winters 내부 1차 자료 (testbed)**
- `Engine/Public/ECS/SpatialIndex.h`, `Engine/Private/ECS/Systems/SpatialHashSystem.cpp` — 균일 grid + spatial hash 색인(`QueryRadius`/`QueryClosest`, `LoLSpatialGridDesc`).
- `Engine/Public/ECS/Systems/VisionSystem.h` — 시야 차단·fog-of-war 기반 relevance(`IsTargetVisible`, `FOW_TEX_DIM`), `CSpatialIndex` 공유.
- `.md/EldenRing/04_WORLD_PARTITIONING_STREAMING.md` — 셀 상태 기계·streaming source(load/visible/unload 반경)·data layer·HLOD 전략.
- `.md/EldenRing/07_ASSET_LOADER_AND_STREAMING_RUNTIME.md` — async IO·GPU upload 큐·프레임 예산·required/optional fallback(→ `06_Editor` 연결).
- `.md/TODO/05-15/02_UDP_M1_M3_TRANSPORT_RELIABILITY_DELTA_AOI.md` — 서버 AoI·delta(added/changed/removed)·baseline cache(→ `10_Server`의 AoI 직결).
