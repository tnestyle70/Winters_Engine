# 면접 질문 은행 — Winters로 답하기

> 이 챕터는 개념을 다시 설명하지 않는다. 개념 설명은 앞 챕터들에 있고, 여기는 **면접장에서 실제로 나오는 질문 → 한 줄 정답 → 파고들기(꼬리질문) → Winters 코드로 증명**만 모은 질문 은행이다.
> 사용법: (A)로 기초를 훑고, 각 항목의 `▸` 를 스스로 답할 수 있는지 점검한다. 막히면 그게 공부할 지점이다. (B)는 꼬리질문 방어용, (C)는 "경험을 말해보라"에 STAR로 답하는 골격이다.
> 인용은 전부 실제 파일에서 확인한 `path:line` 이다. 면접장에서 "이거 제 엔진 코드입니다" 라고 말할 수 있어야 한다.

---

## A. 기초 개념 질문 (한 줄 정답 + 파고들기)

### A-1. 포인터 · 참조 · 수명

1. **포인터와 참조의 차이는?**
   한 줄: 참조는 null 불가 · 재바인딩 불가 · 항상 유효 대상을 전제하는 별칭이고, 포인터는 null·재대입·소유 표현이 가능한 주소값이다.
   ▸ "반환 타입으로 참조와 포인터를 언제 나눠 쓰나?" → **널 가능성 계약**. Winters `CWorld`는 존재를 전제하면 참조 반환(`GetComponent`, `Engine/Public/ECS/World.h:85`), 없을 수 있으면 포인터 반환(`TryGetComponent`, `World.h:94`)으로 갈랐다.

2. **댕글링 포인터(dangling pointer)란?**
   한 줄: 가리키던 객체의 수명이 끝났는데 포인터는 그 주소를 계속 들고 있는 상태.
   ▸ "raw 포인터 멤버는 언제 정당한가?" → 소유가 명확히 다른 곳에 있는 **비소유 관찰자(non-owning view)** 일 때. 소유는 `unique_ptr`, 관찰은 raw로 갈라라.

3. **소유(ownership) vs 관찰(observing)을 코드로 어떻게 구분하나?**
   한 줄: 소유는 스마트 포인터, 관찰은 raw 포인터/참조로 표기해 "누가 delete 하는가"를 타입으로 문서화한다.

4. **`const T*` / `T* const` / `const T* const` 차이는?**
   한 줄: 앞의 const는 가리키는 대상이 불변, 뒤(별표 뒤)의 const는 포인터 자체가 재대입 불가.
   ▸ 읽는 법: 오른쪽에서 왼쪽으로 읽는다(right-to-left rule).

5. **`nullptr` 와 `NULL`, `0` 의 차이는?**
   한 줄: `nullptr`는 `std::nullptr_t` 타입이라 오버로드 해석에서 정수 `0`과 안 헷갈린다. `NULL`/`0`은 정수라 `f(int)` vs `f(char*)` 모호성을 만든다.

### A-2. 복사 · 이동 (copy / move)

6. **복사(copy)와 이동(move)의 차이는?**
   한 줄: 복사는 원본을 그대로 두고 자원을 복제, 이동은 원본의 자원 소유권을 훔쳐오고 원본을 "비었지만 유효한" 상태로 남긴다.
   ▸ "move 후 원본 상태는?" → **valid but unspecified**. destruct/재대입만 안전, 값을 읽으면 안 된다. → (B) 참조.

7. **Rule of Three / Five / Zero는?**
   한 줄: 소멸자(destructor)·복사생성·복사대입 중 하나라도 손으로 쓰면 나머지도 필요(Three), 이동 2개까지 하면 Five, 자원을 직접 안 들고 스마트 타입에 위임하면 아무것도 안 써도 됨(Zero).
   ▸ Winters `CWorld`는 `unique_ptr` 멤버 때문에 복사=delete/이동=default를 명시했다 (`Engine/Public/ECS/World.h:52`).

8. **왜 이동 생성자에 `noexcept`를 붙이나?**
   한 줄: `std::vector`가 재할당할 때 원소 이동이 `noexcept`가 아니면 강한 예외 보장을 위해 이동 대신 **복사**로 폴백하기 때문.
   ▸ 즉 `noexcept` 하나가 컨테이너 성장 성능을 좌우한다.

9. **RVO / NRVO(복사 생략, copy elision)란?**
   한 줄: 반환 임시객체를 호출자 저장소에 직접 생성해 복사/이동 자체를 없애는 것. C++17부터 prvalue 반환은 **의무적** 생략.
   ▸ "그럼 `return std::move(local);` 은?" → NRVO를 오히려 **막는다**. 이름 붙은 지역변수는 그냥 `return local;`.

### A-3. 스마트 포인터 (smart pointers)

10. **`unique_ptr` 와 `shared_ptr` 를 언제 쓰나?**
    한 줄: 단독 소유면 `unique_ptr`(제로 오버헤드), 진짜로 수명을 공유해야 하면 `shared_ptr`.
    ▸ Winters `CResourceCache`는 텍스처=`unique_ptr`(단독소유+raw 관찰 반환, `Engine/Public/Resource/ResourceCache.h:38`), 모델=`shared_ptr`(공동소유, `:40`)로 리소스 종류별 수명 계약에 따라 갈랐다.

11. **`make_unique` / `make_shared` 를 왜 선호하나?**
    한 줄: `new`와 인자 평가 사이 예외로 인한 누수를 막고, `make_shared`는 객체+제어블록을 한 번에 할당해 캐시/할당 이점.
    ▸ "그럼 `make_unique`를 못 쓰는 경우는?" → **private 생성자 + 팩토리**. 접근 제어는 라이브러리 함수(`make_unique` 내부)에 적용되지 않는다. Winters 팩토리는 `unique_ptr<CTimer>(new CTimer())`를 직접 쓴다 (`Engine/Private/Core/CTimer.cpp:42`).

12. **`shared_ptr` 의 refcount는 스레드 안전한가?**
    한 줄: **제어블록의 refcount 증감만** atomic이라 스레드 안전. 가리키는 객체 자체나 같은 `shared_ptr` 인스턴스의 동시 대입은 안전하지 않다.

13. **`weak_ptr` 는 왜 필요한가?**
    한 줄: 순환 참조(cycle)로 refcount가 0이 안 돼 생기는 누수를 끊고, "살아있으면 접근"을 `lock()`으로 표현하기 위해.

### A-4. 클래스 · 다형성 (polymorphism)

14. **가상 함수(virtual function)는 어떻게 동작하나?**
    한 줄: 객체 앞에 숨은 vptr이 클래스별 vtable을 가리키고, 호출 시 vtable에서 함수 주소를 찾아 간접 점프한다.
    ▸ vtable 레이아웃/다중상속은 → (B) 참조.

15. **왜 다형적 base 클래스는 소멸자를 virtual로 둬야 하나?**
    한 줄: `delete pBase;` 가 실제 파생 소멸자를 못 부르면 **미정의 동작(UB)** — 파생 리소스 누수. Winters `IScene` 인터페이스가 `virtual ~IScene() = default` (`Engine/Include/IScene.h:9`).

16. **순수 가상(pure virtual)과 본문 있는 virtual을 언제 나누나?**
    한 줄: 반드시 구현해야 하는 계약은 `= 0`, 안 오버라이드해도 되는 선택적 훅은 기본 본문 있는 virtual. `IScene`은 `OnEnter/OnUpdate`는 순수 가상, `OnLateUpdate/OnImGui`는 빈 본문 훅으로 섞는다 (`Engine/Include/IScene.h:11`~`:16`).

17. **런타임 다형성을 virtual 없이 구현하는 법은?**
    한 줄: 함수 포인터 점프테이블 / `std::function` 콜백 주입 / enum 태그 디스패치. Winters는 챔피언 스킬 훅을 vtable 대신 `using HookFn = void(*)(GameplayHookContext&)` + 256×256 **2D 디스패치 테이블**로 한다 (`Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h:24`, `:36`).
    ▸ 트레이드오프: 데이터지향 컴포넌트에 vptr을 안 심어 캐시/직렬화 친화 vs 간접호출 분기 비용.

18. **싱글턴을 스레드 안전하게 만드는 법은?**
    한 줄: C++11 함수 지역 `static`(magic statics) — 초기화가 스레드 안전으로 표준 보장돼 DCLP 없이 끝. Winters `DECLARE_SINGLETON` 매크로가 이 패턴 + `NO_COPY`(복사 delete) (`Engine/Public/Engine_Macro.h:57`, `:52`).
    ▸ 함정: "초기화 원자성 ≠ 본문 원자성" — 초기화만 보장, 이후 메서드 동시호출은 별개.

### A-5. 템플릿 · 컴파일/링크

19. **`typedef` 와 `using` 의 차이는?**
    한 줄: `using`은 템플릿 별칭(alias template)이 되고 좌→우로 읽혀 함수 포인터 별칭이 명료. non-template에선 기능상 동일한 same-entity.

20. **`const` 와 `constexpr` 의 차이는?**
    한 줄: `const`는 "런타임에 안 바뀜", `constexpr`는 "컴파일타임에 값이 확정됨". `constexpr`는 배열 크기·`static_assert`에 쓸 수 있다.

21. **헤더에 상수를 두는 올바른 방법은?**
    한 줄: C++17 `inline constexpr` 변수 — 여러 TU에 포함돼도 **ODR 위반이 안 나는** 단일 정의. Winters `WintersMath.h:77`~`:79`의 `inline constexpr float kEpsilon/kPi/kTwoPi`가 최종형.
    ▸ 변천: 매크로 → `const`(내부링키지 중복) → `constexpr` → `inline constexpr`.

22. **선언(declaration)과 정의(definition)의 차이는?**
    한 줄: 선언은 "이름·타입이 존재한다", 정의는 "실체(저장소/본문)를 만든다". ODR은 정의가 프로그램 전체에 1개일 것을 요구.
    ▸ 링킹 관점 응용: 오염 헤더를 include하지 않고 심볼만 재선언하면 링크된다 — Winters `SimDebugOutput.h`가 `Windows.h` 없이 `OutputDebugStringA`를 `extern "C" __declspec(dllimport)`로 직접 재선언한다 (`Shared/GameSim/Core/Debug/SimDebugOutput.h:22`).

### A-6. STL · 컨테이너

23. **`vector` / `list` / `map` / `unordered_map` 을 언제 쓰나?**
    한 줄: 기본은 `vector`(연속·캐시친화). 정렬·결정적 순서가 필요하면 `map`(트리), 조회만이면 `unordered_map`(해시), 중간 삽입/포인터 안정성이 핵심이면 노드 기반.
    ▸ Winters는 **결정적 phase 실행 순서**가 필요한 스케줄러에 일부러 `std::map`을 골랐다 (`Engine/Public/ECS/SystemScheduler.h:39`).

24. **iterator 무효화(invalidation) 규칙은?**
    한 줄: `vector`는 재할당/중간 삭제 시 이후 전부 무효, 노드 기반(`list`·`map`·`unordered_map`)은 **삭제된 원소만** 무효(unordered 재해시는 iterator만 무효, 참조/포인터는 유지).
    ▸ Winters FX 메쉬 캐시는 `unordered_map`에 `emplace(std::move(...))` 후 원소 주소를 밖으로 돌려준다 — 노드 기반 참조 안정성 활용 (`Engine/Private/Renderer/RHIFxMeshResource.cpp:289`, `:298`).

25. **컨테이너 순회 중 원소를 지우면 왜 위험한가?**
    한 줄: 삭제가 순회 중인 iterator를 무효화해 UB. 해법은 erase-remove, 또는 **2단계 collect-then-mutate**.
    ▸ Winters ECS는 `ForEach` 순회 중 구조 변경을 금지하고 지연 커맨드 버퍼(`CCommandBuffer`)로 기록했다가 `Flush`에서 실행한다 (`Engine/Public/ECS/CCommandBuffer.h:7`~`:10`).

26. **`map operator[]` 와 `at()` / `find()` 의 차이는?**
    한 줄: `operator[]`는 없으면 **기본값을 삽입**(그래서 non-const), `at()`은 없으면 throw, `find()`는 없으면 `end()`.
    ▸ Winters `EventApplier`는 `m_lastActionSeq[netId]`의 "처음 보는 키는 0으로 삽입" 성질을 시퀀스 비교 로직으로 활용 (`Client/Private/Network/Client/EventApplier.cpp:570`).

### A-7. 동시성 기초 (concurrency)

27. **데이터 레이스(data race)란?**
    한 줄: 최소 하나가 쓰기인 두 접근이 동기화 없이 같은 메모리에 동시 발생 — 표준상 UB.
    ▸ 해결 축 3개: (1) 공유를 없앤다(값 복사/`thread_local`) (2) 동기화한다(mutex/atomic) (3) 시간을 나눈다(phase 분리).

28. **mutex vs atomic을 언제 쓰나?**
    한 줄: 단일 워드 카운터/플래그는 `atomic`(락 없음), 여러 변수의 불변식을 함께 지켜야 하면 `mutex`.

29. **`thread_local` 의 실전 용도는?**
    한 줄: 스레드마다 독립 저장소 — 핫패스에서 락 없이 수집하고 병합 지점만 동기화. Winters 프로파일러가 `thread_local` 스코프 스택으로 수집한다 (`Engine/Private/Core/Profiler/CPUProfiler.cpp:29`).

30. **`std::async` 와 `std::thread` 의 차이는?**
    한 줄: `thread`는 즉시 새 스레드, `async`는 future로 결과/예외를 받고 정책(`launch::async`/`deferred`)을 고른다.
    ▸ 유명 함정: `async`의 future를 안 받으면 임시 future 소멸자가 **동기 대기**해 병렬이 아니게 됨 → (C-2) 참조.

31. **`std::atomic` 을 `std::vector`에 바로 못 담는 이유는?**
    한 줄: `atomic`은 복사·이동 불가라 원소를 재배치하는 `vector`의 요구를 못 채운다. 해법은 `unique_ptr`로 박싱.
    ▸ Winters JobSystem이 `atomic`+`alignas(64)` 멤버를 가진 deque를 `make_unique`로 감싸 담았다 (`Engine/Private/Core/JobSystem.cpp:55`~`:62`).

32. **enum class(scoped enum)와 일반 enum의 차이는?**
    한 줄: `enum class`는 암시적 정수 변환 없음(타입 안전) + 스코프 있음. 비트플래그로 쓰려면 연산자 오버로드나 캐스트가 필요.
    ▸ Winters wire 프로토콜: 타입 태그는 `enum class ePacketType : uint16_t`(크기 고정+타입 안전), 비트 조합 플래그는 일반 `enum ePacketFlags`로 의도적으로 나눴다 (`Shared/Network/PacketEnvelope.h:13`, `:27`).

---

## B. 심화 / 꼬리질문 (한 방에 무너지지 않기)

1. **vtable은 메모리에 정확히 어떻게 놓이나?**
   객체 첫 워드에 vptr, vptr이 함수포인터 배열(vtable)을 가리킴. vtable은 클래스당 1개(공유), 인스턴스당 vptr만 1개. 다중상속이면 base마다 서브오브젝트+vptr이 생기고 `this` 포인터 조정(thunk)이 낀다. 생성자 실행 중엔 vptr이 "현재 생성 단계 클래스"의 vtable을 가리켜 가상 호출이 파생으로 안 내려간다.

2. **`move` 한 뒤 원본을 읽어도 되나?**
   표준 라이브러리 타입은 "valid but unspecified" — 소멸/재대입은 안전하지만 **값을 가정하면 안 된다**. `std::string`은 보통 빈 문자열이 되지만 보장은 아님. 직접 만든 이동 생성자는 원본 포인터를 반드시 null로 만들어 double-free를 막아야 한다.

3. **memory_order를 각각 언제 쓰나?**
   `relaxed`=원자성만(순서 보장 X), `acquire`=이 로드 뒤 읽기가 위로 못 넘어옴, `release`=이 스토어 앞 쓰기가 아래로 못 내려감, `acq_rel`=둘 다, `seq_cst`=전역 단일 순서.
   Winters `CJobCounter`: 증가는 `relaxed`(순서 무관 카운트, `Engine/Public/Core/JobCounter.h:26`), 감소는 `acq_rel`(잡 완료 발행, `:31`), 로드는 `acquire`(`:36`) — 연산별로 필요한 보장만 산다.

4. **acquire/release로 부족하고 `seq_cst`가 꼭 필요한 경우는?**
   store 후 **다른 위치**를 load하는 순서(Dekker/store-load)는 acquire/release로 안 막힌다. Chase-Lev deque의 `Pop`이 정확히 이 사례 — `bottom` 스토어와 `top` 로드 사이에 `atomic_thread_fence(seq_cst)` (`Engine/Public/Core/JobSystem/WorkStealingDeque.h:47`).

5. **`compare_exchange_weak` 와 `strong` 의 차이는?**
   weak는 값이 맞아도 **가짜 실패(spurious failure)** 가능 — 재시도 루프 전제면 더 저렴. strong은 가짜 실패 없음 — 단발 CAS에 쓴다. Winters는 `Pop`의 마지막 1개 경합엔 strong(`WorkStealingDeque.h:58`), 실패하면 그냥 포기하고 다른 victim을 찾는 `Steal`엔 weak(`:75`)을 골라 썼다.

6. **false sharing이란?**
   서로 다른 스레드가 만지는 두 변수가 **같은 캐시라인(64B)** 에 있어 한쪽 쓰기가 상대 코어의 캐시라인을 무효화하는 것. 해법은 `alignas(64)` 분리. Winters deque의 `m_iBottom`(owner 전용)/`m_iTop`(thief 전용)이 각각 `alignas(64)` (`WorkStealingDeque.h:91`).

7. **lock-free 큐에 아무 타입이나 담아도 되나?**
   안 된다. Chase-Lev `Steal`은 CAS로 소유권을 확정하기 **전에** 값을 읽는다(speculative read, `WorkStealingDeque.h:74`). T가 non-trivial(예: `std::function`)이면 검증 실패로 버려질 값이라도 그 복사가 owner의 쓰기와 겹치는 순간 데이터 레이스=UB. trivially copyable 타입이거나 별도 시퀀스 검증이 필요하다.

8. **`shared_ptr` 의 비용은?**
   포인터 2개 크기(객체+제어블록), refcount 증감은 atomic RMW(경합 시 캐시라인 핑퐁), 소멸 분기 비용. `make_shared`는 한 번 할당으로 완화하지만 `weak_ptr`가 살아있으면 객체 메모리가 제어블록과 함께 늦게 반환된다. 그래서 "일단 shared"가 아니라 기본 `unique_ptr`.

9. **ODR(One Definition Rule) 위반이란? 실제로 무슨 사고가 나나?**
   같은 심볼이 두 TU에 서로 다른 정의로 존재하면 링커가 아무거나 하나 골라 **조용히 잘못된 코드**가 링크됨(진단 의무 없음, ill-formed NDR). 헤더의 non-inline 전역 정의가 흔한 원인. C++17 `inline` 변수/함수가 정공법 (`Engine/Include/WintersMath.h:77`).

10. **`static_cast` 로 base→derived 다운캐스트가 언제 안전한가?**
    실제 객체가 그 파생 타입임을 **다른 불변식으로 보장**할 때만. Winters ECS는 스토어 맵의 키가 `std::type_index(typeid(T))` 라서 값이 항상 `CComponentStoreWrapper<T>*` 임이 보장돼 `dynamic_cast` 없이 `static_cast` 한다 (`Engine/Public/ECS/World.h:182`~`:185`).

11. **`dynamic_cast` 의 비용과 조건은?**
    polymorphic(가상함수 보유) base여야 하고 RTTI 필요. 런타임에 타입 정보를 조회하므로 `static_cast`보다 비쌈. 실패 시 포인터는 null, 참조는 `bad_cast` throw. Winters는 인터페이스 뒤 실제 백엔드 타입이 불확실한 RHI 경계에서 사용 (`Engine/Private/RHI/DX11/CDX11Device.cpp:547`).

12. **같은 문자열 리터럴 두 개의 주소는 같은가?**
    같을 수도 있지만 **표준 미보장**(string pooling은 구현 재량). DLL/TU 경계에선 실제로 달라진다. Winters 프로파일러는 이 때문에 같은 이름 카운터가 중복 행으로 갈라지는 버그를 겪고, 포인터 fast-path + `strcmp` fallback인 `SameProfilerName`으로 고쳤다 (`Engine/Private/Core/Profiler/CPUProfiler.cpp:20`, 병합 지점 `:118`).

13. **`#define new` 의 위험은?**
    CRT 누수추적용 `#define new DBG_NEW`(`Engine/Public/Engine_Defines.h:38`~`:41`)는 텍스트 치환이라 **placement new**(`new (ptr) T`) 문법을 깨뜨린다. 서드파티 헤더 앞뒤로 `#pragma push_macro("new")`/`#undef new`/`pop_macro`로 격리 — Winters가 Tracy 헤더에 적용 (`Engine/Include/ProfilerAPI.h:16`~`:19`).

14. **`unique_ptr<불완전타입>` 멤버가 있으면 소멸자를 왜 `.cpp`에 둬야 하나?**
    `unique_ptr` 소멸은 default_delete의 `delete`가 **완전 타입**을 요구한다. 헤더에서 암시적 소멸자가 인라인 생성되는 시점엔 타입이 불완전할 수 있으므로, 소멸자를 `.cpp`에 out-of-line 정의해 완전 타입이 보이는 곳에서 인스턴스화한다. Winters `CGameInstance`가 전방선언 매니저들을 `unique_ptr<class CTimer_Manager>` 식으로 든다 (`Engine/Include/GameInstance.h:174`).

15. **dllexport 클래스에 STL 멤버를 두면 왜 경고(C4251)가 뜨나?**
    STL 멤버의 레이아웃/allocator/CRT가 DLL과 소비자에서 다르면 ABI 불일치·힙 교차 해제 위험. **동일 툴체인·동일 CRT** 전제하에서만 안전해서 Winters는 `#pragma warning(disable:4251)`로 억제하되 위험 인지 주석을 남긴다 (`Engine/Public/ECS/World.h:14`, `Engine/Public/Engine_Defines.h:30`). 정공법은 pimpl/순수가상 인터페이스.

16. **DLL에서 템플릿 멤버는 왜 export 못 하나?**
    템플릿은 **인스턴스화 시점**에 코드가 생기므로 DLL 빌드 때는 어떤 인스턴스가 필요한지 몰라 심볼이 없다. Winters `CWorld`는 non-template 멤버만 `WINTERS_ENGINE`(dllexport)로 내보내고, 템플릿 멤버는 헤더 본문으로 caller 측 인스턴스화 (`Engine/Public/ECS/World.h:58`).

17. **`reinterpret_cast` 로 구조체를 다른 구조체로 읽어도 되나?**
    strict aliasing 위반이면 UB지만, standard-layout + 동일 멤버 배치(layout-compatible)면 실무적으로 통용. Winters `Vec3 → XMFLOAT3`가 그 가정 위에 서 있다 (`Engine/Include/WintersMath.h:44`). 진짜 이식성 있는 방법은 `memcpy` — wire 헤더는 `#pragma pack(1)`+`static_assert(sizeof==16)`+memcpy 직렬화 (`Shared/Network/PacketEnvelope.h:34`, `:46`).

18. **`XMVECTOR`를 클래스 멤버로 두면 왜 위험한가?**
    `XMVECTOR`는 16B 정렬을 요구하는 SIMD 레지스터 타입. 힙/패킹 구조체에서 정렬이 깨지면 크래시. 저장은 `XMFLOAT*`(=`Vec3`), 연산만 `XMLoad → XM연산 → XMStore` 경계에서 — Winters가 헤더 규약으로 성문화 (`Engine/Include/WintersMath.h:13`~`:16`).

19. **magic statics(C++11 함수 지역 static)의 보장 범위는?**
    "초기화가 정확히 한 번, 스레드 안전하게" 까지만. 초기화 완료 후 그 객체의 메서드를 여러 스레드가 부르는 것은 별개 문제다. Winters의 여러 Meyers 싱글턴(`DECLARE_SINGLETON`)이 이 위에 서 있다.

20. **`condition_variable`에서 notify가 유실(lost wakeup)되면?**
    락 밖에서 조건을 바꾸고 notify하면 wait 진입 직전과 겹쳐 깨우기를 놓친다. 정석은 락 안 조건 변경 + predicate wait. Winters JobSystem은 핫패스 비용 때문에 `wait_for(1ms)` 타임아웃으로 **유실을 허용하되 회복 상한**을 뒀다 (`Engine/Private/Core/JobSystem.cpp:186`, 근거 주석 `:182`~`:184`).

21. **정수를 `enum`으로 캐스트할 때 값이 정의역 밖이면?**
    고정 underlying type이면 표현 범위 내 캐스트는 합법이나, 열거 상수 밖 값은 이후 switch/배열 인덱싱에서 버그·UB. 네트워크 경계에선 범위 검사로 막아야 함. Winters `SnapshotApplier`가 wire 정수→enum 변환에 `>= End` 검사 후 기본값으로 clamp (`Client/Private/Network/Client/SnapshotApplier.cpp:121`~`:126`).

22. **32비트 시퀀스 번호가 wraparound해도 순서 비교를 어떻게 맞추나?**
    `unsigned` 뺄셈 결과를 `signed`로 캐스트해 부호로 판정: `static_cast<i32_t>(lhs - rhs) > 0`. 오버플로 경계를 자연스럽게 넘는 TCP seq 관용구. Winters 커맨드 seq 비교가 그대로 이 패턴 (`SnapshotApplier.cpp:118`).

23. **신뢰할 수 없는 네트워크 바이트를 역참조하기 전 무엇을 검증하나?**
    스키마 경계 검증(FlatBuffers `Verifier`)을 먼저 통과시켜야 한다. 실패를 **조용히 삼키면** 스키마 drift가 네트워크 스톨과 구분이 안 됨 → 반드시 상한 있는 로그/카운터로 가시화 (`SnapshotApplier.cpp:485`~`:495`).

24. **`if constexpr` 와 일반 `if` 의 차이는?**
    `if constexpr`는 컴파일타임 상수 조건에서 **버려지는 가지의 코드를 아예 생성하지 않는다**(discarded statement — 템플릿이면 인스턴스화도 안 됨). Winters는 디버그 야우 트레이스 블록을 릴리스에서 완전 제거하는 데 쓴다 (`SnapshotApplier.cpp:702`).

25. **`std::function` 콜백 vs 템플릿(`Fn&&`) 콜백의 트레이드오프는?**
    `std::function`은 타입소거로 ABI/컴파일 경계가 깔끔하지만 간접호출+잠재 힙할당. 템플릿 전달참조(forwarding reference)는 단형화+인라인으로 제로 오버헤드지만 헤더 노출·코드 팽창. Winters `World::ForEach`는 성능 경로라 `template<typename T, typename Fn> ForEach(Fn&&)` (`Engine/Public/ECS/World.h:135`) — 반대로 Ashe Tick이 람다를 굳이 `std::function`으로 감싸 단형화를 무력화한 **안티패턴도 코드베이스에 실존**한다 (`Shared/GameSim/Champions/Ashe/AsheGameSim.cpp:298`). 지적당하기 전에 먼저 말하면 가산점.

26. **파괴된 객체를 가리키는 stale 핸들(ABA)을 어떻게 막나?**
    인덱스만 재사용하면 "죽고 새로 태어난 다른 객체"를 구분 못 한다. 핸들에 **세대(generation) 카운터**를 같이 패킹하고, 해석 시 슬롯의 현재 세대와 대조. Winters `EntityHandle`이 하위 32비트=index, 상위 32비트=generation인 u64 (`Engine/Public/ECS/Entity.h:17`~`:34`).

27. **게임 시뮬레이션 결정론(determinism)을 어떻게 보장하나?**
    (1) 난수: `std::` distribution은 구현 정의라 플랫폼 간 결과가 다름 → 손수 짠 정수 xorshift64 (`Shared/GameSim/Core/Determinism/DeterministicRng.h:15`), 순서 의존을 끊는 키 기반 서브시드 파생(`:42`). (2) 입력 순서: 수신 순서가 아니라 `(tick, session, seq)` 키로 `stable_sort` (`Server/Private/Game/CommandIngress.cpp:98`). (3) 부동소수: 컴파일 옵션(/fp) 통일. 셋 중 하나만 빠져도 클라/서버 시뮬이 드리프트한다.

28. **`const_cast` 는 언제 정당한가?**
    캐스트 4종 중 유일하게 cv 한정자를 벗기는 캐스트. 합법의 경계는 **원본 객체가 실제로 const로 정의됐는가** — const 정의 객체를 const_cast로 벗겨 수정하면 UB(컴파일러가 상수 폴딩/읽기전용 배치 가정). 정당한 용도: (1) const-미비 레거시 C API에 넘기기(수정 안 함 전제) (2) const/비const 오버로드 중복 제거(비const가 const 버전을 호출 후 벗김) (3) 비const 저장소를 읽기 전용 뷰로만 노출한 경우의 의도적 해제. Winters 스킬 튜너가 (3)의 실례 — 실제 저장소는 비const `static SkillDef s_SkillTable[]`(`Client/Private/GameObject/SkillTable.cpp:29`)이고 외부 노출만 `const SkillDef* const g_SkillTable`(`:350`)이라, 디버그 패널의 `const_cast<SkillDef&>` 쓰기가 합법이다 (`Client/Private/UI/SkillTimingPanel.cpp:22`).
    ▸ "논리적 상수성(logical constness)이 목적이면?" → const_cast가 아니라 **`mutable`** — const 조회 경로에서도 잠가야 하는 캐시 뮤텍스가 그 사례 (`Engine/Public/Resource/ResourceCache.h:37`의 `mutable std::mutex`).

29. **`std::string` 내부 구조와 SSO는? `c_str()` / `string_view` 수명 함정은?**
    `std::string`은 대략 {포인터, size, capacity}에 **SSO(small string optimization)** — 짧은 문자열(MSVC 15바이트 등, 구현마다 다름)은 힙 할당 없이 객체 내부 버퍼에 저장. 따름정리: (1) SSO 범위 문자열은 move가 사실상 복사(훔칠 힙 포인터가 없음) (2) SSO 문자열의 `data()`/iterator는 move·swap에도 무효화된다. `c_str()`은 원본 string 수명 + 다음 비const 연산 전까지만 유효 — **임시 string의 `c_str()` 저장 = 즉시 댕글링**. `std::string_view`는 {포인터, 길이} 비소유 뷰라 같은 함정이 더 넓다: `string_view sv = s + "suffix";`처럼 임시에 바인딩하면 문장 끝에서 댕글링, 멤버로 저장하면 원본 수명 추적 불가.
    ▸ 원칙: string_view는 **인자 타입/즉시 소비** 전용 — Winters FX 해시가 그 용법으로 어떤 문자열 소스든 할당 없이 받는다 (`Engine/Public/FX/ParameterMap.h:35`의 `FxHashName(std::string_view)`). 보관할 거면 `std::string`으로 복사.

30. **`constexpr` 함수의 규칙은? C++20 `consteval` / `constinit` 과 뭐가 다른가?**
    `constexpr` 함수는 "컴파일타임에 평가**될 수 있는**" 이중 모드 함수 — 상수 표현식 문맥(배열 크기, `static_assert`, 템플릿 인자)에선 컴파일타임, 아니면 평범한 런타임 호출. C++14부터 루프·지역변수 허용, C++20은 `new`·가상 함수까지 허용이 넓어졌다. `consteval`(immediate function)은 **컴파일타임 평가를 강제** — 런타임 문맥 호출이 컴파일 에러. `constinit`은 정적 변수의 **초기화가 컴파일타임에 끝남**을 보장해 SIOF(static initialization order fiasco)를 차단하되 const는 아니다(런타임 수정 가능).
    ▸ Winters는 전 프로젝트 `LanguageStandard`가 `stdcpp20`(Engine/Client/Server/GameSim 각 `.vcxproj`)이라 셋 다 사용 가능. 현재는 `inline constexpr` 상수(`Engine/Include/WintersMath.h:77`)와 `if constexpr`(B-24)까지 사용 — "왜 consteval은 안 썼나"에는 "컴파일타임 강제가 필요한 지점이 아직 없고, 컴파일타임 해시/테이블 생성 도입 시 후보"로 답한다.

31. **람다는 컴파일러 내부에서 무엇으로 구현되나? 캡처별 함정은?**
    람다식마다 컴파일러가 고유한 **클로저 클래스**를 생성한다 — 캡처가 멤버 변수, 본문이 `operator()`(기본 const, `mutable` 키워드로 비const). 캡처 방식: 값 `[x]`(멤버로 복사), 참조 `[&x]`(수명 보증은 프로그래머 몫), init-capture `[p = std::move(ptr)]`(C++14 — move-only 타입 캡처·표현식 재바인딩). 캡처 없는 람다만 함수 포인터 변환 가능(상태 멤버가 없으므로). 댕글링 위험 서열: 지역변수 `[&]` 캡처를 비동기/저장 콜백에 넘기기 > `[this]`(객체 수명 의존) > 값 캡처(안전하지만 복사 비용).
    ▸ Winters CHttpClient 워커 람다가 실전 균형 — 스냅샷·문자열은 **값 캡처**로 워커 수명에 묶고, `this`는 소멸자 join으로 수명을 보증한다 (`Client/Private/Network/Backend/CHttpClient.cpp:148`~`:153`, 수명 논증은 C-2).

32. **정수 승격(integral promotion)과 signed/unsigned 비교 함정은?**
    산술 연산 전에 `int`보다 작은 타입(`char`/`short`/`uint8_t`...)은 전부 `int`로 **승격**된다 — `uint8_t a=200, b=100;`에서 `a+b`는 int 300(오버플로 아님)이지만 uint8_t에 되담으면 44. 피연산자 타입이 다르면 usual arithmetic conversions — **같은 랭크면 signed가 unsigned로 변환**된다. 함정의 왕: `-1 < 1u`는 **false**(-1이 unsigned 최대값으로 변환). 그래서 `i < vec.size() - 1`은 빈 벡터에서 `size()-1`이 거대 unsigned가 돼 루프가 폭주한다. 방어: 비교 전 같은 signed 타입으로 정리, C++20 `std::cmp_less` 계열, signed/unsigned 경고(C4018) 무시 금지.
    ▸ 이 변환 규칙을 **역이용**하는 것이 B-22의 시퀀스 wraparound 비교 — unsigned 뺄셈의 모듈러 성질 + signed 캐스트 부호 판정은 함정이 아니라 계약된 관용구다 (`SnapshotApplier.cpp:118`).

---

## C. 경험 서술형 (STAR 골격 — 전부 Winters 실제 사건)

> 형식: **S**(상황) **T**(과제) **A**(행동) **R**(결과) + 이어질 꼬리질문. 면접에서 이 뼈대에 판단 근거를 붙여 말한다.

### C-1. "메모리/포인터 버그를 잡아본 경험"

**주제: 문자열 리터럴 주소 동일성이 DLL 경계에서 깨진 프로파일러 버그**
`Engine/Private/Core/Profiler/CPUProfiler.cpp:118`

- **S**: 프로파일러 카운터가 같은 이름인데 화면에 **중복 행**으로 갈라져 나왔다.
- **T**: 원인이 로직인지 데이터인지부터 가려야 했다.
- **A**: 카운터 병합 키가 `const char* pName` 포인터 비교였다. 같은 리터럴이라도 Engine DLL과 Client TU에서 **다른 주소**로 들어와 다른 키로 취급됐음을 확인. 포인터 fast-path는 남기되 다르면 `strcmp` fallback으로 비교하는 `SameProfilerName` 도입 (`:20`).
- **R**: 중복 행 소멸. "리터럴 주소 동일성은 표준 미보장"을 팀 gotcha로 박제.
- ▸ 꼬리질문: "왜 `std::string` 키로 안 했나?" → 핫패스 할당/비교 비용. `const char*` 저장은 "이름은 string literal이어야 한다"는 정적 수명 계약 위에서만 안전 (`Engine/Include/ProfilerAPI.h:46` 주석).

### C-2. "멀티스레딩 버그를 잡아본 경험 (1)"

**주제: `std::async`의 future를 버려서 사실상 동기 실행이 된 HTTP 클라이언트**
`Client/Private/Network/Backend/CHttpClient.cpp:90`

- **S**: 백엔드 요청을 "비동기"로 짰는데 요청마다 프레임이 멈췄다.
- **T**: 왜 병렬이 아닌지 규명.
- **A**: `async(launch::async, ...)`의 반환 future를 안 받으면 **임시 future의 소멸자가 작업 완료까지 블록**한다(표준 규정). 호출 지점마다 한 줄씩 동기 대기 중이었던 것. `m_PendingRequests`가 future를 소유하게 바꾸고(`:148`~`:156`), 완료분은 `wait_for(0s)==ready` 논블로킹 폴링으로 정리(`:129`), 소멸자에서 전부 드레인(`:103`~`:117`).
- **R**: 진짜 비동기가 됐고 블로킹은 파괴 시점 한 곳으로 수렴. gotcha "async lifetime"으로 박제.
- ▸ 꼬리질문: "람다의 raw `this` 캡처는 use-after-free 아닌가?" → 소멸자가 모든 future를 `wait()` 하므로 람다보다 객체가 반드시 오래 산다. 그 join이 `weak_ptr` 없이 안전한 **유일한** 근거임을 스스로 짚어야 한다.

### C-3. "멀티스레딩 버그를 잡아본 경험 (2)"

**주제: Chase-Lev deque의 owner-only-push 규약을 위반한 잡 시스템 race**
`Engine/Private/Core/JobSystem.cpp:126`

- **S**: 병렬 잡 제출 후 간헐적으로 잡이 유실되는 비결정적 사고.
- **T**: lock-free 자료구조 자체 버그인지 사용법 문제인지 구분.
- **A**: Chase-Lev deque는 **owner 스레드만 bottom에 push** 할 수 있는데, 메인/외부 스레드가 워커 deque에 직접 push하고 있었다. 자료구조가 아니라 **사용 규약** 위반이 race의 근원. `thread_local t_iWorkerIdx`(`:18`)로 스레드 신원을 부여해, 워커면 자기 deque(owner 경로), 외부/오버플로면 mutex 보호 global queue로 경로 분리(`:146`~`:158`)하는 하이브리드 큐로 재설계.
- **R**: race 소멸. "lock-free 도입 사고는 대개 자료구조가 아니라 규약 위반"이라는 교훈.
- ▸ 꼬리질문: "어떻게 확신했나?" → 코드 추론만 쌓지 않고 per-thread 프로파일러 카운터를 먼저 붙여 어느 스레드가 어느 큐를 만지는지 계측했다.

### C-4. "성능 최적화 경험 (1) — 알고리즘/메모리"

**주제: A* 스크래치 배열의 매 탐색 초기화를 세대 도장(generation stamp)으로 제거**
`Engine/Private/Manager/Navigation/Pathfinder.cpp:463`

- **S**: 경로탐색이 `kTotalCells` 크기의 gScore/parent/closed 배열을 **매 호출마다 초기화**해 오버헤드가 컸다.
- **T**: 매 탐색 O(N) clear 없이 배열 재사용.
- **A**: `thread_local` 스크래치(`:463`~`:467`) + 세대 스탬프 — 각 칸에 마지막 접근 세대를 기록하고, 탐색 시작마다 세대를 올린 뒤(`BeginSearchGeneration`, `:470`) `Touch()`에서 세대 불일치일 때만 그 칸을 리셋(`:472`~`:481`). 실제 방문한 칸만 만진다. 스레드별 저장이라 락도 없다.
- **R**: O(N) 초기화 → 방문칸 비례 비용. 워커 스레드 동시 탐색도 무락.
- ▸ 꼬리질문: "세대 카운터가 오버플로하면?" → wraparound 시점에 전체 재초기화로 방어. "왜 `thread_local`?" → mutex 없이 워커별 스크래치를 갖기 위해. "open set은?" → `std::priority_queue` + `std::greater` min-heap (`:483`).

### C-5. "성능 최적화 경험 (2) — 락 경합"

**주제: 모든 스레드가 치는 프로파일러의 동기화 비용 3단 축소**
`Engine/Private/Core/Profiler/CPUProfiler.cpp:29`

- **S**: 프로파일러는 전 스레드가 매 스코프마다 호출 — 여기 mutex가 있으면 계측 자체가 병목.
- **T**: 계측 오버헤드 최소화, 프레임 단위 집계는 유지.
- **A**: (1) 수집은 `thread_local` 스코프 스택으로 **push는 무락**(`:29`, `:66`) (2) 병합 지점(PopScope)만 좁은 mutex(`:93`) (3) 프레임 경계에서 `vector::swap` **더블 버퍼링**으로 O(1) 교환 + capacity 재사용(`:48`).
- **R**: 핫패스 락 구간 최소화, 프레임 핸드오프에 할당 없음.
- ▸ 꼬리질문: "swap이 왜 O(1)?" → 벡터 내부 포인터 3개만 교환. "clear 후 capacity는?" → 유지되므로 다음 프레임 재할당 없음.

### C-6. "게임 루프/타이밍 버그"

**주제: 델타타임 하드 클램프가 만든 슬로모션 스터터**
`Engine/Private/Core/CTimer.cpp:26`

- **S**: 프레임이 60fps 경계를 넘는 순간 게임이 실제 시간보다 느려지는 슬로모션 발생.
- **T**: 원인 규명 및 안정화.
- **A**: 과거 코드가 델타를 16.7ms로 **하드 클램프**해, 프레임이 그보다 길어질 때마다 게임 시간이 실제 시간보다 뒤처졌다. 스파이크 보호 상한(0.1s)만 남기고 하드 클램프 제거, 음수 델타 방어 추가 (`:28`~`:35`).
- **R**: 슬로모션 소멸. 클램프의 목적(스파이크 방지)과 부작용(시간 왜곡)을 사고 주석으로 성문화 (`:26`~`:27`).
- ▸ 꼬리질문: "클램프가 왜 필요하긴 한가?" → 디버거 정지/로딩 직후 거대 델타로 물리 터널링을 막으려고. 상한만 두고 정상 프레임은 그대로 흘린다.

### C-7. "컴파일이 안 되는 미스터리를 푼 경험"

**주제: `std::vector<std::atomic>` 계열을 못 담아 unique_ptr로 박싱**
`Engine/Private/Core/JobSystem.cpp:55`

- **S**: 워커 수만큼 work-stealing deque를 `vector<CWorkStealingDeque>`로 만들려는데 컴파일 실패.
- **T**: 원인 규명과 해법.
- **A**: deque가 `std::atomic` 멤버 + `alignas(64)` 조합이라 MSVC `construct_at` 요구를 못 채웠다(atomic은 복사/이동 불가). `make_unique`로 힙에 박싱하고 `vector<unique_ptr<...>>`로 저장 (`:62`).
- **R**: 컴파일 통과 + **주소 안정성**(원소가 재할당으로 안 움직임 — 다른 스레드가 deque 주소를 들고 있어도 안전)이라는 동시성 부수 이득.
- ▸ 꼬리질문: "deque에 빈 move ctor가 있던데?" → `vector` 요구를 맞추려던 흔적(`Engine/Public/Core/JobSystem/WorkStealingDeque.h:26`) — 아무것도 옮기지 않는 가짜 move라 실제로는 박싱으로 우회하는 게 맞다. 결함을 먼저 말하면 신뢰가 산다.

### C-8. "전처리기/매크로 때문에 빌드가 깨진 경험"

**주제: 디버그 `new` 매크로가 서드파티 placement new를 파괴**
`Engine/Include/ProfilerAPI.h:16`

- **S**: CRT 누수추적용 `#define new DBG_NEW`(`Engine/Public/Engine_Defines.h:38`~`:41`) 상태에서 Tracy 헤더가 컴파일 실패.
- **T**: 누수추적을 유지하면서 서드파티 헤더를 살리기.
- **A**: 매크로는 스코프/네임스페이스를 모르는 텍스트 치환이라 `new (ptr) T` 문법을 깨뜨림. 서드파티 include 앞에 `#pragma push_macro("new")`+`#undef new`, 뒤에 `pop_macro`로 격리 (`:16`~`:19`). Windows.h `min`/`max` 매크로 vs flatbuffers도 같은 방식으로 방어 (`Client/Private/Network/Client/SnapshotApplier.cpp:3`~`:11`).
- **R**: 누수추적과 서드파티가 공존.
- ▸ 꼬리질문: "매크로가 함수명도 바꾸나?" → 그렇다. Winters는 `OutputDebugStringA` 자체를 게이트 래퍼로 리매핑한다 (`Engine/Public/Engine_Defines.h:91`~`:98`) — 래퍼 정의를 매크로보다 먼저 둬서 원본 API 접근을 보존하는 정의-순서 설계.

### C-9. "잘못된 입력/신뢰 경계를 방어한 경험"

**주제: FlatBuffers 검증 실패를 조용히 삼켜 world가 얼어붙던 문제**
`Client/Private/Network/Client/SnapshotApplier.cpp:485`

- **S**: 서버/클라 스키마가 어긋나면 world가 멈추는데, 네트워크 스톨과 증상이 똑같아 구분 불가.
- **T**: 신뢰 불가 패킷을 역참조 전에 검증하고 실패를 가시화.
- **A**: 역참조 전에 `flatbuffers::Verifier`로 경계 검증(`:485`~`:486`). 실패 시 bare return이 아니라 **상한 8회 로그**로 방출(`:488`~`:495`). 나아가 접속 Hello에서 `dataBuildHash`를 대조해 서버/클라 데이터 버전 drift를 조기 가시화(`:505`).
- **R**: 조용한 freeze가 진단 가능한 실패로 바뀜. gotcha "authoritative packets"로 박제.
- ▸ 꼬리질문: "왜 무한 로그가 아니라 8회?" → 로그 스팸 자체가 장애가 되므로 상한. "wire 정수→enum은?" → `End` 범위 검사로 미정의 enum 값 차단 (`:121`).

### C-10. "락 보유 시간을 줄인 경험"

**주제: 워커→메인 콜백 마샬링을 swap-under-lock / execute-outside-lock으로**
`Client/Private/Network/Backend/CHttpClient.cpp:159`

- **S**: 워커 스레드 완료 콜백이 게임/UI 상태(단일 스레드 규약)를 직접 건드리면 race.
- **T**: 콜백을 메인 스레드로 옮기되 락을 오래 잡지 않기.
- **A**: 워커는 결과 콜백을 큐에 push만(락 짧게, `:151`~`:152`), 메인의 `ProcessCallbacks`는 **락 안에서 큐를 swap**으로 통째로 비우고(`:163`~`:164`) **락 밖에서** 콜백 실행(`:166`~`:170`).
- **R**: 락 구간이 swap 한 번으로 축소. 콜백이 다시 같은 클라이언트를 호출해도 재진입 데드락 없음.
- ▸ 꼬리질문: "락 안에서 콜백 실행하면?" → 콜백이 같은 mutex를 재획득하면 데드락, 아니어도 실행 시간만큼 생산자(워커)가 전부 블록.

### C-11. "데이터 레이스를 설계로 없앤 경험"

**주제: 워커에 멤버 대신 불변 스냅샷 값복사를 넘겨 공유 가변 상태 제거**
`Client/Private/Network/Backend/CHttpClient.cpp:119`

- **S**: 워커 실행 중 메인이 `m_AuthToken` 같은 멤버를 바꾸면 race.
- **T**: 동기화가 아니라 race 자체를 불가능하게.
- **A**: 요청 발사 직전 필요한 멤버만 `RequestSnapshot` 값 복사(`:119`~`:127`, 구조체는 `CHttpClient.h:44`)로 람다에 캡처. 실제 요청 함수 `DoRequestWith`를 **static**으로 선언해(`CHttpClient.h:53`) 워커 코드가 멤버에 접근하는 것을 컴파일러가 막게 했다.
- **R**: mutex 없이 race 원천 제거. "공유 가변 상태가 없으면 동기화도 없다."
- ▸ 꼬리질문: "복사 비용은?" → 요청당 문자열 몇 개 — 무락 정확성이 그 비용을 정당화. "왜 static 강제?" → 사람이 아니라 **타입 시스템**이 규약을 지키게.

### C-12. "AI/시스템을 멀티스레드화한 경험"

**주제: 미니언 AI를 lock 없이 병렬화한 2-pass + per-slot 버퍼**
`Client/Private/GamePlay/System/LocalUnitAISystem.cpp:61`

- **S**: 미니언 AI가 매 프레임 무거운데 결과가 공유 상태에 쓰여 병렬화가 어려웠다.
- **T**: race 없이 병렬로 돌리기.
- **A**: 결정(Decision)과 적용(Apply)을 **두 패스로 분리**하고, 결정 결과를 슬롯 버퍼에 락 없이 수집 — 슬롯은 메인=0/워커=idx+1로 worker+1개 (`:51`~`:58`, `Engine/Private/Core/JobSystem.cpp:35`~`:40`). 후보가 임계치 미만이면 직렬 폴백으로 스케줄 오버헤드 회피(`:79`). "lock을 거는 대신 phase를 나눠 lock이 필요 없게" 만들었다.
- **R**: 무락 병렬 수집 + 소규모 직렬 폴백의 균형.
- ▸ 꼬리질문: "병렬이 항상 빠른가?" → 아니다. 작업 granularity가 작으면 스케줄 비용이 이득을 먹는다 — 그래서 임계치. "쓰기는 언제?" → Apply 패스에서 단일 스레드로 커밋.

---

## 다른 챕터와의 연결

- 동시성·메모리 오더링의 원리 → [09_concurrency.md](09_concurrency.md) — (B)3~7·20, (C)2·3·5·10~12의 이론 배경
- 스마트 포인터·소유권 계약 → [04_pointers_smart_pointers.md](04_pointers_smart_pointers.md) — (A)10~13, (B)8·14의 상세
- 컴파일/링크 모델·DLL 경계 → [02_compile_link_dll.md](02_compile_link_dll.md) — (B)9·12·13·15·16, (C)1·7·8의 상세
- 네트워크 직렬화·신뢰 경계 → [12_network_serialization.md](12_network_serialization.md) — (B)17·21~23·27, (C)9의 상세
