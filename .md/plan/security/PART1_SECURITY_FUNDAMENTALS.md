# Part 1: 보안 기초 — Windows 보안 모델과 메모리 보호

> 안티치트를 만들려면 먼저 OS가 프로세스를 어떻게 보호하는지,
> 그 보호를 공격자가 어떻게 우회하는지 이해해야 한다.

---

## 1. Windows 보안 아키텍처

### 1.1 Ring 모델 — 커널과 유저의 경계

```
Ring 0 (커널 모드):
  ┌─────────────────────────────────────────┐
  │  ntoskrnl.exe (Windows 커널)            │
  │  HAL.dll (Hardware Abstraction Layer)   │
  │  드라이버 (.sys 파일)                    │
  │  → 모든 메모리, 모든 하드웨어 접근 가능   │
  │  → 하나의 버그 = 블루스크린(BSOD)        │
  └─────────────────────────────────────────┘
                    ↕ 시스템 콜 (SYSCALL/SYSENTER)
Ring 3 (유저 모드):
  ┌─────────────────────────────────────────┐
  │  게임 클라이언트 (WintersLOL.exe)       │
  │  치트 프로그램 (cheat.exe)              │
  │  안티치트 유저모드 서비스                 │
  │  → 자기 프로세스 메모리만 접근 가능       │
  │  → 다른 프로세스 접근 = API 호출 필요     │
  └─────────────────────────────────────────┘
```

**핵심**: 유저모드 프로그램은 아무리 관리자 권한이 있어도 커널 메모리에 직접 접근할 수 없다. 커널 모드 안티치트가 강력한 이유가 바로 이것이다 — 치트 프로그램이 Ring 3에 있는 한, Ring 0의 드라이버가 모든 것을 감시할 수 있다.

### 1.2 가상 주소 공간 격리

```
프로세스 A (게임)              프로세스 B (치트)
┌──────────────────┐          ┌──────────────────┐
│ 0x00000000       │          │ 0x00000000       │
│  .text (코드)    │          │  .text (코드)    │
│  .data (데이터)  │          │  .data (데이터)  │
│  Heap            │          │  Heap            │
│  Stack           │          │  Stack           │
│  DLL 매핑        │          │  DLL 매핑        │
│ 0x7FFFFFFFFFFF   │          │ 0x7FFFFFFFFFFF   │
├──────────────────┤          ├──────────────────┤
│ 커널 공간        │ ◄──────► │ 커널 공간        │
│ (공유, 접근 불가) │          │ (공유, 접근 불가) │
│ 0xFFFFFFFFFFFF   │          │ 0xFFFFFFFFFFFF   │
└──────────────────┘          └──────────────────┘
         │                              │
         │  ← 직접 접근 불가 →          │
         │                              │
         └── OpenProcess(PROCESS_VM_READ/WRITE) ──┘
             ReadProcessMemory / WriteProcessMemory
             → 이것을 차단하는 것이 안티치트의 핵심!
```

### 1.3 프로세스 핸들과 접근 권한

Windows에서 한 프로세스가 다른 프로세스를 조작하려면 **핸들(Handle)**이 필요하다.

```cpp
// 치트 프로그램이 게임 메모리를 읽는 과정
HANDLE hProcess = OpenProcess(
    PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION,
    FALSE,
    gameProcessID  // 게임 프로세스 ID
);

// 게임 메모리에서 체력 값 읽기
float health = 0;
ReadProcessMemory(hProcess, (LPCVOID)0x12345678, &health, sizeof(float), nullptr);

// 체력 값 변조
health = 99999.0f;
WriteProcessMemory(hProcess, (LPVOID)0x12345678, &health, sizeof(float), nullptr);
```

**접근 권한 플래그 — 치트가 요청하는 것:**

| 플래그 | 값 | 용도 |
|--------|-----|------|
| PROCESS_VM_READ | 0x0010 | 메모리 읽기 (체력, 위치 등 해킹) |
| PROCESS_VM_WRITE | 0x0020 | 메모리 쓰기 (체력 변조, 속도 핵) |
| PROCESS_VM_OPERATION | 0x0008 | VirtualAllocEx/VirtualProtectEx (코드 인젝션) |
| PROCESS_CREATE_THREAD | 0x0002 | 원격 스레드 생성 (DLL 인젝션) |
| PROCESS_QUERY_INFORMATION | 0x0400 | PEB, 모듈 목록 조회 |
| PROCESS_ALL_ACCESS | 0x1FFFFF | 모든 권한 (치트가 원하는 것) |

**안티치트의 방어**: `ObRegisterCallbacks`를 사용하면 커널에서 다른 프로세스가 게임 프로세스의 핸들을 열 때 이 권한 플래그를 **강제로 제거**할 수 있다.

---

## 2. PE(Portable Executable) 포맷

### 2.1 PE 구조 — 왜 알아야 하는가

게임 실행 파일(.exe)과 DLL(.dll)의 내부 구조를 이해해야 다음을 할 수 있다:
- 코드 섹션 무결성 검증 (패치 탐지)
- IAT(Import Address Table) 훅 탐지
- 인젝션된 DLL 탐지
- 코드 서명 검증

```
PE 파일 구조:
┌──────────────────────────────┐
│ DOS Header                    │  "MZ" 매직 넘버
│   e_lfanew → PE 시그니처 위치 │
├──────────────────────────────┤
│ PE Signature                  │  "PE\0\0" (0x50450000)
├──────────────────────────────┤
│ COFF File Header              │  Machine, NumberOfSections,
│                               │  TimeDateStamp, SizeOfOptionalHeader
├──────────────────────────────┤
│ Optional Header               │  AddressOfEntryPoint,
│                               │  ImageBase, SectionAlignment,
│                               │  DataDirectory[16]
│                               │   [0] Export Table
│                               │   [1] Import Table (IAT)
│                               │   [12] IAT
├──────────────────────────────┤
│ Section Headers               │
│   .text  (코드, RX)           │  IMAGE_SCN_MEM_READ | EXECUTE
│   .rdata (읽기전용 데이터)     │  IMAGE_SCN_MEM_READ
│   .data  (전역 변수, RW)      │  IMAGE_SCN_MEM_READ | WRITE
│   .rsrc  (리소스)             │
│   .reloc (재배치 테이블)      │
├──────────────────────────────┤
│ Section Bodies                │
│   .text  → 실행 코드          │
│   .rdata → 상수, vtable, IAT  │
│   .data  → 전역/정적 변수     │
└──────────────────────────────┘
```

### 2.2 IAT (Import Address Table) — 함수 호출의 비밀

```
게임 코드에서 MessageBoxW()를 호출할 때:

소스 코드:
  MessageBoxW(nullptr, L"Hello", L"Title", MB_OK);

컴파일 결과 (어셈블리):
  call [IAT_MessageBoxW]     ; IAT 엔트리를 통한 간접 호출
  ; IAT_MessageBoxW 주소에 실제 user32.dll!MessageBoxW의 주소가 저장됨

IAT 메모리 레이아웃:
  IAT[0] = kernel32.dll!GetModuleHandleW  → 0x7FF81234ABCD
  IAT[1] = kernel32.dll!LoadLibraryW      → 0x7FF81234EFGH
  IAT[2] = user32.dll!MessageBoxW         → 0x7FF89876WXYZ
  ...

IAT 훅 (치트):
  IAT[2] = user32.dll!MessageBoxW  → 0x치트코드주소  (원래 주소 덮어쓰기)
  → 게임이 MessageBoxW를 호출할 때마다 치트 코드가 실행됨
```

### 2.3 섹션 메모리 보호 속성

```cpp
// 각 섹션의 메모리 보호 속성을 확인하는 코드
void DumpSectionProtections(HMODULE hModule)
{
    auto* pDosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(hModule);
    auto* pNtHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(
        reinterpret_cast<BYTE*>(hModule) + pDosHeader->e_lfanew);
    auto* pSection = IMAGE_FIRST_SECTION(pNtHeaders);

    for (WORD i = 0; i < pNtHeaders->FileHeader.NumberOfSections; ++i)
    {
        char name[9] = {};
        memcpy(name, pSection[i].Name, 8);

        DWORD chars = pSection[i].Characteristics;
        printf("%s: VAddr=0x%X Size=0x%X [%s%s%s]\n",
            name,
            pSection[i].VirtualAddress,
            pSection[i].Misc.VirtualSize,
            (chars & IMAGE_SCN_MEM_READ)    ? "R" : "-",
            (chars & IMAGE_SCN_MEM_WRITE)   ? "W" : "-",
            (chars & IMAGE_SCN_MEM_EXECUTE) ? "X" : "-"
        );
    }
}

// 출력 예시:
// .text:  VAddr=0x1000 Size=0x45000 [R-X]  ← 코드, 실행 가능
// .rdata: VAddr=0x46000 Size=0x12000 [R--]  ← 읽기 전용
// .data:  VAddr=0x58000 Size=0x8000  [RW-]  ← 읽기/쓰기
```

**보안 함의**: `.text` 섹션은 R-X(읽기+실행)이어야 한다. 만약 RWX(읽기+쓰기+실행)로 변경되었다면 누군가 코드를 패치하려는 것이다.

---

## 3. 가상 메모리 보호

### 3.1 페이지 보호 속성

```
VirtualProtect로 설정 가능한 보호 속성:

PAGE_NOACCESS          (0x01) → 모든 접근 금지 (Guard Page)
PAGE_READONLY          (0x02) → 읽기만 가능
PAGE_READWRITE         (0x04) → 읽기/쓰기
PAGE_EXECUTE           (0x10) → 실행만 가능
PAGE_EXECUTE_READ      (0x20) → 실행+읽기 (일반 코드 페이지)
PAGE_EXECUTE_READWRITE (0x40) → 실행+읽기+쓰기 (위험! 코드 패치 가능)
PAGE_GUARD             (0x100)→ 접근 시 예외 발생 (한 번만)

DEP (Data Execution Prevention):
  스택/힙의 데이터를 코드로 실행하는 것을 차단
  NX bit (No-Execute) — 하드웨어 지원
  → 쉘코드 인젝션 방지의 첫 번째 방어선
```

### 3.2 ASLR (Address Space Layout Randomization)

```
ASLR 없이:
  실행할 때마다 동일한 주소
  → 치트가 "체력 주소 = 0x12345678"을 하드코딩 가능

ASLR 있을 때:
  첫 번째 실행:  ImageBase = 0x7FF6A0000000
  두 번째 실행:  ImageBase = 0x7FF6B2000000
  세 번째 실행:  ImageBase = 0x7FF69C000000
  → 매번 다른 주소 → 정적 주소 기반 치트 무력화

ASLR 적용 대상:
  - EXE ImageBase (프로세스마다 랜덤)
  - DLL ImageBase (부팅마다 랜덥)
  - 스택 시작 주소 (스레드마다 랜덤)
  - 힙 시작 주소 (프로세스마다 랜덤)
  - PEB/TEB 위치 (프로세스/스레드마다 랜덤)
```

```cpp
// ASLR이 활성화되었는지 확인 (PE 헤더)
// DllCharacteristics에 IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE (0x0040) 필요

// ASLR + High Entropy (64비트 전용): 더 넓은 범위에서 랜덤화
// IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA (0x0020)

// vcxproj 설정: /DYNAMICBASE + /HIGHENTROPYVA (기본 활성화)
```

### 3.3 Control Flow Guard (CFG)

```
간접 호출(함수 포인터, vtable)을 통한 코드 실행 공격을 방지:

공격 시나리오:
  1. 치트가 게임 메모리의 vtable 포인터를 변조
  2. 가상 함수 호출 시 치트 코드로 점프
  3. 게임 컨텍스트에서 치트 코드 실행

CFG 방어:
  1. 컴파일 시 모든 유효한 간접 호출 대상을 비트맵에 기록
  2. 런타임에 간접 호출 전 __guard_check_icall로 검증
  3. 비트맵에 없는 주소 → 프로세스 종료

  // MSVC: /guard:cf 플래그
  // 링커: /GUARD:CF
```

---

## 4. Windows 보안 메커니즘

### 4.1 토큰과 권한

```
프로세스 보안 토큰:
  ┌──────────────────────────────┐
  │ User SID (사용자 식별자)      │
  │ Group SIDs (소속 그룹)        │
  │ Privileges (권한 목록)        │
  │   SeDebugPrivilege           │ ← 다른 프로세스 디버깅 (치트에 필요!)
  │   SeLoadDriverPrivilege      │ ← 커널 드라이버 로드 (안티치트에 필요!)
  │   SeTcbPrivilege             │ ← OS의 일부로 행동
  │ Integrity Level (무결성 레벨) │
  │   Low / Medium / High / System│
  └──────────────────────────────┘

SeDebugPrivilege:
  - 이 권한이 있으면 다른 프로세스의 PROCESS_ALL_ACCESS 핸들을 열 수 있음
  - 관리자 계정에 기본 부여
  - 치트 프로그램이 가장 먼저 활성화하는 권한
  → 안티치트는 이 권한을 가진 프로세스를 감시해야 함

무결성 레벨 (Mandatory Integrity Control):
  System (시스템 서비스)     → 가장 높음
  High   (관리자 권한 실행)   → 다른 프로세스 접근 가능
  Medium (일반 프로그램)     → 기본 레벨
  Low    (샌드박스)          → 거의 아무것도 못 함

  낮은 무결성 프로세스는 높은 무결성 프로세스에 접근 불가 (No Write Up)
```

### 4.2 Protected Process Light (PPL)

```
PPL 계층 구조:
  WinTcb (Windows 핵심)          ← 최상위
  WinTcb-Light
  Windows                        ← OS 프로세스
  Windows-Light
  Antimalware-Light              ← ELAM (안티멀웨어)
  Lsa-Light                      ← 인증 서비스
  App-Light
  None                           ← 일반 프로세스 (게임, 치트)

PPL로 보호된 프로세스의 특성:
  - 더 낮은 PPL 프로세스가 핸들을 열어도 PROCESS_VM_READ/WRITE 제거됨
  - 커널 디버거로만 디버깅 가능
  - 인젝션 불가능

한계:
  - 게임이 PPL로 실행되려면 Microsoft의 서명 필요
  - Riot Vanguard는 이 대신 커널 드라이버로 유사 효과 달성
```

### 4.3 보안 디스크립터와 ACL

```cpp
// 프로세스에 보안 디스크립터를 설정하여 접근 제한
SECURITY_DESCRIPTOR sd;
InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);

// NULL DACL = 모든 접근 허용 (위험!)
// 명시적 DACL = 특정 SID만 접근 허용

EXPLICIT_ACCESS ea = {};
ea.grfAccessPermissions = PROCESS_QUERY_LIMITED_INFORMATION; // 최소 권한만 허용
ea.grfAccessMode        = SET_ACCESS;
ea.Trustee.TrusteeType  = TRUSTEE_IS_WELL_KNOWN_GROUP;
ea.Trustee.ptstrName    = L"Everyone";

PACL pDacl = nullptr;
SetEntriesInAcl(1, &ea, nullptr, &pDacl);
SetSecurityDescriptorDacl(&sd, TRUE, pDacl, FALSE);

// 게임 프로세스 생성 시 보안 디스크립터 적용
SECURITY_ATTRIBUTES sa = { sizeof(sa), &sd, FALSE };
CreateProcess(L"WintersLOL.exe", nullptr, &sa, nullptr,
    FALSE, 0, nullptr, nullptr, &si, &pi);
```

---

## 5. 디버깅과 리버스 엔지니어링 기초

### 5.1 디버거의 동작 원리

```
디버거가 프로세스에 연결되면:

1. DebugActiveProcess(pid) 호출
   → 커널이 대상 프로세스에 디버그 포트 설정
   → 대상 프로세스의 모든 스레드가 일시 정지

2. 브레이크포인트 설정:
   소프트웨어 BP: 코드의 바이트를 0xCC (INT 3)로 교체
   하드웨어 BP: DR0~DR3 레지스터에 주소 설정 (4개 한정)
   메모리 BP: PAGE_GUARD 설정 → 접근 시 예외

3. 단일 스텝 실행:
   EFLAGS의 TF(Trap Flag) 설정 → 명령어 하나 실행 후 예외

4. 디버그 이벤트 수신:
   WaitForDebugEvent() → 예외, DLL 로드, 스레드 생성 등 수신
```

**치트 개발자의 디버거 활용:**

```
1. Cheat Engine: 메모리 스캔 + 값 변조
   → "체력 100" 검색 → 피격 → "체력 80" 재검색 → 주소 특정 → 값 변조

2. x64dbg / IDA Pro: 정적/동적 분석
   → 게임 함수 디컴파일 → 주요 로직 파악 → 훅 포인트 결정

3. ReClass.NET: 구조체 리버싱
   → 메모리의 특정 주소에서 시작하여 구조체 필드 추측
   → CPlayer.m_Health, CPlayer.m_Position 등 오프셋 파악
```

### 5.2 치트 엔진 동작 원리

```
Cheat Engine의 메모리 스캔:

1단계: 초기 스캔
  대상 프로세스의 전체 메모리를 ReadProcessMemory로 읽기
  조건에 맞는 주소 목록 생성
  예: "float 100.0" → 수십만 개 후보

2단계: 값 변화 후 재스캔
  게임에서 데미지를 받음 → 체력이 80으로 변경
  "float 80.0"으로 재스캔 → 후보가 수십 개로 줄어듦

3단계: 반복
  몇 번 반복하면 후보가 1~3개로 수렴
  → 그 주소가 체력 변수의 주소

4단계: 포인터 스캔
  매 실행마다 주소가 바뀜 (ASLR + 동적 할당)
  "기준 주소 + 오프셋" 체인을 역추적
  예: [[GameBase + 0x1A8] + 0x50] + 0x28 = 체력 주소
  → 포인터 체인은 ASLR에도 유효
```

### 5.3 PEB (Process Environment Block)

```
PEB는 프로세스의 메타정보를 담는 구조체:

struct PEB {
    BOOLEAN InheritedAddressSpace;      // +0x000
    BOOLEAN ReadImageFileExecOptions;   // +0x001
    BOOLEAN BeingDebugged;              // +0x002  ← 디버거 탐지!
    // ...
    PVOID   Ldr;                        // +0x018  ← 로드된 모듈 목록
    PVOID   ProcessParameters;          // +0x020
    // ...
    PVOID   ProcessHeap;               // +0x030
    // ...
};

PEB->Ldr->InLoadOrderModuleList:
  → 프로세스에 로드된 모든 DLL 목록
  → 인젝션된 DLL도 여기에 나타남 (숨기지 않는 한)

PEB 접근 방법:
  유저모드: NtQueryInformationProcess(ProcessBasicInformation)
            또는 __readgsqword(0x60) (x64 TEB → PEB)
  커널모드: PsGetProcessPeb(PEPROCESS)
```

---

## 6. 커널 모드 기초

### 6.1 시스템 콜 (SYSCALL)

```
유저모드에서 커널 함수 호출 과정:

ReadProcessMemory(hProc, addr, buf, size, &read)
  ↓
kernel32.dll!ReadProcessMemory   (유저모드 래퍼)
  ↓
ntdll.dll!NtReadVirtualMemory    (시스템 콜 스텁)
  mov eax, 0x3F       ; 시스템 콜 번호
  mov r10, rcx
  syscall              ; Ring 3 → Ring 0 전환!
  ↓
nt!NtReadVirtualMemory           (커널 함수)
  → 실제 메모리 복사 수행

SSDT (System Service Descriptor Table):
  시스템 콜 번호 → 커널 함수 주소 매핑 테이블
  SSDT[0x3F] = nt!NtReadVirtualMemory

SSDT 훅 (구형 치트/루트킷):
  SSDT[0x3F] = cheat_NtReadVirtualMemory  ← 자체 함수로 교체
  → 모든 ReadProcessMemory 호출을 가로챔
  → PatchGuard(KPP)가 이를 방지함
```

### 6.2 PatchGuard (Kernel Patch Protection)

```
PatchGuard (KPP):
  Microsoft가 구현한 커널 무결성 보호 시스템

감시 대상:
  - SSDT (System Service Descriptor Table)
  - IDT (Interrupt Descriptor Table)
  - GDT (Global Descriptor Table)
  - 커널 코드 (.text 섹션)
  - 주요 커널 객체 (EPROCESS, ETHREAD 등)

동작:
  - 랜덤 간격(5~10분)으로 검사 실행
  - 변조 감지 시 → BSOD (CRITICAL_STRUCTURE_CORRUPTION)
  - 검사 루틴 자체가 난독화되어 우회 어려움

안티치트에 대한 영향:
  - 정당한 안티치트도 SSDT 훅 불가
  - 대신 콜백 메커니즘 사용:
    ObRegisterCallbacks     → 핸들 필터링
    PsSetCreateProcessNotifyRoutine → 프로세스 생성 감시
    CmRegisterCallback      → 레지스트리 감시
    FltRegisterFilter       → 파일 시스템 감시
```

### 6.3 드라이버 서명 강제 (DSE)

```
Windows 64비트:
  모든 커널 드라이버는 반드시 디지털 서명 필요
  Microsoft 또는 WHQL 인증서로 서명

서명 없이 드라이버 로드하는 방법 (치트):
  1. 테스트 서명 모드 (bcdedit /set testsigning on) → 탐지 용이
  2. 취약한 정상 드라이버를 악용 (BYOVD — Bring Your Own Vulnerable Driver)
     예: capcom.sys, cpuz.sys 등에 커널 코드 실행 취약점
  3. DSE 우회 (EFI bootkit 등) → 고급 기법

안티치트 드라이버:
  - EV (Extended Validation) 인증서로 서명
  - Microsoft WHQL 인증 권장
  - Riot Vanguard: Microsoft 서명 + ELAM (Early Launch Anti-Malware)
```

---

## 7. Windows 커널 객체

### 7.1 EPROCESS와 ETHREAD

```
EPROCESS (Executive Process):
  커널에서 프로세스를 나타내는 구조체 (~0x800 bytes)

  +0x000 Pcb (KPROCESS)           ← 스케줄러 정보
  +0x2E0 UniqueProcessId         ← PID
  +0x2E8 ActiveProcessLinks      ← 프로세스 목록 (이중 연결 리스트)
  +0x3E8 Token                   ← 보안 토큰
  +0x450 ImageFileName            ← 프로세스 이름 (15자)
  +0x550 Peb                     ← PEB 주소
  +0x570 ObjectTable             ← 핸들 테이블

DKOM (Direct Kernel Object Manipulation):
  치트가 커널 드라이버로 EPROCESS.ActiveProcessLinks를 조작하면
  프로세스 목록에서 자신을 숨길 수 있음 (루트킷)

  정상 목록: 게임 ↔ 치트 ↔ 브라우저
  DKOM 후:   게임 ↔ 브라우저  (치트가 목록에서 사라짐)

  → Task Manager, EnumProcesses에서 안 보임
  → 안티치트도 열거 못함 (유저모드에서는)
  → 커널 안티치트가 직접 메모리 스캔으로 숨겨진 프로세스 탐지
```

### 7.2 핸들 테이블

```
프로세스의 핸들 테이블:
  ┌───────┬──────────────────────┬────────────┐
  │ Index │ Object               │ AccessMask │
  ├───────┼──────────────────────┼────────────┤
  │ 0x04  │ \Device\HarddiskVol │ RW         │
  │ 0x08  │ \Sessions\1\Event   │ SYNC       │
  │ 0x0C  │ Process(game.exe)   │ VM_READ    │ ← 치트의 게임 핸들!
  │ 0x10  │ Thread(MainThread)  │ ALL_ACCESS │
  └───────┴──────────────────────┴────────────┘

커널 안티치트의 핸들 감시:
  1. ObRegisterCallbacks로 핸들 생성 시점에 가로채기
  2. 게임 프로세스에 대한 핸들 요청 감시
  3. VM_READ/VM_WRITE 등 위험한 권한 자동 제거
  4. 요청 프로세스의 서명 확인 → 미서명 = 차단
```

---

## 8. 보안 관련 핵심 API 정리

### 8.1 유저모드 API

| 카테고리 | API | 용도 |
|----------|-----|------|
| **프로세스** | OpenProcess | 다른 프로세스 핸들 획득 |
| | CreateRemoteThread | 원격 스레드 생성 (DLL 인젝션) |
| | NtQueryInformationProcess | 프로세스 정보 조회 (디버거 탐지) |
| **메모리** | ReadProcessMemory | 다른 프로세스 메모리 읽기 |
| | WriteProcessMemory | 다른 프로세스 메모리 쓰기 |
| | VirtualAllocEx | 다른 프로세스에 메모리 할당 |
| | VirtualProtectEx | 메모리 보호 속성 변경 |
| **디버깅** | IsDebuggerPresent | PEB.BeingDebugged 확인 |
| | CheckRemoteDebuggerPresent | 원격 디버거 확인 |
| | OutputDebugStringW | 디버그 문자열 출력 (탐지 트릭) |
| **모듈** | GetModuleHandle | 로드된 모듈 핸들 |
| | EnumProcessModules | 프로세스의 모든 모듈 열거 |
| | GetModuleFileName | 모듈 경로 |

### 8.2 커널모드 API (드라이버)

| 카테고리 | API | 용도 |
|----------|-----|------|
| **프로세스 감시** | PsSetCreateProcessNotifyRoutine | 프로세스 생성/종료 콜백 |
| | PsSetCreateThreadNotifyRoutine | 스레드 생성/종료 콜백 |
| | PsSetLoadImageNotifyRoutine | DLL/드라이버 로드 콜백 |
| **핸들 보호** | ObRegisterCallbacks | 핸들 생성/복제 필터링 |
| **파일 감시** | FltRegisterFilter | 미니필터 드라이버 (파일 I/O) |
| **레지스트리** | CmRegisterCallback | 레지스트리 변경 감시 |
| **메모리** | MmCopyVirtualMemory | 커널에서 프로세스 메모리 복사 |
| | ZwQueryVirtualMemory | 메모리 영역 정보 조회 |
| **프로세스 조회** | PsLookupProcessByProcessId | PID → EPROCESS |
| | PsGetProcessImageFileName | 프로세스 이름 |
