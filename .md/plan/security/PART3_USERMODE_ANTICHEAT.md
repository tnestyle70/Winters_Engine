# Part 3: 유저모드 안티치트 구현

> DLL 없이 게임 프로세스 내에서 실행되는 탐지/방어 시스템.
> 커널 드라이버 없이도 가능한 기초~중급 수준의 보호.

---

## 1. 디버거 탐지

### 1.1 기본 탐지

```cpp
class CDebuggerDetector
{
public:
    // 방법 1: IsDebuggerPresent (PEB.BeingDebugged 확인)
    static bool CheckPEB()
    {
        return IsDebuggerPresent();
    }

    // 방법 2: NtQueryInformationProcess — 원격 디버거 포함
    static bool CheckRemote()
    {
        BOOL debugged = FALSE;
        CheckRemoteDebuggerPresent(GetCurrentProcess(), &debugged);
        return debugged;
    }

    // 방법 3: PEB 직접 읽기 (API 훅 우회)
    static bool CheckPEBDirect()
    {
#ifdef _WIN64
        // x64: TEB는 GS:[0x30], PEB는 TEB+0x60
        PPEB pPeb = reinterpret_cast<PPEB>(__readgsqword(0x60));
#else
        PPEB pPeb = reinterpret_cast<PPEB>(__readfsdword(0x30));
#endif
        return pPeb->BeingDebugged;
    }

    // 방법 4: NtQueryInformationProcess — DebugPort
    static bool CheckDebugPort()
    {
        using NtQueryInfoProc = NTSTATUS(NTAPI*)(
            HANDLE, ULONG, PVOID, ULONG, PULONG);

        auto NtQueryInformationProcess = reinterpret_cast<NtQueryInfoProc>(
            GetProcAddress(GetModuleHandleW(L"ntdll.dll"),
                "NtQueryInformationProcess"));

        DWORD_PTR debugPort = 0;
        NTSTATUS status = NtQueryInformationProcess(
            GetCurrentProcess(),
            7,  // ProcessDebugPort
            &debugPort,
            sizeof(debugPort),
            nullptr);

        return NT_SUCCESS(status) && debugPort != 0;
    }

    // 방법 5: 하드웨어 브레이크포인트 탐지
    static bool CheckHardwareBP()
    {
        CONTEXT ctx = {};
        ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        GetThreadContext(GetCurrentThread(), &ctx);

        // DR0~DR3에 주소가 설정되어 있으면 하드웨어 BP 존재
        return (ctx.Dr0 != 0 || ctx.Dr1 != 0 ||
                ctx.Dr2 != 0 || ctx.Dr3 != 0);
    }

    // 방법 6: 타이밍 기반 탐지
    static bool CheckTiming()
    {
        LARGE_INTEGER freq, start, end;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&start);

        // 간단한 연산 수행 (디버거 싱글스텝 시 시간 폭증)
        volatile int sum = 0;
        for (int i = 0; i < 1000; ++i) sum += i;

        QueryPerformanceCounter(&end);

        double elapsedMs = static_cast<double>(end.QuadPart - start.QuadPart)
                         / freq.QuadPart * 1000.0;

        // 정상: < 0.1ms, 디버거 싱글스텝: > 100ms
        return elapsedMs > 10.0;
    }

    // 방법 7: NtSetInformationThread — 디버거에서 숨기기
    static void HideFromDebugger()
    {
        using NtSetInfoThread = NTSTATUS(NTAPI*)(
            HANDLE, ULONG, PVOID, ULONG);

        auto pFunc = reinterpret_cast<NtSetInfoThread>(
            GetProcAddress(GetModuleHandleW(L"ntdll.dll"),
                "NtSetInformationThread"));

        // ThreadHideFromDebugger (0x11)
        pFunc(GetCurrentThread(), 0x11, nullptr, 0);
        // → 이 스레드는 디버거의 이벤트를 받지 않음
    }
};
```

### 1.2 안티-안티디버그 대응

```
치트 개발자의 우회 기법:
  1. IsDebuggerPresent 훅 → 항상 FALSE 반환
  2. PEB.BeingDebugged 직접 0으로 패치
  3. ScyllaHide 등 안티안티디버그 플러그인 사용

대응:
  - 여러 방법을 조합하여 검사 (하나만 의존 금지)
  - 검사 함수 자체를 난독화
  - 검사 시점을 랜덤화 (매 프레임이 아닌 랜덤 간격)
  - 커널 레벨 검사 병행 (유저모드 우회 무력화)
```

---

## 2. 코드 무결성 검증

### 2.1 .text 섹션 해시 검증

```cpp
class CCodeIntegrity
{
public:
    ~CCodeIntegrity() = default;

    static std::unique_ptr<CCodeIntegrity> Create(HMODULE hModule)
    {
        auto ci = std::unique_ptr<CCodeIntegrity>(new CCodeIntegrity());
        ci->m_hModule = hModule;

        // .text 섹션 찾기
        auto* pDos = reinterpret_cast<IMAGE_DOS_HEADER*>(hModule);
        auto* pNt = reinterpret_cast<IMAGE_NT_HEADERS*>(
            reinterpret_cast<BYTE*>(hModule) + pDos->e_lfanew);
        auto* pSection = IMAGE_FIRST_SECTION(pNt);

        for (WORD i = 0; i < pNt->FileHeader.NumberOfSections; ++i)
        {
            if (memcmp(pSection[i].Name, ".text", 5) == 0)
            {
                ci->m_pTextStart = reinterpret_cast<BYTE*>(hModule)
                                 + pSection[i].VirtualAddress;
                ci->m_TextSize = pSection[i].Misc.VirtualSize;
                break;
            }
        }

        // 초기 해시 저장
        ci->m_OriginalHash = ci->ComputeHash();
        return ci;
    }

    bool Verify()
    {
        uint64_t currentHash = ComputeHash();
        return currentHash == m_OriginalHash;
    }

private:
    CCodeIntegrity() = default;

    uint64_t ComputeHash()
    {
        // FNV-1a 해시 (빠르고 충돌 적음)
        uint64_t hash = 14695981039346656037ULL;
        for (size_t i = 0; i < m_TextSize; ++i)
        {
            hash ^= m_pTextStart[i];
            hash *= 1099511628211ULL;
        }
        return hash;
    }

    HMODULE  m_hModule = nullptr;
    BYTE*    m_pTextStart = nullptr;
    size_t   m_TextSize = 0;
    uint64_t m_OriginalHash = 0;
};

// 사용
auto pIntegrity = CCodeIntegrity::Create(GetModuleHandleW(nullptr));

// 주기적 검증 (매 5초)
if (!pIntegrity->Verify())
{
    // 코드 변조 감지! → 서버에 리포트 + 게임 종료
    ReportCheat(CheatType::CodeTamper);
}
```

### 2.2 중요 함수 프롤로그 검증

```cpp
// 특정 함수의 시작 바이트가 변조되었는지 확인
class CFunctionIntegrity
{
public:
    struct FunctionEntry
    {
        void*    pFunc;
        uint8_t  originalBytes[16];
        size_t   checkSize;
    };

    void RegisterFunction(void* pFunc, size_t checkSize = 16)
    {
        FunctionEntry entry;
        entry.pFunc = pFunc;
        entry.checkSize = checkSize;
        memcpy(entry.originalBytes, pFunc, checkSize);
        m_Functions.push_back(entry);
    }

    bool VerifyAll()
    {
        for (auto& entry : m_Functions)
        {
            if (memcmp(entry.pFunc, entry.originalBytes, entry.checkSize) != 0)
            {
                // Inline Hook 탐지!
                return false;
            }
        }
        return true;
    }

private:
    std::vector<FunctionEntry> m_Functions;
};

// 등록 예시
CFunctionIntegrity integrity;

// ntdll 함수들 — 치트가 자주 훅하는 대상
integrity.RegisterFunction(
    GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtReadVirtualMemory"));
integrity.RegisterFunction(
    GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtWriteVirtualMemory"));
integrity.RegisterFunction(
    GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQuerySystemInformation"));

// D3D11 함수들 — 월핵/ESP가 훅하는 대상
integrity.RegisterFunction(
    GetProcAddress(GetModuleHandleW(L"d3d11.dll"), "D3D11CreateDeviceAndSwapChain"));
```

### 2.3 디스크 vs 메모리 비교

```cpp
// DLL의 디스크 파일과 메모리 이미지를 비교하여 패치 탐지
bool CompareWithDisk(const wchar_t* dllName)
{
    HMODULE hModule = GetModuleHandleW(dllName);
    if (!hModule) return true;

    // 1. DLL 파일 경로 획득
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(hModule, path, MAX_PATH);

    // 2. 파일에서 .text 섹션 읽기
    HANDLE hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, 0, nullptr);
    DWORD fileSize = GetFileSize(hFile, nullptr);
    std::vector<BYTE> fileData(fileSize);
    ReadFile(hFile, fileData.data(), fileSize, nullptr, nullptr);
    CloseHandle(hFile);

    // 3. PE 파싱으로 .text 섹션 위치 찾기
    auto* pFileDos = reinterpret_cast<IMAGE_DOS_HEADER*>(fileData.data());
    auto* pFileNt = reinterpret_cast<IMAGE_NT_HEADERS*>(
        fileData.data() + pFileDos->e_lfanew);
    auto* pFileSec = IMAGE_FIRST_SECTION(pFileNt);

    // 4. 메모리의 .text와 파일의 .text 비교
    for (WORD i = 0; i < pFileNt->FileHeader.NumberOfSections; ++i)
    {
        if (memcmp(pFileSec[i].Name, ".text", 5) == 0)
        {
            BYTE* pMemText = reinterpret_cast<BYTE*>(hModule)
                           + pFileSec[i].VirtualAddress;
            BYTE* pFileText = fileData.data()
                            + pFileSec[i].PointerToRawData;
            size_t textSize = pFileSec[i].SizeOfRawData;

            if (memcmp(pMemText, pFileText, textSize) != 0)
            {
                // .text 섹션이 패치됨! → Inline Hook 존재
                return false;
            }
            break;
        }
    }
    return true;
}
```

---

## 3. 모듈 검증

### 3.1 로드된 DLL 열거 및 검증

```cpp
class CModuleScanner
{
public:
    struct ModuleInfo
    {
        std::wstring name;
        std::wstring path;
        HMODULE      handle;
        bool         isSigned;
    };

    std::vector<ModuleInfo> ScanModules()
    {
        std::vector<ModuleInfo> modules;
        HANDLE hProcess = GetCurrentProcess();

        HMODULE hModules[1024];
        DWORD cbNeeded;
        if (!EnumProcessModules(hProcess, hModules, sizeof(hModules), &cbNeeded))
            return modules;

        DWORD count = cbNeeded / sizeof(HMODULE);
        for (DWORD i = 0; i < count; ++i)
        {
            ModuleInfo info;
            info.handle = hModules[i];

            wchar_t name[MAX_PATH], path[MAX_PATH];
            GetModuleBaseNameW(hProcess, hModules[i], name, MAX_PATH);
            GetModuleFileNameExW(hProcess, hModules[i], path, MAX_PATH);

            info.name = name;
            info.path = path;
            info.isSigned = VerifyFileSignature(path);
            modules.push_back(std::move(info));
        }
        return modules;
    }

    // 알려진 치트 DLL 탐지 (해시 기반)
    bool CheckBlacklist(const std::vector<ModuleInfo>& modules)
    {
        // 알려진 치트 DLL 이름 해시 (평문 대신 해시로 비교)
        static const uint64_t blacklist[] = {
            0xA1B2C3D4E5F6A7B8,  // cheatengine-*.dll
            0x1234567890ABCDEF,  // speedhack.dll
            // ... 수천 개의 알려진 치트 시그니처
        };

        for (const auto& mod : modules)
        {
            uint64_t nameHash = HashString(mod.name);
            for (auto blocked : blacklist)
            {
                if (nameHash == blocked)
                    return false;  // 치트 DLL 발견!
            }
        }
        return true;
    }

private:
    bool VerifyFileSignature(const wchar_t* filePath)
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

        LONG status = WinVerifyTrust(nullptr, &policyGUID, &trustData);
        return status == ERROR_SUCCESS;
    }
};
```

### 3.2 실행 가능 메모리 영역 스캔

```cpp
// Manual Mapping 탐지 — 모듈 목록에 없는 실행 가능 영역 찾기
void ScanUnmappedExecutableRegions()
{
    std::set<BYTE*> knownModuleBases;

    // 알려진 모듈 주소 범위 수집
    HMODULE hModules[1024];
    DWORD cbNeeded;
    EnumProcessModules(GetCurrentProcess(), hModules, sizeof(hModules), &cbNeeded);
    DWORD count = cbNeeded / sizeof(HMODULE);

    for (DWORD i = 0; i < count; ++i)
    {
        MODULEINFO mi;
        GetModuleInformation(GetCurrentProcess(), hModules[i], &mi, sizeof(mi));
        knownModuleBases.insert(reinterpret_cast<BYTE*>(mi.lpBaseOfDll));
    }

    // 전체 메모리 스캔
    MEMORY_BASIC_INFORMATION mbi;
    BYTE* addr = nullptr;

    while (VirtualQuery(addr, &mbi, sizeof(mbi)))
    {
        // 실행 가능 + 커밋된 + 프라이빗 메모리
        if (mbi.State == MEM_COMMIT &&
            mbi.Type == MEM_PRIVATE &&
            (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ |
                            PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
        {
            BYTE* base = reinterpret_cast<BYTE*>(mbi.AllocationBase);
            if (knownModuleBases.find(base) == knownModuleBases.end())
            {
                // 알 수 없는 실행 가능 메모리 발견!
                // Manual-mapped DLL 또는 쉘코드 가능성
                ReportSuspiciousRegion(mbi.BaseAddress, mbi.RegionSize);
            }
        }

        addr = reinterpret_cast<BYTE*>(mbi.BaseAddress) + mbi.RegionSize;
    }
}
```

---

## 4. IAT/EAT 훅 탐지

```cpp
bool DetectIATHooks(HMODULE hModule)
{
    auto* pDos = reinterpret_cast<IMAGE_DOS_HEADER*>(hModule);
    auto* pNt = reinterpret_cast<IMAGE_NT_HEADERS*>(
        reinterpret_cast<BYTE*>(hModule) + pDos->e_lfanew);

    auto& importDir = pNt->OptionalHeader
        .DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

    if (importDir.Size == 0) return true;

    auto* pImport = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
        reinterpret_cast<BYTE*>(hModule) + importDir.VirtualAddress);

    while (pImport->Name)
    {
        const char* dllName = reinterpret_cast<const char*>(
            reinterpret_cast<BYTE*>(hModule) + pImport->Name);

        HMODULE hDll = GetModuleHandleA(dllName);
        if (!hDll) { pImport++; continue; }

        // DLL의 메모리 범위
        MODULEINFO mi;
        GetModuleInformation(GetCurrentProcess(), hDll, &mi, sizeof(mi));
        BYTE* dllStart = reinterpret_cast<BYTE*>(mi.lpBaseOfDll);
        BYTE* dllEnd = dllStart + mi.SizeOfImage;

        // IAT 엔트리 검사
        auto* pThunk = reinterpret_cast<IMAGE_THUNK_DATA*>(
            reinterpret_cast<BYTE*>(hModule) + pImport->FirstThunk);

        while (pThunk->u1.Function)
        {
            BYTE* funcAddr = reinterpret_cast<BYTE*>(pThunk->u1.Function);

            // 함수 주소가 해당 DLL 범위 밖이면 훅!
            if (funcAddr < dllStart || funcAddr >= dllEnd)
            {
                // IAT 훅 탐지!
                return false;
            }

            pThunk++;
        }
        pImport++;
    }
    return true;
}
```

---

## 5. 오버레이/외부 치트 탐지

```cpp
class COverlayDetector
{
public:
    // 의심스러운 오버레이 윈도우 탐지
    static std::vector<HWND> ScanOverlays()
    {
        std::vector<HWND> suspicious;

        EnumWindows([](HWND hWnd, LPARAM lParam) -> BOOL
        {
            auto* pList = reinterpret_cast<std::vector<HWND>*>(lParam);

            // 투명 + 최상위 + 클릭 통과 윈도우 검사
            LONG exStyle = GetWindowLongW(hWnd, GWL_EXSTYLE);
            bool isLayered = (exStyle & WS_EX_LAYERED) != 0;
            bool isTopmost = (exStyle & WS_EX_TOPMOST) != 0;
            bool isTransparent = (exStyle & WS_EX_TRANSPARENT) != 0;

            if (isLayered && isTopmost && isTransparent)
            {
                // 윈도우 크기가 화면 전체인지 확인
                RECT rect;
                GetWindowRect(hWnd, &rect);
                int width = rect.right - rect.left;
                int height = rect.bottom - rect.top;

                if (width >= GetSystemMetrics(SM_CXSCREEN) &&
                    height >= GetSystemMetrics(SM_CYSCREEN))
                {
                    pList->push_back(hWnd);
                }
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&suspicious));

        return suspicious;
    }

    // 의심 윈도우의 프로세스 정보 확인
    static void AnalyzeWindow(HWND hWnd)
    {
        DWORD pid;
        GetWindowThreadProcessId(hWnd, &pid);

        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
            FALSE, pid);
        if (hProc)
        {
            wchar_t path[MAX_PATH];
            DWORD size = MAX_PATH;
            QueryFullProcessImageNameW(hProc, 0, path, &size);
            // path를 서버에 리포트
            CloseHandle(hProc);
        }
    }
};
```

---

## 6. 안티치트 서비스 아키텍처

```
게임 프로세스 내부:
  ┌──────────────────────────────────────┐
  │ WintersLOL.exe                       │
  │                                      │
  │  ┌──────────────────────┐            │
  │  │ AntiCheat Module     │            │
  │  │  - 디버거 탐지       │            │
  │  │  - 코드 무결성       │──────┐     │
  │  │  - 모듈 스캔         │      │     │
  │  │  - IAT 검증          │      │     │
  │  │  - 오버레이 탐지     │      │     │
  │  │  - 메모리 암호화     │      │     │
  │  └──────────────────────┘      │     │
  │                                │     │
  └────────────────────────────────│─────┘
                                   │
                            암호화된 리포트
                                   │
                                   ↓
                          ┌────────────────┐
                          │  서버           │
                          │  - 리포트 수집  │
                          │  - 패턴 분석    │
                          │  - 밴 결정      │
                          └────────────────┘

보고 원칙:
  1. 즉시 차단하지 않음 → 데이터 수집
  2. 서버에서 밴 웨이브로 일괄 처리
  3. 치트 개발자가 "어떤 탐지에 걸렸는지" 모르게 함
  4. 오탐 시 서버에서 필터링 가능
```
