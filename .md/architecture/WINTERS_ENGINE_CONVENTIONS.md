# Winters Engine 코딩 컨벤션과 GameInstance 경계 규칙

작성일: 2026-04-15
범위: `Engine/` 와 `Client/` 의 C++ 작성 규칙, DLL export 경계, 새 매니저 추가 절차.

관련 문서:
- [CLAUDE.md](C:/Users/user/Desktop/Winters/CLAUDE.md)
- [AGENTS.md](C:/Users/user/Desktop/Winters/AGENTS.md)
- [THIRDPARTY_INTEGRATION_GUIDE.md](C:/Users/user/Desktop/Winters/.md/build/THIRDPARTY_INTEGRATION_GUIDE.md)

관련 코드:
- [GameInstance.h](C:/Users/user/Desktop/Winters/Engine/Include/GameInstance.h)
- [GameInstance.cpp](C:/Users/user/Desktop/Winters/Engine/Private/GameInstance.cpp)
- [Engine_Typedef.h](C:/Users/user/Desktop/Winters/Engine/Public/Engine_Typedef.h)
- [Engine_Macro.h](C:/Users/user/Desktop/Winters/Engine/Public/Engine_Macro.h)
- [Engine_Defines.h](C:/Users/user/Desktop/Winters/Engine/Public/Engine_Defines.h)

## 0. 최상위 원칙

1. DLL 경계에는 `CGameInstance` 만 신규로 export 한다. 기존에 `WINTERS_API` 가 붙은 `IWintersApp` + `WintersRun` 은 유지.
2. 신규 싱글턴은 `CGameInstance` 하나뿐. 앞으로 `DECLARE_SINGLETON` 를 쓸 때는 `CGameInstance` 에서만 쓴다. 기존 `CEngineApp::s_pInstance` / `CInput::Get()` 같은 레거시 싱글턴은 당분간 공존 (점진 이관).
3. 게이트웨이 포워딩은 저빈도 API 전용. JobSystem, ECS, RHI, Physics 같은 핫패스는 GameInstance 바깥에서 직접 접근한다. 세부 기준은 3장을 본다.
4. 네이밍은 수업 1일차 프레임워크 기준으로 맞춘다 - `f32_t`/`i32_t`/`u32_t`, `m_` + 타입 문자, `NS_BEGIN/NS_END`, private ctor + 정적 `Create()` 팩토리 + `unique_ptr` 반환.
5. 기존 코드는 일괄 리네임 강제하지 않는다. `float32`, `WString` 등의 Winters 별칭은 `WintersTypes.h` 에 이미 `f32_t` 와 공존 alias 가 있고, 새 코드는 `f32_t` 기준으로 작성하면 된다.
6. 혼자 작업해도 다음 사람이 읽는다는 기준으로 작성한다. fallback 정책, asset routing, champion-specific 예외는 흩어진 분기 대신 이름 있는 catalog/helper에 모으고, 일반 흐름을 먼저 읽히게 둔 뒤 특수 케이스는 데이터나 작은 named branch로 표현한다.

## 1. 네이밍 컨벤션 (수업 1일차 기준 + Winters 공존)

### 1.1 클래스 / 인터페이스 / 파일명

클래스 이름:
- 클래스: `C` 접두사 - `CTimer`, `CTimer_Manager`, `CGameInstance`, `CEngineApp`
- 인터페이스: `I` 접두사 - `IWintersApp`, 향후 `IWorld`, `IJobSystem`
- `final` 키워드 - 상속이 명시적으로 필요하지 않은 클래스는 전부 `final`
- 다단어 클래스명에 `_` 허용 - `CTimer_Manager`, `CInput_Manager`, `CScene_Manager` (수업 컨벤션, Winters 에도 채택)

파일명 규칙 (확정):
- 파일명은 `C` 접두사 없이 작성한다. 클래스 이름은 `C` 접두사를 포함한다. 예시:
  - 파일: `GameInstance.h` / `GameInstance.cpp` -> 클래스: `CGameInstance`
  - 파일: `Timer_Manager.h` / `Timer_Manager.cpp` -> 클래스: `CTimer_Manager`
  - 파일: `Timer.h` / `Timer.cpp` -> 클래스: `CTimer`
  - 파일: `DynamicCamera.h` / `DynamicCamera.cpp` -> 클래스: `CDynamicCamera`
- 인터페이스 파일은 `I` 접두사 포함: `IWintersApp.h` -> `IWintersApp`
- 기존에 `C` 가 붙은 파일 (`CTimer.h`, `CEngineApp.h`, `CCamera.h`, `CDX11Device.h`, `CInput.h`, `CTransform.h`, `CWin32Window.h` 등) 은 당분간 그대로 유지. 점진적 리네임은 별도 작업으로 Phase 4 에서 진행하거나, 파일을 손볼 때 같이 이름을 바꾸는 식으로 점진 이관.
- 신규 파일은 무조건 `C` 없는 파일명으로 작성. 이 규칙부터 어기면 혼재가 더 심해진다.

### 1.2 멤버 변수 (Winters 기존 규칙 + 수업 접두사)
- `m_` 접두사 필수
- 타입 문자:
  - `m_p` - 포인터 (raw / unique_ptr / shared_ptr / ComPtr)
  - `m_f` - float (`f32_t`)
  - `m_d` - double (`f64_t`)
  - `m_i` / `m_u` - 정수 (`i32_t` / `u32_t`)
  - `m_b` - bool (`bool_t`)
  - `m_v` - Vec3 / float3 (Winters 기존)
  - `m_str` - 문자열 (`wstring_t`)
  - 컨테이너 / 구조체는 접두 생략 허용 (`m_Timers`, `m_ViewMatrix`)

### 1.3 함수
- PascalCase- `InitializeEngine()`, `GetTimeDelta()`, `AddTimer()`
- Getter 는 `GetXXX()` / `GetXXX()` 혼재 중 -> 신규 코드는 `Get_XXX()` 로 통일 (수업 컨벤션)

### 1.4 타입 alias
- `f32_t` / `f64_t` / `i32_t` / `u32_t` / `wstring_t` / `tchar_t` / `bool_t` 사용
- [Engine_Typedef.h](C:/Users/user/Desktop/Winters/Engine/Public/Engine_Typedef.h) 에 이미 전부 정의됨 (확인됨: `i8_t` ~ `i64_t`, `u8_t` ~ `u64_t` 포함)
- DirectXMath 는 `float2_t`, `float3_t`, `float4_t`, `float4x4_t`
- 기존 `float32` / `int32` / `WString` 등은 유지 ([WintersTypes.h](C:/Users/user/Desktop/Winters/Engine/Include/WintersTypes.h) 에 이미 공존 alias 존재). 리네임 금지. 단 신규 파일은 `f32_t` / `i32_t` / `wstring_t` 로 작성.

### 1.4.1 타입 alias 사용 규칙 (강제)

신규 코드는 반드시 수업 컨벤션 alias 를 사용한다. Winters 기존 alias 는 신규 코드에서 사용 금지.

| 용도 | 사용 (수업 컨벤션) | 금지 (레거시 alias) |
|---|---|---|
| 32비트 부동소수 | `f32_t` | `float32`, raw `float` |
| 64비트 부동소수 | `f64_t` | `float64`, raw `double` |
| 32비트 정수 | `i32_t` | `int32`, raw `int` |
| 64비트 정수 | `i64_t` | `int64` |
| 32비트 부호없는 정수 | `u32_t` | `uint32` |
| wide string | `wstring_t` | `WString`, `std::wstring` |
| wide char | `tchar_t` | `WChar`, `wchar_t` |
| wide C-string | `const tchar_t*` | `WStr`, `const wchar_t*` |
| bool | `bool_t` | raw `bool` (기존 코드는 `bool` 허용) |
| Vec3 저장형 | `float3_t` | `XMFLOAT3` (직접 사용), raw 구조체 |

이유: 수업 1일차 프레임워크 기준을 전체 엔진에 점진 적용 중. 혼재 시 같은 의미의 타입이 파일마다 다른 이름으로 나타나 검색·리네임·리팩토링 비용이 커진다. "하나의 타입 = 하나의 alias" 원칙을 확정.

적용 범위:
- 멤버 변수 선언
- 함수 파라미터 타입
- 함수 반환 타입
- 지역 변수 타입
- `static_cast<f32_t>(x)` 같은 명시적 캐스트

예외 (기존 코드 유지):
- [WintersTypes.h](C:/Users/user/Desktop/Winters/Engine/Include/WintersTypes.h) / [Engine_Typedef.h](C:/Users/user/Desktop/Winters/Engine/Public/Engine_Typedef.h) 자체의 alias 정의 (양쪽 제공 필요)
- `Engine/Include/` 의 공개 API 헤더 ([WintersMath.h](C:/Users/user/Desktop/Winters/Engine/Include/WintersMath.h), [EngineConfig.h](C:/Users/user/Desktop/Winters/Engine/Include/EngineConfig.h), [IWintersApp.h](C:/Users/user/Desktop/Winters/Engine/Include/IWintersApp.h), [WintersPaths.h](C:/Users/user/Desktop/Winters/Engine/Include/WintersPaths.h)): 별도 Phase 4 에서 일괄 정리 예정. 그 전까지는 현행 유지. 이 파일들에 새 코드를 추가할 때는 컨벤션을 지키되, 기존 라인은 건드리지 말 것.
- Win32 타입 (`HWND`, `LPARAM`, `WPARAM`, `DWORD`, `BYTE`, `UINT`, `LONG`, `HRESULT` 등): 그대로 사용. Windows API 타입이므로 alias 로 바꾸지 않는다.
- DirectXMath 타입 (`XMVECTOR`, `XMMATRIX`, `XMFLOAT3`, `XMFLOAT4X4` 등): 그대로 사용. 단, 엔진 레벨 멤버 변수는 `float3_t` (XMFLOAT3 의 alias) 를 선호.
- 서드파티 라이브러리 타입 (Assimp `aiMatrix4x4`, FMOD 핸들 등): 그대로 사용.

namespace Engine 밖 공개 헤더의 alias 제약 (B-6.6 추가, 중요):
`bool_t` / `i32_t` / `u32_t` / `u8_t` 등 [Engine_Typedef.h](C:/Users/user/Desktop/Winters/Engine/Public/Engine_Typedef.h) 내부 alias 는 `namespace Engine` 안에서만 해석된다.
`namespace Engine` 블록 밖에 있는 공개 헤더 (예: `ModelRenderer.h`, `WintersMath.h` 등 `Engine/Include/` 루트에 있거나 `Engine/Public/` 에 있어도 `NS_BEGIN(Engine)` 으로 감싸지 않은 파일) 에서는 이 alias 사용 금지.
- [Engine_Typedef.h](C:/Users/user/Desktop/Winters/Engine/Public/Engine_Typedef.h) 에 있지만 global alias 인 `f32_t`/`f64_t` ([WintersTypes.h](C:/Users/user/Desktop/Winters/Engine/Include/WintersTypes.h)) 는 namespace 밖에서도 사용 가능
- 해당 헤더가 `namespace Engine` 안에 있으면 `bool_t` 등 전부 사용 가능
- 클래스 선언이 `NS_BEGIN(Engine)` 없이 있는데 `bool_t` 를 쓰면 "식별자 'bool_t' 가 정의되어 있지 않습니다" 에러
- 대안: `bool` / `int32_t` / `uint32_t` 같은 C++ 표준 / `<cstdint>` 타입 사용

공개 헤더의 `std::` 명시 규칙 (B-6.6 추가, 중요):
`Engine/Public/` 과 `Engine/Include/` 의 `.h` 는 `using namespace std;` 선행을 가정해선 안 된다. 공개 헤더에서는 `using namespace std;` 와 `using std::...` 개별 using 선언을 모두 금지한다.
- `std::vector<T>`, `std::function<...>`, `std::unique_ptr<T>`, `std::move(...)` 처럼 `std::` 를 명시
- `.cpp` 내부 또는 함수/블록 내부의 지역 using 만 허용
- unqualified `vector<T>` / `function<...>` / `unique_ptr<T>` / `move(...)`

이유: Client TU 가 공개 헤더를 include 할 때 [Engine_Defines.h](C:/Users/user/Desktop/Winters/Engine/Public/Engine_Defines.h) (=`using namespace std;` 체인) 가 항상 먼저 include 되리라는 보장이 없다. 예: `Client/Private/GameObject/SkillTable.cpp` 가 `SkillDef.h` -> `Entity.h` 를 include 하는 경로에서 Entity.h 가 unqualified `vector` 를 써서 "식별자 'vector' 에 인수 목록이 없습니다" 로 실패했음. 수정 후 전체 ECS 공개 헤더에 `std::` 명시 일괄 적용.

위반 예시 -> 수정:
```cpp
// 금지
float32 GetDelta() const;             // alias 혼재
uint32  m_iCount = 0;                  // alias 혼재
WString m_strName;                     // alias 혼재
void    OnUpdate(float deltaTime);     // raw float

// 수정
f32_t     GetDelta() const;
u32_t     m_iCount = 0;
wstring_t m_strName;
void      OnUpdate(f32_t deltaTime);
```

리뷰 체크리스트 (신규 파일 작성/PR 제출 시):
1. `grep -n "float32\|int32\|uint32\|WString\|WStr\b" <new_file>` -> 출력 없어야 함
2. `grep -n " float \| int \| double \| bool " <new_file>` -> raw 타입이 나오면 alias 로 바꿨는지 검토 (Win32/DirectX 타입은 예외)
3. `using namespace std;` 가 있는 TU 에서는 `std::wstring` 직접 쓰지 말고 `wstring_t` 사용

### 1.4.2 DirectXMath XMLoad/XMStore 규칙

원칙: `XMVECTOR` / `XMMATRIX` 는 연산용 임시 타입, `Vec3` / `Vec4` / `Mat4` / `float3_t` / `float4x4_t` / `XMFLOAT*` 는 저장용 타입으로 구분한다.

| 단계 | 사용 타입 | 규칙 |
|---|---|---|
| 저장 | `Vec3`, `Vec4`, `Mat4`, `float3_t`, `float4x4_t`, `XMFLOAT*` | ECS Component, 클래스 멤버, 배열, 파일/네트워크 직렬화에 사용 |
| 로드 | `XMLoadFloat3/4/4x4` | 저장용 데이터를 연산용 SIMD 값으로 변환 |
| 연산 | `XMVECTOR`, `XMMATRIX` | 함수 내부 지역 변수/파라미터로만 사용 |
| 저장 | `XMStoreFloat3/4/4x4` | 연산 결과를 다시 저장용 타입으로 기록 |

금지:
- `XMVECTOR` / `XMMATRIX` 를 ECS Component, 클래스 멤버, `std::vector` 원소, POD 저장 포맷, 네트워크 패킷에 직접 넣지 않는다.
- `reinterpret_cast<XMFLOAT3*>` 는 아무 구조체에나 쓰지 않는다. `Vec3` 처럼 `float x, y, z` 3개가 연속 배치되고 `XMFLOAT3` 와 레이아웃이 동등한 타입에서만 허용한다.

스택 vs 힙 설명 정정:
- 정확한 표현은 "스택은 안전하고 힙은 위험하다" 가 아니다.
- `XMVECTOR` / `XMMATRIX` 는 SIMD 연산용 타입이라 메모리에 저장될 때 16바이트 정렬 조건을 신경써야 한다.
- 함수 내부 지역 변수는 컴파일러가 정렬을 맞춰주는 경로라 보통 안전하다.
- 힙/컨테이너/구조체 멤버도 올바른 aligned allocation 을 쓰면 가능하지만, 엔진 전역 데이터 구조에서 실수하기 쉬우므로 Winters 에서는 저장용 타입으로만 보관한다.
- 그래서 컨벤션은 "저장형 -> XMLoad -> 연산형 지역 변수 -> XMStore -> 저장형" 이다.

production 기준 패턴 (`TransformSystem.cpp` 와 동일):
```cpp
const DirectX::XMVECTOR vScale =
    DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&tf.m_LocalScale));
const DirectX::XMVECTOR vRot =
    DirectX::XMQuaternionRotationRollPitchYaw(
        tf.m_LocalRotation.x,
        tf.m_LocalRotation.y,
        tf.m_LocalRotation.z);
const DirectX::XMVECTOR vPos =
    DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&tf.m_LocalPosition));

const DirectX::XMMATRIX mLocal =
    DirectX::XMMatrixAffineTransformation(
        vScale,
        DirectX::XMVectorZero(),
        vRot,
        vPos);

DirectX::XMStoreFloat4x4(
    reinterpret_cast<DirectX::XMFLOAT4X4*>(&tf.m_LocalMatrix),
    mLocal);
```

권장 확인:
```cpp
static_assert(sizeof(Vec3) == sizeof(DirectX::XMFLOAT3));
static_assert(alignof(Vec3) == alignof(float));
```

### 1.5 네임스페이스
- `NS_BEGIN(Engine)` / `NS_END` 매크로 - [Engine_Macro.h](C:/Users/user/Desktop/Winters/Engine/Public/Engine_Macro.h) 에 이미 정의 (L14-15)
- Engine 신규 코드는 이 매크로로 감싼다
- [Engine_Defines.h](C:/Users/user/Desktop/Winters/Engine/Public/Engine_Defines.h) 가 `using namespace Engine;` 선언 -> Engine 내부 파일은 `Engine::` 접두사 생략 가능

### 1.6 매크로
- 전체 대문자 + `_` - `ENGINE_DLL`, `WINTERS_API`, `NS_BEGIN`, `NS_END`, `DECLARE_SINGLETON`, `NO_COPY`, `NULL_CHECK`, `FAILED_CHECK`
- DLL export 매크로 2개 공존:
  - `WINTERS_API` ([WintersAPI.h](C:/Users/user/Desktop/Winters/Engine/Include/WintersAPI.h)) - 기존 공개 API 용
  - `ENGINE_DLL` ([Engine_Macro.h](C:/Users/user/Desktop/Winters/Engine/Public/Engine_Macro.h)) - 수업 컨벤션 통합 후 신규 용
  - 둘 다 같은 조건 (`WINTERS_ENGINE_EXPORTS`) 으로 동작. 신규 작성 시 `ENGINE_DLL` 권장, 리네임은 강제하지 않는다.

### 1.7 `DECLARE_SINGLETON` 매크로 특이사항 (Winters 버전)
현재 정의 ([Engine_Macro.h](C:/Users/user/Desktop/Winters/Engine/Public/Engine_Macro.h) L57-65):
```cpp
#define DECLARE_SINGLETON(CLASSNAME)	\
    NO_COPY(CLASSNAME)	\
    public:	\
        static CLASSNAME* Get()	\
        {	\
            static CLASSNAME instance;	\
            return &instance;	\
        }	\
    private:
```

차이점 vs 수업 1일차:
- 포인터 반환 (`Get() -> CLASSNAME*`). 수업은 레퍼런스 반환 (`Get() -> CLASSNAME&`)
- 호출 측: `CGameInstance::Get()->Method()` (포인터), 수업은 `CGameInstance::Get().Method()` (레퍼런스)

결정: Winters 정의를 그대로 유지한다. 이유:
- 전방 선언 친화적 (`class CTimer_Manager*` 멤버로 레퍼런스보다 포인터가 자연)
- 기존 `CInput::Get()` 와 호출 스타일 통일
- 수업 코드와 다르지만, 포인터/레퍼런스는 미미한 스타일 차이

### 1.8 헤더 가드
- `#pragma once` 사용 (Winters 기존)
- 수업에서 쓰는 `#ifndef XXX_h__` 도 `Engine_*.h` 파일에 이미 존재 - 혼재 허용

## 2. 클래스 설계 원칙 (Winters 기존 + 수업 팩토리)

### 2.1 생성자/소멸자 규칙
- 생성자: private - 외부에서 직접 `new` 금지
- 소멸자: public - `final` 클래스는 non-virtual, 상속 계층은 virtual
- 복사 금지 - `NO_COPY(ClassName)` 매크로 또는 `= delete`

### 2.2 Create 팩토리 패턴 (수업 컨벤션)
```cpp
class CExample final
{
private:
    CExample() = default;                                   // private ctor

public:
    ~CExample() = default;

public:
    HRESULT Initialize(/* params */);
    f32_t   Get_Value() const { return m_fValue; }

public:
    static std::unique_ptr<CExample> Create(/* params */);  // 정적 팩토리

private:
    f32_t m_fValue = 0.f;
};

// make_unique 는 private ctor 때문에 사용 불가 -> new 직접 사용
std::unique_ptr<CExample> CExample::Create()
{
    auto pInstance = std::unique_ptr<CExample>(new CExample());
    if (FAILED(pInstance->Initialize()))
        return nullptr;
    return pInstance;
}
```

### 2.3 멤버 변수 접근
- 멤버 변수는 전부 private
- 외부 접근은 public 멤버 함수를 통해서만
- Dirty Flag 패턴, RAII

### 2.4 COM / DirectX
- `ComPtr` 필수 (raw `ID3D11*` 금지)
- `HRESULT` 반드시 체크 + `errorBlob` 출력
- cbuffer 16바이트 정렬
- HLSL 행렬은 `row_major` 키워드 필수

## 3. GameInstance 경계 규칙 (핵심)

### 3.1 Export 규칙
- `CGameInstance` 는 `ENGINE_DLL` (또는 `WINTERS_API`) 로 export
- 그 외 신규 내부 클래스 (`CTimer_Manager` 등) 는 export 마크 없음
- 기존 export 클래스들 (`IWintersApp`, `WintersRun` 등) 은 그대로 유지 - 리네임/제거 금지
- `#pragma warning(disable : 4251)` 는 [Engine_Defines.h](C:/Users/user/Desktop/Winters/Engine/Public/Engine_Defines.h) L28 에 그대로 둔다 - 기존 export 클래스가 여전히 STL 멤버 갖고 있어서 당장 제거하면 경고 폭발. GameInstance 가이드라인으로 신규 export 클래스만 STL 멤버 금지.

### 3.2 소유권 규칙
- 모든 엔진 신규 매니저의 소유권은 `CGameInstance` 가 `std::unique_ptr` 로 보유
- 기존 `CEngineApp::s_pInstance` / `CInput::Get()` 은 당분간 공존. 점진 이관은 이후 과제
- `Initialize_Engine()` 에서 순서대로 `Create()`, `Shutdown_Engine()` 에서 역순 해제

### 3.3 포함 기준 (Tier 1 vs Tier 2)

새 서브시스템/매니저를 만들 때 "public API 가 프레임당 몇 번 호출되는가?" 로 판단:

#### Tier 1 - GameInstance 포워딩 (저빈도, 이름 기반, 수명주기성)
기준: 프레임당 수십 회 미만 / 태그나 경로 같은 명명된 리소스 관리 / 세션 수준 이벤트

- `CTimer_Manager` - 태그 기반 멀티 타이머 (Phase 1 최초 도입 대상)
- `CInput_Manager` (향후) - 프레임당 1회 폴링
- `CScene_Manager` (향후) - 씬 전환은 드문 이벤트
- `CResource_Manager` (향후) - 에셋 로드 요청 수준 (고수준 API)
- `CSound_Manager` (향후) - FMOD 고수준 API (`Play_BGM`, `Play_SFX`)
- 네트워크 세션 수준 API (로그인, 매치 참가 - 이미 별도 `AuthClient` 등 존재)
- 로깅, 프로파일러 마커

형태: `CGameInstance` 에 포워딩 메서드 추가
```cpp
// CGameInstance.h
public:
    HRESULT Add_Timer(const wstring_t& strTimerTag);
    f32_t   Get_TimeDelta(const wstring_t& strTimerTag);
    void    Update_TimeDelta(const wstring_t& strTimerTag);
```

#### Tier 2 - GameInstance 바깥 직접 접근 (고빈도, 타입 기반, 핫패스)
기준: 프레임당 수백~수만 회 호출 / 타입 기반 tight loop / 레이턴시 민감

- JobSystem (`CJobSystem`, Fiber Pool) - submit 이 수십 나노초 오버헤드 목표
- ECS World (`World`, `ComponentStore`, `SystemScheduler`) - SoA tight loop
- RHI draw call 제출 (`CDX11Device`, `DX11Pipeline`) - 프레임당 수백~수천 회
- Physics step / 충돌 쿼리 (Jolt 향후)
- Transform 월드 행렬 벌크 갱신 (`CTransform`, ECS Systems)

형태: `CGameInstance` 는 Getter 한 번만, 실제 호출은 레퍼런스 캐시 후 직접
```cpp
// CGameInstance.h - Getter 만 (향후 Phase 5 ECS 본격화 시)
class IJobSystem;   // 전방 선언
class IWorld;       // 전방 선언

public:
    IJobSystem* Get_JobSystem();
    IWorld*     Get_World();
```

핫루프에서:
```cpp
IJobSystem* pJobs = CGameInstance::Get()->Get_JobSystem();  // 한 번만
for (auto& entity : entities)
    pJobs->Submit(...);  // 포워딩 레이어 없이 직접 호출
```

`IJobSystem`, `IWorld` 는 순수 가상 인터페이스 - DLL 경계 통과 안전. 구현체 `CJobSystem`, `CWorld` 는 export 마크 없이 내부에 남음.

#### 애매한 경우의 분할
- Sound: 고수준 `Play_BGM(L"Main")` -> Tier 1. 3D 사운드 엔티티별 위치 갱신 -> Tier 2 (Audio System 내부)
- Network: 세션 이벤트 -> Tier 1. 패킷 송수신 핫루프 -> Tier 2 (Network System 내부)
- Resource: `Load_Mesh(L"Champion_Irelia")` -> Tier 1. 로드된 Mesh 의 GPU 버퍼 바인딩 -> Tier 2 (Renderer 내부)

#### Sound 실제 적용 예시 (B-6.6 완료)

`CSound_Manager` 는 Tier 1 정책으로 구현. Client 는 오직 `CGameInstance` 포워딩으로만 접근.

```cpp
// Client - 올바른 사용
#include "GameInstance.h"      // CGameInstance + eSoundChannel 자동 노출

using namespace Engine;

CGameInstance::Get()->PlayBGM(L"BGM/Title.wav", 0.5f);
CGameInstance::Get()->PlaySoundOn(L"Irelia/attack1.wav",
                                   eSoundChannel::PlayerAction, 1.0f);
CGameInstance::Get()->PlayEffect(L"SwordHit.wav", 0.8f);
CGameInstance::Get()->SetMasterVolume(0.7f);
```

```cpp
// 금지 - 매니저 직접 접근 (EngineSDK 에 Sound_Manager.h 가 배포되어 있어도)
#include "Sound_Manager.h"
CSound_Manager* pSound = /* ... */;
pSound->PlayBGM(...);   // 경계 위반
```

포워딩 메서드: `PlaySoundOn` / `PlayEffect` / `PlayBGM` / `StopChannel` / `StopAllSounds` / `SetChannelVolume` / `SetMasterVolume` / `Tick_Engine`.
채널: `eSoundChannel` enum - BGM / PlayerAction / PlayerVoice / UI / Ambient / Effect0~3 (9 슬롯).
매 프레임 tick: `CEngineApp::Run()` 이 `CGameInstance::Get()->Tick_Engine()` 호출 -> 내부에서 `FMOD::System::update()`.
리소스: `Client/Bin/Resource/Sound/<category>/` 에 .wav 배치 -> Client PostBuild 가 `$(OutDir)Resource\` 로 xcopy -> exe 옆에서 재귀 로드.

### 3.4 금지사항
- `CGameInstance::Get()->Submit_Job(...)` 같은 JobSystem 포워딩 금지
- `CGameInstance::Get()->Get_World()->Query<...>()` 같은 ECS 포워딩 금지 - Getter 리턴값을 캐시해서 쓸 것
- `CGameInstance` 에 editor-only 메서드 추가 금지
- `CTimer_Manager`, `CInput_Manager` 같은 내부 매니저를 `WINTERS_API` / `ENGINE_DLL` 마크 금지
- DLL 경계에서 STL 타입 값 반환 금지 (`std::vector<T>` 같은 것) - out 파라미터 또는 opaque handle

## 4. DLL 아키텍처 (현재 상태)

### 4.1 디렉토리 규약
```
Engine/
├── Include/               <- 공개 SDK (Client 가 참조)
│   ├── WintersAPI.h       <- WINTERS_API / dllexport 매크로
│   ├── WintersTypes.h     <- float32, f32_t, WString 등 공존 alias
│   ├── WintersMath.h
│   ├── WintersEngine.h    <- Client 가 유일하게 include 할 umbrella 헤더
│   ├── WintersPaths.h
│   ├── EngineConfig.h     <- POD 설정 구조체
│   ├── IWintersApp.h      <- Client 가 상속할 순수 가상 인터페이스
│   ├── CGameInstance.h    <- [신규] ENGINE_DLL 게이트웨이
│   ├── Engine.vcxproj
│   └── Engine.vcxproj.filters
│
├── Public/                <- 내부 헤더 (DLL 내부에서만 include)
│   ├── Engine_Defines.h   <- 공통 includes + using namespace
│   ├── Engine_Macro.h     <- NS_BEGIN/END, ENGINE_DLL, DECLARE_SINGLETON ...
│   ├── Engine_Typedef.h   <- f32_t, i32_t, wstring_t ...
│   ├── Engine_Enum.h
│   ├── Engine_Struct.h
│   ├── Engine_Function.h
│   ├── WintersPCH.h       <- 선컴파일 헤더
│   ├── WintersCore.h
│   ├── Core/
│   │   ├── CTimer.h
│   │   ├── CTimer_Manager.h   <- [신규]
│   │   ├── CInput.h
│   │   ├── CTransform.h
│   │   ├── JobSystem.h
│   │   └── JobCounter.h
│   ├── Framework/
│   │   └── CEngineApp.h
│   ├── Platform/, RHI/, ECS/, Renderer/, Resource/
│
└── Private/               <- 구현 (.cpp)
    ├── WintersEngine.cpp  <- DLL 엔트리 + WintersRun
    ├── CGameInstance.cpp  <- [신규]
    ├── Core/
    │   ├── CTimer.cpp
    │   └── CTimer_Manager.cpp  <- [신규]
    ├── Framework/, Platform/, RHI/, ECS/, Renderer/, Resource/
```

### 4.2 EngineSDK 동기화와 Gotcha 연결
- [EngineSDK/inc](C:/Users/user/Desktop/Winters/EngineSDK/inc) 는 플랫 복사 구조 (폴더 계층 무시)
- [Engine.vcxproj](C:/Users/user/Desktop/Winters/Engine/Include/Engine.vcxproj) 의 PostBuildEvent 가 `Include/*.h` + `Public/Engine_*.h` 복사
- [UpdateLib.bat](C:/Users/user/Desktop/Winters/UpdateLib.bat) 가 수동 전체 복사 (재귀, 플랫)
- 주의: 두 경로가 서로 다른 파일을 복사한다. [UpdateLib.bat](C:/Users/user/Desktop/Winters/UpdateLib.bat) 가 더 포괄적이지만 `.lib` 미복사 + Release 경로 미처리 (`UPDATELIB_REVIEW.md` 또는 별첨 참조)

### 4.3 vcxproj filter 배치 규약
- `CGameInstance.h` -> `Include` 필터 (공개 API)
- `CGameInstance.cpp` -> `02. Structure\02. GameInstance` 신규 서브필터
- `CTimer_Manager.h/.cpp` -> 기존 `01. Core\00. Timer` 필터 (Timer 군집화)
- `CTimer.h/.cpp` -> 기존 위치 유지

## 5. Gotchas (신규 / 기존 공존)

### 5.1 기존 (유지)
- vcxproj `/utf-8` 필수 (CP949 해석 시 C4819 + C1075)
- Docker PostgreSQL 5432 충돌 -> 5433 사용
- Docker Kafka `KAFKA_` 접두사 (`KAFKA_CFG_` 아님)
- Go `package main` 위치 `cmd/{service}/`
- Windows IPv6 localhost -> `DB_HOST=127.0.0.1`
- HLSL `row_major` 필수
- Assimp `aiMatrix4x4 -> XMFLOAT4X4` 전치 필수
- 스켈레톤 모델 stride 혼재 금지
- Engine 헤더 수정 후 EngineSDK 동기화 필수 <- 이 gotcha 가 UpdateLib.bat 와 직결

### 5.2 GameInstance / 수업 컨벤션 도입 관련 신규
- `make_unique` 사용 불가: private ctor 때문에 컴파일 에러. `std::unique_ptr<T>(new T())` 직접 사용
- `DECLARE_SINGLETON` 호출 스타일: Winters 버전은 포인터 반환 - `CGameInstance::Get()->Method()` 로 호출 (`->`, 수업의 `.` 아님)
- DLL 경계 STL 금지: `ENGINE_DLL` 클래스의 public 메서드가 `std::vector<T>` 를 값 반환하면 C4251 부활 - out 파라미터 또는 iterator/포인터 제공
- `using namespace Engine;` 가 [Engine_Defines.h](C:/Users/user/Desktop/Winters/Engine/Public/Engine_Defines.h) L43 에 있음 -> Engine 내부 파일이 [Engine_Defines.h](C:/Users/user/Desktop/Winters/Engine/Public/Engine_Defines.h) 를 include 하면 네임스페이스 접두사 생략 가능 (단, 이로 인한 충돌 주의)
- CEngineApp 싱글턴 제거는 하지 않는다 (당분간): `CEngineApp::s_pInstance` 는 유지. CGameInstance 는 별도 싱글턴으로 공존. 점진 이관.
- Timer 이중화 주의: CEngineApp 이 `m_Timer` 를 직접 소유하고 있고, 도입 시 `CGameInstance` -> `CTimer_Manager` -> `Timer_Default` 가 동시에 존재하게 된다. 의도한 점진 이관 단계라는 걸 인식하고 있을 것. 핵심 게임 루프는 당분간 CEngineApp.m_Timer 를 쓰고, Client 는 CGameInstance 경유로 이행.

## 6. 체크리스트: 새 매니저를 추가할 때

1. [ ] public API 가 프레임당 몇 번 호출되는가? 수십 미만 -> Tier 1 / 수백 이상 -> Tier 2
2. [ ] 고수준/저수준 분할 가능한가? (Sound 예시)
3. [ ] Tier 1 이면:
    - [ ] `CGameInstance` 멤버로 `std::unique_ptr<CXxx_Manager>` 추가 (전방 선언으로)
    - [ ] `Initialize_Engine()` 에 `CXxx_Manager::Create()` 추가
    - [ ] `CGameInstance.h/.cpp` 에 포워딩 메서드 추가
    - [ ] 매니저 헤더는 `Engine/Public/` 에 (SDK 에 복사되지 않도록)
    - [ ] 매니저 클래스에 `ENGINE_DLL` 마크 없음
4. [ ] Tier 2 이면:
    - [ ] `IXxx` 순수 가상 인터페이스를 `Engine/Include/` 에 정의 (멤버 없음)
    - [ ] 구현체 `CXxx` 는 `Engine/Public/`, export 마크 없음
    - [ ] `CGameInstance::Get_Xxx() -> IXxx*` Getter 하나 추가
    - [ ] Client 는 init 시 포인터 캐시, 핫루프에서 직접 호출
5. [ ] 네이밍: 클래스 `C` 접두사, 파일명은 C 없이 (`Transform.h` / `class CTransform`), POD struct는 C 금지, 인터페이스는 `I` 접두사, `m_` 멤버, `_t` 타입, private ctor, 정적 `Create()`, `unique_ptr` 반환
6. [ ] `NS_BEGIN(Engine) ... NS_END` 로 감싸기
7. [ ] `Shutdown_Engine()` 에서 역순 `reset()` 확인
8. [ ] Engine.vcxproj + Engine.vcxproj.filters 에 파일 등록 (파일 삭제 시에도 양쪽 동기화 필수)
9. [ ] PostBuildEvent (Engine.vcxproj) 또는 UpdateLib.bat 에 복사 대상 추가 (Engine/Include 에 있으면 자동)
10. [ ] 신규 기능 계획 전 4폴더 전수 grep: `Engine/Public/{Resource,Core,Framework,Renderer}` - 동명 클래스/유틸 중복 방지 (Phase 1a Texture 사고 재발 방지)
11. [ ] 튜닝 가능한 모든 파라미터에 ImGui 슬라이더 노출 (gotchas #14 정책 반전 2026-04-16) - 하드코딩 금지, `WINTERS_EDITOR` 매크로로 Release에서 제거

## 7. 참고 - 수업 1일차 원본

- 경로: `C:\Users\user\Desktop\수업\1일차`
- 구조: `Engine/Public/GameInstance.h` + `Timer.h` + `Timer_Manager.h`, `Client/Public/MainApp.h`
- 패턴: `DECLARE_SINGLETON(CGameInstance)` + `unique_ptr<CTimer_Manager>` + `map<wstring_t, unique_ptr<CTimer>>`
- Winters 적용 시 차이점:
  - `map` -> `std::unordered_map` (O(1) lookup, 성능 기조 부합)
  - 수업의 "모든 매니저 포워딩" -> Winters Tier 1 만 포워딩, Tier 2 는 인터페이스 Getter
  - 수업은 싱글 스레드 -> Winters 는 Fiber/Job 멀티 스레드 (Tier 1 매니저는 락 필요)
  - `DECLARE_SINGLETON` 반환: 수업 = 레퍼런스, Winters = 포인터 (기존 정의 유지)

## 8. 서드파티 의존성 관리 (B-6.6 추가)

### 8.1 기본 원칙

- vcpkg 미사용. 모든 서드파티 는 `Engine/ThirdPartyLib/<LibName>/` 에 Inc/Lib/Bin 구조로 편입
- `Engine.vcxproj` 에 `<VcpkgEnabled>false</VcpkgEnabled>` + `<VcpkgEnableManifest>false</VcpkgEnableManifest>` 강제
- 런타임 DLL 은 `UpdateLib.bat` 이 `Client/Bin/{Debug,Release}/` 로 xcopy 배포
- 레포는 self-contained - `git clone` 후 바로 빌드 가능해야 함

### 8.2 현재 편입 상태

| 라이브러리 | Inc 구조 | Lib | Bin |
|---|---|---|---|
| Assimp | `Inc/assimp/*.h` (서브디렉토리 유지) | Debug/Release 별 `assimp-vc143-mt{d}.lib` | Debug/Release 별 + transitive 5종 (poly2tri, minizip, zlib{d}1, kubazip, pugixml) |
| DirectXTK | `Inc/directxtk/*.h` | Debug/Release 별 `DirectXTK.lib` (이름 동일) | Debug/Release 별 `DirectXTK.dll` |
| FMOD | `Inc/*.h` (flat) | `Lib/fmod_vc.lib` (Debug/Release 공용) | `Bin/fmod.dll` (Debug/Release 공용) |

### 8.3 새 라이브러리 추가 절차

[THIRDPARTY_INTEGRATION_GUIDE.md](C:/Users/user/Desktop/Winters/.md/build/THIRDPARTY_INTEGRATION_GUIDE.md) 참조 - 폴더 구조 템플릿, vcxproj 수정 패턴, UpdateLib.bat 편집 방법, 검증 체크리스트 포함.

### 8.4 관련 Gotcha

- Client PreBuildEvent 경로는 `$(SolutionDir)UpdateLib.bat` - `../../../UpdateLib.bat` 같은 상대경로 실수 금지 (데스크톱을 가리켜 조용히 실패)
- transitive DLL 누락 주의 - 예: assimp 는 zlib/minizip/poly2tri/kubazip/pugixml 5 개를 동적으로 요구. `dumpbin -DEPENDENTS assimp-vc143-mtd.dll` 로 확인 후 ThirdPartyLib/Assimp/Bin 에 전부 편입
- Debug/Release transitive 이름 차이 - `zlibd1.dll` (Debug) vs `zlib1.dll` (Release). 각각 복사

## 변경 이력

- 2026-04-15: 초기 작성. 수업 1일차 프레임워크 컨벤션 채택 + GameInstance Tier 1/2 경계 규칙 확정. `CEngineApp` 싱글턴은 점진 이관 (당분간 공존).
- 2026-04-19: B-6.6 완료 반영. 1. 1.4.1 에 namespace Engine 밖 공개 헤더의 `bool_t` 사용 금지 + 공개 헤더 `std::` 명시 규칙 추가. 2. 3장에 Sound Tier1 포워딩 적용 예시 추가. 3. 8장에 서드파티 의존성 관리 신규 (vcpkg 탈피 -> ThirdPartyLib 자립, Assimp/DirectXTK/FMOD 편입 완료). [THIRDPARTY_INTEGRATION_GUIDE.md](C:/Users/user/Desktop/Winters/.md/build/THIRDPARTY_INTEGRATION_GUIDE.md) 신규 참조.
