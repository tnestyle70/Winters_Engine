# 아키텍처 · OOP vs ECS · 레이어 경계

> 대상: Winters 엔진(C++17/DX11, LoL 스타일 클라+서버 자작 엔진)을 직접 만든 사람.
> 목표: "왜 상속 트리 대신 ECS인가", "시스템 실행 순서와 데이터 흐름", "서버 권위 결정론", "레이어 경계를 어떻게 절단했나"를 개념적으로 정확히 설명하고 내 코드로 증명한다.

---

## 1. OOP 상속 트리의 한계와 ECS

### ① 한 줄 본질
"게임 오브젝트가 수백 종으로 늘어나면 상속 트리는 조합 폭발과 다이아몬드 문제로 무너지고, ECS는 **데이터를 타입별로 연속 저장(SoA)** 하고 **동작을 System으로 분리**해 조합(composition)·캐시 지역성·병렬화를 얻는 데이터 지향(data-oriented) 설계다."

### ② 기본 개념

**상속 트리의 한계.** `GameObject → Character → Champion → Ashe`처럼 세로로 파면, "날아다니면서(Flying) 공격도 하는(Attacker) 미니언"처럼 **직교하는 능력의 조합**은 표현할 수 없다. 능력마다 클래스를 만들면 `2^n` 조합 폭발이 나고, 다중 상속으로 풀면 다이아몬드 상속(diamond inheritance)과 가상 상속(virtual inheritance)의 vtable 복잡도가 따라온다. 더 근본적으로, OOP 객체는 **한 객체의 모든 필드가 메모리에 뭉쳐(AoS, Array of Structs)** 있어서, "모든 유닛의 위치만 갱신"하는 루프가 위치 이외의 필드(체력, 인벤토리, AI 상태)까지 캐시라인에 끌고 와 캐시를 오염시킨다.

**ECS의 답.** 세 가지를 분리한다.
- **Entity**: 데이터도 로직도 없는 **식별자(id)** 일 뿐. "이 게임 오브젝트가 존재한다"만 의미한다.
- **Component**: **순수 데이터(POD)**. 가상 함수도 로직도 없다. 같은 타입 컴포넌트는 한 배열에 연속 저장된다.
- **System**: **로직**. 특정 컴포넌트 조합을 가진 엔티티들을 훑으며(iterate) 동작한다.

핵심 이득은 세 가지다. (1) **조합**: 능력을 컴포넌트 추가/제거로 붙였다 뗀다 — 클래스 폭발이 없다. (2) **캐시 지역성**: `Position`만 담은 배열을 선형 순회하면 프리페처(prefetcher)가 잘 먹고 캐시 미스가 준다. (3) **병렬화**: 로직이 데이터 위에 얹히므로 "충돌 없는 System끼리 병렬"이 스케줄 단계에서 정적으로 판별 가능하다.

| | OOP 상속 트리 | ECS |
|---|---|---|
| 능력 추가 | 새 클래스 / 다중 상속 | 컴포넌트 추가/제거 |
| 메모리 배치 | 객체 단위 AoS | 타입별 연속 배열(SoA) |
| "위치만 갱신" 루프 | 캐시라인에 무관 필드 동반 | 필요한 배열만 선형 순회 |
| 병렬화 | 객체 간 의존 추적 어려움 | 접근 선언으로 정적 판별 |
| 런타임 다형성 | virtual/vtable | System 분기 · enum 태그 · 훅 등록 |

### ③ 심화 (꼬리질문 대비)

- **"ECS가 항상 빠른가?"** 아니다. 컴포넌트 한 종류만 다루는 좁은 순회에서 유리하고, "엔티티 하나의 모든 필드를 자주 함께 만진다"면 AoS(=OOP 객체)가 캐시상 더 나을 수 있다. ECS는 **넓은 순회·희소한 조합·병렬화**가 잦은 게임 시뮬레이션에 맞춘 트레이드오프다.
- **AoS vs SoA**: ECS의 컴포넌트 스토어는 컴포넌트 타입별로 SoA에 가깝다(타입마다 별도 dense 배열). 단, 조회 국소성이 중요한 곳(브로드페이즈 셀)에서는 일부러 여러 필드를 한 구조체로 묶어 AoS로 비정규화(denormalize)하기도 한다 — 아래 §Winters 참조.
- **아키타입(archetype) vs 스파스셋(sparse-set)**: ECS 저장 방식은 크게 둘. 아키타입은 "같은 컴포넌트 조합"을 한 청크에 모아 순회 시 분기가 없지만 컴포넌트 추가/제거 때 엔티티를 청크 간 이사시킨다. 스파스셋은 타입별 dense 배열 + sparse 인덱스로, 추가/제거가 O(1)이고 구현이 단순하다. Winters는 **스파스셋**을 택했다.

---

## 2. Entity / Component / System 각각의 책임

### ② 기본 개념

- **Entity = 핸들**. Winters에는 두 종류가 있다. `EntityID`(index-only, 레거시)와 `EntityHandle`(index + generation, 수명 안전). 슬롯을 재사용할 때 index만 있으면 "파괴된 엔티티를 가리키던 id가 새 엔티티를 가리키는" ABA 문제가 난다. generation을 함께 저장하고 비교해 이를 막는다.
- **Component = POD**. `struct { float ...; }` 뿐. 가상 함수가 없어야 `trivially-copyable`/`standard-layout`이 되고, 그래야 `vector`에 연속 저장·`memcpy`·직렬화가 성립한다. 필드 없는 빈 구조체는 "존재 = 플래그"인 태그(tag) 컴포넌트로 쓴다.
- **System = 로직 + 접근 선언**. 어떤 Phase(실행 순서 그룹)에 속하는지, 어떤 컴포넌트를 Read/Write하는지를 스스로 선언한다. 이 선언이 병렬 스케줄링의 입력이 된다.

### ③ 심화

- **왜 컴포넌트에 vtable을 넣지 않나?** vtable 포인터(8바이트)가 모든 컴포넌트마다 붙으면 캐시 낭비 + 직렬화 불가(포인터는 프로세스마다 다르다). 다형성이 필요하면 **enum 태그 + 팩토리 디스패치**로 대체한다.
- **왜 System은 상태를 최소로 두나?** System이 프레임 간 상태를 많이 들면 그건 사실상 컴포넌트로 빠져야 할 데이터다. System은 "이번 프레임 데이터 위를 지나가는 순수 함수"에 가까울수록 병렬화·테스트가 쉽다.

---

## 3. Winters에서의 적용 — ECS 코어

### 스파스셋 컴포넌트 스토어 (SoA + swap-and-pop)

`Engine/Public/ECS/ComponentStore.h`의 `CComponentStore<T>`는 세 배열로 구성된 전형적 스파스셋이다.

```cpp
std::vector<uint32_t> m_vecSparse;   // entity  -> dense index
std::vector<EntityID> m_vecDense;    // dense index -> entity
std::vector<T>        m_vecData;     // 컴포넌트 연속 저장 (ComponentStore.h:65 "연속된 메모리 사용!")
```

- `Add`(11-22): dense 끝에 `push_back` → O(1).
- `Remove`(24-38): **마지막 원소를 삭제 슬롯으로 당겨넣고 pop_back** 하는 swap-and-pop. `m_vecData[index] = std::move(m_vecData[last]);`(32행). 덕분에 dense 배열에 구멍이 안 생겨 순회가 항상 연속이다.
- `Get`(40-49): sparse 인덱싱으로 O(1).

**왜 이 설계인가**: O(1) add/remove/lookup과 캐시 친화적 순회를 동시에 얻는다. 대가는 한 가지 — swap-and-pop이 **순회 중 원소 순서를 흔든다**. 그래서 System이 순회 도중 컴포넌트를 지우면 인덱스가 무효화된다(→ §5 지연 삭제로 연결).

### 이종(heterogeneous) 스토어를 한 컨테이너에: 타입 소거 + type_index

서로 다른 `T`의 `CComponentStore<T>`를 하나의 맵에 담아야 한다. `CWorld`는 **타입 소거(type erasure)** 를 쓴다 (`Engine/Public/ECS/World.h`).

```cpp
// World.h:18   순수가상 베이스 — 타입 비의존 연산만 노출
class IComponentStoreBase {
    virtual ~IComponentStoreBase() = default;
    virtual void Remove(EntityID) = 0;
    virtual bool Has(EntityID) const = 0;
};
// World.h:27   타입별 래퍼가 실제 CComponentStore<T> 소유
template<typename T> class CComponentStoreWrapper : public IComponentStoreBase { ... };
// World.h:212  이종 스토어를 base 포인터로 소유
std::unordered_map<std::type_index, std::unique_ptr<IComponentStoreBase>> m_mapStores;
```

`GetOrCreateStore<T>`(World.h:197-210)는 `std::type_index(typeid(T))`를 키로 스토어를 찾는다. 컴파일타임 타입 `T`를 **런타임 해시 키**로 접는 것이다. 여기서 다운캐스트가 `static_cast<CComponentStoreWrapper<T>*>`(World.h:92)인데도 안전한 이유는, **맵의 키(type_index)가 값의 실제 타입을 보장**하기 때문이다 — `dynamic_cast`(RTTI 런타임 검사)가 필요 없다.

> `type_info` 대신 `type_index`를 쓰는 이유: `type_info`는 복사·비교 불가라 컨테이너 키가 못 된다. `type_index`는 `type_info`를 감싼 값 의미론(value semantics) + 해시 가능 래퍼다.

### 제너레이셔널 엔티티 핸들 (ABA 방지)

`Engine/Public/ECS/Entity.h`. `EntityHandle`은 상위 32비트 generation + 하위 32비트 index를 `uint64_t` 하나에 packing한다.

```cpp
// Entity.h:47   Make(): generation을 상위 32비트로
handle.value = (static_cast<uint64_t>(generation) << 32) | static_cast<uint64_t>(id);
```

`CEntityManager::TryResolve`(Entity.h:152-169)는 슬롯의 현재 generation과 핸들의 generation이 다르면 실패시킨다. 흥미로운 디테일: generation을 **짝/홀로 살아있음을 표현**한다 — `IsAliveGeneration`은 `(generation & 1u) != 0`(홀수 = 살아있음, Entity.h:189-192). Destroy는 generation을 다음 죽은(짝수) 값으로, Create는 다음 살아있는(홀수) 값으로 밀어, "슬롯 재사용 시 generation이 반드시 바뀐다"를 보장한다. 이게 use-after-free/dangling 재사용 슬롯 참조를 막는 장치다.

### POD 컴포넌트 + dirty flag 캐싱

`Engine/Public/ECS/Components/CoreComponents.h`의 `VelocityComponent`/`HealthComponent`/`ColliderComponent`는 가상 함수 없는 순수 데이터 struct에 default member initializer만 둔다(10-28). `PlayerTag`(30-33)는 필드 없는 빈 태그다.

`TransformComponent`(`Engine/Public/ECS/Components/TransformComponent.h`)는 두 가지 면접 포인트를 한 파일에 담는다.
- **dirty flag lazy 재계산**: `m_bLocalDirty`/`m_bWorldDirty`(28-29)로 로컬 SRT 행렬과 월드 행렬 재계산을 지연시킨다. Setter(`SetPosition` 등, 32-49)가 반드시 두 dirty를 세팅하도록 강제해 멤버 직접 쓰기를 막는다(13행 주석 "멤버를 직접 쓰지 않고 헬퍼 함수 사용"). 매 프레임 전부 재계산하지 않고 변경된 것만 다시 계산한다.
- **값 반환 vs const 참조 반환**: `GetWorldPosition()`(58-61)은 월드 행렬에서 **즉석 계산한 임시 Vec3**라 값으로 반환한다(참조하면 댕글링). `GetLocalPosition()`(64)은 실멤버라 `const Vec3&`로 반환한다. 주석이 이 구분을 명시(57, 63행).

### 상속이 아니라 조합으로: EntityBlueprint

프로토타입/복제를 상속 기반 `Clone()` 가상 함수가 아니라 **`std::function` 설치자(installer) 리스트**로 구현했다 (`Engine/Public/ECS/Systems/EntityBlueprint.h`).

```cpp
using Installer = std::function<void(CWorld&, EntityID)>;
std::vector<Installer> m_vecInstallers;   // EntityBlueprint.h:16, 37
```

`Spawn`(`EntityBlueprint.cpp:6-19`)은 엔티티를 만든 뒤 (1) 불변 원본 리소스 설치자, (2) per-instance 인자 설치자를 순서대로 실행하는 **2단계 초기화**다. 상속 없이 함수 합성으로 다형적 생성을 얻었다.

---

## 4. 시스템 실행 순서와 데이터 흐름 (Phase)

### ① 한 줄 본질
"Phase는 **정수로 표현된 시스템 실행 순서 그룹**이고, 데이터는 낮은 Phase에서 높은 Phase로 단방향으로 흐른다. 같은 Phase 안에서 **접근(access) 충돌이 없는 시스템끼리만 병렬로 묶는다**."

### ② 기본 개념 + Winters 적용

**System 인터페이스.** `ISystem`(`Engine/Public/ECS/ISystem.h`)은 다형성 베이스다.
```cpp
virtual ~ISystem() = default;                       // 다형적 삭제 안전
virtual uint32_t GetPhase() const = 0;              // 실행 순서를 "데이터화"
virtual void Execute(CWorld&, float) = 0;
virtual void DescribeAccess(CSystemAccessBuilder& b) const { b.UnknownWritesAll(); } // 안전 기본값
```
`GetPhase`가 순수가상인 이유는 모든 시스템이 실행 순서를 스스로 밝히도록 계약을 강제하기 위해서고, `DescribeAccess`가 **본문 있는 가상**인 이유는 대부분의 시스템이 오버라이드하지 않아도 되게 하기 위해서다. 단 그 기본값이 `UnknownWritesAll()` — "이 시스템은 뭐든 쓴다"는 **보수적 기본값**이라, 접근을 선언하지 않은 시스템은 절대 병렬화되지 않는다. (안전을 기본값으로 두는 설계.)

**Phase 정렬.** `CSystemSchedular`(`Engine/Public/ECS/SystemScheduler.h`)는 `std::map<uint32_t, ...>`로 Phase를 담는다(39행).
```cpp
map<uint32_t, vector<unique_ptr<ISystem>>> m_mapPhases;   // SystemScheduler.h:39
```
왜 `unordered_map`이 아니라 `map`인가? **실행 계획을 Phase 오름차순으로 순회해야** 하므로 정렬된 키 순회(정렬 트리)가 필요하다. 이게 "map vs unordered_map을 언제 고르나"의 구체적 답이다 — 순서가 의미를 가지면 map.

**실제 Phase 배치.** Phase 값은 각 시스템 헤더에 정수로 박혀 있다: Player/AI=0, Movement=1, Collision=2, Health=3 (`Engine/Public/ECS/Systems/CoreSystems.h:11,23,32,41,51`), StatusEffect=4(`StatusEffectSystem.h:17`), Vision=5(`VisionSystem.h:58`), BehaviorTree=8(`BehaviorTreeSystem.h:34`), MCTS=10(`MCTSSystem.h:30`). "입력/AI 결정 → 이동 → 충돌 → 체력 정산"이라는 **데이터 흐름이 곧 Phase 순서**다. 낮은 Phase의 출력(위치)이 높은 Phase의 입력(충돌)이 되므로, Phase 간에는 순서 보장이 필요하고 Phase 안에서만 병렬을 시도한다.

**프레임레이트와 로직 주기 분리.** 무거운 로직은 같은 파이프라인에 있어도 매 프레임 돌 필요가 없다. `CMCTSSystem::Execute`(`Engine/Private/ECS/Systems/MCTSSystem.cpp:14-17`)는 accumulator로 고정 간격 실행한다.
```cpp
m_fAccumDt += fTimeDelta;
if (m_fAccumDt < TICK_INTERVAL) return;   // 간격 미달이면 이번 프레임은 skip
m_fAccumDt = 0.f;
```

**접근 충돌 기반 병렬 배치.** 각 시스템이 `DescribeAccess`로 Read/Write 컴포넌트를 `type_index`로 선언하면(`Engine/Public/ECS/SystemAccess.h`), 스케줄러가 충돌 없는 시스템끼리 한 batch로 묶는다.
```cpp
// SystemAccess.h:77  충돌 규칙
inline bool SystemAccessConflicts(const SystemAccessDesc& lhs, const SystemAccessDesc& rhs) {
    if (lhs.bUnknown || rhs.bUnknown) return true;                     // 모르면 충돌로 간주
    if (lhs.bWritesWorldStructure || rhs.bWritesWorldStructure) return true; // 엔티티 생성/파괴는 배타
    // 같은 type_index에 한쪽이라도 Write면 충돌 (write-write / write-read)
}
```
`RebuildExecutionPlan`(`Engine/Private/ECS/SystemScheduler.cpp:45-83`)이 이 규칙으로 batch를 구성하고, `Execute`(85-132)는 batch 크기가 `kMinParallelBatchSize`(=2) 이상이면 `CJobSystem`으로 fork하고 `CJobCounter`로 join한다.

**왜 이 설계인가**: 데이터 레이스를 **런타임 lock이 아니라 스케줄 단계에서 정적으로 배제**한다. read-read는 병렬, write가 얽히면 직렬. Rust의 borrow checker가 컴파일타임에 하는 검사를, 런타임 메타데이터로 재현한 구조다.

### 데이터 흐름을 lock 없이 병렬화한 실제 사례: 2-pass Decision/Apply

`Client/Private/GamePlay/System/LocalUnitAISystem.cpp`의 미니언 AI가 "phase를 나눠 lock을 없앤" 대표 사례다.

- **DecisionPass**(61-98): `CJobSystem`으로 병렬 실행. world는 **읽기만** 하고, 결정(`MinionDecision` POD)을 per-worker 슬롯 버퍼에 push.
```cpp
// LocalUnitAISystem.cpp:354   슬롯이 스레드별로 분리 → mutex 불필요
void CLocalUnitAISystem::Push_Decision(const MinionDecision& dec) {
    const uint32_t slot = CJobSystem::Get_WorkerSlot();  // main=0, worker=idx+1
    if (slot >= m_vecDecisionsPerSlot.size()) return;
    m_vecDecisionsPerSlot[slot].push_back(dec);
}
```
`Get_WorkerSlot`은 thread_local `t_iWorkerIdx`로 슬롯을 계산한다 — 메인=0, 워커=idx+1 (`Engine/Private/Core/JobSystem.cpp:35-39`). 슬롯이 스레드마다 다르므로 **공유 쓰기가 없어 lock이 필요 없다**.
- **ApplyPass**(100-103): `WaitForCounter` 후 **메인 스레드 단독**으로 모든 슬롯을 순회하며 컴포넌트를 mutate.

원칙 한 줄: **"병렬 구간에는 공유 쓰기가 없고, 쓰기 구간에는 병렬이 없다."** 16개 미만이면 병렬화 오버헤드를 피해 순차 실행하는 `kParallelThreshold`(20행) 분기도 있다 — 병렬화는 공짜가 아니다.

---

## 5. 순회 중 구조 변경 금지 → 지연(collect-then-mutate)

`Engine/Public/ECS/World.h`의 `ForEach`(135-177)는 dense 배열을 인덱스로 선형 순회한다. 순회 도중 컴포넌트를 추가/삭제하면 §3의 swap-and-pop이 **인덱스를 흔들어** 이터레이터가 무효화된다. 해법은 "순회 중엔 읽기만, 변경은 나중에"다.

`Shared/GameSim/Champions/Jax/JaxGameSim.cpp`의 `Tick`(437-488)이 정석 사례다.
```cpp
std::vector<EntityID> finishedDashes;
world.ForEach<JaxDashComponent, TransformComponent>( ... {
    if (t >= 1.f) finishedDashes.push_back(entity);   // 순회 중엔 모으기만
});
for (EntityID entity : finishedDashes)                 // 순회 후 일괄 삭제
    world.RemoveComponent<JaxDashComponent>(entity);
```

---

## 6. 레이어드 아키텍처 (Client / Engine / Shared)와 의존성 방향

### ① 한 줄 본질
"상위 레이어만 하위 레이어를 참조하고 역방향은 금지 — Winters에서 `Shared/GameSim`(순수 게임 규칙)은 `Engine`(DX11/Win32)을 직접 include하지 않고, **어댑터 심(seam)** 을 통해서만 ECS에 닿는다."

### ② 기본 개념

- **Client**: 렌더/입력/예측. Engine과 Shared를 소비.
- **Engine**: DX11/RHI, ECS 코어, JobSystem 등 플랫폼·인프라. DLL로 빌드.
- **Shared/GameSim**: **결정론적 게임 규칙**(챔피언 스킬, 데미지). 클라와 서버가 **똑같이** 컴파일해 돌려야 하므로, DX11이나 Win32에 의존하면 안 된다. 서버는 콘솔 앱이라 그래픽이 없다.

의존성 방향이 뒤집히면(Shared가 Engine의 DX11 헤더를 끌어오면) 서버 빌드가 불가능하거나, `windows.h`/`using namespace`의 오염이 결정론 코드로 번진다.

### ③ 심화: 어댑터로 경계 절단 (Phase 7F)

이상은 "Shared가 Engine ECS를 전혀 몰라야 한다"이지만, 현실은 과도기다. Winters는 **타입 별칭 어댑터 심**을 먼저 심어 교체 비용을 파일 1개로 만들었다 (`Shared/GameSim/Core/World/World.h`).

```cpp
namespace SharedSim {
    // Temporary adapter boundary for Phase 7F.
    // GameSim은 이 파일을 include할 뿐 Engine ECS로 직접 들어가지 않는다.
    using World = ::CWorld;   // World.h:11
}
```

지금은 `#include "ECS/World.h"` 후 `using`으로 얇게 감싼 **패스스루(passthrough)** 라 컴파일 의존은 아직 Engine에 남아 있다(gotchas에 "위반으로 기록"). 하지만 핵심은, 백엔드를 교체할 때 **이 한 파일만 고치면 되는 단일 교체점**을 미리 만들어 뒀다는 것이다. 챔피언 코드는 실제로 이 심을 통해 접근한다 (`Shared/GameSim/Champions/Ashe/AsheGameSim.cpp:13-14`가 `Core/Ecs/TransformComponent.h`와 `Core/World/World.h`를 include하지, Engine 헤더를 직접 include하지 않는다).

그리고 이 규칙은 사람의 기억이 아니라 **빌드가 강제**한다. "Shared는 Engine을 직접 include 금지"는 컴파일러가 잡아줄 수 없는 규칙이므로, `GameSim.vcxproj`의 PreBuild 이벤트가 `Tools/Harness/Check-SharedBoundary.ps1`을 실행해(`Shared/GameSim/Include/GameSim.vcxproj:63`) `#include "ECS/..."` 같은 직접 참조를 텍스트 스캔으로 발견하면 빌드를 실패시킨다. **컴파일러가 못 막는 레이어 규칙은 lint로 기계 강제한다** — 규칙 문서만 있고 강제 장치가 없으면 언젠가 반드시 뚫린다.

> 면접 포인트: "거대 코드베이스에서 의존성 경계를 **점진적으로** 끊는 법" — 이상적 경계를 한 번에 강제하지 말고, typedef/using 어댑터 심을 먼저 심어 교체 비용을 `O(파일 1개)`로 만든 뒤 실제 절단은 나중에. 이상적 경계와 현실적 과도기를 구분해 **문서화**한 게 핵심이다.

### 챔피언 = 상속 계층이 아니라 free-function + 훅 등록

"150 챔피언 스케일"에서 깊은 상속 대신 **컴포넌트 + 자유함수 + 훅 등록**을 택했다. `Ashe`/`Fiora`/`Jax` 모두 베이스 클래스도 virtual도 없다.

```cpp
// AsheGameSim.h:10   namespace 안의 자유함수 묶음 — 상속 없음
namespace AsheGameSim {
    void RegisterHooks();
    void Tick(CWorld& world, const TickContext& tc);
}
// AsheGameSim.cpp:311   static bool로 멱등 등록 + 훅 함수포인터를 레지스트리에 꽂음
void RegisterHooks() {
    static bool_t s_bRegistered = false;
    if (s_bRegistered) return;
    CGameplayHookRegistry::Instance().Register(
        MakeGameplayHookId(eChampion::ASHE, GameplayHookVariant::Q_CastFrame), &OnQ);
    ...
    s_bRegistered = true;
}
```
챔피언별 상태는 `AsheSimComponent` 같은 ECS 컴포넌트로 분리된다. 가상함수 계층의 결합·빌드시간·다이아몬드 문제를 통째로 피하고, 데이터 테이블 + 컴포넌트 + 훅 등록으로 확장한다.

---

## 7. 결정론적 시뮬레이션 (서버 authoritative + 클라 예측)

### ① 한 줄 본질
"게임 상태의 진실(source of truth)은 **서버**에 있다. 서버가 결정론적으로 시뮬레이션한 결과를 스냅샷(snapshot)으로 내려보내고, 클라는 지연을 감추려 로컬로 예측(prediction)하되 서버 값이 도착하면 그걸로 정정(reconcile)한다."

### ② 기본 개념

- **왜 서버 권위인가**: 클라를 믿으면 치팅(위치 조작, 데미지 조작)이 뚫린다. 판정은 서버가 한다.
- **왜 결정론(determinism)인가**: 같은 입력 → 같은 출력이 보장돼야 스냅샷을 델타(변경분)로만 보내고, 지연보상(lag compensation) rewind가 성립한다. 결정론을 깨는 것들: 부동소수 순서 의존, `unordered_map` 순회 순서, 초기화 안 된 메모리, 플랫폼별 math.
- **직렬화 순서의 함정**: ECS 컴포넌트 저장 순서는 스폰/파괴(§3 swap-and-pop)에 따라 **매 틱 달라질 수 있다**. 그대로 직렬화하면 클라가 델타를 잘못 해석한다.

### Winters 적용: 안정 키(netId)로 재정렬

`Server/Private/Game/SnapshotBuilder.cpp`의 `Build`(106-138)는 엔티티를 모은 뒤 **`netId` 기준으로 정렬**하고 나서 스냅샷 행을 쓴다.

```cpp
// SnapshotBuilder.cpp:117
const auto entities = DeterministicEntityIterator<TransformComponent>::CollectSorted(world);
...
// SnapshotBuilder.cpp:134  netId로 재정렬 → 매 틱 일관된 순서
std::sort(sorted.begin(), sorted.end(),
    [](const SnapshotEntity& lhs, const SnapshotEntity& rhs){ return lhs.netId < rhs.netId; });
```
ECS 내부 순서가 흔들려도, `netId`라는 결정론적 안정 키로 재정렬해 클라가 매 틱 같은 순서로 델타를 해석하게 만든다.

### 클라 적용기: idempotent upsert

스냅샷은 "엔티티가 부분적으로만 존재"할 수 있는 상태에 반복 적용된다. `Client/Private/Network/Client/SnapshotApplier.cpp`(814-816)의 get-or-add 관용구:
```cpp
auto& pose = world.HasComponent<ReplicatedPoseComponent>(e)
    ? world.GetComponent<ReplicatedPoseComponent>(e)
    : world.AddComponent<ReplicatedPoseComponent>(e, ReplicatedPoseComponent{});
```
`AddComponent`가 참조를 반환하도록 설계됐기에 삼항(ternary)의 두 갈래가 같은 `T&` lvalue로 수렴해 `auto&`로 바인딩된다. "있으면 그 참조, 없으면 삽입 후 그 참조"를 한 줄로 표현한 **idempotent 업데이트**다.

> 예측/정정의 클라 쪽 상세(로컬 yaw 보호, ack 시퀀스)는 이 챕터 범위를 넘는다. 핵심만: 클라가 로컬로 미리 움직여도(prediction) 서버 스냅샷이 진실이며, 정정 시 튐(pop)을 줄이려 보간(interpolation)/보호 구간을 둔다.

### 지연보상(lag compensation) rewind + generation 재검증

서버 권위의 또 다른 축: 클라가 쏜 시점의 과거 월드로 되감아(rewind) 히트 판정한다. `Server/Public/Security/LagCompensation.h`의 `CLagCompensation`은 엔티티별 `std::deque<HistoryFrame>`으로 과거 상태를 저장한다(29행).

```cpp
// LagCompensation.h:13   constexpr 올림 나눗셈 — 컴파일 타임에 최대 되감기 틱 수 확정
static constexpr u64_t kMaxRewindTicks = (kMaxRewindMs * kTickRate + 999) / 1000;
// LagCompensation.h:22-27   과거 프레임에 generation을 함께 저장
struct HistoryFrame { u64_t tickIndex; EntityGeneration generation; LagCompensatedEntityState state; };
```

`HistoryFrame`이 `EntityGeneration`을 동반 저장하는 이유: 되감는 사이 ECS가 파괴된 엔티티 슬롯을 재사용하면 같은 `EntityID`가 **다른 엔티티**를 가리키는 ABA가 생긴다. §3의 generational handle과 같은 원리로 stale 프레임을 걸러낸다 — 엔티티 세대(generation) 개념이 스토리지 안전을 넘어 네트워크 보안 판정까지 관통하는 사례다.

---

## 8. 면접 Q&A

**Q1. OOP 상속으로 게임 오브젝트를 설계하면 뭐가 문제인가? ECS는 그걸 어떻게 푸나?**
직교하는 능력의 조합이 클래스 폭발(`2^n`)과 다이아몬드 상속을 부르고, AoS 레이아웃이 "필드 하나만 순회"할 때 캐시를 오염시킨다. ECS는 Entity(id) / Component(POD 데이터) / System(로직)을 분리해, 능력을 컴포넌트 조합으로 붙였다 떼고(조합), 타입별 연속 배열로 캐시 지역성을 얻고, 접근 선언으로 병렬화한다. Winters: `CComponentStore<T>`(ComponentStore.h)의 dense 배열 SoA, 챔피언을 상속 없이 컴포넌트+훅으로 구성(AsheGameSim).

**Q2. ECS에서 컴포넌트를 어떻게 저장해야 순회가 빠른가?**
스파스셋: `sparse`(entity→dense), `dense`(dense→entity), `data`(연속 컴포넌트) 세 배열. Add는 dense 끝에 push(O(1)), Remove는 swap-and-pop(O(1))이라 dense에 구멍이 없어 순회가 캐시 친화적. Winters `ComponentStore.h:24-38`. 단, swap-and-pop이 순회 순서를 흔드는 부작용 → 순회 중 삭제는 지연(JaxGameSim.cpp:437-488).

**Q3. 서로 다른 타입의 컴포넌트 스토어를 한 컨테이너에 담으려면?**
타입 소거: 순수가상 베이스 `IComponentStoreBase` + 템플릿 파생 래퍼 `CComponentStoreWrapper<T>` + `unordered_map<type_index, unique_ptr<base>>`. 다운캐스트는 `dynamic_cast`가 아니라 `static_cast`인데, **맵 키(type_index)가 실제 타입을 보장**해서 안전하다. Winters `World.h:18,27,212`. `type_info` 대신 `type_index`를 쓰는 이유는 값 의미론·해시 가능이라 컨테이너 키가 되기 때문.

**Q4. 파괴된 뒤 재사용된 엔티티 핸들(ABA)을 어떻게 구분하나?**
index만 있으면 못 구분한다. generation을 함께 저장하고 resolve 때 비교한다. Winters `EntityHandle`은 `(generation<<32)|index`를 u64에 packing(Entity.h:47)하고, `TryResolve`가 generation 불일치 시 실패시킨다(Entity.h:152-169). 세대는 짝/홀로 생사를 표현해 재사용 때 반드시 값이 바뀐다.

**Q5. ECS 시스템을 멀티스레드로 돌릴 때 데이터 레이스를 어떻게 막나? lock을 어디에 거나?**
lock을 거는 대신 **접근(access) 선언으로 스케줄 단계에서 충돌을 배제**한다. 각 System이 Read/Write 컴포넌트를 `type_index`로 선언(`DescribeAccess`)하면, 스케줄러가 `SystemAccessConflicts`로 write-write/write-read를 검사해 충돌 없는 시스템끼리만 병렬 batch로 묶는다(SystemScheduler.cpp:45-83). 선언 안 한 시스템은 `UnknownWritesAll`로 절대 병렬화 안 되는 보수적 기본값. 개별 데이터 수집은 per-worker 슬롯 버퍼로 lock 없이(LocalUnitAISystem.cpp:354) — "병렬엔 공유 쓰기 없음, 쓰기엔 병렬 없음".

**Q6. 컨테이너를 순회하면서 원소를 지우면 왜 위험하고, ECS에선 어떻게 푸나?**
swap-and-pop이 인덱스/이터레이터를 무효화한다. 순회 중엔 대상 id를 벡터에 모으기만 하고, 순회가 끝난 뒤 일괄 삭제한다(collect-then-mutate). Winters `JaxGameSim.cpp` Tick이 `finishedDashes`에 모았다가 루프 밖에서 `RemoveComponent`.

**Q7. Shared(게임 규칙) 레이어가 Engine(DX11)을 include하면 안 되는 이유는? 컴파일러가 못 막는 규칙을 어떻게 강제하나?**
서버는 그래픽 없는 콘솔 앱이고, 클라/서버가 같은 결정론 코드를 돌려야 하므로 DX11/Win32 오염이 번지면 안 된다. 언어는 이 규칙을 강제 못 하므로 (1) 어댑터 심(`SharedSim::World = ::CWorld`, World.h:11)으로 접근을 한 파일로 모으고, (2) `GameSim.vcxproj` PreBuild가 `Tools/Harness/Check-SharedBoundary.ps1`을 실행해 직접 include를 발견하면 빌드를 실패시킨다(GameSim.vcxproj:63). 교체 비용을 O(파일 1개)로 만드는 점진적 절단 + 기계 강제.

**Q8. 서버 권위 게임에서 ECS 상태를 어떻게 결정론적으로 직렬화하나?**
ECS 컴포넌트 순서는 스폰/파괴로 매 틱 흔들리므로 그대로 보내면 클라가 델타를 오해석한다. 안정 키(`netId`)로 재정렬 후 직렬화한다(SnapshotBuilder.cpp:117-138). 클라는 스냅샷을 idempotent upsert(get-or-add, SnapshotApplier.cpp:814)로 반복 적용한다.

---

## 9. 흔한 오답 / 함정

1. **"ECS는 항상 OOP보다 빠르다"** — 틀렸다. 넓은 순회·희소 조합·병렬화가 잦을 때 유리한 트레이드오프다. "한 엔티티의 모든 필드를 자주 함께 만진다"면 AoS(OOP 객체)가 캐시상 더 나을 수 있다.

2. **컴포넌트에 가상 함수를 넣기** — vtable 포인터가 캐시를 먹고 직렬화를 깬다(포인터는 프로세스마다 다르다). 다형성이 필요하면 enum 태그 + 팩토리 디스패치로 대체한다. Winters 컴포넌트는 전부 POD(CoreComponents.h).

3. **`ForEach` 순회 중 엔티티/컴포넌트 생성·삭제** — swap-and-pop이 인덱스를 무효화한다. 반드시 collect-then-mutate로 지연시켜라(JaxGameSim.cpp). "루프 안에서 벡터에 삭제 마킹만" 습관.

4. **결정론 시뮬레이션에서 `unordered_map` 순회 순서에 의존** — 순회 순서가 구현·삽입 이력에 따라 달라져 클라/서버가 갈라진다. 순서가 의미를 가지면 `std::map`(SystemScheduler.h:39가 Phase 순서 때문에 map을 쓰는 이유)이나, 직렬화 전에 안정 키로 정렬(SnapshotBuilder).

5. **"타입 소거 다운캐스트니까 `dynamic_cast`를 써야 안전"** — 아니다. 맵 키가 이미 타입을 보장하면 `static_cast`가 안전하고 빠르다. `dynamic_cast`는 RTTI 런타임 검사 비용만 더한다(World.h:92). 반대로 키 같은 외부 보장이 없는 다운캐스트에 `static_cast`를 쓰는 것도 오답 — "무엇이 타입을 보장하는가"를 말할 수 있어야 한다.

---

## 다른 챕터와의 연결

- [컴파일 · 링크 · DLL 경계](02_compile_link_dll.md) — `WINTERS_ENGINE` export/import, 템플릿 멤버는 왜 DLL로 export 못 하나(World.h:58), C4251, unique_ptr 멤버 + dllexport의 copy=delete(SystemScheduler.h:17).
- [동시성](09_concurrency.md) — Chase-Lev 워크스틸링, `Get_WorkerSlot` thread_local(JobSystem.cpp:18), fork-join(JobCounter), 이 챕터의 병렬 스케줄링이 얹히는 실행 엔진.
- [네트워크 · 직렬화](12_network_serialization.md) — 스냅샷/델타의 와이어 포맷(FlatBuffers), verify 실패 처리, 이 챕터 §7 결정론 직렬화의 상세.
- [면접 Q&A 뱅크](13_interview_qa_bank.md) — 본 챕터 주제의 압축 문답 모음.
