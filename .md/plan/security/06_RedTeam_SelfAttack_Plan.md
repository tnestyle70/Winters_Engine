# Winters — Red Team / Self-Attack Plan
## 내 게임을 직접 털어서 방어 검증하기

> **범위**: 본인 소유 게임(Winters Engine) 에 대한 **허가된 오펜시브 보안 연구**. 목적은 방어 설계(Level 0~5) 를 실제 공격으로 검증하고, PART2의 이론을 실습으로 내재화하는 것.
> **금지사항**: 본 문서의 모든 도구/코드는 **Winters 자체 빌드에만 사용**. 제3자 상용 게임/서비스에 적용 시 컴퓨터망법·게임산업법 위반. 저장소 외부 반출 금지.
> **연계 문서**:
> - [PART2_ATTACK_TECHNIQUES.md](PART2_ATTACK_TECHNIQUES.md) — 각 공격 이론 상세
> - [PART3_USERMODE_ANTICHEAT.md](PART3_USERMODE_ANTICHEAT.md) — 유저모드 방어
> - [PART4_KERNEL_ANTICHEAT.md](PART4_KERNEL_ANTICHEAT.md) — 커널 방어
> - [PART5_WINTERS_IMPLEMENTATION.md](PART5_WINTERS_IMPLEMENTATION.md) — Winters 적용
> - [PLAN_SECURITY/Phase0~3](PLAN_SECURITY/) — 방어 Phase 별 구현
> **작성일**: 2026-04-19

---

## 0. 전체 원칙

### 0.1 동기 (Why Self-Attack)
- **방어 스펙만 읽으면 기억에 안 남는다**. 직접 털어봐야 "왜 이 방어가 필요한지" 체감.
- **탐지 지표(IOC) 설계**: 공격을 실행해서 Windows Event / ETW / 내 게임 로그에 어떤 신호가 남는지 수집 → 탐지 룰로 전환.
- **오탐 최소화**: 정상 입력과 구분되지 않는 공격부터 탐지 룰을 만들면 오탐이 폭발. 공격 변종을 직접 만들어보며 경계선을 잡는다.
- **진짜 치터 대응 경험**: 런칭 후 실 치트가 나오기 전에 자기 손으로 먼저 만들어봐야 반응 속도가 빨라진다.

### 0.2 성공 기준 (Red Team 의 KPI)
각 Phase 마다:
1. **공격 성공 재현** — 실제로 게임 상태를 조작하거나 정보를 빼냄
2. **탐지 로그 수집** — 방어 측에서 관측 가능한 시그널 목록 확보
3. **탐지 룰 PoC** — 같은 공격을 감지하는 방어 측 최소 구현
4. **방어 우회 시도** — 탐지 룰을 회피하는 변종 공격 제작 → 다시 탐지
5. **문서화** — 공격/탐지/우회 사이클을 `attack-log.md` 에 기록

### 0.3 법적/윤리 경계 (엄수)
- ✅ **허용**: Winters 자체 빌드(Debug/Release), 로컬 격리 VM, 개인 소유 테스트 서버
- ❌ **금지**: 상용 게임(LoL, 발로란트, 배그, 오버워치 등) 프로세스 메모리 접근/패킷 조작
- ❌ **금지**: 본 저장소의 Red Team 툴 외부 공개/배포/거래
- ❌ **금지**: 저장소 외부 공격 인프라 구축 (실제 치트 상점 모방 등)
- **원칙**: 공격 코드는 Winters 바이너리의 특정 offset/symbol에 하드코딩해 **다른 빌드에는 동작하지 않도록** 좁힌다.

### 0.4 저장소 구조 (격리)

Red Team 도구는 **메인 게임 저장소 분리 또는 gitignore로 격리**. 권장 구조:

```
Winters/
├── Engine/ Client/ Server/ ...   (기존 게임 코드)
└── RedTeam/                       (★ 신규, .gitignore 또는 private submodule)
    ├── README.md                  (경고 + 사용법)
    ├── Lab/                       (VM 이미지, 설정)
    ├── Tools/
    │   ├── MemScanner/            (Phase RT-1)
    │   ├── DllInjector/           (Phase RT-2a)
    │   ├── CheatDll/              (Phase RT-2b — ESP/Aimbot)
    │   ├── PacketProxy/           (Phase RT-3)
    │   └── KernelReader/          (Phase RT-4)
    ├── Experiments/               (실험 로그, 각 시도마다 하나 폴더)
    │   └── YYYY-MM-DD_<topic>/
    │       ├── attack-log.md
    │       ├── screenshots/
    │       └── artifacts/
    └── Detection/                 (방어 측 PoC — 게임 본체에 이관 전 테스트)
```

`.gitignore` 에 `RedTeam/Experiments/**/artifacts/` 추가 (메모리 덤프, 패킷 캡처 같은 용량 큰 산출물).

---

## Phase RT-0: Lab Environment 구축

### 목표
격리된 환경에서 공격/방어를 안전하게 반복 실행.

### 산출물
- **로컬 VM** (Hyper-V 또는 VMware) — Windows 11 Dev + Test-Signing 활성화
- **Winters Debug 빌드** 배치 (VM 내부)
- **Winters 로컬 서버** (Services/ Go 백엔드 + Docker) 호스트 측 실행
- **Cheat Engine 7.5** 설치 (RT-1 분석용) — https://cheatengine.org (공식)
- **x64dbg**, **Process Hacker 2**, **Wireshark**, **WinDbg** 설치
- **IDA Free** 또는 **Ghidra** (리버스용)

### 체크리스트
- [ ] Hyper-V 게스트 생성 (Win11, 16GB RAM, 120GB VHDX)
- [ ] 스냅샷 "clean" 저장 → 실험 후 복원 가능
- [ ] 게스트 네트워크: Internal Switch (호스트 ↔ VM, 외부 인터넷 차단 옵션)
- [ ] Winters Debug EXE + Data/Stage1.dat VM에 복사
- [ ] `bcdedit /set testsigning on` (커널 드라이버 자체 서명 로드 허용)
- [ ] 호스트에서 서버 기동: `docker compose up -d` + `go run ./cmd/...`
- [ ] Wireshark 필터: `udp.port == 27015 or tcp.port == 8081-8086`

### 연계
- [PART1_SECURITY_FUNDAMENTALS.md](PART1_SECURITY_FUNDAMENTALS.md) §Windows 권한
- [PLAN_SECURITY/Phase0_ServerAuthority.md](PLAN_SECURITY/Phase0_ServerAuthority.md) — 서버 구동 필요

---

## Phase RT-1: Memory Manipulation Hack (Cheat Engine 방식)

### 공격 시나리오
"이렐리아 HP 를 100에서 9999로" / "스킬 쿨다운 0으로" / "이동속도 ×5"

### 구현 순서

#### RT-1.1 — Cheat Engine 으로 수동 스캔 (워밍업)
1. Winters 실행 → 이렐리아 선택 → Scene_InGame
2. Cheat Engine → Process attach → `WintersGame.exe`
3. 현재 HP 값(예: 100) → "First Scan"
4. HP 감소(피격) → 새 값(예: 85) → "Next Scan"
5. 몇 번 반복하면 주소 1~5 개로 좁혀짐
6. 주소에 9999 기록 → 인게임 반영 확인
7. **결과 기록**: `Experiments/2026-04-XX_cheat-engine-warmup/attack-log.md`

#### RT-1.2 — MemScanner 자체 제작
`RedTeam/Tools/MemScanner/` Win32 C++ 콘솔.

**핵심 API**:
```cpp
HANDLE hProc = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE
                         | PROCESS_QUERY_INFORMATION, FALSE, pid);
VirtualQueryEx(hProc, addr, &mbi, sizeof(mbi));     // 메모리 맵 순회
ReadProcessMemory(hProc, addr, buf, size, &read);
WriteProcessMemory(hProc, addr, &newValue, sizeof(int), &written);
```

**기능**:
- [ ] `scan <pid> <type> <value>` — 전체 메모리 영역에서 값 검색
- [ ] `next <value>` — 좁혀가기 (이전 결과 중 현재 값도 맞는 것만)
- [ ] `write <addr> <value>` — 메모리 덮어쓰기
- [ ] `freeze <addr> <value>` — 주기적으로 덮어써서 고정 (핵 전형)

**검증 대상**:
- HP / 마나 / 쿨다운 타이머 (이미 float / int32 로 들어있는 값)
- `m_vPlayerDest` 좌표 (Vec3 → float 3개 연속)

**탐지 측 시그널** (PART3 Level 1 구현 때 써먹을 것):
- 외부 프로세스가 `OpenProcess(PROCESS_VM_WRITE, ...)` 로 우리 PID 에 접근 — 커널 콜백 `ObRegisterCallbacks` 없이는 유저모드에서 탐지 힘듦
- 우리 게임의 주요 값 해시 검증: SkillStateComponent.cooldownRemaining 을 주기적으로 스냅샷 해시 → 변조 감지
- 비정상 값 범위 탐지: HP > MaxHP, 쿨다운 < 0 같은 invariant 위반

#### RT-1.3 — 방어 PoC
Client 에 `CheckInvariants()` 훅 추가:
- SkillStateComponent 순회해서 쿨다운 범위 검증
- 위반 시 `[ANTICHEAT] cooldown invariant violated: slot=%d val=%.3f` 로그 → 서버 전송 플래그

### 연계
- [PART2_ATTACK_TECHNIQUES.md](PART2_ATTACK_TECHNIQUES.md) §4 Memory Hacking
- [PART3_USERMODE_ANTICHEAT.md](PART3_USERMODE_ANTICHEAT.md) §무결성 검증

---

## Phase RT-2: DLL Injection + In-Process Hacks

### 공격 시나리오
"ESP(월핵) — 적 위치 선으로 그리기" / "Aim-assist — 마우스를 타겟쪽으로 스냅"

### 구현 순서

#### RT-2a — DllInjector (로더)
`RedTeam/Tools/DllInjector/` 콘솔 EXE.

**알고리즘** (PART2 §1.1 참조):
```
1. OpenProcess(WintersGame.exe)
2. VirtualAllocEx — DLL 경로 버퍼 확보
3. WriteProcessMemory — L"C:\...\CheatDll.dll" 기록
4. GetProcAddress(kernel32, "LoadLibraryW") — 주소 획득
5. CreateRemoteThread(startRoutine=LoadLibraryW, arg=경로버퍼)
6. WaitForSingleObject → DLL 로드 완료
```

**변형 실험**:
- [ ] **Variant A**: `CreateRemoteThread` (탐지 쉬움)
- [ ] **Variant B**: `NtCreateThreadEx` 직접 호출 (유저모드 훅 일부 우회)
- [ ] **Variant C**: 기존 스레드의 Context 갈취 (`SetThreadContext` — APC Injection)
- [ ] **Variant D**: Manual Mapping (LoadLibrary 미경유, PE 파싱 + 재배치 직접 구현)

각 variant 에 대해 방어 측 탐지 가능 시점을 기록.

#### RT-2b — CheatDll (페이로드)
`RedTeam/Tools/CheatDll/` Winters 프로세스 내부에서 동작할 DLL.

**DllMain 훅 구성**:

```cpp
BOOL APIENTRY DllMain(HMODULE h, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(h);
        CreateThread(nullptr, 0, HackMain, nullptr, 0, nullptr);
    }
    return TRUE;
}

DWORD WINAPI HackMain(LPVOID)
{
    // 1) DX11 Present 후킹 → ESP 그리기
    HookPresent();
    // 2) ECS World 포인터 확보 → 적 위치 읽기
    LocateWorld();
    // 3) 루프
    while (!g_bExit) { Sleep(16); UpdateESP(); }
    return 0;
}
```

**ESP (월핵) 구현 요점**:
- `IDXGISwapChain::Present` VTable 후킹 — 게임 EXE 가 이미 메모리에 로드한 D3D11.dll 의 Present를 가로채기
- 후킹 방식: MinHook 라이브러리 또는 직접 JMP patch
- 적 월드 좌표 → 스크린 좌표 변환 (ViewProj 행렬 필요)
- ImGui overlay 로 박스/선 그림 (Winters 가 이미 ImGui DX11 초기화되어 있어 편함)

**Aimbot 구현 요점**:
- `GetCursorPos` / `SetCursorPos` 또는 `mouse_event` 로 강제 이동
- 가장 가까운 적 엔티티 선정 → 타겟 월드 좌표 → 스크린 좌표 → 커서 이동
- 감지 회피: 스무딩(곡선 이동) + 랜덤 지터

**탐지 측 시그널**:
- DLL Notify Callback (`PsSetLoadImageNotifyRoutine`) — 커널 필요
- 모듈 열거 시 서명 안된 DLL 발견 (`EnumProcessModulesEx` + `WinVerifyTrust`)
- IAT/EAT 훅 탐지 (`PART3 §IAT 검증`)
- Present 함수 첫 몇 바이트 해시 검증 (훅 설치 시 JMP 바뀜)
- ImGui draw 호출이 우리가 모르는 위치에서 발생 — 어려움
- 윈도우 커서 이동 패턴 (인간 비스무리한 움직임 vs 스냅) — 서버 측 통계

#### RT-2c — 방어 PoC
Client 에:
- [ ] 프로세스 자신의 로드된 모듈 목록 5초마다 스냅샷 → 화이트리스트 밖이면 서버 리포트
- [ ] 주요 DX11 함수 (`Present`, `DrawIndexed`) 첫 16바이트 해시 검증
- [ ] ImGuiLayer 에 self-check: 우리가 등록한 것 외의 Window/DrawList 개수 이상 시 경고

### 연계
- [PART2_ATTACK_TECHNIQUES.md](PART2_ATTACK_TECHNIQUES.md) §1 DLL Injection + §2 API Hooking
- [PART3_USERMODE_ANTICHEAT.md](PART3_USERMODE_ANTICHEAT.md) §Hooking 탐지 + §모듈 검증
- [PLAN_SECURITY/Phase1_UsermodeBasic.md](PLAN_SECURITY/Phase1_UsermodeBasic.md)
- [PLAN_SECURITY/Phase2_UsermodeAdvanced.md](PLAN_SECURITY/Phase2_UsermodeAdvanced.md)

---

## Phase RT-3: Packet Manipulation

### 공격 시나리오
"스킬 쿨다운 0 보내기" / "HTTP API 리플레이로 재화 무한 획득" / "매치메이킹 큐 스킵"

### 구현 순서

#### RT-3.1 — 패킷 캡처 + 분석 (수동)
1. Wireshark 로 Winters ↔ Services 통신 캡처
2. 각 엔드포인트 분석:
   - `POST /auth/login` (HTTPS) — JWT 발급
   - `POST /payment/charge` (HTTPS) — 결제
   - `POST /shop/purchase` (HTTPS) — 구매
   - `POST /matchmaking/join` (HTTPS)
   - Game server UDP (KCP over UDP, Phase 4) — 평문 여부 확인
3. **기록**: 각 요청/응답의 구조 → `Experiments/YYYY-MM-DD_packet-analysis/`

#### RT-3.2 — HTTPS 인터셉트 (mitmproxy)
`pip install mitmproxy`, VM 에 mitmproxy CA 설치.
- Winters Client 의 WinHTTP (Phase 8 C++ Client SDK) 가 시스템 CA 를 사용하면 mitmproxy 의 중간 인증서로 TLS 복호화 가능
- **검증 실험**:
  - [ ] `/payment/charge` 요청 body 변조 → 금액 증가 시도
  - [ ] `/shop/purchase` 응답 변조 → 구매 실패를 성공으로
  - [ ] JWT 토큰 수정 (`iat`, `exp`, `userId`) — HS256 서명 키 미보유 시 실패해야 정상
  - [ ] `Authorization` 헤더 제거/다른 유저 토큰 삽입

#### RT-3.3 — Replay Attack
- 이전에 정상 수행한 `/payment/charge` 요청을 그대로 재전송
- 서버가 nonce/idempotency key 로 거부하는지 확인
- 거부하지 않으면 → 서버 Phase 에 idempotency middleware 추가 필요

#### RT-3.4 — PacketProxy 자체 제작 (C++ WinDivert)
`RedTeam/Tools/PacketProxy/`
- WinDivert 드라이버로 특정 포트 UDP 패킷 후킹
- 조건부 변조 / 드롭 / 지연 주입 (레이턴시 시뮬)
- **실험**: 서버 권위 검증이 빈틈인 부분 찾기
  - 스킬 캐스트 command 의 `targetId` 를 임의 엔티티로 교체 → 서버가 AOI/시야 검증하는지
  - 이동 command 의 position delta 를 10x 부풀림 → 서버가 speed cap 적용하는지
  - damage 패킷 클라 → 서버 송신 여부 (서버 권위라면 아예 보내지 않아야 정상)

#### RT-3.5 — 방어 PoC
Services/:
- [ ] 모든 쓰기 API 에 `Idempotency-Key` 헤더 검증
- [ ] JWT 검증 + 짧은 TTL (5분) + refresh rotation
- [ ] Game server: 모든 클라 command 의 `sequence` 번호 검증, 롤백 방지
- [ ] Rate limiter: 초당 command 수 상한 (스킬 쿨다운 스팸)

### 연계
- [PART2_ATTACK_TECHNIQUES.md](PART2_ATTACK_TECHNIQUES.md) §6 Network
- [PLAN_SECURITY/Phase0_ServerAuthority.md](PLAN_SECURITY/Phase0_ServerAuthority.md)
- Services backend: Services/ Phase 1-7 완료 코드

---

## Phase RT-4: Kernel-Level Attack (연구용 프로토타입)

### 공격 시나리오
"유저모드 안티치트의 메모리 쓰기 차단을 커널에서 우회" — Vanguard 같은 방어의 한계 관찰

### ⚠️ 경계
이 Phase 는 **가장 위험**. 커널 드라이버는:
- BSOD/시스템 불안정 유발 가능 → 스냅샷 필수
- `bcdedit /set testsigning on` 을 영구 활성화해야 함 (Secure Boot 끄기 필요할 수 있음)
- 실수로 호스트에서 실행하면 재부팅 루프 가능성
- **반드시 VM 안에서만**, 호스트 PC 절대 금지

### 구현 순서

#### RT-4.1 — 최소 드라이버 (Hello World)
`RedTeam/Tools/KernelReader/` WDK Kernel-Mode Driver.

```c
#include <ntifs.h>

VOID DriverUnload(PDRIVER_OBJECT DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);
    DbgPrint("[KernelReader] unload\n");
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);
    DriverObject->DriverUnload = DriverUnload;
    DbgPrint("[KernelReader] loaded\n");
    return STATUS_SUCCESS;
}
```

- `sc create KernelReader type= kernel binPath= C:\...\KernelReader.sys`
- `sc start KernelReader`
- DebugView 로 `DbgPrint` 확인

#### RT-4.2 — Process Memory Read (유저 권한 우회)
```c
// PsLookupProcessByProcessId → EPROCESS 획득
// KeStackAttachProcess(target) → target 프로세스 주소공간 진입
// 원하는 주소 직접 읽기
// KeUnstackDetachProcess
```

**목표**: RT-1.2의 MemScanner 가 OpenProcess 차단(Level 3 방어) 당했을 때, 커널에서 우회해서 같은 값을 읽어낸다.

#### RT-4.3 — ObRegisterCallbacks 우회 연구
- Level 3 방어가 `ObRegisterCallbacks` 로 OpenProcess(PROCESS_VM_READ) 를 차단한다고 가정
- 커널 드라이버는 `ZwOpenProcess` 직접 호출로 우회 → 어떻게 탐지?
- → PART4 Callback 체이닝, PsSetLoadImageNotifyRoutine 로 드라이버 로드 자체를 감시

#### RT-4.4 — 방어 PoC
Winters Engine 에:
- [ ] **드라이버 로드 감시**: 게임 시작 시 `EnumDeviceDrivers` 로 로드된 드라이버 목록 → 화이트리스트 밖 + 서명 없는 드라이버 발견 시 서버에 리포트 (클라 측에서 차단은 못 함, 탐지만)
- [ ] **TPM 2.0 / Secure Boot 강제**: 미지원 환경 게임 실행 차단 (Vanguard 정책 모사)
- [ ] **주기적 공격 surface 최소화**: 민감 데이터는 암호화 보관 (`m_fHP` → XOR + 다른 주소 분산)

### 연계
- [PART4_KERNEL_ANTICHEAT.md](PART4_KERNEL_ANTICHEAT.md) — 커널 방어 전체
- [PLAN_SECURITY/Phase3_KernelDriver.md](PLAN_SECURITY/Phase3_KernelDriver.md)

---

## 실험 기록 템플릿 (`Experiments/YYYY-MM-DD_<topic>/attack-log.md`)

```markdown
# <공격 이름> — YYYY-MM-DD

## 목표
<공격으로 달성하려는 것>

## 전제 조건
- 게임 빌드: Winters <커밋해시>
- 방어 Level: <0 / 1 / 2 / 3>
- 환경: Hyper-V Win11 ...

## 실행 단계
1. ...
2. ...

## 결과
- [x] 공격 성공
- 스크린샷: screenshots/
- 바이너리 산출물: artifacts/

## 관측된 탐지 시그널
- Windows Event: ...
- 게임 로그: ...
- 네트워크: ...

## 탐지 룰 설계
<탐지 쿼리/코드>

## 우회 아이디어
<공격자가 다음으로 시도할 방법>
```

---

## 로드맵 매트릭스 (방어 Level 대 공격 Phase)

| 방어 Level | 대응 공격 Phase | 검증 목표 |
|-----------|----------------|-----------|
| Level 0 (서버 권위) | RT-3 패킷 조작 | 클라 주장만으로 상태 변경되는 엔드포인트 없음 |
| Level 1 (유저모드 기초) | RT-1 메모리 + RT-2a 기초 인젝션 | 평범한 CheatEngine/CreateRemoteThread 탐지 |
| Level 2 (유저모드 심화) | RT-2 변형들 (NtCreateThreadEx, APC, Manual Map) | 은밀한 인젝션도 탐지 |
| Level 3 (커널 기초) | RT-4.1~4.2 커널 읽기 | 유저모드로는 뚫리지 않음, 커널만 남게 됨 |
| Level 4 (커널 심화) | RT-4.3~4.4 커널 우회 | 서명 없는 드라이버 로드 차단 |
| Level 5 (하이퍼바이저) | (연구 범위 외) | — |

---

## 진행 순서 (권장)

1. **Phase RT-0** Lab 구축 (반나절)
2. **Phase RT-1.1** CheatEngine 수동 스캔 체험 (1시간)
3. **Phase RT-1.2** MemScanner 제작 (4~6시간)
4. **[중단점]** 방어 Level 1 (Phase 1 Usermode Basic) 구현 → RT-1이 탐지되는지 확인
5. **Phase RT-2a/b** DllInjector + CheatDll ESP (8~12시간)
6. **[중단점]** 방어 Level 2 구현 → RT-2 variant 몇 개가 탐지되는지
7. **Phase RT-3** mitmproxy + PacketProxy (8시간)
8. **[중단점]** Services Phase 8 완료 후 JWT/idempotency 검증
9. **Phase RT-4** 커널 드라이버 (위험, 주말 하루 통으로 — VM 격리 필수)

---

## 체크리스트 — 시작 전 확인

- [ ] 본 문서를 처음부터 끝까지 읽음
- [ ] `RedTeam/` 폴더가 git-tracked 라면 private 저장소로 분리했거나 `.gitignore` 에 제외했음
- [ ] 실험용 VM 스냅샷 "clean" 존재함
- [ ] 작성한 Red Team 도구는 Winters 외 게임에 사용 안 함 (법적 인지)
- [ ] 각 Phase 완료 시 `attack-log.md` 작성
- [ ] 각 Phase 의 **탐지 측 PoC 도 함께 구현** — 공격만 만들고 넘어가지 않음

---

## 다음 단계 (본 계획 완료 후)

- **Red Team vs Blue Team 반복**: 내가 만든 공격 → 내가 만든 탐지 → 변종 공격 → 개선된 탐지. 3~5 라운드 반복 시 이 게임의 방어 수준이 Valorant 초기 수준에 근접.
- **외부 커뮤니티 참여**: Dreamhack, HackTheBox 의 Pwnable/Reversing 카테고리로 공격 실력 상승 → 다시 Winters 에 적용.
- **PART6 신규 문서**: 본 계획 실행 중 발견한 실전 인사이트를 `PART6_LESSONS_LEARNED.md` 에 축적.
