# 포인터 · 참조 · 스마트 포인터

> 대상: Winters 엔진을 직접 만든 게임 프로그래머 지망생. 목적: 신입~주니어 C++ 면접에서 "소유권(ownership)을 타입으로 설계할 줄 안다"는 것을 자기 코드로 증명하기.

---

## 1. 한 줄 본질

> **"포인터 선택의 첫 질문은 '이게 소유(owning)인가 관찰(non-owning)인가'다. 소유는 `unique_ptr`/`shared_ptr`로 타입에 박아 컴파일러가 수명을 강제하게 하고, 관찰은 raw pointer로 남겨 '나는 수명에 책임이 없다'를 표현한다."**

"raw pointer는 쓰면 안 되나요?"의 정답은 "안 된다"가 아니라 **"소유는 스마트 포인터, 관찰은 raw"라는 규약**이다. Winters의 리소스 캐시·타이머 매니저·챔피언 스폰이 전부 이 규약 위에 서 있다.

---

## 2. 기본 개념 (동작 원리)

### 2.1 포인터 vs 참조 (pointer vs reference)

| | 포인터 | 참조 |
|---|---|---|
| null 가능 | O (`nullptr`) | X (언어 규칙상 유효 객체에 바인딩) |
| 재지정(reseat) | O | X (초기화 이후 대상 변경 불가) |
| 산술 연산 | O (`p+1`) | X |
| 메모리 표현 | 주소값 그 자체 | 대부분의 구현에서 포인터와 동일 (표준은 저장 공간을 요구하지 않음) |

- 컴파일러 수준에서 참조는 보통 "자동 역참조되는 const 포인터"로 구현된다. 다만 최적화 시 아예 사라질 수 있다(레지스터로 승격).
- **선택 기준**: "없을 수 있다(nullable)" 또는 "다시 가리켜야 한다" → 포인터. "반드시 존재하고 별명(alias)일 뿐" → 참조. 함수 매개변수에서 `T&`는 "null 검사 없이 쓰겠다"는 계약이다.
- 참조도 댕글링(dangling)이 된다 — 지역 변수의 참조를 반환하면 UB. "참조는 안전하다"는 null 안전이지 수명 안전이 아니다.

### 2.2 소유권(ownership)

소유자란 **그 객체의 소멸(delete)을 책임지는 주체**다. C++에는 GC가 없으므로 모든 힙 객체는 소유자가 정확히 하나의 규칙 아래 있어야 한다:

- **단독 소유** → `std::unique_ptr<T>`: 이동만 가능(move-only), 복사 불가. 소유권 이전은 `std::move`로 명시.
- **공유 소유** → `std::shared_ptr<T>`: 참조 카운트(refcount)가 0이 될 때 파괴.
- **비소유 관찰** → raw pointer / 참조: 수명은 남이 책임진다. 관찰자의 수명이 소유자보다 짧다는 전제가 계약이다.

### 2.3 `unique_ptr` — 제로 오버헤드(zero-overhead)

- 상태가 없는 기본 deleter(`std::default_delete<T>`)일 때 `sizeof(unique_ptr<T>) == sizeof(T*)`. deleter는 [[no_unique_address]] 격의 EBO(empty base optimization)로 사라진다. `get()`/`operator*`는 인라인되어 raw pointer와 기계어가 같다.
- **커스텀 deleter는 타입의 일부다**: `unique_ptr<FILE, decltype(&fclose)>`처럼 함수 포인터를 deleter로 주면 포인터 하나만큼 크기가 커진다. 상태 없는 람다/함수 객체면 다시 제로 오버헤드.
- 소멸자에서 `delete`(배열 특수화 `unique_ptr<T[]>`는 `delete[]`)를 호출 — 이 한 줄이 RAII의 전부다. 조기 return, 예외, 모든 탈출 경로에서 자동 해제.

### 2.4 `shared_ptr` 내부 — 제어 블록(control block)

`shared_ptr`는 포인터 2개 크기다: **객체 포인터 + 제어 블록 포인터**. 제어 블록에는:

1. **strong count** — 0이 되면 객체 소멸자 호출
2. **weak count** — 0이 되면 제어 블록 자체 해제
3. type-erased **deleter**와 allocator

- `shared_ptr<T>(new T)` → 할당 2회(객체 + 제어 블록). `make_shared<T>()` → **한 덩어리 할당 1회**(객체와 제어 블록이 인접) → 할당 비용 절감 + 캐시 지역성.
- make_shared의 대가: `weak_ptr`가 살아 있으면 strong count가 0이어도 **객체 메모리(같은 덩어리)가 weak count 0까지 해제되지 않는다**(소멸자는 호출되지만 메모리는 잔류).
- refcount 증감은 `std::atomic` 연산(x86에서 `lock` prefix 명령)이다. 복사할 때마다 원자적 increment → 스레드 간 캐시 라인 경합(cache line contention) 비용. 그래서 **함수 인자로는 `const shared_ptr&` 또는 raw pointer로 넘겨 불필요한 복사(=원자 연산)를 피한다**.
- 중요한 구분: **refcount 조작만 스레드 안전**하다. 가리키는 객체 `T`에 대한 동시 접근은 여전히 사용자가 동기화해야 한다.

### 2.5 `weak_ptr`와 순환 참조(circular reference)

- A가 `shared_ptr<B>`, B가 `shared_ptr<A>`를 들면 strong count가 서로를 1 이상으로 붙잡아 **둘 다 영원히 파괴되지 않는다** (누수).
- 해법: 한 방향(보통 자식→부모, 관찰 방향)을 `weak_ptr`로 바꾼다. weak는 strong count를 올리지 않고 제어 블록의 weak count만 올린다.
- 사용은 `lock()`으로: strong count가 아직 0이 아니면 `shared_ptr`를 원자적으로 획득, 이미 죽었으면 빈 포인터. "죽었는지 확인 후 raw로 접근"의 race를 원자적 승격으로 대체한 것이다.

### 2.6 raw pointer의 정당한 용도

Core Guidelines(F.7, R.3) 기준으로 raw pointer/참조는 **비소유 관찰**에 쓰는 것이 정당하다:

- 소유 컨테이너가 확실히 더 오래 사는 경우의 조회 결과 반환
- 함수가 "빌려 쓰기만" 하는 매개변수
- 성능상 refcount 증감이 낭비인 hot path 순회

전제 조건은 **수명 계약이 코드 구조로 보장**될 것 — Winters에선 "캐시/매니저의 수명 > 사용자 수명"이 그 계약이다(§4.1).

---

## 3. 심화 (꼬리질문 대비)

### 3.1 `make_unique`/`make_shared`를 못 쓰는 두 가지 대표 상황

1. **private 생성자 + 팩토리**: `make_unique`는 라이브러리의 자유 함수라서 내부에서 `new T()`를 호출한다. 접근 제어(access control)는 호출 위치 기준이므로 **클래스 멤버가 아닌 make_unique는 private 생성자에 접근 불가** → 컴파일 에러. 클래스 자신의 static 멤버 함수 안에서 `unique_ptr<T>(new T())`로 직접 감싸는 것이 정석 (Winters 전역 패턴, §4.2).
2. **이미 생성된 객체의 소유권 인수**: 객체가 이미 존재하면 make 계열은 의미가 없다. `shared_ptr<T>(existing.release())`처럼 raw 포인터로부터 구성하며, 이때 제어 블록만 새로 할당된다 (§4.3).

### 3.2 deleter의 위치 차이 — `unique_ptr` vs `shared_ptr`

- `unique_ptr<T, D>`: deleter가 **템플릿 파라미터(타입의 일부)**. 다른 deleter면 다른 타입 → 오버헤드 없음, 대신 타입 시그니처가 전파된다.
- `shared_ptr<T>`: deleter가 **제어 블록에 type-erased로 저장**. 어떤 deleter로 만들었든 `shared_ptr<T>` 타입은 동일 → 유연하지만 간접 호출 비용.
- 이 차이 때문에 `shared_ptr`는 "생성 시점의 완전 타입"으로 deleter를 캡처해 두므로, 이후 불완전 타입 상태에서 파괴돼도 안전하다. `unique_ptr`는 **파괴 지점에서 완전 타입(complete type)을 요구**한다 → §3.4.

### 3.3 `enable_shared_from_this` 내부

- 내부에 `mutable weak_ptr<T> weak_this` 멤버를 숨겨 두고, **shared_ptr 생성자가 이 베이스를 감지하면 weak_this를 초기화**한다.
- `shared_from_this()`는 `weak_this.lock()`으로 **기존 제어 블록을 공유하는** shared_ptr를 반환한다. `shared_ptr<T>(this)`를 직접 만들면 제어 블록이 두 개가 되어 이중 해제(double delete) — 이것을 막는 장치다.
- 함정: 아직 shared_ptr로 관리되기 전(생성자 안)이나 shared_ptr 없이 만들어진 객체에서 부르면 C++17부터 `bad_weak_ptr` 예외. 그래서 이 패턴은 **"생성자를 private로 막고 팩토리가 shared_ptr만 반환"과 세트**로 써야 안전하다 (§4.5).

### 3.4 불완전 타입과 `unique_ptr` — 소멸자를 .cpp로 보내는 이유

`unique_ptr<Fwd>` 멤버를 가진 클래스의 소멸자가 헤더에서 (암시적으로든 `=default`로든) 인스턴스화되면, 그 지점에서 `default_delete<Fwd>::operator()`가 `delete p`를 컴파일해야 하는데 `Fwd`가 불완전하면 `static_assert(sizeof(T) > 0)`에 걸린다. 해법은 **헤더에는 소멸자 선언만, 정의는 완전 타입이 보이는 .cpp에** 두는 것. 이것이 pimpl 관용구의 컴파일 방화벽(compilation firewall)과 정확히 같은 메커니즘이다 (§4.4).

### 3.5 `unique_ptr<T[]>` 배열 특수화

- `unique_ptr<T[]>`는 `delete[]`를 호출하고 `operator[]`를 제공한다. `unique_ptr<T>`에 `new T[n]`을 넣으면 `delete`가 호출되어 UB.
- `vector`와 달리 크기를 안 들고 재할당이 없다 — "고정 크기 동적 배열을 RAII로"가 정확한 용처 (§4.2의 EntityBlueprintRegistry).

### 3.6 COM `ComPtr` — 침습적(intrusive) refcount와의 비교

| | `shared_ptr` | `Microsoft::WRL::ComPtr` |
|---|---|---|
| refcount 위치 | 외부 제어 블록 (비침습적) | **객체 내부** — `IUnknown::AddRef/Release` (침습적) |
| 추가 할당 | 제어 블록 1회 | 없음 (객체가 이미 카운터 보유) |
| weak 지원 | `weak_ptr` | 없음 (COM엔 별도 메커니즘) |
| raw에서 재구성 | 제어 블록 중복 위험 | **안전** — 카운터가 객체 안에 있어 어디서 감싸도 동일 |
| 타입 변환 | `dynamic_pointer_cast` | `.As()` = `QueryInterface` |

침습적 방식의 본질적 장점: refcount가 객체 자신에 있으므로 "raw pointer만 받아도 소유권 참여 가능". 단점: 타입이 그 규약(IUnknown 상속)을 강제당한다. `boost::intrusive_ptr`가 같은 계열이다. out-parameter는 `GetAddressOf()`(내부 포인터 주소), 조기 해제는 `Reset()`.

### 3.7 성능 요약 — 면접에서 숫자 감각으로 말할 것

- `unique_ptr`: raw와 동일 (deleter 무상태 전제).
- `shared_ptr` 복사: 원자적 증가 1회 — 단일 스레드에선 수 ns지만, 여러 스레드가 같은 제어 블록을 복사하면 캐시 라인 소유권이 코어 사이를 튀며 수십 배 느려질 수 있다.
- 그래서 게임 hot loop에선 shared_ptr를 **프레임 경계에서 한 번 잡고, 루프 내부는 raw/참조로 순회**하는 것이 관례다.

---

## 4. Winters에서의 적용

### 4.1 "소유는 스마트 포인터, 관찰은 raw" 규약

- `Engine/Public/Core/Timer_Manager.h:26` — `map<wstring_t, unique_ptr<CTimer>> m_Timers;`가 유일한 소유자이고, 내부 조회 `Find_Timer`(같은 파일 :23)는 `CTimer*`를 반환한다. 주의: `std::map`은 노드 기반이라 **원소 주소가 원래 안정적**이다(rehash는 unordered_map 얘기고, 그마저 참조/포인터는 유지 — [08_stl_containers_cache.md](08_stl_containers_cache.md) 무효화 표). 여기서 unique_ptr의 역할은 주소 안정성이 아니라 **단독 소유권의 타입 표기**다. unique_ptr 한 겹의 간접성이 주소 안정성에 실제로 기여하는 곳은 재할당이 있는 vector 쪽 — `EngineSDK/inc/Core/JobSystem.h:87`의 `vector<unique_ptr<CWorkStealingDeque<WorkItem>>>`은 vector가 재할당돼도 도둑 스레드들이 참조하는 deque의 힙 주소가 불변이다. 이 두 근거를 갈라 말해야 raw 관찰자가 안전한 구조적 근거가 된다.
- `Engine/Public/Resource/ResourceCache.h:38-40` — 같은 캐시 안에서 소유권 모델을 자원 성격별로 가른다: 텍스처는 `unordered_map<wstring, unique_ptr<CTexture>>`(캐시 단독 소유, `ResourceCache.cpp:28`의 `return it->second.get()`으로 비소유 raw 반환), 모델은 `unordered_map<string, shared_ptr<CModel>>`(씬 전환 중에도 렌더러가 들고 있을 수 있어 공유 소유). "unique vs shared 언제 쓰나"에 대한 실코드 답변.
- `Engine/Public/Resource/Model.h:115-122` — 한 클래스 안의 소유/관찰 매트릭스: 자기가 로드한 텍스처는 `vector<unique_ptr<CTexture>> m_vecTextures`로 소유, 외부(ModelRenderer)가 소유한 오버라이드는 `CTexture* m_pOverrideTexture; // 비소유 (ModelRenderer가 소유)` raw 관찰. 주석으로 소유자를 명시하는 습관까지.
- `Engine/Public/Renderer/CMaterialPBR.h:46-49` — `CTexture*` 관찰자 3개 + `unique_ptr<DX11ConstantBuffer<...>>` 소유 1개. 그리고 :19-22에서 복사는 `=delete`, 이동은 `noexcept =default` — **unique_ptr 멤버가 이동 가능하므로 컴파일러 생성 이동이 그대로 올바르다**(rule of five의 실전형).
- `Client/Private/GameObject/ChampionSpawnService.cpp:183-197, 231, 290-293` — 스폰 함수 하나에 소유권 수명 주기가 다 있다: `make_unique<ModelRenderer>` 생성 → `Initialize` 실패 시 그냥 `return result;`(지역 unique_ptr가 자동 파괴, 누수 없는 early-return) → 컴포넌트에는 `render.pRenderer = pRenderer.get();`으로 비소유 관찰만 심고 → `context.renderers[entity] = std::move(pRenderer);`로 소유권은 씬 소유 map 한 곳에만 이전.

### 4.2 private 생성자 + `Create()` 팩토리 — make_unique가 안 되는 이유

- `Engine/Private/Core/CTimer.cpp:40-48` + `Engine/Public/Core/CTimer.h:9` — 생성자가 private이고 `Create()`가 `unique_ptr<CTimer>(new CTimer())`로 감싼 뒤 `Initialize()`의 `HRESULT` 실패를 `nullptr` 반환으로 변환한다. **make_unique는 라이브러리 코드라 private 생성자에 접근 불가**하므로 클래스 멤버 안에서 직접 `new`를 감싸는 것. 예외 없는 코드베이스에서 "반쯤 생성된 객체가 밖으로 새지 않게" 하는 2단계 초기화(two-phase init)의 캡슐화다.
- 같은 패턴이 계층 전체에 반복된다: `Engine/Private/RHI/DX11/CDX11Device.cpp:953-963`, `Engine/Private/ECS/Systems/EntityBlueprintRegistry.cpp:59-64`, `Client/Private/Network/Backend/CHttpClient.cpp:21-28`(+ `CHttpClient.h:41`의 private ctor). "한 번 본 트릭"이 아니라 코드베이스 규약임을 어필할 수 있다.
- `Engine/Public/ECS/Systems/EntityBlueprintRegistry.h:39` — `std::unique_ptr<std::unordered_map<std::wstring, CEntityBlueprint>[]> m_pBlueprints;` 씬 개수만큼의 고정 배열을 **배열 특수화 `unique_ptr<T[]>`**로 소유해 `delete[]`를 자동화한 실사례 (§3.5).

### 4.3 `unique_ptr` → `shared_ptr` 소유권 이관

- `Engine/Private/Resource/ResourceCache.cpp:67` — `shared_ptr<CModel> shared(pModel.release());`. `CModel::Create`는 unique_ptr를 반환하지만 캐시는 공유 소유가 필요하다. `release()`로 unique_ptr가 소유권을 놓고, 그 raw 포인터로 shared_ptr를 구성 — **객체는 이미 존재하므로 make_shared는 불가, 제어 블록만 새로 붙는다** (§3.1-2). (참고: `shared_ptr<CModel>(std::move(pModel))` 변환 생성자로도 같은 결과가 되며, deleter까지 보존된다.)

### 4.4 불완전 타입 + out-of-line 소멸자 (pimpl 방화벽)

- `Engine/Private/RHI/DX11/CDX11Device.h:118,160` — `struct ResourceTables;` 전방선언만 하고 `std::unique_ptr<ResourceTables> m_pTables;`로 보관(실체는 .cpp에). 헤더의 :47은 `~CDX11Device() override;` **선언만**, 정의는 `CDX11Device.cpp:732`의 `CDX11Device::~CDX11Device() = default;` — default 정의조차 완전 타입이 보이는 .cpp에서 해야 한다는 것이 포인트 (§3.4).
- `Engine/Include/GameInstance.h:174-183` — 매니저 8종을 `unique_ptr<class CTimer_Manager>` 식의 인라인 전방선언으로 소유. 소멸자는 `Engine/Private/GameInstance.cpp:24`에서 정의. 엔진의 최상위 헤더가 매니저 헤더들을 include하지 않게 하는 **컴파일 의존성 절단**이 목적이다.
- `Server/Public/Game/GameRoom.h:207-220` + `Server/Private/Game/GameRoom.cpp:139` — 서버도 동일: `CNavGrid`, `CSnapshotBuilder` 등 전방선언 서브시스템 10여 개를 unique_ptr로 소유하고 `~CGameRoom()`은 .cpp에서 정의.

### 4.5 `shared_ptr` + `enable_shared_from_this` — 서버 세션

- `Server/Public/Network/Session.h:15-18,57` — `class CSession : public std::enable_shared_from_this<CSession>`, 생성자는 private(:57), `static std::shared_ptr<CSession> Create(...)`(:18, 구현 `Session.cpp:7-10`)만이 인스턴스를 만든다. **스택/전역 생성을 원천 봉쇄해 "반드시 shared_ptr 관리 하에 태어난다"를 강제** — §3.3의 bad_weak_ptr 함정을 설계로 제거한 조합이다.
- 세션 수명의 실제 공유자는 `Server/Public/Network/Session_Manager.h:34-35`의 `unordered_map<u32_t, shared_ptr<CSession>>` + `m_closingSessions` 벡터. IOCP 비동기 IO가 완료될 때까지 세션이 살아 있어야 하므로 단독 소유(unique)로는 모델링이 안 되고, `enable_shared_from_this` 상속은 콜백 체인에서 자기 자신의 shared_ptr를 제어 블록 중복 없이 재획득할 수 있게 하는 안전판이다.

### 4.6 소유 `shared_ptr` + 비소유 raw 뷰의 동거

- `Client/Public/GameObject/FX/FxBillboardComponent.h:26-27,107-118` — ECS 컴포넌트 스토어에 **값 복사로 저장되는** struct가 `const wchar_t* texturePath`(뷰) + `shared_ptr<const wstring_t> texturePathOwner`(소유)를 쌍으로 든다. `SetTexturePath`가 `make_shared` 후 `texturePath = texturePathOwner->c_str();`로 연결. 컴포넌트가 복사되면 shared_ptr refcount가 올라가 **버퍼가 살아 있는 한 raw 뷰도 유효** — const 불변 버퍼라 공유가 안전하다는 조건까지 갖춘, "복사되는 객체 안의 포인터 멤버" 문제의 해법.

### 4.7 `this` 캡처 수명 사고 — CHttpClient

- `Client/Private/Network/Backend/CHttpClient.cpp:90-92,148-156` — 과거 버그: `std::async(launch::async, ...)`의 반환 `future`를 버리면 **임시 future의 소멸자가 작업 완료까지 join하며 블로킹**해 비동기가 사실상 동기가 된다(주석으로 박제, gotcha 2026-07-09). 그리고 그 "우연한 동기화"가 람다의 raw `this` 캡처를 안전하게 보이게 만들고 있었다.
- 수정: `future`를 `vector<future<void>> m_PendingRequests`(`CHttpClient.h:71`)가 소유하고, 소멸자(`CHttpClient.cpp:103-117`)가 mutex 하에 벡터를 swap으로 빼낸 뒤 전부 `wait()`로 드레인 — **워커가 `this`를 만지는 동안 객체가 파괴되지 않는다는 계약을 소멸자가 명시적으로 보장**하고, 블로킹은 파괴 시점 한 번으로 좁혔다. "람다에 this를 캡처하면 무엇을 보장해야 하나"라는 질문의 실전 답이다.

### 4.8 `ComPtr` — DX11 리소스의 침습적 refcount

- `Engine/Private/RHI/DX11/CDX11Device.h:129-134,146-148` — device/context/swapchain/RTV/DSV와 GPU 타이밍 쿼리까지 전부 `Microsoft::WRL::ComPtr`로 보관해 `AddRef/Release`를 자동화. 사용부(`CDX11Device.cpp:968` 등)는 `Get()`/`GetAddressOf()`로 raw COM 인터페이스를 넘긴다. shared_ptr와의 차이(§3.6) — **refcount가 IUnknown 안에 있는 침습적 방식이라 제어 블록이 없다** — 를 실코드로 시연할 수 있다.

### 4.9 (경계 사례) `unique_ptr` 멤버가 복사를 지우는 것 — dllexport와의 상호작용

- `Engine/Public/ECS/World.h:52-56` — `// unique_ptr 멤버 때문에 복사 불가, 이동만 허용` 주석과 함께 copy `=delete` / move `=default`.
- `Engine/Public/ECS/SystemScheduler.h:17-25`, `EngineSDK/inc/Core/JobSystem.h:33-34` — dllexport 클래스는 MSVC가 특수 멤버를 강제 인스턴스화하려다 unique_ptr의 deleted copy와 충돌하므로 **copy를 명시적으로 `=delete` 선언해 인스턴스화 자체를 스킵**시킨다. 상세는 [02_compile_link_dll.md](02_compile_link_dll.md)와 [05_class_design_value_semantics.md](05_class_design_value_semantics.md) 챕터 담당이지만, "unique_ptr 멤버 → 클래스가 move-only로 전파된다"는 이 챕터의 핵심 귀결이다.

---

## 5. 면접 Q&A

**Q1. 포인터와 참조의 차이는? 언제 무엇을 쓰나?**
- 핵심: null 가능/재지정/산술 = 포인터만. 참조는 유효 객체 별명 계약, 구현은 대개 포인터와 동일. "없을 수 있다" → 포인터, "반드시 있다" → 참조. 참조도 수명 관점에선 댕글링 가능.
- Winters 연결: 함수 매개변수는 `CWorld& world`(필수) vs `CTexture* pTexture`(nullable 관찰)로 구분해 쓴다 — `CMaterialPBR.h:26`의 setter들이 raw 포인터를 받는 이유는 "nullptr = 텍스처 해제"까지 표현하기 위해서다.

**Q2. unique_ptr와 shared_ptr는 각각 언제 쓰나?**
- 핵심: 기본은 unique(비용 0, 소유자 1명 명시). 수명이 여러 주체에 걸쳐 마지막 사용자가 정적으로 결정 안 될 때만 shared. shared는 제어 블록 할당 + 원자 refcount 비용.
- Winters 연결: `ResourceCache.h:38-40` — 텍스처(캐시 단독 소유, raw 반환) vs 모델(씬 전환 중 공유 소유). 같은 캐시에서 자원 수명 계약에 따라 갈라 쓴 근거를 말한다.

**Q3. shared_ptr의 내부 구조와 비용을 설명하라.**
- 핵심: 포인터 2개(객체+제어 블록), 제어 블록에 strong/weak count + type-erased deleter. 복사마다 원자적 증가 → 멀티스레드 캐시 라인 경합. refcount만 thread-safe, 객체 접근은 아님. make_shared = 1회 할당 + weak 잔존 시 메모리 지연 해제.
- Winters 연결: hot path인 렌더 루프에선 `shared_ptr<CModel>`을 매 프레임 복사하지 않고 참조/raw로 순회한다.

**Q4. make_unique를 못 쓰는 경우가 있나?**
- 핵심: private 생성자 + 팩토리. 접근 제어는 호출 지점 기준이라 라이브러리 함수인 make_unique 내부의 `new T()`는 private에 접근 불가. 클래스 static 멤버 안에서 `unique_ptr<T>(new T())`로 감싼다. (또 하나: 이미 존재하는 객체의 인수 — make 계열 무의미.)
- Winters 연결: `CTimer.cpp:40-48`, `CDX11Device.cpp:953-963`, `CHttpClient.cpp:21-28` — 코드베이스 전역 규약. 덤으로 `EntityBlueprintRegistry.h:39`의 `unique_ptr<T[]>`(delete[] 자동화)까지 이으면 가산점.

**Q5. unique_ptr 멤버가 있는데 소멸자를 왜 .cpp에 정의해야 하나?**
- 핵심: 기본 deleter의 `delete p`는 완전 타입 요구(`sizeof` 검사). 헤더에서 소멸자가 인스턴스화되면 전방선언 타입에서 컴파일 에러. 선언은 헤더, 정의(=default 포함)는 완전 타입이 보이는 .cpp — pimpl 방화벽과 동일 메커니즘.
- Winters 연결: `CDX11Device.h:118` + `CDX11Device.cpp:732`, `GameInstance.h:174-183` + `GameInstance.cpp:24`, `GameRoom.h:207-220` + `GameRoom.cpp:139`.

**Q6. 순환 참조는 어떻게 발견하고 어떻게 끊나?**
- 핵심: shared 양방향 → strong count가 0에 못 감 → 소멸자 미호출 누수. 관찰 방향을 weak_ptr로, 사용 시 `lock()`으로 원자적 승격. 발견은 "소멸자 로그/카운터가 안 찍힌다"에서 시작.
- Winters 연결: 서버 세션은 `Session_Manager`의 map(소유)과 IO 콜백(임시 공유)만 shared를 들고, 세션이 매니저를 shared로 되잡지 않는 단방향 구조라 순환이 생기지 않는다.

**Q7. enable_shared_from_this는 왜 필요하고 어떻게 동작하나?**
- 핵심: 콜백/비동기에서 `this`의 shared_ptr가 필요할 때 `shared_ptr<T>(this)`를 만들면 제어 블록 2개 → 이중 해제. 베이스의 숨은 weak_ptr를 shared_ptr 생성자가 초기화하고 `shared_from_this()`가 `lock()`으로 기존 제어 블록을 공유. shared 관리 전에 부르면 bad_weak_ptr → private ctor + shared 반환 팩토리와 세트.
- Winters 연결: `Session.h:15-18` — IOCP 세션이 정확히 이 조합. "왜 생성자를 private로 막았나"까지 한 호흡으로.

**Q8. 그럼 raw pointer는 언제 써도 되나?**
- 핵심: 비소유 관찰. 소유자가 스마트 포인터로 명확하고 관찰자 수명 < 소유자 수명이 구조적으로 보장될 때. "raw = 위험"이 아니라 "raw = 나는 delete 책임 없음"이라는 타입 시그널.
- Winters 연결: `Timer_Manager.h:23,26`(소유 map + raw 조회), `Model.h:122`(주석으로 소유자 명시), `ChampionSpawnService.cpp:231,290`(get() 관찰 + move 소유 이전).

---

## 6. 흔한 오답 / 함정

1. **"shared_ptr는 thread-safe하다"** — 반쪽 진실. 제어 블록의 refcount 증감만 원자적이다. 같은 `shared_ptr` 변수 하나를 두 스레드가 동시에 재할당하거나, 가리키는 객체를 동시에 수정하는 것은 data race다.
2. **"참조는 안전하고 포인터는 위험하다"** — 참조는 null 안전일 뿐 수명 안전이 아니다. 지역 변수 참조 반환, 컨테이너 재할당 후의 요소 참조 모두 댕글링. 위험의 근원은 문법이 아니라 수명 계약이다.
3. **`shared_ptr<T>(this)` 직접 생성** — 제어 블록이 하나 더 생겨 이중 해제. `enable_shared_from_this` + `shared_from_this()`가 정답이고, 그마저 shared 관리 전이면 bad_weak_ptr — 그래서 Winters의 CSession처럼 팩토리로 생성 경로를 봉쇄한다.
4. **"unique_ptr를 컨테이너에 넣으면 요소를 raw로 참조하면 안 된다"** — 반대다. 가리켜지는 힙 객체 주소가 안정적이라 raw 관찰이 오히려 안전해진다. 단 근거는 컨테이너별로 다르다: `JobSystem.h:87`의 `vector<unique_ptr<CWorkStealingDeque>>`는 vector 재할당에도 **unique_ptr 한 겹의 간접성 덕에** 도둑 스레드들이 참조하는 deque 주소가 불변이고, `Timer_Manager.h:26`의 map은 노드 기반이라 원소 주소가 원래 안정적이다(§4.1). 위험한 것은 **unique_ptr 원소 자체가 erase될 때**다.
5. **async의 future를 버리고 "비동기니까 this는 안 잡아도 됨"** — `std::async(launch::async)` 반환 future를 버리면 임시 future 소멸자가 join해 동기화되고, 그 우연이 raw this 캡처를 가려준다. future를 진짜로 소유하는 순간(=진짜 비동기가 되는 순간) this 수명 보장은 온전히 내 책임이 된다 — `CHttpClient.cpp:90-117`이 이 사고와 수습의 전 과정이다.

---

## 다른 챕터와의 연결

- [03_memory_lifetime_raii.md](03_memory_lifetime_raii.md) — 스마트 포인터의 토대인 RAII, 스택/힙 수명, early-return 안전성.
- [05_class_design_value_semantics.md](05_class_design_value_semantics.md) — unique_ptr 멤버가 만드는 move-only 전파, rule of five, copy=delete/move=default.
- [02_compile_link_dll.md](02_compile_link_dll.md) — 불완전 타입 pimpl 방화벽, dllexport 특수 멤버 강제 인스턴스화와 unique_ptr의 충돌.
- [09_concurrency.md](09_concurrency.md) — 원자적 refcount 비용, future 소유권, this 캡처와 스레드 수명 계약.
