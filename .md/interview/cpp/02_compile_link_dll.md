# 컴파일 모델 · 링크 · DLL 경계

> 대상: Winters 엔진(C++ / DX11, LoL 스타일 클라이언트 + 서버)을 직접 만든 신입~주니어 게임 프로그래머.
> 목표: "빌드가 왜 이렇게 되는가"를 컴파일러/링커/로더 수준으로 설명하고, 내 엔진 코드로 증명한다.

---

## ① 한 줄 본질

**C++ 빌드는 "번역 단위(translation unit)마다 독립 컴파일 → 링커가 심볼(symbol)을 이어붙이기"의 2단계이고, DLL 경계는 그 이어붙이기가 프로세스 로드 시점으로 미뤄지는 지점이다. 그래서 헤더는 '선언(declaration)'을 배포하는 계약서이고, 실제 코드·데이터가 어느 모듈에 몇 벌 존재하는가(정의·ODR)가 링크 에러와 DLL 경계 버그의 전부다.**

---

## ② 기본 개념

### 번역 단위 (Translation Unit)

컴파일러가 한 번에 보는 단위는 `.cpp` 하나 + 전처리기가 `#include`를 전부 펼친 결과물이다. 핵심은 **TU는 서로를 전혀 모른다**는 것. `A.cpp`가 `B.cpp`의 함수를 호출하면, `A.cpp`는 선언만 보고 "이 시그니처의 심볼이 어딘가 있다"는 미해결 참조(unresolved reference)를 오브젝트 파일(.obj)에 남기고, 실제 연결은 링커가 한다. 컴파일 에러와 링크 에러가 완전히 다른 단계에서 나는 이유다.

### 전처리기와 헤더

전처리기는 컴파일러보다 먼저 도는 **순수 텍스트 치환기**다. `#include`는 파일 내용 복붙, `#define`은 토큰 치환, `#if`는 코드 블록 삽입/제거. **전처리기는 C++의 스코프, 네임스페이스, `::`를 전혀 모른다.** 이 "무지"가 `NOMINMAX`, `#define new`, Win32 `min`/`max` 매크로 충돌 같은 함정의 뿌리다. 헤더는 언어 기능이 아니라 "선언을 여러 TU에 복사해 주는 관례"일 뿐이다.

### include guard vs `#pragma once`

같은 헤더가 한 TU에서 두 번 펼쳐지면 타입 재정의 에러가 난다.

| 방식 | 표준 여부 | 특징 |
|---|---|---|
| `#ifndef X_h__` guard | 표준 | 어디서나 동작. 가드 매크로 이름 충돌이 유일한 위험 |
| `#pragma once` | 비표준(사실상 전 컴파일러 지원) | 파일 identity 기반. 같은 파일이 다른 경로로 두 번 보이면(하드링크, 경로 별칭) 드물게 실패 가능 |

둘 다 **"한 TU 안에서의 중복"만 막는다.** 서로 다른 TU가 같은 헤더를 각각 펼치는 것은 정상이며, 그때 무엇이 중복 정의가 되는가는 ODR의 영역이다.

### 선언 vs 정의

- **선언**: 이름과 타입만 알려줌. `void Foo(int);`, `extern int g;`, `class CWorld;` — 몇 번이고 반복 가능.
- **정의**: 코드나 저장 공간을 실제로 만든다. `void Foo(int){...}`, `int g = 0;` — 프로그램 전체에서 원칙적으로 한 번.

전방 선언(forward declaration)만으로 포인터/참조 멤버, 함수 시그니처는 컴파일된다. 완전한 타입(complete type)이 필요한 순간은 크기 계산(멤버로 값 보유, `sizeof`), 멤버 접근, **`delete`**(소멸자 호출) 시점이다 — 이것이 뒤에 나오는 `unique_ptr<불완전 타입>` 함정으로 이어진다.

### ODR (One Definition Rule)

- non-inline 함수/변수: **프로그램 전체에서 정의는 정확히 1개.** 위반 시 링커가 LNK2005(중복 정의)로 잡는다.
- 클래스, `inline` 함수, 템플릿, `inline` 변수(C++17): **TU마다 정의 허용, 단 모든 정의가 토큰 단위로 동일해야 함.** 동일하지 않으면 진단 의무 없는(no diagnostic required) 미정의 동작 — 링커가 아무거나 하나 골라 조용히 쓴다. "헤더에 정의를 두는 것"이 합법이 되는 근거가 전부 이 두 번째 규칙(inline)이다.

### 링커 동작

각 .obj는 "내가 정의한 심볼" + "내가 참조만 한 심볼" 목록을 가진다. 링커는 (1) 모든 .obj/.lib을 모아 심볼 테이블을 만들고 (2) 미해결 참조를 정의와 짝짓고 (3) 못 찾으면 LNK2019, 두 개 찾으면 LNK2005를 낸다. C++은 오버로딩 때문에 함수명을 시그니처를 포함한 장식된 이름(name mangling, 예: `?CreateEntity@CWorld@@QEAA...`)으로 만들며, `extern "C"`는 이 장식을 끈다 — C ABI로 심볼을 노출할 때 쓰는 이유다.

### static 라이브러리 vs DLL

- **정적 라이브러리(.lib)**: .obj 묶음 아카이브. 링크 시 필요한 .obj가 EXE/DLL 안으로 **복사**된다. 배포 단순, 대신 사용하는 모듈마다 코드가 중복되고 전역 상태도 모듈마다 별도 사본이 생긴다.
- **DLL(.dll + import .lib)**: 코드가 별도 모듈에 한 벌 존재하고, 로더가 프로세스 시작(또는 `LoadLibrary`) 때 주소를 연결한다. import lib은 실제 코드가 아니라 "이 심볼은 그 DLL에 있음"이라는 스텁/메타데이터다. **컴파일 시점에 링크되는 것은 import lib이므로, DLL을 새로 빌드해도 오래된 import lib에 링크하면 심볼 불일치가 난다** (Winters의 stale lib 함정, ④ 참조).

### `__declspec(dllexport)` / `__declspec(dllimport)`

Windows에서 DLL 심볼은 기본 비공개다. `dllexport`는 심볼을 export 테이블에 올리고, `dllimport`는 호출부가 `__imp_` 간접 포인터를 통해 부르도록 컴파일한다(dllimport 없이 선언만으로도 링크는 되지만, 컴파일러가 간접 호출 최적화를 못 한다. **데이터 심볼은 dllimport가 필수**). 같은 헤더를 DLL 제작자와 소비자가 공유해야 하므로, 빌드 매크로로 export/import를 토글하는 것이 표준 관용구다(④의 `WINTERS_ENGINE`).

---

## ③ 심화 (꼬리질문 대비)

### DLL 경계에서 STL/할당자가 왜 문제인가

세 겹의 문제가 겹친다.

1. **레이아웃(ABI)**: `std::vector`의 메모리 배치는 표준이 아니라 구현 세부다. DLL과 EXE가 다른 컴파일러 버전/다른 `_ITERATOR_DEBUG_LEVEL`로 빌드되면 같은 `std::vector`가 다른 크기·배치를 가질 수 있다.
2. **힙(CRT) 불일치**: `/MT`(정적 CRT)로 빌드된 모듈은 각자 자기 힙을 가진다. A 모듈이 할당한 메모리를 B 모듈이 `free`/`delete` 하면 힙 손상. `/MD`(DLL CRT)로 통일하면 모든 모듈이 같은 CRT 힙을 공유해 완화된다.
3. **인스턴스화 분산**: 템플릿(STL)은 각 TU에서 인스턴스화되므로, 컨테이너를 조작하는 코드가 DLL 쪽과 EXE 쪽에 각각 존재한다. 양쪽 코드가 동일하다는 보장은 "같은 툴체인·같은 설정"이라는 빌드 규율뿐이다.

MSVC의 **C4251** 경고("dll-interface 필요")가 바로 이 지점을 찌른다. 끄는 것이 정당화되는 조건은 단 하나 — **모든 모듈을 같은 컴파일러, 같은 표준 버전, 같은 `/MD` 계열 런타임으로 빌드한다는 전제**다. 전제 없이 끄면 힙 손상 크래시를 경고 없이 만나게 된다. 근본 해법은 STL을 경계에서 치우는 것: pimpl, 순수 가상 인터페이스, C 스타일 시그니처(호출자 버퍼), 핸들 반환.

### 모듈마다 복제되는 것들

DLL은 링크 단위마다 자기 몫의 정적 데이터를 가진다. 실전에서 얻어맞는 형태:

- **문자열 리터럴**: 같은 철자라도 모듈마다 각자의 `.rdata` 사본. 리터럴 병합(string pooling)은 링커의 모듈 내부 최적화일 뿐, 언어 보장이 아니다. → 포인터 비교 금지.
- **헤더 인라인 static 지역 변수(싱글턴)**: export 없이 헤더에 정의한 `static T instance`는 모듈마다 하나씩 생긴다. 클래스를 `dllexport` 하면 인라인 멤버도 DLL에서 import되어 단일 인스턴스가 보장된다.
- **전역 상태를 가진 서드파티(프로파일러 등)**: 각 모듈이 정적 링크하면 인스턴스가 모듈 수만큼 생긴다. 한 DLL에만 구현부를 두고 나머지는 import 하는 것이 해법.

### 템플릿과 DLL — 왜 export가 안 되나

템플릿은 "코드"가 아니라 "코드 생성 레시피"다. 어떤 `T`로 인스턴스화될지는 **사용하는 쪽 TU가 결정**하므로, DLL을 빌드하는 시점에는 export할 실체가 없다. 그래서 실무 분할은: 템플릿 멤버는 헤더에 본문을 두어 소비자 TU에서 인스턴스화, non-template 멤버만 `dllexport`. `extern template class std::vector<int>;`(명시적 인스턴스화 선언)는 **같은 바이너리 내부에서** "이 TU에서 인스턴스화하지 말고 링크로 해결하라"는 빌드 시간 최적화이지, DLL 경계를 넘는 수단이 아니다.

### MSVC dllexport 클래스의 특수 멤버 강제 인스턴스화

클래스 전체에 `dllexport`를 붙이면 MSVC는 **모든 멤버(암묵적 특수 멤버 포함)를 즉시 인스턴스화해 export**하려 한다. 멤버에 `unique_ptr`가 있으면 암묵적 복사 생성자 생성 시도가 `unique_ptr`의 deleted copy에 걸려 컴파일 에러가 난다. 일반 클래스라면 "복사를 안 쓰면 에러도 없는" 지연 인스턴스화가, dllexport에서는 즉시 터진다. 해법: 복사를 명시적으로 `= delete` 선언해 컴파일러가 복사 경로 생성 자체를 스킵하게 한다. (④의 Winters 확정 gotcha.)

### PCH (Precompiled Header)

전처리 결과(사실상 컴파일러 내부 상태)를 디스크에 저장해 두고 TU마다 재사용하는 빌드 최적화. `/Yc`로 만드는 TU 하나, 나머지는 `/Yu`로 소비. **PCH에 든 헤더가 하나라도 바뀌면 전체 리빌드**이므로, 변경 빈도 0인 외부 헤더(STL/Windows/DirectXMath)만 넣고 프로젝트 내부 헤더는 절대 넣지 않는 것이 규율이다. 부작용: PCH가 주입한 매크로/using에 모든 TU가 암묵 의존하게 되어, PCH 없는 환경(SDK 소비자)에서 컴파일이 깨질 수 있다 → 배포 헤더는 자기 완결(self-contained)로 작성해야 한다.

### 링크 에러 읽는 법 (MSVC)

| 에러 | 의미 | 첫 번째 의심 |
|---|---|---|
| LNK2019 | 미해결 외부 심볼(참조는 있는데 정의 없음) | 정의 .cpp가 프로젝트에 없음 / .lib 미링크 / dllexport 누락 / 선언·정의 시그니처 불일치(mangled name을 undname으로 풀어 비교) |
| LNK2001 | 미해결 외부 심볼(2019와 유사, 주로 변수/vtable) | 순수 가상 미구현, static 멤버 정의 누락 |
| LNK2005 | 심볼 중복 정의 | 헤더에 non-inline 정의를 둠 / 같은 .cpp를 두 프로젝트가 컴파일 |
| LNK4098 | CRT 혼합(/MT vs /MD) | 런타임 라이브러리 설정 불일치 — DLL 경계 힙 문제의 전조 |
| LNK1120 | 미해결 심볼 개수 요약 | 위 에러들의 합계일 뿐, 원인은 개별 LNK2019/2001 |

요령: mangled name 안의 클래스명·시그니처를 읽고, "정의가 없는 것인가(2019) vs 두 벌인가(2005)"부터 가른다. dllimport 심볼은 `__imp_` 접두가 붙으므로, `__imp_` 미해결이면 import lib 미링크를 의심한다.

---

## ④ Winters에서의 적용

### 1. `WINTERS_ENGINE` 3-way export/import/static 매크로

`Engine/Include/WintersAPI.h:11-17`:

```cpp
#ifdef WINTERS_STATIC_BUILD
    #define WINTERS_ENGINE
#elif defined(WINTERS_ENGINE_EXPORTS)
    #define WINTERS_ENGINE __declspec(dllexport)
#else
    #define WINTERS_ENGINE __declspec(dllimport)
#endif
```

같은 헤더가 세 종류의 소비자에게 다르게 컴파일된다: Engine 프로젝트는 `WINTERS_ENGINE_EXPORTS`를 정의해(`Engine/Include/Engine.vcxproj:54`) dllexport, Client/Server는 미정의라 dllimport, 툴/컨버터 EXE는 `WINTERS_STATIC_BUILD`로 빈 매크로(같은 소스를 정적 재컴파일). 서드파티도 같은 기법을 쓴다 — ImGui 심볼이 Engine DLL에 들어가므로 Engine은 `IMGUI_API=__declspec(dllexport)`(`Engine.vcxproj:54`), Client는 `IMGUI_API=__declspec(dllimport)`(`Client/Include/Client.vcxproj:53`)를 **헤더 수정 없이 빌드 설정으로 주입**한다.

### 2. EngineSDK/inc 헤더 배포 + import lib 직접 링크 (레이어 강제와 stale lib 함정)

Client는 Engine에 MSBuild ProjectReference가 없다. `Client/Include/Client.vcxproj:57`이 include 경로에 `EngineSDK\inc`를 넣고, `:69`가 `WintersEngine.lib`(import lib)를 직접 링크한다:

```xml
<AdditionalDependencies>WintersEngine.lib;%(AdditionalDependencies)</AdditionalDependencies>
```

Engine 빌드 후 `UpdateLib.bat`이 헤더/lib/DLL을 EngineSDK로 배포한다(`Engine.vcxproj:74-77` PostBuildEvent). 이 구조의 의미 두 가지:

- **레이어 강제가 include 경로 수준에서 걸린다.** Client의 include 경로에는 `EngineSDK\inc`만 있고 `Engine/Private`은 없으므로, Client가 엔진 내부 구현 헤더를 include 하는 순간 컴파일이 깨진다. "레이어 규칙은 문서가 아니라 vcxproj가 지킨다."
- **stale lib 함정.** import lib는 DLL의 스냅샷 메타데이터이므로, Engine을 고치고 Client만 단독 빌드하면 EngineSDK에 남은 오래된 .lib에 링크될 수 있다 — ProjectReference로 소비하는 Server와 대비되는, import lib 수동 링크의 실제 비용이다.

### 3. C4251 — dllexport 클래스의 STL 멤버

`Engine/Public/Engine_Defines.h:30`은 `#pragma warning(disable : 4251)`로 전역 억제한다. 정당화 전제는 "Engine/Client/Server 모두 같은 VS 툴셋 + `/MD` 계열 런타임(MultiThreadedDebugDLL, `Client.vcxproj:60`)으로 빌드"라는 빌드 규율이다. 반면 `Engine/Public/ECS/World.h:12-15`는 같은 억제를 하면서도 정직한 주석을 남겼다:

```cpp
//왜 여기에 따로 using을 뺀 거지? -> 이거 무조건 수정 절대 헤더에 이렇게 두면 안 됨
// dll-interface 경고 (unordered_map/unique_ptr 멤버) 범위 억제
#pragma warning(push)
#pragma warning(disable: 4251)
```

억제는 봉합이지 해결이 아니라는 인식이다. 정공법의 예가 `Engine/Public/FX/FxAsset.h:218-240`의 `CFxAssetRegistry`: 클래스 전체가 아니라 **public 메서드에만** `WINTERS_ENGINE`을 붙여 export하고, private의 `std::vector<Slot>` 멤버는 export 표면에서 제외하며, 값 대신 `FxAssetHandle`을 주고받는다.

### 4. dllexport + `unique_ptr` 멤버 = 복사 명시 delete (확정 gotcha)

`Engine/Public/ECS/SystemScheduler.h:17-25`:

```cpp
// ★ 중요: WINTERS_ENGINE dllexport 클래스가 unique_ptr 멤버를 포함할 때 필수.
// MSVC 는 dllexport 클래스의 모든 특수 멤버 함수를 강제 인스턴스화/export 하려 함.
// unique_ptr 의 copy 는 deleted → 암묵적 copy ctor 생성 실패 → construct_at 에러.
CSystemSchedular(const CSystemSchedular&) = delete;
CSystemSchedular& operator=(const CSystemSchedular&) = delete;
CSystemSchedular(CSystemSchedular&&) = default;
```

실제 빌드를 깨뜨렸던 사고가 주석과 팀 gotcha(2026-04-23)로 박제되어 있다. ③에서 설명한 "지연 인스턴스화 vs dllexport 즉시 인스턴스화" 차이를 몸으로 증명하는 코드.

### 5. 템플릿 멤버는 헤더, non-template만 export — `CWorld`

`Engine/Public/ECS/World.h:58-77`:

```cpp
// non-template 멤버만 DLL 경계를 넘음 (template 멤버는 header에서 인스턴스화)
WINTERS_ENGINE EntityID CreateEntity();
...
template<typename T> T& AddComponent(EntityID e, const T& c = T{})
{
    return GetOrCreateStore<T>().Add(e, c);
}
```

`AddComponent<TransformComponent>`가 어떤 T로 쓰일지는 Client/Server가 결정하므로 Engine DLL은 미리 export할 수 없다. 그래서 엔티티 수명 관리 같은 non-template API만 경계를 넘고, 템플릿은 헤더 본문으로 소비자 TU에서 인스턴스화된다 — ③의 원리를 그대로 반영한 분할.

### 6. 모듈별 static 복제 — 문자열 리터럴, 싱글턴, Tracy

**문자열 리터럴**: `Engine/Private/Core/Profiler/CPUProfiler.cpp:120-122`의 주석이 사고를 박제한다:

```cpp
// 포인터 비교는 DLL/번역단위 간 동일 리터럴이 다른 주소를 가질 때
// 같은 카운터를 중복 행으로 만든다 (scope 통계와 동일하게 strcmp 비교).
```

같은 `"DrawCall"` 리터럴도 Engine.dll과 Client.exe가 각자의 `.rdata`에 사본을 가지므로 포인터 비교가 실패해 카운터가 중복 행으로 갈라졌다. 수정은 `SameProfilerName`(`CPUProfiler.cpp:20-27`) — 포인터 비교를 fast-path로 두고 `strcmp`로 확정하는 2단 비교.

**싱글턴**: `Engine/Public/Core/CInput.h:15-22`의 `CInput::Get()`은 헤더 인라인 static 지역 인스턴스인데, 클래스가 `class WINTERS_ENGINE CInput`으로 export되어 있어 Engine.dll과 Client.exe가 **같은** 인스턴스를 공유한다. export 없이 헤더 인라인 싱글턴을 두면 모듈마다 인스턴스가 하나씩 생겨 "Client가 누른 키를 Engine이 못 보는" 부류의 버그가 된다.

**전역 상태 서드파티**: `Engine/Private/Core/Profiler/TracyClientImpl.cpp:1-8`은 Tracy 구현부(`TracyClient.cpp`)를 Engine DLL의 **단일 TU**에서 `TRACY_EXPORTS`로 컴파일하고, Client/Server는 `Engine/Include/ProfilerAPI.h:10-14`에서 `WINTERS_ENGINE_EXPORTS` 유무로 `TRACY_IMPORTS`를 선택해 같은 인스턴스에 기록한다. 각 모듈이 정적 링크했다면 프로파일 데이터가 모듈 수만큼 갈라졌을 것이다.

### 7. DLL 경계 API 설계 — C 스타일 시그니처와 파사드 + 전방 선언

`Engine/Include/WintersPaths.h:12`:

```cpp
WINTERS_ENGINE bool WintersResolveContentPath(const wchar_t* relativePath, wchar_t* outFullPath, uint32_t outCapacityChars);
```

`std::wstring`을 반환하지 않고 호출자 소유 버퍼 + 용량 계약을 쓴다 — STL을 경계에서 치우는 ③의 정공법. 파사드 `Engine/Include/WintersEngine.h`는 "Client는 이 헤더 하나만 include"라는 통합 인클루드로, `:20`에서 `class IRHIDevice;` 전방 선언만 하고 `:37`의 `WintersGetRHIDevice()`가 포인터를 반환한다. 포인터만 오가면 완전한 타입이 필요 없다는 규칙으로 DX11 헤더가 Client TU로 전파되는 것을 끊는 컴파일 방화벽이다.

### 8. PCH 규율 — 불변 외부 헤더만, NOMINMAX

`Engine/Public/WintersPCH.h:2-4`:

```cpp
// Winters Engine Precompiled Header
// ── 절대 변경되지 않는 외부 헤더만 여기에 ──
// ── 엔진 내부 헤더(.h)는 절대 넣지 않는다 ──
```

STL/Windows/DirectXMath만 담고, `Windows.h` 앞에 `WIN32_LEAN_AND_MEAN`(빌드 시간)과 `NOMINMAX`(`min`/`max` 함수형 매크로가 `std::max` 호출을 토큰 치환으로 파괴하는 고전 충돌 차단)를 건다(`:45-51`). Engine.vcxproj는 `PrecompiledHeader=Use` + `ForcedIncludeFiles=WintersPCH.h`(`Engine.vcxproj:63-65`)로 전 TU에 강제 주입한다. 한편 `:121-148`의 전역 `using std::vector; ... using std::mutex;`는 PCH를 쓰는 모든 TU에 무접두 이름을 주입하는 유산 트레이드오프로, "공개 헤더 using 누출 금지" 팀 gotcha와 긴장 관계다 — 그래서 신규 배포 헤더는 `std::` 완전 수식으로 작성해 PCH 없는 컨텍스트(EngineSDK 소비자, `World.h:7-10`의 명시 include가 그 예)에서도 자기 완결로 컴파일되게 격리한다.

### 9. 전처리기 위생 — `#define new`, 함수명 remap, 헤더 없는 dllimport 재선언

**`#define new` 충돌**: `Engine/Public/Engine_Defines.h:38-41`은 `_DEBUG`에서 `new`를 `DBG_NEW(_NORMAL_BLOCK, __FILE__, __LINE__)`로 치환해 CRT 누수 추적을 건다. 이 매크로는 placement new 문법을 파괴하므로, placement new를 쓰는 서드파티 헤더는 격리한다 — `Engine/Include/ProfilerAPI.h:16-19`(Tracy), `Client/Private/Network/Backend/AuthClient.cpp:2-5`(nlohmann json):

```cpp
#pragma push_macro("new")
#undef new
#include "Network/Backend/json.hpp"
#pragma pop_macro("new")
```

**함수명 remap과 정의 순서**: `Engine_Defines.h:91-98`은 `OutputDebugStringA`를 게이트 래퍼 `WintersOutputDebugStringA`로 `#define` 리매핑한다. 래퍼 함수들(`:54-88`)이 매크로 정의보다 **먼저** 정의되므로 함수 내부의 `::OutputDebugStringA`는 진짜 Win32 API로 컴파일되고, 이후의 호출만 래퍼로 치환된다. 전처리기는 스코프와 `::`를 모르기 때문에 **정의 순서가 곧 정확성**이다.

**선언·정의 분리를 이용한 오염 차단**: `Shared/GameSim/Core/Debug/SimDebugOutput.h:21-24`는 `Engine_Defines.h`(`<dinput.h>`, `using namespace std` 오염, `:14-17`)를 Shared TU로 끌어오지 않으려고 필요한 Win32 심볼만 직접 재선언한다:

```cpp
extern "C" __declspec(dllimport) void __stdcall OutputDebugStringA(const char* pText);
```

선언은 헤더가 아니라도 되고, 링커가 kernel32 import lib에서 같은 심볼을 찾는다 — "선언과 정의(링킹)의 분리"를 무기로 쓴 사례. 같은 가드 매크로(`WINTERS_DEBUG_STRING_GATE_DEFINED`)를 Engine_Defines.h와 공유해 한 TU에서 만나도 먼저 온 정의가 이기도록 수동 once-guard를 걸었다.

**헤더 상수의 최종형**: `Client/Public/GameObject/ChampionDef.h:13`의 `inline constexpr uint32_t kChampionTextureSlotMax = 8;`, `Engine/Include/WintersMath.h:77-79`의 `inline constexpr float kPi = ...` — C++17 inline 변수라 여러 TU에 포함돼도 단일 엔티티로 병합되어 ODR 안전하고, 배열 크기(`ChampionDef.h:28`)로 쓸 수 있는 컴파일 타임 상수다.

**내부 링키지**: `Client/Private/GameObject/FX/FxCuePlayer.cpp:17`의 익명 네임스페이스(`namespace { ... }`)는 20여 개 헬퍼를 TU 내부에 가둬 다른 TU의 동명 함수와 ODR 충돌을 원천 차단하고 링커 심볼 표면을 줄인다. `static` 함수와 달리 타입/템플릿에도 적용되는 현대적 관례.

### 10. `unique_ptr<불완전 타입>` 멤버 → 소멸자 out-of-line

`Server/Public/Game/GameRoom.h:35-49`는 `Engine::CNavGrid` 등을 전방 선언만 하고 `:207-220`에서 `std::unique_ptr` 멤버로 소유한다. `unique_ptr`의 기본 deleter는 파괴 지점에서 완전 타입을 요구하므로 소멸자를 헤더에 두면 컴파일이 깨진다. 그래서 `~CGameRoom();`은 선언만(`GameRoom.h:55`), 정의는 완전 타입이 보이는 `GameRoom.cpp:139-142`에 둔다. pimpl과 동일한 컴파일 방화벽 — 헤더 의존을 끊는 대가로 특수 멤버의 정의 위치가 강제된다.

---

## ⑤ 면접 Q&A

**Q1. 헤더에 함수 정의를 넣었더니 LNK2005가 났다. 왜이고, 어떻게 고치나?**
- 포인트: 헤더는 여러 TU에 복붙되므로 non-inline 정의가 TU 수만큼 생겨 ODR 위반 → 링커 중복 정의. 해법은 `inline` 지정(TU마다 정의 허용 + 링커 병합), 정의를 .cpp로 이동, 또는 C++17 `inline constexpr` 변수.
- Winters: `WintersMath.h:77`의 `inline constexpr kPi`, `ChampionDef.h:13`의 `kChampionTextureSlotMax`가 헤더 상수의 최종형. 반대로 TU에 가둘 것은 `FxCuePlayer.cpp:17`처럼 익명 네임스페이스로 내부 링키지를 준다.

**Q2. `__declspec(dllexport)`와 `dllimport`의 차이는? 같은 헤더를 DLL 쪽과 소비자 쪽이 어떻게 공유하나?**
- 포인트: export는 심볼을 DLL export 테이블에 올리고, import는 호출부를 `__imp_` 간접 참조로 컴파일(데이터 심볼은 필수, 함수는 최적화). 빌드 매크로로 토글하는 관용구가 표준.
- Winters: `WintersAPI.h:11-17`의 3-way 분기(export/import/static-empty). 서드파티까지 같은 기법 — `IMGUI_API`를 Engine은 dllexport, Client는 dllimport로 vcxproj에서 주입.

**Q3. DLL 경계에서 `std::string`을 주고받으면 왜 위험한가?**
- 포인트: (1) STL 레이아웃은 구현 세부라 모듈 간 ABI 불일치 가능 (2) 다른 CRT면 다른 힙 — 한쪽이 할당한 것을 다른 쪽이 해제하면 힙 손상 (3) 같은 툴체인/`/MD` 통일이라는 빌드 규율 하에서만 안전. C4251이 이 지점의 경고다.
- Winters: 전제(동일 툴셋 + MultiThreadedDebugDLL)를 깔고 `Engine_Defines.h:30`에서 C4251 억제. 정공법은 `WintersPaths.h:12`의 호출자-버퍼 C 시그니처와 `FxAsset.h`의 메서드 단위 export + 핸들 반환.

**Q4. 템플릿 함수를 DLL로 export할 수 있나?**
- 포인트: 불가. 어떤 T로 인스턴스화될지 소비자 TU가 결정하므로 DLL 빌드 시점에 실체가 없다. non-template API만 export하고 템플릿은 헤더 본문으로 소비자 측 인스턴스화. `extern template`은 같은 바이너리 안의 중복 인스턴스화를 줄이는 빌드 최적화일 뿐 DLL 경계 수단이 아니다.
- Winters: `World.h:58-77` — `CreateEntity`류만 `WINTERS_ENGINE`, `AddComponent<T>`는 헤더 정의.

**Q5. dllexport 클래스에 `unique_ptr` 멤버를 넣었더니 컴파일 에러가 났다. 왜?**
- 포인트: MSVC는 dllexport 클래스의 모든 특수 멤버를 즉시 인스턴스화/export하려 한다. 암묵적 복사 생성자 생성이 `unique_ptr`의 deleted copy에 걸려 실패. 일반 클래스의 지연 인스턴스화와 다르다. 해법은 복사 `= delete` 명시.
- Winters: `SystemScheduler.h:17-25`에 주석으로 박제된 확정 gotcha. `CWorld`(`World.h:52-56`)도 동일 패턴.

**Q6. 같은 철자의 문자열 리터럴 두 개는 주소가 같은가?**
- 포인트: 보장 없음. 리터럴 병합은 링커의 모듈 내부 최적화이고 DLL 경계는 절대 넘지 않는다. 동일성 판단은 내용 비교로.
- Winters: `CPUProfiler.cpp:120-122` — Engine.dll과 Client.exe의 리터럴 주소가 달라 카운터가 중복 행으로 갈라진 실제 버그. 포인터 fast-path + `strcmp` 확정의 `SameProfilerName`으로 수정.

**Q7. 헤더에 정의한 Meyers 싱글턴이 DLL 환경에서 두 개가 될 수 있는 이유는?**
- 포인트: static 지역 변수는 그 함수의 정의가 사는 모듈에 귀속된다. 헤더 인라인이면 함수 정의가 모듈마다 생겨 인스턴스도 모듈마다 생긴다. 클래스를 dllexport하면 인라인 멤버가 DLL에서 import되어 단일화.
- Winters: `CInput.h:15-22`(export된 클래스의 인라인 `Get()`), 그리고 전역 상태 서드파티의 단일화인 `TracyClientImpl.cpp`(단일 TU export) + `ProfilerAPI.h:10-14`(소비자 import).

**Q8. PCH에는 무엇을 넣고 무엇을 넣으면 안 되나?**
- 포인트: 변경 빈도 0인 외부 헤더만(STL/OS/수학). 내부 헤더를 넣으면 그 헤더 수정 = 전체 리빌드. 부수 규율로 PCH 주입 매크로/using에 의존하지 않는 자기 완결 배포 헤더 작성.
- Winters: `WintersPCH.h:2-4`의 규율 주석 + `NOMINMAX`/`WIN32_LEAN_AND_MEAN` 선행 정의(`:45-51`). 전역 `using std::*`(`:121-148`)는 유산 트레이드오프로, 신규 헤더는 `std::` 완전 수식으로 격리.

---

## ⑥ 흔한 오답/함정

1. **"`#pragma once`를 쓰면 ODR 문제는 없다."** — 틀림. include guard류는 *한 TU 안*의 중복 펼침만 막는다. 서로 다른 TU가 같은 헤더를 펼치는 것은 정상이고, 그때 non-inline 정의가 들어 있으면 그대로 LNK2005다. TU 간 문제는 `inline`/ODR의 영역.

2. **"C4251은 MSVC가 유난 떠는 경고라 그냥 꺼도 된다."** — 조건부로만 참. "전 모듈 동일 컴파일러 + 동일 `/MD` 런타임"이라는 전제를 빌드 시스템이 실제로 지킬 때만 안전하다. 전제가 깨지면(다른 VS 버전, `/MT` 혼용) 경고 없이 힙 손상 크래시가 난다. Winters도 `World.h:12`에 "이렇게 두면 안 됨"이라는 자기 인식 주석을 남기고, 신규 API는 핸들/메서드 단위 export로 우회한다.

3. **"`inline`은 함수 호출을 인라인 확장하라는 힌트다."** — 현대 C++에서 `inline`의 실질 의미는 "TU마다 정의를 허용하고 링커가 병합한다"는 ODR 완화다. 인라인 확장 여부는 옵티마이저가 독자적으로 결정한다. C++17 `inline` 변수까지 오면 완전히 링크 의미론이다.

4. **"DLL을 다시 빌드했으니 EXE는 그대로 실행하면 된다."** — export된 클래스의 멤버 배치·인라인 함수·시그니처가 바뀌었으면 import lib과 소비자 재컴파일이 필요하다. 특히 Winters Client처럼 import lib을 수동 링크(ProjectReference 없음, `Client.vcxproj:69`)하면 오래된 .lib에 링크되는 stale lib 함정이 있다 — "링크되는 것은 DLL이 아니라 import lib"임을 모르면 원인 불명의 심볼/동작 불일치를 만난다.

5. **"static 함수와 익명 네임스페이스는 완전히 같다."** — 둘 다 내부 링키지를 주지만, 익명 네임스페이스는 타입·템플릿에도 적용되고(`static`은 함수/변수만), TU-로컬 타입은 고유 typeid를 가져 템플릿 인자로도 충돌 없이 쓸 수 있다. 현대 C++ 권장은 익명 네임스페이스(`FxCuePlayer.cpp:17`).

---

## 다른 챕터와의 연결

- [01_cpp_essence.md](01_cpp_essence.md) — 빌드 파이프라인이 "제로 오버헤드·정적 타입" 철학과 어떻게 맞물리는지의 총론.
- [04_pointers_smart_pointers.md](04_pointers_smart_pointers.md) — `unique_ptr<불완전 타입>`의 deleter 규칙, pimpl과 소유권.
- [07_templates_generic.md](07_templates_generic.md) — 인스턴스화 모델, `extern template`, 헤더-온리 강제의 근본 이유.
- [13_interview_qa_bank.md](13_interview_qa_bank.md) — 이 챕터의 Q1~Q8을 포함한 전체 질문 은행.
