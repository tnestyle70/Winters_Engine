# Phase 2: 유저모드 안티치트 심화 — 구현 계획서

> **목표**: IAT/Inline 훅 탐지, 메모리 암호화, 오버레이 탐지
> **위치**: Phase 1과 동일 (`Engine/Public/AntiCheat/`, `Engine/Private/AntiCheat/`)
> **의존성**: Phase 1 완료

---

## 추가 파일 목록

| # | 경로 | 설명 |
|---|------|------|
| 1 | `Engine/Public/AntiCheat/CHookDetector.h` | IAT/Inline/EAT 훅 탐지 |
| 2 | `Engine/Private/AntiCheat/CHookDetector.cpp` | 구현 |
| 3 | `Engine/Public/AntiCheat/COverlayDetector.h` | 오버레이 윈도우 탐지 |
| 4 | `Engine/Private/AntiCheat/COverlayDetector.cpp` | 구현 |
| 5 | `Engine/Public/AntiCheat/CEncryptedValue.h` | 암호화 값 템플릿 (헤더 Only) |
| 6 | `Engine/Public/AntiCheat/CTimingValidator.h` | Speed Hack 탐지 (타이머 교차검증) |
| 7 | `Engine/Private/AntiCheat/CTimingValidator.cpp` | 구현 |

---

## 1. CHookDetector.h / .cpp

**경로**: `Engine/Public/AntiCheat/CHookDetector.h`

```cpp
#pragma once
#include "AntiCheat/IAntiCheatModule.h"
#include <memory>
#include <Windows.h>

// API 훅 탐지 — IAT, Inline, EAT 훅을 종합 탐지
class CHookDetector : public IAntiCheatModule
{
public:
    ~CHookDetector() override = default;

    static std::unique_ptr<CHookDetector> Create(HMODULE hGameModule);

    ACDetectionResult Check() override;
    const char* GetName() const override { return "HookDetector"; }

private:
    CHookDetector() = default;

    // IAT 훅 탐지: 각 import 함수 주소가 해당 DLL 범위 내인지
    bool DetectIATHooks();

    // ntdll 함수 Inline 훅 탐지: 디스크 vs 메모리 비교
    bool DetectNtdllHooks();

    // D3D11 함수 Inline 훅 탐지 (월핵/ESP 방어)
    bool DetectD3D11Hooks();

    HMODULE m_hGameModule = nullptr;
};
```

**경로**: `Engine/Private/AntiCheat/CHookDetector.cpp`

```cpp
#include "AntiCheat/CHookDetector.h"
#include <Psapi.h>
#pragma comment(lib, "Psapi.lib")

std::unique_ptr<CHookDetector> CHookDetector::Create(HMODULE hGameModule)
{
    auto det = std::unique_ptr<CHookDetector>(new CHookDetector());
    det->m_hGameModule = hGameModule;
    return det;
}

ACDetectionResult CHookDetector::Check()
{
    ACDetectionResult result;

    if (DetectIATHooks())
    {
        result.detected = true;
        result.type = static_cast<uint32_t>(EACDetectionType::IATHook);
        return result;
    }

    if (DetectNtdllHooks())
    {
        result.detected = true;
        result.type = static_cast<uint32_t>(EACDetectionType::InlineHook);
        return result;
    }

    if (DetectD3D11Hooks())
    {
        result.detected = true;
        result.type = static_cast<uint32_t>(EACDetectionType::InlineHook);
        return result;
    }

    return result;
}

bool CHookDetector::DetectIATHooks()
{
    auto* pDos = reinterpret_cast<IMAGE_DOS_HEADER*>(m_hGameModule);
    auto* pNt = reinterpret_cast<IMAGE_NT_HEADERS*>(
        reinterpret_cast<BYTE*>(m_hGameModule) + pDos->e_lfanew);

    auto& importDir = pNt->OptionalHeader
        .DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (importDir.Size == 0) return false;

    auto* pImport = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
        reinterpret_cast<BYTE*>(m_hGameModule) + importDir.VirtualAddress);

    while (pImport->Name)
    {
        const char* dllName = reinterpret_cast<const char*>(
            reinterpret_cast<BYTE*>(m_hGameModule) + pImport->Name);

        HMODULE hDll = GetModuleHandleA(dllName);
        if (!hDll) { pImport++; continue; }

        MODULEINFO mi;
        GetModuleInformation(GetCurrentProcess(), hDll, &mi, sizeof(mi));
        BYTE* dllStart = reinterpret_cast<BYTE*>(mi.lpBaseOfDll);
        BYTE* dllEnd = dllStart + mi.SizeOfImage;

        auto* pThunk = reinterpret_cast<IMAGE_THUNK_DATA*>(
            reinterpret_cast<BYTE*>(m_hGameModule) + pImport->FirstThunk);

        while (pThunk->u1.Function)
        {
            auto* funcAddr = reinterpret_cast<BYTE*>(pThunk->u1.Function);
            if (funcAddr < dllStart || funcAddr >= dllEnd)
                return true; // IAT 훅!
            pThunk++;
        }
        pImport++;
    }
    return false;
}

bool CHookDetector::DetectNtdllHooks()
{
    // ntdll.dll의 주요 함수 프롤로그 검사
    // x64 syscall 스텁 패턴: 4C 8B D1 B8 xx xx 00 00 0F 05 C3
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (!hNtdll) return false;

    const char* criticalFuncs[] = {
        "NtReadVirtualMemory",
        "NtWriteVirtualMemory",
        "NtOpenProcess",
        "NtCreateThreadEx",
        "NtQueryInformationProcess",
    };

    for (const char* funcName : criticalFuncs)
    {
        BYTE* pFunc = reinterpret_cast<BYTE*>(
            GetProcAddress(hNtdll, funcName));
        if (!pFunc) continue;

        // syscall 스텁의 첫 바이트 확인
        // 정상: 4C 8B D1 (mov r10, rcx)
        // 훅됨: E9 xx xx xx xx (jmp) 또는 FF 25 (jmp [rip+x])
        if (pFunc[0] == 0xE9 || pFunc[0] == 0xFF)
            return true; // Inline Hook!
        if (pFunc[0] != 0x4C || pFunc[1] != 0x8B || pFunc[2] != 0xD1)
            return true; // 프롤로그 변조!
    }
    return false;
}

bool CHookDetector::DetectD3D11Hooks()
{
    HMODULE hD3D11 = GetModuleHandleW(L"d3d11.dll");
    if (!hD3D11) return false;

    // Present, DrawIndexed 등 월핵/ESP가 훅하는 함수
    // IDXGISwapChain::Present는 dxgi.dll에 있음
    HMODULE hDXGI = GetModuleHandleW(L"dxgi.dll");
    if (!hDXGI) return false;

    // dxgi.dll .text 섹션의 첫 바이트가 JMP인지 간이 검사
    // 상세 검사는 vtable 포인터 검증으로 수행
    // (추후 Phase 2에서 확장)

    return false;
}
```

---

## 2. CEncryptedValue.h (헤더 Only 템플릿)

**경로**: `Engine/Public/AntiCheat/CEncryptedValue.h`

```cpp
#pragma once
#include <cstdint>
#include <cstring>
#include <intrin.h>

// XOR 암호화된 값 — Cheat Engine 메모리 스캔 방지
// T = float32, int32, uint32 등 POD 타입
template <typename T>
class CEncryptedValue
{
    static_assert(sizeof(T) <= 8, "CEncryptedValue supports up to 8-byte types");

public:
    CEncryptedValue() { Set(T{}); }
    CEncryptedValue(T value) { Set(value); }

    void Set(T value)
    {
        m_Key = GenerateKey();
        uint64_t raw = 0;
        memcpy(&raw, &value, sizeof(T));
        m_Encrypted = raw ^ m_Key;
    }

    T Get() const
    {
        uint64_t raw = m_Encrypted ^ m_Key;
        T value;
        memcpy(&value, &raw, sizeof(T));
        return value;
    }

    // 연산자 오버로드 — 기존 float처럼 사용 가능
    operator T() const { return Get(); }
    CEncryptedValue& operator=(T value) { Set(value); return *this; }
    CEncryptedValue& operator+=(T value) { Set(Get() + value); return *this; }
    CEncryptedValue& operator-=(T value) { Set(Get() - value); return *this; }

private:
    static uint64_t GenerateKey()
    {
        return static_cast<uint64_t>(__rdtsc()) ^
               (static_cast<uint64_t>(__rdtsc()) << 17);
    }

    uint64_t m_Encrypted = 0;
    uint64_t m_Key       = 0;
};

// 타입 별칭
using EncFloat  = CEncryptedValue<float>;
using EncInt32  = CEncryptedValue<int32_t>;
using EncUInt32 = CEncryptedValue<uint32_t>;
```

---

## 3. COverlayDetector.h / .cpp

**경로**: `Engine/Public/AntiCheat/COverlayDetector.h`

```cpp
#pragma once
#include "AntiCheat/IAntiCheatModule.h"
#include <memory>

// 오버레이 윈도우 탐지 (ESP 치트 방어)
class COverlayDetector : public IAntiCheatModule
{
public:
    ~COverlayDetector() override = default;

    static std::unique_ptr<COverlayDetector> Create(HWND hGameWindow);

    ACDetectionResult Check() override;
    const char* GetName() const override { return "OverlayDetector"; }

private:
    COverlayDetector() = default;
    HWND m_hGameWindow = nullptr;
};
```

**경로**: `Engine/Private/AntiCheat/COverlayDetector.cpp`

```cpp
#include "AntiCheat/COverlayDetector.h"
#include <Windows.h>
#include <vector>

std::unique_ptr<COverlayDetector> COverlayDetector::Create(HWND hGameWindow)
{
    auto det = std::unique_ptr<COverlayDetector>(new COverlayDetector());
    det->m_hGameWindow = hGameWindow;
    return det;
}

ACDetectionResult COverlayDetector::Check()
{
    ACDetectionResult result;

    // 전체 화면 + 투명 + 최상위 + 클릭 통과 윈도우 탐지
    struct Context { HWND hGame; bool found; };
    Context ctx = { m_hGameWindow, false };

    EnumWindows([](HWND hWnd, LPARAM lParam) -> BOOL
    {
        auto* pCtx = reinterpret_cast<Context*>(lParam);
        if (hWnd == pCtx->hGame) return TRUE;

        LONG exStyle = GetWindowLongW(hWnd, GWL_EXSTYLE);
        bool layered     = (exStyle & WS_EX_LAYERED)     != 0;
        bool topmost     = (exStyle & WS_EX_TOPMOST)     != 0;
        bool transparent = (exStyle & WS_EX_TRANSPARENT)  != 0;

        if (layered && topmost && transparent)
        {
            RECT rect;
            GetWindowRect(hWnd, &rect);
            int w = rect.right - rect.left;
            int h = rect.bottom - rect.top;

            // 화면의 80% 이상을 덮는 윈도우
            int screenW = GetSystemMetrics(SM_CXSCREEN);
            int screenH = GetSystemMetrics(SM_CYSCREEN);
            if (w >= screenW * 80 / 100 && h >= screenH * 80 / 100)
            {
                // 화이트리스트 제외 (게임바, Discord, OBS 등)
                wchar_t className[256];
                GetClassNameW(hWnd, className, 256);

                // 알려진 정상 오버레이 제외
                if (wcsstr(className, L"Discord") ||
                    wcsstr(className, L"GeForce") ||
                    wcsstr(className, L"NVIDIA") ||
                    wcsstr(className, L"Steam"))
                    return TRUE;

                pCtx->found = true;
                return FALSE; // 탐지됨, 열거 중단
            }
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&ctx));

    if (ctx.found)
    {
        result.detected = true;
        result.type = static_cast<uint32_t>(EACDetectionType::SuspiciousMemory);
    }
    return result;
}
```

---

## 4. CTimingValidator.h / .cpp

**경로**: `Engine/Public/AntiCheat/CTimingValidator.h`

```cpp
#pragma once
#include "AntiCheat/IAntiCheatModule.h"
#include <memory>

// Speed Hack 탐지 — 복수 타이머 교차 검증
class CTimingValidator : public IAntiCheatModule
{
public:
    ~CTimingValidator() override = default;

    static std::unique_ptr<CTimingValidator> Create();

    ACDetectionResult Check() override;
    const char* GetName() const override { return "TimingValidator"; }

private:
    CTimingValidator() = default;

    // QPC, GetTickCount64, RDTSC 3개 타이머의 경과 시간 비교
    // 하나만 훅되었으면 다른 둘과 불일치 → Speed Hack

    int64_t m_LastQPC    = 0;
    int64_t m_LastTick64 = 0;
    int64_t m_LastRDTSC  = 0;
    bool    m_bInitialized = false;
};
```

**경로**: `Engine/Private/AntiCheat/CTimingValidator.cpp`

```cpp
#include "AntiCheat/CTimingValidator.h"
#include <Windows.h>
#include <intrin.h>
#include <cmath>

std::unique_ptr<CTimingValidator> CTimingValidator::Create()
{
    return std::unique_ptr<CTimingValidator>(new CTimingValidator());
}

ACDetectionResult CTimingValidator::Check()
{
    ACDetectionResult result;

    LARGE_INTEGER freq, qpc;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&qpc);
    int64_t tick64 = static_cast<int64_t>(GetTickCount64());
    int64_t rdtsc  = static_cast<int64_t>(__rdtsc());

    if (!m_bInitialized)
    {
        m_LastQPC    = qpc.QuadPart;
        m_LastTick64 = tick64;
        m_LastRDTSC  = rdtsc;
        m_bInitialized = true;
        return result;
    }

    // 경과 시간 (ms 단위로 통일)
    double dtQPC  = static_cast<double>(qpc.QuadPart - m_LastQPC)
                  / freq.QuadPart * 1000.0;
    double dtTick = static_cast<double>(tick64 - m_LastTick64);

    // QPC와 GetTickCount64 차이가 20% 이상이면 Speed Hack 의심
    if (dtQPC > 100.0 && dtTick > 100.0) // 최소 100ms 경과 후 판단
    {
        double ratio = dtQPC / dtTick;
        if (ratio < 0.8 || ratio > 1.2) // ±20% 허용
        {
            result.detected = true;
            result.type = static_cast<uint32_t>(EACDetectionType::TimingAnomaly);
            result.detail = static_cast<uint64_t>(ratio * 1000); // 비율 × 1000
        }
    }

    m_LastQPC    = qpc.QuadPart;
    m_LastTick64 = tick64;
    m_LastRDTSC  = rdtsc;

    return result;
}
```

---

## ECS 컴포넌트에 암호화 적용 예시

```cpp
// Engine/Public/ECS/Components/HealthComponent.h — 수정
#pragma once
#include "AntiCheat/CEncryptedValue.h"

struct HealthComponent
{
    EncFloat  current   = 100.0f;   // 암호화된 float
    EncFloat  maximum   = 100.0f;
    EncFloat  regenRate = 0.0f;
    bool      alive     = true;

    // 사용법은 기존과 동일 (연산자 오버로드)
    // health.current -= damage;
    // if (health.current <= 0.0f) ...
};

struct StatsComponent
{
    EncFloat  attackDamage = 50.0f;
    EncFloat  abilityPower = 0.0f;
    EncFloat  armor        = 20.0f;
    EncFloat  magicResist  = 20.0f;
    EncFloat  moveSpeed    = 325.0f;
};
```

---

## Verification

```
[ ] CHookDetector: ntdll 함수 프롤로그 변조 → 탐지
[ ] CHookDetector: IAT 엔트리를 외부 주소로 변경 → 탐지
[ ] COverlayDetector: 전체화면 투명 윈도우 생성 → 탐지
[ ] COverlayDetector: Discord/NVIDIA 오버레이 → 미탐지 (화이트리스트)
[ ] CTimingValidator: Speed Hack 도구 → QPC/Tick 비율 이상 탐지
[ ] CEncryptedValue: Cheat Engine "float 100.0" 스캔 → 미발견
[ ] CEncryptedValue: 기존 코드 호환성 (health -= damage 동작 확인)
[ ] 성능: EncFloat Get/Set 오버헤드 < 1ns/call
```
