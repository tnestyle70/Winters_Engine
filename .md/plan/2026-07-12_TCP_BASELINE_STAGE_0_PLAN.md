Session - UDP 경계를 만들기 전에 TCP partial send, bounded queue, command ingress, IOCP shutdown 수명주기를 재현 가능하게 고정한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Server/Public/Network/Session.h

아래 기존 코드를:

```cpp
    void OnRecvComplete(const u8_t* bytes, u32_t len);
    void OnSendComplete(u32_t bytes);
    void OnDisconnect();
    bool Send(std::vector<u8_t> packet);
```

아래로 교체:

```cpp
    bool_t OnRecvComplete(const u8_t* bytes, u32_t len);
    bool_t OnSendComplete(u32_t bytes);
    void BeginClose();
    SessionSendResult Send(std::vector<u8_t> packet);
```

기존 코드:

```cpp
class CSession : public std::enable_shared_from_this<CSession>
```

아래로 교체:

```cpp
enum class SessionSendStatus : u8_t
{
    Queued = 0,
    QueueFull,
    Closing,
    InvalidPacket,
    TransportError,
};

struct SessionSendResult
{
    SessionSendStatus status = SessionSendStatus::TransportError;
    int wsaError = 0;

    bool_t IsQueued() const
    {
        return status == SessionSendStatus::Queued;
    }
};

class CSession : public std::enable_shared_from_this<CSession>
```

CONFIRM_NEEDED:

- 초기 cap은 per-session `1 MiB / 128 packets / oldest 1000 ms`로 시작하고 TCP capture 뒤 조정한다. packet 하나를 침묵 drop하지 않고 `QueueFull + BeginClose`로 slow consumer를 격리한다.
- private에는 front offset, remaining queue bytes, enqueue monotonic time, `PostFrontSendLocked`, `m_sendMutex -> m_socketMutex` 단방향 lock order를 넣는다.
- `m_socketMutex`는 `WSARecv/WSASend` post와 `shutdown/closesocket + INVALID_SOCKET`을 직렬화한다. socket mutex를 잡은 채 send mutex로 역진입하지 않는다.

1-2. C:/Users/user/Desktop/Winters/Server/Private/Network/Session.cpp

CONFIRM_NEEDED:

- `PendingSendPacket`, cap 상수와 socket lock을 확정한 뒤 이 파일 섹션 전체를 complete code block으로 재작성하기 전에는 적용하지 않는다.
- `OnSendComplete`는 `bytes == 0` 또는 `bytes > front remaining`이면 bounded WSA 진단 후 false를 반환하고, partial이면 front를 pop하지 않은 채 `data + offset / remaining`으로 재게시한다. front 전체 완료 때만 pop/offset reset하며 Closing이면 다음 send를 게시하지 않는다.
- `Send`, `PostRecv`, `OnRecvComplete`, `BeginClose`도 함께 재작성한다. `Send`는 structured result를 반환하고, fatal path는 socket close/cancel만 수행하며 GameRoom/manager callback을 호출하지 않는다.
- completion callback 실행 중 drain 완료로 오판하지 않도록 pending-I/O 감소 위치를 `CSession_Manager` handler의 callback 종료 뒤로 옮긴다.

1-3. C:/Users/user/Desktop/Winters/Server/Public/Network/Session_Manager.h

아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    void OnSendComplete(u32_t sessionId, u32_t bytes);
```

아래에 추가:

```cpp
    void BeginShutdown();
    bool_t WaitForDrain(u32_t timeoutMs);
    bool_t IsShuttingDown() const;
    size_t ClosingCount() const;
```

CONFIRM_NEEDED:

- `BeginShutdown`은 shutdown flag를 먼저 세워 racing accept를 거절한 뒤 active ids를 snapshot하고, manager mutex 밖에서 unroute와 socket close를 수행한다.
- 정상 runtime disconnect만 `g_pRoom->OnSessionLeave`를 호출한다. global shutdown에서는 room이 이미 quiesce됐으므로 lobby broadcast/새 `WSASend`를 만들지 않는다.
- `WaitForDrain(timeout)==false`는 fatal diagnostic/test failure다. outstanding `OVERLAPPED`가 남은 채 IOCP/context를 강제 해제하지 않고 계속 drain하거나 명시적 process-fatal 정책으로 끝낸다.

1-4. C:/Users/user/Desktop/Winters/Server/Private/Network/Session_Manager.cpp

CONFIRM_NEEDED:

- `OnRecvComplete`와 `OnSendComplete`는 새 bool 결과를 받고 callback이 끝난 뒤 pending-I/O를 정확히 1회 감소한다. false이면 `CSession` method 밖에서 final manager disconnect를 수행한다.
- room state lock 안의 outbound `Send` 실패에서는 manager가 `OnDisconnect -> OnSessionLeave`를 동기 호출하지 않는다. `BeginClose`로 pending recv completion을 깨우고 manager finalization을 defer한다.
- 모든 external call(`GameRoom`, dispatcher, session close)은 `m_mutex` 밖에서 실행한다. active/closing이 모두 0일 때 condition variable을 깨운다.
- 이 파일의 complete code block은 1-1/1-2의 final API와 lock order를 확정한 뒤 작성한다.

1-5. C:/Users/user/Desktop/Winters/Server/Public/Network/IOCPCore.h

아래 기존 코드를:

```cpp
    bool Start();
    void Shutdown();
```

아래로 교체:

```cpp
    bool Start();
    void StopAccepting();
    void Shutdown();
```

아래 기존 필드 바로 아래에 추가:

기존 코드:

```cpp
    std::atomic<bool> m_bRunning{ false };
```

아래에 추가:

```cpp
    std::atomic<bool> m_bAccepting{ false };
    std::atomic<u32_t> m_pendingAcceptCount{ 0u };
```

1-6. C:/Users/user/Desktop/Winters/Server/Private/Network/IOCPCore.cpp

CONFIRM_NEEDED:

- `m_bAccepting`과 worker lifetime을 분리한다. `StopAccepting`은 listen `CancelIoEx/closesocket`과 accept repost만 막고 worker를 종료하지 않는다.
- 각 성공적으로 게시한 AcceptEx context를 count하고 성공/취소 completion에서 정확히 한 번 감소·delete한다. StopAccepting과 race한 successful accept socket은 session으로 승격하지 않고 닫는다.
- `WorkerLoop`는 `m_bRunning`이 false가 됐다는 이유로 loop top에서 빠지지 않는다. accept count 0, active/closing sessions 0, callback/pending I/O 0 뒤에만 explicit sentinel을 받아 종료한다.
- `PostInitialRecv()` 실패는 무시하지 않고 manager disconnect로 정리한다.
- complete `Shutdown` body는 위 count와 `Session_Manager::WaitForDrain` 계약을 확정한 뒤 작성한다. timeout 후 `CloseHandle/WSACleanup` 강행은 금지한다.

1-7. C:/Users/user/Desktop/Winters/Server/Private/main.cpp

아래 기존 코드를:

```cpp
    core->Shutdown();
    room->Stop();
    g_pRoom = nullptr;
    WSACleanup();
```

아래로 교체:

```cpp
    core->StopAccepting();
    room->Stop();
    core->Shutdown();
    g_pRoom = nullptr;
    WSACleanup();
```

아래 기존 코드를:

```cpp
    auto core = CIOCPCore::Create(9000, 4);
    if (!core || !core->Start())
    {
        std::cerr << "[ERROR] IOCPCore start failed\n";
        room->Stop();
        g_pRoom = nullptr;
        WSACleanup();
        return 3;
    }
```

아래로 교체:

```cpp
    auto core = CIOCPCore::Create(9000, 4);
    if (!core)
    {
        std::cerr << "[ERROR] IOCPCore create failed\n";
        room->Stop();
        g_pRoom = nullptr;
        WSACleanup();
        return 3;
    }

    if (!core->Start())
    {
        std::cerr << "[ERROR] IOCPCore start failed\n";
        core->StopAccepting();
        room->Stop();
        core->Shutdown();
        g_pRoom = nullptr;
        WSACleanup();
        return 3;
    }
```

1-8. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomCommands.cpp

아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
#include <cstdio>
```

아래에 추가:

```cpp
#include <iostream>
```

아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    if (!batch)
        return;
```

아래에 추가:

```cpp
    {
        std::lock_guard stateLock(m_stateMutex);
        if (!IsInGamePhase())
        {
            static std::atomic<u32_t> s_preGameRejectCount{ 0u };
            if (s_preGameRejectCount.fetch_add(1u, std::memory_order_relaxed) < 8u)
            {
                std::cerr << "[GameRoom] pre-game CommandBatch rejected sid="
                          << sessionId << '\n';
            }
            return;
        }
    }
```

CONFIRM_NEEDED:

- Stop 시작 시 ingress accepting을 false로 만드는 atomic gate를 추가하고 Join/Lobby/Command는 reject, Leave는 drain 동안 허용한다.

1-9. C:/Users/user/Desktop/Winters/Server/Public/Game/CommandIngress.h

CONFIRM_NEEDED:

- `kMaxCommandsPerBatch=64`, pending global 512, per-session 64를 초기 cap으로 둔다. `PendingCommand`가 fixed-size이므로 count가 byte bound도 제공한다.
- batch preflight 뒤에만 sequence를 소비한다. overflow 중 일부 command만 enqueue하거나 sequence를 먼저 올리고 packet을 drop하지 않는다.
- Move는 기존 same-session pending Move를 먼저 교체해 count를 늘리지 않는다.
- `RemoveSession(sessionId)`는 pending command와 per-session accounting을 함께 지운다.
- complete target header는 Stage 1A command-sequence ownership 변경과 합치지 말고 Stage 0 단독 diff로 다시 작성한다.

1-10. C:/Users/user/Desktop/Winters/Server/Private/Game/CommandIngress.cpp

CONFIRM_NEEDED:

- `AcceptCommandBatch`는 `Accepted / TooManyCommands / QueueFull / UnknownSession` typed result를 반환한다.
- global/per-session cap 검사는 `m_pendingMutex` 한 critical section에서 batch 전체에 대해 수행한다.
- overflow는 bounded Server diagnostic 뒤 해당 slow/abusive session을 room lock 밖에서 disconnect하도록 caller에 반환한다.
- `DrainSorted`와 Move coalescing의 기존 deterministic sort/order는 보존한다.

1-11. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomLobby.cpp

`CGameRoom::OnSessionLeave`에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    m_sessionBinding.Unbind(sessionId);
```

아래에 추가:

```cpp
    m_commandIngress.RemoveSession(sessionId);
    m_lastSimCommandSeqBySession.erase(sessionId);
```

1-12. C:/Users/user/Desktop/Winters/Client/Public/Network/Client/ClientNetwork.h

CONFIRM_NEEDED:

- pending frame을 enqueue timestamp가 있는 named struct로 바꾸고 `2 MiB / 256 frames / oldest 1000 ms` 초기 hard cap과 `m_pendingFrameBytes`를 둔다.
- cap 초과 시 arbitrary Event/Control을 drop하지 않고 connection failed로 전이한다. Pump swap 때 byte counter를 0으로 만든다.
- complete header body는 `Disconnect` join 계약과 함께 작성한다.

1-13. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/ClientNetwork.cpp

아래 기존 코드를:

```cpp
void CClientNetwork::Disconnect()
{
    if (!m_bRunning.exchange(false)) return;
    m_bConnected = false;

    if (m_socket != INVALID_SOCKET)
    {
        shutdown(m_socket, SD_BOTH);
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }

    if (m_recvThread.joinable()) m_recvThread.join();
}
```

아래로 교체:

```cpp
void CClientNetwork::Disconnect()
{
    m_bRunning.store(false, std::memory_order_relaxed);
    m_bConnected.store(false, std::memory_order_relaxed);

    if (m_socket != INVALID_SOCKET)
    {
        shutdown(m_socket, SD_BOTH);
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }

    if (m_recvThread.joinable() &&
        m_recvThread.get_id() != std::this_thread::get_id())
    {
        m_recvThread.join();
    }
}
```

CONFIRM_NEEDED:

- recv worker가 cap/recv failure로 `m_bRunning=false`를 먼저 만들더라도 destructor의 `Disconnect()`가 join을 건너뛰지 않는지 검증한다.
- worker 자기 자신에서 `Disconnect()`를 호출하지 않는다. failure는 flag와 socket shutdown만 수행하고 owner thread/destructor가 join한다.

2. 검증

미검증:

- 위 코드는 계획이며 source에 반영하지 않았다.
- `CONFIRM_NEEDED` 섹션은 complete code block으로 재작성하기 전 적용 금지다.
- Server/Client build, partial-send reproducer, shutdown drain, queue cap runtime은 미검증이다.

검증 명령:

- `git diff --check`
- `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Server\Include\Server.vcxproj /m /t:Build /p:Configuration=Debug /p:Platform=x64 /nologo /v:minimal`
- `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Client\Include\Client.vcxproj /m /t:Build /p:Configuration=Debug /p:Platform=x64 /nologo /v:minimal`
- `rg -n 'OnSendComplete|m_sendOffset|m_sendQueueBytes|SessionSendResult' Server/Public/Network/Session.h Server/Private/Network/Session.cpp`
- `rg -n 'StopAccepting|WaitForDrain|pendingAccept' Server/Public/Network Server/Private/Network Server/Private/main.cpp`

수동 확인:

- 작은 `SO_SNDBUF`와 slow reader로 64 KiB frame을 반복해 partial completion count가 0보다 크고 client parser byte parity가 일치하는지 확인.
- send queue bytes/count/oldest age가 cap을 넘지 않고 slow peer만 disconnect되는지 확인.
- Lobby/Loading에서 `CommandBatch` spam 후 pending global/per-session count가 고정되고 InGame truth가 변하지 않는지 확인.
- idle, recv-pending, send-pending, accept-pending 각각에서 shutdown 후 active=0, closing=0, pendingIo=0, pendingAccept=0인지 확인.
- `Server.Tick.Total` p99 <= 25 ms, max < 33.333 ms를 5-client 10분 capture로 확인.
- TCP packet/replay/state hash가 변경 전 baseline과 일치하는지 확인.

확인 필요:

- `CONFIRM_NEEDED`의 complete code를 현재 source와 lock-order audit 후 다시 작성.
- drain timeout에서 outstanding context를 강제 해제하지 않는 process-fatal/continue-drain 정책 확정.
- cap 초기값을 1/5-client TCP capture의 queue bytes/age p99와 최대 Snapshot 크기로 조정.
- 이 Stage 0 전체 gate를 통과한 뒤 `2026-07-12_UDP_MIGRATION_STAGE_1A_TRANSPORT_BOUNDARY_PLAN.md`의 모든 anchor와 `SessionSendResult -> GamePacketSendResult` mapping을 재작성.
