# C++의 본질과 철학

> 면접 첫 질문 "C++을 어떻게 이해하고 있나요?"에 대한 답. 각론(RAII·스마트 포인터·동시성)은 뒤 챕터로 미루고, 여기서는 그 각론들이 왜 전부 하나의 철학에서 파생되는지를 관통한다.

---

## ① 한 줄 본질

> **"C++은 '쓰지 않는 기능에는 비용을 내지 않고(zero-overhead), 쓰는 기능은 손으로 짠 C만큼 빠르게' 만들면서, 그 위에 클래스·타입·소멸자 같은 추상화를 얹어 사람이 관리 가능한 규모의 코드를 짜게 해주는 언어다. 핵심 3축은 제로 오버헤드 추상화(zero-overhead abstraction), 결정적 소멸(deterministic destruction), 값 의미론(value semantics)이고, 이 셋이 하드웨어에 직결된 메모리 모델 위에서 동작하기 때문에 게임 엔진이 C++을 선택한다."**

Bjarne Stroustrup의 제로 오버헤드 원칙 두 문장으로 요약된다.
1. 쓰지 않는 것에 대해서는 비용을 내지 않는다 (What you don't use, you don't pay for).
2. 쓰는 것은, 손으로 그보다 더 잘 짤 수 없다 (What you do use, you couldn't hand-code any better).

---

## ② 기본 개념 — C++이 어떤 언어인가

### 컴파일 언어(compiled language)로서의 정체성
C++ 소스는 실행 전에 **기계어(machine code)로 완전히 번역**된다. Java/C#처럼 바이트코드로 컴파일한 뒤 런타임 VM이 JIT로 다시 변환하는 것이 아니고, Python처럼 인터프리터가 한 줄씩 읽는 것도 아니다. 파이프라인은:

```
전처리(preprocessor) → 컴파일(각 .cpp = 번역 단위 TU → .obj) → 링크(linker → .exe/.dll)
```

이 구조가 C++의 거의 모든 "함정 질문"의 뿌리다. 헤더/매크로/ODR/DLL 경계 문제는 전부 "번역 단위는 서로를 모른 채 각자 컴파일되고, 나중에 링커가 심볼로 이어붙인다"는 사실에서 나온다.

### 값 의미론(value semantics) — C++의 기본은 "복사"
Java/C#에서 `Foo a = b;`는 대개 **같은 객체를 가리키는 참조 두 개**다. C++에서 `Foo a = b;`는 **b의 내용을 통째로 복사한 독립된 객체**다. 변수는 기본적으로 메모리에 직접 박히는 값이지, 힙 객체를 가리키는 핸들이 아니다. 이 한 가지 차이가:
- 스택에 객체를 놓을 수 있게 하고 (→ 결정적 소멸의 전제),
- 배열에 객체가 연속으로 박히게 하고 (→ 캐시 지역성),
- "누가 이걸 소유하는가(ownership)"를 언어 차원의 질문으로 만든다.

이동 의미론(move semantics, C++11)은 값 의미론의 성능 구멍(불필요한 복사)을 메우는 장치다. 값 의미론을 버리는 게 아니라 완성한다.

### 결정적 소멸(deterministic destruction)과 RAII
객체가 스코프를 벗어나는 **정확히 그 지점**에서 소멸자(destructor)가 호출된다. GC 언어처럼 "언젠가 수거된다"가 아니다. 이 결정성이 RAII(Resource Acquisition Is Initialization)를 가능케 한다 — 생성자에서 자원(메모리·파일·락·GPU 핸들)을 잡고 소멸자에서 놓으면, 예외가 나든 조기 return이든 자원 해제가 **컴파일러가 삽입한 코드로 보장**된다. finally 블록이 필요 없다.

### 하드웨어 직결 메모리 모델(memory model)
C++ 객체는 명확한 크기·정렬(alignment)·레이아웃(layout)을 갖고 메모리에 놓인다. 포인터는 실제 주소다. 비트를 밀고(shift), 마스킹하고, 구조체를 바이트로 memcpy하고, SIMD 레지스터에 로드할 수 있다. C++11부터는 멀티스레드 하에서 무엇이 정의된 동작인지를 규정하는 **표준 메모리 모델**(atomics, memory_order)까지 언어에 들어왔다. "하드웨어를 추상화하지만 하드웨어를 숨기지는 않는다."

### C vs C++ vs 관리형 언어
| 축 | C | C++ | Java/C# (managed) |
|---|---|---|---|
| 추상화 | 거의 없음 | 제로 오버헤드 추상화 | 런타임 비용 있는 추상화 |
| 메모리 해제 | 수동 free | RAII(결정적) | GC(비결정적) |
| 기본 변수 | 값 | 값 | (대개) 참조 |
| 다형성 | 함수 포인터 수동 | vtable/템플릿/함수포인터 선택 | 항상 vtable+런타임 |
| 실행 | 네이티브 | 네이티브 | VM/JIT |

C++은 "C의 성능과 제어"에 "관리 가능한 추상화"를 더하되, **추상화의 비용을 프로그래머가 선택**하게 한다. virtual을 쓰면 vtable 비용을 내고, 안 쓰면 안 낸다. 이게 관리형 언어와의 결정적 차이다.

### 게임 엔진이 왜 C++인가
1. **프레임 예산이 밀리초 단위다.** 16.6ms(60fps) 안에 시뮬레이션+렌더를 끝내야 하는데, GC가 임의 시점에 수십 ms를 멈추면 프레임이 튄다. 결정적 소멸은 스톨(stall) 없는 자원 관리를 준다.
2. **캐시가 성능을 지배한다.** 값 의미론+연속 메모리로 데이터 지향(data-oriented) 레이아웃을 짜야 캐시 미스를 줄인다.
3. **하드웨어에 직접 말해야 한다.** GPU API(DirectX/Vulkan), SIMD, 메모리 정렬은 C++이 그대로 노출한다.
4. **결정론(determinism)이 필요하다.** 네트워크 게임에서 클라와 서버 시뮬이 비트 단위로 일치해야 하는데, 이는 부동소수 모드·정수 틱·플랫폼 독립 난수까지 손으로 통제해야 가능하다.

---

## ③ 심화 — 꼬리질문 대비

**"제로 오버헤드가 정말 '오버헤드 0'인가?"**
아니다. "추상화 자체가 추가 비용을 만들지 않는다"는 뜻이다. `std::vector`는 손으로 짠 동적 배열보다 느리지 않지만, 동적 배열 자체의 비용(할당·bounds)은 여전히 있다. 반례도 존재한다 — `std::function`은 타입 소거(type erasure)로 간접 호출과 힙 할당을 유발하므로 템플릿 콜백보다 오버헤드가 있다. "제로 오버헤드 원칙"은 언어 설계 목표이지 모든 라이브러리가 지키는 보장이 아니다.

**"컴파일 언어인데 왜 RTTI·가상함수 같은 런타임 요소가 있나?"**
C++은 "정적으로 결정 가능한 것은 컴파일 타임에, 런타임에만 알 수 있는 것은 런타임에"라는 노선이다. 다형성이 필요하면 vtable을 쓰되, **그 비용을 낼지 말지를 프로그래머가 `virtual` 키워드로 선택**한다. 안 쓰면 클래스에 vtable 포인터조차 안 생긴다.

**"값 의미론이 좋으면 왜 포인터·참조가 필요한가?"**
공유(sharing)·다형성·큰 객체 전달·소유권 이전 때문이다. 값이 기본이고 간접(indirection)은 명시적 선택이라는 게 요점. Java는 반대로 간접이 기본이라 "언제 복사되는지"가 불투명하다.

**"결정적 소멸의 단점은?"**
소멸 순서를 프로그래머가 책임져야 한다. 순환 참조(shared_ptr cycle)는 자동 회수되지 않고, 전역/정적 객체의 소멸 순서(static destruction order fiasco)는 미정의에 가깝다. GC는 이 짐을 덜어주는 대신 결정성을 포기한다 — 트레이드오프다.

---

## ④ Winters에서의 적용 — 다섯 축을 관통하는 증거

이 프로젝트 전체가 위 철학의 체화다. 축별로 대표 코드를 든다.

### (1) 제로 오버헤드 추상화 — 타입 안전을 공짜로
`Engine/Public/RHI/RHIHandles.h:6`의 `RHIHandle<TTag>`는 **팬텀 타입(phantom type)**이다. 내부 표현은 그냥 `u64_t value`(9행) 하나인데, 태그 구조체(`RHIBufferTag`, `RHITextureTag` … 49~56행)를 템플릿 인자로만 다르게 주어 `RHIBufferHandle`과 `RHITextureHandle`을 **서로 대입 불가능한 별개 타입**으로 만든다. 런타임 크기·비용은 정수 하나와 완전히 동일한데, 텍스처 핸들을 버퍼 슬롯에 잘못 넘기는 실수를 **컴파일 타임에** 차단한다. 이것이 "제로 오버헤드 추상화"의 교과서적 실물 — 추상화(강타입)를 얻되 비용은 0.

`static_assert`로 계약을 컴파일 타임에 박는 것도 같은 정신이다. `Engine/Private/RHI/DX11/DX11ConstantBuffer.h:15`는 `static_assert(sizeof(T) % 16 == 0, ...)`로 HLSL cbuffer의 16바이트 정렬 규칙을 **런타임이 아니라 빌드에서** 강제한다. 어긋난 구조체는 컴파일조차 안 된다 → 런타임 비용 0, 버그 조기 검출.

"쓰지 않는 것에 비용 안 낸다"의 반대편도 있다. `Client/Private/Network/Client/SnapshotApplier.cpp:702`의 `if constexpr (kSnapshotMinionYawDebugOutput)`는 상수가 false면 그 안의 디버그 트레이스 블록을 **아예 코드 생성조차 하지 않는다**(일반 `if`와 달리 죽은 가지가 컴파일 산출물에서 사라진다). 릴리스에서 진단 비용 = 0.

### (2) 결정적 소멸 / RAII — 그리고 안 지킨 코드의 대가
`Engine/Private/RHI/RHITextureLoader.cpp:14`의 `CScopedCOMInit`은 생성자에서 `CoInitializeEx`, 소멸자에서 `CoUninitialize`를 호출하는 스코프 가드다. `m_bNeedsUninit`(30행)로 **초기화가 성공했을 때만** 해제하도록 짝을 맞춘다. 함수 어디서 return하든 COM 언init이 보장된다 — finally 없는 C++의 관용구.

같은 철학의 완성형이 스마트 포인터다. `Engine/Private/RHI/DX11/CDX11Device.h:129`의 디바이스 멤버들은 전부 `Microsoft::WRL::ComPtr<...>`(129~134행)로, COM 참조 카운트를 소멸자가 자동으로 Release한다.

**대조군이 같은 코드베이스에 있다는 게 교육적이다.** `Engine/Private/RHI/DX11/DX11Buffer.h:62`의 `DX11Buffer`는 raw `ID3D11Buffer*` 멤버(62~63행)를 들고 소멸자(26행)에서 수동 `Release()`를 하는데, **복사 생성자/대입을 선언하지 않았다** → 3의 법칙(rule of three) 위반. 이 객체를 실수로 복사하면 두 인스턴스가 같은 포인터를 물고 각자 소멸자에서 Release → 이중 해제(double free). ComPtr/unique_ptr는 이 실수를 **애초에 불가능하게** 만든다. 면접에서 "왜 스마트 포인터인가"를 이 한 파일 대비로 답할 수 있다.

### (3) 값 의미론 — POD 컴포넌트와 캐시
`Engine/Public/ECS/Components/CoreComponents.h:10`의 컴포넌트들(`VelocityComponent`, `HealthComponent` …)은 가상함수·상속·힙 멤버가 없는 **순수 POD 구조체**다. `AbilityComponent`(51행)조차 `std::vector` 대신 `uint32_t skillIDs[MAX_SKILLS]`(53~54행) **고정 배열**을 쓴다. 이유: 값 복사·`memcpy`·직렬화가 자명해지고(trivially copyable), 힙 간접참조가 없어 캐시에 연속으로 박힌다.

이 값들이 어디에 담기는지가 `Engine/Public/ECS/ComponentStore.h:8`의 sparse-set이다. 실제 데이터는 `m_vecData`라는 **연속 배열(dense array)**에 SoA로 쌓이고, 삭제는 swap-and-pop(24~38행)으로 O(1)이다. 값 의미론이 없으면 이 레이아웃 자체가 성립하지 않는다 — 참조가 기본인 언어라면 배열엔 포인터만 들어가고 실제 객체는 힙에 흩어진다.

### (4) 하드웨어 직결 메모리 모델 — 비트·정렬·SIMD
`Engine/Private/Manager/Navigation/NavGrid.cpp:98`은 셀당 1비트로 walkability를 저장하고 `m_vecBits[iIdx >> 3] >> (iIdx & 7) & 1`로 읽는다. `>> 3`은 `/8`(바이트 인덱스), `& 7`은 `%8`(비트 위치)의 하드웨어 직결 표현. 262,144 셀을 32KB로 압축해 캐시에 올린다.

동시성 쪽 하드웨어 모델은 `Engine/Public/Core/JobSystem/WorkStealingDeque.h:91`에 있다. `alignas(64) std::atomic<std::int64_t> m_iBottom`과 `m_iTop`(91~92행)을 **캐시 라인(64B) 단위로 분리**해 false sharing을 막는다. 같은 파일의 `Push/Pop/Steal`은 `memory_order_relaxed/acquire/release`와 `seq_cst` 펜스(37·47·70행)를 연산별로 골라 쓴다 — C++11 메모리 모델을 "필요한 만큼만" 소비하는 정확한 예.

SIMD는 저장/연산 타입을 분리하는 규약으로 다룬다. `Engine/Include/WintersMath.h:9`의 헤더 주석이 원칙을 성문화한다: `Vec3/Mat4`(XMFLOAT*)는 저장용, `XMVECTOR/XMMATRIX`는 함수 내부 연산용 임시. 44행 `XMLoadFloat3(&reinterpret_cast<const XMFLOAT3&>(*this))`는 레이아웃 호환(layout-compatible) POD를 재해석해 SIMD 레지스터에 로드하는 경계다. 16바이트 정렬을 요구하는 `XMVECTOR`를 멤버로 두면 전역 데이터에서 정렬이 깨지기 쉬워, "저장은 XMFLOAT, 연산만 XMVECTOR"를 프로젝트 규칙으로 굳혔다.

### (5) 컴파일 언어 정체성 + 결정론 — 빌드가 곧 설계
`Engine/Include/WintersAPI.h:11`의 `WINTERS_ENGINE` 매크로는 `WINTERS_STATIC_BUILD`(EXE 툴) / `WINTERS_ENGINE_EXPORTS`(DLL 제작) / 그 외(DLL 소비) **3분기**로 `__declspec(dllexport)`·`dllimport`·빈 문자열을 토글한다. 같은 헤더가 세 종류 빌드에서 다르게 컴파일된다 — "번역 단위는 각자 컴파일되고 링커가 잇는다"는 컴파일 모델을 그대로 운용한 코드.

헤더 상수는 C++17 인라인 변수로 ODR-안전하게 둔다. `Engine/Include/WintersMath.h:77`의 `inline constexpr float kEpsilon = ...`(77~79행)은 여러 TU에 포함돼도 정의가 하나로 병합된다(매크로 → const → constexpr → inline constexpr 진화의 최종형).

결정론은 표준 라이브러리를 **의도적으로 거부**하는 데서 나온다. `Shared/GameSim/Core/Determinism/DeterministicRng.h:15`는 `std::random`을 안 쓰고 xorshift64를 손으로 짰다 — `std::uniform_int_distribution` 같은 분포는 결과가 **구현 정의(implementation-defined)**라 플랫폼 간 결정론이 깨지기 때문. `Shared/GameSim/Core/Determinism/DeterministicTime.h:9`는 `kFixedDt = 1/30`을 상수로 두되 **정수 틱을 진리의 원본**으로 삼아 float 누적 오차를 배제한다. 그리고 `Shared/GameSim/Include/GameSim.vcxproj:54`의 `<FloatingPointModel>Precise</FloatingPointModel>`은 `/fp:fast`의 재결합(reassociation) 최적화를 금지해 클라/서버 부동소수 연산을 비트 단위로 맞춘다. **결정론은 코드가 아니라 빌드 설정에서 시작된다** — 컴파일 언어이기에 가능한 통제.

여담으로, "잘못 통제하면 어떻게 되는가"의 사고 기록도 남아 있다. `Engine/Private/Core/CTimer.cpp:26`의 주석은 과거 델타타임을 16.7ms로 하드 클램프했다가 60fps 경계에서 게임 시간이 실제 시간보다 느려지는 슬로모션 스터터가 났음을 적고, 지금은 스파이크 상한(0.1s)만 둔다. 게임 루프에서 시간을 어떻게 다루는지를 실사고로 설명할 수 있다.

---

## ⑤ 면접 Q&A

**Q1. C++을 한 문장으로 정의한다면?**
- 핵심: "제로 오버헤드 추상화 위에서, 결정적 소멸과 값 의미론으로 자원과 성능을 프로그래머가 명시적으로 통제하는 컴파일 언어."
- 꼬리 대비: 세 축(추상화 비용 선택 / RAII / 값 기본)을 각각 한 문장으로 풀 수 있어야 한다.
- Winters: 팬텀 타입 핸들(제로 오버헤드), ComPtr vs raw 포인터 대조(RAII), POD 컴포넌트(값 의미론)로 실물 연결.

**Q2. "쓰지 않는 것에 비용을 내지 않는다"를 코드로 설명해 보라.**
- 핵심: `virtual`을 안 쓰면 vtable 포인터조차 안 생긴다. `if constexpr`로 죽은 가지는 코드 생성이 안 된다. 반대로 `std::function`은 타입 소거 비용을 내는 반례.
- Winters: `SnapshotApplier.cpp:702`의 `if constexpr` 디버그 블록 → 릴리스에서 완전 소거. `RHIHandle`은 강타입인데 크기·비용은 정수 하나.

**Q3. C#/Java 개발자에게 C++의 결정적 소멸을 어떻게 설명하겠나?**
- 핵심: GC는 "언젠가" 회수하지만 C++ 소멸자는 스코프 종료 "그 지점"에서 호출된다. 그래서 락·파일·GPU 핸들을 RAII로 묶으면 예외/조기 return에도 해제가 보장된다.
- 게임 관점: 프레임 도중 GC 스톨이 없다는 게 실시간 렌더링에 결정적.
- Winters: `CScopedCOMInit`(RHITextureLoader.cpp:14) — 함수 어느 경로로 나가도 CoUninitialize 보장.

**Q4. 값 의미론이 게임 성능에 왜 중요한가?**
- 핵심: 객체가 값이라 배열에 연속으로 박히고, 이게 캐시 지역성(cache locality)을 준다. 참조가 기본인 언어는 배열에 포인터만 있고 실제 객체는 힙에 흩어진다.
- Winters: `CoreComponents.h`의 POD 컴포넌트가 `ComponentStore.h`의 dense 배열에 SoA로 쌓임. `AbilityComponent`가 vector 대신 고정 배열을 쓰는 이유도 이것.

**Q5. 게임 엔진이 C++을 쓰는 이유를 GC 언어와 비교해 설명하라.**
- 핵심: (1) 프레임 예산 16.6ms에서 GC 스톨은 치명적 → 결정적 소멸. (2) 캐시 지배 → 값 의미론+데이터 지향. (3) GPU/SIMD 하드웨어 직결. (4) 네트워크 결정론.
- Winters: `/fp:precise`(GameSim.vcxproj:54)+정수 틱(DeterministicTime.h:9)+손수 짠 xorshift(DeterministicRng.h:15)로 클라/서버 시뮬 일치 → GC/JIT 언어로는 재현하기 어려운 통제.

**Q6. C++이 "하드웨어를 추상화하되 숨기지 않는다"는 말의 의미는?**
- 핵심: 포인터는 실제 주소, 구조체는 정해진 레이아웃, 비트 연산·memcpy·SIMD 로드가 다 가능하다. 동시에 클래스·템플릿 같은 추상화도 제공한다.
- Winters: 비트팩 walkability 그리드(`NavGrid.cpp:98`, `>>3 & 7`), `alignas(64)` false sharing 방지(`WorkStealingDeque.h:91`), SIMD 저장/연산 분리(`WintersMath.h:9`).

**Q7. C++은 컴파일 언어라는데, 그게 실제 코드에 어떤 제약을 만드나?**
- 핵심: 번역 단위(TU)가 서로를 모른 채 각자 컴파일되고 링커가 심볼로 잇는다. 그래서 헤더 상수는 ODR을 신경 써야 하고(→ inline constexpr), DLL 경계는 export/import 매크로가 필요하고, 매크로는 스코프를 무시한다.
- Winters: `WINTERS_ENGINE` 3분기 매크로(WintersAPI.h:11), `inline constexpr` 헤더 상수(WintersMath.h:77).

**Q8. "추상화의 비용을 프로그래머가 선택한다"를 예로 들라.**
- 핵심: 다형성이 필요하면 `virtual`(vtable), 컴파일 타임에 결정되면 템플릿(제로 비용), 열린 콜백이면 `std::function`(타입 소거 비용), 닫힌 분기면 함수 포인터 테이블/enum 디스패치. 같은 문제에 비용이 다른 도구가 여럿.
- Winters: RHI는 `IRHIDevice` 가상 인터페이스(런타임 다형성)를 쓰지만, ECS 순회는 템플릿 `ForEach`로 제로 오버헤드를 택한다. 도구 선택 자체가 설계 판단.

---

## ⑥ 흔한 오답 / 함정

1. **"제로 오버헤드 = 무조건 빠르다/오버헤드가 0이다"로 착각.**
   틀림. "추상화가 추가 비용을 만들지 않는다"는 상대적 진술이다. `std::vector`도 할당 비용은 있고, `std::function`·`std::shared_ptr`은 실제로 오버헤드가 있다. 원칙은 목표이지 모든 표준 도구의 보장이 아니다.

2. **"C++은 그냥 객체지향(OOP) 언어"라고 답하기.**
   OOP는 C++이 지원하는 여러 패러다임 중 하나일 뿐이다. 게임 엔진에서는 오히려 상속 계층을 피하고 데이터 지향(ECS)·값 의미론·템플릿 제네릭을 쓴다. Winters도 챔피언을 상속 계층이 아니라 컴포넌트+훅 등록으로 설계했다. "C++ = OOP"라 답하면 데이터 지향 문화를 모른다는 신호.

3. **결정적 소멸을 "delete를 안 해도 된다"로 오해.**
   RAII는 자동 free가 아니라 **소유권을 객체 수명에 묶는 것**이다. 순환 참조(shared_ptr cycle)는 회수되지 않고, 정적 객체 소멸 순서(static destruction order fiasco)는 여전히 위험하다. GC가 없는 대신 소멸 순서의 책임이 프로그래머에게 있다.

4. **"값 의미론이니 항상 값으로 넘겨라"로 과잉 일반화.**
   큰 객체를 값으로 넘기면 복사 비용이 크다. 그래서 `const&`(읽기)·이동(소유권 이전)·포인터(널 가능/비소유 관찰)를 상황별로 쓴다. "값이 기본"과 "항상 값 전달"은 다르다.

5. **C++을 "빠른 C" 정도로만 이해.**
   성능은 결과일 뿐 핵심은 "비용을 프로그래머가 선택하는 추상화"다. C는 추상화 도구가 거의 없어 대규모 코드에서 사람이 관리하기 어렵다. C++은 RAII·타입 시스템·템플릿으로 **관리 가능성과 성능을 동시에** 준다. 이 지점이 관리형 언어와도, C와도 다른 좌표다.

---

## 다른 챕터와의 연결

- [02_raii_lifetime.md](./02_raii_lifetime.md) — 결정적 소멸을 RAII·스코프 가드·소멸 순서로 심화.
- [03_smart_pointers_ownership.md](./03_smart_pointers_ownership.md) — 값 의미론에서 파생되는 소유권(unique_ptr/shared_ptr/raw)의 각론.
- [06_dll_compilation_linking.md](./06_dll_compilation_linking.md) — 컴파일 언어 정체성(TU·ODR·DLL 경계·매크로)의 각론.
- [13_interview_qa_bank.md](./13_interview_qa_bank.md) — 이 챕터의 Q&A를 포함한 통합 질문 은행.
