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

    std::cout << "[Server] WintersServer v0.2 running on 0.0.0.0:9000.\n";
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
