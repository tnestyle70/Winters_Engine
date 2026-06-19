Session - Server 최상단을 CLI와 런타임 수명주기로 분리한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Server/Public/Game/ServerRuntime.h

새 파일:

```cpp
#pragma once

#include "WintersTypes.h"

#include <memory>

class CGameRoom;
class CIOCPCore;

struct ServerRuntimeConfig
{
    u32_t roomId = 1;
    u16_t port = 9000;
    u32_t workerCount = 4;
};

class CServerRuntime final
{
public:
    CServerRuntime() = default;
    ~CServerRuntime();

    CServerRuntime(const CServerRuntime&) = delete;
    CServerRuntime& operator=(const CServerRuntime&) = delete;

    bool_t Start(const ServerRuntimeConfig& config);
    void Stop();

    bool_t IsRunning() const { return m_bRunning; }
    bool_t DebugSetHealthByNetId(NetEntityId netId, f32_t value);

private:
    ServerRuntimeConfig m_config{};
    bool_t m_bWinsockStarted = false;
    bool_t m_bRunning = false;
    std::unique_ptr<CGameRoom> m_room;
    std::unique_ptr<CIOCPCore> m_core;
};
```

1-2. C:/Users/tnest/Desktop/Winters/Server/Private/Game/ServerRuntime.cpp

새 파일:

```cpp
#include "Game/ServerRuntime.h"

#include "Game/GameRoom.h"
#include "Network/IOCPCore.h"
#include "Network/PacketDispatcher.h"

#include <WinSock2.h>

#include <iostream>

CGameRoom* g_pRoom = nullptr;

CServerRuntime::~CServerRuntime()
{
    Stop();
}

bool_t CServerRuntime::Start(const ServerRuntimeConfig& config)
{
    if (m_bRunning)
    {
        std::cerr << "[ServerRuntime] Start called while already running.\n";
        return false;
    }

    m_config = config;

    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        std::cerr << "[ERROR] WSAStartup failed\n";
        return false;
    }
    m_bWinsockStarted = true;

    m_room = CGameRoom::Create(m_config.roomId);
    if (!m_room)
    {
        std::cerr << "[ERROR] GameRoom create failed\n";
        Stop();
        return false;
    }

    g_pRoom = m_room.get();
    CPacketDispatcher::Instance().RegisterRoom(m_config.roomId, m_room.get());
    m_room->Start();

    m_core = CIOCPCore::Create(m_config.port, m_config.workerCount);
    if (!m_core || !m_core->Start())
    {
        std::cerr << "[ERROR] IOCPCore start failed\n";
        Stop();
        return false;
    }

    m_bRunning = true;
    std::cout << "[Server] WintersServer v0.2 running on port "
        << m_config.port << ".\n";
    return true;
}

void CServerRuntime::Stop()
{
    if (m_core)
    {
        m_core->Shutdown();
        m_core.reset();
    }

    if (m_room)
    {
        m_room->Stop();
        CPacketDispatcher::Instance().UnregisterRoom(m_config.roomId);
        m_room.reset();
    }

    g_pRoom = nullptr;

    if (m_bWinsockStarted)
    {
        WSACleanup();
        m_bWinsockStarted = false;
    }

    m_bRunning = false;
}

bool_t CServerRuntime::DebugSetHealthByNetId(NetEntityId netId, f32_t value)
{
    return m_room ? m_room->DebugSetHealthByNetId(netId, value) : false;
}
```

1-3. C:/Users/tnest/Desktop/Winters/Server/Public/Network/PacketDispatcher.h

`class CPacketDispatcher`의 public 영역에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
void RegisterRoom(u32_t roomId, CGameRoom* pRoom);
```

아래에 추가:

```cpp
void UnregisterRoom(u32_t roomId);
```

1-4. C:/Users/tnest/Desktop/Winters/Server/Private/Network/PacketDispatcher.cpp

아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
void CPacketDispatcher::RegisterRoom(u32_t roomId, CGameRoom* pRoom)
{
    std::lock_guard lk(m_mutex);
    m_rooms[roomId] = pRoom;
}
```

아래에 추가:

```cpp
void CPacketDispatcher::UnregisterRoom(u32_t roomId)
{
    std::lock_guard lk(m_mutex);
    m_rooms.erase(roomId);

    for (auto it = m_sessionToRoom.begin(); it != m_sessionToRoom.end();)
    {
        if (it->second == roomId)
            it = m_sessionToRoom.erase(it);
        else
            ++it;
    }
}
```

1-5. C:/Users/tnest/Desktop/Winters/Server/Private/main.cpp

기존 코드:

```text
파일 전체
```

아래로 교체:

```cpp
#include "Game/ServerRuntime.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

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

    ServerRuntimeConfig config{};
    CServerRuntime runtime;
    if (!runtime.Start(config))
        return 1;

    if (smokeSeconds > 0)
    {
        std::cout << "[Server] Smoke mode: running for "
            << smokeSeconds << " seconds.\n";
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
                    if (!runtime.DebugSetHealthByNetId(netId, value))
                        std::cout << "[Server] hp command failed netId=" << netId << "\n";
                }
                else
                {
                    std::cout << "[Server] usage: hp <netId> <value>\n";
                }
            }
        }
    }

    runtime.Stop();
    return 0;
}
```

2. 검증

검증 명령:
- `git diff --check -- Server/Public/Game/ServerRuntime.h Server/Private/Game/ServerRuntime.cpp Server/Public/Network/PacketDispatcher.h Server/Private/Network/PacketDispatcher.cpp Server/Private/main.cpp`
- `& 'C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe' Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal`
- `Server/Bin/Debug/WintersServer.exe --smoke-seconds=1`
- `& 'C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe' Server/Include/Server.vcxproj /p:Configuration=Release /p:Platform=x64 /m:1 /nr:false /v:minimal`
- `Server/Bin/Release/WintersServer.exe --smoke-seconds=1`

확인 필요:
- 새로 추가한 `ServerRuntime.h`, `ServerRuntime.cpp`가 `Server.vcxproj`와 `Server.vcxproj.filters`에 포함되는지 확인.
- `CServerEntry`는 현재 Server 부트스트랩에서 쓰이지 않으므로 S00 이후 S01에서 삭제 또는 역할 재정의 여부를 결정.
