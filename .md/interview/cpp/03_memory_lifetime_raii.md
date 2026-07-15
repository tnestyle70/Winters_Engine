# 메모리 · 객체 수명 · RAII

> 대상: Winters 엔진(C++17/DX11, LoL 스타일 클라+서버 자작 엔진)을 직접 만든 게임 프로그래머 지망생.
> 아래 `path:line` 인용은 전부 실제 파일을 열어 검증한 것이다.

---

## ① 한 줄 본질

**RAII(Resource Acquisition Is Initialization)는 자원의 수명을 객체의 수명에 묶어, 스코프를 벗어나는 순간 소멸자(destructor)가 결정적으로(deterministically) 자원을 반납하게 하는 기법이다. C++가 GC 없이도 예외·조기 return이 난무하는 코드에서 누수(leak)·댕글링(dangling)·이중 해제(double free)를 막는 핵심 메커니즘이며, 저는 GPU 리소스·COM 초기화·프로파일러 스코프·워커 스레드 join까지 전부 이 원리로 관리했습니다.**

---

## ② 기본 개념

### 가상 주소 공간 (virtual address space)

- 각 프로세스는 OS가 만들어 준 독립된 가상 주소 공간을 가진다. 포인터에 담긴 주소는 물리 주소가 아니라 가상 주소이고, MMU가 페이지 테이블(page table)을 통해 물리 프레임으로 변환한다(x64 Windows 기본 페이지 4KB).
- 다른 프로세스의 주소를 침범할 수 없는 격리가 여기서 나온다. 잘못된 주소 접근은 페이지 폴트 → access violation(0xC0000005)로 죽는다.
- Windows는 예약(reserve)과 커밋(commit)을 구분한다(`VirtualAlloc`). 스택도 처음부터 전부 커밋되지 않고, 가드 페이지(guard page)를 건드리면 OS가 한 페이지씩 늘려 준다 — 스택 오버플로우는 이 확장의 한계(기본 1MB)에 닿는 것.

### 스택 vs 힙 vs 정적 영역

| 영역 | 할당 방식 | 속도 | 수명 |
|---|---|---|---|
| 스택 | rsp 이동(함수 프롤로그에서 `sub rsp, N`) | 사실상 0비용 | 스코프 종료까지 (자동) |
| 힙 | 할당자가 free block 탐색/분할 (+락 가능성) | 상대적으로 느림, 단편화 | `delete`까지 (수동/스마트포인터) |
| 정적 | 실행 파일 로드 시 .data/.bss에 배치 | 할당 자체가 없음 | 프로그램 종료까지 |

- 스택 할당은 "포인터 하나 밀기"라 빠르지만 크기가 컴파일 타임에 정해지고 프레임과 함께 사라진다. 힙은 크기·수명이 자유로운 대신 할당 비용, 캐시 지역성 저하, 수명 관리 책임이 따라온다.
- 게임 엔진의 실전 결론: "힙이 나쁘니 피하라"가 아니라, **핫 루프에서의 반복 할당을 피하고, 소유권(ownership)을 명확히 하라**. Winters의 A* 탐색은 262,144칸짜리 배열을 `thread_local`로 한 번만 잡아 재사용한다 (`Engine/Private/Manager/Navigation/Pathfinder.cpp:463-467`).

### 객체 수명과 storage duration

C++의 storage duration은 4가지다. **저장 기간(storage)과 객체 수명(lifetime)은 다른 개념**이다 — 메모리가 있어도 생성자가 끝나기 전/소멸자가 시작된 후에는 객체가 "살아있지 않다".

1. **automatic** — 블록 스코프 지역 변수. 스코프 종료 시 선언 역순으로 파괴.
2. **static** — 전역, `static` 멤버, 함수 지역 `static`. 프로그램 종료 시 생성 역순 파괴. 서로 다른 번역 단위(TU) 간 초기화 순서는 미정 (static initialization order fiasco).
3. **thread** — `thread_local`. 스레드마다 하나, 스레드 종료 시 파괴.
4. **dynamic** — `new`/`delete`로 명시 제어.

객체 수명 규칙: 수명은 "저장 공간 확보 + 초기화 완료" 시점에 시작하고, "소멸자 호출 시작" 시점에 끝난다. 수명이 끝난 객체에 접근하면 UB(use-after-free).

### new/delete의 내부 — 2단계 분해

`new T(args)`라는 **new 표현식(new expression)**은 두 단계다:

1. `operator new(sizeof(T))` 호출 — 원시 메모리 할당 (실패 시 `std::bad_alloc`, `nothrow` 버전은 nullptr).
2. 그 메모리 위에서 T의 생성자 호출.

`delete p`는 역순: 소멸자 호출 → `operator delete(p)`. 이 분해를 알아야 다음이 설명된다:

- **placement new** — `new (ptr) T(args)`: 1단계를 건너뛰고 이미 확보된 메모리에 생성자만 실행한다. `std::vector`가 capacity만큼 원시 메모리를 잡아 두고 원소를 하나씩 구성(construct)하는 원리가 이것. 파괴는 `p->~T()` 명시 호출.
- **`new[]` / `delete[]`** — 배열 new는 원소 개수(쿠키)를 블록 앞에 숨겨 두고 delete[] 때 그 수만큼 소멸자를 돈다. `new[]`로 만든 것을 `delete`로 지우면 UB. Winters는 이 짝을 API 이름부터 분리한다: `Safe_Delete` vs `Safe_Delete_Array` (`Engine/Public/Engine_Function.h:8-26`).
- `delete nullptr`는 표준이 no-op으로 보장한다. null 체크는 안전을 위해서가 아니라 다른 목적(아래 ④)으로 하는 것.

### 정렬 (alignment)

- 모든 타입은 `alignof(T)` 정렬 요구를 가진다. 잘못 정렬된 주소로 SIMD 로드를 하면 x64에서도 크래시(movaps)할 수 있다.
- `malloc`/기본 `operator new`는 `max_align_t`(x64 MSVC에서 16바이트)까지만 보장한다. 그보다 큰 정렬(over-aligned) 타입은 C++17의 `operator new(size_t, align_val_t)` 또는 `_aligned_malloc`이 필요하다.
- DirectXMath의 `XMVECTOR`/`XMMATRIX`는 SIMD 레지스터 매핑 타입이라 16B 정렬을 요구한다. Winters는 이를 규약으로 박제했다: **저장은 `Vec3`/`Mat4`(XMFLOAT 계열), 연산은 XMLoad → 계산 → XMStore 경계에서만** (`Engine/Include/WintersMath.h:9-16`). 주석의 문장 그대로 — "힙이 본질적으로 위험한 것이 아니라, SIMD 저장 타입의 16B 정렬/레이아웃 조건을 전역 데이터 구조에서 놓치기 쉽다."

### RAII — 정의와 왜 핵심인가

- 자원(메모리, 파일, 락, COM 참조, GPU 버퍼, 스레드)의 획득을 생성자에서, 반납을 소멸자에서 한다. 그러면 **자원 수명 = 객체 수명**이 되고, 컴파일러가 모든 탈출 경로(정상 return, 조기 return, 예외)에서 소멸자 호출을 보장한다.
- C++에 `finally`가 없는 이유가 RAII다. 정리 코드를 "잊을 수 있는 위치"(함수 끝)가 아니라 "잊을 수 없는 위치"(타입 자체)에 둔다.
- GC 언어와의 차이: GC는 메모리만, 그것도 비결정적 시점에 회수한다. RAII는 **모든 종류의 자원을 결정적 시점에** 반납한다. 게임에서 GPU 리소스·락·오디오 핸들을 "언젠가" 놓아주는 건 허용되지 않으므로 이 결정성이 곧 엔진 품질이다.

### 예외 안전성과 스택 되감기 (stack unwinding)

- 예외가 던져지면 catch까지 거슬러 올라가며 각 스코프의 자동 객체가 **선언 역순으로** 파괴된다. RAII 객체만이 이 되감기에서 자원을 회수할 수 있다 — raw `new` 후 예외가 나면 그 포인터는 영원히 샌다.
- 소멸자는 C++11부터 암묵 `noexcept`다. 되감기 중 소멸자가 또 던지면 `std::terminate`.
- 예외 안전성 보장 수준 3단계는 용어로 알아두기:
  - **basic guarantee** — 예외가 나도 누수 없고 불변식은 유지 (상태는 바뀌었을 수 있음).
  - **strong guarantee** — 실패 시 호출 전 상태로 롤백 (commit-or-rollback, copy-and-swap이 대표 구현).
  - **nothrow guarantee** — 절대 던지지 않음 (`noexcept`). 소멸자·swap·이동 연산이 여기 속해야 컨테이너가 안전하게 동작한다.
- Winters는 게임플레이 경로에서 예외 대신 HRESULT/bool 이중 초기화를 쓰지만, **조기 return이 예외와 같은 문제를 만든다**는 점은 동일하다. return 지점이 5개인 함수에서 정리 코드를 5번 복붙하지 않으려면 결국 RAII다 (④의 `CScopedCOMInit`, `CounterFlush`).

### 3대 메모리 버그

- **누수(leak)**: 해제를 잊음. 최악은 아님 — 프로그램은 돈다.
- **댕글링(dangling) / use-after-free**: 죽은 객체를 가리키는 포인터로 접근. 재현이 비결정적이라 가장 악질.
- **이중 해제(double free)**: 같은 블록을 두 번 delete. 힙 메타데이터가 깨져 전혀 엉뚱한 곳에서 터진다.

뒤의 둘은 대부분 **소유권이 불명확한 raw pointer 복사**에서 나온다. 해법의 층위: ① 스마트 포인터로 소유권을 타입에 인코딩 ② 복사/이동 제어(rule of three/five) ③ 아예 포인터 대신 세대(generation) 핸들로 stale 접근을 값 수준에서 검출 (④ 참고).

### 커스텀 할당자와 풀 (pool)

일반 할당자를 우회하는 이유: 할당 속도(자유 리스트에서 O(1) pop), 단편화 제거, 캐시 지역성, 프레임 단위 일괄 해제(arena/linear allocator). 전형적 패턴:

- **객체 풀 + free list**: 파괴된 슬롯을 리스트로 꿰어 재사용. Winters의 엔티티 슬롯은 별도 할당 없이 슬롯 자신이 free list 노드를 겸한다(intrusive free list, `Engine/Public/ECS/Entity.h:183-187` — `EntitySlot`은 `generation`과 `nextFree` 두 필드뿐).
- **세대 도장(generation stamp) lazy reset**: 큰 작업 배열을 매번 memset하지 않고, 세대 카운터 비교로 "이번 탐색에서 처음 만진 칸만" 초기화 (`Pathfinder.cpp:469-481`의 `Touch` 람다).
- **슬롯맵(slot map)**: index+generation 핸들로 조회하는 풀. GPU 리소스 테이블이 이 구조다 (④).

---

## ③ 심화 — 꼬리질문 대비

### `#define new`의 부작용과 격리

CRT 디버그 힙은 `new`를 매크로로 치환해 누수 지점을 파일/라인으로 보고하게 한다. Winters도 쓴다 (`Engine/Public/Engine_Defines.h:38-41`):

```cpp
#ifndef DBG_NEW
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#define new DBG_NEW
#endif
```

그런데 이 매크로는 `new (ptr) T` — placement new 문법을 텍스트 치환으로 깨뜨린다. Tracy·DirectXTK처럼 placement new를 쓰는 서드파티 헤더를 include하면 컴파일이 깨진다. 표준적 해법은 매크로 상태를 저장/복원하는 것 (`Engine/Include/ProfilerAPI.h:15-19`, 같은 패턴이 `Engine/Private/Resource/Texture.cpp:9-13`에도):

```cpp
// Engine_Defines.h의 디버그 new 매크로(DBG_NEW)가 Tracy의 placement new를 깨뜨리므로 격리한다.
#pragma push_macro("new")
#undef new
#include "tracy/Tracy.hpp"
#pragma pop_macro("new")
```

면접 포인트: "전역 `new` 매크로 치환은 new **표현식**이 아니라 **토큰**을 바꾸기 때문에, operator new 오버로딩과 달리 placement new까지 오염시킨다."

### unique_ptr + 불완전 타입(incomplete type) = 소멸자 위치 문제

`std::unique_ptr<T>`의 기본 삭제자 `default_delete<T>`는 **delete가 일어나는 지점에서 T의 완전한 정의**를 요구한다(불완전 타입 delete는 UB라 static_assert로 막는다). 전방 선언만 보이는 헤더에서 소멸자를 `= default`(inline)로 정의하면, 그 헤더만 include한 TU가 소멸자 바디를 실체화하다가 컴파일 에러가 난다.

Winters는 이 함정을 주석까지 달아 박제했다 (`Engine/Private/GameInstance.cpp:13-23`):

> "헤더에 = default 로 inline 정의하면, GameInstance.h 만 include 한 TU 가 소멸자 바디를 실체화할 때 default_delete\<CTimer_Manager\> 가 sizeof 를 요구 → static_assert 'can't delete an incomplete type' 발동. out-of-line 으로 내려서 이 .cpp 에서만 생성."

서버도 동일 구조: `CGameRoom`은 `CNavGrid`·`CSnapshotBuilder` 등을 전방 선언(`Server/Public/Game/GameRoom.h:35-49`)한 채 `unique_ptr` 멤버로 소유하고(`GameRoom.h:207-220`), 소멸자는 헤더에 선언만, 정의는 `Server/Private/Game/GameRoom.cpp:139-142`에 둔다. pimpl 패턴이 반드시 소멸자를 .cpp에 내리는 이유와 같다.

### 소멸 순서 3계층

1. **멤버**: 선언 역순으로 파괴. 멤버 간 의존이 있으면 선언 순서가 곧 종료 시퀀스 설계다.
2. **함수 지역 static (Meyers 싱글턴)**: 첫 호출 시 초기화(C++11 magic statics로 스레드 안전), 프로그램 종료 시 **생성 역순** 파괴. Winters의 `DECLARE_SINGLETON`이 이 방식 (`Engine/Public/Engine_Macro.h:57-65`).
3. **TU 간 전역 static**: 순서 미정. 한 전역의 소멸자가 다른 TU의 전역을 만지면 러시안 룰렛.

멀티스레드가 끼면 "역순 자동 파괴"만으로 부족할 수 있다. `~CGameInstance()`는 멤버 unique_ptr들의 자동 reset에 맡기지 않고 `Shutdown_Engine()`을 먼저 호출해 **워커 스레드 join을 다른 매니저 파괴보다 앞세운다** (`Engine/Private/GameInstance.cpp:24-31`). join 프로토콜 자체는 `CJobSystem::Shutdown()`: shutdown 플래그 store(release) → `notify_all`로 잠든 워커 전원 기상 → joinable 검사 후 join → 컨테이너 clear (`Engine/Private/Core/JobSystem.cpp:72-86`). `std::thread`를 join 없이 파괴하면 `std::terminate`이므로, 이 순서는 선택이 아니라 생존 조건이다.

### "파괴해도 되는 시점"이 즉시가 아닌 경우 — 지연 파괴(deferred destruction)

비동기 시스템에서는 "참조가 끊겼다 ≠ 파괴해도 된다"가 된다.

- **IOCP 세션**: 소켓이 끊겨도 커널이 OVERLAPPED/버퍼를 아직 참조하는 in-flight IO가 남아 있으면 delete는 use-after-free다. `CSession`은 atomic `m_pendingIoCount`를 오퍼레이션마다 증감하고, `CanDestroy() = m_bClosing && pendingIoCount == 0`일 때만 파괴를 허용한다 (`Server/Public/Network/Session.h:38-52`). 생성부터가 `enable_shared_from_this` + private 생성자 + `static Create()`가 `shared_ptr`을 반환하는 구조로, 힙+공유 소유를 타입으로 강제한다 (`Session.h:15-18, 57`).
- **std::async의 future**: `std::async(launch::async, ...)`가 반환한 future를 버리면 임시 future의 소멸자가 작업 완료까지 블로킹해 **비동기가 조용히 동기가 된다**. `CHttpClient`는 이 버그를 주석으로 박제하고(`Client/Private/Network/Backend/CHttpClient.cpp:89-92`), future를 `m_PendingRequests`가 소유(`:148-156`), 소멸자에서 전량 `wait()`로 드레인해 블로킹을 파괴 시점 한 번으로 한정한다(`:103-117`). 이 드레인이 곧 워커 람다의 raw `this` 캡처가 dangling이 되지 않는 유일한 수명 논거이기도 하다.

### 객체 수명과 타입 재해석 — reinterpret_cast의 회색 지대

수명(lifetime) 규칙은 "그 주소에 어떤 타입의 객체가 살아 있는가"까지 포함한다. 다른 타입인 척 접근하는 것(type punning)은 strict aliasing 규칙과 충돌할 수 있다.

Winters의 `Vec3::ToXMVECTOR()`가 실전 사례다 (`Engine/Include/WintersMath.h:44`):

```cpp
XMVECTOR ToXMVECTOR() const { return XMLoadFloat3(&reinterpret_cast<const XMFLOAT3&>(*this)); }
```

- `Vec3`와 `XMFLOAT3`는 둘 다 `float x, y, z`만 가진 standard-layout 구조체라 레이아웃은 실제로 일치한다(layout-compatible). 하지만 엄밀히는 "Vec3 객체가 살아 있는 자리에 XMFLOAT3로 접근"이므로 언어 규칙상 회색 지대다. 실무에서는 MSVC가 허용하고 `XMLoadFloat3`가 값을 즉시 SIMD 레지스터로 읽어 끝나기 때문에 문제가 없다.
- 같은 파일의 `ToXMFLOAT3()`(`:43`)는 멤버 복사로 만드는 100% 합법 버전 — 복사 비용 vs 언어 규칙 엄밀성의 트레이드오프를 두 함수의 공존으로 보여줄 수 있다.
- 면접에서 확실히 안전한 대안까지 말하면 만점: `memcpy`(컴파일러가 최적화로 제거해 줌), C++20 `std::bit_cast`.

### 포인터를 아예 없애는 방어 — generational handle

댕글링은 "주소는 남는데 객체가 바뀌는" 문제다. 주소 대신 **index(하위 32bit) + generation(상위 32bit)을 u64에 팩킹한 값 핸들**을 쓰고, 슬롯을 재사용할 때마다 세대를 올리면, 옛 핸들은 조회 시 세대 불일치로 자동 무효화된다(ABA 방지).

- ECS 엔티티: `EntityHandle::Make`가 `(generation << 32) | id`로 팩킹(`Engine/Public/ECS/Entity.h:41-51`), `TryResolve`가 슬롯 세대와 비교해 stale 핸들을 거부(`:152-169`). 살아있음/죽음은 세대의 최하위 비트 홀짝으로 무비용 인코딩(`:189-192`).
- GPU 리소스: `CRHIResourceTable::Insert`가 free list 슬롯 재사용 시 `++generation`(0이면 1로 보정 — 0은 무효 핸들 센티넬)하고(`Engine/Public/RHI/CRHIResourceTable.h:40-42`), `Lookup`은 세대 불일치면 nullptr(`:64-65`). 이 테이블은 락 대신 "렌더 스레드에서만 접근" 계약을 `AssertRenderThread()`로 디버그 빌드에서 강제한다(`:125-131`) — 동기화 비용 0, 위반은 개발 중 결정적 검출.

---

## ④ Winters에서의 적용

### RAII 소품 3종 — "짝 호출"을 타입으로

- **COM init/uninit**: `CScopedCOMInit`은 생성자에서 `CoInitializeEx` 결과를 bool로 기억하고, 소멸자에서 **성공했을 때만** `CoUninitialize`한다 (`Engine/Private/RHI/RHITextureLoader.cpp:14-31`, `Engine/Private/Resource/Texture.cpp:17-34`). HRESULT 실패 조기 return이 여러 개인 텍스처 로드 함수에서 어느 경로로 나가든 짝이 맞는다. 실패 시 uninit을 건너뛰는 게 핵심 — 남의 init 참조 카운트를 깨지 않기 위해.
- **프로파일러 스코프**: `CProfileScope`는 생성자 Push / 소멸자 Pop인 순수 RAII 가드 (`Engine/Include/ProfilerAPI.h:33-38`). `WINTERS_PROFILE_SCOPE`는 `__LINE__`으로 고유 변수명을 만드는데, `a##b` 직접 결합은 `__LINE__` 확장 전에 붙어버리므로 2단계 CAT 매크로로 확장을 지연시킨다(`:41-49`).
- **지역 struct scope guard**: A* 함수는 `CounterFlush`라는 지역 struct의 소멸자에서 방문 노드 수를 프로파일러에 flush한다 (`Pathfinder.cpp:450-459`). return 경로가 몇 개든 정확히 한 번 기록 — 라이브러리 없이 소멸자 결정성만으로 만든 finally.

### GPU 리소스 RAII — 3가지 소유 모델

1. **ComPtr(침습적 refcount)**: 디바이스/스왑체인은 `Microsoft::WRL::ComPtr`로 AddRef/Release 자동화. `shared_ptr`과 달리 카운트가 객체 내부(IUnknown)에 있어 제어 블록이 없다.
2. **raw 포인터 + 수동 Release의 위험**: `DX11Buffer`는 raw `ID3D11Buffer*`를 들고 소멸자에서 `Release()`를 부르지만 **복사 생성/대입을 delete하지 않았다** (`Engine/Private/RHI/DX11/DX11Buffer.h:25-26, 62-63`). 값 복사되면 같은 COM 포인터를 두 번 Release하는 rule of three 위반 — 알고 있는 잠재 함정으로, 면접에서 "내 코드의 결함을 아는가" 질문에 그대로 쓸 수 있다. 반면 `DX11ConstantBuffer<T>`는 복사 delete + noexcept 이동 생성/대입(원본 null화, 자기대입 검사, 기존 자원 Release 순서)으로 rule of five를 올바르게 지킨다 (`Engine/Private/RHI/DX11/DX11ConstantBuffer.h:21-39`).
3. **핸들 소유 RAII**: `CRHIMeshResource`는 네이티브 포인터가 아니라 `RHIBufferHandle`(정수 핸들)을 소유권 토큰으로 갖고, `Shutdown()`에서 `m_pDevice->DestroyBuffer(handle)`로 반납 후 핸들을 `{}`로 리셋한다 (`Engine/Private/Renderer/RHIMeshResource.cpp:86-103`). `m_pDevice`는 비소유 관찰자 — "리소스가 디바이스보다 먼저 죽는다"는 수명 가정이 명시된 설계.

`CTexture` 소멸자도 SRV/샘플러를 Release 후 nullptr로 리셋하는 같은 계열이다 (`Texture.cpp:55-67`).

### 리소스 매니저 수명 — 소유권을 자원 성격별로 가르기

`CResourceCache`는 한 캐시 안에서 두 소유 모델을 쓴다 (`Engine/Public/Resource/ResourceCache.h:36-40`):

```cpp
unordered_map<wstring, unique_ptr<CTexture>> m_mapTextures; // 캐시 단독 소유
unordered_map<string, shared_ptr<CModel>>    m_mapModels;   // 호출부와 공유 소유
```

- 텍스처: 캐시가 단독 소유, 호출자에게는 비소유 raw `CTexture*`만 반환 (`Engine/Private/Resource/ResourceCache.cpp:41-44`). "캐시 수명 > 사용자 수명" 전제를 계약으로.
- 모델: 씬 전환 중에도 들고 있을 주체가 여럿이라 `shared_ptr`. 팩토리가 준 `unique_ptr`를 `shared_ptr<CModel> shared(pModel.release());`로 소유권 이관한다 (`ResourceCache.cpp:67-69`) — 객체가 이미 생성돼 있으므로 `make_shared`는 불가, 제어 블록만 새로 붙는다.

### 스폰 경로의 수명 설계 — 소유는 한 곳, 관찰은 여러 곳

`CChampionSpawnService`의 스폰 함수 (`Client/Private/GameObject/ChampionSpawnService.cpp`):

- `make_unique<ModelRenderer>` 직후 `Initialize` 실패면 그냥 `return result;` — 지역 unique_ptr이 스코프 종료로 자동 파괴되므로 어떤 실패 분기에도 누수·부분 초기화가 없다 (`:183-197`).
- 성공 경로에서는 `RenderComponent.pRenderer = pRenderer.get()`(비소유 관찰, `:231`) 후 `context.renderers[entity] = std::move(pRenderer)`로 소유권을 씬 소유 map에 이관한다 (`:290-293`). **소유자는 map 하나, 나머지는 전부 `get()` 관찰** — 관찰 포인터의 수명은 map 원소 수명에 종속된다는 계약이 코드 구조에 드러난다.

### 소멸 순서 문제 — 실제로 겪은 지점

- `~CGameInstance()`: 멤버 역순 자동 파괴만 믿지 않고 `Shutdown_Engine()`으로 워커 join을 앞세움 (`GameInstance.cpp:24-31`). unique_ptr + 전방 선언 때문에 생성자/소멸자를 .cpp로 내린 이유가 주석으로 박제 (`:13-23`).
- `std::vector<std::atomic<...>>` 함정: `CWorkStealingDeque`는 atomic 멤버 + `alignas(64)` 때문에 복사/이동 불가 → vector에 직접 못 담아 `unique_ptr`로 박싱해 포인터만 담는다 (`JobSystem.cpp:53-63` 주석).

---

## ⑤ 면접 Q&A

**Q1. 스택과 힙의 차이를 설명해 보세요.**
- 핵심: 할당 방식(rsp 이동 vs 할당자 탐색), 속도, 수명(스코프 자동 vs 수동), 크기 제약(기본 1MB vs 가상 주소 공간), 스레드별 스택 vs 공유 힙.
- 한 걸음 더: "힙 할당이 느려서"만이 아니라 캐시 지역성과 수명 책임까지 묶어 말하기.
- Winters 연결: A* 작업 배열을 `thread_local`로 한 번 잡아 재사용 + 세대 도장 lazy reset으로 매 호출 memset도 제거 (`Pathfinder.cpp:463-481`).

**Q2. RAII가 무엇이고 왜 C++의 핵심입니까?**
- 핵심: 자원 수명 = 객체 수명. 모든 탈출 경로(정상/조기 return/예외)에서 소멸자 호출을 컴파일러가 보장. GC와 달리 메모리 외 자원까지 결정적 시점에 반납.
- Winters 연결: `CScopedCOMInit`(짝 API), `CProfileScope`(짝 호출), `CounterFlush`(finally 대체), `~CJobSystem`→`Shutdown`(스레드 join)까지 "메모리가 아닌 자원" 사례를 3개 이상 들 수 있다.

**Q3. delete 후에 왜 nullptr를 대입하나요? 그걸 함수로 만들면 왜 참조 매개변수여야 하나요?**
- 핵심: `delete nullptr`는 no-op 보장이므로 재-delete가 안전해지고, 죽은 주소로의 접근을 조기에 크래시로 드러낸다. 값으로 받으면 사본만 null이 되어 호출자에겐 댕글링이 그대로 남는다.
- Winters 연결: `Safe_Delete(T& Pointer)`가 참조로 받아 호출자 변수까지 null화 (`Engine_Function.h:8-16`). `delete[]`는 `Safe_Delete_Array`로 API부터 분리.

**Q4. 소멸자에서 리소스를 해제하는 클래스를 실수로 복사하면 어떻게 됩니까?**
- 핵심: 두 객체가 같은 자원을 가리키다 각자 소멸자에서 해제 → 이중 해제. 그래서 rule of three/five: 소멸자를 손수 썼다면 복사/이동도 정의하거나 delete해야 한다.
- Winters 연결: `DX11Buffer`는 복사 제어 미선언(잠재 double free, `DX11Buffer.h:25-26`), `DX11ConstantBuffer<T>`는 복사 delete + noexcept 이동으로 올바르게 처리 (`DX11ConstantBuffer.h:21-39`). "구형 코드의 함정과 수정판이 한 코드베이스에 공존하고, 어느 쪽이 왜 맞는지 안다"가 강한 답.

**Q5. unique_ptr 멤버로 전방 선언 타입을 들고 있으면 왜 소멸자를 .cpp에 둬야 하나요?**
- 핵심: `default_delete<T>`가 delete 지점에서 완전 타입을 요구(불완전 타입 delete는 UB → static_assert). 인라인 `= default` 소멸자는 헤더를 include한 모든 TU에서 실체화된다.
- Winters 연결: `GameInstance.cpp:13-23`의 박제 주석, `GameRoom.h:207-220` + `GameRoom.cpp:139` 동일 패턴.

**Q6. placement new는 언제 쓰고, `#define new`는 뭐가 위험한가요?**
- 핵심: placement new는 할당과 생성의 분리 — 풀/컨테이너가 원시 메모리에 생성자만 실행할 때. `#define new`는 토큰 치환이라 placement new 문법 자체를 깨뜨린다.
- Winters 연결: CRT 누수 추적용 `DBG_NEW`(`Engine_Defines.h:38-41`)와 Tracy/DirectXTK 헤더의 충돌을 `push_macro/undef/pop_macro`로 격리 (`ProfilerAPI.h:15-19`, `Texture.cpp:9-13`).

**Q7. 파괴된 객체를 가리키는 stale 참조를 아키텍처 수준에서 막으려면?**
- 핵심: 포인터 대신 index+generation 값 핸들. 슬롯 재사용 시 세대 증가 → 옛 핸들은 조회 시 세대 불일치로 nullptr/실패. ABA 문제의 해법과 동형.
- Winters 연결: `EntityHandle`(`Entity.h:41-51, 152-169`), GPU 리소스 슬롯맵 `CRHIResourceTable`(`CRHIResourceTable.h:40-42, 64-65`). 세대 0을 무효 센티넬로 예약하는 디테일까지.

**Q8. 비동기 작업이 물고 있는 객체는 언제 파괴해도 됩니까?**
- 핵심: "참조가 끊긴 시점"이 아니라 "모든 in-flight 작업이 그 객체를 더는 만지지 않는 시점". 카운터/드레인으로 그 시점을 만들어야 한다.
- Winters 연결: IOCP 세션의 pending IO refcount + `CanDestroy()` (`Session.h:38-52`), `~CHttpClient`의 future 전량 wait 드레인 — 그것이 async 람다의 raw `this` 캡처가 안전한 유일한 근거 (`CHttpClient.cpp:103-117`).

---

## ⑥ 흔한 오답 / 함정

1. **"delete 전에 null 체크는 필수다"** — `delete nullptr`는 표준이 no-op으로 보장한다. `Safe_Delete`의 if문은 delete를 지키는 게 아니라 "이미 null이면 대입 생략" 수준의 관례다. 이걸 "null 체크 안 하면 크래시"라고 답하면 감점.
2. **"스마트 포인터를 쓰면 메모리 문제는 끝"** — `shared_ptr` 순환 참조는 여전히 새고, `std::async`의 future를 버리면 소멸자 블로킹으로 성능 버그가 되고(`CHttpClient.cpp:89-92`), 비소유 `get()` 포인터는 소유자보다 오래 살면 그대로 댕글링이다. 스마트 포인터는 소유권 **표기법**이지 소유권 **설계**를 대신하지 않는다.
3. **"RAII = 스마트 포인터"** — RAII의 대상은 자원 일반이다. COM 초기화(`CScopedCOMInit`), 프로파일러 push/pop(`CProfileScope`), 워커 스레드 join(`~CJobSystem`)처럼 메모리가 아닌 사례를 못 들면 개념을 절반만 아는 것.
4. **"힙은 느리고 위험하니 무조건 피해야 한다"** — 문제는 힙 자체가 아니라 핫 루프의 반복 할당, 불명확한 소유권, 그리고 정렬 조건 같은 계약 위반이다. `WintersMath.h:15-16` 주석이 정확히 이 오해를 겨냥한다: 위험한 건 힙이 아니라 SIMD 저장 타입의 16B 정렬 조건을 놓치는 것.
5. **"소멸자를 썼으면 끝, 복사는 컴파일러가 알아서"** — 소멸자에서 자원을 해제하는 타입의 암묵 복사는 이중 해제 폭탄이다(rule of three). `DX11Buffer`가 살아있는 반례이고, 답변에서 "내 코드에서 이 위반을 찾아 뒀다"고 말할 수 있으면 오히려 강점이 된다.

---

## 다른 챕터와의 연결

- [04_pointers_smart_pointers.md](04_pointers_smart_pointers.md) — unique_ptr/shared_ptr 선택 기준, 비소유 raw 포인터 계약, `release()` 소유권 이관의 세부.
- [05_class_design_value_semantics.md](05_class_design_value_semantics.md) — rule of three/five, 이동 의미론과 noexcept, 복사 제어 삭제.
- [09_concurrency.md](09_concurrency.md) — 스레드 수명 RAII(join 프로토콜), pending IO 지연 파괴, atomic과 memory order.
- [02_compile_link_dll.md](02_compile_link_dll.md) — 불완전 타입과 TU별 실체화, DLL 경계에서의 할당/해제 주체 문제.
