# Phase 8: C++ Client Network SDK — 코드 검토 결과

> **검토일**: 2026-04-13
> **검토 범위**: Client/Public/Network/*.h + Client/Private/Network/*.cpp + vcxproj

---

## 검토 요약

| 분류 | 건수 |
|------|------|
| 🔴 컴파일 에러 | 4건 |
| 🟡 API 필드명 불일치 | 2건 |
| 🔵 컨벤션 불일치 (_t 타입) | 4건 |
| ⚪ 파일명 불일치 (vcxproj) | 1건 |

---

## 🔴 컴파일 에러 (4건)

### 1. ProfileClient.h — 생성자 중복 정의

**파일**: `Client/Public/Network/ProfileClient.h` L39, L41

```cpp
// 현재 (컴파일 에러)
private:
    CProfileClient() = default;    // L39 — private 생성자
public:
    CProfileClient() = default;    // L41 — public 생성자 (중복!)
```

**수정**:
```cpp
public:
    ~CProfileClient() = default;   // public 소멸자

    // ... public 메서드들 ...

private:
    CProfileClient() = default;    // private 생성자 (1개만)
```

---

### 2. AuthClient.cpp — 문자열 이스케이프 오타

**파일**: `Client/Private/Network/AuthClient.cpp` L29

```cpp
// 현재 (컴파일 에러 — 이스케이프 깨짐)
string body = "{\email\":\"" + email + "\",\"password\":\"" + password + "\"}";
//              ^^ 여기! \e 는 유효하지 않은 이스케이프

// 수정
string body = "{\"email\":\"" + email + "\",\"password\":\"" + password + "\"}";
//              ^^ \" 로 수정
```

---

### 3. AuthClient.cpp — `using namespace Client;` 누락

**파일**: `Client/Private/Network/AuthClient.cpp`

```cpp
// 현재 (L1~L4)
#include "Network/AuthClient.h"
#include "Network/json.hpp"

using json = nlohmann::json;
// ← using namespace Client; 없음!

// 수정: L4 앞에 추가
using namespace Client;
using json = nlohmann::json;
```

다른 .cpp 파일들(MatchClient, ProfileClient, PaymentClient, CShopClient)은 전부 `using namespace Client;` 있음. AuthClient.cpp만 빠짐.

---

### 4. CHttpClient.cpp — 파일 끝 닫는 중괄호 `}` 잉여

**파일**: `Client/Private/Network/CHttpClient.cpp` L210

```cpp
// 현재 (L208~210)
return response;
}
}    // ← L210: 잉여 중괄호. NS_BEGIN 안 쓰고 using namespace Client; 사용하므로 불필요

// 수정: L210 삭제
return response;
}
```

---

## 🟡 API 필드명 불일치 (2건) — 백엔드 응답과 안 맞음

### 5. ProfileClient.cpp — `user_name` → `username`

**파일**: `Client/Private/Network/ProfileClient.cpp` L19

```cpp
// 현재 (백엔드는 "username" 반환)
profile.username = data.value("user_name", "");

// 수정
profile.username = data.value("username", "");
```

백엔드 실제 응답:
```json
{"username":"PlayerC","mmr":1050,...}
```

---

### 6. ProfileClient.cpp — `winRate` → `win_rate`

**파일**: `Client/Private/Network/ProfileClient.cpp` L22

```cpp
// 현재 (백엔드는 snake_case "win_rate" 반환)
profile.winRate = data.value("winRate", 0);

// 수정
profile.winRate = data.value("win_rate", 0);
```

백엔드 실제 응답:
```json
{"win_rate":100,...}
```

---

## 🔵 컨벤션 불일치 (4건) — Engine_Typedef 미적용

### 7. CHttpClient.h — `int32_t`, `bool` 사용

**파일**: `Client/Public/Network/CHttpClient.h` L10, L12

```cpp
// 현재
struct HttpResponse
{
    int32_t statusCode = 0;     // L10
    string body;
    bool success = false;       // L12
    string error;
};

// 수정 (Engine_Typedef 적용)
struct HttpResponse
{
    i32_t   statusCode = 0;
    string  body;
    bool_t  success    = false;
    string  error;
};
```

---

### 8. CHttpClient.h L16 — `std::function` 사용

```cpp
// 현재
using HttpCallback = std::function<void(const HttpResponse&)>;

// 수정 (using namespace std; 이미 적용됨)
using HttpCallback = function<void(const HttpResponse&)>;
```

---

### 9. AuthClient.h — `bool`, `int64_t` 사용

**파일**: `Client/Public/Network/AuthClient.h` L9, L12

```cpp
// 현재
bool success = false;       // L9
int64_t expiresAt = 0;      // L12

// 수정
bool_t  success   = false;
i64_t   expiresAt = 0;
```

---

### 10. CHttpClient.cpp L187 — `(int32_t)` 캐스팅

```cpp
// 현재
response.statusCode = (int32_t)statusCode;

// 수정
response.statusCode = (i32_t)statusCode;
```

---

## ⚪ vcxproj 파일명 불일치 (1건)

### 11. Client.vcxproj — `ShotClient` → `CShopClient`

**파일**: `Client/Include/Client.vcxproj` L106, L116

```xml
<!-- 현재 (오타: Shot → Shop) -->
<ClCompile Include="..\Private\Network\ShotClient.cpp" />
<ClInclude Include="..\Public\Network\ShotClient.h" />

<!-- 수정 (실제 파일명과 일치) -->
<ClCompile Include="..\Private\Network\CShopClient.cpp" />
<ClInclude Include="..\Public\Network\CShopClient.h" />
```

실제 파일: `CShopClient.cpp`, `CShopClient.h` — vcxproj에 `ShotClient`로 등록되어 빌드 시 파일 못 찾음.

---

## 수정 파일 목록

| # | 파일 | 수정 내용 |
|---|------|----------|
| 1 | `ProfileClient.h` | 중복 생성자 제거, 소멸자 추가 |
| 2 | `AuthClient.cpp` | L29 `{\email` → `{\"email\"`, `using namespace Client;` 추가 |
| 3 | `CHttpClient.cpp` | L210 잉여 `}` 삭제 |
| 4 | `ProfileClient.cpp` | `user_name` → `username`, `winRate` → `win_rate` |
| 5 | `CHttpClient.h` | `int32_t` → `i32_t`, `bool` → `bool_t`, `std::function` → `function` |
| 6 | `AuthClient.h` | `bool` → `bool_t`, `int64_t` → `i64_t` |
| 7 | `CHttpClient.cpp` | L187 `(int32_t)` → `(i32_t)` |
| 8 | `Client.vcxproj` | `ShotClient` → `CShopClient` |
