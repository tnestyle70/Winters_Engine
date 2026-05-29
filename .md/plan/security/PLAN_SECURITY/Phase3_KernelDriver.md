# Phase 3: 커널 레벨 안티치트 — 구현 계획서

> **목표**: WDK 커널 드라이버로 프로세스 핸들 보호, DLL/드라이버 로드 감시
> **위치**: `AntiCheat/Driver/` (별도 WDK 프로젝트)
> **빌드**: WDK (Windows Driver Kit) — Engine/Client와 별도 .sln
> **서명**: 개발 중 테스트 서명, 릴리스 시 EV 인증서

---

## 프로젝트 구조

```
AntiCheat/
├── Driver/                              ← WDK 커널 드라이버 프로젝트
│   ├── WintersAC.vcxproj               ← WDK 프로젝트 파일
│   ├── WintersAC.inf                   ← 드라이버 설치 정보
│   ├── WintersAC.h                     ← 공용 헤더 (IOCTL 정의, 공유 구조체)
│   ├── main.cpp                        ← DriverEntry + DriverUnload
│   ├── Communication.h                 ← IOCTL 디스패치
│   ├── Communication.cpp               ← 구현
│   ├── ProcessProtection.h             ← ObRegisterCallbacks
│   ├── ProcessProtection.cpp           ← 구현
│   ├── ImageMonitor.h                  ← PsSetLoadImageNotifyRoutine
│   ├── ImageMonitor.cpp                ← 구현
│   ├── ProcessMonitor.h                ← PsSetCreateProcessNotifyRoutine
│   └── ProcessMonitor.cpp              ← 구현
│
├── Service/                             ← 유저모드 서비스 (드라이버 로드/통신)
│   ├── WintersACService.vcxproj
│   ├── ServiceMain.cpp                 ← Windows Service 엔트리
│   ├── CDriverLoader.h                 ← 드라이버 설치/시작/중지
│   ├── CDriverLoader.cpp
│   ├── CDriverComm.h                   ← IOCTL 통신 래퍼
│   └── CDriverComm.cpp
│
├── Shared/                              ← 드라이버↔서비스 공유
│   └── WintersACShared.h               ← IOCTL 코드, 구조체 정의
```

---

## 파일 목록 (총 16파일)

| # | 경로 | 유형 | 설명 |
|---|------|------|------|
| 1 | `AntiCheat/Shared/WintersACShared.h` | 공유 | IOCTL 코드 + 통신 구조체 |
| 2 | `AntiCheat/Driver/main.cpp` | 커널 | DriverEntry, Unload |
| 3 | `AntiCheat/Driver/WintersAC.h` | 커널 | 드라이버 전역 상태 |
| 4 | `AntiCheat/Driver/Communication.h` | 커널 | IOCTL 핸들러 |
| 5 | `AntiCheat/Driver/Communication.cpp` | 커널 | 구현 |
| 6 | `AntiCheat/Driver/ProcessProtection.h` | 커널 | ObRegisterCallbacks |
| 7 | `AntiCheat/Driver/ProcessProtection.cpp` | 커널 | 구현 |
| 8 | `AntiCheat/Driver/ImageMonitor.h` | 커널 | DLL 로드 감시 |
| 9 | `AntiCheat/Driver/ImageMonitor.cpp` | 커널 | 구현 |
| 10 | `AntiCheat/Driver/ProcessMonitor.h` | 커널 | 프로세스 생성 감시 |
| 11 | `AntiCheat/Driver/ProcessMonitor.cpp` | 커널 | 구현 |
| 12 | `AntiCheat/Driver/WintersAC.inf` | 설치 | 드라이버 INF |
| 13 | `AntiCheat/Service/ServiceMain.cpp` | 유저 | 서비스 엔트리 |
| 14 | `AntiCheat/Service/CDriverLoader.h` | 유저 | 드라이버 로드/언로드 |
| 15 | `AntiCheat/Service/CDriverLoader.cpp` | 유저 | 구현 |
| 16 | `AntiCheat/Service/CDriverComm.h/.cpp` | 유저 | IOCTL 통신 |

---

## 1. WintersACShared.h (드라이버↔서비스 공유)

**경로**: `AntiCheat/Shared/WintersACShared.h`

```cpp
#pragma once

// IOCTL 코드 정의
#define WINTERS_AC_DEVICE_TYPE  0x8000

#define IOCTL_AC_REGISTER_GAME \
    CTL_CODE(WINTERS_AC_DEVICE_TYPE, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_AC_UNREGISTER_GAME \
    CTL_CODE(WINTERS_AC_DEVICE_TYPE, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_AC_GET_DETECTION \
    CTL_CODE(WINTERS_AC_DEVICE_TYPE, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_AC_GET_STATUS \
    CTL_CODE(WINTERS_AC_DEVICE_TYPE, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)

// 탐지 유형 (커널 레벨)
enum KERNEL_DETECTION_TYPE
{
    KD_NONE                  = 0,
    KD_HANDLE_ACCESS         = 1,   // 외부 프로세스가 게임 핸들 획득 시도
    KD_UNSIGNED_DLL          = 2,   // 미서명 DLL 로드
    KD_BLACKLISTED_DLL       = 3,   // 블랙리스트 DLL 로드
    KD_SUSPICIOUS_PROCESS    = 4,   // 의심 프로세스 감지
    KD_BLACKLISTED_DRIVER    = 5,   // 취약 드라이버 로드 시도
    KD_THREAD_HIJACK_ATTEMPT = 6,   // 스레드 핸들 위험 접근
};

// 게임 등록 요청
#pragma pack(push, 1)
struct AC_REGISTER_INPUT
{
    unsigned long ProcessId;
    unsigned long long AuthToken;   // 서비스↔드라이버 인증
};

struct AC_DETECTION_OUTPUT
{
    unsigned long  Type;            // KERNEL_DETECTION_TYPE
    unsigned long  SourcePID;       // 위반 프로세스 PID
    unsigned long  TargetPID;       // 대상 프로세스 PID
    unsigned long  AccessMask;      // 요청된 접근 권한
    wchar_t        SourceName[64];  // 위반 프로세스 이름
    wchar_t        ImagePath[260];  // DLL/드라이버 경로 (해당 시)
};

struct AC_STATUS_OUTPUT
{
    unsigned long GameProtected;    // 게임 보호 중 여부
    unsigned long GamePID;          // 보호 중인 PID
    unsigned long DetectionCount;   // 누적 탐지 수
    unsigned long BlockedHandles;   // 차단된 핸들 수
    unsigned long BlockedModules;   // 차단된 모듈 수
};
#pragma pack(pop)

// 디바이스 이름
#define WINTERS_AC_DEVICE_NAME  L"\\Device\\WintersAC"
#define WINTERS_AC_SYMLINK      L"\\??\\WintersAC"
#define WINTERS_AC_USERMODE     L"\\\\.\\WintersAC"
```

---

## 2. ProcessProtection.h / .cpp (핵심)

**경로**: `AntiCheat/Driver/ProcessProtection.h`

```cpp
#pragma once
#include <ntddk.h>

// ObRegisterCallbacks 기반 프로세스/스레드 핸들 보호
NTSTATUS RegisterHandleProtection();
void     UnregisterHandleProtection();
```

**경로**: `AntiCheat/Driver/ProcessProtection.cpp`

```cpp
#include "ProcessProtection.h"
#include "WintersAC.h"

static PVOID g_ObCallbackHandle = nullptr;

// 프로세스 핸들 오픈 사전 콜백
OB_PREOP_CALLBACK_STATUS OnPreOpenProcess(
    PVOID /*RegCtx*/,
    POB_PRE_OPERATION_INFORMATION pInfo)
{
    if (pInfo->ObjectType != *PsProcessType)
        return OB_PREOP_SUCCESS;

    // 보호 대상 게임 프로세스가 아니면 패스
    PEPROCESS pTarget = (PEPROCESS)pInfo->Object;
    HANDLE targetPID = PsGetProcessId(pTarget);
    if (targetPID != g_DriverState.GamePID)
        return OB_PREOP_SUCCESS;

    // 게임 자기 자신이면 허용
    PEPROCESS pCurrent = PsGetCurrentProcess();
    HANDLE currentPID = PsGetProcessId(pCurrent);
    if (currentPID == g_DriverState.GamePID)
        return OB_PREOP_SUCCESS;

    // 안티치트 서비스면 허용
    if (currentPID == g_DriverState.ServicePID)
        return OB_PREOP_SUCCESS;

    // 시스템 프로세스(PID 4)면 허용
    if (HandleToULong(currentPID) == 4)
        return OB_PREOP_SUCCESS;

    // 위험한 접근 권한 제거
    ACCESS_MASK dangerousBits =
        PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION |
        PROCESS_CREATE_THREAD | PROCESS_DUP_HANDLE |
        PROCESS_SUSPEND_RESUME | PROCESS_TERMINATE;

    if (pInfo->Operation == OB_OPERATION_HANDLE_CREATE)
    {
        ACCESS_MASK original = pInfo->Parameters
            ->CreateHandleInformation.DesiredAccess;
        ACCESS_MASK stripped = original & dangerousBits;

        if (stripped != 0)
        {
            pInfo->Parameters->CreateHandleInformation.DesiredAccess
                &= ~dangerousBits;

            // 탐지 기록
            RecordKernelDetection(
                KD_HANDLE_ACCESS,
                currentPID,
                targetPID,
                original);

            InterlockedIncrement(&g_DriverState.BlockedHandles);
        }
    }
    else if (pInfo->Operation == OB_OPERATION_HANDLE_DUPLICATE)
    {
        pInfo->Parameters->DuplicateHandleInformation.DesiredAccess
            &= ~dangerousBits;
    }

    return OB_PREOP_SUCCESS;
}

// 스레드 핸들 오픈 사전 콜백
OB_PREOP_CALLBACK_STATUS OnPreOpenThread(
    PVOID /*RegCtx*/,
    POB_PRE_OPERATION_INFORMATION pInfo)
{
    if (pInfo->ObjectType != *PsThreadType)
        return OB_PREOP_SUCCESS;

    PETHREAD pThread = (PETHREAD)pInfo->Object;
    PEPROCESS pOwner = IoThreadToProcess(pThread);
    if (PsGetProcessId(pOwner) != g_DriverState.GamePID)
        return OB_PREOP_SUCCESS;

    HANDLE currentPID = PsGetProcessId(PsGetCurrentProcess());
    if (currentPID == g_DriverState.GamePID ||
        currentPID == g_DriverState.ServicePID ||
        HandleToULong(currentPID) == 4)
        return OB_PREOP_SUCCESS;

    // 스레드 하이재킹 방지
    ACCESS_MASK threadDangerous =
        THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME |
        THREAD_TERMINATE | THREAD_GET_CONTEXT;

    if (pInfo->Operation == OB_OPERATION_HANDLE_CREATE)
    {
        pInfo->Parameters->CreateHandleInformation.DesiredAccess
            &= ~threadDangerous;
    }

    return OB_PREOP_SUCCESS;
}

NTSTATUS RegisterHandleProtection()
{
    OB_OPERATION_REGISTRATION opReg[2] = {};

    opReg[0].ObjectType = PsProcessType;
    opReg[0].Operations = OB_OPERATION_HANDLE_CREATE |
                          OB_OPERATION_HANDLE_DUPLICATE;
    opReg[0].PreOperation = OnPreOpenProcess;

    opReg[1].ObjectType = PsThreadType;
    opReg[1].Operations = OB_OPERATION_HANDLE_CREATE |
                          OB_OPERATION_HANDLE_DUPLICATE;
    opReg[1].PreOperation = OnPreOpenThread;

    UNICODE_STRING altitude = RTL_CONSTANT_STRING(L"321000");

    OB_CALLBACK_REGISTRATION cbReg = {};
    cbReg.Version = OB_FLT_REGISTRATION_VERSION;
    cbReg.OperationRegistrationCount = 2;
    cbReg.Altitude = altitude;
    cbReg.RegistrationContext = nullptr;
    cbReg.OperationRegistration = opReg;

    return ObRegisterCallbacks(&cbReg, &g_ObCallbackHandle);
}

void UnregisterHandleProtection()
{
    if (g_ObCallbackHandle)
    {
        ObUnRegisterCallbacks(g_ObCallbackHandle);
        g_ObCallbackHandle = nullptr;
    }
}
```

---

## 3. ImageMonitor.h / .cpp

**경로**: `AntiCheat/Driver/ImageMonitor.h`

```cpp
#pragma once
#include <ntddk.h>

NTSTATUS RegisterImageLoadMonitor();
void     UnregisterImageLoadMonitor();
```

**경로**: `AntiCheat/Driver/ImageMonitor.cpp`

```cpp
#include "ImageMonitor.h"
#include "WintersAC.h"

void OnImageLoad(
    PUNICODE_STRING pFullImageName,
    HANDLE processId,
    PIMAGE_INFO pImageInfo)
{
    // 게임 프로세스에 로드되는 이미지만 감시
    if (processId != g_DriverState.GamePID)
        return;

    if (!pFullImageName || !pFullImageName->Buffer)
        return;

    // 시스템 DLL 화이트리스트 (간략)
    if (IsWhitelistedPath(pFullImageName))
        return;

    // 커널 이미지(드라이버)는 별도 처리
    if (pImageInfo->SystemModeImage)
    {
        // 취약 드라이버 블랙리스트 체크
        if (IsBlacklistedDriver(pFullImageName))
        {
            RecordKernelDetection(
                KD_BLACKLISTED_DRIVER,
                processId,
                processId,
                0);
        }
        return;
    }

    // 유저모드 DLL — 탐지 기록 (서비스에서 수집)
    RecordKernelDetection(
        KD_UNSIGNED_DLL,
        processId,
        processId,
        0);

    InterlockedIncrement(&g_DriverState.BlockedModules);
}

NTSTATUS RegisterImageLoadMonitor()
{
    return PsSetLoadImageNotifyRoutine(OnImageLoad);
}

void UnregisterImageLoadMonitor()
{
    PsRemoveLoadImageNotifyRoutine(OnImageLoad);
}
```

---

## 4. CDriverLoader.h / .cpp (유저모드 서비스)

**경로**: `AntiCheat/Service/CDriverLoader.h`

```cpp
#pragma once
#include <Windows.h>
#include <string>
#include <memory>

// 커널 드라이버 로드/언로드 관리
class CDriverLoader
{
public:
    ~CDriverLoader();

    static std::unique_ptr<CDriverLoader> Create(
        const std::wstring& driverPath,
        const std::wstring& serviceName = L"WintersAC");

    bool Install();
    bool Start();
    bool Stop();
    bool Uninstall();

    bool IsRunning() const;

private:
    CDriverLoader() = default;

    std::wstring m_DriverPath;
    std::wstring m_ServiceName;
    SC_HANDLE    m_hSCManager = nullptr;
    SC_HANDLE    m_hService   = nullptr;
};
```

**경로**: `AntiCheat/Service/CDriverLoader.cpp`

```cpp
#include "CDriverLoader.h"

CDriverLoader::~CDriverLoader()
{
    if (m_hService)  CloseServiceHandle(m_hService);
    if (m_hSCManager) CloseServiceHandle(m_hSCManager);
}

std::unique_ptr<CDriverLoader> CDriverLoader::Create(
    const std::wstring& driverPath,
    const std::wstring& serviceName)
{
    auto loader = std::unique_ptr<CDriverLoader>(new CDriverLoader());
    loader->m_DriverPath = driverPath;
    loader->m_ServiceName = serviceName;

    loader->m_hSCManager = OpenSCManagerW(
        nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!loader->m_hSCManager)
        return nullptr;

    return loader;
}

bool CDriverLoader::Install()
{
    m_hService = CreateServiceW(
        m_hSCManager,
        m_ServiceName.c_str(),
        m_ServiceName.c_str(),
        SERVICE_ALL_ACCESS,
        SERVICE_KERNEL_DRIVER,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_NORMAL,
        m_DriverPath.c_str(),
        nullptr, nullptr, nullptr, nullptr, nullptr);

    if (!m_hService)
    {
        DWORD err = GetLastError();
        if (err == ERROR_SERVICE_EXISTS)
        {
            m_hService = OpenServiceW(
                m_hSCManager, m_ServiceName.c_str(), SERVICE_ALL_ACCESS);
            return m_hService != nullptr;
        }
        return false;
    }
    return true;
}

bool CDriverLoader::Start()
{
    if (!m_hService) return false;
    return StartServiceW(m_hService, 0, nullptr) ||
           GetLastError() == ERROR_SERVICE_ALREADY_RUNNING;
}

bool CDriverLoader::Stop()
{
    if (!m_hService) return false;
    SERVICE_STATUS status;
    return ControlService(m_hService, SERVICE_CONTROL_STOP, &status);
}

bool CDriverLoader::Uninstall()
{
    Stop();
    if (!m_hService) return false;
    return DeleteService(m_hService);
}

bool CDriverLoader::IsRunning() const
{
    if (!m_hService) return false;
    SERVICE_STATUS status;
    QueryServiceStatus(m_hService, &status);
    return status.dwCurrentState == SERVICE_RUNNING;
}
```

---

## 5. CDriverComm.h / .cpp (IOCTL 통신)

**경로**: `AntiCheat/Service/CDriverComm.h`

```cpp
#pragma once
#include "../Shared/WintersACShared.h"
#include <Windows.h>
#include <memory>

// 유저모드에서 커널 드라이버와 IOCTL 통신
class CDriverComm
{
public:
    ~CDriverComm();

    static std::unique_ptr<CDriverComm> Create();

    bool RegisterGame(unsigned long gamePID, unsigned long long authToken);
    bool UnregisterGame();
    bool GetDetection(AC_DETECTION_OUTPUT* pOutput);
    bool GetStatus(AC_STATUS_OUTPUT* pOutput);

private:
    CDriverComm() = default;
    HANDLE m_hDevice = INVALID_HANDLE_VALUE;
};
```

**경로**: `AntiCheat/Service/CDriverComm.cpp`

```cpp
#include "CDriverComm.h"

CDriverComm::~CDriverComm()
{
    if (m_hDevice != INVALID_HANDLE_VALUE)
        CloseHandle(m_hDevice);
}

std::unique_ptr<CDriverComm> CDriverComm::Create()
{
    auto comm = std::unique_ptr<CDriverComm>(new CDriverComm());
    comm->m_hDevice = CreateFileW(
        WINTERS_AC_USERMODE,
        GENERIC_READ | GENERIC_WRITE,
        0, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);

    if (comm->m_hDevice == INVALID_HANDLE_VALUE)
        return nullptr;

    return comm;
}

bool CDriverComm::RegisterGame(
    unsigned long gamePID, unsigned long long authToken)
{
    AC_REGISTER_INPUT input = { gamePID, authToken };
    DWORD returned;
    return DeviceIoControl(
        m_hDevice,
        IOCTL_AC_REGISTER_GAME,
        &input, sizeof(input),
        nullptr, 0,
        &returned, nullptr);
}

bool CDriverComm::UnregisterGame()
{
    DWORD returned;
    return DeviceIoControl(
        m_hDevice,
        IOCTL_AC_UNREGISTER_GAME,
        nullptr, 0,
        nullptr, 0,
        &returned, nullptr);
}

bool CDriverComm::GetDetection(AC_DETECTION_OUTPUT* pOutput)
{
    DWORD returned;
    return DeviceIoControl(
        m_hDevice,
        IOCTL_AC_GET_DETECTION,
        nullptr, 0,
        pOutput, sizeof(*pOutput),
        &returned, nullptr);
}

bool CDriverComm::GetStatus(AC_STATUS_OUTPUT* pOutput)
{
    DWORD returned;
    return DeviceIoControl(
        m_hDevice,
        IOCTL_AC_GET_STATUS,
        nullptr, 0,
        pOutput, sizeof(*pOutput),
        &returned, nullptr);
}
```

---

## WDK 빌드 설정 (WintersAC.vcxproj 핵심)

```xml
<PropertyGroup>
  <ConfigurationType>Driver</ConfigurationType>
  <DriverType>WDM</DriverType>
  <TargetVersion>Windows10</TargetVersion>
  <PlatformToolset>WindowsKernelModeDriver10.0</PlatformToolset>
</PropertyGroup>

<PropertyGroup Condition="'$(Configuration)'=='Debug'">
  <DriverTargetPlatform>Desktop</DriverTargetPlatform>
  <TestSign>true</TestSign>
</PropertyGroup>

<ItemDefinitionGroup>
  <ClCompile>
    <AdditionalOptions>/kernel /utf-8 %(AdditionalOptions)</AdditionalOptions>
    <PreprocessorDefinitions>_KERNEL_MODE;%(PreprocessorDefinitions)</PreprocessorDefinitions>
  </ClCompile>
</ItemDefinitionGroup>
```

---

## 테스트 환경 설정

```
1. 테스트 서명 활성화 (개발 머신):
   bcdedit /set testsigning on
   → 재부팅

2. Driver Verifier 활성화:
   verifier /standard /driver WintersAC.sys
   → BSOD 유발 버그 조기 감지

3. WinDbg 커널 디버깅:
   bcdedit /debug on
   bcdedit /dbgsettings net hostip:192.168.x.x port:50000
   → 개발 PC에서 WinDbg로 연결

4. DebugView로 DbgPrint 출력 확인
```

## Verification

```
[ ] 드라이버 로드 → DebugView에 "WintersAC loaded" 출력
[ ] IOCTL_AC_REGISTER_GAME → 게임 PID 등록 확인
[ ] 외부 프로세스가 OpenProcess(VM_READ) → 권한 제거됨 확인
[ ] ReadProcessMemory 실패 확인
[ ] 미서명 DLL 인젝션 → ImageMonitor 탐지
[ ] IOCTL_AC_GET_STATUS → BlockedHandles 카운터 증가 확인
[ ] 드라이버 언로드 → 클린 언로드, BSOD 없음
[ ] Driver Verifier 24시간 안정성 테스트
```
