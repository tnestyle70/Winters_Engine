# C++ 심화 — 기술면접 대비

> 대상: 자체 DX11 엔진(Winters) 제작 경험이 있는 게임 클라이언트/서버 프로그래머 지원자.
> 원칙: 모든 답변은 "정의 → 원리 → 트레이드오프 → 내 프로젝트 실전 연결"의 순서로 말한다. 암기 나열은 감점, 코드가 메모리에서 어떻게 움직이는지 설명하면 가점.

## 출제 경향 개요

게임회사 C++ 면접은 크게 4개 축으로 나온다.

1. **메모리와 레이아웃** — vtable, 정렬/패딩, 캐시. "가상 함수 호출이 왜 느린가"를 어셈블리 수준까지 파고든다. 60fps(16.6ms) 프레임 예산 안에서 수천 엔티티를 돌리는 업계 특성상 다른 도메인보다 훨씬 깊게 묻는다.
2. **소유권과 수명** — RAII, 스마트 포인터 내부 구조, 이동 의미론, 댕글링. "shared_ptr을 실무에서 왜 함부로 못 쓰는가" 같은 비용 질문이 단골.
3. **컴파일/링크/경계** — 번역 단위, ODR, 템플릿 인스턴스화, DLL 경계. 엔진(DLL)과 게임(EXE)을 나누는 회사일수록 dllexport/CRT 힙 질문이 나온다. 자체 엔진 제작자에게는 사실상 필수 검증 항목.
4. **모던 C++ 판단력** — C++11~20 기능을 아는지가 아니라 "언제 쓰고 언제 안 쓰는지"를 묻는다. 예외/RTTI를 끄는 게임업계 관례의 이유를 설명할 수 있으면 실무 경험자로 인정받는다.

Winters에서 실제로 밟았던 지뢰들 — `dllexport + unique_ptr 멤버`, `std::async future 버림 → 동기 블로킹`, `헤더 using std 오염` — 이 전부 면접 단골 주제와 정확히 겹친다. 이 문서는 그 실전 사례를 근거로 붙여서 "책으로 아는 사람"과 차별화하는 것이 목표다.

---

## 핵심 개념 정리

### 1. 컴파일/링크 모델

#### 1-1. 번역 단위 (Translation Unit)

- **정의**: 하나의 `.cpp` 파일에 전처리(`#include` 전개, 매크로 치환)를 끝낸 결과물. 컴파일러는 번역 단위 하나씩만 보고 `.obj`를 만들며, 다른 번역 단위의 존재를 모른다.
- **원리**: `전처리 → 컴파일(TU별 .obj) → 링크(심볼 해석, .exe/.dll)`. 컴파일 단계에서 다른 TU의 함수는 "선언만 믿고" 호출 코드를 생성하고, 실제 주소 연결은 링커가 한다. 그래서 선언과 정의가 어긋나도 컴파일은 통과하고 **링크 에러(LNK2019 unresolved external)** 또는 런타임 오동작으로 늦게 터진다.
- **예시**: 헤더에 `void Foo(int);`라 선언하고 cpp에 `void Foo(long)`을 정의하면 MSVC에서 name mangling이 달라 LNK2019. C에서는 mangling이 없어 조용히 잘못 링크될 수 있다.
- **게임 맥락**: Winters는 Engine(DLL) / Client(EXE) / Server(EXE) / Shared(GameSim)로 TU 집합이 나뉘고, 경계는 `EngineSDK/inc` 헤더 + import lib로만 만난다. "어느 vcxproj가 이 cpp를 컴파일하는가"가 곧 소유권이다 (`.claude/gotchas.md` 2026-05-28 Build ownership 항목: CMake 맵은 브라우징용, 실제 빌드 소유는 `.vcxproj`).

#### 1-2. ODR (One Definition Rule)

- **정의**: 프로그램 전체에서 변수/함수/클래스는 정의가 하나여야 한다. 단 `inline` 함수·템플릿·클래스 정의는 여러 TU에 있어도 되지만 **모든 정의가 토큰 단위로 동일**해야 한다.
- **원리**: 헤더에 정의된 inline 함수는 각 TU마다 인스턴스화되어 .obj에 들어가고, 링커가 COMDAT으로 하나만 골라 남긴다. 정의가 TU마다 다르면(예: 매크로 상태 차이로 같은 헤더가 다르게 전개) 링커는 진단 의무 없이 아무거나 고른다 → **진단 없는 UB**. ODR 위반은 컴파일러가 잡아주지 않는 가장 무서운 부류다.
- **예시**: A.cpp는 `#define ENABLE_DEBUG 1` 후 헤더 포함, B.cpp는 정의 없이 포함 → 같은 클래스의 멤버 배치가 달라져 한쪽 TU가 잘못된 오프셋으로 멤버를 읽는다.
- **게임 맥락**: Winters의 `Engine_Defines.h`가 `WINTERS_ENABLE_NON_AI_DEBUG_STRING` 같은 게이트 매크로를 `#ifndef`로 기본값 고정하는 이유가 이것 — 프로젝트마다 매크로 상태가 달라도 inline 함수(`WintersOutputDebugStringA`)의 정의 자체는 게이트값에 따라 갈리므로, 게이트는 **프로젝트 단위(vcxproj 전역 정의)**로만 바꾸고 TU 중간에 바꾸지 않는 규약을 지킨다 (`EngineSDK/inc/Engine_Defines.h:44-61`).

#### 1-3. 헤더 포함 모델과 include guard

- **정의**: C++은 (modules 이전까지) 텍스트 포함 모델. 헤더는 "복사-붙여넣기"이며, 같은 TU에 두 번 전개되면 클래스 재정의 에러가 나므로 include guard(`#ifndef/#define`) 또는 `#pragma once`로 막는다.
- **비교**: `#pragma once`는 파일 identity 기반(간결, 사실상 전 컴파일러 지원, 표준은 아님), guard는 매크로 기반(표준, 이름 충돌 위험). Winters는 신규 헤더에 `#pragma once`(`EngineSDK/inc/Manager/Navigation/Pathfinder.h:1`), 레거시 공용 헤더에 guard(`Engine_Defines.h`의 `#ifndef Engine_Defines_h__`)가 혼재 — "기존 스타일을 따른다"는 컨벤션으로 관리.
- **비용**: 헤더 하나가 무거우면 그걸 포함하는 모든 TU의 컴파일이 느려진다. 그래서 헤더에는 최소 include + 전방선언, 구현 detail은 cpp로.

#### 1-4. 전방선언 (forward declaration)

- **정의**: `class CWorld;`처럼 이름만 알리는 선언. 그 타입은 **불완전 타입(incomplete type)**이 되어 포인터/참조 선언, 함수 시그니처 사용은 가능하지만 `sizeof`, 멤버 접근, 값 멤버 선언, **delete**는 불가.
- **원리**: 컴파일러는 포인터 크기(8바이트)만 알면 되는 문맥에서는 완전한 정의가 필요 없다. include를 전방선언으로 바꾸면 컴파일 의존성이 끊겨 헤더 수정 시 재컴파일 파급이 줄어든다.
- **함정**: 불완전 타입에 delete를 호출하면 소멸자가 non-trivial일 때 UB. `std::unique_ptr<T>`의 `default_delete`는 `static_assert(sizeof(T) > 0)`으로 이를 컴파일 에러로 승격시킨다 → **pimpl 관용구에서 unique_ptr 멤버를 쓰면 소멸자를 cpp에 out-of-line으로 정의해야 하는 이유**.
- **게임 맥락**: `EngineSDK/inc/ECS/Systems/MCTSSystem.h:10`의 `class CWorld;`가 정확히 이 패턴 — 시스템 헤더가 World 전체 정의를 끌고 오지 않는다.

#### 1-5. 링크와 name mangling

- 심볼은 C++에서 타입 정보를 이름에 인코딩(mangling)해 오버로딩을 구분한다. `extern "C"`는 mangling을 끄므로 C ABI로 export할 때 사용. `static` 함수/익명 namespace는 internal linkage — TU 밖에서 안 보이므로 ODR 충돌 없이 같은 이름 사용 가능.
- DLL 경계에서 mangled name은 컴파일러/버전 종속 → **C++ 클래스를 통째로 export하면 같은 툴체인끼리만 안전**하다는 결론으로 이어진다 (12장 DLL 경계에서 상술).

---

### 2. 객체 메모리 레이아웃과 vtable

#### 2-1. 기본 레이아웃, 정렬/패딩

- 멤버는 선언 순서대로 배치되고(같은 접근 지정자 구간 내 보장), 각 멤버는 자신의 alignment 배수 주소에 놓이며 사이에 패딩이 생긴다. 구조체 전체 크기는 최대 정렬의 배수.
- 예: `struct { char c; double d; int i; };` → 1 + (7 padding) + 8 + 4 + (4 padding) = 24바이트. 멤버를 큰 것부터 정렬하면 16바이트.
- **게임 맥락**: 컴포넌트 배열(SoA/AoS)을 수천 개 순회할 때 패딩은 곧 캐시 라인 낭비. 64바이트 캐시 라인에 컴포넌트가 몇 개 들어가는지가 순회 성능을 결정한다. Winters ECS 컴포넌트(`TransformComponent` 등)를 POD에 가깝게 유지하는 이유.

#### 2-2. vtable / vptr 메커니즘

- **정의**: 가상 함수가 하나라도 있으면 클래스마다 가상 함수 포인터 테이블(vtable)이 하나 만들어지고, 각 객체는 숨겨진 vptr(보통 첫 8바이트)로 자기 클래스의 vtable을 가리킨다.
- **호출 원리**: `obj->VirtualFn()`은 `(1) obj에서 vptr 로드 → (2) vtable에서 함수 포인터 로드 → (3) 간접 call` 3단계. 직접 호출 대비 추가되는 것은:
  - 메모리 로드 2회 (vptr, 슬롯) — 객체가 캐시에 없으면 캐시 미스 2회.
  - 간접 분기 — 분기 예측 실패 시 파이프라인 플러시(수십 사이클).
  - **인라이닝 차단** — 실질적으로 가장 큰 비용. 인라인이 막히면 뒤따르는 상수 전파/루프 최적화가 전부 막힌다.
- **생성 중 디스패치**: 생성자/소멸자 실행 중 vptr은 "현재 실행 중인 클래스"의 vtable을 가리킨다. 생성자에서 가상 함수를 불러도 파생 오버라이드로 안 간다(순수 가상이면 crash). — 꼬리질문 단골.
- **게임 맥락**: 엔티티 1만 개가 각자 `virtual Update()`를 부르면 vtable 비용 + 객체가 힙에 흩어져 캐시 미스가 지배한다. Winters가 OOP 상속 트리 대신 ECS로 간 핵심 근거: 가상 호출은 **시스템 수준(수십 개, `ISystem::Execute`)**에만 남기고, **개체 수준(수천)은 컴포넌트 배열 순회**로 처리한다 (`EngineSDK/inc/ECS/Systems/MCTSSystem.h:14` — `class WINTERS_ENGINE CMCTSSystem final : public ISystem`, 프레임당 가상 호출은 시스템당 1회).

#### 2-3. 가상 소멸자가 필수인 이유

- `Base* p = new Derived; delete p;`에서 Base 소멸자가 virtual이 아니면 **UB**. 전형적 증상: Derived 소멸자와 Derived 부분의 멤버 소멸자가 안 불려 리소스 누수, 다중 상속이면 포인터 오프셋이 어긋나 힙 손상.
- 규칙: **다형적 base(가상 함수가 있는 클래스)는 `virtual ~Base()` 또는 protected non-virtual 소멸자**. 후자는 "base 포인터로 delete 자체를 금지"하는 설계.
- **Winters 근거**: `EngineSDK/inc/AI/BehaviorTree.h:40` — `virtual ~CBTNode() = default;`. BT 노드 트리는 `unique_ptr<CBTNode>` 자식들을 base 포인터로 소유하므로 가상 소멸자가 없으면 트리 파괴 시 파생 노드(쿨다운 데코레이터의 상태 등)가 부분 파괴된다.

#### 2-4. 다중 상속 레이아웃

- `class D : public A, public B`에서 D 객체는 [A 서브객체][B 서브객체][D 고유 멤버] 순서로 배치. A, B 각각 가상 함수가 있으면 **vptr이 2개**.
- `B* pb = pd;` 캐스팅은 **this 포인터에 오프셋을 더하는 실제 연산**이다(0이 아닐 수 있음). B 경로로 호출된 오버라이드 가상 함수는 this를 D 기준으로 되돌리는 **thunk**를 거친다.
- 그래서 다중 상속 객체에서 `reinterpret_cast`로 base를 오가면 오프셋 조정이 생략돼 즉시 UB — `static_cast`만이 조정을 안다.

#### 2-5. 가상 상속 (다이아몬드)

- `A ← B, A ← C, B/C ← D`에서 A가 두 벌 생기는 문제를 `virtual` 상속으로 A를 한 벌만 두어 해결. 대신 A 서브객체의 위치가 최파생 클래스에 따라 달라지므로, B/C는 A까지의 **오프셋을 vtable(MSVC는 vbtable)에서 런타임 조회**해 접근한다 → 멤버 접근마다 간접 참조 1회 추가.
- 가상 base의 생성자는 **최파생 클래스가 직접 호출**한다(중간 클래스의 호출은 무시됨).
- **게임 맥락**: 비용과 복잡도 때문에 게임 코드에서는 사실상 금기. "인터페이스(순수 가상, 데이터 없음) 다중 구현"까지만 쓰고 데이터 있는 다이아몬드가 필요해지면 컴포지션(ECS 컴포넌트)으로 푼다 — Winters가 컴포넌트 조합으로 챔피언 150종 스케일을 잡은 이유와 일치.

#### 2-6. devirtualization과 final

- 컴파일러가 정적으로 타입을 증명하면 가상 호출을 직접 호출로 바꾼다. `final` 클래스/메서드는 이 증명을 쉽게 해준다. Winters 신규 클래스에 `final`을 습관적으로 붙이는 실익 (`CMCTSSystem final`, `CPathfinder final`).

---

### 3. RAII와 예외 안전

#### 3-1. RAII (Resource Acquisition Is Initialization)

- **정의**: 리소스의 수명을 객체 수명에 묶는다. 생성자에서 획득, 소멸자에서 해제. 스코프를 벗어나는 **모든 경로**(정상 return, 조기 return, 예외 스택 되감기)에서 해제가 보장된다.
- **원리**: C++ 스택 되감기(stack unwinding)는 예외 전파 중 지역 객체 소멸자를 역순으로 호출한다. RAII는 이 언어 보장을 리소스 관리에 활용하는 것 — GC 없는 C++이 안전할 수 있는 근본 메커니즘.
- **반례가 내 코드에 있다**: `Client/Private/Network/Backend/CHttpClient.cpp`의 `DoRequestWith`는 WinHTTP 핸들 3개(hSession/hConnect/hRequest)를 raw로 들고 실패 경로마다 `WinHttpCloseHandle`을 수동 3연발로 호출한다(196-247행). 지금은 예외를 안 쓰니 동작하지만, `ToWide`의 wstring 생성이 던지는 순간 누수다. `unique_ptr<void, HandleCloser>` 또는 소멸자에서 닫는 4줄짜리 래퍼면 조기 return 전부에서 자동 해제된다 — 면접에서 "본인 코드에서 고치고 싶은 곳"으로 먼저 꺼낼 수 있는 정직한 소재.

#### 3-2. 예외 안전 3단계 보장

| 보장 | 의미 | 달성 수단 |
|---|---|---|
| **기본(basic)** | 예외가 나도 누수 없고 불변식 유지. 값은 미정일 수 있음 | 모든 리소스를 RAII로 |
| **강한(strong)** | 예외가 나면 호출 전 상태로 롤백 (커밋-롤백) | copy-and-swap: 사본에서 작업 후 noexcept swap으로 커밋 |
| **불투척(nothrow)** | 절대 안 던짐 | 소멸자, swap, move, 메모리 해제 |

- `vector::push_back`이 강한 보장의 표준 예: 재할당 실패 시 기존 벡터는 그대로다. 단 이 보장을 위해 요소 move가 noexcept일 때만 move를 쓴다(3-4절 `move_if_noexcept`와 연결).

#### 3-3. 소멸자와 예외

- 스택 되감기 중 소멸자가 또 던지면 예외 2개 동시 전파 → `std::terminate`. 그래서 C++11부터 소멸자는 암묵 `noexcept(true)`. 소멸자에서 실패 가능 작업(플러시, 조인)을 해야 하면 삼켜서 로깅하거나, 명시적 `Close()` API를 따로 제공한다.
- **Winters 근거**: `CHttpClient::~CHttpClient()`(`CHttpClient.cpp:103-117`)는 진행 중 future들을 `wait()`로 드레인한다 — 던질 수 있는 작업이 아니라 블로킹 대기만 하며, "블로킹은 파괴 시점에만 발생한다"를 헤더 주석으로 계약화했다(`CHttpClient.h:23-25`).

---

### 4. 스마트 포인터 내부 구조

#### 4-1. unique_ptr — 제로 오버헤드의 조건

- **구조**: 단독 소유, move-only. 기본 deleter(`default_delete<T>`)는 상태가 없는 empty class이므로 **compressed pair(EBO, Empty Base Optimization)**로 크기에서 사라진다 → `sizeof(unique_ptr<T>) == sizeof(T*)` = 8바이트. 소멸자 호출은 인라인되어 raw new/delete 대비 런타임 추가 비용 0.
- **크기가 커지는 경우**: 상태 있는 deleter — 특히 **함수 포인터 deleter** `unique_ptr<T, void(*)(T*)>`는 16바이트가 되고 간접 호출이 생긴다. 캡처 없는 람다 타입을 쓰면 다시 8바이트(상태 없는 타입이므로).
- **move-only의 의미**: 복사 생성자/대입이 delete되어 있어 소유권 이전이 코드에 `std::move`로 명시된다. 함수 인자로 `unique_ptr` 값 전달 = "소유권 넘긴다"는 시그니처 차원의 문서화.
- **Winters 근거**: 팩토리 컨벤션 — `static unique_ptr<CHttpClient> Create(const string& baseURL)` + private 생성자(`CHttpClient.h:27,41`), `CMCTSSystem::Create()`가 `unique_ptr<CMCTSSystem>` 반환(`MCTSSystem.h:22-27`). 소유는 unique_ptr, 비소유 참조는 raw 포인터(`const CNavGrid* pGrid` — `Pathfinder.h:27`)로 구분하는 것이 레포 전반의 소유권 규약.

#### 4-2. shared_ptr — control block과 비용

- **구조**: `sizeof(shared_ptr) == 포인터 2개`(16바이트) — 객체 포인터 + **control block** 포인터. control block에는 strong count, weak count, deleter, (커스텀 시) allocator가 들어간다.
- **make_shared vs new**:
  - `make_shared<T>()`: 객체 + control block을 **한 번의 할당**에 배치 → 할당 1회 절약, 객체와 카운트가 같은 캐시 라인 근처라 locality 유리, 예외 안전(C++17 전 인자 평가 interleaving 문제 원천 차단).
  - 단점: `weak_ptr`가 살아있는 동안 strong count가 0이 되어 **소멸자는 돌아도 객체 메모리 자체는 회수 안 됨**(블록이 통짜라). 큰 객체 + 오래 사는 weak_ptr 조합이면 `shared_ptr<T>(new T)`가 낫다.
- **atomic refcount 비용**: 카운트 증감은 `lock xadd` 류 atomic RMW. 단일 스레드에서도 수 나노초, 여러 스레드가 같은 control block을 만지면 **캐시 라인 핑퐁**으로 수십 배 악화. 그래서 hot loop에서 shared_ptr **값 복사**는 금물 — `const shared_ptr&` 또는 raw 포인터/참조로 전달한다.
- **"thread-safe"의 정확한 범위**: 카운트 증감만 atomic이다. (1) 같은 shared_ptr **객체** 하나를 두 스레드가 동시에 write하면 race, (2) 가리키는 대상 객체 접근은 당연히 보호 안 됨.
- **Winters 판단**: 게임 오브젝트/시스템 소유는 전부 unique_ptr, 프레임 단위 참조는 핸들(entity id)이나 raw 포인터. shared_ptr가 필요해지는 순간은 대개 "소유권 설계를 미룬 신호"로 취급한다 — 서버 권위 시뮬레이션에서 수명은 스냅샷 틱이 결정하지 refcount가 결정하지 않는다.

#### 4-3. weak_ptr — 순환 참조 절단

- **정의**: control block만 참조(weak count 증가)하고 객체 수명에는 관여하지 않는 관찰자. `lock()`으로 strong count가 살아있으면 shared_ptr 획득, 죽었으면 nullptr — **획득과 검사가 원자적**이라 "체크 후 사용" race가 없다.
- **순환 참조**: A가 `shared_ptr<B>`, B가 `shared_ptr<A>`면 둘 다 count가 1 밑으로 안 내려가 영원히 누수. 부모→자식은 shared(소유), 자식→부모는 weak(역참조)로 방향을 정하는 것이 정석. 게임에서 전형적 사례: 버프가 시전자를, 시전자가 버프 리스트를 서로 shared로 잡는 실수.

#### 4-4. enable_shared_from_this

- **문제**: 멤버 함수 안에서 `shared_ptr<T>(this)`를 만들면 **새 control block**이 생겨 이중 delete.
- **원리**: `enable_shared_from_this<T>`를 상속하면 base에 숨은 `weak_ptr<T>` 멤버가 생기고, 최초 `shared_ptr` 생성 시 control block이 이 weak를 세팅해 준다. `shared_from_this()`는 그 weak를 lock해 **기존 블록을 공유**하는 shared_ptr을 반환.
- **함정**: shared_ptr이 소유하기 전(생성자 안 포함)에 부르면 C++17부터 `std::bad_weak_ptr` throw(그 전엔 UB). 비동기 콜백에 `this`를 넘길 때 수명 연장 용도로 자주 쓰인다 — Winters `CHttpClient`는 대신 "소멸자가 모든 future를 wait한다"는 계약으로 raw `this` 캡처를 안전하게 만들었는데(`CHttpClient.cpp:105-116`), enable_shared_from_this + weak 캡처는 그 대안 설계로 면접에서 비교 설명할 수 있다.

---

### 5. 이동 의미론

#### 5-1. rvalue 참조와 value category

- lvalue = 이름/주소가 있는 것, prvalue = 순수 임시(리터럴, 함수 반환값 식), xvalue = "만료 예정 lvalue"(`std::move(x)` 결과). `T&&`는 rvalue(prvalue/xvalue)에만 바인딩되어 "이 객체의 내부를 털어가도 된다"는 신호가 된다.
- move 생성자는 포인터/핸들만 훔치고 원본을 비운다 — `vector` move는 원소 복사 없이 포인터 3개 스왑, O(1).

#### 5-2. std::move는 캐스팅일 뿐

- 구현은 사실상 `static_cast<remove_reference_t<T>&&>(x)`. **아무것도 이동시키지 않는다.** 이동은 그 xvalue를 받은 move 생성자/대입이 한다. move 생성자가 없는 타입에 move를 걸면 조용히 **복사**된다(감점 포인트가 아니라 언어 규칙 — 오버로드 해석이 const T&로 폴백).
- **moved-from 상태**: 표준 라이브러리 타입은 "valid but unspecified" — 소멸/재대입은 안전, 값을 읽는 건 금물. 자기 타입을 만들 때도 이 계약을 지키는 게 관례.
- **Winters 근거**: `std::future`는 move-only라서 `m_PendingRequests.push_back(std::move(task))`(`CHttpClient.cpp:156`)가 필수 — move가 "문법 설탕"이 아니라 소유권 이전의 유일한 통로인 실례.

#### 5-3. Perfect forwarding / universal(forwarding) reference

- 템플릿 `template<class T> void f(T&& x)`의 `T&&`만 forwarding reference(auto&& 포함). **reference collapsing**: `& + && = &`, `&& + && = &&`. lvalue를 넘기면 `T=U&`로 추론되어 `U& &&` → `U&`, rvalue면 `T=U`로 `U&&`.
- `std::forward<T>(x)`는 이 추론 정보를 되살려 lvalue는 lvalue로, rvalue는 rvalue로 전달 — `emplace_back`, `make_unique`가 생성자 인자를 복사 없이 꽂아 넣는 원리.
- **구분 주의**: `void f(MyType&& x)`처럼 구체 타입의 `&&`는 그냥 rvalue 참조지 forwarding reference가 아니다. 꼬리질문 단골.

#### 5-4. RVO / NRVO / return std::move 안티패턴

- **RVO**: `return T{...}` (prvalue 반환)는 C++17부터 **보장된 copy elision** — 복사/이동 생성자가 delete여도 컴파일된다. 호출자 스택의 반환 슬롯에 처음부터 직접 생성.
- **NRVO**: 이름 있는 지역 변수 반환은 elision이 허용이지 보장은 아님(단일 return 경로면 대부분 적용). 실패해도 지역 변수 반환은 자동으로 move 취급.
- **`return std::move(local);`은 안티패턴**: NRVO를 명시적으로 꺼버리고 move를 강제한다 — 최선(elision, 0회)을 차선(move 1회)으로 강등. 예외: 반환 타입이 지역 타입과 다르거나, 멤버를 반환할 때.
- **noexcept move와 vector**: `vector` 재할당 시 `move_if_noexcept` — 요소 move 생성자가 noexcept가 아니면 강한 보장을 위해 **복사**로 폴백한다. move 생성자에 `noexcept`를 빼먹으면 vector에 담는 순간 성능이 복사 수준으로 떨어지는, 코드리뷰에서 실제로 잡히는 함정.

---

### 6. 템플릿

#### 6-1. 인스턴스화 모델

- 템플릿은 코드가 아니라 **코드 생성기**. 사용되는 시점에 해당 타입 인자로 인스턴스화되며, 그래서 정의 전체가 헤더에 있어야 한다(각 TU가 독립적으로 인스턴스화 → COMDAT으로 링커가 병합). `extern template class std::vector<Foo>;`로 특정 TU에만 인스턴스화를 몰아 컴파일 시간을 줄일 수 있다.
- **2단계 이름 검색**: 템플릿 정의 시점에 비의존 이름 검사, 인스턴스화 시점에 의존 이름 검사. 의존 타입 멤버에는 `typename`, 의존 템플릿 멤버에는 `template` 키워드 필요.
- **비용**: 타입마다 코드가 복제되는 code bloat — 게임 빌드에서 링크 시간/바이너리 크기의 주범 중 하나. 타입 무관 로직을 non-template 베이스로 빼는 "thin template" 관용구로 완화.

#### 6-2. SFINAE → concepts

- **SFINAE**(Substitution Failure Is Not An Error): 템플릿 인자 치환 실패는 에러가 아니라 그 후보를 오버로드 집합에서 제거. `enable_if`로 "이 조건일 때만 이 오버로드"를 구현하던 전통 기법 — 에러 메시지가 지옥.
- **C++20 concepts**: 같은 목적을 선언적으로. `template<class T> requires std::integral<T>` 또는 `void f(std::integral auto x)`. 제약 실패가 "어떤 요구를 못 채웠는지" 한 줄로 나오고, 제약 간 부분 순서로 오버로드 우선순위도 자연스럽다. 면접 요지: "SFINAE가 하던 일을 컴파일러가 이해하는 1급 문법으로 승격시킨 것".

#### 6-3. CRTP — 정적 다형성

```cpp
template<class Derived>
struct UpdaterBase {
    void Tick(float dt) { static_cast<Derived*>(this)->DoTick(dt); } // 가상 호출 없음, 인라인 가능
};
struct MinionUpdater : UpdaterBase<MinionUpdater> { void DoTick(float dt); };
```

- base가 파생 타입을 템플릿 인자로 알고 있어 **컴파일 타임에 디스패치** — vtable 없음, 인라인 가능, 객체 크기에 vptr 없음. 대가: 타입별 코드 복제, 서로 다른 파생을 한 컨테이너에 못 담음(공통 base 타입이 없으므로).
- **선택 기준**: 런타임에 타입이 섞여야 하면 가상 함수, 컴파일 타임에 타입이 고정되고 호출이 hot하면 CRTP. Winters에서 시스템 목록(런타임 등록, 프레임당 호출 수십 회)은 `ISystem` 가상 인터페이스로 충분하고, per-entity hot path는 아예 다형성 없이 컴포넌트 배열 순회 — "CRTP를 안 쓴 이유"까지 말할 수 있으면 설계 판단력 어필.

---

### 7. STL 컨테이너 내부와 무효화 규칙

#### 7-1. vector

- **구조**: 포인터 3개(begin, end, capacity_end). 연속 메모리 보장 — 캐시 프렌들리의 왕.
- **성장**: capacity 초과 시 재할당 — MSVC는 1.5배, libstdc++/libc++는 2배. 성장 계수가 기하급수라 push_back N회의 총 이동은 O(N), **분할상환 O(1)**. 재할당 한 번은 O(N)이므로 프레임 중 스파이크가 되면 안 되는 곳은 `reserve` 필수.
- **무효화**: 재할당 시 **모든** iterator/포인터/참조 무효. 재할당이 없어도 `insert/erase`는 그 지점 이후 무효. 루프 돌며 erase할 때는 `it = v.erase(it)` 또는 **역순 인덱스** — Winters `CHttpClient::PruneCompletedRequests`(`CHttpClient.cpp:129-141`)가 `for (size_t i = size; i > 0; --i)` 역순 erase로 인덱스 시프트 문제를 회피하는 실례.

#### 7-2. unordered_map

- **구조**: 버킷 배열 + 각 버킷은 체이닝(노드 연결). `hash(key) % bucket_count`로 버킷 선택. load factor(size/buckets)가 `max_load_factor`(기본 1.0)를 넘으면 rehash — 버킷 배열을 키우고 전 노드를 재배치.
- **무효화 규칙이 특이**: rehash 시 **iterator는 전부 무효**지만, 노드 기반이라 **요소에 대한 포인터/참조는 유지**된다. `map`(RB-tree)도 노드 기반이라 erase된 요소 외에는 iterator까지 안정.
- **비용 감각**: 해시 계산 + 캐시 미스(버킷→노드 점프)로 원소 수천 이하 hot path에서는 정렬된 `vector` + 이진 탐색이나 open-addressing 해시가 이기는 경우가 흔하다. 문자열 키면 해시 비용 자체가 지배 — 게임에서 name→id를 로드 타임에 정수 핸들로 구워두는 이유.

#### 7-3. 무효화 요약표 (면접 즉답용)

| 컨테이너 | 삽입 | 삭제 |
|---|---|---|
| vector | 재할당 시 전부 / 아니면 삽입점 이후 | 삭제점 이후 |
| deque | 양끝 삽입: iterator 무효, 참조 유지 / 중간: 전부 | 양끝: 삭제분만 / 중간: 전부 |
| list/map/set | 없음 | 삭제된 요소만 |
| unordered_* | rehash 시 iterator 전부 (참조는 유지) | 삭제된 요소만 |

---

### 8. const correctness / constexpr

- **const 멤버 함수**: this가 `const T*` — 논리적 "읽기 전용" 계약. 캐시/뮤텍스처럼 관측 불가 상태는 `mutable`로 예외 허용(**논리적 const** — 비트 단위가 아니라 의미 단위 불변). C++11부터 표준 라이브러리는 const 멤버 함수 = 스레드 간 동시 호출 안전을 계약으로 가정한다 → mutable 멤버는 스스로 동기화해야 함.
- **읽는 법**: `const char* p`(대상 불변) vs `char* const p`(포인터 불변) — const는 왼쪽을 수식, 맨 앞이면 오른쪽.
- **constexpr**: "컴파일 타임에 평가 **가능**" — 상수 문맥에선 컴파일 타임, 아니면 런타임. `consteval`(C++20)은 컴파일 타임 강제, `constinit`은 정적 초기화 강제(static init order fiasco 방지, const는 아님). `if constexpr`(C++17)은 분기 자체를 컴파일 타임에 소거 — 템플릿에서 인스턴스화되지 않는 쪽은 컴파일 검사도 (의존 코드에 한해) 면제.
- **Winters 근거**: `MCTSSystem.h:39-40`의 `static constexpr f32_t TICK_INTERVAL = 5.f; static constexpr u32_t ITERATIONS = 50;` — 매크로 대신 타입 있는 컴파일 타임 상수, ODR 걱정 없이 헤더에 정의(constexpr static 멤버는 암묵 inline).

---

### 9. 캐스팅 4종

| 캐스트 | 하는 일 | 비용/위험 |
|---|---|---|
| `static_cast` | 관련 타입 간 변환(수치, up/down-cast, void*↔T*). 다중 상속 오프셋 조정 **함** | down-cast는 실제 타입이 다르면 UB (검사 없음) |
| `dynamic_cast` | RTTI 기반 안전한 down/cross-cast. 실패 시 포인터 nullptr / 참조 throw | vtable→type_info 순회. cross-cast·깊은 계층은 수백 사이클. DLL 경계에선 type_info **문자열 비교**까지 |
| `const_cast` | const/volatile 제거 | 원본이 진짜 const 객체면 수정 시 UB. 레거시 C API 어댑터 전용 |
| `reinterpret_cast` | 비트 재해석 | 정렬/aliasing 규칙 위반 시 UB. 다중 상속 오프셋 조정 **안 함** |

- C 스타일 `(T)x`는 위 넷을 순차 시도하는 것과 같아 **의도가 코드에 안 남고**, 검색도 안 되고, const_cast+reinterpret 조합까지 조용히 수행한다 — 금지 규약이 일반적.
- 게임 관례: dynamic_cast 대신 자체 type id(enum/비트마스크) + static_cast. Winters ECS는 컴포넌트 존재 여부가 곧 타입 질의라 down-cast 자체가 드물다.

---### 10. UB 대표 사례

#### 10-1. 댕글링 (수명 종료 후 접근)

- 지역 변수 참조 반환, `vector` 재할당 후 옛 포인터 사용, **람다의 참조 캡처가 비동기로 넘어가 스코프보다 오래 사는** 패턴이 3대장.
- **Winters 실전**: `CHttpClient::LaunchAsyncRequest`의 워커 람다는 `this`를 캡처한다(`CHttpClient.cpp:149`). 이게 안전한 유일한 이유는 소멸자가 모든 pending future를 `wait()`하는 계약 때문이고, 과거에는 `std::async` 반환 future를 버려서 **우연히**(임시 future 소멸자가 블로킹해서) 안전했다 — `.claude/gotchas.md` 2026-07-09 [Async lifetime]: "discarding the std::future returned by std::async makes the call synchronously blocking; async wrappers must own the future's lifetime, and fix raw `this` capture in the same change". "우연한 안전은 안전이 아니다"라는 문장으로 마무리하면 강하다. 추가로 `SetAuthToken`과 워커의 멤버 접근 race는 `RequestSnapshot` 호출 시점 복사본 + static 멤버 함수 강제로 차단했다(`CHttpClient.h:43-52`, `CHttpClient.cpp:177-182`).

#### 10-2. signed 정수 오버플로

- `int` 오버플로는 UB (unsigned는 2^n 모듈러로 정의됨). 컴파일러는 "오버플로는 없다"를 전제로 최적화한다: `for (int i = 0; i <= n; ++i)`에서 `i+1 > i`를 항상 참으로 접거나, `x + 1 < x` 검사를 통째로 제거. 오버플로 검사를 **오버플로가 난 뒤에** 하는 코드는 최적화로 증발한다 — 검사는 연산 전에 (`x > INT_MAX - y`).

#### 10-3. strict aliasing

- 서로 다른 타입의 포인터는 같은 메모리를 가리키지 않는다고 컴파일러가 가정한다. `float f; *(int*)&f`는 UB — 컴파일러가 read/write 재배열을 해버릴 수 있다. 예외는 `char/unsigned char/std::byte`를 통한 접근.
- 합법 경로: `memcpy`(컴파일러가 no-op으로 최적화) 또는 C++20 `std::bit_cast`. 유명한 Quake 빠른 역제곱근의 `*(long*)&y`는 기술적으로 UB — 면접에서 "당대엔 통했지만 지금은 bit_cast"로 답하면 깔끔.

#### 10-4. 기타 필수 목록

- 널 역참조, 배열 범위 밖 접근, 초기화 안 된 값 읽기, data race(동기화 없는 동시 read/write), ODR 위반, 불완전 타입 delete(1-4절), base 포인터 delete(2-3절). 공통 성질: **UB는 "그 지점의 크래시"가 아니라 프로그램 전체 의미의 소멸** — 시간을 거슬러 앞선 코드의 최적화까지 바뀔 수 있다.

---

### 11. C++11/14/17/20 핵심 변화

- **C++11 (현대 C++의 탄생)**: move semantics/rvalue 참조, `auto`, 람다, `unique_ptr/shared_ptr/weak_ptr`, `nullptr`, range-for, `constexpr`, **메모리 모델 + `std::atomic/thread/mutex/future`**(이전엔 표준에 스레드 개념 자체가 없었다), `enum class`, `override/final`, 위임 생성자, `= default/delete`.
- **C++14 (11 다듬기)**: generic 람다(`auto` 파라미터), `make_unique`(11에 빠졌던 구멍), 반환 타입 추론, constexpr 제한 완화, 람다 init-capture(`[p = std::move(ptr)]` — move 캡처가 이때부터 가능).
- **C++17**: structured bindings(`auto [k, v] : map`), `if constexpr`, `optional/variant/string_view`, **보장된 copy elision**, inline 변수(헤더 전역 상수의 ODR 해결), `[[nodiscard]]`, 병렬 알고리즘, `shared_mutex`.
- **C++20**: **concepts**, ranges, **coroutines**(`co_await` — 서버 비동기 IO 판도 변화), modules(텍스트 include 모델 대체 — MSVC 지원 진행 중), `<=>` 3방향 비교, `std::span`(포인터+길이의 안전한 view), `jthread`, `std::format`, `bit_cast`, designated initializer.
- **게임업계 감각**: 클라이언트 코드는 C++17이 사실상 표준선, 콘솔 툴체인 보수성 때문에 20 도입은 신중. 코루틴은 서버(IOCP 위 비동기 플로우)에서 먼저 가치가 큼 — Winters 서버의 IOCP×Fiber 구조와 "코루틴이 하는 일을 Fiber로 직접 구현해봤다"로 연결 가능.

---

### 12. DLL 경계 실전 (Winters 직접 근거)

#### 12-1. export 매크로 3분기

`EngineSDK/inc/WintersAPI.h`:

```cpp
// WINTERS_STATIC_BUILD: Tools/컨버터 등 DLL 아닌 EXE 에서 정의 — export/import 제거
#ifdef WINTERS_STATIC_BUILD
    #define WINTERS_ENGINE
#elif defined(WINTERS_ENGINE_EXPORTS)
    #define WINTERS_ENGINE __declspec(dllexport)   // Engine 프로젝트가 빌드될 때
#else
    #define WINTERS_ENGINE __declspec(dllimport)   // Client/Server 가 헤더를 볼 때
#endif
```

- **같은 헤더**를 Engine이 컴파일하면 export, Client가 컴파일하면 import로 읽힌다 — 매크로가 "이 헤더는 지금 누구의 번역 단위인가"에 따라 다르게 전개되는 것. dllimport는 링커 최적화(간접 호출 IAT 경유를 컴파일 타임에 확정)에도 쓰인다. 정적 링크 툴(에셋 컨버터)을 위한 3번째 분기까지 있는 게 실무 포인트.

#### 12-2. dllexport 클래스 + unique_ptr 멤버 gotcha (프로젝트에서 직접 밟은 것)

- **현상**: `class WINTERS_ENGINE Foo { std::unique_ptr<Bar> m; };` — 아무도 Foo를 복사하지 않는데 컴파일 에러(C2280 "attempting to reference a deleted function").
- **원리**: 일반 클래스는 암묵 특수 멤버(복사 생성자 등)를 **실제로 사용될 때만** 인스턴스화한다. 그런데 **dllexport 클래스는 모든 멤버 함수를 즉시 인스턴스화해서 export**해야 하므로, 컴파일러가 암묵 복사 생성자를 만들려다 unique_ptr 멤버(복사 불가)에서 실패한다.
- **처방**: 복사 생성자/대입을 명시적으로 `= delete` — 프로젝트 이력에 2026-04-23 gotcha로 박제된 규칙("WINTERS_ENGINE dllexport + unique_ptr 멤버 = copy ctor/assign 명시 delete 필수"). 살아있는 코드 근거:

```cpp
// EngineSDK/inc/ECS/Systems/MCTSSystem.h
class WINTERS_ENGINE CMCTSSystem final : public ISystem
{
public:
    ~CMCTSSystem() override = default;
    CMCTSSystem(const CMCTSSystem&) = delete;             // dllexport가 암묵 복사를
    CMCTSSystem& operator=(const CMCTSSystem&) = delete;  // 인스턴스화하는 것을 차단
    ...
private:
    std::unique_ptr<CMCTSPlanner> m_pPlanner;              // 이 멤버가 원인
};
```

#### 12-3. C4251 — STL 멤버의 dll-interface 경고

- dllexport 클래스가 `std::vector` 등 export 안 된 타입 멤버를 가지면 C4251("needs to have dll-interface"). 본질: **STL의 구현/레이아웃은 컴파일러 버전·설정(_ITERATOR_DEBUG_LEVEL 등)에 따라 달라서**, DLL과 EXE가 다른 툴체인이면 같은 vector를 다르게 해석한다.
- Winters의 선택: `EngineSDK/inc/Engine_Defines.h:30`의 `#pragma warning(disable : 4251)` — **"Engine과 Client는 항상 같은 VS 버전, 같은 구성(Debug/Release 짝맞춤)으로 빌드한다"는 전제를 깔고** 경고를 끈 것. 이 전제를 못 깔는 서드파티 배포용 SDK라면 pimpl로 STL을 숨기거나 C ABI(extern "C" + 불투명 핸들)로 내려야 한다 — 전제와 대안을 같이 말하는 게 이 질문의 정답 구조.

#### 12-4. CRT 힙 경계

- DLL과 EXE가 **서로 다른 CRT**(정적 CRT /MT, 또는 다른 버전의 동적 CRT)를 쓰면 힙이 분리된다 → 한쪽에서 `new` 한 걸 다른 쪽에서 `delete` 하면 힙 손상. 동적 CRT(/MD)로 통일하면 힙이 하나라 완화되지만, 원칙은 **"할당한 모듈이 해제한다"** — Winters의 `Create()`가 unique_ptr를 반환하는 팩토리 패턴은 소멸 코드(deleter 인스턴스화 위치)가 어느 모듈 것인지 의식해야 하는 지점이고, 같은 CRT 전제 하에서 성립한다. COM이 `IUnknown::Release`(객체 스스로 자기 모듈에서 delete)로 푸는 문제와 동일 계열.
- Debug 힙 추적: `Engine_Defines.h:32-42`의 `_CRTDBG_MAP_ALLOC` + `#define new DBG_NEW` — CRT 디버그 힙으로 누수 지점의 파일/라인을 잡는 고전 기법. 이 `#define new` 매크로가 placement new나 서드파티 헤더와 충돌하는 것도 알려진 부작용(그래서 include 순서 규약이 생긴다).

#### 12-5. 헤더에 using std 금지 규약

- `.claude/gotchas.md` 2026-05-14 [C++ headers]: "public headers leaking `using std::...` pollutes includers → qualify STL names with `std::` in headers; keep using declarations local to .cpp or block scope only."
- 이유: 헤더의 using은 그 헤더를 포함하는 **모든 TU의 전역 네임스페이스를 오염** — 이름 충돌, 의도치 않은 오버로드 해석 변화(ADL과 결합하면 조용한 동작 변경). Winters의 정직한 현실: 레거시 공용 헤더 `Engine_Defines.h:17`에 `using namespace std;`가 아직 있고(초기 학습기 코드), 신규 헤더는 `Pathfinder.h`의 `std::vector<CNavGrid::Cell>`처럼 전부 한정한다 — "레거시를 한 번에 못 걷어내니 신규 코드부터 규약으로 막고, 걷어내기는 boundary 리팩터링 슬라이스로 계획"이라는 답변이 실무형.

---

### 13. 게임에서의 예외/RTTI 비활성 관례

#### 13-1. 예외 비활성 (`/EH` off, `-fno-exceptions`)

- **비용의 실체**: x64 MSVC/Itanium ABI는 테이블 기반이라 **안 던지는 경로의 런타임 비용은 거의 0**이다. 그런데도 끄는 이유는 (1) unwind 테이블/랜딩 패드로 인한 **바이너리 크기 증가**(콘솔 메모리 예산), (2) "이 함수가 던질 수 있다" 가정이 만드는 **최적화 제약**(코드 이동/인라인 억제), (3) 과거 콘솔 툴체인의 미성숙 지원과 그로 굳은 코드베이스 관례, (4) 프레임 중간에 스택 되감기로 복구한다는 모델 자체가 게임 루프와 안 맞음 — 복구가 아니라 "죽거나, 값으로 실패를 돌려주거나".
- **대안 패턴**: 에러코드/Result 타입 + out 파라미터. **Winters 실물**: `EngineSDK/inc/Manager/Navigation/Pathfinder.h:13-21`의 `enum class ePathFindResult : u8_t { Success, NullGrid, StartBlocked, GoalBlocked, NoRoute, BrokenPath }` — 과거 "빈 vector 하나로 4가지 실패 원인이 뭉개져 미니언이 조용히 멈추던" 사고(silent empty path)를, 예외 없이 실패 원인을 구분하는 결과 enum으로 해결했다. 예외를 안 쓰는 코드베이스에서 "silent fail을 어떻게 막느냐"는 꼬리질문의 준비된 답.
- **주의**: 예외를 꺼도 `std::vector` 등은 bad_alloc 경로를 전제하므로, 진짜 끄려면 할당 실패 정책(터뜨리기)까지 세트로 정해야 한다. Winters는 MSVC 기본(/EHsc)으로 두되 게임플레이 경계에서 예외를 설계 수단으로 쓰지 않는 절충 — "끈 것"과 "안 쓰는 것"의 구분을 정확히 말하면 신뢰도가 올라간다.

#### 13-2. RTTI 비활성

- `dynamic_cast`/`typeid`가 쓰는 type_info 메타데이터를 제거해 바이너리를 줄이고, 계층 순회 비용이 큰 dynamic_cast를 원천 금지하는 효과. 대체: 자체 type id(enum, 해시), 또는 ECS처럼 "타입 질의 = 컴포넌트 보유 질의"로 설계 차원에서 제거. DLL 경계에서 dynamic_cast는 type_info **이름 문자열 비교**까지 하므로 더 느리고 취약하다는 점도 언급 가치.

---

### 14. 커스텀 할당자 / 메모리 풀

- **왜**: 범용 `malloc/new`는 (1) 스레드 동기화, (2) 크기별 빈 탐색, (3) 단편화, (4) 할당 시점 예측 불가라는 비용을 진다. 게임은 할당 패턴이 규칙적(프레임 단위, 동종 객체 대량)이라 특화 할당자의 이득이 크다.
- **3대 패턴**:
  - **Linear/Arena(frame allocator)**: 포인터 증가만으로 할당, 프레임 끝에 통째로 리셋. 할당 = 덧셈 1회, 해제 = 0. 수명이 "이번 프레임"인 임시 데이터 전용.
  - **Object Pool**: 동일 크기 객체를 미리 확보해 free-list로 재사용. 스폰/디스폰이 잦은 것들(투사체, FX 인스턴스, 미니언)에 — 할당 스파이크와 단편화 제거, 같은 풀 = 연속 메모리라 순회 캐시 이득까지.
  - **std::pmr**(C++17): `polymorphic_allocator` + `monotonic_buffer_resource` — 컨테이너 타입을 바꾸지 않고(allocator가 타입에 안 박힘) 메모리 소스만 갈아끼우는 표준 경로.
- **Winters 연결**: 미니언 웨이브/FX 큐 스폰이 정확히 pool 적합 패턴이고, ECS 컴포넌트 배열 자체가 "동종 데이터 연속 배치"라는 풀의 이점을 구조적으로 선취한 형태다. 명시적 범용 풀 할당자는 아직 없다 — "프로파일러(자체 Profiler로 17.8ms→9ms 잡은 경험)로 할당이 병목으로 찍히는 지점부터 도입"이 계획이라고 말하면 과장 없이 강하다.

---

## 예상 질문 & 모범답변

### Q1. 소스 코드가 실행 파일이 되기까지의 과정을 설명해 주세요.

**답**: 전처리 → 컴파일 → 링크 3단계입니다. 전처리기가 `#include`를 텍스트로 전개하고 매크로를 치환해 번역 단위를 만들고, 컴파일러가 번역 단위 **하나만 보고** 목적 파일을 만듭니다. 이때 다른 TU의 함수는 선언만 믿고 심볼 참조로 남기고, 링커가 모든 .obj와 .lib의 심볼을 해석해 실행 파일을 만듭니다.
**왜 중요**: 컴파일러는 TU 밖을 못 보기 때문에 선언/정의 불일치가 링크 에러(LNK2019)나 ODR 위반으로 **늦게** 터지고, 헤더가 무거우면 그걸 포함한 모든 TU가 느려집니다.
**프로젝트 연결**: Winters는 Engine DLL / Client·Server EXE로 나뉘어 링크 단위 경계가 곧 아키텍처 경계입니다. import lib를 통해 심볼이 IAT로 연결되는 것까지 설명할 수 있습니다.
**꼬리질문 대비**: "inline 함수는 어느 단계에서 중복이 정리되나?" → 각 TU가 인스턴스화한 COMDAT 섹션을 링커가 병합.

### Q2. ODR이 무엇이고, 위반하면 어떻게 되나요? inline 함수는 왜 헤더에 정의해도 되나요?

**답**: 프로그램 전역에서 엔티티의 정의는 하나여야 한다는 규칙입니다. inline/템플릿/클래스는 여러 TU에 정의가 있어도 되지만 **모든 정의가 완전히 동일**해야 하고, 다르면 진단 의무 없는 UB — 링커가 아무 정의나 골라 씁니다.
**원리**: inline의 현대적 의미는 "인라인 힌트"가 아니라 "ODR 완화 마커"입니다. 링커가 COMDAT으로 하나만 남깁니다.
**프로젝트 연결**: 매크로 상태에 따라 헤더 전개가 달라지는 게 전형적 위반 경로라, Winters의 디버그 게이트 매크로(`WINTERS_ENABLE_NON_AI_DEBUG_STRING`)는 `Engine_Defines.h`에서 `#ifndef` 기본값을 고정하고 프로젝트 단위로만 재정의합니다.
**함정**: "static 전역 함수 두 TU에 같은 이름으로 정의하면?" → internal linkage라 서로 다른 엔티티, ODR 무관.

### Q3. 전방선언은 언제 쓰고, 어떤 제약이 있나요?

**답**: 헤더에서 그 타입의 포인터/참조/함수 시그니처만 필요할 때 `class Foo;`로 선언해 include를 끊습니다. 컴파일 의존성이 줄어 헤더 수정 시 재빌드 파급이 작아집니다. 불완전 타입이므로 sizeof, 멤버 접근, 값 멤버, 상속, 그리고 **delete**가 안 됩니다.
**심화**: 불완전 타입 delete는 소멸자가 non-trivial이면 UB인데, `unique_ptr`의 `default_delete`는 `static_assert(sizeof(T) > 0)`로 컴파일 에러로 승격시킵니다. 그래서 pimpl에서 `unique_ptr<Impl>` 멤버를 쓰면 소멸자를 cpp에 `= default`로라도 out-of-line 정의해야 합니다.
**프로젝트 연결**: `MCTSSystem.h`가 `class CWorld;` 전방선언으로 World 정의를 끌고 오지 않는 것이 실례입니다.

### Q4. 가상 함수 호출은 내부적으로 어떻게 동작하고, 비용은 무엇인가요?

**답**: 가상 함수가 있는 클래스는 클래스당 vtable 하나, 객체당 vptr 하나(보통 첫 8바이트)를 갖습니다. 호출은 vptr 로드 → vtable 슬롯 로드 → 간접 call의 3단계입니다.
**비용의 우선순위**: 간접 분기 자체보다 (1) **인라이닝 차단**이 가장 큽니다 — 인라인이 막히면 후속 최적화 전체가 막힙니다. (2) 콜드 객체면 vptr/vtable 로드가 캐시 미스 2회. (3) 호출 대상이 매번 바뀌면 분기 예측 실패.
**프로젝트 연결**: Winters ECS는 가상 호출을 시스템 수준(`ISystem::Execute`, 프레임당 수십 회)에만 두고, 수천 엔티티의 갱신은 컴포넌트 배열 순회로 처리해 per-entity 가상 호출을 설계에서 제거했습니다.
**꼬리질문 대비**: "생성자에서 가상 함수 부르면?" → vptr이 현재 생성 중인 클래스 vtable을 가리켜 파생 오버라이드로 안 감, 순수 가상이면 crash. "컴파일러가 최적화 못 하나?" → 타입 증명 시 devirtualization, `final`이 도움.

### Q5. 가상 소멸자는 왜 필요한가요? 없으면 정확히 무슨 일이 벌어지나요?

**답**: base 포인터로 파생 객체를 delete할 때 소멸자가 virtual이 아니면 표준상 UB입니다. 실제로는 base 소멸자만 실행돼 파생부 멤버(unique_ptr 등)의 소멸자가 안 불려 누수되고, 다중 상속이면 delete에 전달되는 주소 자체가 어긋나 힙이 깨질 수 있습니다.
**규칙**: 다형적 base는 `virtual ~Base()`, 또는 base 포인터 delete를 금지하려면 protected non-virtual 소멸자.
**프로젝트 연결**: `EngineSDK/inc/AI/BehaviorTree.h`의 `virtual ~CBTNode() = default` — BT 트리가 `CBTNode*` 기반으로 파생 노드들을 소유하므로, 이게 없으면 트리 해제 때 데코레이터의 상태들이 부분 파괴됩니다.
**꼬리질문 대비**: "그럼 모든 클래스에 가상 소멸자를 넣으면?" → vptr 8바이트 + POD성 상실(trivially destructible 아님 → 최적화/memcpy 계열 불가)이라 다형적으로 쓸 클래스에만.

### Q6. 다중 상속에서 객체 레이아웃과 this 포인터는 어떻게 되나요?

**답**: base 서브객체들이 순서대로 배치되고 두 번째 base부터는 오프셋 위치에 있습니다. `B* pb = pd;` 업캐스트는 실제로 this에 오프셋을 더하는 연산이고, B 경로로 호출된 오버라이드는 this를 되돌리는 thunk를 거칩니다. base마다 가상 함수가 있으면 vptr도 여러 개입니다.
**함정**: 이 오프셋 조정은 `static_cast`/암묵 변환만 안다 — `reinterpret_cast`로 base를 오가면 즉시 UB. 게임에서 다중 상속을 "데이터 없는 인터페이스 구현"까지만 허용하는 이유입니다.
**꼬리질문**: "가상 상속은?" → 공유 base의 오프셋이 최파생 타입에 따라 달라져 vtable에서 런타임 조회, 비용 때문에 게임 코드에선 사실상 금기, 데이터 다이아몬드는 컴포지션(ECS)으로 해소.

### Q7. RAII를 설명하고, 예외 안전 3단계 보장을 말해 보세요.

**답**: 리소스 수명을 객체 수명에 묶어 생성자에서 획득, 소멸자에서 해제하는 관용구입니다. 조기 return이든 예외 스택 되감기든 스코프를 벗어나는 모든 경로에서 해제가 보장됩니다. 예외 안전은 basic(누수 없음 + 불변식 유지), strong(실패 시 원상 복구 — copy-and-swap으로 달성), nothrow(절대 안 던짐 — 소멸자/swap/move)의 3단계입니다.
**프로젝트 연결(자기비판형)**: 제 `CHttpClient::DoRequestWith`는 WinHTTP 핸들 3개를 raw로 들고 실패 경로마다 수동 close를 반복합니다. 예외 없는 현재는 동작하지만 RAII 핸들 래퍼로 바꾸면 모든 조기 return에서 자동 해제되고 코드도 짧아집니다 — 지금 고치고 싶은 1순위입니다.
**꼬리질문**: "소멸자에서 예외 던지면?" → 되감기 중이면 std::terminate, C++11부터 소멸자는 암묵 noexcept. 실패 가능한 정리는 명시적 Close()로 분리.

### Q8. unique_ptr은 왜 '제로 오버헤드'라고 하나요? 항상 그런가요?

**답**: 기본 deleter가 상태 없는 empty class라 compressed pair(EBO)로 크기에서 사라져 `sizeof == 포인터 1개`, 소멸은 인라인된 delete 호출이라 raw 포인터 대비 런타임 추가 비용이 0입니다.
**예외**: 상태 있는 deleter를 주면 커집니다. 특히 함수 포인터 deleter(`unique_ptr<T, void(*)(T*)>`)는 16바이트 + 간접 호출 — 캡처 없는 람다 타입으로 주면 다시 8바이트입니다.
**프로젝트 연결**: Winters 팩토리 컨벤션이 `static unique_ptr<T> Create()` + private 생성자입니다(`CHttpClient`, `CMCTSSystem`). 소유는 unique_ptr, 비소유는 raw 포인터로 시그니처만 봐도 소유권이 읽히게 했습니다.
**꼬리질문**: "unique_ptr을 함수에 어떻게 넘기나?" → 소유권 이전이면 값 + std::move, 참조만 필요하면 `T&`/`T*`를 넘긴다(스마트 포인터 자체를 참조로 넘기는 건 재할당할 때만).

### Q9. shared_ptr의 내부 구조(control block)를 설명해 주세요. make_shared는 왜 권장되나요?

**답**: shared_ptr은 객체 포인터 + control block 포인터 2개로 16바이트입니다. control block에는 atomic strong/weak count, deleter, allocator가 있습니다. `new` 후 shared_ptr 생성은 객체/블록 2회 할당이지만 `make_shared`는 한 덩어리로 1회 할당 — 속도, 캐시 지역성, 예외 안전에서 이깁니다.
**트레이드오프**: make_shared는 객체와 블록이 한 덩어리라 **weak_ptr이 살아있는 한 객체 메모리가 회수되지 않습니다**(소멸자는 strong==0에 실행되지만 storage는 weak==0까지 유지). 큰 객체 + 장수 weak면 분리 할당이 낫습니다.
**꼬리질문**: "커스텀 deleter는 어디 저장되나?" → control block (그래서 `shared_ptr<T>`는 deleter가 타입에 안 박히고, unique_ptr은 박힘). "aliasing constructor?" → 소유는 A의 블록, 가리키는 건 A의 멤버 — 멤버 수명을 부모에 묶는 용도.

### Q10. shared_ptr은 thread-safe한가요? refcount의 비용은?

**답**: 정확히는 **control block의 카운트 증감만** atomic입니다. 서로 다른 shared_ptr 사본을 각 스레드가 쓰는 건 안전하지만, 같은 shared_ptr 객체 하나를 동시 수정하면 race고, 가리키는 객체 접근은 당연히 별도 동기화가 필요합니다.
**비용**: 증감은 lock-prefix RMW라 단일 스레드에서도 일반 증감보다 비싸고, 여러 코어가 같은 블록을 만지면 캐시 라인 핑퐁으로 수십 배 나빠집니다. 그래서 hot loop에서 shared_ptr 값 복사는 금물, `const&`나 raw로 전달합니다.
**프로젝트 연결**: Winters에서 프레임 hot path(수천 엔티티)는 entity id/raw 참조만 돌고, shared_ptr은 등장 자체가 드뭅니다 — 서버 권위 시뮬레이션은 수명이 틱과 스냅샷으로 결정되지 refcount로 결정되지 않는다고 설계 차원에서 답합니다.

### Q11. weak_ptr은 언제 쓰고, 순환 참조는 어떻게 끊나요?

**답**: 수명에 관여하지 않는 관찰자입니다. `lock()`이 "살아있으면 shared_ptr 획득, 죽었으면 nullptr"을 **원자적으로** 수행해 체크-후-사용 race가 없습니다. A↔B가 서로 shared로 잡으면 카운트가 0이 못 돼 누수 — 소유 방향(부모→자식)은 shared, 역방향은 weak로 끊습니다.
**게임 사례**: 버프가 시전자를, 시전자가 버프 목록을 서로 shared로 잡는 패턴이 전형적 누수. 타겟팅 시스템이 "죽었을 수 있는 대상"을 참조할 때 weak_ptr 또는 (Winters처럼) **세대 카운터 붙은 entity 핸들**로 stale 참조를 검출합니다 — 핸들 방식이 refcount 비용 없이 같은 문제를 푸는 대안이라고 비교하면 좋습니다.

### Q12. enable_shared_from_this는 어떤 원리로 동작하나요?

**답**: 멤버 함수에서 `shared_ptr<T>(this)`를 만들면 새 control block이 생겨 이중 delete가 됩니다. `enable_shared_from_this<T>`는 base에 숨은 `weak_ptr<T>`를 두고, 최초 shared_ptr 생성 시 control block이 그 weak를 세팅합니다. `shared_from_this()`는 그 weak를 lock해 기존 블록을 공유합니다.
**함정**: shared_ptr이 아직 소유하지 않은 시점(생성자 포함)에 부르면 C++17부터 `bad_weak_ptr` throw. 그래서 "생성 직후 초기화 훅"은 팩토리에서 shared_ptr을 만든 뒤에 불러야 합니다.
**프로젝트 연결**: 비동기 콜백의 this 수명 문제를 저는 CHttpClient에서 "소멸자가 모든 pending future를 wait하는 계약"으로 풀었는데, shared 소유 구조였다면 `weak_from_this` 캡처 + lock 실패 시 스킵이 대안이었을 것 — 두 설계의 트레이드오프(블로킹 소멸 vs refcount 비용/소유 구조 변경)를 비교해 답합니다.

### Q13. std::move는 실제로 무엇을 하나요?

**답**: 아무것도 이동시키지 않습니다. `static_cast<T&&>`로 xvalue로 캐스팅해 "이동해도 된다"는 자격만 부여하고, 실제 이동은 그걸 받는 move 생성자/대입이 합니다. move 생성자가 없는 타입이면 오버로드 해석이 `const T&`로 폴백해 **조용히 복사**됩니다.
**moved-from 상태**: valid but unspecified — 소멸/재대입은 안전, 값 읽기는 금물.
**프로젝트 연결**: `std::future`는 move-only라 `m_PendingRequests.push_back(std::move(task))`가 유일한 소유권 이전 경로입니다 — move가 최적화가 아니라 **소유권 의미론**인 실례.
**꼬리질문**: "const 객체에 std::move 걸면?" → `const T&&`가 되어 move 생성자에 매칭 안 되고 복사 — const 지역 변수 반환의 조용한 함정.

### Q14. universal reference(forwarding reference)와 perfect forwarding을 설명해 주세요.

**답**: 템플릿 파라미터 `T&&`(및 auto&&)에서만 성립합니다. lvalue를 넘기면 `T = U&`로 추론되고 reference collapsing(`& + && = &`)으로 `U&`, rvalue면 `U&&`가 됩니다. `std::forward<T>`가 이 추론 정보를 사용해 lvalue성/rvalue성을 보존한 채 다음 함수로 넘깁니다 — `emplace_back`, `make_unique`가 인자를 복사 없이 생성자에 꽂는 원리입니다.
**함정**: 구체 타입의 `Foo&&`는 그냥 rvalue 참조입니다. 또 forwarding reference 오버로드는 거의 모든 것에 매칭돼 복사 생성자보다 우선 선택되는 사고가 있어, 제약(concepts/enable_if) 없이 열어두면 위험합니다.
**꼬리질문**: "std::move와 std::forward 차이?" → move는 무조건 xvalue 캐스팅, forward는 추론된 카테고리 조건부 캐스팅.

### Q15. RVO/NRVO를 설명하고, `return std::move(local)`이 왜 안티패턴인지 말해 보세요.

**답**: RVO는 `return T{...}` 같은 prvalue 반환을 호출자 슬롯에 직접 생성하는 것으로 C++17부터 **언어 보장**(복사/이동 생성자가 delete여도 됨)입니다. NRVO는 이름 있는 지역 변수 반환의 elision으로 허용이지 보장은 아니지만, 실패해도 지역 변수는 자동으로 move 취급됩니다. `return std::move(local)`은 반환식을 xvalue로 바꿔 **NRVO 자격을 박탈**하고 move를 강제 — 0회를 1회로 만드는 손해입니다.
**프로젝트 연결**: `CHttpClient::MakeRequestSnapshot`이 지역 구조체를 그냥 `return snapshot;`으로 반환합니다 — NRVO 대상이고, move를 붙일 이유가 없는 전형입니다.
**꼬리질문**: "vector 재할당과 noexcept move 관계?" → `move_if_noexcept`: move가 noexcept 아니면 강한 보장 위해 복사 폴백. move 생성자에 noexcept 빠뜨리면 vector 성장이 복사가 된다.

### Q16. 템플릿은 어떻게 컴파일되나요? 왜 정의가 헤더에 있어야 하나요?

**답**: 템플릿은 사용 시점에 그 타입 인자로 인스턴스화되는 코드 생성기라, 인스턴스화하는 TU가 정의 전체를 봐야 합니다. 각 TU가 만든 중복 인스턴스는 링커가 COMDAT으로 병합합니다. 컴파일 시간이 문제면 `extern template`으로 인스턴스화를 한 TU에 몰 수 있습니다.
**심화**: 2단계 이름 검색 — 비의존 이름은 정의 시점, 의존 이름은 인스턴스화 시점 검사라 `typename`/`template` 키워드가 필요해집니다.
**게임 맥락**: 타입별 코드 복제(code bloat)가 빌드 시간/바이너리 크기를 키우므로, 타입 무관 로직을 non-template 함수로 빼는 습관까지 말하면 실무형 답변이 됩니다.

### Q17. SFINAE가 뭐고, concepts는 무엇을 개선했나요?

**답**: 템플릿 인자 치환 실패는 컴파일 에러가 아니라 그 후보를 오버로드 집합에서 제거한다는 규칙이고, `enable_if`로 "조건 만족 시에만 존재하는 오버로드"를 만들던 기법입니다. 문제는 의도가 표현식 트릭에 숨고 에러 메시지가 해독 불가라는 것. C++20 concepts는 같은 것을 `requires std::integral<T>`처럼 선언적으로 쓰고, 위반 시 "어느 요구를 못 채웠는지"를 컴파일러가 직접 말해주며, 제약 포함 관계로 오버로드 우선순위까지 정리합니다.
**한 줄 요약**: "라이브러리 트릭이던 오버로드 제어를 언어 1급 기능으로 승격시킨 것"까지 말하면 이해도가 전달됩니다.

### Q18. CRTP를 설명하고, 가상 함수 대신 언제 쓰는지 말해 보세요.

**답**: base가 파생 타입을 템플릿 인자로 받아(`class D : Base<D>`) `static_cast<D*>(this)`로 파생 구현을 호출하는 패턴 — 컴파일 타임 디스패치라 vtable/vptr이 없고 인라인이 됩니다.
**트레이드오프**: 타입별 코드 복제, 그리고 공통 base 타입이 없어 서로 다른 파생을 한 컨테이너에 못 담습니다. 즉 "런타임에 타입이 섞이는가"가 선택 기준입니다.
**프로젝트 연결**: Winters에서 시스템 레지스트리는 런타임에 이종 시스템을 한 목록으로 돌려야 하고 호출 빈도도 프레임당 수십 회뿐이라 `ISystem` 가상 인터페이스로 충분했습니다. per-entity hot path는 CRTP조차 안 쓰고 다형성 자체를 제거(컴포넌트 배열 순회)했습니다 — 도구를 아는 것보다 "여기엔 필요 없었다"는 판단을 보여주는 답.

### Q19. vector의 성장 전략과 iterator 무효화 규칙을 설명해 주세요.

**답**: begin/end/capacity 포인터 3개짜리 연속 배열이고, capacity 초과 시 기하급수 재할당(MSVC 1.5배, libc++/libstdc++ 2배)으로 push_back이 분할상환 O(1)입니다. 재할당이 일어나면 iterator/포인터/참조 **전부** 무효, 재할당 없어도 insert/erase 지점 이후는 무효입니다.
**실무 규칙**: 프레임 중 스파이크가 안 되는 곳은 reserve, 순회 중 삭제는 `it = erase(it)`, erase-remove(또는 C++20 `std::erase_if`) 관용구.
**프로젝트 연결**: `CHttpClient::PruneCompletedRequests`가 완료된 future를 걷어낼 때 역순 인덱스 루프로 erase해 시프트로 인한 인덱스 무효화를 회피합니다.
**꼬리질문**: "왜 1.5배?" → 성장 계수가 2 미만이면 이전에 해제한 블록들의 합이 다음 요청보다 커질 수 있어 메모리 재사용 가능성이 생김(2배는 항상 불가).

### Q20. unordered_map의 내부 구조와 rehash 시 무효화를 설명해 주세요.

**답**: 버킷 배열 + 체이닝(노드 연결)입니다. load factor가 max(기본 1.0)를 넘으면 버킷을 늘리고 전 노드를 재배치(rehash)합니다. 이때 **iterator는 전부 무효지만 노드는 재할당되지 않으므로 요소 포인터/참조는 유지**됩니다 — vector와 반대 방향의 규칙이라 정확히 말하면 가점.
**비용 감각**: 해시 계산 + 버킷→노드 포인터 점프의 캐시 미스 때문에, 수천 개 이하 hot path에서는 정렬 vector + 이진 탐색이나 open-addressing이 이기는 경우가 많습니다. 문자열 키는 해시 비용이 지배적이라 게임에서는 로드 타임에 정수 핸들로 구워둡니다.
**프로젝트 연결**: 챔피언/FX 큐 이름 → id 해석을 로드 시 1회로 끝내고 런타임은 정수 인덱스로 도는 구조가 이 원칙의 적용입니다.

### Q21. const correctness와 mutable, constexpr의 관계를 설명해 주세요.

**답**: const 멤버 함수는 this를 `const T*`로 만들어 "관측 가능한 상태를 안 바꾼다"는 계약입니다. 캐시·뮤텍스 같은 관측 불가 상태는 mutable로 예외를 허용하는 **논리적 const**로 운용합니다. C++11부터 표준 라이브러리는 const == 스레드 동시 읽기 안전을 전제하므로 mutable 멤버는 자체 동기화가 필요합니다. constexpr은 "컴파일 타임 평가 가능"이고 consteval은 강제, constinit은 정적 초기화만 보장(const 아님)입니다.
**프로젝트 연결**: `CMCTSSystem`의 `TICK_INTERVAL`/`ITERATIONS`가 `static constexpr` — 매크로 대신 타입 있는 상수로 헤더에 두고 ODR 문제도 없습니다(implicit inline). `MakeRequestSnapshot() const`는 워커 스레드가 읽기 전용 스냅샷만 뜨도록 시그니처로 강제한 사례입니다.

### Q22. C++ 캐스팅 4종을 비교하고, C 스타일 캐스팅이 왜 위험한지 설명해 주세요.

**답**: static_cast(관련 타입 간, 검사 없는 down-cast, MI 오프셋 조정함) / dynamic_cast(RTTI 기반 검사, 실패 시 nullptr/throw, 비용 큼) / const_cast(cv 제거 — 원본이 진짜 const면 수정 UB) / reinterpret_cast(비트 재해석 — aliasing/정렬 위반 UB, 오프셋 조정 안 함). C 스타일은 이 넷을 순차 시도하는 것과 같아 의도가 안 남고 const_cast+reinterpret 조합까지 조용히 수행하며 grep도 안 됩니다.
**게임 관례**: dynamic_cast는 계층 순회 + (DLL 경계에선) type_info 문자열 비교까지 있어 hot path 금지 — 자체 type id + static_cast, 또는 ECS처럼 "타입 질의 = 컴포넌트 보유 질의"로 설계에서 제거합니다.

### Q23. UB(미정의 동작)가 왜 위험한지, signed overflow와 strict aliasing 사례로 설명해 주세요.

**답**: UB는 "그 지점에서 크래시"가 아니라 프로그램 전체 의미의 소멸입니다. 컴파일러는 UB가 없다는 전제로 최적화하므로 증상이 시공간적으로 멀리서 나타납니다. signed overflow: `x + 1 < x` 같은 사후 검사는 "오버플로 없음" 전제로 **제거**됩니다 — 검사는 연산 전에(`x > INT_MAX - y`). strict aliasing: `*(int*)&f`는 다른 타입 포인터는 같은 메모리를 안 가리킨다는 전제를 깨서 read/write 재배열 대상이 됩니다 — memcpy나 C++20 bit_cast가 합법 경로입니다.
**프로젝트 연결**: 제일 아픈 UB는 댕글링이었습니다 — 비동기 람다의 raw this 캡처(Q24에서 상술)와, FX 개발 중 캐스트 프레임 재발동에서 잡은 댕글링. "UB는 디버거보다 계약(수명 규칙)으로 잡는다"가 배운 교훈입니다.

### Q24. std::async가 반환한 future를 버리면 어떻게 되나요? (실전 사례가 있다면)

**답**: `std::async(launch::async, ...)`가 반환하는 future의 공유 상태는 특별해서, **소멸자가 태스크 완료까지 블로킹**합니다. 반환값을 안 받으면 임시 future가 그 자리에서 파괴되며 대기 — "비동기 호출"이 사실상 동기가 됩니다.
**실전**: Winters `CHttpClient`가 정확히 이 버그였습니다. AsyncGet/AsyncPost가 future를 버려서 매 호출이 블로킹이었고, 아이러니하게 그 블로킹 덕에 람다의 raw this 캡처가 우연히 안전했습니다. 수정 시 두 문제를 한 몸으로 다뤄야 했습니다: `m_PendingRequests`(vector<future<void>>)가 future 수명을 소유하고, 소멸자가 전부 drain하며(`CHttpClient.cpp:103-117`), 완료분은 `wait_for(0s) == ready`로 주기적으로 걷어냅니다. 이 패턴은 `.claude/gotchas.md`(2026-07-09 Async lifetime)에 재발 방지 규칙으로 박제했습니다.
**꼬리질문 대비**: "그럼 애초에 std::async가 적합했나?" → 요청 빈도가 낮은 HTTP 백엔드 호출이라 충분하지만, 고빈도라면 스레드 생성 비용 때문에 스레드 풀/IOCP로 가야 한다고 한계까지 답합니다. 콜백은 `ProcessCallbacks()`로 메인 스레드에서만 실행해 게임 로직과의 race를 큐 경계에서 끊었습니다.

### Q25. DLL export한 클래스에 unique_ptr 멤버를 두면 어떤 문제가 생기나요?

**답**: 일반 클래스는 암묵 복사 생성자를 **사용될 때만** 인스턴스화하지만, `__declspec(dllexport)` 클래스는 **모든 멤버 함수를 즉시 인스턴스화해 export**합니다. 그래서 아무도 복사하지 않아도 컴파일러가 암묵 복사 생성자를 만들다 unique_ptr(복사 불가) 멤버에서 C2280으로 실패합니다. 처방은 복사 생성자/대입 명시 `= delete`.
**실전**: Winters에서 직접 밟고 gotcha로 박제한 규칙입니다(2026-04-23: "WINTERS_ENGINE dllexport + unique_ptr 멤버 = copy ctor/assign 명시 delete 필수"). 현재 모든 해당 클래스가 이 패턴입니다 — `EngineSDK/inc/ECS/Systems/MCTSSystem.h`: `class WINTERS_ENGINE CMCTSSystem`이 `unique_ptr<CMCTSPlanner>` 멤버를 갖고 복사 2종을 delete.
**꼬리질문**: "C4251 경고는?" → STL 멤버의 dll-interface 부재 경고. Winters는 Engine/Client 동일 툴체인·동일 구성 빌드를 전제로 `#pragma warning(disable:4251)`(`Engine_Defines.h:30`) — 전제가 깨지는 외부 배포 SDK라면 pimpl이나 C ABI로 내려야 한다고 조건부로 답합니다.

### Q26. DLL 경계에서 CRT 힙 문제란 무엇인가요?

**답**: DLL과 EXE가 서로 다른 CRT(정적 /MT, 또는 버전 다른 /MD)를 쓰면 각자 힙을 가져서, 한 모듈이 new한 메모리를 다른 모듈이 delete하면 힙 손상입니다. 원칙은 "할당한 모듈이 해제한다" — COM의 Release(객체가 자기 모듈에서 자기를 delete)가 고전적 해법이고, 동일 동적 CRT로 통일하면 완화됩니다.
**프로젝트 연결**: Winters는 Engine/Client를 같은 VS 버전·같은 구성으로 빌드하는 전제 위에 `Create() → unique_ptr` 팩토리를 씁니다. Debug 빌드는 `_CRTDBG_MAP_ALLOC` + `#define new DBG_NEW`(`Engine_Defines.h:32-42`)로 CRT 디버그 힙의 누수 추적을 켜두는데, 이 `#define new`가 placement new와 충돌하는 부작용까지 알고 include 순서로 관리합니다.
**꼬리질문**: "Debug DLL + Release EXE 섞으면?" → CRT뿐 아니라 `_ITERATOR_DEBUG_LEVEL`이 달라 STL 레이아웃 자체가 다름 — 링커가 iterator debug level mismatch로 거부하는 게 오히려 다행인 케이스.

### Q27. 헤더에 using namespace std를 쓰면 왜 안 되나요?

**답**: 헤더의 using은 그 헤더를 포함하는 모든 TU의 네임스페이스를 오염시킵니다. 이름 충돌 컴파일 에러는 차라리 낫고, 진짜 위험은 ADL과 결합해 **오버로드 해석이 조용히 바뀌는** 것입니다(예: 사용자 `swap` vs `std::swap`). cpp 파일이나 함수 스코프 using은 영향 범위가 닫혀 있어 허용됩니다.
**프로젝트 연결**: Winters gotcha(2026-05-14)로 명문화된 규칙입니다 — "public 헤더는 std:: 한정, using은 cpp/블록 스코프만". 정직하게는 레거시 `Engine_Defines.h`에 `using namespace std;`가 남아 있고, 신규 헤더(`Pathfinder.h`의 `std::vector<...>`)부터 규약을 강제하며 레거시 제거는 경계 리팩터링 슬라이스로 계획 중입니다 — 대규모 코드베이스에서 규약을 "신규부터 적용 + 레거시 계획 제거"로 운용한 경험 자체가 어필 포인트입니다.

### Q28. 게임에서 예외를 끄는 관례가 있는데, 이유와 대안을 설명해 주세요.

**답**: x64 테이블 기반 EH는 안 던지는 경로 런타임 비용이 거의 0인데도 끄는 이유는 (1) unwind 테이블/랜딩 패드로 인한 바이너리 크기, (2) "던질 수 있다" 가정이 만드는 인라인/코드 이동 최적화 제약, (3) 콘솔 툴체인 역사와 코드베이스 관례, (4) 프레임 루프 중간의 스택 되감기 복구 모델이 게임 구조와 안 맞음입니다. 대안은 Result/에러코드 + out 파라미터이며, 이때 **silent fail 방지가 새 과제**가 됩니다.
**프로젝트 연결**: 미니언이 조용히 멈추던 사고의 원인이 "빈 경로 vector 하나로 4가지 실패가 뭉개진 것"이었고, `Pathfinder.h`의 `ePathFindResult { Success, NullGrid, StartBlocked, GoalBlocked, NoRoute, BrokenPath }`로 실패 원인을 타입으로 구분하게 고쳤습니다. 예외를 안 쓰면 실패를 **값으로 승격**해야 한다는 걸 사고로 배웠습니다. 네트워크 경계도 동일 원칙 — FlatBuffers verify 실패를 bare return으로 버리면 스키마 드리프트가 네트워크 정지처럼 보이므로 bounded trace/카운터를 강제합니다(gotchas 2026-07-09).
**꼬리질문**: "예외를 꺼도 STL은?" → bad_alloc 경로 전제라 할당 실패 정책(즉시 크래시 등)까지 세트로 정해야 하고, "끈 것"과 "설계상 안 쓰는 것"은 구분해야 합니다(Winters는 후자).

### Q29. RTTI를 끄는 이유와 dynamic_cast의 실제 비용을 설명해 주세요.

**답**: RTTI는 클래스마다 type_info 메타데이터를 만들고, dynamic_cast는 vtable에서 type_info를 얻어 상속 그래프를 순회합니다. 깊은 계층/cross-cast는 수백 사이클이고, DLL 경계에서는 type_info 포인터가 모듈마다 달라 **이름 문자열 비교**로 폴백해 더 느립니다. 끄면 바이너리가 줄고 hot path의 안전망 오용을 원천 차단합니다.
**대안**: enum/해시 기반 자체 type id + static_cast, 방문자 패턴, 또는 ECS처럼 타입 질의를 컴포넌트 보유 질의로 대체 — Winters는 마지막 방식이라 down-cast 수요 자체가 거의 없습니다.

### Q30. 커스텀 할당자나 메모리 풀은 언제, 왜 도입하나요?

**답**: 범용 할당자의 비용(동기화, 빈 탐색, 단편화, 예측 불가 스파이크)이 프로파일에 잡힐 때입니다. 게임은 할당 패턴이 규칙적이라 특화가 잘 먹힙니다 — 프레임 수명 데이터는 linear/arena allocator(할당 = 포인터 증가, 해제 = 프레임 끝 리셋), 스폰/디스폰 잦은 동종 객체(투사체, FX, 미니언)는 object pool(free-list 재사용, 연속 배치로 순회 캐시 이득), 컨테이너 단위 교체는 C++17 pmr(`monotonic_buffer_resource`)이 표준 경로입니다.
**프로젝트 연결**: Winters는 ECS 컴포넌트 배열이 "동종 데이터 연속 배치"라는 풀의 이점을 구조로 선취했고, 자체 Profiler로 프레임 17.8ms→9ms를 잡아본 경험 위에서 "할당이 병목으로 **측정될 때** 미니언/FX 스폰부터 풀링한다"는 순서로 답합니다 — 측정 없는 최적화를 안 한다는 태도까지가 답변입니다.
**꼬리질문**: "allocator가 컨테이너 타입에 박히는 문제는?" → 고전 allocator는 `vector<T, MyAlloc>`으로 타입이 갈라짐, pmr은 런타임 다형 리소스로 해소(대신 간접 호출 비용).

### Q31. 멀티스레드 환경에서 공유 상태를 어떻게 보호했는지, 설계 관점에서 말해 보세요.

**답**: 락을 거는 것보다 **공유 자체를 줄이는** 설계가 먼저입니다. (1) 불변 스냅샷: 워커가 멤버를 직접 읽는 대신 호출 시점 복사본만 받게 하고, 함수를 static으로 만들어 컴파일러가 this 접근을 금지하도록 강제 — `CHttpClient::RequestSnapshot` + `static DoRequestWith`(`CHttpClient.h:43-57`)가 `SetAuthToken`과의 race를 이 방식으로 차단합니다. (2) 큐 경계: 워커 결과를 콜백 큐에 넣고 메인 스레드의 `ProcessCallbacks()`만 실행 — 게임 로직과의 race를 한 지점에서 끊습니다. (3) 락 범위 최소화: 소멸자에서 `swap`으로 pending을 로컬로 빼낸 뒤 락 밖에서 wait(`CHttpClient.cpp:107-116`) — 락 잡은 채 블로킹하지 않기.
**꼬리질문**: "shared_ptr로 풀 수도 있지 않나?" → 가능하지만 소유 구조가 바뀌고 refcount 비용이 생김. 여기선 "소멸 시점에만 블로킹"이라는 계약이 더 단순했다고 트레이드오프로 답합니다.

### Q32. Engine을 DLL로 나눈 이유와, 그 경계에서 겪은 문제를 종합해 말해 보세요.

**답**: (1) Client/Server/Tools가 같은 엔진 바이너리를 공유하고, (2) 링크 단위가 곧 아키텍처 경계가 되어 의존 규칙(Client→Engine 단방향)이 물리적으로 강제되며, (3) 엔진만 수정 시 재링크 범위가 줄기 때문입니다. 경계에서 겪은 실전 문제 3종 세트: dllexport 클래스의 암묵 특수 멤버 전체 인스턴스화(→ unique_ptr 멤버 시 복사 delete 필수), C4251/STL 레이아웃(→ 동일 툴체인 전제 명시 후 경고 억제), CRT 힙(→ 할당 모듈이 해제, Debug/Release 혼합 금지). 그리고 include 경로 차원의 경계 강제 — EngineSDK/inc가 비-Engine 프로젝트 include path에 오르면 계층 규칙이 컴파일로 강제 불가능해진다는 것도 gotcha(2026-07-09 Dependency boundary)로 관리하며, Shared의 직접 ECS include는 어댑터(`Shared/GameSim/Core/Ecs/*`)와 검사 스크립트(Check-SharedBoundary.ps1)로 빌드에서 차단합니다.
**요지**: "경계는 문서가 아니라 컴파일/링크가 지키게 한다" — 이 문장이 이 답변의 핵심입니다.

### Q33. 만 개의 게임 오브젝트를 매 프레임 업데이트해야 합니다. 상속+가상함수 구조와 ECS 구조를 비교해 보세요.

**답**: 상속 구조는 객체가 힙에 흩어지고(포인터 추격 = 캐시 미스), 개체마다 가상 호출(인라인 차단), 새 조합마다 계층 수정(다이아몬드 위험)이라는 3중 비용이 있습니다. ECS는 컴포넌트를 타입별 연속 배열로 두고 시스템이 배열을 순회 — 캐시 라인당 여러 개체 처리, 분기/가상 호출 제거, 조합은 컴포넌트 추가로 해결됩니다. 대가는 개체 단위 사고의 직관성 상실과 시스템 간 실행 순서 관리(Winters는 `GetPhase()`로 명시적 순서 부여 — `MCTSSystem.h:30`이 BT(8)/커스텀(9)와 분리된 phase 10을 갖는 식)입니다.
**심화**: 캐시 수치로 말하면 강합니다 — L1 히트 ~4사이클 vs 메인 메모리 ~200사이클, 64바이트 라인에 16바이트 컴포넌트면 4개가 한 번에 올라옴. "가상 함수가 느려서"가 아니라 "메모리 접근 패턴이 지배해서"가 정확한 프레이밍입니다.

### Q34. 본인 코드에서 가장 부끄러운 C++ 실수와 배운 점을 말해 보세요.

**답(예시 프레임)**: std::async의 future를 버려서 비동기 API가 전부 동기로 돌던 CHttpClient 사례를 고릅니다. 부끄러운 이유가 세 겹입니다 — (1) "async라고 썼으니 비동기겠지"라는 무검증 가정, (2) future 소멸자 블로킹이라는 표준의 특례를 몰랐던 것, (3) 그 블로킹 덕에 raw this 캡처 댕글링이 **가려져** 있었다는 것. 수정하면서 future 소유(vector 보관 + 소멸자 drain)와 this 수명 계약을 한 변경으로 묶었고, 재발 방지를 gotchas.md에 박제해 어시스턴트/팀원이 같은 지뢰를 못 밟게 했습니다. 배운 것: "우연히 안전한 코드는 리팩터링 한 번에 UB가 된다 — 안전은 계약으로 명시해야 한다."
**의도**: 이 질문은 실수 자체가 아니라 사후 처리(원인 계층 분석 → 재발 방지 시스템화)를 봅니다.

---

## 내 프로젝트 연결 포인트

면접 중 자연스럽게 꺼낼 수 있는 "증거 있는 문장"들:

- **DLL 경계를 이론이 아니라 사고로 배웠다**: "dllexport 클래스는 암묵 특수 멤버까지 전부 인스턴스화한다는 걸 unique_ptr 멤버 컴파일 에러로 배웠고, 이후 모든 export 클래스에 복사 delete를 규약화했습니다(`EngineSDK/inc/ECS/Systems/MCTSSystem.h`). C4251은 동일 툴체인 전제를 명시하고 억제했고, 그 전제가 깨지는 배포 시나리오의 대안(pimpl/C ABI)도 구분해 둡니다."
- **비동기 수명 버그의 전 과정**: "std::async future 버림 → 우연한 동기화 → raw this 캡처가 가려짐 → future 소유 + 소멸자 drain + 스냅샷 패턴으로 일괄 수정 → gotchas.md 박제(`Client/Private/Network/Backend/CHttpClient.cpp`). 버그 하나를 수명 계약 문제로 일반화한 경험입니다."
- **가상 함수 비용을 설계로 회피**: "Winters ECS는 가상 호출을 시스템 수준(프레임당 수십 회)에 못 박고 개체 수준(수천)은 컴포넌트 배열 순회입니다. '가상 함수를 아는가'가 아니라 '어디에 두는가'로 답할 수 있습니다."
- **예외 없는 실패 설계**: "빈 vector로 뭉개진 pathfinding 실패가 미니언 멈춤 사고가 된 뒤, `ePathFindResult` enum으로 실패 원인 6종을 타입으로 승격했습니다(`EngineSDK/inc/Manager/Navigation/Pathfinder.h`). 게임업계가 예외 대신 Result를 쓸 때 따라오는 silent-fail 문제의 실전 해법입니다."
- **규약을 시스템으로 강제**: "헤더 using 금지, 경계 include 금지 같은 규칙을 문서로 두지 않고 gotchas.md 자동 로딩 + 경계 검사 스크립트(Check-SharedBoundary.ps1)로 빌드가 지키게 했습니다. 사람이 아니라 파이프라인이 컨벤션을 지키는 구조를 선호합니다."
- **측정 우선 최적화**: "자체 Profiler로 프레임 17.8ms→9ms를 잡았습니다. 메모리 풀도 '게임이니까'가 아니라 할당이 프로파일에 찍히는 지점부터 도입한다는 순서로 말할 수 있습니다."
- **자기비판 카드**: "지금 고치고 싶은 코드는 CHttpClient의 raw WinHTTP 핸들 수동 해제입니다 — RAII 래퍼 4줄이면 실패 경로 3중 close가 사라집니다." (약점 질문을 준비된 개선안으로 전환)

---

## 마지막 점검 체크리스트

- [ ] 번역 단위 = 전처리 끝난 cpp 하나. 컴파일러는 TU 밖을 모른다 → 링커가 심볼 해석.
- [ ] ODR: inline/템플릿은 여러 TU 정의 허용, 단 전부 동일. 위반 = 진단 없는 UB.
- [ ] 전방선언: 포인터/참조/시그니처 OK. sizeof/멤버/delete 불가. unique_ptr<Impl> → 소멸자 out-of-line.
- [ ] 가상 호출 3단계: vptr 로드 → 슬롯 로드 → 간접 call. 최대 비용은 **인라인 차단**.
- [ ] 가상 소멸자 없이 base delete = UB. 다형적 base는 virtual ~ 또는 protected non-virtual.
- [ ] 생성자 안 가상 호출은 현재 클래스 것. 다중 상속 = vptr 여러 개 + this 조정 thunk. 가상 상속 = 오프셋 런타임 조회.
- [ ] 예외 안전: basic(누수 없음) / strong(copy-and-swap 롤백) / nothrow(소멸자·swap·move).
- [ ] unique_ptr = 8바이트(EBO), 함수 포인터 deleter면 16. shared_ptr = 포인터 2개 + control block(atomic strong/weak + deleter).
- [ ] make_shared: 1회 할당·지역성, 단 weak 생존 시 메모리 회수 지연. refcount = atomic RMW → hot loop 복사 금지.
- [ ] enable_shared_from_this = 숨은 weak_ptr, 미소유 상태 호출 시 bad_weak_ptr(C++17).
- [ ] std::move = static_cast<T&&>일 뿐. moved-from = valid but unspecified. const에 move = 조용한 복사.
- [ ] forwarding reference는 템플릿 T&&/auto&&만. collapsing: & 이기면 &. forward = 조건부 캐스팅.
- [ ] RVO(C++17 보장, prvalue) vs NRVO(허용). return std::move(local) = NRVO 박탈 안티패턴. move ctor에 noexcept 없으면 vector 재할당이 복사.
- [ ] vector: MSVC 1.5배 성장, 재할당 시 전부 무효. unordered_map: rehash 시 iterator 무효·참조 유지.
- [ ] constexpr(가능)/consteval(강제)/constinit(정적 초기화만). mutable = 논리적 const의 탈출구(스레드 주의).
- [ ] 캐스팅: static(조정함·검사없음)/dynamic(RTTI·느림)/const(원본 const 수정 UB)/reinterpret(조정 안 함). C 스타일 금지 이유 = 의도 은닉.
- [ ] UB 3대장: 댕글링(비동기 캡처 수명!), signed overflow(사후 검사 증발), strict aliasing(memcpy/bit_cast로).
- [ ] 11 move·람다·atomic / 14 make_unique·move 캡처 / 17 elision 보장·optional·if constexpr / 20 concepts·coroutine·span·bit_cast.
- [ ] dllexport 클래스 = 전 멤버 즉시 인스턴스화 → unique_ptr 멤버면 복사 = delete 필수 (내 gotcha, MCTSSystem.h).
- [ ] std::async future 버림 = 소멸자 블로킹 = 동기화 (내 gotcha, CHttpClient). 수정 = future 소유 + dtor drain + 스냅샷.
- [ ] CRT 힙: 할당한 모듈이 해제. Debug/Release·/MT 혼합 금지. C4251 = STL 레이아웃은 툴체인 종속.
- [ ] 헤더 using namespace 금지 — TU 오염 + ADL 오버로드 변조 (내 gotcha 2026-05-14).
- [ ] 게임 예외 off 이유: 크기·최적화 제약·관례·복구 모델 불일치 → Result 승격 + silent fail 대책(ePathFindResult).
- [ ] RTTI off: type_info 제거, dynamic_cast는 DLL 경계에서 문자열 비교. 대체 = 자체 type id/ECS 질의.
- [ ] 풀 3종: arena(프레임 리셋)/object pool(free-list·연속 배치)/pmr. 도입은 프로파일 근거로.
- [ ] 마지막 한 문장: "우연히 안전한 코드는 안전하지 않다 — 수명과 경계는 계약으로 명시하고, 계약은 빌드가 지키게 한다."
