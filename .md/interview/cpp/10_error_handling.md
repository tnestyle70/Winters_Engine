# 에러 처리 전략

## ① 한 줄 본질

에러 처리는 "실패를 어떤 채널로 전파하고(예외 vs 반환값), 실패 후 프로그램 상태를 어디까지 보장하며(예외 안전성 3단계), 실패 사실을 어떻게 반드시 관측 가능하게 남기는가(진단)"의 3축 설계이며, 게임 엔진은 프레임 결정론과 비용 예측성 때문에 대부분 반환값 기반 모델을 선택한다 — Winters도 `WINTERS_ERROR_HANDLING_POLICY.md`로 "예외는 쓰지 않는다, 실패 진단은 반드시 방출한다"를 계층별 규약으로 고정했다.

---

## ② 기본 개념

### 예외 메커니즘 — throw가 실제로 하는 일

`throw expr;`이 실행되면:

1. **예외 객체 생성**: `expr`을 복사/이동해 예외 객체(exception object)를 만든다. 이 객체는 일반 스택 프레임이 아니라 구현이 관리하는 별도 저장소에 놓인다(스택 프레임은 곧 되감겨 사라지므로).
2. **스택 되감기(stack unwinding)**: 매칭되는 `catch` 핸들러를 찾을 때까지 호출 스택을 거슬러 올라가며, 각 프레임의 **완전히 생성된 지역 객체들의 소멸자(destructor)를 역순으로 호출**한다. RAII가 예외 안전의 근간인 이유가 이것 — 되감기 중에도 소멸자는 반드시 실행된다.
3. **핸들러 매칭**: `catch`의 타입과 예외 객체의 타입(및 public 상속 관계)으로 매칭. 값이 아닌 **참조로 잡아야(catch by reference)** 슬라이싱(slicing)이 없다.
4. 핸들러를 끝까지 못 찾으면 `std::terminate()`. 되감기 중 소멸자가 또 throw해도 `std::terminate()` — 그래서 **소멸자는 사실상 noexcept**여야 한다(C++11부터 소멸자는 암묵 `noexcept(true)`).

### 비용 모델 — "zero-cost"의 정확한 의미

- 모던 64비트 ABI(Itanium ABI, MSVC x64 `/EHsc`)는 **테이블 기반(table-driven)** 모델: try 블록 진입 자체는 런타임 명령이 거의 없고, 대신 컴파일러가 "이 PC 범위에서 예외가 나면 어떤 소멸자를 부르고 어디로 가라"는 되감기 테이블을 바이너리에 심는다.
- 따라서 **happy path는 거의 공짜, throw는 매우 비싸다**(테이블 탐색 + 되감기, 수천~수만 사이클 수준). "zero-cost"는 "안 던질 때 0에 가깝다"는 뜻이지 throw가 싸다는 뜻이 아니다.
- 숨은 비용: 바이너리 크기 증가, "이 함수가 던질 수 있다"는 가정이 인라이닝/코드 재배치 최적화를 제약, 그리고 **모든 코드 경로가 예외 안전해야 한다는 설계 비용**.

### 예외 안전성 보장(exception safety guarantee) 3단계 (+1)

| 단계 | 보장 내용 | 대표 수단 |
|---|---|---|
| basic | 누수 없음, 불변식(invariant) 유지. 값은 유효하지만 미지정일 수 있음 | RAII |
| strong | 실패 시 **호출 전 상태 그대로**(commit-or-rollback) | copy-and-swap |
| nothrow(no-fail) | 절대 실패하지 않음 | `noexcept`, swap, 소멸자 |

(네 번째는 "no guarantee" — 보장 없음, 즉 버그.) strong을 만드는 표준 관용구가 **copy-and-swap**: 던질 수 있는 작업(복사)을 전부 사본 위에서 끝낸 뒤, 마지막 커밋만 nothrow인 `swap`으로 한다.

### noexcept

- `noexcept` 함수에서 예외가 밖으로 나가면 **컴파일 에러가 아니라 `std::terminate()`** (스택 되감기 수행 여부는 구현 정의).
- 핵심 실용 효과: **`std::vector` 재할당**. `push_back` 재할당 시 요소 이동 생성자가 `noexcept`면 move, 아니면 strong guarantee를 지키기 위해 **copy로 폴백**한다(`std::move_if_noexcept`). 이동 생성자에 `noexcept`를 빼먹으면 컨테이너 성능이 조용히 복사 수준으로 떨어진다.
- 컴파일타임 질의 연산자 형태도 있다: `noexcept(expr)`은 expr이 던질 수 있는지 bool로 평가.

### 게임/엔진이 예외를 꺼리는 이유

1. **비용 비대칭**: 16.6ms 프레임 예산에서 throw 한 번의 비결정적 수천 사이클은 스파이크. "실패"가 게임에서는 예외 상황이 아니라 **정상 흐름**(경로 없음, 에셋 미스, 패킷 드랍)이라 throw 빈도 가정 자체가 깨진다.
2. **전 경로 예외 안전 요구**: 예외를 켜면 코드베이스 전체가 최소 basic guarantee를 지켜야 한다. 수동 리소스(GPU 핸들, COM)를 많이 다루는 엔진에서 검증 비용이 크다.
3. **경계 호환성**: DLL 경계·C API·콜백(Win32, COM, 잡시스템 워커)을 예외가 넘어가면 UB이거나 terminate. COM은 애초에 HRESULT 모델이다.
4. **바이너리 크기/최적화 제약**, 콘솔 플랫폼의 관례적 `-fno-exceptions`.

### 반환값 계열 스타일 비교

- **에러 코드/enum**: 가장 싸고 명시적. 단점은 무시 가능 → `[[nodiscard]]`로 완화. 실패 "원인 구분"이 필요하면 bool이 아니라 enum class.
- **nullptr / 무효 핸들**: 팩토리·리소스 생성의 관례. "값 자체가 실패를 표현"하므로 별도 채널 불필요.
- **`std::optional<T>`(C++17)**: "값이 없을 수 있음"만 표현, **왜 없는지는 전달 못 함**.
- **`std::expected<T,E>`(C++23)**: 값 또는 에러를 한 타입에 — 에러 코드의 명시성과 예외의 정보량을 결합. C++17 프로젝트에서는 "값 + out-param enum" 또는 result struct로 같은 효과를 낸다.
- **HRESULT**: Windows/COM의 32비트 에러 코드. 최상위 비트가 severity라서 `FAILED(hr)`/`SUCCEEDED(hr)`는 부호 비트 검사 매크로다. 성공 코드가 여러 개(S_OK, S_FALSE)라 `hr == S_OK` 비교와 `SUCCEEDED(hr)`는 다르다.

### assert vs 런타임 검증

- `assert`는 **프로그래머 오류(불변식 위반)** 검출용. `NDEBUG` 정의 시 완전히 컴파일 아웃되므로 **릴리스에서 실행되어야 하는 검증에 쓰면 안 된다**.
- **외부 입력(파일, 네트워크 바이트, 데이터 테이블)은 assert 대상이 아니라 런타임 검증 + 진단 대상**이다. 신뢰 경계(trust boundary)를 넘어온 데이터는 릴리스에서도 항상 검사해야 한다.

### 실패 진단은 반드시 방출 — dead diagnostics 금지

실패 경로에서 메시지를 `sprintf_s`로 포맷만 하고 출력하지 않는 코드(dead diagnostics)는 "로그가 있다"는 착각만 남기고 실제로는 침묵 실패다. 반대로 무제한 로그는 스팸으로 진짜 신호를 묻는다. 절충이 **bounded logging**: 함수 지역 `static` 카운터로 처음 N회만 방출.

---

## ③ 심화 (꼬리질문 대비)

### 생성자와 실패 — 예외 없는 코드베이스의 정답

생성자는 반환값이 없어 실패를 표현할 채널이 예외뿐이다. 예외를 안 쓰면:

- **2단계 초기화(two-phase init)**: ctor는 실패 불가능한 멤버 세팅만, `Initialize()`가 HRESULT/bool 반환.
- 문제는 "Initialize를 잊은 반쪽 객체"가 돌아다니는 것 → **private ctor + static `Create()` 팩토리**로 봉인. Create만이 ctor→Initialize를 수행하고, 실패 시 `nullptr` 반환. 외부에는 **완전히 초기화된 객체 아니면 nullptr** 두 상태만 존재한다.
- 이때 `std::make_unique<T>()`는 **컴파일 에러**다: make_unique는 라이브러리의 자유 함수라 그 내부의 `new T()`가 private ctor에 접근할 수 없다(접근 제어는 friend가 아닌 한 호출 위치 기준). 그래서 클래스 자신의 멤버인 Create 안에서 `std::unique_ptr<T>(new T())`로 직접 감싼다.

### noexcept 위반과 terminate 경로

`noexcept` 함수 탈출 예외 → `std::terminate()` → 기본 handler는 `abort()`. 이는 "계약 위반이면 복구 시도보다 즉사가 낫다"는 설계다. 같은 이유로 되감기 중 이중 예외도 terminate — 예외 모델 자체가 "동시에 두 개의 에러 전파"를 허용하지 않는다.

### scope guard — finally 없는 C++의 관용구

C++에는 `finally`가 없다. 대신 소멸자 실행이 결정적(deterministic)이라는 성질로, **지역 struct의 소멸자에 정리 코드를 넣으면 모든 return 경로(그리고 예외 경로)에서 실행이 보장**된다. `std::scope_exit`(Library Fundamentals TS)나 `gsl::finally`가 이 패턴의 라이브러리화다.

### 신뢰 경계에서의 검증 계층

네트워크 바이트를 구조체로 역참조하기 전 단계별 방어:

1. **프레이밍**: TCP는 스트림이라 메시지 경계가 없다 → length-prefix 파서가 부분 수신(NeedMore)/손상(Invalid)/완성(Complete)을 구분하고, 길이 필드에 상한을 둬 악성 길이 DoS를 막는다.
2. **스키마 검증**: FlatBuffers는 zero-copy라 `GetXxx()`가 곧 포인터 산술 — 손상 버퍼를 그대로 역참조하면 OOB 읽기다. 반드시 `flatbuffers::Verifier`로 오프셋/범위를 선검증.
3. **실패의 관측 가능성**: verify 실패를 bare `return`으로 삼키면 "스키마 드리프트"와 "네트워크 정지"가 화면상 동일한 **조용한 월드 프리즈**가 된다. 복제 경계 실패는 bounded 로그/카운터 필수.

### 폴백(fallback)의 은폐성

실패 시 기본값으로 계속 진행하는 폴백은 크래시는 막지만 **데이터 오구성을 "미묘하게 이상한 동작"으로 위장**시킨다. 폴백을 두려면 반드시 진입 시 진단을 남겨서 폴백이 제2의 truth가 되는 것을 막아야 한다.

---

## ④ Winters에서의 적용

### 정책 문서가 에러 모델을 고정

`.md/architecture/WINTERS_ERROR_HANDLING_POLICY.md` — "예외는 쓰지 않는다. 반환값 기반(bool/HRESULT/nullptr/핸들 IsValid)이 기본"을 §0에 명시하고, §1에서 실패 진단 규약(모든 실패 exit에 진단, bounded가 기본: 실패류 8 / 컨텐츠 miss류 64 / 계측류 512, dead diagnostics 금지)을 고정했다. 계층별 컨벤션(Engine 팩토리=nullptr, RHI=무효 핸들 `{}`, Server=bool+`std::cerr`, Client/Shared=bounded OutputDebugStringA)까지 실측 기반으로 규정. **면접 포인트**: "예외 안 씀"이 습관이 아니라 문서화된 팀 규약이고, 유일한 throw 경계까지 명시했다는 것.

### 유일한 throw 경계 — CBinaryReader

`Engine/Private/AssetFormat/Common/BinaryReader.cpp:16` — cooked asset 파싱에서 EOF 초과 읽기 시 `throw std::runtime_error("CBinaryReader: read past EOF")`. 소비자인 `Engine/Private/AssetFormat/Mesh/WMeshLoader.cpp:101`이 `catch (const std::exception&)`로 잡아 `return false`로 변환한다. 파서 깊은 곳의 수십 개 read 지점마다 bool 체크를 퍼뜨리는 대신, **좁은 모듈 내부에서만 예외로 모아 경계에서 반환값으로 바꾸는** 절충 — "예외 금지"에도 예외 사용이 국소적으로 정당한 케이스를 설명할 수 있다.

### private ctor + Create() 팩토리 — 생성 실패의 표준 경로

`Engine/Private/Core/CTimer.cpp:40`:

```cpp
unique_ptr<CTimer> CTimer::Create()
{
    auto pInstance = unique_ptr<CTimer>(new CTimer());
    if (FAILED(pInstance->Initialize()))
        return nullptr;
    return pInstance;
}
```

`Engine/Private/RHI/DX11/CDX11Device.cpp:953`도 동일 패턴에 실패 로그를 추가(`[CDX11Device] Create() failed`). `make_unique`가 아닌 `unique_ptr<T>(new T())`인 이유 = private ctor 접근 제어. Initialize 실패 시 `return nullptr` 하면 지역 unique_ptr이 반쪽 객체를 자동 파괴 — RAII가 에러 경로 정리를 대신한다.

### HRESULT 다단 폴백 — 우아한 성능 저하

`Engine/Private/RHI/DX11/CDX11Device.cpp:839-857` — `D3D11CreateDeviceAndSwapChain` 실패 시 (1) 디버그 레이어(Graphics Tools) 미설치를 가정해 `D3D11_CREATE_DEVICE_DEBUG` 플래그를 떼고 재시도, (2) 고성능 어댑터 실패면 OS 기본 어댑터(nullptr)로 재시도. 각 폴백 진입마다 `OutputDebugStringA`로 사유를 남긴다. COM의 HRESULT 모델에서 반환값 검사로 복구 사다리를 만드는 전형. 텍스처 쪽도 동일 모델: `Engine/Private/RHI/RHITextureLoader.cpp:33`의 `LogTextureLoadFailure`가 WIC 각 COM 스테이지 실패마다 "스테이지명+경로+HRESULT"를 실제로 emit하고 `return {}`(무효 핸들)로 전파한다.

### 숨은 return을 가진 매크로 + do-while(0)

`Engine/Public/WintersCore.h:12`:

```cpp
#define WINTERS_HR_CHECK(hr, msg)           \
    do {                                     \
        if (FAILED(hr)) {                    \
            OutputDebugStringA(msg);         \
            __debugbreak();                  \
            return false;                    \
        }                                    \
    } while(0)
```

`do{...}while(0)`는 `if (x) WINTERS_HR_CHECK(...); else ...` 같은 문맥에서 매크로가 단일 문장으로 안전하게 확장되게 하는 표준 이디엄. 동시에 이 매크로는 **호출한 함수에서 return을 수행하는 숨은 제어 흐름**이라 bool 반환 함수에서만 쓸 수 있다는 암묵 계약이 생긴다 — 편의와 위험을 한 코드로 설명 가능. 같은 헤더의 `WINTERS_LOG`(26-30행)는 `##__VA_ARGS__`로 빈 가변인자를 처리하고 릴리스에서 `((void)0)`으로 완전 제거된다.

### 실패 사유 enum out-param — silent fail 사고의 재발 방지

`Engine/Public/Manager/Navigation/Pathfinder.h:13`:

```cpp
enum class ePathFindResult : u8_t
{
    Success = 0, NullGrid, StartBlocked, GoalBlocked, NoRoute, BrokenPath,
};
```

과거 미니언 제자리 stuck 사고의 원인이 "빈 vector 하나로 여러 실패 원인이 뭉개짐"이었다(헤더 주석에 사고 이력 명시). `Find_Path(..., ePathFindResult* pOutResult = nullptr)` 선택적 out-param으로 관심 있는 호출자만 원인을 받게 했고, `Pathfinder.cpp:426-447`에서 각 실패 지점이 코드를 채운다. **반환값(경로)과 진단(사유)의 분리** + underlying type `u8_t` 지정까지 언급하면 좋다.

### 소멸자 scope guard — 모든 탈출 경로에서 카운터 flush

`Engine/Private/Manager/Navigation/Pathfinder.cpp:450`:

```cpp
struct CounterFlush
{
    const char* pName;
    const u32_t& count;
    ~CounterFlush() { WINTERS_PROFILE_COUNT(pName, static_cast<i32_t>(count)); }
} guard{ pNodesCounterName, nodesVisited };
```

FindPathInternal이 StartBlocked 조기 return이든 정상 완료든 어느 경로로 나가도 nodesVisited가 정확히 한 번 flush된다. `count`를 참조로 잡아 **스코프 종료 시점의 최종값**을 읽는 디테일까지.

### FlatBuffers Verifier + bounded trace — 조용한 월드 프리즈 방지

클라이언트 수신부 `Client/Private/Network/Client/SnapshotApplier.cpp:485-497`:

```cpp
flatbuffers::Verifier verifier(payload, len);
if (!Shared::Schema::VerifyHelloBuffer(verifier))
{
    static u32_t s_helloVerifyFailLogCount = 0;
    if (s_helloVerifyFailLogCount < 8) { /* sprintf_s + OutputDebugStringA */ ++s_helloVerifyFailLogCount; }
    return;
}
```

서버 수신부 `Server/Private/Network/PacketDispatcher.cpp:73-79`는 verify 실패 시 드랍에 그치지 않고 `pSession->FlagSuspicious()`로 세션을 표시한다 — 클라는 진단, 서버는 진단+격리라는 역할 차이. gotcha(2026-07-09)가 이 규약의 기원: bare return만 하면 스키마 드리프트와 네트워크 정지가 구분 불가능하다.

### TCP 프레이밍의 3상태 반환 + DoS 상한

`Server/Private/Network/FrameParser.cpp:13` — `TryPop`이 `NeedMore/Invalid/Complete` 3상태 enum을 반환한다. 헤더는 `std::memcpy`로 정렬된 struct에 복사(포인터 캐스팅의 정렬/strict-aliasing UB 회피), `hdr.magic`/`hdr.version` 불일치나 `hdr.payloadSize > kMaxPayloadBytes`면 Invalid + `Clear()`. "부분 수신은 에러가 아니다"를 타입으로 표현한 사례 — bool이었다면 NeedMore와 Invalid가 또 뭉개졌을 것이다.

### bounded 진단의 실전 배치

- `Client/Private/GameObject/ChampionSpawnService.cpp:171` — 스폰 실패(no-def)를 `static u32_t s_spawnNoDefLogCount`로 8회까지 로그. 183행에서는 `std::make_unique<ModelRenderer>` 직후 `Initialize` 실패 시 로그 후 그냥 `return result;` — 지역 unique_ptr이 자동 파괴하므로 에러 경로에 delete가 없다.
- `Server/Private/Game/CommandIngress.cpp:111-118` — 계측 트레이스(`TraceCommandTiming`)는 64회 상한 후 조기 return.
- `Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.cpp:142-151` — **폴백 스탯 진입 시** 16회까지 `WintersOutputAIDebugStringA`로 방출. 주석이 의도를 박제: "생성 테이블에 없는 챔피언이 범용 스탯으로 조용히 살아나면 데이터 팩 오구성이 '미묘하게 이상한 챔피언'으로만 보인다." 폴백 + 진단 세트의 모범.
- 자진 언급용 주의점: 이 `static` 카운터 `++`는 원자적이지 않다. 스폰/틱이 메인 스레드 전제라 실무상 안전하지만, 멀티스레드 경로라면 `std::atomic<u32_t>`가 필요하다 — 전제 조건을 아는 것이 시니어 신호.

### 디버그 출력 게이트 — 계층별로 다르게 침묵하는 로그 채널

`EngineSDK/inc/Engine_Defines.h:44-98` — `OutputDebugStringA`를 매크로로 `WintersOutputDebugStringA`에 리매핑(93행)하고, 래퍼는 `WINTERS_ENABLE_NON_AI_DEBUG_STRING`(기본 0=무음) 게이트 뒤에서 `(void)pText`로 사라진다. Engine/Client Debug 구성만 게이트=1이므로 Server/GameSim에서 이 경로는 침묵 — 그래서 정책상 Server 실패 로그는 `std::cerr`, sim 진단은 `Shared/GameSim/Core/Debug/SimDebugOutput.h:44`의 `WintersOutputAIDebugStringA`(별도 게이트 `WINTERS_ENABLE_AI_DEBUG_STRING`, 기본 1)를 쓴다. inline 함수라 헤더 다중 포함에도 ODR 안전하고, 함수 시그니처는 남으므로 호출부에 `#if`가 번지지 않는다. **함수명 매크로 치환이 계층마다 다른 결과를 낳는 전역 상태**라는 위험까지 짚으면 균형 잡힌 답.

### 반례(자기 비판 소재) — 게이트를 우회한 std::cout

`Shared/GameSim/Champions/Ashe/AsheGameSim.cpp:205` — `std::cout << "[AsheSim] W volley caster=" ...`. 게이트된 sim 진단 채널을 일부러 만들어놓고 정작 챔피언 훅은 raw iostream으로 출력한다. 릴리스에서 컴파일 아웃되지 않고, 권위 시뮬레이션 hot path에 동기 I/O를 섞는다. "내 코드의 약점을 대라"는 질문에 구체적 파일로 답하고 수정 방향(표준 진단 채널 `WintersOutputAIDebugStringA`로 교체)까지 제시할 수 있는 소재.

### 예외/반환값 이원화의 실제 경계

`Client/Private/Network/Backend/AuthClient.cpp:81-102` — HTTP 계층은 `HttpResponse.success/error` 반환값, JSON 파싱만 `try { json::parse } catch (const json::exception& e) { result.error = ... }`. 서드파티(nlohmann::json)가 예외를 던지는 라이브러리이므로 **경계에서 잡아 반환값으로 변환**한다. 키 부재는 `j.value("key", default)`로 예외 없이 방어. "구조가 깨질 수 있는 신뢰 불가 입력 파싱"과 "정상 제어 흐름인 네트워크 실패"에 서로 다른 채널을 쓰는 의도적 이원화.

### 데이터 경계의 방어적 정규화

`Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.cpp:92`의 `ResolveTicksFromSeconds` — 데이터 테이블의 초 단위 값을 틱으로 바꿀 때 `std::isfinite`로 NaN/inf를 0으로 정규화하고, `f64_t`로 승격해 곱한 뒤 `std::ceil`, 마지막에 최소 1틱 보장. "0.02초짜리 락이 0틱이 되어 즉시 풀리는" 오프바이원을 막는다. 외부 데이터는 크래시 없이, 그러나 의미가 왜곡되지 않게 경계에서 정규화한다.

### 비동기 실패 모드 — future 폐기 함정의 수정

`Client/Private/Network/Backend/CHttpClient.cpp:143-157` — `std::async(launch::async, ...)`의 future를 받지 않으면 임시 future의 소멸자가 완료까지 블로킹해 "비동기"가 동기가 된다(gotcha 2026-07-09). 수정판은 future를 멤버 `m_PendingRequests`가 소유하고, 소멸자(103-117행)가 전량 `wait()`로 드레인해 블로킹을 파괴 시점으로 한정, `PruneCompletedRequests`(129행)가 `wait_for(0초) == ready`로 완료분을 정리한다. 에러 처리 관점 핵심: **잘못된 수명 관리가 에러 메시지가 아니라 "조용한 성능 버그"로 나타난 사례** — 관측 없이는 발견도 없다.

---

## ⑤ 면접 Q&A

**Q1. 예외를 던지면 내부적으로 무슨 일이 일어나는가? 비용은?**
- 예외 객체 생성(별도 저장소) → 스택 되감기로 각 프레임 지역 객체 소멸자 역순 호출 → 타입 매칭되는 catch로 제어 이동. 못 찾으면 `std::terminate()`.
- 모던 x64는 테이블 기반 zero-cost: try 진입은 거의 무료, throw는 테이블 탐색+되감기로 매우 비쌈. 바이너리 크기와 최적화 제약은 상시 비용.
- Winters 연결: 그래서 정책 문서가 예외를 금지하고, 유일한 throw 경계(`BinaryReader.cpp:16`)도 로더가 catch해 false로 변환하는 국소 사용만 허용.

**Q2. 게임 엔진이 예외를 잘 안 쓰는 이유는?**
- (1) 게임에서 실패는 예외 상황이 아니라 정상 빈도 이벤트(에셋 미스, 경로 없음, 패킷 드랍)라 throw 비용 가정이 깨짐 (2) 프레임 예산 내 결정적 비용 요구 (3) DLL/C API/COM 경계를 예외가 못 넘음 (4) 전 코드 예외 안전 검증 비용.
- Winters 연결: `WINTERS_ERROR_HANDLING_POLICY.md` §0이 계층별 반환 컨벤션(팩토리 nullptr, RHI 무효 핸들, Server bool+cerr)을 고정.

**Q3. 생성자에서 실패하면 어떻게 처리하나? 예외를 못 쓴다면?**
- 예외를 쓰면 ctor에서 throw가 정석(이미 생성된 멤버/기반 클래스만 소멸됨). 못 쓰면 2단계 초기화 + private ctor + static Create() 팩토리로 "완전한 객체 아니면 nullptr"을 보장.
- 꼬리질문 "make_unique는 왜 못 쓰나": 라이브러리 내부의 `new T()`가 private ctor에 접근 불가라 컴파일 에러 → 멤버 함수 안에서 `unique_ptr<T>(new T())`.
- Winters 연결: `CTimer.cpp:40`, `CDX11Device.cpp:953`.

**Q4. 예외 안전성 보장 3단계를 설명하고, strong guarantee는 어떻게 만드나?**
- basic(누수 없음+불변식), strong(commit-or-rollback), nothrow. strong은 copy-and-swap: 던질 수 있는 작업을 사본에서 끝내고 nothrow swap으로 커밋.
- 꼬리: "모든 함수가 strong이어야 하나?" — 아니오, 비용(전체 복사)이 크므로 필요한 API에만. 소멸자와 swap은 nothrow가 사실상 의무.

**Q5. noexcept는 언제 붙이고, 위반하면 어떻게 되나?**
- 위반 시 컴파일 에러가 아니라 `std::terminate()`. 이동 생성자/이동 대입/swap/소멸자에 우선 적용.
- 핵심 근거: vector 재할당이 `std::move_if_noexcept`로 분기 — noexcept 없는 move는 조용히 copy로 폴백해 성능 저하.

**Q6. 신뢰할 수 없는 네트워크 바이트는 어떻게 다루나?**
- 프레이밍(길이 상한 포함) → 스키마 verify → 그 다음에야 역참조. 실패는 드랍하되 반드시 bounded 진단.
- Winters 연결: `FrameParser.cpp:13`(NeedMore/Invalid/Complete + payload 상한), `SnapshotApplier.cpp:485`(Verifier + 8회 로그), `PacketDispatcher.cpp:73`(verify 실패 시 `FlagSuspicious`). "verify 실패를 조용히 삼키면 스키마 드리프트와 네트워크 정지가 구분 안 되는 월드 프리즈"라는 실사고 기반 규약까지.

**Q7. assert와 런타임 검증은 어떻게 구분해 쓰나?**
- assert = 내부 불변식/프로그래머 오류, `NDEBUG`에서 소멸 — 릴리스에 필요한 검사에 쓰면 침묵 통과 사고. 외부 입력(파일/네트워크/데이터 테이블)은 릴리스에서도 항상 런타임 검증 + 진단.
- Winters 연결: FlatBuffers Verifier는 릴리스에서도 실행되는 런타임 검증. `ChampionGameDataDB.cpp:92`는 데이터 경계에서 `std::isfinite` 방어 + f64 승격 + ceil + 최소 1틱 클램프.

**Q8. 실패 로그가 스팸이 되지 않게 하면서 첫 실패는 남기려면?**
- 함수 지역 static 카운터로 상한(bounded logging). 포맷만 하고 출력 안 하는 dead diagnostics는 금지 — 로그를 끄려면 게이트 뒤로 옮기거나 삭제.
- Winters 연결: 실패류 8회(`ChampionSpawnService.cpp:171`), 계측류 64회(`CommandIngress.cpp:111`), 폴백 진입 16회(`ChampionGameDataDB.cpp:142`). 카운터 증가가 스레드 세이프하지 않다는 전제(메인 스레드 스폰)를 자진 언급하면 가점.

---

## ⑥ 흔한 오답/함정

1. **"예외는 항상 느리니까 안 쓴다"** — 절반만 맞다. 테이블 기반 모델에서 안 던지는 경로는 거의 무료다. 정확한 근거는 (1) throw 자체의 높은 비결정적 비용 + 게임에선 실패가 고빈도 (2) 전 경로 예외 안전 요구 (3) DLL/C/COM 경계 비호환 (4) 코드 크기. "느려서"라고만 답하면 꼬리질문에서 무너진다.
2. **"noexcept 함수에서 던지면 컴파일 에러"** — 아니다. 컴파일러는 경고 정도만 줄 수 있고, 런타임에 `std::terminate()`다. noexcept는 위반을 막는 정적 검사가 아니라 "위반하면 즉사"라는 런타임 계약.
3. **assert를 릴리스 검증으로 사용** — `NDEBUG`에서 통째로 사라져 릴리스에서만 뚫리는 구멍이 된다. Winters 정책 문서(§4)도 "DX11 저수준의 assert 전용 사전조건(릴리즈에서 통과)"을 잔여 침묵 지점으로 등재해 두었다 — 외부 입력 검증은 항상 런타임 코드로.
4. **실패 시 기본값/빈 값 반환이면 안전하다는 착각** — silent fail이 최악이다. Winters의 실사고 두 건이 근거: 빈 경로 vector 하나로 실패 원인들이 뭉개져 미니언 stuck 규명이 늦어졌고(`ePathFindResult`로 해소), verify 실패 bare return은 조용한 월드 프리즈를 만들었다(bounded trace로 해소). 폴백에는 반드시 진단을 붙인다(`ChampionGameDataDB.cpp:142`).
5. **`catch (...)`로 최상위에서 전부 삼키고 계속 진행** — 불변식이 깨진 채 실행이 계속돼 크래시 지점과 원인 지점이 분리된다(디버깅 최악). 삼키려면 경계(스레드 최상위, DLL 경계, 서드파티 파싱)에서 잡고 로그+격리(세션 단절, 태스크 폐기, 반환값 변환)까지 해야 한다. 또한 값으로 잡으면 슬라이싱 — 참조로 잡는다(`AuthClient.cpp:98`은 `const json::exception&`).

---

## 다른 챕터와의 연결

- [03_raii_smart_pointers.md](03_raii_smart_pointers.md) — 스택 되감기에서 소멸자가 정리를 보장하는 원리, scope guard, unique_ptr 에러 경로 자동 해제의 기반.
- [04_move_semantics.md](04_move_semantics.md) — noexcept move와 vector 재할당의 copy 폴백(`std::move_if_noexcept`).
- [07_multithreading.md](07_multithreading.md) — `std::async` future 폐기 함정, bounded 카운터의 비원자 증가, 스레드 경계를 넘는 예외(future에 저장되는 예외).
- [12_network_serialization.md](12_network_serialization.md) — TCP 프레이밍, FlatBuffers Verifier 신뢰 경계, 서버 권위 검증의 상세.
