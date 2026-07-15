# 다형성 · 가상 함수 · 인터페이스 설계

## ① 한 줄 본질

런타임 다형성(runtime polymorphism)은 "베이스 포인터 하나로 파생 타입의 동작을 호출하는 것"이고, 컴파일러는 이를 클래스당 하나의 vtable과 객체당 하나의 vptr로 구현하며, 그 대가는 간접 분기(indirect branch)·인라인 불가·vptr 메모리 오버헤드다 — 그래서 저는 Winters에서 씬 전환·RHI 백엔드처럼 호출 빈도가 낮은 경계에는 가상 함수를 쓰고, 수천 개가 도는 ECS 컴포넌트와 챔피언 스킬 디스패치에는 vtable 없는 대안(함수 포인터 테이블, enum 태그, type erasure)을 썼습니다.

## ② 기본 개념

### vtable / vptr 메커니즘

- **vtable(virtual table)은 클래스당 하나**, **vptr(virtual pointer)은 객체당 하나**다. 이걸 반대로 말하면 바로 감점.
- 클래스에 가상 함수가 하나라도 있으면 컴파일러는 그 클래스의 가상 함수 포인터 배열(vtable)을 정적 데이터로 만들고, 객체 안에 숨겨진 멤버 vptr(x64에서 8바이트)을 넣는다. 단일 상속에서 MSVC/Itanium 모두 vptr은 객체 오프셋 0에 위치한다.
- `pBase->Foo()` 호출의 실제 기계 동작은 3단계:
  1. 객체에서 vptr 로드 (메모리 읽기 1회)
  2. vtable에서 `Foo`의 슬롯 인덱스에 있는 함수 포인터 로드 (메모리 읽기 1회)
  3. 그 주소로 간접 호출(indirect call)
- vptr은 **생성자(constructor)에서 세팅**된다. Base 생성자 실행 중에는 vptr이 Base의 vtable을 가리키므로, 생성자/소멸자 안에서 가상 함수를 호출하면 파생 override가 아니라 **현재 생성 중인 클래스의 버전**이 불린다. 순수 가상이면 `pure virtual function call` abort.

```
객체 메모리 (단일 상속, x64):
+--------+-----------------+
| vptr(8)| 멤버 데이터 ... |   ← 객체마다
+--------+-----------------+
    |
    v
클래스당 1개의 vtable: [ ~Dtor | OnEnter | OnExit | OnUpdate | ... ]
```

### 가상 함수 호출 비용 — 정확히 3가지

1. **간접 분기**: call 대상 주소가 런타임에 결정되므로 분기 예측기(BTB)에 의존한다. 같은 타입만 반복 호출하면 예측이 잘 맞아 싸지만, 타입이 뒤섞인 컨테이너를 순회하면 misprediction이 누적된다.
2. **인라인(inlining) 불가**: 컴파일 타임에 대상 함수를 모르므로 인라인이 안 되고, 인라인이 막히면 상수 전파·루프 최적화 같은 **후속 최적화가 연쇄로 막힌다.** 이게 간접 호출 자체보다 더 큰 비용인 경우가 많다.
3. **캐시/메모리**: 객체마다 vptr 8바이트가 붙어 캐시 라인당 담기는 실데이터가 줄고, vtable 로드 자체가 캐시 미스가 될 수 있다.

단, 비용은 절대값이 아니라 **호출 빈도 대비**로 판단한다. 프레임당 몇 번인 씬 디스패치에는 무의미하고, 프레임당 수만 번인 컴포넌트 내부 루프에는 치명적이다.

### 가상 소멸자(virtual destructor)가 필요한 이유

`Base* p = new Derived; delete p;` 에서 `~Base()`가 non-virtual이면 **미정의 동작(UB)** — 실무적으로는 파생부 소멸자가 호출되지 않아 파생 멤버(리소스, unique_ptr 멤버 등)가 누수된다. `std::unique_ptr<Base>`도 내부적으로 `delete base_ptr`을 하므로 똑같이 적용된다. 규칙: **베이스 포인터로 delete될 가능성이 있는 클래스는 `virtual ~Base() = default;`**. 반대로 다형적 삭제가 없는 final 클래스에 습관적으로 붙이면 vptr 8바이트만 낭비한다.

### 순수 가상(pure virtual)과 추상 클래스(abstract class)

- `virtual void F() = 0;` → 순수 가상. 하나라도 있으면 추상 클래스가 되어 인스턴스화 불가.
- 순수 가상 함수도 **본문(정의)을 가질 수 있다** (`=0` 선언 + 클래스 밖 정의, `Base::F()`로 한정 호출 가능). 특히 **순수 가상 소멸자는 반드시 본문이 필요**하다(파생 소멸 시 베이스 소멸자가 실제로 호출되므로).
- MSVC 확장으로 `class IScene abstract`처럼 명시할 수도 있다(표준은 아니고, 표준 방식은 순수 가상 멤버 보유).

### override / final

- `override`: "베이스의 가상 함수를 재정의한다"를 컴파일러가 검증. 시그니처를 오타 내면 조용히 **새로운 함수(name hiding)**가 생기는 사고를 컴파일 에러로 바꾼다. 가상 함수 재정의에는 무조건 붙이는 게 현대 C++ 규칙.
- `final`: 클래스 상속 또는 함수 재정의를 봉인. 설계 의도(더 파생 안 함)를 못박고, 컴파일러에게 **devirtualization**(정적 타입이 final이면 가상 호출을 직접 호출로 변환) 여지를 준다.

### 객체 슬라이싱(object slicing)

`void F(Base b)`에 `Derived d`를 넘기면 **Base 부분만 복사**되고 파생 멤버와 파생 동작이 잘려나간다. 복사본의 vptr은 Base의 vtable을 가리키므로 가상 호출도 Base 버전이 불린다. 예방: 다형적 타입은 **값이 아니라 포인터/참조로만** 다루고, 베이스 복사자를 delete/protected로 막거나, 소유는 `unique_ptr<Base>`로 한다.

## ③ 심화 (꼬리질문 대비)

### 다중 상속과 vptr 여러 개, this 조정

다중 상속 `class C : public A, public B` 에서 A와 B 둘 다 다형적이면 객체에 **vptr이 2개** 들어가고, B 서브오브젝트는 오프셋 0이 아니다. `B* pb = pc;` 는 컴파일러가 오프셋을 더하는 **this 조정(pointer adjustment)**이고, B 경로로 들어온 가상 호출이 C의 override에 도달하려면 **thunk**(this를 되돌리는 작은 점프 코드)가 vtable에 들어간다. "다중 상속에서 캐스팅하면 포인터 값 자체가 바뀔 수 있다"는 사실이 좋은 답변 포인트.

### 다이아몬드와 가상 상속(virtual inheritance)

```
    A
   / \
  B   C     class B : virtual public A;  class C : virtual public A;
   \ /      class D : public B, public C;  → A 서브오브젝트 1개
    D
```

- 가상 상속이 없으면 D 안에 A가 2개 생겨 `D* → A*` 변환이 모호(ambiguous)해진다.
- `virtual` 상속은 A를 공유 서브오브젝트로 만들되, A의 위치가 최종 파생 타입에 따라 달라지므로 vbtable/오프셋 간접 참조 비용이 생기고, **가상 베이스의 생성자는 최종 파생 클래스(most derived class)가 직접 호출**해야 한다.
- 실전 결론: 데이터 있는 클래스의 다이아몬드는 설계 리팩터링 신호. **순수 인터페이스(데이터 없는 추상 클래스)의 다중 구현**은 안전하고 흔하다. Winters도 인터페이스 구현 이상의 다중 상속은 쓰지 않는다.

### dynamic_cast와 RTTI

- `dynamic_cast<Derived*>(pBase)`는 **다형적 베이스(가상 함수 보유)**에서만 동작하고, vtable에 연결된 RTTI 정보로 상속 그래프를 런타임 탐색한다. 실패 시 포인터는 nullptr, 참조는 `std::bad_cast`.
- `static_cast` 다운캐스트는 검사 없이 오프셋만 조정 — **실제 타입이 맞다는 불변식이 코드 구조로 보장될 때만** 안전하고, 그 경우엔 RTTI 탐색 비용이 없어 더 빠르다.
- 판단 기준: "타입 보장이 어디서 오는가?" 보장 없음 → dynamic_cast + nullptr 체크(방어적). 보장 있음(맵 키, enum 태그 등) → static_cast + 그 불변식을 주석으로.

### 정적 다형성(static polymorphism): CRTP · 템플릿 vs 동적 다형성

```cpp
template<typename Derived>
struct Updatable {
    void Update() { static_cast<Derived*>(this)->UpdateImpl(); } // 컴파일타임 결합
};
```

| | 동적 (virtual) | 정적 (CRTP/템플릿) | 함수 포인터/std::function |
|---|---|---|---|
| 결합 시점 | 런타임 | 컴파일타임 | 런타임 (등록 시점) |
| 인라인 | 불가 (devirt. 예외) | 가능 | 함수 포인터: 불가 |
| 이종 컨테이너 | 가능 (`vector<unique_ptr<Base>>`) | 불가 (타입별 별도) | 가능 (시그니처 통일) |
| 객체 비용 | vptr 8B | 0 | 포인터 8B / std::function은 타입 소거 + 힙 할당 가능 |
| 단점 | 위 3대 비용 | 코드 팽창, ABI 경계 못 넘음 | 상태 캡처는 별도 컨텍스트 필요 |

선택 기준: **런타임에 타입이 섞여야 하고 호출 빈도가 낮으면 virtual, 컴파일타임에 타입이 고정이고 핫루프면 템플릿/CRTP, "동작만 갈아끼우기"면 함수 포인터/std::function.** Winters는 세 가지를 전부 자리에 맞게 썼다(아래 ④).

### DLL 경계와 vtable

가상 함수 호출은 vtable 간접 호출이므로, **인터페이스 클래스 자체는 dllexport하지 않아도** 구현(concrete) 클래스만 export하면 경계를 넘어 동작한다. 호출자는 헤더의 vtable 레이아웃(슬롯 순서)만 공유하면 된다 — 대신 이 레이아웃이 곧 ABI라서, **가상 함수 순서 변경/중간 삽입은 전체 재빌드 없이는 바이너리 호환을 깬다.**

## ④ Winters에서의 적용

### 1. IScene — 인터페이스 설계의 기본형 (필수 훅 + 선택 훅 + 가상 소멸자)

`Engine/Include/IScene.h:6` — `class WINTERS_ENGINE IScene abstract`. `OnEnter/OnExit/OnUpdate/OnRender`는 `= 0`(순수 가상)으로 계약을 강제하고, `OnLateUpdate/OnImGui`는 빈 본체 `{}`의 기본 가상 훅이라 파생 씬이 필요할 때만 override한다(`IScene.h:11-16`). 핵심은 `IScene.h:9`의 `virtual ~IScene() = default;` — 씬은 `unique_ptr<IScene>`로 소유·삭제되므로 이게 없으면 씬 전환 때마다 파생 씬 소멸이 누락되는 UB다.

`Engine/Private/Scene/Scene_Manager.cpp:15` `Change_Scene`이 런타임 다형성의 실사용처: `OnExit → 리소스 정리 → Safe_Reset(명시적 파괴) → std::move(pScene) → OnEnter` 순서로 수명을 오케스트레이션하고, `Update/LateUpdate/Render/ImGui`(`Scene_Manager.cpp:41-71`)는 static 씬(영속 오버레이)과 current 씬 두 슬롯에 대해 vtable 경유 호출만 한다 — 매니저는 파생 씬 타입을 전혀 모른다. **프레임당 호출 횟수가 한 자릿수라 가상 호출 비용이 무의미한, virtual이 정답인 자리.**

### 2. RHI — 하나의 인터페이스 뒤에 두 백엔드 (순수/기본 혼합 + 어댑터 + 탈출구)

- `Engine/Public/RHI/IRHIDevice.h:12` — `GetBackend/GetNativeHandle/BeginFrame/EndFrame`은 `= 0`(모든 백엔드 필수), `CreateBuffer/CreateShader/CreateTexture` 등은 `(void)desc; return {};` 형태의 **no-op 기본 가상**(`IRHIDevice.h:37-42`). 순수 가상만 쓰면 새 선택 기능 추가 시 모든 백엔드가 강제로 깨지므로, "필수 계약 = 순수, 선택 능력 = 기본 no-op"으로 나눴다. `IRHIDevice.h:15`에 `virtual ~IRHIDevice() = default;`.
- **어댑터**: `Engine/Private/RHI/DX11/CDX11Device.cpp:524` `class CDX11FrameCommandList final : public IRHICommandList` — IRHICommandList는 DX12식 기록형(record-then-submit) API인데 DX11은 immediate context라 기록 개념이 없다. 그래서 `Begin/End/BeginRenderPass`를 빈 override로 두고(`:532-536`) SetPipeline/Draw 계열만 즉시 컨텍스트 호출로 매핑한다. **실행 모델이 다른 두 API를 no-op 구현으로 흡수하는 어댑터 패턴.** 같은 인터페이스 뒤에서 DX11(immediate)과 DX12(fence 기반 다중 프레임 in-flight)가 전혀 다른 동기화 모델을 갖는 것도 "인터페이스는 계약, 구현은 자유"의 실례.
- **방어적 dynamic_cast**: `CDX11Device.cpp:546-548` — 핸들 테이블은 `IRHIPipelineState*`(인터페이스)를 담고, 바인딩 시 `dynamic_cast<CDX11PipelineState*>` 후 nullptr 체크. 테이블에 다른 백엔드 객체가 섞여 들어오는 사고를 UB가 아닌 안전한 스킵으로 격리한다.
- **탈출구(escape hatch)**: `Engine/Private/Renderer/SSAOPass.cpp:34-40` — SSAO는 DX11 전용 패스라 네이티브 디바이스가 필요하다. `GetNativeHandle(eNativeHandleType::DX11Device)`가 `void*`를 주고 `static_cast<ID3D11Device*>`로 복원. enum 태그가 타입을 지정하고 불일치 요청은 nullptr을 받으므로, "완전 추상화 불가능 지점"을 명시적·검사 가능한 한 곳으로 모았다. 공개 헤더가 `d3d11.h`를 include하지 않아도 되는 DLL 경계 은닉 효과도 있다.

### 3. ECS — "경계에만 virtual, 데이터에는 없음" (type erasure)

`Engine/Public/ECS/World.h:18` — `IComponentStoreBase`는 가상 소멸자 + 순수 가상 `Remove/Has`만 가진 최소 베이스이고, `CComponentStoreWrapper<T>`(`World.h:28`)가 이를 구현해 서로 다른 T의 `CComponentStore<T>`들을 `std::unordered_map<std::type_index, std::unique_ptr<IComponentStoreBase>>`(`World.h:212`) 하나에 담는다 — **추상 베이스 + 템플릿 파생 래퍼로 컴파일타임 타입 다양성을 런타임 다형성으로 접는 type erasure** (std::function/std::any의 내부 구현 원리와 동일한 패턴).

두 가지 면접 포인트:

1. **virtual의 입도(granularity)**: 가상 객체는 "컴포넌트 타입당 1개"(스토어 래퍼)뿐이고, 컴포넌트 인스턴스 수만 개에는 vptr이 전혀 없다. `DestroyEntity`처럼 타입 비의존 연산만 base 포인터로 가상 `Remove/Has`를 호출하고, 핫루프(`ForEach`, `World.h:135-177`)는 템플릿으로 concrete 스토어의 연속 배열을 직접 돈다. **컴포넌트에 vtable을 넣지 않는 이유** = 인스턴스당 8바이트 낭비 + 캐시 라인 오염 + trivially-copyable 상실(스냅샷 복제/직렬화 곤란).
2. **static_cast가 안전한 근거**: `World.h:180-186` `TryGetStore<T>`는 `std::type_index(typeid(T))`로 맵을 조회한 뒤 `static_cast<CComponentStoreWrapper<T>*>`로 내린다. **맵 키가 곧 동적 타입을 보장하는 불변식**(T 키 슬롯엔 Wrapper<T>만 존재)이라 dynamic_cast의 RTTI 탐색 비용이 불필요하다. RTTI를 캐스팅 검증이 아니라 **조회 키**로 쓰는 활용. RHI의 방어적 dynamic_cast와 대비해 설명하면 "캐스트 선택 기준을 이해한다"가 증명된다.

`EngineSDK/inc/ECS/ISystem.h:9`의 `ISystem`도 같은 문법 패턴: `virtual ~ISystem() = default;` + 순수 가상 `GetPhase/Execute/GetName` + 기본 구현 훅 `DescribeAccess`(미구현 시스템은 보수적으로 `builder.UnknownWritesAll()`, `ISystem.h:17-20`). 시스템은 개수가 수십 개 수준이라 가상 디스패치가 싸고, `GetPhase` 반환값으로 스케줄러가 실행 순서를 데이터로 정렬한다.

**DLL/ABI 포인트**: `EngineSDK/inc/ECS/Systems/MCTSSystem.h:14` — `class WINTERS_ENGINE CMCTSSystem final : public ISystem`. 베이스 `ISystem`은 export가 없고 **concrete만 export** — 인터페이스는 헤더의 vtable 레이아웃만 공유하면 경계를 넘는다(③ 참고). 추가로 private 생성자 + `static Create()`가 `unique_ptr` 반환(`MCTSSystem.h:22-27`), `final`로 상속 봉인.

### 4. 챔피언 GameSim — 상속 계층을 의도적으로 버린 곳

150 챔피언 스케일에서 `CChampionBase ← CAshe ← ...` 식 가상 함수 계층을 만들지 않았다:

- `Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h:24` — `using HookFn = void(*)(GameplayHookContext&);` 캡처 없는 **자유 함수 포인터**를 `HookFn m_table[kMaxChamp][kMaxVariant]`(256×256, `:36`) 2D 테이블에 등록하고, `Dispatch`는 인덱싱 한 번으로 O(1) 호출. 상속·vtable·힙 할당이 전혀 없다.
- `Shared/GameSim/Champions/Ashe/AsheGameSim.cpp:311` — 챔피언은 클래스가 아니라 `namespace AsheGameSim`의 자유 함수 묶음. `RegisterHooks()`가 `static bool_t s_bRegistered`로 멱등을 보장하며 `MakeGameplayHookId(eChampion::ASHE, GameplayHookVariant::Q_CastFrame)` 키에 `&OnQ`를 꽂는다(`:317-324`). 챔피언별 상태는 `AsheSimComponent` 같은 ECS 컴포넌트로 분리.

**왜 이 설계인가**: 상속으로 풀면 (1) 챔피언 수 × 스킬 변형 수만큼 클래스 폭발, (2) 베이스 헤더 수정 = 150개 파일 재컴파일(빌드 결합), (3) "Q는 A 계열, R은 B 계열" 재사용에서 다이아몬드 유혹. 함수 포인터 + 컴포넌트는 vtable 대비 데이터 지향적이고(상태가 trivially-copyable — 결정론 시뮬레이션·스냅샷과 궁합), 훅 등록 시점이 자유로워 챔피언을 링크 단위로 추가할 수 있다. **"수백 종 변형 개체를 상속으로 설계하면 안 되는 이유" 질문의 실전 답.**

같은 계열의 vtable 회피 두 가지:

- `Client/Private/GameObject/FX/FxCuePlayer.cpp:440-475` — **enum 태그 디스패치**: `emitter.renderType`을 분기해 Billboard→`CFxSystem::Spawn`, Beam/Ribbon→`CFxBeamSystem::Spawn`, MeshParticle→`CFxMeshSystem::Spawn` 라우팅. FX emitter는 파일에서 로드되는 값 데이터라 vptr을 넣을 수 없고(직렬화 대상), 미지원 타입은 `LogSkippedCueEmitter`(`:469, :474`)로 흘려 silent fail을 막는다.
- `Engine/Private/Manager/Navigation/Pathfinder.cpp:90-91` — `using CellPredicate = bool_t(*)(const CNavGrid*, CNavGrid::Cell, f32_t);` **함수 포인터로 전략 주입**: 반경 유/무 판정(`IsWalkableCell` vs `IsWalkableCellForRadius`)을 A* 본체에 인자로 넘겨, virtual 객체도 템플릿 인스턴스 중복도 없이 한 벌의 알고리즘으로 두 변형을 처리. `std::function`보다 가벼움(할당·타입 소거 없음).

### 5. virtual 대신 std::function — 레이어 결합 절단

`Client/Public/Network/Client/SnapshotApplier.h:14` — `class CSnapshotApplier final`, 가상 함수 0개. 확장점을 관찰자 인터페이스 상속이 아니라 `OnNewEntityFn/OnAuthoritativeSnapshotFn/OnChampionVisualChangedFn` 등 `std::function` 멤버(`:19-23`)로 노출하고 `Set...Callback(fn) { m_x = std::move(fn); }`(`:25-29`)로 주입한다. 상속 방식이면 Scene 레이어와 applier가 인터페이스 헤더로 얽히지만, std::function은 시그니처만 공유하면 되므로 **상위 레이어로의 헤더 의존을 끊는다.** 트레이드오프: std::function은 타입 소거로 큰 캡처에서 힙 할당 가능 — 등록은 초기화 1회, 호출은 스냅샷 단위라 감수 가능한 비용.

### 6. 인터페이스로 의존성 역전 — IWalkableQuery

`Server/Public/Game/GameRoom.h:51` — `class CGameRoom final : public IWalkableQuery`가 `IsWalkableXZ/SegmentWalkableXZ/TryBuildMovePath` 등을 override(`:69-80`). 이동/경로 시스템은 거대한 CGameRoom 구체 타입을 몰라도 `IWalkableQuery*`에만 의존한다 — **의존성 역전(DIP)**: 인터페이스가 "sim이 필요로 하는 최소 질의"를 정의하고 룸이 그것을 구현한다. `final`은 추가 파생이 없음을 못박아 설계 의도와 devirtualization 여지를 준다.

## ⑤ 면접 Q&A

**Q1. 가상 함수 호출이 일반 함수 호출보다 느린 이유를 구체적으로 설명해보세요.**
- 핵심: (1) vptr 로드 + vtable 슬롯 로드 후 간접 분기 — 분기 예측 실패 가능, (2) 인라인 불가 → 상수 전파 등 후속 최적화 연쇄 차단(이게 본질적 비용), (3) 객체당 vptr 8바이트로 캐시 밀도 저하. 그리고 "느린 게 아니라 빈도 대비로 판단"까지 말해야 한다.
- Winters 연결: 씬 디스패치(프레임당 몇 회)는 virtual(`Scene_Manager.cpp:41-71`), 수만 개 컴포넌트에는 vptr 자체를 안 넣음(`World.h` — 가상은 타입당 스토어 래퍼 단위만).

**Q2. 가상 소멸자는 왜 필요하고, 언제는 안 붙여도 됩니까?**
- 핵심: 베이스 포인터로 delete할 때 non-virtual dtor면 UB(파생부 소멸 누락). `unique_ptr<Base>`도 동일. 다형적 삭제가 없는 클래스(특히 final, 값 타입)에는 불필요 — vptr 비용만 생긴다.
- Winters 연결: `IScene.h:9` `virtual ~IScene() = default` — 씬은 `unique_ptr<IScene>`로 파괴됨. 반대로 `CSnapshotApplier`는 `final` + 비다형이라 가상 소멸자가 없다.

**Q3. 생성자 안에서 가상 함수를 호출하면 어떻게 됩니까?**
- 핵심: vptr이 생성 단계마다 "현재 생성 중인 클래스"의 vtable로 세팅되므로 파생 override가 불리지 않는다. 순수 가상이면 pure virtual call abort. 초기화 훅이 필요하면 생성 완료 후 별도 호출로 분리한다.
- Winters 연결: `CMCTSSystem::Create()`(`MCTSSystem.h:22-27`) — private 생성자 + 팩토리에서 생성 완료 후 `m_pPlanner`를 채우는 2단계 초기화. 생성 중 가상 디스패치에 기대지 않는 구조.

**Q4. dynamic_cast와 static_cast 다운캐스트의 차이와 각각 언제 씁니까?**
- 핵심: dynamic_cast는 다형적 베이스에서 RTTI로 런타임 검증(실패 시 nullptr/`bad_cast`), static_cast는 무검사 오프셋 조정. 기준은 "타입 보장이 코드 구조에 있는가".
- Winters 연결: 보장 없음 → `CDX11Device.cpp:547` 핸들 테이블의 `dynamic_cast<CDX11PipelineState*>` + nullptr 체크. 보장 있음 → `World.h:180-186` `type_index` 키가 동적 타입을 보장하므로 `static_cast<CComponentStoreWrapper<T>*>`.

**Q5. 서로 다른 타입의 객체들을 하나의 컨테이너에 담으려면 어떻게 합니까?**
- 핵심: (a) 공통 추상 베이스 + `vector<unique_ptr<Base>>`, (b) 타입별 템플릿 래퍼가 최소 가상 베이스를 구현하는 type erasure, (c) `std::variant`(타입 집합이 닫혀 있을 때).
- Winters 연결: `World.h:18` — `IComponentStoreBase`를 `CComponentStoreWrapper<T>`가 구현, `unordered_map<type_index, unique_ptr<IComponentStoreBase>>`(`:212`)에 이종 스토어 소유. 타입 비의존 연산(Remove/Has)만 가상으로 노출.

**Q6. 가상 함수 없이 런타임 다형성을 구현하는 방법과 각각의 트레이드오프는?**
- 핵심: 함수 포인터 테이블(할당·타입 소거 없음, 캡처 불가 → 컨텍스트 구조체로 보완), std::function(캡처 가능, 타입 소거·힙 할당 가능), enum 태그 + 분기(직렬화 가능한 값 데이터에 적합, 분기 비용), 템플릿/CRTP(컴파일타임 고정, 이종 컨테이너 불가).
- Winters 연결: `GameplayHookRegistry.h:24` 함수 포인터 256×256 테이블(스킬 훅), `SnapshotApplier.h:19-29` std::function 콜백(레이어 절단), `FxCuePlayer.cpp:440` enum 태그(로드되는 FX 데이터), `Pathfinder.cpp:90-91` 함수 포인터 전략 주입.

**Q7. 챔피언이 150종이라면 상속 계층으로 설계하겠습니까?**
- 핵심: 아니오 — 클래스 폭발, 베이스 수정 시 전체 재컴파일(빌드 결합), 스킬 재사용에서 다이아몬드 유혹, 인스턴스 상태의 직렬화/스냅샷 복제 곤란. 대안 = 상태는 컴포넌트, 동작은 훅 등록(합성 + 데이터 지향).
- Winters 연결: `AsheGameSim.cpp:311` — 챔피언은 namespace 자유 함수 + `AsheSimComponent`, `RegisterHooks`로 함수 포인터만 등록. 베이스 클래스·virtual 0개.

**Q8. override와 final은 각각 무엇을 해결합니까?**
- 핵심: override는 시그니처 불일치로 재정의가 조용히 name hiding이 되는 버그를 컴파일 에러로 승격. final은 상속/재정의 봉인 + devirtualization 힌트 + "여기서 계층 끝"이라는 설계 문서화.
- Winters 연결: `CDX11FrameCommandList final`(`CDX11Device.cpp:524`), `CGameRoom final : public IWalkableQuery`(`GameRoom.h:51`), `CMCTSSystem final`(`MCTSSystem.h:14`) — 구현/어댑터 클래스는 봉인이 기본값.

## ⑥ 흔한 오답/함정

1. **"객체마다 vtable이 있다"** — vtable은 클래스당 하나(정적 데이터), 객체에는 vptr만 들어간다. 이 한 문장으로 이해 깊이가 갈린다. (참고: MSVC DLL 환경에선 같은 클래스의 vtable/RTTI 사본이 모듈마다 생길 수 있어 vtable 주소 비교는 금물.)
2. **"가상 함수는 느리니까 게임에서 쓰면 안 된다"** — 빈도를 안 따진 절대화. 프레임당 몇 회의 씬/디바이스 디스패치는 비용이 0에 수렴하고, 진짜 문제는 수만 개 엔티티 내부 루프다. Winters처럼 "경계에는 virtual, 데이터 루프에는 배제"가 정답 프레임.
3. **"안전하니까 다운캐스트는 항상 dynamic_cast"** — 타입 불변식이 구조적으로 보장되는 곳(type_index 키 맵 등)에서는 static_cast가 정당하고 더 싸다. 반대로 불변식 없이 static_cast를 쓰면 UB. "어느 쪽이 항상 옳다"가 아니라 **보장의 출처**를 말해야 한다.
4. **"순수 가상 함수는 본문을 가질 수 없다"** — 가질 수 있고(`=0` 선언 + 클래스 밖 정의), 순수 가상 소멸자는 본문이 필수다. "추상 클래스 = 인스턴스화 불가"와 "가상 소멸자 필요 = 다형적 삭제 여부"는 별개 축이라는 것도 함께.
5. **객체 슬라이싱을 "포인터가 잘린다"로 설명** — 슬라이싱은 **값 복사**에서 Base 서브오브젝트만 복사되는 현상이지 포인터와 무관하다. 베이스 포인터/참조에서는 발생하지 않고, `vector<Base>`에 파생 객체를 push_back하는 순간 발생한다. 예방책(포인터/참조로만 다루기, 복사 금지, `unique_ptr<Base>` 소유)까지 세트로 답한다.

## 다른 챕터와의 연결

- [05_class_design_value_semantics.md](05_class_design_value_semantics.md) — 가상 소멸자와 Rule of Five, 복사 금지로 슬라이싱 차단, private 생성자 + 팩토리.
- [07_templates_generic.md](07_templates_generic.md) — type erasure의 템플릿 쪽 절반(`CComponentStoreWrapper<T>`), CRTP, 코드 팽창.
- [08_stl_containers_cache.md](08_stl_containers_cache.md) — vptr이 캐시 밀도에 미치는 영향, ECS가 연속 배열로 vtable을 배제하는 데이터 지향 설계.
- [02_compile_link_dll.md](02_compile_link_dll.md) — vtable 레이아웃 = ABI, 인터페이스 비export/concrete export 전략, WINTERS_ENGINE 매크로와 DLL 경계.
