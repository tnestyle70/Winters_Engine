# 클래스 설계 · 값 의미론 · 복사/이동

> 대상: Winters 엔진(C++17/DX11)을 직접 만든 게임 프로그래머 지망생. 개념을 정확히 설명하고 자기 코드로 증명한다.

---

## ① 한 줄 본질

> "C++ 클래스 설계의 핵심은 **소유권(ownership)을 타입으로 표현하는 것**이고, 특수 멤버 함수(special member function) 6종은 그 소유권이 복사될 때 **복제할지, 이동할 때 이전할지, 아예 금지할지**를 컴파일러에게 지시하는 계약입니다. 자원을 직접 소유하면 Rule of Three/Five, 소유를 `unique_ptr` 같은 멤버에 위임하면 Rule of Zero로 설계합니다."

면접에서 "복사 생성자가 뭐냐"는 질문은 문법 문제가 아니라 소유권 문제다. Winters에는 `unique_ptr` 멤버, raw `ID3D11*` 포인터, 정수 핸들이라는 세 가지 소유 방식이 공존하고, 각각 복사/이동 규칙이 다르게 설계돼 있어 이 주제 전체를 실코드로 증명할 수 있다.

---

## ② 기본 개념

### 특수 멤버 함수 6종

| 함수 | 시그니처 | 역할 |
|---|---|---|
| 기본 생성자(default ctor) | `T()` | 멤버 순서대로 초기화 |
| 소멸자(destructor) | `~T()` | 멤버를 **선언 역순**으로 파괴 |
| 복사 생성자(copy ctor) | `T(const T&)` | 없던 객체를 복제해 **생성** |
| 복사 대입(copy assignment) | `T& operator=(const T&)` | **이미 살아있는** 객체를 덮어씀 |
| 이동 생성자(move ctor) | `T(T&&)` | 원본의 자원을 훔쳐 생성 |
| 이동 대입(move assignment) | `T& operator=(T&&)` | 기존 자원 해제 후 원본 자원 인수 |

**복사 생성 vs 복사 대입의 차이**: 생성은 raw 메모리를 채우는 초기화이고, 대입은 이미 자원을 가진 객체를 다루므로 "기존 자원 정리 → 자기 대입 방어 → 새 값 확보"라는 추가 책임이 있다. `T a = b;`는 `=` 기호가 있어도 **복사 생성**(선언과 동시 초기화)이고, `a = b;`만 대입이다.

### 컴파일러 자동 생성 규칙 (억제 규칙까지)

- 사용자가 아무것도 선언하지 않으면 6종 모두 암묵 생성된다(멤버가 허용하는 한).
- **소멸자·복사 생성자·복사 대입 중 하나라도 선언하면 이동 연산은 암묵 생성되지 않는다** → 이동해야 할 자리에서 조용히 복사가 일어난다(성능 함정).
- 이동 생성자나 이동 대입을 선언하면 복사 연산은 **delete**된다.
- 멤버 중 하나라도 복사 불가(`unique_ptr`, `atomic`, `mutex`, 참조 멤버)면 암묵 복사가 delete된다.

### Rule of 0 / 3 / 5

- **Rule of Zero**: 자원 관리를 `unique_ptr`/`vector` 같은 멤버에 위임하고 특수 멤버를 하나도 안 쓴다. Winters의 POD 컴포넌트가 이 경우.
- **Rule of Three** (C++98): 소멸자·복사 생성자·복사 대입 중 하나를 정의해야 한다면 셋 다 필요하다. 이유는 소멸자가 필요하다 = 자원을 직접 소유한다 = 얕은 복사(shallow copy)가 이중 해제(double free)를 만들기 때문.
- **Rule of Five** (C++11): 위 셋에 이동 생성자·이동 대입을 더한다. 셋 중 하나라도 선언하면 이동이 자동 생성되지 않으므로, 이동 가능하게 하려면 명시해야 한다.

### 이동 의미론(move semantics)

- **rvalue reference** `T&&`는 "곧 사라질 값(임시 객체, `std::move`된 값)"에만 바인딩되는 참조다. 이 오버로드가 선택되면 "원본을 파괴적으로 재사용해도 된다"는 신호다.
- **`std::move`는 아무것도 옮기지 않는다. `static_cast<T&&>`일 뿐이다.** 실제 이동은 그 캐스트 결과에 대해 이동 생성자/대입이 선택될 때 그 함수 본문이 수행한다. 이동 연산이 없는 타입에 `std::move`를 써도 복사가 일어난다(컴파일은 됨).
- **이동 후 상태(moved-from state)**: 표준 라이브러리 타입은 "유효하지만 미지정(valid but unspecified)" — 소멸과 재대입은 안전하고, 값을 읽는 것은 계약 위반이다. 직접 작성하는 이동 연산은 원본 포인터를 `nullptr`로 만들어 소멸자가 안전하게 지나가도록 해야 한다.

### RVO / NRVO / copy elision

- **RVO(Return Value Optimization)**: `return T{...};`처럼 prvalue를 반환하면 복사/이동 없이 호출자의 저장 공간에 직접 생성한다. **C++17부터는 최적화가 아니라 언어 규칙(guaranteed copy elision)** — 복사/이동 생성자가 delete여도 컴파일된다.
- **NRVO(Named RVO)**: `T t; ...; return t;`처럼 이름 있는 지역 변수 반환. 이건 여전히 허용된 최적화일 뿐 보장이 아니다. NRVO가 실패하면 지역 변수는 자동으로 rvalue 취급되어 **이동**된다(implicit move).
- 그래서 `return std::move(local);`은 **안티패턴** — NRVO를 오히려 방해하고, 이동은 어차피 자동으로 일어난다.

### const correctness

- `const T&` 파라미터: 읽기만 한다는 계약 + 임시 객체도 받는다.
- `const` 멤버 함수: `this`가 `const T*`가 되어 멤버 수정 불가. **bitwise const**(비트 하나도 안 바뀜)가 컴파일러의 강제이고, **logical const**(관찰 가능한 상태 불변)가 설계 의도다. 둘의 간극을 메우는 것이 `mutable`(캐시, 뮤텍스).
- getter의 반환 타입: **실존 멤버는 `const T&`, 즉석 계산한 임시값은 `T` 값 반환.** 임시를 참조로 반환하면 댕글링(dangling)이다.

### explicit

단일 인자 생성자는 암시적 변환 경로가 된다. `void Feed(Champion c)`에 정수를 넘겨도 `Champion(int)` 생성자가 있으면 조용히 컴파일되는 사고를 `explicit`이 막는다. 변환 연산자(`operator bool` 등)에도 붙일 수 있다. **기본 방침: 의도적인 변환 타입(예: 문자열 래퍼)이 아닌 한 단일 인자 생성자는 explicit.**

### 멤버 초기화 순서

멤버는 **초기화 리스트의 순서가 아니라 클래스 내 선언 순서**로 초기화된다. `T() : m_b(m_a), m_a(1)`처럼 쓰면 `m_a`가 먼저(선언 순서) 초기화되므로 우연히 동작하지만, 선언 순서가 `m_b, m_a`라면 `m_b`가 쓰레기 값 `m_a`로 초기화된다. 소멸은 정확히 그 역순. C++11의 **default member initializer**(`float fSpeed{ 0.f };`)를 쓰면 이 문제 자체를 줄일 수 있고, Winters 컴포넌트는 전부 이 방식이다.

### = delete / = default

- `= default`: "컴파일러 기본 구현을 쓰되, 내가 선언했다는 사실을 남긴다." 억제 규칙 때문에 사라진 이동을 되살리거나, 소멸자만 정의하면서 나머지를 유지할 때 쓴다.
- `= delete`: 함수를 오버로드 해소(overload resolution)에는 참여시키되 선택되면 컴파일 에러. private 선언(구식 기법)과 달리 friend도 못 뚫고, 에러 메시지가 "deleted function"으로 명확하다.

---

## ③ 심화 (꼬리질문 대비)

### noexcept 이동과 std::vector의 강한 예외 보장

`std::vector` 재할당은 원소를 새 버퍼로 옮길 때 `std::move_if_noexcept`를 쓴다. 이동 생성자가 `noexcept`가 아니면 — 이동 도중 예외가 나면 원본이 이미 파괴돼 롤백 불가이므로 — vector는 강한 예외 보장을 지키기 위해 **복사**를 선택한다. 즉 `noexcept` 한 단어가 컨테이너 성능을 좌우한다. 손으로 쓰는 이동 연산은 포인터 스왑뿐이라 던질 이유가 없으니 반드시 `noexcept`를 붙인다.

### MSVC dllexport와 특수 멤버 강제 인스턴스화

일반 클래스는 특수 멤버가 **사용될 때만** 인스턴스화되지만, `__declspec(dllexport)` 클래스는 MSVC가 **모든 특수 멤버를 즉시 인스턴스화해 export**하려 한다. 멤버에 `unique_ptr`가 있으면 암묵 복사 생성자 인스턴스화가 `unique_ptr`의 deleted copy에 걸려 `construct_at` 계열 컴파일 에러가 난다. 해법은 복사를 명시적으로 `= delete` 선언해서 MSVC가 복사 경로 인스턴스화를 스킵하게 만드는 것. Winters에서 실제 빌드를 깨뜨렸던 함정이고 gotchas에 박제돼 있다(아래 ④).

### 참조 멤버가 있는 구조체

참조 멤버가 하나라도 있으면 **복사 대입이 암묵 delete**된다(참조는 재바인딩 불가). 복사 생성은 가능하므로 "생성 시 한 번 바인딩되고 재대입은 불가능한 단명 번들"이 된다. 의존성 주입 컨텍스트 구조체에 유용한 성질이다.

### sink parameter 이디엄 — `T` 값으로 받고 `std::move`

소유권을 흡수하는 함수는 `const T&`+내부 복사 대신 **값으로 받는다**. 호출자가 lvalue를 주면 복사 1회, `std::move`로 주면 이동 1회 — 호출자가 비용을 선택한다. 함수 내부에서는 그 값을 `std::move`로 최종 저장소에 밀어 넣는다.

### 복사 vs 이동의 선택 기준

이동은 원본을 비우므로 **소비자가 하나일 때만** 쓸 수 있다. N명에게 같은 버퍼를 뿌리는 팬아웃(fan-out)은 각자 소유가 필요하니 복사가 정답이다. "무조건 move가 빠르다"가 아니라 소유권 그래프가 결정한다.

### 자기 대입(self-assignment) 검사

이동 대입에서 `if (this != &other)` 없이 "기존 자원 해제 → 인수"를 하면 `a = std::move(a)`에서 방금 해제한 자원을 다시 읽는다. 검사 방식 외에 copy-and-swap 이디옴(값 파라미터 + swap)으로 자기 대입·예외 안전을 한 번에 해결하는 방법도 있다(대신 항상 복사 비용).

---

## ④ Winters에서의 적용

아래 인용은 전부 실제 파일을 열어 확인했다.

### 1. dllexport 클래스 + `unique_ptr` 멤버 → 복사 명시 delete (팀 확정 규칙)

`Engine/Public/ECS/SystemScheduler.h:17-25` — 주석이 규칙 자체를 박제하고 있다:

```cpp
// ★ 중요: WINTERS_ENGINE dllexport 클래스가 unique_ptr 멤버를 포함할 때 필수.
// MSVC 는 dllexport 클래스의 모든 특수 멤버 함수를 강제 인스턴스화/export 하려 함.
// unique_ptr 의 copy 는 deleted → 암묵적 copy ctor 생성 실패 → construct_at 에러.
// 명시적으로 copy = delete 선언하면 MSVC 가 copy 경로 인스턴스화를 스킵.
CSystemSchedular(const CSystemSchedular&) = delete;
CSystemSchedular& operator=(const CSystemSchedular&) = delete;
CSystemSchedular(CSystemSchedular&&) = default;
CSystemSchedular& operator=(CSystemSchedular&&) = default;
```

`Engine/Public/ECS/World.h:52-56`도 동일 패턴(`// unique_ptr 멤버 때문에 복사 불가, 이동만 허용`). 이동은 `= default`로 열어둔다 — 멤버가 전부 이동 가능(`unique_ptr`, `unordered_map`)하므로 컴파일러 생성 이동이 정확하다. **Rule of Five를 DLL 경계 제약이 강제한 실사례**로, 2026-04-23 실제 빌드 실패에서 나온 gotcha다.

### 2. Rule of Five를 정석으로 지킨 클래스 vs 위반한 클래스 (같은 폴더 안의 대비)

**정석**: `Engine/Private/RHI/DX11/DX11ConstantBuffer.h:21-39` — 복사 delete + 손수 쓴 `noexcept` 이동:

```cpp
DX11ConstantBuffer(const DX11ConstantBuffer&) = delete;
DX11ConstantBuffer& operator=(const DX11ConstantBuffer&) = delete;
DX11ConstantBuffer(DX11ConstantBuffer&& other) noexcept
    : m_pBuffer(other.m_pBuffer) { other.m_pBuffer = nullptr; }   // 원본 null-out
DX11ConstantBuffer& operator=(DX11ConstantBuffer&& other) noexcept
{
    if (this != &other) { Release(); m_pBuffer = other.m_pBuffer; other.m_pBuffer = nullptr; }
    return *this;
}
```

원본 null-out(이중 Release 방지), 자기 대입 검사, 기존 자원 해제 순서, `noexcept`(vector 재할당 시 이동 선택) — 이동 대입의 4대 체크리스트가 전부 들어 있다.

**위반**: `Engine/Private/RHI/DX11/DX11Buffer.h:26` 소멸자가 `Release()`를 호출하고 `:62-63`에 raw `ID3D11Buffer*` 두 개를 소유하는데, **복사 제어 선언이 하나도 없다**. 값 복사되면 같은 COM 포인터를 두 번 `Release()`하는 double-free — Rule of Three 위반의 살아있는 표본이다. 면접에서 "내 코드에서 발견한 잠재 결함"으로 먼저 꺼내면 이해도를 역으로 증명할 수 있다.

### 3. Rule of Zero에 가까운 절충 — `unique_ptr` 멤버 덕에 이동 `= default`

`Engine/Public/Renderer/CMaterialPBR.h:19-22`:

```cpp
CMaterialPBR(const CMaterialPBR&) = delete;
CMaterialPBR& operator=(const CMaterialPBR&) = delete;
CMaterialPBR(CMaterialPBR&&) noexcept = default;
CMaterialPBR& operator=(CMaterialPBR&&) noexcept = default;
```

소유 자원(`std::unique_ptr<DX11ConstantBuffer<CBPerMaterial>> m_pConstantBuffer`, :49)이 이미 이동을 올바르게 구현하므로 이동을 손으로 쓸 필요가 없다. 같은 클래스 안에서 `CTexture* m_pAlbedo`(:46) 등은 **비소유 관찰자(observer) raw 포인터** — 소유는 스마트 포인터, 참조는 raw로 구분하는 컨벤션이 한 클래스에 정리돼 있다.

### 4. "가짜 이동 생성자" 함정 — 이동이 아무것도 안 옮기는 코드

`Engine/Public/Core/JobSystem/WorkStealingDeque.h:23-27`:

```cpp
CWorkStealingDeque(const CWorkStealingDeque&) = delete;
CWorkStealingDeque& operator=(const CWorkStealingDeque&) = delete;
// std::vector<CWorkStealingDeque> 담기 위해 move 만 허용
CWorkStealingDeque(CWorkStealingDeque&&) noexcept {}
CWorkStealingDeque& operator=(CWorkStealingDeque&&) noexcept { return *this; }
```

`std::atomic` 멤버는 복사/이동이 안 되므로 vector에 담으려고 **본문이 빈 이동 생성자**를 써넣은 것 — 컴파일은 되지만 "이동"해도 큐 내용이 옮겨지지 않는다. 이동 의미론의 시그니처만 흉내 내면 컴파일러는 막아주지 않는다는 교훈. 최종 해법은 `std::vector<std::unique_ptr<CWorkStealingDeque<WorkItem>>>`로 힙 간접화 — 동시에 다른 워커가 참조하는 deque 주소가 vector 재할당에도 불변이 되는 이중 효과를 얻었다.

### 5. `= delete`의 다양한 용법 — 복사 금지를 넘어서

- `Engine/Public/Core/JobCounter.h:21-22`: `std::atomic` 멤버라 어차피 복사 불가지만 **명시적으로 delete해 의도를 드러냄**.
- `Client/Public/GameObject/ChampionSpawnService.h:51`: `CChampionSpawnService() = delete;` — 기본 생성자 자체를 지워 **인스턴스화가 무의미한 static 전용 서비스 클래스**를 컴파일 타임에 봉인.
- `Engine/Public/Resource/Mesh.h:15-16` + `:32-33`: pimpl(`struct Impl; Impl* m_pImpl;`)로 소유 raw 포인터가 생기자 복사를 delete — Rule of Three의 "금지" 해법.

### 6. 참조 멤버 번들 — 복사 대입이 언어 규칙으로 delete되는 구조체

`Client/Public/GameObject/ChampionSpawnService.h:30-37`:

```cpp
struct ChampionSpawnContext
{
    CWorld& world;
    std::unordered_map<EntityID, std::unique_ptr<ModelRenderer>>& renderers;
    // ... 참조 멤버 3개 더
};
```

Scene이 소유한 저장소들을 서비스 함수에 주입하는 단명 컨텍스트. 참조 멤버 덕에 복사 대입이 자동 delete되고, 초기화 리스트로만 바인딩 가능 — "소유가 아닌 차용(borrow)"이 타입에 새겨진다.

### 7. sink parameter + 이동 체인 — 복사 0회 소유권 흐름

`Server/Private/Network/Session.cpp:91,101` — `bool CSession::Send(std::vector<u8_t> packet)`이 **값으로** 받고 `m_sendQueue.push_back(std::move(packet));`으로 큐에 밀어 넣는다. WSASend가 참조할 버퍼를 큐가 완료 시점까지 소유한다.

`Engine/Private/FX/FxAsset.cpp:598-605` — `Register(FxAsset asset)` 값 인자 → `slot.asset = std::move(asset);` → `m_Slots.push_back(std::move(slot));` — 문자열/emitter 벡터를 품은 무거운 자산이 이동 체인으로만 흘러 복사가 없다.

### 8. 복사 vs 이동 선택 기준이 코드에 그대로 드러난 곳

- 팬아웃: `Server/Private/Game/GameRoomLobby.cpp:286-290` — 로비 패킷을 N개 세션에 뿌릴 때 `pSession->Send(std::vector<u8_t>(packet.begin(), packet.end()))`로 **세션마다 복사**. 이동은 원본을 비우므로 여러 소비자에게 쓸 수 없다.
- 단일 수신자: `Server/Private/Game/GameRoomReplication.cpp:170` — 스냅샷은 수신자가 하나라 `pSession->Send(std::move(packet));`로 이동.

### 9. POD 컴포넌트 설계 — Rule of Zero의 극단

`Engine/Public/ECS/Components/CoreComponents.h:16-21`:

```cpp
struct HealthComponent
{
    float fCurrent{ 100.f };
    float fMaximum{ 100.f };
    bool bIsDead{ false };
};
```

가상 함수·상속·소유 포인터가 전혀 없는 trivially copyable 구조체 + default member initializer. 연속 메모리 저장, memcpy 수준 복사, 직렬화가 공짜다. `PlayerTag`(:30-33)는 필드 없는 empty tag(존재 자체가 플래그), `AbilityComponent`(:53-54)는 `vector` 대신 `uint32_t skillIDs[MAX_SKILLS]{}` 고정 배열로 힙 간접참조를 배제해 trivially copyable을 유지한다.

### 10. 팩토리 + private 생성자 — make_unique가 안 되는 이유

`Engine/Public/Core/CTimer.h:9`(private ctor) + `Engine/Private/Core/CTimer.cpp:40-48`:

```cpp
unique_ptr<CTimer> CTimer::Create()
{
    auto pInstance = unique_ptr<CTimer>(new CTimer());
    if (FAILED(pInstance->Initialize()))
        return nullptr;
    return pInstance;
}
```

`std::make_unique<CTimer>()`는 **라이브러리 함수 내부에서** `new CTimer()`를 호출하므로 private 생성자에 접근 불가 → 클래스 자신의 static 멤버 안에서 `unique_ptr<CTimer>(new CTimer())`로 직접 감싼다. 생성자는 실패를 반환할 수 없으니 `Initialize()` 실패를 `nullptr`로 변환하는 2단계 초기화이기도 하다.

### 11. getter 반환 타입 계약 — 값 vs const 참조

`EngineSDK/inc/ECS/Components/TransformComponent.h:57-64`:

```cpp
// 값 반환 (임시 객체라 참조 불가)
Vec3 GetWorldPosition() const
{ return { m_WorldMatrix.m._41, m_WorldMatrix.m._42, m_WorldMatrix.m._43 }; }
// 로컬 원본은 멤버가 존재하니 const 참조 OK
const Vec3& GetLocalPosition() const { return m_LocalPosition; }
```

즉석 계산한 임시는 값 반환(참조하면 댕글링), 실존 멤버는 const 참조 — 주석이 규칙을 명시한다.

---

## ⑤ 면접 Q&A

**Q1. Rule of Three가 왜 필요한가? Rule of Five는 뭐가 추가됐나?**
- 포인트: 소멸자를 직접 쓴다 = 자원을 직접 소유한다 = 암묵 복사(얕은 복사)가 이중 해제를 만든다. C++11에서 복사/소멸 중 하나라도 선언하면 이동이 자동 생성되지 않으므로 이동 2종을 추가로 다뤄야 Five.
- Winters: `DX11ConstantBuffer`가 Five를 정석으로 지킨 예, 같은 폴더 `DX11Buffer`는 소멸자에서 `Release()` 하면서 복사 제어가 없어 Three 위반(double-free 잠재) — 직접 발견한 결함으로 답하면 강하다.

**Q2. std::move는 무엇을 하는가?**
- 포인트: 아무것도 옮기지 않는다. `static_cast<T&&>`로 rvalue로 캐스트해서 **이동 오버로드가 선택되게 만들 뿐**, 실제 이동은 이동 생성자/대입 본문이 한다. 이동 연산이 없으면 move를 써도 복사된다.
- Winters: `WorkStealingDeque`의 빈 본문 이동 생성자 — `std::move`가 붙어도 본문이 비어 있으면 아무것도 안 옮겨진다는 산 증거.

**Q3. 이동 생성자를 손으로 쓸 때 체크리스트는?**
- 포인트: (1) 원본 포인터 null-out — 원본 소멸자가 이중 해제하지 않게 (2) 이동 대입은 자기 대입 검사 + 기존 자원 선해제 (3) `noexcept` — vector가 재할당 시 `move_if_noexcept`로 이동을 선택하게.
- Winters: `DX11ConstantBuffer.h:24-39`가 세 가지를 전부 갖췄다.

**Q4. unique_ptr 멤버가 있는 클래스의 복사/이동은 어떻게 되나? DLL export가 왜 여기 끼어드나?**
- 포인트: 암묵 복사는 delete되고 이동은 (다른 억제 요인이 없으면) 성립한다. 일반 클래스는 복사를 안 쓰면 문제없지만, MSVC dllexport 클래스는 모든 특수 멤버를 강제 인스턴스화/export하려다 deleted copy에 걸려 컴파일 에러 — 복사를 명시 `= delete`해서 인스턴스화를 스킵시킨다.
- Winters: `SystemScheduler.h:17-25` 주석 + `World.h:52-56`, 팀 gotcha로 박제된 실제 빌드 실패 사례.

**Q5. RVO와 NRVO의 차이는? `return std::move(local);`은 왜 나쁜가?**
- 포인트: prvalue 반환(RVO)은 C++17부터 언어 보장(복사/이동 생성자가 delete여도 됨), 이름 있는 변수 반환(NRVO)은 허용된 최적화일 뿐. NRVO 실패 시에도 지역 변수는 자동 이동된다. `return std::move(local)`은 반환식을 rvalue로 만들어 NRVO 적용 조건을 깨고, 이동은 어차피 자동이라 순손해.
- Winters: `CTimer::Create()`가 `unique_ptr` 지역 변수를 그냥 `return pInstance;` — move 없이 반환하는 올바른 형태.

**Q6. 언제 복사하고 언제 이동하는가?**
- 포인트: 이동은 원본을 비우므로 소비자가 하나일 때만. N명 팬아웃은 각자 소유가 필요해 복사. 소유권 그래프가 결정하지 성능 습관이 결정하는 게 아니다.
- Winters: 로비 브로드캐스트는 세션마다 복사(`GameRoomLobby.cpp:289`), 단일 수신 스냅샷은 이동(`GameRoomReplication.cpp:170`).

**Q7. ECS 컴포넌트를 왜 POD(virtual 없는 평면 struct)로 두는가?**
- 포인트: trivially copyable → 연속 배열 저장, memcpy/직렬화 안전, 캐시 지역성. vtable 포인터나 힙 소유 멤버가 들어가는 순간 이 성질이 깨진다. default member initializer로 초기화 누락도 방지.
- Winters: `CoreComponents.h`의 `HealthComponent` 등 전부 이 설계, `AbilityComponent`는 `vector` 대신 고정 배열로 trivially copyable을 지킴.

**Q8. explicit은 언제 붙이나?**
- 포인트: 단일 인자 생성자는 암시적 변환 경로가 되므로 기본적으로 explicit. 의도적으로 변환을 허용할 타입(문자열 래퍼 등)만 예외. 변환 연산자에도 적용 가능(`explicit operator bool`).
- 일반 지식으로 답하고, "Winters에서는 팩토리(private ctor + Create)로 생성 경로 자체를 하나로 좁혀 암시 변환 여지를 없앤 클래스가 많다"로 연결 가능(`CTimer`, `CMaterialPBR::Create`).

---

## ⑥ 흔한 오답/함정

1. **"std::move가 객체를 이동시킨다"** — 캐스트일 뿐이다. 이동 연산이 없는 타입, const 객체(`const T&&`는 이동 오버로드에 안 잡힘)에 쓰면 조용히 복사된다. "move 했으니 빨라졌겠지"는 측정 없이는 거짓일 수 있다.

2. **"소멸자만 정의했는데 뭐가 문제냐"** — 소멸자를 선언하는 순간 이동이 암묵 생성되지 않아 모든 이동 자리가 복사로 바뀌고(성능), 복사는 여전히 암묵 생성되므로 자원 소유 클래스면 double-free 폭탄(정확성)이다. Winters `DX11Buffer`가 정확히 이 상태.

3. **"이동 후 객체는 쓰면 안 되니 건드리지도 말라"** — 절반만 맞다. moved-from 객체는 valid but unspecified: 소멸·재대입은 안전하고 그래서 재사용 패턴(`buffer = ...` 재할당)이 합법이다. 값을 읽는 것만 계약 위반.

4. **"멤버는 초기화 리스트에 쓴 순서로 초기화된다"** — 선언 순서다. 초기화 리스트 순서와 선언 순서가 다르면 경고(-Wreorder/C5038)가 나오고, 멤버 간 의존이 있으면 미정의 값 읽기가 된다. 소멸은 선언 역순이라 의존 자원(디바이스 → 그 디바이스로 만든 버퍼)은 선언 순서로 수명을 설계해야 한다.

5. **"복사 금지는 private 선언으로 하면 된다"** — C++03 기법. `= delete`는 friend/멤버 접근도 오버로드 해소 단계에서 막고 에러 메시지가 명확하다. 또 dllexport 맥락에서는 명시 delete가 MSVC의 특수 멤버 강제 인스턴스화를 스킵시키는 실질 효과까지 있다(Winters gotcha).

---

## 다른 챕터와의 연결

- [03_memory_lifetime_raii.md](03_memory_lifetime_raii.md) — 복사/이동이 지키려는 것이 결국 RAII 소유권이다. moved-from 상태·소멸 순서의 기반 개념.
- [04_pointers_smart_pointers.md](04_pointers_smart_pointers.md) — `unique_ptr`가 move-only인 이유, make_unique와 private 생성자, 소유 vs 관찰자 포인터 구분.
- [02_compile_link_dll.md](02_compile_link_dll.md) — dllexport의 특수 멤버 강제 인스턴스화, C4251, DLL 경계에서의 STL 멤버 문제.
- [08_stl_containers_cache.md](08_stl_containers_cache.md) — noexcept 이동과 vector 재할당, trivially copyable 컴포넌트와 캐시 지역성.
