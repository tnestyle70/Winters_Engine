# Stage 1 — Server Fiber Skeleton 박제 (h/cpp 전문)

작성일: 2026-05-07
상위: [`00_INDEX_SERVER_FIBER_INTEGRATION.md`](00_INDEX_SERVER_FIBER_INTEGRATION.md)
목표: **`CServerEntry` 글로벌에 `CJobSystem` 단일 인스턴스를 보유. `main.cpp` 가 Initialize/Shutdown. `CGameRoom::TickThread` 가 시작 시 `ConvertThreadToFiber`, 종료 시 `ConvertFiberToThread` shell 만 적용. Submit 호출 0, Phase 분해 0, 행동 변화 0.**

---

## 1. Preflight — 사실 확정 표 (TODO 0)

| 항목 | 확정값 | 출처 |
|---|---|---|
| `CJobSystem` 생성/소멸 시그니처 | `CJobSystem();` `~CJobSystem();` non-copyable | [JobSystem.h:28-32](../../../Engine/Public/Core/JobSystem.h:28) |
| `CJobSystem::Initialize` | `void Initialize(std::uint32_t iWorkerCount = 0);` (0 = hw_concurrency-2) | [JobSystem.h:35](../../../Engine/Public/Core/JobSystem.h:35) |
| `CJobSystem::SetExecutionMode` | `void SetExecutionMode(eJobExecutionMode);` | [JobSystem.h:37](../../../Engine/Public/Core/JobSystem.h:37) |
| `eJobExecutionMode` 멤버 | `ThreadOnly = 0`, `FiberShell` | [FiberTypes.h:5-9](../../../Engine/Public/Core/Fiber/FiberTypes.h:5) |
| `CJobSystem::Shutdown` | `void Shutdown();` | [JobSystem.h:36](../../../Engine/Public/Core/JobSystem.h:36) |
| Win32 Fiber API | `ConvertThreadToFiber(LPVOID)`, `ConvertFiberToThread()` (Win32 SDK) | Windows.h |
| Server tick thread 진입점 | `void CGameRoom::TickThread();` | [GameRoom.cpp:268-302](../../../Server/Private/Game/GameRoom.cpp:268) |
| main.cpp 의 entry 흐름 | WSAStartup → `CGameRoom::Create(1)` → `g_pRoom = ...` → `room->Start()` → `CIOCPCore::Create(9000, 4)` → quit loop → `core->Shutdown()` → `room->Stop()` → `g_pRoom = nullptr` → `WSACleanup()` | [main.cpp:37-112](../../../Server/Private/main.cpp:37) |
| Engine project reference | Server include path 에 `Engine/Public` 포함 | [Server.vcxproj:54](../../../Server/Include/Server.vcxproj:54) |
| PostBuild Engine DLL copy | `WintersEngine.dll` Bin/Debug 또는 Bin/Release 에서 copy | [Server.vcxproj:64](../../../Server/Include/Server.vcxproj:64) |

**TODO 0 확인**: 위 모든 행에 "필요"/"추정"/"TBD" 0.

---

## 2. 변경 파일 목록 (5 파일)

| 종류 | 파일 |
|---|---|
| 신규 .h | `Server/Public/Game/ServerEntry.h` |
| 신규 .cpp | `Server/Private/Game/ServerEntry.cpp` |
| 수정 .cpp | `Server/Private/main.cpp` |
| 수정 .cpp | `Server/Private/Game/GameRoom.cpp` (TickThread 만, 다른 함수 변경 0) |
| 수정 vcxproj | `Server/Include/Server.vcxproj` (신규 .cpp 등록) |

---

## 3. 신규 파일 1 — `Server/Public/Game/ServerEntry.h`

### 3.1 경로

`C:\Users\user\Desktop\Winters\Server\Public\Game\ServerEntry.h`

### 3.2 전문

```cpp
// ─────────────────────────────────────────────────────────────
//  ServerEntry.h  |  Server 프로세스 전역 진입점
//
//  역할:
//    - CJobSystem 단일 인스턴스를 보유한다 (process 당 1).
//    - main.cpp 가 Initialize/Shutdown 호출.
//    - 향후 멀티 GameRoom (Sim-10 v2) 진입 시 모든 GameRoom 이 이 인스턴스 공유.
//
//  Owner scope 매트릭스 (00_INDEX §3 참조):
//    - CJobSystem: process 1 (RHI Device 와 동일 패턴)
//    - CGameRoom 의 TickThread: GameRoom 별 1 — 본 진입점에서 Submit 만 호출
//    - CJobCounter: 함수 로컬 (Tick stack)
//
//  Stage 1 의 약속:
//    - Submit 호출 0
//    - WaitForCounter 호출 0
//    - SetExecutionMode(FiberShell) 한 번만 호출
//    - Initialize/Shutdown 수명만 관리
// ─────────────────────────────────────────────────────────────

#pragma once

#include "Core/JobSystem.h"           // CJobSystem (Engine/Public)
#include "Core/JobCounter.h"          // CJobCounter (Engine/Public) — 본 헤더는 forward 만 쓰지만 caller convenience
#include "Core/Fiber/FiberTypes.h"    // eJobExecutionMode

#include <atomic>
#include <cstdint>

class CServerEntry
{
public:
    CServerEntry(const CServerEntry&) = delete;
    CServerEntry& operator=(const CServerEntry&) = delete;

    // ── 생명주기 ──────────────────────────────────────────────
    // main.cpp 가 WSAStartup 직후, GameRoom 생성 전에 호출.
    //   - iWorkerCount: 0 이면 hw_concurrency-2 (CJobSystem.h L34 default)
    //   - bEnableFiberShell: true 면 SetExecutionMode(FiberShell). false 면 ThreadOnly 유지.
    // 두 번째 호출은 무시 + 진단 로그.
    static bool Initialize(std::uint32_t iWorkerCount = 0,
                           bool bEnableFiberShell = true);

    // main.cpp 가 GameRoom 정리 후, WSACleanup 전에 호출.
    static void Shutdown();

    // ── 접근자 ────────────────────────────────────────────────
    // Tick thread / Phase helper 가 Submit/WaitForCounter 호출 시 사용.
    // Initialize 전이거나 Shutdown 후면 nullptr.
    static CJobSystem* Get_JobSystem();

    // 현재 모드 조회 (디버깅/검증용).
    static eJobExecutionMode Get_ExecutionMode();

    static bool IsInitialized();

private:
    CServerEntry() = default;
    ~CServerEntry() = default;

    static std::atomic<bool>      s_bInitialized;
    static CJobSystem             s_JobSystem;
    static eJobExecutionMode      s_eExecutionMode;
};
```

### 3.3 설계 근거 (인용)

- `CJobSystem` 멤버 — non-copyable ([JobSystem.h:31-32](../../../Engine/Public/Core/JobSystem.h:31)) 이므로 static 멤버로 보유. `unique_ptr` 사용 시 dllexport 클래스에서 copy ctor 명시 delete 필요 (CLAUDE.md §5.2 P-2 변형) — 단순 static 멤버로 우회.
- `Get_JobSystem()` 반환 raw 포인터 — Phase 5-A `CJobSystem` 은 비-인터페이스 구체 클래스. `IJobSystem*` 추상화는 Phase 5-B 별도 박제 (CLAUDE.md §6.5 Tier-2 패턴).
- `bEnableFiberShell = true` 기본값 — Stage 2~3 진입 시 자동 적용. Stage 1 은 ThreadOnly 와 동작 동일이므로 회귀 0.

---

## 4. 신규 파일 2 — `Server/Private/Game/ServerEntry.cpp`

### 4.1 경로

`C:\Users\user\Desktop\Winters\Server\Private\Game\ServerEntry.cpp`

### 4.2 전문

```cpp
#include "Game/ServerEntry.h"

#include <iostream>

// ── Static 멤버 정의 ────────────────────────────────────────
std::atomic<bool>   CServerEntry::s_bInitialized{ false };
CJobSystem          CServerEntry::s_JobSystem{};
eJobExecutionMode   CServerEntry::s_eExecutionMode{ eJobExecutionMode::ThreadOnly };

bool CServerEntry::Initialize(std::uint32_t iWorkerCount, bool bEnableFiberShell)
{
    bool expected = false;
    if (!s_bInitialized.compare_exchange_strong(expected, true,
        std::memory_order_acq_rel))
    {
        std::cerr << "[ServerEntry] Initialize called twice — ignored.\n";
        return false;
    }

    // CJobSystem.h L34: iWorkerCount==0 → 자동 (hw_concurrency-2)
    s_JobSystem.Initialize(iWorkerCount);

    // FiberTypes.h L5-L9 의 eJobExecutionMode { ThreadOnly, FiberShell }
    // Stage 1: set 만 — Engine 측 본체 구현 여부와 무관. ThreadOnly 와 동작 동일.
    s_eExecutionMode = bEnableFiberShell
        ? eJobExecutionMode::FiberShell
        : eJobExecutionMode::ThreadOnly;
    s_JobSystem.SetExecutionMode(s_eExecutionMode);

    std::cout << "[ServerEntry] Initialize OK"
              << " workers=" << s_JobSystem.GetWorkerCount()
              << " mode="
              << (s_eExecutionMode == eJobExecutionMode::FiberShell
                  ? "FiberShell" : "ThreadOnly")
              << "\n";
    return true;
}

void CServerEntry::Shutdown()
{
    bool expected = true;
    if (!s_bInitialized.compare_exchange_strong(expected, false,
        std::memory_order_acq_rel))
    {
        return;
    }

    s_JobSystem.Shutdown();
    std::cout << "[ServerEntry] Shutdown complete.\n";
}

CJobSystem* CServerEntry::Get_JobSystem()
{
    if (!s_bInitialized.load(std::memory_order_acquire))
        return nullptr;
    return &s_JobSystem;
}

eJobExecutionMode CServerEntry::Get_ExecutionMode()
{
    return s_eExecutionMode;
}

bool CServerEntry::IsInitialized()
{
    return s_bInitialized.load(std::memory_order_acquire);
}
```

### 4.3 검증 포인트

- `compare_exchange_strong` — Initialize/Shutdown 멱등성 보장.
- `Get_JobSystem()` 이 `nullptr` 반환 가능 — Tick thread 가 Initialize 전 또는 Shutdown 후 호출 시 안전. Stage 2~3 의 Submit 호출 path 에서 nullptr 가드.
- 로그 출력은 `std::cout` (main.cpp 와 동일 — `std::cout.setf(std::ios::unitbuf)` 가 main.cpp L39 에 있음).

---

## 5. 수정 파일 1 — `Server/Private/main.cpp`

### 5.1 경로

`C:\Users\user\Desktop\Winters\Server\Private\main.cpp`

### 5.2 수정 전 (L1-L112 전문, 출처 [main.cpp:1](../../../Server/Private/main.cpp:1))

```cpp
#include "Game/GameRoom.h"
#include "Network/IOCPCore.h"
#include "Network/PacketDispatcher.h"

#include <WinSock2.h>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

CGameRoom* g_pRoom = nullptr;

namespace
{
    u32_t ParseSmokeSeconds(int argc, char** argv)
    {
        constexpr const char* kPrefix = "--smoke-seconds=";
        constexpr size_t kPrefixLen = 16;

        for (int i = 1; i < argc; ++i)
        {
            if (std::strncmp(argv[i], kPrefix, kPrefixLen) != 0)
                continue;

            const unsigned long value = std::strtoul(argv[i] + kPrefixLen, nullptr, 10);
            return static_cast<u32_t>(value);
        }

        return 0;
    }
}

int main(int argc, char** argv)
{
    std::cout.setf(std::ios::unitbuf);
    std::cerr.setf(std::ios::unitbuf);

    const u32_t smokeSeconds = ParseSmokeSeconds(argc, argv);

    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        std::cerr << "[ERROR] WSAStartup failed\n";
        return 1;
    }

    auto room = CGameRoom::Create(1);
    if (!room)
    {
        std::cerr << "[ERROR] GameRoom create failed\n";
        WSACleanup();
        return 2;
    }

    g_pRoom = room.get();
    CPacketDispatcher::Instance().RegisterRoom(1, room.get());
    room->Start();

    auto core = CIOCPCore::Create(9000, 4);
    if (!core || !core->Start())
    {
        std::cerr << "[ERROR] IOCPCore start failed\n";
        room->Stop();
        g_pRoom = nullptr;
        WSACleanup();
        return 3;
    }

    std::cout << "[Server] WintersServer v0.2 running on port 9000.\n";
    if (smokeSeconds > 0)
    {
        std::cout << "[Server] Smoke mode: running for " << smokeSeconds << " seconds.\n";
        std::this_thread::sleep_for(std::chrono::seconds(smokeSeconds));
    }
    else
    {
        std::cout << "[Server] Press 'q' + Enter to quit.\n";

        std::string line;
        while (std::getline(std::cin, line))
        {
            if (line == "q" || line == "Q")
                break;

            if (line.rfind("hp ", 0) == 0)
            {
                std::istringstream iss(line.substr(3));
                u32_t netId = 0;
                f32_t value = 0.f;
                if (iss >> netId >> value)
                {
                    if (!room->DebugSetHealthByNetId(netId, value))
                        std::cout << "[Server] hp command failed netId=" << netId << "\n";
                }
                else
                {
                    std::cout << "[Server] usage: hp <netId> <value>\n";
                }
            }
        }
    }

    core->Shutdown();
    room->Stop();
    g_pRoom = nullptr;
    WSACleanup();
    return 0;
}
```

### 5.3 수정 후 (변경된 라인만 두 hunk 으로 박제)

#### 5.3.1 Hunk A — include 추가 (현재 L1-L13 사이)

**전**:
```cpp
#include "Game/GameRoom.h"
#include "Network/IOCPCore.h"
#include "Network/PacketDispatcher.h"

#include <WinSock2.h>
#include <chrono>
```

**후**:
```cpp
#include "Game/GameRoom.h"
#include "Game/ServerEntry.h"          // ★ 추가 (CServerEntry::Initialize/Shutdown)
#include "Network/IOCPCore.h"
#include "Network/PacketDispatcher.h"

#include <WinSock2.h>
#include <chrono>
```

#### 5.3.2 Hunk B — Initialize 호출 (현재 L48 ~ `auto room = ...` 사이)

**전** (L44-L51):
```cpp
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        std::cerr << "[ERROR] WSAStartup failed\n";
        return 1;
    }

    auto room = CGameRoom::Create(1);
```

**후**:
```cpp
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        std::cerr << "[ERROR] WSAStartup failed\n";
        return 1;
    }

    // ★ CServerEntry::Initialize — CJobSystem 단일 인스턴스 + FiberShell 모드 set.
    //   GameRoom 생성/Start 보다 먼저 호출 — TickThread 가 시작 시 Get_JobSystem()
    //   호출하면 nullptr 가 아니어야 함.
    if (!CServerEntry::Initialize(/*iWorkerCount=*/0, /*bEnableFiberShell=*/true))
    {
        std::cerr << "[ERROR] CServerEntry::Initialize failed\n";
        WSACleanup();
        return 4;
    }

    auto room = CGameRoom::Create(1);
```

#### 5.3.3 Hunk C — Shutdown 호출 (현재 L107-L111 사이)

**전**:
```cpp
    core->Shutdown();
    room->Stop();
    g_pRoom = nullptr;
    WSACleanup();
    return 0;
}
```

**후**:
```cpp
    core->Shutdown();
    room->Stop();
    g_pRoom = nullptr;

    // ★ CServerEntry::Shutdown — GameRoom 정지 후, WSACleanup 전에 호출.
    //   GameRoom Tick thread 가 join 된 후에만 안전 (room->Stop() 이 join 보장).
    CServerEntry::Shutdown();

    WSACleanup();
    return 0;
}
```

### 5.4 변경 라인 수

- 추가 1 줄 (include)
- 추가 9 줄 (Initialize 분기)
- 추가 4 줄 (Shutdown 호출)
- 총 +14 줄. 제거 0. 함수 시그니처 변경 0. 반환 코드 매핑 변경 0 (4 추가, 1~3 보존).

---

## 6. 수정 파일 2 — `Server/Private/Game/GameRoom.cpp`

### 6.1 경로

`C:\Users\user\Desktop\Winters\Server\Private\Game\GameRoom.cpp`

### 6.2 수정 대상 = `TickThread()` 만 (현재 L268-L302). 다른 함수 변경 0.

### 6.3 수정 전 (L268-L302 전문)

```cpp
void CGameRoom::TickThread()
{
    using clock = std::chrono::steady_clock;
    auto next = clock::now();
    const auto period = std::chrono::microseconds(33333);
    i64_t maxJitterMicros = 0;
    i64_t totalTicks = 0;
    auto lastReport = clock::now();

    while (m_bRunning.load(std::memory_order_relaxed))
    {
        const auto tickStart = clock::now();
        Tick();

        i64_t jitter = std::chrono::duration_cast<std::chrono::microseconds>(
            tickStart - next).count();
        if (jitter < 0)
            jitter = -jitter;
        if (jitter > maxJitterMicros)
            maxJitterMicros = jitter;
        ++totalTicks;

        if (clock::now() - lastReport > std::chrono::seconds(30))
        {
            std::cout << "[Tick] count=" << totalTicks
                << " maxJitter=" << maxJitterMicros << " us\n";
            maxJitterMicros = 0;
            totalTicks = 0;
            lastReport = clock::now();
        }

        next += period;
        std::this_thread::sleep_until(next);
    }
}
```

### 6.4 수정 후 (전문, 신규 ConvertThreadToFiber/ConvertFiberToThread shell 추가)

```cpp
void CGameRoom::TickThread()
{
    using clock = std::chrono::steady_clock;
    auto next = clock::now();
    const auto period = std::chrono::microseconds(33333);
    i64_t maxJitterMicros = 0;
    i64_t totalTicks = 0;
    auto lastReport = clock::now();

    // ★ Stage 1 Fiber Shell — Tick thread 자체를 Win32 Fiber 컨텍스트로 변환.
    //   Stage 1 의 약속:
    //     - 본 thread 가 fiber 가 됨으로써 Stage 2 의 Submit + WaitForCounter
    //       호출 시 yield 가능한 컨텍스트 확보.
    //     - Stage 1 은 Submit 호출 0 — 단순 shell 적용 + 정상 복귀 검증.
    //   주의:
    //     - ConvertThreadToFiber 가 nullptr 반환 시 GetLastError 로그 + 일반 thread
    //       모드로 fallback (Tick 동작 보장 우선).
    //     - 본 함수 종료 전 ConvertFiberToThread 로 원상 복귀 (중요: thread 종료 전).
    //     - LPVOID lpParameter = nullptr — 이 fiber 는 Job 실행이 아니라 Tick loop
    //       자체이므로 시작 fiber 의 user data 는 불필요.
    LPVOID hThreadFiber = ::ConvertThreadToFiber(nullptr);
    const bool bFiberMode = (hThreadFiber != nullptr);
    if (!bFiberMode)
    {
        std::cerr << "[Tick] ConvertThreadToFiber failed (err="
                  << ::GetLastError() << ") — fallback to plain thread mode.\n";
    }
    else
    {
        std::cout << "[Tick] Fiber shell entered (room=" << m_roomId << ").\n";
    }

    while (m_bRunning.load(std::memory_order_relaxed))
    {
        const auto tickStart = clock::now();
        Tick();

        i64_t jitter = std::chrono::duration_cast<std::chrono::microseconds>(
            tickStart - next).count();
        if (jitter < 0)
            jitter = -jitter;
        if (jitter > maxJitterMicros)
            maxJitterMicros = jitter;
        ++totalTicks;

        if (clock::now() - lastReport > std::chrono::seconds(30))
        {
            std::cout << "[Tick] count=" << totalTicks
                << " maxJitter=" << maxJitterMicros << " us\n";
            maxJitterMicros = 0;
            totalTicks = 0;
            lastReport = clock::now();
        }

        next += period;
        std::this_thread::sleep_until(next);
    }

    // ★ Fiber Shell 정리 — thread 종료 전 반드시 ConvertFiberToThread 호출.
    //   호출 안 하면 thread 가 fiber 상태로 남아 OS 자원 leak.
    if (bFiberMode)
    {
        if (::ConvertFiberToThread())
        {
            std::cout << "[Tick] Fiber shell exited (room=" << m_roomId << ").\n";
        }
        else
        {
            std::cerr << "[Tick] ConvertFiberToThread failed (err="
                      << ::GetLastError() << ").\n";
        }
    }
}
```

### 6.5 검증 포인트

- `ConvertThreadToFiber(nullptr)` — Win32 SDK. PCH `<Windows.h>` 포함 안 되어 있어도 `WinSock2.h` 가 main.cpp 와 IOCPCore.cpp 에서 include 됨. GameRoom.cpp 는 직접 include 0 — 단 `<chrono>`, `<iostream>` 등을 통해 transitive 로 들어올 가능성. **확실하게 하려면 GameRoom.cpp 상단 include 에 `<Windows.h>` 추가** — 단 `WIN32_LEAN_AND_MEAN` define 으로 충돌 0 (Server.vcxproj L49 에서 define 됨).
- 다른 함수 변경 0. `Tick()` 본체 (L304-325) 그대로. Phase_* 함수 그대로.
- `bFiberMode = false` fallback — Win32 Fiber API 실패 시 일반 thread 로 동작 (행동 보존).

### 6.6 Hunk D — GameRoom.cpp include 추가 (현재 L1-L48 사이)

**전** (L43-L48, [GameRoom.cpp:43](../../../Server/Private/Game/GameRoom.cpp:43)):
```cpp
#include <flatbuffers/flatbuffers.h>
#include <functional>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
```

**후**:
```cpp
#include <flatbuffers/flatbuffers.h>
#include <functional>
#include <iostream>
#include <limits>
#include <string>
#include <utility>

// ★ Stage 1 Fiber shell — ConvertThreadToFiber/ConvertFiberToThread 호출용.
//   WIN32_LEAN_AND_MEAN 은 Server.vcxproj L49 에서 define — 충돌 0.
#include <Windows.h>
```

---

## 7. 수정 파일 3 — `Server/Include/Server.vcxproj`

### 7.1 경로

`C:\Users\user\Desktop\Winters\Server\Include\Server.vcxproj`

### 7.2 수정 대상

ItemGroup 의 `<ClCompile>` 목록에 `ServerEntry.cpp` 추가 (현재 L99-L113 의 server cpp 묶음).

### 7.3 수정 전 (L99-L106, [Server.vcxproj:99](../../../Server/Include/Server.vcxproj:99))

```xml
  <ItemGroup>
    <ClCompile Include="..\Private\Game\AOI.cpp" />
    <ClCompile Include="..\Private\Game\CommandDispatcher.cpp" />
    <ClCompile Include="..\Private\Game\GameLogic.cpp" />
    <ClCompile Include="..\Private\Game\GameRoom.cpp" />
    <ClCompile Include="..\Private\Game\ServerWorld.cpp" />
    <ClCompile Include="..\Private\Game\SnapshotBuilder.cpp" />
    <ClCompile Include="..\Private\main.cpp" />
```

### 7.4 수정 후

```xml
  <ItemGroup>
    <ClCompile Include="..\Private\Game\AOI.cpp" />
    <ClCompile Include="..\Private\Game\CommandDispatcher.cpp" />
    <ClCompile Include="..\Private\Game\GameLogic.cpp" />
    <ClCompile Include="..\Private\Game\GameRoom.cpp" />
    <ClCompile Include="..\Private\Game\ServerEntry.cpp" />
    <ClCompile Include="..\Private\Game\ServerWorld.cpp" />
    <ClCompile Include="..\Private\Game\SnapshotBuilder.cpp" />
    <ClCompile Include="..\Private\main.cpp" />
```

### 7.5 vcxproj.filters (있으면)

`Server/Include/Server.vcxproj.filters` 에 동일 항목 추가. 현재 filters 파일 존재 확인 필요 — 없으면 생성 X (vcxproj 만 변경해도 빌드 동작).

```xml
<!-- 기존 <ClCompile Include="..\Private\Game\GameRoom.cpp"> 옆에 -->
<ClCompile Include="..\Private\Game\ServerEntry.cpp">
  <Filter>Source Files\Game</Filter>
</ClCompile>
```

(filters 파일 부재 시 무시 — Visual Studio 자동 평면 트리 사용)

---

## 8. 빌드/검증

### 8.1 빌드 절차

```powershell
# 1. devenv.exe 종료 확인 (PDB lock 회피, CLAUDE.md §5.1)
Get-Process devenv -ErrorAction SilentlyContinue

# 2. Engine project 단독 빌드 (PostBuild 가 EngineSDK/inc 동기화)
msbuild Engine\Include\Engine.vcxproj /p:Configuration=Debug /p:Platform=x64 /m

# 3. Server project 빌드
msbuild Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m

# 4. 산출물 확인
Test-Path Server\Bin\Debug\WintersServer.exe
```

### 8.2 30초 smoke

```powershell
Server\Bin\Debug\WintersServer.exe --smoke-seconds=30
```

### 8.3 합격 기준

```text
[OK] [ServerEntry] Initialize OK workers=N mode=FiberShell
[OK] [Tick] Fiber shell entered (room=1).
[OK] [IOCPCore] listening on port 9000 (workers=4)
[OK] [Server] WintersServer v0.2 running on port 9000.
[OK] [Tick] count=900 maxJitter=<800> us  (30초 × 30Hz = 900 ± 1)
[OK] [Tick] Fiber shell exited (room=1).
[OK] [ServerEntry] Shutdown complete.
[NO_CRASH] exit code 0
```

### 8.4 회귀 검증

| 항목 | 검증 방법 | 합격 기준 |
|---|---|---|
| Tick jitter | smoke 로그의 maxJitter | < 1000us (Stage 0 baseline 과 차이 +200us 이내) |
| IOCP worker | `WSAStartup` ~ `WSACleanup` 사이 GQCS 호출 | 변경 0 (코드 변경 없음, 회귀 0) |
| BanPick TCP | 1 client 연결 + LobbyState 수신 (선택) | 변경 0 (Stage 1 은 Tick path 만 변경) |
| ConvertThreadToFiber 실패 | `ConvertThreadToFiber` 에러 로그 + plain thread fallback | 동작 보장 (행동 보존) |
| Fiber leak | thread 종료 전 ConvertFiberToThread 호출 | OS handle leak 0 |

### 8.5 Acceptance

```text
[ ] Engine project 단독 빌드 성공 (PostBuild EngineSDK/inc 동기화 OK)
[ ] Server project 빌드 성공 (Debug + Release)
[ ] 30s smoke 통과 (위 합격 기준)
[ ] Tick maxJitter Stage 0 baseline + <200us 이내
[ ] [Tick] Fiber shell entered/exited 로그 각 1회
[ ] grep "Submit\|WaitForCounter" Server/ 결과 0 (Stage 1 약속 — Submit 호출 0)
[ ] CIOCPCore::WorkerLoop 회귀 0 (코드 diff 없음)
[ ] git diff Server/ 의 변경량: ServerEntry.h/cpp 신규 + main.cpp +14L + GameRoom.cpp +29L + vcxproj +1L
```

---

## 9. 19 함정 회피 보고 (Stage 1)

| 함정 | 회피 |
|---|---|
| P-1 데이터 미검증 | §1 Preflight 표 모든 행 확정값. |
| P-2 PIMPL 추측 | `CJobSystem` 의 호출 API 만 사용 — 내부 PIMPL 0. |
| P-3 호출 경로 단일 가정 | Tick thread = TickThread 1 경로만 변경. 다른 호출자 grep 결과: GameRoom.cpp 안에서 `TickThread` 호출은 `Start()` 의 `m_tickThread = std::thread(&CGameRoom::TickThread, this)` 1 곳 ([GameRoom.cpp:256](../../../Server/Private/Game/GameRoom.cpp:256)). |
| P-4 ECS / Scene 결합 | `CServerEntry` 는 Scene/ECS 와 무관. CGameRoom 의 글로벌 `g_pRoom` 도 Stage 1 에서 변경 0. |
| P-5 유령 의존 | 모든 인용 (.h, .cpp, vcxproj) 줄 번호 + 직접 인용. |
| P-6 TODO 박제 진입 | §1 Preflight TODO 0. |
| P-7 자료형 미래 사례 | Stage 1 은 자료형 추가 0. `CJobSystem` 만 사용. |
| P-8 인용 의미 반전 | "FiberShell 모드 set + Submit 호출 0" — CLAUDE.md §1.A Track 3 의 "1차 목표 = Fiber shell only" 와 정확히 일치. |
| P-9 Scheduler 동시성 | Stage 1 은 phase 추가 0. |
| P-10 Owner Scope | `CServerEntry` static 멤버 = 프로세스당 1. RHI Device 패턴과 동일. |
| P-11 도메인 상수 | Server worker count = main.cpp L63 기존 4 유지. CJobSystem worker count = 0 (자동) — 도메인 상수 0. |
| P-12 음수 좌표 | 본 박제는 좌표 계산 0. |
| P-13 미존재 API | 호출 API 모두 헤더 인용으로 실재 검증: `Initialize` ([JobSystem.h:35](../../../Engine/Public/Core/JobSystem.h:35)), `SetExecutionMode` ([L37](../../../Engine/Public/Core/JobSystem.h:37)), `Shutdown` ([L36](../../../Engine/Public/Core/JobSystem.h:36)), `GetWorkerCount` ([L50](../../../Engine/Public/Core/JobSystem.h:50)). |
| P-14 행동 변경 | Stage 1 행동 변화 0 — `bFiberMode = false` fallback 으로 plain thread 동작 보장. |
| P-15 헤더 외부 의존 미include | `ServerEntry.h` 가 `CJobSystem` 사용 — `Core/JobSystem.h` 직접 include. `eJobExecutionMode` 사용 — `Core/Fiber/FiberTypes.h` 직접 include. |
| P-16 산술 검증 | `compare_exchange_strong` 멱등성 — 2회 호출 시 두 번째 false 반환 (산술 X). |
| P-17 Typedef 일괄 변경 | typedef 변경 0. |
| P-18 RHI/Engine 인프라 미인지 | `CJobSystem` 재사용. 신설 0. |
| P-19 Render/Sim 결합 | Server 에 Render 없음. |

---

## 10. Stage 2 진입 조건

본 Stage 1 통과 후 [`02_SERVER_TICK_FIBER_AWARE.md`](02_SERVER_TICK_FIBER_AWARE.md) 진입.

선결:
- 위 §8.5 acceptance 모든 [x] 체크 완료
- Engine 측 `CJobSystem::PushToSomeDeque` 의 Main-push race fix 확인 (CLAUDE.md §1.C 미결) — Stage 2 의 Submit 안전 조건

---

**END OF STAGE 1**
