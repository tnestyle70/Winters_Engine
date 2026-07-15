# 04. ECS · 게임 오브젝트 모델

> 면접 대본 + 지식 베이스. 코드 문법(템플릿, DLL 링크 등)의 상세는 `.md/interview/cpp/` 세트가 담당하고,
> 이 챕터는 "왜 이 구조인가"와 "무엇을 감수했나"에 집중한다.

---

## ① 한 줄 정의

"Winters의 게임 오브젝트 모델은 **sparse set 기반 ECS(Entity-Component-System)를 시뮬레이션의 중심**에 두고,
그 위에 **OOP 씬 레이어(IScene/Scene_InGame)가 렌더 리소스와 연출을 소유**하는 하이브리드 구조입니다.
시스템 실행 순서는 Phase 정수로, 병렬 안전성은 read/write 접근권 선언으로 — 두 계약을 분리해서 관리합니다."

---

## ② 구조와 데이터 흐름

### 2.1 세 개의 기둥

**Entity — generation slot + free-list** (`Engine/Public/ECS/Entity.h`)

- `CEntityManager`는 슬롯 벡터에 generation을 저장하고, 삭제된 id는 free-list(`nextFree`)로 재사용한다.
- alive/dead 판정은 별도 플래그 없이 **generation의 홀/짝 비트**로 인코딩한다
  (`IsAliveGeneration = (generation & 1) != 0`). 홀수 = alive.
- `EntityHandle`은 `(generation << 32) | index`의 64bit 패킹. 슬롯이 재사용되면 generation이 바뀌므로
  stale 핸들은 `TryResolve`에서 generation 불일치로 걸러진다 — **논리적 use-after-free를 런타임에 무효
  핸들로 잡는 장치**다.
- 단, `EntityID`(uint32)는 index-only legacy로 남아 있다. 헤더 주석에 스스로 명시했다:
  "New systems should keep EntityHandle when lifetime safety matters." 두 식별자 체계가 공존하는
  과도기 설계임을 인정한다.

**Component — sparse/dense/data 3배열 sparse set** (`Engine/Public/ECS/ComponentStore.h`)

- `CComponentStore<T>` = `m_vecSparse`(entity → dense 인덱스) + `m_vecDense`(entity 목록) +
  `m_vecData`(연속 T 배열, 65행 주석 그대로 "연속된 메모리 사용!").
- Add는 dense 끝에 push, Remove는 **마지막 원소를 빈 자리로 옮기는 swap-remove로 O(1)**.
- `INVALID = UINT32_MAX` 센티넬로 Has 판정.
- 파생 제약 두 가지: (a) 삭제 시 원소 순서가 바뀌므로 **순서 의존 로직 금지**,
  (b) 순회 중 삭제는 이터레이터를 깨므로 **CommandBuffer로 지연**해야 한다.

**System — Phase + 접근권 선언** (`Engine/Public/ECS/ISystem.h`)

```cpp
virtual uint32_t GetPhase() const = 0;                       // 실행 순서 그룹
virtual void Execute(CWorld& world, float fTimeDelta) = 0;   // 로직 전부 여기
virtual void DescribeAccess(CSystemAccessBuilder& b) const   // 병렬 배칭 근거
{ b.UnknownWritesAll(); }                                    // 기본값 = 보수적
```

**World — 타입 소거 스토어 맵 + ForEach join** (`Engine/Public/ECS/World.h`)

- `unordered_map<type_index, unique_ptr<IComponentStoreBase>>`로 컴포넌트 타입별 스토어를 보관,
  `GetOrCreateStore<T>`로 지연 생성.
- `ForEach`는 1/2/3 컴포넌트 오버로드. **항상 첫 타입 T1의 dense 배열이 driver**가 되고 나머지는
  `Has()` 필터 — 호출자가 인자 순서로 성능을 통제하는 암묵적 계약이다(③-2에서 상술).

### 2.2 프레임 데이터 흐름 — Phase가 곧 파이프라인

`CScene_InGame`이 `CWorld m_World`와 `unique_ptr<CSystemSchedular> m_pScheduler`를 직접 소유하고
(`Client/Public/Scene/Scene_InGame.h`), 매 프레임 스케줄러를 돌린다.

```
[Scene_InGame::OnUpdate]
  └─ CSystemSchedular::Execute(world, dt)     ← map<Phase, ...> 오름차순
       Phase 0   Player / AI / Transform      ┐ 같은 Phase 안에서
       Phase 1   Movement / SpatialHash       │ DescribeAccess가 충돌하지 않는
       Phase 2   Collision / NavThrottle      │ 시스템끼리 SystemBatch로 묶어
       Phase 3   Health / Navigation          ┘ JobSystem 병렬 실행
       Phase 4   StatusEffect
       Phase 5   Vision                       ← Phase 1이 채운 SpatialIndex 소비
       Phase 8   BehaviorTree
       Phase 9   (커스텀 gameplay pass 예약)
       Phase 10  MCTS                         ← 5초 간격·50 iteration 저빈도
  └─ CommandBuffer::Flush(world)              ← 순회 중 생성/삭제 지연분 일괄 반영
```

Phase 번호는 코드에서 직접 확인 가능한 계약이다:
`Engine/Public/ECS/Systems/CoreSystems.h`(Player/AI=0, Movement=1, Collision=2, Health=3),
`TransformSystem.h`(0), `SpatialHashSystem.h`(1), `NavigationThrottleSystem.h`(2),
`NavigationSystem.h`(3), `StatusEffectSystem.h`(4), `VisionSystem.h`(5), `BehaviorTreeSystem.h`(8),
`MCTSSystem.h`(10 — 주석에 "Phase 10 keeps MCTS separate from BT(8) and custom gameplay pass(9)").

핵심 주장: **Phase 번호는 단순한 우선순위가 아니라 "데이터 생산 → 소비" 순서의 인코딩**이다.
Transform(0)이 world 행렬을 갱신해야 SpatialHash(1)가 올바른 위치를 격자에 넣고,
그래야 Vision(5)이 올바른 spatial index를 소비한다. Phase를 틀리면 컴파일은 되지만
**1프레임 지연된(stale) 데이터로 동작하는 논리 버그**가 된다 — 실제로 겪었다(④-1).

### 2.3 스케줄러 내부 — 2층 계약

`Engine/Private/ECS/SystemScheduler.cpp`:

1. **순서 계층(Phase)**: `map<uint32_t, ...>`가 Phase 오름차순 실행을 보장.
2. **병렬 계층(접근권)**: 한 Phase 안에서 각 시스템의 `DescribeAccess` 결과를 받아,
   `SystemAccessConflicts`(같은 컴포넌트에 write가 겹치거나, world 구조 변경, 또는 unknown이면 충돌)로
   서로 안전한 시스템들을 **그리디하게 SystemBatch로 묶는다**. 충돌이 나면 배치를 flush하고 새 배치 시작.
3. 배치 크기 ≥ 2이고 JobSystem이 주입돼 있으면 잡으로 병렬 제출 + `WaitForCounter`, 아니면 순차.
4. ExecutionPlan은 dirty 플래그로 시스템 등록이 바뀔 때만 재빌드. 배치 수/제출 잡 수는
   `WINTERS_PROFILE_COUNT`로 매 프레임 계측한다.

기본값이 중요하다. `ISystem::DescribeAccess`의 디폴트는 `UnknownWritesAll()` —
**접근권을 선언하지 않은 시스템은 절대 병렬 배치에 못 들어가고 항상 단독 순차 실행**된다.
병렬화 이득은 opt-in, 데이터 레이스 위험은 opt-out 불가. "선언 안 하면 안전하게 직렬화"가
이 스케줄러의 fail-safe 철학이다 (`Engine/Public/ECS/SystemAccess.h`의 `SystemAccessConflicts` 참고).

### 2.4 순회 중 구조 변경 — CommandBuffer

sparse set의 swap-remove 때문에 System이 `ForEach`로 dense 배열을 순회하는 도중 엔티티를
생성/삭제하면 순회 컨테이너가 흔들린다. 그래서 `CCommandBuffer`
(`Engine/Public/ECS/CCommandBuffer.h`, `Engine/Private/ECS/CommandBuffer.cpp`)가
`DeferCreate/DeferDestroy/DeferCommand`로 명령을 mutex 보호 벡터에 기록하고, 순회가 끝난 뒤
`Flush`에서 swap으로 로컬 이동 후 **creates → handleCreates → commands → destroys** 순서로 일괄 실행한다.
병렬 잡에서 여러 워커가 동시에 defer할 수 있으므로 각 Defer가 `lock_guard`로 보호된다.
대표 사용처는 `CHealthSystem` — hp ≤ 0 엔티티를 `m_pCmdBuf->DeferDestroy(e)`로 지연 파괴한다
(`Engine/Private/ECS/Systems/CoreSystems.cpp:119`). EnTT/Unity ECS의 command buffer와 같은 패턴이다.

### 2.5 계층(Transform)과 삭제 무결성

- `TransformComponent`(`Engine/Public/ECS/Components/TransformComponent.h`)는 local pos/rot/scale +
  캐시된 local/world 행렬 + parent/children + dirty 플래그 2개를 든 plain struct. Setter가
  dirty를 자동 세팅하고, `CTransformSystem`이 루트부터 재귀로 world = local × parent를 전파한다.
- `CWorld::DestroyEntity`(`Engine/Private/ECS/World.cpp`)는 파괴 전에 **자식들을 루트로 승격**
  (`m_Parent = NULL_ENTITY`)시켜 고아 참조를 막고, 부모의 children 리스트에서 자기를 제거한 뒤
  모든 스토어에서 Remove한다. 부모-자식 양방향 링크의 무결성을 삭제/재부모(SetParent) 시점에
  능동적으로 복구하는 정책이다.

---

## ③ 핵심 설계 결정과 트레이드오프

### 3-1. 왜 ECS인가

- **왜**: 초기 OOP 프레임워크(수업 계열 GameObject/Prototype 구조)에서는 (a) 기능 조합이 상속
  트리로 폭발하고 — "이동하는 + 공격하는 + 은신하는" 유닛마다 계층이 꼬인다, (b) 객체 단위
  힙 할당이라 같은 로직을 도는 데이터가 메모리에 흩어져 캐시 미스가 크고, (c) "어떤 시스템이
  어떤 데이터를 만지는지"가 클래스 내부에 숨어 병렬화 근거를 만들 수 없었다. (이 문단의 동기
  자체는 일반론이고, 아래 선택들이 내 코드에서의 실제 답이다.)
- **대안**: OOP 유지 + 컴포넌트 패턴(Unity GameObject식), 또는 풀 ECS 전환.
- **선택**: 시뮬레이션 상태(위치, 체력, 내비, 시야, 상태이상)는 ECS로, 렌더/연출 리소스는
  기존 OOP 객체로 남기는 **점진 이행**. 컴포넌트 데이터가 연속 배열이 되니 순회가 캐시
  친화적이고, System 단위로 read/write를 선언할 수 있어 병렬 배칭의 근거가 생겼다.
- **감수한 비용**: 두 세계를 잇는 sync 코드(③-5)와 두 식별자 체계(EntityID/EntityHandle)라는
  과도기 부채.

### 3-2. sparse set vs archetype

- **왜**: 컴포넌트 스토리지 방식이 ECS의 성능 특성을 결정한다.
- **대안**: archetype(같은 컴포넌트 조합끼리 chunk에 모아 저장 — Unity DOTS 방식)은 다중
  컴포넌트 join이 chunk 단위 선형 순회라 빠르지만, 구현 복잡도(조합 폭발, 컴포넌트 추가/제거 시
  chunk 간 이동)가 크다.
- **선택**: 타입당 독립 sparse set. 구현이 단순하고, Add/Remove가 O(1)이고, 단일 컴포넌트
  순회는 완전 연속이다. 혼자 만들고 혼자 디버깅하는 엔진에서 "내가 전부 설명할 수 있는 구조"를
  우선했다.
- **감수한 비용**: 다중 컴포넌트 join 시 T2/T3는 sparse 간접 접근이라 캐시 효율이 떨어진다.
  그리고 현재 `ForEach`는 **항상 T1이 driver**라 가장 작은 스토어를 자동으로 주도로 잡는 최적화
  (smallest-set driver)가 없다. 지금은 "작은 집합을 첫 인자로 두는" 호출 관행으로 커버하고,
  개선안은 ⑤에 적었다.

### 3-3. EntityID(uint32) + EntityHandle(64bit) 이중 식별자

- **왜**: 슬롯 재사용 구조에서는 삭제된 엔티티의 옛 id가 새 엔티티를 가리키는 ABA성 버그가 생긴다.
- **대안**: 전면 EntityHandle 전환(모든 스토어/시스템 시그니처 수정) vs 병행 유지.
- **선택**: 스토어와 hot loop는 index 기반 EntityID를 유지(sparse 배열 인덱싱과 궁합이 좋다),
  **수명이 중요한 참조(타겟, 시전 대상, 네트워크 매핑)만 EntityHandle**을 쓰게 했다.
  `TryResolve`가 generation 비교로 stale 핸들을 무효 처리한다.
- **감수한 비용**: "어디에 어떤 id를 쓸지"가 컨벤션에 의존한다. 헤더 주석으로 규칙을 박아뒀지만
  컴파일러가 강제하지는 못한다 — 마이그레이션 부채로 인정하고 면접에서도 숨기지 않는다.

### 3-4. Phase 정수 + 접근권 선언의 2층 계약

- **왜**: "순서 의존성"과 "데이터 경합"은 다른 문제다. 하나의 메커니즘(예: 전체 순차 실행)으로
  뭉개면 병렬화가 불가능하고, 하나의 그래프(의존성 DAG)로 풀면 초기 엔진엔 과설계다.
- **대안**: (a) 시스템 등록 순서 = 실행 순서(암묵), (b) 의존성 그래프 선언, (c) Phase + 접근권.
- **선택**: (c). **순서는 Phase 정수**로 사람이 읽을 수 있게 명시하고, **병렬은 같은 Phase 안에서
  DescribeAccess 충돌 검사**로만 허용한다. 기본값 `UnknownWritesAll`로 미선언 시스템은 자동 직렬화.
- **감수한 비용**: Phase 번호가 암묵적 파이프라인이라 **번호를 잘못 매기면 컴파일 타임에 못 잡는다**
  (④-1의 사고). 또 미선언 시스템이 배치를 끊어 병렬 이득을 깎는다 — 성능 병목이 되면 그 시스템에
  접근권 선언을 추가하는 opt-in 비용을 치른다.

### 3-5. OOP 레이어(씬/렌더러)와 ECS의 공존 전략

- **왜**: 렌더러(ModelRenderer), 카메라, ImGui 패널, FMOD 같은 리소스 소유 객체까지 ECS화하는 건
  전환 비용 대비 이득이 없었다. 반면 게임 상태는 서버 권위 복제·AI·병렬화 때문에 ECS가 필요했다.
- **대안**: 풀 ECS(렌더러도 컴포넌트화) vs 풀 OOP vs 경계선을 긋는 하이브리드.
- **선택**: 경계선. `Client/Public/Scene/Scene_InGame.h` 기준으로,
  - **씬(OOP)이 ECS를 소유한다**: `CWorld m_World`, `unique_ptr<CSystemSchedular> m_pScheduler`가
    Scene_InGame 멤버. 씬 전환이 곧 월드 수명이다.
  - **렌더 리소스는 OOP 객체로, EntityID를 조인 키로 연결**:
    `unordered_map<EntityID, unique_ptr<ModelRenderer>> m_ChampionRenderers` — 시뮬레이션 엔티티당
    렌더러 객체를 맵으로 매달아 둔다.
  - **경계는 명시적 sync 함수**: `SyncPlayerEntityTransformFromECS` / `SyncPlayerEntityTransformToECS`,
    `BindPlayerToECSChampion` 같은 어댑터가 OOP `CTransform`(플레이어 프레젠테이션)과 ECS
    `TransformComponent`(시뮬레이션 진실) 사이를 정해진 시점에만 오간다.
- **감수한 비용**: sync 함수가 늘어나고, "진실이 어느 쪽인가"를 항상 의식해야 한다. 서버 권위
  전환 후에는 ECS(스냅샷 적용 결과)가 진실이고 OOP 쪽은 표현 캐시라는 규칙으로 정리했다.
  이 규칙이 깨졌던 사고(클라 NavigationSystem이 스냅샷 yaw를 덮어쓴 건)는 네트워크 챕터에서 다룬다.

### 3-6. 컴포넌트 설계 원칙

`TransformComponent` 헤더 주석에 원칙이 그대로 적혀 있다: "ECS 친화 Plain Struct",
"struct의 멤버를 직접 쓰지 않고 헬퍼 함수(SetPosition) 사용".

1. **컴포넌트는 로직 없는 데이터**. Execute/Update 같은 행동 함수를 두지 않는다. 로직은 전부 System.
2. **파생 상태는 dirty 플래그로**. Setter가 `m_bLocalDirty/m_bWorldDirty`를 자동 세팅하고,
   재계산은 TransformSystem이 프레임에 한 번 몰아서 한다. 변경 없는 서브트리는 스킵된다.
3. **경계 위생**: 컴포넌트 헤더 상단 주석 — "Windows/Engine_Defines 오염을 Shared/GameSim TU로
   전이시키지 않는다". 컴포넌트는 Client/Server/Shared가 함께 보는 데이터 계약이므로 무거운
   헤더/매크로를 끌고 들어오면 안 된다. Shared 쪽은 `Check-SharedBoundary.ps1`이 직접
   `ECS/*` include를 빌드 실패로 강제한다.
4. **정직한 예외**: `m_vecChildren`이 `std::vector`라 컴포넌트 안에 힙 간접 참조가 있다.
   계층 관계라는 가변 길이 데이터의 실용적 타협이고, hot loop(행렬 전파)는 캐시된 children으로
   돌므로 감수했다 — 순수 POD 원칙과의 트레이드오프로 설명할 수 있어야 한다.

### 3-7. DLL 경계 (요약 — 상세는 cpp 챕터)

`CWorld`는 non-template 멤버(CreateEntity 등)만 `WINTERS_ENGINE`(dllexport)로 마킹하고,
`AddComponent/ForEach` 같은 템플릿 멤버는 헤더에서 includer 측 인스턴스화한다. unordered_map/
unique_ptr 멤버의 C4251 경고는 push/pop으로 범위 억제. 그리고 `SystemScheduler.h` 주석에 박아둔
핵심 함정 — **MSVC는 dllexport 클래스의 모든 특수 멤버 함수를 강제 인스턴스화하려 해서,
unique_ptr 멤버가 있으면 암묵 copy ctor 생성이 실패(construct_at 에러)한다. copy를 명시적
`= delete`하면 copy 경로 인스턴스화를 스킵한다.** 이 패턴을 CWorld/CSystemSchedular 등 export
클래스 전반에 반복 적용했고 gotchas 규칙으로 승격했다.

---

## ④ 어려웠던 점과 해결

### 4-1. Phase swap 사고 — "Phase는 데이터 흐름이다"를 몸으로 배운 건

- **증상**: 첫 미니언 웨이브 라인 클래시에서 한쪽 팀 근접 미니언만 제자리에서 run/attack
  애니메이션을 재생하며 위치가 멈추는 stuck. 비대칭이라 더 헷갈렸다.
- **원인 1 (Phase 순서 race)**: 당시 배치가 Nav 시스템이 AI보다 **앞** Phase였다. AI가 뒤
  Phase에서 Chase를 결정하고 nav target을 갱신해도 Nav는 이미 그 프레임 실행이 끝난 뒤 —
  경로 재계산은 다음 프레임에야 일어나고, 첫 Chase 프레임은 LaneMove의 **stale velocity**
  (웨이포인트 방향)로 움직였다. 해결은 Phase swap: AI가 먼저 결정하고(생산) 같은 프레임에
  Nav가 소비하도록 순서를 뒤집었다.
- **원인 2 (silent fail 결합)**: Pathfinder가 도달 불가 목표에 empty path를 조용히 반환하고
  NavSystem이 속도 0만 세팅 → Chase 상태 + 제자리 애니. Chase 한정 직선 fallback과
  `Nav::PathEmpty` 같은 profiler counter를 추가해 "조용히 실패하는 경로"를 관측 가능하게 만들었다.
- **교훈 → 규칙화**: "ECS Phase 순서 = data dependency 순서", "Pathfinder empty path 시
  silent fail 금지"를 gotchas로 박제했다. 또 코드 추론 3회가 빗나가는 동안 counter 계측 5분이
  정답을 줬다 — 이후 디버깅 파이프라인 원칙(계측 우선)의 근거 사례가 됐다.
- **현재 상태 주의**: 이후 서버 권위 이행으로 클라이언트 시스템 배치가 재편됐다(현재 클라
  `NavigationSystem.h`는 Phase 3, 복제 챔피언에 대한 클라 nav 이동은 비활성). 사고 자체는
  당시 클라 시뮬레이션 기준의 이야기로 설명한다.

### 4-2. dllexport + unique_ptr = construct_at 에러

Engine을 DLL로 나눈 직후, unique_ptr 멤버를 든 export 클래스들이 이해하기 어려운
construct_at 컴파일 에러를 냈다. 원인은 위 ③-7 — MSVC의 특수 멤버 강제 인스턴스화.
해결(명시적 copy delete)을 찾은 뒤 한 클래스만 고치고 끝내지 않고, `SystemScheduler.h`에
"★ 중요" 주석으로 이유까지 남기고 gotchas 규칙으로 승격해 같은 사고의 재발을 막았다.

### 4-3. 순회 중 삭제 — 제약을 API로 격리

sparse set을 선택한 순간 "순회 중 구조 변경 금지"라는 제약이 파생됐다. 이걸 호출자 주의사항
(주석)으로 남기는 대신 CommandBuffer라는 **명시적 API로 격리**했다. HealthSystem이 죽은
엔티티를 즉시 파괴하지 않고 DeferDestroy하는 코드가 그 계약의 실행 예다. TransformSystem에는
반대로 "실제 엔진 흐름에선 Execute 중 엔티티 파괴 없음"이라는 안전 가정을 주석으로 명시해,
어디까지가 보장이고 어디부터가 가정인지 구분해 뒀다.

### 4-4. 자기 리뷰 마커를 코드에 남기는 습관

`World.h:12`에는 지금도 "왜 여기에 따로 using을 뺀 거지? → 이거 무조건 수정, 절대 헤더에
이렇게 두면 안 됨"이라는 자기 리뷰 주석이 있다. 미해결 결정 지점을 코드에 표시해 두고,
반복되는 실수 패턴(헤더 using 오염, dllexport+unique_ptr, silent fail)은 gotchas 문서의
날짜별 규칙으로 승격하는 루프를 돌린다. "실수 → 재발 방지 규칙"이 문서화된 흐름 자체를
품질 관리 방식으로 이야기할 수 있다.

---

## ⑤ 향후 개선 방향

1. **ForEach smallest-set driver**: 현재 T1 고정 driver를 각 스토어 Count 비교 후 최소 집합
   주도로 바꾸면 호출 순서 관행 의존이 사라진다. 측정(스토어 크기 비대칭이 실제로 큰 쿼리)부터.
2. **EntityID → EntityHandle 마이그레이션 완결**: 수명이 걸린 참조를 전수 조사해서 핸들로
   통일하고, index-only id는 스토어 내부 구현 디테일로 감춘다.
3. **ISystemScheduler 인터페이스 추상화**: `SystemScheduler.h` 주석에 예약해 둔 대로, 씬이
   구체 타입을 직접 들지 않게 분리(파이버 잡 시스템 전환과 함께).
4. **접근권 선언 커버리지 확대**: UnknownWritesAll로 남아 있는 시스템들에 read/write 선언을
   붙여 병렬 배치 폭을 넓힌다 — Scheduler의 배치 카운터(`Scheduler::ParallelBatches`)로
   개선을 수치로 증명한다.
5. **시스템 내부 chunk 병렬화**: 지금은 "시스템 단위" 병렬(배칭)과 Transform 루트 분할만 있다.
   대규모 컴포넌트 순회를 chunk 단위 잡으로 쪼개는 다음 단계가 남아 있다.
6. **Shared/GameSim 어댑터 경계 유지**: 시뮬레이션 컴포넌트는 Shared에서 `Core/Ecs` 어댑터를
   통해서만 접근(직접 `ECS/*` include는 빌드 실패). ECS 코어를 엔진 내부 구현으로 캡슐화하는
   방향을 계속 민다.

---

## ⑥ 면접 Q&A

**Q1. ECS를 왜 도입했고, 어떻게 구현했나?**
- 골격: 동기 3개(상속 조합 폭발 / 캐시 / 병렬화 근거) → 구현은 타입별 sparse set
  (sparse/dense/data 3배열, swap-remove O(1)) + type_index 맵의 CWorld + Phase 스케줄러.
- 꼬리질문 대비: "archetype은 왜 안 썼나" → 3-2의 트레이드오프(구현 단순성 vs join 캐시 효율,
  1인 개발에서 전부 설명 가능한 구조 우선)로 답. "그래서 뭐가 빨라졌나"에는 프로파일러
  counter/scope로 측정한다는 원칙과 함께, 연속 배열 순회·dirty 스킵·병렬 배칭이 이득 지점임을 짚는다.

**Q2. 삭제된 엔티티를 다른 시스템이 참조하고 있으면 어떻게 되나?**
- 골격: generation slot + free-list. 핸들은 (generation<<32|index) 패킹이고, 슬롯 재사용 시
  generation이 바뀌므로 TryResolve가 stale 핸들을 무효로 판정한다. alive는 generation 홀수
  비트로 인코딩해 별도 플래그가 없다.
- 꼬리질문 대비: "generation이 wrap하면?" → uint32 generation이 같은 슬롯에서 2^31회 재사용돼야
  충돌 — 실질적으로 무시 가능하다고 답하되 한계로 인지. "왜 EntityID가 아직 남아있나" →
  마이그레이션 부채를 솔직히 말하고 규칙(수명 중요하면 핸들)으로 관리 중이라고 답.

**Q3. 컴포넌트 순회 중에 엔티티를 삭제하면?**
- 골격: swap-remove가 dense 배열을 흔들어 이터레이터가 무효화되므로 금지. CommandBuffer에
  DeferDestroy로 기록하고 순회 종료 후 Flush에서 creates→commands→destroys 순서로 일괄 실행.
  HealthSystem의 사망 처리가 실제 사용처.
- 꼬리질문 대비: "병렬 잡에서 defer하면?" → 각 Defer가 mutex 보호, Flush는 swap으로 로컬
  이동 후 실행이라 수집과 실행이 분리된다. "Unity/EnTT에도 있죠?" → 같은 패턴임을 인정하고,
  내 경우 sparse set 제약에서 스스로 도출했다는 맥락을 붙인다.

**Q4. 시스템 실행 순서는 어떻게 보장하나?**
- 골격: 2층 계약. 순서는 Phase 정수(map 오름차순), 병렬은 같은 Phase 안에서 DescribeAccess
  read/write 충돌 검사로 그리디 배칭. 순서 의존성과 데이터 경합을 별개 메커니즘으로 분리한 게 핵심.
- 꼬리질문 대비: "Phase 번호 잘못 매기면?" → 컴파일은 되지만 1프레임 stale 데이터 버그가 된다며
  미니언 stuck 사고(Q5)로 연결. "의존성 그래프가 더 낫지 않나" → 시스템 수십 개 규모에선 사람이
  읽을 수 있는 정수 파이프라인이 디버깅 우위, 규모가 커지면 그래프로 이행 여지 인정.

**Q5. 이 구조에서 겪은 가장 기억에 남는 버그는?**
- 골격: 미니언 stuck 사고. 증상(한쪽 팀만 제자리 애니) → 원인 1: AI(결정)가 Nav(소비)보다 뒤
  Phase라 첫 Chase 프레임이 stale velocity로 이동 → Phase swap으로 같은 프레임 내
  생산→소비 보장. 원인 2: empty path silent fail → Chase 한정 fallback + counter 계측.
- 꼬리질문 대비: "어떻게 찾았나" → 코드 추론 3회 실패 후 profiler counter 5분이 정답.
  "재발 방지는" → gotchas 규칙 2건("Phase = data dependency", "silent fail 금지") 박제.

**Q6. 시스템을 병렬로 돌리면 데이터 레이스는 어떻게 막나?**
- 골격: 컴파일 타임 태그가 아니라 런타임 read/write 선언(DescribeAccess). 같은 컴포넌트에
  write가 겹치거나 world 구조를 바꾸거나 선언이 없으면(Unknown) 무조건 충돌 처리 → 배치 분리.
  기본값이 UnknownWritesAll이라 선언 안 한 시스템은 자동으로 순차 — 안전이 기본, 병렬이 opt-in.
- 꼬리질문 대비: "선언이 실제 접근과 다르면?" → 현재는 신뢰 기반, 검증(디버그 빌드에서 접근
  추적) 미구현을 한계로 인정. "그 보수적 기본값의 비용은?" → 미선언 시스템이 배치를 끊는 성능
  손해를 counter로 관찰하고 필요할 때만 선언을 추가한다.

**Q7. 멀티 컴포넌트 쿼리 성능은 어떤가?**
- 골격: ForEach는 T1 dense 배열이 driver, 나머지는 Has 필터. driver 집합 크기가 성능을
  좌우하므로 작은 집합을 첫 인자로 두는 관행으로 통제 — 이게 암묵 계약임을 알고 있고,
  smallest-set driver 자동 선택이 개선안.
- 꼬리질문 대비: "archetype이면 해결되지 않나" → 맞다, 대신 컴포넌트 추가/제거 시 chunk 이동
  비용과 구현 복잡도를 치른다. 현 규모(수십 종 컴포넌트, 수백 엔티티)에서는 sparse set +
  브로드페이즈(SpatialIndex)로 충분하다는 측정 기반 판단.

**Q8. 기존 OOP 씬 구조와 ECS는 어떻게 공존시켰나?**
- 골격: 씬(OOP)이 CWorld/스케줄러를 소유하고, 렌더러 같은 리소스 객체는
  `unordered_map<EntityID, unique_ptr<ModelRenderer>>`처럼 EntityID를 조인 키로 매달았다.
  경계는 명시적 Sync 함수로만 넘는다. 시뮬레이션 진실은 ECS, OOP 쪽은 표현 캐시.
- 꼬리질문 대비: "왜 렌더러는 ECS화 안 했나" → 리소스 소유(디바이스 버퍼, 애니 상태)는 객체
  수명 모델이 자연스럽고 전환 이득이 없었다. "진실 규칙이 깨진 적은?" → 클라 nav 시스템이
  스냅샷 yaw를 덮어쓴 사고 — 서버 권위에서 복제 엔티티에 로컬 이동 시스템을 돌리지 않는
  규칙으로 정리(네트워크 챕터에서 상술).

**Q9. 컴포넌트를 설계할 때 지키는 원칙은?**
- 골격: 로직 없는 plain struct + 헬퍼 setter로 dirty 플래그 자동화 + 파생 데이터(행렬)는
  System이 일괄 재계산 + 컴포넌트 헤더는 무거운 의존(Windows 매크로)을 Shared로 전이시키지
  않는 경계 위생.
- 꼬리질문 대비: "컴포넌트에 vector가 있던데(POD 위반)?" → children 계층의 실용적 타협이라고
  선제적으로 인정하고, hot loop 영향과 대안(별도 hierarchy 스토어)을 논할 수 있게 준비.

---

## ⑦ 다른 챕터와의 연결

- **잡 시스템·병렬화 챕터**: 이 챕터의 스케줄러 배칭이 잡을 던지는 곳 — Chase-Lev work-stealing
  deque, owner-only push race 해결, WaitForCounter의 help-stealing은 그쪽에서 상술한다.
  여기서는 "배치 ≥ 2 + JobSystem 주입 시 병렬"이라는 소비자 관점까지만.
- **네트워크·서버 권위 챕터**: EntityHandle/EntityIdMap이 스냅샷 복제의 키가 되고, "ECS가
  진실, OOP는 표현"이라는 규칙이 SnapshotApplier와 클라 예측에서 어떻게 강제되는지 연결된다.
- **프로파일링·최적화 챕터**: Scheduler/Transform의 `WINTERS_PROFILE_COUNT` 계측, "최적화는
  counter/scope로 증명한다" 원칙, Phase swap 사고에서 counter가 정답을 준 사례가 이어진다.
- **월드·스트리밍 챕터**: SpatialIndex(매 프레임 재빌드 uniform grid)와 WorldPartition
  (히스테리시스 셀 상태 머신)은 ECS 위에 얹힌 공간 서비스 — 구조는 그쪽에서.
- **cpp 세트**: DLL 경계의 템플릿/ C4251 / dllexport 특수 멤버 문제의 문법적 상세는
  `.md/interview/cpp/02_compile_link_dll.md`, ECS 아키텍처 일반론은
  `.md/interview/cpp/11_architecture_ecs.md` 참고.

---

### 근거 파일 (전부 이 챕터 작성 시점에 직접 확인)

| 주제 | 파일 |
|---|---|
| sparse set 스토어 | `Engine/Public/ECS/ComponentStore.h` |
| generation slot / EntityHandle | `Engine/Public/ECS/Entity.h` |
| World / ForEach / DLL 경계 | `Engine/Public/ECS/World.h`, `Engine/Private/ECS/World.cpp` |
| 스케줄러 / 접근권 배칭 | `Engine/Public/ECS/SystemScheduler.h`, `Engine/Private/ECS/SystemScheduler.cpp`, `Engine/Public/ECS/SystemAccess.h`, `Engine/Public/ECS/ISystem.h` |
| Phase 번호 | `Engine/Public/ECS/Systems/CoreSystems.h`, `TransformSystem.h`, `SpatialHashSystem.h`, `NavigationThrottleSystem.h`, `NavigationSystem.h`, `StatusEffectSystem.h`, `VisionSystem.h`, `BehaviorTreeSystem.h`, `MCTSSystem.h` |
| CommandBuffer | `Engine/Public/ECS/CCommandBuffer.h`, `Engine/Private/ECS/CommandBuffer.cpp`, `Engine/Private/ECS/Systems/CoreSystems.cpp:119` |
| Transform 계층 | `Engine/Public/ECS/Components/TransformComponent.h`, `Engine/Private/ECS/Systems/TransformSystem.cpp` |
| OOP 공존 | `Client/Public/Scene/Scene_InGame.h` |
