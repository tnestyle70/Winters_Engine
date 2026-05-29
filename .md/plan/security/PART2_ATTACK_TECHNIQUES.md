# Part 2: 공격 기법 — DLL Injection, Hooking, Memory Hacking

> 공격을 이해해야 방어를 설계할 수 있다.
> 모든 기법은 **방어 관점에서의 이해**를 위해 설명한다.

---

## 1. DLL Injection

### 1.1 CreateRemoteThread 인젝션 (가장 기초)

```
공격 흐름:
  1. OpenProcess(PROCESS_ALL_ACCESS)로 게임 핸들 획득
  2. VirtualAllocEx로 게임 프로세스에 메모리 할당
  3. WriteProcessMemory로 DLL 경로 문자열 기록
  4. CreateRemoteThread로 LoadLibraryW를 호출하는 스레드 생성
  5. 게임 프로세스 내에서 치트 DLL 로드 완료!
```

```cpp
// [공격 코드 — 개념 이해용]
void InjectDLL(DWORD pid, const wchar_t* dllPath)
{
    // 1. 게임 프로세스 열기
    HANDLE hProc = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION |
        PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION,
        FALSE, pid);

    // 2. 게임 프로세스 내에 메모리 할당
    size_t pathSize = (wcslen(dllPath) + 1) * sizeof(wchar_t);
    LPVOID pRemotePath = VirtualAllocEx(hProc, nullptr, pathSize,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    // 3. DLL 경로 문자열 기록
    WriteProcessMemory(hProc, pRemotePath, dllPath, pathSize, nullptr);

    // 4. LoadLibraryW의 주소 획득
    //    kernel32.dll은 모든 프로세스에서 같은 주소에 로드됨 (ASLR이지만 공유)
    FARPROC pLoadLibrary = GetProcAddress(
        GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");

    // 5. 원격 스레드 생성 → LoadLibraryW(dllPath) 실행
    HANDLE hThread = CreateRemoteThread(hProc, nullptr, 0,
        (LPTHREAD_START_ROUTINE)pLoadLibrary, pRemotePath, 0, nullptr);

    WaitForSingleObject(hThread, INFINITE);
    // → 게임 프로세스 내에서 치트 DLL의 DllMain 실행!

    VirtualFreeEx(hProc, pRemotePath, 0, MEM_RELEASE);
    CloseHandle(hThread);
    CloseHandle(hProc);
}
```

**방어 포인트:**

```
탐지 기회:
  ① OpenProcess → ObRegisterCallbacks에서 차단
  ② VirtualAllocEx → 게임 프로세스에 새 RWX 영역 생성 감지
  ③ WriteProcessMemory → 메모리 쓰기 차단
  ④ CreateRemoteThread → PsSetCreateThreadNotifyRoutine에서 탐지
  ⑤ LoadLibrary → PsSetLoadImageNotifyRoutine에서 DLL 로드 탐지
```

### 1.2 NtCreateThreadEx (더 은밀한 버전)

```
CreateRemoteThread는 내부적으로 ntdll!NtCreateThreadEx를 호출.
치트가 직접 NtCreateThreadEx를 호출하면 일부 유저모드 훅을 우회 가능.

typedef NTSTATUS(NTAPI* pNtCreateThreadEx)(
    PHANDLE hThread, ACCESS_MASK access, PVOID objAttr,
    HANDLE hProcess, PVOID startRoutine, PVOID arg,
    ULONG flags, SIZE_T zeroBits, SIZE_T stackSize,
    SIZE_T maxStackSize, PVOID attrList);

→ ntdll에서 직접 syscall 스텁 호출
→ 유저모드 훅은 우회되지만 커널 콜백은 우회 못함!
```

### 1.3 Thread Hijacking (스레드 하이재킹)

```
새 스레드를 생성하지 않고, 게임의 기존 스레드를 이용:

  1. SuspendThread로 게임 스레드 정지
  2. GetThreadContext로 RIP(명령 포인터) 저장
  3. VirtualAllocEx로 쉘코드 메모리 할당
  4. SetThreadContext로 RIP를 쉘코드 주소로 변경
  5. ResumeThread → 게임 스레드가 쉘코드 실행
  6. 쉘코드 내에서 LoadLibrary 호출 후 원래 RIP로 복귀

장점 (공격자 관점):
  - CreateRemoteThread 호출 없음 → 새 스레드 생성 콜백 미발생
  - 게임 스레드 컨텍스트에서 실행 → 탐지 어려움

방어:
  - SetThreadContext 호출 감시 (유저모드)
  - 스레드 스택의 반환 주소 검증 (Stack Walking)
  - 예상치 못한 코드 영역 실행 탐지
```

### 1.4 APC Injection (Asynchronous Procedure Call)

```
APC 큐를 이용한 인젝션:

  1. 게임 프로세스의 Alertable 상태 스레드를 찾음
     (SleepEx, WaitForSingleObjectEx, MsgWaitForMultipleObjectsEx 사용 중인 스레드)
  2. QueueUserAPC(LoadLibraryW, hThread, dllPath) 호출
  3. 스레드가 Alertable wait에서 깨어날 때 APC 실행
  4. → 게임 스레드 컨텍스트에서 DLL 로드

방어:
  - 커널에서 APC 삽입 감시 (PsSetCreateThreadNotifyRoutine + APC 검사)
  - 로드된 모듈 목록 주기적 검증
```

### 1.5 Manual Mapping (수동 매핑)

```
LoadLibrary를 사용하지 않고, DLL을 수동으로 메모리에 매핑:

  1. DLL 파일을 로컬에서 읽기
  2. VirtualAllocEx로 게임에 메모리 할당
  3. PE 헤더 파싱 → 섹션별로 메모리 복사
  4. Import Table 해석 → 함수 주소 직접 리졸브
  5. Relocation Table 처리 → 재배치 적용
  6. DllMain 직접 호출 (쉘코드로)
  7. PE 헤더 제거 (MEM_DECOMMIT)

장점 (공격자 관점):
  - PEB->Ldr 모듈 목록에 나타나지 않음!
  - LoadLibrary 호출 없음 → PsSetLoadImageNotifyRoutine 미발생
  - PE 헤더 제거 → 메모리 스캔으로 DLL 식별 어려움

방어:
  - 프로세스 메모리의 실행 가능(RX/RWX) 영역 전수 스캔
  - 알려진 모듈 목록과 비교 → 미등록 실행 영역 = 의심
  - VAD(Virtual Address Descriptor) 트리를 커널에서 직접 열거
  - 코드 서명 검증 (서명 없는 실행 코드 = 의심)
```

---

## 2. Code Injection (코드 인젝션)

### 2.1 Shellcode Injection

```
DLL 전체가 아닌 작은 코드 조각(쉘코드)을 직접 주입:

  1. VirtualAllocEx(MEM_COMMIT, PAGE_EXECUTE_READWRITE)
  2. WriteProcessMemory로 쉘코드 기록
  3. CreateRemoteThread/APC/Hijack으로 실행

쉘코드 예시 (개념):
  ; 게임 체력 변수를 고정하는 쉘코드
  mov rax, [health_address]
  mov dword ptr [rax], 0x42C80000  ; float 100.0
  ret

장점: 매우 작음 (수십~수백 바이트), DLL 파일 불필요
단점: 기능 제한적, 메모리에 코드가 보임
```

### 2.2 Code Cave Injection

```
기존 코드의 빈 공간(Code Cave)에 코드를 숨김:

PE 섹션 정렬(4KB)로 인해 섹션 끝에 0x00 패딩이 있음:

  .text 섹션:
  0x401000: 실제 코드...
  ...
  0x44F800: 마지막 코드
  0x44F820: 00 00 00 00 00 00 00 ...  ← Code Cave!
  0x450000: .rdata 섹션 시작

공격:
  1. Code Cave에 치트 코드 기록
  2. 게임 코드의 원하는 위치를 JMP로 패치 → Cave로 점프
  3. Cave에서 치트 로직 실행 후 원래 코드로 JMP 복귀

방어:
  - .text 섹션 전체 해시 검증 (패치 탐지)
  - Code Cave 영역 무결성 검사
```

---

## 3. API Hooking

### 3.1 IAT Hooking

```
원리:
  실행 파일의 IAT(Import Address Table)에 있는 함수 주소를 변조

  정상 IAT:
    kernel32!ReadFile → 0x7FF8ABCD1234

  훅 후:
    kernel32!ReadFile → 0x치트함수주소

구현:
  1. 대상 모듈의 PE 헤더 → Import Directory 파싱
  2. 원하는 함수의 IAT 엔트리 찾기
  3. VirtualProtect로 IAT 페이지를 RW로 변경
  4. 주소를 치트 함수로 교체
  5. VirtualProtect로 원래 보호 복원

탐지 방법:
  - IAT의 각 엔트리가 해당 DLL의 주소 범위 내에 있는지 확인
  - 예: ReadFile의 주소가 kernel32.dll의 범위(0x7FF8ABCD0000~...)가 아니면 훅!
```

### 3.2 Inline Hooking (Detour)

```
함수의 첫 바이트를 JMP 명령으로 교체:

원본 함수:
  NtReadVirtualMemory:
    4C 8B D1          mov r10, rcx        ← 이 부분을 덮어씀
    B8 3F 00 00 00    mov eax, 0x3F
    0F 05             syscall
    C3                ret

훅 후:
  NtReadVirtualMemory:
    FF 25 00 00 00 00  jmp [rip+0]        ← 14바이트 JMP (x64)
    치트함수주소 8바이트
    ...

치트 함수 (Trampoline 패턴):
  void HookedNtReadVirtualMemory(...)
  {
      // 1. 안티치트가 내 프로세스를 읽으려 하면 가짜 데이터 반환
      if (targetProcess == cheatProcess)
          return STATUS_ACCESS_DENIED;

      // 2. 원본 함수 호출 (덮어쓴 바이트를 복원한 트램펄린)
      return OriginalNtReadVirtualMemory(...);
  }

탐지 방법:
  - 함수 프롤로그 검증: 원본 바이트와 비교
  - ntdll.dll의 디스크 파일과 메모리 비교
  - 함수 시작이 JMP/CALL이면 훅 의심
```

### 3.3 VTable Hooking

```
C++ 가상 함수 테이블(vtable)의 함수 포인터 교체:

  CPlayer 객체:
    vptr → [vtable]
              [0] → CPlayer::Update        (원본)
              [1] → CPlayer::Render         (원본)
              [2] → CPlayer::TakeDamage     (원본) ← 치트 타겟

  훅 후:
    vptr → [가짜 vtable]
              [0] → CPlayer::Update        (원본 유지)
              [1] → CPlayer::Render         (원본 유지)
              [2] → HookedTakeDamage        (무적 치트!) ← 교체

치트 함수:
  void HookedTakeDamage(float damage)
  {
      // 데미지를 0으로 만들어 무적
      return;  // 원본 TakeDamage를 호출하지 않음
  }

방어:
  - vtable을 읽기 전용 메모리(.rdata)에 배치
  - vtable 포인터 주기적 검증
  - 함수 포인터가 .text 섹션 범위 내인지 확인
```

### 3.4 EAT Hooking (Export Address Table)

```
DLL의 Export Table을 변조하여 다른 프로세스가 해당 함수를 가져올 때
치트 함수 주소를 받게 만듦.

ntdll.dll EAT:
  NtReadVirtualMemory → RVA 0x12345 (정상)

훅 후:
  NtReadVirtualMemory → RVA 0x99999 (치트 영역)

→ GetProcAddress로 가져오는 모든 호출자가 치트 함수 주소를 받음

방어:
  - GetProcAddress 결과와 직접 EAT 파싱 결과 비교
  - EAT RVA가 DLL 범위 밖이면 포워딩 또는 훅
```

---

## 4. Memory Hacking

### 4.1 값 변조 (Value Editing)

```
기법                          예시                      방어
──────────────────────────────────────────────────────────────
정적 주소                     [0x12345678] = 체력       ASLR 활성화
포인터 체인                   [[base+0x1A8]+0x50] = HP  포인터 암호화
메모리 스캔                   Cheat Engine 패턴 스캔    값 암호화/분산 저장
값 동결(Freeze)               매 프레임 값 덮어쓰기     서버 권위 검증
```

### 4.2 값 암호화 (Anti-Memory Scan)

```cpp
// 단순 XOR 암호화 — 치트 엔진의 "정확한 값" 스캔 방지
class CEncryptedFloat
{
public:
    void Set(float value)
    {
        // 매번 다른 키로 암호화
        m_Key = GenerateRandomKey();
        uint32_t raw;
        memcpy(&raw, &value, 4);
        m_Encrypted = raw ^ m_Key;
    }

    float Get() const
    {
        uint32_t raw = m_Encrypted ^ m_Key;
        float value;
        memcpy(&value, &raw, 4);
        return value;
    }

private:
    uint32_t m_Encrypted = 0;
    uint32_t m_Key = 0;

    static uint32_t GenerateRandomKey()
    {
        // RDRAND 또는 __rdtsc 기반
        return static_cast<uint32_t>(__rdtsc());
    }
};

// 메모리에 저장되는 것:
//   치트 엔진이 "float 100.0" (0x42C80000)을 검색해도
//   실제 메모리에는 0x42C80000 ^ key 값이 저장되어 있으므로 매치 안 됨
//   + 매번 Set할 때마다 key가 바뀌므로 "변하지 않는 값" 스캔도 실패
```

### 4.3 분산 저장 (Scattered Storage)

```cpp
// 체력을 여러 변수에 분산 저장하고, 읽을 때 합산
class CDistributedHealth
{
public:
    void Set(float value)
    {
        // 3개의 변수에 랜덤 분산
        float r1 = RandomFloat(-1000.0f, 1000.0f);
        float r2 = RandomFloat(-1000.0f, 1000.0f);
        float r3 = value - r1 - r2;

        m_Part1 = r1;
        m_Part2 = r2;
        m_Part3 = r3;
        m_Checksum = HashFloat(value);  // 무결성 체크
    }

    float Get() const
    {
        float value = m_Part1 + m_Part2 + m_Part3;
        if (HashFloat(value) != m_Checksum)
        {
            // 변조 감지! → 서버에 리포트
            ReportCheat(CheatType::MemoryTamper);
        }
        return value;
    }

private:
    float    m_Part1    = 0.0f;  // 의미 없는 값
    float    m_Part2    = 0.0f;  // 의미 없는 값
    float    m_Part3    = 0.0f;  // 의미 없는 값
    uint32_t m_Checksum = 0;
};
```

### 4.4 Speed Hack

```
치트 원리:
  Windows의 시간 함수를 후킹하여 게임 시간을 가속

  QueryPerformanceCounter → 실제 값의 2배 반환
  GetTickCount64         → 실제 값의 2배 반환
  timeGetTime            → 실제 값의 2배 반환

  → 게임의 deltaTime이 2배 → 이동 속도 2배, 공격 속도 2배

방어:
  1. 서버 시간과 비교 — 서버 틱레이트와 클라이언트 시간 차이 검증
  2. 다중 타이머 교차 검증 — QPC, RDTSC, NtQuerySystemTime 결과 비교
  3. 서버 권위 — 이동 속도는 서버에서 검증 (speed = distance / server_dt)
```

---

## 5. Wallhack과 ESP

### 5.1 Depth Buffer 조작

```
월핵 원리:
  깊이 테스트(Depth Test)를 비활성화하면 벽 뒤 적이 보임

  정상: DepthFunc = LESS → 벽이 적보다 가까우면 적 미렌더링
  치트: DepthFunc = ALWAYS → 깊이 무시, 모든 오브젝트 렌더링

구현 방법:
  1. D3D11 IAT 훅 → CreateDepthStencilState를 가로채서
     DepthEnable = FALSE로 변경
  2. DrawIndexed 훅 → 적 챔피언 렌더링 시에만 깊이 비활성화

방어:
  1. 서버에서 시야 밖 적 정보 미전송 (FOW / AOI)
     → 클라이언트에 데이터가 없으면 월핵으로도 못 봄!
  2. D3D11 함수 훅 탐지
  3. 깊이 스텐실 상태 무결성 검증
```

### 5.2 ESP (Extra Sensory Perception)

```
화면에 적 정보를 오버레이로 표시:

  ┌────────────────────────────────────┐
  │ 게임 화면                          │
  │                                    │
  │     [적1] HP: 250/500              │  ← ESP 오버레이
  │     ┌──┐  거리: 1500              │
  │     │  │  →                        │
  │     └──┘                          │
  │                                    │
  │            [적2] HP: 800/800       │
  │            거리: 3200              │
  └────────────────────────────────────┘

구현:
  1. ReadProcessMemory로 적 위치/체력 읽기
  2. 월드좌표 → 스크린좌표 변환 (W2S, ViewProjection 행렬)
  3. GDI/DirectX 오버레이로 그리기

방어:
  1. 서버 사이드 FOW → 시야 밖 적 데이터 미전송 (근본 해결)
  2. 오버레이 윈도우 탐지 (EnumWindows, WS_EX_TOPMOST)
  3. ViewProjection 행렬 암호화
```

---

## 6. Aimbot

```
자동 조준 치트:

원리:
  1. 적 위치 데이터 읽기 (ReadProcessMemory 또는 인젝션)
  2. 가장 가까운 적의 머리 위치 계산
  3. 현재 카메라 각도에서 목표까지의 각도 차이 계산
  4. mouse_event / SendInput으로 마우스 이동
  5. → 자동으로 적 머리에 조준

MOBA에서의 변형:
  - 스킬샷 자동 조준: 적 이동 방향 + 투사체 속도로 예측 위치 계산
  - 콤보 자동화: QWER을 최적 순서/타이밍으로 자동 입력
  - 완벽한 CS(Last Hit): 미니언 체력 계산 → 정확한 타이밍에 공격

방어:
  1. 서버 사이드 입력 검증 — 인간 불가능한 정확도/반응 속도 탐지
  2. 입력 패턴 분석 — 기계적으로 완벽한 입력 패턴 탐지
  3. 마우스 움직임 분석 — 직선/곡선 패턴, 가속도 프로파일
  4. 통계적 이상 탐지 — 스킬샷 적중률이 비정상적으로 높으면 플래그
```
