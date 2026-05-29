# Phase 1: 유저모드 안티치트 기초 — 구현 계획서

> **목표**: Engine DLL 내부에서 디버거 탐지, 코드 무결성, 모듈 스캔
> **위치**: `Engine/Public/AntiCheat/`, `Engine/Private/AntiCheat/`
> **필터**: `11. AntiCheat` (Engine.vcxproj.filters)
> **의존성**: 없음 (Engine 내부 독립 모듈)

---

## 파일 목록

| # | 경로 | 설명 |
|---|------|------|
| 1 | `Engine/Public/AntiCheat/IAntiCheatModule.h` | 안티치트 모듈 인터페이스 |
| 2 | `Engine/Public/AntiCheat/CAntiCheatManager.h` | 매니저 (모듈 통합) |
| 3 | `Engine/Private/AntiCheat/CAntiCheatManager.cpp` | 구현 |
| 4 | `Engine/Public/AntiCheat/CDebugDetector.h` | 디버거 탐지 |
| 5 | `Engine/Private/AntiCheat/CDebugDetector.cpp` | 구현 (7가지 탐지 방법) |
| 6 | `Engine/Public/AntiCheat/CIntegrityChecker.h` | .text 섹션 무결성 |
| 7 | `Engine/Private/AntiCheat/CIntegrityChecker.cpp` | 구현 (FNV-1a 해시) |
| 8 | `Engine/Public/AntiCheat/CModuleScanner.h` | 로드된 DLL 검증 |
| 9 | `Engine/Private/AntiCheat/CModuleScanner.cpp` | 구현 (서명/블랙리스트) |
| 10 | `Engine/Public/AntiCheat/CHeartbeat.h` | 서버 heartbeat |
| 11 | `Engine/Private/AntiCheat/CHeartbeat.cpp` | 구현 |
| 12 | `Engine/Include/WintersAntiCheat.h` | DLL 공개 API (WINTERS_API) |

---

## 1. IAntiCheatModule.h (인터페이스)

**경로**: `Engine/Public/AntiCheat/IAntiCheatModule.h`

```cpp
#pragma once
#include <cstdint>

// 탐지 결과 구조체
struct ACDetectionResult
{
    bool        detected    = false;
    uint32_t    type        = 0;       // EACDetectionType
    uint64_t    detail      = 0;       // 추가 정보 (해시, PID 등)
};

enum class EACDetectionType : uint32_t
{
    None              = 0,
    DebuggerAttached  = 100,
    DebugPort         = 101,
    HardwareBreakpoint= 102,
    TimingAnomaly     = 103,
    CodeTampered      = 200,
    InlineHook        = 201,
    IATHook           = 202,
    UnsignedModule    = 300,
    BlacklistedModule = 301,
    SuspiciousMemory  = 302,
    HeartbeatTimeout  = 400,
};

// 안티치트 탐지 모듈 인터페이스
class IAntiCheatModule
{
public:
    virtual ~IAntiCheatModule() = default;

    // 검사 수행 — 비동기 스레드에서 호출됨
    virtual ACDetectionResult Check() = 0;

    // 모듈 이름 (로깅용)
    virtual const char* GetName() const = 0;
};
```

---

## 2. CDebugDetector.h / .cpp

**경로**: `Engine/Public/AntiCheat/CDebugDetector.h`

```cpp
#pragma once
#include "AntiCheat/IAntiCheatModule.h"
#include <memory>

// 디버거 탐지 모듈 — 7가지 방법 병행
class CDebugDetector : public IAntiCheatModule
{
public:
    ~CDebugDetector() override = default;

    static std::unique_ptr<CDebugDetector> Create();

    ACDetectionResult Check() override;
    const char* GetName() const override { return "DebugDetector"; }

private:
    CDebugDetector() = default;

    bool CheckPEB();               // IsDebuggerPresent
    bool CheckPEBDirect();         // __readgsqword(0x60)
    bool CheckRemoteDebugger();    // CheckRemoteDebuggerPresent
    bool CheckDebugPort();         // NtQueryInformationProcess
    bool CheckHardwareBP();        // DR0~DR3 레지스터
    bool CheckTiming();            // 타이밍 기반
    bool CheckDebugString();       // OutputDebugString 예외 기반
};
```

**경로**: `Engine/Private/AntiCheat/CDebugDetector.cpp`

```cpp
#include "AntiCheat/CDebugDetector.h"
#include <Windows.h>
#include <winternl.h>
#include <intrin.h>

// NtQueryInformationProcess 타입 정의
typedef NTSTATUS(NTAPI* pfnNtQueryInformationProcess)(
    HANDLE, ULONG, PVOID, ULONG, PULONG);

std::unique_ptr<CDebugDetector> CDebugDetector::Create()
{
    return std::unique_ptr<CDebugDetector>(new CDebugDetector());
}

ACDetectionResult CDebugDetector::Check()
{
    ACDetectionResult result;

    // 여러 방법 중 하나라도 탐지되면 리포트
    // 랜덤하게 2~3개만 검사 (매번 전부 검사하면 패턴 예측 가능)
    uint32_t seed = static_cast<uint32_t>(__rdtsc());
    int checkCount = 2 + (seed % 2); // 2~3개

    bool checks[] = {
        CheckPEB(),
        CheckPEBDirect(),
        CheckRemoteDebugger(),
        CheckDebugPort(),
        CheckHardwareBP(),
        CheckTiming(),
    };
    const EACDetectionType types[] = {
        EACDetectionType::DebuggerAttached,
        EACDetectionType::DebuggerAttached,
        EACDetectionType::DebuggerAttached,
        EACDetectionType::DebugPort,
        EACDetectionType::HardwareBreakpoint,
        EACDetectionType::TimingAnomaly,
    };

    int startIdx = seed % 6;
    for (int i = 0; i < checkCount; ++i)
    {
        int idx = (startIdx + i) % 6;
        if (checks[idx])
        {
            result.detected = true;
            result.type = static_cast<uint32_t>(types[idx]);
            return result;
        }
    }
    return result;
}

bool CDebugDetector::CheckPEB()
{
    return IsDebuggerPresent() != FALSE;
}

bool CDebugDetector::CheckPEBDirect()
{
#ifdef _WIN64
    PPEB pPeb = reinterpret_cast<PPEB>(__readgsqword(0x60));
#else
    PPEB pPeb = reinterpret_cast<PPEB>(__readfsdword(0x30));
#endif
    return pPeb->BeingDebugged != 0;
}

bool CDebugDetector::CheckRemoteDebugger()
{
    BOOL debugged = FALSE;
    CheckRemoteDebuggerPresent(GetCurrentProcess(), &debugged);
    return debugged != FALSE;
}

bool CDebugDetector::CheckDebugPort()
{
    auto pFunc = reinterpret_cast<pfnNtQueryInformationProcess>(
        GetProcAddress(GetModuleHandleW(L"ntdll.dll"),
            "NtQueryInformationProcess"));
    if (!pFunc) return false;

    DWORD_PTR debugPort = 0;
    NTSTATUS status = pFunc(GetCurrentProcess(), 7, // ProcessDebugPort
        &debugPort, sizeof(debugPort), nullptr);
    return NT_SUCCESS(status) && debugPort != 0;
}

bool CDebugDetector::CheckHardwareBP()
{
    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (!GetThreadContext(GetCurrentThread(), &ctx))
        return false;
    return (ctx.Dr0 != 0 || ctx.Dr1 != 0 || ctx.Dr2 != 0 || ctx.Dr3 != 0);
}

bool CDebugDetector::CheckTiming()
{
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);

    volatile int sum = 0;
    for (int i = 0; i < 1000; ++i) sum += i;

    QueryPerformanceCounter(&end);
    double ms = static_cast<double>(end.QuadPart - start.QuadPart)
              / freq.QuadPart * 1000.0;
    return ms > 50.0; // 정상: <1ms, 디버거 싱글스텝: >50ms
}

bool CDebugDetector::CheckDebugString()
{
    // OutputDebugString은 디버거가 없으면 예외 발생
    __try
    {
        OutputDebugStringW(L"AC_PROBE");
    }
    __except (GetExceptionCode() == DBG_PRINTEXCEPTION_C ?
        EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
    {
        return false; // 예외 발생 = 디버거 없음
    }
    return true; // 예외 미발생 = 디버거 있음
}
```

---

## 3. CIntegrityChecker.h / .cpp

**경로**: `Engine/Public/AntiCheat/CIntegrityChecker.h`

```cpp
#pragma once
#include "AntiCheat/IAntiCheatModule.h"
#include <memory>
#include <vector>
#include <Windows.h>

// 코드 무결성 검증 — .text 섹션 해시 + 함수 프롤로그 검증
class CIntegrityChecker : public IAntiCheatModule
{
public:
    ~CIntegrityChecker() override = default;

    static std::unique_ptr<CIntegrityChecker> Create(HMODULE hModule);

    ACDetectionResult Check() override;
    const char* GetName() const override { return "IntegrityChecker"; }

    // 중요 함수 프롤로그 등록 (inline hook 탐지용)
    void RegisterFunction(const char* name, void* pFunc, size_t checkSize = 16);

private:
    CIntegrityChecker() = default;

    uint64_t ComputeTextHash() const;

    HMODULE  m_hModule       = nullptr;
    BYTE*    m_pTextStart    = nullptr;
    size_t   m_TextSize      = 0;
    uint64_t m_OriginalHash  = 0;

    struct FuncEntry
    {
        const char* name;
        void*       pFunc;
        uint8_t     originalBytes[16];
        size_t      checkSize;
    };
    std::vector<FuncEntry> m_RegisteredFunctions;
};
```

**경로**: `Engine/Private/AntiCheat/CIntegrityChecker.cpp`

```cpp
#include "AntiCheat/CIntegrityChecker.h"
#include <cstring>

std::unique_ptr<CIntegrityChecker> CIntegrityChecker::Create(HMODULE hModule)
{
    auto checker = std::unique_ptr<CIntegrityChecker>(new CIntegrityChecker());
    checker->m_hModule = hModule;

    // .text 섹션 찾기
    auto* pDos = reinterpret_cast<IMAGE_DOS_HEADER*>(hModule);
    auto* pNt = reinterpret_cast<IMAGE_NT_HEADERS*>(
        reinterpret_cast<BYTE*>(hModule) + pDos->e_lfanew);
    auto* pSec = IMAGE_FIRST_SECTION(pNt);

    for (WORD i = 0; i < pNt->FileHeader.NumberOfSections; ++i)
    {
        if (memcmp(pSec[i].Name, ".text", 5) == 0)
        {
            checker->m_pTextStart = reinterpret_cast<BYTE*>(hModule)
                                  + pSec[i].VirtualAddress;
            checker->m_TextSize = pSec[i].Misc.VirtualSize;
            break;
        }
    }

    checker->m_OriginalHash = checker->ComputeTextHash();
    return checker;
}

ACDetectionResult CIntegrityChecker::Check()
{
    ACDetectionResult result;

    // 1. .text 섹션 전체 해시 검증
    if (m_pTextStart && ComputeTextHash() != m_OriginalHash)
    {
        result.detected = true;
        result.type = static_cast<uint32_t>(EACDetectionType::CodeTampered);
        return result;
    }

    // 2. 등록된 함수 프롤로그 검증
    for (const auto& entry : m_RegisteredFunctions)
    {
        if (memcmp(entry.pFunc, entry.originalBytes, entry.checkSize) != 0)
        {
            result.detected = true;
            result.type = static_cast<uint32_t>(EACDetectionType::InlineHook);
            return result;
        }
    }

    return result;
}

void CIntegrityChecker::RegisterFunction(
    const char* name, void* pFunc, size_t checkSize)
{
    FuncEntry entry;
    entry.name = name;
    entry.pFunc = pFunc;
    entry.checkSize = (checkSize > 16) ? 16 : checkSize;
    memcpy(entry.originalBytes, pFunc, entry.checkSize);
    m_RegisteredFunctions.push_back(entry);
}

uint64_t CIntegrityChecker::ComputeTextHash() const
{
    if (!m_pTextStart) return 0;

    // FNV-1a 64비트
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < m_TextSize; ++i)
    {
        hash ^= m_pTextStart[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}
```

---

## 4. CModuleScanner.h / .cpp

**경로**: `Engine/Public/AntiCheat/CModuleScanner.h`

```cpp
#pragma once
#include "AntiCheat/IAntiCheatModule.h"
#include <memory>
#include <vector>
#include <string>

struct SuspiciousModule
{
    std::wstring name;
    std::wstring path;
    uint32_t     reason;  // EACDetectionType
};

// 로드된 모듈 스캔 — 미서명 DLL, 블랙리스트, 미등록 실행 영역
class CModuleScanner : public IAntiCheatModule
{
public:
    ~CModuleScanner() override = default;

    static std::unique_ptr<CModuleScanner> Create();

    ACDetectionResult Check() override;
    const char* GetName() const override { return "ModuleScanner"; }

    // 상세 결과
    const std::vector<SuspiciousModule>& GetSuspicious() const
    { return m_Suspicious; }

private:
    CModuleScanner() = default;

    void ScanLoadedModules();
    void ScanUnmappedRegions();
    bool IsSystemPath(const std::wstring& path) const;
    bool VerifySignature(const wchar_t* filePath) const;
    uint64_t HashWString(const std::wstring& str) const;

    std::vector<SuspiciousModule> m_Suspicious;

    // 알려진 치트 DLL 이름 해시 목록
    static const uint64_t s_Blacklist[];
    static const size_t   s_BlacklistSize;
};
```

**경로**: `Engine/Private/AntiCheat/CModuleScanner.cpp`

```cpp
#include "AntiCheat/CModuleScanner.h"
#include <Windows.h>
#include <Psapi.h>
#include <Softpub.h>
#include <WinTrust.h>
#pragma comment(lib, "Wintrust.lib")
#pragma comment(lib, "Psapi.lib")

const uint64_t CModuleScanner::s_Blacklist[] = {
    // 해시 목록 — 실제 구현 시 알려진 치트 DLL 이름 해시 추가
    0 // placeholder
};
const size_t CModuleScanner::s_BlacklistSize = 0;

std::unique_ptr<CModuleScanner> CModuleScanner::Create()
{
    return std::unique_ptr<CModuleScanner>(new CModuleScanner());
}

ACDetectionResult CModuleScanner::Check()
{
    ACDetectionResult result;
    m_Suspicious.clear();

    ScanLoadedModules();
    ScanUnmappedRegions();

    if (!m_Suspicious.empty())
    {
        result.detected = true;
        result.type = m_Suspicious[0].reason;
    }
    return result;
}

void CModuleScanner::ScanLoadedModules()
{
    HMODULE hModules[1024];
    DWORD cbNeeded;
    HANDLE hProc = GetCurrentProcess();

    if (!EnumProcessModules(hProc, hModules, sizeof(hModules), &cbNeeded))
        return;

    DWORD count = cbNeeded / sizeof(HMODULE);
    for (DWORD i = 0; i < count; ++i)
    {
        wchar_t name[MAX_PATH], path[MAX_PATH];
        GetModuleBaseNameW(hProc, hModules[i], name, MAX_PATH);
        GetModuleFileNameExW(hProc, hModules[i], path, MAX_PATH);

        // 시스템 경로 제외
        if (IsSystemPath(path)) continue;

        // 블랙리스트 검사
        uint64_t nameHash = HashWString(name);
        for (size_t j = 0; j < s_BlacklistSize; ++j)
        {
            if (nameHash == s_Blacklist[j])
            {
                m_Suspicious.push_back({name, path,
                    static_cast<uint32_t>(EACDetectionType::BlacklistedModule)});
                return;
            }
        }

        // 서명 검증
        if (!VerifySignature(path))
        {
            m_Suspicious.push_back({name, path,
                static_cast<uint32_t>(EACDetectionType::UnsignedModule)});
        }
    }
}

void CModuleScanner::ScanUnmappedRegions()
{
    // 모듈 목록에 없는 RX/RWX 메모리 영역 탐지 (Manual Mapping)
    HANDLE hProc = GetCurrentProcess();
    HMODULE hModules[1024];
    DWORD cbNeeded;
    EnumProcessModules(hProc, hModules, sizeof(hModules), &cbNeeded);
    DWORD modCount = cbNeeded / sizeof(HMODULE);

    // 알려진 모듈 범위 수집
    struct ModRange { BYTE* base; SIZE_T size; };
    std::vector<ModRange> known;
    for (DWORD i = 0; i < modCount; ++i)
    {
        MODULEINFO mi;
        GetModuleInformation(hProc, hModules[i], &mi, sizeof(mi));
        known.push_back({reinterpret_cast<BYTE*>(mi.lpBaseOfDll), mi.SizeOfImage});
    }

    MEMORY_BASIC_INFORMATION mbi;
    BYTE* addr = nullptr;
    while (VirtualQuery(addr, &mbi, sizeof(mbi)))
    {
        if (mbi.State == MEM_COMMIT && mbi.Type == MEM_PRIVATE &&
            (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ |
                            PAGE_EXECUTE_READWRITE)))
        {
            BYTE* base = reinterpret_cast<BYTE*>(mbi.AllocationBase);
            bool isKnown = false;
            for (const auto& mod : known)
            {
                if (base >= mod.base && base < mod.base + mod.size)
                { isKnown = true; break; }
            }
            if (!isKnown)
            {
                m_Suspicious.push_back({L"[unmapped]", L"",
                    static_cast<uint32_t>(EACDetectionType::SuspiciousMemory)});
            }
        }
        addr = reinterpret_cast<BYTE*>(mbi.BaseAddress) + mbi.RegionSize;
    }
}

bool CModuleScanner::IsSystemPath(const std::wstring& path) const
{
    return (path.find(L"\\Windows\\") != std::wstring::npos ||
            path.find(L"\\System32\\") != std::wstring::npos ||
            path.find(L"\\SysWOW64\\") != std::wstring::npos);
}

bool CModuleScanner::VerifySignature(const wchar_t* filePath) const
{
    WINTRUST_FILE_INFO fileInfo = {};
    fileInfo.cbStruct = sizeof(fileInfo);
    fileInfo.pcwszFilePath = filePath;

    GUID policyGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    WINTRUST_DATA trustData = {};
    trustData.cbStruct = sizeof(trustData);
    trustData.dwUIChoice = WTD_UI_NONE;
    trustData.dwUnionChoice = WTD_CHOICE_FILE;
    trustData.pFile = &fileInfo;
    trustData.dwStateAction = WTD_STATEACTION_VERIFY;

    LONG status = WinVerifyTrust(nullptr, &policyGUID, &trustData);

    trustData.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(nullptr, &policyGUID, &trustData);

    return status == ERROR_SUCCESS;
}

uint64_t CModuleScanner::HashWString(const std::wstring& str) const
{
    uint64_t hash = 14695981039346656037ULL;
    for (wchar_t c : str)
    {
        // 대소문자 무시
        wchar_t lower = (c >= L'A' && c <= L'Z') ? c + 32 : c;
        hash ^= static_cast<uint64_t>(lower);
        hash *= 1099511628211ULL;
    }
    return hash;
}
```

---

## 5. CAntiCheatManager.h / .cpp (Engine 통합)

**경로**: `Engine/Public/AntiCheat/CAntiCheatManager.h`

```cpp
#pragma once
#include "AntiCheat/IAntiCheatModule.h"
#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <functional>

// Engine 안티치트 매니저 — 모든 탐지 모듈을 관리, 비동기 실행
class CAntiCheatManager
{
public:
    ~CAntiCheatManager();

    static std::unique_ptr<CAntiCheatManager> Create();

    // 모듈 등록
    void AddModule(std::unique_ptr<IAntiCheatModule> pModule);

    // 탐지 콜백 (서버 리포트용)
    using DetectionCallback = std::function<void(const ACDetectionResult&, const char* moduleName)>;
    void SetDetectionCallback(DetectionCallback cb);

    // 비동기 감시 시작/중지
    void Start();
    void Stop();

    bool IsRunning() const { return m_bRunning.load(); }

private:
    CAntiCheatManager() = default;
    void MonitorLoop();

    std::vector<std::unique_ptr<IAntiCheatModule>> m_Modules;
    DetectionCallback m_Callback;
    std::thread       m_Thread;
    std::atomic<bool> m_bRunning = false;
};
```

**경로**: `Engine/Private/AntiCheat/CAntiCheatManager.cpp`

```cpp
#include "AntiCheat/CAntiCheatManager.h"
#include <chrono>
#include <intrin.h>

CAntiCheatManager::~CAntiCheatManager()
{
    Stop();
}

std::unique_ptr<CAntiCheatManager> CAntiCheatManager::Create()
{
    return std::unique_ptr<CAntiCheatManager>(new CAntiCheatManager());
}

void CAntiCheatManager::AddModule(std::unique_ptr<IAntiCheatModule> pModule)
{
    m_Modules.push_back(std::move(pModule));
}

void CAntiCheatManager::SetDetectionCallback(DetectionCallback cb)
{
    m_Callback = std::move(cb);
}

void CAntiCheatManager::Start()
{
    if (m_bRunning.load()) return;
    m_bRunning.store(true);
    m_Thread = std::thread([this]() { MonitorLoop(); });
}

void CAntiCheatManager::Stop()
{
    m_bRunning.store(false);
    if (m_Thread.joinable())
        m_Thread.join();
}

void CAntiCheatManager::MonitorLoop()
{
    while (m_bRunning.load())
    {
        // 랜덤 간격 (2~5초)
        uint32_t delayMs = 2000 + (static_cast<uint32_t>(__rdtsc()) % 3000);
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));

        if (!m_bRunning.load()) break;

        for (auto& pModule : m_Modules)
        {
            auto result = pModule->Check();
            if (result.detected && m_Callback)
            {
                m_Callback(result, pModule->GetName());
            }
        }
    }
}
```

---

## 6. WintersAntiCheat.h (DLL 공개 API)

**경로**: `Engine/Include/WintersAntiCheat.h`

```cpp
#pragma once
#include "WintersAPI.h"

// Engine DLL에서 내보내는 안티치트 공개 API
// Client(Game)에서 사용

namespace WintersAC
{
    // 안티치트 초기화 (게임 시작 시 호출)
    WINTERS_API bool Initialize();

    // 안티치트 종료 (게임 종료 시 호출)
    WINTERS_API void Shutdown();

    // 탐지 콜백 설정
    using DetectionFunc = void(*)(uint32_t type, uint64_t detail);
    WINTERS_API void SetCallback(DetectionFunc func);

    // 수동 검사 트리거
    WINTERS_API bool RunCheck();
}
```

---

## vcxproj.filters 추가 (Engine)

```xml
<Filter Include="11. AntiCheat">
  <UniqueIdentifier>{AC110001-0000-0000-0000-000000000001}</UniqueIdentifier>
</Filter>

<!-- Public 헤더 -->
<ClInclude Include="..\Public\AntiCheat\IAntiCheatModule.h">
  <Filter>11. AntiCheat</Filter>
</ClInclude>
<ClInclude Include="..\Public\AntiCheat\CAntiCheatManager.h">
  <Filter>11. AntiCheat</Filter>
</ClInclude>
<ClInclude Include="..\Public\AntiCheat\CDebugDetector.h">
  <Filter>11. AntiCheat</Filter>
</ClInclude>
<ClInclude Include="..\Public\AntiCheat\CIntegrityChecker.h">
  <Filter>11. AntiCheat</Filter>
</ClInclude>
<ClInclude Include="..\Public\AntiCheat\CModuleScanner.h">
  <Filter>11. AntiCheat</Filter>
</ClInclude>

<!-- Include (DLL API) -->
<ClInclude Include="WintersAntiCheat.h">
  <Filter>Include</Filter>
</ClInclude>

<!-- Private 구현 -->
<ClCompile Include="..\Private\AntiCheat\CAntiCheatManager.cpp">
  <Filter>11. AntiCheat</Filter>
</ClCompile>
<ClCompile Include="..\Private\AntiCheat\CDebugDetector.cpp">
  <Filter>11. AntiCheat</Filter>
</ClCompile>
<ClCompile Include="..\Private\AntiCheat\CIntegrityChecker.cpp">
  <Filter>11. AntiCheat</Filter>
</ClCompile>
<ClCompile Include="..\Private\AntiCheat\CModuleScanner.cpp">
  <Filter>11. AntiCheat</Filter>
</ClCompile>
```

---

## Client 통합 코드

```cpp
// Client/Private/CGameApp.cpp 에서 사용

#include "WintersAntiCheat.h"

void CGameApp::Initialize()
{
    // ... 기존 초기화 ...

    // 안티치트 초기화
    WintersAC::SetCallback([](uint32_t type, uint64_t detail)
    {
        // 서버에 탐지 리포트 전송
        // PacketSend(PACKET_AC_DETECTION, type, detail);
    });
    WintersAC::Initialize();
}

void CGameApp::Shutdown()
{
    WintersAC::Shutdown();
    // ... 기존 종료 ...
}
```

---

## Verification

```
[ ] Engine DLL 빌드 → WintersAntiCheat.h 심볼 export 확인
[ ] Cheat Engine 연결 → CDebugDetector 탐지 → 콜백 호출
[ ] x64dbg 연결 → DebugPort 탐지
[ ] 하드웨어 BP 설정 → CheckHardwareBP 탐지
[ ] .text 1바이트 패치 → CIntegrityChecker 탐지
[ ] 미서명 DLL 인젝션 → CModuleScanner 탐지
[ ] 오탐 테스트: Visual Studio 디버깅 시 경고만 (킥 안 함)
[ ] 성능: 안티치트 스레드 CPU 1% 미만
```
