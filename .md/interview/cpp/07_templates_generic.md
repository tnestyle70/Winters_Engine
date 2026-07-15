# 템플릿 · 제네릭 프로그래밍

> 대상: Winters 엔진(C++20 빌드·코드 스타일은 C++17 관용구 중심/DX11, LoL 스타일 클라+서버)을 직접 만든 게임 프로그래머 지망생.
> 목적: 템플릿을 "문법"이 아니라 "컴파일 모델"로 설명하고, 내 ECS/RHI 코드로 증명한다.

---

## ① 한 줄 본질

템플릿(template)은 **타입을 매개변수로 받는 컴파일타임 코드 생성기**다 — 소스에 쓴 것은 설계도일 뿐이고, 실제 함수/클래스는 사용된 타입마다 컴파일러가 **인스턴스화(instantiation)** 하는 순간 생겨난다. 이 모델 하나가 "왜 헤더에 정의를 두는가", "왜 DLL로 export 못 하는가", "왜 코드 팽창(code bloat)이 생기는가"를 전부 설명한다.

---

## ② 기본 개념

### 2.1 인스턴스화 모델 — 왜 정의가 헤더에 있어야 하나

일반 함수는 선언(.h)/정의(.cpp)를 분리해도 된다. 각 번역 단위(translation unit, TU)는 선언만 보고 호출 코드를 만들고, 링커(linker)가 정의가 컴파일된 오브젝트 파일의 심볼과 이어붙이기 때문이다.

템플릿은 다르다. `template<typename T> T& AddComponent(EntityID, const T&)`는 그 자체로 **기계어를 한 줄도 만들지 않는다.** `AddComponent<TransformComponent>(e)`처럼 구체 타입으로 **쓰이는 시점**에, 그 TU 안에서 컴파일러가 `T`를 치환해 실제 함수를 찍어낸다(암시적 인스턴스화). 따라서:

- 인스턴스화하는 TU는 **템플릿의 전체 정의**를 볼 수 있어야 한다. 정의를 `.cpp`에 숨기면 다른 TU는 인스턴스화를 못 하고, 링크 단계에서 `unresolved external symbol`이 난다. 헤더에 두는 건 스타일이 아니라 **컴파일-링크 모델의 강제**다.
- 같은 `Store<int>`가 여러 TU에서 중복 인스턴스화돼도 ODR(One Definition Rule) 위반이 아니다 — 템플릿 인스턴스는 인라인 함수처럼 취급되고, 링커가 COMDAT 병합으로 하나만 남긴다.
- 예외: **명시적 인스턴스화(explicit instantiation)** `template class Store<int>;`를 `.cpp`에 두면 정의를 숨길 수 있지만, 쓸 타입을 전부 미리 못박아야 한다. 임의의 `T`에 열린 API에는 불가능.

### 2.2 함수 템플릿 vs 클래스 템플릿

- **함수 템플릿**: 호출 인자에서 타입을 **추론(deduction)** 하므로 보통 `<>`를 생략한다. `Safe_Delete(m_pBuffer)`처럼.
- **클래스 템플릿**: 멤버 함수는 **실제로 호출될 때만** 인스턴스화된다(lazy instantiation). 그래서 `T`가 어떤 연산을 지원하지 않아도 그 멤버를 안 부르면 컴파일이 통과한다 — `Safe_Release`가 `->Release()` 멤버만 있으면 어떤 타입이든 받는 "컴파일타임 덕 타이핑(duck typing)"이 이 원리다.
- C++17 CTAD(class template argument deduction)부터는 `std::pair p{1, 2.0f};`처럼 클래스 템플릿도 생성자 인자에서 추론 가능.

### 2.3 특수화(specialization) / 부분 특수화(partial specialization)

- **완전 특수화** `template<> struct Hash<MyKey> {...}`: 특정 타입에 대해 별도 구현 제공. 함수/클래스 모두 가능.
- **부분 특수화** `template<typename T> struct Traits<T*> {...}`: "포인터 타입 전부" 같은 타입 패밀리에 별도 구현. **클래스 템플릿만 가능** — 함수 템플릿에는 부분 특수화가 없고, 같은 효과는 오버로딩으로 낸다.
- 함정: 함수 템플릿의 완전 특수화는 **오버로드 해석(overload resolution)에 참여하지 않는다.** 오버로드 후보는 원본 템플릿과 일반 함수들 중에서 먼저 고르고, 그 다음에 특수화가 있으면 그것이 쓰인다. 그래서 실무에서는 함수 특수화 대신 오버로드를 권장한다.

### 2.4 타입 추론과 auto

`auto`의 추론 규칙은 **함수 템플릿 인자 추론과 동일**하다 (`auto x = expr;`는 `template<typename T> void f(T x)`에 `f(expr)`한 것과 같음):

- `auto` (값): 참조/최상위 const 벗겨짐 → 복사.
- `auto&`: lvalue에만 바인딩, const 유지.
- `auto&&`: **forwarding reference** — lvalue면 `T&`, rvalue면 `T&&`로 접힌다(reference collapsing).
- 유일한 예외: `auto x = {1,2,3};`은 `std::initializer_list`로 추론(템플릿은 이걸 추론 못 함).

ECS 순회에서 `for (auto& comp : ...)`를 `auto comp`로 잘못 쓰면 **컴포넌트가 복사되어 수정이 원본에 반영 안 되는** 실전 버그가 된다.

---

## ③ 심화

### 3.1 SFINAE → if constexpr → concepts 진화

| 단계 | 도구 | 원리 | 한계 |
|---|---|---|---|
| C++98~ | SFINAE | 치환 실패는 에러가 아니라 후보 제외(Substitution Failure Is Not An Error). `enable_if`로 오버로드를 조건부 제거 | 에러 메시지 지옥, 의도가 코드에 안 드러남 |
| C++17 | `if constexpr` | 분기를 하나의 함수 안에서 컴파일타임에 선택. 템플릿 안에서 **버려진 가지는 인스턴스화되지 않음** | 오버로드 집합 제어는 못 함 |
| C++20 | concepts | `requires`로 타입 제약을 **선언부에 명시**. 제약 불만족 시 후보 제외 + 읽을 수 있는 에러 | C++20 필요 (Winters는 C++20 빌드지만 concepts 도입 전 — `static_assert`로 계약 강제, 도입 여지 있음) |

핵심 이해:
- SFINAE는 "실패를 이용하는" 트릭이고, concepts는 같은 일을 **1급 언어 기능**으로 만든 것.
- `if constexpr`은 **템플릿 안**에서는 false 가지를 아예 인스턴스화하지 않으므로, `T`에 따라 컴파일 불가능한 코드를 가지에 둘 수 있다. **비템플릿 문맥**에서는 양쪽 가지 모두 문법/의미 검사는 통과해야 하고, 대신 false 가지의 **코드 생성이 보장되어 제거**된다 — 옵티마이저에 기대는 런타임 `if (상수)`와 달리 언어 차원의 보장이다.

### 3.2 가변 인자 템플릿(variadic template)과 perfect forwarding

```cpp
template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
```

- `Args&&`는 forwarding reference: lvalue가 오면 `Args=U&`로 추론되고 reference collapsing(`& && → &`)으로 lvalue 참조가, rvalue가 오면 `Args=U`로 rvalue 참조가 된다.
- `std::forward<Args>(args)`는 이 추론 정보를 이용해 **원래의 값 범주(value category)를 복원**해 넘긴다. `std::move`는 무조건 rvalue화, `forward`는 조건부 — 이것이 둘의 차이다.
- `emplace_back`/`make_unique`/`make_shared`가 전부 이 조합이다: 인자를 복사 없이 최종 생성자까지 관통시킨다.
- 파라미터 팩 전개는 재귀 또는 C++17 fold expression(`(f(args), ...)`)으로 처리.

### 3.3 코드 팽창(code bloat)과 컴파일 시간

- 템플릿은 타입마다 코드를 **찍어내므로** 인스턴스 수 × 함수 크기만큼 바이너리가 커진다. `vector<int>`, `vector<float>`, `vector<Foo*>`는 완전히 별개의 코드.
- 완화 기법:
  - **Thin template idiom**: 타입 비의존 로직을 non-template 코어 함수(예: `void* + size` 기반)로 빼고, 템플릿은 얇은 캐스팅 래퍼만 담당. `vector<T*>` 계열을 `vector<void*>` 하나로 접는 게 고전 사례.
  - **extern template** (C++11): `extern template class Store<int>;`로 "이 TU에서는 인스턴스화하지 말라"고 선언하고 한 TU에서만 명시적 인스턴스화 → 중복 컴파일 제거로 **컴파일 시간** 단축.
  - 링커 ICF(identical COMDAT folding, MSVC `/OPT:ICF`): 기계어가 동일한 인스턴스(예: 모든 `T*` 특수화)를 하나로 병합.
- 컴파일 시간 비용의 본질: 헤더에 있는 템플릿 정의는 **포함하는 모든 TU에서 매번 파싱+인스턴스화**된다. 템플릿 헤더가 무거울수록 빌드가 전체적으로 느려진다 — `std::function`으로 인터페이스를 좁혀 "컴파일 방화벽"을 치는 트레이드오프가 여기서 나온다(§4.4).

### 3.4 템플릿 vs 상속 — 정적 다형성 vs 동적 다형성

| | 템플릿 (정적) | 상속+가상함수 (동적) |
|---|---|---|
| 결정 시점 | 컴파일타임 (단형화, monomorphization) | 런타임 (vtable 간접호출) |
| 호출 비용 | 0 — 인라인 가능 | 간접호출 + 인라인 차단 |
| 이종 컨테이너 | 불가 (타입마다 별개 타입) | 가능 (base 포인터로 담음) |
| 인터페이스 강제 | 암묵적(쓰는 연산만 요구) / C++20 concepts | 명시적(순수가상) |
| 비용 | 코드 팽창, 컴파일 시간, 헤더 노출 | 런타임 오버헤드, 힙/포인터 간접성 |

실전 답: **양자택일이 아니라 조합**한다. "타입별로 최적화된 코드가 필요한 안쪽"은 템플릿, "타입이 다른 것들을 한 곳에 모아야 하는 경계"는 가상함수 — 이 조합이 type erasure이고, Winters ECS가 정확히 이 구조다(§4.2).

---

## ④ Winters에서의 적용

### 4.1 인스턴스화 모델을 DLL 경계 설계에 그대로 반영 — `CWorld`

`Engine/Public/ECS/World.h:58-77` — 비템플릿 멤버만 `WINTERS_ENGINE`(dllexport)이고, 템플릿 멤버는 헤더에 본문을 둔다:

```cpp
// non-template 멤버만 DLL 경계를 넘음 (template 멤버는 header에서 인스턴스화)
WINTERS_ENGINE EntityID CreateEntity();
...
template<typename T> T& AddComponent(EntityID e, const T& c = T{})
{
    return GetOrCreateStore<T>().Add(e, c);
}
```

왜 이 설계인가: DLL은 **이미 컴파일된 기계어**의 집합이다. Engine.dll을 빌드하는 시점에는 Client/Server가 `AddComponent`를 어떤 `T`로 쓸지 알 수 없으므로, 열린 템플릿은 export가 원리적으로 불가능하다. 그래서 `CreateEntity`/`DestroyEntity` 같은 타입 비의존 API만 DLL 심볼로 내보내고, `AddComponent<T>`는 소비 측 TU(Client, Server)에서 각자 인스턴스화되게 헤더에 노출했다. Jax 전용 컴포넌트(`Shared/GameSim/Champions/Jax/JaxGameSim.cpp:302`)를 Engine 재빌드 없이 추가할 수 있는 것도 이 구조 덕이다.

### 4.2 Type erasure — 가상 베이스 + 템플릿 래퍼로 이종(heterogeneous) 스토어 소유

`Engine/Public/ECS/World.h:18-44, 212`:

```cpp
class IComponentStoreBase {
public:
    virtual ~IComponentStoreBase() = default;
    virtual void Remove(EntityID entity) = 0;
    virtual bool Has(EntityID entity) const = 0;
};
template<typename T>
class CComponentStoreWrapper : public IComponentStoreBase { ... CComponentStore<T> m_store; };
...
std::unordered_map<std::type_index, std::unique_ptr<IComponentStoreBase>> m_mapStores;
```

- 컴파일타임 타입 다양성(`CComponentStore<T>`마다 별개 타입)을 런타임 다형성(base 포인터)으로 접었다. `DestroyEntity`는 타입을 몰라도 가상 `Remove`만 돌면 된다.
- `Engine/Public/ECS/World.h:179-186`의 `TryGetStore<T>`는 `std::type_index(typeid(T))`로 맵을 찾은 뒤 **`dynamic_cast`가 아닌 `static_cast`** 로 내린다. 맵의 불변식이 "키 `type_index(T)` 슬롯에는 반드시 `CComponentStoreWrapper<T>`가 들어있다"를 보장하므로(삽입 지점 `GetOrCreateStore<T>`가 유일), 런타임 타입검사 비용을 낼 이유가 없다. **RTTI를 캐스팅이 아니라 조회 키로 쓰는** 사례.

### 4.3 Forwarding reference 순회 — `ForEach(Fn&&)`의 0-오버헤드와 그걸 스스로 버린 함정

`Engine/Public/ECS/World.h:135-144`:

```cpp
template<typename T, typename Fn>
void ForEach(Fn&& fn)
{
    auto* s = TryGetStore<T>();
    if (!s) return;
    for (uint32_t i = 0; i < s->Count(); ++i)
        fn(s->Entities()[i], s->Data()[i]);
}
```

콜러블을 `std::function`이 아닌 `Fn&&`로 받아 호출자마다 람다 타입으로 **단형화**된다 — 람다 본문이 루프 안에 인라인되고 간접호출/힙할당이 없다.

그런데 `Shared/GameSim/Champions/Ashe/AsheGameSim.cpp:298-308`은 이 이점을 스스로 무력화한다:

```cpp
world.ForEach<AsheSimComponent>(
    std::function<void(EntityID, AsheSimComponent&)>(
        [&](EntityID, AsheSimComponent& state) { ... }));
```

`Fn`이 `std::function`으로 추론되면서 엔티티마다 타입 소거된 간접호출이 강제되고 인라인이 막힌다(Jax `JaxGameSim.cpp:440-442`도 동일). `[&]` 캡처가 작아 SBO(small buffer optimization)로 힙할당은 피하더라도, "제네릭 API를 `std::function`으로 좁히지 말라"는 교훈의 실물 — 면접에서 **자기 코드의 약점을 자기 입으로 지적하고 수정안(그냥 람다를 전달)까지 내는** 소재로 쓸 수 있다.

### 4.4 반대편 트레이드오프 — `std::function`이 정당한 자리

`Client/Public/GamePlay/ChampionRegistry.h:14-20`:

```cpp
using IterFn = std::function<void(eChampion, const ChampionDef&)>;
void ForEach(const IterFn& fn) const;
```

여기서는 의도적으로 타입 소거를 택했다: `ForEach`의 본문을 `.cpp`에 숨길 수 있어 헤더가 컴파일 방화벽이 되고, Registry 헤더가 호출자(Catalog)를 몰라도 된다. 챔피언 목록 순회는 틱마다 도는 hot path가 아니므로 간접호출 비용이 문제되지 않는다. **같은 프로젝트 안에서 hot path(ECS ForEach)는 템플릿, cold path(등록/조회)는 `std::function`** — 트레이드오프를 위치별로 다르게 판단했다는 것이 답변 포인트.

### 4.5 Phantom type 태그 — 타입 안전 핸들 `RHIHandle<TTag>`

`Engine/Public/RHI/RHIHandles.h:6-9, 58-59`:

```cpp
template<typename TTag>
struct RHIHandle { u64_t value = 0; ... };

using RHIBufferHandle  = RHIHandle<RHIBufferTag>;
using RHITextureHandle = RHIHandle<RHITextureTag>;
```

`TTag`는 본문에서 한 번도 쓰이지 않는 phantom type이다. 모든 핸들의 실체는 `u64_t` 하나(상위 32비트 generation + 하위 32비트 index 패킹)로 동일하지만, 태그가 다르면 **서로 다른 타입**이므로 버퍼 핸들 자리에 텍스처 핸들을 넘기면 컴파일 에러다. 빈 태그 struct는 코드도 데이터도 만들지 않으므로 **zero-cost strong typedef**. `IsValid()`가 `Generation() != 0`을 확인하고 슬롯 재사용 시 generation을 올려 stale 핸들(use-after-free)을 런타임에도 잡는다 — 컴파일타임(타입 혼용)과 런타임(수명) 안전을 한 구조로 해결.

### 4.6 컴파일타임 계약 — 클래스 스코프 `static_assert`

`Engine/Private/RHI/DX11/DX11ConstantBuffer.h:12-15`:

```cpp
template<typename T>
class DX11ConstantBuffer
{
    static_assert(sizeof(T) % 16 == 0, "Constant buffer size must be 16-byte aligned");
```

HLSL cbuffer는 16바이트 정렬을 요구한다. CPU struct가 이를 어기면 GPU가 쓰레기 값을 읽는 **런타임에서 추적 지옥인 버그**가 되는데, 클래스 스코프 `static_assert`는 인스턴스화 시점에 이를 빌드 실패로 바꾼다. "템플릿 인자에 대한 요구사항을 코드로 문서화하고 강제한다"는 목적은 concepts와 같다 — 빌드 표준은 C++20이지만 아직 concepts 도입 전이라 `static_assert`로 계약을 강제하고 있고, `requires`로의 전환 여지가 있는 자리다.

### 4.7 참조 매개변수 함수 템플릿 — `Safe_Delete(T&)`

`Engine/Public/Engine_Function.h:8-16`:

```cpp
template<typename T>
void	Safe_Delete(T& Pointer)
{
    if (nullptr != Pointer)
    {
        delete Pointer;
        Pointer = nullptr;
    }
}
```

- `T&`(포인터에 대한 참조)로 받아야 `nullptr` 대입이 **호출자의 변수**에 반영된다. 값으로 받으면 사본만 null이 되고 호출자에겐 dangling pointer가 남는다.
- `delete nullptr`는 표준이 no-op을 보장하므로 null 체크는 delete 보호가 아니라 대입 경로용이다.
- `Safe_Delete_Array`(`:18`)를 별도 함수로 둬서 `new[]`/`delete` 짝 오류를 API 이름 수준에서 구분했고, `Safe_Release`(`:39-52`)는 `->Release()` 멤버만 요구하는 덕 타이핑으로 COM 계열(DX11 리소스)을 일괄 처리한다.

### 4.8 템플릿으로 타입을 런타임 데이터로 내리기 — `Read<T>()/Write<T>()`

`Engine/Public/ECS/SystemAccess.h:31-43, 66-72` — `CSystemAccessBuilder::Read<T>()`가 `std::type_index(typeid(T))`를 벡터에 push해 시스템의 접근 집합을 **선언적 메타데이터**로 만들고, `SystemAccessConflicts`(`:77-96`)가 write-write/write-read 충돌을 검사해 충돌 없는 시스템만 같은 batch로 병렬 실행한다. `ISystem::DescribeAccess`의 기본 구현(`Engine/Public/ECS/ISystem.h:17-20`)은 `UnknownWritesAll()` — 선언하지 않은 시스템은 "전부 쓴다"로 간주돼 절대 병렬화되지 않는 fail-safe 기본값. 템플릿이 컴파일타임 타입을 런타임 스케줄링 데이터로 변환하는 다리 역할을 한다.

### 4.9 `if constexpr`로 트레이스 블록 소거

`Client/Private/Network/Client/SnapshotApplier.cpp:79, 701-727` — `constexpr bool_t kSnapshotMinionYawDebugOutput` 플래그를 `#if defined(_DEBUG)` 안에서 다시 `if constexpr`로 감싼다. 플래그를 `false`로 바꾸면 릴리스는 물론 **디버그 빌드에서도** `sprintf_s` 트레이스 블록의 코드 생성이 제거된다 — 전처리기(빌드 구성 차원)와 `if constexpr`(상수 플래그 차원)의 계층적 조합.

### 4.10 익명 네임스페이스 타입을 템플릿 인자로 — TU-로컬 컴포넌트

`Shared/GameSim/Champions/Jax/JaxGameSim.cpp:30, 71-77` — `JaxDashComponent`는 `.cpp`의 anonymous namespace 안 내부 연결(internal linkage) 타입인데 그대로 `world.AddComponent<JaxDashComponent>`(`:302`)의 템플릿 인자로 쓰인다. 익명 네임스페이스 타입은 TU마다 고유한 `typeid`를 가지므로, `type_index`로 키잉하는 ECS에서 다른 TU와 충돌 없이 Jax TU 안에서만 일관되게 조회된다. 대시 상태를 헤더에 노출하지 않는 캡슐화 + 컴파일 결합 최소화이지만, **다른 TU에서 같은 이름의 컴포넌트를 조회하려는 순간 조용히 못 찾는** 양날의 검임을 함께 말할 수 있어야 한다.

---

## ⑤ 면접 Q&A

### Q1. 템플릿은 왜 정의를 헤더에 둬야 하나요?

- 템플릿은 사용되는 타입으로 **인스턴스화되는 시점**에 코드가 생성되고, 인스턴스화는 사용하는 TU 안에서 일어난다. 정의가 안 보이면 코드 생성 자체가 불가능 → 링크 에러.
- 예외는 명시적 인스턴스화로 타입을 못박는 경우뿐.
- **Winters**: `World.h:58` 주석이 이 규칙을 그대로 문서화 — 템플릿 멤버는 헤더 본문, 비템플릿만 dllexport.

### Q2. 템플릿 함수를 DLL로 export할 수 있나요?

- 열린 `T`에 대해서는 불가. DLL은 컴파일 완료된 바이너리인데, 빌드 시점에 어떤 `T`로 쓰일지 모르므로 찍어낼 코드가 없다. 특정 인스턴스만 명시적 인스턴스화+export는 가능하지만 API가 닫힌다.
- **Winters**: `CWorld`는 `CreateEntity` 등 비템플릿만 `WINTERS_ENGINE`으로 내보내고 `AddComponent<T>`는 Client/Server TU에서 각자 인스턴스화(`World.h:58-77`). 덕분에 챔피언별 신규 컴포넌트를 Engine 재빌드 없이 추가.

### Q3. 콜백을 `std::function`으로 받는 것과 템플릿(`Fn&&`)으로 받는 것의 차이는?

- 템플릿: 호출자마다 단형화 → 인라인, 간접호출/할당 없음. 대신 정의가 헤더에 노출되고 인스턴스마다 코드 생성.
- `std::function`: 타입 소거 → 간접호출 + (SBO 초과 시) 힙할당, 대신 정의를 `.cpp`에 숨겨 컴파일 방화벽/ABI 경계 가능.
- **Winters**: hot path인 `CWorld::ForEach`는 `Fn&&`(`World.h:135`), cold path인 `CChampionRegistry::ForEach`는 `std::function`(`ChampionRegistry.h:14`). 그리고 `AsheGameSim.cpp:298`은 `Fn&&` API에 `std::function`을 감싸 넘겨 이점을 버린 안티패턴 — 발견하면 래핑 제거가 정답.

### Q4. 타입이 서로 다른 객체들을 하나의 컨테이너에 담으려면?

- 상속 기반 type erasure: 타입 비의존 연산만 가진 가상 베이스 + 타입별 템플릿 래퍼를 base 포인터(`unique_ptr`)로 소유. (`std::any`/`std::variant`도 언급하되, variant는 닫힌 타입 집합용.)
- **Winters**: `IComponentStoreBase` + `CComponentStoreWrapper<T>` + `unordered_map<type_index, unique_ptr<...>>`(`World.h:18-44, 212`). 다운캐스트는 맵 키가 동적 타입을 보장하므로 `dynamic_cast`가 아닌 `static_cast`(`World.h:182-185`) — "언제 static 다운캐스트가 안전한가"까지 이어서 답변.

### Q5. 같은 정수인데 서로 다른 종류의 ID가 섞이는 실수를 컴파일타임에 막으려면?

- 빈 태그 struct를 템플릿 인자로 받는 phantom type — 표현은 같아도 타입은 별개인 zero-cost strong typedef.
- **Winters**: `RHIHandle<TTag>`(`RHIHandles.h:6`), `RHIBufferHandle` vs `RHITextureHandle`은 상호 대입 불가. 추가로 generation 32비트를 패킹해 stale 핸들 감지까지 — 타입 안전(컴파일타임) + 수명 안전(런타임)을 한 설계로.

### Q6. `if`, `#if`, `if constexpr`의 차이를 설명해 보세요.

- `#if`: 전처리 단계에서 텍스트 자체를 제거. 빌드 구성/플랫폼 분기.
- 런타임 `if`: 양쪽 가지 모두 컴파일+코드 생성, 분기는 실행 시.
- `if constexpr`: 조건이 컴파일타임 상수. 템플릿 안에서는 버려진 가지가 **인스턴스화조차 안 되고**, 비템플릿에서도 코드 생성 제거가 보장.
- **Winters**: `SnapshotApplier.cpp:702`가 `#if defined(_DEBUG)` 안에 `if constexpr(k...DebugOutput)`를 계층으로 조합 — 빌드 구성 분기와 기능 플래그 분기를 분리.

### Q7. SFINAE에서 concepts까지의 진화를 설명해 보세요.

- SFINAE: 치환 실패가 에러가 아니라 오버로드 후보 제외라는 규칙을 `enable_if`로 역이용. 동작하지만 의도가 숨고 에러가 난해.
- C++17 `if constexpr`: 분기 선택 문제를 함수 하나 안에서 해결.
- C++20 concepts: 타입 요구사항을 `requires`로 선언부에 명시, 에러도 읽을 수 있음.
- **Winters**는 빌드 표준이 C++20(`<LanguageStandard>stdcpp20`, 4개 프로젝트 공통)이지만 concepts는 아직 도입 전 — 대신 클래스 스코프 `static_assert`로 계약을 강제한다. `DX11ConstantBuffer<T>`의 16바이트 정렬 검사(`DX11ConstantBuffer.h:15`)가 그 예이고, "이 자리는 `requires`로 옮길 수 있다"고 도입 여지까지 말하면 완결.

### Q8. `std::move`와 `std::forward`의 차이는?

- `move`: 무조건 rvalue로 캐스팅 — "나는 이 값을 포기한다"는 선언.
- `forward<T>`: forwarding reference로 받은 인자의 **원래 값 범주를 복원** — lvalue로 왔으면 lvalue로, rvalue로 왔으면 rvalue로. 추론된 `T`가 참조인지 아닌지에 정보가 실려 있어 가능(reference collapsing).
- 용도 구분: 자기 소유 자원을 넘길 땐 `move`, 템플릿에서 남의 인자를 관통시킬 땐 `forward`. `DX11ConstantBuffer`의 이동 연산(`DX11ConstantBuffer.h:24-39`)은 move 시맨틱 쪽 연결 고리.

---

## ⑥ 흔한 오답/함정

1. **"템플릿은 런타임에 타입을 결정한다"** — 완전히 반대. 템플릿은 100% 컴파일타임이고 런타임 비용이 없다(그래서 단형화·인라인이 가능). 런타임 타입 분기는 가상함수/`variant`의 영역. 이 혼동은 탈락급 오답.
2. **"헤더에 두는 건 관례/스타일이다"** — 아니다. 인스턴스화하는 TU가 정의를 봐야 한다는 컴파일-링크 모델의 강제다. "그럼 `.cpp`에 두고 명시적 인스턴스화하면?"이라는 꼬리질문에 "닫힌 타입 집합에서만 가능"까지 답해야 완결.
3. **제네릭 API에 `std::function`을 끼워 넣기** — `Fn&&`로 받는 API에 굳이 `std::function`을 만들어 넘기면 단형화가 무력화되고 per-element 간접호출이 생긴다. Winters `AsheGameSim.cpp:298`이 실물 — "내 코드에도 있었고 이렇게 고친다"고 말하면 오히려 가점.
4. **`Safe_Delete`류를 값 매개변수로 작성** — `void Safe_Delete(T* p)`로 받으면 사본만 null이 되어 호출자에 dangling pointer가 남는다. `T&`(포인터의 참조)여야 한다(`Engine_Function.h:8`). 추가 함정: null 체크가 delete를 보호한다고 답하는 것 — `delete nullptr`는 표준 no-op.
5. **"함수 템플릿도 부분 특수화 된다"** — 안 된다. 부분 특수화는 클래스 템플릿 전용이고, 함수는 오버로딩으로 해결한다. 완전 특수화가 오버로드 해석에 참여하지 않는다는 것까지 알면 상급.

---

## 다른 챕터와의 연결

- [02_compile_link_dll.md](02_compile_link_dll.md) — 인스턴스화/ODR/COMDAT, dllexport 경계의 컴파일-링크 모델 전반.
- [06_polymorphism_virtual.md](06_polymorphism_virtual.md) — type erasure의 반대축인 가상함수/vtable, `static_cast` vs `dynamic_cast` 다운캐스트.
- [08_stl_containers_cache.md](08_stl_containers_cache.md) — `std::function`의 SBO/할당 비용, 컨테이너 순회 성능.
- [11_architecture_ecs.md](11_architecture_ecs.md) — `CWorld` 레지스트리/컴포넌트 스토어 설계 전체 맥락.
