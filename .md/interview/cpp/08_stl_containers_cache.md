# STL 컨테이너 · 캐시 · 데이터 지향 설계

## ① 한 줄 본질

"게임 코드가 `std::vector`를 기본값으로 쓰는 이유는 빅오가 아니라 캐시다 — 연속 메모리 순회는 하드웨어 프리페처(prefetcher)를 타서 노드 기반 컨테이너보다 실측으로 수십 배 빠를 수 있고, 그래서 자료구조 선택의 첫 질문은 '접근 패턴이 순회인가 조회인가'가 되어야 한다."

---

## ② 기본 개념

### vector 내부 구조

`std::vector`는 힙에 잡은 연속(contiguous) 버퍼 하나와 포인터 3개(시작, 끝=size, 용량 끝=capacity)로 구성된다.

- **성장 정책(growth policy)**: `push_back` 시 `size == capacity`면 더 큰 버퍼를 새로 할당하고 기존 원소를 전부 이동/복사(move ctor가 `noexcept`면 이동)한 뒤 옛 버퍼를 해제한다. 배수는 구현 정의 — MSVC는 1.5배, libstdc++/libc++는 2배. 기하급수 성장이라 push_back N회의 총 비용은 O(N), 즉 **분할상환(amortized) O(1)**. 산술 성장(+k)이었다면 O(N²).
- **재할당과 무효화**: 재할당이 일어나면 **모든 반복자(iterator)·포인터·참조가 무효화**된다. 재할당이 없어도 삽입 지점 이후의 반복자는 무효화된다. `erase`는 삭제 지점부터 뒤로 전부 무효화.
- **`reserve`의 중요성**: 원소 수 상한을 알면 `reserve(n)`으로 재할당을 0회로 만든다. 재할당은 (1) 할당 비용 (2) 전체 이동 비용 (3) 포인터 무효화라는 세 가지 문제를 동시에 일으키므로, 핫루프에서 `reserve` 없는 push_back 누적은 흔한 성능/버그 원인이다.
- **`clear()`는 capacity를 유지**한다. size만 0이 되므로 다음 프레임 push_back은 재할당 없이 진행된다 — 프레임 단위 재사용 버퍼의 근거.

### 주요 컨테이너 내부 구조와 복잡도

| 컨테이너 | 내부 구조 | 조회 | 삽입/삭제 | 순회 캐시 | 참조 안정성 |
|---|---|---|---|---|---|
| `vector` | 연속 배열 | 인덱스 O(1) | 끝 O(1)*, 중간 O(N) | 최상 | 재할당 시 전부 무효 |
| `deque` | 고정 블록들의 포인터 맵 | O(1) | 양끝 O(1) | 블록 단위로 양호 | 양끝 삽입 시 **참조는 유지**, 반복자는 무효 |
| `list` | 이중 연결 노드 | O(N) | 위치 알면 O(1) | 최악 (노드마다 캐시 미스) | 삭제된 원소 외 항상 유지 |
| `map` | 레드-블랙 트리(red-black tree) | O(log N) | O(log N) | 나쁨 | 삭제된 원소 외 항상 유지 |
| `unordered_map` | 버킷 배열 + 노드 체이닝 | 평균 O(1), 최악 O(N) | 평균 O(1) | 나쁨 | rehash에도 **참조/포인터 유지**, 반복자는 무효 |

\* 끝 삽입은 분할상환 O(1).

### 해시맵: 버킷과 로드 팩터

`std::unordered_map`은 `hash(key) % bucket_count`로 버킷을 고르고, 표준 인터페이스(`bucket_count`, 참조 안정성 보장)가 사실상 separate chaining(버킷당 노드 리스트) 구현을 강제한다.

- **load factor** = size / bucket_count. `max_load_factor`(기본 1.0)를 넘으면 **rehash** — 버킷 배열을 키우고 모든 노드를 재분배한다. 이때 반복자는 무효화되지만 **원소가 노드에 살아 있으므로 참조/포인터는 유지**된다.
- rehash가 산발적 O(N) 스파이크를 만들므로, 크기를 알면 `reserve(n)`으로 미리 버킷을 확보한다.
- 노드 기반 + 체이닝이라 조회 한 번에 캐시 미스가 여러 번(버킷 배열 → 노드 → 키 비교) 발생한다. 그래서 게임 엔진들이 open addressing 해시맵(flat_hash_map류)을 따로 만든다.

### 캐시 계층과 지역성

- 대략적 지연: L1 ~4cycle, L2 ~12cycle, L3 ~40cycle, **메인 메모리 ~200+cycle**. 캐시 미스 한 번이 산술 연산 수백 개 값이다.
- 캐시는 **64바이트 캐시 라인(cache line)** 단위로 채워진다. 4바이트 float 하나를 읽어도 이웃 60바이트가 공짜로 딸려온다 — 연속 배열 순회가 빠른 직접적 이유(공간 지역성, spatial locality).
- 하드웨어 프리페처는 **규칙적인 스트라이드 접근**을 감지해 다음 라인을 미리 당겨온다. `vector` 선형 순회는 프리페치가 완벽히 작동하고, `list`/트리의 포인터 추적(pointer chasing)은 다음 주소를 읽기 전엔 알 수 없어 프리페치가 불가능하다.

### False Sharing

서로 다른 스레드가 **서로 다른 변수**를 쓰더라도 그 변수들이 **같은 캐시 라인**에 있으면, 캐시 일관성 프로토콜(MESI)이 라인 소유권을 코어 간에 핑퐁시켜 성능이 급락한다. 해결은 `alignas(64)`(또는 C++17 `std::hardware_destructive_interference_size`)로 스레드별 쓰기 변수를 다른 라인에 격리하는 것.

### AoS vs SoA

- **AoS(Array of Structures)**: `struct Entity { pos; vel; hp; }; vector<Entity>`. 한 개체의 모든 필드가 붙어 있다 — 개체 단위 접근에 유리.
- **SoA(Structure of Arrays)**: `vector<Vec3> positions; vector<Vec3> velocities;`. 한 필드의 모든 개체가 붙어 있다 — "모든 개체의 position만" 도는 시스템 순회에서 캐시 라인을 100% 유효 데이터로 채우고 SIMD 벡터화에도 유리.
- ECS가 SoA를 택하는 이유: 시스템은 "관심 있는 컴포넌트만" 순회하기 때문. 반대로 질의 시점에 여러 필드가 한꺼번에 필요하면 오히려 AoS로 뭉치는 게 맞다(아래 Winters SpatialIndex 사례).

### 왜 게임은 vector를 기본으로 쓰나

1. 프레임마다 같은 데이터를 **반복 순회**하는 워크로드가 지배적 — 연속 메모리 + 프리페치의 이득이 최대화된다.
2. `clear()` 후 capacity 재사용, `swap` O(1) 교환, `reserve` 등 **할당 제어 수단**이 풍부하다.
3. 중간 삭제 O(N) 약점은 순서가 무의미한 게임 데이터에서 **swap-and-pop**(마지막 원소를 구멍으로 이동 후 pop_back)으로 O(1)이 된다.
4. `list`가 이기는 시나리오(순서 유지 + 잦은 중간 삽입 + 원소가 큼)는 게임 코드에서 드물고, 그 경우조차 실측하면 vector가 이기는 일이 많다(원소 이동 비용 < 캐시 미스 비용).

---

## ③ 심화 (꼬리질문 대비)

### 반복자 무효화 규칙 정리

- `vector::push_back`: 재할당 시 전부 무효. 재할당 없으면 `end()`만 무효.
- `deque::push_back/push_front`: 반복자는 전부 무효, **참조는 유지** (블록은 안 움직이고 블록 맵만 재배치되기 때문). 중간 삽입은 참조까지 무효.
- `map`/`list`: 삭제된 원소의 반복자만 무효.
- `unordered_map`: rehash 시 반복자 전부 무효, 참조/포인터는 유지. 이 보장 덕분에 **맵 원소의 주소를 밖으로 반환하는 캐시 패턴**이 합법이다.
- 무효화된 반복자/포인터 사용은 미정의 동작(undefined behavior) — 디버그에서 안 터지고 릴리즈에서만 터지는 전형적 패턴.

### 순회 중 구조 변경 문제의 3가지 해법

1. **collect-then-mutate**: 순회 중엔 삭제 대상을 벡터에 모으기만 하고, 순회가 끝난 뒤 일괄 삭제.
2. **erase-remove 관용구**: `v.erase(std::remove_if(...), v.end())` — remove_if가 생존 원소를 앞으로 압축(O(N) 1패스)하고 erase가 꼬리를 자른다.
3. **명령 버퍼(command buffer)**: 변경을 명령 객체로 기록하고 프레임의 안전한 지점에서 flush (멀티스레드 순회와도 호환).

### swap 트릭들

- `a.swap(b)`: 내부 포인터 교환, O(1), 할당 0회. 더블 버퍼링의 핵심.
- **swap-under-lock**: 락 안에서는 멤버 컨테이너를 지역 변수와 swap만 하고, 실제 처리(정렬/콜백)는 락 밖에서 — 임계구역 최소화 + 재진입 데드락 회피.
- `shrink_to_fit`은 비구속(non-binding) 요청. 확실히 줄이려면 `vector<T>(v).swap(v)` 고전 관용구.

### generation stamp (세대 도장)

큰 스크래치 배열을 매 호출 O(N) memset하는 대신, 세대 카운터를 증가시키고 "셀의 스탬프 != 현재 세대면 미방문"으로 판정해 **처음 만지는 셀만 지연 초기화**한다. 카운터가 0으로 wrap하는 순간만 전체 fill. 같은 아이디어가 **generational handle**(슬롯 재사용 시 세대 증가로 stale 핸들 검출 — ABA/use-after-free 방어)에도 쓰인다.

### sort vs stable_sort

`std::sort`는 introsort(퀵+힙+삽입), 평균 O(N log N), 동률 원소의 상대 순서 보장 없음. `std::stable_sort`는 머지소트 계열로 동률 순서를 보존하는 대신 추가 메모리(또는 O(N log²N)). **UI 행처럼 값이 미세하게 흔들리는 데이터를 정렬하면 sort는 프레임마다 순서가 튀고 stable_sort는 안정적**이다. 비교자는 strict weak ordering을 만족해야 하며 위반 시 UB.

---

## ④ Winters에서의 적용

### 1. ECS sparse-set — SoA 연속 배열 + swap-and-pop

`Engine/Public/ECS/ComponentStore.h:8` — `CComponentStore<T>`는 `m_vecSparse`(엔티티→dense 인덱스), `m_vecDense`(dense→엔티티), `m_vecData`(컴포넌트 실데이터, 65행 주석 "연속된 메모리 사용!") 3개 vector로 구성된 sparse-set이다. `Remove`(24-38행)는 마지막 원소를 구멍으로 `std::move`한 뒤 `pop_back`하는 swap-and-pop이라 **O(1) 삭제 + dense 배열 무공백**을 동시에 얻는다. 시스템 순회는 `m_vecData.data()` 선형 스캔 — 이 챕터의 모든 이론(연속 메모리, 프리페치, SoA)이 집약된 자료구조다. 부작용도 명확하다: 삭제가 원소를 이동시키므로 **순회 중 삭제는 인덱스를 흔든다** → 아래 4번의 지연 삭제 패턴이 필수가 된다.

### 2. POD 컴포넌트 — vector 대신 고정 배열, 축소 enum

`Engine/Public/ECS/Components/CoreComponents.h:16` — `HealthComponent` 등은 가상함수·상속 없는 순수 데이터 struct에 default member initializer만 둔다. `AbilityComponent`(51-55행)는 스킬 데이터를 `std::vector`가 아니라 `uint32_t skillIDs[MAX_SKILLS]` 고정 배열로 둬서 힙 간접참조 없이 trivially-copyable을 유지하고, `AIStateComponent::State`(44행)는 `: uint8_t`로 언더라잉 타입을 축소해 컴포넌트 크기를 줄인다. 컴포넌트 안에 vector를 넣으면 데이터가 힙으로 흩어져 dense 배열 순회의 캐시 이점이 사라진다 — "핫데이터에 포인터를 넣지 마라"의 실물. 같은 원칙이 `Client/Public/GameObject/ChampionVisualData.h:9`의 데이터 카탈로그(`texturePath[8]`, `poses[8]` 등 컴파일 타임 상한 고정 배열)에도 적용돼 있다.

### 3. 프레임 더블 버퍼 — swap O(1) + capacity 재사용

`Engine/Private/Core/Profiler/CPUProfiler.cpp:47` — `EndFrame`은 `m_vLastFrameEvents.swap(m_vCurrentFrame)` 후 `clear()`. swap은 포인터 교환 O(1), clear는 capacity를 유지하므로 다음 프레임의 push_back이 재할당 없이 진행된다. 복사 대입이었다면 매 프레임 전체 이벤트 복사 + 할당. 읽기(오버레이)는 last, 쓰기는 current로 분리되는 생산자/소비자 더블 버퍼다. 계측 핫패스는 29행의 `thread_local std::vector<PendingProfilerScope> t_vProfilerStack`으로 스레드별 분리해 락이 없고, 공유 버퍼 병합 지점(93행)만 mutex를 잡는다. 이 프로파일러가 프레임 17.8ms→9ms 복구 작업(2026-04-24 세션)의 계측 기반이었다 — "튜닝 전에 계측"이라는 팀 규칙의 출발점.

### 4. 순회 중 삭제 금지 — collect-then-mutate

`Shared/GameSim/Champions/Jax/JaxGameSim.cpp:439` — `Tick`은 `ForEach` 순회 중 완료된 대시를 `finishedDashes` 벡터에 모으기만 하고(453행 `push_back`), 루프가 끝난 뒤에 컴포넌트를 제거한다. sparse-set의 swap-remove가 순회 인덱스를 흔들기 때문이다. 엔진 레벨에서는 `Engine/Private/ECS/CommandBuffer.cpp`가 같은 문제를 명령 버퍼로 일반화한다 — 구조 변경을 기록만 하고 순회 종료 후 Flush, Flush 내부는 락 안에서 지역 벡터와 swap만 하고 실행은 락 밖(swap-under-lock).

### 5. 해시맵 캐시 3종 — 키 정규화, 참조 안정성, 유한 dedup

- **키 정규화**: `Engine/Private/Resource/ResourceCache.cpp:91` — `NormalizePath`가 백슬래시→슬래시, 소문자 변환으로 `unordered_map` 키를 정규화(canonicalize)한다. 대소문자/구분자만 다른 동일 경로가 다른 키로 잡혀 같은 리소스를 두 번 로드하는 것을 방지.
- **참조 안정성 활용**: `Engine/Private/Renderer/RHIFxMeshResource.cpp:289` — 메시 캐시가 `emplace(strMeshKey, std::move(resource))` 후 `&inserted.first->second`(298행)를 반환한다. `unordered_map`은 노드 기반이라 이후 삽입/rehash에도 원소 주소가 유지되므로 raw 포인터 반환이 안전하다 — `vector`였다면 재할당으로 댕글링. 같은 근거가 `Shared/GameSim/Registries/ChampionStats/ChampionStatsRegistry.cpp:18`의 `Find`(`&it->second` 반환, nullable 관찰 포인터 계약)에도 깔려 있다.
- **유한 dedup 캐시**: `Client/Private/Network/Client/EventApplier.cpp:428` — FNV-1a 해시로 이벤트 dedup 키를 만들고 `unordered_set::insert().second`로 최초 1회만 재생. 872-873행에서 세트가 4096을 넘으면 통째로 `clear()`해 장기 실행 시 무한 증식을 차단한다. 570행 `m_lastActionSeq[ev->netId()]`는 `operator[]`의 기본값 삽입(처음 보는 키 = 0)을 의도적으로 이용한 사례.

### 6. map vs unordered_map 선택 근거 — 결정적 순서

`Engine/Public/ECS/SystemScheduler.h:39` — `m_mapPhases`는 `map<uint32_t, vector<unique_ptr<ISystem>>>`. 시스템 실행 계획을 phase 번호 **오름차순으로 순회**해야 하므로 정렬 트리인 `std::map`을 쓴다. "조회가 빠르니 무조건 unordered_map"이 아니라, **순서가 요구사항이면 map**이라는 선택 기준의 실례.

### 7. 캐시를 위한 비트/세대/스크래치 — Pathfinder & NavGrid

- **비트팩 그리드**: `Engine/Private/Manager/Navigation/NavGrid.cpp:93` — 262,144칸 걷기 맵을 셀당 1비트로 압축(`m_vecBits[iIdx >> 3] >> (iIdx & 7) & 1`, `>>3`은 /8, `&7`은 %8). 바이트 대비 메모리 1/8 = 캐시에 8배 더 들어간다. `SetAllWalkable`(118-126행)은 마지막 바이트의 남는 상위 비트를 마스킹해 오염 방지.
- **generation stamp**: `Engine/Private/Manager/Navigation/Pathfinder.cpp:463` — A*의 gScore/parent/closed 배열을 `thread_local`로 두고(워커 재진입 안전 + 할당 상각), 472-481행의 `Touch` 람다가 `tls_generation[idx] != currentGeneration`일 때만 그 셀을 초기화한다. 매 탐색 O(N) memset을 O(방문 셀 수)로 바꾼 것. `BeginSearchGeneration`(123-136행)은 카운터가 0으로 wrap하면 전체 fill 후 1로 리셋하는 엣지까지 처리한다.
- **min-heap**: 483행 `std::priority_queue<Node, std::vector<Node>, std::greater<Node>>` — 기본 max-heap을 `std::greater` + `operator>`로 뒤집어 최소 f-cost 노드를 꺼낸다. 컨테이너 어댑터가 vector 위라 캐시 지역성도 좋다.

### 8. 공간 해시 — 질의를 위한 의도적 AoS 비정규화

`Engine/Public/ECS/SpatialIndex.h:65` — 브로드페이즈 셀에 넣는 `CellEntry`는 `{id, pos, kind, team, radius}`를 통째로 복사한 AoS 구조체다(79행 `unordered_map<i64_t, std::vector<CellEntry>>`). ECS 본체는 SoA인데 여기서는 반대로 간다 — `QueryRadius`가 Transform/컴포넌트를 다시 조회하지 않고 **셀 벡터 하나만 선형 순회**하면 되도록, Rebuild 시점에 복사 비용을 지불하고 읽기를 국소화한 것. "SoA vs AoS는 접근 패턴이 정한다"의 실례. `Engine/Private/ECS/SpatialIndex.cpp:191`의 `MakeKey`는 셀 좌표 2개를 `(i64(cx) << 32) | u32(cz)`로 팩킹하는데, `u32` 캐스트가 음수 cz의 부호확장(sign extension)이 상위 32비트를 오염시키는 것을 막는다.

### 9. false sharing — alignas(64) 실전

`Engine/Public/Core/JobSystem/WorkStealingDeque.h:91` — Chase-Lev work-stealing deque에서 owner가 갱신하는 `m_iBottom`과 도둑들이 CAS하는 `m_iTop`을 각각 `alignas(64)`로 다른 캐시 라인에 격리한다(파일 머리 주석 12행 "alignas(64) 로 top/bottom false-sharing 방지"). 19행 `static_assert((kCapacity & (kCapacity - 1)) == 0)`은 용량이 2의 거듭제곱임을 컴파일 타임에 강제해 `%` 대신 `& (kCapacity - 1)` 마스크 인덱싱을 성립시킨다. owner-LIFO(방금 push한 job이 캐시에 뜨거움)/thief-FIFO(오래된 job을 가져가 라인 경쟁 최소화) 분리 자체가 캐시 논리다.

### 10. vector에 atomic을 못 담는다 — unique_ptr 박싱

`Engine/Private/Core/JobSystem.cpp:55` — 주석이 이유를 박제한다: "CWorkStealingDeque 의 std::atomic 멤버 + alignas(64) 조합이 MSVC construct_at SFINAE 에서 실패하므로 힙 할당 + 포인터 저장." `std::atomic`은 복사/이동 불가라 vector 원소 요건(MoveInsertable)을 못 채우고, 해법은 `vector<unique_ptr<CWorkStealingDeque>>`로 포인터만 담는 것(62행).

### 11. stable_sort + erase-remove — UI 안정성이라는 요구

`Engine/Private/Core/Profiler/ProfilerStableView.cpp:149` — HUD 행 정렬에 `std::stable_sort` + 4단 tie-breaker(visible → emaTotalMs → firstSeenFrame → name) 람다. EMA 값이 프레임마다 미세하게 흔들려도 동률 원소 순서가 보존돼 행이 깜빡이지 않는다. 만료 행 제거는 166-174행의 erase-remove 관용구. 66-67행은 `push_back` **직후에** `&m_vRows.back()`을 취한다 — 재할당 가능성이 있는 push_back 이전에 잡아둔 포인터였다면 댕글링이 될 자리라, vector 무효화 규칙을 지킨 배치다.

### 12. 병렬화 임계 — 오버헤드 실측 기반 판단

`Engine/Private/ECS/Systems/TransformSystem.cpp:11` — `kParallelThreshold = 16` 주석: "Submit / steal / Counter 오버헤드가 병렬 이득보다 커지는 임계. 프로파일링으로 조정." 루트 16개 미만이면 단일 스레드 fallback(34행). "병렬화가 항상 빠르다"가 아니라 작업 granularity가 스케줄링 비용을 넘어야 한다는 데이터 기반 판단 — 데이터 지향 설계는 자료구조뿐 아니라 이런 실측 의사결정까지 포함한다.

---

## ⑤ 면접 Q&A

**Q1. vector의 push_back이 amortized O(1)인 이유를 설명해보세요.**
- 핵심: capacity 초과 시 기하급수(1.5~2배) 재할당. 재할당 비용을 이후 삽입 횟수로 분할상환하면 삽입당 상수. 산술 성장(+k)이면 O(N²)이 되는 것과 대비.
- 꼬리: 재할당 순간 모든 반복자/포인터/참조 무효화 → "그래서 push_back 직후에 back()의 주소를 잡는다" — Winters `ProfilerStableView.cpp:66-67`이 정확히 그 순서를 지킨다.

**Q2. map과 unordered_map, 언제 무엇을 쓰나요?**
- 핵심: unordered_map은 평균 O(1) 조회지만 순서 없음 + rehash 스파이크. map은 O(log N)이지만 키 정렬 순회와 결정적 순서 보장.
- Winters: 시스템 실행 phase는 오름차순 순회가 요구사항이라 `SystemScheduler.h:39`가 `std::map`을 선택. 리소스 캐시·레지스트리처럼 순서 무관 조회는 전부 `unordered_map`.

**Q3. unordered_map의 rehash가 일어나면 무엇이 무효화되나요?**
- 핵심: 반복자는 전부 무효, **참조/포인터는 유지**(노드 기반이라 원소가 안 움직임). load factor > max_load_factor 시 발생.
- Winters: `RHIFxMeshResource.cpp:289-298`이 이 보장을 근거로 맵 원소 주소를 캐시 밖으로 반환. vector였다면 불가능한 API.

**Q4. 게임에서 list 대신 vector를 쓰라고 하는 이유는?**
- 핵심: 빅오표에는 안 나오는 캐시. list는 노드마다 포인터 추적 = 캐시 미스, 프리페치 불가, 노드당 할당/헤더 오버헤드. vector 중간 삭제 O(N)은 순서 무관 데이터면 swap-and-pop으로 O(1).
- Winters: `ComponentStore.h`의 sparse-set이 swap-and-pop으로 dense 배열 무공백을 유지 — ECS 순회 성능의 토대.

**Q5. AoS와 SoA의 차이와 선택 기준은?**
- 핵심: 순회가 일부 필드만 만지면 SoA(캐시 라인 낭비 제거 + SIMD), 접근이 개체 단위로 여러 필드를 한꺼번에 만지면 AoS.
- Winters: 컴포넌트 저장은 SoA(`ComponentStore`), 브로드페이즈 질의는 의도적 AoS 비정규화(`SpatialIndex.h:65` CellEntry — 질의 시 컴포넌트 재조회를 없애려고 Rebuild 때 복사). 같은 코드베이스 안에서 접근 패턴 따라 반대 선택을 한 것이 포인트.

**Q6. false sharing이 무엇이고 어떻게 잡나요?**
- 핵심: 다른 스레드가 다른 변수를 써도 같은 64B 캐시 라인이면 MESI 소유권 핑퐁으로 성능 급락. 진단은 프로파일러(높은 캐시 미스 + 코어 수 대비 확장성 붕괴), 해결은 alignas(64) 라인 격리.
- Winters: `WorkStealingDeque.h:91-92` — owner 전용 bottom과 thief 전용 top을 각각 alignas(64)로 분리.

**Q7. 매 프레임 쌓이는 데이터를 할당 없이 다음 소비자에게 넘기려면?**
- 핵심: 복사 대입 대신 `swap`(포인터 교환 O(1)) + `clear()`(capacity 유지). 생산/소비 버퍼 분리 = 더블 버퍼링.
- Winters: `CPUProfiler.cpp:47-53` — EndFrame에서 swap+clear 3쌍. reserve된 capacity가 프레임을 넘어 재사용된다.

**Q8. 컨테이너를 순회하면서 원소를 지우면 왜 안 되고, 어떻게 해결하나요?**
- 핵심: vector erase는 이후 반복자 무효화, sparse-set swap-remove는 인덱스 재배치 — 순회 상태가 깨진다(UB 또는 원소 건너뜀). 해법은 collect-then-mutate, erase-remove 관용구, 명령 버퍼 3종.
- Winters: `JaxGameSim.cpp:439`(완료 대시 수집 후 루프 밖 제거), `ProfilerStableView.cpp:166`(erase-remove), `CommandBuffer.cpp`(구조 변경 지연 실행) — 세 해법이 전부 실코드로 존재.

---

## ⑥ 흔한 오답/함정

1. **"vector는 2배씩 커진다"라고 단정** — 성장 배수는 구현 정의다. MSVC는 1.5배, libstdc++/libc++는 2배. "기하급수 성장이라 amortized O(1)이고, 배수는 구현마다 다르다"가 정확한 답.

2. **"unordered_map이 항상 map보다 빠르다"** — 평균 조회는 빠르지만 (1) rehash 스파이크 (2) 순서 없음 (3) 노드 체이닝의 캐시 미스 (4) 해시 비용(긴 문자열 키)이 있다. 결정적 순서가 필요하면 map(Winters `SystemScheduler.h:39`), 최고 성능이 필요하면 open addressing 커스텀 맵까지 언급하면 가산점.

3. **reserve()와 resize()의 혼동** — reserve는 capacity만 확보(size 불변, 원소 접근 불가), resize는 size를 바꾸고 원소를 생성한다. reserve 후 `v[i]` 쓰기는 UB. 또 map `operator[]`가 **없는 키를 기본값으로 삽입**한다는 것(Winters `EventApplier.cpp:570`은 이를 의도적으로 이용하지만, 조회만 하려다 키를 오염시키거나 const 함수에서 컴파일이 안 되는 사고가 흔함)도 `at()`/`find()`와의 차이로 세트 정리.

4. **"list는 중간 삽입 O(1)이니 삽입 많으면 list"** — O(1)은 "그 위치의 반복자를 이미 갖고 있을 때"다. 위치를 찾는 O(N) 탐색이 캐시 미스 연발이라, 실측하면 vector의 원소 이동이 이기는 경우가 대부분. 빅오만 보고 자료구조를 고르는 것 자체가 함정이라는 메타 답변까지 하면 좋다.

5. **"swap-and-pop을 쓰면 만사 해결"** — swap-and-pop은 순서를 파괴하고, 순회 중에 쓰면 인덱스가 흔들린다. 순서 의존 데이터에는 못 쓰고, 순회 중에는 지연 삭제와 결합해야 한다(Winters ECS가 `CommandBuffer`/collect-then-mutate를 함께 두는 이유). "어떤 전제(순서 무관, 순회 밖)에서 유효한 기법인지"를 같이 말해야 완결된 답이다.

---

## 다른 챕터와의 연결

- [05_class_design_value_semantics.md](05_class_design_value_semantics.md) — move/swap이 컨테이너 재할당·소유권 이전 비용을 없애는 원리 (sink parameter, `std::move`로 맵에 넣기).
- [09_concurrency.md](09_concurrency.md) — false sharing/alignas(64), thread_local 스크래치, swap-under-lock의 동시성 측면 심화.
- [04_pointers_smart_pointers.md](04_pointers_smart_pointers.md) — `map<key, unique_ptr<T>>` 소유 컨테이너 + raw 관찰 포인터 규약, generational handle의 수명 안전.
- [11_architecture_ecs.md](11_architecture_ecs.md) — sparse-set·시스템 스케줄링·명령 버퍼 등 데이터 지향 ECS 아키텍처 전체 그림.
