# Part 4: 커널 레벨 안티치트 — Vanguard 스타일

> Ring 0에서 동작하는 커널 드라이버 안티치트.
> 유저모드 치트가 절대 우회할 수 없는 최종 방어선.

---

## 1. 커널 드라이버 기본 구조

### 1.1 WDM (Windows Driver Model) 드라이버

```cpp
// WintersAC.sys — 안티치트 커널 드라이버 엔트리 포인트

#include <ntddk.h>

// 전역 상태
PDEVICE_OBJECT g_pDeviceObject = nullptr;
UNICODE_STRING g_DeviceName = RTL_CONSTANT_STRING(L"\\Device\\WintersAC");
UNICODE_STRING g_SymLink = RTL_CONSTANT_STRING(L"\\??\\WintersAC");

// 보호 대상 게임 프로세스 ID
volatile HANDLE g_GameProcessId = nullptr;

// 드라이버 언로드
void DriverUnload(PDRIVER_OBJECT pDriverObject)
{
    // 콜백 해제
    UnregisterCallbacks();

    // 심볼릭 링크 및 디바이스 삭제
    IoDeleteSymbolicLink(&g_SymLink);
    IoDeleteDevice(g_pDeviceObject);
    DbgPrint("[WintersAC] Driver unloaded\n");
}

// 드라이버 진입점
extern "C" NTSTATUS DriverEntry(
    PDRIVER_OBJECT pDriverObject,
    PUNICODE_STRING pRegistryPath)
{
    UNREFERENCED_PARAMETER(pRegistryPath);

    DbgPrint("[WintersAC] Driver loading...\n");

    pDriverObject->DriverUnload = DriverUnload;

    // 디바이스 오브젝트 생성 (유저모드와 통신용)
    NTSTATUS status = IoCreateDevice(
        pDriverObject,
        0,
        &g_DeviceName,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &g_pDeviceObject);

    if (!NT_SUCCESS(status)) return status;

    // 심볼릭 링크 생성 (유저모드에서 접근 가능하게)
    status = IoCreateSymbolicLink(&g_SymLink, &g_DeviceName);
    if (!NT_SUCCESS(status))
    {
        IoDeleteDevice(g_pDeviceObject);
        return status;
    }

    // IOCTL 디스패치 설정
    pDriverObject->MajorFunction[IRP_MJ_CREATE] = DispatchCreateClose;
    pDriverObject->MajorFunction[IRP_MJ_CLOSE]  = DispatchCreateClose;
    pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchIOCTL;

    // 보호 콜백 등록
    RegisterProcessProtection();
    RegisterImageLoadNotify();

    DbgPrint("[WintersAC] Driver loaded successfully\n");
    return STATUS_SUCCESS;
}
```

### 1.2 IOCTL — 유저모드↔커널 통신

```cpp
// IOCTL 코드 정의
#define IOCTL_AC_REGISTER_GAME  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, \
    METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_AC_GET_STATUS     CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, \
    METHOD_BUFFERED, FILE_ANY_ACCESS)

struct AC_REGISTER_INPUT
{
    ULONG ProcessId;     // 보호할 게임 PID
    ULONG64 Checksum;    // 인증 체크섬
};

struct AC_STATUS_OUTPUT
{
    ULONG  CheatDetected;
    ULONG  DetectionType;
    ULONG  SuspiciousPID;
};

NTSTATUS DispatchIOCTL(PDEVICE_OBJECT pDevice, PIRP pIrp)
{
    auto* pStack = IoGetCurrentIrpStackLocation(pIrp);
    ULONG ioctl = pStack->Parameters.DeviceIoControl.IoControlCode;
    PVOID pBuffer = pIrp->AssociatedIrp.SystemBuffer;
    ULONG inSize = pStack->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outSize = pStack->Parameters.DeviceIoControl.OutputBufferLength;
    ULONG info = 0;
    NTSTATUS status = STATUS_SUCCESS;

    switch (ioctl)
    {
    case IOCTL_AC_REGISTER_GAME:
    {
        if (inSize < sizeof(AC_REGISTER_INPUT))
        {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        auto* pInput = reinterpret_cast<AC_REGISTER_INPUT*>(pBuffer);

        // 체크섬 검증 (유저모드 서비스가 진짜 우리 것인지 확인)
        if (!VerifyChecksum(pInput->Checksum))
        {
            status = STATUS_ACCESS_DENIED;
            break;
        }

        g_GameProcessId = ULongToHandle(pInput->ProcessId);
        DbgPrint("[WintersAC] Protecting PID: %u\n", pInput->ProcessId);
        break;
    }

    case IOCTL_AC_GET_STATUS:
    {
        if (outSize < sizeof(AC_STATUS_OUTPUT))
        {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        auto* pOutput = reinterpret_cast<AC_STATUS_OUTPUT*>(pBuffer);
        GetDetectionStatus(pOutput);
        info = sizeof(AC_STATUS_OUTPUT);
        break;
    }
    }

    pIrp->IoStatus.Status = status;
    pIrp->IoStatus.Information = info;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);
    return status;
}
```

---

## 2. ObRegisterCallbacks — 핸들 보호

### 2.1 프로세스 핸들 필터링

```cpp
// 게임 프로세스에 대한 핸들 오픈을 가로채서 위험한 권한 제거

PVOID g_ObCallbackHandle = nullptr;

OB_PREOP_CALLBACK_STATUS OnPreOpenProcess(
    PVOID /*RegistrationContext*/,
    POB_PRE_OPERATION_INFORMATION pInfo)
{
    // 대상이 게임 프로세스인지 확인
    if (pInfo->ObjectType != *PsProcessType)
        return OB_PREOP_SUCCESS;

    PEPROCESS pTarget = reinterpret_cast<PEPROCESS>(pInfo->Object);
    HANDLE targetPID = PsGetProcessId(pTarget);

    if (targetPID != g_GameProcessId)
        return OB_PREOP_SUCCESS;  // 게임이 아니면 패스

    // 요청자가 게임 자신이면 허용
    PEPROCESS pRequester = PsGetCurrentProcess();
    if (PsGetProcessId(pRequester) == g_GameProcessId)
        return OB_PREOP_SUCCESS;

    // 요청자가 시스템 프로세스면 허용
    if (PsGetProcessId(pRequester) == ULongToHandle(4))
        return OB_PREOP_SUCCESS;

    // 그 외: 위험한 접근 권한 제거!
    if (pInfo->Operation == OB_OPERATION_HANDLE_CREATE)
    {
        pInfo->Parameters->CreateHandleInformation.DesiredAccess &=
            ~(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION |
              PROCESS_CREATE_THREAD | PROCESS_DUP_HANDLE);
    }
    else if (pInfo->Operation == OB_OPERATION_HANDLE_DUPLICATE)
    {
        pInfo->Parameters->DuplicateHandleInformation.DesiredAccess &=
            ~(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION |
              PROCESS_CREATE_THREAD | PROCESS_DUP_HANDLE);
    }

    // 로그 기록
    UCHAR processName[16] = {};
    memcpy(processName, PsGetProcessImageFileName(pRequester), 15);
    DbgPrint("[WintersAC] Blocked access from %s (PID %u) to game\n",
        processName, HandleToULong(PsGetProcessId(pRequester)));

    return OB_PREOP_SUCCESS;
}

// 스레드 핸들도 동일하게 보호
OB_PREOP_CALLBACK_STATUS OnPreOpenThread(
    PVOID /*RegistrationContext*/,
    POB_PRE_OPERATION_INFORMATION pInfo)
{
    if (pInfo->ObjectType != *PsThreadType)
        return OB_PREOP_SUCCESS;

    PETHREAD pThread = reinterpret_cast<PETHREAD>(pInfo->Object);
    PEPROCESS pOwner = IoThreadToProcess(pThread);
    HANDLE ownerPID = PsGetProcessId(pOwner);

    if (ownerPID != g_GameProcessId)
        return OB_PREOP_SUCCESS;

    PEPROCESS pRequester = PsGetCurrentProcess();
    if (PsGetProcessId(pRequester) == g_GameProcessId)
        return OB_PREOP_SUCCESS;

    // 스레드 하이재킹 방지: THREAD_SET_CONTEXT 제거
    if (pInfo->Operation == OB_OPERATION_HANDLE_CREATE)
    {
        pInfo->Parameters->CreateHandleInformation.DesiredAccess &=
            ~(THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME |
              THREAD_TERMINATE | THREAD_GET_CONTEXT);
    }

    return OB_PREOP_SUCCESS;
}

NTSTATUS RegisterProcessProtection()
{
    OB_CALLBACK_REGISTRATION cbReg = {};
    OB_OPERATION_REGISTRATION opReg[2] = {};

    // 프로세스 핸들 보호
    opReg[0].ObjectType = PsProcessType;
    opReg[0].Operations = OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE;
    opReg[0].PreOperation = OnPreOpenProcess;

    // 스레드 핸들 보호
    opReg[1].ObjectType = PsThreadType;
    opReg[1].Operations = OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE;
    opReg[1].PreOperation = OnPreOpenThread;

    UNICODE_STRING altitude = RTL_CONSTANT_STRING(L"321000");
    cbReg.Version = OB_FLT_REGISTRATION_VERSION;
    cbReg.OperationRegistrationCount = 2;
    cbReg.Altitude = altitude;
    cbReg.RegistrationContext = nullptr;
    cbReg.OperationRegistration = opReg;

    return ObRegisterCallbacks(&cbReg, &g_ObCallbackHandle);
}
```

**효과**: 치트가 `OpenProcess(PROCESS_VM_READ, ...)` 를 호출해도, 커널 콜백이 `PROCESS_VM_READ` 플래그를 제거하므로 실제로는 읽기 권한이 없는 핸들을 받게 된다. → `ReadProcessMemory` 실패.

---

## 3. 프로세스/이미지 로드 감시

### 3.1 프로세스 생성 감시

```cpp
void OnProcessNotify(
    PEPROCESS pProcess,
    HANDLE processId,
    PPS_CREATE_NOTIFY_INFO pCreateInfo)
{
    if (pCreateInfo == nullptr)
    {
        // 프로세스 종료
        if (processId == g_GameProcessId)
        {
            g_GameProcessId = nullptr;
            DbgPrint("[WintersAC] Game process terminated\n");
        }
        return;
    }

    // 프로세스 생성
    if (pCreateInfo->ImageFileName)
    {
        // 알려진 치트 프로세스 차단
        if (IsBlacklistedImage(pCreateInfo->ImageFileName))
        {
            pCreateInfo->CreationStatus = STATUS_ACCESS_DENIED;
            DbgPrint("[WintersAC] Blocked cheat process: %wZ\n",
                pCreateInfo->ImageFileName);
        }
    }
}

// 등록
PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, FALSE);
```

### 3.2 DLL 로드 감시

```cpp
void OnImageLoadNotify(
    PUNICODE_STRING pFullImageName,
    HANDLE processId,
    PIMAGE_INFO pImageInfo)
{
    // 게임 프로세스에 로드되는 DLL만 감시
    if (processId != g_GameProcessId)
        return;

    if (pFullImageName == nullptr)
        return;

    // 시스템 DLL 화이트리스트
    if (IsSystemDLL(pFullImageName))
        return;

    // 서명 검증
    if (!VerifyImageSignature(pFullImageName))
    {
        DbgPrint("[WintersAC] Unsigned DLL in game: %wZ\n", pFullImageName);
        // 서버에 리포트 (즉시 차단보다 데이터 수집 우선)
        QueueDetectionReport(DetectionType::UnsignedDLL, processId, pFullImageName);
    }

    // 알려진 치트 DLL 해시 체크
    if (pImageInfo->ImageBase)
    {
        ULONG hash = HashImageSection(pImageInfo->ImageBase, pImageInfo->ImageSize);
        if (IsBlacklistedHash(hash))
        {
            DbgPrint("[WintersAC] Known cheat DLL detected: %wZ\n", pFullImageName);
            QueueDetectionReport(DetectionType::KnownCheatDLL, processId, pFullImageName);
        }
    }
}

// 등록
PsSetLoadImageNotifyRoutine(OnImageLoadNotify);
```

---

## 4. 미니필터 드라이버 — 파일 시스템 감시

```cpp
// 치트 파일의 읽기/실행을 감시하는 미니필터

const FLT_OPERATION_REGISTRATION g_Callbacks[] = {
    { IRP_MJ_CREATE, 0, PreCreateCallback, nullptr },
    { IRP_MJ_OPERATION_END }
};

FLT_PREOP_CALLBACK_STATUS PreCreateCallback(
    PFLT_CALLBACK_DATA pData,
    PCFLT_RELATED_OBJECTS pFltObjects,
    PVOID* /*CompletionContext*/)
{
    // 게임 프로세스의 파일 접근만 감시
    HANDLE pid = PsGetCurrentProcessId();
    if (pid != g_GameProcessId)
        return FLT_PREOP_SUCCESS_NO_CALLBACK;

    // 파일 이름 획득
    PFLT_FILE_NAME_INFORMATION pNameInfo;
    NTSTATUS status = FltGetFileNameInformation(pData,
        FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT,
        &pNameInfo);

    if (!NT_SUCCESS(status))
        return FLT_PREOP_SUCCESS_NO_CALLBACK;

    FltParseFileNameInformation(pNameInfo);

    // .dll 파일 로드 감시
    if (EndsWithW(pNameInfo->Extension, L"dll") ||
        EndsWithW(pNameInfo->Extension, L"sys"))
    {
        // 화이트리스트에 없는 DLL/SYS 접근 로깅
        if (!IsWhitelistedPath(&pNameInfo->Name))
        {
            DbgPrint("[WintersAC] Suspicious file access: %wZ\n",
                &pNameInfo->Name);
        }
    }

    FltReleaseFileNameInformation(pNameInfo);
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}
```

---

## 5. Vanguard 아키텍처 분석

### 5.1 Riot Vanguard의 구조

```
시스템 부팅 시:
  ┌──────────────────────────────────────────┐
  │ vgk.sys (커널 드라이버)                   │
  │ - ELAM(Early Launch Anti-Malware)으로 시작│
  │ - 부팅 초기부터 모든 드라이버 로드 감시   │
  │ - ObRegisterCallbacks (핸들 보호)         │
  │ - PsSetLoadImageNotifyRoutine (DLL 감시)  │
  │ - 커널 무결성 검증                        │
  └──────────┬───────────────────────────────┘
             │ IOCTL 통신
  ┌──────────▼───────────────────────────────┐
  │ vgtray.exe (유저모드 서비스, 시스템 트레이)│
  │ - 항시 상주 (PC 부팅~종료)                │
  │ - 커널 드라이버와 통신                     │
  │ - 탐지 결과를 Riot 서버에 전송            │
  │ - UI 트레이 아이콘                        │
  └──────────┬───────────────────────────────┘
             │ 프로세스 보호
  ┌──────────▼───────────────────────────────┐
  │ VALORANT.exe (게임)                       │
  │ - 인게임 안티치트 모듈                     │
  │ - 유저모드 탐지 (디버거, 훅, 모듈 등)     │
  │ - Heartbeat → 서버에 무결성 보고          │
  └──────────────────────────────────────────┘
```

### 5.2 Vanguard의 핵심 방어 메커니즘

```
1. 부팅 시간 드라이버 (ELAM):
   Windows 부팅 과정에서 가장 먼저 로드되는 드라이버 중 하나
   → 치트 드라이버보다 먼저 실행되어 선점
   → 모든 드라이버 로드를 감시 가능

2. 항시 상주:
   게임 실행 여부와 무관하게 PC 부팅부터 동작
   → 치트 드라이버가 게임 실행 전에 로드되는 것을 탐지
   → "게임 시작 전에 치트 활성화" 시나리오 차단

3. 취약 드라이버 차단 (BYOVD 방어):
   알려진 취약 드라이버(capcom.sys, cpuz.sys 등)의 로드 차단
   → 치트가 커널 코드 실행을 위해 악용하는 경로 차단

4. 커널 메모리 스캔:
   주기적으로 커널 메모리를 스캔하여 의심 코드 탐지
   → SSDT 훅, DKOM 등 커널 레벨 치트 탐지

5. 하드웨어 ID 수집:
   CPU, GPU, 마더보드, MAC 주소 등 수집
   → 밴된 유저가 새 계정으로 돌아와도 하드웨어 밴 가능
   → HWID 스푸핑 탐지 (레지스트리 vs 실제 하드웨어 비교)
```

### 5.3 Vanguard의 탐지 우회 방지

```
치트 개발자의 우회 시도 vs Vanguard 대응:

1. ReadProcessMemory → ObRegisterCallbacks가 VM_READ 제거
                       → 실패

2. Manual Mapping → PsSetLoadImageNotifyRoutine 미발생이지만
                    → VAD 트리 스캔으로 미등록 실행 영역 탐지
                    → 주기적 메모리 스캔

3. 커널 드라이버 치트 → ELAM이 로드 차단
                       → 취약 드라이버 블랙리스트
                       → PatchGuard가 커널 변조 방지

4. DMA (하드웨어) 치트 → PCIe 디바이스를 통한 물리 메모리 직접 접근
                        → Vanguard는 부분적으로만 탐지 가능
                        → 서버 사이드 행동 분석으로 보완

5. 하이퍼바이저 치트 → 가상화 기반으로 Vanguard 아래에서 동작
                      → Vanguard가 하이퍼바이저 존재 여부 탐지 시도
                      → CPUID 리프, timing discrepancy 등
```

---

## 6. 커널 무결성 검증

### 6.1 SSDT 무결성

```cpp
// SSDT(System Service Descriptor Table) 변조 탐지
// PatchGuard가 보호하지만, 추가 검증으로 이중 방어

void VerifySSDTIntegrity()
{
    // ntoskrnl.exe의 디스크 이미지와 메모리 비교
    // SSDT의 각 엔트리가 ntoskrnl.exe 범위 내인지 확인

    PVOID ntBase = GetKernelModuleBase(L"ntoskrnl.exe");
    ULONG ntSize = GetKernelModuleSize(L"ntoskrnl.exe");

    // KeServiceDescriptorTable에서 SSDT 베이스 주소 획득
    // 각 엔트리의 대상 주소가 ntoskrnl 범위 내인지 검증

    // 참고: 실제 구현은 복잡 (SSDT는 상대 오프셋 테이블)
    // x64에서 SSDT[i] = SSDTBase + (SSDTBase[i] >> 4)
}
```

### 6.2 콜백 목록 보호

```cpp
// 안티치트의 콜백이 제거되었는지 감시

// 콜백 등록 시 핸들 보관
PVOID g_ObHandle = nullptr;
LARGE_INTEGER g_ObCookie;

void VerifyCallbacksAlive()
{
    // ObRegisterCallbacks의 핸들이 여전히 유효한지 확인
    // 주기적으로 콜백 목록을 스캔하여 자신의 콜백이 존재하는지 검증

    // 치트가 커널에서 콜백을 제거(unregister)하면:
    // → 이 검증에서 탐지됨
    // → 드라이버 재등록 또는 탐지 리포트
}
```

---

## 7. 하이퍼바이저 기반 보호 (참고)

```
VT-x (Intel Virtualization Technology):

  ┌─────────────────────┐
  │ VMX Root Mode       │  ← 하이퍼바이저 (Ring -1)
  │ (안티치트 하이퍼바이저)│
  ├─────────────────────┤
  │ VMX Non-Root Mode   │  ← 게스트 OS (기존 Ring 0~3)
  │ ┌─────────────────┐ │
  │ │ Ring 0 (커널)   │ │
  │ │ Ring 3 (유저)   │ │
  │ └─────────────────┘ │
  └─────────────────────┘

EPT (Extended Page Tables) 보호:
  - 물리 메모리 접근을 하이퍼바이저가 제어
  - 게임 메모리 페이지를 Execute-Only로 설정
    → 읽기 시도 시 EPT Violation → 하이퍼바이저가 가로챔
    → 치트의 메모리 읽기를 물리적으로 차단

MSR 훅 감시:
  - SYSCALL의 진입점 (IA32_LSTAR MSR)이 변조되었는지 감시
  - 루트킷이 시스템 콜을 가로채는 것 방지

한계:
  - 구현 극도로 복잡
  - BSOD 위험 높음
  - 다른 하이퍼바이저(Hyper-V)와 충돌 가능성
  - Winters Engine에서는 Level 5 (최종 단계)에서 연구 목적으로만 검토
```
